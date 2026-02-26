#include "cloud_labeler.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

CloudAutoLabeler::CloudAutoLabeler(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setInterval(POLL_INTERVAL);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() {
        if (m_batchMode) pollBatch();
        else             pollSingle();
    });
}

// ── Configuration ───────────────────────────────────────────────────────────

void CloudAutoLabeler::setApiKey(const QString &apiKey) { m_apiKey = apiKey; }
void CloudAutoLabeler::setPrompt(const QString &prompt)  { m_prompt = prompt; }
void CloudAutoLabeler::setClasses(const QStringList &c)  { m_classes = c; }

// ── Public actions ──────────────────────────────────────────────────────────

void CloudAutoLabeler::labelImage(const QString &imagePath)
{
    if (m_busy) return;
    m_allPaths = { imagePath };
    m_queue    = { 0 };
    setBusy(true);
    m_cancelRequested = false;
    processNextInQueue();
}

void CloudAutoLabeler::labelImages(const QStringList &imagePaths)
{
    if (m_busy || imagePaths.isEmpty()) return;
    m_allPaths = imagePaths;
    setBusy(true);
    m_cancelRequested = false;

    if (imagePaths.size() == 1) {
        m_queue = { 0 };
        processNextInQueue();
        return;
    }

    // Batch path
    m_batchMode  = true;
    m_batchTotal = imagePaths.size();
    m_batchDone  = 0;
    m_batchChunks.clear();
    m_batchJobIds.clear();
    m_batchPaths.clear();

    for (int i = 0; i < imagePaths.size(); i += BATCH_SIZE) {
        QStringList chunk;
        for (int j = i; j < qMin(i + BATCH_SIZE, imagePaths.size()); ++j)
            chunk.append(imagePaths[j]);
        m_batchChunks.append(chunk);
    }

    emit progress(0, m_batchTotal);
    submitBatchChunk(m_batchChunks.takeFirst());
}

void CloudAutoLabeler::cancel()
{
    ++m_generation;          // invalidate all in-flight callbacks
    m_cancelRequested = true;
    m_pollTimer->stop();
    resetState();
    setBusy(false);
    emit statusMessage("Cloud auto-label cancelled.", 3000);
}

// ── Private helpers ─────────────────────────────────────────────────────────

void CloudAutoLabeler::setBusy(bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    emit busyChanged(busy);
}

void CloudAutoLabeler::resetState()
{
    m_pollCount    = 0;
    m_retryCount   = 0;
    m_pendingJobId = -1;
    m_pendingPath.clear();
    m_queue.clear();
    m_singlePolling = false;
    m_batchMode    = false;
    m_batchPolling = false;
    m_batchJobIds.clear();
    m_batchJobStatuses.clear();
    m_batchPaths.clear();
    m_batchChunks.clear();
    m_batchTotal              = 0;
    m_batchDone               = 0;
    m_batchFailed             = 0;
    m_batchChunkWriteSucceeded = 0;
}

void CloudAutoLabeler::handleFatalError(const QString &message)
{
    m_pollTimer->stop();
    resetState();
    setBusy(false);
    emit errorOccurred(message);
}

QNetworkRequest CloudAutoLabeler::makeRequest(const QString &endpoint) const
{
    QNetworkRequest req(QUrl(QString::fromLatin1(API_HOST) + endpoint));
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
    return req;
}

QString CloudAutoLabeler::mimeForImage(const QString &path)
{
    const QString lower = path.toLower();
    if (lower.endsWith(".png"))                            return "image/png";
    if (lower.endsWith(".bmp"))                            return "image/bmp";
    if (lower.endsWith(".tif") || lower.endsWith(".tiff")) return "image/tiff";
    if (lower.endsWith(".webp"))                           return "image/webp";
    return "image/jpeg";
}

void CloudAutoLabeler::backupLabelFile(const QString &labelPath)
{
    if (QFile::exists(labelPath)) {
        const QString bak = labelPath + ".bak";
        QFile::remove(bak);
        QFile::copy(labelPath, bak);
    }
}

QString CloudAutoLabeler::labelPathFor(const QString &imagePath)
{
    return QFileInfo(imagePath).dir().filePath(
        QFileInfo(imagePath).baseName() + ".txt");
}

// Validates and filters server-returned YOLO text.
// Each line must have exactly 5 fields, all numeric, with classId in [0, numClasses)
// and coordinates/dimensions within [0, 1] range.
// Returns empty string when numClasses <= 0 (no class file loaded) to prevent
// writing unvalidatable data to disk.
QString CloudAutoLabeler::filterValidDetections(const QString &yoloTxt, int numClasses)
{
    // Reject all results when no class file is loaded — class IDs cannot be validated.
    if (numClasses <= 0) return QString();

    QString result;
    for (const QString &rawLine : yoloTxt.split('\n')) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() != 5) continue; // reject malformed lines

        bool okId, okCx, okCy, okW, okH;
        const int    classId = parts[0].toInt(&okId);
        const double cx      = parts[1].toDouble(&okCx);
        const double cy      = parts[2].toDouble(&okCy);
        const double w       = parts[3].toDouble(&okW);
        const double h       = parts[4].toDouble(&okH);

        if (!okId || !okCx || !okCy || !okW || !okH) continue; // non-numeric fields
        if (classId < 0 || classId >= numClasses)    continue; // class ID out of range
        if (cx < 0.0 || cx > 1.0 || cy < 0.0 || cy > 1.0) continue; // center out of [0,1]
        if (w <= 0.0 || w > 1.0  || h <= 0.0 || h > 1.0)  continue; // size out of (0,1]

        result += line + '\n';
    }
    return result;
}

// Remaps yolo_txt using the server-provided class_names list (Option B).
// serverClassNames[i] is the class name the server assigned to ID i.
// Matches by name against the local class list and rewrites each line with
// the local class ID. Lines whose server class is not in the local file are
// dropped. Coordinate validation is preserved.
QString CloudAutoLabeler::remapWithClassNames(
    const QString &yoloTxt,
    const QStringList &serverClassNames,
    const QStringList &localClasses)
{
    if (localClasses.isEmpty()) return QString();

    QHash<int, int> remap;
    for (int serverId = 0; serverId < serverClassNames.size(); ++serverId)
        remap[serverId] = localClasses.indexOf(serverClassNames[serverId]);

    QString result;
    for (const QString &rawLine : yoloTxt.split('\n')) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() != 5) continue;

        bool okId, okCx, okCy, okW, okH;
        const int    serverId = parts[0].toInt(&okId);
        const double cx       = parts[1].toDouble(&okCx);
        const double cy       = parts[2].toDouble(&okCy);
        const double w        = parts[3].toDouble(&okW);
        const double h        = parts[4].toDouble(&okH);

        if (!okId || !okCx || !okCy || !okW || !okH)        continue;
        if (cx < 0.0 || cx > 1.0 || cy < 0.0 || cy > 1.0)  continue;
        if (w <= 0.0 || w > 1.0  || h <= 0.0 || h > 1.0)   continue;

        const int localId = remap.value(serverId, -1);
        if (localId == -1) continue;

        result += QString::number(localId) + line.mid(line.indexOf(' ')) + '\n';
    }
    return result;
}

// ── Single-image flow ───────────────────────────────────────────────────────

void CloudAutoLabeler::processNextInQueue()
{
    if (m_cancelRequested || m_queue.isEmpty()) {
        if (!m_cancelRequested)
            emit finished(m_allPaths.size());
        resetState();
        setBusy(false);
        return;
    }

    int idx  = m_queue.takeFirst();
    int done = m_allPaths.size() - m_queue.size() - 1;
    emit progress(done, m_allPaths.size());
    submitSingle(m_allPaths[idx]);
}

void CloudAutoLabeler::submitSingle(const QString &imagePath)
{
    m_pendingPath  = imagePath;
    m_retryCount   = 0;

    QString effectivePrompt = m_prompt.isEmpty() ? m_classes.join(" ; ") : m_prompt;

    QJsonArray classesArr;
    for (const QString &c : m_classes)
        classesArr.append(c);

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // Use setBodyDevice() so the file is streamed directly without a redundant in-memory copy.
    // QFile is parented to multiPart and will be deleted when the reply is deleted.
    QFile *imgFile = new QFile(imagePath, multiPart);
    if (!imgFile->open(QIODevice::ReadOnly)) {
        delete multiPart;
        handleFatalError("Cannot read image: " + imagePath);
        return;
    }

    QHttpPart imgPart;
    imgPart.setHeader(QNetworkRequest::ContentTypeHeader, mimeForImage(imagePath));
    imgPart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QString("form-data; name=\"image\"; filename=\"%1\"")
            .arg(QFileInfo(imagePath).fileName()));
    imgPart.setBodyDevice(imgFile);
    multiPart->append(imgPart);

    QHttpPart promptPart;
    promptPart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"prompt\"");
    promptPart.setBody(effectivePrompt.toUtf8());
    multiPart->append(promptPart);

    QHttpPart classesPart;
    classesPart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"classes\"");
    classesPart.setBody(QJsonDocument(classesArr).toJson(QJsonDocument::Compact));
    multiPart->append(classesPart);

    QNetworkRequest req = makeRequest("/v1/jobs");
    req.setTransferTimeout(30000);

    QNetworkReply *reply = m_net->post(req, multiPart);
    multiPart->setParent(reply);

    const int gen = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, imagePath, gen]() {
        reply->deleteLater();
        if (m_generation != gen) return;

        if (reply->error() != QNetworkReply::NoError) {
            if (++m_retryCount <= MAX_RETRIES) {
                emit statusMessage(
                    QString("Submit failed (%1/%2), retrying\u2026")
                        .arg(m_retryCount).arg(MAX_RETRIES), 2000);
                submitSingle(imagePath);
                return;
            }
            handleFatalError("Submit failed: " + reply->errorString());
            return;
        }

        m_retryCount = 0;
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isNull() || !doc.isObject() || !doc.object().contains("job_id")) {
            handleFatalError("Unexpected server response.");
            return;
        }
        m_pendingJobId = doc.object()["job_id"].toVariant().toLongLong();
        m_pollCount    = 0;
        m_pollTimer->start();
    });
}

void CloudAutoLabeler::pollSingle()
{
    if (m_cancelRequested) return;
    if (m_singlePolling) return;  // previous poll reply still in-flight

    if (++m_pollCount > MAX_POLLS) {
        handleFatalError("Job timed out. Please try again.");
        return;
    }

    QNetworkRequest req = makeRequest(
        QString("/v1/jobs/%1").arg(m_pendingJobId));
    req.setTransferTimeout(10000);

    m_singlePolling = true;
    const int gen = m_generation;
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gen]() {
        reply->deleteLater();
        m_singlePolling = false;
        if (m_generation != gen) return;

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isNull() || !doc.isObject()) return; // keep polling

        const QString status = doc.object()["status"].toString();
        if (status == "succeeded") {
            m_pollTimer->stop();
            m_pollCount = 0;
            fetchSingleResult();
        } else if (status == "failed") {
            handleFatalError("Job failed: " +
                doc.object().value("error_message").toString());
        }
        // queued / running → keep polling
    });
}

void CloudAutoLabeler::fetchSingleResult(int retryCount)
{
    QNetworkRequest req = makeRequest(
        QString("/v1/jobs/%1/result").arg(m_pendingJobId));
    req.setTransferTimeout(15000);

    const int gen = m_generation;
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, gen, retryCount]() {
        reply->deleteLater();
        if (m_generation != gen) return;

        if (reply->error() != QNetworkReply::NoError) {
            if (retryCount < MAX_RETRIES) {
                emit statusMessage(
                    QString("Fetch failed (%1/%2), retrying\u2026")
                        .arg(retryCount + 1).arg(MAX_RETRIES), 2000);
                fetchSingleResult(retryCount + 1);
                return;
            }
            handleFatalError("Fetch result failed: " + reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isNull() || !doc.isObject()) {
            handleFatalError("Failed to parse job result.");
            return;
        }

        const QJsonObject result = doc.object();
        const QString     raw    = result.value("yolo_txt").toString();
        QString yoloTxt;
        if (result.contains("class_names")) {
            QStringList serverNames;
            for (const QJsonValue &v : result.value("class_names").toArray())
                serverNames.append(v.toString());
            yoloTxt = remapWithClassNames(raw, serverNames, m_classes);
        } else {
            yoloTxt = filterValidDetections(raw, m_classes.size());
        }
        const int         n       = yoloTxt.isEmpty() ? 0
                                    : yoloTxt.trimmed().count('\n') + 1;
        const int         ms      = result.value("compute_ms").toInt();

        const QString lp = labelPathFor(m_pendingPath);
        backupLabelFile(lp);
        QFile lf(lp);
        if (!lf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            emit statusMessage(
                QString("Cloud auto-label: write failed for %1").arg(lp), 5000);
            processNextInQueue();
            return;
        }
        lf.write(yoloTxt.toUtf8());
        lf.close();

        emit statusMessage(
            QString("Cloud auto-label: %1 detection(s) in %2 ms").arg(n).arg(ms), 4000);
        emit labelReady(m_pendingPath, n, ms);

        processNextInQueue();
    });
}

// ── Batch flow ──────────────────────────────────────────────────────────────

void CloudAutoLabeler::submitBatchChunk(const QStringList &imagePaths)
{
    m_batchPaths = imagePaths;
    m_batchJobIds.clear();
    m_retryCount = 0;

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    for (const QString &imagePath : imagePaths) {
        // Use setBodyDevice() so each file is streamed without an in-memory copy.
        // QFile is parented to multiPart and deleted automatically with the reply.
        QFile *imgFile = new QFile(imagePath, multiPart);
        if (!imgFile->open(QIODevice::ReadOnly)) {
            delete multiPart;
            handleFatalError("Cannot read image: " + imagePath);
            return;
        }

        QHttpPart imgPart;
        imgPart.setHeader(QNetworkRequest::ContentTypeHeader, mimeForImage(imagePath));
        imgPart.setHeader(QNetworkRequest::ContentDispositionHeader,
            QString("form-data; name=\"images\"; filename=\"%1\"")
                .arg(QFileInfo(imagePath).fileName()));
        imgPart.setBodyDevice(imgFile);
        multiPart->append(imgPart);
    }

    const QString effectivePrompt = m_prompt.isEmpty() ? m_classes.join(" ; ") : m_prompt;

    QHttpPart promptPart;
    promptPart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"prompt\"");
    promptPart.setBody(effectivePrompt.toUtf8());
    multiPart->append(promptPart);

    QJsonArray classesArr;
    for (const QString &c : m_classes)
        classesArr.append(c);

    QHttpPart classesPart;
    classesPart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"classes\"");
    classesPart.setBody(QJsonDocument(classesArr).toJson(QJsonDocument::Compact));
    multiPart->append(classesPart);

    QNetworkRequest req = makeRequest("/v1/jobs/batch");
    req.setTransferTimeout(60000);

    QNetworkReply *reply = m_net->post(req, multiPart);
    multiPart->setParent(reply);

    const int gen = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, imagePaths, gen]() {
        reply->deleteLater();
        if (m_generation != gen) return;

        if (reply->error() != QNetworkReply::NoError) {
            if (++m_retryCount <= MAX_RETRIES) {
                emit statusMessage(
                    QString("Batch submit failed (%1/%2), retrying\u2026")
                        .arg(m_retryCount).arg(MAX_RETRIES), 2000);
                submitBatchChunk(imagePaths);
                return;
            }
            handleFatalError("Batch submit failed: " + reply->errorString());
            return;
        }

        m_retryCount = 0;
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isNull() || !doc.isObject() || !doc.object().contains("job_ids")) {
            handleFatalError("Unexpected batch response from server.");
            return;
        }
        for (const QJsonValue &v : doc.object()["job_ids"].toArray())
            m_batchJobIds.append(v.toVariant().toLongLong());

        // Validate that the server returned exactly one job ID per submitted image
        if (m_batchJobIds.size() != m_batchPaths.size()) {
            handleFatalError(
                QString("Server returned %1 job IDs for %2 images — aborting.")
                    .arg(m_batchJobIds.size()).arg(m_batchPaths.size()));
            return;
        }

        // Initialize per-job status tracking (0 = pending)
        m_batchJobStatuses.resize(m_batchJobIds.size());
        m_batchJobStatuses.fill(0);

        m_pollCount = 0;
        m_pollTimer->start();
    });
}

void CloudAutoLabeler::pollBatch()
{
    if (m_batchPolling) return;

    if (++m_pollCount > MAX_POLLS) {
        const int pending = m_batchJobStatuses.count(0);
        handleFatalError(
            QString("Batch jobs timed out: %1/%2 jobs still pending.")
                .arg(pending).arg(m_batchJobIds.size()));
        return;
    }

    // Only poll jobs that are still pending (status 0)
    QList<int> pendingIdxs;
    for (int i = 0; i < m_batchJobIds.size(); ++i)
        if (i < m_batchJobStatuses.size() && m_batchJobStatuses[i] == 0)
            pendingIdxs.append(i);

    if (pendingIdxs.isEmpty()) return;  // all terminal, timer will be stopped

    // Rate-limit: poll at most MAX_CONCURRENT_POLLS jobs per tick.
    // Remaining pending jobs are polled on the next tick when their status is still 0.
    if (pendingIdxs.size() > MAX_CONCURRENT_POLLS)
        pendingIdxs = pendingIdxs.mid(0, MAX_CONCURRENT_POLLS);

    m_batchPolling = true;
    const int pollCount = pendingIdxs.size();
    auto remaining = std::make_shared<int>(pollCount);

    const int gen = m_generation;
    for (int i : pendingIdxs) {
        const qint64 jobId = m_batchJobIds[i];
        QNetworkRequest req = makeRequest(QString("/v1/jobs/%1").arg(jobId));
        req.setTransferTimeout(10000);

        QNetworkReply *reply = m_net->get(req);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, i, remaining, gen]() {
            reply->deleteLater();
            if (m_generation != gen) {
                if (--(*remaining) == 0) m_batchPolling = false;
                return;
            }

            if (i < m_batchJobStatuses.size()) {
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                if (!doc.isNull() && doc.isObject()) {
                    const QString status = doc.object()["status"].toString();
                    if (status == "succeeded")
                        m_batchJobStatuses[i] = 1;
                    else if (status == "failed")
                        m_batchJobStatuses[i] = 2;
                }
            }

            if (--(*remaining) == 0) {
                m_batchPolling = false;

                // Check whether all jobs have reached a terminal state
                const bool allTerminal = !m_batchJobStatuses.contains(0);
                if (!allTerminal) return;  // keep polling

                m_pollTimer->stop();
                m_pollCount = 0;

                const int succeeded = m_batchJobStatuses.count(1);
                const int failed    = m_batchJobStatuses.count(2);

                if (failed > 0) {
                    emit statusMessage(
                        QString("%1/%2 batch jobs failed — fetching %3 result(s).")
                            .arg(failed).arg(m_batchJobIds.size()).arg(succeeded), 5000);
                    m_batchFailed += failed;
                }

                if (succeeded > 0) {
                    m_batchChunkWriteSucceeded = 0; // reset before fetch pass for this chunk
                    fetchBatchResults(0);  // skips failed indices
                } else {
                    handleFatalError("All batch jobs in this chunk failed.");
                }
            }
        });
    }
}

void CloudAutoLabeler::fetchBatchResults(int idx, int retryCount)
{
    const int gen = m_generation;  // captured for lambdas in this call frame

    if (idx >= m_batchJobIds.size()) {
        // Chunk complete — count only images where the file was actually written
        m_batchDone += m_batchChunkWriteSucceeded;
        m_batchJobIds.clear();
        m_batchJobStatuses.clear();
        m_batchPaths.clear();
        emit progress(m_batchDone, m_batchTotal);

        if (!m_batchChunks.isEmpty()) {
            submitBatchChunk(m_batchChunks.takeFirst());
        } else {
            const int totalFailed = m_batchFailed;
            emit finished(m_batchDone);
            if (totalFailed > 0) {
                emit statusMessage(
                    QString("Cloud auto-label: %1 done, %2 failed.")
                        .arg(m_batchDone).arg(totalFailed), 6000);
            } else {
                emit statusMessage(
                    QString("Cloud auto-label: %1 images done.").arg(m_batchDone), 5000);
            }
            resetState();
            setBusy(false);
        }
        return;
    }

    // Skip jobs that failed on the server — just move to the next
    if (idx < m_batchJobStatuses.size() && m_batchJobStatuses[idx] == 2) {
        fetchBatchResults(idx + 1);
        return;
    }

    const qint64  jobId     = m_batchJobIds[idx];
    const QString imagePath = m_batchPaths[idx];

    QNetworkRequest req = makeRequest(QString("/v1/jobs/%1/result").arg(jobId));
    req.setTransferTimeout(15000);

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, imagePath, idx, gen, retryCount]() {
        reply->deleteLater();
        if (m_generation != gen) return;

        if (reply->error() != QNetworkReply::NoError) {
            if (retryCount < MAX_RETRIES) {
                fetchBatchResults(idx, retryCount + 1);
                return;
            }
            // Fetch failed after retries — count as failure, move to next image
            ++m_batchFailed;
            fetchBatchResults(idx + 1);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isNull() && doc.isObject()) {
            const QJsonObject obj = doc.object();
            const QString     raw = obj.value("yolo_txt").toString();
            QString yoloTxt;
            if (obj.contains("class_names")) {
                QStringList serverNames;
                for (const QJsonValue &v : obj.value("class_names").toArray())
                    serverNames.append(v.toString());
                yoloTxt = remapWithClassNames(raw, serverNames, m_classes);
            } else {
                yoloTxt = filterValidDetections(raw, m_classes.size());
            }
            const QString lp      = labelPathFor(imagePath);
            backupLabelFile(lp);
            QFile lf(lp);
            if (!lf.open(QIODevice::WriteOnly | QIODevice::Text)) {
                // File write failed — count as failure, do not emit labelReady
                ++m_batchFailed;
                fetchBatchResults(idx + 1);
                return;
            }
            lf.write(yoloTxt.toUtf8());
            lf.close();

            ++m_batchChunkWriteSucceeded;
            const int n = yoloTxt.isEmpty() ? 0
                          : yoloTxt.trimmed().count('\n') + 1;
            emit labelReady(imagePath, n, 0);
        } else {
            // JSON parse failure — count as failure, move to next image
            ++m_batchFailed;
        }
        fetchBatchResults(idx + 1);
    });
}

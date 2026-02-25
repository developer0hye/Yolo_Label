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
    m_batchMode    = false;
    m_batchPolling = false;
    m_batchJobIds.clear();
    m_batchPaths.clear();
    m_batchChunks.clear();
    m_batchTotal   = 0;
    m_batchDone    = 0;
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

    QFile f(imagePath);
    if (!f.open(QIODevice::ReadOnly)) {
        handleFatalError("Cannot read image: " + imagePath);
        return;
    }
    QByteArray imageData = f.readAll();
    f.close();

    QString effectivePrompt = m_prompt.isEmpty() ? m_classes.join(" ; ") : m_prompt;

    QJsonArray classesArr;
    for (const QString &c : m_classes)
        classesArr.append(c);

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart imgPart;
    imgPart.setHeader(QNetworkRequest::ContentTypeHeader, mimeForImage(imagePath));
    imgPart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QString("form-data; name=\"image\"; filename=\"%1\"")
            .arg(QFileInfo(imagePath).fileName()));
    imgPart.setBody(imageData);
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

    connect(reply, &QNetworkReply::finished, this, [this, reply, imagePath]() {
        reply->deleteLater();
        if (m_cancelRequested) return;

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

    if (++m_pollCount > MAX_POLLS) {
        handleFatalError("Job timed out. Please try again.");
        return;
    }

    QNetworkRequest req = makeRequest(
        QString("/v1/jobs/%1").arg(m_pendingJobId));
    req.setTransferTimeout(10000);

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_cancelRequested) return;

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

void CloudAutoLabeler::fetchSingleResult()
{
    QNetworkRequest req = makeRequest(
        QString("/v1/jobs/%1/result").arg(m_pendingJobId));
    req.setTransferTimeout(15000);

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_cancelRequested) return;

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isNull() || !doc.isObject()) {
            handleFatalError("Failed to parse job result.");
            return;
        }

        const QJsonObject result  = doc.object();
        const QString     yoloTxt = result.value("yolo_txt").toString();
        const int         n       = result.value("detections").toArray().size();
        const int         ms      = result.value("compute_ms").toInt();

        const QString lp = labelPathFor(m_pendingPath);
        backupLabelFile(lp);
        QFile lf(lp);
        if (lf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            lf.write(yoloTxt.toUtf8());
            lf.close();
        }
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
        QFile f(imagePath);
        if (!f.open(QIODevice::ReadOnly)) {
            delete multiPart;
            handleFatalError("Cannot read image: " + imagePath);
            return;
        }
        QByteArray imageData = f.readAll();
        f.close();

        QHttpPart imgPart;
        imgPart.setHeader(QNetworkRequest::ContentTypeHeader, mimeForImage(imagePath));
        imgPart.setHeader(QNetworkRequest::ContentDispositionHeader,
            QString("form-data; name=\"images\"; filename=\"%1\"")
                .arg(QFileInfo(imagePath).fileName()));
        imgPart.setBody(imageData);
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

    connect(reply, &QNetworkReply::finished, this, [this, reply, imagePaths]() {
        reply->deleteLater();
        if (m_cancelRequested) return;

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

        m_pollCount = 0;
        m_pollTimer->start();
    });
}

void CloudAutoLabeler::pollBatch()
{
    if (m_cancelRequested) return;
    if (m_batchPolling) return;

    if (++m_pollCount > MAX_POLLS) {
        handleFatalError("Batch jobs timed out. Please try again.");
        return;
    }

    m_batchPolling = true;

    const int total = m_batchJobIds.size();
    struct State { int remaining; int succeeded; bool failed; };
    auto state = std::make_shared<State>(State{total, 0, false});

    for (qint64 jobId : m_batchJobIds) {
        QNetworkRequest req = makeRequest(QString("/v1/jobs/%1").arg(jobId));
        req.setTransferTimeout(10000);

        QNetworkReply *reply = m_net->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, state, total]() {
            reply->deleteLater();
            if (m_cancelRequested) {
                if (--state->remaining == 0) m_batchPolling = false;
                return;
            }
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QString status;
            if (!doc.isNull() && doc.isObject())
                status = doc.object()["status"].toString();

            if (status == "succeeded")   ++state->succeeded;
            else if (status == "failed") state->failed = true;

            if (--state->remaining == 0) {
                m_batchPolling = false;
                if (state->failed) {
                    handleFatalError("One or more batch jobs failed.");
                } else if (state->succeeded == total) {
                    m_pollTimer->stop();
                    m_pollCount = 0;
                    fetchBatchResults(0);
                }
                // else: still queued/running — keep polling
            }
        });
    }
}

void CloudAutoLabeler::fetchBatchResults(int idx)
{
    if (m_cancelRequested) return;

    if (idx >= m_batchJobIds.size()) {
        // Chunk complete
        m_batchDone += m_batchJobIds.size();
        m_batchJobIds.clear();
        m_batchPaths.clear();
        emit progress(m_batchDone, m_batchTotal);

        if (!m_batchChunks.isEmpty()) {
            submitBatchChunk(m_batchChunks.takeFirst());
        } else {
            emit finished(m_batchTotal);
            emit statusMessage(
                QString("Cloud auto-label: %1 images done.").arg(m_batchTotal), 5000);
            resetState();
            setBusy(false);
        }
        return;
    }

    const qint64  jobId     = m_batchJobIds[idx];
    const QString imagePath = m_batchPaths[idx];

    QNetworkRequest req = makeRequest(QString("/v1/jobs/%1/result").arg(jobId));
    req.setTransferTimeout(15000);

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, imagePath, idx]() {
        reply->deleteLater();
        if (m_cancelRequested) return;

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isNull() && doc.isObject()) {
            const QString yoloTxt = doc.object().value("yolo_txt").toString();
            const QString lp = labelPathFor(imagePath);
            backupLabelFile(lp);
            QFile lf(lp);
            if (lf.open(QIODevice::WriteOnly | QIODevice::Text)) {
                lf.write(yoloTxt.toUtf8());
                lf.close();
            }
            const int n = doc.object().value("detections").toArray().size();
            emit labelReady(imagePath, n, 0);
        }
        fetchBatchResults(idx + 1);
    });
}

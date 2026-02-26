#ifndef CLOUD_LABELER_H
#define CLOUD_LABELER_H

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>

// CloudAutoLabeler encapsulates all network communication with the
// yololabel.com cloud inference API. It mirrors the YoloDetector pattern:
// MainWindow holds an instance, calls its public methods, and reacts to
// its signals — keeping all networking out of MainWindow.
class CloudAutoLabeler : public QObject
{
    Q_OBJECT

public:
    explicit CloudAutoLabeler(QObject *parent = nullptr);

    // Configuration — call before labelImage() / labelImages()
    void setApiKey(const QString &apiKey);
    void setPrompt(const QString &prompt);
    void setClasses(const QStringList &classes);

    QString apiKey()  const { return m_apiKey; }
    QString prompt()  const { return m_prompt; }
    bool    isBusy()  const { return m_busy; }

    // Actions
    void labelImage(const QString &imagePath);          // single image
    void labelImages(const QStringList &imagePaths);    // batch (all images)
    void cancel();

signals:
    // Emitted when a label file has been written for one image.
    void labelReady(const QString &imagePath, int nDetections, int computeMs);
    // Emitted after every image completes during a batch run.
    void progress(int done, int total);
    // Emitted when all images (single or batch) are done.
    void finished(int total);
    // Emitted on unrecoverable errors.
    void errorOccurred(const QString &message);
    // Short-lived status bar messages.
    void statusMessage(const QString &message, int timeoutMs);
    // True while a job or batch is in flight.
    void busyChanged(bool busy);

private:
    // Helpers
    void setBusy(bool busy);
    void resetState();
    void submitSingle(const QString &imagePath);
    void submitBatchChunk(const QStringList &imagePaths);
    void pollSingle();
    void pollBatch();
    void fetchSingleResult(int retryCount = 0);
    void fetchBatchResults(int idx, int retryCount = 0);
    void processNextInQueue();
    void handleFatalError(const QString &message);

    static QString  mimeForImage(const QString &path);
    static void     backupLabelFile(const QString &labelPath);
    static QString  labelPathFor(const QString &imagePath);
    static QString  filterValidDetections(const QString &yoloTxt, int numClasses);
    QNetworkRequest makeRequest(const QString &endpoint) const;

    static constexpr const char *API_HOST            = "https://api.yololabel.com";
    static constexpr int         POLL_INTERVAL        = 1500;  // ms
    static constexpr int         MAX_POLLS            = 200;   // ~5 min
    static constexpr int         BATCH_SIZE           = 20;
    static constexpr int         MAX_RETRIES          = 3;
    static constexpr int         MAX_CONCURRENT_POLLS = 5;     // per tick, to avoid rate-limit

    QNetworkAccessManager *m_net;
    QTimer                *m_pollTimer;

    QString       m_apiKey;
    QString       m_prompt;
    QStringList   m_classes;

    bool          m_busy           = false;
    bool          m_cancelRequested= false;
    int           m_generation     = 0;    // incremented on cancel to invalidate in-flight callbacks
    int           m_pollCount      = 0;
    int           m_retryCount     = 0;

    // Single-image state
    qint64        m_pendingJobId   = -1;
    QString       m_pendingPath;
    QList<int>    m_queue;          // indices into m_allPaths

    // Single-poll in-flight guard
    bool          m_singlePolling  = false;

    // Batch state
    bool                m_batchMode     = false;
    bool                m_batchPolling  = false;
    QList<qint64>       m_batchJobIds;
    QVector<int>        m_batchJobStatuses;  // 0=pending, 1=succeeded, 2=failed
    QStringList         m_batchPaths;
    QList<QStringList>  m_batchChunks;
    int                 m_batchTotal              = 0;
    int                 m_batchDone               = 0;
    int                 m_batchFailed             = 0;  // cumulative failed count across chunks
    int                 m_batchChunkWriteSucceeded = 0; // write successes in the current chunk's fetch pass

    QStringList   m_allPaths;  // full image list for queue-based "label all"
};

#endif // CLOUD_LABELER_H

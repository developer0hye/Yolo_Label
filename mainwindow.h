#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWheelEvent>
#include <QTableWidgetItem>
#include <QMessageBox>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QComboBox>

#include "label_img.h"
#ifdef ONNXRUNTIME_AVAILABLE
#include "yolo_detector.h"
#endif
#include <fstream>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void set_args(int argc, char *argv[]);


private slots:
    void on_pushButton_open_files_clicked();
    void on_pushButton_prev_clicked();
    void on_pushButton_next_clicked();

    void keyPressEvent(QKeyEvent *);

    void next_img(bool bSavePrev = true);
    void prev_img(bool bSavePrev = true);
    void save_label_data();
    void clear_label_data();
    void remove_img();

    void next_label();
    void prev_label();

    void on_tableWidget_label_cellDoubleClicked(int , int );
    void on_tableWidget_label_cellClicked(int , int );

    void on_horizontalSlider_images_sliderMoved(int );

    void on_horizontalSlider_contrast_sliderMoved(int value);

    void on_checkBox_visualize_class_name_clicked(bool checked);

    void copy_annotations();
    void paste_annotations();

    void undo();
    void redo();

    void on_usageTimer_timeout();
    void on_usageTimerReset_clicked();

    void reset_zoom();

private:
    void updateUsageTimerLabel();

    void            init();
    void            init_table_widget();
    void            init_horizontal_slider();

    void            set_label_progress(const int);
    void            set_focused_file(const int);

    void            goto_img(const int);

    void            load_label_list_data(QString);
    QString         get_labeling_data(QString)const;

    void            saveSession();
    void            restoreLastSession();

    void            set_label(const int);
    void            set_label_color(const int , const QColor);

    void            pjreddie_style_msgBox(QMessageBox::Icon, QString, QString);

    void            open_img_dir(bool&);
    void            open_obj_file(bool&);
    bool            get_files(QString imgDir);

    // ── Cloud auto-label ───────────────────────────────────────────────
    QNetworkAccessManager *m_cloudNet;
    QPushButton           *m_btnCloudAutoLabel;
    QPushButton           *m_btnCloudAutoLabelAll;
    QTimer                *m_pollTimer;
    int                    m_pendingJobId = -1;
    QString                m_pendingImagePath;
    QList<int>             m_cloudQueue;

    QString m_cloudHost;
    QString m_cloudApiKey;
    QString m_cloudPrompt;

    QTabWidget *m_sideTabWidget;
    QLineEdit  *m_settingsKeyEdit;
    QLineEdit  *m_settingsPromptEdit;

    void initSideTabWidget();
    void syncAiSettingsTab();
    void saveAiSettings();

    // ── autolabel-cloud ───────────────────────────────────────────────
    void submitCloudJob();
    void doSubmitCloudJob(const QString &imagePath);
    void cloudAutoLabelAll();
    void cloudProcessNextInQueue();
    void pollJobStatus();
    void fetchJobResult();
    void openCloudSettings();

    // batch
    void doSubmitBatchCloudJob(const QStringList &imagePaths);
    void pollBatchJobStatuses();
    void fetchAllBatchResults(int idx);

    bool             m_batchMode         = false;
    bool             m_batchPolling      = false;
    QList<int>       m_batchPendingJobIds;
    QStringList      m_batchPendingPaths;
    QList<QStringList> m_batchChunks;
    int              m_batchTotalImages  = 0;
    int              m_batchDoneImages   = 0;

    // ── Landing AI ────────────────────────────────────────────────────
    QComboBox   *m_modelCombo;
    QString      m_landingApiKey;
    QList<int>   m_landingQueue;
    QString      m_landingPendingImagePath;

    void submitLandingAIJob();
    void doLandingAIJob(const QString &imagePath);
    void landingAIAutoLabelAll();
    void landingAIProcessNextInQueue();
    // ──────────────────────────────────────────────────────────────────

    Ui::MainWindow *ui;

    QString         m_imgDir;
    QString         m_objFilePath;
    QStringList     m_imgList;
    int             m_imgIndex;

    QStringList     m_objList;
    int             m_objIndex;
    int             m_lastLabeledImgIndex;

    QVector<ObjectLabelingBox> m_copiedAnnotations;

    QTimer         *m_usageTimer;
    qint64          m_usageTimerElapsedSeconds;
    QLabel         *m_usageTimerLabel;
    QPushButton    *m_usageTimerResetButton;

#ifdef ONNXRUNTIME_AVAILABLE
    YoloDetector    m_detector;

    QPushButton    *m_btnLoadModel;
    QPushButton    *m_btnAutoLabel;
    QPushButton    *m_btnAutoLabelAll;
    QSlider        *m_sliderConfidence;
    QLabel         *m_labelConfidence;
    QLabel         *m_labelModelStatus;

    void on_loadModel_clicked();
    void loadOnnxModel(const QString& modelPath);
    void on_autoLabel_clicked();
    void on_autoLabelAll_clicked();
    void on_confidenceSlider_changed(int value);
    void applyDetections(const std::vector<DetectionResult>& detections);
    void loadClassesFromModel();
    float getConfidenceThreshold() const;
#endif

protected:
    void    wheelEvent(QWheelEvent *);

};

#endif // MAINWINDOW_H

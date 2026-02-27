#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QFileDialog>
#include <QColorDialog>
#include <QKeyEvent>
#include <QShortcut>
#include <QCollator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QSettings>
#include <QTextStream>
#include <QVBoxLayout>
#ifdef ONNXRUNTIME_AVAILABLE
#include <QProgressDialog>
#endif
#include <iomanip>
#include <cmath>

using std::ofstream;
using std::ifstream;
using std::string;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), this), &QShortcut::activated, this, &MainWindow::save_label_data);
    connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Delete), this), &QShortcut::activated, this, &MainWindow::clear_label_data);

    connect(new QShortcut(QKeySequence(Qt::Key_S), this), &QShortcut::activated, this, &MainWindow::next_label);
    connect(new QShortcut(QKeySequence(Qt::Key_W), this), &QShortcut::activated, this, &MainWindow::prev_label);
    connect(new QShortcut(QKeySequence(Qt::Key_A), this), &QShortcut::activated, this, [this]() { prev_img(); });
    connect(new QShortcut(QKeySequence(Qt::Key_D), this), &QShortcut::activated, this, [this]() { next_img(); });
    connect(new QShortcut(QKeySequence(Qt::Key_Space), this), &QShortcut::activated, this, [this]() { next_img(); });
    connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), this), &QShortcut::activated, this, &MainWindow::remove_img);
    connect(new QShortcut(QKeySequence(Qt::Key_Delete), this), &QShortcut::activated, this, &MainWindow::remove_img);
    connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_C), this), &QShortcut::activated, this, &MainWindow::copy_annotations);
    connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_V), this), &QShortcut::activated, this, &MainWindow::paste_annotations);
    connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_0), this), &QShortcut::activated, this, &MainWindow::reset_zoom);

    ui->checkBox_visualize_class_name->setStyleSheet(
        "QCheckBox { color: rgb(0, 255, 255); }"
        "QCheckBox::indicator { width: 18px; height: 18px; border: 2px solid rgb(0, 255, 255); background-color: white; border-radius: 3px; }"
        "QCheckBox::indicator:checked { background-color: rgb(0, 255, 255); }"
    );

    m_usageTimerElapsedSeconds = 0;
    m_usageTimer = new QTimer(this);
    m_usageTimer->setInterval(1000);
    connect(m_usageTimer, &QTimer::timeout, this, &MainWindow::on_usageTimer_timeout);
    m_usageTimer->start();

    m_usageTimerLabel = new QLabel(this);
    m_usageTimerLabel->setStyleSheet(
        "color: rgb(0, 255, 255); font-weight: bold; font-family: 'Consolas', 'Courier New', monospace; font-size: 15px; padding: 2px 8px;"
    );
    m_usageTimerLabel->setMinimumWidth(150);
    m_usageTimerResetButton = new QPushButton(QStringLiteral("\u21BA"), this);
    m_usageTimerResetButton->setFixedSize(30, 30);
    m_usageTimerResetButton->setToolTip(tr("Reset Timer"));
    m_usageTimerResetButton->setStyleSheet(
        "QPushButton { background-color: rgb(0, 0, 17); color: rgb(0, 255, 255); border: 1px solid rgb(0, 255, 255); border-radius: 15px; font-size: 18px; font-weight: bold; }"
        "QPushButton:hover { background-color: rgb(0, 40, 60); }"
        "QPushButton:pressed { background-color: rgb(0, 80, 100); }"
    );
    connect(m_usageTimerResetButton, &QPushButton::clicked, this, &MainWindow::on_usageTimerReset_clicked);
    updateUsageTimerLabel();
    ui->statusBar->setStyleSheet(
        "QStatusBar { background-color: rgb(0, 0, 17); border: none; }"
        "QStatusBar::item { border: none; }"
    );
    ui->statusBar->addPermanentWidget(m_usageTimerLabel);
    ui->statusBar->addPermanentWidget(m_usageTimerResetButton);

    QShortcut *undoShortcut = new QShortcut(QKeySequence::Undo, this);
    undoShortcut->setContext(Qt::ApplicationShortcut);
    connect(undoShortcut, &QShortcut::activated, this, &MainWindow::undo);

    QShortcut *redoShortcut = new QShortcut(QKeySequence::Redo, this);
    redoShortcut->setContext(Qt::ApplicationShortcut);
    connect(redoShortcut, &QShortcut::activated, this, &MainWindow::redo);

#ifdef ONNXRUNTIME_AVAILABLE
    // --- Auto-Label Controls ---
    QString btnStyle =
        "QPushButton { background-color: rgb(0, 0, 17); color: rgb(0, 255, 255); border: 2px solid rgb(0, 255, 255); border-radius: 4px; padding: 4px 8px; font-weight: bold; font-size: 12px; }"
        "QPushButton:hover { background-color: rgb(0, 40, 60); }"
        "QPushButton:pressed { background-color: rgb(0, 80, 100); }"
        "QPushButton:disabled { color: rgb(80, 80, 80); border-color: rgb(80, 80, 80); }";

    QString sliderStyle =
        "QSlider::groove:horizontal { border: 1px solid rgb(0, 255, 255); height: 6px; background: rgb(0, 0, 17); border-radius: 3px; }"
        "QSlider::handle:horizontal { background: rgb(255, 187, 0); border: 1px solid rgb(255, 187, 0); width: 14px; margin: -5px 0; border-radius: 7px; }";

    QString labelStyle = "color: rgb(0, 255, 255); font-weight: bold; font-size: 12px;";

    m_btnLoadModel = new QPushButton("Load Model", this);
    m_btnLoadModel->setStyleSheet(btnStyle);

    m_sliderConfidence = new QSlider(Qt::Horizontal, this);
    m_sliderConfidence->setRange(1, 99);
    m_sliderConfidence->setValue(25);
    m_sliderConfidence->setFixedWidth(120);
    m_sliderConfidence->setStyleSheet(sliderStyle);

    m_labelConfidence = new QLabel("Conf: 25%", this);
    m_labelConfidence->setStyleSheet(labelStyle);
    m_labelConfidence->setFixedWidth(70);

    m_btnAutoLabel = new QPushButton("Auto Label", this);
    m_btnAutoLabel->setStyleSheet(btnStyle);
    m_btnAutoLabel->setEnabled(false);
    m_btnAutoLabel->setToolTip(tr("Auto-label current image (R)"));

    m_btnAutoLabelAll = new QPushButton("Auto Label All", this);
    m_btnAutoLabelAll->setStyleSheet(btnStyle);
    m_btnAutoLabelAll->setEnabled(false);

    m_labelModelStatus = new QLabel("No model loaded", this);
    m_labelModelStatus->setStyleSheet(labelStyle);

    QHBoxLayout *autoLabelLayout = new QHBoxLayout();
    autoLabelLayout->setContentsMargins(0, 2, 0, 2);
    autoLabelLayout->addWidget(m_btnLoadModel);
    autoLabelLayout->addWidget(m_sliderConfidence);
    autoLabelLayout->addWidget(m_labelConfidence);
    autoLabelLayout->addWidget(m_btnAutoLabel);
    autoLabelLayout->addWidget(m_btnAutoLabelAll);
    autoLabelLayout->addWidget(m_labelModelStatus, 1);

    ui->gridLayout->addLayout(autoLabelLayout, 1, 0);

    connect(m_btnLoadModel, &QPushButton::clicked, this, &MainWindow::on_loadModel_clicked);
    connect(m_btnAutoLabel, &QPushButton::clicked, this, &MainWindow::on_autoLabel_clicked);
    connect(m_btnAutoLabelAll, &QPushButton::clicked, this, &MainWindow::on_autoLabelAll_clicked);
    connect(m_sliderConfidence, &QSlider::valueChanged, this, &MainWindow::on_confidenceSlider_changed);
    connect(new QShortcut(QKeySequence(Qt::Key_R), this), &QShortcut::activated, this, &MainWindow::on_autoLabel_clicked);
#endif

    // ── Cloud auto-label setup ─────────────────────────────────────────
    {
        QSettings s("YoloLabel", "CloudAI");
        m_cloudApiKey   = s.value("apiKey",      "").toString();
        m_cloudPrompt   = s.value("prompt",      "").toString();
        m_landingApiKey = s.value("landingApiKey","").toString();
    }

    m_cloudLabeler = new CloudAutoLabeler(this);
    m_cloudLabeler->setApiKey(m_cloudApiKey);
    m_cloudLabeler->setPrompt(m_cloudPrompt);

    m_landingNet = new QNetworkAccessManager(this);

    connect(m_cloudLabeler, &CloudAutoLabeler::busyChanged, this, [this](bool busy) {
        // Disable image slider while a batch is running to prevent navigation
        ui->horizontalSlider_images->setEnabled(!busy);
        if (!busy) resetCloudButtons();
    });
    connect(m_cloudLabeler, &CloudAutoLabeler::progress, this, [this](int done, int total) {
        m_btnCloudAutoLabelAll->setText(
            QString("\u2601 Auto Label All (%1/%2)\u2026").arg(done).arg(total));
    });
    connect(m_cloudLabeler, &CloudAutoLabeler::labelReady, this,
            [this](const QString &imagePath, int /*n*/, int /*ms*/) {
        if (imagePath == m_imgList.value(m_imgIndex))
            goto_img(m_imgIndex);
    });
    connect(m_cloudLabeler, &CloudAutoLabeler::finished, this, [this](int) {
        goto_img(m_imgIndex);
    });
    connect(m_cloudLabeler, &CloudAutoLabeler::errorOccurred, this, [this](const QString &msg) {
        QMessageBox::warning(this, "Auto Label", msg);
    });
    connect(m_cloudLabeler, &CloudAutoLabeler::statusMessage, this,
            [this](const QString &msg, int ms) {
        statusBar()->showMessage(msg, ms);
    });

    QString cloudBtnStyle =
        "QPushButton { background-color: rgb(0, 0, 17); color: rgb(0, 255, 255); "
        "border: 2px solid rgb(0, 255, 255); border-radius: 4px; padding: 4px 12px; "
        "font-weight: bold; font-size: 12px; }"
        "QPushButton:hover { background-color: rgb(0, 40, 60); }"
        "QPushButton:pressed { background-color: rgb(0, 80, 100); }"
        "QPushButton:disabled { color: rgb(80,80,80); border-color: rgb(80,80,80); }";

    m_btnCloudAutoLabel = new QPushButton("\u2601 Auto Label AI", this);
    m_btnCloudAutoLabel->setToolTip("Label current image using cloud AI");
    m_btnCloudAutoLabel->setStyleSheet(cloudBtnStyle);
    connect(m_btnCloudAutoLabel, &QPushButton::clicked, this, [this](){
        if (m_modelCombo->currentIndex() == 1) submitLandingAIJob();
        else                                   submitCloudJob();
    });

    m_btnCloudAutoLabelAll = new QPushButton("\u2601 Auto Label All AI", this);
    m_btnCloudAutoLabelAll->setToolTip("Label all images using cloud AI");
    m_btnCloudAutoLabelAll->setStyleSheet(cloudBtnStyle);
    connect(m_btnCloudAutoLabelAll, &QPushButton::clicked, this, [this](){
        if (m_modelCombo->currentIndex() == 1) landingAIAutoLabelAll();
        else                                   cloudAutoLabelAll();
    });

    m_btnCancelAutoLabel = new QPushButton("\u2715 Cancel", this);
    m_btnCancelAutoLabel->setToolTip("Cancel cloud auto-labeling");
    m_btnCancelAutoLabel->setStyleSheet(
        "QPushButton { background-color: rgb(0, 0, 17); color: rgb(255, 80, 80); "
        "border: 2px solid rgb(255, 80, 80); border-radius: 4px; padding: 4px 12px; "
        "font-weight: bold; font-size: 12px; }"
        "QPushButton:hover { background-color: rgb(40, 0, 0); }");
    m_btnCancelAutoLabel->setVisible(false);
    connect(m_btnCancelAutoLabel, &QPushButton::clicked, this, &MainWindow::cancelAutoLabel);

    {
        QHBoxLayout *cloudLayout = new QHBoxLayout();
        cloudLayout->setContentsMargins(0, 2, 0, 2);
        cloudLayout->addWidget(m_btnCloudAutoLabel);
        cloudLayout->addWidget(m_btnCloudAutoLabelAll);
        cloudLayout->addWidget(m_btnCancelAutoLabel);
        cloudLayout->addStretch();
        // Insert below the ONNX auto-label row (or at row 1 if ONNX is not built)
        int cloudRow = ui->gridLayout->rowCount();
        ui->gridLayout->addLayout(cloudLayout, cloudRow, 0);
    }

    initSideTabWidget();
    // ──────────────────────────────────────────────────────────────────

    init_table_widget();

    QTimer::singleShot(0, this, &MainWindow::restoreLastSession);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::set_args(int argc, char *argv[])
{
  if (argc > 1) {
    QString dir = QString::fromLocal8Bit(argv[1]);
    if (!get_files(dir)) return;

    QString onnxModelPath;

    for (int i = 2; i < argc; ++i) {
      QString arg = QString::fromLocal8Bit(argv[i]);
      if (arg.endsWith(".onnx", Qt::CaseInsensitive)) {
        onnxModelPath = arg;
      } else {
        m_objFilePath = arg;
        load_label_list_data(arg);
      }
    }

#ifdef ONNXRUNTIME_AVAILABLE
    if (!onnxModelPath.isEmpty()) {
      loadOnnxModel(onnxModelPath);
    }
#else
    Q_UNUSED(onnxModelPath);
#endif

    if (m_objList.empty()) return;

    init();
  }
}


void MainWindow::on_pushButton_open_files_clicked()
{
    bool bRetImgDir     = false;
    open_img_dir(bRetImgDir);

    if (!bRetImgDir) return ;

    if (m_objList.empty())
    {
        bool bRetObjFile    = false;
        open_obj_file(bRetObjFile);
        if (!bRetObjFile) return ;
    }

    init();
}

void MainWindow::init()
{
    m_lastLabeledImgIndex = -1;

    ui->label_image->init();

    init_horizontal_slider();

#ifdef ONNXRUNTIME_AVAILABLE
    if (m_detector.isLoaded()) {
        m_btnAutoLabel->setEnabled(true);
        m_btnAutoLabelAll->setEnabled(true);
    }
#endif

    set_label(0);
    goto_img(0);
    saveSession();
}

void MainWindow::saveSession()
{
    QSettings s("YoloLabel", "Session");
    s.setValue("imgDir",  m_imgDir);
    s.setValue("objFile", m_objFilePath);

    QStringList colors;
    for (const QColor &c : ui->label_image->m_drawObjectBoxColor)
        colors << c.name();
    s.setValue("classColors", colors);
}

void MainWindow::restoreLastSession()
{
    // Skip if set_args() already loaded a folder
    if (!m_imgList.isEmpty()) return;

    QSettings s("YoloLabel", "Session");
    const QString lastDir = s.value("imgDir").toString();
    const QString lastObj = s.value("objFile").toString();

    if (lastDir.isEmpty() || !QDir(lastDir).exists()) return;
    if (!get_files(lastDir)) return;

    QStringList savedColors;
    if (!lastObj.isEmpty() && QFile::exists(lastObj)) {
        m_objFilePath = lastObj;
        load_label_list_data(lastObj);
        savedColors = s.value("classColors").toStringList();
    } else if (m_objList.isEmpty()) {
        bool bRet = false;
        open_obj_file(bRet);
        if (!bRet) return;
    }

    init();

    // Restore custom class colors after init() so they aren't overwritten
    for (int i = 0; i < savedColors.size() && i < ui->label_image->m_drawObjectBoxColor.size(); ++i) {
        QColor c(savedColors[i]);
        if (!c.isValid()) continue;
        ui->label_image->m_drawObjectBoxColor[i] = c;
        if (ui->tableWidget_label->item(i, 1))
            ui->tableWidget_label->item(i, 1)->setBackground(c);
    }
    ui->label_image->showImage();

    statusBar()->showMessage("Session restored: " + lastDir, 4000);
}

void MainWindow::set_label_progress(const int fileIndex)
{
    QString strCurFileIndex = QString::number(fileIndex + 1);
    QString strEndFileIndex = QString::number(m_imgList.size());

    ui->label_progress->setText(strCurFileIndex + " / " + strEndFileIndex);
}

void MainWindow::set_focused_file(const int fileIndex)
{
    QString str = "";

    if(m_lastLabeledImgIndex >= 0)
    {
        str += "Last Labeled Image: " + m_imgList.at(m_lastLabeledImgIndex);
        str += '\n';
    }

    str += "Current Image: " + m_imgList.at(fileIndex);

    ui->textEdit_log->setText(str);
}

void MainWindow::goto_img(const int fileIndex)
{
    if (m_imgList.isEmpty() || fileIndex < 0 || fileIndex >= m_imgList.size()) return;

    m_imgIndex = fileIndex;

    bool bImgOpened;
    ui->label_image->openImage(m_imgList.at(m_imgIndex), bImgOpened);
    ui->label_image->resetZoom();

    ui->label_image->clearUndoHistory();
    ui->label_image->loadLabelData(get_labeling_data(m_imgList.at(m_imgIndex)));
    ui->label_image->showImage();

    set_label_progress(m_imgIndex);
    set_focused_file(m_imgIndex);

    //it blocks crash with slider change
    ui->horizontalSlider_images->blockSignals(true);
    ui->horizontalSlider_images->setValue(m_imgIndex);
    ui->horizontalSlider_images->blockSignals(false);
}

void MainWindow::next_img(bool bSavePrev)
{
    if (m_cloudLabeler && m_cloudLabeler->isBusy()) return;
    if(bSavePrev && ui->label_image->isOpened()) save_label_data();
    goto_img(m_imgIndex + 1);
}

void MainWindow::prev_img(bool bSavePrev)
{
    if (m_cloudLabeler && m_cloudLabeler->isBusy()) return;
    if(bSavePrev && ui->label_image->isOpened()) save_label_data();
    goto_img(m_imgIndex - 1);
}

void MainWindow::save_label_data()
{
    if(m_imgList.size() == 0) return;

    QString qstrOutputLabelData = get_labeling_data(m_imgList.at(m_imgIndex));
    ofstream fileOutputLabelData(qPrintable(qstrOutputLabelData));

    if(fileOutputLabelData.is_open())
    {
        for(int i = 0; i < ui->label_image->m_objBoundingBoxes.size(); i++)
        {
            ObjectLabelingBox objBox = ui->label_image->m_objBoundingBoxes[i];

            double midX     = objBox.box.x() + objBox.box.width() / 2.;
            double midY     = objBox.box.y() + objBox.box.height() / 2.;
            double width    = objBox.box.width();
            double height   = objBox.box.height();

            fileOutputLabelData << objBox.label;
            fileOutputLabelData << " ";
            fileOutputLabelData << std::fixed << std::setprecision(6) << midX;
            fileOutputLabelData << " ";
            fileOutputLabelData << std::fixed << std::setprecision(6) << midY;
            fileOutputLabelData << " ";
            fileOutputLabelData << std::fixed << std::setprecision(6) << width;
            fileOutputLabelData << " ";
            fileOutputLabelData << std::fixed << std::setprecision(6) << height << std::endl;
        }
        m_lastLabeledImgIndex = m_imgIndex;
        fileOutputLabelData.close();
    }
}

void MainWindow::clear_label_data()
{
    ui->label_image->clearAllBoxes();
    ui->label_image->showImage();
}

void MainWindow::remove_img()
{
    if(m_imgList.size() > 0) {
        //remove a image
        QFile::remove(m_imgList.at(m_imgIndex));

        //remove a txt file
        QString qstrOutputLabelData = get_labeling_data(m_imgList.at(m_imgIndex));
        QFile::remove(qstrOutputLabelData);

        m_imgList.removeAt(m_imgIndex);

        if(m_imgList.size() == 0)
        {
            pjreddie_style_msgBox(QMessageBox::Information,"End", "In directory, there are not any image. program quit.");
            QCoreApplication::quit();
        }
        else if( m_imgIndex == m_imgList.size())
        {
            m_imgIndex--;
        }

        ui->horizontalSlider_images->setRange(0, m_imgList.size() - 1);
        goto_img(m_imgIndex);
    }
}

void MainWindow::next_label()
{
    set_label(m_objIndex + 1);
}

void MainWindow::prev_label()
{
    set_label(m_objIndex - 1);
}

void MainWindow::load_label_list_data(QString qstrLabelListFile)
{
    ifstream inputLabelListFile(qPrintable(qstrLabelListFile));

    if(inputLabelListFile.is_open())
    {
        for(int i = 0 ; i <= ui->tableWidget_label->rowCount(); i++)
            ui->tableWidget_label->removeRow(ui->tableWidget_label->currentRow());

        m_objList.clear();

        ui->tableWidget_label->setRowCount(0);
        ui->label_image->m_drawObjectBoxColor.clear();

        string strLabel;
        int fileIndex = 0;
        while(getline(inputLabelListFile, strLabel))
        {
            int nRow = ui->tableWidget_label->rowCount();
  
            QString qstrLabel   = QString().fromStdString(strLabel);
            QColor  labelColor  = label_img::BOX_COLORS[(fileIndex++)%10];
            m_objList << qstrLabel;

            ui->tableWidget_label->insertRow(nRow);

            ui->tableWidget_label->setItem(nRow, 0, new QTableWidgetItem(qstrLabel));
            ui->tableWidget_label->item(nRow, 0)->setFlags(ui->tableWidget_label->item(nRow, 0)->flags() ^  Qt::ItemIsEditable);

            ui->tableWidget_label->setItem(nRow, 1, new QTableWidgetItem(QString().fromStdString("")));
            ui->tableWidget_label->item(nRow, 1)->setBackground(labelColor);
            ui->tableWidget_label->item(nRow, 1)->setFlags(ui->tableWidget_label->item(nRow, 1)->flags() ^  Qt::ItemIsEditable ^  Qt::ItemIsSelectable);

            ui->label_image->m_drawObjectBoxColor.push_back(labelColor);
        }
        ui->label_image->m_objList = m_objList;
    }
}

QString MainWindow::get_labeling_data(QString qstrImgFile)const
{
    string strImgFile = qstrImgFile.toStdString();
    string strLabelData = strImgFile.substr(0, strImgFile.find_last_of('.')) + ".txt";

    return QString().fromStdString(strLabelData);
}

void MainWindow::set_label(const int labelIndex)
{
    bool bIndexIsOutOfRange = (labelIndex < 0 || labelIndex > m_objList.size() - 1);

    if(!bIndexIsOutOfRange)
    {
        m_objIndex = labelIndex;
        ui->label_image->setFocusObjectLabel(m_objIndex);
        ui->tableWidget_label->setCurrentCell(m_objIndex, 0);
    }
}

void MainWindow::set_label_color(const int index, const QColor color)
{
    ui->label_image->m_drawObjectBoxColor.replace(index, color);
}

void MainWindow::pjreddie_style_msgBox(QMessageBox::Icon icon, QString title, QString content)
{
    QMessageBox msgBox(icon, title, content, QMessageBox::Ok);

    QFont font;
    font.setBold(true);
    msgBox.setFont(font);
    msgBox.button(QMessageBox::Ok)->setFont(font);
    msgBox.button(QMessageBox::Ok)->setStyleSheet("border-style: outset; border-width: 2px; border-color: rgb(0, 255, 0); color : rgb(0, 255, 0);");
    msgBox.button(QMessageBox::Ok)->setFocusPolicy(Qt::ClickFocus);
    msgBox.setStyleSheet("background-color : rgb(34, 0, 85); color : rgb(0, 255, 0);");

    msgBox.exec();
}

bool MainWindow::get_files(QString imgDir)
{
    bool value = false;
    QDir dir(imgDir);
    QCollator collator;
    collator.setNumericMode(true);

    QStringList fileList = dir.entryList(
                QStringList() << "*.jpg" << "*.JPG" << "*.png" << "*.bmp",
                QDir::Files);

    std::sort(fileList.begin(), fileList.end(), collator);

    if(!fileList.empty())
    {
        value = true;
        m_imgDir    = imgDir;
        m_imgList  = fileList;

        for(QString& str: m_imgList)
            str = m_imgDir + "/" + str;
    }
    return value;
}

void MainWindow::open_img_dir(bool& ret)
{
    pjreddie_style_msgBox(QMessageBox::Information,"Help", "Step 1. Open Your Data Set Directory");

    QString opened_dir;
    if(m_imgDir.size() > 0) opened_dir = m_imgDir;
    else opened_dir = QCoreApplication::applicationDirPath();

    QString imgDir = QFileDialog::getExistingDirectory(
                nullptr,
                tr("Open Dataset Directory"),
                opened_dir,
                QFileDialog::ShowDirsOnly);


    if(imgDir.isEmpty())
    {
        ret = false;
        return;
    }

    ret = get_files(imgDir);
    if (!ret)
        pjreddie_style_msgBox(QMessageBox::Critical,"Error", "This folder is empty");
}

void MainWindow::open_obj_file(bool& ret)
{
    pjreddie_style_msgBox(QMessageBox::Information,"Help", "Step 2. Open Your Label List File(*.txt or *.names)");

    QString opened_dir;
    if(m_imgDir.size() > 0) opened_dir = m_imgDir;
    else opened_dir = QCoreApplication::applicationDirPath();

    QString fileLabelList = QFileDialog::getOpenFileName(
                nullptr,
                tr("Open LabelList file"),
                opened_dir,
                tr("LabelList Files (*.txt *.names)"));

    if(fileLabelList.size() == 0)
    {
        ret = false;
        pjreddie_style_msgBox(QMessageBox::Critical,"Error", "LabelList file is not opened()");
    }
    else
    {
        ret = true;
        m_objFilePath = fileLabelList;
        load_label_list_data(fileLabelList);
    }
}

void MainWindow::wheelEvent(QWheelEvent *ev)
{
    QPoint labelPos = ui->label_image->mapFromGlobal(ev->globalPosition().toPoint());
    bool overImage = ui->label_image->rect().contains(labelPos);

    if(ev->modifiers() & Qt::ControlModifier && overImage)
    {
        if(ev->angleDelta().y() > 0)
            ui->label_image->zoomIn(labelPos);
        else if(ev->angleDelta().y() < 0)
            ui->label_image->zoomOut(labelPos);
    }
    else if(overImage)
    {
        if(ev->angleDelta().y() > 0)
            prev_img();
        else if(ev->angleDelta().y() < 0)
            next_img();
    }
}

void MainWindow::on_pushButton_prev_clicked()
{
    prev_img();
}

void MainWindow::on_pushButton_next_clicked()
{
    next_img();
}

void MainWindow::keyPressEvent(QKeyEvent * event)
{
    int     nKey = event->key();

    bool    graveAccentKeyIsPressed    = (nKey == Qt::Key_QuoteLeft);
    bool    numKeyIsPressed            = (nKey >= Qt::Key_0 && nKey <= Qt::Key_9 );
    bool    arrowKeyIsPressed          = (nKey == Qt::Key_Up || nKey == Qt::Key_Down ||
                                          nKey == Qt::Key_Left || nKey == Qt::Key_Right);

    if(graveAccentKeyIsPressed)
    {
        set_label(0);
    }
    else if(numKeyIsPressed)
    {
        int asciiToNumber = nKey - Qt::Key_0;

        if(asciiToNumber < m_objList.size() )
        {
            set_label(asciiToNumber);
        }
    }
    else if(arrowKeyIsPressed && ui->label_image->isOpened())
    {
        static int nudgeBoxIdx = -1;

        if(!event->isAutoRepeat())
        {
            QPointF cursorPos = ui->label_image->cvtAbsoluteToRelativePoint(
                ui->label_image->mapFromGlobal(QCursor::pos()));
            nudgeBoxIdx = ui->label_image->findBoxUnderCursor(cursorPos);
            if(nudgeBoxIdx != -1)
                ui->label_image->saveState();
        }

        if(nudgeBoxIdx != -1)
        {
            bool ctrlHeld  = (event->modifiers() & Qt::ControlModifier);
            bool shiftHeld = (event->modifiers() & Qt::ShiftModifier);
            double step = shiftHeld ? 0.01 : 0.002;

            if(ctrlHeld)
            {
                double dw = 0.0, dh = 0.0;
                if(nKey == Qt::Key_Left)  dw = -step;
                if(nKey == Qt::Key_Right) dw =  step;
                if(nKey == Qt::Key_Up)    dh = -step;
                if(nKey == Qt::Key_Down)  dh =  step;

                ui->label_image->resizeBox(nudgeBoxIdx, dw, dh);
            }
            else
            {
                double dx = 0.0, dy = 0.0;
                if(nKey == Qt::Key_Left)  dx = -step;
                if(nKey == Qt::Key_Right) dx =  step;
                if(nKey == Qt::Key_Up)    dy = -step;
                if(nKey == Qt::Key_Down)  dy =  step;

                ui->label_image->moveBox(nudgeBoxIdx, dx, dy);
            }
        }
    }
}

void MainWindow::on_tableWidget_label_cellDoubleClicked(int row, int column)
{
    bool bColorAttributeClicked = (column == 1);

    if(bColorAttributeClicked)
    {
        QColor color = QColorDialog::getColor(Qt::white,nullptr,"Set Label Color");
        if(color.isValid())
        {
            set_label_color(row, color);
            ui->tableWidget_label->item(row, 1)->setBackground(color);
            saveSession();
        }
        set_label(row);
        ui->label_image->showImage();
    }
}

void MainWindow::on_tableWidget_label_cellClicked(int row, int column)
{
    set_label(row);
}

void MainWindow::on_horizontalSlider_images_sliderMoved(int position)
{
    goto_img(position);
}

void MainWindow::init_horizontal_slider()
{
    ui->horizontalSlider_images->setEnabled(true);
    ui->horizontalSlider_images->setRange(0, m_imgList.size() - 1);
    ui->horizontalSlider_images->blockSignals(true);
    ui->horizontalSlider_images->setValue(0);
    ui->horizontalSlider_images->blockSignals(false);

    ui->horizontalSlider_contrast->setEnabled(true);
    ui->horizontalSlider_contrast->setRange(0, 1000);
    ui->horizontalSlider_contrast->setValue(ui->horizontalSlider_contrast->maximum()/2);
    ui->label_image->setContrastGamma(1.0);
    ui->label_contrast->setText(QString("Contrast(%) ") + QString::number(50));
}

void MainWindow::init_table_widget()
{
    ui->tableWidget_label->horizontalHeader()->setVisible(true);
    ui->tableWidget_label->horizontalHeader()->setStyleSheet("");
    ui->tableWidget_label->horizontalHeader()->setStretchLastSection(true);

    disconnect(ui->tableWidget_label->horizontalHeader(), &QHeaderView::sectionPressed, ui->tableWidget_label, &QTableWidget::selectColumn);
}

void MainWindow::on_horizontalSlider_contrast_sliderMoved(int value)
{
    float valueToPercentage = float(value)/ui->horizontalSlider_contrast->maximum(); //[0, 1.0]
    float percentageToGamma = std::pow(1/(valueToPercentage + 0.5), 7.);

    ui->label_image->setContrastGamma(percentageToGamma);
    ui->label_contrast->setText(QString("Contrast(%) ") + QString::number(int(valueToPercentage * 100.)));
}

void MainWindow::on_checkBox_visualize_class_name_clicked(bool checked)
{
    ui->label_image->m_bVisualizeClassName = checked;
    ui->label_image->showImage();
}

void MainWindow::copy_annotations()
{
    if(!ui->label_image->isOpened()) return;
    m_copiedAnnotations = ui->label_image->m_objBoundingBoxes;
}

void MainWindow::paste_annotations()
{
    if(m_copiedAnnotations.isEmpty() || !ui->label_image->isOpened()) return;
    ui->label_image->saveState();
    ui->label_image->m_objBoundingBoxes = m_copiedAnnotations;
    ui->label_image->showImage();
}

void MainWindow::undo()
{
    if(ui->label_image->undo())
        ui->label_image->showImage();
}

void MainWindow::redo()
{
    if(ui->label_image->redo())
        ui->label_image->showImage();
}

void MainWindow::on_usageTimer_timeout()
{
    if (isActiveWindow())
    {
        m_usageTimerElapsedSeconds++;
        updateUsageTimerLabel();
    }
}

void MainWindow::on_usageTimerReset_clicked()
{
    m_usageTimerElapsedSeconds = 0;
    updateUsageTimerLabel();
}

void MainWindow::updateUsageTimerLabel()
{
    qint64 secs = m_usageTimerElapsedSeconds;
    int hours = static_cast<int>(secs / 3600);
    int mins  = static_cast<int>((secs % 3600) / 60);
    int s     = static_cast<int>(secs % 60);
    QString text = QStringLiteral("\u23F1 %1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(mins, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
    m_usageTimerLabel->setText(text);
}

void MainWindow::reset_zoom()
{
    ui->label_image->resetZoom();
}

#ifdef ONNXRUNTIME_AVAILABLE
void MainWindow::on_loadModel_clicked()
{
    QString dir = m_imgDir.isEmpty() ? QDir::currentPath() : m_imgDir;
    QString modelPath = QFileDialog::getOpenFileName(
        this, tr("Open YOLO ONNX Model"), dir,
        tr("ONNX Models (*.onnx)"));

    if (modelPath.isEmpty()) return;

    loadOnnxModel(modelPath);
}

void MainWindow::loadOnnxModel(const QString& modelPath)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    std::string errorMsg;
    bool ok = m_detector.loadModel(modelPath.toStdString(), errorMsg);
    QApplication::restoreOverrideCursor();

    if (ok) {
        QString versionStr;
        switch (m_detector.getVersion()) {
            case YoloVersion::V5:  versionStr = "YOLOv5"; break;
            case YoloVersion::V8:  versionStr = "YOLOv8"; break;
            case YoloVersion::V11: versionStr = "YOLO11"; break;
            case YoloVersion::V12: versionStr = "YOLO12"; break;
            case YoloVersion::V26: versionStr = "YOLOv26"; break;
            default:               versionStr = "YOLO"; break;
        }
        if (m_detector.isEndToEnd()) versionStr += " (end2end)";

        m_labelModelStatus->setText(
            QString("%1 | %2 classes | %3x%4 | %5")
                .arg(QFileInfo(modelPath).fileName())
                .arg(m_detector.getNumClasses())
                .arg(m_detector.getInputWidth())
                .arg(m_detector.getInputHeight())
                .arg(versionStr));

        // Class list handling
        const auto& modelClasses = m_detector.getClassNames();
        if (!modelClasses.empty() && m_objList.isEmpty()) {
            // No user classes loaded — auto-populate from model
            loadClassesFromModel();
        } else if (!modelClasses.empty() && !m_objList.isEmpty()) {
            // Both exist — check if they match
            bool mismatch = false;
            if (static_cast<int>(modelClasses.size()) != m_objList.size()) {
                mismatch = true;
            } else {
                int idx = 0;
                for (const auto& pair : modelClasses) {
                    if (QString::fromStdString(pair.second) != m_objList.at(idx)) {
                        mismatch = true;
                        break;
                    }
                    idx++;
                }
            }
            if (mismatch) {
                pjreddie_style_msgBox(QMessageBox::Warning, "Class Mismatch",
                    QString("The loaded class list (%1 classes) does not match "
                            "the model's class list (%2 classes).\n\n"
                            "Auto-labeling is disabled to prevent incorrect labels.\n"
                            "Either load a matching class file or re-load the model "
                            "without a class file to use the model's built-in classes.")
                        .arg(m_objList.size())
                        .arg(modelClasses.size()));
                m_btnAutoLabel->setEnabled(false);
                m_btnAutoLabelAll->setEnabled(false);
                m_labelModelStatus->setText(m_labelModelStatus->text() + " | CLASS MISMATCH");
                return;
            }
        }

        bool hasImages = !m_imgList.isEmpty();
        m_btnAutoLabel->setEnabled(hasImages);
        m_btnAutoLabelAll->setEnabled(hasImages);
    } else {
        pjreddie_style_msgBox(QMessageBox::Critical, "Error",
            QString("Failed to load model:\n%1").arg(QString::fromStdString(errorMsg)));
        m_labelModelStatus->setText("Load failed");
    }
}

void MainWindow::loadClassesFromModel()
{
    const auto& classNames = m_detector.getClassNames();
    if (classNames.empty()) return;

    // Clear existing table
    while (ui->tableWidget_label->rowCount() > 0)
        ui->tableWidget_label->removeRow(0);

    m_objList.clear();
    ui->label_image->m_drawObjectBoxColor.clear();

    // Populate from model metadata (same pattern as load_label_list_data)
    int fileIndex = 0;
    for (const auto& pair : classNames) {
        int nRow = ui->tableWidget_label->rowCount();
        QString qstrLabel = QString::fromStdString(pair.second);
        QColor labelColor = label_img::BOX_COLORS[(fileIndex++) % 10];
        m_objList << qstrLabel;

        ui->tableWidget_label->insertRow(nRow);
        ui->tableWidget_label->setItem(nRow, 0, new QTableWidgetItem(qstrLabel));
        ui->tableWidget_label->item(nRow, 0)->setFlags(
            ui->tableWidget_label->item(nRow, 0)->flags() ^ Qt::ItemIsEditable);

        ui->tableWidget_label->setItem(nRow, 1, new QTableWidgetItem(QString()));
        ui->tableWidget_label->item(nRow, 1)->setBackground(labelColor);
        ui->tableWidget_label->item(nRow, 1)->setFlags(
            ui->tableWidget_label->item(nRow, 1)->flags() ^ Qt::ItemIsEditable ^ Qt::ItemIsSelectable);

        ui->label_image->m_drawObjectBoxColor.push_back(labelColor);
    }
    ui->label_image->m_objList = m_objList;

    if (!m_objList.isEmpty()) {
        set_label(0);
    }
}

void MainWindow::on_autoLabel_clicked()
{
    if (!m_detector.isLoaded() || !ui->label_image->isOpened()) return;

    QImage img = ui->label_image->getInputImage();
    if (img.isNull()) return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto detections = m_detector.detect(img, getConfidenceThreshold());
    QApplication::restoreOverrideCursor();

    if (detections.empty()) {
        pjreddie_style_msgBox(QMessageBox::Information, "Auto Label",
            "No objects detected at current confidence threshold.");
        return;
    }

    applyDetections(detections);
}

void MainWindow::on_autoLabelAll_clicked()
{
    if (!m_detector.isLoaded() || m_imgList.isEmpty()) return;

    QMessageBox msgBox(QMessageBox::Question, "Auto Label All",
        QString("Auto-label all %1 images?\nExisting labels will be overwritten.")
            .arg(m_imgList.size()),
        QMessageBox::Yes | QMessageBox::No);
    msgBox.setStyleSheet("background-color: rgb(34, 0, 85); color: rgb(0, 255, 0);");
    if (msgBox.exec() != QMessageBox::Yes) return;

    save_label_data();

    QProgressDialog progress("Auto-labeling images...", "Cancel", 0, m_imgList.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setStyleSheet("QProgressDialog { background-color: rgb(34, 0, 85); color: rgb(0, 255, 0); }");

    int labeled = 0;
    int maxClassIdx = m_objList.size() - 1;

    for (int i = 0; i < m_imgList.size(); ++i) {
        progress.setValue(i);
        if (progress.wasCanceled()) break;

        QImage img(m_imgList.at(i));
        if (img.isNull()) continue;

        auto detections = m_detector.detect(img, getConfidenceThreshold());

        QString labelPath = get_labeling_data(m_imgList.at(i));
        ofstream out(qPrintable(labelPath));
        if (!out.is_open()) continue;

        for (const auto& det : detections) {
            if (det.classId < 0 || det.classId > maxClassIdx) continue;
            double cx = det.x + det.width / 2.0;
            double cy = det.y + det.height / 2.0;
            out << det.classId << " "
                << std::fixed << std::setprecision(6) << cx << " "
                << std::fixed << std::setprecision(6) << cy << " "
                << std::fixed << std::setprecision(6) << static_cast<double>(det.width) << " "
                << std::fixed << std::setprecision(6) << static_cast<double>(det.height) << "\n";
        }
        out.close();
        labeled++;
    }
    progress.setValue(m_imgList.size());

    goto_img(m_imgIndex);

    pjreddie_style_msgBox(QMessageBox::Information, "Auto Label All",
        QString("Auto-labeled %1 of %2 images.").arg(labeled).arg(m_imgList.size()));
}

void MainWindow::on_confidenceSlider_changed(int value)
{
    m_labelConfidence->setText(QString("Conf: %1%").arg(value));
}

void MainWindow::applyDetections(const std::vector<DetectionResult>& detections)
{
    ui->label_image->saveState();
    ui->label_image->m_objBoundingBoxes.clear();

    int maxClassIdx = m_objList.size() - 1;

    for (const auto& det : detections) {
        if (det.classId < 0 || det.classId > maxClassIdx) continue;

        ObjectLabelingBox box;
        box.label = det.classId;
        box.box = QRectF(det.x, det.y, det.width, det.height);
        ui->label_image->m_objBoundingBoxes.push_back(box);
    }

    ui->label_image->showImage();
}

float MainWindow::getConfidenceThreshold() const
{
    return static_cast<float>(m_sliderConfidence->value()) / 100.0f;
}
#endif

// ── Side tab widget ─────────────────────────────────────────────────────────

void MainWindow::initSideTabWidget()
{
    const QString tabStyle =
        "QTabWidget::pane { border: 2px solid rgb(0,255,255); background: rgb(0,0,17); }"
        "QTabBar::tab { background: rgb(0,0,17); color: rgb(0,255,255); "
        "border: 1px solid rgb(0,255,255); padding: 5px 12px; font-weight: bold; font-size: 12px; }"
        "QTabBar::tab:selected { background: rgb(0,40,60); }"
        "QTabBar::tab:hover    { background: rgb(0,20,40); }";

    const QString fieldStyle =
        "QLineEdit { background: rgb(0,0,17); color: rgb(0,255,255); "
        "border: 1px solid rgb(0,255,255); border-radius: 3px; padding: 3px 6px; font-size: 12px; }"
        "QLineEdit:focus { border: 2px solid rgb(0,255,255); }";

    const QString labelStyle =
        "QLabel { color: rgb(0,255,255); font-weight: bold; font-size: 12px; }";

    const QString btnStyle =
        "QPushButton { background: rgb(0,0,17); color: rgb(0,255,255); "
        "border: 2px solid rgb(0,255,255); border-radius: 4px; padding: 5px 16px; "
        "font-weight: bold; font-size: 12px; }"
        "QPushButton:hover   { background: rgb(0,40,60); }"
        "QPushButton:pressed { background: rgb(0,80,100); }";

    // ── Tab 1: Labels ────────────────────────────────────────────────
    QWidget *labelsTab = new QWidget();
    QVBoxLayout *labelsLayout = new QVBoxLayout(labelsTab);
    labelsLayout->setContentsMargins(0, 0, 0, 0);
    labelsLayout->setSpacing(0);
    ui->horizontalLayout_5->removeWidget(ui->tableWidget_label);
    labelsLayout->addWidget(ui->tableWidget_label);

    // ── Tab 2: AI Settings ───────────────────────────────────────────
    QWidget *aiTab = new QWidget();
    aiTab->setStyleSheet("background: rgb(0,0,17);");
    QVBoxLayout *aiLayout = new QVBoxLayout(aiTab);
    aiLayout->setContentsMargins(10, 14, 10, 10);
    aiLayout->setSpacing(10);

    auto *modelLabel = new QLabel("Model:", aiTab);
    modelLabel->setStyleSheet(labelStyle);

    m_modelCombo = new QComboBox(aiTab);
    m_modelCombo->addItem("YoloLabel AI");
    m_modelCombo->addItem("Landing AI");
    m_modelCombo->setStyleSheet(
        "QComboBox { background: rgb(0,0,17); color: rgb(0,255,255);"
        "  border: 2px solid rgb(0,255,255); border-radius: 4px;"
        "  padding: 4px 32px 4px 10px; font-weight: bold; font-size: 12px; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: right center;"
        "  width: 24px; border-left: 1px solid rgb(0,255,255); }"
        "QComboBox::down-arrow { width: 0; height: 0;"
        "  border-left: 5px solid transparent; border-right: 5px solid transparent;"
        "  border-top: 7px solid rgb(0,255,255); }"
        "QComboBox QAbstractItemView { background: rgb(0,0,17); color: rgb(0,255,255);"
        "  selection-background-color: rgb(0,40,60);"
        "  border: 1px solid rgb(0,255,255); outline: none; }");
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](){ syncAiSettingsTab(); });

    aiLayout->addWidget(modelLabel);
    aiLayout->addWidget(m_modelCombo);

    auto *keyLabel = new QLabel("API Key:", aiTab);
    keyLabel->setStyleSheet(labelStyle);

    m_settingsKeyEdit = new QLineEdit(aiTab);
    m_settingsKeyEdit->setEchoMode(QLineEdit::Password);
    m_settingsKeyEdit->setStyleSheet(fieldStyle);

    auto *toggleKeyBtn = new QPushButton("Show", aiTab);
    toggleKeyBtn->setCheckable(true);
    toggleKeyBtn->setFixedWidth(54);
    toggleKeyBtn->setStyleSheet(btnStyle);
    connect(toggleKeyBtn, &QPushButton::toggled, this, [this, toggleKeyBtn](bool checked){
        m_settingsKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        toggleKeyBtn->setText(checked ? "Hide" : "Show");
    });
    auto *keyRow = new QHBoxLayout();
    keyRow->addWidget(m_settingsKeyEdit);
    keyRow->addWidget(toggleKeyBtn);

    auto *promptLabel = new QLabel("Prompt:", aiTab);
    promptLabel->setStyleSheet(labelStyle);
    m_settingsPromptEdit = new QLineEdit(aiTab);
    m_settingsPromptEdit->setPlaceholderText("auto (from class file)");
    m_settingsPromptEdit->setStyleSheet(fieldStyle);
    m_settingsPromptEdit->setToolTip(
        "Labels separated by  ;  \u2014 leave blank to use the loaded class file automatically");

    auto *hintLabel = new QLabel("Tip: leave blank to use class file labels", aiTab);
    hintLabel->setStyleSheet("color: rgb(120,180,180); font-size: 11px;");
    hintLabel->setWordWrap(true);

    const QString clearBtnStyle =
        "QPushButton { background: rgb(0,0,17); color: rgb(255,80,80); "
        "border: 2px solid rgb(255,80,80); border-radius: 4px; padding: 5px 12px; "
        "font-weight: bold; font-size: 12px; }"
        "QPushButton:hover { background: rgb(40,0,0); }";

    auto *saveBtn  = new QPushButton("Save", aiTab);
    auto *clearBtn = new QPushButton("Clear Key", aiTab);
    saveBtn->setStyleSheet(btnStyle);
    clearBtn->setStyleSheet(clearBtnStyle);

    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(saveBtn);
    btnRow->addWidget(clearBtn);
    btnRow->addStretch();

    connect(saveBtn,  &QPushButton::clicked, this, &MainWindow::saveAiSettings);
    connect(clearBtn, &QPushButton::clicked, this, [this](){ m_settingsKeyEdit->clear(); saveAiSettings(); });

    aiLayout->addWidget(keyLabel);
    aiLayout->addLayout(keyRow);
    aiLayout->addWidget(promptLabel);
    aiLayout->addWidget(m_settingsPromptEdit);
    aiLayout->addWidget(hintLabel);
    aiLayout->addLayout(btnRow);
    aiLayout->addStretch();

    // ── Assemble tab widget ──────────────────────────────────────────
    m_sideTabWidget = new QTabWidget(this);
    m_sideTabWidget->setStyleSheet(tabStyle);
    m_sideTabWidget->setMinimumWidth(220);
    m_sideTabWidget->setMaximumWidth(330);
    m_sideTabWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    m_sideTabWidget->addTab(labelsTab, "Labels");
    m_sideTabWidget->addTab(aiTab, "\u2699 AI Settings");

    connect(m_sideTabWidget, &QTabWidget::currentChanged, this, [this](int idx){
        if (idx == 1) syncAiSettingsTab();
    });

    ui->gridLayout->addWidget(m_sideTabWidget, 0, 1, ui->gridLayout->rowCount(), 1);
}

void MainWindow::syncAiSettingsTab()
{
    bool isLanding = (m_modelCombo->currentIndex() == 1);
    m_settingsKeyEdit->setText(isLanding ? m_landingApiKey : m_cloudApiKey);
    m_settingsKeyEdit->setPlaceholderText(isLanding ? "" : "ylk_\u2026");
    // Show only the explicitly saved prompt (empty = auto from class file,
    // shown via placeholder text). Never auto-fill from the class list here
    // to avoid accidentally hardcoding classes as the prompt on Save.
    m_settingsPromptEdit->setText(m_cloudPrompt);
}

void MainWindow::saveAiSettings()
{
    bool isLanding = (m_modelCombo->currentIndex() == 1);
    QString key    = m_settingsKeyEdit->text().trimmed();
    m_cloudPrompt  = m_settingsPromptEdit->text().trimmed();

    if (isLanding) m_landingApiKey = key;
    else           m_cloudApiKey   = key;

    QSettings s("YoloLabel", "CloudAI");
    s.setValue("apiKey",       m_cloudApiKey);
    s.setValue("prompt",       m_cloudPrompt);
    s.setValue("landingApiKey", m_landingApiKey);

    m_cloudLabeler->setApiKey(m_cloudApiKey);
    m_cloudLabeler->setPrompt(m_cloudPrompt);

    statusBar()->showMessage("AI settings saved.", 2000);
}

void MainWindow::resetCloudButtons()
{
    m_btnCloudAutoLabel->setEnabled(true);
    m_btnCloudAutoLabel->setText("\u2601 Auto Label AI");
    m_btnCloudAutoLabelAll->setEnabled(true);
    m_btnCloudAutoLabelAll->setText("\u2601 Auto Label All AI");
    m_btnCancelAutoLabel->setVisible(false);
    ui->horizontalSlider_images->setEnabled(true);
}

void MainWindow::cancelAutoLabel()
{
    m_cloudLabeler->cancel();
    m_landingCancelled = true;
    m_landingQueue.clear();
}

// ── Cloud auto-label ────────────────────────────────────────────────────────

bool MainWindow::checkUploadConsent()
{
    QSettings s("YoloLabel", "CloudAI");
    if (s.value("uploadConsentGiven", false).toBool()) return true;

    QMessageBox dlg(QMessageBox::Information, "Cloud Auto-Label — Privacy Notice",
        "Images will be uploaded to <b>api.yololabel.com</b> for inference.<br><br>"
        "By continuing you confirm that you have the right to share these images "
        "with a third-party service and accept the associated privacy implications.",
        QMessageBox::Ok | QMessageBox::Cancel, this);
    dlg.button(QMessageBox::Ok)->setText("I Understand — Continue");

    if (dlg.exec() != QMessageBox::Ok) return false;

    s.setValue("uploadConsentGiven", true);
    return true;
}

void MainWindow::submitCloudJob()
{
    if (m_imgList.isEmpty()) return;

    if (m_cloudApiKey.isEmpty()) {
        m_sideTabWidget->setCurrentIndex(1);
        syncAiSettingsTab();
        statusBar()->showMessage("Enter your API key in the AI Settings tab.", 4000);
        return;
    }

    if (!checkUploadConsent()) return;

    save_label_data();  // preserve current manual annotations before overwriting
    ui->label_image->saveState(); // enable Ctrl+Z undo after cloud labels are applied

    m_btnCloudAutoLabel->setEnabled(false);
    m_btnCloudAutoLabelAll->setEnabled(false);
    m_btnCancelAutoLabel->setVisible(true);
    m_btnCloudAutoLabel->setText("\u2601 Labelling\u2026");
    m_cloudLabeler->setApiKey(m_cloudApiKey);
    m_cloudLabeler->setPrompt(m_cloudPrompt);
    m_cloudLabeler->setClasses(m_objList);
    m_cloudLabeler->labelImage(m_imgList[m_imgIndex]);
}

void MainWindow::cloudAutoLabelAll()
{
    if (m_imgList.isEmpty()) return;

    if (m_cloudApiKey.isEmpty()) {
        m_sideTabWidget->setCurrentIndex(1);
        syncAiSettingsTab();
        statusBar()->showMessage("Enter your API key in the AI Settings tab.", 4000);
        return;
    }

    if (!checkUploadConsent()) return;

    // Estimate total upload size and warn if large
    qint64 totalBytes = 0;
    for (const QString &path : m_imgList)
        totalBytes += QFileInfo(path).size();
    QString confirmMsg = QString("Send all %1 images to cloud AI?\nExisting labels will be overwritten.")
        .arg(m_imgList.size());
    constexpr qint64 warnThresholdMB = 500;
    if (totalBytes > warnThresholdMB * 1024 * 1024) {
        confirmMsg += QString("\n\nWarning: estimated upload size is ~%1 MB. "
                              "This may take a while and use significant bandwidth.")
            .arg(totalBytes / (1024 * 1024));
    }

    QMessageBox msgBox(QMessageBox::Question, "Auto Label All",
        confirmMsg, QMessageBox::Yes | QMessageBox::No, this);
    if (msgBox.exec() != QMessageBox::Yes) return;

    save_label_data();

    m_btnCloudAutoLabel->setEnabled(false);
    m_btnCloudAutoLabelAll->setEnabled(false);
    m_btnCancelAutoLabel->setVisible(true);
    m_btnCloudAutoLabelAll->setText(
        QString("\u2601 Auto Label All (0/%1)\u2026").arg(m_imgList.size()));

    m_cloudLabeler->setApiKey(m_cloudApiKey);
    m_cloudLabeler->setPrompt(m_cloudPrompt);
    m_cloudLabeler->setClasses(m_objList);
    m_cloudLabeler->labelImages(m_imgList);
}

// ── Landing AI ───────────────────────────────────────────────────────────────
// API docs: https://landing-ai.com/

bool MainWindow::checkLandingConsent()
{
    QSettings s("YoloLabel", "CloudAI");
    if (s.value("landingConsentGiven", false).toBool()) return true;

    QMessageBox dlg(QMessageBox::Information, "Landing AI — Privacy Notice",
        "Images will be uploaded to <b>api.va.landing.ai</b> for inference.<br><br>"
        "By continuing you confirm that you have the right to share these images "
        "with a third-party service and accept the associated privacy implications.",
        QMessageBox::Ok | QMessageBox::Cancel, this);
    dlg.button(QMessageBox::Ok)->setText("I Understand — Continue");

    if (dlg.exec() != QMessageBox::Ok) return false;

    s.setValue("landingConsentGiven", true);
    return true;
}

static const char *LANDING_AI_ENDPOINT =
    "https://api.va.landing.ai/v1/tools/text-to-object-detection";

void MainWindow::submitLandingAIJob()
{
    if (m_imgList.isEmpty()) return;

    if (m_landingApiKey.isEmpty()) {
        m_sideTabWidget->setCurrentIndex(1);
        syncAiSettingsTab();
        QMessageBox::information(this, "Landing AI",
            "Please enter your Landing AI API key in the AI Settings tab.");
        return;
    }

    if (!checkLandingConsent()) return;

    save_label_data();
    ui->label_image->saveState();

    m_landingCancelled = false;
    m_btnCloudAutoLabel->setEnabled(false);
    m_btnCloudAutoLabelAll->setEnabled(false);
    m_btnCancelAutoLabel->setVisible(true);
    m_btnCloudAutoLabel->setText("\u2601 Labelling\u2026");
    ui->horizontalSlider_images->setEnabled(false);
    doLandingAIJob(m_imgList[m_imgIndex]);
}

void MainWindow::doLandingAIJob(const QString &imagePath, int retryCount)
{
    if (m_landingCancelled) {
        resetCloudButtons();
        return;
    }

    static constexpr int MAX_LANDING_RETRIES = 3;

    QFile f(imagePath);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Landing AI", "Cannot read image: " + imagePath);
        m_landingQueue.clear();
        resetCloudButtons();
        return;
    }
    QByteArray imageData = f.readAll();
    f.close();

    QString effectivePrompt = m_cloudPrompt.isEmpty()
        ? m_objList.join(" ; ")
        : m_cloudPrompt;
    QStringList labels;
    for (const QString &p : effectivePrompt.split(";"))
        labels << p.trimmed();

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart imgPart;
    imgPart.setHeader(QNetworkRequest::ContentTypeHeader,
                      CloudAutoLabeler::mimeForImage(imagePath));
    imgPart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QString("form-data; name=\"image\"; filename=\"%1\"")
            .arg(QFileInfo(imagePath).fileName()));
    imgPart.setBody(imageData);
    multiPart->append(imgPart);

    for (const QString &l : labels) {
        if (l.isEmpty()) continue;
        QHttpPart promptsPart;
        promptsPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                              "form-data; name=\"prompts\"");
        promptsPart.setBody(l.toUtf8());
        multiPart->append(promptsPart);
    }

    QHttpPart modelPart;
    modelPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        "form-data; name=\"model\"");
    modelPart.setBody("agentic");
    multiPart->append(modelPart);

    QNetworkRequest req(QUrl(QString::fromLatin1(LANDING_AI_ENDPOINT)));
    req.setRawHeader("Authorization", "Bearer " + m_landingApiKey.toUtf8());
    req.setTransferTimeout(30000);

    QNetworkReply *reply = m_landingNet->post(req, multiPart);
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, imagePath, retryCount]() {
        reply->deleteLater();

        if (m_landingCancelled) {
            resetCloudButtons();
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            if (retryCount < MAX_LANDING_RETRIES) {
                statusBar()->showMessage(
                    QString("Landing AI: network error, retrying (%1/%2)…")
                        .arg(retryCount + 1).arg(MAX_LANDING_RETRIES), 2000);
                doLandingAIJob(imagePath, retryCount + 1);
            } else {
                QMessageBox::warning(this, "Landing AI",
                                     "Request failed: " + reply->errorString());
                m_landingQueue.clear();
                resetCloudButtons();
            }
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isNull() || !doc.isObject()) {
            QMessageBox::warning(this, "Landing AI", "Unexpected response from server.");
            m_landingQueue.clear();
            resetCloudButtons();
            return;
        }

        // Response: {"data": [[{label, score, bounding_box:[x1,y1,x2,y2]}, ...], ...]}
        QJsonValue dataVal = doc.object().value("data");
        if (!dataVal.isArray() || dataVal.toArray().isEmpty()) {
            // No detections or unexpected format
            if (m_landingQueue.isEmpty()) {
                goto_img(m_imgIndex);
                statusBar()->showMessage("Landing AI: no detections.", 4000);
                resetCloudButtons();
            } else {
                landingAIProcessNextInQueue();
            }
            return;
        }

        QJsonValue firstVal = dataVal.toArray().first();
        if (!firstVal.isArray()) {
            QMessageBox::warning(this, "Landing AI", "Unexpected response format from server.");
            m_landingQueue.clear();
            resetCloudButtons();
            return;
        }
        QJsonArray detections = firstVal.toArray();

        // Use QImageReader to get image dimensions without decoding pixels
        QImageReader reader(imagePath);
        QSize imgSize = reader.size();
        if (!imgSize.isValid() || imgSize.isEmpty()) {
            // Fallback: load fully only if size query fails
            imgSize = QImage(imagePath).size();
        }
        double imgW = imgSize.width();
        double imgH = imgSize.height();

        QString labelPath = QFileInfo(imagePath).dir().filePath(
            QFileInfo(imagePath).baseName() + ".txt");
        CloudAutoLabeler::backupLabelFile(labelPath);
        QFile lf(labelPath);
        int written = 0;
        QStringList skippedLabels;
        if (lf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&lf);
            for (const QJsonValue &v : detections) {
                if (!v.isObject()) continue;
                QJsonObject obj = v.toObject();
                QString label   = obj.value("label").toString();
                int classId     = m_objList.indexOf(label);
                if (classId < 0) {
                    if (!label.isEmpty() && !skippedLabels.contains(label))
                        skippedLabels << label;
                    continue;
                }

                QJsonArray bb = obj.value("bounding_box").toArray();
                if (bb.size() < 4) continue;
                if (!bb[0].isDouble() || !bb[1].isDouble() ||
                    !bb[2].isDouble() || !bb[3].isDouble()) continue;
                double x1 = bb[0].toDouble();
                double y1 = bb[1].toDouble();
                double x2 = bb[2].toDouble();
                double y2 = bb[3].toDouble();

                double cx = ((x1 + x2) / 2.0) / imgW;
                double cy = ((y1 + y2) / 2.0) / imgH;
                double nw = (x2 - x1) / imgW;
                double nh = (y2 - y1) / imgH;

                // Skip degenerate or out-of-range boxes
                if (cx < 0.0 || cx > 1.0 || cy < 0.0 || cy > 1.0 ||
                    nw <= 0.0 || nw > 1.0 || nh <= 0.0 || nh > 1.0) continue;

                out << classId << " "
                    << QString::number(cx, 'f', 6) << " "
                    << QString::number(cy, 'f', 6) << " "
                    << QString::number(nw, 'f', 6) << " "
                    << QString::number(nh, 'f', 6) << "\n";
                ++written;
            }
            lf.close();
        } else {
            statusBar()->showMessage(
                "Landing AI: could not write label file: " + labelPath, 5000);
        }

        if (!skippedLabels.isEmpty()) {
            statusBar()->showMessage(
                QString("Landing AI: detections skipped (unknown labels: %1)")
                    .arg(skippedLabels.join(", ")), 5000);
        }

        if (m_landingQueue.isEmpty()) {
            goto_img(m_imgIndex);
            statusBar()->showMessage(
                QString("Landing AI: %1 detection(s)").arg(written), 4000);
            resetCloudButtons();
        } else {
            landingAIProcessNextInQueue();
        }
    });
}

void MainWindow::landingAIAutoLabelAll()
{
    if (m_imgList.isEmpty()) return;

    if (m_landingApiKey.isEmpty()) {
        m_sideTabWidget->setCurrentIndex(1);
        syncAiSettingsTab();
        statusBar()->showMessage("Enter your Landing AI API key in the AI Settings tab.", 4000);
        return;
    }

    if (!checkLandingConsent()) return;

    QMessageBox msgBox(QMessageBox::Question, "Landing AI All",
        QString("Send all %1 images to Landing AI?\nExisting labels will be overwritten.")
            .arg(m_imgList.size()),
        QMessageBox::Yes | QMessageBox::No, this);
    if (msgBox.exec() != QMessageBox::Yes) return;

    save_label_data();

    m_landingCancelled = false;
    m_landingQueue.clear();
    for (int i = 0; i < m_imgList.size(); ++i)
        m_landingQueue.append(i);

    m_btnCloudAutoLabel->setEnabled(false);
    m_btnCloudAutoLabelAll->setEnabled(false);
    m_btnCancelAutoLabel->setVisible(true);
    m_btnCloudAutoLabelAll->setText(
        QString("\u2601 Auto Label All (0/%1)\u2026").arg(m_imgList.size()));
    ui->horizontalSlider_images->setEnabled(false);

    landingAIProcessNextInQueue();
}

void MainWindow::landingAIProcessNextInQueue()
{
    if (m_landingQueue.isEmpty()) {
        goto_img(m_imgIndex);
        statusBar()->showMessage(
            QString("Landing AI: all %1 images done.").arg(m_imgList.size()), 5000);
        resetCloudButtons();
        return;
    }

    int idx   = m_landingQueue.takeFirst();
    // After takeFirst(), remaining = m_landingQueue.size().
    // done = total - remaining - 1 (current image is in-flight, not yet done).
    int done  = m_imgList.size() - m_landingQueue.size() - 1;
    int total = m_imgList.size();
    m_btnCloudAutoLabelAll->setText(
        QString("\u2601 Auto Label All (%1/%2)\u2026").arg(done).arg(total));
    m_btnCloudAutoLabel->setText("\u2601 Labelling\u2026");

    doLandingAIJob(m_imgList[idx]);
}
// ────────────────────────────────────────────────────────────────────────────


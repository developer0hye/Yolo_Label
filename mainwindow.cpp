#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QColorDialog>
#include <QKeyEvent>
#include <QDebug>
#include <QShortcut>
#include <QCollator>
#include <iomanip>
#include <cmath>

using std::cout;
using std::endl;
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

    init_table_widget();
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

    if (argc > 2) {
      QString obj_file = QString::fromLocal8Bit(argv[2]);
      load_label_list_data(obj_file);
    }

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

//void MainWindow::on_pushButton_change_dir_clicked()
//{
//    bool bRetImgDir     = false;

//    open_img_dir(bRetImgDir);

//    if (!bRetImgDir) return ;

//    init();
//}


void MainWindow::init()
{
    m_lastLabeledImgIndex = -1;

    ui->label_image->init();

    init_button_event();
    init_horizontal_slider();

    set_label(0);
    goto_img(0);
}

void MainWindow::set_label_progress(const int fileIndex)
{
    QString strCurFileIndex = QString::number(fileIndex);
    QString strEndFileIndex = QString::number(m_imgList.size() - 1);

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
    if(bSavePrev && ui->label_image->isOpened()) save_label_data();
    goto_img(m_imgIndex + 1);
}

void MainWindow::prev_img(bool bSavePrev)
{
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
        ui->label_image->setFocusObjectName(m_objList.at(m_objIndex));
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
            bool shiftHeld = (event->modifiers() & Qt::ShiftModifier);
            double step = shiftHeld ? 0.01 : 0.002;

            double dx = 0.0, dy = 0.0;
            if(nKey == Qt::Key_Left)  dx = -step;
            if(nKey == Qt::Key_Right) dx =  step;
            if(nKey == Qt::Key_Up)    dy = -step;
            if(nKey == Qt::Key_Down)  dy =  step;

            ui->label_image->moveBox(nudgeBoxIdx, dx, dy);
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

void MainWindow::init_button_event()
{
//    ui->pushButton_change_dir->setEnabled(true);
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

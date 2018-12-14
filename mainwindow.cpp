#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QColorDialog>
#include <QKeyEvent>
#include <QDebug>
#include <QShortcut>

#include <iomanip>

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

    connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_S), this), SIGNAL(activated()), this, SLOT(save_label_data()));
    connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_C), this), SIGNAL(activated()), this, SLOT(clear_label_data()));

    connect(new QShortcut(QKeySequence(Qt::Key_S), this), SIGNAL(activated()), this, SLOT(next_label()));
    connect(new QShortcut(QKeySequence(Qt::Key_W), this), SIGNAL(activated()), this, SLOT(prev_label()));
    connect(new QShortcut(QKeySequence(Qt::Key_A), this), SIGNAL(activated()), this, SLOT(prev_img()));
    connect(new QShortcut(QKeySequence(Qt::Key_D), this), SIGNAL(activated()), this, SLOT(next_img()));
    connect(new QShortcut(QKeySequence(Qt::Key_Space), this), SIGNAL(activated()), this, SLOT(next_img()));

    init_table_widget();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_open_files_clicked()
{
    bool bRetImgDir     = false;
    bool bRetObjFile    = false;

    open_img_dir(bRetImgDir);

    if (!bRetImgDir) return ;

    open_obj_file(bRetObjFile);

    if (!bRetObjFile) return ;

    init();
}

void MainWindow::init()
{
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
    ui->label_file->setText("File: " + m_imgList.at(fileIndex));
}

void MainWindow::goto_img(const int fileIndex)
{
    bool bIndexIsOutOfRange = (fileIndex < 0 || fileIndex > m_imgList.size() - 1);
    if (bIndexIsOutOfRange) return;

    m_imgIndex = fileIndex;

    bool bImgOpened;
    ui->label_image->openImage(m_imgList.at(m_imgIndex), bImgOpened);

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
    if(bSavePrev) save_label_data();
    goto_img(m_imgIndex - 1);
}

void MainWindow::save_label_data()const
{
    if(m_imgList.size() == 0) return;

    QString qstrOutputLabelData = get_labeling_data(m_imgList.at(m_imgIndex));

    ofstream fileOutputLabelData(qstrOutputLabelData.toStdString());

    if(fileOutputLabelData.is_open())
    {
        for(int i = 0; i < ui->label_image->m_objBoundingBoxes.size(); i++)
        {
            if(i != 0) fileOutputLabelData << '\n';

            ObjectLabelingBox objBox = ui->label_image->m_objBoundingBoxes[i];

            if(ui->checkBox_cropping->isChecked())
            {
                QImage cropped = ui->label_image->crop(ui->label_image->cvtRelativeToAbsoluteRectInImage(objBox.box));

                if(!cropped.isNull())
                {
                    string strImgFile   = m_imgList.at(m_imgIndex).toStdString();

                    strImgFile = strImgFile.substr( strImgFile.find_last_of('/') + 1,
                                                    strImgFile.find_last_of('.') - strImgFile.find_last_of('/') - 1);

                    cropped.save(QString().fromStdString(strImgFile) + "_cropped_" + QString::number(i) + ".png");
                }
            }

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
            fileOutputLabelData << std::fixed << std::setprecision(6) << height;
        }

        fileOutputLabelData.close();

        ui->textEdit_log->setText(qstrOutputLabelData + " saved.");
    }
}

void MainWindow::clear_label_data()
{
    ui->label_image->m_objBoundingBoxes.clear();
    ui->label_image->showImage();
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
    ifstream inputLabelListFile(qstrLabelListFile.toStdString());

    if(inputLabelListFile.is_open())
    {

        for(int i = 0 ; i <= ui->tableWidget_label->rowCount(); i++)
            ui->tableWidget_label->removeRow(ui->tableWidget_label->currentRow());

        m_objList.clear();

        ui->label_image->m_drawObjectBoxColor.clear();

        string strLabel;
        int fileIndex = 0;
        while(getline(inputLabelListFile, strLabel))
        {
            int nRow = ui->tableWidget_label->rowCount();
            std::cout << nRow << endl;
            QString qstrLabel   = QString().fromStdString(strLabel);
            QColor  labelColor  = label_img::BOX_COLORS[(fileIndex++)%10];
            m_objList << qstrLabel;

            ui->tableWidget_label->insertRow(nRow);

            ui->tableWidget_label->setItem(nRow, 0, new QTableWidgetItem(qstrLabel));
            ui->tableWidget_label->item(nRow, 0)->setFlags(ui->tableWidget_label->item(nRow, 0)->flags() ^  Qt::ItemIsEditable);

            ui->tableWidget_label->setItem(nRow, 1, new QTableWidgetItem(QString().fromStdString("")));
            ui->tableWidget_label->item(nRow, 1)->setBackgroundColor(labelColor);
            ui->tableWidget_label->item(nRow, 1)->setFlags(ui->tableWidget_label->item(nRow, 1)->flags() ^  Qt::ItemIsEditable ^  Qt::ItemIsSelectable);

            ui->label_image->m_drawObjectBoxColor.push_back(labelColor);
        }
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

void MainWindow::open_img_dir(bool& ret)
{
    pjreddie_style_msgBox(QMessageBox::Information,"Help", "Step 1. Open Your Data Set Directory");

    QString imgDir = QFileDialog::getExistingDirectory(
                this,
                tr("Open Dataset Directory"),
                "./",QFileDialog::ShowDirsOnly);

    QDir dir(imgDir);

    QStringList fileList = dir.entryList(
                QStringList() << "*.jpg" << "*.JPG" << "*.png",
                QDir::Files);

    if(fileList.empty())
    {
        ret = false;
        pjreddie_style_msgBox(QMessageBox::Critical,"Error", "This folder is empty");
    }
    else
    {
        ret = true;
        m_imgDir    = imgDir;
        m_imgList  = fileList;

        for(QString& str: m_imgList)
            str = m_imgDir + "/" + str;
    }
}

void MainWindow::open_obj_file(bool& ret)
{
    pjreddie_style_msgBox(QMessageBox::Information,"Help", "Step 2. Open Your Label List File(*.txt or *.names)");

    QString fileLabelList = QFileDialog::getOpenFileName(
                this,
                tr("Open LabelList file"),
                "./",
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

void MainWindow::reupdate_img_list()
{

}

void MainWindow::wheelEvent(QWheelEvent *ev)
{
    if(ev->delta() > 0) // up Wheel
        prev_img();
    else if(ev->delta() < 0) //down Wheel
        next_img();
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
    cout << "key pressed" <<endl;
    int     nKey = event->key();

    bool    graveAccentKeyIsPressed    = (nKey == Qt::Key_QuoteLeft);
    bool    numKeyIsPressed            = (nKey >= Qt::Key_0 && nKey <= Qt::Key_9 );

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
}

void MainWindow::on_pushButton_save_clicked()
{
    save_label_data();
}

void MainWindow::on_pushButton_remove_clicked()
{
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
        m_imgIndex --;
    }

    goto_img(m_imgIndex);
}

void MainWindow::on_tableWidget_label_cellDoubleClicked(int row, int column)
{
    bool bColorAttributeClicked = (column == 1);

    if(bColorAttributeClicked)
    {
        QColor color = QColorDialog::getColor(Qt::white,this,"Set Label Color");
        if(color.isValid())
        {
            set_label_color(row, color);
            ui->tableWidget_label->item(row, 1)->setBackgroundColor(color);
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
    ui->pushButton_next->setEnabled(true);
    ui->pushButton_prev->setEnabled(true);
    ui->pushButton_save->setEnabled(true);
    ui->pushButton_remove->setEnabled(true);
}

void MainWindow::init_horizontal_slider()
{
    ui->horizontalSlider_images->setEnabled(true);
    ui->horizontalSlider_images->setRange(0, m_imgList.size() - 1);
    ui->horizontalSlider_images->blockSignals(true);
    ui->horizontalSlider_images->setValue(0);
    ui->horizontalSlider_images->blockSignals(false);
}

void MainWindow::init_table_widget()
{
    ui->tableWidget_label->horizontalHeader()->setVisible(true);
    ui->tableWidget_label->horizontalHeader()->setStyleSheet("");
    ui->tableWidget_label->horizontalHeader()->setStretchLastSection(true);

    disconnect(ui->tableWidget_label->horizontalHeader(), SIGNAL(sectionPressed(int)),ui->tableWidget_label, SLOT(selectColumn(int)));
}

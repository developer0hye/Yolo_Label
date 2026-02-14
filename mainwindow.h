#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWheelEvent>
#include <QTableWidgetItem>
#include <QMessageBox>
#include <QTimer>
#include <QLabel>
#include <QPushButton>

#include "label_img.h"
#include <iostream>
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

    void copy_previous_annotations();

    void undo();
    void redo();

    void on_usageTimer_timeout();
    void on_usageTimerReset_clicked();

    void reset_zoom();

private:
    void updateUsageTimerLabel();

    void            init();
    void            init_table_widget();
    void            init_button_event();
    void            init_horizontal_slider();

    void            set_label_progress(const int);
    void            set_focused_file(const int);

    void            goto_img(const int);

    void            load_label_list_data(QString);
    QString         get_labeling_data(QString)const;

    void            set_label(const int);
    void            set_label_color(const int , const QColor);

    void            pjreddie_style_msgBox(QMessageBox::Icon, QString, QString);

    void            open_img_dir(bool&);
    void            open_obj_file(bool&);
    bool            get_files(QString imgDir);

    Ui::MainWindow *ui;

    QString         m_imgDir;
    QStringList     m_imgList;
    int             m_imgIndex;

    QStringList     m_objList;
    int             m_objIndex;
    int             m_lastLabeledImgIndex;

    QVector<ObjectLabelingBox> m_previousAnnotations;

    QTimer         *m_usageTimer;
    qint64          m_usageTimerElapsedSeconds;
    QLabel         *m_usageTimerLabel;
    QPushButton    *m_usageTimerResetButton;

protected:
    void    wheelEvent(QWheelEvent *);

};

#endif // MAINWINDOW_H

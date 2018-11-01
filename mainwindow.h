#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWheelEvent>
#include <QTableWidgetItem>
#include <QMessageBox>

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

Q_SIGNALS:

private slots:
    void on_pushButton_open_files_clicked();
    void on_pushButton_save_clicked();

    void on_pushButton_prev_clicked();
    void on_pushButton_next_clicked();

    void keyPressEvent(QKeyEvent *);

    void mouseCurrentPos();
    void mousePressed();
    void mouseReleased();

    void next_img();
    void prev_img();
    void save_label_data() const;
    void clear_label_data();

    void next_label();
    void prev_label();

    void on_tableWidget_label_cellDoubleClicked(int row, int column);
    void on_tableWidget_label_cellClicked(int row, int column);

    void on_horizontalSlider_images_sliderMoved(int position);
    void on_horizontalSlider_images_sliderPressed();

private:
    Ui::MainWindow *ui;

    QString         m_fileDir;
    QStringList     m_fileList;
    int             m_fileIndex;

    QStringList     m_labelNameList;
    int             m_labelIndex;


    void            init_tableWidget();

    void            init();

    void            img_open(const int);
    void            set_label_progress(const int);
    void            set_focused_file(const int);

    void            goto_img(int);

    void            load_label_list_data(QString);
    QString         get_labeling_data(QString)const;

    void            set_label(int);
    void            set_label_color(int , QColor);

    void            pjreddie_style_msgBox(QMessageBox::Icon, QString, QString);

protected:
    void    wheelEvent(QWheelEvent *);
};

#endif // MAINWINDOW_H

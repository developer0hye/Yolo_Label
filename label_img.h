#ifndef LABEL_IMG_H
#define LABEL_IMG_H

#include <QObject>
#include <QLabel>
#include <QImage>
#include <QMouseEvent>
#include <iostream>
#include <fstream>

struct ObjectLabelingBox
{
    int     label;
    QRectF  box;
};

class label_img : public QLabel
{
    Q_OBJECT
public:
    label_img(QWidget *parent = nullptr);

    void mouseMoveEvent(QMouseEvent *ev);
    void mousePressEvent(QMouseEvent *ev);
    void mouseReleaseEvent(QMouseEvent *ev);

    QVector<QColor> m_drawObjectBoxColor;

    int m_uiX;
    int m_uiY;

    int m_imgX;
    int m_imgY;

    bool m_bMouseLeftButtonClicked;

    static  QColor BOX_COLORS[10];

    QVector <ObjectLabelingBox>     m_objBoundingBoxes;

    void openImage(const QString &);
    void showImage();

    void loadLabelData(const QString & );

    void setFocusObjectLabel(int);
    void setFocusObjectName(QString);

    QImage crop(QRect);

    QRectF  getRelativeRectFromTwoPoints(QPointF , QPointF);

    QRect   cvtRelativeToAbsoluteRectInUi(QRectF);
    QRect   cvtRelativeToAbsoluteRectInImage(QRectF);

    QPoint  cvtRelativeToAbsolutePoint(QPointF);
    QPointF cvtAbsoluteToRelativePoint(QPoint);

signals:
    void Mouse_Moved();
    void Mouse_Pressed();
    void Mouse_Release();

private:
    int             m_focusedObjectLabel;
    QString         m_foucsedObjectName;

    double m_aspectRatioWidth;
    double m_aspectRatioHeight;

    QImage m_inputImg;

    QPointF m_relative_mouse_pos_in_ui;
    QPointF m_relatvie_mouse_pos_LBtnClicked_in_ui;

    void setMousePosition(int , int);

    void drawCrossLine(QPainter& , QColor , int thickWidth = 3);
    void drawFocusedObjectBox(QPainter& , Qt::GlobalColor , int thickWidth = 3);
    void drawObjectBoxes(QPainter& , int thickWidth = 3);

    void removeFocusedObjectBox(QPointF);
};

#endif // LABEL_IMG_H

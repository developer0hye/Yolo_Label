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
    QStringList     m_objList;

    int m_uiX;
    int m_uiY;

    int m_imgX;
    int m_imgY;

    bool m_bLabelingStarted;
    bool m_bVisualizeClassName;

    static  QColor BOX_COLORS[10];

    QVector <ObjectLabelingBox>     m_objBoundingBoxes;

    void init();
    void openImage(const QString &, bool& ret);
    void showImage();

    void loadLabelData(const QString & );

    void setFocusObjectLabel(int);
    void setFocusObjectName(QString);
    void setContrastGamma(float);

    bool isOpened();

    void saveState();
    void clearUndoHistory();

    void moveBoxUnderCursor(QPointF cursorPos, double dx, double dy);
    int  findBoxUnderCursor(QPointF point) const;
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
    QImage m_resized_inputImg;

    QPointF m_relative_mouse_pos_in_ui;
    QPointF m_relatvie_mouse_pos_LBtnClicked_in_ui;

    unsigned char m_gammatransform_lut[256];
    QVector<QRgb> colorTable;

    QVector< QVector<ObjectLabelingBox> > m_undoHistory;

    bool    m_bDragging;
    bool    m_bDragPending;
    int     m_dragBoxIdx;
    QPointF m_dragOffset;
    QPointF m_dragStartPos;

    void setMousePosition(int , int);

    void drawCrossLine(QPainter& , QColor , int thickWidth = 3);
    void drawFocusedObjectBox(QPainter& , Qt::GlobalColor , int thickWidth = 3);
    void drawObjectBoxes(QPainter& , int thickWidth = 3);
    void drawObjectLabels(QPainter& , int thickWidth = 3, int fontPixelSize = 14, int xMargin = 5, int yMargin = 2);
    void gammaTransform(QImage& image);
    void removeFocusedObjectBox(QPointF);
};

#endif // LABEL_IMG_H

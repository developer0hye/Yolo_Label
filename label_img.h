#ifndef LABEL_IMG_H
#define LABEL_IMG_H

#include <QObject>
#include <QLabel>
#include <QImage>
#include <QMouseEvent>
#include <QWheelEvent>
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

    void setFocusedObjectBoxLabel(QPointF point, int newLabel);

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
    void setContrastGamma(float);

    bool isOpened();

    void clearAllBoxes();
    void saveState();
    bool undo();
    bool redo();
    void clearUndoHistory();

    void moveBox(int boxIdx, double dx, double dy);
    int  findBoxUnderCursor(QPointF point) const;
    QImage getInputImage() const;

    void zoomIn(QPoint widgetPos);
    void zoomOut(QPoint widgetPos);
    void resetZoom();

    QRectF  getRelativeRectFromTwoPoints(QPointF , QPointF);

    QRect   cvtRelativeToAbsoluteRectInUi(QRectF);
    QPoint  cvtRelativeToAbsolutePoint(QPointF);
    QPointF cvtAbsoluteToRelativePoint(QPoint);


signals:
    void Mouse_Moved();
    void Mouse_Pressed();
    void Mouse_Release();

private:
    int             m_focusedObjectLabel;

    double m_aspectRatioWidth;
    double m_aspectRatioHeight;

    QImage m_inputImg;
    QImage m_resized_inputImg;

    QPointF m_relative_mouse_pos_in_ui;
    QPointF m_relatvie_mouse_pos_LBtnClicked_in_ui;

    unsigned char m_gammatransform_lut[256];
    QVector<QRgb> colorTable;

    static const int MAX_UNDO_HISTORY = 50;
    QVector< QVector<ObjectLabelingBox> > m_undoHistory;
    QVector< QVector<ObjectLabelingBox> > m_redoHistory;

    bool    m_bDragging;
    bool    m_bDragPending;
    int     m_dragBoxIdx;
    QPointF m_dragOffset;
    QPointF m_dragStartPos;

    double  m_zoomFactor;
    QPointF m_panOffset;
    bool    m_bPanning;
    QPoint  m_panStartWidgetPos;
    QPointF m_panStartOffset;

    void setMousePosition(int , int);
    void clampPanOffset();

    void drawCrossLine(QPainter& , QColor , int thickWidth = 3);
    void drawFocusedObjectBox(QPainter& , Qt::GlobalColor , int thickWidth = 3);
    void drawObjectBoxes(QPainter& , int thickWidth = 3);
    void drawObjectLabels(QPainter& , int thickWidth = 3, int fontPixelSize = 14, int xMargin = 5, int yMargin = 2);
    void gammaTransform(QImage& image);
    bool removeFocusedObjectBox(QPointF);

protected:
    void wheelEvent(QWheelEvent *ev) override;
};

#endif // LABEL_IMG_H

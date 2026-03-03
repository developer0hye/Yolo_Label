#include "label_img.h"
#include "qdir.h"
#include <QPainter>
#include <QTimer>
#include <QImageReader>
#include <math.h>       /* fabs */
#include <algorithm>
//#include <omp.h>

using std::ifstream;

QColor label_img::BOX_COLORS[10] ={  Qt::green,
        Qt::darkGreen,
        Qt::blue,
        Qt::darkBlue,
        Qt::yellow,
        Qt::darkYellow,
        Qt::red,
        Qt::darkRed,
        Qt::cyan,
        Qt::darkCyan};

label_img::label_img(QWidget *parent)
    :QLabel(parent)
{
    init();
}

void label_img::mouseMoveEvent(QMouseEvent *ev)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint pos = ev->position().toPoint();
#else
    const QPoint pos = ev->pos();
#endif
    setMousePosition(pos.x(), pos.y());

    showImage();
    emit Mouse_Moved();
}

void label_img::mousePressEvent(QMouseEvent *ev)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint pos = ev->position().toPoint();
#else
    const QPoint pos = ev->pos();
#endif
    setMousePosition(pos.x(), pos.y());

    if(ev->button() == Qt::RightButton)
    {
        removeFocusedObjectBox(m_relative_mouse_pos_in_ui);
        showImage();
    }
    else if(ev->button() == Qt::LeftButton)
    {
        if(m_bLabelingStarted == false)
        {
            m_relatvie_mouse_pos_LBtnClicked_in_ui      = m_relative_mouse_pos_in_ui;
            m_bLabelingStarted                          = true;
        }
        else
        {
            ObjectLabelingBox objBoundingbox;

            objBoundingbox.label    = m_focusedObjectLabel;
            objBoundingbox.box      = getRelativeRectFromTwoPoints(m_relative_mouse_pos_in_ui,
                                                                   m_relatvie_mouse_pos_LBtnClicked_in_ui);

            bool width_is_too_small     = objBoundingbox.box.width() * m_inputImg.width()  < 4;
            bool height_is_too_small    = objBoundingbox.box.height() * m_inputImg.height() < 4;

            if(!width_is_too_small && !height_is_too_small)
                m_objBoundingBoxes.push_back(objBoundingbox);

            m_bLabelingStarted              = false;

            showImage();
        }
    }
    else if(ev->button() == Qt::MiddleButton)
    {
        qDebug() << "Get wheel";
        // double  nearestBoxDistance   = 99999999999999.;

        // for(int i = 0; i < m_objBoundingBoxes.size(); i++)
        // {
        //     QRectF objBox = m_objBoundingBoxes.at(i).box;


        //     if(objBox.contains(m_relative_mouse_pos_in_ui))
        //     {
        //         m_objBoundingBoxes.at(i).label = (m_objBoundingBoxes.at(i).label + 1) % m_objList.size();

        //         // Визуальный эффект - временное увеличение размера
        //         QRectF original = box.box;
        //         box.box.adjust(-5, -5, 5, 5);
        //         showImage();
        //         box.box = original;

        //         QTimer::singleShot(100, this, [this]() {
        //             showImage();
        //         });
        //     }
        // }

        for (auto& box : m_objBoundingBoxes) {
            if (box.box.contains(m_relative_mouse_pos_in_ui)) {
                box.label = (box.label + 1) % m_objList.size();

                // Визуальный эффект - временное увеличение размера
                QRectF original = box.box;
                box.box.adjust(-5, -5, 5, 5);
                showImage();
                box.box = original;

                QTimer::singleShot(100, this, [this]() {
                    showImage();
                });

                // emit labelChanged();
                // ev->accept();
            }
        }
    }

    emit Mouse_Pressed();
}

void label_img::mouseReleaseEvent(QMouseEvent *ev)
{
    Q_UNUSED(ev);
    emit Mouse_Release();
}

bool label_img::zoomAtPosition(const QPoint& posInUi, int wheelDeltaY)
{
    if(m_inputImg.isNull()) return false;
    if(this->width() <= 0 || this->height() <= 0) return false;
    if(wheelDeltaY == 0) return false;

    const QPoint clampedPos(
        std::clamp(posInUi.x(), 0, this->width() - 1),
        std::clamp(posInUi.y(), 0, this->height() - 1));
    const QPointF anchorInImage = cvtAbsoluteToRelativePoint(clampedPos);

    const double zoomStep = 1.1;
    const bool isZoomIn = wheelDeltaY > 0;

    if(isZoomIn)
        m_zoomFactor = std::min(m_zoomFactor * zoomStep, m_maxZoomFactor);
    else
        m_zoomFactor = std::max(m_zoomFactor / zoomStep, m_minZoomFactor);

    if(m_zoomFactor <= 1.0)
    {
        m_zoomFactor = 1.0;
        m_viewTopLeftInImage = QPointF(0.0, 0.0);
    }
    else
    {
        const QPointF visibleSize = getVisibleRegionSize();
        const QPointF anchorInUi(static_cast<double>(clampedPos.x()) / this->width(),
                                 static_cast<double>(clampedPos.y()) / this->height());

        m_viewTopLeftInImage.setX(anchorInImage.x() - anchorInUi.x() * visibleSize.x());
        m_viewTopLeftInImage.setY(anchorInImage.y() - anchorInUi.y() * visibleSize.y());
        clampViewTopLeft();
    }

    showImage();
    return true;
}

void label_img::init()
{
    m_objBoundingBoxes.clear();
    m_bLabelingStarted              = false;
    m_focusedObjectLabel            = 0;
    m_zoomFactor                    = 1.0;
    m_minZoomFactor                 = 1.0;
    m_maxZoomFactor                 = 20.0;
    m_drawLineThickness             = 3;
    m_viewTopLeftInImage            = QPointF(0.0, 0.0);

    QPoint mousePosInUi = this->mapFromGlobal(QCursor::pos());
    bool mouse_is_in_image = QRect(0, 0, this->width(), this->height()).contains(mousePosInUi);

    if  (mouse_is_in_image)
    {
        setMousePosition(mousePosInUi.x(), mousePosInUi.y());
    }
    else
    {
        setMousePosition(0., 0.);
    }
}

void label_img::setMousePosition(int x, int y)
{
    if(x < 0) x = 0;
    if(y < 0) y = 0;

    if(x > this->width())   x = this->width() - 1;
    if(y > this->height())  y = this->height() - 1;

    m_relative_mouse_pos_in_ui = cvtAbsoluteToRelativePoint(QPoint(x, y));
}

void label_img::openImage(const QString &qstrImg, bool &ret)
{
    QImageReader imgReader(qstrImg);
    imgReader.setAutoTransform(true);
    QImage img = imgReader.read();

    if(img.isNull())
    {
        m_inputImg = QImage();
        ret = false;
    }
    else
    {
        ret = true;

        m_objBoundingBoxes.clear();

        m_inputImg          = img.copy();
        m_inputImg          = m_inputImg.convertToFormat(QImage::Format_RGB888);
        m_resized_inputImg  = m_inputImg.scaled(this->width(), this->height(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation)
                .convertToFormat(QImage::Format_RGB888);
        m_zoomFactor        = 1.0;
        m_viewTopLeftInImage = QPointF(0.0, 0.0);

        m_bLabelingStarted  = false;

        QPoint mousePosInUi     = this->mapFromGlobal(QCursor::pos());
        bool mouse_is_in_image  = QRect(0, 0, this->width(), this->height()).contains(mousePosInUi);

        if  (mouse_is_in_image)
        {
            setMousePosition(mousePosInUi.x(), mousePosInUi.y());
        }
        else
        {
            setMousePosition(0., 0.);
        }
    }
}

void label_img::showImage()
{
    if(m_inputImg.isNull()) return;
    clampViewTopLeft();

    const QPointF visibleRegionSize = getVisibleRegionSize();
    int cropX = static_cast<int>(m_viewTopLeftInImage.x() * m_inputImg.width() + 0.5);
    int cropY = static_cast<int>(m_viewTopLeftInImage.y() * m_inputImg.height() + 0.5);
    int cropW = static_cast<int>(visibleRegionSize.x() * m_inputImg.width() + 0.5);
    int cropH = static_cast<int>(visibleRegionSize.y() * m_inputImg.height() + 0.5);

    cropW = std::max(cropW, 1);
    cropH = std::max(cropH, 1);

    if(cropX + cropW > m_inputImg.width()) cropX = m_inputImg.width() - cropW;
    if(cropY + cropH > m_inputImg.height()) cropY = m_inputImg.height() - cropH;
    cropX = std::clamp(cropX, 0, std::max(m_inputImg.width() - 1, 0));
    cropY = std::clamp(cropY, 0, std::max(m_inputImg.height() - 1, 0));

    const QRect cropRect(cropX, cropY, cropW, cropH);
    m_resized_inputImg = m_inputImg.copy(cropRect)
                             .scaled(this->width(), this->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                             .convertToFormat(QImage::Format_RGB888);

    QImage img = m_resized_inputImg;

    gammaTransform(img);

    QPainter painter(&img);
    QFont font = painter.font();
    int fontSize = 16, xMargin = 5, yMargin = 2;
    font.setPixelSize(fontSize);
    font.setBold(true);
    painter.setFont(font);

    int penThick = m_drawLineThickness;

    QColor crossLineColor(255, 187, 0);

    drawCrossLine(painter, crossLineColor, penThick);
    drawFocusedObjectBox(painter, Qt::magenta, penThick);
    drawObjectBoxes(painter, penThick);
    if(m_bVisualizeClassName)
        drawObjectLabels(painter, penThick, fontSize, xMargin, yMargin);

    this->setPixmap(QPixmap::fromImage(img));
}

void label_img::loadLabelData(const QString& labelFilePath)
{
    //qDebug() << "Trying to load label file:" << labelFilePath;
    m_objBoundingBoxes.clear();

    QFile file(labelFilePath);
    if (!file.exists()) {
        //qDebug() << "File does not exist:" << labelFilePath;
        return;
    }

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        //qDebug() << "File opened successfully";
        QTextStream in(&file);

        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;

            QStringList values = line.split(' ', Qt::SkipEmptyParts);
            if (values.size() != 5) {
                //qDebug() << "Invalid line format:" << line;
                continue;
            }

            bool ok;
            ObjectLabelingBox objBox;

            // Парсим класс
            objBox.label = values[0].toInt(&ok);
            if (!ok) {
                //qDebug() << "Invalid class id:" << values[0];
                continue;
            }

            // Парсим координаты
            double midX = values[1].toDouble(&ok);
            if (!ok || midX < 0 || midX > 1) {
                //qDebug() << "Invalid x_center:" << values[1];
                continue;
            }

            double midY = values[2].toDouble(&ok);
            if (!ok || midY < 0 || midY > 1) {
                //qDebug() << "Invalid y_center:" << values[2];
                continue;
            }

            double width = values[3].toDouble(&ok);
            if (!ok || width <= 0 || width > 1) {
                //qDebug() << "Invalid width:" << values[3];
                continue;
            }

            double height = values[4].toDouble(&ok);
            if (!ok || height <= 0 || height > 1) {
                //qDebug() << "Invalid height:" << values[4];
                continue;
            }

            // Рассчитываем координаты прямоугольника
            double leftX = midX - width/2.0;
            double topY = midY - height/2.0;

            objBox.box.setRect(leftX, topY, width, height);
            m_objBoundingBoxes.push_back(objBox);

            //qDebug() << "Loaded box:" << objBox.label << leftX << topY << width << height;
        }
        file.close();
    } else {
        qDebug() << "Failed to open file:" << labelFilePath
                 << "Error:" << file.errorString();
    }

    //showImage(); // Обновляем отображение
}

void loadLabelData(const QString& labelFilePath)
{
    //qDebug() << "Inside loadLabelData";
    ifstream inputFile(qPrintable(labelFilePath));

    if(inputFile.is_open())
    {
        //qDebug() << "loadLabelData " << labelFilePath << " is open";
        double          inputFileValue;
        QVector<double> inputFileValues;

        while(inputFile >> inputFileValue)
            inputFileValues.push_back(inputFileValue);

        for(int i = 0; i < inputFileValues.size(); i += 5)
        {
            try {
                // 0 0.304567 0.628547 0.094422 0.044712
                // 0 0.815766 0.636715 0.091787 0.043852

                //qDebug() << "Data loadLabelData" << inputFileValues.data();
                ObjectLabelingBox objBox;

                objBox.label = static_cast<int>(inputFileValues.at(i));

                double midX     = inputFileValues.at(i + 1);
                double midY     = inputFileValues.at(i + 2);
                double width    = inputFileValues.at(i + 3);
                double height   = inputFileValues.at(i + 4);

                double leftX    = midX - width/2.;
                double topY     = midY - height/2.;

                objBox.box.setX(leftX); // translation: midX -> leftX
                objBox.box.setY(topY); // translation: midY -> topY
                objBox.box.setWidth(width);
                objBox.box.setHeight(height);

                // m_objBoundingBoxes.push_back(objBox);
            }
            catch (const std::out_of_range& e) {
                std::cout << "loadLabelData: Out of Range error.";
            }
        }
    }
}

void label_img::setFocusObjectLabel(int nLabel)
{
    m_focusedObjectLabel = nLabel;
}

void label_img::setFocusObjectName(QString qstrName)
{
    m_foucsedObjectName = qstrName;
}

bool label_img::isOpened()
{
    return !m_inputImg.isNull();
}

QImage label_img::crop(QRect rect)
{
    return m_inputImg.copy(rect);
}

void label_img::drawCrossLine(QPainter& painter, QColor color, int thickWidth)
{
    if(m_relative_mouse_pos_in_ui == QPointF(0., 0.)) return;

    QPen pen;
    pen.setWidth(thickWidth);

    pen.setColor(color);
    painter.setPen(pen);

    QPoint absolutePoint = cvtRelativeToAbsolutePoint(m_relative_mouse_pos_in_ui);

    //draw cross line
    painter.drawLine(QPoint(absolutePoint.x(),0), QPoint(absolutePoint.x(), this->height() - 1));
    painter.drawLine(QPoint(0,absolutePoint.y()), QPoint(this->width() - 1, absolutePoint.y()));
}

void label_img::drawFocusedObjectBox(QPainter& painter, Qt::GlobalColor color, int thickWidth)
{
    if(m_bLabelingStarted == true)
    {
        QPen pen;
        pen.setWidth(thickWidth);

        pen.setColor(color);
        painter.setPen(pen);

        //relative coord to absolute coord

        QPoint absolutePoint1 = cvtRelativeToAbsolutePoint(m_relatvie_mouse_pos_LBtnClicked_in_ui);
        QPoint absolutePoint2 = cvtRelativeToAbsolutePoint(m_relative_mouse_pos_in_ui);

        painter.drawRect(QRect(absolutePoint1, absolutePoint2));
    }
}

void label_img::drawObjectBoxes(QPainter& painter, int thickWidth)
{
    QPen pen;
    pen.setWidth(thickWidth);

    for(ObjectLabelingBox boundingbox: m_objBoundingBoxes)
    {
        pen.setColor(m_drawObjectBoxColor.at(boundingbox.label));
        painter.setPen(pen);

        painter.drawRect(cvtRelativeToAbsoluteRectInUi(boundingbox.box));
    }
}

void label_img::drawObjectLabels(QPainter& painter, int thickWidth, int fontPixelSize, int xMargin, int yMargin)
{
    QFontMetrics fontMetrics = painter.fontMetrics();
    QPen blackPen;

    for(ObjectLabelingBox boundingbox: m_objBoundingBoxes)
    {
        QColor labelColor = m_drawObjectBoxColor.at(boundingbox.label);
        QRect rectUi = cvtRelativeToAbsoluteRectInUi(boundingbox.box);

        QRect labelRect = fontMetrics.boundingRect(m_objList.at(boundingbox.label));
        if (rectUi.top() > fontPixelSize + yMargin * 2 + thickWidth + 1) {
            labelRect.moveTo(rectUi.topLeft() + QPoint(-thickWidth / 2, -(fontPixelSize + yMargin * 2 + thickWidth + 1)));
            labelRect.adjust(0, 0, xMargin * 2, yMargin * 2);
        } else {
            labelRect.moveTo(rectUi.topLeft() + QPoint(-thickWidth / 2, 0));
            labelRect.adjust(0, 0, xMargin * 2, yMargin * 2);
        }
        painter.fillRect(labelRect, labelColor);

        if (qGray(m_drawObjectBoxColor.at(boundingbox.label).rgb()) > 120) {
            blackPen.setColor(QColorConstants::Black);
        } else {
            blackPen.setColor(QColorConstants::White);
        }
        painter.setPen(blackPen);
        painter.drawText(labelRect.topLeft() + QPoint(xMargin, yMargin + fontPixelSize), m_objList.at(boundingbox.label));
    }
}

void label_img::gammaTransform(QImage &image)
{
    uchar* bits = image.bits();

    int h = image.height();
    int w = image.width();

    //#pragma omp parallel for collapse(2)
    for(int y = 0 ; y < h; ++y)
    {
        for(int x = 0; x < w; ++x)
        {
            int index_pixel = (y*w+x)*3;

            unsigned char r = bits[index_pixel + 0];
            unsigned char g = bits[index_pixel + 1];
            unsigned char b = bits[index_pixel + 2];

            bits[index_pixel + 0] = m_gammatransform_lut[r];
            bits[index_pixel + 1] = m_gammatransform_lut[g];
            bits[index_pixel + 2] = m_gammatransform_lut[b];
        }
    }
}

void label_img::removeFocusedObjectBox(QPointF point)
{
    int     removeBoxIdx = -1;
    double  nearestBoxDistance   = 99999999999999.;

    for(int i = 0; i < m_objBoundingBoxes.size(); i++)
    {
        QRectF objBox = m_objBoundingBoxes.at(i).box;

        if(objBox.contains(point))
        {
            double distance = objBox.width() + objBox.height();
            if(distance < nearestBoxDistance)
            {
                nearestBoxDistance = distance;
                removeBoxIdx = i;
            }
        }
    }

    if(removeBoxIdx != -1)
    {
        m_objBoundingBoxes.remove(removeBoxIdx);
    }
}

QRectF label_img::getRelativeRectFromTwoPoints(QPointF p1, QPointF p2)
{
    double midX    = (p1.x() + p2.x()) / 2.;
    double midY    = (p1.y() + p2.y()) / 2.;
    double width   = fabs(p1.x() - p2.x());
    double height  = fabs(p1.y() - p2.y());

    QPointF topLeftPoint(midX - width/2., midY - height/2.);
    QPointF bottomRightPoint(midX + width/2., midY + height/2.);

    return QRectF(topLeftPoint, bottomRightPoint);
}

QRect label_img::cvtRelativeToAbsoluteRectInUi(QRectF rectF)
{
    const QPointF visibleSize = getVisibleRegionSize();

    const double x = (rectF.x() - m_viewTopLeftInImage.x()) / visibleSize.x();
    const double y = (rectF.y() - m_viewTopLeftInImage.y()) / visibleSize.y();
    const double w = rectF.width() / visibleSize.x();
    const double h = rectF.height() / visibleSize.y();

    return QRect(static_cast<int>(x * this->width() + 0.5),
                 static_cast<int>(y * this->height()+ 0.5),
                 static_cast<int>(w * this->width()+ 0.5),
                 static_cast<int>(h * this->height()+ 0.5));
}

QRect label_img::cvtRelativeToAbsoluteRectInImage(QRectF rectF)
{
    return QRect(static_cast<int>(rectF.x() * m_inputImg.width()),
                 static_cast<int>(rectF.y() * m_inputImg.height()),
                 static_cast<int>(rectF.width() * m_inputImg.width()),
                 static_cast<int>(rectF.height()* m_inputImg.height()));
}

QPoint label_img::cvtRelativeToAbsolutePoint(QPointF p)
{
    const QPointF visibleSize = getVisibleRegionSize();

    const double x = (p.x() - m_viewTopLeftInImage.x()) / visibleSize.x();
    const double y = (p.y() - m_viewTopLeftInImage.y()) / visibleSize.y();

    return QPoint(static_cast<int>(x * this->width() + 0.5), static_cast<int>(y * this->height() + 0.5));
}

QPointF label_img::cvtAbsoluteToRelativePoint(QPoint p)
{
    if(this->width() <= 0 || this->height() <= 0)
        return QPointF(0.0, 0.0);

    const QPointF visibleSize = getVisibleRegionSize();

    const double uiX = std::clamp(static_cast<double>(p.x()) / this->width(), 0.0, 1.0);
    const double uiY = std::clamp(static_cast<double>(p.y()) / this->height(), 0.0, 1.0);

    const double imgX = m_viewTopLeftInImage.x() + uiX * visibleSize.x();
    const double imgY = m_viewTopLeftInImage.y() + uiY * visibleSize.y();

    return QPointF(imgX, imgY);
}

void label_img::setContrastGamma(float gamma)
{
    for(int i=0; i < 256; i++)
    {
        int s = (int)(pow((float)i/255., gamma) * 255.);
        s = std::clamp(s, 0, 255);
        m_gammatransform_lut[i] = (unsigned char)s;
    }
    showImage();
}

void label_img::zoomIn()
{
    setZoomFactor(m_zoomFactor * 1.25);
}

void label_img::zoomOut()
{
    setZoomFactor(m_zoomFactor / 1.25);
}

void label_img::setZoomFactor(double zoomFactor)
{
    if(m_inputImg.isNull()) return;

    const double clampedZoom = std::clamp(zoomFactor, m_minZoomFactor, m_maxZoomFactor);
    if(std::abs(clampedZoom - m_zoomFactor) < 1e-9) return;

    const QPointF oldVisibleSize = getVisibleRegionSize();
    const QPointF viewCenter(m_viewTopLeftInImage.x() + oldVisibleSize.x() * 0.5,
                             m_viewTopLeftInImage.y() + oldVisibleSize.y() * 0.5);

    m_zoomFactor = clampedZoom;

    const QPointF newVisibleSize = getVisibleRegionSize();
    m_viewTopLeftInImage.setX(viewCenter.x() - newVisibleSize.x() * 0.5);
    m_viewTopLeftInImage.setY(viewCenter.y() - newVisibleSize.y() * 0.5);
    clampViewTopLeft();
    showImage();
}

double label_img::zoomFactor() const
{
    return m_zoomFactor;
}

void label_img::panByUiPixels(int dx, int dy)
{
    if(m_inputImg.isNull()) return;
    if(m_zoomFactor <= 1.0) return;
    if(this->width() <= 0 || this->height() <= 0) return;

    const QPointF visibleSize = getVisibleRegionSize();
    const double dxInImage = visibleSize.x() * static_cast<double>(dx) / this->width();
    const double dyInImage = visibleSize.y() * static_cast<double>(dy) / this->height();

    m_viewTopLeftInImage.setX(m_viewTopLeftInImage.x() + dxInImage);
    m_viewTopLeftInImage.setY(m_viewTopLeftInImage.y() + dyInImage);
    clampViewTopLeft();
    showImage();
}

void label_img::setLineThickness(int thickness)
{
    m_drawLineThickness = std::max(1, thickness);
    showImage();
}

int label_img::lineThickness() const
{
    return m_drawLineThickness;
}

void label_img::clampViewTopLeft()
{
    const QPointF visibleSize = getVisibleRegionSize();

    const double maxX = std::max(0.0, 1.0 - visibleSize.x());
    const double maxY = std::max(0.0, 1.0 - visibleSize.y());

    m_viewTopLeftInImage.setX(std::clamp(m_viewTopLeftInImage.x(), 0.0, maxX));
    m_viewTopLeftInImage.setY(std::clamp(m_viewTopLeftInImage.y(), 0.0, maxY));
}

QPointF label_img::getVisibleRegionSize() const
{
    const double safeZoomFactor = std::max(m_zoomFactor, 1.0);
    const double width = 1.0 / safeZoomFactor;
    const double height = 1.0 / safeZoomFactor;
    return QPointF(width, height);
}

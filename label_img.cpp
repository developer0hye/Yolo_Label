#include "label_img.h"
#include <QPainter>
#include <QImageReader>
#include <math.h>       /* fabs */
#include <algorithm>

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
    if(m_bPanning)
    {
        QPoint currentPos = ev->position().toPoint();
        int dx = currentPos.x() - m_panStartWidgetPos.x();
        int dy = currentPos.y() - m_panStartWidgetPos.y();
        m_panOffset.setX(m_panStartOffset.x() - static_cast<double>(dx) / (m_zoomFactor * this->width()));
        m_panOffset.setY(m_panStartOffset.y() - static_cast<double>(dy) / (m_zoomFactor * this->height()));
        clampPanOffset();
        showImage();
        return;
    }

    setMousePosition(ev->position().toPoint().x(), ev->position().toPoint().y());

    // Check if a pending press should become a drag
    if(m_bDragPending && !m_bDragging)
    {
        QPoint absCurrent = cvtRelativeToAbsolutePoint(m_relative_mouse_pos_in_ui);
        QPoint absStart   = cvtRelativeToAbsolutePoint(m_dragStartPos);
        int pixDist = (absCurrent - absStart).manhattanLength();
        if(pixDist > 5)
        {
            m_bDragging = true;
            saveState();
        }
    }

    // Move box while dragging
    if(m_bDragging && m_dragBoxIdx >= 0 && m_dragBoxIdx < m_objBoundingBoxes.size())
    {
        QRectF &box = m_objBoundingBoxes[m_dragBoxIdx].box;
        double newX = m_relative_mouse_pos_in_ui.x() - m_dragOffset.x();
        double newY = m_relative_mouse_pos_in_ui.y() - m_dragOffset.y();

        newX = std::max(0.0, std::min(newX, 1.0 - box.width()));
        newY = std::max(0.0, std::min(newY, 1.0 - box.height()));

        box.moveLeft(newX);
        box.moveTop(newY);
    }

    // Update cursor based on hover state
    if(!m_bLabelingStarted && !m_bDragging)
    {
        if(findBoxUnderCursor(m_relative_mouse_pos_in_ui) != -1)
            setCursor(Qt::OpenHandCursor);
        else
            setCursor(Qt::CrossCursor);
    }
    else if(m_bDragging)
    {
        setCursor(Qt::ClosedHandCursor);
    }

    showImage();
    emit Mouse_Moved();
}

void label_img::mousePressEvent(QMouseEvent *ev)
{
    setMousePosition(ev->position().toPoint().x(), ev->position().toPoint().y());

    if(ev->button() == Qt::LeftButton && (ev->modifiers() & Qt::AltModifier) && !m_bLabelingStarted)
    {
        setFocusedObjectBoxLabel(m_relative_mouse_pos_in_ui, m_focusedObjectLabel);
        showImage();
        emit Mouse_Pressed();
        return;
    }

    bool panStart = (m_zoomFactor > 1.0) &&
                    ((ev->button() == Qt::MiddleButton) ||
                     (ev->button() == Qt::LeftButton && (ev->modifiers() & Qt::ControlModifier)));
    if(panStart)
    {
        m_bPanning = true;
        m_panStartWidgetPos = ev->position().toPoint();
        m_panStartOffset = m_panOffset;
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if(ev->button() == Qt::RightButton)
    {
        removeFocusedObjectBox(m_relative_mouse_pos_in_ui);
        showImage();
    }
    else if(ev->button() == Qt::LeftButton)
    {
        if(m_bLabelingStarted == false)
        {
            int boxIdx = findBoxUnderCursor(m_relative_mouse_pos_in_ui);
            if(boxIdx != -1)
            {
                // Prepare for potential drag
                m_bDragPending = true;
                m_dragBoxIdx   = boxIdx;
                QRectF &box    = m_objBoundingBoxes[boxIdx].box;
                m_dragOffset   = QPointF(m_relative_mouse_pos_in_ui.x() - box.x(),
                                         m_relative_mouse_pos_in_ui.y() - box.y());
                m_dragStartPos = m_relative_mouse_pos_in_ui;
            }
            else
            {
                m_relatvie_mouse_pos_LBtnClicked_in_ui  = m_relative_mouse_pos_in_ui;
                m_bLabelingStarted                      = true;
                grabMouse();
            }
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
            {
                saveState();
                m_objBoundingBoxes.push_back(objBoundingbox);
            }

            releaseMouse();
            m_bLabelingStarted              = false;

            showImage();
        }
    }

    emit Mouse_Pressed();
}

void label_img::mouseReleaseEvent(QMouseEvent *ev)
{
    if(m_bPanning && (ev->button() == Qt::MiddleButton || ev->button() == Qt::LeftButton))
    {
        m_bPanning = false;
        if(findBoxUnderCursor(m_relative_mouse_pos_in_ui) != -1)
            setCursor(Qt::OpenHandCursor);
        else
            setCursor(Qt::CrossCursor);
    }
    else if(ev->button() == Qt::LeftButton)
    {
        if(m_bDragPending && !m_bDragging)
        {
            // Click on box without dragging -> start labeling from that point
            m_relatvie_mouse_pos_LBtnClicked_in_ui = m_relative_mouse_pos_in_ui;
            m_bLabelingStarted = true;
            grabMouse();
        }
        m_bDragging    = false;
        m_bDragPending = false;
        m_dragBoxIdx   = -1;

        if(findBoxUnderCursor(m_relative_mouse_pos_in_ui) != -1)
            setCursor(Qt::OpenHandCursor);
        else
            setCursor(Qt::CrossCursor);
    }
    emit Mouse_Release();
}

void label_img::init()
{
    m_objBoundingBoxes.clear();
    if (m_bLabelingStarted) releaseMouse();
    m_bLabelingStarted              = false;
    m_bDragging                     = false;
    m_bDragPending                  = false;
    m_dragBoxIdx                    = -1;
    m_focusedObjectLabel            = 0;
    m_zoomFactor                    = 1.0;
    m_panOffset                     = QPointF(0.0, 0.0);
    m_bPanning                      = false;

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

    if(x >= this->width())   x = this->width() - 1;
    if(y >= this->height())  y = this->height() - 1;

    m_relative_mouse_pos_in_ui = cvtAbsoluteToRelativePoint(QPoint(x, y));
}

void label_img::openImage(const QString &qstrImg, bool &ret)
{
    QImageReader imgReader(qstrImg);
    imgReader.setAllocationLimit(0);
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

        m_inputImg          = std::move(img);
        m_resized_inputImg  = m_inputImg.scaled(this->width(), this->height(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation)
                .convertToFormat(QImage::Format_RGB888);

        if (m_bLabelingStarted) releaseMouse();
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

    QImage img;

    if(m_zoomFactor <= 1.0)
    {
        if(m_resized_inputImg.width() != this->width() || m_resized_inputImg.height() != this->height())
        {
            m_resized_inputImg = m_inputImg.scaled(this->width(), this->height(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation)
                    .convertToFormat(QImage::Format_RGB888);
        }
        img = m_resized_inputImg;
    }
    else
    {
        int imgW = m_inputImg.width();
        int imgH = m_inputImg.height();

        int visX = static_cast<int>(m_panOffset.x() * imgW);
        int visY = static_cast<int>(m_panOffset.y() * imgH);
        int visW = static_cast<int>(imgW / m_zoomFactor);
        int visH = static_cast<int>(imgH / m_zoomFactor);

        visX = std::max(0, std::min(visX, imgW - 1));
        visY = std::max(0, std::min(visY, imgH - 1));
        visW = std::max(1, std::min(visW, imgW - visX));
        visH = std::max(1, std::min(visH, imgH - visY));

        QImage cropped = m_inputImg.copy(visX, visY, visW, visH);
        img = cropped.scaled(this->width(), this->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                .convertToFormat(QImage::Format_RGB888);
    }

    gammaTransform(img);

    QPainter painter(&img);
    QFont font = painter.font();
    int fontSize = 16, xMargin = 5, yMargin = 2;
    font.setPixelSize(fontSize);
    font.setBold(true);
    painter.setFont(font);

    int penThick = 3;

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
    ifstream inputFile(qPrintable(labelFilePath));

    if(inputFile.is_open())
    {
        double          inputFileValue;
        QVector<double> inputFileValues;

        while(inputFile >> inputFileValue)
            inputFileValues.push_back(inputFileValue);

        for(int i = 0; i < inputFileValues.size(); i += 5)
        {
            try {
                ObjectLabelingBox objBox;

                objBox.label = static_cast<int>(inputFileValues.at(i));
                if(objBox.label < 0 || objBox.label >= m_objList.size())
                    continue;

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

                m_objBoundingBoxes.push_back(objBox);
            }
            catch (const std::out_of_range&) {
            }
        }
    }
}

void label_img::setFocusObjectLabel(int nLabel)
{
    m_focusedObjectLabel = nLabel;
}

bool label_img::isOpened()
{
    return !m_inputImg.isNull();
}

QImage label_img::getInputImage() const
{
    return m_inputImg;
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
    int h = image.height();
    int w = image.width();
    int stride = image.bytesPerLine();

    for(int y = 0 ; y < h; ++y)
    {
        uchar* row = image.bits() + y * stride;
        for(int x = 0; x < w; ++x)
        {
            int offset = x * 3;
            row[offset + 0] = m_gammatransform_lut[row[offset + 0]];
            row[offset + 1] = m_gammatransform_lut[row[offset + 1]];
            row[offset + 2] = m_gammatransform_lut[row[offset + 2]];
        }
    }
}

bool label_img::removeFocusedObjectBox(QPointF point)
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
        saveState();
        m_objBoundingBoxes.removeAt(removeBoxIdx);
        return true;
    }
    return false;
}

void label_img::clearAllBoxes()
{
    saveState();
    m_objBoundingBoxes.clear();
}

void label_img::saveState()
{
    if(m_undoHistory.size() >= MAX_UNDO_HISTORY)
        m_undoHistory.removeFirst();
    m_undoHistory.append(m_objBoundingBoxes);
    m_redoHistory.clear();
}

bool label_img::undo()
{
    if(m_undoHistory.isEmpty())
        return false;
    m_redoHistory.append(m_objBoundingBoxes);
    m_objBoundingBoxes = m_undoHistory.takeLast();
    return true;
}

bool label_img::redo()
{
    if(m_redoHistory.isEmpty())
        return false;
    m_undoHistory.append(m_objBoundingBoxes);
    m_objBoundingBoxes = m_redoHistory.takeLast();
    return true;
}

void label_img::clearUndoHistory()
{
    m_undoHistory.clear();
    m_redoHistory.clear();
}

int label_img::findBoxUnderCursor(QPointF point) const
{
    int     foundIdx = -1;
    double  nearestBoxDistance = 99999999999999.;

    for(int i = 0; i < m_objBoundingBoxes.size(); i++)
    {
        QRectF objBox = m_objBoundingBoxes.at(i).box;
        if(objBox.contains(point))
        {
            double distance = objBox.width() + objBox.height();
            if(distance < nearestBoxDistance)
            {
                nearestBoxDistance = distance;
                foundIdx = i;
            }
        }
    }
    return foundIdx;
}

void label_img::moveBox(int boxIdx, double dx, double dy)
{
    if(boxIdx < 0 || boxIdx >= m_objBoundingBoxes.size())
        return;

    QRectF &box = m_objBoundingBoxes[boxIdx].box;
    double newX = box.x() + dx;
    double newY = box.y() + dy;

    newX = std::max(0.0, std::min(newX, 1.0 - box.width()));
    newY = std::max(0.0, std::min(newY, 1.0 - box.height()));

    box.moveLeft(newX);
    box.moveTop(newY);
    showImage();
}

void label_img::resizeBox(int boxIdx, double dw, double dh)
{
    if(boxIdx < 0 || boxIdx >= m_objBoundingBoxes.size())
        return;

    constexpr double MIN_BOX_DIM = 0.002;

    QRectF &box = m_objBoundingBoxes[boxIdx].box;
    double newW = box.width()  + dw;
    double newH = box.height() + dh;

    newW = std::max(MIN_BOX_DIM, newW);
    newH = std::max(MIN_BOX_DIM, newH);

    newW = std::min(newW, 1.0 - box.x());
    newH = std::min(newH, 1.0 - box.y());

    box.setWidth(newW);
    box.setHeight(newH);
    showImage();
}

void label_img::setFocusedObjectBoxLabel(QPointF point, int newLabel)
{
    int boxIdx = findBoxUnderCursor(point);

    if(boxIdx != -1 && newLabel >= 0 && newLabel < m_objList.size())
    {
        saveState();
        m_objBoundingBoxes[boxIdx].label = newLabel;
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
    QPoint topLeft = cvtRelativeToAbsolutePoint(rectF.topLeft());
    int w = static_cast<int>(rectF.width()  * m_zoomFactor * this->width()  + 0.5);
    int h = static_cast<int>(rectF.height() * m_zoomFactor * this->height() + 0.5);
    return QRect(topLeft.x(), topLeft.y(), w, h);
}

QPoint label_img::cvtRelativeToAbsolutePoint(QPointF p)
{
    double x = (p.x() - m_panOffset.x()) * m_zoomFactor * this->width();
    double y = (p.y() - m_panOffset.y()) * m_zoomFactor * this->height();
    return QPoint(static_cast<int>(x + 0.5), static_cast<int>(y + 0.5));
}

QPointF label_img::cvtAbsoluteToRelativePoint(QPoint p)
{
    double denom = m_zoomFactor * this->width();
    if(denom <= 0.0 || this->height() <= 0)
        return QPointF(0., 0.);
    double x = m_panOffset.x() + static_cast<double>(p.x()) / denom;
    double y = m_panOffset.y() + static_cast<double>(p.y()) / (m_zoomFactor * this->height());
    return QPointF(x, y);
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

void label_img::wheelEvent(QWheelEvent *ev)
{
    if(ev->modifiers() & Qt::ControlModifier)
    {
        if(ev->angleDelta().y() > 0)
            zoomIn(ev->position().toPoint());
        else if(ev->angleDelta().y() < 0)
            zoomOut(ev->position().toPoint());
        ev->accept();
    }
    else
    {
        ev->ignore();
    }
}

void label_img::zoomIn(QPoint widgetPos)
{
    if(m_inputImg.isNull()) return;

    QPointF relPos = cvtAbsoluteToRelativePoint(widgetPos);

    m_zoomFactor = std::min(m_zoomFactor * 1.05, 10.0);

    m_panOffset.setX(relPos.x() - static_cast<double>(widgetPos.x()) / (m_zoomFactor * this->width()));
    m_panOffset.setY(relPos.y() - static_cast<double>(widgetPos.y()) / (m_zoomFactor * this->height()));

    clampPanOffset();
    showImage();
}

void label_img::zoomOut(QPoint widgetPos)
{
    if(m_inputImg.isNull()) return;

    QPointF relPos = cvtAbsoluteToRelativePoint(widgetPos);

    m_zoomFactor = std::max(m_zoomFactor / 1.05, 1.0);

    if(m_zoomFactor <= 1.0)
    {
        m_zoomFactor = 1.0;
        m_panOffset = QPointF(0.0, 0.0);
    }
    else
    {
        m_panOffset.setX(relPos.x() - static_cast<double>(widgetPos.x()) / (m_zoomFactor * this->width()));
        m_panOffset.setY(relPos.y() - static_cast<double>(widgetPos.y()) / (m_zoomFactor * this->height()));
        clampPanOffset();
    }

    showImage();
}

void label_img::resetZoom()
{
    m_zoomFactor = 1.0;
    m_panOffset = QPointF(0.0, 0.0);
    showImage();
}

void label_img::clampPanOffset()
{
    double maxPanX = std::max(0.0, 1.0 - 1.0 / m_zoomFactor);
    double maxPanY = std::max(0.0, 1.0 - 1.0 / m_zoomFactor);
    m_panOffset.setX(std::max(0.0, std::min(m_panOffset.x(), maxPanX)));
    m_panOffset.setY(std::max(0.0, std::min(m_panOffset.y(), maxPanY)));
}

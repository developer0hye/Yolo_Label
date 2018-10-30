#include "label_img.h"
#include <QPainter>

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
    :QLabel(parent),
    m_bMouseLeftButtonClicked(false),
    m_focusedObjectLabel(0)

{
}

void label_img::mouseMoveEvent(QMouseEvent *ev)
{
    std::cout<< "moved"<< std::endl;

    setMousePosition(ev->x(), ev->y());

    showImage();
    emit Mouse_Moved();
}

void label_img::mousePressEvent(QMouseEvent *ev)
{
    std::cout<< "clicked"<< std::endl;

    setMousePosition(ev->x(), ev->y());

    if(ev->button() == Qt::RightButton)
    {
        removeFocusedObjectBox(m_mouse_pos_in_image_coordinate);
        showImage();
    }
    else if(ev->button() == Qt::LeftButton)
    {
       m_bMouseLeftButtonClicked = true;
       m_leftButtonClickedPoint = m_mouse_pos_in_image_coordinate;
    }

    emit Mouse_Pressed();
}

void label_img::mouseReleaseEvent(QMouseEvent *ev)
{
    std::cout<< "released"<< std::endl;

    if(ev->button() == Qt::LeftButton && m_bMouseLeftButtonClicked == true)
    {
        setMousePosition(ev->x(), ev->y());

        ObjectLabelingBox objBoundingbox;

        objBoundingbox.label    = m_focusedObjectLabel;
        objBoundingbox.box      = getRectFromTwoPoints(m_mouse_pos_in_image_coordinate, m_leftButtonClickedPoint);

        bool width_is_too_small     = objBoundingbox.box.width()    < m_inputImg.width() * 0.01;
        bool height_is_too_small    = objBoundingbox.box.height()   < m_inputImg.height() * 0.01;

        if(!width_is_too_small && !height_is_too_small)
            m_objBoundingBoxes.push_back(objBoundingbox);

        showImage();

        m_bMouseLeftButtonClicked = false;
    }
    emit Mouse_Release();
}

void label_img::setMousePosition(int x, int y)
{
    if(x < 0) x = 0;
    if(y < 0) y = 0;

    if(x > this->width())   x = this->width() - 1;
    if(y > this->height())  y = this->height() - 1;

    m_mouse_pos_in_image_coordinate = QPoint(static_cast<int>(x * m_aspectRatioWidth),
                                             static_cast<int>(y * m_aspectRatioHeight));
}

void label_img::openImage(const QString &qstrImg)
{
    m_inputImg      = QImage(qstrImg);
    m_inputImg      = m_inputImg.convertToFormat(QImage::Format_RGB888);
    m_inputImgCopy  = m_inputImg;

    m_aspectRatioWidth  = static_cast<double>(m_inputImg.width()) / this->width();
    m_aspectRatioHeight = static_cast<double>(m_inputImg.height()) / this->height();

    m_objBoundingBoxes.clear();

    m_leftButtonClickedPoint = QPoint();

    m_bMouseLeftButtonClicked = false;

    QPoint mousePosInUi = this->mapFromGlobal(QCursor::pos());
    bool mouse_is_in_image = QRect(0, 0, this->width(), this->height()).contains(mousePosInUi);

    if  (mouse_is_in_image)
    {
        setMousePosition(mousePosInUi.x(), mousePosInUi.y());
    }
    else
    {
        m_mouse_pos_in_image_coordinate = QPoint();
    }
}

void label_img::showImage()
{
    if(m_inputImg.isNull()) return;

    m_inputImg = m_inputImgCopy;

    QPainter painter(&m_inputImg);

    int penThick = (m_inputImg.width() + m_inputImg.height())/400;

    QColor crossLineColor(255, 187, 0);

    drawCrossLine(painter, crossLineColor, penThick);
    drawFocusedObjectBox(painter, Qt::magenta, penThick);
    drawObjectBoxes(painter, penThick);

    this->setPixmap(QPixmap::fromImage(m_inputImg));
}

void label_img::loadLabelData(const QString& labelFilePath)
{
    ifstream inputFile(labelFilePath.toStdString());

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

                double midX = inputFileValues.at(i + 1) * m_inputImg.width();
                double midY = inputFileValues.at(i + 2) * m_inputImg.height();
                double width = inputFileValues.at(i + 3) * m_inputImg.width();
                double height = inputFileValues.at(i + 4) * m_inputImg.height();

                objBox.box.setX(static_cast<int>(midX - width/2));
                objBox.box.setY(static_cast<int>(midY - height/2));
                objBox.box.setWidth(static_cast<int>(width));
                objBox.box.setHeight(static_cast<int>(height));

                m_objBoundingBoxes.push_back(objBox);
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

void label_img::drawCrossLine(QPainter& painter, QColor color, int thickWidth)
{
    if(m_mouse_pos_in_image_coordinate == QPoint()) return;

    QPen pen;
    pen.setWidth(thickWidth);

    pen.setColor(color);
    painter.setPen(pen);

    //draw cross line
    painter.drawLine(QPoint(m_mouse_pos_in_image_coordinate.x(),0), QPoint(m_mouse_pos_in_image_coordinate.x(),m_inputImg.height() - 1));
    painter.drawLine(QPoint(0,m_mouse_pos_in_image_coordinate.y()), QPoint(m_inputImg.width() - 1, m_mouse_pos_in_image_coordinate.y()));

    std::cout << "draw Cross Line" << std::endl;
}

void label_img::drawFocusedObjectBox(QPainter& painter, Qt::GlobalColor color, int thickWidth)
{
    if(m_bMouseLeftButtonClicked)
    {
        QPen pen;
        pen.setWidth(thickWidth);

        pen.setColor(color);
        painter.setPen(pen);
        painter.drawRect(QRect(m_leftButtonClickedPoint, m_mouse_pos_in_image_coordinate));
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

        painter.drawRect(boundingbox.box);
    }
}

void label_img::removeFocusedObjectBox(QPoint point)
{
    int     removeBoxIdx = -1;
    double  nearestBoxDistance   = 99999999999999.;

    for(int i = 0; i < m_objBoundingBoxes.size(); i++)
    {
        QRect objBox = m_objBoundingBoxes.at(i).box;

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

QRect label_img::getRectFromTwoPoints(QPoint p1, QPoint p2)
{
    int midX    = (p1.x() + p2.x()) / 2;
    int midY    = (p1.y() + p2.y()) / 2;
    int width   = abs(p1.x() - p2.x());
    int height  = abs(p1.y() - p2.y());

    QPoint topLeftPoint(midX - width/2, midY - height/2);
    QPoint bottomRightPoint(midX + width/2, midY + height/2);

    return QRect(topLeftPoint, bottomRightPoint);
}

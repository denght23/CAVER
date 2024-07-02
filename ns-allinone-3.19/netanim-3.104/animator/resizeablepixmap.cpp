/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: John Abraham <john.abraham@gatech.edu>
 */

#include "animator/resizeablepixmap.h"

#include <QGraphicsSceneMouseEvent>
#include <QCursor>

#define NS_LOG_FUNCTION(x)
#define NS_LOG_DEBUG(x)



namespace netanim {

ResizeablePixmap::ResizeablePixmap(QPixmap pix):
    QGraphicsPixmapItem(pix),
    m_mousePressed(false),
    m_currentResizeDirection(RESIZE_NOTRESIZING),
    m_lastResizeDirection(RESIZE_NOTRESIZING)
{
    NS_LOG_FUNCTION(m_mousePressed);
    setAcceptsHoverEvents(true);
}

void ResizeablePixmap::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    //qDebug("Mouse pressed");
    m_mousePressed = true;
    prepareGeometryChange();
    QGraphicsPixmapItem::mousePressEvent(event);
}

bool ResizeablePixmap::isResizing()
{
    return (m_currentResizeDirection == m_lastResizeDirection);
}
void ResizeablePixmap::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    //NS_LOG_FUNCTION("Event Pos: " << event->pos() << " SB Rect:" << sceneBoundingRect());
    QGraphicsPixmapItem::mouseMoveEvent(event);
    NS_LOG_DEBUG("POS:" << pos() << " EventPos:" << event->pos() << sceneBoundingRect().topLeft());
    if (!m_mousePressed || !isResizing())
        return;
    qreal eventPosX = event->pos().x();
    qreal eventPosY = event->pos().y();
    if (m_currentResizeDirection == RESIZE_RIGHT)
    {
        qreal xScale = eventPosX/getItemWidth();
        scale(xScale, 1);
    }
    if (m_currentResizeDirection == RESIZE_LEFT)
    {
        qreal xScale = (getItemWidth() - eventPosX)/getItemWidth();
        qreal savedY = pos().y();
        scale(xScale, 1);
        setPos(QPointF(mapToScene(event->pos())).x(), savedY );
    }
    if (m_currentResizeDirection == RESIZE_TOP)
    {
        qreal yScale = (getItemHeight()- eventPosY)/getItemHeight();
        qreal savedX = pos().x();
        scale(1, yScale);
        setPos(savedX, QPointF(mapToScene(event->pos())).y());
    }
    if (m_currentResizeDirection == RESIZE_BOTTOM)
    {
        qreal yScale = eventPosY/getItemHeight();
        scale(1, yScale);
    }
    prepareGeometryChange();
}


qreal ResizeablePixmap::getItemWidth()
{
    //NS_LOG_FUNCTION_NOARGS();
    QPointF sceneLeft = sceneBoundingRect().topLeft();
    QPointF sceneRight = sceneBoundingRect().topRight();
    QPointF itemMapLeft = mapFromScene(sceneLeft);
    QPointF itemMapRight = mapFromScene(sceneRight);
    qreal w = itemMapRight.x() - itemMapLeft.x();
    NS_LOG_DEBUG ("ItemWidth:" << w);
    return w;

}

qreal ResizeablePixmap::getItemHeight()
{
    QPointF sceneTop = sceneBoundingRect().topLeft();
    QPointF sceneBottom = sceneBoundingRect().bottomLeft();
    QPointF itemMapTop = mapFromScene(sceneTop);
    QPointF itemMapBottom = mapFromScene(sceneBottom);
    qreal h = itemMapBottom.y() - itemMapTop.y();
    NS_LOG_DEBUG ("ItemHeight:" << h);
    return h;
}

void ResizeablePixmap::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    //qDebug("Mouse released");
    m_mousePressed = false;
    QGraphicsPixmapItem::mouseReleaseEvent(event);
}

void ResizeablePixmap::setResizingDirection(ResizeDirection_t direction)
{
    QCursor c;
    switch (direction)
    {

        case RESIZE_BOTTOM:
        case RESIZE_TOP:
            c.setShape(Qt::SizeVerCursor);
            setFlags(QGraphicsItem::ItemIsSelectable);
            break;
        case RESIZE_LEFT:
        case RESIZE_RIGHT:
            c.setShape(Qt::SizeHorCursor);
            setFlags(QGraphicsItem::ItemIsSelectable);
            break;
        case RESIZE_NOTRESIZING:
            c.setShape(Qt::OpenHandCursor);
            setFlags(QGraphicsItem::ItemIsMovable|QGraphicsItem::ItemIsSelectable);


    }
    setCursor(c);
    m_currentResizeDirection = direction;
}

void ResizeablePixmap::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{

    qreal borderWidth =   getItemWidth()/PIXMAP_RESIZING_BORDER;
    qreal bottomRightX = boundingRect().bottomRight().x();
    qreal bottomRightXLow = bottomRightX - borderWidth;

    qreal bottomRightY = boundingRect().bottomRight().y();
    qreal bottomRightYLow = bottomRightY - borderWidth;

    qreal eventPosX = ((event->pos())).x();
    qreal eventPosY = ((event->pos())).y();
    if (((eventPosX >= bottomRightXLow) && (eventPosX <= bottomRightX)))
    {
        setResizingDirection(RESIZE_RIGHT);
    }
    else if (eventPosX <= borderWidth)
    {
        setResizingDirection(RESIZE_LEFT);
    }
    else if (((eventPosY >= bottomRightYLow) && (eventPosY <= bottomRightY)))
    {
        setResizingDirection(RESIZE_BOTTOM);
    }
    else if (eventPosY <= borderWidth)
    {
        setResizingDirection(RESIZE_TOP);
    }
    else
    {
        setResizingDirection(RESIZE_NOTRESIZING);
    }
    QGraphicsPixmapItem::hoverMoveEvent(event);
    //QDEBUG("Current Resize Direction:" + QString::number(m_currentResizeDirection) + " Last Resize:" + QString::number(m_lastResizeDirection));

    m_lastResizeDirection = m_currentResizeDirection;
}

QRectF ResizeablePixmap::boundingRect() const
{
    return QGraphicsPixmapItem::boundingRect();
}

void ResizeablePixmap::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    //NS_LOG_DEBUG(sceneBoundingRect());
    return QGraphicsPixmapItem::paint(painter, option, widget);

}


} // namespace netanim

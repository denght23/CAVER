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


#ifndef RESIZEABLE_PIXMAP_H
#define RESIZEABLE_PIXMAP_H

#include <QGraphicsPixmapItem>

#define PIXMAP_RESIZING_BORDER 3

namespace netanim {
class ResizeablePixmap : public QGraphicsPixmapItem
{

public:
    typedef enum {
        RESIZE_RIGHT,
        RESIZE_LEFT,
        RESIZE_TOP,
        RESIZE_BOTTOM,
        RESIZE_NOTRESIZING
    } ResizeDirection_t;
    ResizeablePixmap(QPixmap pix);
    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    qreal getItemWidth();
    qreal getItemHeight();

private:
    bool m_mousePressed;
    ResizeDirection_t m_currentResizeDirection;
    ResizeDirection_t m_lastResizeDirection;
    bool isResizing();
    void setResizingDirection(ResizeDirection_t direction);

};

} // namespace netanim
#endif // RESIZEABLE_PIXMAP_H


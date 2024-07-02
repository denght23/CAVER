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
 * Contributions: Eugene Kalishenko <ydginster@gmail.com> (Open Source and Linux Laboratory http://dev.osll.ru/)
 */

#include "animator/animatorconstants.h"
#include "animatorscene.h"
#include "animnode.h"
#include "debug/xdebug.h"
#include <QtGui/QGraphicsEllipseItem>
#include <QtGui/QGraphicsRectItem>
#include <QtGui/QBrush>
#include <QtGui/QPen>
#include <QDir>

#include <iostream>

namespace netanim {

AnimNodeMgr * AnimNodeMgr::pAnimNodeMgr = 0;

AnimNodeEnergySource::AnimNodeEnergySource(EnergySourceType type, qreal capacity):
    m_type(type)
{
  setResidualCapacity(capacity);
}

void AnimNodeEnergySource::setResidualCapacity(qreal capacity)
{
  Q_ASSERT(capacity >=0 && capacity <= 1);

  m_capacity = capacity;
}

AnimNodeEnergySource::EnergySourceType
AnimNodeEnergySource::getType() const
{
    return m_type;
}

qreal
AnimNodeEnergySource::getResidualCapacity() const
{
    return m_capacity;
}

AnimNode::AnimNode(uint32_t nodeId,
                   AnimNodeShape shape,
                   qreal width,
                   qreal height,
                   QString description,
                   QColor * color,
                   const AnimNodeEnergySource& energySource) :
    m_nodeId(nodeId),
    m_shape(shape),
    m_width(width),
    m_height(height),
    m_description(description),
    m_batteryItem(NULL),
    m_color(color),
    m_graphicsNodeIdTextItem(0),
    m_routePathMarked(false),
    m_routePathSource(false),
    m_routePathDestination(false),
    m_energySource(energySource)
{
    switch (shape)
    {
        case CIRCLE:
        m_graphicsItem = new AnimNodeEllipse;
            break;
        case RECTANGLE:
        m_graphicsItem = new QGraphicsRectItem;
            break;
        case IMAGE:
            m_graphicsItem = new QGraphicsPixmapItem;
        break;
        default:
            m_graphicsItem = 0;
            // TODO
        break;
    }
    QString nodeDescription;
    if(m_description != "")
    {
        nodeDescription = m_description;
    }
    else
    {
        nodeDescription = QString::number(m_nodeId);
    }
    m_graphicsNodeIdTextItem = new QGraphicsSimpleTextItem(nodeDescription);
    m_graphicsNodeIdTextItem->setFlag(QGraphicsItem::ItemIgnoresTransformations);

    m_visible = true;
    m_graphicsItem->setZValue(ANIMNODE_ELLIPSE_TYPE);
    m_graphicsNodeIdTextItem->setZValue(ANIMNODE_ID_TYPE);
    updateBatteryCapacityImage();
}


AnimNode::~AnimNode()
{
    if(m_graphicsItem)
        delete m_graphicsItem;
    if(m_graphicsNodeIdTextItem)
        delete m_graphicsNodeIdTextItem;
    if(m_color)
        delete m_color;
    if(m_batteryItem)
        delete m_batteryItem;
}

void
AnimNode::markRoutePath(bool mark)
{
    m_routePathMarked = mark;
    setSize(m_width);
    setDescription(m_description);
}

void
AnimNode::setRoutePathSource(bool source)
{
    m_routePathSource = source;
}
void
AnimNode::setRouteDestination(bool destination)
{
    m_routePathDestination = destination;
}

void
AnimNode::addIpv4Address(QString ip)
{
    m_ipv4Vector.push_back(ip);
}

void
AnimNode::addMacAddress(QString mac)
{
    m_macVector.push_back(mac);
}

bool
AnimNode::hasIpv4(QString ip)
{
    bool result = false;
    QStringList quads = ip.split(".");
    if(quads.count() == 4)
    {
        if(quads.at(3) == "255")
            return true;
        for(Ipv4Vector_t::const_iterator i = m_ipv4Vector.begin();
            i != m_ipv4Vector.end();
            ++i)
        {
            if(*i == ip)
            {
                //QDEBUG(ip);
                return true;
            }
        }
    }

    return result;
}


bool
AnimNode::hasMac(QString mac)
{
    bool result = false;
    QStringList bytes = mac.split(":");
    if(bytes.count() == 6)
    {
        for(MacVector_t::const_iterator i = m_macVector.begin();
            i != m_macVector.end();
            ++i)
        {
            if(*i == mac)
            {
                return true;
            }
        }
    }

    return result;
}

QGraphicsItem *
AnimNode::getGraphicsItem()
{
    return m_graphicsItem;
}

QGraphicsItem *
AnimNode::getBatteryItem() const
{
    return m_batteryItem;
}

void
AnimNode::setRect(QPointF pos)
{

    //qDebug(pos,"SetRect");
    QBrush brush;
    if (!m_color)
    {
        brush.setColor(Qt::red);
    }
    else
    {
        brush.setColor(*m_color);
    }
    if(m_routePathMarked)
    {
        brush.setColor(Qt::blue);
    }
    brush.setStyle(Qt::SolidPattern);
    QPen pen;
    pen.setColor(Qt::red);
    qreal sizeMultiplier = m_routePathMarked? 2:1;
    switch(m_shape)
    {
        case CIRCLE:
        {
            AnimNodeEllipse * g = qgraphicsitem_cast<AnimNodeEllipse*> (m_graphicsItem);
            g->setRect(pos.x()-(m_width * sizeMultiplier)/2, pos.y()-(sizeMultiplier * m_height)/2, m_width * sizeMultiplier, m_height * sizeMultiplier);
            g->setBrush(brush);
            g->setPen(pen);

            break;
        }
        case RECTANGLE:
        {
            QGraphicsRectItem * g = qgraphicsitem_cast<QGraphicsRectItem*> (m_graphicsItem);
            g->setRect(pos.x(), pos.y(), m_width * sizeMultiplier , m_height * sizeMultiplier);
            g->setBrush(brush);
            break;
        }
        case IMAGE:
        break;
    }

    updateBatteryCapacityImage();
}


uint32_t
AnimNode::getNodeId()
{
    return m_nodeId;
}

void
AnimNode::setSize(qreal size)
{
    m_width = m_height = size;
    //qDebug("SetSize");
    setRect(m_graphicsItem->sceneBoundingRect().center());
}

qreal
AnimNode::getSize()
{
    return m_width;
}

AnimNodeShape
AnimNode::getNodeShape()
{
    return m_shape;
}


void
AnimNode::setPos(QPointF pos)
{
    QPointF invertedPos = QPointF (pos.x(), AnimatorScene::getInstance()->getHeight() - pos.y());
    setRect(invertedPos);
    //setRect(pos);
    //m_graphicsNodeIdTextItem->setPos(m_graphicsItem->sceneBoundingRect().bottomRight());
    m_graphicsNodeIdTextItem->setPos(m_graphicsItem->sceneBoundingRect().bottomRight());
    updateBatteryCapacityImagePosition();
}

QGraphicsSimpleTextItem *
AnimNode::getNodeIdTextItem()
{
    return m_graphicsNodeIdTextItem;
}


void
AnimNode::updateNode(QColor c, QString description, bool visible, bool skipColor, const AnimNodeEnergySource& energySource)
{
    if(!skipColor)
    {
        setColor(c.red(), c.green(), c.blue());
    }
    m_description = description;
    if(m_description == "")
    {
        m_description = QString::number(m_nodeId);
    }
    m_graphicsNodeIdTextItem->setText(m_description);

    m_visible = visible;
    m_energySource = energySource;
    updateBatteryCapacityImage();
    setVisible(m_visible);
}


void
AnimNode::showNodeIdText(bool show)
{
    m_graphicsNodeIdTextItem->setPos(m_graphicsItem->sceneBoundingRect().bottomRight());
    m_graphicsNodeIdTextItem->setVisible(show);
    if(show == true && m_visible == false)
    {
        m_graphicsNodeIdTextItem->setVisible(m_visible);
    }
}

void
AnimNode::showBatteryCapacity(bool show)
{
    if(!m_batteryItem)
        return;

    updateBatteryCapacityImagePosition();
    m_batteryItem->setVisible(show && m_visible);
}

void
AnimNode::setColor(uint8_t r, uint8_t g, uint8_t b)
{
    QColor * c = new QColor(r, g, b);
    if (m_color)
    {
        delete m_color;
    }
    m_color = c;
}

void
AnimNode::setDescription(QString description)
{
    if(m_routePathMarked)
    {
        QFont f = m_graphicsNodeIdTextItem->font();
        f.setBold(true);
        m_graphicsNodeIdTextItem->setFont(f);
        if(m_routePathSource)
        {
            m_graphicsNodeIdTextItem->setText("SOURCE");
            return;
        }
        if(m_routePathDestination)
        {
            m_graphicsNodeIdTextItem->setText("DESTINATION");
            return;
        }
    }
    m_description = description;
    m_graphicsNodeIdTextItem->setText(description);
}

void
AnimNode::setVisible(bool visible)
{
    m_graphicsItem->setVisible(visible);
    m_graphicsNodeIdTextItem->setVisible(visible);

    if(visible == true && m_visible == false)
    {
        m_graphicsItem->setVisible(m_visible);
        m_graphicsNodeIdTextItem->setVisible(m_visible);
    }

}

AnimNodeEnergySource
AnimNode::getEnergySource() const
{
    return m_energySource;
}

void
AnimNode::updateBatteryCapacityImage()
{
    if(m_energySource.getType() != AnimNodeEnergySource::BATTERY)
        return;

    if(!m_batteryItem)
    {
        m_batteryItem = new QGraphicsPixmapItem();
        m_batteryItem->setZValue(ANIMNODE_BATTERY_TYPE);
    }

    QString batteryCapacityImagePath(":/animator_resource/battery_icon_");
    const qreal capacity = m_energySource.getResidualCapacity();

    if(capacity > 0.75) batteryCapacityImagePath += "4";
    else if(capacity > 0.5) batteryCapacityImagePath += "3";
    else if(capacity > 0.25) batteryCapacityImagePath += "2";
    else if(capacity >= 0) batteryCapacityImagePath += "1";
    else batteryCapacityImagePath += "0";

    batteryCapacityImagePath += ".png";
    m_batteryItem->setPixmap(QPixmap(batteryCapacityImagePath));

    const QGraphicsScene* scene = AnimatorScene::getInstance();

    if(scene)
        m_batteryItem->setScale(m_height / scene->height() / 3.);
}

void
AnimNode::updateBatteryCapacityImagePosition()
{
    if(!m_batteryItem)
        return;

    const QPointF bottomLeft = m_graphicsItem->sceneBoundingRect().bottomLeft();

    m_batteryItem->setPos(bottomLeft.x() - m_batteryItem->sceneBoundingRect().width(), bottomLeft.y());
}

AnimNode *
AnimNodeMgr::getNode(uint32_t nodeId)
{
    AnimNode * aNode = 0;
    if (m_animNodes.find(nodeId) != m_animNodes.end())
    {
        aNode = m_animNodes[nodeId];
    }
    return aNode;
}

AnimNode *
AnimNodeMgr::addNode(uint32_t nodeId,
                     AnimNodeShape shape,
                     qreal width,
                     qreal height,
                     QString description,
                     QColor *color,
                     bool * addToScene,
                     const AnimNodeEnergySource& energySource)
{
    AnimNode * aNode = 0;
    //qDebug(QString("Adding node id:") + QString::number(nodeId));
    if(m_animNodes.find(nodeId) == m_animNodes.end())
    {
        aNode = new AnimNode(nodeId,
                             shape,
                             width,
                             height,
                             description,
                             color,
                             energySource);

        m_animNodes[nodeId] = aNode;
        *addToScene = true;

    }
    else
    {
        aNode = m_animNodes[nodeId];
        *addToScene = false;
    }
    return aNode;
}

AnimNodeMgr *
AnimNodeMgr::getInstance()
{
    if(!pAnimNodeMgr)
    {
        pAnimNodeMgr = new AnimNodeMgr;
    }
    return pAnimNodeMgr;
}

AnimNodeMgr::AnimNodeMgr()
{

}

uint32_t
AnimNodeMgr::getNodeCount()
{
    return m_animNodes.size();
}

bool
AnimNodeMgr::isEmpty()
{
    return m_animNodes.empty();
}

AnimNodeMgr::AnimNodeMap_t *
AnimNodeMgr::getAnimNodes()
{
    return &m_animNodes;
}


void
AnimNodeMgr::addIpv4Address(uint32_t nodeId, QString ip)
{
    getNode(nodeId)->addIpv4Address(ip);
}

void
AnimNodeMgr::addMacAddress(uint32_t nodeId, QString mac)
{
    getNode(nodeId)->addMacAddress(mac);
}



void
AnimNodeMgr::systemReset()
{
    for (AnimNodeMgr::AnimNodeMap_t::const_iterator i = m_animNodes.begin();
         i != m_animNodes.end();
         ++i)
    {

        AnimatorScene::getInstance()->removeItem(i->second->getGraphicsItem());
        delete (i->second);
    }
    m_animNodes.clear();
}

} // namespace netanim

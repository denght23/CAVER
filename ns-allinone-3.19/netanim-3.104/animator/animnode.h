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

#ifndef ANIMNODE_H
#define ANIMNODE_H

#include "main/common.h"
#include "animator/animatorconstants.h"
#include <QtGui/QGraphicsItem>
#include <QColor>

namespace netanim {

enum AnimNodeShape
{
    CIRCLE,
    RECTANGLE,
    IMAGE
};

class AnimNodeEllipse : public QGraphicsEllipseItem
{
public:
    enum { Type = ANIMNODE_ELLIPSE_TYPE };
    int type () const
    {
        return Type;
    }
};

class AnimNodeEnergySource
{
public:
    enum EnergySourceType
    {
      BATTERY, //!< Internal power source, typical for mobile nodes
      GRID //!< Stationary power source
    };

    AnimNodeEnergySource(EnergySourceType type = GRID, qreal capacity = 0);

    void setResidualCapacity(qreal capacity);

    /*!
     * \brief Get current battery capacity
     * \return Number between 0 and 1 like persent of charge
     */
    qreal getResidualCapacity() const;

    EnergySourceType getType() const;

private:

    EnergySourceType m_type;
    qreal m_capacity;
};

class AnimNode
{
public:
    explicit AnimNode(uint32_t nodeId,
                      AnimNodeShape shape,
                      qreal width = 1,
                      qreal height = 1,
                      QString description ="",
                      QColor * color = 0,
                      const AnimNodeEnergySource& energySource = AnimNodeEnergySource());
    ~AnimNode();

    QGraphicsItem * getGraphicsItem();
    QGraphicsItem * getBatteryItem() const;
    AnimNodeShape getNodeShape();
    void setPos(QPointF pos);
    void setSize(qreal size);
    uint32_t getNodeId();
    qreal getSize();
    AnimNodeEnergySource getEnergySource() const;
    QGraphicsSimpleTextItem * getNodeIdTextItem();
    void showNodeIdText(bool show);
    void showBatteryCapacity(bool show);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void setDescription(QString description);
    void setVisible(bool visible=true);
    void updateNode(QColor c, QString description, bool visible, bool skipColor, const AnimNodeEnergySource& energySource);
    void addIpv4Address(QString ip);
    void addMacAddress(QString mac);
    bool hasIpv4(QString ip);
    bool hasMac(QString mac);
    void markRoutePath(bool mark);
    void setRoutePathSource(bool source);
    void setRouteDestination(bool destination);
private:
    typedef QVector <QString> Ipv4Vector_t;
    typedef QVector <QString> MacVector_t;

    /*!
     * \brief Synchronize battery image with current battery capacity
     */
    void updateBatteryCapacityImage();

    /*!
     * \brief Synchronize battery image position with current node image
     */
    void updateBatteryCapacityImagePosition();

    uint32_t m_nodeId;
    AnimNodeShape m_shape;
    qreal m_width;
    qreal m_height;
    QString m_description;
    QGraphicsItem * m_graphicsItem;
    QGraphicsPixmapItem * m_batteryItem; //!< Battery image
    QColor * m_color;
    QGraphicsSimpleTextItem * m_graphicsNodeIdTextItem;
    bool m_visible;
    Ipv4Vector_t m_ipv4Vector;
    MacVector_t m_macVector;
    bool m_routePathMarked;
    bool m_routePathSource;
    bool m_routePathDestination;
    AnimNodeEnergySource m_energySource; //!< Residual battery capacity and source type

    void setRect(QPointF pos);

    
signals:
    
public slots:
    
};

class AnimNodeMgr
{
public:
    static AnimNodeMgr * getInstance();
    typedef std::map <uint32_t, AnimNode *> AnimNodeMap_t;
    uint32_t getNodeCount();
    bool isEmpty();
    AnimNodeMap_t * getAnimNodes();
    AnimNode * addNode(uint32_t nodeId,
                      AnimNodeShape shape,
                      qreal width,
                      qreal height,
                      QString description,
                      QColor * color,
                      bool * addToScene,
                      const AnimNodeEnergySource& energySource);
    AnimNode * getNode(uint32_t nodeId);
    void addIpv4Address(uint32_t nodeId, QString ip);
    void addMacAddress(uint32_t nodeId, QString mac);
    void systemReset();
private:
    AnimNodeMgr();
    static AnimNodeMgr * pAnimNodeMgr;
    AnimNodeMap_t m_animNodes;

};


} // namespace netanim
#endif // ANIMNODE_H

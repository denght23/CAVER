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


#ifndef ANIMATORSCENE_H
#define ANIMATORSCENE_H

#include "main/common.h"
#include "animnode.h"
#include "animpacket.h"
#include "mobilitymanager.h"
#include "animxmlparser.h"
#include "packetstatisticsdialog.h"
#include "nodepositionstatisticsdialog.h"
#include "nodetrajectorydialog.h"
#include "animlink.h"
#include "linkupdatemanager.h"
#include "nodeupdatemanager.h"
#include "statistics/routingstatsscene.h"

#include <QGraphicsScene>
#include <QtGui/QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QProgressBar>
#include <QTimer>
#include <QCheckBox>
#include <QPainterPath>
#include <QGraphicsPathItem>
#include <QListWidget>
#include <QPushButton>

namespace netanim {

#define DEFAULT_SCENE_WIDTH 1024
#define DEFAULT_SCENE_HEIGHT 1024

struct WirelessCircleDimensions
{
    QPointF center;
    qreal radius;
};



class AnimInterfaceText : public QGraphicsTextItem
{
public:
    typedef enum textMode {
        NONE,
        IPV4,
        MAC,
        BOTH
    } TextMode_t;

    AnimInterfaceText(QString description, bool leftAligned=false);
    ~AnimInterfaceText();
    enum { Type = ANIMINTERFACE_TEXT_TYPE };
    int type () const
    {
        return Type;
    }
    QPainterPath shape() const;
    bool setLine(QLineF l);
    QGraphicsLineItem * getLine();
    void setMode(bool showIpv4, bool showMac);
    QString getText() const;
    void setLeftAligned(bool leftAligned);

private:
    bool m_leftAligned;
    QGraphicsLineItem * m_line;
    TextMode_t m_mode;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};


class RoutePathManager
{
public:
    static RoutePathManager * getInstance();
    void systemReset();
    void update(qreal currentTime);
private:
    typedef std::vector <AnimNode *> MarkedNodeVector_t;
    typedef std::vector <QGraphicsLineItem *> RoutePathLineVector_t;
    QGraphicsProxyWidget * m_routeLabelProxyWidget;
    QLabel * m_routeLabel;

    RoutePathManager();
    void add(RoutePath_t rp);
    void remove();
    MarkedNodeVector_t m_markedNodes;
    RoutePathLineVector_t m_routePathLines;

};

class AnimatorScene : public QGraphicsScene
{
Q_OBJECT

public:

    // Getters

    static AnimatorScene * getInstance();
    qreal getWidth();
    qreal getHeight();
    uint32_t getNodeCount();
    QRectF getSceneRect();
    NodeTrajectoryDialog * getNodeTrajectoryDialog();
    QVector <QGraphicsSimpleTextItem *> getGridCoordinatesItems();
    uint32_t getAnimatedPacketCount();
    QPointF getNodeCenter(uint32_t nodeId);
    bool isRoutePathTracking();
    qreal getNextPacketTime();
    qreal getNextPacketTime(qreal currentTime);


    // Setters

    void setWidth(qreal width);
    void setHeight(qreal height);
    bool setGridStep(bool increase);
    void setGridLinesCount(int nGridLines);
    void setShowGrid(bool show);
    void setNodeSize(qreal nNodeSize);
    void setShowNodeId(bool show);
    void setShowBatteryCapacity(bool show);
    void setCurrentScale(qreal xScale,qreal yScale);
    void setCurrentUpdateRate(double updateRate);
    void setSceneInfoText(QString text, bool show);
    void setMousePositionLabel(QPointF pos);
    void setShowWirelessCircles(bool show);
    void setShowInterfaceTexts(bool showIp, bool showMac);
    void setBlockPacketRendering(bool block);
    bool setShowMeta(bool show, bool reset);
    void setUnicastMatch(bool match);
    void setShowRoutePath(bool show);


    // Show

    void showMousePositionLabel(bool show);
    bool showPacketStats(bool show);
    void showNode(uint32_t nodeId, bool show);
    bool showNodePosStats(bool show);
    void showAllLinkItems(bool show);
    void showAllNodeItems(bool show);



    // Actions

    void timeToUpdate(double currentTime, bool isReset=false);
    void test();
    void systemReset();
    void softReset();
    void prepareForPlay();
    void preParse();
    void postParse();
    void zoomEventComplete();
    void purgeOldPackets(double currentTime, bool force = false);

    void addNode(uint32_t nodeId,
                 qreal x,
                 qreal y,
                 AnimNodeShape shape,
                 qreal width,
                 qreal height,
                 QString description,
                 QColor * color,
                 bool hasColorUpdate,
                 qreal batteryCapacity,
                 bool hasBattery);
    void addLink(uint32_t fromId,
                 uint32_t toId,
                 QString fromNodeDescription,
                 QString toNodeDescription,
                 QString linkDescription, bool p2p = true);
    void updateLink(uint32_t fromId,
                    uint32_t toId,
                    qreal updateTime,
                    QString linkDescription);

    void updateNode(qreal updateTime,
                    uint32_t nodeId,
                    uint8_t r,
                    uint8_t g,
                    uint8_t b,
                    QString description,
                    bool visible,
                    bool hasColorUpdate,
                    qreal batteryCapacity,
                    bool hasBattery);

    void addPacketRx(uint32_t fromId,
                     double fbTx,
                     double lbTx,
                     uint32_t toId,
                     double fbRx,
                     double lbRx,
                     ParsedElementType type,
                     QString metaInfo);


private:
    typedef QVector <QGraphicsSimpleTextItem*>     GridCoordinatesVector_t;
    typedef std::map <uint32_t, AnimPacket *>      PacketToAnimate_t;
    typedef QVector <QGraphicsPathItem *>          PathItemVector_t;
    typedef QVector <QGraphicsLineItem *>          LineItemVector_t;
    typedef QVector <AnimInterfaceText *>          AnimInterfaceTextVector_t;
    typedef QVector <QGraphicsSimpleTextItem *>    AnimatedMetaInfoVector_t;







    // State

    qreal           m_width;
    qreal           m_height;
    qreal           m_gridStep;
    bool            m_showGrid;
    int             m_nGridLines;
    qreal           m_nodeSize;
    qreal           m_currentXscale;
    qreal           m_currentYscale;
    bool            m_showNodeId;
    bool            m_showBatteryCapacity;
    bool            m_showWirelessCircles;
    QTransform      m_textTransform;
    bool            m_showPackets;
    double          m_lastTime;
    QPen            m_currentPacketPen;
    bool            m_showMeta;
    double          m_currentUpdateRate;
    qreal           m_leftTop;
    qreal           m_righTop;
    bool            m_showIpInterfaceTexts;
    bool            m_showMacInterfaceTexts;
    bool            m_unicastMatch;
    uint32_t        m_packetsShown;
    bool            m_showRoutePath;
    qreal           m_maxLbRx;


    GridCoordinatesVector_t      m_gridCoordinates;
    PacketToAnimate_t            m_packetsToAnimate;
    QLabel *                     m_mousePositionLabel;
    QGraphicsProxyWidget *       m_mousePositionProxyWidget;
    PathItemVector_t             m_wirelessCircles;
    LineItemVector_t             m_gridLines;


    QGraphicsSimpleTextItem *    m_sceneInfoText;
    AnimatedMetaInfoVector_t     m_animatedMetaInfo;
    AnimInterfaceTextVector_t    m_interfaceATexts;
    AnimInterfaceTextVector_t    m_interfaceBTexts;

    explicit AnimatorScene(qreal width = DEFAULT_SCENE_WIDTH, qreal height = DEFAULT_SCENE_HEIGHT, QObject * parent = 0);
    void markGridCoordinates();
    void initGridCoordinates();
    int isPacketInTimeWindow(const AnimPacket * pkt,double currentTime);
    qreal packetForwardScan(double currentTime);
    void printAnimPackets();
    AnimPktMgr::AnimPacketMap_t::const_iterator & printAnimPackets(AnimPktMgr::AnimPacketMap_t::const_iterator iter);
    void addToPacketsToAnimate(AnimPacket * packet);
    void printPacketsToAnimate();
    QPointF getNodeLoc(uint32_t nodeId);
    void showPackets(double currentTime);
    bool showPacket(AnimPacket * packet,double currentTime);
    QPointF getBitPosition(double speed,double currentTime,double bitTime,QLineF linkLine);
    void updateNodeLocations();
    void addToWirelessCircles(WirelessCircleDimensions);
    void resetWirelessCircles();
    void addGrid();
    void resetGrid();
    void resetMetaInfo();
    QRectF getNodeRect(uint32_t nodeId);
    void updateHook(double currentTime);
    void resetInterfaceTexts();
    void removeInterfaceTextCollision();
    QList <QGraphicsItem *> getInterfaceTextCollisionList(AnimInterfaceText * text);
    void initSceneInfoText();
    void initMouseMoveWidgets();
    void setSceneDefaults();
    void initHelpers();
    void initSceneElements();
    bool isNodeListEmpty();
    void repositionInterfaceText(AnimInterfaceText * textItem);
    void resetInterfaceTextTop();
    void repairP2pLinks();
signals:
    
public slots:

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);



};

} // namespace netanim

#endif // ANIMATORSCENE_H

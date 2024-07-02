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
#include "animatormode.h"
#include "debug/xdebug.h"
#include "statistics/routingstatsscene.h"

#include <QtGui/QPainter>
#include <QtGui/QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QtGui/QGraphicsProxyWidget>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QApplication>
#include <QTime>
#include <QFormLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QSvgGenerator>
#include <QGraphicsItem>
#include <QFontMetrics>

#include <math.h>


namespace netanim {

static double lastpacketTimestamp = 0; // TODO
QTransform gTextTransform;
AnimatorScene * pAnimatorScene = 0;
RoutePathManager * pRoutePathManager = 0;
AnimPktMgr::AnimPacketMap_t::const_iterator gAnimPktIterator;


AnimatorScene::AnimatorScene(
        qreal width,
        qreal height,
        QObject * parent) :
    QGraphicsScene(parent),
    m_width(width),
    m_height(height),
    m_showGrid(false),
    m_nGridLines(GRID_LINES_DEFAULT),
    m_nodeSize(NODE_SIZE_SCENE_DEFAULT),
    m_currentXscale(XSCALE_SCENE_DEFAULT),
    m_currentYscale(YSCALE_SCENE_DEFAULT),
    m_showNodeId(true),
    m_showWirelessCircles(false),
    m_showPackets(true),
    m_lastTime(0),
    m_showMeta(true),
    m_showIpInterfaceTexts(false),
    m_showMacInterfaceTexts(false),
    m_unicastMatch(false),
    m_showRoutePath(false),
    m_maxLbRx(0)
{
    initSceneElements();
    setSceneDefaults();
}

AnimatorScene *
AnimatorScene::getInstance()
{
    if(!pAnimatorScene)
    {
        pAnimatorScene = new AnimatorScene;
    }
    return pAnimatorScene;
}

void
AnimatorScene::setSceneDefaults()
{
    m_currentPacketPen.setWidthF(PACKET_PEN_WIDTH_DEFAULT / m_currentXscale);
    m_currentPacketPen.setColor(Qt::blue);
    m_textTransform.scale(1, 1);
    m_width  = DEFAULT_SCENE_WIDTH;
    m_height = DEFAULT_SCENE_HEIGHT;
    lastpacketTimestamp = 0;
    m_currentXscale = XSCALE_SCENE_DEFAULT;
    m_currentYscale = YSCALE_SCENE_DEFAULT;
    m_lastTime = 0;
}

void
AnimatorScene::initSceneElements()
{
    initSceneInfoText();
    initMouseMoveWidgets();
    initGridCoordinates();
}

void
AnimatorScene::initMouseMoveWidgets()
{
    m_mousePositionLabel = new QLabel;
    m_mousePositionLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_mousePositionProxyWidget = addWidget(m_mousePositionLabel, Qt::ToolTip);
    m_mousePositionProxyWidget->setFlag(QGraphicsItem::ItemIgnoresTransformations);

}


void
AnimatorScene::initSceneInfoText()
{
    m_sceneInfoText = new QGraphicsSimpleTextItem;
    m_sceneInfoText->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    addItem(m_sceneInfoText);
}

void
AnimatorScene::initGridCoordinates()
{
    for (int i = 0; i < m_gridCoordinates.size(); ++i)
    {
        QGraphicsSimpleTextItem * item = m_gridCoordinates[i];
        removeItem(item);
        delete item;
    }
    m_gridCoordinates.clear();
    for(int i = 0; i < 9; i++) // only 9 coordinates will be marked
    {
        QGraphicsSimpleTextItem * item = new QGraphicsSimpleTextItem;
        item->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        m_gridCoordinates.push_back(item);
        addItem(item);

    }
    markGridCoordinates();

}


void
AnimatorScene::setSceneInfoText(QString text, bool show)
{
    m_sceneInfoText->setText(text);
    m_sceneInfoText->setVisible(show);    
    QFontMetrics fm(font());
    QRectF r = getSceneRect();
    QPointF pos = QPointF((getWidth() - fm.width(text))/2, r.center().y());
    m_sceneInfoText->setPos(pos);
}

void
AnimatorScene::setShowRoutePath(bool show)
{
    m_showRoutePath = show;
    if(!show)
    {
        RoutePathManager::getInstance()->systemReset();
    }
}


bool
AnimatorScene::setShowMeta(bool show, bool reset)
{
    if(show && (!AnimPktMgr::getInstance()->getMetaInfoSeen()))
    {
        if(!reset)
        {
            AnimatorMode::getInstance()->showPopup("Packet stream does not contain meta-data. \
                                                    Please use AnimationInterface::EnablePacketMetadata(true)");
        }
        return false;
    }
    m_showMeta = show;
    return show;
}


void
AnimatorScene::setShowGrid(bool show)
{
    m_showGrid = show;
    resetGrid();
    if(m_showGrid)
    {
        addGrid();
    }
    update();
}

void
AnimatorScene::setMousePositionLabel(QPointF pos)
{

    QString string = "    (" + QString::number(qRound(pos.x())) + "," + QString::number(getHeight()-qRound(pos.y())) + ")";
    m_mousePositionLabel->setText(string);
    m_mousePositionProxyWidget->setPos(pos.x(), pos.y());
    m_mousePositionLabel->adjustSize();

}

void
AnimatorScene::setCurrentScale(qreal xScale, qreal yScale)
{
//    qDebug(QString("Current Scale:")+ QString::number(m_currentXscale)+QString(" ")+QString::number(m_currentYscale));
//    qDebug(QString("Set Current Scale:")+ QString::number(xScale)+QString(" ")+QString::number(yScale));
    m_currentXscale = xScale;
    m_currentYscale = yScale;
    m_textTransform.scale(1/m_currentXscale, 1/m_currentYscale);
    gTextTransform = m_textTransform;

}

void
AnimatorScene::setWidth(qreal width)
{
    m_width = width;
}

void
AnimatorScene::setHeight(qreal height)
{
    m_height = height;
}


void
AnimatorScene::setShowWirelessCircles(bool show)
{
    m_showWirelessCircles = show;
    if(!show)
    {
        resetWirelessCircles();
    }
}


void
AnimatorScene::setGridLinesCount(int nGridLines)
{
    m_nGridLines = nGridLines;
    resetGrid();
    if(m_showGrid)
    {
        addGrid();
    }
    update();
}


void
AnimatorScene::setNodeSize(qreal nNodeSize)
{
    //qDebug(QString("Current Scale:")+ QString::number(m_currentXscale)+QString(" ")+QString::number(m_currentYscale));
    m_nodeSize = nNodeSize/m_currentXscale * 10;
    m_currentPacketPen.setWidthF(PACKET_PEN_WIDTH_DEFAULT / m_currentXscale);
    for (AnimNodeMgr::AnimNodeMap_t::const_iterator i = AnimNodeMgr::getInstance()->getAnimNodes()->begin();
         i != AnimNodeMgr::getInstance()->getAnimNodes()->end();
         ++i)
    {
        AnimNode * aNode = i->second;
        aNode->setSize(m_nodeSize);
    }
    setShowNodeId(m_showNodeId);
    update();

}

bool
AnimatorScene::setGridStep(bool increase)
{
    if(increase)
    {
        m_gridStep *= 2;
    }
    else
    {
        m_gridStep /= 2;
    }
    update();
    if((m_gridStep >= GRID_STEP_MAX) && (increase))
    {
        return false;
    }
    if((m_gridStep <  GRID_STEP_MIN) && (!increase))
    {
        return false;
    }
    return true;
}

void
AnimatorScene::setUnicastMatch(bool match)
{
    m_unicastMatch = match;
}

void
AnimatorScene::setShowInterfaceTexts(bool showIp, bool showMac)
{
    resetInterfaceTexts();
    m_showIpInterfaceTexts = showIp;
    m_showMacInterfaceTexts = showMac;
    if(!m_showIpInterfaceTexts && !m_showMacInterfaceTexts)
    {
        return;
    }
    if(!m_interfaceATexts.size())
    {
        for(LinkManager::NodeIdAnimLinkVectorMap_t::const_iterator i = LinkManager::getInstance()->getLinks()->begin();
            i != LinkManager::getInstance()->getLinks()->end();
            ++i)
        {

            LinkManager::AnimLinkVector_t linkVector = i->second;

            for(LinkManager::AnimLinkVector_t::const_iterator j = linkVector.begin();
                j != linkVector.end();
                ++j)
            {
                AnimLink * animLink = *j;

                QString pointADescription = animLink->getInterfaceADescription();
                QPointF pointApos = animLink->getInterfacePosA();
                AnimInterfaceText * interfaceAText = new AnimInterfaceText(pointADescription);
                interfaceAText->setPos(pointApos);
                addItem(interfaceAText);
                m_interfaceATexts.push_back(interfaceAText);
                interfaceAText->setMode(m_showIpInterfaceTexts, m_showMacInterfaceTexts);

                QString pointBDescription = animLink->getInterfaceBDescription();
                if(pointBDescription == "")
                {
                    continue;
                }
                QPointF pointBpos = animLink->getInterfacePosB();
                AnimInterfaceText * interfaceBText = new AnimInterfaceText(pointBDescription, true);
                interfaceBText->setMode(m_showIpInterfaceTexts, m_showMacInterfaceTexts);
                addItem(interfaceBText);
                interfaceBText->setPos(pointBpos);
                m_interfaceBTexts.push_back(interfaceBText);
            }
        }
        update();
        removeInterfaceTextCollision();
        return;
     }
    for(AnimInterfaceTextVector_t::const_iterator i = m_interfaceATexts.begin();
        i != m_interfaceATexts.end();
        ++i)
    {
        AnimInterfaceText * interfaceText = *i;
        interfaceText->setMode(m_showIpInterfaceTexts, m_showMacInterfaceTexts);
        QGraphicsLineItem * l = interfaceText->getLine();
        if(l)
        {
            l->setVisible(showIp || showMac);
        }
        interfaceText->setVisible(showIp || showMac);
    }
    for(AnimInterfaceTextVector_t::const_iterator i = m_interfaceBTexts.begin();
        i != m_interfaceBTexts.end();
        ++i)
    {
        AnimInterfaceText * interfaceText = *i;
        interfaceText->setMode(m_showIpInterfaceTexts, m_showMacInterfaceTexts);
        QGraphicsLineItem * l = interfaceText->getLine();
        if(l)
        {
            l->setVisible(showIp || showMac);
        }
        interfaceText->setVisible(showIp || showMac);
    }
    removeInterfaceTextCollision();
    update();
}

void
AnimatorScene::setShowNodeId(bool show)
{

    m_showNodeId = show;

    for (AnimNodeMgr::AnimNodeMap_t::const_iterator i = AnimNodeMgr::getInstance()->getAnimNodes()->begin();
     i != AnimNodeMgr::getInstance()->getAnimNodes()->end();
     ++i)
    {
        AnimNode * aNode = i->second;
        aNode->showNodeIdText(show);
    }
}

void
AnimatorScene::setShowBatteryCapacity(bool show)
{
    m_showBatteryCapacity = show;

    Q_FOREACH(const AnimNodeMgr::AnimNodeMap_t::value_type& nodePair, *AnimNodeMgr::getInstance()->getAnimNodes())
        nodePair.second->showBatteryCapacity(show);
}

void
AnimatorScene::setCurrentUpdateRate(double updateRate)
{
    m_currentUpdateRate = updateRate;
}

void
AnimatorScene::setBlockPacketRendering(bool block)
{
    m_showPackets = !block;
}


qreal
AnimatorScene::getNextPacketTime()
{

    AnimPktMgr::AnimPacketMap_t::const_iterator i = gAnimPktIterator;
    ++i;
    if(i == AnimPktMgr::getInstance()->getPackets()->end())
    {
        return 0;
    }
    AnimPacket * p = i->second;
    qDebug(p->m_fbTx, " t");
    return p->m_fbTx;

}

qreal
AnimatorScene::getNextPacketTime(qreal currentTime)
{

    AnimPktMgr::AnimPacketMap_t::const_iterator i = gAnimPktIterator;
    if(i == AnimPktMgr::getInstance()->getPackets()->end())
    {
        return PACKET_TIME_MAX;
    }
    AnimPacket * p = 0;
    for (;
         i != AnimPktMgr::getInstance()->getPackets()->end();
         ++i)
    {
        p = i->second;
        if(p->m_fbTx >= currentTime)
        {
            break;
        }
    }
    if (i == AnimPktMgr::getInstance()->getPackets()->end())
    {
        return PACKET_TIME_MAX;
    }
    return p->m_fbTx;

}

QRectF
AnimatorScene::getSceneRect()
{
    qreal sceneOffsetX = AnimatorView::getInstance()->getCurrentZoomFactor() * getWidth()/10;
    qreal sceneOffsetY = AnimatorView::getInstance()->getCurrentZoomFactor() * getHeight()/10;
    QPointF topLeft(-2 * sceneOffsetX, -sceneOffsetY);
    QPointF bottomRight(m_width +  sceneOffsetX, m_height + 2 * sceneOffsetY);
    return QRectF(topLeft, bottomRight);
}


qreal
AnimatorScene::getWidth()
{
    return m_width;
}

qreal
AnimatorScene::getHeight()
{
    return m_height;
}

uint32_t
AnimatorScene::getNodeCount()
{
    if(AnimNodeMgr::getInstance()->isEmpty())
    {
        return 0;
    }
    return (AnimNodeMgr::getInstance()->getAnimNodes()->end())->first;

}

QPointF
AnimatorScene::getNodeCenter(uint32_t nodeId)
{
    AnimNode * node = AnimNodeMgr::getInstance()->getNode(nodeId);
    if(!node)
    {
        fatalError("getNodeCenter: node == 0");
    }
    return node->getGraphicsItem()->boundingRect().center();
}



void
AnimatorScene::timeToUpdate(double currentTime, bool isReset)
{
    if(NodeMobilityMgr::getInstance()->updateLocations(currentTime))
    {
        updateNodeLocations();
    }

    LinkUpdateManager::getInstance()->updateLinks(currentTime);
    NodeUpdateManager::getInstance()->updateNodes(currentTime);
    updateHook(currentTime);
    bool nodePosVisible = NodePositionStatisticsDialog::getInstance()->isVisible();
    if(m_lastTime > currentTime)
    {
        if(nodePosVisible)
        {
            NodePositionStatisticsDialog::getInstance()->applyNodePosFilterSlot();
        }
    }
    qreal suggestedNextPacketTime = PACKET_TIME_MAX;
    if (!isReset && (!AnimPktMgr::getInstance()->isEmpty()))
    {
        if(!nodePosVisible)
        {
            suggestedNextPacketTime = packetForwardScan(currentTime);
            showPackets(currentTime);
        }
    } else if (isReset)
    {
        suggestedNextPacketTime = 0;
    }
    RoutePathManager::getInstance()->update(currentTime);

    qreal suggestedNextMobilityTime = NodeMobilityMgr::getInstance()->getNextUpdateTime();
    qreal nextTime = qMin(suggestedNextMobilityTime, suggestedNextPacketTime);
    nextTime = qMin(nextTime, LinkUpdateManager::getInstance()->getNextUpdateTime());
    nextTime = qMin(nextTime, NodeUpdateManager::getInstance()->getNextUpdateTime());
    AnimatorMode::getInstance()->setCurrentTime(nextTime);
    m_lastTime = currentTime;
    update();

}

void
AnimatorScene::preParse()
{
    setSceneInfoText("", false);
}


void
AnimatorScene::postParse()
{
    setSceneRect(getSceneRect());
    setSceneInfoText("", false);
    gAnimPktIterator = AnimPktMgr::getInstance()->getPackets()->begin();
    timeToUpdate(0, true);
    repairP2pLinks();
    resetInterfaceTextTop();
    setShowInterfaceTexts(m_showIpInterfaceTexts, m_showMacInterfaceTexts);
}

void
AnimatorScene::prepareForPlay()
{
    m_textTransform.scale(1/m_currentXscale, 1/m_currentYscale);
}

void
AnimatorScene::test()
{
   /* static AnimPacketMap_t::const_iterator iter = m_animPackets.begin();
    if (!m_animPackets.empty())
    iter = printAnimPackets(iter);*/

   // printPacketsToAnimate();
    //qDebug(m_packetsToAnimate.size(),"PacketsToAnimate");

}

void
AnimatorScene::zoomEventComplete()
{
    setSceneRect(getSceneRect());
    updateNodeLocations();
    resetInterfaceTexts();
    setShowInterfaceTexts(m_showIpInterfaceTexts, m_showMacInterfaceTexts);
}


void
AnimatorScene::systemReset()
{

    setSceneDefaults();
    resetMetaInfo();
    purgeOldPackets(0, true);

    AnimPktMgr::getInstance()->systemReset();
    AnimNodeMgr::getInstance()->systemReset();
    NodeMobilityMgr::getInstance()->systemReset();
    LinkUpdateManager::getInstance()->systemReset();
    NodeUpdateManager::getInstance()->systemReset();
    LinkManager::getInstance()->systemReset();
    RoutePathManager::getInstance()->systemReset();

    resetWirelessCircles();
    resetInterfaceTexts();
    gAnimPktIterator = AnimPktMgr::getInstance()->getPackets()->begin();
    m_maxLbRx = 0;

    invalidate();

}

void
AnimatorScene::softReset()
{
    purgeOldPackets(10000, true);
    lastpacketTimestamp = 0;
    m_maxLbRx = 0;
    gAnimPktIterator = AnimPktMgr::getInstance()->getPackets()->begin();
    invalidate();
    timeToUpdate(0, true);

}


bool
AnimatorScene::showNodePosStats(bool show)
{
    if(!show)
    {
        showAllLinkItems(true);
        showAllNodeItems(true);
    }
    if(show && (NodeMobilityMgr::getInstance()->isNodeListEmpty()))
    {
        AnimatorMode::getInstance()->showPopup("No Nodes have been parsed. Did you load the xml trace file?");
        return false;
    }
    NodePositionStatisticsDialog::getInstance()->setMode(show);
    return true;
}

bool
AnimatorScene::showPacketStats(bool show)
{
    if(show && (AnimPktMgr::getInstance()->isEmpty()))
    {
        AnimatorMode::getInstance()->showPopup("No packets have been parsed. Did you load the xml trace file?");
        return false;
    }
    Packetstatisticsdialog::getInstance()->setMode(show);
    return true;
}

void
AnimatorScene::showMousePositionLabel(bool show)
{
    m_mousePositionProxyWidget->setVisible(show);
}


void
AnimatorScene::addPacketRx(uint32_t fromId, double fbTx,
            double lbTx, uint32_t toId,
            double fbRx, double lbRx,
            ParsedElementType type,
            QString metaInfo)
{
    lastpacketTimestamp = lbRx;// TODO
    if(fromId == toId)
    {
        return;
    }
    AnimPacket * packet = AnimPktMgr::getInstance()->addPacket(fromId, toId,
                                      fbTx, lbTx,
                                      fbRx, lbRx,
                                      (type == XML_WPACKET_RX)?1:0);
   if ((metaInfo != "null") && (m_showMeta))
   {
       AnimPktMgr::getInstance()->setMetaInfoSeen();
       packet->parseMeta(metaInfo.toAscii().data());
   }
}


void
AnimatorScene::addLink(uint32_t fromId, uint32_t toId, QString fromNodeDescription,
                       QString toNodeDescription,
                       QString linkDescription,
                       bool p2p)
{
    //qDebug(QString ("Adding link:") + QString::number(fromId) + QString::number(toId));
    QPointF fromNodeLoc = getNodeLoc(fromId);
    QPointF toNodeLoc = getNodeLoc(toId);
    QPointF invertedFromNodeLoc = QPointF(fromNodeLoc.x(), getHeight()-fromNodeLoc.y());
    QPointF invertedToNodeLoc = QPointF(toNodeLoc.x(), getHeight()-toNodeLoc.y());

//    QLineF line(fromNodeLoc, toNodeLoc);
    QLineF line(invertedFromNodeLoc, invertedToNodeLoc);
    if(!p2p)
    {
        AnimLink * item = LinkManager::getInstance()->addLink(fromId, toId, line, fromNodeDescription, toNodeDescription, linkDescription, p2p);
        addItem(item);
        return;
    }

    AnimNodeMgr::getInstance()->getNode(fromId)->setPos(line.p1());
    AnimNodeMgr::getInstance()->getNode(toId)->setPos(line.p2());
    AnimLink * item = LinkManager::getInstance()->addLink(fromId, toId, line, fromNodeDescription, toNodeDescription, linkDescription);
    LinkUpdateManager::getInstance()->addLinkUpdate(0, item, linkDescription);

    addItem(item);
}

void
AnimatorScene::addNode(uint32_t nodeId,
                       qreal x,
                       qreal y,
                       AnimNodeShape shape,
                       qreal width,
                       qreal height,
                       QString description,
                       QColor * color,
                       bool hasColorUpdate,
                       qreal batteryCapacity,
                       bool hasBattery)
{
    AnimNode * aNode = 0;
    bool addToScene = false;
    const AnimNodeEnergySource::EnergySourceType energySourceType = hasBattery ? AnimNodeEnergySource::BATTERY : AnimNodeEnergySource::GRID;
    const AnimNodeEnergySource energySource(energySourceType, batteryCapacity);

    aNode = AnimNodeMgr::getInstance()->addNode(nodeId,
                                        shape,
                                        width,
                                        height,
                                        description,
                                        color,
                                        &addToScene,
                                        energySource);

    if(addToScene)
    {
        addItem(aNode->getGraphicsItem());
        addItem(aNode->getNodeIdTextItem());
        aNode->showNodeIdText(m_showNodeId);

        if(aNode->getEnergySource().getType() != AnimNodeEnergySource::GRID)
        {
            Q_ASSERT(aNode->getBatteryItem());
            addItem(aNode->getBatteryItem());
        }

        aNode->showBatteryCapacity(m_showBatteryCapacity);
    }


    QPointF pos(x, y);
    aNode->setPos(pos);
    NodeMobilityMgr::getInstance()->addNode(lastpacketTimestamp, nodeId, pos);
    if (color && hasColorUpdate)
    {
        NodeUpdateManager::getInstance()->addNodeUpdate(0, aNode, color->red(), color->green(), color->blue(), description, true, !hasColorUpdate, energySource);
    }
    else
    {
        NodeUpdateManager::getInstance()->addNodeUpdate(0, aNode, 255, 0, 0, description, true, !hasColorUpdate, energySource);
    }
}


void
AnimatorScene::updateLink(uint32_t fromId, uint32_t toId, qreal updateTime, QString linkDescription)
{
    AnimLink * link = LinkManager::getInstance()->getAnimLink(fromId, toId);
    if(link)
    {
        LinkUpdateManager::getInstance()->addLinkUpdate(updateTime, link, linkDescription);
    }
}

void
AnimatorScene::updateNode(qreal updateTime, uint32_t nodeId, uint8_t r, uint8_t g, uint8_t b,
                          QString description, bool visible, bool hasColorUpdate, qreal batteryCapacity,
                          bool hasBattery)
{
    AnimNode * animNode = AnimNodeMgr::getInstance()->getNode(nodeId);
    if(animNode)
    {
        const AnimNodeEnergySource::EnergySourceType energySourceType = hasBattery ? AnimNodeEnergySource::BATTERY : AnimNodeEnergySource::GRID;

        NodeUpdateManager::getInstance()->addNodeUpdate(updateTime, animNode, r, g, b, description,
                                                        visible, !hasColorUpdate, AnimNodeEnergySource(energySourceType, batteryCapacity));
    }
}


void
AnimatorScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if(AnimatorMode::getInstance()->getBackground())
    {
        return QGraphicsScene::mouseMoveEvent(event);
    }
      QPointF scenePos = event->scenePos();
 //   QString s = "Mouse:" + QString::number(event->scenePos().x()) + "," + QString::number(event->scenePos().y());
 //   qDebug(s.toAscii().data());
    setMousePositionLabel(scenePos);
    if((scenePos.x() < 0) ||
       (scenePos.y() < 0) ||
       (scenePos.x() > getWidth()) ||
       (scenePos.y() > getHeight()))
    {
        showMousePositionLabel(false);
    }
    else
    {
        showMousePositionLabel(true);
    }
    QGraphicsScene::mouseMoveEvent(event);

}


QVector <QGraphicsSimpleTextItem *>
AnimatorScene::getGridCoordinatesItems()
{
    return m_gridCoordinates;
}

void
AnimatorScene::markGridCoordinates()
{
    int i = 0;
    for (qreal x = 0; x <= m_width  ; x = x + (m_width/2))
    for (qreal y = 0; y <= m_height ; y = y + (m_height/2))
    {
        QString text = QString::number(qRound(x))
                       + ","
                       + QString::number(getHeight()-qRound(y));
        m_gridCoordinates[i]->setText(text);
        m_gridCoordinates[i]->setPos(QPointF(x, y));
        m_gridCoordinates[i]->setVisible(m_showGrid);
        i++;
    }

}

void
AnimatorScene::addGrid()
{
    qreal xStep = m_width/(m_nGridLines-1);
    qreal yStep = m_height/(m_nGridLines-1);
    QPen pen(QColor(100, 100, 155, 125));

    // draw horizontal grid

    for (qreal y = 0; y <= m_height; y += yStep) {
       m_gridLines.push_back(addLine(0, y, m_width, y, pen));
    }
    // now draw vertical grid
    for (qreal x = 0; x <= m_width; x += xStep) {
       m_gridLines.push_back(addLine(x, 0, x, m_height, pen));
    }
    initGridCoordinates();
    markGridCoordinates();
}

void
AnimatorScene::resetGrid()
{
    for(LineItemVector_t::const_iterator i = m_gridLines.begin();
        i != m_gridLines.end();
        ++i)
    {

        removeItem(*i);
        delete (*i);
    }
    m_gridLines.clear();

    for(GridCoordinatesVector_t::const_iterator i = m_gridCoordinates.begin();
        i != m_gridCoordinates.end();
        ++i)
    {
        removeItem(*i);
        delete (*i);
    }
    m_gridCoordinates.clear();
}




bool
AnimatorScene::isNodeListEmpty()
{
    return NodeMobilityMgr::getInstance()->isNodeListEmpty();
}

void
AnimatorScene::updateNodeLocations()
{
    for (AnimNodeMgr::AnimNodeMap_t::const_iterator i = AnimNodeMgr::getInstance()->getAnimNodes()->begin();
         i != AnimNodeMgr::getInstance()->getAnimNodes()->end();
         ++i)
    {
        AnimNode * aNode = i->second;
        aNode->setPos(NodeMobilityMgr::getInstance()->getNodeLocation(i->first));
        //qDebug(m_nodeMobilityMgr->getNodeLocation(i->first), QString::number(i->first));
    }
}

int
AnimatorScene::isPacketInTimeWindow(const AnimPacket *pkt, double currentTime)
{
    qDebug(currentTime, "CurrentTime");
    qDebug(currentTime + m_currentUpdateRate, "Bound");

    if(pkt->m_isWpacket)
    {
        /*if (((pkt->m_fbTx) > currentTime) && (pkt->m_fbTx < (currentTime + m_currentUpdateRate)))
        {
            return 0;
        }*/
        if(pkt->m_fbTx == currentTime)
        {
            return 0;
        }
        else if (pkt->m_fbTx < currentTime)
        {
            return 1;
        }
        else
        {
            return 2;
        }
    }
    // Possibly a wired packet at this point

    if (!pkt->m_lbRx)
    {
        return 1;
    }
    // 1 In the past
    // 2 In the future
    if((pkt->m_lbRx) && (currentTime > *pkt->m_lbRx))
    {
        //qDebug("Packet in past");
        return 1;
    }
    if ((currentTime) < pkt->m_fbTx)
    {
        //qDebug("Packet in future");
        return 2;
    }
    return 0;

}


void
AnimatorScene::updateHook(double currentTime)
{
    // An example of how to manipulate the nodes for a given time slot
    /*if (currentTime >= 3)
    {
        AnimNode * aNode = getAnimNode(3);
        if(aNode)
        {
            aNode->setVisible(true); // Make the node invisible
            aNode->setDescription("New Description"); //Set a new description
            aNode->setColor(255, 255, 255); //Set node to white
        }
    }*/
    (void)currentTime;
}




void
AnimatorScene::purgeOldPackets(double currentTime, bool force)
{
    //qDebug(1, "Purging");
    QTime eTime;
    eTime.start();
    QVector <uint64_t> purgeList;
    for (PacketToAnimate_t::const_iterator i = m_packetsToAnimate.begin();
         i != m_packetsToAnimate.end();
         ++i)
    {
        AnimPacket * packet = i->second;
        if((packet->m_isWpacket && (currentTime > packet->m_fbTx)) || force)
        {
            purgeList.push_back(packet->getAnimId());
            removeItem(packet->m_graphicsItem);
            if(packet->m_graphicsTextItem->scene())
            {
                removeItem(packet->m_graphicsTextItem);
            }
            continue;
        }
        if(packet->m_isWpacket)
        {
            continue;
        }
        if ((*packet->m_lbRx < currentTime) || force)
        {
            purgeList.push_back(packet->getAnimId());
            removeItem(packet->m_graphicsItem);
            if(packet->m_graphicsTextItem->scene())
            {
                removeItem(packet->m_graphicsTextItem);
            }
        }

    }
    //qDebug(purgeList.size(),"size of purge list");
    //qDebug(purgeList.count(), "Purge list count");
    for (QVector <uint64_t>::const_iterator i = purgeList.begin();
         i != purgeList.end();
         ++i)
    {
        m_packetsToAnimate.erase(*i);
    }
    //qDebug(m_packetsToAnimate.size(), "Pkt anim count");
    //qDebug(eTime.elapsed(), "Purge time");

}

bool
AnimatorScene::isRoutePathTracking()
{
    return m_showRoutePath;
}

QPointF
AnimatorScene::getNodeLoc(uint32_t nodeId)
{
    return NodeMobilityMgr::getInstance()->getNodeLocation(nodeId);
}


QPointF
AnimatorScene::getBitPosition(double speed, double currentTime, double bitTime, QLineF linkLine)
{
    double timeDelta = currentTime - bitTime;
    double distanceTraveled = timeDelta * speed;
    linkLine.setLength(distanceTraveled);
    return linkLine.p2();

}

RoutePathManager::RoutePathManager()
{
    m_routeLabel = new QLabel();
    m_routeLabelProxyWidget = AnimatorScene::getInstance()->addWidget(m_routeLabel);
    m_routeLabelProxyWidget->setVisible(false);
    m_routeLabelProxyWidget->setFlags(QGraphicsItem::ItemIgnoresTransformations);

}

RoutePathManager *
RoutePathManager::getInstance()
{
    if(!pRoutePathManager)
    {
        pRoutePathManager = new RoutePathManager;
    }
    return pRoutePathManager;
}


void
RoutePathManager::systemReset()
{
    remove();
    m_routeLabelProxyWidget->setVisible(false);

}

void
RoutePathManager::add(RoutePath_t rp)
{
    QString labelText = "Route from Node:" + QString::number(rp.nodeIdDest.fromNodeId) + "---> " + rp.nodeIdDest.destination + "\n";
    AnimatorScene * scene = AnimatorScene::getInstance();
    AnimNode * srcNode = AnimNodeMgr::getInstance()->getNode(rp.nodeIdDest.fromNodeId);
    srcNode->setRoutePathSource(true);

    srcNode->markRoutePath(true);
    QPointF lastNodePos = scene->getNodeCenter(srcNode->getNodeId());

    for (RoutePathElementsVector_t::const_iterator i = rp.elements.begin()+1;
         i != rp.elements.end();
         ++i)
    {
        RoutePathElement rpe = *i;
        AnimNode * nextNode = AnimNodeMgr::getInstance()->getNode(rpe.nodeId);
        QString nextHop = rpe.nextHop;
        if (rpe.nextHop == "C")
            nextHop = "Connected";
        if (rpe.nextHop == "L")
        {
            nextHop = "Local";
            nextNode->setRouteDestination(true);
        }
        if (rpe.nextHop == "-1")
            nextHop = "No Route";
        labelText += "Node:" + QString::number(rpe.nodeId) + " NextHop:" + nextHop + "\n";
        if(!nextNode)
        {
            continue;
        }
        nextNode->markRoutePath(true);
        QPointF thisNodePos = scene->getNodeCenter(nextNode->getNodeId());
        QLineF line(lastNodePos, thisNodePos);
        QPen pen;
        pen.setColor(Qt::red);
        //pen.setWidth(nextNode->getSize()/AnimatorView::getInstance()->transform().m11());
        QGraphicsLineItem * lineItem = scene->addLine(line, pen);
        m_routePathLines.push_back(lineItem);
        m_markedNodes.push_back(nextNode);
        lastNodePos = thisNodePos;
    }
    m_routeLabel->setText(labelText);
    m_routeLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_routeLabelProxyWidget->setVisible(true);
    m_routeLabelProxyWidget->setPos(QPointF(scene->getWidth()/2, 0));

}

void
RoutePathManager::remove()
{
    for(MarkedNodeVector_t::const_iterator i = m_markedNodes.begin();
        i != m_markedNodes.end();
        ++i)
    {
        (*i)->markRoutePath(false);
    }
    m_markedNodes.clear();
    for(RoutePathLineVector_t::const_iterator i = m_routePathLines.begin();
        i != m_routePathLines.end();
        ++i)
    {
        AnimatorScene::getInstance()->removeItem(*i);
    }
    m_routePathLines.clear();
}

void
RoutePathManager::update(qreal currentTime)
{
    if(!AnimatorScene::getInstance()->isRoutePathTracking())
    {
        return;
    }
    remove();
    RoutePathVector_t rps =  RoutingStatsScene::getInstance()->getRoutePaths(currentTime);
    for(RoutePathVector_t::const_iterator i = rps.begin();
        i != rps.end();
        ++i)
    {
        RoutePath_t rp = *i;
        add(rp);
    }

}


bool
AnimatorScene::showPacket(AnimPacket * packet,double currentTime)
{
    if(!m_showPackets)
    {
        return false;
    }
    if(!packet->m_selected)
    {
        return false;
    }
    if(packet->m_fromId == packet->m_toId)
    {
        return false;
    }
    bool packetVisible = true;
    if(m_unicastMatch && packet->m_isWpacket)
    {
        if(packet->getShortMeta() != "")
        {
            AnimNode * pAnimNode = AnimNodeMgr::getInstance()->getNode(packet->m_toId);
            if(packet->m_wifiMacInfo)
            {
                packetVisible = pAnimNode->hasMac(QString(packet->m_wifiMacInfo->Da.c_str()));
                if(!packetVisible)
                {
                    packetVisible = pAnimNode->hasMac(QString(packet->m_wifiMacInfo->Ra.c_str()));
                }

            }
            else
            {
                packetVisible = false;
            }
            if(!packetVisible && packet->m_ipv4Info)
            {
                packetVisible = pAnimNode->hasIpv4(QString(packet->m_ipv4Info->DstIp.c_str()));
            }
            else
            {
                packetVisible = false;
            }
        }
        else
        {
            packetVisible = false;
        }
    }


    QPointF fromNodeLoc = getNodeCenter(packet->m_fromId);
    QPointF toNodeLoc   = getNodeCenter(packet->m_toId);


    QLineF linkLine(fromNodeLoc, toNodeLoc);
    double fbRx = 0;
    if(packet->m_fbRx)
    {
        fbRx = *packet->m_fbRx;
    }
    double propagationDelay = fbRx - packet->m_fbTx;

    double speed = linkLine.length()/propagationDelay;

    QLineF packetLine(linkLine);

    if((!packet->m_isWpacket))
     {
        if(*packet->m_lbTx > currentTime)
        {
            packetLine.setP1(fromNodeLoc);
        }
        else
        {
            packetLine.setP1(getBitPosition(speed, currentTime, *packet->m_lbTx, linkLine));
        }

        if(*packet->m_fbRx < currentTime)
        {
            packetLine.setP2(toNodeLoc);

        }
        else
        {
            packetLine.setP2(getBitPosition(speed, currentTime, packet->m_fbTx, linkLine));
        }
    }
    else
    {
        if(m_showWirelessCircles && packet->m_isWpacket && packetVisible)
        {
            WirelessCircleDimensions dimensions = { fromNodeLoc, packetLine.length() };
            addToWirelessCircles(dimensions);
        }
    }

    qreal angle = packetLine.angle();
    packetLine.setLength(packetLine.length() * INTER_PACKET_GAP);
    packet->m_graphicsItem->setLine(packetLine);
    packet->m_graphicsItem->setPen(m_currentPacketPen);



    packet->m_graphicsItem->setVisible(packetVisible);

    if (!packetVisible || !m_showMeta || ((packet->getShortMeta() == "")))
    {
        return packetVisible;
    }



    bool isWest = (fromNodeLoc.x() > toNodeLoc.x())? true: false;
    packet->m_graphicsTextItem->m_isWest = isWest;
    packet->m_graphicsTextItem->show();
    packet->m_graphicsTextItem->setText(packet->getShortMeta().c_str());
    packet->m_graphicsTextItem->setRotation(-angle);
    if (isWest)
    {
        packet->m_graphicsTextItem->setRotation(180-angle);
        packet->m_graphicsTextItem->setPos(packetLine.p2());
    }
    else
    {
        packet->m_graphicsTextItem->setPos(packetLine.p1());
    }
    return packetVisible;
}


void
AnimatorScene::resetWirelessCircles()
{
    for(PathItemVector_t::const_iterator i = m_wirelessCircles.begin();
        i != m_wirelessCircles.end();
        ++i)
    {
        removeItem(*i);
        delete (*i);
    }
    m_wirelessCircles.clear();
}

void
AnimatorScene::addToWirelessCircles(WirelessCircleDimensions dimensions)
{
    QPainterPath path;
    path.addEllipse(dimensions.center, dimensions.radius/4, dimensions.radius/4);
    path.addEllipse(dimensions.center, dimensions.radius/2, dimensions.radius/2);
    path.addEllipse(dimensions.center, dimensions.radius, dimensions.radius);
    QGraphicsPathItem * pathItem = addPath(path);
    m_wirelessCircles.push_back(pathItem);
}

void
AnimatorScene::showPackets(double currentTime)
{
    m_packetsShown = 0;
    //purgeOldPackets(currentTime);
    if(m_showWirelessCircles)
    {
        resetWirelessCircles();
    }
    if(m_showRoutePath)
    {
        return;
    }
    for (PacketToAnimate_t::const_iterator i = m_packetsToAnimate.begin();
         i != m_packetsToAnimate.end();
         ++i)
    {
        showPacket(i->second, currentTime)?++m_packetsShown:0;
    }
}


qreal
AnimatorScene::packetForwardScan(double currentTime)
{
    //qDebug(currentTime ," packetForwardScan currentTime");
    if(m_lastTime > currentTime)
    {
        gAnimPktIterator = AnimPktMgr::getInstance()->getPackets()->begin();
        purgeOldPackets(currentTime, true);
    }
    AnimPktMgr::AnimPacketMap_t::const_iterator i = gAnimPktIterator;
    if(i == AnimPktMgr::getInstance()->getPackets()->end())
    {
        return PACKET_TIME_MAX;
    }
    qreal suggestedTime = currentTime;
    for (;
         i != AnimPktMgr::getInstance()->getPackets()->end();
         ++i)
    {
        AnimPacket * p = i->second;

        if(p->m_isWpacket)
        {
            if(p->m_fbTx == currentTime)
            {
                addToPacketsToAnimate(i->second);
                gAnimPktIterator++;

            }
            else if (p->m_fbTx > currentTime)
            {
                suggestedTime = p->m_fbTx;
                gAnimPktIterator = i ;
                break;
            }
        }
        else
        {
            m_maxLbRx = qMax(*p->m_lbRx, m_maxLbRx);
            if(*p->m_lbRx < currentTime)
            {
                gAnimPktIterator++;
                continue;
            }
            if(p->m_fbTx == currentTime)
            {
                addToPacketsToAnimate(i->second);
                gAnimPktIterator = i;

            }
            else if ((p->m_fbTx < currentTime) && (*p->m_lbRx >= currentTime))
            {
                addToPacketsToAnimate(i->second);
            }
            else
            {
                if(!getAnimatedPacketCount())
                {
                    suggestedTime = getNextPacketTime(currentTime);

                    if ((suggestedTime == PACKET_TIME_MAX) && (currentTime < m_maxLbRx))
                    {
                        suggestedTime = currentTime + 0.0001;
                    }
                    else if (suggestedTime == PACKET_TIME_MAX)
                    {
                        gAnimPktIterator = AnimPktMgr::getInstance()->getPackets()->end();
                    }
                }
                break;
            }
        }
    }

    return suggestedTime;

}

uint32_t
AnimatorScene::getAnimatedPacketCount()
{
    return m_packetsShown + m_packetsToAnimate.size();
}

void
AnimatorScene::addToPacketsToAnimate(AnimPacket * packet)
{
   //qDebug(1, "Adding packets to animate");
    uint64_t animId = packet->getAnimId();
    if(m_packetsToAnimate.find(animId) == m_packetsToAnimate.end())
    {
        addItem(packet->m_graphicsItem);
        addItem(packet->m_graphicsTextItem);
        packet->m_graphicsTextItem->setVisible(m_showMeta);
        m_animatedMetaInfo.push_back(packet->m_graphicsTextItem);
    }
    else
    {
        //qDebug("Id present. Skipping");
        return;
    }
    packet->m_graphicsItem->hide();
    packet->m_graphicsTextItem->hide();

    m_packetsToAnimate[animId] = packet;
}

void
AnimatorScene::resetMetaInfo()
{
    for(int i = 0; i < m_animatedMetaInfo.size(); ++i)
    {
        if(m_animatedMetaInfo[i]->scene())
        {
            removeItem(m_animatedMetaInfo[i]);
        }
    }
    m_animatedMetaInfo.clear();
}


void
AnimatorScene::repairP2pLinks()
{
    for(LinkManager::NodeIdAnimLinkVectorMap_t::const_iterator i = LinkManager::getInstance()->getLinks()->begin();
        i != LinkManager::getInstance()->getLinks()->end();
        ++i)
    {
        LinkManager::AnimLinkVector_t v = i->second;
        for(LinkManager::AnimLinkVector_t::const_iterator j = v.begin();
            j != v.end() ;
            ++j)
        {
            AnimLink * link = *j;
            QPointF aNodeLoc = getNodeLoc(link->m_fromId);
            QPointF bNodeLoc = getNodeLoc(link->m_toId);
            QPointF invertedANodeLoc = QPointF(aNodeLoc.x(), getHeight() - aNodeLoc.y());
            QPointF invertedBNodeLoc = QPointF(bNodeLoc.x(), getHeight() - bNodeLoc.y());

            link->setLine(QLineF(invertedANodeLoc, invertedBNodeLoc));
        }
    }
}

void
AnimatorScene::showAllLinkItems(bool show)
{
    for(LinkManager::NodeIdAnimLinkVectorMap_t::const_iterator i = LinkManager::getInstance()->getLinks()->begin();
        i != LinkManager::getInstance()->getLinks()->end();
        ++i)
    {
        LinkManager::AnimLinkVector_t v = i->second;
        for(LinkManager::AnimLinkVector_t::const_iterator j = v.begin();
            j != v.end() ;
            ++j)
        {
            (*j)->setVisible(show);
        }
    }
}


void
AnimatorScene::showAllNodeItems(bool show)
{
    Q_FOREACH(const AnimNodeMgr::AnimNodeMap_t::value_type& nodePair, *AnimNodeMgr::getInstance()->getAnimNodes())
        showNode(nodePair.first, show);
}

void
AnimatorScene::showNode(uint32_t nodeId, bool show)
{
    AnimNode* node = AnimNodeMgr::getInstance()->getNode(nodeId);

    node->getGraphicsItem()->setVisible(show);
    node->getNodeIdTextItem()->setVisible(show && m_showNodeId);

    if(node->getEnergySource().getType() != AnimNodeEnergySource::GRID)
    {
        Q_ASSERT(node->getBatteryItem());
        node->getBatteryItem()->setVisible(show && m_showBatteryCapacity);
    }
}

void
AnimatorScene::resetInterfaceTextTop()
{
    m_leftTop = 0;
    m_righTop = 0;
}

void
AnimatorScene::resetInterfaceTexts()
{
    resetInterfaceTextTop();
    for(AnimInterfaceTextVector_t::const_iterator i = m_interfaceATexts.begin();
        i != m_interfaceATexts.end();
        ++i)
    {
        AnimInterfaceText * text = *i;
        QGraphicsLineItem * l = text->getLine();
        if(l)
        {
            removeItem(l);
        }
        removeItem(*i);
        delete (*i);
    }
    m_interfaceATexts.clear();
    for(AnimInterfaceTextVector_t::const_iterator i = m_interfaceBTexts.begin();
        i != m_interfaceBTexts.end();
        ++i)
    {
        AnimInterfaceText * text = *i;
        QGraphicsLineItem * l = text->getLine();
        if(l)
        {
            removeItem(l);
        }
        removeItem(*i);
        delete (*i);
    }
    m_interfaceBTexts.clear();
    update();
}

QRectF
AnimatorScene::getNodeRect(uint32_t nodeId)
{
    AnimNode * node = AnimNodeMgr::getInstance()->getNode(nodeId);
    return node->getGraphicsItem()->boundingRect();
}

QList <QGraphicsItem *>
AnimatorScene::getInterfaceTextCollisionList(AnimInterfaceText * text)
{
    QList <QGraphicsItem *> l = text->collidingItems();
    QList <QGraphicsItem *> collidingList;
    for (QList <QGraphicsItem *>::const_iterator i = l.begin();
         i != l.end();
         ++i)
    {
        QGraphicsItem * item = *i;
        if(item->type() == (ANIMINTERFACE_TEXT_TYPE))
        {
            collidingList.append(item);
        }
    }
    return collidingList;
}


void
AnimatorScene::repositionInterfaceText(AnimInterfaceText *textItem)
{
    bool isRight = textItem->pos().x() > (m_width/2);
    QPointF oldPos = textItem->pos();
    QFontMetrics fm(font());
    QPointF newPos;
    if(!isRight)
    {
        textItem->setLeftAligned(false);
        qreal y = m_leftTop + 1.5 * fm.height()/AnimatorView::getInstance()->transform().m11();
        newPos = QPointF(-fm.width(textItem->getText())/AnimatorView::getInstance()->transform().m11(), y);
        m_leftTop = newPos.y();
    }
    else
    {
        textItem->setLeftAligned(true);
        qreal y = m_righTop + 1.5 * fm.height()/AnimatorView::getInstance()->transform().m11();
        newPos = QPointF(m_width + fm.width(textItem->getText())/AnimatorView::getInstance()->transform().m11(), y);
        m_righTop = newPos.y();
    }
    textItem->setPos(newPos);
    QLineF l(oldPos, newPos);
    if(textItem->setLine(l))
    {
        addItem(textItem->getLine());
    }

}

void
AnimatorScene::removeInterfaceTextCollision()
{


    for(AnimInterfaceTextVector_t::iterator i = m_interfaceATexts.begin();
        i != m_interfaceATexts.end();
        ++i)
    {
        AnimInterfaceText * text = *i;
        QList <QGraphicsItem *> collidingList = getInterfaceTextCollisionList(text);
        //qDebug(collidingList.count(), "CL count");
        if(collidingList.count())
        {
            repositionInterfaceText(text);
        }

    }
    for(AnimInterfaceTextVector_t::iterator i = m_interfaceBTexts.begin();
        i != m_interfaceBTexts.end();
        ++i)
    {
        AnimInterfaceText * text = *i;
        QList <QGraphicsItem *> collidingList = getInterfaceTextCollisionList(text);
        //qDebug(collidingList.count(), "CL count");
        if(collidingList.count())
        {
            repositionInterfaceText(text);
        }
    }

}

void
AnimatorScene::printAnimPackets()
{
    if(AnimPktMgr::getInstance()->isEmpty())
    {
        return;
    }
    for (AnimPktMgr::AnimPacketMap_t::const_iterator i = AnimPktMgr::getInstance()->getPackets()->begin();
         i != AnimPktMgr::getInstance()->getPackets()->end();
         ++i)
    {
        qDebug(i->second);

    }

}


void
AnimatorScene::printPacketsToAnimate()
{

    for(PacketToAnimate_t::const_iterator i = m_packetsToAnimate.begin();
        i != m_packetsToAnimate.end();
        ++i)
    {
        qDebug(i->second);
    }

}

AnimPktMgr::AnimPacketMap_t::const_iterator &
AnimatorScene::printAnimPackets(AnimPktMgr::AnimPacketMap_t::const_iterator iter)
{

    qDebug(iter->second);
    return ++iter;
}


AnimInterfaceText::AnimInterfaceText(QString description, bool leftAligned):QGraphicsTextItem(description),
    m_leftAligned(leftAligned),
    m_line(0)
{
    setFlag(QGraphicsItem::ItemIgnoresTransformations);
    setZValue(ANIMINTERFACE_TEXT_TYPE);
}

AnimInterfaceText::~AnimInterfaceText()
{
    if(m_line)
    {
        delete m_line;
    }
}

void
AnimInterfaceText::setLeftAligned(bool leftAligned)
{
    m_leftAligned = leftAligned;
}

QPainterPath
AnimInterfaceText::shape() const
{
    QPainterPath p;
    QFontMetrics fm(font());
    QRectF r(0, 0, fm.width(getText())/AnimatorView::getInstance()->transform().m11(),
             fm.height()/AnimatorView::getInstance()->transform().m11());
    p.addRect(r);
    return p;
}

QString
AnimInterfaceText::getText() const
{
    QStringList parts = toPlainText().split('~');
    if(m_mode == AnimInterfaceText::IPV4)
    {
        return parts.at(0);
    }
    if(m_mode == AnimInterfaceText::MAC)
    {
        if(parts.length() != 2)
        {
            return "";
        }
        return parts.at(1);
    }
    return toPlainText();
}


void
AnimInterfaceText::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{

    Q_UNUSED(option)
    Q_UNUSED(widget)
    if(m_leftAligned)
    {
        QFontMetrics fm = painter->fontMetrics();
        QPointF leftAlignPoint = QPointF(-fm.width(getText()), 0);
        painter->save();
        painter->translate(leftAlignPoint);
        painter->drawText(QPointF(0, 0), getText());
        //QGraphicsTextItem::paint(painter, option, widget);
        painter->restore();
    }
    else
    {
        //QGraphicsTextItem::paint(painter, option, widget);
        painter->drawText(QPointF(0, 0), getText());

    }
}

bool
AnimInterfaceText::setLine(QLineF l)
{
    bool newLine = false;
    if(!m_line)
    {
        m_line = new QGraphicsLineItem;
        newLine = true;
    }
    QPen p;

    p.setColor(QColor(0, 0, 255, 50));
    m_line->setPen(p);
    m_line->setLine(l);
    return newLine;
}

QGraphicsLineItem *
AnimInterfaceText::getLine()
{
    return m_line;
}

void
AnimInterfaceText::setMode(bool showIpv4, bool showMac)
{
    if(!showIpv4 && !showMac)
    {
        m_mode = AnimInterfaceText::NONE;
    }
    if(showIpv4 && !showMac)
    {
        m_mode = AnimInterfaceText::IPV4;
    }
    if(!showIpv4 && showMac)
    {
        m_mode = AnimInterfaceText::MAC;
    }
    if(showIpv4 && showMac)
    {
        m_mode = AnimInterfaceText::BOTH;
    }
}


} // namespace netanim

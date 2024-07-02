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

#include "animator/animlink.h"
#include "animator/animatorconstants.h"
#include "animator/animnode.h"
#include "animator/animatorscene.h"
#include "debug/xdebug.h"


#include <QPainter>

namespace netanim {

LinkManager       * pLinkManager = 0;

AnimLink::AnimLink(uint32_t fromId, uint32_t toId,
                   QLineF line, QString pointADescription,
                   QString pointBDescription, QString linkDescription, bool p2p):QGraphicsLineItem(line),
                   m_fromId(fromId), m_toId (toId), m_p2p(p2p)
{
    m_pointADescription = 0;
    m_pointBDescription = 0;
    m_currentLinkDescription = 0;




    if (pointADescription != "")
    {
        m_pointADescription = new QString(pointADescription);
        QStringList parts = (*m_pointADescription).split('~');
        if(parts.count() == 2)
        {
            AnimNodeMgr::getInstance()->addIpv4Address(fromId, parts.at(0));
            AnimNodeMgr::getInstance()->addMacAddress(fromId, parts.at(1));
        }

    }
    if (pointBDescription != "")
    {
        m_pointBDescription = new QString(pointBDescription);
        QStringList parts = (*m_pointBDescription).split('~');
        if(parts.count() == 2)
        {
            AnimNodeMgr::getInstance()->addIpv4Address(fromId, parts.at(0));
            AnimNodeMgr::getInstance()->addMacAddress(fromId, parts.at(1));
        }
    }
    m_originalLinkDescription = new QString("");
    if (linkDescription != "")
    {
        m_currentLinkDescription = new QString(linkDescription);
        *m_originalLinkDescription = linkDescription;
    }
}

AnimLink::~AnimLink()
{
    if (m_pointADescription)
        delete m_pointADescription;
    if (m_pointBDescription)
        delete m_pointBDescription;
    if (m_currentLinkDescription)
        delete m_currentLinkDescription;
    if (m_originalLinkDescription)
        delete m_originalLinkDescription;

}


QPointF
AnimLink::getLinkDescriptionCenter(QPainter * painter , QPointF * offset)
{
    QFontMetrics fm = painter->fontMetrics();
    qreal x = (line().length() - fm.width(m_currentLinkDescription->toAscii().data()))/2;
    QPointF pOffset = line().p1().x() < line().p2().x()? line().p1():line().p2();
    *offset = pOffset;
    QPointF p = QPointF(x, -1);
    return p;
}

void
AnimLink::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if(AnimatorScene::getInstance()->isRoutePathTracking())
    {
        setVisible(false);
        return;
    }

    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->drawLine(line());
    QFont font;
    font.setPointSize(3);
    QPen pen;
    pen.setColor(Qt::darkRed);
    painter->setFont(font);
    painter->setPen(pen);
    if (m_currentLinkDescription)
    {
        QPointF offset;
        QPointF center = getLinkDescriptionCenter(painter, &offset);
        painter->save();
        painter->translate(offset);

        if (offset != line().p1())
        {
            painter->rotate(180-line().angle());
        }
        else
        {
            painter->rotate(-line().angle());
        }
        painter->drawText(center, *m_currentLinkDescription);
        painter->restore();
    }

    if(!m_p2p)
    {
        m_interfacePosA = line().p1();
    }

    QList <QGraphicsItem *> collidingList = collidingItems();
    for (QList <QGraphicsItem *>::const_iterator i = collidingList.begin();
         i != collidingList.end();
         ++i)
    {
        QGraphicsItem * item = *i;
        if(item->type() == ANIMNODE_ELLIPSE_TYPE)
        {
            AnimNodeEllipse * node = qgraphicsitem_cast <AnimNodeEllipse *> (item);
            qreal radius = node->rect().width()/2;
            QPointF center = node->rect().center();


            QPointF other;
            if ((center.x() == line().x1()) && (center.y() == line().y1()))
            {
                other = line().p2();
                QLineF l (center, other);
                if(!m_p2p)
                {
                    l = QLineF(center, QPointF(center.x(), 0));
                }

                l.setLength(radius);
                QBrush br(Qt::lightGray);
                painter->save();
                painter->setBrush(br);
                painter->drawEllipse(l.p1(), radius/4, radius/4);
                m_interfacePosA = l.p1();
                painter->restore();
            }
            else
            {
                other = line().p1();
                QLineF l (center, other);
                l.setLength(radius);
                QBrush br(Qt::lightGray);
                painter->save();
                painter->setBrush(br);
                painter->drawEllipse(l.p2(), radius/4, radius/4);
                m_interfacePosB = l.p2();
                painter->restore();

            }


        }
    }
}


void
AnimLink::updateCurrentLinkDescription(QString linkDescription)
{
    if(!m_currentLinkDescription)
    {
        m_currentLinkDescription = new QString(linkDescription);
        return;
    }
    *m_currentLinkDescription = linkDescription;
}

void
AnimLink::resetCurrentLinkDescription()
{
    if (m_originalLinkDescription)
    {
        m_currentLinkDescription = m_originalLinkDescription;
    }
}

QString
AnimLink::toString()
{
    QString s = QString("From:") + QString::number(m_fromId) + " To:" + QString::number(m_toId);
    return s;
}

QPointF
AnimLink::getInterfacePosA()
{
    return m_interfacePosA;
}

QPointF
AnimLink::getInterfacePosB()
{
    return m_interfacePosB;
}

QString
AnimLink::getInterfaceADescription()
{
    if (m_pointADescription)
    {
        return *m_pointADescription;
    }
    else
    {
        return "";
    }

}

QString
AnimLink::getInterfaceBDescription()
{
    if (m_pointBDescription)
    {
        return *m_pointBDescription;
    }
    else
    {
        return "";
    }
}


LinkManager::LinkManager()
{

}

LinkManager *
LinkManager::getInstance()
{
    if(!pLinkManager)
    {
        pLinkManager = new LinkManager;
    }
    return pLinkManager;
}

AnimLink *
LinkManager::addLink(uint32_t fromId, uint32_t toId, QLineF line, QString pointADescription, QString pointBDescription, QString linkDescription, bool p2p)
{
    AnimLink * item = new AnimLink(fromId, toId, line, pointADescription, pointBDescription, linkDescription, p2p);
    if(m_pointToPointLinks.find(fromId) == m_pointToPointLinks.end())
    {
        LinkManager::AnimLinkVector_t v;
        v.push_back(item);
        m_pointToPointLinks[fromId] = v;
        return item;
    }
    else
    {
        LinkManager::AnimLinkVector_t & v = m_pointToPointLinks[fromId];
        v.push_back(item);
        return item;
    }
}

LinkManager::NodeIdAnimLinkVectorMap_t *
LinkManager::getLinks()
{
    return &m_pointToPointLinks;
}

AnimLink *
LinkManager::getAnimLink(uint32_t fromId, uint32_t toId)
{
    AnimLink * theLink = 0;
    for(LinkManager::NodeIdAnimLinkVectorMap_t::const_iterator i = m_pointToPointLinks.begin();
        i != m_pointToPointLinks.end();
        ++i)
    {
        if(fromId != i->first)
        {
            continue;
        }
        LinkManager::AnimLinkVector_t v = i->second;
        for(LinkManager::AnimLinkVector_t::const_iterator j = v.begin();
            j != v.end();
            ++j)
        {
            AnimLink * link = *j;
            if ((link->m_fromId == fromId && link->m_toId == toId) ||
                (link->m_fromId == toId && link->m_toId == fromId))
                return link;
        }
    }
    return theLink;

}

void
LinkManager::systemReset()
{
    // remove links
    for(LinkManager::NodeIdAnimLinkVectorMap_t::const_iterator i = m_pointToPointLinks.begin();
        i != m_pointToPointLinks.end();
        ++i)
    {
        LinkManager::AnimLinkVector_t v = i->second;
        for(LinkManager::AnimLinkVector_t::const_iterator j = v.begin();
            j != v.end();
            ++j)
        {
            AnimatorScene::getInstance()->removeItem(*j);
            delete (*j);
        }
    }
    m_pointToPointLinks.clear();

}



} // namespace netanim

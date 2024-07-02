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


#include "animxmlparser.h"
#include "debug/xdebug.h"
#include "animatormode.h"

#include <QFile>
#include <QRegExp>

namespace netanim {

Animxmlparser::Animxmlparser(QString traceFileName):
    m_traceFileName(traceFileName),
    m_parsingComplete(false),
    m_reader(0),
    m_maxSimulationTime(0),
    m_fileIsValid(true)
{
    m_version = 0;
    if(m_traceFileName == "")
        return;

    m_traceFile = new QFile(m_traceFileName);
    if (!m_traceFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug(QString("Critical:Trace file is invalid"));
        m_fileIsValid = false;
        return;
    }
    //qDebug(m_traceFileName);
    m_reader = new QXmlStreamReader(m_traceFile);
}

Animxmlparser::~Animxmlparser()
{
    if(m_traceFile)
        delete m_traceFile;
    if(m_reader)
        delete m_reader;
}

void
Animxmlparser::searchForVersion()
{
    QFile * f = new QFile(m_traceFileName);
    if(f->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString firstLine = QString(f->readLine());
        int startIndex = 0;
        int endIndex = 0;
        QString versionField = VERSION_FIELD_DEFAULT;
        startIndex = firstLine.indexOf(versionField);
        endIndex = firstLine.lastIndexOf("\"");
        if((startIndex != -1) && (endIndex > startIndex))
          {
            int adjustedStartIndex = startIndex + versionField.length();
            QString v = firstLine.mid(adjustedStartIndex, endIndex-adjustedStartIndex);
            m_version = v.toDouble();
          }
        f->close();
        delete f;
    }
}

uint64_t
Animxmlparser::getRxCount()
{
    searchForVersion();
    uint64_t count = 0;
    QFile * f = new QFile(m_traceFileName);
    if(f->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString allContent = QString(f->readAll());
        int j = 0;
        QString searchString = " toId=";
        if(m_version >= 3.102)
          searchString = " tId";

        while ((j = allContent.indexOf(searchString, j)) != -1) {
            ++j;
            ++count;
        }
        f->close();
        delete f;
        //qDebug (QString::number(count));
        return count;
    }
    return count;
}

bool
Animxmlparser::isFileValid()
{
    return m_fileIsValid;
}

bool
Animxmlparser::isParsingComplete()
{
    return m_parsingComplete;
}


void
Animxmlparser::doParse()
{
    qreal currentNodeSize = AnimatorMode::getInstance()->getCurrentNodeSize();
    uint64_t parsedElementCount = 0;
    while(!isParsingComplete())
    {
        if(AnimatorMode::getInstance()->keepAppResponsive())
        {
            AnimatorMode::getInstance()->setParsingCount(parsedElementCount);

        }
       ParsedElement parsedElement = parseNext();
       switch(parsedElement.type)
       {
           case XML_ANIM:
           {
               AnimatorMode::getInstance()->setVersion(parsedElement.version);
               //qDebug(QString("XML Version:") + QString::number(version));
               break;
           }

           case XML_TOPOLOGY:
           {
               qreal maxValue = qMax(parsedElement.topo_width, parsedElement.topo_height);
               AnimatorScene::getInstance()->setWidth(maxValue);
               AnimatorScene::getInstance()->setHeight(maxValue);
               break;
           }
           case XML_NODE:
           {
               QColor * pColor = 0;
               uint8_t r = parsedElement.node_r;
               uint8_t g = parsedElement.node_g;
               uint8_t b = parsedElement.node_b;
               if (!(r == 255 && g == 0 && b == 0)) //If it is RED ignore it
               {
                   if(m_version < 3.102)  // Color is not supported in version < 3.101
                   {
                       pColor = new QColor(255, 0, 0);
                   }
                   else
                   {
                       pColor = new QColor(r, g, b);
                   }
               }

               AnimatorScene::getInstance()->addNode(parsedElement.nodeId,
                              parsedElement.node_x,
                              parsedElement.node_y,
                              CIRCLE,
                              currentNodeSize,
                              currentNodeSize,
                              parsedElement.nodeDescription, pColor, parsedElement.hasColorUpdate,
                              parsedElement.node_batteryCapacity, parsedElement.hasBattery);
               break;
           }
           case XML_LINK:
           {
               AnimatorScene::getInstance()->addLink(parsedElement.link_fromId,
                              parsedElement.link_toId,
                              parsedElement.fromNodeDescription,
                              parsedElement.toNodeDescription,
                              parsedElement.linkDescription);
               break;
           }
           case XML_NONP2P_LINK:
           {
                AnimatorScene::getInstance()->addLink(parsedElement.link_fromId,
                               parsedElement.link_fromId,
                               parsedElement.fromNodeDescription,
                               "",
                               "",
                               false);

           }
           case XML_LINKUPDATE:
           {

               AnimatorScene::getInstance()->updateLink(parsedElement.link_fromId, parsedElement.link_toId,
                              parsedElement.updateTime, parsedElement.linkDescription);
               break;
           }

           case XML_NODEUPDATE:
           {
               AnimatorScene::getInstance()->updateNode(parsedElement.updateTime, parsedElement.nodeId, parsedElement.node_r,
                              parsedElement.node_g, parsedElement.node_b, parsedElement.nodeDescription,
                              parsedElement.visible, parsedElement.hasColorUpdate, parsedElement.node_batteryCapacity,
                              parsedElement.hasBattery);
               break;
           }

           case XML_PACKET_RX:
           {
               AnimatorScene::getInstance()->addPacketRx(parsedElement.packetrx_fromId,
                              parsedElement.packetrx_fbTx,
                              parsedElement.packetrx_lbTx,
                              parsedElement.packetrx_toId,
                              parsedElement.packetrx_fbRx,
                              parsedElement.packetrx_lbRx,
                              parsedElement.type,
                              parsedElement.meta_info);
               ++parsedElementCount;
               break;

           }
           case XML_WPACKET_RX:
           {
               AnimatorScene::getInstance()->addPacketRx(parsedElement.packetrx_fromId,
                              parsedElement.packetrx_fbTx,
                              parsedElement.packetrx_lbTx,
                              parsedElement.packetrx_toId,
                              parsedElement.packetrx_fbRx,
                              parsedElement.packetrx_lbRx,
                              parsedElement.type,
                              parsedElement.meta_info);
               ++parsedElementCount;
               AnimatorMode::getInstance()->setWPacketDetected();
               break;
           }
           case XML_INVALID:
           default:
           {
               //qDebug("Invalid XML element");
           }
       } //switch
    } // while loop
}

ParsedElement
Animxmlparser::parseNext()
{
    ParsedElement parsedElement;
    parsedElement.type = XML_INVALID;
    parsedElement.version = m_version;

    if(m_reader->atEnd() || m_reader->hasError())
    {
        m_parsingComplete = true;
        m_traceFile->close();
        return parsedElement;
    }



    QXmlStreamReader::TokenType token =  m_reader->readNext();
    if(token == QXmlStreamReader::StartDocument)
        return parsedElement;

    if(token == QXmlStreamReader::StartElement)
    {
        if(m_reader->name() == "anim")
        {
            parsedElement = parseAnim();
        }
        if(m_reader->name() == "topology")
        {
            parsedElement = parseTopology();
        }
        if(m_reader->name() == "node")
        {
            parsedElement = parseNode();
        }
        if(m_reader->name() == "packet")
        {
            parsedElement = parsePacket();
        }
        if(m_reader->name() == "p")
        {
            parsedElement = parseP();
        }
        if(m_reader->name() == "wp")
        { 
            parsedElement = parseWp();
        }
        if(m_reader->name() == "wpacket")
        {
            parsedElement = parseWPacket();
        }
        if(m_reader->name() == "link")
        {
            parsedElement = parseLink();
        }
        if(m_reader->name() == "nonp2plinkproperties")
        {
            parsedElement = parseNonP2pLink();
        }
        if(m_reader->name() == "linkupdate")
        {
            parsedElement = parseLinkUpdate();
        }
        if(m_reader->name() == "nodeupdate")
        {
            parsedElement = parseNodeUpdate();
        }
        //qDebug(m_reader->name().toString());
    }

    if(m_reader->atEnd())
    {
        m_parsingComplete = true;
        m_traceFile->close();
    }
    return parsedElement;
}


ParsedElement
Animxmlparser::parseAnim()
{
    ParsedElement parsedElement;
    parsedElement.type = XML_ANIM;
    parsedElement.version = m_version;
    QString v = m_reader->attributes().value("ver").toString();
    if(!v.contains("netanim-"))
        return parsedElement;
    v = v.replace ("netanim-","");
    m_version = v.toDouble();
    parsedElement.version = m_version; 
    //qDebug(QString::number(m_version));
    return parsedElement;
}

ParsedElement
Animxmlparser::parseTopology()
{
    ParsedElement parsedElement;
    parsedElement.type = XML_TOPOLOGY;
    parsedElement.topo_width = m_reader->attributes().value("maxX").toString().toDouble();
    parsedElement.topo_height = m_reader->attributes().value("maxY").toString().toDouble();
    return parsedElement;

}

ParsedElement
Animxmlparser::parseLink()
{
    ParsedElement parsedElement;
    parsedElement.type = XML_LINK;
    parsedElement.link_fromId = m_reader->attributes().value("fromId").toString().toUInt();
    parsedElement.link_toId = m_reader->attributes().value("toId").toString().toDouble();
    parsedElement.fromNodeDescription = m_reader->attributes().value("fd").toString();
    parsedElement.toNodeDescription = m_reader->attributes().value("td").toString();
    parsedElement.linkDescription = m_reader->attributes().value("ld").toString();
    return parsedElement;

}


ParsedElement
Animxmlparser::parseNonP2pLink()
{
    ParsedElement parsedElement;
    parsedElement.type = XML_NONP2P_LINK;
    parsedElement.link_fromId = m_reader->attributes().value("id").toString().toUInt();
    parsedElement.fromNodeDescription = m_reader->attributes().value("ipv4Address").toString();
    return parsedElement;
}

ParsedElement
Animxmlparser::parseLinkUpdate()
{
    ParsedElement parsedElement;
    parsedElement.type = XML_LINKUPDATE;
    parsedElement.link_fromId = m_reader->attributes().value("fromId").toString().toUInt();
    parsedElement.link_toId = m_reader->attributes().value("toId").toString().toDouble();
    parsedElement.linkDescription = m_reader->attributes().value("ld").toString();
    parsedElement.updateTime = m_reader->attributes().value("t").toString().toDouble();
    return parsedElement;

}

ParsedElement
Animxmlparser::parseNode()
{
    ParsedElement parsedElement;
    parsedElement.type = XML_NODE;
    parsedElement.nodeId = m_reader->attributes().value("id").toString().toUInt();
    parsedElement.node_x = m_reader->attributes().value("locX").toString().toDouble();
    parsedElement.node_y = m_reader->attributes().value("locY").toString().toDouble();
    parsedElement.node_batteryCapacity = m_reader->attributes().value("rc").toString().toDouble();
    parsedElement.nodeDescription = m_reader->attributes().value("descr").toString();
    parsedElement.node_r = m_reader->attributes().value("r").toString().toUInt();
    parsedElement.node_g = m_reader->attributes().value("g").toString().toUInt();
    parsedElement.node_b = m_reader->attributes().value("b").toString().toUInt();
    parsedElement.hasColorUpdate = !m_reader->attributes().value("r").isEmpty();
    parsedElement.hasBattery = !m_reader->attributes().value("rc").isEmpty();
    return parsedElement;
}

ParsedElement
Animxmlparser::parseNodeUpdate()
{
    ParsedElement parsedElement;
    parsedElement.type = XML_NODEUPDATE;
    parsedElement.nodeId = m_reader->attributes().value("id").toString().toUInt();
    parsedElement.nodeDescription = m_reader->attributes().value("descr").toString();
    parsedElement.node_r = m_reader->attributes().value("r").toString().toUInt();
    parsedElement.node_g = m_reader->attributes().value("g").toString().toUInt();
    parsedElement.node_b = m_reader->attributes().value("b").toString().toUInt();
    parsedElement.visible = m_reader->attributes().value("visible").toString().toInt();
    parsedElement.updateTime = m_reader->attributes().value("t").toString().toDouble();
    parsedElement.hasColorUpdate = !m_reader->attributes().value("r").isEmpty();
    parsedElement.node_batteryCapacity = m_reader->attributes().value("rc").toString().toDouble();
    parsedElement.hasBattery = !m_reader->attributes().value("rc").isEmpty();
    return parsedElement;
}

void
Animxmlparser::parseGeneric(ParsedElement & parsedElement)
{
    parsedElement.packetrx_fromId = m_reader->attributes().value("fId").toString().toUInt();
    parsedElement.packetrx_fbTx = m_reader->attributes().value("fbTx").toString().toDouble();
    parsedElement.packetrx_lbTx = m_reader->attributes().value("lbTx").toString().toDouble();
    m_maxSimulationTime = std::max(m_maxSimulationTime,parsedElement.packetrx_lbTx);
    parsedElement.packetrx_toId = m_reader->attributes().value("tId").toString().toUInt();
    parsedElement.packetrx_fbRx = m_reader->attributes().value("fbRx").toString().toDouble();
    parsedElement.packetrx_lbRx = m_reader->attributes().value("lbRx").toString().toDouble();
    m_maxSimulationTime = std::max(m_maxSimulationTime, parsedElement.packetrx_lbRx);
    parsedElement.meta_info = m_reader->attributes().value("meta-info").toString();
    if(parsedElement.meta_info == "")
    {
        parsedElement.meta_info = "null";
    }
}

ParsedElement
Animxmlparser::parseP()
{
    ParsedElement parsedElement;
    parsedElement.type = XML_PACKET_RX;
    parseGeneric(parsedElement);
    return parsedElement;
}

ParsedElement
Animxmlparser::parseWp()
{
    ParsedElement parsedElement;
    parsedElement.type = XML_WPACKET_RX;
    parseGeneric(parsedElement);
    return parsedElement;
}

ParsedElement
Animxmlparser::parsePacket()
{
    ParsedElement parsedElement;
    parsedElement.type = XML_PACKET_RX;
    parsedElement.packetrx_fromId = m_reader->attributes().value("fromId").toString().toUInt();
    parsedElement.packetrx_fbTx = m_reader->attributes().value("fbTx").toString().toDouble();
    parsedElement.packetrx_lbTx = m_reader->attributes().value("lbTx").toString().toDouble();
    parsedElement.meta_info = "null";
    m_maxSimulationTime = std::max(m_maxSimulationTime,parsedElement.packetrx_lbTx);
    while(m_reader->name() != "rx")
        m_reader->readNext();

    if(m_reader->atEnd() || m_reader->hasError())
    {
        m_parsingComplete = true;
        m_traceFile->close();
        return parsedElement;
    }

    parsedElement.packetrx_toId = m_reader->attributes().value("toId").toString().toUInt();
    parsedElement.packetrx_fbRx = m_reader->attributes().value("fbRx").toString().toDouble();
    parsedElement.packetrx_lbRx = m_reader->attributes().value("lbRx").toString().toDouble();
    m_maxSimulationTime = std::max(m_maxSimulationTime, parsedElement.packetrx_lbRx);

    while(m_reader->name() == "rx")
        m_reader->readNext();
    if(m_reader->name() == "packet")
        return parsedElement;
    m_reader->readNext();
    if(m_reader->name() != "meta")
        return parsedElement;
    parsedElement.meta_info = m_reader->attributes().value("info").toString();
    //qDebug(parsedElement.meta_info);
    return parsedElement;

}

ParsedElement
Animxmlparser::parseWPacket()
{

    ParsedElement parsedElement;
    parsedElement.type = XML_WPACKET_RX;
    parsedElement.packetrx_fromId = m_reader->attributes().value("fromId").toString().toUInt();
    parsedElement.packetrx_fbTx = m_reader->attributes().value("fbTx").toString().toDouble();
    parsedElement.packetrx_lbTx = m_reader->attributes().value("lbTx").toString().toDouble();
    parsedElement.meta_info = "null";
    m_maxSimulationTime = std::max(m_maxSimulationTime,parsedElement.packetrx_lbTx);
    while(m_reader->name() != "rx")
        m_reader->readNext();
    if(m_reader->atEnd() || m_reader->hasError())
    {
        m_parsingComplete = true;
        m_traceFile->close();
        return parsedElement;
    }

    //qDebug(m_reader->name().toString()+"parseWpacket");
    parsedElement.packetrx_toId = m_reader->attributes().value("toId").toString().toUInt();
    parsedElement.packetrx_fbRx = m_reader->attributes().value("fbRx").toString().toDouble();
    parsedElement.packetrx_lbRx = m_reader->attributes().value("lbRx").toString().toDouble();
    m_maxSimulationTime = std::max(m_maxSimulationTime, parsedElement.packetrx_lbRx);
    while(m_reader->name() == "rx")
        m_reader->readNext();
    if(m_reader->name() == "wpacket")
        return parsedElement;
    m_reader->readNext();
    if(m_reader->name() != "meta")
        return parsedElement;
    parsedElement.meta_info = m_reader->attributes().value("info").toString();
    //qDebug(parsedElement.meta_info);
    return parsedElement;

}

double
Animxmlparser::getMaxSimulationTime()
{
    return m_maxSimulationTime;
}


} // namespace netanim

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


#ifndef ANIMXMLPARSER_H
#define ANIMXMLPARSER_H

#include "main/common.h"
#include <QDialog>
#include <QFile>
#include <QtCore/QXmlStreamReader>
#include <QLabel>
#include <QTableWidget>

namespace netanim {

enum ParsedElementType
{
    XML_INVALID,
    XML_ANIM,
    XML_TOPOLOGY,
    XML_NODE,
    XML_LINK,
    XML_NONP2P_LINK,
    XML_PACKET_RX,
    XML_WPACKET_RX,
    XML_LINKUPDATE,
    XML_NODEUPDATE
};

struct ParsedElement
{
    ParsedElementType type;

    // Anim
    double version;

    // Topology
    qreal topo_width;
    qreal topo_height;

    // Node

    uint32_t nodeId;
    qreal node_x;
    qreal node_y;
    qreal node_batteryCapacity;
    uint8_t node_r;
    uint8_t node_g;
    uint8_t node_b;
    bool visible;

    // Link

    uint32_t link_fromId;
    uint32_t link_toId;

    // Link description

    QString fromNodeDescription;
    QString toNodeDescription;
    QString linkDescription;

    // Packet Rx
    double packetrx_fbTx;
    double packetrx_lbTx;
    uint32_t packetrx_fromId;
    double packetrx_toId;
    double packetrx_fbRx;
    double packetrx_lbRx;

    //meta-info
    QString meta_info;
    QString nodeDescription;


    // Update time
    double updateTime;

    // Has Color update
    bool hasColorUpdate;
    bool hasBattery; //!< Has battery with possible capacity
};


class Animxmlparser
{
public:
    Animxmlparser(QString traceFileName);
    ~Animxmlparser();
    ParsedElement parseNext();
    bool isParsingComplete();
    double getMaxSimulationTime();
    bool isFileValid();
    uint64_t getRxCount();
    void doParse();


private:
    QString m_traceFileName;
    bool m_parsingComplete;
    QXmlStreamReader * m_reader;
    QFile * m_traceFile;
    double m_maxSimulationTime;
    bool m_fileIsValid;
    double m_version;

    ParsedElement parseAnim();
    ParsedElement parseTopology();
    ParsedElement parseNode();
    ParsedElement parseLink();
    ParsedElement parseNonP2pLink();
    ParsedElement parsePacket();
    ParsedElement parseWPacket();
    ParsedElement parseLinkUpdate();
    ParsedElement parseNodeUpdate();
    ParsedElement parseP();
    ParsedElement parseWp();
    void parseGeneric(ParsedElement &);

    void searchForVersion();
};

} // namespace netanim
#endif // ANIMXMLPARSER_H

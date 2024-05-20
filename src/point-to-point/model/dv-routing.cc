/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2023 NUS
 *
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
 * Authors: Chahwan Song <songch@comp.nus.edu.sg>
 */

#include "ns3/dv-routing.h"

#include "assert.h"
#include "ns3/assert.h"
#include "ns3/event-id.h"
#include "ns3/ipv4-header.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/settings.h"
#include "ns3/simulator.h"

// NS_LOG_COMPONENT_DEFINE("DVRouting");

namespace ns3 {

    /*---- DVUdp-Tag -----*/
    DVUdpTag::DVUdpTag() {}
    DVUdpTag::~DVUdpTag() {}
    TypeId DVUdpTag::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::DVUdpTag").SetParent<Tag>().AddConstructor<DVUdpTag>();
        return tid;
    }
    void DVUdpTag::SetPathId(uint32_t pathId) { m_pathId = pathId; }
    uint32_t DVUdpTag::GetPathId(void) const { return m_pathId; }
    void DVUdpTag::SetHopCount(uint32_t hopCount) { m_hopCount = hopCount; }
    uint32_t DVUdpTag::GetHopCount(void) const { return m_hopCount; }
    void DVUdpTag::SetSrcRouteEnable(bool SrcRouteEnable) {
        if (SrcRouteEnable){
            m_SrcRouteEnable = 1;
        }
        else{
            m_SrcRouteEnable = 0;
        }

    }
    uint8_t DVUdpTag::GetSrcRouteEnable(void) const { return m_SrcRouteEnable; }
    TypeId DVUdpTag::GetInstanceTypeId(void) const { return GetTypeId(); }
    uint32_t DVUdpTag::GetSerializedSize(void) const {
        return sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    }
    void DVUdpTag::Serialize(TagBuffer i) const {
        i.WriteU32(m_pathId);
        i.WriteU32(m_hopCount);
        i.WriteU8(m_SrcRouteEnable);
    }
    void DVUdpTag::Deserialize(TagBuffer i) {
        m_pathId = i.ReadU32();
        m_hopCount = i.ReadU32();
        m_SrcRouteEnable = i.ReadU8();
    }
    void DVUdpTag::Print(std::ostream& os) const {
        os << "m_pathId=" << m_pathId;
        os << ", m_hopCount=" << m_hopCount;
        os << ", m_SrcRouteEnable=" << m_SrcRouteEnable;
    }

    /*---- DVAck-Tag -----*/
    DVAckTag::DVAckTag() {}
    DVAckTag::~DVAckTag() {}
    TypeId DVAckTag::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::DVAckTag").SetParent<Tag>().AddConstructor<DVAckTag>();
        return tid;
    }
    void DVAckTag::SetPathId(uint32_t pathId) { m_pathId = pathId; }
    uint32_t DVAckTag::GetPathId(void) const { return m_pathId; }
    void DVAckTag::SetCE(uint32_t ce) { m_ce = ce; }
    uint32_t DVAckTag::GetCE(void) const { return m_ce; }
    void DVAckTag::SetLength(uint8_t length) { m_length = length; }
    uint8_t DVAckTag::GetLength(void) const { return m_length; }
    void DVAckTag::SetLastSwitchId(uint32_t last_switch_id) { m_last_switch_id = last_switch_id; }
    uint32_t DVAckTag::GetLastSwitchId(void) const { return m_last_switch_id; }
    TypeId DVAckTag::GetInstanceTypeId(void) const { return GetTypeId(); }
    uint32_t DVAckTag::GetSerializedSize(void) const {
        return sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    }
    void DVAckTag::Serialize(TagBuffer i) const {
        i.WriteU32(m_pathId);
        i.WriteU32(m_ce);
        i.WriteU8(m_length);
        i.WriteU32(m_last_switch_id);
    }
    void DVAckTag::Deserialize(TagBuffer i) {
        m_pathId = i.ReadU32();
        m_ce = i.ReadU32();
        m_length = i.ReadU8();
        m_last_switch_id = i.ReadU32();
    }
    void DVAckTag::Print(std::ostream& os) const {
        os << "m_pathId=" << m_pathId;
        os << ", m_CE=" << m_ce;
        os << ", m_length=" << m_length;
        os << ", m_last_switch_id=" << m_last_switch_id;
    }


    /*---- DV-Routing -----*/
    DVRouting::DVRouting() {
        m_isToR = false;
        m_switch_id = (uint32_t)-1;

        // set constants
        m_dreTime = Time(MicroSeconds(200));
        m_agingTime = Time(MilliSeconds(10));
        m_flowletTimeout = Time(MilliSeconds(1));
        m_quantizeBit = 3;
        m_alpha = 0.2;
    }

    // it defines flowlet's 64bit key (order does not matter)
    uint64_t DVRouting::GetQpKey(uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg) {
        return ((uint64_t)dip << 32) | ((uint64_t)sport << 16) | (uint64_t)pg | (uint64_t)dport;
    }

    TypeId DVRouting::GetTypeId(void) {
        static TypeId tid =
            TypeId("ns3::DVRouting").SetParent<Object>().AddConstructor<DVRouting>();

        return tid;
    }

    uint32_t DVRouting::GetOutPortFromPath(const uint32_t& path, const uint32_t& hopCount) {
        std::vector<uint8_t> bytes(4);
        bytes[0] = (path >> 24) & 0xFF; // 获取高位字节
        bytes[1] = (path >> 16) & 0xFF;
        bytes[2] = (path >> 8) & 0xFF;
        bytes[3] = path & 0xFF; // 获取低位字节
        return bytes[hopCount];
    }

    // void DVRouting::SetOutPortToPath(uint32_t& path, const uint32_t& hopCount,
    //                                     const uint32_t& outPort) {
    //     ((uint8_t*)&path)[hopCount] = outPort;
    // }


    void DVRouting::DoSwitchSend(Ptr<Packet> p, CustomHeader& ch, uint32_t outDev, uint32_t qIndex) {
        m_switchSendCallback(p, ch, outDev, qIndex);
    }
    void DVRouting::DoSwitchSendToDev(Ptr<Packet> p, CustomHeader& ch) {
        m_switchSendToDevCallback(p, ch);
    }

    void DVRouting::SetSwitchSendCallback(SwitchSendCallback switchSendCallback) {
        m_switchSendCallback = switchSendCallback;
    }

    void DVRouting::SetSwitchSendToDevCallback(SwitchSendToDevCallback switchSendToDevCallback) {
        m_switchSendToDevCallback = switchSendToDevCallback;
    }

    void DVRouting::SetSwitchInfo(bool isToR, uint32_t switch_id) {
        m_isToR = isToR;
        m_switch_id = switch_id;
    }

    void DVRouting::SetLinkCapacity(uint32_t outPort, uint64_t bitRate) {
        auto it = m_outPort2BitRateMap.find(outPort);
        if (it != m_outPort2BitRateMap.end()) {
            // already exists, then check matching
            NS_ASSERT_MSG(it->second == bitRate,
                        "bitrate already exists, but inconsistent with new input");
        } else {
            m_outPort2BitRateMap[outPort] = bitRate;
        }
    }

    uint32_t DVRouting::UpdateLocalDre(Ptr<Packet> p, CustomHeader ch, uint32_t outPort) {
        uint32_t X = m_DreMap[outPort];
        uint32_t newX = X + p->GetSize();
        // NS_LOG_FUNCTION("Old X" << X << "New X" << newX << "outPort" << outPort << "Switch" <<
        // m_switch_id << Simulator::Now());
        m_DreMap[outPort] = newX;
        return newX;
    }

    uint32_t DVRouting::QuantizingX(uint32_t outPort, uint32_t X) {
        auto it = m_outPort2BitRateMap.find(outPort);
        if (it == m_outPort2BitRateMap.end()){
            if (Error_log){
                for (auto it = m_outPort2BitRateMap.begin(); it != m_outPort2BitRateMap.end(); ++it) {
                    std::cout << "Port: " << it->first << ", Rate: " << it->second << std::endl;
                }
                std::cout<< "Error wrong port: Port:" << outPort << ", switch: " << m_switch_id <<  std::endl;
            }
            assert(it != m_outPort2BitRateMap.end() && "Cannot find bitrate of interface" );

        }
        uint64_t bitRate = it->second;
        double ratio = static_cast<double>(X * 8) / (bitRate * m_dreTime.GetSeconds() / m_alpha);
        uint32_t quantX = static_cast<uint32_t>(ratio * std::pow(2, m_quantizeBit));
        if (quantX > 3) {
            NS_LOG_FUNCTION("X" << X << "Ratio" << ratio << "Bits" << quantX << Simulator::Now());
        }
        return quantX;
    }
    std::vector<uint8_t> DVRouting::uint32_to_uint8(uint32_t number) {
        std::vector<uint8_t> bytes(4);
        bytes[0] = (number >> 24) & 0xFF; // 获取高位字节
        bytes[1] = (number >> 16) & 0xFF;
        bytes[2] = (number >> 8) & 0xFF;
        bytes[3] = number & 0xFF; // 获取低位字节
        return bytes;
    }
    void DVRouting::RouteInput(Ptr<Packet> p, CustomHeader ch){
        // Packet arrival time
        Time now = Simulator::Now();
        if (ch.l3Prot != 0x11 && ch.l3Prot != 0xFC) {
            // 如果不是ACK或UDP，则按照ECMP来进行路由
            DoSwitchSendToDev(p, ch);
            return;
        }
        if (ch.l3Prot != 0x11 && ch.l3Prot != 0xFC) {
        // 如果不是，则调用 assert 报错
            assert(false && "l3Prot is not 0x11 or 0xFC");
        }

            // Turn on DRE event scheduler if it is not running
        if (!m_dreEvent.IsRunning()) {
            NS_LOG_FUNCTION("DV routing restarts dre event scheduling, Switch:" << m_switch_id
                                                                                << now);
            m_dreEvent = Simulator::Schedule(m_dreTime, &DVRouting::DreEvent, this);
        }

        // Turn on aging event scheduler if it is not running
        if (!m_agingEvent.IsRunning()) {
            NS_LOG_FUNCTION("DV routing restarts aging event scheduling:" << m_switch_id << now);
            m_agingEvent = Simulator::Schedule(m_agingTime, &DVRouting::AgingEvent, this);
        }
        //判断是否是同一个ToR下的两个节点，如果是的话，则直接转发，不经过DV算法
        if (m_isToR){
            uint32_t dip = ch.dip;
            uint32_t sip = ch.sip;
            std::vector<uint32_t> server_vector = Settings::TorSwitch_nodelist[m_switch_id];
            auto src_iter = std::find(server_vector.begin(), server_vector.end(), dip);
            auto dst_iter = std::find(server_vector.begin(), server_vector.end(), sip);
            if (src_iter != server_vector.end() && dst_iter != server_vector.end()) {
                //直接转发
                DoSwitchSendToDev(p, ch);
                return;
            }
        }
        // get QpKey to find flowlet
        uint64_t qpkey = GetQpKey(ch.dip, ch.udp.sport, ch.udp.dport, ch.udp.pg);

        if (ch.l3Prot == 0x11){
            // udp packet
            DVUdpTag udpTag;
            bool found = p->PeekPacketTag(udpTag);
            if(m_isToR){ // ToR switch
                if (!found) {// sender-side
                    /*---- choosing outPort ----*/
                    struct DV_Flowlet* flowlet = NULL;
                    auto flowletItr = m_flowletTable.find(qpkey);
                    if (flowletItr != m_flowletTable.end()){
                        // 1) when flowlet already exists   
                        flowlet = flowletItr->second;
                        flowlet->_nPackets++;
                        uint32_t outPort;
                        uint32_t pathid;
                        bool src_enable;
                        if (flowlet->_SrcRoute_ENABLE){
                            //从path id中提取outPort
                            outPort = GetOutPortFromPath(flowlet->_PathId, 0);
                            pathid = flowlet->_PathId;
                            src_enable = true;
                        }else{
                            outPort = flowlet->_outPort;
                            src_enable = false;
                            pathid = 0;
                        }
                        udpTag.SetSrcRouteEnable(src_enable);
                        udpTag.SetPathId(pathid);
                        udpTag.SetHopCount(0);
                        uint32_t X = UpdateLocalDre(p, ch, outPort);  // update local DRE
                        if (DreTable_log){
                            printf("Dre Table: Src switch %d\n", m_switch_id);
                            for (auto it = m_DreMap.begin(); it != m_DreMap.end(); ++it) {
                                uint32_t ce = it->second;
                                if (Error_log){
                                    if (it->first == 0){
                                        std::cout << "Error: Port 0 should not be used" << std::endl;
                                    }
                                }
                                uint32_t localce = QuantizingX(it->first, ce);
                                std::cout << "Port: " << it->first << ", CE: " << it->second << ",localCE: " << localce << std::endl;
                            }
                        }
                        p->AddPacketTag(udpTag);
                        std::cout << "ToR switch: " << m_switch_id << " UDP packet: " << PARSE_FIVE_TUPLE(ch) << " outPort: " << outPort <<" exists flowlet" <<std::endl;
                        DoSwitchSend(p, ch, outPort, ch.udp.pg);
                        // return outPort;
                        return;
                    }
                    // 2) flowlet does not exist, e.g., first packet of flow
                    uint32_t dip = ch.dip;
                    RouteChoice  m_choice = GetBestPath(dip, ch);
                    struct DV_Flowlet* newFlowlet = new DV_Flowlet;
                    newFlowlet->_nPackets = 1;
                    newFlowlet->_SrcRoute_ENABLE = m_choice.SrcRoute;
                    newFlowlet->_outPort = m_choice.outPort;
                    newFlowlet->_PathId = m_choice.pathid;
                    m_flowletTable[qpkey] = newFlowlet;
                    udpTag.SetSrcRouteEnable(m_choice.SrcRoute);
                    udpTag.SetPathId(m_choice.pathid);
                    udpTag.SetHopCount(0);
                    p->AddPacketTag(udpTag);
                    uint32_t X = UpdateLocalDre(p, ch, m_choice.outPort);  // update local DRE
                    if (DreTable_log){
                        printf("Dre Table: Src switch %d\n", m_switch_id);
                        for (auto it = m_DreMap.begin(); it != m_DreMap.end(); ++it) {
                            uint32_t ce = it->second;
                            if (Error_log){
                                if (it->first == 0){
                                    std::cout << "Error: Port 0 should not be used" << std::endl;
                                }
                            }
                            uint32_t localce = QuantizingX(it->first, ce);
                            std::cout << "Port: " << it->first << ", CE: " << it->second << ",localCE: " << localce << std::endl;
                        }
                    }
                    if (Nodepass_log){
                        std::cout << "ToR switch: " << m_switch_id << " UDP packet: " << PARSE_FIVE_TUPLE(ch) << " outPort: " << m_choice.outPort <<" new flowlet" <<std::endl;
                    }
                    DoSwitchSend(p, ch, m_choice.outPort, ch.udp.pg);
                    return;
                }
                /*---- receiver-side ----*/
                p->RemovePacketTag(udpTag);
                if (Nodepass_log){
                    std::cout << "Dst ToR switch: " << m_switch_id << " UDP packet: " << PARSE_FIVE_TUPLE(ch) <<std::endl;
                }
                if (Route_log){
                    uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
                    printf("Route info: flow id %d, dst switch %d\n", flowid, m_switch_id);
                }
                DoSwitchSendToDev(p, ch);
                return;
            }
            // Agg/Core switch
            assert(found && "If not ToR (leaf), DVTag should be found");
            uint32_t hopCount = udpTag.GetHopCount() + 1;
            if (Nodepass_log){
                std::cout << "Mid Switch: " << m_switch_id << " UDP packet: " << PARSE_FIVE_TUPLE(ch) << ",hop" << hopCount << std::endl;
            }
            udpTag.SetHopCount(hopCount);
            uint8_t src_enable = udpTag.GetSrcRouteEnable();
            if(Route_log){
                uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
                printf("Route info: flow id %d, switch %d\n", flowid, m_switch_id);
            }
            if (src_enable){
                //源路由
                uint32_t pathid = udpTag.GetPathId();
                uint32_t outPort = GetOutPortFromPath(pathid, hopCount);
                p->AddPacketTag(udpTag);
                uint32_t X = UpdateLocalDre(p, ch, outPort);  // update local DRE
                DoSwitchSend(p, ch, outPort, ch.udp.pg);
                if (Route_log){
                    std::cout << "Source route:" << std::endl;
                    std::cout << "outPort: " << outPort << ", hopCount: " << hopCount <<  ",path id:" << pathid <<", path: ";
                    std::vector<uint8_t> showpath = uint32_to_uint8(pathid);
                    for (int i = 0; i < 4; i++) {
                        std::cout << static_cast<int>(showpath[i]) << "->";
                    }
                    std::cout << std::endl;
                }
                if (DreTable_log){
                    printf("Dre Table: Mid switch %d\n", m_switch_id);
                    for (auto it = m_DreMap.begin(); it != m_DreMap.end(); ++it) {
                            uint32_t ce = it->second;
                            uint32_t localce = QuantizingX(it->first, ce);
                            std::cout << "Port: " << it->first << ", CE: " << it->second << ",localCE: " << localce << std::endl;
                    }
                }
                return;
            }
            else{
                //ECMP
                if (Route_log){
                    std::cout << "ECMP route" << std::endl;
                }
                DoSwitchSendToDev(p, ch);
                return;
            }
        }
        else if (ch.l3Prot == 0xFC){
            // ACK PACKET       
            DVAckTag ackTag;
            bool found = p->PeekPacketTag(ackTag);
            if (m_isToR){
                if (!found)// sender-side
                {
                    ackTag.SetPathId(0);
                    uint32_t sip = ch.sip;
                    std::map<uint32_t, DVInfo> PortINfo = m_DVTable[sip];
                    uint32_t minCE = DV_NULL;
                    for (const auto& pair : PortINfo) {
                        uint32_t src_port = pair.first;
                        uint32_t localce = 0;
                        auto ceitr = m_DreMap.find(src_port);
                        if (ceitr != m_DreMap.end()) {
                            uint32_t ce = m_DreMap[src_port];
                            localce =  QuantizingX(src_port, ce);
                        }
                        std::cout << "dst ToR generate local ack: " << "port: " << src_port << ", ce: " << localce << std::endl;
                        if (localce < minCE){
                            minCE = localce;
                        }
                    }
                    if (ACK_log){
                        uint32_t ack_src_id = Settings::hostIp2IdMap[ch.sip];
                        uint32_t ack_dst_id = Settings::hostIp2IdMap[ch.dip];
                        uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.dip], Settings::hostIp2IdMap[ch.sip], ch.udp.dport, ch.udp.sport)];
                        printf("ACK info: flow id: %d, Ack from node %d to %d, current: Src switch %d \n", flowid, ack_src_id, ack_dst_id, m_switch_id);
                        printf("send ack packet info: local min CE: %d, port:%d, to node:%d\n", minCE, id2Port[ack_src_id], ack_src_id);
                        //显示一下本地的Dre表
                        if (DreTable_log){
                            printf("Dre Table: Src switch %d\n", m_switch_id);
                            for (auto it = m_DreMap.begin(); it != m_DreMap.end(); ++it) {
                                    uint32_t ce = it->second;
                                    uint32_t localce = QuantizingX(it->first, ce);
                                    std::cout << "Port: " << it->first << ", CE: " << it->second << ",localCE: " << localce << std::endl;
                            }
                        }
                    }
                    ackTag.SetCE(minCE);
                    ackTag.SetLength(0);
                    ackTag.SetLastSwitchId(m_switch_id);
                    ackTag.SetPathId(0);
                    p->AddPacketTag(ackTag);
                    DoSwitchSendToDev(p, ch);
                    return;
                }
                // receiver-side
                //更新本地的CE表
                uint32_t last_swtich = ackTag.GetLastSwitchId();
                uint32_t inPort = id2Port[last_swtich];
                std::vector<uint8_t> path;
                std::vector<uint8_t> fullpath = uint32_to_uint8(ackTag.GetPathId());
                for (int i = 0; i < ackTag.GetLength(); i++) {
                    path.push_back(fullpath [i]);
                }       

                if (ACK_log){
                    uint32_t ack_src_id = Settings::hostIp2IdMap[ch.sip];
                    uint32_t ack_dst_id = Settings::hostIp2IdMap[ch.dip];
                    uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.dip], Settings::hostIp2IdMap[ch.sip], ch.udp.dport, ch.udp.sport)];
                    printf("ACK info: flow id: %d, Ack from node %d to %d, current: Dst switch %d \n", flowid, ack_src_id, ack_dst_id, m_switch_id);
                    //显示从哪个交换机收到的ack，以及ack的格式
                    printf("receive ack packet info: from switch: %d, inPort %d, packet CE: %d, \n", last_swtich, inPort, ackTag.GetCE());
                    std::cout << "receive ack path id: " << ackTag.GetPathId() << std::endl;
                    std::cout << "receive ack packet path: ";
                    for (int i = 0; i < path.size(); i++) {
                        std::cout << static_cast<int>(path[i]) << "->";
                    }
                    std::cout << std::endl;
                    //更新前的CE表
                    std::cout << "before ack DV table:" << std::endl;
                    for (auto it = m_DVTable[ch.sip].begin(); it != m_DVTable[ch.sip].end(); ++it) {
                        if (it->second._valid){
                            std::cout << "Port: " << it->first << ", CE: " << it->second._ce << ",Path: ";
                            for (int i = 0; i < it->second._path.size(); i++) 
                                std::cout << static_cast<int>(it->second._path[i]) << "->";
                            std::cout <<std::endl;
                        }
                    }
                }

                m_DVTable[ch.sip][inPort]._ce = ackTag.GetCE();
                m_DVTable[ch.sip][inPort]._path = path;
                m_DVTable[ch.sip][inPort]._valid = true;

                if (ACK_log){
                    //更新后的CE表
                    std::cout << "after ack DV table:" << std::endl;
                    for (auto it = m_DVTable[ch.sip].begin(); it != m_DVTable[ch.sip].end(); ++it) {
                        if (it->second._valid){
                            std::cout << "Port: " << it->first << ", CE: " << it->second._ce << ",Path: ";
                            for (int i = 0; i < it->second._path.size(); i++) 
                                std::cout << static_cast<int>(it->second._path[i]) << "->";
                            std::cout <<std::endl;
                        }
                    }
                    //显示一下本地的Dre表
                    if (DreTable_log){
                        printf("Dre Table: Dst switch %d\n", m_switch_id);
                        for (auto it = m_DreMap.begin(); it != m_DreMap.end(); ++it) {
                                uint32_t ce = it->second;
                                uint32_t localce = QuantizingX(it->first, ce);
                                std::cout << "Port: " << it->first << ", CE: " << it->second << ",localCE: " << localce << std::endl;
                        }
                    }
                }

                p->RemovePacketTag(ackTag);
                DoSwitchSendToDev(p, ch);
                return;
            }
            // Agg/Core switch
            assert(found && "If not ToR (leaf), DVTag should be found");
            //更新本地的CE表
            uint32_t last_swtich = ackTag.GetLastSwitchId();
            uint32_t inPort = id2Port[last_swtich];
            std::vector<uint8_t> path;
            std::vector<uint8_t> fullpath = uint32_to_uint8(ackTag.GetPathId());
                for (int i = 0; i < ackTag.GetLength(); i++) {
                    path.push_back(fullpath [i]);
                }       

            if (ACK_log){
                uint32_t ack_src_id = Settings::hostIp2IdMap[ch.sip];
                uint32_t ack_dst_id = Settings::hostIp2IdMap[ch.dip];
                uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.dip], Settings::hostIp2IdMap[ch.sip], ch.udp.dport, ch.udp.sport)];
                printf("ACK info: flow id: %d, Ack from node %d to %d, current: Mid switch %d \n", flowid, ack_src_id, ack_dst_id, m_switch_id);
                //显示从哪个交换机收到的ack，以及ack的格式
                printf("receive ack packet info: from switch: %d, inPort %d, packet CE: %d, \n", last_swtich, inPort, ackTag.GetCE());
                std::cout << "receive ack path id: " << ackTag.GetPathId() << std::endl;
                std::cout << "receive ack packet path: ";
                for (int i = 0; i < path.size(); i++) {
                    std::cout << static_cast<int>(path[i]) << "->";
                }
                std::cout <<std::endl;
                //更新前的CE表
                std::cout << "before ack DV table:" << std::endl;
                for (auto it = m_DVTable[ch.sip].begin(); it != m_DVTable[ch.sip].end(); ++it) {
                    if (it->second._valid){
                        std::cout << "Port: " << it->first << ", CE: " << it->second._ce << ",Path: ";
                        for (int i = 0; i < it->second._path.size(); i++) 
                            std::cout << static_cast<int>(it->second._path[i]) << "->";
                        std::cout <<std::endl;
                    }
                }
            }

            m_DVTable[ch.sip][inPort]._ce = ackTag.GetCE();
            m_DVTable[ch.sip][inPort]._path = path;
            m_DVTable[ch.sip][inPort]._valid = true;

            if (ACK_log){
                //更新后的CE表
                std::cout << "after ack DV table:" << std::endl;
                for (auto it = m_DVTable[ch.sip].begin(); it != m_DVTable[ch.sip].end(); ++it) {
                    if (it->second._valid){
                        std::cout << "Port: " << it->first << ", CE: " << it->second._ce << ",Path: ";
                        for (int i = 0; i < it->second._path.size(); i++) 
                            std::cout << static_cast<int>(it->second._path[i]) << "->";
                        std::cout << std::endl;
                    }
                }
                //显示一下本地的Dre表
                if (DreTable_log){
                    printf("Dre Table: Mid switch %d\n", m_switch_id);
                    for (auto it = m_DreMap.begin(); it != m_DreMap.end(); ++it) {
                            uint32_t ce = it->second;
                            uint32_t localce = QuantizingX(it->first, ce);
                            std::cout << "Port: " << it->first << ", CE: " << it->second << ",localCE: " << localce << std::endl;
                    }
                }
            }

            //选择一个最佳的路径放入到包中
            CEChoice m_choice = GetKnownBestPath(ch.sip);

            if (ACK_log){
                //显示Ack包中携带的信息
                std::vector<uint8_t> show_path;
                std::vector<uint8_t> fullpath = uint32_to_uint8(m_choice._path);
                for (int i = 0; i < ackTag.GetLength() + 1; i++) {
                    show_path.push_back(fullpath[i]);
                }       
                std::cout << "send ack packet: ce: " << m_choice._ce << ",path length: " << ackTag.GetLength() + 1 << ",path_id: " << m_choice._path<< ",path: ";
                for (int i = 0; i < ackTag.GetLength() + 1; i++) {
                    std::cout << static_cast<int>(show_path[i]) << "->";
                }
                std::cout << std::endl;
            }


            ackTag.SetPathId(m_choice._path);
            ackTag.SetCE(m_choice._ce);
            ackTag.SetLastSwitchId(m_switch_id);
            //更新包的length
            ackTag.SetLength(ackTag.GetLength() + 1);
            p->AddPacketTag(ackTag);
            DoSwitchSendToDev(p, ch);
            return;
        }
    }
    CEChoice DVRouting::GetKnownBestPath(uint32_t dip){
        auto pathItr = m_DVTable.find(dip);
        std::vector<CEChoice> candidateRoutes;
        uint32_t minCongestion = DV_NULL;
        for (auto it = pathItr->second.begin(); it != pathItr->second.end(); ++it) {
            // printf("GetKnownBestPath: id:%d, port:%d\n", m_switch_id, it->first);
            uint32_t port = it->first;
            DVInfo info  = it->second;
            auto innerDre = m_DreMap.find(port);
            uint32_t localCongestion = 0;
            uint32_t remoteCongestion = 0;
            std::vector<uint8_t> path;
            if (innerDre != m_DreMap.end()) {

                localCongestion = QuantizingX(port, innerDre->second);
                // std::cout <<"GetKnownBestPath:, "<< "swtichid" <<  m_switch_id << "local congestion: " << port << " ," <<localCongestion << std::endl;
            }
            if (info._valid) {
                remoteCongestion = info._ce;
                path = info._path;
            }
            uint32_t CurrCongestion = std::max(localCongestion, remoteCongestion);
            if (minCongestion > CurrCongestion) {
                if (info._valid) {
                    //path id is the best path
                    minCongestion = CurrCongestion;
                    candidateRoutes.clear();
                    CEChoice choice;
                    choice._ce = CurrCongestion;
                    uint8_t port_8 = port & 0xFF;
                    choice._path = mergePortAndVector(port_8, path);
                    // std::cout << "GetKnownBestPath: " << "port: " << port << "CE path: " << choice._path << std::endl;
                    choice._port = port;
                    candidateRoutes.push_back(choice);
                }
            } else if (minCongestion == CurrCongestion) {
                if (info._valid) {
                    CEChoice choice;
                    choice._ce = CurrCongestion;
                    uint8_t port_8 = port & 0xFF;
                    choice._path = mergePortAndVector(port_8, path);
                    // std::cout << "GetKnownBestPath: " << "port: " << port << "CE path: " << choice._path << std::endl;
                    choice._port = port;
                    candidateRoutes.push_back(choice);
                }
            }
        }
        // printf("find %lu possible path", candidateRoutes.size());
        return candidateRoutes[rand() % candidateRoutes.size()];
    }
    RouteChoice DVRouting::GetBestPath(uint32_t dip, CustomHeader ch){
        auto pathItr = m_DVTable.find(dip);
        assert(pathItr != m_DVTable.end() && "Cannot find dip from m_DVTable");

        if (Route_log){
            uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
            printf("Route info: flow id %d, switch %d\n", flowid, m_switch_id);
        }
        auto pathInfoMap = m_DVTable[dip];

        std::vector<RouteChoice> candidateRoutes;
        uint32_t minCongestion = DV_NULL;
        for (auto it = pathItr->second.begin(); it != pathItr->second.end(); ++it) {

            uint32_t port = it->first;
            // std::cout <<"Path select: " << std::endl;
            // std::cout << "port: " << port;        
            if (Route_log){
                std::cout << "port: " << port << " ,";
            }
            uint32_t localCongestion = 0;
            uint32_t remoteCongestion = 0;
            std::vector<uint8_t> path;
            bool valid = false;

            auto innerDre = m_DreMap.find(port);
            if (innerDre != m_DreMap.end()) {
                localCongestion = QuantizingX(port, innerDre->second);
                if (Route_log){
                    std::cout << "Dre_map find: localCe: " << localCongestion << " ,";
                }
            }
            else{
                if (Route_log){
                    std::cout << "Dre_map not find ,";
                }
            }

            auto Pathinfo = pathInfoMap.find(port);
            if (Pathinfo != pathInfoMap.end()) {
                if (Pathinfo->second._valid) {
                    remoteCongestion = Pathinfo->second._ce;
                    path = Pathinfo->second._path;
                    valid = true;
                    if (Route_log){
                        std::cout << "remote congestion valid: " << remoteCongestion << ", path: ";
                        for (size_t i = 0; i < path.size(); ++i) {
                            std::cout << static_cast<int>(path[i]) << "->";
                        }
                    }
                    std::cout << std::endl;
                }
                else{
                    if (Route_log){
                        std::cout << "remote congestion unvalid: " << remoteCongestion << std::endl;
                    }
                }
            }
            uint32_t CurrCongestion = std::max(localCongestion, remoteCongestion);
            if (minCongestion > CurrCongestion) {
                minCongestion = CurrCongestion;
                candidateRoutes.clear();
                if (valid) {
                    //path id is the best path
                    RouteChoice choice;
                    choice.SrcRoute = true;
                    choice.outPort = port;
                    uint8_t port_8 = port & 0xFF;
                    uint32_t pathid = mergePortAndVector(port_8, path);
                    choice.pathid = pathid;
                    candidateRoutes.push_back(choice);
                }
                else{
                    RouteChoice choice;
                    choice.SrcRoute = false;
                    choice.outPort = port;
                    candidateRoutes.push_back(choice);
                }
            } else if (minCongestion == CurrCongestion) {
                RouteChoice choice;
                if (valid) {
                    //path id is the best path
                    choice.SrcRoute = true;
                    choice.outPort = port;
                    uint8_t port_8 = port & 0xFF;
                    uint32_t pathid = mergePortAndVector(port_8, path);
                    choice.pathid = pathid;
                    candidateRoutes.push_back(choice);
                }
                else{
                    choice.SrcRoute = false;
                    choice.outPort = port;
                    candidateRoutes.push_back(choice);
                }
                candidateRoutes.push_back(choice);  // equally good
            }

        }
        // 在这里可以检查一下每个路径的拥塞情况，以及最后获得的candidateRoutes
        RouteChoice finaleChoice = candidateRoutes[rand() % candidateRoutes.size()];
        if (Route_log){
            std::vector<uint8_t> showpath = uint32_to_uint8(finaleChoice.pathid);
            std::cout << "Final choice: port: " << finaleChoice.outPort << ", pathid: " << finaleChoice.pathid << ", path: ";
            for (int i = 0; i < 4; i++) {
                std::cout << static_cast<int>(showpath[i]) << "->";
            }
            std::cout << "bool SrcRoute: " << finaleChoice.SrcRoute << std::endl;
        }
        return finaleChoice;

    }
    uint32_t DVRouting::mergePortAndVector(uint8_t port, std::vector<uint8_t> vec) {
        uint32_t result = port; // 先将 port 存入结果中

        // 将 vector 中的元素逐个存入结果中
        for (size_t i = 0; i < vec.size(); ++i) {
            result <<= 8; // 左移 8 位，腾出一个字节的空间
            result |= vec[i]; // 将 vector 中的元素放入结果中
        }

        // 如果 vector 的大小不足 4 个字节，补充 0
        size_t remainingBytes = 3 - vec.size();
        while (remainingBytes > 0) {
            result <<= 8; // 左移 8 位
            --remainingBytes;
        }

        return result;
    }
    void DVRouting::SetConstants(Time dreTime, Time agingTime, Time flowletTimeout,
                                    uint32_t quantizeBit, double alpha) {
        m_dreTime = dreTime;
        m_agingTime = agingTime;
        m_flowletTimeout = flowletTimeout;
        m_quantizeBit = quantizeBit;
        m_alpha = alpha;
    }

    void DVRouting::DoDispose() {
        for (auto i : m_flowletTable) {
            delete (i.second);
        }
        m_dreEvent.Cancel();
        m_agingEvent.Cancel();
    }

    void DVRouting::DreEvent() {
        std::map<uint32_t, uint32_t>::iterator itr = m_DreMap.begin();
        auto now = Simulator::Now();
        if (Dre_decrease_log){
            std::cout << "Dre decrease info: switch: " << m_switch_id << ", time: " << now << std::endl;
        }
        for (; itr != m_DreMap.end(); ++itr) {
            if (Dre_decrease_log){
                std::cout << "Dre decrease: port: " << itr->first << ", old X: " << itr->second << ", old localCe:"<< QuantizingX(itr->first, itr->second) <<std::endl;
            }
            uint32_t newX = itr->second * (1 - m_alpha);
            itr->second = newX;
            if (Dre_decrease_log){
                std::cout << "Dre decrease: port: " << itr->first << ", new X: " << itr->second << ", new localCe:"<< QuantizingX(itr->first, itr->second) <<std::endl;
            }
        }
        NS_LOG_FUNCTION(Simulator::Now());
        m_dreEvent = Simulator::Schedule(m_dreTime, &DVRouting::DreEvent, this);
    }

    void DVRouting::AgingEvent() {
        auto now = Simulator::Now();

        auto itr = m_DVTable.begin();  // always non-empty
        for (; itr != m_DVTable.end(); ++itr) {
            auto innerItr = (itr->second).begin();
            for (; innerItr != (itr->second).end(); ++innerItr) {
                if (now - (innerItr->second)._updateTime > m_agingTime) {
                    (innerItr->second)._ce = 0;
                    (innerItr->second)._valid = false;
                }
            }
        }
        NS_LOG_FUNCTION(Simulator::Now());
        m_agingEvent = Simulator::Schedule(m_agingTime, & DVRouting::AgingEvent, this);
    }
}

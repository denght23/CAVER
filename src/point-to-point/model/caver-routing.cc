/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MIT License
 * 
 * Copyright (c) 2025 CAVER-LB
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "ns3/caver-routing.h"
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
#include <random>

// NS_LOG_COMPONENT_DEFINE("CaverRouting");

namespace ns3 {

    /*---- CaverUdp-Tag -----*/ 
    CaverUdpTag::CaverUdpTag() {}
    CaverUdpTag::~CaverUdpTag() {}
    TypeId CaverUdpTag::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::CaverUdpTag").SetParent<Tag>().AddConstructor<CaverUdpTag>();
        return tid;
    }
    void CaverUdpTag::SetPathId(uint32_t pathId) { m_pathId = pathId; }
    uint32_t CaverUdpTag::GetPathId(void) const { return m_pathId; }
    void CaverUdpTag::SetHopCount(uint32_t hopCount) { m_hopCount = hopCount; }
    uint32_t CaverUdpTag::GetHopCount(void) const { return m_hopCount; }
    void CaverUdpTag::SetSrcRouteEnable(bool SrcRouteEnable) {
        if (SrcRouteEnable){
            m_SrcRouteEnable = 1;
        }
        else{
            m_SrcRouteEnable = 0;
        }

    }
    uint8_t CaverUdpTag::GetSrcRouteEnable(void) const { return m_SrcRouteEnable; }
    TypeId CaverUdpTag::GetInstanceTypeId(void) const { return GetTypeId(); }
    uint32_t CaverUdpTag::GetSerializedSize(void) const {
        return sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    }
    void CaverUdpTag::Serialize(TagBuffer i) const {
        i.WriteU32(m_pathId);
        i.WriteU32(m_hopCount);
        i.WriteU8(m_SrcRouteEnable);
    }
    void CaverUdpTag::Deserialize(TagBuffer i) {
        m_pathId = i.ReadU32();
        m_hopCount = i.ReadU32();
        m_SrcRouteEnable = i.ReadU8();
    }
    void CaverUdpTag::Print(std::ostream& os) const {
        os << "m_pathId=" << m_pathId;
        os << ", m_hopCount=" << m_hopCount;
        os << ", m_SrcRouteEnable=" << m_SrcRouteEnable;
    }

    /*---- CaverAck-Tag -----*/
    CaverAckTag::CaverAckTag() {}
    CaverAckTag::~CaverAckTag() {}
    TypeId CaverAckTag::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::CaverAckTag").SetParent<Tag>().AddConstructor<CaverAckTag>();
        return tid;
    }
    void CaverAckTag::SetMPathId(uint32_t pathId) { m_pathId = pathId; }
    uint32_t CaverAckTag::GetMPathId(void) const { return m_pathId; }
    void CaverAckTag::SetMCE(uint32_t ce) { m_ce = ce; }
    uint32_t CaverAckTag::GetMCE(void) const { return m_ce; }
    void CaverAckTag::SetBestPathId(uint32_t pathId) { best_pathId = pathId; }
    uint32_t CaverAckTag::GetBestPathId(void) const { return best_pathId; }
    void CaverAckTag::SetBestCE(uint32_t ce) { best_ce = ce; }
    uint32_t CaverAckTag::GetBestCE(void) const { return best_ce; }
    void CaverAckTag::SetLength(uint8_t length) { m_length = length; }
    uint8_t CaverAckTag::GetLength(void) const { return m_length; }
    void CaverAckTag::SetLastSwitchId(uint32_t last_switch_id) { m_last_switch_id = last_switch_id; }
    uint32_t CaverAckTag::GetLastSwitchId(void) const { return m_last_switch_id; }
    TypeId CaverAckTag::GetInstanceTypeId(void) const { return GetTypeId(); }
    uint32_t CaverAckTag::GetHostId(void) const {return m_host_id;}
    void CaverAckTag::SetHostId(uint32_t host_id) {m_host_id = host_id; }
    uint32_t CaverAckTag::GetSerializedSize(void) const {
        return sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t);
    }
    void CaverAckTag::Serialize(TagBuffer i) const {
        i.WriteU32(m_pathId);
        i.WriteU32(m_ce);
        i.WriteU32(best_pathId);
        i.WriteU32(best_ce);
        i.WriteU8(m_length);
        i.WriteU32(m_last_switch_id);
        i.WriteU32(m_host_id);
    }
    void CaverAckTag::Deserialize(TagBuffer i) {
        m_pathId = i.ReadU32();
        m_ce = i.ReadU32();
        best_pathId = i.ReadU32();
        best_ce = i.ReadU32();
        m_length = i.ReadU8();
        m_last_switch_id = i.ReadU32();
        m_host_id = i.ReadU32();
    }
    void CaverAckTag::Print(std::ostream& os) const {
        os << "m_pathId=" << m_pathId;
        os << ", m_CE=" << m_ce;
        os << ", best_pathId=" << best_pathId;
        os << ", best_CE=" << best_ce;
        os << ", m_length=" << m_length;
        os << ", m_last_switch_id=" << m_last_switch_id;
        os << ", m_host_id=" << m_host_id;
    }


    /*---- Caver-Routing -----*/
    CaverRouting::CaverRouting() {
        m_isToR = false;
        m_switch_id = (uint32_t)-1;

        // set constants
        m_dreTime = Time(MicroSeconds(200));
        m_agingTime = Time(MilliSeconds(10));
        m_flowletTimeout = Time(MilliSeconds(1));
        m_quantizeBit = 3;
        m_alpha = 0.2;
        m_ce_threshold = 1.5;
        m_patchoiceTimeout = Time(MilliSeconds(10));
        m_pathChoice_num = 5;
    }

    // it defines flowlet's 64bit key (order does not matter)
    uint64_t CaverRouting::GetQpKey(uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg) {
        return ((uint64_t)dip << 32) | ((uint64_t)sport << 16) | (uint64_t)pg | (uint64_t)dport;
    }

    TypeId CaverRouting::GetTypeId(void) {
        static TypeId tid =
            TypeId("ns3::CaverRouting").SetParent<Object>().AddConstructor<CaverRouting>();

        return tid;
    }

    uint32_t CaverRouting::GetOutPortFromPath(const uint32_t& path, const uint32_t& hopCount) {
        std::vector<uint8_t> bytes(4);
        bytes[0] = (path >> 24) & 0xFF;
        bytes[1] = (path >> 16) & 0xFF;
        bytes[2] = (path >> 8) & 0xFF;
        bytes[3] = path & 0xFF;
        return bytes[hopCount];
    }

    // void CaverRouting::SetOutPortToPath(uint32_t& path, const uint32_t& hopCount,
    //                                     const uint32_t& outPort) {
    //     ((uint8_t*)&path)[hopCount] = outPort;
    // }


    void CaverRouting::DoSwitchSend(Ptr<Packet> p, CustomHeader& ch, uint32_t outDev, uint32_t qIndex) {
        m_switchSendCallback(p, ch, outDev, qIndex);
    }
    void CaverRouting::DoSwitchSendToDev(Ptr<Packet> p, CustomHeader& ch) {
        m_switchSendToDevCallback(p, ch);
    }

    void CaverRouting::SetSwitchSendCallback(SwitchSendCallback switchSendCallback) {
        m_switchSendCallback = switchSendCallback;
    }

    void CaverRouting::SetSwitchSendToDevCallback(SwitchSendToDevCallback switchSendToDevCallback) {
        m_switchSendToDevCallback = switchSendToDevCallback;
    }

    void CaverRouting::SetSwitchInfo(bool isToR, uint32_t switch_id) {
        m_isToR = isToR;
        m_switch_id = switch_id;
        if (m_isToR){
            ToR_host_num = Settings::TorSwitch_nodelist[m_switch_id].size();
            host_round_index = 0;
        }
    }

    void CaverRouting::SetLinkCapacity(uint32_t outPort, uint64_t bitRate) {
        auto it = m_outPort2BitRateMap.find(outPort);
        if (it != m_outPort2BitRateMap.end()) {
            // already exists, then check matching
            NS_ASSERT_MSG(it->second == bitRate,
                        "bitrate already exists, but inconsistent with new input");
        } else {
            m_outPort2BitRateMap[outPort] = bitRate;
        }
    }

    uint32_t CaverRouting::UpdateLocalDre(Ptr<Packet> p, CustomHeader ch, uint32_t outPort) {
        if (useEWMA) {
            uint32_t X = m_DreMap[outPort];
            Time deltaT = Simulator::Now() - m_Port2UpdateTime[outPort];
            double decayFactor = std::max(0.0, (1.0 - deltaT / tau).GetDouble());
            uint32_t newX = p->GetSize() + X * decayFactor;
            m_Port2UpdateTime[outPort] = Simulator::Now();
            m_DreMap[outPort] = newX;
            return newX;
        } else {
            uint32_t X = m_DreMap[outPort];
            uint32_t newX = X + p->GetSize();
            // NS_LOG_FUNCTION("Old X" << X << "New X" << newX << "outPort" << outPort << "Switch" <<
            // m_switch_id << Simulator::Now());
            m_DreMap[outPort] = newX;
            return newX;
        }
    }

    uint32_t CaverRouting::QuantizingX(uint32_t outPort, uint32_t X) {
        auto it = m_outPort2BitRateMap.find(outPort);
        if (it == m_outPort2BitRateMap.end()){
            if (Error_log){
                for (auto it = m_outPort2BitRateMap.begin(); it != m_outPort2BitRateMap.end(); ++it) {
                    std::cout << "Port: " << it->first << ", Rate: " << it->second << std::endl;
                }
                std::cout<< "Error wrong port: Port:" << outPort << ", switch: " << m_switch_id <<  std::endl;
            }
            if (it != m_outPort2BitRateMap.end()){
                std::cout << "Port: " << outPort << ", sw: " << m_switch_id << std::endl;
            }
            assert(it != m_outPort2BitRateMap.end() && "Cannot find bitrate of interface");

        }
        if (Caver_debug){
            std::cout << "Debug local CE related param info:" << std::endl;
            std::cout << "Port: " << outPort << ", Rate: " << it->second << std::endl;
            std::cout << "alpha: " << m_alpha << std::endl;
            std::cout << "DreTime: " << m_dreTime.GetSeconds() << std::endl;
        }
        uint64_t bitRate = it->second;
        double ratio;
        if (useEWMA) {
            ratio = static_cast<double>(X * 8) / (bitRate * tau.GetSeconds());
        } else {
            ratio = static_cast<double>(X * 8) / (bitRate * m_dreTime.GetSeconds() / m_alpha);
        }
        if (ratio >= 1) {
            //printf("time: %lf ratio:%lf\n", Simulator::Now().GetDouble(), ratio);
            ratio = 1;    
        }

        uint32_t quantX = static_cast<uint32_t>(ratio * std::pow(2, m_quantizeBit));
        if (quantX > 3) {
            NS_LOG_FUNCTION("X" << X << "Ratio" << ratio << "Bits" << quantX << Simulator::Now());
        }
        return quantX;
    }
    std::vector<uint8_t> CaverRouting::uint32_to_uint8(uint32_t number) {
        std::vector<uint8_t> bytes(4);
        bytes[0] = (number >> 24) & 0xFF;
        bytes[1] = (number >> 16) & 0xFF;
        bytes[2] = (number >> 8) & 0xFF;
        bytes[3] = number & 0xFF; // 获取低位字节
        return bytes;
    }
    void CaverRouting::RouteInput(Ptr<Packet> p, CustomHeader ch){
        // Packet arrival time
        Time now = Simulator::Now();
        if (ch.l3Prot != 0x11 && ch.l3Prot != 0xFC) {
            // if not ack or udp packet, use ECMP 
            DoSwitchSendToDev(p, ch);
            return;
        }
        if (ch.l3Prot != 0x11 && ch.l3Prot != 0xFC) {
            assert(false && "l3Prot is not 0x11 or 0xFC");
        }

        //This code is used to filter ACK packets with specific sequence numbers to simulate the performance of Caver under different ACK ratios.
        //if (ch.l3Prot == 0xFC) {
        //    if (ch.ack.seq % 1000 == 0 && ch.ack.seq % 80000 != 0) { //ch.ack.seq % 1000 != 0 usually means this is the ack of the last packet in the flow
        //        DoSwitchSendToDev(p, ch);
        //        return;
        //    }
        //    //uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.dip], Settings::hostIp2IdMap[ch.sip], ch.udp.dport, ch.udp.sport)];
        //    //printf("Ack:%u, FlowId:%u, Length:%u\n", ch.ack.seq, flowid, Settings::FlowId2Length[flowid]);
        //}

        // Turn on DRE event scheduler if it is not running
        if (!m_dreEvent.IsRunning() && !useEWMA) {
            NS_LOG_FUNCTION("Caver routing restarts dre event scheduling, Switch:" << m_switch_id
                                                                                << now);
            m_dreEvent = Simulator::Schedule(m_dreTime, &CaverRouting::DreEvent, this);
        }

        // Turn on aging event scheduler if it is not running
        if (!m_agingEvent.IsRunning()) {
            NS_LOG_FUNCTION("Caver routing restarts aging event scheduling:" << m_switch_id << now);
            m_agingEvent = Simulator::Schedule(m_agingTime, &CaverRouting::AgingEvent, this);
        }
        //if the src host and dst host is under the same tor, forward the packet directly
        if (m_isToR){
            uint32_t dip = ch.dip;
            uint32_t sip = ch.sip;
            std::vector<uint32_t> server_vector = Settings::TorSwitch_nodelist[m_switch_id];
            auto src_iter = std::find(server_vector.begin(), server_vector.end(), dip);
            auto dst_iter = std::find(server_vector.begin(), server_vector.end(), sip);
            if (src_iter != server_vector.end() && dst_iter != server_vector.end()) {
                DoSwitchSendToDev(p, ch);
                return;
            }
        }
        // get QpKey to find flowlet
        uint64_t qpkey = GetQpKey(ch.dip, ch.udp.sport, ch.udp.dport, ch.udp.pg);

        if (ch.l3Prot == 0x11){
            // udp packet
            if(Packet_begin_end_flag){
                std::cout << "---------------UDP begin-----------------\n";
            }
            CaverUdpTag udpTag;
            bool found = p->PeekPacketTag(udpTag);
            if(m_isToR){ // ToR switch
                if (!found) {// sender-side
                    /*---- choosing outPort ----*/
                    if (Nodepass_log){
                        uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
                        std::cout << "Nodepass_log" << std::endl;
                        printf("flow id %d, src switch %d\n", flowid, m_switch_id);
                    }
                    struct Caver_Flowlet* flowlet = NULL;
                    auto flowletItr = m_flowletTable.find(qpkey);
                    if (flowletItr != m_flowletTable.end()){
                        // 1) when flowlet already exists   
                        flowlet = flowletItr->second;
                        assert(flowlet != NULL &&
                       "Impossible in normal cases - flowlet is not correctly registered");
                        if (now - flowlet->_activeTime <= m_flowletTimeout) {  // no timeout
                            flowlet->_activeTime = now;
                            flowlet->_nPackets++;
                            uint32_t outPort;
                            uint32_t pathid;
                            bool src_enable;
                            if(flowlet->_SrcRoute_ENABLE){
                                outPort = GetOutPortFromPath(flowlet->_PathId, 0);
                                pathid = flowlet->_PathId;
                                src_enable = true;
                                udpTag.SetSrcRouteEnable(src_enable);
                                udpTag.SetPathId(pathid);
                                udpTag.SetHopCount(0);
                                uint32_t X = UpdateLocalDre(p, ch, outPort);  // update local DRE
                                p->AddPacketTag(udpTag);
                                if(Route_log){
                                    std::cout << "Route_decision_log" << std::endl;
                                    std::cout << "ToR switch: " << m_switch_id << " UDP packet: " << PARSE_FIVE_TUPLE(ch) << " exists flowlet with SrcRoute" << std::endl;
                                    std::cout << " outPort: " << outPort << std::endl;
                                }
                                DoSwitchSend(p, ch, outPort, ch.udp.pg);
                            }
                            else{
                                src_enable = false;
                                pathid = 0;
                                udpTag.SetSrcRouteEnable(src_enable);
                                udpTag.SetPathId(pathid);
                                udpTag.SetHopCount(0);
                                p->AddPacketTag(udpTag);
                                if(Route_log){
                                    std::cout << "Route_decision_log" << std::endl;
                                    std::cout << "ToR switch: " << m_switch_id << " UDP packet: " << PARSE_FIVE_TUPLE(ch) << " exists flowlet with ECMP" << std::endl;
                                }
                                DoSwitchSendToDev(p, ch);
                            }
                            if(Packet_begin_end_flag){
                                std::cout << "---------------UDP END-----------------\n";
                            }
                            return;
                        }
                        // 2) flowlet expires
                        uint32_t dip = ch.dip;
                        CaverRouteChoice  m_choice;
                        if (show_pathchoice_detail){
                            m_choice = ChoosePathWithDetail(dip, ch);
                        }
                        else{
                            m_choice = ChoosePath(dip, ch);
                        }
                        uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
                        // printf("flowid:%u\n", flowid);
                        if(flowlet_log){
                            std::cout << "Flowlet expires, calculate the new port" << std::endl;
                        }
                        flowlet->_activatedTime = now;
                        flowlet->_activeTime = now;
                        flowlet->_nPackets++;
                        flowlet->_PathId = m_choice.pathid;
                        flowlet->_SrcRoute_ENABLE = m_choice.SrcRoute;
                        flowlet->_outPort = m_choice.outPort;
                        udpTag.SetSrcRouteEnable(m_choice.SrcRoute);
                        udpTag.SetPathId(m_choice.pathid);
                        udpTag.SetHopCount(0);
                        p->AddPacketTag(udpTag);
                        if(Route_log){
                            // show path choice info
                            std::cout << "Route_decision_log" << std::endl;
                            std::cout << "expired flowlet" << std::endl;
                            std::cout << "ToR switch: " << m_switch_id << " UDP packet: " << PARSE_FIVE_TUPLE(ch) << " new flowlet" << std::endl;
                            std::cout << "path choice table" << std::endl;
                            printPathChoiceTable_Entry(dip);
                            std::cout << "RouteChoice: " << std::endl;
                            showRouteChoice(m_choice);
                        }
                        if (Dive_optimal_log){
                            showOptimalvsCaver(ch, m_choice);
                        }
                        if(m_choice.SrcRoute){
                            uint32_t X = UpdateLocalDre(p, ch, m_choice.outPort);  // update local DRE
                            DoSwitchSend(p, ch, m_choice.outPort, ch.udp.pg);
                        }
                        else{
                            DoSwitchSendToDev(p, ch);
                            if (Nodepass_log){
                                std::cout << "ToR switch: " << m_switch_id << " UDP packet: " << PARSE_FIVE_TUPLE(ch) << " ecmp " <<" new flowlet" <<std::endl;
                            }
                        }
                        if(Packet_begin_end_flag){
                                std::cout << "---------------UDP END-----------------\n";
                        }
                        return;
                    }
                    // 3) flowlet does not exist, e.g., first packet of flow
                    uint32_t dip = ch.dip;
                    CaverRouteChoice  m_choice;
                    if (show_pathchoice_detail){
                            m_choice = ChoosePathWithDetail(dip, ch);
                        }
                        else{
                            m_choice = ChoosePath(dip, ch);
                        }
                    uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
                    // printf("flowid:%u\n", flowid);
                    struct Caver_Flowlet* newFlowlet = new Caver_Flowlet;
                    newFlowlet->_activeTime = now;
                    newFlowlet->_activatedTime = now;
                    newFlowlet->_nPackets = 1;
                    newFlowlet->_SrcRoute_ENABLE = m_choice.SrcRoute;
                    newFlowlet->_outPort = m_choice.outPort;
                    newFlowlet->_PathId = m_choice.pathid;
                    m_flowletTable[qpkey] = newFlowlet;
                    udpTag.SetSrcRouteEnable(m_choice.SrcRoute);
                    udpTag.SetPathId(m_choice.pathid);
                    udpTag.SetHopCount(0);
                    p->AddPacketTag(udpTag);
                    if(Route_log){
                        // 显示PathChoice表
                        std::cout << "Route_decision_log" << std::endl;
                        std::cout << "new flowlet" << std::endl;
                        std::cout << "ToR switch: " << m_switch_id << " UDP packet: " << PARSE_FIVE_TUPLE(ch) << " new flowlet" << std::endl;
                        std::cout << "path choice table" << std::endl;
                        printPathChoiceTable_Entry(dip);
                        std::cout << "RouteChoice: " << std::endl;
                        showRouteChoice(m_choice);
                    }
                    if (Dive_optimal_log){
                        std::cout <<"Optimal Path related info" << std::endl;
                        std::cout <<"local Dre Table" << std::endl;
                        showDreTable();
                        std::cout << "Global Dre Table" << std::endl;
                        showglobalDreTable();
                        showOptimalvsCaver(ch, m_choice);                    }
                    if(m_choice.SrcRoute){
                        uint32_t X = UpdateLocalDre(p, ch, m_choice.outPort);  // update local DRE
                        DoSwitchSend(p, ch, m_choice.outPort, ch.udp.pg);
                    }
                    else{
                        DoSwitchSendToDev(p, ch);
                        if (Route_log){
                            std::cout << "ToR switch: " << m_switch_id << " UDP packet: " << PARSE_FIVE_TUPLE(ch) << " ecmp " <<" new flowlet" <<std::endl;
                        }
                    }
                    if(Packet_begin_end_flag){
                                std::cout << "---------------UDP END-----------------\n";
                    }
                    return;
                }
                /*---- receiver-side ----*/
                p->RemovePacketTag(udpTag);
                if (Nodepass_log){
                    uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
                    std::cout << "Nodepass_log" << std::endl;
                    printf("flow id %d, dst switch %d\n", flowid, m_switch_id);
                }
                DoSwitchSendToDev(p, ch);
                if(Packet_begin_end_flag){
                                std::cout << "---------------UDP END-----------------\n";
                }
                return;
            }
            // Agg/Core switch
            assert(found && "If not ToR (leaf),CaverTag should be found");
            uint32_t hopCount = udpTag.GetHopCount() + 1;
            udpTag.SetHopCount(hopCount);
            uint8_t src_enable = udpTag.GetSrcRouteEnable();
            if(Nodepass_log){
                std::cout << "Nodepass_log" << std::endl;
                uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
                printf("flow id %d, Mid switch %d\n", flowid, m_switch_id);
                showCaverUdpinfo(udpTag);
            }
            if (src_enable){
                //source route
                uint32_t pathid = udpTag.GetPathId();
                uint32_t outPort = GetOutPortFromPath(pathid, hopCount);
                p->AddPacketTag(udpTag);
                uint32_t X = UpdateLocalDre(p, ch, outPort);  // update local DRE
                DoSwitchSend(p, ch, outPort, ch.udp.pg);
                if(Packet_begin_end_flag){
                                std::cout << "---------------UDP END-----------------\n";
                }
                return;
            }
            else{
                //ECMP
                DoSwitchSendToDev(p, ch);
                if(Packet_begin_end_flag){
                                std::cout << "---------------UDP END-----------------\n";
                }
                return;
            }
        }
        else if (ch.l3Prot == 0xFC){
            // ACK PACKET       
            CaverAckTag ackTag;
            bool found = p->PeekPacketTag(ackTag);
            if(Packet_begin_end_flag){
                                std::cout << "---------------ACK BEGIN-----------------\n";
            }
            if (m_isToR){
                if (!found)// sender-side
                {
                    uint32_t sid;
                    if(ToR_Rouding){
                        uint32_t choose_host_ip = Settings::TorSwitch_nodelist[m_switch_id][host_round_index];
                        sid = Settings::hostIp2IdMap[choose_host_ip];
                        host_round_index = (host_round_index + 1) % ToR_host_num;
                    }
                    else{
                        sid = Settings::hostIp2IdMap[ch.sip];
                    }
                    uint32_t port = id2Port[sid];
                    uint32_t ce = m_DreMap[port];
                    uint32_t localce = QuantizingX(port, ce);
                    ackTag.SetMCE(localce);
                    ackTag.SetMPathId(0);
                    ackTag.SetBestCE(localce);
                    ackTag.SetBestPathId(0);
                    ackTag.SetLength(0);
                    ackTag.SetLastSwitchId(m_switch_id);
                    ackTag.SetHostId(sid);
                    p->AddPacketTag(ackTag);

                    if (ACK_log){
                        printf("ACK info: current: Src switch %d \n", m_switch_id);
                        printf("Received ACK without CAVER Tag\n");
                        showAck_info(ch);
                        if(DreTable_log){
                            printf("ingress port %d\n", port);
                            printf("DRE info: Src switch %d\n", m_switch_id);
                            showDreTable();
                        }
                        printf("Send ACK with CAVER Tag\n");
                        showCaverAck_info(ackTag, ch);
                    }
                    DoSwitchSendToDev(p, ch);
                    if(Packet_begin_end_flag){
                                std::cout << "---------------ACK END-----------------\n";
                    }
                    return;
                }
                // receiver-side
                // *******************************information carried by the ack**********************//
                uint32_t last_swtich = ackTag.GetLastSwitchId();
                uint32_t inPort = id2Port[last_swtich];
                uint32_t remoteBestCE = ackTag.GetBestCE();
                uint32_t ce = m_DreMap[inPort];
                uint32_t localCE = QuantizingX(inPort, ce);
                uint32_t totalBestCE = std::max(localCE, remoteBestCE);
                uint32_t host_id = ackTag.GetHostId();
                uint32_t host_ip = Settings::hostId2IpMap[host_id];
                // *******************************Determine whether to update the sender's BestTable.**********************//
                uint32_t currentBestCE = 0;
                bool update = false;
                if (best_pathCE_Table[host_ip]._valid == false){
                    update = true;
                }
                else {
                        uint32_t table_portCE = QuantizingX(best_pathCE_Table[host_ip]._inPort, m_DreMap[best_pathCE_Table[host_ip]._inPort]);
                        currentBestCE = std::max(table_portCE, best_pathCE_Table[host_ip]._ce);
                        if(currentBestCE >= totalBestCE or best_pathCE_Table[host_ip]._path[0] == inPort){
                            update = true;
                        }
                }
                if (ACK_log){
                    // show ack content
                    printf("ACK info: current: Dst switch %d \n", m_switch_id);
                    printf("Received ACK with CAVER Tag\n");
                    showCaverAck_info(ackTag, ch);
                    if(DreTable_log){
                        printf("DRE info\n");
                        showPortCE(inPort);
                    }
                    printf("ACK's carried path after combine with port CE %d\n", totalBestCE);
                }
                if(BestTable_log ){
                    printf("BestTable info: Dst switch %d \n", m_switch_id);
                    printf("Before update BestTable\n");
                    printBestPathCETable_Entry(host_ip);
                    std::cout << "If choose to update: " << (update ? "true" : "false") << "\n";
                }
                if(update){
                    currentBestCE = totalBestCE;
                    best_pathCE_Table[host_ip]._valid = true;
                    best_pathCE_Table[host_ip]._updateTime = now;
                    best_pathCE_Table[host_ip]._ce = remoteBestCE;
                    best_pathCE_Table[host_ip]._inPort = inPort;
                    std::vector<uint8_t> path;
                    path.push_back((uint8_t(inPort)));
                    std::vector<uint8_t> fullpath = uint32_to_uint8(ackTag.GetBestPathId());
                    for (int i = 0; i < ackTag.GetLength(); i++) {
                        path.push_back(fullpath [i]);
                    }  
                    best_pathCE_Table[host_ip]._path = path;
                }
                if(BestTable_log){
                    printf("BestTable info: Dst switch %d \n", m_switch_id);
                    printf("After update BestTable\n");
                    printBestPathCETable_Entry(host_ip);
                }

                // *******************************Determine whether the path information carried by the packet should be retained or filtered.**********************//
                uint32_t remoteMCE = ackTag.GetMCE();
                uint32_t totalMCE = std::max(localCE, remoteMCE);
                bool M_is_usable = false;
                //if (totalMCE <= m_ce_threshold * currentBestCE){
                //    M_is_usable = true;
                //}

                if ((256 - std::min(totalMCE, 256u)) * m_ce_threshold >= 256 - (std::min(currentBestCE, 256u))) {
                    M_is_usable = true;
                }

                
                if(PathChoice_log){
                    printf("PathChoice info: current: Dst switch %d\n", m_switch_id);
                    printf("MCE: %d, BestCE: %d, currentBestCE: %d\n", totalMCE, totalBestCE, currentBestCE);
                    std::cout << "If acceptable: " << (M_is_usable ? "true" : "false") << "\n";
                }

                // *******************************update pathChoiceTable**********************//
                // get the index of current pathChoiceTable
                auto flagItr = PathChoiceFlagMap.find(host_ip);
                assert(flagItr != PathChoiceFlagMap.end() && "Cannot find dip from PathChoiceFlagMap");
                uint32_t flag = PathChoiceFlagMap[host_ip];
                PathChoiceInfo newPathChoice;
                if (M_is_usable){
                    std::vector<uint8_t> path;
                    path.push_back((uint8_t(inPort)));
                    std::vector<uint8_t> fullpath = uint32_to_uint8(ackTag.GetMPathId());
                    for (int i = 0; i < ackTag.GetLength(); i++) {
                        path.push_back(fullpath [i]);
                    }  
                    newPathChoice._path = path;
                    newPathChoice._updateTime = now;
                    newPathChoice._is_used = false;
                    newPathChoice._remoteCE = remoteMCE;
                }
                else{
                    newPathChoice._path = best_pathCE_Table[host_ip]._path;
                    newPathChoice._updateTime = now;
                    newPathChoice._is_used = false;
                    newPathChoice._remoteCE = best_pathCE_Table[host_ip]._ce;
                }
                if(PathChoice_log){
                    // *******************************Display the relevant information when updating the PathChoiceTable.**********************//
                    printf("PathChoice info: current: Dst switch %d\n", m_switch_id);
                    // 更新相关的信息
                    if(M_is_usable){
                        printf("MCE is usable, update with ACK carried path\n");
                    }
                    else{
                        printf("MCE is not usable, update with best_pathCE_Table\n");
                    }
                    printf("before update PathChoiceMapTable\n");
                    printPathChoiceFlagMap_Entry(host_ip);
                    showPathChoiceInfo(newPathChoice);
                    printf("update index: %d\n", flag);
                }


                PathChoiceTable[host_ip][flag] = newPathChoice;
                PathChoiceFlagMap[host_ip] = (PathChoiceFlagMap[host_ip] + 1) % m_pathChoice_num;

                // *******************************Display the relevant information when updating the PathChoiceTable.**********************//
                if(PathChoice_log){
                    printf("after update PathChoiceTable\n");
                    printPathChoiceTable_Entry(host_ip);
                    printf("after update PathChoiceMapTable\n");
                    printPathChoiceFlagMap_Entry(host_ip);
                }
        
                fprintf(Settings::caverLog, "Time:%ld, Switch:%u, Did:%u, update:%d, M_is_usable:%d, totalBestCe:%u|", 
                    Simulator::Now().GetNanoSeconds(), m_switch_id, host_id, update, M_is_usable, totalBestCE);
                std::vector<uint8_t> path;
                path.push_back((uint8_t(inPort)));
                std::vector<uint8_t> fullpath = uint32_to_uint8(ackTag.GetMPathId());
                for (int i = 0; i < ackTag.GetLength(); i++) {
                    path.push_back(fullpath[i]);
                }                  
                auto node_path = getPathNodeIds(path, m_switch_id);
                for (uint32_t node_id : node_path) {
                    fprintf(Settings::caverLog,"%u ", node_id);
                }
                fprintf(Settings::caverLog, "\n");p->RemovePacketTag(ackTag);
                
                DoSwitchSendToDev(p, ch);
                if(Packet_begin_end_flag){
                                std::cout << "---------------ACK END-----------------\n";
                }
                return;
            }
            // Agg/Core switch
            // *******************************the best information carried by packet**********************//
            assert(found && "If not ToR (leaf), CaverTag should be found");
            uint32_t last_swtich = ackTag.GetLastSwitchId();
            uint32_t inPort = id2Port[last_swtich];
            uint32_t remoteBestCE = ackTag.GetBestCE();
            uint32_t ce = m_DreMap[inPort];
            uint32_t localCE = QuantizingX(inPort, ce);
            uint32_t totalBestCE = std::max(localCE, remoteBestCE);
            uint32_t host_id = ackTag.GetHostId();
            uint32_t host_ip = Settings::hostId2IpMap[host_id];
            // *******************************determine whether update sender's BestTable**********************//
            uint32_t currentBestCE = 0;
            if (best_pathCE_Table[host_ip]._valid){
                uint32_t table_portCE = QuantizingX(best_pathCE_Table[host_ip]._inPort, m_DreMap[best_pathCE_Table[host_ip]._inPort]);
                currentBestCE = std::max(table_portCE, best_pathCE_Table[host_ip]._ce);
            }
            bool update = false;
            if (best_pathCE_Table[host_ip]._valid == false){
                update = true;
            }
            else {
                if(currentBestCE  >= totalBestCE or best_pathCE_Table[host_ip]._path[0] == inPort){
                    update = true;
                }
            }
            if (ACK_log){
                // display ack's content
                printf("ACK info: current: Middle switch %d \n", m_switch_id);
                printf("Received ACK with CAVER Tag\n");
                showCaverAck_info(ackTag, ch);
                printf("ingress port %d\n", inPort);
                if(DreTable_log){
                    printf("DRE info\n");
                    showPortCE(inPort);
                }
                printf("ACK's carried path after combine with port CE %d\n", totalBestCE);
                printf("currentBestCE % d\n", currentBestCE);
            }
            if(BestTable_log ){
                printf("BestTable info: Middle switch %d \n", m_switch_id);
                printf("Before update BestTable\n");
                printBestPathCETable_Entry(host_ip);
                std::cout << "If choose to update: " << (update ? "true" : "false") << "\n";
            }
            if (update){
                best_pathCE_Table[host_ip]._valid = true;
                best_pathCE_Table[host_ip]._updateTime= now;
                best_pathCE_Table[host_ip]._ce = remoteBestCE;
                best_pathCE_Table[host_ip]._inPort = inPort;
                currentBestCE = totalBestCE;
                std::vector<uint8_t> path;
                path.push_back((uint8_t(inPort)));
                std::vector<uint8_t> fullpath = uint32_to_uint8(ackTag.GetBestPathId());
                for (int i = 0; i < ackTag.GetLength(); i++) {
                    path.push_back(fullpath [i]);
                }  
                best_pathCE_Table[host_ip]._path = path;
            }
            if(BestTable_log){
                //显示更新后的bestTable
                printf("BestTable info: Middle switch %d \n", m_switch_id);
                printf("After update BestTable\n");
                printBestPathCETable_Entry(host_ip);
            }
            // *******************************Determine whether the path information carried by the packet should be retained or filtered.**********************//
            uint32_t remoteMCE = ackTag.GetMCE();
            uint32_t totalMCE = std::max(localCE, remoteMCE);
            bool M_is_usable = false;
            //if (totalMCE <= m_ce_threshold * currentBestCE){
            //    M_is_usable = true;
            //}

            if ((256 - std::min(totalMCE, 256u)) * m_ce_threshold >= 256 - std::min(currentBestCE, 256u)) {
                M_is_usable = true;
            }

            bestCaverInfo new_avaliable_path;
            new_avaliable_path._valid = true;
            new_avaliable_path._updateTime = now;
            if(ACK_log){
                printf("localCE: %d, remoteMCE: %d, remoteBestCE: %d, currentBestCE: %d\n", localCE, remoteMCE, remoteBestCE, currentBestCE);
                std::cout << "If acceptable: " << (M_is_usable ? "true" : "false") << "\n";
            }
            // *******************************store acceptable path in current acceptabl path table中**********************//
            uint32_t new_avaliable_path_localCE;
            if (M_is_usable){
                std::vector<uint8_t> path;
                path.push_back((uint8_t(inPort)));
                std::vector<uint8_t> fullpath = uint32_to_uint8(ackTag.GetMPathId());
                if(Caver_debug){
                    printf("fullpath : ");
                    showPathVec(fullpath);
                    std::cout.flush();
                }
                std::cout.flush(); 
                for (int i = 0; i < ackTag.GetLength(); i++) {
                    path.push_back(fullpath[i]);
                }  
                if(Caver_debug){
                    printf("ACKTAG:length: %d\n", ackTag.GetLength());
                    printf("path : ");
                    showPathVec(path);
                    std::cout.flush();
                }
                new_avaliable_path._path = path;
                new_avaliable_path._inPort = inPort;
                if(Caver_debug){
                    printf("new_avaliable_path._path : ");
                    printf("new_avaliable_path._path's length: %d\n", new_avaliable_path._path.size());
                    showPathVec(new_avaliable_path._path);
                    std::cout.flush();
                }
                new_avaliable_path._ce = remoteMCE;
                new_avaliable_path_localCE = localCE;  
            }
            else{
                new_avaliable_path._path = best_pathCE_Table[host_ip]._path;
                new_avaliable_path._inPort = best_pathCE_Table[host_ip]._inPort;
                new_avaliable_path._ce = best_pathCE_Table[host_ip]._ce;
                new_avaliable_path_localCE = QuantizingX(best_pathCE_Table[host_ip]._inPort, m_DreMap[best_pathCE_Table[host_ip]._inPort]);
            }
            // *******************************get the old acceptable path table information**********************//
            auto avpathItr = acceptable_path_table.find(host_ip);
            assert(avpathItr!= acceptable_path_table.end() && "Cannot find dip from AVPathTable");
            auto old_acceptable_path = acceptable_path_table[host_ip];
            if(AccceptablePath_log){
                printf("new acceptable path: ");
                showPathVec(new_avaliable_path._path);
                printf("new acceptable path CE: %d\n", new_avaliable_path._ce);
                printf("Before Acceptable update\n");
                printAcceptablePathTable_Entry(host_ip);
                printf("recorded port info in table: ");

                if (old_acceptable_path._valid){
                    showPortCE(old_acceptable_path._inPort);
                }
                else{
                    printf("No valid path\n");
                }
                std::cout.flush();
            }
            // *******************************update mpath in acktag**********************//
            if (old_acceptable_path._valid){
                ackTag.SetMPathId(Vector2PathId(old_acceptable_path._path));
                uint32_t old_acceptable_path_localCE = QuantizingX(old_acceptable_path._inPort, m_DreMap[old_acceptable_path._inPort]);
                uint32_t totalCE = std::max(old_acceptable_path_localCE, old_acceptable_path._ce);
                ackTag.SetMCE(totalCE);
            }
            else{
                ackTag.SetMPathId(Vector2PathId(new_avaliable_path._path));
                uint32_t totalCE = std::max(new_avaliable_path_localCE, new_avaliable_path._ce);
                ackTag.SetMCE(totalCE);
            }
            // *******************************update avaliable path table**********************//
            acceptable_path_table[host_ip] = new_avaliable_path;
            if(AccceptablePath_log){
                printf("After Acceptable update\n");
                printAcceptablePathTable_Entry(host_ip);
            }
            // *******************************BestPathId**********************//
            ackTag.SetBestPathId(Vector2PathId(best_pathCE_Table[host_ip]._path));
            ackTag.SetBestCE(currentBestCE);
            ackTag.SetLength(ackTag.GetLength() + 1);
            ackTag.SetLastSwitchId(m_switch_id);
            ackTag.SetHostId(host_id);
            p->AddPacketTag(ackTag);
            if (ACK_log){
                printf("Send ACK with CAVER Tag\n");
                showCaverAck_info(ackTag, ch);
            }
            DoSwitchSendToDev(p, ch);
            if(Packet_begin_end_flag){
                std::cout << "---------------ACK END-----------------\n";
            }
            return;
        }
    }


    CaverRouteChoice CaverRouting::ChoosePath(uint32_t dip, CustomHeader ch){
        auto now = Simulator::Now();
        auto pathItr = PathChoiceTable.find(dip);
        assert(pathItr != PathChoiceTable.end() && "Cannot find dip from PathChoiceTable");
        auto pathChoiceVec = PathChoiceTable[dip];
        CaverRouteChoice choice;
        auto flagItr = PathChoiceFlagMap.find(dip);
        assert(flagItr != PathChoiceFlagMap.end() && "Cannot find dip from PathChoiceFlagMap");
        uint32_t flag = PathChoiceFlagMap[dip];
        // flag表示当前待插入的位置
        bool find_path = false;
        std::list<int> valid_path_index_list;
        for (int index = flag;;) {
            auto pathChoice = pathChoiceVec[index];
            if (now - pathChoice._updateTime < m_patchoiceTimeout) {
                valid_path_index_list.push_back(index);
                if (!pathChoice._is_used) {
                    find_path = true;
                    choice.SrcRoute = true;
                    choice.outPort = pathChoice._path[0];
                    choice.pathid = Vector2PathId(pathChoice._path);
                    choice.pathVec = pathChoice._path;
                    choice.remoteCE = pathChoice._remoteCE;
                    PathChoiceTable[dip][index]._is_used = true;
                }
            }
            index = (index - 1 + m_pathChoice_num) % m_pathChoice_num;
            if (index == flag) break;
        }
        if(show_acceptable_detail){
            printf("CHOOSEPATH:num of valid paths:%zu, find an unused path:%d#", valid_path_index_list.size(), (int)find_path);
                for (auto index : valid_path_index_list) {
                    auto node_path = getPathNodeIds(pathChoiceVec[index]._path, m_switch_id);
                    for (uint32_t node_id : node_path) {
                        printf("%u ", node_id);
                    }
                    printf("%u ", pathChoiceVec[index]._remoteCE);
                    printf("%ld ", pathChoiceVec[index]._updateTime.ToInteger(Time::Unit::NS));
                    printf("|");
            }
            uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
            printf("flowid:%u\n", flowid);
        }
        if (find_path){
            return choice;
        }
        else{
            if (!valid_path_index_list.empty()){
                uint32_t random_index = getRandomElement(valid_path_index_list);
                auto pathChoice = pathChoiceVec[random_index];
                choice.SrcRoute = true;
                choice.outPort = pathChoice._path[0];
                choice.pathid = Vector2PathId(pathChoice._path);
                choice.pathVec = pathChoice._path;
                choice.remoteCE = pathChoice._remoteCE;
                PathChoiceTable[dip][random_index]._is_used = true;
            }
            else{
                choice.SrcRoute = false;
                choice.outPort = 0;
                choice.pathid = 0;
            }
            return choice;
        }
    }
    CaverRouteChoice CaverRouting::ChoosePathWithDetail(uint32_t dip, CustomHeader ch) {
        auto now = Simulator::Now();
        auto pathItr = PathChoiceTable.find(dip);
        assert(pathItr != PathChoiceTable.end() && "Cannot find dip from PathChoiceTable");
        auto pathChoiceVec = PathChoiceTable[dip];
        CaverRouteChoice choice;
        auto flagItr = PathChoiceFlagMap.find(dip);
        assert(flagItr != PathChoiceFlagMap.end() && "Cannot find dip from PathChoiceFlagMap");
        uint32_t flag = PathChoiceFlagMap[dip];
        bool has_valid_paths = false;
        bool choose_newest_path = false;
        bool choose_srcRoute = false;
        bool find_path = false;
        Time choose_path_time;
        Time newest_path_time;
        // Check if the latest path is available and has not been used before.
        int newest_index = (flag -1) % m_pathChoice_num;
        auto newest_path_choice = pathChoiceVec[newest_index];
        if (now - newest_path_choice._updateTime < m_patchoiceTimeout) {
            has_valid_paths = true;
            newest_path_time = newest_path_choice._updateTime;
            if (!newest_path_choice._is_used){
                choose_newest_path = true;
            }
        }
        //select path
        for (int index = flag;;) {
            if (index == 0) {
                index = m_pathChoice_num - 1;
            } else {
                --index;
            }
            // Stop if it returns to the starting index.
            auto pathChoice = pathChoiceVec[index];
            if (now - pathChoice._updateTime < m_patchoiceTimeout) {
                has_valid_paths = true;
                if (!pathChoice._is_used) {
                    find_path = true;
                    choice.SrcRoute = true;
                    choice.outPort = pathChoice._path[0];
                    choice.pathid = Vector2PathId(pathChoice._path);
                    choice.pathVec = pathChoice._path;
                    choice.remoteCE = pathChoice._remoteCE;
                    PathChoiceTable[dip][index]._is_used = true;
                    choose_path_time = pathChoice._updateTime;
                    choose_srcRoute = true;
                    break;
                }
            }
            index = (index - 1 + m_pathChoice_num) % m_pathChoice_num;
            if (index == flag) break;
        }
        //use ecmp if there's no usable path
        if(!find_path){
            choice.SrcRoute = false;
            choice.outPort = 0;
            choice.pathid = 0;
            choose_srcRoute = false;
        }
        // Output the required information
        std::cout << "Path_choice_detail info: ";
        std::cout << "srchostId: " << Settings::hostIp2IdMap[ch.sip] << ", dsthostId: " << Settings::hostIp2IdMap[ch.dip];
        std::cout << ", has_valid_paths: " << (has_valid_paths ? "true" : "false");
        if (has_valid_paths) {
            std::cout << ", choose_newest_path: " << (choose_newest_path ? "true" : "false");
            if (!choose_newest_path) {
                std::cout << ", choose_srcRoute: " << (choose_srcRoute ? "true" : "false");
                if (choose_srcRoute) {
                    std::cout << ", newest_time: " << newest_path_time.GetMicroSeconds();
                    std::cout << ", choose_time: " << choose_path_time.GetMicroSeconds();
                }
            }
        }
        std::cout << std::endl;
        return choice;
    }
    uint32_t CaverRouting::Vector2PathId(std::vector<uint8_t> vec) {
        uint32_t result = 0; 
        result |= vec[0];

        for (size_t i = 1; i < vec.size(); ++i) {
            result <<= 8; 
            result |= vec[i]; 
        }

        size_t remainingBytes = 4 - vec.size();
        while (remainingBytes > 0) {
            result <<= 8; // 左移 8 位
            --remainingBytes;
        }

        return result;
    }
    // *******************************Add end**********************//
    uint32_t CaverRouting::mergePortAndVector(uint8_t port, std::vector<uint8_t> vec) {
        uint32_t result = port; 

        for (size_t i = 0; i < vec.size(); ++i) {
            result <<= 8; 
            result |= vec[i]; 
        }

        size_t remainingBytes = 3 - vec.size();
        while (remainingBytes > 0) {
            result <<= 8; 
            --remainingBytes;
        }

        return result;
    }
    void CaverRouting::SetConstants(Time dreTime, Time agingTime, Time flowletTimeout,
                                    uint32_t quantizeBit, double alpha, double ce_threshold, Time patchoiceTimeout, uint32_t pathChoice_num, 
                                    Time tau, bool useEWMA) {
        m_dreTime = dreTime;
        m_agingTime = agingTime;
        m_flowletTimeout = flowletTimeout;
        m_quantizeBit = quantizeBit;
        m_alpha = alpha;

        m_ce_threshold = ce_threshold;
        m_patchoiceTimeout = patchoiceTimeout;
        m_pathChoice_num = pathChoice_num;
        this->tau = tau;
        this->useEWMA = useEWMA;
    }

    void CaverRouting::DoDispose() {
        for (auto i : m_flowletTable) {
            delete (i.second);
        }
        m_dreEvent.Cancel();
        m_agingEvent.Cancel();
    }

    void CaverRouting::DreEvent() {
        if (useEWMA) {
            return;
        }
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
        m_dreEvent = Simulator::Schedule(m_dreTime, &CaverRouting::DreEvent, this);
    }

    void CaverRouting::AgingEvent() {
        auto now = Simulator::Now();
        auto itr = best_pathCE_Table.begin();
        for (; itr != best_pathCE_Table.end(); ++itr){
            bestCaverInfo info = itr->second;
            if(now - info._updateTime > m_agingTime){
                (itr->second)._ce = 0;
                (itr->second)._valid = false;
            }
        }
        NS_LOG_FUNCTION(Simulator::Now());
        auto itr2 = m_flowletTable.begin();
        while (itr2 != m_flowletTable.end()) {
            if (now - ((itr2->second)->_activeTime) > m_agingTime) {
                delete(itr2->second); // delete pointer
                itr2 = m_flowletTable.erase(itr2);
            } else {
                ++itr2;
            }
        }

        m_agingEvent = Simulator::Schedule(m_agingTime, & CaverRouting::AgingEvent, this);
    }

    void CaverRouting::printBestPathCETable() {
        for (const auto& entry : best_pathCE_Table) {
            const auto& key = entry.first;
            const auto& info = entry.second;
            uint32_t id = Settings::hostIp2IdMap[key];
            std::cout << "dst_ip: " << id << "\n";
            printBestPathCETable_Entry(key);
        }
    }
    void CaverRouting::printPathChoiceTable() {
        for (const auto& entry : PathChoiceTable) {
            const auto& key = entry.first;
            const auto& info = entry.second;
            uint32_t id = Settings::hostIp2IdMap[key];
            std::cout << "dst_id: " << id << "\n";
            printPathChoiceTable_Entry(key);
        }
    }
    void CaverRouting::printPathChoiceFlagMap() {
        for (const auto& entry : PathChoiceFlagMap) {
            const auto& destination = entry.first;
            const auto& pathIndex = entry.second;
            uint32_t id = Settings::hostIp2IdMap[destination];
            std::cout << "Destination: " << id << "\n";
            printPathChoiceFlagMap_Entry(destination);
        }
    }
    void CaverRouting::printBestPathCETable_Entry(uint32_t dip) {
        auto dstIter = best_pathCE_Table.find(dip);
        if (dstIter == best_pathCE_Table.end()) {
            std::cout << "Destination IP not found in BestPathCETable\n";
            return;
        }
        else {
            bestCaverInfo info = dstIter->second;
            std::cout << "  CE: " << info._ce << "\n";
            std::cout << "  Path: ";
            for (const auto& p : info._path) {
                std::cout << static_cast<int>(p) << " ";
            }
            std::cout << "\n  Update Time: " << info._updateTime.GetMicroSeconds() << "\n";
            std::cout << "\n  Valid: " << (info._valid ? "true" : "false") << "\n";
            std::cout << "  InPort: " << info._inPort << "\n";
            std::cout << "-------------------------\n";
        }
    }
    void CaverRouting::printAcceptablePathTable(){
        for (const auto& entry : acceptable_path_table) {
            const auto& key = entry.first;
            const auto& info = entry.second;
            uint32_t id = Settings::hostIp2IdMap[key];
            std::cout << "dst_ip: " << id << "\n";
            printAcceptablePathTable_Entry(key);
        }
    }
    void CaverRouting::printAcceptablePathTable_Entry(uint32_t dip){
        auto dstIter = acceptable_path_table.find(dip);
        if (dstIter == acceptable_path_table.end()) {
            std::cout << "Destination IP not found in acceptable_path_table\n";
            return;
        }
        else {
            bestCaverInfo info = dstIter->second;
            std::cout << "  CE: " << info._ce << "\n";
            std::cout << "  Path: ";
            for (const auto& p : info._path) {
                std::cout << static_cast<int>(p) << " ";
            }
            std::cout << "\n  Update Time: " << info._updateTime.GetMicroSeconds() << "\n";
            std::cout << "\n  Valid: " << (info._valid ? "true" : "false") << "\n";
            std::cout << "  InPort: " << info._inPort << "\n";
            std::cout << "-------------------------\n";
        }
    }
    void CaverRouting::printPathChoiceTable_Entry(uint32_t dip) {
        auto dstIter = PathChoiceTable.find(dip);
        if (dstIter == PathChoiceTable.end()) {
            std::cout << "Destination IP not found in PathChoiceTable\n";
            return;
        }
        else {
            auto paths = dstIter->second;
            for (const auto& pathInfo : paths) {
                std::cout << "  Path: ";
                for (const auto& node : pathInfo._path) {
                    std::cout << static_cast<int>(node) << " ";
                }
                std::cout << "\n  Update Time: " << pathInfo._updateTime.GetMicroSeconds() << "\n";
                std::cout << "\n  Is Used: " << (pathInfo._is_used ? "true" : "false") << "\n";
                std::cout << "-------------------------\n";
            }
        }
    }
    void CaverRouting::showPathChoiceInfo(PathChoiceInfo pc){
        std::cout << "  Path: ";
        for (const auto& node : pc._path) {
            std::cout << static_cast<int>(node) << " ";
        }
        std::cout << "\n  Update Time: " << pc._updateTime.GetMicroSeconds() << "\n";
        std::cout << "\n  Is Used: " << (pc._is_used ? "true" : "false") << "\n";
    }
    void CaverRouting::printPathChoiceFlagMap_Entry(uint32_t dip){
        auto dstIter = PathChoiceFlagMap.find(dip);
        if (dstIter == PathChoiceFlagMap.end()) {
            std::cout << "Destination IP not found in PathChoiceFlagMap\n";
            return;
        }
        else {
            uint32_t id = Settings::hostIp2IdMap[dip];
            std::cout << "Destination: " << id << ", Path Index: " << dstIter->second << "\n";
        }
    }
    
    void CaverRouting::showCaverAck_info(CaverAckTag ackTag, CustomHeader ch){
        uint32_t ack_src_id = Settings::hostIp2IdMap[ch.sip];
        uint32_t ack_dst_id = Settings::hostIp2IdMap[ch.dip];
        uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.dip], Settings::hostIp2IdMap[ch.sip], ch.udp.dport, ch.udp.sport)];
        printf("ACK of flow id: %d, Ack from host %d to host %d\n", flowid, ack_src_id, ack_dst_id);
        std::vector<uint8_t> fullMPath = uint32_to_uint8(ackTag.GetMPathId());
        std::vector<uint8_t> showMPath;
        for (int i = 0; i < ackTag.GetLength(); i++) {
            showMPath.push_back(fullMPath[i]);
        }
        std::vector<uint8_t> fullBestPath = uint32_to_uint8(ackTag.GetBestPathId());
        std::vector<uint8_t> showBestPath;
        for (int i = 0; i < ackTag.GetLength(); i++) {
            showBestPath.push_back(fullBestPath[i]);
        }
        std::cout << "m_last_switch_id" << ackTag.GetLastSwitchId() << std::endl;
        std::cout << "m_host_id: " << ackTag.GetHostId() << std::endl;
        std::cout << "m_length: " << static_cast<unsigned int>(ackTag.GetLength())<<std::endl;
        std::cout << "m_pathId: ";
        for (int i = 0; i < showMPath.size(); i++) {
            std::cout << static_cast<int>(showMPath[i]) << "->";
        }
        std::cout <<  std::endl;
        std::cout << "m_ce: " << ackTag.GetMCE() << std::endl;
        std::cout << "best_path_id: ";
        for (int i = 0; i < showBestPath.size(); i++) {
            std::cout << static_cast<int>(showBestPath[i]) << "->";
        }
        std::cout <<  std::endl;
        std::cout <<"best_ce: " << ackTag.GetBestCE() << std::endl;
    }

    void CaverRouting::showAck_info(CustomHeader ch){
        uint32_t ack_src_id = Settings::hostIp2IdMap[ch.sip];
        uint32_t ack_dst_id = Settings::hostIp2IdMap[ch.dip];
        uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.dip], Settings::hostIp2IdMap[ch.sip], ch.udp.dport, ch.udp.sport)];
        printf("ACK of flow id: %d, Ack from host %d to host %d\n", flowid, ack_src_id, ack_dst_id);
    }

    void CaverRouting::showPortCE(uint32_t port){
        uint32_t ce = m_DreMap[port];
        uint32_t localce = QuantizingX(port, ce);
        std::cout << "Port: " << port << ", CE: " << ce << ",localCE: " << localce << std::endl;
    }

    void CaverRouting::showRouteChoice(CaverRouteChoice rc){
        std::cout << "SrcRoute: " << (rc.SrcRoute ? "true" : "false") << "\n";
        std::cout << "outPort: " << rc.outPort << "\n";
        if(rc.SrcRoute){
            std::vector<uint8_t> Path = rc.pathVec;
            std::cout << "path_id: ";
            for (int i = 0; i < Path.size(); i++) {
                std::cout << static_cast<int>(Path[i]) << "->";
            }
        }
        std::cout <<std::endl;
    }
    void CaverRouting::showCaverUdpinfo(CaverUdpTag udpTag){
        std::cout << "SrcRoute: " << (udpTag.GetSrcRouteEnable() == 1 ? "true" : "false") << "\n";
        std::cout << "hopCount: " << udpTag.GetHopCount() << "\n";
        std::cout << "pathId: " ;
        std::vector<uint8_t> showpath = uint32_to_uint8(udpTag.GetPathId());
        for (int i = 0; i < 4; i++) {
            std::cout << static_cast<int>(showpath[i]) << "->";
        }
        std::cout << std::endl;
    }
    void CaverRouting::showDreTable(){
        for (auto it = m_DreMap.begin(); it != m_DreMap.end(); ++it) {
            uint32_t ce = it->second;
            uint32_t localce = QuantizingX(it->first, ce);
            std::cout << "Port: " << it->first << ", CE: " << it->second << ",localCE: " << localce << std::endl;
        }
    }

    void CaverRouting::showglobalDreTable(){
        for (auto it = m_DreMap.begin(); it != m_DreMap.end(); ++it) {
            uint32_t outPort = it->first;
            uint32_t neighbor_id = Settings::m_nodeInterfaceMap[m_switch_id][outPort];
            uint32_t X = Settings::global_dre_map[{m_switch_id, neighbor_id}];
            uint32_t localce = QuantizingX(outPort, X);
            std::cout << "Port: " << outPort << ",global_localCE: " << localce << ",global_CE_store" << Settings::global_CE_map[{m_switch_id, neighbor_id}]<< std::endl;
        }
    }

    void CaverRouting::showPathVec(std::vector<uint8_t> path){
        for (int i = 0; i < path.size(); i++) {
            std::cout << static_cast<int>(path[i]) << "->";
        }
        std::cout << std::endl;
    }

    std::vector<uint32_t> CaverRouting::getPathNodeIds(const std::vector<uint8_t>& pathVec, uint32_t currentNodeId) {
        std::vector<uint32_t> nodePath;
        uint32_t currentNode = currentNodeId;

        nodePath.push_back(currentNode);

        for (uint8_t outPortId : pathVec) {
            assert(Settings::m_nodeInterfaceMap.find(currentNode) != Settings::m_nodeInterfaceMap.end() &&
               "Current node ID not found in m_nodeInterfaceMap.");

            const auto& interfaceMap = Settings::m_nodeInterfaceMap[currentNode];

            if (interfaceMap.find(outPortId) == interfaceMap.end()) {
                throw std::runtime_error("Out port ID not found in interface map for current node.");
            }

            uint32_t nextNodeId = interfaceMap.at(outPortId);

            nodePath.push_back(nextNodeId);

            currentNode = nextNodeId;
        }

        return nodePath;
    }
    void CaverRouting::showOptimalvsCaver(CustomHeader ch, CaverRouteChoice m_choice){
        if(Caver_debug){
            std::cout << "Dre Table: " << std::endl;
            showDreTable();
        }
        std::pair<std::vector<uint32_t>, uint32_t> result = Settings::FindMinCostPath(m_switch_id, Settings::hostIp2IdMap[ch.dip]);
        std::vector<uint32_t> optimalPath = result.first;
        uint32_t optimalCE = result.second;
        std::cout << "Optimal Path: ";
        for (int i = 0; i < optimalPath.size(); i++) {
            std::cout << optimalPath[i] << "->";
        }
        std::cout << "Optimal CE: " << optimalCE << std::endl;
        if (m_choice.SrcRoute){
            std::vector<uint32_t> caverPath = getPathNodeIds(m_choice.pathVec, m_switch_id);            
            uint32_t localCE = QuantizingX(m_choice.outPort, m_DreMap[m_choice.outPort]);
            uint32_t caverCE = std::max(localCE, m_choice.remoteCE);
            std::cout << "Caver Path: ";
            for (int i = 0; i < caverPath.size(); i++) {
                std::cout << caverPath[i] << "->";
            }
            std::cout << "Caver outport: " << m_choice.outPort << std::endl;
            std::cout << "Caver CE: " << caverCE << std::endl;
        }
        else{
            std::cout << "Caver Path: ECMP" << std::endl;
        }
    }
    
    int CaverRouting::getRandomElement(const std::list<int>& myList) {
            if (myList.empty()) {
                throw std::runtime_error("List is empty");
            }

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, myList.size() - 1);

            int randomIndex = dis(gen);

            auto it = myList.begin();
            std::advance(it, randomIndex);

            return *it;
        }
}
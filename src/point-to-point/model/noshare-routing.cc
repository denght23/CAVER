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

#include "ns3/noshare-routing.h"
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

   

    /*---- Noshare-Routing -----*/
    NoshareRouting::NoshareRouting() {
        m_isToR = false;
        m_switch_id = (uint32_t)-1;

        // set constants
        m_dreTime = Time(MicroSeconds(200));
        m_agingTime = Time(MilliSeconds(10));
        m_flowletTimeout = Time(MilliSeconds(1));
        m_quantizeBit = 3;
        m_alpha = 0.2;
        // CAVER的参数：路径选择的阈值，超时时间，路径选择的数量
        m_ce_threshold = 1.5;
        m_patchoiceTimeout = Time(MilliSeconds(10));
        m_pathChoice_num = 5;
    }

    // it defines flowlet's 64bit key (order does not matter)
    uint64_t NoshareRouting::GetQpKey(uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg) {
        return ((uint64_t)dip << 32) | ((uint64_t)sport << 16) | (uint64_t)pg | (uint64_t)dport;
    }

    TypeId NoshareRouting::GetTypeId(void) {
        static TypeId tid =
            TypeId("ns3::NoshareRouting").SetParent<Object>().AddConstructor<NoshareRouting>();

        return tid;
    }

    uint32_t NoshareRouting::GetOutPortFromPath(const uint32_t& path, const uint32_t& hopCount) {
        std::vector<uint8_t> bytes(4);
        bytes[0] = (path >> 24) & 0xFF; // 获取高位字节
        bytes[1] = (path >> 16) & 0xFF;
        bytes[2] = (path >> 8) & 0xFF;
        bytes[3] = path & 0xFF; // 获取低位字节
        return bytes[hopCount];
    }

    // void CaverRouting::SetOutPortToPath(uint32_t& path, const uint32_t& hopCount,
    //                                     const uint32_t& outPort) {
    //     ((uint8_t*)&path)[hopCount] = outPort;
    // }


    void NoshareRouting::DoSwitchSend(Ptr<Packet> p, CustomHeader& ch, uint32_t outDev, uint32_t qIndex) {
        m_switchSendCallback(p, ch, outDev, qIndex);
    }
    void NoshareRouting::DoSwitchSendToDev(Ptr<Packet> p, CustomHeader& ch) {
        m_switchSendToDevCallback(p, ch);
    }

    void NoshareRouting::SetSwitchSendCallback(SwitchSendCallback switchSendCallback) {
        m_switchSendCallback = switchSendCallback;
    }

    void NoshareRouting::SetSwitchSendToDevCallback(SwitchSendToDevCallback switchSendToDevCallback) {
        m_switchSendToDevCallback = switchSendToDevCallback;
    }

    void NoshareRouting::SetSwitchInfo(bool isToR, uint32_t switch_id) {
        m_isToR = isToR;
        m_switch_id = switch_id;
        if (m_isToR){
            ToR_host_num = Settings::TorSwitch_nodelist[m_switch_id].size();
            host_round_index = 0;
        }
    }

    void NoshareRouting::SetLinkCapacity(uint32_t outPort, uint64_t bitRate) {
        auto it = m_outPort2BitRateMap.find(outPort);
        if (it != m_outPort2BitRateMap.end()) {
            // already exists, then check matching
            NS_ASSERT_MSG(it->second == bitRate,
                        "bitrate already exists, but inconsistent with new input");
        } else {
            m_outPort2BitRateMap[outPort] = bitRate;
        }
    }

    uint32_t NoshareRouting::UpdateLocalDre(Ptr<Packet> p, CustomHeader ch, uint32_t outPort) {
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

   uint32_t NoshareRouting::QuantizingX(uint32_t outPort, uint32_t X) {
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
            //显示当前的bitrate等信息：
            std::cout << "Debug local CE related param info:" << std::endl;
            std::cout << "Port: " << outPort << ", Rate: " << it->second << std::endl;
            std::cout << "alpha: " << m_alpha << std::endl;
            std::cout << "DreTime: " << m_dreTime.GetSeconds() << std::endl;
        }
        uint64_t bitRate = it->second;
        double ratio = static_cast<double>(X * 8) / (bitRate * tau.GetSeconds());
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
    std::vector<uint8_t> NoshareRouting::uint32_to_uint8(uint32_t number) {
        std::vector<uint8_t> bytes(4);
        bytes[0] = (number >> 24) & 0xFF; // 获取高位字节
        bytes[1] = (number >> 16) & 0xFF;
        bytes[2] = (number >> 8) & 0xFF;
        bytes[3] = number & 0xFF; // 获取低位字节
        return bytes;
    }
    void NoshareRouting::RouteInput(Ptr<Packet> p, CustomHeader ch){
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
        if (!m_dreEvent.IsRunning() && !useEWMA) {
            NS_LOG_FUNCTION("Caver routing restarts dre event scheduling, Switch:" << m_switch_id
                                                                                << now);
            m_dreEvent = Simulator::Schedule(m_dreTime, &NoshareRouting::DreEvent, this);
        }

        // Turn on aging event scheduler if it is not running
        if (!m_agingEvent.IsRunning()) {
            NS_LOG_FUNCTION("Caver routing restarts aging event scheduling:" << m_switch_id << now);
            m_agingEvent = Simulator::Schedule(m_agingTime, &NoshareRouting::AgingEvent, this);
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
                                    std::cout << "Route_decision_info:" << std::endl;
                                    //显示时间now以及记录的激活时间，以及显示flowlet的npacket计数
                                    uint32_t flow_id = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
                                    std::cout << "flow_id: " << flow_id << std::endl;
                                    std::cout << "nPacket:" << flowlet->_nPackets << std::endl;
                                    std::cout << "now: " << now.GetSeconds() << " activeTime: " << flowlet->_activatedTime.GetSeconds() << std::endl;
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
                                    std::cout << "Route_decision_info:" << std::endl;
                                    uint32_t flow_id = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
                                    std::cout << "flow_id: " << flow_id << std::endl;
                                    std::cout << "nPacket:" << flowlet->_nPackets << std::endl;
                                    std::cout << "now: " << now.GetSeconds() << " activeTime: " << flowlet->_activatedTime.GetSeconds() << std::endl;
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
                        m_choice = ChoosePath(dip, ch);
                        if(flowlet_log){
                            std::cout << "Flowlet info: Flowlet expires, calculate the new port" << std::endl;
                            //显示当前时间以及flowlet的activeTime，flowid
                            std::cout << "now: " << now.GetSeconds() << " activeTime: " << flowlet->_activatedTime.GetSeconds() << std::endl;
                            uint32_t flow_id = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
                            std::cout << "flow_id: " << flow_id << std::endl;
                        }
                        flowlet->_activatedTime = now;
                        flowlet->_activeTime = now;
                        flowlet->_nPackets = 0;
                        flowlet->_PathId = m_choice.pathid;
                        flowlet->_SrcRoute_ENABLE = m_choice.SrcRoute;
                        flowlet->_outPort = m_choice.outPort;
                        udpTag.SetSrcRouteEnable(m_choice.SrcRoute);
                        udpTag.SetPathId(m_choice.pathid);
                        udpTag.SetHopCount(0);
                        p->AddPacketTag(udpTag);
                        if(Route_log){
                            // 显示PathChoice表
                            std::cout << "Route_decision_info" << std::endl;
                            std::cout << "expired flowlet" << std::endl;
                            //显示flowid
                            uint32_t flow_id = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
                            std::cout << "flow_id: " << flow_id << std::endl;
                            std::cout << "ToR switch: " << m_switch_id << " UDP packet: " << PARSE_FIVE_TUPLE(ch) << " new flowlet" << std::endl;
                            std::cout << "path choice table info:" << std::endl;
                            printPathChoiceTable_Entry(dip);
                            std::cout << "RouteChoice info: " << std::endl;
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
                    m_choice = ChoosePath(dip, ch);
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
                    if(flowlet_log){
                        std::cout << "Flowlet info: Flowlet not exits, calculate the path" << std::endl;
                        //显示当前时间以及flowlet的activeTime，flowid
                        std::cout << "now: " << now.GetSeconds() << " activeTime: " << newFlowlet->_activatedTime.GetSeconds() << std::endl;
                        uint32_t flow_id = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
                        std::cout << "flow_id: " << flow_id << std::endl;
                    }
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
                        // 显示本地dre表项：
                        std::cout <<"Optimal Path related info" << std::endl;
                        if (Caver_debug){
                            std::cout <<"local Dre Table" << std::endl;
                            showDreTable();
                            std::cout << "Global Dre Table" << std::endl;
                            showglobalDreTable();
                        }
                        showOptimalvsCaver(ch, m_choice);
                    }
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
                // 显示udp包携带的路由信息
                showCaverUdpinfo(udpTag);
            }
            if (src_enable){
                //源路由
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
                // *******************************数据包携带的信息**********************//
                uint32_t last_swtich = ackTag.GetLastSwitchId();
                uint32_t inPort = id2Port[last_swtich];
                uint32_t remoteBestCE = ackTag.GetBestCE();
                uint32_t ce = m_DreMap[inPort];
                uint32_t localCE = QuantizingX(inPort, ce);
                uint32_t totalBestCE = std::max(localCE, remoteBestCE);
                uint32_t host_id = ackTag.GetHostId();
                uint32_t host_ip = Settings::hostId2IpMap[host_id];
                // *******************************判断是否更新发送端的BestTable**********************//
                uint32_t currentBestCE = 0;
                bool update = false;
                if (best_pathCE_Table[host_ip]._valid == false){
                    update = true;
                }
                else {
                        uint32_t table_portCE = QuantizingX(best_pathCE_Table[host_ip]._inPort, m_DreMap[best_pathCE_Table[host_ip]._inPort]);
                        currentBestCE = std::max(table_portCE, best_pathCE_Table[host_ip]._ce);
                        if(currentBestCE >= totalBestCE or best_pathCE_Table[host_ip]._inPort == inPort){
                            update = true;
                        }
                }
                if (ACK_log){
                    // 显示ACK的内容
                    printf("ACK info: current: Dst switch %d \n", m_switch_id);
                    printf("Received ACK with CAVER Tag\n");
                    showCaverAck_info(ackTag, ch);
                    // 显示ingress port的DRE信息
                    if(DreTable_log){
                        printf("DRE info\n");
                        showPortCE(inPort);
                    }
                    std::cout << "localCE: " << localCE << ", remoteBestCE: " << remoteBestCE << ", totalBestCE: " << totalBestCE << std::endl;
                    //显示拼接后的ACK携带的最优路径的信息
                    printf("ACK's carried path after combine with port CE %d\n", totalBestCE);
                }
                if(BestTable_log){
                    //显示更新前的bestTable
                    printf("BestTable info: Dst switch %d \n", m_switch_id);
                    printf("Before update BestTable\n");
                    printBestPathCETable_Entry(host_ip);
                     //显示是否决定更新
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
                    //显示更新后的bestTable
                    printf("After update BestTable\n");
                    printBestPathCETable_Entry(host_ip);
                }

                // *******************************判断数据包携带的路径信息是应该保留还是被过滤**********************//
                uint32_t remoteMCE = ackTag.GetMCE();
                uint32_t totalMCE = std::max(localCE, remoteMCE);
                bool M_is_usable = false;
                // TODO：引入新的阈值机制；
                if ((256 - std::min(totalMCE, 256u)) * m_ce_threshold >= 256 - (std::min(currentBestCE, 256u))) {
                    M_is_usable = true;
                }
                if(PathChoice_log){
                    printf("PathChoice info: current: Dst switch %d\n", m_switch_id);
                    printf("remoteMCE: %d, localCE: %d, totalMCE: %d\n", remoteMCE, localCE, totalMCE);
                    printf("ACK record totalBestCE: %d, currentBestCE: %d\n", totalBestCE, currentBestCE);
                    printf("CE threshold: %f\n", m_ce_threshold * currentBestCE);
                    std::cout << "If acceptable: " << (M_is_usable ? "true" : "false") << "\n";
                }

                // *******************************更新pathChoiceTable**********************//
                // 获取当前的pathChoiceTable的index
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
                    // TODO: 也使用携带的best来进行更新
                    // 也可以使用bestTable表中的path
                    newPathChoice._path = best_pathCE_Table[host_ip]._path;
                    newPathChoice._updateTime = now;
                    //TODO：这里如果使用best table的路径的话，是否也可以考虑一下记录一下best 被使用的次数
                    newPathChoice._is_used = false;
                    newPathChoice._remoteCE = best_pathCE_Table[host_ip]._ce;
                }
                if(PathChoice_log){
                    // *******************************显示更新PathChoiceTable时相关信息**********************//
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

                // *******************************显示更新PathChoiceTable时相关信息**********************//
                if(PathChoice_log){
                    printf("after update PathChoiceTable\n");
                    printPathChoiceTable_Entry(host_ip);
                    printf("after update PathChoiceMapTable\n");
                    printPathChoiceFlagMap_Entry(host_ip);
                }
                p->RemovePacketTag(ackTag);
                DoSwitchSendToDev(p, ch);
                if(Packet_begin_end_flag){
                                std::cout << "---------------ACK END-----------------\n";
                }
                return;
            }
            // Agg/Core switch
            // *******************************数据包携带的最优信息**********************//
            assert(found && "If not ToR (leaf), CaverTag should be found");
            uint32_t last_swtich = ackTag.GetLastSwitchId();
            uint32_t inPort = id2Port[last_swtich];
            uint32_t remoteBestCE = ackTag.GetBestCE();
            uint32_t remoteGoodCE = ackTag.GetMCE();
            uint32_t ce = m_DreMap[inPort];
            uint32_t localCE = QuantizingX(inPort, ce);
            uint32_t totalBestCE = std::max(localCE, remoteBestCE);
            uint32_t totalGoodCE = std::max(localCE, remoteGoodCE);
            uint32_t host_id = ackTag.GetHostId();
            uint32_t host_ip = Settings::hostId2IpMap[host_id];
            std::vector<uint8_t> new_best_path;
                new_best_path.push_back((uint8_t(inPort)));
                std::vector<uint8_t> fullpath = uint32_to_uint8(ackTag.GetBestPathId());
                for (int i = 0; i < ackTag.GetLength(); i++) {
                    new_best_path.push_back(fullpath [i]);
            }  
            ackTag.SetBestPathId(Vector2PathId(new_best_path));
            std::vector<uint8_t> new_good_path;
                new_good_path.push_back((uint8_t(inPort)));
                std::vector<uint8_t> fullMpath = uint32_to_uint8(ackTag.GetMPathId());
                for (int i = 0; i < ackTag.GetLength(); i++) {
                    new_good_path.push_back(fullMpath [i]);
            }  
            ackTag.SetMPathId(Vector2PathId(new_good_path));
            // *******************************BestPathId的部分**********************//
            ackTag.SetBestCE(totalBestCE);
            ackTag.SetMCE(totalGoodCE);
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


    CaverRouteChoice NoshareRouting::ChoosePath(uint32_t dip, CustomHeader ch){
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
                if (!pathChoice._is_used)
                {
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

    uint32_t NoshareRouting::Vector2PathId(std::vector<uint8_t> vec) {
        uint32_t result = 0; // 先将 port 存入结果中
        result |= vec[0];

        // 将 vector 中的元素逐个存入结果中
        for (size_t i = 1; i < vec.size(); ++i) {
            result <<= 8; // 左移 8 位，腾出一个字节的空间
            result |= vec[i]; // 将 vector 中的元素放入结果中
        }

        // 如果 vector 的大小不足 4 个字节，补充 0
        size_t remainingBytes = 4 - vec.size();
        while (remainingBytes > 0) {
            result <<= 8; // 左移 8 位
            --remainingBytes;
        }

        return result;
    }
    // *******************************Add end**********************//
    uint32_t NoshareRouting::mergePortAndVector(uint8_t port, std::vector<uint8_t> vec) {
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
    void NoshareRouting::SetConstants(Time dreTime, Time agingTime, Time flowletTimeout,
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

    void NoshareRouting::DoDispose() {
        for (auto i : m_flowletTable) {
            delete (i.second);
        }
        m_dreEvent.Cancel();
        m_agingEvent.Cancel();
    }

    void NoshareRouting::DreEvent() {
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
        m_dreEvent = Simulator::Schedule(m_dreTime, &NoshareRouting::DreEvent, this);
    }

    void NoshareRouting::AgingEvent() {
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

        m_agingEvent = Simulator::Schedule(m_agingTime, &NoshareRouting::AgingEvent, this);
    }

    void NoshareRouting::printBestPathCETable() {
        for (const auto& entry : best_pathCE_Table) {
            const auto& key = entry.first;
            const auto& info = entry.second;
            uint32_t id = Settings::hostIp2IdMap[key];
            std::cout << "dst_ip: " << id << "\n";
            printBestPathCETable_Entry(key);
        }
    }
    void NoshareRouting::printPathChoiceTable() {
        for (const auto& entry : PathChoiceTable) {
            const auto& key = entry.first;
            const auto& info = entry.second;
            uint32_t id = Settings::hostIp2IdMap[key];
            std::cout << "dst_id: " << id << "\n";
            printPathChoiceTable_Entry(key);
        }
    }
    void NoshareRouting::printPathChoiceFlagMap() {
        for (const auto& entry : PathChoiceFlagMap) {
            const auto& destination = entry.first;
            const auto& pathIndex = entry.second;
            uint32_t id = Settings::hostIp2IdMap[destination];
            std::cout << "Destination: " << id << "\n";
            printPathChoiceFlagMap_Entry(destination);
        }
    }
    void NoshareRouting::printBestPathCETable_Entry(uint32_t dip) {
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
    void NoshareRouting::printAcceptablePathTable(){
        for (const auto& entry : acceptable_path_table) {
            const auto& key = entry.first;
            const auto& info = entry.second;
            uint32_t id = Settings::hostIp2IdMap[key];
            std::cout << "dst_ip: " << id << "\n";
            printAcceptablePathTable_Entry(key);
        }
    }
    void NoshareRouting::printAcceptablePathTable_Entry(uint32_t dip){
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
    void NoshareRouting::printPathChoiceTable_Entry(uint32_t dip) {
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
    void NoshareRouting::showPathChoiceInfo(PathChoiceInfo pc){
        std::cout << "  Path: ";
        for (const auto& node : pc._path) {
            std::cout << static_cast<int>(node) << " ";
        }
        std::cout << "\n  Update Time: " << pc._updateTime.GetMicroSeconds() << "\n";
        std::cout << "\n  Is Used: " << (pc._is_used ? "true" : "false") << "\n";
    }
    void NoshareRouting::printPathChoiceFlagMap_Entry(uint32_t dip){
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
    
    void NoshareRouting::showCaverAck_info(CaverAckTag ackTag, CustomHeader ch){
        uint32_t ack_src_id = Settings::hostIp2IdMap[ch.sip];
        uint32_t ack_dst_id = Settings::hostIp2IdMap[ch.dip];
        uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.dip], Settings::hostIp2IdMap[ch.sip], ch.udp.dport, ch.udp.sport)];
        // ACK固有的信息
        printf("ACK of flow id: %d, Ack from host %d to host %d\n", flowid, ack_src_id, ack_dst_id);
        // ACK携带的信息
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

    void NoshareRouting::showAck_info(CustomHeader ch){
        uint32_t ack_src_id = Settings::hostIp2IdMap[ch.sip];
        uint32_t ack_dst_id = Settings::hostIp2IdMap[ch.dip];
        uint32_t flowid = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.dip], Settings::hostIp2IdMap[ch.sip], ch.udp.dport, ch.udp.sport)];
        // ACK固有的信息
        printf("ACK of flow id: %d, Ack from host %d to host %d\n", flowid, ack_src_id, ack_dst_id);
    }

    void NoshareRouting::showPortCE(uint32_t port){
        uint32_t ce = m_DreMap[port];
        uint32_t localce = QuantizingX(port, ce);
        std::cout << "Port: " << port << ", CE: " << ce << ",localCE: " << localce << std::endl;
    }

    void NoshareRouting::showRouteChoice(CaverRouteChoice rc){
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
    void NoshareRouting::showCaverUdpinfo(CaverUdpTag udpTag){
        std::cout << "SrcRoute: " << (udpTag.GetSrcRouteEnable() == 1 ? "true" : "false") << "\n";
        std::cout << "hopCount: " << udpTag.GetHopCount() << "\n";
        std::cout << "pathId: " ;
        std::vector<uint8_t> showpath = uint32_to_uint8(udpTag.GetPathId());
        for (int i = 0; i < 4; i++) {
            std::cout << static_cast<int>(showpath[i]) << "->";
        }
        std::cout << std::endl;
    }
    void NoshareRouting::showDreTable(){
        for (auto it = m_DreMap.begin(); it != m_DreMap.end(); ++it) {
            uint32_t ce = it->second;
            uint32_t localce = QuantizingX(it->first, ce);
            std::cout << "Port: " << it->first << ", CE: " << it->second << ",localCE: " << localce << std::endl;
        }
    }
    void NoshareRouting::showglobalDreTable(){
        for (auto it = m_DreMap.begin(); it != m_DreMap.end(); ++it) {
            uint32_t outPort = it->first;
            uint32_t neighbor_id = Settings::m_nodeInterfaceMap[m_switch_id][outPort];
            uint32_t X = Settings::global_dre_map[{m_switch_id, neighbor_id}];
            uint32_t localce = QuantizingX(outPort, X);
            std::cout << "Port: " << outPort << ",global_localCE: " << localce << ",global_CE_store" << Settings::global_CE_map[{m_switch_id, neighbor_id}]<< std::endl;
        }
    }
    void NoshareRouting::showPathVec(std::vector<uint8_t> path){
        for (int i = 0; i < path.size(); i++) {
            std::cout << static_cast<int>(path[i]) << "->";
        }
        std::cout << std::endl;
    }

    std::vector<uint32_t> NoshareRouting::getPathNodeIds(const std::vector<uint8_t>& pathVec, uint32_t currentNodeId) {
        std::vector<uint32_t> nodePath;
        uint32_t currentNode = currentNodeId;

        // 将当前节点加入路径
        nodePath.push_back(currentNode);

        for (uint8_t outPortId : pathVec) {
            // 检查当前节点是否存在于 m_nodeInterfaceMap 中
            assert(Settings::m_nodeInterfaceMap.find(currentNode) != Settings::m_nodeInterfaceMap.end() &&
               "Current node ID not found in m_nodeInterfaceMap.");

            // 获取当前节点的接口映射
            const auto& interfaceMap = Settings::m_nodeInterfaceMap[currentNode];

            // 检查给定的 outPortId 是否存在于接口映射中
            if (interfaceMap.find(outPortId) == interfaceMap.end()) {
                throw std::runtime_error("Out port ID not found in interface map for current node.");
            }

            // 获取下一个节点 ID
            uint32_t nextNodeId = interfaceMap.at(outPortId);

            // 将下一个节点加入路径
            nodePath.push_back(nextNodeId);

            // 更新当前节点为下一个节点
            currentNode = nextNodeId;
        }

        return nodePath;
    }
    void NoshareRouting::showOptimalvsCaver(CustomHeader ch, CaverRouteChoice m_choice){
        if(Caver_debug){
            // 显示一下全局的dre值与本地的dre值的区别
            // 显示m_DreMap的值
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
        std::cout << ", Optimal CE: " << optimalCE << std::endl;
        if (m_choice.SrcRoute){
            std::vector<uint32_t> caverPath = getPathNodeIds(m_choice.pathVec, m_switch_id);
            uint32_t localCE = QuantizingX(m_choice.outPort, m_DreMap[m_choice.outPort]);
            uint32_t caverCE = std::max(localCE, m_choice.remoteCE);
            std::cout << "Caver Path: ";
            for (int i = 0; i < caverPath.size(); i++) {
                std::cout << caverPath[i] << "->";
            }
            std::cout << "Caver outport: " << m_choice.outPort << std::endl;
            std::cout << "Caver recorded CE: " << caverCE << std::endl;
            std:: cout << "Caver's path real CE: ";//所选路径对应的实际的gloable CE值
            caverPath.push_back(Settings::hostIp2IdMap[ch.dip]);//添加上目的节点；
            uint32_t caverRealCE = Settings::calculatePathCE(caverPath);
            std::cout << caverRealCE << std::endl;
        }
        else{
            std::cout << "Caver Path: ECMP" << std::endl;
        }
    }
    int NoshareRouting::getRandomElement(const std::list<int>& myList) {
        // 检查列表是否为空
        if (myList.empty()) {
            throw std::runtime_error("List is empty");
        }

        // 使用随机数生成器
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, myList.size() - 1);

        // 生成一个随机索引
        int randomIndex = dis(gen);

        // 迭代到随机索引位置
        auto it = myList.begin();
        std::advance(it, randomIndex);

        return *it;
    }

}

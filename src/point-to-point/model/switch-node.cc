#include "switch-node.h"

#include "assert.h"
#include "ns3/boolean.h"
#include "ns3/conweave-routing.h"
#include "ns3/double.h"
#include "ns3/flow-id-tag.h"
#include "ns3/int-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4.h"
#include "ns3/letflow-routing.h"
#include "ns3/packet.h"
#include "ns3/pause-header.h"
#include "ns3/settings.h"
#include "ns3/uinteger.h"
#include "ppp-header.h"
#include "qbb-net-device.h"
#include <arpa/inet.h> // 包含 inet_addr 和 inet_ntoa 函数
#include <cstdint> // 包含 uint32_t
#include <random>

namespace ns3 {

TypeId SwitchNode::GetTypeId(void) {
    static TypeId tid =
        TypeId("ns3::SwitchNode")
            .SetParent<Node>()
            .AddConstructor<SwitchNode>()
            .AddAttribute("EcnEnabled", "Enable ECN marking.", BooleanValue(false),
                          MakeBooleanAccessor(&SwitchNode::m_ecnEnabled), MakeBooleanChecker())
            .AddAttribute("CcMode", "CC mode.", UintegerValue(0),
                          MakeUintegerAccessor(&SwitchNode::m_ccMode),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("AckHighPrio", "Set high priority for ACK/NACK or not", UintegerValue(0),
                          MakeUintegerAccessor(&SwitchNode::m_ackHighPrio),
                          MakeUintegerChecker<uint32_t>());
    return tid;
}

SwitchNode::SwitchNode() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, std::numeric_limits<uint32_t>::max());

    // 生成一个随机整数
    uint32_t random_integer = dis(gen);

    m_ecmpSeed = random_integer ^ m_id;
    // m_ecmpSeed = m_id;
    m_isToR = false;
    m_node_type = 1;
    m_isToR = false;
    m_drill_candidate = 2;
    m_mmu = CreateObject<SwitchMmu>();
    
    // //DV's Callback for switch functions
    // m_mmu->m_dvRouting.SetSwitchSendCallback(MakeCallback(&SwitchNode::DoSwitchSend, this));
    // m_mmu->m_dvRouting.SetSwitchSendToDevCallback(
    //     MakeCallback(&SwitchNode::SendToDevContinue, this));


    // Conga's Callback for switch functions
    m_mmu->m_congaRouting.SetSwitchSendCallback(MakeCallback(&SwitchNode::DoSwitchSend, this));
    m_mmu->m_congaRouting.SetSwitchSendToDevCallback(
        MakeCallback(&SwitchNode::SendToDevContinue, this));
    // ConWeave's Callback for switch functions
    m_mmu->m_conweaveRouting.SetSwitchSendCallback(MakeCallback(&SwitchNode::DoSwitchSend, this));
    m_mmu->m_conweaveRouting.SetSwitchSendToDevCallback(
        MakeCallback(&SwitchNode::SendToDevContinue, this));
    // Hula's Callback for switch function
    m_mmu->m_hulaRouting.SetSwitchSendCallback(MakeCallback(&SwitchNode::DoSwitchSend, this));
    m_mmu->m_hulaRouting.SetSwitchSendToDevCallback(
        MakeCallback(&SwitchNode::SendToDevContinue, this));
    m_mmu->m_hulaRouting.SetSwitchSendHulaProbeCallback(MakeCallback(&SwitchNode::SendHulaProbe, this));

    //DV's Callback for switch functions
    m_mmu->m_dvRouting.SetSwitchSendCallback(MakeCallback(&SwitchNode::DoSwitchSend, this));
    m_mmu->m_dvRouting.SetSwitchSendToDevCallback(
        MakeCallback(&SwitchNode::SendToDevContinue, this));
    //Caver's Callback for switch functions
    m_mmu->m_caverRouting.SetSwitchSendCallback(MakeCallback(&SwitchNode::DoSwitchSend, this));
    m_mmu->m_caverRouting.SetSwitchSendToDevCallback(
        MakeCallback(&SwitchNode::SendToDevContinue, this));
        //Noshare's Callback for switch functions
    m_mmu->m_noshareRouting.SetSwitchSendCallback(MakeCallback(&SwitchNode::DoSwitchSend, this));
    m_mmu->m_noshareRouting.SetSwitchSendToDevCallback(
        MakeCallback(&SwitchNode::SendToDevContinue, this));
    // pCnt:端口数量
    for (uint32_t i = 0; i < pCnt; i++) {
        m_txBytes[i] = 0;
    }
    for (uint32_t i = 0; i < pCnt; i++) {
        m_rxBytes[i] = 0;
    }
}

/**
 * @brief Load Balancing
 */
uint32_t SwitchNode::DoLbFlowECMP(Ptr<const Packet> p, const CustomHeader &ch,
                                  const std::vector<int> &nexthops) {
    // pick one next hop based on hash
    union {
        uint8_t u8[4 + 4 + 2 + 2];
        uint32_t u32[3];
    } buf;
    buf.u32[0] = ch.sip;
    buf.u32[1] = ch.dip;
    if (ch.l3Prot == 0x6)
        buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
    else if (ch.l3Prot == 0x11)  // XXX RDMA traffic on UDP
        buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
    else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)  // ACK or NACK
        buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
    else {
        std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                  << "Cannot support other protoocls than TCP/UDP (l3Prot:" << ch.l3Prot << ")"
                  << std::endl;
        assert(false && "Cannot support other protoocls than TCP/UDP");
    }

    uint32_t hashVal = EcmpHash(buf.u8, 12, m_ecmpSeed);
    uint32_t idx = hashVal % nexthops.size();
    return nexthops[idx];
}

/*-----------------CONGA-----------------*/
uint32_t SwitchNode::DoLbConga(Ptr<Packet> p, CustomHeader &ch, const std::vector<int> &nexthops) {
    return DoLbFlowECMP(p, ch, nexthops);  // flow ECMP (dummy)
}
/*-----------------DV-----------------*/
uint32_t SwitchNode::DoLbDV(Ptr<Packet> p, CustomHeader &ch, const std::vector<int> &nexthops) {
    return DoLbFlowECMP(p, ch, nexthops);  // flow ECMP (dummy)
}
/*-----------------Caver-----------------*/
uint32_t SwitchNode::DoLbCaver(Ptr<Packet> p, CustomHeader &ch, const std::vector<int> &nexthops) {
    return DoLbFlowECMP(p, ch, nexthops);  // flow ECMP (dummy)
}
/*-----------------Noshare-----------------*/
uint32_t SwitchNode::DoLbNoshare(Ptr<Packet> p, CustomHeader &ch, const std::vector<int> &nexthops) {
    return DoLbFlowECMP(p, ch, nexthops);  // flow ECMP (dummy)
}
/*-----------------Letflow-----------------*/
uint32_t SwitchNode::DoLbLetflow(Ptr<Packet> p, CustomHeader &ch,
                                 const std::vector<int> &nexthops) {
    if (m_isToR && nexthops.size() == 1) {
        if (m_isToR_hostIP.find(ch.sip) != m_isToR_hostIP.end() &&
            m_isToR_hostIP.find(ch.dip) != m_isToR_hostIP.end()) {
            return nexthops[0];  // intra-pod traffic
        }
    }

    /* ONLY called for inter-Pod traffic */
    uint32_t outPort = m_mmu->m_letflowRouting.RouteInput(p, ch);
    if (outPort == LETFLOW_NULL) {
        assert(nexthops.size() == 1);  // Receiver's TOR has only one interface to receiver-server
        outPort = nexthops[0];         // has only one option
    }
    assert(std::find(nexthops.begin(), nexthops.end(), outPort) !=
           nexthops.end());  // Result of Letflow cannot be found in nexthops
    return outPort;
}

/*-----------------DRILL-----------------*/
uint32_t SwitchNode::CalculateInterfaceLoad(uint32_t interface) {
    Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[interface]);
    NS_ASSERT_MSG(!!device && !!device->GetQueue(),
                  "Error of getting a egress queue for calculating interface load");
    return device->GetQueue()->GetNBytesTotal();  // also used in HPCC
}

uint32_t SwitchNode::DoLbDrill(Ptr<const Packet> p, const CustomHeader &ch,
                               const std::vector<int> &nexthops) {
    // find the Egress (output) link with the smallest local Egress Queue length
    uint32_t leastLoadInterface = 0;
    uint32_t leastLoad = std::numeric_limits<uint32_t>::max();
    auto rand_nexthops = nexthops;
    std::random_shuffle(rand_nexthops.begin(), rand_nexthops.end());

    std::map<uint32_t, uint32_t>::iterator itr = m_previousBestInterfaceMap.find(ch.dip);
    if (itr != m_previousBestInterfaceMap.end()) {
        leastLoadInterface = itr->second;
        leastLoad = CalculateInterfaceLoad(itr->second);
    }

    uint32_t sampleNum =
        m_drill_candidate < rand_nexthops.size() ? m_drill_candidate : rand_nexthops.size();
    for (uint32_t samplePort = 0; samplePort < sampleNum; samplePort++) {
        uint32_t sampleLoad = CalculateInterfaceLoad(rand_nexthops[samplePort]);
        if (sampleLoad < leastLoad) {
            leastLoad = sampleLoad;
            leastLoadInterface = rand_nexthops[samplePort];
        }
    }
    m_previousBestInterfaceMap[ch.dip] = leastLoadInterface;
    return leastLoadInterface;
}

/*------------------ConWeave Dummy ----------------*/
uint32_t SwitchNode::DoLbConWeave(Ptr<const Packet> p, const CustomHeader &ch,
                                  const std::vector<int> &nexthops) {
    return DoLbFlowECMP(p, ch, nexthops);  // flow ECMP (dummy)
}
/*----------------------------------*/

/*------------------Hula Dummy ----------------*/
uint32_t SwitchNode::DoLbHula(Ptr<Packet> p, CustomHeader &ch, const std::vector<int> &nexthops) {
    return DoLbFlowECMP(p, ch, nexthops);  // flow ECMP (dummy)
}
/*----------------------------------*/

void SwitchNode::CheckAndSendPfc(uint32_t inDev, uint32_t qIndex) {
    Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
    bool pClasses[qCnt] = {0};
    m_mmu->GetPauseClasses(inDev, qIndex, pClasses);
    for (int j = 0; j < qCnt; j++) {
        if (pClasses[j]) {
            uint32_t paused_time = device->SendPfc(j, 0);
            m_mmu->SetPause(inDev, j, paused_time);
            //std::cout << "PFC event: " << std::endl; 
            m_mmu->m_pause_remote[inDev][j] = true;
            /** PAUSE SEND COUNT ++ */
        }
    }

    for (int j = 0; j < qCnt; j++) {
        if (!m_mmu->m_pause_remote[inDev][j]) continue;

        if (m_mmu->GetResumeClasses(inDev, j)) {
            device->SendPfc(j, 1);
            //std::cout << "PFC event: " << std::endl; 
            m_mmu->SetResume(inDev, j);
            m_mmu->m_pause_remote[inDev][j] = false;
        }
    }
}

void SwitchNode::SendHulaProbe(uint32_t dev, uint32_t torID, uint8_t minUtil) {
    Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[dev]);
    device->SendHulaProbe(torID, minUtil);
}

void SwitchNode::CheckAndSendResume(uint32_t inDev, uint32_t qIndex) {
    Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
    if (m_mmu->GetResumeClasses(inDev, qIndex)) {
        device->SendPfc(qIndex, 1);
        //std::cout << "PFC Resumeevent: " << std::endl;
        m_mmu->SetResume(inDev, qIndex);
    }
}

/********************************************
 *              MAIN LOGICS                 *
 *******************************************/


std::string convertToSpaceSeparatedString(uint32_t id, CustomHeader &ch) {
    std::ostringstream oss;
    oss << std::to_string(id) << " " << Settings::hostIp2IdMap[ch.sip] << " " << Settings::hostIp2IdMap[ch.dip] << " " << ch.udp.sport << " " << ch.udp.dport;
    return oss.str();
}
// This function can only be called in switch mode
bool SwitchNode::SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet,
                                         CustomHeader &ch) {
    if (ch.l3Prot == 0xFB && Settings::lb_mode == 12) {
        m_mmu->m_hulaRouting.processProbe(device->GetIfIndex(), packet, ch);
        return true;
    }                                            
    //TODO: my code to  visualize the packet header
    if (ch.l3Prot == 0x11)  // XXX RDMA traffic on UDP
    {
        // std::string head = convertToSpaceSeparatedString(m_id, ch);
        // if (headSet.find(head) == headSet.end()) {
        //     headSet.insert(head);
        //     std::cout << "flow passed:" <<head << std::endl;
        // }
        uint32_t flow_id = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
        // std::cout << "Flow ID: " << flow_id << std::endl;
        if (flow_bytes.find(flow_id) == flow_bytes.end()) {
            //std::cout << "flow_passed: " << "switch_id "  <<  m_id << " flow_id "<< flow_id << std::endl;
        }
        flow_bytes[flow_id] += packet->GetSize();
        if(Settings::motivation_pathCE){
            if (m_isToR){
                if (std::find(Settings::TorSwitch_nodelist[m_id].begin(), Settings::TorSwitch_nodelist[m_id].end(), ch.sip) != Settings::TorSwitch_nodelist[m_id].end()){
                    if (std::find(Settings::TorSwitch_nodelist[m_id].begin(), Settings::TorSwitch_nodelist[m_id].end(), ch.dip) == Settings::TorSwitch_nodelist[m_id].end()) {
                        uint64_t qp_key = GetQpKey(ch.dip, ch.udp.sport, ch.udp.dport, ch.udp.pg);
                        if (easy_flowtable.find(qp_key) == easy_flowtable.end()){
                            easy_flowtable.insert(qp_key);
                            std::cout << "motivation pathCE new flow info: " << flow_id << std::endl;
                            Settings::savePathCEs(m_id, Settings::hostIp2IdMap[ch.dip]);
                        }
                    }
                } 
            }
        }
    }
    uint32_t ifIndex = device->GetIfIndex();
        //在这里显示与PFC（ingress相关的内容）
        // 
    // if (m_mmu->GetusedIngressPortBytes(ifIndex)!= 0)
        // {
        // std::cout<< "Ingress buffer" << ",Sw:" << m_id << "," 
        //             << "ingress port:" << ifIndex
        //             << ",m_usedIngressPortBytes:" << m_mmu->GetusedIngressPortBytes(ifIndex)
        //             << ",m_port_max_shared_cell:" << m_mmu->Getport_max_shared_cell()
        //             << ",At " << Simulator::Now() << std::endl;
        // }
    m_rxBytes[ifIndex] += packet->GetSize();
    SendToDev(packet, ch);
    return true;
}

void SwitchNode::SendToDev(Ptr<Packet> p, CustomHeader &ch) {
    /** HIJACK: hijack the packet and run DoSwitchSend internally for Conga and ConWeave.
     * Note that DoLbConWeave() and DoLbConga() are flow-ECMP function for control packets
     * or intra-ToR traffic.
     */
    if (!m_GlobaldreEvent.IsRunning()){
        m_GlobaldreEvent = Simulator::Schedule(Settings::Dre_time_map[GetId()], &SwitchNode::GlobalDreEvent, this);
    }
    // Conga
    if (Settings::lb_mode == 3) {
        m_mmu->m_congaRouting.RouteInput(p, ch);
        return;
    }

    // ConWeave
    if (Settings::lb_mode == 9) {
        m_mmu->m_conweaveRouting.RouteInput(p, ch);
        return;
    }

    if (Settings::lb_mode == 10) {
        m_mmu->m_dvRouting.RouteInput(p, ch);
        return;
    }

    if(Settings::lb_mode == 20){
        m_mmu->m_caverRouting.RouteInput(p, ch);
        return;
    }
    //hula
    if (Settings::lb_mode == 12) {
        m_mmu->m_hulaRouting.RouteInput(p, ch);
        return;
    }
    if(Settings::lb_mode == 21){
        m_mmu->m_noshareRouting.RouteInput(p, ch);
        return;
    }

    // Others
    SendToDevContinue(p, ch);
}

void SwitchNode::SendToDevContinue(Ptr<Packet> p, CustomHeader &ch) {
    int idx;
    if (Settings::set_fixed_routing && ch.l3Prot == 0x11){
        //udp的数据包
        idx = GetStaticRoute(p, ch);
    }
    else{
        idx = GetOutDev(p, ch);
    }
    if (idx >= 0) {
        NS_ASSERT_MSG(m_devices[idx]->IsLinkUp(),
                      "The routing table look up should return link that is up");
        // 如果时采用DV的方法，且是UdP包的话，则应该更新一下Dre，这部分是采用ECMP或者直接发送的方法
        if (Settings::lb_mode == 10 and ch.l3Prot == 0x11) {
            m_mmu->m_dvRouting.UpdateLocalDre(p, ch, idx);
            if (m_mmu->m_dvRouting.DreTable_log){
                if (m_isToR)
                    printf("Dre Table: ToR switch %d\n", m_mmu->m_dvRouting.m_switch_id);
                else
                    printf("Dre Table: Mid switch %d\n", m_mmu->m_dvRouting.m_switch_id);
                for (auto it = m_mmu->m_dvRouting.m_DreMap.begin(); it != m_mmu->m_dvRouting.m_DreMap.end(); ++it) {
                    uint32_t ce = it->second;
                    uint32_t localce = m_mmu->m_dvRouting.QuantizingX(it->first, ce);
                    std::cout << "Port: " << it->first << ", CE: " << it->second << ",localCE: " << localce << std::endl;
                }
            }
        }
        // 如果是采用Caver的方法，且是UdP包的话，则应该更新一下Dre
        if (Settings::lb_mode == 20 and ch.l3Prot == 0x11) {
            m_mmu->m_caverRouting.UpdateLocalDre(p, ch, idx);
            if (m_mmu->m_caverRouting.DreTable_log){
                if (m_isToR)
                    printf("Dre Table: ToR switch %d\n", m_mmu->m_caverRouting.m_switch_id);
                else
                    printf("Dre Table: Mid switch %d\n", m_mmu->m_caverRouting.m_switch_id);
                for (auto it = m_mmu->m_caverRouting.m_DreMap.begin(); it != m_mmu->m_caverRouting.m_DreMap.end(); ++it) {
                    uint32_t ce = it->second;
                    uint32_t localce = m_mmu->m_caverRouting.QuantizingX(it->first, ce);
                    std::cout << "Port: " << it->first << ", CE: " << it->second << ",localCE: " << localce << std::endl;
                }
            }
        }
        if (Settings::lb_mode == 21 and ch.l3Prot == 0x11) {
            m_mmu->m_noshareRouting.UpdateLocalDre(p, ch, idx);
            if (m_mmu->m_noshareRouting.DreTable_log){
                if (m_isToR)
                    printf("Dre Table: ToR switch %d\n", m_mmu->m_noshareRouting.m_switch_id);
                else
                    printf("Dre Table: Mid switch %d\n", m_mmu->m_noshareRouting.m_switch_id);
                for (auto it = m_mmu->m_noshareRouting.m_DreMap.begin(); it != m_mmu->m_noshareRouting.m_DreMap.end(); ++it) {
                    uint32_t ce = it->second;
                    uint32_t localce = m_mmu->m_noshareRouting.QuantizingX(it->first, ce);
                    std::cout << "Port: " << it->first << ", CE: " << it->second << ",localCE: " << localce << std::endl;
                }
            } 
        }
        // determine the qIndex
        uint32_t qIndex;
        if (ch.l3Prot == 0xFF || ch.l3Prot == 0xFE ||
            (m_ackHighPrio &&
             (ch.l3Prot == 0xFD ||
              ch.l3Prot == 0xFC))) {  // QCN or PFC or ACK/NACK, go highest priority
            qIndex = 0;               // high priority
        } else {
            qIndex = (ch.l3Prot == 0x06 ? 1 : ch.udp.pg);  // if TCP, put to queue 1. Otherwise, it
                                                           // would be 3 (refer to trafficgen)
        }
        // 在这个位置输出egress缓存相关的内容
        // if (m_mmu->GetusedEgressQSharedBytes(idx, qIndex) != 0)
        // {
        // std::cout<< "Egress buffer" << ",Sw:" << m_id << "," 
        //             << "egress port:" << idx 
        //             << ",m_usedEgressQSharedBytes:" << m_mmu->GetusedEgressQSharedBytes(idx, qIndex) 
        //             << ",kmin:" << m_mmu->kmin[idx]
        //             << ",kmax:" << m_mmu->kmax[idx]
        //             << ",pmax:" << m_mmu->pmax[idx]
        //             << ",m_op_uc_port_config1_cell:" << m_mmu->Getop_uc_port_config1_cell()
        //             << ",At " << Simulator::Now() << std::endl;
        // }
        DoSwitchSend(p, ch, idx, qIndex);  // m_devices[idx]->SwitchSend(qIndex, p, ch);
        return;
    }
    std::cout << "WARNING - Drop occurs in SendToDevContinue()" << std::endl;
    return;  // Drop otherwise
}

/**
 * @brief Retrieves the static route for a given packet.
 *
 * This function determines the next hop for a packet based on its flow ID and
 * the static paths defined in the network settings. It uses the source and 
 * destination IP addresses, as well as the UDP source and destination ports, 
 * to identify the flow ID and find the corresponding path.
 *
 * @param p Pointer to the packet for which the route is being determined.
 * @param ch Custom header containing the source and destination IP addresses 
 *           and UDP ports.
 * @return The interface index for the next hop.
 */
int SwitchNode::GetStaticRoute(Ptr<Packet> p, CustomHeader &ch){
    uint32_t flow_id = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
    const auto &path = Settings::static_paths[flow_id];
    uint32_t current_id = GetId();
    auto it = std::find(path.begin(), path.end(), current_id);
    uint32_t next_hop_id;

    if (it != path.end() && std::next(it) != path.end()) {
        next_hop_id = *std::next(it);
    } else {
        next_hop_id = Settings::hostIp2IdMap[ch.dip];
    }

    return Settings::m_nbr2if[current_id][next_hop_id];

}

int SwitchNode::GetOutDev(Ptr<Packet> p, CustomHeader &ch) {
    // look up entries
    auto entry = m_rtTable.find(ch.dip);

    // no matching entry
    if (entry == m_rtTable.end()) {
        std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                  << "No matching entry, so drop this packet at SwitchNode (l3Prot:" << ch.l3Prot
                  << ")" << std::endl;
        assert(false);
    }

    // entry found
    const auto &nexthops = entry->second;
    bool control_pkt =
        (ch.l3Prot == 0xFF || ch.l3Prot == 0xFE || ch.l3Prot == 0xFD || ch.l3Prot == 0xFC);

    if (Settings::lb_mode == 0 || control_pkt) {  // control packet (ACK, NACK, PFC, QCN)
        return DoLbFlowECMP(p, ch, nexthops);     // ECMP routing path decision (4-tuple)
    }

    switch (Settings::lb_mode) {
        case 2:
            return DoLbDrill(p, ch, nexthops);
        case 3:
            return DoLbConga(p, ch, nexthops); /** DUMMY: Do ECMP */
        case 6:
            return DoLbLetflow(p, ch, nexthops);
        case 9:
            return DoLbConWeave(p, ch, nexthops); /** DUMMY: Do ECMP */
        case 10:
            return DoLbDV(p, ch, nexthops); /** DUMMY: Do ECMP */
        case 20:
            return DoLbCaver(p, ch, nexthops); /** DUMMY: Do ECMP */
        case 21:
            return DoLbNoshare(p, ch, nexthops); /** DUMMY: Do ECMP */
        case 12:
            return DoLbHula(p, ch, nexthops);
        default:
            std::cout << "Unknown lb_mode(" << Settings::lb_mode << ")" << std::endl;
            assert(false);
    }
}

/*
 * The (possible) callback point when conweave dequeues packets from buffer
 */
void SwitchNode::DoSwitchSend(Ptr<Packet> p, CustomHeader &ch, uint32_t outDev, uint32_t qIndex) {
    // admission control
    FlowIdTag t;
    p->PeekPacketTag(t);
    uint32_t inDev = t.GetFlowId();

    /** NOTE:
     * ConWeave control packets have the high priority as ACK/NACK/PFC/etc with qIndex = 0.
     */
    if (inDev == Settings::CONWEAVE_CTRL_DUMMY_INDEV) { // sanity check
        // ConWeave reply is on ACK protocol with high priority, so qIndex should be 0
        assert(qIndex == 0 && m_ackHighPrio == 1 && "ConWeave's reply packet follows ACK, so its qIndex should be 0");
    }

    if (Settings::lb_mode == 12) {
        m_mmu->m_hulaRouting.updateLink(outDev, p->GetSize());
    }
    
    if (qIndex != 0) {  // not highest priority
        if (m_mmu->CheckEgressAdmission(outDev, qIndex,
                                        p->GetSize())) {  // Egress Admission control
            if (m_mmu->CheckIngressAdmission(inDev, qIndex,
                                             p->GetSize())) {  // Ingress Admission control
                m_mmu->UpdateIngressAdmission(inDev, qIndex, p->GetSize());
                m_mmu->UpdateEgressAdmission(outDev, qIndex, p->GetSize());
            } else { /** DROP: At Ingress */
#if (0)
                /** NOTE: logging dropped pkts */
                std::cout << "LostPkt ingress - Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                          << "L3Prot:" << ch.l3Prot
                          << ",Size:" << p->GetSize()
                          << ",At " << Simulator::Now() << std::endl;
#endif
                if (ch.l3Prot == 0x11) {
                    printf("An UDP packet dropped because ingress admission check false: Node:%u, Flow:%u, Seq=%u\n", 
                        m_mmu->m_noshareRouting.m_switch_id, 
                        Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)],
                        ch.udp.seq);
                }
                Settings::dropped_pkt_sw_ingress++;
                return;  // drop
            }
        } else { /** DROP: At Egress */
#if (0)
            /** NOTE: logging dropped pkts */
            std::cout << "LostPkt egress - Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                      << "L3Prot:" << ch.l3Prot << ",Size:" << p->GetSize() << ",At "
                      << Simulator::Now() << std::endl;
#endif                
            if (ch.l3Prot == 0x11) {
                printf("An UDP packet dropped because egress admission check false: Node:%u, Flow:%u, Seq=%u\n", 
                    this->m_id,
                    Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)],
                    ch.udp.seq);
            }
            Settings::dropped_pkt_sw_egress++;
            return;  // drop
        }
        //此时已经判断完了是否要DROP了
        CheckAndSendPfc(inDev, qIndex);
    }
    if(Dive_optimal_log){
        UpdateGlobalDre(p, outDev);
    }
    Settings::record_flow_distribution(p, ch, this, outDev);
    m_devices[outDev]->SwitchSend(qIndex, p, ch);
}

void SwitchNode::UpdateGlobalDre(Ptr<Packet> p, uint32_t outPort){
    uint32_t neighbor_id = Settings::m_nodeInterfaceMap[GetId()][outPort];
    uint32_t X = Settings::global_dre_map[{GetId(), neighbor_id}];
    uint32_t newX = X + p->GetSize();
    Settings::global_dre_map[{GetId(), neighbor_id}] = newX;
}

void SwitchNode::SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p) {
    FlowIdTag t;
    p->PeekPacketTag(t);
    if (qIndex != 0) {
        uint32_t inDev = t.GetFlowId();
        if (inDev != Settings::CONWEAVE_CTRL_DUMMY_INDEV) {
            // NOTE: ConWeave's probe/reply does not need to pass inDev interface,
            // so skip for conweave's queued packets
            m_mmu->RemoveFromIngressAdmission(inDev, qIndex, p->GetSize());
        }
        m_mmu->RemoveFromEgressAdmission(ifIndex, qIndex, p->GetSize());
        if (m_ecnEnabled) {
            bool egressCongested = m_mmu->ShouldSendCN(ifIndex, qIndex);
            if (egressCongested) {
                PppHeader ppp;
                Ipv4Header h;
                p->RemoveHeader(ppp);
                p->RemoveHeader(h);
                h.SetEcn((Ipv4Header::EcnType)0x03);
                p->AddHeader(h);
                p->AddHeader(ppp);
            }
        }
        // NOTE: ConWeave's probe/reply does not need to pass inDev interface
        if (inDev != Settings::CONWEAVE_CTRL_DUMMY_INDEV) {
            CheckAndSendResume(inDev, qIndex);
        }
    }

    // HPCC's INT
    if (1) {
        uint8_t *buf = p->GetBuffer();
        if (buf[PppHeader::GetStaticSize() + 9] == 0x11) {  // udp packet
            IntHeader *ih = (IntHeader *)&buf[PppHeader::GetStaticSize() + 20 + 8 +
                                              6];  // ppp, ip, udp, SeqTs, INT
            Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(m_devices[ifIndex]);
            if (m_ccMode == 3) {  // HPCC
                ih->PushHop(Simulator::Now().GetTimeStep(), m_txBytes[ifIndex],
                            dev->GetQueue()->GetNBytesTotal(), dev->GetDataRate().GetBitRate());
            }
        }
    }
    m_txBytes[ifIndex] += p->GetSize();
}

uint32_t SwitchNode::EcmpHash(const uint8_t *key, size_t len, uint32_t seed) {
    uint32_t h = seed;
    if (len > 3) {
        const uint32_t *key_x4 = (const uint32_t *)key;
        size_t i = len >> 2;
        do {
            uint32_t k = *key_x4++;
            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
            h = (h << 13) | (h >> 19);
            h += (h << 2) + 0xe6546b64;
        } while (--i);
        key = (const uint8_t *)key_x4;
    }
    if (len & 3) {
        size_t i = len & 3;
        uint32_t k = 0;
        key = &key[i - 1];
        do {
            k <<= 8;
            k |= *key--;
        } while (--i);
        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        h ^= k;
    }
    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

void SwitchNode::SetEcmpSeed(uint32_t seed) { m_ecmpSeed = seed; }

void SwitchNode::AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx) {
    uint32_t dip = dstAddr.Get();
    m_rtTable[dip].push_back(intf_idx);
}
void SwitchNode::AddDVTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx, Time now) {
    uint32_t dip = dstAddr.Get();
    auto dstIter = m_mmu->m_dvRouting.m_DVTable.find(dip);
    if (dstIter == m_mmu->m_dvRouting.m_DVTable.end()) {
        // 如果不存在，则创建一个新的条目
        m_mmu->m_dvRouting.m_DVTable[dip] = std::map<uint32_t, DVInfo>();
    }
    DVInfo dvInfo;
    dvInfo._ce = 0;
    dvInfo._updateTime = now;
    dvInfo._valid = false;
    m_mmu->m_dvRouting.m_DVTable[dip][intf_idx] = dvInfo;
    
}
// *******************************Add begin**********************//
void SwitchNode::AddPathCETableEntry(Ipv4Address &dstAddr, Time now){
    std::cout << dstAddr;
    uint32_t dip = dstAddr.Get();
    auto dstIter = m_mmu->m_dvRouting.PathCE_Table.find(dip);
    if (dstIter == m_mmu->m_dvRouting.PathCE_Table.end()) {
        // 如果不存在，则创建一个新的条目
        singleDVInfo dvInfo;
        dvInfo._ce = 0;
        dvInfo._updateTime = now;
        dvInfo._valid = false;
        dvInfo._inPort = 0;
        m_mmu->m_dvRouting.PathCE_Table[dip] = dvInfo;
    }
}

void SwitchNode::AddPathChoiceTableEntry(Ipv4Address &dstAddr, Time now){
    Time t1 = Seconds (0.0);
    uint32_t dip = dstAddr.Get();
    auto dstIter = m_mmu->m_caverRouting.PathChoiceTable.find(dip);
    if (dstIter == m_mmu->m_caverRouting.PathChoiceTable.end()) {
        // 如果不存在，则创建一个新的条目
        for (int i = 0; i < m_mmu->m_caverRouting.m_pathChoice_num; ++i) {
            PathChoiceInfo pathChoiceInfo;
            // TODO:这里不确定初始化的时候设置成为now会不会让这些路径都是invalid
            pathChoiceInfo._updateTime = t1;
            pathChoiceInfo._is_used = false;
            m_mmu->m_caverRouting.PathChoiceTable[dip].push_back(pathChoiceInfo); 
        }
    }

    auto dstMapIter = m_mmu->m_caverRouting.PathChoiceFlagMap.find(dip);
    if (dstMapIter == m_mmu->m_caverRouting.PathChoiceFlagMap.end()) {
        // 如果不存在，则创建一个新的条目
        m_mmu->m_caverRouting.PathChoiceFlagMap[dip] = 0;
    }
}
void SwitchNode::AddPathChoiceTableEntry_noshare(Ipv4Address &dstAddr, Time now){
    Time t1 = Seconds (0.0);
    uint32_t dip = dstAddr.Get();
    auto dstIter = m_mmu->m_noshareRouting.PathChoiceTable.find(dip);
    if (dstIter == m_mmu->m_noshareRouting.PathChoiceTable.end()) {
        // 如果不存在，则创建一个新的条目
        for (int i = 0; i < m_mmu->m_noshareRouting.m_pathChoice_num; ++i) {
            PathChoiceInfo pathChoiceInfo;
            // TODO:这里不确定初始化的时候设置成为now会不会让这些路径都是invalid
            pathChoiceInfo._updateTime = t1;
            pathChoiceInfo._is_used = false;
            m_mmu->m_noshareRouting.PathChoiceTable[dip].push_back(pathChoiceInfo); 
        }
    }
    auto dstMapIter = m_mmu->m_noshareRouting.PathChoiceFlagMap.find(dip);
    if (dstMapIter == m_mmu->m_noshareRouting.PathChoiceFlagMap.end()) {
        // 如果不存在，则创建一个新的条目
        m_mmu->m_noshareRouting.PathChoiceFlagMap[dip] = 0;
    }
}

void SwitchNode::AddBestPathCETableEntry(Ipv4Address &dstAddr, Time now){
    // std::cout << dstAddr;
    uint32_t dip = dstAddr.Get();
    auto dstIter = m_mmu->m_caverRouting.best_pathCE_Table.find(dip);
    if (dstIter == m_mmu->m_caverRouting.best_pathCE_Table.end()) {
        // 如果不存在，则创建一个新的条目
        bestCaverInfo caverInfo;
        caverInfo._ce = 0;
        caverInfo._updateTime = now;
        caverInfo._valid = false;
        caverInfo._inPort = 0;
        m_mmu->m_caverRouting.best_pathCE_Table[dip] = caverInfo;
    }
}

void SwitchNode::AddBestPathCETableEntry_noshare(Ipv4Address &dstAddr, Time now){
    // std::cout << dstAddr;
    uint32_t dip = dstAddr.Get();
    auto dstIter = m_mmu->m_noshareRouting.best_pathCE_Table.find(dip);
    if (dstIter == m_mmu->m_noshareRouting.best_pathCE_Table.end()) {
        // 如果不存在，则创建一个新的条目
        bestCaverInfo caverInfo;
        caverInfo._ce = 0;
        caverInfo._updateTime = now;
        caverInfo._valid = false;
        caverInfo._inPort = 0;
        m_mmu->m_noshareRouting.best_pathCE_Table[dip] = caverInfo;
    }
}
void SwitchNode::AddACCPathCETableEntry(Ipv4Address &dstAddr, Time now){
    // std::cout << dstAddr;
    uint32_t dip = dstAddr.Get();
    auto dstIter = m_mmu->m_caverRouting.acceptable_path_table.find(dip);
    if (dstIter == m_mmu->m_caverRouting.acceptable_path_table.end()) {
        // 如果不存在，则创建一个新的条目
        bestCaverInfo caverInfo;
        caverInfo._ce = 0;
        caverInfo._updateTime = now;
        caverInfo._valid = false;
        caverInfo._inPort = 0;
        m_mmu->m_caverRouting.acceptable_path_table[dip] = caverInfo;
    }
}

void SwitchNode::AddACCPathCETableEntry_noshare(Ipv4Address &dstAddr, Time now){
    // std::cout << dstAddr;
    uint32_t dip = dstAddr.Get();
    auto dstIter = m_mmu->m_noshareRouting.acceptable_path_table.find(dip);
    if (dstIter == m_mmu->m_noshareRouting.acceptable_path_table.end()) {
        // 如果不存在，则创建一个新的条目
        bestCaverInfo caverInfo;
        caverInfo._ce = 0;
        caverInfo._updateTime = now;
        caverInfo._valid = false;
        caverInfo._inPort = 0;
        m_mmu->m_noshareRouting.acceptable_path_table[dip] = caverInfo;
    }
}
void SwitchNode::AddPathCE_port_TableEntry(Ipv4Address &dstAddr, uint32_t intf_idx, Time now){
    uint32_t dip = dstAddr.Get();
    auto dstIter = m_mmu->m_dvRouting.PathCE_port_Table.find(dip);
    if (dstIter == m_mmu->m_dvRouting.PathCE_port_Table.end()) {
        // 如果不存在，则创建一个新的条目
        m_mmu->m_dvRouting.PathCE_port_Table[dip] = std::map<uint32_t, DVInfo>();
    }
    DVInfo dvInfo;
    dvInfo._ce = 0;
    dvInfo._updateTime = now;
    dvInfo._valid = false;
    m_mmu->m_dvRouting.PathCE_port_Table[dip][intf_idx] = dvInfo;
}

// *******************************Add end**********************//
void SwitchNode::ClearTable() { m_rtTable.clear(); }

uint64_t SwitchNode::GetTxBytesOutDev(uint32_t outdev) {
    assert(outdev < pCnt);
    return m_txBytes[outdev];
}
uint64_t SwitchNode::GetRxBytesOutDev(uint32_t outdev) {
    assert(outdev < pCnt);
    return m_rxBytes[outdev];
} /* namespace ns3 */
void SwitchNode::DecreaseGlobalDre(){
    if(Dive_optimal_log){
        auto it = Settings::m_nodeInterfaceMap.find(GetId());
        if (it != Settings::m_nodeInterfaceMap.end()) {
            const std::map<uint32_t, uint32_t>& interfaceMap = it->second;
            for (const auto& interface : interfaceMap) {
                uint32_t neighborID = interface.second;
                uint32_t X = Settings::global_dre_map[{GetId(), neighborID}];
                X = X * (1 - Settings::caver_alpha);
                Settings::global_dre_map[{GetId(), neighborID}] = X;
            }
        } else {
            assert(false && "Cannot find the interface map");
        }
    }
}
void SwitchNode::GlobalDreEvent() {
    if(Dive_optimal_log){
        DecreaseGlobalDre();
        NS_LOG_FUNCTION(Simulator::Now());
        std::cout << "GlobalDreEvent: " << GetId() << " at " << Simulator::Now() << std::endl;
        m_GlobaldreEvent = Simulator::Schedule(Settings::Dre_time_map[GetId()], &SwitchNode::GlobalDreEvent, this);
    }
}
void SwitchNode::DoDispose(){
    if(Dive_optimal_log){
        m_GlobaldreEvent.Cancel();
    }
}

uint64_t SwitchNode::GetQpKey(uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg) {
    return ((uint64_t)dip << 32) | ((uint64_t)sport << 16) | (uint64_t)pg | (uint64_t)dport;
}
std::unordered_map<uint32_t, uint64_t> SwitchNode::GetFlowBytes(){
    return flow_bytes;
}
}

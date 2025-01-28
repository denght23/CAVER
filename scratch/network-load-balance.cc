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
 * Authors: Chahwan Song <skychahwan@gmail.com>
 */

#include <ns3/assert.h>
#include <ns3/rdma-client-helper.h>
#include <ns3/rdma-client.h>
#include <ns3/rdma-driver.h>
#include <ns3/rdma.h>
#include <ns3/sim-setting.h>
#include <ns3/switch-node.h>
#include <time.h>

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <filesystem>

#include "ns3/applications-module.h"
#include "ns3/broadcom-node.h"
#include "ns3/conga-routing.h"
#include "ns3/conweave-voq.h"
#include "ns3/hula-routing.h"
#include "ns3/core-module.h"
#include "ns3/error-model.h"
#include "ns3/global-route-manager.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/letflow-routing.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/qbb-helper.h"
#include "ns3/qbb-net-device.h"
#include "ns3/rdma-hw.h"
#include "ns3/settings.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("GENERIC_SIMULATION");

/*------Load balancing parameters-----*/
// mode for load balancer, 0: flow ECMP, 2: DRILL, 3: Conga, 6: Letflow, 9: ConWeave 12:Hula
uint32_t lb_mode = 0;

// Conga params (based on paper recommendation)
Time conga_flowletTimeout = MicroSeconds(100);  // 100us
Time conga_dreTime = MicroSeconds(50);
Time conga_agingTime = MicroSeconds(500);
uint32_t conga_quantizeBit = 3;
double conga_alpha = 0.2;

// DV params
Time dv_flowletTimeout = MilliSeconds(2); // 2ms
Time dv_dreTime = MicroSeconds(50);
Time dv_agingTime = MicroSeconds(500);

// *******************************Delete begin**********************//
// uint32_t dv_quantizeBit = 3;
// *******************************Delete end**********************//
// *******************************Add begin**********************//
uint32_t dv_quantizeBit = 8;
// *******************************Add end**********************//
double dv_alpha = 0.2;

// CAVER params
// TODO:看一下flowletTimeout是否会有影响？
Time caver_dreTime = MicroSeconds(30);
Time caver_agingTime = MicroSeconds(500);
uint32_t caver_quantizeBit = 8;
double caver_alpha = 0.2;
Time caver_flowletTimeout = MicroSeconds(100); // 100us
double caver_ce_threshold = 1.5;
Time caver_patchoiceTimeout = MicroSeconds(50);
uint32_t caver_pathChoice_num = 4;

Time caver_tau = MicroSeconds(100);
bool caver_useEWMA = true;
bool init_log = false;
bool global_ce_log = false;
uint32_t global_ce_mon_interval = 20; //us

// Letflow params
Time letflow_flowletTimeout = MicroSeconds(100);  // 100us
Time letflow_agingTime = MilliSeconds(2);  // just to clear the unused map entries for simul speed

// Conweave params
Time conweave_extraReplyDeadline = MicroSeconds(4);       // additional term to reply deadline
Time conweave_pathPauseTime = MicroSeconds(8);            // time to send packets to congested path
Time conweave_txExpiryTime = MicroSeconds(1000);          // waiting time for CLEAR
Time conweave_extraVOQFlushTime = MicroSeconds(32);       // extra for uncertainty
Time conweave_defaultVOQWaitingTime = MicroSeconds(500);  // default flush timer if no history
bool conweave_pathAwareRerouting = true;

// Hula params
Time hula_probeGenerationInterval = MicroSeconds(70);//探针的生成间隔
Time hula_keepAliveThresh = MicroSeconds(hula_probeGenerationInterval.GetMicroSeconds() * 5);        //探针老化时间
Time hula_probeTransmitInterval = MicroSeconds(hula_probeGenerationInterval.GetMicroSeconds());  //转发探针的时间窗口
Time hula_flowletInterval = MicroSeconds(100);       //flowlet的区分间隔 (e.g., 100us)
//计算链路利用率使用，至少是探针的生成间隔的两倍,这里暂时设置为三倍
Time hula_tau = MicroSeconds(hula_probeGenerationInterval.GetMicroSeconds() * 3);      

/*------------------------ simulation variables -----------------------------*/
uint64_t one_hop_delay = 1000;  // nanoseconds
uint32_t cc_mode = 1;           // mode for congestion control, 1: DCQCN
bool enable_qcn = true, enable_pfc = true, use_dynamic_pfc_threshold = true;
uint32_t packet_payload_size = 1000, l2_chunk_size = 0, l2_ack_interval = 0;
double pause_time = 5;  // PFC pause, microseconds
double flowgen_start_time = 2.0, flowgen_stop_time = 2.5, simulator_extra_time = 0.1;
// queue length monitoring time is not used in this simulator
// uint32_t qlen_dump_interval = 100000000, qlen_mon_interval = 1000;  // ns
uint64_t qlen_mon_start;               // ns
uint64_t qlen_mon_end;                 // ns
uint32_t switch_mon_interval = 10000;  // ns
uint32_t server_rtt_mon_interval = 10000;
uint64_t cnp_mon_start;                // ns
uint64_t cnp_monitor_bucket = 100000;  // ns
uint64_t irn_mon_start;                // ns
uint64_t irn_monitor_bucket = 100000;  // ns

FILE *pfc_file = NULL;
FILE *fct_output = NULL;
FILE *m_packetHeader_output = NULL;
FILE *flow_input_stream = NULL;
FILE *cnp_output = NULL;
FILE *est_error_output = NULL;
FILE *voq_output = NULL;
FILE *voq_detail_output = NULL;
FILE *uplink_output = NULL;
FILE *downlink_output = NULL;
FILE *uplink_rx_output = NULL;
FILE *downlink_rx_output = NULL; 
FILE *flow_rx_output = NULL;
FILE *bps_tx_output = NULL;
FILE *conn_output = NULL;
FILE *global_CE_map_output =NULL;
FILE *all_links_output = NULL;
FILE *flow_distribution = NULL;
FILE *packetId2FlowId = NULL;
FILE *ideal_ce = NULL;

std::string data_rate, link_delay, topology_file, flow_file;
std::string flow_input_file = "flow.txt";
std::string fct_output_file = "fct.txt";
std::string pfc_output_file = "pfc.txt";
std::string cnp_output_file = "cnp.txt";
std::string qlen_mon_file = "qlen.txt";
std::string voq_mon_file = "voq.txt";
std::string voq_mon_detail_file = "voq_detail.txt";
std::string uplink_mon_file = "uplink.txt";
std::string downlink_mon_file = "downlink.txt";
std::string uplink_rx_mon_file = "uplink_rx.txt";
std::string downlink_rx_mon_file = "downlink_rx.txt";
std::string flow_mon_file = "flow_rx.txt";
std::string bps_mon_file  = "bps_tx.txt";
std::string conn_mon_file = "conn.txt";
std::string est_error_output_file = "est_error.txt";
std::string global_CE_map_mon_file = "global_ce_map.txt";
std::string all_links_mon_file = "all_links.txt";
//TODO:my code to add a file to store the packet header
std::string m_packetHeaderFile = "pakcet_header.txt";

// CC params
double alpha_resume_interval = 55, rp_timer = 300, ewma_gain = 1 / 16;
double rate_decrease_interval = 4;
uint32_t fast_recovery_times = 1;
std::string rate_ai, rate_hai, min_rate = "100Mb/s";
std::string dctcp_rate_ai = "1000Mb/s";

bool clamp_target_rate = false, l2_back_to_zero = false;
double error_rate_per_link = 0.0;
uint32_t has_win = 1;
uint32_t global_t = 1;
uint32_t mi_thresh = 5;
bool var_win = false, fast_react = true;
bool multi_rate = true;
bool sample_feedback = false;
double u_target = 0.95;
uint32_t int_multi = 1;
bool rate_bound = true;
unordered_map<uint64_t, uint32_t> rate2kmax, rate2kmin;
unordered_map<uint64_t, double> rate2pmax;
unordered_map<uint32_t, Ptr<SwitchNode>> idxNodeToR;  // Id -> Ptr

// config of link-down scenario, ACK priority, and buffer
uint64_t link_down_time = 0;
uint32_t link_down_A = 0, link_down_B = 0;
uint32_t buffer_size = 0;  // 0 to set buffer size automatically

// Added from Here
double load = 10.0;
int enable_irn = 0;
int random_seed = 1;  // change this randomly if you want random expt

uint64_t maxRtt, maxBdp;

// app parameters
//struct Interface {
//    uint32_t idx;
//    bool up;
//    uint64_t delay;
//    uint64_t bw;
//
//    Interface() : idx(0), up(false) {} //initial 
//};
// nbr2if[snode][dnode] -> Interface
map<Ptr<Node>, map<Ptr<Node>, Interface>> nbr2if;
map<Ptr<Node>, map<uint32_t, uint32_t> > &if2id = Settings::if2id;
// Mapping destination to next hop for each node: <node, <dest, <nexthop0, ...> > >
map<Ptr<Node>, map<Ptr<Node>, vector<Ptr<Node>>>> nextHop;
map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairDelay;
map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairTxDelay;
map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairBw;
map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairBdp;
map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairRtt;

// for uplink/Downlink monitoring at TOR switches (load balance performance)
std::map<uint32_t, std::vector<uint32_t>> torId2UplinkIf;
std::map<uint32_t, std::vector<uint32_t>> torId2DownlinkIf;

// input files
std::ifstream topof, flowf; // variable to store file contents
NodeContainer n;                         // node container
std::vector<Ipv4Address> serverAddress;  // server address 
//place to store the server ip address
std::map<uint32_t, uint32_t> SrcId2CurSrcToR;//Src server id 2 current source ToR local index

// flow generator
std::unordered_map<uint32_t, uint32_t> flows_per_host;
uint32_t flow_id = 0;
std::unordered_map<uint32_t, uint16_t> portNumber;
std::unordered_map<uint32_t, uint16_t> dportNumber;
uint16_t *port_per_host;

// Scheduling input flows from flow.txt
struct FlowInput {
    uint32_t src, dst, pg, maxPacketCount, port;
    double start_time;
    uint32_t idx;
};//TODO: what is pg, maybe Priority Group?
FlowInput flow_input = {0};  // global variable
uint32_t flow_num;


//my log
bool SrcDstToR_log = false;
bool Path_id_log = false;
bool Path_Table_log = false;
std::map<uint32_t, std::map<uint32_t, std::vector<uint32_t>>> ConvertAndStore(const std::map<Ptr<Node>, std::map<Ptr<Node>, std::vector<Ptr<Node>>>>& nextHop){
    std::map<uint32_t, std::map<uint32_t, std::vector<uint32_t>>> m_nextHop;
    for (const auto& outerPair : nextHop) {
        uint32_t outerId = outerPair.first->GetId();
        for (const auto& innerPair : outerPair.second) {
            uint32_t innerId = innerPair.first->GetId();
            
            std::vector<uint32_t> idVector;
            for (const auto& nodePtr : innerPair.second) {
                idVector.push_back(nodePtr->GetId());
            }

            m_nextHop[outerId][innerId] = idVector;
        }
    }
    return m_nextHop;
}
std::map<uint32_t, std::map<uint32_t, uint32_t>> CreateNodeInterfaceMap(const std::map<Ptr<Node>, std::map<Ptr<Node>, Interface>>& nbr2if){
    std::map<uint32_t, std::map<uint32_t, uint32_t>> nodeInterfaceMap;//给定本节点的id 以及接口的id，返回邻居节点的id
    // 遍历输入的 nbr2if 数据结构
    for (const auto& [node, ifMap] : nbr2if) {
        int nodeId = node->GetId();
        
        for (const auto& [nbrNode, iface] : ifMap) {
            int interfaceId = iface.idx;
            int nbrNodeId = nbrNode->GetId();
            
            // 填充 map：本节点 ID -> (接口 ID -> 邻居节点 ID)
            nodeInterfaceMap[nodeId][interfaceId] = nbrNodeId;
        }
    }
    return nodeInterfaceMap;
}

std::map<uint32_t, std::map<uint32_t, uint32_t>> CreateNbr2Interface(const std::map<Ptr<Node>, std::map<Ptr<Node>, Interface>>& nbr2if){
    std::map<uint32_t, std::map<uint32_t, uint32_t>> Id_nbr2if;//给定本节点的id 以及接口的id，返回邻居节点的id
    // 遍历输入的 nbr2if 数据结构
    for (const auto& [node, ifMap] : nbr2if) {
        int nodeId = node->GetId();
        
        for (const auto& [nbrNode, iface] : ifMap) {
            int interfaceId = iface.idx;
            int nbrNodeId = nbrNode->GetId();
            
            // 填充 map：本节点 ID -> (接口 ID -> 邻居节点 ID)
            Id_nbr2if[nodeId][nbrNodeId] = interfaceId;
        }
    }
    return Id_nbr2if;
}

/**
 * Read flow input from file "flowf"
 */
void ReadFlowInput() {
    if (flow_input.idx < flow_num) {
        flowf >> flow_input.src >> flow_input.dst >> flow_input.pg >> flow_input.maxPacketCount >>
            flow_input.start_time;
        assert(n.Get(flow_input.src)->GetNodeType() == 0 &&
               n.Get(flow_input.dst)->GetNodeType() == 0);
    } else {
        std::cout << "*** input flow is over the prefixed number -- flow number : " << flow_num
                  << std::endl;
        std::cout << "*** flow_input.idx : " << flow_input.idx << std::endl;
        std::cout << "*** THIS IS THE LAST FLOW TO SEND :) " << std::endl;
    }
}

/**
 * Scheduling flows given in /config/L_XX....txt file
 */
void ScheduleFlowInputs(FILE *infile) {
    NS_LOG_DEBUG("ScheduleFlowInputs at " << Simulator::Now());
    while (flow_input.idx < flow_num && Seconds(flow_input.start_time) == Simulator::Now()) {
        if (flow_input.idx % 1000 == 0) {
            std::time_t t = std::time(nullptr);
            std::tm* now = std::localtime(&t);
            std::cout << std::put_time(now, "%H:%M:%S") << " " << flow_input.idx << "条流已导入"<<std::endl;
        }
        uint32_t pg, src, dst, sport, dport, maxPacketCount, target_len;
        pg = flow_input.pg;
        src = flow_input.src;
        dst = flow_input.dst;


        // src port
        sport = portNumber[src];  // get a new port number
        portNumber[src] = portNumber[src] + 1;

        // dst port
        dport = dportNumber[dst];
        dportNumber[dst] = dportNumber[dst] + 1;

        if (lb_mode == 9 || lb_mode == 12 || lb_mode == 3 || lb_mode == 6){
            //std::cout << "Flow read" << std::endl;
            auto it = SrcId2CurSrcToR.find(src);
            if (it != SrcId2CurSrcToR.end()){
                uint32_t SrcToR = Settings::hostId2ToRlist[src][it->second];
                uint32_t DstToR;
                SrcId2CurSrcToR[src] = (SrcToR + 1) % Settings::hostId2ToRlist[src].size();
                //判断src是否能两跳到达dst
                auto dst_it = std::find(Settings::TorSwitch_nodelist[SrcToR].begin(), Settings::TorSwitch_nodelist[SrcToR].end(), Settings::hostId2IpMap[dst]);
                if (SrcDstToR_log){
                    std::cout << "TorSwitch_nodelist info:" << "flow id:" << flow_input.idx << std::endl;
                    for (size_t i = 0; i < Settings::TorSwitch_nodelist[SrcToR].size(); ++i) {
                        std::cout << Settings::TorSwitch_nodelist[SrcToR][i] << " ";
                    }
                    std::cout << std::endl;
                }
                if (dst_it != Settings::TorSwitch_nodelist[SrcToR].end()){
                    //两跳到达
                    DstToR = SrcToR;
                    Settings::flowId2SrcDst[flow_input.idx] = std::make_pair(SrcToR, DstToR);
                    uint32_t outPort = nbr2if[n.Get(src)][n.Get(SrcToR)].idx;
                    Settings::flowId2Port2Src[flow_input.idx] = outPort;
                }
                else{
                    //随机选取一个ToR switch
                    int randomIndex = std::rand() % Settings::hostId2ToRlist[dst].size();
                    DstToR = Settings::hostId2ToRlist[dst][randomIndex];
                    Settings::flowId2SrcDst[flow_input.idx] = std::make_pair(SrcToR, DstToR);
                    uint32_t outPort = nbr2if[n.Get(src)][n.Get(SrcToR)].idx;
                    Settings::flowId2Port2Src[flow_input.idx] = outPort;
                }
                if (SrcDstToR_log){
                    std::cout << "SrcDstToR info: " << " Flow id: "<< flow_input.idx<<", Src: " << src << " Dst: " << dst << " SrcToR: " << SrcToR << " DstToR: " << DstToR << 
                           " sport: " << sport << " dport: " << dport << std::endl;
                    std::cout << "OutPort list:" << std::endl;
                    auto& show_innerMap = nbr2if[n.Get(src)];
                    for (auto& show_innerPair : show_innerMap) {
                        Ptr<Node> innerKey = show_innerPair.first; // 获取内部 map 的第一个键
                        Interface value = show_innerPair.second; // 获取内部 map 的值

                        // 输出内部 map 的键值对
                        std::cout << "OuPort: " << value.idx << " -> " << "id: "  << innerKey->GetId() << std::endl;
                    }
                    std::cout << "OutPort: " << Settings::flowId2Port2Src[flow_input.idx] << std::endl;
                    fflush(stdout);
                }
            }
            else{
                assert(false && "SrcId2CurSrcToR does not contain the src server id");
            }
        }
        fflush(stdout);

        Settings::PacketId2FlowId[std::make_tuple(src, dst, sport, dport)] = flow_input.idx;
        Settings::QPPair_info2FlowId[std::make_tuple(serverAddress[src], serverAddress[dst], static_cast<uint16_t>(sport), static_cast<uint16_t>(dport))] = flow_input.idx;
        Settings::FlowId2SrcId[flow_input.idx] = src;
        //std::cout << "Flow ID: " << flow_input.idx << std::endl;
        target_len = flow_input.maxPacketCount;  // this is actually not packet-count, but bytes
        if (target_len == 0) {
            target_len = 1;
        }
        Settings::FlowId2Length[flow_input.idx] = target_len;
        assert(n.Get(src)->GetNodeType() == 0 && n.Get(dst)->GetNodeType() == 0);

        /**
         * Turn on if you want to record all input streams into output file for logging.
         * But, the input stream can be found in config. We do not recommend to do this
         * as it consumes storage resource, redundantly.
         */
        if (0) {  // logging input streams to "XXXX_out_in.txt"
            /************************
             * record flow's 4-tuple
             ************************/
            fprintf(infile, "%u %u %u %u %u %lu\n", src, dst, sport, dport, target_len,
                    (uint64_t)(flow_input.start_time * (uint64_t)1000000000));
            fflush(infile);

            /***********    FCT Tracking    **************/
            UdpServerHelper server0(dport);
            server0.SetAttribute("FlowSize", UintegerValue(target_len));
            server0.SetAttribute("irn", BooleanValue(enable_irn));
            server0.SetAttribute("StatHostSrc", UintegerValue(src));
            server0.SetAttribute("StatHostDst", UintegerValue(dst));
            server0.SetAttribute("StatRxLen", UintegerValue(target_len));
            server0.SetAttribute("StatFlowID", UintegerValue(flow_input.idx));
            server0.SetAttribute("Port", UintegerValue(dport));

            ApplicationContainer apps0s = server0.Install(n.Get(dst));  // DST
            apps0s.Start(Seconds(Time(0)));
            apps0s.Stop(Seconds(100.0));
        }  // end of logging input streams
 
        if (pairRtt.find(n.Get(src)) == pairRtt.end() ||
            pairRtt[n.Get(src)].find(n.Get(dst)) == pairRtt[n.Get(src)].end()) {
            std::cerr << "pairRtt src: " << src << " -> dst: " << dst
                      << " ==> cannot be found from database" << std::endl;
            assert(false);
        }

        RdmaClientHelper clientHelper(
            pg, serverAddress[src], serverAddress[dst], sport, dport, target_len,
            has_win ? (global_t == 1 ? maxBdp : pairBdp[n.Get(src)][n.Get(dst)]) : 0,
            global_t == 1 ? maxRtt : pairRtt[n.Get(src)][n.Get(dst)]);
        clientHelper.SetAttribute("StatFlowID", IntegerValue(flow_input.idx));

        ApplicationContainer appCon = clientHelper.Install(n.Get(src));  // SRC
        appCon.Start(Seconds(Time(0)));
        appCon.Stop(Seconds(100.0));

        flow_input.idx++;
        ReadFlowInput();
    }

    // schedule the next time to run this function
    if (flow_input.idx < flow_num) {
        Simulator::Schedule(Seconds(flow_input.start_time) - Simulator::Now(), &ScheduleFlowInputs,
                            infile);
    } else {  // no more flows, close the file
        flowf.close();
    }
}

/**
 * @brief CNP frequency monitoring (timestamp nodeId ECN OoO Total)
 */
void cnp_freq_monitoring(FILE *fout, Ptr<RdmaHw> rdmahw) {
    if (rdmahw->cnp_total > 0) {
        // flush
        fprintf(fout, "%lu %u %u %u %u\n", Simulator::Now().GetNanoSeconds(),
                rdmahw->m_node->GetId(), rdmahw->cnp_by_ecn, rdmahw->cnp_by_ooo, rdmahw->cnp_total);
        fflush(fout);

        // initialize
        rdmahw->cnp_by_ecn = 0;
        rdmahw->cnp_by_ooo = 0;
        rdmahw->cnp_total = 0;
    }

    // recursive callback
    Simulator::Schedule(NanoSeconds(cnp_monitor_bucket), &cnp_freq_monitoring, fout, rdmahw);
}

/**
 * @brief TOR Switch monitoring
 * - VOQ number and uplink throughput at switches
 * - the number of active connections at RNICS
 */
void periodic_monitoring(FILE *fout_voq, FILE *fout_voq_detail, FILE *fout_uplink,  FILE *fout_conn,
                         uint32_t *lb_mode) {
    uint32_t lb_mode_val = *lb_mode;
    uint64_t now = Simulator::Now().GetNanoSeconds();
    if (lb_mode_val == 12 && 0) {
        std::cout<<"Periodic monitor "<<now<<std::endl;
        for (int i = 0; i < n.GetN(); i++) {
            Ptr<Node> node = n.Get(i);
            if (node->GetNodeType() == 1) {
                Ptr<SwitchNode> snode = DynamicCast<SwitchNode>(node);
                snode->m_mmu->m_hulaRouting.Print();
            }
        }
    }
    for (const auto &tor2If : torId2UplinkIf) {  // for each TOR switches
        Ptr<Node> node = n.Get(tor2If.first);    // tor id
        auto swNode = DynamicCast<SwitchNode>(node);
        assert(swNode->m_isToR == true);  // sanity check

        if (lb_mode_val == 9) {  // Conweave
            // monitor VOQ number per switch <time, ToRId, #VOQ, #Pkts>
            uint32_t nVOQ = swNode->m_mmu->m_conweaveRouting.GetNumVOQ();
            uint32_t nVolumeVOQ = swNode->m_mmu->m_conweaveRouting.GetVolumeVOQ();
            fprintf(fout_voq, "%lu,%u,%u,%u\n", now, tor2If.first, nVOQ, nVolumeVOQ);

            // monitor VOQ per destination IP <time, dstip, #VOQ, #Pkts>
            std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> dip_to_nvoq_npkt;
            for (auto voq : swNode->m_mmu->m_conweaveRouting.GetVOQMap()) {
                auto &nvoq_npkt = dip_to_nvoq_npkt[voq.second.getDIP()];
                nvoq_npkt.first += 1;
                nvoq_npkt.second += voq.second.getQueueSize();
            }
            for (auto x : dip_to_nvoq_npkt) {
                fprintf(fout_voq_detail, "%lu,%u,%u,%u\n", now, x.first, x.second.first,
                        x.second.second);
            }
        }

        // common: monitor TOR's uplink to measure load balancing performance
        for (const auto &iface : tor2If.second) {
            // monitor uplink txBytes <time, ToRId, OutDev, Bytes>
            uint64_t uplink_txbyte = swNode->GetTxBytesOutDev(iface);
            fprintf(fout_uplink, "%lu,%u,%u,%lu\n", now, tor2If.first, iface, uplink_txbyte);
        }
    }
    // for (const auto &tor2If : torId2DownlinkIf) {  // for each TOR switches
    //     Ptr<Node> node = n.Get(tor2If.first);    // tor id
    //     auto swNode = DynamicCast<SwitchNode>(node);
    //     assert(swNode->m_isToR == true);  // sanity check

    //     // common: monitor TOR's downlink to measure load balancing performance
    //     for (const auto &iface : tor2If.second) {
    //         // monitor downlink txBytes <time, ToRId, OutDev, Bytes>
    //         uint64_t downlink_txbyte = swNode->GetTxBytesOutDev(iface);
    //         fprintf(fout_downlink, "%lu,%u,%u,%lu\n", now, tor2If.first, iface, downlink_txbyte);
    //     }
    // }

    // common: get number of concurrent connections at each server
    for (uint32_t i = 0; i < Settings::node_num; i++) {
        if (n.Get(i)->GetNodeType() == 0) {  // is server
            Ptr<Node> server = n.Get(i);
            Ptr<RdmaDriver> rdmaDriver = server->GetObject<RdmaDriver>();
            Ptr<RdmaHw> rdmaHw = rdmaDriver->m_rdma;
            // monitor total/active QP number <time, serverId, #ExistingQP, #ActiveQP>
            uint64_t nQP = rdmaHw->m_qpMap.size();
            uint64_t nActiveQP = 0;
            for (auto qp : rdmaHw->m_qpMap) {
                if (qp.second->GetBytesLeft() > 0) {  // conns with bytes left
                    nActiveQP++;
                }
            }
            fprintf(fout_conn, "%lu,%u,%lu,%lu\n", now, i, nQP, nActiveQP);
        }
    }

    if (Simulator::Now() < Seconds(flowgen_stop_time + 0.05)) {
        // recursive callback
        Simulator::Schedule(NanoSeconds(switch_mon_interval), &periodic_monitoring, fout_voq,
                            fout_voq_detail, fout_uplink, fout_conn, lb_mode);  // every 10us
    }
    return;
}


void m_periodic_monitoring(FILE *fout_voq,FILE *fout_uplink,  FILE *fout_downlink, FILE *fout_conn,
                         uint32_t *lb_mode) {
    uint32_t lb_mode_val = *lb_mode;
    uint64_t now = Simulator::Now().GetNanoSeconds();
    for (const auto &tor2If : torId2UplinkIf) {  // for each TOR switches
        Ptr<Node> node = n.Get(tor2If.first);    // tor id
        auto swNode = DynamicCast<SwitchNode>(node);
        assert(swNode->m_isToR == true);  // sanity check

        if (lb_mode_val == 9) {  // Conweave
            // monitor VOQ number per switch <time, ToRId, #VOQ, #Pkts>
            uint32_t nVOQ = swNode->m_mmu->m_conweaveRouting.GetNumVOQ();
            uint32_t nVolumeVOQ = swNode->m_mmu->m_conweaveRouting.GetVolumeVOQ();
            fprintf(fout_voq, "%lu,%u,%u,%u\n", now, tor2If.first, nVOQ, nVolumeVOQ);

            // monitor VOQ per destination IP <time, dstip, #VOQ, #Pkts>
            std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> dip_to_nvoq_npkt;
            for (auto voq : swNode->m_mmu->m_conweaveRouting.GetVOQMap()) {
                auto &nvoq_npkt = dip_to_nvoq_npkt[voq.second.getDIP()];
                nvoq_npkt.first += 1;
                nvoq_npkt.second += voq.second.getQueueSize();
            }
        }

        // common: monitor TOR's uplink to measure load balancing performance
        for (const auto &iface : tor2If.second) {
            // monitor uplink txBytes <time, ToRId, OutDev, Bytes>
            uint64_t uplink_txbyte = swNode->GetTxBytesOutDev(iface);
            fprintf(fout_uplink, "%lu,%u,%u,%lu\n", now, tor2If.first, if2id[swNode][iface], uplink_txbyte);
        }
    }
    for (const auto &tor2If : torId2DownlinkIf) {  // for each TOR switches
        Ptr<Node> node = n.Get(tor2If.first);    // tor id
        auto swNode = DynamicCast<SwitchNode>(node);
        assert(swNode->m_isToR == true);  // sanity check

        // common: monitor TOR's downlink to measure load balancing performance
        for (const auto &iface : tor2If.second) {
            // monitor downlink txBytes <time, ToRId, OutDev, Bytes>
            uint64_t downlink_txbyte = swNode->GetTxBytesOutDev(iface);
            fprintf(fout_downlink, "%lu,%u,%u,%lu\n", now, tor2If.first, if2id[swNode][iface], downlink_txbyte);
        }
    }

    // common: get number of concurrent connections at each server
    for (uint32_t i = 0; i < Settings::node_num; i++) {
        if (n.Get(i)->GetNodeType() == 0) {  // is server
            Ptr<Node> server = n.Get(i);
            Ptr<RdmaDriver> rdmaDriver = server->GetObject<RdmaDriver>();
            Ptr<RdmaHw> rdmaHw = rdmaDriver->m_rdma;
            // monitor total/active QP number <time, serverId, #ExistingQP, #ActiveQP>
            uint64_t nQP = rdmaHw->m_qpMap.size();
            uint64_t nActiveQP = 0;
            for (auto qp : rdmaHw->m_qpMap) {
                if (qp.second->GetBytesLeft() > 0) {  // conns with bytes left
                    nActiveQP++;
                }
            }
            fprintf(fout_conn, "%lu,%u,%lu,%lu\n", now, i, nQP, nActiveQP);
        }
    }

    if (Simulator::Now() < Seconds(flowgen_stop_time + 0.05)) {
        // recursive callback
        Simulator::Schedule(NanoSeconds(switch_mon_interval), &m_periodic_monitoring, fout_voq,
                            fout_uplink, fout_downlink, fout_conn, lb_mode);  // every 10us
    }
    return;
}

void m_QP_rate_monitoring(FILE *fout_voq)
{
    uint64_t now = Simulator::Now().GetNanoSeconds();
    for (uint32_t i = 0; i < Settings::node_num; i++) {
        if (n.Get(i)->GetNodeType() == 0) {  // is server
            Ptr<Node> server = n.Get(i);
            Ptr<RdmaDriver> rdmaDriver = server->GetObject<RdmaDriver>();
            Ptr<RdmaHw> rdmaHw = rdmaDriver->m_rdma;
            // monitor total/active QP number <time, serverId, #ExistingQP, #ActiveQP>
            uint64_t nQP = rdmaHw->m_qpMap.size();
            uint64_t nActiveQP = 0;
            for (auto qp : rdmaHw->m_qpMap) {
                Ipv4Address src = qp.second->sip;
                Ipv4Address dst = qp.second->dip;
                uint16_t sport = qp.second->sport;
                uint16_t dport = qp.second->dport;
                uint32_t flowid = Settings::QPPair_info2FlowId[std::make_tuple(src, dst, sport, dport)];
                DataRate m_rate = qp.second->m_rate;
                uint64_t m_bps = m_rate.GetBitRate();
                // std::cout << "bps: " << now << flowid << m_bps << std::endl;
                fprintf(fout_voq, "%lu,%u,%lu\n", now, flowid, m_bps);
            }
        }
    }
    if (Simulator::Now() < Seconds(flowgen_stop_time + 0.05)) {
        // recursive callback
        Simulator::Schedule(NanoSeconds(server_rtt_mon_interval), &m_QP_rate_monitoring, fout_voq);  // every 10us
    }
    return;
}

void m_global_ce_map_monitoring(FILE *fout){
    std::cout << "Global CE map monitoring, file_path: " <<  global_CE_map_mon_file << std::endl;
    Settings::UpdateCETable();
    Settings::writeCEMapSnapshot(fout);
    fprintf(ideal_ce, "{\"timestamp\": %ld, \"data\": [", Simulator::Now().GetNanoSeconds());
    bool first_i_j = true;  // 用于判断是否是第一个 (i, j)
    for (uint32_t i = 256; i < 256 + 32; i++) {
        for (uint32_t j = 0; j < 256; j++) {
            if (i == j) {
                continue;
            }

            std::vector<std::vector<uint32_t>> allPaths;
            Settings::findAllPaths(i, j, allPaths);

            // 对于每一对 (i, j) 在 JSON 中单独生成一个对象
            if (!first_i_j) {
                fprintf(ideal_ce, ",");  // 不是第一个对象，前面需要逗号
            }
            fprintf(ideal_ce, "{\"i\": %u, \"j\": %u, \"paths\": [", i, j);
            
            bool first_path = true;
            for (const auto& path : allPaths) {
                uint32_t pathCEExcludeLast = Settings::calculatePathCEExcludeLast(path);
                if (!first_path) {
                    fprintf(ideal_ce, ",");  // 不是第一个路径，前面需要逗号
                }
                fprintf(ideal_ce, "%u", pathCEExcludeLast);
                first_path = false;
            }

            fprintf(ideal_ce, "]}");
            first_i_j = false;
        }
    }
    
    fprintf(ideal_ce, "]}\n");
    if (Simulator::Now() < Seconds(flowgen_stop_time + 0.05)) {
        // recursive callback
        Simulator::Schedule(MicroSeconds(global_ce_mon_interval), &m_global_ce_map_monitoring, fout);  // every 10us
    }
}

void m_all_links_monitoring(FILE *fout){
    uint64_t now = Simulator::Now().GetNanoSeconds();
    for (auto i = nextHop.begin(); i != nextHop.end(); i++) {
        Ptr<Node> node = i->first;
        if (node->GetNodeType() == 1) {  // is switch
            Ptr<SwitchNode> swNode = DynamicCast<SwitchNode>(node);
            for (const auto &iface : Settings::m_nodeInterfaceMap[swNode->GetId()]) {
                uint64_t txBytes = swNode->GetTxBytesOutDev(iface.first);
                fprintf(fout, "%lu,%u,%u,%lu\n", now, swNode->GetId(), iface.second, txBytes);
            }
        }
    }
    if (Simulator::Now() < Seconds(flowgen_stop_time + 0.05)) {
        // recursive callback
        Simulator::Schedule(NanoSeconds(switch_mon_interval), &m_all_links_monitoring, fout);  // every 10us
    }

}

void m_rx_periodic_monitoring(FILE *fout_uplink_rx,  FILE *fout_downlink_rx, FILE *fout_flow_rx) {
    uint64_t now = Simulator::Now().GetNanoSeconds();
    for (const auto &tor2If : torId2UplinkIf) {  // for each TOR switches
        Ptr<Node> node = n.Get(tor2If.first);    // tor id
        auto swNode = DynamicCast<SwitchNode>(node);
        assert(swNode->m_isToR == true);  // sanity check

        // common: monitor TOR's uplink to measure load balancing performance
        for (const auto &iface : tor2If.second) {
            // monitor uplink txBytes <time, ToRId, OutDev, Bytes>
            uint64_t uplink_txbyte = swNode->GetRxBytesOutDev(iface);
            fprintf(fout_uplink_rx, "%lu,%u,%u,%lu\n", now, tor2If.first, if2id[swNode][iface], uplink_txbyte);
        }
    }
    for (const auto &tor2If : torId2DownlinkIf) {  // for each TOR switches
        Ptr<Node> node = n.Get(tor2If.first);    // tor id
        auto swNode = DynamicCast<SwitchNode>(node);
        assert(swNode->m_isToR == true);  // sanity check

        // common: monitor TOR's downlink to measure load balancing performance
        for (const auto &iface : tor2If.second) {
            // monitor downlink txBytes <time, ToRId, OutDev, Bytes>
            uint64_t downlink_txbyte = swNode->GetRxBytesOutDev(iface);
            fprintf(fout_downlink_rx, "%lu,%u,%u,%lu\n", now, tor2If.first, if2id[swNode][iface], downlink_txbyte);
        }
    }
    for (size_t ToRId = 0; ToRId < Settings::node_num; ToRId++) {
        Ptr<Node> node = n.Get(ToRId);
        if (node->GetNodeType() == 1) {  // switches
            auto swNode = DynamicCast<SwitchNode>(n.Get(ToRId));
            if (swNode->m_isToR == true){
                // sanity check
                std::unordered_map<uint32_t, uint64_t> flow_BYtes = swNode->GetFlowBytes();
                for (auto it = flow_BYtes.begin(); it != flow_BYtes.end(); ++it) {
                    uint32_t flow_id = it->first;
                    uint32_t src_id = Settings::FlowId2SrcId[flow_id];
                    uint32_t src_ip = Settings::hostId2IpMap[src_id];
                    auto find_ip = swNode->m_isToR_hostIP.find(src_ip);
                    if (find_ip != swNode->m_isToR_hostIP.end()) {
                        //fprintf(fout_flow_rx, "%lu,%u,%lu\n", now, it->first, it->second);
                    }
                }
            }
        }
    }
    if (Simulator::Now() < Seconds(flowgen_stop_time + 0.05)) {
        // recursive callback
        Simulator::Schedule(NanoSeconds(switch_mon_interval), &m_rx_periodic_monitoring, 
                            fout_uplink_rx, fout_downlink_rx, fout_flow_rx);  // every 10us
    }
    return;
}

void flow_distribution_monitoring(Time start_time, FILE* fout_flow_distribution) {
    Simulator::Schedule(start_time, &Settings::print_flow_distribution, fout_flow_distribution, MicroSeconds(50));
}


/**
 * @brief Conga timeout number recording
 */
void conga_history_print() {
    std::cout << "\n------------CONGA History---------------" << std::endl;
    std::cout << "Number of flowlet's timeout:" << CongaRouting::nFlowletTimeout
              << "Conga's timeout: " << conga_flowletTimeout << std::endl;
}

/**
 * @brief Letflow timeout number recording
 */
void letflow_history_print() {
    std::cout << "\n------------Letflow History---------------" << std::endl;
    std::cout << "Number of flowlet's timeout:" << LetflowRouting::nFlowletTimeout
              << "\nLetflow's timeout: " << letflow_flowletTimeout << std::endl;
}

/**
 * @brief Conweave rerouting/VOQ number recording
 */
void conweave_history_print() {
    // Conweave params
    std::cout << "\n------ConWeave parameters-----" << std::endl;
    std::cout << "Param - extraReplyDeadline:" << conweave_extraReplyDeadline << std::endl;
    std::cout << "Param - extraVOQFlushTime:" << conweave_extraVOQFlushTime << std::endl;
    std::cout << "Param - txExpiryTime:" << conweave_txExpiryTime << std::endl;
    std::cout << "Param - defaultVOQWaitingTime:" << conweave_defaultVOQWaitingTime << std::endl;
    std::cout << "Param - pathPauseTime:" << conweave_pathPauseTime << std::endl;
    std::cout << "Param - pathAwareRerouting:" << conweave_pathAwareRerouting << std::endl;

    std::cout << "\n------------ConWeave History---------------" << std::endl;
    std::cout << "Number of INIT's Reply sent (RTT_REPLY):" << ConWeaveRouting::m_nReplyInitSent
              << "\nNumber of Timely RTT_REPLY (INIT's Reply):" << ConWeaveRouting::m_nTimelyInitReplied
              << "\nNumber of TAIL's Reply Sent (CLEAR):" << ConWeaveRouting::m_nReplyTailSent
              << "\nNumber of Timely CLEAR (TAIL's Reply):" << ConWeaveRouting::m_nTimelyTailReplied
              << "\nNumber of NOTIFY Sent:" << ConWeaveRouting::m_nNotifySent
              << "\nNumber of Rerouting:" << ConWeaveRouting::m_nReRoute
              << "\nNumber of OoO enqueued pkts:" << ConWeaveRouting::m_nOutOfOrderPkts
              << "\nNumber of VOQ Flush Total:" << ConWeaveRouting::m_nFlushVOQTotal
              << "\nNumber of VOQ Flush From History:" << ConWeaveRouting::m_historyVOQSize.size()
              << "\nNumber of VOQ Flush by TAIL:" << ConWeaveRouting::m_nFlushVOQByTail
              << std::endl;

    std::cout << "--------------------------" << std::endl;

    /** VOQ: Sanity check*/
    for (size_t ToRId = 0; ToRId < Settings::node_num; ToRId++) {
        Ptr<Node> node = n.Get(ToRId);
        if (node->GetNodeType() == 1) {  // switches
            auto swNode = DynamicCast<SwitchNode>(n.Get(ToRId));
            if (swNode->m_isToR) {  // TOR switch
                uint32_t num_remained_voq = swNode->m_mmu->m_conweaveRouting.GetNumVOQ();
                if (num_remained_voq > 0) {
                    printf("*******************************\n");
                    printf("*** WARNING - Tor Sw (%lu) - VOQ (num=%u) is not flushed yet!! ***\n",
                           ToRId, num_remained_voq);
                    printf(
                        " -- Probably the history print is too early so simulation might not be "
                        "finished?");
                    printf("********************************\n");
                }
            }
        }
    }

    /** Get ConWeave Flush Time Estimation Error */
    if (0) {
        // sanity check - extraVOQFlushTime must be large enough to get accuracy
        assert(conweave_extraVOQFlushTime >= MicroSeconds(128) && "PARAMETER ERROR!!");

        std::cout << "\n--------------------------" << std::endl;
        std::cout << "Extracting ConWeave Estimation Error Data..." << std::endl;
        est_error_output = fopen(est_error_output_file.c_str(), "w");
        for (auto x : ConWeaveVOQ::m_flushEstErrorhistory) {
            fprintf(est_error_output, "%d\n", x);
        }
        ConWeaveVOQ::m_flushEstErrorhistory.clear();
        std::cout << "---------D O N E---------" << std::endl;
    }
}


void hula_history_print() {
    std::cout << "\n------------Hula History---------------" << std::endl;
    std::cout << "Number of flowlet's timeout:" << HulaRouting::nFlowletTimeout
              << "\nHula's timeout: " << hula_flowletInterval << std::endl;    
    for (auto module : HulaRouting::hulaModules) {
        printf("%d: Recieved %d probes, sent %d probes, %d aged probes, %d probes update next hop\n", 
            module->m_switch_id, module->probeReceiveNum, module->probeSendNum, module->probeAgedNum, module->probeUpdateHopNum);
    }
}

void dv_history_print() {
    std::cout << "\n------------DV History---------------" << std::endl;
    for (auto module : DVRouting::dvModules) {
        printf("%d: 刷新%d次最优路径\n", 
            module->m_switch_id, module->pathUpdateTimes);
    }
}

/**
 * @brief When one RDMA is finished, so does (1) QP, (2) RxQP, (3) write it on file fct.txt.
 */
void qp_finish(FILE *fout, Ptr<RdmaQueuePair> q) {
    uint32_t sid = Settings::ip_to_node_id(q->sip), did = Settings::ip_to_node_id(q->dip);
    uint64_t base_rtt = pairRtt[n.Get(sid)][n.Get(did)];
    uint64_t b = pairBw[n.Get(sid)][n.Get(did)];
    uint32_t total_bytes =
        q->m_size + ((q->m_size - 1) / packet_payload_size + 1) *
                        (CustomHeader::GetStaticWholeHeaderSize() -
                         IntHeader::GetStaticSize());  // translate to the minimum bytes required
                                                       // (with header but no INT)
    uint64_t standalone_fct = base_rtt + total_bytes * 8000000000lu / b;

    // XXX: remove rxQP from the receiver 
    Ptr<Node> dstNode = n.Get(did);
    Ptr<RdmaDriver> rdma = dstNode->GetObject<RdmaDriver>();
    rdma->m_rdma->DeleteRxQp(q->sip.Get(), q->sport, q->dport, q->m_pg);

    // fprintf(fout, "%lu QP complete\n", Simulator::Now().GetTimeStep());
    fprintf(fout, "%u %u %u %u %lu %lu %lu %lu\n", Settings::ip_to_node_id(q->sip),
            Settings::ip_to_node_id(q->dip), q->sport, q->dport, q->m_size,
            q->startTime.GetTimeStep(), (Simulator::Now() - q->startTime).GetTimeStep(),
            standalone_fct);

    // for debugging
    NS_LOG_DEBUG("%u %u %u %u %lu %lu %lu %lu\n" %
                 (Settings::ip_to_node_id(q->sip), Settings::ip_to_node_id(q->dip), q->sport,
                  q->dport, q->m_size, q->startTime.GetTimeStep(),
                  (Simulator::Now() - q->startTime).GetTimeStep(), standalone_fct));
    Settings::cnt_finished_flows++;
    fflush(fout);

    //clean
    static std::queue<std::tuple<Ipv4Address, Ipv4Address, uint16_t, uint16_t>> finishedQpBuffer; //这个buffer存放将要被清理的flow。但是我们不能立即清理，因为在乱序状态下依旧可能有部分包残存在拓扑中
    uint32_t flow_id = Settings::PacketId2FlowId[std::make_tuple(sid, did, q->sport, q->dport)];
    fprintf(packetId2FlowId, "FlowKey: (%u %u %u %u) -> FlowId: %u\n", sid, did, q->sport, q->dport, flow_id);

    finishedQpBuffer.push(std::make_tuple(q->sip, q->dip, static_cast<uint16_t>(q->sport), static_cast<uint16_t>(q->dport)));
    if (finishedQpBuffer.size() > 100000) {
        auto& qp_info = finishedQpBuffer.front();
        uint32_t flow_id = Settings::QPPair_info2FlowId[qp_info];    
        Settings::PacketId2FlowId.erase(std::make_tuple(Settings::ip_to_node_id(std::get<0>(qp_info)), Settings::ip_to_node_id(std::get<1>(qp_info)), std::get<2>(qp_info), std::get<3>(qp_info)));
        Settings::flowId2SrcDst.erase(flow_id);
        Settings::flowId2Port2Src.erase(flow_id);
        Settings::FlowId2SrcId.erase(flow_id);
        Settings::FlowId2Length.erase(flow_id);
        Settings::QPPair_info2FlowId.erase(qp_info);
        finishedQpBuffer.pop();
        //printf("Flow %u cleared! current queue size:%d\n", flow_id, finishedQpBuffer.size());
    }
    fflush(stdin);
}

/**
 * @brief PFC event logging
 */
void get_pfc(FILE *fout, Ptr<QbbNetDevice> dev, uint32_t type) {
    // time, nodeID, nodeType, Interface's Idx, 0:resume, 1:pause
    //std::cout << "PFC event: " << Simulator::Now().GetTimeStep() << " " << dev->GetNode()->GetId()
    //          << " " << dev->GetNode()->GetNodeType() << " " << dev->GetIfIndex() << " " << type
    //          << std::endl;
    fprintf(fout, "%lu %u %u %u %u\n", Simulator::Now().GetTimeStep(), dev->GetNode()->GetId(),
            dev->GetNode()->GetNodeType(), dev->GetIfIndex(), type);
}

/*******************************************************************/
#if (false)

/**
 * @brief Qlen monitoring at switches (output: qlen.txt), I think "periodically"...
 *
 */
struct QlenDistribution {
    vector<uint32_t> cnt;  // cnt[i] is the number of times that the queue len is i KB
    void add(uint32_t qlen) {
        uint32_t kb = qlen / 1000;
        if (cnt.size() < kb + 1) cnt.resize(kb + 1);
        cnt[kb]++;
    }
};

map<uint32_t, map<uint32_t, QlenDistribution>> queue_result;
void monitor_buffer(FILE *qlen_output, NodeContainer *n) {
    /*******************************************************************/
    /************************** UNUSED NOW *****************************/
    /*******************************************************************/
    for (uint32_t i = 0; i < n->GetN(); i++) {
        if (n->Get(i)->GetNodeType() == 1) {  // is switch
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n->Get(i));
            if (queue_result.find(i) == queue_result.end()) queue_result[i];
            for (uint32_t j = 1; j < sw->GetNDevices(); j++) {
                uint32_t size = 0;
                for (uint32_t k = 0; k < SwitchMmu::qCnt; k++)
                    size += sw->m_mmu->egress_bytes[j][k];
                queue_result[i][j].add(size);
            }
        }
    }
    if (Simulator::Now().GetTimeStep() % qlen_dump_interval == 0) {
        fprintf(qlen_output, "time: %lu\n", Simulator::Now().GetTimeStep());
        for (auto &it0 : queue_result) {
            for (auto &it1 : it0.second) {
                fprintf(qlen_output, "%u %u", it0.first, it1.first);
                auto &dist = it1.second.cnt;
                for (uint32_t i = 0; i < dist.size(); i++) fprintf(qlen_output, " %u", dist[i]);
                fprintf(qlen_output, "\n");
            }
        }
        fflush(qlen_output);
    }
    if (Simulator::Now().GetTimeStep() < qlen_mon_end)
        Simulator::Schedule(NanoSeconds(qlen_mon_interval), &monitor_buffer, qlen_output, n);
}
#endif
/*******************************************************************/

/**
 * @brief Stop simulation in the middle (when almost all flows are done).
 * This function allows to finish simulation quickly when all messages are sent.
 */
void stop_simulation_middle() {
    uint32_t target_flow_num = flow_num - 0;  // can be lower than flownum
    if (Settings::cnt_finished_flows >= target_flow_num) {
        std::cout << "\n*** Simulator is enforced to be finished, finished so far: "
                  << Settings::cnt_finished_flows << "/ total: " << target_flow_num
                  << ", Time:" << Simulator::Now() << std::endl;

        // schedule conga timeout monitor
        if (lb_mode == 3) {  // CONGA
            conga_history_print();
        }
        if (lb_mode == 6) {  // LETFLOW
            letflow_history_print();
        }
        if (lb_mode == 9) {  // CONWEAVE
            conweave_history_print();
        }
        if (lb_mode == 12) {
            hula_history_print();
        }
        if (lb_mode == 10) {
            dv_history_print();
        }
        Simulator::Stop(NanoSeconds(1));  // finish soon, stop this schedule (NECESSARY!)
        return;
    }

    Simulator::Schedule(MicroSeconds(100), &stop_simulation_middle);  // check every 100us
}

/**
 * @brief Calculate edge-to-edge delays, TX delays, and bandwidths
 */
void CalculateRoute(Ptr<Node> host) {
    // queue for the BFS.
    vector<Ptr<Node>> q;
    // Distance from the host to each node.
    map<Ptr<Node>, int> dis;
    map<Ptr<Node>, uint64_t> delay;
    map<Ptr<Node>, uint64_t> txDelay;
    map<Ptr<Node>, uint64_t> bw;
    // init BFS.
    q.push_back(host);
    dis[host] = 0;
    delay[host] = 0;
    txDelay[host] = 0;
    bw[host] = 0xfffffffffffffffflu;

    // BFS.
    for (int i = 0; i < (int)q.size(); i++) {
        Ptr<Node> now = q[i];
        int d = dis[now];
        for (auto it = nbr2if[now].begin(); it != nbr2if[now].end(); it++) {
            // skip down link
            if (!it->second.up) continue;
            Ptr<Node> next = it->first;
            // If 'next' have not been visited.
            if (dis.find(next) == dis.end()) {
                dis[next] = d + 1;
                delay[next] = delay[now] + it->second.delay;  // maybe nanoseconds?
                txDelay[next] = txDelay[now] + packet_payload_size * 1000000000lu * 8 /
                                                   it->second.bw;  // maybe nanoseconds?
                bw[next] = std::min(bw[now], it->second.bw);
                // we only enqueue switch, because we do not want packets to go through host as
                // middle point
                if (next->GetNodeType() == 1) {
                    q.push_back(next);
                }
            }
            // if 'now' is on the shortest path from 'next' to 'host'.
            if (d + 1 == dis[next]) {
                nextHop[next][host].push_back(now);
            }
        }
    }
    for (auto it : delay) {
        pairDelay[it.first][host] = it.second;
    }
    for (auto it : txDelay) {
        pairTxDelay[it.first][host] = it.second;
    }
    for (auto it : bw) {
        pairBw[it.first][host] = it.second;
    }
}
void CalculateRoutes(NodeContainer &n) {
    for (int i = 0; i < (int)n.GetN(); i++) {
        Ptr<Node> node = n.Get(i);
        if (node->GetNodeType() == 0) {
            CalculateRoute(node);
        }
    }
    //for (const auto& [node, destMap] : nextHop) {
    //    cout << "Node: " << node->GetId() << endl;
    //    for (const auto& [dest, nexthops] : destMap) {
    //        cout << "  Destination: " << dest->GetId() << endl;
    //        cout << "    Next Hops: ";
    //        for (const auto& nexthop : nexthops) {
    //            cout << nexthop->GetId() << " ";
    //        }
    //        cout << endl;
    //    }
    //}
}

/**
 * @brief Set the Routing Entries object
 */
void SetRoutingEntries() {
    // For each node.
    for (auto i = nextHop.begin(); i != nextHop.end(); i++) {
        Ptr<Node> node = i->first;
        auto &table = i->second;
        for (auto j = table.begin(); j != table.end(); j++) {
            // The destination node.
            Ptr<Node> dst = j->first;
            // The IP address of the dst.
            Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
            // The next hops towards the dst.
            vector<Ptr<Node>> nexts = j->second;
            for (int k = 0; k < (int)nexts.size(); k++) {
                Ptr<Node> next = nexts[k];
                uint32_t interface = nbr2if[node][next].idx;
                if (node->GetNodeType() == 1)
                    DynamicCast<SwitchNode>(node)->AddTableEntry(dstAddr, interface);
                else {
                    node->GetObject<RdmaDriver>()->m_rdma->AddTableEntry(dstAddr, interface);
                }
            }
        }
    }
}
// for DV load balancing

void SetDVTables(){
    Time now = Simulator::Now();
    for (auto i = nextHop.begin(); i != nextHop.end(); i++) {
        Ptr<Node> node = i->first;
        auto &table = i->second;
        for (auto j = table.begin(); j != table.end(); j++) {
            // The destination node.
            Ptr<Node> dst = j->first;
            // The IP address of the dst.
            Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
            // The next hops towards the dst.
            vector<Ptr<Node>> nexts = j->second;
            for (int k = 0; k < (int)nexts.size(); k++) {
                Ptr<Node> next = nexts[k];
                uint32_t interface = nbr2if[node][next].idx;
                if (node->GetNodeType() == 1)
                    DynamicCast<SwitchNode>(node)->AddDVTableEntry(dstAddr, interface, now);
            }
        }
    }
}
// *******************************Add begin**********************//
void SetBestPathCETables(){
    Time now = Simulator::Now();
    for (auto i = nextHop.begin(); i != nextHop.end(); i++){
        Ptr<Node> node = i->first;
        auto &table = i->second;
        for (auto j = table.begin(); j != table.end(); j++){
            // The destination node.
            Ptr<Node> dst = j->first;
            // The IP address of the dst.
            Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
            if (node->GetNodeType() == 1){
                DynamicCast<SwitchNode>(node)->AddBestPathCETableEntry(dstAddr, now);
            }
        }
    }
}
void SetBestPathCETables_noshare(){
    Time now = Simulator::Now();
    for (auto i = nextHop.begin(); i != nextHop.end(); i++){
        Ptr<Node> node = i->first;
        auto &table = i->second;
        for (auto j = table.begin(); j != table.end(); j++){
            // The destination node.
            Ptr<Node> dst = j->first;
            // The IP address of the dst.
            Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
            if (node->GetNodeType() == 1){
                DynamicCast<SwitchNode>(node)->AddBestPathCETableEntry_noshare(dstAddr, now);
            }
        }
    }
}
void SetACCPathCETables(){
        Time now = Simulator::Now();
    for (auto i = nextHop.begin(); i != nextHop.end(); i++){
        Ptr<Node> node = i->first;
        if (node->GetNodeType() == 1){
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);
            if(sw->m_isToR == false){
                auto &table = i->second;
                for (auto j = table.begin(); j != table.end(); j++){
                    // The destination node.
                    Ptr<Node> dst = j->first;
                    // The IP address of the dst.
                    Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
                    if (node->GetNodeType() == 1){
                        sw->AddACCPathCETableEntry(dstAddr, now);
                    }
                }
            }
        }
    }
}
void SetACCPathCETables_noshare(){
        Time now = Simulator::Now();
    for (auto i = nextHop.begin(); i != nextHop.end(); i++){
        Ptr<Node> node = i->first;
        if (node->GetNodeType() == 1){
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);
            if(sw->m_isToR == false){
                auto &table = i->second;
                for (auto j = table.begin(); j != table.end(); j++){
                    // The destination node.
                    Ptr<Node> dst = j->first;
                    // The IP address of the dst.
                    Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
                    if (node->GetNodeType() == 1){
                        sw->AddACCPathCETableEntry_noshare(dstAddr, now);
                    }
                }
            }
        }
    }
}
void SetPathChoiceTables(){
    Time now = Simulator::Now();
    for (auto i = nextHop.begin(); i != nextHop.end(); i++){
        Ptr<Node> node = i->first;
        auto &table = i->second;
        for (auto j = table.begin(); j != table.end(); j++){
            // The destination node.
            Ptr<Node> dst = j->first;
            // The IP address of the dst.
            Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
            if (node->GetNodeType() == 1){
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);
                if(sw->m_isToR == true){
                    sw->AddPathChoiceTableEntry(dstAddr, now);
                }
            }
        }
    }
}
void SetPathChoiceTables_noshare(){
    Time now = Simulator::Now();
    for (auto i = nextHop.begin(); i != nextHop.end(); i++){
        Ptr<Node> node = i->first;
        auto &table = i->second;
        for (auto j = table.begin(); j != table.end(); j++){
            // The destination node.
            Ptr<Node> dst = j->first;
            // The IP address of the dst.
            Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
            if (node->GetNodeType() == 1){
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);
                if(sw->m_isToR == true){
                    sw->AddPathChoiceTableEntry_noshare(dstAddr, now);
                }
            }
        }
    }
}
// *******************************Add begin**********************//
void SetPathCETables(){
    Time now = Simulator::Now();
    for (auto i = nextHop.begin(); i != nextHop.end(); i++) {
        Ptr<Node> node = i->first;
        auto &table = i->second;
        for (auto j = table.begin(); j != table.end(); j++) {
            // The destination node.
            Ptr<Node> dst = j->first;
            // The IP address of the dst.
            Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
            if (node->GetNodeType() == 1)
                DynamicCast<SwitchNode>(node)->AddPathCETableEntry(dstAddr, now);
        }
    }
}
void SetPathCE_port_Tables(){
    Time now = Simulator::Now();
    for (auto i = nextHop.begin(); i != nextHop.end(); i++) {
        Ptr<Node> node = i->first;
        auto &table = i->second;
        for (auto j = table.begin(); j != table.end(); j++) {
            // The destination node.
            Ptr<Node> dst = j->first;
           // The IP address of the dst.
            Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
            // The next hops towards the dst.
            vector<Ptr<Node>> nexts = j->second;
            for (int k = 0; k < (int)nexts.size(); k++) {
                Ptr<Node> next = nexts[k];
                uint32_t interface = nbr2if[node][next].idx;
                if (node->GetNodeType() == 1)
                    DynamicCast<SwitchNode>(node)->AddPathCE_port_TableEntry(dstAddr, interface, now);
            }
        }
    }

}
// *******************************Add end**********************//
/**
 * @brief take down the link between a and b, and redo the routing
 */
void TakeDownLink(NodeContainer n, Ptr<Node> a, Ptr<Node> b) {
    if (!nbr2if[a][b].up) return;
    // take down link between a and b
    nbr2if[a][b].up = nbr2if[b][a].up = false;
    nextHop.clear();
    CalculateRoutes(n);
    // clear routing tables
    for (uint32_t i = 0; i < n.GetN(); i++) {
        if (n.Get(i)->GetNodeType() == 1)
            DynamicCast<SwitchNode>(n.Get(i))->ClearTable();
        else
            n.Get(i)->GetObject<RdmaDriver>()->m_rdma->ClearTable();
    }
    DynamicCast<QbbNetDevice>(a->GetDevice(nbr2if[a][b].idx))->TakeDown();
    DynamicCast<QbbNetDevice>(b->GetDevice(nbr2if[b][a].idx))->TakeDown();
    // reset routing table
    SetRoutingEntries();

    // redistribute qp on each host
    for (uint32_t i = 0; i < n.GetN(); i++) {
        if (n.Get(i)->GetNodeType() == 0)
            n.Get(i)->GetObject<RdmaDriver>()->m_rdma->RedistributeQp();
    }
}

uint64_t get_nic_rate(NodeContainer &n) {
    uint64_t avg_nic_rate;
    uint64_t n_servers = 0;
    for (uint32_t i = 0; i < n.GetN(); i++) {
        if (n.Get(i)->GetNodeType() == 0) {
            avg_nic_rate +=
                DynamicCast<QbbNetDevice>(n.Get(i)->GetDevice(1))->GetDataRate().GetBitRate();
            n_servers += 1;
        }
    }
    return avg_nic_rate / n_servers;
}

/************************************************************************/
//                                                                      //
//                                M A I N                               //
//                                                                      //
/************************************************************************/

int main(int argc, char *argv[]) {
    uint32_t *workload_cdf = nullptr;
    clock_t begint, endt;
    begint = clock();

//参数配置
#ifndef PGO_TRAINING
    if (argc > 1)
#else
    if (true)
#endif
    {
        // Read the configuration file
        std::ifstream conf;
#ifndef PGO_TRAINING
        conf.open(argv[1]);
#else
        conf.open(PATH_TO_PGO_CONFIG);
#endif
        while (!conf.eof()) {
            std::string key;
            conf >> key;
            if (key.compare("FLOW_INPUT_FILE") == 0) {
                std::string v;
                conf >> v;
                flow_input_file = v;
                std::cerr << "FLOW_INPUT_FILE\t\t\t" << flow_input_file << "\n";
            } else if (key.compare("CNP_OUTPUT_FILE") == 0) {
                std::string v;
                conf >> v;
                cnp_output_file = v;
                std::cerr << "CNP_OUTPUT_FILE\t\t\t" << cnp_output_file << "\n";
            } else if (key.compare("EST_ERROR_MON_FILE") == 0) {
                std::string v;
                conf >> v;
                est_error_output_file = v;
                std::cerr << "EST_ERROR_MON_FILE\t\t\t" << est_error_output_file << "\n";
            } else if (key.compare("LB_MODE") == 0) {
                uint32_t v;
                conf >> v;
                lb_mode = v;
                std::cerr << "LB_MODE\t\t\t" << lb_mode << "\n";
            } else if (key.compare("SW_MONITORING_INTERVAL") == 0) {
                uint32_t v;
                conf >> v;
                switch_mon_interval = v;
                std::cerr << "SW_MONITORING_INTERVAL\t\t\t" << switch_mon_interval << "\n";
            } else if (key.compare("CONWEAVE_TX_EXPIRY_TIME") == 0) {
                uint32_t v;
                conf >> v;
                conweave_txExpiryTime = Time(MicroSeconds(v));
                std::cerr << "CONWEAVE_TX_EXPIRY_TIME\t\t\t" << conweave_txExpiryTime << "\n";
            } else if (key.compare("CONWEAVE_REPLY_TIMEOUT_EXTRA") == 0) {
                uint32_t v;
                conf >> v;
                conweave_extraReplyDeadline = Time(MicroSeconds(v));
                std::cerr << "CONWEAVE_REPLY_TIMEOUT_EXTRA\t\t\t" << conweave_extraReplyDeadline
                          << "\n";
            } else if (key.compare("CONWEAVE_EXTRA_VOQ_FLUSH_TIME") == 0) {
                uint32_t v;
                conf >> v;
                conweave_extraVOQFlushTime = Time(MicroSeconds(v));
                std::cerr << "CONWEAVE_EXTRA_VOQ_FLUSH_TIME\t\t\t" << conweave_extraVOQFlushTime
                          << "\n";
            } else if (key.compare("CONWEAVE_PATH_PAUSE_TIME") == 0) {
                uint32_t v;
                conf >> v;
                conweave_pathPauseTime = Time(MicroSeconds(v));
                std::cerr << "CONWEAVE_PATH_PAUSE_TIME\t\t\t" << conweave_pathPauseTime << "\n";
            } else if (key.compare("CONWEAVE_DEFAULT_VOQ_WAITING_TIME") == 0) {
                uint32_t v;
                conf >> v;
                conweave_defaultVOQWaitingTime = Time(MicroSeconds(v));
                std::cerr << "CONWEAVE_DEFAULT_VOQ_WAITING_TIME\t\t\t"
                          << conweave_defaultVOQWaitingTime << "\n";
            } else if (key.compare("CAVER_DRETIME") == 0) {
                uint32_t v;
                conf >> v;
                caver_dreTime = Time(MicroSeconds(v));
                std::cerr << "CAVER_DRETIME\t\t\t" << caver_dreTime << "\n";
            } else if (key.compare("CAVER_ALPHA") == 0) {
                double v;
                conf >> v;
                caver_alpha = v;
                std::cerr << "CAVER_ALPHA\t\t\t" << caver_alpha << "\n";
            } else if (key.compare("CAVER_CE_THRESHOLD") == 0) {
                double v;
                conf >> v;
                caver_ce_threshold = v;
                std::cerr << "CAVER_CE_THRESHOLD\t\t\t" << caver_ce_threshold << "\n";
            } else if (key.compare("CAVER_PATCHOICETIMEOUT") == 0) {
                uint32_t v;
                conf >> v;
                caver_patchoiceTimeout = Time(MicroSeconds(v));
                std::cerr << "CAVER_PATCHOICETIMEOUT\t\t\t" << caver_patchoiceTimeout << "\n";
            } else if (key.compare("CAVER_PATHCHOICE_NUM") == 0) {
                uint32_t v;
                conf >> v;
                caver_pathChoice_num = v;
                std::cerr << "CAVER_PATHCHOICE_NUM\t\t\t" << caver_pathChoice_num << "\n";
            } else if (key.compare("CAVER_TAU") == 0) {
                uint32_t v;
                conf >> v;
                caver_tau = Time(MicroSeconds(v));
                std::cerr << "CAVER_TAU\t\t\t" << caver_tau << "\n";
            } else if (key.compare("CAVER_USE_EWMA") == 0) {
                bool v;
                conf >> v;
                caver_useEWMA = v;
                std::cerr << "CAVER_USE_EWMA\t\t\t" << caver_useEWMA << "\n";
            }else if (key.compare("ENABLE_PFC") == 0) {
                uint32_t v;
                conf >> v;
                enable_pfc = v;
                if (enable_pfc)
                    std::cerr << "ENABLE_PFC\t\t\t"
                              << "Yes"
                              << "\n";
                else
                    std::cerr << "ENABLE_PFC\t\t\t"
                              << "No"
                              << "\n";
            } else if (key.compare("ENABLE_QCN") == 0) {
                uint32_t v;
                conf >> v;
                enable_qcn = v;
                if (enable_qcn)
                    std::cerr << "ENABLE_QCN\t\t\t"
                              << "Yes"
                              << "\n";
                else
                    std::cerr << "ENABLE_QCN\t\t\t"
                              << "No"
                              << "\n";
            } else if (key.compare("USE_DYNAMIC_PFC_THRESHOLD") == 0) {
                uint32_t v;
                conf >> v;
                use_dynamic_pfc_threshold = v;
                if (use_dynamic_pfc_threshold)
                    std::cerr << "USE_DYNAMIC_PFC_THRESHOLD\t"
                              << "Yes"
                              << "\n";
                else
                    std::cerr << "USE_DYNAMIC_PFC_THRESHOLD\t"
                              << "No"
                              << "\n";
            } else if (key.compare("CLAMP_TARGET_RATE") == 0) {
                uint32_t v;
                conf >> v;
                clamp_target_rate = v;
                if (clamp_target_rate)
                    std::cerr << "CLAMP_TARGET_RATE\t\t"
                              << "Yes"
                              << "\n";
                else
                    std::cerr << "CLAMP_TARGET_RATE\t\t"
                              << "No"
                              << "\n";
            } else if (key.compare("PAUSE_TIME") == 0) {
                double v;
                conf >> v;
                pause_time = v;
                std::cerr << "PAUSE_TIME\t\t\t" << pause_time << "\n";
            } else if (key.compare("DATA_RATE") == 0) {
                std::string v;
                conf >> v;
                data_rate = v;
                std::cerr << "DATA_RATE\t\t\t" << data_rate << "\n";
            } else if (key.compare("LINK_DELAY") == 0) {
                std::string v;
                conf >> v;
                link_delay = v;
                std::cerr << "LINK_DELAY\t\t\t" << link_delay << "\n";
            } else if (key.compare("PACKET_PAYLOAD_SIZE") == 0) {
                uint32_t v;
                conf >> v;
                packet_payload_size = v;
                std::cerr << "PACKET_PAYLOAD_SIZE\t\t" << packet_payload_size << "\n";
            } else if (key.compare("L2_CHUNK_SIZE") == 0) {
                uint32_t v;
                conf >> v;
                l2_chunk_size = v;
                std::cerr << "L2_CHUNK_SIZE\t\t\t" << l2_chunk_size << "\n";
            } else if (key.compare("L2_ACK_INTERVAL") == 0) {
                uint32_t v;
                conf >> v;
                l2_ack_interval = v;
                std::cerr << "L2_ACK_INTERVAL\t\t\t" << l2_ack_interval << "\n";
            } else if (key.compare("L2_BACK_TO_ZERO") == 0) {
                uint32_t v;
                conf >> v;
                l2_back_to_zero = v;
                if (l2_back_to_zero)
                    std::cerr << "L2_BACK_TO_ZERO\t\t\t"
                              << "Yes"
                              << "\n";
                else
                    std::cerr << "L2_BACK_TO_ZERO\t\t\t"
                              << "No"
                              << "\n";
            } else if (key.compare("TOPOLOGY_FILE") == 0) {
                std::string v;
                conf >> v;
                topology_file = v;
                std::cerr << "TOPOLOGY_FILE\t\t\t" << topology_file << "\n";
            } else if (key.compare("FLOW_FILE") == 0) {
                std::string v;
                conf >> v;
                flow_file = v;
                std::cerr << "FLOW_FILE\t\t\t" << flow_file << "\n";
            } else if (key.compare("FLOWGEN_START_TIME") == 0) {
                double v;
                conf >> v;
                flowgen_start_time = v;
                qlen_mon_start = v;
                qlen_mon_end = v;
                cnp_mon_start = v;
                irn_mon_start = v;
                std::cerr << "FLOWGEN_START_TIME\t\t" << flowgen_start_time << "\n";
            } else if (key.compare("FLOWGEN_STOP_TIME") == 0) {
                double v;
                conf >> v;
                flowgen_stop_time = v;
                std::cerr << "FLOWGEN_STOP_TIME\t\t" << flowgen_stop_time << "\n";
            } else if (key.compare("ALPHA_RESUME_INTERVAL") == 0) {
                double v;
                conf >> v;
                alpha_resume_interval = v;
                std::cerr << "ALPHA_RESUME_INTERVAL\t\t" << alpha_resume_interval << "\n";
            } else if (key.compare("RP_TIMER") == 0) {
                double v;
                conf >> v;
                rp_timer = v;
                std::cerr << "RP_TIMER\t\t\t" << rp_timer << "\n";
            } else if (key.compare("EWMA_GAIN") == 0) {
                double v;
                conf >> v;
                ewma_gain = v;
                std::cerr << "EWMA_GAIN\t\t\t" << ewma_gain << "\n";
            } else if (key.compare("FAST_RECOVERY_TIMES") == 0) {
                uint32_t v;
                conf >> v;
                fast_recovery_times = v;
                std::cerr << "FAST_RECOVERY_TIMES\t\t" << fast_recovery_times << "\n";
            } else if (key.compare("RATE_AI") == 0) {
                std::string v;
                conf >> v;
                rate_ai = v;
                std::cerr << "RATE_AI\t\t\t\t" << rate_ai << "\n";
            } else if (key.compare("RATE_HAI") == 0) {
                std::string v;
                conf >> v;
                rate_hai = v;
                std::cerr << "RATE_HAI\t\t\t" << rate_hai << "\n";
            } else if (key.compare("ERROR_RATE_PER_LINK") == 0) {
                double v;
                conf >> v;
                error_rate_per_link = v;
                std::cerr << "ERROR_RATE_PER_LINK\t\t" << error_rate_per_link << "\n";
            } else if (key.compare("CC_MODE") == 0) {
                conf >> cc_mode;
                std::cerr << "CC_MODE\t\t" << cc_mode << '\n';
            } else if (key.compare("RATE_DECREASE_INTERVAL") == 0) {
                double v;
                conf >> v;
                rate_decrease_interval = v;
                std::cerr << "RATE_DECREASE_INTERVAL\t\t" << rate_decrease_interval << "\n";
            } else if (key.compare("MIN_RATE") == 0) {
                conf >> min_rate;
                std::cerr << "MIN_RATE\t\t" << min_rate << "\n";
            } else if (key.compare("FCT_OUTPUT_FILE") == 0) {
                conf >> fct_output_file;
                std::cerr << "FCT_OUTPUT_FILE\t\t" << fct_output_file << '\n';
            } else if (key.compare("PACKET_HEADER_FILE") == 0) {
                conf >> m_packetHeaderFile;
                std::cerr << "PACKET_HEADER_FILE\t\t" << m_packetHeaderFile << '\n';
            } else if (key.compare("HAS_WIN") == 0) {
                conf >> has_win;
                std::cerr << "HAS_WIN\t\t" << has_win << "\n";
            } else if (key.compare("GLOBAL_T") == 0) {
                conf >> global_t;
                std::cerr << "GLOBAL_T\t\t" << global_t << '\n';
            } else if (key.compare("MI_THRESH") == 0) {
                conf >> mi_thresh;
                std::cerr << "MI_THRESH\t\t" << mi_thresh << '\n';
            } else if (key.compare("VAR_WIN") == 0) {
                uint32_t v;
                conf >> v;
                var_win = v;
                std::cerr << "VAR_WIN\t\t" << v << '\n';
            } else if (key.compare("FAST_REACT") == 0) {
                uint32_t v;
                conf >> v;
                fast_react = v;
                std::cerr << "FAST_REACT\t\t" << v << '\n';
            } else if (key.compare("U_TARGET") == 0) {
                conf >> u_target;
                std::cerr << "U_TARGET\t\t" << u_target << '\n';
            } else if (key.compare("INT_MULTI") == 0) {
                conf >> int_multi;
                std::cerr << "INT_MULTI\t\t\t\t" << int_multi << '\n';
            } else if (key.compare("RATE_BOUND") == 0) {
                uint32_t v;
                conf >> v;
                rate_bound = v;
                std::cerr << "RATE_BOUND\t\t" << rate_bound << '\n';
            } else if (key.compare("DCTCP_RATE_AI") == 0) {
                conf >> dctcp_rate_ai;
                std::cerr << "DCTCP_RATE_AI\t\t\t\t" << dctcp_rate_ai << "\n";
            } else if (key.compare("PFC_OUTPUT_FILE") == 0) {
                conf >> pfc_output_file;
                std::cerr << "PFC_OUTPUT_FILE\t\t\t\t" << pfc_output_file << '\n';
            } else if (key.compare("LINK_DOWN") == 0) {
                conf >> link_down_time >> link_down_A >> link_down_B;
                std::cerr << "LINK_DOWN\t\t\t\t" << link_down_time << ' ' << link_down_A << ' '
                          << link_down_B << '\n';
            } else if (key.compare("KMAX_MAP") == 0) {
                int n_k;
                conf >> n_k;
                std::cerr << "KMAX_MAP\t\t\t\t";
                for (int i = 0; i < n_k; i++) {
                    uint64_t rate;
                    uint32_t k;
                    conf >> rate >> k;
                    rate2kmax[rate] = k;
                    std::cerr << ' ' << rate << ' ' << k;
                }
                std::cerr << '\n';
            } else if (key.compare("KMIN_MAP") == 0) {
                int n_k;
                conf >> n_k;
                std::cerr << "KMIN_MAP\t\t\t\t";
                for (int i = 0; i < n_k; i++) {
                    uint64_t rate;
                    uint32_t k;
                    conf >> rate >> k;
                    rate2kmin[rate] = k;
                    std::cerr << ' ' << rate << ' ' << k;
                }
                std::cerr << '\n';
            } else if (key.compare("PMAX_MAP") == 0) {
                int n_k;
                conf >> n_k;
                std::cerr << "PMAX_MAP\t\t\t\t";
                for (int i = 0; i < n_k; i++) {
                    uint64_t rate;
                    double p;
                    conf >> rate >> p;
                    rate2pmax[rate] = p;
                    std::cerr << ' ' << rate << ' ' << p;
                }
                std::cerr << '\n';
            } else if (key.compare("BUFFER_SIZE") == 0) {
                conf >> buffer_size;
                std::cerr << "BUFFER_SIZE\t\t\t\t" << buffer_size << '\n';
            } else if (key.compare("QLEN_MON_FILE") == 0) {
                conf >> qlen_mon_file;
                std::cerr << "QLEN_MON_FILE\t\t\t\t" << qlen_mon_file << '\n';
            } else if (key.compare("VOQ_MON_FILE") == 0) {
                conf >> voq_mon_file;
                std::cerr << "VOQ_MON_FILE\t\t\t\t" << voq_mon_file << '\n';
            } else if (key.compare("VOQ_MON_DETAIL_FILE") == 0) {
                conf >> voq_mon_detail_file;
                std::cerr << "VOQ_MON_DETAIL_FILE\t\t\t\t" << voq_mon_detail_file << '\n';
            } else if (key.compare("UPLINK_MON_FILE") == 0) {
                conf >> uplink_mon_file;
                std::cerr << "UPLINK_MON_FILE\t\t\t\t" << uplink_mon_file << '\n';
            } else if (key.compare("FLOW_MON_FILE") == 0){
                conf >> flow_mon_file;
                std::cerr << "FLOW_MON_FILE\t\t\t\t" << flow_mon_file << '\n';
            } else if(key.compare("BPS_MON_FILE") == 0)
            {
                conf >> bps_mon_file;
                std::cerr << "BPS_MON_FILE\t\t\t\t" << bps_mon_file << '\n';
            } else if (key.compare("PathCE_MON_FILE") == 0)
            {
                conf >> Settings::pathCE_mon_file;
                std::cerr << "PathCE_MON_FILE\t\t\t\t" << Settings::pathCE_mon_file << '\n';
            } else if (key.compare("PathCE_Exclude_last_hop_FILE") == 0)
            {
                conf >> Settings::pathCE_exclude_lasthop_mon_file;
                std::cerr << "PathCE_Exclude_last_hop_FILE\t\t\t\t" << Settings::pathCE_exclude_lasthop_mon_file << '\n';
            } else if (key.compare("Global_CE_Map_FILE") == 0)
            {
                conf >> global_CE_map_mon_file;
                std::cerr << "Global_CE_Map_FILE\t\t\t\t" << global_CE_map_mon_file << '\n';
            } else if (key.compare("ALL_link_tx_FILE") == 0)
            {
                conf >> all_links_mon_file;
                std::cerr << "ALL_link_tx_FILE\t\t\t\t" << all_links_mon_file << '\n';
            }
            else if (key.compare("DOWNLINK_MON_FILE") == 0) {
                conf >> downlink_mon_file;
                std::cerr << "DOWNLINK_MON_FILE\t\t\t\t" << downlink_mon_file << '\n';
            } else if (key.compare("UPLINK_RX_MON_FILE") == 0){
                conf >> uplink_rx_mon_file;
                std::cerr << "UPLINK_RX_MON_FILE\t\t\t\t" << uplink_rx_mon_file << '\n';
            } else if (key.compare("DOWNLINK_RX_MON_FILE") == 0){
                conf >> downlink_rx_mon_file;
                std::cerr << "DOWNLINK_RX_MON_FILE\t\t\t\t" << downlink_rx_mon_file << '\n';
            } else if (key.compare("CONN_MON_FILE") == 0) {
                conf >> conn_mon_file;
                std::cerr << "CONN_MON_FILE\t\t\t\t" << conn_mon_file << '\n';
            } else if (key.compare("QLEN_MON_START") == 0) {
                conf >> qlen_mon_start;
                std::cerr << "QLEN_MON_START\t\t\t\t" << qlen_mon_start << '\n';
            } else if (key.compare("QLEN_MON_END") == 0) {
                conf >> qlen_mon_end;
                std::cerr << "QLEN_MON_END\t\t\t\t" << qlen_mon_end << '\n';
            } else if (key.compare("MULTI_RATE") == 0) {
                int v;
                conf >> v;
                multi_rate = v;
                std::cerr << "MULTI_RATE\t\t\t\t" << multi_rate << '\n';
            } else if (key.compare("SAMPLE_FEEDBACK") == 0) {
                int v;
                conf >> v;
                sample_feedback = v;
                std::cerr << "SAMPLE_FEEDBACK\t\t\t\t" << sample_feedback << '\n';
            } else if (key.compare("LOAD") == 0) {
                double v;
                conf >> v;
                load = v;
                std::cerr << "LOAD\t\t\t" << load << "\n";
            } else if (key.compare("ENABLE_IRN") == 0) {
                bool v;
                conf >> v;
                enable_irn = v;
                std::cerr << "ENABLE_IRN\t\t" << enable_irn << "\n";
            } else if (key.compare("RANDOM_SEED") == 0) {
                int v;
                conf >> v;
                random_seed = v;
                std::cerr << "RANDOM_SEED\t\t\t" << random_seed << "\n";
            }

            fflush(stdout);
        }
        conf.close();

    } else {
        std::cerr << "Error: require a config file\n";
        fflush(stdout);
        return 1;
    }

    /******************* READING CONFIG FILE IS DONE ***********************/

    /**
     * Activate ns3 logging
     */
    LogComponentEnable("GENERIC_SIMULATION", LOG_LEVEL_DEBUG);

    /**
     * @brief Random seed setup
     */
    NS_LOG_INFO("Initialize random seed: " << random_seed);
    srand((unsigned)random_seed);
    SeedManager::SetSeed(random_seed);

    /**
     * @brief PFC/QCN setup
     */
    bool dynamicth = use_dynamic_pfc_threshold;
    Config::SetDefault("ns3::QbbNetDevice::PauseTime", UintegerValue(pause_time));
    Config::SetDefault("ns3::QbbNetDevice::QcnEnabled", BooleanValue(enable_qcn));
    Config::SetDefault("ns3::QbbNetDevice::DynamicThreshold", BooleanValue(dynamicth));
    Config::SetDefault("ns3::QbbNetDevice::QbbEnabled", BooleanValue(enable_pfc));

    if (cc_mode != 1 && lb_mode == 9) {
        std::cout << "Currently, ConWeave supports only DCQCN congestion control for RDMA. \nIf "
                     "you want to extend, the reordering delay at DstTor must be considered."
                  << std::endl;
        exit(1);
    }

    /**
     * @brief INT header setup
     */
    IntHop::multi = int_multi;
    // IntHeader::mode
    if (cc_mode == 7)  // timely, use ts
        IntHeader::mode = 1;
    else if (cc_mode == 3)  // hpcc, use int
        IntHeader::mode = 0;
    else  // others, no extra header
        IntHeader::mode = 5;

    /**
     * @brief open topology config, input-flows config.
     */
    topof.open(topology_file.c_str());
    flowf.open(flow_file.c_str());
    uint32_t node_num, switch_num, link_num;
    topof >> node_num >> switch_num >> link_num;
    flowf >> flow_num;

    /*-------Parameter of Settings-------*/
    Settings::node_num = node_num;
    Settings::host_num = node_num - switch_num;
    Settings::switch_num = switch_num;
    Settings::lb_mode = lb_mode;
    Settings::packet_payload = packet_payload_size;
    // Settings::MTU = packet_payload_size + 48;  // for simplicity
    /*------------------------------------*/

    //创建节点
    std::vector<uint32_t> node_type(node_num, 0);
    for (uint32_t i = 0; i < switch_num; i++) {
        uint32_t sid;
        topof >> sid;
        node_type[sid] = 1;
    }//switch is 1, server is 0
    for (uint32_t i = 0; i < node_num; i++) {
        if (node_type[i] == 0)
            n.Add(CreateObject<Node>());
        else {
            Ptr<SwitchNode> sw = CreateObject<SwitchNode>();
            n.Add(sw);
            sw->SetAttribute("EcnEnabled", BooleanValue(enable_qcn));
            //TODO: my code to send the packet head file writer
            FILE* m_packetHeader_output = fopen(m_packetHeaderFile.c_str(), "w");
            sw->setFilePointer(m_packetHeader_output);
        }
    }
    NS_LOG_INFO("Create nodes.");

    /*----------------------------------------*/

    InternetStackHelper internet;
    internet.Install(n);  // aggregate ipv4, ipv6, udp, tcp, etc

    //
    // Assign IP to each server
    //
    for (uint32_t i = 0; i < node_num; i++) {
        if (n.Get(i)->GetNodeType() == 0) {  // is server
            serverAddress.resize(i + 1);
            serverAddress[i] = Settings::node_id_to_ip(i); 
        }
    }
    //node_id_to_ip just return an ipv4 address based on the node id without change any other thing
    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the channels required by the topology.
    //
    Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    rem->SetRandomVariable(uv);
    uv->SetStream(50);
    rem->SetAttribute("ErrorRate", DoubleValue(error_rate_per_link));
    rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

    pfc_file = fopen(pfc_output_file.c_str(), "w");

    QbbHelper qbb;
    Ipv4AddressHelper ipv4;
    std::vector<std::pair<uint32_t, uint32_t>> link_pairs;  // src, dst link pairs
    for (uint32_t i = 0; i < link_num; i++) {
        uint32_t src, dst;
        std::string data_rate, link_delay;
        double error_rate;
        topof >> src >> dst >> data_rate >> link_delay >> error_rate;

        //std::cout << "link_delay: " << link_delay << std::endl;
        /** ASSUME: fixed one-hop delay across network */
        assert(std::to_string(one_hop_delay) + "ns" == link_delay);

        link_pairs.push_back(std::make_pair(src, dst));
        Ptr<Node> snode = n.Get(src), dnode = n.Get(dst);

        qbb.SetDeviceAttribute("DataRate", StringValue(data_rate));
        qbb.SetChannelAttribute("Delay", StringValue(link_delay));

        if (error_rate > 0) {
            Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
            Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
            rem->SetRandomVariable(uv);
            uv->SetStream(50);
            rem->SetAttribute("ErrorRate", DoubleValue(error_rate));
            rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
            qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
        } else {
            qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
        }

        fflush(stdout);

        // Assigne server IP
        // Note: this should be before the automatic assignment below (ipv4.Assign(d)),
        // because we want our IP to be the primary IP (first in the IP address list),
        // so that the global routing is based on our IP

        // Adding network devices to a link
        NetDeviceContainer d = qbb.Install(snode, dnode);
        if (snode->GetNodeType() == 0) {
            Ptr<Ipv4> ipv4 = snode->GetObject<Ipv4>();
            ipv4->AddInterface(d.Get(0));
            ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[src], Ipv4Mask(0xff000000)));
        }
        if (dnode->GetNodeType() == 0) {
            Ptr<Ipv4> ipv4 = dnode->GetObject<Ipv4>();
            ipv4->AddInterface(d.Get(1));
            ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[dst], Ipv4Mask(0xff000000)));
        }
        
        // used to create a graph of the topology
        nbr2if[snode][dnode].idx = DynamicCast<QbbNetDevice>(d.Get(0))->GetIfIndex();
        nbr2if[snode][dnode].up = true;
        nbr2if[snode][dnode].delay =
            DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(0))->GetChannel())
                ->GetDelay()
                .GetTimeStep();
        nbr2if[snode][dnode].bw = DynamicCast<QbbNetDevice>(d.Get(0))->GetDataRate().GetBitRate();
        nbr2if[dnode][snode].idx = DynamicCast<QbbNetDevice>(d.Get(1))->GetIfIndex();
        nbr2if[dnode][snode].up = true;
        nbr2if[dnode][snode].delay =
            DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(1))->GetChannel())
                ->GetDelay()
                .GetTimeStep();
        nbr2if[dnode][snode].bw = DynamicCast<QbbNetDevice>(d.Get(1))->GetDataRate().GetBitRate();
        if2id[snode][nbr2if[snode][dnode].idx] = dnode->GetId();
        if2id[dnode][nbr2if[dnode][snode].idx] = snode->GetId();
        //std::cout << "link: " << src << "->" << dst << " interface: " << "id: " << nbr2if[snode][dnode].idx <<  " src: " << nbr2if[snode][dnode].idx << " dst: " << nbr2if[dnode][snode].idx << endl;
        // This is just to set up the connectivity between nodes. The IP addresses are useless
        char ipstring[16];
        Ipv4Address x;
        snprintf(ipstring, sizeof(ipstring), "10.%d.%d.0", i / 254 + 1, i % 254 + 1);
        ipv4.SetBase(ipstring, "255.255.255.0");
        ipv4.Assign(d);
    
        // setup PFC trace
        DynamicCast<QbbNetDevice>(d.Get(0))->TraceConnectWithoutContext(
            "QbbPfc", MakeBoundCallback(&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(0))));
        DynamicCast<QbbNetDevice>(d.Get(1))->TraceConnectWithoutContext(
            "QbbPfc", MakeBoundCallback(&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(1))));
    }

    std::cout << "(AVG) NIC RATE: " << get_nic_rate(n) << std::endl;

    /* Get IP address <-> NodeID pairs */
    Ipv4Address empty_ip;
    for (uint32_t i = 0; i < node_num; ++i) {
        if (n.Get(i)->GetNodeType() == 0) {  // is server
            if (serverAddress[i].IsEqual(empty_ip)) {
                printf("XXX ERROR %d\n", i);
                printf("size of serverAddress: %lu", serverAddress.size());
                NS_FATAL_ERROR("An end-host belongs to no link");
            }
        }
        Settings::hostId2IpMap[i] = serverAddress[i].Get();
        Settings::hostIp2IdMap[serverAddress[i].Get()] = i;
        //std::cout << "IP Address before SetBase: " << serverAddress[i].Get() << std::endl;
    }
    //TODO: this hostIp2IdMap maybe needs to change but i am not sure whether this will effect anything?
    // config switch
    for (uint32_t i = 0; i < node_num; i++) {
        if (n.Get(i)->GetNodeType() == 1) {  // is switch
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
            uint32_t shift = 3;  // by default 1/8
            for (uint32_t j = 1; j < sw->GetNDevices(); j++) {
                Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(sw->GetDevice(j));
                // set ecn
                uint64_t rate = dev->GetDataRate().GetBitRate();
                NS_ASSERT_MSG(rate2kmin.find(rate) != rate2kmin.end(),
                              "must set kmin for each link speed");
                NS_ASSERT_MSG(rate2kmax.find(rate) != rate2kmax.end(),
                              "must set kmax for each link speed");
                NS_ASSERT_MSG(rate2pmax.find(rate) != rate2pmax.end(),
                              "must set pmax for each link speed");
                assert(rate2kmin.find(rate) != rate2kmin.end() &&
                       rate2kmax.find(rate) != rate2kmax.end() &&
                       rate2pmax.find(rate) != rate2pmax.end());
                sw->m_mmu->ConfigEcn(j, rate2kmin[rate], rate2kmax[rate], rate2pmax[rate]);
                // set pfc
                uint64_t delay =
                    DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep();
                uint32_t headroom = rate * delay / 8 / 1000000000 * 2 + 2 * sw->m_mmu->MTU;
                sw->m_mmu->ConfigHdrm(j, headroom);
            }
            sw->m_mmu->ConfigNPort(sw->GetNDevices() - 1);
            sw->m_mmu->ConfigBufferSize(buffer_size * 1024 *
                                        1024);  // default 0, specify in run.py!!
            sw->m_mmu->node_id = sw->GetId();
            NS_LOG_INFO("Node %u : Broadcom switch (%u ports / %gMB MMU)\n" %
                        (i, sw->GetNDevices() - 1, sw->m_mmu->GetMmuBufferBytes() / 1000000.));
        }
    }

    fct_output = fopen(fct_output_file.c_str(), "w");
    flow_input_stream = fopen(flow_input_file.c_str(), "w");
    if (cc_mode == 1) {
        cnp_output = fopen(cnp_output_file.c_str(), "w");
    }

    /**
     * @brief install RDMA driver (Mellanox parameters)
     *
     * [ClampTargetRate]clamp_tgt_rate (false) - when receiving a CNP, the target rate is always
     *updated to be the current rate
     *[-]clamp_tgt_rate_after_time_inc (true) - when receiving a CNP, the target rate is updated to
     *be the current rate also if the last rate increase event was due to the timer, and not only
     *due to the byte counter
     * [-]initial_alpha_value(1023) -
     * [RateDecreaseInterval]rate_reduce_monitor_period(4) - Minimal interval for rate reduction for
     *a flow. If a CNP is received during the interval, the flow rate is reduced at the beginning of
     *the next rate_reduce_monitor_period interval to (1-Alpha/Gd)*CurrentRate. rpg_gd is given as
     *log2(Gd), where Gd may only be powers of 2.
     * [-]rpg_gd(11) - If an CNP is received, the flow rate is reduced at the beginning of the next
     *rate_reduce_monitor_period interval to (1-Alpha/Gd)*CurrentRate.
     * -> in this simulator, (alpha / gd) ~ 0.5 setup, initially. We do not need rpg_gd parameter.
     * [RateOnFirstCnp]rate_to_set_on_first_cnp(0) - The rate that is set for the flow, upon first
     *CNP received, in Mbps. [RPTimer]rpg_time_reset(300us) - Time counter for rate increase event
     *[FastRecoveryTimes]rpg_threshold(1) - Number of rate increase events for switching between
     *Fast Recovery, Active Increase, Hyper Active Increase modes.
     * [AlphaResumInterval]dce_tcp_rtt(1) - Window for sampling of moving average calculation of
     *alpha
     * [-]dce_tcp_g(1019) - Weight of the new sampling in moving average calculation of alpha
     * [-]rpg_byte_reset(32767) - Byte counter for rate increase event
     * [-]rpg_min_dec_fac(50) -  Maximal factor by which the rate can be reduced (2 means that the
     *new rate can be divided by 2 at maximum)
     */
 
    // manually type BDP
    std::map<std::string, uint32_t> topo2bdpMap;
    topo2bdpMap[std::string("leaf_spine_128_100G_OS2")] = 104000;  // RTT=8320
    topo2bdpMap[std::string("fat_k4_100G_OS2")] = 156000;
    topo2bdpMap[std::string("fat_k8_100G_OS2")] = 156000;  
    topo2bdpMap[std::string("fat_k8_100G_OS1")] = 156000;  
    topo2bdpMap[std::string("fat_k8_100G_bond_OS2")] = 156000;     // RTT=12480 --> all 100G links
    topo2bdpMap[std::string("fat_k8_100G_bond_OS1")] = 156000;
    topo2bdpMap[std::string("fat_k4_100G_OS1")] = 156000;
    topo2bdpMap[std::string("fat_k16_100G_OS1")] = 156000;
    topo2bdpMap[std::string("leaf_spine_k_4_bond_2_OS1")] = 104000;        // RTT=3120
    topo2bdpMap[std::string("leaf_spine_k_6_bond_2_OS1")] = 104000; 
    topo2bdpMap[std::string("leaf_spine_k_8_bond_2_OS1")] = 104000; 
    topo2bdpMap[std::string("leaf_spine_k_10_bond_2_OS1")] = 104000; 
    topo2bdpMap[std::string("leaf_spine_k_12_bond_2_OS1")] = 104000; 
    topo2bdpMap[std::string("leaf_spine_k_14_bond_2_OS1")] = 104000; 
    topo2bdpMap[std::string("leaf_spine_k_16_bond_2_OS1")] = 104000; 
    topo2bdpMap[std::string("leaf_spine_k_18_bond_2_OS1")] = 104000; 

    topo2bdpMap[std::string("leaf_spine_k_4_bond_2_CLOS_3_OS1")] = 156000;
    topo2bdpMap[std::string("leaf_spine_k_4_bond_2_CLOS_3_OS1")] = 156000;
    topo2bdpMap[std::string("Fabric_x_4_k_4_OS1")] = 156000;
    topo2bdpMap[std::string("fat_k_4_OS1")] = 156000;
    topo2bdpMap[std::string("fat_k_4_nobond_OS1")] = 156000;
    topo2bdpMap[std::string("Congestion_OS1")] = 104000; 

    // topology_file
    bool found_topo2bdpMap = false;
    uint32_t irn_bdp_lookup = 0;
    for (auto pair : topo2bdpMap) {
        if (topology_file.find(pair.first) !=
            std::string::npos) {  // if topology file string includes the word
            irn_bdp_lookup = pair.second;
            found_topo2bdpMap = true;
            break;
        }
    }
    if (found_topo2bdpMap == false) {
        std::cout << __FILE__ << "(" << __LINE__ << ")"
                  << " ERROR - topo2bdpMap has no matched item with " << topology_file << std::endl;
        assert(false);
    }

    // rdmaHw config
    for (uint32_t i = 0; i < node_num; i++) {
        if (n.Get(i)->GetNodeType() == 0) {  // is server
            // create RdmaHw
            Ptr<RdmaHw> rdmaHw = CreateObject<RdmaHw>();
            rdmaHw->SetAttribute("ClampTargetRate", BooleanValue(clamp_target_rate));
            rdmaHw->SetAttribute("AlphaResumInterval", DoubleValue(alpha_resume_interval));
            rdmaHw->SetAttribute("RPTimer", DoubleValue(rp_timer));
            rdmaHw->SetAttribute("FastRecoveryTimes", UintegerValue(fast_recovery_times));
            rdmaHw->SetAttribute("EwmaGain", DoubleValue(ewma_gain));
            rdmaHw->SetAttribute("RateAI", DataRateValue(DataRate(rate_ai)));
            rdmaHw->SetAttribute("RateHAI", DataRateValue(DataRate(rate_hai)));
            rdmaHw->SetAttribute("L2BackToZero", BooleanValue(l2_back_to_zero));
            rdmaHw->SetAttribute("L2ChunkSize", UintegerValue(l2_chunk_size));
            rdmaHw->SetAttribute("L2AckInterval", UintegerValue(l2_ack_interval));
            rdmaHw->SetAttribute("CcMode", UintegerValue(cc_mode));
            rdmaHw->SetAttribute("RateDecreaseInterval", DoubleValue(rate_decrease_interval));
            rdmaHw->SetAttribute("MinRate", DataRateValue(DataRate(min_rate)));
            rdmaHw->SetAttribute("Mtu", UintegerValue(packet_payload_size));
            rdmaHw->SetAttribute("MiThresh", UintegerValue(mi_thresh));
            rdmaHw->SetAttribute("VarWin", BooleanValue(var_win));
            rdmaHw->SetAttribute("FastReact", BooleanValue(fast_react));
            rdmaHw->SetAttribute("MultiRate", BooleanValue(multi_rate));
            rdmaHw->SetAttribute("SampleFeedback", BooleanValue(sample_feedback));
            rdmaHw->SetAttribute("TargetUtil", DoubleValue(u_target));
            rdmaHw->SetAttribute("RateBound", BooleanValue(rate_bound));
            rdmaHw->SetAttribute("DctcpRateAI", DataRateValue(DataRate(dctcp_rate_ai)));
            rdmaHw->SetAttribute("IrnEnable", BooleanValue(enable_irn));
            // topo2bdpMap (e.g., longest BDP 25000: 8us * 25Gbps)
            rdmaHw->SetAttribute("IrnRtoHigh", TimeValue(MicroSeconds(320)));  // 1930
            rdmaHw->SetAttribute("IrnRtoLow", TimeValue(MicroSeconds(100)));   // 454
            rdmaHw->SetAttribute("IrnBdp", UintegerValue(irn_bdp_lookup));
            // Monitoring CNP Marking frequency of DCQCN
            if (cc_mode == 1) {
                Simulator::Schedule(NanoSeconds(cnp_mon_start), &cnp_freq_monitoring, cnp_output,
                                    rdmaHw);
            }

            // create and install RdmaDriver
            Ptr<RdmaDriver> rdma = CreateObject<RdmaDriver>();
            Ptr<Node> node = n.Get(i);
            rdma->SetNode(node);
            rdma->SetRdmaHw(rdmaHw);

            node->AggregateObject(rdma);
            rdma->Init();
            rdma->TraceConnectWithoutContext("QpComplete",
                                             MakeBoundCallback(qp_finish, fct_output));
        }
    }

    /**
     * @brief setup switch's CcMode and ACK with high priority
     */
    for (uint32_t i = 0; i < node_num; i++) {
        if (n.Get(i)->GetNodeType() == 1) {  // switch
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
            sw->SetAttribute("CcMode", UintegerValue(cc_mode));
            sw->SetAttribute("AckHighPrio", UintegerValue(1));
        }
    }

    /**
     * @brief setup routing
     */
    CalculateRoutes(n);
    SetRoutingEntries();
    // init DV tables
    if (lb_mode == 10) 
    {
        // *******************************Delete end**********************//
        // SetDVTables();
        // *******************************Delete end**********************//
        // *******************************Add begin**********************//
        SetPathCETables();
        SetPathCE_port_Tables();
        // *******************************Add end**********************//
        //更新每个交换机的接口与邻居id的关系
        for (const auto& outerPair : nbr2if) {
            ns3::Ptr<ns3::Node> SrcNode = outerPair.first;
            const auto& innerMap = outerPair.second;

            if (SrcNode->GetNodeType() == 1) {
                // 使用范围 for 循环遍历内层 map 中的每个键值对
                for (const auto& innerPair : innerMap) {
                    ns3::Ptr<ns3::Node> DstNode = innerPair.first;
                    uint32_t Dstid = DstNode->GetId();
                    ns3::Ptr<ns3::SwitchNode> Srcsw = DynamicCast<SwitchNode>(n.Get(SrcNode->GetId()));
                    Interface interface = innerPair.second;
                    uint32_t port = interface.idx;
                    Srcsw->m_mmu->m_dvRouting.id2Port[Dstid] = port;
                }
            }
        }
    }
    
    if (lb_mode == 20){
        //更新每个交换机的接口与邻居id的关系
        for (const auto& outerPair : nbr2if) {
            ns3::Ptr<ns3::Node> SrcNode = outerPair.first;
            const auto& innerMap = outerPair.second;

            if (SrcNode->GetNodeType() == 1) {
                // 使用范围 for 循环遍历内层 map 中的每个键值对
                for (const auto& innerPair : innerMap) {
                    ns3::Ptr<ns3::Node> DstNode = innerPair.first;
                    uint32_t Dstid = DstNode->GetId();
                    ns3::Ptr<ns3::SwitchNode> Srcsw = DynamicCast<SwitchNode>(n.Get(SrcNode->GetId()));
                    Interface interface = innerPair.second;
                    uint32_t port = interface.idx;
                    Srcsw->m_mmu->m_caverRouting.id2Port[Dstid] = port;
                }
            }
        }
    }
    if (lb_mode == 21){
        //更新每个交换机的接口与邻居id的关系
        for (const auto& outerPair : nbr2if) {
            ns3::Ptr<ns3::Node> SrcNode = outerPair.first;
            const auto& innerMap = outerPair.second;

            if (SrcNode->GetNodeType() == 1) {
                // 使用范围 for 循环遍历内层 map 中的每个键值对
                for (const auto& innerPair : innerMap) {
                    ns3::Ptr<ns3::Node> DstNode = innerPair.first;
                    uint32_t Dstid = DstNode->GetId();
                    ns3::Ptr<ns3::SwitchNode> Srcsw = DynamicCast<SwitchNode>(n.Get(SrcNode->GetId()));
                    Interface interface = innerPair.second;
                    uint32_t port = interface.idx;
                    Srcsw->m_mmu->m_noshareRouting.id2Port[Dstid] = port;
                }
            }
        }
    }
    /**
     * @brief get BDP and delay
     */
    maxRtt = maxBdp = 0;
    fprintf(stderr, "node_num=%d\n", node_num);
    for (uint32_t i = 0; i < node_num; i++) {
        if (n.Get(i)->GetNodeType() != 0) continue;
        // 只考虑server
        for (uint32_t j = i + 1; j < node_num; j++) {
            if (n.Get(j)->GetNodeType() != 0) continue;
            uint64_t delay = pairDelay[n.Get(i)][n.Get(j)];
            uint64_t txDelay = pairTxDelay[n.Get(i)][n.Get(j)];
            uint64_t rtt = delay * 2 + txDelay;
            uint64_t bw = pairBw[n.Get(i)][n.Get(j)];
            uint64_t bdp = rtt * bw / 1000000000 / 8;
            pairBdp[n.Get(i)][n.Get(j)] = bdp;
            pairBdp[n.Get(j)][n.Get(i)] = bdp;
            pairRtt[n.Get(i)][n.Get(j)] = rtt;
            pairRtt[n.Get(j)][n.Get(i)] = rtt;
            if (rtt < server_rtt_mon_interval) server_rtt_mon_interval = rtt;
            if (bdp > maxBdp) maxBdp = bdp;
            if (rtt > maxRtt) maxRtt = rtt;
        }
    }
    std::cout << "server_rtt_mon_interval: " << server_rtt_mon_interval << std::endl;
    fprintf(stderr, "maxRtt: %lu, maxBdp: %lu\n", maxRtt, maxBdp);
    assert(maxBdp == irn_bdp_lookup);

    std::cout << "Configuring switches" << std::endl;
    /* config ToR Switch */
    for (auto &pair : link_pairs) {
        Ptr<Node> probably_host = n.Get(pair.first);
        Ptr<Node> probably_switch = n.Get(pair.second);

        // host-switch link
        if (probably_host->GetNodeType() == 0 && probably_switch->GetNodeType() == 1) {
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(probably_switch);
            sw->m_isToR = true;
            uint32_t hostIP = serverAddress[pair.first].Get();
            sw->m_isToR_hostIP.insert(hostIP);
            if (idxNodeToR.find(sw->GetId()) == idxNodeToR.end()) {
                idxNodeToR[sw->GetId()] = sw;
            };
        }
    }

    if (lb_mode == 20){
        SetPathChoiceTables();
        SetBestPathCETables();
        SetACCPathCETables();
        if (init_log){
            printf("This is init table logging\n");
            for (auto i = nextHop.begin(); i != nextHop.end(); i++){
                Ptr<Node> node = i->first;
                auto &table = i->second;
                if (node->GetNodeType() == 1){
                    Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);
                    printf("Switch %d's BestPathCETable\n", sw->GetId());
                    sw->m_mmu->m_caverRouting.printBestPathCETable();
                    if(sw->m_isToR){
                        printf("ToR switch %d's PathChoiceTable\n", sw->GetId());
                        sw->m_mmu->m_caverRouting.printPathChoiceTable();
                        printf("ToR switch %d's PathChoiceFlagMap\n", sw->GetId());
                        sw->m_mmu->m_caverRouting.printPathChoiceFlagMap();
                    } else {
                        printf("Switch %d's ACCPathCETable\n", sw->GetId());
                        sw->m_mmu->m_caverRouting.printAcceptablePathTable();
                    }
                }
            }
        }
    }
    if (lb_mode == 21){
        SetPathChoiceTables_noshare();
        SetBestPathCETables_noshare();
        SetACCPathCETables_noshare();
        if (init_log){
            printf("This is init table logging\n");
            for (auto i = nextHop.begin(); i != nextHop.end(); i++){
                Ptr<Node> node = i->first;
                auto &table = i->second;
                if (node->GetNodeType() == 1){
                    Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);
                    printf("Switch %d's BestPathCETable\n", sw->GetId());
                    sw->m_mmu->m_noshareRouting.printBestPathCETable();
                    if(sw->m_isToR){
                        printf("ToR switch %d's PathChoiceTable\n", sw->GetId());
                        sw->m_mmu->m_noshareRouting.printPathChoiceTable();
                        printf("ToR switch %d's PathChoiceFlagMap\n", sw->GetId());
                        sw->m_mmu->m_noshareRouting.printPathChoiceFlagMap();
                    }
                    else{
                        printf("Switch %d's ACCPathCETable\n", sw->GetId());
                        sw->m_mmu->m_noshareRouting.printAcceptablePathTable();
                    }
                }
            }
        }
    }
    //init TorSwitch_nodelist, hostId2ToRlist, SrcId2CurSrcToR
    if (lb_mode == 3 || lb_mode == 6 || lb_mode == 9 || lb_mode == 10 || lb_mode == 12 || lb_mode == 20) {
        for (auto &pair : link_pairs) {
            Ptr<Node> probably_host = n.Get(pair.first);
            Ptr<Node> probably_switch = n.Get(pair.second);

            // host-switch link
            if (probably_host->GetNodeType() == 0 && probably_switch->GetNodeType() == 1) {
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(probably_switch);
                uint32_t hostIP = serverAddress[pair.first].Get();
                uint32_t hostId = Settings::hostIp2IdMap[hostIP];
                auto dstIter = Settings::TorSwitch_nodelist.find(sw->GetId());
                if (dstIter == Settings::TorSwitch_nodelist.end()) {
                    // 如果不存在，则创建一个新的条目
                    Settings::TorSwitch_nodelist[sw->GetId()] = std::vector<uint32_t>();
                }
                Settings::TorSwitch_nodelist[sw->GetId()].push_back(hostIP);

                auto hostIter = Settings::hostId2ToRlist.find(hostId);
                if (hostIter == Settings::hostId2ToRlist.end()) {
                    // 如果不存在，则创建一个新的条目
                    Settings::hostId2ToRlist[hostId] = std::vector<uint32_t>();
                }
                Settings::hostId2ToRlist[hostId].push_back(sw->GetId());
                SrcId2CurSrcToR[probably_host->GetId()] = 0;
            }
        } 
    }
    //idxNodeToR: save Tor switch, idxNodeToR[sw->GetId()] = sw;
    if (lb_mode == 9 || lb_mode == 3 || lb_mode == 6){
        //构建pathid
        if (Path_id_log){
            printf("Pathid construct info:");
        }
        for (auto i = nextHop.begin(); i != nextHop.end(); i++) {  // every node
            if (i->first->GetNodeType() == 1) {                    // switch
                Ptr<Node> nodeSrc = i->first; //nodeSrc：源ToR；
                Ptr<SwitchNode> swSrc = DynamicCast<SwitchNode>(nodeSrc);  // switch
                uint32_t swSrcId = swSrc->GetId();

                if (swSrc->m_isToR) {
                    if (Path_id_log){
                        printf("--- ToR Switch %d\n", swSrcId);
                    }
                    auto table1 = i->second;
                    for (auto j = table1.begin(); j != table1.end(); j++) {
                        Ptr<Node> dst = j->first;  // j->first: dst node； j->second: nodeSrc 到达 dst node的路径上的下一跳的节点列表
                        uint32_t dstIP = Settings::hostId2IpMap[dst->GetId()];
                        uint32_t dstId = dst->GetId();
                        //判断是否能直接到达：
                        auto dstfind_it = std::find(Settings::TorSwitch_nodelist[swSrcId].begin(), Settings::TorSwitch_nodelist[swSrcId].end(), dstId);
                        if (dstfind_it != Settings::TorSwitch_nodelist[swSrcId].end()){
                            continue;
                        }

                        if (lb_mode == 3) {
                            // initialize `m_congaFromLeafTable` and `m_congaToLeafTable`
                            for (uint32_t dstTorId : Settings::hostId2ToRlist[dstId]) {
                                swSrc->m_mmu->m_congaRouting
                                    .m_congaFromLeafTable[dstTorId];  // dynamically will be added in
                                                                    // conga
                                swSrc->m_mmu->m_congaRouting.m_congaToLeafTable[dstTorId];
                                //printf("init:%d, %d\n", swSrcId, dstTorId);
                            }
                        }

                        //遍历dstId相连的swDstUId
                        for (uint32_t swDstId : Settings::hostId2ToRlist[dstId]) {
                            uint32_t pathId;
                            uint8_t path_ports[4] = {0, 0, 0, 0};  // interface is always large than 0
                            vector<Ptr<Node>> nexts1 = j->second; //j->second: nodeSrc 到达 dst node的路径上的下一跳的节点列表
                            for (auto next1 : nexts1) {
                                uint32_t outPort1 = nbr2if[nodeSrc][next1].idx;
                                auto nexts2 = nextHop[next1][dst];

                                bool find = false;

                                for (auto next2 : nexts2) {
                                    uint32_t outPort2 = nbr2if[next1][next2].idx;
                                    if (next2->GetId() == swDstId){
                                        find = true;
                                        // this destination has 2-hop distance
                                        if(Path_id_log){
                                            printf("[IntraPod-2hop] %d (%d)-> %d (%d) -> %d -> %d\n",nodeSrc->GetId(), outPort1, next1->GetId(), outPort2, next2->GetId(), dst->GetId()); 
                                        }
                                        path_ports[0] = (uint8_t)outPort1;
                                        path_ports[1] = (uint8_t)outPort2;
                                        pathId = *((uint32_t *)path_ports);
                                        if (lb_mode == 3) {
                                            swSrc->m_mmu->m_congaRouting.m_congaRoutingTable[swDstId].insert(pathId);
                                        }
                                        if (lb_mode == 6) {
                                            swSrc->m_mmu->m_letflowRouting.m_letflowRoutingTable[swDstId]
                                                .insert(pathId);
                                        }
                                        if (lb_mode == 9) {
                                            swSrc->m_mmu->m_conweaveRouting.m_ConWeaveRoutingTable[swDstId]
                                                .insert(pathId);
                                            swSrc->m_mmu->m_conweaveRouting.m_rxToRId2BaseRTT[swDstId] =
                                                one_hop_delay * 4;
                                        }
                                        continue;
                                    }
                                    auto nexts3 = nextHop[next2][dst];
                                    for (auto next3 : nexts3) {
                                        uint32_t outPort3 = nbr2if[next2][next3].idx;
                                        if (next3->GetId() == swDstId){
                                            find = true;
                                            // this destination has 2-hop distance
                                            if(Path_id_log){
                                                printf("[IntraPod-3hop] %d (%d)-> %d (%d) -> %d (%d) -> %d ->%d\n", nodeSrc->GetId(), outPort1, next1->GetId(), outPort2,next2->GetId(), outPort3, next3->GetId(), dst->GetId());
                                            }
                                            path_ports[0] = (uint8_t)outPort1;
                                            path_ports[1] = (uint8_t)outPort2;
                                            path_ports[2] = (uint8_t)outPort3;
                                            pathId = *((uint32_t *)path_ports);
                                            if (lb_mode == 3) {
                                                swSrc->m_mmu->m_congaRouting.m_congaRoutingTable[swDstId]
                                                    .insert(pathId);
                                            }
                                            if (lb_mode == 6) {
                                                swSrc->m_mmu->m_letflowRouting
                                                    .m_letflowRoutingTable[swDstId]
                                                    .insert(pathId);
                                            }
                                            if (lb_mode == 9) {
                                                swSrc->m_mmu->m_conweaveRouting.m_ConWeaveRoutingTable[swDstId]
                                                    .insert(pathId);
                                                swSrc->m_mmu->m_conweaveRouting.m_rxToRId2BaseRTT[swDstId] =
                                                    one_hop_delay * 6;
                                            }
                                            continue;
                                        }
                                        auto nexts4 = nextHop[next3][dst];
                                        for (auto next4 : nexts4) {
                                            uint32_t outPort4 = nbr2if[next3][next4].idx;
                                            if (next4->GetId() == swDstId) {
                                                // this destination has 4-hop distance
                                                find = true;
                                                if(Path_id_log){
                                                    printf("[IntraPod-4hop] %d (%d)-> %d (%d) -> %d (%d) -> %d (%d) -> %d -> %d\n", nodeSrc->GetId(), outPort1,
                                                    next1->GetId(), outPort2, next2->GetId(), outPort3,
                                                    next3->GetId(), outPort4, next4->GetId(),
                                                    dst->GetId());
                                                }
                                                path_ports[0] = (uint8_t)outPort1;
                                                path_ports[1] = (uint8_t)outPort2;
                                                path_ports[2] = (uint8_t)outPort3;
                                                path_ports[3] = (uint8_t)outPort4;
                                                pathId = *((uint32_t *)path_ports);
                                                if (lb_mode == 3) {
                                                    swSrc->m_mmu->m_congaRouting
                                                        .m_congaRoutingTable[swDstId]
                                                        .insert(pathId);
                                                }
                                                if (lb_mode == 6) {
                                                    swSrc->m_mmu->m_letflowRouting
                                                        .m_letflowRoutingTable[swDstId]
                                                        .insert(pathId);
                                                }
                                                if (lb_mode == 9) {
                                                    swSrc->m_mmu->m_conweaveRouting
                                                        .m_ConWeaveRoutingTable[swDstId]
                                                        .insert(pathId);
                                                    swSrc->m_mmu->m_conweaveRouting
                                                        .m_rxToRId2BaseRTT[swDstId] = one_hop_delay * 8;

                                                }
                                            } 
                                            else if (!DynamicCast<SwitchNode>(next4)->m_isToR) {
                                                printf("False too large topo: %d (%d)-> %d (%d) -> %d (%d) -> %d (%d) -> %d -> %d\n", nodeSrc->GetId(), outPort1,
                                                    next1->GetId(), outPort2, next2->GetId(), outPort3,
                                                    next3->GetId(), outPort4, next4->GetId(),
                                                    dst->GetId());
                                                fflush(stdout);
                                                assert(false);                                   
                                            }
                                        }
                                    }
                                }
                            }    
                        }
                    }
                    //std::cout << "Path Table info: << sw: " <<swSrc->GetId() <<"\n";
                    // if (Path_Table_log){
                    //     for (const auto& [key, valueSet] : swSrc->m_mmu->m_conweaveRouting.m_ConWeaveRoutingTable) {
                    //         std::cout << "Path Table info: << sw: " <<swSrc->GetId() <<",Dst ToR: " << key << ",Path num "  << valueSet.size()<< "\n Path: " ;
                    //         for (const auto& value : valueSet) {
                    //             for (int temp_c = 0; temp_c < 4; temp_c++) {
                    //                 std::cout << static_cast<int>(((uint8_t *)&value)[temp_c]) << " ";
                    //             }
                    //             std::cout << std::endl;
                    //         }
                    //     }
                    //     for (const auto& [key, value] : swSrc->m_mmu->m_conweaveRouting.m_rxToRId2BaseRTT) {
                    //         std::cout << "Path Table info: << sw: " <<swSrc->GetId() <<",Dst ToR: " << key <<  "\n Path length: " ;
                    //         std::cout << value << std::endl;
                    //         std::cout << std::endl;
                    //     }
// 
// 
                    // }

                }
            }
        }

        // m_outPort2BitRateMap - only for Conga
        for (auto i = nextHop.begin(); i != nextHop.end(); i++) {  // every node
            if (i->first->GetNodeType() == 1) {                    // switch
                Ptr<Node> node = i->first;
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);  // switch
                uint32_t swId = sw->GetId();

                auto table = i->second;
                for (auto j = table.begin(); j != table.end(); j++) {
                    for (auto next : j->second) {
                        uint32_t outPort = nbr2if[node][next].idx;
                        uint64_t bw = nbr2if[node][next].bw;
                        sw->m_mmu->m_congaRouting.SetLinkCapacity(outPort, bw);
                        // printf("Node: %d, interface: %d, bw: %lu\n", swId, outPort, bw);
                    }
                }
            }
        }
        //设置参数
        for (auto i = nextHop.begin(); i != nextHop.end(); i++) {  // every node
            if (i->first->GetNodeType() == 1) {
                Ptr<Node> node = i->first;
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);  // switch
                NS_LOG_INFO("Switch Info - ID:%u, ToR:%d\n" % (sw->GetId(), sw->m_isToR));
                if (lb_mode == 3) {
                    sw->m_mmu->m_congaRouting.SetConstants(conga_dreTime, conga_agingTime,
                                                           conga_flowletTimeout, conga_quantizeBit,
                                                           conga_alpha);
                    sw->m_mmu->m_congaRouting.SetSwitchInfo(sw->m_isToR, sw->GetId());
                }
                if (lb_mode == 6) {
                    sw->m_mmu->m_letflowRouting.SetConstants(letflow_agingTime,
                                                             letflow_flowletTimeout);
                    sw->m_mmu->m_letflowRouting.SetSwitchInfo(sw->m_isToR, sw->GetId());
                }
                if (lb_mode == 9) {
                    sw->m_mmu->m_conweaveRouting.SetConstants(
                        conweave_extraReplyDeadline, conweave_extraVOQFlushTime,
                        conweave_txExpiryTime, conweave_defaultVOQWaitingTime,
                        conweave_pathPauseTime, conweave_pathAwareRerouting);
                    sw->m_mmu->m_conweaveRouting.SetSwitchInfo(sw->m_isToR, sw->GetId());
                }
            }
        }
        if (lb_mode == 9) {
            Simulator::Schedule(Seconds(flowgen_stop_time + simulator_extra_time),
                    conweave_history_print);
        }
    }
    if (lb_mode == 10){
        NS_LOG_INFO("Configuring Load Balancer's Switches");
        for (auto i = nextHop.begin(); i != nextHop.end(); i++) {  // every node
            if (i->first->GetNodeType() == 1) {
                Ptr<Node> node = i->first;
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);  // switch
                NS_LOG_INFO("Switch Info - ID:%u, ToR:%d\n" % (sw->GetId(), sw->m_isToR));
                sw->m_mmu->m_dvRouting.SetConstants(dv_dreTime, dv_agingTime,
                                                           dv_flowletTimeout, dv_quantizeBit,
                                                           dv_alpha);
                sw->m_mmu->m_dvRouting.SetSwitchInfo(sw->m_isToR, sw->GetId());
            }
        }

        for (auto i = nextHop.begin(); i != nextHop.end(); i++) {  // every node
            if (i->first->GetNodeType() == 1) {                    // switch
                Ptr<Node> node = i->first;
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);  // switch
                uint32_t swId = sw->GetId();

                auto table = i->second;
                for (auto j = table.begin(); j != table.end(); j++) {
                    Ptr<Node> dst = j->first;  // dst
                    uint32_t dstIP = Settings::hostId2IpMap[dst->GetId()];
                    uint32_t swDstId = Settings::hostIp2SwitchId[dstIP];

                    for (auto next : j->second) {
                        uint32_t outPort = nbr2if[node][next].idx;
                        uint64_t bw = nbr2if[node][next].bw;
                        sw->m_mmu->m_dvRouting.SetLinkCapacity(outPort, bw);
                        //printf("Node: %d, interface: %d, bw: %lu\n", swId, outPort, bw);
                    }
                }
            }
        }
        Simulator::Schedule(Seconds(flowgen_stop_time + simulator_extra_time), dv_history_print);
    }
    if (lb_mode == 20){
        std::cout << "caver_dreTime: " << caver_dreTime << std::endl;
        std::cout << "caver_agingTime: " << caver_agingTime << std::endl;
        std::cout << "caver_quantizeBit: " << caver_quantizeBit << std::endl;
        std::cout << "caver_alpha: " << caver_alpha << std::endl;
        std::cout << "caver_flowletTimeout: " << caver_flowletTimeout << std::endl;
        std::cout << "caver_ce_threshold: " << caver_ce_threshold << std::endl;
        std::cout << "caver_patchoiceTimeout: " << caver_patchoiceTimeout << std::endl;
        std::cout << "caver_pathChoice_num: " << caver_pathChoice_num << std::endl;
        std::cout << "caver_tau:" << caver_tau << std::endl;
        std::cout << "caver_useEWMA" << caver_useEWMA << std::endl;
        NS_LOG_INFO("Configuring Load Balancer's Switches");
        for (auto i = nextHop.begin(); i != nextHop.end(); i++) {  // every node
            if (i->first->GetNodeType() == 1) {
                Ptr<Node> node = i->first;
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);  // switch
                NS_LOG_INFO("Switch Info - ID:%u, ToR:%d\n" % (sw->GetId(), sw->m_isToR));
                sw->m_mmu->m_caverRouting.SetConstants(caver_dreTime, caver_agingTime,
                                                           caver_flowletTimeout, caver_quantizeBit,
                                                           caver_alpha, caver_ce_threshold, caver_patchoiceTimeout, caver_pathChoice_num, caver_tau, caver_useEWMA);
                sw->m_mmu->m_caverRouting.SetSwitchInfo(sw->m_isToR, sw->GetId());
                // dive into related
            }
        }

        for (auto i = nextHop.begin(); i != nextHop.end(); i++) {  // every node
            if (i->first->GetNodeType() == 1) {                    // switch
                Ptr<Node> node = i->first;
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);  // switch
                uint32_t swId = sw->GetId();

                auto table = i->second;
                for (auto j = table.begin(); j != table.end(); j++) {
                    Ptr<Node> dst = j->first;  // dst
                    uint32_t dstIP = Settings::hostId2IpMap[dst->GetId()];
                    uint32_t swDstId = Settings::hostIp2SwitchId[dstIP];

                    for (auto next : j->second) {
                        uint32_t outPort = nbr2if[node][next].idx;
                        uint64_t bw = nbr2if[node][next].bw;
                        sw->m_mmu->m_caverRouting.SetLinkCapacity(outPort, bw);
                        //printf("Node: %d, interface: %d, bw: %lu\n", swId, outPort, bw);
                    }
                }
            }
        }
    }
if (lb_mode == 21){
        NS_LOG_INFO("Configuring Load Balancer's Switches");
        for (auto i = nextHop.begin(); i != nextHop.end(); i++) {  // every node
            if (i->first->GetNodeType() == 1) {
                Ptr<Node> node = i->first;
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);  // switch
                NS_LOG_INFO("Switch Info - ID:%u, ToR:%d\n" % (sw->GetId(), sw->m_isToR));
                sw->m_mmu->m_noshareRouting.SetConstants(caver_dreTime, caver_agingTime,
                                                           caver_flowletTimeout, caver_quantizeBit,
                                                           caver_alpha, caver_ce_threshold, caver_patchoiceTimeout, caver_pathChoice_num, caver_tau, caver_useEWMA);
                sw->m_mmu->m_noshareRouting.SetSwitchInfo(sw->m_isToR, sw->GetId());
            }
        }


        for (auto i = nextHop.begin(); i != nextHop.end(); i++) {  // every node
            if (i->first->GetNodeType() == 1) {                    // switch
                Ptr<Node> node = i->first;
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);  // switch
                uint32_t swId = sw->GetId();

                auto table = i->second;
                for (auto j = table.begin(); j != table.end(); j++) {
                    Ptr<Node> dst = j->first;  // dst
                    uint32_t dstIP = Settings::hostId2IpMap[dst->GetId()];
                    uint32_t swDstId = Settings::hostIp2SwitchId[dstIP];

                    for (auto next : j->second) {
                        uint32_t outPort = nbr2if[node][next].idx;
                        uint64_t bw = nbr2if[node][next].bw;
                        sw->m_mmu->m_noshareRouting.SetLinkCapacity(outPort, bw);
                        //printf("Node: %d, interface: %d, bw: %lu\n", swId, outPort, bw);
                    }
                }
            }
        }
    }
        //配置hula
    if (lb_mode == 12) {
        std::cout<<"=====Hula Constants=====\n";
        std::cout<<"Hula probe generation interval:"<<hula_probeGenerationInterval<<std::endl;
        std::cout<<"Hula probe transmit interval:"<<hula_probeTransmitInterval<<std::endl;
        for (auto &pair1 : nbr2if) {
            if (pair1.first->GetNodeType() == 0) { //只考虑交换机
                continue;
            }
            Ptr<SwitchNode> snode = DynamicCast<SwitchNode>(pair1.first);
            if (snode->m_isToR) {
                snode->m_mmu->m_hulaRouting.active(2);
            }
            for (auto &pair2 : pair1.second) {
                Ptr<Node> dnode = pair2.first;
                Interface &itf = pair2.second;
                if (snode->GetId() < dnode->GetId()) {
                    snode->m_mmu->m_hulaRouting.upLayerDevs.insert(itf.idx);
                    //std::cout<<snode->GetId()<<"上"<<dnode->GetId()<<"itf"<<itf.idx<<std::endl;
                } else {
                    snode->m_mmu->m_hulaRouting.downLayerDevs.insert(itf.idx);
                    //std::cout<<snode->GetId()<<"下"<<dnode->GetId()<<"itf"<<itf.idx<<std::endl;
                }
                snode->m_mmu->m_hulaRouting.SetLinkCapacity(itf.idx, itf.bw);
            }
        }
        for (auto i = nextHop.begin(); i != nextHop.end(); i++) {  // every node
            if (i->first->GetNodeType() == 1) {
                Ptr<Node> node = i->first;
                Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);  // switch
                NS_LOG_INFO("Switch Info - ID:%u, ToR:%d\n" % (sw->GetId(), sw->m_isToR));
                sw->m_mmu->m_hulaRouting.SetConstants(
                    hula_keepAliveThresh,
                    hula_probeTransmitInterval,
                    hula_flowletInterval,
                    hula_probeGenerationInterval,
                    hula_tau
                );
                sw->m_mmu->m_hulaRouting.SetSwitchInfo(sw->m_isToR, sw->GetId());
            }
        }
        Simulator::Schedule(Seconds(flowgen_stop_time + simulator_extra_time), hula_history_print);
    }

    //dive into related，记录最优路径相关的代码；
    Settings::init_nextHop(ConvertAndStore(nextHop));
    //初始化最优路径相关的表
    Settings::init_nodeInterfaceMap(CreateNodeInterfaceMap(nbr2if));
    Settings::init_nbr2if(CreateNbr2Interface(nbr2if));
    Settings::SetCaverQuantizeBit(caver_quantizeBit);
    Settings::SetCaverAlpha(caver_alpha);
    
    for (auto i = nextHop.begin(); i != nextHop.end(); i++) {  // every node
        if (i->first->GetNodeType() == 1) {                    // switch
            Ptr<Node> node = i->first;
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(node);  // switch
            uint32_t swId = sw->GetId();
            Settings::SetDreTime(sw->GetId(), caver_dreTime);
            auto table = i->second;
            for (auto j = table.begin(); j != table.end(); j++) {
                Ptr<Node> dst = j->first;  // dst
                uint32_t dstIP = Settings::hostId2IpMap[dst->GetId()];
                uint32_t swDstId = Settings::hostIp2SwitchId[dstIP];
                for (auto next : j->second) {
                    uint32_t outPort = nbr2if[node][next].idx;
                    uint64_t bw = nbr2if[node][next].bw;
                    //dive into related
                    Settings::SetLinkCapacity(swId, outPort, bw);
                    Settings::init_global_dre_map(swId, outPort);
                    //printf("Node: %d, interface: %d, bw: %lu\n", swId, outPort, bw);
                }
            }
        }
    }
    //std::cout << "global optimal init info: " << std::endl;
    //Settings::ShowInit();
    // populate routing tables (although we use our custom impl in switch_node.cc)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // maintain port number for each host
    for (uint32_t i = 0; i < node_num; i++) {
        if (n.Get(i)->GetNodeType() == 0) {
            portNumber[i] = 10000;  // each host use port number from 10000
            dportNumber[i] = 100;
        }
    }

    flow_input.idx = 0;
    port_per_host = new uint16_t[node_num - switch_num];
    if (flow_num > 0) {
        // generate flows
        ReadFlowInput();
        Simulator::Schedule(Seconds(0), &ScheduleFlowInputs, flow_input_stream);
    }

    topof.close();

    // schedule link down
    if (link_down_time > 0) {
        Simulator::Schedule(Seconds(flowgen_start_time) + MicroSeconds(link_down_time),
                            &TakeDownLink, n, n.Get(link_down_A), n.Get(link_down_B));
    }

    if (lb_mode == 9) {
        voq_output = fopen(voq_mon_file.c_str(), "w");                // specific to ConWeave
        voq_detail_output = fopen(voq_mon_detail_file.c_str(), "w");  // specific to ConWeave
    }

    uplink_output = fopen(uplink_mon_file.c_str(), "w");  // common
    downlink_output = fopen(downlink_mon_file.c_str(), "w");
    uplink_rx_output = fopen(uplink_rx_mon_file.c_str(), "w");
    downlink_rx_output = fopen(downlink_rx_mon_file.c_str(), "w");
    flow_rx_output = fopen(flow_mon_file.c_str(), "w");
    bps_tx_output = fopen(bps_mon_file.c_str(), "w");
    conn_output = fopen(conn_mon_file.c_str(), "w");      // common
    global_CE_map_output = fopen(global_CE_map_mon_file.c_str(), "w");
    all_links_output = fopen(all_links_mon_file.c_str(), "w");

    // update torId2UplinkIf, torId2DownlinkIf
    for (size_t ToRId = 0; ToRId < Settings::node_num; ToRId++) {
        Ptr<Node> node = n.Get(ToRId);
        if (node->GetNodeType() == 1) {  // switches
            auto swNode = DynamicCast<SwitchNode>(n.Get(ToRId));
            if (swNode->m_isToR) {  // TOR switch
                for (auto &nextNodeIf : nbr2if[node]) {
                    if (nextNodeIf.first->GetNodeType() ==
                        1) {  // nextNode is switch (i.e., uplink)
                        auto &vec = torId2UplinkIf[ToRId];
                        vec.push_back(
                            nextNodeIf.second.idx);  // record this uplink port (outDev index)
                        //printf("Sw %lu - uplink port %u node id %u\n", ToRId, nextNodeIf.second.idx, nextNodeIf.first->GetId());  //
                        // debugging
                    } else {
                        auto &vec = torId2DownlinkIf[ToRId];
                        vec.push_back(
                            nextNodeIf.second.idx);  // record this downlink port (outDev index)
                        //printf("Sw %lu - downlink port %u node id %u\n", ToRId, nextNodeIf.second.idx, nextNodeIf.first->GetId());  //
                        // debugging
                    }
                }
            }
        }
    }
    Simulator::Schedule(Seconds(flowgen_start_time), &periodic_monitoring, voq_output,
                        voq_detail_output, uplink_output, conn_output, &lb_mode);
    Simulator::Schedule(Seconds(flowgen_start_time), &m_periodic_monitoring, voq_output,
                        uplink_output, downlink_output, conn_output, &lb_mode);

    Simulator::Schedule(Seconds(flowgen_start_time), &m_rx_periodic_monitoring, uplink_rx_output,
                        downlink_rx_output, flow_rx_output);
    Simulator::Schedule(Seconds(flowgen_start_time), &m_QP_rate_monitoring, bps_tx_output);

    if (global_ce_log){
        //Settings::read_static_path("config/my_path.txt");
        Simulator::Schedule(Seconds(flowgen_start_time), &m_global_ce_map_monitoring, global_CE_map_output);
        Simulator::Schedule(Seconds(flowgen_start_time), &m_all_links_monitoring, all_links_output);
    }
    
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();
    flowMonitor->Start(Seconds(flowgen_start_time));
    flowMonitor->Stop(Seconds(flowgen_stop_time + 10.0));

    size_t lastSlashPos = pfc_output_file.find_last_of("/\\");
    flow_distribution_monitoring(Seconds(flowgen_start_time), fopen((pfc_output_file.substr(0, lastSlashPos + 1) + "flow_distribution.txt").c_str(), "w"));
    Settings::caverLog = fopen((pfc_output_file.substr(0, lastSlashPos + 1) + "caver_log.txt").c_str(), "w");
    packetId2FlowId = fopen((pfc_output_file.substr(0, lastSlashPos + 1) + "packetId2FlowId.txt").c_str(), "w");
    ideal_ce = fopen((pfc_output_file.substr(0, lastSlashPos + 1) + "ideal_ce.txt").c_str(), "w");

    //
    // Now, do the actual simulation.
    //
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Running Simulation.\n";
    fflush(stdout);
    NS_LOG_INFO("Run Simulation.");
    Simulator::Schedule(Seconds(flowgen_start_time),
                        &stop_simulation_middle);  // check every 100us
    Simulator::Stop(Seconds(flowgen_stop_time + 10.0));
    Simulator::Run();

    //for (const auto& entry : Settings::PacketId2FlowId) {
    //    const auto& tuple_key = entry.first;
    //    uint32_t value = entry.second;
    //    
    //    fprintf(packetId2FlowId, "FlowKey: (%u %u %u %u) -> FlowId: %u\n",
    //            std::get<0>(tuple_key), std::get<1>(tuple_key), 
    //            std::get<2>(tuple_key), std::get<3>(tuple_key), value);
    //}

    //TODO:my code to caculate the throughput of each flow
        // 输出每个流的发送速率
    //flowMonitor->SerializeToXmlFile("NameOfFile.xml", true, true);
    /*-----------------------------------------------------------------------------*/
    /*----- we don't need below. Just we can enforce to close this simulation. -----*/
    /*-----------------------------------------------------------------------------*/
    Simulator::Destroy();
    NS_LOG_INFO("Total number of packets: " << RdmaHw::nAllPkts);
    NS_LOG_INFO("Done.");
    endt = clock();
    std::cerr << (double)(endt - begint) / CLOCKS_PER_SEC << "\n";
}

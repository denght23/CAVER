#include "ns3/settings.h"
#include "ns3/simulator.h"



namespace ns3 {
/* helper function */
Ipv4Address Settings::node_id_to_ip(uint32_t id) {
    return Ipv4Address(0x0b000001 + ((id / 256) * 0x00010000) + ((id % 256) * 0x00000100));
}
uint32_t Settings::ip_to_node_id(Ipv4Address ip) {
    return (ip.Get() >> 8) & 0xffff;
}

/* others */
uint32_t Settings::lb_mode = 0;

std::map<uint32_t, uint32_t> Settings::hostIp2IdMap;
std::map<uint32_t, uint32_t> Settings::hostId2IpMap;

std::unordered_map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, uint32_t, Settings::tuple_hash> Settings::PacketId2FlowId; 
std::map<std::tuple<ns3::Ipv4Address, ns3::Ipv4Address, uint16_t, uint16_t>, uint32_t>Settings:: QPPair_info2FlowId;
std::unordered_map<uint32_t, uint32_t> Settings::FlowId2SrcId;

/* statistics */
uint32_t Settings::node_num = 0;
uint32_t Settings::host_num = 0;
uint32_t Settings::switch_num = 0;
uint64_t Settings::cnt_finished_flows = 0;
uint32_t Settings::packet_payload = 1000;

uint32_t Settings::dropped_pkt_sw_ingress = 0;
uint32_t Settings::dropped_pkt_sw_egress = 0;

/* for load balancer */
std::map<uint32_t, uint32_t> Settings::hostIp2SwitchId;
std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> Settings::flowId2SrcDst; //流的id对应源Torid和目的Torid
std::unordered_map<uint32_t, uint32_t> Settings::flowId2Port2Src; //流的id对应源Torid所需要选择的出端口
std::map<uint32_t, std::vector<uint32_t>>Settings::hostId2ToRlist;
bool Settings::isBond = false;

std::map<uint32_t, std::vector<uint32_t>>Settings::TorSwitch_nodelist;

std::map<Ptr<Node>, std::map<uint32_t, uint32_t> > Settings::if2id;

std::unordered_map<uint64_t, std::unordered_map<uint32_t, Time>> Settings::flowRecorder;
void Settings::record_flow_distribution(CustomHeader &ch, Ptr<Node> srcNode, uint32_t outDev) {
    if (ch.l3Prot != 0x11) {
        return;
    }
    uint32_t srcId = srcNode->GetId();
    uint32_t dstId = Settings::if2id[srcNode][outDev];
    uint64_t linkKey = (static_cast<uint64_t>(srcId) << 32) | static_cast<uint64_t>(dstId);
    if (dstId == Settings::hostIp2IdMap[ch.dip]) {
        return;
    }
    uint32_t flowId = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
    flowRecorder[linkKey][flowId] = Simulator::Now();
}

void Settings::print_flow_distribution(FILE *out, Time nextTime) {
    // 打印当前时间
    fprintf(out, "#####Time[%ld]#####\n", Simulator::Now().GetNanoSeconds());

    for (auto linkEntry = flowRecorder.begin(); linkEntry != flowRecorder.end(); ++linkEntry) {
        uint64_t linkKey = linkEntry->first;
        auto& flowMap = linkEntry->second;
        uint32_t srcId = static_cast<uint32_t>(linkKey >> 32);
        uint32_t dstId = static_cast<uint32_t>(linkKey & 0xFFFFFFFF);
        //去除不活跃的流
        for (auto flowEntry = flowMap.begin(); flowEntry != flowMap.end();) {
            Time flowTime = flowEntry->second;
            if (Simulator::Now() - flowTime > nextTime) {
                flowEntry = flowMap.erase(flowEntry); 
                continue;
            }
            ++flowEntry;
        }

        fprintf(out, "Link: srcId=%u, dstId=%u, flowNum=%d, active flow:", srcId, dstId, flowMap.size());

        for (auto flowEntry = flowMap.begin(); flowEntry != flowMap.end();) {
            uint32_t flowId = flowEntry->first;
            fprintf(out, "%d, ", flowId);
            ++flowEntry;
        }
        fprintf(out, "\n");
    }
    fprintf(out, "\n");
    Simulator::Schedule(nextTime, &Settings::print_flow_distribution, out, nextTime);
}



}  // namespace ns3

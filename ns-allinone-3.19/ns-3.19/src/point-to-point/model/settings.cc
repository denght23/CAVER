#include "ns3/settings.h"

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

std::map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, uint32_t>Settings:: PacketId2FlowId; 
std::map<std::tuple<ns3::Ipv4Address, ns3::Ipv4Address, uint16_t, uint16_t>, uint32_t>Settings:: QPPair_info2FlowId;
std::map<uint32_t, uint32_t> Settings::FlowId2SrcId;

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
std::map<uint32_t, std::pair<uint32_t, uint32_t>> Settings::flowId2SrcDst; //流的id对应源Torid和目的Torid
std::map<uint32_t, uint32_t> Settings::flowId2Port2Src; //流的id对应源Torid所需要选择的出端口
std::map<uint32_t, std::vector<uint32_t>>Settings::hostId2ToRlist;

std::map<uint32_t, std::vector<uint32_t>>Settings::TorSwitch_nodelist;

}  // namespace ns3

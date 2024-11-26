#include "ns3/settings.h"

#include <limits> // for std::numeric_limits
#include <map>
#include <vector>
#include <set>
#include <algorithm> // for std::max
#include <functional>
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

std::map<uint32_t, std::map<uint32_t, std::vector<uint32_t>>> Settings::m_nextHop;
std::map<std::pair<uint32_t, uint32_t>, double> Settings::global_dre_map;
std::map<std::pair<uint32_t, uint32_t>, uint32_t> Settings::global_CE_map;
std::map<std::pair<uint32_t, uint32_t>, uint64_t> Settings::global_linkwidth;
std::map<uint32_t, std::map<uint32_t, uint32_t>> Settings::m_nodeInterfaceMap;
std::map<uint32_t, Time> Settings::Dre_time_map;
uint32_t Settings::caver_quantizeBit;
double Settings::caver_alpha;

std::pair<std::vector<uint32_t>, uint32_t> Settings::FindMinCostPath(uint32_t startNode, uint32_t destNode) {
    std::set<uint32_t> visited;
    std::vector<uint32_t> currentPath;
    std::vector<uint32_t> minPath;
    uint32_t minCost = std::numeric_limits<uint32_t>::max();
    //更新CEtable
    UpdateCETable();

        // 辅助函数：DFS 寻找路径
        std::function<void(uint32_t, uint32_t)> dfs = [&](uint32_t currentNode, uint32_t currentCost) {
            if (currentNode == destNode) {
                // 更新最小开销路径
                if (currentCost < minCost) {
                    minCost = currentCost;
                    minPath = currentPath;
                }
                return;
            }

            // 标记当前节点已访问
            visited.insert(currentNode);

            // 遍历所有下一跳
            for (const uint32_t& nextHop : m_nextHop[currentNode][destNode]) {
                if (visited.find(nextHop) == visited.end()) {
                    // 获取当前链路的 CE 值
                    uint32_t linkCost = global_CE_map[{currentNode, nextHop}];
                    uint32_t newCost = std::max(currentCost, linkCost);

                    currentPath.push_back(nextHop);
                    dfs(nextHop, newCost);
                    currentPath.pop_back();
                }
            }

            // 回溯，撤销当前节点访问状态
            visited.erase(currentNode);
        };

        // 初始化搜索
    currentPath.push_back(startNode);
    dfs(startNode, 0.0);

    return {minPath, minCost};
}
void Settings::init_nextHop(std::map<uint32_t, std::map<uint32_t, std::vector<uint32_t>>>nextHop){
    m_nextHop = nextHop;
}
void Settings::init_global_dre_map() {
    for (uint32_t i = 0; i < node_num; i++) {
        for (uint32_t j = 0; j < node_num; j++) {
            global_dre_map[{i, j}] = 0.0;
        }
    }
}
void Settings::init_nodeInterfaceMap(std::map<uint32_t, std::map<uint32_t, uint32_t>> nodeInterfaceMap){
    m_nodeInterfaceMap = nodeInterfaceMap;
}
void Settings::SetLinkCapacity(uint32_t src_id, uint32_t outPort, uint64_t bitRate){
    uint32_t dst_id = m_nodeInterfaceMap[src_id][outPort];
    global_linkwidth[{src_id, dst_id}] = bitRate;
}
void Settings::SetDreTime(uint32_t switch_id, Time dreTime){
    Dre_time_map[switch_id] = dreTime;
}
void Settings::SetCaverQuantizeBit(uint32_t quantizeBit){
    caver_quantizeBit = quantizeBit;
}
void Settings::SetCaverAlpha(double alpha){
    caver_alpha = alpha;
}
void Settings::UpdateCETable(){
    for (const auto& entry : global_dre_map) {
        std::pair<uint32_t, uint32_t> key = entry.first;
        uint32_t src_id = key.first;
        uint32_t dst_id = key.second;
        double dre = entry.second;

        uint64_t bitRate = global_linkwidth[key];
        Time m_dreTime = Dre_time_map[src_id];

        double ratio = static_cast<double>(dre * 8) / (bitRate * m_dreTime.GetSeconds() / caver_alpha);
        uint32_t quantX = static_cast<uint32_t>(ratio * std::pow(2, caver_quantizeBit));
        global_CE_map[key] = quantX;
    }
}
}  // namespace ns3

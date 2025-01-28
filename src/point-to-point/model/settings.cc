#include "ns3/settings.h"

#include <limits> // for std::numeric_limits
#include <map>
#include <vector>
#include <set>
#include <algorithm> // for std::max
#include <functional>
#include "ns3/simulator.h"
#include <queue>



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
std::unordered_map<uint32_t, uint32_t> Settings::FlowId2Length;
FILE* Settings::caverLog;

/* statistics */
uint32_t Settings::node_num = 0;
uint32_t Settings::host_num = 0;
uint32_t Settings::switch_num = 0;
uint64_t Settings::cnt_finished_flows = 0;
uint32_t Settings::packet_payload = 1000;

uint32_t Settings::dropped_pkt_sw_ingress = 0;
uint32_t Settings::dropped_pkt_sw_egress = 0;
bool Settings::setting_debug = false;
bool Settings::motivation_pathCE = false;
bool Settings::set_fixed_routing = false;    //选择固定的路由

std::string Settings::pathCE_mon_file = "pathCE_mon.txt";
std::string Settings::pathCE_exclude_lasthop_mon_file = "pathCE_exclude_lasthop_mon.txt";

/* for load balancer */
std::map<uint32_t, uint32_t> Settings::hostIp2SwitchId;


std::map<uint32_t, std::map<uint32_t, std::vector<uint32_t>>> Settings::m_nextHop;
std::map<std::pair<uint32_t, uint32_t>, uint32_t> Settings::global_dre_map;
std::map<std::pair<uint32_t, uint32_t>, uint32_t> Settings::global_CE_map;
std::map<std::pair<uint32_t, uint32_t>, uint64_t> Settings::global_linkwidth;
std::map<uint32_t, std::map<uint32_t, uint32_t>> Settings::m_nodeInterfaceMap;
std::map<uint32_t, std::map<uint32_t, uint32_t>> Settings::m_nbr2if;
std::vector<std::vector<uint32_t>> Settings::static_paths;
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

void Settings::init_global_dre_map(uint32_t src_id, uint32_t outPort) {
    uint32_t dst_id = m_nodeInterfaceMap[src_id][outPort];
    global_dre_map[{src_id, dst_id}] = 0;
    global_CE_map[{src_id, dst_id}] = 0;
}

void Settings::init_nodeInterfaceMap(std::map<uint32_t, std::map<uint32_t, uint32_t>> nodeInterfaceMap){
    m_nodeInterfaceMap = nodeInterfaceMap;
}
void Settings::init_nbr2if(std::map<uint32_t, std::map<uint32_t, uint32_t>> nbr2if){
    m_nbr2if = nbr2if;
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
void Settings::ShowInit(){
    std::cout << "caver_quantizeBit: " << caver_quantizeBit << std::endl;
    std::cout << "caver_alpha: " << caver_alpha << std::endl;
    // 显示global_linkwidth
    for (const auto& entry : global_linkwidth) {
        std::pair<uint32_t, uint32_t> key = entry.first;
        uint32_t src_id = key.first;
        uint32_t dst_id = key.second;
        uint64_t bitRate = entry.second;
        std::cout << "src_id: " << src_id << ", dst_id: " << dst_id << ", bitRate: " << bitRate << std::endl;
    }
    // 显示Dre_time_map
    for (const auto& entry : Dre_time_map) {
        uint32_t switch_id = entry.first;
        Time dreTime = entry.second;
        std::cout << "switch_id: " << switch_id << ", dreTime: " << dreTime.GetSeconds() << std::endl;
    }
}
void Settings::UpdateCETable(){
    if(setting_debug){
        std::cout << "Debug info: Update CE table: " << std::endl;
    }
    for (const auto& entry : global_dre_map) {
        std::pair<uint32_t, uint32_t> key = entry.first;
        if (global_linkwidth.find(key) == global_linkwidth.end()) {
            continue;
        }
        uint32_t src_id = key.first;
        uint32_t dst_id = key.second;
        uint32_t dre = entry.second;

        uint64_t bitRate = global_linkwidth[key];
        Time m_dreTime = Dre_time_map[src_id];

        double ratio = static_cast<double>(dre * 8) / (bitRate * m_dreTime.GetSeconds() / caver_alpha);
        uint32_t quantX = static_cast<uint32_t>(ratio * std::pow(2, caver_quantizeBit));
        global_CE_map[key] = quantX;
        if(setting_debug){
            std ::cout << "bitRate: " << bitRate << std::endl;
            std::cout << "m_dreTime.GetSeconds(): " << m_dreTime.GetSeconds() << std::endl;
            std::cout << "caver_alpha: " << caver_alpha << std::endl;
            std::cout << "bitRate * m_dreTime.GetSeconds() / caver_alpha: " << bitRate * m_dreTime.GetSeconds() / caver_alpha << std::endl;
            std::cout << "src_id: " << src_id << ", dst_id: " << dst_id << ", dre: "<< dre <<", ratio:" << ratio << ", quantX" << quantX << global_dre_map[key] << std::endl;
        }
    }
}
std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> Settings::flowId2SrcDst; //流的id对应源Torid和目的Torid
std::unordered_map<uint32_t, uint32_t> Settings::flowId2Port2Src; //流的id对应源Torid所需要选择的出端口
std::map<uint32_t, std::vector<uint32_t>>Settings::hostId2ToRlist;
bool Settings::isBond = false;

std::map<uint32_t, std::vector<uint32_t>>Settings::TorSwitch_nodelist;

std::map<Ptr<Node>, std::map<uint32_t, uint32_t> > Settings::if2id;

std::unordered_map<uint64_t, std::unordered_map<uint32_t, Time>> Settings::flowRecorder;
std::unordered_map<uint64_t, Settings::LinkRecord> Settings::linkRecorder; 
void Settings::record_flow_distribution(Ptr<Packet> p, CustomHeader &ch, Ptr<Node> srcNode, uint32_t outDev) {
    return;
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
    //printf("[%ld]%u -> %u, Flow:%u, Seq:%u\n", Simulator::Now().GetNanoSeconds(), srcId, dstId, flowId, ch.udp.seq);
    //flowRecorder[linkKey][flowId] = Simulator::Now();
    linkRecorder[linkKey].total_size += p->GetSize();
    linkRecorder[linkKey].flowRecorder.emplace(flowId, ch.udp.seq);
    linkRecorder[linkKey].flowRecorder[flowId].seq_end = ch.udp.seq + p->GetSize() - ch.GetSerializedSize();
}

uint32_t Settings::dropped_flow_id = -1;


void Settings::print_flow_distribution(FILE *out, Time nextTime) {
    return;
    for (auto& [linkKey, linkRecord] : linkRecorder) {
        uint32_t srcId = static_cast<uint32_t>(linkKey >> 32);
        uint32_t dstId = static_cast<uint32_t>(linkKey & 0xFFFFFFFF);
        fprintf(out, "%ld#%u->%u:%u#", Simulator::Now().GetNanoSeconds(), srcId, dstId, linkRecord.total_size);
        for (auto& [flowId, flowRecord] : linkRecord.flowRecorder) {
            fprintf(out, "%u:%u-%u, ", flowId, flowRecord.seq_start, flowRecord.seq_end);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "\n");
    linkRecorder.clear();
    /*fprintf(out, "#####Time[%ld]#####\n", Simulator::Now().GetNanoSeconds());

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

        fprintf(out, "Link: srcId=%u, dstId=%u, flowNum=%zu, active flow:", srcId, dstId, flowMap.size());

        for (auto flowEntry = flowMap.begin(); flowEntry != flowMap.end();) {
            uint32_t flowId = flowEntry->first;
            fprintf(out, "%d, ", flowId);
            ++flowEntry;
        }
        fprintf(out, "\n");
    }
    fprintf(out, "\n");*/
    Simulator::Schedule(nextTime, &Settings::print_flow_distribution, out, nextTime);
}

// 辅助函数：计算路径的PathCE
uint32_t Settings::calculatePathCE(const std::vector<uint32_t>& path) {
    uint32_t maxCE = 0;
    for (size_t i = 1; i < path.size(); ++i) {
        auto link = std::make_pair(path[i - 1], path[i]);
        if (global_CE_map.find(link) != global_CE_map.end()) {
            maxCE = std::max(maxCE, global_CE_map[link]);
        }
    }
    return maxCE;
}
// 辅助函数：计算路径的PathCE（不包括最后一跳）
uint32_t Settings::calculatePathCEExcludeLast(const std::vector<uint32_t>& path) {
    uint32_t maxCE = 0;
    for (size_t i = 1; i < path.size() - 1; ++i) {
        auto link = std::make_pair(path[i - 1], path[i]);
        if (global_CE_map.find(link) != global_CE_map.end()) {
            maxCE = std::max(maxCE, global_CE_map[link]);
        }
    }
    return maxCE;
}
// 辅助函数：通过BFS找到所有路径
void Settings::findAllPaths(uint32_t src, uint32_t dst, std::vector<std::vector<uint32_t>>& allPaths) {
    std::queue<std::vector<uint32_t>> q;
    q.push({src});
    while (!q.empty()) {
        auto path = q.front();
        q.pop();
        uint32_t lastNode = path.back();
        if (lastNode == dst) {
            allPaths.push_back(path);
            continue;
        }
        if (m_nextHop.find(lastNode) != m_nextHop.end() &&
            m_nextHop[lastNode].find(dst) != m_nextHop[lastNode].end()) {
            for (const auto& nextHop : m_nextHop[lastNode][dst]) {
                if (std::find(path.begin(), path.end(), nextHop) == path.end()) { // 避免循环
                    auto newPath = path;
                    newPath.push_back(nextHop);
                    q.push(newPath);
                }
            }
        }
    }
}
// 主函数：计算并保存路径CE值
//调用方法：savePathCEs(1, 4);
void Settings::savePathCEs(uint32_t src, uint32_t dst) {
    std::vector<std::vector<uint32_t>> allPaths;
    UpdateCETable();

    findAllPaths(src, dst, allPaths);

    FILE* ofs1 = fopen(pathCE_mon_file.c_str(), "a"); // 使用 "a" 模式追加写入
    FILE* ofs2 = fopen(pathCE_exclude_lasthop_mon_file.c_str(), "a"); // 使用 "a" 模式追加写入

    if (!ofs1 || !ofs2) {
        std::cerr << "Error: Unable to open file for writing." << std::endl;
        if (ofs1) fclose(ofs1);
        if (ofs2) fclose(ofs2);
        return;
    }

    if (!setting_debug) {
        // 写入 src 和 dst
        fprintf(ofs1, "%u, %u", src, dst);
        fprintf(ofs2, "%u, %u", src, dst);

        // 写入路径的 CE 和 CEExcludeLast
        for (const auto& path : allPaths) {
            uint32_t pathCE = calculatePathCE(path);
            uint32_t pathCEExcludeLast = calculatePathCEExcludeLast(path);

            fprintf(ofs1, ", %u", pathCE);
            fprintf(ofs2, ", %u", pathCEExcludeLast);
        }

        // 换行符
        fprintf(ofs1, "\n");
        fprintf(ofs2, "\n");
    } else {
        // 写入 src 和 dst
        fprintf(ofs1, "%u, %u\n", src, dst);

        // 写入每个路径和其 CE 和 CEExcludeLast
        for (const auto& path : allPaths) {
            uint32_t pathCE = calculatePathCE(path);
            uint32_t pathCEExcludeLast = calculatePathCEExcludeLast(path);

            fprintf(ofs1, "Path: ");
            for (const auto& node : path) {
                fprintf(ofs1, "%u ", node);
            }
            fprintf(ofs1, "CE: %u, ", pathCE);
            fprintf(ofs1, "CEExcludeLast: %u\n", pathCEExcludeLast);
        }
    }

    // 关闭文件
    fclose(ofs1);
    fclose(ofs2);
}


void Settings::writeCEMapSnapshot(FILE* ofs) {
    if (!ofs) {
        throw std::ios_base::failure("Invalid file pointer.");
    }

    fprintf(ofs, "[");
    bool first = true;
    for (const auto& entry : global_CE_map) {
        if (!first) {
            fprintf(ofs, ",");
        }
        first = false;
        fprintf(ofs, "[%u,%u,%u]", entry.first.first, entry.first.second, entry.second);
    }
    fprintf(ofs, "]\n");
}
void Settings::read_static_path(std::string path){
    std::ifstream infile(path);
    if (!infile.is_open()) {
        std::cerr << "Error: Unable to open file " << path << " for reading." << std::endl;
        return;
    }
    std::string line;
    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        std::vector<uint32_t> path;
        uint32_t node;
        while (iss >> node) {
            path.push_back(node);
            if (iss.peek() == ',') {
                iss.ignore();
            }
        }
        // 假设储存路径的数据结构是 std::vector<std::vector<uint32_t>> static_paths;
        static_paths.push_back(path);
    }
    infile.close();
}
}  // namespace ns3

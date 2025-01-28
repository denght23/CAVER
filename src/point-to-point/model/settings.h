#ifndef __SETINGS_H__
#define __SETINGS_H__

#include <stdbool.h>
#include <stdint.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <tuple>

#include "ns3/callback.h"
#include "ns3/custom-header.h"
#include "ns3/double.h"
#include "ns3/ipv4-address.h"
#include "ns3/net-device.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/string.h"
#include "ns3/tag.h"
#include "ns3/uinteger.h"
#include "ns3/node.h"

namespace ns3 {

#define SLB_DEBUG (false)

#define PARSE_FIVE_TUPLE(ch)                                                    \
    DEPARSE_FIVE_TUPLE(std::to_string(Settings::hostIp2IdMap[ch.sip]),          \
                       std::to_string(ch.udp.sport),                            \
                       std::to_string(Settings::hostIp2IdMap[ch.dip]),          \
                       std::to_string(ch.udp.dport), std::to_string(ch.l3Prot), \
                       std::to_string(ch.udp.seq), std::to_string(ch.GetIpv4EcnBits()))
#define PARSE_REVERSE_FIVE_TUPLE(ch)                                            \
    DEPARSE_FIVE_TUPLE(std::to_string(Settings::hostIp2IdMap[ch.dip]),          \
                       std::to_string(ch.udp.dport),                            \
                       std::to_string(Settings::hostIp2IdMap[ch.sip]),          \
                       std::to_string(ch.udp.sport), std::to_string(ch.l3Prot), \
                       std::to_string(ch.udp.seq), std::to_string(ch.GetIpv4EcnBits()))
#define DEPARSE_FIVE_TUPLE(sip, sport, dip, dport, protocol, seq, ecn)                        \
    sip << "(" << sport << ")," << dip << "(" << dport << ")[" << protocol << "],SEQ:" << seq \
        << ",ECN:" << ecn << ","

#if (SLB_DEBUG == true)
#define SLB_LOG(msg) \
    std::cout << __FILE__ << "(" << __LINE__ << "):" << Simulator::Now() << "," << msg << std::endl
#else
#define SLB_LOG(msg) \
    do {             \
    } while (0)
#endif

/**
 * @brief For flowlet-routing
 */
struct Flowlet {
    Time _activeTime;     // to check creating a new flowlet储存的是上一次到达包的时间
    Time _activatedTime;  // start time of new flowlet
    uint32_t _PathId;     // current pathId
    uint32_t _nPackets;   // for debugging
};
struct DV_Flowlet{
    uint32_t _PathId;
    uint32_t _nPackets;
    bool _SrcRoute_ENABLE;
    uint32_t _outPort; // 对于使用ECMP路由的包，需要记录在Src ToR上出发的端口
};
struct Caver_Flowlet{
    Time _activeTime;     // to check creating a new flowlet储存的是上一次到达包的时间
    Time _activatedTime;  // start time of new flowlet
    uint32_t _PathId;
    uint32_t _nPackets;
    bool _SrcRoute_ENABLE;
    uint32_t _outPort; // 对于使用ECMP路由的包，需要记录在Src ToR上出发的端口
};

struct RouteChoice{
    bool SrcRoute;
    uint32_t outPort;
    uint32_t pathid;
};
struct CEChoice{
    uint32_t _ce;
    uint32_t _path;
    uint32_t _port;
};

struct Interface {
    uint32_t idx;
    bool up;
    uint64_t delay;
    uint64_t bw;

    Interface() : idx(0), up(false) {} //initial 
};

/**
 * @brief Tag for monitoring last data sending time per flow
 */
class LastSendTimeTag : public Tag {
   public:
    LastSendTimeTag() : Tag() {}
    static TypeId GetTypeId(void) {
        static TypeId tid =
            TypeId("ns3::LastSendTimeTag").SetParent<Tag>().AddConstructor<LastSendTimeTag>();
        return tid;
    }
    virtual TypeId GetInstanceTypeId(void) const { return GetTypeId(); }
    virtual void Print(std::ostream &os) const {}
    virtual uint32_t GetSerializedSize(void) const { return sizeof(m_pktType); }
    virtual void Serialize(TagBuffer i) const { i.WriteU8(m_pktType); }
    virtual void Deserialize(TagBuffer i) { m_pktType = i.ReadU8(); }
    void SetPktType(uint8_t type) { m_pktType = type; }
    uint8_t GetPktType() { return m_pktType; }

    enum pktType {
        PACKET_NULL = 0,
        PACKET_FIRST = 1,
        PACKET_LAST = 2,
        PACKET_SINGLE = 3,
    };

   private:
    uint8_t m_pktType;
};

/**
 * @brief Global setting parameters
 */

class Settings {
   public:
    Settings() {}

    static std::pair<std::vector<uint32_t>, uint32_t> FindMinCostPath(uint32_t startNode, uint32_t destNode);
    static void init_nextHop(std::map<uint32_t, std::map<uint32_t, std::vector<uint32_t>>>nextHop);//初始化netHop的函数
    static void init_global_dre_map(uint32_t src_id, uint32_t outPort);//初始化全局的dre_map；
    static void init_nodeInterfaceMap(std::map<uint32_t, std::map<uint32_t, uint32_t>> nodeInterfaceMap);//初始化nodeInterfaceMap
    static void init_nbr2if(std::map<uint32_t, std::map<uint32_t, uint32_t>> nbr2if);//初始化nbr2if
    static void SetLinkCapacity(uint32_t src_id, uint32_t outPort, uint64_t bitRate);//初始化链路带宽
    static void SetDreTime(uint32_t switch_id, Time dreTime);//初始化每个交换机的dre时间
    static void UpdateCETable();//将Dre表转化成CE表
    static void SetCaverQuantizeBit(uint32_t quantizeBit);//设置Caver的量化位数
    static void SetCaverAlpha(double alpha);//设置Caver的alpha值
    static void ShowInit();//显示初始化的信息
    virtual ~Settings() {}

    /* helper function */
    static Ipv4Address node_id_to_ip(uint32_t id);  // node_id -> ip
    static uint32_t ip_to_node_id(Ipv4Address ip);  // ip -> node_id

    /* conweave params */
    static const uint32_t CONWEAVE_CTRL_DUMMY_INDEV = 88888888;  // just arbitrary

    /* load balancer */
    // 0: flow ECMP, 2: DRILL, 3: Conga, 4: ConWeave
    static uint32_t lb_mode;

    // for common setting
    static uint32_t packet_payload;

    // for statistic
    static uint32_t node_num;
    static uint32_t host_num;
    static uint32_t switch_num;
    static uint64_t cnt_finished_flows;  // number of finished flows (in qp_finish())

    struct tuple_hash {
        template <class T1, class T2, class T3, class T4>
        std::size_t operator () (const std::tuple<T1, T2, T3, T4>& tuple) const {
            auto hash1 = std::hash<T1>{}(std::get<0>(tuple));
            auto hash2 = std::hash<T2>{}(std::get<1>(tuple));
            auto hash3 = std::hash<T3>{}(std::get<2>(tuple));
            auto hash4 = std::hash<T4>{}(std::get<3>(tuple));
            return hash1 ^ (hash2 << 1) ^ (hash3 << 2) ^ (hash4 << 3); // 使用位运算合并哈希
        }
    };

    /* The map between hosts' IP and ID, initial when build topology */
    static std::map<uint32_t, uint32_t> hostIp2IdMap;
    static std::map<uint32_t, uint32_t> hostId2IpMap;
    static std::map<uint32_t, uint32_t> hostIp2SwitchId;  // host's IP -> connected Switch's Id

    // 一个2维数组，每个位置存放一个uint32_t，数组的大小为node_num*node_num
    //dive into related:计算最优所需要的信息
    // static std::map<uint32_t, std::map<uint32_t, std::vector<uint32_t>>> nextHop;
    static std::map<std::pair<uint32_t, uint32_t>, uint32_t> global_dre_map;
    static std::map<std::pair<uint32_t, uint32_t>, uint32_t> global_CE_map;//给定链路的源和目的节点，返回链路的CE值
    static std::map<std::pair<uint32_t, uint32_t>, uint64_t> global_linkwidth; //给定链路的源和目的节点，返回链路带宽
    static std::map<uint32_t, std::map<uint32_t, uint32_t>> m_nodeInterfaceMap;//给定本节点的id 以及接口的id，返回邻居节点的id
    static std::map<uint32_t, std::map<uint32_t, uint32_t>> m_nbr2if;//给定本节点的id 以及邻居节点的id，返回接口的id
    static std::map<uint32_t, std::map<uint32_t, std::vector<uint32_t>>> m_nextHop;//对于每个节点，到每个目的地的下一跳的节点id
    static std::map<uint32_t, Time> Dre_time_map; //记录每个交换机的DRE时间
    static uint32_t caver_quantizeBit;
    static double caver_alpha;

    // 与motivation的全部路径的pathCE有关
    static bool motivation_pathCE;     
    static std::string pathCE_mon_file;
    static std::string pathCE_exclude_lasthop_mon_file;
    static uint32_t calculatePathCE(const std::vector<uint32_t>& path);// 辅助函数：计算路径的PathCE
    static uint32_t calculatePathCEExcludeLast(const std::vector<uint32_t>& path);// 辅助函数：计算路径的PathCE（不包括最后一跳）
    static void findAllPaths(uint32_t src, uint32_t dst, std::vector<std::vector<uint32_t>>& allPaths);// 辅助函数：通过BFS找到所有路径
    static void savePathCEs(uint32_t src, uint32_t dst);// 主函数：计算并保存路径CE值
    static void writeCEMapSnapshot(FILE* ofs);//将global_CE_map追加到txt文件的一行
    static bool isBond;

    static std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> flowId2SrcDst; //流的id对应源Torid和目的Torid
    static std::unordered_map<uint32_t, uint32_t> flowId2Port2Src; //流的id对应源Torid所需要选择的出端口
    static std::map<uint32_t, std::vector<uint32_t>>TorSwitch_nodelist; //记录每个ToR交换机下的节点 ip列表
    static std::map<uint32_t, std::vector<uint32_t>>hostId2ToRlist; //记录每个节点相连的ToR交换机id的vector

    static std::unordered_map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, uint32_t, tuple_hash> PacketId2FlowId;
    //例子：flow_id = Settings::PacketId2FlowId[std::make_tuple(Settings::hostIp2IdMap[ch.sip], Settings::hostIp2IdMap[ch.dip], ch.udp.sport, ch.udp.dport)];
    static std::map<std::tuple<ns3::Ipv4Address, ns3::Ipv4Address, uint16_t, uint16_t>, uint32_t>QPPair_info2FlowId;
    static std::unordered_map<uint32_t, uint32_t> FlowId2SrcId; // 可能可以删除
    static std::unordered_map<uint32_t, uint32_t> FlowId2Length;
    static FILE* caverLog;
    
    static std::map<Ptr<Node>, std::map<uint32_t, uint32_t> > if2id;

    static uint32_t dropped_pkt_sw_ingress;
    static uint32_t dropped_pkt_sw_egress;

    static bool setting_debug;

    static bool set_fixed_routing;//选择固定的路由；
    static void read_static_path(std::string path);//读取固定的路由表
    static std::vector<std::vector<uint32_t>> static_paths;
    
    static void record_flow_distribution(Ptr<Packet> p, CustomHeader &ch, Ptr<Node> srcNode, uint32_t outDev);
    static void print_flow_distribution(FILE *out, Time nextTime);

    static uint32_t dropped_flow_id;

   private:
    
    static std::unordered_map<uint64_t, std::unordered_map<uint32_t, Time>> flowRecorder; //(src,dst)->(flow_id, active_time)
    struct LinkRecord {
        uint32_t total_size=0;
        struct FlowRecord {
            uint32_t seq_start;
            uint32_t seq_end;
            FlowRecord(uint32_t seq): seq_start(seq), seq_end(seq) {};
            FlowRecord() = default;
        };
        std::unordered_map<uint32_t, FlowRecord> flowRecorder;
    };
    static std::unordered_map<uint64_t, struct LinkRecord> linkRecorder; 
};

}  // namespace ns3

#endif

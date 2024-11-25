// CAVER code
#ifndef __CAVER_ROUTING_H__
#define __CAVER_ROUTING_H__

#include <arpa/inet.h>

#include <map>
#include <queue>
#include <unordered_map>
#include <vector>
#include <list>

#include "ns3/address.h"
#include "ns3/callback.h"
#include "ns3/event-id.h"
#include "ns3/net-device.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/settings.h"
#include "ns3/simulator.h"
#include "ns3/tag.h"

namespace ns3 {

const uint32_t CAVER_NULL = UINT32_MAX;

struct bestCaverInfo{
    uint32_t _ce;
    std::vector<uint8_t> _path;
    Time _updateTime;
    bool _valid;
    uint32_t _inPort;
};//表示中间交换机以及tor上储存的最优路径的表项
struct PathChoiceInfo{
    std::vector<uint8_t> _path;
    Time _updateTime;
    bool _is_used;
};// 表示tor上储存的路径表的表项



class CaverUdpTag : public Tag {
   public:
    CaverUdpTag();
    ~CaverUdpTag();
    static TypeId GetTypeId(void);
    void SetPathId(uint32_t pathId);
    uint32_t GetPathId(void) const;
    void SetHopCount(uint32_t hopCount);
    uint32_t GetHopCount(void) const;
    void SetSrcRouteEnable(bool SrcRouteEnable);
    uint8_t GetSrcRouteEnable(void) const;
    virtual TypeId GetInstanceTypeId(void) const;
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize(TagBuffer i) const;
    virtual void Deserialize(TagBuffer i);
    virtual void Print(std::ostream& os) const;

   private:
    uint32_t m_pathId;    // forward
    uint32_t m_hopCount;  // hopCount to get outPort
    uint8_t m_SrcRouteEnable;  //若为True，表示使用pathid来进行源路由，若为False，则使用ECMP进行路由
};

class CaverAckTag : public Tag{
    public:
        CaverAckTag();
        ~CaverAckTag();
        static TypeId GetTypeId(void);
        void SetMPathId(uint32_t pathId);
        uint32_t GetMPathId(void) const;
        void SetMCE(uint32_t ce);
        uint32_t GetMCE(void) const;
        void SetBestPathId(uint32_t pathId);
        uint32_t GetBestPathId(void) const;
        void SetBestCE(uint32_t ce);
        uint32_t GetBestCE(void) const;
        void SetLength(uint8_t length);
        uint8_t GetLength(void) const;
        void SetLastSwitchId(uint32_t last_switch_id);
        uint32_t GetLastSwitchId(void) const;
        uint32_t GetHostId(void) const;
        void SetHostId(uint32_t host_id);
        virtual TypeId GetInstanceTypeId(void) const;
        virtual uint32_t GetSerializedSize(void) const;
        virtual void Serialize(TagBuffer i) const;
        virtual void Deserialize(TagBuffer i);
        virtual void Print(std::ostream& os) const;
    private:
        uint32_t m_pathId;    // forward
        uint32_t m_ce;  // hopCount to get outPort
        uint8_t m_length;
        uint32_t best_pathId;
        uint32_t best_ce;
        uint32_t m_last_switch_id;
        uint32_t m_host_id;
};

class CaverRouting : public Object {

    friend class SwitchMmu;
    friend class SwitchNode;

    public:
    CaverRouting();
    /* static */
    static TypeId GetTypeId(void);
    static uint64_t GetQpKey(uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg);              // same as in rdma_hw.cc
    static uint32_t GetOutPortFromPath(const uint32_t& path, const uint32_t& hopCount);               // decode outPort from path, given a hop's order
    // static void SetOutPortToPath(uint32_t& path, const uint32_t& hopCount, uint32_t& outPort);  // encode outPort to path
    static uint32_t nFlowletTimeout;     // number of flowlet's timeout


    /* main function */
    void RouteInput(Ptr<Packet> p, CustomHeader ch);
    uint32_t UpdateLocalDre(Ptr<Packet> p, CustomHeader ch, uint32_t outPort);
    uint32_t QuantizingX(uint32_t outPort, uint32_t X);  // X is bytes here and we quantizing it to 0 - 2^Q
    virtual void DoDispose();
    uint32_t mergePortAndVector(uint8_t port, const std::vector<uint8_t> vec);
    uint32_t Vector2PathId(std::vector<uint8_t> vec);
    std::vector<uint8_t> uint32_to_uint8(uint32_t number);

    std::map<uint32_t, uint32_t> id2Port;//维护一个交换机的邻居id到端口的id的映射

    RouteChoice ChoosePath(uint32_t dip, CustomHeader ch);//从PathChoiceTable中选择一个路径

    /*-----CALLBACK------*/
    void DoSwitchSend(Ptr<Packet> p, CustomHeader& ch, uint32_t outDev,
                      uint32_t qIndex);  // TxToR and Agg/CoreSw
    void DoSwitchSendToDev(Ptr<Packet> p, CustomHeader& ch);  // only at RxToR
    typedef Callback<void, Ptr<Packet>, CustomHeader&, uint32_t, uint32_t> SwitchSendCallback;
    typedef Callback<void, Ptr<Packet>, CustomHeader&> SwitchSendToDevCallback;
    void SetSwitchSendCallback(SwitchSendCallback switchSendCallback);  // set callback
    void SetSwitchSendToDevCallback(
        SwitchSendToDevCallback switchSendToDevCallback);  // set callback
    /*-----------*/

    /* SET functions */
    void SetConstants(Time dreTime, Time agingTime, Time flowletTimeout, uint32_t quantizeBit, double alpha, double ce_threshold, Time patchoiceTimeout, uint32_t pathChoice_num);
    void SetSwitchInfo(bool isToR, uint32_t switch_id);
    void SetLinkCapacity(uint32_t outPort, uint64_t bitRate);

    // periodic events
    EventId m_dreEvent;
    EventId m_agingEvent;
    void DreEvent();
    void AgingEvent();
    // topological info (should be initialized in the beginning)
    std::map<uint32_t, uint64_t> m_outPort2BitRateMap;       
    // std::map<uint32_t, std::map<uint32_t, DVInfo> > m_DVTable;  // (node ip, port)-> DVInfo
    // *******************************Add begin**********************//
    std::map<uint32_t, bestCaverInfo> best_pathCE_Table; //中间交换机以及tor上的最优路径表
    std::map<uint32_t, bestCaverInfo> acceptable_path_table;//中间交换机上的路径交换表
    std::unordered_map<uint32_t, std::vector<PathChoiceInfo>> PathChoiceTable; //tor交换机上储存的路径表
    std::unordered_map<uint32_t, uint32_t> PathChoiceFlagMap; //针对每个目的地，收到的新路径储存在pathChoiceTable的哪个地方
    // 表项显示相关函数
    void printBestPathCETable_Entry(uint32_t dip);
    void printBestPathCETable();
    void printPathChoiceTable_Entry(uint32_t dip);
    void printPathChoiceTable();
    void printPathChoiceFlagMap_Entry(uint32_t dip);
    void showPathChoiceInfo(PathChoiceInfo pc);
    void printAcceptablePathTable();
    void printAcceptablePathTable_Entry(uint32_t dip);
    void printPathChoiceFlagMap();
    void showCaverAck_info(CaverAckTag ackTag, CustomHeader ch);
    void showAck_info(CustomHeader ch);
    void showRouteChoice(RouteChoice rc);
    void showCaverUdpinfo(CaverUdpTag udpTag);
    void showDreTable();
    void showPortCE(uint32_t port);
    void showPathVec(vector<uint8_t>path);

    //性能监控相关的函数
    void UpdateGlobalDre(Ptr<Packet> p, uint32_t outPort);
    void DecreaseGlobalDre();//将本交换机连接的端口的dre值减小

    //性能分析监控的log
    bool Dive_optimal_log = true;
    //log
    bool DreTable_log = true;
    bool ACK_log = true;
    bool AccceptablePath_log = true;//记录与acceptable table更新相关的log
    bool Route_log = true;//src进行路由选择时的log
    bool Nodepass_log = true;//数据包经过节点时的log
    bool BestTable_log = true;//与BestTable更新相关的log
    bool PathChoice_log = true;//与PathChoiceTable更新相关的log
    bool Packet_begin_end_flag = true;//数据包的开始和结束标志

    bool Error_log = false;
    bool Dre_decrease_log = false;
    bool flowlet_log = true; //在flowlet过期时打印的log
    //method
    bool ToR_Rouding = true;
    bool multi_PathSet = false;


    uint32_t m_pathChoice_num; //pathCHoiceTable每个目的地存放的路径数量

    private:
        SwitchSendCallback m_switchSendCallback;  // bound to SwitchNode::SwitchSend (for Request/UDP)
        SwitchSendToDevCallback m_switchSendToDevCallback;  // bound to SwitchNode::SendToDevContinue (for Probe, Reply)

        // topology parameters
        bool m_isToR;          // is ToR (leaf)
        uint32_t m_switch_id;  // switch's nodeID      

        // dv constants  
        Time m_dreTime;          // dre alogrithm (e.g., 200us)
        Time m_agingTime;        // dre algorithm (e.g., 10ms)
        Time m_flowletTimeout;   // flowlet timeout (e.g., 1ms)
        Time m_patchoiceTimeout; // PathChoice表项的过期时间
        
        uint32_t m_quantizeBit;  // quantizing (2**X) param (e.g., X=3)
        double m_alpha;          // dre algorithm (e.g., 0.2)

        // local
        std::map<uint32_t, uint32_t> m_DreMap;        // outPort -> DRE (at SrcToR)
        std::map<uint64_t, Caver_Flowlet*> m_flowletTable;  // QpKey -> Flowlet (at SrcToR)

        uint32_t host_round_index;
        uint32_t ToR_host_num;

        double m_ce_threshold; // ce阈值，应该是一个大于1的数
};

}

#endif

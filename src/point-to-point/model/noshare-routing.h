// NOSHARE code
#ifndef __NOSHARE_ROUTING_H__
#define __NOSHARE_ROUTING_H__

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
#include "ns3/caver-routing.h"

namespace ns3 {

class NoshareRouting : public Object {

    friend class SwitchMmu;
    friend class SwitchNode;

    public:
    NoshareRouting();
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

    CaverRouteChoice ChoosePath(uint32_t dip, CustomHeader ch);//从PathChoiceTable中选择一个路径

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
    void SetConstants(Time dreTime, Time agingTime, Time flowletTimeout, uint32_t quantizeBit, double alpha, double ce_threshold, Time patchoiceTimeout, uint32_t pathChoice_num, Time tau, bool useEWMA);
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
    void showRouteChoice(CaverRouteChoice rc);
    void showCaverUdpinfo(CaverUdpTag udpTag);
    void showDreTable();
    void showglobalDreTable();
    void showPortCE(uint32_t port);
    void showPathVec(std::vector<uint8_t>path);
    void showOptimalvsCaver(CustomHeader ch, CaverRouteChoice caver);
    int getRandomElement(const std::list<int>& myList);

    //性能监控相关的函数
    std::vector<uint32_t> getPathNodeIds(const std::vector<uint8_t>& pathVec, uint32_t currentNodeId);//将pathVec转化为nodeIdVec

    //性能分析监控的log
    bool Dive_optimal_log = false;//每个流到来的时候计算最优路径的CE以及选择路径的CE值
    //log
    bool DreTable_log = false;
    bool ACK_log = false;
    bool AccceptablePath_log = false;//记录与acceptable table更新相关的log
    bool Route_log = false;//src进行路由选择时的log
    bool Nodepass_log = false;//数据包经过节点时的log
    bool BestTable_log = false;//与BestTable更新相关的log
    bool PathChoice_log = false;//与PathChoiceTable更新相关的log
    bool Packet_begin_end_flag = false;//数据包的开始和结束标志
    bool Caver_debug = false;
    bool Dre_decrease_log = false;//DreTable中的X值的减小情况
    bool flowlet_log = false; //在flowlet过期时打印的log
    bool Dre_debug = false; //查看Dre，尤其是CE的计算是否存在bug
    bool Error_log = false; //打印错误信息

    //method
    bool ToR_Rouding = false;


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
        bool useEWMA;
        Time tau = MicroSeconds(100);
        std::map<uint32_t, Time> m_Port2UpdateTime;

        // local
        std::map<uint32_t, uint32_t> m_DreMap;        // outPort -> DRE (at SrcToR)
        std::map<uint64_t, Caver_Flowlet*> m_flowletTable;  // QpKey -> Flowlet (at SrcToR)

        uint32_t host_round_index;
        uint32_t ToR_host_num;

        double m_ce_threshold; // ce阈值，应该是一个大于1的数
};

}

#endif

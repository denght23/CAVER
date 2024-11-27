// CAVER code
#ifndef __DV_ROUTING_H__
#define __DV_ROUTING_H__

#include <arpa/inet.h>

#include <map>
#include <queue>
#include <unordered_map>
#include <vector>

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

const uint32_t DV_NULL = UINT32_MAX;

struct DVInfo {
    uint32_t _ce;
    std::vector<uint8_t> _path;
    Time _updateTime;
    bool _valid;
};
struct singleDVInfo{
    uint32_t _ce;
    std::vector<uint8_t> _path;
    Time _updateTime;
    bool _valid;
    uint32_t _inPort;
};

class DVUdpTag : public Tag {
   public:
    DVUdpTag();
    ~DVUdpTag();
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

class DVAckTag : public Tag{
    public:
        DVAckTag();
        ~DVAckTag();
        static TypeId GetTypeId(void);
        void SetPathId(uint32_t pathId);
        uint32_t GetPathId(void) const;
        void SetCE(uint32_t ce);
        uint32_t GetCE(void) const;
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
        uint32_t m_last_switch_id;
        uint32_t m_host_id;
};

class DVRouting : public Object {

    friend class SwitchMmu;
    friend class SwitchNode;

    public:
    DVRouting();
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
    RouteChoice GetBestPath(uint32_t dip, CustomHeader ch); 
    CEChoice GetKnownBestPath(uint32_t dip);
    uint32_t mergePortAndVector(uint8_t port, const std::vector<uint8_t> vec);
    // *******************************Add begin**********************//
    RouteChoice GetBestPath_PathCE_port_table(uint32_t dip, CustomHeader ch);
    uint32_t Vector2PathId(std::vector<uint8_t> vec);
    // *******************************Add end**********************//
    std::vector<uint8_t> uint32_to_uint8(uint32_t number);

    std::map<uint32_t, uint32_t> id2Port;//维护一个交换机的邻居id到端口的id的映射

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
    void SetConstants(Time dreTime, Time agingTime, Time flowletTimeout, uint32_t quantizeBit, double alpha);
    void SetSwitchInfo(bool isToR, uint32_t switch_id);
    void SetLinkCapacity(uint32_t outPort, uint64_t bitRate);

    // periodic events
    EventId m_dreEvent;
    EventId m_agingEvent;
    void DreEvent();
    void AgingEvent();
    // topological info (should be initialized in the beginning)
    std::map<uint32_t, uint64_t> m_outPort2BitRateMap;       
    std::map<uint32_t, std::map<uint32_t, DVInfo> > m_DVTable;  // (node ip, port)-> DVInfo
    // *******************************Add begin**********************//
    std::map<uint32_t, singleDVInfo> PathCE_Table;
    std::map<uint32_t, std::map<uint32_t, DVInfo>> PathCE_port_Table;
    // *******************************Add end**********************//

    //log
    bool DreTable_log = false;
    bool ACK_log = false;
    bool Route_log = false;
    bool Nodepass_log = false;
    bool Error_log = false;
    bool Dre_decrease_log = false;
    //method
    bool ToR_Rouding = true;
    bool multi_PathSet = true;

    int  pathUpdateTimes = 0;
    static std::vector<DVRouting*> dvModules;

    // topology parameters
    bool m_isToR;          // is ToR (leaf)
    uint32_t m_switch_id;  // switch's nodeID  
    private:
        SwitchSendCallback m_switchSendCallback;  // bound to SwitchNode::SwitchSend (for Request/UDP)
        SwitchSendToDevCallback m_switchSendToDevCallback;  // bound to SwitchNode::SendToDevContinue (for Probe, Reply)
    

        // dv constants  
        Time m_dreTime;          // dre alogrithm (e.g., 200us)
        Time m_agingTime;        // dre algorithm (e.g., 10ms)
        Time m_flowletTimeout;   // flowlet timeout (e.g., 1ms)
        
        uint32_t m_quantizeBit;  // quantizing (2**X) param (e.g., X=3)
        double m_alpha;          // dre algorithm (e.g., 0.2)

        // local
        std::map<uint32_t, uint32_t> m_DreMap;        // outPort -> DRE (at SrcToR)
        std::map<uint64_t, DV_Flowlet*> m_flowletTable;  // QpKey -> Flowlet (at SrcToR)

        uint32_t host_round_index;
        uint32_t ToR_host_num;


};

}

#endif

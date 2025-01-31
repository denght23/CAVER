/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MIT License
 * 
 * Copyright (c) 2025 CAVER-LB
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
};// Represents the best path entries stored on intermediate switches and tor.

struct PathChoiceInfo{
    std::vector<uint8_t> _path;
    Time _updateTime;
    bool _is_used;
    // The purpose of the following items is for comparison with the optimal path
    uint32_t _remoteCE;
};// Represents the path table entries stored on the tor.

struct CaverRouteChoice{
    bool SrcRoute;
    uint32_t outPort;
    uint32_t pathid;
    // The purpose of the following two items is for comparison with the optimal path
    std::vector<uint8_t> pathVec;
    uint32_t remoteCE;
};// Caver source tor path selection result

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
    uint8_t m_SrcRouteEnable;  // If True, source routing is used with pathid; if False, ECMP routing is used.
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

    std::map<uint32_t, uint32_t> id2Port;// Maintains a mapping from switch's neighbor id to port id

    CaverRouteChoice ChoosePath(uint32_t dip, CustomHeader ch);// Select a path from PathChoiceTable
    CaverRouteChoice ChoosePathWithDetail(uint32_t dip, CustomHeader ch);
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
    void SetConstants(Time dreTime, 
                      Time agingTime, 
                      Time flowletTimeout, 
                      uint32_t quantizeBit, 
                      double alpha, 
                      double ce_threshold, 
                      Time patchoiceTimeout, 
                      uint32_t pathChoice_num, 
                      Time tau, 
                      bool useEWMA);
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
    std::map<uint32_t, bestCaverInfo> best_pathCE_Table; // Best path table on intermediate switches and tor
    std::map<uint32_t, bestCaverInfo> acceptable_path_table;// Path exchange table on intermediate switches
    std::unordered_map<uint32_t, std::vector<PathChoiceInfo>> PathChoiceTable; // Path table stored on tor switches
    std::unordered_map<uint32_t, uint32_t> PathChoiceFlagMap; // For each destination, where the newly received paths are stored in the pathChoiceTable
    // Display-related functions
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
    // Performance monitoring-related functions
    std::vector<uint32_t> getPathNodeIds(const std::vector<uint8_t>& pathVec, uint32_t currentNodeId);// Convert pathVec to nodeIdVec

    // Performance analysis monitoring log
    bool Dive_optimal_log = false;
    //log
    bool DreTable_log = false;
    bool ACK_log = false;
    bool AccceptablePath_log = false;// Log related to acceptable table updates
    bool Route_log = false;// Log during src routing selection
    bool Nodepass_log = false;// Log when the packet passes through nodes
    bool BestTable_log = false;// Log related to BestTable updates
    bool PathChoice_log = false;// Log related to PathChoiceTable updates
    bool Packet_begin_end_flag = false;// Flag for packet start and end
    bool Caver_debug = false;

    bool Error_log = false;
    bool Dre_decrease_log = false;
    bool flowlet_log = false; // Log when flowlet expires
    bool show_pathchoice_detail = false;// Display detailed information from PathChoiceTable
    bool show_acceptable_detail = false;// Display detailed information from acceptable table
    //method
    bool ToR_Rouding = true;

    uint32_t m_pathChoice_num; // Number of paths stored in each destination of the PathChoiceTable

    private:
        SwitchSendCallback m_switchSendCallback;  // bound to SwitchNode::SwitchSend (for Request/UDP)
        SwitchSendToDevCallback m_switchSendToDevCallback;  // bound to SwitchNode::SendToDevContinue (for Probe, Reply)

        // topology parameters
        bool m_isToR;          // is ToR (leaf)
        uint32_t m_switch_id;  // switch's nodeID      

        // dv constants  
        Time m_agingTime;        // dre algorithm (e.g., 10ms)
        Time m_flowletTimeout;   // flowlet timeout (e.g., 1ms)
        Time m_patchoiceTimeout; // Expiration time of PathChoice table entries
        
        //CE calculation method
        uint32_t m_quantizeBit;  // quantizing (2**X) param (e.g., X=3)

        bool useEWMA;
        Time tau = MicroSeconds(100);
        std::map<uint32_t, Time> m_Port2UpdateTime;

        double m_alpha;          // dre algorithm (e.g., 0.2)
        Time m_dreTime;          // dre algorithm (e.g., 200us)

        // local
        std::map<uint32_t, uint32_t> m_DreMap;        // outPort -> DRE (at SrcToR)
        std::map<uint64_t, Caver_Flowlet*> m_flowletTable;  // QpKey -> Flowlet (at SrcToR)

        uint32_t host_round_index;
        uint32_t ToR_host_num;

        double m_ce_threshold; // CE threshold, should be a number greater than 1
};

}

#endif

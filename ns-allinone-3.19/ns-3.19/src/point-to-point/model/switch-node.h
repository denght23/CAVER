#ifndef SWITCH_NODE_H
#define SWITCH_NODE_H

#include <ns3/node.h>

#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include "qbb-net-device.h"
#include "switch-mmu.h"
#include "ns3/settings.h"
#include "ns3/dv-routing.h"

namespace ns3 {

class Packet;

//TODO: my 5 tripe:

class SwitchNode : public Node {
    static const unsigned qCnt = 8;    // Number of queues/priorities used
    static const unsigned pCnt = 128;  // port 0 is not used so + 1	// Number of ports used
    uint32_t m_ecmpSeed;
    std::unordered_map<uint32_t, std::vector<int> >
        m_rtTable;  // map from ip address (u32) to possible ECMP port (index of dev)

    // monitor uplinks
    uint64_t m_txBytes[pCnt];  // counter of tx bytes, for HPCC
    uint64_t m_rxBytes[pCnt];  // counter of rx bytes, for HPCC
    std::unordered_map<uint32_t, uint64_t> flow_bytes; 
   protected:
    bool m_ecnEnabled;
    uint32_t m_ccMode;
    uint32_t m_ackHighPrio;  // set high priority for ACK/NACK

   private:
    int GetOutDev(Ptr<Packet>, CustomHeader &ch);
    void SendToDev(Ptr<Packet> p, CustomHeader &ch);
    void SendToDevContinue(Ptr<Packet> p, CustomHeader &ch);
    static uint32_t EcmpHash(const uint8_t *key, size_t len, uint32_t seed);
    void CheckAndSendPfc(uint32_t inDev, uint32_t qIndex);
    void CheckAndSendResume(uint32_t inDev, uint32_t qIndex);

    /* Sending packet to Egress port */
    void DoSwitchSend(Ptr<Packet> p, CustomHeader &ch, uint32_t outDev, uint32_t qIndex);

    /*----- Load balancer -----*/
    // Flow ECMP (lb_mode = 0)
    uint32_t DoLbFlowECMP(Ptr<const Packet> p, const CustomHeader &ch,
                          const std::vector<int> &nexthops);
    // DRILL (lb_mode = 2)
    uint32_t DoLbDrill(Ptr<const Packet> p, const CustomHeader &ch,
                       const std::vector<int> &nexthops);     // choose egress port
    uint32_t m_drill_candidate;                               // always 2 (power of two)
    std::map<uint32_t, uint32_t> m_previousBestInterfaceMap;  // <dip, previousBestInterface>
    uint32_t CalculateInterfaceLoad(uint32_t interface);      // Get the load of a interface
    // Conga (lb_mode = 3)
    uint32_t DoLbConga(Ptr<Packet> p, CustomHeader &ch, const std::vector<int> &nexthops);
    // Conga (lb_mode = 6)
    uint32_t DoLbDV(Ptr<Packet> p, CustomHeader &ch, const std::vector<int> &nexthops);
    uint32_t DoLbLetflow(Ptr<Packet> p, CustomHeader &ch, const std::vector<int> &nexthops);
    // ConWeave (lb_mode = 9)
    uint32_t DoLbConWeave(Ptr<const Packet> p, const CustomHeader &ch,
                           const std::vector<int> &nexthops);  // dummy

   public:
    // Ptr<BroadcomNode> m_broadcom;
    Ptr<SwitchMmu> m_mmu;
    bool m_isToR;                                 // true if ToR switch
    std::unordered_set<uint32_t> m_isToR_hostIP;  // host's IP connected to this ToR

    //TODO: my code to add a file to store the packet header
    std::set<std::string> headSet;
    FILE* m_filePtr = nullptr;
    void setFilePointer(FILE* filePtr) {
        m_filePtr = filePtr;
    }
    // void writePacketInfo(uint32_t id, uint32_t srcip, uint32_t dstip, uint16_t srcport, uint16_t dstport) {
    //     // if (m_filePtr != nullptr) {
    //     //     fprintf(m_filePtr, "%u %u %u %u %u \n", 
    //     //             id, Settings::ip_to_node_id(Ipv4Address(srcip)), Settings::ip_to_node_id(Ipv4Address(dstip)), srcport, dstport);
    //     // }
    //     printf("%u %u %u %u %u\n", id, srcip, dstip, srcport, dstport); 

    // }

    static TypeId GetTypeId(void);
    SwitchNode();
    void SetEcmpSeed(uint32_t seed);
    void AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx);
    void AddDVTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx, Time now);
    // *******************************Add begin**********************//
    void AddPathCETableEntry(Ipv4Address &dstAddr, Time now);
    // *******************************Add end**********************//
    void ClearTable();
    bool SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch);
    void SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p);
    uint64_t GetTxBytesOutDev(uint32_t outdev);
    uint64_t GetRxBytesOutDev(uint32_t outdev);
    std::unordered_map<uint32_t, uint64_t> GetFlowBytes();

};

} /* namespace ns3 */



#endif /* SWITCH_NODE_H */

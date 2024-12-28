/*******************************************************************************
 *  INTEL CONFIDENTIAL
 *
 *  Copyright (c) 2021 Intel Corporation
 *  All Rights Reserved.
 *
 *  This software and the related documents are Intel copyrighted materials,
 *  and your use of them is governed by the express license under which they
 *  were provided to you ("License"). Unless the License provides otherwise,
 *  you may not use, modify, copy, publish, distribute, disclose or transmit
 *  this software or the related documents without Intel's prior written
 *  permission.
 *
 *  This software and the related documents are provided as is, with no express
 *  or implied warranties, other than those that are expressly stated in the
 *  License.
 ******************************************************************************/


#ifndef _HEADERS_
#define _HEADERS_

typedef bit<48> mac_addr_t;
typedef bit<32> ipv4_addr_t;
typedef bit<8> port_t;
typedef bit<8> CE_t;
typedef bit<8> table_CE_t;
typedef bit<32> time_t;
typedef bit<32> DRE_t;
typedef bit<8> shift_t;
typedef bit<8> host_t;
typedef bit<32> flow_t;

typedef bit<16> ether_type_t;
const ether_type_t ETHERTYPE_IPV4 = 16w0x0800;

typedef bit<8> ip_protocol_t;
const ip_protocol_t IP_PROTOCOLS_ICMP = 1;
const ip_protocol_t IP_PROTOCOLS_TCP = 6;
const ip_protocol_t IP_PROTOCOLS_UDP = 17;
const ip_protocol_t IP_PROTOCOLS_ACK = 0x43;
const ip_protocol_t IP_PROTOCOLS_CAVER_ACK = 0x44;
const ip_protocol_t IP_PROTOCOLS_CAVER_DATA = 0x42;


// CAVER相关常数
const bit<32> HOSTNUM = 0x64; // host上限数量
const bit<32> FLOWNUM = 1024; // flow上限数量
const bit<32> PORTNUM = 0x64; // port数量
const time_t PATHCE_TOUT = 32w1 << 3;
const time_t FLOWLET_TOUT = 32w1 << 3;
const time_t CE_reduce_gap = 256; //微秒
const shift_t CE_reduce_gap_math = 8;
//CE_reduce_gap = 2 ** CE_reduce_gap_math
const shift_t CE_reduce_alpha = 2;
const DRE_t alpha_con = 3; 
//alpha = 0.25
//x *(1-alpha) = (x >> CE_reduce_alpha) * alpha_con
const shift_t link_rate = 3; 
const shift_t m_quantizeBit = 3;
// 2**link_rate  (Mbps)

header ethernet_h {
    mac_addr_t dst_addr;
    mac_addr_t src_addr;
    bit<16> ether_type;
}


header ipv4_h {
    bit<4> version;
    bit<4> ihl;
    bit<8> diffserv;
    bit<16> total_len;
    bit<16> identification;
    bit<3> flags;
    bit<13> frag_offset;
    bit<8> ttl;
    bit<8> protocol;
    bit<16> hdr_checksum;
    ipv4_addr_t src_addr;
    ipv4_addr_t dst_addr;
}
header tcp_h {
    bit<16> src_port;
    bit<16> dst_port;
    bit<32> seq_no;
    bit<32> ack_no;
    bit<4> data_offset;
    bit<4> res;
    bit<8> flags;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgent_ptr;
}

header udp_h {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<16> hdr_length;
    bit<16> checksum;
}



header ACK_t{
    bit<16> srcPort;
    bit<16> dstPort;
    bit<16> len;
    bit<16> checksum;
    bit<8> seq;
}

header CAVER_data_t{
    bit<16> srcPort;
    bit<16> dstPort;
    bit<16> len;
    bit<16> checksum;

    port_t hop_0;
    port_t hop_1;
    port_t hop_2;
    port_t hop_3;
    bit<2> hopCount;
    bit<1> SrcRoute;
    bit<5> padding;
}

header CAVER_ack_t{
    bit<16> srcPort;
    bit<16> dstPort;
    bit<16> len;
    bit<16> checksum;
    bit<8> seq; 
    
    port_t hop_0;
    port_t hop_1;
    port_t hop_2;
    port_t hop_3;
    CE_t pathCE;
}

struct header_t {
    ethernet_h ethernet;
    ipv4_h ipv4;
    tcp_h tcp;
    udp_h udp;
    ACK_t       ack;
    CAVER_data_t caver_data;
    CAVER_ack_t caver_ack;

    // Add more headers here.
}
header resubmit_info{
    host_t host_id;
    port_t eport;
    CE_t localCE;
    port_t CE_port;

    bit<1> m_isSrcToR;
    bit<1> m_isDstToR;
    bit<1> same_port;//判断ACK的ingress的port和目前储存的port是否一致
    bit<1> SrcRoute_Valid;
    bit<4> padding;
}
struct metadata_t {
    time_t current_time;
    ipv4_addr_t Addr;
    flow_t flow_id;

    bit<16>srcPort;
    bit<16>dstPort;
    bit<16> len;
    bit<16> checksum;
    bit<8> seq; 

    DRE_t localDre;
    CE_t newPathCE;
    CE_t pathCE;

    port_t SrcRoute_hop_0;
    port_t SrcRoute_hop_1;
    port_t SrcRoute_hop_2;
    port_t SrcRoute_hop_3;

    port_t outPort;

    bit<1> is_data;
    bit<1> isACK;
    bit<1> isCAVER_data;
    bit<1> isCAVER_ack;
    bit<1> isUDP;
    bit<1> byPass; //直接转发的逻辑，会作为后面所有表项的key，如果byPass = 1，则noaction
    bit<1> new_info;
    bit<1> updateCE;
    bit<1> flow_valid;
    bit<1> CE_reduce; 

    bit<5> padding;


    resubmit_info info;

}

struct empty_header_t {}

struct empty_metadata_t {}

#endif /* _HEADERS_ */

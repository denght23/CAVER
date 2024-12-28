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

// same as "tna_register.p4"

#include <core.p4>
#if __TARGET_TOFINO__ == 3
#include <t3na.p4>
#elif __TARGET_TOFINO__ == 2
#include <t2na.p4>
#else
#include <tna.p4>
#endif

#include "common/headers.p4"
#include "common/util.p4"


// ---------------------------------------------------------------------------
// Ingress parser
// ---------------------------------------------------------------------------
parser SwitchIngressParser(
        packet_in pkt,
        out header_t hdr,
        out metadata_t meta,
        out ingress_intrinsic_metadata_t ig_intr_md) {
        
    state start {
        pkt.extract(ig_intr_md);
        transition select(ig_intr_md.resubmit_flag) {
            0 : parse_port_metadata;
            1 : parse_resubmit;
        }
    }
    state parse_port_metadata {
        pkt.advance(PORT_METADATA_SIZE);
        transition parse_ethernet;
    }
    state parse_resubmit{
        pkt.extract(meta.info);
        transition parse_ethernet;
    }
    state parse_ethernet{
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            ETHERTYPE_IPV4 : parse_ipv4;
            default : accept;
        }
    }
    state parse_ipv4{
        pkt.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            IP_PROTOCOLS_UDP : parse_udp;
            IP_PROTOCOLS_ACK : parse_ack;
            IP_PROTOCOLS_CAVER_ACK : parse_caver_ack;
            IP_PROTOCOLS_CAVER_DATA : parse_caver_data;
            default : accept;
        }
    }
    state parse_udp{
        meta.isACK = 0;
        meta.isUDP = 1;
        meta.isCAVER_ack = 0;
        meta.isCAVER_data = 0;
        pkt.extract(hdr.udp);
        transition accept;
    }
    state parse_ack{
        meta.isACK = 1;
        meta.isUDP = 0;
        meta.isCAVER_ack = 0;
        meta.isCAVER_data = 0;
        pkt.extract(hdr.ack);
        transition accept;
    }
    state parse_caver_ack{
        meta.isACK = 0;
        meta.isUDP = 0;
        meta.isCAVER_ack = 1;
        meta.isCAVER_data = 0;
        pkt.extract(hdr.caver_ack);
        transition accept;
    }
    state parse_caver_data{
        meta.isACK = 0;
        meta.isUDP = 0;
        meta.isCAVER_ack = 0;
        meta.isCAVER_data = 1;
        pkt.extract(hdr.caver_data);
        transition accept;
    }
}


control SwitchIngress(
        inout header_t hdr,
        inout metadata_t meta,
        in ingress_intrinsic_metadata_t ig_intr_md,
        in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
        inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {


    // Register
    Register<time_t, bit<32>>(HOSTNUM, 0) pathCE_time_reg;
    Register<bit<8>, bit<32>>(HOSTNUM, 0) pathCE_reg;
    Register<port_t, bit<32>>(HOSTNUM, 0) SrcRoute_hop_0_reg;//PathCE表储存的路径（1个路径使用4个下一跳来表示）
    Register<port_t, bit<32>>(HOSTNUM, 0) SrcRoute_hop_1_reg;
    Register<port_t, bit<32>>(HOSTNUM, 0) SrcRoute_hop_2_reg;
    Register<port_t, bit<32>>(HOSTNUM, 0) SrcRoute_hop_3_reg;
    Register<time_t, bit<32>>(FLOWNUM, 0) flow_time_reg;
    Register<bit<8>, bit<32>>(FLOWNUM, 0) flow_is_SrcRoute_reg;
    Register<port_t, bit<32>>(FLOWNUM, 0) flow_SrcRoute_hop_0_reg;
    Register<port_t, bit<32>>(FLOWNUM, 0) flow_SrcRoute_hop_1_reg;
    Register<port_t, bit<32>>(FLOWNUM, 0) flow_SrcRoute_hop_2_reg;
    Register<port_t, bit<32>>(FLOWNUM, 0) flow_SrcRoute_hop_3_reg;
    Register<time_t, bit<32>>(PORTNUM, 0) portCE_reduce_time_reg;//储存端口DRE减小的时间
    Register<DRE_t, bit<32>>(PORTNUM, 0) portDre_reg;
    Register<DRE_t, bit<32>>(PORTNUM, 0) portCE_reg;

    //Registeraction
    
    RegisterAction<time_t, bit<32>, time_t>(pathCE_time_reg) pathCE_checkSrcValid_action = {
        void apply(inout time_t val, out time_t rv) {
            if (meta.current_time - val > PATHCE_TOUT){
                // meta.info.SrcRoute_Valid = 0;
                rv = 0;
            }
            else{
                // meta.info.SrcRoute_Valid = 1;
                rv = 1;
            }
        }
    };
    RegisterAction<time_t, bit<32>, time_t>(pathCE_time_reg) pathCE_updateSrcValid_action = {
        void apply(inout time_t val, out time_t rv) {
            if (meta.current_time - val > PATHCE_TOUT){
                // meta.info.SrcRoute_Valid = 0;
                rv = 0;
            }
            else{
                // meta.info.SrcRoute_Valid = 1;
                rv = 1;
            }
            val = meta.current_time;
        }
    };
    RegisterAction<CE_t, bit<32>, CE_t>(pathCE_reg) pathCE_updateCE_action = {
        void apply(inout CE_t val, out CE_t rv) {
            val  = meta.newPathCE;
            rv = 128 + meta.newPathCE;
        }
    };
    RegisterAction<CE_t, bit<32>, CE_t>(pathCE_reg) pathCE_compareCE_action = {
        void apply(inout CE_t val, out CE_t rv) {
            rv = (val > meta.newPathCE) ? (128 + val) :val;
            if (meta.newPathCE < val){
                val  = meta.newPathCE;
            }
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(SrcRoute_hop_0_reg) pathCE_get_hop_0_action = {
        void apply(inout port_t val, out port_t rv) {
            rv = val;
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(SrcRoute_hop_0_reg) pathCE_judge_hop_0_action = {
        void apply(inout port_t val, out port_t rv){
            // meta.info.CE_port表示ack进入的端口，该值用于判断目前pathCE表中的下一跳是否等于ack的入端口
            rv = (meta.info.CE_port == val) ? 8w1 : 8w0;
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(SrcRoute_hop_0_reg) pathCE_update_hop_0_action = {
        void apply(inout port_t val, out port_t rv){
            // val = (meta.updateCE == 1) ? ig_intr_md.ingress_port[7:0] : val;
            if (meta.updateCE == 1){
                val = meta.info.CE_port;
            }
            rv = val;
        }
    };
    RegisterAction<time_t, bit<32>, time_t>(flow_time_reg) flow_time_check_action = {
        void apply(inout time_t val, out time_t rv){
            rv = (meta.current_time - val > FLOWLET_TOUT) ? 32w0 : 32w1;
            val = meta.current_time;
        }
    };
    RegisterAction<bit<8>, bit<32>, bit<8>>(flow_is_SrcRoute_reg) update_flow_is_SrcRoute_action ={
        void apply(inout bit<8> val, out bit<8> rv){
            val = (bit<8>)meta.info.SrcRoute_Valid;
            rv = val;
        }
    };
    RegisterAction<bit<8>, bit<32>, bit<8>>(flow_is_SrcRoute_reg) get_flow_is_SrcRoute_action = {
        void apply(inout bit<8> val, out bit<8> rv){
            rv = val;
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(SrcRoute_hop_1_reg) pathCE_get_hop_1_action = {
        void apply(inout port_t val, out port_t rv) {
            rv = val;
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(SrcRoute_hop_1_reg) pathCE_update_hop_1_action = {
        void apply(inout port_t val, out port_t rv){
            // val = (meta.updateCE == 1) ? ig_intr_md.ingress_port[7:0] : val;
            if (meta.updateCE == 1){
                val = hdr.caver_ack.hop_0;
            }
            rv = val;
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(SrcRoute_hop_2_reg) pathCE_get_hop_2_action = {
        void apply(inout port_t val, out port_t rv) {
            rv = val;
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(SrcRoute_hop_2_reg) pathCE_update_hop_2_action = {
        void apply(inout port_t val, out port_t rv){
            // val = (meta.updateCE == 1) ? ig_intr_md.ingress_port[7:0] : val;
            if (meta.updateCE == 1){
                val = hdr.caver_ack.hop_1;
            }
            rv = val;
        }
    };
        RegisterAction<port_t, bit<32>, port_t>(SrcRoute_hop_3_reg) pathCE_get_hop_3_action = {
        void apply(inout port_t val, out port_t rv) {
            rv = val;
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(SrcRoute_hop_3_reg) pathCE_update_hop_3_action = {
        void apply(inout port_t val, out port_t rv){
            // val = (meta.updateCE == 1) ? ig_intr_md.ingress_port[7:0] : val;
            if (meta.updateCE == 1){
                val =  hdr.caver_ack.hop_2;
            }
            rv = val;
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(flow_SrcRoute_hop_0_reg) update_flow_SrcRoute_hop_0_action = {
        void apply(inout port_t val, out port_t rv){
            val = meta.SrcRoute_hop_0;
            rv = val;
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(flow_SrcRoute_hop_0_reg) get_flow_SrcRoute_hop_0_action = {
        void apply(inout port_t val, out port_t rv){
            rv = val;
        }
    };
        RegisterAction<port_t, bit<32>, port_t>(flow_SrcRoute_hop_1_reg) update_flow_SrcRoute_hop_1_action = {
        void apply(inout port_t val, out port_t rv){
            val = meta.SrcRoute_hop_1;
            rv = val;
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(flow_SrcRoute_hop_1_reg) get_flow_SrcRoute_hop_1_action = {
        void apply(inout port_t val, out port_t rv){
            rv = val;
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(flow_SrcRoute_hop_2_reg) update_flow_SrcRoute_hop_2_action = {
        void apply(inout port_t val, out port_t rv){
            val = meta.SrcRoute_hop_2;
            rv = val;
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(flow_SrcRoute_hop_2_reg) get_flow_SrcRoute_hop_2_action = {
        void apply(inout port_t val, out port_t rv){
            rv = val;
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(flow_SrcRoute_hop_3_reg) update_flow_SrcRoute_hop_3_action = {
        void apply(inout port_t val, out port_t rv){
            val = meta.SrcRoute_hop_3;
            rv = val;
        }
    };
    RegisterAction<port_t, bit<32>, port_t>(flow_SrcRoute_hop_3_reg) get_flow_SrcRoute_hop_3_action = {
        void apply(inout port_t val, out port_t rv){
            rv = val;
        }
    };
    RegisterAction<time_t, bit<32>, time_t>(portCE_reduce_time_reg) check_portDre_reduce_time_action = {
        void apply(inout time_t val, out time_t rv){
            rv = (meta.current_time - val > CE_reduce_gap)? 32w1:0;
            if (meta.current_time - val > CE_reduce_gap){
                val = meta.current_time;
            }
        }
    };
    MathUnit<DRE_t>(MathOp_t.DIV, 4, 5) m_mul;
    MathUnit<DRE_t>(MathOp_t.DIV, 1, 12559) m_quant;
    RegisterAction<DRE_t, bit<32>, DRE_t>(portCE_reg) QuantizingX_action = {
        void apply(inout DRE_t val, out DRE_t rv){
            val = m_quant.execute(meta.localDre);
            rv = val;
        }
    };
    RegisterAction<DRE_t, bit<32>, DRE_t>(portDre_reg) update_Dre_action = {
        void apply(inout DRE_t val, out DRE_t rv){
            if (meta.CE_reduce == 1){
                val = m_mul.execute(val + (DRE_t)hdr.ipv4.total_len);
            }
            else{
                val = val + (DRE_t)hdr.ipv4.total_len;
            }
            rv = val;
        }

    };
    RegisterAction<DRE_t, bit<32>, DRE_t>(portDre_reg) get_Dre_action = {
        void apply(inout DRE_t val, out DRE_t rv){
            rv = val;
        }
    };

    action no_action(){}
    action drop() { ig_dprsr_md.drop_ctl = 1; }
    action QuantizingX_table_action(){
        meta.info.localCE = (CE_t) QuantizingX_action.execute((bit<32>)meta.info.CE_port);
    }
    table QuantizingX_table{
        key = {
            meta.is_data: exact;
            meta.byPass: exact;
            ig_intr_md.resubmit_flag: exact;
        }
        actions ={
            no_action;
            QuantizingX_table_action;
        }
        size = 8;
        default_action = no_action;
    }
    action  update_DRE_table_action(){
        update_Dre_action.execute((bit<32>)meta.info.CE_port);
    }
    action get_DRE_table_action(){
        meta.localDre = get_Dre_action.execute((bit<32>)meta.info.CE_port);
    }
    table Dre_reg_table{
        key = {
            meta.is_data: exact;
            ig_intr_md.resubmit_flag: exact;
        }
        actions = {
            update_DRE_table_action;
            get_DRE_table_action;
            no_action;
        }
        default_action = no_action;
        size = 4;
    }
    action check_portDre_reduce_timetable_action(){
        meta.CE_reduce = (bit<1>) check_portDre_reduce_time_action.execute((bit<32>)meta.info.CE_port);
    }
    action check_portDre_reduce_time_table_no_action(){
        meta.CE_reduce = 0;
    }
    table portDre_reduce_time_table{
        key = {
            meta.is_data: exact;
        }
        actions = {
            check_portDre_reduce_timetable_action;
            check_portDre_reduce_time_table_no_action;
        }
        size = 2;
        default_action = check_portDre_reduce_time_table_no_action;

    }

    action update_flow_SrcRoute_hop_0_table_action(){
        // meta.SrcRoute_hop_0 = temp_port;
        meta.info.CE_port = update_flow_SrcRoute_hop_0_action.execute(meta.flow_id);
    }
    action get_flow_SrcRoute_hop_0_table_action(){
        meta.info.CE_port = get_flow_SrcRoute_hop_0_action.execute(meta.flow_id);
        // meta.SrcRoute_hop_0 = temp_port;
    }
    table flow_SrcRoute_hop_0_Table{
        key = {
            meta.byPass: exact;
            meta.is_data: exact;
            meta.info.m_isSrcToR: exact;
            meta.flow_valid: exact;
            meta.info.SrcRoute_Valid: exact;
        }
        actions ={
            update_flow_SrcRoute_hop_0_table_action;
            get_flow_SrcRoute_hop_0_table_action;
            no_action;
        }
        size = 4;
        default_action = no_action;
    }
    action update_flow_SrcRoute_hop_1_table_action(){
        meta.SrcRoute_hop_1 = update_flow_SrcRoute_hop_1_action.execute(meta.flow_id);
    }
    action get_flow_SrcRoute_hop_1_table_action(){
        meta.SrcRoute_hop_1 = get_flow_SrcRoute_hop_1_action.execute(meta.flow_id);
    }
    table flow_SrcRoute_hop_1_Table{
        key = {
            meta.byPass: exact;
            meta.is_data: exact;
            meta.info.m_isSrcToR: exact;
            meta.flow_valid: exact;
            meta.info.SrcRoute_Valid: exact;
        }
        actions ={
            update_flow_SrcRoute_hop_1_table_action;
            get_flow_SrcRoute_hop_1_table_action;
            no_action;
        }
        size = 4;
        default_action = no_action;
    }
    action update_flow_SrcRoute_hop_2_table_action(){
        meta.SrcRoute_hop_2 = update_flow_SrcRoute_hop_2_action.execute(meta.flow_id);
    }
    action get_flow_SrcRoute_hop_2_table_action(){
        meta.SrcRoute_hop_2 = get_flow_SrcRoute_hop_2_action.execute(meta.flow_id);
    }
    table flow_SrcRoute_hop_2_Table{
        key = {
            meta.byPass: exact;
            meta.is_data: exact;
            meta.info.m_isSrcToR: exact;
            meta.flow_valid: exact;
            meta.info.SrcRoute_Valid: exact;
        }
        actions ={
            update_flow_SrcRoute_hop_2_table_action;
            get_flow_SrcRoute_hop_2_table_action;
            no_action;
        }
        size = 4;
        default_action = no_action;
    }
    action update_flow_SrcRoute_hop_3_table_action(){
        meta.SrcRoute_hop_3 = update_flow_SrcRoute_hop_3_action.execute(meta.flow_id);
    }
    action get_flow_SrcRoute_hop_3_table_action(){
        meta.SrcRoute_hop_3 = get_flow_SrcRoute_hop_3_action.execute(meta.flow_id);
    }
    table flow_SrcRoute_hop_3_Table{
        key = {
            meta.byPass: exact;
            meta.is_data: exact;
            meta.info.m_isSrcToR: exact;
            meta.flow_valid: exact;
            meta.info.SrcRoute_Valid: exact;
        }
        actions ={
            update_flow_SrcRoute_hop_3_table_action;
            get_flow_SrcRoute_hop_3_table_action;
            no_action;
        }
        size = 4;
        default_action = no_action;
    }


    action update_flow_is_SrcRoute_table_action(){
        meta.info.SrcRoute_Valid = (bit<1>)update_flow_is_SrcRoute_action.execute(meta.flow_id);
    }
    action get_flow_is_SrcRoute_table_action(){
        meta.info.SrcRoute_Valid = (bit<1>)get_flow_is_SrcRoute_action.execute(meta.flow_id);
    }
    action flow_is_SrcRoute_no_action(){

    }
    table flow_is_SrcRoute_table{
        key = {
            meta.byPass: exact;
            meta.is_data: exact;
            meta.info.m_isSrcToR: exact;
            meta.flow_valid: exact;
        }
        actions = {
            update_flow_is_SrcRoute_table_action;
            get_flow_is_SrcRoute_table_action;
            flow_is_SrcRoute_no_action;
        }
        size = 4;
        default_action = flow_is_SrcRoute_no_action;

    }
    action flow_time_check_table_action(){
        meta.flow_valid = (bit<1>)flow_time_check_action.execute(meta.flow_id);
    }
    action flow_time_no_action(){
        meta.flow_valid = 0;
    }
    table flow_time_table{
        key = {
            meta.is_data: exact;
            meta.byPass: exact;
            meta.info.m_isSrcToR: exact;
        }
        actions = {
            flow_time_check_table_action;
            flow_time_no_action;
        }
        size = 4;
        default_action = flow_time_no_action;
    }
    action pathCE_get_hop_0_table_action(){
        meta.SrcRoute_hop_0 = pathCE_get_hop_0_action.execute((bit<32>)meta.info.host_id);
    }

    action pathCE_judge_hop_0_table_action(){
        meta.info.same_port = (bit<1>)pathCE_judge_hop_0_action.execute((bit<32>)meta.info.host_id);
    }
    action pathCE_update_hop_0_table_action(){
        meta.SrcRoute_hop_0 = pathCE_update_hop_0_action.execute((bit<32>)meta.info.host_id);
    }

    action pathCE_hop_0_no_action(){
        meta.SrcRoute_hop_0 = 255;
        meta.info.same_port = 0;
    }
    table pathCE_hop_0_table{
        key = {
            meta.is_data: exact;
            meta.info.m_isSrcToR: exact;
            ig_intr_md.resubmit_flag: exact;
            meta.byPass: exact;
            meta.info.SrcRoute_Valid: exact;
        }
        actions = {
            pathCE_get_hop_0_table_action;
            pathCE_judge_hop_0_table_action;
            pathCE_update_hop_0_table_action;
            pathCE_hop_0_no_action;
        }
        size = 8;
        default_action = pathCE_hop_0_no_action;
    }

    action pathCE_get_hop_1_table_action(){
        meta.SrcRoute_hop_1 = pathCE_get_hop_1_action.execute((bit<32>)meta.info.host_id);
    }
    action pathCE_update_hop_1_table_action(){
        meta.SrcRoute_hop_1 = pathCE_update_hop_1_action.execute((bit<32>)meta.info.host_id);
    }
    action pathCE_hop_1_no_action(){
        meta.SrcRoute_hop_1 = 255;
    }
    table pathCE_hop_1_table{
        key = {
            meta.is_data: exact;
            meta.info.m_isSrcToR: exact;
            ig_intr_md.resubmit_flag: exact;
            meta.byPass: exact;
            meta.info.SrcRoute_Valid: exact;
        }
        actions = {
            pathCE_get_hop_1_table_action;
            pathCE_update_hop_1_table_action;
            pathCE_hop_1_no_action;
        }
        size = 8;
        default_action = pathCE_hop_1_no_action;
    }

    action pathCE_get_hop_2_table_action(){
        meta.SrcRoute_hop_2 = pathCE_get_hop_2_action.execute((bit<32>)meta.info.host_id);
    }

    action pathCE_update_hop_2_table_action(){
        meta.SrcRoute_hop_2 = pathCE_update_hop_2_action.execute((bit<32>)meta.info.host_id);
    }

    action pathCE_hop_2_no_action(){
        meta.SrcRoute_hop_2 = 255;
    }
    table pathCE_hop_2_table{
        key = {
            meta.is_data: exact;
            meta.info.m_isSrcToR: exact;
            ig_intr_md.resubmit_flag: exact;
            meta.byPass: exact;
            meta.info.SrcRoute_Valid: exact;
        }
        actions = {
            pathCE_get_hop_2_table_action;
            pathCE_update_hop_2_table_action;
            pathCE_hop_2_no_action;
        }
        size = 8;
        default_action = pathCE_hop_2_no_action;
    }

    action pathCE_get_hop_3_table_action(){
        meta.SrcRoute_hop_3 = pathCE_get_hop_3_action.execute((bit<32>)meta.info.host_id);
    }

    action pathCE_update_hop_3_table_action(){
        meta.SrcRoute_hop_3 = pathCE_update_hop_3_action.execute((bit<32>)meta.info.host_id);
    }

    action pathCE_hop_3_no_action(){
        meta.SrcRoute_hop_3 = 255;
    }
    table pathCE_hop_3_table{
        key = {
            meta.is_data: exact;
            meta.info.m_isSrcToR: exact;
            ig_intr_md.resubmit_flag: exact;
            meta.byPass: exact;
            meta.info.SrcRoute_Valid: exact;
        }
        actions = {
            pathCE_get_hop_3_table_action;
            pathCE_update_hop_3_table_action;
            pathCE_hop_3_no_action;
        }
        size = 8;
        default_action = pathCE_hop_3_no_action;
    }
    action updateCE_table_action(){
        meta.pathCE = pathCE_updateCE_action.execute((bit<32>)meta.info.host_id);
    }
    action compareCE_table_action(){
        meta.pathCE = pathCE_compareCE_action.execute((bit<32>)meta.info.host_id);
    }
    action pathCE_reg_table_no_action(){
        meta.pathCE = 0;
    }
    table pathCE_reg_action_Table{
        key = {
            ig_intr_md.resubmit_flag: exact;
            meta.is_data: exact;
            meta.new_info: exact;
            meta.byPass: exact;
        }
        actions = {
            updateCE_table_action;
            compareCE_table_action;
            pathCE_reg_table_no_action;
        }
        size = 4;
        default_action = pathCE_reg_table_no_action;
    }

    action do_pathCE_checkSrcValid_action(){
        meta.info.SrcRoute_Valid = (bit<1>)pathCE_checkSrcValid_action.execute((bit<32>)meta.info.host_id);
    }
    action do_pathCE_updateSrcValid_action(){
        meta.info.SrcRoute_Valid  = (bit<1>)pathCE_updateSrcValid_action.execute((bit<32>)meta.info.host_id);
    }
    action pathCE_time_reg_no_action(){
        meta.info.SrcRoute_Valid = 0;
    }
    table pathCE_time_reg_action_Table{
        key = {
            meta.is_data : exact;
            meta.info.m_isSrcToR : exact;
            ig_intr_md.resubmit_flag: exact;
            meta.byPass: exact;
        }
        actions = {
            do_pathCE_checkSrcValid_action;
            do_pathCE_updateSrcValid_action;
            pathCE_time_reg_no_action;
        }
        default_action =pathCE_time_reg_no_action;
        size = 8;
    }
    action parse_isack_first() { 
        meta.info.m_isSrcToR = 1;
        meta.Addr = hdr.ipv4.src_addr;
        meta.srcPort = hdr.ack.srcPort;
        meta.dstPort = hdr.ack.dstPort;
        meta.len = hdr.ack.len;
        meta.checksum = hdr.ack.checksum;
        meta.seq = hdr.ack.seq;
        meta.is_data = 0;
        ig_dprsr_md.resubmit_type = 1;
    }
    action parse_isack_resubmit() { 
        meta.info.m_isSrcToR = 1;
        meta.Addr = hdr.ipv4.src_addr;
        meta.srcPort = hdr.ack.srcPort;
        meta.dstPort = hdr.ack.dstPort;
        meta.len = hdr.ack.len;
        meta.checksum = hdr.ack.checksum;
        meta.seq = hdr.ack.seq;
        meta.is_data = 0;
        ig_dprsr_md.resubmit_type = 0;
    }
    action parse_isudp() { 
        meta.info.m_isSrcToR = 1;
        meta.Addr = hdr.ipv4.dst_addr;
        meta.srcPort = hdr.udp.srcPort;
        meta.dstPort = hdr.udp.dstPort;
        meta.len = hdr.udp.hdr_length;
        meta.is_data = 1;
        ig_dprsr_md.resubmit_type = 0;
    }
    action parse_isCAVER_ack_first() { 
        meta.info.m_isSrcToR = 0;
        meta.Addr = hdr.ipv4.src_addr;
        meta.srcPort = hdr.caver_ack.srcPort;
        meta.dstPort = hdr.caver_ack.dstPort;
        meta.len = hdr.caver_ack.len;
        meta.checksum = hdr.caver_ack.checksum;
        meta.is_data = 0;
        meta.seq = hdr.caver_ack.seq;
        ig_dprsr_md.resubmit_type = 1;
    }
    action parse_isCAVER_ack_resubmit() { 
        meta.info.m_isSrcToR = 0;
        meta.Addr = hdr.ipv4.src_addr;
        meta.srcPort = hdr.caver_ack.srcPort;
        meta.dstPort = hdr.caver_ack.dstPort;
        meta.is_data = 0;
        meta.len = hdr.caver_ack.len;
        meta.checksum = hdr.caver_ack.checksum;
        meta.seq = hdr.caver_ack.seq;
        ig_dprsr_md.resubmit_type = 0;
    }
    action parse_isCAVER_data() { 
        meta.info.m_isSrcToR = 0;
        meta.Addr = hdr.ipv4.dst_addr;
        meta.srcPort = hdr.caver_data.srcPort;
        meta.dstPort = hdr.caver_data.dstPort;
        meta.len = hdr.caver_data.len;
        meta.is_data = 1;
        ig_dprsr_md.resubmit_type = 0;
    }
    action parse_no_action(){
    }
    ////ig_intr_md.resubmit_flag: exact; 为0时触发
    table parser_init_Table {
        key = {
            ig_intr_md.resubmit_flag: exact;
            meta.isACK : exact;
            meta.isUDP : exact;
            meta.isCAVER_ack : exact;
            meta.isCAVER_data : exact;
        }
        actions = {
            parse_isack_first;
            parse_isack_resubmit;
            parse_isudp;
            parse_isCAVER_ack_first;
            parse_isCAVER_ack_resubmit;
            parse_isCAVER_data;
            parse_no_action;
        }
        default_action = parse_no_action;
        size = 4;
    }
    //根据目的ip地址，确定目的的host id； 
    //ig_intr_md.resubmit_flag: exact; 为0时触发
    action get_host_id(host_t host_id){
        meta.info.host_id =  host_id;
    }
    table Host_ip_2_id_Table {
        key= {
            ig_intr_md.resubmit_flag: exact;
            meta.Addr: exact;
        }
        actions = {
          get_host_id;
          no_action;
        }
        default_action = no_action;
        size = HOSTNUM;
    }

    Hash<bit<16>>(HashAlgorithm_t.CRC16) ecmp_hash;
    Hash<bit<32>>(HashAlgorithm_t.CRC32) flow_hash;
    // Action Profile Size = max group size x max number of groups
    ActionProfile(20000) ecmp_ap;
    ActionSelector(ecmp_ap, // action profile
                   ecmp_hash, // hash extern
                   SelectorMode_t.FAIR, // Selector algorithm
                   200, // max group size
                   100 // max number of groups
                   ) ecmp_action_selector;
    //ECMP_related
    //ig_intr_md.resubmit_flag = 0时触发
    action getEport(port_t port) {
        meta.info.eport = port;
    }
    table ECMP_Table {
        key = {
            ig_intr_md.resubmit_flag: exact;
            hdr.ipv4.dst_addr : exact;
            hdr.ipv4.src_addr : selector;
            hdr.ipv4.dst_addr : selector;
            hdr.udp.srcPort : selector;
            hdr.udp.dstPort : selector;
        }

        actions = {
            getEport;
            no_action;
        }

        const default_action = no_action;
        size = 512;
        implementation = ecmp_action_selector;
    }
    //需要给所有不是dstToR的host配置表项
    action  set_dstToR(){
        meta.info.m_isDstToR = 1;
    }
    action set_non_dstToR(){
        meta.info.m_isDstToR = 0;
    }
    table ToR_host_Table{
        key = {
            ig_intr_md.resubmit_flag: exact;
            hdr.ipv4.dst_addr: exact;
        }
        actions = {
            set_dstToR;
            set_non_dstToR;
            no_action;
        }
        default_action = no_action;
        size = PORTNUM;
    }
    action is_new_info(){
        meta.new_info = 1;
    }
    action not_new_info(){
        meta.new_info = 0;
    }

    table check_new_info_Table{
        key = {
            //ig_intr_md.resubmit_flag = 1时触发
            ig_intr_md.resubmit_flag: exact;
            meta.info.SrcRoute_Valid: exact;
            meta.info.same_port:exact;
            meta.byPass: exact;
            meta.info.m_isSrcToR:exact;
        }
        actions ={
            is_new_info;
            not_new_info;
        }
        size = 4;
        default_action = not_new_info;
    }
    action is_byPass(){
        meta.byPass = 1;
    }
    action not_byPass(){
        meta.byPass = 0;
    }
    table byPass_Table{
        key = {
            meta.info.m_isSrcToR: exact;
            meta.info.m_isDstToR:exact;
        }
        actions = {
            is_byPass;
            not_byPass;
        }
        size = 2;
        default_action = not_byPass;
    }
    action gather_CE(){
        meta.newPathCE = (meta.info.localCE > hdr.caver_ack.pathCE) ? meta.info.localCE :hdr.caver_ack.pathCE;
    }
    action gatherCE_no_action(){
    }
    table gatherCE_Table{
        key = {
            ig_intr_md.resubmit_flag: exact;
            meta.byPass: exact;
            meta.isCAVER_ack:exact;
            meta.info.m_isSrcToR:exact;
        }
        actions = {
            gather_CE;
            gatherCE_no_action;
        }
        size = 2;
        default_action =  gatherCE_no_action;
    }
    action get_flow_id(){
        meta.flow_id[31:8] = ((flow_hash.get({hdr.ipv4.src_addr[31:0], hdr.ipv4.dst_addr[31:0], meta.srcPort, meta.dstPort})))[31:8];
        meta.flow_id[7:0] =  meta.info.host_id;
    }
    action get_no_flow_id(){
        meta.flow_id = 0;
    }
    table get_flow_id_table{
        key = {
            meta.byPass: exact;
            meta.is_data: exact;
        }
        actions = {
            get_flow_id;
            get_no_flow_id;
        }
        size = 2;
        default_action = get_no_flow_id;
    }
    action Set_data_CE_port(){
        meta.info.CE_port = meta.info.eport;
    }
    action Set_ack_CE_port(){
        meta.info.CE_port = ig_intr_md.ingress_port[7:0];
    }
    table Set_CE_port_table{
        key = {
            meta.is_data: exact;
        }
        actions = {
            Set_data_CE_port;
            Set_ack_CE_port;
        }
        default_action = Set_data_CE_port;
        size = 2;
    }
    action SrcRoute_choose_Port_0(){
        meta.info.CE_port = hdr.caver_data.hop_0;
    }
    action SrcRoute_choose_Port_1(){
        meta.info.CE_port = hdr.caver_data.hop_1;
    }
    action SrcRoute_choose_Port_2(){
        meta.info.CE_port = hdr.caver_data.hop_2;
    }
    action SrcRoute_choose_Port_3(){
        meta.info.CE_port = hdr.caver_data.hop_3;
    }
    action ecmp_Port(){
        meta.info.CE_port = meta.info.eport;
    }

    table Mid_Switch_outPort_Table{
        key = {
            hdr.caver_data.hopCount: exact;
            hdr.caver_data.SrcRoute: exact;
            meta.is_data:exact;
            meta.info.m_isSrcToR: exact;
            meta.byPass:exact;
        }
        actions = {
            SrcRoute_choose_Port_0;
            SrcRoute_choose_Port_1;
            SrcRoute_choose_Port_2;
            SrcRoute_choose_Port_3;
            ecmp_Port;
        }
        default_action = ecmp_Port;
        
        size = 4;
    }
    // action data_SrcToR_SrcRoute_header_action(){
    //     hdr.caver_data.setValid();
    //     hdr.caver_data.srcPort = meta.srcPort;
    //     hdr.caver_data.dstPort = meta.dstPort;
    //     hdr.caver_data.len = hdr.udp.hdr_length;

    //     hdr.caver_data.hop_0 = meta.SrcRoute_hop_0;
    //     hdr.caver_data.hop_1 = meta.SrcRoute_hop_1;
    //     hdr.caver_data.hop_2 = meta.SrcRoute_hop_2;
    //     hdr.caver_data.hop_3 = meta.SrcRoute_hop_3;
    //     hdr.caver_data.hopCount = 0;
    //     hdr.caver_data.SrcRoute = 1;
    //     hdr.caver_data.padding = 0;
    //     hdr.udp.setInvalid();
    //     hdr.ipv4.protocol = IP_PROTOCOLS_CAVER_DATA;
    //     ig_tm_md.ucast_egress_port = (bit<9>) meta.info.CE_port;
    // }
    // action data_SrcToR_ecmp_header_action(){
    //     hdr.caver_data.setValid();
    //     hdr.caver_data.srcPort = meta.srcPort;
    //     hdr.caver_data.dstPort = meta.dstPort;
    //     hdr.caver_data.len = hdr.udp.hdr_length;

    //     hdr.caver_data.hop_0 = 0;
    //     hdr.caver_data.hop_1 = 0;
    //     hdr.caver_data.hop_2 = 0;
    //     hdr.caver_data.hop_3 = 0;
    //     hdr.caver_data.hopCount = 0;
    //     hdr.caver_data.SrcRoute = 0;
    //     hdr.caver_data.padding = 0;
    //     hdr.udp.setInvalid();
    //     hdr.ipv4.protocol = IP_PROTOCOLS_CAVER_DATA;
    //     ig_tm_md.ucast_egress_port = (bit<9>) meta.info.eport;
    // }
    // action data_Mid_SrcRoute_header_action(){
    //     hdr.caver_data.hopCount = hdr.caver_data.hopCount + 1;
    //     ig_tm_md.ucast_egress_port = (bit<9>) meta.info.CE_port;
    // }
    // action data_Mid_ecmp_header_action(){
    //     ig_tm_md.ucast_egress_port = (bit<9>) meta.info.eport;
    // }
    // action data_DstToR_header_action(){
    //     hdr.udp.setValid();
    //     hdr.udp.srcPort = meta.srcPort;
    //     hdr.udp.dstPort = meta.dstPort;
    //     hdr.udp.hdr_length = hdr.caver_data.len;
    //     hdr.caver_data.setInvalid();
    //     hdr.ipv4.protocol = IP_PROTOCOLS_UDP;
    //     ig_tm_md.ucast_egress_port = (bit<9>) meta.info.eport;
    // }
    // action ack_SrcToR_header_action(){
    //     hdr.caver_ack.setValid();
    //     hdr.caver_ack.srcPort = meta.srcPort;
    //     hdr.caver_ack.dstPort = meta.dstPort;
    //     hdr.caver_ack.len = hdr.ack.len;

    //     hdr.caver_ack.hop_0 = meta.SrcRoute_hop_0;
    //     hdr.caver_ack.hop_1 = meta.SrcRoute_hop_1;
    //     hdr.caver_ack.hop_2 = meta.SrcRoute_hop_2;
    //     hdr.caver_ack.hop_3 = meta.SrcRoute_hop_3; 
    //     hdr.caver_ack.pathCE = meta.pathCE;
    //     hdr.ack.setInvalid();
    //     hdr.ipv4.protocol = IP_PROTOCOLS_CAVER_ACK;
    //     ig_tm_md.ucast_egress_port = (bit<9>) meta.info.eport;
    // }
    // action ack_Mid_header_action(){
    //     hdr.caver_ack.hop_0 = meta.SrcRoute_hop_0;
    //     hdr.caver_ack.hop_1 = meta.SrcRoute_hop_1;
    //     hdr.caver_ack.hop_2 = meta.SrcRoute_hop_2;
    //     hdr.caver_ack.hop_3 = meta.SrcRoute_hop_3; 
    //     hdr.caver_ack.pathCE = meta.pathCE;
    //     ig_tm_md.ucast_egress_port = (bit<9>) meta.info.eport;
    // }
    // action ack_DstToR_header_action(){
    //     hdr.ack.setValid();
    //     hdr.ack.srcPort = meta.srcPort;
    //     hdr.ack.dstPort = meta.dstPort;
    //     hdr.ack.len = hdr.caver_data.len;
    //     hdr.caver_ack.setInvalid();
    //     hdr.ipv4.protocol = IP_PROTOCOLS_ACK;
    //     ig_tm_md.ucast_egress_port = (bit<9>) meta.info.eport;
    // }
    // table generate_header_table{
    //     key = {
    //         meta.byPass: exact;
    //         meta.is_data:exact;
    //         meta.info.m_isSrcToR: exact;
    //         meta.info.m_isDstToR: exact;
    //         meta.info.SrcRoute_Valid: exact;
    //     }
    //     actions = {
    //         data_SrcToR_SrcRoute_header_action;
    //         data_SrcToR_ecmp_header_action;
    //         data_Mid_SrcRoute_header_action;
    //         data_Mid_ecmp_header_action;
    //         data_DstToR_header_action;
    //         ack_SrcToR_header_action;
    //         ack_Mid_header_action;
    //         ack_DstToR_header_action;
    //         no_action;
    //     }
    //     size = 16;
    //     default_action = no_action;
    // }

    action header_data2CAVER(){
        hdr.udp.setInvalid();
        hdr.caver_data.setValid();
    }
    action header_CAVER2data(){
        hdr.caver_data.setInvalid();
        hdr.udp.setValid();
    }
    action header_ack2CAVER(){
        hdr.ack.setInvalid();
        hdr.caver_ack.setValid();
    }
    action header_CAVER2ack(){
        hdr.caver_ack.setInvalid();
        hdr.caver_ack.setValid();
    }
    table header_change_table{
        key = {
            meta.info.m_isSrcToR: exact;
            meta.info.m_isDstToR: exact;
            meta.is_data:exact;
            meta.byPass:exact;
        }
        actions = {
            header_data2CAVER;
            header_CAVER2data;
            header_ack2CAVER;
            header_CAVER2ack;
            no_action;
        }
        default_action = no_action;
        size = 5;
    }

    action header_data_SrcPort(){ hdr.udp.srcPort = meta.srcPort;}
    action header_caver_data_SrcPort(){ hdr.caver_data.srcPort = meta.srcPort;}
    action header_ack_SrcPort(){ hdr.ack.srcPort = meta.srcPort;}
    action header_caver_ack_SrcPort(){ hdr.caver_ack.srcPort = meta.srcPort;}
    table header_SrcPort_table{
        key = {
            meta.info.m_isSrcToR: exact;
            meta.info.m_isDstToR: exact;
            meta.is_data:exact;
            meta.byPass:exact;
        }
        actions = {
            header_data_SrcPort;
            header_caver_data_SrcPort;
            header_ack_SrcPort;
            header_caver_ack_SrcPort;
            no_action;
        }
        default_action = no_action;
        size = 5;
    }
    action header_data_DstPort(){ hdr.udp.dstPort = meta.dstPort;}
    action header_caver_data_DstPort(){ hdr.caver_data.dstPort = meta.dstPort;}
    action header_ack_DstPort(){ hdr.ack.dstPort = meta.dstPort;}
    action header_caver_ack_DstPort(){ hdr.caver_ack.dstPort = meta.dstPort;}
    table header_DstPort_table{
        key = {
            meta.info.m_isSrcToR: exact;
            meta.info.m_isDstToR: exact;
            meta.is_data:exact;
            meta.byPass:exact;
        }
        actions = {
            header_data_DstPort;
            header_caver_data_DstPort;
            header_ack_DstPort;
            header_caver_ack_DstPort;
            no_action;
        }
        default_action = no_action;
        size = 5;
    }
    action header_data_checksum(){ hdr.udp.checksum = meta.checksum;}
    action header_caver_data_checksum(){ hdr.caver_data.checksum = meta.checksum;}
    action header_ack_checksum(){ hdr.ack.checksum = meta.checksum;}
    action header_caver_ack_checksum(){ hdr.caver_ack.checksum = meta.checksum;}
    table header_checksum_table{
        key = {
            meta.info.m_isSrcToR: exact;
            meta.info.m_isDstToR: exact;
            meta.is_data:exact;
            meta.byPass:exact;
        }
        actions = {
            header_data_checksum;
            header_caver_data_checksum;
            header_ack_checksum;
            header_caver_ack_checksum;
            no_action;
        }
        default_action = no_action;
        size = 5;
    }
    action header_data_len(){ hdr.udp.hdr_length = meta.len;}
    action header_caver_data_len(){ hdr.caver_data.len = meta.len;}
    action header_ack_len(){ hdr.ack.len = meta.len;}
    action header_caver_ack_len(){ hdr.caver_ack.len = meta.len;}
    table header_len_table{
        key = {
            meta.info.m_isSrcToR: exact;
            meta.info.m_isDstToR: exact;
            meta.is_data:exact;
            meta.byPass:exact;
        }
        actions = {
            header_data_len;
            header_caver_data_len;
            header_ack_len;
            header_caver_ack_len;
            no_action;
        }
        default_action = no_action;
        size = 5;
    }
    action header_data_SrcRoute_hop_0(){ hdr.caver_data.hop_0 = meta.SrcRoute_hop_0; }
    action header_data_ecmp_hop_0(){ hdr.caver_data.hop_0 = 0; }
    action header_ack_hop_0(){ hdr.caver_ack.hop_0 = meta.SrcRoute_hop_0; }
    table header_hop_0_table{
        key = {
            meta.info.m_isSrcToR: exact;
            meta.info.m_isDstToR: exact;
            meta.is_data:exact;
            meta.byPass:exact;
        }
        actions = {
            header_data_SrcRoute_hop_0;
            header_data_ecmp_hop_0;
            header_ack_hop_0;
            no_action;
        }
        default_action = no_action;
        size = 4;
    }
    action header_data_SrcRoute_hop_1(){ hdr.caver_data.hop_1 = meta.SrcRoute_hop_1; }
    action header_data_ecmp_hop_1(){ hdr.caver_data.hop_1 = 0; }
    action header_ack_hop_1(){ hdr.caver_ack.hop_1 = meta.SrcRoute_hop_1; }
    table header_hop_1_table{
        key = {
            meta.info.m_isSrcToR: exact;
            meta.info.m_isDstToR: exact;
            meta.is_data:exact;
            meta.byPass:exact;
        }
        actions = {
            header_data_SrcRoute_hop_1;
            header_data_ecmp_hop_1;
            header_ack_hop_1;
            no_action;
        }
        default_action = no_action;
        size = 4;
    }
    action header_data_SrcRoute_hop_2(){ hdr.caver_data.hop_2 = meta.SrcRoute_hop_2; }
    action header_data_ecmp_hop_2(){ hdr.caver_data.hop_2 = 0; }
    action header_ack_hop_2(){ hdr.caver_ack.hop_2 = meta.SrcRoute_hop_2; }
    table header_hop_2_table{
        key = {
            meta.info.m_isSrcToR: exact;
            meta.info.m_isDstToR: exact;
            meta.is_data:exact;
            meta.byPass:exact;
        }
        actions = {
            header_data_SrcRoute_hop_2;
            header_data_ecmp_hop_2;
            header_ack_hop_2;
            no_action;
        }
        default_action = no_action;
        size = 4;
    }
    action header_data_SrcRoute_hop_3(){ hdr.caver_data.hop_3 = meta.SrcRoute_hop_3; }
    action header_data_ecmp_hop_3(){ hdr.caver_data.hop_3 = 0; }
    action header_ack_hop_3(){ hdr.caver_ack.hop_3 = meta.SrcRoute_hop_3; }
    table header_hop_3_table{
        key = {
            meta.info.m_isSrcToR: exact;
            meta.info.m_isDstToR: exact;
            meta.is_data:exact;
            meta.byPass:exact;
        }
        actions = {
            header_data_SrcRoute_hop_3;
            header_data_ecmp_hop_3;
            header_ack_hop_3;
            no_action;
        }
        default_action = no_action;
        size = 4;
    }
    action header_hopCount_SrcToR(){ hdr.caver_data.hopCount = 0; }   
    action header_hopCount_MidSwitch(){ hdr.caver_data.hopCount = hdr.caver_data.hopCount + 1; }
    table header_hopCount_table{
        key = {
            meta.info.m_isSrcToR: exact;
            meta.info.m_isDstToR: exact;
            meta.is_data:exact;
            meta.byPass:exact;
        }
        actions = {
            header_hopCount_SrcToR;
            header_hopCount_MidSwitch;
            no_action;
        }
        default_action = no_action;
        size = 3;
    }
    action header_SrcRoute(){ hdr.caver_data.SrcRoute = meta.info.SrcRoute_Valid; }
    table header_SrcRoute_table{
        key = {
            meta.info.m_isSrcToR: exact;
            meta.info.m_isDstToR: exact;
            meta.is_data:exact;
            meta.byPass:exact;
        }
        actions = {
            header_SrcRoute;
            no_action;
        }
        size = 2;
        default_action = no_action;
    }
    action header_caver_ack_seq(){ hdr.caver_ack.seq = meta.seq; }
    action header_ack_seq(){ hdr.ack.seq = meta.seq; }
    table header_seq_table{
        key = {
            meta.info.m_isSrcToR: exact;
            meta.info.m_isDstToR: exact;
            meta.is_data:exact;
            meta.byPass:exact;
        }
        actions = {
            header_caver_ack_seq;
            header_ack_seq;
            no_action;
        }
        size = 3;
        default_action = no_action;
    }

    action header_pathCE(){ hdr.caver_ack.pathCE = meta.pathCE; }
    table header_pathCE_table{
        key = {
            meta.info.m_isSrcToR: exact;
            meta.info.m_isDstToR: exact;
            meta.is_data:exact;
            meta.byPass:exact;
        }
        actions = {
            header_pathCE;
            no_action;
        }
        default_action = no_action;
        size = 4;
    }
    action data_outPort(){ meta.outPort= meta.info.CE_port; }
    action ecmp_outPort(){ meta.outPort= meta.info.eport; }
    table outPort_table{
        key = {
            meta.is_data:exact;
            meta.byPass:exact;
            ig_intr_md.resubmit_flag: exact;
        }
        actions = {
            data_outPort;
            ecmp_outPort;
        }
        default_action = ecmp_outPort;
        size = 3;
    }
    action allocate_outPort(port_t outPort){ig_tm_md.ucast_egress_port[7:0] = outPort;}
    table allocate_table{
        key = {
            meta.outPort: exact;
        }
        actions = {
            allocate_outPort;
            no_action;
        }
        default_action = no_action;
        size = 32;
    }

    apply {
            meta.current_time = ig_intr_md.ingress_mac_tstamp[31:0];
            ECMP_Table.apply();
            ToR_host_Table.apply();
            parser_init_Table.apply();
            byPass_Table.apply();
            //获取host id;
            Host_ip_2_id_Table.apply();
            //TODO:ACK在SrcToR处获取host id；
            //
            //
            Set_CE_port_table.apply();
            get_flow_id_table.apply();
            flow_time_table.apply();

            if (ig_intr_md.resubmit_flag == 1){
                check_new_info_Table.apply();
                gatherCE_Table.apply();
                pathCE_reg_action_Table.apply();
                if (meta.pathCE > 8w128){
                    meta.pathCE = meta.pathCE - 8w128;
                    meta.updateCE = 1;
                }
                else{
                    meta.updateCE = 0;
                }
            }
            else{
                pathCE_time_reg_action_Table.apply();
            }
            pathCE_hop_0_table.apply();
            pathCE_hop_1_table.apply();
            pathCE_hop_2_table.apply();
            pathCE_hop_3_table.apply();
            if (meta.info.m_isSrcToR == 0 && meta.info.m_isDstToR == 0){
                Mid_Switch_outPort_Table.apply();
            }
            else{
                flow_is_SrcRoute_table.apply();
                flow_SrcRoute_hop_0_Table.apply();
                meta.SrcRoute_hop_0 = meta.info.CE_port;
                flow_SrcRoute_hop_1_Table.apply();
                flow_SrcRoute_hop_2_Table.apply();
                flow_SrcRoute_hop_3_Table.apply();
            }

            // PortCE表更新及量化相关
            portDre_reduce_time_table.apply();
            Dre_reg_table.apply();
            QuantizingX_table.apply();

            //包头处理以及outport选择
            header_change_table.apply();
            header_SrcPort_table.apply();
            header_DstPort_table.apply();
            header_checksum_table.apply();
            header_len_table.apply();
            header_hop_0_table.apply();
            header_hop_1_table.apply();
            header_hop_2_table.apply();
            header_hop_3_table.apply();
            header_hopCount_table.apply();
            header_SrcRoute_table.apply();
            header_seq_table.apply();
            header_pathCE_table.apply();
            outPort_table.apply();
            allocate_table.apply();
        }
}


// ---------------------------------------------------------------------------
// Ingress Deparser
// ---------------------------------------------------------------------------
control SwitchIngressDeparser(
        packet_out pkt,
        inout header_t hdr,
        in metadata_t meta,
        in ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md) {

    Resubmit() resubmit; // call to resubmit
    apply {
        if(ig_dprsr_md.resubmit_type == 1){
            resubmit.emit(meta.info);
        }
        pkt.emit(hdr);
    }
}
Pipeline(SwitchIngressParser(),
         SwitchIngress(),
         SwitchIngressDeparser(),
         EmptyEgressParser(),
         EmptyEgress(),
         EmptyEgressDeparser()) pipe;

Switch(pipe) main;

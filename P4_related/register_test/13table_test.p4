#include <core.p4>
#include <tna.p4>

#include "header.p4"
#include "parser.p4"
#include "action.p4"
#include "table.p4"

control SwitchIngress(
        inout header_t hdr,
        inout metadata_t ig_md,
        in ingress_intrinsic_metadata_t ig_intr_md,
        in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
        inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {

    @stage(0)
    Register<bit<64>, _>(65536) r_test_0;
    RegisterAction<_, _, bit<64>>(r_test_0) ra_update_test_0 = {
        void apply(inout bit<64> value, out bit<64> output) {
            value = value + 1;
        }
    };
    Register<bit<64>, _>(65536) r_test_1;
    RegisterAction<_, _, bit<64>>(r_test_1) ra_update_test_1 = {
        void apply(inout bit<64> value, out bit<64> output) {
            value = value + 1;
        }
    };
    Register<bit<64>, _>(65536) r_test_2;
    RegisterAction<_, _, bit<64>>(r_test_2) ra_update_test_2 = {
        void apply(inout bit<64> value, out bit<64> output) {
            value = value + 1;
        }
    };
    Register<bit<64>, _>(65536) r_test_3;
    RegisterAction<_, _, bit<64>>(r_test_3) ra_update_test_3 = {
        void apply(inout bit<64> value, out bit<64> output) {
            value = value + 1;
        }
    };
    Register<bit<64>, _>(65536) r_test_4;
    RegisterAction<_, _, bit<64>>(r_test_4) ra_update_test_4 = {
        void apply(inout bit<64> value, out bit<64> output) {
            value = value + 1;
        }
    };
    Register<bit<64>, _>(65536) r_test_5;
    RegisterAction<_, _, bit<64>>(r_test_5) ra_update_test_5 = {
        void apply(inout bit<64> value, out bit<64> output) {
            value = value + 1;
        }
    };
    Register<bit<64>, _>(65536) r_test_6;
    RegisterAction<_, _, bit<64>>(r_test_6) ra_update_test_6 = {
        void apply(inout bit<64> value, out bit<64> output) {
            value = value + 1;
        }
    };
    Register<bit<64>, _>(65536) r_test_7;
    RegisterAction<_, _, bit<64>>(r_test_7) ra_update_test_7 = {
        void apply(inout bit<64> value, out bit<64> output) {
            value = value + 1;
        }
    };
    Register<bit<64>, _>(65536) r_test_8;
    RegisterAction<_, _, bit<64>>(r_test_8) ra_update_test_8 = {
        void apply(inout bit<64> value, out bit<64> output) {
            value = value + 1;
        }
    };
    Register<bit<64>, _>(65536) r_test_9;
    RegisterAction<_, _, bit<64>>(r_test_9) ra_update_test_9 = {
        void apply(inout bit<64> value, out bit<64> output) {
            value = value + 1;
        }
    };
    Register<bit<64>, _>(65536) r_test_10;
    RegisterAction<_, _, bit<64>>(r_test_10) ra_update_test_10 = {
        void apply(inout bit<64> value, out bit<64> output) {
            value = value + 1;
        }
    };
    Register<bit<64>, _>(65536) r_test_11;
    RegisterAction<_, _, bit<64>>(r_test_11) ra_update_test_11 = {
        void apply(inout bit<64> value, out bit<64> output) {
            value = value + 1;
        }
    };
    Register<bit<64>, _>(65536) r_test_12;
    RegisterAction<_, _, bit<64>>(r_test_12) ra_update_test_12 = {
        void apply(inout bit<64> value, out bit<64> output) {
            value = value + 1;
        }
    };
    Register<bit<64>, _>(65536) r_test_13;
    RegisterAction<_, _, bit<64>>(r_test_13) ra_update_test_13 = {
        void apply(inout bit<64> value, out bit<64> output) {
            value = value + 1;
        }
    };

    action a_update_test_0() {
        ra_update_test_0.execute(0);
    }
    action a_update_test_1() {
        ra_update_test_1.execute(0);
    }
    action a_update_test_2() {
        ra_update_test_2.execute(0);
    }
    action a_update_test_3() {
        ra_update_test_3.execute(0);
    }
    action a_update_test_4() {
        ra_update_test_4.execute(0);
    }
    action a_update_test_5() {
        ra_update_test_5.execute(0);
    }
    action a_update_test_6() {
        ra_update_test_6.execute(0);
    }
    action a_update_test_7() {
        ra_update_test_7.execute(0);
    }
    action a_update_test_8() {
        ra_update_test_8.execute(0);
    }
    action a_update_test_9() {
        ra_update_test_9.execute(0);
    }
    action a_update_test_10() {
        ra_update_test_10.execute(0);
    }
    action a_update_test_11() {
        ra_update_test_11.execute(0);
    }
    action a_update_test_12() {
        ra_update_test_12.execute(0);
    }
    action a_update_test_13() {
        ra_update_test_13.execute(0);
    }

    table t_test_0 {
        actions = {
            a_update_test_0;
        }
        default_action = a_update_test_0();
    }
    table t_test_1 {
        actions = {
            a_update_test_1;
        }
        default_action = a_update_test_1();
    }
    table t_test_2 {
        actions = {
            a_update_test_2;
        }
        default_action = a_update_test_2();
    }
    table t_test_3 {
        actions = {
            a_update_test_3;
        }
        default_action = a_update_test_3();
    }
    table t_test_4 {
        actions = {
            a_update_test_4;
        }
        default_action = a_update_test_4();
    }
    table t_test_5 {
        actions = {
            a_update_test_5;
        }
        default_action = a_update_test_5();
    }
    table t_test_6 {
        actions = {
            a_update_test_6;
        }
        default_action = a_update_test_6();
    }
    table t_test_7 {
        actions = {
            a_update_test_7;
        }
        default_action = a_update_test_7();
    }
    table t_test_8 {
        actions = {
            a_update_test_8;
        }
        default_action = a_update_test_8();
    }
    table t_test_9 {
        actions = {
            a_update_test_9;
        }
        default_action = a_update_test_9();
    }
    table t_test_10 {
        actions = {
            a_update_test_10;
        }
        default_action = a_update_test_10();
    }
    table t_test_11 {
        actions = {
            a_update_test_11;
        }
        default_action = a_update_test_11();
    }
    table t_test_12 {
        actions = {
            a_update_test_12;
        }
        default_action = a_update_test_12();
    }
    table t_test_13 {
        actions = {
            a_update_test_13;
        }
        default_action = a_update_test_13();
    }
    @stage(1)
    action forward_1(){
        ig_tm_md.ucast_egress_port = 180;
    }
    table forward_1_table{
        actions = {
            forward_1;
        }
        size = 1024;
        default_action = forward_1;
    }
    @stage(1)
    action forward_2(){
        ig_tm_md.ucast_egress_port = 188;
    }
    table forward_2_table{
        actions = {
            forward_2;
        }
        size = 1024;
        default_action = forward_2;
    }

    apply {
        t_test_0.apply();
        t_test_1.apply();
        t_test_2.apply();
        t_test_3.apply();
        t_test_4.apply();
        t_test_5.apply();
        t_test_6.apply();
        t_test_7.apply();
        t_test_8.apply();
        t_test_9.apply();
        t_test_10.apply();
        t_test_11.apply();
        t_test_12.apply();
        t_test_13.apply();
        if (ig_intr_md.ingress_port == 188){
                // ig_tm_md.ucast_egress_port = 180;
                forward_1_table.apply();
        }
        if (ig_intr_md.ingress_port == 180){
                // ig_tm_md.ucast_egress_port = 188;
            forward_2_table.apply();
        }
        //    if (hdr.infiniband.isValid() && !hdr.infiniband_ack.isValid() && hdr.infiniband.ack_request == 0){
        //         hdr.infiniband.ack_request = 1;
        //         hdr.icrc.crc = hdr.icrc.crc ^ 0x6728070a;
        //    }
    }
}

Pipeline(RoCEv2IngressParser(),
         SwitchIngress(),
         SwitchIngressDeparser(),
         SwitchEgressParser(),
         SwitchEgress(),
         SwitchEgressDeparser()) pipe;

Switch(pipe) main;

/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ECPRISS_XBAR_H_
#define ECPRISS_XBAR_H_

#define TOTAL_LINKS 12
#define XBAR_LINKS 4
#define LUT_INDEX 65535
#define NUM_OF_FHP 3
#define NUM_OF_FLTR 16
#define NUM_EGRESS_ENTRY 256
#define MAX_XBAR_WM_ENTRY 10

#include "ecpriss_xbar_hal.h"
#include "ecpriss_flow.h"
#include "ecpriss_core.h"
#include "ecpriss_qudp_hwio_def.h"
#include "ecpriss_qudp_hwio_def_v2.h"
#include "ecpriss_netlink.h"


/*
 * Index of this in device tree file
 * ecpriss_qudp_interrupt_events_e it has index 0-6
 */
#define EXPRISS_XBAR_INDEX 6
typedef enum
{
	ECPRI_XBAR_DEINIT,
	ECPRI_XBAR_COLD_INIT,
	ECRPI_XBAR_READY

}ecpriss_xbar_state_e;

typedef struct ecpriss_xbar_interrupt_cfg
{
	//place holder
}ecpriss_xbar_interrupt_cfg_s;

typedef struct ecpriss_xbar_interrupt_cfg_v2
{
	ecpri_xbar_hwio_def_ecpri_xbar_sw_irq_status_s xbar_sw_irq_status;
}ecpriss_xbar_interrupt_cfg_s_v2;


/**
 *
 */
typedef struct fh_port_config
{
	uint64_t xbar_fhrx_lut[LUT_INDEX];
}fh_port_config;
typedef struct ecpriss_xbar_stats_s
{
	uint64_t xbar_fhrx_pkt_cnt[TOTAL_LINKS];
	uint64_t xbar_fhrx_dma_pkt_cnt;
	uint64_t xbar_fhrx_uc_pkt_cnt;
	uint64_t xbar_fhrx_uc_err_pkt_cnt;
	uint64_t xbar_fhrx_err_pkt_cnt;
	uint64_t xbar_fhtx_pkt_cnt[TOTAL_LINKS];
	uint64_t xbar_fhtx_c2c_pkt_ovf_cnt;
	uint64_t xbar_fhtx_dma_pkt_cnt;
	uint64_t xbar_c2c_pkt_ovf_cnt;
	uint64_t xbar_c2crx_dma_pkt_cnt;
	uint64_t xbar_c2crx_err_pkt_cnt;
	uint64_t xbar_fhtx_uc_pkt_cnt;
	uint32_t xbar_ocrx_fh_buff_watermark_fh0;
	uint32_t xbar_ocrx_fh_buff_watermark_fh1;
	uint32_t xbar_ocrx_fh_buff_watermark_fh2;
	uint32_t xbar_dbg_ocrx_0_1_buff_watermark_cc0;
	uint32_t xbar_dbg_ocrx_0_1_buff_watermark_cc1;
	uint32_t xbar_dbg_ocrx_2_3_buff_watermark_cc2;
	uint32_t xbar_dbg_ocrx_2_3_buff_watermark_cc3;
	uint32_t octx_oc_0_1_buff_watermark_cc0;
	uint32_t octx_oc_0_1_buff_watermark_cc1;
	uint32_t octx_oc_2_3_buff_watermark_cc2;
	uint32_t octx_oc_2_3_buff_watermark_cc3;
	uint32_t xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_0_cnt;
	uint32_t xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_1_cnt;
	uint32_t xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_2_cnt;
	uint32_t xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_0_cnt;
	uint32_t xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_1_cnt;
	uint32_t xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_2_cnt;
	uint32_t ocrx_unknown_pcid_info_1_n[TOTAL_LINKS];
	uint32_t ocrx_unknown_pcid_info_2_n[TOTAL_LINKS];
	uint32_t fhrx_unknown_pcid_info_1_n[TOTAL_LINKS];
	uint32_t fhrx_unknown_pcid_info_2_n[TOTAL_LINKS];
	uint64_t xbar_c2crx_pkt_cnt[TOTAL_LINKS];
	uint64_t xbar_c2ctx_pkt_cnt[TOTAL_LINKS];
	uint64_t xbar_ocrx_pkt_cnt[XBAR_LINKS];
	uint64_t xbar_octx_pkt_cnt[XBAR_LINKS];
	uint32_t ocrx_fh_wm_fh0[MAX_XBAR_WM_ENTRY];
	uint32_t ocrx_fh_wm_fh1[MAX_XBAR_WM_ENTRY];
	uint32_t ocrx_fh_wm_fh2[MAX_XBAR_WM_ENTRY];
	uint32_t ocrx_0_1_wm_cc0[MAX_XBAR_WM_ENTRY];
	uint32_t ocrx_0_1_wm_cc1[MAX_XBAR_WM_ENTRY];
	uint32_t ocrx_2_3_wm_cc2[MAX_XBAR_WM_ENTRY];
	uint32_t ocrx_2_3_wm_cc3[MAX_XBAR_WM_ENTRY];
	uint32_t octx_0_1_wm_cc0[MAX_XBAR_WM_ENTRY];
	uint32_t octx_0_1_wm_cc1[MAX_XBAR_WM_ENTRY];
	uint32_t octx_2_3_wm_cc2[MAX_XBAR_WM_ENTRY];
	uint32_t octx_2_3_wm_cc3[MAX_XBAR_WM_ENTRY];
	uint64_t curr_wm_index;
	fh_port_config xbar_fh_port[NUM_OF_FHP];
}ecpriss_xbar_stats_s;

typedef struct ecpriss_xbar_stats_s_v2
{
	uint64_t xbar_fhrx_pkt_cnt[TOTAL_LINKS];
	uint64_t xbar_fhrx_dma_pkt_cnt;
	uint64_t xbar_fhrx_uc_pkt_cnt;
	uint64_t xbar_fhrx_uc_err_pkt_cnt;
	uint64_t xbar_fhrx_err_pkt_cnt;
	uint64_t xbar_fhrx_c2c_pkt_cnt[NUM_OF_FHP];
	uint64_t xbar_fhrx_oc_pkt_cnt[NUM_OF_FHP];
	uint64_t xbar_fhtx_pkt_cnt[TOTAL_LINKS];
	uint64_t xbar_fhtx_c2c_pkt_ovf_cnt;
	uint64_t xbar_fhtx_dma_pkt_cnt;
	uint64_t xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_0_cnt;
	uint64_t xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_1_cnt;
	uint64_t xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_2_cnt;
	uint64_t fhrx_unknown_pcid_info_1_n[TOTAL_LINKS];
	uint64_t fhrx_unknown_pcid_info_2_n[TOTAL_LINKS];
	uint64_t xbar_fhtx_uc_pkt_cnt;

	uint64_t xbar_octx_c2c_pkt_drop_cnt;
	uint64_t xbar_octx_fh_pkt_drop_cnt;
	uint64_t xbar_octx_c2c_len_err_cnt;
	uint64_t xbar_octx_fh_len_err_cnt;
	uint64_t xbar_octx_pkt_cnt[XBAR_LINKS];
	uint64_t octx_oc_0_1_buff_watermark_cc0;
	uint64_t octx_oc_0_1_buff_watermark_cc1;
	uint64_t octx_oc_2_3_buff_watermark_cc2;
	uint64_t octx_oc_2_3_buff_watermark_cc3;
	uint64_t octx_0_1_wm_cc0[MAX_XBAR_WM_ENTRY];
	uint64_t octx_0_1_wm_cc1[MAX_XBAR_WM_ENTRY];
	uint64_t octx_2_3_wm_cc2[MAX_XBAR_WM_ENTRY];
	uint64_t octx_2_3_wm_cc3[MAX_XBAR_WM_ENTRY];
	uint64_t curr_wm_index;
	uint64_t xbar_ocrx_pkt_cnt[XBAR_LINKS];
	uint64_t xbar_ocrx_fh_pkt_cnt[TOTAL_LINKS];
	uint64_t xbar_ocrx_c2c_pkt_cnt[NUM_OF_FHP];
	uint64_t xbar_dbg_ocrx_0_1_buff_watermark_cc0;
	uint64_t xbar_dbg_ocrx_0_1_buff_watermark_cc1;
	uint64_t xbar_dbg_ocrx_2_3_buff_watermark_cc2;
	uint64_t xbar_dbg_ocrx_2_3_buff_watermark_cc3;
	uint64_t xbar_ocrx_fh_buff_watermark_fh0;
	uint64_t xbar_ocrx_fh_buff_watermark_fh1;
	uint64_t xbar_ocrx_fh_buff_watermark_fh2;
	uint64_t xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_0_cnt;
	uint64_t xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_1_cnt;
	uint64_t xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_2_cnt;
	uint64_t ocrx_unknown_pcid_info_1_n[TOTAL_LINKS];
	uint64_t ocrx_unknown_pcid_info_2_n[TOTAL_LINKS];
	uint64_t ocrx_fh_wm_fh0[MAX_XBAR_WM_ENTRY];
	uint64_t ocrx_fh_wm_fh1[MAX_XBAR_WM_ENTRY];
	uint64_t ocrx_fh_wm_fh2[MAX_XBAR_WM_ENTRY];
	uint64_t ocrx_0_1_wm_cc0[MAX_XBAR_WM_ENTRY];
	uint64_t ocrx_0_1_wm_cc1[MAX_XBAR_WM_ENTRY];
	uint64_t ocrx_2_3_wm_cc2[MAX_XBAR_WM_ENTRY];
	uint64_t ocrx_2_3_wm_cc3[MAX_XBAR_WM_ENTRY];
	fh_port_config xbar_fh_port[NUM_OF_FHP];
}ecpriss_xbar_stats_s_v2;


typedef struct ecpriss_xbar_interrupt_stats{
	uint32_t octx_fh_len_err;
	uint32_t octx_c2c_len_err;
	uint32_t fhtx_c2c_overflow;
	uint32_t c2ctx_fh_overflow;
	uint32_t octx_fh_overflow;
	uint32_t octx_c2c_overflow;
	uint32_t fhrx_uc_pkt_pending;
	uint32_t fhrx_uc_overflow;
	uint32_t fhrx_uc_pkt_err;
	uint32_t fhrx_uc_pkt_drop;
	uint32_t ocrx_unknown_pcid;
	uint32_t fhrx_unknown_pcid;
	uint32_t c2crx_unkown_pcid;
} ecpriss_xbar_interrupt_stats_s;

typedef struct ecpriss_xbar_interrupt_stats_v2{
	uint32_t octx_fh_len_err;
	uint32_t octx_c2c_len_err;
	uint32_t fhtx_c2c_overflow;
	uint32_t c2ctx_fh_overflow;
	uint32_t octx_fh_overflow;
	uint32_t octx_c2c_overflow;
	uint32_t fhrx_uc_pkt_pending;
	uint32_t fhrx_uc_overflow;
	uint32_t fhrx_uc_pkt_err;
	uint32_t fhrx_uc_pkt_drop;
	uint32_t ocrx_unknown_pcid;
	uint32_t fhrx_unknown_pcid;
	uint32_t c2crx_unkown_pcid;
} ecpriss_xbar_interrupt_stats_s_v2;


typedef struct ecpriss_xbar_port_cfg
{
	eth_ecpriss_port_params_s             eth_cfg;
	struct ecpri_dma_port_params          dma_port_cfg[ECPRISS_MAX_PORTS];
}ecpriss_xbar_port_cfg_s;

typedef struct ecpriss_xbar_flow_cfg
{
	uint32_t     pcid : 16;
	uint32_t     dma_ring_id : 7;
	uint32_t     oc_link_id : 2;
	uint32_t     route_to_oran : 1;
	uint32_t     route_to_c2c : 1;
	uint32_t     route_to_dma : 1;
	uint32_t     valid : 1;
}ecpriss_xbar_pcid_flow_cfg_s;

typedef struct ecpriss_xbar_flow_cfg_v2
{
	uint32_t     pcid : 16;
	uint32_t     other_oc_link_id : 2;
	uint32_t     ul_oc_link_id : 2;
	uint32_t     dl_oc_link_id : 2;
	uint32_t      other_route_to_oran : 1;
	uint32_t      other_route_to_c2c : 1;
	uint32_t      other_route_to_dma : 1;
	uint32_t      dl_route_to_c2c : 1;
	uint32_t      dl_route_to_dma : 1;
	uint32_t      dl_route_to_oran : 1;
	uint32_t      ul_route_to_c2c : 1;
	uint32_t      ul_route_to_dma : 1;
	uint32_t      ul_route_to_oran : 1;
}ecpriss_xbar_pcid_flow_cfg_s_v2;

typedef struct ecpriss_xbar_port_lut
{
	uint32_t	current_pcid;
	ecpriss_xbar_pcid_flow_cfg_s	lut_table[ECPRISS_MAX_PCID_ENTRIES];
}ecpriss_xbar_port_lut_s;

typedef struct ecpriss_xbar_port_lut_v2
{
	uint32_t	current_pcid;
	uint16_t	num_pcid_entries;
	uint16_t	configured_pcids[ECPRISS_MAX_PCID_ENTRIES];
	ecpriss_xbar_pcid_flow_cfg_s_v2	lut_table[ECPRISS_MAX_PCID_ENTRIES];
}ecpriss_xbar_port_lut_s_v2;


typedef struct ecpriss_xbar_oc_rx_flow_cfg
{
	uint32_t     pcid : 16;
	uint32_t     l2_encap_info : 7;
	uint32_t     l3_encap_info : 7;
	uint32_t     l3_encap_valid : 1;
	uint32_t     valid : 1;
}ecpriss_xbar_oc_rx_flow_cfg_s;

typedef struct ecpriss_xbar_oc_rx_port_lut
{
	uint32_t	current_pcid;
	uint16_t	num_pcid_entries;
	uint16_t	configured_pcids[ECPRISS_MAX_PCID_ENTRIES];
	ecpriss_xbar_oc_rx_flow_cfg_s	lut_table[ECPRISS_MAX_PCID_ENTRIES];
}ecpriss_xbar_oc_rx_port_lut_s;

typedef struct ecpriss_flow_ctx
{
	ecpriss_xbar_port_lut_s		fh_xbar_lut[ECPRISS_MAX_PORTS];
#ifdef C2C_XBAR_LUT
	ecpriss_xbar_port_lut_s		c2c_dl_xbar_lut[ECPRISS_MAX_PORTS];
	ecpriss_xbar_port_lut_s		c2c_ul_xbar_lut[ECPRISS_MAX_PORTS];
#endif
	ecpriss_xbar_oc_rx_port_lut_s	oc_rx_xbar_lut[ECPRISS_MAX_PORTS];
}ecpriss_flow_ctx_s;

typedef struct ecpriss_flow_ctx_v2
{
	ecpriss_xbar_port_lut_s_v2		fh_xbar_lut[ECPRISS_MAX_PORTS];

#ifdef C2C_XBAR_LUT
	ecpriss_xbar_port_lut_s_v2		c2c_dl_xbar_lut[ECPRISS_MAX_PORTS];
	ecpriss_xbar_port_lut_s_v2		c2c_ul_xbar_lut[ECPRISS_MAX_PORTS];
#endif
	ecpriss_xbar_oc_rx_port_lut_s	oc_rx_xbar_lut[ECPRISS_MAX_PORTS];
}ecpriss_flow_ctx_s_v2;


typedef struct ecpriss_xbar_ctx
{
	ecpriss_xbar_state_e		state;
	uint32_t			num_of_port_types;
	ecpriss_xbar_port_cfg_s		fh_port_cfg;
	ecpriss_xbar_port_cfg_s		c2c_port_cfg;
	ecpriss_xbar_port_cfg_s		l2_port_cfg;
	ecpriss_xbar_port_cfg_s		fh_exception_port_cfg;
	ecpriss_xbar_interrupt_cfg_s	interrupt_cfg;
	ecpriss_xbar_stats_s		stats;
	ecpriss_xbar_interrupt_stats_s interrupt_stats;
	ecpriss_flow_ctx_s		flow_ctx;
	ecpriss_xbar_hal_context_s	*ecpriss_xbar_hal;
	bool				def_lut_cfg_done[ECPRISS_MAX_PORTS];
}ecpriss_xbar_ctx_s;

typedef struct ecpriss_xbar_global_cfg{
	uint64_t global;
}ecpriss_xbar_global_cfg_s;

typedef struct ecpriss_xbar_ctx_v2
{
	ecpriss_xbar_state_e		state;
	uint32_t			num_of_port_types;
	ecpriss_xbar_port_cfg_s		fh_port_cfg;
	ecpriss_xbar_port_cfg_s		c2c_port_cfg;
	ecpriss_xbar_port_cfg_s		l2_port_cfg;
	ecpriss_xbar_port_cfg_s		fh_exception_port_cfg;
	ecpriss_xbar_port_cfg_s         fh_lte_port_cfg[ECPRISS_MAX_LTE_VF];
	ecpriss_xbar_port_cfg_s		oran_log_port_cfg;
	ecpriss_xbar_interrupt_cfg_s_v2	interrupt_cfg_v2;
	ecpriss_xbar_stats_s_v2		stats_v2;
	ecpriss_xbar_interrupt_stats_s_v2 interrupt_stats_v2;
	ecpriss_flow_ctx_s_v2		flow_ctx_v2;
	ecpriss_xbar_hal_context_s	*ecpriss_xbar_hal;
	ecpri_xbar_hwio_def_ecpri_xbar_flush_s xbar_flush_status;
	ecpriss_xbar_global_cfg_s	xbar_global_cfg;
	bool				def_lut_cfg_done[ECPRISS_MAX_PORTS];
        bool                            disable_xbar_dma_fh_same_prio;
	uint32_t			enable_len_check;
}ecpriss_xbar_ctx_s_v2;


typedef struct ecpriss_xbar_lut_cfg{
	uint64_t ocrx[NUM_OF_FHP][LUT_INDEX];
	uint64_t fhrx[NUM_OF_FHP][LUT_INDEX];
	uint64_t c2crxdl;
	uint64_t c2crxul;
}ecpriss_xbar_lut_cfg_s;

typedef struct ecpriss_xbar_cfg{
	ecpriss_xbar_global_cfg_s global_cfg;
	ecpriss_xbar_lut_cfg_s lut_cfg;
}ecpriss_xbar_cfg_s;

typedef struct ecpriss_xbar_cfg_v2{
	ecpriss_xbar_global_cfg_s global_cfg;
	ecpriss_xbar_lut_cfg_s lut_cfg;
}ecpriss_xbar_cfg_s_v2;


typedef struct ecpriss_src_ip_addr_cfg{
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr0_port_p_entry_n_s   ip_src0;
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr1_port_p_entry_n_s   ip_src1;
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr2_port_p_entry_n_s   ip_src2;
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr3_port_p_entry_n_s   ip_src3;
}ecpriss_src_ip_addr_cfg_s;

typedef struct ecpriss_src_ip_addr_cfg_v2{
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr0_port_p_entry_n_s_v2   ip_src0;
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr1_port_p_entry_n_s_v2   ip_src1;
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr2_port_p_entry_n_s_v2   ip_src2;
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr3_port_p_entry_n_s_v2   ip_src3;
}ecpriss_src_ip_addr_cfg_s_v2;


typedef struct ecpriss_dst_ip_addr_cfg{
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr0_port_p_entry_n_s   ip_dst0;
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr1_port_p_entry_n_s   ip_dst1;
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr2_port_p_entry_n_s   ip_dst2;
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr3_port_p_entry_n_s   ip_dst3;
}ecpriss_dst_ip_addr_cfg_s;

typedef struct ecpriss_dst_ip_addr_cfg_v2{
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr0_port_p_entry_n_s_v2   ip_dst0;
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr1_port_p_entry_n_s_v2   ip_dst1;
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr2_port_p_entry_n_s_v2   ip_dst2;
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr3_port_p_entry_n_s_v2   ip_dst3;
}ecpriss_dst_ip_addr_cfg_s_v2;


typedef struct ecpriss_qudp_egress_cfg{
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_udp_ports_port_p_entry_n_s udp_ports[NUM_OF_FHP][NUM_EGRESS_ENTRY];
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_vlan_ethertype_port_p_entry_n_s  vlan_ethertype[NUM_OF_FHP][NUM_EGRESS_ENTRY];
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_dst0_port_p_entry_n_s eth_dst0_port[NUM_OF_FHP][NUM_EGRESS_ENTRY];
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src1_dst1_port_p_entry_n_s eth_src1_dst1_port[NUM_OF_FHP][NUM_EGRESS_ENTRY];
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src0_port_p_entry_n_s eth_src0_port[NUM_OF_FHP][NUM_EGRESS_ENTRY];
	ecpriss_dst_ip_addr_cfg_s dst_ip_addr[NUM_OF_FHP][NUM_EGRESS_ENTRY];
	ecpriss_src_ip_addr_cfg_s src_ip_addr[NUM_OF_FHP][NUM_EGRESS_ENTRY];
}ecpriss_qudp_egress_cfg_s;

typedef struct ecpriss_qudp_egress_cfg_v2{
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_udp_ports_port_p_entry_n_s_v2 udp_ports[NUM_OF_FHP][NUM_EGRESS_ENTRY];
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_vlan_ethertype_port_p_entry_n_s_v2  vlan_ethertype[NUM_OF_FHP][NUM_EGRESS_ENTRY];
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_dst0_port_p_entry_n_s_v2 eth_dst0_port[NUM_OF_FHP][NUM_EGRESS_ENTRY];
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src1_dst1_port_p_entry_n_s_v2 eth_src1_dst1_port[NUM_OF_FHP][NUM_EGRESS_ENTRY];
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src0_port_p_entry_n_s_v2 eth_src0_port[NUM_OF_FHP][NUM_EGRESS_ENTRY];
	ecpriss_dst_ip_addr_cfg_s_v2 dst_ip_addr[NUM_OF_FHP][NUM_EGRESS_ENTRY];
	ecpriss_src_ip_addr_cfg_s_v2 src_ip_addr[NUM_OF_FHP][NUM_EGRESS_ENTRY];
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_sa_tag_ip_tos_misc_port_p_entry_n_s_v2 sa_ip_tos_misc_port[NUM_OF_FHP][NUM_EGRESS_ENTRY];
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_ipv4_fields_p_s_v2 ipv4_cfg[NUM_OF_FHP];
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_ipv6_fields_p_s_v2 ipv6_cfg[NUM_OF_FHP];
}ecpriss_qudp_egress_cfg_s_v2;


typedef struct ecpriss_ip_addr_fltr_cfg{
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr0_port_p_entry_n_s dst_ip0;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr1_port_p_entry_n_s dst_ip1;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr2_port_p_entry_n_s dst_ip2;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr3_port_p_entry_n_s dst_ip3;
}ecpriss_ip_addr_fltr_cfg_s;

typedef struct ecpriss_ip_addr_fltr_cfg_v2{
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr0_port_p_entry_n_s_v2 dst_ip0;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr1_port_p_entry_n_s_v2 dst_ip1;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr2_port_p_entry_n_s_v2 dst_ip2;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr3_port_p_entry_n_s_v2 dst_ip3;
}ecpriss_ip_addr_fltr_cfg_s_v2;


typedef struct ecpriss_mac_addr_fltr_cfg{
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_lsb_port_p_entry_n_s mac_lsb;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_msb_port_p_entry_n_s mac_msb;
}ecpriss_mac_addr_fltr_cfg_s;

typedef struct ecpriss_mac_addr_fltr_cfg_v2{
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_lsb_port_p_entry_n_s_v2 mac_lsb;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_msb_port_p_entry_n_s_v2 mac_msb;
}ecpriss_mac_addr_fltr_cfg_s_v2;


typedef struct ecpriss_qudp_ingress_cfg_value{
	ecpri_qudp_hwio_def_ecpri_udp_fh_ingress_config_p_s global_cfg[NUM_OF_FHP];
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_vlan_addr_port_p_entry_n_s vlan[NUM_OF_FHP][NUM_OF_FLTR];
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_classification_list_port_p_entry_n_s udp_clss[NUM_OF_FHP][NUM_OF_FLTR];
	ecpriss_ip_addr_fltr_cfg_s ip_addr[NUM_OF_FHP][NUM_OF_FLTR];
	ecpriss_mac_addr_fltr_cfg_s mac_addr[NUM_OF_FHP][NUM_OF_FLTR];
}ecpriss_qudp_ingress_cfg_value_s;

typedef struct ecpriss_qudp_ingress_cfg_value_v2{
	ecpri_qudp_hwio_def_ecpri_udp_fh_ingress_config_p_s_v2 global_cfg[NUM_OF_FHP];
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_vlan_addr_port_p_entry_n_s_v2 vlan[NUM_OF_FHP][NUM_OF_FLTR];
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_classification_list_port_p_entry_n_s_v2 udp_clss[NUM_OF_FHP][NUM_OF_FLTR];
	ecpriss_ip_addr_fltr_cfg_s_v2 ip_addr[NUM_OF_FHP][NUM_OF_FLTR];
	ecpriss_mac_addr_fltr_cfg_s_v2 mac_addr[NUM_OF_FHP][MAX_MAC_FILTER_ENTRIES];
}ecpriss_qudp_ingress_cfg_value_s_v2;


typedef struct ecpriss_qudp_ingress_cfg_valid_bits{
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_vlan_addr_port_p_entries_valid_bits_s vlan[NUM_OF_FHP];
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_classification_list_port_p_entries_valid_bits_s udp_clss[NUM_OF_FHP];
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr_port_p_entries_valid_bits_s ip_addr[NUM_OF_FHP];
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_port_p_entries_valid_bits_s mac_addr[NUM_OF_FHP];
}ecpriss_qudp_ingress_cfg_valid_bits_s;

typedef struct ecpriss_qudp_ingress_cfg_valid_bits_v2{
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_vlan_addr_port_p_entries_valid_bits_s_v2 vlan[NUM_OF_FHP];
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_classification_list_port_p_entries_valid_bits_s_v2 udp_clss[NUM_OF_FHP];
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr_port_p_entries_valid_bits_s_v2 ip_addr[NUM_OF_FHP];
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_port_p_entries_valid_bits_s_v2 mac_addr[NUM_OF_FHP];
}ecpriss_qudp_ingress_cfg_valid_bits_s_v2;


typedef struct ecpriss_qudp_ingress_cfg{
	ecpriss_qudp_ingress_cfg_value_s cfg;
	ecpriss_qudp_ingress_cfg_valid_bits_s vbits;
}ecpriss_qudp_ingress_cfg_s;

typedef struct ecpriss_qudp_ingress_cfg_v2{
	ecpriss_qudp_ingress_cfg_value_s_v2 cfg;
	ecpriss_qudp_ingress_cfg_valid_bits_s_v2 vbits;
}ecpriss_qudp_ingress_cfg_s_v2;


typedef struct ecpriss_qudp_cfg{
	ecpriss_qudp_egress_cfg_s  egress;
	ecpriss_qudp_ingress_cfg_s ingress;
}ecpriss_qudp_cfg_s;

typedef struct ecpriss_qudp_cfg_v2{
	ecpriss_qudp_egress_cfg_s_v2  egress;
	ecpriss_qudp_ingress_cfg_s_v2 ingress;
}ecpriss_qudp_cfg_s_v2;


typedef struct ecpriss_config_stats
{
	ecpriss_xbar_cfg_s xbar_cfg;
	ecpriss_qudp_cfg_s qudp_cfg;
}ecpriss_config_stats_s;

typedef struct ecpriss_config_stats_v2
{
	ecpriss_xbar_cfg_s_v2 xbar_cfg_v2;
	ecpriss_qudp_cfg_s_v2 qudp_cfg_v2;
}ecpriss_config_stats_s_v2;


int ecpriss_xbar_cold_init(struct device *dev);
int ecpriss_xbar_fh_rx_lut(uint32_t port_index,
		ecpriss_flow_rx_cfg_s *xbar_rx_cfg);

int ecpriss_xbar_c2c_lut(void);
int ecpriss_xbar_l2_lut(void);
int ecpriss_xbar_oc_rx_lut(uint32_t port_index,
		ecpriss_flow_tx_cfg_s *xbar_tx_cfg);
void ecpriss_xbar_non_ecpri_lut_cfg(void);
void ecpriss_xbar_stats_update(void);
void ecpriss_xbar_config_stats_update(void);




int ecpriss_xbar_cold_init_v2(struct device *dev);
int ecpriss_xbar_fh_rx_lut_v2(uint32_t port_index,
		ecpriss_flow_rx_cfg_s *xbar_rx_cfg);

int ecpriss_xbar_oc_rx_lut_v2(uint32_t port_index,
		ecpriss_flow_tx_cfg_s *xbar_tx_cfg);


int ecpriss_xbar_fh_rx_lut_decfg_v2(uint32_t port_index,
		ecpriss_flow_rx_cfg_s *xbar_rx_cfg);

int ecpriss_xbar_oc_rx_lut_decfg_v2(uint32_t port_index,
		ecpriss_flow_tx_cfg_s *xbar_tx_cfg);

void ecpriss_xbar_non_ecpri_lut_cfg(void);
void ecpriss_xbar_stats_update(void);
void ecpriss_xbar_stats_update_v2(void);
void ecpriss_fh_xbar_stats_update_usr(void);
void ecpriss_xbar_config_stats_update(void);
void ecpriss_xbar_config_stats_update_v2(void);

void ecpriss_xbar_destroy_interrupts_v2(void);

void ecpriss_xbar_delete_pcid_entry(uint16_t *configured_pcids,uint16_t pcid_value,uint16_t num_pcid_entry);

void ecpriss_xbar_set_logging_route(ecpriss_log_cfg_s *log_cfg);

int ecpriss_xbar_fh_rx_lut_v2_logging(uint32_t port_index,
		uint16_t pcid_index , ecpriss_log_dir_e log_dir,
		ecpriss_logging_action action);

int ecpriss_xbar_c2c_rx_lut_v2_logging(uint32_t port_index,
		uint16_t pcid_index , ecpriss_log_dir_e log_dir,
		ecpriss_logging_action action);

void ecpriss_xbar_fhrx_default_dma_channel(void);

void ecpriss_xbar_c2crx_default_dma_channel(void);

void ecpriss_xbar_set_cascade_default_c2c1_lut_route_to_fh_v2(void);
#endif

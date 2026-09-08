/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ECPRISS_QUDP_H_
#define ECPRISS_QUDP_H_

#include "ecpriss_qudp_hal.h"
#include "ecpriss_flow.h"
#include "ecpriss_core.h"
#include "ecpriss_netlink.h"
#include "csm_lte_eth_dev.h" 

#define MAX_WHITELIST_ENTRIES       16
#define QUDP_IRQ_MAX                6
#define ECPRISS_MAX_LINKS           4
#define ECPRISS_MAX_PORT_TYPE       2
#define MAX_QUDP_WM_ENTRY    10

#define ECPRISS_QUDP_ACTION_PASS_TO_UC 0
#define ECPRISS_QUDP_ACTION_PASS_TO_A55 1
#define ECPRISS_QUDP_ACTION_DISCARD 2
#define ECPRISS_QUDP_ACTION_PASS_TO_REMOTE 3
#define ECPRISS_QUDP_ACTION_CONTINUE 4

typedef enum
{
	ECPRISS_UDP_C2C_IRQ_PORT0,
	ECPRISS_UDP_C2C_IRQ_PORT1,
	ECPRISS_UDP_FH_IRQ_PORT0,
	ECPRISS_UDP_FH_IRQ_PORT1,
	ECPRISS_UDP_FH_IRQ_PORT2,
	ECPRISS_UDP_L2_IRQ
}ecpriss_qudp_interrupt_events_e;


/**
 * enum ecpriss_port_dir
 */
typedef enum {
	ECPRISS_PORT_0 = 0,
	ECPRISS_PORT_1,
	ECPRISS_PORT_2,
	ECPRISS_PORT_MAX
}ecpriss_port_idx_e;



typedef enum
{
	ECPRI_QUDP_DEINIT,
	ECPRI_QUDP_READY

}epriss_qudp_state_e;

typedef struct ecpriss_mac_filtr
{
	uint32_t lsb;
	uint32_t msb;
}ecpriss_mac_fltr;
/**
 *
 */
typedef struct ecpriss_qudp_ingress_per_port_cfg
{
	uint32_t	num_ip_fltr_entries;
	uint32_t	num_vlan_fltr_entries;
	uint32_t	num_udp_fltr_entries;
	uint32_t
		ipdst_addr[MAX_WHITELIST_ENTRIES][ECPRISS_IP_ADDR_MAX_WORDS];
	uint32_t	vlan_addr[MAX_WHITELIST_ENTRIES];
	uint32_t        udp_port[MAX_WHITELIST_ENTRIES];
	//change to smaller struct with only filter values
	ecpri_qudp_hwio_def_ecpri_udp_fh_ingress_config_p_s   fh_ingress_config;
	ecpri_qudp_hwio_def_ecpri_udp_l2_ingress_config_p_s   l2_ingress_config;
}ecpriss_qudp_ingress_per_port_cfg_s;

typedef struct ecpriss_qudp_ingress_per_port_cfg_v2
{
	uint32_t	num_ip_fltr_entries;
	uint32_t	num_vlan_fltr_entries;
	uint32_t	num_udp_fltr_entries;
	uint32_t	num_mac_fltr_entries;
	ecpriss_mac_fltr dmac[MAX_MAC_FILTER_ENTRIES];
	uint32_t
		ipdst_addr[MAX_WHITELIST_ENTRIES][ECPRISS_IP_ADDR_MAX_WORDS];
	uint32_t	vlan_addr[MAX_WHITELIST_ENTRIES];
	uint32_t        udp_port[MAX_WHITELIST_ENTRIES];
	//change to smaller struct with only filter values
	ecpri_qudp_hwio_def_ecpri_udp_fh_ingress_config_p_s_v2   fh_ingress_config;
	ecpri_qudp_hwio_def_ecpri_udp_l2_ingress_config_p_s_v2   l2_ingress_config;
}ecpriss_qudp_ingress_per_port_cfg_s_v2;


/**
 *
 */
typedef struct ecpriss_qudp_egress_port_cfg
{
	uint32_t	num_l2_tbl_entries;
	uint32_t	num_l3_tbl_entries;
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_mtu_p_s   egress_eth_mtu;
}ecpriss_qudp_egress_per_port_cfg_s;

typedef struct ecpriss_qudp_egress_port_cfg_v2
{
	uint32_t	num_l2_tbl_entries;
	bool		l2_tbl_valid_entry[NUM_EGRESS_ENTRY];
	uint32_t	num_l3_tbl_entries;
	bool		l3_tbl_valid_entry[NUM_EGRESS_ENTRY];
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_udp_ports_port_p_entry_n_s_v2 udp_ports[NUM_EGRESS_ENTRY];
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_vlan_ethertype_port_p_entry_n_s_v2  vlan_ethertype[NUM_EGRESS_ENTRY];
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_dst0_port_p_entry_n_s_v2 eth_dst0_port[NUM_EGRESS_ENTRY];
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src1_dst1_port_p_entry_n_s_v2 eth_src1_dst1_port[NUM_EGRESS_ENTRY];
        ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src0_port_p_entry_n_s_v2 eth_src0_port[NUM_EGRESS_ENTRY];
	ecpriss_dst_ip_addr_cfg_s_v2 dst_ip_addr[NUM_EGRESS_ENTRY];
	ecpriss_src_ip_addr_cfg_s_v2 src_ip_addr[NUM_EGRESS_ENTRY];
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_mtu_p_s_v2 egress_eth_mtu;
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_config_p_s_v2 fh_egress_config;
}ecpriss_qudp_egress_per_port_cfg_s_v2;


/**
 *
 */
typedef struct ecpriss_qudp_trap_cfg
{
	//place holder
}ecpriss_qudp_trap_cfg_s;
typedef struct ecpriss_qudp_trap_cfg_v2
{
	//place holder
}ecpriss_qudp_trap_cfg_s_v2;


/**
 *
 */
typedef struct ecpriss_qudp_interrupt_cfg
{
	//place holder
}ecpriss_qudp_interrupt_cfg_s;
typedef struct ecpriss_qudp_interrupt_cfg_v2
{
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_status_0_port_p_s_v2 fh_udp_sw_irq_status_0_port_p;
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_status_1_port_p_s_v2 fh_udp_sw_irq_status_1_port_p;
}ecpriss_qudp_interrupt_cfg_s_v2;


typedef struct ecpriss_qudp_stats
{
	uint64_t egress_num_udp_packets[ECPRISS_MAX_LINKS];
	uint64_t egress_num_eth_packets[ECPRISS_MAX_LINKS];
	uint64_t egress_num_bypassed_packets[ECPRISS_MAX_LINKS];
	uint64_t egress_num_mtu_err_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_udp_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_non_udp_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_fcs_err_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_ipv4_cs_err_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_udp_cs_err_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_vlan_filtered_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_sec_err_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_ip_len_err_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_ip_filtered_packets[ECPRISS_MAX_LINKS];
	uint32_t fh_egress_udp_watermark_port_p_aligner_output_fifo;
	uint32_t fh_egress_udp_watermark_port_p_cs_update_fifo;
	uint32_t fh_egress_udp_watermark_port_p_cs_calc_fifo;
	uint32_t fh_egress_udp_watermark_port_p_hdri_output_fifo;
	uint32_t fh_egress_udp_watermark_port_p_hdri_cfg_index_fifo;
	uint32_t fh_egress_udp_watermark_port_p_pkt_fifo;
	uint32_t fh_ingress_udp_watermark_port_p_link_n_ptp_timestamp_fifo[ECPRISS_MAX_LINKS];
	uint32_t fh_ingress_udp_watermark_port_p_link_n_pkt_handler_sync_fifos[ECPRISS_MAX_LINKS];
	uint32_t fh_ingress_udp_watermark_port_p_link_n_cmd_fifo[ECPRISS_MAX_LINKS];
	uint32_t fh_ingress_udp_watermark_port_p_link_n_pkt_fifo[ECPRISS_MAX_LINKS];

	uint32_t fh_ingress_wm_ptp_fifo[ECPRISS_MAX_LINKS][MAX_QUDP_WM_ENTRY];
	uint32_t fh_ingress_wm_sync_fifo[ECPRISS_MAX_LINKS][MAX_QUDP_WM_ENTRY];
	uint32_t fh_ingress_wm_cmd_fifo[ECPRISS_MAX_LINKS][MAX_QUDP_WM_ENTRY];
	uint32_t fh_ingress_wm_pkt_fifo[ECPRISS_MAX_LINKS][MAX_QUDP_WM_ENTRY];

	uint32_t fh_egress_output_fifo[MAX_QUDP_WM_ENTRY];
	uint32_t fh_egress_cs_update_fifo[MAX_QUDP_WM_ENTRY];
	uint32_t fh_egress_cs_calc_fifo[MAX_QUDP_WM_ENTRY];
	uint32_t fh_egress_hdri_output_fifo[MAX_QUDP_WM_ENTRY];
	uint32_t fh_egress_hdri_cfg_index_fifo[MAX_QUDP_WM_ENTRY];
	uint32_t fh_egress_pkt_fifo[MAX_QUDP_WM_ENTRY];

	uint64_t curr_wm_index;
}ecpriss_qudp_stats;

typedef struct ecpriss_qudp_stats_v2
{
	uint64_t egress_num_udp_packets[ECPRISS_MAX_LINKS];
	uint64_t egress_num_eth_packets[ECPRISS_MAX_LINKS];
	uint64_t egress_num_bypassed_packets[ECPRISS_MAX_LINKS];
	uint64_t egress_num_mtu_err_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_eth_udp_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_fcs_err_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_ipv4_cs_err_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_udp_cs_err_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_ip_filtered_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_vlan_filtered_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_sec_err_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_ip_len_err_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_eth_ecpri_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_eth_ptp_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_eth_other_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_udp_ecpri_or_nfapi_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_udp_ptp_packets[ECPRISS_MAX_LINKS];
	uint64_t ingress_num_udp_other_packets[ECPRISS_MAX_LINKS];
}ecpriss_qudp_stats_v2;


typedef struct ecpriss_qudp_interrupt_stats
{
	uint32_t egress_mtu_err_packet_link[ECPRISS_MAX_LINKS];
	uint32_t ingress_fcs_err_packet_link[ECPRISS_MAX_LINKS];
	uint32_t ingress_pkt_fifo_empty_before_eop_link[ECPRISS_MAX_LINKS];
	uint32_t ingress_ipv4_cs_error_link[ECPRISS_MAX_LINKS];
	uint32_t ingress_udp_cs_error_link[ECPRISS_MAX_LINKS];
	uint32_t ingress_ip_filtered_packet_link[ECPRISS_MAX_LINKS];
	uint32_t ingress_vlan_filtered_packet_link[ECPRISS_MAX_LINKS];
	uint32_t ingress_sec_err_packet_link[ECPRISS_MAX_LINKS];
	uint32_t ingress_ip_len_err_packet_link[ECPRISS_MAX_LINKS];
	uint32_t ingress_trap_rule_0_link[ECPRISS_MAX_LINKS];
	uint32_t ingress_trap_rule_1_link[ECPRISS_MAX_LINKS];
	uint32_t ingress_trap_rule_2_link[ECPRISS_MAX_LINKS];
	uint32_t ingress_trap_rule_3_link[ECPRISS_MAX_LINKS];
	uint32_t
	ingress_last_in_chain_non_local_dst_packet_link[ECPRISS_MAX_LINKS];
}ecpriss_qudp_interrupt_stats_s;

typedef struct ecpriss_qudp_interrupt_stats_v2
{
	uint64_t egress_mtu_err_packet_link[ECPRISS_MAX_LINKS];
	uint64_t ingress_fcs_err_packet_link[ECPRISS_MAX_LINKS];
	uint64_t ingress_pkt_fifo_empty_before_eop_link[ECPRISS_MAX_LINKS];
	uint64_t ingress_ipv4_cs_error_link[ECPRISS_MAX_LINKS];
	uint64_t ingress_udp_cs_error_link[ECPRISS_MAX_LINKS];
	uint64_t ingress_ip_filtered_packet_link[ECPRISS_MAX_LINKS];
	uint64_t ingress_vlan_filtered_packet_link[ECPRISS_MAX_LINKS];
	uint64_t ingress_sec_err_packet_link[ECPRISS_MAX_LINKS];
	uint64_t ingress_ip_len_err_packet_link[ECPRISS_MAX_LINKS];
	uint64_t ingress_trap_rule_0_link[ECPRISS_MAX_LINKS];
	uint64_t ingress_trap_rule_1_link[ECPRISS_MAX_LINKS];
	uint64_t ingress_trap_rule_2_link[ECPRISS_MAX_LINKS];
	uint64_t ingress_trap_rule_3_link[ECPRISS_MAX_LINKS];
	uint64_t
	ingress_last_in_chain_non_local_dst_packet_link[ECPRISS_MAX_LINKS];
	uint32_t ingress_timestamped_packets_bw_too_high_link[ECPRISS_MAX_LINKS];
}ecpriss_qudp_interrupt_stats_s_v2;


/**
 *
 */
typedef struct ecpriss_qudp_port_cfg
{
	//ecpriss_port_type_e                   port_type;
	ecpriss_port_idx_e                    port_index;
	ecpriss_qudp_ingress_per_port_cfg_s   ingress_port_cfg;
	ecpriss_qudp_egress_per_port_cfg_s    egress_cfg;
	ecpriss_qudp_trap_cfg_s               trap_cfg;
	ecpriss_qudp_interrupt_cfg_s          interrupt_cfg;
	ecpriss_qudp_interrupt_stats_s        interrupt_stats;
	ecpriss_qudp_stats                    stats;
	eth_ecpriss_port_params_s             eth_cfg;
}ecpriss_qudp_port_cfg_s;

typedef struct ecpriss_qudp_port_cfg_v2
{
	//ecpriss_port_type_e                   port_type;
	ecpriss_port_idx_e			port_index;
	ecpriss_qudp_ingress_per_port_cfg_s_v2  ingress_port_cfg;
	ecpriss_qudp_egress_per_port_cfg_s_v2   egress_cfg;
	ecpriss_qudp_trap_cfg_s			trap_cfg;
	ecpriss_qudp_interrupt_cfg_s_v2         interrupt_cfg_v2;
	ecpriss_qudp_interrupt_stats_s_v2       interrupt_stats_v2;
	ecpriss_qudp_stats_v2                   stats_v2;
	eth_ecpriss_port_params_s		eth_cfg;
	uint8_t					strict_filter_status;
}ecpriss_qudp_port_cfg_s_v2;


/**
 *
 */
typedef struct ecpriss_qudp_ctx
{
	epriss_qudp_state_e                 state;
	uint32_t                            num_ports;
	ecpriss_qudp_port_cfg_s             fh_port_cfg[ECPRISS_PORT_MAX];
	ecpriss_qudp_port_cfg_s             c2c_port_cfg[ECPRISS_PORT_MAX];
	ecpriss_qudp_port_cfg_s             l2_port_cfg[ECPRISS_PORT_MAX];
	//Todo:change to typedef
	ecpriss_qudp_hal_context_s         *ecpriss_qudp_hal_ctx;
}ecpriss_qudp_ctx_s;

typedef struct ecpriss_qudp_ctx_v2
{
	epriss_qudp_state_e                 state;
	uint32_t                            num_ports[ETH_ECPRISS_PORT_TYPE_MAX];
	ecpriss_qudp_port_cfg_s_v2          fh_port_cfg_v2[ECPRISS_PORT_MAX];
	ecpriss_qudp_port_cfg_s_v2          c2c_port_cfg_v2[ECPRISS_PORT_MAX];
	ecpriss_qudp_port_cfg_s_v2          l2_port_cfg_v2[ECPRISS_PORT_MAX];
	int                                 lte_fh_enabled;
	//Todo:change to typedef
	ecpriss_qudp_hal_context_s         *ecpriss_qudp_hal_ctx;
}ecpriss_qudp_ctx_s_v2;

enum {
	ECPRISS_MAC_ACTION_PASS_TO_UC = 0,
	ECPRISS_MAC_ACTION_PASS_TO_A55,
	ECPRISS_MAC_ACTION_DISCARD,
	ECPRISS_MAC_ACTION_PASS_TO_REMOTE_QUDP,
	ECPRISS_MAC_ACTION_CONTINUE_NORMAL_PROCESSING
};

int ecpriss_qudp_init_v2(struct device *dev);
int ecpriss_qudp_fh_tx_hdr_ins_cfg_v2(uint32_t               port_index,
		ecpriss_qudp_tx_cfg_s *tx_cfg,
		ecpriss_transp_type tp_type);
int ecpriss_qudp_fh_rx_filter_cfg_v2(uint32_t               port_index,
		ecpriss_qudp_rx_cfg_s *rx_cfg);

void debug_qudp_ingress_config_v2(void);
void debug_qudp_egress_config_v2(void);


int ecpriss_qudp_fh_tx_hdr_decfg_v2(uint32_t               port_index,
		ecpriss_qudp_tx_cfg_s *tx_cfg);


int ecpriss_qudp_fh_rx_filter_decfg_v2(uint32_t               port_index,
		ecpriss_qudp_rx_cfg_s *rx_cfg);



void ecpriss_qudp_print_c2c_ingress_stats_v2(uint32_t port_index,
		uint32_t link_index);
void ecpriss_qudp_print_c2c_egress_stats_v2(uint32_t port_index,
		uint32_t link_index);

void ecpriss_qudp_fh_ingress_stats_update(uint32_t port_index, uint32_t link_index);
void ecpriss_qudp_fh_ingress_stats_update_v2(uint32_t port_index, uint32_t link_index);
void ecpriss_fh_qudp_stats_update_usr(void);

void ecpriss_qudp_fh_egress_stats_update(uint32_t port_index, uint32_t link_index);
void ecpriss_qudp_fh_egress_stats_update_v2(uint32_t port_index, uint32_t link_index);

void ecpriss_qudp_ingress_config_stats_update_v2(int32_t fh_index);
void ecpriss_qudp_ingress_config_stats_update(int32_t fh_index);

void ecpriss_qudp_egress_config_stats_update(int32_t fh_index);
void ecpriss_qudp_egress_config_stats_update_v2(int32_t fh_index);

void ecpriss_qudp_print_l2_ingress_stats_v2(uint32_t port_index,
		uint32_t link_index);
void ecpriss_qudp_print_l2_egress_stats_v2(uint32_t port_index,
		uint32_t link_index);
void ecpriss_qudp_clear_stats_v2(uint32_t port_index,
		uint32_t link_index);


int ecpriss_qudp_init(struct device *dev);
int ecpriss_qudp_fh_tx_hdr_ins_cfg(uint32_t               port_index,
		ecpriss_qudp_tx_cfg_s *tx_cfg);
int ecpriss_qudp_fh_rx_filter_cfg(uint32_t               port_index,
		ecpriss_qudp_rx_cfg_s *rx_cfg);

int ecpriss_qudp_fh_egress_cfg_reset(int32_t port_index);
int ecpriss_qudp_fh_egress_cfg_reset_v2(int32_t port_index);
int ecpriss_qudp_l2_egress_cfg_reset_v2(int32_t port_index);

void ecpriss_qudp_fh_ingress_stats_update(uint32_t port_index,
		uint32_t link_index);
void ecpriss_qudp_fh_egress_stats_update(uint32_t port_index,
		uint32_t link_index);
void ecpriss_qudp_ingress_config_stats_update(int32_t fh_index);
void debug_qudp_ingress_config(void);
void ecpriss_qudp_egress_config_stats_update(int32_t fh_index);
void debug_qudp_egress_config(void);

void ecpriss_qudp_print_c2c_ingress_stats(uint32_t port_index,
		uint32_t link_index);
void ecpriss_qudp_print_c2c_egress_stats(uint32_t port_index,
		uint32_t link_index);


void ecpriss_qudp_print_l2_ingress_stats(uint32_t port_index,
		uint32_t link_index);
void ecpriss_qudp_print_l2_egress_stats(uint32_t port_index,
		uint32_t link_index);
void ecpriss_qudp_clear_stats(uint32_t port_index,
		uint32_t link_index);

void ecpriss_qudp_set_ecpriss_filt_enable_info(int val);
int ecpriss_qudp_get_ecpriss_filt_enable_info(void);
void ecpriss_qudp_non_ecpri_dma_ring_info(void) ;
void ecpriss_qudp_irq_destroy_v2(void);

int ecpriss_qudp_ingress_init_cfg_modify_v2(int action);

int ecpriss_qudp_get_ingress_action(void);
void ecpriss_qudp_set_ingress_action(int val);

int ecpriss_qudp_get_strict_filter_config(int fh_index);
void ecpriss_qudp_set_strict_filter_config(int val, int fh_index);
int32_t ecpriss_qudp_egress_l2_table_reconfig(ecpriss_packet_payload_s *packet);
int32_t ecpriss_qudp_egress_l3_table_reconfig(ecpriss_packet_payload_s *packet);
int32_t ecpriss_qudp_egress_l2_l3_table_reconfig(ecpriss_packet_payload_s *packet);
int32_t ecpriss_qudp_ingress_table_config(ecpriss_packet_payload_s *packet);
int32_t ecpriss_qudp_ingress_table_deconfig(ecpriss_packet_payload_s *packet);
void ecpriss_qudp_add_loopback_filters(ecpriss_packet_payload_s *packet);
void ecpriss_qudp_remove_loopback_filters(ecpriss_packet_payload_s *packet);

int32_t ecpriss_qudp_set_lte_mac_filter(ecpriss_packet_payload_s *packet);
void ecpriss_qudp_set_nr_mac_filter(void);
void ecpriss_qudp_set_lte_mac_filter_info(void);
void ecpriss_qudp_set_nr_mac_filter_info(void);

int ecpriss_qudp_set_lte_mac_addr(int port, int index, csm_lte_ethdev_mac_s *mac_info);
int ecpriss_qudp_get_lte_mac_addr(int port, int index, csm_lte_ethdev_mac_s *mac_info);

int ecpriss_qudp_l2_egress_tp_cfg_v2(int32_t port_index);

int ecpri_global_cfg_init_cascade_mode(void);
int ecpri_global_cfg_deinit_cascade_mode(void);

void ecpriss_qudp_set_cascade_fh_mac_dst_check_v2(void);
void ecpriss_qudp_set_cascade_l2_mac_dst_check_v2(void);
void ecpriss_qudp_set_cascade_arp_trap_rules_v2(void);
void ecpriss_qudp_set_cascade_icmp_trap_rules_v2(void);

#endif

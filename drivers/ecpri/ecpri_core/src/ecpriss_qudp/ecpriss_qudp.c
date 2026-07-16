/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/string.h>
#include "ecpriss_core.h"
//#include "ecpriss_qudp_hal.h"
#include "ecpriss_log.h"
#include "ecpriss_xbar_hwio_v2.h"
#include "ecpriss_qudp.h"
#include "ecpriss_flow.h"

volatile int ecpriss_filtering_enabled = 0;
volatile int ecpriss_qudp_ingress_action = ECPRISS_QUDP_ACTION_PASS_TO_A55;
extern struct ecpri_dma_endp_mapping dma_endp_g;
extern struct eth_ecpriss_ops mtip_ecpri_ops;
extern eth_ecpriss_topology_root_s        eth_link_params_g;


#define ECPRISS_ETH_QUDP_MTU_SIZE_V4   9000
#define ECPRISS_ETH_QUDP_MTU_SIZE_V6   9000
#define ECPRISS_QUDP_REG_FIELD_ENABLE  1
#define ENABLE_FILTER                  1
#define BYTE_SHIFT                     8
#define MSB_SHIFT                      32
#define CONFIGURE		       1
#define DE_CONFIGURE		       0
#define ECPRISS_RESERVED_OVERRIDE_INDEX 255

int qudp_irq_mapping[QUDP_IRQ_MAX];

ecpriss_loopback_filter_cfg_s ecpriss_loopback_filters_list = {0};


void ecpriss_qudp_clear_stats_v2(uint32_t port_index, uint32_t link_index)
{
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS ,
		ECPRI_UDP_FH_INGRESS_NUM_ETH_UDP_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_FCS_ERR_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_IPV4_CS_ERROR_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_UDP_CS_ERROR_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_IP_FILTERED_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_VLAN_FILTERED_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_SEC_ERR_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_IP_LEN_ERR_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_NUM_ETH_ECPRI_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_NUM_ETH_PTP_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_NUM_ETH_OTHER_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_NUM_UDP_ECPRI_OR_NFAPI_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_NUM_UDP_PTP_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_NUM_UDP_OTHER_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_EGRESS_NUM_UDP_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_EGRESS_NUM_ETH_ONLY_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_EGRESS_NUM_BYPASSED_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_EGRESS_MTU_ERR_PACKETS_PORT_p_LINK_n_V2,
		port_index, link_index,0);

}
void ecpriss_qudp_clear_stats(uint32_t port_index, uint32_t link_index)
{
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS ,
		ECPRI_UDP_FH_EGRESS_NUM_UDP_PACKETS_LSB_PORT_p_LINK_n,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_EGRESS_NUM_UDP_PACKETS_MSB_PORT_p_LINK_n,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_EGRESS_NUM_ETH_ONLY_PACKETS_LSB_PORT_p_LINK_n,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_EGRESS_NUM_ETH_ONLY_PACKETS_MSB_PORT_p_LINK_n,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_EGRESS_NUM_BYPASSED_PACKETS_LSB_PORT_p_LINK_n,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_EGRESS_NUM_BYPASSED_PACKETS_MSB_PORT_p_LINK_n,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_EGRESS_MTU_ERR_PACKETS_LSB_PORT_p_LINK_n,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_EGRESS_MTU_ERR_PACKETS_MSB_PORT_p_LINK_n,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_NUM_UDP_PACKETS_LSB_PORT_p_LINK_n,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_NUM_UDP_PACKETS_MSB_PORT_p_LINK_n,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_NUM_NON_UDP_PACKETS_LSB_PORT_p_LINK_n,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_NUM_NON_UDP_PACKETS_MSB_PORT_p_LINK_n,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_FCS_ERR_PACKETS_LSB_PORT_p_LINK_n,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_FCS_ERR_PACKETS_MSB_PORT_p_LINK_n,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_IPV4_CS_ERROR_PACKETS_LSB_PORT_p_LINK_n,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_IPV4_CS_ERROR_PACKETS_MSB_PORT_p_LINK_n,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_UDP_CS_ERROR_PACKETS_LSB_PORT_p_LINK_n,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_UDP_CS_ERROR_PACKETS_MSB_PORT_p_LINK_n,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_VLAN_FILTERED_PACKETS_LSB_PORT_p_LINK_n,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_VLAN_FILTERED_PACKETS_MSB_PORT_p_LINK_n,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_SEC_ERR_PACKETS_LSB_PORT_p_LINK_n,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_SEC_ERR_PACKETS_MSB_PORT_p_LINK_n,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_IP_LEN_ERR_PACKETS_LSB_PORT_p_LINK_n,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_IP_LEN_ERR_PACKETS_MSB_PORT_p_LINK_n,
		port_index, link_index,0);

	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_IP_FILTERED_PACKETS_LSB_PORT_p_LINK_n,
		port_index, link_index,0);
	ecpriss_qudp_hal_write_reg_mn(
		ECPRISS_QUDP_FH_RAMS,
		ECPRI_UDP_FH_INGRESS_IP_FILTERED_PACKETS_MSB_PORT_p_LINK_n,
		port_index, link_index,0);

}
void debug_qudp_egress_config(void)
{
	int32_t fh_index = 0;
	int32_t egress_table_index = 0;

	for(fh_index = 0; fh_index < NUM_OF_FHP; fh_index++){

		for(egress_table_index = 0; egress_table_index < NUM_EGRESS_ENTRY; egress_table_index++){
			ECPRILOGDBG("SRC udp_ports[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.udp_ports[fh_index][egress_table_index].src);
			ECPRILOGDBG("DST udp_ports[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.udp_ports[fh_index][egress_table_index].dst);
			ECPRILOGDBG("VLAN vlan_ethertype[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.vlan_ethertype[fh_index][egress_table_index].vlan_data);
			ECPRILOGDBG("EtherType vlan_ethertype[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.vlan_ethertype[fh_index][egress_table_index].ethertype);
			ECPRILOGDBG("Eth_DST0_port eth_dst0_port[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.eth_dst0_port[fh_index][egress_table_index].value);
			ECPRILOGDBG("DST_MSB eth_src1_dst1_port[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.eth_src1_dst1_port[fh_index][egress_table_index].dst_msb);
			ECPRILOGDBG("SRC_MSB eth_src1_dst1_port[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.eth_src1_dst1_port[fh_index][egress_table_index].src_msb);
			ECPRILOGDBG("ETH_src0 eth_src0_port[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.eth_src0_port[fh_index][egress_table_index].value);
			ECPRILOGDBG("DST IP addr0 ip_addr0[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.dst_ip_addr[fh_index][egress_table_index].ip_dst0.value);
			ECPRILOGDBG("DST IP addr1 ip_addr1[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.dst_ip_addr[fh_index][egress_table_index].ip_dst1.value);
			ECPRILOGDBG("DST IP addr2 ip_addr2[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.dst_ip_addr[fh_index][egress_table_index].ip_dst2.value);
			ECPRILOGDBG("DST IP addr3 ip_addr3[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.dst_ip_addr[fh_index][egress_table_index].ip_dst3.value);
			ECPRILOGDBG("SRC IP addr0 ip_addr0[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.src_ip_addr[fh_index][egress_table_index].ip_src0.value);
			ECPRILOGDBG("SRC IP addr1 ip_addr1[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.src_ip_addr[fh_index][egress_table_index].ip_src1.value);
			ECPRILOGDBG("SRC IP addr2 ip_addr2[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.src_ip_addr[fh_index][egress_table_index].ip_src2.value);
			ECPRILOGDBG("SRC IP addr3 ip_addr3[%d][%d]: 0x%x\n",fh_index,egress_table_index,ecpriss_pdata->cfg_stats.qudp_cfg.egress.src_ip_addr[fh_index][egress_table_index].ip_src3.value);
		}
	}
}
// ecpriss_pdata->cfg_stats.qudp_cfg.egress
void ecpriss_qudp_egress_config_stats_update(int32_t fh_index)
{
	int32_t egress_table_index = 0;

	for(egress_table_index = 0; egress_table_index < NUM_EGRESS_ENTRY ; egress_table_index++)
	{

		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_UDP_PORTS_PORT_p_ENTRY_n,
				fh_index,
				egress_table_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.egress.udp_ports[fh_index][egress_table_index]);

		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_VLAN_ETHERTYPE_PORT_p_ENTRY_n,
				fh_index,
				egress_table_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.egress.vlan_ethertype[fh_index][egress_table_index]);

		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_DST0_PORT_p_ENTRY_n,
				fh_index,
				egress_table_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.egress.eth_dst0_port[fh_index][egress_table_index]);

		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_SRC1_DST1_PORT_p_ENTRY_n,
				fh_index,
				egress_table_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.egress.eth_src1_dst1_port[fh_index][egress_table_index]);

		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_SRC0_PORT_p_ENTRY_n,
				fh_index,
				egress_table_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.egress.eth_src0_port[fh_index][egress_table_index]);

		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR0_PORT_p_ENTRY_n,
				fh_index,
				egress_table_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.egress.dst_ip_addr[fh_index][egress_table_index].ip_dst0);
		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR1_PORT_p_ENTRY_n,
				fh_index,
				egress_table_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.egress.dst_ip_addr[fh_index][egress_table_index].ip_dst1);
		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR2_PORT_p_ENTRY_n,
				fh_index,
				egress_table_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.egress.dst_ip_addr[fh_index][egress_table_index].ip_dst2);
		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR3_PORT_p_ENTRY_n,
				fh_index,
				egress_table_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.egress.dst_ip_addr[fh_index][egress_table_index].ip_dst3);

		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR0_PORT_p_ENTRY_n,
				fh_index,
				egress_table_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.egress.src_ip_addr[fh_index][egress_table_index].ip_src0);
		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR1_PORT_p_ENTRY_n,
				fh_index,
				egress_table_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.egress.src_ip_addr[fh_index][egress_table_index].ip_src1);
		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR2_PORT_p_ENTRY_n,
				fh_index,
				egress_table_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.egress.src_ip_addr[fh_index][egress_table_index].ip_src2);
		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR3_PORT_p_ENTRY_n,
				fh_index,
				egress_table_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.egress.src_ip_addr[fh_index][egress_table_index].ip_src3);
	}
	return;
}
void ecpriss_qudp_egress_config_stats_update_v2(int32_t fh_index)
{
	int32_t egress_table_index = 0;

	ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_IPV4_FIELDS_P_V2,
			fh_index,
			&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv4_cfg[fh_index]);

	ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_IPV6_FIELDS_P_V2,
			fh_index,
			&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv6_cfg[fh_index]);

	for(egress_table_index = 0; egress_table_index < NUM_EGRESS_ENTRY ; egress_table_index++)
	{
		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_UDP_PORTS_PORT_p_ENTRY_n_V2,
				fh_index,
				egress_table_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.udp_ports[fh_index][egress_table_index]);

		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_VLAN_ETHERTYPE_PORT_p_ENTRY_n_V2,
				fh_index,
				egress_table_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.vlan_ethertype[fh_index][egress_table_index]);

		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_DST0_PORT_p_ENTRY_n_V2,
				fh_index,
				egress_table_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.eth_dst0_port[fh_index][egress_table_index]);

		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_SRC1_DST1_PORT_p_ENTRY_n_V2,
				fh_index,
				egress_table_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.eth_src1_dst1_port[fh_index][egress_table_index]);

		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_SRC0_PORT_p_ENTRY_n_V2,
				fh_index,
				egress_table_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.eth_src0_port[fh_index][egress_table_index]);

		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR0_PORT_p_ENTRY_n_V2,
				fh_index,
				egress_table_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.dst_ip_addr[fh_index][egress_table_index].ip_dst0);
		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR1_PORT_p_ENTRY_n_V2,
				fh_index,
				egress_table_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.dst_ip_addr[fh_index][egress_table_index].ip_dst1);
		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR2_PORT_p_ENTRY_n_V2,
				fh_index,
				egress_table_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.dst_ip_addr[fh_index][egress_table_index].ip_dst2);
		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR3_PORT_p_ENTRY_n_V2,
				fh_index,
				egress_table_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.dst_ip_addr[fh_index][egress_table_index].ip_dst3);

		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR0_PORT_p_ENTRY_n_V2,
				fh_index,
				egress_table_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.src_ip_addr[fh_index][egress_table_index].ip_src0);
		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR1_PORT_p_ENTRY_n_V2,
				fh_index,
				egress_table_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.src_ip_addr[fh_index][egress_table_index].ip_src1);
		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR2_PORT_p_ENTRY_n_V2,
				fh_index,
				egress_table_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.src_ip_addr[fh_index][egress_table_index].ip_src2);
		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR3_PORT_p_ENTRY_n_V2,
				fh_index,
				egress_table_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.src_ip_addr[fh_index][egress_table_index].ip_src3);
		ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_SA_TAG_IP_TOS_MISC_PORT_p_ENTRY_n_V2,
				fh_index,
				egress_table_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.sa_ip_tos_misc_port[fh_index][egress_table_index]);
	}
	return;
}


void debug_qudp_ingress_config(void)
{
	int i ,j;

	ECPRILOGERR("Global config\n");

	for(i=0; i < NUM_OF_FHP; i++){
		ECPRILOGDBG("0x%x\n",ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.global_cfg[i]);
		ECPRILOGDBG("IP_FLTR: %u\n",ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.global_cfg[i].enable_ip_dst_filt);
		ECPRILOGDBG("UDP_CLASS: %u\n",ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.global_cfg[i].enable_udp_dst_class);
		ECPRILOGDBG("VLAN_FLTR: %u\n",ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.global_cfg[i].enable_vlan_filt);
		ECPRILOGDBG("MAC_FLTR: %u\n",ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.global_cfg[i].enable_mac_dst_check);
	}
	ECPRILOGERR("VLAID BITS \n");
	for(i=0; i < NUM_OF_FHP; i++){
		ECPRILOGDBG("VALID VLAN[%d] 0x%x\n",i,ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.vlan[i].valid_bits);
		ECPRILOGDBG("VLAID UDP[%d] 0x%x\n",i,ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.udp_clss[i].valid_bits);
		ECPRILOGDBG("VLAID IP[%d] 0x%x\n",i,ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.ip_addr[i].valid_bits);
		ECPRILOGDBG("VLAID mac[%d] 0x%x\n",i,ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.mac_addr[i].valid_bits);
	}
	for(i=0; i < NUM_OF_FHP; i++){
		for(j=0; j<NUM_OF_FLTR; j++){
			ECPRILOGDBG("vlan[%d][%d]: 0x%x\t",i,j, ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.vlan[i][j].value);
			ECPRILOGDBG("udp[%d][%d]: 0x%x\t",i,j,ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.udp_clss[i][j].value);
			ECPRILOGDBG("mac_lsb[%d][%d]: 0x%x\t",i,j,ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.mac_addr[i][j].mac_lsb);
			ECPRILOGDBG("mac_msb[%d][%d]: 0x%x\t",i,j,ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.mac_addr[i][j].mac_msb);
			ECPRILOGDBG("ip0[%d][%d]: 0x%x\t",i,j,ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.ip_addr[i][j].dst_ip0.value);
			ECPRILOGDBG("ip1[%d][%d]: 0x%x\t",i,j,ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.ip_addr[i][j].dst_ip1.value);
			ECPRILOGDBG("ip2[%d][%d]: 0x%x\t",i,j,ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.ip_addr[i][j].dst_ip2.value);
			ECPRILOGDBG("ip3[%d][%d]: 0x%x\t",i,j,ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.ip_addr[i][j].dst_ip3.value);
			ECPRILOGDBG("\n");
		}
	}

}
// ecpriss_pdata->cfg_stats.qudp_cfg.ingress
void ecpriss_qudp_ingress_config_stats_update(int32_t fh_index)
{
	uint32_t flag = 1;
	int32_t fltr_index = 0;

	ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_INGRESS_CONFIG_P,
			fh_index,
			&ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.global_cfg[fh_index]);

	ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.vlan[fh_index].valid_bits = 0;
	if(ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.global_cfg[fh_index].enable_vlan_filt){
		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_VLAN_ADDR_PORT_p_ENTRIES_VALID_BITS,
				fh_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.vlan[fh_index]);
	}
	ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.udp_clss[fh_index].valid_bits = 0;
	if(ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.global_cfg[fh_index].enable_udp_dst_class){
		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_CLASSIFICATION_LIST_PORT_p_ENTRIES_VALID_BITS,
				fh_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.udp_clss[fh_index]);
	}
	ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.ip_addr[fh_index].valid_bits = 0;
	if(ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.global_cfg[fh_index].enable_ip_dst_filt){
		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_IP_DST_ADDR_PORT_p_ENTRIES_VALID_BITS,
				fh_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.ip_addr[fh_index]);
	}
	ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.mac_addr[fh_index].valid_bits = 0;
	if(ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.global_cfg[fh_index].enable_mac_dst_check){
		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_MAC_ADDRESS_PORT_p_ENTRIES_VALID_BITS,
				fh_index,
				&ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.mac_addr[fh_index]);
	}
	for(fltr_index = 0; fltr_index < NUM_OF_FLTR ; fltr_index ++){

		flag = flag << fltr_index;
		ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.vlan[fh_index][fltr_index].value = 0;
		if(ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.global_cfg[fh_index].enable_vlan_filt){
			if(ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.vlan[fh_index].valid_bits & flag){
				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_VLAN_ADDR_PORT_p_ENTRY_n,
						fh_index,
						fltr_index,
						&ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.vlan[fh_index][fltr_index]);
			}
		}
		ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip0.value = 0;
		ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip1.value = 0;
		ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip2.value = 0;
		ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip3.value = 0;
		if(ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.global_cfg[fh_index].enable_ip_dst_filt){
			if(ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.ip_addr[fh_index].valid_bits & flag){
				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_IP_DST_ADDR0_PORT_p_ENTRY_n,
						fh_index,
						fltr_index,
						&ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip0);

				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_IP_DST_ADDR1_PORT_p_ENTRY_n,
						fh_index,
						fltr_index,
						&ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip1);

				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_IP_DST_ADDR2_PORT_p_ENTRY_n,
						fh_index,
						fltr_index,
						&ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip2);

				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_IP_DST_ADDR3_PORT_p_ENTRY_n,
						fh_index,
						fltr_index,
						&ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip3);
			}
		}
		ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.udp_clss[fh_index][fltr_index].value = 0;
		if(ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.global_cfg[fh_index].enable_udp_dst_class){
			if(ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.udp_clss[fh_index].valid_bits & flag){
				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_UDP_CLASSIFICATION_LIST_PORT_p_ENTRY_n,
						fh_index,
						fltr_index,
						&ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.udp_clss[fh_index][fltr_index]);
			}
		}
		ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.mac_addr[fh_index][fltr_index].mac_lsb.value = 0;
		ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.mac_addr[fh_index][fltr_index].mac_msb.value = 0;
		if(ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.global_cfg[fh_index].enable_mac_dst_check && fltr_index <= 4){
			if(ecpriss_pdata->cfg_stats.qudp_cfg.ingress.vbits.mac_addr[fh_index].valid_bits & flag){
				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_MAC_ADDRESS_LSB_PORT_p_ENTRY_n,
						fh_index,
						fltr_index,
						&ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.mac_addr[fh_index][fltr_index].mac_lsb);
				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_MAC_ADDRESS_MSB_PORT_p_ENTRY_n,
						fh_index,
						fltr_index,
						&ecpriss_pdata->cfg_stats.qudp_cfg.ingress.cfg.mac_addr[fh_index][fltr_index].mac_msb);
			}
		}

		flag = 1;
	}
	return;
}

void ecpriss_qudp_ingress_config_stats_update_v2(int32_t fh_index)
{
	uint32_t flag = 1;
	int32_t fltr_index = 0;

	ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_INGRESS_CONFIG_P_V2,
			fh_index,
			&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.global_cfg[fh_index]);

	ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.vbits.vlan[fh_index].valid_bits = 0;
	if(ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.global_cfg[fh_index].enable_vlan_filt){
		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_VLAN_ADDR_PORT_p_ENTRIES_VALID_BITS_V2,
				fh_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.vbits.vlan[fh_index]);
	}
	ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.vbits.udp_clss[fh_index].valid_bits = 0;
	if(ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.global_cfg[fh_index].enable_udp_dst_class){
		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_CLASSIFICATION_LIST_PORT_p_ENTRIES_VALID_BITS_V2,
				fh_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.vbits.udp_clss[fh_index]);
	}
	ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.vbits.ip_addr[fh_index].valid_bits = 0;
	if(ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.global_cfg[fh_index].enable_ip_dst_filt){
		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_IP_DST_ADDR_PORT_p_ENTRIES_VALID_BITS_V2,
				fh_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.vbits.ip_addr[fh_index]);
	}
	ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.vbits.mac_addr[fh_index].valid_bits = 0;
	if(ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.global_cfg[fh_index].enable_mac_dst_check){
		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_MAC_ADDRESS_PORT_p_ENTRIES_VALID_BITS_V2,
				fh_index,
				&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.vbits.mac_addr[fh_index]);
	}

	for(fltr_index = 0; fltr_index < NUM_OF_FLTR ; fltr_index ++){

		flag = flag << fltr_index;
		ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.vlan[fh_index][fltr_index].value = 0;
		if(ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.global_cfg[fh_index].enable_vlan_filt){
			if(ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.vbits.vlan[fh_index].valid_bits & flag){
				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_VLAN_ADDR_PORT_p_ENTRY_n_V2,
						fh_index,
						fltr_index,
						&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.vlan[fh_index][fltr_index]);
			}
		}
		ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip0.value = 0;
		ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip1.value = 0;
		ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip2.value = 0;
		ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip3.value = 0;
		if(ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.global_cfg[fh_index].enable_ip_dst_filt){
			if(ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.vbits.ip_addr[fh_index].valid_bits & flag){
				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_IP_DST_ADDR0_PORT_p_ENTRY_n_V2,
						fh_index,
						fltr_index,
						&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip0);

				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_IP_DST_ADDR1_PORT_p_ENTRY_n_V2,
						fh_index,
						fltr_index,
						&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip1);

				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_IP_DST_ADDR2_PORT_p_ENTRY_n_V2,
						fh_index,
						fltr_index,
						&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip2);

				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_IP_DST_ADDR3_PORT_p_ENTRY_n_V2,
						fh_index,
						fltr_index,
						&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.ip_addr[fh_index][fltr_index].dst_ip3);
			}
		}
		ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.udp_clss[fh_index][fltr_index].value = 0;
		if(ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.global_cfg[fh_index].enable_udp_dst_class){
			if(ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.vbits.udp_clss[fh_index].valid_bits & flag){
				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_UDP_CLASSIFICATION_LIST_PORT_p_ENTRY_n_V2,
						fh_index,
						fltr_index,
						&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.udp_clss[fh_index][fltr_index]);
			}
		}
		flag = 1;
	}

	flag = 1;
	if(ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.global_cfg[fh_index].enable_mac_dst_check){
		for(fltr_index = 0; fltr_index < MAX_MAC_FILTER_ENTRIES ; fltr_index++){

			flag = flag << fltr_index;
			ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.mac_addr[fh_index][fltr_index].mac_lsb.value = 0;
			ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.mac_addr[fh_index][fltr_index].mac_msb.value = 0;
			if(ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.vbits.mac_addr[fh_index].valid_bits & flag){
				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_MAC_ADDRESS_LSB_PORT_p_ENTRY_n_V2,
						fh_index,
						fltr_index,
						&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.mac_addr[fh_index][fltr_index].mac_lsb);
				ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_MAC_ADDRESS_MSB_PORT_p_ENTRY_n_V2,
						fh_index,
						fltr_index,
						&ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.ingress.cfg.mac_addr[fh_index][fltr_index].mac_msb);
			}

			flag = 1;
		}
	}
	return;
}
void ecpriss_qudp_fh_egress_stats_update(uint32_t port_index, uint32_t link_index)
{
	uint32_t lsb_val = 0;
	uint64_t msb_val = 0;
	uint64_t val = 0;
	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_EGRESS_NUM_UDP_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);

	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_EGRESS_NUM_UDP_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.egress_num_udp_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_FH_EGRESS_NUM_UDP_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val =ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_EGRESS_NUM_ETH_ONLY_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_EGRESS_NUM_ETH_ONLY_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.egress_num_eth_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_FH_EGRESS_NUM_ETH_ONLY_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_EGRESS_NUM_BYPASSED_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_EGRESS_NUM_BYPASSED_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.egress_num_bypassed_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_FH_EGRESS_NUM_BYPASSED_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_EGRESS_MTU_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val =ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_EGRESS_MTU_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.egress_num_mtu_err_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_FH_EGRESS_MTU_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);
	return;
}
void ecpriss_qudp_fh_egress_stats_update_v2(uint32_t port_index, uint32_t link_index)
{
	uint32_t val = 0;
	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_EGRESS_NUM_UDP_PACKETS_PORT_p_LINK_n_V2,
			port_index,
			link_index);

	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.egress_num_udp_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_EGRESS_NUM_UDP_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	val =ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_EGRESS_NUM_ETH_ONLY_PACKETS_PORT_p_LINK_n_V2,
			port_index,
			link_index);
	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.egress_num_eth_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_EGRESS_NUM_ETH_ONLY_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_EGRESS_NUM_BYPASSED_PACKETS_PORT_p_LINK_n_V2,
			port_index,
			link_index);
	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.egress_num_bypassed_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_EGRESS_NUM_BYPASSED_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_EGRESS_MTU_ERR_PACKETS_PORT_p_LINK_n_V2,
			port_index,
			link_index);
	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.egress_num_mtu_err_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_EGRESS_MTU_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);
	return;
}



void ecpriss_qudp_fh_ingress_stats_update(uint32_t port_index, uint32_t link_index)
{
	uint32_t lsb_val = 0;
	uint64_t msb_val = 0;
	uint64_t val = 0;
	uint32_t curr_wm_index = 0;

	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_udp_watermark_port_p_s fh_egress_udp_watermark_port_p;
	ecpri_qudp_hwio_def_ecpri_udp_fh_ingress_udp_watermark_port_p_link_n_s  fh_ingress_udp_watermark_port_p_link_n;

	curr_wm_index =
		ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.curr_wm_index % MAX_QUDP_WM_ENTRY;

	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.curr_wm_index++;


	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_NUM_UDP_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);

	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_NUM_UDP_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_num_udp_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_NUM_UDP_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_NUM_NON_UDP_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);

	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_NUM_NON_UDP_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_num_non_udp_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_NUM_NON_UDP_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_FCS_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_FCS_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_num_fcs_err_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_FCS_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_IPV4_CS_ERROR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_IPV4_CS_ERROR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_num_ipv4_cs_err_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_IPV4_CS_ERROR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_UDP_CS_ERROR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);

	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_UDP_CS_ERROR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_num_udp_cs_err_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_UDP_CS_ERROR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_VLAN_FILTERED_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_VLAN_FILTERED_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_num_vlan_filtered_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_VLAN_FILTERED_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_SEC_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);

	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_SEC_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);
	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_num_sec_err_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_SEC_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_IP_LEN_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_IP_LEN_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);
	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_ip_len_err_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_IP_LEN_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_IP_FILTERED_PACKETS_LSB_PORT_p_LINK_n,port_index,link_index);

	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_IP_FILTERED_PACKETS_MSB_PORT_p_LINK_n,port_index,link_index);

	val = msb_val >> MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_ip_filtered_packets[link_index]=val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_IP_FILTERED_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_INGRESS_UDP_WATERMARK_PORT_p_LINK_n,
			port_index,
			link_index,
			&fh_ingress_udp_watermark_port_p_link_n);

	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_UDP_WATERMARK_PORT_p_LINK_n, : port_index :%d link_index %d value = %u\n", port_index,link_index,
			fh_ingress_udp_watermark_port_p_link_n);

	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_ingress_udp_watermark_port_p_link_n_ptp_timestamp_fifo[link_index] =
		fh_ingress_udp_watermark_port_p_link_n.ptp_timestamp_fifo;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_ingress_udp_watermark_port_p_link_n_pkt_handler_sync_fifos[link_index] =
		fh_ingress_udp_watermark_port_p_link_n.pkt_handler_sync_fifos;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_ingress_udp_watermark_port_p_link_n_cmd_fifo[link_index] =
		fh_ingress_udp_watermark_port_p_link_n.cmd_fifo;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_ingress_udp_watermark_port_p_link_n_pkt_fifo[link_index] =
		fh_ingress_udp_watermark_port_p_link_n.pkt_fifo;

	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_ingress_wm_ptp_fifo[link_index][curr_wm_index] =
		fh_ingress_udp_watermark_port_p_link_n.ptp_timestamp_fifo;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_ingress_wm_sync_fifo[link_index][curr_wm_index] =
		fh_ingress_udp_watermark_port_p_link_n.pkt_handler_sync_fifos;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_ingress_wm_cmd_fifo[link_index][curr_wm_index] =
		fh_ingress_udp_watermark_port_p_link_n.cmd_fifo;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_ingress_wm_pkt_fifo[link_index][curr_wm_index] =
		fh_ingress_udp_watermark_port_p_link_n.pkt_fifo;

	ecpriss_qudp_hal_read_reg_mn_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_UDP_WATERMARK_PORT_p,
			port_index,
			link_index,
			&fh_egress_udp_watermark_port_p);

	ECPRILOGDBG("ECPRI_UDP_FH_EGRESS_UDP_WATERMARK_PORT_p, : port_index :%d link_index %d value = %u\n", port_index,link_index,
			fh_egress_udp_watermark_port_p);

	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_egress_udp_watermark_port_p_aligner_output_fifo =
		fh_egress_udp_watermark_port_p.aligner_output_fifo;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_egress_udp_watermark_port_p_cs_update_fifo =
		fh_egress_udp_watermark_port_p.cs_update_fifo;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_egress_udp_watermark_port_p_cs_calc_fifo =
		fh_egress_udp_watermark_port_p.cs_calc_fifo;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_egress_udp_watermark_port_p_hdri_output_fifo =
		fh_egress_udp_watermark_port_p.hdri_output_fifo;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_egress_udp_watermark_port_p_hdri_cfg_index_fifo =
		fh_egress_udp_watermark_port_p.hdri_cfg_index_fifo;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_egress_udp_watermark_port_p_pkt_fifo =
		fh_egress_udp_watermark_port_p.pkt_fifo;


	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_egress_output_fifo[curr_wm_index] =
		fh_egress_udp_watermark_port_p.aligner_output_fifo;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_egress_cs_update_fifo[curr_wm_index] =
		fh_egress_udp_watermark_port_p.cs_update_fifo;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_egress_cs_calc_fifo[curr_wm_index] =
		fh_egress_udp_watermark_port_p.cs_calc_fifo;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_egress_hdri_output_fifo[curr_wm_index] =
		fh_egress_udp_watermark_port_p.hdri_output_fifo;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_egress_hdri_cfg_index_fifo[curr_wm_index] =
		fh_egress_udp_watermark_port_p.hdri_cfg_index_fifo;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.fh_egress_pkt_fifo[curr_wm_index] =
		fh_egress_udp_watermark_port_p.pkt_fifo;

	return;
}
void ecpriss_fh_qudp_stats_update_usr(void)
{
	uint32_t port_index = 0;
	uint32_t link_index = 0;
	ecpriss_Qudp_Stats *stats  = NULL;

	for(port_index = 0; port_index < ECPRISS_PORT_MAX; port_index++){
		for(link_index =0; link_index < ECPRISS_MAX_LINKS ; link_index++){
			if(port_index == ECPRISS_PORT_0){
				if(link_index == 0){

					stats = &ecpriss_pdata_v2->fh_stats_usr.qudp.fh_0.link_0.stats;
				}else if(link_index == 1){

					stats = &ecpriss_pdata_v2->fh_stats_usr.qudp.fh_0.link_1.stats;
				}else if(link_index == 2){

					stats = &ecpriss_pdata_v2->fh_stats_usr.qudp.fh_0.link_2.stats;
				}else if(link_index == 3){

					stats = &ecpriss_pdata_v2->fh_stats_usr.qudp.fh_0.link_3.stats;
				}
			}else if(port_index == ECPRISS_PORT_1){
				if(link_index == 0){

					stats = &ecpriss_pdata_v2->fh_stats_usr.qudp.fh_1.link_0.stats;
				}else if(link_index == 1){

					stats = &ecpriss_pdata_v2->fh_stats_usr.qudp.fh_1.link_1.stats;
				}else if(link_index == 2){

					stats = &ecpriss_pdata_v2->fh_stats_usr.qudp.fh_1.link_2.stats;
				}else if(link_index == 3){

					stats = &ecpriss_pdata_v2->fh_stats_usr.qudp.fh_1.link_3.stats;
				}
			}else if(port_index == ECPRISS_PORT_2){
				if(link_index == 0){

					stats = &ecpriss_pdata_v2->fh_stats_usr.qudp.fh_2.link_0.stats;
				}else if(link_index == 1){

					stats = &ecpriss_pdata_v2->fh_stats_usr.qudp.fh_2.link_1.stats;
				}else if(link_index == 2){

					stats = &ecpriss_pdata_v2->fh_stats_usr.qudp.fh_2.link_2.stats;
				}else if(link_index == 3){

					stats = &ecpriss_pdata_v2->fh_stats_usr.qudp.fh_2.link_3.stats;
				}
			}
			stats->egress_num_udp_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.egress_num_udp_packets[link_index];
			stats->egress_num_eth_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.egress_num_eth_packets[link_index];
			stats->egress_num_bypassed_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.egress_num_bypassed_packets[link_index];
			stats->egress_num_mtu_err_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.egress_num_mtu_err_packets[link_index];
			stats->ingress_num_eth_udp_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_eth_udp_packets[link_index];
			stats->ingress_num_fcs_err_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_fcs_err_packets[link_index];
			stats->ingress_num_ipv4_cs_err_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_ipv4_cs_err_packets[link_index];
			stats->ingress_num_udp_cs_err_packets =	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_udp_cs_err_packets[link_index];
			stats->ingress_ip_filtered_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_ip_filtered_packets[link_index];
			stats->ingress_num_vlan_filtered_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_vlan_filtered_packets[link_index];
			stats->ingress_num_sec_err_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_sec_err_packets[link_index];
			stats->ingress_ip_len_err_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_ip_len_err_packets[link_index];
			stats->ingress_num_eth_ecpri_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_eth_ecpri_packets[link_index];
			stats->ingress_num_eth_ptp_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_eth_ptp_packets[link_index];
			stats->ingress_num_eth_other_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_eth_other_packets[link_index];
			stats->ingress_num_udp_ecpri_or_nfapi_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_udp_ecpri_or_nfapi_packets[link_index];
			stats->ingress_num_udp_ptp_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_udp_ptp_packets[link_index];
			stats->ingress_num_udp_other_packets = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_udp_other_packets[link_index];
		}
	}
	return;
}

void ecpriss_qudp_fh_ingress_stats_update_v2(uint32_t port_index, uint32_t link_index)
{
	uint32_t val = 0;
	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_NUM_ETH_UDP_PACKETS_PORT_p_LINK_n_V2,
			port_index,
			link_index);

	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_eth_udp_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_NUM_ETH_UDP_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_FCS_ERR_PACKETS_PORT_p_LINK_n_V2,
			port_index,
			link_index);

	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_fcs_err_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_FCS_ERR_PACKETS_PORT_p_LINK_n_V2: port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_IPV4_CS_ERROR_PACKETS_PORT_p_LINK_n_V2,
			port_index,
			link_index);
	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_ipv4_cs_err_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_IPV4_CS_ERROR_PACKETS_PORT_p_LINK_n_V2: port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_UDP_CS_ERROR_PACKETS_PORT_p_LINK_n_V2,
			port_index,
			link_index);
	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_udp_cs_err_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_UDP_CS_ERROR_PACKETS_PORT_p_LINK_n_V2: port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_IP_FILTERED_PACKETS_PORT_p_LINK_n_V2,
			port_index,
			link_index);

	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_ip_filtered_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_IP_FILTERED_PACKETS_PORT_p_LINK_n_V2: port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_VLAN_FILTERED_PACKETS_PORT_p_LINK_n_V2,
			port_index,
			link_index);
	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_vlan_filtered_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_VLAN_FILTERED_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_SEC_ERR_PACKETS_PORT_p_LINK_n_V2,
			port_index,
			link_index);

	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_sec_err_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_SEC_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_IP_LEN_ERR_PACKETS_PORT_p_LINK_n_V2,
			port_index,
			link_index);
	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_ip_len_err_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_IP_LEN_ERR_PACKETS_PORT_p_LINK_n_V2: port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_NUM_ETH_ECPRI_PACKETS_PORT_p_LINK_n_V2,port_index,link_index);


	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_eth_ecpri_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_NUM_ETH_ECPRI_PACKETS_PORT_p_LINK_n_V2: port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_NUM_ETH_PTP_PACKETS_PORT_p_LINK_n_V2,port_index,link_index);


	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_eth_ptp_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_NUM_ETH_PTP_PACKETS_PORT_p_LINK_n_V2: port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_NUM_ETH_OTHER_PACKETS_PORT_p_LINK_n_V2,port_index,link_index);


	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_eth_other_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_NUM_ETH_OTHER_PACKETS_PORT_p_LINK_n_V2: port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_NUM_UDP_ECPRI_OR_NFAPI_PACKETS_PORT_p_LINK_n_V2,port_index,link_index);


	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_udp_ecpri_or_nfapi_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_NUM_UDP_ECPRI_OR_NFAPI_PACKETS_PORT_p_LINK_n_V2: port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_NUM_UDP_PTP_PACKETS_PORT_p_LINK_n_V2,port_index,link_index);


	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_udp_ptp_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_NUM_UDP_PTP_PACKETS_PORT_p_LINK_n_V2: port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_FH_RAMS,
			ECPRI_UDP_FH_INGRESS_NUM_UDP_OTHER_PACKETS_PORT_p_LINK_n_V2,port_index,link_index);


	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].stats_v2.ingress_num_udp_other_packets[link_index] += val;
	ECPRILOGDBG("ECPRI_UDP_FH_INGRESS_NUM_UDP_OTHER_PACKETS_PORT_p_LINK_n_V2: port_index :%d link_index %d value = %d\n", port_index,link_index,val);



	return;
}

void ecpriss_qudp_print_c2c_egress_stats(uint32_t port_index, uint32_t link_index)
{
	uint32_t lsb_val = 0;
	uint64_t msb_val = 0;
	uint64_t val = 0;
	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_NUM_UDP_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_NUM_UDP_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.egress_num_udp_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_C2C_EGRESS_NUM_UDP_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val =ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_NUM_ETH_ONLY_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_NUM_ETH_ONLY_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.egress_num_eth_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_C2C_EGRESS_NUM_ETH_ONLY_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_NUM_BYPASSED_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_NUM_BYPASSED_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.egress_num_bypassed_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_C2C_EGRESS_NUM_BYPASSED_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_MTU_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val =ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_MTU_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.egress_num_mtu_err_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_C2C_EGRESS_MTU_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	return;
}


void ecpriss_qudp_print_c2c_ingress_stats(uint32_t port_index, uint32_t link_index)
{
	uint32_t lsb_val = 0;
	uint64_t msb_val = 0;
	uint64_t val = 0;
	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_NUM_UDP_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_NUM_UDP_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_num_udp_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_C2C_INGRESS_NUM_UDP_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_NUM_NON_UDP_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_NUM_NON_UDP_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_num_non_udp_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_C2C_INGRESS_NUM_NON_UDP_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_FCS_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_FCS_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_num_fcs_err_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_C2C_INGRESS_FCS_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_IPV4_CS_ERROR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_IPV4_CS_ERROR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_num_ipv4_cs_err_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_C2C_INGRESS_IPV4_CS_ERROR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_UDP_CS_ERROR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_UDP_CS_ERROR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_num_udp_cs_err_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_C2C_INGRESS_UDP_CS_ERROR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_VLAN_FILTERED_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_VLAN_FILTERED_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_num_vlan_filtered_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_C2C_INGRESS_VLAN_FILTERED_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_SEC_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_SEC_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);
	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_num_sec_err_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_C2C_INGRESS_SEC_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_IP_LEN_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_IP_LEN_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);
	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_ip_len_err_packets[link_index] = val;
	ECPRILOGDBG("ECPRI_UDP_C2C_INGRESS_IP_LEN_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	return;
}


void ecpriss_qudp_print_l2_egress_stats(uint32_t port_index, uint32_t link_index)
{
	uint32_t lsb_val = 0;
	uint64_t msb_val = 0;
	uint64_t val = 0;
	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_EGRESS_NUM_UDP_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_EGRESS_NUM_UDP_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.egress_num_udp_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_L2_EGRESS_NUM_UDP_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val =ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_EGRESS_NUM_ETH_ONLY_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_EGRESS_NUM_ETH_ONLY_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.egress_num_eth_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_L2_EGRESS_NUM_ETH_ONLY_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_EGRESS_NUM_BYPASSED_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_EGRESS_NUM_BYPASSED_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.egress_num_bypassed_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_L2_EGRESS_NUM_BYPASSED_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_EGRESS_MTU_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val =ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_EGRESS_MTU_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.egress_num_mtu_err_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_L2_EGRESS_MTU_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	return;
}


void ecpriss_qudp_print_l2_ingress_stats(uint32_t port_index, uint32_t link_index)
{
	uint32_t lsb_val = 0;
	uint64_t msb_val = 0;
	uint64_t val = 0;
	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_NUM_UDP_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_NUM_UDP_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_num_udp_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_L2_INGRESS_NUM_UDP_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_NUM_NON_UDP_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_NUM_NON_UDP_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_num_non_udp_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_L2_INGRESS_NUM_NON_UDP_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_FCS_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_FCS_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_num_fcs_err_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_L2_INGRESS_FCS_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_IPV4_CS_ERROR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_IPV4_CS_ERROR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_num_ipv4_cs_err_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_L2_INGRESS_IPV4_CS_ERROR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_UDP_CS_ERROR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_UDP_CS_ERROR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_num_udp_cs_err_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_L2_INGRESS_UDP_CS_ERROR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_VLAN_FILTERED_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_VLAN_FILTERED_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_num_vlan_filtered_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_L2_INGRESS_VLAN_FILTERED_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_SEC_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_SEC_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);
	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_num_sec_err_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_L2_INGRESS_SEC_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_IP_LEN_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_IP_LEN_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);
	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].stats.ingress_ip_len_err_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_L2_INGRESS_IP_LEN_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	return;
}

/**
 * ecpri_qudp_ingress_modify_cfg()
 *
 *
 * Returns:	0 on success, negative on failure
 */

static int ecpriss_qudp_ingress_modify_cfg(uint32_t port_index,
		uint32_t value,	ecpriss_ingress_filter_mask_e field , uint8_t filtnum,
		uint8_t cfg_action)
{
	ecpri_qudp_hwio_def_ecpri_udp_fh_ingress_config_p_s ingress_cfg;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr_port_p_entries_valid_bits_s ip_dst_valid_bit;
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_classification_list_port_p_entries_valid_bits_s udp_port;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_vlan_addr_port_p_entries_valid_bits_s vlan_id;
	ecpriss_qudp_ingress_per_port_cfg_s *qudp_ingress_port =
		&ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].ingress_port_cfg;

	memset(&ingress_cfg, 0, sizeof(ingress_cfg));

	ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_INGRESS_CONFIG_P,
			port_index,
			&ingress_cfg);

	if(field & ECPRISS_QUDP_RX_CFG_FLTR_MASK_IP_DADDR)
	{
		memset(&ip_dst_valid_bit ,0, sizeof(ip_dst_valid_bit));


		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_IP_DST_ADDR_PORT_p_ENTRIES_VALID_BITS,
				port_index, &ip_dst_valid_bit);


		if(ecpriss_filtering_enabled){
			if(qudp_ingress_port->num_ip_fltr_entries <= 0)
				ingress_cfg.enable_ip_dst_filt = 0;
			else
				ingress_cfg.enable_ip_dst_filt = 1;

			if(cfg_action == CONFIGURE)
				ip_dst_valid_bit.valid_bits |= 1UL << filtnum;
			else
				ip_dst_valid_bit.valid_bits &= ~(1UL << filtnum);
		}else{
			ingress_cfg.enable_ip_dst_filt = 0;
			ip_dst_valid_bit.valid_bits = 0;

		}


		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_IP_DST_ADDR_PORT_p_ENTRIES_VALID_BITS,
				port_index, &ip_dst_valid_bit);

	}

	if(field & ECPRISS_QUDP_RX_CFG_FLTR_MASK_UDP_DPORT)
	{
		memset(&udp_port ,0, sizeof(udp_port));

		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_CLASSIFICATION_LIST_PORT_p_ENTRIES_VALID_BITS,
				port_index, &udp_port);
		if(qudp_ingress_port->num_udp_fltr_entries <= 0)
			ingress_cfg.enable_udp_dst_class = 0;
		else
			ingress_cfg.enable_udp_dst_class = 1;

		if(cfg_action == CONFIGURE)
			udp_port.valid_bits  |= 1UL << filtnum;
		else
			udp_port.valid_bits  &= ~( 1UL << filtnum);

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_CLASSIFICATION_LIST_PORT_p_ENTRIES_VALID_BITS,
				port_index, &udp_port);
	}

	if(field & ECPRISS_QUDP_RX_CFG_FLTR_MASK_VLAN)
	{

		memset(&vlan_id ,0, sizeof(vlan_id));

		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_VLAN_ADDR_PORT_p_ENTRIES_VALID_BITS,
				port_index,
				&vlan_id);


		if(ecpriss_filtering_enabled){
			if(qudp_ingress_port->num_vlan_fltr_entries <= 0)
				ingress_cfg.enable_vlan_filt = 0;
			else
				ingress_cfg.enable_vlan_filt = 1;

			if(cfg_action == CONFIGURE)
				vlan_id.valid_bits  |= (1UL << filtnum);
			else
				vlan_id.valid_bits  &= ~(1UL << filtnum);

		}else{
			ingress_cfg.enable_vlan_filt = 0;
			vlan_id.valid_bits  = 0;
		}

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_VLAN_ADDR_PORT_p_ENTRIES_VALID_BITS,
				port_index,&vlan_id);


	}

	ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_INGRESS_CONFIG_P,
			port_index,
			&ingress_cfg);


	return 0;
}

static int ecpriss_qudp_ingress_modify_cfg_v2(uint32_t port_index,
		uint32_t value,	ecpriss_ingress_filter_mask_e field , uint8_t filtnum,
		uint8_t cfg_action)
{
	ecpri_qudp_hwio_def_ecpri_udp_fh_ingress_config_p_s_v2 ingress_cfg;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr_port_p_entries_valid_bits_s_v2 ip_dst_valid_bit;
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_classification_list_port_p_entries_valid_bits_s_v2 udp_port;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_vlan_addr_port_p_entries_valid_bits_s_v2 vlan_id;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_port_p_entries_valid_bits_s_v2 mac_valid_bit;

	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port =
		&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].ingress_port_cfg;

	memset(&ingress_cfg, 0, sizeof(ingress_cfg));

	ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_INGRESS_CONFIG_P_V2,
			port_index,
			&ingress_cfg);

	if(field & ECPRISS_QUDP_RX_CFG_FLTR_MASK_IP_DADDR)
	{
		memset(&ip_dst_valid_bit ,0, sizeof(ip_dst_valid_bit));


		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_IP_DST_ADDR_PORT_p_ENTRIES_VALID_BITS_V2,
				port_index, &ip_dst_valid_bit);


		if(ecpriss_filtering_enabled){
			if(qudp_ingress_port->num_ip_fltr_entries <= 0)
				ingress_cfg.enable_ip_dst_filt = 0;
			else
				ingress_cfg.enable_ip_dst_filt = 1;

			if(cfg_action == CONFIGURE)
				ip_dst_valid_bit.valid_bits |= 1UL << filtnum;
			else
				ip_dst_valid_bit.valid_bits &= ~(1UL << filtnum);
		}else{
			ingress_cfg.enable_ip_dst_filt = 0;
			ip_dst_valid_bit.valid_bits = 0;

		}


		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_IP_DST_ADDR_PORT_p_ENTRIES_VALID_BITS_V2,
				port_index, &ip_dst_valid_bit);

	}

	if(field & ECPRISS_QUDP_RX_CFG_FLTR_MASK_UDP_DPORT)
	{
		memset(&udp_port ,0, sizeof(udp_port));

		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_CLASSIFICATION_LIST_PORT_p_ENTRIES_VALID_BITS_V2,
				port_index, &udp_port);
		if(qudp_ingress_port->num_udp_fltr_entries <= 0)
			ingress_cfg.enable_udp_dst_class = 0;
		else
			ingress_cfg.enable_udp_dst_class = 1;

		if(cfg_action == CONFIGURE)
			udp_port.valid_bits  |= 1UL << filtnum;
		else
			udp_port.valid_bits  &= ~( 1UL << filtnum);

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_CLASSIFICATION_LIST_PORT_p_ENTRIES_VALID_BITS_V2,
				port_index, &udp_port);
	}

	if(field & ECPRISS_QUDP_RX_CFG_FLTR_MASK_VLAN)
	{

		memset(&vlan_id ,0, sizeof(vlan_id));

		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_VLAN_ADDR_PORT_p_ENTRIES_VALID_BITS_V2,
				port_index,
				&vlan_id);


		if(ecpriss_filtering_enabled){
			if(qudp_ingress_port->num_vlan_fltr_entries <= 0)
				ingress_cfg.enable_vlan_filt = 0;
			else
				ingress_cfg.enable_vlan_filt = 1;

			if(cfg_action == CONFIGURE)
				vlan_id.valid_bits  |= (1UL << filtnum);
			else
				vlan_id.valid_bits  &= ~(1UL << filtnum);

		}else{
			ingress_cfg.enable_vlan_filt = 0;
			vlan_id.valid_bits  = 0;
		}

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_VLAN_ADDR_PORT_p_ENTRIES_VALID_BITS_V2,
				port_index,&vlan_id);


	}

	if(field & ECPRISS_QUDP_RX_CFG_FLTR_MASK_LOCAL_MAC_ADDR)
	{
		ingress_cfg.enable_mac_dst_check = 1;
		ingress_cfg.enable_broadcast_check = 1;
		/* In cascade mode FH1/FH2 (port_index >= 1) must forward
		 * non-local-dst packets to the remote RU, not to A55.
		 */
		if (ru_cascade_mode && port_index >= 1)
			ingress_cfg.non_local_dst_action = ECPRISS_QUDP_ACTION_PASS_TO_REMOTE;
		else
			ingress_cfg.non_local_dst_action = ECPRISS_MAC_ACTION_PASS_TO_A55;

		memset(&mac_valid_bit,0, sizeof(mac_valid_bit));


		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_MAC_ADDRESS_PORT_p_ENTRIES_VALID_BITS_V2,
				port_index, &mac_valid_bit);


		if(cfg_action == CONFIGURE)
			mac_valid_bit.valid_bits |= 1UL << filtnum;
		else
			mac_valid_bit.valid_bits &= ~(1UL << filtnum);

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_FILT_MAC_ADDRESS_PORT_p_ENTRIES_VALID_BITS_V2,
				port_index, &mac_valid_bit);


	}


	ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_INGRESS_CONFIG_P_V2,
			port_index,
			&ingress_cfg);


	return 0;
}

/**
 * It will add trap rules at init
 * Rules are listed below
 * 1. Trap ecpri MSG-5 packets if
 *	a. packet with ether type 0xaefe [32 Bit only rule at index 0]
 *	b. packet with ether type vlan & 0xaefe [64 Bit only rule at index 1]
 */
static void ecpriss_qudp_add_trap_rules_for_msg5(void)
{
	uint32_t port_index  = 0;
	uint32_t cfg_value = 0;
	ecpri_qudp_hwio_def_ecpri_udp_fh_trap_misc_port_p_entry_n_s_v2 trap_misc_port_cfg;

	for(port_index=0; port_index < ECPRISS_PORT_MAX; port_index++){

		memset(&trap_misc_port_cfg,0,sizeof(ecpri_qudp_hwio_def_ecpri_udp_fh_trap_misc_port_p_entry_n_s_v2));

		/*
		 * Adding 32 bit rule config
		 */
		trap_misc_port_cfg.rule32_offset = 12; // src MAC + dst MAC
		trap_misc_port_cfg.action = 5; //5 - add timestamp and pass to A55
		trap_misc_port_cfg.enable = 1;

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_MISC_PORT_p_ENTRY_n_V2,
				port_index,
				0,
				&trap_misc_port_cfg);

		cfg_value = 0x0500feae; // ether type 0xaefe & ecpri protocol MSG type 5
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE32_VAL_PORT_p_ENTRY_n_V2,
				port_index,
				0,
				&cfg_value);

		cfg_value = 0xff00ffff; // MASK to validate only ethere type and MSG-ID
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE32_MASK_PORT_p_ENTRY_n_V2,
				port_index,
				0,
				&cfg_value);


		/*
		 * Adding 64 bit rule config
		 */

		memset(&trap_misc_port_cfg,0,sizeof(ecpri_qudp_hwio_def_ecpri_udp_fh_trap_misc_port_p_entry_n_s_v2));

		trap_misc_port_cfg.rule64_offset = 12; // src MAC + dst MAC
		trap_misc_port_cfg.action = 5; //5 - add timestamp and pass to A55
		trap_misc_port_cfg.enable = 1;

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_MISC_PORT_p_ENTRY_n_V2,
				port_index,
				1,
				&trap_misc_port_cfg);

		cfg_value = 0x0500feae; // ether type 0xaefe & ecpri protocol MSG type 5
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE64_VAL_MSB_PORT_p_ENTRY_n_V2,
				port_index,
				1,
				&cfg_value);

		cfg_value =  0x00000081; // ether type vlan
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE64_VAL_LSB_PORT_p_ENTRY_n_V2,
				port_index,
				1,
				&cfg_value);

		cfg_value = 0xff00ffff; // MASK to validate only ethere type and MSG-ID
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE64_MASK_MSB_PORT_p_ENTRY_n_V2,
				port_index,
				1,
				&cfg_value);

		cfg_value = 0x0000ffff; // MASK to validate only ethere type as vlan
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE64_MASK_LSB_PORT_p_ENTRY_n_V2,
				port_index,
				1,
				&cfg_value);


	}
	return;
}

/**
 * ecpri_qudp_ingress_init_cfg()
 *
 *
 * Returns:	0 on success, negative on failure
 */
static int ecpriss_qudp_ingress_init_cfg(void)
{
	int ret = 0;
	int port_type = 0;
	int port_idx = 0;

	ecpri_qudp_hwio_def_ecpri_udp_fh_debug_features_cfg_s udp_fh_debug_feature_cfg;
	ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_DEBUG_FEATURES_CFG,
				0,
				&udp_fh_debug_feature_cfg);

	 udp_fh_debug_feature_cfg.watermark_en = 1;
	 udp_fh_debug_feature_cfg.en_clear_watermark_on_read = 1;

 	 ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_DEBUG_FEATURES_CFG,
				0,
				&udp_fh_debug_feature_cfg);

	for(port_type=0;port_type<ECPRISS_PORT_TYPE_MAX;port_type++)
	{
		if(port_type == ECPRISS_PORT_TYPE_FH)
		{

			for(port_idx=0;port_idx<ecpriss_pdata->qudp_ctx->num_ports;port_idx++)
			{

				ecpriss_qudp_ingress_per_port_cfg_s       *ingress_cfg =
					&ecpriss_pdata->qudp_ctx->fh_port_cfg[port_idx].ingress_port_cfg;

				ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH ,
						ECPRI_UDP_FH_INGRESS_CONFIG_P,
						port_idx,
						&ingress_cfg->fh_ingress_config);

				ingress_cfg->fh_ingress_config.ipv4_cs_err_action = 1;
				ingress_cfg->fh_ingress_config.udp_cs_err_action = 1;
				ingress_cfg->fh_ingress_config.fcs_err_action = 1;
				ingress_cfg->fh_ingress_config.pkt_err_action = 1;
				ingress_cfg->fh_ingress_config.ip_len_err_action = 1;
				ingress_cfg->fh_ingress_config.vlan_filt_miss_action = 1;
				ingress_cfg->fh_ingress_config.ip_filt_miss_action = 1;
				/* FH1 and FH2 are cascade ports: non-local dst packets must
				 * be forwarded to the remote RU, not sent to A55.
				 */
				if (ru_cascade_mode && port_idx >= 1)
					ingress_cfg->fh_ingress_config.non_local_dst_action =
							ECPRISS_QUDP_ACTION_PASS_TO_REMOTE;
				else
					ingress_cfg->fh_ingress_config.non_local_dst_action = 1;
				ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
						ECPRI_UDP_FH_INGRESS_CONFIG_P,
						port_idx,
						&ingress_cfg->fh_ingress_config);
				memset(&(ingress_cfg->fh_ingress_config),
						0,
						sizeof(ecpri_qudp_hwio_def_ecpri_udp_fh_ingress_config_p_s));

				ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH ,
						ECPRI_UDP_FH_INGRESS_CONFIG_P,
						port_idx,
						&ingress_cfg->fh_ingress_config);
			}
		}
#if 0
		else if(port_type == ECPRISS_PORT_TYPE_L2)
		{
			for(port_idx=0;port_idx<ecpriss_pdata->qudp_ctx->num_ports;port_idx++)
			{
				ecpriss_qudp_ingress_per_port_cfg_s      *ingress_cfg =
					&(ecpriss_pdata->qudp_ctx->l2_port_cfg[port_idx].ingress_port_cfg);
				memset(&(ingress_cfg->l2_ingress_config),
						0,
						sizeof(ecpri_qudp_hwio_def_ecpri_udp_l2_ingress_config_p_s));
				ingress_cfg->l2_ingress_config.enable_ip_dst_filt = ECPRISS_QUDP_REG_FIELD_ENABLE;
				ingress_cfg->l2_ingress_config.enable_udp_dst_class = ECPRISS_QUDP_REG_FIELD_ENABLE;
				ingress_cfg->l2_ingress_config.enable_vlan_filt = ECPRISS_QUDP_REG_FIELD_ENABLE;
				ingress_cfg->l2_ingress_config.enable_udp_cs_check = ECPRISS_QUDP_REG_FIELD_ENABLE;
				ingress_cfg->l2_ingress_config.enable_ip_len_check = ECPRISS_QUDP_REG_FIELD_ENABLE;
				ingress_cfg->l2_ingress_config.ip_filt_miss_action = ECPRISS_QUDP_REG_FIELD_ENABLE;
				ingress_cfg->l2_ingress_config.vlan_filt_miss_action = ECPRISS_QUDP_REG_FIELD_ENABLE;
				ingress_cfg->l2_ingress_config.ip_len_err_action = ECPRISS_QUDP_REG_FIELD_ENABLE;
				ingress_cfg->l2_ingress_config.pkt_err_action = ECPRISS_QUDP_REG_FIELD_ENABLE;
				ingress_cfg->l2_ingress_config.disable_st_and_fw = ECPRISS_QUDP_REG_FIELD_ENABLE;
				ingress_cfg->l2_ingress_config.use_external_not_local_mac_dst = ECPRISS_QUDP_REG_FIELD_ENABLE;
				ingress_cfg->l2_ingress_config.last_in_chain = ECPRISS_QUDP_REG_FIELD_ENABLE;
				//ingress_cfg->l2_ingress_config.enable_shared_filtering_2_links = ECPRISS_QUDP_REG_FIELD_ENABLE;
				//ingress_cfg->l2_ingress_config.enable_shared_filtering_4_links = ECPRISS_QUDP_REG_FIELD_ENABLE;
				ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
						ECPRI_UDP_L2_INGRESS_CONFIG_P,
						port_idx,
						&ingress_cfg->l2_ingress_config);
			}
		}
#endif
	}
	return ret;
}

static void ecpriss_qudp_strict_filter_cfg_v2(void)
{
	int strict_filter_config = 0;
	int port_idx = 0;

	for(port_idx = 0;(port_idx < ecpriss_pdata_v2->qudp_ctx_v2->num_ports[ETH_ECPRISS_PORT_TYPE_FH]) && (port_idx < ECPRISS_PORT_MAX);port_idx++) {

		strict_filter_config = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_idx].strict_filter_status;
		if(strict_filter_config) {
			ecpriss_qudp_set_strict_filter_config(strict_filter_config, port_idx);
		}

	}

}


static int ecpriss_qudp_ingress_init_cfg_v2(void)
{
	int ret = 0;
	int port_type = 0;
	int port_idx = 0;

	for(port_type=0;port_type<ECPRISS_PORT_TYPE_MAX;port_type++)
	{
		if(port_type == ECPRISS_PORT_TYPE_FH)
		{

			for(port_idx=0;port_idx<ecpriss_pdata_v2->qudp_ctx_v2->num_ports[ETH_ECPRISS_PORT_TYPE_FH];port_idx++)
			{

				ecpriss_qudp_ingress_per_port_cfg_s_v2       *ingress_cfg =
					&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_idx].ingress_port_cfg;

				ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH ,
						ECPRI_UDP_FH_INGRESS_CONFIG_P_V2,
						port_idx,
						&ingress_cfg->fh_ingress_config);

				ingress_cfg->fh_ingress_config.ipv4_cs_err_action = ECPRISS_QUDP_ACTION_DISCARD;
				ingress_cfg->fh_ingress_config.udp_cs_err_action = ECPRISS_QUDP_ACTION_DISCARD;
				ingress_cfg->fh_ingress_config.fcs_err_action = ECPRISS_QUDP_ACTION_DISCARD;
				ingress_cfg->fh_ingress_config.pkt_err_action = ECPRISS_QUDP_ACTION_DISCARD;
				ingress_cfg->fh_ingress_config.ip_len_err_action = ECPRISS_QUDP_ACTION_DISCARD;
				ingress_cfg->fh_ingress_config.vlan_filt_miss_action = ECPRISS_QUDP_ACTION_PASS_TO_A55;
				ingress_cfg->fh_ingress_config.ip_filt_miss_action = ECPRISS_QUDP_ACTION_PASS_TO_A55;
				/* FH1 and FH2 are cascade ports: non-local dst packets must
				 * be forwarded to the remote RU, not discarded.
				 */
				if (ru_cascade_mode && port_idx >= 1)
					ingress_cfg->fh_ingress_config.non_local_dst_action =
							ECPRISS_QUDP_ACTION_PASS_TO_REMOTE;
				else
					ingress_cfg->fh_ingress_config.non_local_dst_action = ECPRISS_QUDP_ACTION_DISCARD;


				ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
						ECPRI_UDP_FH_INGRESS_CONFIG_P_V2,
						port_idx,
						&ingress_cfg->fh_ingress_config);
				memset(&(ingress_cfg->fh_ingress_config),
						0,
						sizeof(ecpri_qudp_hwio_def_ecpri_udp_fh_ingress_config_p_s_v2));

				ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH ,
						ECPRI_UDP_FH_INGRESS_CONFIG_P_V2,
						port_idx,
						&ingress_cfg->fh_ingress_config);

			}
		}
	}
	ecpriss_qudp_add_trap_rules_for_msg5();
	return ret;
}

int ecpriss_qudp_ingress_init_cfg_modify_v2(int action)
{
	int ret = 0;
	int port_type = 0;
	int port_idx = 0;



	for(port_type=0;port_type<ECPRISS_PORT_TYPE_MAX;port_type++)
	{
		if(port_type == ECPRISS_PORT_TYPE_FH)
		{

			for(port_idx=0;port_idx<ecpriss_pdata_v2->qudp_ctx_v2->num_ports[ETH_ECPRISS_PORT_TYPE_FH];port_idx++)
			{

				ecpriss_qudp_ingress_per_port_cfg_s_v2       *ingress_cfg =
					&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_idx].ingress_port_cfg;

				ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH ,
						ECPRI_UDP_FH_INGRESS_CONFIG_P_V2,
						port_idx,
						&ingress_cfg->fh_ingress_config);

				ingress_cfg->fh_ingress_config.ipv4_cs_err_action = action;
				ingress_cfg->fh_ingress_config.udp_cs_err_action = action;
				ingress_cfg->fh_ingress_config.fcs_err_action = action;
				ingress_cfg->fh_ingress_config.pkt_err_action = action;
				ingress_cfg->fh_ingress_config.ip_len_err_action = action;
				ingress_cfg->fh_ingress_config.vlan_filt_miss_action = action;
				ingress_cfg->fh_ingress_config.ip_filt_miss_action = action;
				/* In cascade mode, FH1 and FH2 (port_idx >= 1) must keep
				 * non_local_dst_action = PASS_TO_REMOTE so that packets
				 * with a non-local MAC destination are forwarded to the
				 * remote RU instead of being sent to A55 or discarded.
				 * FH0 (port_idx == 0) is not a cascade port and uses
				 * the caller-supplied action unchanged.
				 */
				if (ru_cascade_mode && port_idx >= 1)
					ingress_cfg->fh_ingress_config.non_local_dst_action =
							ECPRISS_QUDP_ACTION_PASS_TO_REMOTE;
				else
					ingress_cfg->fh_ingress_config.non_local_dst_action = action;
				ECPRILOGINFO("ecpriss_qudp_ingress_init_cfg_modify_v2: port_idx=%d action=%d non_local_dst_action=%d\n",
						port_idx, action,
						ingress_cfg->fh_ingress_config.non_local_dst_action);

				ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
						ECPRI_UDP_FH_INGRESS_CONFIG_P_V2,
						port_idx,
						&ingress_cfg->fh_ingress_config);


			}
		}
	}
	return ret;
}

static int ecpriss_qudp_egress_init_cfg_v2(void)
{
	int ret = 0;
	int port_type = 0;
	int port_idx = 0;

	for(port_type=0;port_type<ECPRISS_PORT_TYPE_MAX;port_type++)
	{
		if(port_type == ECPRISS_PORT_TYPE_FH)
		{

			for(port_idx=0;port_idx<ecpriss_pdata_v2->qudp_ctx_v2->num_ports[ETH_ECPRISS_PORT_TYPE_FH];port_idx++)
			{

				ecpriss_qudp_egress_per_port_cfg_s_v2 *egress_cfg =
					&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_idx].egress_cfg;

					ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH ,
						ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
						port_idx,
						&egress_cfg->fh_egress_config);

				egress_cfg->fh_egress_config.calc_ip_udp_len_from_byte_count = 0;
				egress_cfg->fh_egress_config.bypassed_packets_vport_action = 0;
				egress_cfg->fh_egress_config.bypassed_packets_vport = 0;
				egress_cfg->fh_egress_config.disable_padding_removal = 0;
				egress_cfg->fh_egress_config.l2_encap_index_override_en = 0;
				egress_cfg->fh_egress_config.l3_encap_index_override_en = 0;
				egress_cfg->fh_egress_config.disable_ptp_detection = 0;

				ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
						ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
						port_idx,
						&egress_cfg->fh_egress_config);

				ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
						ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
						port_idx,
						&egress_cfg->fh_egress_config);

			}
		}
		else if(port_type == ECPRISS_PORT_TYPE_L2)
		{

			for(port_idx=0;port_idx<ecpriss_pdata_v2->qudp_ctx_v2->num_ports[ETH_ECPRISS_PORT_TYPE_FH];port_idx++)
			{

				ecpriss_qudp_egress_per_port_cfg_s_v2 *egress_cfg =
					&ecpriss_pdata_v2->qudp_ctx_v2->l2_port_cfg_v2[port_idx].egress_cfg;

					ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_L2 ,
						ECPRI_UDP_L2_EGRESS_CONFIG_P_V2,
						port_idx,
						&egress_cfg->fh_egress_config);

				egress_cfg->fh_egress_config.calc_ip_udp_len_from_byte_count = 0;
				egress_cfg->fh_egress_config.bypassed_packets_vport_action = 0;
				egress_cfg->fh_egress_config.bypassed_packets_vport = 0;
				egress_cfg->fh_egress_config.disable_padding_removal = 0;
				egress_cfg->fh_egress_config.l2_encap_index_override_en = 0;
				egress_cfg->fh_egress_config.l3_encap_index_override_en = 0;
				egress_cfg->fh_egress_config.disable_ptp_detection = 0;

				ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_L2,
						ECPRI_UDP_L2_EGRESS_CONFIG_P_V2,
						port_idx,
						&egress_cfg->fh_egress_config);

				ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_L2,
						ECPRI_UDP_L2_EGRESS_CONFIG_P_V2,
						port_idx,
						&egress_cfg->fh_egress_config);

			}
		}
	}
	return ret;
}


/**
 *  ecpriss_qudp_rx_filter()
 *
 *
 * Returns:	0 on success, negative on failure
 */
static void ecpriss_qudp_configure_mtu(void)
{
	int port_type=0;
	int port_idx=0;
	ecpriss_qudp_egress_per_port_cfg_s  *egress_cfg;
	eth_ecpriss_port_params_s   *fh_eth_cfg ;
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_mtu_p_s fh_egress_eth_mtu_p;
	for(port_type=0;port_type<ECPRISS_PORT_TYPE_MAX;port_type++)
	{
		if(port_type == ECPRISS_PORT_TYPE_FH)
		{
			for(port_idx=0;port_idx<ecpriss_pdata->qudp_ctx->num_ports;port_idx++)
			{


				egress_cfg = &(ecpriss_pdata->qudp_ctx->fh_port_cfg[port_idx].egress_cfg);

				fh_eth_cfg = &(ecpriss_pdata->qudp_ctx->fh_port_cfg[port_idx].eth_cfg);

				memset(&fh_egress_eth_mtu_p ,0, sizeof(fh_egress_eth_mtu_p));

				ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
						ECPRI_UDP_FH_EGRESS_ETH_MTU_P,
						port_idx,
						&fh_egress_eth_mtu_p);


				fh_egress_eth_mtu_p.value = ECPRISS_ETH_QUDP_MTU_SIZE_V4;
				ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
						ECPRI_UDP_FH_EGRESS_ETH_MTU_P,
						port_idx,
						&fh_egress_eth_mtu_p);

			}
		}
		else if(port_type == ECPRISS_PORT_TYPE_C2C)
		{

#if 0
			for(port_idx=0;port_idx<ecpriss_pdata->qudp_ctx->num_ports;port_idx++)
			{
				ecpriss_qudp_egress_per_port_cfg_s       *egress_cfg =
					&(ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_idx].egress_cfg);

				eth_ecpriss_port_params_s   *c2c_eth_cfg =
					&(ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_idx].eth_cfg);

				ecpri_qudp_hwio_def_ecpri_udp_c2c_egress_eth_mtu_p_s c2c_egress_eth_mtu_p;

				/*Check if MTU is per link or per port*/
				egress_cfg->egress_eth_mtu.value = c2c_eth_cfg->port_mtu;
				c2c_egress_eth_mtu_p.value = c2c_eth_cfg->port_mtu;
				//egress_cfg->egress_eth_mtu.value = ECPRISS_ETH_MTU_SIZE;
				ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_C2C,
						ECPRI_UDP_C2C_EGRESS_ETH_MTU_P,
						port_idx,
						&c2c_egress_eth_mtu_p);
			}
#endif
		}
		else if (port_type == ECPRISS_PORT_TYPE_L2)
		{
#if 0
			for(port_idx=0;port_idx<ecpriss_pdata->qudp_ctx->num_ports;port_idx++)
			{
				ecpriss_qudp_egress_per_port_cfg_s       *egress_cfg =
					&(ecpriss_pdata->qudp_ctx->l2_port_cfg[port_idx].egress_cfg);

				eth_ecpriss_port_params_s   *l2_eth_cfg =
					&(ecpriss_pdata->qudp_ctx->l2_port_cfg[port_idx].eth_cfg);

				ecpri_qudp_hwio_def_ecpri_udp_l2_egress_eth_mtu_p_s l2_egress_eth_mtu_p;
				/*Check if MTU is per link or per port*/
				egress_cfg->egress_eth_mtu.value = l2_eth_cfg->port_mtu;
				l2_egress_eth_mtu_p.value = l2_eth_cfg->port_mtu;
				//egress_cfg->egress_eth_mtu.value = ECPRISS_ETH_MTU_SIZE;
				ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_L2,
						ECPRI_UDP_L2_EGRESS_ETH_MTU_P,
						port_idx,
						&l2_egress_eth_mtu_p);
			}
#endif

		}
	}
	return;
}
static void ecpriss_qudp_configure_mtu_v2(void)
{
	int port_type=0;
	int port_idx=0;
	ecpriss_qudp_egress_per_port_cfg_s_v2  *egress_cfg;
	eth_ecpriss_port_params_s   *fh_eth_cfg ;
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_mtu_p_s_v2 fh_egress_eth_mtu_p;

	for(port_type=0;port_type<ECPRISS_PORT_TYPE_MAX;port_type++)
	{
		if(port_type == ECPRISS_PORT_TYPE_FH)
		{
			for(port_idx=0;port_idx<ecpriss_pdata_v2->qudp_ctx_v2->num_ports[ETH_ECPRISS_PORT_TYPE_FH];port_idx++)
			{


				egress_cfg = &(ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_idx].egress_cfg);

				fh_eth_cfg = &(ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_idx].eth_cfg);

				memset(&fh_egress_eth_mtu_p ,0, sizeof(fh_egress_eth_mtu_p));

				ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
						ECPRI_UDP_FH_EGRESS_ETH_MTU_P_V2,
						port_idx,
						&fh_egress_eth_mtu_p);


				fh_egress_eth_mtu_p.value = ECPRISS_ETH_QUDP_MTU_SIZE_V4;
				ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
						ECPRI_UDP_FH_EGRESS_ETH_MTU_P_V2,
						port_idx,
						&fh_egress_eth_mtu_p);

			}
		}
		else if(port_type == ECPRISS_PORT_TYPE_C2C)
		{

		}
		else if (port_type == ECPRISS_PORT_TYPE_L2)
		{

		}
	}
	return;
}



static irqreturn_t ecpriss_qudp_isr(int irq, void *ctxt)
{
	unsigned long flags = 0;
	int port_index;
	//Todo: check with respect to spinlock_irqsave and spinlock_irqrestore
	spin_lock_irqsave(&ecpriss_pdata->irq_lock, flags);

	for(port_index=0;port_index<ECPRISS_PORT_MAX;port_index++) {
		ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_status_0_port_p_s fh_udp_sw_irq_status_0_port_p;
		ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_status_1_port_p_s fh_udp_sw_irq_status_1_port_p;

		ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_clr_0_port_p_s fh_udp_sw_irq_clr_0_port_p;
		ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_clr_1_port_p_s fh_udp_sw_irq_clr_1_port_p;
		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_STATUS_0_PORT_P,
				port_index,
				&fh_udp_sw_irq_status_0_port_p);

		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_STATUS_1_PORT_P,
				port_index,
				&fh_udp_sw_irq_status_1_port_p);

		/* Increment stats
		link 0 */
		if(fh_udp_sw_irq_status_0_port_p.egress_mtu_err_packet_link_0) {
			fh_udp_sw_irq_clr_0_port_p.egress_mtu_err_packet_link_0 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.egress_mtu_err_packet_link[0]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_fcs_err_packet_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_fcs_err_packet_link_0 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_fcs_err_packet_link[0]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_pkt_fifo_empty_before_eop_link_0) {
			fh_udp_sw_irq_status_0_port_p.ingress_pkt_fifo_empty_before_eop_link_0 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_pkt_fifo_empty_before_eop_link[0]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_ipv4_cs_error_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_ipv4_cs_error_link_0 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_ipv4_cs_error_link[0]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_ip_filtered_packet_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_ip_filtered_packet_link_0 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_ip_filtered_packet_link[0]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_vlan_filtered_packet_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_vlan_filtered_packet_link_0 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_vlan_filtered_packet_link[0]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_sec_err_packet_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_sec_err_packet_link_0 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_sec_err_packet_link[0]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_ip_len_err_packet_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_ip_len_err_packet_link_0 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_ip_len_err_packet_link[0]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_0_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_0_link_0 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_0_link[0]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_1_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_1_link_0 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_1_link[0]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_2_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_2_link_0 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_2_link[0]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_3_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_3_link_0 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_3_link[0]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_last_in_chain_non_local_dst_packet_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_last_in_chain_non_local_dst_packet_link_0 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_last_in_chain_non_local_dst_packet_link[0]++;
		}


		/* link1 */

		if(fh_udp_sw_irq_status_0_port_p.egress_mtu_err_packet_link_1) {
			fh_udp_sw_irq_clr_0_port_p.egress_mtu_err_packet_link_1 =1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.egress_mtu_err_packet_link[1]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_fcs_err_packet_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_fcs_err_packet_link_1 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_fcs_err_packet_link[1]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_pkt_fifo_empty_before_eop_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_pkt_fifo_empty_before_eop_link_1 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_pkt_fifo_empty_before_eop_link[1]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_ipv4_cs_error_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_ipv4_cs_error_link_1 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_ipv4_cs_error_link[1]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_ip_filtered_packet_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_ip_filtered_packet_link_1 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_ip_filtered_packet_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_vlan_filtered_packet_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_vlan_filtered_packet_link_1 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_vlan_filtered_packet_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_sec_err_packet_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_sec_err_packet_link_1 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_sec_err_packet_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_ip_len_err_packet_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_ip_len_err_packet_link_1 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_ip_len_err_packet_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_0_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_0_link_1 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_0_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_1_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_1_link_1 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_1_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_2_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_2_link_1 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_2_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_3_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_3_link_1 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_3_link[1]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_last_in_chain_non_local_dst_packet_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_last_in_chain_non_local_dst_packet_link_1 = 1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_last_in_chain_non_local_dst_packet_link[1]++;
		}


		/* link 2 */

		if(fh_udp_sw_irq_status_1_port_p.egress_mtu_err_packet_link_2) {
			fh_udp_sw_irq_status_1_port_p.egress_mtu_err_packet_link_2=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.egress_mtu_err_packet_link[2]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_fcs_err_packet_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_fcs_err_packet_link_2=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_fcs_err_packet_link[2]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_pkt_fifo_empty_before_eop_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_pkt_fifo_empty_before_eop_link_2=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_pkt_fifo_empty_before_eop_link[2]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_ipv4_cs_error_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_ipv4_cs_error_link_2=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_ipv4_cs_error_link[2]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_ip_filtered_packet_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_ip_filtered_packet_link_2=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_ip_filtered_packet_link[2]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_vlan_filtered_packet_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_vlan_filtered_packet_link_2=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_vlan_filtered_packet_link[2]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_sec_err_packet_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_sec_err_packet_link_2=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_sec_err_packet_link[2]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_ip_len_err_packet_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_ip_len_err_packet_link_2=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_ip_len_err_packet_link[2]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_0_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_0_link_2=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_0_link[2]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_1_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_1_link_2=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_1_link[2]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_2_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_2_link_2=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_2_link[2]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_3_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_3_link_2=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_3_link[2]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_last_in_chain_non_local_dst_packet_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_last_in_chain_non_local_dst_packet_link_2=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_last_in_chain_non_local_dst_packet_link[2]++;
		}

		/* link 3 */
		if(fh_udp_sw_irq_status_1_port_p.egress_mtu_err_packet_link_3) {
			fh_udp_sw_irq_clr_1_port_p.egress_mtu_err_packet_link_3=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.egress_mtu_err_packet_link[3]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_fcs_err_packet_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_fcs_err_packet_link_3=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_fcs_err_packet_link[3]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_pkt_fifo_empty_before_eop_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_pkt_fifo_empty_before_eop_link_3=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_pkt_fifo_empty_before_eop_link[3]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_ipv4_cs_error_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_ipv4_cs_error_link_3=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_ipv4_cs_error_link[3]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_ip_filtered_packet_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_ip_filtered_packet_link_3=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_ip_filtered_packet_link[3]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_vlan_filtered_packet_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_vlan_filtered_packet_link_3=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_vlan_filtered_packet_link[3]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_sec_err_packet_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_sec_err_packet_link_3=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_sec_err_packet_link[3]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_ip_len_err_packet_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_ip_len_err_packet_link_3=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_ip_len_err_packet_link[3]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_0_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_0_link_3=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_0_link[3]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_1_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_1_link_3=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_1_link[3]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_2_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_2_link_3=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_2_link[3]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_3_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_3_link_3=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_trap_rule_3_link[3]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_last_in_chain_non_local_dst_packet_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_last_in_chain_non_local_dst_packet_link_3=1;
			ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].interrupt_stats.ingress_last_in_chain_non_local_dst_packet_link[3]++;
		}

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_CLR_0_PORT_P,
				port_index,
				&fh_udp_sw_irq_clr_0_port_p);

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_CLR_1_PORT_P,
				port_index,
				&fh_udp_sw_irq_clr_1_port_p);

	}

	spin_unlock_irqrestore(&ecpriss_pdata->irq_lock, flags);
	return IRQ_HANDLED;
}
static irqreturn_t ecpriss_qudp_isr_v2(int irq, void *ctxt)
{
	unsigned long flags = 0;
	int port_index;
	//Todo: check with respect to spinlock_irqsave and spinlock_irqrestore
	spin_lock_irqsave(&ecpriss_pdata_v2->irq_lock, flags);


	for(port_index=0;port_index<ECPRISS_PORT_MAX;port_index++) {

		ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_status_0_port_p_s_v2 fh_udp_sw_irq_status_0_port_p;
		ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_status_1_port_p_s_v2 fh_udp_sw_irq_status_1_port_p;

		ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_clr_0_port_p_s_v2 fh_udp_sw_irq_clr_0_port_p;
		ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_clr_1_port_p_s_v2 fh_udp_sw_irq_clr_1_port_p;

		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_STATUS_0_PORT_P_V2,
				port_index,
				&fh_udp_sw_irq_status_0_port_p);

		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_STATUS_1_PORT_P_V2,
				port_index,
				&fh_udp_sw_irq_status_1_port_p);

		memcpy(&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_cfg_v2.fh_udp_sw_irq_status_0_port_p,&fh_udp_sw_irq_status_0_port_p,sizeof(fh_udp_sw_irq_status_0_port_p));
		memcpy(&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_cfg_v2.fh_udp_sw_irq_status_1_port_p,&fh_udp_sw_irq_status_1_port_p,sizeof(fh_udp_sw_irq_status_1_port_p));

		if(fh_udp_sw_irq_status_0_port_p.ingress_vlan_filtered_packet_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_vlan_filtered_packet_link_0 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_vlan_filtered_packet_link[0]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_sec_err_packet_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_sec_err_packet_link_0 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_sec_err_packet_link[0]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_ip_len_err_packet_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_ip_len_err_packet_link_0 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_ip_len_err_packet_link[0]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_0_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_0_link_0 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_0_link[0]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_1_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_1_link_0 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_1_link[0]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_2_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_2_link_0 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_2_link[0]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_3_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_3_link_0 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_3_link[0]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_last_in_chain_non_local_dst_packet_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_last_in_chain_non_local_dst_packet_link_0 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_last_in_chain_non_local_dst_packet_link[0]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_timestamped_packets_bw_too_high_link_0) {
			fh_udp_sw_irq_clr_0_port_p.ingress_timestamped_packets_bw_too_high_link_0 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_timestamped_packets_bw_too_high_link[0]++;
		}



		/* link1 */

		if(fh_udp_sw_irq_status_0_port_p.egress_mtu_err_packet_link_1) {
			fh_udp_sw_irq_clr_0_port_p.egress_mtu_err_packet_link_1 =1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.egress_mtu_err_packet_link[1]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_fcs_err_packet_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_fcs_err_packet_link_1 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_fcs_err_packet_link[1]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_pkt_fifo_empty_before_eop_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_pkt_fifo_empty_before_eop_link_1 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_pkt_fifo_empty_before_eop_link[1]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_ipv4_cs_error_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_ipv4_cs_error_link_1 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_ipv4_cs_error_link[1]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_ip_filtered_packet_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_ip_filtered_packet_link_1 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_ip_filtered_packet_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_vlan_filtered_packet_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_vlan_filtered_packet_link_1 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_vlan_filtered_packet_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_sec_err_packet_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_sec_err_packet_link_1 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_sec_err_packet_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_ip_len_err_packet_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_ip_len_err_packet_link_1 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_ip_len_err_packet_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_0_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_0_link_1 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_0_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_1_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_1_link_1 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_1_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_2_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_2_link_1 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_2_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_trap_rule_3_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_trap_rule_3_link_1 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_3_link[1]++;
		}

		if(fh_udp_sw_irq_status_0_port_p.ingress_last_in_chain_non_local_dst_packet_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_last_in_chain_non_local_dst_packet_link_1 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_last_in_chain_non_local_dst_packet_link[1]++;
		}


		if(fh_udp_sw_irq_status_0_port_p.ingress_timestamped_packets_bw_too_high_link_1) {
			fh_udp_sw_irq_clr_0_port_p.ingress_timestamped_packets_bw_too_high_link_1 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_timestamped_packets_bw_too_high_link[1]++;
		}



		/* link 2 */

		if(fh_udp_sw_irq_status_1_port_p.egress_mtu_err_packet_link_2) {
			fh_udp_sw_irq_status_1_port_p.egress_mtu_err_packet_link_2=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.egress_mtu_err_packet_link[2]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_fcs_err_packet_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_fcs_err_packet_link_2=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_fcs_err_packet_link[2]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_pkt_fifo_empty_before_eop_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_pkt_fifo_empty_before_eop_link_2=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_pkt_fifo_empty_before_eop_link[2]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_ipv4_cs_error_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_ipv4_cs_error_link_2=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_ipv4_cs_error_link[2]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_ip_filtered_packet_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_ip_filtered_packet_link_2=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_ip_filtered_packet_link[2]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_vlan_filtered_packet_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_vlan_filtered_packet_link_2=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_vlan_filtered_packet_link[2]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_sec_err_packet_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_sec_err_packet_link_2=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_sec_err_packet_link[2]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_ip_len_err_packet_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_ip_len_err_packet_link_2=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_ip_len_err_packet_link[2]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_0_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_0_link_2=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_0_link[2]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_1_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_1_link_2=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_1_link[2]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_2_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_2_link_2=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_2_link[2]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_3_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_3_link_2=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_3_link[2]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_last_in_chain_non_local_dst_packet_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_last_in_chain_non_local_dst_packet_link_2=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_last_in_chain_non_local_dst_packet_link[2]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_timestamped_packets_bw_too_high_link_2) {
			fh_udp_sw_irq_clr_1_port_p.ingress_timestamped_packets_bw_too_high_link_2 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_timestamped_packets_bw_too_high_link[2]++;
		}



		/* link 3 */
		if(fh_udp_sw_irq_status_1_port_p.egress_mtu_err_packet_link_3) {
			fh_udp_sw_irq_clr_1_port_p.egress_mtu_err_packet_link_3=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.egress_mtu_err_packet_link[3]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_fcs_err_packet_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_fcs_err_packet_link_3=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_fcs_err_packet_link[3]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_pkt_fifo_empty_before_eop_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_pkt_fifo_empty_before_eop_link_3=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_pkt_fifo_empty_before_eop_link[3]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_ipv4_cs_error_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_ipv4_cs_error_link_3=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_ipv4_cs_error_link[3]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_ip_filtered_packet_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_ip_filtered_packet_link_3=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_ip_filtered_packet_link[3]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_vlan_filtered_packet_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_vlan_filtered_packet_link_3=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_vlan_filtered_packet_link[3]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_sec_err_packet_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_sec_err_packet_link_3=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_sec_err_packet_link[3]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_ip_len_err_packet_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_ip_len_err_packet_link_3=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_ip_len_err_packet_link[3]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_0_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_0_link_3=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_0_link[3]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_1_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_1_link_3=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_1_link[3]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_2_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_2_link_3=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_2_link[3]++;
		}


		if(fh_udp_sw_irq_status_1_port_p.ingress_trap_rule_3_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_trap_rule_3_link_3=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_trap_rule_3_link[3]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_last_in_chain_non_local_dst_packet_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_last_in_chain_non_local_dst_packet_link_3=1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_last_in_chain_non_local_dst_packet_link[3]++;
		}

		if(fh_udp_sw_irq_status_1_port_p.ingress_timestamped_packets_bw_too_high_link_3) {
			fh_udp_sw_irq_clr_1_port_p.ingress_timestamped_packets_bw_too_high_link_3 = 1;
			ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].interrupt_stats_v2.ingress_timestamped_packets_bw_too_high_link[3]++;
		}



		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_CLR_0_PORT_P_V2,
				port_index,
				&fh_udp_sw_irq_clr_0_port_p);

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_CLR_1_PORT_P_V2,
				port_index,
				&fh_udp_sw_irq_clr_1_port_p);

	}

	spin_unlock_irqrestore(&ecpriss_pdata_v2->irq_lock, flags);
	return IRQ_HANDLED;
}
static int ecpriss_irq_init(ecpriss_qudp_interrupt_events_e qudp_irq,
		struct device *dev)
{
	int res= 0;

	struct platform_device *pdev = NULL;
	/*
	 *	 * c2c and l2 are not supported for now
	 *		 */
	if((qudp_irq == ECPRISS_UDP_FH_IRQ_PORT0) ||
			(qudp_irq == ECPRISS_UDP_FH_IRQ_PORT1) ||
			(qudp_irq == ECPRISS_UDP_FH_IRQ_PORT2)){
		ECPRILOGINFO("QUDP irq init\n");
	}else{
		return res;
	}
	do{

		pdev = to_platform_device(dev);

		if(dev ==  NULL)
		{
			ECPRILOGERR("ecpriss_irq_init pdev is NULL" );
			break;
		}
		ECPRILOGINFO("IRQ =  %d\n", qudp_irq_mapping[qudp_irq]);
		qudp_irq_mapping[qudp_irq] =  platform_get_irq(pdev, qudp_irq);

		res = request_irq(qudp_irq_mapping[qudp_irq], ecpriss_qudp_isr, IRQF_TRIGGER_HIGH, "ecpri_ss", NULL);

		if (res){
			ECPRILOGERR("IRQ request failed irq=%d res=%d\n",
					qudp_irq_mapping[qudp_irq], res);
			break;
		}
		ECPRILOGDBG("IRQ =  %d\n", qudp_irq_mapping[qudp_irq]);

		res = enable_irq_wake(qudp_irq_mapping[qudp_irq]);
		if (res){
			ECPRILOGERR("fail to enable IPA IRQ wakeup irq=%d res=%d\n",
					qudp_irq_mapping[qudp_irq], res);
			break;
		}

		ECPRILOGINFO("ecpriss_irq_init Interrupt Registration Success %d" , qudp_irq_mapping[qudp_irq]);

	}while (0);
	return res;
}
static int ecpriss_irq_init_v2(ecpriss_qudp_interrupt_events_e qudp_irq,
		struct device *dev)
{
	int res= 0;
	struct platform_device *pdev = NULL;
	/*
	 *	 * c2c and l2 are not supported for now
	 *		 */
	if((qudp_irq == ECPRISS_UDP_FH_IRQ_PORT0) ||
			(qudp_irq == ECPRISS_UDP_FH_IRQ_PORT1) ||
			(qudp_irq == ECPRISS_UDP_FH_IRQ_PORT2)){
		ECPRILOGINFO("QUDP irq init\n");
	}else{
		return res;
	}
	do{

		pdev = to_platform_device(dev);

		if(dev ==  NULL)
		{
			ECPRILOGERR("ecpriss_irq_init pdev is NULL" );
			break;
		}
		ECPRILOGINFO("IRQ =  %d\n", qudp_irq_mapping[qudp_irq]);
		qudp_irq_mapping[qudp_irq] =  platform_get_irq(pdev, qudp_irq);

		res = request_irq(qudp_irq_mapping[qudp_irq], ecpriss_qudp_isr_v2, IRQF_TRIGGER_HIGH, "ecpri_ss", NULL);

		if (res){
			ECPRILOGERR("IRQ request failed irq=%d res=%d\n",
					qudp_irq_mapping[qudp_irq], res);
			break;
		}
		ECPRILOGINFO("IRQ =  %d\n", qudp_irq_mapping[qudp_irq]);

		res = enable_irq_wake(qudp_irq_mapping[qudp_irq]);
		if (res){
			ECPRILOGERR("fail to enable IPA IRQ wakeup irq=%d res=%d\n",
					qudp_irq_mapping[qudp_irq], res);
			break;
		}

		ECPRILOGINFO("ecpriss_irq_init Interrupt Registration Success %d" , qudp_irq_mapping[qudp_irq]);

	}while (0);
	return res;
}

void ecpriss_qudp_irq_destroy_v2(void)
{
	uint32_t i = 0;

	for(i=0; i < QUDP_IRQ_MAX; i++){
		if((i == ECPRISS_UDP_FH_IRQ_PORT0) ||
				(i == ECPRISS_UDP_FH_IRQ_PORT1) ||
				(i == ECPRISS_UDP_FH_IRQ_PORT2)){

			disable_irq_wake(qudp_irq_mapping[i]);
			free_irq(qudp_irq_mapping[i],NULL);
		}
	}

}

/**
 * ecpri_qudp_reg_irq
 *
 *
 * Returns:	0 on success, negative on failure
 */
static int ecpriss_qudp_register_interrupts(uint8_t                 port_index,
		ecpriss_port_type_e     port_type,
		struct device						*dev)
{
	int res = 0;
	do{
		if(port_type == ECPRISS_PORT_TYPE_FH)
		{
			if(port_index == ECPRISS_PORT_0) {
				ecpriss_irq_init(ECPRISS_UDP_FH_IRQ_PORT0,dev);
			}
			else if(port_index == ECPRISS_PORT_1) {
				ecpriss_irq_init(ECPRISS_UDP_FH_IRQ_PORT1,dev);
			}
			else if(port_index == ECPRISS_PORT_2) {
				ecpriss_irq_init(ECPRISS_UDP_FH_IRQ_PORT2,dev);
			}
		}

		else if (port_type == ECPRISS_PORT_TYPE_C2C) {
			if(port_index == ECPRISS_PORT_0) {
				ecpriss_irq_init(ECPRISS_UDP_C2C_IRQ_PORT0,dev);
			}
			else if(port_index == ECPRISS_PORT_1) {
				ecpriss_irq_init(ECPRISS_UDP_C2C_IRQ_PORT1,dev);
			}
		}
		else if (port_type == ECPRISS_PORT_TYPE_L2) {
			ecpriss_irq_init(ECPRISS_UDP_L2_IRQ,dev);
		}

	}while (0);


	return res;
}

static int ecpriss_qudp_register_interrupts_v2(uint8_t                 port_index,
		ecpriss_port_type_e     port_type,
		struct device						*dev)
{
	do{
		if(port_type == ECPRISS_PORT_TYPE_FH)
		{
			if(port_index == ECPRISS_PORT_0) {
				ecpriss_irq_init_v2(ECPRISS_UDP_FH_IRQ_PORT0,dev);
			}
			else if(port_index == ECPRISS_PORT_1) {
				ecpriss_irq_init_v2(ECPRISS_UDP_FH_IRQ_PORT1,dev);
			}
			else if(port_index == ECPRISS_PORT_2) {
				ecpriss_irq_init_v2(ECPRISS_UDP_FH_IRQ_PORT2,dev);
			}
		}

		else if (port_type == ECPRISS_PORT_TYPE_C2C) {
			if(port_index == ECPRISS_PORT_0) {
				ecpriss_irq_init_v2(ECPRISS_UDP_C2C_IRQ_PORT0,dev);
			}
			else if(port_index == ECPRISS_PORT_1) {
				ecpriss_irq_init_v2(ECPRISS_UDP_C2C_IRQ_PORT1,dev);
			}
		}
		else if (port_type == ECPRISS_PORT_TYPE_L2) {
			ecpriss_irq_init_v2(ECPRISS_UDP_L2_IRQ,dev);
		}

	}while (0);


	return 0;
}


/*
static void ecpriss_qudp_enable_interrupts(uint8_t                 port_index,
		uint32_t                 port_type)
{
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_mask_0_port_p_s fh_udp_sw_irq_mask_0_port_p;
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_mask_1_port_p_s fh_udp_sw_irq_mask_1_port_p;

	memset(&fh_udp_sw_irq_mask_0_port_p , 0, sizeof(fh_udp_sw_irq_mask_0_port_p) );
	memset(&fh_udp_sw_irq_mask_1_port_p , 0, sizeof(fh_udp_sw_irq_mask_1_port_p) );



	if(port_type == ECPRISS_PORT_TYPE_FH) {




		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_MASK_0_PORT_P,
				port_index,
				&fh_udp_sw_irq_mask_0_port_p);


		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_MASK_1_PORT_P,
				port_index,
				&fh_udp_sw_irq_mask_1_port_p);




		fh_udp_sw_irq_mask_0_port_p.egress_mtu_err_packet_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_fcs_err_packet_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_pkt_fifo_empty_before_eop_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_ipv4_cs_error_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_udp_cs_error_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_ip_filtered_packet_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_vlan_filtered_packet_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_sec_err_packet_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_ip_len_err_packet_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_0_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_1_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_2_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_3_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_last_in_chain_non_local_dst_packet_link_0 = 1;

		fh_udp_sw_irq_mask_0_port_p.egress_mtu_err_packet_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_fcs_err_packet_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_pkt_fifo_empty_before_eop_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_ipv4_cs_error_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_udp_cs_error_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_ip_filtered_packet_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_vlan_filtered_packet_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_sec_err_packet_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_ip_len_err_packet_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_0_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_1_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_2_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_3_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_last_in_chain_non_local_dst_packet_link_1 = 1;


		fh_udp_sw_irq_mask_1_port_p.egress_mtu_err_packet_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_fcs_err_packet_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_pkt_fifo_empty_before_eop_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_ipv4_cs_error_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_udp_cs_error_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_ip_filtered_packet_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_vlan_filtered_packet_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_sec_err_packet_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_ip_len_err_packet_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_0_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_1_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_2_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_3_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_last_in_chain_non_local_dst_packet_link_2 = 1;

		fh_udp_sw_irq_mask_1_port_p.egress_mtu_err_packet_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_fcs_err_packet_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_pkt_fifo_empty_before_eop_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_ipv4_cs_error_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_udp_cs_error_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_ip_filtered_packet_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_vlan_filtered_packet_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_sec_err_packet_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_ip_len_err_packet_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_0_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_1_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_2_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_3_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_last_in_chain_non_local_dst_packet_link_3 = 1;

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_MASK_0_PORT_P,
				port_index,
				&fh_udp_sw_irq_mask_0_port_p);

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_MASK_1_PORT_P,
				port_index,
				&fh_udp_sw_irq_mask_1_port_p);

	}

	return;
}
*/

static void ecpriss_qudp_disable_interrupts(uint8_t                 port_index,
		uint32_t                 port_type)
{
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_mask_0_port_p_s fh_udp_sw_irq_mask_0_port_p;
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_mask_1_port_p_s fh_udp_sw_irq_mask_1_port_p;

	memset(&fh_udp_sw_irq_mask_0_port_p , 0, sizeof(fh_udp_sw_irq_mask_0_port_p) );
	memset(&fh_udp_sw_irq_mask_1_port_p , 0, sizeof(fh_udp_sw_irq_mask_1_port_p) );

	if(port_type == ECPRISS_PORT_TYPE_FH) {

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_MASK_0_PORT_P,
				port_index,
				&fh_udp_sw_irq_mask_0_port_p);

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_MASK_1_PORT_P,
				port_index,
				&fh_udp_sw_irq_mask_1_port_p);

	}

	return;
}

/*
static void ecpriss_qudp_enable_interrupts_v2(uint8_t                 port_index,
		uint32_t                 port_type)
{
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_mask_0_port_p_s_v2 fh_udp_sw_irq_mask_0_port_p;
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_mask_1_port_p_s_v2 fh_udp_sw_irq_mask_1_port_p;

	memset(&fh_udp_sw_irq_mask_0_port_p , 0, sizeof(fh_udp_sw_irq_mask_0_port_p) );
	memset(&fh_udp_sw_irq_mask_1_port_p , 0, sizeof(fh_udp_sw_irq_mask_1_port_p) );



	if(port_type == ECPRISS_PORT_TYPE_FH) {

		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_MASK_0_PORT_P_V2,
				port_index,
				&fh_udp_sw_irq_mask_0_port_p);


		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_MASK_1_PORT_P_V2,
				port_index,
				&fh_udp_sw_irq_mask_1_port_p);

		fh_udp_sw_irq_mask_0_port_p.egress_mtu_err_packet_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_fcs_err_packet_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_pkt_fifo_empty_before_eop_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_ipv4_cs_error_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_udp_cs_error_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_ip_filtered_packet_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_vlan_filtered_packet_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_sec_err_packet_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_ip_len_err_packet_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_0_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_1_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_2_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_3_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_last_in_chain_non_local_dst_packet_link_0 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_timestamped_packets_bw_too_high_link_0= 1;

		fh_udp_sw_irq_mask_0_port_p.egress_mtu_err_packet_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_fcs_err_packet_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_pkt_fifo_empty_before_eop_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_ipv4_cs_error_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_udp_cs_error_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_ip_filtered_packet_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_vlan_filtered_packet_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_sec_err_packet_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_ip_len_err_packet_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_0_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_1_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_2_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_trap_rule_3_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_last_in_chain_non_local_dst_packet_link_1 = 1;
		fh_udp_sw_irq_mask_0_port_p.ingress_timestamped_packets_bw_too_high_link_1= 1;

		fh_udp_sw_irq_mask_1_port_p.egress_mtu_err_packet_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_fcs_err_packet_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_pkt_fifo_empty_before_eop_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_ipv4_cs_error_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_udp_cs_error_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_ip_filtered_packet_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_vlan_filtered_packet_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_sec_err_packet_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_ip_len_err_packet_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_0_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_1_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_2_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_3_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_last_in_chain_non_local_dst_packet_link_2 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_timestamped_packets_bw_too_high_link_2= 1;

		fh_udp_sw_irq_mask_1_port_p.egress_mtu_err_packet_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_fcs_err_packet_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_pkt_fifo_empty_before_eop_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_ipv4_cs_error_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_udp_cs_error_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_ip_filtered_packet_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_vlan_filtered_packet_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_sec_err_packet_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_ip_len_err_packet_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_0_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_1_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_2_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_trap_rule_3_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_last_in_chain_non_local_dst_packet_link_3 = 1;
		fh_udp_sw_irq_mask_1_port_p.ingress_timestamped_packets_bw_too_high_link_3= 1;

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_MASK_0_PORT_P_V2,
				port_index,
				&fh_udp_sw_irq_mask_0_port_p);

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_MASK_1_PORT_P_V2,
				port_index,
				&fh_udp_sw_irq_mask_1_port_p);

	}

	return;
}
*/

static void ecpriss_qudp_disable_interrupts_v2(uint8_t                 port_index,
		uint32_t                 port_type)
{
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_mask_0_port_p_s_v2 fh_udp_sw_irq_mask_0_port_p;
	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_sw_irq_mask_1_port_p_s_v2 fh_udp_sw_irq_mask_1_port_p;

	memset(&fh_udp_sw_irq_mask_0_port_p , 0, sizeof(fh_udp_sw_irq_mask_0_port_p) );
	memset(&fh_udp_sw_irq_mask_1_port_p , 0, sizeof(fh_udp_sw_irq_mask_1_port_p) );

	if(port_type == ECPRISS_PORT_TYPE_FH) {

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_MASK_0_PORT_P_V2,
				port_index,
				&fh_udp_sw_irq_mask_0_port_p);

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_UDP_SW_IRQ_MASK_1_PORT_P_V2,
				port_index,
				&fh_udp_sw_irq_mask_1_port_p);

	}

	return;
}

/**
 * ecpri_qudp_set_eth_type()
 *
 *
 * Returns:	0 on success, negative on failure
 */
int ecpriss_qudp_init(struct device *dev)
{
	int ret = 0;
	int port_type = 0;
	int port_idx = 0;

	do
	{

		ecpriss_pdata->qudp_ctx->state = ECPRI_QUDP_DEINIT ;

		/* ecpriss_global_operation_mode_cfg(); */

		ret = ecpriss_qudp_global_hal_reg_init(dev,
				ecpriss_pdata->ecpri_hw_ver);
		if(ret < 0)
		{
			break;
		}

		ret = ecpriss_qudp_fh_hal_reg_init(dev);
		if(ret < 0)
		{
			break;
		}

		ret = ecpriss_qudp_ingress_init_cfg();

		if(ret < 0)
		{
			break;
		}

		ecpriss_qudp_fh_egress_cfg_reset(0);
		ecpriss_qudp_fh_egress_cfg_reset(1);
		ecpriss_qudp_fh_egress_cfg_reset(2);

		for(port_type=0;port_type < ECPRISS_PORT_TYPE_MAX;port_type++)
		{
			for(port_idx=0;port_idx < ECPRISS_PORT_MAX;port_idx++)
			{


				ret = ecpriss_qudp_register_interrupts(port_idx,port_type,dev);
				if(ret < 0)
				{
					break;
				}

				ecpriss_qudp_disable_interrupts(port_idx,port_type);

				/* ret = ecpriss_qudp_enable_stats(port_idx,port_type); */
				if(ret < 0)
				{
					break;
				}
			}
		}


		ecpriss_qudp_configure_mtu();
		ecpriss_pdata->qudp_ctx->state = ECPRI_QUDP_READY ;

	}while (0);
	return ret;
}


/*
 * ecpriss_qudp_set_cascade_fh_mac_dst_check_v2 - Enable MAC dst address
 * filtering on FH ingress ports FH1 and FH2 for RU cascade mode.
 * Sets enable_mac_dst_check=1 with non_local_dst_action=PASS_TO_REMOTE so
 * packets whose dst MAC does not match the local FH port MAC are forwarded
 * to the remote RU instead of being dropped.
 */
void ecpriss_qudp_set_cascade_fh_mac_dst_check_v2(void)
{
	ecpri_qudp_hwio_def_ecpri_udp_fh_ingress_config_p_s_v2 ingress_cfg;
	uint32_t port_index;

	ECPRILOGINFO("ecpriss_qudp_set_cascade_fh_mac_dst_check_v2: setting enable_mac_dst_check=1 non_local_dst_action=%d for FH1 and FH2\n",
			ECPRISS_QUDP_ACTION_PASS_TO_REMOTE);

	/* Apply to FH1 (p=1) and FH2 (p=2) only */
	for (port_index = 1; port_index <= 2; port_index++) {
		memset(&ingress_cfg, 0, sizeof(ingress_cfg));

		ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_INGRESS_CONFIG_P_V2,
				port_index,
				&ingress_cfg);

		ingress_cfg.enable_mac_dst_check = 1;
		ingress_cfg.non_local_dst_action = ECPRISS_QUDP_ACTION_PASS_TO_REMOTE;

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_INGRESS_CONFIG_P_V2,
				port_index,
				&ingress_cfg);

		ECPRILOGINFO("ecpriss_qudp_set_cascade_fh_mac_dst_check_v2: port_index=%d done\n", port_index);
	}
}


/*
 * ecpriss_qudp_set_cascade_l2_mac_dst_check_v2 - Enable MAC dst address
 * filtering on the L2 (C2C2/eth30) ingress port for RU cascade mode.
 * Sets enable_mac_dst_check=1 and non_local_dst_action=PASS_TO_REMOTE so
 * that non-local packets received on the C2C2 link are forwarded to the
 * remote QUDP path rather than dropped.
 */
void ecpriss_qudp_set_cascade_l2_mac_dst_check_v2(void)
{
	ecpri_qudp_hwio_def_ecpri_udp_l2_ingress_config_p_u_v2 ingress_cfg;

	ECPRILOGINFO("ecpriss_qudp_set_cascade_l2_mac_dst_check_v2: setting enable_mac_dst_check=1 non_local_dst_action=%d for L2 p=0\n",
			ECPRISS_QUDP_ACTION_PASS_TO_REMOTE);

	/* Apply to L2 p=0 only.
	 * Use raw read_reg_mn/write_reg_mn to avoid NULL construct/parse
	 * pointers - no HAL dispatch entry exists for this reg in v2 */
	ingress_cfg.value = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_CONFIG_P,
			0, 0);

	ingress_cfg.def.enable_mac_dst_check = 1;
	ingress_cfg.def.non_local_dst_action = ECPRISS_QUDP_ACTION_PASS_TO_REMOTE;

	ecpriss_qudp_hal_write_reg_mn(ECPRISS_QUDP_L2,
			ECPRI_UDP_L2_INGRESS_CONFIG_P,
			0, 0,
			ingress_cfg.value);

	ECPRILOGINFO("ecpriss_qudp_set_cascade_l2_mac_dst_check_v2: L2 p=0 val=0x%x done\n", ingress_cfg.value);
}


/*
 * ecpriss_qudp_set_cascade_arp_trap_rules_v2 - Install ARP trap rules on
 * FH ingress ports FH1 and FH2 for RU cascade mode.
 * Configures trap rule entry n=3 to redirect ARP packets arriving on the
 * cascaded FH ports to the A55 CPU so that ARP requests from the remote
 * RU are handled locally.
 */
void ecpriss_qudp_set_cascade_arp_trap_rules_v2(void)
{
	ecpri_qudp_hwio_def_ecpri_udp_fh_trap_misc_port_p_entry_n_s_v2 trap_misc_cfg;
	uint32_t cfg_value;
	uint32_t port_index;

	ECPRILOGINFO("ecpriss_qudp_set_cascade_arp_trap_rules_v2: enabling ARP trap on FH1 and FH2 (p=1,2) n=3\n");

	/* Apply to FH1 (p=1) and FH2 (p=2), small rule index n=3 */
	for (port_index = 1; port_index <= 2; port_index++) {

		/* --- MISC: enable trap, action=pass to A55, rule32_offset=12 (Ethertype) --- */
		memset(&trap_misc_cfg, 0, sizeof(trap_misc_cfg));
		trap_misc_cfg.enable       = 1;
		trap_misc_cfg.action       = 1; /* pass to A55 */
		trap_misc_cfg.rule32_offset = 12; /* Ethertype at bytes [12:13] */

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_MISC_PORT_p_ENTRY_n_V2,
				port_index, 3,
				&trap_misc_cfg);

		/* --- Rule32 value: ARP Ethertype 0x0806 --- */
		cfg_value = 0x00000608;
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE32_VAL_PORT_p_ENTRY_n_V2,
				port_index, 3,
				&cfg_value);

		/* --- Rule32 mask: 0xFFFF to match first 2 bytes (Ethertype) --- */
		cfg_value = 0x0000FFFF;
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE32_MASK_PORT_p_ENTRY_n_V2,
				port_index, 3,
				&cfg_value);

		/* --- Rule64 val LSB/MSB: 0 (not used) --- */
		cfg_value = 0x00000000;
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE64_VAL_LSB_PORT_p_ENTRY_n_V2,
				port_index, 3,
				&cfg_value);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE64_VAL_MSB_PORT_p_ENTRY_n_V2,
				port_index, 3,
				&cfg_value);

		/* --- Rule64 mask LSB/MSB: 0 (not used) --- */
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE64_MASK_LSB_PORT_p_ENTRY_n_V2,
				port_index, 3,
				&cfg_value);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE64_MASK_MSB_PORT_p_ENTRY_n_V2,
				port_index, 3,
				&cfg_value);

		ECPRILOGINFO("ecpriss_qudp_set_cascade_arp_trap_rules_v2: port_index=%d n=3 done\n", port_index);
	}
}



/*
 * ecpriss_qudp_set_cascade_icmp_trap_rules_v2 - Install ICMP trap rules on
 * FH ingress ports FH1 and FH2 for RU cascade mode.
 * Configures trap rule entry n=2 to redirect ICMP packets arriving on the
 * cascaded FH ports to the A55 CPU so that ping/ICMP requests from the
 * remote RU are handled locally.
 */
void ecpriss_qudp_set_cascade_icmp_trap_rules_v2(void)
{
	ecpri_qudp_hwio_def_ecpri_udp_fh_trap_misc_port_p_entry_n_s_v2 trap_misc_cfg;
	uint32_t cfg_value;
	uint32_t port_index;

	ECPRILOGINFO("ecpriss_qudp_set_cascade_icmp_trap_rules_v2: enabling ICMP trap on FH1 and FH2 (p=1,2) n=2\n");

	/* Apply to FH1 (p=1) and FH2 (p=2), small rule index n=2 */
	for (port_index = 1; port_index <= 2; port_index++) {

		/* --- MISC: enable trap, action=5 (add timestamp + pass to A55), rule32_offset=12 --- */
		memset(&trap_misc_cfg, 0, sizeof(trap_misc_cfg));
		trap_misc_cfg.enable        = 1;
		trap_misc_cfg.action        = 5; /* add timestamp and pass to A55 */
		trap_misc_cfg.rule32_offset = 12; /* Ethertype at bytes [12:13] */

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_MISC_PORT_p_ENTRY_n_V2,
				port_index, 2,
				&trap_misc_cfg);

		/* --- Rule32 value: IPv4 Ethertype 0x0800 for ICMP packets --- */
		cfg_value = 0x00000008;
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE32_VAL_PORT_p_ENTRY_n_V2,
				port_index, 2,
				&cfg_value);

		/* --- Rule32 mask: 0xFFFF to extract 2 bytes (Ethertype) from offset 12 --- */
		cfg_value = 0x0000FFFF;
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE32_MASK_PORT_p_ENTRY_n_V2,
				port_index, 2,
				&cfg_value);

		/* --- Rule64 val LSB/MSB: 0 (not used) --- */
		cfg_value = 0x00000000;
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE64_VAL_LSB_PORT_p_ENTRY_n_V2,
				port_index, 2,
				&cfg_value);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE64_VAL_MSB_PORT_p_ENTRY_n_V2,
				port_index, 2,
				&cfg_value);

		/* --- Rule64 mask LSB/MSB: 0 (not used) --- */
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE64_MASK_LSB_PORT_p_ENTRY_n_V2,
				port_index, 2,
				&cfg_value);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_TRAP_RULE64_MASK_MSB_PORT_p_ENTRY_n_V2,
				port_index, 2,
				&cfg_value);

		ECPRILOGINFO("ecpriss_qudp_set_cascade_icmp_trap_rules_v2: port_index=%d n=2 done\n", port_index);
	}
}

int ecpri_global_cfg_init_cascade_mode(void) {

       ecpri_global_hwio_def_ecpri_global_cfg_s ecpri_global_cfg;

       memset(&ecpri_global_cfg, 0,
                       sizeof(ecpri_global_hwio_def_ecpri_global_cfg_s));

       /*Enable CASCADE Mode*/
       ecpri_global_cfg.operation_mode = HWIO_ECPRI_GLOBAL_CFG_OPERATION_MODE_RU_CASCADE_FVAL;
       ecpri_global_cfg.ahb_resp_err_en = 0x1;

       ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_GLOBAL,
                       ECPRI_GLOBAL_CFG, 0,
                       &ecpri_global_cfg);

       return 0;
}

int ecpri_global_cfg_deinit_cascade_mode(void) {

       ecpri_global_hwio_def_ecpri_global_cfg_s ecpri_global_cfg;

       memset(&ecpri_global_cfg, 0,
                       sizeof(ecpri_global_hwio_def_ecpri_global_cfg_s));

       /*Disable CASCADE Mode*/
       ecpri_global_cfg.operation_mode = HWIO_ECPRI_GLOBAL_CFG_OPERATION_MODE_DU_WITHOUT_L2_FVAL;
       ecpri_global_cfg.ahb_resp_err_en = 0x1;

       ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_GLOBAL,
                       ECPRI_GLOBAL_CFG, 0,
                       &ecpri_global_cfg);

       return 0;
}

int ecpriss_qudp_init_v2(struct device *dev)
{
	int ret = 0;
	int port_type = 0;
	int port_idx = 0;

	do
	{

		ecpriss_pdata_v2->qudp_ctx_v2->state = ECPRI_QUDP_DEINIT ;


		ret = ecpriss_qudp_global_hal_reg_init(dev,
				ecpriss_pdata_v2->ecpri_hw_ver);

		if(ret < 0)
		{
			break;
		}

		if(cascade_enable)
			ecpri_global_cfg_init_cascade_mode();

		ret = ecpriss_qudp_fh_hal_reg_init(dev);
		if(ret < 0)
		{
			break;
		}

		ret = ecpriss_qudp_l2_hal_reg_init(dev);
		if(ret < 0)
		{
			break;
		}

		ret = ecpriss_qudp_ingress_init_cfg_v2();

		if(ret < 0)
		{
			break;
		}

		if (ru_cascade_mode) {
			ecpriss_qudp_set_cascade_fh_mac_dst_check_v2();
			ecpriss_qudp_set_cascade_l2_mac_dst_check_v2();
			ecpriss_qudp_set_cascade_arp_trap_rules_v2();
			ecpriss_qudp_set_cascade_icmp_trap_rules_v2();
			/* Keep global in sync so future ingress_init_cfg_modify calls
			 * do not overwrite the PASS_TO_REMOTE setting on FH1/FH2.
			 */
			ecpriss_qudp_ingress_action = ECPRISS_QUDP_ACTION_PASS_TO_REMOTE;
			ECPRILOGINFO("ecpriss_qudp_init_v2: ru_cascade_mode active, ecpriss_qudp_ingress_action set to PASS_TO_REMOTE (%d)\n",
					ECPRISS_QUDP_ACTION_PASS_TO_REMOTE);
		}

		ret = ecpriss_qudp_egress_init_cfg_v2();

		if(ret < 0)
		{
			break;
		}


		ecpriss_qudp_fh_egress_cfg_reset_v2(0);
		ecpriss_qudp_fh_egress_cfg_reset_v2(1);
		ecpriss_qudp_fh_egress_cfg_reset_v2(2);
		ecpriss_qudp_l2_egress_cfg_reset_v2(0);

		for(port_type=0;port_type < ECPRISS_PORT_TYPE_MAX;port_type++)
		{
			for(port_idx=0;port_idx < ECPRISS_PORT_MAX;port_idx++)
			{

				ret = ecpriss_qudp_register_interrupts_v2(port_idx,port_type,dev);
				if(ret < 0)
				{
					break;
				}

				ecpriss_qudp_disable_interrupts_v2(port_idx,port_type);

				/* ret = ecpriss_qudp_enable_stats_v2(port_idx,port_type); */
				if(ret < 0)
				{
					break;
				}
			}
		}

		ecpriss_qudp_strict_filter_cfg_v2();
		ecpriss_qudp_configure_mtu_v2();
		ecpriss_qudp_non_ecpri_dma_ring_info();

		ecpriss_filtering_enabled = 1;

		if(ecpriss_pdata_v2->dev_mode != ECPRISS_DEV_MODE_RU && ecpriss_pdata_v2->qudp_ctx_v2->lte_fh_enabled) {
			ecpriss_qudp_set_lte_mac_filter_info();
			ecpriss_qudp_set_nr_mac_filter_info();
		}

		if(ecpriss_pdata_v2->dev_mode != ECPRISS_DEV_MODE_RU && ru_cascade_mode) {
			ecpriss_qudp_set_nr_mac_filter_info();
		}

		ecpriss_qudp_l2_egress_tp_cfg_v2(0);

		ecpriss_pdata_v2->qudp_ctx_v2->state = ECPRI_QUDP_READY ;

	}while (0);
	return ret;
}
static bool ecpriss_qudp_fh_rx_is_ip_filter_exist_v2(
		ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr0_port_p_entry_n_s_v2 dst_ip0,
		ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr1_port_p_entry_n_s_v2 dst_ip1,
		ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr2_port_p_entry_n_s_v2 dst_ip2,
		ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr3_port_p_entry_n_s_v2 dst_ip3,
		uint8_t *filter_idx,
		uint8_t port_index,
		ecpriss_ip_type ip_type
		)
{
	uint32_t i  =0;
	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port =
		&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].ingress_port_cfg;

	for(i = 0; i< MAX_WHITELIST_ENTRIES; i++){
		if(dst_ip0.value == qudp_ingress_port->ipdst_addr[i][0]){
			if(ip_type == ECPRISS_IPV6_TYPE)
			{
				if(dst_ip1.value == qudp_ingress_port->ipdst_addr[i][1] &&
						dst_ip2.value == qudp_ingress_port->ipdst_addr[i][2] &&
						dst_ip3.value == qudp_ingress_port->ipdst_addr[i][3]){
					*filter_idx = i;
					return true;
				}
			}else{
				*filter_idx = i;
					return true;
			}

		}
	}
	return false;
}
static bool ecpriss_qudp_fh_rx_get_available_ipfilter_idx_v2(
		uint8_t *filter_idx,
		uint8_t port_index,
		ecpriss_ip_type ip_type
		)
{
	uint32_t i  =0;
	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port =
		&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].ingress_port_cfg;

	for(i = 0; i< MAX_WHITELIST_ENTRIES; i++){
		if(0 == qudp_ingress_port->ipdst_addr[i][0]){
			if(ip_type == ECPRISS_IPV6_TYPE)
			{
				if(0 == qudp_ingress_port->ipdst_addr[i][1] &&
						0 == qudp_ingress_port->ipdst_addr[i][2] &&
						0 == qudp_ingress_port->ipdst_addr[i][3]){
					*filter_idx = i;
					return true;
				}
			}else{
				*filter_idx = i;
					return true;
			}

		}
	}
	return false;
}

static bool ecpriss_qudp_fh_rx_is_ip_filter_exist(
		ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr0_port_p_entry_n_s dst_ip0,
		ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr1_port_p_entry_n_s dst_ip1,
		ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr2_port_p_entry_n_s dst_ip2,
		ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr3_port_p_entry_n_s dst_ip3,
		uint8_t *filter_idx,
		uint8_t port_index,
		ecpriss_ip_type ip_type
		)
{
	uint32_t i  =0;
	ecpriss_qudp_ingress_per_port_cfg_s *qudp_ingress_port =
		&ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].ingress_port_cfg;

	for(i = 0; i< MAX_WHITELIST_ENTRIES; i++){
		if(dst_ip0.value == qudp_ingress_port->ipdst_addr[i][0]){
			if(ip_type == ECPRISS_IPV6_TYPE)
			{
				if(dst_ip1.value == qudp_ingress_port->ipdst_addr[i][1] &&
						dst_ip2.value == qudp_ingress_port->ipdst_addr[i][2] &&
						dst_ip3.value == qudp_ingress_port->ipdst_addr[i][3]){
					*filter_idx = i;
					return true;
				}
			}else{
				*filter_idx = i;
					return true;
			}

		}
	}
	return false;
}
static bool ecpriss_qudp_fh_rx_get_available_ipfilter_idx(
		uint8_t *filter_idx,
		uint8_t port_index,
		ecpriss_ip_type ip_type
		)
{
	uint32_t i  =0;
	ecpriss_qudp_ingress_per_port_cfg_s *qudp_ingress_port =
		&ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].ingress_port_cfg;

	for(i = 0; i< MAX_WHITELIST_ENTRIES; i++){
		if(0 == qudp_ingress_port->ipdst_addr[i][0]){
			if(ip_type == ECPRISS_IPV6_TYPE)
			{
				if(0 == qudp_ingress_port->ipdst_addr[i][1] &&
						0 == qudp_ingress_port->ipdst_addr[i][2] &&
						0 == qudp_ingress_port->ipdst_addr[i][3]){
					*filter_idx = i;
					return true;
				}
			}else{
				*filter_idx = i;
					return true;
			}

		}
	}
	return false;
}
static bool ecpriss_qudp_fh_rx_ip_filter_cfg(uint32_t port_index, ecpriss_qudp_rx_cfg_s *rx_cfg)
{

	uint8_t filter_idx = 0;
	uint8_t cfg_action = DE_CONFIGURE;
	ecpriss_qudp_ingress_per_port_cfg_s *qudp_ingress_port =
		&ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].ingress_port_cfg;

	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr0_port_p_entry_n_s dst_ip0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr1_port_p_entry_n_s dst_ip1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr2_port_p_entry_n_s dst_ip2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr3_port_p_entry_n_s dst_ip3 = {0};

	if(rx_cfg == NULL) {
		return false;
	}

	if(rx_cfg->fltr_en_mask & ECPRISS_QUDP_RX_CFG_FLTR_MASK_IP_DADDR){
		cfg_action = CONFIGURE;
	}
	if(cfg_action == CONFIGURE){
		if(qudp_ingress_port->num_ip_fltr_entries >= MAX_WHITELIST_ENTRIES){
			ECPRILOGERR("Cannot apply new IP filter, Capacity is full\n");
			return false;
		}
	}
	if(cfg_action == DE_CONFIGURE){
		if(qudp_ingress_port->num_ip_fltr_entries <= 0){
			ECPRILOGERR("Cannot Remove IP filter, No filter exist\n");
			return false;
		}
	}
	dst_ip0.value = ((rx_cfg->ip_dst_addr[3]) | (rx_cfg->ip_dst_addr[2] << 8) | (rx_cfg->ip_dst_addr[1] << 16)
			| (rx_cfg->ip_dst_addr[0] << 24));

	if(rx_cfg->ip_type == ECPRISS_IPV6_TYPE)
	{
		dst_ip1.value = ((rx_cfg->ip_dst_addr[7]) | (rx_cfg->ip_dst_addr[6] << 8) | (rx_cfg->ip_dst_addr[5] << 16)
				| (rx_cfg->ip_dst_addr[4] << 24));

		dst_ip2.value = ((rx_cfg->ip_dst_addr[11]) | (rx_cfg->ip_dst_addr[10] << 8) | (rx_cfg->ip_dst_addr[9] << 16)
				| (rx_cfg->ip_dst_addr[8] << 24));

		dst_ip3.value = ((rx_cfg->ip_dst_addr[15]) | (rx_cfg->ip_dst_addr[14] << 8) | (rx_cfg->ip_dst_addr[13] << 16)
				| (rx_cfg->ip_dst_addr[12] << 24));
	}
	if(cfg_action == DE_CONFIGURE){
		if((dst_ip0.value + dst_ip1.value + dst_ip2.value + dst_ip3.value ) == 0){
			ECPRILOGERR("Invalid ip Config \n");
			return false;
		}
	}
	/*
	 * Check if the IP filter already exist
	 */
	if(ecpriss_filtering_enabled){
		if(ecpriss_qudp_fh_rx_is_ip_filter_exist(dst_ip0, dst_ip1 , dst_ip2, dst_ip3, &filter_idx, port_index, rx_cfg->ip_type)){
			ECPRILOGERR("IP Filter exist in database \n");
			if(cfg_action == CONFIGURE){
				ECPRILOGERR("Cannot apply IP filter, Already exist\n");
				return false;
			}
		}else{
			ECPRILOGERR("IP Filter Doesn't exist in database \n");
			if(cfg_action == CONFIGURE){
				if(!ecpriss_qudp_fh_rx_get_available_ipfilter_idx(&filter_idx, port_index, rx_cfg->ip_type)){
					ECPRILOGERR("Ip filter configuration is full, Can not apply new ip filtr config\n");
					return false;
				}

			}else{
				ECPRILOGERR("Cannot remove IP filter, does not exist\n");
				return false;
			}
		}
	}
	/*
	 * if ecpriss_filtering_enabled is set or it is One
	 * remove the matching filter
	 */
	if(ecpriss_filtering_enabled == 1){
		/*
		 * remove filters
		 */
		if(cfg_action == DE_CONFIGURE){
			ECPRILOGERR("forcing IP filter to zero to remove filter \n");
			dst_ip0.value = 0;
			dst_ip1.value = 0;
			dst_ip2.value = 0;
			dst_ip3.value = 0;
			if(qudp_ingress_port->num_ip_fltr_entries)
				qudp_ingress_port->num_ip_fltr_entries--;
		}
	}

	ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
			ECPRI_UDP_FH_FILT_IP_DST_ADDR0_PORT_p_ENTRY_n,
			port_index,
			filter_idx,
			&dst_ip0);

	qudp_ingress_port->ipdst_addr[filter_idx][0] = dst_ip0.value ;


	if(rx_cfg->ip_type == ECPRISS_IPV6_TYPE)
	{

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_FILT_IP_DST_ADDR1_PORT_p_ENTRY_n,
				port_index,
				filter_idx,
				&dst_ip1);

		qudp_ingress_port->ipdst_addr[filter_idx][1] = dst_ip1.value ;

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_FILT_IP_DST_ADDR2_PORT_p_ENTRY_n,
				port_index,
				filter_idx,
				&dst_ip2);

		qudp_ingress_port->ipdst_addr[filter_idx][2] = dst_ip2.value ;

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_FILT_IP_DST_ADDR3_PORT_p_ENTRY_n,
				port_index,
				filter_idx,
				&dst_ip3);

		qudp_ingress_port->ipdst_addr[filter_idx][3] = dst_ip3.value ;

	}

	if(ecpriss_filtering_enabled != 0 && cfg_action == CONFIGURE){
		qudp_ingress_port->num_ip_fltr_entries++;
	}
	ecpriss_qudp_ingress_modify_cfg(port_index,
			ENABLE_FILTER,
			ECPRISS_QUDP_RX_CFG_FLTR_MASK_IP_DADDR,
			filter_idx,
			cfg_action);

	return true;
}

static bool ecpriss_qudp_fh_rx_vlan_filter_cfg(uint32_t port_index, ecpriss_qudp_rx_cfg_s *rx_cfg)
{

	int i = 0;
	uint8_t filter_idx = 0;
	uint8_t cfg_action = DE_CONFIGURE;
	ecpriss_qudp_ingress_per_port_cfg_s *qudp_ingress_port =
		&ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].ingress_port_cfg;

	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_vlan_addr_port_p_entry_n_s vlan_addr_port = {0};

	if(rx_cfg == NULL) {
		return false;
	}
	if(rx_cfg->vlan_addr_port > 0)
	{
		filter_idx = 0;
		cfg_action = DE_CONFIGURE;
		if(rx_cfg->fltr_en_mask & ECPRISS_QUDP_RX_CFG_FLTR_MASK_VLAN){
			cfg_action = CONFIGURE;
		}
		if(cfg_action == CONFIGURE){
			if(qudp_ingress_port->num_vlan_fltr_entries >= MAX_WHITELIST_ENTRIES){
				ECPRILOGERR("Cannot apply new VLAN filter, Capacity is full\n");
				return false;
			}
		}
		if(cfg_action == DE_CONFIGURE){
			if(qudp_ingress_port->num_vlan_fltr_entries <= 0){
				ECPRILOGERR("Cannot remove VLAN filter, No filter exist\n");
				return false;
			}
		}

		/*
		 * Check if the VLAN filter already exist
		 */
		if(ecpriss_filtering_enabled){
			for(i = 0; i< MAX_WHITELIST_ENTRIES; i++){
				if(rx_cfg->vlan_addr_port == qudp_ingress_port->vlan_addr[i]){
					if(cfg_action == CONFIGURE){
						ECPRILOGERR("Cannot apply VLAN filter :%u, Already exist\n",rx_cfg->vlan_addr_port);
						return false;
					}else{
						ECPRILOGERR("Filter exist at index %u\n", i);
						filter_idx = i;
						break;
					}
				}
			}

			if(cfg_action == DE_CONFIGURE){
				if(filter_idx != i){
					ECPRILOGERR("VLAN filter does not exist\n");
					return false;
				}
			}else{
				/*
				 * look for available space in table
				 */
				ECPRILOGERR("Looking for available slot in table\n");
				for(i = 0; i< MAX_WHITELIST_ENTRIES; i++){
					if(0 == qudp_ingress_port->vlan_addr[i]){
						filter_idx = i;
						break;
					}
				}
				if(i != filter_idx){
					ECPRILOGERR("VLAN filter full, can not cfg new filter\n");
					return false;
				}

			}
		}
		/*
		 * remove the last applied filter
		 */
		if(ecpriss_filtering_enabled == 1){
			if(cfg_action == DE_CONFIGURE){
				ECPRILOGERR("forcing vlan id to zero to remove filter \n");
				rx_cfg->vlan_addr_port = 0;
				if(qudp_ingress_port->num_vlan_fltr_entries)
					qudp_ingress_port->num_vlan_fltr_entries--;
			}
		}
		vlan_addr_port.value = rx_cfg->vlan_addr_port;

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_FILT_VLAN_ADDR_PORT_p_ENTRY_n,
				port_index,
				filter_idx,
				&vlan_addr_port);

		qudp_ingress_port->vlan_addr[filter_idx] = rx_cfg->vlan_addr_port;


		if(ecpriss_filtering_enabled != 0 && cfg_action == CONFIGURE){
			qudp_ingress_port->num_vlan_fltr_entries++;
		}
		ecpriss_qudp_ingress_modify_cfg(port_index,
				ENABLE_FILTER,
				ECPRISS_QUDP_RX_CFG_FLTR_MASK_VLAN,
				filter_idx,
				cfg_action);

	}
	return true;
}
static bool ecpriss_qudp_fh_rx_udp_filter_cfg(uint32_t port_index, ecpriss_qudp_rx_cfg_s *rx_cfg)
{

	int i = 0;
	uint8_t filter_idx = 0;
	uint8_t cfg_action = DE_CONFIGURE;
	ecpriss_qudp_ingress_per_port_cfg_s *qudp_ingress_port =
		&ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index].ingress_port_cfg;

	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_classification_list_port_p_entry_n_s udp_classification_port = {0};

	if(rx_cfg == NULL) {
		return false;
	}
	if(rx_cfg->udp_dst_port > 0)
	{
		if(rx_cfg->fltr_en_mask & ECPRISS_QUDP_RX_CFG_FLTR_MASK_UDP_DPORT){
			cfg_action = CONFIGURE;
		}

		udp_classification_port.value = rx_cfg->udp_dst_port;
		if(cfg_action == CONFIGURE){
			ECPRILOGDBG("configure UDP\n");
		}else{
			ECPRILOGDBG("De configure UDP\n");
		}
		ECPRILOGDBG("udp_classification_port.value = %u\n",udp_classification_port.value);
		ECPRILOGDBG("qudp_ingress_port->num_udp_fltr_entries = %u\n",qudp_ingress_port->num_udp_fltr_entries);

		if(cfg_action == CONFIGURE){
			if(qudp_ingress_port->num_udp_fltr_entries >= MAX_WHITELIST_ENTRIES){
				ECPRILOGERR("Cannot apply new UDP_CLASSIFICATION filter, Capacity is full\n");
				return false;
			}
		}
		if(cfg_action == DE_CONFIGURE){
			if(qudp_ingress_port->num_udp_fltr_entries <= 0){
				ECPRILOGERR("Cannot remove UDP_CLASSIFICATION filter, No filter exist\n");
				return false;
			}
		}

		/*
		 * Check if the UDP_CLASSIFICATION filter already exist
		 */
		for(i = 0; i< MAX_WHITELIST_ENTRIES; i++){
			if(udp_classification_port.value == qudp_ingress_port->udp_port[i]){
				if(cfg_action == CONFIGURE){
					ECPRILOGERR("Cannot apply UDP_CLASSIFICATION filter :%u, Already exist\n",udp_classification_port.value);
					return false;
				}else{
					ECPRILOGERR("Filter exist at index %u\n", i);
					filter_idx = i;
					break;
				}
			}
		}
		if(cfg_action == DE_CONFIGURE){
			if(filter_idx != i){
				ECPRILOGERR("VLAN filter does not exist\n");
				return false;
			}
		}else{
			/*
			 * look for available space in table
			 */
			ECPRILOGERR("Looking for available slots \n");
			for(i = 0; i< MAX_WHITELIST_ENTRIES; i++){
				if(0 == qudp_ingress_port->udp_port[i]){
					filter_idx = i;
					break;
				}
			}
			if(i != filter_idx){
				ECPRILOGERR("UDP filter full, can not cfg new filter\n");
				return false;
			}
		}

		/*
		 * UDP_classification value 0 is to remove last applied filter
		 * UDP_class id '0' can not be used as filter
		 * if filter exist remove the last applied filter
		 */
		if(cfg_action == DE_CONFIGURE){
			udp_classification_port.value = 0;
			rx_cfg->udp_dst_port = 0;
			if(qudp_ingress_port->num_udp_fltr_entries)
				qudp_ingress_port->num_udp_fltr_entries--;
		}

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_UDP_CLASSIFICATION_LIST_PORT_p_ENTRY_n,
				port_index,
				filter_idx,
				&udp_classification_port);

		qudp_ingress_port->udp_port[filter_idx] = udp_classification_port.value;

		if(cfg_action == CONFIGURE){
			qudp_ingress_port->num_udp_fltr_entries++;
		}
		ecpriss_qudp_ingress_modify_cfg(port_index,
				ENABLE_FILTER,
				ECPRISS_QUDP_RX_CFG_FLTR_MASK_UDP_DPORT,
				filter_idx,
				cfg_action);
		ECPRILOGDBG("UDP filter_idx = %u\n", filter_idx);
		ECPRILOGDBG("P_Data qudp_ingress_port->udp_port[filter_idx] = %u\n", qudp_ingress_port->udp_port[filter_idx]);
		ECPRILOGDBG("qudp_ingress_port->num_udp_fltr_entries = %u\n",qudp_ingress_port->num_udp_fltr_entries);

	}
	return true;
}

/**
 *  ecpriss_qudp_fh_rx_filter_cfg()
 *
 *  Port index provided as part of qudp_rx_cfg struct ?
 *  Description
 *
 *  Configure IP destination filter:
 *  FILT_IP_DST_ADDR0_PORT_p_ENTRY_n
 *  FILT_IP_DST_ADDR1_PORT_p_ENTRY_n
 *  FILT_IP_DST_ADDR3_PORT_p_ENTRY_n
 *  FILT_IP_DST_ADDR4_PORT_p_ENTRY_n

 *  Configure VLAN filter
 *  FILT_VLAN_ADDR_PORT_p_ENTRY_n
 *
 *  Configure UDP filter
 *  UDP_CLASSIFICATION_LIST_PORT_p_ENTRY_n
 *
 *  Maintain next qudp entry n for port p
 *
 *  Set valid bits
 *
 *  FILT_IP_DST_ADDR_PORT_p_ENTRIES_VALID_BITS
 *  FILT_VLAN_ADDR_PORT_p_ENTRIES_VALID_BITS
 *  UDP_CLASSIFICATION_LIST_PORT_p_ENTRIES_VALID_BITS
 *
 *
 *  Enable sharing of whitelist (Required ?)
 *
 *	INGRESS_CONFIG_n::ENABLE_SHARED_FILTERING_2_LINKS
 INGRESS_CONFIG_n::ENABLE_SHARED_FILTERING_4_LINKS
 *
 *  Enable filtering
 *
 *  INGRESS_CONFIG_n::ENABLE_IP_DST_FILT
 *  INGRESS_CONFIG_n::ENABLE_UDP_DST_CLASS
 *  INGRESS_CONFIG_n::ENABLE_VLAN_FILT
 * Returns:	0 on success, negative on failure
 */
int ecpriss_qudp_fh_rx_filter_cfg(uint32_t port_index, ecpriss_qudp_rx_cfg_s *rx_cfg)
{
	bool ret = false;
	if(ecpriss_filtering_enabled){
		ret = ecpriss_qudp_fh_rx_ip_filter_cfg(port_index, rx_cfg);
		if(ret == false){
			ECPRILOGERR("IP filter Config validation failed \n");
		}
	}else{
		ECPRILOGERR("Global filter config is disabled, Can't config IP filter \n");
	}
	if(ecpriss_filtering_enabled){
		ret = ecpriss_qudp_fh_rx_vlan_filter_cfg(port_index, rx_cfg);
		if(ret == false){
			ECPRILOGERR("Vlan filter config validation failed \n");
		}
	}else{
		ECPRILOGERR("Global filter config is disabled, Can't config VLAN filter \n");
	}
	ret = ecpriss_qudp_fh_rx_udp_filter_cfg(port_index, rx_cfg);
	if(ret == false){
		ECPRILOGERR("UDP filter config validation failed \n");
	}

	return 0;
}

static bool ecpriss_qudp_fh_rx_ip_filter_cfg_v2(uint32_t port_index, ecpriss_qudp_rx_cfg_s *rx_cfg)
{

	uint8_t filter_idx = 0;
	uint8_t cfg_action = DE_CONFIGURE;
	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port =
		&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].ingress_port_cfg;

	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr0_port_p_entry_n_s_v2 dst_ip0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr1_port_p_entry_n_s_v2 dst_ip1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr2_port_p_entry_n_s_v2 dst_ip2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr3_port_p_entry_n_s_v2 dst_ip3 = {0};

	if(rx_cfg == NULL) {
		return false;
	}

	if(rx_cfg->fltr_en_mask & ECPRISS_QUDP_RX_CFG_FLTR_MASK_IP_DADDR){
		cfg_action = CONFIGURE;
	}
	if(cfg_action == CONFIGURE){
		if(qudp_ingress_port->num_ip_fltr_entries >= MAX_WHITELIST_ENTRIES){
			ECPRILOGERR("Cannot apply new IP filter, Capacity is full\n");
			return false;
		}
	}
	if(cfg_action == DE_CONFIGURE){
		if(qudp_ingress_port->num_ip_fltr_entries <= 0){
			ECPRILOGINFO("Cannot Remove IP filter, No filter exist\n");
			return false;
		}
	}
	dst_ip0.value = ((rx_cfg->ip_dst_addr[3]) | (rx_cfg->ip_dst_addr[2] << 8) | (rx_cfg->ip_dst_addr[1] << 16)
			| (rx_cfg->ip_dst_addr[0] << 24));

	if(rx_cfg->ip_type == ECPRISS_IPV6_TYPE)
	{
		dst_ip1.value = ((rx_cfg->ip_dst_addr[7]) | (rx_cfg->ip_dst_addr[6] << 8) | (rx_cfg->ip_dst_addr[5] << 16)
				| (rx_cfg->ip_dst_addr[4] << 24));

		dst_ip2.value = ((rx_cfg->ip_dst_addr[11]) | (rx_cfg->ip_dst_addr[10] << 8) | (rx_cfg->ip_dst_addr[9] << 16)
				| (rx_cfg->ip_dst_addr[8] << 24));

		dst_ip3.value = ((rx_cfg->ip_dst_addr[15]) | (rx_cfg->ip_dst_addr[14] << 8) | (rx_cfg->ip_dst_addr[13] << 16)
				| (rx_cfg->ip_dst_addr[12] << 24));
	}
	if(cfg_action == DE_CONFIGURE){
		if((dst_ip0.value + dst_ip1.value + dst_ip2.value + dst_ip3.value ) == 0){
			ECPRILOGERR("Invalid ip Config \n");
			return false;
		}
	}
	/*
	 * Check if the IP filter already exist
	 */
	if(ecpriss_filtering_enabled){
		if(ecpriss_qudp_fh_rx_is_ip_filter_exist_v2(dst_ip0, dst_ip1 , dst_ip2, dst_ip3, &filter_idx, port_index, rx_cfg->ip_type)){
			ECPRILOGINFO("IP Filter exist in database \n");
			if(cfg_action == CONFIGURE){
				ECPRILOGINFO("Cannot apply IP filter, Already exist\n");
				return false;
			}
		}else{
			ECPRILOGINFO("IP Filter Doesn't exist in database \n");
			if(cfg_action == CONFIGURE){
				if(!ecpriss_qudp_fh_rx_get_available_ipfilter_idx_v2(&filter_idx, port_index, rx_cfg->ip_type)){
					ECPRILOGERR("Ip filter configuration is full, Can not apply new ip filtr config\n");
					return false;
				}

			}else{
				ECPRILOGINFO("Cannot remove IP filter, does not exist\n");
				return false;
			}
		}
	}
	/*
	 * if ecpriss_filtering_enabled is set or it is One
	 * remove the matching filter
	 */
	if(ecpriss_filtering_enabled == 1){
		/*
		 * remove filters
		 */
		if(cfg_action == DE_CONFIGURE){
			ECPRILOGINFO("forcing IP filter to zero to remove filter \n");
			dst_ip0.value = 0;
			dst_ip1.value = 0;
			dst_ip2.value = 0;
			dst_ip3.value = 0;
			if(qudp_ingress_port->num_ip_fltr_entries)
				qudp_ingress_port->num_ip_fltr_entries--;
		}
	}

	ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
			ECPRI_UDP_FH_FILT_IP_DST_ADDR0_PORT_p_ENTRY_n_V2,
			port_index,
			filter_idx,
			&dst_ip0);

	qudp_ingress_port->ipdst_addr[filter_idx][0] = dst_ip0.value ;


	if(rx_cfg->ip_type == ECPRISS_IPV6_TYPE)
	{

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_FILT_IP_DST_ADDR1_PORT_p_ENTRY_n_V2,
				port_index,
				filter_idx,
				&dst_ip1);

		qudp_ingress_port->ipdst_addr[filter_idx][1] = dst_ip1.value ;

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_FILT_IP_DST_ADDR2_PORT_p_ENTRY_n_V2,
				port_index,
				filter_idx,
				&dst_ip2);

		qudp_ingress_port->ipdst_addr[filter_idx][2] = dst_ip2.value ;

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_FILT_IP_DST_ADDR3_PORT_p_ENTRY_n_V2,
				port_index,
				filter_idx,
				&dst_ip3);

		qudp_ingress_port->ipdst_addr[filter_idx][3] = dst_ip3.value ;

	}

	if(ecpriss_filtering_enabled != 0 && cfg_action == CONFIGURE){
		qudp_ingress_port->num_ip_fltr_entries++;
	}
	ecpriss_qudp_ingress_modify_cfg_v2(port_index,
			ENABLE_FILTER,
			ECPRISS_QUDP_RX_CFG_FLTR_MASK_IP_DADDR,
			filter_idx,
			cfg_action);

	return true;
}
static bool ecpriss_qudp_fh_rx_vlan_filter_cfg_v2(uint32_t port_index, ecpriss_qudp_rx_cfg_s *rx_cfg)
{

	int i = 0;
	uint8_t filter_idx = 0;
	uint8_t cfg_action = DE_CONFIGURE;
	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port =
		&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].ingress_port_cfg;

	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_vlan_addr_port_p_entry_n_s_v2 vlan_addr_port = {0};

	if(rx_cfg == NULL) {
		return false;
	}
	if(rx_cfg->vlan_addr_port > 0)
	{
		filter_idx = 0;
		cfg_action = DE_CONFIGURE;
		if(rx_cfg->fltr_en_mask & ECPRISS_QUDP_RX_CFG_FLTR_MASK_VLAN){
			cfg_action = CONFIGURE;
		}
		if(cfg_action == CONFIGURE){
			if(qudp_ingress_port->num_vlan_fltr_entries >= MAX_WHITELIST_ENTRIES){
				ECPRILOGERR("Cannot apply new VLAN filter, Capacity is full\n");
				return false;
			}
		}
		if(cfg_action == DE_CONFIGURE){
			if(qudp_ingress_port->num_vlan_fltr_entries <= 0){
				ECPRILOGINFO("Cannot remove VLAN filter, No filter exist\n");
				return false;
			}
		}

		/*
		 * Check if the VLAN filter already exist
		 */
		if(ecpriss_filtering_enabled){
			for(i = 0; i< MAX_WHITELIST_ENTRIES; i++){
				if(rx_cfg->vlan_addr_port == qudp_ingress_port->vlan_addr[i]){
					if(cfg_action == CONFIGURE){
						ECPRILOGINFO("Cannot apply VLAN filter :%u, Already exist\n",rx_cfg->vlan_addr_port);
						return false;
					}else{
						ECPRILOGINFO("Filter exist at index %u\n", i);
						filter_idx = i;
						break;
					}
				}
			}

			if(cfg_action == DE_CONFIGURE){
				if(filter_idx != i){
					ECPRILOGINFO("VLAN filter does not exist\n");
					return false;
				}
			}else{
				/*
				 * look for available space in table
				 */
				ECPRILOGINFO("Looking for available slot in table\n");
				for(i = 0; i< MAX_WHITELIST_ENTRIES; i++){
					if(0 == qudp_ingress_port->vlan_addr[i]){
						filter_idx = i;
						break;
					}
				}
				if(i != filter_idx){
					ECPRILOGERR("VLAN filter full, can not cfg new filter\n");
					return false;
				}

			}
		}
		/*
		 * remove the last applied filter
		 */
		if(ecpriss_filtering_enabled == 1){
			if(cfg_action == DE_CONFIGURE){
				ECPRILOGINFO("forcing vlan id to zero to remove filter \n");
				rx_cfg->vlan_addr_port = 0;
				if(qudp_ingress_port->num_vlan_fltr_entries)
					qudp_ingress_port->num_vlan_fltr_entries--;
			}
		}
		vlan_addr_port.value = rx_cfg->vlan_addr_port;

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_FILT_VLAN_ADDR_PORT_p_ENTRY_n_V2,
				port_index,
				filter_idx,
				&vlan_addr_port);

		qudp_ingress_port->vlan_addr[filter_idx] = rx_cfg->vlan_addr_port;


		if(ecpriss_filtering_enabled != 0 && cfg_action == CONFIGURE){
			qudp_ingress_port->num_vlan_fltr_entries++;
		}
		ecpriss_qudp_ingress_modify_cfg_v2(port_index,
				ENABLE_FILTER,
				ECPRISS_QUDP_RX_CFG_FLTR_MASK_VLAN,
				filter_idx,
				cfg_action);

	}
	return true;
}
static bool ecpriss_qudp_fh_rx_udp_filter_cfg_v2(uint32_t port_index, ecpriss_qudp_rx_cfg_s *rx_cfg)
{

	int i = 0;
	uint8_t filter_idx = 0;
	uint8_t cfg_action = DE_CONFIGURE;
	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port =
		&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].ingress_port_cfg;

	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_classification_list_port_p_entry_n_s_v2 udp_classification_port = {0};

	if(rx_cfg == NULL) {
		return false;
	}
	if(rx_cfg->udp_dst_port > 0)
	{
		if(rx_cfg->fltr_en_mask & ECPRISS_QUDP_RX_CFG_FLTR_MASK_UDP_DPORT){
			cfg_action = CONFIGURE;
		}

		udp_classification_port.value = rx_cfg->udp_dst_port;
		if(cfg_action == CONFIGURE){
			ECPRILOGDBG("configure UDP\n");
		}else{
			ECPRILOGDBG("De configure UDP\n");
		}
		ECPRILOGDBG("udp_classification_port.value = %u\n",udp_classification_port.value);
		ECPRILOGDBG("qudp_ingress_port->num_udp_fltr_entries = %u\n",qudp_ingress_port->num_udp_fltr_entries);

		if(cfg_action == CONFIGURE){
			if(qudp_ingress_port->num_udp_fltr_entries >= MAX_WHITELIST_ENTRIES){
				ECPRILOGERR("Cannot apply new UDP_CLASSIFICATION filter, Capacity is full\n");
				return false;
			}
		}
		if(cfg_action == DE_CONFIGURE){
			if(qudp_ingress_port->num_udp_fltr_entries <= 0){
				ECPRILOGINFO("Cannot remove UDP_CLASSIFICATION filter, No filter exist\n");
				return false;
			}
		}

		/*
		 * Check if the UDP_CLASSIFICATION filter already exist
		 */
		for(i = 0; i< MAX_WHITELIST_ENTRIES; i++){
			if(udp_classification_port.value == qudp_ingress_port->udp_port[i]){
				if(cfg_action == CONFIGURE){
					ECPRILOGINFO("Cannot apply UDP_CLASSIFICATION filter :%u, Already exist\n",udp_classification_port.value);
					return false;
				}else{
					ECPRILOGINFO("Filter exist at index %u\n", i);
					filter_idx = i;
					break;
				}
			}
		}
		if(cfg_action == DE_CONFIGURE){
			if(filter_idx != i){
				ECPRILOGINFO("UDP filter does not exist\n");
				return false;
			}
		}else{
			/*
			 * look for available space in table
			 */
			ECPRILOGINFO("Looking for available slots \n");
			for(i = 0; i< MAX_WHITELIST_ENTRIES; i++){
				if(0 == qudp_ingress_port->udp_port[i]){
					filter_idx = i;
					break;
				}
			}
			if(i != filter_idx){
				ECPRILOGERR("UDP filter full, can not cfg new filter\n");
				return false;
			}
		}

		/*
		 * UDP_classification value 0 is to remove last applied filter
		 * UDP_class id '0' can not be used as filter
		 * if filter exist remove the last applied filter
		 */
		if(cfg_action == DE_CONFIGURE){
			udp_classification_port.value = 0;
			rx_cfg->udp_dst_port = 0;
			if(qudp_ingress_port->num_udp_fltr_entries)
				qudp_ingress_port->num_udp_fltr_entries--;
		}

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_UDP_CLASSIFICATION_LIST_PORT_p_ENTRY_n_V2,
				port_index,
				filter_idx,
				&udp_classification_port);

		qudp_ingress_port->udp_port[filter_idx] = udp_classification_port.value;

		if(cfg_action == CONFIGURE){
			qudp_ingress_port->num_udp_fltr_entries++;
		}
		ecpriss_qudp_ingress_modify_cfg_v2(port_index,
				ENABLE_FILTER,
				ECPRISS_QUDP_RX_CFG_FLTR_MASK_UDP_DPORT,
				filter_idx,
				cfg_action);
		ECPRILOGDBG("UDP filter_idx = %u\n", filter_idx);
		ECPRILOGDBG("P_Data qudp_ingress_port->udp_port[filter_idx] = %u\n", qudp_ingress_port->udp_port[filter_idx]);
		ECPRILOGDBG("qudp_ingress_port->num_udp_fltr_entries = %u\n",qudp_ingress_port->num_udp_fltr_entries);

	}
	return true;
}

int ecpriss_qudp_fh_rx_filter_cfg_v2(uint32_t port_index, ecpriss_qudp_rx_cfg_s *rx_cfg)
{
	bool ret = false;

	if(rx_cfg->fltr_en_mask & ECPRISS_QUDP_RX_CFG_FLTR_MASK_IP_DADDR){
		ret = ecpriss_qudp_fh_rx_ip_filter_cfg_v2(port_index, rx_cfg);
		if(ret == false){
			ECPRILOGERR("IP filter Config validation failed \n");
		}
	}
	if(rx_cfg->fltr_en_mask & ECPRISS_QUDP_RX_CFG_FLTR_MASK_VLAN){
		ret = ecpriss_qudp_fh_rx_vlan_filter_cfg_v2(port_index, rx_cfg);
		if(ret == false){
			ECPRILOGERR("Vlan filter config validation failed \n");
		}
	}
	if(rx_cfg->fltr_en_mask & ECPRISS_QUDP_RX_CFG_FLTR_MASK_UDP_DPORT){
		ret = ecpriss_qudp_fh_rx_udp_filter_cfg_v2(port_index, rx_cfg);
		if(ret == false){
			ECPRILOGERR("UDP filter config validation failed \n");
		}
	}

	return 0;
}


static bool ecpriss_qudp_fh_rx_remove_ip_filter_cfg_v2(uint32_t port_index, ecpriss_qudp_rx_cfg_s *rx_cfg)
{
	uint8_t filter_idx = 0;
	uint8_t cfg_action = DE_CONFIGURE;
	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port =
		&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].ingress_port_cfg;

	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr0_port_p_entry_n_s_v2 dst_ip0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr1_port_p_entry_n_s_v2 dst_ip1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr2_port_p_entry_n_s_v2 dst_ip2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_ip_dst_addr3_port_p_entry_n_s_v2 dst_ip3 = {0};

	if(qudp_ingress_port == NULL)
		return false;

	if(rx_cfg == NULL) {
		return false;
	}

	if(qudp_ingress_port->num_ip_fltr_entries <= 0){
		ECPRILOGINFO("Cannot Remove IP filter, No filter exist\n");
		return true;
	}

	dst_ip0.value = ((rx_cfg->ip_dst_addr[3]) | (rx_cfg->ip_dst_addr[2] << 8) | (rx_cfg->ip_dst_addr[1] << 16)
			| (rx_cfg->ip_dst_addr[0] << 24));

	if(rx_cfg->ip_type == ECPRISS_IPV6_TYPE)
	{
		dst_ip1.value = ((rx_cfg->ip_dst_addr[7]) | (rx_cfg->ip_dst_addr[6] << 8) | (rx_cfg->ip_dst_addr[5] << 16)
				| (rx_cfg->ip_dst_addr[4] << 24));

		dst_ip2.value = ((rx_cfg->ip_dst_addr[11]) | (rx_cfg->ip_dst_addr[10] << 8) | (rx_cfg->ip_dst_addr[9] << 16)
				| (rx_cfg->ip_dst_addr[8] << 24));

		dst_ip3.value = ((rx_cfg->ip_dst_addr[15]) | (rx_cfg->ip_dst_addr[14] << 8) | (rx_cfg->ip_dst_addr[13] << 16)
				| (rx_cfg->ip_dst_addr[12] << 24));
	}

	if((dst_ip0.value + dst_ip1.value + dst_ip2.value + dst_ip3.value ) == 0){
		ECPRILOGERR("Invalid ip Config \n");
		return false;
	}
	/*
	 * Check if the IP filter exist
	 */
	if(ecpriss_filtering_enabled){
		if(ecpriss_qudp_fh_rx_is_ip_filter_exist_v2(dst_ip0, dst_ip1 , dst_ip2, dst_ip3, &filter_idx, port_index, rx_cfg->ip_type)){
				ECPRILOGINFO("IP filter exist and forcing IP filter to zero to remove filter \n");
				dst_ip0.value = 0;
				dst_ip1.value = 0;
				dst_ip2.value = 0;
				dst_ip3.value = 0;
				if(qudp_ingress_port->num_ip_fltr_entries)
					qudp_ingress_port->num_ip_fltr_entries--;

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
						ECPRI_UDP_FH_FILT_IP_DST_ADDR0_PORT_p_ENTRY_n_V2,
						port_index,
						filter_idx,
						&dst_ip0);

				qudp_ingress_port->ipdst_addr[filter_idx][0] = dst_ip0.value ;


				if(rx_cfg->ip_type == ECPRISS_IPV6_TYPE) {

					ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
							ECPRI_UDP_FH_FILT_IP_DST_ADDR1_PORT_p_ENTRY_n_V2,
							port_index,
							filter_idx,
							&dst_ip1);

					qudp_ingress_port->ipdst_addr[filter_idx][1] = dst_ip1.value ;

					ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
							ECPRI_UDP_FH_FILT_IP_DST_ADDR2_PORT_p_ENTRY_n_V2,
							port_index,
							filter_idx,
							&dst_ip2);

					qudp_ingress_port->ipdst_addr[filter_idx][2] = dst_ip2.value ;

					ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
							ECPRI_UDP_FH_FILT_IP_DST_ADDR3_PORT_p_ENTRY_n_V2,
							port_index,
							filter_idx,
							&dst_ip3);

					qudp_ingress_port->ipdst_addr[filter_idx][3] = dst_ip3.value ;

				}
			}else{
				ECPRILOGINFO("IP filter does not exist in the database \n");
			}
	}else{
		ECPRILOGINFO("Cannot remove IP filter, does not exist\n");
		return false;
	}
	ecpriss_qudp_ingress_modify_cfg_v2(port_index,
			ENABLE_FILTER,
			ECPRISS_QUDP_RX_CFG_FLTR_MASK_IP_DADDR,
			filter_idx,
			cfg_action);

	return true;
}


static bool ecpriss_qudp_fh_rx_remove_vlan_filter_cfg_v2(uint32_t port_index, ecpriss_qudp_rx_cfg_s *rx_cfg)
{

	int i = 0;
	uint8_t filter_idx = 0;
	uint8_t cfg_action = DE_CONFIGURE;
	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port =
		&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].ingress_port_cfg;

	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_vlan_addr_port_p_entry_n_s_v2 vlan_addr_port = {0};

	if(qudp_ingress_port == NULL)
		return false;

	if(rx_cfg == NULL) {
		return false;
	}
	if(rx_cfg->vlan_addr_port > 0)
	{
		if(qudp_ingress_port->num_vlan_fltr_entries <= 0){
			ECPRILOGINFO("Cannot remove VLAN filter, No filter exist\n");
			return true;
		}

		/*
		 * Check if the VLAN filter already exist
		 */
		if(ecpriss_filtering_enabled){
			for(i = 0; i< MAX_WHITELIST_ENTRIES; i++){
				if(rx_cfg->vlan_addr_port == qudp_ingress_port->vlan_addr[i]){
						ECPRILOGINFO("Filter exist at index %u\n", i);
						filter_idx = i;
						break;
				}
			}

			if(filter_idx != i){
				ECPRILOGINFO("VLAN filter does not exist\n");
				return false;
			}
		}
		/*
		 * remove the last applied filter
		 */
		if(ecpriss_filtering_enabled == 1){
			ECPRILOGINFO("forcing vlan id to zero to remove filter \n");
			rx_cfg->vlan_addr_port = 0;
			if(qudp_ingress_port->num_vlan_fltr_entries)
				qudp_ingress_port->num_vlan_fltr_entries--;
		}
		vlan_addr_port.value = rx_cfg->vlan_addr_port;

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_FILT_VLAN_ADDR_PORT_p_ENTRY_n_V2,
				port_index,
				filter_idx,
				&vlan_addr_port);

		qudp_ingress_port->vlan_addr[filter_idx] = rx_cfg->vlan_addr_port;


		ecpriss_qudp_ingress_modify_cfg_v2(port_index,
				ENABLE_FILTER,
				ECPRISS_QUDP_RX_CFG_FLTR_MASK_VLAN,
				filter_idx,
				cfg_action);

	}
	return true;
}

static bool ecpriss_qudp_fh_rx_remove_udp_filter_cfg_v2(uint32_t port_index, ecpriss_qudp_rx_cfg_s *rx_cfg)
{

	int i = 0;
	uint8_t filter_idx = 0;
	uint8_t cfg_action = DE_CONFIGURE;
	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port =
		&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].ingress_port_cfg;

	ecpri_qudp_hwio_def_ecpri_udp_fh_udp_classification_list_port_p_entry_n_s_v2 udp_classification_port = {0};

	if(qudp_ingress_port == NULL)
		return false;

	if(rx_cfg == NULL) {
		return false;
	}
	if(rx_cfg->udp_dst_port > 0)
	{

		udp_classification_port.value = rx_cfg->udp_dst_port;
		ECPRILOGDBG("udp_classification_port.value = %u\n",udp_classification_port.value);
		ECPRILOGDBG("qudp_ingress_port->num_udp_fltr_entries = %u\n",qudp_ingress_port->num_udp_fltr_entries);

		if(qudp_ingress_port->num_udp_fltr_entries <= 0){
			ECPRILOGINFO("Cannot remove UDP_CLASSIFICATION filter, No filter exist\n");
			return true;
		}

		/*
		 * Check if the UDP_CLASSIFICATION filter already exist
		 */
		for(i = 0; i< MAX_WHITELIST_ENTRIES; i++){
			if(udp_classification_port.value == qudp_ingress_port->udp_port[i]){
					ECPRILOGINFO("Filter exist at index %u\n", i);
					filter_idx = i;
					break;
				}
			}
		if(filter_idx != i){
			ECPRILOGINFO("UDP filter does not exist\n");
			return false;
		}

		/*
		 * UDP_classification value 0 is to remove last applied filter
		 * UDP_class id '0' can not be used as filter
		 * if filter exist remove the last applied filter
		 */
		udp_classification_port.value = 0;
		rx_cfg->udp_dst_port = 0;
		if(qudp_ingress_port->num_udp_fltr_entries)
			qudp_ingress_port->num_udp_fltr_entries--;

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_UDP_CLASSIFICATION_LIST_PORT_p_ENTRY_n_V2,
				port_index,
				filter_idx,
				&udp_classification_port);

		qudp_ingress_port->udp_port[filter_idx] = udp_classification_port.value;

		ecpriss_qudp_ingress_modify_cfg_v2(port_index,
				ENABLE_FILTER,
				ECPRISS_QUDP_RX_CFG_FLTR_MASK_UDP_DPORT,
				filter_idx,
				cfg_action);
		ECPRILOGDBG("qudp_ingress_port udp filter removed\n");
		ECPRILOGDBG("qudp_ingress_port->num_udp_fltr_entries = %u\n",qudp_ingress_port->num_udp_fltr_entries);

	}
	return true;
}

int32_t ecpriss_qudp_fh_rx_filter_decfg_v2(uint32_t port_index, ecpriss_qudp_rx_cfg_s *rx_cfg)
{
	int32_t ret = 0;

	if(port_index < 0 || port_index > ECPRISS_PORT_MAX) {
		ECPRILOGERR("Invalid Port_index\n");
		return -1;
	}

	ret = ecpriss_qudp_fh_rx_remove_ip_filter_cfg_v2(port_index, rx_cfg);
	if(ret == false){
		ECPRILOGERR("IP filter de-Config failed \n");
		ret = -1;
	}

	ret = ecpriss_qudp_fh_rx_remove_vlan_filter_cfg_v2(port_index, rx_cfg);
	if(ret == false){
		ECPRILOGERR("Vlan filter de-config failed \n");
		ret = -1;
	}

	ret = ecpriss_qudp_fh_rx_remove_udp_filter_cfg_v2(port_index, rx_cfg);
	if(ret == false){
		ECPRILOGERR("UDP filter de-config failed \n");
		ret = -1;
	}

	return ret;
}



/**
 *  ecpriss_qudp_fh_tx_hdr_ins_cfg
 *
 *	Requirements - port index and maintain a table of entries
 *
 *	Description -
 *
 *
 *
 * Returns:	0 on success, negative on failure
 */
int ecpriss_qudp_fh_tx_hdr_ins_cfg(uint32_t               port_index,
		ecpriss_qudp_tx_cfg_s *tx_cfg)
{
	int ret = 0;

	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_dst0_port_p_entry_n_s       eth_dst0_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src1_dst1_port_p_entry_n_s  eth_src1_dst1_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src0_port_p_entry_n_s       eth_src0_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_vlan_ethertype_port_p_entry_n_s vlan_ethertype_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_vport_misc_port_p_entry_n_s     vport_misc_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr0_port_p_entry_n_s   ip_src0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr1_port_p_entry_n_s   ip_src1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr2_port_p_entry_n_s   ip_src2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr3_port_p_entry_n_s   ip_src3 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr0_port_p_entry_n_s   ip_dst0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr1_port_p_entry_n_s   ip_dst1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr2_port_p_entry_n_s   ip_dst2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr3_port_p_entry_n_s   ip_dst3 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_udp_ports_port_p_entry_n_s      udp_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_sa_tag_ip_tos_misc_port_p_entry_n_s  ip_opts = {0};


	do
	{
		if(tx_cfg == NULL) {
			ret = -ENOMEM;
			break;
		}

		/* 4 LSB goes to this eth_dst0_port */

		eth_dst0_port.value = ((tx_cfg->eth_hdr.dst_mac_addr[5]) | (tx_cfg->eth_hdr.dst_mac_addr[4] << 8)
				| (tx_cfg->eth_hdr.dst_mac_addr[3] << 16) | (tx_cfg->eth_hdr.dst_mac_addr[2] << 24));



		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_DST0_PORT_p_ENTRY_n,
				port_index,
				tx_cfg->l2_hdr_tbl_idx,
				&eth_dst0_port);

		eth_src1_dst1_port.dst_msb = ((tx_cfg->eth_hdr.dst_mac_addr[1]) | (tx_cfg->eth_hdr.dst_mac_addr[0] << 8));
		eth_src1_dst1_port.src_msb = ((tx_cfg->eth_hdr.src_mac_addr[1]) | (tx_cfg->eth_hdr.src_mac_addr[0] << 8));


		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_SRC1_DST1_PORT_p_ENTRY_n,
				port_index,
				tx_cfg->l2_hdr_tbl_idx,
				&eth_src1_dst1_port);



		eth_src0_port.value = ((tx_cfg->eth_hdr.src_mac_addr[5]) | (tx_cfg->eth_hdr.src_mac_addr[4] << 8)
				| (tx_cfg->eth_hdr.src_mac_addr[3] << 16) | (tx_cfg->eth_hdr.src_mac_addr[2]) << 24);



		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_SRC0_PORT_p_ENTRY_n,
				port_index,
				tx_cfg->l2_hdr_tbl_idx,
				&eth_src0_port);

		vlan_ethertype_port.ethertype = tx_cfg->eth_hdr.orig_ethertype;

		vlan_ethertype_port.vlan_data = tx_cfg->eth_hdr.vlan_data;

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_VLAN_ETHERTYPE_PORT_p_ENTRY_n,
				port_index,
				tx_cfg->l2_hdr_tbl_idx,
				&vlan_ethertype_port);

		vport_misc_port.vport = tx_cfg->eth_hdr.vport;

		vport_misc_port.has_vlan = tx_cfg->eth_hdr.is_vlan;

		vport_misc_port.vport_action = tx_cfg->eth_hdr.vport_action;

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_VPORT_MISC_PORT_p_ENTRY_n,
				port_index,
				tx_cfg->l2_hdr_tbl_idx,
				&vport_misc_port);

		if(tx_cfg->eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

			ip_src0.value |= ((tx_cfg->ip_hdr.src_ip_addr[3]) | (tx_cfg->ip_hdr.src_ip_addr[2] << 8) | (tx_cfg->ip_hdr.src_ip_addr[1] << 16)
					| (tx_cfg->ip_hdr.src_ip_addr[0] << 24));

			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
					ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR0_PORT_p_ENTRY_n,
					port_index,
					tx_cfg->l3_hdr_tbl_idx,
					&ip_src0);

			ip_dst0.value |= ((tx_cfg->ip_hdr.dst_ip_addr[3]) | (tx_cfg->ip_hdr.dst_ip_addr[2] << 8) | (tx_cfg->ip_hdr.dst_ip_addr[1] << 16)
					| (tx_cfg->ip_hdr.dst_ip_addr[0] << 24));

			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
					ECPRI_UDP_FH_EGRESS_IP_DST_ADDR0_PORT_p_ENTRY_n,
					port_index,
					tx_cfg->l3_hdr_tbl_idx,
					&ip_dst0);

			if(tx_cfg->ip_hdr.ip_type == ECPRISS_IPV6_TYPE)
			{

				ip_src1.value |= ((tx_cfg->ip_hdr.src_ip_addr[7]) | (tx_cfg->ip_hdr.src_ip_addr[6] << 8) | (tx_cfg->ip_hdr.src_ip_addr[5] << 16)
						| (tx_cfg->ip_hdr.src_ip_addr[4] << 24));

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR1_PORT_p_ENTRY_n,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_src1);

				ip_src2.value |= ((tx_cfg->ip_hdr.src_ip_addr[11]) | (tx_cfg->ip_hdr.src_ip_addr[10] << 8) | (tx_cfg->ip_hdr.src_ip_addr[9] << 16)
						| (tx_cfg->ip_hdr.src_ip_addr[8] << 24));

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR2_PORT_p_ENTRY_n,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_src2);

				ip_src3.value |= ((tx_cfg->ip_hdr.src_ip_addr[15]) | (tx_cfg->ip_hdr.src_ip_addr[14] << 8) | (tx_cfg->ip_hdr.src_ip_addr[13] << 16)
						| (tx_cfg->ip_hdr.src_ip_addr[12] << 24));

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR3_PORT_p_ENTRY_n,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_src3);

				ip_dst1.value |= ((tx_cfg->ip_hdr.dst_ip_addr[7]) | (tx_cfg->ip_hdr.dst_ip_addr[6] << 8) | (tx_cfg->ip_hdr.dst_ip_addr[5] << 16)
						| (tx_cfg->ip_hdr.dst_ip_addr[4] << 24));

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_IP_DST_ADDR1_PORT_p_ENTRY_n,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_dst1);

				ip_dst2.value |= ((tx_cfg->ip_hdr.dst_ip_addr[11]) | (tx_cfg->ip_hdr.dst_ip_addr[10] << 8) | (tx_cfg->ip_hdr.dst_ip_addr[9] << 16)
						| (tx_cfg->ip_hdr.dst_ip_addr[8] << 24));

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_IP_DST_ADDR2_PORT_p_ENTRY_n,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_dst2);

				ip_dst3.value |= ((tx_cfg->ip_hdr.dst_ip_addr[15]) | (tx_cfg->ip_hdr.dst_ip_addr[14] << 8) | (tx_cfg->ip_hdr.dst_ip_addr[13] << 16)
						| (tx_cfg->ip_hdr.dst_ip_addr[12] << 24));

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_IP_DST_ADDR3_PORT_p_ENTRY_n,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_dst3);

			}

			udp_port.src = tx_cfg->ip_hdr.src_udp_port;
			udp_port.dst = tx_cfg->ip_hdr.dst_udp_port;

			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
					ECPRI_UDP_FH_EGRESS_UDP_PORTS_PORT_p_ENTRY_n,
					port_index,
					tx_cfg->l3_hdr_tbl_idx,
					&udp_port);


			ip_opts.sa_tag_data = tx_cfg->ip_hdr.sa_tag_data ;
			ip_opts.tos = tx_cfg->ip_hdr.tos;
			ip_opts.df_bit = tx_cfg->ip_hdr.df_en;
			ip_opts.calc_udp_cs = tx_cfg->ip_hdr.udp_chksum_en;
			ip_opts.is_ipsec = tx_cfg->ip_hdr.ipsec_en;
			ip_opts.rsvd = tx_cfg->ip_hdr.rsvd;
			ip_opts.ip_type = tx_cfg->ip_hdr.ip_type;

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_SA_TAG_IP_TOS_MISC_PORT_p_ENTRY_n,
				port_index,
				tx_cfg->l3_hdr_tbl_idx,
				&ip_opts);
		}

	}while(0);
	return 0;
}

int ecpriss_qudp_fh_egress_cfg_reset(int32_t port_index)
{
	int32_t egress_table_index = 0;

	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_dst0_port_p_entry_n_s       eth_dst0_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src1_dst1_port_p_entry_n_s  eth_src1_dst1_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src0_port_p_entry_n_s       eth_src0_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_vlan_ethertype_port_p_entry_n_s vlan_ethertype_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_vport_misc_port_p_entry_n_s     vport_misc_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr0_port_p_entry_n_s   ip_src0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr1_port_p_entry_n_s   ip_src1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr2_port_p_entry_n_s   ip_src2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr3_port_p_entry_n_s   ip_src3 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr0_port_p_entry_n_s   ip_dst0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr1_port_p_entry_n_s   ip_dst1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr2_port_p_entry_n_s   ip_dst2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr3_port_p_entry_n_s   ip_dst3 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_udp_ports_port_p_entry_n_s      udp_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_sa_tag_ip_tos_misc_port_p_entry_n_s  ip_opts = {0};

	if(port_index > 3)
		return 0;

	for(egress_table_index = 0; egress_table_index < NUM_EGRESS_ENTRY ; egress_table_index++)
	{

		/* 4 LSB goes to this eth_dst0_port */

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_DST0_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&eth_dst0_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_SRC1_DST1_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&eth_src1_dst1_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_SRC0_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&eth_src0_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_VLAN_ETHERTYPE_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&vlan_ethertype_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_VPORT_MISC_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&vport_misc_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR0_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&ip_src0);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR0_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&ip_dst0);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR1_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&ip_src1);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR2_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&ip_src2);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR3_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&ip_src3);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR1_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&ip_dst1);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR2_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&ip_dst2);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR3_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&ip_dst3);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_UDP_PORTS_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&udp_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_SA_TAG_IP_TOS_MISC_PORT_p_ENTRY_n,
				port_index,
				egress_table_index,
				&ip_opts);

	}
	return 0;
}
int ecpriss_qudp_fh_egress_cfg_reset_v2(int32_t port_index)
{
	int32_t egress_table_index = 0;

	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_dst0_port_p_entry_n_s_v2       eth_dst0_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src1_dst1_port_p_entry_n_s_v2  eth_src1_dst1_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src0_port_p_entry_n_s_v2       eth_src0_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_vlan_ethertype_port_p_entry_n_s_v2 vlan_ethertype_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_vport_misc_port_p_entry_n_s_v2     vport_misc_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr0_port_p_entry_n_s_v2   ip_src0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr1_port_p_entry_n_s_v2   ip_src1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr2_port_p_entry_n_s_v2   ip_src2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr3_port_p_entry_n_s_v2   ip_src3 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr0_port_p_entry_n_s_v2   ip_dst0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr1_port_p_entry_n_s_v2   ip_dst1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr2_port_p_entry_n_s_v2   ip_dst2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr3_port_p_entry_n_s_v2   ip_dst3 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_udp_ports_port_p_entry_n_s_v2      udp_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_sa_tag_ip_tos_misc_port_p_entry_n_s_v2  ip_opts = {0};

	if(port_index > 3)
		return 0;

	for(egress_table_index = 0; egress_table_index < NUM_EGRESS_ENTRY ; egress_table_index++)
	{

		/* 4 LSB goes to this eth_dst0_port */

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_DST0_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&eth_dst0_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_SRC1_DST1_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&eth_src1_dst1_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_SRC0_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&eth_src0_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_VLAN_ETHERTYPE_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&vlan_ethertype_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_VPORT_MISC_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&vport_misc_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR0_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_src0);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR0_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_dst0);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR1_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_src1);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR2_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_src2);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR3_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_src3);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR1_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_dst1);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR2_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_dst2);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_IP_DST_ADDR3_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_dst3);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_UDP_PORTS_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&udp_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_SA_TAG_IP_TOS_MISC_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_opts);

	}
	return 0;
}
int ecpriss_qudp_l2_egress_cfg_reset_v2(int32_t port_index)
{
	int32_t egress_table_index = 0;

	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_eth_dst0_port_p_entry_n_s_v2       eth_dst0_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_eth_src1_dst1_port_p_entry_n_s_v2  eth_src1_dst1_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_eth_src0_port_p_entry_n_s_v2       eth_src0_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_vlan_ethertype_port_p_entry_n_s_v2 vlan_ethertype_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_vport_misc_port_p_entry_n_s_v2     vport_misc_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_ip_src_addr0_port_p_entry_n_s_v2   ip_src0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_ip_src_addr1_port_p_entry_n_s_v2   ip_src1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_ip_src_addr2_port_p_entry_n_s_v2   ip_src2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_ip_src_addr3_port_p_entry_n_s_v2   ip_src3 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_ip_dst_addr0_port_p_entry_n_s_v2   ip_dst0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_ip_dst_addr1_port_p_entry_n_s_v2   ip_dst1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_ip_dst_addr2_port_p_entry_n_s_v2   ip_dst2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_ip_dst_addr3_port_p_entry_n_s_v2   ip_dst3 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_udp_ports_port_p_entry_n_s_v2      udp_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_sa_tag_ip_tos_misc_port_p_entry_n_s_v2  ip_opts = {0};

	if(port_index > 0)
		return 0;

	for(egress_table_index = 0; egress_table_index < NUM_EGRESS_ENTRY ; egress_table_index++)
	{

		/* 4 LSB goes to this eth_dst0_port */

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_ETH_DST0_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&eth_dst0_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_ETH_SRC1_DST1_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&eth_src1_dst1_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_ETH_SRC0_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&eth_src0_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_VLAN_ETHERTYPE_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&vlan_ethertype_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_VPORT_MISC_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&vport_misc_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_IP_SRC_ADDR0_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_src0);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_IP_DST_ADDR0_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_dst0);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_IP_SRC_ADDR1_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_src1);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_IP_SRC_ADDR2_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_src2);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_IP_SRC_ADDR3_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_src3);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_IP_DST_ADDR1_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_dst1);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_IP_DST_ADDR2_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_dst2);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_IP_DST_ADDR3_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_dst3);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_UDP_PORTS_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&udp_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_SA_TAG_IP_TOS_MISC_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&ip_opts);

	}
	return 0;
}

int ecpriss_qudp_l2_egress_tp_cfg_v2(int32_t port_index)
{
	int32_t egress_table_index = 0;

	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_eth_dst0_port_p_entry_n_s_v2       eth_dst0_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_eth_src1_dst1_port_p_entry_n_s_v2  eth_src1_dst1_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_eth_src0_port_p_entry_n_s_v2       eth_src0_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_vlan_ethertype_port_p_entry_n_s_v2 vlan_ethertype_port = {0};

	if(port_index > 0)
		return 0;

	for(egress_table_index = 0; egress_table_index < NUM_EGRESS_ENTRY ; egress_table_index++)
	{

		/* 4 LSB goes to this eth_dst0_port */
		/* Adding Dummy transport header for L2 port */
		eth_dst0_port.value = 0x4E554C30;
		eth_src1_dst1_port.dst_msb = 0x0053;
		eth_src1_dst1_port.src_msb = 0x0053;
		eth_src0_port.value = 0x4E554C30;
		vlan_ethertype_port.ethertype = 0xAEFE;

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_ETH_DST0_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&eth_dst0_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_ETH_SRC1_DST1_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&eth_src1_dst1_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_ETH_SRC0_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&eth_src0_port);

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2_RAMS,
				ECPRI_UDP_L2_EGRESS_VLAN_ETHERTYPE_PORT_p_ENTRY_n_V2,
				port_index,
				egress_table_index,
				&vlan_ethertype_port);

	}
	return 0;
}

int ecpriss_qudp_get_ecpriss_filt_enable_info(void)
{
	return ecpriss_filtering_enabled;
}

void ecpriss_qudp_set_ecpriss_filt_enable_info(int val)
{
	ecpriss_filtering_enabled = val;
}
void ecpriss_qudp_non_ecpri_dma_ring_info(void)
{
	int32_t fh_index = 0;
	int32_t index = 0;
	int j =0;

	ecpri_qudp_hwio_def_ecpri_udp_fh_non_ecpri_dma_ring_info_port_p_link_n_s_v2 non_ecpri_dma_ring_info;
	ecpri_qudp_hwio_def_ecpri_udp_l2_non_ecpri_dma_ring_info_port_p_link_n_s_v2 c2c_non_ecpri_dma_ring_info;

	for(fh_index = 0; fh_index < NUM_OF_FHP; fh_index++){

		struct ecpri_dma_port_params *dma_port_cfg= &ecpriss_pdata_v2->xbar_ctx_v2->fh_port_cfg.dma_port_cfg[fh_index];

		for(j=0;j<dma_port_cfg->num_of_rings;j++)
		{
			memset(&non_ecpri_dma_ring_info , 0, sizeof(non_ecpri_dma_ring_info));

			if(dma_port_cfg->dma_rings_param[j].dma_ring_type == ECPRI_DMA_RING_TYPE_FH_DEFAULT)
			{
				switch(j)
				{
					case 0:
						non_ecpri_dma_ring_info.ring_id = dma_port_cfg->dma_rings_param[0].dest_dma_ring_id;
						non_ecpri_dma_ring_info.gsi_id= dma_port_cfg->dma_rings_param[0].dest_dma_ring_gsi_id;
							break;
					case 1:
						non_ecpri_dma_ring_info.ring_id = dma_port_cfg->dma_rings_param[1].dest_dma_ring_id;
						non_ecpri_dma_ring_info.gsi_id = dma_port_cfg->dma_rings_param[1].dest_dma_ring_gsi_id;
							break;
					case 2:
						non_ecpri_dma_ring_info.ring_id = dma_port_cfg->dma_rings_param[2].dest_dma_ring_id;
						non_ecpri_dma_ring_info.gsi_id = dma_port_cfg->dma_rings_param[2].dest_dma_ring_gsi_id;
							break;
					case 3:
						non_ecpri_dma_ring_info.ring_id = dma_port_cfg->dma_rings_param[3].dest_dma_ring_id;
						non_ecpri_dma_ring_info.gsi_id = dma_port_cfg->dma_rings_param[3].dest_dma_ring_gsi_id;
							break;
					default:
						ECPRILOGERR("Wrong default value %d\n",j);
						break;
				}
				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH,
						ECPRI_UDP_FH_NON_ECPRI_DMA_RING_INFO_PORT_p_LINK_n_V2,
						fh_index,j,
						&non_ecpri_dma_ring_info);


			}
		}

	}
	for(index = 0; index < 1; index++){

		struct ecpri_dma_port_params *dma_port_cfg= &ecpriss_pdata_v2->xbar_ctx_v2->c2c_port_cfg.dma_port_cfg[index];

		memset(&c2c_non_ecpri_dma_ring_info , 0, sizeof(c2c_non_ecpri_dma_ring_info));

		if(dma_port_cfg->dma_rings_param[0].dma_ring_type == ECPRI_DMA_RING_TYPE_C2C_DEFAULT)
		{
			c2c_non_ecpri_dma_ring_info.ring_id = dma_port_cfg->dma_rings_param[0].dest_dma_ring_id;
		        c2c_non_ecpri_dma_ring_info.gsi_id= dma_port_cfg->dma_rings_param[0].dest_dma_ring_gsi_id;
			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_L2,
				ECPRI_UDP_L2_NON_ECPRI_DMA_RING_INFO_PORT_p_LINK_n_V2,
				index,0,
				&c2c_non_ecpri_dma_ring_info);
		}

	}
}

int ecpriss_qudp_fh_tx_hdr_decfg_v2(uint32_t               port_index,
		ecpriss_qudp_tx_cfg_s *tx_cfg)
{
	int ret = 0;

	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_dst0_port_p_entry_n_s_v2       eth_dst0_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src1_dst1_port_p_entry_n_s_v2   eth_src1_dst1_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src0_port_p_entry_n_s_v2        eth_src0_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_vlan_ethertype_port_p_entry_n_s_v2  vlan_ethertype_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_vport_misc_port_p_entry_n_s_v2      vport_misc_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr0_port_p_entry_n_s_v2    ip_src0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr1_port_p_entry_n_s_v2    ip_src1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr2_port_p_entry_n_s_v2    ip_src2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr3_port_p_entry_n_s_v2    ip_src3 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr0_port_p_entry_n_s_v2    ip_dst0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr1_port_p_entry_n_s_v2    ip_dst1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr2_port_p_entry_n_s_v2    ip_dst2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr3_port_p_entry_n_s_v2    ip_dst3 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_udp_ports_port_p_entry_n_s_v2       udp_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_sa_tag_ip_tos_misc_port_p_entry_n_s_v2   ip_opts = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_ipv4_fields_p_s_v2 ipv4_fields = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_ipv6_fields_p_s_v2 ipv6_fields = {0};


	ecpriss_qudp_egress_per_port_cfg_s_v2 *qudp_egress_port =
		&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].egress_cfg;


	do
	{
		if(tx_cfg == NULL) {
			ret = -ENOMEM;
			break;
		}


		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_DST0_PORT_p_ENTRY_n_V2,
				port_index,
				tx_cfg->l2_hdr_tbl_idx,
				&eth_dst0_port);

		memcpy(&qudp_egress_port->eth_dst0_port[tx_cfg->l2_hdr_tbl_idx],&eth_dst0_port,sizeof(eth_dst0_port));

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_SRC1_DST1_PORT_p_ENTRY_n_V2,
				port_index,
				tx_cfg->l2_hdr_tbl_idx,
				&eth_src1_dst1_port);
		memcpy(&qudp_egress_port->eth_src1_dst1_port[tx_cfg->l2_hdr_tbl_idx],&eth_src1_dst1_port,sizeof(eth_src1_dst1_port));

		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_ETH_SRC0_PORT_p_ENTRY_n_V2,
				port_index,
				tx_cfg->l2_hdr_tbl_idx,
				&eth_src0_port);
		memcpy(&qudp_egress_port->eth_src0_port[tx_cfg->l2_hdr_tbl_idx],&eth_src0_port,sizeof(eth_src0_port));
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_VLAN_ETHERTYPE_PORT_p_ENTRY_n_V2,
				port_index,
				tx_cfg->l2_hdr_tbl_idx,
				&vlan_ethertype_port);

		memcpy(&qudp_egress_port->vlan_ethertype[tx_cfg->l2_hdr_tbl_idx],&vlan_ethertype_port,sizeof(vlan_ethertype_port));
		ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
				ECPRI_UDP_FH_EGRESS_VPORT_MISC_PORT_p_ENTRY_n_V2,
				port_index,
				tx_cfg->l2_hdr_tbl_idx,
				&vport_misc_port);

		if(tx_cfg->eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
					ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR0_PORT_p_ENTRY_n_V2,
					port_index,
					tx_cfg->l3_hdr_tbl_idx,
					&ip_src0);
			memcpy(&qudp_egress_port->src_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_src0,&ip_src0,sizeof(ip_src0));

			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
					ECPRI_UDP_FH_EGRESS_IP_DST_ADDR0_PORT_p_ENTRY_n_V2,
					port_index,
					tx_cfg->l3_hdr_tbl_idx,
					&ip_dst0);
			memcpy(&qudp_egress_port->dst_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_dst0,&ip_dst0,sizeof(ip_dst0));
			if(tx_cfg->ip_hdr.ip_type == ECPRISS_IPV6_TYPE)
			{

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR1_PORT_p_ENTRY_n_V2,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_src1);
				memcpy(&qudp_egress_port->src_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_src1,&ip_src1,sizeof(ip_src1));

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR2_PORT_p_ENTRY_n_V2,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_src2);
				memcpy(&qudp_egress_port->src_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_src2,&ip_src0,sizeof(ip_src2));

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR3_PORT_p_ENTRY_n_V2,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_src3);
				memcpy(&qudp_egress_port->src_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_src3,&ip_src0,sizeof(ip_src3));

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_IP_DST_ADDR1_PORT_p_ENTRY_n_V2,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_dst1);
				memcpy(&qudp_egress_port->dst_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_dst1,&ip_dst1,sizeof(ip_dst1));

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_IP_DST_ADDR2_PORT_p_ENTRY_n_V2,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_dst2);
				memcpy(&qudp_egress_port->dst_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_dst2,&ip_dst2,sizeof(ip_dst2));

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_IP_DST_ADDR3_PORT_p_ENTRY_n_V2,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_dst3);
				memcpy(&qudp_egress_port->dst_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_dst3,&ip_dst3,sizeof(ip_dst3));




			}

			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
					ECPRI_UDP_FH_EGRESS_UDP_PORTS_PORT_p_ENTRY_n_V2,
					port_index,
					tx_cfg->l3_hdr_tbl_idx,
					&udp_port);
			memcpy(&qudp_egress_port->udp_ports[tx_cfg->l3_hdr_tbl_idx],&udp_port,sizeof(udp_port));

			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
					ECPRI_UDP_FH_EGRESS_SA_TAG_IP_TOS_MISC_PORT_p_ENTRY_n_V2,
					port_index,
					tx_cfg->l3_hdr_tbl_idx,
					&ip_opts);
			if(tx_cfg->ip_hdr.ip_type == ECPRISS_IPV6_TYPE){
				memset(&ipv6_fields, 0 ,sizeof(ipv6_fields));
				ipv6_fields.flow_label = 0;
				ipv6_fields.hop_limit = 255;

				ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
						ECPRI_UDP_FH_EGRESS_IPV6_FIELDS_P_V2,
						port_index,
						&ipv6_fields);
				ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv6_cfg[port_index].hop_limit = ipv6_fields.hop_limit; 
				ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv6_cfg[port_index].flow_label = ipv6_fields.flow_label;

			}else{
				memset(&ipv4_fields, 0 ,sizeof(ipv4_fields));
				ipv4_fields.ttl = 255;
				ipv4_fields.id = 0;

				ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
						ECPRI_UDP_FH_EGRESS_IPV4_FIELDS_P_V2,
						port_index,
						&ipv4_fields);

				ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv4_cfg[port_index].ttl = ipv4_fields.ttl;
				ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv4_cfg[port_index].id = ipv4_fields.id;

			}

			qudp_egress_port->l3_tbl_valid_entry[tx_cfg->l3_hdr_tbl_idx] = false;
			qudp_egress_port->num_l3_tbl_entries--;
			ECPRILOGDBG("Entry deleted for l3 table index %d\n",tx_cfg->l3_hdr_tbl_idx);
		}
			qudp_egress_port->l2_tbl_valid_entry[tx_cfg->l2_hdr_tbl_idx] = false;
			qudp_egress_port->num_l2_tbl_entries--;
			ECPRILOGDBG("Entry deleted for l2 table index %d\n",tx_cfg->l2_hdr_tbl_idx);
	}while(0);
	return 0;
}


void ecpriss_qudp_set_lte_mac_filter_info(void)
{
	int32_t fh_index = 0;
	int j =0;
	int i =0;
	int lte_fh_index = 0;

	int port_mac_index[NUM_OF_FHP] = {ECPRISS_MAX_NR_MAC_PER_PORT,
		ECPRISS_MAX_NR_MAC_PER_PORT,
		ECPRISS_MAX_NR_MAC_PER_PORT};

	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_info_port_p_entry_n_s_v2 lte_fh_mac_info;
	struct ecpri_dma_port_params *dma_port_cfg;

	for(i=0;i<dma_endp_g.num_of_port_types;i++) {
		if(dma_endp_g.topology_params[i].port_type ==
				ECPRI_DMA_ENDP_STREAM_DEST_FH_LTE) {

			for(fh_index = 0; fh_index < NUM_OF_FHP; fh_index++){

				dma_port_cfg= &ecpriss_pdata_v2->xbar_ctx_v2->fh_lte_port_cfg[lte_fh_index].dma_port_cfg[fh_index];

				for(j=0;j<dma_port_cfg->num_of_rings;j++)
				{
					memset(&lte_fh_mac_info, 0, sizeof(lte_fh_mac_info));

					if(dma_port_cfg->dma_rings_param[j].dma_ring_type == ECPRI_DMA_RING_TYPE_LTE_DEFAULT)
					{
						switch(j)
						{
							case ECPRISS_CORE_LINK_ID_0:
								lte_fh_mac_info.ring_id = dma_port_cfg->dma_rings_param[0].dest_dma_ring_id;
								lte_fh_mac_info.gsi_id= dma_port_cfg->dma_rings_param[0].dest_dma_ring_gsi_id;
								break;
							case ECPRISS_CORE_LINK_ID_1:
								lte_fh_mac_info.ring_id = dma_port_cfg->dma_rings_param[1].dest_dma_ring_id;
								lte_fh_mac_info.gsi_id = dma_port_cfg->dma_rings_param[1].dest_dma_ring_gsi_id;
								break;
							case ECPRISS_CORE_LINK_ID_2:
								lte_fh_mac_info.ring_id = dma_port_cfg->dma_rings_param[2].dest_dma_ring_id;
								lte_fh_mac_info.gsi_id = dma_port_cfg->dma_rings_param[2].dest_dma_ring_gsi_id;
								break;
							case ECPRISS_CORE_LINK_ID_3:
								lte_fh_mac_info.ring_id = dma_port_cfg->dma_rings_param[3].dest_dma_ring_id;
								lte_fh_mac_info.gsi_id = dma_port_cfg->dma_rings_param[3].dest_dma_ring_gsi_id;
								break;
							default:
								ECPRILOGERR("Wrong default value %d\n",j);
								break;
						}

						lte_fh_mac_info.action = ECPRISS_MAC_ACTION_PASS_TO_A55;

						ECPRILOGDBG("LTE FH DMA Endp Params: Port:%d Index:%d RingID:%d action:%d\n",fh_index, port_mac_index[fh_index], lte_fh_mac_info.ring_id,lte_fh_mac_info.action);

						ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
								ECPRI_UDP_FH_FILT_MAC_ADDRESS_INFO_PORT_p_ENTRY_n_V2,
								fh_index,port_mac_index[fh_index]++,
								&lte_fh_mac_info);


					}
				}
			}
			lte_fh_index++;
		}
	}
}

void ecpriss_qudp_set_nr_mac_filter_info(void)
{
	int32_t fh_index = 0;
	int j =0;
	int i =0;

	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_info_port_p_entry_n_s_v2 nr_fh_mac_info;

	for(i=0;i<dma_endp_g.num_of_port_types;i++) {
		if(dma_endp_g.topology_params[i].port_type ==
				ECPRI_DMA_ENDP_STREAM_DEST_FH) {

			for(fh_index = 0; fh_index < NUM_OF_FHP; fh_index++){

				struct ecpri_dma_port_params *dma_port_cfg= &ecpriss_pdata_v2->xbar_ctx_v2->fh_port_cfg.dma_port_cfg[fh_index];

				for(j=0;j<dma_port_cfg->num_of_rings;j++)
				{
					memset(&nr_fh_mac_info, 0, sizeof(nr_fh_mac_info));
					if(dma_port_cfg->dma_rings_param[j].dma_ring_type == ECPRI_DMA_RING_TYPE_FH_DEFAULT)
					{
						nr_fh_mac_info.action = ECPRISS_MAC_ACTION_CONTINUE_NORMAL_PROCESSING;

						ECPRILOGDBG("NR FH DMA Endp Params: Port:%d Index:%d RingID:%d action:%d\n",fh_index, j, nr_fh_mac_info.ring_id, nr_fh_mac_info.action);

						ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
								ECPRI_UDP_FH_FILT_MAC_ADDRESS_INFO_PORT_p_ENTRY_n_V2,
								fh_index,j,
								&nr_fh_mac_info);


					}
				}
			}
		}
	}
}

int32_t ecpriss_qudp_set_lte_mac_filter(ecpriss_packet_payload_s *packet)
{
	int index = -1;
	int fh_index = -1;

	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_lsb_port_p_entry_n_u_v2 mac_lsb;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_msb_port_p_entry_n_u_v2 mac_msb;
	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port = NULL;
	ecpriss_lte_mac_addr_cfg_s *mac_info = NULL;

	mac_info = &packet->flow_cfg.mac_cfg;

	if(mac_info == NULL)
		return -1;

	if(ecpriss_pdata_v2->qudp_ctx_v2->lte_fh_enabled == 0)
		return -1;


	for(fh_index = 0; fh_index < NUM_OF_FHP; fh_index++){

		qudp_ingress_port = &ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[fh_index].ingress_port_cfg;

		for(index = 0 ; index < ECPRISS_MAX_LTE_MAC_PER_PORT ; index++) {

			memset(&mac_lsb, 0, sizeof(mac_lsb));
			memset(&mac_msb, 0, sizeof(mac_msb));

			mac_lsb.value = ((mac_info->lte_mac_addr[fh_index][index].mac[5]) | (mac_info->lte_mac_addr[fh_index][index].mac[4] << 8)
					| (mac_info->lte_mac_addr[fh_index][index].mac[3] << 16) | (mac_info->lte_mac_addr[fh_index][index].mac[2] << 24));


			mac_msb.value = ((mac_info->lte_mac_addr[fh_index][index].mac[1]) | (mac_info->lte_mac_addr[fh_index][index].mac[0] << 8));

			if(0 == mac_lsb.value && 0 == mac_msb.value){
				ECPRILOGINFO("Empty Mac Config for FH %u Index %u\n",fh_index, index);
				continue;
			}
			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
					ECPRI_UDP_FH_FILT_MAC_ADDRESS_LSB_PORT_p_ENTRY_n_V2,
					fh_index,
					index + ECPRISS_MAX_NR_MAC_PER_PORT,
					&mac_lsb);



			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
					ECPRI_UDP_FH_FILT_MAC_ADDRESS_MSB_PORT_p_ENTRY_n_V2,
					fh_index,
					index + ECPRISS_MAX_NR_MAC_PER_PORT,
					&mac_msb);

			ecpriss_qudp_ingress_modify_cfg_v2(fh_index,
					ENABLE_FILTER,
					ECPRISS_QUDP_RX_CFG_FLTR_MASK_LOCAL_MAC_ADDR,
					index + ECPRISS_MAX_NR_MAC_PER_PORT,
					CONFIGURE);

			qudp_ingress_port->dmac[index + ECPRISS_MAX_NR_MAC_PER_PORT].lsb = mac_lsb.value;
			qudp_ingress_port->dmac[index + ECPRISS_MAX_NR_MAC_PER_PORT].msb = mac_msb.value;
			qudp_ingress_port->num_mac_fltr_entries++;
		}

	}
	return 0;
}

bool ecpriss_qudp_check_loopback_filter_present(ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_msb_port_p_entry_n_u_v2 mac_msb, 
		ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_lsb_port_p_entry_n_u_v2 mac_lsb, ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port)
{	
	int i = 0;
	for(i = 0; i <  MAX_MAC_FILTER_ENTRIES ; i++){
		if(qudp_ingress_port->dmac[i].msb == mac_msb.value && qudp_ingress_port->dmac[i].lsb == mac_lsb.value)
			return true;
	}

	return false;
}



ecpriss_core_link_id_e get_link_id_from_mac_addr(uint32_t port_id, uint8_t *mac_addr, eth_ecpriss_topology_root_s* cur_topology){

	int i = 0;
	ecpriss_core_link_id_e link_index = ECPRISS_CFG_LINK_ID_MAX;
	for(i = 0; i< MAX_MAC_LINKS; i++){
		if(!memcmp(mac_addr, cur_topology->topology_params[ETH_ECPRISS_PORT_TYPE_FH].port_params[port_id].link_params[i].eth_mac_addr, ECPRISS_MAC_ADDR_LEN)){
			return i;
		}
	}
	return link_index;
}


void ecpriss_qudp_add_loopback_filters(ecpriss_packet_payload_s *packet)
{
	eth_ecpriss_topology_root_s cur_topology;
	eth_ecpriss_dev_mode_e device_mode;
	uint8_t dst_mac_addr[ECPRISS_MAC_ADDR_LEN];
	uint8_t src_mac_addr[ECPRISS_MAC_ADDR_LEN];
	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port = NULL;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_lsb_port_p_entry_n_u_v2 mac_lsb;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_msb_port_p_entry_n_u_v2 mac_msb;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_info_port_p_entry_n_s_v2 lte_fh_mac_info;
	struct ecpri_dma_port_params *dma_port_cfg;

	int i = 0, filter_slot = 0, mac_index = -1, ret = -1, base_slot = 0;
	bool is_loopback_enabled = false;
	uint8_t port_id = packet->flow_cfg.flow_tx_cfg.port_index;
	ecpriss_core_link_id_e link_id = ECPRISS_CFG_LINK_ID_MAX;

	ECPRILOGDBG("=== ADD_LOOPBACK_FILTERS: Entry for port %d ===\n", port_id);

	/* Extract source MAC from flow configuration */
	memcpy(src_mac_addr, packet->flow_cfg.flow_tx_cfg.qudp_tx_cfg.eth_hdr.src_mac_addr, ECPRISS_MAC_ADDR_LEN);
	memset(dst_mac_addr, 0, sizeof(dst_mac_addr));

	/* Get topology to check loopback status */
	ret = mtip_ecpri_ops.eth_ecpriss_get_topology(&device_mode, &cur_topology);
	if(ret == ETH_ECPRISS_STATUS_FAILURE){
		ECPRILOGERR("ADD_LOOPBACK: Failed to get topology for port %d\n", port_id);
		return;
	}

	/* Determine link ID from source MAC address */
	link_id = get_link_id_from_mac_addr(port_id, src_mac_addr, &cur_topology);
	if(link_id == ECPRISS_CFG_LINK_ID_MAX){
		ECPRILOGERR("ADD_LOOPBACK: Cannot get link_id for port %d\n", port_id);
		return;
	}

	ECPRILOGDBG("ADD_LOOPBACK: Port %d, Link ID: %d\n", port_id, link_id);

	qudp_ingress_port = &ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_id].ingress_port_cfg;
	if (!qudp_ingress_port) {
		ECPRILOGERR("ADD_LOOPBACK: qudp_ingress_port is NULL for port %d\n", port_id);
		return;
	}

	is_loopback_enabled = cur_topology.topology_params[ETH_ECPRISS_PORT_TYPE_FH].port_params[port_id].link_params[link_id].loopback_enabled;

	ECPRILOGDBG("ADD_LOOPBACK: Port %d, Link %d, Loopback enabled: %d\n", 
	             port_id, link_id, is_loopback_enabled);
	ECPRILOGDBG("ADD_LOOPBACK: Total Filter entries = %u\n", qudp_ingress_port->num_mac_fltr_entries);

	if(is_loopback_enabled){
		/* Extract destination MAC from egress configuration */
		memcpy(dst_mac_addr, packet->flow_cfg.flow_tx_cfg.qudp_tx_cfg.eth_hdr.dst_mac_addr, sizeof(dst_mac_addr));

		memset(&mac_lsb, 0, sizeof(mac_lsb));
		memset(&mac_msb, 0, sizeof(mac_msb));

		/* Construct MAC address LSB and MSB from dst_mac_addr */
		mac_lsb.value = ((dst_mac_addr[5]) | (dst_mac_addr[4] << 8)
				| (dst_mac_addr[3] << 16) | (dst_mac_addr[2] << 24));

		mac_msb.value = ((dst_mac_addr[1]) | (dst_mac_addr[0] << 8));

		/* Check if filter already exists */
		if(ecpriss_qudp_check_loopback_filter_present(mac_msb, mac_lsb, qudp_ingress_port) == false){

			ECPRILOGDBG("ADD_LOOPBACK: Filter not present, adding new entry\n");

			/* Find first available slot in range 16-19 by checking filter_index */
			base_slot = ECPRISS_MAX_NR_MAC_PER_PORT + ECPRISS_MAX_LTE_MAC_PER_PORT;
			filter_slot = -1;
			mac_index = -1;

			for(i = 0; i < 4; i++){
				/* Check if this mac_index slot is available (filter_index == -1) */
				if(ecpriss_loopback_filters_list.loopback_mac_addr[port_id][i].filter_added == false){
					filter_slot = base_slot + i;  /* Assign the actual filter slot (16-19) */
					mac_index = i;                 /* This is the index in the loopback list (0-3) */
					memcpy(ecpriss_loopback_filters_list.loopback_mac_addr[port_id][i].mac, dst_mac_addr, sizeof(dst_mac_addr));
					ECPRILOGINFO("ADD_LOOPBACK: Found available slot at mac_index %d, filter_slot %d\n", mac_index, filter_slot);
					break;
				}
			}

			if(mac_index == -1) {
				ECPRILOGERR("ADD_LOOPBACK: No available slot for port %d (all slots 16-19 occupied)\n", port_id);
				return;
			}

			if(filter_slot >= MAX_MAC_FILTER_ENTRIES || filter_slot < 0) {
				ECPRILOGERR("ADD_LOOPBACK: Invalid filter_slot %d (max: %d)\n",
				            filter_slot, MAX_MAC_FILTER_ENTRIES);
				return;
			}

			ECPRILOGDBG("ADD_LOOPBACK: Using filter slot: %d, mac_index: %d\n", filter_slot, mac_index);

			ecpriss_loopback_filters_list.num_of_loopback_filters_added[port_id]++;
			ecpriss_loopback_filters_list.loopback_mac_addr[port_id][mac_index].filter_index = filter_slot;
			ecpriss_loopback_filters_list.loopback_mac_addr[port_id][mac_index].filter_added = true;

			/* Write MAC address LSB to hardware */
			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_FILT_MAC_ADDRESS_LSB_PORT_p_ENTRY_n_V2,
				port_id,
				filter_slot,
				&mac_lsb);

			/* Write MAC address MSB to hardware */
			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
					ECPRI_UDP_FH_FILT_MAC_ADDRESS_MSB_PORT_p_ENTRY_n_V2,
					port_id,
					filter_slot,
					&mac_msb);

			/* Enable the filter in hardware */
			ecpriss_qudp_ingress_modify_cfg_v2(port_id,
					ENABLE_FILTER,
					ECPRISS_QUDP_RX_CFG_FLTR_MASK_LOCAL_MAC_ADDR,
					filter_slot,
					CONFIGURE);

			/* Update local cache */
			qudp_ingress_port->dmac[filter_slot].lsb = mac_lsb.value;
			qudp_ingress_port->dmac[filter_slot].msb = mac_msb.value;
			qudp_ingress_port->num_mac_fltr_entries++;

			ECPRILOGDBG("ADD_LOOPBACK: Successfully added filter at slot %d, total filters: %u\n",
			             filter_slot, qudp_ingress_port->num_mac_fltr_entries);

			/* Configure DMA ring info for loopback traffic */
			memset(&lte_fh_mac_info, 0, sizeof(lte_fh_mac_info));
			dma_port_cfg = &ecpriss_pdata_v2->xbar_ctx_v2->fh_lte_port_cfg[port_id].dma_port_cfg[port_id];

			lte_fh_mac_info.ring_id = dma_port_cfg->dma_rings_param[link_id].dest_dma_ring_id;
			lte_fh_mac_info.gsi_id = dma_port_cfg->dma_rings_param[link_id].dest_dma_ring_gsi_id;
			lte_fh_mac_info.action = ECPRISS_MAC_ACTION_PASS_TO_A55;

			ECPRILOGDBG("LTE FH DMA Endp Params: Port:%d Index:%d RingID:%d action:%d\n",
			            port_id, filter_slot, lte_fh_mac_info.ring_id, lte_fh_mac_info.action);

			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
					ECPRI_UDP_FH_FILT_MAC_ADDRESS_INFO_PORT_p_ENTRY_n_V2,
					port_id, filter_slot,
					&lte_fh_mac_info);

			ECPRILOGDBG("ADD_LOOPBACK: Added loopback filter port = %u, link_id = %u\n", port_id, link_id);
		} else {
			ECPRILOGDBG("ADD_LOOPBACK: Filter already exists, skipping\n");
		}
	} else {
		ECPRILOGDBG("ADD_LOOPBACK: Loopback not enabled for port %d link %d\n", port_id, link_id);
	}

	ECPRILOGDBG("=== ADD_LOOPBACK_FILTERS: Exit for port %d ===\n", port_id);
	return;
}

void ecpriss_qudp_remove_loopback_filters(ecpriss_packet_payload_s *packet)
{
	int port_id = packet->flow_cfg.flow_tx_cfg.port_index;
	int l2_hdr_tbl_idx = packet->flow_cfg.flow_tx_cfg.qudp_tx_cfg.l2_hdr_tbl_idx;
	int filter_slot = -1, i = 0;
	uint8_t dst_mac_addr[ECPRISS_MAC_ADDR_LEN];
	uint32_t dst_mac_lsb = 0;
	uint16_t dst_mac_msb = 0;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_lsb_port_p_entry_n_u_v2 mac_lsb;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_msb_port_p_entry_n_u_v2 mac_msb;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_info_port_p_entry_n_s_v2 lte_fh_mac_info;
	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port = NULL;
	ecpriss_qudp_egress_per_port_cfg_s_v2 *qudp_egress_port = NULL;

	ECPRILOGDBG("=== REMOVE_LOOPBACK_FILTERS: Entry for port %d, l2_idx %d ===\n", port_id, l2_hdr_tbl_idx);

	memset(&mac_lsb, 0, sizeof(mac_lsb));
	memset(&mac_msb, 0, sizeof(mac_msb));
	memset(&lte_fh_mac_info, 0, sizeof(lte_fh_mac_info));
	memset(dst_mac_addr, 0, sizeof(dst_mac_addr));

	qudp_ingress_port = &ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_id].ingress_port_cfg;
	qudp_egress_port = &ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_id].egress_cfg;

	if(ecpriss_loopback_filters_list.num_of_loopback_filters_added[port_id] == 0) {
		ECPRILOGERR("REMOVE_LOOPBACK: No loopback filters to remove for port %d\n", port_id);
		return;
	}

	/* Check if l2_hdr_tbl_idx is valid */
	if(l2_hdr_tbl_idx < 0 || l2_hdr_tbl_idx >= NUM_EGRESS_ENTRY) {
		ECPRILOGERR("REMOVE_LOOPBACK: Invalid l2_hdr_tbl_idx %d for port %d\n", l2_hdr_tbl_idx, port_id);
		return;
	}

	/* Extract destination MAC address from egress table using l2_hdr_tbl_idx */
	dst_mac_lsb = qudp_egress_port->eth_dst0_port[l2_hdr_tbl_idx].value;
	dst_mac_msb = qudp_egress_port->eth_src1_dst1_port[l2_hdr_tbl_idx].dst_msb;

	/* Validate that we have a valid MAC address in egress config */
	if(dst_mac_lsb == 0 && dst_mac_msb == 0) {
		ECPRILOGERR("REMOVE_LOOPBACK: No valid destination MAC found in egress config at l2_idx %d\n", l2_hdr_tbl_idx);
		return;
	}

	/* Extract MAC address bytes from LSB (lower 4 bytes) */
	dst_mac_addr[5] = (dst_mac_lsb & 0x000000ff);
	dst_mac_addr[4] = (dst_mac_lsb & 0x0000ff00) >> 8;
	dst_mac_addr[3] = (dst_mac_lsb & 0x00ff0000) >> 16;
	dst_mac_addr[2] = (dst_mac_lsb & 0xff000000) >> 24;

	/* Extract MAC address bytes from MSB (upper 2 bytes) */
	dst_mac_addr[1] = (dst_mac_msb & 0x00ff);
	dst_mac_addr[0] = (dst_mac_msb & 0xff00) >> 8;

	ECPRILOGDBG("REMOVE_LOOPBACK: Checking Dst MAC from egress cfg[%d]: %02x:%02x:%02x:%02x:%02x:%02x\n",
	             l2_hdr_tbl_idx,
	             dst_mac_addr[0], dst_mac_addr[1], dst_mac_addr[2],
	             dst_mac_addr[3], dst_mac_addr[4], dst_mac_addr[5]);

	/* Find matching MAC in loopback filter list */
	for(i = 0; i < 4; i++){
		if(ecpriss_loopback_filters_list.loopback_mac_addr[port_id][i].filter_added == false) {
			continue;
		}

		if(!memcmp(ecpriss_loopback_filters_list.loopback_mac_addr[port_id][i].mac, 
		           dst_mac_addr, ECPRISS_MAC_ADDR_LEN)){
			filter_slot = ecpriss_loopback_filters_list.loopback_mac_addr[port_id][i].filter_index;
			ECPRILOGERR("REMOVE_LOOPBACK: Found matching MAC at index %d, filter_slot %d\n", i, filter_slot);
			break;
		}
	}

	if(filter_slot == -1) {
		ECPRILOGERR("REMOVE_LOOPBACK: No matching filter found for this MAC address\n");
		return;
	}

	/* Clear MAC address LSB register */
	ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
			ECPRI_UDP_FH_FILT_MAC_ADDRESS_LSB_PORT_p_ENTRY_n_V2,
			port_id,
			filter_slot,
			&mac_lsb);

	/* Clear MAC address MSB register */
	ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
			ECPRI_UDP_FH_FILT_MAC_ADDRESS_MSB_PORT_p_ENTRY_n_V2,
			port_id,
			filter_slot,
			&mac_msb);

	/* Disable the filter in hardware */
	ecpriss_qudp_ingress_modify_cfg_v2(port_id,
			ENABLE_FILTER,
			ECPRISS_QUDP_RX_CFG_FLTR_MASK_LOCAL_MAC_ADDR,
			filter_slot,
			DE_CONFIGURE);

	/* Update local cache */
	qudp_ingress_port->dmac[filter_slot].lsb = 0;
	qudp_ingress_port->dmac[filter_slot].msb = 0;
	qudp_ingress_port->num_mac_fltr_entries--;

	ECPRILOGDBG("REMOVE_LOOPBACK: Removed filter at slot %d, remaining filters: %u\n",
	             filter_slot, qudp_ingress_port->num_mac_fltr_entries);

	lte_fh_mac_info.action = ECPRISS_MAC_ACTION_CONTINUE_NORMAL_PROCESSING;

	/* Clear DMA ring info */
	ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
			ECPRI_UDP_FH_FILT_MAC_ADDRESS_INFO_PORT_p_ENTRY_n_V2,
			port_id, filter_slot,
			&lte_fh_mac_info);

	/* Clear the entry in loopback list */
	ecpriss_loopback_filters_list.loopback_mac_addr[port_id][i].filter_index = 0;
	ecpriss_loopback_filters_list.loopback_mac_addr[port_id][i].filter_added = false;
	memset(ecpriss_loopback_filters_list.loopback_mac_addr[port_id][i].mac, 0, ECPRISS_MAC_ADDR_LEN);

	/* Decrement the count */
	ecpriss_loopback_filters_list.num_of_loopback_filters_added[port_id]--;

	ECPRILOGDBG("REMOVE_LOOPBACK: Successfully removed filter, remaining count: %d\n",
	             ecpriss_loopback_filters_list.num_of_loopback_filters_added[port_id]);

	ECPRILOGDBG("=== REMOVE_LOOPBACK_FILTERS: Exit for port %d ===\n", port_id);
	return;
}

void ecpriss_qudp_set_nr_mac_filter(void)
{
	int ret = 0;
	int i,j,k;
	uint8_t port_index;
	uint8_t num_links;
	eth_ecpriss_dev_mode_e device_mode;
	int action = CONFIGURE;
	ecpriss_qudp_port_cfg_s_v2      *port_cfg_local;
	eth_ecpriss_port_params_s *port_params = NULL;
	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port = NULL;

	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_lsb_port_p_entry_n_u_v2 mac_lsb;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_msb_port_p_entry_n_u_v2 mac_msb;

	do {
		ret = (mtip_ecpri_ops.eth_ecpriss_get_topology)(&device_mode,
				&eth_link_params_g);
		if(ret < 0) {
			break;
		}

		for(i=0;i<eth_link_params_g.num_unique_port_types;i++) {


			if(eth_link_params_g.topology_params[i].port_type ==
					ETH_ECPRISS_PORT_TYPE_FH) {

				ecpriss_pdata_v2->qudp_ctx_v2->num_ports[ETH_ECPRISS_PORT_TYPE_FH] =
					eth_link_params_g.topology_params[i].num_ports;

				for(j=0;j<ecpriss_pdata_v2->qudp_ctx_v2->num_ports[ETH_ECPRISS_PORT_TYPE_FH];j++){

					port_index =
						eth_link_params_g.topology_params[i].port_params[j].port_index;

					port_cfg_local =
						&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index];

					num_links =
						eth_link_params_g.topology_params[i].port_params[j].num_links;
					port_params = &eth_link_params_g.topology_params[i].port_params[port_index];

					qudp_ingress_port =
						&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].ingress_port_cfg;
					for(k=0;k<num_links;k++){

						memset(&mac_lsb, 0, sizeof(mac_lsb));
						memset(&mac_msb, 0, sizeof(mac_msb));


						if(port_params->link_params[k].link_state == ETH_ECPRISS_LINK_STATE_OPEN ||
								port_params->link_params[k].link_state == ETH_ECPRISS_LINK_STATE_UP){
							action = CONFIGURE;

							mac_lsb.value = ((port_params->link_params[k].eth_mac_addr[5]) | (port_params->link_params[k].eth_mac_addr[4] << 8)
									| (port_params->link_params[k].eth_mac_addr[3] << 16) | (port_params->link_params[k].eth_mac_addr[2] << 24));

							mac_msb.value = ((port_params->link_params[k].eth_mac_addr[1]) | (port_params->link_params[k].eth_mac_addr[0] << 8));

							ECPRILOGDBG("ecpriss_qudp: Configure NR MAC Filter MSB:%d LSB:%d for Port:%d Link:%d\n ", mac_msb.value, mac_lsb.value, port_index, k);

						}
						else if (port_params->link_params[k].link_state == ETH_ECPRISS_LINK_STATE_CLOSE){

							action = DE_CONFIGURE;

							ECPRILOGDBG("ecpriss_qudp: Deconfigure NR MAC Filter Port:%d Link:%d\n ", port_index, k);
						}else{
							continue;
						}

						ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
								ECPRI_UDP_FH_FILT_MAC_ADDRESS_LSB_PORT_p_ENTRY_n_V2,
								port_index,
								k,
								&mac_lsb);


						ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
								ECPRI_UDP_FH_FILT_MAC_ADDRESS_MSB_PORT_p_ENTRY_n_V2,
								port_index,
								k,
								&mac_msb);

						ecpriss_qudp_ingress_modify_cfg_v2(port_index,
								ENABLE_FILTER,
								ECPRISS_QUDP_RX_CFG_FLTR_MASK_LOCAL_MAC_ADDR,
								k,
								action);
						qudp_ingress_port->dmac[k].lsb = mac_lsb.value;
						qudp_ingress_port->dmac[k].msb = mac_msb.value;
						qudp_ingress_port->num_mac_fltr_entries++;
					}
				}
			}
		}

	}while (0);
}

int32_t ecpriss_qudp_fh_tx_hdr_ins_cfg_v2(uint32_t               port_index,
		ecpriss_qudp_tx_cfg_s *tx_cfg,
		ecpriss_transp_type tp_type)
{
	int ret = 0;

	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_dst0_port_p_entry_n_s_v2       eth_dst0_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src1_dst1_port_p_entry_n_s_v2   eth_src1_dst1_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_eth_src0_port_p_entry_n_s_v2        eth_src0_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_vlan_ethertype_port_p_entry_n_s_v2  vlan_ethertype_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_vport_misc_port_p_entry_n_s_v2      vport_misc_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr0_port_p_entry_n_s_v2    ip_src0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr1_port_p_entry_n_s_v2    ip_src1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr2_port_p_entry_n_s_v2    ip_src2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_src_addr3_port_p_entry_n_s_v2    ip_src3 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr0_port_p_entry_n_s_v2    ip_dst0 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr1_port_p_entry_n_s_v2    ip_dst1 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr2_port_p_entry_n_s_v2    ip_dst2 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_ip_dst_addr3_port_p_entry_n_s_v2    ip_dst3 = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_udp_ports_port_p_entry_n_s_v2       udp_port = {0};
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_sa_tag_ip_tos_misc_port_p_entry_n_s_v2   ip_opts = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_ipv4_fields_p_s_v2 ipv4_fields = {0};
	ecpri_qudp_hwio_def_ecpri_udp_l2_egress_ipv6_fields_p_s_v2 ipv6_fields = {0};


	ecpriss_qudp_egress_per_port_cfg_s_v2 *qudp_egress_port =
		&ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index].egress_cfg;
	do
	{
		if(tx_cfg == NULL) {
			ret = -ENOMEM;
			break;
		}

		/* 4 LSB goes to this eth_dst0_port */
		if(tp_type == ECPRISS_L2_TRANSP || tp_type == ECPRISS_L2_L3_TRANSP){

			eth_dst0_port.value = ((tx_cfg->eth_hdr.dst_mac_addr[5]) | (tx_cfg->eth_hdr.dst_mac_addr[4] << 8)
					| (tx_cfg->eth_hdr.dst_mac_addr[3] << 16) | (tx_cfg->eth_hdr.dst_mac_addr[2] << 24));



			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
					ECPRI_UDP_FH_EGRESS_ETH_DST0_PORT_p_ENTRY_n_V2,
					port_index,
					tx_cfg->l2_hdr_tbl_idx,
					&eth_dst0_port);

			memcpy(&qudp_egress_port->eth_dst0_port[tx_cfg->l2_hdr_tbl_idx],&eth_dst0_port,sizeof(eth_dst0_port));

			eth_src1_dst1_port.dst_msb = ((tx_cfg->eth_hdr.dst_mac_addr[1]) | (tx_cfg->eth_hdr.dst_mac_addr[0] << 8));
			eth_src1_dst1_port.src_msb = ((tx_cfg->eth_hdr.src_mac_addr[1]) | (tx_cfg->eth_hdr.src_mac_addr[0] << 8));


			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
					ECPRI_UDP_FH_EGRESS_ETH_SRC1_DST1_PORT_p_ENTRY_n_V2,
					port_index,
					tx_cfg->l2_hdr_tbl_idx,
					&eth_src1_dst1_port);


			memcpy(&qudp_egress_port->eth_src1_dst1_port[tx_cfg->l2_hdr_tbl_idx],&eth_src1_dst1_port,sizeof(eth_src1_dst1_port));

			eth_src0_port.value = ((tx_cfg->eth_hdr.src_mac_addr[5]) | (tx_cfg->eth_hdr.src_mac_addr[4] << 8)
					| (tx_cfg->eth_hdr.src_mac_addr[3] << 16) | (tx_cfg->eth_hdr.src_mac_addr[2]) << 24);



			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
					ECPRI_UDP_FH_EGRESS_ETH_SRC0_PORT_p_ENTRY_n_V2,
					port_index,
					tx_cfg->l2_hdr_tbl_idx,
					&eth_src0_port);
			memcpy(&qudp_egress_port->eth_src0_port[tx_cfg->l2_hdr_tbl_idx],&eth_src0_port,sizeof(eth_src0_port));

			vlan_ethertype_port.ethertype = tx_cfg->eth_hdr.orig_ethertype;

			vlan_ethertype_port.vlan_data = tx_cfg->eth_hdr.vlan_data;

			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
					ECPRI_UDP_FH_EGRESS_VLAN_ETHERTYPE_PORT_p_ENTRY_n_V2,
					port_index,
					tx_cfg->l2_hdr_tbl_idx,
					&vlan_ethertype_port);

			memcpy(&qudp_egress_port->vlan_ethertype[tx_cfg->l2_hdr_tbl_idx],&vlan_ethertype_port,sizeof(vlan_ethertype_port));
			vport_misc_port.vport = tx_cfg->eth_hdr.vport;

			vport_misc_port.has_vlan = tx_cfg->eth_hdr.is_vlan;

			vport_misc_port.vport_action = tx_cfg->eth_hdr.vport_action;

			ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
					ECPRI_UDP_FH_EGRESS_VPORT_MISC_PORT_p_ENTRY_n_V2,
					port_index,
					tx_cfg->l2_hdr_tbl_idx,
					&vport_misc_port);
		}

		if(tp_type == ECPRISS_L3_TRANSP || tp_type == ECPRISS_L2_L3_TRANSP){

			if(tx_cfg->eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

				ip_src0.value |= ((tx_cfg->ip_hdr.src_ip_addr[3]) | (tx_cfg->ip_hdr.src_ip_addr[2] << 8) | (tx_cfg->ip_hdr.src_ip_addr[1] << 16)
						| (tx_cfg->ip_hdr.src_ip_addr[0] << 24));

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR0_PORT_p_ENTRY_n_V2,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_src0);


				memcpy(&qudp_egress_port->src_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_src0,&ip_src0,sizeof(ip_src0));

				ip_dst0.value |= ((tx_cfg->ip_hdr.dst_ip_addr[3]) | (tx_cfg->ip_hdr.dst_ip_addr[2] << 8) | (tx_cfg->ip_hdr.dst_ip_addr[1] << 16)
						| (tx_cfg->ip_hdr.dst_ip_addr[0] << 24));

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_IP_DST_ADDR0_PORT_p_ENTRY_n_V2,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_dst0);
				memcpy(&qudp_egress_port->dst_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_dst0,&ip_dst0,sizeof(ip_dst0));

				if(tx_cfg->ip_hdr.ip_type == ECPRISS_IPV6_TYPE)
				{

					ip_src1.value |= ((tx_cfg->ip_hdr.src_ip_addr[7]) | (tx_cfg->ip_hdr.src_ip_addr[6] << 8) | (tx_cfg->ip_hdr.src_ip_addr[5] << 16)
							| (tx_cfg->ip_hdr.src_ip_addr[4] << 24));

					ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
							ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR1_PORT_p_ENTRY_n_V2,
							port_index,
							tx_cfg->l3_hdr_tbl_idx,
							&ip_src1);
					memcpy(&qudp_egress_port->src_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_src1,&ip_src1,sizeof(ip_src0));

					ip_src2.value |= ((tx_cfg->ip_hdr.src_ip_addr[11]) | (tx_cfg->ip_hdr.src_ip_addr[10] << 8) | (tx_cfg->ip_hdr.src_ip_addr[9] << 16)
							| (tx_cfg->ip_hdr.src_ip_addr[8] << 24));

					ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
							ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR2_PORT_p_ENTRY_n_V2,
							port_index,
							tx_cfg->l3_hdr_tbl_idx,
							&ip_src2);

					memcpy(&qudp_egress_port->src_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_src2,&ip_src0,sizeof(ip_src2));
					ip_src3.value |= ((tx_cfg->ip_hdr.src_ip_addr[15]) | (tx_cfg->ip_hdr.src_ip_addr[14] << 8) | (tx_cfg->ip_hdr.src_ip_addr[13] << 16)
							| (tx_cfg->ip_hdr.src_ip_addr[12] << 24));

					ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
							ECPRI_UDP_FH_EGRESS_IP_SRC_ADDR3_PORT_p_ENTRY_n_V2,
							port_index,
							tx_cfg->l3_hdr_tbl_idx,
							&ip_src3);

					memcpy(&qudp_egress_port->src_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_src3,&ip_src0,sizeof(ip_src3));
					ip_dst1.value |= ((tx_cfg->ip_hdr.dst_ip_addr[7]) | (tx_cfg->ip_hdr.dst_ip_addr[6] << 8) | (tx_cfg->ip_hdr.dst_ip_addr[5] << 16)
							| (tx_cfg->ip_hdr.dst_ip_addr[4] << 24));

					ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
							ECPRI_UDP_FH_EGRESS_IP_DST_ADDR1_PORT_p_ENTRY_n_V2,
							port_index,
							tx_cfg->l3_hdr_tbl_idx,
							&ip_dst1);
					memcpy(&qudp_egress_port->dst_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_dst1,&ip_dst1,sizeof(ip_dst1));

					ip_dst2.value |= ((tx_cfg->ip_hdr.dst_ip_addr[11]) | (tx_cfg->ip_hdr.dst_ip_addr[10] << 8) | (tx_cfg->ip_hdr.dst_ip_addr[9] << 16)
							| (tx_cfg->ip_hdr.dst_ip_addr[8] << 24));

					ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
							ECPRI_UDP_FH_EGRESS_IP_DST_ADDR2_PORT_p_ENTRY_n_V2,
							port_index,
							tx_cfg->l3_hdr_tbl_idx,
							&ip_dst2);

					memcpy(&qudp_egress_port->dst_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_dst2,&ip_dst2,sizeof(ip_dst2));
					ip_dst3.value |= ((tx_cfg->ip_hdr.dst_ip_addr[15]) | (tx_cfg->ip_hdr.dst_ip_addr[14] << 8) | (tx_cfg->ip_hdr.dst_ip_addr[13] << 16)
							| (tx_cfg->ip_hdr.dst_ip_addr[12] << 24));

					ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
							ECPRI_UDP_FH_EGRESS_IP_DST_ADDR3_PORT_p_ENTRY_n_V2,
							port_index,
							tx_cfg->l3_hdr_tbl_idx,
							&ip_dst3);

					memcpy(&qudp_egress_port->dst_ip_addr[tx_cfg->l3_hdr_tbl_idx].ip_dst3,&ip_dst3,sizeof(ip_dst3));

				}

				udp_port.src = tx_cfg->ip_hdr.src_udp_port;
				udp_port.dst = tx_cfg->ip_hdr.dst_udp_port;

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_UDP_PORTS_PORT_p_ENTRY_n_V2,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&udp_port);

				memcpy(&qudp_egress_port->udp_ports[tx_cfg->l3_hdr_tbl_idx],&udp_port,sizeof(udp_port));

				ip_opts.sa_tag_data = tx_cfg->ip_hdr.sa_tag_data ;
				ip_opts.tos = tx_cfg->ip_hdr.tos;
				ip_opts.df_bit = tx_cfg->ip_hdr.df_en;
				ip_opts.calc_udp_cs = tx_cfg->ip_hdr.udp_chksum_en;
				ip_opts.is_ipsec = tx_cfg->ip_hdr.ipsec_en;
				//ip_opts.rsvd = tx_cfg->ip_hdr.rsvd;
				ip_opts.ip_type = tx_cfg->ip_hdr.ip_type;

				ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_RAMS,
						ECPRI_UDP_FH_EGRESS_SA_TAG_IP_TOS_MISC_PORT_p_ENTRY_n_V2,
						port_index,
						tx_cfg->l3_hdr_tbl_idx,
						&ip_opts);

				if(!qudp_egress_port->l3_tbl_valid_entry[tx_cfg->l3_hdr_tbl_idx]){
					qudp_egress_port->l3_tbl_valid_entry[tx_cfg->l3_hdr_tbl_idx] = true;
					qudp_egress_port->num_l3_tbl_entries++;
				}
			}
		}
		if(!qudp_egress_port->l2_tbl_valid_entry[tx_cfg->l2_hdr_tbl_idx]){
			qudp_egress_port->l2_tbl_valid_entry[tx_cfg->l2_hdr_tbl_idx] = true;
			qudp_egress_port->num_l2_tbl_entries++;
		}

		if(tx_cfg->ip_hdr.ip_type == ECPRISS_IPV6_TYPE){

			/*
			 * Hop limit zero is an invalid value. if it is zero set it as 255 POR value
			 */
			if( 0 == ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv6_cfg[port_index].hop_limit)
				ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv6_cfg[port_index].hop_limit = 255;
			/*
			 * If new configuration is same as previous configuration,
			 * no need to do register write.
			 */
			if((tx_cfg->ip_hdr_p.flow_label != ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv6_cfg[port_index].flow_label ||
					tx_cfg->ip_hdr_p.hop_limit != ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv6_cfg[port_index].hop_limit) &&
					tx_cfg->ip_hdr_p.hop_limit != 0){
				memset(&ipv6_fields, 0 ,sizeof(ipv6_fields));

				ipv6_fields.flow_label = tx_cfg->ip_hdr_p.flow_label;
				ipv6_fields.hop_limit = tx_cfg->ip_hdr_p.hop_limit;

				ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
						ECPRI_UDP_FH_EGRESS_IPV6_FIELDS_P_V2,
						port_index,
						&ipv6_fields);
				ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv6_cfg[port_index].hop_limit = ipv6_fields.hop_limit;
				ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv6_cfg[port_index].flow_label = ipv6_fields.flow_label;
			}
		}else{
			/*
			 * TTL zero is an invalid value. if it is zero set it as 255 POR value
			 */
			if(0 == ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv4_cfg[port_index].ttl)
				ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv4_cfg[port_index].ttl  = 255;
			/*
			 * If new configuration is same as previous configuration,
			 * no need to do register write.
			 */
			if((tx_cfg->ip_hdr_p.ttl != ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv4_cfg[port_index].ttl ||
					tx_cfg->ip_hdr_p.identification != ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv4_cfg[port_index].id) &&
					tx_cfg->ip_hdr_p.ttl != 0){
				memset(&ipv4_fields, 0 ,sizeof(ipv4_fields));

				ipv4_fields.ttl = tx_cfg->ip_hdr_p.ttl;
				ipv4_fields.id = tx_cfg->ip_hdr_p.identification;
			}

			ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
					ECPRI_UDP_FH_EGRESS_IPV4_FIELDS_P_V2,
					port_index,
					&ipv4_fields);
			ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv4_cfg[port_index].ttl = ipv4_fields.ttl;
			ecpriss_pdata_v2->cfg_stats_v2.qudp_cfg_v2.egress.ipv4_cfg[port_index].id = ipv4_fields.id;
		}

	}while(0);
	return 0;
}


int ecpriss_qudp_get_ingress_action(void)
{
	return ecpriss_qudp_ingress_action;
}
void ecpriss_qudp_set_ingress_action(int val)
{
	int ret = 0;
	ecpriss_qudp_ingress_action = val;

	ret = ecpriss_qudp_ingress_init_cfg_modify_v2(ecpriss_qudp_ingress_action);

	if(ret != 0)
		ECPRILOGINFO("ecpriss_qudp_ingress_init_action modify failed\n");

	return;
}


int ecpriss_qudp_get_strict_filter_config(int fh_index)
{
	int strict_filter_config = 0;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_error_channel_cfg_p_s_v2 filt_error_channel_cfg;

	ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
		ECPRI_UDP_FH_FILT_ERROR_CHANNEL_CFG_p_V2,
		fh_index,
		&filt_error_channel_cfg);

	if(filt_error_channel_cfg.send_vlan_filt_miss_to_error_channel &&
			filt_error_channel_cfg.send_ip_filt_miss_to_error_channel) {
		strict_filter_config = 1;
	}

	return strict_filter_config;
}
void ecpriss_qudp_set_strict_filter_config(int val,int fh_index)
{
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_error_channel_cfg_p_s_v2 strict_filter_config;

	if(fh_index < 0 || fh_index >= ECPRISS_MAX_PORTS)
		return;

	memset(&strict_filter_config,0,sizeof(strict_filter_config));
  	strict_filter_config.send_vlan_filt_miss_to_error_channel = val;
  	strict_filter_config.send_ip_filt_miss_to_error_channel = val;

	ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
		ECPRI_UDP_FH_FILT_ERROR_CHANNEL_CFG_p_V2,
		fh_index,
		&strict_filter_config);

	ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[fh_index].strict_filter_status = val;

	return;
}

int ecpriss_qudp_get_lte_mac_addr(int port, int index, csm_lte_ethdev_mac_s *mac_info)
{

	uint32_t mac_msb = 0;
	uint32_t mac_lsb = 0;

	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port = NULL;
	qudp_ingress_port = &ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port].ingress_port_cfg;

	if(!qudp_ingress_port)
		return -1;


	mac_msb = qudp_ingress_port->dmac[index].msb;
	mac_lsb = qudp_ingress_port->dmac[index].lsb;


	mac_info->mac_addr[5] = (mac_lsb & 0x000000ff);
	mac_info->mac_addr[4] = (mac_lsb & 0x0000ff00) >> 8;
	mac_info->mac_addr[3] = (mac_lsb & 0x00ff0000) >> 16;
	mac_info->mac_addr[2] = (mac_lsb & 0xff000000) >> 24;

	mac_info->mac_addr[1] = (mac_msb & 0x000000ff);
	mac_info->mac_addr[0] = (mac_msb & 0x0000ff00) >> 8;

	ECPRILOGDBG("%x:%x:%x:%x\n", mac_info->mac_addr[2], mac_info->mac_addr[3],
			mac_info->mac_addr[4],mac_info->mac_addr[5]);

	return 0;

}

int ecpriss_qudp_set_lte_mac_addr(int port, int index, csm_lte_ethdev_mac_s *mac_info)
{

	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_lsb_port_p_entry_n_u_v2 mac_lsb;
	ecpri_qudp_hwio_def_ecpri_udp_fh_filt_mac_address_msb_port_p_entry_n_u_v2 mac_msb;

	ecpriss_qudp_ingress_per_port_cfg_s_v2 *qudp_ingress_port = NULL;
	qudp_ingress_port = &ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port].ingress_port_cfg;

	if(!qudp_ingress_port)
		return -1;

	memset(&mac_lsb, 0, sizeof(mac_lsb));
	memset(&mac_msb, 0, sizeof(mac_msb));

	mac_lsb.value = ((mac_info->mac_addr[5]) | (mac_info->mac_addr[4] << 8)
				| (mac_info->mac_addr[3] << 16) | (mac_info->mac_addr[2] << 24));


	mac_msb.value = ((mac_info->mac_addr[1]) | (mac_info->mac_addr[0] << 8));

	ECPRILOGDBG("%x:%x:%x:%x:%x:%x\n",mac_info->mac_addr[0], mac_info->mac_addr[1], mac_info->mac_addr[2], mac_info->mac_addr[3],
			mac_info->mac_addr[4],mac_info->mac_addr[5]);

	if(0 == mac_lsb.value && 0 == mac_msb.value){
		return -1;
	}

	ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_FILT_MAC_ADDRESS_LSB_PORT_p_ENTRY_n_V2,
				port,
				index,
				&mac_lsb);



	ecpriss_qudp_hal_write_reg_mn_fields(ECPRISS_QUDP_FH_FILTER,
				ECPRI_UDP_FH_FILT_MAC_ADDRESS_MSB_PORT_p_ENTRY_n_V2,
				port,
				index,
				&mac_msb);

	qudp_ingress_port->dmac[index].lsb = mac_lsb.value;
	qudp_ingress_port->dmac[index].msb = mac_msb.value;

	return 0;

}

#ifdef UNUSED

/**
 * ecpri_qudp_set_eth_type()
 *
 *
 * Returns:	0 on success, negative on failure
 */
static int ecpriss_qudp_set_eth_type(void)
{
	return 0;
}



/**
 * ecpri_qudp_egress_modify_cfg()
 *
 *
 * Returns:	0 on success, negative on failure
 */
static int ecpriss_qudp_egress_modify_cfg(uint32_t value,
		uint32_t field)
{
	return 0;
}


/**
 *  ecpriss_qudp_fh_tx_hdr_ins_cfg
 *
 *	Requirements - port index and maintain a table of entries
 *
 *	Description -
 *
 *
 *
 * Returns:	0 on success, negative on failure
 */
static int ecpriss_qudp_validate_rx_idx_cfg(uint32_t               port_index,
		ecpriss_qudp_tx_cfg_s *tx_qudp_cfg,
		ecpriss_xbar_tx_cfg_s *tx_xbar_cfg)
{
	return 0;
}

/**
 *  ecpriss_qudp_fh_tx_hdr_ins_cfg
 *
 *	Requirements - port index and maintain a table of entries
 *
 *	Description -
 *
 *
 *
 * Returns:	0 on success, negative on failure
 */
static int ecpriss_qudp_validate_tx_idx_cfg(uint32_t  port_index,
		ecpriss_qudp_tx_cfg_s *tx_qudp_cfg,
		ecpriss_xbar_tx_cfg_s *tx_xbar_cfg)
{
	return 0;
}



/**
 * ecpri_qudp_fh_enable_stats()
 *
 *
 * Returns:	0 on success, negative on failure
 */
static int ecpriss_global_operation_mode_cfg(void)
{
	/*Todo: Complete this function */
	return 0;
}


/**
 * ecpri_qudp_fh_enable_stats()
 *
 *
 * Returns:	0 on success, negative on failure
 */
static int ecpriss_qudp_enable_stats(uint8_t                 port_index,
		uint32_t                port_type)
{
	/* Todo:Complete stats */
	if(port_type == ECPRISS_PORT_TYPE_FH)
	{
	}
	else if (port_type == ECPRISS_PORT_TYPE_C2C)
	{
	}
	else if (port_type == ECPRISS_PORT_TYPE_L2)
	{
	}

	return 0;
}


#endif
#if 0


void ecpriss_qudp_print_c2c_egress_stats(uint32_t port_index, uint32_t link_index)
{
	uint32_t lsb_val = 0;
	uint64_t msb_val = 0;
	uint64_t val = 0;
	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_NUM_UDP_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_NUM_UDP_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.egress_num_udp_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_C2C_EGRESS_NUM_UDP_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val =ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_NUM_ETH_ONLY_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_NUM_ETH_ONLY_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.egress_num_eth_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_C2C_EGRESS_NUM_ETH_ONLY_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_NUM_BYPASSED_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_NUM_BYPASSED_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.egress_num_bypassed_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_C2C_EGRESS_NUM_BYPASSED_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_MTU_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val =ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_EGRESS_MTU_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.egress_num_mtu_err_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_C2C_EGRESS_MTU_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	return;
}


void ecpriss_qudp_print_c2c_ingress_stats(uint32_t port_index, uint32_t link_index)
{
	uint32_t lsb_val = 0;
	uint64_t msb_val = 0;
	uint64_t val = 0;
	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_NUM_UDP_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_NUM_UDP_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_num_udp_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_C2C_INGRESS_NUM_UDP_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);


	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_NUM_NON_UDP_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_NUM_NON_UDP_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_num_non_udp_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_C2C_INGRESS_NUM_NON_UDP_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_FCS_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_FCS_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_num_fcs_err_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_C2C_INGRESS_FCS_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_IPV4_CS_ERROR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_IPV4_CS_ERROR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_num_ipv4_cs_err_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_C2C_INGRESS_IPV4_CS_ERROR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_UDP_CS_ERROR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_UDP_CS_ERROR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_num_udp_cs_err_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_C2C_INGRESS_UDP_CS_ERROR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_VLAN_FILTERED_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_VLAN_FILTERED_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);

	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_num_vlan_filtered_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_C2C_INGRESS_VLAN_FILTERED_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_SEC_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_SEC_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);
	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_num_sec_err_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_C2C_INGRESS_SEC_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	lsb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_IP_LEN_ERR_PACKETS_LSB_PORT_p_LINK_n,
			port_index,
			link_index);
	msb_val = ecpriss_qudp_hal_read_reg_mn(ECPRISS_QUDP_C2C,
			ECPRI_UDP_C2C_INGRESS_IP_LEN_ERR_PACKETS_MSB_PORT_p_LINK_n,
			port_index,
			link_index);
	val = msb_val << MSB_SHIFT | lsb_val;
	ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index].stats.ingress_ip_len_err_packets[link_index] = val;
	ECPRILOGINFO("ECPRI_UDP_C2C_INGRESS_IP_LEN_ERR_PACKETS : port_index :%d link_index %d value = %d\n", port_index,link_index,val);

	return;
}
#endif
int32_t ecpriss_qudp_ingress_table_config(ecpriss_packet_payload_s *packet)
{
	ecpriss_flow_rx_cfg_s *flow_rx = NULL;
	int ret = 0;

	if(packet == NULL) {
		return -1;
	}
	if(ecpriss_hw_ver == ECPRISS_HW_v2_0) {
		flow_rx = &packet->flow_cfg.flow_rx_cfg;
		ret = ecpriss_qudp_fh_rx_filter_cfg_v2(
				flow_rx->port_index,
				&flow_rx->qudp_rx_cfg);
		if(ret < 0) {
			ECPRILOGERR("%s: QUDP filter configuration failed Port: %d\n",__func__,flow_rx->port_index);
		}
	}
	return ret;
}
int32_t ecpriss_qudp_ingress_table_deconfig(ecpriss_packet_payload_s *packet)
{
	ecpriss_flow_rx_cfg_s *flow_rx = NULL;
	int ret = 0;

	if(packet == NULL) {
		return -1;
	}
	if(ecpriss_hw_ver == ECPRISS_HW_v2_0) {
		flow_rx = &packet->flow_cfg.flow_rx_cfg;
		ret = ecpriss_qudp_fh_rx_filter_decfg_v2(
				flow_rx->port_index,
				&flow_rx->qudp_rx_cfg);

		if(ret < 0) {
			ECPRILOGERR("%s: QUDP filter de-configuration failed Port: %d\n",__func__,flow_rx->port_index);
		}
	}
	return ret;
}
int32_t ecpriss_qudp_egress_l2_table_reconfig(ecpriss_packet_payload_s *packet)
{
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_l2_encap_index_override_p_s_v2 l2_cfg;
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_config_p_s_v2 egress_config;
	ecpriss_flow_tx_cfg_s *flow_tx = NULL;
	int port_idx = 0;
	int ret = 0;

	if(packet == NULL) {
		return -1;
	}

	if(ecpriss_hw_ver != ECPRISS_HW_v2_0) {
		ECPRILOGERR("Not a V2 Hw %s\n",__func__);
		return -1;
	}

	flow_tx = &packet->flow_cfg.flow_tx_cfg;
	port_idx = flow_tx->port_index;

	memset(&l2_cfg,0, sizeof(l2_cfg));
	memset(&egress_config,0,sizeof(egress_config));

	/*
	 * Below methode of reconfiguration is required when
	 * User want to reconfigure flows while traffic is flowing in the background.
	 */

	l2_cfg.redirect_from = flow_tx->qudp_tx_cfg.l2_hdr_tbl_idx;
	l2_cfg.redirect_to = ECPRISS_RESERVED_OVERRIDE_INDEX;

	ECPRILOGINFO("L2 %u\n",l2_cfg.redirect_from);

	/*
	 * We need to write this config at L2/L3 index ECPRISS_RESERVED_OVERRIDE_INDEX first
	 * save the original index and updat the config with ECPRISS_RESERVED_OVERRIDE_INDEX
	 */
	flow_tx->qudp_tx_cfg.l2_hdr_tbl_idx = ECPRISS_RESERVED_OVERRIDE_INDEX;

	/*
	 * Update the new config at index ECPRISS_RESERVED_OVERRIDE_INDEX
	 */
	ECPRILOGINFO("Reconfig L2 idx %u \n",flow_tx->qudp_tx_cfg.l2_hdr_tbl_idx);

	ret = ecpriss_qudp_fh_tx_hdr_ins_cfg_v2(
			flow_tx->port_index,
			&flow_tx->qudp_tx_cfg,
			ECPRISS_L2_TRANSP);

	/*
	 * Read rigister before any configuration change
	 */
	ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
			port_idx,
			&egress_config);
	/*
	 * Only change override enable bit config
	 */
	egress_config.l2_encap_index_override_en = 1;

	/*
	 * Update the HW registers with over from and override to index
	 * l2 index X to ECPRISS_RESERVED_OVERRIDE_INDEX, l3 index Y to ECPRISS_RESERVED_OVERRIDE_INDEX
	 */
	ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_L2_ENCAP_INDEX_OVERRIDE_P_V2,
			port_idx,
			&l2_cfg);

	/*
	 * Redirect the current L2 -> X connfig to ECPRISS_RESERVED_OVERRIDE_INDEX
	 * Redirect the current L3-> Y config to ECPRISS_RESERVED_OVERRIDE_INDEX
	 * Set enable bit to start redirection
	 */
	ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
			port_idx,
			&egress_config);


	/*
	 * Restor the original l2/l3 table indexes
	 */
	flow_tx->qudp_tx_cfg.l2_hdr_tbl_idx = l2_cfg.redirect_from;

	/*
	 * Now we need to update l2/l3 config at real index
	 */
	ret = ecpriss_qudp_fh_tx_hdr_ins_cfg_v2(
			flow_tx->port_index,
			&flow_tx->qudp_tx_cfg,
			ECPRISS_L2_TRANSP);
	/*
	 * Clear enable bit to stop redirection of l2/l3 table
	 */
	ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
			port_idx,
			&egress_config);

	egress_config.l2_encap_index_override_en = 0;

	ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
			port_idx,
			&egress_config);

	/*
	 * Reset the ECPRISS_RESERVED_OVERRIDE_INDEX indexes with zero
	 */
	memset(&flow_tx->qudp_tx_cfg, 0,sizeof(ecpriss_qudp_tx_cfg_s));

	flow_tx->qudp_tx_cfg.l2_hdr_tbl_idx = ECPRISS_RESERVED_OVERRIDE_INDEX;

	/*
	 * Update the new config at index ECPRISS_RESERVED_OVERRIDE_INDEX
	 */
	ret = ecpriss_qudp_fh_tx_hdr_ins_cfg_v2(
			flow_tx->port_index,
			&flow_tx->qudp_tx_cfg,
			ECPRISS_L2_TRANSP);
	return ret;
}

int32_t ecpriss_qudp_egress_l3_table_reconfig(ecpriss_packet_payload_s *packet)
{
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_l3_encap_index_override_p_s_v2 l3_cfg;
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_config_p_s_v2 egress_config;
	ecpriss_flow_tx_cfg_s *flow_tx = NULL;
	int port_idx = 0;
	int ret = 0;

	if(packet == NULL) {
		return -1;
	}

	if(ecpriss_hw_ver != ECPRISS_HW_v2_0) {
		ECPRILOGERR("Not a V2 Hw %s\n",__func__);
		return -1;
	}

	flow_tx = &packet->flow_cfg.flow_tx_cfg;
	port_idx = flow_tx->port_index;

	memset(&l3_cfg,0, sizeof(l3_cfg));
	memset(&egress_config,0,sizeof(egress_config));

	/*
	 * Below methode of reconfiguration is required when
	 * User want to reconfigure flows while traffic is flowing in the background.
	 */

	if(flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

		l3_cfg.redirect_from =  flow_tx->qudp_tx_cfg.l3_hdr_tbl_idx;
		l3_cfg.redirect_to = ECPRISS_RESERVED_OVERRIDE_INDEX;
	}
	ECPRILOGINFO("L3 %u\n",l3_cfg.redirect_from);

	/*
	 * We need to write this config at L2/L3 index ECPRISS_RESERVED_OVERRIDE_INDEX first
	 * save the original index and updat the config with ECPRISS_RESERVED_OVERRIDE_INDEX
	 */

	if(flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

		flow_tx->qudp_tx_cfg.l3_hdr_tbl_idx = ECPRISS_RESERVED_OVERRIDE_INDEX;
	}

	/*
	 * Update the new config at index ECPRISS_RESERVED_OVERRIDE_INDEX
	 */
	ECPRILOGINFO("Reconfig L3 idx %u \n",flow_tx->qudp_tx_cfg.l3_hdr_tbl_idx);

	ret = ecpriss_qudp_fh_tx_hdr_ins_cfg_v2(
			flow_tx->port_index,
			&flow_tx->qudp_tx_cfg,
			ECPRISS_L3_TRANSP);

	/*
	 * Read rigister before any configuration change
	 */
	ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
			port_idx,
			&egress_config);
	/*
	 * Only change override enable bit config
	 */

	if(flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

		egress_config.l3_encap_index_override_en = 1;
	}

	/*
	 * Update the HW registers with over from and override to index
	 * l2 index X to ECPRISS_RESERVED_OVERRIDE_INDEX, l3 index Y to ECPRISS_RESERVED_OVERRIDE_INDEX
	 */
	if(flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_EGRESS_L3_ENCAP_INDEX_OVERRIDE_P_V2,
				port_idx,
				&l3_cfg);
	}

	/*
	 * Redirect the current L2 -> X connfig to ECPRISS_RESERVED_OVERRIDE_INDEX
	 * Redirect the current L3-> Y config to ECPRISS_RESERVED_OVERRIDE_INDEX
	 */
	ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
			port_idx,
			&egress_config);


	/*
	 * Restor the original l2/l3 table indexes
	 */

	if(flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

		flow_tx->qudp_tx_cfg.l3_hdr_tbl_idx = l3_cfg.redirect_from;
	}
	/*
	 * Now we need to update l2/l3 config at real index
	 */
	ret = ecpriss_qudp_fh_tx_hdr_ins_cfg_v2(
			flow_tx->port_index,
			&flow_tx->qudp_tx_cfg,
			ECPRISS_L3_TRANSP);

	ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
			port_idx,
			&egress_config);

	if(flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

		egress_config.l3_encap_index_override_en = 0;
	}

	ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
			port_idx,
			&egress_config);

	/*
	 * Reset the ECPRISS_RESERVED_OVERRIDE_INDEX indexes with zero
	 */
	memset(&flow_tx->qudp_tx_cfg, 0,sizeof(ecpriss_qudp_tx_cfg_s));

	if(flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

		flow_tx->qudp_tx_cfg.l3_hdr_tbl_idx = ECPRISS_RESERVED_OVERRIDE_INDEX;
	}

	/*
	 * Update the new config at index ECPRISS_RESERVED_OVERRIDE_INDEX
	 */
	ret = ecpriss_qudp_fh_tx_hdr_ins_cfg_v2(
			flow_tx->port_index,
			&flow_tx->qudp_tx_cfg,
			ECPRISS_L3_TRANSP);

	return ret;
}

int32_t ecpriss_qudp_egress_l2_l3_table_reconfig(ecpriss_packet_payload_s *packet)
{
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_l2_encap_index_override_p_s_v2 l2_cfg;
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_l3_encap_index_override_p_s_v2 l3_cfg;
	ecpri_qudp_hwio_def_ecpri_udp_fh_egress_config_p_s_v2 egress_config;
	ecpriss_flow_tx_cfg_s *flow_tx = NULL;
	int port_idx = 0;
	int ret = 0;

	if(packet == NULL) {
		return -1;
	}

	if(ecpriss_hw_ver != ECPRISS_HW_v2_0) {
		ECPRILOGERR("Not a V2 Hw %s\n",__func__);
		return -1;
	}

	flow_tx = &packet->flow_cfg.flow_tx_cfg;
	port_idx = flow_tx->port_index;

	memset(&l2_cfg,0, sizeof(l2_cfg));
	memset(&l3_cfg,0, sizeof(l3_cfg));
	memset(&egress_config,0,sizeof(egress_config));

	/*
	 * Below methode of reconfiguration is required when
	 * User want to reconfigure flows while traffic is flowing in the background.
	 */

	l2_cfg.redirect_from = flow_tx->qudp_tx_cfg.l2_hdr_tbl_idx;
	l2_cfg.redirect_to = ECPRISS_RESERVED_OVERRIDE_INDEX;

	if(flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

		l3_cfg.redirect_from =  flow_tx->qudp_tx_cfg.l3_hdr_tbl_idx;
		l3_cfg.redirect_to = ECPRISS_RESERVED_OVERRIDE_INDEX;
	}
	ECPRILOGINFO("L2 %u, L3 %u\n",l2_cfg.redirect_from,l3_cfg.redirect_from);

	/*
	 * We need to write this config at L2/L3 index ECPRISS_RESERVED_OVERRIDE_INDEX first
	 * save the original index and updat the config with ECPRISS_RESERVED_OVERRIDE_INDEX
	 */
	flow_tx->qudp_tx_cfg.l2_hdr_tbl_idx = ECPRISS_RESERVED_OVERRIDE_INDEX;

	if(flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

		flow_tx->qudp_tx_cfg.l3_hdr_tbl_idx = ECPRISS_RESERVED_OVERRIDE_INDEX;
	}

	/*
	 * Update the new config at index ECPRISS_RESERVED_OVERRIDE_INDEX
	 */
	ECPRILOGINFO("Reconfig L2 L3 idx %u %u \n",flow_tx->qudp_tx_cfg.l2_hdr_tbl_idx,flow_tx->qudp_tx_cfg.l2_hdr_tbl_idx);

	ret = ecpriss_qudp_fh_tx_hdr_ins_cfg_v2(
			flow_tx->port_index,
			&flow_tx->qudp_tx_cfg,
			ECPRISS_L2_L3_TRANSP);

	/*
	 * Read rigister before any configuration change
	 */
	ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
			port_idx,
			&egress_config);
	/*
	 * Only change override enable bit config
	 */
	egress_config.l2_encap_index_override_en = 1;

	if(flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

		egress_config.l3_encap_index_override_en = 1;
	}

	/*
	 * Update the HW registers with over from and override to index
	 * l2 index X to ECPRISS_RESERVED_OVERRIDE_INDEX, l3 index Y to ECPRISS_RESERVED_OVERRIDE_INDEX
	 */
	ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_L2_ENCAP_INDEX_OVERRIDE_P_V2,
			port_idx,
			&l2_cfg);

	if(flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

		ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
				ECPRI_UDP_FH_EGRESS_L3_ENCAP_INDEX_OVERRIDE_P_V2,
				port_idx,
				&l3_cfg);
	}

	/*
	 * Redirect the current L2 -> X connfig to ECPRISS_RESERVED_OVERRIDE_INDEX
	 * Redirect the current L3-> Y config to ECPRISS_RESERVED_OVERRIDE_INDEX
	 */
	ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
			port_idx,
			&egress_config);


	/*
	 * Restor the original l2/l3 table indexes
	 */
	flow_tx->qudp_tx_cfg.l2_hdr_tbl_idx = l2_cfg.redirect_from;

	if(flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

		flow_tx->qudp_tx_cfg.l3_hdr_tbl_idx = l3_cfg.redirect_from;
	}
	/*
	 * Now we need to update l2/l3 config at real index
	 */
	ret = ecpriss_qudp_fh_tx_hdr_ins_cfg_v2(
			flow_tx->port_index,
			&flow_tx->qudp_tx_cfg,
			ECPRISS_L2_L3_TRANSP);

	ecpriss_qudp_hal_read_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
			port_idx,
			&egress_config);

	if(flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

		egress_config.l3_encap_index_override_en = 0;
	}
	egress_config.l2_encap_index_override_en = 0;

	ecpriss_qudp_hal_write_reg_n_fields(ECPRISS_QUDP_FH,
			ECPRI_UDP_FH_EGRESS_CONFIG_P_V2,
			port_idx,
			&egress_config);

	/*
	 * Reset the ECPRISS_RESERVED_OVERRIDE_INDEX indexes with zero
	 */
	memset(&flow_tx->qudp_tx_cfg, 0,sizeof(ecpriss_qudp_tx_cfg_s));

	flow_tx->qudp_tx_cfg.l2_hdr_tbl_idx = ECPRISS_RESERVED_OVERRIDE_INDEX;

	if(flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype != ECPRISS_ETHERTYPE_ECPRI){

		flow_tx->qudp_tx_cfg.l3_hdr_tbl_idx = ECPRISS_RESERVED_OVERRIDE_INDEX;
	}

	/*
	 * Update the new config at index ECPRISS_RESERVED_OVERRIDE_INDEX
	 */
	ret = ecpriss_qudp_fh_tx_hdr_ins_cfg_v2(
			flow_tx->port_index,
			&flow_tx->qudp_tx_cfg,
			ECPRISS_L2_L3_TRANSP);

	return ret;
}

/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "ecpriss_core.h"
#include "ecpriss_netlink.h"
#include "ecpriss_workqueue.h"
#include "ecpriss_debugfs.h"
#include "ecpriss_log.h"
#include "ecpriss_mhi.h"
#include "ecpriss_flow.h"
#include <linux/notifier.h>
#include <linux/panic_notifier.h>
#include <linux/delay.h>

extern struct ecpri_dma_ecpri_ss_ops dma_ecpri_ss_driver_ops;
extern struct eth_ecpriss_ops mtip_ecpri_ops;

extern struct ecpri_delayed_work_q_params *ecpri_delay_wq_p;

#define ECPRISS_CORE_IPC_LOG_PAGES   50


/* Compile time flag for pre integration with DMA and ETH */
#define PRE_INT                       1

#define MAX_NUM_FLOW 120

int disable_xbar_dma_fh_same_prio = false;
module_param(disable_xbar_dma_fh_same_prio, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(disable_xbar_dma_fh_same_prio, "XBAR FH and DMA Priority Configuration");

int lte_fh_enabled = 0;
module_param(lte_fh_enabled, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(lte_fh_enabled, "Enable LTE FH");

int cascade_enable = 1;
module_param(cascade_enable, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(cascade_enable, "Enable Cascade Mode");

int ru_cascade_mode = 0;
module_param(ru_cascade_mode, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(ru_cascade_mode, "Enable RU Cascade Mode: routes C2C1 LUT to FH");

int enable_len_check = 1;
module_param(enable_len_check, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(enable_len_check, "Enable Length check for C/U plane packets");

int stats_timeout_ms = 250;
int ecpriss_qudp_strict_filt_cfg[MAX_PORTS] = {0,0,0};
void ecpriss_eth_topology_cb(void);
void ecpriss_eth_topology_cb_v2(void);
void ecpriss_dma_events_cb(void *user_data, enum ecpri_dma_event_type);
void ecpriss_dma_events_cb_v2(void *user_data, enum ecpri_dma_event_type);
void ecpriss_dma_endp_cb(void * user_data);
void ecpriss_stats_timer_cb(struct timer_list *data);
void ecpriss_dma_endp_cb_v2(void * user_data);
static int ecpriss_ssr_events_cb(struct notifier_block *this,unsigned long code, void *data);

void ecpriss_eth_events_cb(eth_ecpriss_event_e event_type,
		eth_ecpriss_link_event_params_s *link_event_params);
void ecpriss_configure_xbar_flush(ecpriss_port_type_e port_type,ecpriss_port_idx_e port_idx,eth_ecpriss_event_e event_type);
void ecpriss_configure_xbar_flush_v2(ecpriss_port_type_e port_type,ecpriss_port_idx_e port_idx,eth_ecpriss_event_e event_type);
void ecpriss_eth_events_cb_v2(eth_ecpriss_event_e event_type,
		eth_ecpriss_link_event_params_s *link_event_params);
void ecpriss_dma_ecpri_ss_log_msg_cb(void *user_data, const char *fmt, ...);
void ecpriss_dma_ecpri_ss_log_msg_cb_v2(void *user_data, const char *fmt, ...);

/***
 *
 * 'ecpriss_qudp_strict_filt_cfg' param takes an array as parameter
 * user need to pass three comma separted values for corresponding action.
 * values of each element can be either 0 or 1.
 * 1-> enable
 * 0-> disble
 *
***/

module_param_array(ecpriss_qudp_strict_filt_cfg, int,NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ecpriss_qudp_strict_filt_cfg, "configuration for qudp strict filtering for all ports");

ecpri_clock sys_clock;
ecpriss_core_private_s 	pdata;
ecpriss_core_private_s *ecpriss_pdata= &pdata;
ecpriss_xbar_ctx_s    	xbar_ctx_g;
ecpriss_qudp_ctx_s     	qudp_ctx_g;

ecpriss_core_private_s_v2 	pdata_v2;
ecpriss_core_private_s_v2 *ecpriss_pdata_v2= &pdata_v2;
ecpriss_xbar_ctx_s_v2    	xbar_ctx_g_v2;
ecpriss_qudp_ctx_s_v2     	qudp_ctx_g_v2;

ecpriss_hw_name_e ecpriss_hw_ver;

ecpriss_core_callback_flags_s      callback_flag_g;
struct ecpri_dma_endp_mapping      dma_endp_g;
eth_ecpriss_topology_root_s        eth_link_params_g;
/* void                                   *ecpriss_core_logbuf_g; */
ecpri_events_workqueue_params_s           events_workqueue_g;
ecpri_interrupt_workqueue_params_s        interrupts_workqueue_g;
eth_ecpriss_topology_ready_cb             eth_topology_ready_cb;
eth_ecpriss_interface_events_cb           eth_interface_events_cb;
struct ecpri_dma_ecpri_ss_register_params dma_ready_info;
eth_ecpriss_link_event_params_s           link_event_params;
/* ecpriss_stats_s                           stats_g; */
struct ecpriss_ssr_nb ssr_info_g;


/* DEbug Useful Data */
typedef struct {

	int tx_flow_cnt;
	int rx_flow_cnt;
	ecpriss_flow_tx_cfg_s  tx_cfg[MAX_NUM_FLOW];
	ecpriss_flow_rx_cfg_s  rx_cfg[MAX_NUM_FLOW];

}ecpri_flow_cfg;

ecpri_flow_cfg gecpri_flow_cfg = {0};

static int ecpriss_panic_notifier(struct notifier_block *this,unsigned long event, void *ptr)
{
	/*Panic Notifier Handler for Ecpriss Module*/
	if(ecpriss_hw_ver == ECPRISS_HW_v2_0){
		ecpriss_panic_notifr_handler_v2();
	}else {
		ecpriss_panic_notifr_handler();
	}
	return 0;
}

static struct notifier_block ecpriss_panic =
{
  .notifier_call  = ecpriss_panic_notifier,
};

static int ecpriss_core_remove(struct platform_device *pdev)
{
	if(ECPRISS_HW_v2_0 == ecpriss_hw_ver){
#ifndef NO_DEBUGFS_PERF
		clear_debugfs_directory();
#endif
		qcom_unregister_ssr_notifier(ecpriss_pdata_v2->ssr_info->notifier_handle,&ecpriss_pdata_v2->ssr_info->nb);
		atomic_notifier_chain_unregister(&panic_notifier_list, &ecpriss_panic);
		mtip_ecpri_ops.eth_ecpriss_deregister_events_cb();
		netlink_kernel_release(ecpriss_pdata_v2->netlink_socket);
		dma_ecpri_ss_driver_ops.ecpri_dma_ecpri_ss_deregister();
		ecpriss_qudp_irq_destroy_v2();
		ecpriss_xbar_destroy_interrupts_v2();
		ecpriss_destroy_workq();
		ecpriss_destroy_ipc_log_v2();
		ecpriss_unmap_xbar_qudp_v2();
	}
	return 0;
}

static int ecpriss_core_suspend(struct device *dev)
{
	return 0;
}

static int ecpriss_core_resume(struct device *dev)
{
	return 0;
}
static int ecpriss_core_get_hw_ver(struct platform_device *pdev)
{
	int result = 0;
	result = of_property_read_u32(pdev->dev.of_node, "qcom,ecpri-hw-ver",
	&ecpriss_hw_ver);

	ECPRILOGINFO("ecpriss_core: HW Version %d\n",ecpriss_hw_ver);
	return ecpriss_hw_ver;

}

int32_t ecpriss_process_packet_decfg(ecpriss_packet_payload_s *packet, ecpriss_message_id_e message_id)
{
	int ret=0;
	ecpriss_flow_rx_cfg_s *flow_rx = NULL;
	ecpriss_flow_tx_cfg_s *flow_tx = NULL;

	do{
		if(packet == NULL) {
			ret = -ENOMEM;
			break;
		}
		flow_tx = &packet->flow_cfg.flow_tx_cfg;

		switch((int)flow_tx->src){
			case ECPRISS_ROUTE_SRC_OC:
				if(ecpriss_hw_ver == ECPRISS_HW_v2_0) {

					ecpriss_qudp_remove_loopback_filters(packet);
					
					if(message_id == ECPRISS_MESSAGE_FLOW_DECFG) {

						ret = ecpriss_xbar_oc_rx_lut_decfg_v2(
								flow_tx->port_index,
								flow_tx);

						if(ret < 0) {
							ECPRILOGERR("OCRX lut de-configuration failed for port: %d, PCID:%d\n",flow_tx->port_index,flow_tx->xbar_tx_cfg.pcid);
							break;
						}
					}
					if(message_id == ECPRISS_MESSAGE_FLOW_TRANSP_DECFG) {
						ret = ecpriss_qudp_fh_tx_hdr_decfg_v2(
								flow_tx->port_index,
								&flow_tx->qudp_tx_cfg);

						if(ret < 0) {
							ECPRILOGERR("FHRX header de-configuration failed for port: %d\n",flow_tx->port_index);
							break;
						}

						ret = ecpriss_xbar_oc_rx_lut_decfg_v2(
								flow_tx->port_index,
								flow_tx);

						if(ret < 0) {
							ECPRILOGERR("OCRX lut de-configuration failed for port: %d, PCID:%d\n",flow_tx->port_index,flow_tx->xbar_tx_cfg.pcid);
							break;
						}
					}
				}


				break;

			case ECPRISS_ROUTE_SRC_FH:
				if(ecpriss_hw_ver == ECPRISS_HW_v2_0) {
					flow_rx = &packet->flow_cfg.flow_rx_cfg;
					if(message_id == ECPRISS_MESSAGE_FLOW_DECFG) {

						ret = ecpriss_xbar_fh_rx_lut_decfg_v2(
								flow_rx->port_index,
								flow_rx);

						if(ret < 0) {
							ECPRILOGERR("FHRX lut de-configuration failed for port: %d, PCID:%d\n",flow_tx->port_index,flow_rx->xbar_rx_cfg.flow_id);
							break;
						}
					}
					if (message_id == ECPRISS_MESSAGE_FLOW_TRANSP_DECFG) {
						ret = ecpriss_qudp_fh_rx_filter_decfg_v2(
								flow_rx->port_index,
								&flow_rx->qudp_rx_cfg);

						if(ret < 0) {
							ECPRILOGERR("QUDP filter de-configuration failed Port: %d\n",flow_rx->port_index);
							break;
						}
						ret = ecpriss_xbar_fh_rx_lut_decfg_v2(
								flow_rx->port_index,
								flow_rx);
						if(ret < 0) {
							ECPRILOGERR("FHRX lut de-configuration failed for port: %d, PCID:%d\n",flow_tx->port_index,flow_rx->xbar_rx_cfg.flow_id);
							break;
						}
					}
				}

				break;
			default:
				break;

		}
	}while (0);
	return ret;
}

/* Calls XBAR RX/TX and QUDP RX/TX depending on the msg_id of the packets */
int32_t ecpriss_process_packet(ecpriss_packet_payload_s *packet)
{
	int ret = 0;
	ecpriss_flow_rx_cfg_s *flow_rx = NULL;
	ecpriss_flow_tx_cfg_s *flow_tx = NULL;

	do{
		if(packet == NULL) {
			ret = -ENOMEM;
			break;
		}
		flow_tx = &packet->flow_cfg.flow_tx_cfg;

		switch((int)flow_tx->src){
			case ECPRISS_ROUTE_SRC_OC:

				memcpy(
					&gecpri_flow_cfg.tx_cfg[
					gecpri_flow_cfg.tx_flow_cnt % MAX_NUM_FLOW
					], flow_tx, sizeof(ecpriss_flow_tx_cfg_s));

				gecpri_flow_cfg.tx_flow_cnt++;

				if(ecpriss_hw_ver == ECPRISS_HW_v1_0) {

					ret = ecpriss_qudp_fh_tx_hdr_ins_cfg(
							flow_tx->port_index,
							&flow_tx->qudp_tx_cfg);
					if(ret < 0) {
						break;
					}

					ret = ecpriss_xbar_oc_rx_lut(
						flow_tx->port_index,
						flow_tx);

					if(ret < 0) {
						break;
					}


				}else {

					ret = ecpriss_qudp_fh_tx_hdr_ins_cfg_v2(
							flow_tx->port_index,
							&flow_tx->qudp_tx_cfg,
							ECPRISS_L2_L3_TRANSP);
					if(ret < 0) {
						break;
					}

					ret = ecpriss_xbar_oc_rx_lut_v2(
						flow_tx->port_index,
						flow_tx);

					if(ret < 0) {
						break;
					}
					ecpriss_qudp_add_loopback_filters(packet);
				}
				break;

			case ECPRISS_ROUTE_SRC_FH:

				flow_rx = &packet->flow_cfg.flow_rx_cfg;
				memcpy(&gecpri_flow_cfg.rx_cfg[
						gecpri_flow_cfg.rx_flow_cnt % MAX_NUM_FLOW],
						flow_rx , sizeof(ecpriss_flow_rx_cfg_s));
				gecpri_flow_cfg.rx_flow_cnt++;

				if(ecpriss_hw_ver == ECPRISS_HW_v1_0) {

					ret = ecpriss_qudp_fh_rx_filter_cfg(
							flow_rx->port_index,
							&flow_rx->qudp_rx_cfg);
					if(ret < 0) {
						break;
					}

					ret = ecpriss_xbar_fh_rx_lut(
							flow_rx->port_index,
							flow_rx);
					if(ret < 0) {
						break;
					}

				}else {

					ret = ecpriss_qudp_fh_rx_filter_cfg_v2(
							flow_rx->port_index,
							&flow_rx->qudp_rx_cfg);
					if(ret < 0) {
						break;
					}

					ret = ecpriss_xbar_fh_rx_lut_v2(
							flow_rx->port_index,
							flow_rx);
					if(ret < 0) {
						break;
					}

				}

				break;
			default:
				break;

		}
	}while (0);
	return ret;
}

void ecpriss_xbar_set_logging_route(ecpriss_log_cfg_s *log_cfg)
{
	int i,j;
	int num_of_pcid = 0;

	num_of_pcid = log_cfg->num_of_pcid;

	for(i=0;i<MAX_PORTS;i++)
	{
		for(j=0;j<num_of_pcid;j++)
		{
			ecpriss_xbar_fh_rx_lut_v2_logging(i, log_cfg->pcids[j], log_cfg->log_dir, log_cfg->action);
		}
	}

	ECPRILOGDBG("Configured UL and DL route to DMA for CP and UP packets in ecpriss_xbar_fh_rx_lut_v2_logging function for all ports for dir:%d and action:%d\n",log_cfg->log_dir, log_cfg->action);

	ECPRILOGDBG("PCID List is:\n");
	for(j=0;j<num_of_pcid;j++)
	{
		ECPRILOGDBG("%u ",log_cfg->pcids[j]);
	}
}

void ecpriss_xbar_set_l2_logging_route(ecpriss_log_cfg_s *log_cfg)
{
	int i=2,j;
	int num_of_pcid = 0;

	num_of_pcid = log_cfg->num_of_pcid;

	for(j=0;j<num_of_pcid;j++)
	{
		ecpriss_xbar_c2c_rx_lut_v2_logging(i, log_cfg->pcids[j], log_cfg->log_dir, log_cfg->action);
	}

	ECPRILOGDBG("Configured CP and UP packet route to DMA in ecpriss_xbar_c2c_rx_lut_v2_logging function for dir:%d and action:%d\n",log_cfg->log_dir,log_cfg->action);

	ECPRILOGDBG("PCID List is:\n");
	for(j=0;j<num_of_pcid;j++)
	{
		ECPRILOGDBG("%u ",log_cfg->pcids[j]);
	}
}

int32_t ecpri_send_logging_trigger_to_dma(ecpriss_log_cfg_s *log_cfg)
{
	if(log_cfg->action == ECPRISS_LOGGING_START)
	{
		ECPRILOGDBG("Sending Ingress Logging trigger to DMA for action:%d, dir:%d and log_buf_size:%u\n",log_cfg->action,log_cfg->log_dir,(log_cfg->log_buf_size)*1024);

		return (dma_ecpri_ss_driver_ops.ecpri_dma_ecpri_ss_start_oran_log)
			((log_cfg->log_buf_size)*1024, log_cfg->packet_size, ECPRI_DMA_ORAN_LOGGING_DIRECTION_INGRESS);
	}
	else
	{
		ECPRILOGDBG("Sending Ingress Logging trigger to DMA for action:%d and dir:%d\n",log_cfg->action,log_cfg->log_dir);

		return (dma_ecpri_ss_driver_ops.ecpri_dma_ecpri_ss_stop_oran_log)
			(ECPRI_DMA_ORAN_LOGGING_DIRECTION_INGRESS );
	}
}

int32_t ecpri_send_egress_logging_trigger_to_dma(ecpriss_log_cfg_s *log_cfg)
{
	if(log_cfg->action == ECPRISS_LOGGING_START)
	{
		ECPRILOGDBG("Sending Egress Logging trigger to DMA for action:%d, dir:%d and log_buf_size:%u\n",log_cfg->action,log_cfg->log_dir,(log_cfg->log_buf_size)*1024);

		return (dma_ecpri_ss_driver_ops.ecpri_dma_ecpri_ss_start_oran_log)
			((log_cfg->log_buf_size)*1024, log_cfg->packet_size, ECPRI_DMA_ORAN_LOGGING_DIRECTION_EGRESS);
	}
	else
	{
		ECPRILOGDBG("Sending Egress Logging trigger to DMA for action:%d and dir:%d\n",log_cfg->action,log_cfg->log_dir);
		return (dma_ecpri_ss_driver_ops.ecpri_dma_ecpri_ss_stop_oran_log)
			(ECPRI_DMA_ORAN_LOGGING_DIRECTION_EGRESS );
	}
}

int32_t ecpriss_configure_logging(ecpriss_packet_payload_s *packet)
{
	ecpriss_log_cfg_s *log_cfg = NULL;
	int ret = 0;

	do{
		if(packet == NULL) {
			ret = -ENOMEM;
			break;
		}

		log_cfg = &packet->flow_cfg.log_cfg;
		if(log_cfg == NULL)
		{
			ret = -ENOMEM;
			break;
		}

		if(log_cfg->log_dir == ECPRISS_LOG_DIR_DL)
		{
			if(ecpriss_pdata_v2->dev_mode == ECPRISS_DEV_MODE_RU){

				ecpriss_xbar_set_logging_route(log_cfg);
				ret = ecpri_send_logging_trigger_to_dma(log_cfg);
				if (ret < 0) {
					ECPRILOGERR("Ecpri Ingress Logging trigger to DMA failed\n");
					break;
				}
			}

			else if(ecpriss_pdata_v2->dev_mode == ECPRISS_DEV_MODE_DU_PCIE_3_X_12){

				ecpriss_xbar_set_l2_logging_route(log_cfg);
				ret = ecpri_send_egress_logging_trigger_to_dma(log_cfg);
				if (ret < 0) {
					ECPRILOGERR("Ecpri Egress Logging trigger to DMA failed\n");
					break;
				}
			}
		}

		else if(log_cfg->log_dir == ECPRISS_LOG_DIR_UL || log_cfg->log_dir == ECPRISS_LOG_DIR_UL_DL)
		{
			ecpriss_xbar_set_logging_route(log_cfg);
			ret = ecpri_send_logging_trigger_to_dma(log_cfg);
			if (ret < 0) {
				ECPRILOGERR("Ecpri Ingress logging trigger to DMA failed\n");
			}

			ecpriss_xbar_set_l2_logging_route(log_cfg);
			ret = ecpri_send_egress_logging_trigger_to_dma(log_cfg);
			if (ret < 0) {
				ECPRILOGERR("Ecpri Egress logging trigger to DMA failed\n");
			}
		}
	}while (0);

	return ret;
}

static void ecpriss_eth_cpy_params(ecpriss_qudp_port_cfg_s       *port_cfg,
		eth_ecpriss_topology_root_s    *eth_params,
		uint8_t                        port_index,
		uint8_t                        num_links,
		uint8_t                        topology_idx)
{
	uint8_t j;
	eth_ecpriss_port_params_s *port_params = NULL;

	if(port_cfg == NULL || eth_params == NULL) {
		return;
	}
	port_params =
	&eth_params->topology_params[topology_idx].port_params[port_index];

	for(j = 0;j<num_links;j++) {
		memcpy(&port_cfg->eth_cfg.link_params[j],
				&port_params->link_params[j],
				sizeof(port_cfg->eth_cfg.link_params[j]));
	}
	return;
}

static void ecpriss_eth_cpy_params_v2(ecpriss_qudp_port_cfg_s_v2       *port_cfg,
		eth_ecpriss_topology_root_s    *eth_params,
		uint8_t                        port_index,
		uint8_t                        num_links,
		uint8_t                        topology_idx)
{
	uint8_t j;
	eth_ecpriss_port_params_s *port_params = NULL;

	if(port_cfg == NULL || eth_params == NULL) {
		return;
	}
	port_params =
	&eth_params->topology_params[topology_idx].port_params[port_index];

	for(j = 0;j<num_links;j++) {
		memcpy(&port_cfg->eth_cfg.link_params[j],
				&port_params->link_params[j],
				sizeof(port_cfg->eth_cfg.link_params[j]));
	}
	return;
}


void ecpriss_eth_topology_init(void)
{
	int ret = 0;
	int i,j,k;
	eth_ecpriss_dev_mode_e device_mode;
	uint8_t port_index;
	uint8_t num_links;

	ecpriss_qudp_port_cfg_s      *port_cfg_local;
        eth_ecpriss_port_params_s *port_params = NULL;
        bool link_state_flag = false;
	do {
		ret = (mtip_ecpri_ops.eth_ecpriss_get_topology)(&device_mode,
				&eth_link_params_g);
		if(ret < 0) {
			break;
		}

		for(i=0;i<eth_link_params_g.num_unique_port_types;i++) {
			if(eth_link_params_g.topology_params[i].port_type ==
					ETH_ECPRISS_PORT_TYPE_FH) {
				ecpriss_pdata->qudp_ctx->num_ports =
				eth_link_params_g.topology_params[i].num_ports;
				for(j=0;j<ecpriss_pdata->qudp_ctx->num_ports;j++){
					port_index =
					eth_link_params_g.topology_params[i].port_params[j].port_index;
					port_cfg_local =
						&ecpriss_pdata->qudp_ctx->fh_port_cfg[port_index];
					num_links =
						eth_link_params_g.topology_params[i].port_params[j].num_links;
					port_params = &eth_link_params_g.topology_params[i].port_params[port_index];
                                        for(k=0;k<num_links;k++){
						//pr_err("port_index: %d link_index: %d link state: %d\n",port_index,k,port_params->link_params[k].link_state);
                                                if(port_params->link_params[k].link_state == ETH_ECPRISS_LINK_STATE_UP){
                                                        link_state_flag = true;
	                                        }
                                        }
                                        if(link_state_flag == true){
                                                ecpriss_configure_xbar_flush(ECPRISS_PORT_TYPE_FH,port_index,ETH_ECPRISS_EVENT_UP);
                                        }
                                        else{
                                                ecpriss_configure_xbar_flush(ECPRISS_PORT_TYPE_FH,port_index,ETH_ECPRISS_EVENT_DOWN);
                                        }
					link_state_flag = false;
					ecpriss_eth_cpy_params(port_cfg_local,
							&eth_link_params_g,
							port_index,
							num_links,
							i);
				}
			}
			else if(eth_link_params_g.topology_params[i].port_type ==
					ETH_ECPRISS_PORT_TYPE_C2C) {
				ecpriss_pdata->qudp_ctx->num_ports =
					eth_link_params_g.topology_params[i].num_ports;
				for(j=0;j<ecpriss_pdata->qudp_ctx->num_ports;j++) {
					port_index =
						eth_link_params_g.topology_params[i].port_params[j].port_index;
					port_cfg_local =
						&ecpriss_pdata->qudp_ctx->c2c_port_cfg[port_index];
					num_links =
						eth_link_params_g.topology_params[i].port_params[j].num_links;
					ecpriss_eth_cpy_params(port_cfg_local,
							&eth_link_params_g,
							port_index,
							num_links,
							i);
				}
			}
			else if(eth_link_params_g.topology_params[i].port_type ==
					ETH_ECPRISS_PORT_TYPE_L2) {
				ecpriss_pdata->qudp_ctx->num_ports =
					eth_link_params_g.topology_params[i].num_ports;
				for(j=0;j<ecpriss_pdata->qudp_ctx->num_ports;j++) {
					port_index =
						eth_link_params_g.topology_params[i].port_params[j].port_index;
					port_cfg_local =
						&ecpriss_pdata->qudp_ctx->l2_port_cfg[port_index];
					num_links =
						eth_link_params_g.topology_params[i].port_params[j].num_links;
					ecpriss_eth_cpy_params(port_cfg_local,
							&eth_link_params_g,
							port_index,
							num_links,
							i);
				}
			}
		}
	}while (0);

	ecpriss_pdata->eth_topology_params->eth_topology_init_done = 1;
	return;
}

void ecpriss_eth_topology_init_v2(void)
{
	int ret = 0;
	int i,j,k;
	eth_ecpriss_dev_mode_e device_mode;
	uint8_t port_index;
	uint8_t num_links;

	ecpriss_qudp_port_cfg_s_v2      *port_cfg_local;
        eth_ecpriss_port_params_s *port_params = NULL;
        bool link_state_flag = false;

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
                                        for(k=0;k<num_links;k++){
						//pr_err("FH:port_index: %d link_index: %d link state: %d\n",port_index,k,port_params->link_params[k].link_state);

                                                if(port_params->link_params[k].link_state == ETH_ECPRISS_LINK_STATE_UP){
                                                        link_state_flag = true;
	                                        }
                                        }

                                        if(link_state_flag == true){
                                                ecpriss_configure_xbar_flush_v2(ECPRISS_PORT_TYPE_FH,port_index,ETH_ECPRISS_EVENT_UP);
                                        }
                                        else{
                                                ecpriss_configure_xbar_flush_v2(ECPRISS_PORT_TYPE_FH,port_index,ETH_ECPRISS_EVENT_DOWN);
                                        }
					link_state_flag = false;
					ecpriss_eth_cpy_params_v2(port_cfg_local,
							&eth_link_params_g,
							port_index,
							num_links,
							i);
				}
			}
			if(eth_link_params_g.topology_params[i].port_type ==
					ETH_ECPRISS_PORT_TYPE_C2C) {
				ecpriss_pdata_v2->qudp_ctx_v2->num_ports[ETH_ECPRISS_PORT_TYPE_C2C] =
				eth_link_params_g.topology_params[i].num_ports;
				for(j=0;j<ecpriss_pdata_v2->qudp_ctx_v2->num_ports[ETH_ECPRISS_PORT_TYPE_C2C];j++){
					link_state_flag = false;
					port_index =
					eth_link_params_g.topology_params[i].port_params[j].port_index;
					port_cfg_local =
						&ecpriss_pdata_v2->qudp_ctx_v2->c2c_port_cfg_v2[port_index];
					num_links =
						eth_link_params_g.topology_params[i].port_params[j].num_links;

					port_params = &eth_link_params_g.topology_params[i].port_params[port_index];
                                        for(k=0;k<num_links;k++){
						//pr_err("C2C:port_index: %d link_index: %d link state: %d\n",port_index,k,port_params->link_params[k].link_state);
                                                if(port_params->link_params[k].link_state == ETH_ECPRISS_LINK_STATE_UP){
                                                        link_state_flag = true;
	                                        }
                                        }

                                        if(link_state_flag == true){
                                                ecpriss_configure_xbar_flush_v2(ECPRISS_PORT_TYPE_C2C,2,ETH_ECPRISS_EVENT_UP);
                                        }
                                        else{
                                                ecpriss_configure_xbar_flush_v2(ECPRISS_PORT_TYPE_C2C,2,ETH_ECPRISS_EVENT_DOWN);
                                        }
					ecpriss_eth_cpy_params_v2(port_cfg_local,
							&eth_link_params_g,
							port_index,
							num_links,
							i);
				}
			}


		}
	}while (0);

	ecpriss_pdata_v2->eth_topology_params->eth_topology_init_done = 1;
	return;
}


void ecpriss_eth_link_update_for_mhi_v2(void)
{
	int ret = 0;
	int i,j,k;
	eth_ecpriss_dev_mode_e device_mode;
	uint8_t port_index;
	uint8_t num_links;

	ecpriss_qudp_port_cfg_s_v2      *port_cfg_local;
	eth_ecpriss_port_params_s *port_params = NULL;
	bool link_state_flag = false;

	do {
		ret = (mtip_ecpri_ops.eth_ecpriss_get_topology)(&device_mode,
				&eth_link_params_g);
		if(ret < 0) {
			break;
		}

		for(i=0;i<eth_link_params_g.num_unique_port_types;i++) {

			if(eth_link_params_g.topology_params[i].port_type == ETH_ECPRISS_PORT_TYPE_FH) {
				ecpriss_pdata_v2->qudp_ctx_v2->num_ports[ETH_ECPRISS_PORT_TYPE_FH] =
				eth_link_params_g.topology_params[i].num_ports;

				for(j=0;j<ecpriss_pdata_v2->qudp_ctx_v2->num_ports[ETH_ECPRISS_PORT_TYPE_FH];j++){

					port_index = eth_link_params_g.topology_params[i].port_params[j].port_index;
					port_cfg_local = &ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_index];
					num_links = eth_link_params_g.topology_params[i].port_params[j].num_links;
					port_params = &eth_link_params_g.topology_params[i].port_params[port_index];

					for(k=0;k<num_links;k++){

						if(port_params->link_params[k].link_state == ETH_ECPRISS_LINK_STATE_UP){
							link_state_flag = true;
						}
						
						if(lte_fh_enabled) {
							ECPRILOGINFO("Port = %d, Link = %d, Link state = %d\n", port_index, k, port_params->link_params[k].link_state);
							ecpriss_mhi_process_async_link_state(port_index, k, port_params->link_params[k].link_state);
						}
					}

				}
			}
		}
	}while (0);
	return;
}


void ecpriss_eth_event_processing(void)
{
	if(ecpriss_hw_ver == ECPRISS_HW_v1_0){
		ecpriss_eth_topology_init();
	}else {
		ecpriss_eth_topology_init_v2();
		if(ecpriss_pdata_v2->dev_mode != ECPRISS_DEV_MODE_RU && lte_fh_enabled
				&& ecpriss_pdata_v2->ecpri_state == ECPRI_CORE_INIT) {

			ecpriss_qudp_set_nr_mac_filter();
			ecpriss_eth_link_update_for_mhi_v2();
		}

		if(ecpriss_pdata_v2->dev_mode == ECPRISS_DEV_MODE_RU && ru_cascade_mode
				&& ecpriss_pdata_v2->ecpri_state == ECPRI_CORE_INIT) {

			ecpriss_qudp_set_nr_mac_filter();
		}
	}
	return;
}


void ecpriss_eth_topology_init_wq(struct work_struct *work)
{
	int ret = 0;
	bool enable_c2c2 = true;

	if(ecpriss_hw_ver == ECPRISS_HW_v1_0){
		ecpriss_eth_topology_init();
	}else {
		ecpriss_eth_topology_init_v2();

		if(ecpriss_pdata_v2->dev_mode != ECPRISS_DEV_MODE_RU && lte_fh_enabled
				&& ecpriss_pdata_v2->ecpri_state == ECPRI_CORE_INIT) {

			ecpriss_qudp_set_nr_mac_filter();
		}

		if(ecpriss_pdata_v2->dev_mode == ECPRISS_DEV_MODE_RU && ru_cascade_mode
				&& ecpriss_pdata_v2->ecpri_state == ECPRI_CORE_INIT) {

			ecpriss_qudp_set_nr_mac_filter();
		}

	ECPRILOGINFO("dev_mode=%d\n",ecpriss_pdata_v2->dev_mode);
	if(ecpriss_pdata_v2->dev_mode == ECPRISS_DEV_MODE_RU || ecpriss_pdata_v2->dev_mode
			== ECPRISS_DEV_MODE_DU_PCIE_3_X_12) {
		if (ru_cascade_mode) {
			ECPRILOGINFO("Bringing up C2C2 and C2C1 in E2E mode (ru_cascade_mode=%d)\n",
				     ru_cascade_mode);
			ret = (mtip_ecpri_ops.eth_ecpriss_enable_ru_cascade_c2c_bringup)();
			if(ret != 0) {
				ECPRILOGERR("C2C E2E bringup failed\n");
			}
		} else {
			ECPRILOGINFO("Bringing up C2C2 port in loopback mode\n");
			ret = (mtip_ecpri_ops.eth_ecpriss_enable_logging_port)(enable_c2c2);
			if(ret != 0) {
				ECPRILOGERR("C2C2 bringup failed\n");
			}
		}
	}

	}
	return;
}

static int ecpriss_dma_endp_config(void)
{
	int ret = 0;
	int i,j;


	memset(&dma_endp_g , 0 , sizeof(dma_endp_g));

	do{
		ret = (dma_ecpri_ss_driver_ops.ecpri_dma_ecpri_ss_get_endp_mapping)(&dma_endp_g);
		if(ret < 0) {
			break;
		}

		ecpriss_pdata->dev_mode = (ecpriss_dev_mode_e)dma_endp_g.flv;
		ecpriss_pdata->xbar_ctx->num_of_port_types = dma_endp_g.num_of_port_types;

		for(i=0;i<dma_endp_g.num_of_port_types;i++) {
			if(dma_endp_g.topology_params[i].port_type ==
					ECPRI_DMA_ENDP_STREAM_DEST_FH) {
				for(j=0;j<dma_endp_g.topology_params[i].num_of_ports;j++) {
					memcpy(&ecpriss_pdata->xbar_ctx->fh_port_cfg.dma_port_cfg[j],
							&dma_endp_g.topology_params[i].dma_port_param[j],
							sizeof(struct ecpri_dma_port_params));
				}
			}
			else if(dma_endp_g.topology_params[i].port_type ==
					ECPRI_DMA_ENDP_STREAM_DEST_C2C) {
				for(j=0;j<dma_endp_g.topology_params[i].num_of_ports;j++) {
					memcpy(&ecpriss_pdata->xbar_ctx->c2c_port_cfg.dma_port_cfg[j],
							&dma_endp_g.topology_params[i].dma_port_param[j],
							sizeof(struct ecpri_dma_port_params));
				}
			}
			else if (dma_endp_g.topology_params[i].port_type ==
					ECPRI_DMA_ENDP_STREAM_DEST_L2) {
				for(j=0;j<dma_endp_g.topology_params[i].num_of_ports;j++) {
					memcpy(&ecpriss_pdata->xbar_ctx->l2_port_cfg.dma_port_cfg[j],
							&dma_endp_g.topology_params[i].dma_port_param[j],
							sizeof(struct ecpri_dma_port_params));
				}
			}
			else if(dma_endp_g.topology_params[i].port_type ==
					ECPRI_DMA_ENDP_STREAM_DEST_FH_EXCEPTION) {
				for(j=0;j<dma_endp_g.topology_params[i].num_of_ports;j++) {
					memcpy(&ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[j],
							&dma_endp_g.topology_params[i].dma_port_param[j],
							sizeof(struct ecpri_dma_port_params));
				}
			}
		}
	}while (0);

					/* Set the non ecpri LUT Cfg */
					ecpriss_xbar_non_ecpri_lut_cfg();
					return ret;
}

static int ecpriss_dma_endp_config_v2(void)
{
	int ret = 0;
	int i,j;
	int lte_fh_index = 0;

	memset(&dma_endp_g , 0 , sizeof(dma_endp_g));

	do{
		ret = (dma_ecpri_ss_driver_ops.ecpri_dma_ecpri_ss_get_endp_mapping)(&dma_endp_g);
		if(ret < 0) {
			break;
		}

		ecpriss_pdata_v2->dev_mode = (ecpriss_dev_mode_e)dma_endp_g.flv;
		ecpriss_pdata_v2->xbar_ctx_v2->num_of_port_types = dma_endp_g.num_of_port_types;

		for(i=0;i<dma_endp_g.num_of_port_types;i++) {
			if(dma_endp_g.topology_params[i].port_type ==
					ECPRI_DMA_ENDP_STREAM_DEST_FH) {
				for(j=0;j<dma_endp_g.topology_params[i].num_of_ports;j++) {
					memcpy(&ecpriss_pdata_v2->xbar_ctx_v2->fh_port_cfg.dma_port_cfg[j],
							&dma_endp_g.topology_params[i].dma_port_param[j],
							sizeof(struct ecpri_dma_port_params));
				}
			}
			else if (dma_endp_g.topology_params[i].port_type ==
					ECPRI_DMA_ENDP_STREAM_DEST_L2) {
				for(j=0;j<dma_endp_g.topology_params[i].num_of_ports;j++) {
					memcpy(&ecpriss_pdata_v2->xbar_ctx_v2->l2_port_cfg.dma_port_cfg[j],
							&dma_endp_g.topology_params[i].dma_port_param[j],
							sizeof(struct ecpri_dma_port_params));
				}
			}
			else if(dma_endp_g.topology_params[i].port_type ==
					ECPRI_DMA_ENDP_STREAM_DEST_FH_EXCEPTION) {
				for(j=0;j<dma_endp_g.topology_params[i].num_of_ports;j++) {
					memcpy(&ecpriss_pdata_v2->xbar_ctx_v2->fh_exception_port_cfg.dma_port_cfg[j],
							&dma_endp_g.topology_params[i].dma_port_param[j],
							sizeof(struct ecpri_dma_port_params));
				}
			}
			else if(dma_endp_g.topology_params[i].port_type ==
					ECPRI_DMA_ENDP_STREAM_DEST_FH_LTE) {
				for(j=0;j<dma_endp_g.topology_params[i].num_of_ports;j++) {
					memcpy(&ecpriss_pdata_v2->xbar_ctx_v2->fh_lte_port_cfg[lte_fh_index].dma_port_cfg[j],
							&dma_endp_g.topology_params[i].dma_port_param[j],
							sizeof(struct ecpri_dma_port_params));
				}
				lte_fh_index++;
			}
			else if(dma_endp_g.topology_params[i].port_type ==
					ECPRI_DMA_ENDP_STREAM_DEST_C2C) {
				for(j=0;j<dma_endp_g.topology_params[i].num_of_ports;j++) {
					memcpy(&ecpriss_pdata_v2->xbar_ctx_v2->c2c_port_cfg.dma_port_cfg[j],
							&dma_endp_g.topology_params[i].dma_port_param[j],
							sizeof(struct ecpri_dma_port_params));
				}
			}
			else if (dma_endp_g.topology_params[i].port_type ==
					ECPRI_DMA_ENDP_STREAM_DEST_ORAN_LOG) {
				for(j=0;j<dma_endp_g.topology_params[i].num_of_ports;j++) {
					memcpy(&ecpriss_pdata_v2->xbar_ctx_v2->oran_log_port_cfg.dma_port_cfg[j],
							&dma_endp_g.topology_params[i].dma_port_param[j],
							sizeof(struct ecpri_dma_port_params));
				}
			}
		}
	}while (0);

					return ret;
}


int ecpriss_get_link_state(eth_ecpriss_port_type_e port_type,
		int port, int link)
{
	eth_ecpriss_link_state_e state;

	if(port < 0 || port >= ECPRISS_MAX_PORTS)
		return -1;

	if(link < 0 || link >= ECPRISS_MAX_LINKS)
		return -1;

	if(port_type < 0 || port_type >= ECPRISS_MAX_PORT_TYPE)
		return -1;

	if(!ecpriss_pdata_v2)
		return -1;

	state = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port].eth_cfg.link_params[link].link_state;


	ECPRILOGINFO("ecpriss_get_link_state: Ptype:%d Port:%d Link:%d state:%d",
			port_type, port, link, state);

	return state;

}

int ecpriss_get_link_rate(eth_ecpriss_port_type_e port_type,
		int port, int link)
{
	eth_ecpriss_link_rate_e link_rate;

	if(port < 0 || port >= ECPRISS_MAX_PORTS)
		return -1;

	if(link < 0 || link >= ECPRISS_MAX_LINKS)
		return -1;

	if(port_type < 0 || port_type >= ECPRISS_MAX_PORT_TYPE)
		return -1;

	link_rate = ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port].eth_cfg.link_params[link].link_rate;


	ECPRILOGINFO("ecpriss_get_link_state: Ptype:%d Port:%d Link:%d rate:%d",
			port_type, port, link, link_rate);

	return link_rate;

}


void ecpriss_dma_event_processing_wq(struct work_struct *work)
{
	if(work == NULL) {
		return;
	}

	if(ecpriss_hw_ver == ECPRISS_HW_v1_0){
		ecpriss_dma_endp_config();
	}else {
		ecpriss_dma_endp_config_v2();
	}
	return;
}


void ecpriss_ssr_events_processing_wq(struct work_struct *work)
{
	uint32_t val = -1;
	if(work == NULL) {
		return;
	}
	val = atomic_read(&ecpriss_pdata_v2->ssr_info->curr_ssr_state);

	ecpriss_xbar_oc_flush_enable(val);

	return;
}
void ecpriss_interrupt_events_processing_wq(struct work_struct *work)
{
	if(work == NULL) {
		return;
	}
	if(ecpriss_hw_ver == ECPRISS_HW_v1_0){
		ecpriss_update_all_stats();
		ecpriss_stats_timer_enable(stats_timeout_ms);
	}else {
		ecpriss_update_all_stats_v2();
	}

	return;
}
void ecpriss_update_stats_and_requeue(struct work_struct *work)
{
	ecpriss_update_all_stats_v2();

	ecpriss_queue_delayed_work(&ecpri_delay_wq_p->wq_item,stats_timeout_ms);

	return;
}

void ecpriss_dma_events_cb(void *user_data, enum ecpri_dma_event_type evt)
{
	return;
}
void ecpriss_dma_events_cb_v2(void *user_data, enum ecpri_dma_event_type evt)
{
	return;
}


int ecpriss_ssr_events_cb(struct notifier_block *this,unsigned long code, void *data)
{
	int ret = 0;
	struct workqueue_struct *ecpriss_wq = NULL;
	struct work_struct *ecpriss_work = NULL;

	ECPRILOGINFO("received %ld for %s\n", code,ecpriss_pdata_v2-> ssr_info->ssr_label);

	switch (code) {
		case QCOM_SSR_AFTER_SHUTDOWN:
			atomic_set(&ecpriss_pdata_v2->ssr_info->curr_ssr_state,code);
			break;
	 	case QCOM_SSR_AFTER_POWERUP:
			atomic_set(&ecpriss_pdata_v2->ssr_info->curr_ssr_state,code);
			break;
		default:
			return NOTIFY_DONE;
	}

	do {
		ecpriss_pdata_v2->callback_flag->ssr_callback_rcvd = 1;
		ecpriss_wq =
			ecpriss_pdata_v2->events_workqueue->kernel_events_workqueue;
		ecpriss_work =
			ecpriss_pdata_v2->events_workqueue->ecpriss_ssr_events_rdy_work;

		if(ecpriss_wq == NULL || ecpriss_wq == NULL) {
			ECPRILOGERR("ecpriss_ssr_events_cb: NULL Wq or Work");
			break;
		}

		ret = ecpriss_queue_work(ecpriss_wq,ecpriss_work);

		if(ret < 0) {
			ECPRILOGERR("Queue work failed\n");
			break;
		}
	}while (0);

 	return NOTIFY_DONE;
}
void ecpriss_eth_topology_cb(void)
{
	int ret=0;
	struct workqueue_struct *ecpriss_wq = NULL;
	struct work_struct *ecpriss_work = NULL;
	do {

		ecpriss_pdata->callback_flag->eth_link_callback_rcvd = 1;
		ecpriss_wq =
		ecpriss_pdata->events_workqueue->kernel_events_workqueue;
		ecpriss_work =
	ecpriss_pdata->events_workqueue->ecpriss_eth_topology_events_rdy_work;
		ret = ecpriss_queue_work(ecpriss_wq,
				ecpriss_work);
		if(ret < 0) {
			ECPRILOGERR("Queue work failed\n");
			break;
		}
	} while (0);
	return;
}

void ecpriss_eth_topology_cb_v2(void)
{
	int ret=0;
	struct workqueue_struct *ecpriss_wq = NULL;
	struct work_struct *ecpriss_work = NULL;
	do {

		ecpriss_pdata_v2->callback_flag->eth_link_callback_rcvd = 1;
		ecpriss_wq =
		ecpriss_pdata_v2->events_workqueue->kernel_events_workqueue;
		ecpriss_work =
	ecpriss_pdata_v2->events_workqueue->ecpriss_eth_topology_events_rdy_work;

		if(ecpriss_wq == NULL || ecpriss_wq == NULL) {
			ECPRILOGERR("ecpriss_eth_topology_cb_v2:NULL Wq or Work");
			break;
		}
		ret = ecpriss_queue_work(ecpriss_wq,
				ecpriss_work);
		if(ret < 0) {
			ECPRILOGERR("Queue work failed\n");
			break;
		}
	} while (0);
	return;
}
void ecpriss_eth_events_cb(eth_ecpriss_event_e event_type,
		eth_ecpriss_link_event_params_s *link_event_params)
{
	int ret = 0;
	struct workqueue_struct *ecpriss_wq;
	struct work_struct *ecpriss_work;

	ECPRILOGDBG("ecpriss_eth_events_cb event received %d", event_type);


	do{
		if(link_event_params == NULL) {

		}
		ecpriss_wq =
		ecpriss_pdata->events_workqueue->kernel_events_workqueue;
		ecpriss_work =
		ecpriss_pdata->events_workqueue->ecpriss_eth_events_rdy_work;
		ret = ecpriss_queue_work(ecpriss_wq,
				ecpriss_work);
		if(ret < 0) {
			ECPRILOGERR("Queue work failed\n");
			break;
		}

	} while (0);
	return;
}
void ecpriss_eth_events_cb_v2(eth_ecpriss_event_e event_type,
		eth_ecpriss_link_event_params_s *link_event_params)
{

	ECPRILOGDBG("ecpriss_eth_events_cb_v2 event received %d", event_type);

	ecpriss_eth_event_processing();

	return;
}


void ecpriss_dma_endp_cb(void * userdata)
{
	int ret =0;
	struct workqueue_struct    *ecpriss_wq;
	struct work_struct         *ecpriss_work;
	do{
		ecpriss_pdata->callback_flag->dma_callback_rcvd = 1;
		ecpriss_wq =
		ecpriss_pdata->events_workqueue->kernel_events_workqueue;
		ecpriss_work =
		ecpriss_pdata->events_workqueue->ecpriss_dma_events_rdy_work;
		ret = ecpriss_queue_work(ecpriss_wq,
				ecpriss_work);
		if(ret < 0) {
			ECPRILOGERR("Queue work failed\n");
			break;
		}
	}while (0);
	return;
}

void ecpriss_stats_timer_cb(struct timer_list *data)
{
	int ret = 0;
	struct workqueue_struct    *ecpriss_wq;
	struct work_struct         *ecpriss_work;

	do{
		ecpriss_wq =
		ecpriss_pdata->interrupts_workqueue->ecpriss_interrupts_workq;
		ecpriss_work =
		ecpriss_pdata->interrupts_workqueue->ecpriss_interrupt_events_rdy_work;
		ret = ecpriss_queue_work(ecpriss_wq,
				ecpriss_work);
		if(ret < 0) {
			ECPRILOGERR("Queue work failed\n");
			break;
		}

	}while (0);
	return;
}
void ecpriss_stats_timer_cb_v2(struct timer_list *data)
{
	int ret = 0;
	struct workqueue_struct    *ecpriss_wq;
	struct work_struct         *ecpriss_work;

	do{
		ecpriss_wq =
		ecpriss_pdata_v2->interrupts_workqueue->ecpriss_interrupts_workq;
		ecpriss_work =
		ecpriss_pdata_v2->interrupts_workqueue->ecpriss_interrupt_events_rdy_work;

		if(ecpriss_wq == NULL || ecpriss_wq == NULL) {
			ECPRILOGERR("ecpriss_stats_timer_cb_v2: NULL Wq or Work");
			break;
		}

		ret = ecpriss_queue_work(ecpriss_wq,
				ecpriss_work);
		if(ret < 0) {
			ECPRILOGERR("Queue work failed\n");
			break;
		}

	}while (0);
	return;

}
void ecpriss_dma_endp_cb_v2(void * userdata)
{
	int ret = 0;
	struct workqueue_struct    *ecpriss_wq;
	struct work_struct         *ecpriss_work;
	ECPRILOGERR("ecpriss dma endp cb 2\n");
	do{
		ecpriss_pdata_v2->callback_flag->dma_callback_rcvd = 1;
		ecpriss_wq =
		ecpriss_pdata_v2->events_workqueue->kernel_events_workqueue;
		ecpriss_work =
		ecpriss_pdata_v2->events_workqueue->ecpriss_dma_events_rdy_work;

		if(ecpriss_wq == NULL || ecpriss_wq == NULL) {
			ECPRILOGERR("ecpriss_dma_endp_cb_v2: NULL Wq or Work");
			break;
		}
		ret = ecpriss_queue_work(ecpriss_wq,
				ecpriss_work);
		if(ret < 0) {
			ECPRILOGERR("Queue work failed\n");
			break;
		}
	}while (0);
	return;
}



void ecpriss_eth_event_processing_wq(struct work_struct *work)
{
	ecpriss_eth_event_processing();
}

void ecpriss_dma_ecpri_ss_log_msg_cb(void *user_data, const char *fmt, ...)
{
	return ;
}

void ecpriss_dma_ecpri_ss_log_msg_cb_v2(void *user_data, const char *fmt, ...)
{
	return ;
}
void ecpriss_panic_notifr_handler(void)
{
    int i;
	ECPRILOGERR("ecpriss_hw_ver : %u",ecpriss_hw_ver);

	ECPRILOGERR("ecpriss_pdata->dev_mode : %u",ecpriss_pdata->dev_mode);

	ECPRILOGERR("ecpriss_pdata->callback_flag->eth_link_callback_rcvd  : %u ",ecpriss_pdata->callback_flag->eth_link_callback_rcvd);
	ECPRILOGERR("ecpriss_pdata->callback_flag->dma_callback_rcvd : %u ",ecpriss_pdata->callback_flag->dma_callback_rcvd);
	ECPRILOGERR("ecpriss_pdata->callback_flag->ssr_callback_rcvd : %u ",ecpriss_pdata->callback_flag->ssr_callback_rcvd);
	ECPRILOGERR("ecpriss_pdata->callback_flag->macsec_callback_rcvd : %u ",ecpriss_pdata->callback_flag->macsec_callback_rcvd);

	ECPRILOGERR("ecpriss_pdata->qudp_ctx->state : %u",ecpriss_pdata->qudp_ctx->state);
	ECPRILOGERR("ecpriss_pdata->xbar_ctx->state : %u",ecpriss_pdata->xbar_ctx->state);
	ECPRILOGERR("ecpriss_pdata->state : %u",ecpriss_pdata->ecpri_state);

	for(i=0;i<XBAR_LINKS;i++)
	{
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_octx_pkt_cnt[%d]  : %u",i,ecpriss_pdata->xbar_ctx->stats.xbar_octx_pkt_cnt[i]);
        ECPRILOGERR(" ecpriss_pdata->xbar_ctx->stats.xbar_ocrx_pkt_cnt[%d] : %u",i,ecpriss_pdata->xbar_ctx->stats.xbar_ocrx_pkt_cnt[i]);
	}

	ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh0  : %u ",ecpriss_pdata->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh0);
	ECPRILOGERR("Vecpriss_pdata->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh1 : %u ",ecpriss_pdata->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh1);
	ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh2  : %u ",ecpriss_pdata->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh2);

	ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.octx_oc_0_1_buff_watermark_cc0 : %u ",ecpriss_pdata->xbar_ctx->stats.octx_oc_0_1_buff_watermark_cc0);
	ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.octx_oc_0_1_buff_watermark_cc1 : %u ",ecpriss_pdata->xbar_ctx->stats.octx_oc_0_1_buff_watermark_cc1);
	ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.octx_oc_2_3_buff_watermark_cc2 : %u ",ecpriss_pdata->xbar_ctx->stats.octx_oc_2_3_buff_watermark_cc2);
	ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.octx_oc_2_3_buff_watermark_cc3 : %u ",ecpriss_pdata->xbar_ctx->stats.octx_oc_2_3_buff_watermark_cc3);

	ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_0_1_buff_watermark_cc0 : %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_0_1_buff_watermark_cc0);
	ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_0_1_buff_watermark_cc1 : %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_0_1_buff_watermark_cc1);
	ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_2_3_buff_watermark_cc2 : %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_2_3_buff_watermark_cc2);
	ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_2_3_buff_watermark_cc3 : %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_2_3_buff_watermark_cc3);
}
void ecpriss_panic_notifr_handler_v2(void)
{
    int i;
    int *flush_state = NULL;

	ECPRILOGERR("ecpriss_hw_ver : %u",ecpriss_hw_ver);

	ECPRILOGERR("ecpriss_pdata_v2->dev_mode : %u",ecpriss_pdata_v2->dev_mode);

	ECPRILOGERR("ecpriss_pdata_v2->callback_flag->eth_link_callback_rcvd  : %u ",ecpriss_pdata_v2->callback_flag->eth_link_callback_rcvd);
	ECPRILOGERR("ecpriss_pdata_v2->callback_flag->dma_callback_rcvd : %u ",ecpriss_pdata_v2->callback_flag->dma_callback_rcvd);
	ECPRILOGERR("ecpriss_pdata_v2->callback_flag->ssr_callback_rcvd : %u ",ecpriss_pdata_v2->callback_flag->ssr_callback_rcvd);
	ECPRILOGERR("ecpriss_pdata_v2->callback_flag->macsec_callback_rcvd : %u ",ecpriss_pdata_v2->callback_flag->macsec_callback_rcvd);

	ECPRILOGERR("ecpriss_pdata_v2->qudp_ctx->state : %u",ecpriss_pdata_v2->qudp_ctx_v2->state);
	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->state : %u",ecpriss_pdata_v2->xbar_ctx_v2->state);
	ECPRILOGERR("ecpriss_pdata_v2->state : %u",ecpriss_pdata_v2->ecpri_state);
	ECPRILOGERR("ecpriss_pdata_v2->ssr_state : %u",ecpriss_pdata_v2->ssr_info->curr_ssr_state);

	for(i=0;i<XBAR_LINKS;i++)
	{
		ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_fhrx_pkt_cnt[%d]  : %llu",i,ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_fhrx_pkt_cnt[i]);
		ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_fhtx_pkt_cnt[%d]  : %llu",i,ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_fhtx_pkt_cnt[i]);
		ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_octx_pkt_cnt[%d]  : %llu",i,ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_octx_pkt_cnt[i]);
		ECPRILOGERR(" ecpriss_pdata_v2->xbar_ctx->stats.xbar_ocrx_pkt_cnt[%d] : %llu",i,ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_ocrx_pkt_cnt[i]);
	}

	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_fhrx_dma_pkt_cnt) %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_fhrx_dma_pkt_cnt);
	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_fhtx_dma_pkt_cnt) %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_fhtx_dma_pkt_cnt);

	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh0  : %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_ocrx_fh_buff_watermark_fh0);
	ECPRILOGERR("Vecpriss_pdata_v2->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh1 : %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_ocrx_fh_buff_watermark_fh1);
	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh2  : %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_ocrx_fh_buff_watermark_fh2);

	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.octx_oc_0_1_buff_watermark_cc0 : %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.octx_oc_0_1_buff_watermark_cc0);
	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.octx_oc_0_1_buff_watermark_cc1 : %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.octx_oc_0_1_buff_watermark_cc1);
	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.octx_oc_2_3_buff_watermark_cc2 : %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.octx_oc_2_3_buff_watermark_cc2);
	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.octx_oc_2_3_buff_watermark_cc3 : %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.octx_oc_2_3_buff_watermark_cc3);

	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_dbg_ocrx_0_1_buff_watermark_cc0 : %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_dbg_ocrx_0_1_buff_watermark_cc0);
	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_dbg_ocrx_0_1_buff_watermark_cc1 : %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_dbg_ocrx_0_1_buff_watermark_cc1);
	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_dbg_ocrx_2_3_buff_watermark_cc2 : %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_dbg_ocrx_2_3_buff_watermark_cc2);
	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_dbg_ocrx_2_3_buff_watermark_cc3 : %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_dbg_ocrx_2_3_buff_watermark_cc3);

	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_0_cnt) %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_0_cnt);
	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_1_cnt) %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_1_cnt);
	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_2_cnt) %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_2_cnt);

	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_0_cnt) %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_0_cnt);
	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_1_cnt) %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_1_cnt);
	ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->stats.xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_2_cnt) %llu ",ecpriss_pdata_v2->xbar_ctx_v2->stats_v2.xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_2_cnt);

	flush_state = (int *)&ecpriss_pdata_v2->xbar_ctx_v2->xbar_flush_status;
	if(flush_state) {
		ECPRILOGERR("ecpriss_pdata_v2->xbar_ctx->flush_state) 0x%x ", *flush_state);
	}
}

static int ecpriss_core_data_init(void)
{
	/*1. Initialize all the tables and data strucutres
	  2. Create the netlink socket
	  */
	int ret = 0;
	ecpriss_pdata->dev_mode = (ecpriss_dev_mode_e)ECPRI_HW_FLAVOR_RU;
	ecpriss_pdata->ecpri_hw_ver = ecpriss_hw_ver;
	ecpriss_pdata->callback_flag = &callback_flag_g;
	ecpriss_pdata->dma_endp = &dma_endp_g;
	ecpriss_pdata->eth_topology_params = &eth_link_params_g;
	ecpriss_pdata->events_workqueue = &events_workqueue_g;
	ecpriss_pdata->interrupts_workqueue = &interrupts_workqueue_g;

	ecpriss_pdata->qudp_ctx = &qudp_ctx_g;
	ecpriss_pdata->xbar_ctx = &xbar_ctx_g;

	ecpriss_pdata->qudp_ctx->ecpriss_qudp_hal_ctx =
		qudp_ctx_g.ecpriss_qudp_hal_ctx;
	ecpriss_pdata->xbar_ctx->ecpriss_xbar_hal = xbar_ctx_g.ecpriss_xbar_hal;
	spin_lock_init(&ecpriss_pdata->irq_lock);
	atomic_notifier_chain_register(&panic_notifier_list,&ecpriss_panic);
	ecpriss_pdata->ecpriss_core_logbuf =
        ipc_log_context_create(ECPRISS_CORE_IPC_LOG_PAGES,
                "ecpriss_core", 0);
        if (ecpriss_pdata->ecpriss_core_logbuf == NULL)
        ECPRILOGERR("failed to create log context for ECPRISS_SS driver\n");

	ecpriss_pdata->ecpriss_core_cfg_logbuf =
        ipc_log_context_create(ECPRISS_CORE_IPC_LOG_PAGES,
                "ecpriss_core_cfg", 0);
        if (ecpriss_pdata->ecpriss_core_cfg_logbuf == NULL)
        ECPRILOGERR("failed to create log context for ECPRISS_SS driver\n");
	/* ecpriss_pdata->stats = &stats_g; */
	eth_topology_ready_cb = &ecpriss_eth_topology_cb;
	eth_interface_events_cb = &ecpriss_eth_events_cb;
	do {
		ret = ecpriss_initialize_workq();
		if(ret < 0) {
			ECPRILOGERR("Work queue init failed\n");
			break;
		}
		ECPRILOGINFO("eCPRI core Work queue Inited\n");

		ret = ecpriss_netlink_socket_create();
		if(ret < 0) {
			ECPRILOGERR("Netlink socket initialization failed\n");
			break;
		}
		ECPRILOGINFO("eCPRI Netlink Socket(NETLINK_ECPRI family) Created\n");

		ret = ecpriss_stats_timer_interrupt_create();
		if(ret < 0) {
			ECPRILOGERR("eCPRI Timer Interrupt creation failed\n");
			break;
		}
		ECPRILOGINFO("eCPRI Statistics Timer Interrupt created\n");

	} while (0);
	return ret;
}


static void ecpriss_update_strict_filter_cfg(ecpriss_core_private_s_v2 *ecpriss_pata_v2, int *strict_filter_cfg)
{
	int port_idx = 0;

	for(port_idx = 0 ; port_idx < MAX_PORTS ; port_idx++) {
		ecpriss_pdata_v2->qudp_ctx_v2->fh_port_cfg_v2[port_idx].strict_filter_status = strict_filter_cfg[port_idx];
		ECPRILOGINFO("strict_filter_cfg: port %d, ecpri_strict_filter_cfg %d\n",port_idx,strict_filter_cfg[port_idx]);
	}

	return;

}
static int ecpriss_core_data_init_v2(void)
{
	/*1. Initialize all the tables and data strucutres
	  2. Create the netlink socket
	  */
	int ret = 0;
	int port_index;
	ecpriss_pdata_v2->dev_mode = (ecpriss_dev_mode_e)ECPRI_HW_FLAVOR_RU;
	ecpriss_pdata_v2->callback_flag = &callback_flag_g;
	ecpriss_pdata_v2->ecpri_hw_ver = ecpriss_hw_ver;
	ecpriss_pdata_v2->dma_endp = &dma_endp_g;
	ecpriss_pdata_v2->eth_topology_params = &eth_link_params_g;
	ecpriss_pdata_v2->events_workqueue = &events_workqueue_g;
	ecpriss_pdata_v2->interrupts_workqueue = &interrupts_workqueue_g;
	spin_lock_init(&ecpriss_pdata_v2->irq_lock);
	ecpriss_pdata_v2->ecpriss_core_logbuf =
        ipc_log_context_create(ECPRISS_CORE_IPC_LOG_PAGES,
                "ecpriss_core", 0);
        if (ecpriss_pdata_v2->ecpriss_core_logbuf == NULL)
		ECPRILOGERR("failed to create log context for ECPRISS_SS driver\n");

	ecpriss_pdata_v2->ecpriss_core_cfg_logbuf =
        ipc_log_context_create(ECPRISS_CORE_IPC_LOG_PAGES,
                "ecpriss_core_cfg", 0);
        if (ecpriss_pdata_v2->ecpriss_core_cfg_logbuf == NULL)
        ECPRILOGERR("failed to create log context for ECPRISS_SS driver\n");
	mutex_init(&ecpriss_pdata_v2->ecpriss_mutex_lock);
	ecpriss_pdata_v2->qudp_ctx_v2 = &qudp_ctx_g_v2;

	ecpriss_update_strict_filter_cfg(ecpriss_pdata_v2, ecpriss_qudp_strict_filt_cfg);
	ecpriss_pdata_v2->xbar_ctx_v2 = &xbar_ctx_g_v2;

	ecpriss_pdata_v2->xbar_ctx_v2->disable_xbar_dma_fh_same_prio = disable_xbar_dma_fh_same_prio;

	ecpriss_pdata_v2->qudp_ctx_v2->lte_fh_enabled = lte_fh_enabled;
	ecpriss_pdata_v2->xbar_ctx_v2->enable_len_check = enable_len_check;

	for (port_index = 0;port_index < MAX_PORTS;port_index++)
	{
	memset(&ecpriss_pdata_v2->xbar_ctx_v2->flow_ctx_v2.fh_xbar_lut[port_index].configured_pcids
			,-1, sizeof(uint16_t) * ECPRISS_MAX_PCID_ENTRIES);

	memset(&ecpriss_pdata_v2->xbar_ctx_v2->flow_ctx_v2.oc_rx_xbar_lut[port_index].configured_pcids
			,-1, sizeof(uint16_t) * ECPRISS_MAX_PCID_ENTRIES);

	}
	ecpriss_pdata_v2->qudp_ctx_v2->ecpriss_qudp_hal_ctx =
		qudp_ctx_g.ecpriss_qudp_hal_ctx;
	ecpriss_pdata_v2->xbar_ctx_v2->ecpriss_xbar_hal = xbar_ctx_g_v2.ecpriss_xbar_hal;
	ecpriss_pdata_v2->ssr_info = &ssr_info_g;

	eth_topology_ready_cb = &ecpriss_eth_topology_cb_v2;
	eth_interface_events_cb = &ecpriss_eth_events_cb_v2;

	atomic_notifier_chain_register(&panic_notifier_list,&ecpriss_panic);
	do {
		ret = ecpriss_initialize_workq_v2();
		if(ret < 0) {
			ECPRILOGERR("Work queue init failed\n");
			break;
		}
	} while (0);
	return ret;
}


static int ecpriss_core_register_callbacks(void)
{

	/*
	   1. Register for callback with Ethernet and update state
	   2. Register callback with DMA and update state
	   3. Register callback with MACSEC and SSR update state
	   */



	int ret = 0;
	bool ready = 0;
	bool *is_ready = &ready;
	do{
		ret = (mtip_ecpri_ops.eth_ecpriss_register_ready_cb)
			(eth_topology_ready_cb, is_ready);

		if (ret < 0) {
			break;
		}

		ret = (mtip_ecpri_ops.eth_ecpriss_register_events_cb)
			(eth_interface_events_cb);

		if (ret < 0) {
			break;
		}

		if(*is_ready == true) {

			ecpriss_eth_topology_init();
		}

		ready = 0;


		dma_ready_info.notify_ready = &ecpriss_dma_endp_cb;
		dma_ready_info.dma_event_notify = &ecpriss_dma_events_cb;
		dma_ready_info.log_msg = &ecpriss_dma_ecpri_ss_log_msg_cb;

		ret = (dma_ecpri_ss_driver_ops.ecpri_dma_ecpri_ss_register)
			(&dma_ready_info,is_ready);
		if (ret < 0) {
			break;
		}

		if(*is_ready == true &&
		(ecpriss_pdata->callback_flag->dma_callback_rcvd == 0)) {

			ret = ecpriss_dma_endp_config();

			if(ret < 0) {
				break;
			}
		}
	}while (0);
	return ret;
}
static int ecpriss_core_register_callbacks_v2(bool *is_eth_ready)
{
	/*
	   1. Register for callback with Ethernet and update state
	   2. Register callback with DMA and update state
	   3. Register callback with MACSEC and SSR update state
	   */


	int ret = 0;
	bool ready = 0;
	bool *is_ready = &ready;
	void * handle;
	bool enable_c2c2 = true;

	do{
		ret = (mtip_ecpri_ops.eth_ecpriss_register_ready_cb)
			(eth_topology_ready_cb, is_eth_ready);

		if (ret < 0) {
			ECPRILOGERR("callback registration for mtip register_ready_cb failed\n");
			break;
		}

		ret = (mtip_ecpri_ops.eth_ecpriss_register_events_cb)
			(eth_interface_events_cb);

		if (ret < 0) {
			ECPRILOGERR("callback registration for mtip register_events_cb failed\n");
			break;
		}

		if(*is_eth_ready == true) {
			ecpriss_eth_topology_init_v2();
		}

		ready = 0;


		dma_ready_info.notify_ready = &ecpriss_dma_endp_cb_v2;
		dma_ready_info.dma_event_notify = &ecpriss_dma_events_cb_v2;
		dma_ready_info.log_msg = &ecpriss_dma_ecpri_ss_log_msg_cb_v2;

		ret = (dma_ecpri_ss_driver_ops.ecpri_dma_ecpri_ss_register)
			(&dma_ready_info,is_ready);
		if (ret < 0) {
			ECPRILOGERR("callback registration for dma dma_ecpri_ss_register failed\n");
			break;
		}

		if(*is_ready == true &&
		(ecpriss_pdata_v2->callback_flag->dma_callback_rcvd == 0)) {

			ret = ecpriss_dma_endp_config_v2();

			if(ret < 0) {
				ECPRILOGERR("callback registration for dma dma_callback_rcvd failed\n");
				break;
			}
		}
		ecpriss_pdata_v2->ssr_info->ssr_label = "ecpriss_core_ssr";
		ecpriss_pdata_v2->ssr_info->nb.notifier_call = ecpriss_ssr_events_cb;

		handle = qcom_register_ssr_notifier("mpss",&ecpriss_pdata_v2->ssr_info->nb);

		if (IS_ERR_OR_NULL(handle)) {
			ECPRILOGERR("could not register for SSR notifier for %s\n failed with err %d",
			ecpriss_pdata_v2->ssr_info->ssr_label,PTR_ERR(handle));
				break;
		}

		ECPRILOGINFO("SSR registered successfully for %s\n",ecpriss_pdata_v2->ssr_info->ssr_label);
		ecpriss_pdata_v2->ssr_info->notifier_handle = handle;

		if(*is_eth_ready == true) {
			ECPRILOGINFO("CB dev_mode=%d\n",ecpriss_pdata_v2->dev_mode);
			if(ecpriss_pdata_v2->dev_mode == ECPRISS_DEV_MODE_RU || ecpriss_pdata_v2->dev_mode
					== ECPRISS_DEV_MODE_DU_PCIE_3_X_12) {
				if (ru_cascade_mode) {
					ECPRILOGINFO("CB: Bringing up C2C2 and C2C1 in E2E mode (ru_cascade_mode=%d)\n",
						     ru_cascade_mode);
					ret = (mtip_ecpri_ops.eth_ecpriss_enable_ru_cascade_c2c_bringup)();
					if(ret != 0) {
						ECPRILOGERR("CB: C2C E2E bringup failed\n");
					}
				} else {
					ECPRILOGINFO("CB: Bringing up C2C2 port in loopback mode\n");
					ret = (mtip_ecpri_ops.eth_ecpriss_enable_logging_port)(enable_c2c2);
					if(ret != 0) {
						ECPRILOGERR("CB: C2C2 bringup failed\n");
					}
				}
			}
		}
	}while (0);
	return ret;
}
static int ecpriss_clock_init(struct device *dev)
{
	int ret = 0;
	sys_clock.ecpri_cg = devm_clk_get(dev,"ecpri_cg");
	if (!sys_clock.ecpri_cg){
		ECPRILOGERR("Failed to get ecpri_cg\n");
		return -ENOMEM;
	}
	sys_clock.ecpri_fr = devm_clk_get(dev,"ecpri_fr");
	if (!sys_clock.ecpri_fr){
		ECPRILOGERR("Failed to get ecpri_fr \n");
		return -ENOMEM;
	}
	sys_clock.ecpri_eth_100G_fh0 = devm_clk_get(dev,"ecpri_eth_100G_fh0");
	if (!sys_clock.ecpri_eth_100G_fh0){
		ECPRILOGERR("Failed to get ecpri_eth_100G_fh0\n");
		return -ENOMEM;
	}
	sys_clock.ecpri_eth_100G_fh1 = devm_clk_get(dev,"ecpri_eth_100G_fh1");
	if (!sys_clock.ecpri_eth_100G_fh1){
		ECPRILOGERR("Failed to get ecpri_eth_100G_fh1\n");
		return -ENOMEM;
	}
	sys_clock.ecpri_eth_100G_fh2 = devm_clk_get(dev,"ecpri_eth_100G_fh2");
	if (!sys_clock.ecpri_eth_100G_fh2){
		ECPRILOGERR("Failed to get ecpri_eth_100G_fh2\n");
		return -ENOMEM;
	}
	sys_clock.ecpri_eth_100G_c2c0 = devm_clk_get(dev,"ecpri_eth_100G_c2c0");
	if (!sys_clock.ecpri_eth_100G_c2c0){
		ECPRILOGERR("Failed to get ecpri_eth_100G_c2c0\n");
		return -ENOMEM;
	}
	sys_clock.ecpri_eth_100G_c2c1 = devm_clk_get(dev,"ecpri_eth_100G_c2c1");
	if (!sys_clock.ecpri_eth_100G_c2c1){
		ECPRILOGERR("Failed to get ecpri_eth_100G_c2c1\n");
		return -ENOMEM;
	}
	sys_clock.ecpri_eth_100G_dbg_c2c = devm_clk_get(dev,"ecpri_eth_100G_dbg_c2c");
	if (!sys_clock.ecpri_eth_100G_dbg_c2c){
		ECPRILOGERR("Failed to get ecpri_eth_100G_dbg_c2c\n");
		return -ENOMEM;
	}
	sys_clock.ecpri_oran_div2 = devm_clk_get(dev,"ecpri_oran_div2");
	if (!sys_clock.ecpri_oran_div2){
		ECPRILOGERR("Failed to get ecpri_oran_div2\n");
		return -ENOMEM;
	}
	sys_clock.ecpri_mss_oran = devm_clk_get(dev,"ecpri_mss_oran");
	if (!sys_clock.ecpri_mss_oran){
		ECPRILOGERR("Failed to get ecpri_mss_oran\n");
		return -ENOMEM;
	}

	ret = clk_prepare_enable(sys_clock.ecpri_cg);
	if (ret){
		ECPRILOGERR("Failed to vote ecpri_cg \n");
        }

	clk_set_rate(sys_clock.ecpri_cg, ECPRI_CG_CLK_NOM_MAX);

	ret = clk_prepare_enable(sys_clock.ecpri_fr);
	if (ret){
		ECPRILOGERR("Failed to vote ecpri_fr\n");
        }
	ret = clk_prepare_enable(sys_clock.ecpri_eth_100G_fh0);
	if (ret){
		ECPRILOGERR("Failed to vote ecpri_eth_100G_fh0\n");
        }
	ret = clk_prepare_enable(sys_clock.ecpri_eth_100G_fh1);
	if (ret){
		ECPRILOGERR("Failed to vote ecpri_eth_100G_fh1\n");
        }
	ret = clk_prepare_enable(sys_clock.ecpri_eth_100G_fh2);
	if (ret){
		ECPRILOGERR("Failed to vote ecpri_eth_100G_fh2\n");
        }
	ret = clk_prepare_enable(sys_clock.ecpri_eth_100G_c2c0);
	if (ret){
		ECPRILOGERR("Failed to vote ecpri_eth_100G_c2c0\n");
        }
	ret = clk_prepare_enable(sys_clock.ecpri_eth_100G_c2c1);
	if (ret){
		ECPRILOGERR("Failed to vote ecpri_eth_100G_c2c1\n");
        }
	ret = clk_prepare_enable(sys_clock.ecpri_eth_100G_dbg_c2c);
	if (ret){
		ECPRILOGERR("Failed to vote ecpri_eth_100G_dbg_c2c\n");
        }
	ret = clk_prepare_enable(sys_clock.ecpri_oran_div2);
	if (ret){
		ECPRILOGERR("Failed to vote ecpri_oran_div2\n");
        }
	ret = clk_prepare_enable(sys_clock.ecpri_mss_oran);
	if (ret){
		ECPRILOGERR("Failed to vote ecpri_mss_oran\n");
        }

	clk_set_rate(sys_clock.ecpri_mss_oran, ECPRI_MSS_ORAN_NOM_MAX);

	return 0;
}
void ecpriss_update_all_stats(void)
{
	int fh = 0;
	int link = 0;
	for ( fh = 0 ; fh < MAX_PORTS; fh++) {

		for (link = 0; link < MAX_MAC_LINKS; link++){
			ecpriss_qudp_fh_ingress_stats_update(fh,link);
			ecpriss_qudp_fh_egress_stats_update(fh,link);
		}
	}
	ecpriss_xbar_stats_update();
}

void ecpriss_fh_stats_update_for_usr(void)
{
	ecpriss_fh_qudp_stats_update_usr();
	ecpriss_fh_xbar_stats_update_usr();
	return;
}

void ecpriss_update_all_stats_v2(void)
{
	int fh = 0;
	int link = 0;
	mutex_lock(&ecpriss_pdata_v2->ecpriss_mutex_lock);
	for ( fh = 0 ; fh < MAX_PORTS; fh++) {

		for (link = 0; link < MAX_MAC_LINKS; link++){
			ecpriss_qudp_fh_ingress_stats_update_v2(fh,link);
			ecpriss_qudp_fh_egress_stats_update_v2(fh,link);
		}
	}
	ecpriss_xbar_stats_update_v2();
	ecpriss_fh_stats_update_for_usr();
	mutex_unlock(&ecpriss_pdata_v2->ecpriss_mutex_lock);
}


int ecpriss_stats_timer_interrupt_create(void)
{
	int ret = 0;

	do {

		timer_setup(&ecpriss_pdata->stats_timer_info.stats_timer,
				&ecpriss_stats_timer_cb,0);
		ecpriss_pdata->stats_timer_info.stats_timer_running = 0;
		ecpriss_pdata->stats_timer_info.stats_interval = 0;

	}while (0);

	return ret;

}


int ecpriss_stats_timer_disable(void)
{
	int ret = 0;

	do{
		ecpriss_pdata->stats_timer_info.stats_timer.expires = jiffies;
		mod_timer(&ecpriss_pdata->stats_timer_info.stats_timer,
				ecpriss_pdata->stats_timer_info.stats_timer.expires);

		ecpriss_pdata->stats_timer_info.stats_timer_running = 0;
		ecpriss_pdata->stats_timer_info.stats_interval = 0;
	}while(0);

	return ret;
}


int ecpriss_stats_timer_enable(int timeout)
{
	int ret = 0;

	do {
		ecpriss_pdata->stats_timer_info.stats_timer.expires =
			jiffies + msecs_to_jiffies(timeout);
		mod_timer(&ecpriss_pdata->stats_timer_info.stats_timer,
				ecpriss_pdata->stats_timer_info.stats_timer.expires);

		ecpriss_pdata->stats_timer_info.stats_timer_running = 1;
		ecpriss_pdata->stats_timer_info.stats_interval = timeout;


	}while (0);
	return ret;
}

int ecpriss_core_get_stats_timeout_info(void)
{
	ECPRILOGINFO("ecpriss: Stats Timeout current val %d\n", stats_timeout_ms);
	return stats_timeout_ms;
}

void ecpriss_core_set_stats_timeout_info(int val)
{
	stats_timeout_ms = val;
	ECPRILOGINFO("ecpriss: Setting Stats Timeout to val %d\n", stats_timeout_ms);
}

int ecpriss_core_get_enable_len_check_info(void)
{
	uint32_t enable_len_check = ecpriss_pdata_v2->xbar_ctx_v2->enable_len_check;
	ECPRILOGINFO("ecpriss: Length check val is %d\n", enable_len_check);
	return enable_len_check;
}


void ecpriss_core_set_enable_len_check_info(uint32_t val)
{
	ecpriss_pdata_v2->xbar_ctx_v2->enable_len_check = val;
	ECPRILOGINFO("ecpriss: Setting Length check %d\n",
			ecpriss_pdata_v2->xbar_ctx_v2->enable_len_check);
}


static int ecpriss_core_init(struct platform_device *pdev)
{
	/*1. Initialize ECPRISS private data struct
	  2. Register for the callbacks with the external modules such as
	  EMAC,DMA and MACSEC
	  4. Flow manager init, Initialize the flow tables and the
	  nfapi tables (In user space)
	  5. Initialize XBAR by calling in ecpriss_xbar_init()
	  -->Dependency DMA endpoints
	  6. Initialize all the QUDP instances based on the topology
	  -->Dependency on eemac topology */

	int ret = 0;

	do{

		if(pdev == NULL) {
			ret = -ENOMEM;
			break;
		}
		memset(ecpriss_pdata,0,sizeof(ecpriss_core_private_s));

		ret = ecpriss_core_data_init();
		if(ret < 0) {
			ECPRILOGERR("Initialization of pdata failed\n");
			break;
		}

		ret = ecpriss_clock_init(&pdev->dev);
		if(ret < 0) {
			ECPRILOGERR("Initialization of clock failed\n");
			break;
		}


		ret = ecpriss_xbar_cold_init(&pdev->dev);
		if(ret < 0) {
			ECPRILOGERR("XBAR cold init failed\n");
			break;
		}

		ret = ecpriss_qudp_init(&pdev->dev);
		if(ret < 0) {
			ECPRILOGERR("QUDP initialization failed\n");
			break;
		}

		ECPRILOGERR("QUDP init complete\n");

		ret = ecpriss_core_register_callbacks();
		if(ret < 0) {
			ECPRILOGERR("Callback registrations failed\n");
			break;
		}

		ret = ecpriss_stats_timer_enable(stats_timeout_ms);

		if(ret < 0) {
			ECPRILOGERR("Stats Collection failed\n");
			break;
		}

		ecpriss_pdata->ecpri_state = ECPRI_CORE_INIT;

	}while (0);
	return ret;

}
static int ecpriss_core_init_v2(struct platform_device *pdev)
{
	/*1. Initialize ECPRISS private data struct
	  2. Register for the callbacks with the external modules such as
	  EMAC,DMA and MACSEC
	  4. Flow manager init, Initialize the flow tables and the
	  nfapi tables (In user space)
	  5. Initialize XBAR by calling in ecpriss_xbar_init()
	  -->Dependency DMA endpoints
	  6. Initialize all the QUDP instances based on the topology
	  -->Dependency on eemac topology */

	int ret = 0;
	bool is_eth_ready = false;

	do{

		if(pdev == NULL) {
			ret = -ENOMEM;
			break;
		}
		memset(ecpriss_pdata_v2,0,sizeof(ecpriss_core_private_s));
		ecpriss_pdata_v2->pdev = pdev;
		ret = ecpriss_clock_init(&pdev->dev);
		if(ret < 0) {
			ECPRILOGERR("Initialization of clock failed\n");
			break;
		}
		ret = ecpriss_core_data_init_v2();
		if(ret < 0) {
			ECPRILOGERR("Initialization of pdata failed\n");
			break;
		}

		ret = ecpriss_xbar_cold_init_v2(&pdev->dev);
		if(ret < 0) {
			ECPRILOGERR("XBAR cold init failed\n");
			break;
		}

		ret = ecpriss_core_register_callbacks_v2(&is_eth_ready);
		if(ret < 0) {
			ECPRILOGERR("Callback registrations failed\n");
			break;
		}

		ret = ecpriss_qudp_init_v2(&pdev->dev);
		if(ret < 0) {
			ECPRILOGERR("QUDP initialization failed\n");
			break;
		}

		ECPRILOGERR("QUDP init complete\n");

		ecpriss_xbar_fhrx_default_dma_channel();

		ecpriss_xbar_c2crx_default_dma_channel();

		ecpriss_update_stats_and_requeue(NULL);

		ret = ecpriss_netlink_socket_create_v2();
		if(ret < 0) {
			ECPRILOGERR("Netlink socket initialization failed\n");
			break;
		}
		ECPRILOGINFO("eCPRI Netlink Socket(NETLINK_ECPRI family) Created\n");

		ret = ecpriss_netlink_stats_socket_create();
		if(ret < 0){
			ECPRILOGERR("Netlink socket (STATS_NETLINK_ECPRI family) created\n");
			break;
		}
		ecpriss_pdata_v2->ecpri_state = ECPRI_CORE_INIT;

		if(lte_fh_enabled) {

			ret = ecpriss_mhi_ctx_init(&ecpriss_pdata_v2->mhi_ctx);

			if(ret < 0) {
				ECPRILOGERR("LTE_FH:MHI Ctx Init Failed\n");
			}

			if(is_eth_ready == true) {
				ecpriss_eth_link_update_for_mhi_v2();

			}
		}

		if(ecpriss_pdata_v2->dev_mode != ECPRISS_DEV_MODE_RU && lte_fh_enabled
				&& ecpriss_pdata_v2->ecpri_state == ECPRI_CORE_INIT) {

				ecpriss_qudp_set_nr_mac_filter();
		}

		if(ecpriss_pdata_v2->dev_mode == ECPRISS_DEV_MODE_RU && ru_cascade_mode
				&& ecpriss_pdata_v2->ecpri_state == ECPRI_CORE_INIT) {

			ecpriss_qudp_set_nr_mac_filter();
		}

	}while (0);
	return ret;
}
static int ecpriss_core_probe(struct platform_device *pdev)
{
	int ret = 0;
	ecpriss_hw_name_e hw_ver;

	ECPRILOGDBG("ecpriss_core_probe(): Start \n");

	if(pdev == NULL) {
		ret = -ENOMEM;
		return ret;
	}
	hw_ver = ecpriss_core_get_hw_ver(pdev);

	if(hw_ver == ECPRISS_HW_v1_0) {
		ecpriss_core_init(pdev);
	} else {
		ecpriss_core_init_v2(pdev);
	}
	/*
	 * Debug FS Init
	 */
#ifndef NO_DEBUGFS_PERF
	setup_debugfs_directory();
#endif
	ECPRILOGDBG("ecpriss_core_probe(): End\n");
	/*Clean up for init failure.*/
	return ret;
}

static const struct of_device_id ecpriss_core_dt_match[] = {
	{ .compatible = "qcom,ecpriss_core" },
	{ },
};

MODULE_DEVICE_TABLE(of, ecpriss_core_dt_match);

static const struct dev_pm_ops ecpriss_core_pm_ops = {
	.suspend = ecpriss_core_suspend,
	.resume = ecpriss_core_resume,
};

static struct platform_driver ecpriss_core_driver = {
	.driver = {
		.name = "ecpriss_core",
		.owner = THIS_MODULE,
		.of_match_table = ecpriss_core_dt_match,
		.pm = &ecpriss_core_pm_ops,
	},
	.probe = ecpriss_core_probe,
	.remove = ecpriss_core_remove,
};


static int __init ecpriss_core_module_init(void)
{
	ECPRILOGDBG("ecpriss_core_module_init():Start \n");
	return platform_driver_register(&ecpriss_core_driver);
}

static void __exit ecpriss_core_module_exit(void)
{

	pr_err("ecpriss_core_module_exit():Exit \n");

	platform_driver_unregister(&ecpriss_core_driver);

	return;
}
void ecpriss_destroy_ipc_log_v2(void)
{
	if(ecpriss_pdata_v2){
		if(ecpriss_pdata_v2->ecpriss_core_logbuf)
			ipc_log_context_destroy(ecpriss_pdata_v2->ecpriss_core_logbuf);

		if(ecpriss_pdata_v2->ecpriss_core_cfg_logbuf)
			ipc_log_context_destroy(ecpriss_pdata_v2->ecpriss_core_cfg_logbuf);
	}
}
void ecpriss_unmap_xbar_qudp_v2(void)
{
	if(ecpriss_pdata_v2){

		if(ecpriss_pdata_v2->qudp_ctx_v2->ecpriss_qudp_hal_ctx->fh_rams_base)
			iounmap(ecpriss_pdata_v2->qudp_ctx_v2->ecpriss_qudp_hal_ctx->fh_rams_base);
		if(ecpriss_pdata_v2->qudp_ctx_v2->ecpriss_qudp_hal_ctx->fh_filter_base)
			iounmap(ecpriss_pdata_v2->qudp_ctx_v2->ecpriss_qudp_hal_ctx->fh_filter_base);
		if(ecpriss_pdata_v2->qudp_ctx_v2->ecpriss_qudp_hal_ctx->fh_base)
			iounmap(ecpriss_pdata_v2->qudp_ctx_v2->ecpriss_qudp_hal_ctx->fh_base);
		if(ecpriss_pdata_v2->qudp_ctx_v2->ecpriss_qudp_hal_ctx->global_base)
			iounmap(ecpriss_pdata_v2->qudp_ctx_v2->ecpriss_qudp_hal_ctx->global_base);

		if(ecpriss_pdata_v2->xbar_ctx_v2->ecpriss_xbar_hal->base)
			iounmap(ecpriss_pdata_v2->xbar_ctx_v2->ecpriss_xbar_hal->base);
		if(ecpriss_pdata_v2->xbar_ctx_v2->ecpriss_xbar_hal->lut_base)
			iounmap(ecpriss_pdata_v2->xbar_ctx_v2->ecpriss_xbar_hal->lut_base);
	}
}

static void ecpriss_debug_stringify_flow_tp_cfg(ecpriss_packet_payload_s *packet, char *cmd_buf, uint32_t *offset_p, uint32_t max_str_size)
{
	ecpriss_flow_rx_cfg_s *flow_rx = NULL;
	ecpriss_flow_tx_cfg_s *flow_tx = NULL;
	uint32_t i= 0;
	uint32_t ip_sum = 0;
	uint32_t offset = 0;

	if(!packet || !cmd_buf || !offset_p){
		ECPRILOGERR("%s: Null Input \n",__func__);
		return;
	}

	offset = *offset_p;
	flow_tx = &packet->flow_cfg.flow_tx_cfg;

	if((int)flow_tx->src == ECPRISS_ROUTE_SRC_OC){
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"PCID|%u| ",flow_tx->xbar_tx_cfg.pcid);
		offset = offset % max_str_size;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"|OC-FH| ");
		offset = offset % max_str_size;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Port|%u| ",flow_tx->port_index);
		offset = offset % max_str_size;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"L2|%u| ",flow_tx->xbar_tx_cfg.l2_hdr_tbl_idx);
		offset = offset % max_str_size;
		if(flow_tx->xbar_tx_cfg.l3_hdr_valid)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"L3|%u| ",flow_tx->xbar_tx_cfg.l3_hdr_tbl_idx);
		offset = offset % max_str_size;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"SM|");
		offset = offset % max_str_size;
		for(i=0;i<6;i++){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x:",flow_tx->qudp_tx_cfg.eth_hdr.src_mac_addr[i]);
			offset = offset % max_str_size;
		}
		cmd_buf[offset-1]='|';
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ," DM|");
		offset = offset % max_str_size;
		for(i=0;i<6;i++){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x:",flow_tx->qudp_tx_cfg.eth_hdr.dst_mac_addr[i]);
			offset = offset % max_str_size;
		}
		cmd_buf[offset-1]='|';
		if(flow_tx->qudp_tx_cfg.eth_hdr.is_vlan)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ," Vlan|%u|",flow_tx->qudp_tx_cfg.eth_hdr.vlan_data);
		offset = offset % max_str_size;

		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ," |0x%x| ",flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype);
		offset = offset % max_str_size;

		if(ECPRISS_IPV4_TYPE == flow_tx->qudp_tx_cfg.ip_hdr.ip_type){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"SI|");
			offset = offset % max_str_size;
			for(i=0;i<4;i++){
				offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%u.",flow_tx->qudp_tx_cfg.ip_hdr.src_ip_addr[i]);
				offset = offset % max_str_size;
			}
			--offset;
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,":%u| ",flow_tx->qudp_tx_cfg.ip_hdr.src_udp_port);
			offset = offset % max_str_size;
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ," DI|");
			offset = offset % max_str_size;
			for(i=0;i<4;i++){
				offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%u.",flow_tx->qudp_tx_cfg.ip_hdr.dst_ip_addr[i]);
				offset = offset % max_str_size;
			}
			--offset;
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,":%u| ",flow_tx->qudp_tx_cfg.ip_hdr.dst_udp_port);
			offset = offset % max_str_size;
		}else if(ECPRISS_IPV6_TYPE == flow_tx->qudp_tx_cfg.ip_hdr.ip_type){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"SI|");
			offset = offset % max_str_size;
			for(i=0;i<16;i++){
				offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x",flow_tx->qudp_tx_cfg.ip_hdr.src_ip_addr[i]);
				offset = offset % max_str_size;
				offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x:",flow_tx->qudp_tx_cfg.ip_hdr.src_ip_addr[++i]);
				offset = offset % max_str_size;
			}
			--offset;
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,":%u| ",flow_tx->qudp_tx_cfg.ip_hdr.src_udp_port);
			offset = offset % max_str_size;
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ," Dip|");
			offset = offset % max_str_size;
			for(i=0;i<16;i++){
				offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x",flow_tx->qudp_tx_cfg.ip_hdr.dst_ip_addr[i]);
				offset = offset % max_str_size;
				offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x:",flow_tx->qudp_tx_cfg.ip_hdr.dst_ip_addr[++i]);
				offset = offset % max_str_size;
			}
			--offset;
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,":%u| ",flow_tx->qudp_tx_cfg.ip_hdr.dst_udp_port);
			offset = offset % max_str_size;
		}
		if(flow_tx->qudp_tx_cfg.eth_hdr.vport)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Vport|%u| ",flow_tx->qudp_tx_cfg.eth_hdr.vport);
		offset = offset % max_str_size;

		if(flow_tx->qudp_tx_cfg.eth_hdr.vport_action)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Vport_action|%u| ",flow_tx->qudp_tx_cfg.eth_hdr.vport_action);
		offset = offset % max_str_size;
		if(flow_tx->qudp_tx_cfg.ip_hdr.sa_tag_data)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Sa_data|%u| ",flow_tx->qudp_tx_cfg.ip_hdr.sa_tag_data);
		offset = offset % max_str_size;
		if(flow_tx->qudp_tx_cfg.ip_hdr.udp_chksum_en)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Checksum_en|%u| ",flow_tx->qudp_tx_cfg.ip_hdr.udp_chksum_en);
		offset = offset % max_str_size;
		if(flow_tx->qudp_tx_cfg.ip_hdr.ipsec_en)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"IPsec_en|%u| ",flow_tx->qudp_tx_cfg.ip_hdr.ipsec_en);
		offset = offset % max_str_size;
		if(flow_tx->qudp_tx_cfg.ip_hdr.df_en)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"DE_Frag|%u| ",flow_tx->qudp_tx_cfg.ip_hdr.df_en);
		offset = offset % max_str_size;
		if(flow_tx->qudp_tx_cfg.ip_hdr_p.ttl)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"TTL|%u| ",flow_tx->qudp_tx_cfg.ip_hdr_p.ttl);
		offset = offset % max_str_size;
		if(flow_tx->qudp_tx_cfg.ip_hdr_p.identification)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"IPV4_ID|%u| ",flow_tx->qudp_tx_cfg.ip_hdr_p.identification);
		offset = offset % max_str_size;
		if(flow_tx->qudp_tx_cfg.ip_hdr_p.flow_label)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Flow_label|%u| ",flow_tx->qudp_tx_cfg.ip_hdr_p.flow_label);
		offset = offset % max_str_size;
		if(flow_tx->qudp_tx_cfg.ip_hdr_p.hop_limit)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Hop_limit|%u| ",flow_tx->qudp_tx_cfg.ip_hdr_p.hop_limit);
		offset = offset % max_str_size;
		if(flow_tx->qudp_tx_cfg.ip_hdr_p.hop_limit)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"DSCP/TOS|%u| ",flow_tx->qudp_tx_cfg.ip_hdr.tos);
		offset = offset % max_str_size;

	}else if( (int)flow_tx->src == ECPRISS_ROUTE_SRC_FH){
		flow_rx = &packet->flow_cfg.flow_rx_cfg;

		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"PCID|%u| ",flow_rx->xbar_rx_cfg.flow_id);
		offset = offset % max_str_size;

		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"|FH-OC| ");
		offset = offset % max_str_size;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Port|%u| ",flow_rx->port_index);
		offset = offset % max_str_size;


		for(i=0;i<16;i++){
			ip_sum += flow_rx->qudp_rx_cfg.ip_dst_addr[i];
		}
		if(!flow_rx->qudp_rx_cfg.fltr_en_mask && (flow_rx->qudp_rx_cfg.vlan_addr_port || flow_rx->qudp_rx_cfg.udp_dst_port || ip_sum)){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset,"|REM| ");
			offset = offset % max_str_size;
		}else{
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset,"|ADD| ");
			offset = offset % max_str_size;
		}
		if(flow_rx->qudp_rx_cfg.vlan_addr_port)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Vlan|%u| ",flow_rx->qudp_rx_cfg.vlan_addr_port);
		offset = offset % max_str_size;
		if(flow_rx->qudp_rx_cfg.udp_dst_port)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Port|%u| ",flow_rx->qudp_rx_cfg.udp_dst_port);
		offset = offset % max_str_size;

		if(ip_sum){
			if(ECPRISS_IPV4_TYPE == flow_rx->qudp_rx_cfg.ip_type){
				offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"IP|");
				offset = offset % max_str_size;
				for(i=0;i<4;i++){
					offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%u.",flow_rx->qudp_rx_cfg.ip_dst_addr[i]);
					offset = offset % max_str_size;
				}
				cmd_buf[offset-1]='|';
			}else if(ECPRISS_IPV6_TYPE == flow_rx->qudp_rx_cfg.ip_type){
				offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"IP|");
				offset = offset % max_str_size;
				for(i=0;i<16;i++){
					offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x",flow_rx->qudp_rx_cfg.ip_dst_addr[i]);
					offset = offset % max_str_size;
					offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x:",flow_rx->qudp_rx_cfg.ip_dst_addr[++i]);
					offset = offset % max_str_size;
				}
				cmd_buf[offset-1]='|';
			}
		}
		if(ECPRISS_PACKET_UL == flow_rx->xbar_rx_cfg.flow_dir){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"|DIR_UL| ");
			offset = offset % max_str_size;
		}else if(ECPRISS_PACKET_DL == flow_rx->xbar_rx_cfg.flow_dir ){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"|DIR_DL| ");
			offset = offset % max_str_size;
		}
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"OcLink|%u| ",flow_rx->xbar_rx_cfg.oc_link_id);
		offset = offset % max_str_size;
	}
	*offset_p = offset;
	return;
}

static void ecpriss_debug_stringify_flow_cfg(ecpriss_packet_payload_s *packet, char *cmd_buf, uint32_t *offset_p, uint32_t max_str_size)
{
	ecpriss_flow_rx_cfg_s *flow_rx = NULL;
	ecpriss_flow_tx_cfg_s *flow_tx = NULL;
	uint32_t offset = 0;

	if(!packet || !cmd_buf || !offset_p){
		ECPRILOGERR("%s: Null Input \n",__func__);
		return;
	}

	offset = *offset_p;
	flow_tx = &packet->flow_cfg.flow_tx_cfg;

	if((int)flow_tx->src == ECPRISS_ROUTE_SRC_OC){
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"PCID|%u| ",flow_tx->xbar_tx_cfg.pcid);
		offset = offset % max_str_size;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"|OC-FH| ");
		offset = offset % max_str_size;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Port|%u| ",flow_tx->port_index);
		offset = offset % max_str_size;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"L2|%u| ",flow_tx->xbar_tx_cfg.l2_hdr_tbl_idx);
		offset = offset % max_str_size;
		if(flow_tx->xbar_tx_cfg.l3_hdr_valid)
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"L3|%u| ",flow_tx->xbar_tx_cfg.l3_hdr_tbl_idx);
		offset = offset % max_str_size;
	}else if( (int)flow_tx->src == ECPRISS_ROUTE_SRC_FH){
		flow_rx = &packet->flow_cfg.flow_rx_cfg;

		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"PCID|%u| ",flow_rx->xbar_rx_cfg.flow_id);
		offset = offset % max_str_size;

		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"|FH-OC| ");
		offset = offset % max_str_size;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Port|%u| ",flow_rx->port_index);
		offset = offset % max_str_size;

		if(ECPRISS_PACKET_UL == flow_rx->xbar_rx_cfg.flow_dir){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"|DIR_UL| ");
			offset = offset % max_str_size;
		}else if(ECPRISS_PACKET_DL == flow_rx->xbar_rx_cfg.flow_dir ){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"|DIR_DL| ");
			offset = offset % max_str_size;
		}
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"OcLink|%u| ",flow_rx->xbar_rx_cfg.oc_link_id);
		offset = offset % max_str_size;
	}

	*offset_p = offset;
	return;
}

static void ecpriss_debug_stringify_egress_tp_cfg(ecpriss_packet_payload_s *packet, char *cmd_buf, uint32_t *offset_p, uint32_t max_str_size)
{
	ecpriss_flow_tx_cfg_s *flow_tx = NULL;
	uint32_t i= 0;
	uint32_t offset= 0;

	if(!packet || !cmd_buf || !offset_p){
		ECPRILOGERR("%s: Null Input \n",__func__);
		return;
	}
	offset = *offset_p;
	flow_tx = &packet->flow_cfg.flow_tx_cfg;

	offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"-Egress Port|%u| ",flow_tx->port_index);
	offset = offset % max_str_size;
	offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"L2|%u| ",flow_tx->xbar_tx_cfg.l2_hdr_tbl_idx);
	offset = offset % max_str_size;
	if(flow_tx->xbar_tx_cfg.l3_hdr_valid)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"L3|%u| ",flow_tx->xbar_tx_cfg.l3_hdr_tbl_idx);
	offset = offset % max_str_size;
	offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"SM|");
	offset = offset % max_str_size;
	for(i=0;i<6;i++){
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x:",flow_tx->qudp_tx_cfg.eth_hdr.src_mac_addr[i]);
		offset = offset % max_str_size;
	}
	cmd_buf[offset-1]='|';
	offset += scnprintf(cmd_buf + offset ,max_str_size - offset ," DM|");
	offset = offset % max_str_size;
	for(i=0;i<6;i++){
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x:",flow_tx->qudp_tx_cfg.eth_hdr.dst_mac_addr[i]);
		offset = offset % max_str_size;
	}
	cmd_buf[offset-1]='|';
	if(flow_tx->qudp_tx_cfg.eth_hdr.is_vlan)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ," Vlan|%u|",flow_tx->qudp_tx_cfg.eth_hdr.vlan_data);
	offset = offset % max_str_size;

	offset += scnprintf(cmd_buf + offset ,max_str_size - offset ," |0x%x| ",flow_tx->qudp_tx_cfg.eth_hdr.orig_ethertype);
	offset = offset % max_str_size;

	if(ECPRISS_IPV4_TYPE == flow_tx->qudp_tx_cfg.ip_hdr.ip_type){
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"SI|");
		offset = offset % max_str_size;
		for(i=0;i<4;i++){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%u.",flow_tx->qudp_tx_cfg.ip_hdr.src_ip_addr[i]);
			offset = offset % max_str_size;
		}
		--offset;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,":%u| ",flow_tx->qudp_tx_cfg.ip_hdr.src_udp_port);
		offset = offset % max_str_size;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ," DI|");
		offset = offset % max_str_size;
		for(i=0;i<4;i++){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%u.",flow_tx->qudp_tx_cfg.ip_hdr.dst_ip_addr[i]);
			offset = offset % max_str_size;
		}
		--offset;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,":%u| ",flow_tx->qudp_tx_cfg.ip_hdr.dst_udp_port);
		offset = offset % max_str_size;
	}else if(ECPRISS_IPV6_TYPE == flow_tx->qudp_tx_cfg.ip_hdr.ip_type){
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"SI|");
		offset = offset % max_str_size;
		for(i=0;i<16;i++){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x",flow_tx->qudp_tx_cfg.ip_hdr.src_ip_addr[i]);
			offset = offset % max_str_size;
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x:",flow_tx->qudp_tx_cfg.ip_hdr.src_ip_addr[++i]);
			offset = offset % max_str_size;
		}
		--offset;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,":%u| ",flow_tx->qudp_tx_cfg.ip_hdr.src_udp_port);
		offset = offset % max_str_size;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ," Dip|");
		offset = offset % max_str_size;
		for(i=0;i<16;i++){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x",flow_tx->qudp_tx_cfg.ip_hdr.dst_ip_addr[i]);
			offset = offset % max_str_size;
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x:",flow_tx->qudp_tx_cfg.ip_hdr.dst_ip_addr[++i]);
			offset = offset % max_str_size;
		}
		--offset;
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,":%u| ",flow_tx->qudp_tx_cfg.ip_hdr.dst_udp_port);
		offset = offset % max_str_size;
	}
	if(flow_tx->qudp_tx_cfg.eth_hdr.vport)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Vport|%u| ",flow_tx->qudp_tx_cfg.eth_hdr.vport);
	offset = offset % max_str_size;
	if(flow_tx->qudp_tx_cfg.eth_hdr.vport_action)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Vport_action|%u| ",flow_tx->qudp_tx_cfg.eth_hdr.vport_action);
	offset = offset % max_str_size;
	if(flow_tx->qudp_tx_cfg.ip_hdr.tos)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"DSCP/TOS|%u| ",flow_tx->qudp_tx_cfg.ip_hdr.tos);
	offset = offset % max_str_size;
	if(flow_tx->qudp_tx_cfg.ip_hdr.sa_tag_data)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Sa_data|%u| ",flow_tx->qudp_tx_cfg.ip_hdr.sa_tag_data);
	offset = offset % max_str_size;
	if(flow_tx->qudp_tx_cfg.ip_hdr.udp_chksum_en)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Checksum_en|%u| ",flow_tx->qudp_tx_cfg.ip_hdr.udp_chksum_en);
	offset = offset % max_str_size;
	if(flow_tx->qudp_tx_cfg.ip_hdr.ipsec_en)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"IPsec_en|%u| ",flow_tx->qudp_tx_cfg.ip_hdr.ipsec_en);
	offset = offset % max_str_size;
	if(flow_tx->qudp_tx_cfg.ip_hdr.df_en)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"DE_Frag|%u| ",flow_tx->qudp_tx_cfg.ip_hdr.df_en);
	offset = offset % max_str_size;
	if(flow_tx->qudp_tx_cfg.ip_hdr_p.ttl)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"TTL|%u| ",flow_tx->qudp_tx_cfg.ip_hdr_p.ttl);
	offset = offset % max_str_size;
	if(flow_tx->qudp_tx_cfg.ip_hdr_p.identification)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"IPV4_ID|%u| ",flow_tx->qudp_tx_cfg.ip_hdr_p.identification);
	offset = offset % max_str_size;
	if(flow_tx->qudp_tx_cfg.ip_hdr_p.flow_label)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Flow_label|%u| ",flow_tx->qudp_tx_cfg.ip_hdr_p.flow_label);
	offset = offset % max_str_size;
	if(flow_tx->qudp_tx_cfg.ip_hdr_p.hop_limit)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Hop_limit|%u| ",flow_tx->qudp_tx_cfg.ip_hdr_p.hop_limit);
	offset = offset % max_str_size;
	if(flow_tx->qudp_tx_cfg.ip_hdr_p.hop_limit)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"DSCP/TOS|%u| ",flow_tx->qudp_tx_cfg.ip_hdr.tos);
	offset = offset % max_str_size;



	*offset_p = offset;
	return;
}

static void ecpriss_debug_stringify_ingress_tp_cfg(ecpriss_packet_payload_s *packet, char *cmd_buf, uint32_t *offset_p, uint32_t max_str_size)
{
	ecpriss_flow_rx_cfg_s *flow_rx = NULL;
	uint32_t ip_sum = 0;
	uint32_t i= 0;
	uint32_t offset = 0;

	if(!packet || !cmd_buf || !offset_p){
		ECPRILOGERR("%s: Null Input \n",__func__);
		return;
	}

	offset = *offset_p;

	flow_rx = &packet->flow_cfg.flow_rx_cfg;

	offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"-Ingress Port|%u| ",flow_rx->port_index);
	offset = offset % max_str_size;


	for(i=0;i<16;i++){
		ip_sum += flow_rx->qudp_rx_cfg.ip_dst_addr[i];
	}
	if(flow_rx->qudp_rx_cfg.vlan_addr_port)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"Vlan|%u| ",flow_rx->qudp_rx_cfg.vlan_addr_port);
	offset = offset % max_str_size;
	if(flow_rx->qudp_rx_cfg.udp_dst_port)
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"UDPPort|%u| ",flow_rx->qudp_rx_cfg.udp_dst_port);
	offset = offset % max_str_size;

	if(ip_sum){
		if(ECPRISS_IPV4_TYPE == flow_rx->qudp_rx_cfg.ip_type){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"IP|");
			offset = offset % max_str_size;
			for(i=0;i<4;i++){
				offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%u.",flow_rx->qudp_rx_cfg.ip_dst_addr[i]);
				offset = offset % max_str_size;
			}
			cmd_buf[offset-1]='|';
		}else if(ECPRISS_IPV6_TYPE == flow_rx->qudp_rx_cfg.ip_type){
			offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"IP|");
			offset = offset % max_str_size;
			for(i=0;i<16;i++){
				offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x",flow_rx->qudp_rx_cfg.ip_dst_addr[i]);
				offset = offset % max_str_size;
				offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%x:",flow_rx->qudp_rx_cfg.ip_dst_addr[++i]);
				offset = offset % max_str_size;
			}
			cmd_buf[offset-1]='|';
		}
	}
	*offset_p = offset;
	return;
}
/*
 * It has been implement to log all config, deconfig and reconfig events with params
 */
void ecpriss_debug_flow_info(ecpriss_packet_payload_s *packet, uint8_t msg_id)
{

	char cmd_buf[1024];
	uint32_t max_str_size = 0;
	uint32_t offset = 0;

	if(!packet){
		ECPRILOGERR("ecpriss_debug_flow_info: Null Input \n");
		return;
	}

	max_str_size = sizeof(cmd_buf);
	memset(cmd_buf,0,max_str_size);

	if(msg_id == ECPRISS_MESSAGE_FLOW_CFG ||
			msg_id == ECPRISS_MESSAGE_TRANSPORT_CFG){

		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%s","CFG ");
		offset = offset % max_str_size;

	}else if(msg_id == ECPRISS_MESSAGE_FLOW_DECFG  ||
			msg_id == ECPRISS_MESSAGE_TRANSPORT_DECFG ||
			msg_id == ECPRISS_MESSAGE_FLOW_TRANSP_DECFG ){

		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%s","DeCFG ");
		offset = offset % max_str_size;

	}else if(msg_id == ECPRISS_MESSAGE_TRANSPORT_EGRESS_L2_TABLE_RECFG ||
			msg_id == ECPRISS_MESSAGE_TRANSPORT_EGRESS_L3_TABLE_RECFG ||
			msg_id == ECPRISS_MESSAGE_TRANSPORT_EGRESS_L2_L3_TABLE_RECFG ||
			msg_id == ECPRISS_MESSAGE_TRANSPORT_INGESS_TABLE_CFG ||
			msg_id == ECPRISS_MESSAGE_TRANSPORT_INGESS_TABLE_DECFG){
		offset += scnprintf(cmd_buf + offset ,max_str_size - offset ,"%s","ReCFG");
		offset = offset % max_str_size;

	}else{
		return;
	}


	switch(msg_id){

		case ECPRISS_MESSAGE_FLOW_CFG :
		case ECPRISS_MESSAGE_FLOW_TRANSP_DECFG :
			ecpriss_debug_stringify_flow_tp_cfg(packet, cmd_buf, &offset, max_str_size);
			break;
		case ECPRISS_MESSAGE_FLOW_DECFG:
			ecpriss_debug_stringify_flow_cfg(packet, cmd_buf, &offset, max_str_size);
			break;
		case ECPRISS_MESSAGE_TRANSPORT_EGRESS_L2_TABLE_RECFG:
		case ECPRISS_MESSAGE_TRANSPORT_EGRESS_L3_TABLE_RECFG:
		case ECPRISS_MESSAGE_TRANSPORT_EGRESS_L2_L3_TABLE_RECFG:
			ecpriss_debug_stringify_egress_tp_cfg(packet, cmd_buf, &offset, max_str_size);
			break;
		case ECPRISS_MESSAGE_TRANSPORT_INGESS_TABLE_CFG :
		case ECPRISS_MESSAGE_TRANSPORT_INGESS_TABLE_DECFG :
			ecpriss_debug_stringify_ingress_tp_cfg(packet, cmd_buf, &offset, max_str_size);
			break;

	}
	cmd_buf[++offset]= 0;
	ECPRILOGCFG("%s",cmd_buf);
}


#if 0
void ecpriss_dump_pdata(void)
{
    int i,j,k;
	ECPRILOGERR("ecpriss_pdata->ecpri_state %u ",ecpriss_pdata->ecpri_state);

	ECPRILOGERR("ecpriss_pdata->callback_flag->eth_link_callback_rcvd %u ",ecpriss_pdata->callback_flag->eth_link_callback_rcvd);
	ECPRILOGERR("ecpriss_pdata->callback_flag->dma_callback_rcvd %u ",ecpriss_pdata->callback_flag->dma_callback_rcvd);
	ECPRILOGERR("ecpriss_pdata->callback_flag->ssr_callback_rcvd %u ",ecpriss_pdata->callback_flag->ssr_callback_rcvd);
	ECPRILOGERR("ecpriss_pdata->callback_flag->macsec_callback_rcvd %u ",ecpriss_pdata->callback_flag->macsec_callback_rcvd);

	ECPRILOGERR("ecpriss_pdata->user_pid %u ",ecpriss_pdata->user_pid);

	ECPRILOGERR("ecpriss_pdata->netlink_socket %u ",ecpriss_pdata->netlink_socket);

	ECPRILOGERR("ecpriss_pdata->dma_endp->num_of_port_types  %u ",ecpriss_pdata->dma_endp->num_of_port_types);

	ECPRILOGERR("ecpriss_pdata->dma_endp->flv  %u ",ecpriss_pdata->dma_endp->flv);

	for(i=0;i<ECPRI_DMA_ENDP_STREAM_DEST_MAX;i++)
	{
		ECPRILOGERR("ecpriss_pdata->dma_endp->topology_params[%d].num_of_ports %u",i,ecpriss_pdata->dma_endp->topology_params[i].num_of_ports);
		ECPRILOGERR("ecpriss_pdata->dma_endp->topology_params[%d].port_type %u",i,ecpriss_pdata->dma_endp->topology_params[i].port_type);
		for(j=0;j<ECPRI_DMA_NUM_PORT_MAX;j++)
		{
		 ECPRILOGERR("ecpriss_pdata->dma_endp->topology_params[%d].dma_port_param[%d].port_index %u",i,j,ecpriss_pdata->dma_endp->topology_params[i].dma_port_param[j].port_index);
		 ECPRILOGERR("ecpriss_pdata->dma_endp->topology_params[%d].dma_port_param[%d].num_of_rings %u",i,j,ecpriss_pdata->dma_endp->topology_params[i].dma_port_param[j].num_of_rings);
		  for(k=0;k<ECPRI_DMA_RING_PER_PORT_MAX;k++)
		  {
		   ECPRILOGERR("ecpriss_pdata->dma_endp->topology_params[%d].dma_port_param[%d].dma_rings_param[%d].link_index %u",i,j,k,ecpriss_pdata->dma_endp->topology_params[i].dma_port_param[j].dma_rings_param[k].link_index);
		   ECPRILOGERR("ecpriss_pdata->dma_endp->topology_params[%d].dma_port_param[%d].dma_rings_param[%d].nfapi_vm_id %u",i,j,k,ecpriss_pdata->dma_endp->topology_params[i].dma_port_param[j].dma_rings_param[k].nfapi_vm_id);
		   ECPRILOGERR("Vecpriss_pdata->dma_endp->topology_params[%d].dma_port_param[%d].dma_rings_param[%d].dma_ring_type %u",i,j,k,ecpriss_pdata->dma_endp->topology_params[i].dma_port_param[j].dma_rings_param[k].dma_ring_type);
		   ECPRILOGERR("ecpriss_pdata->dma_endp->topology_params[%d].dma_port_param[%d].dma_rings_param[%d].src_dma_ring_id %u",i,j,k,ecpriss_pdata->dma_endp->topology_params[i].dma_port_param[j].dma_rings_param[k].src_dma_ring_id);
		   ECPRILOGERR("ecpriss_pdata->dma_endp->topology_params[%d].dma_port_param[%d].dma_rings_param[%d].dest_dma_ring_id %u",i,j,k,ecpriss_pdata->dma_endp->topology_params[i].dma_port_param[j].dma_rings_param[k].dest_dma_ring_id);
		  }
		}
	}

	ECPRILOGERR("ecpriss_pdata->eth_topology_params->eth_topology_init_done %u ", ecpriss_pdata->eth_topology_params->eth_topology_init_done);
	ECPRILOGERR("ecpriss_pdata->eth_topology_params->num_unique_port_types %u ", ecpriss_pdata->eth_topology_params->num_unique_port_types);

	for(i=0;i<ECPRISS_MAX_UNIQUE_PORT;i++)
	{
		ECPRILOGERR("ecpriss_pdata->eth_topology_params->topology_params[%d].port_type %u ",i,ecpriss_pdata->eth_topology_params->topology_params[i].port_type);
		ECPRILOGERR("ecpriss_pdata->eth_topology_params->topology_params[%d].num_ports %u ",i,ecpriss_pdata->eth_topology_params->topology_params[i].num_ports);
		for(j=0;j<ECPRISS_MAX_PORTS;j++)
		{
			ECPRILOGERR("ecpriss_pdata->eth_topology_params->topology_params[%d].port_params[%d].port_index %u ",i,j,ecpriss_pdata->eth_topology_params->topology_params[i].port_params[j].port_index);
			ECPRILOGERR("ecpriss_pdata->eth_topology_params->topology_params[%d].port_params[%d].num_links %u ",i,j,ecpriss_pdata->eth_topology_params->topology_params[i].port_params[j].num_links);
			for(k=0;k<ECPRISS_MAX_LINKS;k++)
			{
				ECPRILOGERR("ecpriss_pdata->eth_topology_params->topology_params[%d].port_params[%d].link_params[%d].link_index %u ",ecpriss_pdata->eth_topology_params->topology_params[i].port_params[j].link_params[k].link_index);
	            ECPRILOGERR("ecpriss_pdata->eth_topology_params->topology_params[%d].port_params[%d].link_params[%d].link_mtu %u ",ecpriss_pdata->eth_topology_params->topology_params[i].port_params[j].link_params[k].link_mtu);
				ECPRILOGERR("ecpriss_pdata->eth_topology_params->topology_params[%d].port_params[%d].link_params[%d].link_state %u ",ecpriss_pdata->eth_topology_params->topology_params[i].port_params[j].link_params[k].link_state);
				ECPRILOGERR("ecpriss_pdata->eth_topology_params->topology_params[%d].port_params[%d].link_params[%d].link_rate %u ",ecpriss_pdata->eth_topology_params->topology_params[i].port_params[j].link_params[k].link_rate);
			}
		}
	}
	ECPRILOGERR("ecpriss_pdata->ecpriss_core_logbuf %u ",ecpriss_pdata->ecpriss_core_logbuf);

	ECPRILOGERR("ecpriss_pdata->events_workqueue->ecpriss_eth_events_rdy_work %u",ecpriss_pdata->events_workqueue->ecpriss_eth_events_rdy_work);

	ECPRILOGERR("ecpriss_pdata->events_workqueue->ecpriss_dma_events_rdy_work %u",ecpriss_pdata->events_workqueue->ecpriss_dma_events_rdy_work);

	ECPRILOGERR("ecpriss_pdata->events_workqueue->ecpriss_eth_topology_events_rdy_work %u",ecpriss_pdata->events_workqueue->ecpriss_eth_topology_events_rdy_work);

	ECPRILOGERR("ecpriss_pdata->events_workqueue->kernel_events_workqueue %u",ecpriss_pdata->events_workqueue->kernel_events_workqueue);

	ECPRILOGERR("ecpriss_pdata->interrupts_workqueue->ecpriss_interrupt_events_rdy_work %u",ecpriss_pdata->interrupts_workqueue->ecpriss_interrupt_events_rdy_work);

	ECPRILOGERR("ecpriss_pdata->interrupts_workqueue->ecpriss_interrupts_workq %u",ecpriss_pdata->interrupts_workqueue->ecpriss_interrupts_workq);

	ECPRILOGERR("ecpriss_pdata->ready_cb %u",ecpriss_pdata->ready_cb);

	ECPRILOGERR("ecpriss_pdata->dev_mode %u",ecpriss_pdata->dev_mode);

	ECPRILOGERR("ecpriss_pdata->qudp_ctx->state %u",ecpriss_pdata->qudp_ctx->state);

	ECPRILOGERR("ecpriss_pdata->qudp_ctx->num_ports %u",ecpriss_pdata->qudp_ctx->num_ports);

    for(i=0;i<ecpriss_pdata->qudp_ctx->fh_port_cfg[0].port_index;i++)
	{
		ECPRILOGERR("ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].port_index %u",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].port_index);
		ECPRILOGERR("ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.num_ip_fltr_entries %u",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.num_ip_fltr_entries);
		ECPRILOGERR("ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.num_vlan_fltr_entries %u",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.num_vlan_fltr_entries);
		ECPRILOGERR("ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.num_udp_fltr_entries %u",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.num_udp_fltr_entries);
		for(j=0;j<MAX_WHITELIST_ENTRIES;j++)
		{
			for(k=0;k<ECPRISS_IP_ADDR_MAX_WORDS;k++)
			{
				ECPRILOGERR("ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.ipdst_addr[%d][%d] %u",i,j,k,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.ipdst_addr[j][k]);
			}
		}
		for(j=0;j<MAX_WHITELIST_ENTRIES;j++)
		{
			  ECPRILOGERR("ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.vlan_addr[%d] %u",i,j,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.vlan_addr[j]);
			  ECPRILOGERR("ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.udp_port[%d] %u",i,j,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.udp_port[j]);
		}
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config. enable_ip_dst_filt %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config. enable_ip_dst_filt);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.enable_udp_dst_class %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.enable_udp_dst_class);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.enable_vlan_filt %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.enable_vlan_filt);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.enable_udp_cs_check %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.enable_udp_cs_check);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.enable_ip_len_check %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.enable_ip_len_check);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.ipv4_cs_err_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.ipv4_cs_err_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.udp_cs_err_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.udp_cs_err_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.fcs_err_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.fcs_err_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.pkt_err_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.pkt_err_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.ip_len_err_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.ip_len_err_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.disable_st_and_fw %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.disable_st_and_fw);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.vlan_filt_miss_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.vlan_filt_miss_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.ip_filt_miss_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.ip_filt_miss_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.enable_mac_dst_check %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.enable_mac_dst_check);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.use_external_not_local_mac_dst %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.use_external_not_local_mac_dst);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.enable_broadcast_checkk %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.enable_broadcast_check);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.last_in_chain %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.last_in_chain);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.non_local_dst_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.non_local_dst_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.enable_shared_filtering_2_links %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.enable_shared_filtering_2_links);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.enable_shared_filtering_4_links %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.enable_shared_filtering_4_links);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.enable_eth_padding_removal %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.enable_eth_padding_removal);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.fh_ingress_config.reserved0 %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.fh_ingress_config.reserved0);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.enable_ip_dst_filt %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.enable_ip_dst_filt);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.enable_udp_dst_class %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.enable_udp_dst_class);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.enable_vlan_filt %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.enable_vlan_filt);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.enable_udp_cs_check %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.enable_udp_cs_check);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.enable_ip_len_check %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.enable_ip_len_check);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.ipv4_cs_err_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.ipv4_cs_err_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.udp_cs_err_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.udp_cs_err_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.fcs_err_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.fcs_err_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.pkt_err_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.pkt_err_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.ip_len_err_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.ip_len_err_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.disable_st_and_fw %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.disable_st_and_fw);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.vlan_filt_miss_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.vlan_filt_miss_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.ip_filt_miss_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.ip_filt_miss_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.enable_mac_dst_check %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.enable_mac_dst_check);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.use_external_not_local_mac_dst %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.use_external_not_local_mac_dst);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.enable_broadcast_check %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.enable_broadcast_check);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.last_in_chain %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.last_in_chain);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.non_local_dst_action %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.non_local_dst_action);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.reserved0 %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.reserved0);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.enable_eth_padding_removal %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.enable_eth_padding_removal);
		ECPRILOGERR(" ecpriss_pdata->qudp_ctx->fh_port_cfg[%d].ingress_port_cfg.l2_ingress_config.reserved1 %u ",i,ecpriss_pdata->qudp_ctx->fh_port_cfg[i].ingress_port_cfg.l2_ingress_config.reserved1);
	}

		ECPRILOGERR(" ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.port_index %u ",ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.port_index);
		ECPRILOGERR(" ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.num_links %u ",ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.num_links);

		for(j=0;j<ECPRISS_MAX_LINKS;j++)
		{
			ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[%d].link_index %u ",j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[j].link_index);
			ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[%d].link_mtu %u ",j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[j].link_mtu);
			for(k=0;k<ECPRISS_MAC_ADDR_LEN;k++)
			{
			ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[%d].eth_mac_addr[%d] %u ",j,k,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[j].eth_mac_addr[k]);
			}
			ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].port_index %u ",j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[j].link_state);
			ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[%d].link_rate %u ",j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[j].link_rate);
		}

		for(i=0;i<ECPRISS_MAX_PORTS;i++)
		{
			ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].port_indexs %u ",i,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[i].port_index);
			ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].num_of_rings %u ",i,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[i].num_of_rings);
			for(j=0;j<ECPRI_DMA_RING_PER_PORT_MAX;j++)
			{
			  ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].dma_rings_param[%d].link_index%u ",i,j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[i].dma_rings_param[j].link_index);
			  ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].dma_rings_param[%d].nfapi_vm_id %u ",i,j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[i].dma_rings_param[j].nfapi_vm_id);
			  ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].dma_rings_param[%d].dma_ring_type %u ",i,j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[i].dma_rings_param[j].dma_ring_type);
			  ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].dma_rings_param[%d].src_dma_ring_id %u ",i,j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[i].dma_rings_param[j].src_dma_ring_id);
			  ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].dma_rings_param[%d].dest_dma_ring_id %u ",i,j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[i].dma_rings_param[j].dest_dma_ring_id);
			}
		}

		for(j=0;j<ECPRISS_MAX_LINKS;j++)
		{
			ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[%d].link_index %u ",j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[j].link_index);
			ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[%d].link_mtu %u ",j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[j].link_mtu);
			for(k=0;k<ECPRISS_MAC_ADDR_LEN;k++)
			{
			ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[%d].eth_mac_addr[%d] %u ",j,k,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[j].eth_mac_addr[k]);
			}
			ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[%d].link_state %u ",j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[j].link_state);
			ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[%d].link_rate %u ",j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.eth_cfg.link_params[j].link_rate);
        }

		for(i=0;i<ECPRISS_MAX_PORTS;i++)
		{
			ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].port_index %u ",i,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[i].port_index);
			ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].num_of_rings %u ",i,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[i].num_of_rings);
			for(j=0;j<ECPRI_DMA_RING_PER_PORT_MAX;j++)
			{
			  ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].dma_rings_param[%d].link_index %u ",i,j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[i].dma_rings_param[j].link_index);
			  ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].dma_rings_param[%d].nfapi_vm_id %u ",i,j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[i].dma_rings_param[j].nfapi_vm_id);
			  ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].dma_rings_param[%d].dma_ring_type %u ",i,j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[i].dma_rings_param[j].dma_ring_type);
			  ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].dma_rings_param[%d].src_dma_ring_id %u ",i,j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[i].dma_rings_param[j].src_dma_ring_id);
			  ECPRILOGERR("ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[%d].dma_rings_param[%d].dest_dma_ring_id %u ",i,j,ecpriss_pdata->xbar_ctx->fh_exception_port_cfg.dma_port_cfg[i].dma_rings_param[j].dest_dma_ring_id);
			}
		}

		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_cfg %u ",ecpriss_pdata->xbar_ctx->interrupt_cfg);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_cfg %u ",ecpriss_pdata->xbar_ctx->interrupt_cfg);

		for(i=0;i<TOTAL_LINKS;i++)
		{
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_fhrx_pkt_cnt[%d] %u ",i,ecpriss_pdata->xbar_ctx->stats.xbar_fhrx_pkt_cnt[i]);
		}

		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_fhrx_dma_pkt_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_fhrx_dma_pkt_cnt);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_fhrx_uc_pkt_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_fhrx_uc_pkt_cnt);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_fhrx_uc_err_pkt_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_fhrx_uc_err_pkt_cnt);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_fhrx_err_pkt_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_fhrx_err_pkt_cnt);

		for(i=0;i<TOTAL_LINKS;i++)
		{
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_fhtx_pkt_cnt[%d] %u ",ecpriss_pdata->xbar_ctx->stats.xbar_fhtx_pkt_cnt[i]);
		}

		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_fhtx_c2c_pkt_ovf_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_fhtx_c2c_pkt_ovf_cnt);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_fhtx_dma_pkt_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_fhtx_dma_pkt_cnt);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_c2c_pkt_ovf_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_c2c_pkt_ovf_cnt);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_c2crx_dma_pkt_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_c2crx_dma_pkt_cnt);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_c2crx_err_pkt_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_c2crx_err_pkt_cnt);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_fhtx_uc_pkt_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_fhtx_uc_pkt_cnt);

		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh0 %u ",ecpriss_pdata->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh0);
		ECPRILOGERR("Vecpriss_pdata->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh1 %u ",ecpriss_pdata->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh1);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh2 %u ",ecpriss_pdata->xbar_ctx->stats.xbar_ocrx_fh_buff_watermark_fh2);

		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_0_1_buff_watermark_cc0 %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_0_1_buff_watermark_cc0);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_0_1_buff_watermark_cc1 %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_0_1_buff_watermark_cc1);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_2_3_buff_watermark_cc2 %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_2_3_buff_watermark_cc2);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_2_3_buff_watermark_cc3 %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_2_3_buff_watermark_cc3);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_2_3_buff_watermark_cc3 %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_2_3_buff_watermark_cc3);

		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.octx_oc_0_1_buff_watermark_cc0 %u ",ecpriss_pdata->xbar_ctx->stats.octx_oc_0_1_buff_watermark_cc0);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.octx_oc_0_1_buff_watermark_cc1 %u ",ecpriss_pdata->xbar_ctx->stats.octx_oc_0_1_buff_watermark_cc1);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.octx_oc_2_3_buff_watermark_cc2 %u ",ecpriss_pdata->xbar_ctx->stats.octx_oc_2_3_buff_watermark_cc2);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.octx_oc_2_3_buff_watermark_cc3 %u ",ecpriss_pdata->xbar_ctx->stats.octx_oc_2_3_buff_watermark_cc3);

		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_0_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_0_cnt);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_1_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_1_cnt);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_2_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_fhrx_unknown_pcid_cnt_fhrx_2_cnt);

		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_0_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_0_cnt);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_1_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_1_cnt);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_2_cnt %u ",ecpriss_pdata->xbar_ctx->stats.xbar_dbg_ocrx_unknown_pcid_cnt_ocrx_fh_2_cnt);

		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_stats.octx_fh_len_err %u ",ecpriss_pdata->xbar_ctx->interrupt_stats.octx_fh_len_err);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_stats.octx_c2c_len_err %u ",ecpriss_pdata->xbar_ctx->interrupt_stats.octx_c2c_len_err);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_stats.fhtx_c2c_overflow %u ",ecpriss_pdata->xbar_ctx->interrupt_stats.fhtx_c2c_overflow);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_stats.c2ctx_fh_overflow %u ",ecpriss_pdata->xbar_ctx->interrupt_stats.c2ctx_fh_overflow);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_stats.octx_fh_overflow %u ",ecpriss_pdata->xbar_ctx->interrupt_stats.octx_fh_overflow);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_stats.octx_c2c_overflow %u ",ecpriss_pdata->xbar_ctx->interrupt_stats.octx_c2c_overflow);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_stats.fhrx_uc_pkt_pending %u ",ecpriss_pdata->xbar_ctx->interrupt_stats.fhrx_uc_pkt_pending);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_stats.fhrx_uc_overflow %u ",ecpriss_pdata->xbar_ctx->interrupt_stats.fhrx_uc_overflow);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_stats.fhrx_uc_pkt_err %u ",ecpriss_pdata->xbar_ctx->interrupt_stats.fhrx_uc_pkt_err);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_stats.fhrx_uc_pkt_drop %u ",ecpriss_pdata->xbar_ctx->interrupt_stats.fhrx_uc_pkt_drop);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_stats.ocrx_unknown_pcid %u ",ecpriss_pdata->xbar_ctx->interrupt_stats.ocrx_unknown_pcid);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_stats.fhrx_unknown_pcid %u ",ecpriss_pdata->xbar_ctx->interrupt_stats.fhrx_unknown_pcid);
		ECPRILOGERR("ecpriss_pdata->xbar_ctx->interrupt_stats.c2crx_unkown_pcid %u ",ecpriss_pdata->xbar_ctx->interrupt_stats.c2crx_unkown_pcid);
		ECPRILOGERR("ecpriss_pdata->cfg_stats.xbar_cfg.global_cfg.global %u ",ecpriss_pdata->cfg_stats.xbar_cfg.global_cfg.global);

		for(i=0;i<NUM_OF_FHP;i++)
		{
			for(j=0;j<LUT_INDEX;j++)
			{
		      ECPRILOGERR("ecpriss_pdata->cfg_stats.xbar_cfg.lut_cfg.ocrx[%d][%d] %u ",i,j,ecpriss_pdata->cfg_stats.xbar_cfg.lut_cfg.ocrx[i][j]);
		      ECPRILOGERR("ecpriss_pdata->cfg_stats.xbar_cfg.lut_cfg.fhrx[%d][%d] %u ",i,j,ecpriss_pdata->cfg_stats.xbar_cfg.lut_cfg.fhrx[i][j]);
		      ECPRILOGERR("ecpriss_pdata->cfg_stats.xbar_cfg.lut_cfg.c2crxd %u ",ecpriss_pdata->cfg_stats.xbar_cfg.lut_cfg.c2crxdl);
		      ECPRILOGERR("ecpriss_pdata->cfg_stats.xbar_cfg.lut_cfg.c2crxul %u ",ecpriss_pdata->cfg_stats.xbar_cfg.lut_cfg.c2crxul);
			}
		}
}
#endif


MODULE_LICENSE("GPL");
module_init(ecpriss_core_module_init);
module_exit(ecpriss_core_module_exit);

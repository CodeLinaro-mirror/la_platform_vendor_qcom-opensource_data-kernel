/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ECPRISS_CORE_H_
#define ECPRISS_CORE_H_

#include <linux/module.h>
#include <linux/kernel.h>
//#include <linux/delay.h>
#include <net/netlink.h>
#include <net/net_namespace.h>
#include <linux/signal.h>           /* Definition of SIGEV_* constants */
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/netdevice.h>
#include <linux/of_device.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/notifier.h>
#include <soc/qcom/subsystem_notif.h>
#include <linux/remoteproc/qcom_rproc.h>
#include "ecpri_dma_ecpri_ss.h"
#include "eth_ecpriss_iface.h"
#include "ecpriss_xbar.h"
#include "ecpriss_qudp.h"
#include "ecpriss_flow.h"
#include "ecpriss_debugfs.h"
#include "ecpriss_mhi.h"
#include "csm_lte_eth_dev.h"

#define ECPRI_DMA_RING_PER_PORT_MAX 4

#define ENABLE_ECPRI_TEST    1

#define MAX_PORTS 3
#define MAX_MAC_LINKS 4

#define ECPRI_CLK_FREQ(x) (x * 1000 * 1000UL)

/* ECPRI clock */
#define ECPRI_CG_CLK_NOM_MAX (ECPRI_CLK_FREQ(466.50))
#define ECPRI_MSS_ORAN_NOM_MAX (ECPRI_CLK_FREQ(500))

extern int cascade_enable;
extern int ru_cascade_mode;

typedef enum
{
	ECPRI_CORE_PRE_INIT,
	ECPRI_CORE_COLD_INIT,
	ECPRI_CORE_INIT,
	ECPRI_CORE_DEINIT
} ecpriss_core_state_e;

/**
 * enum dev mode
 */

typedef enum {
	ECPRISS_DEV_MODE_NONE = 0,
	ECPRISS_DEV_MODE_RU = 1,
	ECPRISS_DEV_MODE_DU_PCIE = 2,
	ECPRISS_DEV_MODE_DU_PCIE_3_X_12 = ECPRI_HW_FLAVOR_DU_PCIE,
	ECPRISS_DEV_MODE_DU_L2 = 3,
	ECPRISS_DEV_MODE_DU_PCIE_4_X_9 = 4,
	ECPRISS_DEV_DEV_DU_PCIE_5_X_6 = 5,
	ECPRISS_DEV_MODE_MAX
}ecpriss_dev_mode_e;


typedef struct ecpriss_core_callback_flags
{
	uint32_t eth_link_callback_rcvd;
	uint32_t dma_callback_rcvd;
	uint32_t ssr_callback_rcvd;
	uint32_t macsec_callback_rcvd;
}ecpriss_core_callback_flags_s;


/**
 * enum ecpriss_port_dir
 */
typedef enum {
	ECPRISS_PORT_DIR_RX = 0,
	ECPRISS_PORT_DIR_TX,
	ECPRISS_PORT_DIR_MAX
}ecpriss_port_dir_e;



/**
 * enum ecpriss_eth_link_rate - link rate(Gbps)
 */
typedef enum {
	ECPRISS_ETH_LINK_RATE_1000 = 0,
	ECPRISS_ETH_LINK_RATE_100,
	ECPRISS_ETH_LINK_RATE_25,
	ECPRISS_ETH_LINK_RATE_10,
	ECPRISS_ETH_LINK_RATE_MAX,
}ecpriss_eth_link_rate_e;

/**
 * enum ecpriss_eth_port_type - port type
 */
typedef enum {
	ECPRISS_PORT_TYPE_FH = 0,
	ECPRISS_PORT_TYPE_C2C,
	ECPRISS_PORT_TYPE_L2,
	ECPRISS_PORT_TYPE_OC,
	ECPRISS_PORT_TYPE_MAX
}ecpriss_port_type_e;



/**
 * enum eth_ecpriss_event_e - event type
 */
typedef enum {
	ECPRISS_ETH_EVENT_CREATE = 0,
	ECPRISS_ETH_EVENT_UP,
	ECPRISS_ETH_EVENT_DOWN,
	ECPRISS_ETH_EVENT_DESTROY,
	ECPRISS_ETH_EVENT_MAX
}ecpriss_eth_event_e;



typedef struct ecpri_kernel_events_workqueue_params
{
	struct work_struct          *ecpriss_eth_events_rdy_work;
	struct work_struct          *ecpriss_dma_events_rdy_work;
	struct work_struct          *ecpriss_eth_topology_events_rdy_work;
	struct work_struct          *ecpriss_ssr_events_rdy_work;
	struct workqueue_struct     *kernel_events_workqueue;
}ecpri_events_workqueue_params_s;


typedef struct ecpri_interrupt_events_workqueue_params
{
	struct work_struct          *ecpriss_interrupt_events_rdy_work;
	struct workqueue_struct     *ecpriss_interrupts_workq;
}ecpri_interrupt_workqueue_params_s;

typedef struct ecpriss_core_clock {
	struct clk* ecpri_cg;
	struct clk* ecpri_fr;
	struct clk* ecpri_eth_100G_fh0;
	struct clk* ecpri_eth_100G_fh1;
	struct clk* ecpri_eth_100G_fh2;
	struct clk* ecpri_eth_100G_c2c0;
	struct clk* ecpri_eth_100G_c2c1;
	struct clk* ecpri_eth_100G_dbg_c2c;
	struct clk* ecpri_oran_div2 ;
	struct clk* ecpri_mss_oran;
}ecpri_clock;

typedef struct ecpri_stats_timer_params
{
	struct timer_list stats_timer;
	uint32_t stats_interval;
	uint8_t stats_timer_running;
}ecpri_stats_timer_params_s;

struct ecpriss_ssr_nb {
	const char *ssr_label;
	void *notifier_handle;
	struct notifier_block nb;
	atomic_t curr_ssr_state;
};

/**
 * struct ecpri_dma_endp_cfg - DMA endpoint configurations
 * @ecpri_state:
 * @callback_flag:
 * @user_pid:
 * @netlink_socket:
 * @dma_endp:
 * @eth_link_params:
 * @flow_id_tbl:
 * @stats:
 * @ecpriss_core_logbuf:
 */
typedef struct ecpriss_core_private_s {
	ecpriss_core_state_e                  ecpri_state;
	ecpriss_core_callback_flags_s        *callback_flag;
	uint32_t                              user_pid;
	struct sock                          *netlink_socket;
	struct ecpri_dma_endp_mapping        *dma_endp;
	eth_ecpriss_topology_root_s          *eth_topology_params;
	void                                 *ecpriss_core_logbuf;
	void                                 *ecpriss_core_cfg_logbuf;
	ecpri_events_workqueue_params_s      *events_workqueue;
	ecpri_interrupt_workqueue_params_s   *interrupts_workqueue;
	eth_ecpriss_topology_ready_cb        *ready_cb;
	ecpriss_dev_mode_e                    dev_mode;
	ecpriss_qudp_ctx_s                   *qudp_ctx;
	ecpriss_xbar_ctx_s                   *xbar_ctx;
	ecpriss_config_stats_s                cfg_stats;
	ecpri_stats_timer_params_s            stats_timer_info;
	ecpriss_hw_name_e                     ecpri_hw_ver;
	struct mutex                          ecpriss_mutex_lock;
	spinlock_t                            irq_lock;
} ecpriss_core_private_s;

typedef struct ecpriss_core_private_s_v2 {
	ecpriss_core_state_e                  ecpri_state;
	ecpriss_core_callback_flags_s        *callback_flag;
	uint32_t                              user_pid;
	struct sock                          *netlink_socket;
	struct sock                          *stats_netlink_socket;
	struct ecpri_dma_endp_mapping        *dma_endp;
	eth_ecpriss_topology_root_s          *eth_topology_params;
	void                                 *ecpriss_core_logbuf;
	void                                 *ecpriss_core_cfg_logbuf;
	ecpri_events_workqueue_params_s      *events_workqueue;
	ecpri_interrupt_workqueue_params_s   *interrupts_workqueue;
	eth_ecpriss_topology_ready_cb        *ready_cb;
	ecpriss_dev_mode_e                    dev_mode;
	ecpriss_qudp_ctx_s_v2                *qudp_ctx_v2;
	ecpriss_xbar_ctx_s_v2                *xbar_ctx_v2;
	ecpriss_config_stats_s_v2             cfg_stats_v2;
	ecpriss_hw_name_e                     ecpri_hw_ver;
	struct ecpriss_ssr_nb		     *ssr_info;
	struct platform_device		     *pdev;
	struct mutex                          ecpriss_mutex_lock;
	void                                 *mhi_ctx;
	spinlock_t                            irq_lock;
	datacsm_ecpriss_stats_info_s          fh_stats_usr;
} ecpriss_core_private_s_v2;


extern ecpriss_core_private_s *ecpriss_pdata;
extern ecpriss_core_private_s_v2 *ecpriss_pdata_v2;
extern ecpriss_xbar_ctx_s     xbar_ctx_g;
extern ecpriss_qudp_ctx_s     qudp_ctx_g;
extern ecpriss_hw_name_e      ecpriss_hw_ver;

void ecpriss_eth_event_processing_wq(struct work_struct *work);
void ecpriss_ssr_events_processing_wq(struct work_struct *work);
void ecpriss_dma_event_processing_wq(struct work_struct *work);
void ecpriss_eth_topology_init_wq(struct work_struct *work);
void ecpriss_interrupt_events_processing_wq(struct work_struct *work);
void ecpriss_update_stats_and_requeue(struct work_struct *work);

int ecpriss_stats_timer_enable(int timeout);
int ecpriss_stats_timer_interrupt_create(void);
void ecpriss_update_all_stats(void);
void ecpriss_update_all_stats_v2(void);
void ecpriss_core_set_stats_timeout_info(int val);
int ecpriss_core_get_stats_timeout_info(void);

void clear_debugfs_directory(void);
void ecpriss_destroy_ipc_log_v2(void);
void ecpriss_unmap_xbar_qudp_v2(void);
void ecpriss_xbar_oc_flush_enable(uint32_t code);
void ecpriss_panic_notifr_handler_v2(void);
void ecpriss_panic_notifr_handler(void);
int ecpriss_get_link_state(eth_ecpriss_port_type_e port_type,
		                int port, int link);
int ecpriss_get_link_rate(eth_ecpriss_port_type_e port_type,
		                int port, int link);


void ecpriss_debug_flow_info(ecpriss_packet_payload_s *packet, uint8_t msg_id);

void ecpriss_xbar_oc_cfg_enable_len_check_v2(uint32_t len_check_action);
void ecpriss_core_set_enable_len_check_info(uint32_t val);
int ecpriss_core_get_enable_len_check_info(void);

int32_t ecpri_send_logging_trigger_to_dma(ecpriss_log_cfg_s *log_cfg);
int32_t ecpriss_configure_logging(ecpriss_packet_payload_s *packet);

#endif

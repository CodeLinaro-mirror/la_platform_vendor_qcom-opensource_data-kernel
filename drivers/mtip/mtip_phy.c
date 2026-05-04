//SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */ 

#include <linux/init.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/gfp.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/bitrev.h>
#include <linux/slab.h>

#include <linux/moduleparam.h>

#include <linux/sched.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/interrupt.h> /* mark_bh */

#include <linux/in.h>
#include <linux/netdevice.h>   /* struct device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <linux/tcp.h>         /* struct tcphdr */
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/of_net.h>

#include "mtip.h"
#include "mtip_device.h"
#include "mtip_phy.h"
#include "eth_phy_iface.h"
#include "mtip_mac.h"
#include "mtip_pcs.h"
#include "mtip_workq.h"
#include "mtip_sysfs.h"
#include "mtip_ethtool.h"
#include "mtip_notifr.h"

extern struct mtip_delayed_work_q_params *delayed_wq_notifr_param;

struct eth_phy_iface_eth_register_params mtip_phy_eth_params;

extern struct eth_phy_iface_ops qcom_aw_phy_driver_iface_ops;

extern struct mutex delayed_wq_mutex_lock;
extern struct workqueue_struct *delayed_wq;

u8 mtip_phy_retry_num[MTIP_MAX_LINKS] = {0};

u8 mtip_phy_an_seq_num[MTIP_MAX_PORTS] = {0};

/* Timer to define the polling time for link to come up, before retrying or
before attempting new speed mode */
int mtip_link_polling_timer = 2000;

/* 
 * qsfp_eth_get_link_type: returns sfp port type
 * based on values defined in ethtool.h
#define PORT_TP            0x00
#define PORT_AUI           0x01
#define PORT_MII           0x02
#define PORT_FIBRE         0x03
#define PORT_BNC           0x04
#define PORT_DA            0x05
#define PORT_NONE          0xef
#define PORT_OTHER         0xff
 */ 
extern int qsfp_eth_get_link_type(u32 qsfp_phandle, u8* link_info);

// PCS level retry delay to bring up PHY lane
#define MTIP_PHY_RETRY_TIMER_INTERVAL       1000

static void mtip_phy_ready_cb(void *user_data)
{
    CSMLOGDBG("Got the PHY ready cb\n");

    if (platform_driver_priv->phy_is_ready == false) 
    {
        // TBD: do we wait for both DMA and PHY?
    }

    // set the phy_is_ready flag
    platform_driver_priv->phy_is_ready = true;
    return;
}

static void mtip_phy_an_result_cb(enum mtip_port_type_enum port_type, bool an_result, enum mtip_port_config_enum port_config, u8 seq_num)
{
    CSMLOGINFO("Got AN complete CB for port: %d seq %d result %d port_config %d %s\n", port_type, seq_num, an_result, port_config, mtip_ethtool_get_port_config_str(port_config));

    if(seq_num != mtip_phy_an_seq_num[port_type])
    {
        CSMLOGINFO("Ignore the older AN result");
        return;
    }

    post_mtip_process_an_result(port_type, an_result, port_config, seq_num);
    return;
}

void mtip_phy_lane_bring_up_progress_ind(u32 link_index, bool in_progress)
{
    CSMLOGDBG("Lane bring up progress: %d for link index %d", in_progress, link_index);

    if(in_progress)
        mtip_mac_clear_link_status_interrupt_mask(link_index);
    else
        mtip_mac_set_link_status_interrupt_mask(link_index);

    return;
}

static void mtip_phy_cdr_lock_ind(u32 link_index, bool status, u8 an_seq_num)
{
    struct mtip_process_cdr_lock_ind* taskstruct;
    u32 port_type;

    if(link_index >= MTIP_MAX_LINKS)
    {
        CSMLOGINFO("Invalid link index %d", link_index);
        return;
    }

    if(mtip_lookup_port_type_by_link_index(link_index, &port_type) != 0)
    {
        CSMLOGINFO("Invalid link/port!");
        return;
    }

    if(an_seq_num != 0 &&
       an_seq_num != mtip_phy_an_seq_num[port_type])
    {
        CSMLOGINFO("Ignore the older CDR lock status");
        return;
    }

    taskstruct = kmalloc(sizeof(struct mtip_process_cdr_lock_ind), GFP_ATOMIC);
    if(taskstruct == NULL)
    {
      CSMLOGERR("memory alloc failed\n");
      return;
    }

    taskstruct->link_index= link_index;
    taskstruct->status = status;
    taskstruct->an_seq_num = an_seq_num;
    mtip_queue_work(MTIP_WORKQ_TASK_PROCESS_CDR_LOCK_IND, taskstruct, port_type);
    return;
}

u32 mtip_phy_get_max_retry_num(void)
{
    u32 max_retries_num = 0;
    u16 random_timer;

    // Get a random number
    get_random_bytes(&random_timer, sizeof(random_timer));

    // Make sure that random value is less than 0.5 of the defined timer
    random_timer %= (mtip_link_polling_timer/2);

    // Max number of retries will be the final timer/retry interval
    max_retries_num = ((mtip_link_polling_timer+random_timer)/MTIP_PHY_RETRY_TIMER_INTERVAL);

    return max_retries_num;
}

void run_mtip_process_cdr_lock_ind(void* workptr)
{
    struct mtip_process_cdr_lock_ind *taskstruct = (struct mtip_process_cdr_lock_ind *)workptr;
    u32 link_index = taskstruct->link_index;
    bool status = taskstruct->status;
    u8 an_seq_num = taskstruct->an_seq_num;
    u32 port_type;
    u32 lane_index;

    if(link_index >= MTIP_MAX_LINKS)
    {
        CSMLOGINFO("Invalid link index %d", link_index);
        goto out;
    }

    if(platform_driver_priv == NULL || platform_driver_priv->mtip_links[link_index] == NULL)
         goto out;

    if(mtip_lookup_port_type_by_link_index(link_index, &port_type) != 0)
    {
        CSMLOGINFO("Invalid link/port!");
        goto out;
    }

    if(platform_driver_priv->mtip_ports[port_type]->needs_rx_los_processing)
    {
        CSMLOGDBG("Skip processing as Rx LOS is set for port %d", port_type);
        goto out;
    }

    if((status == false) &&
       (platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_UP) &&
       (mtip_phy_retry_num[link_index] >= mtip_phy_get_max_retry_num()))
    {
        CSMLOGDBG("Max retries already done for link index %d", link_index);
        goto out;
    }

    if(platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_CLOSE ||
       platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_OPEN_WAITING_FOR_LANES)
    {
        goto out;
    }

    CSMLOGINFO("CDR lock indication for link_index %d, status %d, an_seq_num %d\n",
               link_index, status, an_seq_num);

    if(an_seq_num != 0 &&
       an_seq_num != mtip_phy_an_seq_num[port_type])
    {
        CSMLOGINFO("Ignore the older CDR lock status");
        goto out;
    }

    // Fetch the first lane index
    lane_index = platform_driver_priv->mtip_links[link_index]->assigned_lane_indices[0];

    // Process link up if MAC wrapper core status register indicates so
    if (mtip_mac_wrapper_get_link_status(link_index) == true) 
    {
        mtip_process_link_state(link_index, true);
    }
    // Retry logic will apply only for FIBRE, or with DAC if AN is disabled
    else if(platform_driver_priv->mtip_ports[port_type]->sfp_port_type == PORT_FIBRE ||
            platform_driver_priv->mtip_ports[port_type]->autoneg == false)
    {
        /* If the PCS link didn't come up, then retry after the configured
           interval. Skip restarting the timer if it was already running */
        if(platform_driver_priv->mtip_links[link_index]->phy_retry_timer_valid == true &&
           !timer_pending(&platform_driver_priv->mtip_links[link_index]->phy_retry_timer))
        {
            mod_timer(&platform_driver_priv->mtip_links[link_index]->phy_retry_timer,
                      jiffies + msecs_to_jiffies(MTIP_PHY_RETRY_TIMER_INTERVAL));
        }
    }
    else if((platform_driver_priv->mtip_ports[port_type]->autoneg == true) &&
            (mtip_mac_wrapper_get_link_status(link_index) == false) &&
            (platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_UP))
    {
        mtip_process_link_state(link_index, false);
    }

out:
    kfree(taskstruct);
    return;
}

int mtip_phy_register_eth(void)
{
    int res = 0;
    bool is_ready = false;
    mtip_phy_eth_params.notify_an_result = mtip_phy_an_result_cb;
    mtip_phy_eth_params.userdata_ready = NULL;
    mtip_phy_eth_params.notify_ready = mtip_phy_ready_cb;
    mtip_phy_eth_params.cdr_lock_ind = mtip_phy_cdr_lock_ind;
    mtip_phy_eth_params.lane_bring_up_progress_ind = mtip_phy_lane_bring_up_progress_ind;

    // register with the PHY
    res = (qcom_aw_phy_driver_iface_ops.eth_phy_iface_eth_register)(&mtip_phy_eth_params, &is_ready);

    if (res < 0) 
    {
        CSMLOGERR("Failed to register with PHY: %d\n", res);
        return res;
    }

    // check if PHY is ready
    if (is_ready)
    {
        platform_driver_priv->phy_is_ready = true;
    }
    return 0;
}

int mtip_phy_deregister_eth(void)
{
    CSMLOGDBG("Deregistering with the phy driver\n");

    // deregister with the PHY driver
    (qcom_aw_phy_driver_iface_ops.eth_phy_iface_eth_deregister)();

    return 0;
}

int mtip_phy_setup_phy(struct mtip_port_device_info* port_device)
{
    int i;
    int rv;
    u32 port_type = port_device->port_type;
    struct mtip_port_info* port_info = NULL;

    // setup the phy for the port
    // pass the consolidated lane config of the port to phy
    CSMLOGDBG("setting up phy for port %d", port_device->port_type);

    port_info = platform_driver_priv->mtip_ports[port_type];

    for (i = 0; i < PHY_LANE_MAX; ++i)
    {
        CSMLOGDBG("lane config[%d] enabled %d speed %d link_index %d", i, 
                  port_info->lane_config[i].lane_enabled, 
                  port_info->lane_config[i].lane_speed,
                  port_info->lane_config[i].link_index);
    }

    rv = (qcom_aw_phy_driver_iface_ops.eth_phy_iface_phy_setup)(port_type, port_info->lane_config);

    CSMLOGDBG("phy_setup returned rv %d", rv);
    return rv;
}

void mtip_phy_get_lanes_of_link(u32 link_index, bool lanes_enabled[PHY_LANE_MAX])
{
    u32 num_lanes;
    int i;
    u32 lane;
    u32 lane_index;
    int lane_count = 0;

    // set lanes enabled to false
    for (i = 0; i < PHY_LANE_MAX; ++i)
    {
        lanes_enabled[i] = false;
    }

    // number of lanes assigned to the link
    num_lanes = platform_driver_priv->mtip_links[link_index]->num_assigned_lanes;

    if (num_lanes == 0) 
    {
        CSMLOGDBG("no lanes assigned to link_index %d", link_index);
        return;
    }

    // enable the assigned lanes
    for (i = 0; i < num_lanes; ++i)
    {
        lane_index = platform_driver_priv->mtip_links[link_index]->assigned_lane_indices[i];

        mtip_lookup_real_lane_number_by_lane_index(lane_index, &lane);

        lanes_enabled[lane] = true;

       // CSMLOGDBG("using lane %d for link_index: %d", lane, link_index);
        ++lane_count;
    }

    //CSMLOGDBG("lane count of link_index: %d is %d\n", link_index, lane_count);
    return;
}

void mtip_phy_retry_timer_cb(struct timer_list *list)
{
    struct mtip_process_phy_retry_bringup* taskstruct;
    struct mtip_link_info *link_info;
    u32 port_type;

    if(!platform_driver_priv)
        return;

    link_info = from_timer(link_info, list, phy_retry_timer);
    if(!link_info)
        return;

    if(mtip_lookup_port_type_by_link_index(link_info->link_index, &port_type))
    {
      CSMLOGERR("invalid link index");
      return;
    }

    taskstruct = kmalloc(sizeof(struct mtip_process_phy_retry_bringup), GFP_ATOMIC);
    if(taskstruct == NULL)
    {
      CSMLOGERR("memory alloc failed\n");
      return;
    }

    taskstruct->link_index = link_info->link_index;
    mtip_queue_work(MTIP_WORKQ_TASK_PROCESS_RETRY_PHY_BRINGUP, taskstruct, port_type);

    return;
}

void run_mtip_phy_retry_bringup(void* workptr)
{
    struct mtip_process_phy_retry_bringup *taskstruct = (struct mtip_process_phy_retry_bringup *)workptr;
    u32 port_type;
    u32 link_index = taskstruct->link_index;
    struct mtip_process_next_speed_mode info = {0};

    if(link_index >= MTIP_MAX_LINKS)
    {
      CSMLOGERR("invalid link_index %d\n", link_index);
      goto func_exit;
    }

    if(!platform_driver_priv || !platform_driver_priv->mtip_links[link_index]){
        CSMLOGERR("platform_driver_priv NULL \n");
        goto func_exit;
    }

    if(platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_CLOSE ||
       platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_OPEN_WAITING_FOR_LANES)
    {
        goto func_exit;
    }

    // If PCS link is up, set the state and return back, else continue
    if(mtip_mac_wrapper_get_link_status(link_index) == true)
    {
        mtip_process_link_state(link_index, true);
        goto func_exit;
    }

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        goto func_exit;
    }

    if(platform_driver_priv->mtip_ports[port_type]->needs_rx_los_processing)
    {
        CSMLOGDBG("Skip processing as Rx LOS is set for port %d", port_type);
        goto func_exit;
    }

    mtip_phy_retry_num[link_index]++;

    // If max attempts for PHY lane bring up retry by PCS are completed,
    if(mtip_phy_retry_num[link_index] >= mtip_phy_get_max_retry_num())
    {
        CSMLOGINFO("Max retries done for link_index %d", link_index);

        /* Switch and process next speed mode if,
           1. Optical module that supports dual rate
           2. Ethtool setting is such that more than one speed mode is advertised
           3. Max attempts to switch the speed mode is not yet reached
        */
        if(platform_driver_priv->mtip_ports[port_type]->sfp_port_type == PORT_FIBRE &&
           platform_driver_priv->mtip_ports[port_type]->multi_rate_supported == true &&
           mtip_device_count_priv_flag_bits(port_type) > 1 &&
           platform_driver_priv->mtip_ports[port_type]->next_speed_retry_count < MTIP_NEXT_SPEED_MODE_RETRY_MAX_COUNT)
        {
            mtip_phy_retry_num[link_index] = 0;

            // check if retry for other speed modes need to be attempted
            info.port_type = port_type;
            info.link_index = link_index;
            post_mtip_process_next_speed_mode(info);
        }
        else
        {
            // Enable PCS link interrupts as link status wont be polled after max retries are done
            mtip_phy_lane_bring_up_progress_ind(link_index, false);
        }

        goto func_exit;
    }

    CSMLOGDBG("run_mtip_phy_retry_bringup with link: %d, port: %d\n",
               link_index, port_type);

    mtip_phy_teardown_phy(link_index);
    mdelay(1000);
    mtip_phy_bringup_phy(link_index,
                    platform_driver_priv->mtip_ports[port_type]->sfp_port_type);

func_exit:
    // free the taskstruct
    kfree(taskstruct);

    return;
}
extern uint32_t ber_sim_status[12];
void mtip_fault_notifr_status(struct work_struct *work)
{
    u32 link_index = 0;
    u32 port_type = 0;
    void __iomem *wrapper_base_addr;
    u32 port_link_id = 0;
    u32 read_val;

    if(!platform_driver_priv){
        CSMLOGERR("platform_driver_priv NULL \n");
        return;
    }
    for(port_type = MTIP_PORT_TYPE_FH_0;  port_type <= MTIP_PORT_TYPE_FH_2; port_type++){

        if(!platform_driver_priv->devices.port_devices[port_type].port_device_valid){
            continue;
        }

        wrapper_base_addr = platform_driver_priv->devices.port_devices[port_type].wrapper_base_addr;

        read_val = (u32)ioread32(wrapper_base_addr + MTIP_MAC_WRAPPER_CORE_STATUS_REG_OFFSET);

        for(link_index=0 ; link_index < MTIP_MAX_LINKS_PER_PORT; link_index++){

            mtip_lookup_link_index_by_port_type_and_real_link(&port_link_id, port_type, link_index);

            if(!platform_driver_priv->mtip_links[port_link_id]){
                continue;
            }

            if(platform_driver_priv->mtip_links[port_link_id]->state == MTIP_LINK_STATE_CLOSE){
                continue;
            }

            if ( (((read_val & GENMASK(5,2)) >> 2) & (1 << link_index)) || ber_sim_status[port_link_id]){

                mtip_snd_event_notification(port_link_id, HIGH_BER_SET);
            }else{
                mtip_snd_event_notification(port_link_id, HIGH_BER_CLR);
            }
        }
    }
    mutex_lock(&delayed_wq_mutex_lock);
    if(delayed_wq)
    { 
      mtip_workq_queue_delayed_work(delayed_wq_notifr_param , MTIP_NOTIFY_TIMER);
    }
    mutex_unlock(&delayed_wq_mutex_lock);

    return;
}

int mtip_phy_bringup_phy(u32 link_index, int sfp_port_type)
{
    enum mtip_port_type_enum port_type;
    bool lanes_enabled[PHY_LANE_MAX];
    int i;
    int rv;
    enum mtip_port_config_enum port_config;

    CSMLOGINFO("calling phy_bringup with link: %d, sfp_port_type: %d\n", link_index, sfp_port_type);

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return -1;
    }

    port_config = platform_driver_priv->mtip_ports[port_type]->port_config;

    CSMLOGINFO("phy bringup of port_type %d with config %d, %s", port_type, port_config, mtip_ethtool_get_port_config_str(port_config));

    mtip_phy_get_lanes_of_link(link_index, lanes_enabled);

    for (i = 0; i < PHY_LANE_MAX; ++i) 
    {
        CSMLOGDBG("phy_bringup_phy lane enabled[%d] is %d", i, lanes_enabled[i]);
    }

    // bringup the phy for the specified lanes
    rv = (qcom_aw_phy_driver_iface_ops.eth_phy_iface_phy_bringup)(port_type, lanes_enabled, sfp_port_type);

    CSMLOGDBG("phy bringup returned rv %d", rv);
    return rv;
}

int mtip_phy_teardown_phy(u32 link_index)
{
    enum mtip_port_type_enum port_type;
    bool lanes_enabled[PHY_LANE_MAX];
    int ret_val = 0;
    int i;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return -1;
    }

    // stop listening to link status interrupts
    mtip_phy_lane_bring_up_progress_ind(link_index, true);

    // Process link down
    if( ( link_index != MTIP_L2_ETH_LINK_INDEX && link_index != MTIP_C2C1_ETH_LINK_INDEX && (mtip_loopback_mode == MTIP_MODE_DEFAULT || mtip_loopback_mode == MTIP_MODE_PHY_LOOPBACK)) ||
        ( (link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX) && (mtip_c2c2_loopback_mode == MTIP_MODE_DEFAULT || mtip_c2c2_loopback_mode == MTIP_MODE_PHY_LOOPBACK) ) )
    {
        mtip_process_link_state(link_index, false);
    }

    mtip_phy_get_lanes_of_link(link_index, lanes_enabled);

    for (i = 0; i < PHY_LANE_MAX; ++i) 
    {
        CSMLOGDBG("phy teardown of link_index %d lanes_enabled[%d] = %d", link_index, i, lanes_enabled[i]);
    }

    // teardown the phy for the specified lanes
    ret_val = (qcom_aw_phy_driver_iface_ops.eth_phy_iface_phy_teardown)(port_type, lanes_enabled);

    CSMLOGINFO("phy teardown done for link_index %d rv %d", link_index, ret_val);

    // Clear the retry count if interface has been torn down
    if(platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_CLOSE)
    {
        mtip_phy_retry_num[link_index] = 0;
    }

    return ret_val;
}

int mtip_phy_notify_link_status(u32 link_index, bool status)
{
    enum mtip_port_type_enum port_type;
    bool lanes_enabled[PHY_LANE_MAX];
    int i;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return -1;
    }

    mtip_phy_get_lanes_of_link(link_index, lanes_enabled);

    CSMLOGDBG("phy_notify_link for port: %d link: %d", port_type, link_index);

    for (i = 0; i < PHY_LANE_MAX; ++i) 
    {
        CSMLOGDBG("phy_notify_link lane enabled[%d] is %d", i, lanes_enabled[i]);
    }

    // notify PHY of the link status
    return (qcom_aw_phy_driver_iface_ops.eth_phy_iface_notify_mac_link_status)(port_type, lanes_enabled, status);
}

int mtip_phy_initiate_an(u32 port_type, int num_lanes, u32 port_config_mask)
{
    mtip_phy_an_seq_num[port_type]++;
    if(mtip_phy_an_seq_num[port_type] == 255)
        mtip_phy_an_seq_num[port_type] = 1;

    CSMLOGINFO("Initiating AN on port %d with seq %d, num_lanes %d and mask %d",
               port_type, mtip_phy_an_seq_num[port_type], num_lanes, port_config_mask);
    return (qcom_aw_phy_driver_iface_ops.eth_phy_iface_initiate_an)(
              port_type, mtip_phy_an_seq_num[port_type], num_lanes, port_config_mask);
}

int mtip_phy_reset_phy_sm(u32 port_type)
{
    CSMLOGDBG("Resetting PHY state machine on port %d", port_type);
    return (qcom_aw_phy_driver_iface_ops.eth_phy_iface_reset_phy_sm)(port_type);
}

void mtip_phy_set_tx_compliance(bool flag)
{
    CSMLOGDBG("Compliance setting for PHY configured as: %d", flag);
    qcom_aw_phy_driver_iface_ops.eth_phy_iface_set_tx_compliance(flag);
    return;
}

void mtip_phy_set_c2c_phy_loopback_mode(enum qcom_aw_phy_loopback_mode_enum loopback_mode)
{
    CSMLOGDBG("Loopback mode of C2C phy is set as: %d", loopback_mode);
    qcom_aw_phy_driver_iface_ops.eth_phy_iface_set_c2c_phy_loopback_mode(loopback_mode);
    return;

}
static int mtip_phy_find_matching_lane(struct phylink_config *config, u32* lane_index)
{
    struct mtip_lanedev_priv* priv;
    struct net_device* dev = to_net_dev(config->dev);

    priv = netdev_priv(dev);

    *lane_index = priv->lane_index;
    return 0;
}

static void mtip_phy_get_link_state(struct phylink_config *config,
				 struct phylink_link_state *state)
{
    u32 lane_index;
    int ret;

    ret = mtip_phy_find_matching_lane(config, &lane_index);

    if (ret < 0) 
    {
        CSMLOGERR("failed to find matching lane for config: 0x%lx\n", (unsigned long)config);
        return;
    }
    else
    {
        CSMLOGDBG("get_link_state found matching lane: %d\n", lane_index);
    }

    // indicate that the mac pcs state is up
    state->link = 1;

    CSMLOGDBG("mtip_mac_link_state %d\n", state->link);

	return;
}

static void mtip_phy_config(struct phylink_config *config,
                     unsigned int mode,
                     const struct phylink_link_state *state) 
{
    u32 lane_index;
    int ret;
    unsigned int an_enabled;

    ret = mtip_phy_find_matching_lane(config, &lane_index);

    if (ret < 0) 
    {
        CSMLOGERR("failed to find matching lane for config: 0x%lx\n", (unsigned long)config);
        return;
    }
    else
    {
        CSMLOGDBG("config found matching lane: %d\n", lane_index);
    }

    an_enabled = state->an_enabled;

    CSMLOGDBG("phy config ops received with an_enabled: %d\n", an_enabled);

   return;
}

static void mtip_phy_handle_lane_up(struct mtip_process_lane_up lane_up_info)
{
    u32 port_type;
    enum mtip_lane_state_enum current_state;
    u8 lane_speed_count = 0;
    u32 link_index;

    current_state = platform_driver_priv->mtip_lanes[lane_up_info.lane_index]->lane_state;

    switch (current_state)
    {
    case MTIP_LANE_STATE_DISCONNECTED:
    case MTIP_LANE_STATE_INIT:
       {
          CSMLOGINFO("Handling transition from lane state %d to CONNECTED for lane: %d", current_state, lane_up_info.lane_index);

          if (mtip_lookup_port_type_by_lane_index(lane_up_info.lane_index, &port_type) < 0)
          {
              CSMLOGERR("invalid port_type for lane_index %d", lane_up_info.lane_index);
              return;
          }

          CSMLOGINFO("Updating state of Port: %d with lane_index: %d in state %d\n", port_type, lane_up_info.lane_index, MTIP_LANE_STATE_CONNECTED);

          /* Indicate connected even with local plug out clear to user space */
          if(mtip_lookup_link_index_by_lane_index(&link_index, lane_up_info.lane_index) == 0)
             mtip_snd_event_notification(link_index, LOCAL_PLUG_OUT_CLR);

          // set the lane state of the lane to CONNECTED
          platform_driver_priv->mtip_lanes[lane_up_info.lane_index]->lane_state = MTIP_LANE_STATE_CONNECTED;
          platform_driver_priv->mtip_ports[port_type]->needs_rx_los_processing = false;
          platform_driver_priv->mtip_lanes[lane_up_info.lane_index]->rx_los_set = false;

          // set the sfp_port_type
          platform_driver_priv->mtip_lanes[lane_up_info.lane_index]->sfp_port_type = lane_up_info.sfp_port_type;

          // set the lane speed
          platform_driver_priv->mtip_lanes[lane_up_info.lane_index]->speed_mask = lane_up_info.speed_mask;

          // set the sfp port type of the port
          platform_driver_priv->mtip_ports[port_type]->sfp_port_type = lane_up_info.sfp_port_type;

          if(lane_up_info.sfp_port_type == PORT_FIBRE)
          {
              if(lane_up_info.speed_mask & TRX_LANE_SPEED_10G)
                  lane_speed_count++;

              if(lane_up_info.speed_mask & TRX_LANE_SPEED_25G)
                  lane_speed_count++;

              if(lane_up_info.speed_mask & TRX_LANE_SPEED_50G)
                  lane_speed_count++;

              if(lane_up_info.speed_mask & TRX_LANE_SPEED_100G)
                  lane_speed_count++;

              if(lane_speed_count > 1)
                  platform_driver_priv->mtip_ports[port_type]->multi_rate_supported = true;
              else
                  platform_driver_priv->mtip_ports[port_type]->multi_rate_supported = false;
          }

          // update the port configuration
          post_mtip_process_configure_port_using_lane(port_type, lane_up_info.lane_index);
       }
       break;

    case MTIP_LANE_STATE_CONNECTED:
       {
          if (mtip_lookup_port_type_by_lane_index(lane_up_info.lane_index, &port_type) < 0)
          {
              CSMLOGERR("invalid port_type for lane_index %d", lane_up_info.lane_index);
              return;
          }

          // Trigger port reconfig with all the supported speed modes if RX LOS is cleared
          if(platform_driver_priv->mtip_ports[port_type] !=  NULL &&
             platform_driver_priv->mtip_ports[port_type]->needs_rx_los_processing == true &&
             mtip_lookup_link_index_by_lane_index(&link_index, lane_up_info.lane_index) == 0 &&
             platform_driver_priv->mtip_links[link_index] != NULL)
          {

             /* Indicate RX LOS clear to user space */
             mtip_snd_event_notification(link_index, RX_LOS_CLR);

             /* This is a case where lane up is triggered as part of phy_validate,
                which means that while in RX LOS state, module insert was detected.
                As this can be same or a different module, so port reconfiguration
                would be needed. lane_connected is false only if this function is
                called as part of phy_validate i.e. on a module insert event.
                For RX LOS set/clear scenarios, lane_connected would be passed
                as true */
             if(lane_up_info.lane_connected == false)
             {
                // set the sfp_port_type
                platform_driver_priv->mtip_lanes[lane_up_info.lane_index]->sfp_port_type = lane_up_info.sfp_port_type;

                // set the lane speed
                platform_driver_priv->mtip_lanes[lane_up_info.lane_index]->speed_mask = lane_up_info.speed_mask;

                // set the sfp port type of the port
                platform_driver_priv->mtip_ports[port_type]->sfp_port_type = lane_up_info.sfp_port_type;

                if(lane_up_info.speed_mask & TRX_LANE_SPEED_10G)
                    lane_speed_count++;
                if(lane_up_info.speed_mask & TRX_LANE_SPEED_25G)
                    lane_speed_count++;
                if(lane_up_info.speed_mask & TRX_LANE_SPEED_50G)
                    lane_speed_count++;
                if(lane_up_info.speed_mask & TRX_LANE_SPEED_100G)
                    lane_speed_count++;

                if(lane_speed_count > 1)
                    platform_driver_priv->mtip_ports[port_type]->multi_rate_supported = true;
                else
                    platform_driver_priv->mtip_ports[port_type]->multi_rate_supported = false;

                platform_driver_priv->mtip_ports[port_type]->port_priv_flags_optical = 0;
                platform_driver_priv->mtip_ports[port_type]->next_speed_retry_count = 0;
                platform_driver_priv->mtip_links[link_index]->link_down_received_post_link_up = false;
                mtip_phy_retry_num[link_index] = 0;
                post_mtip_process_reconfigure_port(port_type);
             }
             /* Port reconfiguration post RX LOS clear will be triggered in following cases
                1. If the PCS link was up and it went down due to RX LOS, or
                2. If multi rate is supported with more than one speed configured via ethtool
                   and max speed mode toggle attempts are done */
             else if((platform_driver_priv->mtip_links[link_index]->link_down_received_post_link_up) ||
                (platform_driver_priv->mtip_ports[port_type]->multi_rate_supported == true &&
                 mtip_device_count_priv_flag_bits(port_type) > 1 &&
                 platform_driver_priv->mtip_ports[port_type]->next_speed_retry_count >= MTIP_NEXT_SPEED_MODE_RETRY_MAX_COUNT))
             {
                platform_driver_priv->mtip_ports[port_type]->port_priv_flags_optical = 0;
                platform_driver_priv->mtip_ports[port_type]->next_speed_retry_count = 0;
                platform_driver_priv->mtip_links[link_index]->link_down_received_post_link_up = false;
                mtip_phy_retry_num[link_index] = 0;
                post_mtip_process_reconfigure_port(port_type);
             }
             else if (mtip_mac_wrapper_get_link_status(link_index) == true) 
             {
                mtip_process_link_state(link_index, true);
             }

             platform_driver_priv->mtip_ports[port_type]->needs_rx_los_processing = false;
          }
       }

       /* Clear RX LOS flag for the lane if set as part of lane up processing */
       if(platform_driver_priv->mtip_lanes[lane_up_info.lane_index]->rx_los_set)
       {
          platform_driver_priv->mtip_lanes[lane_up_info.lane_index]->rx_los_set = false;
          if(mtip_lookup_link_index_by_lane_index(&link_index, lane_up_info.lane_index) == 0)
             mtip_snd_event_notification(link_index, RX_LOS_CLR);
       }

       break;

    default:
       {
          CSMLOGINFO("Handling transition from unknown to CONNECTED for lane: %d", lane_up_info.lane_index);
       }
       break;
    }
    return;
}

static void mtip_phy_handle_lane_down(struct mtip_process_lane_down lane_down_info)
{
   int i;
   u32 link_index;
   enum mtip_link_state_enum link_state;
   u32 port_type;
   enum mtip_lane_state_enum current_state = MTIP_LANE_STATE_INIT;
   u32 tmp_lane_index;
   bool any_lane_connected = false;

   // determine the port using lane
   mtip_lookup_port_type_by_lane_index(lane_down_info.lane_index, &port_type);

   if(platform_driver_priv == NULL ||
      platform_driver_priv->mtip_ports[port_type] == NULL ||
      platform_driver_priv->mtip_lanes[lane_down_info.lane_index] == NULL)
   {
      return;
   }

   // Lane down received due to RX LOS
   if(platform_driver_priv->mtip_lanes[lane_down_info.lane_index]->sfp_port_type == PORT_FIBRE &&
      lane_down_info.reason_code == TRX_RX_LOS)
   {
      /* Set needs_rx_los_processing flag which will be used to trigger port
         reconfiguration once RX LOS gets cleared */
      platform_driver_priv->mtip_ports[port_type]->needs_rx_los_processing = true;
      platform_driver_priv->mtip_lanes[lane_down_info.lane_index]->rx_los_set = true;

      /* Indicate RX LOS to user space */
      if(mtip_lookup_link_index_by_lane_index(&link_index, lane_down_info.lane_index) == 0)
      {
         mtip_snd_event_notification(link_index, RX_LOS_SET);
         /* If MAC wrapper confirms link is down, update link state accordingly */
         if (mtip_mac_wrapper_get_link_status(link_index) == false)
         {
            mtip_process_link_state(link_index, false);
         }
      }

      return;
   }

   /* Indicate local plug put to user space */
   if(lane_down_info.reason_code == TRX_LOCAL_PLUGOUT &&
      mtip_lookup_link_index_by_lane_index(&link_index, lane_down_info.lane_index) == 0)
   {
      /* If RX LOS was set, clear it as part of local plug out */
      if(platform_driver_priv->mtip_lanes[lane_down_info.lane_index]->rx_los_set)
      {
         platform_driver_priv->mtip_lanes[lane_down_info.lane_index]->rx_los_set = false;
         mtip_snd_event_notification(link_index, RX_LOS_CLR);
      }

      mtip_snd_event_notification(link_index, LOCAL_PLUG_OUT_SET);
   }

   current_state = platform_driver_priv->mtip_lanes[lane_down_info.lane_index]->lane_state;

   switch (current_state)
   {
   case MTIP_LANE_STATE_DISCONNECTED:
   case MTIP_LANE_STATE_INIT:
      {
         CSMLOGINFO("Handling transition from lane state %d to DISCONNECTED for lane: %d", current_state, lane_down_info.lane_index);
         platform_driver_priv->mtip_lanes[lane_down_info.lane_index]->lane_state = MTIP_LANE_STATE_DISCONNECTED;
         platform_driver_priv->mtip_ports[port_type]->needs_rx_los_processing = false;
      }
      break;

   case MTIP_LANE_STATE_CONNECTED:
      {
         CSMLOGINFO("Handling transition from CONNECTED to DISCONNECTED for lane: %d", lane_down_info.lane_index);

         // set the lane state as DISCONNECTED
         platform_driver_priv->mtip_lanes[lane_down_info.lane_index]->lane_state = MTIP_LANE_STATE_DISCONNECTED;

         // Bring down only the link which is mapped to this lane
         if(mtip_lookup_link_index_by_lane_index(&link_index, lane_down_info.lane_index) == 0)
         {
            if (platform_driver_priv->mtip_links[link_index] != NULL)
            {
               link_state = mtip_get_link_state_by_link_index(link_index);
               if ((link_state == MTIP_LINK_STATE_OPEN_DONE) ||
                   (link_state == MTIP_LINK_STATE_UP) ||
                   (link_state == MTIP_LINK_STATE_DOWN))
               {
                  CSMLOGDBG("Port: %d with link_index: %d in %d state\n", port_type, link_index, link_state);
                  // teardown the phy
                  mtip_phy_teardown_phy(link_index);
               }
            }
         }

         // Keep the port as connected if any other lane is still connected
         for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_lane_phandles; ++i)
         {
            tmp_lane_index = platform_driver_priv->devices.port_devices[port_type].lane_devices[i]->lane_index;
            if (platform_driver_priv->mtip_lanes[tmp_lane_index]->lane_state == MTIP_LANE_STATE_CONNECTED)
            {
               /* If local plug out is triggered while RX LOS is set for any of the lanes,
                  clear it and move the lane to disconnected state */
               if(lane_down_info.reason_code == TRX_LOCAL_PLUGOUT &&
                  platform_driver_priv->mtip_lanes[tmp_lane_index]->rx_los_set)
               {
                  platform_driver_priv->mtip_lanes[tmp_lane_index]->rx_los_set = false;
                  if(mtip_lookup_link_index_by_lane_index(&link_index, tmp_lane_index) == 0)
                     mtip_snd_event_notification(link_index, RX_LOS_CLR);

                 // set the lane state as DISCONNECTED
                 platform_driver_priv->mtip_lanes[tmp_lane_index]->lane_state = MTIP_LANE_STATE_DISCONNECTED;
               }
               else
               {
                  any_lane_connected = true;
                  break;
               }
            }
         }

         if(any_lane_connected == false)
         {
            // Reset PHY state machine if links were torn down
            mtip_phy_reset_phy_sm(port_type);

            // Clear the RX LOS processing flag as all lanes are down
            platform_driver_priv->mtip_ports[port_type]->needs_rx_los_processing = false;

            // set the port state back to INIT
            platform_driver_priv->mtip_ports[port_type]->port_state = MTIP_PORT_STATE_INIT;
         }
      }
      break;

   default:
      {
         CSMLOGINFO("Handling transition from unknown to CONNECTED for lane: %d", lane_down_info.lane_index);
         platform_driver_priv->mtip_lanes[lane_down_info.lane_index]->lane_state = MTIP_LANE_STATE_DISCONNECTED; 
         platform_driver_priv->mtip_ports[port_type]->needs_rx_los_processing = false;
      }
      break;
   }

   return;
}

void post_mtip_phy_handle_lane_up(struct mtip_process_lane_up lane_up_info)
{
    u32 port_type;
    struct mtip_process_lane_up* taskstruct = NULL;

    if(mtip_lookup_port_type_by_lane_index(lane_up_info.lane_index, &port_type))
    {
      CSMLOGERR("invalid lane_index");
      return;
    }

    taskstruct = kmalloc(sizeof(struct mtip_process_lane_up), GFP_ATOMIC);
    if(taskstruct == NULL)
    {
      CSMLOGERR("memory alloc failed\n");
      return;
    }

    memcpy(taskstruct, &lane_up_info, sizeof(struct mtip_process_lane_up));

    mtip_queue_work(MTIP_WORKQ_TASK_PROCESS_LANE_UP, taskstruct, port_type);
}

void run_mtip_process_lane_up(void* workptr)
{
    struct mtip_process_lane_up *taskstruct = (struct mtip_process_lane_up *)workptr;

    mtip_phy_handle_lane_up(*taskstruct);

    // free the taskstruct
    kfree(taskstruct);
}

void post_mtip_phy_handle_lane_down(struct mtip_process_lane_down lane_down_info)
{
    u32 port_type;
    struct mtip_process_lane_down* taskstruct = NULL;

    if(mtip_lookup_port_type_by_lane_index(lane_down_info.lane_index, &port_type))
    {
      CSMLOGERR("invalid lane_index");
      return;
    }

    taskstruct = kmalloc(sizeof(struct mtip_process_lane_down), GFP_ATOMIC);
    if(taskstruct == NULL)
    {
      CSMLOGERR("memory alloc failed\n");
      return;
    }

    memcpy(taskstruct, &lane_down_info, sizeof(struct mtip_process_lane_down));
    mtip_queue_work(MTIP_WORKQ_TASK_PROCESS_LANE_DOWN, taskstruct, port_type);
}

void run_mtip_process_lane_down(void* workptr)
{
    struct mtip_process_lane_down *taskstruct = (struct mtip_process_lane_down *)workptr;

    mtip_phy_handle_lane_down(*taskstruct);

    // free the taskstruct
    kfree(taskstruct);
}

static void mtip_phy_phy_validate(struct phylink_config *config,
                           unsigned long *supported,
                           struct phylink_link_state *state) 
{
    u32 lane_index;
    int ret;
    u8  sfp_port_type = PORT_DA;
    int sfp_phandle;
    struct qsfp_info trx_info = {0};
    struct mtip_process_lane_up lane_up_info = {0};

    ret = mtip_phy_find_matching_lane(config, &lane_index);
    if (ret < 0) 
    {
        CSMLOGERR("failed to find matching lane for config: 0x%lx\n", (unsigned long)config);
        return;
    }

    CSMLOGDBG("phy validate ops received for lane: %d config: 0x%lx", lane_index, (unsigned long)config);

    // ignore this if there is no associated lane device
    if (platform_driver_priv->mtip_lanes[lane_index] == NULL)
    {
       CSMLOGINFO("Got phy validate on unused lane_index: %d ignoring", lane_index);
       return;
    }

    // /lookup the sfp_port_type of the port
    sfp_phandle = platform_driver_priv->devices.lane_devices[lane_index].sfp_phandle;
    if (sfp_phandle < 0)
    {
       CSMLOGERR("Got an unexpected link event! lane: %d\n", lane_index);
       return;
    }

    // ask the qsfp driver about the sfp port type
    if (qsfp_trx_get_lane_type(sfp_phandle, &sfp_port_type) < 0)
    {
       CSMLOGDBG("TRX not initialized yet, ignoring event for lane: %d\n", lane_index);
       return;
    }

    CSMLOGINFO("phy validate read sfp_port_type %d for sfp_phandle %d", sfp_port_type, sfp_phandle);

    // if sfp port type is OTHER, force it to be PORT_DA
    if (sfp_port_type == PORT_OTHER)
    {
        sfp_port_type = PORT_DA;
    }

    // set the sfp port_type of the lane
    platform_driver_priv->mtip_lanes[lane_index]->sfp_port_type = sfp_port_type;

    ret = qsfp_trx_get_info(sfp_phandle, &trx_info);
    if (ret == 0) 
    {
        // copy the trx info to the lane
        memcpy(&platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info, &trx_info, sizeof(struct qsfp_info));

        CSMLOGINFO("mod_type %d , trx_speed_mask 0x%x, lane_info 0x%x, breakout_cfg 0x%x, link length range %d, associated with lane_index %d",
                   trx_info.trx_module_type, trx_info.speed_mask, trx_info.trx_laneinfo, trx_info.trx_bout_cfg , trx_info.trx_link_length_range, lane_index);

        if((trx_info.speed_mask & (TRX_LANE_SPEED_10G | TRX_LANE_SPEED_25G | TRX_LANE_SPEED_50G | TRX_LANE_SPEED_100G)) == 0)
            return;

        // For DAC, backward compatibility is supported for lower speed modes
        if(sfp_port_type == PORT_DA)
        {
            if(trx_info.speed_mask & TRX_LANE_SPEED_100G)
                trx_info.speed_mask |= TRX_LANE_SPEED_10G | TRX_LANE_SPEED_25G | TRX_LANE_SPEED_50G;
            else if(trx_info.speed_mask & TRX_LANE_SPEED_50G)
                trx_info.speed_mask |= TRX_LANE_SPEED_10G | TRX_LANE_SPEED_25G;
            else if(trx_info.speed_mask & TRX_LANE_SPEED_25G)
                trx_info.speed_mask |= TRX_LANE_SPEED_10G;
        }

        lane_up_info.lane_index = lane_index;
        lane_up_info.sfp_port_type = sfp_port_type;
        lane_up_info.speed_mask = trx_info.speed_mask;
        lane_up_info.lane_connected = false;
        post_mtip_phy_handle_lane_up(lane_up_info);
    }

    return;
}

static void mtip_phy_phylink_lane_up(struct phylink_config *config,
                                     struct phy_device *phy, unsigned int mode,
                                     phy_interface_t interface, int speed,
                                     int duplex, bool tx_pause, bool rx_pause)
{
    u32 lane_index;
    int ret;
    u8  sfp_port_type = PORT_DA;
    int sfp_phandle;
    struct qsfp_info trx_info = {0};
    struct mtip_process_lane_up lane_up_info = {0};

    ret = mtip_phy_find_matching_lane(config, &lane_index);

    if (ret < 0)
    {
       CSMLOGERR("failed to find matching lane for config: 0x%lx\n", (unsigned long)config);
       return;
    }
    else
    {
       CSMLOGDBG("link_up found matching lane_index: %d\n", lane_index);
    }

    CSMLOGINFO("phy link up ops received for lane_index %d\n", lane_index);

    // print the data passed
    CSMLOGINFO("mode: %d, interface: %d, speed: %d, duplex: %d, tx_pause: %d, rx_pause: %d\n", mode, (unsigned int)interface, speed, duplex, tx_pause, rx_pause);

    // ignore this if there is no associated lane device
    if (platform_driver_priv->mtip_lanes[lane_index] == NULL)
    {
       CSMLOGINFO("Got lane up on unused lane_index: %d ignoring", lane_index);
       return;
    }

    // /lookup the sfp_port_type of the port
    sfp_phandle = platform_driver_priv->devices.lane_devices[lane_index].sfp_phandle;
    if (sfp_phandle < 0)
    {
       CSMLOGERR("Got an unexpected link event! lane: %d\n", lane_index);
       return;
    }

    // ask the qsfp driver about the sfp port type
    qsfp_trx_get_lane_type(sfp_phandle, &sfp_port_type);

    CSMLOGINFO("read sfp_port_type %d for sfp_phandle %d", sfp_port_type, sfp_phandle);

    // if sfp port type is OTHER, force it to be PORT_DA
    if (sfp_port_type == PORT_OTHER)
    {
       sfp_port_type = PORT_DA;
    }

    // set the sfp port_type
    platform_driver_priv->mtip_lanes[lane_index]->sfp_port_type = sfp_port_type;

    ret = qsfp_trx_get_info(sfp_phandle, &trx_info);
    if (ret == 0) 
    {
        // copy the trx info to the lane
        memcpy(&platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info, &trx_info, sizeof(struct qsfp_info));

        CSMLOGINFO("mod_type %d , trx_speed_mask 0x%x, lane_info 0x%x, breakout_cfg 0x%x, link length range %d, associated with lane_index %d",
                   trx_info.trx_module_type, trx_info.speed_mask, trx_info.trx_laneinfo, trx_info.trx_bout_cfg , trx_info.trx_link_length_range, lane_index);

        if((trx_info.speed_mask & (TRX_LANE_SPEED_10G | TRX_LANE_SPEED_25G | TRX_LANE_SPEED_50G | TRX_LANE_SPEED_100G)) == 0)
           return;

        // For DAC, backward compatibility is supported for lower speed modes
        if(sfp_port_type == PORT_DA)
        {
            if(trx_info.speed_mask & TRX_LANE_SPEED_100G)
                trx_info.speed_mask |= TRX_LANE_SPEED_10G | TRX_LANE_SPEED_25G | TRX_LANE_SPEED_50G;
            else if(trx_info.speed_mask & TRX_LANE_SPEED_50G)
                trx_info.speed_mask |= TRX_LANE_SPEED_10G | TRX_LANE_SPEED_25G;
            else if(trx_info.speed_mask & TRX_LANE_SPEED_25G)
                trx_info.speed_mask |= TRX_LANE_SPEED_10G;
        }

        lane_up_info.lane_index = lane_index;
        lane_up_info.sfp_port_type = sfp_port_type;
        lane_up_info.speed_mask = trx_info.speed_mask;
        lane_up_info.lane_connected = true;
        post_mtip_phy_handle_lane_up(lane_up_info);
    }   

   return;
}

static void mtip_phy_phylink_lane_down(struct phylink_config *config, unsigned int mode,
                                phy_interface_t interface) 
{
    u32 lane_index;
    int ret;
    int sfp_phandle;
    trx_lane_down_reason_code_type reason_code;
    struct mtip_process_lane_down lane_down_info = {0};

    ret = mtip_phy_find_matching_lane(config, &lane_index);

    if (ret < 0) 
    {
        CSMLOGERR("failed to find matching lane for config: 0x%lx\n", (unsigned long)config);
        return;
    }
    else
    {
        CSMLOGDBG("link_down found matching lane: %d\n", lane_index);
    }

    CSMLOGDBG("phy link down ops received\n");

    // /lookup the sfp_port_type of the port
    sfp_phandle = platform_driver_priv->devices.lane_devices[lane_index].sfp_phandle;

    if (sfp_phandle < 0) 
    {
        CSMLOGERR("Got an unexpected link event! lane: %d\n", lane_index);
        return;
    }

    // get the lane down reason code from the transceiver
    if (qsfp_trx_get_lane_down_reason_code(sfp_phandle, &reason_code) < 0) 
    {
        CSMLOGERR("got an invalid lane_down indication %d", lane_index);
        return;
    }

    CSMLOGINFO("Phylink lane down on lane_index %d reason_code: %d", lane_index, reason_code);

    platform_driver_priv->devices.lane_devices[lane_index].reason_code = reason_code;

    switch (reason_code) 
    {
        case TRX_LOCAL_PLUGOUT:
        case TRX_RX_LOS:
        {
            lane_down_info.lane_index = lane_index;
            lane_down_info.reason_code = reason_code;
            post_mtip_phy_handle_lane_down(lane_down_info);
        }
        break;

        case TRX_TX_FAULT:
        {
            CSMLOGDBG("Ignore lane down for lane_index %d", lane_index);
        }
        break;

        default:
        {
            CSMLOGERR("unknown reason code received for lane_index %d", lane_index);
        }
        break;
    }
   return;
}

/* Dummy function added to avoid NULL pointer dereferencing in phylink for 
   unsupported modules like 1G transceiver module on SFP port */
static void mtip_phy_phylink_mac_an_restart(struct phylink_config *config) 
{
  return;
}

static const struct phylink_mac_ops mtip_phylink_mac_ops = {
	.validate = mtip_phy_phy_validate,
	.mac_pcs_get_state = mtip_phy_get_link_state,
	.mac_config = mtip_phy_config,
	.mac_link_up = mtip_phy_phylink_lane_up,
	.mac_link_down = mtip_phy_phylink_lane_down,
	.mac_an_restart = mtip_phy_phylink_mac_an_restart,
};

static void mtip_phy_lane_netdevice_init(struct net_device *dev) 
{
   CSMLOGDBG("dummy netdev init for 0x%lx\n", (unsigned long)dev);
}

int mtip_phy_create_phylink(struct mtip_lane_device_info* lane_device)
{
    phy_interface_t mode;
    u32 lane_index = lane_device->lane_index;
    struct platform_device* lane_pdev = lane_device->lane_pdev;
    struct mtip_lane_info* lane_info;
    struct mtip_lanedev_priv* priv;

    // allocate the mtip_lane_info for the lane_index
    lane_info = platform_driver_priv->mtip_lanes[lane_index];

    // check if there is a valid sfp_handle associated with the port
    if (platform_driver_priv->devices.lane_devices[lane_index].sfp_phandle < 0) 
    {
        CSMLOGINFO("lane sfp_handle is not present\n");

        // set lane state as CONNECTED and return
        lane_info->lane_state = MTIP_LANE_STATE_CONNECTED;
        return 0;
    }

    if (mtip_loopback_mode != MTIP_MODE_DEFAULT)
    {
        // we are not using phylink for PCS loopback or
        // near end loopback
        // set port state as CONNECTED and return
        lane_info->lane_state = MTIP_LANE_STATE_CONNECTED;
        return 0;
    }

    // for E2E set the port state as INIT
    // this will be set to CONNECTED on receiving a link up
    lane_info->lane_state = MTIP_LANE_STATE_INIT;

    lane_info->lane_dummy_ndev = alloc_netdev(sizeof(struct mtip_lanedev_priv), "lane%d", NET_NAME_ENUM, mtip_phy_lane_netdevice_init);

    SET_NETDEV_DEV(lane_info->lane_dummy_ndev, &lane_pdev->dev);

    priv = netdev_priv(lane_info->lane_dummy_ndev);
    priv->lane_index = lane_index;

    /* PHYLINK setup */
	lane_info->phylink_config.dev = &lane_info->lane_dummy_ndev->dev;
	lane_info->phylink_config.type = PHYLINK_NETDEV;
	of_get_phy_mode(lane_pdev->dev.of_node, &mode);

	lane_info->phylink = phylink_create(&lane_info->phylink_config, of_fwnode_handle(lane_pdev->dev.of_node),
	                         mode, &mtip_phylink_mac_ops);

    CSMLOGDBG("got phylink 0x%lx\n", lane_info->phylink);

	if (IS_ERR(lane_info->phylink))
	{
		CSMLOGERR("PHYLINK creation failed with err = %d", PTR_ERR(lane_info->phylink));
		return IS_ERR(lane_info->phylink);
	}

    // start has to be done in rtnl context
    rtnl_lock();

    // start the phylink
    phylink_start(lane_info->phylink);

    // start has to be done in rtnl context
    rtnl_unlock();

    CSMLOGDBG("phylink start done\n");

    return 0;
}

int mtip_phy_destroy_phylink(struct phylink *phylink,struct net_device* lane_dummy_ndev)
{
    if(phylink) 
    {

        // stop the phylink
        rtnl_lock();
        phylink_stop(phylink);
        rtnl_unlock();

        // destory the phylink
        phylink_destroy(phylink);
    }

       // free the netdev
    if(lane_dummy_ndev)
        free_netdev(lane_dummy_ndev);

    return 0;
}

trx_link_length_range mtip_phy_get_trx_link_length_range(struct mtip_port_device_info* port_device)
{
    int i;
    u32 port_type = port_device->port_type;
    u32 lane_index;

    // find a lane that is connected
    for (i = 0; i < port_device->num_lane_phandles; ++i) 
    {
        lane_index = port_device->lane_devices[i]->lane_index;

        if (platform_driver_priv->mtip_lanes[lane_index]->lane_state == MTIP_LANE_STATE_CONNECTED) 
        {
            CSMLOGINFO("mtip_phy_get_trx_link_length_range %d for port %d",
                      platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info.trx_link_length_range, port_type);
            return platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info.trx_link_length_range;
        }
    }

    return TRX_LINK_UNKNOWN;
}

void mtip_phy_notify_eth_event_to_trx(u32 link_index, trx_phy_event event)
{
    enum mtip_port_type_enum port_type;
    bool lanes_enabled[PHY_LANE_MAX];
    int i;
    u32 lane_index;
    u32 sfp_phandle[MTIP_MAX_LANES_PER_PORT] = {0};
    u8 sfp_lane_count = 0;
    struct trx_eth_event_t trx_event_info = {0};

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return;
    }

    mtip_phy_get_lanes_of_link(link_index, lanes_enabled);

    for (i = 0; i < PHY_LANE_MAX; ++i)
    {
        if(lanes_enabled[i] == true){
            mtip_lookup_lane_index_by_port_type_and_real_lane(&lane_index, port_type, i);
            sfp_phandle[sfp_lane_count++] = platform_driver_priv->devices.lane_devices[lane_index].sfp_phandle;
            CSMLOGINFO("eth_event %d for link_index %d = lane %d = sfp_phandle=%d",
                       event, link_index, lane_index, sfp_phandle[sfp_lane_count-1]);
        }
    }

    // Indicate transceiver driver about interface bring up

    trx_event_info.event = event;
    trx_event_info.lane_phandle = sfp_phandle;
    trx_event_info.num_lanes = sfp_lane_count;

    // During link bring up, inform TRX about the lane speed used for port configuration
    if(event == TRX_IFCONFIG_UP)
    {
        trx_event_info.eth_cfg_speed =
               mtip_phy_port_config_to_trx_lane_speed(platform_driver_priv->mtip_ports[port_type]->port_config);
    }

    qsfp_trx_eth_event_notifier(&trx_event_info);

    return;
}

bool mtip_phy_is_breakout_config(u32 port_type)
{
    struct qsfp_info lane_qsfp_info;

    if (mtip_device_lookup_lane_qsfp_cfg(port_type, &lane_qsfp_info) < 0)
    {
        CSMLOGERR("unable to lookup lane cfg of port_type %d", port_type);
        return false;
    }

    if(lane_qsfp_info.trx_module_type == TRX_QSFP_PLS_QSFP28_QSFP56)
    {
        if(lane_qsfp_info.trx_bout_cfg != 0 && lane_qsfp_info.trx_bout_cfg != 0xFF)
        {
            return true;
        }
        else if(lane_qsfp_info.trx_laneinfo != 0xF)
        {
            return true;
        }
    }
    else if(lane_qsfp_info.trx_module_type == TRX_QSFPDD)
    {
        if(lane_qsfp_info.trx_bout_cfg != 0)
        {
            return true;
        }
        else if(lane_qsfp_info.trx_laneinfo != 0xFF)
        {
            return true;
        }
    }

    return false;
}

trx_lane_speed mtip_phy_port_config_to_trx_lane_speed(enum mtip_port_config_enum port_config)
{
    switch(port_config)
    {
        case MTIP_PORT_CONFIG_1x100GBASE_R:
        case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC_LL:
        case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC:
            return TRX_LANE_SPEED_100G;

        case MTIP_PORT_CONFIG_1x100GBASE_R2:
        case MTIP_PORT_CONFIG_1x100GBASE_R2_RSFEC:
        case MTIP_PORT_CONFIG_2x50GBASE_R:
        case MTIP_PORT_CONFIG_2x50GBASE_R_RSFEC:
        case MTIP_PORT_CONFIG_1x50GBASE_R:
        case MTIP_PORT_CONFIG_1x50GBASE_R_RSFEC:
            return TRX_LANE_SPEED_50G;

        case MTIP_PORT_CONFIG_1x100GBASE_R4:
        case MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC:
        case MTIP_PORT_CONFIG_2x50GBASE_R2:
        case MTIP_PORT_CONFIG_2x50GBASE_R2_FEC:
        case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI:
        case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI_FEC:
        case MTIP_PORT_CONFIG_1x50GBASE_R2:
        case MTIP_PORT_CONFIG_1x50GBASE_R2_RSFEC:
        case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI:
        case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI_FEC:
        case MTIP_PORT_CONFIG_4x25GBASE_R:
        case MTIP_PORT_CONFIG_4x25GBASE_R_FEC:
        case MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC:
        case MTIP_PORT_CONFIG_1x25GBASE_R:
        case MTIP_PORT_CONFIG_1x25GBASE_R_FEC:
        case MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC:
            return TRX_LANE_SPEED_25G;

        case MTIP_PORT_CONFIG_1x40GBASE_R4:
        case MTIP_PORT_CONFIG_1x40GBASE_R4_FEC:
        case MTIP_PORT_CONFIG_4x10GBASE_R:
        case MTIP_PORT_CONFIG_4x10GBASE_R_FEC:
        case MTIP_PORT_CONFIG_1x10GBASE_R:
        case MTIP_PORT_CONFIG_1x10GBASE_R_FEC:
            return TRX_LANE_SPEED_10G;

        case MTIP_PORT_CONFIG_MAX:
        default:
            return TRX_LANE_SPEED_UNKNOWN;
    }

    return TRX_LANE_SPEED_UNKNOWN;
}

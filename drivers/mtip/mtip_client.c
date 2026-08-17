//SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "mtip.h"
#include "mtip_client.h"
#include "mtip_workq.h"
#include "mtip_mac.h"
#include "mtip_pcs.h"
#include "mtip_phy.h"
#include "mtip_platform.h"
#include "mtip_device.h"

static eth_ecpriss_link_rate_e mtip_client_get_link_rate(u32 port_type)
{
    enum mtip_port_config_enum port_config = platform_driver_priv->mtip_ports[port_type]->port_config;
    eth_ecpriss_link_rate_e link_rate = ETH_ECPRISS_LINK_RATE_25;

    switch (port_config) 
    {
    case MTIP_PORT_CONFIG_1x100GBASE_R:
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC_LL:
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_1x100GBASE_R2:
    case MTIP_PORT_CONFIG_1x100GBASE_R2_RSFEC:
    case MTIP_PORT_CONFIG_1x100GBASE_R4:
    case MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC:
        {
            link_rate = ETH_ECPRISS_LINK_RATE_100;
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R:
    case MTIP_PORT_CONFIG_1x50GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_2x50GBASE_R:
    case MTIP_PORT_CONFIG_2x50GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_1x50GBASE_R2:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_RSFEC:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI_FEC:
    case MTIP_PORT_CONFIG_2x50GBASE_R2:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_FEC:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI_FEC:
        {
            link_rate = ETH_ECPRISS_LINK_RATE_50;
        }
        break;
    case MTIP_PORT_CONFIG_1x40GBASE_R4:
    case MTIP_PORT_CONFIG_1x40GBASE_R4_FEC:
        {
            link_rate = ETH_ECPRISS_LINK_RATE_40;
        }
        break;
    case MTIP_PORT_CONFIG_1x25GBASE_R:
    case MTIP_PORT_CONFIG_1x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_4x25GBASE_R:
    case MTIP_PORT_CONFIG_4x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC:
        {
            link_rate = ETH_ECPRISS_LINK_RATE_25;
        }
        break;
    case MTIP_PORT_CONFIG_1x10GBASE_R:
    case MTIP_PORT_CONFIG_1x10GBASE_R_FEC:
    case MTIP_PORT_CONFIG_4x10GBASE_R:
    case MTIP_PORT_CONFIG_4x10GBASE_R_FEC:
        {
            link_rate = ETH_ECPRISS_LINK_RATE_10;
        }
        break;
    default:
        {
            CSMLOGERR("Unknown port config %d", port_config);
        }
        break;
    }
    
    return link_rate;
}

static eth_ecpriss_link_state_e mtip_client_get_link_state_by_link_index(u32 link_index)
{
    enum mtip_link_state_enum link_state;
    eth_ecpriss_link_state_e retval;

    link_state = mtip_get_link_state_by_link_index(link_index);

    switch (link_state) 
    {
    case MTIP_LINK_STATE_INIT:
        {
            retval = ETH_ECPRISS_LINK_STATE_INIT;
        }
        break;
    case MTIP_LINK_STATE_CLOSE:
        {
            retval = ETH_ECPRISS_LINK_STATE_CLOSE;
        }
        break;
    case MTIP_LINK_STATE_UP:
        {
            retval = ETH_ECPRISS_LINK_STATE_UP;
        }
        break;
    case MTIP_LINK_STATE_DOWN:
        {
            retval = ETH_ECPRISS_LINK_STATE_DOWN;
        }
        break;
    case MTIP_LINK_STATE_OPEN_WAITING_FOR_LANES:
    case MTIP_LINK_STATE_OPEN_DONE:
        {
            retval = ETH_ECPRISS_LINK_STATE_OPEN;
        }
        break;
    case MTIP_LINK_STATE_OPEN_FAILED:
        {
            retval = ETH_ECPRISS_LINK_STATE_DOWN;
        }
        break;
    default:
        {
            retval = ETH_ECPRISS_LINK_STATE_DOWN;
        }
        break;
    }

    return retval;
}

void mtip_update_topology(void)
{
    int i, j;
    u32 port_number = 0;
    u32 link_number = 0;
    eth_ecpriss_topology_root_s* topology;
    unsigned long flags;
    spinlock_t *lock = &platform_driver_priv->driver_lock;
    u32 port = 0;
    u32 link_index;

    CSMLOGDBG("Updating topology\n");

    spin_lock_irqsave(lock, flags);

    // get the topology pointer
    topology = platform_driver_priv->topology;

    // add check here for null pointer
    if (topology == NULL) {
        CSMLOGERR("Unable to update topology\n");
        spin_unlock_irqrestore(lock, flags);
        return;
    }

    // clear the contents of topology
    memset(topology, 0, sizeof(eth_ecpriss_topology_root_s));

    // set the init as complete
    topology->eth_topology_init_done = true;

    // set the number of unique ports to 3
    topology->num_unique_port_types = ECPRISS_MAX_UNIQUE_PORT;

    // set the three port types
    topology->topology_params[ETH_ECPRISS_PORT_TYPE_FH].port_type = ETH_ECPRISS_PORT_TYPE_FH;
    topology->topology_params[ETH_ECPRISS_PORT_TYPE_C2C].port_type = ETH_ECPRISS_PORT_TYPE_C2C;
    topology->topology_params[ETH_ECPRISS_PORT_TYPE_L2].port_type = ETH_ECPRISS_PORT_TYPE_L2;

    // go through the ports
    for (i = 0; i < MTIP_MAX_PORTS; ++i)
    {
        if (platform_driver_priv->devices.port_devices[i].port_device_valid)
        {
            switch (platform_driver_priv->devices.port_devices[i].port_type)
            {
            case MTIP_PORT_TYPE_FH_0:
            case MTIP_PORT_TYPE_FH_1:
            case MTIP_PORT_TYPE_FH_2:
                {
                    port_number = ETH_ECPRISS_PORT_TYPE_FH;
                    port = topology->topology_params[port_number].num_ports;

                    topology->topology_params[port_number].port_params[port].port_index = platform_driver_priv->devices.port_devices[i].port_type;

                    link_number = 0;

                    for (j = 0; (j < platform_driver_priv->devices.port_devices[i].num_link_phandles) && (j < MTIP_MAX_LINKS_PER_PORT) ; ++j)
                    {
                        if (platform_driver_priv->devices.port_devices[i].link_devices[j]->mac_ioaddr != NULL)
                        {
                            link_index = platform_driver_priv->devices.port_devices[i].link_devices[j]->link_index;

                            // set the link index
                            topology->topology_params[port_number].port_params[port].link_params[link_number].link_index 
                                = link_index;

                            // get the MTU that has been set in the HW register for this port, link
                            topology->topology_params[port_number].port_params[port].link_params[link_number].link_mtu = mtip_mac_get_frame_length(i, j);

                            // get the eth_addr
                            mtip_mac_get_mac_address_by_link_index(link_index, topology->topology_params[port_number].port_params[port].link_params[link_number].eth_mac_addr);

                            // set the link state
                            topology->topology_params[port_number].port_params[port].link_params[link_number].link_state = mtip_client_get_link_state_by_link_index(link_index);

                            // set the link rate
                            topology->topology_params[port_number].port_params[port].link_params[link_number].link_rate = mtip_client_get_link_rate(platform_driver_priv->devices.port_devices[i].port_type);

                            // set the loopback enabled status
                topology->topology_params[port_number].port_params[port].link_params[link_number].loopback_enabled =
                    mtip_is_link_in_loopback(link_index);

                            // increment the link number
                            ++link_number;
                        }

                        // set the num links
                        topology->topology_params[port_number].port_params[port].num_links = platform_driver_priv->devices.port_devices[i].num_link_phandles;
                    }

                    // increment the FH port type
                    ++topology->topology_params[port_number].num_ports;
                }
                break;
            case MTIP_PORT_TYPE_L2:
                {
                    port_number = ETH_ECPRISS_PORT_TYPE_C2C;
                    port = 0;
                    topology->topology_params[port_number].port_params[port].port_index = 0;

                    link_number = 0;

                    for (j = 0; (j < platform_driver_priv->devices.port_devices[i].num_link_phandles) && (j < MTIP_MAX_LINKS_PER_PORT); ++j)
                    {
                        if (platform_driver_priv->devices.port_devices[i].link_devices[j]->mac_ioaddr != NULL)
                        {
                            link_index = platform_driver_priv->devices.port_devices[i].link_devices[j]->link_index;

                            // set the link index
                            topology->topology_params[port_number].port_params[port].link_params[link_number].link_index 
                                = link_index;

                            // get the MTU that has been set in the HW register for this port, link
                            topology->topology_params[port_number].port_params[port].link_params[link_number].link_mtu = mtip_mac_get_frame_length(i, j);

                            // get the eth_addr
                            mtip_mac_get_mac_address_by_link_index(link_index, topology->topology_params[port_number].port_params[port].link_params[link_number].eth_mac_addr);

                            // set the link state
                            topology->topology_params[port_number].port_params[port].link_params[link_number].link_state = mtip_client_get_link_state_by_link_index(link_index);

                            // set the link rate
                            topology->topology_params[port_number].port_params[port].link_params[link_number].link_rate = ETH_ECPRISS_LINK_RATE_25;

                            // increment the link number
                            ++link_number;
                        }

                        // set the num links
                        topology->topology_params[port_number].port_params[port].num_links = platform_driver_priv->devices.port_devices[i].num_link_phandles;
                    }

                    // increment the FH port type
                    ++topology->topology_params[port_number].num_ports;
                }
                break;
            case MTIP_PORT_TYPE_DEBUG:
            default:
                {
                    CSMLOGDBG("Ignoring port of type: %d", platform_driver_priv->devices.port_devices[i].port_type);
                }
                break;
            }
        }
    }

    spin_unlock_irqrestore(lock, flags);
}

void post_mtip_client_send_ready(void)
{
   struct mtip_send_ready_task* taskstruct = kmalloc(sizeof(struct mtip_send_ready_task), GFP_ATOMIC);
   mtip_queue_work(MTIP_WORKQ_TASK_INDICATE_READY, taskstruct, MTIP_PORT_TYPE_FH_0);
}

void run_mtip_client_send_ready(void* work_ptr)
{
   int i;
   struct mtip_send_ready_task* taskstruct = (struct mtip_send_ready_task*)work_ptr;
   unsigned long flags;
   spinlock_t *lock = &platform_driver_priv->driver_lock;
   eth_ecpriss_topology_ready_cb ready_cb = NULL;

   // update the topology since something might have changed
   mtip_update_topology();

   CSMLOGDBG("Sending Ready to all registered clients\n");

   for (i = 0; i < MTIP_MAX_CLIENTS; ++i)
   {
       spin_lock_irqsave(lock, flags);

       ready_cb = platform_driver_priv->clients[i].ready_cb;

       spin_unlock_irqrestore(lock, flags);

       if (ready_cb != NULL)
       {
           // invoke the client cb
           (*ready_cb)();
       }
   }

   // free the taskstruct
   kfree(taskstruct);
}

void mtip_client_send_event(eth_ecpriss_event_e event, u32 link_index)
{
   int i;
   unsigned long flags;
   spinlock_t *lock = &platform_driver_priv->driver_lock;
   eth_ecpriss_interface_events_cb events_cb = NULL;

   // update the topology since something might have changed
   mtip_update_topology();

   CSMLOGDBG("Sending Event to all registered clients\n");

   for (i = 0; i < MTIP_MAX_CLIENTS; ++i)
   {
       spin_lock_irqsave(lock, flags);

       events_cb = platform_driver_priv->clients[i].events_cb;

       spin_unlock_irqrestore(lock, flags);

       if (events_cb != NULL)
       {
           // invoke the client cb
           (*events_cb)(event, NULL);
       }
   }
}

void mtip_print_topology(eth_ecpriss_topology_root_s *topology)
{
    uint8_t i, j, k;
    uint8_t                        num_unique_port_types;
    eth_ecpriss_port_type_e        port_type;
    uint8_t                        num_ports;
    uint8_t                        port_index;
    uint8_t                        num_links;
    uint8_t                        link_index;
    uint16_t                       link_mtu;
    uint16_t                       link_state;
    eth_ecpriss_link_rate_e        link_rate;

    num_unique_port_types = topology->num_unique_port_types;

    // log the information here
    CSMLOGINFO("eth_topology_init_done %d", topology->eth_topology_init_done);
    CSMLOGINFO("num_unique_port_types %d", num_unique_port_types);

    for (i = 0; i < num_unique_port_types; ++i)
    {
        port_type = topology->topology_params[i].port_type;
        num_ports = topology->topology_params[i].num_ports;

        // print the topology->topology_params[i]
        CSMLOGINFO("index: %d, port_type: %d, num_ports: %d", i, port_type, num_ports);

        for (j = 0; j < num_ports; ++j) 
        {
            port_index = topology->topology_params[i].port_params[j].port_index;
            num_links = topology->topology_params[i].port_params[j].num_links;

            // print the topology->topology_params[i].port_params[j]
            CSMLOGINFO("index: (%d, %d) port_index: %d, num_links: %d", i, j, port_index, num_links);

            for (k = 0; k < num_links; ++k) 
            {
                link_index = topology->topology_params[i].port_params[j].link_params[k].link_index;
                link_mtu = topology->topology_params[i].port_params[j].link_params[k].link_mtu;
                link_state = topology->topology_params[i].port_params[j].link_params[k].link_state;
                link_rate = topology->topology_params[i].port_params[j].link_params[k].link_rate;

                // print the topology->topology_params[i].port_params[j].link_params[k]
                CSMLOGINFO("index: (%d, %d, %d) link_index: %d, link_mtu: %d, link_state: %d, link_rate: %d ", i, j, k, link_index, link_mtu, link_state, link_rate);
            }
        }
    }
}

/*
    Use the available port information to update the topology
 */
int mtip_setup_topology(void)
{
    eth_ecpriss_topology_root_s* topology;

    CSMLOGDBG("setting up initial topology\n");

    // allocate the memory for the topology structure
    topology = (eth_ecpriss_topology_root_s*)kmalloc(sizeof(eth_ecpriss_topology_root_s), GFP_KERNEL);

    // set the platform topology
    platform_driver_priv->topology = topology;

    // set the initial topology
    mtip_update_topology();

    return 0;
}

void mtip_free_topology(void)
{
    eth_ecpriss_topology_root_s* topology = NULL;
    unsigned long flags;
    spinlock_t *lock = &platform_driver_priv->driver_lock;

    CSMLOGDBG("Free topology\n");

    spin_lock_irqsave(lock, flags);

    // get the topology pointer
    topology = platform_driver_priv->topology;
    platform_driver_priv->topology = NULL;

    spin_unlock_irqrestore(lock, flags);

    if (topology)
        kfree(topology);
}
/*
    The exported function to get the current topology
 */
eth_ecpriss_status_e mtip_eth_get_topology(eth_ecpriss_dev_mode_e *device_mode, eth_ecpriss_topology_root_s *topology_params)
{
    eth_ecpriss_status_e ret = ETH_ECPRISS_STATUS_FAILURE;
    unsigned long flags;
    spinlock_t *lock = &platform_driver_priv->driver_lock;
    eth_ecpriss_topology_root_s* topology = NULL;

    spin_lock_irqsave(lock, flags);

    topology = platform_driver_priv->topology;

    spin_unlock_irqrestore(lock, flags);

    if (topology != NULL)
    {
        *device_mode = (eth_ecpriss_dev_mode_e)platform_driver_priv->devices.mode;

        memcpy(topology_params, platform_driver_priv->topology, sizeof(eth_ecpriss_topology_root_s));

        ret = ETH_ECPRISS_STATUS_SUCCESS;

        CSMLOGDBG("Device mode is %d, ret is %d", *device_mode, ret);
    }
    else
    {
        CSMLOGINFO("topology is NULL ret: %d", ret);
    }

    return ret;
}

eth_ecpriss_status_e mtip_eth_register_events_cb(eth_ecpriss_interface_events_cb events_cb)
{
    eth_ecpriss_status_e ret = ETH_ECPRISS_STATUS_FAILURE;
    int i;
    unsigned long flags;
    spinlock_t *lock = &platform_driver_priv->driver_lock;

    spin_lock_irqsave(lock, flags);

    // find the next open spot
    for (i = 0; i < MTIP_MAX_CLIENTS; ++i) {

        if (platform_driver_priv->clients[i].events_cb == NULL) {
            platform_driver_priv->clients[i].events_cb = events_cb;
            ret = ETH_ECPRISS_STATUS_SUCCESS;
        }
    }

    spin_unlock_irqrestore(lock, flags);
    return ret;
}
eth_ecpriss_status_e mtip_eth_deregister_events_cb()
{
    eth_ecpriss_status_e ret = ETH_ECPRISS_STATUS_SUCCESS;
    int i;
    unsigned long flags;
    spinlock_t *lock = &platform_driver_priv->driver_lock;

    spin_lock_irqsave(lock, flags);

    // find the next open spot
    for (i = 0; i < MTIP_MAX_CLIENTS; ++i) {
            platform_driver_priv->clients[i].events_cb = NULL;
            platform_driver_priv->clients[i].ready_cb = NULL;
    }

    spin_unlock_irqrestore(lock, flags);
    return ret;
}

eth_ecpriss_status_e mtip_eth_register_ready_cb(eth_ecpriss_topology_ready_cb ready_cb, bool *is_ready)
{
    eth_ecpriss_status_e ret = ETH_ECPRISS_STATUS_FAILURE;
    int i;
    unsigned long flags;
    spinlock_t *lock = &platform_driver_priv->driver_lock;

    spin_lock_irqsave(lock, flags);

    // find the next open spot
    for (i = 0; i < MTIP_MAX_CLIENTS; ++i) {

        if (platform_driver_priv->clients[i].ready_cb == NULL) {
            platform_driver_priv->clients[i].ready_cb = ready_cb;

            // check if we are ready to handle get requests
            if (platform_driver_priv->topology != NULL) {
                *is_ready = true;
            }
            else
            {
                *is_ready = false;
            }
            ret = ETH_ECPRISS_STATUS_SUCCESS;
        }
    }

    spin_unlock_irqrestore(lock, flags);
    return ret;
}

int setup_interface_in_loopback_mode(struct net_device *netdev, u32 link_index)
{
    struct mtip_netdev_priv *priv;
    int ret = 0, i;
    u32 port_type;

    priv = netdev_priv(netdev);

    mtip_c2c2_loopback_mode = MTIP_MODE_PHY_LOOPBACK;

    // set the promiscous mode
    ret = mtip_mac_set_promisc_mode(priv, true);

    if(!mtip_loopback_enable_arp)
    {
        /* add NOARP */
        netdev->flags |= IFF_NOARP;
    }

    if (platform_driver_priv->mtip_links[link_index] != NULL)
    {
        platform_driver_priv->mtip_links[link_index]->num_assigned_lanes = 1;

        // set the lane_index to be the same as link_index
        platform_driver_priv->mtip_links[link_index]->assigned_lane_indices[0] = link_index;

        if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
        {
            CSMLOGERR("invalid port_type for link_index %d", link_index);
            return -1;
        }
       // setup the ports for loopback

       // don't do autoneg for loopback modes
        platform_driver_priv->mtip_ports[port_type]->autoneg = false;

        // set the port state as connected
        platform_driver_priv->mtip_ports[port_type]->port_state = MTIP_PORT_STATE_CONNECTED;

        // set the below port priv flags supported for L2 port
        // 1x100GBASE_R2, 1x50GBASE_R, 1x50GBASE_R2, 1x25GBASE_R, 1x10GBASE_R
        platform_driver_priv->mtip_ports[port_type]->port_priv_flags = (1 << MTIP_PORT_CONFIG_1x25GBASE_R);

        // set the port sfp as DAC
        platform_driver_priv->mtip_ports[port_type]->sfp_port_type = PORT_DA;

        for (i = 0; i < 2; ++i)
        {
            platform_driver_priv->mtip_ports[port_type]->lane_config[i].lane_enabled = true;
            platform_driver_priv->mtip_ports[port_type]->lane_config[i].lane_speed = PHY_LANE_SPEED_25G;
            platform_driver_priv->mtip_ports[port_type]->lane_config[i].link_index = (port_type*PHY_LANE_MAX) + i;
        }
    }
    for (i = MTIP_L2_LANE1_INDEX ; i <= MTIP_L2_LANE2_INDEX ; ++i)
    {
        if (platform_driver_priv->mtip_lanes[i] != NULL)
        {

            // set the lane state as CONNECTED
            platform_driver_priv->mtip_lanes[i]->lane_state = MTIP_LANE_STATE_CONNECTED;

            // set the lane sfp as DAC
            platform_driver_priv->mtip_lanes[i]->sfp_port_type = PORT_DA;

            // set the lane speed mask
            platform_driver_priv->mtip_lanes[i]->speed_mask = TRX_LANE_SPEED_10G | TRX_LANE_SPEED_25G | TRX_LANE_SPEED_50G | TRX_LANE_SPEED_100G;

            // set the lane properties for TRX
            platform_driver_priv->mtip_lanes[i]->lane_qsfp_info.trx_module_type = TRX_QSFP_PLS_QSFP28_QSFP56;
            platform_driver_priv->mtip_lanes[i]->lane_qsfp_info.speed_mask = TRX_LANE_SPEED_10G | TRX_LANE_SPEED_25G | TRX_LANE_SPEED_50G | TRX_LANE_SPEED_100G;
            platform_driver_priv->mtip_lanes[i]->lane_qsfp_info.trx_laneinfo = 0x3;
            platform_driver_priv->mtip_lanes[i]->lane_qsfp_info.trx_bout_cfg = 0;
        }
    }
    CSMLOGINFO("Setting up interface:%d in loopback mode\n", link_index);
    post_mtip_process_link_state(link_index, true);
    return 0;
}

/*
 * mtip_eth_enable_ru_cascade_c2c_bringup - Bring up C2C2 (eth30) and
 * C2C1 (eth31) in E2E mode for RU cascade.
 *
 * Called from ecpriss_eth_topology_init_wq() when ru_cascade_mode=1.
 */

eth_ecpriss_status_e mtip_eth_enable_ru_cascade_c2c_bringup(void)
{
    eth_ecpriss_status_e ret = ETH_ECPRISS_STATUS_SUCCESS;
    struct net_device *netdev_c2c2 = NULL;
    struct net_device *netdev_c2c1 = NULL;
    struct mtip_netdev_priv *priv_c2c2 = NULL;
    struct mtip_netdev_priv *priv_c2c1 = NULL;
    u32 port_type = MTIP_PORT_TYPE_L2;
    u32 lane_index;
    int i, open_ret;
    struct mtip_process_lane_up lane_up_info;

    CSMLOGINFO("mtip_eth_enable_ru_cascade_c2c_bringup: E2E bringup of C2C2(eth30) and C2C1(eth31)\n");

    /* --- Null guards --- */
    if (platform_driver_priv == NULL)
    {
        CSMLOGERR("mtip_eth_enable_ru_cascade_c2c_bringup: platform_driver_priv is NULL\n");
        return ETH_ECPRISS_STATUS_FAILURE;
    }

    if (platform_driver_priv->mtip_links[MTIP_L2_ETH_LINK_INDEX] == NULL ||
        platform_driver_priv->mtip_links[MTIP_C2C1_ETH_LINK_INDEX] == NULL ||
        platform_driver_priv->mtip_ports[port_type] == NULL)
    {
        CSMLOGERR("mtip_eth_enable_ru_cascade_c2c_bringup: links or port not allocated\n");
        return ETH_ECPRISS_STATUS_FAILURE;
    }

    netdev_c2c2 = platform_driver_priv->mtip_links[MTIP_L2_ETH_LINK_INDEX]->dev;
    netdev_c2c1 = platform_driver_priv->mtip_links[MTIP_C2C1_ETH_LINK_INDEX]->dev;

    if (!netdev_c2c2 || !netdev_c2c1)
    {
        CSMLOGERR("mtip_eth_enable_ru_cascade_c2c_bringup: netdev is NULL\n");
        return ETH_ECPRISS_STATUS_FAILURE;
    }

    if (platform_driver_priv->devices.port_devices[port_type].num_lane_phandles == 0)
    {
        CSMLOGERR("mtip_eth_enable_ru_cascade_c2c_bringup: no lane phandles for L2 port\n");
        return ETH_ECPRISS_STATUS_FAILURE;
    }


    for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_lane_phandles; ++i)
    {
        if (platform_driver_priv->devices.port_devices[port_type].lane_devices[i] == NULL)
        {
            CSMLOGERR("mtip_eth_enable_ru_cascade_c2c_bringup: lane_devices[%d] is NULL\n", i);
            return ETH_ECPRISS_STATUS_FAILURE;
        }

        lane_index = platform_driver_priv->devices.port_devices[port_type].lane_devices[i]->lane_index;

        if (lane_index >= MTIP_MAX_LANES)
        {
            CSMLOGERR("mtip_eth_enable_ru_cascade_c2c_bringup: lane_index %d out of bounds\n", lane_index);
            return ETH_ECPRISS_STATUS_FAILURE;
        }

        if (platform_driver_priv->mtip_lanes[lane_index] == NULL)
        {
            CSMLOGERR("mtip_eth_enable_ru_cascade_c2c_bringup: mtip_lanes[%d] is NULL\n", lane_index);
            return ETH_ECPRISS_STATUS_FAILURE;
        }

      
        platform_driver_priv->mtip_lanes[lane_index]->sfp_port_type              = PORT_DA;
        platform_driver_priv->mtip_lanes[lane_index]->speed_mask                 = TRX_LANE_SPEED_10G | TRX_LANE_SPEED_25G |
                                                                                   TRX_LANE_SPEED_50G | TRX_LANE_SPEED_100G;
        platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info.trx_module_type = TRX_QSFP_PLS_QSFP28_QSFP56;
        platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info.speed_mask      = TRX_LANE_SPEED_10G | TRX_LANE_SPEED_25G |
                                                                                        TRX_LANE_SPEED_50G | TRX_LANE_SPEED_100G;
        /* L2 port has 2 lanes (eth30=lane0, eth31=lane2); trx_laneinfo bits 0 and 1
         * represent those two lanes. 0x3 = both lanes present, which is what
         * mtip_device_filter_priv_flags() checks for num_lanes=2 on L2 port. */
        platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info.trx_laneinfo    = 0x3;
        platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info.trx_bout_cfg    = 0;
        /* Also set lane_state = CONNECTED synchronously so that
         * mtip_device_configure_port() sees all lanes connected immediately
         * and does not bail out waiting for them. */
        platform_driver_priv->mtip_lanes[lane_index]->lane_state                 = MTIP_LANE_STATE_CONNECTED;

        memset(&lane_up_info, 0, sizeof(lane_up_info));
        lane_up_info.lane_index     = lane_index;
        lane_up_info.sfp_port_type  = PORT_DA;
        lane_up_info.speed_mask     = TRX_LANE_SPEED_10G | TRX_LANE_SPEED_25G |
                                      TRX_LANE_SPEED_50G | TRX_LANE_SPEED_100G;
        lane_up_info.lane_connected = true;

        CSMLOGINFO("mtip_eth_enable_ru_cascade_c2c_bringup: msglvl1 lane_up lane_index=%d PORT_DA\n", lane_index);
        post_mtip_phy_handle_lane_up(lane_up_info);
    }


    platform_driver_priv->mtip_ports[port_type]->sfp_port_type  = PORT_DA;
    platform_driver_priv->mtip_ports[port_type]->port_state     = MTIP_PORT_STATE_CONNECTED;

    netdev_c2c2->flags |= IFF_PROMISC;
    netdev_c2c1->flags |= IFF_PROMISC;
    CSMLOGINFO("mtip_eth_enable_ru_cascade_c2c_bringup: IFF_PROMISC pre-set on eth30 and eth31\n");

    rtnl_lock();
    if (netif_running(netdev_c2c2))
        dev_close(netdev_c2c2);
    open_ret = dev_open(netdev_c2c2, NULL);
    rtnl_unlock();

    if (open_ret)
    {
        CSMLOGERR("mtip_eth_enable_ru_cascade_c2c_bringup: dev_open(eth30) failed: %d\n", open_ret);
        ret = ETH_ECPRISS_STATUS_FAILURE;
        goto out;
    }
    CSMLOGINFO("mtip_eth_enable_ru_cascade_c2c_bringup: dev_open(eth30) done\n");

    /* Explicitly set promisc on eth30 MAC after open in case
     * mtip_mac_initialize() ran before IFF_PROMISC was visible. */
    priv_c2c2 = netdev_priv(netdev_c2c2);
    if (mtip_mac_set_promisc_mode(priv_c2c2, true))
        CSMLOGERR("mtip_eth_enable_ru_cascade_c2c_bringup: promisc set failed for eth30\n");
    else
        CSMLOGINFO("mtip_eth_enable_ru_cascade_c2c_bringup: promisc enabled for eth30\n");

    rtnl_lock();
    if (netif_running(netdev_c2c1))
        dev_close(netdev_c2c1);
    open_ret = dev_open(netdev_c2c1, NULL);
    rtnl_unlock();

    if (open_ret)
    {
        CSMLOGERR("mtip_eth_enable_ru_cascade_c2c_bringup: dev_open(eth31) failed: %d\n", open_ret);
        ret = ETH_ECPRISS_STATUS_FAILURE;
        goto out;
    }
    CSMLOGINFO("mtip_eth_enable_ru_cascade_c2c_bringup: dev_open(eth31) done\n");

    /* Explicitly set promisc on eth31 MAC after open. */
    priv_c2c1 = netdev_priv(netdev_c2c1);
    if (mtip_mac_set_promisc_mode(priv_c2c1, true))
        CSMLOGERR("mtip_eth_enable_ru_cascade_c2c_bringup: promisc set failed for eth31\n");
    else
        CSMLOGINFO("mtip_eth_enable_ru_cascade_c2c_bringup: promisc enabled for eth31\n");

    /* -----------------------------------------------------------------------
     * Set 25G speed mode for C2C2
     * ----------------------------------------------------------------------- */
    priv_c2c2->priv_flags     = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_25G_ONLY_L2_PORT;
    priv_c2c2->priv_flags_set = true;

    platform_driver_priv->mtip_ports[port_type]->autoneg         = false;
    platform_driver_priv->mtip_ports[port_type]->autoneg_changed = true;

    CSMLOGINFO("mtip_eth_enable_ru_cascade_c2c_bringup: triggering reconfigure at 25G autoneg=off\n");
    mtip_netdev_set_port_priv_flags(netdev_c2c2);

out:
    return ret;
}

/*
    The exported function to enable link
 */
eth_ecpriss_status_e mtip_eth_enable_logging_port(bool action)
{
    eth_ecpriss_status_e ret = ETH_ECPRISS_STATUS_SUCCESS;
    struct net_device *netdev = NULL;
    static char *ifname = "eth30";
    u32 link_index = MTIP_L2_ETH_LINK_INDEX;

    if (platform_driver_priv->mtip_links[link_index] != NULL)
        netdev = platform_driver_priv->mtip_links[link_index]->dev;
    else
        return ETH_ECPRISS_STATUS_FAILURE;

    if(action == true)
    {

        if(platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_UP)
        {
            CSMLOGINFO("Enabling PHY NES loopback mode for Interface %s\n", ifname);
            mtip_phy_set_c2c_phy_loopback_mode(QCOM_AW_PHY_NEAR_END_SERIAL_LB);
            setup_interface_in_loopback_mode(netdev, link_index);

            rtnl_lock(); // Lock the network namespace
            if (!(netdev->flags & IFF_UP))
            {
                CSMLOGINFO("Bringing up interface %s\n", ifname);
                ret = dev_open(netdev, NULL);
                if (ret)
                    CSMLOGERR("Failed to bring up interface %s: %d\n", ifname, ret);
                else
                    CSMLOGINFO("Interface %s is now up\n", ifname);
            }
            else
            {
                CSMLOGINFO("Interface %s is already up\n", ifname);
            }
            rtnl_unlock();
        }
    }
    else
    {
         rtnl_lock();   // Required before calling dev_close
         if (netif_running(netdev))
         {
             CSMLOGINFO("dev_close: Bringing down interface %s\n", ifname);
             dev_close(netdev);
             CSMLOGINFO("Interface %s is now down\n", ifname);
         }
         else
         {
             CSMLOGINFO("dev_close: Interface %s is already down\n", ifname);
         }
         rtnl_unlock();
    }

    return ret;
}


void mtip_eth_reeval_logging_port(void)
{
    int i =0;
    struct mtip_port_info* port_info;
    u32 port_type = 0;
    u32 link_index = 0;
    u32 lane_index = 0;
    int lane_speed = 0;
    struct net_device *netdev = NULL;
    static char *ifname = "eth30";
    u32 c2c_link_index = MTIP_L2_ETH_LINK_INDEX;

    if (platform_driver_priv == NULL)
        return;

    for(port_type = 0; port_type < MTIP_MAX_FH_PORTS; port_type++)
    {
        port_info = platform_driver_priv->mtip_ports[port_type];
        if(!port_info)
          continue;

        for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_lane_phandles; ++i)
        {
            lane_index = platform_driver_priv->devices.port_devices[port_type].lane_devices[i]->lane_index;

            if(mtip_lookup_link_index_by_lane_index(&link_index, lane_index) == 0)
            {
                 if (platform_driver_priv->mtip_links[link_index] != NULL)
                 {
                    if ((platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_INIT) &&
                        (platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_CLOSE))
                    {
                       lane_speed += mtip_platform_convert_lane_speed_to_gbps(port_info->lane_config[i].lane_speed);
                       CSMLOGINFO("Lane index: %d, lane_speed %d", lane_index, mtip_platform_convert_lane_speed_to_gbps(port_info->lane_config[i].lane_speed));
                    }
                 }
            }
        }
    }

    if(lane_speed == (MTIP_MAX_FH_PORTS * mtip_platform_convert_lane_speed_to_gbps(PHY_LANE_SPEED_100G)))
    {
        if (platform_driver_priv->mtip_links[c2c_link_index] != NULL)
        {
            netdev = platform_driver_priv->mtip_links[c2c_link_index]->dev;

            rtnl_lock();   // Required before calling dev_close
            if (netif_running(netdev))
            {
                CSMLOGINFO("dev_close: Bringing down interface %s as 300G FH limit is reached\n", ifname);
                dev_close(netdev);
                CSMLOGINFO("Interface %s is now down\n", ifname);
            }
            else
            {
                CSMLOGINFO("dev_close: Interface %s is already down\n", ifname);
            }
            rtnl_unlock();                
        }
    }

    return;
}

struct eth_ecpriss_ops mtip_ecpri_ops = {
    .eth_ecpriss_register_ready_cb = mtip_eth_register_ready_cb,
    .eth_ecpriss_register_events_cb = mtip_eth_register_events_cb,
    .eth_ecpriss_deregister_events_cb = mtip_eth_deregister_events_cb,
    .eth_ecpriss_get_topology = mtip_eth_get_topology,
    .eth_ecpriss_enable_logging_port = mtip_eth_enable_logging_port,
    .eth_ecpriss_enable_ru_cascade_c2c_bringup = mtip_eth_enable_ru_cascade_c2c_bringup,
};

EXPORT_SYMBOL(mtip_ecpri_ops);

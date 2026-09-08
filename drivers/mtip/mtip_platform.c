//SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */ 

#include <linux/init.h>

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
#include <linux/clk.h>

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
#include <linux/of.h>
#include <linux/ethtool.h>
#include <linux/nvmem-consumer.h>

#include "mtip_platform.h"
#include "mtip_dma.h"
#include "mtip.h"
#include "mtip_device.h"
#include "mtip_mac.h"
#include "mtip_mdio.h"
#include "mtip_hashmap.h"
#include "mtip_client.h"
#include "mtip_ptp.h"
#include "mtip_sysfs.h"
#include "mtip_pcs.h"
#include "mtip_phy.h"
#include "mtip_dut.h"
#include "mtip_clocks.h"
#include "mtip_ethtool.h"
#include "mtip_workq.h"
#include "mtip_debug_eth.h"

int mtip_netdev_header(struct sk_buff *skb, struct net_device *dev,
                 unsigned short type, const void *daddr, const void *saddr,
                 unsigned int len);

const struct header_ops mtip_header_ops = {
   .create  = mtip_netdev_header,
   .cache   = NULL,
};

static int mtip_platform_setup(void);
/*
 * mtip_platform_setup_link: allocate memory for the link and connect to the dma
 */
static int mtip_platform_setup_link(unsigned int port_device_index, unsigned int link_device_index)
{
   ecpri_dma_eth_conn_hdl_t hdl;
   int rv = 0;
   u32 link_index;

   // get the link index using port number and link number
   link_index = platform_driver_priv->devices.port_devices[port_device_index].link_devices[link_device_index]->link_index;

   if (platform_driver_priv->mtip_links[link_index] != NULL)
   {
       CSMLOGERR("Link index %d is already in use!\n", link_index);
       return -1;
   }

   platform_driver_priv->mtip_links[link_index] = (struct mtip_link_info*)kmalloc(sizeof(struct mtip_link_info), GFP_KERNEL);

   // HANDLE THE ERROR
   if (platform_driver_priv->mtip_links[link_index] == NULL)
   {
      CSMLOGERR("failed to allocated memory for mtip_links[%d]\n", link_index);
      rv = -ENOMEM;
      goto out;
   }

   memset(platform_driver_priv->mtip_links[link_index], 0, sizeof(struct mtip_link_info));

   // set the link device
   platform_driver_priv->mtip_links[link_index]->link_index = link_index;

   platform_driver_priv->mtip_links[link_index]->ptp_ts_enabled = false;

   platform_driver_priv->mtip_links[link_index]->lanes_assignment_complete = false;
   platform_driver_priv->mtip_links[link_index]->num_assigned_lanes = 0;

   // Initialize loopback_enabled to false
   platform_driver_priv->mtip_links[link_index]->loopback_enabled = false;

   // initialize the mutex
   mutex_init(&platform_driver_priv->mtip_links[link_index]->dev_lock);

   // initialize the PTP lists for the link index
   mtip_ptp_initialize(link_index);
   mtip_dma_tx_comp_list_initialize(link_index);

   // set the link state to INIT
   platform_driver_priv->mtip_links[link_index]->state = MTIP_LINK_STATE_INIT;

   platform_driver_priv->mtip_links[link_index]->config_fec = ETHTOOL_FEC_NONE;
   platform_driver_priv->mtip_links[link_index]->active_fec = ETHTOOL_FEC_OFF;

   platform_driver_priv->mtip_links[link_index]->link_down_received_post_link_up = false;

   timer_setup(&platform_driver_priv->mtip_links[link_index]->phy_retry_timer,
               &mtip_phy_retry_timer_cb, 0);
   platform_driver_priv->mtip_links[link_index]->phy_retry_timer_valid = true;

   timer_setup(&platform_driver_priv->mtip_links[link_index]->rx_replenish_retry_timer,
               &mtip_rx_replenish_retry_timer_cb, 0);

   timer_setup(&platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer,
               &mtip_pcs_link_up_defer_timer_cb, 0);
   platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer_running = false;

   if(link_index != MTIP_DEBUG_ETH_LINK_INDEX){
      // connect to the dma pipe
      rv = mtip_connect_dma_pipe(link_index, &hdl);

      // HANDLE THE ERROR
      if (rv < 0)
      {
         CSMLOGERR("dma connect pipe failed for link_index: %d\n", link_index);
         goto cleanup;
      }

      // set the hdl of the link
      platform_driver_priv->mtip_links[link_index]->dma_hdl = hdl;

      // add to hashmap
      mtip_hashmap_insert(hdl, link_index);

      CSMLOGDBG("connect_dma_pipe is complete with hdl: %d for link_index: %d\n", hdl, link_index);
   }

   goto out;

cleanup:
   kfree(platform_driver_priv->mtip_links[link_index]);
   platform_driver_priv->mtip_links[link_index] = NULL;

out:
   return rv;
}

/*
 * mtip_platform_setup_lane: allocate memory for the lane
 */
static int mtip_platform_setup_lane(unsigned int port_device_index, unsigned int lane_device_index)
{
   int rv = 0;
   u32 lane_index;

   // get the lane index using port number and link number
   lane_index = platform_driver_priv->devices.port_devices[port_device_index].lane_devices[lane_device_index]->lane_index;

   if (platform_driver_priv->mtip_lanes[lane_index] != NULL)
   {
       CSMLOGERR("Lane index %d is already in use!\n", lane_index);
       return -1;
   }

   platform_driver_priv->mtip_lanes[lane_index] = (struct mtip_lane_info*)kmalloc(sizeof(struct mtip_lane_info), GFP_KERNEL);

   // HANDLE THE ERROR
   if (platform_driver_priv->mtip_lanes[lane_index] == NULL)
   {
      CSMLOGERR("failed to allocated memory for mtip_lanes[%d]\n", lane_index);
      rv = -ENOMEM;
      goto out;
   }

   memset(platform_driver_priv->mtip_lanes[lane_index], 0, sizeof(struct mtip_lane_info));

   // set the lane device
   platform_driver_priv->mtip_lanes[lane_index]->lane_index = lane_index;

   // set the lane state to INIT
   platform_driver_priv->mtip_lanes[lane_index]->lane_state = MTIP_LANE_STATE_INIT;

   // initialize the lock
   spin_lock_init(&platform_driver_priv->mtip_lanes[lane_index]->lock);

out:
   return rv;
}

static int mtip_platform_setup_port(u32 port_type)
{
    int ret = 0;

    if (platform_driver_priv->mtip_ports[port_type] != NULL)
    {
        CSMLOGERR("port_type %d is already in use!\n", port_type);
        return -1;
    }

    platform_driver_priv->mtip_ports[port_type] = (struct mtip_port_info*)kmalloc(sizeof(struct mtip_port_info), GFP_KERNEL);

    if (platform_driver_priv->mtip_ports[port_type] == NULL) 
    {
        CSMLOGERR("failed to alloc memory for port_type %d", port_type);
        ret = -ENOMEM;
        goto out;
    }

    memset(platform_driver_priv->mtip_ports[port_type], 0, sizeof(struct mtip_port_info));

    // set the port_type
    platform_driver_priv->mtip_ports[port_type]->port_type = port_type;

    // set the port_state
    platform_driver_priv->mtip_ports[port_type]->port_state = MTIP_PORT_STATE_INIT;

          // the default is to enable autoneg

    if (port_type == MTIP_PORT_TYPE_L2)
    {
        if(mtip_c2c2_loopback_mode == MTIP_MODE_DEFAULT)
        {
            // set the below port priv flags supported for L2 port
            // 1x100GBASE_R2, 1x50GBASE_R, 1x50GBASE_R2, 1x25GBASE_R, 1x10GBASE_R
            platform_driver_priv->mtip_ports[port_type]->autoneg = true;
            platform_driver_priv->mtip_ports[port_type]->port_priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_L2_PORT_NON_FEC;
        }
        else
        {
            // don't do autoneg for loopback modes
            platform_driver_priv->mtip_ports[port_type]->autoneg = false;
            // set the below port priv flags supported for L2 port
            // 1x100GBASE_R2, 1x50GBASE_R, 1x50GBASE_R2, 1x25GBASE_R, 1x10GBASE_R
            platform_driver_priv->mtip_ports[port_type]->port_priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_L2_PORT_NON_FEC;
        }
    }
    else if (port_type == MTIP_PORT_TYPE_DEBUG )
    {

        if (mtip_loopback_mode == MTIP_MODE_DEFAULT)
        {
            platform_driver_priv->mtip_ports[port_type]->autoneg = true;
            platform_driver_priv->mtip_ports[port_type]->port_priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_DBG_PORT_NON_FEC_NON_50G;

        }
        else
        {
            // don't do autoneg for loopback modes
            platform_driver_priv->mtip_ports[port_type]->autoneg = false;

            // set the default port config to 4x25GBASE_R
            platform_driver_priv->mtip_ports[port_type]->port_config = MTIP_PORT_CONFIG_4x25GBASE_R;

            // set the default port priv flags
            platform_driver_priv->mtip_ports[port_type]->port_priv_flags = (1 << MTIP_PORT_CONFIG_4x25GBASE_R);
        }
    }
    else
    {
        if (mtip_loopback_mode == MTIP_MODE_DEFAULT)
        {
            platform_driver_priv->mtip_ports[port_type]->autoneg = true;
            platform_driver_priv->mtip_ports[port_type]->port_priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_NON_FEC;
        }
        else
        {
            // don't do autoneg for loopback modes
            platform_driver_priv->mtip_ports[port_type]->autoneg = false;

            // set the default port config to 4x25GBASE_R
            platform_driver_priv->mtip_ports[port_type]->port_config = MTIP_PORT_CONFIG_4x25GBASE_R;

            // set the default port priv flags
            platform_driver_priv->mtip_ports[port_type]->port_priv_flags = (1 << MTIP_PORT_CONFIG_4x25GBASE_R);
        }
    }

    // set the default sfp port type
    platform_driver_priv->mtip_ports[port_type]->sfp_port_type = PORT_DA;

    // initialize the lock
    spin_lock_init(&platform_driver_priv->mtip_ports[port_type]->lock);

out:
    return ret;
}

static int mtip_platform_cleanup_link(unsigned int link_index)
{
   if (platform_driver_priv->mtip_links[link_index] != NULL) 
   {
      if(platform_driver_priv->mtip_links[link_index]->dma_hdl != 0)
      {
         mutex_lock(&platform_driver_priv->mtip_links[link_index]->dev_lock);
         if (platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_INIT && platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_CLOSE)
         {
            mutex_unlock(&platform_driver_priv->mtip_links[link_index]->dev_lock);
            // stop the pipe
            mtip_stop_dma_pipe(platform_driver_priv->mtip_links[link_index]->dma_hdl);
         }
         else
         {
            mutex_unlock(&platform_driver_priv->mtip_links[link_index]->dev_lock);
         }
         // disconnect the pipes
         mtip_disconnect_dma_pipe(platform_driver_priv->mtip_links[link_index]->dma_hdl);

         // remove from hashmap
         mtip_hashmap_remove(platform_driver_priv->mtip_links[link_index]->dma_hdl);

         // reset the hdl of the link
         platform_driver_priv->mtip_links[link_index]->dma_hdl = 0;
      }

      platform_driver_priv->mtip_links[link_index]->phy_retry_timer_valid = false;
      del_timer_sync(&platform_driver_priv->mtip_links[link_index]->phy_retry_timer);

      del_timer_sync(&platform_driver_priv->mtip_links[link_index]->rx_replenish_retry_timer);

      platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer_running = false;
      del_timer_sync(&platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer);
   }
   return 0;
}

static int mtip_platform_cleanup_lane(unsigned int lane_index)
{
   struct phylink *phylink = NULL;
   struct net_device* ndev = NULL;

   if (platform_driver_priv->mtip_lanes[lane_index] != NULL)
   {
     phylink = platform_driver_priv->mtip_lanes[lane_index]->phylink;
     ndev = platform_driver_priv->mtip_lanes[lane_index]->lane_dummy_ndev;

     mtip_phy_destroy_phylink(phylink, ndev);

     kfree(platform_driver_priv->mtip_lanes[lane_index]);
     platform_driver_priv->mtip_lanes[lane_index] = NULL;
   }
   return 0;
}

static void mtip_platform_cleanup_ports(void) 
{
   int i;

   CSMLOGINFO("cleaning up %d ports\n", MTIP_MAX_PORTS);

   for (i = 0; i < MTIP_MAX_PORTS; ++i) 
   {
       if (platform_driver_priv->mtip_ports[i] != NULL) 
       {
           // free the port info allocation
           kfree(platform_driver_priv->mtip_ports[i]);

           platform_driver_priv->mtip_ports[i] = NULL;
       }
   }
}

static bool mtip_platform_check_if_all_probes_received()
{
   int i, j, k;
   bool phandle_found = false;
   struct mtip_devices_info *devices;
   struct mtip_port_device_info *port_device;
   u32 phandle;
   unsigned long flags;
   spinlock_t *lock = &platform_driver_priv->driver_lock;
   bool setup_platform = false;

   // first check that the platform device is valid
   if (platform_driver_priv->devices.platform_device_valid == 0)
   {
      CSMLOGDBG("platform device probe not received");
      return false;
   }

   // check if we have already complete platform setup
   if (platform_driver_priv->devices.platform_setup_complete > 0) 
   {
       CSMLOGDBG("platform already setup. ignoring");
       return true;
   }

   devices = &platform_driver_priv->devices;

   // match the port phandles to the port devices
   for (i = 0; i < devices->num_port_phandles; ++i)
   {
      phandle = devices->port_phandles[i];
      phandle_found = false;

      // look up port devices to see if this phandle has been received
      for (j = 0; j < MTIP_MAX_PORTS; ++j)
      {
         if (devices->port_devices[j].port_device_valid == 1)
         {
            if (devices->port_devices[j].port_phandle == phandle)
            {
               phandle_found = true;
               break;
            }
         }
      }

      if (phandle_found == false)
      {
         // port probe not received yet
         CSMLOGDBG("port probe for phandle %d not received", phandle);
         return false;
      }
   }

   // all port probes have been received
   CSMLOGDBG("All port probes received");

   // check if all link probes have been received
   for (i = 0; i < MTIP_MAX_PORTS; ++i)
   {
       if (platform_driver_priv->devices.port_devices[i].port_device_valid) 
       {
           port_device = &devices->port_devices[i];

          for (j = 0; j < port_device->num_link_phandles; ++j)
          {
             phandle = port_device->link_phandles[j];
             phandle_found = false;

             // look up link devices to see if this phandle has been received
             for (k = 0; k < MTIP_MAX_LINKS; ++k)
             {
                if (devices->link_devices[k].link_device_valid == 1)
                {
                   if (devices->link_devices[k].link_phandle == phandle)
                   {
                      phandle_found = true;

                      // set the link device pointer
                      port_device->link_devices[j] = &devices->link_devices[k];
                      break;
                   }
                }
             }

             if (phandle_found == false)
             {
                // link probe not received yet
                CSMLOGDBG("link probe for phandle %d not received", phandle);
                return false;
             }
          }
       }
   }

   // all link probes have been received
   CSMLOGDBG("All link probes received");

   // check if all lane probes have been received
   for (i = 0; i < MTIP_MAX_PORTS; ++i)
   {
       if (platform_driver_priv->devices.port_devices[i].port_device_valid) 
       {
           port_device = &devices->port_devices[i];

          for (j = 0; j < port_device->num_lane_phandles; ++j)
          {
             phandle = port_device->lane_phandles[j];
             phandle_found = false;

             // look up lane devices to see if this phandle has been received
             for (k = 0; k < MTIP_MAX_LANES; ++k)
             {
                if (devices->lane_devices[k].lane_device_valid == 1)
                {
                   if (devices->lane_devices[k].lane_phandle == phandle)
                   {
                      phandle_found = true;

                      // set the lane device pointer
                      port_device->lane_devices[j] = &devices->lane_devices[k];
                      break;
                   }
                }
             }

             if (phandle_found == false)
             {
                // lane probe not received yet
                CSMLOGDBG("lane probe for phandle %d not received", phandle);
                return false;
             }
          }
       }
   }

   // all lane probes have been received
   CSMLOGINFO("All probes received");

   // acquire the lock
   spin_lock_irqsave(lock, flags);

   if (platform_driver_priv->devices.platform_setup_complete == 0)
   {
       platform_driver_priv->devices.platform_setup_complete = 1;
       setup_platform = true;
   }

   // free the lock
   spin_unlock_irqrestore(lock, flags);

   if (setup_platform == true) 
   {
       // setup the platform now
       mtip_platform_setup();
   }

   return true;
}

int mtip_link_probe(struct platform_device *pdev) 
{
    int ret = 0;
    int result = 0;
    struct mtip_link_device_info link_device = {0};
    struct resource *resource;
    unsigned long flags;
    spinlock_t *lock = &platform_driver_priv->driver_lock;
    u32 link_index;
    const char *linkname;

    CSMLOGDBG("mtip_link_probe called of device \"%s\"\n", pdev->name);

    link_device.link_pdev = pdev;
    link_device.link_phandle = pdev->dev.of_node->phandle;

    CSMLOGDBG("phandle of the link device: %d\n", link_device.link_phandle);

    /* Get the link index */
    result	= of_property_read_u32(pdev->dev.of_node, "qcom,mac-link-index", &link_index);

    if (result < 0) {
        CSMLOGERR(":get resource failed for qcom,mac-link-index\n");
        return ret;
    }

    link_device.link_index = link_index;
    CSMLOGDBG("qcom,mac-link-index is %d\n", link_device.link_index);

    /* Get the name */
    result = of_property_read_string(pdev->dev.of_node, "qcom,mac-link-name", &linkname);

    if (result < 0) {
        CSMLOGERR(":get resource failed for reg-names\n");
        return ret;
    }

    link_device.link_name = linkname;
    CSMLOGDBG("link name is %s\n", link_device.link_name);

    // get the mac link base address
   resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mac");
   if (!resource) {
       CSMLOGERR(":get resource failed for mac\n");
       return -ENODEV;
   }
   CSMLOGDBG("index: %d, link base = 0x%x, link size = 0x%x\n", link_device.link_index, resource->start, resource_size(resource));

   // set the MAC base address
   link_device.mac_ioaddr = devm_ioremap_resource(&pdev->dev, resource);

    if (mtip_rumi_platform == MTIP_PLATFORM_SOC)
   {
       // get the pcs link base address
       resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcs");
       if (!resource) {
           CSMLOGERR(":get resource failed for pcs\n");
           return -ENODEV;
       }
       CSMLOGDBG("index: %d, link base = 0x%x, link size = 0x%x\n", link_device.link_index, resource->start, resource_size(resource));

       // set the base address
       link_device.pcs_ioaddr = devm_ioremap_resource(&pdev->dev, resource);
   }

    // set this as a valid device
    link_device.link_device_valid = 1;

    spin_lock_irqsave(lock, flags);

    // set the link device at the link index
    memcpy(&platform_driver_priv->devices.link_devices[link_index], &link_device, sizeof(struct mtip_link_device_info));

    spin_unlock_irqrestore(lock, flags);

    CSMLOGDBG("done with processing link device: %lx", (unsigned long)pdev);

    // check if all probes have been received
    mtip_platform_check_if_all_probes_received();

    return ret;
}

int mtip_lane_probe(struct platform_device *pdev) 
{
    int ret = 0;
    int result = 0;
    struct mtip_lane_device_info lane_device;
    unsigned long flags;
    spinlock_t *lock = &platform_driver_priv->driver_lock;
    u32 lane_index;
    u32 sfp_phandle;

    CSMLOGDBG("mtip_lane_probe called of device \"%s\"\n", pdev->name);

    lane_device.lane_pdev = pdev;
    lane_device.lane_phandle = pdev->dev.of_node->phandle;

    CSMLOGDBG("phandle of the lane device: %d\n", lane_device.lane_phandle);

    /* Get the lane index */
    result	= of_property_read_u32(pdev->dev.of_node, "qcom,mac-lane-index", &lane_index);

    if (result < 0) {
        CSMLOGERR(":get resource failed for qcom,mac-lane-index\n");
        return ret;
   }

    lane_device.lane_index = lane_index;
    CSMLOGDBG("qcom,mac-lane-index is %d\n", lane_device.lane_index);

    // read the sfp phandle
    if (of_property_read_u32_index(pdev->dev.of_node, "sfp", 0, &sfp_phandle) >= 0) {
        CSMLOGDBG("Lane found sfp_phandle: %d\n", sfp_phandle);
        lane_device.sfp_phandle = sfp_phandle;
    } else {
        CSMLOGDBG("Lane failed to find sfp_phandle\n");
        lane_device.sfp_phandle = -1;
    }

    // set this as a valid lane device
    lane_device.lane_device_valid = 1;

    // set default lane down reason code
    lane_device.reason_code = TRX_ERROR;

    // the DT entries have been processed
    spin_lock_irqsave(lock, flags);

    // set the lane device at lane index
    memcpy(&platform_driver_priv->devices.lane_devices[lane_index], &lane_device, sizeof(struct mtip_lane_device_info));

   spin_unlock_irqrestore(lock, flags);

    CSMLOGDBG("done with processing link device: %lx", (unsigned long)pdev);

    // check if all probes have been received
    mtip_platform_check_if_all_probes_received();

    return ret;
}

int mtip_port_probe(struct platform_device *pdev) 
{
    int ret = 0;
    int i;
    int 			result = 0;
    u32             phandle;
    u32             port_type;
    struct mtip_port_device_info port_device;
    struct mtip_port_device_info *tmp_port_device;
    struct resource *wrapper_resource;
    struct resource *irq_resource;
    int             link_entries;
    int             lane_entries;
    unsigned long flags;
    spinlock_t *lock = &platform_driver_priv->driver_lock;
    struct resource dev_resource;
    u32             dut_base_regs[2];

    memset(&port_device, 0, sizeof(struct mtip_port_device_info));
    CSMLOGDBG("mtip_port_probe called of device \"%s\"\n", pdev->name);

    port_device.port_pdev = pdev;

    port_device.port_phandle = pdev->dev.of_node->phandle;
    CSMLOGDBG("phandle of the port device: %d\n", port_device.port_phandle);

    /* Get the port type */
    result = of_property_read_u32(pdev->dev.of_node, "qcom,port-type", &port_type);

    if (result < 0) {
        CSMLOGERR(":get resource failed for port-type\n");
        return -ENODEV;
    }
    if(port_type < MTIP_PORT_TYPE_FH_0 || port_type >= MTIP_PORT_TYPE_MAX) {
        CSMLOGERR("Invalid port-type\n");
        return -EINVAL;
    }
    // copy the port type
    port_device.port_type = port_type;
   CSMLOGINFO("port type is %d\n", port_device.port_type);

   // mac wrapper base address
   wrapper_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mac-wrapper");
   if (!wrapper_resource) {
       CSMLOGERR(":get resource failed for mac-wrapper\n");
       return -ENODEV;
   }
   CSMLOGDBG("port type: %d, port base = 0x%x, port size = 0x%x\n", port_device.port_type, wrapper_resource->start, resource_size(wrapper_resource));

   // set the base address
   port_device.wrapper_base_addr = devm_ioremap_resource(&pdev->dev, wrapper_resource);

   // macstats base address
   wrapper_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "macstats");
    if (!wrapper_resource) 
    {
       CSMLOGERR(":get resource failed for macstats\n");
       return -ENODEV;
   }
   CSMLOGDBG("port type: %d, port base = 0x%x, port size = 0x%x\n", port_device.port_type, wrapper_resource->start, resource_size(wrapper_resource));

   // set the base address
   port_device.macstats_base_addr = devm_ioremap_resource(&pdev->dev, wrapper_resource);

    if (mtip_rumi_platform == MTIP_PLATFORM_SOC) 
   {
       // rsfec base address
       wrapper_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rsfec");
       if (!wrapper_resource) {
           CSMLOGERR(":get resource failed for rsfec\n");
           return -ENODEV;
       }
       CSMLOGDBG("port type: %d, port base = 0x%x, port size = 0x%x\n", port_device.port_type, wrapper_resource->start, resource_size(wrapper_resource));

       // set the base address
       port_device.rsfec_base_addr = devm_ioremap_resource(&pdev->dev, wrapper_resource);
   }

   // get the irq resource
   irq_resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "pcs-mac-irq");
   if (!irq_resource) {
       CSMLOGERR(":get resource failed for irq 0\n");
       return -ENODEV;
   }
   CSMLOGDBG("port type: %d, :irq = %d\n", port_device.port_type, irq_resource->start);

   // set the irq
   port_device.wrapper_irq = irq_resource->start;

    if (mtip_rumi_platform != MTIP_PLATFORM_SOC) 
   {
       // read the dut_base_regs
       ret = of_property_read_u32_array(pdev->dev.of_node, "qcom,dut-base-reg", dut_base_regs, 2);
       if (ret < 0) {
            CSMLOGERR("dut_base_regs[0] : %x, dut_base_regs[1] = %x, ret: %d\n", dut_base_regs[0], dut_base_regs[1], ret);
           return -ENODEV;
       }

       dev_resource.start = dut_base_regs[0];
       dev_resource.end = dut_base_regs[0] + dut_base_regs[1];
       dev_resource.flags = IORESOURCE_MEM;
       dev_resource.parent = dev_resource.child = dev_resource.sibling = NULL;

       // set the dut base address
       port_device.dut_base_addr = devm_ioremap_resource(&pdev->dev, &dev_resource);

       CSMLOGDBG("ioremap of resource done: 0x%lx\n", port_device.dut_base_addr);
   }

   // get the number of links
    if (!of_get_property(pdev->dev.of_node, "qcom,mac-port-link-references", &link_entries)) {
       CSMLOGERR("Port no references to links");
       return -ENODEV;
   }

    port_device.num_link_phandles = link_entries / (sizeof(u32));

    if (port_device.num_link_phandles > MTIP_MAX_LINKS_PER_PORT) {
       CSMLOGERR("Num phandle references %d exceeds MAX\n", port_device.num_link_phandles);
       return -ENODEV;
   }

    if (port_device.num_link_phandles == 0) {
       CSMLOGERR("Num phandle references %d is 0!\n", port_device.num_link_phandles);
       return -ENODEV;
   }

   CSMLOGINFO("Port link entries: %d, num_phandles: %d\n", link_entries, port_device.num_link_phandles);

    for (i = 0; i < port_device.num_link_phandles; ++i) {
        if (of_property_read_u32_index(pdev->dev.of_node, "qcom,mac-port-link-references", i, &phandle) >= 0) {
           CSMLOGDBG("Port found phandle: %d for index: %d\n", phandle, i);
           port_device.link_phandles[i] = phandle;
        } else {
           CSMLOGERR("Port failed to find phandle for index: %d", i);
           return -ENODEV;
       }
   }

    // get the number of lanes
    if (!of_get_property(pdev->dev.of_node, "qcom,mac-port-lane-references", &lane_entries)) {
        CSMLOGERR("Port no references to lanes");
        return -ENODEV;
    }

    port_device.num_lane_phandles = lane_entries / (sizeof(u32));

    if (port_device.num_lane_phandles > MTIP_MAX_LANES_PER_PORT) {
        CSMLOGERR("Num phandle references %d exceeds MAX\n", port_device.num_lane_phandles);
        return -ENODEV;
       }

    if (port_device.num_lane_phandles == 0) {
        CSMLOGERR("Num phandle references %d is 0!\n", port_device.num_lane_phandles);
        return -ENODEV;
   }

    CSMLOGINFO("Port lane entries: %d, num_phandles: %d\n", lane_entries, port_device.num_lane_phandles);

    for (i = 0; i < port_device.num_lane_phandles; ++i) {
        if (of_property_read_u32_index(pdev->dev.of_node, "qcom,mac-port-lane-references", i, &phandle) >= 0) {
            CSMLOGDBG("Port found phandle: %d for index: %d\n", phandle, i);
            port_device.lane_phandles[i] = phandle;
        } else {
            CSMLOGERR("Port failed to find phandle for index: %d", i);
            return -ENODEV;
        }
    }

    // all the entries of the port device have been processed
    port_device.port_device_valid = 1;

   spin_lock_irqsave(lock, flags);

    // copy the port device based on port_type
    tmp_port_device = &platform_driver_priv->devices.port_devices[port_type];
    tmp_port_device->port_pdev = port_device.port_pdev;
    tmp_port_device->port_phandle = port_device.port_phandle;
    tmp_port_device->port_type = port_device.port_type;
    tmp_port_device->wrapper_base_addr = port_device.wrapper_base_addr;
    tmp_port_device->macstats_base_addr = port_device.macstats_base_addr;
    tmp_port_device->rsfec_base_addr = port_device.rsfec_base_addr;
    tmp_port_device->wrapper_irq = port_device.wrapper_irq;
    tmp_port_device->dut_base_addr = port_device.dut_base_addr;
    tmp_port_device->num_link_phandles = port_device.num_link_phandles;
    for (i = 0; i < port_device.num_link_phandles; ++i) {
        tmp_port_device->link_phandles[i] = port_device.link_phandles[i];
    }
    tmp_port_device->num_lane_phandles = port_device.num_lane_phandles;
    for (i = 0; i < port_device.num_lane_phandles; ++i) {
        tmp_port_device->lane_phandles[i] = port_device.lane_phandles[i];
    }
    tmp_port_device->port_device_valid = port_device.port_device_valid; 

   spin_unlock_irqrestore(lock, flags);

    CSMLOGDBG("done with processing port device: %lx port_type %d", (unsigned long)pdev, port_type);

    // check if all probes have been received
    mtip_platform_check_if_all_probes_received();

    return ret;
}

int mtip_platform_probe(struct platform_device *pdev) 
{
    int ret = 0;
    int i;
    int result = 0;
    int mode;
    int port_entries;
    int port_count;
    u32 phandle;
    u32 fuse_bit_offset;

    CSMLOGDBG("mtip_platform_probe called for device: \"%s\"\n", pdev->name);

    /* Get the device mode */
    result	= of_property_read_u32(pdev->dev.of_node, "qcom,device-mode", &mode);

    if (result < 0) {
        CSMLOGERR(":get resource failed for device-mode\n");
        return -ENODEV;
    }

    switch (mode) 
    {
    case MTIP_DEVICE_RU:
        {
            CSMLOGERR("device-mode is RU");
        }
        break;
    case MTIP_DEVICE_DU:
        {
            CSMLOGERR("device-mode is DU");
        }
        break;
    case MTIP_DEVICE_RUv2:
        {
            CSMLOGERR("device-mode is RUv2");
        }
        break;
    case MTIP_DEVICE_DUv2:
        {
            CSMLOGERR("device-mode is DUv2");
        }
        break;
    default:
        {
            CSMLOGERR("device-mode is Unknown!");
        }
        break;
    }

    platform_driver_priv->devices.mode = mode;

    if (mtip_rumi_platform == MTIP_PLATFORM_SOC) 
    {
        // read the MAC address fuse bit offset
        result = of_property_read_u32(pdev->dev.of_node, "qcom,mac-address-bit-offset", &fuse_bit_offset);

        if (result < 0) {
            CSMLOGERR("Unable to read mac-address-bit-offset, result: %d\n", result);
            return -ENODEV;
        }
        CSMLOGDBG("fuse_bit_offset = %d, ret: %d\n", fuse_bit_offset, result);

        // set the fuse bit offset
        platform_driver_priv->devices.fuse_bit_offset = fuse_bit_offset;
    }

    // copy the platform device pointer
    platform_driver_priv->devices.root_pdev = pdev;

    // get the number of ports
    if (!of_get_property(pdev->dev.of_node, "qcom,mac-port-references", &port_entries)) {
        CSMLOGERR("Platform no references to ports");
        return -ENODEV;
    }

    port_count = port_entries / (sizeof(u32));

    CSMLOGINFO("Platform port entries: %d, port count: %d\n", port_entries, port_count);

    platform_driver_priv->devices.num_port_phandles = port_count;

    if (port_count > MTIP_MAX_PORTS) {
        CSMLOGERR("Number of port phandles exceeds MAX");
        return -ENODEV;
    }

    for (i = 0; i < port_count; ++i) {
        if (of_property_read_u32_index(pdev->dev.of_node, "qcom,mac-port-references", i, &phandle) >= 0) {
            CSMLOGDBG("Platform found phandle: %d for index: %d\n", phandle, i);
            platform_driver_priv->devices.port_phandles[i] = phandle;
        } else {
            CSMLOGERR("Platform failed to find phandle for index: %d", i);
            return -ENODEV;
        }
    }

    // set the platform_device_valid
    platform_driver_priv->devices.platform_device_valid = 1;

    CSMLOGDBG("done with processing platform device: %lx", (unsigned long)pdev);

    // check if all probes have been received
    mtip_platform_check_if_all_probes_received();

    return ret;
}

static int mtip_platform_set_mac_addresses_for_rumi(void)
{
    uint8_t saddr[6];
    int i;
    u32 oui;
    u32 nic;
    
    CSMLOGDBG("Setting default MAC addresses on RUMI\n");

    // set the oui and nic values in little endian order
    oui = 0x00534E; 
    nic = 0x554C30; 

    for (i = 0; i < MTIP_MAX_LINKS; ++i) 
    {
        // for each valid link
        if (platform_driver_priv->mtip_links[i] != NULL) 
        {
            saddr[0] = (oui >> 16) & 0xFF;
            saddr[1] = (oui >> 8) & 0xFF;
            saddr[2] = (oui) & 0xFF;

            saddr[3] = (nic >> 16) & 0xFF;
            saddr[4] = (nic >> 8) & 0xFF;
            saddr[5] = (nic) & 0xFF;

            mtip_mac_set_mac_address_by_link_index(i, saddr);

            // increment the lower bits
            ++nic;
        }
    }
    return 0;
}

static u8 mtip_platform_read_fuse_mac_info_version(void)
{
    u8 fuse_bit_offset;
    u64 first_fuse_word;
    u8 version;
    struct nvmem_cell *cell;
    u64 *buf;
    size_t len;

    CSMLOGDBG("mtip_platform_read_fuse_mac_info_version");
    // the start address is
    fuse_bit_offset = platform_driver_priv->devices.fuse_bit_offset;

    cell = nvmem_cell_get(&((platform_driver_priv->devices.root_pdev)->dev), "mac_fuse");
    if (IS_ERR(cell)){
      CSMLOGERR("%s:Unable to get mac_fuse from devicetree \n",__func__);
      return -EINVAL;
    }

   buf = (u64 *)nvmem_cell_read(cell, &len);
   CSMLOGDBG("len = %d\n",len);

   if (IS_ERR(buf) || (len != 16)){
     nvmem_cell_put(cell);
     if(!IS_ERR(buf)){
       kfree(buf);
     }
     CSMLOGERR("%s: Unable to read mac_fuse value \n",__func__);
     return -EINVAL;
   }

   // read the first 64 bits
   first_fuse_word = buf[0];

   // shift the fuse word by fuse_bit_offset
   first_fuse_word = (first_fuse_word >> fuse_bit_offset);

   // the version will be the first three bits
   version = (u8)((first_fuse_word) & 0x7);

   kfree(buf);
   nvmem_cell_put(cell);

   CSMLOGINFO("FUSE version: %d\n", version);

   return version;
}

static int mtip_platform_read_fuse_version1_info(u32* oui,
                                                  u32* start_nic,
                                                  u8* num_macs,
                                                  u32* start_secondary_nic,
                                                  u8* num_secondary_macs)
{
    u8 fuse_bit_offset;
    u64 first_fuse_word;
    u64 second_fuse_word;
    struct nvmem_cell *cell;
    u64 *buf;
    size_t len;

    // set as default
    *num_macs = 0;
    *num_secondary_macs = 0;
    CSMLOGDBG("mtip_platform_read_fuse_version1_info called");

    // the start address is
    fuse_bit_offset = platform_driver_priv->devices.fuse_bit_offset;

    // version 1 will always start at fuse_bit_offset = 0
    if (fuse_bit_offset != 0) 
    {
        CSMLOGERR("Version 1 fuse bit offset has to be 0 not %d\n", fuse_bit_offset);
        return -1;
    }

    cell = nvmem_cell_get(&((platform_driver_priv->devices.root_pdev)->dev), "mac_fuse");
    if (IS_ERR(cell)){
      CSMLOGERR("%s: Unable to get mac_fuse from devicetree \n",__func__);
      return -EINVAL;
    }

    buf = (u64 *)nvmem_cell_read(cell, &len);
    CSMLOGDBG("len = %d\n",len);
	
    if (IS_ERR(buf) || (len != 16)){
      nvmem_cell_put(cell);
      if(!IS_ERR(buf)){
        kfree(buf);
      }
      CSMLOGERR("%s: Unable to read mac_fuse value \n",__func__);
      return -EINVAL;
    }

    // read the first 64 bits
    first_fuse_word = buf[0];

    // shift by 3 bits for version
    first_fuse_word = (first_fuse_word >> 3);

    // set the oui
    *oui = (u32)(first_fuse_word & 0xFFFFFF);

    CSMLOGDBG("VENDOR OUI is 0x%x\n", *oui);

    // shift by 24 bits
    first_fuse_word = (first_fuse_word >> 24);

    // set the start_nic
    *start_nic = (u32)(first_fuse_word & 0xFFFFFF);

    CSMLOGDBG("MAC Address Start Offset is 0x%x\n", *start_nic);

    // shift by 24 bits
    first_fuse_word = (first_fuse_word >> 24);

    // set the num_macs
    *num_macs = (u8)(first_fuse_word & 0x1F);

    CSMLOGDBG("Number of MAC addresses is %d\n", *num_macs);

    kfree(buf);
    nvmem_cell_put(cell);

    // read the second 64 bits

    cell = nvmem_cell_get(&((platform_driver_priv->devices.root_pdev)->dev), "mac_fuse_second");
    if (IS_ERR(cell)){
      CSMLOGERR("%s: Unable to get mac_fuse_second from devicetree \n",__func__);
      return -EINVAL;
    }

    buf = (u64 *)nvmem_cell_read(cell, &len);
    CSMLOGDBG("len = %d\n",len);
	
    if (IS_ERR(buf) || (len != 8)){
      nvmem_cell_put(cell);
      if(!IS_ERR(buf)){
       kfree(buf);
      }
     CSMLOGERR("%s: Unable to read mac_fuse_second value \n",__func__);
     return -EINVAL;
    }

    second_fuse_word = buf[0];

    // set the secondary nic
    *start_secondary_nic = (u32)(second_fuse_word & 0xFFFFFF);

    CSMLOGDBG("Secondary MAC address offset: 0x%x\n", *start_secondary_nic);

    // shift by 24 bits
    second_fuse_word = (second_fuse_word >> 24);

    // set the num secondary macs
    *num_secondary_macs = (u8)(second_fuse_word & 0x1F);

    CSMLOGDBG("Num secondary MAC Addresses: %d\n", *num_secondary_macs);

    kfree(buf);
    nvmem_cell_put(cell);

    return 0;
}

static int mtip_platform_set_mac_addresses(void)
{
    uint8_t saddr[6];
    int i;
    u32 oui;
    u32 start_nic;
    u8 no_of_macs = 0;
    u32 start_secondary_nic;
    u8 num_secondary_macs = 0;
    u8 version;

    version = mtip_platform_read_fuse_mac_info_version();

    // for now we only support version = 0x1
    if (version != 0x1) 
    {
        CSMLOGERR("Unsupported FUSE MAC INFO version number: %d\n", version);

        CSMLOGDBG("Setting default MAC addresses\n");

        // the oui and nic are in little endian order
        oui = 0x00534E; // EXAMPLE
        start_nic = 0x554C30; // EXAMPLE

        // set the MAC address for the other interfaces
        for (i = 0; i < MTIP_MAX_LINKS; ++i) 
        {
            // for each valid link
            if (platform_driver_priv->mtip_links[i] != NULL) 
            {
                saddr[0] = (oui >> 16) & 0xFF;
                saddr[1] = (oui >> 8) & 0xFF;
                saddr[2] = (oui) & 0xFF;

                saddr[3] = (start_nic >> 16) & 0xFF;
                saddr[4] = (start_nic >> 8) & 0xFF;
                saddr[5] = (start_nic) & 0xFF;

                mtip_mac_set_mac_address_by_link_index(i, saddr);
            }

            // increment the lower bits
            ++start_nic;
        }
    }
    else
    {
        // read the remaining fields of fuse
        // read the fuse and update the oui, nic and no of macs
        mtip_platform_read_fuse_version1_info(&oui, &start_nic, &no_of_macs, &start_secondary_nic, &num_secondary_macs);

        // check that we have sufficient addresses
        if (no_of_macs < 16)
        {
            CSMLOGERR("Number of MAC addresses in fuse: %d is less than required: %d\n", no_of_macs, 16);
        }

        // first set the MAC address of Debug Ethernet if presennt
        i = MTIP_MAX_LINKS - 1;

        if (platform_driver_priv->mtip_links[i] != NULL) 
        {
            saddr[0] = (oui >> 16) & 0xFF;
            saddr[1] = (oui >> 8) & 0xFF;
            saddr[2] = (oui) & 0xFF;

            saddr[3] = (start_nic >> 16) & 0xFF;
            saddr[4] = (start_nic >> 8) & 0xFF;
            saddr[5] = (start_nic) & 0xFF;

            mtip_mac_set_mac_address_by_link_index(i, saddr);

            // increment the MAC OFFSET
            ++start_nic;
        }

        // set the MAC address for the other interfaces
        for (i = 0; i < MTIP_MAX_LINKS - 1; ++i) 
        {
            // for each valid link
            if (platform_driver_priv->mtip_links[i] != NULL) 
            {
                saddr[0] = (oui >> 16) & 0xFF;
                saddr[1] = (oui >> 8) & 0xFF;
                saddr[2] = (oui) & 0xFF;

                saddr[3] = (start_nic >> 16) & 0xFF;
                saddr[4] = (start_nic >> 8) & 0xFF;
                saddr[5] = (start_nic) & 0xFF;

                mtip_mac_set_mac_address_by_link_index(i, saddr);

               // increment the lower bits
               ++start_nic;
            }
        }
    }

    return 0;
}

int mtip_platform_setup_ethernet(unsigned int port_type)
{
   int i;
   int result;
   int ret = 0;
   struct mtip_netdev_priv *priv;
   u32 link_port_type;

   CSMLOGDBG("Setting up ethernet for port_type %d", port_type);

   if (mtip_rumi_platform != MTIP_PLATFORM_SOC)
   {
      // Reset the EMULATION DUT ONLY FOR RUMI
      CSMLOGDBG("Reseting the FH emulation at index: %d\n", port_type);

      // reset the FH emulation
      mtip_dut_reset(platform_driver_priv->devices.port_devices[port_type].dut_base_addr);
   }
   else
   {
      // initialize the RSFEC, SETUP PHY and PHYLINK of the ports
      CSMLOGDBG("Initializing RSFEC and PHY for port: %d\n", port_type);

      // initialize the RSFEC of the port
      mtip_rsfec_initialize(&platform_driver_priv->devices.port_devices[port_type]);

      // setup the phy of the port
      mtip_phy_setup_phy(&platform_driver_priv->devices.port_devices[port_type]);
   }

   // Initialize the MAC WRAPPER
   CSMLOGDBG("Initializing MAC port at index: %d\n", port_type);

   // MAC wrapper Init
   mtip_mac_wrapper_init(&platform_driver_priv->devices.port_devices[port_type]);

   if (mtip_rumi_platform == MTIP_PLATFORM_SOC)
   {
      // set the mac wrapper pcs mode control
      mtip_mac_wrapper_pcs_mode_control(&platform_driver_priv->devices.port_devices[port_type]);
   }

   // setup the net devices
   for (i = 0; i < MTIP_MAX_LINKS; ++i)
   {
      // for each valid link
      if (platform_driver_priv->devices.link_devices[i].link_device_valid != 0)
      {
         // look up the link port type
         if (mtip_lookup_port_type_by_link_index(i, &link_port_type) < 0)
         {
            CSMLOGDBG("invalid port_type for link_index %d", i);
         }
         else
         {
            // check for the link object
            if (platform_driver_priv->mtip_links[i] != NULL)
            {
               // make sure there is at least one lane assigned to the link
               if (platform_driver_priv->mtip_links[i]->num_assigned_lanes != 0)
               {
                  if (port_type == link_port_type)
                  {
                     priv = netdev_priv(platform_driver_priv->mtip_links[i]->dev);

                     // Initialize the MAC block
                     mtip_mac_initialize(priv);

                     if (mtip_rumi_platform != MTIP_PLATFORM_SOC)
                     {
                        // setup loopback if needed
                        if (mtip_loopback_mode != MTIP_MODE_DEFAULT)
                        {
                           // enable IOMACRO loopback
                           mtip_dut_enable_rgmii_loopback(i);
                        }
                        else
                        {
                           // MDIO registration
                           result = mtip_mdio_register(platform_driver_priv->mtip_links[i]->dev,
                                                       platform_driver_priv->devices.link_devices[i].link_pdev->dev.of_node);
                           if (result)
                           {
                              CSMLOGERR("MDIO registration failed with err %d", result);
                           }

                           CSMLOGDBG("TX delay = %d, RX delay = %d", mtip_dut_get_tx_delay(i), mtip_dut_get_rx_delay(i));
                        }
                     }
                     else
                     {
                        // this is the default for the target
                        // initialize the PCS for the link
                        mtip_pcs_config_pcs(i);

                        if ( (i != MTIP_L2_ETH_LINK_INDEX && i != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode == MTIP_MODE_LOOPBACK) || ( (i == MTIP_L2_ETH_LINK_INDEX || i == MTIP_C2C1_ETH_LINK_INDEX) && mtip_c2c2_loopback_mode == MTIP_MODE_C2C2_LOOPBACK))
                        {
                           // enable pcs loopback on the link
                           mtip_pcs_enable_loopback(i);
                        }

                        if (mtip_rumi_platform == MTIP_PLATFORM_SOC)
                        {
                           // set the MAC interrupt mask
                           mtip_mac_set_interrupt_mask(i);
                        }
                     }
                  }
               }
            }
            else
            {
               // clear the interrupt mask of unused link
               if (port_type == link_port_type)
               {
                  if (mtip_rumi_platform == MTIP_PLATFORM_SOC)
                  {
                     // clear the MAC interrupt mask
                     mtip_mac_clear_interrupt_mask(i);
                  }
               }
            }
         }
      }
   }

   return ret;
}


/**
 * mtip_platform_setup
 */
static int mtip_platform_setup(void)
{
   int i, j = 0;
   int result;
   int ret = 0;
   struct net_device *netdev = NULL;
   struct mtip_netdev_priv *priv;
   u32 total_num_links = 0;
   u32 port_type;

   // enable all the necessary clocks
   mtip_clocks_setup_clocks();

   // setup the port info data
   for (i = 0; i < MTIP_MAX_PORTS; ++i)
   {
      if (platform_driver_priv->devices.port_devices[i].port_device_valid != 0)
      {
          port_type = platform_driver_priv->devices.port_devices[i].port_type;

          // setup the port
          mtip_platform_setup_port(port_type);

         // set the clock rates based on the default port config
         mtip_clocks_set_clock_rates(port_type, platform_driver_priv->mtip_ports[port_type]->port_config);
      }
   }

   // calculate the total number of active links across all ports
   total_num_links = 0;

   for (i = 0; i < MTIP_MAX_PORTS; ++i)
   {
       if (platform_driver_priv->devices.port_devices[i].port_device_valid) 
       {
           total_num_links += platform_driver_priv->devices.port_devices[i].num_link_phandles;
       }
   }

   CSMLOGDBG("Setting up %d links\n", total_num_links);

   for (i = 0; i < MTIP_MAX_PORTS; ++i)
   {
      // allocate the mtip_links and connect to dma
      for (j = 0; (j < platform_driver_priv->devices.port_devices[i].num_link_phandles) && (j < MTIP_MAX_LINKS_PER_PORT); ++j)
      {
         // check if this is a valid link
         if (platform_driver_priv->devices.port_devices[i].link_devices[j]->link_device_valid != 0)
         {
            // setup the link for port: i and link number: j
            // setup the link info data structures
            // initialize and connect to dma
            ret = mtip_platform_setup_link(i, j);

            // HANDLE THE ERROR
            if (ret < 0)
            {
               CSMLOGERR("link setup failed for port_device_index: %d and link number: %d\n", i, j);
               goto cleanup;
            }
         }
      }

      // allocate the mtip_lanes
      for (j = 0; (j < platform_driver_priv->devices.port_devices[i].num_lane_phandles) && (j < MTIP_MAX_LANES_PER_PORT); ++j)
      {
         // check if this is a valid lane`
         if (platform_driver_priv->devices.port_devices[i].lane_devices[j]->lane_device_valid != 0)
         {
            // setup the lane for port: i and lane number: j
            // setup the lane info data structure
            ret = mtip_platform_setup_lane(i, j);

            // HANDLE THE ERROR
            if (ret < 0)
            {
               CSMLOGERR("lane setup failed for port_device_index: %d and lane number: %d\n", i, j);
               goto cleanup;
            }
         }
      }
   }

   // allocate the net device structures
   for (i = 0; i < MTIP_MAX_LINKS; ++i)
   {
      if (mtip_lookup_port_type_by_link_index(i, &port_type) < 0)
      {
         CSMLOGDBG("invalid port_type for link_index %d", i);
      }
      else
      {
         // for each valid link
         if (platform_driver_priv->mtip_links[i] != NULL)
         {

            // alloc the netdev
            platform_driver_priv->mtip_links[i]->dev = alloc_netdev(sizeof(struct mtip_netdev_priv),
                                                                    platform_driver_priv->devices.link_devices[i].link_name,
                                                                    NET_NAME_ENUM,
                                                                    mtip_netdevice_init);

            priv = netdev_priv(platform_driver_priv->mtip_links[i]->dev);

            netdev = platform_driver_priv->mtip_links[i]->dev;

            if (( i != MTIP_L2_ETH_LINK_INDEX && i != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode == MTIP_MODE_DEFAULT) || ( (i == MTIP_L2_ETH_LINK_INDEX || i == MTIP_C2C1_ETH_LINK_INDEX) && mtip_c2c2_loopback_mode == MTIP_MODE_DEFAULT))
            {
               // the supported features and hw features
               netdev->hw_features = 0;
               netdev->features = netdev->hw_features | NETIF_F_HIGHDMA;
               netdev->vlan_features = 0;
            }
            else
            {
               // for loopback set the features
               netdev->features = NETIF_F_HW_CSUM | NETIF_F_RXCSUM;
               netdev->hw_features = netdev->features;
               netdev->vlan_features |= NETIF_F_HW_CSUM;
            }

            /* MTU range: 46 - 9194 */
            netdev->min_mtu = MTIP_MAC_MIN_ETH_FRAME_SIZE -
               (ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN);
            netdev->max_mtu = MTIP_MAC_MAX_ETH_FRAME_SIZE -
               (ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN);

            priv->link_index = i;
            if (mtip_loopback_mode != MTIP_MODE_DEFAULT && !mtip_loopback_enable_arp && priv->link_index != MTIP_L2_ETH_LINK_INDEX && priv->link_index != MTIP_C2C1_ETH_LINK_INDEX)
            {
               CSMLOGERR("header ops registered for link_index : %d\n",priv->link_index);
               netdev->header_ops = &mtip_header_ops;
            }
            priv->hashtablebits = 0;

            // set the port priv flags as the default
            priv->priv_flags = platform_driver_priv->mtip_ports[port_type]->port_priv_flags;

            // mark the priv flags as the default
            priv->priv_flags_set = false;

            priv->rx_polled_count = 0;
            priv->head = NULL;
            priv->rx_curr_index = 0;
            // Set up link between ndev and pdev
            SET_NETDEV_DEV(platform_driver_priv->mtip_links[i]->dev, &platform_driver_priv->devices.link_devices[i].link_pdev->dev);

            priv->mac_ioaddr = platform_driver_priv->devices.link_devices[i].mac_ioaddr;

          // add the mtip_napi_rx
          // this needs to be done before register netdev
          netif_napi_add(platform_driver_priv->mtip_links[i]->dev, &(platform_driver_priv->mtip_links[i]->napi), mtip_napi_poll, MTIP_NAPI_WEIGHT);
          netif_napi_add(platform_driver_priv->mtip_links[i]->dev, &(platform_driver_priv->mtip_links[i]->napi_tx), mtip_napi_poll_tx, MTIP_NAPI_WEIGHT);
            CSMLOGDBG("dev = 0x%lx with link_index = %d",
                      (unsigned long)platform_driver_priv->mtip_links[i]->dev,
                      priv->link_index);
         }
      }
   }

   if (mtip_rumi_platform == MTIP_PLATFORM_SOC)
   {
      // program the MAC address of all the links by reading the fuse registers
      mtip_platform_set_mac_addresses();
   }
   else
   {
      // set default mac addresses
      mtip_platform_set_mac_addresses_for_rumi();
   }

   // register the net devices
   for (i = 0; i < MTIP_MAX_LINKS; ++i)
   {
      if (platform_driver_priv->mtip_links[i] != NULL)
      {

         // register the netdev
         result = register_netdev(platform_driver_priv->mtip_links[i]->dev);

         if (result)
         {
            CSMLOGERR("mtip: error %i for device \"%s\"\n", result, platform_driver_priv->mtip_links[i]->dev->name);
         }
         else
         {
            ret = 0;

            CSMLOGDBG("mtip: register netdev complete for \"%s\"\n", platform_driver_priv->mtip_links[i]->dev->name);

            // set the netdev MAC address from the HW
            mtip_set_netdev_hw_mac_addr(platform_driver_priv->mtip_links[i]->dev, i);
         }
      }
   }

   // register the IRQ handlers
   for (i = 0; i < MTIP_MAX_PORTS; ++i)
   {
      if (platform_driver_priv->devices.port_devices[i].port_device_valid)
      {
         // register for MAC wrapper IRQ
         mtip_mac_wrapper_register_irq(&platform_driver_priv->devices.port_devices[i].port_pdev->dev,
                                       platform_driver_priv->devices.port_devices[i].wrapper_irq,
                                       DRV_NAME,
                                       (void *)&platform_driver_priv->devices.port_devices[i]);
      }
   }

   // handle the special case of loopback
   // we are doing some sort of loopback
   // assign lanes to links and set mode to 4x25GBASE_R
   for (i = 0; i < MTIP_MAX_LINKS; ++i)
   {
      if (platform_driver_priv->mtip_links[i] != NULL)
      {
         if( ((i == MTIP_L2_ETH_LINK_INDEX || i == MTIP_C2C1_ETH_LINK_INDEX) && mtip_c2c2_loopback_mode != MTIP_MODE_DEFAULT) || (i != MTIP_L2_ETH_LINK_INDEX && i != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode != MTIP_MODE_DEFAULT))
         {
            // Ensure proper lane assignment for all links
            platform_driver_priv->mtip_links[i]->num_assigned_lanes = 1;

            // set the lane_index to be the same as link_index
            platform_driver_priv->mtip_links[i]->assigned_lane_indices[0] = i;

            if (mtip_lookup_port_type_by_link_index(i, &port_type) < 0)
            {
               CSMLOGERR("invalid port_type for link_index %d", i);
               return -1;
            }

            // setup the ports for loopback
            // set the port state as connected
            platform_driver_priv->mtip_ports[port_type]->port_state = MTIP_PORT_STATE_CONNECTED;

            if(port_type == MTIP_PORT_TYPE_L2)
            {
               // set the below port priv flags supported for L2 port
               // 1x100GBASE_R2, 1x50GBASE_R, 1x50GBASE_R2, 1x25GBASE_R, 1x10GBASE_R
               platform_driver_priv->mtip_ports[port_type]->port_priv_flags = (1 << MTIP_PORT_CONFIG_1x25GBASE_R);
            }
            else
            {
               //For 4x25GBASE_R mode, ensure all 4 links are supported
               platform_driver_priv->mtip_ports[port_type]->port_priv_flags = (1 << MTIP_PORT_CONFIG_4x25GBASE_R);

               // set the port config as 4x25GBASE_R
               platform_driver_priv->mtip_ports[port_type]->port_config = MTIP_PORT_CONFIG_4x25GBASE_R;
            }
            // set the port sfp as DAC
            platform_driver_priv->mtip_ports[port_type]->sfp_port_type = PORT_DA;

            // Configure all 4 lanes for the port
            for (j = 0; j < PHY_LANE_MAX; ++j)
            {
               platform_driver_priv->mtip_ports[port_type]->lane_config[j].lane_enabled = true;
               platform_driver_priv->mtip_ports[port_type]->lane_config[j].lane_speed = PHY_LANE_SPEED_25G;
               platform_driver_priv->mtip_ports[port_type]->lane_config[j].link_index = (port_type*PHY_LANE_MAX) + j;
            }

            CSMLOGERR("LOOPBACK: Configured link_index %d with lane %d for port_type %d", i, i, port_type);
         }
      }
   }

   // set up the lanes for loopback
   for (i = 0; i < MTIP_MAX_LANES; ++i)
   {
      if (platform_driver_priv->mtip_lanes[i] != NULL)
      {

         if( ( ( i == MTIP_L2_LANE1_INDEX || i == MTIP_L2_LANE2_INDEX ) && mtip_c2c2_loopback_mode != MTIP_MODE_DEFAULT) || ( ( i != MTIP_L2_LANE1_INDEX && i != MTIP_L2_LANE2_INDEX ) && mtip_loopback_mode != MTIP_MODE_DEFAULT))
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
            platform_driver_priv->mtip_lanes[i]->lane_qsfp_info.trx_laneinfo = 0xF;
            platform_driver_priv->mtip_lanes[i]->lane_qsfp_info.trx_bout_cfg = 0;
         }
      }
   }

   // set the ethtool ops for debug eth
   mtip_debug_eth_set_ethtool_ops();

   // the system topology is now setup using the device tree
   mtip_setup_topology();

   // create phylinks here
   post_mtip_process_create_phylink();

   // indicate readiness to any registered clients */
   post_mtip_client_send_ready();

   goto out;

cleanup:
   for (i = 0; i < MTIP_MAX_LINKS; ++i)
   {
      mtip_platform_cleanup_link(i);
   }

out:
   return ret;
}

void post_mtip_process_create_phylink(void)
{
   struct mtip_process_create_phylink_task* taskstruct = kmalloc(sizeof(struct mtip_process_create_phylink_task), GFP_ATOMIC);

   if(taskstruct == NULL)
   {
       CSMLOGERR("memory alloc failed\n");
       return;
   }

   taskstruct->value = 0;

   mtip_queue_work(MTIP_WORKQ_TASK_CREATE_PHYLINK, taskstruct, MTIP_PORT_TYPE_FH_0);
}

void run_mtip_process_create_phylink(void *work_ptr)
{
   struct mtip_process_create_phylink_task *taskstruct = (struct mtip_process_create_phylink_task *)work_ptr;
   u32 value = taskstruct->value;
   int i;

   (void)value;

   // create phylink for all the lane devices
   for (i = 0; i < MTIP_MAX_LANES; ++i)
   {
      if (platform_driver_priv->mtip_lanes[i] != NULL)
      {

         if( ( ( i == MTIP_L2_LANE1_INDEX || i == MTIP_L2_LANE2_INDEX ) && mtip_c2c2_loopback_mode == MTIP_MODE_DEFAULT) || ( ( i != MTIP_L2_LANE1_INDEX && i != MTIP_L2_LANE2_INDEX ) && mtip_loopback_mode == MTIP_MODE_DEFAULT))
         {
            if (platform_driver_priv->devices.lane_devices[i].lane_device_valid == 1)
            {
               // setup phylink for the lane
               mtip_phy_create_phylink(&platform_driver_priv->devices.lane_devices[i]);
            }
         }
      }
   }

   // free the taskstruct
   kfree(taskstruct);
}

int mtip_link_remove(struct platform_device *pdev)
{
   int i,j;
   struct mtip_netdev_priv *priv;
   struct ecpri_dma_pkt **rx_pkts = NULL;
   struct ecpri_dma_pkt_completion_wrapper **tx_comp_pkts = NULL;
   int num_pkt_allocs = MTIP_NAPI_WEIGHT*MTIP_RX_DMA_MAX_BUFFERS_PER_PACKET;
   struct ecpri_dma_pkt **tx_pkts = NULL;
   struct ecpri_dma_pkt_completion_wrapper **rx_comp_pkts = NULL;

   CSMLOGINFO("mtip_link_remove called\n");

   // free the net devices
   for (i = 0; i < MTIP_MAX_LINKS; ++i)
   {
      if (platform_driver_priv->mtip_links[i] != NULL)
      {

         if (platform_driver_priv->mtip_links[i]->dev != NULL)
         {
            if (!netif_queue_stopped(platform_driver_priv->mtip_links[i]->dev))
            {
               CSMLOGINFO("stopping queue for link_index %d", i);
               netif_stop_queue(platform_driver_priv->mtip_links[i]->dev);
            }
         }

         // cleanup the link
         mtip_platform_cleanup_link(i);

         if (platform_driver_priv->mtip_links[i]->dev != NULL)
         {
            priv = netdev_priv(platform_driver_priv->mtip_links[i]->dev);
            rx_pkts = priv->head;
            tx_comp_pkts = priv->tx_comp_pkts;
            tx_pkts = priv->tx_pkts;
            rx_comp_pkts = priv->rx_comp_pkts;
            priv->head = NULL;
            // free ptp lists
            mtip_ptp_finalize(i);
            // unregister the netdevs
            unregister_netdev(platform_driver_priv->mtip_links[i]->dev);
            // free the netdevs
            free_netdev(platform_driver_priv->mtip_links[i]->dev);
            platform_driver_priv->mtip_links[i]->dev = NULL;
         }

         mutex_destroy(&platform_driver_priv->mtip_links[i]->dev_lock);
         kfree(platform_driver_priv->mtip_links[i]);
         platform_driver_priv->mtip_links[i] = NULL;

         CSMLOGINFO("Deallocate TX completion wrappers\n");

         if(tx_comp_pkts != NULL)
         {
            for (j = 0; j < num_pkt_allocs; ++j)
            {
            // free the completion wrappers
            if(tx_comp_pkts[j])
               mtip_dma_free_completion_wrapper(tx_comp_pkts[j]);
            }
            kfree(tx_comp_pkts);
         }

         CSMLOGINFO("Deallocate RX completion wrappers\n");

         if(rx_comp_pkts != NULL)
         {
            for (j = 0; j < num_pkt_allocs; ++j)
            {
               // free the completion wrappers
               if(rx_comp_pkts[j])
                  mtip_dma_free_completion_wrapper(rx_comp_pkts[j]);
            }
            kfree(rx_comp_pkts);
         }

         CSMLOGINFO("Deallocate TX buffers\n");

         if(tx_pkts != NULL)
         {
            for (j = 0; j < MTIP_TX_RING_SIZE; ++j)
            {
               if(tx_pkts[j] == NULL)
                  continue;

               mtip_dma_free_pkt(tx_pkts[j]);
               tx_pkts[j] = NULL;
            }
         }

         CSMLOGINFO("Deallocate RX Buffers replenished to DMA\n");

         if(rx_pkts != NULL)
         {
            for (j = 0; j < MTIP_RX_RING_SIZE-1; j++)
            {
               if(rx_pkts[j] == NULL)
                  continue;

               mtip_dma_free_pkt(rx_pkts[j]);
               rx_pkts[j] = NULL;
            }
            kfree(rx_pkts);
         }
      }
   }

   return 0;
}

int mtip_lane_remove(struct platform_device *pdev)
{
   int i;
   CSMLOGINFO("mtip_lane_remove called\n");

   // free the net devices
   for (i = 0; i < MTIP_MAX_LANES; ++i)
   {
      if (platform_driver_priv->mtip_lanes[i] != NULL)
      {
         // cleanup the link
         mtip_platform_cleanup_lane(i);
      }
   }

   return 0;
}

int mtip_port_remove(struct platform_device *pdev)
{
   CSMLOGINFO("mtip_port_remove called\n");

   // free the ports
   mtip_platform_cleanup_ports();
   return 0;
}

int mtip_platform_remove(struct platform_device *pdev)
{
   CSMLOGINFO("mtip_platform_remove called\n");

   // nothing to be done here since there are no dynamic allocations
   // during platform_probe

   return 0;
}

int mtip_platform_convert_lane_speed_to_gbps(enum eth_phy_iface_phy_lane_speed_enum lane_speed)
{
    switch (lane_speed)
    {
    case PHY_LANE_SPEED_100G:
        {
            return 100000;
        }
        break;

    case PHY_LANE_SPEED_50G:
        {
            return 50000;
        }
        break;

    case PHY_LANE_SPEED_10G:
        {
            return 10000;
        }
        break;
    case PHY_LANE_SPEED_25G:
        {
            return 25000;
        }
        break;
    default:
        {
            CSMLOGERR("Unknown lane_speed %d\n", lane_speed);
            return -1;
        }
        break;
    }
    return -1;
}

void mtip_platform_print_link_device(struct mtip_link_device_info* link_device)
{
    CSMLOGINFO("link phandle: %d, link_index: %d, link name: %s", 
               link_device->link_phandle, 
               link_device->link_index,
               link_device->link_name);
}

void mtip_platform_print_lane_device(struct mtip_lane_device_info* lane_device)
{
    CSMLOGINFO("lane phandle: %d, lane_index: %d", 
               lane_device->lane_phandle, 
               lane_device->lane_index);
}

void mtip_platform_print_port_device(struct mtip_port_device_info* port_device)
{
    int i;
    u32 port_type = port_device->port_type;
    struct mtip_port_info* port_info;

    CSMLOGINFO("printing port phandle: %d, port_type: %d", 
               port_device->port_phandle,
               port_type);

    port_info = platform_driver_priv->mtip_ports[port_type];

    CSMLOGINFO("port config: %d str %s", port_info->port_config, mtip_ethtool_get_port_config_str(port_info->port_config));

    for (i = 0; i < PHY_LANE_MAX; ++i) 
    {
        CSMLOGINFO("lane config[%d] enabled: %d speed: %d link_index: %d", 
                   i, 
                   port_info->lane_config[i].lane_enabled,
                   port_info->lane_config[i].lane_speed,
                   port_info->lane_config[i].link_index);
    }

    CSMLOGINFO("num links: %d", port_device->num_link_phandles);

    for (i = 0; i < port_device->num_link_phandles; ++i) 
    {
        CSMLOGINFO("link device[%d] link phandle: %d start", i, port_device->link_phandles[i]);
        mtip_platform_print_link_device(port_device->link_devices[i]);
        CSMLOGINFO("link device[%d] end", i);
    }

    for (i = 0; i < port_device->num_lane_phandles; ++i) 
    {
        CSMLOGINFO("lane device[%d] lane phandle: %d start", i, port_device->lane_phandles[i]);
        mtip_platform_print_lane_device(port_device->lane_devices[i]);
        CSMLOGINFO("lane device[%d] end", i);
    }
}

/**
 * print the contents of devices
 */
void mtip_platform_print_devices(void)
{
    int i;

    CSMLOGINFO("devices enum_mode: %d, num ports: %d", 
               platform_driver_priv->devices.mode,
               platform_driver_priv->devices.num_port_phandles);

    for (i = 0; i < MTIP_MAX_PORTS; ++i) 
    {
        if (platform_driver_priv->devices.port_devices[i].port_device_valid) 
        {
            CSMLOGINFO("port device: %d port phandle: %d start", i, platform_driver_priv->devices.port_phandles[i]);
            mtip_platform_print_port_device(&platform_driver_priv->devices.port_devices[i]);
            CSMLOGINFO("port device: %d end", i);
        }
    }
}

/**
 * print the contents of links
 */
void mtip_platform_print_links(void)
{
    int i;
    int j;
    struct mtip_link_info* link;
    struct net_device* dev;
    struct mtip_netdev_priv *priv;

    for (i = 0; i < MTIP_MAX_LINKS; ++i) 
    {
        if (platform_driver_priv->mtip_links[i] != NULL) 
        {
            link = platform_driver_priv->mtip_links[i];

            CSMLOGINFO("mtip_links[%d] link_index: %d state: %d, dma_hdl: %d, ts_enable: %d, peak_rx_available: %d, active fec: %d", 
                       i,
                       link->link_index,
                       link->state,
                       link->dma_hdl,
                       link->ptp_ts_enabled,
                       link->peak_rx_available,
                       link->active_fec);

            CSMLOGINFO("mtip_links[%d] assign %d num_lanes %d", i, link->lanes_assignment_complete, link->num_assigned_lanes);

            dev = platform_driver_priv->mtip_links[i]->dev;
            priv = netdev_priv(dev);

            CSMLOGINFO("mtip_links[%d] hashtbl %ld priv_flags %d priv_flags_set %d", i, priv->hashtablebits, priv->priv_flags, priv->priv_flags_set);

            for (j = 0; j < PHY_LANE_MAX; ++j) 
            {
                CSMLOGINFO("mtip_links[%d] lane_indices[%d] %d", i, j, link->assigned_lane_indices[j]);
            }
        }
        else
        {
            CSMLOGINFO("mtip_link[%d] is NULL", i);
        }
    }
}

/**
 * print the contents of lanes
 */
void mtip_platform_print_lanes(void)
{
    int i;
    struct mtip_lane_info* lane;

    for (i = 0; i < MTIP_MAX_LANES; ++i) 
    {
        if (platform_driver_priv->mtip_lanes[i] != NULL) 
        {
            lane = platform_driver_priv->mtip_lanes[i];

            CSMLOGINFO("mtip_lane[%d] lane_index: %d state: %d sfp %d speed mask 0x%x",
                       i,
                       lane->lane_index,
                       lane->lane_state,
                       lane->sfp_port_type,
                       lane->speed_mask);

            CSMLOGINFO("trx_module_type %d, trx_laneinfo: %d, breakout cfg: %d",
                       lane->lane_qsfp_info.trx_module_type, lane->lane_qsfp_info.trx_laneinfo, lane->lane_qsfp_info.trx_bout_cfg);
        }
        else
        {
            CSMLOGINFO("mtip_lane[%d] is NULL", i);
        }
    }
}

/**
 * print the contents of ports
 */
void mtip_platform_print_ports(void)
{
    int i;
    int j;
    struct mtip_port_info* port;

    for (i = 0; i < MTIP_MAX_PORTS; ++i) 
    {
        if (platform_driver_priv->mtip_ports[i] != NULL) 
        {
            port = platform_driver_priv->mtip_ports[i];

            CSMLOGINFO("mtip_ports[%d] type: %d, state: %d priv_flags %d config %d sfp %d", 
                       i,
                       port->port_type,
                       port->port_state,
                       port->port_priv_flags,
                       port->port_config,
                       port->sfp_port_type);

            for (j = 0; j < PHY_LANE_MAX; ++j) 
            {
                CSMLOGINFO("mtip_ports[%d] lane_config[%d]: enabled: %d speed: %d link_index: %d", 
                           i, 
                           j, 
                           port->lane_config[j].lane_enabled,
                           port->lane_config[j].lane_speed,
                           port->lane_config[j].link_index);
            }
        }
        else
        {
            CSMLOGINFO("mtip_ports[%d] is NULL", i);
        }
    }
}

void mtip_platform_print_platform(void)
{
    CSMLOGINFO("Printing platform start");
    CSMLOGINFO("perr: %d dma_is_ready: %d, phy_is_ready: %d", platform_driver_priv->perr, platform_driver_priv->dma_is_ready, platform_driver_priv->phy_is_ready);
    mtip_platform_print_devices();
    mtip_platform_print_ports();
    mtip_platform_print_links();
    mtip_platform_print_lanes();
    CSMLOGINFO("Printing platform end");
}

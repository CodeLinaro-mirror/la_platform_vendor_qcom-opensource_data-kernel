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

#include "mtip_mac.h"
#include "mtip.h"
#include "mtip_device.h"
#include "mtip_platform.h"
#include "mtip_dut.h"
#include "mtip_logging.h"
#include "mtip_ptp.h"
#include "mtip_workq.h"
#include "mtip_ethtool.h"
#include "mtip_phy.h"
#include "mtip_notifr.h"
#include "mtip_debug_eth.h"
#include "eth_phy_iface.h"
#include "mtip_pcs.h"

static u32 mtip_mac_get_interrupt_summary(struct mtip_port_device_info* port_device)
{
    u32 read_val = 0;
    void __iomem *wrapper_base_addr;

    wrapper_base_addr = port_device->wrapper_base_addr;

    read_val = (u32)ioread32(wrapper_base_addr + MTIP_MAC_WRAPPER_INTERRUPT_SUMMARY_REG_OFFSET);
    return read_val;
}

static u32 mtip_mac_get_interrupt_status(u32 link_index)
{
    u32 read_val = 0;
    void __iomem *wrapper_base_addr;
    u32 port_type;
    u32 real_link_number;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return 0;
    }

    mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number);

    wrapper_base_addr = platform_driver_priv->devices.port_devices[port_type].wrapper_base_addr;

    read_val = (u32)ioread32(wrapper_base_addr + real_link_number*MTIP_MAC_WRAPPER_INTERRUPT_OFFSET + MTIP_MAC_WRAPPER_INTERRUPT_STAT_REG_OFFSET);
    return read_val;
}

void mtip_mac_clear_interrupts(u32 link_index, u32 int_to_clear)
{
    u32 write_val = int_to_clear;
    void __iomem *wrapper_base_addr;
    u32 port_type;
    u32 real_link_number;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return;
    }

    mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number);

    wrapper_base_addr = platform_driver_priv->devices.port_devices[port_type].wrapper_base_addr;

    CSMLOGDBG("clearing 0x%x on link_index: %d", write_val, link_index);

    // clear all interrupts
    iowrite32(write_val,
              wrapper_base_addr + real_link_number*MTIP_MAC_WRAPPER_INTERRUPT_OFFSET + MTIP_MAC_WRAPPER_INTERRUPT_CLR_REG_OFFSET);
    return;
}

static void mtip_mac_clear_all_interrupts(u32 link_index, u32 int_to_clear)
{
    u32 write_val = int_to_clear;
    void __iomem *wrapper_base_addr;
    u32 port_type;
    u32 real_link_number;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return;
    }

    mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number);

    wrapper_base_addr = platform_driver_priv->devices.port_devices[port_type].wrapper_base_addr;

    CSMLOGDBG("clearing all interrupts on link_index: %d", link_index);

    // clear all interrupts
    iowrite32(write_val,
              wrapper_base_addr + real_link_number*MTIP_MAC_WRAPPER_INTERRUPT_OFFSET + MTIP_MAC_WRAPPER_INTERRUPT_CLR_REG_OFFSET);

    // reset the CLR_REG
    iowrite32(0x0,
              wrapper_base_addr + real_link_number*MTIP_MAC_WRAPPER_INTERRUPT_OFFSET + MTIP_MAC_WRAPPER_INTERRUPT_CLR_REG_OFFSET);
    return;
}

static irqreturn_t mtip_mac_interrupt_handler(int irq, void *devptr) 
{
   irqreturn_t retval = IRQ_NONE;
   struct mtip_port_device_info * portptr;
   u32 summary;
   u32 int_status;
   u32 int_mask;
   int i;
   u32 link_index;
   u32 timestamp_secs;
   u32 timestamp_nsecs;
   bool found = false;
   bool handled = false;
   u8 ts_seq_num = 0x0;
   enum mtip_device_mode_enum mode = MTIP_DEVICE_DU;
   u32 handled_interrupts = MTIP_MAC_INTERRUPT_PTP_TX_INTR;
   handled_interrupts |= MTIP_MAC_INTERRUPT_LINK_DOWN_INTR;
   handled_interrupts |= MTIP_MAC_INTERRUPT_LINK_UP_INTR;

   mode = platform_driver_priv->devices.mode;

   CSMLOGDBG("ENTER: Interrupt! handling 0x%x\n", handled_interrupts);

   // check if this an interrupt that needs to be handled
   for (i = 0; i < MTIP_MAX_PORTS; ++i) 
   {
       if (devptr == (void *)&platform_driver_priv->devices.port_devices[i]) 
       {
           found = true;
           break;
       }
   }

   if (found == false) 
   {
       // no need to handle interrupt
       retval = IRQ_NONE;
       goto func_exit;
   }

   // set the portptr
   portptr = (struct mtip_port_device_info *)devptr;

   CSMLOGDBG("Got an interrupt! on port type %d\n", portptr->port_type);

   // read the interrupt summary
   summary = mtip_mac_get_interrupt_summary(portptr);

   for (i = 0; i < MTIP_MAX_LINKS_PER_PORT; ++i)
   {
       if ((summary & 0x01) == 0x1)
       {
           CSMLOGDBG("Got an interrupt! on port type %d, link: %d\n", portptr->port_type, i);

           // the bit for link i is set
           // there is an interrupt pending
           mtip_lookup_link_index_by_port_type_and_real_link(&link_index, portptr->port_type, i);

           int_status = mtip_mac_get_interrupt_status(link_index);
           int_mask = mtip_mac_get_interrupt_mask(link_index);

           CSMLOGDBG("Interrupt status 0x%x for link: %d with link_index: %d, mask: 0x%x\n", int_status, i, link_index, int_mask);

           // check for PTP interrupt
           if ((int_status & MTIP_MAC_INTERRUPT_PTP_TX_INTR) != 0)
           {
               // there is a PTP interrupt pending
               mtip_mac_read_timestamp(link_index, &timestamp_secs, &timestamp_nsecs);

               CSMLOGDBG("Tx Timestamp %d, %d read for link: %d with link_index: %d\n", timestamp_secs, timestamp_nsecs, i, link_index);

               if ((mode == MTIP_DEVICE_RUv2 || mode == MTIP_DEVICE_DUv2) && link_index != MTIP_L2_ETH_LINK_INDEX && link_index != MTIP_C2C1_ETH_LINK_INDEX)
               {
                   mtip_mac_read_ts_seq_num(link_index, &ts_seq_num);
               }
               else
               {
                   ts_seq_num = platform_driver_priv->mtip_links[link_index]->ptp_ts_seq_num-1;
               }

               CSMLOGDBG("TS seq num: %d for link_index: %d", ts_seq_num, link_index);

               // post a job to workqueue to process this timestamp
               post_mtip_process_timestamp(link_index, timestamp_secs, timestamp_nsecs, ts_seq_num);

               handled = true;
           }

           // check for LINK DOWN
           if ((int_status & MTIP_MAC_INTERRUPT_LINK_DOWN_INTR) != 0)
           {
               CSMLOGDBG("Got a link down interrupt link_index: %d", link_index);

               // got a link down interrupt for link index
               post_mtip_process_link_state(link_index, false);

               handled = true;
           }

           // check for LINK UP
           if ((int_status & MTIP_MAC_INTERRUPT_LINK_UP_INTR) != 0) 
           {
               CSMLOGDBG("Got a link up interrupt link_index: %d", link_index);

               // got a link up interrupt for link index
               post_mtip_process_link_state(link_index, true);

               handled = true;
           }

           // catchall
           if ((int_status & ~(handled_interrupts)) != 0)
           {
               CSMLOGDBG("Interrupt 0x%x received for link: %d with link_index: %d\n", (int_status & ~(handled_interrupts)), i, link_index);

               handled = true;
           }

           // clear all the pending interrupts
           mtip_mac_clear_all_interrupts(link_index, int_status);
       }

       summary = summary >> 1;
   }

   if (handled == true) 
   {
       // set as handled
       retval = IRQ_HANDLED;
   }

func_exit:
   CSMLOGDBG("EXIT: Interrupt! handling retval = %d\n", retval);
   return retval;
}

static int mtip_mac_read_version(struct mtip_netdev_priv *priv) {
   u32 read_val;
   u32 link_index = priv->link_index;

   CSMLOGDBG("Reading REVISION register for link index: %d\n", link_index);

   // read the value
   read_val = ioread32(priv->mac_ioaddr + MTIP_MAC_REVISION);

   CSMLOGDBG("MAC REVISION: 0x%x for link index: %d\n", read_val, link_index);
   return 0;
}

static int mtip_mac_test_scratch(struct mtip_netdev_priv *priv) {
    u32 read_val;
    u32 write_val = 0xAAAAAAAA;
    u32 link_index = priv->link_index;

    // read the current value of scratch
    read_val = ioread32(priv->mac_ioaddr + MTIP_MAC_SCRATCH);

    // write the value
    iowrite32(write_val, priv->mac_ioaddr + MTIP_MAC_SCRATCH);

    // read the value again
    read_val = ioread32(priv->mac_ioaddr + MTIP_MAC_SCRATCH);

    // check that the read and write values are the same
    if (read_val != write_val) {
       CSMLOGERR("link index: %d, Read val: %d, write val: %d mismatch\n", link_index, read_val, write_val);
       return -1;
    }

    CSMLOGDBG("scratch register test passed for link index: %d\n", link_index);
   return 0;
}

static void mtip_mac_reset_mac(struct mtip_netdev_priv *priv) {
   u32 read_val;
   u32 write_val = MTIP_MAC_RESET;
   int flag = 0;
   u32 link_index = priv->link_index;

   CSMLOGDBG("Reseting MAC for link index: %d\n", link_index);

   // reset the MAC
   iowrite32(write_val, priv->mac_ioaddr + MTIP_MAC_COMMAND_CONFIG);

   while (flag == 0) {
      // wait for reset to complete
      mdelay(MTIP_MAC_RESET_DELAY_MSECS);

      // read the value again
      read_val = ioread32(priv->mac_ioaddr + MTIP_MAC_COMMAND_CONFIG);

      // check that the reset bit has cleared
      if (read_val & 0x00001000) {
         flag = 0;
      } else {
         flag = 1;
      }
   }

   CSMLOGDBG("MAC Reset complete for link index: %d\n", link_index);
   return;
}

void mtip_mac_set_frame_length(struct mtip_netdev_priv *priv, u32 frame_length)
{
   u32 link_index = priv->link_index;

   // check if the frame length at least MTIP_MAC_INIT_FRAME_LENGTH
   if (frame_length < MTIP_MAC_INIT_FRAME_LENGTH) 
   {
       // set the frame length to MTIP_MAC_INIT_FRAME_LENGTH
       iowrite32(MTIP_MAC_INIT_FRAME_LENGTH, priv->mac_ioaddr + MTIP_MAC_FRM_LENGTH);
       CSMLOGDBG("Setting frame length to %d for link index: %d\n", MTIP_MAC_INIT_FRAME_LENGTH, link_index);
   }
   else
   {
       iowrite32(frame_length, priv->mac_ioaddr + MTIP_MAC_FRM_LENGTH);
       CSMLOGDBG("Setting frame length to %d for link index: %d\n", frame_length, link_index);
   }
}

u32 mtip_mac_get_frame_length(u32 port_number, u32 link_number)
{
    u32 frame_length;
    void __iomem *mac_ioaddr;

    // lookup the mac_ioadr for the port and link
    mac_ioaddr = platform_driver_priv->devices.port_devices[port_number].link_devices[link_number]->mac_ioaddr;

    frame_length = ioread32(mac_ioaddr + MTIP_MAC_FRM_LENGTH);
    return frame_length;
}

void mtip_mac_set_mac_address(struct mtip_netdev_priv *priv,
                              struct sockaddr *saddr) {
   u32 lower = 0;
   u32 upper = 0;
   u32 link_index = priv->link_index;

   lower = (saddr->sa_data[0]) | (saddr->sa_data[1] << 8) | (saddr->sa_data[2] << 16) | (saddr->sa_data[3] << 24);

   upper = (saddr->sa_data[4]) | (saddr->sa_data[5] << 8);

   // write the lower bits
   iowrite32(lower, priv->mac_ioaddr + MTIP_MAC_MAC_ADDR_0);

   // write the upper bits
   iowrite32(upper, priv->mac_ioaddr + MTIP_MAC_MAC_ADDR_1);

   CSMLOGDBG("Set the MAC address for link index: %d to upper: 0x%x, lower: 0x%x\n", link_index, upper, lower);
}

void mtip_mac_get_mac_address_by_device(u32 port_device_index, u32 link_device_index, uint8_t sa_data[]) {
   u32 lower = 0;
   u32 upper = 0;
   void __iomem *mac_ioaddr;
   u32 link_index;
   const char* link_name;

   mtip_lookup_link_index_by_device(&link_index, port_device_index, link_device_index);

   // lookup the mac_ioadr for the port and link
   mac_ioaddr = platform_driver_priv->devices.port_devices[port_device_index].link_devices[link_device_index]->mac_ioaddr;

   // the link name
   link_name = platform_driver_priv->devices.port_devices[port_device_index].link_devices[link_device_index]->link_name;

   // read the lower bits
   lower = ioread32(mac_ioaddr + MTIP_MAC_MAC_ADDR_0);

   // read the lower bits
   upper = ioread32(mac_ioaddr + MTIP_MAC_MAC_ADDR_1);

   // update the sa_addr->sa_data
   sa_data[0] = (lower) & 0xFF;
   sa_data[1] = (lower >> 8) & 0xFF;
   sa_data[2] = (lower >> 16) & 0xFF;
   sa_data[3] = (lower >> 24) & 0xFF;

   sa_data[4] = (upper) & 0xFF;
   sa_data[5] = (upper >> 8) & 0xFF;

   CSMLOGDBG("Retrieved the MAC address of link index: %d, name: %s,  %x:%x:%x:%x:%x:%x \n", link_index, link_name, 
              sa_data[0], sa_data[1], sa_data[2], sa_data[3], sa_data[4], sa_data[5]);
}

void mtip_mac_set_mac_address_by_device(u32 port_device_index, u32 link_device_index, uint8_t sa_data[]) 
{
    u32 lower = 0;
    u32 upper = 0;
    void __iomem *mac_ioaddr;
    u32 link_index;
    const char* link_name;

    mtip_lookup_link_index_by_device(&link_index, port_device_index, link_device_index);

    // the link name
    link_name = platform_driver_priv->devices.port_devices[port_device_index].link_devices[link_device_index]->link_name;

    // lookup the mac_ioadr for the port and link
    mac_ioaddr = platform_driver_priv->devices.port_devices[port_device_index].link_devices[link_device_index]->mac_ioaddr;

    lower = (sa_data[0]) | (sa_data[1] << 8) | (sa_data[2] << 16) | (sa_data[3] << 24);

    upper = (sa_data[4]) | (sa_data[5] << 8);

    // write the lower bits
    iowrite32(lower, platform_driver_priv->devices.port_devices[port_device_index].link_devices[link_device_index]->mac_ioaddr + MTIP_MAC_MAC_ADDR_0);

    // write the upper bits
    iowrite32(upper, platform_driver_priv->devices.port_devices[port_device_index].link_devices[link_device_index]->mac_ioaddr + MTIP_MAC_MAC_ADDR_1);

    CSMLOGDBG("Set the MAC address for link index: %d, name: %s,  %x:%x:%x:%x:%x:%x \n", link_index, link_name, 
               sa_data[0], sa_data[1], sa_data[2], sa_data[3], sa_data[4], sa_data[5]);
}

void mtip_mac_get_mac_address_by_link_index(u32 link_index, uint8_t sa_data[]) {
   u32 lower = 0;
   u32 upper = 0;
   void __iomem *mac_ioaddr;
   const char* link_name;

   // lookup the mac_ioadr for the port and link
   mac_ioaddr = platform_driver_priv->devices.link_devices[link_index].mac_ioaddr;

   // the link name
   link_name = platform_driver_priv->devices.link_devices[link_index].link_name;

   // read the lower bits
   lower = ioread32(mac_ioaddr + MTIP_MAC_MAC_ADDR_0);

   // read the lower bits
   upper = ioread32(mac_ioaddr + MTIP_MAC_MAC_ADDR_1);

   // update the sa_addr->sa_data
   sa_data[0] = (lower) & 0xFF;
   sa_data[1] = (lower >> 8) & 0xFF;
   sa_data[2] = (lower >> 16) & 0xFF;
   sa_data[3] = (lower >> 24) & 0xFF;

   sa_data[4] = (upper) & 0xFF;
   sa_data[5] = (upper >> 8) & 0xFF;

}

void mtip_mac_set_mac_address_by_link_index(u32 link_index, uint8_t sa_data[]) 
{
    u32 lower = 0;
    u32 upper = 0;
    void __iomem *mac_ioaddr;
    const char* link_name;

    // the link name
    link_name = platform_driver_priv->devices.link_devices[link_index].link_name;

    // lookup the mac_ioadr for the port and link
    mac_ioaddr = platform_driver_priv->devices.link_devices[link_index].mac_ioaddr;

    lower = (sa_data[0]) | (sa_data[1] << 8) | (sa_data[2] << 16) | (sa_data[3] << 24);

    upper = (sa_data[4]) | (sa_data[5] << 8);

    // write the lower bits
    iowrite32(lower, platform_driver_priv->devices.link_devices[link_index].mac_ioaddr + MTIP_MAC_MAC_ADDR_0);

    // write the upper bits
    iowrite32(upper, platform_driver_priv->devices.link_devices[link_index].mac_ioaddr + MTIP_MAC_MAC_ADDR_1);

    CSMLOGDBG("Set the MAC address for link index: %d, name: %s,  %x:%x:%x:%x:%x:%x \n", link_index, link_name, 
               sa_data[0], sa_data[1], sa_data[2], sa_data[3], sa_data[4], sa_data[5]);
}

int mtip_mac_set_promisc_mode(struct mtip_netdev_priv *priv, bool mode)
{
    u32 read_val;
    u32 write_val;
    u32 link_index = priv->link_index;
    int ret = 0;

    // read the current value of the command config register
    read_val = ioread32(priv->mac_ioaddr + MTIP_MAC_COMMAND_CONFIG);

    if (mode)
    {
        // we need to set promisc mode bit
        // first check if the bit is already set
        if ((read_val & MTIP_MAC_COMMAND_CONFIG_SET_PROMISC) == 0)
        {
            write_val = read_val | MTIP_MAC_COMMAND_CONFIG_SET_PROMISC;

            // write the value back to the CONFIG register
            iowrite32(write_val, priv->mac_ioaddr + MTIP_MAC_COMMAND_CONFIG);

            CSMLOGDBG("Set the promisc mode for link index: %d\n", link_index);
        }
        else
        {
            CSMLOGDBG("Promisc mode already set for link index: %d\n", link_index);

            ret = 1;
        }
    }
    else
    {
        // we need to reset promisc mode bit
        // first check if the bit is already reset
        if ((read_val & MTIP_MAC_COMMAND_CONFIG_SET_PROMISC) != 0)
        {
            write_val = read_val & ~(MTIP_MAC_COMMAND_CONFIG_SET_PROMISC);

            // write the value back to the CONFIG register
            iowrite32(write_val, priv->mac_ioaddr + MTIP_MAC_COMMAND_CONFIG);
            CSMLOGDBG("Reset the promisc mode for link index: %d\n", link_index);
        }
        else
        {
            CSMLOGDBG("Promisc mode already reset for link index: %d\n", link_index);

            ret = 1;
        }
    }
    return ret;
}

void mtip_mac_set_hashtable_entry(struct mtip_netdev_priv *priv, u8 entry_address, u8 val)
{
   u32 write_val = 0;
   u32 link_index = priv->link_index;

   // first set the bottom 6 bits for the address
   write_val = (u32)entry_address & 0x0000003F;

   if (val != 0)
   {
      // set the bit 8
      write_val |= 0x00000100;
   }

   // write the value back to the CONFIG register
   iowrite32(write_val, priv->mac_ioaddr + MTIP_MAC_HASHTABLE_LOAD);
   CSMLOGDBG("Setting hashtable of link index: %d, address: %d to %d\n", link_index, entry_address, write_val);
}

void mtip_mac_link_up(u32 link_index)
{
    u32 port_type;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return;
    }
    mtip_pcs_update_active_fec(link_index, platform_driver_priv->mtip_ports[port_type]->port_config, platform_driver_priv->mtip_ports[port_type]->sfp_port_type);
    // Don't enable TX/RX if the link has been closed
    if (platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_CLOSE || platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_UP)
    {
        return;
    }

    // Reset the flags and counters for dual rate module speed switch
    platform_driver_priv->mtip_links[link_index]->link_down_received_post_link_up = false;
    platform_driver_priv->mtip_ports[port_type]->next_speed_retry_count = 0;

    CSMLOGINFO("MAC/PCS link up on link_index: %d\n", link_index);
    // set the link state as up
    platform_driver_priv->mtip_links[link_index]->state = MTIP_LINK_STATE_UP;

    //send a notification to ldmm.ko
    mtip_snd_event_notification(link_index, PCS_IF_UP);

    // Notify TRX driver to disable TX
    mtip_phy_notify_eth_event_to_trx(link_index, TRX_ETH_LINK_UP);

}

void mtip_mac_link_down(u32 link_index)
{
    platform_driver_priv->mtip_links[link_index]->active_fec = ETHTOOL_FEC_NONE;
    if(platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_DOWN)
        return;

    // Set the flag to track if this link down was received post a successful link up
    if(platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_UP)
        platform_driver_priv->mtip_links[link_index]->link_down_received_post_link_up = true;
    else
        platform_driver_priv->mtip_links[link_index]->link_down_received_post_link_up = false;

    CSMLOGINFO("MAC/PCS link down on link_index: %d\n", link_index);
    // set the link state to DOWN if not closed
    if (platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_CLOSE) 
    {
        // set link state as down
        platform_driver_priv->mtip_links[link_index]->state = MTIP_LINK_STATE_DOWN;
    }

    //send a notification to ldmm.ko
    mtip_snd_event_notification(link_index, PCS_IF_DOWN);

    // Notify TRX driver to disable TX
    mtip_phy_notify_eth_event_to_trx(link_index, TRX_ETH_LINK_DOWN);
}

static void mtip_mac_set_xif_mode(struct mtip_netdev_priv *priv) {
    u32 link_index = priv->link_index;
    u32 port_type;
    enum mtip_port_config_enum port_config;
    u32 xif_mode = MTIP_MAC_INIT_XIF_MODE_FOR_XLGMII;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return;
    }

    port_config = platform_driver_priv->mtip_ports[port_type]->port_config;

    switch (port_config) {
    case MTIP_PORT_CONFIG_1x25GBASE_R:
    case MTIP_PORT_CONFIG_1x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_4x25GBASE_R:
    case MTIP_PORT_CONFIG_4x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_1x10GBASE_R:
    case MTIP_PORT_CONFIG_1x10GBASE_R_FEC:
    case MTIP_PORT_CONFIG_4x10GBASE_R:
    case MTIP_PORT_CONFIG_4x10GBASE_R_FEC:
        {
            xif_mode = MTIP_MAC_INIT_XIF_MODE_FOR_XGMII;
        }
        break;

    default:
        {
            xif_mode = MTIP_MAC_INIT_XIF_MODE_FOR_XLGMII;
        }
        break;
    }

    CSMLOGDBG("Setting xif_mode of link_index: %d to: 0x%x", link_index, xif_mode);

    iowrite32(xif_mode, priv->mac_ioaddr + MTIP_MAC_XIF_MODE);
    return;
}

/*
1.	Provide a soft reset to the MAC core by doing the following steps.
a.	Write to the register MAC0_COMMAND_CONFIG with vaue 0x1800.
After the soft reset we need to initialize the  MAC by following steps 2 to 10.
2.	Program the MAC0_COMMAND_CONFIG with value 0x2813.
3.	Program MAC0_FRM_LENGTH register with the required frame length.
4.	Program MAC0_RX_FIFO_SECTIONS with 0x0001
5.	Program MAC0_TX_FIFO_SECTIONS with value 0x0004
6.	Program MAC0_HASHTABLE_LOAD with 0x0
7.	Program MAC0_TX_IPG_LENGTH with value 0xC.
8.	Program MAC0_CRC_MODE register with value 0x0.
9.	Program MAC0_CRC_INV_MASK with value 0xFFFFFFFF.
10.	Program MAC0_XIF_MODE register.
*/
void mtip_mac_initialize(struct mtip_netdev_priv *priv)
{
    u32 link_index = priv->link_index;

   CSMLOGINFO("Initializing the MAC for link index: %d\n", link_index);

   // read the version info and print it
   mtip_mac_read_version(priv);

   // check the scratch memory write and read
   mtip_mac_test_scratch(priv);

   // reset the MAC
   mtip_mac_reset_mac(priv);

   // configure the mac for operation
   iowrite32(MTIP_MAC_INIT_COMMAND_CONFIG, priv->mac_ioaddr + MTIP_MAC_COMMAND_CONFIG);

   // Configure MAC with the promiscuous mode if already set by QNCM before MAC init
   mtip_rx_mode_set(platform_driver_priv->mtip_links[link_index]->dev);

   // Process link up only for PCS loopback mode
   if ( (link_index != MTIP_L2_ETH_LINK_INDEX && link_index != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode == MTIP_MODE_LOOPBACK) || ((link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX) && mtip_c2c2_loopback_mode == MTIP_MODE_C2C2_LOOPBACK))
   {
       post_mtip_process_link_state(link_index, true);
   }

   // set the initial frame length
   mtip_mac_set_frame_length(priv, MTIP_MAC_INIT_FRAME_LENGTH);

   // configure the RX and TX FIFO sections
   iowrite32(MTIP_MAC_INIT_RX_FIFO_SECTIONS, priv->mac_ioaddr + MTIP_MAC_RX_FIFO_SECTIONS);

   iowrite32(MTIP_MAC_INIT_TX_FIFO_SECTIONS, priv->mac_ioaddr + MTIP_MAC_TX_FIFO_SECTIONS);

   // set the HASH TABLE LOAD
   iowrite32(MTIP_MAC_INIT_HASHTABLE_LOAD, priv->mac_ioaddr + MTIP_MAC_HASHTABLE_LOAD);

   // set the IPG
   iowrite32(MTIP_MAC_INIT_TX_IPG_LENGTH, priv->mac_ioaddr + MTIP_MAC_TX_IPG_LENGTH);

   // set the CRC mode
   iowrite32(MTIP_MAC_INIT_CRC_MODE, priv->mac_ioaddr + MTIP_MAC_CRC_MODE);

   // set the CRC INV MASK
   iowrite32(MTIP_MAC_INIT_CRC_INV_MASK, priv->mac_ioaddr + MTIP_MAC_CRC_INV_MASK);

   // set the XIF mode
   mtip_mac_set_xif_mode(priv);

   CSMLOGDBG("MAC Init complete\n");
}

void mtip_mac_finalize(void __iomem *mac_base_addr, unsigned int irq, const char *name, void *devptr) {
   // reset the MTIP hardware

    // disable the irq wake
    disable_irq_wake(irq);
    free_irq(irq, devptr);
}

static u32 mtip_mac_wrapper_calendar_cfg_val(struct mtip_port_device_info* port_device)
{
    u32 cfg_val = 0;
    u32 port_type = port_device->port_type;
    enum mtip_port_config_enum port_config = platform_driver_priv->mtip_ports[port_type]->port_config;

    switch (port_config) 
    {
    case MTIP_PORT_CONFIG_1x100GBASE_R:
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC_LL:
    case MTIP_PORT_CONFIG_1x100GBASE_R2:
    case MTIP_PORT_CONFIG_1x100GBASE_R2_RSFEC:
    case MTIP_PORT_CONFIG_1x50GBASE_R:
    case MTIP_PORT_CONFIG_1x50GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_1x40GBASE_R4:
    case MTIP_PORT_CONFIG_1x40GBASE_R4_FEC:
    case MTIP_PORT_CONFIG_1x10GBASE_R:
    case MTIP_PORT_CONFIG_1x10GBASE_R_FEC:
    case MTIP_PORT_CONFIG_1x100GBASE_R4:
    case MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC:
    case MTIP_PORT_CONFIG_1x50GBASE_R2:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_RSFEC:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI_FEC:
    case MTIP_PORT_CONFIG_1x25GBASE_R:
    case MTIP_PORT_CONFIG_1x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC:
        {
            if (port_type == MTIP_PORT_TYPE_L2)
                cfg_val = MTIP_MAC_WRAPPER_CALENDAR_CFG_REG_VAL_2_LINKS;
            else
                cfg_val = MTIP_MAC_WRAPPER_CALENDAR_CFG_REG_VAL_1_LINKS;
        }
        break;

    case MTIP_PORT_CONFIG_2x50GBASE_R:
    case MTIP_PORT_CONFIG_2x50GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_2x50GBASE_R2:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_FEC:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI_FEC:
        {
            cfg_val = MTIP_MAC_WRAPPER_CALENDAR_CFG_REG_VAL_2_LINKS;
        }
        break;
    case MTIP_PORT_CONFIG_4x25GBASE_R:
    case MTIP_PORT_CONFIG_4x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_4x10GBASE_R:
    case MTIP_PORT_CONFIG_4x10GBASE_R_FEC:
        {
            cfg_val = MTIP_MAC_WRAPPER_CALENDAR_CFG_REG_VAL_4_LINKS;
        }
        break;
    default:
        {
            cfg_val = MTIP_MAC_WRAPPER_CALENDAR_CFG_REG_VAL_4_LINKS;
        }
        break;
    }

    CSMLOGDBG("Setting calendar config of port %d to 0x%x with port config %d, str %s", 
               port_device->port_type, 
               cfg_val, 
               port_config, 
               mtip_ethtool_get_port_config_str(port_config));

    return cfg_val;
}

static void mtip_mac_wrapper_set_csr_cfg(struct mtip_port_device_info* port_device)
{
    // by default enable TX_CRC_APPEND for all modes
    u32 csr_cfg = MTIP_MAC_WRAPPER_CSR_CFG_REG_BASE_VAL;
    void __iomem* wrapper_base_addr = port_device->wrapper_base_addr;
    u32 port_type = port_device->port_type;
    enum mtip_port_config_enum port_config = platform_driver_priv->mtip_ports[port_type]->port_config;

    switch (port_config)
    {
    case MTIP_PORT_CONFIG_1x100GBASE_R:
        {
          if (port_type == MTIP_PORT_TYPE_L2 )
            csr_cfg |= 0x3f0;
          else
            csr_cfg |= 0x130;
        }
        break;
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC:
        {
            csr_cfg |= 0x130;
        }
        break;
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC_LL:
        {
            csr_cfg |= 0x130;
        }
        break;
    case MTIP_PORT_CONFIG_1x100GBASE_R2:
        {
          if (port_type == MTIP_PORT_TYPE_L2 )
            csr_cfg |= 0xf0;
          else
            csr_cfg |= 0x30;
        }
        break;
    case MTIP_PORT_CONFIG_1x100GBASE_R2_RSFEC:
        {
            csr_cfg |= 0x30;
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R:
        {
            csr_cfg |= 0x30;
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R_RSFEC:
        {
            csr_cfg |= 0x30;
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R:
        {
            csr_cfg |= 0x30;
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R_RSFEC:
        {
            csr_cfg |= 0x30;
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R2:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R2_FEC:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI_FEC:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_1x40GBASE_R4:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_1x40GBASE_R4_FEC:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_1x10GBASE_R:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_1x10GBASE_R_FEC:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_1x100GBASE_R4:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R2:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R2_RSFEC:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI_FEC:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_1x25GBASE_R:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_1x25GBASE_R_FEC:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_4x25GBASE_R:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_4x25GBASE_R_FEC:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_4x10GBASE_R:
        {
            csr_cfg |= 0x0;
        }
        break;
    case MTIP_PORT_CONFIG_4x10GBASE_R_FEC:
        {
            csr_cfg |= 0x0;
        }
        break;
    default:
        {
            // set to the default value
            csr_cfg |= 0x0;
        }
        break;
    }

    CSMLOGDBG("Setting CSR_CFG to: 0x%x", csr_cfg);

    // set the mac wrapper csr cfg
    iowrite32(csr_cfg,
              wrapper_base_addr + MTIP_MAC_WRAPPER_CSR_CONFIG_OFFSET);
    return;
}

static void mtip_mac_wrapper_set_pcs_mode(struct mtip_port_device_info* port_device)
{
    u32 pcs_mode_set = 0;
    void __iomem* wrapper_base_addr = port_device->wrapper_base_addr;
    u32 port_type = port_device->port_type;
    enum mtip_port_config_enum port_config = platform_driver_priv->mtip_ports[port_type]->port_config;

    switch (port_config) 
    {
    case MTIP_PORT_CONFIG_1x100GBASE_R:
        {
          if(port_type == MTIP_PORT_TYPE_L2 )
            pcs_mode_set =  0x0C03CF; //0x404c0;
          else
            pcs_mode_set =  0x404c0;
        }
        break;
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC:
        {
            pcs_mode_set =  0x400c0;
        }
        break;
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC_LL:
        {
            pcs_mode_set =  0x430c0;
        }
        break;
    case MTIP_PORT_CONFIG_1x100GBASE_R2:
        {
          if(port_type == MTIP_PORT_TYPE_L2 )
            pcs_mode_set = 0x0C03CF;// 0x400c3;
          else
            pcs_mode_set = 0x400c3;
        }
        break;
    case MTIP_PORT_CONFIG_1x100GBASE_R2_RSFEC:
        {
            pcs_mode_set =  0x4f0c3;
        }
        break;
    case MTIP_PORT_CONFIG_1x100GBASE_R4:
        {
            pcs_mode_set =  0x40000;
        }
        break;
    case MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC:
        {
            pcs_mode_set =  0x4000F;
        }
        break;
    case MTIP_PORT_CONFIG_1x40GBASE_R4:
        {
            pcs_mode_set =  0x100000;
        }
        break;
    case MTIP_PORT_CONFIG_1x40GBASE_R4_FEC:
        {
            pcs_mode_set =  0x100000;
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R:
        {
            pcs_mode_set =  0xc3;
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R_RSFEC:
        {
            pcs_mode_set =  0x30c3;
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R2:
        {
            pcs_mode_set = 0x30000;
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R2_RSFEC:
        {
            pcs_mode_set =  0x3003;
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI:
        {
            pcs_mode_set =  0x10000;
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI_FEC:
        {
            pcs_mode_set =  0;
        }
        break;
    case MTIP_PORT_CONFIG_1x25GBASE_R:
    case MTIP_PORT_CONFIG_1x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_1x10GBASE_R:
    case MTIP_PORT_CONFIG_1x10GBASE_R_FEC:
    case MTIP_PORT_CONFIG_4x25GBASE_R:
    case MTIP_PORT_CONFIG_4x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_4x10GBASE_R:
    case MTIP_PORT_CONFIG_4x10GBASE_R_FEC:
        {
            pcs_mode_set =  0;
        }
        break;
    case MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC:
        {
            pcs_mode_set =  0x0031;
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R:
        {
            pcs_mode_set =  0xc3;
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R_RSFEC:
        {
            pcs_mode_set =  0x30c3;
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R2:
        {
            pcs_mode_set = 0x30000;
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R2_FEC:
        {
            pcs_mode_set =  0;
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI:
        {
            pcs_mode_set =  0;
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI_FEC:
        {
            pcs_mode_set =  0;
        }
        break;
    case MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC:
        {
            pcs_mode_set =  0x3f;
        }
        break;
    default:
        {
            pcs_mode_set = 0x0;
        }
        break;
    }

    CSMLOGDBG("Setting PCS Mode to: 0x%x", pcs_mode_set);

    // set the mac wrapper pcs mode set
    iowrite32(pcs_mode_set,
              wrapper_base_addr + MTIP_MAC_WRAPPER_PCS_MODE_SET_OFFSET);
    return;
}

static void mtip_mac_wrapper_set_serdes_mux_cfg(struct mtip_port_device_info* port_device)
{
    u32 serdes_mux_val = 0;
    void __iomem* wrapper_base_addr = port_device->wrapper_base_addr;
    u32 port_type = port_device->port_type;
    enum mtip_port_config_enum port_config = platform_driver_priv->mtip_ports[port_type]->port_config;


    if(port_type == MTIP_PORT_TYPE_DEBUG && !check_if_valid_port_config_for_debug_eth(port_config))
    {
        CSMLOGERR("config %d not supported on port_type %d", port_config, port_type);
        return;
    }

    switch (port_config)
    {
    case MTIP_PORT_CONFIG_1x100GBASE_R:
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC_LL:
        {
            serdes_mux_val = 0x1;
        }
        break;

    case MTIP_PORT_CONFIG_1x100GBASE_R2:
    case MTIP_PORT_CONFIG_1x100GBASE_R2_RSFEC:
        {
            serdes_mux_val = 0x2;
        }
        break;

    case MTIP_PORT_CONFIG_1x100GBASE_R4:
    case MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC:
        {
            serdes_mux_val = 0x4;
        }
        break;

    case MTIP_PORT_CONFIG_1x50GBASE_R:
    case MTIP_PORT_CONFIG_1x50GBASE_R_RSFEC:
        {
            serdes_mux_val = 0x20;
        }
        break;

    case MTIP_PORT_CONFIG_1x50GBASE_R2:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_RSFEC:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI_FEC:
        {
            serdes_mux_val = 0x8;
        }
        break;

    case MTIP_PORT_CONFIG_1x40GBASE_R4:
    case MTIP_PORT_CONFIG_1x40GBASE_R4_FEC:
        {
            serdes_mux_val = 0x10;
        }
        break;

    case MTIP_PORT_CONFIG_1x25GBASE_R:
    case MTIP_PORT_CONFIG_1x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC:
        {
            serdes_mux_val = 0x400;
        }
        break;

    case MTIP_PORT_CONFIG_1x10GBASE_R:
    case MTIP_PORT_CONFIG_1x10GBASE_R_FEC:
        {
            serdes_mux_val = 0;
        }
        break;

    case MTIP_PORT_CONFIG_2x50GBASE_R:
    case MTIP_PORT_CONFIG_2x50GBASE_R_RSFEC:
        {
            serdes_mux_val = 0x20;
        }
        break;

    case MTIP_PORT_CONFIG_2x50GBASE_R2:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_FEC:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI_FEC:
        {
            serdes_mux_val = 0x800;
        }
        break;

    case MTIP_PORT_CONFIG_4x25GBASE_R:
    case MTIP_PORT_CONFIG_4x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC:
        {
            serdes_mux_val = 0x400;
        }
        break;

    case MTIP_PORT_CONFIG_4x10GBASE_R:
    case MTIP_PORT_CONFIG_4x10GBASE_R_FEC:
        {
            serdes_mux_val = 0;
        }
        break;

    default:
        {
            serdes_mux_val = 0;
        }
        break;
    }

    CSMLOGDBG("Setting SERDES MUX CFG to: 0x%x\n", serdes_mux_val);

    // set the serdes mux val register for debug port
    if(port_type == MTIP_PORT_TYPE_DEBUG)
    {
        iowrite32(serdes_mux_val,
              wrapper_base_addr + MTIP_DBG_MAC_WRAPPER_SERDES_MUX_CFG1_OFFSET);
    }
    // set the serdes mux val register for fh ports
    else
    {
    iowrite32(serdes_mux_val,
              wrapper_base_addr + MTIP_MAC_WRAPPER_SERDES_MUX_CFG_OFFSET);
    }
    return;
}

/* 
 * mtip_mac_wrapper_pcs_mode_control 
 */ 
void mtip_mac_wrapper_pcs_mode_control(struct mtip_port_device_info* port_device)
{
    // set the csr cfg value
    mtip_mac_wrapper_set_csr_cfg(port_device);

    // set the pcs mode value
    mtip_mac_wrapper_set_pcs_mode(port_device);

    // set the serdes mux cfg value
    mtip_mac_wrapper_set_serdes_mux_cfg(port_device);

    return;
}

/*
 * MAC Wrapper Init
 */
void mtip_mac_wrapper_init(struct mtip_port_device_info* port_device)
{
    void __iomem *wrapper_base_addr = port_device->wrapper_base_addr;
    u32 calendar_cfg_val;
    u32 port_type = port_device->port_type;
    enum mtip_port_config_enum port_config = platform_driver_priv->mtip_ports[port_type]->port_config;
    u32 tx_amf_cfg_val;

   CSMLOGDBG("MAC Wrapper Init\n");

   // set the TSC_OFFSET REG
   iowrite32(MTIP_MAC_WRAPPER_TSC_OFFSET_REG_VAL,
             wrapper_base_addr + MTIP_MAC_WRAPPER_TSC_OFFSET_REG_OFFSET);

   // enable the Global timer
   iowrite32(MTIP_MAC_WRAPPER_GLOBAL_TIMER_EN_REG_VAL,
             wrapper_base_addr + MTIP_MAC_WRAPPER_GLOBAL_TIMER_EN_REG_OFFSET);

   calendar_cfg_val = mtip_mac_wrapper_calendar_cfg_val(port_device);

   // Enable MAC instances with calendar config register
   iowrite32(calendar_cfg_val,
             wrapper_base_addr + MTIP_MAC_WRAPPER_CALENDAR_CFG_REG_OFFSET);

   // set the TX_AMF value
   switch (port_config)
   {
   case MTIP_PORT_CONFIG_1x100GBASE_R:
   case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC:
   case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC_LL:
   case MTIP_PORT_CONFIG_1x100GBASE_R2:
   case MTIP_PORT_CONFIG_1x100GBASE_R2_RSFEC:
   case MTIP_PORT_CONFIG_1x50GBASE_R:
   case MTIP_PORT_CONFIG_1x50GBASE_R_RSFEC:
   case MTIP_PORT_CONFIG_1x40GBASE_R4:
   case MTIP_PORT_CONFIG_1x40GBASE_R4_FEC:
   case MTIP_PORT_CONFIG_1x10GBASE_R:
   case MTIP_PORT_CONFIG_1x10GBASE_R_FEC:
   case MTIP_PORT_CONFIG_1x100GBASE_R4:
   case MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC:
   case MTIP_PORT_CONFIG_1x50GBASE_R2:
   case MTIP_PORT_CONFIG_1x50GBASE_R2_RSFEC:
   case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI:
   case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI_FEC:
   case MTIP_PORT_CONFIG_1x25GBASE_R:
   case MTIP_PORT_CONFIG_1x25GBASE_R_FEC:
   case MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC:
       {
           if (port_type == MTIP_PORT_TYPE_L2)
               tx_amf_cfg_val = MTIP_MAC_WRAPPER_TX_AMF_CFG_REG_VAL_2_LINKS;
           else
               tx_amf_cfg_val = MTIP_MAC_WRAPPER_TX_AMF_CFG_REG_VAL_1_LINKS;
       }
       break;

   case MTIP_PORT_CONFIG_2x50GBASE_R:
   case MTIP_PORT_CONFIG_2x50GBASE_R_RSFEC:
   case MTIP_PORT_CONFIG_2x50GBASE_R2:
   case MTIP_PORT_CONFIG_2x50GBASE_R2_FEC:
   case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI:
   case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI_FEC:
       {
           tx_amf_cfg_val = MTIP_MAC_WRAPPER_TX_AMF_CFG_REG_VAL_2_LINKS;
       }
       break;
   case MTIP_PORT_CONFIG_4x25GBASE_R:
   case MTIP_PORT_CONFIG_4x25GBASE_R_FEC:
   case MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC:
   case MTIP_PORT_CONFIG_4x10GBASE_R:
   case MTIP_PORT_CONFIG_4x10GBASE_R_FEC:
       {
           tx_amf_cfg_val = MTIP_MAC_WRAPPER_TX_AMF_CFG_REG_VAL_4_LINKS;
       }
       break;
   default:
       {
           tx_amf_cfg_val = MTIP_MAC_WRAPPER_TX_AMF_CFG_REG_VAL_4_LINKS;
       }
       break;
   }

   CSMLOGDBG("Setting tx_amf to 0x%x for port type %d with port config %d", tx_amf_cfg_val, port_type, port_config);

   // Configure TX AMF value
   iowrite32(tx_amf_cfg_val,
             wrapper_base_addr + MTIP_MAC_WRAPPER_TX_AMF_CFG_REG_OFFSET);

   return;
}

void mtip_mac_wrapper_register_irq(struct device *dev, unsigned int irq,
                              const char* dev_name, void* devptr)
{
   int irqret;

   CSMLOGDBG("Registering IRQ %d, %s for MAC Wrapper\n", irq, dev_name);

   irqret = devm_request_irq(dev, irq, (irq_handler_t)mtip_mac_interrupt_handler, IRQF_SHARED | IRQF_TRIGGER_HIGH | IRQF_ONESHOT, dev_name, devptr);
   if (irqret) {
       CSMLOGERR("failed to register IRQ handler irq=%d\n", irq);
       return;
   }
   CSMLOGDBG("IRQ handler irq=%d registered\n", irq);

   irqret = enable_irq_wake(irq);
   if (irqret)
       CSMLOGERR("fail to enable IRQ wakeup irq=%d res=%d\n", irq, irqret);
   else
       CSMLOGDBG("IRQ wakeup enabled irq=%d\n", irq);

   CSMLOGDBG("MAC Wrapper IRQ register done\n");
}

void mtip_mac_set_interrupt_mask(u32 link_index)
{
    u32 write_val = 0;
    void __iomem *wrapper_base_addr;
    u32 port_type;
    u32 real_link_number;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return;
    }

    mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number);

    wrapper_base_addr = platform_driver_priv->devices.port_devices[port_type].wrapper_base_addr;

    // set the interrupts we are interested in
    write_val  =  MTIP_MAC_INTERRUPT_PTP_TX_INTR;

    CSMLOGDBG("Setting mask: 0x%x to register 0x%x with real_link_number %d link_index %d\n", write_val, 
               real_link_number*MTIP_MAC_WRAPPER_INTERRUPT_OFFSET + MTIP_MAC_WRAPPER_INTERRUPT_MASK_REG_OFFSET, real_link_number, link_index);

    // Enable MAC interrupt
    iowrite32(write_val,
              wrapper_base_addr + real_link_number*MTIP_MAC_WRAPPER_INTERRUPT_OFFSET + MTIP_MAC_WRAPPER_INTERRUPT_MASK_REG_OFFSET);
    return;
}

void mtip_mac_clear_interrupt_mask(u32 link_index)
{
    u32 write_val = 0;
    void __iomem *wrapper_base_addr;
    u32 port_type;
    u32 real_link_number;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return;
    }

    mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number);

    wrapper_base_addr = platform_driver_priv->devices.port_devices[port_type].wrapper_base_addr;

    CSMLOGDBG("Setting mask: 0x%x to register 0x%x with real_link_number %d link_index %d\n", write_val, 
               real_link_number*MTIP_MAC_WRAPPER_INTERRUPT_OFFSET + MTIP_MAC_WRAPPER_INTERRUPT_MASK_REG_OFFSET, real_link_number, link_index);

    // Enable MAC interrupt
    iowrite32(write_val,
              wrapper_base_addr + real_link_number*MTIP_MAC_WRAPPER_INTERRUPT_OFFSET + MTIP_MAC_WRAPPER_INTERRUPT_MASK_REG_OFFSET);
    return;
}

void mtip_mac_set_link_status_interrupt_mask(u32 link_index)
{
    u32 write_val = 0;
    void __iomem *wrapper_base_addr;
    u32 port_type;
    u32 real_link_number;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return;
    }

    mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number);

    wrapper_base_addr = platform_driver_priv->devices.port_devices[port_type].wrapper_base_addr;

    write_val  =  mtip_mac_get_interrupt_mask(link_index);
    write_val |= MTIP_MAC_INTERRUPT_LINK_DOWN_INTR;
    write_val |= MTIP_MAC_INTERRUPT_LINK_UP_INTR;

    CSMLOGDBG("Setting mask: 0x%x to register 0x%x with real_link_number %d link_index %d\n", write_val, 
               real_link_number*MTIP_MAC_WRAPPER_INTERRUPT_OFFSET + MTIP_MAC_WRAPPER_INTERRUPT_MASK_REG_OFFSET, real_link_number, link_index);

    // Enable MAC interrupt
    iowrite32(write_val,
              wrapper_base_addr + real_link_number*MTIP_MAC_WRAPPER_INTERRUPT_OFFSET + MTIP_MAC_WRAPPER_INTERRUPT_MASK_REG_OFFSET);
    return;
}

void mtip_mac_clear_link_status_interrupt_mask(u32 link_index)
{
    u32 write_val = 0;
    void __iomem *wrapper_base_addr;
    u32 port_type;
    u32 real_link_number;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return;
    }

    mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number);

    wrapper_base_addr = platform_driver_priv->devices.port_devices[port_type].wrapper_base_addr;

    write_val  =  mtip_mac_get_interrupt_mask(link_index);
    write_val &= (~MTIP_MAC_INTERRUPT_LINK_DOWN_INTR);
    write_val &= (~MTIP_MAC_INTERRUPT_LINK_UP_INTR);

    CSMLOGDBG("Setting mask: 0x%x to register 0x%x with real_link_number %d link_index %d\n", write_val, 
               real_link_number*MTIP_MAC_WRAPPER_INTERRUPT_OFFSET + MTIP_MAC_WRAPPER_INTERRUPT_MASK_REG_OFFSET, real_link_number, link_index);

    // Enable MAC interrupt
    iowrite32(write_val,
              wrapper_base_addr + real_link_number*MTIP_MAC_WRAPPER_INTERRUPT_OFFSET + MTIP_MAC_WRAPPER_INTERRUPT_MASK_REG_OFFSET);
    return;
}

void mtip_mac_read_timestamp(u32 link_index, u32* timestamp_secs, u32* timestamp_nsecs)
{
    void __iomem *wrapper_base_addr;
    u32 port_type;
    u32 real_link_number;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return;
    }

    mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number);

    wrapper_base_addr = platform_driver_priv->devices.port_devices[port_type].wrapper_base_addr;

    // read the upper 32 bits
    *timestamp_secs = (u32)ioread32(wrapper_base_addr + real_link_number*MTIP_MAC_WRAPPER_TX_TS_REG_OFFSET + MTIP_MAC_WRAPPER_TX_TS1_REG_BASE_OFFSET);

    // read the lower 32 bits
    *timestamp_nsecs = (u32)ioread32(wrapper_base_addr + real_link_number*MTIP_MAC_WRAPPER_TX_TS_REG_OFFSET + MTIP_MAC_WRAPPER_TX_TS0_REG_BASE_OFFSET);

    return;
}

void mtip_mac_read_ts_seq_num(u32 link_index, u8* ts_seq_num)
{
    void __iomem *wrapper_base_addr;
    u32 port_type;
    u32 real_link_number;
    u32 ts_reg_val;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return;
    }

    mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number);

    wrapper_base_addr = platform_driver_priv->devices.port_devices[port_type].wrapper_base_addr;

    // read the ts sequence number
    ts_reg_val = (u32)ioread32(wrapper_base_addr + real_link_number*MTIP_MAC_WRAPPER_TX_TS_SEQ_NUM_LINK_REG_OFFSET + MTIP_MAC_WRAPPER_TX_TS_SEQ_NUM_REG_BASE_OFFSET);

    // the ts seq number are the bottom four bits
    // as defined in IPCAT
    *ts_seq_num = (u8)(ts_reg_val & 0x0000000F);
    return;
}
void mtip_mac_read_tx_ts_stat_reg(u32 link_index, u8* tx_ts_stat)
{
    void __iomem *wrapper_base_addr;
    u32 port_type;
    u32 real_link_number;
    u32 ts_reg_val;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return;
    }

    mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number);

    wrapper_base_addr = platform_driver_priv->devices.port_devices[port_type].wrapper_base_addr;

    // read the ts status register value for checking underflow and overflow of H.W timestamp queue
    ts_reg_val = (u32)ioread32(wrapper_base_addr + real_link_number*MTIP_MAC_WRAPPER_TX_TS_STAT_REG_OFFSET + MTIP_MAC_WRAPPER_TX_TS_STAT_REG_BASE_OFFSET);

    // the ts status value are the bottom two bits
    // as defined in IPCAT
    *tx_ts_stat = (u8)(ts_reg_val & 0x0000000F);
    return;
}
u32 mtip_mac_get_interrupt_mask(u32 link_index)
{
    u32 read_val = 0;
    void __iomem *wrapper_base_addr;
    u32 port_type;
    u32 real_link_number;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return 0;
    }

    mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number);

    wrapper_base_addr = platform_driver_priv->devices.port_devices[port_type].wrapper_base_addr;

    read_val = (u32)ioread32(wrapper_base_addr + real_link_number*MTIP_MAC_WRAPPER_INTERRUPT_OFFSET + MTIP_MAC_WRAPPER_INTERRUPT_MASK_REG_OFFSET);
    return read_val;
}

bool mtip_mac_wrapper_get_link_status(u32 link_index)
{
    u32 read_val = 0;
    void __iomem *wrapper_base_addr;
    u32 port_type;
    u32 real_link_number;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return false;
    }

    mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number);

    wrapper_base_addr = platform_driver_priv->devices.port_devices[port_type].wrapper_base_addr;

    read_val = (u32)ioread32(wrapper_base_addr + MTIP_MAC_WRAPPER_CORE_STATUS_REG_OFFSET);
    CSMLOGINFO("mtip_mac_wrapper_get_link_status link_index: %d, core status = %d, for port %d, real link %d",
               link_index, read_val, port_type, real_link_number);

    if (((read_val & GENMASK(9,6)) >> 6) & (1 << real_link_number))
      return true;

    return false;
}

// enable RSFEC for 25G mode
// set the MAC WRAPPER PCS MODE for the port
void mtip_mac_wrapper_enable_rsfec_for_25g_mode(struct mtip_port_device_info* port_device)
{
    // PCS_MODE_SET to 0x3F
    u32 pcs_mode_set = MTIP_MAC_WRAPPER_PCS_MODE_4X25G_RSFEC_ENABLE_VAL;
    void __iomem* wrapper_base_addr = port_device->wrapper_base_addr;

    CSMLOGDBG("Setting PCS Mode: 0x%x\n", pcs_mode_set);

    // set the mac wrapper pcs mode set
    iowrite32(pcs_mode_set,
              wrapper_base_addr + MTIP_MAC_WRAPPER_PCS_MODE_SET_OFFSET);

    return;
}

// disable RSFEC for 25G mode
// reset the MAC WRAPPER PCS MODE for the port
void mtip_mac_wrapper_disable_rsfec_for_25g_mode(struct mtip_port_device_info* port_device)
{
    // PCS_MODE_SET to 0x0
    u32 pcs_mode_set = MTIP_MAC_WRAPPER_PCS_MODE_4X25G_RSFEC_DISABLE_VAL;
    void __iomem* wrapper_base_addr = port_device->wrapper_base_addr;

    CSMLOGDBG("Setting PCS Mode: 0x%x\n", pcs_mode_set);

    // set the mac wrapper pcs mode set
    iowrite32(pcs_mode_set,
              wrapper_base_addr + MTIP_MAC_WRAPPER_PCS_MODE_SET_OFFSET);

    return;
}


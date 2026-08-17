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

#include "mtip.h"
#include "mtip_logging.h"
#include "mtip_device.h"
#include "mtip_dma.h"
#include "mtip_mac.h"
#include "mtip_workq.h"
#include "mtip_ethtool.h"
#include "mtip_client.h"
#include "mtip_ptp.h"
#include "mtip_debug_eth.h"
#include "mtip_phy.h"
#include "mtip_sysfs.h"
#include "mtip_platform.h"
#include "eth_phy_iface.h"

#include "mtip_notifr.h"

extern u8 mtip_phy_retry_num[MTIP_MAX_LINKS];

int macsec_eth_set_macsec_ops(const struct macsec_ops* rb_macsec_ops)
{
    int i;

    // set the macsec ops of the platform
    platform_driver_priv->security_macsec_ops = (struct macsec_ops*)rb_macsec_ops;

    for (i = 0; i < MTIP_MAX_LINKS; ++i)
    {
        if (platform_driver_priv->mtip_links[i] != NULL)
        {
            platform_driver_priv->mtip_links[i]->dev->macsec_ops = rb_macsec_ops;
            platform_driver_priv->mtip_links[i]->dev->features |= NETIF_F_HW_MACSEC;
        }
    }
    return 0;
}

EXPORT_SYMBOL(macsec_eth_set_macsec_ops);

int macsec_eth_lookup_eth_link_by_netdev(struct net_device* netdev, u32* link_index)
{
    struct mtip_netdev_priv* priv;

    priv = netdev_priv(netdev);

    *link_index = priv->link_index;
    return 0;
}

EXPORT_SYMBOL(macsec_eth_lookup_eth_link_by_netdev);

struct net_device* macsec_eth_get_netdev_from_link(u32 link_index)
{
    if (platform_driver_priv->mtip_links[link_index] == NULL)
    {
        return NULL;
    }
    return platform_driver_priv->mtip_links[link_index]->dev;
}

EXPORT_SYMBOL(macsec_eth_get_netdev_from_link);
void mtip_get_rx_mode_immediate(ecpri_dma_eth_conn_hdl_t hdl, enum ecpri_dma_notify_mode *getmode)
{
    int rv;

    rv = (ecpri_dma_eth_driver_ops.ecpri_dma_eth_rx_mode_get)(hdl, getmode);

    if (rv < 0)
    {
        CSMLOGDBG("Get Rx mode of hdl: %d to %d failed.. %d\n", hdl, getmode, rv);
    }
}

void mtip_set_rx_mode_immediate(ecpri_dma_eth_conn_hdl_t hdl, enum ecpri_dma_notify_mode setmode)
{
    int rv;

    rv = (ecpri_dma_eth_driver_ops.ecpri_dma_eth_rx_mode_set)(hdl, setmode);

    if (rv < 0) 
    {
        CSMLOGDBG("Set Rx mode of hdl: %d to %d failed.. %d\n", hdl, setmode, rv);
    }
}
void mtip_set_tx_mode_immediate(ecpri_dma_eth_conn_hdl_t hdl, enum ecpri_dma_notify_mode setmode)
{
    int rv;

    rv = (ecpri_dma_eth_driver_ops.ecpri_dma_eth_tx_mode_set)(hdl, setmode);

    if (rv < 0)
    {
        CSMLOGDBG("Set Tx mode of hdl: %d to %d failed.. %d\n", hdl, setmode, rv);
    }
}


// reset the number of buffers waiting for completion
static int mtip_device_reset_buff_completion_count(struct net_device *netdev)
{
    unsigned long flags;
    struct mtip_netdev_priv* priv;
    spinlock_t *lock;

    priv = netdev_priv(netdev);
    lock = &(priv->lock);

    spin_lock_irqsave(lock, flags);
    priv->num_buff_pending_completion = 0;
    spin_unlock_irqrestore(lock, flags);
    return 0;
}

// update the completion count
static int mtip_device_update_buff_completion_count(struct net_device *netdev, int count)
{
    unsigned long flags;
    struct mtip_netdev_priv* priv;
    spinlock_t *lock;

    priv = netdev_priv(netdev);
    lock = &(priv->lock);

    spin_lock_irqsave(lock, flags);
    priv->num_buff_pending_completion += count;
    spin_unlock_irqrestore(lock, flags);
    return 0;
}

// get the completion count
static int mtip_device_get_buff_completion_count(struct net_device *netdev)
{
    struct mtip_netdev_priv* priv;

    priv = netdev_priv(netdev);

    return priv->num_buff_pending_completion;
}

void post_mtip_tx_comp_cb(void *user_data, ecpri_dma_eth_conn_hdl_t hdl, struct ecpri_dma_pkt_completion_wrapper **comp_pkts, u32 num_of_completed)
{
   struct mtip_tx_comp_cb_task* taskstruct = NULL;
   u32 link_index;
   u32 port_type;

   if(mtip_lookup_link_index_by_handle(hdl, &link_index))
   {
      CSMLOGERR("Unable to find link_index of hdl: %d\n", hdl);
      return;
   }

   if(mtip_lookup_port_type_by_link_index(link_index, &port_type))
   {
      CSMLOGERR("invalid link_index");
      return;
   }

   taskstruct = kmalloc(sizeof(struct mtip_tx_comp_cb_task), GFP_ATOMIC);
   if(taskstruct == NULL)
   {
      CSMLOGERR("memory alloc failed\n");
      return;
   }

   taskstruct->user_data = user_data;
   taskstruct->hdl = hdl;
   taskstruct->comp_pkts = comp_pkts;
   taskstruct->num_of_completed = num_of_completed;

   mtip_queue_work(MTIP_WORKQ_TASK_TX_COMP_CB, taskstruct, port_type);
}

void mtip_process_tx_comp_cb(ecpri_dma_eth_conn_hdl_t hdl, struct mtip_dma_tx_comp_params *tx_comp_params)
{
   int i;
   int j;
   struct ecpri_dma_pkt_completion_wrapper **comp_pkts;
   u32 num_of_completed;
   struct ecpri_dma_pkt_completion_wrapper *comp;
   struct ecpri_dma_pkt* pkt = NULL;
   struct sk_buff *skb;
   struct mtip_pkt_priv *pkt_priv = NULL;
   struct ecpri_dma_mem_buffer **buffs;
   unsigned int num_of_buffers;
   struct net_device *netdev = NULL;
   struct mtip_netdev_priv *priv;
   u32 link_index = 0;
   bool free_skb = true;
   u8 pkt_ts_seq_num = 0;
   struct ecpri_dma_tx_header *pre_header_buff;
   enum mtip_device_mode_enum mode = platform_driver_priv->devices.mode;
   int pending_buff_completion_count = 0;
   u32 num_buf_completed = 0;
   struct mtip_time_stamp msg5_time_stamp = {0};
   char *tmp=NULL;
   comp_pkts = tx_comp_params->local_comp_pkts;
   num_of_completed = tx_comp_params->num_of_completed;
   //CSMLOGDBG("Tx comp callback for hdl: %d, num_of_completed: %d\n", hdl, num_of_completed);

   // process the Tx completions
   for (i = 0; i < num_of_completed; ++i)
   {
      // for each completed packet
      comp = comp_pkts[i];
      pkt = comp->pkt;

      if (pkt == NULL)
      {
          CSMLOGERR("Got a NULL pkt\n");
          continue;
      }

      pkt_priv = (struct mtip_pkt_priv *)pkt->user_data;
      if (pkt_priv == NULL)
      {
          CSMLOGERR("Got a NULL userdata\n");
          continue;
      }

      skb = pkt_priv->skb;
      if (skb == NULL)
      {
          CSMLOGERR("Got a NULL skb\n");
          continue;
      }

      //CSMLOGDBG("Tx comp for hdl: %d, skb->data: 0x%lx mode %d \n", hdl, (unsigned long)skb->data, mode);

      // store the netdev
      netdev = skb->dev;
      priv = netdev_priv(netdev);
      link_index = priv->link_index;

      free_skb = true;

      buffs = (struct ecpri_dma_mem_buffer **)pkt->buffs;
      num_of_buffers = pkt->num_of_buffers;

      // Check if there is TX preheader,
      // 1. if yes, Read the buffer and clear it. Even if time stamp is not there, 
      //    TX preheader will be present for V2
      if ((mode == MTIP_DEVICE_RUv2) || (mode == MTIP_DEVICE_DUv2))
      {
          if(num_of_buffers == 2)
          {
              pre_header_buff = (struct ecpri_dma_tx_header *)buffs[0]->virt_base;
              if (pre_header_buff->timestamp_packet == true)
              {
                  pkt_ts_seq_num = (u8)pre_header_buff->timestamp_tag & MTIP_PKT_TS_SEQ_MASK;
              }

              // free the pre header buff
              mtip_dma_free_tx_header(pre_header_buff);
          }
      }

      // check if this skb needs HW timestamping
      if ((skb_shinfo(skb)->tx_flags & SKBTX_IN_PROGRESS)  != 0)
      {
          CSMLOGPTP("Tx comp cb for packet needing HW_TSTAMP\n");

          // this packet should have the packet TS info in pre-header
          // read the pkt_ts_seq_num from the pre-header (Already done above)

          // this packet needs to be timestamped
          // acquire the ptp lock
          tmp=(char*)(skb->data);
          CSMLOGPTP("pkt_type=%x,seq_id=%x%x,skb=%lx,pkt_ts_seq_num=%d,ts_list_size=%d,\
            skb_list_size=%d [%s]\n",tmp[46],tmp[44],tmp[45],(unsigned long)skb->data, \
            pkt_ts_seq_num,mtip_ptp_tx_ts_list_size(link_index), \
            mtip_ptp_tx_ts_skb_list_size(link_index),__func__);
         CSMLOGPTP("MTIP_DMA_CB: Seq Num Packet %u\n",pkt_ts_seq_num);
          mtip_ptp_tx_ts_lock_acquire(link_index);
          // check if there is a timestamp available
         if(MTIP_ECPRI_MSG5 == pkt_ts_seq_num)
         {
            if(!is_valid_mtip_msg5_tx_ts_time_stamp_exist())
            {
               free_skb = false;
               /*
                * As Time stamp doesn't exist
                * save the skb
                */
               mtip_msg5_tx_ts_skb_set(skb);
            }
            else
            {
               msg5_time_stamp = mtip_msg5_tx_ts_get();
               mtip_msg5_tx_ts_clear();
               mtip_ptp_set_tx_timestamp(skb, msg5_time_stamp.tstamp_secs, msg5_time_stamp.tstamp_nsecs);
               free_skb = true;
            }
         }
         else
         {
	    if(platform_driver_priv->mtip_links[link_index]->tstamp_info.tstamp[pkt_ts_seq_num].tstamp_secs ||
			    platform_driver_priv->mtip_links[link_index]->tstamp_info.tstamp[pkt_ts_seq_num].tstamp_nsecs)
	    {
	       CSMLOGPTP("SEQ %u : Got Time stamp in DB, free skb sec=%u nsec=%u %s\n",pkt_ts_seq_num,
			       platform_driver_priv->mtip_links[link_index]->tstamp_info.tstamp[pkt_ts_seq_num].tstamp_secs,
			       platform_driver_priv->mtip_links[link_index]->tstamp_info.tstamp[pkt_ts_seq_num].tstamp_nsecs,__func__);
               free_skb = true;
	       mtip_ptp_set_tx_timestamp(skb,
			    platform_driver_priv->mtip_links[link_index]->tstamp_info.tstamp[pkt_ts_seq_num].tstamp_secs,
			    platform_driver_priv->mtip_links[link_index]->tstamp_info.tstamp[pkt_ts_seq_num].tstamp_nsecs);

               mtip_ptp_tx_ts_skb_clear(link_index, pkt_ts_seq_num);
	       platform_driver_priv->mtip_links[link_index]->tstamp_info.tstamp[pkt_ts_seq_num].tstamp_secs = 0;
	       platform_driver_priv->mtip_links[link_index]->tstamp_info.tstamp[pkt_ts_seq_num].tstamp_nsecs = 0;
	    }else{
		    free_skb = false;
               mtip_ptp_tx_ts_skb_set(skb, link_index, pkt_ts_seq_num);
		    CSMLOGPTP("SEQ %u : Time stamp not found in DB, saving skb %s\n",pkt_ts_seq_num,__func__);
	    }

         }
          // release the ptp lock
          mtip_ptp_tx_ts_lock_release(link_index);
      }
      
      if (free_skb == true)
      {
          // no Tx timestamping needed for this packet
          //CSMLOGDBG("freeing skb hdl: %d, len: %d\n", hdl, skb->len);

          dev_kfree_skb(skb);
      }

      //CSMLOGDBG("i: %d, buffers: %d\n", i, num_of_buffers);

      for (j = 0; j < num_of_buffers; ++j) {
         // free the mem buffer
         mtip_dma_free_mem_buffer(buffs[j]);
      }

      // free the container
      if (num_of_buffers == 1) 
      {
          mtip_dma_free_mem_buffer_single_ptr(buffs);
      }
      else if (num_of_buffers == 2) 
      {
          mtip_dma_free_mem_buffer_dual_ptr(buffs);
      }
      else
      {
          CSMLOGERR("invalid number of buffers %d", num_of_buffers);
      }

      num_buf_completed += num_of_buffers;

      // Check if this packet is present in TX array 
      if(priv->tx_pkts[pkt_priv->tx_index] == pkt)
      {
         priv->tx_pkts[pkt_priv->tx_index] = NULL;
         mtip_dma_free_dma_pkt(pkt);
         pkt = NULL;
      }
      else
      {
         CSMLOGERR("TX Comp not matched with TX array,link_index=%d,tx_index=%d,pkt=0x%x,priv->tx_pkts[%d]=0x%x\n",
          link_index,pkt_priv->tx_index,pkt,pkt_priv->tx_index,priv->tx_pkts[pkt_priv->tx_index]);
      }

      kfree(pkt_priv);

      // free the dma pkt
      if(pkt)
         mtip_dma_free_dma_pkt(pkt);
   }

   if(netdev == NULL)
   {
     return;
   }

   // decrement the buff completion count
   mtip_device_update_buff_completion_count(netdev, (-1*(int)num_buf_completed));

   pending_buff_completion_count = mtip_device_get_buff_completion_count(netdev);

   if (pending_buff_completion_count < 0) 
   {
       mtip_device_reset_buff_completion_count(netdev);
       pending_buff_completion_count = 0;
   }

   // check if we need to flow control the interface
   if (pending_buff_completion_count < (MTIP_TX_RING_SIZE - MTIP_TX_PACKET_AVAILABILITY_THRESHOLD))
   {
      if (netif_queue_stopped(netdev))
      {
         CSMLOGDBG("waking queue for link_index %d", link_index);
         
         // wake the queue
         netif_wake_queue(netdev);
      }
   }

}

void post_mtip_process_link_state(u32 link_index, bool link_up)
{
   u32 port_type;
   struct mtip_process_link_state_task* taskstruct = NULL;

   if(mtip_lookup_port_type_by_link_index(link_index, &port_type))
   {
      CSMLOGERR("invalid link index\n");
      return;
   }

   taskstruct = kmalloc(sizeof(struct mtip_process_link_state_task), GFP_ATOMIC);
   if(taskstruct == NULL)
   {
      CSMLOGERR("memory alloc failed\n");
      return;
   }

   taskstruct->link_index = link_index;
   taskstruct->link_up = link_up;

   mtip_queue_work(MTIP_WORKQ_TASK_PROCESS_LINK_STATE, taskstruct, port_type);
}

void run_mtip_process_link_state(void* work_ptr)
{
    struct mtip_process_link_state_task* taskstruct = (struct mtip_process_link_state_task*)work_ptr;
    u32 link_index = taskstruct->link_index;
    bool link_up = taskstruct->link_up;
    struct net_device *dev = platform_driver_priv->mtip_links[link_index]->dev;
    ecpri_dma_eth_conn_hdl_t dma_handle = 0;
    enum ecpri_dma_notify_mode setmode = ECPRI_DMA_NOTIFY_MODE_IRQ;
    enum ecpri_dma_notify_mode mode = ECPRI_DMA_NOTIFY_MODE_MAX;
    int rv = 0;
    u32 tx_available = 0;
    u32 rx_available = 0;
    struct mtip_netdev_priv *priv;
    struct mtip_link_info* link;

    if(link_index >= MTIP_MAX_LINKS)
    {
        CSMLOGERR("invalid link_index %d", link_index);
        goto func_exit;
    }

    if(platform_driver_priv == NULL || platform_driver_priv->mtip_links[link_index] == NULL)
         goto func_exit;

    priv = netdev_priv(dev);
    dma_handle = platform_driver_priv->mtip_links[link_index]->dma_hdl;
    if (link_up)
    {
        if(!platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer_running)
        {
          // Start defer timer to confirm the stability of link up
          mod_timer(&platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer,
                    jiffies + msecs_to_jiffies(MTIP_PCS_LINK_UP_DEFER_TIMER_INTERVAL));
          platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer_running = true;
          goto func_exit;
        }
        else if(timer_pending(&platform_driver_priv->mtip_links[link_index]->phy_retry_timer))
        {
          // Defer timer already running, let it expire
          goto func_exit;
        }
        else
        {
          platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer_running = false;
        }

        if((platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_UP) ||
           (mtip_mac_wrapper_get_link_status(link_index) == false))
        {
            // Ignore the stale event
            goto func_exit;
        }

        CSMLOGDBG("Processing LINK_UP for link_index: %d\n", link_index);

        // Process MAC link up state
        mtip_mac_link_up(link_index);

        rv = mtip_dma_get_ring_state(dma_handle, &tx_available, &rx_available);
        CSMLOGINFO("rx_available: %d,priv->rx_polled_count %d,link_index:%d\n", rx_available,priv->rx_polled_count,link_index);
        if (rv < 0)
        {
            CSMLOGERR("get ring state from DMA failed for hdl: %d\n", dma_handle);
        }
        else if( (link_index != MTIP_L2_ETH_LINK_INDEX && link_index != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode != MTIP_MODE_DEFAULT) || rx_available == MTIP_RX_RING_SIZE)
        {
            // set the rx mode to IRQ
            setmode = ECPRI_DMA_NOTIFY_MODE_IRQ;
            mtip_set_rx_mode_immediate(dma_handle, setmode);

            mtip_get_rx_mode_immediate(dma_handle, &mode);
            CSMLOGINFO("rx mode is:%d for hdl: %d\n", mode,dma_handle);
        }
        else
        {
            link = platform_driver_priv->mtip_links[link_index];

            if (napi_schedule_prep(&(link->napi)))
            {
                 // set the rx mode to POLL
                 setmode = ECPRI_DMA_NOTIFY_MODE_POLL;
                 mtip_set_rx_mode_immediate(dma_handle, setmode);

                 mtip_get_rx_mode_immediate(dma_handle, &mode);
                 CSMLOGINFO("rx mode is:%d for hdl: %d\n", mode,dma_handle);

                 __napi_schedule(&(link->napi));
                 CSMLOGINFO("RX napi schedule for hdl: %d, link_index: %d, link 0x%lx, netdev 0x%lx\n", dma_handle, link_index, (unsigned long)link, (unsigned long)dev);             // schedule napi
            }
        }

        // set the tx mode to IRQ
        setmode = ECPRI_DMA_NOTIFY_MODE_IRQ;
        mtip_set_tx_mode_immediate(dma_handle, setmode);
        // wake queues
        netif_tx_wake_all_queues(dev);

        if(link_index == MTIP_DEBUG_ETH_LINK_INDEX)
        {
           mtip_sysfs_mac_link_status(true);
        }

        // carrier is on
        if (!netif_carrier_ok(dev)) {
            netif_carrier_on(dev);
            netdev_info(dev, "Link is Up\n");
        }

        // tell all the clients of the link status update
        mtip_client_send_event(ETH_ECPRISS_EVENT_UP, link_index);
    }
    else
    {
        // If link up defer timer was running, stop the timer and ignore this link down
        if(platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer_running)
        {
          del_timer_sync(&platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer);
          platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer_running = false;
          goto func_exit;
        }

        if((platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_DOWN) ||
           (mtip_mac_wrapper_get_link_status(link_index) == true))
        {
            // Ignore the stale event
            goto func_exit;
        }

        CSMLOGDBG("Processing LINK_DOWN for link_index: %d\n", link_index);

        // stop the queues
        netif_tx_stop_all_queues(platform_driver_priv->mtip_links[link_index]->dev);

        // Process MAC link down state
        mtip_mac_link_down(link_index);

        if(link_index == MTIP_DEBUG_ETH_LINK_INDEX)
        {
            mtip_sysfs_mac_link_status(false);
        }

        if (netif_carrier_ok(dev)) {
            netif_carrier_off(dev);
            netdev_info(dev, "Link is Down\n");
        }

        // tell all the clients of the link status update
        mtip_client_send_event(ETH_ECPRISS_EVENT_DOWN, link_index);
    }

    if ( (link_index != MTIP_L2_ETH_LINK_INDEX && link_index != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode != MTIP_MODE_LOOPBACK) || ((link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX) && mtip_c2c2_loopback_mode != MTIP_MODE_C2C2_LOOPBACK ) )
    {
        // notify phy of the link status
        mtip_phy_notify_link_status(link_index, link_up);
    }

func_exit:
    // free the taskstruct
    kfree(taskstruct);
}

void mtip_process_link_state(u32 link_index, bool link_up)
{
    struct net_device *dev = platform_driver_priv->mtip_links[link_index]->dev;
    ecpri_dma_eth_conn_hdl_t dma_handle = 0;
    enum ecpri_dma_notify_mode setmode = ECPRI_DMA_NOTIFY_MODE_IRQ;
    enum ecpri_dma_notify_mode mode = ECPRI_DMA_NOTIFY_MODE_MAX;
    int rv = 0;
    u32 tx_available = 0;
    u32 rx_available = 0;
    struct mtip_netdev_priv *priv;
    struct mtip_link_info* link;

    dma_handle = platform_driver_priv->mtip_links[link_index]->dma_hdl;
    priv = netdev_priv(dev);
    if (link_up)
    {
        if(!platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer_running)
        {
          // Start defer timer to confirm the stability of link up
          mod_timer(&platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer,
                    jiffies + msecs_to_jiffies(MTIP_PCS_LINK_UP_DEFER_TIMER_INTERVAL));
          platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer_running = true;
          return;
        }
        else if(timer_pending(&platform_driver_priv->mtip_links[link_index]->phy_retry_timer))
        {
          // Defer timer already running, let it expire
          return;
        }
        else
        {
          platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer_running = false;
        }

        CSMLOGDBG("Processing LINK_UP for link_index: %d\n", link_index);

        // Process MAC link up state
        mtip_mac_link_up(link_index);
        rv = mtip_dma_get_ring_state(dma_handle, &tx_available, &rx_available);
        CSMLOGINFO("rx_available: %d,priv->rx_polled_count %d,link_index:%d\n", rx_available,priv->rx_polled_count,link_index);
        if (rv < 0)
        {
            CSMLOGERR("get ring state from DMA failed for hdl: %d\n", dma_handle);
        }
        else if( (link_index != MTIP_L2_ETH_LINK_INDEX && link_index != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode != MTIP_MODE_DEFAULT) || rx_available == MTIP_RX_RING_SIZE)
        {
            // set the rx mode to IRQ
            setmode = ECPRI_DMA_NOTIFY_MODE_IRQ;
            mtip_set_rx_mode_immediate(dma_handle, setmode);

            mtip_get_rx_mode_immediate(dma_handle, &mode);
            CSMLOGINFO("rx mode is:%d for hdl: %d\n", mode,dma_handle);
        }
        else
        {
            link = platform_driver_priv->mtip_links[link_index];

            if (napi_schedule_prep(&(link->napi)))
            {
                // set the rx mode to POLL
                setmode = ECPRI_DMA_NOTIFY_MODE_POLL;
                mtip_set_rx_mode_immediate(dma_handle, setmode);

                mtip_get_rx_mode_immediate(dma_handle, &mode);
                CSMLOGINFO("rx mode is:%d for hdl: %d\n", mode,dma_handle);

                __napi_schedule(&(link->napi));
                CSMLOGINFO("RX napi schedule for hdl: %d, link_index: %d, link 0x%lx, netdev 0x%lx\n", dma_handle, link_index, (unsigned long)link, (unsigned long)dev);             // schedule napi
            }
        }

        // set the tx mode to IRQ
        setmode = ECPRI_DMA_NOTIFY_MODE_IRQ;
        mtip_set_tx_mode_immediate(dma_handle, setmode);
        // wake queues
        netif_tx_wake_all_queues(dev);

        if(link_index == MTIP_DEBUG_ETH_LINK_INDEX)
        {
           mtip_sysfs_mac_link_status(true);
        }

        // carrier is on
        if (!netif_carrier_ok(dev)) {
            netif_carrier_on(dev);
            netdev_info(dev, "Link is Up\n");
        }

        // tell all the clients of the link status update
        mtip_client_send_event(ETH_ECPRISS_EVENT_UP, link_index);

        mtip_phy_lane_bring_up_progress_ind(link_index, false);
        mtip_phy_retry_num[link_index] = 0;
    }
    else
    {
        // If link up defer timer was running, stop the timer and ignore this link down
        if(platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer_running)
        {
          del_timer_sync(&platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer);
          platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer_running = false;
          return;
        }

        CSMLOGDBG("Processing LINK_DOWN for link_index: %d\n", link_index);

        // stop the queues
        netif_tx_stop_all_queues(platform_driver_priv->mtip_links[link_index]->dev);

        // Process MAC link down state
        mtip_mac_link_down(link_index);

        if(link_index == MTIP_DEBUG_ETH_LINK_INDEX)
        {
            mtip_sysfs_mac_link_status(false);
        }

        if (netif_carrier_ok(dev)) {
            netif_carrier_off(dev);
            netdev_info(dev, "Link is Down\n");
        }

        // tell all the clients of the link status update
        mtip_client_send_event(ETH_ECPRISS_EVENT_DOWN, link_index);
    }

    if ( ( link_index != MTIP_L2_ETH_LINK_INDEX && link_index != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode != MTIP_MODE_LOOPBACK) || ( (link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX) &&  mtip_c2c2_loopback_mode != MTIP_MODE_C2C2_LOOPBACK) )
    {
        // notify phy of the link status
        mtip_phy_notify_link_status(link_index, link_up);
    }

    return;
}

static int mtip_set_mac_address(struct net_device *dev, void *addr)
{
   unsigned long flags;
   struct mtip_netdev_priv* priv;
   u32 link_index;
   struct sockaddr *saddr = addr;
   spinlock_t *lock;

   if (!is_valid_ether_addr(saddr->sa_data))
      return -EADDRNOTAVAIL;

   priv = netdev_priv(dev);

   link_index = priv->link_index;
   lock = &(priv->lock);

   CSMLOGDBG("mtip_set_mac_address called for link_index: %d\n", link_index);

   spin_lock_irqsave(lock, flags);
   memcpy(dev->dev_addr, saddr->sa_data, ETH_ALEN);
   mtip_mac_set_mac_address(priv, saddr);
   spin_unlock_irqrestore(lock, flags);

   /* Send update to clients */
   /* TBD - Need to post this event in wq context if needed in future */
   //mtip_client_send_event(ETH_ECPRISS_EVENT_UP, link_index);

   return 0;
}

int mtip_set_netdev_hw_mac_addr(struct net_device *netdev, u32 link_index)
{
    uint8_t saddr[ETH_ALEN];

    mtip_mac_get_mac_address_by_link_index(link_index, saddr);

    CSMLOGDBG("Setting MAC address of link_index: %d\n", link_index);

    memcpy(netdev->dev_addr, saddr, ETH_ALEN);

    return 0;
}

int mtip_napi_poll_tx(struct napi_struct *napi_ptr, int budget)
{
    int rv = 0;
    int npackets = 0;
    int num_buffers = 0;
    u32 link_index;
    u32 list_counter;
    struct net_device* dev;
    struct mtip_netdev_priv *priv;
    unsigned int tx_comp_list_size;
    struct mtip_dma_tx_comp_params tx_comp_params = {0};
    struct mtip_link_info* link = container_of(napi_ptr, struct mtip_link_info, napi_tx);
    ecpri_dma_eth_conn_hdl_t hdl = link->dma_hdl;
    enum ecpri_dma_notify_mode setmode = ECPRI_DMA_NOTIFY_MODE_IRQ;
    ecpri_dma_eth_conn_hdl_t actual_handle = hdl;

    rv = mtip_lookup_link_index_by_handle(hdl, &link_index);

    // HANDLE THE ERROR
    if (rv < 0)
    {
        CSMLOGERR("Unable to find link_index of hdl: %d\n", hdl);
        return 0;
    }

    dev = platform_driver_priv->mtip_links[link_index]->dev;
    priv = netdev_priv(dev);

    tx_comp_list_size = mtip_dma_tx_comp_list_size(link_index);

    if(!enable_tx_comp_poll)
    {
        if (tx_comp_list_size > 0)
        {
            for (list_counter = 0; list_counter < tx_comp_list_size; list_counter++ )
            {
                if(mtip_dma_tx_comp_list_pop(link_index, &tx_comp_params) > 0)
		{
                   mtip_process_tx_comp_cb(hdl, &tx_comp_params);
                   npackets += tx_comp_params.num_of_completed; 
		}
            }
        }
    }
    else
    {
        // read the packets and process for tx completion 
        rv = mtip_dma_poll_tx_comp_packets(dev, napi_ptr, hdl, budget, &npackets, &num_buffers);

        // HANDLE THE ERROR
        if (rv < 0)
        {
            CSMLOGERR("poll_tx_packets failed for hdl: %d\n", hdl);
        }
    }

    if (npackets < budget)
    {
        napi_complete(napi_ptr);
        setmode = ECPRI_DMA_NOTIFY_MODE_IRQ;

        // set the tx mode to IRQ
        mtip_set_tx_mode_immediate(actual_handle, setmode);
    }

    return npackets;
}

/* NAPI Poll function */
int mtip_napi_poll(struct napi_struct *napi_ptr, int budget)
{
   int rv = 0;
   int npackets = 0;
   int num_buffers = 0;
   enum ecpri_dma_notify_mode setmode = ECPRI_DMA_NOTIFY_MODE_IRQ;
   struct mtip_link_info* link = container_of(napi_ptr, struct mtip_link_info, napi);
   ecpri_dma_eth_conn_hdl_t hdl = link->dma_hdl;
   u32 link_index = link->link_index;
   struct net_device* dev;
   struct mtip_netdev_priv *priv;
   ecpri_dma_eth_conn_hdl_t actual_handle = hdl;
   u32 tx_available = 0;
   u32 rx_available = 0;

   if (mtip_loopback_mode != MTIP_MODE_DEFAULT) 
   {
#ifdef MTIP_LOOPBACK_SWAP_HANDLE
       switch (hdl)
       {
       case 1:
          actual_handle = 2;
          break;
       case 2:
          actual_handle = 1;
          break;
       case 3:
          actual_handle = 4;
          break;
       case 4:
          actual_handle = 3;
          break;
       case 5:
          actual_handle = 6;
          break;
       case 6:
          actual_handle = 5;
          break;
       case 7:
          actual_handle = 8;
          break;
       case 8:
          actual_handle = 7;
          break;
       case 9:
          actual_handle = 10;
          break;
       case 10:
          actual_handle = 9;
          break;
       case 11:
          actual_handle = 12;
          break;
       case 12:
          actual_handle = 11;
          break;
       }
#endif
   }

   dev = platform_driver_priv->mtip_links[link_index]->dev;
   priv = netdev_priv(dev);

   CSMLOGDBG("mtip_napi_poll called with budget %d for link_index %d hdl %d\n", budget, link_index, hdl);

   // read the packets and push into the stack
   rv = mtip_dma_poll_rx_packets(dev, napi_ptr, hdl, budget, &npackets, &num_buffers);
   priv->rx_polled_count += num_buffers;
   // HANDLE THE ERROR
   if (rv < 0)
   {
      CSMLOGERR("poll_rx_packets failed for hdl: %d\n", hdl);
   }

   // Check if RX ring is 75% empty or not,
   // if yes then replenish buffers to DMA
   if(priv->rx_polled_count >= ((MTIP_RX_RING_SIZE*3)/4))
   {
      rv = mtip_dma_get_ring_state(actual_handle, &tx_available, &rx_available);
      if (rv < 0)
      {
         CSMLOGERR("get ring state from DMA failed for hdl: %d\n", hdl);
      }
      else
      {
         rv = mtip_replenish_dma_rx_buffers_reuse(dev, actual_handle, rx_available -1);
         if(rv == 0)
            priv->rx_polled_count = 0;
         else if(rx_available == MTIP_RX_RING_SIZE-1)
         {
            CSMLOGERR("priv->rx_polled_count %d, rx_available %d", priv->rx_polled_count, rx_available);
            /* If the replenish fails, while DMA has exhausted it's ring size,
               start a timer to retry replensish after a buffer time. By this
               time, the expectation is that NW stack will drain out the
               outstanding packets */
            if(!timer_pending(&platform_driver_priv->mtip_links[link_index]->rx_replenish_retry_timer))
            {
               mod_timer(&platform_driver_priv->mtip_links[link_index]->rx_replenish_retry_timer,
                         jiffies + msecs_to_jiffies(MTIP_RX_REPLENISH_RETRY_TIMER_INTERVAL));
            }
         }

      }
   }

   /* If we processed all packets, we're done; tell the kernel and re-enable ints */
   if (npackets < budget)
   {
      napi_complete(napi_ptr);

      // set the rx mode to IRQ
      setmode = ECPRI_DMA_NOTIFY_MODE_IRQ;
      mtip_set_rx_mode_immediate(actual_handle, setmode);
   }
   else
   {
      CSMLOGDBG("Remaining in POLL mode\n");
   }

   return npackets;
}

/* Transmit the packet */
static int mtip_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
   u32 link_index;
   ecpri_dma_eth_conn_hdl_t hdl;
   struct mtip_netdev_priv *priv;
   int ret;
   ecpri_dma_eth_conn_hdl_t other_hdl = 0;
   u32 other_link_index;
   struct mtip_security_device *sec_dev;
   u8 ts_seq_num = 0;
   bool send_tx_pre_header = false;
   bool send_tx_seq_num = false;
   enum mtip_link_state_enum link_state;
   enum mtip_device_mode_enum mode = platform_driver_priv->devices.mode;
   int pending_buff_completion_count = 0;
   char* tmp=NULL;
   u32 port_type;
   u8 tmp_ts_seq_num = 0;
   u8 tx_ts_stat=0;
   u32 timestamp_secs;
   u32 timestamp_nsecs;

   CSMLOGDBG("mtip_start_xmit called\n");

   priv = netdev_priv(netdev);
   link_index = priv->link_index;

   // Block A55 TX API calls for loopback interfaces
   if (mtip_is_link_in_loopback(link_index)) {
       CSMLOGDBG("Blocking TX on loopback interface %d\n", link_index);
       dev_kfree_skb(skb);
       return NETDEV_TX_OK;
   }

   if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
   {
        CSMLOGERR("invalid port_type for link_index %d", link_index);

        // free the skb
        dev_kfree_skb(skb);

        // drop the packet
        return NETDEV_TX_OK;
   }

   link_state = mtip_get_link_state_by_link_index(link_index);

   // we are not done opening the link
   if (link_state != MTIP_LINK_STATE_UP)
   {
       // free the skb
       dev_kfree_skb(skb);

       // drop the packet
       return NETDEV_TX_OK;
   }

   hdl = platform_driver_priv->mtip_links[link_index]->dma_hdl;
   sec_dev = priv->sec_dev;
   other_hdl = hdl;

   if(priv->link_index == MTIP_DEBUG_ETH_LINK_INDEX){
      return mtip_debug_eth_start_xmit(skb, netdev);
   }

   if (mtip_loopback_mode != MTIP_MODE_DEFAULT)
   {
       switch (hdl)
       {
       case 1:
          other_hdl = 2;
          break;
       case 2:
          other_hdl = 1;
          break;
       case 3:
          other_hdl = 4;
          break;
       case 4:
          other_hdl = 3;
          break;
       case 5:
          other_hdl = 6;
          break;
       case 6:
          other_hdl = 5;
          break;
       case 7:
          other_hdl = 8;
          break;
       case 8:
          other_hdl = 7;
          break;
       case 9:
          other_hdl = 10;
          break;
       case 10:
          other_hdl = 9;
          break;
       case 11:
          other_hdl = 12;
          break;
       case 12:
          other_hdl = 11;
          break;
       }

       if (mtip_lookup_link_index_by_handle(other_hdl, &other_link_index) < 0)
       {
          CSMLOGERR("did not find other link index for hdl: %d\n", other_hdl);

          // free the skb
          dev_kfree_skb(skb);

          // drop the packet
          return NETDEV_TX_OK;
       }

       // check if it is a neighbor solicitation msg and drop it for now
       if (!mtip_loopback_enable_arp && (skb->data[0] == 0x33) && (skb->data[1] == 0x33) && (skb->data[12] == 0x86) && (skb->data[13] == 0xdd))
       {
          CSMLOGERR("Dropping neighbor solicitation msg in loopback mode\n");

          // free the skb
          dev_kfree_skb(skb);

          // drop the packet
          return NETDEV_TX_OK;
       }
       if(port_type != MTIP_PORT_TYPE_L2)
       {
          // For FH ports, check that both links are in OPEN state for loopback mode
          if ((platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_OPEN_DONE &&
               platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_UP) || 
              (platform_driver_priv->mtip_links[other_link_index]->state != MTIP_LINK_STATE_OPEN_DONE &&
               platform_driver_priv->mtip_links[other_link_index]->state != MTIP_LINK_STATE_UP))
          {
             CSMLOGERR("Waiting for both interfaces to be open/up... dropping,link_index=%d, state=%d\n",link_index, platform_driver_priv->mtip_links[link_index]->state);

             // free the skb
             dev_kfree_skb(skb);

             // drop the packet
             return NETDEV_TX_OK;
          }
       }
       else 
       {
          // For L2 port, check that if L2 link is in OPEN state for loopback mode
          if ((platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_OPEN_DONE &&
            platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_UP) ) 
          {
             CSMLOGERR("Waiting for link to come up... dropping,link_index=%d, state=%d\n",link_index, platform_driver_priv->mtip_links[link_index]->state);

             // free the skb
             dev_kfree_skb(skb);

             // drop the packet
             return NETDEV_TX_OK;
          }

       }
   }

   pending_buff_completion_count = mtip_device_get_buff_completion_count(netdev);

   // check if we need to flow control the interface
   if (pending_buff_completion_count >= (MTIP_TX_RING_SIZE - MTIP_TX_PACKET_AVAILABILITY_THRESHOLD))
   {
       if (!netif_queue_stopped(netdev))
       {
           CSMLOGDBG("stopping queue for link_index %d", link_index);

           // wait for space to become available
           netif_stop_queue(netdev);
       }
       return NETDEV_TX_BUSY;
   }

   // check if we need to send pre-header
   if ((mode == MTIP_DEVICE_RUv2) || (mode == MTIP_DEVICE_DUv2)) 
   {
       send_tx_pre_header = true;
   }
   tmp=(char*)skb->data;
   // check if this packet needs timestamping
   if ((skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) != 0)
   {
       CSMLOGPTP("Tx packet needing HW_TSTAMP skb->data: 0x%lx\n", (unsigned long)skb->data);
       // check if we need to send sequence number
       if ((mode == MTIP_DEVICE_RUv2) || (mode == MTIP_DEVICE_DUv2)) 
       {
           send_tx_seq_num = true;
       }
       CSMLOGPTP("pkt_type=%x,seq_id=%x%x,skb=0x%lx, \
       ts_list_size=%d,skb_list_size=%d[%s]\n",tmp[46], \
       tmp[44],tmp[45],(unsigned long)skb->data,\
       mtip_ptp_tx_ts_list_size(link_index),mtip_ptp_tx_ts_skb_list_size(link_index) \
       ,__func__);
   
      mtip_ptp_tx_ts_lock_acquire(link_index);

      /*
       * The following checks are optimized for performance:
       * 1. Is it an eCPRI VLAN packet?
       *	- VLAN Ether Type: 0x8100 (in network byte order: htons(0x8100) --> 0x0081)
       * 2. Is it an eCPRI packet?
       *	- Ether Type for eCPRI: 0xAEFE (in network byte order: htons(0xAEFE) --> 0xFEAE)
       * 3. For eCPRI, only MSG-5 packets are supported for timestamping.
       */

      if(!((uint16_t)(0x0081) ^ *((uint16_t*)(tmp + 12)))  &&
		      (!((uint16_t )(0xfeae) ^ *((uint16_t*)(tmp+16)))) &&
			      (!((uint8_t)(0x05) ^ *((uint8_t*)(tmp+19)))))
      {
         {
            ts_seq_num = MTIP_ECPRI_MSG5; 
         }
      } 
      else if((!((uint16_t )(0xfeae) ^ *((uint16_t*)(tmp+12)))) &&
		      (!((uint8_t)(0x05) ^ *((uint8_t*)(tmp+15)))))
      {
         ts_seq_num = MTIP_ECPRI_MSG5; 

      }else{

         ts_seq_num = mtip_netdev_get_next_ptp_ts_seq_num(link_index);

         mtip_mac_read_timestamp(link_index, &timestamp_secs, &timestamp_nsecs);
         mtip_mac_read_tx_ts_stat_reg(link_index,&tx_ts_stat);
         CSMLOGPTP("timestamp_nsecs=%d,tx_ts_stat=%x\n",timestamp_nsecs,tx_ts_stat);
         while(tx_ts_stat!=2)
         {
             if ( (mode == MTIP_DEVICE_RUv2 || mode == MTIP_DEVICE_DUv2) && link_index != MTIP_L2_ETH_LINK_INDEX && link_index != MTIP_C2C1_ETH_LINK_INDEX)
             {
                 mtip_mac_read_ts_seq_num(link_index, &tmp_ts_seq_num);
             }
             mtip_mac_read_timestamp(link_index, &timestamp_secs, &timestamp_nsecs);
             mtip_mac_read_tx_ts_stat_reg(link_index,&tx_ts_stat);
             CSMLOGPTP("Pending h.w TS FIFO timestamp_nsecs=%d,tx_ts_stat=%x\n",timestamp_nsecs,tx_ts_stat);
         }
      }
      mtip_ptp_tx_ts_lock_release(link_index);
       // set the flag to in progress
       skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
      CSMLOGPTP("XMIT: Seq Num Packet %d\n",ts_seq_num);
   }

   if (sec_dev && sec_dev->ops && sec_dev->ops->fixup_tx_skb) {
      if (sec_dev->ops->fixup_tx_skb(skb)) {
         ++(platform_driver_priv->mtip_links[link_index]->net_stats.tx_errors);
         dev_kfree_skb(skb);
         return NETDEV_TX_OK;
      }
   }
   ret = mtip_dma_send_packet(netdev, hdl, skb, send_tx_pre_header, send_tx_seq_num, ts_seq_num);

   // HANDLE THE ERROR
   if (ret < 0) {
      return NETDEV_TX_BUSY;
   }

   // increment the pkt completion count
   if(send_tx_pre_header == true)
      mtip_device_update_buff_completion_count(netdev, 2);
   else
      mtip_device_update_buff_completion_count(netdev, 1);

   pending_buff_completion_count = mtip_device_get_buff_completion_count(netdev);

   if (pending_buff_completion_count < 0) 
   {
       mtip_device_reset_buff_completion_count(netdev);
       pending_buff_completion_count = 0;
   }

   return NETDEV_TX_OK;
}

static void mtip_configure_hashtable(struct mtip_netdev_priv *priv, u64 original_hashtablebits, u64 new_hashtablebits)
{
    u8 i;
    u8 val = 0;
    u64 pattern = 0x01;
    u64 delta = original_hashtablebits ^ new_hashtablebits;

    CSMLOGDBG("orig: %x, new: %x, delta: %x\n", original_hashtablebits, new_hashtablebits, delta);

    for (i = 0; i < MTIP_MAC_HASHTABLE_SIZE; ++i)
    {
        if ((delta & pattern) != 0)
        {
            if ((new_hashtablebits & pattern) != 0)
            {
                val = 0x01;
            }
            else
            {
                val = 0;
            }
            mtip_mac_set_hashtable_entry(priv, i, val);

            CSMLOGDBG("set hashtable entry: 0x%x to val: 0x%x\n", i, val);
        }
        pattern = pattern << 1;
    }
}

static u8 mtip_xor_bits(u8 bits)
{
    int i;
    u8 rv = 0;
    u8 tmpbits = bits;
    u8 val;

    for (i = 0; i < 8; ++i) {
        // find the least significant bit
        val = tmpbits & 0x01;

        rv = rv ^ val;

        tmpbits = tmpbits >> 1;
    }

    return rv;
}

static void mtip_generate_entry_address(struct netdev_hw_addr *ha, u8* entry_address)
{
    int i;
    u8 val;
    *entry_address = 0;

    // set the least significant bits
    for (i = 0; i < 6; ++i)
    {
        val = mtip_xor_bits(ha->addr[i]);

        // shift val by i
        val = val << i;

        *entry_address |= val;
    }

}

static void mtip_generate_hashtablebits(struct net_device *netdev, u64* hashtablebits)
{
    int i;
    struct netdev_hw_addr *ha;
    u8 entry_address;
    u64 pattern;

    *hashtablebits = 0;

    netdev_for_each_mc_addr(ha, netdev) {

        mtip_generate_entry_address(ha, &entry_address);

        pattern = 0x1;
        // set the corresponding hashtablebit to 1
        for (i = 0; i < entry_address; ++i) {
            pattern = pattern << 1;
        }
        *hashtablebits |= pattern;

    }

}

/* Configure Multicast and Promiscuous modes */
void mtip_rx_mode_set(struct net_device *netdev)
{
   bool is_multicast_enabled = !!(netdev->flags & IFF_MULTICAST);
   bool all_multi_needed = !!(netdev->flags & IFF_ALLMULTI);
   bool promisc_needed = !!(netdev->flags & IFF_PROMISC);
   struct mtip_netdev_priv *priv;
   u32 link_index;
   u64 hashtablebits = 0;
   int ret = 0;

   priv = netdev_priv(netdev);
   link_index = priv->link_index;

   CSMLOGDBG("mtip_rx_mode_set called for link_index: %d (%d, %d, %d)\n", link_index, is_multicast_enabled, all_multi_needed, promisc_needed);

 	if (promisc_needed) 
    {
        /* 
         * Set the PROMISC MODE if IFF_PROMISC is explicitly set 
         */ 
        // set the promiscous mode
        ret = mtip_mac_set_promisc_mode(priv, true);
        CSMLOGDBG("Setting promiscuous mode ON for link index: %d\n", link_index);
 	} 
    else if ((netdev_mc_count(netdev) > MTIP_MAC_HASHTABLE_SIZE) || (all_multi_needed))
    {
        ret = mtip_mac_set_promisc_mode(priv, true);
        CSMLOGDBG("Enabling all multicast for link index: %d\n", link_index);
 	} 
    else if (mtip_is_link_in_loopback(link_index) || ( link_index != MTIP_L2_ETH_LINK_INDEX && link_index != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode != MTIP_MODE_DEFAULT) ||
            ((link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX) && mtip_c2c2_loopback_mode != MTIP_MODE_DEFAULT ))
    {
        ret = mtip_mac_set_promisc_mode(priv, true);
        CSMLOGDBG("Setting promiscuous mode ON for loopback link index: %d\n", link_index);
    }
    else
    {
        if (netdev_mc_empty(netdev))
        {
            hashtablebits = 0;
        }
        else
        {
            mtip_generate_hashtablebits(netdev, &hashtablebits);
        }

        CSMLOGDBG("Setting up hashtable for multicast for link index: %d, original: 0x%lx, new: 0x%lx\n", link_index, priv->hashtablebits, hashtablebits);

        // reset promisc mode
        mtip_mac_set_promisc_mode(priv, false);

        mtip_configure_hashtable(priv, priv->hashtablebits, hashtablebits);

        priv->hashtablebits = hashtablebits;
    }

    /* update the topology only if there was an actual change */
    if (ret == 0)
    {
        /* Send update to clients */
        /* TBD - Need to post this event in wq context if needed in future */
        //mtip_client_send_event(ETH_ECPRISS_EVENT_UP, link_index);
    }

   return; 
}

/* Change the Maximum Transfer Unit (MTU) */
static int mtip_change_mtu(struct net_device *netdev, int new_mtu)
{
   unsigned long flags;
   struct mtip_netdev_priv *priv;
   spinlock_t *lock;
   u32 link_index;
   int mplane_mtu;
   int mtu_overhead = ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN;
   
   priv = (struct mtip_netdev_priv*)netdev_priv(netdev);
   lock = &(priv->lock);
   link_index = priv->link_index;

   CSMLOGDBG("mtip_change_mtu called for link index: %d, new_mtu: %d\n", link_index, new_mtu);

   /* check ranges */
   if ((new_mtu < MTIP_MIN_MTU_SIZE) || (new_mtu > MTIP_MAX_MTU_SIZE))
      return -EINVAL;

   /* Restrict the M Plane MTU to MAX FOR MPLANE */
   mplane_mtu = new_mtu;
   if (mplane_mtu > MTIP_MAX_MPLANE_MTU_SIZE) 
   {
       mplane_mtu = MTIP_MAX_MPLANE_MTU_SIZE;
   }

   spin_lock_irqsave(lock, flags);

   // set the netdev MTU
   netdev->mtu = mplane_mtu;

   spin_unlock_irqrestore(lock, flags);

   // set the frame length in the hardware
   mtip_mac_set_frame_length(priv, new_mtu + mtu_overhead);

   /* Send update to clients */
   /* TBD - Need to post this event in wq context if needed in future */
   //mtip_client_send_event(ETH_ECPRISS_EVENT_UP, link_index);

   return 0;
}

int mtip_device_open_completion(u32 link_index)
{
    struct net_device *netdev;
    u32 port_type;
    int sfp_port_type;
    ecpri_dma_eth_conn_hdl_t hdl;
    struct mtip_netdev_priv *priv;
    enum mtip_port_config_enum port_config;

    netdev = platform_driver_priv->mtip_links[link_index]->dev;

    priv = netdev_priv(netdev);
    hdl = platform_driver_priv->mtip_links[link_index]->dma_hdl;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return -1;
    }

    CSMLOGINFO("device open completion called for link_index %d", link_index);

    port_config = platform_driver_priv->mtip_ports[port_type]->port_config;

    // check if lanes have been assigned
    if (platform_driver_priv->mtip_links[link_index]->lanes_assignment_complete == false) 
    {
        CSMLOGERR("Lane assignment not complete for link_index %d", link_index);
        return 0;
    }
    else if (platform_driver_priv->mtip_links[link_index]->num_assigned_lanes == 0) 
    {
        CSMLOGINFO("No lanes assigned for link_index %d", link_index);

        mutex_lock(&platform_driver_priv->mtip_links[link_index]->dev_lock);

        platform_driver_priv->mtip_links[link_index]->state = MTIP_LINK_STATE_OPEN_FAILED;

        mutex_unlock(&platform_driver_priv->mtip_links[link_index]->dev_lock);
    }

    mutex_lock(&platform_driver_priv->mtip_links[link_index]->dev_lock);
    if(platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_CLOSE)
    {
      mutex_unlock(&platform_driver_priv->mtip_links[link_index]->dev_lock);
      return -1;
    }
    mutex_unlock(&platform_driver_priv->mtip_links[link_index]->dev_lock);

    // get the sfp port type; C2C links in loopback always use PORT_DA
    if ((link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX) &&
        mtip_c2c2_loopback_mode != MTIP_MODE_DEFAULT)
        sfp_port_type = PORT_DA;
    else
        sfp_port_type = platform_driver_priv->mtip_ports[port_type]->sfp_port_type;

    // Notify TRX driver to enable TX
    mtip_phy_notify_eth_event_to_trx(link_index, TRX_IFCONFIG_UP);

        // Bring down logging C2C2 port if needed
    // Skip this in E2E mode (mtip_c2c2_loopback_mode == DEFAULT) as eth30 is
    // a live C2C link and must not be torn down based on FH bandwidth.
    if (mtip_c2c2_loopback_mode != MTIP_MODE_DEFAULT)
        mtip_eth_reeval_logging_port();
    // bring up the phy
    mtip_phy_bringup_phy(link_index, sfp_port_type);

    CSMLOGINFO("phy bringup done for link: %d\n", link_index);

    mutex_lock(&platform_driver_priv->mtip_links[link_index]->dev_lock);

    /* 
     * set the link state to OPEN DONE * 
     */
    platform_driver_priv->mtip_links[link_index]->state = MTIP_LINK_STATE_OPEN_DONE;

    mutex_unlock(&platform_driver_priv->mtip_links[link_index]->dev_lock);

    /* update the security port config */
    mtip_device_update_security_config(netdev, port_config);

    /* Send update to clients */
    mtip_client_send_event(ETH_ECPRISS_EVENT_UP, link_index);

    return 0;
}

/* Called when the network interface is made active */
static int mtip_open(struct net_device *netdev)
{
   post_mtip_process_netdev_open(netdev);
   return 0;
}

/* Called when the network interface is disabled */
static int mtip_close(struct net_device *netdev)
{
   post_mtip_process_netdev_close(netdev);
   return 0;
}

/* Respond to a TX hang
 *  This function is called when a packet transmission fails to
 *   complete within a reasonable time. The driver will mark the error in the
 *   netdev structure and arrange for the device to be reset to a sane state
 *   in order to transmit a new packet.
 */
static void mtip_tx_timeout(struct net_device *netdev, unsigned int txqueue)
{
   struct mtip_netdev_priv* priv;
   u32 link_index;
   ecpri_dma_eth_conn_hdl_t hdl;
   int pending_buff_completion_count = 0;

   if (!netif_carrier_ok(netdev)) {
      CSMLOGERR("Interface is down, no need to process the timeout\n");
      return;
   }

   CSMLOGINFO("mtip_tx_timeout called\n");

   priv = netdev_priv(netdev);
   link_index = priv->link_index;
   hdl = platform_driver_priv->mtip_links[link_index]->dma_hdl;

   pending_buff_completion_count = mtip_device_get_buff_completion_count(netdev);

   if (pending_buff_completion_count < 0)
   {
       mtip_device_reset_buff_completion_count(netdev);
       pending_buff_completion_count = 0;
   }

   // check if we need to flow control the interface
   if (pending_buff_completion_count < (MTIP_TX_RING_SIZE - MTIP_TX_PACKET_AVAILABILITY_THRESHOLD))
   {
      if (netif_queue_stopped(netdev))
      {
         CSMLOGDBG("waking queue for link_index %d", link_index);

         // wake the queue
         netif_trans_update(netdev); /* prevent tx timeout */
         netif_wake_queue(netdev);
      }
   }

}

/* IOCTL support for the interface */
static int mtip_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
   int ret = -EOPNOTSUPP;
   struct mtip_netdev_priv* priv;
   u32 link_index;

   priv = netdev_priv(netdev);
   link_index = priv->link_index;

   CSMLOGDBG("mtip_ioctl called cmd: %d, link_index: %d\n", cmd, link_index);

   switch (cmd) 
   {
      case SIOCGHWTSTAMP:
      case SIOCSHWTSTAMP:
      {
         ret = mtip_ptp_handle_hwtstamp_ioctl(ifr, link_index);
         CSMLOGERR("cmd: %d, link_index: %d, netif_running %d, ret %d", cmd, link_index, netif_running(netdev), ret);
      }
      break;

      default:
         break;
   }

   return ret;
}

static int mtip_siocdevprivate(struct net_device *netdev, struct ifreq *ifr, void __user *udata, int cmd)
{
   int ret = -EOPNOTSUPP;
   struct mtip_netdev_priv* priv;
   u32 link_index;

   priv = netdev_priv(netdev);
   link_index = priv->link_index;

   CSMLOGDBG("mtip_siocdevprivate called cmd: %d, link_index: %d\n", cmd, link_index);

   if (!netif_running(netdev))
      return -EINVAL;

   return ret;
}

/* Provide network statistics info for the interface */
static void mtip_get_stats64(struct net_device *netdev,
			     struct rtnl_link_stats64 *net_stats)
{
   unsigned long flags;
   struct mtip_netdev_priv *priv;
   spinlock_t *lock;
   u32 link_index;

   priv = (struct mtip_netdev_priv*)netdev_priv(netdev);
   lock = &(priv->lock);
   link_index = priv->link_index;

   spin_lock_irqsave(lock, flags);

   memcpy(net_stats, &(platform_driver_priv->mtip_links[link_index]->net_stats), sizeof(struct rtnl_link_stats64));

   spin_unlock_irqrestore(lock, flags);
   return;
}

/*
 * mtip_netdev_header: fill the eth header 
 * Only needed for test purposes. Use default on target 
 */
int mtip_netdev_header(struct sk_buff *skb, struct net_device *dev,
		 unsigned short type, const void *daddr, const void *saddr,
                 unsigned int len) 
{
   struct mtip_netdev_priv *priv;
   int link_index;
   struct ethhdr *eth = (struct ethhdr *)skb_push(skb,ETH_HLEN);
   int other_link_index;
   struct net_device *other_dev = NULL;

   priv = (struct mtip_netdev_priv*)netdev_priv(dev);
   link_index = priv->link_index;

   if (link_index%2 == 0) 
   {
       other_link_index = link_index + 1;
   }
   else
   {
       other_link_index = link_index - 1;
   }

   other_dev = platform_driver_priv->mtip_links[other_link_index]->dev;

   CSMLOGDBG("mtip_netdev_header called link_index %d other_link_index %d\n", link_index, other_link_index);
   
   eth->h_proto = htons(type);
   memcpy(eth->h_source, dev->dev_addr, dev->addr_len);
   memcpy(eth->h_dest,   other_dev->dev_addr, other_dev->addr_len);
   return (dev->hard_header_len);
}

static const struct net_device_ops mtip_netdev_ops = {
	.ndo_open		      = mtip_open,
	.ndo_stop		      = mtip_close,
	.ndo_start_xmit	   = mtip_start_xmit,
	.ndo_change_mtu	   = mtip_change_mtu,
	.ndo_eth_ioctl		   = mtip_ioctl,
    .ndo_siocdevprivate = mtip_siocdevprivate,
	.ndo_tx_timeout	   = mtip_tx_timeout,
	.ndo_get_stats64	   = mtip_get_stats64,
	.ndo_set_rx_mode     = mtip_rx_mode_set,
   .ndo_set_mac_address = mtip_set_mac_address,
};

void mtip_netdevice_init(struct net_device *dev) 
{
   struct mtip_netdev_priv* priv;

   CSMLOGINFO("mtip_netdevice_init called 0x%lx\n", (unsigned long)dev);

   ether_setup(dev);

   // HANDLE THE ERROR

   dev->netdev_ops = &mtip_netdev_ops;
   priv = netdev_priv(dev);

   if (( (priv->link_index != MTIP_L2_ETH_LINK_INDEX && priv->link_index != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode != MTIP_MODE_DEFAULT) || ( (priv->link_index == MTIP_L2_ETH_LINK_INDEX || priv->link_index == MTIP_C2C1_ETH_LINK_INDEX) && mtip_c2c2_loopback_mode != MTIP_MODE_DEFAULT ) ) && !mtip_loopback_enable_arp)
   {
       /* add NOARP */
       dev->flags           |= IFF_NOARP;
   }

   dev->watchdog_timeo = MTIP_TIMEOUT;


   // initialize the lock
   spin_lock_init(&priv->lock);
   spin_lock_init(&priv->rx_lock);

   /*
    * set the ethtool ops
    */
   mtip_ethtool_set_ops(dev);
}

enum mtip_link_state_enum mtip_get_link_state_by_link_index(u32 link_index)
{
    if (platform_driver_priv->mtip_links[link_index] == NULL) {
        return MTIP_LINK_STATE_INIT;
    }
    else {
        return platform_driver_priv->mtip_links[link_index]->state;
    }
}

static void mtip_netdev_reconfigure_port
(
    u32 port_type,
    u32 num_links,
    u32 num_lanes,
    enum eth_phy_iface_phy_lane_speed_enum lane_speed,
    u32 real_link_index_array[MTIP_MAX_LINKS_PER_PORT],
    u32 real_lane_index_array[MTIP_MAX_LANES_PER_PORT],
    u32 lane_to_link_map[MTIP_MAX_LANES_PER_PORT]
)
{
    int i, j;
    struct mtip_port_info* port_info = platform_driver_priv->mtip_ports[port_type];
    struct mtip_link_info* link_info = NULL;
    u32 link_index;
    u32 lane_index;
    u32 real_link_index;
    u32 mapped_real_link_index;
    u32 real_lane_index;
    int lane_count;

    // first update the port lane_config
    for (i = 0; i < num_lanes; ++i) 
    {
        real_lane_index = real_lane_index_array[i];

        port_info->lane_config[real_lane_index].lane_enabled = true;

        port_info->lane_config[real_lane_index].lane_speed = lane_speed;

        mapped_real_link_index = lane_to_link_map[real_lane_index];

        if (mtip_lookup_link_index_by_port_type_and_real_link(&link_index, port_type, mapped_real_link_index) >= 0)
        {
            port_info->lane_config[real_lane_index].link_index = link_index;
        }
        else
        {
            port_info->lane_config[real_lane_index].link_index = -1;
        }
    }

    // update the assigned lane indices of all the links
    for (i = 0; i < num_links; ++i) 
    {
        real_link_index = real_link_index_array[i];

        lane_count = 0;

        if (mtip_lookup_link_index_by_port_type_and_real_link(&link_index, port_type, real_link_index) >= 0)
        {
            // we need to update the assigned lane indices of link_index
            link_info = platform_driver_priv->mtip_links[link_index];

            if(link_info == NULL)
                continue;

            for (j = 0; j < num_lanes; ++j) 
            {
                real_lane_index = real_lane_index_array[j];

                mapped_real_link_index = lane_to_link_map[real_lane_index];
                
                if (mapped_real_link_index == real_link_index) 
                {
                    // this lane is to be assigned to the link
                    if (mtip_lookup_lane_index_by_port_type_and_real_lane(&lane_index, port_type, real_lane_index) >= 0)
                    {
                        link_info->assigned_lane_indices[lane_count] = lane_index;
                    }
                    else
                    {
                        link_info->assigned_lane_indices[lane_count] = -1;
                        CSMLOGERR("invalid port_type %d and real_lane %d", port_type, real_lane_index);
                    }
                    ++lane_count;
                }
            }

            link_info->num_assigned_lanes = lane_count;
        }
        else
        {
            CSMLOGERR("unable to find link_index for port_type %d, real_link %d", port_type, real_link_index);
        }
    }
}

void mtip_netdev_assign_port_lanes(u32 port_type)
{
    int i;
    struct mtip_port_device_info *port_device = &platform_driver_priv->devices.port_devices[port_type];
    struct mtip_port_info* port_info = NULL;
    struct mtip_link_info* link_info = NULL;
    u32 link_index;
    enum mtip_port_config_enum port_config = platform_driver_priv->mtip_ports[port_type]->port_config;
    u32 num_links;
    u32 num_lanes;
    u32 real_link_index_array[MTIP_MAX_LINKS_PER_PORT];
    u32 real_lane_index_array[MTIP_MAX_LANES_PER_PORT];
    u32 lane_to_link_map[MTIP_MAX_LANES_PER_PORT];
    enum eth_phy_iface_phy_lane_speed_enum lane_speed;

    CSMLOGDBG("reconfiguring port %d to config %d", port_type, port_config);

    port_info = platform_driver_priv->mtip_ports[port_type];

    // clear the previous port/lane configuration
    for (i = 0; i < PHY_LANE_MAX; ++i) 
    {
        port_info->lane_config[i].lane_enabled = false;
        port_info->lane_config[i].lane_speed = 0;
        port_info->lane_config[i].link_index = 0;
    }

    // clear the lane config of all the links
    for (i = 0; i < port_device->num_link_phandles; ++i) 
    {
        link_index = port_device->link_devices[i]->link_index;

        link_info = platform_driver_priv->mtip_links[link_index];

        link_info->num_assigned_lanes = 0;
        link_info->lanes_assignment_complete = false;
    }

    // set the new port/lane configuration
    switch (port_config) 
    {
    case MTIP_PORT_CONFIG_1x100GBASE_R:
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC_LL:
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC:
        {
            num_links = 1;
            num_lanes = 1;
            lane_speed = PHY_LANE_SPEED_100G;

            // front haul ports
            if (port_type <= MTIP_PORT_TYPE_FH_2) 
            {
                real_link_index_array[0] = 0;

                real_lane_index_array[0] = 0;

                lane_to_link_map[0] = 0;
            }
            else if (port_type == MTIP_PORT_TYPE_L2) 
            {
                real_link_index_array[0] = 0;

                real_lane_index_array[0] = 0;

                lane_to_link_map[0] = 0;
            }
            else if (port_type == MTIP_PORT_TYPE_DEBUG) 
            {
                real_link_index_array[0] = 1;

                real_lane_index_array[0] = 2;

                lane_to_link_map[2] = 1;
            }
        }
        break;
    case MTIP_PORT_CONFIG_1x100GBASE_R2:
    case MTIP_PORT_CONFIG_1x100GBASE_R2_RSFEC:
        {
            num_links = 1;
            num_lanes = 2;
            lane_speed = PHY_LANE_SPEED_50G;

            // front haul ports
            if (port_type <= MTIP_PORT_TYPE_FH_2) 
            {
                real_link_index_array[0] = 0;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 1;

                lane_to_link_map[0] = 0;
                lane_to_link_map[1] = 0;
            }
            else if (port_type == MTIP_PORT_TYPE_L2) 
            {
                real_link_index_array[0] = 0;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 1;

                lane_to_link_map[0] = 0;
                lane_to_link_map[1] = 0;
            }
            else if (port_type == MTIP_PORT_TYPE_DEBUG) 
            {
                real_link_index_array[0] = 1;

                real_lane_index_array[0] = 2;
                real_lane_index_array[1] = 3;

                lane_to_link_map[2] = 1;
                lane_to_link_map[3] = 1;
            }
        }
        break;
    case MTIP_PORT_CONFIG_1x100GBASE_R4:
    case MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC:
        {
            num_links = 1;
            num_lanes = 4;
            lane_speed = PHY_LANE_SPEED_25G;

            // front haul ports
            if (port_type <= MTIP_PORT_TYPE_FH_2) 
            {
                real_link_index_array[0] = 0;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 1;
                real_lane_index_array[2] = 2;
                real_lane_index_array[3] = 3;

                lane_to_link_map[0] = 0;
                lane_to_link_map[1] = 0;
                lane_to_link_map[2] = 0;
                lane_to_link_map[3] = 0;
            }
            else 
            {
                CSMLOGERR("config %d not supported on port_type %d", port_config, port_type);
                goto out;
            }
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R:
    case MTIP_PORT_CONFIG_1x50GBASE_R_RSFEC:
        {
            num_links = 1;
            num_lanes = 1;
            lane_speed = PHY_LANE_SPEED_50G;

            // front haul ports
            if (port_type <= MTIP_PORT_TYPE_FH_2) 
            {
                real_link_index_array[0] = 0;

                real_lane_index_array[0] = 0;

                lane_to_link_map[0] = 0;
            }
            else if (port_type == MTIP_PORT_TYPE_L2) 
            {
                real_link_index_array[0] = 0;

                real_lane_index_array[0] = 0;

                lane_to_link_map[0] = 0;
            }
            else if (port_type == MTIP_PORT_TYPE_DEBUG) 
            {
                real_link_index_array[0] = 1;

                real_lane_index_array[0] = 2;

                lane_to_link_map[2] = 1;
            }
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R:
    case MTIP_PORT_CONFIG_2x50GBASE_R_RSFEC:
        {
            num_links = 2;
            num_lanes = 2;
            lane_speed = PHY_LANE_SPEED_50G;

            // front haul ports
            if (port_type <= MTIP_PORT_TYPE_FH_2) 
            {
                real_link_index_array[0] = 0;
                real_link_index_array[1] = 1;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 1;

                lane_to_link_map[0] = 0;
                lane_to_link_map[1] = 1;
            }
            else if (port_type == MTIP_PORT_TYPE_L2) 
            {
                real_link_index_array[0] = 0;
                real_link_index_array[1] = 1;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 2;

                lane_to_link_map[0] = 0;
                lane_to_link_map[2] = 1;
            }
            else if (port_type == MTIP_PORT_TYPE_DEBUG) 
            {
                real_link_index_array[0] = 0;
                real_link_index_array[1] = 1;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 2;

                lane_to_link_map[0] = 0;
                lane_to_link_map[2] = 1;
            }
        }
        break;
    case MTIP_PORT_CONFIG_1x50GBASE_R2:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_RSFEC:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI_FEC:
        {
            num_links = 1;
            num_lanes = 2;
            lane_speed = PHY_LANE_SPEED_25G;

            // front haul ports
            if (port_type <= MTIP_PORT_TYPE_FH_2) 
            {
                real_link_index_array[0] = 0;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 1;

                lane_to_link_map[0] = 0;
                lane_to_link_map[1] = 0;
            }
            else if (port_type == MTIP_PORT_TYPE_L2) 
            {
                real_link_index_array[0] = 0;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 1;

                lane_to_link_map[0] = 0;
                lane_to_link_map[1] = 0;
            }
            else if (port_type == MTIP_PORT_TYPE_DEBUG) 
            {
                real_link_index_array[0] = 1;

                real_lane_index_array[0] = 2;
                real_lane_index_array[1] = 3;

                lane_to_link_map[2] = 1;
                lane_to_link_map[3] = 1;
            }
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R2:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_FEC:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI_FEC:
        {
            num_links = 2;
            num_lanes = 4;
            lane_speed = PHY_LANE_SPEED_25G;

            // front haul ports
            if (port_type <= MTIP_PORT_TYPE_FH_2) 
            {
                real_link_index_array[0] = 0;
                real_link_index_array[0] = 1;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 1;
                real_lane_index_array[2] = 2;
                real_lane_index_array[3] = 3;

                lane_to_link_map[0] = 0;
                lane_to_link_map[1] = 0;
                lane_to_link_map[2] = 1;
                lane_to_link_map[3] = 1;
            }
            else if (port_type == MTIP_PORT_TYPE_L2) 
            {
                real_link_index_array[0] = 0;
                real_link_index_array[0] = 1;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 1;
                real_lane_index_array[2] = 2;
                real_lane_index_array[3] = 3;

                lane_to_link_map[0] = 0;
                lane_to_link_map[1] = 0;
                lane_to_link_map[2] = 1;
                lane_to_link_map[3] = 1;
            }
            else if (port_type == MTIP_PORT_TYPE_DEBUG) 
            {
                real_link_index_array[0] = 0;
                real_link_index_array[0] = 1;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 1;
                real_lane_index_array[2] = 2;
                real_lane_index_array[3] = 3;

                lane_to_link_map[0] = 0;
                lane_to_link_map[1] = 0;
                lane_to_link_map[2] = 1;
                lane_to_link_map[3] = 1;
            }
        }
        break;
    case MTIP_PORT_CONFIG_1x40GBASE_R4:
    case MTIP_PORT_CONFIG_1x40GBASE_R4_FEC:
        {
            num_links = 1;
            num_lanes = 4;
            lane_speed = PHY_LANE_SPEED_10G;

            // front haul ports
            if (port_type <= MTIP_PORT_TYPE_FH_2) 
            {
                real_link_index_array[0] = 0;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 1;
                real_lane_index_array[2] = 2;
                real_lane_index_array[3] = 3;

                lane_to_link_map[0] = 0;
                lane_to_link_map[1] = 0;
                lane_to_link_map[2] = 0;
                lane_to_link_map[3] = 0;
            }
            else 
            {
                CSMLOGERR("config %d not supported on port_type %d", port_config, port_type);
                goto out;
            }
        }
        break;
    case MTIP_PORT_CONFIG_1x25GBASE_R:
    case MTIP_PORT_CONFIG_1x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC:
        {
            num_links = 1;
            num_lanes = 1;
            lane_speed = PHY_LANE_SPEED_25G;

            // front haul ports
            if (port_type <= MTIP_PORT_TYPE_FH_2)
            {
                real_link_index_array[0] = 0;

                real_lane_index_array[0] = 0;

                lane_to_link_map[0] = 0;
            }
            else if (port_type == MTIP_PORT_TYPE_L2)
            {
                num_links = 2;
                num_lanes = 2;

                real_link_index_array[0] = 0;
                real_link_index_array[1] = 1;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 2;

                lane_to_link_map[0] = 0;
                lane_to_link_map[2] = 1;
            }
            else if (port_type == MTIP_PORT_TYPE_DEBUG)
            {
                real_link_index_array[0] = 1;

                real_lane_index_array[0] = 2;

                lane_to_link_map[2] = 1;
            }
        }
        break;
    case MTIP_PORT_CONFIG_4x25GBASE_R:
    case MTIP_PORT_CONFIG_4x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC:
        {
            num_links = 4;
            num_lanes = 4;
            lane_speed = PHY_LANE_SPEED_25G;

            // front haul ports
            if (port_type <= MTIP_PORT_TYPE_FH_2) 
            {
                real_link_index_array[0] = 0;
                real_link_index_array[1] = 1;
                real_link_index_array[2] = 2;
                real_link_index_array[3] = 3;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 1;
                real_lane_index_array[2] = 2;
                real_lane_index_array[3] = 3;

                lane_to_link_map[0] = 0;
                lane_to_link_map[1] = 1;
                lane_to_link_map[2] = 2;
                lane_to_link_map[3] = 3;
            }
            else if (port_type == MTIP_PORT_TYPE_L2) 
            {
                real_link_index_array[0] = 0;
                real_link_index_array[1] = 1;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 2;

                lane_to_link_map[0] = 0;
                lane_to_link_map[2] = 1;
            }
            else 
            {
                CSMLOGERR("config %d not supported on port_type %d", port_config, port_type);
                goto out;
            }
        }
        break;
    case MTIP_PORT_CONFIG_1x10GBASE_R:
    case MTIP_PORT_CONFIG_1x10GBASE_R_FEC:
        {
            num_links = 1;
            num_lanes = 1;
            lane_speed = PHY_LANE_SPEED_10G;

            // front haul ports
            if (port_type <= MTIP_PORT_TYPE_FH_2) 
            {
                real_link_index_array[0] = 0;

                real_lane_index_array[0] = 0;

                lane_to_link_map[0] = 0;
            }
            else if (port_type == MTIP_PORT_TYPE_L2) 
            {
                real_link_index_array[0] = 0;

                real_lane_index_array[0] = 0;

                lane_to_link_map[0] = 0;
            }
            else if (port_type == MTIP_PORT_TYPE_DEBUG) 
            {
                real_link_index_array[0] = 1;

                real_lane_index_array[0] = 2;

                lane_to_link_map[2] = 1;
            }
        }
        break;
    case MTIP_PORT_CONFIG_4x10GBASE_R:
    case MTIP_PORT_CONFIG_4x10GBASE_R_FEC:
        {
            num_links = 4;
            num_lanes = 4;
            lane_speed = PHY_LANE_SPEED_10G;

            // front haul ports
            if (port_type <= MTIP_PORT_TYPE_FH_2) 
            {
                real_link_index_array[0] = 0;
                real_link_index_array[1] = 1;
                real_link_index_array[2] = 2;
                real_link_index_array[3] = 3;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 1;
                real_lane_index_array[2] = 2;
                real_lane_index_array[3] = 3;

                lane_to_link_map[0] = 0;
                lane_to_link_map[1] = 1;
                lane_to_link_map[2] = 2;
                lane_to_link_map[3] = 3;
            }
            else if (port_type == MTIP_PORT_TYPE_L2)
            {
                real_link_index_array[0] = 0;
                real_link_index_array[1] = 1;

                real_lane_index_array[0] = 0;
                real_lane_index_array[1] = 2;

                lane_to_link_map[0] = 0;
                lane_to_link_map[2] = 1;
            }
            else 
            {
                CSMLOGERR("config %d not supported on port_type %d", port_config, port_type);
                goto out;
            }
        }
        break;
    default:
        {
            CSMLOGERR("Unknown port config %d", port_config);
            goto out;
        }
        break;
    }

    // reconfigure the port
    mtip_netdev_reconfigure_port(port_type, num_links,num_lanes,lane_speed,real_link_index_array,real_lane_index_array,lane_to_link_map);

    // set the port config
    platform_driver_priv->mtip_ports[port_type]->port_config = port_config;

    // complete the lane assignment to all the links of port
    for (i = 0; i < port_device->num_link_phandles; ++i) 
    {
        link_index = port_device->link_devices[i]->link_index;
        link_info = platform_driver_priv->mtip_links[link_index];
        link_info->lanes_assignment_complete = true;

        CSMLOGDBG("lane assignment complete for link_index %d", link_index);
    }

out:
    return;
}

int mtip_device_update_security_config(struct net_device *netdev, enum mtip_port_config_enum port_config)
{
    struct mtip_netdev_priv *mtip_priv = NULL;
    struct mtip_security_device *sdev = NULL;
    u32 port_type;
    u32 link_index;
    u32 num_links = 4;

    mtip_priv = (struct mtip_netdev_priv *)netdev_priv(netdev);
    sdev = mtip_priv->sec_dev;
    link_index = mtip_priv->link_index;

    mtip_lookup_port_type_by_link_index(link_index, &port_type);

    // set the new port/lane configuration
    switch (port_config) 
    {
    case MTIP_PORT_CONFIG_1x100GBASE_R:
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC_LL:
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_1x100GBASE_R2:
    case MTIP_PORT_CONFIG_1x100GBASE_R2_RSFEC:
    case MTIP_PORT_CONFIG_1x100GBASE_R4:
    case MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC:
    case MTIP_PORT_CONFIG_1x50GBASE_R:
    case MTIP_PORT_CONFIG_1x50GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_1x50GBASE_R2:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_RSFEC:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI_FEC:
    case MTIP_PORT_CONFIG_1x40GBASE_R4:
    case MTIP_PORT_CONFIG_1x40GBASE_R4_FEC:
    case MTIP_PORT_CONFIG_1x25GBASE_R:
    case MTIP_PORT_CONFIG_1x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_1x10GBASE_R:
    case MTIP_PORT_CONFIG_1x10GBASE_R_FEC:
        {
            num_links = 1;
        }
        break;
    case MTIP_PORT_CONFIG_2x50GBASE_R:
    case MTIP_PORT_CONFIG_2x50GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_2x50GBASE_R2:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_FEC:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI_FEC:
        {
            num_links = 2;
        }
        break;
    case MTIP_PORT_CONFIG_4x25GBASE_R:
    case MTIP_PORT_CONFIG_4x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_4x10GBASE_R:
    case MTIP_PORT_CONFIG_4x10GBASE_R_FEC:
        {
            num_links = 4;
        }
        break;
    default:
        {
            CSMLOGERR("Unknown port config %d", port_config);
        }
        break;
    }

    if (sdev) 
    {
        if (sdev->ops) 
        {
            if (sdev->ops->update_config) 
            {
                (sdev->ops->update_config)(sdev, num_links);
            }
        }
    }
    return num_links;
}

int mtip_netdev_setup_port_hw(u32 port_type)
{
    struct mtip_port_info* port_info = platform_driver_priv->mtip_ports[port_type];
    enum mtip_port_config_enum port_config = port_info->port_config;

    CSMLOGINFO("Setting up the port HW of port_type %d to %d str %s", port_type, port_config, mtip_ethtool_get_port_config_str(port_config));

    // setup ethernet based on the updated port config
    mtip_platform_setup_ethernet(port_type);

    // set the clock rates based on updated port config
    mtip_clocks_set_clock_rates(port_type, port_config);

    return 0;
}

// get the next ptp ts seq num to use
u8 mtip_netdev_get_next_ptp_ts_seq_num(u32 link_index)
{
    u8 ts_seq_num;
    u8 next_ts_seq_num;

    ts_seq_num = platform_driver_priv->mtip_links[link_index]->ptp_ts_seq_num;

    // the ts seq numbers are 3 bits
    next_ts_seq_num = (ts_seq_num + 1)%7;

    platform_driver_priv->mtip_links[link_index]->ptp_ts_seq_num = next_ts_seq_num;

    return ts_seq_num;
}

int mtip_netdev_set_port_priv_flags(struct net_device *netdev)
{
    struct mtip_netdev_priv *priv;
    u32 link_index;
    u32 port_priv_flags = 0;
    u32 port_type;
    u32 port_link0_index;

    priv = netdev_priv(netdev);
    link_index = priv->link_index;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return -1;
    }

    // Process only for link 0 of the port
    port_link0_index = platform_driver_priv->devices.port_devices[port_type].link_devices[0]->link_index;
    priv = netdev_priv(platform_driver_priv->mtip_links[port_link0_index]->dev);
    port_priv_flags = priv->priv_flags;
    if (link_index != port_link0_index)
    {
        CSMLOGERR("ignoring the default priv flags of link_index %d", link_index);
        return -1;
    }

     if(platform_driver_priv->mtip_ports[port_type]->port_priv_flags == port_priv_flags &&
        platform_driver_priv->mtip_ports[port_type]->autoneg_changed == false)
    {
        CSMLOGERR("No change in speed/autoneg for port %d", port_type);
        return -1;
    }

    // Reset the AN changed flag
    platform_driver_priv->mtip_ports[port_type]->autoneg_changed = false;

    CSMLOGDBG("Setting the port %d priv flags to %d", port_type, port_priv_flags);
    platform_driver_priv->mtip_ports[port_type]->port_priv_flags = port_priv_flags;
    platform_driver_priv->mtip_ports[port_type]->port_priv_flags_optical = 0;
    platform_driver_priv->mtip_ports[port_type]->next_speed_retry_count = 0;

    post_mtip_process_reconfigure_port(port_type);

    return 0;
}

int mtip_device_lookup_lane_qsfp_cfg(u32 port_type, struct qsfp_info *lane_qsfp_info)
{
    int i;
    u32 lane_index;
    bool connected_lane_found = false;
    struct mtip_lane_info* lane_info = NULL;

    if(!platform_driver_priv)
        return -1;

    // find a lane that is connected
    for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_lane_phandles; ++i) 
    {
        lane_index = platform_driver_priv->devices.port_devices[port_type].lane_devices[i]->lane_index;

        if (platform_driver_priv->mtip_lanes[lane_index] != NULL &&
            platform_driver_priv->mtip_lanes[lane_index]->lane_state == MTIP_LANE_STATE_CONNECTED) 
        {
            connected_lane_found = true;
            break;
        }
    }

    if (connected_lane_found == false) 
    {
        CSMLOGERR("No connected lane found for port_type %d", port_type);
        return -1;
    }

    lane_info = platform_driver_priv->mtip_lanes[lane_index];

    // set the lane_cfg
    *lane_qsfp_info = lane_info->lane_qsfp_info;
    return 0;
}

// filter the priv flags based on the current lane speed
// and SFP module attached
u32 mtip_device_filter_priv_flags(u32 port_type)
{
    u32 filtered = 0;
    int i;
    struct mtip_port_info* port_info = platform_driver_priv->mtip_ports[port_type];
    u32 port_priv_flags;
    u32 lane_index;
    bool connected_lane_found = false;
    u8  lane_speed_mask;
    struct qsfp_info lane_qsfp_info;
    u32 num_lanes;
    u32 filtered_mask = 0;
    u32 real_link = 0;
    u32 link_index;

    if(!port_info)
        return 0;
    else
        port_priv_flags = port_info->port_priv_flags;

    // find a lane that is connected
    for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_lane_phandles; ++i) 
    {
        lane_index = platform_driver_priv->devices.port_devices[port_type].lane_devices[i]->lane_index;

        if (platform_driver_priv->mtip_lanes[lane_index] != NULL &&
            platform_driver_priv->mtip_lanes[lane_index]->lane_state == MTIP_LANE_STATE_CONNECTED) 
        {
            connected_lane_found = true;
            break;
        }
    }

    if (connected_lane_found == false) 
    {
        CSMLOGERR("No connected lane found for port_type %d", port_type);
        return 0;
    }

    // This is mask of all the supported speed modes by the TRX module
    lane_speed_mask = platform_driver_priv->mtip_lanes[lane_index]->speed_mask;

    if (mtip_device_lookup_lane_qsfp_cfg(port_type, &lane_qsfp_info) < 0)
    {
        CSMLOGERR("unable to lookup lane cfg of port_type %d", port_type);
        return 0;
    }

    // use the number of lanes to decide the supported port configurations
    if(port_type == MTIP_PORT_TYPE_DEBUG)
    {
        if(lane_qsfp_info.trx_module_type == TRX_QSFPDD)
        {
            // for QSFPDD on DU, bits at index 6 and 7 map to Debug port lanes
            if((lane_qsfp_info.trx_laneinfo & 0xC0) == 0xC0)
                num_lanes = 2;
            else if((lane_qsfp_info.trx_laneinfo & 0x40) == 0x40)
                num_lanes = 1;
            else
                return 0;
        }
        else
        {
            // for other modules, bits at index 2 and 3 map to Debug port lanes
            if((lane_qsfp_info.trx_laneinfo & 0xC) == 0xC)
                num_lanes = 2;
            else if((lane_qsfp_info.trx_laneinfo & 0x4) == 0x4)
                num_lanes = 1;
            else
                return 0;
        }
    } 
    else if(port_type == MTIP_PORT_TYPE_L2)
    {
        if((lane_qsfp_info.trx_laneinfo & 0x3) == 0x3)
            num_lanes = 2;
        else if((lane_qsfp_info.trx_laneinfo & 0x1) == 0x1)
            num_lanes = 1;
        else
        {
            CSMLOGERR("Invalid trx_laneinfo 0x%x", lane_qsfp_info.trx_laneinfo);
            return 0;
        }
    }
    else
    {
        // For FH ports, bits 0 to 3 will map to the 4 lanes of the port
        if((lane_qsfp_info.trx_laneinfo & 0xF) == 0xF)
            num_lanes = 4;
        else if((lane_qsfp_info.trx_laneinfo & 0x3) == 0x3)
            num_lanes = 2;
        else if((lane_qsfp_info.trx_laneinfo & 0x1) == 0x1)
            num_lanes = 1;
        else
        {
            CSMLOGERR("Invalid trx_laneinfo 0x%x", lane_qsfp_info.trx_laneinfo);
            return 0;
        }
    }

    // TBD, need to use FEC setting as well to filter out supported speed modes
    if(port_type == MTIP_PORT_TYPE_DEBUG)
    {
        real_link = 1;
    }

    // Get the link index of the first link for this port
    if(mtip_lookup_link_index_by_port_type_and_real_link(&link_index, port_type, real_link) != 0)
    {
        return 0;
    }

    if(lane_speed_mask & TRX_LANE_SPEED_100G)
    {
       filtered_mask |= (1 << MTIP_PORT_CONFIG_1x100GBASE_R);
    }

    if(lane_speed_mask & TRX_LANE_SPEED_50G)
    {
        if(num_lanes == 1 || port_type == MTIP_PORT_TYPE_L2)
        {
           filtered_mask |= (1 << MTIP_PORT_CONFIG_1x50GBASE_R);
        }
        if(num_lanes == 2 || num_lanes == 4)
        {
           // TBD - need to enhance breakout handling
	   if(port_type == MTIP_PORT_TYPE_L2)
	   {
               filtered_mask |= (1 << MTIP_PORT_CONFIG_1x100GBASE_R2);
	   }
	   else
	   {
               filtered_mask |= ((1 << MTIP_PORT_CONFIG_1x100GBASE_R2)|
                                 (1 << MTIP_PORT_CONFIG_2x50GBASE_R));
	   }
        }
    }

    if(lane_speed_mask & TRX_LANE_SPEED_25G)
    {
       if(num_lanes == 1)
       {
          filtered_mask |= (1 << MTIP_PORT_CONFIG_1x25GBASE_R);
       }
       else if(num_lanes == 2)
       {
          // TBD - need to enhance breakout handling
          filtered_mask |= ((1 << MTIP_PORT_CONFIG_1x50GBASE_R2)|
                            (1 << MTIP_PORT_CONFIG_1x25GBASE_R));
       }
       else if(num_lanes == 4)
       {
          // TBD - need to enhance breakout handling
          filtered_mask |= ((1 << MTIP_PORT_CONFIG_1x100GBASE_R4) |
                            (1 << MTIP_PORT_CONFIG_1x50GBASE_R2)|
                            (1 << MTIP_PORT_CONFIG_4x25GBASE_R));
       }
    }

    if(lane_speed_mask & TRX_LANE_SPEED_10G)
    {
       if(num_lanes == 1 || num_lanes == 2)
       {
          filtered_mask |= (1 << MTIP_PORT_CONFIG_1x10GBASE_R);
       }
       else if(num_lanes == 4)
       {
          // TBD - need to enhance breakout handling, where we need to exclude 1x40_R4
          filtered_mask |= ((1 << MTIP_PORT_CONFIG_4x10GBASE_R) |
                            (1 << MTIP_PORT_CONFIG_1x40GBASE_R4));
       }
    }

    filtered = port_priv_flags & filtered_mask;

    /* For DR, we currently support only QSFP28 100G DR1, so only 100G_R4 will
       be supported, and other modes will be masked out */
    if(lane_qsfp_info.trx_link_length_range == TRX_DR)
       filtered &= (1 << MTIP_PORT_CONFIG_1x100GBASE_R4);

    CSMLOGINFO("mtip_device_filter_priv_flags: Port_type %d, lane %d, lane_speed_mask 0x%x, port_priv_flags 0x%x, filtered_mask 0x%x, filtered 0x%x",
               port_type, lane_index, lane_speed_mask, port_priv_flags, filtered_mask, filtered);

    return filtered;
}

int mtip_device_count_priv_flag_bits(u32 port_type)
{
    int i;
    int ret = 0;
    u32 pattern = 0x1;
    u32 pflags = mtip_device_filter_priv_flags(port_type);

    for (i = 0; i < MTIP_PORT_CONFIG_MAX; ++i) 
    {
        if ((pflags & pattern) != 0) 
        {
            ++ret;
        }
        pattern = pattern << 1;
    }
    return ret;
}

// use the port priv flags to find a port configuration to use
static u32 mtip_device_resolve_port_configuration(u32 port_type)
{
    int i;
    u32 port_config_mask = 0, final_port_config_mask = 0;
    bool found = false;
    u32 pattern = 0x1;
    u32 pflags = mtip_device_filter_priv_flags(port_type);
    u32 link_index;
    u32 real_link = 0;
    u32 config_fec = 0;
    enum mtip_port_config_enum port_config;
    struct mtip_port_info *port_info = platform_driver_priv->mtip_ports[port_type];
    if(port_type == MTIP_PORT_TYPE_DEBUG)
      real_link = 1;

    // Get the link index of the first link for this port
    if(mtip_lookup_link_index_by_port_type_and_real_link(&link_index, port_type, real_link) < 0)
        return MTIP_PORT_CONFIG_MAX;

    /* port_priv_flags_optical will be configured with the truncated set of
       speed modes on which attempt needs to be made for dual rate modules */
    if(platform_driver_priv->mtip_ports[port_type]->sfp_port_type == PORT_FIBRE &&
       platform_driver_priv->mtip_ports[port_type]->port_priv_flags_optical != 0)
    {
        pflags &= platform_driver_priv->mtip_ports[port_type]->port_priv_flags_optical;
    }

    if (port_info->autoneg == false || platform_driver_priv->mtip_ports[port_type]->sfp_port_type == PORT_FIBRE)
    {
        // find the first port config bit that is set
        for (i = 0; i < MTIP_PORT_CONFIG_MAX; ++i)
        {
            if ((pflags & pattern) != 0)
            {
                found = true;
                port_config_mask = 1 << i;
                break;
            }
            pattern = pattern << 1;
        }
        if (found == false)
        {
            CSMLOGERR("No priv flags %d ON. Ignoring", pflags);
            return MTIP_PORT_CONFIG_MAX;
        }
    }
    else
        port_config_mask = pflags;

    config_fec = platform_driver_priv->mtip_links[link_index]->config_fec;
    pattern = 0x1;
    port_config = MTIP_PORT_CONFIG_1x100GBASE_R;
    while(port_config_mask)
    {
        if(port_config_mask & pattern)
        {
            switch (port_config)
            {
                case MTIP_PORT_CONFIG_1x25GBASE_R:
                    {
                        if( (port_info->sfp_port_type == PORT_FIBRE) && ((config_fec == ETHTOOL_FEC_RS)
                        || (config_fec == ETHTOOL_FEC_NONE)) )
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC;
                        else if(config_fec == ETHTOOL_FEC_RS)
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC;
                        else if (config_fec == ETHTOOL_FEC_BASER)
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x25GBASE_R_FEC;
                        else
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x25GBASE_R;
                    }
                    break;
                case MTIP_PORT_CONFIG_4x25GBASE_R:
                    {
                        if( (port_info->sfp_port_type == PORT_FIBRE) && ((config_fec == ETHTOOL_FEC_RS)
                        || (config_fec == ETHTOOL_FEC_NONE)) )
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC;
                        else if(config_fec == ETHTOOL_FEC_RS)
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC;
                        else if (config_fec == ETHTOOL_FEC_BASER)
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_4x25GBASE_R_FEC;
                        else
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_4x25GBASE_R;
                    }
                    break;
                case MTIP_PORT_CONFIG_4x10GBASE_R:
                    {
                        if (config_fec == ETHTOOL_FEC_BASER)
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_4x10GBASE_R_FEC;
                        else
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_4x10GBASE_R;
                    }
                    break;
                case MTIP_PORT_CONFIG_1x10GBASE_R:
                    {
                        if (config_fec == ETHTOOL_FEC_BASER)
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x10GBASE_R_FEC;
                        else
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x10GBASE_R;
                    }
                    break;
                case MTIP_PORT_CONFIG_1x40GBASE_R4:
                    {
                        if (config_fec == ETHTOOL_FEC_BASER)
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x40GBASE_R4_FEC;
                        else
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x40GBASE_R4;
                    }
                    break;
                case MTIP_PORT_CONFIG_1x100GBASE_R4:
                    {
                        /* For 100G_R4, default mode to be used is with RSFEC enabled
                       unless set as OFF by ethtool set-priv-flags, or if
                       DR module is used */
                        if( (mtip_phy_get_trx_link_length_range(&platform_driver_priv->devices.port_devices[port_type]) != TRX_DR)
                        && (config_fec == ETHTOOL_FEC_RS || config_fec == ETHTOOL_FEC_BASER || config_fec == ETHTOOL_FEC_NONE) )
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC;
                        else if( (mtip_phy_get_trx_link_length_range(&platform_driver_priv->devices.port_devices[port_type]) == TRX_DR) && (config_fec == ETHTOOL_FEC_RS) )
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC;
                        else
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x100GBASE_R4;
                    }
                    break;
                case MTIP_PORT_CONFIG_2x50GBASE_R:
                    {
                        final_port_config_mask |= 1 << MTIP_PORT_CONFIG_2x50GBASE_R;
                    }
                    break;
                case MTIP_PORT_CONFIG_1x50GBASE_R:
                    {
                        final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x50GBASE_R;
                    }
                    break;
                case MTIP_PORT_CONFIG_1x100GBASE_R2:
                    {
                        final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x100GBASE_R2;
                    }
                    break;
                case MTIP_PORT_CONFIG_1x50GBASE_R2:
                    {
                        if(config_fec == ETHTOOL_FEC_RS)
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x50GBASE_R2_RSFEC;
                        else
                            final_port_config_mask |= 1 << MTIP_PORT_CONFIG_1x50GBASE_R2;
                    }
                    break;
                default:
                    CSMLOGERR("default:final_port_config_mask: 0x%x,config_fec=%d\n", final_port_config_mask,config_fec);
                    break;
            }
        }
        port_config_mask = port_config_mask >> 1;
        port_config++;
    }

    CSMLOGINFO("selected port_config: 0x%x, link_index: %d\n", final_port_config_mask,link_index);
    return final_port_config_mask;
}

static int mtip_device_complete_port_open(u32 port_type)
{
    int rv = 0;
    u32 link_index;
    int i;
    int sfp_port_type;

    // check if any links are waiting to complete open
    for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_link_phandles; ++i)
    {
       link_index = platform_driver_priv->devices.port_devices[port_type].link_devices[i]->link_index;

       if (platform_driver_priv->mtip_links[link_index] != NULL)
       {
          if ( ( (link_index != MTIP_L2_ETH_LINK_INDEX && link_index != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode != MTIP_MODE_LOOPBACK ) || ( (link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX) &&  mtip_c2c2_loopback_mode != MTIP_MODE_C2C2_LOOPBACK)) &&
              (platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_OPEN_WAITING_FOR_LANES ||
               platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_DOWN))
          {
             rv = mtip_device_open_completion(link_index);
          }
          else if ((link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX) &&
                   mtip_c2c2_loopback_mode != MTIP_MODE_C2C2_LOOPBACK &&
                   (platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_INIT ||
                    platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_CLOSE) &&
                   platform_driver_priv->mtip_links[link_index]->lanes_assignment_complete == true)
          {
             sfp_port_type = PORT_DA;
             CSMLOGINFO("Pre-enabling PCS TX for closed C2C link_index %d sfp_type %d",
                        link_index, sfp_port_type);
             mtip_phy_bringup_phy(link_index, sfp_port_type);
          }
       }
    }

    return rv;
}

// find the highest number of lanes based on the priv flags set for the port
static int mtip_device_calculate_num_an_lanes(u32 port_type)
{
   int i;
   u32 pattern = 0x1;
   u32 config_bit;
   int num_an_lanes = 0;
   int config_lanes = 0;
   u32 port_priv_flags = mtip_device_filter_priv_flags(port_type);

   for (i = 0; i < MTIP_PORT_CONFIG_MAX; ++i)
   {
      config_bit = port_priv_flags & pattern;

      if (config_bit != 0x0)
      {
         config_lanes = 0;

         // find the number of lanes for the config
         switch (i)
         {
            // configs requiring all four lanes
         case MTIP_PORT_CONFIG_1x100GBASE_R4:
         case MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC:
         case MTIP_PORT_CONFIG_2x50GBASE_R2:
         case MTIP_PORT_CONFIG_2x50GBASE_R2_FEC:
         case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI:
         case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI_FEC:
         case MTIP_PORT_CONFIG_1x40GBASE_R4:
         case MTIP_PORT_CONFIG_1x40GBASE_R4_FEC:
         case MTIP_PORT_CONFIG_4x25GBASE_R:
         case MTIP_PORT_CONFIG_4x25GBASE_R_FEC:
         case MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC:
         case MTIP_PORT_CONFIG_4x10GBASE_R:
         case MTIP_PORT_CONFIG_4x10GBASE_R_FEC:
            config_lanes = 4;
            break;

            // configs requiring two lanes
         case MTIP_PORT_CONFIG_1x100GBASE_R2:
         case MTIP_PORT_CONFIG_1x100GBASE_R2_RSFEC:
         case MTIP_PORT_CONFIG_2x50GBASE_R:
         case MTIP_PORT_CONFIG_2x50GBASE_R_RSFEC:
         case MTIP_PORT_CONFIG_1x50GBASE_R2:
         case MTIP_PORT_CONFIG_1x50GBASE_R2_RSFEC:
         case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI:
         case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI_FEC:
            config_lanes = 2;
            break;

            // configs requiring only one lane
         case MTIP_PORT_CONFIG_1x100GBASE_R:
         case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC_LL:
         case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC:
         case MTIP_PORT_CONFIG_1x50GBASE_R:
         case MTIP_PORT_CONFIG_1x50GBASE_R_RSFEC:
         case MTIP_PORT_CONFIG_1x25GBASE_R:
         case MTIP_PORT_CONFIG_1x25GBASE_R_FEC:
         case MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC:
         case MTIP_PORT_CONFIG_1x10GBASE_R:
         case MTIP_PORT_CONFIG_1x10GBASE_R_FEC:
            config_lanes = 1;
            break;
         }

         // update num_an_lanes if the new config requires higher
         if (num_an_lanes < config_lanes)
         {
            num_an_lanes = config_lanes;
         }

         // break if we have reached maximum
         if (num_an_lanes == 4)
         {
            break;
         }
      }

      pattern = pattern << 1;
   }
   return num_an_lanes;
}

// There has been some change in the configuration of a port
// this is the common function to handle all such (re)configurations
void mtip_device_configure_port(u32 port_type)
{
   u32 tmp_lane_index;
   bool all_lanes_connected = true;
   bool breakout_lane_connected = false;
   int i;
   struct mtip_port_info *port_info;
   bool set_port_config = false;
   u32 num_links_waiting_for_lanes = 0;
   u32 link_index = 0;
   bool loopflag = true;
   bool port_has_loopback_link = false, port_already_configured_for_loopback = false;
   bool waiting_link_is_loopback = false;
   int bc = 0;
   int num_an_lanes = 0;
   u32 filtered_priv_flags = 0, port_config_mask = 0;
   u32 real_link = 0;
   u32 real_lane = 0;
   u32 lane_index;
   u32 sfp_phandle[MTIP_MAX_LANES_PER_PORT] = {0};
   struct qsfp_info lane_qsfp_info;
   u32 pattern = 0x1;
   enum mtip_port_config_enum port_config;
   struct trx_eth_event_t trx_event_info = {0};

   if(platform_driver_priv == NULL || platform_driver_priv->mtip_ports[port_type] == NULL)
      goto out;

   port_info = platform_driver_priv->mtip_ports[port_type];

   loopflag = true;

   while (loopflag)
   {
      CSMLOGDBG("port %d in state %d", port_type, port_info->port_state);

      switch (port_info->port_state)
      {
      case MTIP_PORT_STATE_INIT:
         {
            // check if this is a breakout cable case
            if (mtip_phy_is_breakout_config(port_type))
            {
                // this is a breakout cable, check that if any one lane of the port is in connected state
               for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_lane_phandles; ++i)
               {
                  tmp_lane_index = platform_driver_priv->devices.port_devices[port_type].lane_devices[i]->lane_index;

                  if (platform_driver_priv->mtip_lanes[tmp_lane_index]->lane_state == MTIP_LANE_STATE_CONNECTED)
                  {
                     breakout_lane_connected = true;
                     break;
                  }
               }

               if(breakout_lane_connected == false)
               {
                   CSMLOGINFO("Waiting for at least one breakout lane for port: %d to be connected", port_type);

                   // stay in INIT state
                   loopflag = false;
                   goto out;
               }
               else
                   CSMLOGINFO("At least one breakout lane for port: %d is connected", port_type);
            }
            else
            {
               // this is not a breakout cable
               CSMLOGDBG("Not breakout cable case");

               if (mtip_device_lookup_lane_qsfp_cfg(port_type, &lane_qsfp_info) < 0)
               {
                  // stay in INIT state
                  loopflag = false;
                  goto out;
               }

               // check that all the lanes of the port are in connected state
               all_lanes_connected = true;
               for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_lane_phandles; ++i)
               {
                  tmp_lane_index = platform_driver_priv->devices.port_devices[port_type].lane_devices[i]->lane_index;

                  // Don't check for the lanes which are not valid
                  if(((1 << i) & lane_qsfp_info.trx_laneinfo) == 0)
                     continue;

                  if (platform_driver_priv->mtip_lanes[tmp_lane_index]->lane_state != MTIP_LANE_STATE_CONNECTED)
                  {
                     all_lanes_connected = false;
                     break;
                  }
               }

               if (all_lanes_connected == false)
               {
                  CSMLOGINFO("Waiting for lanes to be connected");

                  // stay in INIT state
                  loopflag = false;
                  goto out;
               }

               // all lanes of port are connected
               CSMLOGINFO("All lanes of port: %d are connected", port_type);
            }

            // set the port to CONNECTED state and continue loop
            port_info->port_state = MTIP_PORT_STATE_CONNECTED;
         }
         break;

      case MTIP_PORT_STATE_CONNECTED:
         {
            port_has_loopback_link = false;
            waiting_link_is_loopback = false;

            // check if we can reconfigure the port
            set_port_config = true;
            num_links_waiting_for_lanes = 0;

            // Check if any link in the port is in loopback mode, and if the link
            // waiting for lanes (trying to open) is also a loopback link
            for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_link_phandles; ++i)
            {
               u32 tmp_link_idx = platform_driver_priv->devices.port_devices[port_type].link_devices[i]->link_index;
               if (platform_driver_priv->mtip_links[tmp_link_idx] != NULL &&
                   mtip_is_link_in_loopback(tmp_link_idx))
               {
                  port_has_loopback_link = true;
                  if (platform_driver_priv->mtip_links[tmp_link_idx]->state == MTIP_LINK_STATE_OPEN_WAITING_FOR_LANES)
                  {
                     waiting_link_is_loopback = true;
                     CSMLOGINFO("Link %d is waiting for lanes and is in loopback mode", tmp_link_idx);
                  }
               }
               if (port_has_loopback_link && waiting_link_is_loopback)
                  break;
            }

            // port can be reconfigured only if there are no links already open
            // and there is atleast one link waiting for lane assignment
            // EXCEPTION: If a loopback link is trying to open, allow it even if non-loopback links are up
            for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_link_phandles; ++i)
            {
               link_index = platform_driver_priv->devices.port_devices[port_type].link_devices[i]->link_index;

               if (platform_driver_priv->mtip_links[link_index] != NULL)
               {
                  // we cannot set the port config if link is already up (non-loopback case)
                  if ((platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_INIT) &&
                      (platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_CLOSE) &&
                      (platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_OPEN_WAITING_FOR_LANES) &&
                      (platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_DOWN))
                  {
                     // EXCEPTION: If a loopback link is waiting to open, skip blocking for already-up links
                     if (waiting_link_is_loopback)
                     {
                        CSMLOGDBG("Skipping set_port_config=false for link %d because loopback link is waiting", link_index);
                        // Don't set set_port_config = false
                     }
                     else
                     {
                        set_port_config = false;
                        break;
                     }
                  }

                  if (platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_OPEN_WAITING_FOR_LANES ||
                      platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_DOWN)
                  {
                     ++num_links_waiting_for_lanes;
                  }
               }
            }

            if (set_port_config == false)
            {
               CSMLOGINFO("not setting up the port for now");

               // stay in connected state and exit loop
               loopflag = false;
               goto out;
            }

            if (num_links_waiting_for_lanes == 0)
            {
               // there are no links waiting to be assigned lanes
               CSMLOGINFO("no links waiting for lane assignment or to be brought up,link_index=%d,link_state=%d\n",link_index,platform_driver_priv->mtip_links[link_index]->state);

               // stay in connected and exit loop
               loopflag = false;
               goto out;
            }
            else
            {
               // we can reconfigure the port and we have links waiting for lane assignment

               // For loopback mode: If port is already configured, skip to link completion
               if (port_has_loopback_link &&
                   platform_driver_priv->mtip_ports[port_type]->port_config != MTIP_PORT_CONFIG_MAX)
               {
                  CSMLOGINFO("Port %d already configured for loopback (config=%d), proceeding to link completion",
                             port_type, platform_driver_priv->mtip_ports[port_type]->port_config);
                  loopflag = false;
                  goto resolved;
               }

               // check if an optical is connected
               if (platform_driver_priv->mtip_ports[port_type]->sfp_port_type == PORT_FIBRE || port_info->autoneg == false)
               {
                  // we cannot do auto-negotiation
                  // resolve port configuration manually
                  port_config_mask =  mtip_device_resolve_port_configuration(port_type);
                  if (port_config_mask == MTIP_PORT_CONFIG_MAX)
                  {
                     if (platform_driver_priv->mtip_ports[port_type]->sfp_port_type == PORT_FIBRE)
                        CSMLOGERR("Unable to resolve optical port configuration for port: %d", port_type);
                     else
                        CSMLOGERR("Unable to resolve copper port configuration for port: %d", port_type);
                     loopflag = false;
                     goto out;
                  }
                  else
                  {
                     // we have a resolved port configuration
                     // we can go ahead with the configuration
                     pattern = 0x1;
                     port_config = MTIP_PORT_CONFIG_1x100GBASE_R;
                     while(port_config_mask)
                     {
                        if(port_config_mask & pattern)
                           break;
                        pattern = pattern << 1;
                        port_config++;
                     }
                     port_info->port_config = port_config;
                     loopflag = false;
                     goto resolved;
                  }
               }
               else
               {
                  // count the number of bits set in priv_flags
                  bc = mtip_device_count_priv_flag_bits(port_type);

                  if (bc == 0)
                  {
                     CSMLOGERR("Unable to resolve copper port configuration for port: %d", port_type);
                     loopflag = false;
                     goto out;
                  }
                  else
                  {
                     if (port_info->autoneg == true)
                     {
                        CSMLOGINFO("going to initiate AN for port_type %d", port_type);

                        // we need to do AN
                        // set the port state to AN in progress and continue loop
                        port_info->port_state = MTIP_PORT_STATE_CONNECTED_INITIATE_AN;
                     }
                  }
               }
            }
         }
         break;

      case MTIP_PORT_STATE_CONNECTED_INITIATE_AN:
         {
            port_info->port_state = MTIP_PORT_STATE_CONNECTED_NEGOTIATION_IN_PROGRESS;

            num_an_lanes = mtip_device_calculate_num_an_lanes(port_type);

            CSMLOGINFO("initiating AN on port_type %d with num lanes %d and priv_flags %d", port_type, num_an_lanes,
                       platform_driver_priv->mtip_ports[port_type]->port_priv_flags);

            filtered_priv_flags = mtip_device_resolve_port_configuration(port_type);
            if (filtered_priv_flags == 0)
            {
               CSMLOGERR("No priv flags %d ON for AN mode\n");
               loopflag = false;
               goto out;
            }
            if(port_type == MTIP_PORT_TYPE_DEBUG)
            {
               real_link = 1;
               real_lane = 2;
            }

            // Notify TRX driver to enable TX, for the primary lane used for AN
            mtip_lookup_lane_index_by_port_type_and_real_lane(&lane_index, port_type, real_lane);
            sfp_phandle[real_lane] = platform_driver_priv->devices.lane_devices[lane_index].sfp_phandle;
            trx_event_info.event = TRX_IFCONFIG_UP;
            trx_event_info.lane_phandle = sfp_phandle;
            trx_event_info.num_lanes = 1;
            trx_event_info.eth_cfg_speed = TRX_LANE_SPEED_UNKNOWN;
            qsfp_trx_eth_event_notifier(&trx_event_info);

            // initiate AN with the PHY
            mtip_phy_initiate_an(port_type, num_an_lanes, filtered_priv_flags);

            loopflag = false;
            goto out;
         }
         break;

      case MTIP_PORT_STATE_CONNECTED_NEGOTIATION_IN_PROGRESS:
         {
            CSMLOGDBG("staying in state %d until AN callback", port_info->port_state);

            loopflag = false;
            goto out;
         }
         break;

      case MTIP_PORT_STATE_CONNECTED_NEGOTIATION_DONE:
         {
            CSMLOGINFO("Negotiation done. Moving back to CONNECTED state for port_type %d", port_type);

            // set the port back to CONNECTED
            port_info->port_state = MTIP_PORT_STATE_CONNECTED;

            // negotiation was successful
            // we can go ahead with the configuration
            loopflag = false;
            goto resolved;
         }
         break;

      case MTIP_PORT_STATE_DISCONNECTED:
         {
            CSMLOGERR("port_type %d is in DISCONNECTED state");
            loopflag = false;
            goto out;
         }
         break;

      default:
         {
            CSMLOGERR("port_type %d in unknown state %d", port_type, port_info->port_state);
            loopflag = false;
            goto out;
         }
         break;
      }

      if (loopflag)
      {
         // keep looping
         CSMLOGDBG("looping to process port_type %d new state %d", port_type, port_info->port_state);
      }
      else
      {
         // exit loop
         CSMLOGDBG("exiting loop port_type %d state %d", port_type, port_info->port_state);
      }
   }

resolved:
   CSMLOGINFO("Negotiated port configuration for port %d is %d %s", port_type,
              port_info->port_config,
              mtip_ethtool_get_port_config_str(port_info->port_config));

   // Check if this is a subsequent loopback link configuration
   port_already_configured_for_loopback = false;
   if (port_has_loopback_link)
   {
      // Check if any loopback link is already in OPEN_DONE or UP state
      for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_link_phandles; ++i)
      {
         u32 tmp_link_idx = platform_driver_priv->devices.port_devices[port_type].link_devices[i]->link_index;
         if (platform_driver_priv->mtip_links[tmp_link_idx] != NULL &&
             mtip_is_link_in_loopback(tmp_link_idx) &&
             (platform_driver_priv->mtip_links[tmp_link_idx]->state == MTIP_LINK_STATE_OPEN_DONE ||
              platform_driver_priv->mtip_links[tmp_link_idx]->state == MTIP_LINK_STATE_UP))
         {
            port_already_configured_for_loopback = true;
            CSMLOGINFO("Port %d already has active loopback link %d", port_type, tmp_link_idx);
            break;
         }
      }
   }

   // Reset PHY SM for optical if any old configuration was active earlier
   // Skip for subsequent loopback links
   if (!port_already_configured_for_loopback &&
       (port_info->sfp_port_type == PORT_FIBRE || port_info->autoneg == false))
   {
      mtip_phy_reset_phy_sm(port_type);
   }

   // Assign lanes to links and setup MAC/PCS/Wrapper HW blocks of the port
   // Skip for subsequent loopback links (lanes already assigned, HW already configured)
   if (!port_already_configured_for_loopback)
   {
      mtip_netdev_assign_port_lanes(port_type);
      mtip_netdev_setup_port_hw(port_type);
   }
   else
   {
      CSMLOGINFO("Skipping lane assignment and HW setup - port already configured for loopback");
   }

   // Complete the netdev open of all links pending lane assignment
   mtip_device_complete_port_open(port_type);

out:
   return;
}

void post_mtip_process_configure_port_using_lane(u32 port_type, u32 lane_index)
{
   struct mtip_process_configure_port_using_lane_task* taskstruct = kmalloc(sizeof(struct mtip_process_configure_port_using_lane_task), GFP_ATOMIC);

   if(taskstruct == NULL)
   {
      CSMLOGERR("memory alloc failed\n");
      return;
   }

   taskstruct->port_type = port_type;
   taskstruct->lane_index = lane_index;

   mtip_queue_work(MTIP_WORKQ_TASK_PROCESS_PORT_CONFIGURATION_USING_LANE, taskstruct, port_type);
}

// There has been some change in the configuration of a port
// This is invoked when a lane up indication is received
// the updated port qsfp information is available
void run_mtip_process_configure_port_using_lane(void *work_ptr)
{
   struct mtip_process_configure_port_using_lane_task *taskstruct = (struct mtip_process_configure_port_using_lane_task *)work_ptr;
   u32 port_type = taskstruct->port_type;
   u32 lane_index = taskstruct->lane_index;

   // configure the port based on stored link and lane information
   CSMLOGINFO("configuring port %d with change in lane_index: %d", port_type, lane_index);

   mtip_device_configure_port(port_type);

   // free the taskstruct
   kfree(taskstruct);
}

void post_mtip_process_configure_port_using_link(u32 port_type, u32 link_index)
{
   struct mtip_process_configure_port_using_link_task* taskstruct = kmalloc(sizeof(struct mtip_process_configure_port_using_link_task), GFP_ATOMIC);

   if(taskstruct == NULL)
   {
      CSMLOGERR("memory alloc failed\n");
      return;
   }

   taskstruct->port_type = port_type;
   taskstruct->link_index = link_index;

   mtip_queue_work(MTIP_WORKQ_TASK_PROCESS_PORT_CONFIGURATION_USING_LINK, taskstruct, port_type);
}

// There has been some change in the configuration of a port
void run_mtip_process_configure_port_using_link(void *work_ptr)
{
   struct mtip_process_configure_port_using_link_task *taskstruct = (struct mtip_process_configure_port_using_link_task *)work_ptr;
   u32 port_type = taskstruct->port_type;
   u32 link_index = taskstruct->link_index;

   // configure the port based on stored link and lane information
   CSMLOGINFO("configuring port %d with change in link_index: %d", port_type, link_index);

   mtip_device_configure_port(port_type);

   // free the taskstruct
   kfree(taskstruct);
}

void post_mtip_process_an_result(enum mtip_port_type_enum port_type, bool an_result, enum mtip_port_config_enum port_config, u8 seq_num)
{
   struct mtip_process_an_result_task* taskstruct = kmalloc(sizeof(struct mtip_process_an_result_task), GFP_ATOMIC);

   if(taskstruct == NULL)
   {
      CSMLOGERR("memory alloc failed\n");
      return;
   }

   taskstruct->port_type = port_type;
   taskstruct->an_result = an_result;
   taskstruct->port_config = port_config;
   taskstruct->seq_num = seq_num;

   mtip_queue_work(MTIP_WORKQ_TASK_PROCESS_AN_RESULT, taskstruct, port_type);
}

// we got the updated result of AN
void run_mtip_process_an_result(void *work_ptr)
{
    struct mtip_process_an_result_task *taskstruct = (struct mtip_process_an_result_task *)work_ptr;
    u32 port_type = taskstruct->port_type;
    bool an_result = taskstruct->an_result;
    u8 seq_num = taskstruct->seq_num;
    enum mtip_port_config_enum port_config = taskstruct->port_config;

    if(platform_driver_priv == NULL || platform_driver_priv->mtip_ports[port_type] == NULL)
       goto out;

    CSMLOGINFO("Processing AN result for port_type %d seq %d with result %d config %d %s",
               port_type, an_result, seq_num, port_config, mtip_ethtool_get_port_config_str(port_config));

    if(an_result == false)
    {
        CSMLOGERR("AN failed for port_type %d", port_type);
        // need to figure out what to do with AN failures
        goto out;
    }

    if(seq_num != mtip_phy_an_seq_num[port_type])
    {
        CSMLOGINFO("Ignore the older AN result");
        return;
    }

    // check that the port is waiting for AN result
    if(platform_driver_priv->mtip_ports[port_type]->port_state != MTIP_PORT_STATE_CONNECTED_NEGOTIATION_IN_PROGRESS)
    {
        // Process this AN result if the result port_config has changed
        if(platform_driver_priv->mtip_ports[port_type]->port_config != port_config)
        {
            CSMLOGINFO("AN result has changed, apply reconfiguration");
            post_mtip_process_reconfigure_port(port_type);
            goto out;
        }
        // Ignore if same port config is received
        else
        {
            CSMLOGDBG("Got a spurious AN result callback in state %d for port_type %d", platform_driver_priv->mtip_ports[port_type]->port_state, port_type);
            goto out;
        }
    }

    // set the port config
    platform_driver_priv->mtip_ports[port_type]->port_config = port_config;
    platform_driver_priv->mtip_ports[port_type]->port_state = MTIP_PORT_STATE_CONNECTED_NEGOTIATION_DONE;

    // configure the port with the new port config
    mtip_device_configure_port(port_type);

out:
    // free the taskstruct
    kfree(taskstruct);
}

void post_mtip_process_netdev_open(struct net_device *netdev)
{
   u32 port_type;
   struct mtip_netdev_priv *priv = netdev_priv(netdev);
   struct mtip_process_process_netdev_events* taskstruct = NULL;

   if(mtip_lookup_port_type_by_link_index(priv->link_index, &port_type))
   {
      CSMLOGERR("invalid link index");
      return;
   }

   taskstruct = kmalloc(sizeof(struct mtip_process_process_netdev_events), GFP_ATOMIC);
   if(taskstruct == NULL)
   {
      CSMLOGERR("memory alloc failed\n");
      return;
   }

   taskstruct->netdev = netdev;

   mtip_queue_work(MTIP_WORKQ_TASK_PROCESS_NETDEV_OPEN, taskstruct, port_type);
}

void run_mtip_process_netdev_open(void* workptr)
{
   struct mtip_process_process_netdev_events *taskstruct = (struct mtip_process_process_netdev_events *)workptr;
   struct net_device *netdev;
   struct mtip_netdev_priv *priv;
   u32 link_index;
   ecpri_dma_eth_conn_hdl_t hdl;
   int sfp_port_type;
   enum ecpri_dma_notify_mode setmode = ECPRI_DMA_NOTIFY_MODE_IRQ;
   u32 port_type;
   u32 lane_index;
   trx_lane_down_reason_code_type reason_code;

   if (!taskstruct || !taskstruct->netdev) {
       CSMLOGERR("Invalid taskstruct or netdev");
       goto out;
   }

   netdev = taskstruct->netdev;
   priv = netdev_priv(netdev);
   if(!priv)
      goto out;

   link_index = priv->link_index;
   if(link_index >= MTIP_MAX_LINKS)
   {
     CSMLOGERR("invalid link_index %d", link_index);
     goto out;
   }

   if(platform_driver_priv == NULL || platform_driver_priv->mtip_links[link_index] == NULL)
      goto out;

   hdl = platform_driver_priv->mtip_links[link_index]->dma_hdl;

   CSMLOGINFO("mtip_open called for link_index: %d with hdl: %d\n", link_index, hdl);

   if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
   {
      CSMLOGERR("invalid port_type for link_index %d", link_index);
      goto out;
   }
   // Initialize the carrier state as off
   if ( ( link_index != MTIP_L2_ETH_LINK_INDEX && link_index != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode == MTIP_MODE_DEFAULT) || ((link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX) && mtip_c2c2_loopback_mode == MTIP_MODE_DEFAULT))
   {
      netif_carrier_off(netdev);
   }

   // first get the interface going
   if (hdl)
   {
      /*
       * enable napi
       */
      napi_enable(&(platform_driver_priv->mtip_links[link_index]->napi));
      napi_enable(&(platform_driver_priv->mtip_links[link_index]->napi_tx));

      // start the pipe
      mtip_start_dma_pipe(netdev, hdl);
      // set the netdev MAC address from the HW
      mtip_set_netdev_hw_mac_addr(netdev, link_index);

      // set to POLL mode
      setmode = ECPRI_DMA_NOTIFY_MODE_IRQ;

      // set the tx mode to IRQ
      mtip_set_tx_mode_immediate(hdl, setmode);

      /* 
       * Start the interface's transmit queue 
       * (allowing it to accept packets for transmission) 
       * once it is ready to start sending data. 
       */
      netif_start_queue(netdev);
   }

   // this is done only for the RUMI E2E
   if (mtip_rumi_platform != MTIP_PLATFORM_SOC)
   {
      if (mtip_loopback_mode == MTIP_MODE_DEFAULT)
      {
         // Configure phylib in poll mode
         priv->phydev->irq = PHY_POLL;

         // PHYLINK-PHY binding and PHY bringup
         phylink_connect_phy(priv->phylink, priv->phydev);

         // Start the PHYLINK
         phylink_start(priv->phylink);
      }
   }
   else
   {
      // Change the state for PCS loopback
      CSMLOGINFO("Link state change and dev open for link = %u\n", link_index);
      if ( mtip_is_link_in_loopback(link_index) || ( link_index != MTIP_L2_ETH_LINK_INDEX && link_index != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode == MTIP_MODE_LOOPBACK) || ( (link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX) && mtip_c2c2_loopback_mode == MTIP_MODE_C2C2_LOOPBACK) )
         platform_driver_priv->mtip_links[link_index]->state = MTIP_LINK_STATE_OPEN_WAITING_FOR_LANES;

      // For PCS/PHY loopback mode, configure port based on the speed modes set
      if ( mtip_is_link_in_loopback(link_index) || ( link_index != MTIP_L2_ETH_LINK_INDEX && link_index != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode != MTIP_MODE_DEFAULT) || ((link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX) && mtip_c2c2_loopback_mode != MTIP_MODE_DEFAULT))
         mtip_device_configure_port(port_type);

      // PCS looback mode
      if ( ( link_index != MTIP_L2_ETH_LINK_INDEX && link_index != MTIP_C2C1_ETH_LINK_INDEX && mtip_loopback_mode == MTIP_MODE_LOOPBACK) || ( (link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX) && mtip_c2c2_loopback_mode == MTIP_MODE_C2C2_LOOPBACK))
      {
         // Process MAC link up state
         mtip_mac_link_up(link_index);
      }
      else
      {
         // the default E2E mode
         // check if lane assignment is complete
         if (platform_driver_priv->mtip_links[link_index]->lanes_assignment_complete == true)
         {
            // lane assignment is already done for the port
            // this means the port HW has been configured already

            mutex_lock(&platform_driver_priv->mtip_links[link_index]->dev_lock);

            if ((platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_INIT) ||
                (platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_CLOSE))
            {
               // set the state to OPEN_DONE
               platform_driver_priv->mtip_links[link_index]->state = MTIP_LINK_STATE_OPEN_DONE;
            }

            mutex_unlock(&platform_driver_priv->mtip_links[link_index]->dev_lock);

            // get the sfp port type; C2C links in loopback always use PORT_DA
            if ((link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX) &&
                mtip_c2c2_loopback_mode != MTIP_MODE_DEFAULT)
                sfp_port_type = PORT_DA;
            else
                sfp_port_type = platform_driver_priv->mtip_ports[port_type]->sfp_port_type;

            // Notify TRX driver to enable TX
            mtip_phy_notify_eth_event_to_trx(link_index, TRX_IFCONFIG_UP);

            // Bring down logging C2C2 port if needed
            // Skip in E2E mode — eth30 is a live C2C link, not a logging port.
            if (mtip_c2c2_loopback_mode != MTIP_MODE_DEFAULT)
                mtip_eth_reeval_logging_port();

            // bring up the phy
            mtip_phy_bringup_phy(link_index, sfp_port_type);

            CSMLOGINFO("phy bringup done for link: %d\n", link_index);
         }
         else
         {
            // lane assignment has not been done for this port
            mutex_lock(&platform_driver_priv->mtip_links[link_index]->dev_lock);

            if ((platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_INIT) ||
                (platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_CLOSE))
            {
               // lane assignment is not complete yet
               // set the state to OPEN_WAITING_FOR_LANES
               platform_driver_priv->mtip_links[link_index]->state = MTIP_LINK_STATE_OPEN_WAITING_FOR_LANES;

               // Inform local plugout at boot up
               if(mtip_lookup_lane_index_by_port_type_and_real_lane(&lane_index, port_type, 0) >= 0)
               {
                 if((qsfp_trx_get_lane_down_reason_code(platform_driver_priv->devices.lane_devices[lane_index].sfp_phandle,
                                                       &reason_code) >= 0) &&
                    (reason_code == TRX_LOCAL_PLUGOUT))
                 {
                   mtip_snd_event_notification(link_index, LOCAL_PLUG_OUT_SET);
                 }
               }
            }

            mutex_unlock(&platform_driver_priv->mtip_links[link_index]->dev_lock);

            CSMLOGINFO("link_index %d set to WAITING_FOR_LANES state", link_index);

            post_mtip_process_configure_port_using_link(port_type, link_index);
         }
      }
   }

   /* Send update to clients */
   mtip_client_send_event(ETH_ECPRISS_EVENT_UP, link_index);
   mtip_snd_event_notification(link_index, IF_UP);

out:
   // free the taskstruct
   kfree(taskstruct);

   return;
}

void post_mtip_process_netdev_close(struct net_device *netdev)
{
   u32 port_type;
   struct mtip_netdev_priv *priv = netdev_priv(netdev);
   struct mtip_process_process_netdev_events* taskstruct = NULL;

   if(mtip_lookup_port_type_by_link_index(priv->link_index, &port_type))
   {
       CSMLOGERR("invalid link index");
       return;
   }

   taskstruct = kmalloc(sizeof(struct mtip_process_process_netdev_events), GFP_ATOMIC);
   if(taskstruct == NULL)
   {
      CSMLOGERR("memory alloc failed\n");
      return;
   }

   taskstruct->netdev = netdev;
   mtip_queue_work(MTIP_WORKQ_TASK_PROCESS_NETDEV_CLOSE, taskstruct, port_type);
}

void run_mtip_process_netdev_close(void* workptr)
{
   struct mtip_process_process_netdev_events *taskstruct = (struct mtip_process_process_netdev_events *)workptr;
   struct net_device *netdev = taskstruct->netdev;
   struct mtip_netdev_priv *priv;
   ecpri_dma_eth_conn_hdl_t hdl;
   u32 link_index;
   u32 port_type;
   bool all_closed = true;
   int i;
   u32 tmp_link_index;
   u32 real_lane = 0;
   u32 lane_index;
   u32 sfp_phandle[MTIP_MAX_LANES_PER_PORT] = {0};
   struct trx_eth_event_t trx_event_info = {0};

   priv = netdev_priv(netdev);
   if(!priv)
      goto exit;

   link_index = priv->link_index;
   if(link_index >= MTIP_MAX_LINKS)
   {
     CSMLOGERR("invalid link_index %d", link_index);
     goto exit;
   }

   if(platform_driver_priv == NULL || platform_driver_priv->mtip_links[link_index] == NULL)
      goto exit;

   hdl = platform_driver_priv->mtip_links[link_index]->dma_hdl;

   CSMLOGINFO("mtip_close called with link_index: %d with hdl: %d\n", link_index, hdl);

   mtip_lookup_port_type_by_link_index(link_index, &port_type);

   // set the link state to CLOSE
   mutex_lock(&platform_driver_priv->mtip_links[link_index]->dev_lock);

   platform_driver_priv->mtip_links[link_index]->state = MTIP_LINK_STATE_CLOSE;

   mutex_unlock(&platform_driver_priv->mtip_links[link_index]->dev_lock);

   if(platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer_running)
   {
      del_timer_sync(&platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer);
      platform_driver_priv->mtip_links[link_index]->pcs_link_up_defer_timer_running = false;
   }

   // do this only for RUMI E2E
   if (mtip_rumi_platform != MTIP_PLATFORM_SOC)
   {
      if (mtip_loopback_mode == MTIP_MODE_DEFAULT)
      {
         /* Stop and disconnect the PHY */
         phylink_stop(priv->phylink);
         phylink_disconnect_phy(priv->phylink);
      }
   }
   else
   {
      if ( ( link_index != MTIP_L2_ETH_LINK_INDEX && (mtip_loopback_mode == MTIP_MODE_DEFAULT ||
          mtip_loopback_mode == MTIP_MODE_PHY_LOOPBACK)) || (link_index == MTIP_L2_ETH_LINK_INDEX && ( mtip_c2c2_loopback_mode == MTIP_MODE_DEFAULT || mtip_c2c2_loopback_mode == MTIP_MODE_PHY_LOOPBACK)))
      {
         /* teardown the phy if
            1. AN is not in progress OR
            2. If there is no other active link while AN is in progress
         */
         if(platform_driver_priv->mtip_ports[port_type]->port_state != MTIP_PORT_STATE_CONNECTED_NEGOTIATION_IN_PROGRESS ||
            mtip_lookup_if_any_other_link_active_for_port(port_type, link_index) == false)
         {
            mtip_phy_teardown_phy(link_index);
            CSMLOGDBG("phy teardown done for link: %d\n", link_index);
         }

         // Notify TRX driver to disable TX
         mtip_phy_notify_eth_event_to_trx(link_index, TRX_IFCONFIG_DOWN);

         // Notify TRX driver to disable TX on primary lane if AN was in progress and not other links are active
         if(platform_driver_priv->mtip_ports[port_type]->port_state == MTIP_PORT_STATE_CONNECTED_NEGOTIATION_IN_PROGRESS &&
            mtip_lookup_if_any_other_link_active_for_port(port_type, link_index) == false)
         {
            if(port_type == MTIP_PORT_TYPE_DEBUG)
            {
               real_lane = 2;
            }

            mtip_lookup_lane_index_by_port_type_and_real_lane(&lane_index, port_type, real_lane);
            sfp_phandle[real_lane] = platform_driver_priv->devices.lane_devices[lane_index].sfp_phandle;
            trx_event_info.event = TRX_IFCONFIG_DOWN;
            trx_event_info.lane_phandle = sfp_phandle;
            trx_event_info.num_lanes = 1;
            qsfp_trx_eth_event_notifier(&trx_event_info);
         }
      }
      // PCS looback mode
      else
      {
         // Process MAC link down state
         mtip_mac_link_down(link_index);
      }
   }

   if (hdl)
   {
      // stop the pipe
      mtip_stop_dma_pipe(hdl);

      /*
       * disable napi
       */
      napi_disable(&(platform_driver_priv->mtip_links[link_index]->napi));
      napi_disable(&(platform_driver_priv->mtip_links[link_index]->napi_tx));

      /* release ports, irq and such -- like fops->close */
      netif_stop_queue(netdev);
   }

   all_closed = true;

   for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_link_phandles; ++i) 
   {
       tmp_link_index = platform_driver_priv->devices.port_devices[port_type].link_devices[i]->link_index;
       mutex_lock(&platform_driver_priv->mtip_links[link_index]->dev_lock);
       if ((platform_driver_priv->mtip_links[tmp_link_index]) && (platform_driver_priv->mtip_links[tmp_link_index]->state != MTIP_LINK_STATE_INIT) &&
           (platform_driver_priv->mtip_links[tmp_link_index]->state != MTIP_LINK_STATE_CLOSE))
       {
           all_closed = false;
           mutex_unlock(&platform_driver_priv->mtip_links[link_index]->dev_lock);
           break;
       }
       mutex_unlock(&platform_driver_priv->mtip_links[link_index]->dev_lock);
   }

   if (all_closed) 
   {
       // reset the port state to INIT
      if(platform_driver_priv->mtip_ports[port_type]) {
         platform_driver_priv->mtip_ports[port_type]->port_state = MTIP_PORT_STATE_INIT;

         // reset the port speed to 0, across lanes
         for (i = 0; i < PHY_LANE_MAX; ++i) 
         {
            platform_driver_priv->mtip_ports[port_type]->lane_config[i].lane_speed = 0;
         }

         // Reset the speed mode switch counters and flags
         platform_driver_priv->mtip_ports[port_type]->port_priv_flags_optical = 0;
         platform_driver_priv->mtip_ports[port_type]->next_speed_retry_count = 0;
      }

      // reset all the lane assignments
      for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_link_phandles; ++i) 
      {
         tmp_link_index = platform_driver_priv->devices.port_devices[port_type].link_devices[i]->link_index;

         if(platform_driver_priv->mtip_links[tmp_link_index]) {
            mutex_lock(&platform_driver_priv->mtip_links[tmp_link_index]->dev_lock);

            platform_driver_priv->mtip_links[tmp_link_index]->lanes_assignment_complete = false;
            mutex_unlock(&platform_driver_priv->mtip_links[tmp_link_index]->dev_lock);
         }
      }

      mtip_phy_reset_phy_sm(port_type);
   }

   /* Send update to clients */
   mtip_client_send_event(ETH_ECPRISS_EVENT_DOWN, link_index);
   mtip_snd_event_notification(link_index, IF_DOWN);

exit:
   // free the taskstruct
   kfree(taskstruct);

   return;
}

void post_mtip_process_reconfigure_port(u32 port_type)
{
   struct mtip_process_reconfigure_port* taskstruct = kmalloc(sizeof(struct mtip_process_reconfigure_port), GFP_ATOMIC);
   if(taskstruct == NULL)
   {
       CSMLOGERR("memory alloc failed\n");
       return;
   }

   CSMLOGINFO("Reconfiguration queued for port %d\n", port_type);

   taskstruct->port_type = port_type;
   mtip_queue_work(MTIP_WORKQ_TASK_PROCESS_RECONFIGURE_PORT, taskstruct, port_type);
}

void run_mtip_process_reconfigure_port(void* workptr)
{
   struct mtip_process_reconfigure_port *taskstruct = (struct mtip_process_reconfigure_port *)workptr;
   u32 port_type = taskstruct->port_type;
   u32 tmp_link_index;
   int i;
   enum mtip_link_state_enum link_state;

   if(platform_driver_priv == NULL || platform_driver_priv->mtip_ports[port_type] == NULL)
      goto exit;

   // Set the port to DISCONNECTED
   platform_driver_priv->mtip_ports[port_type]->port_state = MTIP_PORT_STATE_DISCONNECTED;

   // Go through all the links of the port that are in UP or DOWN state
   for (i = 0; i < MTIP_MAX_LINKS_PER_PORT; ++i)
   {
      if (mtip_lookup_link_index_by_port_type_and_real_link(&tmp_link_index, port_type, i) == 0)
      {
         if (platform_driver_priv->mtip_links[tmp_link_index] != NULL)
         {
            link_state = mtip_get_link_state_by_link_index(tmp_link_index);
            if ((link_state == MTIP_LINK_STATE_OPEN_DONE) ||
                (link_state == MTIP_LINK_STATE_UP) ||
                (link_state == MTIP_LINK_STATE_DOWN))
            {
               // teardown the phy
               mtip_phy_teardown_phy(tmp_link_index);
            }
         }
      }
   }

   // reset PHY SM
   mtip_phy_reset_phy_sm(port_type);

   // Move the port state back to INIT
   platform_driver_priv->mtip_ports[port_type]->port_state = MTIP_PORT_STATE_INIT;

   // Issue port configuration with the new configuration
   mtip_device_configure_port(port_type);

exit:
   // free the taskstruct
   kfree(taskstruct);

   return;
}

void post_mtip_process_next_speed_mode(struct mtip_process_next_speed_mode info)
{
   struct mtip_process_next_speed_mode* taskstruct;

   taskstruct = kmalloc(sizeof(struct mtip_process_next_speed_mode), GFP_ATOMIC);
   if(taskstruct == NULL)
   {
       CSMLOGERR("memory alloc failed\n");
       return;
   }

   memcpy(taskstruct, &info, sizeof(struct mtip_process_next_speed_mode));
   mtip_queue_work(MTIP_WORKQ_TASK_PROCESS_NEXT_SPEED_MODE, taskstruct, info.port_type);
}

void run_mtip_process_next_speed_mode(void* workptr)
{
   struct mtip_process_next_speed_mode *taskstruct = (struct mtip_process_next_speed_mode *)workptr;
   u32 port_type = taskstruct->port_type;
   u32 link_index = taskstruct->link_index;
   u32 lane_index;
   u32 pflags;
   int i;

   if(platform_driver_priv == NULL || platform_driver_priv->mtip_links[link_index] == NULL || platform_driver_priv->mtip_ports[port_type] == NULL)
      goto exit;

   pflags = platform_driver_priv->mtip_ports[port_type]->port_priv_flags_optical;

   // Fetch the first lane index
   lane_index = platform_driver_priv->mtip_links[link_index]->assigned_lane_indices[0];

   CSMLOGINFO("Checking if attempt needed for next speed mode for port %d", port_type);

   // Init port_priv_flags_optical with the supported modes
   if(pflags == 0)
      pflags = platform_driver_priv->mtip_ports[port_type]->port_priv_flags_optical = mtip_device_filter_priv_flags(port_type);

   for (i = 0; i < MTIP_PORT_CONFIG_MAX; ++i) 
   {
      // Clear the highest configured speed mode, and attempt for the pending ones
      if (pflags & 0x1) 
      {
         platform_driver_priv->mtip_ports[port_type]->port_priv_flags_optical &= (~(1<<i));
         break;
      }

      pflags >>= 1;
   }

   CSMLOGDBG("port_priv_flags_optical 0x%x, next_speed_retry_count %d", platform_driver_priv->mtip_ports[port_type]->port_priv_flags_optical, platform_driver_priv->mtip_ports[port_type]->next_speed_retry_count);

   /* If all supported speed modes have been tried already, begin from the start
      and increment the retry count */
   if(platform_driver_priv->mtip_ports[port_type]->port_priv_flags_optical == 0)
   {
      platform_driver_priv->mtip_ports[port_type]->port_priv_flags_optical = mtip_device_filter_priv_flags(port_type);
      platform_driver_priv->mtip_ports[port_type]->next_speed_retry_count++;
   }

   CSMLOGINFO("Attempting for next speed mode with port config 0x%x", platform_driver_priv->mtip_ports[port_type]->port_priv_flags_optical);

   // Issue port configuration with the new set of speed mode flags
   post_mtip_process_reconfigure_port(port_type);

exit:
   // free the taskstruct
   kfree(taskstruct);

   return;
}


void mtip_rx_replenish_retry_timer_cb(struct timer_list *list)
{
    struct mtip_link_info *link_info;

    if(!platform_driver_priv)
        return;

    link_info = from_timer(link_info, list, rx_replenish_retry_timer);
    if(!link_info)
        return;

    // Simulate RX completion callback which will schedule NAPI
    mtip_dma_rx_comp_cb(NULL, link_info->dma_hdl);

    return;
}

void mtip_pcs_link_up_defer_timer_cb(struct timer_list *list)
{
    struct mtip_link_info *link_info;

    if(!platform_driver_priv)
        return;

    link_info = from_timer(link_info, list, pcs_link_up_defer_timer);
    if(!link_info)
        return;

    if(!platform_driver_priv->mtip_links[link_info->link_index]->pcs_link_up_defer_timer_running)
    {
      CSMLOGERR("Timer not valid, skip processing link up");
      return;
    }

    post_mtip_process_link_state(link_info->link_index, true);

    return;
}

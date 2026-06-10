/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

/**
  @file mtip_sysfs.c
  @brief SYS FS for Debug ETH MAC Driver.

  This file implements the SYS FS nodes interface for Debug Ethernet MAC driver
*/

#include <linux/bitrev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/fcntl.h>
#include <linux/gfp.h>
#include <linux/in.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

#include <linux/errno.h>     /* error codes */
#include <linux/interrupt.h> /* mark_bh */
#include <linux/kernel.h>    /* printk() */
#include <linux/sched.h>
#include <linux/slab.h>  /* kmalloc() */
#include <linux/types.h> /* size_t */

#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/in.h>
#include <linux/ip.h>        /* struct iphdr */
#include <linux/netdevice.h> /* struct device, and other headers */
#include <linux/of.h>
#include <linux/skbuff.h>
#include <linux/tcp.h> /* struct tcphdr */

#include "mtip.h"
#include "mtip_client.h"
#include "mtip_debug_eth_gnl_uapi.h"
#include "mtip_device.h"
#include "mtip_dma.h"
#include "mtip_hashmap.h"
#include "mtip_mac.h"
#include "mtip_mdio.h"
#include "mtip_platform.h"
#include "mtip_sysfs.h"

#include <linux/kobject.h>
#include <linux/sysfs.h> /* sysfs addition*/

#ifndef MIN
#define MIN(a,b) ((a < b) ? a : b)
#endif

void __iomem *debug_port_base_address;

struct kobject *kobj_root, *kobj_ref_L3L2headers, *kobj_ref_L2headers,
    *kobj_ref_L3headers, *kobj_FIFO_0, *kobj_FIFO_1, *kobj_FIFO_2, *kobj_FIFO_3,
    *kobj_FIFO_4, *kobj_FIFO_5, *kobj_FIFO_6, *kobj_FIFO_7;

struct RootDirectory root = {.enabled = 1};
struct L2Headers L2 = {.saddr = {0}, .daddr = {0}};
struct L3Headers L3 = {.saddr = {0}, .daddr = {0}, .sport = 0, .dport = 0};
struct StreamingFifo F0 = {.status = 0,
                           .flush = 0,
                           .txcount = 0,
                           .AddrRange_Start = 0x0,
                           .AddrRange_End = 0x1000,
                           .OverFlowInterrupt = 0,
                           .Threshold = 1500,
                           .Timeout = 0,
                           .vlanID = 1};
struct StreamingFifo F1 = {.status = 0,
                           .flush = 0,
                           .txcount = 0,
                           .AddrRange_Start = 0x1000,
                           .AddrRange_End = 0x2000,
                           .OverFlowInterrupt = 0,
                           .Threshold = 1500,
                           .Timeout = 0,
                           .vlanID = 2};
struct StreamingFifo F2 = {.status = 0,
                           .flush = 0,
                           .txcount = 0,
                           .AddrRange_Start = 0x2000,
                           .AddrRange_End = 0x3000,
                           .OverFlowInterrupt = 0,
                           .Threshold = 1500,
                           .Timeout = 0,
                           .vlanID = 3};
struct StreamingFifo F3 = {.status = 0,
                           .flush = 0,
                           .txcount = 0,
                           .AddrRange_Start = 0x3000,
                           .AddrRange_End = 0x4000,
                           .OverFlowInterrupt = 0,
                           .Threshold = 1500,
                           .Timeout = 0,
                           .vlanID = 4};
struct StreamingFifo F4 = {.status = 0,
                           .flush = 0,
                           .txcount = 0,
                           .AddrRange_Start = 0x4000,
                           .AddrRange_End = 0x5000,
                           .OverFlowInterrupt = 0,
                           .Threshold = 1500,
                           .Timeout = 0,
                           .vlanID = 5};
struct PacketFifo F5 = {.status = 0,
                        .flush = 0,
                        .txcount = 0,
                        .AddrRange_Start = 0x5000,
                        .AddrRange_End = 0x6000,
                        .OverFlowInterrupt = 0,
                        .vlanID = 6};
struct PacketFifo F6 = {.status = 0,
                        .flush = 0,
                        .txcount = 0,
                        .AddrRange_Start = 0,
                        .AddrRange_End = 0,
                        .OverFlowInterrupt = 0,
                        .vlanID = 7};
struct PacketFifo F7 = {.status = 0,
                        .flush = 0,
                        .txcount = 0,
                        .AddrRange_Start = 0,
                        .AddrRange_End = 0,
                        .OverFlowInterrupt = 0,
                        .vlanID = 8};

// Sysfs Attribute Definitions
struct kobj_attribute enabled_attr =
    __ATTR(enabled, 0440, sysfs_show_enabled, NULL);
struct kobj_attribute saddr_attr =
    __ATTR(saddr, 0660, sysfs_show_saddr, sysfs_store_saddr);
struct kobj_attribute daddr_attr =
    __ATTR(daddr, 0660, sysfs_show_daddr, sysfs_store_daddr);
struct kobj_attribute sport_attr =
    __ATTR(sport, 0660, sysfs_show_sport, sysfs_store_sport);
struct kobj_attribute dport_attr =
    __ATTR(dport, 0660, sysfs_show_dport, sysfs_store_dport);
struct kobj_attribute status_attr =
    __ATTR(status, 0440, sysfs_show_status, NULL);
struct kobj_attribute flush_attr = __ATTR(flush, 0220, NULL, sysfs_store_flush);
struct kobj_attribute txcount_attr =
    __ATTR(txcount, 0440, sysfs_show_txcount, NULL);
struct kobj_attribute AddrRange_attr_start =
    __ATTR(AddrRangeStart, 0660, sysfs_show_AddrRange_Start,
           sysfs_store_AddrRange_Start);
struct kobj_attribute AddrRange_attr_end = __ATTR(
    AddrRangeEnd, 0660, sysfs_show_AddrRange_End, sysfs_store_AddrRange_End);
struct kobj_attribute OverFlowInterrupt_attr =
    __ATTR(OverFlowInterrupt, 0440, sysfs_show_OverFlowInterrupt, NULL);
struct kobj_attribute Threshold_attr =
    __ATTR(Threshold, 0660, sysfs_show_Threshold, sysfs_store_Threshold);
struct kobj_attribute Timeout_attr =
    __ATTR(Timeout, 0660, sysfs_show_Timeout, sysfs_store_Timeout);
struct kobj_attribute vlanID_attr =
    __ATTR(vlanID, 0660, sysfs_show_vlanID, sysfs_store_vlanID);

void set_debug_base_address(void __iomem *addr) {
  debug_port_base_address = addr;
  CSMLOGINFO("Debug port base address is %llx", debug_port_base_address);
}

void __iomem *get_debug_base_address(void) { return debug_port_base_address; }

void mtip_sysfs_mac_link_status(int status) {

  if (!root.enabled)
    return;

  if (status == true) {
    F0.status = 1;
    F1.status = 1;
    F2.status = 1;
    F3.status = 1;
    F4.status = 1;
    F5.status = 1;
    F6.status = 1;
    F7.status = 1;
  } else if (status == false) {
    F0.status = 0;
    F1.status = 0;
    F2.status = 0;
    F3.status = 0;
    F4.status = 0;
    F5.status = 0;
    F6.status = 0;
    F7.status = 0;
  }
}

void mtip_sysfs_isr_work_thread(struct work_struct *work) {
  int mask_val = 0;
  int set_bit = 1;
  int i;
  int *overflow_ptr;
  int fifo_interrupt_offset = 10;
  int Overflow_Interrupt_Array[] = {F0.OverFlowInterrupt, F1.OverFlowInterrupt,
                                    F2.OverFlowInterrupt, F3.OverFlowInterrupt,
                                    F4.OverFlowInterrupt, F5.OverFlowInterrupt,
                                    F6.OverFlowInterrupt, F7.OverFlowInterrupt};

  mask_val = (u32)ioread32(get_debug_base_address() + ERROR_INTR_MASK);

  for (i = FIFO_0 + fifo_interrupt_offset; i <= FIFO_7 + fifo_interrupt_offset;
       i++) {
    if (mask_val & (set_bit << i)) {
      overflow_ptr = &Overflow_Interrupt_Array[i - fifo_interrupt_offset];
      *overflow_ptr = 1;
    }
  }
}

void setup_common_params(void) {
  int data;
  int index;
  int val;
  uint8_t saddr[ETH_ALEN];
  u32 lower_SA = 0;
  u32 upper_SA = 0;
  u32 prev_val = 0;
  int i = 0;
  unsigned int UDP_SP_DP_ARRAY[] = {DBG_UDP_SP_DP_0, DBG_UDP_SP_DP_1,
                                    DBG_UDP_SP_DP_2};
  unsigned int L2_SA_ADDR_HI_ARRAY[] = {
      L2_SA_ADDR_HI_0, L2_SA_ADDR_HI_1, L2_SA_ADDR_HI_2, L2_SA_ADDR_HI_3,
      L2_SA_ADDR_HI_4, L2_SA_ADDR_HI_5, L2_SA_ADDR_HI_6, L2_SA_ADDR_HI_7};
  unsigned int L2_SA_ADDR_LO_ARRAY[] = {
      L2_SA_ADDR_LO_0, L2_SA_ADDR_LO_1, L2_SA_ADDR_LO_2, L2_SA_ADDR_LO_3,
      L2_SA_ADDR_LO_4, L2_SA_ADDR_LO_5, L2_SA_ADDR_LO_6, L2_SA_ADDR_LO_7};

  // Set up default value for sport and dport
  data = L3.sport = L3.dport = 5001;
  for (index = 0; index < MAX_PACKET_FIFO_COUNT; index++) {
    val = (u32)ioread32(debug_port_base_address + UDP_SP_DP_ARRAY[index]);
    val &= (~(GENMASK(15, 0)));
    val |= data;
    iowrite32(val, debug_port_base_address + UDP_SP_DP_ARRAY[index]);
  }

  // Set up source MAC address
  mtip_mac_get_mac_address_by_link_index(MTIP_DEBUG_ETH_LINK_INDEX, saddr);
  for(i = 0; i < ETH_ALEN; i++)
    L2.saddr[i] = saddr[i];

  for (index = 0; index < MAX_FIFO_COUNT; index++) {
    lower_SA = (L2.saddr[0]) | (L2.saddr[1] << 8) | (L2.saddr[2] << 16) |
               (L2.saddr[3] << 24);
    upper_SA = (L2.saddr[4]) | (L2.saddr[5] << 8);

    // write the lower bits
    iowrite32(lower_SA, debug_port_base_address + L2_SA_ADDR_LO_ARRAY[index]);

    // write the upper bits
    prev_val =
        (u32)ioread32(debug_port_base_address + L2_SA_ADDR_HI_ARRAY[index]);
    prev_val &= (~(GENMASK(15, 0)));
    prev_val |= ((upper_SA & GENMASK(15, 0)));
    iowrite32(prev_val, debug_port_base_address + L2_SA_ADDR_HI_ARRAY[index]);
  }

  return;
}

void setup_default_vlan_id(void) {
  u32 val;
  u32 vlan_id = 0;
  u32 vlan_tag = 1;
  u32 tpid = 0x8100;
  u32 vid = 0;
  u32 tci_val = 0;
  u32 tpid_val = 0;
  u32 i = 0;
  unsigned int VLAN_TAG_ADDR_ARRAY[] = {
      VLAN_TAG_0, VLAN_TAG_1, VLAN_TAG_2, VLAN_TAG_3, VLAN_TAG_4, VLAN_TAG_5,
      VLAN_TAG_6, VLAN_TAG_7};
  unsigned int L2_SA_ADDR_HI_ARRAY[] = {
      L2_SA_ADDR_HI_0, L2_SA_ADDR_HI_1, L2_SA_ADDR_HI_2, L2_SA_ADDR_HI_3,
      L2_SA_ADDR_HI_4, L2_SA_ADDR_HI_5, L2_SA_ADDR_HI_6, L2_SA_ADDR_HI_7};

  for(i = 0; i < MAX_FIFO_COUNT; i++){

    vid = i + 1;
    tpid_val = (((tpid >> 8) & GENMASK(7, 0)) | (((tpid & GENMASK(7, 0)) << 8)));
    tci_val = (((vid >> 8) & GENMASK(7, 0)) | (((vid & GENMASK(7, 0)) << 8)));
    vlan_id = (tci_val << 16) | tpid_val;
    iowrite32(vlan_id, debug_port_base_address + VLAN_TAG_ADDR_ARRAY[i]);

    val = (u32)ioread32(debug_port_base_address + L2_SA_ADDR_HI_ARRAY[i]);
    val &= (~(GENMASK(16, 16)));
    val |= ((vlan_tag) << 16);
    iowrite32(val, debug_port_base_address + L2_SA_ADDR_HI_ARRAY[i]);
  }

  return;
}

void setup_diag_l3_saddr(u_int8_t *l3_saddr){
  int i = 0, val = 0;
  unsigned int L3_IPV4_SA_ARRAY[] = {ETH_DBG_IPV4_SA_0, ETH_DBG_IPV4_SA_1,
                                     ETH_DBG_IPV4_SA_2};

  for (i = 0; i < 4; i++)
    L3.saddr[i] = l3_saddr[i];

  val = (L3.saddr[0]) | (L3.saddr[1] << 8) | (L3.saddr[2] << 16) |
        (L3.saddr[3] << 24);

  CSMLOGINFO("L3 source addr val is %x", val);

  for (i = 0; i < MAX_PACKET_FIFO_COUNT; i++)
    iowrite32(val, debug_port_base_address + L3_IPV4_SA_ARRAY[i]);

  return;
}

void setup_diag_l3_daddr(u_int8_t *l3_daddr){

  int i = 0, val = 0;
  unsigned int L3_IPV4_DA_ARRAY[] = {ETH_DBG_IPV4_DA_0, ETH_DBG_IPV4_DA_1,
                                     ETH_DBG_IPV4_DA_2};

  for (i = 0; i < 4; i++)
    L3.daddr[i] = l3_daddr[i];

  val = (L3.daddr[0]) | (L3.daddr[1] << 8) | (L3.daddr[2] << 16) |
        (L3.daddr[3] << 24);

  CSMLOGINFO("L3 dest addr val is %x", val);

  for (i = 0; i < MAX_PACKET_FIFO_COUNT; i++)
    iowrite32(val, debug_port_base_address + L3_IPV4_DA_ARRAY[i]);

  return;
}

void setup_diag_l2_daddr(u_int8_t *l2_daddr){

  int index;
  u32 lower_DA = 0;
  u32 upper_DA = 0;
  u32 prev_val = 0;
  int i = 0;
  unsigned int L2_DA_ADDR_HI_ARRAY[] = {
      L2_DA_ADDR_HI_0, L2_DA_ADDR_HI_1, L2_DA_ADDR_HI_2, L2_DA_ADDR_HI_3,
      L2_DA_ADDR_HI_4, L2_DA_ADDR_HI_5, L2_DA_ADDR_HI_6, L2_DA_ADDR_HI_7};
  unsigned int L2_DA_ADDR_LO_ARRAY[] = {
      L2_DA_ADDR_LO_0, L2_DA_ADDR_LO_1, L2_DA_ADDR_LO_2, L2_DA_ADDR_LO_3,
      L2_DA_ADDR_LO_4, L2_DA_ADDR_LO_5, L2_DA_ADDR_LO_6, L2_DA_ADDR_LO_7};

  for(i = 0; i < ETH_ALEN; i++)
    L2.daddr[i] = l2_daddr[i];

  for (index = 0; index < MAX_FIFO_COUNT; index++) {
    lower_DA = (L2.daddr[0]) | (L2.daddr[1] << 8) | (L2.daddr[2] << 16) |
               (L2.daddr[3] << 24);
    upper_DA = (L2.daddr[4]) | (L2.daddr[5] << 8);

    // write the lower bits
    CSMLOGINFO("Value in lowerSA is %x", lower_DA);
    CSMLOGINFO("Value in UpperSA is %x", upper_DA);
    iowrite32(lower_DA, debug_port_base_address + L2_DA_ADDR_LO_ARRAY[index]);

    // write the upper bits
    prev_val =
        (u32)ioread32(debug_port_base_address + L2_DA_ADDR_HI_ARRAY[index]);
    prev_val &= (~(GENMASK(15, 0)));
    prev_val |= ((upper_DA & GENMASK(15, 0)));
    iowrite32(prev_val, debug_port_base_address + L2_DA_ADDR_HI_ARRAY[index]);
  }

  return;
}

void setup_diag_addr_range(u_int8_t fifo_num, u_int32_t addr_range_start,
                           u_int32_t addr_range_end) {

  pr_err("setup_diag_addr_range called with fifo_num :%d addr_range_start : %d "
         "addr_range_end : %d \n",
         fifo_num, addr_range_start, addr_range_end);

  switch (fifo_num) {
  case FIFO_0:
    F0.AddrRange_Start = addr_range_start;
    F0.AddrRange_End = addr_range_end;
    iowrite32(F0.AddrRange_Start,
              debug_port_base_address + STREAM_FIFO_ADDR_MIN_0);
    iowrite32(F0.AddrRange_End,
              debug_port_base_address + STREAM_FIFO_ADDR_MAX_0);
    break;
  case FIFO_1:
    F1.AddrRange_Start = addr_range_start;
    F1.AddrRange_End = addr_range_end;
    iowrite32(F1.AddrRange_Start,
              debug_port_base_address + STREAM_FIFO_ADDR_MIN_1);
    iowrite32(F1.AddrRange_End,
              debug_port_base_address + STREAM_FIFO_ADDR_MAX_1);
    break;
  case FIFO_2:
    F2.AddrRange_Start = addr_range_start;
    F2.AddrRange_End = addr_range_end;
    iowrite32(F2.AddrRange_Start,
              debug_port_base_address + STREAM_FIFO_ADDR_MIN_2);
    iowrite32(F3.AddrRange_End,
              debug_port_base_address + STREAM_FIFO_ADDR_MAX_2);
    break;
  case FIFO_3:
    F3.AddrRange_Start = addr_range_start;
    F3.AddrRange_End = addr_range_end;
    iowrite32(F3.AddrRange_Start,
              debug_port_base_address + STREAM_FIFO_ADDR_MIN_3);
    iowrite32(F3.AddrRange_End,
              debug_port_base_address + STREAM_FIFO_ADDR_MAX_3);
    break;
  case FIFO_4:
    F4.AddrRange_Start = addr_range_start;
    F4.AddrRange_End = addr_range_end;
    iowrite32(F4.AddrRange_Start,
              debug_port_base_address + STREAM_FIFO_ADDR_MIN_4);
    iowrite32(F4.AddrRange_End,
              debug_port_base_address + STREAM_FIFO_ADDR_MAX_4);
    break;
  case FIFO_5:
    F5.AddrRange_Start = addr_range_start;
    F5.AddrRange_End = addr_range_end;
    iowrite32(F5.AddrRange_Start,
              debug_port_base_address + PACKET_FIFO_ADDR_MIN);
    iowrite32(F5.AddrRange_End, debug_port_base_address + PACKET_FIFO_ADDR_MAX);
    break;
  case FIFO_6:
    F6.AddrRange_Start = addr_range_start;
    F6.AddrRange_End = addr_range_end;
    iowrite32(F6.AddrRange_Start,
              debug_port_base_address + PACKET_FIFO_ADDR_MIN);
    iowrite32(F6.AddrRange_End, debug_port_base_address + PACKET_FIFO_ADDR_MAX);
    break;
  case FIFO_7:
    F7.AddrRange_Start = addr_range_start;
    F7.AddrRange_End = addr_range_end;
    iowrite32(F7.AddrRange_Start,
              debug_port_base_address + PACKET_FIFO_ADDR_MIN);
    iowrite32(F7.AddrRange_End, debug_port_base_address + PACKET_FIFO_ADDR_MAX);
    break;
  default:
    CSMLOGERR("Fifo num should be < 8 \n");
    break;
  }
}
void setup_diag_port(u_int16_t src_port, u_int16_t dest_port) {
  int data;
  int index;
  int val;
  unsigned int UDP_SP_DP_ARRAY[] = {DBG_UDP_SP_DP_0, DBG_UDP_SP_DP_1,
                                    DBG_UDP_SP_DP_2};
  pr_err("setup_diag_port called with src_port :%d dest_port : %d \n", src_port,
         dest_port);

  L3.sport = src_port;
  data = L3.sport;
  for (index = 0; index < MAX_PACKET_FIFO_COUNT; index++) {
    val = (u32)ioread32(debug_port_base_address + UDP_SP_DP_ARRAY[index]);
    val &= (~(GENMASK(15, 0)));
    val |= ((data & GENMASK(15, 8)) >> 8);
    val |= ((data & GENMASK(7, 0)) << 8);
    iowrite32(val, debug_port_base_address + UDP_SP_DP_ARRAY[index]);
  }

  L3.dport = dest_port;
  data = L3.dport;
  for (index = 0; index < MAX_PACKET_FIFO_COUNT; index++) {
    val = (u32)ioread32(debug_port_base_address + UDP_SP_DP_ARRAY[index]);
    val &= (~(GENMASK(31, 16)));
    val |= ((data & GENMASK(15, 8)) << 8);
    val |= ((data & GENMASK(7, 0)) << 24);
    iowrite32(val, debug_port_base_address + UDP_SP_DP_ARRAY[index]);
  }
}

void setup_diag_flush(u_int8_t fifo_num) {

  int val = -1;

  pr_err("setup_diag_flush called with fifo_num :%d \n", fifo_num);

  switch (fifo_num) {
  case FIFO_0:
    F0.flush = 1;
    break;
  case FIFO_1:
    F1.flush = 1;
    break;
  case FIFO_2:
    F2.flush = 1;
    break;
  case FIFO_3:
    F3.flush = 1;
    break;
  case FIFO_4:
    F4.flush = 1;
    break;
  case FIFO_5:
    F5.flush = 1;
    break;
  case FIFO_6:
    F6.flush = 1;
    break;
  case FIFO_7:
    F7.flush = 1;
    break;
  default:
    CSMLOGERR("Fifo num should be < 8 \n");
    break;
  }

  sysfs_store_flush_register_set(fifo_num,&val);
}

void setup_diag_threshold(u_int8_t fifo_num, u_int16_t threshold) {

  int value = 0;
  unsigned int fifo_registers[] = {
      STREAM_FIFO_THRESHOLD_0, STREAM_FIFO_THRESHOLD_1, STREAM_FIFO_THRESHOLD_2,
      STREAM_FIFO_THRESHOLD_3, STREAM_FIFO_THRESHOLD_4};

  pr_err("setup_diag_threshold called with fifo_num :%d threshold : %d \n",
         fifo_num, threshold);

  if (threshold >= MINIMUM_PACKET_SIZE && threshold <= MAXIMUM_PACKET_SIZE) {
    value |= ((threshold / BYTE_PER_WATERMARK_UNIT) & GENMASK(15, 0));

    switch (fifo_num) {
    case FIFO_0:
      F0.Threshold = threshold;
      break;
    case FIFO_1:
      F1.Threshold = threshold;
      break;
    case FIFO_2:
      F2.Threshold = threshold;
      break;
    case FIFO_3:
      F3.Threshold = threshold;
      break;
    case FIFO_4:
      F4.Threshold = threshold;
      break;
    default:
      CSMLOGERR("Threshold not available for fifo : %d \n", fifo_num);
      break;
    }

    iowrite32(value, debug_port_base_address + fifo_registers[fifo_num]);
  }
}

void setup_diag_timeout(u_int8_t fifo_num, u_int32_t timeout) {

  unsigned int stream_fifo_registers[] = {
      STREAM_FIFO_TIMER_0, STREAM_FIFO_TIMER_1, STREAM_FIFO_TIMER_2,
      STREAM_FIFO_TIMER_3, STREAM_FIFO_TIMER_4};

  pr_err("setup_diag_timeout called with fifo_num :%d timeout : %d \n",
         fifo_num, timeout);

  switch (fifo_num) {
  case FIFO_0:
    F0.Timeout = timeout;
    break;
  case FIFO_1:
    F1.Timeout = timeout;
    break;
  case FIFO_2:
    F2.Timeout = timeout;
    break;
  case FIFO_3:
    F3.Timeout = timeout;
    break;
  case FIFO_4:
    F4.Timeout = timeout;
    break;
  default:
    CSMLOGERR("Timeout not available for fifo : %d \n", fifo_num);
    break;
  }

  iowrite32(timeout,
            debug_port_base_address + stream_fifo_registers[fifo_num]);
}

void setup_diag_vlanID_register_set(u_int16_t vlanID, u_int32_t reg1,
                                    u_int32_t reg2) {
  u_int32_t val;
  u_int32_t vlan_id = 0;
  u_int32_t vlan_tag = 1;
  u_int32_t tpid = 0x8100;
  u_int32_t vid = 0;
  u_int32_t tci_val = 0;
  u_int32_t tpid_val = 0;

  vid = vlanID;

  // tpid_val = tpid;
  tpid_val = (((tpid >> 8) & GENMASK(7, 0)) | (((tpid & GENMASK(7, 0)) << 8)));
  tci_val = (((vid >> 8) & GENMASK(7, 0)) | (((vid & GENMASK(7, 0)) << 8)));

  vlan_id = (tci_val << 16) | tpid_val;

  iowrite32(vlan_id, debug_port_base_address + reg1);

  val = (u32)ioread32(debug_port_base_address + reg2);
  val &= (~(GENMASK(16, 16)));
  val |= ((vlan_tag) << 16);

  iowrite32(val, debug_port_base_address + reg2);
}
void setup_diag_vlanID(u_int8_t fifo_num, u_int16_t vlanID) {

if(vlanID >= MINIMUM_VLANID && vlanID <= MAXIMUM_VLANID) {
  switch (fifo_num) {

  case FIFO_0:
    setup_diag_vlanID_register_set(vlanID, VLAN_TAG_0, L2_SA_ADDR_HI_0);
    F0.vlanID = vlanID;
    break;
  case FIFO_1:
    setup_diag_vlanID_register_set(vlanID, VLAN_TAG_1, L2_SA_ADDR_HI_1);
    F1.vlanID = vlanID;
    break;
  case FIFO_2:
    setup_diag_vlanID_register_set(vlanID, VLAN_TAG_2, L2_SA_ADDR_HI_2);
    F2.vlanID = vlanID;
    break;
  case FIFO_3:
    setup_diag_vlanID_register_set(vlanID, VLAN_TAG_3, L2_SA_ADDR_HI_3);
    F3.vlanID = vlanID;
    break;
  case FIFO_4:
    setup_diag_vlanID_register_set(vlanID, VLAN_TAG_4, L2_SA_ADDR_HI_4);
    F4.vlanID = vlanID;
    break;
  case FIFO_5:
    setup_diag_vlanID_register_set(vlanID, VLAN_TAG_5, L2_SA_ADDR_HI_5);
    F5.vlanID = vlanID;
    break;
  case FIFO_6:
    setup_diag_vlanID_register_set(vlanID, VLAN_TAG_6, L2_SA_ADDR_HI_6);
    F6.vlanID = vlanID;
    break;
  case FIFO_7:
    setup_diag_vlanID_register_set(vlanID, VLAN_TAG_7, L2_SA_ADDR_HI_7);
    F7.vlanID = vlanID;
    break;
  default:
    CSMLOGERR("Fifo num should be < 8");
    break;
  }
 }
}

mtip_debug_eth_gnl_params get_diag_result(u_int8_t fifo_num) {

  mtip_debug_eth_gnl_params mtip_debug_eth_gnl_params_tbl = {0};
  int i = 0;

  mtip_debug_eth_gnl_params_tbl.fifo_num = fifo_num;

  for (i = 0; i < 4; i++) {
    mtip_debug_eth_gnl_params_tbl.source_l3_addr[i] = L3.saddr[i];
    mtip_debug_eth_gnl_params_tbl.dest_l3_addr[i] = L3.daddr[i];
  }
  for (i = 0; i < 6; i++) {
    mtip_debug_eth_gnl_params_tbl.source_l2_addr[i] = L2.saddr[i];
    mtip_debug_eth_gnl_params_tbl.dest_l2_addr[i] = L2.daddr[i];
  }
  mtip_debug_eth_gnl_params_tbl.source_port = L3.sport;
  mtip_debug_eth_gnl_params_tbl.dest_port = L3.dport;

  switch (fifo_num) {
  case FIFO_0:
    F0.txcount = (int)ioread32(debug_port_base_address + STREAM_PKT_CNT_0);
    mtip_debug_eth_gnl_params_tbl.addr_range_start = F0.AddrRange_Start;
    mtip_debug_eth_gnl_params_tbl.addr_range_end = F0.AddrRange_End;
    mtip_debug_eth_gnl_params_tbl.flush = F0.flush;
    mtip_debug_eth_gnl_params_tbl.threshold = F0.Threshold;
    mtip_debug_eth_gnl_params_tbl.timeout = F0.Timeout;
    mtip_debug_eth_gnl_params_tbl.vlanID = F0.vlanID;
    mtip_debug_eth_gnl_params_tbl.status = F0.status;
    mtip_debug_eth_gnl_params_tbl.txcount = F0.txcount;

    break;
  case FIFO_1:
    F1.txcount = (int)ioread32(debug_port_base_address + STREAM_PKT_CNT_1);
    mtip_debug_eth_gnl_params_tbl.addr_range_start = F1.AddrRange_Start;
    mtip_debug_eth_gnl_params_tbl.addr_range_end = F1.AddrRange_End;
    mtip_debug_eth_gnl_params_tbl.flush = F1.flush;
    mtip_debug_eth_gnl_params_tbl.threshold = F1.Threshold;
    mtip_debug_eth_gnl_params_tbl.timeout = F1.Timeout;
    mtip_debug_eth_gnl_params_tbl.vlanID = F1.vlanID;
    mtip_debug_eth_gnl_params_tbl.status = F1.status;
    mtip_debug_eth_gnl_params_tbl.txcount = F1.txcount;
    break;
  case FIFO_2:
    F2.txcount = (int)ioread32(debug_port_base_address + STREAM_PKT_CNT_2);
    mtip_debug_eth_gnl_params_tbl.addr_range_start = F2.AddrRange_Start;
    mtip_debug_eth_gnl_params_tbl.addr_range_end = F2.AddrRange_End;
    mtip_debug_eth_gnl_params_tbl.flush = F2.flush;
    mtip_debug_eth_gnl_params_tbl.threshold = F2.Threshold;
    mtip_debug_eth_gnl_params_tbl.timeout = F2.Timeout;
    mtip_debug_eth_gnl_params_tbl.vlanID = F2.vlanID;
    mtip_debug_eth_gnl_params_tbl.status = F2.status;
    mtip_debug_eth_gnl_params_tbl.txcount = F2.txcount;
    break;
  case FIFO_3:
    F3.txcount = (int)ioread32(debug_port_base_address + STREAM_PKT_CNT_3);
    mtip_debug_eth_gnl_params_tbl.addr_range_start = F3.AddrRange_Start;
    mtip_debug_eth_gnl_params_tbl.addr_range_end = F3.AddrRange_End;
    mtip_debug_eth_gnl_params_tbl.flush = F3.flush;
    mtip_debug_eth_gnl_params_tbl.threshold = F3.Threshold;
    mtip_debug_eth_gnl_params_tbl.timeout = F3.Timeout;
    mtip_debug_eth_gnl_params_tbl.vlanID = F3.vlanID;
    mtip_debug_eth_gnl_params_tbl.status = F3.status;
    mtip_debug_eth_gnl_params_tbl.txcount = F3.txcount;
    break;
  case FIFO_4:
    F4.txcount = (int)ioread32(debug_port_base_address + STREAM_PKT_CNT_4);
    mtip_debug_eth_gnl_params_tbl.addr_range_start = F4.AddrRange_Start;
    mtip_debug_eth_gnl_params_tbl.addr_range_end = F4.AddrRange_End;
    mtip_debug_eth_gnl_params_tbl.flush = F4.flush;
    mtip_debug_eth_gnl_params_tbl.threshold = F4.Threshold;
    mtip_debug_eth_gnl_params_tbl.timeout = F4.Timeout;
    mtip_debug_eth_gnl_params_tbl.vlanID = F4.vlanID;
    mtip_debug_eth_gnl_params_tbl.status = F4.status;
    mtip_debug_eth_gnl_params_tbl.txcount = F4.txcount;
    break;
  case FIFO_5:
    F5.txcount = (int)ioread32(debug_port_base_address + PACKET_FIFO1_PKT_CNT);
    mtip_debug_eth_gnl_params_tbl.addr_range_start = F5.AddrRange_Start;
    mtip_debug_eth_gnl_params_tbl.addr_range_end = F5.AddrRange_End;
    mtip_debug_eth_gnl_params_tbl.flush = F5.flush;
    mtip_debug_eth_gnl_params_tbl.vlanID = F5.vlanID;
    mtip_debug_eth_gnl_params_tbl.status = F5.status;
    mtip_debug_eth_gnl_params_tbl.txcount = F5.txcount;
    break;
  case FIFO_6:
    F6.txcount = (int)ioread32(debug_port_base_address + PACKET_FIFO2_PKT_CNT);
    mtip_debug_eth_gnl_params_tbl.addr_range_start = F6.AddrRange_Start;
    mtip_debug_eth_gnl_params_tbl.addr_range_end = F6.AddrRange_End;
    mtip_debug_eth_gnl_params_tbl.flush = F6.flush;
    mtip_debug_eth_gnl_params_tbl.vlanID = F6.vlanID;
    mtip_debug_eth_gnl_params_tbl.status = F6.status;
    mtip_debug_eth_gnl_params_tbl.txcount = F6.txcount;
    break;
  case FIFO_7:
    F7.txcount = (int)ioread32(debug_port_base_address + PACKET_FIFO3_PKT_CNT);
    mtip_debug_eth_gnl_params_tbl.addr_range_start = F7.AddrRange_Start;
    mtip_debug_eth_gnl_params_tbl.addr_range_end = F7.AddrRange_End;
    mtip_debug_eth_gnl_params_tbl.flush = F7.flush;
    mtip_debug_eth_gnl_params_tbl.vlanID = F7.vlanID;
    mtip_debug_eth_gnl_params_tbl.status = F7.status;
    mtip_debug_eth_gnl_params_tbl.txcount = F7.txcount;
    break;
  default:
    CSMLOGERR("Fifo num should be < 8 \n");
    break;
  }

  return mtip_debug_eth_gnl_params_tbl;
}

int setup_sysfs(void __iomem *addr, struct device *dev) {
  // setup the sysfs filesystem
  int index;
  CSMLOGERR("sysfs filesystem initialization called\n");

  set_debug_base_address(addr);

  /* creating the directory structure in /sys/kernel */
  kobj_root = kobject_create_and_add("debugeth", kernel_kobj);
  sysfs_create_generic_dir_structure(kobj_root);

  // Enable Debug FS
  root.enabled = 1;

  // L2L3 header info
  kobj_ref_L3L2headers = kobject_create_and_add("L2L3HeaderInfo", kobj_root);
  kobj_ref_L2headers = kobject_create_and_add("L2_Info", kobj_ref_L3L2headers);
  kobj_ref_L3headers = kobject_create_and_add("L3_Info", kobj_ref_L3L2headers);
  sysfs_create_L2headers(kobj_ref_L2headers);
  sysfs_create_L3headers(kobj_ref_L3headers);

  // Fifo queues
  kobj_FIFO_0 = kobject_create_and_add("FIFO_0", kobj_root);
  sysfs_create_StreamingFifo(kobj_FIFO_0);
  kobj_FIFO_1 = kobject_create_and_add("FIFO_1", kobj_root);
  sysfs_create_StreamingFifo(kobj_FIFO_1);
  kobj_FIFO_2 = kobject_create_and_add("FIFO_2", kobj_root);
  sysfs_create_StreamingFifo(kobj_FIFO_2);
  kobj_FIFO_3 = kobject_create_and_add("FIFO_3", kobj_root);
  sysfs_create_StreamingFifo(kobj_FIFO_3);
  kobj_FIFO_4 = kobject_create_and_add("FIFO_4", kobj_root);
  sysfs_create_StreamingFifo(kobj_FIFO_4);
  kobj_FIFO_5 = kobject_create_and_add("FIFO_5", kobj_root);
  sysfs_create_PacketFifo(kobj_FIFO_5);
  kobj_FIFO_6 = kobject_create_and_add("FIFO_6", kobj_root);
  sysfs_create_PacketFifo(kobj_FIFO_6);
  kobj_FIFO_7 = kobject_create_and_add("FIFO_7", kobj_root);
  sysfs_create_PacketFifo(kobj_FIFO_7);

  // SysFS Directory Structure completed
  for (index = 0; index < MAX_STREAM_FIFO_COUNT; index++) {
    setup_StreamingFIFO(index);
  }

  for (index = 0; index <= MAX_STREAM_FIFO_COUNT; index++) {
    setup_AXI_Address_Range(index);
  }

  // Setup common L2/L3 params
  setup_common_params();

  // Set default VLAN IDs
  setup_default_vlan_id();

  return 0;
}

void del_sysfs(void) {

	sysfs_remove_PacketFifo(kobj_FIFO_7);
	sysfs_remove_PacketFifo(kobj_FIFO_6);
	sysfs_remove_PacketFifo(kobj_FIFO_5);
	sysfs_remove_StreamingFifo(kobj_FIFO_4);
	sysfs_remove_StreamingFifo(kobj_FIFO_3);
	sysfs_remove_StreamingFifo(kobj_FIFO_2);
	sysfs_remove_StreamingFifo(kobj_FIFO_1);
	sysfs_remove_StreamingFifo(kobj_FIFO_0);
	sysfs_remove_L3headers(kobj_ref_L3headers);
	sysfs_remove_L2headers(kobj_ref_L2headers);
	sysfs_remove_generic_dir_structure(kobj_root);
}

/*
Function responsible for the static allocation of the AXI Address Range of the
corrseponding FIFO's at the beginning. */
void setup_AXI_Address_Range(int index) {

  unsigned int AXI_START_ARRAY[] = {F0.AddrRange_Start, F1.AddrRange_Start,
                                    F2.AddrRange_Start, F3.AddrRange_Start,
                                    F4.AddrRange_Start, F5.AddrRange_Start};
  unsigned int AXI_END_ARRAY[] = {F0.AddrRange_End, F1.AddrRange_End,
                                  F2.AddrRange_End, F3.AddrRange_End,
                                  F4.AddrRange_End, F5.AddrRange_End};
  unsigned int AXI_REG_START[] = {
      STREAM_FIFO_ADDR_MIN_0, STREAM_FIFO_ADDR_MIN_1, STREAM_FIFO_ADDR_MIN_2,
      STREAM_FIFO_ADDR_MIN_3, STREAM_FIFO_ADDR_MIN_4, PACKET_FIFO_ADDR_MIN};
  unsigned int AXI_REG_END[] = {STREAM_FIFO_ADDR_MAX_0, STREAM_FIFO_ADDR_MAX_1,
                                STREAM_FIFO_ADDR_MAX_2, STREAM_FIFO_ADDR_MAX_3,
                                STREAM_FIFO_ADDR_MAX_4, PACKET_FIFO_ADDR_MAX};

  // Setup AXI Address Range for the FIFOS
  unsigned int i = 0;

  while (i <= index) {
    iowrite32(AXI_START_ARRAY[i], debug_port_base_address + AXI_REG_START[i]);
    iowrite32(AXI_END_ARRAY[i], debug_port_base_address + AXI_REG_END[i]);
    i++;
  }
}

/* Common Function to setup the Streaming FIFO's with a non-zero threshold value
 * initially and also set the registers dynamically later on via the
 * store_threshold endpoint. */
void setup_StreamingFIFO(int index) {
  int value = 0;
  unsigned int fifo_registers[] = {
      STREAM_FIFO_THRESHOLD_0, STREAM_FIFO_THRESHOLD_1, STREAM_FIFO_THRESHOLD_2,
      STREAM_FIFO_THRESHOLD_3, STREAM_FIFO_THRESHOLD_4};
  unsigned int STREAM_FIFO_THRESHOLD_ARRAY[] = {
      F0.Threshold, F1.Threshold, F2.Threshold, F3.Threshold, F4.Threshold};
  unsigned int STREAM_TIMEOUT_ARRAY[] = {F0.Timeout, F1.Timeout, F2.Timeout,
                                         F3.Timeout, F4.Timeout};
  unsigned int stream_fifo_registers[] = {
      STREAM_FIFO_TIMER_0, STREAM_FIFO_TIMER_1, STREAM_FIFO_TIMER_2,
      STREAM_FIFO_TIMER_3, STREAM_FIFO_TIMER_4};

  value |= ((STREAM_FIFO_THRESHOLD_ARRAY[index] / BYTE_PER_WATERMARK_UNIT) &
              GENMASK(15, 0));
  iowrite32(value, debug_port_base_address + fifo_registers[index]);
  iowrite32(STREAM_TIMEOUT_ARRAY[index],
            debug_port_base_address + stream_fifo_registers[index]);
}

ssize_t sysfs_show_enabled(struct kobject *kobj, struct kobj_attribute *attr,
                           char *buf) {
  int val;
  CSMLOGINFO(KERN_INFO " Reading - sysfs show func...%s \n", kobj->name);
  val = snprintf(buf, sizeof(root.enabled), "%d\n", root.enabled);
  CSMLOGINFO(KERN_INFO " Sysfs show func returned %d \n", val);
  return val;
}

/*
Source Address Endpoints – (/sys/kernel/debugeth/L2L3HeaderInfo/L3_Info/saddr)
Description: This endpoint is responsible for the setting up of the Source
Address (saddr) value in the i-th FIFO’s. Access Type: Read/Write Format: String
Example Write:
For L3 Headers, if the source address is 255.255.192.100,
# echo 255.255.192.100 > /sys/kernel/debugeth/L2L3HeaderInfo/L3_Info/saddr
Example Read:
# cat /sys/kernel/debugeth/L2L3HeaderInfo/L3_Info/saddr (Expected output is
255.255.192.100.)
*/
ssize_t sysfs_show_saddr(struct kobject *kobj, struct kobj_attribute *attr,
                         char *buf) {
  int val = -1;
  char L3_show_value[100] = "";
  char L2_show_value[100] = "";
  char tmp_L3[4][20];
  char tmp_L2[6][20];
  int i;

  CSMLOGINFO(KERN_INFO " Reading - sysfs show func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "L2_Info", Kobj_Name_L2_Info_Size)) {
    for (i = 0; i <= L2_size; i++) {
      snprintf(tmp_L2[i], sizeof(L2.saddr), "%x", L2.saddr[i]);
      strlcat(L2_show_value, tmp_L2[i], sizeof(L2_show_value));
      if (i != L2_size)
        strlcat(L2_show_value, ":", sizeof(L2_show_value));
    }
    val = snprintf(buf, sizeof(L2_show_value), "%s\n", L2_show_value);
  } else if (!strncmp(kobj->name, "L3_Info", Kobj_Name_L3_Info_Size)) {
    for (i = 0; i <= L3_size; i++) {
      snprintf(tmp_L3[i], sizeof(L3.saddr), "%d", L3.saddr[i]);
      strlcat(L3_show_value, tmp_L3[i], sizeof(L3_show_value));
      if (i != L3_size)
        strlcat(L3_show_value, ".", sizeof(L3_show_value));
    }
    val = snprintf(buf, sizeof(L3_show_value), "%s\n", L3_show_value);
  }
  CSMLOGINFO(KERN_INFO " Sysfs show func returned %d \n", val);
  return val;
}

ssize_t sysfs_store_saddr(struct kobject *kobj, struct kobj_attribute *attr,
                          const char *buf, size_t count) {
  int index;
  u32 lower_SA = 0;
  u32 upper_SA = 0;
  u32 prev_val = 0;
  u64 L3_val;
  char L3_seps[2] = ".";
  char L2_seps[2] = ":";
  char *token;
  char token_string[100] = "";
  u64 var;
  int i = 0;
  unsigned int L2_SA_ADDR_HI_ARRAY[] = {
      L2_SA_ADDR_HI_0, L2_SA_ADDR_HI_1, L2_SA_ADDR_HI_2, L2_SA_ADDR_HI_3,
      L2_SA_ADDR_HI_4, L2_SA_ADDR_HI_5, L2_SA_ADDR_HI_6, L2_SA_ADDR_HI_7};
  unsigned int L2_SA_ADDR_LO_ARRAY[] = {
      L2_SA_ADDR_LO_0, L2_SA_ADDR_LO_1, L2_SA_ADDR_LO_2, L2_SA_ADDR_LO_3,
      L2_SA_ADDR_LO_4, L2_SA_ADDR_LO_5, L2_SA_ADDR_LO_6, L2_SA_ADDR_LO_7};
  unsigned int L3_IPV4_SA_ARRAY[] = {ETH_DBG_IPV4_SA_0, ETH_DBG_IPV4_SA_1,
                                     ETH_DBG_IPV4_SA_2};

  CSMLOGINFO(KERN_INFO " Reading - sysfs store func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "L2_Info", Kobj_Name_L2_Info_Size)) {
    strlcpy(token_string, buf, MIN(sizeof(token_string), strlen(buf)+1));
    token = mtip_sysfs_strtok(token_string, L2_seps);
    while (token != NULL) {
      sscanf(token, "%x", &var);
      CSMLOGINFO("Value passed in saddr is %x at index %d", var, i);
      L2.saddr[i++] = var;
      token = mtip_sysfs_strtok(NULL, L2_seps);
    }
    for (index = 0; index < MAX_FIFO_COUNT; index++) {
      lower_SA = (L2.saddr[0]) | (L2.saddr[1] << 8) | (L2.saddr[2] << 16) |
                 (L2.saddr[3] << 24);
      upper_SA = (L2.saddr[4]) | (L2.saddr[5] << 8);
      // write the lower bits
      CSMLOGINFO("Value in lowerSA is %x", lower_SA);
      CSMLOGINFO("Value in UpperSA is %x", upper_SA);
      iowrite32(lower_SA, debug_port_base_address + L2_SA_ADDR_LO_ARRAY[index]);
      // write the upper bits
      prev_val =
          (u32)ioread32(debug_port_base_address + L2_SA_ADDR_HI_ARRAY[index]);
      prev_val &= (~(GENMASK(15, 0)));
      prev_val |= ((upper_SA & GENMASK(15, 0)));
      iowrite32(prev_val, debug_port_base_address + L2_SA_ADDR_HI_ARRAY[index]);
    }
  } else if (!strncmp(kobj->name, "L3_Info", Kobj_Name_L3_Info_Size)) {
    strlcpy(token_string, buf, MIN(sizeof(token_string), strlen(buf)+1));
    token = mtip_sysfs_strtok(token_string, L3_seps);
    while (token != NULL) {
      sscanf(token, "%d", &var);
      CSMLOGINFO("Value passed in saddr is %x at index %d", var, i);
      L3.saddr[i++] = var;
      token = mtip_sysfs_strtok(NULL, L3_seps);
    }
    L3_val = (L3.saddr[0]) | (L3.saddr[1] << 8) | (L3.saddr[2] << 16) |
             (L3.saddr[3] << 24);
    CSMLOGINFO("Value in L3_Val is %x", L3_val);
    for (index = 0; index < MAX_PACKET_FIFO_COUNT; index++) {
      iowrite32(L3_val, debug_port_base_address + L3_IPV4_SA_ARRAY[index]);
    }
  }

  return count;
}

/*
Destination Address Endpoints –
(/sys/kernel/debugeth/L2L3HeaderInfo/L3_Info/daddr) Description: This endpoint
is responsible for the setting up of the Destination Address (daddr) value in
the i-th FIFO’s. Access Type: Read/Write Format: String Example Write: For L3
Headers, if the source address is 255.255.192.100 # echo 255.255.192.100 >
/sys/kernel/debugeth/L2L3HeaderInfo/L3_Info/daddr Example Read: # cat
/sys/kernel/debugeth/L2L3HeaderInfo/L3_Info/daddr (Expected output is
255.255.192.100.)
*/
ssize_t sysfs_show_daddr(struct kobject *kobj, struct kobj_attribute *attr,
                         char *buf) {
  int val = -1;
  char L3_show_value[100] = "";
  char L2_show_value[100] = "";
  char tmp_L3[4][20];
  char tmp_L2[6][20];
  int i;

  CSMLOGINFO(KERN_INFO " Reading - sysfs show func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "L2_Info", Kobj_Name_L2_Info_Size)) {
    for (i = 0; i <= L2_size; i++) {
      snprintf(tmp_L2[i], sizeof(L2.daddr), "%x", L2.daddr[i]);
      strlcat(L2_show_value, tmp_L2[i], sizeof(L2_show_value));
      if (i != L2_size)
        strlcat(L2_show_value, ":", sizeof(L2_show_value));
    }
    val = snprintf(buf, sizeof(L2_show_value), "%s\n", L2_show_value);
  } else if (!strncmp(kobj->name, "L3_Info", Kobj_Name_L3_Info_Size)) {
    for (i = 0; i <= L3_size; i++) {
      snprintf(tmp_L3[i], sizeof(L3.daddr), "%d", L3.daddr[i]);
      strlcat(L3_show_value, tmp_L3[i], sizeof(L3_show_value));
      if (i != L3_size)
        strlcat(L3_show_value, ".", sizeof(L3_show_value));
    }
    val = snprintf(buf, sizeof(L3_show_value), "%s\n", L3_show_value);
  }
  CSMLOGINFO(KERN_INFO " Sysfs show func returned %d \n", val);
  return val;
}

ssize_t sysfs_store_daddr(struct kobject *kobj, struct kobj_attribute *attr,
                          const char *buf, size_t count) {
  int index;
  u32 lower_DA = 0;
  u32 upper_DA = 0;
  u64 L3_val;
  char L3_seps[2] = ".";
  char L2_seps[2] = ":";
  char *token;
  char token_string[100] = "";
  int var;
  int i = 0;
  unsigned int L2_DA_ADDR_HI_ARRAY[] = {
      L2_DA_ADDR_HI_0, L2_DA_ADDR_HI_1, L2_DA_ADDR_HI_2,
      L2_DA_ADDR_HI_3, L2_DA_ADDR_HI_4, L2_DA_ADDR_HI_5,
      L2_DA_ADDR_HI_6, L2_DA_ADDR_HI_7, L2_DA_ADDR_HI_8};
  unsigned int L2_DA_ADDR_LO_ARRAY[] = {
      L2_DA_ADDR_LO_0, L2_DA_ADDR_LO_1, L2_DA_ADDR_LO_2,
      L2_DA_ADDR_LO_3, L2_DA_ADDR_LO_4, L2_DA_ADDR_LO_5,
      L2_DA_ADDR_LO_6, L2_DA_ADDR_LO_7, L2_DA_ADDR_LO_8};
  unsigned int L3_IPV4_DA_ARRAY[] = {ETH_DBG_IPV4_DA_0, ETH_DBG_IPV4_DA_1,
                                     ETH_DBG_IPV4_DA_2};

  CSMLOGINFO(KERN_INFO " Reading - sysfs store func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "L2_Info", Kobj_Name_L2_Info_Size)) {
    strlcpy(token_string, buf, MIN(sizeof(token_string), strlen(buf)+1));
    token = mtip_sysfs_strtok(token_string, L2_seps);
    while (token != NULL) {
      sscanf(token, "%x", &var);
      L2.daddr[i++] = var;
      token = mtip_sysfs_strtok(NULL, L2_seps);
    }
    for (index = 0; index < MAX_FIFO_COUNT; index++) {
      lower_DA = (L2.daddr[0]) | (L2.daddr[1] << 8) | (L2.daddr[2] << 16) |
                 (L2.daddr[3] << 24);
      upper_DA = (L2.daddr[4]) | (L2.daddr[5] << 8);
      // write the lower bits
      iowrite32(lower_DA, debug_port_base_address + L2_DA_ADDR_LO_ARRAY[index]);
      // write the upper bits
      iowrite32(upper_DA, debug_port_base_address + L2_DA_ADDR_HI_ARRAY[index]);
    }
  } else if (!strncmp(kobj->name, "L3_Info", Kobj_Name_L3_Info_Size)) {
    strlcpy(token_string, buf, MIN(sizeof(token_string), strlen(buf)+1));
    token = mtip_sysfs_strtok(token_string, L3_seps);
    while (token != NULL) {
      sscanf(token, "%d", &var);
      L3.daddr[i++] = var;
      token = mtip_sysfs_strtok(NULL, L3_seps);
    }
    L3_val = (L3.daddr[0]) | (L3.daddr[1] << 8) | (L3.daddr[2] << 16) |
             (L3.daddr[3] << 24);
    for (index = 0; index < MAX_PACKET_FIFO_COUNT; index++) {
      iowrite32(L3_val, debug_port_base_address + L3_IPV4_DA_ARRAY[index]);
    }
  }

  return count;
}

/*
Source Port Endpoints – (/sys/kernel/debugeth/L2L3HeaderInfo/L3_Info/sport)
Description: This endpoint is responsible for the setting up of the Source Port
(sport) value in the i-th FIFO’s. Access Type: Read/Write Format: Integer
Example Write:
For L3 Headers, if the source port is 24456, applications are expected to write
in the format as specified below. # echo 24456 >
/sys/kernel/debugeth/L2L3HeaderInfo/L3_Info/sport Example Read: # cat
/sys/kernel/debugeth/L2L3HeaderInfo/L3_Info/sport (Expected output is 24456.)
*/
ssize_t sysfs_show_sport(struct kobject *kobj, struct kobj_attribute *attr,
                         char *buf) {
  int val;
  CSMLOGINFO(KERN_INFO " Reading - sysfs show func...%s \n", kobj->name);
  val = snprintf(buf, sizeof(L3.sport), "%d\n", L3.sport);
  CSMLOGINFO(KERN_INFO " Sysfs show func returned %d \n", val);
  return val;
}

ssize_t sysfs_store_sport(struct kobject *kobj, struct kobj_attribute *attr,
                          const char *buf, size_t count) {
  int data;
  int index;
  int val;
  unsigned int UDP_SP_DP_ARRAY[] = {DBG_UDP_SP_DP_0, DBG_UDP_SP_DP_1,
                                    DBG_UDP_SP_DP_2};

  CSMLOGINFO(KERN_INFO " Reading - sysfs store func...%s \n", kobj->name);
  sscanf(buf, "%d", &L3.sport);
  data = L3.sport;
  for (index = 0; index < MAX_PACKET_FIFO_COUNT; index++) {
    val = (u32)ioread32(debug_port_base_address + UDP_SP_DP_ARRAY[index]);
    val &= (~(GENMASK(15, 0)));
    val |= ((data & GENMASK(15, 8))>>8);
    val |= ((data & GENMASK(7, 0))<<8);
    iowrite32(val, debug_port_base_address + UDP_SP_DP_ARRAY[index]);
  }

  return count;
}

/*
Destination Port Endpoints – (/sys/kernel/debugeth/L2L3HeaderInfo/L3_Info/dport)
Description: This endpoint is responsible for the setting up of the Destination
Port (dport) value in the i-th FIFO’s. Access Type: Read/Write Format: Integer
Example Write:
For L3 Headers, if the destination port is 24456, applications are expected to
write in the format as specified below. # echo 24456
/sys/kernel/debugeth/L2L3HeaderInfo/L3_Info/dport Example Read: # cat
/sys/kernel/debugeth/L2L3HeaderInfo/L3_Info/dport (Expected output is 24456.)
*/
ssize_t sysfs_show_dport(struct kobject *kobj, struct kobj_attribute *attr,
                         char *buf) {
  int val;
  CSMLOGINFO(KERN_INFO " Reading - sysfs show func...%s \n", kobj->name);
  val = snprintf(buf, sizeof(L3.dport), "%d\n", L3.dport);
  CSMLOGINFO(KERN_INFO " Sysfs show func returned %d \n", val);
  return val;
}

ssize_t sysfs_store_dport(struct kobject *kobj, struct kobj_attribute *attr,
                          const char *buf, size_t count) {
  int data;
  int index;
  int val;
  unsigned int UDP_SP_DP_ARRAY[] = {DBG_UDP_SP_DP_0, DBG_UDP_SP_DP_1,
                                    DBG_UDP_SP_DP_2};

  CSMLOGINFO(KERN_INFO " Reading - sysfs store func...%s \n", kobj->name);
  sscanf(buf, "%d", &L3.dport);
  data = L3.dport;
  for (index = 0; index < MAX_PACKET_FIFO_COUNT; index++) {
    val = (u32)ioread32(debug_port_base_address + UDP_SP_DP_ARRAY[index]);
    val &= (~(GENMASK(31, 16)));
    val |= ((data & GENMASK(15, 8))<< 8);
    val |= ((data & GENMASK(7, 0))<< 24);
    iowrite32(val, debug_port_base_address + UDP_SP_DP_ARRAY[index]);
  }

  return count;
}

/*
Status Endpoint – (/sys/kernel/debugeth/FIFO_#/status)
Description: This endpoint is responsible for checking the values of the Status
(Link UP/DOWN) in the i-th Streaming FIFO’s. Access Type: Read Format: Integer
Example Read:
# cat /sys/kernel/debugeth/FIFO_#/status
*/
ssize_t sysfs_show_status(struct kobject *kobj, struct kobj_attribute *attr,
                          char *buf) {
  int val = -1;
  CSMLOGINFO(KERN_INFO " Reading - sysfs show func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "FIFO_0", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F0.status), "%d\n", F0.status);
  } else if (!strncmp(kobj->name, "FIFO_1", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F1.status), "%d\n", F1.status);
  } else if (!strncmp(kobj->name, "FIFO_2", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F2.status), "%d\n", F2.status);
  } else if (!strncmp(kobj->name, "FIFO_3", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F3.status), "%d\n", F3.status);
  } else if (!strncmp(kobj->name, "FIFO_4", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F4.status), "%d\n", F4.status);
  } else if (!strncmp(kobj->name, "FIFO_5", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F5.status), "%d\n", F5.status);
  } else if (!strncmp(kobj->name, "FIFO_6", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F6.status), "%d\n", F6.status);
  } else if (!strncmp(kobj->name, "FIFO_7", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F7.status), "%d\n", F7.status);
  }
  CSMLOGINFO(KERN_INFO " Sysfs show func returned %d \n", val);
  return val;
}

/*
Flush Endpoint - (/sys/kernel/debugeth/FIFO_#/flush)
Description: This endpoint is responsible for flushing the values in the i-th
FIFO’s. Access Type: Write Format: Integer Example Write: # echo 1 >
/sys/kernel/debugeth/FIFO_#/flush
*/
ssize_t sysfs_store_flush(struct kobject *kobj, struct kobj_attribute *attr,
                          const char *buf, size_t count) {
  int val = count;
  CSMLOGINFO(KERN_INFO " Reading - sysfs store func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "FIFO_0", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F0.flush);
    sysfs_store_flush_register_set(FIFO_0, &val);
  } else if (!strncmp(kobj->name, "FIFO_1", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F1.flush);
    sysfs_store_flush_register_set(FIFO_1, &val);
  } else if (!strncmp(kobj->name, "FIFO_2", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F2.flush);
    sysfs_store_flush_register_set(FIFO_2, &val);
  } else if (!strncmp(kobj->name, "FIFO_3", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F3.flush);
    sysfs_store_flush_register_set(FIFO_3, &val);
  } else if (!strncmp(kobj->name, "FIFO_4", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F4.flush);
    sysfs_store_flush_register_set(FIFO_4, &val);
  } else if (!strncmp(kobj->name, "FIFO_5", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F5.flush);
    sysfs_store_flush_register_set(FIFO_5, &val);
  } else if (!strncmp(kobj->name, "FIFO_6", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F6.flush);
    sysfs_store_flush_register_set(FIFO_6, &val);
  } else if (!strncmp(kobj->name, "FIFO_7", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F7.flush);
    sysfs_store_flush_register_set(FIFO_7, &val);
  }

  return count;
}

void sysfs_store_flush_register_set(int index, int* ret_val) {
  int data = 0;
  int set_bit = 1;
  int flush_bit;
  u32 v;

  // Setting the bit
  data |= set_bit << index;
  iowrite32(data, debug_port_base_address + DBG_ETH_DBG_SW_FLUSH);

  // Waiting till poll done
  if (index >= FIFO_0 && index <= FIFO_4) {
    // Changes to accomodate corresponding FIFO's instead of all FIFO's.
    flush_bit = 1;
    if (readl_poll_timeout(debug_port_base_address + DBG_ETH_DBG_FIFO_STATUS, v,
                           (v & (flush_bit << index)), 100, 10000)){
      *ret_val = -EBUSY;
    }
  } else if (index >= FIFO_5 && index <= FIFO_7) {
    // Changes to accomodate corresponding FIFO's instead of all FIFO's.
    flush_bit = 0x400;
    if (readl_poll_timeout(debug_port_base_address + DBG_ETH_DBG_FIFO_STATUS, v,
                           (v & (flush_bit << (index-FIFO_5))), 100, 10000)){
      *ret_val = -EBUSY;
    }
  }

  // Clearing the bit
  data &= ~(set_bit << index);
  iowrite32(data, debug_port_base_address + DBG_ETH_DBG_SW_FLUSH);

  return;
}

/*
TXcount Endpoint – (/sys/kernel/debugeth/FIFO_#/txcount)
Description: This endpoint is responsible for returning the TX count value in
the i-th FIFO’s. Access Type: Read Format: Integer Example Read: # cat
/sys/kernel/debugeth/FIFO_#/txcount (Expected output is the Txcount in bytes.)
*/
ssize_t sysfs_show_txcount(struct kobject *kobj, struct kobj_attribute *attr,
                           char *buf) {
  int data = 0;
  ssize_t buff_size = 0;

  CSMLOGINFO(KERN_INFO " Reading - sysfs show func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "FIFO_0", Kobj_Name_FIFO_Size)) {
    data = (int)ioread32(debug_port_base_address + STREAM_PKT_CNT_0);
    F0.txcount = data;
    buff_size = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F0.txcount);
  } else if (!strncmp(kobj->name, "FIFO_1", Kobj_Name_FIFO_Size)) {
    data = (int)ioread32(debug_port_base_address + STREAM_PKT_CNT_1);
    F1.txcount = data;
    buff_size = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F1.txcount);
  } else if (!strncmp(kobj->name, "FIFO_2", Kobj_Name_FIFO_Size)) {
    data = (int)ioread32(debug_port_base_address + STREAM_PKT_CNT_2);
    F2.txcount = data;
    buff_size = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F2.txcount);
  } else if (!strncmp(kobj->name, "FIFO_3", Kobj_Name_FIFO_Size)) {
    data = (int)ioread32(debug_port_base_address + STREAM_PKT_CNT_3);
    F3.txcount = data;
    buff_size = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F3.txcount);
  } else if (!strncmp(kobj->name, "FIFO_4", Kobj_Name_FIFO_Size)) {
    data = (int)ioread32(debug_port_base_address + STREAM_PKT_CNT_4);
    F4.txcount = data;
    buff_size = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F4.txcount);
  } else if (!strncmp(kobj->name, "FIFO_5", Kobj_Name_FIFO_Size)) {
    data = (int)ioread32(debug_port_base_address + PACKET_FIFO1_PKT_CNT);
    F5.txcount = data;
    buff_size = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F5.txcount);
  } else if (!strncmp(kobj->name, "FIFO_6", Kobj_Name_FIFO_Size)) {
    data = (int)ioread32(debug_port_base_address + PACKET_FIFO2_PKT_CNT);
    F6.txcount = data;
    buff_size = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F6.txcount);
  } else if (!strncmp(kobj->name, "FIFO_7", Kobj_Name_FIFO_Size)) {
    data = (int)ioread32(debug_port_base_address + PACKET_FIFO3_PKT_CNT);
    F6.txcount = data;
    buff_size = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F7.txcount);
  }
  CSMLOGINFO(KERN_INFO " Sysfs show func returned %d with size %d \n", data,
             buff_size);
  return buff_size;
}

/*
Address Range Start Endpoints – (/sys/kernel/debugeth/FIFO_#/AddrRangeStart)
Description: This endpoint is responsible for updating the Address Range
Starting value in the i-th FIFO’s. Access Type: Read/Write Format: Integer
Example Write:
# echo 1000 > /sys/kernel/debugeth/FIFO_#/AddrRangeStart
Example Read:
# cat /sys/kernel/debugeth/FIFO_#/AddrRangeStart (Expected output is the Address
Range Starting value.)
*/
ssize_t sysfs_show_AddrRange_Start(struct kobject *kobj,
                                   struct kobj_attribute *attr, char *buf) {
  int val = -1;
  CSMLOGINFO(KERN_INFO " Reading - sysfs show func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "FIFO_0", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F0.AddrRange_Start); //%x
  } else if (!strncmp(kobj->name, "FIFO_1", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F1.AddrRange_Start);
  } else if (!strncmp(kobj->name, "FIFO_2", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F2.AddrRange_Start);
  } else if (!strncmp(kobj->name, "FIFO_3", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F3.AddrRange_Start);
  } else if (!strncmp(kobj->name, "FIFO_4", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F4.AddrRange_Start);
  } else if (!strncmp(kobj->name, "FIFO_5", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F5.AddrRange_Start);
  } else if (!strncmp(kobj->name, "FIFO_6", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F6.AddrRange_Start);
  } else if (!strncmp(kobj->name, "FIFO_7", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F7.AddrRange_Start);
  }

  CSMLOGINFO(KERN_INFO " Sysfs show func returned %d \n", val);
  return val;
}

ssize_t sysfs_store_AddrRange_Start(struct kobject *kobj,
                                    struct kobj_attribute *attr,
                                    const char *buf, size_t count) {

  CSMLOGINFO(KERN_INFO " Reading - sysfs store func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "FIFO_0", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F0.AddrRange_Start);
    iowrite32(F0.AddrRange_Start, debug_port_base_address + STREAM_FIFO_ADDR_MIN_0);
  } else if (!strncmp(kobj->name, "FIFO_1", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F1.AddrRange_Start);
    iowrite32(F1.AddrRange_Start, debug_port_base_address + STREAM_FIFO_ADDR_MIN_1);
  } else if (!strncmp(kobj->name, "FIFO_2", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F2.AddrRange_Start);
    iowrite32(F2.AddrRange_Start, debug_port_base_address + STREAM_FIFO_ADDR_MIN_2);
  } else if (!strncmp(kobj->name, "FIFO_3", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F3.AddrRange_Start);
    iowrite32(F3.AddrRange_Start, debug_port_base_address + STREAM_FIFO_ADDR_MIN_3);
  } else if (!strncmp(kobj->name, "FIFO_4", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F4.AddrRange_Start);
    iowrite32(F4.AddrRange_Start, debug_port_base_address + STREAM_FIFO_ADDR_MIN_4);
  } else if (!strncmp(kobj->name, "FIFO_5", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F5.AddrRange_Start);
    iowrite32(F5.AddrRange_Start, debug_port_base_address + PACKET_FIFO_ADDR_MIN);
  } else if (!strncmp(kobj->name, "FIFO_6", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F6.AddrRange_Start);
    iowrite32(F6.AddrRange_Start, debug_port_base_address + PACKET_FIFO_ADDR_MIN);
  } else if (!strncmp(kobj->name, "FIFO_7", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F7.AddrRange_Start);
    iowrite32(F7.AddrRange_Start, debug_port_base_address + PACKET_FIFO_ADDR_MIN);
  }

  return count;
}

/*
Address Range End Endpoints– (/sys/kernel/debugeth/FIFO_#/AddrRangeEnd)
Description: This endpoint is responsible for updating the Address Range Ending
value in the i-th FIFO’s. Access Type: Read/Write Format: Integer Example Write:
# echo 1000 > /sys/kernel/debugeth/FIFO_#/AddrRangeEnd
Example Read:
# cat /sys/kernel/debugeth/FIFO_#/AddrRangeEnd (Expected output is the Address
Range Ending value.)
*/
ssize_t sysfs_show_AddrRange_End(struct kobject *kobj,
                                 struct kobj_attribute *attr, char *buf) {
  int val = -1;
  CSMLOGINFO(KERN_INFO " Reading - sysfs show func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "FIFO_0", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F0.AddrRange_End);
  } else if (!strncmp(kobj->name, "FIFO_1", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F1.AddrRange_End);
  } else if (!strncmp(kobj->name, "FIFO_2", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F2.AddrRange_End);
  } else if (!strncmp(kobj->name, "FIFO_3", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F3.AddrRange_End);
  } else if (!strncmp(kobj->name, "FIFO_4", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F4.AddrRange_End);
  } else if (!strncmp(kobj->name, "FIFO_5", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F5.AddrRange_End);
  } else if (!strncmp(kobj->name, "FIFO_6", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F6.AddrRange_End);
  } else if (!strncmp(kobj->name, "FIFO_7", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "0x%x\n", F7.AddrRange_End);
  }

  CSMLOGINFO(KERN_INFO " Sysfs show func returned %d \n", val);
  return val;
}

ssize_t sysfs_store_AddrRange_End(struct kobject *kobj,
                                  struct kobj_attribute *attr, const char *buf,
                                  size_t count) {
  CSMLOGINFO(KERN_INFO " Reading - sysfs store func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "FIFO_0", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F0.AddrRange_End);
    iowrite32(F0.AddrRange_End, debug_port_base_address + STREAM_FIFO_ADDR_MAX_0);
  } else if (!strncmp(kobj->name, "FIFO_1", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F1.AddrRange_End);
    iowrite32(F1.AddrRange_End, debug_port_base_address + STREAM_FIFO_ADDR_MAX_1);
  } else if (!strncmp(kobj->name, "FIFO_2", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F2.AddrRange_End);
    iowrite32(F2.AddrRange_End, debug_port_base_address + STREAM_FIFO_ADDR_MAX_2);
  } else if (!strncmp(kobj->name, "FIFO_3", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F3.AddrRange_End);
    iowrite32(F3.AddrRange_End, debug_port_base_address + STREAM_FIFO_ADDR_MAX_3);
  } else if (!strncmp(kobj->name, "FIFO_4", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F4.AddrRange_End);
    iowrite32(F4.AddrRange_End, debug_port_base_address + STREAM_FIFO_ADDR_MAX_4);
  } else if (!strncmp(kobj->name, "FIFO_5", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F5.AddrRange_End);
    iowrite32(F5.AddrRange_End, debug_port_base_address + PACKET_FIFO_ADDR_MAX);
  } else if (!strncmp(kobj->name, "FIFO_6", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F6.AddrRange_End);
    iowrite32(F6.AddrRange_End, debug_port_base_address + PACKET_FIFO_ADDR_MAX);
  } else if (!strncmp(kobj->name, "FIFO_7", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%x", &F7.AddrRange_End);
    iowrite32(F7.AddrRange_End, debug_port_base_address + PACKET_FIFO_ADDR_MAX);
  }

  return count;
}
/*
OverFlowInterrupt Endpoint – (/sys/kernel/debugeth/FIFO_#/OverFlowInterrupt)
Description: This endpoint is responsible for updating the OverFlowInterrupt
value in the i-th FIFO’s. Access Type: Read Format: Integer Example Read: # cat
/sys/kernel/debugeth/FIFO_#/ OverFlowInterrupt (Expected output is the
OverFlowInterrupt value.)
*/
ssize_t sysfs_show_OverFlowInterrupt(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf) {
  int val = -1;
  CSMLOGINFO(KERN_INFO " Reading - sysfs show func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "FIFO_0", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F0.OverFlowInterrupt), "%d\n", F0.OverFlowInterrupt);
  } else if (!strncmp(kobj->name, "FIFO_1", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F1.OverFlowInterrupt), "%d\n", F1.OverFlowInterrupt);
  } else if (!strncmp(kobj->name, "FIFO_2", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F2.OverFlowInterrupt), "%d\n", F2.OverFlowInterrupt);
  } else if (!strncmp(kobj->name, "FIFO_3", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F3.OverFlowInterrupt), "%d\n", F3.OverFlowInterrupt);
  } else if (!strncmp(kobj->name, "FIFO_4", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F4.OverFlowInterrupt), "%d\n", F4.OverFlowInterrupt);
  } else if (!strncmp(kobj->name, "FIFO_5", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F5.OverFlowInterrupt), "%d\n", F5.OverFlowInterrupt);
  } else if (!strncmp(kobj->name, "FIFO_6", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F6.OverFlowInterrupt), "%d\n", F6.OverFlowInterrupt);
  } else if (!strncmp(kobj->name, "FIFO_7", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F7.OverFlowInterrupt), "%d\n", F7.OverFlowInterrupt);
  }
  CSMLOGINFO(KERN_INFO " Sysfs show func returned %d \n", val);
  return val;
}

/*
Threshold Endpoints - (/sys/kernel/debugeth/FIFO_#/Threshold)
Description: This endpoint is responsible for the setting up of the “Threshold”
value in the i-th Streaming FIFO. Access Type: Read/Write Format: Integer
Example Write: # echo 1 /sys/kernel/debugeth/FIFO_#/Threshold
Example Read: cat /sys/kernel/debugeth/FIFO_#/Threshold (Expected Output is 1)
*/
ssize_t sysfs_show_Threshold(struct kobject *kobj, struct kobj_attribute *attr,
                             char *buf) {
  int val = -1;
  CSMLOGINFO(KERN_INFO " Reading - sysfs show func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "FIFO_0", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F0.Threshold);
  } else if (!strncmp(kobj->name, "FIFO_1", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F1.Threshold);
  } else if (!strncmp(kobj->name, "FIFO_2", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F2.Threshold);
  } else if (!strncmp(kobj->name, "FIFO_3", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F3.Threshold);
  } else if (!strncmp(kobj->name, "FIFO_4", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F4.Threshold);
  }
  CSMLOGINFO(KERN_INFO " Sysfs show func returned %d \n", val);
  return val;
}

ssize_t sysfs_store_Threshold(struct kobject *kobj, struct kobj_attribute *attr,
                              const char *buf, size_t count) {
  unsigned int temp;
  CSMLOGINFO(KERN_INFO " Reading - sysfs store func...%s \n", kobj->name);
  sscanf(buf, "%d", &temp);
  if (temp < MINIMUM_PACKET_SIZE || temp > MAXIMUM_PACKET_SIZE)
    return EINVAL;

  if (!strncmp(kobj->name, "FIFO_0", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F0.Threshold);
    setup_StreamingFIFO(FIFO_0);
  } else if (!strncmp(kobj->name, "FIFO_1", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F1.Threshold);
    setup_StreamingFIFO(FIFO_1);
  } else if (!strncmp(kobj->name, "FIFO_2", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F2.Threshold);
    setup_StreamingFIFO(FIFO_2);
  } else if (!strncmp(kobj->name, "FIFO_3", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F3.Threshold);
    setup_StreamingFIFO(FIFO_3);
  } else if (!strncmp(kobj->name, "FIFO_4", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F4.Threshold);
    setup_StreamingFIFO(FIFO_4);
  }
  return count;
}

/*
Timeout Endpoints – (/sys/kernel/debugeth/FIFO_#/Timeout)
Description: This endpoint is responsible for setting the values of the Timeout
in the i-th Streaming FIFO’s. Access Type: Write Format: Integer (Value is in
number of clock cycles for that corresponding interface.) Example Write: # echo
100 > /sys/kernel/debugeth/FIFO_#/Timeout Example Read: # cat
/sys/kernel/debugeth/FIFO_#/Timeout
*/
ssize_t sysfs_show_Timeout(struct kobject *kobj, struct kobj_attribute *attr,
                           char *buf) {
  int val = -1;
  CSMLOGINFO(KERN_INFO " Reading - sysfs show func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "FIFO_0", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F0.Timeout);
  } else if (!strncmp(kobj->name, "FIFO_1", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F1.Timeout);
  } else if (!strncmp(kobj->name, "FIFO_2", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F2.Timeout);
  } else if (!strncmp(kobj->name, "FIFO_3", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F3.Timeout);
  } else if (!strncmp(kobj->name, "FIFO_4", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", F4.Timeout);
  }
  CSMLOGINFO(KERN_INFO " Sysfs show func returned %d \n", val);
  return val;
}

ssize_t sysfs_store_Timeout(struct kobject *kobj, struct kobj_attribute *attr,
                            const char *buf, size_t count) {
  CSMLOGINFO(KERN_INFO " Reading - sysfs store func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "FIFO_0", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F0.Timeout);
    iowrite32(F0.Timeout, debug_port_base_address + STREAM_FIFO_TIMER_0);
  } else if (!strncmp(kobj->name, "FIFO_1", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F1.Timeout);
    iowrite32(F1.Timeout, debug_port_base_address + STREAM_FIFO_TIMER_1);
  } else if (!strncmp(kobj->name, "FIFO_2", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F2.Timeout);
    iowrite32(F2.Timeout, debug_port_base_address + STREAM_FIFO_TIMER_2);
  } else if (!strncmp(kobj->name, "FIFO_3", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F3.Timeout);
    iowrite32(F3.Timeout, debug_port_base_address + STREAM_FIFO_TIMER_3);
  } else if (!strncmp(kobj->name, "FIFO_4", Kobj_Name_FIFO_Size)) {
    sscanf(buf, "%d", &F4.Timeout);
    iowrite32(F4.Timeout, debug_port_base_address + STREAM_FIFO_TIMER_4);
  }
  return count;
}

/*
VLAN-ID Endpoints – (/sys/kernel/debugeth/FIFO_#/VlanID)
Description: This endpoint is responsible for the setting up of the VLAN-ID’s
value in the i-th FIFO’s. Access Type: Read/Write Format: Integer (0-4095 in
decimals) Example Write: VLAN_ID = TPID( 0x8100 Default) + PCP ( 0 Default ) +
DEI ( 0 Default ) + VID ( User Input ) # echo 1024 >
/sys/kernel/debugeth/FIFO_#/VlanID Example Read: # cat
/sys/kernel/debugeth/FIFO_#/VlanID (Expected output is 1024.)
*/
ssize_t sysfs_show_vlanID(struct kobject *kobj, struct kobj_attribute *attr,
                          char *buf) {
  int val = -1;

  CSMLOGINFO(KERN_INFO " Reading - sysfs show func...%s \n", kobj->name);
  if (!strncmp(kobj->name, "FIFO_0", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F0.vlanID), "%d\n", F0.vlanID);
  } else if (!strncmp(kobj->name, "FIFO_1", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F1.vlanID), "%d\n", F1.vlanID);
  } else if (!strncmp(kobj->name, "FIFO_2", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F2.vlanID), "%d\n", F2.vlanID);
  } else if (!strncmp(kobj->name, "FIFO_3", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F3.vlanID), "%d\n", F3.vlanID);
  } else if (!strncmp(kobj->name, "FIFO_4", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F4.vlanID), "%d\n", F4.vlanID);
  } else if (!strncmp(kobj->name, "FIFO_5", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F5.vlanID), "%d\n", F5.vlanID);
  } else if (!strncmp(kobj->name, "FIFO_6", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F6.vlanID), "%d\n", F6.vlanID);
  } else if (!strncmp(kobj->name, "FIFO_7", Kobj_Name_FIFO_Size)) {
    val = snprintf(buf, sizeof(F7.vlanID), "%d\n", F7.vlanID);
  }
  CSMLOGINFO(KERN_INFO " Sysfs show func returned %d \n", val);
  return val;
}

void sysfs_store_vlanID_Register_Set(const char *buf, u64 *FIFO_vlanID, u32 reg1,
                                    u32 reg2) {
  u32 val;
  u32 vlan_id = 0;
  u32 vlan_tag = 1;
  u32 tpid = 0x8100;
  u32 vid = 0;
  u32 tci_val = 0;
  u32 tpid_val = 0;

  sscanf(buf, "%d", &vid);

  // Read the current VLAN tag enabled bit
  val = (u32)ioread32(debug_port_base_address + reg2);
  CSMLOGINFO(KERN_INFO " sysfs_store_vlanID_Register_Set - read reg2 %x", val);

  // Reset the VLAN tag enabled bit
  val &= (~(GENMASK(16, 16)));

  /* If VLAN ID is valid, set the VLAN tag enabled bit again and
     assign the VLAN ID value */
  if(vid != 0){
    val |= ((vlan_tag) << 16);
    tpid_val = (((tpid >> 8) & GENMASK(7, 0)) | (((tpid & GENMASK(7, 0)) << 8)));
    tci_val = (((vid >> 8) & GENMASK(7, 0)) | (((vid & GENMASK(7, 0)) << 8)));
    vlan_id = (tci_val << 16) | tpid_val;
  }

  // Store the VLAN ID in local cache
  *FIFO_vlanID = vid;

  // Store the VLAN ID in reg
  CSMLOGINFO(KERN_INFO " sysfs_store_vlanID_Register_Set - reg1 %x", vlan_id);
  iowrite32(vlan_id, debug_port_base_address + reg1);

  // Store the VLAN ID enabled bit in reg
  CSMLOGINFO(KERN_INFO " sysfs_store_vlanID_Register_Set - write reg2 %x", val);
  iowrite32(val, debug_port_base_address + reg2);

  return;
}

ssize_t sysfs_store_vlanID(struct kobject *kobj, struct kobj_attribute *attr,
                           const char *buf, size_t count) {

  unsigned int temp;
  CSMLOGINFO(KERN_INFO " Reading - sysfs store func...%s \n", kobj->name);
  sscanf(buf, "%d", &temp);
  if(temp >= MINIMUM_VLANID && temp <= MAXIMUM_VLANID) {
   if (!strncmp(kobj->name, "FIFO_0", Kobj_Name_FIFO_Size)) {
     sysfs_store_vlanID_Register_Set(buf, &F0.vlanID, VLAN_TAG_0,
                                           L2_SA_ADDR_HI_0);
   } else if (!strncmp(kobj->name, "FIFO_1", Kobj_Name_FIFO_Size)) {
     sysfs_store_vlanID_Register_Set(buf, &F1.vlanID, VLAN_TAG_1,
                                           L2_SA_ADDR_HI_1);
   } else if (!strncmp(kobj->name, "FIFO_2", Kobj_Name_FIFO_Size)) {
     sysfs_store_vlanID_Register_Set(buf, &F2.vlanID, VLAN_TAG_2,
                                           L2_SA_ADDR_HI_2);
   } else if (!strncmp(kobj->name, "FIFO_3", Kobj_Name_FIFO_Size)) {
     sysfs_store_vlanID_Register_Set(buf, &F3.vlanID, VLAN_TAG_3,
                                           L2_SA_ADDR_HI_3);
   } else if (!strncmp(kobj->name, "FIFO_4", Kobj_Name_FIFO_Size)) {
     sysfs_store_vlanID_Register_Set(buf, &F4.vlanID, VLAN_TAG_4,
                                           L2_SA_ADDR_HI_4);
   } else if (!strncmp(kobj->name, "FIFO_5", Kobj_Name_FIFO_Size)) {
     sysfs_store_vlanID_Register_Set(buf, &F5.vlanID, VLAN_TAG_5,
                                           L2_SA_ADDR_HI_5);
   } else if (!strncmp(kobj->name, "FIFO_6", Kobj_Name_FIFO_Size)) {
     sysfs_store_vlanID_Register_Set(buf, &F6.vlanID, VLAN_TAG_6,
                                           L2_SA_ADDR_HI_6);
   } else if (!strncmp(kobj->name, "FIFO_7", Kobj_Name_FIFO_Size)) {
     sysfs_store_vlanID_Register_Set(buf, &F7.vlanID, VLAN_TAG_7,
                                           L2_SA_ADDR_HI_7);
   }
  }
  return count;
}

int remove_sysfs(struct kobject *kobj_ref, struct kobj_attribute *attr) {
  CSMLOGINFO("Inside remove sysfs");
  kobject_put(kobj_ref);
  sysfs_remove_file(kernel_kobj, &attr->attr);
  return -1;
}

int sysfs_create_generic_dir_structure(struct kobject *kobj_ref) {
  CSMLOGINFO("Inside sysfs create directory structure function");

  if (sysfs_create_file(kobj_ref, &enabled_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &enabled_attr);
  }
  return -1;
}

void sysfs_remove_generic_dir_structure(struct kobject *kobj_ref) {

	sysfs_remove_file(kobj_ref, &enabled_attr.attr);
	kobject_del(kobj_ref);
	kobject_put(kobj_ref);
	kobj_ref=NULL;
}

int sysfs_create_L2headers(struct kobject *kobj_ref) {
  if (sysfs_create_file(kobj_ref, &saddr_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &saddr_attr);
  }

  else if (sysfs_create_file(kobj_ref, &daddr_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &daddr_attr);
  }

  return -1;
}

void sysfs_remove_L2headers(struct kobject *kobj_ref) {

	sysfs_remove_file(kobj_ref, &daddr_attr.attr);
	sysfs_remove_file(kobj_ref, &saddr_attr.attr);
	kobject_del(kobj_ref);
	kobject_put(kobj_ref);
	kobj_ref=NULL;
}


int sysfs_create_L3headers(struct kobject *kobj_ref) {
  if (sysfs_create_file(kobj_ref, &saddr_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &saddr_attr);
  }

  else if (sysfs_create_file(kobj_ref, &daddr_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &daddr_attr);
  }

  else if (sysfs_create_file(kobj_ref, &sport_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &sport_attr);
  }

  else if (sysfs_create_file(kobj_ref, &dport_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &dport_attr);
  }

  return -1;
}

void sysfs_remove_L3headers(struct kobject *kobj_ref) {

	sysfs_remove_file(kobj_ref, &dport_attr.attr);
	sysfs_remove_file(kobj_ref, &sport_attr.attr);
	sysfs_remove_file(kobj_ref, &daddr_attr.attr);
	sysfs_remove_file(kobj_ref, &saddr_attr.attr);
	kobject_del(kobj_ref);
	kobject_put(kobj_ref);
	kobj_ref=NULL;
}

int sysfs_create_StreamingFifo(struct kobject *kobj_ref) {
  if (sysfs_create_file(kobj_ref, &status_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &status_attr);
  }

  else if (sysfs_create_file(kobj_ref, &flush_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &flush_attr);
  }

  else if (sysfs_create_file(kobj_ref, &txcount_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &txcount_attr);
  }

  else if (sysfs_create_file(kobj_ref, &AddrRange_attr_start.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &AddrRange_attr_start);
  }

  else if (sysfs_create_file(kobj_ref, &AddrRange_attr_end.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &AddrRange_attr_end);
  }

  else if (sysfs_create_file(kobj_ref, &OverFlowInterrupt_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &OverFlowInterrupt_attr);
  }

  else if (sysfs_create_file(kobj_ref, &Threshold_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &Threshold_attr);
  }

  else if (sysfs_create_file(kobj_ref, &Timeout_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &Timeout_attr);
  }

  else if (sysfs_create_file(kobj_ref, &vlanID_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &vlanID_attr);
  }

  return -1;
}

void sysfs_remove_StreamingFifo(struct kobject *kobj_ref) {

	sysfs_remove_file(kobj_ref, &vlanID_attr.attr);
	sysfs_remove_file(kobj_ref, &Timeout_attr.attr);
	sysfs_remove_file(kobj_ref, &Threshold_attr.attr);
	sysfs_remove_file(kobj_ref, &OverFlowInterrupt_attr.attr);
	sysfs_remove_file(kobj_ref, &AddrRange_attr_end.attr);
	sysfs_remove_file(kobj_ref, &AddrRange_attr_start.attr);
	sysfs_remove_file(kobj_ref, &txcount_attr.attr);
	sysfs_remove_file(kobj_ref, &flush_attr.attr);
	sysfs_remove_file(kobj_ref, &status_attr.attr);
	kobject_del(kobj_ref);
	kobject_put(kobj_ref);
	kobj_ref=NULL;
}

int sysfs_create_PacketFifo(struct kobject *kobj_ref) {
  if (sysfs_create_file(kobj_ref, &status_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &status_attr);
  }

  else if (sysfs_create_file(kobj_ref, &flush_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &flush_attr);
  }

  else if (sysfs_create_file(kobj_ref, &txcount_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &txcount_attr);
  }

  else if (sysfs_create_file(kobj_ref, &AddrRange_attr_start.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &AddrRange_attr_start);
  }

  else if (sysfs_create_file(kobj_ref, &AddrRange_attr_end.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &AddrRange_attr_end);
  }

  else if (sysfs_create_file(kobj_ref, &OverFlowInterrupt_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &OverFlowInterrupt_attr);
  }

  else if (sysfs_create_file(kobj_ref, &vlanID_attr.attr)) {
    CSMLOGINFO("Unable to create the sysfs file...\n");
    remove_sysfs(kobj_ref, &vlanID_attr);
  }

  return -1;
}

void sysfs_remove_PacketFifo(struct kobject *kobj_ref) {

	sysfs_remove_file(kobj_ref, &vlanID_attr.attr);
	sysfs_remove_file(kobj_ref, &OverFlowInterrupt_attr.attr);
	sysfs_remove_file(kobj_ref, &AddrRange_attr_end.attr);
	sysfs_remove_file(kobj_ref, &AddrRange_attr_start.attr);
	sysfs_remove_file(kobj_ref, &txcount_attr.attr);
	sysfs_remove_file(kobj_ref, &flush_attr.attr);
	sysfs_remove_file(kobj_ref, &status_attr.attr);
	kobject_del(kobj_ref);
	kobject_put(kobj_ref);
	kobj_ref=NULL;
}

unsigned int is_delim(char c, char *delim) {
  while (*delim != '\0') {
    if (c == *delim)
      return 1;
    delim++;
  }
  return 0;
}
char *mtip_sysfs_strtok(char *srcString, char *delim) {
  static char *backup_string; // start of the next search
  char *ret;

  if (!srcString) {
    srcString = backup_string;
  }
  if (!srcString) {
    // user is bad user
    return NULL;
  }
  // handle beginning of the string containing delims
  while (1) {
    if (is_delim(*srcString, delim)) {
      srcString++;
      continue;
    }
    if (*srcString == '\0') {
      // we've reached the end of the string
      return NULL;
    }
    break;
  }
  ret = srcString;
  while (1) {
    if (*srcString == '\0') {
      /*end of the input string and
      next exec will return NULL*/
      backup_string = srcString;
      return ret;
    }
    if (is_delim(*srcString, delim)) {
      *srcString = '\0';
      backup_string = srcString + 1;
      return ret;
    }
    srcString++;
  }
}

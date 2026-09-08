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
#include <linux/panic_notifier.h>
#include <linux/debugfs.h>

MODULE_LICENSE("GPL v2");

#include <linux/module.h>
#include <linux/init.h>
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
#include <linux/of.h>

#include "ecpri_dma_eth.h"
#include "mtip.h"
#include "mtip_device.h"
#include "mtip_dma.h"
#include "mtip_platform.h"
#include "mtip_workq.h"
#include "mtip_hashmap.h"
#include "mtip_client.h"
#include "mtip_phy.h"
#include "mtip_dut.h"
#include "mtip_debug_eth.h"
#include "mtip_macstats.h"
#include "mtip_ethtool.h"
#include "mtip_notifr.h"
#include "mtip_sysfs.h"
#include "mtip_mac.h"
#include "mtip_client.h"
#include "ldmm_genl.h"
#include "eth_phy_iface.h"
#include "ldmm_notifr.h"


/* Global variables of the driver */
struct mtip_platform_driver_priv* platform_driver_priv = NULL;
struct mtip_delayed_work_q_params delayed_wq_notifr_param_v;
struct mtip_delayed_work_q_params *delayed_wq_notifr_param = &delayed_wq_notifr_param_v;

/* Module parameters */
int mtip_tx_delay[MTIP_MAX_LINKS];
int mtip_tx_delay_argc = 0;
module_param_array(mtip_tx_delay, int, &mtip_tx_delay_argc, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(mtip_tx_delay, "Transmit delay array for RGMII IO Macro");

int mtip_rx_delay[MTIP_MAX_LINKS];
int mtip_rx_delay_argc = 0;
module_param_array(mtip_rx_delay, int, &mtip_rx_delay_argc, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(mtip_rx_delay, "Receive delay array for RGMII IO Macro");

int mtip_loopback_mode = MTIP_MODE_DEFAULT;
module_param(mtip_loopback_mode, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(mtip_loopback_mode, "Loopback mode of the driver");

int mtip_c2c2_loopback_mode = MTIP_MODE_DEFAULT;
module_param(mtip_c2c2_loopback_mode, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(mtip_c2c2_loopback_mode, "Loopback mode of C2C2 interface");

bool mtip_loopback_swap_addr = true;
module_param(mtip_loopback_swap_addr, bool, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(mtip_loopback_swap_addr, "Swap SA/DA in loopback mode operation");

bool mtip_loopback_enable_arp = false;
module_param(mtip_loopback_enable_arp, bool, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(mtip_loopback_enable_arp, "Enable ARP in loopback mode");

int mtip_rumi_platform = MTIP_PLATFORM_SOC;
module_param(mtip_rumi_platform, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(mtip_rumi_platform, "Set Platform mode as SOC/RUMI");

int mtip_dma_max_rx_buff_size = MTIP_DMA_RX_BUFF_SIZE;
module_param(mtip_dma_max_rx_buff_size, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(mtip_dma_max_rx_buff_size, "SET mtip_dma_rx buff size");

/* Module parameter for enabling Tx napi poll feature for
 * Tx completion packets received from DMA.
 * If this value is false, then polling of Tx of completion
 * packets from DMA will work in regular IRQ mode.
 */
bool enable_tx_comp_poll = true;
module_param(enable_tx_comp_poll, bool, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(enable_tx_comp_poll, "Enable TX Completion Poll");

extern struct eth_phy_iface_ops qcom_aw_phy_driver_iface_ops;

struct blocking_notifier_head lassen_qxdm_timer_update_notifr;

EXPORT_SYMBOL_GPL(lassen_qxdm_timer_update_notifr);

uint32_t ber_sim_status[12]={0};
int logging_timer_value = 10;

/* Root object for sysfs directory */
struct kobject *mtip_kobj_root;

/* File attribute for link polling timer sysfs node */
struct kobj_attribute mtip_link_polling_timer_attr =
                         __ATTR(mtip_link_poll_timer_msec, 0660,
                                mtip_show_link_polling_timer,
                                mtip_store_link_polling_timer);
extern int mtip_link_polling_timer;

#ifdef FEATURE_MTIP_TEST_DEBUG_FS

struct dentry *mtip_dobj;

int mtip_attr_val;

#ifndef MIN
#define MIN(a,b) ((a < b) ? a : b)
#endif

char help_menu[] = {
"Help Menu:\n\
           Interface index (0-11), Enable(1)/Disable(0) BER Simulation\n\
           For enable/disable BER Simulation on link index n, use following command:\n\
           echo n,1 > /sys/kernel/debug/mtip_test/mtip_sim_ber\n\
           echo n,0 > /sys/kernel/debug/mtip_test/mtip_sim_ber\n"
};

ssize_t mtip_set_attr(struct file *file, const char __user *buf,
                             size_t count, loff_t *ppos) {
 
  char *token;
  char token_string[100];
  int ber_sim = 0;
  
  memset(token_string, 0, sizeof(token_string));

  if (copy_from_user(&token_string, buf, MIN(sizeof(token_string), count)))
  {
    CSMLOGERR("Copy from user failed\n");
    return -EFAULT;
  }

  token = mtip_sysfs_strtok(token_string, ",");
  if(token!=NULL)
    sscanf(token, "%d", &mtip_attr_val);
  else
  {
    CSMLOGERR("Invalid Input\n");
    return -EFAULT;
  }

  if((mtip_attr_val < 0) || (mtip_attr_val > 11))
  {
    CSMLOGERR("Invalid interface index\n");
    return -EFAULT;
  }
  else
  {
    token = mtip_sysfs_strtok(NULL, ",");
    if(token!=NULL)
      sscanf(token, "%d", &ber_sim);
    else
    {
      CSMLOGERR("Invalid Input\n");
      return -EFAULT;
    }

    if((ber_sim != 0) && (ber_sim != 1))
    {
      CSMLOGERR("Invalid ber_sim value\n");
      return -EFAULT;
    }
    ber_sim_status[mtip_attr_val] = ber_sim;

   }

  return count;

}

ssize_t mtip_get_attr(struct file *file, char __user *buf,
                             size_t count, loff_t *ppos) {
  char ber_sim_str[700]={0};
  uint32_t ret_val = 0, i = 0;

  scnprintf(ber_sim_str + strlen(ber_sim_str), sizeof(help_menu), help_menu);
  scnprintf(ber_sim_str + strlen(ber_sim_str), 40, "\nBer Simulation Status:\n");

  for(i=0;i<12;i++)
  {
    scnprintf(ber_sim_str + strlen(ber_sim_str), 30, "    ber_status[%d] = %d\n",i,ber_sim_status[i]);
  }

  ret_val=simple_read_from_buffer(buf, count, ppos, ber_sim_str, 700);
  return ret_val;
}


ssize_t qxdm_logger_set_interval(struct file *file, const char __user *buf,
                             size_t count, loff_t *ppos) {
  char temp_string[3];
  int temp_val;

  if (copy_from_user(&temp_string, buf, MIN(sizeof(temp_string), count)))
  {
    CSMLOGERR("Copy from user failed\n");
    return -EFAULT;
  }
  sscanf(temp_string, "%d", &temp_val);
  
  //timer value could only be set for range 1 to 100
  if(temp_val < 1 || temp_val > 100)
    return -EFAULT;
  logging_timer_value = temp_val;

  blocking_notifier_call_chain(&lassen_qxdm_timer_update_notifr, logging_timer_value, NULL);

  return count;
}

ssize_t qxdm_logger_get_interval(struct file *file, char __user *buf,
                             size_t count, loff_t *ppos) {
  char temp_string[3];
  ssize_t ret_val;

  scnprintf(temp_string, sizeof(temp_string), "%d", logging_timer_value);
  ret_val = simple_read_from_buffer(buf, count, ppos, temp_string, sizeof(temp_string));

  return ret_val;
}


static const struct file_operations mtip_debug_fs_ops = {
  .write = mtip_set_attr,
  .read = mtip_get_attr,
};


static const struct file_operations qxdm_logging_fs_ops = {
  .write = qxdm_logger_set_interval,
  .read = qxdm_logger_get_interval,
};


void mtip_setup_debugfs(void) {

  /* creating the directory structure in /sys/kernel/debug */
  mtip_dobj = debugfs_create_dir("mtip_test", NULL);

  debugfs_create_file("mtip_sim_ber", 0644, mtip_dobj, 0, &mtip_debug_fs_ops);

  debugfs_create_file("eth_qxdm_logging_interval", 0644, mtip_dobj, 0, &qxdm_logging_fs_ops);

  return;
}

void mtip_del_debugfs(void) {

  /* deleting the directory structure in /sys/kernel/debug */
  debugfs_remove_recursive(mtip_dobj);
  return;
}

#endif /* FEATURE_MTIP_TEST_DEBUG_FS */

ssize_t mtip_show_link_polling_timer(
                struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
  return snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n", mtip_link_polling_timer);
}

ssize_t mtip_store_link_polling_timer(
                              struct kobject *kobj, struct kobj_attribute *attr,
                              const char *buf, size_t count) {
  sscanf(buf, "%d", &mtip_link_polling_timer);
  return count;
}

void mtip_setup_sysfs(void) {

  /* Creating the root directory structure in /sys/kernel */
  mtip_kobj_root = kobject_create_and_add("mtip", kernel_kobj);

  /* Creating file for link polling timer */
  if(sysfs_create_file(mtip_kobj_root, &mtip_link_polling_timer_attr.attr))
  {
    kobject_put(mtip_kobj_root);
    sysfs_remove_file(kernel_kobj, &mtip_link_polling_timer_attr.attr);
  }

  return;
}

void mtip_del_sysfs(void) {

  sysfs_remove_file(mtip_kobj_root, &mtip_link_polling_timer_attr.attr);
  kobject_del(mtip_kobj_root);
  mtip_kobj_root=NULL;

  return;
}

int mtip_lookup_link_index_by_name(char *name, u32 *link_index) {
   int i;
   struct net_device *dev;
   int ret = -1;
   unsigned long flags;
   spinlock_t *lock = &(platform_driver_priv->driver_lock);

   CSMLOGDBG("mtip_lookup_link_index_by_name called for %s\n", name);

   spin_lock_irqsave(lock, flags);

   // go through all the allocated netdevs and find link_index
   for (i = 0; i < MTIP_MAX_LINKS; ++i) 
   {
       if (platform_driver_priv->mtip_links[i] != NULL)
       {
           if (platform_driver_priv->mtip_links[i]->dev != NULL) {

             dev = (platform_driver_priv->mtip_links[i]->dev);
             if (strncmp(name, dev->name, IFNAMSIZ) == 0) {
                *link_index = i;
                ret = 0;
                break;
             }
       }
      }
   }

   spin_unlock_irqrestore(lock, flags);
   return ret;
}

int mtip_lookup_link_index_by_handle(ecpri_dma_eth_conn_hdl_t hdl, u32 *link_index) {

   CSMLOGDBG("mtip_lookup_link_index_by_handle called for %d\n", hdl);

   // use the hashmap to lookup index
   return mtip_hashmap_find(hdl, link_index);
}

/*
 * lookup the link index by the port_type and link number 
 * For example link_index 10 maps to port_type 2 and real_link_number 2 
 */
int mtip_lookup_link_index_by_port_type_and_real_link(u32* link_index, u32 port_type, u32 real_link_number)
{
	int ret = 0;
    u32 tmp_port_type;

	if (port_type <= MTIP_PORT_TYPE_FH_2)
	{
		// front haul port
		if (real_link_number >= 4)
		{
            CSMLOGERR("invalid link number: %d for port %d:\n", real_link_number, port_type);
			ret = -1;
            goto out;
		}
        else
        {
            *link_index = 4*port_type + real_link_number;
        }
	}
	else if (port_type < MTIP_PORT_TYPE_MAX)
	{
        if (real_link_number >= 2)
        {
//            CSMLOGDBG("invalid link number: %d for port %d:\n", real_link_number, port_type);
            ret = -1;
            goto out;
        }
        else
        {
            *link_index = 12 + 2*(port_type - MTIP_PORT_TYPE_L2) + real_link_number;
        }
	}
	else
	{
        CSMLOGERR("invalid port_number: %d\n", port_type);
		ret = -1;
        goto out;
	}

    // cross check that link_index maps to the port
    if (mtip_lookup_port_type_by_link_index(*link_index, &tmp_port_type) < 0)
    {
        ret = -1;
        goto out;
    }
    else if (tmp_port_type != port_type) 
    {
        ret = -1;
        goto out;
    }

out:
	return ret;
}

/*
 * lookup the port_type from the link index 
 */
int mtip_lookup_port_type_by_link_index(u32 link_index, u32* port_type)
{
    int ret = 0;

    switch (link_index) 
    {
    case 0:
    case 1:
    case 2:
    case 3:
        *port_type = MTIP_PORT_TYPE_FH_0;
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        *port_type = MTIP_PORT_TYPE_FH_1;
        break;
    case 8:
    case 9:
    case 10:
    case 11:
        *port_type = MTIP_PORT_TYPE_FH_2;
        break;
    case 12:
    case 13:
        *port_type = MTIP_PORT_TYPE_L2;
        break;
    case 14:
    case 15:
        *port_type = MTIP_PORT_TYPE_DEBUG;
        break;
    default:
//        CSMLOGDBG("invalid link_index: %d\n", link_index);
        ret = -1;
        break;
    }
    return ret;
}

/*
 * lookup the real link number from the link index 
 * for example, link_index 7 will have the real link number 3 
 */
int mtip_lookup_real_link_number_by_link_index(u32 link_index, u32* real_link_number)
{
	int ret = 0;
	switch(link_index)
	{
	case 0:
		*real_link_number = 0;
		break;
	case 1:
		*real_link_number = 1;
		break;
	case 2:
		*real_link_number = 2;
		break;
	case 3:
		*real_link_number = 3;
		break;
	case 4:
		*real_link_number = 0;
		break;
	case 5:
		*real_link_number = 1;
		break;
	case 6:
		*real_link_number = 2;
		break;
	case 7:
		*real_link_number = 3;
		break;
	case 8:
		*real_link_number = 0;
		break;
	case 9:
		*real_link_number = 1;
		break;
	case 10:
		*real_link_number = 2;
		break;
	case 11:
		*real_link_number = 3;
		break;
	case 12:
		*real_link_number = 0;
		break;
	case 13:
		*real_link_number = 1;
		break;
	case 14:
		*real_link_number = 0;
		break;
	case 15:
		*real_link_number = 1;
		break;
	default:
        CSMLOGERR("invalid link_index: %d\n", link_index);
	    ret = -1;
		break;
	}
	return ret;
}

/*
 * mtip_lookup_link_index_by_device 
 *  find the link index within a port and the link_device_index (0 thru num_link_phandles - 1) 
 */
int mtip_lookup_link_index_by_device(u32* link_index, u32 port_type, u32 link_device_index)
{
    if (port_type >= MTIP_MAX_PORTS) 
    {
        CSMLOGERR("port_type %d out of bounds %d", port_type, MTIP_MAX_PORTS);
        return -1;
    }

    if (link_device_index >= platform_driver_priv->devices.port_devices[port_type].num_link_phandles)
    {
        CSMLOGERR("link_device_index %d out of bounds %d", link_device_index, platform_driver_priv->devices.port_devices[port_type].num_link_phandles);
        return -1;
    }

    *link_index = platform_driver_priv->devices.port_devices[port_type].link_devices[link_device_index]->link_index;
    return 0;
}

/*
 * mtip_lookup_device_by_link_index 
 *  find the port_type and the link_device_index (between 0 and num_phandles - 1) 
 */
int mtip_lookup_device_by_link_index(u32 link_index, u32* port_type, u32* link_device_index)
{
    int i;
    u32 link_phandle;

    if (mtip_lookup_port_type_by_link_index(link_index, port_type) < 0) 
    {
        CSMLOGDBG("Could not find port_type of link_index: %d", link_index);
        return -1;
    }

    link_phandle = platform_driver_priv->devices.link_devices[link_index].link_phandle;

    for (i = 0; i < platform_driver_priv->devices.port_devices[*port_type].num_link_phandles; ++i) 
    {
        if (link_phandle == platform_driver_priv->devices.port_devices[*port_type].link_phandles[i]) 
        {
            *link_device_index = i;
        return 0;
    }
    }
    return -1;
}

/*
 * lookup the lane index by the port_type and lane number 
 * For example lane_index 10 maps to port_type 2 and real_lane_number 2 
 */
int mtip_lookup_lane_index_by_port_type_and_real_lane(u32 *lane_index, u32 port_type, u32 real_lane_number)
{
	int ret = 0;
	if (port_type <= MTIP_PORT_TYPE_FH_2)
	{
		// front haul port
		if (real_lane_number >= 4)
		{
            CSMLOGERR("invalid lane number: %d for port %d:\n", real_lane_number, port_type);
			ret = -1;
		}
        else
        {
         *lane_index = 4 * port_type + real_lane_number;
        }
	}
	else if (port_type == MTIP_PORT_TYPE_L2)
	{
        if (real_lane_number >= 4)
        {
            CSMLOGERR("invalid lane number: %d for port %d:\n", real_lane_number, port_type);
            ret = -1;
        }
        else
        {
            *lane_index = 12 + real_lane_number;
        }
	}
    else if (port_type == MTIP_PORT_TYPE_DEBUG)
    {
        // only real_lane_number 2 and 3 are valid for DEBUGETH
        if ((real_lane_number == 2) || (real_lane_number == 3))
        {
            *lane_index = 16 + real_lane_number;
        }
	else
	{
         CSMLOGDBG("invalid lane number: %d for port %d:\n", real_lane_number, port_type);
            ret = -1;
        }
    }
	else
	{
        CSMLOGERR("invalid port_number: %d\n", port_type);
		ret = -1;
	}
	return ret;
}

/*
 * lookup the port_type from the lane index 
 */
int mtip_lookup_port_type_by_lane_index(u32 lane_index, u32* port_type)
{
	int ret = 0;

    switch (lane_index) 
	{
	case 0:
	case 1:
	case 2:
	case 3:
        *port_type = MTIP_PORT_TYPE_FH_0;
		break;
	case 4:
	case 5:
	case 6:
	case 7:
        *port_type = MTIP_PORT_TYPE_FH_1;
		break;
	case 8:
	case 9:
	case 10:
	case 11:
        *port_type = MTIP_PORT_TYPE_FH_2;
		break;
	case 12:
	case 13:
	case 14:
    case 15:
        *port_type = MTIP_PORT_TYPE_L2;
		break;
    case 18:
    case 19:
        *port_type = MTIP_PORT_TYPE_DEBUG;
		break;
    case 16:
    case 17:
	default:
        CSMLOGERR("invalid lane_index: %d\n", lane_index);
	    ret = -1;
		break;
	}
	return ret;
}

/*
 * lookup the real lane number from the lane index 
 * for example, lane_index 7 will have the real lane number 3 
 */
int mtip_lookup_real_lane_number_by_lane_index(u32 lane_index, u32* real_lane_number)
{
    int ret = 0;

    if (lane_index >= MTIP_MAX_LANES) 
    {
        CSMLOGERR("invalid lane_index %d", lane_index);
        ret = -1;
    }
    else 
    {
        *real_lane_number = lane_index%4;
    }
    return ret;
}

/*
 * mtip_lookup_lane_index_by_device 
 *  find the lane index within a port and the lane_device_index (0 thru num_lane_phandles - 1) 
 */
int mtip_lookup_lane_index_by_device(u32* lane_index, u32 port_type, u32 lane_device_index)
{
    if (port_type >= MTIP_MAX_PORTS) 
    {
        CSMLOGERR("port_type %d out of bounds %d", port_type, MTIP_MAX_PORTS);
        return -1;
    }

    if (lane_device_index >= platform_driver_priv->devices.port_devices[port_type].num_lane_phandles)
    {
        CSMLOGERR("lane_device_index %d out of bounds %d", lane_device_index, platform_driver_priv->devices.port_devices[port_type].num_lane_phandles);
        return -1;
    }

    *lane_index = platform_driver_priv->devices.port_devices[port_type].lane_devices[lane_device_index]->lane_index;
    return 0;
}

/*
 * mtip_lookup_device_by_lane_index 
 *  find the port_type and the lane_device_index (between 0 and num_phandles - 1) 
 */
int mtip_lookup_device_by_lane_index(u32 lane_index, u32* port_type, u32* lane_device_index)
{
    int i;
    u32 lane_phandle;

    CSMLOGERR("lookup_device by lane_index called");

    if (mtip_lookup_port_type_by_lane_index(lane_index, port_type) < 0) 
    {
        CSMLOGERR("Could not find port_type of lane_index: %d", lane_index);
        return -1;
    }

    lane_phandle = platform_driver_priv->devices.lane_devices[lane_index].lane_phandle;

    for (i = 0; i < platform_driver_priv->devices.port_devices[*port_type].num_lane_phandles; ++i) 
    {
        if (lane_phandle == platform_driver_priv->devices.port_devices[*port_type].lane_phandles[i]) 
        {
            *lane_device_index = i;
            return 0;
        }
    }
    return -1;
}

bool mtip_lookup_if_any_other_link_active_for_port(u32 port_type, u32 link_index)
{
    u32 i = 0;
    u32 temp_link_index = 0;

    for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_link_phandles; ++i)
    {
       temp_link_index = platform_driver_priv->devices.port_devices[port_type].link_devices[i]->link_index;

       if (platform_driver_priv->mtip_links[temp_link_index] != NULL &&
           temp_link_index != link_index)
       {
          if ((platform_driver_priv->mtip_links[temp_link_index]->state != MTIP_LINK_STATE_INIT) &&
              (platform_driver_priv->mtip_links[temp_link_index]->state != MTIP_LINK_STATE_CLOSE))
          {
             return true;
          }
       }
    }

    return false;
}

int mtip_lookup_link_index_by_lane_index(u32 *link_index, u32 lane_index)
{
    int i;
    int j;

    for (i = 0; i < MTIP_MAX_LINKS; ++i) 
    {
        if (platform_driver_priv->mtip_links[i] != NULL)
        {
            for(j = 0; j < platform_driver_priv->mtip_links[i]->num_assigned_lanes; j++)
            {
                if(platform_driver_priv->mtip_links[i]->assigned_lane_indices[j] == lane_index)
                {
                    *link_index = i;
                    return 0;
                }
            }
        }
    }

    return -1;
}



static const struct of_device_id mtip_mac_link_match[] = {
    { .compatible = "mtip-mac-link", },
    { }
};

MODULE_DEVICE_TABLE(of, mtip_mac_link_match);

static struct platform_driver ethernet_mac_link_driver = { 
	.probe  = mtip_link_probe,
	.remove = mtip_link_remove,
	.driver = {
		.name = "MTIP_MAC_LINK",
		.of_match_table = of_match_ptr(mtip_mac_link_match),
	},
};

static const struct of_device_id mtip_mac_lane_match[] = {
    { .compatible = "mtip-mac-lane", },
    { }
};

MODULE_DEVICE_TABLE(of, mtip_mac_lane_match);

static struct platform_driver ethernet_mac_lane_driver = { 
	.probe  = mtip_lane_probe,
	.remove = mtip_lane_remove,
	.driver = {
		.name = "MTIP_MAC_LANE",
		.of_match_table = of_match_ptr(mtip_mac_lane_match),
	},
};

static const struct of_device_id mtip_mac_port_match[] = {
    { .compatible = "mtip-mac-port", },
    { }
};

MODULE_DEVICE_TABLE(of, mtip_mac_port_match);

static struct platform_driver ethernet_mac_port_driver = { 
	.probe  = mtip_port_probe,
	.remove = mtip_port_remove,
	.driver = {
		.name = "MTIP_MAC_PORT",
		.of_match_table = of_match_ptr(mtip_mac_port_match),
	},
};

static const struct of_device_id mtip_mac_match[] = {
    { .compatible = "mtip-mac", },
    { }
};

MODULE_DEVICE_TABLE(of, mtip_mac_match);

static struct platform_driver ethernet_mac_platform_driver = { 
	.probe  = mtip_platform_probe,
    .remove = mtip_platform_remove,
	.driver = {
		.name = "MTIP_MAC",
		.of_match_table = of_match_ptr(mtip_mac_match),
	},
};

static struct ecpri_dma_eth_register_params mtip_dma_register_params;

/** 
 * mtip_register_platform_driver 
 *  - register as a platform driver
*/ 
int mtip_register_platform_driver(void)
{
   int ret = 0;

   CSMLOGDBG("mtip_register_platform_driver called\n");

   // register for the platform driver
   platform_driver_priv->perr = platform_driver_register(&ethernet_mac_platform_driver);

   // HANDLE THE ERROR
   if (platform_driver_priv->perr < 0)
   {
      ret = platform_driver_priv->perr;

      CSMLOGERR("platform_driver_register with error: %d\n", ret);
      return ret;
   }

   // register for port devices
   // register for the port driver
   platform_driver_priv->perr = platform_driver_register(&ethernet_mac_port_driver);

   // HANDLE THE ERROR
   if (platform_driver_priv->perr < 0)
   {
      ret = platform_driver_priv->perr;

      CSMLOGERR("platform_driver_register for port with error: %d\n", ret);
      return -ENODEV;
   }

   // register for link devices
   // register for the link driver
   platform_driver_priv->perr = platform_driver_register(&ethernet_mac_link_driver);

   // HANDLE THE ERROR
   if (platform_driver_priv->perr < 0)
   {
      ret = platform_driver_priv->perr;

      CSMLOGERR("platform_driver_register for link with error: %d\n", ret);
      return ret;
   }

   // register for lane devices
   // register for the lane driver
   platform_driver_priv->perr = platform_driver_register(&ethernet_mac_lane_driver);

   // HANDLE THE ERROR
   if (platform_driver_priv->perr < 0)
   {
      ret = platform_driver_priv->perr;

      CSMLOGERR("platform_driver_register for lane with error: %d\n", ret);
      return ret;
   }
   mtip_fault_notifr_init();
   INIT_DELAYED_WORK(&delayed_wq_notifr_param->wq_item, mtip_fault_notifr_status);
   mtip_workq_queue_delayed_work(delayed_wq_notifr_param, MTIP_NOTIFY_TIMER);
   return ret;
}

static void mtip_save_eth_stats(void)
{
    int i,j;
    char ** ethtool_stat_strings = NULL;
    u64 temp_val[DEBUG_ETHTOOL_STAT_STRINGS_LEN] = {0};
    ethtool_stat_strings = get_mtip_debug_ethtool_stat_strings();

    if(!platform_driver_priv)
      return;

    for(i = 0; i < MTIP_MAX_LINKS; i++)
    {
	if(!platform_driver_priv->mtip_links[i])
	    continue;

	if(!platform_driver_priv->mtip_links[i]->dev)
	    continue;

        //getting stats of fh ports
        if(i < 12)
        {  
            mtip_macstats_get_stats(platform_driver_priv->mtip_links[i]->dev, (u64 *)temp_val);
        }
        //getting stats of debug ports
        else if(i == 15)
        {
            mtip_debug_eth_macstats_get_stats(platform_driver_priv->mtip_links[i]->dev, (u64 *)temp_val);
        }
        else
        {
            continue;
        }

        for(j = 0; j < DEBUG_ETHTOOL_STAT_STRINGS_LEN; j++)
        {
            //breaking the loop for fh ports when loop exceeds stats string length
            if(i < 12 && j >= ETHTOOL_STAT_STRINGS_LEN)
            {
                break;
            }

            memcpy(platform_driver_priv->mtip_links[i]->stats[j].stats_name, ethtool_stat_strings[j], strlen(ethtool_stat_strings[j]));
            platform_driver_priv->mtip_links[i]->stats[j].stats_value = temp_val[j];
	    if (platform_driver_priv->mtip_links[i]->state == MTIP_LINK_STATE_UP)
	    {
                CSMLOGERR("link : %s, %s : %lu \n",platform_driver_priv->devices.link_devices[i].link_name,platform_driver_priv->mtip_links[i]->stats[j].stats_name,platform_driver_priv->mtip_links[i]->stats[j].stats_value);
	    }

        }
    }
}

static int mtip_panic_notifier(struct notifier_block *this, unsigned long event, void *ptr)
{
    mtip_save_eth_stats();
    return NOTIFY_DONE;
}



static struct notifier_block mtip_panic_blk = {
	.notifier_call = mtip_panic_notifier,
};



//mapping 4 bits of number corresponding to the 4 lanes
int map_lanes_to_link(bool lanes_enabled[])
{
  int lane_index, mapped_value = 0;
  for(lane_index = 0; lane_index < PHY_LANE_MAX; lane_index++)
  {
    if(lanes_enabled[lane_index] == true)
    {
      mapped_value |= (1<<lane_index);
    }
  }
  return mapped_value;
}


link_info get_link_info(int port_type, int real_link_number, int port_config)
{

  link_info links = {0};
  bool lanes_enabled[PHY_LANE_MAX];
  int link_index;

  if(mtip_lookup_link_index_by_port_type_and_real_link(&link_index, port_type, real_link_number) == -1)
    goto out;

  links.link_name = link_index;

  if(platform_driver_priv == NULL || platform_driver_priv->mtip_links[link_index] == NULL)
    goto out;

  if(platform_driver_priv->mtip_links[link_index]->state != MTIP_LINK_STATE_UP)
    goto out;

  links.link_status = platform_driver_priv->mtip_links[link_index]->state;

  mtip_phy_get_lanes_of_link(link_index, lanes_enabled);
  links.lanes_mapped = map_lanes_to_link(lanes_enabled);

  switch (port_config)
  {
    case MTIP_PORT_CONFIG_1x100GBASE_R:
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC_LL:
    case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC:
    {
      links.link_speed = MTIP_LINK_SPEED_100G;
      links.lanes_speed = PHY_LANE_SPEED_100G;
    }
    break;
    case MTIP_PORT_CONFIG_1x100GBASE_R2:
    case MTIP_PORT_CONFIG_1x100GBASE_R2_RSFEC:
    {
      links.link_speed = MTIP_LINK_SPEED_100G;
      links.lanes_speed = PHY_LANE_SPEED_50G;
    }
    break;
    case MTIP_PORT_CONFIG_1x100GBASE_R4:
    case MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC:
    {
      links.link_speed = MTIP_LINK_SPEED_100G;
      links.lanes_speed = PHY_LANE_SPEED_25G;
    }
    break;
    case MTIP_PORT_CONFIG_1x50GBASE_R:
    case MTIP_PORT_CONFIG_1x50GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_2x50GBASE_R:
    case MTIP_PORT_CONFIG_2x50GBASE_R_RSFEC:
    {
      links.link_speed = MTIP_LINK_SPEED_50G;
      links.lanes_speed = PHY_LANE_SPEED_50G;
    }
    break;
    case MTIP_PORT_CONFIG_1x50GBASE_R2:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_RSFEC:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI:
    case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI_FEC:
    case MTIP_PORT_CONFIG_2x50GBASE_R2:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_FEC:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI:
    case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI_FEC:
    {
      links.link_speed = MTIP_LINK_SPEED_50G;
      links.lanes_speed = PHY_LANE_SPEED_25G;
    }
    break;
    case MTIP_PORT_CONFIG_1x40GBASE_R4:
    case MTIP_PORT_CONFIG_1x40GBASE_R4_FEC:
    {
      links.link_speed = MTIP_LINK_SPEED_40G;
      links.lanes_speed = PHY_LANE_SPEED_10G;
    }
    break;
    case MTIP_PORT_CONFIG_1x25GBASE_R:
    case MTIP_PORT_CONFIG_1x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC:
    case MTIP_PORT_CONFIG_4x25GBASE_R:
    case MTIP_PORT_CONFIG_4x25GBASE_R_FEC:
    case MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC:
    {
      links.link_speed = MTIP_LINK_SPEED_25G;
      links.lanes_speed = PHY_LANE_SPEED_25G;
    }
    break;
    case MTIP_PORT_CONFIG_1x10GBASE_R:
    case MTIP_PORT_CONFIG_1x10GBASE_R_FEC:
    case MTIP_PORT_CONFIG_4x10GBASE_R:
    case MTIP_PORT_CONFIG_4x10GBASE_R_FEC:
    {
      links.link_speed = MTIP_LINK_SPEED_10G;
      links.lanes_speed = PHY_LANE_SPEED_10G;
    }
    break;
    default:
    {
      CSMLOGERR("Unknown port config %d", port_config);
    }
    break;
  }

  out:
    return links;
}

config_packet_info mtip_get_config_info(void)
{
  int port_type, real_link_number;
  config_packet_info config = {0};
  int master_link = 0, master_lane = 0, link_index, lane_index;

  for(port_type = 0; port_type < MAX_PORTS; port_type++)
  {
    config.ports[port_type].port_type = port_type;
    
    if(platform_driver_priv == NULL || platform_driver_priv->mtip_ports[port_type] == NULL)
    {
        config.ports[port_type].port_enabled = 0;
        config.ports[port_type].sfp_port_type = QXDM_LOGGING_VAR_NA;
        config.ports[port_type].port_config = QXDM_LOGGING_VAR_NA;
        config.ports[port_type].active_fec = QXDM_LOGGING_VAR_NA;
        config.ports[port_type].link_length_range = QXDM_LOGGING_VAR_NA;
        config.ports[port_type].phy_eq_mode = QXDM_LOGGING_VAR_NA;
        config.ports[port_type].active_links = 0;
        continue;
    }
      
    config.ports[port_type].port_enabled = (int)platform_driver_priv->devices.port_devices[port_type].port_device_valid;
    config.ports[port_type].sfp_port_type = platform_driver_priv->mtip_ports[port_type]->sfp_port_type;
    config.ports[port_type].port_config = platform_driver_priv->mtip_ports[port_type]->port_config;
    
    if(port_type == MTIP_PORT_TYPE_DEBUG)
    {
      master_link = 1;
      master_lane = 2;
    }
    mtip_lookup_link_index_by_port_type_and_real_link(&link_index, port_type, master_link);
    mtip_lookup_lane_index_by_port_type_and_real_lane(&lane_index, port_type, master_lane);

    if(platform_driver_priv->mtip_links[link_index] == NULL || platform_driver_priv->mtip_lanes[lane_index] == NULL)
      goto out;

    //setting master link/lane info to port
    config.ports[port_type].active_fec = platform_driver_priv->mtip_links[link_index]->active_fec;
    config.ports[port_type].link_length_range = platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info.trx_link_length_range;

    config.ports[port_type].phy_eq_mode = qcom_aw_phy_driver_iface_ops.eth_phy_iface_get_phy_phy_eq_mode(port_type);

    for(real_link_number = 0; real_link_number < MAX_LINKS_PER_PORT; real_link_number++)
    {
      config.ports[port_type].links[real_link_number] = get_link_info(port_type, real_link_number, config.ports[port_type].port_config);
      if(config.ports[port_type].links[real_link_number].link_status == MTIP_LINK_STATE_UP)
        config.ports[port_type].active_links++;
    }
  }
  out:
    return config;
}

stats_info mtip_get_stats_info(int link_index)
{
  int j = 0;
  u64 data[DEBUG_ETHTOOL_STAT_STRINGS_LEN] = {0};
  stats_info stats = {0};

  memset(data, 0, DEBUG_ETHTOOL_STAT_STRINGS_LEN*sizeof(u64));

  if(link_index < 12)
  {
    mtip_macstats_get_stats(platform_driver_priv->mtip_links[link_index]->dev, data);
  }
  else if(link_index == MTIP_DEBUG_ETH_LINK_INDEX)
  {
    mtip_debug_eth_macstats_get_stats(platform_driver_priv->mtip_links[link_index]->dev, data);
  }
  else
  {
    //TBD
    return stats;
  }
  stats.link_name = link_index;
  stats.EtherStatsOctets = data[j++];
  stats.OctetsReceivedOK = data[j++];
  stats.VLANReceivedOK = data[j++];
  stats.InErrors = data[j++];
  stats.InUCastPkts = data[j++];
  stats.InMCastPkts = data[j++];
  stats.InBCastPkts = data[j++];
  stats.EtherStatsDrops = data[j++];
  stats.EtherStatsPkts = data[j++];
  stats.OctetsTransmittedOK = data[j++];
  stats.VLANTransmittedOK = data[j++];
  stats.OutErrors = data[j++];
  stats.OutUCastPkts = data[j++];
  stats.OutMCastPkts = data[j++];
  stats.OutBCastPkts = data[j++];
  if(link_index == MTIP_DEBUG_ETH_LINK_INDEX)
  {
    stats.FIFO_0_TX_Count = data[j++];
    stats.FIFO_1_TX_Count = data[j++];
    stats.FIFO_2_TX_Count = data[j++];
    stats.FIFO_3_TX_Count = data[j++];
    stats.FIFO_4_TX_Count = data[j++];
    stats.FIFO_5_TX_Count = data[j++];
    stats.FIFO_6_TX_Count = data[j++];
    stats.FIFO_7_TX_Count = data[j++];
  }
  stats.Software_TX_Errors = platform_driver_priv->mtip_links[link_index]->net_stats.tx_errors;
  stats.Software_RX_Errors = platform_driver_priv->mtip_links[link_index]->net_stats.rx_errors;
  stats.Software_TX_Packets = platform_driver_priv->mtip_links[link_index]->net_stats.tx_packets;
  stats.Software_RX_Packets = platform_driver_priv->mtip_links[link_index]->net_stats.rx_packets;
  return stats;
}

int mtip_get_total_active_links(void)
{
  int link_index, total_active_links = 0;
  for(link_index = 0; link_index < MTIP_MAX_LINKS; link_index++)
  {
    if(platform_driver_priv != NULL && platform_driver_priv->mtip_links[link_index] != NULL && 
                  platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_UP){
      total_active_links++;
    }
  }
  return total_active_links;
}


bool mtip_if_link_up(int link_index)
{
  if(platform_driver_priv != NULL && platform_driver_priv->mtip_links[link_index] != NULL && platform_driver_priv->mtip_links[link_index]->state == MTIP_LINK_STATE_UP)
    return true;
  return false;
}

/**
 * mtip_is_link_in_loopback - Check if a link is in loopback mode
 * @link_index: The link index to check
 *
 * Returns true if the link is configured for loopback mode, false otherwise.
 * Used for A55 TX API blocking and promiscuous mode configuration.
 */
bool mtip_is_link_in_loopback(u32 link_index)
{
    if (link_index >= MTIP_MAX_LINKS) {
        CSMLOGERR("Invalid link_index %d (max %d)", link_index, MTIP_MAX_LINKS);
        return false;
    }
    
    if (platform_driver_priv && platform_driver_priv->mtip_links[link_index]) {
        return platform_driver_priv->mtip_links[link_index]->loopback_enabled;
    }
    return false;
}

/**
 * mtip_phy_set_loopback_mode - Set PHY loopback mode for a specific link
 * @link_index: The link index to configure
 * @loopback_mode: The loopback mode to set
 *
 * This function bridges MTIP to the PHY driver's per-interface loopback functionality
 * and ensures proper integration between MTIP's link-based and PHY's lane-based approaches
 */
int mtip_phy_set_loopback_mode(u32 link_index, enum qcom_aw_phy_loopback_mode_enum loopback_mode)
{
  u32 port_type;
  bool lanes_enabled[PHY_LANE_MAX] = {false};
  int ret = 0;

  CSMLOGINFO("Setting PHY loopback mode %d for link_index %d\n", loopback_mode, link_index);

  /* Validate link index */
  if (link_index >= MTIP_MAX_LINKS) {
    CSMLOGERR("Invalid link_index %d (max %d)", link_index, MTIP_MAX_LINKS);
    return -EINVAL;
  }

  /* Get port type from link index */
  if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0) {
    CSMLOGERR("Failed to get port type for link_index %d\n", link_index);
    return -EINVAL;
  }

  /* Get lanes enabled for this link and build the lane mapping */
  if (platform_driver_priv && platform_driver_priv->mtip_links[link_index]) {
    int i;
    for (i = 0; i < platform_driver_priv->mtip_links[link_index]->num_assigned_lanes; i++) {
      u32 lane_index = platform_driver_priv->mtip_links[link_index]->assigned_lane_indices[i];
      u32 real_lane_number;

      if (mtip_lookup_real_lane_number_by_lane_index(lane_index, &real_lane_number) == 0) {
        if (real_lane_number < PHY_LANE_MAX) {
          lanes_enabled[real_lane_number] = true;
          CSMLOGINFO("Link %d maps to PHY lane %d (lane_index %d)\n", 
                     link_index, real_lane_number, lane_index);
        }
      }
    }
  } else {
    CSMLOGERR("MTIP link not found for link_index %d\n", link_index);
    return -EINVAL;
  }

  /* Call PHY driver's per-interface loopback function */
  if (qcom_aw_phy_driver_iface_ops.eth_phy_iface_set_phy_loopback_mode) {
    ret = qcom_aw_phy_driver_iface_ops.eth_phy_iface_set_phy_loopback_mode(
      port_type, lanes_enabled, loopback_mode);

    if (ret == 0) {
      CSMLOGINFO("Successfully set PHY loopback mode %d for link_index %d\n",
                 loopback_mode, link_index);
    } else {
      CSMLOGERR("Failed to set PHY loopback mode %d for link_index %d, error: %d\n",
                loopback_mode, link_index, ret);
    }
  } else {
    CSMLOGERR("PHY loopback interface function not available\n");
    ret = -ENOSYS;
  }

  return ret;
}


void post_mtip_process_loopback_config(u32 link_index, bool enable)
{
    struct mtip_loopback_config_task* taskstruct = kmalloc(sizeof(struct mtip_loopback_config_task), GFP_ATOMIC);
    u32 port_type;
    
    if (!taskstruct) {
        CSMLOGERR("Failed to allocate loopback config task for link %d", link_index);
        return;
    }
    
    taskstruct->link_index = link_index;
    taskstruct->enable = enable;
    
    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0) {
        port_type = MTIP_PORT_TYPE_FH_0; // Default fallback
    }
    
    mtip_queue_work(MTIP_WORKQ_TASK_PROCESS_LOOPBACK_CONFIG, taskstruct, port_type);
}

int mtip_set_loopback_interfaces(char **interface_list, int interface_count)
{
    int i, ret = 0;
    u32 link_index;
    
    CSMLOGINFO("Setting loopback for %d interfaces\n", interface_count);

    /* Validate input parameters */
    if (!interface_list || interface_count <= 0) {
        CSMLOGERR("No loopback interfaces provided or invalid count: %d\n", interface_count);
        return -EINVAL;
    }

    /* Process each interface name and convert to link index */
    for (i = 0; i < interface_count; i++) {
        if (!interface_list[i]) {
            CSMLOGERR("Interface name at index %d is NULL\n", i);
            ret = -EINVAL;
            continue;
        }

        /* Convert interface name to link index */
        if (mtip_lookup_link_index_by_name(interface_list[i], &link_index) == 0) {
            CSMLOGINFO("Enabling loopback for %s (link_index=%d)\n", 
                       interface_list[i], link_index);
            
            /* Queue work for this specific interface */
            post_mtip_process_loopback_config(link_index, true);
        } else {
            CSMLOGERR("Interface %s not found\n", interface_list[i]);
            ret = -ENODEV;
        }
    }

    return ret;
}

/* API exposed structure */
const struct ldmm_eth_iface_ops mtip_driver_iface_ops = {
    .ldmm_eth_iface_get_stats_info = mtip_get_stats_info,
    .ldmm_eth_iface_get_config_info = mtip_get_config_info,
    .ldmm_eth_iface_get_if_link_up = mtip_if_link_up,
    .ldmm_eth_iface_set_loopback_interfaces = mtip_set_loopback_interfaces,
};
EXPORT_SYMBOL(mtip_driver_iface_ops);
/**
 * mtip_assign_lanes_for_loopback - Assign lanes to all links (before PHY loopback)
 * @requested_link_index: The specific link requested for loopback
 *
 * This function performs minimal lane assignment to ALL 4 links of the port.
 * This is called BEFORE PHY loopback setup.
 */
static int mtip_assign_lanes_for_loopback(u32 requested_link_index)
{
    u32 port_type;
    struct mtip_link_info* link_info = NULL;
    int i;

    CSMLOGINFO("Step 1: Assigning lanes for loopback - link_index %d\n", requested_link_index);

    /* Validate link index */
    if (requested_link_index >= MTIP_MAX_LINKS) {
        CSMLOGERR("Invalid link_index %d (max %d)", requested_link_index, MTIP_MAX_LINKS);
        return -EINVAL;
    }

    /* Get port type from link index */
    if (mtip_lookup_port_type_by_link_index(requested_link_index, &port_type) < 0) {
        CSMLOGERR("Failed to get port type for link_index %d", requested_link_index);
        return -EINVAL;
    }

    /* Validate port structure exists */
    if (!platform_driver_priv || !platform_driver_priv->mtip_ports[port_type]) {
        CSMLOGERR("Port structure not found for port_type %d", port_type);
        return -EINVAL;
    }

    /* Assign lanes to ALL 4 links of the port (1 lane per link in 4x25G) */
    for (i = 0; i < 4; i++) {
        u32 port_link_index;
        if (mtip_lookup_link_index_by_port_type_and_real_link(&port_link_index, port_type, i) == 0) {
            if (platform_driver_priv->mtip_links[port_link_index] != NULL) {
                link_info = platform_driver_priv->mtip_links[port_link_index];

                /* Assign lane to each link (lane_index = link_index in 4x25G) */
                link_info->assigned_lane_indices[0] = port_link_index;
                link_info->num_assigned_lanes = 1;
                link_info->lanes_assignment_complete = true;

                CSMLOGERR("Assigned lane %d to link_index %d\n", port_link_index, port_link_index);
            }
        }
    }

    CSMLOGINFO("Complete: Lane assignment done\n");
    return 0;
}

/**
 * mtip_configure_port_and_lanes_for_loopback - Complete port and lane configuration (after PHY loopback)
 * @link_index: The link index for loopback
 *
 * This function configures ALL port and lane properties matching C2C2 implementation:
 * - Port: state, autoneg, priv_flags, sfp_port_type, port_config, lane_config
 * - Lanes: state, sfp_port_type, speed_mask, QSFP info
 * This is called AFTER PHY loopback setup.
 */
static int mtip_configure_port_and_lanes_for_loopback(u32 link_index)
{
    u32 port_type;
    int i;
    u32 first_lane_index;
    bool is_optical = false;
    enum mtip_port_config_enum loopback_port_config;
    int loopback_sfp_type;

    CSMLOGINFO("Step 2: Configuring port and lanes for loopback - link_index %d\n", link_index);

    /* Get port type from link index */
    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0) {
        CSMLOGERR("Failed to get port type for link_index %d", link_index);
        return -EINVAL;
    }

    /* Validate port structure exists */
    if (!platform_driver_priv || !platform_driver_priv->mtip_ports[port_type]) {
        CSMLOGERR("Port structure not found for port_type %d", port_type);
        return -EINVAL;
    }

    /* Determine transceiver type (DAC vs optical) from the first lane of the port.
       If optical is connected, RSFEC speed mode must be used. AN is off for both cases. */
    if (mtip_lookup_lane_index_by_port_type_and_real_lane(&first_lane_index, port_type, 0) == 0 &&
        platform_driver_priv->mtip_lanes[first_lane_index] != NULL)
    {
        is_optical = (platform_driver_priv->mtip_lanes[first_lane_index]->sfp_port_type == PORT_FIBRE);
    }

    if (is_optical)
    {
        loopback_sfp_type = PORT_FIBRE;
        loopback_port_config = MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC;
        CSMLOGINFO("Port %d: optical transceiver detected, using 4x25G RSFEC config\n", port_type);
    }
    else
    {
        loopback_sfp_type = PORT_DA;
        loopback_port_config = MTIP_PORT_CONFIG_4x25GBASE_R;
        CSMLOGINFO("Port %d: DAC transceiver detected, using 4x25G config\n", port_type);
    }
    
    /* Don't do autoneg for loopback modes */
    platform_driver_priv->mtip_ports[port_type]->autoneg = false;
    
    /* Set the port state as connected */
    platform_driver_priv->mtip_ports[port_type]->port_state = MTIP_PORT_STATE_CONNECTED;
    
    /* Set port priv flags based on detected transceiver */
    platform_driver_priv->mtip_ports[port_type]->port_priv_flags = (1 << loopback_port_config);
    
    /* Set port sfp type and configuration based on transceiver */
    platform_driver_priv->mtip_ports[port_type]->sfp_port_type = loopback_sfp_type;
    platform_driver_priv->mtip_ports[port_type]->port_config = loopback_port_config;

    CSMLOGINFO("Port %d: autoneg=false, state=CONNECTED, priv_flags=%s, sfp=%s\n",
               port_type,
               is_optical ? "4x25G_RSFEC" : "4x25G",
               is_optical ? "PORT_FIBRE" : "PORT_DA");

    /* Configure lane_config for all 4 lanes in port structure */
    for (i = 0; i < PHY_LANE_MAX; ++i) {
        platform_driver_priv->mtip_ports[port_type]->lane_config[i].lane_enabled = true;
        platform_driver_priv->mtip_ports[port_type]->lane_config[i].lane_speed = PHY_LANE_SPEED_25G;
        platform_driver_priv->mtip_ports[port_type]->lane_config[i].link_index = (port_type * PHY_LANE_MAX) + i;
    }
    CSMLOGINFO("Configured port lane_config: 4 lanes enabled at 25G speed\n");

    
    /* Configure all 4 lanes for the port */
    for (i = 0; i < PHY_LANE_MAX; ++i) {
        u32 lane_index;
        if (mtip_lookup_lane_index_by_port_type_and_real_lane(&lane_index, port_type, i) == 0) {
            if (platform_driver_priv->mtip_lanes[lane_index] != NULL) {
                
                /* Set the lane state as CONNECTED */
                platform_driver_priv->mtip_lanes[lane_index]->lane_state = MTIP_LANE_STATE_CONNECTED;
                
                /* Set the lane sfp type based on detected transceiver */
                platform_driver_priv->mtip_lanes[lane_index]->sfp_port_type = loopback_sfp_type;

                /* Set the lane speed mask - 25G only for forced 4x25G mode */
                platform_driver_priv->mtip_lanes[lane_index]->speed_mask = TRX_LANE_SPEED_25G;
                
                /* Set the lane properties for TRX/QSFP */
                platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info.trx_module_type = TRX_QSFP_PLS_QSFP28_QSFP56;
                platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info.speed_mask = TRX_LANE_SPEED_25G;
                platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info.trx_laneinfo = 0xF;  // 4 lanes
                platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info.trx_bout_cfg = 0;
                
                CSMLOGINFO("Lane %d: state=CONNECTED, sfp=%s, speed_mask=25G, qsfp_type=QSFP28/56\n",
                           lane_index, is_optical ? "PORT_FIBRE" : "PORT_DA");
            }
        }
    }

    CSMLOGINFO("Step 2 Complete: Port and lane configuration done \n");
    return 0;
}



/**
 * run_mtip_process_loopback_config - Workq handler for loopback configuration
 * @work_ptr: Work pointer containing loopback configuration data
 *
 * This function processes loopback configuration requests through the workq.
 * It uses a modular approach with separate helper functions for each step.
 */
void run_mtip_process_loopback_config(void *work_ptr)
{
    struct mtip_loopback_config_task *taskstruct = (struct mtip_loopback_config_task *)work_ptr;
    struct net_device *netdev = NULL;
    struct mtip_netdev_priv *priv;
    int ret = 0;
    u32 port_type;
    u32 priv_flags =0;

    CSMLOGINFO("Processing loopback configuration in workq\n");

    if (!taskstruct || !platform_driver_priv) {
        CSMLOGERR("Invalid parameters\n");
        goto cleanup;
    }

    if (platform_driver_priv->mtip_links[taskstruct->link_index] != NULL) {
        netdev = platform_driver_priv->mtip_links[taskstruct->link_index]->dev;

        if (netdev != NULL) {
            CSMLOGINFO("Enabling loopback mode for link_index %d\n", taskstruct->link_index);
            
            priv = netdev_priv(netdev);

            /* Mark interface as in loopback mode */
            platform_driver_priv->mtip_links[taskstruct->link_index]->loopback_enabled = true;

            /* Set promiscuous mode for loopback */
            ret = mtip_mac_set_promisc_mode(priv, true);
            if (ret) {
                CSMLOGERR("Failed to set promiscuous mode");
                goto cleanup;
            }

            if (!mtip_loopback_enable_arp) {
                netdev->flags |= IFF_NOARP;
            }


            ret = mtip_assign_lanes_for_loopback(taskstruct->link_index);
            if (ret) {
                CSMLOGERR("Failed: Lane assignment");
                goto cleanup;
            }

           
            CSMLOGINFO("Configuring PHY loopback mode for link_index %d\n", taskstruct->link_index);
            ret = mtip_phy_set_loopback_mode(taskstruct->link_index, QCOM_AW_PHY_NEAR_END_SERIAL_LB);
            if (ret) {
                CSMLOGERR("Failed to set PHY loopback mode");
                goto cleanup;
            }

          
            ret = mtip_configure_port_and_lanes_for_loopback(taskstruct->link_index);
            if (ret) {
                CSMLOGERR("Failed: Port and lane configuration");
                goto cleanup;
            }

            if (mtip_lookup_port_type_by_link_index(taskstruct->link_index, &port_type) < 0) {
                CSMLOGERR("Invalid port_type for link_index %d", taskstruct->link_index);
                goto cleanup;
            }

            /* Set interface-level priv_flags for the loopback interface */
            priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_25G_ONLY;
            priv_flags &= MTIP_DEVICE_PRIV_FLAGS_SINGLE_LANE_MASK;
            priv->priv_flags_set = true;
            priv->priv_flags = priv_flags;

            platform_driver_priv->mtip_ports[port_type]->autoneg = AUTONEG_DISABLE;
            platform_driver_priv->mtip_ports[port_type]->port_priv_flags_optical = 0;
            platform_driver_priv->mtip_ports[port_type]->next_speed_retry_count = 0;
        
            rtnl_lock();
            if (!(netdev->flags & IFF_UP)) {
                CSMLOGINFO("Bringing up interface link_index %d\n", taskstruct->link_index);
                ret = dev_open(netdev, NULL);
                if (ret) {
                    CSMLOGERR("Failed to bring up interface: %d\n", ret);
                } else {
                    CSMLOGINFO("Interface link_index %d is now up\n", taskstruct->link_index);
                }
            } else {
                CSMLOGERR("Interface link_index %d is already up\n", taskstruct->link_index);
            }
            rtnl_unlock();

            /* Update topology */
            mtip_update_topology();

            /* Send event to clients */
            mtip_client_send_event(ETH_ECPRISS_EVENT_UP, taskstruct->link_index);

            CSMLOGINFO("Loopback configuration completed for link_index %d\n", taskstruct->link_index);
        } else {
            CSMLOGERR("Network device not found for link_index %d\n", taskstruct->link_index);
        }
    } else {
        CSMLOGERR("MTIP link not found for link_index %d\n", taskstruct->link_index);
    }

cleanup:
    /* Clean up allocated memory */
    if (taskstruct) {
        kfree(taskstruct);
    }
}

static int mtip_module_init(void)
{
   int i;
   bool is_dma_ready = false;
   int ret = 0;

   CSMLOGERR("mtip_module_init called\n");

    if (mtip_rumi_platform != MTIP_PLATFORM_SOC) { 
   // process the module parameters
   // tx_delay parameter
        for (i = 0; i < (sizeof mtip_tx_delay / sizeof (int)); i++)
        {
            if (mtip_tx_delay[i] != 0) 
            {
                CSMLOGDBG("mtip_tx_delay[%d] = %d\n", i, mtip_tx_delay[i]);
            }
        }

        CSMLOGDBG("mtip_tx_delay module params set for %d\n", mtip_tx_delay_argc);

        for (i = mtip_tx_delay_argc; i < MTIP_MAX_LINKS; ++i) {
            mtip_tx_delay[i] = TX_DELAY_DEFAULT_VAL;
        }

        // rx_delay parameter
        for (i = 0; i < (sizeof mtip_rx_delay / sizeof (int)); i++)
        {
            if (mtip_rx_delay[i] != 0) 
            {
                CSMLOGDBG("mtip_rx_delay[%d] = %d\n", i, mtip_rx_delay[i]);
            }
        }
        CSMLOGDBG("mtip_rx_delay module params set for %d\n", mtip_rx_delay_argc);

        for (i = mtip_rx_delay_argc; i < MTIP_MAX_LINKS; ++i) {
            mtip_rx_delay[i] = RX_DELAY_DEFAULT_VAL;
        }
    }

#ifdef FEATURE_MTIP_TEST_DEBUG_FS
   mtip_setup_debugfs();
#endif /* FEATURE_MTIP_TEST_DEBUG_FS */

   mtip_setup_sysfs();

   // initialize the workq
   ret = mtip_initialize_workq();

   // HANDLE THE ERROR
   if (ret < 0)
   {
      goto out;
   }

   // initialize the hashmap
   ret = mtip_hashmap_initialize();

   // HANDLE THE ERROR
   if (ret < 0)
   {
      goto out;
   }

   // allocate the memory for the platform device private struct
   platform_driver_priv = (struct mtip_platform_driver_priv *)kmalloc(sizeof(struct mtip_platform_driver_priv), GFP_KERNEL);

   // HANDLE THE ERROR
   if (platform_driver_priv == NULL)
   {
      CSMLOGERR("Unable to allocate platform_driv_priv memory!");
      ret = -ENOMEM;
      goto out;
   }

   memset(platform_driver_priv, 0,sizeof(struct mtip_platform_driver_priv));

   // initialize the platform driver error status
   platform_driver_priv->perr = 0;

   // initialize the remaining platform_driver fields to default
   platform_driver_priv->dma_is_ready = false;

   // initialize the topology
   platform_driver_priv->topology = NULL;

   // Init IPC log buffers
   platform_driver_priv->ipc_log_buf = ipc_log_context_create(CSM_IPC_LOG_PAGES,
		"csm_mtip", 0);
	if (platform_driver_priv->ipc_log_buf == NULL)
    {
		CSMLOGERR("mtip_init(): failed to create IPC log context, continue...\n");
    }
    else
    {
        CSMLOGDBG("mtip_init(): IPC log context created successfully, continue...\n");
    }
   platform_driver_priv->ipc_ptp_log_buf = ipc_log_context_create(CSM_IPC_LOG_PAGES,
		"csm_ptp_mtip", 0);
	if (platform_driver_priv->ipc_ptp_log_buf == NULL)
    {
		CSMLOGERR("mtip_init(): failed to create IPC log context, continue...\n");
    }
    else
    {
        CSMLOGDBG("mtip_init(): IPC log context created successfully, continue...\n");
    }

    platform_driver_priv->ipc_log_buf_low = ipc_log_context_create(CSM_IPC_LOG_PAGES,
		"csm_mtip_low", 0);
    if (platform_driver_priv->ipc_log_buf_low == NULL)
    {
		CSMLOGERR("mtip_init(): failed to create IPC log LOW context, continue...\n");
    }
    else
    {
        CSMLOGDBG("mtip_init(): IPC log context LOW created successfully, continue...\n");
    }

    platform_driver_priv->ipc_log_buf_dbg = ipc_log_context_create(CSM_IPC_LOG_PAGES,
		"csm_mtip_dbg", 0);
    if (platform_driver_priv->ipc_log_buf_dbg == NULL)
    {
		CSMLOGERR("mtip_init(): failed to create IPC log LOW context, continue...\n");
    }
    else
    {
        CSMLOGDBG("mtip_init(): IPC log context LOW created successfully, continue...\n");
    }

    CSMLOGDBG("FH Loopback mode is %d, C2C2 Loopback mode is:%d\n", mtip_loopback_mode,mtip_c2c2_loopback_mode);

    if (mtip_rumi_platform != MTIP_PLATFORM_SOC)
    {
        if (mtip_loopback_mode != MTIP_MODE_DEFAULT)
        {
            CSMLOGINFO("Mode: RUMI with LOOPBACK\n");
        }
        else
        {
            CSMLOGINFO("Mode: RUMI NO LOOPBACK\n");
        }
    }
    else
    {
        if (mtip_loopback_mode == MTIP_MODE_DEFAULT)
        {
            CSMLOGINFO("Mode: SOC:FH: NO LOOPBACK\n");
        }
        else if (mtip_loopback_mode == MTIP_MODE_PHY_LOOPBACK)
        {
            CSMLOGINFO("Mode: SOC:FH: PHY LOOPBACK\n");
        }
        else if (mtip_loopback_mode == MTIP_MODE_LOOPBACK)
        {
            CSMLOGINFO("Mode: SOC:FH: PCS LOOPBACK\n");
        }
        if (mtip_c2c2_loopback_mode == MTIP_MODE_DEFAULT)
        {
            CSMLOGINFO("Mode: SOC:C2C2: NO LOOPBACK\n");
        }
        else if (mtip_c2c2_loopback_mode == MTIP_MODE_PHY_LOOPBACK)
        {
            CSMLOGINFO("Mode: SOC:C2C2: PHY LOOPBACK\n");
        }
        else if (mtip_c2c2_loopback_mode == MTIP_MODE_LOOPBACK)
        {
            CSMLOGINFO("Mode: SOC:C2C2 with PCS LOOPBACK\n");
        }
    }

    // initialize the dma array of allocs
    for (i = 0; i < MTIP_DMA_ALLOC_LIST_MAX; ++i) 
    {
        mtip_dma_alloc_initialize(i);
    }

   if (mtip_rumi_platform == MTIP_PLATFORM_SOC) 
   {
       // register with the PHY
       mtip_phy_register_eth();
   }

   for (i = 0; i < MTIP_MAX_CLIENTS; ++i) {
       platform_driver_priv->clients[i].events_cb = NULL;
       platform_driver_priv->clients[i].ready_cb = NULL;
   }

   mtip_dma_register_params.notify_ready = mtip_dma_ready_cb;
   mtip_dma_register_params.userdata_ready = NULL;
   mtip_dma_register_params.notify_rx_comp = mtip_dma_rx_comp_cb;
   mtip_dma_register_params.userdata_rx = NULL;
   mtip_dma_register_params.notify_tx_comp = mtip_dma_tx_comp_cb;
   mtip_dma_register_params.userdata_tx = NULL;
   mtip_dma_register_params.notify_tx_comp_irq = mtip_dma_tx_irq_comp_cb;
   mtip_dma_register_params.userdata_tx_irq = NULL;

   // initialize the spinlock
   spin_lock_init(&platform_driver_priv->driver_lock);

   // register with the dma driver
   ret = (ecpri_dma_eth_driver_ops.ecpri_dma_eth_register)(&mtip_dma_register_params, &is_dma_ready);

   // HANDLE THE ERROR
   if (ret < 0)
   {
      CSMLOGERR("Failed to register with DMA");
      goto cleanup;
   }

   // set the dma_is_ready flag
   platform_driver_priv->dma_is_ready = is_dma_ready;

   if (is_dma_ready) 
   {
      CSMLOGDBG("DMA is ready: going to register platform driver\n");

      ret = mtip_register_platform_driver();

      // HANDLE THE ERROR
      if (ret < 0)
      {
         CSMLOGERR("Failed to register platform driver");
         goto cleanup;
      }
   }

   // register the debug eth platform driver
   mtip_debug_eth_register_platform_driver();

   // register panic notifier
   atomic_notifier_chain_register(&panic_notifier_list, &mtip_panic_blk);

   BLOCKING_INIT_NOTIFIER_HEAD(&lassen_qxdm_timer_update_notifr);

   goto ret;

cleanup:

   if (platform_driver_priv->ipc_log_buf)
		ipc_log_context_destroy(platform_driver_priv->ipc_log_buf);
   if (platform_driver_priv->ipc_log_buf_low)
        ipc_log_context_destroy(platform_driver_priv->ipc_log_buf_low);
   kfree(platform_driver_priv);
   platform_driver_priv = NULL;

out:
#ifdef FEATURE_MTIP_TEST_DEBUG_FS
   mtip_del_debugfs();
#endif /* FEATURE_MTIP_TEST_DEBUG_FS */
ret:   
   return ret;
}

static void mtip_module_exit(void)
{
   int i;
   CSMLOGERR("mtip_module_exit called\n");

   // finalize the workq
   mtip_destroy_workq();
   // destroy the hashmap
   mtip_hashmap_destroy();
   mtip_eth_deregister_events_cb();

   mtip_del_sysfs();

#ifdef FEATURE_MTIP_TEST_DEBUG_FS
   mtip_del_debugfs();
#endif /* FEATURE_MTIP_TEST_DEBUG_FS */
   // deregister panic notifier
   atomic_notifier_chain_unregister(&panic_notifier_list, &mtip_panic_blk);

   mtip_debug_eth_unregister_platform_driver();
   if (!platform_driver_priv->perr)
   {
       platform_driver_unregister(&ethernet_mac_lane_driver);
       platform_driver_unregister(&ethernet_mac_link_driver);
       platform_driver_unregister(&ethernet_mac_port_driver);
       platform_driver_unregister(&ethernet_mac_platform_driver);
   }
   // deregister with the dma driver
   (ecpri_dma_eth_driver_ops.ecpri_dma_eth_deregister)();

   if (mtip_rumi_platform == MTIP_PLATFORM_SOC) 
   {
       // dergister with the phy driver
       mtip_phy_deregister_eth();
   }
   /* memory leak needs to be fixed later */
   // finalize the dma array of allocs
   for (i = 0; i < MTIP_DMA_ALLOC_LIST_MAX; ++i)
   {
       mtip_dma_alloc_finalize(i);
   }

   
   if (platform_driver_priv->ipc_log_buf)
		ipc_log_context_destroy(platform_driver_priv->ipc_log_buf);
   if (platform_driver_priv->ipc_log_buf_low)
        ipc_log_context_destroy(platform_driver_priv->ipc_log_buf_low);

   // free topology
   mtip_free_topology();
   // free the platform driver priv
   kfree(platform_driver_priv);
   platform_driver_priv = NULL;

   return;
}

module_init(mtip_module_init);
module_exit(mtip_module_exit);

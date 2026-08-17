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

#include "mtip_ethtool.h"
#include "mtip.h"
#include "mtip_platform.h"
#include "mtip_device.h"
#include "mtip_phy.h"
#include "mtip_macstats.h"
#include "mtip_mac.h"
#include "mtip_pcs.h"
#include "mtip_debug_eth.h"
#include "mtip_client.h"
#include "mtip_security.h"

int mtip_ethtool_debug_logging_enable = 0;
int mtip_ethtool_ptp_logging_enable = 0;

static const char * const mtip_ethtool_stat_strings[] = {
    "EtherStatsOctets",
    "OctetsReceivedOK",
	"VLANReceivedOK",
    "InErrors",
    "InUCastPkts",
    "InMCastPkts",
    "InBCastPkts",
    "EtherStatsDrops",
    "EtherStatsPkts",
    "OctetsTransmittedOK",
    "VLANTransmittedOK",
    "OutErrors",
    "OutUCastPkts",
    "OutMCastPkts",
    "OutBCastPkts",
};

#define MTIP_ETHTOOL_STATS_LEN	ARRAY_SIZE(mtip_ethtool_stat_strings)

static const char* const mtip_ethtool_priv_flags_str_arr[MTIP_ETHTOOL_PRIV_FLAGS_LEN] = {
    "1x100GBASE_R2",
    "1x100GBASE_R4",
    "2x50GBASE_R",
    "1x50GBASE_R",
    "1x50GBASE_R2",
    "1x40GBASE_R4",
    "4x25GBASE_R",
    "1x25GBASE_R",
    "4x10GBASE_R",
    "1x10GBASE_R",
};

static const char* const mtip_ethtool_port_config_str_arr[] = {
   "1x100GBASE_R",
   "1x100GBASE_R_RSFEC_LL",
   "1x100GBASE_R_RSFEC",
   "1x100GBASE_R2",
   "1x100GBASE_R2_RSFEC",
   "1x100GBASE_R4",
   "1x100GBASE_R4_RSFEC",
   "2x50GBASE_R",
   "2x50GBASE_R_RSFEC",
   "2x50GBASE_R2",
   "2x50GBASE_R2_FEC",
   "2x50GBASE_R2_LUAI",
   "2x50GBASE_R2_LUAI_FEC",
   "1x50GBASE_R",
   "1x50GBASE_R_RSFEC",
   "1x50GBASE_R2",
   "1x50GBASE_R2_RSFEC",
   "1x50GBASE_R2_LUAI",
   "1x50GBASE_R2_LUAI_FEC",
   "1x40GBASE_R4",
   "1x40GBASE_R4_FEC",
   "4x25GBASE_R",
   "4x25GBASE_R_FEC",
   "4x25GBASE_R_RSFEC",
   "1x25GBASE_R",
   "1x25GBASE_R_FEC",
   "1x25GBASE_R_RSFEC",
   "4x10GBASE_R",
   "4x10GBASE_R_FEC",
   "1x10GBASE_R",
   "1x10GBASE_R_FEC",
};

enum mtip_priv_flag_enum
{
	MTIP_PRIV_FLAG_1x100GBASE_R2,
	MTIP_PRIV_FLAG_1x100GBASE_R4,
	MTIP_PRIV_FLAG_2x50GBASE_R,
	MTIP_PRIV_FLAG_1x50GBASE_R,
	MTIP_PRIV_FLAG_1x50GBASE_R2,
	MTIP_PRIV_FLAG_1x40GBASE_R4,
	MTIP_PRIV_FLAG_4x25GBASE_R,
	MTIP_PRIV_FLAG_1x25GBASE_R,
	MTIP_PRIV_FLAG_4x10GBASE_R,
	MTIP_PRIV_FLAG_1x10GBASE_R,
	MTIP_PRIV_FLAG_MAX
};

#define MTIP_ETHTOOL_REG_OFFSET_ARRAY_SIZE 14
int mtip_ethtool_reg_buffer_size;

enum mtip_ethtool_regs_e
{
    MTIP_ETHTOOL_MAC,
    MTIP_ETHTOOL_PCS,
    MTIP_ETHTOOL_MAC_WRAPPER,
    MTIP_ETHTOOL_MAC_STATS,
    MTIP_ETHTOOL_RSFEC,
    MTIP_ETHTOOL_REG_MAX
};

struct mtip_ethtool_reg_offset
{
    u32 start_offset;
    u32 end_offset;
    enum mtip_ethtool_regs_e mtip_ethtool_regs;
};


struct mtip_ethtool_reg_offset mtip_ethtool_reg_offset_val[MTIP_ETHTOOL_REG_OFFSET_ARRAY_SIZE] =
{   {0,             0x000000A0,     MTIP_ETHTOOL_MAC},
    {0,             0x000000D4,     MTIP_ETHTOOL_PCS},
    {0x00000320,    0x0000036C,     MTIP_ETHTOOL_PCS},
    {0x00000640,    0x0000068C,     MTIP_ETHTOOL_PCS},
    {0x00020000,    0x00020040,     MTIP_ETHTOOL_PCS},
    {0x00020100,    0x0002019C,     MTIP_ETHTOOL_PCS},
    {0,             0x00000384,     MTIP_ETHTOOL_MAC_WRAPPER},
    {0,             0x0000001C,     MTIP_ETHTOOL_MAC_STATS},
    {0x00000100,    0x000004CC,     MTIP_ETHTOOL_MAC_STATS},
    {0,             0x0000007c,     MTIP_ETHTOOL_RSFEC},
    {0x00000100,    0x0000012c,     MTIP_ETHTOOL_RSFEC},
    {0x00000200,    0x0000023c,     MTIP_ETHTOOL_RSFEC},
    {0x00000284,    0x00000290,     MTIP_ETHTOOL_RSFEC},
    {0x000002C0,    0x000002D8,     MTIP_ETHTOOL_RSFEC}
};


const char* mtip_ethtool_get_priv_flags_str(u32 index)
{
    return mtip_ethtool_priv_flags_str_arr[index];
}

#define MTIP_ETHTOOL_PORT_CONFIG_LEN ARRAY_SIZE(mtip_ethtool_port_config_str_arr)

const char* mtip_ethtool_get_port_config_str(u32 index)
{
    return mtip_ethtool_port_config_str_arr[index];
}

static int mtip_ethtool_get_sset_count(struct net_device *netdev, int sset)
{
	int eip_ethtool_sset = mtip_security_get_sset_count(netdev);

	switch (sset) {
	case ETH_SS_STATS:
		return (MTIP_ETHTOOL_STATS_LEN + eip_ethtool_sset);
    case ETH_SS_PRIV_FLAGS:
        return MTIP_ETHTOOL_PRIV_FLAGS_LEN;
	default:
		return -EOPNOTSUPP;
	}
}

static void mtip_ethtool_get_strings(struct net_device *netdev, u32 stringset, u8 *data)
{
    int i;

    if (stringset == ETH_SS_STATS) 
    {
		for (i = 0; i < MTIP_ETHTOOL_STATS_LEN; i++) {
			strlcpy(data, mtip_ethtool_stat_strings[i],
				ETH_GSTRING_LEN);
			data += ETH_GSTRING_LEN;
		}
        mtip_security_get_strings(netdev, data);
    }
    else if (stringset == ETH_SS_PRIV_FLAGS) 
    {
        for (i = 0; i < MTIP_ETHTOOL_PRIV_FLAGS_LEN; i++) {
            strlcpy(data, mtip_ethtool_priv_flags_str_arr[i],
                ETH_GSTRING_LEN);
            data += ETH_GSTRING_LEN;
        }
    }
}

static void mtip_ethtool_get_stats(struct net_device *netdev, struct ethtool_stats *stats, u64 *data) 
{
    memset(data, 0, MTIP_ETHTOOL_STATS_LEN*sizeof(u64));

    // read the stats from the HW
    mtip_macstats_get_stats(netdev, data);

    // Get stats
    mtip_security_get_stats(netdev, &data[MTIP_ETHTOOL_STATS_LEN]);
}

void mtip_ethtool_get_dev_regs
(
    struct platform_device* port_pdev,
    void __iomem *dev_base_addr,
    void *buf,
    u32 *wr_ptr,
    struct resource *dev_resource,
    struct mtip_ethtool_reg_offset *reg_offset_array,
    u32 reg_offset_array_idx
)
{
    u32 *rbuf = (u32 *)buf;
    u32 *reg_addr = 0x0;
    u32 reg_offset = 0;
    
    CSMLOGINFO("mtip_ethtool: mtip_get_dev_regs port base = 0x%x, port end 0x%x port size = 0x%x "
               "Dev wr_ptr: %d addr 0x%x arry_idx %d \n",dev_resource->start, dev_resource->end,
               resource_size(dev_resource), *wr_ptr, dev_base_addr, reg_offset_array_idx);

    for (reg_offset = reg_offset_array[reg_offset_array_idx].start_offset; 
         (reg_offset <= reg_offset_array[reg_offset_array_idx].end_offset) &&
         (*wr_ptr < mtip_ethtool_reg_buffer_size);)
    {
        reg_addr = (u32*)(dev_base_addr + reg_offset);
        rbuf[(*wr_ptr)++] = (u32)(dev_resource->start + reg_offset);    // Reg Address
        rbuf[(*wr_ptr)++] = (u32)ioread32(reg_addr);                    // Reg Value
        reg_offset+=4;
    }
}

static void mtip_ethtool_dump_regs(u32 link_index, void *buf)
{
    u32 wr_idx = 0;
    struct mtip_link_device_info* link_device;
    struct resource *dev_resource = NULL;
    u32 mtip_reg_idx = 0;
    u32 port_type;

    struct platform_device* pdev;
    void __iomem *dev_ioaddr = NULL;

    CSMLOGINFO("mtip_ethtool: Entering mtip_ethtool_dump_regs with link_idx %d \n", link_index);

    link_device = &platform_driver_priv->devices.link_devices[link_index];

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return;
    }

    for (mtip_reg_idx = 0; mtip_reg_idx < MTIP_ETHTOOL_REG_OFFSET_ARRAY_SIZE; mtip_reg_idx++)
    {
        switch (mtip_ethtool_reg_offset_val[mtip_reg_idx].mtip_ethtool_regs)
        {
            case MTIP_ETHTOOL_MAC:
                dev_ioaddr = link_device->mac_ioaddr;
                pdev = platform_driver_priv->devices.link_devices[link_index].link_pdev;
                dev_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mac");
            break;

            case MTIP_ETHTOOL_PCS:
                dev_ioaddr = link_device->pcs_ioaddr;
                pdev = platform_driver_priv->devices.link_devices[link_index].link_pdev;
                dev_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcs");
            break;

            case MTIP_ETHTOOL_MAC_WRAPPER:
                dev_ioaddr = platform_driver_priv->devices.port_devices[port_type].wrapper_base_addr;
                pdev = platform_driver_priv->devices.port_devices[port_type].port_pdev;
                dev_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mac-wrapper");
            break;

            case MTIP_ETHTOOL_MAC_STATS:
                dev_ioaddr = platform_driver_priv->devices.port_devices[port_type].macstats_base_addr;
                pdev = platform_driver_priv->devices.port_devices[port_type].port_pdev;
                dev_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "macstats");
            break;

            case MTIP_ETHTOOL_RSFEC:
                dev_ioaddr = platform_driver_priv->devices.port_devices[port_type].rsfec_base_addr;
                pdev = platform_driver_priv->devices.port_devices[port_type].port_pdev;
                dev_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rsfec");
            break;

            default:
            break;
        }
        if((dev_ioaddr != NULL) && (dev_resource != NULL))
        {
          mtip_ethtool_get_dev_regs(pdev, dev_ioaddr, buf, &wr_idx, dev_resource, mtip_ethtool_reg_offset_val, mtip_reg_idx);
        }
    }
}

static void mtip_ethtool_get_regs(struct net_device *dev, struct ethtool_regs *regs, void *buf)
{
    struct mtip_netdev_priv *priv;
    u32 link_index;

    priv = netdev_priv(dev);
    link_index = priv->link_index;

    CSMLOGINFO("mtip_ethtool: Entering mtip_get_regs with Dev %s link_idx %d \n", dev->name, link_index);

    mtip_ethtool_dump_regs(link_index, buf);
}

static int mtip_ethtool_dump_regs_len()
{
    u32 reg_buf_size = 0;
    u32 mtip_reg_idx = 0;

    for (mtip_reg_idx = 0; mtip_reg_idx < MTIP_ETHTOOL_REG_OFFSET_ARRAY_SIZE; mtip_reg_idx++)
    {
        // add 1 at the end to inlcude the reg at the current index as well
        reg_buf_size += (mtip_ethtool_reg_offset_val[mtip_reg_idx].end_offset - mtip_ethtool_reg_offset_val[mtip_reg_idx].start_offset)/4 + 1;
    }

    // Multiply by 2 to add addresses of registers in buffer
    mtip_ethtool_reg_buffer_size = reg_buf_size * 2;

    CSMLOGDBG("mtip_ethtool: reg buffer size %d  total buff size %d \n", 
               reg_buf_size, mtip_ethtool_reg_buffer_size);
   
    // return size in bytes
    return (mtip_ethtool_reg_buffer_size * sizeof(u32));
}

static int mtip_ethtool_get_regs_len(struct net_device *dev)
{
    u32 reg_buf_size = 0;

    reg_buf_size = mtip_ethtool_dump_regs_len();
    
    return reg_buf_size;
}

void mtip_ethtool_getdrvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
    CSMLOGDBG("ethtool: getdrvinfo\n");

    strlcpy(info->driver, MTIP_MAC_DRIVER, sizeof(info->driver));
	strlcpy(info->version, MTIP_MAC_DRIVER_VERSION, sizeof(info->version));
}

void mtip_ethtool_get_supported_speed_modes(struct mtip_port_info* port_info, u32 real_link_number, struct ethtool_link_ksettings *cmd)
{
    int i;
    __ETHTOOL_DECLARE_LINK_MODE_MASK(supported) = { 0, };
    bool supports_100_g = false;
    bool supports_50_g = false;
    bool supports_40_g = false;

    // Only link 0 for FH ports, and link 1 of Debug port will support 100G MAC configs
    if(((port_info->port_type != MTIP_PORT_TYPE_DEBUG) && (real_link_number == 0)) ||
       ((port_info->port_type == MTIP_PORT_TYPE_DEBUG) && (real_link_number == 1)))
    {
        supports_100_g = true;
        supports_50_g = true;
    }
    // Only link 1 for FH and Debug ports will support 50G MAC configs
    else if(real_link_number == 1)
    {
        supports_50_g = true;
    }

    // Only link 0 of FH port will support 40G MAC configuration
    if((port_info->port_type != MTIP_PORT_TYPE_DEBUG) && (real_link_number == 0))
    {
        supports_40_g = true;
    }
    linkmode_set_bit(ETHTOOL_LINK_MODE_FEC_NONE_BIT, supported);
    linkmode_set_bit(ETHTOOL_LINK_MODE_FEC_RS_BIT, supported);
    linkmode_set_bit(ETHTOOL_LINK_MODE_FEC_BASER_BIT, supported);
    for (i = 0; i < MTIP_PORT_CONFIG_MAX; ++i)
    {
        switch (i)
        {
            case MTIP_PORT_CONFIG_1x100GBASE_R:
            case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC:
            case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC_LL:
            {
// These bits are not currently supported, need to add back once kernel adds support
#if 0
                linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseCR_Full_BIT, supported);
                linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseSR_Full_BIT, supported);
                linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseDR_Full_BIT, supported);
                linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseLR_ER_FR_Full_BIT, supported);
#endif
            }
            break;

            case MTIP_PORT_CONFIG_1x100GBASE_R2:
            case MTIP_PORT_CONFIG_1x100GBASE_R2_RSFEC:
            {
                if(supports_100_g)
                {
                    linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseCR2_Full_BIT, supported);
                    linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseSR2_Full_BIT, supported);
                    linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseLR2_ER2_FR2_Full_BIT, supported);
                }
            }
            break;

            case MTIP_PORT_CONFIG_1x100GBASE_R4:
            case MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC:
            {
                // For R4, only link 0 can be mapped to 4 lanes
                if(supports_100_g && (real_link_number == 0))
                {
                    linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT, supported);
                    linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT, supported);
                    //linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseDR_Full_BIT, supported);
                    linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT, supported);
                }
            }
            break;

            case MTIP_PORT_CONFIG_2x50GBASE_R:
            case MTIP_PORT_CONFIG_2x50GBASE_R_RSFEC:
            case MTIP_PORT_CONFIG_1x50GBASE_R:
            case MTIP_PORT_CONFIG_1x50GBASE_R_RSFEC:
            {
                if(supports_50_g)
                {
                    linkmode_set_bit(ETHTOOL_LINK_MODE_50000baseCR_Full_BIT, supported);
                    linkmode_set_bit(ETHTOOL_LINK_MODE_50000baseSR_Full_BIT, supported);
                    linkmode_set_bit(ETHTOOL_LINK_MODE_50000baseLR_ER_FR_Full_BIT, supported);
                }
            }
            break;

            case MTIP_PORT_CONFIG_2x50GBASE_R2:
            case MTIP_PORT_CONFIG_2x50GBASE_R2_FEC:
            case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI:
            case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI_FEC:
            case MTIP_PORT_CONFIG_1x50GBASE_R2:
            case MTIP_PORT_CONFIG_1x50GBASE_R2_RSFEC:
            case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI:
            case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI_FEC:
            {
                if(supports_50_g)
                {
                    linkmode_set_bit(ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT, supported);
                }
            }
            break;

            case MTIP_PORT_CONFIG_1x40GBASE_R4:
            case MTIP_PORT_CONFIG_1x40GBASE_R4_FEC:
            {
                if(supports_40_g)
                {
                    linkmode_set_bit(ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT, supported);
                    linkmode_set_bit(ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT, supported);
                }
            }
            break;

            case MTIP_PORT_CONFIG_4x25GBASE_R:
            case MTIP_PORT_CONFIG_4x25GBASE_R_FEC:
            case MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC:
            case MTIP_PORT_CONFIG_1x25GBASE_R:
            case MTIP_PORT_CONFIG_1x25GBASE_R_FEC:
            case MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC:
            {
                linkmode_set_bit(ETHTOOL_LINK_MODE_25000baseCR_Full_BIT, supported);
                linkmode_set_bit(ETHTOOL_LINK_MODE_25000baseSR_Full_BIT, supported);
            }
            break;

            case MTIP_PORT_CONFIG_4x10GBASE_R:
            case MTIP_PORT_CONFIG_4x10GBASE_R_FEC:
            case MTIP_PORT_CONFIG_1x10GBASE_R:
            case MTIP_PORT_CONFIG_1x10GBASE_R_FEC:
            {
                linkmode_set_bit(ETHTOOL_LINK_MODE_10000baseCR_Full_BIT, supported);
                linkmode_set_bit(ETHTOOL_LINK_MODE_10000baseSR_Full_BIT, supported);
            }
            break;

            default:
                break;
        }
    }

    linkmode_copy(cmd->link_modes.supported, supported);

    return;
}

void mtip_ethtool_get_advertised_speed_modes(struct mtip_port_info* port_info, u32 real_link_number, struct ethtool_link_ksettings *cmd)
{
    int i;
    u32 port_priv_flags = mtip_device_filter_priv_flags(port_info->port_type);
    __ETHTOOL_DECLARE_LINK_MODE_MASK(advertised) = { 0, };
    struct qsfp_info lane_qsfp_info = {0};
    bool lane_qsfp_info_valid = false;
    bool supports_100_g = false;
    bool supports_50_g = false;
    bool supports_40_g = false;

    // Only link 0 for FH ports, and link 1 of Debug port will support 100G MAC configs
    if(((port_info->port_type != MTIP_PORT_TYPE_DEBUG) && (real_link_number == 0)) ||
       ((port_info->port_type == MTIP_PORT_TYPE_DEBUG) && (real_link_number == 1)))
    {
        supports_100_g = true;
        supports_50_g = true;
    }
    // Only link 1 for FH and Debug ports will support 50G MAC configs
    else if(real_link_number == 1)
    {
        supports_50_g = true;
    }

    // Only link 0 of FH port will support 40G MAC configuration
    if((port_info->port_type != MTIP_PORT_TYPE_DEBUG) && (real_link_number == 0))
    {
        supports_40_g = true;
    }

    if (mtip_device_lookup_lane_qsfp_cfg(port_info->port_type, &lane_qsfp_info) == 0)
    {
       lane_qsfp_info_valid = true;
    }
    linkmode_set_bit(ETHTOOL_LINK_MODE_FEC_NONE_BIT, advertised);
    linkmode_set_bit(ETHTOOL_LINK_MODE_FEC_RS_BIT, advertised);
    linkmode_set_bit(ETHTOOL_LINK_MODE_FEC_BASER_BIT, advertised);
    for (i = 0; i < MTIP_PORT_CONFIG_MAX; ++i)
    {
        if (port_priv_flags & (1<<i))
        {
            switch (i)
            {
                case MTIP_PORT_CONFIG_1x100GBASE_R:
                case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC:
                case MTIP_PORT_CONFIG_1x100GBASE_R_RSFEC_LL:
                {
                    if(supports_100_g)
                    {
                        if(lane_qsfp_info_valid)
                        {
                           if(lane_qsfp_info.trx_link_length_range == TRX_CR)
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseCR_Full_BIT, advertised);
                           else if(lane_qsfp_info.trx_link_length_range == TRX_SR)
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseSR_Full_BIT, advertised);
                           else if(lane_qsfp_info.trx_link_length_range == TRX_LR ||
                                   lane_qsfp_info.trx_link_length_range == TRX_ER ||
                                   lane_qsfp_info.trx_link_length_range == TRX_FR)
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseLR_ER_FR_Full_BIT, advertised);
                           else
                           {
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseCR_Full_BIT, advertised);
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseSR_Full_BIT, advertised);
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseLR_ER_FR_Full_BIT, advertised);
                           }
                        }
                        else
                        {
                           linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseCR_Full_BIT, advertised);
                           linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseSR_Full_BIT, advertised);
                           linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseLR_ER_FR_Full_BIT, advertised);
                        }
                    }
                }
                break;

                case MTIP_PORT_CONFIG_1x100GBASE_R2:
                case MTIP_PORT_CONFIG_1x100GBASE_R2_RSFEC:
                {
                    if(supports_100_g)
                    {
                        if(lane_qsfp_info_valid)
                        {
                           if(lane_qsfp_info.trx_link_length_range == TRX_CR)
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseCR2_Full_BIT, advertised);
                           else if(lane_qsfp_info.trx_link_length_range == TRX_SR)
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseSR2_Full_BIT, advertised);
                           else if(lane_qsfp_info.trx_link_length_range == TRX_LR ||
                                   lane_qsfp_info.trx_link_length_range == TRX_ER ||
                                   lane_qsfp_info.trx_link_length_range == TRX_FR)
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseLR2_ER2_FR2_Full_BIT, advertised);
                           else
                           {
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseCR2_Full_BIT, advertised);
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseSR2_Full_BIT, advertised);
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseLR2_ER2_FR2_Full_BIT, advertised);
                           }
                        }
                        else
                        {
                           linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseCR2_Full_BIT, advertised);
                           linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseSR2_Full_BIT, advertised);
                           linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseLR2_ER2_FR2_Full_BIT, advertised);
                        }
                    }
                }
                break;

                case MTIP_PORT_CONFIG_1x100GBASE_R4:
                case MTIP_PORT_CONFIG_1x100GBASE_R4_RSFEC:
                {
                    if(supports_100_g && (real_link_number == 0))
                    {
                        if(lane_qsfp_info_valid)
                        {
                           if(lane_qsfp_info.trx_link_length_range == TRX_CR)
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT, advertised);
                           else if(lane_qsfp_info.trx_link_length_range == TRX_SR)
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT, advertised);
                           else if(lane_qsfp_info.trx_link_length_range == TRX_DR)
                           {
                              //linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseDR_Full_BIT, advertised);

                              // Adding temporarily as current ethtool userspace is not displaying DR mode
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT, advertised);
                           }
                           else if(lane_qsfp_info.trx_link_length_range == TRX_LR ||
                                   lane_qsfp_info.trx_link_length_range == TRX_ER ||
                                   lane_qsfp_info.trx_link_length_range == TRX_FR)
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT, advertised);
                           else
                           {
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT, advertised);
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT, advertised);
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseDR_Full_BIT, advertised);
                              linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT, advertised);
                           }
                        }
                        else
                        {
                           linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT, advertised);
                           linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT, advertised);
                           linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseDR_Full_BIT, advertised);
                           linkmode_set_bit(ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT, advertised);
                        }
                    }
                }
                break;

                case MTIP_PORT_CONFIG_2x50GBASE_R:
                case MTIP_PORT_CONFIG_2x50GBASE_R_RSFEC:
                case MTIP_PORT_CONFIG_1x50GBASE_R:
                case MTIP_PORT_CONFIG_1x50GBASE_R_RSFEC:
                {
                    if(supports_50_g)
                    {
                        if(lane_qsfp_info_valid)
                        {
                           if(lane_qsfp_info.trx_link_length_range == TRX_CR)
                              linkmode_set_bit(ETHTOOL_LINK_MODE_50000baseCR_Full_BIT, advertised);
                           else if(lane_qsfp_info.trx_link_length_range == TRX_SR)
                              linkmode_set_bit(ETHTOOL_LINK_MODE_50000baseSR_Full_BIT, advertised);
                           else if(lane_qsfp_info.trx_link_length_range == TRX_LR ||
                                   lane_qsfp_info.trx_link_length_range == TRX_ER ||
                                   lane_qsfp_info.trx_link_length_range == TRX_FR)
                              linkmode_set_bit(ETHTOOL_LINK_MODE_50000baseLR_ER_FR_Full_BIT, advertised);
                           else
                           {
                              linkmode_set_bit(ETHTOOL_LINK_MODE_50000baseCR_Full_BIT, advertised);
                              linkmode_set_bit(ETHTOOL_LINK_MODE_50000baseSR_Full_BIT, advertised);
                              linkmode_set_bit(ETHTOOL_LINK_MODE_50000baseLR_ER_FR_Full_BIT, advertised);
                           }
                        }
                        else
                        {
                           linkmode_set_bit(ETHTOOL_LINK_MODE_50000baseCR_Full_BIT, advertised);
                           linkmode_set_bit(ETHTOOL_LINK_MODE_50000baseSR_Full_BIT, advertised);
                           linkmode_set_bit(ETHTOOL_LINK_MODE_50000baseLR_ER_FR_Full_BIT, advertised);
                        }
                    }
                }
                break;

                case MTIP_PORT_CONFIG_2x50GBASE_R2:
                case MTIP_PORT_CONFIG_2x50GBASE_R2_FEC:
                case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI:
                case MTIP_PORT_CONFIG_2x50GBASE_R2_LUAI_FEC:
                case MTIP_PORT_CONFIG_1x50GBASE_R2:
                case MTIP_PORT_CONFIG_1x50GBASE_R2_RSFEC:
                case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI:
                case MTIP_PORT_CONFIG_1x50GBASE_R2_LUAI_FEC:
                {
                    if(supports_50_g)
                    {
                        linkmode_set_bit(ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT, advertised);
                    }
                }
                break;

                case MTIP_PORT_CONFIG_1x40GBASE_R4:
                case MTIP_PORT_CONFIG_1x40GBASE_R4_FEC:
                {
                    if(supports_40_g)
                    {
                        if(lane_qsfp_info_valid)
                        {
                           if(lane_qsfp_info.trx_link_length_range == TRX_CR)
                              linkmode_set_bit(ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT, advertised);
                           else if(lane_qsfp_info.trx_link_length_range == TRX_SR)
                              linkmode_set_bit(ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT, advertised);
                           else
                           {
                              linkmode_set_bit(ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT, advertised);
                              linkmode_set_bit(ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT, advertised);
                           }
                        }
                        else
                        {
                           linkmode_set_bit(ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT, advertised);
                           linkmode_set_bit(ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT, advertised);
                        }
                    }
                }
                break;

                case MTIP_PORT_CONFIG_4x25GBASE_R:
                case MTIP_PORT_CONFIG_4x25GBASE_R_FEC:
                case MTIP_PORT_CONFIG_4x25GBASE_R_RSFEC:
                case MTIP_PORT_CONFIG_1x25GBASE_R:
                case MTIP_PORT_CONFIG_1x25GBASE_R_FEC:
                case MTIP_PORT_CONFIG_1x25GBASE_R_RSFEC:
                {
                    if(lane_qsfp_info_valid)
                    {
                       if(lane_qsfp_info.trx_link_length_range == TRX_CR)
                          linkmode_set_bit(ETHTOOL_LINK_MODE_25000baseCR_Full_BIT, advertised);
                       else if(lane_qsfp_info.trx_link_length_range == TRX_SR)
                          linkmode_set_bit(ETHTOOL_LINK_MODE_25000baseSR_Full_BIT, advertised);
                       else
                       {
                          linkmode_set_bit(ETHTOOL_LINK_MODE_25000baseCR_Full_BIT, advertised);
                          linkmode_set_bit(ETHTOOL_LINK_MODE_25000baseSR_Full_BIT, advertised);
                       }
                    }
                    else
                    {
                       linkmode_set_bit(ETHTOOL_LINK_MODE_25000baseCR_Full_BIT, advertised);
                       linkmode_set_bit(ETHTOOL_LINK_MODE_25000baseSR_Full_BIT, advertised);
                    }
                }
                break;

                case MTIP_PORT_CONFIG_4x10GBASE_R:
                case MTIP_PORT_CONFIG_4x10GBASE_R_FEC:
                case MTIP_PORT_CONFIG_1x10GBASE_R:
                case MTIP_PORT_CONFIG_1x10GBASE_R_FEC:
                {
                    if(lane_qsfp_info_valid)
                    {
                       if(lane_qsfp_info.trx_link_length_range == TRX_CR)
                          linkmode_set_bit(ETHTOOL_LINK_MODE_10000baseCR_Full_BIT, advertised);
                       else if(lane_qsfp_info.trx_link_length_range == TRX_SR)
                          linkmode_set_bit(ETHTOOL_LINK_MODE_10000baseSR_Full_BIT, advertised);
                       else
                       {
                          linkmode_set_bit(ETHTOOL_LINK_MODE_10000baseCR_Full_BIT, advertised);
                          linkmode_set_bit(ETHTOOL_LINK_MODE_10000baseSR_Full_BIT, advertised);
                       }
                    }
                    else
                    {
                       linkmode_set_bit(ETHTOOL_LINK_MODE_10000baseCR_Full_BIT, advertised);
                       linkmode_set_bit(ETHTOOL_LINK_MODE_10000baseSR_Full_BIT, advertised);
                    }
                }
                break;

                default:
                    break;
            }
       }
    }

    linkmode_copy(cmd->link_modes.advertising, advertised);

    return;
}

int mtip_ethtool_get_link_ksettings(struct net_device *dev, struct ethtool_link_ksettings *cmd)
{
    struct mtip_netdev_priv *priv;
    u32 link_index;
    u32 real_link_number;
    struct mtip_link_info* link_info;
    u32 port_type;
    struct mtip_port_info* port_info;
    int lane_speed = 0;
    int i;
    u32 lane_index;
    bool lane_connected=false;

    priv = netdev_priv(dev);
    link_index = priv->link_index;

    CSMLOGDBG("ethtool: get_link_ksettings for link_index: %d\n", link_index);

    link_info = platform_driver_priv->mtip_links[link_index];
    if(!link_info)
        return -EINVAL;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return -EINVAL;
    }

    if (mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number) < 0)
    {
        CSMLOGERR("invalid link_index %d", link_index);
        return -EINVAL;
    }

    port_info = platform_driver_priv->mtip_ports[port_type];
    if(!port_info)
        return -EINVAL;

    // Set the supported and advertised speed modes
    mtip_ethtool_get_supported_speed_modes(port_info, real_link_number, cmd);
    mtip_ethtool_get_advertised_speed_modes(port_info, real_link_number, cmd);

    // Set the supported port type
    linkmode_set_bit(ETHTOOL_LINK_MODE_TP_BIT, cmd->link_modes.supported);
    linkmode_set_bit(ETHTOOL_LINK_MODE_FIBRE_BIT, cmd->link_modes.supported);

    // check if lane is connected
    for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_lane_phandles; ++i)
    {
        lane_index = platform_driver_priv->devices.port_devices[port_type].lane_devices[i]->lane_index;
        if(platform_driver_priv->mtip_lanes[lane_index] != NULL &&
           platform_driver_priv->mtip_lanes[lane_index]->lane_state == MTIP_LANE_STATE_CONNECTED)
        {
            lane_connected = true;
            break;
        }
    }

    if(port_info->sfp_port_type == PORT_FIBRE && lane_connected == true)
        cmd->base.port = PORT_FIBRE;
    else if(port_info->sfp_port_type == PORT_DA && lane_connected == true)
        cmd->base.port = PORT_DA;
    else
        cmd->base.port = PORT_NONE;

    // Duplex is always set to TRUE
    cmd->base.duplex = true;

    // Set the supported and avertised AN setting
    linkmode_set_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, cmd->link_modes.supported);
    if (port_info->autoneg == true) 
    {
        CSMLOGDBG("autoneg is ON");
        cmd->base.autoneg = AUTONEG_ENABLE;

        if(port_info->sfp_port_type != PORT_FIBRE)
            linkmode_set_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, cmd->link_modes.advertising);
    }
    else
    {
        CSMLOGDBG("autoneg is OFF");
        cmd->base.autoneg = AUTONEG_DISABLE;
    }

    if (link_info->num_assigned_lanes == 0) 
    {
        cmd->base.speed = 0;
    }
    else
    {
        lane_speed = 0;
        for (i = 0; i < PHY_LANE_MAX; ++i) 
        {
            if ((port_info->lane_config[i].link_index == link_index) &&
                (port_info->lane_config[i].lane_enabled) &&
                (link_info->state == MTIP_LINK_STATE_UP))
            {
                lane_speed += mtip_platform_convert_lane_speed_to_gbps(port_info->lane_config[i].lane_speed);
            }
        }
        CSMLOGDBG("lane speed is %d", lane_speed);
        cmd->base.speed = lane_speed;
    }

    return 0;
}

int mtip_ethtool_set_link_ksettings(struct net_device *netdev, const struct ethtool_link_ksettings *cmd)
{
    struct mtip_netdev_priv *priv;
    u32 link_index;
    u32 real_link_number;
    struct mtip_link_info* link_info;
    u32 port_type;
    struct mtip_port_info* port_info;
    bool autoneg = false;
    u32 speed = 0;
    u32 priv_flags = 0;

    priv = netdev_priv(netdev);
    link_index = priv->link_index;

    CSMLOGINFO("ethtool: set_link_ksettings for link_index: %d, speed %d, autoneg %d, lanes %d\n",
               link_index, cmd->base.speed, cmd->base.autoneg, cmd->lanes);

    link_info = platform_driver_priv->mtip_links[link_index];

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return -EINVAL;
    }

    if (mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number) < 0)
    {
        CSMLOGERR("invalid link_index %d", link_index);
        return -EINVAL;
    }

    // Honor only for primary link of the port
    if((port_type != MTIP_PORT_TYPE_DEBUG && port_type != MTIP_PORT_TYPE_L2 && real_link_number != 0) ||
       (port_type == MTIP_PORT_TYPE_DEBUG && real_link_number != 1))
    {
        CSMLOGERR("Ignore for non primary link %d of the port %d",
                  real_link_number, port_type);
        return -EINVAL;
    }

    port_info = platform_driver_priv->mtip_ports[port_type];

    speed = cmd->base.speed;
    CSMLOGDBG("Speed for link_index %d set to %d", link_index, speed);
    if(port_type == MTIP_PORT_TYPE_DEBUG)
    {
        if(!check_if_valid_speed_for_debug_eth(speed))
        {
            CSMLOGERR("invalid speed for link_index %d", link_index);
            return -EINVAL;
        }

        if(cmd->lanes > 2)
        {
            CSMLOGERR("invalid lanes for link_index %d", link_index);
            return -EINVAL;
        }
    }

    // set link settings can be used to change autoneg to off/on
    if (cmd->base.autoneg == AUTONEG_DISABLE) 
    {
        CSMLOGDBG("setting autoneg OFF on port_type %d", port_type);
        autoneg = false;
    }
    else
    {
        CSMLOGDBG("setting autoneg ON on port_type %d", port_type);
        autoneg = true;
    }

    // Set the autoneg config
    if(port_info->autoneg != autoneg)
    {
        port_info->autoneg = autoneg;
        port_info->autoneg_changed = true;
    }

    /* Private flags will be set based on the new speed value specified in
       ethtool command. If the link is up, ethtool_link_ksettings will be
       fetched with get API and cmd->base.speed will be filled based on the
       current link speed.  */
    if(speed != 0)
    {
        if(speed == 10000)
        {
            if(link_index == MTIP_DEBUG_ETH_LINK_INDEX)
                priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_10G_ONLY_DBG_PORT;
	    else if(link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX)
                priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_10G_ONLY_L2_PORT;
            else
                priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_10G_ONLY;
        }
        else if(speed == 25000)
        {
            if(link_index == MTIP_DEBUG_ETH_LINK_INDEX)
                priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_25G_ONLY_DBG_PORT;
	    else if(link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX)
                priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_25G_ONLY_L2_PORT;
            else
                priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_25G_ONLY;
        }
        else if(speed == 40000)
            priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_40G_ONLY;
        else if(speed == 50000)
        {
            if(link_index == MTIP_DEBUG_ETH_LINK_INDEX)
                priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_50G_ONLY_DBG_PORT;
            if(link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX)
                priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_50G_ONLY_L2_PORT;
            else
                priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_50G_ONLY;
        }
        else if(speed == 100000)
        {
            if(link_index == MTIP_DEBUG_ETH_LINK_INDEX)
                priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_100G_ONLY_DBG_PORT;
	    else if(link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX)
                priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_100G_ONLY_L2_PORT;
            else
                priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_100G_ONLY;
        }
    }
    else
    {
        if(link_index == MTIP_DEBUG_ETH_LINK_INDEX)
            priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_DBG_PORT_NON_FEC_NON_50G;
        if(link_index == MTIP_L2_ETH_LINK_INDEX || link_index == MTIP_C2C1_ETH_LINK_INDEX)
            priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_L2_PORT_NON_FEC;
        else
            priv_flags = MTIP_DEVICE_PRIV_FLAGS_BIT_MASK_NON_FEC;
    }

    /* Restrict the number of lanes as specified in ethtool command */
    if(cmd->lanes == 1)
        priv_flags &= MTIP_DEVICE_PRIV_FLAGS_SINGLE_LANE_MASK;
    else if(cmd->lanes == 2)
        priv_flags &= MTIP_DEVICE_PRIV_FLAGS_TWO_LANES_MASK;
    else if(cmd->lanes == 4)
        priv_flags &= MTIP_DEVICE_PRIV_FLAGS_FOUR_LANES_MASK;

    // Set the priv flags for the speed config
    priv->priv_flags_set = true;
    priv->priv_flags = priv_flags;

    mtip_netdev_set_port_priv_flags(netdev);

    return 0;
}

static int mtip_ethtool_get_ts_info(struct net_device *ndev, struct ethtool_ts_info *info)
{
    CSMLOGDBG("ethtool: getting ts info\n");

	ethtool_op_get_ts_info(ndev, info);

	info->so_timestamping |=
            SOF_TIMESTAMPING_TX_HARDWARE |
			SOF_TIMESTAMPING_RX_HARDWARE |
			SOF_TIMESTAMPING_RAW_HARDWARE |
            SOF_TIMESTAMPING_SYS_HARDWARE;

	info->tx_types = BIT(HWTSTAMP_TX_OFF) |
			 BIT(HWTSTAMP_TX_ON);

	info->rx_filters = BIT(HWTSTAMP_FILTER_NONE);

	info->rx_filters |= BIT(HWTSTAMP_FILTER_PTP_V2_L4_EVENT) |
			    BIT(HWTSTAMP_FILTER_PTP_V2_L2_EVENT) |
			    BIT(HWTSTAMP_FILTER_PTP_V2_EVENT);

    info->phc_index = 0;
	return 0;
}

int mtip_ethtool_get_fecparam(struct net_device* netdev, struct ethtool_fecparam* pfec)
{
    u32 cmd = pfec->cmd;
    struct mtip_netdev_priv *priv;
    u32 link_index;

    priv = netdev_priv(netdev);
    link_index = priv->link_index;

    // set the config fec
    pfec->fec = platform_driver_priv->mtip_links[link_index]->config_fec;
    // set the active fec
    pfec->active_fec = platform_driver_priv->mtip_links[link_index]->active_fec;

    CSMLOGDBG("Getting FEC parameter for link index: %d, cmd: %d", link_index, cmd);

    return 0;
}

int mtip_ethtool_set_fecparam(struct net_device* netdev, struct ethtool_fecparam* pfec)
{
    u32 cmd = pfec->cmd;
    u32 active_fec = pfec->active_fec;
    u32 fec = pfec->fec;
    struct mtip_netdev_priv *priv;
    u32 link_index;
    u32 port_type;
    u32 real_link_number;

    priv = netdev_priv(netdev);
    link_index = priv->link_index;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return -1;
    }

    if (mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number) < 0)
    {
        CSMLOGERR("invalid link_index %d", link_index);
        return -EINVAL;
    }

    // Honor only for primary link of the port
    if((port_type != MTIP_PORT_TYPE_DEBUG && real_link_number != 0) ||
       (port_type == MTIP_PORT_TYPE_DEBUG && real_link_number != 1))
    {
        CSMLOGERR("Ignore for non primary link %d of the port %d",
                  real_link_number, port_type);
        return -EINVAL;
    }

    CSMLOGINFO("Setting FEC parameter for link index: %d, cmd: %d, active: %d, fec: %d", link_index, cmd, active_fec, fec);
    // set the configured fec
    platform_driver_priv->mtip_links[link_index]->config_fec = fec;

    if(fec == platform_driver_priv->mtip_links[link_index]->active_fec)
    {
        CSMLOGERR("FEC is already active\n");
        return 0;
    }

    post_mtip_process_reconfigure_port(port_type);

    return 0;
}


u32 port_config_to_priv_flags_mapping(u32 flags)
{
    u32 pattern = 0x1;
    u32 filtered_flags = 0;
    enum mtip_port_config_enum port_config = MTIP_PORT_CONFIG_1x100GBASE_R;
    u32 tmp_flags = flags;
    while(flags)
    {
        if(flags & pattern)
        {
            switch(port_config)
            {
                case MTIP_PORT_CONFIG_1x100GBASE_R2 :
                    filtered_flags |= pattern << MTIP_PRIV_FLAG_1x100GBASE_R2;
                    break;
                case MTIP_PORT_CONFIG_1x100GBASE_R4 :
                    filtered_flags |= pattern << MTIP_PRIV_FLAG_1x100GBASE_R4;
                    break;
                case MTIP_PORT_CONFIG_2x50GBASE_R :
                    filtered_flags |= pattern << MTIP_PRIV_FLAG_2x50GBASE_R;
                    break;
                case MTIP_PORT_CONFIG_1x50GBASE_R :
                    filtered_flags |= pattern << MTIP_PRIV_FLAG_1x50GBASE_R;
                    break;
                case MTIP_PORT_CONFIG_1x50GBASE_R2:
                    filtered_flags |= pattern << MTIP_PRIV_FLAG_1x50GBASE_R2;
                    break;
                case MTIP_PORT_CONFIG_1x40GBASE_R4:
                    filtered_flags |= pattern << MTIP_PRIV_FLAG_1x40GBASE_R4;
                    break;
                case MTIP_PORT_CONFIG_4x25GBASE_R:
                    filtered_flags |= pattern << MTIP_PRIV_FLAG_4x25GBASE_R;
                    break;
                case MTIP_PORT_CONFIG_1x25GBASE_R:
                    filtered_flags |= pattern << MTIP_PRIV_FLAG_1x25GBASE_R;
                    break;
                case MTIP_PORT_CONFIG_4x10GBASE_R:
                    filtered_flags |= pattern << MTIP_PRIV_FLAG_4x10GBASE_R;
                    break;
                case MTIP_PORT_CONFIG_1x10GBASE_R:
                    filtered_flags |= pattern << MTIP_PRIV_FLAG_1x10GBASE_R;
                    break;
                default:
                    CSMLOGERR("Invalid private flag was set: %x\n", tmp_flags);
            }
        }
        flags = flags >> 1;
        port_config++;
    }
    CSMLOGDBG("final filtered_flags: %x\n", filtered_flags);
    return filtered_flags;
}

u32 mtip_ethtool_get_priv_flags(struct net_device *netdev)
{
    struct mtip_netdev_priv *priv;
    u32 link_index;
    u32 port_type;
    u32 port_link0_index;
    u32 filtered_flags=0;

    priv = netdev_priv(netdev);
    link_index = priv->link_index;

    CSMLOGDBG("Get priv called for link index: %d, priv->priv_flags: 0x%x\n", link_index, priv->priv_flags);

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return -1;
    }

    // Process only for link 0 of the port
    port_link0_index = platform_driver_priv->devices.port_devices[port_type].link_devices[0]->link_index;
    if (link_index != port_link0_index) 
    {
        CSMLOGDBG("ignoring the default priv flags of link_index %d", link_index);
    }

    priv = netdev_priv(platform_driver_priv->mtip_links[port_link0_index]->dev);

    // return flags currently enabled
    filtered_flags = port_config_to_priv_flags_mapping(priv->priv_flags);

    return filtered_flags;
}

u32 priv_flags_to_port_config_mapping(u32 flags)
{
    u32 pattern = 0x1;
    u32 filtered_flags = 0;
    enum mtip_port_config_enum port_config = MTIP_PORT_CONFIG_1x100GBASE_R;
    u32 pflags = flags;
    while(pflags)
    {
        if(pflags&pattern)
        {
            switch(port_config)
            {
                case MTIP_PRIV_FLAG_1x100GBASE_R2 :
                    filtered_flags |= pattern << MTIP_PORT_CONFIG_1x100GBASE_R2;
                    break;
                case MTIP_PRIV_FLAG_1x100GBASE_R4 :
                    filtered_flags |= pattern << MTIP_PORT_CONFIG_1x100GBASE_R4;
                    break;
                case MTIP_PRIV_FLAG_2x50GBASE_R :
                    filtered_flags |= pattern << MTIP_PORT_CONFIG_2x50GBASE_R;
                    break;
                case MTIP_PRIV_FLAG_1x50GBASE_R :
                    filtered_flags |= pattern << MTIP_PORT_CONFIG_1x50GBASE_R;
                    break;
                case MTIP_PRIV_FLAG_1x50GBASE_R2:
                    filtered_flags |= pattern << MTIP_PORT_CONFIG_1x50GBASE_R2;
                    break;
                case MTIP_PRIV_FLAG_1x40GBASE_R4:
                    filtered_flags |= pattern << MTIP_PORT_CONFIG_1x40GBASE_R4;
                    break;
                case MTIP_PRIV_FLAG_4x25GBASE_R:
                    filtered_flags |= pattern << MTIP_PORT_CONFIG_4x25GBASE_R;
                    break;
                case MTIP_PRIV_FLAG_1x25GBASE_R:
                    filtered_flags |= pattern << MTIP_PORT_CONFIG_1x25GBASE_R;
                    break;
                case MTIP_PRIV_FLAG_4x10GBASE_R:
                    filtered_flags |= pattern << MTIP_PORT_CONFIG_4x10GBASE_R;
                    break;
                case MTIP_PRIV_FLAG_1x10GBASE_R:
                    filtered_flags |= pattern << MTIP_PORT_CONFIG_1x10GBASE_R;
                    break;
                default:
                    CSMLOGERR("Invalid private flag set: %x\n", flags);
            }
        }
        pflags = pflags >> 1;
        port_config++;
    }
    CSMLOGINFO("final filtered_flags: %x\n", filtered_flags);
    return filtered_flags;
}

int mtip_ethtool_set_priv_flags(struct net_device *netdev, u32 flags)
{
    struct mtip_netdev_priv *priv;
    u32 link_index;
    int temp_flag = flags;
    enum mtip_port_config_enum port_config = MTIP_PORT_CONFIG_1x100GBASE_R;
    u32 real_link_number;
    u32 port_type;
    int filtered_flags=0;
    priv = netdev_priv(netdev);
    link_index = priv->link_index;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return -EINVAL;
    }

    if (mtip_lookup_real_link_number_by_link_index(link_index, &real_link_number) < 0)
    {
        CSMLOGERR("invalid link_index %d", link_index);
        return -EINVAL;
    }

    // Honor only for primary link of the port
    if((port_type != MTIP_PORT_TYPE_DEBUG && real_link_number != 0) ||
       (port_type == MTIP_PORT_TYPE_DEBUG && real_link_number != 1))
    {
        CSMLOGERR("Ignore for non primary link %d of the port %d",
                  real_link_number, port_type);
        return -EINVAL;
    }

    CSMLOGDBG("Set priv called for link index: %d with flags: 0x%x", link_index, flags);

    //checking for valid port config for debugeth
    if(link_index == MTIP_DEBUG_ETH_LINK_INDEX)
    {
        //extracting which bits of flag are set
        while (temp_flag)
        {
            if((temp_flag & 0x1) &&
               check_if_valid_port_config_for_debug_eth(port_config) == false)
            {
                CSMLOGERR("Invalid port config %d for Debug ETH", port_config);
                return -EINVAL;
            }

            temp_flag >>= 1;
            port_config++;
        }
    }
    filtered_flags = priv_flags_to_port_config_mapping(flags);
    // mark that priv flags have been set using ethtool
    priv->priv_flags_set = true;
    priv->priv_flags = filtered_flags;

    mtip_netdev_set_port_priv_flags(netdev);
    return 0;
}

void mtip_ethtool_set_msglevel(struct net_device *netdev, u32 level)
{
    int i;
    u32 link_index;
    u32 lane_index;
    u32 port_type;
    struct mtip_netdev_priv *priv;
    struct qsfp_info trx_info = {0};
    struct mtip_process_lane_up lane_up_info = {0};
    struct mtip_process_lane_down lane_down_info = {0};

    priv = netdev_priv(netdev);
    link_index = priv->link_index;

    // check if the corresponding port is in LINK_UP state
    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid link_index: %d", link_index);
    }
    else
    {
        CSMLOGINFO("Set msglvl for link_index: %d, port_type: %d", link_index, port_type);
    }

    trx_info.trx_module_type = TRX_QSFP_PLS_QSFP28_QSFP56;
    trx_info.speed_mask = TRX_LANE_SPEED_10G | TRX_LANE_SPEED_25G | TRX_LANE_SPEED_50G | TRX_LANE_SPEED_100G;
    trx_info.trx_laneinfo = 0xF;
    trx_info.trx_bout_cfg = 0;

    switch (level)
    {
    case 0:
        {
            // call handle lane down for all lanes of the port
            for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_lane_phandles; ++i)
            {
                if (platform_driver_priv->devices.port_devices[port_type].lane_devices[i] != NULL) 
                {
                    lane_index = platform_driver_priv->devices.port_devices[port_type].lane_devices[i]->lane_index;

                    memcpy(&platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info, &trx_info, sizeof(struct qsfp_info));
                    platform_driver_priv->devices.lane_devices[lane_index].reason_code = TRX_LOCAL_PLUGOUT;

                    lane_down_info.lane_index = lane_index;
                    lane_down_info.reason_code = TRX_LOCAL_PLUGOUT;
                    post_mtip_phy_handle_lane_down(lane_down_info);
                }
                else
                {
                    CSMLOGERR("lane device[%d] is NULL for port_type: %d link_index: %d", i, port_type, link_index);
                }
            }
        }
        break;

    case 1:
        {
            // call handle lane up for all lanes of the port with DAC
            for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_lane_phandles; ++i)
            {
                if (platform_driver_priv->devices.port_devices[port_type].lane_devices[i] != NULL) 
                {
                    lane_index = platform_driver_priv->devices.port_devices[port_type].lane_devices[i]->lane_index;

                    memcpy(&platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info, &trx_info, sizeof(struct qsfp_info));

                    lane_up_info.lane_index = lane_index;
                    lane_up_info.sfp_port_type = PORT_DA;
                    lane_up_info.speed_mask = TRX_LANE_SPEED_10G | TRX_LANE_SPEED_25G | TRX_LANE_SPEED_50G | TRX_LANE_SPEED_100G;
                    lane_up_info.lane_connected = true;
                    post_mtip_phy_handle_lane_up(lane_up_info);
                }
                else
                {
                    CSMLOGERR("lane device[%d] is NULL for port_type: %d link_index: %d", i, port_type, link_index);
                }
            }
        }
        break;

    case 2:
        {
            // call handle lane up for all lanes of the port with FIBER
            for (i = 0; i < platform_driver_priv->devices.port_devices[port_type].num_lane_phandles; ++i)
            {
                if (platform_driver_priv->devices.port_devices[port_type].lane_devices[i] != NULL) 
                {
                    lane_index = platform_driver_priv->devices.port_devices[port_type].lane_devices[i]->lane_index;

                    memcpy(&platform_driver_priv->mtip_lanes[lane_index]->lane_qsfp_info, &trx_info, sizeof(struct qsfp_info));

                    lane_up_info.lane_index = lane_index;
                    lane_up_info.sfp_port_type = PORT_FIBRE;
                    lane_up_info.speed_mask = TRX_LANE_SPEED_10G | TRX_LANE_SPEED_25G | TRX_LANE_SPEED_50G | TRX_LANE_SPEED_100G;
                    lane_up_info.lane_connected = true;
                    post_mtip_phy_handle_lane_up(lane_up_info);
                }
                else
                {
                    CSMLOGERR("lane device[%d] is NULL for port_type: %d link_index: %d", i, port_type, link_index);
                }
            }
        }
        break;

    case 3:
        {
            // print the information about the platform
            mtip_platform_print_platform();
        }
        break;

    case 4:
        {
            // print the information about the topology
            mtip_print_topology(platform_driver_priv->topology);
        }
        break;

    case 5:
        {
            mtip_ethtool_debug_logging_enable = 1;
        }
        break;

    case 6:
        {
           mtip_ethtool_debug_logging_enable = 0;
        }
        break;

    case 7:
        {
            // print the information about the ports
            mtip_platform_print_devices();
        }
        break;

    case 8:
        {
            // print the information about the ports
            mtip_platform_print_ports();
        }
        break;

    case 9:
        {
            // print the information about the links
            mtip_platform_print_links();
        }
        break;

    case 10:
        {
            // print the information about the lanes
            mtip_platform_print_lanes();
        }
        break;

    case 11:
        {
            // Set TX compliance to disable retry attempts for PHY lane bring up
            mtip_phy_set_tx_compliance(true);
        }
        break;

    case 12:
        {
            // Unset TX compliance to enable retry attempts for PHY lane bring up
            mtip_phy_set_tx_compliance(false);
        }
        break;

    case 13:
        {
            mtip_ethtool_ptp_logging_enable = 1;
        }
        break;

    case 14:
        {
            mtip_ethtool_ptp_logging_enable = 0;
        }
        break;

    default:
        {
            CSMLOGINFO("Ignoring msglevel %d for link index: %d", level, link_index);
        }
        break;
    }
}

u32 mtip_ethtool_get_msglevel(struct net_device *netdev)
{
    u32 link_index;
    u32 port_type;
    struct mtip_netdev_priv *priv;

    priv = netdev_priv(netdev);
    link_index = priv->link_index;

    // check if the corresponding port is in LINK_UP state
    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return 0;
    }

    return platform_driver_priv->mtip_ports[port_type]->port_state;
}

int mtip_ethtool_get_module_info(struct net_device *netdev,
                                                struct ethtool_modinfo *modinfo)
{
    u32 link_index;
    u32 port_type;
    u32 lane_index;
    struct mtip_netdev_priv *priv;
    int i;
    u32 sfp_phandle;

    priv = netdev_priv(netdev);
    link_index = priv->link_index;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return 0;
    }

   for (i = 0; i < PHY_LANE_MAX; ++i)
   {
      if(mtip_lookup_lane_index_by_port_type_and_real_lane(&lane_index, port_type, i) == 0)
      {
         sfp_phandle = platform_driver_priv->devices.lane_devices[lane_index].sfp_phandle;
         return qsfp_trx_get_module_info(sfp_phandle, modinfo);
      }
   }

   return -EINVAL;
}
 
int mtip_ethtool_get_module_eeprom(struct net_device *netdev,
                                                      struct ethtool_eeprom *ee,
                                                      u8 *data)
{

    u32 link_index;
    u32 port_type;
    u32 lane_index;
    struct mtip_netdev_priv *priv;
    int i;
    u32 sfp_phandle;

    priv = netdev_priv(netdev);
    link_index = priv->link_index;

    if (mtip_lookup_port_type_by_link_index(link_index, &port_type) < 0)
    {
        CSMLOGERR("invalid port_type for link_index %d", link_index);
        return 0;
    }

   for (i = 0; i < PHY_LANE_MAX; ++i)
   {
      if(mtip_lookup_lane_index_by_port_type_and_real_lane(&lane_index, port_type, i) == 0)
      {
         sfp_phandle = platform_driver_priv->devices.lane_devices[lane_index].sfp_phandle;
         return qsfp_trx_get_module_eeprom(sfp_phandle, ee, data);
      }
   }

   return -EINVAL;
}

static const struct ethtool_ops mtip_ethtool_ops = {
   .get_drvinfo = mtip_ethtool_getdrvinfo,
   .get_regs = mtip_ethtool_get_regs,
   .get_regs_len = mtip_ethtool_get_regs_len,
   .get_sset_count  = mtip_ethtool_get_sset_count,
   .get_strings = mtip_ethtool_get_strings,
   .get_ethtool_stats = mtip_ethtool_get_stats,
   .get_ts_info = mtip_ethtool_get_ts_info,
   .get_link_ksettings = mtip_ethtool_get_link_ksettings,
   .set_link_ksettings = mtip_ethtool_set_link_ksettings,
   .get_priv_flags = mtip_ethtool_get_priv_flags,
   .set_priv_flags = mtip_ethtool_set_priv_flags,
   .get_fecparam = mtip_ethtool_get_fecparam,
   .set_fecparam = mtip_ethtool_set_fecparam,
   .set_msglevel = mtip_ethtool_set_msglevel,
   .get_msglevel = mtip_ethtool_get_msglevel,
   .get_link = ethtool_op_get_link,
   .get_module_info = mtip_ethtool_get_module_info,
   .get_module_eeprom = mtip_ethtool_get_module_eeprom,
};

void mtip_ethtool_set_ops(struct net_device *netdev)
{
  struct mtip_netdev_priv* priv;
  u32 link_index;

   CSMLOGDBG("Setting ethtool ops for netdev 0x%lx\n", (unsigned long)netdev);

   priv = netdev_priv(netdev);
   link_index = priv->link_index;

   netdev->ethtool_ops = &mtip_ethtool_ops;
}

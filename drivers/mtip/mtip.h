//SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MTIP_H
#define _MTIP_H

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/netdevice.h>

#include "ecpri_dma_eth.h"
#include "ecpri_dma.h"
#include "eth_ecpriss_iface.h"
#include "macsec_eth.h"

#define DRV_NAME "csm-mtip"

#include "mtip_logging.h"
#include "eth_phy_iface.h"
#include "transceiver_api.h"
#include "mtip_ptp.h"
#include "mtip_client.h"
#include "mtip_clocks.h"
#include "mtip_dma.h"

#ifndef NO_DEBUGFS_PERF
#define FEATURE_MTIP_TEST_DEBUG_FS
#endif

// the driver name
#define MTIP_MAC_DRIVER                 "MTIP_MAC"
#define MTIP_MAC_DRIVER_VERSION         "1.0.0"

// Common feature definitions

/* if loopback is enabled then SWAP_HANDLE will swap the handle of Rx/Tx interfaces */
#define MTIP_LOOPBACK_SWAP_HANDLE

/* Dump the contents of Tx and Rx skbs */
//#define MTIP_DUMP_PACKETS

/* Setting the modes of the driver */
#define MTIP_MODE_DEFAULT          0   // This is the default mode with no loopback
#define MTIP_MODE_LOOPBACK         1   // This is the loopback mode. For RUMI, this is IOMACRO loopback and for SOC it is PCS LOOPBACK
#define MTIP_MODE_PHY_LOOPBACK     2   // This is the PHY loopback mode for SOC. Not applicable for RUMI
#define MTIP_MODE_C2C2_LOOPBACK    1   // This is loopback mode for C2C2 port. Not applicable for RUMI

/* Setting the platform RUMI/SOC */
#define MTIP_PLATFORM_SOC          0
#define MTIP_PLATFORM_RUMI         1

/* System Configuration */
#define MTIP_MAX_PORTS  7          // Max of 7 ports allowed in the system
#define MTIP_MAX_LINKS  16         // Setup for a max of 16 Ethernet links
#define MTIP_MAX_LANES  20         // Setup for a max of 16 lanes

#define MTIP_MAX_FH_PORTS  3       // Max of 3 FH ports

#define MTIP_MAX_LINKS_PER_PORT 4  // Max of 4 links per port
#define MTIP_MAX_LANES_PER_PORT 4  // Max of 4 lanes per port

/* Networking Stack Configuration */
#define MTIP_TIMEOUT     5 /* in Jiffies*/
#define MTIP_NAPI_WEIGHT 64 /* Initial NAPI weight */

// the range of supported MTU sizes
#define MTIP_MIN_MTU_SIZE  68
#define MTIP_MAX_MTU_SIZE  65535

// the max size of MTU for M Plane
#define MTIP_MAX_MPLANE_MTU_SIZE 9198

#define MTIP_DMA_RX_BUFF_SIZE 2500 /* use buffers of size 2500 */

extern int mtip_dma_max_rx_buff_size;
extern bool enable_tx_comp_poll;

#define MTIP_RX_DMA_MAX_BUFFERS_PER_PACKET   4

#define MTIP_TX_RING_SIZE 2048
#define MTIP_RX_RING_SIZE 2048

#define MTIP_TX_MOD_COUNTER_THRESHOLD 32
#define MTIP_TX_MOD_TIMER_THRESHOLD   10

#define MTIP_TX_PACKET_AVAILABILITY_THRESHOLD     50

#define MTIP_TSC_OFFSET_VAL          0x00000013  // TSC OFFSET REGISTER VALUE TO BE SET AFTER BRING UP IS COMPLETE
#define MTIP_DEBUG_ETH_LINK_INDEX     15
#define MTIP_L2_ETH_LINK_INDEX     12
#define MTIP_C2C1_ETH_LINK_INDEX   13
#define MTIP_C2C0_ETH_LINK_INDEX   14
#define ETHTOOL_STAT_STRINGS_LEN 15
#define DEBUG_ETHTOOL_STAT_STRINGS_LEN 23
#define STATS_NAME_LEN 20
#define QXDM_LOGGING_VAR_NA 255

#define MTIP_L2_LANE1_INDEX     12
#define MTIP_L2_LANE2_INDEX     13
#define MTIP_C2C1_LANE1_INDEX   14
#define MTIP_C2C1_LANE2_INDEX   15
// Maximum number of retries with speed mode change for dual rate optics module
#define MTIP_NEXT_SPEED_MODE_RETRY_MAX_COUNT 2

/*
 * Information related to the devices in the device tree
 */
// the information stored for each link device
struct mtip_link_device_info
{
    // link device valid
    u8 link_device_valid;

    // the lane device platform device
    struct platform_device* link_pdev;

    // the link phandle
    u32 link_phandle;

    // link indices
    u32 link_index;

    // link base mac register address
    void __iomem           *mac_ioaddr;

    // link base pcs register address
    void __iomem           *pcs_ioaddr;

    // store the name of the interface
    const char*            link_name;
};

// the information stored for each lane device
struct mtip_lane_device_info
{
    // lane device valid
    u8 lane_device_valid;

    // the lane device platform device
    struct platform_device* lane_pdev;

    // the lane phandle
    u32 lane_phandle;

    // lane index
    u32 lane_index;

    // sfp phandle
    int sfp_phandle;

    // lane down reason
    trx_lane_down_reason_code_type reason_code;

};

// the information stored for each port device
struct mtip_port_device_info
{
    // port device valid
    u8 port_device_valid;

    // the port device platform device
    struct platform_device* port_pdev;

    // the phandle of the port device
    u32 port_phandle;

   // the port type
   u32 port_type;

   // the base address for the MAC Wrapper
   void __iomem          *wrapper_base_addr;

   // the base address for the MACSTATS CONTROL
   void __iomem          *macstats_base_addr;

   // the base address for the RSFEC
   void __iomem          *rsfec_base_addr;

   // the irq for MAC wrapper
   u32 wrapper_irq;

   // Base address for emulation DUT(FH instance)
   void __iomem          *dut_base_addr;

   // the references to links of the port
   u32 num_link_phandles;
   u32 link_phandles[MTIP_MAX_LINKS_PER_PORT];

   // the references to lanes of the port
   u32 num_lane_phandles;
   u32 lane_phandles[MTIP_MAX_LANES_PER_PORT];

   // the link devices
   struct mtip_link_device_info *link_devices[MTIP_MAX_LINKS_PER_PORT];

   // the lane devices
   struct mtip_lane_device_info *lane_devices[MTIP_MAX_LANES_PER_PORT];
};

/*
 * The root device that contains all the other devices
 */
struct mtip_devices_info
{
    // platform device valid
    u8 platform_device_valid;

    // flag to indicate platform setup is complete
    u8 platform_setup_complete;

    // the root platform device
    struct platform_device* root_pdev;

    // device mode
    enum mtip_device_mode_enum mode;

    // the base address for the MAC Address fuses
    void __iomem          *fuse_base_addr;

    // the bit offset at the fuse address where the MAC address starts
    u8  fuse_bit_offset;

    // the port phandles
    u32 num_port_phandles;
    u32 port_phandles[MTIP_MAX_PORTS];

    // the port devices
    struct mtip_port_device_info port_devices[MTIP_MAX_PORTS];

    // the link devices
    struct mtip_link_device_info link_devices[MTIP_MAX_LINKS];

    // the lane devices
    struct mtip_lane_device_info lane_devices[MTIP_MAX_LANES];
};

struct mtip_pkt_priv
{
	struct sk_buff *skb;
	uint16_t tx_index;
};

/*
 * information stored for each active link including netdev struct etc
 */
/*
 * mtip_link_state enum
 */
enum mtip_link_state_enum
{
   MTIP_LINK_STATE_INIT = 0,
   MTIP_LINK_STATE_OPEN_WAITING_FOR_LANES,
   MTIP_LINK_STATE_CLOSE,
   MTIP_LINK_STATE_UP,
   MTIP_LINK_STATE_DOWN,
   MTIP_LINK_STATE_OPEN_DONE,
   MTIP_LINK_STATE_OPEN_FAILED,
   MTIP_LINK_STATE_MAX
};

//information related to ethtool stats of each link
struct eth_stats
{
    char stats_name[STATS_NAME_LEN];
    u64 stats_value;

};


// information relevant to each link
struct mtip_link_info
{
    // the link index
    u32 link_index;

   // the link state
   enum mtip_link_state_enum state;

   // the dma hdl
   ecpri_dma_eth_conn_hdl_t dma_hdl;

   // the link netdev info
   struct net_device* dev;
   struct napi_struct napi;
   struct napi_struct napi_tx;
   struct rtnl_link_stats64 net_stats;
   bool ptp_ts_enabled;
   u8 ptp_ts_seq_num;
   struct mtip_tx_ts_list tx_ts_list;
   struct mtip_tx_ts_skb_list tx_ts_skb_list;
   struct mtip_tx_comp_list tx_comp_list;
   struct mtip_ptp_time_stamp_info tstamp_info;
   u32 peak_rx_available;
   u32 config_fec;
   u32 active_fec;
   spinlock_t ptp_lock;
   unsigned long flags;

   // Per-interface loopback flag for A55 TX blocking and promiscuous mode
   bool loopback_enabled;

   // link lane assignments
   // lanes assignment complete
   bool lanes_assignment_complete;

   // the number of assigned lanes
   u32  num_assigned_lanes;

   // these are lane_indices assigned to the link
   u32  assigned_lane_indices[PHY_LANE_MAX];

   struct mutex dev_lock;
   struct eth_stats stats[DEBUG_ETHTOOL_STAT_STRINGS_LEN];

   // Timer to retry lane bring up in case of failure
   struct timer_list phy_retry_timer;
   bool phy_retry_timer_valid;

   /* Flag to track if link down came post link was up. This will help to decide
      if RX LOS processing is needed or not for speed mode change of dual rate
      optics modules */
   bool link_down_received_post_link_up;

   // Timer to retry RX buffer replenish with NAPI poll in case of mem alloc failure
   struct timer_list rx_replenish_retry_timer;

   // Defer timer to check the validity of PCS link state
   struct timer_list pcs_link_up_defer_timer;
   bool pcs_link_up_defer_timer_running;
};

/*
 * mtip_lane_state enum
 */
enum mtip_lane_state_enum
{
   MTIP_LANE_STATE_INIT = 0,
   MTIP_LANE_STATE_CONNECTED,
   MTIP_LANE_STATE_DISCONNECTED,
   MTIP_LANE_STATE_MAX
};

// information relevant to each port
struct mtip_lane_info
{
    // the lane index
    u32 lane_index;

    // the lane state
    enum mtip_lane_state_enum lane_state;

    // the lane information from qsfp driver
    struct qsfp_info lane_qsfp_info;

    // sfp port type
    u32                sfp_port_type;

    // Mask of lane speeds supported by the module
    u8  speed_mask;

    // the phylink related to the lane
    struct phylink         *phylink;
    struct phylink_config   phylink_config;

    // dummy ndev for the lane
    struct net_device* lane_dummy_ndev;

    spinlock_t lock;

    bool rx_los_set;
};

/*
 * mtip_port_state enum
 */
enum mtip_port_state_enum
{
   MTIP_PORT_STATE_INIT = 0,
   MTIP_PORT_STATE_CONNECTED,
   MTIP_PORT_STATE_CONNECTED_INITIATE_AN,
   MTIP_PORT_STATE_CONNECTED_NEGOTIATION_IN_PROGRESS,
   MTIP_PORT_STATE_CONNECTED_NEGOTIATION_DONE,
   MTIP_PORT_STATE_DISCONNECTED,
   MTIP_PORT_STATE_MAX
};

// information relevant to each port
struct mtip_port_info
{
    // the port_type
    u32 port_type;

    // the consolidated port state
    // what is the port state?
    enum mtip_port_state_enum port_state;

    // the consolidate priv flags of all links of port
    u32 port_priv_flags;

    // the port priv flags of FIBRE on which retries need to be made
    u32 port_priv_flags_optical;

    // Number of speed switch retries for dual rate modules
    u8 next_speed_retry_count;

    // autoneg flag to see if autoneg is enabled
    bool autoneg;

    // flag to see if autoneg setting was modified
    bool autoneg_changed;

    // the negotiated port configuration
    enum mtip_port_config_enum port_config;

    // consolidate port lane configuration
    struct eth_phy_iface_phy_lane_config lane_config[PHY_LANE_MAX];

    // the sfp port type
    u32  sfp_port_type;

    spinlock_t lock;

    // Flag to track if this a dual/multi rate optical module
    bool multi_rate_supported;

    // Flag to track if port reconfiguration is needed after RX LOS is cleared
    bool needs_rx_los_processing;
};

// platform struct private
struct mtip_platform_driver_priv
{
   // the platform driver error status
   int perr;

   // devices information
   // this gets filled up as device probes are received
   struct mtip_devices_info devices;

   // store the clocks here
   struct mtip_clocks_clock_struct clocks;

   // information stored for each active link including netdev structs etc
   struct mtip_link_info* mtip_links[MTIP_MAX_LINKS];

   // information stored for each active lane
   struct mtip_lane_info* mtip_lanes[MTIP_MAX_LANES];

   // information stored for each active port
   struct mtip_port_info* mtip_ports[MTIP_MAX_PORTS];

   // dma ready state
   bool dma_is_ready;

   // phy ready state
   bool phy_is_ready;

	// the system topology
	eth_ecpriss_topology_root_s* topology;

    // macsec ops
    struct macsec_ops* security_macsec_ops;

   // the clients registered
   struct mtip_client_struct clients[MTIP_MAX_CLIENTS];

    // the spin lock to access driver priv
    spinlock_t driver_lock;

    // IPC log buffer
    void* ipc_log_buf;
    void* ipc_ptp_log_buf;

    // IPC log buffer low
    void* ipc_log_buf_low;

    // IPC log buffer dbg
    void* ipc_log_buf_dbg;

    // array of lists of dma allocations
    // static array of lists for allocations
    struct mtip_dma_alloc_struct mtip_dma_alloc_array[MTIP_DMA_ALLOC_LIST_MAX];

    // MSG5 time stamp
    struct mtip_time_stamp msg5_time_stamp;

    //MSG5 SKB
    struct sk_buff *msg5_skb_buff;
};

// extern declarations
extern struct mtip_platform_driver_priv* platform_driver_priv;

// this is the extern controlling loopback mode
extern int mtip_loopback_mode;
extern int mtip_c2c2_loopback_mode;

extern bool mtip_loopback_swap_addr;
extern bool mtip_loopback_enable_arp;

// this is the extern to set if we are running on a RUMI
extern int mtip_rumi_platform;

// this is to control debug level logging
extern int mtip_ethtool_debug_logging_enable;

// this is to control ptp level logging
extern int mtip_ethtool_ptp_logging_enable;

// function prototypes
int mtip_register_platform_driver(void);

/*
 * mtip_lookup_link_index_by_name 
 *  find the link index using the interface name
 *  rarely used
 */
int mtip_lookup_link_index_by_name(char* name, u32* link_index);

/*
 * mtip_lookup_link_index_by_handle 
 *  find the link index using the dma hdl
 *  also rarely used
 */
int mtip_lookup_link_index_by_handle(ecpri_dma_eth_conn_hdl_t hdl, u32* link_index);

/*
 * mtip_lookup_link_index_by_port_type_and_real_link 
 *  find the link index using port type and real link number (between 0 and 3) 
 */
int mtip_lookup_link_index_by_port_type_and_real_link(u32* link_index, u32 port_type, u32 real_link_number);

/*
 * mtip_lookup_port_type_by_link_index 
 *   find port_type of the link index 
 */
int mtip_lookup_port_type_by_link_index(u32 link_index, u32* port_type);

/*
 * mtip_lookup_real_link_number_by_link_index 
 *   find the real link number (between 0 and 3) within the port of the link index 
 */
int mtip_lookup_real_link_number_by_link_index(u32 link_index, u32* real_link_number);

/*
 * mtip_lookup_link_index_by_device 
 *  find the link index using port type and link device index (0 thru num_link_phandles - 1) 
 */
int mtip_lookup_link_index_by_device(u32* link_index, u32 port_type, u32 link_device_index);

/*
 * mtip_lookup_device_by_link_index 
 *   find the device within a port by matching the phandle 
 */
int mtip_lookup_device_by_link_index(u32 link_index, u32* port_device_index, u32* link_device_index);

/*
 * mtip_lookup_lane_index_by_port_type_and_real_link 
 *  find the lane index using port type and real lane number (between 0 and 3) 
 */
int mtip_lookup_lane_index_by_port_type_and_real_lane(u32* lane_index, u32 port_type, u32 real_lane_number);

/*
 * mtip_lookup_port_type_by_lane_index 
 *   find port_type of the lane index 
 */
int mtip_lookup_port_type_by_lane_index(u32 lane_index, u32* port_type);

/*
 * mtip_lookup_real_lane_number_by_lane_index 
 *   find the real lane number (between 0 and 3) within the port of the lane index 
 */
int mtip_lookup_real_lane_number_by_lane_index(u32 lane_index, u32* real_lane_number);

/*
 * mtip_lookup_lane_index_by_device 
 *  find the lane index using port type and lane device index (0 thru num_lane_phandles - 1) 
 */
int mtip_lookup_lane_index_by_device(u32* lane_index, u32 port_type, u32 lane_device_index);

/*
 * mtip_lookup_device_by_lane_index 
 *   find the device within a port by matching the phandle 
 */
int mtip_lookup_device_by_lane_index(u32 lane_index, u32* port_device_index, u32* lane_device_index);

/*
 * mtip_lookup_if_any_link_active_for_port 
 *   checks whether there is any active link other than the given link for the given port type
 */
bool mtip_lookup_if_any_other_link_active_for_port(u32 port_type, u32 link_index);

/*
 * mtip_lookup_link_index_by_lane_index 
 *  find the link on which the given lane is mapped to 
 */
int mtip_lookup_link_index_by_lane_index(u32 *link_index, u32 lane_index);

ssize_t mtip_show_link_polling_timer(
                struct kobject *kobj, struct kobj_attribute *attr, char *buf);

ssize_t mtip_store_link_polling_timer(
                              struct kobject *kobj, struct kobj_attribute *attr,
                              const char *buf, size_t count);

/* Simplified loopback work structure */
struct mtip_loopback_config_task {
    u32 link_index;
    bool enable;
};

/* Loopback interface functions */
bool mtip_is_link_in_loopback(u32 link_index);
int mtip_phy_set_loopback_mode(u32 link_index, enum qcom_aw_phy_loopback_mode_enum loopback_mode);
void post_mtip_process_loopback_config(u32 link_index, bool enable);
void run_mtip_process_loopback_config(void *work_ptr);
int setup_interface_in_loopback_mode(struct net_device *netdev, u32 link_index);

#endif // _MTIP_H

/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ECPRI_ETH_H
#define ECPRI_ETH_H

#define ECPRISS_MAC_ADDR_LEN        6
#define ECPRISS_MAX_PORTS           3
#define ECPRISS_MAX_UNIQUE_PORT     3
#define ECPRISS_MAX_LINKS           4


typedef void (*eth_ecpriss_topology_ready_cb)(void);

/**
 * enum dev mode
 */
typedef enum {
	ETH_ECPRISS_DEV_MODE_DU = 0,
	ETH_ECPRISS_DEV_MODE_RU,
	ETH_ECPRISS_DEV_MODE_MAX
} eth_ecpriss_dev_mode_e;

/**
 * enum eth_ecpriss_event_e - event type
 */
typedef enum {
	ETH_ECPRISS_EVENT_UP = 0,
	ETH_ECPRISS_EVENT_DOWN,
	ETH_ECPRISS_EVENT_DESTROY,
	ETH_ECPRISS_EVENT_MAX
} eth_ecpriss_event_e;

/**
 * enum eth_ecpriss_status_e - status type
 */
typedef enum {
	ETH_ECPRISS_STATUS_SUCCESS = 0,
	ETH_ECPRISS_STATUS_FAILURE,
	ETH_ECPRISS_STATUS_MAX
} eth_ecpriss_status_e;

/**
 * enum eth_ecpriss_port_type - port type
 */
typedef enum {
	ETH_ECPRISS_PORT_TYPE_FH = 0,
	ETH_ECPRISS_PORT_TYPE_C2C,
	ETH_ECPRISS_PORT_TYPE_L2,
	ETH_ECPRISS_PORT_TYPE_MAX,
} eth_ecpriss_port_type_e;

/**
 * enum eth_ecpriss_link_type - link type
 */
typedef enum {
    ETH_ECPRISS_LINK_RATE_10 = 0,
    ETH_ECPRISS_LINK_RATE_25,
    ETH_ECPRISS_LINK_RATE_40,
    ETH_ECPRISS_LINK_RATE_50,
    ETH_ECPRISS_LINK_RATE_100,
    ETH_ECPRISS_LINK_RATE_MAX
} eth_ecpriss_link_rate_e;

/**
 * enum eth_ecpriss_link_state - link state
 */
typedef enum {
    ETH_ECPRISS_LINK_STATE_INIT = 0,
    ETH_ECPRISS_LINK_STATE_OPEN,
    ETH_ECPRISS_LINK_STATE_CLOSE,
    ETH_ECPRISS_LINK_STATE_UP,
    ETH_ECPRISS_LINK_STATE_DOWN,
    ETH_ECPRISS_LINK_STATE_MAX
} eth_ecpriss_link_state_e;

/**
 * ecpriss_eth_link_params
 * This struct is used for the link parameter values from the
 * topology.
 * link_index - link index associated with the specific link
 * eth_mac_address - mac address of the link
 * link_rate - speed of the link associated with the link
 */

typedef struct eth_ecpriss_link_params {
	uint8_t                          link_index;
	uint16_t                         link_mtu;
	uint8_t                          eth_mac_addr[ECPRISS_MAC_ADDR_LEN];
	eth_ecpriss_link_state_e         link_state;
	eth_ecpriss_link_rate_e          link_rate;
	bool                             loopback_enabled;
} eth_ecpriss_link_params_s;

/**
 * ecpriss_eth_port_params
 * This struct is used for the port parameter values from the
 * topology.
 * num_links - Total number of links associated with the port
 * link_params - Values associated with each link
 */
typedef struct eth_ecpriss_port_params {
	uint8_t                        port_index;
	uint8_t                        num_links;
	eth_ecpriss_link_params_s      link_params[ECPRISS_MAX_LINKS];
} eth_ecpriss_port_params_s;


/**
 * ecpriss_eth_topology_params
 * This struct is used for the topology values from the
 *  topology.
 * port_type - Type of port - FH/C2C/L2
 * num_ports- Number of active ports associated
 * port_params -number of links per port and associated params,
 * index corresponds to the port index
 */
typedef struct eth_ecpriss_topology_params {
	eth_ecpriss_port_type_e        port_type;
	uint8_t                        num_ports; //3 FH ports
	eth_ecpriss_port_params_s      port_params[ECPRISS_MAX_PORTS];
} eth_ecpriss_topology_params_s;

/**
 * ecpriss_eth_topology_params
 * This struct is used for the topology values from the
 * topology.
 * num_unique_port_types- unique port such as
 * L2/FH/C2C e.g 2 if there is 1 FH and 1 C2C
 * topology_params- Consists of different topology params
 */
typedef struct eth_ecpriss_topology_root {
	bool                            eth_topology_init_done;
	uint8_t                         num_unique_port_types;
	eth_ecpriss_topology_params_s
			topology_params[ECPRISS_MAX_UNIQUE_PORT];
} eth_ecpriss_topology_root_s;


/**
 * eth_ecpriss_register_events_params
 * Port and link information for incoming events
 * port_num - registers for IF up/down event notification
 * link_id - link which has received the up/down event
 * port_type - Type of port
 */
typedef struct eth_ecpriss_link_event_params {
	uint8_t                        port_index;
	uint8_t                        link_index;
	eth_ecpriss_port_type_e        port_type;
} eth_ecpriss_link_event_params_s;



typedef void (*eth_ecpriss_interface_events_cb)
		(eth_ecpriss_event_e event_type,
		eth_ecpriss_link_event_params_s *link_event_params);


/**
 * eth_ecpriss_register_topology_cb() - ECPRISS driver
 * registration with ETH driver for topology
 * @eth_ecpriss_register_topology_params - Register for the
 * topology CB and provide the params
 * Registration for topology callback is done via this function.
 * When the topology is available the ethernet invokes this
 * callback function and provides the required parameters.
 *
 *
 * Returns:	0 on success, negative on failure
 */
eth_ecpriss_status_e eth_ecpriss_register_ready_cb
		(eth_ecpriss_topology_ready_cb ready_cb, bool *is_ready);


/**
 * eth_ecpriss_get_stats() - ETH driver will provide the
 * relevant stats to the ECPRISS driver.
 *
 * Device_mode : DU or RU operating mode
 * num_port_type:Number of unique port types for the device
 * (FH, C2C, L2)
 *
 * Returns:	0 on success, negative on failure
 */
eth_ecpriss_status_e eth_ecpriss_get_topology
		(eth_ecpriss_dev_mode_e *device_mode,
		eth_ecpriss_topology_root_s   *topology_params);


/**
 * eth_ecpriss_register_events_cb() - ECPRISS driver
 * registration with ETH driver for events
 * @eth_ecpriss_register_topology_params - Register for the
 * required events CB and provide the params
 * Registration for events callback is done via this function.
 * For up/down events the ecpriss driver will obtain the callback
 * from the ethernet driver
 *
 * Returns:	0 on success, negative on failure
 */
eth_ecpriss_status_e eth_ecpriss_register_events_cb
			(eth_ecpriss_interface_events_cb events_cb);
/***
 * eth_ecpriss_deregister_events_cb() - ECPRISS driver
 * deregistration with ETH driver for events
 *
 * Returns:	0 on success, 1 on failure
 */

eth_ecpriss_status_e eth_ecpriss_deregister_events_cb(void);

/**
 * eth_ecpriss_enable_logging_port() - ETH driver will enable the
 * logging port - C2C2 for Egress logging of CU plane data.
 * action : To enable/disable port
 * 	Enable : true
 * 	Disable: false
 *
 * Returns:	0 on success, negative on failure
 */

eth_ecpriss_status_e eth_ecpriss_enable_logging_port(bool action);


/**
 * eth_ecpriss_enable_ru_cascade_c2c_bringup() - Bring up C2C2
 * (eth30) and C2C1 (eth31) in E2E mode for RU cascade.
 * Called when ru_cascade_mode=1 instead of the loopback path.
 *
 * Returns:	0 on success, negative on failure
 */

eth_ecpriss_status_e eth_ecpriss_enable_ru_cascade_c2c_bringup(void);

/**
 * struct eth_ecpriss_ops - Structure to hold ETH - eCPRI
 * interface functions
 */
struct eth_ecpriss_ops {
	eth_ecpriss_status_e (*eth_ecpriss_register_ready_cb)
		(eth_ecpriss_topology_ready_cb ready_cb, bool *is_ready);
	eth_ecpriss_status_e (*eth_ecpriss_register_events_cb)
		(eth_ecpriss_interface_events_cb events_cb);
	eth_ecpriss_status_e (*eth_ecpriss_deregister_events_cb)(void);
	eth_ecpriss_status_e (*eth_ecpriss_get_topology)
		(eth_ecpriss_dev_mode_e *device_mode,
		 eth_ecpriss_topology_root_s *topology_params);
	eth_ecpriss_status_e (*eth_ecpriss_enable_logging_port)(bool action);
	eth_ecpriss_status_e (*eth_ecpriss_enable_ru_cascade_c2c_bringup)(void);
};

#endif // ECPRI_ETH_H

/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <net/genetlink.h>
#include "ldmm_shrd_genntlk.h"
#include "ldmm_ipc_log.h"
#include "ldmm_notifr.h"
#include "ldmm_shrd_notifr.h"
#include "ldmm_genl.h"

#define MSG_MAX_LEN 1024
#define INIT_MSG 1234
#define SIZEOF_CONFIG_PACKET sizeof(int)*240

#define IS_MULTICAST_EN

static struct genl_family genl_fam;
static uint32_t dst_portid;


extern struct ldmm_eth_iface_ops mtip_driver_iface_ops;

/* Handler for LDMM_CMD_FULT_MGMT_RCV messages received */
static int fult_mgmt_rcv_doit(struct sk_buff *skb, struct genl_info *info)
{
	int ret = 0;
	uint32_t *str = NULL;

	if (info->attrs[LDMM_A_MSG]) {
		str = nla_data(info->attrs[LDMM_A_MSG]);

		LDMM_LOG_INFO("message received: %u\n", str);
	} else {
		LDMM_LOG_INFO("empty message received\n");
	}
	dst_portid = info->snd_portid;

	/*
	 * Dummy MSG to check connectivity
	 */
#ifdef IS_MULTICAST_EN
	fult_mgmt_snd(INIT_MSG, MULTICAST_MSG);
#else
	fult_mgmt_snd(INIT_MSG, UNICAST_MSG);
#endif

	return ret;
}


/* Attribute validation policy for our fult_mgmt_rcv command */
static struct nla_policy fult_mgmt_rcv_pol[LDMM_A_MAX + 1] = {
	[LDMM_A_MSG] = { .type = NLA_U32 },
  	[LDMM_QXDM_LOGGER_ATTR_GET_STATS_INFO] = { .type = NLA_U32 },
  	[LDMM_QXDM_LOGGER_ATTR_GET_CONFIG_INFO] = { .type = NLA_U32},
  	[LDMM_QXDM_LOGGER_ATTR_LINK_CHANGE_NOTIFICATION] = { .type = NLA_U32},
	[LDMM_QXDM_LOGGER_ATTR_UPDATE_TIMER_VALUE] = { .type = NLA_U32},
	[LDMM_QXDM_LOGGER_ATTR_SET_INTERFACE_LOOPBACK_MODE] = { .type = NLA_BINARY, .len = LOOPBACK_CONFIG_SIZE},
};

/* Operations for our Generic Netlink family */
static struct genl_ops genl_ops[] = {
 	{
    		.cmd	= LDMM_CMD_FULT_MGMT_RCV,
		.policy = fult_mgmt_rcv_pol,
		.doit	= fult_mgmt_rcv_doit,
	},
  	{
    		.cmd = LDMM_QXDM_LOGGER_CMD_GET_STATS_INFO,
	        .policy = fult_mgmt_rcv_pol,
        	.doit = ldmm_qxdm_logger_get_stats_info,
      	},
      	{
        	.cmd = LDMM_QXDM_LOGGER_CMD_GET_CONFIG_INFO,
        	.policy = fult_mgmt_rcv_pol,
        	.doit = ldmm_qxdm_logger_get_config_info,
      	},
      	{
        	.cmd = LDMM_QXDM_LOGGER_CMD_LINK_CHANGE_NOTIFICATION,
        	.policy = fult_mgmt_rcv_pol,
        	.doit = ldmm_qxdm_logger_no_action,
      	},
	{
        	.cmd = LDMM_QXDM_LOGGER_CMD_UPDATE_TIMER_VALUE,
        	.policy = fult_mgmt_rcv_pol,
        	.doit = ldmm_qxdm_logger_no_action,
      	},
	{
		.cmd = LDMM_QXDM_LOGGER_CMD_SET_INTERFACE_LOOPBACK_MODE,
		.policy = fult_mgmt_rcv_pol,
		.doit = ldmm_qxdm_logger_set_loopback_mode,
	},
};

/* Multicast groups for our family */
static const struct genl_multicast_group genl_mcgrps[] = {
	{ .name = LDMM_MC_GRP_NAME },
};

/* Generic Netlink family */
static struct genl_family genl_fam = {
	.name	  = LDMM_GENL_NAME,
	.version  = LDMM_GENL_VERSION,
	.maxattr  = LDMM_A_MAX,
	.ops	  = genl_ops,
	.n_ops	  = ARRAY_SIZE(genl_ops),
	.mcgrps	  = genl_mcgrps,
	.n_mcgrps = ARRAY_SIZE(genl_mcgrps),
};

/*
 * mode =0 unicast
 * mode =1 multicast
 */
int fult_mgmt_snd(uint32_t buf, int mode)
{
	int ret = 0;
	void *hdr = NULL;

	/* Allocate message buffer */
	struct sk_buff *skb = genlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);

	if ((!skb)) {
		LDMM_LOG_ERR("failed to allocate memory for genl message\n");
		return -ENOMEM;
	}

	/* Put the Generic Netlink header */
	hdr = genlmsg_put(skb, 0, 0, &genl_fam, 0, LDMM_CMD_FULT_MGMT_RCV);
	if ((!hdr)) {
		LDMM_LOG_ERR("failed to allocate memory for genl header\n");
		nlmsg_free(skb);
		return -ENOMEM;
	}

	if ((ret = nla_put_u32(skb, LDMM_A_MSG, buf))) {
		LDMM_LOG_ERR("unable to create netlink msg Err: %d\n",ret);
		genlmsg_cancel(skb, hdr);
		nlmsg_free(skb);
		return ret;
	}

	/* Finalize the message */
	genlmsg_end(skb, hdr);

	LDMM_LOG_INFO("%s: Buf = 0x%x\n",__func__,buf);

#ifdef IS_MULTICAST_EN
	if(mode == MULTICAST_MSG){
		ret = genlmsg_multicast(&genl_fam, skb, 0, 0, GFP_KERNEL);
		if (ret == -ESRCH) {
			LDMM_LOG_ERR("multicast message sent, but nobody was listening...\n");
		} else if (ret) {
			LDMM_LOG_ERR("failed to send multicast genl message\n");
		} else {
			LDMM_LOG_INFO("multicast message sent\n");
		}
	}else if(mode == UNICAST_MSG){
		ret = genlmsg_unicast(&init_net, skb, dst_portid);
		if(ret != 0){
			LDMM_LOG_ERR("Unicast message sent failed: %d\n", ret );
		}
	}
#else
	ret = genlmsg_unicast(&init_net, skb, dst_portid);
	if(ret != 0){
		LDMM_LOG_ERR("Unicast message sent failed Err: %d\n", ret );
	}
#endif

	return ret;
}

stats_packet_info stats = {0};

int ldmm_qxdm_logger_get_stats_info(struct sk_buff *sender_skb, struct genl_info *info) {
	struct nlattr *na;
	struct sk_buff *reply_skb;
	void *msg_head;
	int ret_val = 0;
	int link_index, i = 0;

	if (info == NULL) {
		ret_val = EINVAL;
		return ret_val;
	}

	na = info->attrs[LDMM_QXDM_LOGGER_ATTR_GET_STATS_INFO];
	if (!na) {
		ret_val = EINVAL;
		return ret_val;
	}

	memset(&stats, 0,sizeof(stats_packet_info));

	for(link_index = 0; link_index < TOTAL_LINKS; link_index++)
	{
		if(link_index != MTIP_L2_ETH_LINK_INDEX && mtip_driver_iface_ops.ldmm_eth_iface_get_if_link_up(link_index))
		{
			stats.total_active_links++;
			stats.stats[i++] = mtip_driver_iface_ops.ldmm_eth_iface_get_stats_info(link_index);
		}
    	}

	reply_skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (reply_skb == NULL)
	{
		pr_err("Out of Memory \n");
		return -1;
	}

	msg_head = genlmsg_put(reply_skb, info->snd_portid, info->snd_seq + 1, &genl_fam, 0, LDMM_QXDM_LOGGER_CMD_GET_STATS_INFO);


	if (msg_head == NULL) {
        	pr_err("genlmsg_put failed \n");
        	return -1;
    	}
    	ret_val = nla_put(reply_skb, LDMM_QXDM_LOGGER_ATTR_GET_STATS_INFO, sizeof(stats), &stats);


	if (ret_val != 0) {
		pr_err("nla_put API failed \n");
		return -1;
	}

	genlmsg_end(reply_skb, msg_head);

	ret_val = genlmsg_reply(reply_skb, info);
	if (ret_val != 0) {
		pr_err("genlmsg_reply failed \n");
		return -1;
    	}
    	return 0;
}

int ldmm_qxdm_logger_update_timer_value(int timer_value)
{
	struct sk_buff *reply_skb;
	void *msg_head;
	int ret_val = 0;

	reply_skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (reply_skb == NULL) {
        	pr_err("Out of Memory \n");
        	return -1;
    	}

	msg_head = genlmsg_put(reply_skb, 0, 0, &genl_fam, 0, LDMM_QXDM_LOGGER_CMD_UPDATE_TIMER_VALUE);

	if (msg_head == NULL) {
		pr_err("genlmsg_put failed \n");
		return -1;
	}

	ret_val = nla_put(reply_skb, LDMM_QXDM_LOGGER_ATTR_UPDATE_TIMER_VALUE, sizeof(int), &timer_value);

	if (ret_val != 0) {
		pr_err("nla_put API failed \n");
		return -1;
	}

	genlmsg_end(reply_skb, msg_head);
#ifdef IS_MULTICAST_EN
		ret_val = genlmsg_multicast(&genl_fam, reply_skb, 0, 0, GFP_KERNEL);
		if (ret_val == -ESRCH) {
			LDMM_LOG_ERR("multicast message sent, but nobody was listening...\n");
		} else if (ret_val) {
			LDMM_LOG_ERR("failed to send multicast genl message\n");
		} else {
			LDMM_LOG_INFO("multicast message sent\n");
		}
#else
	ret_val = genlmsg_unicast(&init_net, reply_skb, dst_portid);
	if (ret_val != 0) {
		pr_err("genlmsg_unicast failed \n");
		return -1;
	}
#endif
	return 0;
}

void parse_config_packet(config_packet_info* config_packet, int parsed_msg[])
{
	int port_index, link_index, i = 0;
	for(port_index = 0; port_index < MAX_PORTS; port_index++)
	{
		memcpy(&parsed_msg[i++], &config_packet->ports[port_index].port_type, sizeof(int));
		memcpy(&parsed_msg[i++], &config_packet->ports[port_index].port_enabled, sizeof(int));
		memcpy(&parsed_msg[i++], &config_packet->ports[port_index].phy_eq_mode, sizeof(int));
		memcpy(&parsed_msg[i++], &config_packet->ports[port_index].sfp_port_type, sizeof(int));
		memcpy(&parsed_msg[i++], &config_packet->ports[port_index].port_config, sizeof(int));
		memcpy(&parsed_msg[i++], &config_packet->ports[port_index].active_fec, sizeof(int));
		memcpy(&parsed_msg[i++], &config_packet->ports[port_index].link_length_range, sizeof(int));
		memcpy(&parsed_msg[i++], &config_packet->ports[port_index].active_links, sizeof(int));

		for(link_index = 0; link_index < MAX_LINKS_PER_PORT; link_index++)
		{
			if(config_packet->ports[port_index].links[link_index].link_status != 3)
				continue;
			memcpy(&parsed_msg[i++], &config_packet->ports[port_index].links[link_index].link_name, sizeof(int));
			memcpy(&parsed_msg[i++], &config_packet->ports[port_index].links[link_index].link_status, sizeof(int));
			memcpy(&parsed_msg[i++], &config_packet->ports[port_index].links[link_index].link_speed, sizeof(int));
			memcpy(&parsed_msg[i++], &config_packet->ports[port_index].links[link_index].lanes_mapped, sizeof(int));
			memcpy(&parsed_msg[i++], &config_packet->ports[port_index].links[link_index].lanes_speed, sizeof(int));
		}
	}
}

int ldmm_qxdm_logger_get_config_info(struct sk_buff *sender_skb, struct genl_info *info) {
	struct nlattr *na;
	struct sk_buff *reply_skb;
	void *msg_head;
	config_packet_info config;
	int ret_val = 0;
	int parsed_msg[240] = {0};

	if (info == NULL) {
		ret_val = EINVAL;
		return ret_val;
	}

	na = info->attrs[LDMM_QXDM_LOGGER_ATTR_GET_CONFIG_INFO];
	if (!na) {
        	ret_val = EINVAL;
        	return ret_val;
    	}

	config = mtip_driver_iface_ops.ldmm_eth_iface_get_config_info();

	parse_config_packet(&config, parsed_msg);

	reply_skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (reply_skb == NULL) {
        	pr_err("Out of Memory \n");
        	return -1;
    	}

	msg_head = genlmsg_put(reply_skb, info->snd_portid, info->snd_seq + 1, &genl_fam, 0, LDMM_QXDM_LOGGER_CMD_GET_CONFIG_INFO);

	if (msg_head == NULL) {
		pr_err("genlmsg_put failed \n");
		return -1;
	}

	ret_val = nla_put(reply_skb, LDMM_QXDM_LOGGER_ATTR_GET_CONFIG_INFO, SIZEOF_CONFIG_PACKET, parsed_msg);

	if (ret_val != 0) {
		pr_err("nla_put API failed \n");
		return -1;
	}

	genlmsg_end(reply_skb, msg_head);

	ret_val = genlmsg_reply(reply_skb, info);

	if (ret_val != 0) {
		pr_err("genlmsg_reply failed \n");
		return -1;
	}
	return 0;
}

int ldmm_qxdm_logger_link_change_notification(event_info_struct *event_info, int link_up)
{
	struct sk_buff *reply_skb;
	void *msg_head;
	config_packet_info config;
	int ret_val = 0;
	int parsed_msg[240] = {0};

	config = mtip_driver_iface_ops.ldmm_eth_iface_get_config_info();

	parsed_msg[0] = link_up;
	parsed_msg[1] = event_info->port_type;
	parsed_msg[2] = event_info->interface;

	parse_config_packet(&config, &parsed_msg[EVENT_PACKET_SIZE]);

	reply_skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (reply_skb == NULL) {
		pr_err("Out of Memory \n");
		return -1;
	}

	msg_head = genlmsg_put(reply_skb, 0, 0, &genl_fam, 0, LDMM_QXDM_LOGGER_CMD_LINK_CHANGE_NOTIFICATION);

	if (msg_head == NULL) {
		pr_err("genlmsg_put failed \n");
		return -1;
	}

	ret_val = nla_put(reply_skb, LDMM_QXDM_LOGGER_ATTR_LINK_CHANGE_NOTIFICATION, SIZEOF_CONFIG_PACKET, parsed_msg);

	if (ret_val != 0) {
		pr_err("nla_put API failed \n");
		return -1;
	}

	genlmsg_end(reply_skb, msg_head);

#ifdef IS_MULTICAST_EN
		ret_val = genlmsg_multicast(&genl_fam, reply_skb, 0, 0, GFP_KERNEL);
		if (ret_val == -ESRCH) {
			LDMM_LOG_ERR("multicast message sent, but nobody was listening...\n");
		} else if (ret_val) {
			LDMM_LOG_ERR("failed to send multicast genl message\n");
		} else {
			LDMM_LOG_INFO("multicast message sent\n");
		}
#else
	ret_val = genlmsg_unicast(&init_net, reply_skb, dst_portid);
	if (ret_val != 0) {
		pr_err("genlmsg_unicast failed \n");
		return -1;
	}
#endif
	return 0;
}

int ldmm_qxdm_logger_set_loopback_mode(struct sk_buff *sender_skb, struct genl_info *info) {
	struct nlattr *na;
	loopback_config_info *loopback_config;
	char **interface_list = NULL;
	int ret_val = 0;
	int i;

	if (info == NULL) {
		ret_val = -EINVAL;
		return ret_val;
	}

	na = info->attrs[LDMM_QXDM_LOGGER_ATTR_SET_INTERFACE_LOOPBACK_MODE];
	if (!na) {
		LDMM_LOG_ERR("No loopback configuration data received\n");
		ret_val = -EINVAL;
		return ret_val;
	}

	/* Extract loopback configuration from netlink message */
	loopback_config = (loopback_config_info *)nla_data(na);
	if (!loopback_config) {
		LDMM_LOG_ERR("Failed to extract loopback configuration data\n");
		ret_val = -EINVAL;
		return ret_val;
	}

	LDMM_LOG_INFO("Received loopback interface configuration with %d interfaces\n", 
		loopback_config->interface_count);

	/* Validate interface count */
	if (loopback_config->interface_count <= 0 || 
	    loopback_config->interface_count > MAX_LOOPBACK_INTERFACES) {
		LDMM_LOG_ERR("Invalid interface count: %d\n", loopback_config->interface_count);
		ret_val = -EINVAL;
		return ret_val;
	}

	/* Allocate array of interface name pointers */
	interface_list = kmalloc(loopback_config->interface_count * sizeof(char *), GFP_KERNEL);
	if (!interface_list) {
		LDMM_LOG_ERR("Failed to allocate memory for interface list\n");
		ret_val = -ENOMEM;
		return ret_val;
	}

	/* Set up pointers to interface names */
	for (i = 0; i < loopback_config->interface_count; i++) {
		interface_list[i] = loopback_config->interface_names[i];
		LDMM_LOG_INFO("Interface[%d]: %s\n", i, interface_list[i]);
	}

	/* Call MTIP driver interface to set loopback interfaces */
	if (mtip_driver_iface_ops.ldmm_eth_iface_set_loopback_interfaces) {
		ret_val = mtip_driver_iface_ops.ldmm_eth_iface_set_loopback_interfaces(
			interface_list, loopback_config->interface_count);
		
		if (ret_val == 0) {
			LDMM_LOG_INFO("Successfully configured %d loopback interfaces in MTIP\n", 
				loopback_config->interface_count);
		} else {
			LDMM_LOG_ERR("Failed to configure loopback interfaces in MTIP, error: %d\n", ret_val);
		}
	} else {
		LDMM_LOG_ERR("MTIP loopback interface function not available\n");
		ret_val = -ENOSYS;
	}

	/* Clean up allocated memory */
	kfree(interface_list);

	return ret_val;
}

int ldmm_qxdm_logger_no_action(struct sk_buff *sender_skb, struct genl_info *info) {
	return 0;
}

int  init_genlldmm(void)
{
	int ret = 0;

	LDMM_LOG_INFO("init start\n");

	ret = genl_register_family(&genl_fam);
	if((ret)){
		LDMM_LOG_ERR("failed to register generic netlink family Err: %d\n",ret);
	}

	ldmm_enable_notification();
	LDMM_LOG_INFO("init end\n");

	return ret;
}

void exit_genlldmm(void)
{
	int ret = 0;

	if ((ret = genl_unregister_family(&genl_fam))) {
		LDMM_LOG_ERR("failed to unregister generic netlink family Err: %d\n",ret);
	}

	ldmm_disable_notification();

	LDMM_LOG_INFO("exit\n");
}

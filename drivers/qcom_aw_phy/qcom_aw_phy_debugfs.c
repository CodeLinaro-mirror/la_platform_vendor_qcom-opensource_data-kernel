
/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

/**
  @file qcom_aw_phy_test_sysfs.c
  @brief Test SYS FS for AW PHY driver.

  This file contains SYS FS definitions for AW PHY driver.
*/

#include <linux/sysfs.h>
#include <linux/debugfs.h>

#include "qcom_aw_phy_main.h"
#include "qcom_aw_phy_mtip_if.h"
#include "qcom_aw_phy_gnl.h"
#include "qcom_aw_phy_utils.h"
#include "eth_phy_iface.h"
#include "aw_c_api/aw_alphacore.h"
#include "qcom_aw_phy_debugfs_dir_struct.h"

#ifdef FEATURE_QCOM_AW_DEBUG_FS

struct dentry *dobj;
struct dentry *list_dv[64];
char input_string[] = TREE;
#define FH_WRAPPER_SIZE 64

typedef struct{
	char *source;
	char *delim;
	char *token;
	uint32_t *index;
	uint32_t token_len;
	uint32_t delim_len;
	uint32_t source_len;
}parser_s;

static const struct file_operations qcom_aw_phy_debug_fs_ops = {
  .write = qcom_aw_phy_set_attr,
  .read = qcom_aw_phy_get_attr,
};

static const struct file_operations qcom_aw_phy_debug_fs_prbs_result_ops = {
  .read = qcom_aw_phy_get_prbs_result,
};

static const struct file_operations qcom_aw_phy_debug_fs_tx_eq_ops = {
  .write = qcom_aw_phy_set_tx_eq_val,
  .read = qcom_aw_phy_get_tx_eq_val,
};

enum qcom_aw_phy_debug_fs_cmd{
  PHY_REG = 1,
  PHY_SETUP,
  PHY_BRINGUP,
  PHY_TEAR_DOWN,
  MAC_LINK_UP,
  MAC_LINK_DOWN,
  PHY_DEREG,
  SNR_VALID_CB,
  AN_DONE_CB,
  NES_LB,
  NEP_LB,
  DUMP_PHY_REG,
  SET_PORT,
  SET_NUM_LANES,
  SET_LANE_SPEED,
  TX_BIST_PHY_NUM,
  TX_BIST_LANE_NUM,
  RX_BIST_PHY_NUM,
  RX_BIST_LANE_NUM,
  PRBS_PATTERN,
  UDP_DATA,
  BIST_DURATION,
  BIST_ERROR_COUNT,
  ENABLE_TX_BIST,
  RX_CDR_CHECKER,
  ENABLE_RX_BIST,
  CHECK_PRBS_ALL_LANES,
  SET_EQ_MODE,
  SET_PORT_CONFIG_MASK,
  INITIATE_AN,
  RESET_PHY_SM,
};

int                                        qcom_aw_phy_attr_val;
enum mtip_port_type_enum                   port_type = MTIP_PORT_TYPE_FH_0;
int                                        num_lanes = 4;
enum eth_phy_iface_phy_lane_speed_enum     lane_speed = PHY_LANE_SPEED_25G;

// PRBS parameters
enum qcom_aw_phy_instance_enum         tx_bist_phy_inst = QCOM_AW_PHY_INST_FH0;
enum eth_phy_iface_phy_lane_num_enum   tx_bist_lane_num = PHY_LANE_0;
enum qcom_aw_phy_instance_enum         rx_bist_phy_inst = QCOM_AW_PHY_INST_FH0;
enum eth_phy_iface_phy_lane_num_enum   rx_bist_lane_num = PHY_LANE_0;
aw_bist_pattern_t                      bist_pattern = AW_PRBS31;
int                                    measure_time = 1;
int                                    inject_error_count = 0;
uint64_t                               user_data = 0;
uint32_t                               err_count_overflow[12] = {0};
uint64_t                               err_count[12] = {0};
uint64_t                               ber[12] = {0};
bool                                   check_prbs_all_lanes = false;
uint32_t                               port_config_mask = 0x800000;

aw_txfir_config_t        tx_fir_cfg_cache[QCOM_AW_PHY_INST_MAX][PHY_LANE_MAX] = {{0}};
bool                     tx_fir_cfg_cache_valid[QCOM_AW_PHY_INST_MAX][PHY_LANE_MAX] = {{false}};

extern struct eth_phy_iface_ops qcom_aw_phy_driver_iface_ops;

#ifndef MIN
#define MIN(a,b) ((a < b) ? a : b)
#endif

char help_menu[] = {
"1		ETH PHY drivers interface registration(dummy MAC/PHY registration)\n\
2		PHY setup from MAC\n\
3		PHY bring up from MAC\n\
4		PHY tear down from MAC\n\
5		MAC link up status to PHY\n\
6		MAC link down status to PHY\n\
7		ETH PHY drivers interface deregistration\n\
8		SNR valid status change from PHY to SyncE\n\
9		AN done callback from PHY to MAC\n\
10		Sets near end serial loopback mode on PHY\n\
11		Sets near end parallel loopback mode on PHY\n\
12		Dumps PHY registers to /tmp/phy_registers.txt on target\n\
13,x	Sets the port type for set up/bring up/tear down operations\n\
		(Value 'x' can be 0-FH0, 1-FH1, 2-FH2, 3-L2, 4-Debug)\n\
		For example, echo 13,0 > /sys/kernel/debug/qcom_aw_phy_test/qcom_aw_phy\n\
		This will configure port as FH0, and all further operations will correspond to the last configured port.\n\
14,x	Sets the number of lanes for set up/bring up/tear down operations\n\
		(Value 'x' can be 1/2/3/4 based on the number of lanes to be configured)\n\
		For example, echo 14,4 > /sys/kernel/debug/qcom_aw_phy_test/qcom_aw_phy\n\
		This will configure number of lanes as 4, and all further operations will correspond to the last configured number of lanes.\n\
15,x	Sets the lane speed for set up/bring up/tear down operations\n\
		(Value 'x' can be 0-10G, 1-25G, 2-50G, 3-100G)\n\
		For example, echo 15,1 > /sys/kernel/debug/qcom_aw_phy_test/qcom_aw_phy\n\
		This will configure lane speed as 25G, and all further operations will correspond to this lane speed for all the lanes specified above.\n\
16,x	Sets the PHY instance type for TX BIST generator\n\
		(Value 'x' can be 0-FH0, 1-FH1, 2-FH2, 3-L2, 4-Debug)\n\
17,x	Sets the PHY lane number 'x' for TX BIST generator\n\
18,x	Sets the PHY instance type for RX BIST checker\n\
		(Value 'x' can be 0-FH0, 1-FH1, 2-FH2, 3-L2, 4-Debug)\n\
19,x	Sets the PHY lane number 'x' for RX BIST checker\n\
20,x	Sets the PRBS BIST pattern. Value 'x' can be set from one of the below patterns\n\
		AW_PRBS7 = 0,\n\
		AW_PRBS9 = 1,\n\
		AW_PRBS11 = 2,\n\
		AW_PRBS13 = 3,\n\
		AW_PRBS15 = 4,\n\
		AW_PRBS23 = 5,\n\
		AW_PRBS31 = 6,\n\
		AW_QPRBS13 = 7,\n\
		AW_JP03A = 8,\n\
		AW_JP03B = 9,\n\
		AW_LINEARITY_PATTERN = 10,\n\
		AW_USER_DEFINED_PATTERN = 11,\n\
		AW_FULL_RATE_CLOCK = 12,\n\
		AW_HALF_RATE_CLOCK = 13,\n\
		AW_QUARTER_RATE_CLOCK = 14,\n\
		AW_PATT_32_1S_32_0S = 15\n\
21,x	Sets the user data pattern in hex if BIST pattern is selected as AW_USER_DEFINED_PATTERN = 11 in the above option\n\
22,x	Sets the duration in 'x' seconds to monitor pattern on RX checker side.\n\
23,x	Sets the number of errors 'x' to be injected from TX BIST generator. Default is 0.\n\
24,x	Enables/disables TX BIST generator based on value 'x' (0-disable/1-enable)\n\
25		Checks RX CDR lock on RX PHY and lane set with options 18 and 19 respectively.\n\
26,x	Enables/disables RX BIST checker based on value 'x' (0-disable/1-enable)\n\
27,x	Flag to configure if PRBS needs to be validated for all lanes\n\
28,x	Set equalization mode 'x' (1-Manual, 2-ANLT. 3-LT)\n"};

void qcom_aw_phy_setup_debugfs() {

  /* creating the directory structure in /sys/kernel/debug */
  dobj = debugfs_create_dir("qcom_aw_phy_test", NULL);

  debugfs_create_file("qcom_aw_phy", 0644, dobj, 0, &qcom_aw_phy_debug_fs_ops);

  debugfs_create_file("prbs_result", 0644, dobj, 0,
                      &qcom_aw_phy_debug_fs_prbs_result_ops);

  debugfs_create_file("tx_eq_val", 0644, dobj, 0,
                      &qcom_aw_phy_debug_fs_tx_eq_ops);
  setup_phy_status_debugfs_directory();

  return;
}

void qcom_aw_phy_del_debugfs() {

  /* deleting the directory structure in /sys/kernel/debug */
  debugfs_remove_recursive(dobj);
  return;
}

void qcom_aw_phy_ready_cb(void *udata) {
  QCOM_AW_PHY_LOG_DBG("qcom_aw_phy_ready_cb");
}

void qcom_aw_phy_an_result_cb(enum mtip_port_type_enum port_type,
                                      bool an_result,
                                      enum mtip_port_config_enum port_config,
                                      u8 seq_num) {

  QCOM_AW_PHY_LOG_DBG("qcom_aw_phy_an_result_cb port %d, seq %d, result %d, config %d",
                      port_type, seq_num, an_result, port_config);
}

void qcom_aw_phy_cdr_lock_ind(u32 link_index, bool status, u8 an_seq_num) {
  QCOM_AW_PHY_LOG_DBG("CDR lock for link_index %d, status %d, an_seq_num %d", link_index, status, an_seq_num);
}

void qcom_aw_phy_lane_bring_up_progress_ind(u32 link_index, bool in_progress) {
  QCOM_AW_PHY_LOG_DBG("Lane bring up in progress: %d for link index %d", in_progress, link_index);
}
ssize_t qcom_aw_phy_get_prbs_result(struct file *file, char __user *buf,
                                    size_t count, loff_t *ppos){
  char dbg_buf[1500] = {0};
  int nbytes = 0, i, j, min, max, min_port, max_port, lane_index;

  if(rx_bist_lane_num != PHY_LANE_MAX){
    min = rx_bist_lane_num;
    max = rx_bist_lane_num;
  }
  else{
    min = PHY_LANE_0;
    max = PHY_LANE_3;
  }

  if(check_prbs_all_lanes == false){
    if(rx_bist_phy_inst <= QCOM_AW_PHY_INST_FH2){
      min_port = rx_bist_phy_inst;
      max_port = rx_bist_phy_inst;
    }
    else{
      min_port = QCOM_AW_PHY_INST_FH0;
      max_port = QCOM_AW_PHY_INST_FH0;
    }
  }
  else{
    min_port = QCOM_AW_PHY_INST_FH0;
    max_port = QCOM_AW_PHY_INST_FH2;
  }

  for (j = min_port; j <= max_port; j++) {
    for (i = min; i <= max; i++) {
      lane_index = (j*PHY_LANE_MAX) + i;
      nbytes += scnprintf(dbg_buf + strlen(dbg_buf), 120,
                          "Port %d, Lane %d, error count = %lu, "
                          "error overflow count = %d, ber = %lu\n",
                          j, i, err_count[lane_index],
                          err_count_overflow[lane_index], ber[lane_index]);
    }
  }

  return simple_read_from_buffer(buf, count, ppos, dbg_buf, nbytes);
}

ssize_t qcom_aw_phy_get_attr(struct file *file, char __user *buf,
                             size_t count, loff_t *ppos) {
  return simple_read_from_buffer(buf, count, ppos, help_menu, sizeof(help_menu));
}

ssize_t qcom_aw_phy_set_attr(struct file *file, const char __user *buf,
                             size_t count, loff_t *ppos) {
  struct eth_phy_iface_eth_register_params ready_info;
  bool is_phy_ready;
  struct eth_phy_iface_phy_lane_config lane_config[PHY_LANE_MAX];
  int i = 0, j = 0, k = 0;
  int min = 0, max = 0, min_port = 0, max_port = 0, lane_index = 0;
  bool lanes_enabled[PHY_LANE_MAX] = {true, true, true, true};
  struct qcom_aw_phy_gnl_snr_valid_change snr_valid_info;
  char *token;
  char token_string[100];
  char *save_ptr = NULL;
  struct qcom_aw_phy_config *phy_config_info = NULL;
  struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
  mss_access_t mss = {.phy_offset = 0, .lane_offset = 0};
  struct qcom_aw_phy_inst_config *tx_phy_inst_info = NULL;
  mss_access_t tx_mss = {.phy_offset = 0, .lane_offset = 0};
  int enable_flag = 0;
  uint32_t err_cnt_55_32, err_cnt_31_0;
  bool error = false;
  enum qcom_aw_phy_eq_mode_enum eq_mode = QCOM_AW_PHY_EQ_MODE_MIN;

  memset(token_string, 0, sizeof(token_string));
  if (copy_from_user(&token_string, buf, MIN(sizeof(token_string), count))){
    QCOM_AW_PHY_LOG_ERR("Copy from user failed");
    return -EFAULT;
  }

  token = qcom_aw_phy_strtok(token_string, ',', &save_ptr);
  if(token!=NULL)
    sscanf(token, "%d", &qcom_aw_phy_attr_val);
  else
  {
    QCOM_AW_PHY_LOG_ERR("Invalid input, \"cat /sys/kernel/debug/qcom_aw_phy_test/qcom_aw_phy\" for help menu");
    return -EFAULT;
  }

  QCOM_AW_PHY_LOG_ERR("qcom_aw_phy_set_attr val %d", qcom_aw_phy_attr_val);

  switch (qcom_aw_phy_attr_val) {

    case PHY_REG:
      QCOM_AW_PHY_LOG_ERR("Register");
      ready_info.notify_ready = qcom_aw_phy_ready_cb;
      ready_info.userdata_ready = NULL;
      ready_info.notify_an_result = qcom_aw_phy_an_result_cb;
      ready_info.cdr_lock_ind = qcom_aw_phy_cdr_lock_ind;
      ready_info.lane_bring_up_progress_ind = qcom_aw_phy_lane_bring_up_progress_ind;
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_eth_register(&ready_info,
                                                              &is_phy_ready);
      QCOM_AW_PHY_LOG_ERR("is_phy_ready %d", is_phy_ready);
      break;

    case PHY_SETUP:
      QCOM_AW_PHY_LOG_ERR("Setup");
      memset(lane_config, 0, 
             PHY_LANE_MAX * sizeof(struct eth_phy_iface_phy_lane_config));
      for (i = PHY_LANE_0; i < num_lanes; i++) {
        lane_config[i].lane_enabled = true;
        lane_config[i].lane_speed = lane_speed;
        lane_config[i].link_index = i;
      }
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_phy_setup(port_type,
                                                           lane_config);
      break;

    case PHY_BRINGUP:
      QCOM_AW_PHY_LOG_ERR("Bringup");
      memset(lanes_enabled, 0, PHY_LANE_MAX * sizeof(bool));
      for (i = PHY_LANE_0; i < num_lanes; i++) {
        lanes_enabled[i] = true;
      }
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_phy_bringup(port_type,
                                                             lanes_enabled, 0);
      break;

    case PHY_TEAR_DOWN:
      QCOM_AW_PHY_LOG_ERR("Tear Down");
      memset(lanes_enabled, 0, PHY_LANE_MAX * sizeof(bool));
      for (i = PHY_LANE_0; i < num_lanes; i++) {
        lanes_enabled[i] = true;
      }
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_phy_teardown(port_type,
                                                              lanes_enabled);
      QCOM_AW_PHY_LOG_ERR("Resetting loopback mode for all PHY instances and lanes!");
      qcom_aw_phy_apply_loopback_mode_global(QCOM_AW_PHY_NO_LB);
      break;

    case MAC_LINK_UP:
      QCOM_AW_PHY_LOG_ERR("MAC link status up");
      memset(lanes_enabled, 0, PHY_LANE_MAX * sizeof(bool));
      for (i = PHY_LANE_0; i < num_lanes; i++) {
        lanes_enabled[i] = true;
      }
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_notify_mac_link_status(
          port_type, lanes_enabled, true);
      break;

    case MAC_LINK_DOWN:
      QCOM_AW_PHY_LOG_ERR("MAC link status down");
      memset(lanes_enabled, 0, PHY_LANE_MAX * sizeof(bool));
      for (i = PHY_LANE_0; i < num_lanes; i++) {
        lanes_enabled[i] = true;
      }
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_notify_mac_link_status(
          port_type, lanes_enabled, false);
      break;

    case PHY_DEREG:
      QCOM_AW_PHY_LOG_ERR("Deregister");
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_eth_deregister();
      break;

    case SNR_VALID_CB:
      QCOM_AW_PHY_LOG_ERR("SNR valid status change");
      snr_valid_info.eth_inst = 0;
      snr_valid_info.snr_valid_status = true;
      qcom_aw_phy_gnl_snr_valid_change(snr_valid_info);
      break;

    case AN_DONE_CB:
      QCOM_AW_PHY_LOG_ERR("AN done callback simulation");
      // TBD
      break;

    case NES_LB:
      QCOM_AW_PHY_LOG_ERR("Set near end serial loopback mode !");
      qcom_aw_phy_apply_loopback_mode_global(QCOM_AW_PHY_NEAR_END_SERIAL_LB);
      break;

    case NEP_LB:
      QCOM_AW_PHY_LOG_ERR("Set near end parallel loopback mode !");
      qcom_aw_phy_apply_loopback_mode_global(QCOM_AW_PHY_NEAR_END_PARALLEL_LB);
      break;

    case DUMP_PHY_REG:
      QCOM_AW_PHY_LOG_ERR("Dumping PHY registers !");
      qcom_aw_phy_dump_registers();
      break;

    case SET_PORT:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &port_type);
      else
      {
        error = true;
        break;
      }

      if(port_type < MTIP_PORT_TYPE_FH_0 || port_type >= MTIP_PORT_TYPE_MAX){
        port_type = MTIP_PORT_TYPE_FH_0;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring port %d", port_type);
      break;

    case SET_NUM_LANES:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &num_lanes);
      else
      {
        error = true;
        break;
      }

      if(num_lanes < 1 || num_lanes > PHY_LANE_MAX){
        num_lanes = 1;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring num_lanes %d", num_lanes);
      break;

    case SET_LANE_SPEED:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &lane_speed);
      else
      {
        error = true;
        break;
      }

      if(lane_speed < PHY_LANE_SPEED_10G || lane_speed >= PHY_LANE_SPEED_MAX){
        lane_speed = PHY_LANE_SPEED_25G;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring lane_speed %d", lane_speed);
      break;

    case TX_BIST_PHY_NUM:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &tx_bist_phy_inst);
      else
      {
        error = true;
        break;
      }

      if(!QCOM_AW_PHY_INST_VALID(tx_bist_phy_inst)){
        tx_bist_phy_inst = QCOM_AW_PHY_INST_FH0;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring TX BIST PHY instance %d",
                          tx_bist_phy_inst);
      break;

    case TX_BIST_LANE_NUM:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &tx_bist_lane_num);
      else
      {
        error = true;
        break;
      }

      if(tx_bist_lane_num > PHY_LANE_MAX){
        tx_bist_lane_num = PHY_LANE_0;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring TX BIST PHY lane %d",
                          tx_bist_lane_num);
      break;

    case RX_BIST_PHY_NUM:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &rx_bist_phy_inst);
      else
      {
        error = true;
        break;
      }

      if(!QCOM_AW_PHY_INST_VALID(rx_bist_phy_inst)){
        rx_bist_phy_inst = QCOM_AW_PHY_INST_FH0;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring RX BIST PHY instance %d",
                          rx_bist_phy_inst);
      break;

    case RX_BIST_LANE_NUM:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &rx_bist_lane_num);
      else
      {
        error = true;
        break;
      }

      if(rx_bist_lane_num > PHY_LANE_MAX){
        rx_bist_lane_num = PHY_LANE_0;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring RX BIST PHY lane %d",
                          rx_bist_lane_num);
      break;

    case PRBS_PATTERN:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &bist_pattern);
      else
      {
        error = true;
        break;
      } 

      if(bist_pattern >= AW_BIST_PATTERN_MAX){
        bist_pattern = AW_PRBS31;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring BIST pattern %d", bist_pattern);
      break;

    case UDP_DATA:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%llx", &user_data);
      else
      {
        error = true;
        break;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring user data %llx", user_data);
      break;

    case BIST_DURATION:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &measure_time);
      else
      {
        error = true;
        break;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring measurement time %d", measure_time);
      break;

    case BIST_ERROR_COUNT:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &inject_error_count);
      else
      {
        error = true;
        break;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring inject error count %d",
                          inject_error_count);
      break;

    case ENABLE_TX_BIST:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &enable_flag);
      else
      {
        error = true;
        break;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring TX BIST, enabled = %d", enable_flag);

      phy_config_info = qcom_aw_phy_get_config_info();

      if(tx_bist_lane_num != PHY_LANE_MAX){
        min = tx_bist_lane_num;
        max = tx_bist_lane_num;
      }
      else{
        min = PHY_LANE_0;
        max = PHY_LANE_3;
      }

      if(check_prbs_all_lanes == false){
        min_port = tx_bist_phy_inst;
        max_port = tx_bist_phy_inst;
      }
      else{
        min_port = QCOM_AW_PHY_INST_FH0;
        max_port = QCOM_AW_PHY_INST_FH2;
      }

      for (j = min_port; j <= max_port; j++) {

        phy_inst_info = &phy_config_info->phy_inst_config_info[j];
        mss.phy_offset = phy_inst_info->base_addr;
        QCOM_AW_PHY_LOG_ERR("TX BIST for port %d", j);

        for (i = min; i <= max; i++) {
          QCOM_AW_PHY_LOG_ERR("TX BIST for lane %d", i);
          pmd_set_lane(&mss, i);
          if(enable_flag){
            aw_pmd_tx_gen_config_set(&mss, bist_pattern, user_data, user_data);
            aw_pmd_gen_tx_en_set(&mss, 1);
          }
          else{
            aw_pmd_gen_tx_en_set(&mss, 0);
          }
        }
      }
      break;

    case RX_CDR_CHECKER:
      phy_config_info = qcom_aw_phy_get_config_info();
      phy_inst_info = &phy_config_info->phy_inst_config_info[rx_bist_phy_inst];
      mss.phy_offset = phy_inst_info->base_addr;

      if(rx_bist_lane_num != PHY_LANE_MAX){
        min = rx_bist_lane_num;
        max = rx_bist_lane_num;
      }
      else{
        min = PHY_LANE_0;
        max = PHY_LANE_3;
      }

      for (i = min; i <= max; i++) {
        pmd_set_lane(&mss, i);
        if(AW_ERR_CODE_NONE == aw_pmd_rx_check_cdr_lock(&mss, RX_CDR_TIMEOUT_US)){
          QCOM_AW_PHY_LOG_INFO("RX CDR lock success for PHY %d, lane %d",
                               rx_bist_phy_inst, i);
        }
        else{
          QCOM_AW_PHY_LOG_INFO("RX CDR lock failure for PHY %d, lane %d",
                               rx_bist_phy_inst, i);
        }
      }
      break;

    case ENABLE_RX_BIST:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &enable_flag);
      else
      {
        error = true;
        break;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring RX BIST, enabled = %d", enable_flag);

      phy_config_info = qcom_aw_phy_get_config_info();

      if(rx_bist_lane_num != PHY_LANE_MAX){
        min = rx_bist_lane_num;
        max = rx_bist_lane_num;
      }
      else{
        min = PHY_LANE_0;
        max = PHY_LANE_3;
      }

      if(check_prbs_all_lanes == false){
        min_port = rx_bist_phy_inst;
        max_port = rx_bist_phy_inst;
      }
      else{
        min_port = QCOM_AW_PHY_INST_FH0;
        max_port = QCOM_AW_PHY_INST_FH2;
      }

      if(enable_flag){

        for (j = min_port; j <= max_port; j++) {
          phy_inst_info = &phy_config_info->phy_inst_config_info[j];
          mss.phy_offset = phy_inst_info->base_addr;
          QCOM_AW_PHY_LOG_ERR("RX BIST for port %d", j);

          for (i = min; i <= max; i++) {
            QCOM_AW_PHY_LOG_ERR("RX BIST checker for lane %d", i);
            pmd_set_lane(&mss, i);

            aw_pmd_rx_chk_config_set(&mss, bist_pattern, AW_DWELL,
                                     user_data, user_data, 2, 2000);
            aw_pmd_rx_chk_en_set(&mss, 1);
            CHECK(pmd_write_field(&mss, RX_DATABIST_TOP_REG1_ADDR,
                                  RX_DATABIST_TOP_REG1_BIST_ENABLE_A_MASK,
                                  RX_DATABIST_TOP_REG1_BIST_ENABLE_A_OFFSET, 0));
            CHECK(pmd_write_field(&mss, RX_DATABIST_TOP_REG1_ADDR,
                                  RX_DATABIST_TOP_REG1_BIST_ENABLE_A_MASK,
                                  RX_DATABIST_TOP_REG1_BIST_ENABLE_A_OFFSET, 1));
            CHECK(pmd_write_field(&mss, RX_DATABIST_TOP_REG1_ADDR,
                                  RX_DATABIST_TOP_REG1_ERROR_CNT_CLR_A_MASK,
                                  RX_DATABIST_TOP_REG1_ERROR_CNT_CLR_A_OFFSET, 0));
            CHECK(pmd_write_field(&mss, RX_DATABIST_TOP_REG1_ADDR,
                                  RX_DATABIST_TOP_REG1_ERROR_CNT_CLR_A_MASK,
                                  RX_DATABIST_TOP_REG1_ERROR_CNT_CLR_A_OFFSET, 1));
            CHECK(pmd_write_field(&mss, RX_DATABIST_TOP_REG1_ADDR,
                                  RX_DATABIST_TOP_REG1_ERROR_CNT_CLR_A_MASK,
                                  RX_DATABIST_TOP_REG1_ERROR_CNT_CLR_A_OFFSET, 0));
          }
        }

        USR_SLEEP(1000000 * measure_time);

        for (j = min_port; j <= max_port; j++) {
          phy_inst_info = &phy_config_info->phy_inst_config_info[j];
          mss.phy_offset = phy_inst_info->base_addr;
          QCOM_AW_PHY_LOG_ERR("RX BIST for port %d", j);
          if(check_prbs_all_lanes == false)
            tx_phy_inst_info = &phy_config_info->phy_inst_config_info[tx_bist_phy_inst];
          else
            tx_phy_inst_info = &phy_config_info->phy_inst_config_info[j];
          tx_mss.phy_offset = tx_phy_inst_info->base_addr;

          for (i = min; i <= max; i++) {
            QCOM_AW_PHY_LOG_ERR("RX BIST checker for lane %d", i);
            pmd_set_lane(&mss, i);
            if(tx_bist_lane_num != PHY_LANE_MAX)
              pmd_set_lane(&tx_mss, tx_bist_lane_num);
            else
              pmd_set_lane(&tx_mss, i);

            if(inject_error_count > 0){
              for (k = 0; k < inject_error_count; k++) {
                udelay(1);
                aw_pmd_tx_gen_err_inject_en_set(&tx_mss,1);
                udelay(1);
                aw_pmd_tx_gen_err_inject_en_set(&tx_mss,0);
              }
            }

            lane_index = (j*PHY_LANE_MAX) + i;
            err_count[lane_index] = 0;
            err_count_overflow[lane_index] = 0;
            ber[lane_index] = 0;

            CHECK(pmd_read_field(&mss, RX_DATABIST_TOP_RDREG3_ADDR,
                  RX_DATABIST_TOP_RDREG3_ERROR_CNT_55T32_NT_MASK,
                  RX_DATABIST_TOP_RDREG3_ERROR_CNT_55T32_NT_OFFSET,
                  &err_cnt_55_32));
            CHECK(pmd_read_field(&mss, RX_DATABIST_TOP_RDREG2_ADDR,
                  RX_DATABIST_TOP_RDREG2_ERROR_CNT_NT_MASK,
                  RX_DATABIST_TOP_RDREG2_ERROR_CNT_NT_OFFSET,
                  &err_cnt_31_0));
            err_count[lane_index] += (uint64_t)err_cnt_55_32 << 32 | (uint64_t)err_cnt_31_0;
            CHECK(pmd_read_check_field(
                  &mss, RX_DATABIST_TOP_RDREG1_ADDR,
                  RX_DATABIST_TOP_RDREG1_ERROR_CNT_OVERFLOW_NT_MASK,
                  RX_DATABIST_TOP_RDREG1_ERROR_CNT_OVERFLOW_NT_OFFSET, RD_EQ,
                  &err_count_overflow[lane_index], 0, 0));
            ber[lane_index] += (uint64_t)err_count[lane_index] / (measure_time * 25);
            QCOM_AW_PHY_LOG_ERR("err_count = %lu\n", err_count[lane_index]);
            QCOM_AW_PHY_LOG_ERR("err_count_overflow = %d\n", err_count_overflow[lane_index]);
            QCOM_AW_PHY_LOG_ERR("ber = %lu\n", ber[lane_index]);
          }
        }
      }
      else{
        for (j = min_port; j <= max_port; j++) {
          phy_inst_info = &phy_config_info->phy_inst_config_info[j];
          mss.phy_offset = phy_inst_info->base_addr;
          QCOM_AW_PHY_LOG_ERR("RX BIST for port %d", j);

          for (i = min; i <= max; i++) {
            QCOM_AW_PHY_LOG_ERR("Disabling RX BIST for lane %d", i);
            pmd_set_lane(&mss, i);
            aw_pmd_rx_chk_en_set(&mss, 0);
          }
        }
      }

      break;

    case CHECK_PRBS_ALL_LANES:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &enable_flag);
      else
      {
        error = true;
        break;
      }

      QCOM_AW_PHY_LOG_ERR("Checking PRBS for all lanes = %d", enable_flag);
      check_prbs_all_lanes = enable_flag;
      break;

    case SET_EQ_MODE:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &eq_mode);
      else
      {
        error = true;
        break;
      }

      QCOM_AW_PHY_LOG_ERR("Equalization mode set to %d", eq_mode);

      phy_config_info = qcom_aw_phy_get_config_info();
      for (i = QCOM_AW_PHY_INST_FH0; i < QCOM_AW_PHY_INST_MAX; i++) {
        phy_inst_info = &phy_config_info->phy_inst_config_info[i];
        phy_inst_info->phy_eq_mode = eq_mode;
      }
      break;

    case SET_PORT_CONFIG_MASK:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &port_config_mask);
      else
      {
        error = true;
        break;
      }

      QCOM_AW_PHY_LOG_ERR("Port config mask set to 0x%x", port_config_mask);
      break;


    case INITIATE_AN:
      QCOM_AW_PHY_LOG_ERR("Initiate AN");
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_initiate_an(port_type, 0,
                                                             num_lanes,
                                                             port_config_mask);
      break;

    case RESET_PHY_SM:
      QCOM_AW_PHY_LOG_ERR("Reset PHY SM");
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_reset_phy_sm(port_type);
      break;

    default:
      error = true;
      break;
  }

  if(error)
  {
    QCOM_AW_PHY_LOG_ERR("Invalid input, \"cat /sys/kernel/debug/qcom_aw_phy_test/qcom_aw_phy\" for help menu");
	return -EFAULT;
  }
  return count;
}

ssize_t qcom_aw_phy_set_tx_eq_val(struct file *file, const char __user *buf,
                             size_t count, loff_t *ppos) {
  char *token;
  char token_string[100];
  char *save_ptr = NULL;
  struct qcom_aw_phy_config *phy_config_info = NULL;
  struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
  mss_access_t mss = {.phy_offset = 0, .lane_offset = 0};
  aw_txfir_config_t txfir_cfg = {0};

  memset(token_string, 0, sizeof(token_string));
  if (copy_from_user(&token_string, buf, MIN(sizeof(token_string), count))){
    QCOM_AW_PHY_LOG_ERR("Copy from user failed");
    return -EFAULT;
  }

  token = qcom_aw_phy_strtok(token_string, ',', &save_ptr);
  if(token!=NULL)
    sscanf(token, "%d", &txfir_cfg.CM3);
  else
  {
    QCOM_AW_PHY_LOG_ERR("Invalid Input\n");
    return -EFAULT;
  }

  token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
  if(token!=NULL)
    sscanf(token, "%d", &txfir_cfg.CM2);
  else
  {
    QCOM_AW_PHY_LOG_ERR("Invalid Input\n");
    return -EFAULT;
  }

  token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
  if(token!=NULL)
    sscanf(token, "%d", &txfir_cfg.CM1);
  else
  {
    QCOM_AW_PHY_LOG_ERR("Invalid Input\n");
    return -EFAULT;
  }

  token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
  if(token!=NULL)
    sscanf(token, "%d", &txfir_cfg.C0);
  else
  {
    QCOM_AW_PHY_LOG_ERR("Invalid Input\n");
    return -EFAULT;
  }

  token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
  if(token!=NULL)
    sscanf(token, "%d", &txfir_cfg.C1);
  else
  {
    QCOM_AW_PHY_LOG_ERR("Invalid Input\n");
    return -EFAULT;
  }

  token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
  if(token!=NULL)
    sscanf(token, "%d", &txfir_cfg.C2);
  else
  {
    QCOM_AW_PHY_LOG_ERR("Invalid Input\n");
    return -EFAULT;
  }

  token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
  if(token!=NULL)
    sscanf(token, "%d", &txfir_cfg.C3);
  else
  {
    QCOM_AW_PHY_LOG_ERR("Invalid Input\n");
    return -EFAULT;
  }

  token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
  if(token!=NULL)
    sscanf(token, "%d", &txfir_cfg.main_or_max);
  else
  {
    QCOM_AW_PHY_LOG_ERR("Invalid Input\n");
    return -EFAULT;
  }


  QCOM_AW_PHY_LOG_ERR("TX EQ values for PHY instance %d, lane %d, "
                      "CM3=%d, CM2=%d, CM1=%d, C0=%d, C1=%d, C2=%d, C3=%d, "
                      "main_or_max=%d\n",
                      tx_bist_phy_inst, tx_bist_lane_num,
                      txfir_cfg.CM3, txfir_cfg.CM2, txfir_cfg.CM1,
                      txfir_cfg.C0, txfir_cfg.C1, txfir_cfg.C2, txfir_cfg.C3,
                      txfir_cfg.main_or_max);

  phy_config_info = qcom_aw_phy_get_config_info();
  phy_inst_info = &phy_config_info->phy_inst_config_info[tx_bist_phy_inst];
  mss.phy_offset = phy_inst_info->base_addr;
  pmd_set_lane(&mss, tx_bist_lane_num);

  aw_pmd_txfir_config_set(&mss, &txfir_cfg, 1);

  tx_fir_cfg_cache[tx_bist_phy_inst][tx_bist_lane_num] = txfir_cfg;
  tx_fir_cfg_cache_valid[tx_bist_phy_inst][tx_bist_lane_num] = true;

  return count;
}

ssize_t qcom_aw_phy_get_tx_eq_val(struct file *file, char __user *buf,
                                    size_t count, loff_t *ppos){
  struct qcom_aw_phy_config *phy_config_info = NULL;
  struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
  mss_access_t mss = {.phy_offset = 0, .lane_offset = 0};
  aw_txfir_config_t txfir_cfg = {0};
  char dbg_buf[200] = {0};
  int nbytes = 0;

  phy_config_info = qcom_aw_phy_get_config_info();
  phy_inst_info = &phy_config_info->phy_inst_config_info[tx_bist_phy_inst];
  mss.phy_offset = phy_inst_info->base_addr;
  pmd_set_lane(&mss, tx_bist_lane_num);

  if(tx_fir_cfg_cache_valid[tx_bist_phy_inst][tx_bist_lane_num])
    txfir_cfg.main_or_max = tx_fir_cfg_cache[tx_bist_phy_inst][tx_bist_lane_num].main_or_max;
  else
    txfir_cfg.main_or_max = 1;

  aw_pmd_txfir_config_get(&mss, &txfir_cfg);

  nbytes += scnprintf(dbg_buf, 200,
                      "TX EQ values for PHY instance %d, lane %d, "
                      "CM3=%d, CM2=%d, CM1=%d, C0=%d, C1=%d,C2=%d, C3=%d, "
                      "main_or_max=%d\n",
                      tx_bist_phy_inst, tx_bist_lane_num,
                      txfir_cfg.CM3, txfir_cfg.CM2, txfir_cfg.CM1,
                      txfir_cfg.C0, txfir_cfg.C1, txfir_cfg.C2, txfir_cfg.C3,
                      txfir_cfg.main_or_max);

  return simple_read_from_buffer(buf, count, ppos, dbg_buf, nbytes);
}

bool qcom_aw_phy_debugfs_get_tx_fir_val(enum qcom_aw_phy_instance_enum tx_bist_phy_inst,
                    enum eth_phy_iface_phy_lane_num_enum lane, void* txfir_cfg){
  if(!QCOM_AW_PHY_INST_VALID(tx_bist_phy_inst))
    return false;

  if(tx_fir_cfg_cache_valid[tx_bist_phy_inst][lane]){
    memcpy(txfir_cfg , &tx_fir_cfg_cache[tx_bist_phy_inst][lane], sizeof(aw_txfir_config_t));
    return true;
  }

  return false;
}

void loopback_mode_parser(char *str, int size)
{
	int mode = 0;
	int global_mode = qcom_aw_phy_get_loopback_mode();
	enum qcom_aw_phy_instance_enum phy_inst;
	enum eth_phy_iface_phy_lane_num_enum lane;
	int offset = size;
	
	/* Display global mode first */
	switch (global_mode) {
		case 0:
			offset += scnprintf(str+offset, 60, "Global Mode: NO_LB (all lanes forced)\n\n");
			break;
		case 1:
			offset += scnprintf(str+offset, 60, "Global Mode: NEAR_END_SERIAL_LB (all lanes forced)\n\n");
			break;
		case 2:
			offset += scnprintf(str+offset, 60, "Global Mode: NEAR_END_PARALLEL_LB (all lanes forced)\n\n");
			break;
		case 3:
			offset += scnprintf(str+offset, 60, "Global Mode: DEFAULT (per-lane control)\n\n");
			/* Display per-lane status only in DEFAULT mode */
			for (phy_inst = QCOM_AW_PHY_INST_FH0; phy_inst <= QCOM_AW_PHY_INST_FH2; phy_inst++) {
				for (lane = 0; lane < PHY_LANE_MAX; lane++) {
					mode = qcom_aw_phy_get_effective_loopback_mode(phy_inst, lane);
					if (mode != 0) {  /* Only show non-NO_LB lanes */
						offset += scnprintf(str+offset, 60, "  PHY%d Lane%d: %s\n", 
							phy_inst, lane, 
							mode == 1 ? "SERIAL_LB" : "PARALLEL_LB");
					}
				}
			}
			break;
		default:
			offset += scnprintf(str+offset, 60, "Global Mode: Unknown\n\n");
			break;
	}
}

static ssize_t get_aw_loopback_mode(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[1024]={0};
	uint32_t ret_val = 0;
	loopback_mode_parser(phy_status_str,strlen(phy_status_str));
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 1024);
	return ret_val;
}

void ref_clk_mode_parser(char *str, int size)
{
	int mode=0;
	mode = qcom_aw_phy_get_ref_clk_mode();
	switch (mode)
	{
		case 0:
			scnprintf(str+size,30,"Ref Clock Mode = %s\n","SILABS");
			break;
		case 1:
			scnprintf(str+size,30,"Ref Clock Mode = %s\n","OSCILLATOR");
			break;
		default:
			scnprintf(str+size,30,"Ref Clock Mode = %s\n","Unknown");
			break;
	}
}

static ssize_t get_aw_ref_clock(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	ref_clk_mode_parser(phy_status_str,strlen(phy_status_str));
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_no_of_ports(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_config *phy_config_info = NULL;
	phy_config_info=qcom_aw_phy_get_config_info();
	if(phy_config_info!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Total Ports = %d\n",phy_config_info->num_phy_instances);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_fw_ver(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_config *phy_config_info = NULL;
	phy_config_info=qcom_aw_phy_get_config_info();
	if(phy_config_info!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 60, "Major:Minor:Patch = %d:%d:%d\n",phy_config_info->fw_major_ver,phy_config_info->fw_minor_ver,phy_config_info->fw_patch_ver);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

void eq_mode_parser(char *str, int size, enum qcom_aw_phy_eq_mode_enum phy_eq_mode)
{
	switch (phy_eq_mode)
	{
		case 1:
			scnprintf(str+size,30, "Eq Mode = %s\n","MANUAL_EQ_MODE");
			break;
		case 2:
			scnprintf(str+size,30, "Eq Mode = %s\n","ANLT_MODE");
			break;
		case 3:
			scnprintf(str+size,30, "Eq Mode = %s\n","LT_MODE");
			break;
		default:
			scnprintf(str+size,30, "Eq Mode = %s\n","Unknown");
			break;
	}
}

void lane_speed_parser(char *str, int size, enum eth_phy_iface_phy_lane_speed_enum lane_speed)
{
	switch (lane_speed)
	{
		case 0:
			scnprintf(str+size,20, "Speed = %s\n","10G");
			 break;
		case 1:
			scnprintf(str+size,20, "Speed = %s\n","25G");
			break;
		case 2:
			scnprintf(str+size,20, "Speed = %s\n","50G");
			break;
		case 3:
			scnprintf(str+size,20, "Speed = %s\n","100G");
			break;
		default:
			scnprintf(str+size,20, "Speed = %s\n","Unknown");
			break;
	}
}

void cdr_lock_parser(char *str, int size, enum qcom_aw_phy_cdr_lock_lane_status cdr_lock)
{
	switch (cdr_lock)
	{
		case -1:
			scnprintf(str+size,40, "CDR Lock Status = %s\n","NONE");
			break;
		case 0:
			scnprintf(str+size,40, "CDR Lock Status = %s\n","FAILURE");
			break;
		case 1:
			scnprintf(str+size,40, "CDR Lock Status = %s\n","SUCCESS");
			break;
		default:
			scnprintf(str+size,40, "CDR Lock Status = %s\n","Unknown");
			break;
	}
}

static ssize_t get_aw_phy0_eq_mode(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH0);
	if(phy_inst_info!=NULL){
		eq_mode_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->phy_eq_mode);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_bu_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH0);
	if(phy_inst_info!=NULL){
		if(phy_inst_info->bring_up_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 50, "Bringup Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 50, "Bringup Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lanes_enabled(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH0);
	if(phy_inst_info!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 50, "Num of lanes Enabled = %d\n",phy_inst_info->num_lanes);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_sfp_port_type(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH0);
	if(phy_inst_info!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 50, "SFP Port Type = %d\n",phy_inst_info->sfp_port_type);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane0_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane0_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane0_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH0);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_0]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane0_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane0_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}


static ssize_t get_aw_phy0_lane1_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane1_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane1_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH0);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_1]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane1_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane1_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane2_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane2_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane2_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH0);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_2]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane2_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane2_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane3_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane3_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane3_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH0);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_3]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane3_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy0_lane3_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH0,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_eq_mode(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH1);
	if(phy_inst_info!=NULL){
		eq_mode_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->phy_eq_mode);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_bu_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH1);
	if(phy_inst_info!=NULL){
		if(phy_inst_info->bring_up_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 50, "Bringup Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 50, "Bringup Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lanes_enabled(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH1);
	if(phy_inst_info!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 50, "Num of lanes Enabled = %d\n",phy_inst_info->num_lanes);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_sfp_port_type(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH1);
	if(phy_inst_info!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 50, "SFP Port Type = %d\n",phy_inst_info->sfp_port_type);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane0_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane0_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane0_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH1);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_0]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane0_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane0_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane1_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane1_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane1_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH1);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_1]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane1_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane1_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane2_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane2_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane2_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH1);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_2]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane2_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane2_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane3_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane3_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane3_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH1);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_3]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane3_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy1_lane3_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH1,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_eq_mode(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH2);
	if(phy_inst_info!=NULL){
		eq_mode_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->phy_eq_mode);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_bu_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH2);
	if(phy_inst_info!=NULL){
		if(phy_inst_info->bring_up_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 50, "Bringup Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 50, "Bringup Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lanes_enabled(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH2);
	if(phy_inst_info!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 50, "Num of lanes Enabled = %d\n",phy_inst_info->num_lanes);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_sfp_port_type(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH2);
	if(phy_inst_info!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 50, "SFP Port Type = %d\n",phy_inst_info->sfp_port_type);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane0_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane0_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane0_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH2);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_0]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane0_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane0_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane1_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane1_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane1_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH2);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_1]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane1_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane1_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane2_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane2_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane2_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH2);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_2]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane2_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane2_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane3_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane3_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane3_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_FH2);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_3]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane3_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy2_lane3_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_FH2,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_eq_mode(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_L2_C2C);
	if(phy_inst_info!=NULL){
		eq_mode_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->phy_eq_mode);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_bu_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_L2_C2C);
	if(phy_inst_info!=NULL){
		if(phy_inst_info->bring_up_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 50, "Bringup Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 50, "Bringup Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lanes_enabled(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_L2_C2C);
	if(phy_inst_info!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 50, "Num of lanes Enabled = %d\n",phy_inst_info->num_lanes);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_sfp_port_type(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_L2_C2C);
	if(phy_inst_info!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 50, "SFP Port Type = %d\n",phy_inst_info->sfp_port_type);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane0_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane0_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane0_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_L2_C2C);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_0]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane0_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane0_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane1_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane1_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane1_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_L2_C2C);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_1]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane1_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane1_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane2_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane2_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane2_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_L2_C2C);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_2]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane2_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane2_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane3_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane3_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane3_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_L2_C2C);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_3]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane3_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy3_lane3_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_L2_C2C,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_eq_mode(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_DEBUG_C2C);
	if(phy_inst_info!=NULL){
		eq_mode_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->phy_eq_mode);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_bu_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_DEBUG_C2C);
	if(phy_inst_info!=NULL){
		if(phy_inst_info->bring_up_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 50, "Bringup Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 50, "Bringup Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lanes_enabled(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_DEBUG_C2C);
	if(phy_inst_info!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 50, "Num of lanes Enabled = %d\n",phy_inst_info->num_lanes);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_sfp_port_type(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_DEBUG_C2C);
	if(phy_inst_info!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 50, "SFP Port Type = %d\n",phy_inst_info->sfp_port_type);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane0_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane0_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane0_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_DEBUG_C2C);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_0]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane0_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane0_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_0);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane1_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane1_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane1_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_DEBUG_C2C);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_1]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane1_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane1_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_1);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane2_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane2_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;;
}

static ssize_t get_aw_phy4_lane2_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_DEBUG_C2C);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_2]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane2_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane2_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_2);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane3_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->lane_config.lane_enabled)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "Enable Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane3_speed(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		lane_speed_parser(phy_status_str,strlen(phy_status_str),phy_lane_params->lane_config.lane_speed);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane3_cdr_lock_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	phy_inst_info = qcom_aw_phy_get_inst_config(QCOM_AW_PHY_INST_DEBUG_C2C);
	if(phy_inst_info!=NULL){
		cdr_lock_parser(phy_status_str,strlen(phy_status_str),phy_inst_info->cdr_lock_status_flag[PHY_LANE_3]);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane3_link_status(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		if(phy_lane_params->link_status)
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","TRUE");
		else
			scnprintf(phy_status_str + strlen(phy_status_str), 30, "PCS Link Status = %s\n","FALSE");
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static ssize_t get_aw_phy4_lane3_mac_link_index(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char phy_status_str[100]={0};
	uint32_t ret_val = 0;
	struct qcom_aw_lane_params *phy_lane_params = NULL;
	phy_lane_params=qcom_aw_phy_get_lane_params(QCOM_AW_PHY_INST_DEBUG_C2C,PHY_LANE_3);
	if(phy_lane_params!=NULL){
		scnprintf(phy_status_str + strlen(phy_status_str), 30, "Mac Link Index = %d\n",phy_lane_params->lane_config.link_index);
	}
	ret_val=simple_read_from_buffer(buf, count, ppos, phy_status_str, 100);
	return ret_val;
}

static struct file_operations dummy;
static struct file_operations aw_loopback_mode_fs_ops = {
	.read = get_aw_loopback_mode,
};

static struct file_operations aw_ref_clock_fs_ops = {
	.read = get_aw_ref_clock,
};

static struct file_operations aw_no_of_ports_fs_ops = {
	.read = get_aw_no_of_ports,
};

static struct file_operations aw_fw_ver_fs_ops = {
	.read = get_aw_fw_ver,
};

static struct file_operations aw_phy0_eq_mode_fs_ops = {
	.read = get_aw_phy0_eq_mode,
};

static struct file_operations aw_phy0_bu_status_fs_ops = {
	.read = get_aw_phy0_bu_status,
};

static struct file_operations aw_phy0_lanes_enabled_fs_ops = {
	.read = get_aw_phy0_lanes_enabled,
};

static struct file_operations aw_phy0_sfp_port_type_fs_ops = {
	.read = get_aw_phy0_sfp_port_type,
};

static struct file_operations aw_phy0_lane0_status_fs_ops = {
	.read = get_aw_phy0_lane0_status,
};

static struct file_operations aw_phy0_lane0_speed_fs_ops = {
	.read = get_aw_phy0_lane0_speed,
};

static struct file_operations aw_phy0_lane0_cdr_lock_status_fs_ops = {
	.read = get_aw_phy0_lane0_cdr_lock_status,
};

static struct file_operations aw_phy0_lane0_link_status_fs_ops = {
	.read = get_aw_phy0_lane0_link_status,
};

static struct file_operations aw_phy0_lane0_mac_link_index_fs_ops = {
	.read = get_aw_phy0_lane0_mac_link_index,
};

static struct file_operations aw_phy0_lane1_status_fs_ops = {
	.read = get_aw_phy0_lane1_status,
};

static struct file_operations aw_phy0_lane1_speed_fs_ops = {
	.read = get_aw_phy0_lane1_speed,
};

static struct file_operations aw_phy0_lane1_cdr_lock_status_fs_ops = {
	.read = get_aw_phy0_lane1_cdr_lock_status,
};

static struct file_operations aw_phy0_lane1_link_status_fs_ops = {
	.read = get_aw_phy0_lane1_link_status,
};

static struct file_operations aw_phy0_lane1_mac_link_index_fs_ops = {
	.read = get_aw_phy0_lane1_mac_link_index,
};

static struct file_operations aw_phy0_lane2_status_fs_ops = {
	.read = get_aw_phy0_lane2_status,
};

static struct file_operations aw_phy0_lane2_speed_fs_ops = {
	.read = get_aw_phy0_lane2_speed,
};

static struct file_operations aw_phy0_lane2_cdr_lock_status_fs_ops = {
	.read = get_aw_phy0_lane2_cdr_lock_status,
};

static struct file_operations aw_phy0_lane2_link_status_fs_ops = {
	.read = get_aw_phy0_lane2_link_status,
};

static struct file_operations aw_phy0_lane2_mac_link_index_fs_ops = {
	.read = get_aw_phy0_lane2_mac_link_index,
};

static struct file_operations aw_phy0_lane3_status_fs_ops = {
	.read = get_aw_phy0_lane3_status,
};

static struct file_operations aw_phy0_lane3_speed_fs_ops = {
	.read = get_aw_phy0_lane3_speed,
};

static struct file_operations aw_phy0_lane3_cdr_lock_status_fs_ops = {
	.read = get_aw_phy0_lane3_cdr_lock_status,
};

static struct file_operations aw_phy0_lane3_link_status_fs_ops = {
	.read = get_aw_phy0_lane3_link_status,
};

static struct file_operations aw_phy0_lane3_mac_link_index_fs_ops = {
	.read = get_aw_phy0_lane3_mac_link_index,
};

static struct file_operations aw_phy1_eq_mode_fs_ops = {
	.read = get_aw_phy1_eq_mode,
};

static struct file_operations aw_phy1_bu_status_fs_ops = {
	.read = get_aw_phy1_bu_status,
};

static struct file_operations aw_phy1_lanes_enabled_fs_ops = {
	.read = get_aw_phy1_lanes_enabled,
};

static struct file_operations aw_phy1_sfp_port_type_fs_ops = {
	.read = get_aw_phy1_sfp_port_type,
};

static struct file_operations aw_phy1_lane0_status_fs_ops = {
	.read = get_aw_phy1_lane0_status,
};

static struct file_operations aw_phy1_lane0_speed_fs_ops = {
	.read = get_aw_phy1_lane0_speed,
};

static struct file_operations aw_phy1_lane0_cdr_lock_status_fs_ops = {
	.read = get_aw_phy1_lane0_cdr_lock_status,
};

static struct file_operations aw_phy1_lane0_link_status_fs_ops = {
	.read = get_aw_phy1_lane0_link_status,
};

static struct file_operations aw_phy1_lane0_mac_link_index_fs_ops = {
	.read = get_aw_phy1_lane0_mac_link_index,
};

static struct file_operations aw_phy1_lane1_status_fs_ops = {
	.read = get_aw_phy1_lane1_status,
};

static struct file_operations aw_phy1_lane1_speed_fs_ops = {
	.read = get_aw_phy1_lane1_speed,
};

static struct file_operations aw_phy1_lane1_cdr_lock_status_fs_ops = {
	.read = get_aw_phy1_lane1_cdr_lock_status,
};

static struct file_operations aw_phy1_lane1_link_status_fs_ops = {
	.read = get_aw_phy1_lane1_link_status,
};

static struct file_operations aw_phy1_lane1_mac_link_index_fs_ops = {
	.read = get_aw_phy1_lane1_mac_link_index,
};

static struct file_operations aw_phy1_lane2_status_fs_ops = {
	.read = get_aw_phy1_lane2_status,
};

static struct file_operations aw_phy1_lane2_speed_fs_ops = {
	.read = get_aw_phy1_lane2_speed,
};

static struct file_operations aw_phy1_lane2_cdr_lock_status_fs_ops = {
	.read = get_aw_phy1_lane2_cdr_lock_status,
};

static struct file_operations aw_phy1_lane2_link_status_fs_ops = {
	.read = get_aw_phy1_lane2_link_status,
};

static struct file_operations aw_phy1_lane2_mac_link_index_fs_ops = {
	.read = get_aw_phy1_lane2_mac_link_index,
};

static struct file_operations aw_phy1_lane3_speed_fs_ops = {
	.read = get_aw_phy1_lane3_speed,
};

static struct file_operations aw_phy1_lane3_cdr_lock_status_fs_ops = {
	.read = get_aw_phy1_lane3_cdr_lock_status,
};

static struct file_operations aw_phy1_lane3_link_status_fs_ops = {
	.read = get_aw_phy1_lane3_link_status,
};

static struct file_operations aw_phy1_lane3_mac_link_index_fs_ops = {
	.read = get_aw_phy1_lane3_mac_link_index,
};

static struct file_operations aw_phy1_lane3_status_fs_ops = {
	.read = get_aw_phy1_lane3_status,
};

static struct file_operations aw_phy2_eq_mode_fs_ops = {
	.read = get_aw_phy2_eq_mode,
};

static struct file_operations aw_phy2_bu_status_fs_ops = {
	.read = get_aw_phy2_bu_status,
};

static struct file_operations aw_phy2_lanes_enabled_fs_ops = {
	.read = get_aw_phy2_lanes_enabled,
};

static struct file_operations aw_phy2_sfp_port_type_fs_ops = {
	.read = get_aw_phy2_sfp_port_type,
};

static struct file_operations aw_phy2_lane0_status_fs_ops = {
	.read = get_aw_phy2_lane0_status,
};

static struct file_operations aw_phy2_lane0_speed_fs_ops = {
	.read = get_aw_phy2_lane0_speed,
};

static struct file_operations aw_phy2_lane0_cdr_lock_status_fs_ops = {
	.read = get_aw_phy2_lane0_cdr_lock_status,
};

static struct file_operations aw_phy2_lane0_link_status_fs_ops = {
	.read = get_aw_phy2_lane0_link_status,
};

static struct file_operations aw_phy2_lane0_mac_link_index_fs_ops = {
	.read = get_aw_phy2_lane0_mac_link_index,
};

static struct file_operations aw_phy2_lane1_status_fs_ops = {
	.read = get_aw_phy2_lane1_status,
};

static struct file_operations aw_phy2_lane1_speed_fs_ops = {
	.read = get_aw_phy2_lane1_speed,
};

static struct file_operations aw_phy2_lane1_cdr_lock_status_fs_ops = {
	.read = get_aw_phy2_lane1_cdr_lock_status,
};

static struct file_operations aw_phy2_lane1_link_status_fs_ops = {
	.read = get_aw_phy2_lane1_link_status,
};

static struct file_operations aw_phy2_lane1_mac_link_index_fs_ops = {
	.read = get_aw_phy2_lane1_mac_link_index,
};

static struct file_operations aw_phy2_lane2_status_fs_ops = {
	.read = get_aw_phy2_lane2_status,
};

static struct file_operations aw_phy2_lane2_speed_fs_ops = {
	.read = get_aw_phy2_lane2_speed,
};

static struct file_operations aw_phy2_lane2_cdr_lock_status_fs_ops = {
	.read = get_aw_phy2_lane2_cdr_lock_status,
};

static struct file_operations aw_phy2_lane2_link_status_fs_ops = {
	.read = get_aw_phy2_lane2_link_status,
};

static struct file_operations aw_phy2_lane2_mac_link_index_fs_ops = {
	.read = get_aw_phy2_lane2_mac_link_index,
};

static struct file_operations aw_phy2_lane3_status_fs_ops = {
	.read = get_aw_phy2_lane3_status,
};

static struct file_operations aw_phy2_lane3_speed_fs_ops = {
	.read = get_aw_phy2_lane3_speed,
};

static struct file_operations aw_phy2_lane3_cdr_lock_status_fs_ops = {
	.read = get_aw_phy2_lane3_cdr_lock_status,
};

static struct file_operations aw_phy2_lane3_link_status_fs_ops = {
	.read = get_aw_phy2_lane3_link_status,
};

static struct file_operations aw_phy2_lane3_mac_link_index_fs_ops = {
	.read = get_aw_phy2_lane3_mac_link_index,
};

static struct file_operations aw_phy3_eq_mode_fs_ops = {
	.read = get_aw_phy3_eq_mode,
};

static struct file_operations aw_phy3_bu_status_fs_ops = {
	.read = get_aw_phy3_bu_status,
};

static struct file_operations aw_phy3_lanes_enabled_fs_ops = {
	.read = get_aw_phy3_lanes_enabled,
};

static struct file_operations aw_phy3_sfp_port_type_fs_ops = {
	.read = get_aw_phy3_sfp_port_type,
};

static struct file_operations aw_phy3_lane0_status_fs_ops = {
	.read = get_aw_phy3_lane0_status,
};

static struct file_operations aw_phy3_lane0_speed_fs_ops = {
	.read = get_aw_phy3_lane0_speed,
};

static struct file_operations aw_phy3_lane0_cdr_lock_status_fs_ops = {
	.read = get_aw_phy3_lane0_cdr_lock_status,
};

static struct file_operations aw_phy3_lane0_link_status_fs_ops = {
	.read = get_aw_phy3_lane0_link_status,
};

static struct file_operations aw_phy3_lane0_mac_link_index_fs_ops = {
	.read = get_aw_phy3_lane0_mac_link_index,
};

static struct file_operations aw_phy3_lane1_status_fs_ops = {
	.read = get_aw_phy3_lane1_status,
};

static struct file_operations aw_phy3_lane1_speed_fs_ops = {
	.read = get_aw_phy3_lane1_speed,
};

static struct file_operations aw_phy3_lane1_cdr_lock_status_fs_ops = {
	.read = get_aw_phy3_lane1_cdr_lock_status,
};

static struct file_operations aw_phy3_lane1_link_status_fs_ops = {
	.read = get_aw_phy3_lane1_link_status,
};

static struct file_operations aw_phy3_lane1_mac_link_index_fs_ops = {
	.read = get_aw_phy3_lane1_mac_link_index,
};

static struct file_operations aw_phy3_lane2_status_fs_ops = {
	.read = get_aw_phy3_lane2_status,
};

static struct file_operations aw_phy3_lane2_speed_fs_ops = {
	.read = get_aw_phy3_lane2_speed,
};

static struct file_operations aw_phy3_lane2_cdr_lock_status_fs_ops = {
	.read = get_aw_phy3_lane2_cdr_lock_status,
};

static struct file_operations aw_phy3_lane2_link_status_fs_ops = {
	.read = get_aw_phy3_lane2_link_status,
};

static struct file_operations aw_phy3_lane2_mac_link_index_fs_ops = {
	.read = get_aw_phy3_lane2_mac_link_index,
};

static struct file_operations aw_phy3_lane3_status_fs_ops = {
	.read = get_aw_phy3_lane3_status,
};

static struct file_operations aw_phy3_lane3_speed_fs_ops = {
	.read = get_aw_phy3_lane3_speed,
};

static struct file_operations aw_phy3_lane3_cdr_lock_status_fs_ops = {
	.read = get_aw_phy3_lane3_cdr_lock_status,
};

static struct file_operations aw_phy3_lane3_link_status_fs_ops = {
	.read = get_aw_phy3_lane3_link_status,
};

static struct file_operations aw_phy3_lane3_mac_link_index_fs_ops = {
	.read = get_aw_phy3_lane3_mac_link_index,
};

static struct file_operations aw_phy4_eq_mode_fs_ops = {
	.read = get_aw_phy4_eq_mode,
};

static struct file_operations aw_phy4_bu_status_fs_ops = {
	.read = get_aw_phy4_bu_status,
};

static struct file_operations aw_phy4_lanes_enabled_fs_ops = {
	.read = get_aw_phy4_lanes_enabled,
};

static struct file_operations aw_phy4_sfp_port_type_fs_ops = {
	.read = get_aw_phy4_sfp_port_type,
};
static struct file_operations aw_phy4_lane0_status_fs_ops = {
	.read = get_aw_phy4_lane0_status,
};

static struct file_operations aw_phy4_lane0_speed_fs_ops = {
	.read = get_aw_phy4_lane0_speed,
};

static struct file_operations aw_phy4_lane0_cdr_lock_status_fs_ops = {
	.read = get_aw_phy4_lane0_cdr_lock_status,
};

static struct file_operations aw_phy4_lane0_mac_link_index_fs_ops = {
	.read = get_aw_phy4_lane0_mac_link_index,
};

static struct file_operations aw_phy4_lane0_link_status_fs_ops = {
	.read = get_aw_phy4_lane0_link_status,
};

static struct file_operations aw_phy4_lane1_status_fs_ops = {
	.read = get_aw_phy4_lane1_status,
};

static struct file_operations aw_phy4_lane1_speed_fs_ops = {
	.read = get_aw_phy4_lane1_speed,
};

static struct file_operations aw_phy4_lane1_cdr_lock_status_fs_ops = {
	.read = get_aw_phy4_lane1_cdr_lock_status,
};

static struct file_operations aw_phy4_lane1_link_status_fs_ops = {
	.read = get_aw_phy4_lane1_link_status,
};

static struct file_operations aw_phy4_lane1_mac_link_index_fs_ops = {
	.read = get_aw_phy4_lane1_mac_link_index,
};

static struct file_operations aw_phy4_lane2_status_fs_ops = {
	.read = get_aw_phy4_lane2_status,
};

static struct file_operations aw_phy4_lane2_speed_fs_ops = {
	.read = get_aw_phy4_lane2_speed,
};

static struct file_operations aw_phy4_lane2_cdr_lock_status_fs_ops = {
	.read = get_aw_phy4_lane2_cdr_lock_status,
};

static struct file_operations aw_phy4_lane2_link_status_fs_ops = {
	.read = get_aw_phy4_lane2_link_status,
};

static struct file_operations aw_phy4_lane2_mac_link_index_fs_ops = {
	.read = get_aw_phy4_lane2_mac_link_index,
};

static struct file_operations aw_phy4_lane3_status_fs_ops = {
	.read = get_aw_phy4_lane3_status,
};

static struct file_operations aw_phy4_lane3_speed_fs_ops = {
	.read = get_aw_phy4_lane3_speed,
};

static struct file_operations aw_phy4_lane3_cdr_lock_status_fs_ops = {
	.read = get_aw_phy4_lane3_cdr_lock_status,
};

static struct file_operations aw_phy4_lane3_link_status_fs_ops = {
	.read = get_aw_phy4_lane3_link_status,
};

static struct file_operations aw_phy4_lane3_mac_link_index_fs_ops = {
	.read = get_aw_phy4_lane3_mac_link_index,
};

/*
 * only white listed alphbates are allowed
 * { } , _  : and 0 to 9 a to z A to Z
 */
/*****************************************************************************/
static void remove_whitespaces(char * str, uint32_t len)
{
	int i,j;

	if(!str){
		QCOM_AW_PHY_LOG_ERR("Null pointer Input\n");
		return;
	}
	for(i = 0, j=0; str[i] != '\0'; i++)
	{
		if(i > len)
		{
			QCOM_AW_PHY_LOG_ERR("Invalid Input\n");
			return;
		}
		if((str[i] >= 'a' && str[i] <= 'z') ||
			(str[i] >= 'A' && str[i] <='Z') ||
			(str[i] >= '0' && str[i] <= '9') ||
			(str[i] == '}') ||
			(str[i] == '{') ||
			(str[i] == ',') ||
			(str[i] == '_') ||
			(str[i] == ':'))
		{
			str[j++]= str[i];
		}
	}
	str[j] = 0;
}
static char string_parser(parser_s *parser_in)
{
	int i,j,k;

	if(!parser_in || !parser_in->source || !parser_in->token || !parser_in->delim)
		return 0;

	memset(parser_in->token, 0, parser_in->token_len);

	for(i= *parser_in->index, j = 0; parser_in->source[i] !=0 ; i++)
	{
		if(j >= parser_in->token_len)
		{
			QCOM_AW_PHY_LOG_ERR("Invalid token length\n");
			return 0;
		}
		if(i >= parser_in->source_len)
		{
			QCOM_AW_PHY_LOG_ERR("Invalid Input\n");
			return 0;
		}

		for(k = 0; parser_in->delim[k] !=0; k++)
		{
			if(k >= parser_in->delim_len){
				QCOM_AW_PHY_LOG_ERR("Invalid Input\n");
				break;
			}
			if(parser_in->source[i] == parser_in->delim[k]){
				if(strlen(parser_in->token) == 0){
					continue;
				}
					*parser_in->index = i++;
					return parser_in->delim[k];
			}
		}
		parser_in->token[j++] = parser_in->source[i];

	}
	return 0;
}

static void remove_firstchar(char * str, uint32_t len)
{
	int i;

	if(!str)
		return;

	for(i=0;i<len;i++)
	{
		str[i] = str[i+1];

		if(str[i] == 0)
			break;
	}
	return;

}
static void get_file_name(char *filename, uint32_t len)
{

	int index = 0;
	char token[64];
	int i = 0;
	int iscoln = 0;
	parser_s parser_in;

	if(!filename){
		QCOM_AW_PHY_LOG_ERR("Null Pointer Input\n");
		return ;
	}

	for(i=0; i < len && filename[i] !=0; i++){
		if(filename[i] ==  ':'){
			iscoln = 1;
			break;
		}
	}
	if(!iscoln)
		return;

	parser_in.source = filename;
	parser_in.delim = ":";
	parser_in.token = token;
	parser_in.index = &index;
	parser_in.token_len = sizeof(token);
	parser_in.delim_len = 1;
	parser_in.source_len = len;

	string_parser(&parser_in);
	string_parser(&parser_in);
	remove_firstchar(token, sizeof(token));
	scnprintf(filename, sizeof(token), "%s", token);
	return;
}

static struct file_operations *file_name_to_wrapper(char *filename)
{
	if(!filename){
		QCOM_AW_PHY_LOG_ERR("Null Pointer Input\n");
		return NULL;
	}
	if (!strncmp(filename, "loopback_mode", FH_WRAPPER_SIZE))
	{
		return &aw_loopback_mode_fs_ops;
	}
	else if (!strncmp(filename, "ref_clock", FH_WRAPPER_SIZE))
	{
		return &aw_ref_clock_fs_ops;
	}
	else if (!strncmp(filename, "no_of_ports", FH_WRAPPER_SIZE))
	{
		return &aw_no_of_ports_fs_ops;
	}
	else if (!strncmp(filename, "aw_fw_ver", FH_WRAPPER_SIZE))
	{
		return &aw_fw_ver_fs_ops;
	}
	else if (!strncmp(filename, "info:mode:00", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_eq_mode_fs_ops;
	}
	else if (!strncmp(filename, "info:bu_status:00", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_bu_status_fs_ops;
	}
	else if (!strncmp(filename, "info:num_of_lanes_enabled:00", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lanes_enabled_fs_ops;
	}
	else if (!strncmp(filename, "info:sfp_port_type:00", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_sfp_port_type_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:00", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane0_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:00", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane0_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:00", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane0_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:00", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane0_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:00", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane0_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:01", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane1_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:01", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane1_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:01", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane1_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:01", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane1_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:01", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane1_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:02", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane2_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:02", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane2_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:02", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane2_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:02", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane2_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:02", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane2_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:03", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane3_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:03", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane3_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:03", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane3_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:03", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane3_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:03", FH_WRAPPER_SIZE))
	{
		return &aw_phy0_lane3_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "info:mode:10", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_eq_mode_fs_ops;
	}
	else if (!strncmp(filename, "info:bu_status:10", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_bu_status_fs_ops;
	}
	else if (!strncmp(filename, "info:num_of_lanes_enabled:10", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lanes_enabled_fs_ops;
	}
	else if (!strncmp(filename, "info:sfp_port_type:10", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_sfp_port_type_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:10", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane0_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:10", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane0_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:10", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane0_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:10", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane0_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:10", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane0_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:11", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane1_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:11", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane1_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:11", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane1_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:11", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane1_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:11", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane1_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:12", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane2_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:12", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane2_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:12", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane2_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:12", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane2_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:12", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane2_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:13", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane3_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:13", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane3_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:13", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane3_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:13", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane3_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:13", FH_WRAPPER_SIZE))
	{
		return &aw_phy1_lane3_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "info:mode:20", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_eq_mode_fs_ops;
	}
	else if (!strncmp(filename, "info:bu_status:20", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_bu_status_fs_ops;
	}
	else if (!strncmp(filename, "info:num_of_lanes_enabled:20", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lanes_enabled_fs_ops;
	}
	else if (!strncmp(filename, "info:sfp_port_type:20", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_sfp_port_type_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:20", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane0_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:20", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane0_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:20", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane0_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:20", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane0_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:20", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane0_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:21", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane1_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:21", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane1_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:21", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane1_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:21", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane1_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:21", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane1_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:22", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane2_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:22", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane2_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:22", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane2_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:22", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane2_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:22", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane2_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:23", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane3_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:23", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane3_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:23", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane3_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:23", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane3_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:23", FH_WRAPPER_SIZE))
	{
		return &aw_phy2_lane3_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "info:mode:30", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_eq_mode_fs_ops;
	}
	else if (!strncmp(filename, "info:bu_status:30", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_bu_status_fs_ops;
	}
	else if (!strncmp(filename, "info:num_of_lanes_enabled:30", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lanes_enabled_fs_ops;
	}
	else if (!strncmp(filename, "info:sfp_port_type:30", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_sfp_port_type_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:30", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane0_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:30", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane0_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:30", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane0_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:30", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane0_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:30", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane0_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:31", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane1_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:31", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane1_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:31", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane1_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:31", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane1_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:31", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane1_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:32", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane2_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:32", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane2_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:32", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane2_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:32", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane2_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:32", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane2_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:33", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane3_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:33", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane3_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:33", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane3_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:33", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane3_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:33", FH_WRAPPER_SIZE))
	{
		return &aw_phy3_lane3_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "info:mode:40", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_eq_mode_fs_ops;
	}
	else if (!strncmp(filename, "info:bu_status:40", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_bu_status_fs_ops;
	}
	else if (!strncmp(filename, "info:num_of_lanes_enabled:40", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lanes_enabled_fs_ops;
	}
	else if (!strncmp(filename, "info:sfp_port_type:40", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_sfp_port_type_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:40", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane0_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:40", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane0_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:40", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane0_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:40", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane0_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:40", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane0_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:41", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane1_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:41", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane1_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:41", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane1_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:41", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane1_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:41", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane1_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:42", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane2_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:42", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane2_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:42", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane2_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:42", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane2_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:42", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane2_mac_link_index_fs_ops;
	}
	else if (!strncmp(filename, "lane:enable_status:43", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane3_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:speed:43", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane3_speed_fs_ops;
	}
	else if (!strncmp(filename, "lane:cdr_lock_status:43", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane3_cdr_lock_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:pcs_link_status:43", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane3_link_status_fs_ops;
	}
	else if (!strncmp(filename, "lane:mac_link_index:43", FH_WRAPPER_SIZE))
	{
		return &aw_phy4_lane3_mac_link_index_fs_ops;
	}
	else{
		QCOM_AW_PHY_LOG_ERR("Invalid file name, no entry available\n");
		return &dummy;
	}
	return NULL;
}

int32_t setup_phy_status_debugfs_directory()
{
	char token[64];
	char my_delm;
	int index=0;
	int len= 0;
	struct file_operations *fileops = NULL;
	struct dentry *kobj_root = NULL;
	int curr_index = 0;
	parser_s parser_in;

	remove_whitespaces(input_string, sizeof(input_string));
	QCOM_AW_PHY_LOG_DBG("Inpurt String:%s\n", input_string);
	token[63] = '\0';

	parser_in.source = input_string;
	parser_in.delim = "{},";
	parser_in.token = token;
	parser_in.index = &index;
	parser_in.token_len = sizeof(token);
	parser_in.delim_len = 4;
	parser_in.source_len = sizeof(input_string);

	my_delm = string_parser(&parser_in);
	while (my_delm != 0)
	{
		len= strlen(token);
		QCOM_AW_PHY_LOG_DBG("token: %s\n",token);

		if(token[0] == '{' && len >2)
		{
			remove_firstchar(token, sizeof(token));
			//QCOM_AW_PHY_LOG_ERR("token after removal: %s\n",token);

			if(curr_index == 0)
			{
				list_dv[curr_index] = debugfs_create_dir(token,dobj);
				QCOM_AW_PHY_LOG_DBG("Curr_index = %u, inserting %s, parent was null", curr_index, token);
				curr_index++;
			}
			else
			{
				kobj_root = debugfs_create_dir(token, list_dv[curr_index -1]);
				list_dv[curr_index] = kobj_root;
				QCOM_AW_PHY_LOG_DBG("Curr_index = %u, inserting token:%s, parent was %u", curr_index, token, curr_index-1);
				curr_index++;
			}
		}
		else if( token[0] == '}'  && len > 2)
		{
			remove_firstchar(token, sizeof(token));
			QCOM_AW_PHY_LOG_DBG("Curr_index = %u, inserting %s, parent was %u", curr_index, token, curr_index-1);
			curr_index--;
			if(curr_index > 0)
			{
				kobj_root = debugfs_create_dir(token, list_dv[curr_index -1 ]);
				list_dv[curr_index] = kobj_root;
				curr_index++;
			}
		}
		else if(token[0] == ',' && len > 2)
		{
			// we want to create file
			remove_firstchar(token, sizeof(token));
			// file_name_to_callback , this will return a function pointer
			fileops = file_name_to_wrapper(token);
			get_file_name(token, sizeof(token));
			if(!debugfs_create_file(token, 0444, list_dv[curr_index - 1], 0, fileops))
			{
				QCOM_AW_PHY_LOG_ERR("Unable to create the debugfs file...\n");
			}
			QCOM_AW_PHY_LOG_DBG("Curr_index = %u, inserting token:%s, parent was %u", curr_index, token, curr_index -1);
		}
		else if(token[0] == '}' && len < 2)
		{
			QCOM_AW_PHY_LOG_DBG("Curr_index = %u, inserting Nothing, parent was %u", curr_index,  curr_index+1);
			curr_index--;
		}
		my_delm = string_parser(&parser_in);
	}
	QCOM_AW_PHY_LOG_INFO("Debugfs directory Structure for phy status created successfully...\n");
	return 0;
}

#endif /* FEATURE_QCOM_AW_DEBUG_FS */

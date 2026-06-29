
/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

/**
  @file qcom_aw_phy_test_sysfs.c
  @brief Test SYS FS for AW PHY driver.

  This file contains SYS FS definitions for AW PHY driver.
*/

#include <linux/sysfs.h>
#include <linux/delay.h>

#include "qcom_aw_phy_main.h"
#include "qcom_aw_phy_mtip_if.h"
#include "qcom_aw_phy_gnl.h"
#include "qcom_aw_phy_utils.h"
#include "eth_phy_iface.h"
#include "aw_c_api/aw_alphacore.h"

int qcom_aw_phy_an_restart_delay_timer_val = 1000;

enum qcom_aw_phy_sys_fs_cmd{
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

int                                        qcom_aw_phy_sysfs_attr_val;
enum mtip_port_type_enum                   sysfs_port_type = MTIP_PORT_TYPE_FH_0;
int                                        sysfs_num_lanes = 4;
enum eth_phy_iface_phy_lane_speed_enum     sysfs_lane_speed = PHY_LANE_SPEED_25G;

// PRBS parameters
enum qcom_aw_phy_instance_enum         sysfs_tx_bist_phy_inst = QCOM_AW_PHY_INST_FH0;
enum eth_phy_iface_phy_lane_num_enum   sysfs_tx_bist_lane_num = PHY_LANE_0;
enum qcom_aw_phy_instance_enum         sysfs_rx_bist_phy_inst = QCOM_AW_PHY_INST_FH0;
enum eth_phy_iface_phy_lane_num_enum   sysfs_rx_bist_lane_num = PHY_LANE_0;
aw_bist_pattern_t                      sysfs_bist_pattern = AW_PRBS31;
int                                    sysfs_measure_time = 1;
int                                    sysfs_inject_error_count = 0;
uint64_t                               sysfs_user_data = 0;
uint32_t                               sysfs_err_count_overflow[12] = {0};
uint64_t                               sysfs_err_count[12] = {0};
uint64_t                               sysfs_ber[12] = {0};
bool                                   sysfs_check_prbs_all_lanes = false;
uint32_t                               sysfs_port_config_mask = 0x800000;

aw_txfir_config_t        sysfs_tx_fir_cfg_cache[QCOM_AW_PHY_INST_MAX][PHY_LANE_MAX] = {{0}};
bool                     sysfs_tx_fir_cfg_cache_valid[QCOM_AW_PHY_INST_MAX][PHY_LANE_MAX] = {{false}};

extern struct eth_phy_iface_ops qcom_aw_phy_driver_iface_ops;
extern bool prbs_test_running;

#ifndef MIN
#define MIN(a,b) ((a < b) ? a : b)
#endif

#define MAX_INT_CHAR_SIZE 15

char sysfs_help_menu[] = {
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


void qcom_aw_phy_sysfs_ready_cb(void *udata) {
  QCOM_AW_PHY_LOG_DBG("qcom_aw_phy_sysfs_ready_cb");
}

void qcom_aw_phy_sysfs_an_result_cb(enum mtip_port_type_enum port_type,
                                      bool an_result,
                                      enum mtip_port_config_enum port_config,
                                      u8 seq_num) {

  QCOM_AW_PHY_LOG_DBG("qcom_aw_phy_sysfs_an_result_cb port %d, seq %d, result %d, config %d",
                      port_type, seq_num, an_result, port_config);
}

void qcom_aw_phy_sysfs_cdr_lock_ind(u32 link_index, bool status, u8 an_seq_num) {
  QCOM_AW_PHY_LOG_DBG("CDR lock for link_index %d, status %d, an_seq_num %d", link_index, status, an_seq_num);
}

void qcom_aw_phy_sysfs_lane_bring_up_progress_ind(u32 link_index, bool in_progress) {
  QCOM_AW_PHY_LOG_DBG("Lane bring up in progress: %d for link index %d", in_progress, link_index);
}

ssize_t qcom_aw_phy_sysfs_get_prbs_result(struct kobject *kobj, struct kobj_attribute *attr,
                           char *buf){
  char dbg_buf[1500] = {0};
  int nbytes = 0, i, j, min, max, min_port, max_port, lane_index;

  if(sysfs_rx_bist_lane_num != PHY_LANE_MAX){
    min = sysfs_rx_bist_lane_num;
    max = sysfs_rx_bist_lane_num;
  }
  else{
    min = PHY_LANE_0;
    max = PHY_LANE_3;
  }

  if(sysfs_check_prbs_all_lanes == false){
    if(sysfs_rx_bist_phy_inst <= QCOM_AW_PHY_INST_FH2){
      min_port = sysfs_rx_bist_phy_inst;
      max_port = sysfs_rx_bist_phy_inst;
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
                          j, i, sysfs_err_count[lane_index],
                          sysfs_err_count_overflow[lane_index], sysfs_ber[lane_index]);
    }
  }

  return snprintf(buf, nbytes+1, "%s\n", dbg_buf);
}

ssize_t qcom_aw_phy_sysfs_get_attr(struct kobject *kobj, struct kobj_attribute *attr,
                           char *buf) {
  return snprintf(buf, sizeof(sysfs_help_menu), "%s \n", sysfs_help_menu);
}

ssize_t qcom_aw_phy_sysfs_set_attr(struct kobject *kobj, struct kobj_attribute *attr,
                          const char *buf, size_t count) {
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
  u32 remaining = 0;
  bool error = false;
  enum qcom_aw_phy_eq_mode_enum eq_mode = QCOM_AW_PHY_EQ_MODE_MIN;
  memset(token_string, 0, sizeof(token_string));
  memcpy(token_string, buf, count);

  token = qcom_aw_phy_strtok(token_string, ',', &save_ptr);
  if(token!=NULL)
    sscanf(token, "%d", &qcom_aw_phy_sysfs_attr_val);
  else
  {
    QCOM_AW_PHY_LOG_ERR("Invalid input, \"cat /sys/kernel/debug/qcom_aw_phy_test/qcom_aw_phy\" for help menu");
    return -EFAULT;
  }

  QCOM_AW_PHY_LOG_ERR("qcom_aw_phy_set_attr val %d", qcom_aw_phy_sysfs_attr_val);

  switch (qcom_aw_phy_sysfs_attr_val) {

    case PHY_REG:
      QCOM_AW_PHY_LOG_ERR("Register");
      ready_info.notify_ready = qcom_aw_phy_sysfs_ready_cb;
      ready_info.userdata_ready = NULL;
      ready_info.notify_an_result = qcom_aw_phy_sysfs_an_result_cb;
      ready_info.cdr_lock_ind = qcom_aw_phy_sysfs_cdr_lock_ind;
      ready_info.lane_bring_up_progress_ind = qcom_aw_phy_sysfs_lane_bring_up_progress_ind;
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_eth_register(&ready_info,
                                                              &is_phy_ready);
      QCOM_AW_PHY_LOG_ERR("is_phy_ready %d", is_phy_ready);
      break;

    case PHY_SETUP:
      QCOM_AW_PHY_LOG_ERR("Setup");
      memset(lane_config, 0, 
             PHY_LANE_MAX * sizeof(struct eth_phy_iface_phy_lane_config));
      for (i = PHY_LANE_0; i < sysfs_num_lanes; i++) {
        lane_config[i].lane_enabled = true;
        lane_config[i].lane_speed = sysfs_lane_speed;
        lane_config[i].link_index = i;
      }
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_phy_setup(sysfs_port_type,
                                                           lane_config);
      break;

    case PHY_BRINGUP:
      QCOM_AW_PHY_LOG_ERR("Bringup");
      memset(lanes_enabled, 0, PHY_LANE_MAX * sizeof(bool));
      for (i = PHY_LANE_0; i < sysfs_num_lanes; i++) {
        lanes_enabled[i] = true;
      }
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_phy_bringup(sysfs_port_type,
                                                             lanes_enabled, 0);
      break;

    case PHY_TEAR_DOWN:
      QCOM_AW_PHY_LOG_ERR("Tear Down");
      memset(lanes_enabled, 0, PHY_LANE_MAX * sizeof(bool));
      for (i = PHY_LANE_0; i < sysfs_num_lanes; i++) {
        lanes_enabled[i] = true;
      }
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_phy_teardown(sysfs_port_type,
                                                              lanes_enabled);
      QCOM_AW_PHY_LOG_ERR("Resetting loopback mode for all PHY instances and lanes!");
      qcom_aw_phy_apply_loopback_mode_global(QCOM_AW_PHY_NO_LB);
      break;

    case MAC_LINK_UP:
      QCOM_AW_PHY_LOG_ERR("MAC link status up");
      memset(lanes_enabled, 0, PHY_LANE_MAX * sizeof(bool));
      for (i = PHY_LANE_0; i < sysfs_num_lanes; i++) {
        lanes_enabled[i] = true;
      }
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_notify_mac_link_status(
          sysfs_port_type, lanes_enabled, true);
      break;

    case MAC_LINK_DOWN:
      QCOM_AW_PHY_LOG_ERR("MAC link status down");
      memset(lanes_enabled, 0, PHY_LANE_MAX * sizeof(bool));
      for (i = PHY_LANE_0; i < sysfs_num_lanes; i++) {
        lanes_enabled[i] = true;
      }
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_notify_mac_link_status(
          sysfs_port_type, lanes_enabled, false);
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
      QCOM_AW_PHY_LOG_ERR("Set near end serial loopback mode for all PHY instances and lanes!");
      qcom_aw_phy_apply_loopback_mode_global(QCOM_AW_PHY_NEAR_END_SERIAL_LB);
      break;

    case NEP_LB:
      QCOM_AW_PHY_LOG_ERR("Set near end parallel loopback mode for all PHY instances and lanes!");
      qcom_aw_phy_apply_loopback_mode_global(QCOM_AW_PHY_NEAR_END_PARALLEL_LB);
      break;

    case DUMP_PHY_REG:
      QCOM_AW_PHY_LOG_ERR("Dumping PHY registers !");
      qcom_aw_phy_dump_registers();
      break;

    case SET_PORT:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &sysfs_port_type);
      else
      {
        error = true;
        break;
      }

      if(sysfs_port_type < MTIP_PORT_TYPE_FH_0 || sysfs_port_type >= MTIP_PORT_TYPE_MAX){
        sysfs_port_type = MTIP_PORT_TYPE_FH_0;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring port %d", sysfs_port_type);
      break;

    case SET_NUM_LANES:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &sysfs_num_lanes);
      else
      {
        error = true;
        break;
      }

      if(sysfs_num_lanes < 1 || sysfs_num_lanes > PHY_LANE_MAX){
        sysfs_num_lanes = 1;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring num_lanes %d", sysfs_num_lanes);
      break;

    case SET_LANE_SPEED:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &sysfs_lane_speed);
      else
      {
        error = true;
        break;
      }

      if(sysfs_lane_speed < PHY_LANE_SPEED_10G || sysfs_lane_speed >= PHY_LANE_SPEED_MAX){
        sysfs_lane_speed = PHY_LANE_SPEED_25G;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring lane_speed %d", sysfs_lane_speed);
      break;

    case TX_BIST_PHY_NUM:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &sysfs_tx_bist_phy_inst);
      else
      {
        error = true;
        break;
      }

      if(!QCOM_AW_PHY_INST_VALID(sysfs_tx_bist_phy_inst)){
        sysfs_tx_bist_phy_inst = QCOM_AW_PHY_INST_FH0;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring TX BIST PHY instance %d",
                          sysfs_tx_bist_phy_inst);
      break;

    case TX_BIST_LANE_NUM:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &sysfs_tx_bist_lane_num);
      else
      {
        error = true;
        break;
      }

      if(sysfs_tx_bist_lane_num > PHY_LANE_MAX){
        sysfs_tx_bist_lane_num = PHY_LANE_0;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring TX BIST PHY lane %d",
                          sysfs_tx_bist_lane_num);
      break;

    case RX_BIST_PHY_NUM:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &sysfs_rx_bist_phy_inst);
      else
      {
        error = true;
        break;
      }

      if(!QCOM_AW_PHY_INST_VALID(sysfs_rx_bist_phy_inst)){
        sysfs_rx_bist_phy_inst = QCOM_AW_PHY_INST_FH0;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring RX BIST PHY instance %d",
                          sysfs_rx_bist_phy_inst);
      break;

    case RX_BIST_LANE_NUM:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &sysfs_rx_bist_lane_num);
      else
      {
        error = true;
        break;
      }

      if(sysfs_rx_bist_lane_num > PHY_LANE_MAX){
        sysfs_rx_bist_lane_num = PHY_LANE_0;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring RX BIST PHY lane %d",
                          sysfs_rx_bist_lane_num);
      break;

    case PRBS_PATTERN:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &sysfs_bist_pattern);
      else
      {
        error = true;
        break;
      } 

      if(sysfs_bist_pattern >= AW_BIST_PATTERN_MAX){
        sysfs_bist_pattern = AW_PRBS31;
        error = true;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring BIST pattern %d", sysfs_bist_pattern);
      break;

    case UDP_DATA:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%llx", &sysfs_user_data);
      else
      {
        error = true;
        break;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring user data %llx", sysfs_user_data);
      break;

    case BIST_DURATION:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &sysfs_measure_time);
      else
      {
        error = true;
        break;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring measurement time %d", sysfs_measure_time);
      break;

    case BIST_ERROR_COUNT:
      token = qcom_aw_phy_strtok(NULL, ',', &save_ptr);
      if(token!=NULL)
        sscanf(token, "%d", &sysfs_inject_error_count);
      else
      {
        error = true;
        break;
      }

      QCOM_AW_PHY_LOG_ERR("Configuring inject error count %d",
                          sysfs_inject_error_count);
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

      if(sysfs_tx_bist_lane_num != PHY_LANE_MAX){
        min = sysfs_tx_bist_lane_num;
        max = sysfs_tx_bist_lane_num;
      }
      else{
        min = PHY_LANE_0;
        max = PHY_LANE_3;
      }

      if(sysfs_check_prbs_all_lanes == false){
        min_port = sysfs_tx_bist_phy_inst;
        max_port = sysfs_tx_bist_phy_inst;
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
            aw_pmd_tx_gen_config_set(&mss, sysfs_bist_pattern, sysfs_user_data, sysfs_user_data);
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
      phy_inst_info = &phy_config_info->phy_inst_config_info[sysfs_rx_bist_phy_inst];
      mss.phy_offset = phy_inst_info->base_addr;

      if(sysfs_rx_bist_lane_num != PHY_LANE_MAX){
        min = sysfs_rx_bist_lane_num;
        max = sysfs_rx_bist_lane_num;
      }
      else{
        min = PHY_LANE_0;
        max = PHY_LANE_3;
      }

      for (i = min; i <= max; i++) {
        pmd_set_lane(&mss, i);
        if(AW_ERR_CODE_NONE == aw_pmd_rx_check_cdr_lock(&mss, RX_CDR_TIMEOUT_US)){
          QCOM_AW_PHY_LOG_INFO("RX CDR lock success for PHY %d, lane %d",
                               sysfs_rx_bist_phy_inst, i);
        }
        else{
          QCOM_AW_PHY_LOG_INFO("RX CDR lock failure for PHY %d, lane %d",
                               sysfs_rx_bist_phy_inst, i);
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

      if(sysfs_rx_bist_lane_num != PHY_LANE_MAX){
        min = sysfs_rx_bist_lane_num;
        max = sysfs_rx_bist_lane_num;
      }
      else{
        min = PHY_LANE_0;
        max = PHY_LANE_3;
      }

      if(sysfs_check_prbs_all_lanes == false){
        min_port = sysfs_rx_bist_phy_inst;
        max_port = sysfs_rx_bist_phy_inst;
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

            aw_pmd_rx_chk_config_set(&mss, sysfs_bist_pattern, AW_DWELL,
                                     sysfs_user_data, sysfs_user_data, 2, 2000);
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

          prbs_test_running = true;
          remaining = (u32)sysfs_measure_time;
          while (remaining--)
            msleep(1000);
          prbs_test_running = false;

        for (j = min_port; j <= max_port; j++) {
          phy_inst_info = &phy_config_info->phy_inst_config_info[j];
          mss.phy_offset = phy_inst_info->base_addr;
          QCOM_AW_PHY_LOG_ERR("RX BIST for port %d", j);
          if(sysfs_check_prbs_all_lanes == false)
            tx_phy_inst_info = &phy_config_info->phy_inst_config_info[sysfs_tx_bist_phy_inst];
          else
            tx_phy_inst_info = &phy_config_info->phy_inst_config_info[j];
          tx_mss.phy_offset = tx_phy_inst_info->base_addr;

          for (i = min; i <= max; i++) {
            QCOM_AW_PHY_LOG_ERR("RX BIST checker for lane %d", i);
            pmd_set_lane(&mss, i);
            if(sysfs_tx_bist_lane_num != PHY_LANE_MAX)
              pmd_set_lane(&tx_mss, sysfs_tx_bist_lane_num);
            else
              pmd_set_lane(&tx_mss, i);

            if(sysfs_inject_error_count > 0){
              for (k = 0; k < sysfs_inject_error_count; k++) {
                udelay(1);
                aw_pmd_tx_gen_err_inject_en_set(&tx_mss,1);
                udelay(1);
                aw_pmd_tx_gen_err_inject_en_set(&tx_mss,0);
              }
            }

            lane_index = (j*PHY_LANE_MAX) + i;
            sysfs_err_count[lane_index] = 0;
            sysfs_err_count_overflow[lane_index] = 0;
            sysfs_ber[lane_index] = 0;

            CHECK(pmd_read_field(&mss, RX_DATABIST_TOP_RDREG3_ADDR,
                  RX_DATABIST_TOP_RDREG3_ERROR_CNT_55T32_NT_MASK,
                  RX_DATABIST_TOP_RDREG3_ERROR_CNT_55T32_NT_OFFSET,
                  &err_cnt_55_32));
            CHECK(pmd_read_field(&mss, RX_DATABIST_TOP_RDREG2_ADDR,
                  RX_DATABIST_TOP_RDREG2_ERROR_CNT_NT_MASK,
                  RX_DATABIST_TOP_RDREG2_ERROR_CNT_NT_OFFSET,
                  &err_cnt_31_0));
            sysfs_err_count[lane_index] += (uint64_t)err_cnt_55_32 << 32 | (uint64_t)err_cnt_31_0;
            CHECK(pmd_read_check_field(
                  &mss, RX_DATABIST_TOP_RDREG1_ADDR,
                  RX_DATABIST_TOP_RDREG1_ERROR_CNT_OVERFLOW_NT_MASK,
                  RX_DATABIST_TOP_RDREG1_ERROR_CNT_OVERFLOW_NT_OFFSET, RD_EQ,
                  &sysfs_err_count_overflow[lane_index], 0, 0));
            sysfs_ber[lane_index] += (uint64_t)sysfs_err_count[lane_index] / (sysfs_measure_time * 25);
            QCOM_AW_PHY_LOG_ERR("err_count = %lu\n", sysfs_err_count[lane_index]);
            QCOM_AW_PHY_LOG_ERR("err_count_overflow = %d\n", sysfs_err_count_overflow[lane_index]);
            QCOM_AW_PHY_LOG_ERR("ber = %lu\n", sysfs_ber[lane_index]);
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
      sysfs_check_prbs_all_lanes = enable_flag;
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
        sscanf(token, "%d", &sysfs_port_config_mask);
      else
      {
        error = true;
        break;
      }

      QCOM_AW_PHY_LOG_ERR("Port config mask set to 0x%x", sysfs_port_config_mask);
      break;


    case INITIATE_AN:
      QCOM_AW_PHY_LOG_ERR("Initiate AN");
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_initiate_an(sysfs_port_type, 0,
                                                             sysfs_num_lanes,
                                                             sysfs_port_config_mask);
      break;

    case RESET_PHY_SM:
      QCOM_AW_PHY_LOG_ERR("Reset PHY SM");
      qcom_aw_phy_driver_iface_ops.eth_phy_iface_reset_phy_sm(sysfs_port_type);
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

ssize_t qcom_aw_phy_sysfs_set_tx_eq_val(struct kobject *kobj, struct kobj_attribute *attr,
                          const char *buf, size_t count) {
  char *token;
  char token_string[100];
  char *save_ptr = NULL;
  struct qcom_aw_phy_config *phy_config_info = NULL;
  struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
  mss_access_t mss = {.phy_offset = 0, .lane_offset = 0};
  aw_txfir_config_t txfir_cfg = {0};

  memset(token_string, 0, sizeof(token_string));
  memcpy(token_string, buf, count);

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
                      sysfs_tx_bist_phy_inst, sysfs_tx_bist_lane_num,
                      txfir_cfg.CM3, txfir_cfg.CM2, txfir_cfg.CM1,
                      txfir_cfg.C0, txfir_cfg.C1, txfir_cfg.C2, txfir_cfg.C3,
                      txfir_cfg.main_or_max);

  phy_config_info = qcom_aw_phy_get_config_info();
  phy_inst_info = &phy_config_info->phy_inst_config_info[sysfs_tx_bist_phy_inst];
  mss.phy_offset = phy_inst_info->base_addr;
  pmd_set_lane(&mss, sysfs_tx_bist_lane_num);

  aw_pmd_txfir_config_set(&mss, &txfir_cfg, 1);

  sysfs_tx_fir_cfg_cache[sysfs_tx_bist_phy_inst][sysfs_tx_bist_lane_num] = txfir_cfg;
  sysfs_tx_fir_cfg_cache_valid[sysfs_tx_bist_phy_inst][sysfs_tx_bist_lane_num] = true;

  return count;
}

ssize_t qcom_aw_phy_sysfs_get_tx_eq_val(struct kobject *kobj, struct kobj_attribute *attr,
                         char *buf){
  struct qcom_aw_phy_config *phy_config_info = NULL;
  struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
  mss_access_t mss = {.phy_offset = 0, .lane_offset = 0};
  aw_txfir_config_t txfir_cfg = {0};
  char dbg_buf[200] = {0};
  int nbytes = 0;

  phy_config_info = qcom_aw_phy_get_config_info();
  phy_inst_info = &phy_config_info->phy_inst_config_info[sysfs_tx_bist_phy_inst];
  mss.phy_offset = phy_inst_info->base_addr;
  pmd_set_lane(&mss, sysfs_tx_bist_lane_num);

  if(sysfs_tx_fir_cfg_cache_valid[sysfs_tx_bist_phy_inst][sysfs_tx_bist_lane_num])
    txfir_cfg.main_or_max = sysfs_tx_fir_cfg_cache[sysfs_tx_bist_phy_inst][sysfs_tx_bist_lane_num].main_or_max;
  else
    txfir_cfg.main_or_max = 1;

  aw_pmd_txfir_config_get(&mss, &txfir_cfg);

  nbytes += scnprintf(dbg_buf, 200,
                      "TX EQ values for PHY instance %d, lane %d, "
                      "CM3=%d, CM2=%d, CM1=%d, C0=%d, C1=%d,C2=%d, C3=%d, "
                      "main_or_max=%d\n",
                      sysfs_tx_bist_phy_inst, sysfs_tx_bist_lane_num,
                      txfir_cfg.CM3, txfir_cfg.CM2, txfir_cfg.CM1,
                      txfir_cfg.C0, txfir_cfg.C1, txfir_cfg.C2, txfir_cfg.C3,
                      txfir_cfg.main_or_max);

  return snprintf(buf, nbytes+1, "%s\n", dbg_buf);
}

bool qcom_aw_phy_sysfs_get_tx_fir_val(enum qcom_aw_phy_instance_enum tx_bist_phy_inst,
                    enum eth_phy_iface_phy_lane_num_enum lane, void* txfir_cfg){

  if(!QCOM_AW_PHY_INST_VALID(tx_bist_phy_inst) || !QCOM_AW_PHY_LANE_VALID(lane))
    return false;

  if(sysfs_tx_fir_cfg_cache_valid[tx_bist_phy_inst][lane]){
    memcpy(txfir_cfg , &sysfs_tx_fir_cfg_cache[tx_bist_phy_inst][lane], sizeof(aw_txfir_config_t));
    return true;
  }

  return false;
}

ssize_t qcom_aw_phy_sysfs_show_an_restart_delay_timer(
                struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
  return snprintf(buf, MAX_INT_CHAR_SIZE, "%d\n",
                  qcom_aw_phy_an_restart_delay_timer_val);
}

ssize_t qcom_aw_phy_sysfs_store_an_restart_delay_timer(
                              struct kobject *kobj, struct kobj_attribute *attr,
                              const char *buf, size_t count) {
  sscanf(buf, "%d", &qcom_aw_phy_an_restart_delay_timer_val);
  return count;
}

struct kobject *kobj;

struct kobj_attribute qcom_aw_phy_attr =
    __ATTR(qcom_aw_phy, 0644, qcom_aw_phy_sysfs_get_attr, qcom_aw_phy_sysfs_set_attr);
struct kobj_attribute qcom_aw_phy_prbs_attr =
    __ATTR(prbs_result, 0644, qcom_aw_phy_sysfs_get_prbs_result, NULL);
struct kobj_attribute qcom_aw_phy_tx_eq_val_attr =
    __ATTR(tx_eq_val, 0644, qcom_aw_phy_sysfs_get_tx_eq_val, qcom_aw_phy_sysfs_set_tx_eq_val);

/* File attribute for AN restart delay timer value sysfs node */
struct kobj_attribute qcom_aw_phy_an_restart_delay_timer_attr =
                         __ATTR(an_restart_delay_timer_msec, 0660,
                                qcom_aw_phy_sysfs_show_an_restart_delay_timer,
                                qcom_aw_phy_sysfs_store_an_restart_delay_timer);

void qcom_aw_phy_setup_sysfs(void) {

  /* creating the directory structure in /sys/kernel/ */
  kobj = kobject_create_and_add("qcom_aw_phy_test", kernel_kobj);

  if(sysfs_create_file(kobj, &qcom_aw_phy_attr.attr))
    QCOM_AW_PHY_LOG_ERR("SysFS File creation failed");

  if(sysfs_create_file(kobj, &qcom_aw_phy_prbs_attr.attr))
    QCOM_AW_PHY_LOG_ERR("SysFS File creation failed");

  if(sysfs_create_file(kobj, &qcom_aw_phy_tx_eq_val_attr.attr))
    QCOM_AW_PHY_LOG_ERR("SysFS File creation failed");

  if(sysfs_create_file(kobj, &qcom_aw_phy_an_restart_delay_timer_attr.attr))
    QCOM_AW_PHY_LOG_ERR("SysFS File creation failed");

  return;
}

void qcom_aw_phy_del_sysfs(void) {

  /* deleting the directory structure in /sys/kernel/ */

  sysfs_remove_file(kobj, &qcom_aw_phy_attr.attr);
  sysfs_remove_file(kobj, &qcom_aw_phy_prbs_attr.attr);
  sysfs_remove_file(kobj, &qcom_aw_phy_tx_eq_val_attr.attr);
  sysfs_remove_file(kobj, &qcom_aw_phy_an_restart_delay_timer_attr.attr);

  kobject_del(kobj);
  kobject_put(kobj);
  kobj=NULL;

  return;
}

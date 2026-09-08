/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

/**
  @file qcom_aw_phy_prbs.c
  @brief Interface file between PRBS and DIAG Framework.

  This file contains interface handling between DIAG user space library and
  PRBS and PRBS functionalities.
*/

#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/syscalls.h>
#include <linux/sysfs.h> /* sysfs addition*/
#include <linux/delay.h>
#include <net/genetlink.h>
#include "qcom_aw_phy_utils.h"
#include "eth_phy_iface.h"
#include "aw_c_api/aw_alphacore.h"
#include "qcom_aw_phy_prbs_gnl_uapi.h"
#include "qcom_aw_phy_prbs.h"

// PRBS parameters
data_csm_diag_prbs_params data_csm_diag_prbs_params_info = {
    .tx_bist_phy_inst = QCOM_AW_PHY_INST_FH0,
    .tx_bist_phy_lane = PHY_LANE_0,
    .rx_bist_phy_inst = QCOM_AW_PHY_INST_FH0,
    .rx_bist_phy_lane = PHY_LANE_0,
    .check_all_lanes = 0,
    .prbs_bist_pattern = AW_PRBS31,
    .user_data_pattern = 0,
    .monitor_duration = 1,
    .num_of_errors = 0};

data_csm_diag_prbs_result data_csm_diag_prbs_result_info;
bool prbs_test_running;

struct genl_ops qcom_aw_phy_prbs_genl_ops[DATA_CSM_DIAG_GNL_CMD_COUNT] = {
    {
        .cmd = DATA_CSM_DIAG_GNL_CMD_SET_PRBS_PARAMS,
        .doit = qcom_aw_phy_prbs_gnl_set_prbs_params,
        .validate = 0,
    },
    {
        .cmd = DATA_CSM_DIAG_GNL_CMD_ENABLE_DISABLE_PRBS,
        .doit = qcom_aw_phy_prbs_gnl_enable_disable_prbs,
        .validate = 0,
    },
    {
        .cmd = DATA_CSM_DIAG_GNL_CMD_GET_PRBS_RESULT,
        .doit = qcom_aw_phy_prbs_gnl_get_prbs_result,
        .validate = 0,
    },
};

static struct nla_policy
    qcom_aw_phy_prbs_gnl_policy[DATA_CSM_DIAG_GNL_ATTR_MAX] = {
        [DATA_CSM_DIAG_GNL_ATTR_MIN] = {.type = NLA_UNSPEC},
        [DATA_CSM_DIAG_GNL_ATTR_SET_PRBS_PARAMS] = {.type = NLA_NUL_STRING},
        [DATA_CSM_DIAG_GNL_ATTR_ENABLE_DISABLE_PRBS] = {.type = NLA_S8},
        [DATA_CSM_DIAG_GNL_ATTR_GET_PRBS_RESULT] = {.type = NLA_NUL_STRING},
};

static struct genl_family qcom_aw_phy_prbs_gnl_family = {
    .id = 0,
    .hdrsize = 0,
    .name = PRBS_GNL_FAMILY_NAME,
    .version = 1,
    .ops = qcom_aw_phy_prbs_genl_ops,
    .n_ops = DATA_CSM_DIAG_GNL_CMD_COUNT,
    .policy = qcom_aw_phy_prbs_gnl_policy,
    .maxattr = DATA_CSM_DIAG_GNL_ATTR_MAX,
    .module = THIS_MODULE,
};

void qcom_aw_phy_prbs_set_params(data_csm_diag_prbs_params *prbs_params_info) {

  bool error = false;

  data_csm_diag_prbs_params_info.tx_bist_phy_inst =
      prbs_params_info->tx_bist_phy_inst;
  if (!QCOM_AW_PHY_INST_VALID(
          data_csm_diag_prbs_params_info.tx_bist_phy_inst)) {
    data_csm_diag_prbs_params_info.tx_bist_phy_inst = QCOM_AW_PHY_INST_FH0;
    error = true;
  }

  QCOM_AW_PHY_LOG_ERR("Configuring TX BIST PHY instance %d",
                      data_csm_diag_prbs_params_info.tx_bist_phy_inst);

  data_csm_diag_prbs_params_info.tx_bist_phy_lane =
      prbs_params_info->tx_bist_phy_lane;
  if (data_csm_diag_prbs_params_info.tx_bist_phy_lane > PHY_LANE_MAX) {
    data_csm_diag_prbs_params_info.tx_bist_phy_lane = PHY_LANE_0;
    error = true;
  }

  QCOM_AW_PHY_LOG_ERR("Configuring TX BIST PHY lane %d",
                      data_csm_diag_prbs_params_info.tx_bist_phy_lane);

  data_csm_diag_prbs_params_info.rx_bist_phy_inst =
      prbs_params_info->rx_bist_phy_inst;
  if (!QCOM_AW_PHY_INST_VALID(
          data_csm_diag_prbs_params_info.rx_bist_phy_inst)) {
    data_csm_diag_prbs_params_info.rx_bist_phy_inst = QCOM_AW_PHY_INST_FH0;
    error = true;
  }

  QCOM_AW_PHY_LOG_ERR("Configuring RX BIST PHY instance %d",
                      data_csm_diag_prbs_params_info.rx_bist_phy_inst);

  data_csm_diag_prbs_params_info.rx_bist_phy_lane =
      prbs_params_info->rx_bist_phy_lane;
  if (data_csm_diag_prbs_params_info.rx_bist_phy_lane > PHY_LANE_MAX) {
    data_csm_diag_prbs_params_info.rx_bist_phy_lane = PHY_LANE_0;
    error = true;
  }

  QCOM_AW_PHY_LOG_ERR("Configuring RX BIST PHY lane %d",
                      data_csm_diag_prbs_params_info.rx_bist_phy_lane);

  data_csm_diag_prbs_params_info.prbs_bist_pattern =
      prbs_params_info->prbs_bist_pattern;
  if (data_csm_diag_prbs_params_info.prbs_bist_pattern >= AW_BIST_PATTERN_MAX) {
    data_csm_diag_prbs_params_info.prbs_bist_pattern = AW_PRBS31;
    error = true;
  }

  QCOM_AW_PHY_LOG_ERR("Configuring BIST pattern %d",
                      data_csm_diag_prbs_params_info.prbs_bist_pattern);

  data_csm_diag_prbs_params_info.check_all_lanes =
      prbs_params_info->check_all_lanes;
  QCOM_AW_PHY_LOG_ERR("Configuring check_all_lanes %d",
                      data_csm_diag_prbs_params_info.check_all_lanes);

  data_csm_diag_prbs_params_info.user_data_pattern =
      prbs_params_info->user_data_pattern;
  QCOM_AW_PHY_LOG_ERR("Configuring user data %llx",
                      data_csm_diag_prbs_params_info.user_data_pattern);

  data_csm_diag_prbs_params_info.monitor_duration =
      prbs_params_info->monitor_duration;
  QCOM_AW_PHY_LOG_ERR("Configuring measurement time %d",
                      data_csm_diag_prbs_params_info.monitor_duration);

  data_csm_diag_prbs_params_info.num_of_errors =
      prbs_params_info->num_of_errors;
  QCOM_AW_PHY_LOG_ERR("Configuring inject error count %d",
                      data_csm_diag_prbs_params_info.num_of_errors);

  if (error)
    QCOM_AW_PHY_LOG_ERR("Invalid input \n");

}

int qcom_aw_phy_prbs_enable_disable(uint8_t enable_flag) {

  int i = 0, j = 0, k = 0;
  int min = 0, max = 0, min_port = 0, max_port = 0;
  struct qcom_aw_phy_config *phy_config_info = NULL;
  struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
  struct qcom_aw_phy_inst_config *tx_phy_inst_info = NULL;
  mss_access_t mss = {.phy_offset = 0, .lane_offset = 0};
  mss_access_t tx_mss = {.phy_offset = 0, .lane_offset = 0};
  uint32_t err_cnt_55_32, err_cnt_31_0;
  u32 remaining = 0;
  int prbs_lane_index = 0;

  QCOM_AW_PHY_LOG_ERR("Configuring TX BIST, enabled = %d", enable_flag);
  phy_config_info = qcom_aw_phy_get_config_info();

  if (data_csm_diag_prbs_params_info.tx_bist_phy_lane != PHY_LANE_MAX) {
    min = data_csm_diag_prbs_params_info.tx_bist_phy_lane;
    max = data_csm_diag_prbs_params_info.tx_bist_phy_lane;
  } else {
    min = PHY_LANE_0;
    max = PHY_LANE_3;
  }

  if (data_csm_diag_prbs_params_info.check_all_lanes == 0) {
    min_port = data_csm_diag_prbs_params_info.tx_bist_phy_inst;
    max_port = data_csm_diag_prbs_params_info.tx_bist_phy_inst;
  } else {
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
      if (enable_flag) {
        aw_pmd_tx_gen_config_set(
            &mss, data_csm_diag_prbs_params_info.prbs_bist_pattern,
            data_csm_diag_prbs_params_info.user_data_pattern,
            data_csm_diag_prbs_params_info.user_data_pattern);
        aw_pmd_gen_tx_en_set(&mss, 1);
      } else {
        aw_pmd_gen_tx_en_set(&mss, 0);
      }
    }
  }

  QCOM_AW_PHY_LOG_ERR("Configuring RX BIST, enabled = %d", enable_flag);

  phy_config_info = qcom_aw_phy_get_config_info();

  if (data_csm_diag_prbs_params_info.rx_bist_phy_lane != PHY_LANE_MAX) {
    min = data_csm_diag_prbs_params_info.rx_bist_phy_lane;
    max = data_csm_diag_prbs_params_info.rx_bist_phy_lane;
  } else {
    min = PHY_LANE_0;
    max = PHY_LANE_3;
  }

  if (data_csm_diag_prbs_params_info.check_all_lanes == 0) {
    min_port = data_csm_diag_prbs_params_info.rx_bist_phy_inst;
    max_port = data_csm_diag_prbs_params_info.rx_bist_phy_inst;
  } else {
    min_port = QCOM_AW_PHY_INST_FH0;
    max_port = QCOM_AW_PHY_INST_FH2;
  }

  if (enable_flag) {

    for (j = min_port; j <= max_port; j++) {
      phy_inst_info = &phy_config_info->phy_inst_config_info[j];
      mss.phy_offset = phy_inst_info->base_addr;
      QCOM_AW_PHY_LOG_ERR("RX BIST for port %d", j);

      for (i = min; i <= max; i++) {
        QCOM_AW_PHY_LOG_ERR("RX BIST checker for lane %d", i);
        pmd_set_lane(&mss, i);

        aw_pmd_rx_chk_config_set(
            &mss, data_csm_diag_prbs_params_info.prbs_bist_pattern, AW_DWELL,
            data_csm_diag_prbs_params_info.user_data_pattern,
            data_csm_diag_prbs_params_info.user_data_pattern, 2, 2000);
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
    remaining = (u32)data_csm_diag_prbs_params_info.monitor_duration;
    while (remaining--)
      msleep(1000);
    prbs_test_running = false;

    for (j = min_port; j <= max_port; j++) {
      phy_inst_info = &phy_config_info->phy_inst_config_info[j];
      mss.phy_offset = phy_inst_info->base_addr;
      QCOM_AW_PHY_LOG_ERR("RX BIST for port %d", j);
      if (data_csm_diag_prbs_params_info.check_all_lanes == 0)
        tx_phy_inst_info =
            &phy_config_info->phy_inst_config_info
                 [data_csm_diag_prbs_params_info.tx_bist_phy_inst];
      else
        tx_phy_inst_info = &phy_config_info->phy_inst_config_info[j];
      tx_mss.phy_offset = tx_phy_inst_info->base_addr;

      for (i = min; i <= max; i++) {
        QCOM_AW_PHY_LOG_ERR("RX BIST checker for lane %d", i);
        pmd_set_lane(&mss, i);
        if (data_csm_diag_prbs_params_info.tx_bist_phy_lane != PHY_LANE_MAX)
          pmd_set_lane(&tx_mss,
                       data_csm_diag_prbs_params_info.tx_bist_phy_lane);
        else
          pmd_set_lane(&tx_mss, i);

        if (data_csm_diag_prbs_params_info.num_of_errors > 0) {
          for (k = 0; k < data_csm_diag_prbs_params_info.num_of_errors; k++) {
            udelay(1);
            aw_pmd_tx_gen_err_inject_en_set(&tx_mss, 1);
            udelay(1);
            aw_pmd_tx_gen_err_inject_en_set(&tx_mss, 0);
          }
        }

        prbs_lane_index = (j * PHY_LANE_MAX) + i;
        data_csm_diag_prbs_result_info.err_count[prbs_lane_index] = 0;
        data_csm_diag_prbs_result_info.err_count_overflow[prbs_lane_index] = 0;
        data_csm_diag_prbs_result_info.ber[prbs_lane_index] = 0;

        CHECK(pmd_read_field(&mss, RX_DATABIST_TOP_RDREG3_ADDR,
                             RX_DATABIST_TOP_RDREG3_ERROR_CNT_55T32_NT_MASK,
                             RX_DATABIST_TOP_RDREG3_ERROR_CNT_55T32_NT_OFFSET,
                             &err_cnt_55_32));
        CHECK(pmd_read_field(&mss, RX_DATABIST_TOP_RDREG2_ADDR,
                             RX_DATABIST_TOP_RDREG2_ERROR_CNT_NT_MASK,
                             RX_DATABIST_TOP_RDREG2_ERROR_CNT_NT_OFFSET,
                             &err_cnt_31_0));
        data_csm_diag_prbs_result_info.err_count[prbs_lane_index] +=
            (uint64_t)err_cnt_55_32 << 32 | (uint64_t)err_cnt_31_0;
        CHECK(pmd_read_check_field(
            &mss, RX_DATABIST_TOP_RDREG1_ADDR,
            RX_DATABIST_TOP_RDREG1_ERROR_CNT_OVERFLOW_NT_MASK,
            RX_DATABIST_TOP_RDREG1_ERROR_CNT_OVERFLOW_NT_OFFSET, RD_EQ,
            &data_csm_diag_prbs_result_info.err_count_overflow[prbs_lane_index],
            0, 0));
        data_csm_diag_prbs_result_info.ber[prbs_lane_index] +=
            (uint64_t)data_csm_diag_prbs_result_info.err_count[prbs_lane_index] /
            (data_csm_diag_prbs_params_info.monitor_duration * 25);
      }
    }
  } else {
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

  return 0;
}
data_csm_diag_prbs_result qcom_aw_phy_prbs_get_result(void) {

  int i, j, min, max, min_port, max_port, prbs_lane_index;

  if (data_csm_diag_prbs_params_info.rx_bist_phy_lane != PHY_LANE_MAX) {
    min = data_csm_diag_prbs_params_info.rx_bist_phy_lane;
    max = data_csm_diag_prbs_params_info.rx_bist_phy_lane;
  } else {
    min = PHY_LANE_0;
    max = PHY_LANE_3;
  }

  if (data_csm_diag_prbs_params_info.check_all_lanes == 0) {
    min_port = data_csm_diag_prbs_params_info.rx_bist_phy_inst;
    max_port = data_csm_diag_prbs_params_info.rx_bist_phy_inst;
  } else {
    min_port = QCOM_AW_PHY_INST_FH0;
    max_port = QCOM_AW_PHY_INST_FH2;
  }

  data_csm_diag_prbs_result_info.min_lane = min;
  data_csm_diag_prbs_result_info.max_lane = max;
  data_csm_diag_prbs_result_info.min_port = min_port;
  data_csm_diag_prbs_result_info.max_port = max_port;

  for (j = min_port; j <= max_port; j++) {
    for (i = min; i <= max; i++) {
      prbs_lane_index = (j * PHY_LANE_MAX) + i;
      QCOM_AW_PHY_LOG_ERR(
          "Port %d, Lane %d, error count = %lu, "
          "error overflow count = %d, ber = %lu\n",
          j, i, data_csm_diag_prbs_result_info.err_count[prbs_lane_index],
          data_csm_diag_prbs_result_info.err_count_overflow[prbs_lane_index],
          data_csm_diag_prbs_result_info.ber[prbs_lane_index]);
    }
  }

  return data_csm_diag_prbs_result_info;
}

int qcom_aw_phy_prbs_gnl_set_prbs_params(struct sk_buff *sender_skb,
                                         struct genl_info *info) {

  struct nlattr *na;
  data_csm_diag_prbs_params *data_csm_diag_prbs_params_info;
  int ret_val = 0;
  enum local_error_enum local_err_val = LOCAL_ERROR_INVALID;

  QCOM_AW_PHY_LOG_ERR("qcom_aw_phy_prbs_gnl_set_prbs_params invoked");

  if (info == NULL) {
    ret_val = EINVAL;
    local_err_val = LOCAL_ERROR_0;
    return ret_val;
  }

  na = info->attrs[DATA_CSM_DIAG_GNL_ATTR_SET_PRBS_PARAMS];
  if (!na) {
    ret_val = EINVAL;
    local_err_val = LOCAL_ERROR_1;
    return ret_val;
  }

  data_csm_diag_prbs_params_info = (data_csm_diag_prbs_params *)nla_data(na);
  if (data_csm_diag_prbs_params_info == NULL) {
    ret_val = EINVAL;
    local_err_val = LOCAL_ERROR_2;
    return ret_val;
  } else {

    qcom_aw_phy_prbs_set_params(data_csm_diag_prbs_params_info);
  }

  return 0;
}

int qcom_aw_phy_prbs_gnl_enable_disable_prbs(struct sk_buff *sender_skb,
                                             struct genl_info *info) {

  struct nlattr *na;
  u8 *enable_disable_cmd;
  int ret_val = 0;
  enum local_error_enum local_err_val = LOCAL_ERROR_INVALID;

  QCOM_AW_PHY_LOG_ERR("qcom_aw_phy_prbs_gnl_enable_disable_prbs invoked");

  if (info == NULL) {
    ret_val = EINVAL;
    local_err_val = LOCAL_ERROR_0;
    return ret_val;
  }

  na = info->attrs[DATA_CSM_DIAG_GNL_ATTR_ENABLE_DISABLE_PRBS];
  if (!na) {
    ret_val = EINVAL;
    local_err_val = LOCAL_ERROR_1;
    return ret_val;
  }

  enable_disable_cmd = (u8 *)nla_data(na);
  if (enable_disable_cmd == NULL) {
    ret_val = EINVAL;
    local_err_val = LOCAL_ERROR_2;
    return ret_val;
  } else {
    QCOM_AW_PHY_LOG_ERR("Received value for Enable/Disable PRBS cmd : %d \n",
                        *enable_disable_cmd);
  }

  return qcom_aw_phy_prbs_enable_disable(*enable_disable_cmd);
}

int qcom_aw_phy_prbs_gnl_get_prbs_result(struct sk_buff *sender_skb,
                                         struct genl_info *info) {

  struct nlattr *na;
  char *recv_msg;
  struct sk_buff *reply_skb;
  void *msg_head;
  data_csm_diag_prbs_result data_csm_diag_prbs_result_tbl;
  int ret_val = 0;
  enum local_error_enum local_err_val = LOCAL_ERROR_INVALID;

  QCOM_AW_PHY_LOG_ERR("qcom_aw_phy_prbs_gnl_get_prbs_result invoked");

  if (info == NULL) {
    ret_val = EINVAL;
    local_err_val = LOCAL_ERROR_0;
    return ret_val;
  }

  na = info->attrs[DATA_CSM_DIAG_GNL_ATTR_GET_PRBS_RESULT];
  if (!na) {
    ret_val = EINVAL;
    local_err_val = LOCAL_ERROR_1;
    return ret_val;
  }

  recv_msg = (char *)nla_data(na);
  if (recv_msg == NULL) {
    ret_val = EINVAL;
    local_err_val = LOCAL_ERROR_2;
    return ret_val;
  }

  data_csm_diag_prbs_result_tbl = qcom_aw_phy_prbs_get_result();

  reply_skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
  if (reply_skb == NULL) {
    ret_val = ENOMEM;
    local_err_val = LOCAL_ERROR_4;
    return ret_val;
  }

  msg_head = genlmsg_put(reply_skb, info->snd_portid, info->snd_seq + 1,
                         &qcom_aw_phy_prbs_gnl_family, 0,
                         DATA_CSM_DIAG_GNL_CMD_GET_PRBS_RESULT);
  if (msg_head == NULL) {
    ret_val = ENOMEM;
    local_err_val = LOCAL_ERROR_5;
    return ret_val;
  }

  ret_val = nla_put(reply_skb, DATA_CSM_DIAG_GNL_ATTR_GET_PRBS_RESULT,
                    sizeof(data_csm_diag_prbs_result_tbl),
                    &data_csm_diag_prbs_result_tbl);
  if (ret_val != 0) {
    local_err_val = LOCAL_ERROR_6;
    return ret_val;
  }

  genlmsg_end(reply_skb, msg_head);

  ret_val = genlmsg_reply(reply_skb, info);
  if (ret_val != 0) {
    local_err_val = LOCAL_ERROR_6;
    return ret_val;
  }

  return 0;
}

/* Init Function */
int qcom_aw_phy_prbs_gnl_init(void) {
  int res;
  // Register family with its operations and policies
  res = genl_register_family(&qcom_aw_phy_prbs_gnl_family);
  if (res != 0) {
    QCOM_AW_PHY_LOG_ERR("FAILED: genl_register_family(): %i\n", res);
    return -1;
  } else {
    QCOM_AW_PHY_LOG_ERR("successfully registered using Generic Netlink.\n");
  }

  return 0;
}

/* Exit Function */
void qcom_aw_phy_prbs_gnl_exit(void) {
  // Unrerister family
  genl_unregister_family(&qcom_aw_phy_prbs_gnl_family);

  return;
}

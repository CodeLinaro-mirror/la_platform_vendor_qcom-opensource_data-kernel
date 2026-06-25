/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

/**
  @file qcom_aw_phy_main.c
  @brief Main file for the AW PHY driver.

  This file contains AW PHY driver probe handling.
*/

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/phylink.h>
#include <linux/string.h>
#include <linux/reset.h>

#include "qcom_aw_phy_main.h"
#include "qcom_aw_phy_mtip_if.h"
#include "qcom_aw_phy_synce.h"
#include "qcom_aw_phy_utils.h"
#include "qcom_aw_phy_gnl.h"
#include "qcom_aw_phy_prbs.h"
#include "aw_c_api/aw_alphacore.h"
#include "aw_c_api/aw_alphacore_ip_defines.h"
#include "aw_c_api/aw_driver_sim.h"
#include "aw_c_api/interface.h"

/* PHY Driver Name */
#define QCOM_AW_PHY_DRV_MAIN_NAME "qcom-aw-phy-main"
#define QCOM_AW_PHY_DRV_INST_NAME "qcom-aw-phy-inst"

#define REF_CLK_MODE_SILABS       0
#define REF_CLK_MODE_OSCILLATOR   1

#define RX_SIGNAL_DETECT_RETRY_DELAY_TIMER 10

/* Global to store the device level PHY information */
static struct qcom_aw_phy_config qcom_aw_phy_config_info;

/* Global to cache CXO clock reference */
struct clk *cxo_clk = NULL;

#define QCOM_AW_PHY_WORKQ_NAME_PREFIX      "qcom_aw_phy_wq"
#define QCOM_AW_PHY_WORKQ_NAME_MAX_LEN     25

/* Module parameters */
int qcom_aw_phy_loopback_mode = QCOM_AW_PHY_DEFAULT_LB;
module_param(qcom_aw_phy_loopback_mode, int,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(qcom_aw_phy_loopback_mode, "PHY loopback mode (0=NO_LB, 1=SERIAL, 2=PARALLEL, 3=DEFAULT/per-lane)");

int qcom_aw_phy_c2c_loopback_mode = QCOM_AW_PHY_NO_LB;


int qcom_aw_phy_ref_clk_mode = 0;
module_param(qcom_aw_phy_ref_clk_mode, int,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(qcom_aw_phy_ref_clk_mode, "PHY REF clock mode");

/*
 * CXO oscillator clock termination mode for ETH_CXO_OSC:
 *   0 = AW_RC_HI_Z   - High impedance
 *   1 = AW_RC_R50_SE - 50 Ω single-ended (HW reset default)
 *   2 = AW_RC_R100_DF - 100 Ω differential (board bring-up default)
 *
 * Set to 2 by default to match board-level differential clock design
 * (Amazon LEO BU requirement).  Only applies when ref_clk_mode == OSCILLATOR.
 */
int qcom_aw_phy_cxo_osc_term_mode = AW_RC_R100_DF;
module_param(qcom_aw_phy_cxo_osc_term_mode, int,
             S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(qcom_aw_phy_cxo_osc_term_mode,
                 "CXO OSC ref-clk termination: 0=Hi-Z, 1=50R SE, 2=100R diff");

int qcom_aw_phy_toggle_polarity = 0;
module_param(qcom_aw_phy_toggle_polarity, int,
             S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(qcom_aw_phy_toggle_polarity,
                 "Toggle PHY TX/RX polarity");

/* Module parameter to disable lane tear down as part of
   retry procedure for TX compliance tests */
bool qcom_aw_phy_tx_compliance_flag = false;

/*-------------------------------------------------------------------
* Function Definitions
------------------------------------------------------------------- */

/*-------------------------------------------------------------------
* qcom_aw_phy_get_config_info

* Description: This function returns the cache for PHY config information.
------------------------------------------------------------------- */
struct qcom_aw_phy_config *qcom_aw_phy_get_config_info(void) {
  return &qcom_aw_phy_config_info;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_get_inst_config

* Description: This function returns the PHY instance information.
------------------------------------------------------------------- */
struct qcom_aw_phy_inst_config *qcom_aw_phy_get_inst_config(enum qcom_aw_phy_instance_enum port) {
	struct qcom_aw_phy_config *phy_config_info = NULL;
	if(!QCOM_AW_PHY_INST_VALID(port))
		goto func_ret;
	phy_config_info=qcom_aw_phy_get_config_info();
	if(phy_config_info!=NULL){
		return &phy_config_info->phy_inst_config_info[port];
	}
func_ret:
	return NULL;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_get_lane_params

* Description: This function returns the lane param information.
------------------------------------------------------------------- */
struct qcom_aw_lane_params *qcom_aw_phy_get_lane_params(enum qcom_aw_phy_instance_enum port, enum eth_phy_iface_phy_lane_num_enum lane) {
	struct qcom_aw_phy_config *phy_config_info = NULL;
	struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
	if(!QCOM_AW_PHY_INST_VALID(port) || !QCOM_AW_PHY_LANE_VALID(lane))
		goto func_ret;
	phy_config_info=qcom_aw_phy_get_config_info();
	if(phy_config_info!=NULL){
		phy_inst_info = &phy_config_info->phy_inst_config_info[port];
		if(phy_inst_info!=NULL){
			return &phy_inst_info->lane_params[lane];
		}
	}
func_ret:
	return NULL;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_get_effective_loopback_mode

* @phy_inst: PHY instance type
* @lane: PHY lane number

* Description: This function returns the effective loopback mode for a specific
               PHY lane, considering global precedence over per-interface settings.
               
               Precedence order:
               1. Global qcom_aw_phy_loopback_mode (if not DEFAULT - highest priority)
               2. C2C-specific qcom_aw_phy_c2c_loopback_mode (for C2C instances)
               3. Per-lane loopback mode (lowest priority)
------------------------------------------------------------------- */
enum qcom_aw_phy_loopback_mode_enum qcom_aw_phy_get_effective_loopback_mode(
    enum qcom_aw_phy_instance_enum phy_inst,
    enum eth_phy_iface_phy_lane_num_enum lane) {
  struct qcom_aw_lane_params *lane_params = NULL;

  /* Validate inputs first to prevent crashes */
  if (!QCOM_AW_PHY_INST_VALID(phy_inst) || !QCOM_AW_PHY_LANE_VALID(lane)) {
    return QCOM_AW_PHY_NO_LB;
  }

  /* PRIORITY 1: Global flag takes precedence if not DEFAULT */
  if (qcom_aw_phy_loopback_mode != QCOM_AW_PHY_DEFAULT_LB) {
    /* Global mode is explicitly set (NO_LB, SERIAL, or PARALLEL) */
    return qcom_aw_phy_loopback_mode;
  }

  /* DEFAULT mode: Check C2C loopback mode for C2C instances */
  if ((phy_inst == QCOM_AW_PHY_INST_L2_C2C || phy_inst == QCOM_AW_PHY_INST_DEBUG_C2C)) {
    return qcom_aw_phy_c2c_loopback_mode;
  }

  /* DEFAULT mode: Check per-lane loopback mode from lane params */
  lane_params = qcom_aw_phy_get_lane_params(phy_inst, lane);
  if (lane_params) {
    return lane_params->per_lane_loopback_mode;
  }

  return QCOM_AW_PHY_NO_LB;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_apply_loopback_mode_global

* Description: Apply global loopback mode to all PHY lanes.
*              Only called when global mode is explicitly set.
------------------------------------------------------------------- */
void qcom_aw_phy_apply_loopback_mode_global(enum qcom_aw_phy_loopback_mode_enum mode) {
  enum qcom_aw_phy_instance_enum inst;
  enum eth_phy_iface_phy_lane_num_enum lane;
  
  /* Don't apply if DEFAULT mode */
  if (mode == QCOM_AW_PHY_DEFAULT_LB) {
    QCOM_AW_PHY_LOG_INFO("Mode Cannot be applied to lanes = %d\n", mode);
    return;
  }
  
  QCOM_AW_PHY_LOG_INFO("Applying global loopback mode %d to all FH PHY lanes", mode);
  
  /* Apply to all FH PHY instances */
  for (inst = QCOM_AW_PHY_INST_FH0; inst <= QCOM_AW_PHY_INST_FH2; inst++) {
    for (lane = PHY_LANE_0; lane < PHY_LANE_MAX; lane++) {
      qcom_aw_phy_set_effective_loopback_mode(inst, lane, mode);
    }
  }
}

/*-------------------------------------------------------------------
* qcom_aw_phy_get_loopback_mode
*
* Description: Get the current global loopback mode setting.
------------------------------------------------------------------- */
int qcom_aw_phy_get_loopback_mode(void) {
  return qcom_aw_phy_loopback_mode;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_set_effective_loopback_mode

* @phy_inst: PHY instance type
* @lane: PHY lane number
* @mode: Loopback mode to set

* Description: This function sets the effective loopback mode for a specific
*              PHY lane. Unlike the global set_loopback_mode function which
*              sets a system-wide loopback mode, this function allows setting
*              per-lane loopback configuration that can be retrieved using
*              qcom_aw_phy_get_effective_loopback_mode().
------------------------------------------------------------------- */
void qcom_aw_phy_set_effective_loopback_mode(
    enum qcom_aw_phy_instance_enum phy_inst,
    enum eth_phy_iface_phy_lane_num_enum lane,
    enum qcom_aw_phy_loopback_mode_enum mode) {
  struct qcom_aw_lane_params *lane_params = NULL;

  /* Validate input parameters */
  if (!QCOM_AW_PHY_INST_VALID(phy_inst) || !QCOM_AW_PHY_LANE_VALID(lane)) {
    QCOM_AW_PHY_LOG_ERR("Invalid PHY instance (%d) or lane (%d)", phy_inst, lane);
    return;
  }

  if ((phy_inst == QCOM_AW_PHY_INST_L2_C2C || phy_inst == QCOM_AW_PHY_INST_DEBUG_C2C)) {
    qcom_aw_phy_c2c_loopback_mode = mode;
    return;
  }

  /* Get the lane parameters structure */
  lane_params = qcom_aw_phy_get_lane_params(phy_inst, lane);
  if (!lane_params) {
    QCOM_AW_PHY_LOG_ERR("Failed to get lane params for PHY instance %d, lane %d", 
                        phy_inst, lane);
    return;
  }

  /* Set the per-lane loopback mode */
  lane_params->per_lane_loopback_mode = mode;

  QCOM_AW_PHY_LOG_INFO("Set effective loopback mode %d for PHY instance %d, lane %d", 
                       mode, phy_inst, lane);
}

/*-------------------------------------------------------------------
* qcom_aw_phy_any_lane_no_loopback

* Description: This function checks if any lane of any PHY instance is in
*              no loopback mode by using the effective loopback mode for
*              each lane.
*
* Return: true if any lane has no loopback, false otherwise
------------------------------------------------------------------- */
bool qcom_aw_phy_any_lane_no_loopback(void) {
  struct qcom_aw_phy_config *phy_config_info = NULL;
  enum qcom_aw_phy_instance_enum inst;
  enum eth_phy_iface_phy_lane_num_enum lane;

  phy_config_info = qcom_aw_phy_get_config_info();
  if (!phy_config_info) {
    return false;
  }

  /* Loop through all PHY instances */
  for (inst = QCOM_AW_PHY_INST_FH0; inst <= QCOM_AW_PHY_INST_FH2; inst++) {
    struct qcom_aw_phy_inst_config *inst_info = 
        &phy_config_info->phy_inst_config_info[inst];
    
    if (inst_info && inst_info->valid) {
      /* Loop through all lanes in this instance */
      for (lane = 0; lane < PHY_LANE_MAX; lane++) {
        enum qcom_aw_phy_loopback_mode_enum effective_lb_mode = 
            qcom_aw_phy_get_effective_loopback_mode(inst, lane);
        
        if (effective_lb_mode == QCOM_AW_PHY_NO_LB) {
          return true; /* Found at least one lane without loopback */
        }
      }
    }
  }

  return false; /* All lanes are in loopback mode */
}

/*------------------------------------------------------------------------
* qcom_aw_phy_get_polarity_flag

* Description: This function returns the polarity toggle flag for AW PHY.
----------------------------------------------------------------------- */
int qcom_aw_phy_get_polarity_flag(void) {
  return qcom_aw_phy_toggle_polarity;
}

int qcom_aw_phy_get_ref_clk_mode(void) {
  return qcom_aw_phy_ref_clk_mode;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_interrupt_handler

* @irq: Interrupt number
* @devptr : User data(struct aw_phy_inst_config)

* Description: This function does the formware loading, DT parsing and
   PHY structure initializations.
------------------------------------------------------------------- */
static irqreturn_t qcom_aw_phy_interrupt_handler(int irq, void *devptr) {
  struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
  u32 intr_status = 0;
  u32 intr_error = 0;
  u32 temp_bmask = 0;
  u32 i = 0;
  u32 clear = 0;
  enum local_error_enum local_err_val = LOCAL_ERROR_INVALID;
  int ret_val = IRQ_HANDLED;
  mss_access_t mss = {.phy_offset = 0, .lane_offset = 0};
  uint64_t tx_np_data = 1ULL;
  enum eth_phy_iface_phy_lane_num_enum  lane_num;

  // check if this an interrupt that needs to be handled
  for (i = QCOM_AW_PHY_INST_FH0; i < QCOM_AW_PHY_INST_MAX; i++)
  {
    if (devptr == (void *)&qcom_aw_phy_config_info.phy_inst_config_info[i])
    {
      phy_inst_info = (struct qcom_aw_phy_inst_config *)devptr;
      break;
    }
  }

  if (!phy_inst_info) {
    ret_val = IRQ_NONE;
    local_err_val = LOCAL_ERROR_0;
    goto func_exit;
  }

  // Read interrupt status register
  intr_status = ioread32(phy_inst_info->wrapper_base_addr +
                         QCOM_AW_PHY_WRAPPER_INT_STATUS_REG_OFFSET);

  // Read interrupt error register
  intr_error = ioread32(phy_inst_info->wrapper_base_addr +
                        QCOM_AW_PHY_WRAPPER_INT_ERROR_REG_OFFSET);

  if(intr_status==0 && intr_error==0)
    return ret_val;

  QCOM_AW_PHY_LOG_DBG(
      "Interrupt received for PHY instance %d, status %x, error %x",
      phy_inst_info->phy_inst, intr_status, intr_error);

  // Handle error interrupt
  for (i = QCOM_AW_PHY_INT_ERROR_BIT_MIN; i < QCOM_AW_PHY_INT_ERROR_BIT_MAX;
       i++) {
    temp_bmask = intr_error & (1 << i);
    if (temp_bmask) {
      switch (i) {
      case QCOM_AW_PHY_SNR_VALID_ERR_LANE_0:
      case QCOM_AW_PHY_SNR_VALID_ERR_LANE_1:
      case QCOM_AW_PHY_SNR_VALID_ERR_LANE_2:
      case QCOM_AW_PHY_SNR_VALID_ERR_LANE_3:
        lane_num = i - QCOM_AW_PHY_SNR_VALID_ERR_LANE_0;
        queue_work(phy_inst_info->wq,
           &phy_inst_info->lane_params[lane_num].snr_valid_err_wq_item.wq_item);
        clear |= (1<<i);
        break;

      default:
        break;
      }
    }
  }

  // clear error interrupt
  iowrite32(clear, phy_inst_info->wrapper_base_addr +
                            QCOM_AW_PHY_WRAPPER_INT_ERROR_CLR_REG_OFFSET);

  clear = 0;

  // Handle status interrupt
  for (i = QCOM_AW_PHY_INT_STATUS_BIT_MIN; i < QCOM_AW_PHY_INT_STATUS_BIT_MAX;
       i++) {
    temp_bmask = intr_status & (1 << i);
    if (temp_bmask) {
      switch (i) {
      case QCOM_AW_PHY_AN_DONE_LANE_0:
      case QCOM_AW_PHY_AN_DONE_LANE_1:
      case QCOM_AW_PHY_AN_DONE_LANE_2:
      case QCOM_AW_PHY_AN_DONE_LANE_3:
        lane_num = i - QCOM_AW_PHY_AN_DONE_LANE_0;
        queue_work(phy_inst_info->wq,
                 &phy_inst_info->lane_params[lane_num].an_done_wq_item.wq_item);
        clear |= (1<<i);
        break;

      case QCOM_AW_PHY_AN_LINK_GOOD_LANE_0:
      case QCOM_AW_PHY_AN_LINK_GOOD_LANE_1:
      case QCOM_AW_PHY_AN_LINK_GOOD_LANE_2:
      case QCOM_AW_PHY_AN_LINK_GOOD_LANE_3:
        lane_num = i - QCOM_AW_PHY_AN_LINK_GOOD_LANE_0;
        queue_work(phy_inst_info->wq,
             &phy_inst_info->lane_params[lane_num].an_link_good_wq_item.wq_item);
        clear |= (1<<i);
        break;

      case QCOM_AW_PHY_AN_NEW_PAGE_LANE_0:
      case QCOM_AW_PHY_AN_NEW_PAGE_LANE_1:
      case QCOM_AW_PHY_AN_NEW_PAGE_LANE_2:
      case QCOM_AW_PHY_AN_NEW_PAGE_LANE_3:
        QCOM_AW_PHY_LOG_DBG("AN new page for lane %d",
                            i - QCOM_AW_PHY_AN_NEW_PAGE_LANE_0);
        mss.phy_offset = phy_inst_info->base_addr;
        pmd_set_lane(&mss, i-QCOM_AW_PHY_AN_NEW_PAGE_LANE_0);
        aw_pmd_anlt_auto_neg_next_page_set(&mss, tx_np_data);
        clear |= (1<<i);
        break;

      case QCOM_AW_PHY_SNR_VALID_LANE_0:
      case QCOM_AW_PHY_SNR_VALID_LANE_1:
      case QCOM_AW_PHY_SNR_VALID_LANE_2:
      case QCOM_AW_PHY_SNR_VALID_LANE_3:
        lane_num = i - QCOM_AW_PHY_SNR_VALID_LANE_0;
        queue_work(phy_inst_info->wq,
               &phy_inst_info->lane_params[lane_num].snr_valid_wq_item.wq_item);
        clear |= (1<<i);
        break;

      default:
        break;
      }
    }
  }

  // clear status interrupt
  iowrite32(clear, phy_inst_info->wrapper_base_addr +
                             QCOM_AW_PHY_WRAPPER_INT_STATUS_CLR_REG_OFFSET);

func_exit:
  if(local_err_val != LOCAL_ERROR_INVALID){
    QCOM_AW_PHY_LOG_INFO(
            "QCOM_aw_phy_interrupt_handler returns %d, local_err = %d", ret_val,
            local_err_val);
  }

  return ret_val;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_enable_interrupt

* @phy_inst_info: PHY instance pointer

* Description: This function enables the interrupts PHY driver is
               interested in.
------------------------------------------------------------------- */
void qcom_aw_phy_enable_interrupt(
    struct qcom_aw_phy_inst_config *phy_inst_info) {
  u32 temp_bmask = 0;
  u32 i = 0;
  enum local_error_enum local_err_val = LOCAL_ERROR_INVALID;

  if (!phy_inst_info) {
    local_err_val = LOCAL_ERROR_0;
    goto func_exit;
  }

  // Enable status interrupt
  for (i = QCOM_AW_PHY_INT_STATUS_BIT_MIN; i < QCOM_AW_PHY_INT_STATUS_BIT_MAX;
       i++) {
    switch (i) {
    case QCOM_AW_PHY_AN_DONE_LANE_0:
    case QCOM_AW_PHY_AN_DONE_LANE_1:
    case QCOM_AW_PHY_AN_DONE_LANE_2:
    case QCOM_AW_PHY_AN_DONE_LANE_3:
    case QCOM_AW_PHY_AN_LINK_GOOD_LANE_0:
    case QCOM_AW_PHY_AN_LINK_GOOD_LANE_1:
    case QCOM_AW_PHY_AN_LINK_GOOD_LANE_2:
    case QCOM_AW_PHY_AN_LINK_GOOD_LANE_3:
    case QCOM_AW_PHY_AN_NEW_PAGE_LANE_0:
    case QCOM_AW_PHY_AN_NEW_PAGE_LANE_1:
    case QCOM_AW_PHY_AN_NEW_PAGE_LANE_2:
    case QCOM_AW_PHY_AN_NEW_PAGE_LANE_3:
      temp_bmask |= (1 << i);
      break;

    default:
      break;
    }
  }

#ifndef FEATURE_QCOM_AW_RUMI_SW
  iowrite32(temp_bmask, phy_inst_info->wrapper_base_addr +
                            QCOM_AW_PHY_WRAPPER_INT_STATUS_EN_REG_OFFSET);
#endif

  QCOM_AW_PHY_LOG_INFO(
      "Status interrupt enabled for PHY instance %d, bitmask %x",
      phy_inst_info->phy_inst, temp_bmask);

#if 0
  // Enable error interrupt
  temp_bmask = 0;
  for (i = QCOM_AW_PHY_INT_ERROR_BIT_MIN; i < QCOM_AW_PHY_INT_ERROR_BIT_MAX;
       i++) {
    switch (i) {
    case QCOM_AW_PHY_RX_SIGNAL_DETECT_ERR_LANE_0:
    case QCOM_AW_PHY_RX_SIGNAL_DETECT_ERR_LANE_1:
    case QCOM_AW_PHY_RX_SIGNAL_DETECT_ERR_LANE_2:
    case QCOM_AW_PHY_RX_SIGNAL_DETECT_ERR_LANE_3:
      temp_bmask |= (1 << i);
      break;

    default:
      break;
    }
  }

#ifndef FEATURE_QCOM_AW_RUMI_SW
  iowrite32(temp_bmask, phy_inst_info->wrapper_base_addr +
                            QCOM_AW_PHY_WRAPPER_INT_ERROR_EN_REG_OFFSET);
#endif

  QCOM_AW_PHY_LOG_INFO(
      "Error interrupt enabled for PHY instance %d, bitmask %x",
      phy_inst_info->phy_inst, temp_bmask);
#endif

func_exit:
  QCOM_AW_PHY_LOG_INFO("qcom_aw_phy_enable_interrupt local_err = %d",
                       local_err_val);

  return;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_enable_snr_interrupt

* @phy_inst_info: PHY instance pointer
* @lane: PHY lane number

* Description: This function enables the SNR valid and error
               interrupts for the given PHY instance and lane.
------------------------------------------------------------------- */
void qcom_aw_phy_enable_snr_interrupt(
    struct qcom_aw_phy_inst_config *phy_inst_info,
    enum eth_phy_iface_phy_lane_num_enum lane) {
  u32 val = 0;

  if (!phy_inst_info || !QCOM_AW_PHY_LANE_VALID(lane)) {
    return;
  }

  // Enable status interrupt
  val = ioread32(phy_inst_info->wrapper_base_addr +
                                 QCOM_AW_PHY_WRAPPER_INT_STATUS_EN_REG_OFFSET);
  val |= (1 << (QCOM_AW_PHY_SNR_VALID_LANE_0 + lane));
  iowrite32(val, phy_inst_info->wrapper_base_addr +
                                 QCOM_AW_PHY_WRAPPER_INT_STATUS_EN_REG_OFFSET);

  // Enable error interrupt
  val = ioread32(phy_inst_info->wrapper_base_addr +
                                  QCOM_AW_PHY_WRAPPER_INT_ERROR_EN_REG_OFFSET);
  val |= (1 << (QCOM_AW_PHY_SNR_VALID_ERR_LANE_0 + lane));
  iowrite32(val, phy_inst_info->wrapper_base_addr +
                                  QCOM_AW_PHY_WRAPPER_INT_ERROR_EN_REG_OFFSET);

  return;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_disable_snr_interrupt

* @phy_inst_info: PHY instance pointer
* @lane: PHY lane number

* Description: This function disables the SNR valid and error
               interrupts for the given PHY instance and lane.
------------------------------------------------------------------- */
void qcom_aw_phy_disable_snr_interrupt(
    struct qcom_aw_phy_inst_config *phy_inst_info,
    enum eth_phy_iface_phy_lane_num_enum lane) {
  u32 val = 0;

  if (!phy_inst_info || !QCOM_AW_PHY_LANE_VALID(lane)) {
    return;
  }

  // Disable status interrupt
  val = ioread32(phy_inst_info->wrapper_base_addr +
                                 QCOM_AW_PHY_WRAPPER_INT_STATUS_EN_REG_OFFSET);
  val &= ~(1 << (QCOM_AW_PHY_SNR_VALID_LANE_0 + lane));
  iowrite32(val, phy_inst_info->wrapper_base_addr +
                                 QCOM_AW_PHY_WRAPPER_INT_STATUS_EN_REG_OFFSET);

  // Disable error interrupt
  val = ioread32(phy_inst_info->wrapper_base_addr +
                                  QCOM_AW_PHY_WRAPPER_INT_ERROR_EN_REG_OFFSET);
  val &= ~(1 << (QCOM_AW_PHY_SNR_VALID_ERR_LANE_0 + lane));
  iowrite32(val, phy_inst_info->wrapper_base_addr +
                                  QCOM_AW_PHY_WRAPPER_INT_ERROR_EN_REG_OFFSET);

  return;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_get_clock

* @dev: platform device's dev pointer
  @id: Clock name

* Description: This function gets the PHY clocks.
------------------------------------------------------------------- */
static struct clk* qcom_aw_phy_get_clock(struct device *dev, const char *id) {
  struct clk *clk = NULL;

  /* Fetch the clock */
  clk = devm_clk_get(dev, id);
  if (IS_ERR_OR_NULL(clk)) {
    QCOM_AW_PHY_LOG_ERR("Failed to get %s, error %d", id, PTR_ERR(clk));
    return NULL;
  }

  return clk;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_enable_clock

* @dev: platform device's dev pointer
  @id: Clock name

* Description: This function fetches and enables PHY clocks..
------------------------------------------------------------------- */
static void qcom_aw_phy_enable_clock(struct device *dev, const char *id) {
  struct clk *clk = NULL;

  /* Fetch the clock */
  clk = devm_clk_get(dev, id);
  if (IS_ERR_OR_NULL(clk)) {
    QCOM_AW_PHY_LOG_ERR("Failed to get %s, error %d", id, PTR_ERR(clk));
    return;
  }

  /* Prepare/enable the clock */
  if (clk_prepare_enable(clk)) {
    QCOM_AW_PHY_LOG_ERR("Failed to prepare/enable %s", id);
    return;
  }

  return;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_setup_clocks

* @dev: platform device's dev pointer

* Description: This function sets up PHY clocks.
------------------------------------------------------------------- */
static void qcom_aw_phy_setup_clocks(struct device *dev) {

#ifdef FEATURE_QCOM_AW_RUMI_SW
  return;
#endif /* FEATURE_QCOM_AW_RUMI_SW */

  QCOM_AW_PHY_LOG_INFO("qcom_aw_phy_setup_clocks");

  // GCC Clocks
  qcom_aw_phy_enable_clock(dev, "GCC_ETH_100G_C2C_HM_APB_CLK");
  qcom_aw_phy_enable_clock(dev, "GCC_ETH_100G_FH_HM_APB_0_CLK");
  qcom_aw_phy_enable_clock(dev, "GCC_ETH_100G_FH_HM_APB_1_CLK");
  qcom_aw_phy_enable_clock(dev, "GCC_ETH_100G_FH_HM_APB_2_CLK");
  qcom_aw_phy_enable_clock(dev, "GCC_ETH_DBG_C2C_HM_APB_CLK");
  qcom_aw_phy_enable_clock(dev, "GCC_ETH_DBG_SNOC_AXI_CLK");

  // FH0 PHY Clocks
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY0_LANE0_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY0_LANE0_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY0_LANE1_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY0_LANE1_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY0_LANE2_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY0_LANE2_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY0_LANE3_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY0_LANE3_TX_CLK");

  // FH1 PHY Clocks
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY1_LANE0_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY1_LANE0_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY1_LANE1_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY1_LANE1_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY1_LANE2_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY1_LANE2_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY1_LANE3_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY1_LANE3_TX_CLK");

  // FH2 PHY Clocks
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY2_LANE0_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY2_LANE0_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY2_LANE1_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY2_LANE1_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY2_LANE2_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY2_LANE2_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY2_LANE3_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY2_LANE3_TX_CLK");

  // L2 PHY Clocks
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY3_LANE0_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY3_LANE0_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY3_LANE1_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY3_LANE1_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY3_LANE2_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY3_LANE2_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY3_LANE3_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY3_LANE3_TX_CLK");

  // Debug PHY Clocks
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY4_LANE0_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY4_LANE0_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY4_LANE1_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY4_LANE1_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY4_LANE2_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY4_LANE2_TX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY4_LANE3_RX_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_PHY4_LANE3_TX_CLK");

  // SRAM clocks
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_ETH_PHY_0_OCK_SRAM_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_ETH_PHY_1_OCK_SRAM_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_ETH_PHY_2_OCK_SRAM_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_ETH_PHY_3_OCK_SRAM_CLK");
  qcom_aw_phy_enable_clock(dev, "ECPRI_CC_ETH_PHY_4_OCK_SRAM_CLK");

  return;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_setup_synce_clocks

* @dev: platform device's dev pointer

* Description: This function sets up PHY SyncE clocks.
------------------------------------------------------------------- */
static void qcom_aw_phy_setup_synce_clocks(struct device *dev) {

  QCOM_AW_PHY_LOG_INFO("qcom_aw_phy_setup_synce_clocks");

  qcom_aw_phy_config_info.synce_cmux_clk_src =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_CMUX_CLK_SRC");

  qcom_aw_phy_config_info.synce_cmux_clk =
             qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_CMUX_CLK");

  qcom_aw_phy_config_info.synce_div_clk =
          qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_DIV_CLK_SRC");

  qcom_aw_phy_config_info.synce_phy_lane_clk[FH0_LANE_0] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY0_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[FH0_LANE_1] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY1_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[FH0_LANE_2] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY2_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[FH0_LANE_3] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY3_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[FH1_LANE_0] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY4_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[FH1_LANE_1] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY5_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[FH1_LANE_2] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY6_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[FH1_LANE_3] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY7_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[FH2_LANE_0] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY8_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[FH2_LANE_1] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY9_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[FH2_LANE_2] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY10_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[FH2_LANE_3] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY11_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[L2_C2C_LANE_0] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY12_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[L2_C2C_LANE_1] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY13_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[L2_C2C_LANE_2] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY14_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[L2_C2C_LANE_3] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY15_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[DBG_C2C_LANE0] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY16_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[DBG_C2C_LANE1] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY17_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[DBG_C2C_LANE2] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY18_CLK_SRC");
  qcom_aw_phy_config_info.synce_phy_lane_clk[DBG_C2C_LANE3] =
         qcom_aw_phy_get_clock(dev, "ECPRI_CC_EMAC_SYNCE_PHY19_CLK_SRC");

  return;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_enable_ref_clk_propagation

* @phy_inst_info: PHY instance pointer

* Description: This function enables the oscillator REF clock
               propgation from Debug PHY(botton) to FH0 PHY (top).
------------------------------------------------------------------- */
static void qcom_aw_phy_enable_ref_clk_propagation(
    struct qcom_aw_phy_inst_config *phy_inst_info) {
  mss_access_t mss = {.phy_offset = 0, .lane_offset = 0};
  enum local_error_enum local_err_val = LOCAL_ERROR_INVALID;
  int reg_val = 0;

  if (!phy_inst_info) {
    local_err_val = LOCAL_ERROR_0;
    goto func_exit;
  }

  // Setup PHY offset
  mss.phy_offset = phy_inst_info->base_addr;

  pmd_write_field(&mss, DIG_SOC_CMN_OVRD_ADDR,
                  DIG_SOC_CMN_OVRD_REF_SEL_OVRD_EN_A_MASK,
                  DIG_SOC_CMN_OVRD_REF_SEL_OVRD_EN_A_OFFSET, 1);

  pmd_write_field(&mss, DIG_SOC_CMN_OVRD_ADDR,
                  DIG_SOC_CMN_OVRD_CMN_OVRD_EN_A_MASK,
                  DIG_SOC_CMN_OVRD_CMN_OVRD_EN_A_OFFSET, 1);

  pmd_write_field(&mss, DIG_SOC_CMN_OVRD_ADDR,
                  DIG_SOC_CMN_OVRD_ICTL_REF_LS_ENA_A_MASK,
                  DIG_SOC_CMN_OVRD_ICTL_REF_LS_ENA_A_OFFSET, 1);

  if(qcom_aw_phy_ref_clk_mode == REF_CLK_MODE_OSCILLATOR){
    if (phy_inst_info->phy_inst == QCOM_AW_PHY_INST_DEBUG_C2C)
      reg_val = 0x4;
    else
      reg_val = 0x1;
  }
  else{
    if (phy_inst_info->phy_inst == QCOM_AW_PHY_INST_FH0)
      reg_val = 0x4;
    else
      reg_val = 0x3;
  }

  pmd_write_field(&mss, DIG_SOC_CMN_OVRD_ADDR,
                  DIG_SOC_CMN_OVRD_ICTL_LSREF_SELECT_NT_MASK,
                  DIG_SOC_CMN_OVRD_ICTL_LSREF_SELECT_NT_OFFSET, reg_val);

  if(qcom_aw_phy_ref_clk_mode == REF_CLK_MODE_OSCILLATOR){
    if (phy_inst_info->phy_inst == QCOM_AW_PHY_INST_FH0)
      reg_val = 0x0;
    else if (phy_inst_info->phy_inst == QCOM_AW_PHY_INST_DEBUG_C2C)
      reg_val = 0x3;
    else
      reg_val = 0x1;

    pmd_write_field(&mss, DIG_SOC_CMN_OVRD_ADDR,
                    DIG_SOC_CMN_OVRD_ICTL_L2R0_LSREF_SELECT_NT_MASK,
                    DIG_SOC_CMN_OVRD_ICTL_L2R0_LSREF_SELECT_NT_OFFSET, reg_val);
  }
  else{
    if (phy_inst_info->phy_inst == QCOM_AW_PHY_INST_FH0)
      reg_val = 0x3;
    else if (phy_inst_info->phy_inst == QCOM_AW_PHY_INST_DEBUG_C2C)
      reg_val = 0x0;
    else
      reg_val = 0x1;

    pmd_write_field(&mss, DIG_SOC_CMN_OVRD_ADDR,
                    DIG_SOC_CMN_OVRD_ICTL_R2L0_LSREF_SELECT_NT_MASK,
                    DIG_SOC_CMN_OVRD_ICTL_R2L0_LSREF_SELECT_NT_OFFSET, reg_val);
  }

func_exit:
  QCOM_AW_PHY_LOG_ERR("%s: local error %d", __func__, local_err_val);

  return;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_set_cxo_osc_termination

* Description: Programs ETH_CXO_OSC ref-clock input termination on
*   every valid PHY instance.  Must be called after ref-clock
*   propagation is configured so that the CMN block is accessible.
*
*   Termination mode is controlled by the qcom_aw_phy_cxo_osc_term_mode
*   module parameter (default AW_RC_R100_DF = 100 Ω differential).
*
*   Only meaningful when qcom_aw_phy_ref_clk_mode == REF_CLK_MODE_OSCILLATOR.
*   Skipped silently in Si-Labs mode to avoid touching clock regs that
*   are driven by the external clock chip.
------------------------------------------------------------------- */
static void qcom_aw_phy_set_cxo_osc_termination(
    struct qcom_aw_phy_config *phy_config_info)
{
  struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
  enum qcom_aw_phy_instance_enum phy_inst_type;
  mss_access_t mss = {.phy_offset = 0, .lane_offset = 0};
  aw_refclk_term_mode_t term_mode =
      (aw_refclk_term_mode_t)qcom_aw_phy_cxo_osc_term_mode;
  int ret;

  if (qcom_aw_phy_ref_clk_mode != REF_CLK_MODE_OSCILLATOR) {
    QCOM_AW_PHY_LOG_INFO(
        "CXO OSC term skipped: ref_clk_mode=%d (not oscillator)",
        qcom_aw_phy_ref_clk_mode);
    return;
  }

  if (term_mode < AW_RC_HI_Z || term_mode > AW_RC_R100_DF) {
    QCOM_AW_PHY_LOG_ERR(
        "CXO OSC term: invalid mode %d, defaulting to AW_RC_R100_DF",
        qcom_aw_phy_cxo_osc_term_mode);
    term_mode = AW_RC_R100_DF;
  }

  QCOM_AW_PHY_LOG_INFO("CXO OSC term: setting mode %d on all PHY instances",
                       term_mode);

  for (phy_inst_type = QCOM_AW_PHY_INST_FH0;
       phy_inst_type < QCOM_AW_PHY_INST_MAX; phy_inst_type++) {
    phy_inst_info = &phy_config_info->phy_inst_config_info[phy_inst_type];
    if (!phy_inst_info || !phy_inst_info->valid)
      continue;

    mss.phy_offset = phy_inst_info->base_addr;
    mss.lane_offset = 0;

    ret = aw_pmd_refclk_termination_set(&mss, term_mode);
    if (ret != AW_ERR_CODE_NONE)
      QCOM_AW_PHY_LOG_ERR(
          "CXO OSC term: aw_pmd_refclk_termination_set failed "
          "phy_inst=%d ret=%d", phy_inst_type, ret);
    else
      QCOM_AW_PHY_LOG_INFO(
          "CXO OSC term: phy_inst=%d mode=%d OK", phy_inst_type, term_mode);
  }
}

/*-------------------------------------------------------------------
* qcom_aw_phy_hw_init

* @pdev: platform device pointer

* Description: This function does the clock setting, firmware
  loading.
------------------------------------------------------------------- */
static void qcom_aw_phy_hw_init() {
  struct qcom_aw_phy_config *phy_config_info = NULL;
  enum qcom_aw_phy_instance_enum phy_inst_type = QCOM_AW_PHY_INST_FH0;
  struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
  mss_access_t mss = {.phy_offset = 0, .lane_offset = 0};
  u32 sram_cfg_val = 2;
  bool phy_input_config = false;
  uint32_t version_raw;
  enum local_error_enum local_err_val = LOCAL_ERROR_INVALID;
  int ret_val = 0;
  char wq_name[QCOM_AW_PHY_WORKQ_NAME_MAX_LEN] = {0};

  QCOM_AW_PHY_LOG_INFO("qcom_aw_phy_hw_init!");

  /* PHY Inititalization sequence
     First two steps are not handled by PHY driver.
       Power-on-Reset (Pin name - irst_por_ba)
       APB reset (Pin name - irst_apb4_presetn) */

  phy_config_info = qcom_aw_phy_get_config_info();
  if (!phy_config_info) {
    local_err_val = LOCAL_ERROR_0;
    goto func_exit;
  }

  // Reference clock propagation using PHY inputs
  if (phy_input_config) {

    for (phy_inst_type = QCOM_AW_PHY_INST_DEBUG_C2C;
         QCOM_AW_PHY_INST_VALID(phy_inst_type); phy_inst_type--) {
      phy_inst_info = &phy_config_info->phy_inst_config_info[phy_inst_type];
      if (phy_inst_info && phy_inst_info->valid) {

        sv_write_field(phy_inst_info->wrapper_base_addr +
                           QCOM_AW_PHY_WRAPPER_PHY_ICTL_CMN_CFG_OFFSET,
                       0x1, ICTL_CMN_CFG_REF_LS_ENA_A_OFFSET,
                       ICTL_CMN_CFG_REF_LS_ENA_A_MASK);
      }
    }

    sv_write_field(phy_config_info->tcsr_base_addr +
                       TCSR_FH0_ICTL_REF_CML_LS_REG_OFFSET,
                   0x0, B2T_0_MUX_SEL_NT_OFFSET, B2T_0_MUX_SEL_NT_MASK);
    sv_write_field(phy_config_info->tcsr_base_addr +
                       TCSR_FH1_ICTL_REF_CML_LS_REG_OFFSET,
                   0x1, B2T_0_MUX_SEL_NT_OFFSET, B2T_0_MUX_SEL_NT_MASK);
    sv_write_field(phy_config_info->tcsr_base_addr +
                       TCSR_FH2_ICTL_REF_CML_LS_REG_OFFSET,
                   0x1, B2T_0_MUX_SEL_NT_OFFSET, B2T_0_MUX_SEL_NT_MASK);
    sv_write_field(phy_config_info->tcsr_base_addr +
                       TCSR_C2C_L2_ICTL_REF_CML_LS_REG_OFFSET,
                   0x1, B2T_0_MUX_SEL_NT_OFFSET, B2T_0_MUX_SEL_NT_MASK);
    sv_write_field(phy_config_info->tcsr_base_addr +
                       TCSR_C2C_DBG_ICTL_REF_CML_LS_REG_OFFSET,
                   0x3, B2T_0_MUX_SEL_NT_OFFSET, B2T_0_MUX_SEL_NT_MASK);

    sv_write_field(phy_config_info->tcsr_base_addr +
                       TCSR_FH0_ICTL_REF_LS_MUX_SEL_REG_OFFSET,
                   0x1, LS_MUX_SEL_NT_OFFSET, LS_MUX_SEL_NT_MASK);
    sv_write_field(phy_config_info->tcsr_base_addr +
                       TCSR_FH1_ICTL_REF_LS_MUX_SEL_REG_OFFSET,
                   0x1, LS_MUX_SEL_NT_OFFSET, LS_MUX_SEL_NT_MASK);
    sv_write_field(phy_config_info->tcsr_base_addr +
                       TCSR_FH2_ICTL_REF_LS_MUX_SEL_REG_OFFSET,
                   0x1, LS_MUX_SEL_NT_OFFSET, LS_MUX_SEL_NT_MASK);
    sv_write_field(phy_config_info->tcsr_base_addr +
                       TCSR_C2C_L2_ICTL_REF_LS_MUX_SEL_REG_OFFSET,
                   0x1, LS_MUX_SEL_NT_OFFSET, LS_MUX_SEL_NT_MASK);
    sv_write_field(phy_config_info->tcsr_base_addr +
                       TCSR_C2C_DBG_ICTL_REF_LS_MUX_SEL_REG_OFFSET,
                   0x4, LS_MUX_SEL_NT_OFFSET, LS_MUX_SEL_NT_MASK);
  }
  // Reference clock propagation using PHY internal registers
  else {

    QCOM_AW_PHY_LOG_INFO("REF clock in mode %d (0-silabs, 1- osc)",
                         qcom_aw_phy_ref_clk_mode);

    for (phy_inst_type = QCOM_AW_PHY_INST_FH0;
         phy_inst_type < QCOM_AW_PHY_INST_MAX; phy_inst_type++) {
      phy_inst_info = &phy_config_info->phy_inst_config_info[phy_inst_type];
      if (phy_inst_info && phy_inst_info->valid) {
        mss.phy_offset = phy_inst_info->base_addr;
        qcom_aw_phy_enable_ref_clk_propagation(phy_inst_info);
      }
    }
  }

  /* Set CXO oscillator clock termination (ETH_CXO_OSC CMN_REFCLK register).
   * Must follow ref-clock propagation so the CMN block is live.
   * Default: AW_RC_R100_DF (100 Ω differential) for board bring-up. */
  qcom_aw_phy_set_cxo_osc_termination(phy_config_info);

  mdelay(500);

  // Disable SyncE ACGC output by setting SyncE MUX to no lane.
  qcom_aw_phy_synce_set_synce_mux(LANE_NONE);

  if(qcom_aw_phy_any_lane_no_loopback()){
    /* Allocate and start workqueue for RX signal detect handling */
    phy_config_info->rx_sig_detect_wq =
                    create_singlethread_workqueue("qcom_aw_phy_rx_sig_det_wq");
    if (!phy_config_info->rx_sig_detect_wq) {
      local_err_val = LOCAL_ERROR_1;
      goto func_exit;
    }

    INIT_DELAYED_WORK(&phy_config_info->rx_sig_detect_wq_item.wq_item,
                      qcom_aw_phy_handle_rx_sig_detect);
    queue_delayed_work(phy_config_info->rx_sig_detect_wq,
                       &phy_config_info->rx_sig_detect_wq_item.wq_item, 0);
  }

  for (phy_inst_type = QCOM_AW_PHY_INST_FH0;
       phy_inst_type < QCOM_AW_PHY_INST_MAX; phy_inst_type++) {
    phy_inst_info = &phy_config_info->phy_inst_config_info[phy_inst_type];
    if (phy_inst_info && phy_inst_info->valid) {

      /* Allocate workqueue */
      snprintf(wq_name, sizeof(wq_name), "%s_%d",
               QCOM_AW_PHY_WORKQ_NAME_PREFIX, phy_inst_type);
      phy_inst_info->wq = create_singlethread_workqueue(wq_name);
      if (!phy_inst_info->wq) {
        local_err_val = LOCAL_ERROR_2;
        goto func_exit;
      }

      mss.phy_offset = phy_inst_info->base_addr;

      pmd_write_field(&mss, RXMFSM_SCRATCH_REG7_ADDR,
                      RXMFSM_SCRATCH_REG7_RXMFSM_SCRATCH7_MASK,
                      RXMFSM_SCRATCH_REG7_RXMFSM_SCRATCH7_OFFSET, 0xFF);

      /* PHY SRAM Configuration */
      aw_pmd_rd_data_pipeline_stages_set(&mss, sram_cfg_val);

      /* Load firmware/hex file*/
      qcom_aw_phy_load_hexfile(
          &mss, "/lib/firmware/qcom_aw_phy/eth_custom_rates_1.hex");

      if(phy_inst_type == QCOM_AW_PHY_INST_DEBUG_C2C){
        iowrite32(0x4, phy_inst_info->wrapper_base_addr +
                             QCOM_AW_PHY_WRAPPER_PHY_ICTL_AN_MASTER_CFG_OFFSET);
      }

#ifndef FEATURE_QCOM_AW_RUMI_SW
      /* Register for PHY status IRQ */
      ret_val = devm_request_irq(
          &phy_inst_info->pdev->dev, phy_inst_info->phy_status_irq,
          (irq_handler_t)qcom_aw_phy_interrupt_handler,
          IRQF_SHARED | IRQF_TRIGGER_HIGH | IRQF_ONESHOT, NULL, phy_inst_info);
      if (ret_val) {
        local_err_val = LOCAL_ERROR_3;
        goto func_exit;
      }

      ret_val = enable_irq_wake(phy_inst_info->phy_status_irq);
      if (ret_val) {
        local_err_val = LOCAL_ERROR_4;
        goto func_exit;
      }

      /* Enable the interrupts which PHY driver is interested in */
      qcom_aw_phy_enable_interrupt(phy_inst_info);
#endif
    }
  }
  pmd_read_addr(&mss,0x80000000, &version_raw);
  phy_config_info->fw_major_ver= ((version_raw >> 16) & 0xFF);
  phy_config_info->fw_minor_ver= ((version_raw >> 8) & 0xFF);
  phy_config_info->fw_patch_ver= (version_raw & 0xFF);
  QCOM_AW_PHY_LOG_INFO("FW loaded : Version MAJOR = %d "
                       "Version MINOR = %d Version PATCH = %d\n",
                       (version_raw >> 16) & 0xFF,
                       (version_raw >> 8) & 0xFF, version_raw & 0xFF);


  QCOM_AW_PHY_LOG_INFO("PHY IRQ register done\n");

func_exit:
  QCOM_AW_PHY_LOG_ERR("%s: local error %d", __func__, local_err_val);

  return;
}

/*-------------------------------------------------------------------
* qcom_aw_phy_inst_probe

* @pdev: platform device pointer

* Description: This function does the instance level probe handling.
------------------------------------------------------------------- */
static int qcom_aw_phy_inst_probe(struct platform_device *pdev) {
  struct device_node *np;
  u32 phy_inst_type;
  struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
  struct resource *phy_resource;
  enum local_error_enum local_err_val = LOCAL_ERROR_INVALID;
  int ret_val = 0;
  struct resource *tcsr_resource;
  struct pinctrl *pinctrl;
  uint8_t i = 0;

  QCOM_AW_PHY_LOG_INFO("QCOM AW PHY driver instance probed for device %s!",
                       pdev->name);

  /* Incrment the PHY instance count */
  qcom_aw_phy_config_info.num_phy_instances++;

  /* DT read */
  np = pdev->dev.of_node;

  /* Get the PHY instance type */
  ret_val =
      of_property_read_u32(pdev->dev.of_node, "phy-inst-type", &phy_inst_type);
  if (!ret_val) {
    QCOM_AW_PHY_LOG_INFO("phy_inst_type = %d, ret: %d", phy_inst_type, ret_val);

    if (phy_inst_type < QCOM_AW_PHY_INST_MAX) {
      phy_inst_info =
          &qcom_aw_phy_config_info.phy_inst_config_info[phy_inst_type];

      /* Initialize the PHY instance fields */
      phy_inst_info->valid = true;
      phy_inst_info->phy_inst = phy_inst_type;

      mutex_init(&phy_inst_info->phy_inst_lock);

      for (i = 0; i < PHY_LANE_MAX; i++) {
        mutex_init(&phy_inst_info->lane_lock[i]);

        phy_inst_info->lane_params[i].an_restart_wq_item.phy_inst = phy_inst_type;
        phy_inst_info->lane_params[i].an_restart_wq_item.lane_num = i;
        INIT_DELAYED_WORK(&phy_inst_info->lane_params[i].an_restart_wq_item.wq_item,
                          qcom_aw_phy_handle_an_restart);

        phy_inst_info->lane_params[i].snr_valid_err_wq_item.phy_inst = phy_inst_type;
        phy_inst_info->lane_params[i].snr_valid_err_wq_item.lane_num = i;
        phy_inst_info->lane_params[i].snr_valid_err_wq_item.user_data = (void*)false;
        INIT_WORK(&phy_inst_info->lane_params[i].snr_valid_err_wq_item.wq_item,
                  qcom_aw_phy_synce_handle_snr_valid_change);

        phy_inst_info->lane_params[i].snr_valid_wq_item.phy_inst = phy_inst_type;
        phy_inst_info->lane_params[i].snr_valid_wq_item.lane_num = i;
        phy_inst_info->lane_params[i].snr_valid_wq_item.user_data = (void*)true;
        INIT_WORK(&phy_inst_info->lane_params[i].snr_valid_wq_item.wq_item,
                  qcom_aw_phy_synce_handle_snr_valid_change);

        phy_inst_info->lane_params[i].an_done_wq_item.phy_inst = phy_inst_type;
        phy_inst_info->lane_params[i].an_done_wq_item.lane_num = i;
        INIT_WORK(&phy_inst_info->lane_params[i].an_done_wq_item.wq_item,
                  qcom_aw_phy_handle_an_done);
        
        phy_inst_info->lane_params[i].an_link_good_wq_item.phy_inst = phy_inst_type;
        phy_inst_info->lane_params[i].an_link_good_wq_item.lane_num = i;
        INIT_WORK(&phy_inst_info->lane_params[i].an_link_good_wq_item.wq_item,
                  qcom_aw_phy_handle_an_link_good);
      }
    } else {
      local_err_val = LOCAL_ERROR_0;
      ret_val = EINVAL;
      goto func_exit;
    }
  } else {
    local_err_val = LOCAL_ERROR_1;
    ret_val = EINVAL;
    goto func_exit;
  }

  if (phy_inst_type == QCOM_AW_PHY_INST_FH0) {

    // Enable regulator
    if(qcom_aw_phy_ref_clk_mode == REF_CLK_MODE_OSCILLATOR){
      if (of_property_read_bool(pdev->dev.of_node, "vdd-supply")) {
        qcom_aw_phy_config_info.ldo16_supply =
                                            devm_regulator_get(&pdev->dev, "vdd");
        if (IS_ERR(qcom_aw_phy_config_info.ldo16_supply)) {
          QCOM_AW_PHY_LOG_ERR("Cannot get <%s>\n", "ld016");
          return PTR_ERR(qcom_aw_phy_config_info.ldo16_supply);
        }

        ret_val = regulator_set_load(qcom_aw_phy_config_info.ldo16_supply, 10000);
        if (ret_val) {
          QCOM_AW_PHY_LOG_ERR("Can not set Regulator Load <%s>\n", "ldo16");
        }

        ret_val = regulator_set_voltage(qcom_aw_phy_config_info.ldo16_supply,
                                        1800000, 1800000);
        if (ret_val) {
          QCOM_AW_PHY_LOG_ERR("Can not set Regulator Voltage enable <%s>\n",
                              "ldo16");
        }

        ret_val = regulator_enable(qcom_aw_phy_config_info.ldo16_supply);
        if (ret_val) {
          QCOM_AW_PHY_LOG_ERR("Can not enable <%s>\n", "ldo16");
          goto func_exit;
        }

        QCOM_AW_PHY_LOG_INFO("Enabled <%s>\n", "ldo16");
      }

      pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
      if (IS_ERR(pinctrl))
        QCOM_AW_PHY_LOG_ERR("No default pinctrl found\n");
    }

    // Read TCSR base address and apply clamp register setting
    tcsr_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "tcsr");
    if (!tcsr_resource) {
      local_err_val = LOCAL_ERROR_2;
      goto func_exit;
    }

    qcom_aw_phy_config_info.tcsr_base_addr =
        devm_ioremap_resource(&pdev->dev, tcsr_resource);

    write_csr(qcom_aw_phy_config_info.tcsr_base_addr +
                  TCSR_ETH_CLAMP_EN_REG_OFFSET,
              0x0);

    // Fetch the ACGC reset control handle
    qcom_aw_phy_config_info.acgc_reset_ctrl =
                        devm_reset_control_get(&pdev->dev, "synce_acgc_reset");
    if (IS_ERR(qcom_aw_phy_config_info.acgc_reset_ctrl)) {
      local_err_val = LOCAL_ERROR_3;
      goto func_exit;
    }

    /* Fetch the SyncE clocks */
    qcom_aw_phy_setup_synce_clocks(&pdev->dev);

    /* Enable PHY clocks */
    qcom_aw_phy_setup_clocks(&pdev->dev);
  }

  /* Read the PHY TOP address */
  phy_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy-top");
  if (!phy_resource) {
    local_err_val = LOCAL_ERROR_4;
    goto func_exit;
  }

  QCOM_AW_PHY_LOG_INFO("phy-top base: %x, phy-top size = %x",
                       phy_resource->start, resource_size(phy_resource));

#ifndef FEATURE_QCOM_AW_RUMI_SW
  phy_inst_info->base_addr = devm_ioremap_resource(&pdev->dev, phy_resource);
#endif

  /* Read the PHY wrapper address */
  phy_resource =
      platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy-wrapper");
  if (!phy_resource) {
    local_err_val = LOCAL_ERROR_5;
    goto func_exit;
  }

  QCOM_AW_PHY_LOG_INFO("phy-wrapper base: %x, phy-wrapper size = %x",
                       phy_resource->start, resource_size(phy_resource));

#ifndef FEATURE_QCOM_AW_RUMI_SW
  phy_inst_info->wrapper_base_addr =
      devm_ioremap_resource(&pdev->dev, phy_resource);
#endif

  /* Get the PHY equalization mode */
  phy_inst_info->phy_eq_mode =
      QCOM_AW_PHY_MANUAL_EQ_MODE; // Default value for now

  /* Default AN adv ability set to 25G_BaseR */
  phy_inst_info->an_params.adv_ability[PHY_SPEED_SPEC_25G_BASE_K_CR_S] = 1;
  phy_inst_info->an_params.adv_ability[PHY_SPEED_SPEC_25G_BASE_K_CR] = 1;

  /* Get the IRQ info */
  phy_inst_info->phy_status_irq = platform_get_irq_byname(pdev, "phy-irq");
  if (phy_inst_info->phy_status_irq < 0) {
    local_err_val = LOCAL_ERROR_6;
    ret_val = ENODEV;
#ifndef FEATURE_QCOM_AW_RUMI_SW
    goto func_exit;
#endif
  }

  QCOM_AW_PHY_LOG_INFO("PHY IRQ = %d", phy_inst_info->phy_status_irq);

  /* Store the platform device */
  phy_inst_info->pdev = pdev;

  if (QCOM_AW_PHY_INST_MAX == qcom_aw_phy_config_info.num_phy_instances) {
    QCOM_AW_PHY_LOG_INFO("Max PHY instance probe reached !");

    /* Initialize PHY HW */
    qcom_aw_phy_hw_init();

    /* Indicate PHY driver readiness to MAC */
    qcom_aw_phy_set_drv_ready();
  }

func_exit:
  QCOM_AW_PHY_LOG_ERR("%s: returns %d with local error %d", __func__, ret_val,
                      local_err_val);

  return ret_val;
}

/*-------------------------------------------------------------------
* aw_phy_remove

* @pdev: platform device pointer

* Description: This function resets PHY driver module parameters.
------------------------------------------------------------------- */
static int qcom_aw_phy_inst_remove(struct platform_device *pdev) {

  QCOM_AW_PHY_LOG_INFO("qcom_aw_phy_inst_remove");

  return 0;
}

/* PHY Driver Instance Compatible string */
static const struct of_device_id qcom_aw_phy_inst_match[] = {
    {.compatible = "qcom-aw-phy-inst"}, {}};

MODULE_DEVICE_TABLE(of, qcom_aw_phy_inst_match);

/* Platform Driver Structure - Instance*/
static struct platform_driver qcom_aw_phy_inst_driver = {
    .probe = qcom_aw_phy_inst_probe,
    .remove = qcom_aw_phy_inst_remove,
    .driver =
        {
            .name = QCOM_AW_PHY_DRV_INST_NAME,
            .of_match_table = of_match_ptr(qcom_aw_phy_inst_match),
        },
};

/* Module Init Function */
static int __init qcom_aw_phy_init(void) {
  int ret_val;

  memset(&qcom_aw_phy_config_info, 0, sizeof(struct qcom_aw_phy_config));

  qcom_aw_phy_config_info.phy_ipc_log_buf =
                               ipc_log_context_create(PHY_IPC_LOG_PAGES,
                                                      "qcom_aw_phy", 0);
  if(qcom_aw_phy_config_info.phy_ipc_log_buf == NULL)
    QCOM_AW_PHY_LOG_ERR("Failed to create IPC log context");

  qcom_aw_phy_config_info.phy_ipc_log_buf_low =
                               ipc_log_context_create(PHY_IPC_LOG_PAGES,
                                                      "qcom_aw_phy_low", 0);
  if(qcom_aw_phy_config_info.phy_ipc_log_buf_low == NULL)
    QCOM_AW_PHY_LOG_ERR("Failed to create IPC low log context");

  qcom_aw_phy_mtip_if_init();

  qcom_aw_phy_gnl_init();
  qcom_aw_phy_prbs_gnl_init();

#ifdef FEATURE_QCOM_AW_DEBUG_FS
  qcom_aw_phy_setup_debugfs();
#endif

  qcom_aw_phy_setup_sysfs();

  ret_val = platform_driver_register(&qcom_aw_phy_inst_driver);
  if (ret_val < 0) {
    QCOM_AW_PHY_LOG_INFO(
        "Platform registration failed for PHY instance driver !");
  }

  return 0;
}

/* Module Exit Function */
static void __exit qcom_aw_phy_exit(void) {

  enum qcom_aw_phy_instance_enum phy_inst_type = QCOM_AW_PHY_INST_MAX;
  struct qcom_aw_phy_inst_config *phy_inst_info = NULL;
  enum eth_phy_iface_phy_lane_num_enum lane = PHY_LANE_0;
  extern struct qcom_aw_phy_mtip_if_info qcom_aw_phy_mtip_if_info_s;

  QCOM_AW_PHY_LOG_INFO("qcom_aw_phy_exit");

  if(qcom_aw_phy_config_info.ldo16_supply){
    regulator_disable(qcom_aw_phy_config_info.ldo16_supply);
    qcom_aw_phy_config_info.ldo16_supply = NULL;
  }

  for(phy_inst_type = QCOM_AW_PHY_INST_FH0; phy_inst_type < QCOM_AW_PHY_INST_MAX;phy_inst_type ++){
    phy_inst_info = &qcom_aw_phy_config_info.phy_inst_config_info[phy_inst_type];
    if(phy_inst_info) {

      /* Free the IRQ to stop further interrupts */
      disable_irq_wake(phy_inst_info->phy_status_irq);
      devm_free_irq(&phy_inst_info->pdev->dev, phy_inst_info->phy_status_irq,
                    phy_inst_info);

      for (lane = PHY_LANE_0; lane < PHY_LANE_MAX; lane++) {
        cancel_delayed_work_sync(
                  &phy_inst_info->lane_params[lane].an_restart_wq_item.wq_item);
      }
    }
  }

  if (qcom_aw_phy_config_info.rx_sig_detect_wq != NULL) {
    cancel_delayed_work_sync(
                      &qcom_aw_phy_config_info.rx_sig_detect_wq_item.wq_item);
    destroy_workqueue(qcom_aw_phy_config_info.rx_sig_detect_wq);
  }

  for(phy_inst_type = QCOM_AW_PHY_INST_FH0; phy_inst_type < QCOM_AW_PHY_INST_MAX;phy_inst_type ++)  {

    phy_inst_info = &qcom_aw_phy_config_info.phy_inst_config_info[phy_inst_type];

    flush_workqueue(phy_inst_info->wq);
    destroy_workqueue(phy_inst_info->wq);

    if(phy_inst_info) {
      for (lane = PHY_LANE_0; lane < PHY_LANE_MAX; lane++) {
        mutex_destroy(&phy_inst_info->lane_lock[lane]);
      }
    }
    mutex_destroy(&phy_inst_info->phy_inst_lock);
  }

  mutex_destroy(&qcom_aw_phy_mtip_if_info_s.lock);

  qcom_aw_phy_gnl_exit();
  qcom_aw_phy_prbs_gnl_exit();

#ifdef FEATURE_QCOM_AW_DEBUG_FS
  qcom_aw_phy_del_debugfs();
#endif

  qcom_aw_phy_del_sysfs();

  if(qcom_aw_phy_config_info.phy_ipc_log_buf){
    ipc_log_context_destroy(qcom_aw_phy_config_info.phy_ipc_log_buf);
    qcom_aw_phy_config_info.phy_ipc_log_buf = NULL;
  }

  if(qcom_aw_phy_config_info.phy_ipc_log_buf_low){
    ipc_log_context_destroy(qcom_aw_phy_config_info.phy_ipc_log_buf_low);
    qcom_aw_phy_config_info.phy_ipc_log_buf_low = NULL;
  }

  platform_driver_unregister(&qcom_aw_phy_inst_driver);

  return;
}

module_init(qcom_aw_phy_init);
module_exit(qcom_aw_phy_exit);
MODULE_DESCRIPTION("QCOM AW PHY Device Driver");
MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);

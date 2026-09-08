/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
//#include <linux/delay.h>
#include <net/netlink.h>
#include <net/net_namespace.h>
#include <linux/signal.h>           /* Definition of SIGEV_* constants */
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/netdevice.h>
#include <linux/of_device.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/atomic.h>
#include <linux/fs.h>
#include <asm/io.h>

#include "ecpriss_xbar_hal.h"
#include "ecpriss_log.h"

static const char *ecpriss_xbar_hal_reg_name_to_str[ECPRI_XBAR_MAX+1] = {

	"ECPRI_XBAR_XBAR_CFG",
	"ECPRI_XBAR_XBAR_C2C_IPG_CFG" ,
	"ECPRI_XBAR_XBAR_C2C_CFG",
	"ECPRI_XBAR_XBAR_FH_CFG",
	"ECPRI_XBAR_XBAR_OC_CFG",
	"ECPRI_XBAR_XBAR_OC_LOOPBACK_CFG",
	"ECPRI_XBAR_XBAR_FLUSH",
	"ECPRI_XBAR_XBAR_FH_STATUS",
	"ECPRI_XBAR_XBAR_C2C_STATUS",
	"ECPRI_XBAR_XBAR_OCTX_STATUS",
	"ECPRI_XBAR_XBAR_LUT_STATUS",
	"ECPRI_XBAR_XBAR_FW_IRQ_STATUS",
	"ECPRI_XBAR_XBAR_FW_IRQ_MASK",
	"ECPRI_XBAR_XBAR_FW_IRQ_CLR",
	"ECPRI_XBAR_XBAR_SW_IRQ_STATUS",
	"ECPRI_XBAR_XBAR_SW_IRQ_MASK",
	"ECPRI_XBAR_XBAR_SW_IRQ_CLR",
	"ECPRI_XBAR_XBAR_TPDM_GLOBAL_CFG",
	"ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT",
	"ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT",
	"ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT",
	"ECPRI_XBAR_XBAR_FHRX_n_DEFAULT_LUT",
	"ECPRI_XBAR_XBAR_FHRX_MSG_TYPE_LUT_0",
	"ECPRI_XBAR_XBAR_FHRX_MSG_TYPE_LUT_1",
	"ECPRI_XBAR_XBAR_FHRX_MSG_TYPE_n_LUT_RESULT",
	"ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_0",
	"ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_1",
	"ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_n_LUT_RESULT",
	"ECPRI_XBAR_XBAR_C2CRX_n_DL_DEFAULT_LUT",
	"ECPRI_XBAR_XBAR_C2CRX_n_UL_DEFAULT_LUT",
	"ECPRI_XBAR_XBAR_C2CRX_NON_ECPRI_LUT",
	"ECPRI_XBAR_XBAR_OCRX_FH_n_DEFAULT_LUT",
	"ECPRI_XBAR_XBAR_OCRX_C2C_DEFAULT_LUT",
	"ECPRI_XBAR_XBAR_UC_FHRX_BUFF_ADDR",
	"ECPRI_XBAR_XBAR_UC_FHRX_BUFF_SIZE",
	"ECPRI_XBAR_XBAR_UC_FHRX_BUFF_RD",
	"ECPRI_XBAR_XBAR_UC_FHRX_PKT_ATTR_POP",
	"ECPRI_XBAR_XBAR_UC_FHRX_PKT_ATTR_NO_POP",
	"ECPRI_XBAR_XBAR_UC_FHRX_STATUS",
	"ECPRI_XBAR_XBAR_UC_FH_CTRL",
	"ECPRI_XBAR_XBAR_UC_FHTX_PKT_ADDR",
	"ECPRI_XBAR_XBAR_UC_FHTX_PKT_ATTR",
	"ECPRI_XBAR_LUT_XBAR_FHRX_m_LUT_n",
	"ECPRI_XBAR_LUT_XBAR_C2CRX_m_DL_LUT_n",
	"ECPRI_XBAR_LUT_XBAR_C2CRX_m_UL_LUT_n",
	"ECPRI_XBAR_LUT_XBAR_OCRX_m_LUT_n",
	//Stats
	"ECPRI_XBAR_XBAR_DBG_FHRX_PKT_CNT_n",//per link
	"ECPRI_XBAR_XBAR_DBG_FHRX_DMA_PKT_CNT",
	"ECPRI_XBAR_XBAR_DBG_FHRX_UC_PKT_CNT",
	"ECPRI_XBAR_XBAR_DBG_FHRX_UC_ERR_PKT_CNT",
	"ECPRI_XBAR_XBAR_DBG_FHRX_ERR_PKT_CNT",
	"ECPRI_XBAR_XBAR_DBG_FHTX_PKT_CNT_n",//per link
	"ECPRI_XBAR_XBAR_DBG_FHTX_C2C_PKT_OVF_CNT",
	"ECPRI_XBAR_XBAR_DBG_FHTX_DMA_PKT_CNT",
	"ECPRI_XBAR_XBAR_DBG_FHTX_UC_PKT_CNT",
	"ECPRI_XBAR_XBAR_DBG_C2CRX_PKT_CNT_n",// per link
	"ECPRI_XBAR_XBAR_DBG_C2CRX_DMA_PKT_CNT",
	"ECPRI_XBAR_XBAR_DBG_C2CRX_ERR_PKT_CNT",
	"ECPRI_XBAR_XBAR_DBG_C2CTX_PKT_CNT_n",//per link
	"ECPRI_XBAR_XBAR_DBG_C2CTX_FH_PKT_OVF_CNT",
	"ECPRI_XBAR_XBAR_DBG_C2CTX_DMA_PKT_CNT",
	"ECPRI_XBAR_XBAR_DBG_OCTX_C2C_PKT_DROP_CNT",
	"ECPRI_XBAR_XBAR_DBG_OCTX_PKT_CNT_n", //per link
	"ECPRI_XBAR_XBAR_DBG_OCRX_PKT_CNT_n", //per link
	"ECPRI_XBAR_XBAR_DBG_OCRX_FH_PKT_CNT_n",//per link
	"ECPRI_XBAR_XBAR_DBG_OCRX_C2C_PKT_CNT_n", //per link
	"ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT",
	"ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT",
	"ECPRI_XBAR_XBAR_DBG_C2CRX_UNKNOWN_PCID_CNT",
	"ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_INFO_1_n",
	"ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_INFO_2_n",
	"ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_INFO_1_n",
	"ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_INFO_2_n",
	"ECPRI_XBAR_XBAR_DBG_OCRX_FH_BUFF_WATERMARK",
	"ECPRI_XBAR_XBAR_DBG_OCRX_0_1_BUFF_WATERMARK",
	"ECPRI_XBAR_XBAR_DBG_OCRX_2_3_BUFF_WATERMARK",
	"ECPRI_XBAR_XBAR_DBG_OCTX_OC_0_1_BUFF_WATERMARK",
	"ECPRI_XBAR_XBAR_DBG_OCTX_OC_2_3_BUFF_WATERMARK",

	"ECPRI_XBAR_CFG",
	"ECPRI_XBAR_LUT_STATUS",
	"ECPRI_XBAR_FLUSH",
	"ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG",
	"ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID",
	"ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG",
	"ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID",
	"ECPRI_XBAR_FHRX_MSG_TYPE_LUT_0",
	"ECPRI_XBAR_FHRX_MSG_TYPE_LUT_1",
	"ECPRI_XBAR_SW_IRQ_STATUS",
	"ECPRI_XBAR_SW_IRQ_MASK",
	"ECPRI_XBAR_SW_IRQ_CLR",
	"ECPRI_XBAR_LUT_XBAR_FHRX_m_LUT_n",
	"ECPRI_XBAR_LUT_XBAR_C2CRX_m_LUT_n",
	"ECPRI_XBAR_LUT_XBAR_OCRX_m_LUT_n",

	"ECPRI_XBAR_DBG_FHRX_PKT_CNT_n",
	"ECPRI_XBAR_DBG_FHRX_DMA_PKT_CNT",
	"ECPRI_XBAR_DBG_FHRX_UC_PKT_CNT",
	"ECPRI_XBAR_DBG_FHRX_UC_ERR_PKT_CNT",
	"ECPRI_XBAR_DBG_FHRX_ERR_PKT_CNT",
	"ECPRI_XBAR_DBG_FHRX_OC_PKT_CNT",
	"ECPRI_XBAR_DBG_FHRX_C2C_PKT_CNT",
	"ECPRI_XBAR_DBG_FHTX_PKT_CNT_n",
	"ECPRI_XBAR_DBG_FHTX_C2C_PKT_OVF_DROP_CNT",
	"ECPRI_XBAR_DBG_FHTX_DMA_PKT_CNT",
	"ECPRI_XBAR_DBG_FHTX_UC_PKT_CNT",
	"ECPRI_XBAR_DBG_FHTX_C2C_PKT_CNT_n",
	"ECPRI_XBAR_DBG_FHTX_OC_PKT_CNT_n",

	"ECPRI_XBAR_DBG_C2CRX_PKT_CNT",
	"ECPRI_XBAR_DBG_C2CRX_DMA_PKT_CNT",
	"ECPRI_XBAR_DBG_C2CRX_ERR_PKT_CNT_n",
	"ECPRI_XBAR_DBG_C2CTX_PKT_CNT_n",
	"ECPRI_XBAR_DBG_C2CTX_FH_PKT_OVF_DROP_CNT",
	"ECPRI_XBAR_DBG_C2CTX_DMA_PKT_CNT",
	"ECPRI_XBAR_DBG_C2CRX_FH_PKT_CNT",
	"ECPRI_XBAR_DBG_C2CRX_OC_PKT_CNT",

	"ECPRI_XBAR_DBG_OCTX_C2C_PKT_DROP_CNT",
	"ECPRI_XBAR_DBG_OCTX_FH_PKT_DROP_CNT",
	"ECPRI_XBAR_DBG_OCTX_C2C_LEN_ERR_CNT",
	"ECPRI_XBAR_DBG_OCTX_FH_LEN_ERR_CNT",
	"ECPRI_XBAR_DBG_OCTX_PKT_CNT_n",
	"ECPRI_XBAR_DBG_OCRX_PKT_CNT_n",
	"ECPRI_XBAR_DBG_OCRX_FH_PKT_CNT_n",
	"ECPRI_XBAR_DBG_OCRX_C2C_PKT_CNT_n",


	"ECPRI_XBAR_LUT_XBAR_NFAPI_n",

	"ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT",
        "ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_INFO_1_n",
        "ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_INFO_2_n",
        "ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT",
        "ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_INFO_1_n",
        "ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_INFO_2_n",
        "ECPRI_XBAR_DBG_OCRX_0_1_BUFF_WATERMARK",
        "ECPRI_XBAR_DBG_OCRX_2_3_BUFF_WATERMARK",
        "ECPRI_XBAR_DBG_OCRX_FH_BUFF_WATERMARK",
        "ECPRI_XBAR_DBG_OCTX_OC_0_1_BUFF_WATERMARK",
        "ECPRI_XBAR_DBG_OCTX_OC_2_3_BUFF_WATERMARK",
	"ECPRI_XBAR_FH_CFG",
	"ECPRI_XBAR_OC_CFG",
	"ECPRI_XBAR_C2CRX_n_DEFAULT_LUT",
	"ECPRI_XBAR_C2CRX_n_DEFAULT_DMA_CHANNEL",

	"ECPRI_XBAR_MAX"

};
ecpriss_xbar_hal_context_s ecpriss_xbar_hal_ctx;

/*
 * ecpriss_xbar_hal_reg_name_str() - returns string that represent the register
 * @reg_name: [in] register name
 */
const char *ecpriss_xbar_hal_reg_name_str(enum ecpriss_xbar_hal_reg_name reg_name)
{
	if (reg_name < 0 || reg_name >= ECPRI_XBAR_MAX) {
		ECPRILOGERR("requested name of invalid reg=%d\n", reg_name);
		return "Invalid Register";
	}

	return ecpriss_xbar_hal_reg_name_to_str[reg_name];
}
	static void ecpriss_xbar_hal_reg_construct_xbar_cfg
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_cfg_s *xbar_cfg
		= (ecpri_xbar_hwio_def_ecpri_xbar_xbar_cfg_s *)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->fhrx_cnt_en,
			HWIO_ECPRI_XBAR_XBAR_CFG_FHRX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_FHRX_CNT_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->fhtx_cnt_en,
			HWIO_ECPRI_XBAR_XBAR_CFG_FHTX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_FHTX_CNT_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->ocrx_cnt_en,
			HWIO_ECPRI_XBAR_XBAR_CFG_OCRX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_OCRX_CNT_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->octx_cnt_en,
			HWIO_ECPRI_XBAR_XBAR_CFG_OCTX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_OCTX_CNT_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->c2crx_cnt_en,
			HWIO_ECPRI_XBAR_XBAR_CFG_C2CRX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_C2CRX_CNT_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->c2ctx_cnt_en,
			HWIO_ECPRI_XBAR_XBAR_CFG_C2CTX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_C2CTX_CNT_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->xbar_cnt_en,
			HWIO_ECPRI_XBAR_XBAR_CFG_XBAR_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_XBAR_CNT_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->xbar_cnt_clr_en,
			HWIO_ECPRI_XBAR_XBAR_CFG_XBAR_CNT_CLR_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_XBAR_CNT_CLR_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->tpdm_en,
			HWIO_ECPRI_XBAR_XBAR_CFG_TPDM_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_TPDM_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->axis_arb_fix_en,
			HWIO_ECPRI_XBAR_XBAR_CFG_AXIS_ARB_FIX_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_AXIS_ARB_FIX_EN_BMSK);


	return;
}

	static void ecpriss_xbar_hal_reg_construct_xbar_cfg_2
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_cfg_s *xbar_cfg
		= (ecpri_xbar_hwio_def_ecpri_xbar_cfg_s *)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->fhrx_cnt_en,
			HWIO_ECPRI_XBAR_CFG_FHRX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_FHRX_CNT_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->fhtx_cnt_en,
			HWIO_ECPRI_XBAR_CFG_FHTX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_FHTX_CNT_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->ocrx_cnt_en,
			HWIO_ECPRI_XBAR_CFG_OCRX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_OCRX_CNT_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->octx_cnt_en,
			HWIO_ECPRI_XBAR_CFG_OCTX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_OCTX_CNT_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->c2crx_cnt_en,
			HWIO_ECPRI_XBAR_CFG_C2CRX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_C2CRX_CNT_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->c2ctx_cnt_en,
			HWIO_ECPRI_XBAR_CFG_C2CTX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_C2CTX_CNT_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->xbar_cnt_en,
			HWIO_ECPRI_XBAR_CFG_XBAR_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_XBAR_CNT_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->xbar_cnt_clr_en,
			HWIO_ECPRI_XBAR_CFG_XBAR_CNT_CLR_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_XBAR_CNT_CLR_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->tpdm_en,
			HWIO_ECPRI_XBAR_CFG_TPDM_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_TPDM_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_cfg->axis_arb_fix_en,
			HWIO_ECPRI_XBAR_CFG_AXIS_ARB_FIX_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_AXIS_ARB_FIX_EN_BMSK);


	return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_cfg_2(enum ecpriss_xbar_hal_reg_name reg, void *fields,
		u32 val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_cfg_u *xbar_cfg;
	xbar_cfg = fields;


	xbar_cfg->def.c2crx_cnt_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_CFG_C2CRX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_C2CRX_CNT_EN_BMSK);
	xbar_cfg->def.c2ctx_cnt_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_CFG_C2CTX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_C2CTX_CNT_EN_BMSK);

	xbar_cfg->def.fhrx_cnt_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_CFG_FHRX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_FHRX_CNT_EN_BMSK);
	xbar_cfg->def.fhtx_cnt_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_CFG_FHTX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_FHTX_CNT_EN_BMSK);

	xbar_cfg->def.ocrx_cnt_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_CFG_OCRX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_OCRX_CNT_EN_BMSK);
	xbar_cfg->def.octx_cnt_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_CFG_OCTX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_OCTX_CNT_EN_BMSK);

	xbar_cfg->def.xbar_cnt_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_CFG_XBAR_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_XBAR_CNT_EN_BMSK);
	xbar_cfg->def.xbar_cnt_clr_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_CFG_XBAR_CNT_CLR_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_XBAR_CNT_CLR_EN_BMSK);

	xbar_cfg->def.tpdm_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_CFG_TPDM_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_TPDM_EN_BMSK);

	xbar_cfg->def.axis_arb_fix_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_CFG_AXIS_ARB_FIX_EN_SHFT,
			HWIO_ECPRI_XBAR_CFG_AXIS_ARB_FIX_EN_BMSK);


	return;
}

	static void ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_msg_type_uplane_cfg
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{

	ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_uplane_cfg_s * uplane_cfg;
	uplane_cfg =(ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_uplane_cfg_s*)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			uplane_cfg->value_0,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_0_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_0_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			uplane_cfg->value_1,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_1_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_1_BMSK);


	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			uplane_cfg->value_2,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_2_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_2_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			uplane_cfg->value_3,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_3_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_3_BMSK);


	return;

}

	static void ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_msg_type_cplane_cfg
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{

	ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_cplane_cfg_s * cplane_cfg;
	cplane_cfg =(ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_cplane_cfg_s*)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			cplane_cfg->value_0,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_0_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_0_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			cplane_cfg->value_1,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_1_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_1_BMSK);


	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			cplane_cfg->value_2,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_2_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_2_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			cplane_cfg->value_3,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_3_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_3_BMSK);


	return;

}

	static void ecpriss_xbar_hal_reg_parse_lut_xbar_fhrx_msg_type_uplane_cfg
(enum ecpriss_xbar_hal_reg_name reg, void *fields, u32 val)
{

	ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_uplane_cfg_s * uplane_cfg;
	uplane_cfg =(ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_uplane_cfg_s*)fields;

	uplane_cfg->value_0 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_0_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_0_BMSK);

	uplane_cfg->value_1 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_1_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_1_BMSK);

	uplane_cfg->value_2 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_2_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_2_BMSK);

	uplane_cfg->value_3 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_3_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG_VALUE_3_BMSK);


	return;

}

	static void ecpriss_xbar_hal_reg_parse_lut_xbar_fhrx_msg_type_cplane_cfg
(enum ecpriss_xbar_hal_reg_name reg, void *fields, u32 val)
{

	ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_cplane_cfg_s * cplane_cfg;
	cplane_cfg =(ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_cplane_cfg_s*)fields;

	cplane_cfg->value_0 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_0_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_0_BMSK);

	cplane_cfg->value_1 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_1_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_1_BMSK);

	cplane_cfg->value_2 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_2_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_2_BMSK);

	cplane_cfg->value_3 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_3_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG_VALUE_3_BMSK);


	return;

}

	static void ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_msg_type_uplane_valid
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_uplane_valid_s * uplane_valid;
	uplane_valid =(ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_uplane_valid_s* )fields;


	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			uplane_valid->valid_0,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_0_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_0_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			uplane_valid->valid_1,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_1_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_1_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			uplane_valid->valid_2,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_2_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_2_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			uplane_valid->valid_3,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_3_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_3_BMSK);


	return;

}

	static void ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_msg_type_cplane_valid
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_cplane_valid_s * cplane_valid;
	cplane_valid =(ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_cplane_valid_s *)fields;


	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			cplane_valid->valid_0,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_0_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_0_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			cplane_valid->valid_1,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_1_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_1_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			cplane_valid->valid_2,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_2_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_2_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			cplane_valid->valid_3,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_3_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_3_BMSK);


	return;

}


	static void ecpriss_xbar_hal_reg_parse_lut_xbar_fhrx_msg_type_uplane_valid
(enum ecpriss_xbar_hal_reg_name reg, void *fields, u32 val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_uplane_valid_s * uplane_valid;
	uplane_valid =(ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_uplane_valid_s *)fields;


	uplane_valid->valid_0 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_0_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_0_BMSK);

	uplane_valid->valid_1 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_1_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_1_BMSK);

	uplane_valid->valid_2 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_2_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_2_BMSK);

	uplane_valid->valid_3 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_3_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID_VALID_3_BMSK);


	return;

}
	static void ecpriss_xbar_hal_reg_parse_lut_xbar_fhrx_msg_type_cplane_valid
(enum ecpriss_xbar_hal_reg_name reg, void *fields, u32 val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_cplane_valid_s * cplane_valid;
	cplane_valid =(ecpri_xbar_hwio_def_ecpri_xbar_fhrx_msg_type_cplane_valid_s *)fields;


	cplane_valid->valid_0 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_0_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_0_BMSK);

	cplane_valid->valid_1 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_1_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_1_BMSK);

	cplane_valid->valid_2 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_2_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_2_BMSK);

	cplane_valid->valid_3 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_3_SHFT,
			HWIO_ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID_VALID_3_BMSK);


	return;


}

static void ecpriss_xbar_hal_reg_construct_xbar_fhrx_default_dma_channel
	(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
        ecpri_xbar_hwio_def_ecpri_xbar_fhrx_n_default_dma_channel_s * dma_channel;
        dma_channel =(ecpri_xbar_hwio_def_ecpri_xbar_fhrx_n_default_dma_channel_s *)fields;

        ECPRISS_HAL_SETFIELD_IN_REG(*val,
                        dma_channel->dma_ring_id,
                        HWIO_ECPRI_XBAR_FHRX_n_DEFAULT_DMA_CHANNEL_DMA_RING_ID_SHFT,
                        HWIO_ECPRI_XBAR_FHRX_n_DEFAULT_DMA_CHANNEL_DMA_RING_ID_BMSK);

        ECPRISS_HAL_SETFIELD_IN_REG(*val,
                        dma_channel->gsi_id,
                        HWIO_ECPRI_XBAR_FHRX_n_DEFAULT_DMA_CHANNEL_GSI_ID_SHFT,
                        HWIO_ECPRI_XBAR_FHRX_n_DEFAULT_DMA_CHANNEL_GSI_ID_BMSK);

        return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_fhrx_default_dma_channel
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, u32 val)
{
        ecpri_xbar_hwio_def_ecpri_xbar_fhrx_n_default_dma_channel_s * dma_channel;
        dma_channel =(ecpri_xbar_hwio_def_ecpri_xbar_fhrx_n_default_dma_channel_s *)fields;

        dma_channel->dma_ring_id = ECPRISS_HAL_GETFIELD_FROM_REG(val,
                        HWIO_ECPRI_XBAR_FHRX_n_DEFAULT_DMA_CHANNEL_DMA_RING_ID_SHFT,
                        HWIO_ECPRI_XBAR_FHRX_n_DEFAULT_DMA_CHANNEL_DMA_RING_ID_BMSK);

        dma_channel->gsi_id = ECPRISS_HAL_GETFIELD_FROM_REG(val,
                        HWIO_ECPRI_XBAR_FHRX_n_DEFAULT_DMA_CHANNEL_GSI_ID_SHFT,
                        HWIO_ECPRI_XBAR_FHRX_n_DEFAULT_DMA_CHANNEL_GSI_ID_BMSK);

        return;
}

static void ecpriss_xbar_hal_reg_construct_xbar_c2crx_default_lut
	(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_c2crx_n_default_lut_s *lut =
		(ecpri_xbar_hwio_def_ecpri_xbar_c2crx_n_default_lut_s *)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			lut->up_ul_route,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_UP_UL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_UP_UL_ROUTE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			lut->up_dl_route,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_UP_DL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_UP_DL_ROUTE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			lut->cp_ul_route,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_CP_UL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_CP_UL_ROUTE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			lut->cp_dl_route,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_CP_DL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_CP_DL_ROUTE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			lut->other_route,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_OTHER_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_OTHER_ROUTE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			lut->l3_encap_valid,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_L3_ENCAP_VALID_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_L3_ENCAP_VALID_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			lut->l3_encap_info,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_L3_ENCAP_INFO_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_L3_ENCAP_INFO_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			lut->l2_encap_info,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_L2_ENCAP_INFO_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_L2_ENCAP_INFO_BMSK);

	return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_c2crx_default_lut
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, u32 val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_c2crx_n_default_lut_s *lut =
		(ecpri_xbar_hwio_def_ecpri_xbar_c2crx_n_default_lut_s *)fields;

	lut->up_ul_route = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_UP_UL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_UP_UL_ROUTE_BMSK);

	lut->up_dl_route = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_UP_DL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_UP_DL_ROUTE_BMSK);

	lut->cp_ul_route = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_CP_UL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_CP_UL_ROUTE_BMSK);

	lut->cp_dl_route = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_CP_DL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_CP_DL_ROUTE_BMSK);

	lut->other_route = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_OTHER_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_OTHER_ROUTE_BMSK);

	lut->l3_encap_valid = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_L3_ENCAP_VALID_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_L3_ENCAP_VALID_BMSK);

	lut->l3_encap_info = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_L3_ENCAP_INFO_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_L3_ENCAP_INFO_BMSK);

	lut->l2_encap_info = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_L2_ENCAP_INFO_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_LUT_L2_ENCAP_INFO_BMSK);

	return;
}

static void ecpriss_xbar_hal_reg_construct_xbar_c2crx_default_dma_channel
	(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
        ecpri_xbar_hwio_def_ecpri_xbar_c2crx_n_default_dma_channel_s * dma_channel;
        dma_channel =(ecpri_xbar_hwio_def_ecpri_xbar_c2crx_n_default_dma_channel_s *)fields;

        ECPRISS_HAL_SETFIELD_IN_REG(*val,
                        dma_channel->dma_ring_id,
                        HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_DMA_CHANNEL_DMA_RING_ID_SHFT,
                        HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_DMA_CHANNEL_DMA_RING_ID_BMSK);

        ECPRISS_HAL_SETFIELD_IN_REG(*val,
                        dma_channel->gsi_id,
                        HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_DMA_CHANNEL_GSI_ID_SHFT,
                        HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_DMA_CHANNEL_GSI_ID_BMSK);

        return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_c2crx_default_dma_channel
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, u32 val)
{
        ecpri_xbar_hwio_def_ecpri_xbar_c2crx_n_default_dma_channel_s * dma_channel;
        dma_channel =(ecpri_xbar_hwio_def_ecpri_xbar_c2crx_n_default_dma_channel_s *)fields;

        dma_channel->dma_ring_id = ECPRISS_HAL_GETFIELD_FROM_REG(val,
                        HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_DMA_CHANNEL_DMA_RING_ID_SHFT,
                        HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_DMA_CHANNEL_DMA_RING_ID_BMSK);

        dma_channel->gsi_id = ECPRISS_HAL_GETFIELD_FROM_REG(val,
                        HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_DMA_CHANNEL_GSI_ID_SHFT,
                        HWIO_ECPRI_XBAR_C2CRX_n_DEFAULT_DMA_CHANNEL_GSI_ID_BMSK);

        return;
}

static void ecpriss_xbar_hal_reg_parse_lut_status_2(enum ecpriss_xbar_hal_reg_name reg, void *fields,
		u32 val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_lut_status_s *lut_status;
	lut_status = (ecpri_xbar_hwio_def_ecpri_xbar_lut_status_s *)fields;

	ECPRILOGINFO("ecpriss_xbar_hal_reg_parse_lut_status_2: 0x%x", val);

	lut_status->fhrx_lut_init_done = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_LUT_STATUS_FHRX_LUT_INIT_DONE_SHFT,
			HWIO_ECPRI_XBAR_LUT_STATUS_FHRX_LUT_INIT_DONE_BMSK);
	lut_status->c2crx_lut_init_done = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_LUT_STATUS_C2CRX_LUT_INIT_DONE_SHFT,
			HWIO_ECPRI_XBAR_LUT_STATUS_C2CRX_LUT_INIT_DONE_BMSK);
	lut_status->ocrx_lut_init_done = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_LUT_STATUS_OCRX_LUT_INIT_DONE_SHFT,
			HWIO_ECPRI_XBAR_LUT_STATUS_OCRX_LUT_INIT_DONE_BMSK);

	return;
}


static void ecpriss_xbar_hal_reg_parse_xbar_flush_2(enum ecpriss_xbar_hal_reg_name reg, void *fields,
		u32 val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_flush_u *xbar_flush;
	xbar_flush = fields;

	/*C2C0 RX TX*/
	xbar_flush->def.flush_c2c_0_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_0_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_0_RX_BMSK);
	xbar_flush->def.flush_c2c_0_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_0_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_0_TX_BMSK);

	/*C2C1 RX TX*/
	xbar_flush->def.flush_c2c_1_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_1_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_1_RX_BMSK);
	xbar_flush->def.flush_c2c_1_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_1_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_1_TX_BMSK);

	/*C2C2 RX TX*/
	xbar_flush->def.flush_c2c_2_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_2_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_2_RX_BMSK);
	xbar_flush->def.flush_c2c_2_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_2_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_2_TX_BMSK);


	/*FH0 RX TX*/
	xbar_flush->def.flush_fh_0_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_0_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_0_RX_BMSK);
	xbar_flush->def.flush_fh_0_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_0_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_0_TX_BMSK);



	xbar_flush->def.flush_fh_1_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_1_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_1_RX_BMSK);
	xbar_flush->def.flush_fh_1_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_1_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_1_TX_BMSK);

	/*FH2 RX TX*/
	xbar_flush->def.flush_fh_2_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_2_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_2_RX_BMSK);
	xbar_flush->def.flush_fh_2_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_2_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_2_TX_BMSK);



	/*OC0 RX TX*/
	xbar_flush->def.flush_oc_0_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_0_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_0_RX_BMSK);
	xbar_flush->def.flush_oc_0_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_0_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_0_TX_BMSK);

	/*OC1 RX TX*/
	xbar_flush->def.flush_oc_1_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_1_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_1_RX_BMSK);
	xbar_flush->def.flush_oc_1_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_1_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_1_TX_BMSK);

	/*OC2 RX TX*/
	xbar_flush->def.flush_oc_2_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_2_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_2_RX_BMSK);
	xbar_flush->def.flush_oc_2_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_2_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_2_TX_BMSK);


	/*OC3 RX TX*/
	xbar_flush->def.flush_oc_3_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_3_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_3_RX_BMSK);
	xbar_flush->def.flush_oc_3_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_3_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_3_TX_BMSK);

	return;
}

	static void ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_m_lut_n_2
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_fhrx_m_lut_n_s *xbar_fhrx_m_lut_n
		= (ecpri_xbar_hwio_def_ecpri_xbar_fhrx_m_lut_n_s *)fields;


	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->ul_oc_link_id,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_OC_LINK_ID_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_OC_LINK_ID_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->ul_route_to_oran,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_ROUTE_TO_ORAN_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_ROUTE_TO_ORAN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->ul_route_to_c2c,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_ROUTE_TO_C2C_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_ROUTE_TO_C2C_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->ul_route_to_dma,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_ROUTE_TO_DMA_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_ROUTE_TO_DMA_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->dl_oc_link_id,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_OC_LINK_ID_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_OC_LINK_ID_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->dl_route_to_oran,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_ROUTE_TO_ORAN_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_ROUTE_TO_ORAN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->dl_route_to_c2c,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_ROUTE_TO_C2C_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_ROUTE_TO_C2C_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->dl_route_to_dma,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_ROUTE_TO_DMA_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_ROUTE_TO_DMA_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->other_oc_link_id,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_OC_LINK_ID_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_OC_LINK_ID_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->other_route_to_oran,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_ROUTE_TO_ORAN_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_ROUTE_TO_ORAN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->other_route_to_c2c,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_ROUTE_TO_C2C_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_ROUTE_TO_C2C_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->other_route_to_dma,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_ROUTE_TO_DMA_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_ROUTE_TO_DMA_SHFT);



	return;
}

	static void ecpriss_xbar_hal_reg_parse_lut_xbar_fhrx_m_lut_n_2
(enum ecpriss_xbar_hal_reg_name reg,  void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_fhrx_m_lut_n_s *xbar_fhrx_m_lut_n
		= (ecpri_xbar_hwio_def_ecpri_xbar_fhrx_m_lut_n_s *)fields;


	xbar_fhrx_m_lut_n->ul_oc_link_id = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_OC_LINK_ID_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_OC_LINK_ID_BMSK);

	xbar_fhrx_m_lut_n->ul_route_to_oran = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_ROUTE_TO_ORAN_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_ROUTE_TO_ORAN_BMSK);

	xbar_fhrx_m_lut_n->ul_route_to_c2c = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_ROUTE_TO_C2C_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_ROUTE_TO_C2C_BMSK);

	xbar_fhrx_m_lut_n->ul_route_to_dma = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_ROUTE_TO_DMA_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_UL_ROUTE_TO_DMA_BMSK);

	xbar_fhrx_m_lut_n->dl_oc_link_id = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_OC_LINK_ID_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_OC_LINK_ID_BMSK);

	xbar_fhrx_m_lut_n->dl_route_to_oran = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_ROUTE_TO_ORAN_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_ROUTE_TO_ORAN_BMSK);

	xbar_fhrx_m_lut_n->dl_route_to_c2c = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_ROUTE_TO_C2C_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_ROUTE_TO_C2C_BMSK);

	xbar_fhrx_m_lut_n->dl_route_to_dma = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_ROUTE_TO_DMA_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_DL_ROUTE_TO_DMA_BMSK);

	xbar_fhrx_m_lut_n->other_oc_link_id = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_OC_LINK_ID_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_OC_LINK_ID_BMSK);

	xbar_fhrx_m_lut_n->other_route_to_oran = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_ROUTE_TO_ORAN_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_ROUTE_TO_ORAN_BMSK);

	xbar_fhrx_m_lut_n->other_route_to_c2c = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_ROUTE_TO_C2C_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_ROUTE_TO_C2C_BMSK);

	xbar_fhrx_m_lut_n->other_route_to_dma = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_ROUTE_TO_DMA_SHFT,
			HWIO_ECPRI_XBAR_FHRX_m_LUT_n_OTHER_ROUTE_TO_DMA_SHFT);



	return;
}


	static void ecpriss_xbar_hal_reg_construct_lut_xbar_c2crx_m_lut_n_2
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_c2crx_m_lut_n_s *xbar_c2crx_m_lut_n
		= (ecpri_xbar_hwio_def_ecpri_xbar_c2crx_m_lut_n_s *)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_c2crx_m_lut_n->up_ul_route,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_UP_UL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_UP_UL_ROUTE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_c2crx_m_lut_n->up_dl_route,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_UP_DL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_UP_DL_ROUTE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_c2crx_m_lut_n->cp_ul_route,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_CP_UL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_CP_UL_ROUTE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_c2crx_m_lut_n->cp_dl_route,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_CP_DL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_CP_DL_ROUTE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_c2crx_m_lut_n->other_route,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_OTHER_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_OTHER_ROUTE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_c2crx_m_lut_n->l3_encap_valid,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_L3_ENCAP_VALID_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_L3_ENCAP_VALID_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_c2crx_m_lut_n->l3_encap_info,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_L3_ENCAP_INFO_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_L3_ENCAP_INFO_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_c2crx_m_lut_n->l2_encap_info,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_L2_ENCAP_INFO_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_L2_ENCAP_INFO_BMSK);

	return;
}

	static void ecpriss_xbar_hal_reg_parse_lut_xbar_c2crx_m_lut_n_2
(enum ecpriss_xbar_hal_reg_name reg,  void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_c2crx_m_lut_n_s *xbar_c2crx_m_lut_n
		= (ecpri_xbar_hwio_def_ecpri_xbar_c2crx_m_lut_n_s *)fields;

	xbar_c2crx_m_lut_n->up_ul_route = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_UP_UL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_UP_UL_ROUTE_BMSK);

	xbar_c2crx_m_lut_n->up_dl_route = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_UP_DL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_UP_DL_ROUTE_BMSK);

	xbar_c2crx_m_lut_n->cp_ul_route = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_CP_UL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_CP_UL_ROUTE_BMSK);

	xbar_c2crx_m_lut_n->cp_dl_route = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_CP_DL_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_CP_DL_ROUTE_BMSK);

	xbar_c2crx_m_lut_n->other_route = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_OTHER_ROUTE_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_OTHER_ROUTE_BMSK);

	xbar_c2crx_m_lut_n->l3_encap_valid = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_L3_ENCAP_VALID_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_L3_ENCAP_VALID_BMSK);

	xbar_c2crx_m_lut_n->l3_encap_info = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_L3_ENCAP_INFO_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_L3_ENCAP_INFO_BMSK);

	xbar_c2crx_m_lut_n->l2_encap_info = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_L2_ENCAP_INFO_SHFT,
			HWIO_ECPRI_XBAR_C2CRX_m_LUT_n_L2_ENCAP_INFO_BMSK);

	return;
}
	static void  ecpriss_xbar_hal_reg_construct_lut_xbar_ocrx_m_lut_n_2
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{

	ecpri_xbar_hwio_def_ecpri_xbar_ocrx_m_lut_n_s * xbar_ocrx_m_lut_n
		= (ecpri_xbar_hwio_def_ecpri_xbar_ocrx_m_lut_n_s *)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_ocrx_m_lut_n->l2_encap_info,
			ECPRI_XBAR_XBAR_OCRX_LUT_M_L2_ENCAP_SHFT,
			ECPRI_XBAR_XBAR_OCRX_LUT_M_L2_ENCAP_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_ocrx_m_lut_n->l3_encap_info,
			ECPRI_XBAR_XBAR_OCRX_LUT_M_L3_ENCAP_SHFT,
			ECPRI_XBAR_XBAR_OCRX_LUT_M_L3_ENCAP_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_ocrx_m_lut_n->l3_encap_valid,
			ECPRI_XBAR_XBAR_OCRX_LUT_M_L3_ENCAP_VALID_SHFT,
			ECPRI_XBAR_XBAR_OCRX_LUT_M_L3_ENCAP_VALID_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_ocrx_m_lut_n->valid,
			ECPRI_XBAR_XBAR_OCRX_LUT_M_VALID_SHFT, //17 0x3ff
			ECPRI_XBAR_XBAR_OCRX_LUT_M_VALID_BMSK);

	return;
}


	static void ecpriss_xbar_hal_reg_construct_lut_xbar_nfapi_m_lut_n
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	return;
}


	static void ecpriss_xbar_hal_reg_construct_fhrx_0_non_ecpri_lut
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_fhrx_0_non_ecpri_lut_s *fhrx_0_non_ecpri_lut
		= (ecpri_xbar_hwio_def_ecpri_xbar_xbar_fhrx_0_non_ecpri_lut_s *)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fhrx_0_non_ecpri_lut->ring_id_0,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_0_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_0_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fhrx_0_non_ecpri_lut->ring_id_1,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_1_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_1_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fhrx_0_non_ecpri_lut->ring_id_2,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_2_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_2_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fhrx_0_non_ecpri_lut->ring_id_3,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_3_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_3_BMSK);

	return;
}

	static void ecpriss_xbar_hal_reg_parse_fhrx_0_non_ecpri_lut
(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_fhrx_0_non_ecpri_lut_s *fhrx_0_non_ecpri_lut
		= (ecpri_xbar_hwio_def_ecpri_xbar_xbar_fhrx_0_non_ecpri_lut_s *)fields;

	fhrx_0_non_ecpri_lut->ring_id_0 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_0_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_0_BMSK);

	fhrx_0_non_ecpri_lut->ring_id_1 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_1_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_1_BMSK);

	fhrx_0_non_ecpri_lut->ring_id_2 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_2_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_2_BMSK);

	fhrx_0_non_ecpri_lut->ring_id_3 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_3_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT_RING_ID_3_BMSK);

	return;
}

	static void ecpriss_xbar_hal_reg_construct_fhrx_1_non_ecpri_lut
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_fhrx_1_non_ecpri_lut_s *fhrx_1_non_ecpri_lut
		= (ecpri_xbar_hwio_def_ecpri_xbar_xbar_fhrx_1_non_ecpri_lut_s *)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fhrx_1_non_ecpri_lut->ring_id_0,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_0_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_0_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fhrx_1_non_ecpri_lut->ring_id_1,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_1_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_1_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fhrx_1_non_ecpri_lut->ring_id_2,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_2_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_2_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fhrx_1_non_ecpri_lut->ring_id_3,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_3_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_3_BMSK);

	return;
}

	static void ecpriss_xbar_hal_reg_parse_fhrx_1_non_ecpri_lut
(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_fhrx_1_non_ecpri_lut_s *fhrx_1_non_ecpri_lut
		= (ecpri_xbar_hwio_def_ecpri_xbar_xbar_fhrx_1_non_ecpri_lut_s *)fields;

	fhrx_1_non_ecpri_lut->ring_id_0 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_0_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_0_BMSK);

	fhrx_1_non_ecpri_lut->ring_id_1 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_1_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_1_BMSK);

	fhrx_1_non_ecpri_lut->ring_id_2 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_2_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_2_BMSK);

	fhrx_1_non_ecpri_lut->ring_id_3 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_3_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT_RING_ID_3_BMSK);

	return;
}


	static void ecpriss_xbar_hal_reg_construct_fhrx_2_non_ecpri_lut
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_fhrx_2_non_ecpri_lut_s *fhrx_2_non_ecpri_lut
		= (ecpri_xbar_hwio_def_ecpri_xbar_xbar_fhrx_2_non_ecpri_lut_s *)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fhrx_2_non_ecpri_lut->ring_id_0,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_0_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_0_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fhrx_2_non_ecpri_lut->ring_id_1,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_1_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_1_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fhrx_2_non_ecpri_lut->ring_id_2,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_2_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_2_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fhrx_2_non_ecpri_lut->ring_id_3,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_3_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_3_BMSK);

	return;
}

	static void ecpriss_xbar_hal_reg_parse_fhrx_2_non_ecpri_lut
(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_fhrx_2_non_ecpri_lut_s *fhrx_2_non_ecpri_lut
		= (ecpri_xbar_hwio_def_ecpri_xbar_xbar_fhrx_2_non_ecpri_lut_s *)fields;

	fhrx_2_non_ecpri_lut->ring_id_0 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_0_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_0_BMSK);

	fhrx_2_non_ecpri_lut->ring_id_1 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_1_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_1_BMSK);

	fhrx_2_non_ecpri_lut->ring_id_2 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_2_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_2_BMSK);

	fhrx_2_non_ecpri_lut->ring_id_3 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_3_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT_RING_ID_3_BMSK);

	return;
}

#if 0
	static void ecpriss_xbar_hal_reg_construct_lut_status
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_lut_status_s *lut_status
		= (ecpri_xbar_hwio_def_ecpri_xbar_xbar_lut_status_s *)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			lut_status->fhrx_lut_init_done,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_FHRX_LUT_INIT_DONE_SHFT,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_FHRX_LUT_INIT_DONE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			lut_status->c2crx_dl_lut_init_done,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_C2CRX_DL_LUT_INIT_DONE_SHFT,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_C2CRX_DL_LUT_INIT_DONE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			lut_status->c2crx_ul_lut_init_done,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_C2CRX_UL_LUT_INIT_DONE_SHFT,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_C2CRX_UL_LUT_INIT_DONE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			lut_status->ocrx_lut_init_done,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_OCRX_LUT_INIT_DONE_SHFT,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_OCRX_LUT_INIT_DONE_BMSK);
	return;
}

#endif

	static void ecpriss_xbar_hal_reg_construct_xbar_flush
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_flush_s *xbar_flush
		= (ecpri_xbar_hwio_def_ecpri_xbar_xbar_flush_s *)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_fh_0_tx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_0_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_0_TX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_fh_0_rx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_0_RX_SHFT ,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_0_RX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_fh_1_tx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_1_TX_SHFT ,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_1_TX_BMSK );

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_fh_1_rx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_1_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_1_RX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_fh_2_tx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_2_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_2_TX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_fh_2_rx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_2_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_2_RX_BMSK);


	//C2C
	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_c2c_0_tx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_0_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_0_TX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_c2c_0_rx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_0_RX_SHFT ,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_0_RX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_c2c_1_tx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_1_TX_SHFT ,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_1_TX_BMSK );

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_c2c_1_rx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_1_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_1_RX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_c2c_2_tx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_2_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_2_TX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_c2c_2_rx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_2_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_2_RX_BMSK);

	//OC

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_0_tx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_0_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_0_TX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_0_rx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_0_RX_SHFT ,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_0_RX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_1_tx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_1_TX_SHFT ,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_1_TX_BMSK );

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_1_rx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_1_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_1_RX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_2_tx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_2_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_2_TX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_2_rx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_2_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_2_RX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_3_tx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_3_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_3_TX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_3_rx,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_3_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_3_RX_BMSK);
	return;
}

	static void ecpriss_xbar_hal_reg_construct_xbar_flush_2
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_flush_s *xbar_flush
		= (ecpri_xbar_hwio_def_ecpri_xbar_flush_s *)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_fh_0_tx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_0_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_0_TX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_fh_0_rx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_0_RX_SHFT ,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_0_RX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_fh_1_tx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_1_TX_SHFT ,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_1_TX_BMSK );

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_fh_1_rx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_1_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_1_RX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_fh_2_tx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_2_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_2_TX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_fh_2_rx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_2_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_FH_2_RX_BMSK);


	//C2C
	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_c2c_0_tx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_0_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_0_TX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_c2c_0_rx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_0_RX_SHFT ,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_0_RX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_c2c_1_tx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_1_TX_SHFT ,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_1_TX_BMSK );

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_c2c_1_rx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_1_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_1_RX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_c2c_2_tx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_2_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_2_TX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_c2c_2_rx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_2_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_C2C_2_RX_BMSK);

	//OC

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_0_tx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_0_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_0_TX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_0_rx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_0_RX_SHFT ,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_0_RX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_1_tx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_1_TX_SHFT ,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_1_TX_BMSK );

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_1_rx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_1_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_1_RX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_2_tx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_2_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_2_TX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_2_rx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_2_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_2_RX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_3_tx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_3_TX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_3_TX_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_flush->flush_oc_3_rx,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_3_RX_SHFT,
			HWIO_ECPRI_XBAR_FLUSH_FLUSH_OC_3_RX_BMSK);
	return;
}
static void ecpriss_xbar_hal_reg_parse_lut_status(enum ecpriss_xbar_hal_reg_name reg, void *fields,
		u32 val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_lut_status_s *lut_status;
	lut_status = (ecpri_xbar_hwio_def_ecpri_xbar_xbar_lut_status_s *)fields;

	ECPRILOGINFO("ecpriss_xbar_hal_reg_parse_lut_status: 0x%x", val);

	lut_status->fhrx_lut_init_done = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_FHRX_LUT_INIT_DONE_SHFT,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_FHRX_LUT_INIT_DONE_BMSK);
	lut_status->c2crx_dl_lut_init_done = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_C2CRX_DL_LUT_INIT_DONE_SHFT,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_C2CRX_DL_LUT_INIT_DONE_BMSK);
	lut_status->c2crx_ul_lut_init_done = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_C2CRX_UL_LUT_INIT_DONE_SHFT,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_C2CRX_UL_LUT_INIT_DONE_BMSK);
	lut_status->ocrx_lut_init_done = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_OCRX_LUT_INIT_DONE_SHFT,
			HWIO_ECPRI_XBAR_XBAR_LUT_STATUS_OCRX_LUT_INIT_DONE_BMSK);

	return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_flush(enum ecpriss_xbar_hal_reg_name reg, void *fields,
		u32 val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_flush_u *xbar_flush;
	xbar_flush = fields;

	/*C2C0 RX TX*/
	xbar_flush->def.flush_c2c_0_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_0_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_0_RX_BMSK);
	xbar_flush->def.flush_c2c_0_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_0_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_0_TX_BMSK);

	/*C2C1 RX TX*/
	xbar_flush->def.flush_c2c_1_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_1_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_1_RX_BMSK);
	xbar_flush->def.flush_c2c_1_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_1_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_1_TX_BMSK);

	/*C2C2 RX TX*/
	xbar_flush->def.flush_c2c_2_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_2_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_2_RX_BMSK);
	xbar_flush->def.flush_c2c_2_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_2_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_C2C_2_TX_BMSK);


	/*FH0 RX TX*/
	xbar_flush->def.flush_fh_0_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_0_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_0_RX_BMSK);
	xbar_flush->def.flush_fh_0_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_0_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_0_TX_BMSK);

	/*FH1 RX TX*/
	xbar_flush->def.flush_fh_1_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_1_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_1_RX_BMSK);
	xbar_flush->def.flush_fh_1_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_1_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_1_TX_BMSK);

	/*FH2 RX TX*/
	xbar_flush->def.flush_fh_2_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_2_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_2_RX_BMSK);
	xbar_flush->def.flush_fh_2_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_2_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_FH_2_TX_BMSK);



	/*OC0 RX TX*/
	xbar_flush->def.flush_oc_0_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_0_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_0_RX_BMSK);
	xbar_flush->def.flush_oc_0_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_0_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_0_TX_BMSK);

	/*OC1 RX TX*/
	xbar_flush->def.flush_oc_1_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_1_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_1_RX_BMSK);
	xbar_flush->def.flush_oc_1_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_1_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_1_TX_BMSK);

	/*OC2 RX TX*/
	xbar_flush->def.flush_oc_2_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_2_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_2_RX_BMSK);
	xbar_flush->def.flush_oc_2_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_2_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_2_TX_BMSK);

	/*OC3 RX TX*/
	xbar_flush->def.flush_oc_3_rx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_3_RX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_3_RX_BMSK);
	xbar_flush->def.flush_oc_3_tx = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_3_TX_SHFT,
			HWIO_ECPRI_XBAR_XBAR_FLUSH_FLUSH_OC_3_TX_BMSK);

	return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_cfg(enum ecpriss_xbar_hal_reg_name reg, void *fields,
		u32 val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_cfg_u *xbar_cfg;
	xbar_cfg = fields;


	xbar_cfg->def.c2crx_cnt_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_CFG_C2CRX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_C2CRX_CNT_EN_BMSK);
	xbar_cfg->def.c2ctx_cnt_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_CFG_C2CTX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_C2CTX_CNT_EN_BMSK);

	xbar_cfg->def.fhrx_cnt_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_CFG_FHRX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_FHRX_CNT_EN_BMSK);
	xbar_cfg->def.fhtx_cnt_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_CFG_FHTX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_FHTX_CNT_EN_BMSK);

	xbar_cfg->def.ocrx_cnt_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_CFG_OCRX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_OCRX_CNT_EN_BMSK);
	xbar_cfg->def.octx_cnt_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_CFG_OCTX_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_OCTX_CNT_EN_BMSK);

	xbar_cfg->def.xbar_cnt_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_CFG_XBAR_CNT_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_XBAR_CNT_EN_BMSK);
	xbar_cfg->def.xbar_cnt_clr_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_CFG_XBAR_CNT_CLR_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_XBAR_CNT_CLR_EN_BMSK);

	xbar_cfg->def.tpdm_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_CFG_TPDM_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_TPDM_EN_BMSK);

	xbar_cfg->def.axis_arb_fix_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_CFG_AXIS_ARB_FIX_EN_SHFT,
			HWIO_ECPRI_XBAR_XBAR_CFG_AXIS_ARB_FIX_EN_BMSK);


	return;
}
static void ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_unknown_pcid_cnt
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_dbg_ocrx_unknown_pcid_cnt_s *xbar_dbg_ocrx_unknown_pcid_cnt =
		(ecpri_xbar_hwio_def_ecpri_xbar_xbar_dbg_ocrx_unknown_pcid_cnt_s*) fields;

	xbar_dbg_ocrx_unknown_pcid_cnt->ocrx_fh_0_cnt = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT_OCRX_FH_0_CNT_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT_OCRX_FH_0_CNT_BMSK);

	xbar_dbg_ocrx_unknown_pcid_cnt->ocrx_fh_1_cnt = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT_OCRX_FH_1_CNT_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT_OCRX_FH_1_CNT_BMSK);

	xbar_dbg_ocrx_unknown_pcid_cnt->ocrx_fh_2_cnt = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT_OCRX_FH_2_CNT_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT_OCRX_FH_2_CNT_BMSK);

	return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_unknown_pcid_cnt_2
        (enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
        ecpri_xbar_hwio_def_ecpri_xbar_dbg_ocrx_unknown_pcid_cnt_s *xbar_dbg_ocrx_unknown_pcid_cnt =
                (ecpri_xbar_hwio_def_ecpri_xbar_dbg_ocrx_unknown_pcid_cnt_s*) fields;

        xbar_dbg_ocrx_unknown_pcid_cnt->ocrx_fh_0_cnt = ECPRISS_HAL_GETFIELD_FROM_REG(val,
                        HWIO_ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT_OCRX_FH_0_CNT_SHFT,
                        HWIO_ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT_OCRX_FH_0_CNT_BMSK);

        xbar_dbg_ocrx_unknown_pcid_cnt->ocrx_fh_1_cnt = ECPRISS_HAL_GETFIELD_FROM_REG(val,
                        HWIO_ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT_OCRX_FH_1_CNT_SHFT,
                        HWIO_ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT_OCRX_FH_1_CNT_BMSK);

        xbar_dbg_ocrx_unknown_pcid_cnt->ocrx_fh_2_cnt = ECPRISS_HAL_GETFIELD_FROM_REG(val,
                        HWIO_ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT_OCRX_FH_2_CNT_SHFT,
                        HWIO_ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT_OCRX_FH_2_CNT_BMSK);

        return;
}



static void ecpriss_xbar_hal_reg_parse_xbar_dbg_fhrx_unknown_pcid_cnt
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_dbg_fhrx_unknown_pcid_cnt_s *xbar_dbg_fhrx_unknown_pcid_cnt =
		(ecpri_xbar_hwio_def_ecpri_xbar_xbar_dbg_fhrx_unknown_pcid_cnt_s*) fields;

	xbar_dbg_fhrx_unknown_pcid_cnt->fhrx_0_cnt = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT_FHRX_0_CNT_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT_FHRX_0_CNT_BMSK);

	xbar_dbg_fhrx_unknown_pcid_cnt->fhrx_1_cnt = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT_FHRX_1_CNT_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT_FHRX_1_CNT_BMSK);

	xbar_dbg_fhrx_unknown_pcid_cnt->fhrx_2_cnt = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT_FHRX_2_CNT_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT_FHRX_2_CNT_BMSK);
	return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_dbg_fhrx_unknown_pcid_cnt_2
        (enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
        ecpri_xbar_hwio_def_ecpri_xbar_dbg_fhrx_unknown_pcid_cnt_s *xbar_dbg_fhrx_unknown_pcid_cnt =
                (ecpri_xbar_hwio_def_ecpri_xbar_dbg_fhrx_unknown_pcid_cnt_s*) fields;

        xbar_dbg_fhrx_unknown_pcid_cnt->fhrx_0_cnt = ECPRISS_HAL_GETFIELD_FROM_REG(val,
                        HWIO_ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT_FHRX_0_CNT_SHFT,
                        HWIO_ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT_FHRX_0_CNT_BMSK);

        xbar_dbg_fhrx_unknown_pcid_cnt->fhrx_1_cnt = ECPRISS_HAL_GETFIELD_FROM_REG(val,
                        HWIO_ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT_FHRX_1_CNT_SHFT,
                        HWIO_ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT_FHRX_1_CNT_BMSK);

        xbar_dbg_fhrx_unknown_pcid_cnt->fhrx_2_cnt = ECPRISS_HAL_GETFIELD_FROM_REG(val,
                        HWIO_ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT_FHRX_2_CNT_SHFT,
                        HWIO_ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT_FHRX_2_CNT_BMSK);
        return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_fh_buff_watermark
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_dbg_ocrx_fh_buff_watermark_s *xbar_dbg_ocrx_fh_buff_watermark =
		(ecpri_xbar_hwio_def_ecpri_xbar_xbar_dbg_ocrx_fh_buff_watermark_s*) fields;

	xbar_dbg_ocrx_fh_buff_watermark->fh0 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_FH_BUFF_WATERMARK_FH0_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_FH_BUFF_WATERMARK_FH0_BMSK);

	xbar_dbg_ocrx_fh_buff_watermark->fh1 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_FH_BUFF_WATERMARK_FH1_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_FH_BUFF_WATERMARK_FH1_BMSK);

	xbar_dbg_ocrx_fh_buff_watermark->fh2 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_FH_BUFF_WATERMARK_FH2_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_FH_BUFF_WATERMARK_FH2_BMSK);
	return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_fh_buff_watermark_2
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_dbg_ocrx_fh_buff_watermark_s *xbar_dbg_ocrx_fh_buff_watermark =
		(ecpri_xbar_hwio_def_ecpri_xbar_dbg_ocrx_fh_buff_watermark_s*) fields;

	xbar_dbg_ocrx_fh_buff_watermark->fh0 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_DBG_OCRX_FH_BUFF_WATERMARK_FH0_SHFT,
			HWIO_ECPRI_XBAR_DBG_OCRX_FH_BUFF_WATERMARK_FH0_BMSK);

	xbar_dbg_ocrx_fh_buff_watermark->fh1 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_DBG_OCRX_FH_BUFF_WATERMARK_FH1_SHFT,
			HWIO_ECPRI_XBAR_DBG_OCRX_FH_BUFF_WATERMARK_FH1_BMSK);

	xbar_dbg_ocrx_fh_buff_watermark->fh2 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_DBG_OCRX_FH_BUFF_WATERMARK_FH2_SHFT,
			HWIO_ECPRI_XBAR_DBG_OCRX_FH_BUFF_WATERMARK_FH2_BMSK);
	return;
}


static void ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_0_1_buff_watermark
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_dbg_ocrx_0_1_buff_watermark_s *xbar_dbg_ocrx_0_1_buff_watermark =
		(ecpri_xbar_hwio_def_ecpri_xbar_xbar_dbg_ocrx_0_1_buff_watermark_s*) fields;

	xbar_dbg_ocrx_0_1_buff_watermark->cc0 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_0_1_BUFF_WATERMARK_CC0_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_0_1_BUFF_WATERMARK_CC0_BMSK);

	xbar_dbg_ocrx_0_1_buff_watermark->cc1 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_0_1_BUFF_WATERMARK_CC1_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_0_1_BUFF_WATERMARK_CC1_BMSK);
	return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_0_1_buff_watermark_2
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_dbg_ocrx_0_1_buff_watermark_s *xbar_dbg_ocrx_0_1_buff_watermark =
		(ecpri_xbar_hwio_def_ecpri_xbar_dbg_ocrx_0_1_buff_watermark_s*) fields;

	xbar_dbg_ocrx_0_1_buff_watermark->cc0 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_DBG_OCRX_0_1_BUFF_WATERMARK_CC0_SHFT,
			HWIO_ECPRI_XBAR_DBG_OCRX_0_1_BUFF_WATERMARK_CC0_BMSK);

	xbar_dbg_ocrx_0_1_buff_watermark->cc1 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_DBG_OCRX_0_1_BUFF_WATERMARK_CC1_SHFT,
			HWIO_ECPRI_XBAR_DBG_OCRX_0_1_BUFF_WATERMARK_CC1_BMSK);
	return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_2_3_buff_watermark
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_dbg_ocrx_2_3_buff_watermark_s *xbar_dbg_ocrx_2_3_buff_watermark =
		(ecpri_xbar_hwio_def_ecpri_xbar_xbar_dbg_ocrx_2_3_buff_watermark_s*) fields;

	xbar_dbg_ocrx_2_3_buff_watermark->cc2 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_2_3_BUFF_WATERMARK_CC2_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_2_3_BUFF_WATERMARK_CC2_BMSK);

	xbar_dbg_ocrx_2_3_buff_watermark->cc3 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_2_3_BUFF_WATERMARK_CC3_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCRX_2_3_BUFF_WATERMARK_CC3_BMSK);
	return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_2_3_buff_watermark_2
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_dbg_ocrx_2_3_buff_watermark_s *xbar_dbg_ocrx_2_3_buff_watermark =
		(ecpri_xbar_hwio_def_ecpri_xbar_dbg_ocrx_2_3_buff_watermark_s*) fields;

	xbar_dbg_ocrx_2_3_buff_watermark->cc2 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_DBG_OCRX_2_3_BUFF_WATERMARK_CC2_SHFT,
			HWIO_ECPRI_XBAR_DBG_OCRX_2_3_BUFF_WATERMARK_CC2_BMSK);

	xbar_dbg_ocrx_2_3_buff_watermark->cc3 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_DBG_OCRX_2_3_BUFF_WATERMARK_CC3_SHFT,
			HWIO_ECPRI_XBAR_DBG_OCRX_2_3_BUFF_WATERMARK_CC3_BMSK);
	return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_dbg_octx_oc_0_1_buff_watermark
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_dbg_octx_oc_0_1_buff_watermark_s *octx_oc_0_1_buff_watermark =
		(ecpri_xbar_hwio_def_ecpri_xbar_xbar_dbg_octx_oc_0_1_buff_watermark_s*) fields;

	octx_oc_0_1_buff_watermark->cc0 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCTX_OC_0_1_BUFF_WATERMARK_CC0_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCTX_OC_0_1_BUFF_WATERMARK_CC0_BMSK);

	octx_oc_0_1_buff_watermark->cc1 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCTX_OC_0_1_BUFF_WATERMARK_CC1_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCTX_OC_0_1_BUFF_WATERMARK_CC1_BMSK);
	return;
}
static void ecpriss_xbar_hal_reg_parse_xbar_dbg_octx_oc_2_3_buff_watermark
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_dbg_octx_oc_2_3_buff_watermark_s *octx_oc_2_3_buff_watermark =
		(ecpri_xbar_hwio_def_ecpri_xbar_xbar_dbg_octx_oc_2_3_buff_watermark_s*) fields;

	octx_oc_2_3_buff_watermark->cc2 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCTX_OC_2_3_BUFF_WATERMARK_CC2_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCTX_OC_2_3_BUFF_WATERMARK_CC2_BMSK);

	octx_oc_2_3_buff_watermark->cc3 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCTX_OC_2_3_BUFF_WATERMARK_CC3_SHFT,
			HWIO_ECPRI_XBAR_XBAR_DBG_OCTX_OC_2_3_BUFF_WATERMARK_CC3_BMSK);
	return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_dbg_octx_oc_0_1_buff_watermark_2
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_dbg_octx_oc_0_1_buff_watermark_s *octx_oc_0_1_buff_watermark =
		(ecpri_xbar_hwio_def_ecpri_xbar_dbg_octx_oc_0_1_buff_watermark_s*) fields;

	octx_oc_0_1_buff_watermark->cc0 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_DBG_OCTX_OC_0_1_BUFF_WATERMARK_CC0_SHFT,
			HWIO_ECPRI_XBAR_DBG_OCTX_OC_0_1_BUFF_WATERMARK_CC0_BMSK);

	octx_oc_0_1_buff_watermark->cc1 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_DBG_OCTX_OC_0_1_BUFF_WATERMARK_CC1_SHFT,
			HWIO_ECPRI_XBAR_DBG_OCTX_OC_0_1_BUFF_WATERMARK_CC1_BMSK);
	return;
}

static void ecpriss_xbar_hal_reg_parse_xbar_dbg_octx_oc_2_3_buff_watermark_2
	(enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_dbg_octx_oc_2_3_buff_watermark_s *octx_oc_2_3_buff_watermark =
		(ecpri_xbar_hwio_def_ecpri_xbar_dbg_octx_oc_2_3_buff_watermark_s*) fields;

	octx_oc_2_3_buff_watermark->cc2 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_DBG_OCTX_OC_2_3_BUFF_WATERMARK_CC2_SHFT,
			HWIO_ECPRI_XBAR_DBG_OCTX_OC_2_3_BUFF_WATERMARK_CC2_BMSK);

	octx_oc_2_3_buff_watermark->cc3 = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_DBG_OCTX_OC_2_3_BUFF_WATERMARK_CC3_SHFT,
			HWIO_ECPRI_XBAR_DBG_OCTX_OC_2_3_BUFF_WATERMARK_CC3_BMSK);
	return;
}

	static void ecpriss_xbar_hal_reg_construct_fh_cfg
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{

	ecpri_xbar_hwio_def_ecpri_xbar_fh_cfg_s *fh_cfg =
		(ecpri_xbar_hwio_def_ecpri_xbar_fh_cfg_s *) fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fh_cfg->dma_prio,
			HWIO_ECPRI_XBAR_FH_CFG_DMA_PRIO_SHFT,
			HWIO_ECPRI_XBAR_FH_CFG_DMA_PRIO_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fh_cfg->c2c_prio,
			HWIO_ECPRI_XBAR_FH_CFG_C2C_PRIO_SHFT,
			HWIO_ECPRI_XBAR_FH_CFG_C2C_PRIO_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fh_cfg->oran_prio,
			HWIO_ECPRI_XBAR_FH_CFG_ORAN_PRIO_SHFT,
			HWIO_ECPRI_XBAR_FH_CFG_ORAN_PRIO_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			fh_cfg->uc_prio,
			HWIO_ECPRI_XBAR_FH_CFG_UC_PRIO_SHFT,
			HWIO_ECPRI_XBAR_FH_CFG_UC_PRIO_BMSK);


	return;
}

static void ecpriss_xbar_hal_reg_parse_fh_cfg
	    (enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_fh_cfg_s *fh_cfg =
		(ecpri_xbar_hwio_def_ecpri_xbar_fh_cfg_s *) fields;

	fh_cfg->dma_prio = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FH_CFG_DMA_PRIO_SHFT,
			HWIO_ECPRI_XBAR_FH_CFG_DMA_PRIO_BMSK);

	fh_cfg->c2c_prio= ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FH_CFG_C2C_PRIO_SHFT,
			HWIO_ECPRI_XBAR_FH_CFG_C2C_PRIO_BMSK);

	fh_cfg->oran_prio= ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FH_CFG_ORAN_PRIO_SHFT,
			HWIO_ECPRI_XBAR_FH_CFG_ORAN_PRIO_BMSK);

	fh_cfg->uc_prio= ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_FH_CFG_UC_PRIO_SHFT,
			HWIO_ECPRI_XBAR_FH_CFG_UC_PRIO_BMSK);

	return;
}


	static void ecpriss_xbar_hal_reg_construct_oc_cfg
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{

	ecpri_xbar_hwio_def_ecpri_xbar_oc_cfg_s *oc_cfg =
		(ecpri_xbar_hwio_def_ecpri_xbar_oc_cfg_s *) fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			oc_cfg->octx_len_chk_enable,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_LEN_CHK_ENABLE_SHFT,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_LEN_CHK_ENABLE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			oc_cfg->octx_len_chk_drop,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_LEN_CHK_DROP_SHFT,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_LEN_CHK_DROP_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			oc_cfg->octx_dynamic_prio_en,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_DYNAMIC_PRIO_EN_SHFT,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_DYNAMIC_PRIO_EN_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			oc_cfg->octx_overflow_mode,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_OVERFLOW_MODE_SHFT,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_OVERFLOW_MODE_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			oc_cfg->ocrx_async_threshold,
			HWIO_ECPRI_XBAR_OC_CFG_OCRX_ASYNC_THRESHOLD_SHFT,
			HWIO_ECPRI_XBAR_OC_CFG_OCRX_ASYNC_THRESHOLD_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			oc_cfg->ocrx_eop_timeout,
			HWIO_ECPRI_XBAR_OC_CFG_OCRX_EOP_TIMEOUT_SHFT,
			HWIO_ECPRI_XBAR_OC_CFG_OCRX_EOP_TIMEOUT_BMSK);


	return;
}

static void ecpriss_xbar_hal_reg_parse_oc_cfg
	    (enum ecpriss_xbar_hal_reg_name reg, void *fields, uint32_t val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_oc_cfg_s *oc_cfg =
		(ecpri_xbar_hwio_def_ecpri_xbar_oc_cfg_s *) fields;

	oc_cfg->octx_len_chk_enable = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_LEN_CHK_ENABLE_SHFT,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_LEN_CHK_ENABLE_BMSK);

	oc_cfg->octx_len_chk_drop = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_LEN_CHK_DROP_SHFT,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_LEN_CHK_DROP_BMSK);

	oc_cfg->octx_dynamic_prio_en = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_DYNAMIC_PRIO_EN_SHFT,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_DYNAMIC_PRIO_EN_BMSK);

	oc_cfg->octx_overflow_mode = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_OVERFLOW_MODE_SHFT,
			HWIO_ECPRI_XBAR_OC_CFG_OCTX_OVERFLOW_MODE_BMSK);

	oc_cfg->ocrx_async_threshold = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_OC_CFG_OCRX_ASYNC_THRESHOLD_SHFT,
			HWIO_ECPRI_XBAR_OC_CFG_OCRX_ASYNC_THRESHOLD_BMSK);

	oc_cfg->ocrx_eop_timeout = ECPRISS_HAL_GETFIELD_FROM_REG(val,
			HWIO_ECPRI_XBAR_OC_CFG_OCRX_EOP_TIMEOUT_SHFT,
			HWIO_ECPRI_XBAR_OC_CFG_OCRX_EOP_TIMEOUT_BMSK);
	return;
}


static void ecpriss_xbar_hal_reg_parse_dummy(enum ecpriss_xbar_hal_reg_name reg, void *fields,
		u32 val)
{
	return;
}

static void ecpriss_xbar_hal_reg_parse_dummy_v2(enum ecpriss_xbar_hal_reg_name reg, void *fields,
		u32 val)
{
	return;
}


	static void ecpriss_xbar_hal_reg_construct_dummy
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	return;
}
	static void ecpriss_xbar_hal_reg_construct_dummy_v2
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	return;
}


	static void ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_msg_type_lut_0
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	return;
}


	static void ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_msg_type_lut_1
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	return;
}

#if 0
	static void ecpriss_xbar_hal_reg_construct_c2crx_msg_type_lut_0
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_xbar_c2crx_msg_type_lut_0_s *c2crx_msg_type_lut
		= (ecpri_xbar_hwio_def_ecpri_xbar_xbar_c2crx_msg_type_lut_0_s *)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			c2crx_msg_type_lut->msg_type_0,
			HWIO_ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_0_MSG_TYPE_0_SHFT,
			HWIO_ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_0_MSG_TYPE_0_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			c2crx_msg_type_lut->valid_0,
			HWIO_ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_0_VALID_0_SHFT,
			HWIO_ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_0_VALID_0_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			c2crx_msg_type_lut->msg_type_1,
			HWIO_ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_0_MSG_TYPE_1_SHFT,
			HWIO_ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_0_MSG_TYPE_1_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			c2crx_msg_type_lut->valid_1,
			HWIO_ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_0_VALID_1_SHFT,
			HWIO_ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_0_VALID_1_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			c2crx_msg_type_lut->msg_type_2,
			HWIO_ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_0_MSG_TYPE_2_SHFT,
			HWIO_ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_0_MSG_TYPE_2_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			c2crx_msg_type_lut->valid_2,
			HWIO_ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_0_VALID_2_SHFT,
			HWIO_ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_0_VALID_2_BMSK);
	return;
}
#endif
	static void  ecpriss_xbar_hal_reg_parse_lut_xbar_fw_irq_status
(enum ecpriss_xbar_hal_reg_name reg, void *fields,u32 val)
{
	return;
}

	static void ecpriss_xbar_hal_reg_construct_lut_xbar_fw_irq_clr
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	return;
}

	static void ecpriss_xbar_hal_reg_construct_lut_xbar_fw_irq_mask
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	return;
}

	static void ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_m_lut_n
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	ecpri_xbar_hwio_def_ecpri_xbar_lut_xbar_fhrx_m_lut_n_s *xbar_fhrx_m_lut_n
		= (ecpri_xbar_hwio_def_ecpri_xbar_lut_xbar_fhrx_m_lut_n_s *)fields;


	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->dma_ring_id,
			ECPRI_XBAR_XBAR_FH_RX_LUT_DMA_RING_ID_SHFT,
			ECPRI_XBAR_XBAR_FH_RX_LUT_DMA_RING_ID_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->oc_link_id,
			ECPRI_XBAR_XBAR_FH_RX_LUT_LINK_ID_SHFT,
			ECPRI_XBAR_XBAR_FH_RX_LUT_LINK_ID_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->route_to_oran,
			ECPRI_XBAR_XBAR_FH_RX_LUT_ROUTE_ORAN_SHFT,
			ECPRI_XBAR_XBAR_FH_RX_LUT_ROUTE_ORAN_BMSK);


	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->route_to_c2c,
			ECPRI_XBAR_XBAR_FH_RX_LUT_ROUTE_C2C_SHFT,
			ECPRI_XBAR_XBAR_FH_RX_LUT_ROUTE_C2C_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->route_to_dma,
			ECPRI_XBAR_XBAR_FH_RX_LUT_ROUTE_DMA_SHFT,
			ECPRI_XBAR_XBAR_FH_RX_LUT_ROUTE_DMA_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_fhrx_m_lut_n->valid,
			ECPRI_XBAR_XBAR_FH_RX_LUT_IS_VALID_SHFT,
			ECPRI_XBAR_XBAR_FH_RX_LUT_IS_VALID_BMSK);

	return;
}

	static void ecpriss_xbar_hal_reg_construct_lut_xbar_c2crx_m_dl_lut_n
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	return;
}

	static void  ecpriss_xbar_hal_reg_construct_lut_xbar_c2crx_m_ul_lut_n
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{
	return;
}

	static void  ecpriss_xbar_hal_reg_construct_lut_xbar_ocrx_m_lut_n
(enum ecpriss_xbar_hal_reg_name reg, const void *fields, uint32_t* val)
{

	ecpri_xbar_hwio_def_ecpri_xbar_lut_xbar_ocrx_m_lut_n_s * xbar_ocrx_m_lut_n
		= (ecpri_xbar_hwio_def_ecpri_xbar_lut_xbar_ocrx_m_lut_n_s *)fields;

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_ocrx_m_lut_n->l2_encap_info,
			ECPRI_XBAR_XBAR_OCRX_LUT_M_L2_ENCAP_SHFT,
			ECPRI_XBAR_XBAR_OCRX_LUT_M_L2_ENCAP_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_ocrx_m_lut_n->l3_encap_info,
			ECPRI_XBAR_XBAR_OCRX_LUT_M_L3_ENCAP_SHFT,
			ECPRI_XBAR_XBAR_OCRX_LUT_M_L3_ENCAP_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_ocrx_m_lut_n->l3_encap_valid,
			ECPRI_XBAR_XBAR_OCRX_LUT_M_L3_ENCAP_VALID_SHFT,
			ECPRI_XBAR_XBAR_OCRX_LUT_M_L3_ENCAP_VALID_BMSK);

	ECPRISS_HAL_SETFIELD_IN_REG(*val,
			xbar_ocrx_m_lut_n->valid,
			ECPRI_XBAR_XBAR_OCRX_LUT_M_VALID_SHFT, //17 0x3ff
			ECPRI_XBAR_XBAR_OCRX_LUT_M_VALID_BMSK);

	return;
}



static struct ecpriss_xbar_hal_reg_obj ecpriss_xbar_hal_reg_objs[ECPRISS_XBAR_HW_MAX][ECPRI_XBAR_MAX] = {
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_CFG] = {
		ecpriss_xbar_hal_reg_construct_xbar_cfg,
		ecpriss_xbar_hal_reg_parse_xbar_cfg,
		0x09220000, 0x0, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT] = {
		ecpriss_xbar_hal_reg_construct_fhrx_0_non_ecpri_lut,
		ecpriss_xbar_hal_reg_parse_fhrx_0_non_ecpri_lut,
		0x09220000, 0x00000800, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT] = {
		ecpriss_xbar_hal_reg_construct_fhrx_1_non_ecpri_lut,
		ecpriss_xbar_hal_reg_parse_fhrx_1_non_ecpri_lut,
		0x09220000, 0x00000804, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT] = {
		ecpriss_xbar_hal_reg_construct_fhrx_2_non_ecpri_lut,
		ecpriss_xbar_hal_reg_parse_fhrx_2_non_ecpri_lut,
		0x09220000, 0x00000808, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_LUT_STATUS] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_lut_status,
		0x09220000, 0x00000030, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_FLUSH] = {
		ecpriss_xbar_hal_reg_construct_xbar_flush,
		ecpriss_xbar_hal_reg_parse_xbar_flush,
		0x09220000, 0x0000001C, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_FHRX_MSG_TYPE_LUT_0] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_msg_type_lut_0,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x00000820, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_FHRX_MSG_TYPE_LUT_1] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_msg_type_lut_1,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x00000824, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_FW_IRQ_STATUS] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_lut_xbar_fw_irq_status,
		0x09220000, 0x00000040, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_FW_IRQ_MASK] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_fw_irq_mask,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x00000044, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_FW_IRQ_CLR] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_fw_irq_clr,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x00000048, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_LUT_XBAR_FHRX_m_LUT_n] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_m_lut_n,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09300000, 0x0, 0x4, 0, 0, 0, 0x40000},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_LUT_XBAR_C2CRX_m_DL_LUT_n] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_c2crx_m_dl_lut_n,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09300000, 0xC0000, 0x4, 0, 0, 0, 0x40000},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_LUT_XBAR_C2CRX_m_UL_LUT_n] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_c2crx_m_ul_lut_n,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09300000, 0x180000, 0x4, 0, 0, 0, 0x40000},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_LUT_XBAR_OCRX_m_LUT_n] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_ocrx_m_lut_n,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09300000, 0x240000, 0x4, 0, 0, 0, 0x40000},

	//Stats
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_FHRX_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2000, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_FHRX_DMA_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2060, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_FHRX_UC_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2090, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_FHRX_UC_ERR_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2098, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_FHRX_ERR_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x20A0, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_FHTX_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 	0x2100, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_FHTX_C2C_PKT_OVF_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2190, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_FHTX_DMA_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2194, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_FHTX_UC_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2198, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_C2CRX_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2200, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_C2CRX_DMA_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2230, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_C2CRX_ERR_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2234, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_C2CTX_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2260, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_C2CTX_FH_PKT_OVF_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2290, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_C2CTX_DMA_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2300, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_OCTX_C2C_PKT_DROP_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2304, 0, 0, 0, 0,0},
	[ECPRISS_XBAR_HW_v1_0][	ECPRI_XBAR_XBAR_DBG_OCTX_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2310, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_OCRX_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2320, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_OCRX_FH_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 	0x2330, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_OCRX_C2C_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2360, 0x4, 0, 0, 0, 0x0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_unknown_pcid_cnt,
		0x09220000, 0x2370, 0x0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_xbar_dbg_fhrx_unknown_pcid_cnt,
		0x09220000, 	0x23A0, 0x0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_C2CRX_UNKNOWN_PCID_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x23D0, 0x0, 0, 0, 0, 0},

	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_INFO_1_n] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2374, 0x04, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_INFO_2_n] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x2384, 0x04, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_INFO_1_n] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x23A4, 0x04, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_INFO_2_n] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_dummy,
		0x09220000, 0x23B4, 0x04, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_OCRX_FH_BUFF_WATERMARK] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_fh_buff_watermark,
		0x09220000, 0x2838, 0x0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_OCRX_0_1_BUFF_WATERMARK] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_0_1_buff_watermark,
		0x09220000, 0x2820, 0x0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_OCRX_2_3_BUFF_WATERMARK] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_2_3_buff_watermark,
		0x09220000, 0x2824, 0x0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_OCTX_OC_0_1_BUFF_WATERMARK] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_xbar_dbg_octx_oc_0_1_buff_watermark,
		0x09220000, 0x2850, 0x0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v1_0][ECPRI_XBAR_XBAR_DBG_OCTX_OC_2_3_BUFF_WATERMARK] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_xbar_dbg_octx_oc_2_3_buff_watermark,
		0x09220000, 0x2854, 0x0, 0, 0, 0, 0},


	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_CFG] = {
		ecpriss_xbar_hal_reg_construct_xbar_cfg_2,
		ecpriss_xbar_hal_reg_parse_xbar_cfg_2,
		0x09420000, 0x0, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_LUT_STATUS] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_lut_status_2,
		0x09420000, 0x00000030, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_FLUSH] = {
		ecpriss_xbar_hal_reg_construct_xbar_flush_2,
		ecpriss_xbar_hal_reg_parse_xbar_flush_2,
		0x09420000, 0x0000001C, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_msg_type_uplane_cfg,
		ecpriss_xbar_hal_reg_parse_lut_xbar_fhrx_msg_type_uplane_cfg,
		0x09420000, 0x00000800, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_msg_type_uplane_valid,
		ecpriss_xbar_hal_reg_parse_lut_xbar_fhrx_msg_type_uplane_valid,
		0x09420000, 0x00000804, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_msg_type_cplane_cfg,
		ecpriss_xbar_hal_reg_parse_lut_xbar_fhrx_msg_type_cplane_cfg,
		0x09420000, 0x00000808, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_msg_type_cplane_valid,
		ecpriss_xbar_hal_reg_parse_lut_xbar_fhrx_msg_type_cplane_valid,
		0x09420000, 0x0000080C, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_FHRX_n_DEFAULT_DMA_CHANNEL] = {
		ecpriss_xbar_hal_reg_construct_xbar_fhrx_default_dma_channel,
		ecpriss_xbar_hal_reg_parse_xbar_fhrx_default_dma_channel,
		0x09420000, 0x00000820, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_FHRX_MSG_TYPE_LUT_0] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x00000830, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_FHRX_MSG_TYPE_LUT_1] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x00000834, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_C2CRX_n_DEFAULT_LUT] = {
		ecpriss_xbar_hal_reg_construct_xbar_c2crx_default_lut,
		ecpriss_xbar_hal_reg_parse_xbar_c2crx_default_lut,
		0x09420000, 0x00000870, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_C2CRX_n_DEFAULT_DMA_CHANNEL] = {
		ecpriss_xbar_hal_reg_construct_xbar_c2crx_default_dma_channel,
		ecpriss_xbar_hal_reg_parse_xbar_c2crx_default_dma_channel,
		0x09420000, 0x00000880, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_SW_IRQ_STATUS] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x00000050, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_SW_IRQ_MASK] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x00000054, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_SW_IRQ_CLR] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x00000058, 0, 0, 0, 0, 0},



	/* LUT */

	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_LUT_XBAR_FHRX_m_LUT_n_V2] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_fhrx_m_lut_n_2,
		ecpriss_xbar_hal_reg_parse_lut_xbar_fhrx_m_lut_n_2,
		0x09500000, 0x0, 0x4, 0, 0, 0, 0x40000},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_LUT_XBAR_C2CRX_m_LUT_n_V2] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_c2crx_m_lut_n_2,
		ecpriss_xbar_hal_reg_parse_lut_xbar_c2crx_m_lut_n_2,
		0x09500000, 0xC0000, 0x4, 0, 0, 0, 0x40000},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_LUT_XBAR_OCRX_m_LUT_n_V2] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_ocrx_m_lut_n_2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09500000, 0x180000, 0x4, 0, 0, 0, 0x40000},


	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHRX_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x4000, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHRX_DMA_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x4030, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHRX_UC_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x4034, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHRX_UC_ERR_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x4038, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHRX_ERR_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x403C, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHRX_OC_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x4088, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHRX_C2C_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x407C, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHTX_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000,     0x4040, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHTX_C2C_PKT_OVF_DROP_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x4070, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHTX_DMA_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x4074, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHTX_UC_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x4078, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHTX_C2C_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x407C, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHTX_OC_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x4088, 0x4, 0, 0, 0, 0},

	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_C2CRX_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x5000, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_C2CRX_DMA_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x500C, 0x0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_C2CRX_ERR_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x5010, 0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_C2CTX_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x5014, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_C2CTX_FH_PKT_OVF_DROP_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x5020, 0, 0, 0, 0,0},
	[ECPRISS_XBAR_HW_v2_0][ ECPRI_XBAR_DBG_C2CTX_DMA_PKT_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x5024, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_C2CRX_FH_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x5028, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_C2CRX_OC_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x5034, 0x4, 0, 0, 0, 0},

	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCTX_C2C_PKT_DROP_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x6000, 0x0, 0, 0, 0, 0x0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCTX_FH_PKT_DROP_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x6004, 0x0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCTX_C2C_LEN_ERR_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x6008, 0x0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCTX_FH_LEN_ERR_CNT] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x600C, 0x0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCTX_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x6010, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCRX_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x6020, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCRX_FH_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x6030, 0x4, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCRX_C2C_PKT_CNT_n] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x6060, 0x4, 0, 0, 0, 0},

	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_LUT_XBAR_NFAPI_n] = {
		ecpriss_xbar_hal_reg_construct_lut_xbar_nfapi_m_lut_n,
		ecpriss_xbar_hal_reg_parse_dummy_v2,
		0x09420000, 0x7000, 0x4, 0, 0, 0, 0},

	/*Unknown pcid counters and watermark reg*/

	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT] = {
                ecpriss_xbar_hal_reg_construct_dummy_v2,
                ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_unknown_pcid_cnt_2,
                0x09420000, 0x2000, 0, 0, 0, 0, 0},

	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_INFO_1_n] = {
                ecpriss_xbar_hal_reg_construct_dummy_v2,
                ecpriss_xbar_hal_reg_parse_dummy_v2,
                0x09420000, 0x2010, 0x4, 0, 0, 0, 0},

	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_INFO_2_n] = {
                ecpriss_xbar_hal_reg_construct_dummy_v2,
                ecpriss_xbar_hal_reg_parse_dummy_v2,
                0x09420000, 0x2020, 0x4, 0, 0, 0, 0},

	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT] = {
                ecpriss_xbar_hal_reg_construct_dummy_v2,
                ecpriss_xbar_hal_reg_parse_xbar_dbg_fhrx_unknown_pcid_cnt_2,
                0x09420000, 0x2030, 0, 0, 0, 0, 0},

        [ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_INFO_1_n] = {
                ecpriss_xbar_hal_reg_construct_dummy_v2,
                ecpriss_xbar_hal_reg_parse_dummy_v2,
                0x09420000, 0x2040, 0x4, 0, 0, 0, 0},

        [ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_INFO_2_n] = {
                ecpriss_xbar_hal_reg_construct_dummy_v2,
                ecpriss_xbar_hal_reg_parse_dummy_v2,
                0x09420000, 0x2050, 0x4, 0, 0, 0, 0},

	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCRX_0_1_BUFF_WATERMARK] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_0_1_buff_watermark_2,
		0x09420000, 0x2420, 0x0, 0, 0, 0, 0},


	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCRX_2_3_BUFF_WATERMARK] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_2_3_buff_watermark_2,
		0x09420000, 0x2424, 0x0, 0, 0, 0, 0},

	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCRX_FH_BUFF_WATERMARK] = {
		ecpriss_xbar_hal_reg_construct_dummy_v2,
		ecpriss_xbar_hal_reg_parse_xbar_dbg_ocrx_fh_buff_watermark_2,
		0x09420000, 0x2438, 0x0, 0, 0, 0, 0},

	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCTX_OC_0_1_BUFF_WATERMARK] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_xbar_dbg_octx_oc_0_1_buff_watermark_2,
		0x09420000, 0x2450, 0x0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_DBG_OCTX_OC_2_3_BUFF_WATERMARK] = {
		ecpriss_xbar_hal_reg_construct_dummy,
		ecpriss_xbar_hal_reg_parse_xbar_dbg_octx_oc_2_3_buff_watermark_2,
		0x09420000, 0x2454, 0x0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_FH_CFG] = {
		ecpriss_xbar_hal_reg_construct_fh_cfg,
		ecpriss_xbar_hal_reg_parse_fh_cfg,
		0x09420000, 0x10, 0x0, 0, 0, 0, 0},
	[ECPRISS_XBAR_HW_v2_0][ECPRI_XBAR_OC_CFG] = {
		ecpriss_xbar_hal_reg_construct_oc_cfg,
		ecpriss_xbar_hal_reg_parse_oc_cfg,
		0x09420000, 0x14, 0x0, 0, 0, 0, 0},




};
/*
 * ecpriss_xbar_hal_reg_init() - Build the registers information table
 *  See ecpriss_xbar_hal_reg_objs[][] comments
 *
 * Note: As global variables are initialized with zero, any un-overridden
 *  register entry will be zero. By this we recognize them.
 */
int ecpriss_xbar_hal_reg_init(struct device *dev, ecpriss_xbar_hw_name_e hw_ver)
{
	int result = 0;
	/* setup ECPRISS register access */

	do
	{
		ecpriss_xbar_hal_ctx.ecpriss_pdev = dev;
		ecpriss_xbar_hal_ctx.hw_type = hw_ver;

		if(ecpriss_xbar_hal_ctx.hw_type == ECPRISS_XBAR_HW_v1_0){

			ecpriss_xbar_hal_ctx.phy_base = ECPRISS_XBAR_REG_BASE_ADDRESS;

		} else {
			ecpriss_pdata_v2->xbar_ctx_v2->ecpriss_xbar_hal = &ecpriss_xbar_hal_ctx;
			ecpriss_xbar_hal_ctx.phy_base = ECPRISS_XBAR_REG_BASE_ADDRESS_V2;
		}

		ECPRILOGINFO("Mapping  XBAR HAL reg space : 0x%x\n", ecpriss_xbar_hal_ctx.phy_base);

		ecpriss_xbar_hal_ctx.base = ioremap(ecpriss_xbar_hal_ctx.phy_base,ECPRISS_XBAR_REG_SIZE);
		ECPRILOGINFO("XBAR HAL reg mapped : 0x%x\n", ecpriss_xbar_hal_ctx.base);

		if (!ecpriss_xbar_hal_ctx.base) {
			ECPRILOGERR(":ecpriss_xbar_base ioremap err\n");
			result = -EFAULT;
			break;
		}
	}while(0);

	return result;
}

int ecpriss_xbar_hal_lut_reg_init(struct device *dev)
{
	int result = 0;
	/* setup ECPRISS register access */
	do
	{
		if(ecpriss_xbar_hal_ctx.hw_type == ECPRISS_XBAR_HW_v1_0){

			ecpriss_xbar_hal_ctx.lut_phy_base = FH_TABLE_LUT_BASE_ADDRESS;

		} else {
			ecpriss_xbar_hal_ctx.lut_phy_base = FH_TABLE_LUT_BASE_ADDRESS_V2;
		}

		ECPRILOGINFO("Mapping  XBAR LUT HAL reg space : 0x%x\n", ecpriss_xbar_hal_ctx.lut_phy_base);

		ecpriss_xbar_hal_ctx.lut_base = ioremap(ecpriss_xbar_hal_ctx.lut_phy_base, ECPRISS_XBAR_LUT_SIZE);

		if (!ecpriss_xbar_hal_ctx.lut_base) {
			ECPRILOGERR(":ecpriss-base ioremap err\n");
			result = -EFAULT;
			break;
		}
	}while(0);

	return result;
}

uint32_t ecpriss_xbar_reg_read(ecpriss_xbar_hal_reg_type_e  reg_type, uint32_t offset)
{
	uint32_t val = 0;
	if(reg_type == ECPRISS_XBAR_GLOBAL) {
		val = ioread32(ecpriss_xbar_hal_ctx.base + offset);
	}
	else if(reg_type == ECPRISS_XBAR_LUT) {
		val = ioread32(ecpriss_xbar_hal_ctx.lut_base + offset);
	}

	ECPRILOGDBG("Xbar read : reg type %d: offset %d and val %d\n",
			reg_type,offset,val);
	return val;
}



void ecpriss_xbar_reg_write(ecpriss_xbar_hal_reg_type_e  reg_type,
		uint32_t offset,
		uint32_t val)
{
	ECPRILOGDBG("Xbar Write : reg type %d: offset %d and val %d\n",
			reg_type,offset,val);

	if(reg_type == ECPRISS_XBAR_GLOBAL) {
		iowrite32(val, ecpriss_xbar_hal_ctx.base + offset);
	}
	else if(reg_type == ECPRISS_XBAR_LUT) {
		iowrite32(val, ecpriss_xbar_hal_ctx.lut_base + offset);
	}
	return;
}




/*
 * ecpriss_xbar_hal_read_reg_n() - Get n parameterized reg value
 */
u32 ecpriss_xbar_hal_read_reg(ecpriss_xbar_hal_reg_type_e  reg_type,
		enum ecpriss_xbar_hal_reg_name reg)
{
	u32 offset;
	int val=0;
	if (reg >= ECPRI_XBAR_MAX) {
		ECPRILOGERR("Invalid register reg=%u\n", reg);
		WARN_ON(1);
		return -EINVAL;
	}

	//	ECPRILOGINFO("read from %s \n",
	//	ecpriss_xbar_hal_reg_name_str(reg));

	offset = ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].offset;
	if (offset == -1) {
		ECPRILOGERR("Read access to obsolete reg=%s\n",
				ecpriss_xbar_hal_reg_name_str(reg));
		WARN_ON(1);
		return -EPERM;
	}
	//offset += ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].n_ofst * n;
	val =ecpriss_xbar_reg_read(reg_type,offset);
	return val;
}

/*
 * ecpriss_xbar_hal_read_reg_n() - Get n parameterized reg value
 */
u32 ecpriss_xbar_hal_read_reg_n(ecpriss_xbar_hal_reg_type_e  reg_type,
		enum ecpriss_xbar_hal_reg_name reg,
		u32 n)
{
	u32 offset;
	int val=0;
	if (reg >= ECPRI_XBAR_MAX) {
		ECPRILOGERR("Invalid register reg=%u\n", reg);
		WARN_ON(1);
		return -EINVAL;
	}

	//ECPRILOGINFO("read from %s n=%u\n",
	//ecpriss_xbar_hal_reg_name_str(reg), n);

	offset = ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].offset;
	if (offset == -1) {
		ECPRILOGERR("Read access to obsolete reg=%s\n",
				ecpriss_xbar_hal_reg_name_str(reg));
		WARN_ON(1);
		return -EPERM;
	}
	offset += ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].n_ofst * n;
	val =ecpriss_xbar_reg_read(reg_type,offset);
	return val;
}

/*
 * ecpriss_xbar_hal_read_reg_mn() - Get mn parameterized reg value
 */
u32 ecpriss_xbar_hal_read_reg_mn(ecpriss_xbar_hal_reg_type_e  reg_type,
		enum ecpriss_xbar_hal_reg_name reg,
		u32 m,
		u32 n)
{
	u32 offset;
	int val=0;
	if (reg >= ECPRI_XBAR_MAX) {
		ECPRILOGERR("Invalid register reg=%u\n", reg);
		WARN_ON(1);
		return -EINVAL;
	}

	//ECPRILOGINFO("read %s m=%u n=%u\n",
	//ecpriss_xbar_hal_reg_name_str(reg), m, n);
	offset = ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].offset;
	if (offset == -1) {
		ECPRILOGERR("Read access to obsolete reg=%s\n",
				ecpriss_xbar_hal_reg_name_str(reg));
		WARN_ON_ONCE(1);
		return -EPERM;
	}

	offset += ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].m_ofst * m;
	offset += ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].n_ofst * n;
	val =ecpriss_xbar_reg_read(reg_type,offset);
	return val;
}

/*
 * ecpriss_xbar_hal_write_reg_mn() - Write to m/n parameterized reg a raw value
 */
void ecpriss_xbar_hal_write_reg_mn(ecpriss_xbar_hal_reg_type_e  reg_type,
		enum ecpriss_xbar_hal_reg_name reg,
		u32 m,
		u32 n,
		u32 val)
{
	u32 offset;

	if (reg >= ECPRI_XBAR_MAX) {
		ECPRILOGERR("Invalid register reg=%u\n", reg);
		WARN_ON(1);
		return;
	}

	//	ECPRILOGINFO("write to %s m=%u n=%u val=%u\n",
	//	ecpriss_xbar_hal_reg_name_str(reg), m, n, val);
	offset = ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].offset;
	if (offset == -1) {
		ECPRILOGERR("Write access to obsolete reg=%s\n",
				ecpriss_xbar_hal_reg_name_str(reg));
		WARN_ON(1);
		return;
	}

	offset += ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].m_ofst * m;
	offset += ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].n_ofst * n;
	ecpriss_xbar_reg_write(reg_type,offset,val);
	return;
}
/*
 * ecpriss_xbar_hal_write_reg_mn() - Write to m/n parameterized reg a raw value
 */
void ecpriss_xbar_hal_write_reg_mn_fields(ecpriss_xbar_hal_reg_type_e  reg_type,
		enum ecpriss_xbar_hal_reg_name reg,
		u32 m,
		u32 n,
		void *fields)
{
	u32 offset;
	u32 val = 0;
	if (reg >= ECPRI_XBAR_MAX) {
		ECPRILOGERR("Invalid register reg=%u\n", reg);
		WARN_ON(1);
		return;
	}

	//ECPRILOGINFO("write to %s m=%u n=%u val=%u\n",
	//ecpriss_xbar_hal_reg_name_str(reg), m, n, val);
	offset = ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].offset;
	if (offset == -1) {
		ECPRILOGERR("Write access to obsolete reg=%s\n",
				ecpriss_xbar_hal_reg_name_str(reg));
		WARN_ON(1);
		return;
	}

	offset += ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].m_ofst * m;
	offset += ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].n_ofst * n;
	ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].construct(reg, fields, &val);
	ecpriss_xbar_reg_write(reg_type,offset,val);
}
u32 ecpriss_xbar_hal_read_reg_mn_fields(ecpriss_xbar_hal_reg_type_e  reg_type,
		enum ecpriss_xbar_hal_reg_name reg,
		u32 m,
		u32 n,
		void *fields)
{
	u32 val = 0;
	u32 offset;

	if (!fields) {
		ECPRILOGERR("Input error fields\n");
		WARN_ON(1);
		return -EINVAL;
	}

	if (reg >= ECPRI_XBAR_MAX) {
		ECPRILOGERR("Invalid register reg=%u\n", reg);
		WARN_ON(1);
		return -EINVAL;
	}

	//ECPRILOGINFO("read from %s n=%u and parse it\n",
	//	ecpriss_xbar_hal_reg_name_str(reg), n);
	offset = ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].offset;
	if (offset == -1) {
		ECPRILOGERR("Read access to obsolete reg=%s\n",
				ecpriss_xbar_hal_reg_name_str(reg));
		WARN_ON(1);
		return -EPERM;
	}
	offset += ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].m_ofst * m;
	offset += ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].n_ofst * n;
	val =ecpriss_xbar_reg_read(reg_type,offset);
	ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].parse(reg, fields, val);
	return val;
}
/*
 * ecpriss_xbar_hal_read_reg_n_fields() - Get the parsed value of n parameterized reg
 */
u32 ecpriss_xbar_hal_read_reg_n_fields(ecpriss_xbar_hal_reg_type_e  reg_type,
		enum ecpriss_xbar_hal_reg_name reg,
		u32 n,
		void *fields)
{
	u32 val = 0;
	u32 offset;

	if (!fields) {
		ECPRILOGERR("Input error fields\n");
		WARN_ON(1);
		return -EINVAL;
	}

	if (reg >= ECPRI_XBAR_MAX) {
		ECPRILOGERR("Invalid register reg=%u\n", reg);
		WARN_ON(1);
		return -EINVAL;
	}

	//	ECPRILOGINFO("read from %s n=%u and parse it\n",	ecpriss_xbar_hal_reg_name_str(reg), n);

	offset = ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].offset;
	//ECPRILOGINFO("read reg ofst 0x%x \n",	offset);


	if (offset == -1) {
		ECPRILOGERR("Read access to obsolete reg=%s\n",
				ecpriss_xbar_hal_reg_name_str(reg));
		WARN_ON(1);
		return -EPERM;
	}
	offset += ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].n_ofst * n;
	//	ECPRILOGINFO("read reg ofst after n_ofst adjust 0x%x \n",	offset);

	val = ecpriss_xbar_reg_read(reg_type,offset);
	ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].parse(reg, fields, val);

	return val;
}
/*
 * ecpriss_xbar_hal_write_reg_n_fields() - Write to n parameterized reg a prased value
 */
void ecpriss_xbar_hal_write_reg_n_fields(ecpriss_xbar_hal_reg_type_e  reg_type,
		enum ecpriss_xbar_hal_reg_name reg,
		u32 n,
		const void *fields)
{
	u32 val = 0;
	u32 offset;

	if (!fields) {
		ECPRILOGERR("Input error fields=%pK\n", fields);
		WARN_ON(1);
		return;
	}

	if (reg >= ECPRI_XBAR_MAX) {
		ECPRILOGERR("Invalid register reg=%u\n", reg);
		WARN_ON(1);
		return;
	}

	// ECPRILOGINFO("write to %s n=%u after constructing it\n",
	//ecpriss_xbar_hal_reg_name_str(reg), n);
	offset = ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].offset;
	if (offset == -1) {
		ECPRILOGERR("Write access to obsolete reg=%s\n",
				ecpriss_xbar_hal_reg_name_str(reg));
		WARN_ON(1);
		return;
	}
	offset += ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].n_ofst * n;
	ecpriss_xbar_hal_reg_objs[ecpriss_xbar_hal_ctx.hw_type][reg].construct(reg, fields, &val);
	ecpriss_xbar_reg_write(reg_type,offset,val);
	return;
}


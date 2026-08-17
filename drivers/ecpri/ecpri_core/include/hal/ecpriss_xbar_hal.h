/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ECPRISS_XBAR_HAL_H_
#define ECPRISS_XBAR_HAL_H_

#include "ecpriss_hal.h"
#include "ecpriss_xbar_hwio.h"
#include "ecpriss_xbar_hwio_def.h"

#include "ecpriss_xbar_hwio_v2.h"
#include "ecpriss_xbar_hwio_def_v2.h"

/*
 * struct ipahal_context - HAL global context data
 * @hw_type: IPA H/W type/version.
 * @base: Base address to be used for accessing IPA memory. This is
 *  I/O memory mapped address.
 *  Controlled by debugfs. default is off
 * @dent: Debugfs folder dir entry
 * @ipa_pdev: IPA Platform Device. Will be used for DMA memory
 * @empty_fltrt_tbl: Empty table to be used at tables init.
 */
typedef struct ecpriss_xbar_hal_context {
	unsigned int hw_type;
	uint64_t phy_base;
	uint64_t lut_phy_base;
	void __iomem *lut_base;
	void __iomem *base;
	struct device *ecpriss_pdev;
	void *regdumpbuf;
	//ecpriss_xbar_lut_entries_table_s lut_base;
}ecpriss_xbar_hal_context_s;


#define ECPRISS_XBAR_REG_BASE_ADDRESS 0x09220000
#define FH_TABLE_LUT_BASE_ADDRESS     0x9300000
#define C2C_DL_TABLE_LUT_BASE_ADDRESS 0x93C0000
#define C2C_UL_TABLE_LUT_BASE_ADDRESS 0x9480000
#define OC_RX_TABLE_LUT_BASE_ADDRESS  0x9540000

#define ECPRISS_XBAR_LUT_SIZE 0x300000

#define ECPRISS_XBAR_REG_SIZE 0x8000
#define ECPRISS_XBAR_REG_BASE_ADDRESS_V2    0x9420000
#define FH_TABLE_LUT_BASE_ADDRESS_V2     0x9500000
#define C2C_TABLE_LUT_BASE_ADDRESS_V2    0x95C0000
#define OC_RX_TABLE_LUT_BASE_ADDRESS_V2  0x9680000
#define ECPRISS_XBAR_LUT_SIZE_V2 0x240000






#define ECPRI_XBAR_XBAR_FH_RX_LUT_DMA_RING_ID_SHFT   		0
#define ECPRI_XBAR_XBAR_FH_RX_LUT_DMA_RING_ID_BMSK   		0x7f

#define	ECPRI_XBAR_XBAR_FH_RX_LUT_LINK_ID_SHFT       		7
#define ECPRI_XBAR_XBAR_FH_RX_LUT_LINK_ID_BMSK       		0x1ff

#define	ECPRI_XBAR_XBAR_FH_RX_LUT_ROUTE_ORAN_SHFT    		9
#define	ECPRI_XBAR_XBAR_FH_RX_LUT_ROUTE_ORAN_BMSK    		0x3ff

#define	ECPRI_XBAR_XBAR_FH_RX_LUT_ROUTE_C2C_SHFT     		10
#define	ECPRI_XBAR_XBAR_FH_RX_LUT_ROUTE_C2C_BMSK     		0x7ff

#define	ECPRI_XBAR_XBAR_FH_RX_LUT_ROUTE_DMA_SHFT     		11
#define ECPRI_XBAR_XBAR_FH_RX_LUT_ROUTE_DMA_BMSK     		0xfff

#define	ECPRI_XBAR_XBAR_FH_RX_LUT_IS_VALID_SHFT      		12
#define	ECPRI_XBAR_XBAR_FH_RX_LUT_IS_VALID_BMSK      		0x1fff

#define	ECPRI_XBAR_XBAR_OCRX_LUT_M_L2_ENCAP_SHFT     		0
#define	ECPRI_XBAR_XBAR_OCRX_LUT_M_L2_ENCAP_BMSK     		0xff

#define	ECPRI_XBAR_XBAR_OCRX_LUT_M_L3_ENCAP_SHFT     		8
#define	ECPRI_XBAR_XBAR_OCRX_LUT_M_L3_ENCAP_BMSK     		0xffff

#define	ECPRI_XBAR_XBAR_OCRX_LUT_M_L3_ENCAP_VALID_SHFT   	16
#define	ECPRI_XBAR_XBAR_OCRX_LUT_M_L3_ENCAP_VALID_BMSK 		0x1ffff

#define	ECPRI_XBAR_XBAR_OCRX_LUT_M_VALID_SHFT               17
#define	ECPRI_XBAR_XBAR_OCRX_LUT_M_VALID_BMSK               0x3ffff



typedef struct ecpriss_xbar_lut_entries_table {
	uint64_t fh_table_lut_base;
	uint64_t oc_table_lut_base;
	uint64_t c2c_table_lut_base;
}ecpriss_xbar_lut_entries_table_s;

typedef enum ecpriss_xbar_hw_name
{
	ECPRISS_XBAR_HW_v1_0 = 1,
	ECPRISS_XBAR_HW_v2_0,
	ECPRISS_XBAR_HW_MAX
}ecpriss_xbar_hw_name_e;

typedef enum{
	ECPRISS_XBAR_GLOBAL,
	ECPRISS_XBAR_LUT,
}ecpriss_xbar_hal_reg_type_e;

/*
 * Registers names
 *
 * NOTE:: Any change to this enum, need to change to ipareg_name_to_str
 *	array as well.
 */
enum ecpriss_xbar_hal_reg_name {
	ECPRI_XBAR_XBAR_CFG,
	ECPRI_XBAR_XBAR_C2C_IPG_CFG,
	ECPRI_XBAR_XBAR_C2C_CFG,
	ECPRI_XBAR_XBAR_FH_CFG,
	ECPRI_XBAR_XBAR_OC_CFG,
	ECPRI_XBAR_XBAR_OC_LOOPBACK_CFG,
	ECPRI_XBAR_XBAR_FLUSH,
	ECPRI_XBAR_XBAR_FH_STATUS,
	ECPRI_XBAR_XBAR_C2C_STATUS,
	ECPRI_XBAR_XBAR_OCTX_STATUS,
	ECPRI_XBAR_XBAR_LUT_STATUS,
	ECPRI_XBAR_XBAR_FW_IRQ_STATUS,
	ECPRI_XBAR_XBAR_FW_IRQ_MASK,
	ECPRI_XBAR_XBAR_FW_IRQ_CLR,
	ECPRI_XBAR_XBAR_SW_IRQ_STATUS,
	ECPRI_XBAR_XBAR_SW_IRQ_MASK,
	ECPRI_XBAR_XBAR_SW_IRQ_CLR,
	ECPRI_XBAR_XBAR_TPDM_GLOBAL_CFG,
	ECPRI_XBAR_XBAR_FHRX_0_NON_ECPRI_LUT,
	ECPRI_XBAR_XBAR_FHRX_1_NON_ECPRI_LUT,
	ECPRI_XBAR_XBAR_FHRX_2_NON_ECPRI_LUT,
	ECPRI_XBAR_XBAR_FHRX_n_DEFAULT_LUT,
	ECPRI_XBAR_XBAR_FHRX_MSG_TYPE_LUT_0,
	ECPRI_XBAR_XBAR_FHRX_MSG_TYPE_LUT_1,
	ECPRI_XBAR_XBAR_FHRX_MSG_TYPE_n_LUT_RESULT,
	ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_0,
	ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_LUT_1,
	ECPRI_XBAR_XBAR_C2CRX_MSG_TYPE_n_LUT_RESULT,
	ECPRI_XBAR_XBAR_C2CRX_n_DL_DEFAULT_LUT,
	ECPRI_XBAR_XBAR_C2CRX_n_UL_DEFAULT_LUT,
	ECPRI_XBAR_XBAR_C2CRX_NON_ECPRI_LUT,
	ECPRI_XBAR_XBAR_OCRX_FH_n_DEFAULT_LUT,
	ECPRI_XBAR_XBAR_OCRX_C2C_DEFAULT_LUT,
	ECPRI_XBAR_XBAR_UC_FHRX_BUFF_ADDR,
	ECPRI_XBAR_XBAR_UC_FHRX_BUFF_SIZE,
	ECPRI_XBAR_XBAR_UC_FHRX_BUFF_RD,
	ECPRI_XBAR_XBAR_UC_FHRX_PKT_ATTR_POP,
	ECPRI_XBAR_XBAR_UC_FHRX_PKT_ATTR_NO_POP,
	ECPRI_XBAR_XBAR_UC_FHRX_STATUS,
	ECPRI_XBAR_XBAR_UC_FH_CTRL,
	ECPRI_XBAR_XBAR_UC_FHTX_PKT_ADDR,
	ECPRI_XBAR_XBAR_UC_FHTX_PKT_ATTR,
	ECPRI_XBAR_LUT_XBAR_FHRX_m_LUT_n,
	ECPRI_XBAR_LUT_XBAR_C2CRX_m_DL_LUT_n,
	ECPRI_XBAR_LUT_XBAR_C2CRX_m_UL_LUT_n,
	ECPRI_XBAR_LUT_XBAR_OCRX_m_LUT_n,
	//Stats
	ECPRI_XBAR_XBAR_DBG_FHRX_PKT_CNT_n,//per link
	ECPRI_XBAR_XBAR_DBG_FHRX_DMA_PKT_CNT,
	ECPRI_XBAR_XBAR_DBG_FHRX_UC_PKT_CNT,
	ECPRI_XBAR_XBAR_DBG_FHRX_UC_ERR_PKT_CNT,
	ECPRI_XBAR_XBAR_DBG_FHRX_ERR_PKT_CNT,
	ECPRI_XBAR_XBAR_DBG_FHTX_PKT_CNT_n,//per link
	ECPRI_XBAR_XBAR_DBG_FHTX_C2C_PKT_OVF_CNT,
	ECPRI_XBAR_XBAR_DBG_FHTX_DMA_PKT_CNT,
	ECPRI_XBAR_XBAR_DBG_FHTX_UC_PKT_CNT,
	ECPRI_XBAR_XBAR_DBG_C2CRX_PKT_CNT_n,// per link
	ECPRI_XBAR_XBAR_DBG_C2CRX_DMA_PKT_CNT,
	ECPRI_XBAR_XBAR_DBG_C2CRX_ERR_PKT_CNT,
	ECPRI_XBAR_XBAR_DBG_C2CTX_PKT_CNT_n,//per link
	ECPRI_XBAR_XBAR_DBG_C2CTX_FH_PKT_OVF_CNT,
	ECPRI_XBAR_XBAR_DBG_C2CTX_DMA_PKT_CNT,
	ECPRI_XBAR_XBAR_DBG_OCTX_C2C_PKT_DROP_CNT,
	ECPRI_XBAR_XBAR_DBG_OCTX_PKT_CNT_n, //per link
	ECPRI_XBAR_XBAR_DBG_OCRX_PKT_CNT_n, //per link
	ECPRI_XBAR_XBAR_DBG_OCRX_FH_PKT_CNT_n,//per link
	ECPRI_XBAR_XBAR_DBG_OCRX_C2C_PKT_CNT_n, //per link
	ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT,
	ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT,
	ECPRI_XBAR_XBAR_DBG_C2CRX_UNKNOWN_PCID_CNT,
	ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_INFO_1_n,
	ECPRI_XBAR_XBAR_DBG_OCRX_UNKNOWN_PCID_INFO_2_n,
	ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_INFO_1_n,
	ECPRI_XBAR_XBAR_DBG_FHRX_UNKNOWN_PCID_INFO_2_n,
	ECPRI_XBAR_XBAR_DBG_OCRX_FH_BUFF_WATERMARK,
	ECPRI_XBAR_XBAR_DBG_OCRX_0_1_BUFF_WATERMARK,
	ECPRI_XBAR_XBAR_DBG_OCRX_2_3_BUFF_WATERMARK,
	ECPRI_XBAR_XBAR_DBG_OCTX_OC_0_1_BUFF_WATERMARK,
	ECPRI_XBAR_XBAR_DBG_OCTX_OC_2_3_BUFF_WATERMARK,


	ECPRI_XBAR_CFG,
	ECPRI_XBAR_LUT_STATUS,
	ECPRI_XBAR_FLUSH,
	ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_CFG,
	ECPRI_XBAR_FHRX_MSG_TYPE_UPLANE_VALID,
	ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_CFG,
	ECPRI_XBAR_FHRX_MSG_TYPE_CPLANE_VALID,
	ECPRI_XBAR_FHRX_MSG_TYPE_LUT_0,
	ECPRI_XBAR_FHRX_MSG_TYPE_LUT_1,
	ECPRI_XBAR_SW_IRQ_STATUS,
	ECPRI_XBAR_SW_IRQ_MASK,
	ECPRI_XBAR_SW_IRQ_CLR,
	ECPRI_XBAR_LUT_XBAR_FHRX_m_LUT_n_V2,
	ECPRI_XBAR_LUT_XBAR_C2CRX_m_LUT_n_V2,
	ECPRI_XBAR_LUT_XBAR_OCRX_m_LUT_n_V2,

	ECPRI_XBAR_DBG_FHRX_PKT_CNT_n,
	ECPRI_XBAR_DBG_FHRX_DMA_PKT_CNT,
	ECPRI_XBAR_DBG_FHRX_UC_PKT_CNT,
	ECPRI_XBAR_DBG_FHRX_UC_ERR_PKT_CNT,
	ECPRI_XBAR_DBG_FHRX_ERR_PKT_CNT,
	ECPRI_XBAR_DBG_FHRX_OC_PKT_CNT,
	ECPRI_XBAR_DBG_FHRX_C2C_PKT_CNT,
	ECPRI_XBAR_DBG_FHTX_PKT_CNT_n,
	ECPRI_XBAR_DBG_FHTX_C2C_PKT_OVF_DROP_CNT,
	ECPRI_XBAR_DBG_FHTX_DMA_PKT_CNT,
	ECPRI_XBAR_DBG_FHTX_UC_PKT_CNT,
	ECPRI_XBAR_DBG_FHTX_C2C_PKT_CNT_n,
	ECPRI_XBAR_DBG_FHTX_OC_PKT_CNT_n,


	ECPRI_XBAR_DBG_C2CRX_PKT_CNT_n,
	ECPRI_XBAR_DBG_C2CRX_DMA_PKT_CNT,
	ECPRI_XBAR_DBG_C2CRX_ERR_PKT_CNT,
	ECPRI_XBAR_DBG_C2CTX_PKT_CNT_n,
	ECPRI_XBAR_DBG_C2CTX_FH_PKT_OVF_DROP_CNT,
	ECPRI_XBAR_DBG_C2CTX_DMA_PKT_CNT,
	ECPRI_XBAR_DBG_C2CRX_FH_PKT_CNT_n,
	ECPRI_XBAR_DBG_C2CRX_OC_PKT_CNT_n,

	ECPRI_XBAR_DBG_OCTX_C2C_PKT_DROP_CNT,
	ECPRI_XBAR_DBG_OCTX_FH_PKT_DROP_CNT,
	ECPRI_XBAR_DBG_OCTX_C2C_LEN_ERR_CNT,
	ECPRI_XBAR_DBG_OCTX_FH_LEN_ERR_CNT,
	ECPRI_XBAR_DBG_OCTX_PKT_CNT_n,
	ECPRI_XBAR_DBG_OCRX_PKT_CNT_n,
	ECPRI_XBAR_DBG_OCRX_FH_PKT_CNT_n,
	ECPRI_XBAR_DBG_OCRX_C2C_PKT_CNT_n,


	ECPRI_XBAR_LUT_XBAR_NFAPI_n,

	ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_CNT,
	ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_INFO_1_n,
	ECPRI_XBAR_DBG_OCRX_UNKNOWN_PCID_INFO_2_n,
	ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_CNT,
	ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_INFO_1_n,
	ECPRI_XBAR_DBG_FHRX_UNKNOWN_PCID_INFO_2_n,
	ECPRI_XBAR_DBG_OCRX_0_1_BUFF_WATERMARK,
	ECPRI_XBAR_DBG_OCRX_2_3_BUFF_WATERMARK,
	ECPRI_XBAR_DBG_OCRX_FH_BUFF_WATERMARK,
	ECPRI_XBAR_DBG_OCTX_OC_0_1_BUFF_WATERMARK,
	ECPRI_XBAR_DBG_OCTX_OC_2_3_BUFF_WATERMARK,
	ECPRI_XBAR_FH_CFG,
	ECPRI_XBAR_OC_CFG,

	ECPRI_XBAR_FHRX_n_DEFAULT_DMA_CHANNEL,
	ECPRI_XBAR_C2CRX_n_DEFAULT_LUT,
	ECPRI_XBAR_C2CRX_n_DEFAULT_DMA_CHANNEL,

	ECPRI_XBAR_MAX


};
/*
 * struct ecpriss_hal_reg_obj - Register H/W information for specific
 * ECPRISS version
 * @construct - CB to construct register value from abstracted structure
 * @parse - CB to parse register value to abstracted structure
 * @offset - register offset relative to base address
 * @n_ofst - N parameterized register sub-offset
 * @n_start - starting n for n_registers used for printing
 * @n_end - ending n for n_registers used for printing
 * @en_print - enable this register to be printed when the device crashes
 */
struct ecpriss_xbar_hal_reg_obj {
	void (*construct)(enum ecpriss_xbar_hal_reg_name reg,
			const void *fields,
			u32 *val);
	void (*parse)(enum ecpriss_xbar_hal_reg_name reg, void *fields,
			u32 val);
	u32 base;
	u32 offset;
	u32 n_ofst;
	int n_start;
	int n_end;
	bool en_print;
	u32 m_ofst;
};
extern ecpriss_xbar_hal_context_s     ecpriss_xbar_hal_ctx;
u32 ecpriss_xbar_hal_read_reg(ecpriss_xbar_hal_reg_type_e reg_type,
		enum ecpriss_xbar_hal_reg_name reg);

u32 ecpriss_xbar_hal_read_reg_n(ecpriss_xbar_hal_reg_type_e reg_type,
		enum ecpriss_xbar_hal_reg_name reg, u32 n);


u32 ecpriss_xbar_hal_read_reg_mn(ecpriss_xbar_hal_reg_type_e reg_type,
		enum ecpriss_xbar_hal_reg_name reg, u32 m, u32 n);

void ecpriss_xbar_hal_write_reg_mn(ecpriss_xbar_hal_reg_type_e reg_type,
		enum ecpriss_xbar_hal_reg_name reg, u32 m, u32 n, u32 val);

u32 ecpriss_xbar_hal_read_reg_n_fields(ecpriss_xbar_hal_reg_type_e reg_type,
		enum ecpriss_xbar_hal_reg_name reg, u32 n, void *fields);

u32 ecpriss_xbar_hal_read_reg_mn_fields(ecpriss_xbar_hal_reg_type_e reg_type,
		enum ecpriss_xbar_hal_reg_name reg, u32 m, u32 n, void *fields);

void ecpriss_xbar_hal_write_reg_n_fields(ecpriss_xbar_hal_reg_type_e reg_type,
		enum ecpriss_xbar_hal_reg_name reg, u32 n, const void *fields);

const char *ecpriss_xbar_hal_reg_name_str(
		enum ecpriss_xbar_hal_reg_name reg_name);

void ecpriss_xbar_hal_write_reg_mn_fields(ecpriss_xbar_hal_reg_type_e  reg_type,
		enum ecpriss_xbar_hal_reg_name reg, u32 m, u32 n, void *fields);

int ecpriss_xbar_hal_lut_reg_init(struct device *dev);

int ecpriss_xbar_hal_reg_init(struct device *dev, ecpriss_xbar_hw_name_e hw_ver);
#endif

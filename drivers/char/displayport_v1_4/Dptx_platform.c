// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
/*
* Copyright (C) Telechips Inc.
*/

#include <linux/delay.h>
#include <linux/slab.h>
#include <soc/tcc/chipinfo.h>

#include "Dptx_v14.h"
#include "Dptx_reg.h"
#include "Dptx_drm_dp_addition.h"
#include "Dptx_dbg.h"

#define MAX_TRY_PLL_LOCK				10

static struct Dptx_Params *pstHandle;


int32_t Dptx_Platform_Init_Params(struct Dptx_Params *pstDptx, struct device *pstParentDev)
{
	uint8_t ucStream_Index;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	const void *pvRet;
	pstDptx->pstParentDev				= pstParentDev;
	pstDptx->bSpreadSpectrum_Clock		= (bool)true;
	pstDptx->eEstablished_Timing		= DMT_NOT_SUPPORTED;
	pstDptx->uiHDCP22_RegAddr_Offset	= DP_HDCP_OFFSET;
	pstDptx->uiRegBank_RegAddr_Offset	= DP_REGISTER_BANK_OFFSET;
	pstDptx->uiCKC_RegAddr_Offset		= DP_CKC_OFFSET;
	pstDptx->uiProtect_RegAddr_Offset	= DP_PROTECT_OFFSET;

	pstDptx->ucMax_Lanes				= DPTX_MAX_LINK_LANES;

	pstDptx->pvHPD_Intr_CallBack		= NULL;

	for (ucStream_Index = 0; ucStream_Index < (uint8_t)PHY_INPUT_STREAM_MAX; ucStream_Index++) {
		pstDptx->aucVCP_Id[ucStream_Index] = (ucStream_Index + 1U);

		pstDptx->paucEdidBuf_Entry[ucStream_Index] = (uint8_t *)kzalloc((size_t)DPTX_EDID_BUFLEN, GFP_KERNEL);

		if (pstDptx->paucEdidBuf_Entry[ucStream_Index] == NULL) {
			dptx_err("failed to alloc EDID memory");
			iRetVal = DPTX_RETURN_ENOMEM;
		} else {
			pvRet = memset(pstDptx->paucEdidBuf_Entry[ucStream_Index], 0, (size_t)DPTX_EDID_BUFLEN);
			if (pvRet == NULL) {
				dptx_err("Can't memset to 0 in Dev memory");
				iRetVal = DPTX_RETURN_ENOMEM;
			}
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		pstDptx->pucEdidBuf = (uint8_t *)kzalloc((size_t)DPTX_EDID_BUFLEN, GFP_KERNEL);
		if (pstDptx->pucEdidBuf == NULL) {
			dptx_err("failed to alloc EDID Buf");
			iRetVal = DPTX_RETURN_ENOMEM;
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		pstDptx->uiTCC805X_Revision = get_chip_rev();

		if (pstDptx->uiTCC805X_Revision != (uint32_t)TCC805X_REVISION_ES) {
			iRetVal = Dptx_Platform_Get_PHY_StandardLane_PinConfig(pstDptx);

			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				iRetVal = Dptx_Platform_Get_SDMBypass_Ctrl(pstDptx);
			}
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				iRetVal = Dptx_Platform_Get_SRVCBypass_Ctrl(pstDptx);
			}
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				iRetVal = Dptx_Platform_Get_MuxSelect(pstDptx);
			}
		}
	}

	return iRetVal;
}

int32_t Dptx_Platform_Init(struct Dptx_Params *pstDptx)
{
	bool bPLL_LockStatus;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	iRetVal = Dptx_Platform_Set_PLL_Divisor(pstDptx);
	if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
		/* For Coverity */
		dptx_err("from Dptx_Platform_Set_PLL_Divisor()");
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		iRetVal = Dptx_Platform_Get_PLLLock_Status(pstDptx, &bPLL_LockStatus);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
			/* For Coverity */
			dptx_err("from Dptx_Platform_Get_PLLLock_Status()");
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		iRetVal = Dptx_Platform_Set_PLL_ClockSource(pstDptx, (uint8_t)DPTX_CLKCTRL0_PLL_DIVIDER_OUTPUT);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
			/* For Coverity */
			dptx_err("from Dptx_Platform_Set_PLL_ClockSource()");
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		iRetVal = Dptx_Platform_Set_RegisterBank(pstDptx, (enum PHY_LINK_RATE)pstDptx->ucMax_Rate);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
			/* For Coverity */
			dptx_err("from Dptx_Platform_Set_RegisterBank()");
		}
	}

	pstHandle = pstDptx;

	return iRetVal;
}

int32_t Dptx_Platform_Deinit(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	iRetVal = Dptx_Platform_Free_Handle(pstDptx);

	pstHandle = NULL;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Platform_Set_Protect_PW(struct Dptx_Params *pstDptx, uint32_t uiProtect_Cfg_PW)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	Dptx_Reg_Writel(pstDptx, (pstDptx->uiProtect_RegAddr_Offset | (uint32_t)DP_PORTECT_CFG_PW_OK), (uint32_t)uiProtect_Cfg_PW);

	return iRetVal;
}

int32_t Dptx_Platform_Set_Protect_Cfg_Access(struct Dptx_Params *pstDptx, bool bAccessable)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	
	if (bAccessable) {
		/* For Coverity */
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiProtect_RegAddr_Offset | (uint32_t)DP_PORTECT_CFG_ACCESS), (uint32_t)DP_PORTECT_CFG_ACCESSABLE);
	} else {
		/* For Coverity */
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiProtect_RegAddr_Offset | (uint32_t)DP_PORTECT_CFG_ACCESS), (uint32_t)DP_PORTECT_CFG_NOT_ACCESSABLE);
	}

	return iRetVal;
}

int32_t Dptx_Platform_Set_Protect_Cfg_Lock(struct Dptx_Params *pstDptx, bool bAccessable)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	if (bAccessable) {
		/* For Coverity */
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiProtect_RegAddr_Offset | (uint32_t)DP_PORTECT_CFG_LOCK), (uint32_t)DP_PORTECT_CFG_UNLOCKED);
	} else {
		/* For Coverity */
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiProtect_RegAddr_Offset | (uint32_t)DP_PORTECT_CFG_LOCK), (uint32_t)DP_PORTECT_CFG_LOCKED);
	}

	return iRetVal;
}

int32_t Dptx_Platform_Set_PLL_Divisor(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	
	Dptx_Reg_Writel(pstDptx, (pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DPTX_CKC_CFG_PLLCON), 0x00000FC0U);
	Dptx_Reg_Writel(pstDptx, (pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DPTX_CKC_CFG_PLLMON), 0x00008800U);
	Dptx_Reg_Writel(pstDptx, (pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DPTX_CKC_CFG_CLKDIVC0), (uint32_t)DIV_CFG_CLK_200HMZ);
	Dptx_Reg_Writel(pstDptx, (pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DPTX_CKC_CFG_CLKDIVC1), (uint32_t)DIV_CFG_CLK_160HMZ);
	Dptx_Reg_Writel(pstDptx, (pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DPTX_CKC_CFG_CLKDIVC2), (uint32_t)DIV_CFG_CLK_100HMZ);
	Dptx_Reg_Writel(pstDptx, (pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DPTX_CKC_CFG_CLKDIVC3), (uint32_t)DIV_CFG_CLK_40HMZ);
	Dptx_Reg_Writel(pstDptx, (pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DPTX_CKC_CFG_PLLPMS), 0x05026403U);
	Dptx_Reg_Writel(pstDptx, (pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DPTX_CKC_CFG_PLLPMS), 0x85026403U);

	return iRetVal;
}

int32_t Dptx_Platform_Set_PLL_ClockSource(struct Dptx_Params *pstDptx, uint8_t ucClockSource)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegMap_Val = 0;

	uiRegMap_Val |= ucClockSource;

	Dptx_Reg_Writel(pstDptx, (pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DPTX_CKC_CFG_CLKCTRL0), uiRegMap_Val);
	Dptx_Reg_Writel(pstDptx, (pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DPTX_CKC_CFG_CLKCTRL1), uiRegMap_Val);
	Dptx_Reg_Writel(pstDptx, (pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DPTX_CKC_CFG_CLKCTRL2), uiRegMap_Val);
	Dptx_Reg_Writel(pstDptx, (pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DPTX_CKC_CFG_CLKCTRL3), uiRegMap_Val);

	return iRetVal;
}

int32_t Dptx_Platform_Get_PLLLock_Status(struct Dptx_Params *pstDptx, bool *pbPll_Locked)
{
	bool bPllLock;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint8_t ucCount = 0;
	uint32_t uiRegMap_PllPMS;

	do {
		uiRegMap_PllPMS = Dptx_Reg_Readl(pstDptx, (pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DPTX_CKC_CFG_PLLPMS));

		bPllLock = ((uiRegMap_PllPMS & (uint32_t)DPTX_PLLPMS_LOCK_MASK) == 0U) ? (bool)false : (bool)true;

		if (!bPllLock) {
			/* For Coverity */
			mdelay(1);
		}

		ucCount++;
	} while (!bPllLock && (ucCount < (uint8_t)MAX_TRY_PLL_LOCK));

	if (bPllLock) {
		/* For Coverity */
		dptx_dbg("Success to get PLL Locking after %dms", (uint32_t)((ucCount + 1) * 1));
	} else {
		/* For Coverity */
		dptx_err("Fail to get PLL Locking ");
	}

	*pbPll_Locked = bPllLock;

	return iRetVal;
}

int32_t Dptx_Platform_Set_RegisterBank(struct Dptx_Params *pstDptx, enum PHY_LINK_RATE eLinkRate)
{
	uint8_t ucLinkRate;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_R_data, uiReg_W_data;

	uiReg_R_data = Dptx_Reg_Readl(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_22));

	if ((uiReg_R_data & (uint32_t)AXI_SLAVE_BRIDGE_RST_MASK) == 0U) {
		/* For Coverity */
		uiReg_W_data = (uint32_t)0x00000008;
	} else {
		/* For Coverity */
		uiReg_W_data = (uint32_t)0x00000000;
	}

	dptx_dbg("Writing REG_22[0x12480058]: 0x%x -> 0x%x", uiReg_R_data, uiReg_W_data);

	ucLinkRate = (uint8_t)eLinkRate;

	switch (ucLinkRate) {
	case (uint8_t)LINK_RATE_RBR:
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_0), 0x004D0000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_20), 0x00002501U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_11), 0x00000401U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_12), 0x00000000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_13), 0x00000000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_14), 0x00000000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_16), 0x00000000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_15), 0x08080808U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_17), 0x10000000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_20), 0x00002501U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_21), 0x00000000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_23), 0x00000008U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_22), 0x00000801U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_22), uiReg_W_data);
		break;
	case (uint8_t)LINK_RATE_HBR:
	case (uint8_t)LINK_RATE_HBR2:
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_0), 0x002D0000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_21), 0x00000000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_20), 0x00002501U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_1), 0xA00F0001U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_2), 0x00A80122U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_3), 0x001E8A00U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_4), 0x00000004U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_7), 0x00000C0CU);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_8), 0x05460546U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_9), 0x00000011U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_10), 0x00700000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_11), 0x00000401U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_13), 0xA8000122U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_14), 0x418A001EU);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_16), 0x00004000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_15), 0x0B000000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_17), 0x10000000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_20), 0x00002501U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_10), 0x08700000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_12), 0xA00F0003U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_23), 0x00000008U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_22), 0x00000801U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_22), uiReg_W_data);
		break;
	case (uint8_t)LINK_RATE_HBR3:
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_0), 0x002D0000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_21), 0x00000000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_20), 0x00002501U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_1), 0xA00F0001U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_2), 0x00A80122U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_3), 0x001E8A00U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_4), 0x00000004U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_7), 0x00000C0CU);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_8), 0x05460546U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_9), 0x00000011U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_10), 0x00700000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_11), 0x00000401U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_13), 0xA8000616U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_14), 0x115C0045U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_16), 0x00004000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_15), 0x8B000000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_17), 0x10000000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_20), 0x00002501U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_10), 0x08700000U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_12), 0xA00F0003U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_23), 0x00000008U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_22), 0x00000801U);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_22), uiReg_W_data);
		break;
	default:
		dptx_err("Invalid PHY rate %d\n", eLinkRate);
		iRetVal = DPTX_RETURN_EINVAL;
		break;
	}

	return iRetVal;
}

int32_t Dptx_Platform_Set_APAccess_Mode(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegMap_R_ApbSel, uiRegMap_W_ApbSel;

	uiRegMap_R_ApbSel = Dptx_MCU_DP_Reg_Read(pstDptx, (uint32_t)DPTX_APB_SEL_REG);

	uiRegMap_W_ApbSel = (uint32_t)(uiRegMap_R_ApbSel | BIT(DPTX_APB_SEL_MASK));
	Dptx_MCU_DP_Reg_Write(pstDptx, (uint32_t)DPTX_APB_SEL_REG, uiRegMap_W_ApbSel);

	dptx_dbg("Register access...MCU Reg Offset[0x%x]:0x%08x -> 0x%08x", (uint32_t)DPTX_APB_SEL_REG, uiRegMap_R_ApbSel, uiRegMap_W_ApbSel);

	return iRetVal;
}

int32_t Dptx_Platform_Set_PMU_ColdReset_Release(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegMap_R_HsmRst_Msk, uiRegMap_W_HsmRst_Msk;

	uiRegMap_R_HsmRst_Msk = Dptx_PMU_Reg_Read(pstDptx, (uint32_t)PMU_HSM_RSTN_MASK);

	if ((uiRegMap_R_HsmRst_Msk & PMU_COLD_RESET_MASK) != 0U) {
		uiRegMap_W_HsmRst_Msk = (uint32_t)(uiRegMap_R_HsmRst_Msk & ~PMU_COLD_RESET_MASK);
		Dptx_PMU_Reg_Write(pstDptx, (uint32_t)PMU_HSM_RSTN_MASK, uiRegMap_W_HsmRst_Msk);

		dptx_dbg("DP Cold reset mask release...0x%08x -> 0x%08x", uiRegMap_R_HsmRst_Msk, uiRegMap_W_HsmRst_Msk);
	}

	return iRetVal;
}

int32_t Dptx_Platform_Set_PHY_StandardLane_PinConfig(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegMap_R_Reg24, uiRegMap_W_Reg24;

	uiRegMap_R_Reg24 = Dptx_Reg_Readl(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24));

	uiRegMap_W_Reg24 = (pstDptx->bPHY_Lane_Reswap == (bool)true) ? (uint32_t)(uiRegMap_R_Reg24 | (uint32_t)STD_EN_MASK) : (uint32_t)(uiRegMap_R_Reg24 & ~STD_EN_MASK);

	Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24), uiRegMap_W_Reg24);

	dptx_dbg("Set PHY lane swap %s...Reg[0x%x]: 0x%08x -> 0x%08x ",
				(pstDptx->bPHY_Lane_Reswap) ? "On" : "Off",
				(uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24),
				uiRegMap_R_Reg24,
				uiRegMap_W_Reg24);

	return iRetVal;
}

int32_t Dptx_Platform_Get_PHY_StandardLane_PinConfig(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegMap_R_Reg24;

	uiRegMap_R_Reg24 = Dptx_Reg_Readl(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24));

	pstDptx->bPHY_Lane_Reswap = ((uiRegMap_R_Reg24 & (uint32_t)STD_EN_MASK) == 0U) ? (bool)false : (bool)true;

	return iRetVal;
}

int32_t Dptx_Platform_Set_SDMBypass_Ctrl(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegMap_R_Reg24, uiRegMap_W_Reg24;

	uiRegMap_R_Reg24 = Dptx_Reg_Readl(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24));

	uiRegMap_W_Reg24 = (pstDptx->bSDM_Bypass == (bool)true) ? (uint32_t)(uiRegMap_R_Reg24 | (uint32_t)SDM_BYPASS_MASK) : (uint32_t)(uiRegMap_R_Reg24 & ~SDM_BYPASS_MASK);

	Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24), uiRegMap_W_Reg24);

	dptx_dbg("Set SDM Bypass %s...Reg[0x%x]: 0x%08x -> 0x%08x ",
				(pstDptx->bSDM_Bypass) ? "On" : "Off",
				(uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24),
				uiRegMap_R_Reg24,
				uiRegMap_W_Reg24);

	return iRetVal;
}

int32_t Dptx_Platform_Get_SDMBypass_Ctrl(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegMap_R_Reg24;

	uiRegMap_R_Reg24 = Dptx_Reg_Readl(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24));

	pstDptx->bSDM_Bypass = ((uiRegMap_R_Reg24 & (uint32_t)SDM_BYPASS_MASK) == 0U) ? (bool)false : (bool)true;

	return iRetVal;
}

int32_t Dptx_Platform_Set_SRVCBypass_Ctrl(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegMap_R_Reg24, uiRegMap_W_Reg24;

	uiRegMap_R_Reg24 = Dptx_Reg_Readl(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24));

	uiRegMap_W_Reg24 = (pstDptx->bSRVC_Bypass == (bool)true) ? (uint32_t)(uiRegMap_R_Reg24 | (uint32_t)SRVC_BYPASS_MASK) : (uint32_t)(uiRegMap_R_Reg24 & ~SRVC_BYPASS_MASK);

	Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24), uiRegMap_W_Reg24);

	dptx_dbg("Set SRVC Bypass %s...Reg[0x%x]: 0x%08x -> 0x%08x ",
				(pstDptx->bSRVC_Bypass) ? "On" : "Off",
				(uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24),
				uiRegMap_R_Reg24,
				uiRegMap_W_Reg24);

	return iRetVal;
}

int32_t Dptx_Platform_Get_SRVCBypass_Ctrl(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegMap_R_Reg24;

	uiRegMap_R_Reg24 = Dptx_Reg_Readl(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24));

	pstDptx->bSRVC_Bypass = ((uiRegMap_R_Reg24 & (uint32_t)SRVC_BYPASS_MASK) == 0U) ? (bool)false : (bool)true;

	return iRetVal;
}

int32_t Dptx_Platform_Set_MuxSelect(struct Dptx_Params *pstDptx)
{
	uint8_t ucDP_Index;
	uint8_t ucRegMap_MuxSel_Shift = 0;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegMap_R_Reg24, uiRegMap_W_Reg24;
	uint32_t uiRegMap_MuxSel_Mask;

	for (ucDP_Index = 0; ucDP_Index < (uint8_t)PHY_INPUT_STREAM_MAX; ucDP_Index++) {
		switch (ucDP_Index) {
		case (uint8_t)PHY_INPUT_STREAM_0:
			uiRegMap_MuxSel_Mask = (uint32_t)SOURCE0_MUX_SEL_MASK;
			ucRegMap_MuxSel_Shift = (uint8_t)SOURCE0_MUX_SEL_SHIFT;
			break;
		case (uint8_t)PHY_INPUT_STREAM_1:
			uiRegMap_MuxSel_Mask = (uint32_t)SOURCE1_MUX_SEL_MASK;
			ucRegMap_MuxSel_Shift = (uint8_t)SOURCE1_MUX_SEL_SHIFT;
			break;
		case (uint8_t)PHY_INPUT_STREAM_2:
			uiRegMap_MuxSel_Mask = (uint32_t)SOURCE2_MUX_SEL_MASK;
			ucRegMap_MuxSel_Shift = (uint8_t)SOURCE2_MUX_SEL_SHIFT;
			break;
		case (uint8_t)PHY_INPUT_STREAM_3:
		default:
			uiRegMap_MuxSel_Mask = (uint32_t)SOURCE3_MUX_SEL_MASK;
			ucRegMap_MuxSel_Shift = (uint8_t)SOURCE3_MUX_SEL_SHIFT;
			break;
		}

		uiRegMap_R_Reg24 = Dptx_Reg_Readl(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24));

		uiRegMap_W_Reg24 = (uiRegMap_R_Reg24 & ~uiRegMap_MuxSel_Mask);
		uiRegMap_W_Reg24 |= (uint32_t)(pstDptx->aucMuxInput_Index[ucDP_Index] << ucRegMap_MuxSel_Shift);

		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24), uiRegMap_W_Reg24);

		dptx_notice("Set Mux select[0x%x](0x%x -> 0x%x): Mux %d -> DP %d",
						(uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24),
						uiRegMap_R_Reg24,
						uiRegMap_W_Reg24,
						pstDptx->aucMuxInput_Index[ucDP_Index],
						ucDP_Index);
	}

	return iRetVal;
}

int32_t Dptx_Platform_Get_MuxSelect(struct Dptx_Params *pstDptx)
{
	uint8_t ucRegMap_MuxSel_Shift = 0;
	uint8_t ucDP_Index;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegMap_R_Reg24;
	uint32_t uiRegMap_MuxSel_Mask;

	uiRegMap_R_Reg24 = Dptx_Reg_Readl(pstDptx, (uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24));

	pstDptx->uiMuxInput_Idx = (uiRegMap_R_Reg24 & (uint32_t)SOURCE_MUX_SEL_MASK);

	for (ucDP_Index = 0; ucDP_Index < (uint8_t)PHY_INPUT_STREAM_MAX; ucDP_Index++) {
		switch (ucDP_Index) {
		case (uint8_t)PHY_INPUT_STREAM_0:
			uiRegMap_MuxSel_Mask = (uint32_t)SOURCE0_MUX_SEL_MASK;
			ucRegMap_MuxSel_Shift = (uint8_t)SOURCE0_MUX_SEL_SHIFT;
			break;
		case (uint8_t)PHY_INPUT_STREAM_1:
			uiRegMap_MuxSel_Mask = (uint32_t)SOURCE1_MUX_SEL_MASK;
			ucRegMap_MuxSel_Shift = (uint8_t)SOURCE1_MUX_SEL_SHIFT;
			break;
		case (uint8_t)PHY_INPUT_STREAM_2:
			uiRegMap_MuxSel_Mask = (uint32_t)SOURCE2_MUX_SEL_MASK;
			ucRegMap_MuxSel_Shift = (uint8_t)SOURCE2_MUX_SEL_SHIFT;
			break;
		case (uint8_t)PHY_INPUT_STREAM_3:
		default:
			uiRegMap_MuxSel_Mask = (uint32_t)SOURCE3_MUX_SEL_MASK;
			ucRegMap_MuxSel_Shift = (uint8_t)SOURCE3_MUX_SEL_SHIFT;
			break;
		}

		pstDptx->aucMuxInput_Index[ucDP_Index] = (uint8_t)((uiRegMap_R_Reg24 & uiRegMap_MuxSel_Mask) >> ucRegMap_MuxSel_Shift);

		dptx_notice("Get Mux select[0x%x](0x%x): DP %d -> Mux %d ",
						(uint32_t)(pstDptx->uiRegBank_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_24),
						uiRegMap_R_Reg24,
						ucDP_Index,
						pstDptx->aucMuxInput_Index[ucDP_Index]);
	}

	return iRetVal;
}

int32_t Dptx_Platform_Set_ClkPath_To_XIN(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_Offset, uiRegMap_PLLPMS;

	iRetVal = Dptx_Platform_Set_Protect_PW(pstDptx, (uint32_t)DP_PORTECT_CFG_PW_VAL);

	if (iRetVal == 0) {
		/* For Coverity */
		iRetVal = Dptx_Platform_Set_Protect_Cfg_Lock(pstDptx, (bool)true);
	}

	if (iRetVal == 0) {
		/* For Coverity */
		iRetVal = Dptx_Platform_Set_Protect_Cfg_Access(pstDptx, (bool)true);
	}

	if (iRetVal == 0) {
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_0), (uint32_t)0x00);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_1), (uint32_t)0x00);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_2), (uint32_t)0x00);
		Dptx_Reg_Writel(pstDptx, (uint32_t)(pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_3), (uint32_t)0x00);

		uiReg_Offset = (uint32_t)(pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_4);
		
		uiRegMap_PLLPMS = Dptx_Reg_Readl(pstDptx, uiReg_Offset);
		uiRegMap_PLLPMS = (uint32_t)(uiRegMap_PLLPMS | (uint32_t)ENABLE_BYPASS_MODE_MASK);
		Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_PLLPMS);

		uiRegMap_PLLPMS = Dptx_Reg_Readl(pstDptx, uiReg_Offset);
		uiRegMap_PLLPMS = (uiRegMap_PLLPMS & ~RESET_PLL_MASK);
		Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_PLLPMS);

		dptx_dbg("Clk path to XIN...Reg[0x%x] -> 0x%08x", (pstDptx->uiCKC_RegAddr_Offset | (uint32_t)DP_REGISTER_BANK_REG_4), uiRegMap_PLLPMS);
	}

	return iRetVal;
}


int32_t Dptx_Platform_Free_Handle(struct Dptx_Params *pstDptx_Handle)
{
	uint8_t ucStreamIdx;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	if (pstDptx_Handle == NULL) {
		dptx_err("pstDptx_Handle is NULL");
		iRetVal = DPTX_RETURN_EINVAL;
	} else {
		if (pstDptx_Handle->pucEdidBuf != NULL) {
			kfree(pstDptx_Handle->pucEdidBuf);
			pstDptx_Handle->pucEdidBuf = NULL;
		}

		for (ucStreamIdx = 0; ucStreamIdx < (uint8_t)PHY_INPUT_STREAM_MAX; ucStreamIdx++) {
			if (pstDptx_Handle->paucEdidBuf_Entry[ucStreamIdx] != NULL) {
				/* For Coverity */
				kfree(pstDptx_Handle->paucEdidBuf_Entry[ucStreamIdx]);
			}
		}

		kfree(pstDptx_Handle);
	}

	return iRetVal;
}

void Dptx_Platform_Set_Device_Handle(struct Dptx_Params *pstDptx)
{
	pstHandle = pstDptx;
}

struct Dptx_Params *Dptx_Platform_Get_Device_Handle(void)
{
	return pstHandle;
}


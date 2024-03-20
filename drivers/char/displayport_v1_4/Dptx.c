// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
/*
* Copyright (C) Telechips Inc.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>

#if defined(CONFIG_PM)
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/pm.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include "Dptx_v14.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"
#include "Dptx_drm_dp_addition.h"

static int32_t of_parse_dp_dt(
			struct Dptx_Params *pstDptx,
			struct device_node *pstOfNode,
			uint32_t *puiPeri0_PClk,
			uint32_t *puiPeri1_PClk,
			uint32_t *puiPeri2_PClk,
			uint32_t *puiPeri3_PClk)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiSerDes_Reset_STR, uiVCP_Id;
	uint64_t ulDispX_Peri_Clk;
	struct clk *DISP0_Peri_Clk, *DISP1_Peri_Clk;
	struct clk *DISP2_Peri_Clk, *DISP3_Peri_Clk;

	DISP0_Peri_Clk = of_clk_get_by_name(pstOfNode, "disp0-clk");
	if (DISP0_Peri_Clk == NULL) {
		/* For Coverity */
		dptx_err("Can't find DISP0 Peri clock\n");
	}

	DISP1_Peri_Clk = of_clk_get_by_name(pstOfNode, "disp1-clk");
	if (DISP1_Peri_Clk == NULL) {
		/* For Coverity */
		dptx_err("Can't find DISP1 Peri clock\n");
	}

	DISP2_Peri_Clk = of_clk_get_by_name(pstOfNode, "disp2-clk");
	if (DISP0_Peri_Clk == NULL) {
		/* For Coverity */
		dptx_err("Can't find DISP2 Peri clock\n");
	}

	DISP3_Peri_Clk = of_clk_get_by_name(pstOfNode, "disp3-clk");
	if (DISP3_Peri_Clk == NULL) {
		/* For Coverity */
		dptx_err("Can't find DISP3 Peri clock\n");
	}

	ulDispX_Peri_Clk = clk_get_rate(DISP0_Peri_Clk);
	*puiPeri0_PClk = (uint32_t)((ulDispX_Peri_Clk / 1000U) & 0xFFFFFFFFU);

	ulDispX_Peri_Clk = clk_get_rate(DISP1_Peri_Clk);
	*puiPeri1_PClk = (uint32_t)((ulDispX_Peri_Clk / 1000U) & 0xFFFFFFFFU);

	ulDispX_Peri_Clk = clk_get_rate(DISP2_Peri_Clk);
	*puiPeri2_PClk = (uint32_t)((ulDispX_Peri_Clk / 1000U) & 0xFFFFFFFFU);

	ulDispX_Peri_Clk = clk_get_rate(DISP3_Peri_Clk);
	*puiPeri3_PClk = (uint32_t)((ulDispX_Peri_Clk / 1000U) & 0xFFFFFFFFU);

	iRetVal = of_property_read_u32(pstOfNode, "serdes_reset_str", &uiSerDes_Reset_STR);
	if (iRetVal < 0) {
		dptx_err("Can't get SerDes reset option on STR.. set to 'On' by default");
		uiSerDes_Reset_STR = (u32)1;
	}
	pstDptx->bSerDes_Reset_STR = (bool)uiSerDes_Reset_STR;

	iRetVal = of_property_read_u32(pstOfNode, "vcp_id_1st_sink", &uiVCP_Id);
	if (iRetVal < 0) {
		dptx_err("Can't get 1st Sink VCP Id.. set to '1' by default");
		uiVCP_Id = (u32)1;
	}
	pstDptx->aucVCP_Id[PHY_INPUT_STREAM_0] = (uint8_t)(uiVCP_Id & 0xFFU);

	iRetVal = of_property_read_u32(pstOfNode, "vcp_id_2nd_sink", &uiVCP_Id);
	if (iRetVal < 0) {
		dptx_err("Can't get 2nd Sink VCP Id.. set to '2' by default");
		uiVCP_Id = (u32)2;
	}
	pstDptx->aucVCP_Id[PHY_INPUT_STREAM_1] = (uint8_t)(uiVCP_Id & 0xFFU);

	iRetVal = of_property_read_u32(pstOfNode, "vcp_id_3rd_sink", &uiVCP_Id);
	if (iRetVal < 0) {
		dptx_err("Can't get 3rd Sink VCP Id.. set to '3' by default");
		uiVCP_Id = (u32)3;
	}
	pstDptx->aucVCP_Id[PHY_INPUT_STREAM_2] = (uint8_t)(uiVCP_Id & 0xFFU);

	iRetVal = of_property_read_u32(pstOfNode, "vcp_id_4th_sink", &uiVCP_Id);
	if (iRetVal < 0) {
		dptx_err("Can't get 4th Sink VCP Id.. set to '4' by default");
		uiVCP_Id = (u32)4;
	}
	pstDptx->aucVCP_Id[PHY_INPUT_STREAM_3] = (uint8_t)(uiVCP_Id & 0xFFU);

	return iRetVal;
}

static int32_t Dpv14_Tx_Probe(struct platform_device *pdev)
{
	uint8_t ucHotPlugged;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t auiPeri_Pixel_Clock[PHY_INPUT_STREAM_MAX] = { 0, };
	const void *pvRet;
	struct Dptx_Params *pstDptx;
	const struct resource *pstResource;
	const struct Dptx_Video_Params *pstVideoParams;

	if (pdev->dev.of_node == NULL) {
		dptx_err("Node wasn't found");
		iRetVal = -ENODEV;
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
/* Coverity DR -> Needs to do */
/*
	CID 948298 (#2 of 2): MISRA C-2012 Pointer Type Conversions (MISRA C-2012 Rule 11.5)
	1. misra_c_2012_rule_11_5_violation: The void pointer expression devm_kzalloc(&pdev->dev, 808UL, 3264U) is cast to an object pointer type struct Dptx_Params *.

	CID 824469 (#1 of 2): MISRA C-2012 The Essential Type Model (MISRA C-2012 Rule 10.8)
	1. misra_c_2012_rule_10_8_violation: Cast from 16 bit width expression 0x400U | 0x800U to a wider 32 bit type
*/
		pstDptx = (struct Dptx_Params *)devm_kzalloc(&pdev->dev, sizeof(*pstDptx), GFP_KERNEL);
		if (pstDptx == NULL) {
			dptx_err("Can't alloc Dev memory");
			iRetVal = -ENOMEM;
		}

		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			pvRet = memset(pstDptx, 0, sizeof(struct Dptx_Params));
			if (pvRet == NULL) {
				dptx_err("Can't memset to 0 in Dev memory");
				iRetVal = -ENOMEM;
			}
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		pstDptx->pstParentDev = &pdev->dev;

		pstResource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (pstResource == NULL) {
			dptx_err("[%s:%d]No memory resource\n", __func__, __LINE__);
			iRetVal = -ENOMEM;
		}

		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			pstDptx->pioDPLink_BaseAddr = devm_ioremap(&pdev->dev, pstResource->start, pstResource->end - pstResource->start);
			if (pstDptx->pioDPLink_BaseAddr == NULL) {
				dptx_err("Failed to map memory resource\n");
				iRetVal = -ENOMEM;
			}
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		pstResource = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (pstResource == NULL) {
			dptx_err("[%s:%d]No memory resource\n", __func__, __LINE__);
			iRetVal = -ENOMEM;
		}

		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			pstDptx->pioMIC_SubSystem_BaseAddr = devm_ioremap(&pdev->dev, pstResource->start, pstResource->end - pstResource->start);
			if (pstDptx->pioMIC_SubSystem_BaseAddr == NULL) {
				dptx_err("Failed to map memory resource\n");
				iRetVal = -ENOMEM;
			}
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		pstResource = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		if (pstResource == NULL) {
			dptx_err("[%s:%d]No memory resource\n", __func__, __LINE__);
			iRetVal = -ENOMEM;
		}

		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			pstDptx->pioPMU_BaseAddr = devm_ioremap(&pdev->dev, pstResource->start, pstResource->end - pstResource->start);
			if (pstDptx->pioPMU_BaseAddr == NULL) {
				dptx_err("Failed to map memory resource\n");
				iRetVal = -ENOMEM;
			}
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		pstDptx->iHPD_IRQ = platform_get_irq(pdev, 0);
		if (pstDptx->iHPD_IRQ < 0) {
			dptx_err("No IRQ\n");
			iRetVal = -ENODEV;
		}
	}

#ifdef CONFIG_OF
	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		iRetVal = of_parse_dp_dt(pstDptx, pdev->dev.of_node, 
						&auiPeri_Pixel_Clock[PHY_INPUT_STREAM_0], &auiPeri_Pixel_Clock[PHY_INPUT_STREAM_1],
						&auiPeri_Pixel_Clock[PHY_INPUT_STREAM_2], &auiPeri_Pixel_Clock[PHY_INPUT_STREAM_3]);
	}
#endif

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		platform_set_drvdata(pdev, pstDptx);

		Dptx_Platform_Set_Device_Handle(pstDptx);

		iRetVal = Dptx_Platform_Init_Params(pstDptx, &pdev->dev);
	}
	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		/* For Coverity */
		iRetVal = Dptx_Core_Init_Params(pstDptx);
	}
	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		/* For Coverity */
		iRetVal = Dptx_VidIn_Init_Params(pstDptx, auiPeri_Pixel_Clock);
	}
	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		/* For Coverity */
		iRetVal = Dptx_Api_Init_Params();
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		pstDptx->pvStr_Resume_CallBack = Str_Resume_CallBabck;
		
		iRetVal = Dptx_Intr_Register_HPD_Callback(pstDptx, Hpd_Intr_CallBabck);
	}
	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		/* For Coverity */
		iRetVal = Dptx_Intr_Register_Panel_Callback(pstDptx, Panel_Topology_CallBabck);
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		mutex_init(&pstDptx->Mutex);

		init_waitqueue_head(&pstDptx->WaitQ);

		atomic_set(&pstDptx->Sink_request, 0);
		atomic_set(&pstDptx->HPD_IRQ_State, 0);

		Dptx_Core_Disable_Global_Intr(pstDptx);

		iRetVal = devm_request_threaded_irq(&pdev->dev,
												(uint32_t)pstDptx->iHPD_IRQ,
												Dptx_Intr_IRQ,
												Dptx_Intr_Threaded_IRQ,
												((unsigned long)IRQF_SHARED | (unsigned long)IRQ_LEVEL),
												"Dpv14_Tx",
												pstDptx);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
			/* For Coverity */
			dptx_err("from devm_request_threaded_irq()");
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		iRetVal = Dptx_Intr_Get_HotPlug_Status(pstDptx, &ucHotPlugged);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
			dptx_err("from Dptx_Intr_Get_HotPlug_Status()");
			ucHotPlugged = (uint8_t)HPD_STATUS_UNPLUGGED;
		}

		pstDptx->eLast_HPDStatus = (ucHotPlugged == (uint8_t)HPD_STATUS_PLUGGED) ? HPD_STATUS_PLUGGED : HPD_STATUS_UNPLUGGED;
	}

	if ((iRetVal == DPTX_RETURN_NO_ERROR) &&(ucHotPlugged == (uint8_t)HPD_STATUS_PLUGGED)) {
		iRetVal = Dptx_Intr_Get_Port_Composition(pstDptx, 0);

		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			Dptx_Core_Enable_Global_Intr(pstDptx, (uint32_t)(DPTX_IEN_HPD | DPTX_IEN_HDCP | DPTX_IEN_SDP | DPTX_IEN_TYPE_C));

			iRetVal = Dptx_Ext_Proc_Interface_Init(pstDptx);
		}

#if !defined(CONFIG_DRM_PANEL_MAX968XX)
		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			iRetVal = Touch_Max968XX_update_reg(pstDptx);
			if (iRetVal != DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				dptx_err("failed to update Ser/Des Register for Touch");
			}
		}
#endif
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		pstVideoParams = &pstDptx->stVideoParams;

		dptx_notice("TCC-DPTX-V %d.%d.%d", TCC_DPTX_DRV_MAJOR_VER, TCC_DPTX_DRV_MINOR_VER, TCC_DPTX_DRV_SUBTITLE_VER);
		dptx_notice("	TCC805X %s ", (pstDptx->uiTCC805X_Revision == (uint32_t)TCC805X_REVISION_ES) ? "ES" :
									  (pstDptx->uiTCC805X_Revision == (uint32_t)TCC805X_REVISION_CS) ? "CS" :"BX");
		dptx_notice("	Hot %s ", (pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED) ? "Plugged" : "Unplugged");
		dptx_notice("	%s mode, %d lanes, %s rate", (pstDptx->bMultStreamTransport == (bool)true) ? "MST" : "SST",
						pstDptx->stDptxLink.ucNumOfLanes,
						(pstDptx->ucMax_Rate == (uint8_t)DPTX_PHYIF_CTRL_RATE_RBR) ? "RBR" :
						(pstDptx->ucMax_Rate == (uint8_t)DPTX_PHYIF_CTRL_RATE_HBR) ? "HBR" :
						(pstDptx->ucMax_Rate == (uint8_t)DPTX_PHYIF_CTRL_RATE_HBR2) ? "HB2" : "HBR3");
		dptx_notice("	SerDes reset from STR is %s ", (pstDptx->bSerDes_Reset_STR) ? "On" : "Off");
		dptx_notice("	PHY Lane is %s ", (pstDptx->bPHY_Lane_Reswap) ? "re-swapped" : "not re-swapped");
		dptx_notice("	SDM Bypass is %s ", (pstDptx->bSDM_Bypass) ? "On" : "Off");
		dptx_notice("	SRVC Bypass is %s ", (pstDptx->bSRVC_Bypass) ? "On" : "Off");
		dptx_notice("	VCP Id :  %d %d %d %d", pstDptx->aucVCP_Id[0], pstDptx->aucVCP_Id[1], pstDptx->aucVCP_Id[2], pstDptx->aucVCP_Id[3]);
		dptx_notice("	%d streams enable -> Pixel encoding = %s, VIC = %d %d %d %d, PClk = %d %d %d %d",
					pstDptx->ucNumOfStreams,
					(pstVideoParams->ucPixel_Encoding == (uint8_t)PIXEL_ENCODING_TYPE_RGB) ? "RGB" :
					(pstVideoParams->ucPixel_Encoding == (uint8_t)PIXEL_ENCODING_TYPE_YCBCR422) ? "YCbCr422" : "YCbCr444",
					pstVideoParams->auiVideo_Code[0], pstVideoParams->auiVideo_Code[1],
					pstVideoParams->auiVideo_Code[2], pstVideoParams->auiVideo_Code[3],
					pstVideoParams->uiPeri_Pixel_Clock[0], pstVideoParams->uiPeri_Pixel_Clock[1],
					pstVideoParams->uiPeri_Pixel_Clock[2], pstVideoParams->uiPeri_Pixel_Clock[3]);
	}

	return iRetVal;
}



/* Coverity DR -> Needs to do */
/*
	CID 805652 (#2 of 2): MISRA C-2012 Declarations and Definitions (MISRA C-2012 Rule 8.13)
	misra_c_2012_rule_8_13_violation: The pointer variable pstDev points to a non-constant type but does not modify the object it points to. Consider adding const qualifier to the points-to type.
*/
static int Dpv14_Tx_Remove(struct platform_device *pstDev)
{
	int32_t	iRetVal;
	struct Dptx_Params *pstDptx;

	dptx_dbg("");
	dptx_dbg("****************************************");
	dptx_dbg("Remove: DP V1.4 driver ");
	dptx_dbg("****************************************");
	dptx_dbg("");

/* Coverity DR -> Needs to do */
/*
	CID 805026 (#2 of 2): MISRA C-2012 Pointer Type Conversions (MISRA C-2012 Rule 11.5)
	1. misra_c_2012_rule_11_5_violation: The void pointer expression platform_get_drvdata(pstDev) is cast to an object pointer type struct Dptx_Params *.
*/
	pstDptx = (struct Dptx_Params *)platform_get_drvdata(pstDev);

	iRetVal = Dptx_Core_Deinit(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* For Coverity */
		dptx_err("from Dptx_Core_Deinit()");
	}

	iRetVal = Dptx_Intr_Handle_HotUnplug(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* For Coverity */
		dptx_err("from Dptx_Intr_Handle_HotUnplug()");
	}

	mutex_destroy(&pstDptx->Mutex);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dpv14_Tx_Suspend_T(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal;

	dptx_info("*** PM Suspend Test ***");

	iRetVal = Dptx_Core_Deinit(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* For Coverity */
		dptx_err("from Dptx_Core_Deinit()");
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		iRetVal = Dptx_Intr_Handle_HotUnplug(pstDptx);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			/* For Coverity */
			dptx_err("from Dptx_Intr_Handle_HotUnplug()");
		}
	}

	return iRetVal;
}


int32_t Dpv14_Tx_Resume_T(struct Dptx_Params *pstDptx)
{
	uint8_t ucRetry_MSTAct = 0;
	uint8_t ucHotPlugged = (uint8_t)HPD_STATUS_UNPLUGGED;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	const struct Dptx_Video_Params *pstVideoParams;

	dptx_info("*** PM Resume Test *** ");

	pstVideoParams = &pstDptx->stVideoParams;

	dptx_info("Resume: Hot %s, HPD IRQ %d, SSC = %s, PClk %d %d %d %d\n",
				(pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED) ? "Plugged":"Unplugged",
				pstDptx->iHPD_IRQ,
				pstDptx->bSpreadSpectrum_Clock ? "On":"Off",
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_0],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_1],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_2],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_3]);

	if ((pstDptx->bSerDes_Reset_STR) && (pstDptx->pvStr_Resume_CallBack != NULL)) {
		/* For Coverity */
		iRetVal = pstDptx->pvStr_Resume_CallBack();
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		/* For Coverity */
		iRetVal = Dptx_Platform_Set_PMU_ColdReset_Release(pstDptx);
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		/* For Coverity */
		iRetVal = Dptx_Platform_Set_APAccess_Mode(pstDptx);
	}

	do {
		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			/* For Coverity */
			iRetVal = Dptx_Platform_Set_ClkPath_To_XIN(pstDptx);
		}

		if (pstDptx->uiTCC805X_Revision != (uint32_t)TCC805X_REVISION_ES) {
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				iRetVal = Dptx_Platform_Set_PHY_StandardLane_PinConfig(pstDptx);
			}
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				iRetVal = Dptx_Platform_Set_SDMBypass_Ctrl(pstDptx);
			}
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				iRetVal = Dptx_Platform_Set_SRVCBypass_Ctrl(pstDptx);
			}
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				iRetVal = Dptx_Platform_Set_MuxSelect(pstDptx);
			}
		}

		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			/* For Coverity */
			iRetVal = Dptx_Platform_Init(pstDptx);
		}
		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			/* For Coverity */
			iRetVal = Dptx_Core_Init(pstDptx);
		}
		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			/* For Coverity */
			iRetVal = Dptx_Intr_Get_HotPlug_Status(pstDptx, &ucHotPlugged);
		}

		pstDptx->eLast_HPDStatus = (enum HPD_Detection_Status)ucHotPlugged;

		if ((iRetVal == DPTX_RETURN_NO_ERROR) && (ucHotPlugged == (uint8_t)HPD_STATUS_PLUGGED)) {
			iRetVal = Dptx_Link_Perform_BringUp(pstDptx, pstDptx->bMultStreamTransport);
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				iRetVal = Dptx_Link_Perform_Training(pstDptx, pstDptx->ucMax_Rate, pstDptx->ucMax_Lanes);
			}
		} else {
			/* For Coverity */
			iRetVal = -ENODEV;
		}

		if ((iRetVal == DPTX_RETURN_NO_ERROR) && (pstDptx->bMultStreamTransport)) {
			iRetVal = Dptx_Ext_Set_Topology_Configuration(pstDptx, pstDptx->ucNumOfPorts, pstDptx->bSideBand_MSG_Supported);
			if (iRetVal == DPTX_RETURN_MST_ACT_TIMEOUT) {
				if (ucRetry_MSTAct == 0U) {
					dptx_notice("\n==== Re-initialize DP Link ===\n");

					ucRetry_MSTAct = 1;

					iRetVal = Dptx_Core_Deinit(pstDptx);
					iRetVal = Dptx_Intr_Handle_HotUnplug(pstDptx);

					iRetVal = DPTX_RETURN_NO_ERROR;
				} else {
					dptx_err("MST ACT timeout...");
					ucRetry_MSTAct = 0;
				}
			}
		}
	} while (ucRetry_MSTAct == 1U);

	return iRetVal;
}

#if defined(CONFIG_PM)
/* Coverity DR -> Needs to do */
/*
	CID 857066 (#2 of 2): MISRA C-2012 Declarations and Definitions (MISRA C-2012 Rule 8.13)
	misra_c_2012_rule_8_13_violation: The pointer variable pdev points to a non-constant type but does not modify the object it points to. Consider adding const qualifier to the points-to type
*/
static int Dpv14_Tx_Suspend(struct platform_device *pdev, pm_message_t state)
{
	int32_t iRetVal;
	int32_t iEvt;
	struct Dptx_Params *pstDptx;

	dptx_dbg("");
	dptx_dbg("****************************************");
	dptx_dbg("PM Suspend: DP V1.4 driver ");
	dptx_dbg("****************************************");
	dptx_dbg("");

	iEvt = state.event;

/* Coverity DR -> Needs to do */
/*
	CID 711212 (#2 of 2): MISRA C-2012 Pointer Type Conversions (MISRA C-2012 Rule 11.5)
	1. misra_c_2012_rule_11_5_violation: The void pointer expression platform_get_drvdata(pdev) is cast to an object pointer type struct Dptx_Params *.
*/
	pstDptx = (struct Dptx_Params *)platform_get_drvdata(pdev);

	iRetVal = Dptx_Core_Deinit(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* For Coverity */
		dptx_err("from Dptx_Core_Deinit()");
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		iRetVal = Dptx_Intr_Handle_HotUnplug(pstDptx);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			/* For Coverity */
			dptx_err("from Dptx_Intr_Handle_HotUnplug()");
		}
	}

	dptx_notice("PM Suspend: event as %d", iEvt);

	return iRetVal;
}

static int Dpv14_Tx_Resume(struct platform_device *pdev)
{
	uint8_t ucRetry_MSTAct = 0;
	uint8_t ucHotPlugged = (uint8_t)HPD_STATUS_UNPLUGGED;
	int32_t iPinRet, iRetVal = DPTX_RETURN_NO_ERROR;
	struct Dptx_Params *pstDptx;
	const struct Dptx_Video_Params *pstVideoParams;

	dptx_dbg("");
	dptx_dbg("****************************************");
	dptx_dbg("PM Resume: DP V1.4 driver ");
	dptx_dbg("****************************************");
	dptx_dbg("");

/* Coverity DR -> Needs to do */
/*
	CID 711212 (#2 of 2): MISRA C-2012 Pointer Type Conversions (MISRA C-2012 Rule 11.5)
	1. misra_c_2012_rule_11_5_violation: The void pointer expression platform_get_drvdata(pdev) is cast to an object pointer type struct Dptx_Params *.
*/
	pstDptx = (struct Dptx_Params *)platform_get_drvdata(pdev);

	pstVideoParams = &pstDptx->stVideoParams;

	iPinRet = pinctrl_pm_select_default_state(&pdev->dev);
	if (iPinRet != 0) {
		/* For Coverity */
		dptx_err("from pinctrl_pm_select_default_state()");
	}

	dptx_info("Resume: Hot %s, HPD IRQ %d, SSC = %s, PClk %d %d %d %d\n",
				(pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED) ? "Plugged":"Unplugged",
				pstDptx->iHPD_IRQ,
				pstDptx->bSpreadSpectrum_Clock ? "On":"Off",
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_0],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_1],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_2],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_3]);

	if ((pstDptx->bSerDes_Reset_STR) && (pstDptx->pvStr_Resume_CallBack != NULL)) {
		/* For Coverity */
		iRetVal = pstDptx->pvStr_Resume_CallBack();
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		/* For Coverity */
		iRetVal = Dptx_Platform_Set_PMU_ColdReset_Release(pstDptx);
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		/* For Coverity */
		iRetVal = Dptx_Platform_Set_APAccess_Mode(pstDptx);
	}

	do {
		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			/* For Coverity */
			iRetVal = Dptx_Platform_Set_ClkPath_To_XIN(pstDptx);
		}

		if (pstDptx->uiTCC805X_Revision != (uint32_t)TCC805X_REVISION_ES) {
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				iRetVal = Dptx_Platform_Set_PHY_StandardLane_PinConfig(pstDptx);
			}
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				iRetVal = Dptx_Platform_Set_SDMBypass_Ctrl(pstDptx);
			}
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				iRetVal = Dptx_Platform_Set_SRVCBypass_Ctrl(pstDptx);
			}
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				iRetVal = Dptx_Platform_Set_MuxSelect(pstDptx);
			}
		}

		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			/* For Coverity */
			iRetVal = Dptx_Platform_Init(pstDptx);
		}
		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			/* For Coverity */
			iRetVal = Dptx_Core_Init(pstDptx);
		}
		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			/* For Coverity */
			iRetVal = Dptx_Intr_Get_HotPlug_Status(pstDptx, &ucHotPlugged);
		}

		pstDptx->eLast_HPDStatus = (enum HPD_Detection_Status)ucHotPlugged;

		if ((iRetVal == DPTX_RETURN_NO_ERROR) && (ucHotPlugged == (uint8_t)HPD_STATUS_PLUGGED)) {
			iRetVal = Dptx_Link_Perform_BringUp(pstDptx, pstDptx->bMultStreamTransport);
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				iRetVal = Dptx_Link_Perform_Training(pstDptx, pstDptx->ucMax_Rate, pstDptx->ucMax_Lanes);
			}
		} else {
			/* For Coverity */
			iRetVal = -ENODEV;
		}

		if ((iRetVal == DPTX_RETURN_NO_ERROR) && (pstDptx->bMultStreamTransport)) {
			iRetVal = Dptx_Ext_Set_Topology_Configuration(pstDptx, pstDptx->ucNumOfPorts, pstDptx->bSideBand_MSG_Supported);
			if (iRetVal == DPTX_RETURN_MST_ACT_TIMEOUT) {
				if (ucRetry_MSTAct == 0U) {
					dptx_notice("\n==== Re-initialize DP Link ===\n");

					ucRetry_MSTAct = 1;

					iRetVal = Dptx_Core_Deinit(pstDptx);
					iRetVal = Dptx_Intr_Handle_HotUnplug(pstDptx);

					iRetVal = DPTX_RETURN_NO_ERROR;
				} else {
					dptx_err("MST ACT timeout...");
					ucRetry_MSTAct = 0;
				}
			}
		}
	} while (ucRetry_MSTAct == 1U);

	return iRetVal;
}

#endif


static const struct of_device_id dpv14_tx[] = {
		{ .compatible =	"telechips,dpv14-tx" },
		{ }
};

static struct platform_driver __refdata stDpv14_Tx_pdrv = {
		.probe = Dpv14_Tx_Probe,
		.remove = Dpv14_Tx_Remove,
		.driver = {
				.name = "telechips,dpv14-tx",
				.owner = THIS_MODULE,

#if defined(CONFIG_OF)
				.of_match_table = dpv14_tx,
#endif
		},

#if defined(CONFIG_PM)
		.suspend = Dpv14_Tx_Suspend,
		.resume = Dpv14_Tx_Resume,
#endif

};

static __init int Dpv14_Tx_init(void)
{
	return platform_driver_register(&stDpv14_Tx_pdrv);
}
module_init(Dpv14_Tx_init);

static __exit void Dpv14_Tx_exit(void)
{
	return platform_driver_unregister(&stDpv14_Tx_pdrv);
}
module_exit(Dpv14_Tx_exit);



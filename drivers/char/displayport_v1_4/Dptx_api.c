// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
/*
* Copyright (C) Telechips Inc.
*/

#include <linux/delay.h>
#include <linux/types.h>

#include <drm/drm_encoder.h>

#include "Dptx_api.h"
#include "Dptx_v14.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"
#include "Dptx_drm_dp_addition.h"

#if defined(CONFIG_DRM_TCC)
#include "dptx_drm.h"
#include <tcc_drm_dpi.h>
#endif

#if defined(CONFIG_DRM_PANEL_MAX968XX)
#include <panel-tcc-dpv14.h>
#endif

#define MAX_CHECK_HPD_NUM					200

#define REG_PRINT_BUF_SIZE					1024
#define VIDEO_REG_DUMP_START_OFFSET			0x300
#define VIDEO_REG_DUMP_SIZE					(0x34 / 4)

#define VCP_REG_DUMP_START_OFFSET			0x200
#define VCP_REG_DUMP_SIZE					(0x30 / 4)

#define VCP_DPCD_DUMP_SIZE					64

#if defined(CONFIG_DRM_TCC)
int dpv14_api_get_hpd_state(int dp_id, unsigned char *hpd_state);
int dpv14_api_get_edid(int dp_id, unsigned char *edid_buf, unsigned int buf_length);
int dpv14_api_set_video_timing(int dp_id, const struct dptx_detailed_timing_t *dptx_detailed_timing);
int dpv14_api_set_video_stream_enable(int dp_id, unsigned char enable);
int dpv14_api_set_audio_stream_enable(int dp_id, unsigned char enable);

static struct dptx_drm_helper_funcs dptx_drm_ops = {
	.get_hpd_state = dpv14_api_get_hpd_state,
	.get_edid = dpv14_api_get_edid,
	.set_video = dpv14_api_set_video_timing,
	.set_enable_video = dpv14_api_set_video_stream_enable,
	.set_enable_audio = dpv14_api_set_audio_stream_enable,
};

struct Dptx_drm_context_t {
	bool				bRegistered;
	struct drm_encoder *pstDrm_encoder;
	struct tcc_drm_dp_callback_funcs sttcc_drm_dp_callbacks;
};


static struct Dptx_drm_context_t stDptx_drm_context[PHY_INPUT_STREAM_MAX];


static int dpv14_api_attach_drm(u8 ucDP_Index)
{
	int32_t iRetVal = 0;
	const struct Dptx_drm_context_t *pstDptx_drm_context;
	struct drm_encoder *pstDrm_encoder;

	pstDptx_drm_context = &stDptx_drm_context[ucDP_Index];

	if (!pstDptx_drm_context->bRegistered) {
		dptx_err("Port %d is not registered", ucDP_Index);
		iRetVal = -ENOMEM;
	} else {
		if (pstDptx_drm_context->sttcc_drm_dp_callbacks.attach == NULL) {
			dptx_err("DP %d attach callback is not registered", ucDP_Index);
			iRetVal =  -ENOMEM;
		}
	}

	if (iRetVal == 0) {
		dptx_info("Calling attach() with DP %d to DRM", ucDP_Index);

		pstDrm_encoder = pstDptx_drm_context->pstDrm_encoder;

		iRetVal = pstDptx_drm_context->sttcc_drm_dp_callbacks.attach(pstDrm_encoder, (int)ucDP_Index, 0);
	}

	return iRetVal;
}

static int dpv14_api_detach_drm(u8 ucDP_Index)
{
	int32_t iRetVal = 0;
	const struct Dptx_drm_context_t *pstDptx_drm_context;
	struct drm_encoder *pstDrm_encoder;

	pstDptx_drm_context = &stDptx_drm_context[ucDP_Index];

	if (!pstDptx_drm_context->bRegistered) {
		dptx_err("Port %d is not registered", ucDP_Index);
		iRetVal = -ENOMEM;
	} else {
		if (pstDptx_drm_context->sttcc_drm_dp_callbacks.detach == NULL) {
			dptx_err("DP %d detach callback is not registered", ucDP_Index);
			iRetVal = -ENOMEM;
		}
	}

	if (iRetVal == 0) {
		dptx_info("Calling dettach() with DP %d to DRM", ucDP_Index);

		pstDrm_encoder = pstDptx_drm_context->pstDrm_encoder;

		iRetVal = pstDptx_drm_context->sttcc_drm_dp_callbacks.detach(pstDrm_encoder, (int)ucDP_Index, 0);
	}

	return iRetVal;
}

int tcc_dp_register_drm(struct drm_encoder *encoder, const struct tcc_drm_dp_callback_funcs *callbacks)
{
	int32_t iRetVal = 0;
	int32_t  iDP_Index;
	struct Dptx_drm_context_t *pstDptx_drm_context;

	for (iDP_Index = 0; iDP_Index < (int32_t)PHY_INPUT_STREAM_MAX; iDP_Index++) {
		pstDptx_drm_context = &stDptx_drm_context[iDP_Index];

		if (!pstDptx_drm_context->bRegistered) {
			pstDptx_drm_context->bRegistered = (bool)true;
			break;
		}
	}

	if (iDP_Index == (int32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("Port index is reached to limit %d\n", iDP_Index);
		iRetVal =  -ENODEV;
	} else {
		if (encoder == NULL) {
			dptx_err("drm encoder ptr is NULL");
			iRetVal = -ENODEV;
		}
	}

	if (iRetVal == 0) {
		pstDptx_drm_context->pstDrm_encoder = encoder;
		pstDptx_drm_context->sttcc_drm_dp_callbacks.attach = callbacks->attach;
		pstDptx_drm_context->sttcc_drm_dp_callbacks.detach = callbacks->detach;

		iRetVal = callbacks->register_helper_funcs(pstDptx_drm_context->pstDrm_encoder, &dptx_drm_ops);

		if (iRetVal == 0) {
			/* For Coverity */
			dptx_notice("DP %d is registered to DRM", iDP_Index);
		} else {
			dptx_notice("DP %d is failed to register to DRM", iDP_Index);
			iDP_Index = 0;
		}
	} else {
		/* For Coverity */
		iDP_Index = 0;
	}

	return iDP_Index;
}
EXPORT_SYMBOL(tcc_dp_register_drm);

int tcc_dp_unregister_drm(void)
{
	uint8_t ucStream_Max;
	const void *pvRet;

	ucStream_Max = (uint8_t)PHY_INPUT_STREAM_MAX;

	pvRet = memset(&stDptx_drm_context[PHY_INPUT_STREAM_0], 0, sizeof(struct Dptx_drm_context_t) * ucStream_Max);
	if (pvRet == NULL) {
		/* For Coverity */
		dptx_err("Can't memset to 0 in Drm structure");
	}

	return 0;
}
EXPORT_SYMBOL(tcc_dp_unregister_drm);


int dpv14_api_get_hpd_state(int dp_id, unsigned char *hpd_state)
{
	uint8_t	ucHPD_State;
	int32_t iRetVal = 0;
	struct Dptx_Params *pstHdl;

	if ((dp_id >= (int)PHY_INPUT_STREAM_MAX) || (dp_id < (int)PHY_INPUT_STREAM_0)) {
		dptx_err("Invalid dp id as %d", dp_id);
		iRetVal = -ENODEV;
	} else {
		if (hpd_state == NULL) {
			dptx_err("drm hpd buffer ptr is NULL");
			iRetVal = -ENODEV;
		} else {
			pstHdl = Dptx_Platform_Get_Device_Handle();
			if (pstHdl == NULL) {
				dptx_err("Failed to get handle");
				iRetVal = -ENODEV;
			}
		}
	}

	if (iRetVal == 0) {
		iRetVal = Dptx_Intr_Get_HotPlug_Status(pstHdl, &ucHPD_State);

		if (iRetVal == 0) {
			if (ucHPD_State == (uint8_t)HPD_STATUS_PLUGGED) {
				if (dp_id >= (int32_t)pstHdl->ucNumOfPorts) {
					dptx_info("DP index %d is larger than the number of ports -> set to unplugged", dp_id);
					*hpd_state = (unsigned char)0;
				} else {
					dptx_info("DP %d is plugged", dp_id);
					*hpd_state = (unsigned char)1;
				}
			} else {
				dptx_info("DP %d is not plugged", dp_id);
				*hpd_state = (unsigned char)0;
			}
		} else {
			dptx_err("Error from Dptx_Intr_Get_HotPlug_Status() -> set to unplugged");
			*hpd_state = (unsigned char)0;
		}
	}

	return iRetVal;
}

int dpv14_api_get_edid(int dp_id, unsigned char *edid_buf, unsigned int buf_length)
{
	u8 *pucEDID_Buf;
	int32_t iRetVal = 0;
	size_t szWrite_Len;
	const void *pvRet;
	const struct Dptx_Params *pstHdl;

	if ((dp_id >= (int)PHY_INPUT_STREAM_MAX) || (dp_id < (int)PHY_INPUT_STREAM_0)) {
		dptx_err("Invalid dp id as %d", dp_id);
		iRetVal = -ENODEV;
	} else {
		if (edid_buf == NULL) {
			dptx_err("drm edid buffer ptr is NULL");
			iRetVal = -ENODEV;
		} else {
			pstHdl = Dptx_Platform_Get_Device_Handle();
			if (pstHdl == NULL) {
				dptx_err("Failed to get handle");
				iRetVal = -ENODEV;
			}
		}
	}

	if (iRetVal == 0) {
		pucEDID_Buf = pstHdl->paucEdidBuf_Entry[dp_id];
		if (pucEDID_Buf == NULL) {
			dptx_err("DP %d EDID buffer is not available", dp_id);

			szWrite_Len = (size_t)buf_length;

			pvRet = memset(edid_buf, 0, szWrite_Len);
			if (pvRet == NULL) {
				dptx_err("Can't memset to 0 in Edid buf");
			}
			iRetVal = -ENODEV;
		}
	}

	if (iRetVal == 0) {
		iRetVal = Dptx_Edid_Verify_BaseBlk_Data(pucEDID_Buf);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			dptx_err("DP %d EDID data is not valid", dp_id);

			szWrite_Len = (size_t)buf_length;

			pvRet = memset(edid_buf, 0, szWrite_Len);
			if (pvRet == NULL) {
				dptx_err("Can't memset to 0 in Edid buf");
			}
		}
	}

	if (iRetVal == 0) {
		if (buf_length < (uint32_t)DPTX_EDID_BUFLEN) {
			/* For Coverity */
			szWrite_Len = (size_t)buf_length;
		} else {
			/* For Coverity */
			szWrite_Len = (size_t)DPTX_EDID_BUFLEN;
		}

		pvRet = memcpy(edid_buf, pucEDID_Buf, szWrite_Len);
		if (pvRet == NULL) {
			dptx_err("Can't copy edid data to buff.. len(%d)", (uint32_t)szWrite_Len);
			iRetVal = -ENODEV;
		}
	}

	return iRetVal;
}

/*
	CID 1522478 (#1 of 1): MISRA C-2012 Declarations and Definitions (MISRA C-2012 Rule 8.8)
	1. misra_c_2012_rule_8_8_violation: missing static storage modifier for "dpv14_api_set_video_timing" which has internal linkage
*/
int dpv14_api_set_video_timing(int dp_id, const struct dptx_detailed_timing_t *dptx_detailed_timing)
{
	bool bVStream_Enabled = (bool)false;
	bool bSkip_SetTiming = (bool)false;
	int32_t iRetVal = 0;
	struct Dptx_Params *pstHdl;
	struct Dptx_Dtd_Params stDtd_Params;
	struct Dptx_Dtd_Params stDtd_Params_Configured;

	if ((dp_id >= (int)PHY_INPUT_STREAM_MAX) || (dp_id < (int)PHY_INPUT_STREAM_0)) {
		dptx_err("Invalid dp id as %d", dp_id);
		iRetVal = -ENODEV;
	} else {
		if (dptx_detailed_timing == NULL) {
			dptx_err("drm timing buffer ptr is NULL");
			iRetVal = -ENODEV;
		} else {
			pstHdl = Dptx_Platform_Get_Device_Handle();
			if (pstHdl == NULL) {
				dptx_err("Failed to get handle");
				iRetVal = -ENODEV;
			}
		}
	}

	if (iRetVal == 0) {
		stDtd_Params.pixel_repetition_input = (uint16_t)dptx_detailed_timing->pixel_repetition_input;
		stDtd_Params.interlaced = dptx_detailed_timing->interlaced;
		stDtd_Params.h_active = (uint16_t)(dptx_detailed_timing->h_active & 0xFFFFU);
		stDtd_Params.h_blanking = (uint16_t)(dptx_detailed_timing->h_blanking & 0xFFFFU);
		stDtd_Params.h_sync_offset = (uint16_t)(dptx_detailed_timing->h_sync_offset & 0xFFFFU);
		stDtd_Params.h_sync_pulse_width = (uint16_t)(dptx_detailed_timing->h_sync_pulse_width & 0xFFFFU);
		stDtd_Params.h_sync_polarity = (uint8_t)(dptx_detailed_timing->h_sync_polarity & 0xFFU);
		stDtd_Params.v_active = (uint16_t)(dptx_detailed_timing->v_active & 0xFFFFU);
		stDtd_Params.v_blanking = (uint16_t)(dptx_detailed_timing->v_blanking & 0xFFFFU);
		stDtd_Params.v_sync_offset = (uint16_t)(dptx_detailed_timing->v_sync_offset & 0xFFFFU);
		stDtd_Params.v_sync_pulse_width = (uint16_t)(dptx_detailed_timing->v_sync_pulse_width & 0xFFFFU);
		stDtd_Params.v_sync_polarity = (uint8_t)(dptx_detailed_timing->v_sync_polarity & 0xFFU);
		stDtd_Params.uiPixel_Clock = dptx_detailed_timing->pixel_clock;
		stDtd_Params.h_image_size = 16;
		stDtd_Params.v_image_size = 9;

		iRetVal = Dptx_VidIn_Get_Stream_Enable(pstHdl, &bVStream_Enabled, (u8)dp_id);
	}

	if (iRetVal == 0) {
		if (bVStream_Enabled == (bool)true) {
			iRetVal = Dptx_VidIn_Get_Configured_Timing(pstHdl, (u8)dp_id, &stDtd_Params_Configured);

			if ((stDtd_Params.interlaced == stDtd_Params_Configured.interlaced) &&
				(stDtd_Params.h_sync_polarity == stDtd_Params_Configured.h_sync_polarity) &&
				(stDtd_Params.h_active == stDtd_Params_Configured.h_active) &&
				(stDtd_Params.h_blanking == stDtd_Params_Configured.h_blanking) &&
				(stDtd_Params.h_sync_offset == stDtd_Params_Configured.h_sync_offset) &&
				(stDtd_Params.h_sync_pulse_width == stDtd_Params_Configured.h_sync_pulse_width) &&
				(stDtd_Params.v_sync_polarity == stDtd_Params_Configured.v_sync_polarity) &&
				(stDtd_Params.v_active == stDtd_Params_Configured.v_active) &&
				(stDtd_Params.v_blanking == stDtd_Params_Configured.v_blanking) &&
				(stDtd_Params.v_sync_offset == stDtd_Params_Configured.v_sync_offset) &&
				(stDtd_Params.v_sync_pulse_width == stDtd_Params_Configured.v_sync_pulse_width)) {
					dptx_notice("[Detailed timing from DRM] : ");
					dptx_notice("Video timing of DP %d was already configured --> Skip", dp_id);
					dptx_notice("		Pixel clk = %d ", (u32)dptx_detailed_timing->pixel_clock);
					dptx_notice("		%s", (dptx_detailed_timing->interlaced == 0U) ? "Progressive" : "Interlace");
					dptx_notice("		H Active(%d), V Active(%d)", (u32)dptx_detailed_timing->h_active, (u32)dptx_detailed_timing->v_active);
					dptx_notice("		H Blanking(%d), V Blanking(%d)", (u32)dptx_detailed_timing->h_blanking, (u32)dptx_detailed_timing->v_blanking);
					dptx_notice("		H Sync offset(%d), V Sync offset(%d) ", (u32)dptx_detailed_timing->h_sync_offset, (u32)dptx_detailed_timing->v_sync_offset);
					dptx_notice("		H Sync plus W(%d), V Sync plus W(%d) ", (u32)dptx_detailed_timing->h_sync_pulse_width, (u32)dptx_detailed_timing->v_sync_pulse_width);
					dptx_notice("		H Sync Polarity(%d), V Sync Polarity(%d)", (u32)dptx_detailed_timing->h_sync_polarity, (u32)dptx_detailed_timing->v_sync_polarity);

					bSkip_SetTiming = (bool)true;
			}
		}
	}

	if (iRetVal == 0) {
		if (bSkip_SetTiming == (bool)false) {
			iRetVal = Dptx_VidIn_Set_Detailed_Timing(pstHdl, (u8)dp_id, &stDtd_Params);

			dptx_notice("[Detailed timing from DRM] : Video timing of DP %d is being aconfigured", dp_id);
			dptx_notice("		Pixel clk = %d ", (u32)dptx_detailed_timing->pixel_clock);
			dptx_notice("		%s", (dptx_detailed_timing->interlaced == 0U) ? "Progressive" : "Interlace");
			dptx_notice("		H Active(%d), V Active(%d)", (u32)dptx_detailed_timing->h_active, (u32)dptx_detailed_timing->v_active);
			dptx_notice("		H Blanking(%d), V Blanking(%d)", (u32)dptx_detailed_timing->h_blanking, (u32)dptx_detailed_timing->v_blanking);
			dptx_notice("		H Sync offset(%d), V Sync offset(%d) ", (u32)dptx_detailed_timing->h_sync_offset, (u32)dptx_detailed_timing->v_sync_offset);
			dptx_notice("		H Sync plus W(%d), V Sync plus W(%d) ", (u32)dptx_detailed_timing->h_sync_pulse_width,(u32)dptx_detailed_timing->v_sync_pulse_width);
			dptx_notice("		H Sync Polarity(%d), V Sync Polarity(%d)", (u32)dptx_detailed_timing->h_sync_polarity, (u32)dptx_detailed_timing->v_sync_polarity);
		}
	}

	return iRetVal;
}

/*
	CID 1522484 (#1 of 1): MISRA C-2012 Declarations and Definitions (MISRA C-2012 Rule 8.8)
	1. misra_c_2012_rule_8_8_violation: missing static storage modifier for "dpv14_api_set_video_stream_enable" which has internal linkage
*/
int dpv14_api_set_video_stream_enable(int dp_id, unsigned char enable)
{
	int32_t iRetVal = 0;
	struct Dptx_Params *pstHdl;

	if ((dp_id >= (int)PHY_INPUT_STREAM_MAX) || (dp_id < (int)PHY_INPUT_STREAM_0)) {
		dptx_err("Invalid dp id as %d", dp_id);
		iRetVal = -ENODEV;
	} else {
		pstHdl = Dptx_Platform_Get_Device_Handle();
		if (pstHdl == NULL) {
			dptx_err("Failed to get handle");
			iRetVal = -ENODEV;
		}
	}

	if (iRetVal == 0) {
		iRetVal = Dptx_VidIn_Set_Stream_Enable(pstHdl, (bool)enable, (u8)dp_id);
		dptx_info("Set DP %d video %s...", dp_id, (enable == 0U) ? "disable":"enable");
	}

	return iRetVal;
}

/*
	CID 1522480 (#1 of 1): MISRA C-2012 Declarations and Definitions (MISRA C-2012 Rule 8.8)
	1. misra_c_2012_rule_8_8_violation: missing static storage modifier for "dpv14_api_set_audio_stream_enable" which has internal linkage
*/
int dpv14_api_set_audio_stream_enable(int dp_id, unsigned char enable)
{
	int32_t iRetVal = 0;
	struct Dptx_Params *pstHdl;

	if ((dp_id >= (int)PHY_INPUT_STREAM_MAX) || (dp_id < (int)PHY_INPUT_STREAM_0)) {
		dptx_err("Invalid dp id as %d", dp_id);
		iRetVal = -ENODEV;
	} else {
		pstHdl = Dptx_Platform_Get_Device_Handle();
		if (pstHdl == NULL) {
			dptx_err("Failed to get handle");
			iRetVal = -ENODEV;
		}
	}

	if (iRetVal == 0) {
		Dptx_AudIn_Set_Mute(pstHdl, (u8)dp_id, enable);
		dptx_info("Set DP %d audio %s...", dp_id, (enable == 0U) ? "disable":"enable");
	}

	return iRetVal;
}

int32_t tcc_dp_identify_lcd_mux_configuration(uint32_t dp_id, uint8_t lcd_mux_index)
{
	int32_t iRetVal = 0;
	struct Dptx_Params *pstHdl;

	if (dp_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("Invalid dp id as %d", dp_id);
		iRetVal = -EINVAL;
	} else {
		pstHdl = Dptx_Platform_Get_Device_Handle();
		if (pstHdl == NULL) {
			dptx_err("Failed to get handle");
			iRetVal = -ENODEV;
		}
	}

	if (iRetVal == 0) {
		iRetVal = Dptx_Platform_Get_MuxSelect(pstHdl);
		if ((iRetVal == 0) && (lcd_mux_index != pstHdl->aucMuxInput_Index[dp_id])) {
			dptx_err("The mux index(%d) of DRM is NOT matched with mux index(%d) of DP%d configured in u-boot",
						lcd_mux_index,
						pstHdl->aucMuxInput_Index[dp_id],
						dp_id);
		}
	}

	return iRetVal;
}
#endif

#if defined(CONFIG_DRM_PANEL_MAX968XX)
int dpv14_set_num_of_panels(unsigned char num_of_panels)
{
	bool bMST_Mode;
	int iRetVal = 0;
	struct Dptx_Params *pstHdl;

	if (num_of_panels > (unsigned char)PHY_INPUT_STREAM_MAX) {
		dptx_err("Invalid num_of_panels as %d", num_of_panels);
		iRetVal = -EINVAL;
	} else {
		pstHdl = Dptx_Platform_Get_Device_Handle();
		if (pstHdl == NULL) {
			dptx_err("Failed to get handle");
			iRetVal = -ENXIO;
		}
	}

	if ((iRetVal == 0) && (pstHdl->bSideBand_MSG_Supported == (bool)false)) {
		bMST_Mode = (num_of_panels > 1U) ? (bool)true : (bool)false;

		iRetVal = Dptx_Ext_Set_Stream_Mode(pstHdl, bMST_Mode, num_of_panels);

		dptx_info("%s mode, Num of panels: %d", (bMST_Mode == (bool)true) ? "MST" : "SST", num_of_panels);
	}

	return iRetVal;
}
#endif

void Hpd_Intr_CallBabck(u8 ucDP_Index, bool bHPD_State)
{
#if defined(CONFIG_DRM_TCC)
	int iRetVal = 0;
#endif

	dptx_info("Callback called with DP %d, HPD %s", ucDP_Index, bHPD_State ? "Plugged" : "Unplugged");

#if defined(CONFIG_DRM_TCC)
	if (bHPD_State == (bool)HPD_STATUS_PLUGGED) {
		iRetVal = dpv14_api_attach_drm(ucDP_Index);
		if (iRetVal != 0) {
			/* For Coverity */
			dptx_err("Error from dpv14_api_attach_drm()");
		}
	} else {
		iRetVal = dpv14_api_detach_drm(ucDP_Index);
		if (iRetVal != 0) {
			/* For Coverity */
			dptx_err("Error from dpv14_api_detach_drm()");
		}
	}
#endif
}

int Str_Resume_CallBabck(void)
{
	int iRetVal = 0;

#if !defined(CONFIG_DRM_PANEL_MAX968XX)
	const struct Dptx_Params *pstHdl;
#endif

	dptx_info("Str Resume Callback is called");

#if defined(CONFIG_DRM_PANEL_MAX968XX)
	iRetVal = panel_max968xx_reset();
	if (iRetVal != 0) {
		/* For Coverity */
		dptx_info("Error from panel_max968xx_reset()");
	}
#else
	pstHdl = Dptx_Platform_Get_Device_Handle();
	if (pstHdl == NULL) {
		dptx_err("Failed to get handle");
		iRetVal = -ENXIO;
	} else {
		/* For Coverity */
		iRetVal = Dptx_Max968XX_Reset(pstHdl);
	}
#endif

	return iRetVal;
}

int Panel_Topology_CallBabck(uint8_t *pucNumOfPorts)
{
	int iRetVal = 0;

	if (pucNumOfPorts == NULL) {
		dptx_err("pucNumOfPorts == NULL");
		iRetVal = -ENXIO;
	} else {
#if defined(CONFIG_DRM_PANEL_MAX968XX)
		iRetVal = panel_max968xx_get_topology(pucNumOfPorts);
		if (iRetVal != 0) {
			/* For Coverity */
			dptx_err("Error from panel_max968xx_get_topology()");
		}
#else
		iRetVal = Dptx_Max968XX_Get_TopologyState(pucNumOfPorts);
		if (iRetVal != 0) {
			/* For Coverity */
			dptx_err("Error from Dptx_Max968XX_Get_TopologyState()");
		}
#endif
	}

	return iRetVal;
}

int32_t Dptx_Api_Init_Params(void)
{
	int32_t iRetVal = 0;

#if defined(CONFIG_DRM_TCC)
	uint8_t ucStream_Max = 0;
	const void *pvRet;

	ucStream_Max = (uint8_t)PHY_INPUT_STREAM_MAX;

	pvRet = memset(&stDptx_drm_context[PHY_INPUT_STREAM_0], 0, sizeof(struct Dptx_drm_context_t) * ucStream_Max);
	if (pvRet == NULL) {
		dptx_err("Can't memset to 0 in Drm memory");
		iRetVal = -ENOMEM;
	}
#endif

	return iRetVal;
}

int32_t Dpv14_Tx_API_Set_Audio_Configuration(uint8_t ucDP_Index, const struct DPTX_API_Audio_Params *pstDptx_audio_params)
{
	int32_t iRetVal = 0;
	struct Dptx_Params *pstHdl;
	struct Dptx_Audio_Params stAudioParams;

	pstHdl = Dptx_Platform_Get_Device_Handle();
	if (pstHdl == NULL) {
		dptx_err("Failed to get handle");
		iRetVal = -ENXIO;
	} else {
		if (pstDptx_audio_params == NULL) {
			dptx_err("pstDptx_audio_params == NULL");
			iRetVal = -ENXIO;
		}
	}

	if(iRetVal == 0) {
		stAudioParams.ucInput_InterfaceType     = (uint8_t)pstDptx_audio_params->eIn_InterfaceType;
		stAudioParams.ucInput_DataWidth         = (uint8_t)pstDptx_audio_params->eIn_DataWidth;
		stAudioParams.ucInput_Max_NumOfchannels = (uint8_t)pstDptx_audio_params->eIn_Max_NumOfchannels;
		stAudioParams.ucInput_HBR_Mode          = (uint8_t)pstDptx_audio_params->eIn_HBR_Mode;
		stAudioParams.ucIEC_Sampling_Freq       = (uint8_t)pstDptx_audio_params->eIEC_Sampling_Freq;
		stAudioParams.ucIEC_OriginSamplingFreq  = (uint8_t)pstDptx_audio_params->eIEC_OrgSamplingFreq;
		stAudioParams.ucInput_TimestampVersion  = 0x12;

		Dptx_AudIn_Configure(pstHdl, ucDP_Index, &stAudioParams);
	}

	return iRetVal;
}

int32_t Dpv14_Tx_API_Get_Audio_Configuration(uint8_t ucDP_Index, struct DPTX_API_Audio_Params *pstDptx_audio_params)
{
	uint8_t ucInfType, ucNumOfCh, ucDataWidth;
	uint8_t ucHBRMode, ucSamplingFreq, ucOrgSamplingFreq;
	int32_t iRetVal = 0;
	struct Dptx_Params *pstHdl;

	pstHdl = Dptx_Platform_Get_Device_Handle();
	if (pstHdl == NULL) {
		dptx_err("Failed to get handle");
		iRetVal = -ENXIO;
	} else {
		if (pstDptx_audio_params == NULL) {
			dptx_err("pstDptx_audio_params == NULL");
			iRetVal = -ENXIO;
		}
	}

	if(iRetVal == 0) {
		Dptx_AudIn_Get_Input_InterfaceType(pstHdl, ucDP_Index, &ucInfType);
		Dptx_AudIn_Get_DataWidth(pstHdl, ucDP_Index, &ucDataWidth);
		Dptx_AudIn_Get_MaxNumOfChannels(pstHdl, ucDP_Index, &ucNumOfCh);
		Dptx_AudIn_Get_HBR_En(pstHdl, ucDP_Index, &ucHBRMode);
		Dptx_AudIn_Get_SamplingFreq( pstHdl, &ucSamplingFreq, &ucOrgSamplingFreq);

		pstDptx_audio_params->eIn_InterfaceType        = (enum  DPTX_AUDIO_INTERFACE_TYPE)ucInfType;
		pstDptx_audio_params->eIn_DataWidth            = (enum  DPTX_AUDIO_DATA_WIDTH)ucDataWidth;
		pstDptx_audio_params->eIn_Max_NumOfchannels = (enum  DPTX_AUDIO_NUM_OF_CHANNELS)ucNumOfCh;
		pstDptx_audio_params->eIn_HBR_Mode             = (enum  DPTX_AUDIO_INTERFACE_TYPE)ucHBRMode;
		pstDptx_audio_params->eIEC_Sampling_Freq       = (enum  DPTX_AUDIO_IEC60958_3_SAMPLE_FREQ)ucSamplingFreq;
		pstDptx_audio_params->eIEC_OrgSamplingFreq     = (enum  DPTX_AUDIO_IEC60958_3_ORIGINAL_SAMPLE_FREQ)ucOrgSamplingFreq;
	}

	return iRetVal;
}


int32_t Dpv14_Tx_API_Set_Audio_Mute(uint8_t ucDP_Index, uint8_t ucMute)
{
	int32_t iRetVal = 0;
	struct Dptx_Params *pstHdl;

	pstHdl = Dptx_Platform_Get_Device_Handle();
	if (pstHdl == NULL) {
		dptx_err("Failed to get handle");
		iRetVal = -ENXIO;
	} else {
		/* For Coverity */
		Dptx_AudIn_Set_Mute(pstHdl, ucDP_Index, ucMute);
	}

	return iRetVal;
}

int32_t Dpv14_Tx_API_Get_Audio_Mute(uint8_t ucDP_Index, uint8_t *pucMute)
{
	int32_t iRetVal = 0;
	struct Dptx_Params *pstHdl;

	pstHdl = Dptx_Platform_Get_Device_Handle();
	if (pstHdl == NULL) {
		dptx_err("Failed to get handle");
		iRetVal = -ENXIO;
	} else {
		/* For Coverity */
		Dptx_AudIn_Get_Mute(pstHdl, ucDP_Index, pucMute);
	}

	return iRetVal;
}


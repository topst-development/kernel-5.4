/*
 * Copyright (c) 2016 Synopsys, Inc.
 *
 * Synopsys DP TX Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
*/

/*
* Modified by Telechips Inc.
*/

#include "Dptx_v14.h"
#include "Dptx_drm_dp_addition.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"

//#define ENABLE_AVGEN_AUDIO_DEBUG

static int32_t dptx_vidin_set_sampler(struct Dptx_Params *pstDptx, u8 ucStream_Index)
{
	u8 ucVideoMapping = 0;
	u32 uiRegMap_VidSampleCtrl;
	struct Dptx_Video_Params *pstVideoParams = &pstDptx->stVideoParams;

	uiRegMap_VidSampleCtrl = Dptx_Reg_Readl(pstDptx, DPTX_VSAMPLE_CTRL_N(ucStream_Index));

	uiRegMap_VidSampleCtrl &= ~(DPTX_VSAMPLE_CTRL_VMAP_BPC_MASK | DPTX_VSAMPLE_CTRL_MULTI_PIXEL_MASK);

	switch ((enum VIDEO_PIXEL_ENCODING_TYPE)pstVideoParams->ucPixel_Encoding) {
	case PIXEL_ENCODING_TYPE_RGB:
		if (pstVideoParams->ucBitPerComponent == (u8)PIXEL_BPC_8_BITS) {
			ucVideoMapping = 1;
		} else {
			dptx_err("Invalid bpc = %d ", pstVideoParams->ucBitPerComponent);
			return DPTX_RETURN_EINVAL;
		}
		break;
	case PIXEL_ENCODING_TYPE_YCBCR422:
		if (pstVideoParams->ucBitPerComponent == (u8)PIXEL_BPC_8_BITS) {
			ucVideoMapping = 9;
		} else {
			dptx_err("Invalid bpc = %d ", pstVideoParams->ucBitPerComponent);
			return DPTX_RETURN_EINVAL;
		}
		break;
	case PIXEL_ENCODING_TYPE_YCBCR444:
		if (pstVideoParams->ucBitPerComponent == (u8)PIXEL_BPC_8_BITS) {
			ucVideoMapping = 5;
		} else {
			dptx_err("Invalid bpc = %d ", pstVideoParams->ucBitPerComponent);
			return DPTX_RETURN_EINVAL;
		}
		break;
	default:
		dptx_err("Invalid encoding type = %d ", (u32)pstVideoParams->ucPixel_Encoding);
		return DPTX_RETURN_EINVAL;
	}

	uiRegMap_VidSampleCtrl |= (ucVideoMapping << DPTX_VSAMPLE_CTRL_VMAP_BPC_SHIFT);
	uiRegMap_VidSampleCtrl |= (pstVideoParams->ucMultiPixel << DPTX_VSAMPLE_CTRL_MULTI_PIXEL_SHIFT);

	Dptx_Reg_Writel(pstDptx, DPTX_VSAMPLE_CTRL_N(ucStream_Index), uiRegMap_VidSampleCtrl);

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_vidin_set_config(struct Dptx_Params *pstDptx, u8 ucStream_Index)
{
	u32 uiVideo_Code, uiRegMap_VidConfig = 0;
	struct Dptx_Video_Params *pstVideoParams = &pstDptx->stVideoParams;
	struct Dptx_Dtd_Params *pstDtd = &pstVideoParams->stDtdParams[ucStream_Index];

{
	uiVideo_Code = pstVideoParams->auiVideo_Code[ucStream_Index];

	if (pstVideoParams->ucVideo_Format == VIDEO_FORMAT_CEA_861) {
		if (uiVideo_Code == 5 || uiVideo_Code == 6 || uiVideo_Code == 7 ||  uiVideo_Code == 10 || uiVideo_Code == 11 || uiVideo_Code == 20 ||
			uiVideo_Code == 21 || uiVideo_Code == 22 || uiVideo_Code == 39 || uiVideo_Code == 25 || uiVideo_Code == 26 || uiVideo_Code == 40 ||
			uiVideo_Code == 44 || uiVideo_Code == 45 || uiVideo_Code == 46 || uiVideo_Code == 50 || uiVideo_Code == 51 || uiVideo_Code == 54 ||
			uiVideo_Code == 55 || uiVideo_Code == 58 || uiVideo_Code  == 59) {
			uiRegMap_VidConfig |= DPTX_VIDEO_CONFIG1_IN_OSC_EN;
		}
	}

	if (pstDtd->interlaced == 1)
		uiRegMap_VidConfig |= DPTX_VIDEO_CONFIG1_O_IP_EN;

	uiRegMap_VidConfig |= (pstDtd->h_active << DPTX_VIDEO_H_ACTIVE_SHIFT);
	uiRegMap_VidConfig |= (pstDtd->h_blanking << DPTX_VIDEO_H_BLANK_SHIFT);

	Dptx_Reg_Writel(pstDptx, DPTX_VIDEO_CONFIG1_N(ucStream_Index), uiRegMap_VidConfig);
}

{
	uiRegMap_VidConfig = 0;

	uiRegMap_VidConfig |= (pstDtd->v_active << DPTX_VIDEO_V_ACTIVE_SHIFT);
	uiRegMap_VidConfig |= (pstDtd->v_blanking << DPTX_VIDEO_V_BLANK_SHIFT);

	Dptx_Reg_Writel(pstDptx, DPTX_VIDEO_CONFIG2_N(ucStream_Index), uiRegMap_VidConfig);
}

{
	uiRegMap_VidConfig = 0;

	uiRegMap_VidConfig |= (pstDtd->h_sync_offset << DPTX_VIDEO_H_FRONT_PORCH_SHIFT);
	uiRegMap_VidConfig |= (pstDtd->h_sync_pulse_width << DPTX_VIDEO_H_SYNC_WIDTH_SHIFT);

	Dptx_Reg_Writel(pstDptx, DPTX_VIDEO_CONFIG3_N(ucStream_Index), uiRegMap_VidConfig);
}

{
	uiRegMap_VidConfig = 0;

	uiRegMap_VidConfig |= (pstDtd->v_sync_offset << DPTX_VIDEO_V_FRONT_PORCH_SHIFT);
	uiRegMap_VidConfig |= (pstDtd->v_sync_pulse_width << DPTX_VIDEO_V_SYNC_WIDTH_SHIFT);

	Dptx_Reg_Writel(pstDptx, DPTX_VIDEO_CONFIG4_N(ucStream_Index), uiRegMap_VidConfig);
}

{
	uiRegMap_VidConfig = Dptx_Reg_Readl(pstDptx, DPTX_VIDEO_CONFIG5_N(ucStream_Index));

	uiRegMap_VidConfig = uiRegMap_VidConfig & (~DPTX_VIDEO_CONFIG5_TU_MASK);
	uiRegMap_VidConfig = (uiRegMap_VidConfig | (pstVideoParams->ucAverage_BytesPerTu << DPTX_VIDEO_CONFIG5_TU_SHIFT));

	if (pstDptx->bMultStreamTransport) {
		uiRegMap_VidConfig &= (~DPTX_VIDEO_CONFIG5_TU_FRAC_MASK_MST);
		uiRegMap_VidConfig |= (pstVideoParams->ucAver_BytesPer_Tu_Frac << DPTX_VIDEO_CONFIG5_TU_FRAC_SHIFT_MST);
	} else {
		uiRegMap_VidConfig &= (~DPTX_VIDEO_CONFIG5_TU_FRAC_MASK_SST);
		uiRegMap_VidConfig |= (pstVideoParams->ucAver_BytesPer_Tu_Frac << DPTX_VIDEO_CONFIG5_TU_FRAC_SHIFT_SST);
	}

	uiRegMap_VidConfig &= (~DPTX_VIDEO_CONFIG5_INIT_THRESHOLD_MASK);
	uiRegMap_VidConfig |= (pstVideoParams->ucInit_Threshold << DPTX_VIDEO_CONFIG5_INIT_THRESHOLD_SHIFT);

	Dptx_Reg_Writel(pstDptx, DPTX_VIDEO_CONFIG5_N(ucStream_Index), uiRegMap_VidConfig);
}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_vidin_set_msa(struct Dptx_Params *pstDptx, u8 ucStream_Index)
{
	u8 ucBpc_Mapping = 0, ucColorimetry_Mapping = 0;
	u32 uiRegMap_Msa1 = 0, uiRegMap_Msa2, uiRegMap_Msa3;
	struct Dptx_Video_Params *pstVideoParams =	&pstDptx->stVideoParams;
	struct Dptx_Dtd_Params *pstDtd = &pstVideoParams->stDtdParams[ucStream_Index];

{
	uiRegMap_Msa1 |= ((pstDtd->h_blanking - pstDtd->h_sync_offset) << DPTX_VIDEO_MSA1_H_START_SHIFT);
	uiRegMap_Msa1 |= ((pstDtd->v_blanking - pstDtd->v_sync_offset) << DPTX_VIDEO_MSA1_V_START_SHIFT);

	Dptx_Reg_Writel(pstDptx, DPTX_VIDEO_MSA1_N(ucStream_Index), uiRegMap_Msa1);
}

{
	uiRegMap_Msa2 = Dptx_Reg_Readl(pstDptx, DPTX_VIDEO_MSA2_N(ucStream_Index));

	uiRegMap_Msa2 &= ~(DPTX_VIDEO_VMSA2_BPC_MASK | DPTX_VIDEO_VMSA2_COL_MASK);

	switch ((enum VIDEO_PIXEL_ENCODING_TYPE)pstVideoParams->ucPixel_Encoding) 	{
	case PIXEL_ENCODING_TYPE_RGB:
		if (pstVideoParams->ucRGB_Standard == RGB_STANDARD_S)
			ucColorimetry_Mapping = 4;
		else if (pstVideoParams->ucRGB_Standard == RGB_STANDARD_LEGACY)
			ucColorimetry_Mapping = 0;
		break;
	case PIXEL_ENCODING_TYPE_YCBCR422:
		if (pstVideoParams->ucColorimetry == (u8)COLORIMETRY_ITU601)
			ucColorimetry_Mapping = 5;
		else if (pstVideoParams->ucColorimetry == (u8)COLORIMETRY_ITU709)
			ucColorimetry_Mapping = 13;
		break;
	case PIXEL_ENCODING_TYPE_YCBCR444:
		if (pstVideoParams->ucColorimetry == (u8)COLORIMETRY_ITU601)
			ucColorimetry_Mapping = 6;
		else if (pstVideoParams->ucColorimetry == (u8)COLORIMETRY_ITU709)
			ucColorimetry_Mapping = 14;
		break;
	default:
		dptx_err("Invalid encoding type = 0x%x", pstVideoParams->ucPixel_Encoding);
		return DPTX_RETURN_EINVAL;
	}

	switch ((enum VIDEO_PIXEL_ENCODING_TYPE)pstVideoParams->ucPixel_Encoding) {
	case PIXEL_ENCODING_TYPE_RGB:
		if (pstVideoParams->ucBitPerComponent == PIXEL_BPC_8_BITS) {
			ucBpc_Mapping = 1;
		} else {
			dptx_err("Invalid bpc = %d ", pstVideoParams->ucBitPerComponent);
			return DPTX_RETURN_EINVAL;
		}
		break;
	case PIXEL_ENCODING_TYPE_YCBCR422:
	case PIXEL_ENCODING_TYPE_YCBCR444:
		if (pstVideoParams->ucBitPerComponent == PIXEL_BPC_8_BITS) {
			ucBpc_Mapping = 1;
		} else {
			dptx_err("Invalid bpc = %d ", pstVideoParams->ucBitPerComponent);
			return DPTX_RETURN_EINVAL;
		}
		break;
	default:
		dptx_err("Invalid encoding type = 0x%x ", pstVideoParams->ucPixel_Encoding);
		return DPTX_RETURN_EINVAL;
	}

	uiRegMap_Msa2 |= (ucColorimetry_Mapping << DPTX_VIDEO_VMSA2_COL_SHIFT);
	uiRegMap_Msa2 |= (ucBpc_Mapping << DPTX_VIDEO_VMSA2_BPC_SHIFT);

	Dptx_Reg_Writel(pstDptx, DPTX_VIDEO_MSA2_N(ucStream_Index), uiRegMap_Msa2);
}


{
	uiRegMap_Msa3 = Dptx_Reg_Readl(pstDptx, DPTX_VIDEO_MSA3_N(ucStream_Index));

	uiRegMap_Msa3 &= ~DPTX_VIDEO_VMSA3_PIX_ENC_MASK;

	Dptx_Reg_Writel(pstDptx, DPTX_VIDEO_MSA3_N(ucStream_Index), uiRegMap_Msa3);
}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_vidin_set_hblank_interval(struct Dptx_Params *pstDptx, u8 ucStream_Index)
{
	u32 uiRegMap_VidHblankInterval;
	u32 uiLink_Clock, uiHBlank_Interval;
	struct Dptx_Video_Params *pstVideoParams = &pstDptx->stVideoParams;
	struct Dptx_Dtd_Params *pstDtd = &pstVideoParams->stDtdParams[ucStream_Index];

	switch (pstDptx->stDptxLink.ucLinkRate) {
	case DPTX_PHYIF_CTRL_RATE_RBR:
		uiLink_Clock = 40500;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR:
		uiLink_Clock = 67500;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR2:
		uiLink_Clock = 135000;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR3:
		uiLink_Clock = 202500;
		break;
	default:
		dptx_err("Invalid rate 0x%x", pstDptx->stDptxLink.ucLinkRate);
		return DPTX_RETURN_EINVAL;
	}

	if (pstDptx->bMultStreamTransport)
		uiHBlank_Interval = ((pstDtd->h_blanking / 16) * pstVideoParams->ucAverage_BytesPerTu * uiLink_Clock / pstDtd->uiPixel_Clock);
	else
		uiHBlank_Interval = (pstDtd->h_blanking * uiLink_Clock / pstDtd->uiPixel_Clock);

	uiRegMap_VidHblankInterval = uiHBlank_Interval;

	uiRegMap_VidHblankInterval |= (DPTX_VIDEO_HBLANK_INTERVAL_ENABLE << DPTX_VIDEO_HBLANK_INTERVAL_SHIFT);

	Dptx_Reg_Writel(pstDptx, DPTX_VIDEO_HBLANK_INTERVAL_N(ucStream_Index), uiRegMap_VidHblankInterval);

	return DPTX_RETURN_NO_ERROR;
}



static int32_t dptx_vidin_config_video_input(struct Dptx_Params *pstDptx, u8 ucStream_Index)
{
	int iRetVal;
	struct Dptx_Video_Params *pstVideoParams = &pstDptx->stVideoParams;
	struct Dptx_Dtd_Params *pstDtd = &pstVideoParams->stDtdParams[ucStream_Index];

	iRetVal = dptx_vidin_set_sampler(pstDptx, ucStream_Index);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

{
	u32 uiRegMap_VidPolarityCtrl = 0;

	if (pstDtd->h_sync_polarity == 1)
		uiRegMap_VidPolarityCtrl |= DPTX_POL_CTRL_H_SYNC_POL_EN;

	if (pstDtd->v_sync_polarity == 1)
		uiRegMap_VidPolarityCtrl |= DPTX_POL_CTRL_V_SYNC_POL_EN;

	Dptx_Reg_Writel(pstDptx, DPTX_VSAMPLE_POLARITY_CTRL_N(ucStream_Index), uiRegMap_VidPolarityCtrl);

}
	iRetVal = dptx_vidin_set_config(pstDptx, ucStream_Index);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = dptx_vidin_set_msa(pstDptx, ucStream_Index);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = dptx_vidin_set_hblank_interval(pstDptx, ucStream_Index);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	return DPTX_RETURN_NO_ERROR;
}

static void dptx_vidin_reset_dtd(struct Dptx_Dtd_Params *pstDtd)
{
	pstDtd->pixel_repetition_input	= 0;
	pstDtd->uiPixel_Clock			= 0;
	pstDtd->h_active				= 0;
	pstDtd->h_blanking				= 0;
	pstDtd->h_sync_offset			= 0;
	pstDtd->h_sync_pulse_width		= 0;
	pstDtd->h_image_size			= 0;
	pstDtd->v_active				= 0;
	pstDtd->v_blanking				= 0;
	pstDtd->v_sync_offset			= 0;
	pstDtd->v_sync_pulse_width		= 0;
	pstDtd->v_image_size			= 0;
	pstDtd->interlaced				= 0;
	pstDtd->v_sync_polarity			= 0;
	pstDtd->h_sync_polarity			= 0;
}

int32_t Dptx_VidIn_Init_Params(struct Dptx_Params *pstDptx, u32 uiPeri_Pixel_Clock[PHY_INPUT_STREAM_MAX])
{
	u8 ucStream_Index;
	int32_t iRetVal;
	struct Dptx_Video_Params *pstVideoParams = &pstDptx->stVideoParams;

	pstVideoParams->ucMultiPixel		= (u8)MULTI_PIXEL_TYPE_SINGLE;
	pstVideoParams->ucBitPerComponent	= (u8)PIXEL_BPC_8_BITS;
	pstVideoParams->ucColorimetry		= (u8)COLORIMETRY_ITU601;
	pstVideoParams->ucRGB_Standard		= (u8)RGB_STANDARD_LEGACY;
	pstVideoParams->ucVideo_Format		= (u8)VIDEO_FORMAT_CEA_861;
	pstVideoParams->ucDynamicRange		= (u8)VIDEO_DYNAMIC_RANGE_CEA;
	pstVideoParams->ucPixel_Encoding	= (u8)PIXEL_ENCODING_TYPE_RGB;
	pstVideoParams->uiRefresh_Rate		= (u32)VIDEO_REFRESH_RATE_60_00HZ;

	iRetVal = Dptx_VidIn_Get_NumOfStreams(pstDptx, &pstDptx->ucNumOfStreams);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		dptx_err("from Dptx_VidIn_Get_NumOfStreams()");
		return iRetVal;
	}

	if (pstDptx->ucNumOfStreams == 0) {
		dptx_warn("DP was not enabled");
		return DPTX_RETURN_NO_ERROR;
	}

	iRetVal = Dptx_VidIn_Get_Pixel_Mode(pstDptx, (u8)PHY_INPUT_STREAM_0, &pstVideoParams->ucMultiPixel);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_VidIn_Get_PixelEncoding_Type(pstDptx, (u8)PHY_INPUT_STREAM_0, &pstVideoParams->ucPixel_Encoding);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	for (ucStream_Index = 0; ucStream_Index < pstDptx->ucNumOfStreams; ucStream_Index++) {
		iRetVal = Dptx_VidIn_Get_VIC_From_Dtd(pstDptx, ucStream_Index, &pstVideoParams->auiVideo_Code[ucStream_Index]);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return iRetVal;
	}

	for (ucStream_Index = 0; ucStream_Index < (u8)PHY_INPUT_STREAM_MAX; ucStream_Index++)
		pstVideoParams->uiPeri_Pixel_Clock[ucStream_Index] = uiPeri_Pixel_Clock[ucStream_Index];

	dptx_dbg("Pixel mode = %s, Pixel encoding = %s, NumOfStreams = %d, VIC = %d %d %d %d, PClk = %d %d %d %d",
				(pstVideoParams->ucMultiPixel == 0) ? "Single" : (pstVideoParams->ucMultiPixel == 1) ? "Dual" : "Quad",
				(pstVideoParams->ucPixel_Encoding == PIXEL_ENCODING_TYPE_RGB) ? "RGB" :
				(pstVideoParams->ucPixel_Encoding == PIXEL_ENCODING_TYPE_YCBCR422) ? "YCbCr422" : "YCbCr444",
				pstDptx->ucNumOfStreams,
				pstVideoParams->auiVideo_Code[0], pstVideoParams->auiVideo_Code[1],
				pstVideoParams->auiVideo_Code[2], pstVideoParams->auiVideo_Code[3],
				pstVideoParams->uiPeri_Pixel_Clock[0], pstVideoParams->uiPeri_Pixel_Clock[1],
				pstVideoParams->uiPeri_Pixel_Clock[2], pstVideoParams->uiPeri_Pixel_Clock[3]);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_VidIn_Set_Detailed_Timing(struct Dptx_Params *pstDptx, u8 ucStream_Index, struct Dptx_Dtd_Params *pstDtd_Params)
{
	int32_t iRetVal;
	struct Dptx_Video_Params *pstVideoParams = &pstDptx->stVideoParams;

	if (ucStream_Index >= PHY_INPUT_STREAM_MAX) {
		dptx_err("Invalid stream index(%d) ", ucStream_Index);
		return DPTX_RETURN_EINVAL;
	}

	memcpy(&pstVideoParams->stDtdParams[ucStream_Index], pstDtd_Params, sizeof(struct Dptx_Dtd_Params));

	pstDptx->stVideoParams.uiPeri_Pixel_Clock[ucStream_Index] = pstVideoParams->stDtdParams[ucStream_Index].uiPixel_Clock;

	dptx_dbg("Stream %d :  ", ucStream_Index);
	dptx_dbg("Refresh rate: %d", pstVideoParams->uiRefresh_Rate);
	dptx_dbg("DTD format: %s", pstVideoParams->ucVideo_Format == VIDEO_FORMAT_CEA_861 ? "CEA_861":"Others");
	dptx_dbg("Pixel Clk: %d", pstVideoParams->stDtdParams[ucStream_Index].uiPixel_Clock);

	iRetVal = Dptx_VidIn_Calculate_Average_TU_Symbols(pstDptx,
															pstDptx->stDptxLink.ucNumOfLanes,
															pstDptx->stDptxLink.ucLinkRate,
															pstVideoParams->ucBitPerComponent,
															pstVideoParams->ucPixel_Encoding,
															pstVideoParams->stDtdParams[ucStream_Index].uiPixel_Clock,
															ucStream_Index);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	Dptx_VidIn_Set_Stream_Enable(pstDptx, false, ucStream_Index);

	iRetVal = dptx_vidin_config_video_input(pstDptx, (u8)ucStream_Index);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_VidIn_Set_Reset(struct Dptx_Params *pstDptx, bool ucReset, uint8_t ucStream_Index)
{
	u32 uiRegMap_SRSTCtrl;

	uiRegMap_SRSTCtrl = Dptx_Reg_Readl(pstDptx, DPTX_SRST_CTRL);

	if (ucReset)
		uiRegMap_SRSTCtrl |= DPTX_SRST_VIDEO_RESET_N(ucStream_Index);
	else
		uiRegMap_SRSTCtrl &= ~(DPTX_SRST_VIDEO_RESET_N(ucStream_Index));

	Dptx_Reg_Writel(pstDptx, DPTX_SRST_CTRL, uiRegMap_SRSTCtrl);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_VidIn_Set_Test_Pattern_Timing(struct Dptx_Params *pstDptx, u8 ucStream_Index)
{
	uint8_t ucTest_h_total_lsb, ucTest_h_total_msb,      ucTest_v_total_lsb, ucTest_v_total_msb;
	uint8_t ucTest_h_start_lsb, ucTest_h_start_msb, ucTest_v_start_lsb, ucTest_v_start_msb;
	uint8_t ucTest_hsync_width_lsb, ucTest_hsync_width_msb, ucTest_vsync_width_lsb, ucTest_vsync_width_msb;
	uint8_t ucTest_h_width_lsb, ucTest_h_width_msb, ucTest_v_width_lsb, ucTest_v_width_msb;
	uint8_t ucTest_refresh_rate = 0;
	uint8_t ucVIC;
	int32_t iRetVal = 0;
	uint32_t uiH_total = 0, uiV_total = 0, uiH_start = 0, uiV_start = 0;
	uint32_t uiH_width = 0, uiV_width = 0, uiHsync_width = 0, uiVsync_width = 0;
	uint32_t uiH_sync_pol = 0, uiV_sync_pol = 0;
	enum VIDEO_FORMAT_TYPE eVideo_format;
	struct Dptx_Dtd_Params mdtd;
	struct Dptx_Video_Params *pstVideoParams;

	pstVideoParams = &pstDptx->stVideoParams;

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_H_TOTAL_LSB, &ucTest_h_total_lsb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;
	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_H_TOTAL_MSB, &ucTest_h_total_msb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	uiH_total |= ucTest_h_total_lsb;
	uiH_total |= ucTest_h_total_msb << 8;

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_V_TOTAL_LSB, &ucTest_v_total_lsb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;
	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_V_TOTAL_MSB, &ucTest_v_total_msb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	uiV_total |= ucTest_v_total_lsb;
	uiV_total |= ucTest_v_total_msb << 8;

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_H_START_LSB, &ucTest_h_start_lsb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;
	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_H_START_MSB, &ucTest_h_start_msb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	uiH_start |= ucTest_h_start_lsb;
	uiH_start |= ucTest_h_start_msb << 8;

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_V_START_LSB, &ucTest_v_start_lsb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;
	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_V_START_MSB, &ucTest_v_start_msb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	uiV_start |= ucTest_v_start_lsb;
	uiV_start |= ucTest_v_start_msb << 8;

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_H_SYNC_WIDTH_LSB, &ucTest_hsync_width_lsb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;
	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_H_SYNC_WIDTH_MSB, &ucTest_hsync_width_msb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	uiHsync_width |= ucTest_hsync_width_lsb;
	uiHsync_width |= (ucTest_hsync_width_msb & (~(1 << 7))) << 8;
	uiH_sync_pol |= (ucTest_hsync_width_msb & (1 << 7)) >> 8;

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_V_SYNC_WIDTH_LSB, &ucTest_vsync_width_lsb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;
	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_V_SYNC_WIDTH_MSB, &ucTest_vsync_width_msb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	uiVsync_width |= ucTest_vsync_width_lsb;
	uiVsync_width |= (ucTest_vsync_width_msb & (~(1 << 7))) << 8;
	uiV_sync_pol |= (ucTest_vsync_width_msb & (1 << 7)) >> 8;

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_H_WIDTH_LSB, &ucTest_h_width_lsb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;
	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_H_WIDTH_MSB, &ucTest_h_width_msb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	uiH_width |= ucTest_h_width_lsb;
	uiH_width |= ucTest_h_width_msb << 8;

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_V_WIDTH_LSB, &ucTest_v_width_lsb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;
	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_V_WIDTH_MSB, &ucTest_v_width_msb);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	uiV_width |= ucTest_v_width_lsb;
	uiV_width |= ucTest_v_width_msb << 8;

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_TEST_REFRESH_RATE, &ucTest_refresh_rate);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	eVideo_format = VIDEO_FORMAT_VESA_DMT;
	ucTest_refresh_rate = (ucTest_refresh_rate * 1000);

	if (uiH_total == 1056 && uiV_total == 628 && uiH_start == 216 && uiV_start == 27 &&
		uiHsync_width == 128 && uiVsync_width == 4 && uiH_width == 800 && uiV_width == 600) {
		ucVIC = 9;
	} else if (uiH_total == 1088 && uiV_total == 517 && uiH_start == 224 && uiV_start == 31 &&
				uiHsync_width == 112 && uiVsync_width == 8 && uiH_width == 848 && uiV_width == 480) {
		ucVIC = 14;
	} else if (uiH_total == 1344 && uiV_total == 806 && uiH_start == 296 && uiV_start == 35 &&
				uiHsync_width == 136 && uiVsync_width == 6 && uiH_width == 1024 && uiV_width == 768) {
		ucVIC = 16;
	} else if (uiH_total == 1440 && uiV_total == 790 && uiH_start == 112 && uiV_start == 19 &&
				uiHsync_width == 32 && uiVsync_width == 7 && uiH_width == 1280 && uiV_width == 768) {
		ucVIC = 22;
	} else if (uiH_total == 1664 && uiV_total == 798 && uiH_start == 320 && uiV_start == 27 &&
				uiHsync_width == 128 && uiVsync_width == 7 && uiH_width == 1280 && uiV_width == 768) {
		ucVIC = 23;
	} else if (uiH_total == 1440 && uiV_total == 823 && uiH_start == 112 && uiV_start == 20 &&
				uiHsync_width == 32 && uiVsync_width == 6 && uiH_width == 1280 && uiV_width == 800) {
		ucVIC = 27;
	} else if (uiH_total == 1800 && uiV_total == 1000 && uiH_start == 424 && uiV_start == 39 &&
				uiHsync_width == 112 && uiVsync_width == 3 && uiH_width == 1280 && uiV_width == 960) {
		ucVIC = 32;
	} else if (uiH_total == 1688 && uiV_total == 1066 && uiH_start == 360 && uiV_start == 41 &&
				uiHsync_width == 112 && uiVsync_width == 3 && uiH_width == 1280 && uiV_width  == 1024) {
		ucVIC = 35;
	} else if (uiH_total == 1792 && uiV_total == 795 && uiH_start == 368 && uiV_start == 24 &&
				uiHsync_width == 112 && uiVsync_width == 6 && uiH_width == 1360 && uiV_width == 768) {
		ucVIC = 39;
	} else if (uiH_total == 1560 && uiV_total == 1080  && uiH_start == 112  && uiV_start == 27 &&
				uiHsync_width == 32 && uiVsync_width == 4 && uiH_width == 1400 && uiV_width == 1050) {
		ucVIC = 41;
	} else if (uiH_total == 2160 && uiV_total == 1250 && uiH_start == 496 && uiV_start == 49 &&
				uiHsync_width == 192 && uiVsync_width == 3 && uiH_width == 1600 && uiV_width == 1200) {
		ucVIC = 51;
	} else if (uiH_total == 2448 && uiV_total == 1394 && uiH_start == 528 && uiV_start == 49 &&
				uiHsync_width == 200 && uiVsync_width == 3 && uiH_width == 1792 && uiV_width == 1344) {
		ucVIC = 62;
	} else if (uiH_total == 2600 && uiV_total == 1500 && uiH_start == 552 &&
		   uiV_start == 59  && uiHsync_width == 208 && uiVsync_width == 3 &&
		   uiH_width == 1920 && uiV_width == 1440) {
		ucVIC = 73;
	} else if (uiH_total == 2200 && uiV_total == 1125 && uiH_start == 192 && uiV_start == 41 &&
				uiHsync_width == 44 && uiVsync_width == 5 && uiH_width == 1920 && uiV_width == 1080) {
		if (ucTest_refresh_rate == 120000) {
			ucVIC = 63;
			eVideo_format = VIDEO_FORMAT_CEA_861;
		} else {
			ucVIC = 82;
		}
	} else if (uiH_total == 800 && uiV_total == 525 && uiH_start == 144 && uiV_start == 35 &&
				uiHsync_width == 96 && uiVsync_width == 2 && uiH_width == 640 && uiV_width == 480) {
		ucVIC = 1;
		eVideo_format = VIDEO_FORMAT_CEA_861;
	} else if (uiH_total == 1650 && uiV_total == 750 && uiH_start == 260 && uiV_start == 25 &&
				uiHsync_width == 40 && uiVsync_width == 5 && uiH_width == 1280  && uiV_width == 720) {
		ucVIC = 4;
		eVideo_format = VIDEO_FORMAT_CEA_861;
	} else if (uiH_total == 1680 && uiV_total == 831 && uiH_start == 328 && uiV_start == 28 &&
				uiHsync_width == 128 && uiVsync_width == 6 && uiH_width == 1280 && uiV_width == 800) {
		ucVIC = 28;
		eVideo_format = VIDEO_FORMAT_VESA_CVT;
	} else if (uiH_total == 1760 && uiV_total == 1235 && uiH_start == 112 && uiV_start == 32 &&
				uiHsync_width == 32 && uiVsync_width == 4 && uiH_width == 1600 && uiV_width == 1200) {
		ucVIC = 40;
		eVideo_format = VIDEO_FORMAT_VESA_CVT;
	} else if (uiH_total == 2208 && uiV_total == 1580 && uiH_start == 112 && uiV_start == 41 &&
				uiHsync_width == 32 &&  uiVsync_width == 4 && uiH_width == 2048 && uiV_width == 1536) {
		ucVIC = 41;
		eVideo_format = VIDEO_FORMAT_VESA_CVT;
	} else {
		dptx_err("Unknown video mode");
		return DPTX_RETURN_EINVAL;
	}

	dptx_info("\nTest pattern video timming :\n");
	dptx_info(" 	H total = %d, V total = %d", uiH_total, uiV_total);
	dptx_info(" 	H start = %d, V start = %d", uiH_start, uiV_start);
	dptx_info(" 	H sync width = %d, V sync width = %d", uiHsync_width, uiVsync_width);
	dptx_info(" 	H width = %d, V width = %d", uiH_width, uiV_width);
	dptx_info(" 	H sync polarity = %d, V sync polarity = %d", uiH_sync_pol, uiV_sync_pol);
	dptx_info(" 	Refresh rate = %d", ucTest_refresh_rate);
	dptx_info(" 	Video Code = %d", ucVIC);
	dptx_info(" 	Video Format = %s\n", (eVideo_format == VIDEO_FORMAT_CEA_861) ? "CEA-861" :
											(eVideo_format == VIDEO_FORMAT_VESA_CVT) ? "VESA CVT" :
											(eVideo_format == VIDEO_FORMAT_VESA_DMT) ? "VESA DMT" :"Unknown");

	iRetVal = Dptx_VidIn_Fill_Dtd(&mdtd, ucVIC, ucTest_refresh_rate, (uint8_t)eVideo_format);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	pstVideoParams->stDtdParams[ucStream_Index] = mdtd;
	pstVideoParams->uiRefresh_Rate = ucTest_refresh_rate;
	pstVideoParams->auiVideo_Code[ucStream_Index] = ucVIC;
	pstVideoParams->ucVideo_Format = (uint8_t)eVideo_format;

	/* Need to change pixel clock here */
	/* pstVideoParams->stDtdParams[ucStream_Index].uiPixel_Clock should be pixel clock retrieved from Peri and
		it should copied to pstDptx->stVideoParams.uiPeri_Pixel_Clock too */

	iRetVal = Dptx_VidIn_Calculate_Average_TU_Symbols(pstDptx,
															pstDptx->stDptxLink.ucNumOfLanes,
															pstDptx->stDptxLink.ucLinkRate,
															pstVideoParams->ucBitPerComponent,
															pstVideoParams->ucPixel_Encoding,
															pstVideoParams->stDtdParams[ucStream_Index].uiPixel_Clock,
															ucStream_Index);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_VidIn_Set_Reset(pstDptx, true, ucStream_Index);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_VidIn_Set_Stream_Enable(pstDptx, false, ucStream_Index);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
			return iRetVal;

	iRetVal = Dptx_VidIn_Set_BPC(pstDptx, ucStream_Index);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
			return iRetVal;

	iRetVal = Dptx_VidIn_Set_Timing(pstDptx, ucStream_Index);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
			return iRetVal;

#if defined(DP_LINK_VG_REGISTER_SET)
	iRetVal = dptx_vidin_config_gen(pstDptx, ucStream_Index);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
			return iRetVal;

	Dptx_Reg_Writel(pstDptx, DPTX_VG_SWRST_N(ucStream_Index), 0);
#endif

	return DPTX_RETURN_NO_ERROR;
}


int32_t Dptx_VidIn_Set_BPC(struct Dptx_Params *pstDptx, u8 ucStream_Index)
{
	u8 ucVideoMapping = 0;
	u32 uiRegMap_VidSampleCtrl;
	struct Dptx_Video_Params *pstVideoParams = &pstDptx->stVideoParams;

	uiRegMap_VidSampleCtrl = Dptx_Reg_Readl(pstDptx, DPTX_VSAMPLE_CTRL_N(ucStream_Index));

	uiRegMap_VidSampleCtrl &= ~(DPTX_VSAMPLE_CTRL_VMAP_BPC_MASK | DPTX_VSAMPLE_CTRL_MULTI_PIXEL_MASK);

	switch ((enum VIDEO_PIXEL_ENCODING_TYPE)pstVideoParams->ucPixel_Encoding) {
	case PIXEL_ENCODING_TYPE_RGB:
		if (pstVideoParams->ucBitPerComponent == (u8)PIXEL_BPC_8_BITS) {
			ucVideoMapping = 1;
		} else {
			dptx_err("Invalid bpc = %d ", pstVideoParams->ucBitPerComponent);
			return DPTX_RETURN_EINVAL;
		}
		break;
	case PIXEL_ENCODING_TYPE_YCBCR422:
		if (pstVideoParams->ucBitPerComponent == (u8)PIXEL_BPC_8_BITS) {
			ucVideoMapping = 9;
		} else {
			dptx_err("Invalid bpc = %d ", pstVideoParams->ucBitPerComponent);
			return DPTX_RETURN_EINVAL;
		}
		break;
	case PIXEL_ENCODING_TYPE_YCBCR444:
		if (pstVideoParams->ucBitPerComponent == (u8)PIXEL_BPC_8_BITS) {
			ucVideoMapping = 5;
		} else {
			dptx_err("Invalid bpc = %d ", pstVideoParams->ucBitPerComponent);
			return DPTX_RETURN_EINVAL;
		}
		break;
	default:
		dptx_err("Invalid encoding type = %d ", (u32)pstVideoParams->ucPixel_Encoding);
		return DPTX_RETURN_EINVAL;
		break;
	}

	uiRegMap_VidSampleCtrl |= (ucVideoMapping << DPTX_VSAMPLE_CTRL_VMAP_BPC_SHIFT);
	uiRegMap_VidSampleCtrl |= (pstVideoParams->ucMultiPixel << DPTX_VSAMPLE_CTRL_MULTI_PIXEL_SHIFT);

	Dptx_Reg_Writel(pstDptx, DPTX_VSAMPLE_CTRL_N(ucStream_Index), uiRegMap_VidSampleCtrl);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_VidIn_Set_Ts_Change(struct Dptx_Params *pstDptx, u8 ucStream_Index)
{
	uint32_t uiRegMap_VidConfig;
	struct Dptx_Video_Params *pstVideoParams;

	pstVideoParams = &pstDptx->stVideoParams;

	uiRegMap_VidConfig = Dptx_Reg_Readl(pstDptx, DPTX_VIDEO_CONFIG5_N(ucStream_Index));

	uiRegMap_VidConfig = uiRegMap_VidConfig & (~DPTX_VIDEO_CONFIG5_TU_MASK);
	uiRegMap_VidConfig = (uiRegMap_VidConfig | (pstVideoParams->ucAverage_BytesPerTu << DPTX_VIDEO_CONFIG5_TU_SHIFT));

	if (pstDptx->bMultStreamTransport) {
		uiRegMap_VidConfig &= (~DPTX_VIDEO_CONFIG5_TU_FRAC_MASK_MST);
		uiRegMap_VidConfig |= (pstVideoParams->ucAver_BytesPer_Tu_Frac << DPTX_VIDEO_CONFIG5_TU_FRAC_SHIFT_MST);
	} else 	{
		uiRegMap_VidConfig &= (~DPTX_VIDEO_CONFIG5_TU_FRAC_MASK_SST);
		uiRegMap_VidConfig |= (pstVideoParams->ucAver_BytesPer_Tu_Frac << DPTX_VIDEO_CONFIG5_TU_FRAC_SHIFT_SST);
	}

	uiRegMap_VidConfig &= (~DPTX_VIDEO_CONFIG5_INIT_THRESHOLD_MASK);
	uiRegMap_VidConfig |= (pstVideoParams->ucInit_Threshold << DPTX_VIDEO_CONFIG5_INIT_THRESHOLD_SHIFT);

	Dptx_Reg_Writel(pstDptx, DPTX_VIDEO_CONFIG5_N(ucStream_Index), uiRegMap_VidConfig);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_VidIn_Set_Stream_Enable(struct Dptx_Params *pstDptx, bool bEnable_Stream, u8 ucStream_Index)
{
	u32	uiRegMap_VSampleCtrl;

	uiRegMap_VSampleCtrl = Dptx_Reg_Readl(pstDptx, DPTX_VSAMPLE_CTRL_N(ucStream_Index));

	if (bEnable_Stream)
		uiRegMap_VSampleCtrl |= DPTX_VSAMPLE_CTRL_STREAM_EN;
	else
		uiRegMap_VSampleCtrl &= ~DPTX_VSAMPLE_CTRL_STREAM_EN;

	Dptx_Reg_Writel(pstDptx, DPTX_VSAMPLE_CTRL_N(ucStream_Index), uiRegMap_VSampleCtrl);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_VidIn_Get_Stream_Enable(struct Dptx_Params *pstDptx, bool *pbEnable_Stream, u8 ucStream_Index)
{
	u32 uiRegMap_VSampleCtrl;

	uiRegMap_VSampleCtrl = Dptx_Reg_Readl(pstDptx, DPTX_VSAMPLE_CTRL_N(ucStream_Index));

	*pbEnable_Stream = (bool)(uiRegMap_VSampleCtrl & DPTX_VSAMPLE_CTRL_STREAM_EN);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_VidIn_Set_Timing(struct Dptx_Params *pstDptx, u8 ucStream_Index)
{
	int32_t	iRetVal;

	Dptx_VidIn_Set_Stream_Enable(pstDptx, false, ucStream_Index);

	iRetVal = dptx_vidin_config_video_input(pstDptx, (u8)ucStream_Index);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	Dptx_VidIn_Set_Stream_Enable(pstDptx, true, ucStream_Index);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_VidIn_Get_NumOfStreams(struct Dptx_Params *pstDptx, u8 *pucEnabled_Streams)
{
	bool bEnabled_Stream;
	u8 ucStreamIndex;
	u8 ucEnabled_StreamCnt = 0;
	int32_t iRetVal;

	for (ucStreamIndex = 0; ucStreamIndex < PHY_INPUT_STREAM_MAX; ucStreamIndex++) {
		iRetVal = Dptx_VidIn_Get_Stream_Enable(pstDptx, &bEnabled_Stream, ucStreamIndex);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return iRetVal;

		if (bEnabled_Stream)
			ucEnabled_StreamCnt++;
	}

	*pucEnabled_Streams = ucEnabled_StreamCnt;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_VidIn_Get_Pixel_Mode(struct Dptx_Params *pstDptx, u8 ucStream_Index, u8 *pucPixelMode)
{
	u32	uiRegMap_VidSampleCtrl;

	uiRegMap_VidSampleCtrl = Dptx_Reg_Readl(pstDptx, DPTX_VSAMPLE_CTRL_N(ucStream_Index));

	*pucPixelMode = (u8)((uiRegMap_VidSampleCtrl & DPTX_VSAMPLE_CTRL_MULTI_PIXEL_MASK) >> DPTX_VSAMPLE_CTRL_MULTI_PIXEL_SHIFT);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_VidIn_Get_PixelEncoding_Type(struct Dptx_Params *pstDptx, u8 ucStream_Index, u8 *pucPixel_Encoding)
{
	u8	ucColorimetry_Mapping = 0;
	u32 uiRegMap_Msa2;

	uiRegMap_Msa2 = Dptx_Reg_Readl(pstDptx, DPTX_VIDEO_MSA2_N(ucStream_Index));

	ucColorimetry_Mapping = ((uiRegMap_Msa2 & DPTX_VIDEO_VMSA2_COL_MASK) >> DPTX_VIDEO_VMSA2_COL_SHIFT);

	if ((ucColorimetry_Mapping == 0) || (ucColorimetry_Mapping == 4)) {
		*pucPixel_Encoding = (u8)PIXEL_ENCODING_TYPE_RGB;
	} else if ((ucColorimetry_Mapping == 5) || (ucColorimetry_Mapping == 13)) {
		*pucPixel_Encoding = (u8)PIXEL_ENCODING_TYPE_YCBCR422;
	} else if ((ucColorimetry_Mapping == 6) || (ucColorimetry_Mapping == 14)) {
		*pucPixel_Encoding = (u8)PIXEL_ENCODING_TYPE_YCBCR444;
	} else {
		dptx_warn("Invalid encoding type %d... set default to RGB ", ucColorimetry_Mapping);
		*pucPixel_Encoding = (u8)PIXEL_ENCODING_TYPE_RGB;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_VidIn_Get_Configured_Timing(struct Dptx_Params *pstDptx, u8 ucStream_Index, struct Dptx_Dtd_Params *pstDtd)
{
	u32 uiRegMap_VidConfig1, uiRegMap_VidConfig2, uiRegMap_VidConfig3, uiRegMap_VidConfig4;
	u32 uiRegMap_VidPolarityCtrl;

	uiRegMap_VidPolarityCtrl = Dptx_Reg_Readl(pstDptx, DPTX_VSAMPLE_POLARITY_CTRL_N(ucStream_Index));
	uiRegMap_VidConfig1 = Dptx_Reg_Readl(pstDptx, DPTX_VIDEO_CONFIG1_N(ucStream_Index));
	uiRegMap_VidConfig2 = Dptx_Reg_Readl(pstDptx, DPTX_VIDEO_CONFIG2_N(ucStream_Index));
	uiRegMap_VidConfig3 = Dptx_Reg_Readl(pstDptx, DPTX_VIDEO_CONFIG3_N(ucStream_Index));
	uiRegMap_VidConfig4 = Dptx_Reg_Readl(pstDptx, DPTX_VIDEO_CONFIG4_N(ucStream_Index));

	if (uiRegMap_VidPolarityCtrl &  DPTX_POL_CTRL_H_SYNC_POL_EN)
		pstDtd->h_sync_polarity = 1;
	else
		pstDtd->h_sync_polarity = 0;

	if (uiRegMap_VidPolarityCtrl &  DPTX_POL_CTRL_V_SYNC_POL_EN)
		pstDtd->v_sync_polarity = 1;
	else
		pstDtd->v_sync_polarity = 0;

	if (uiRegMap_VidConfig1 &  DPTX_VIDEO_CONFIG1_O_IP_EN)
		pstDtd->interlaced = 1;
	else
		pstDtd->interlaced = 0;

	pstDtd->h_active = ((uiRegMap_VidConfig1 & DPTX_VIDEO_H_ACTIVE_MASK) >> DPTX_VIDEO_H_ACTIVE_SHIFT);
	pstDtd->h_blanking = ((uiRegMap_VidConfig1 & DPTX_VIDEO_H_BLANK_MASK) >> DPTX_VIDEO_H_BLANK_SHIFT);

	pstDtd->v_active = ((uiRegMap_VidConfig2 & DPTX_VIDEO_V_ACTIVE_MASK) >> DPTX_VIDEO_V_ACTIVE_SHIFT);
	pstDtd->v_blanking = ((uiRegMap_VidConfig2 & DPTX_VIDEO_V_BLANK_MASK) >> DPTX_VIDEO_V_BLANK_SHIFT);

	pstDtd->h_sync_offset = ((uiRegMap_VidConfig3 & DPTX_VIDEO_H_FRONTE_PORCH_MASK) >> DPTX_VIDEO_H_FRONT_PORCH_SHIFT);
	pstDtd->h_sync_pulse_width = ((uiRegMap_VidConfig3 & DPTX_VIDEO_H_SYNC_WIDTH_MASK) >> DPTX_VIDEO_H_SYNC_WIDTH_SHIFT);

	pstDtd->v_sync_offset = ((uiRegMap_VidConfig4 & DPTX_VIDEO_V_FRONTE_PORCH_MASK) >> DPTX_VIDEO_V_FRONT_PORCH_SHIFT);
	pstDtd->v_sync_pulse_width = ((uiRegMap_VidConfig4 & DPTX_VIDEO_V_SYNC_WIDTH_MASK) >> DPTX_VIDEO_V_SYNC_WIDTH_SHIFT);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_VidIn_Calculate_Average_TU_Symbols(struct Dptx_Params *pstDptx, int iNumOfLane, int iLinkRate, int iBpc, int iEncodingType, int iPixel_Clock, u8 ucStream_Index)
{
	int iLink_Rate, iLink_Clock, iColor_Depth, iT1 = 0, iT2 = 0;
	int iH_AverageSymbol_Per_TU, iAverageSymbol_Per_TU, iTransfer_Unit_Fraction;
	struct Dptx_Video_Params *pstVideoParams = &pstDptx->stVideoParams;
	struct Dptx_Dtd_Params *pstDtd = &pstVideoParams->stDtdParams[ucStream_Index];

	switch (iLinkRate) {
	case DPTX_PHYIF_CTRL_RATE_RBR:
		iLink_Rate = 162;
		iLink_Clock = 40500;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR:
		iLink_Rate = 270;
		iLink_Clock = 67500;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR2:
		iLink_Rate = 540;
		iLink_Clock = 135000;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR3:
		iLink_Rate = 810;
		iLink_Clock = 202500;
		break;
	default:
		dptx_err("Invalid rate param = %d", iLinkRate);
		return DPTX_RETURN_EINVAL;
	}

	switch ((enum VIDEO_PIXEL_BIT_DEPTH)iBpc) {
	case PIXEL_BPC_8_BITS:
		if ((iEncodingType == PIXEL_ENCODING_TYPE_RGB) || (iEncodingType == PIXEL_ENCODING_TYPE_YCBCR444)) {
			iColor_Depth  = (PIXEL_BPC_8_BITS * VIDEO_LINK_BPP_RGB_YCbCr444);
		} else if (iEncodingType == PIXEL_ENCODING_TYPE_YCBCR422) {
			iColor_Depth = (PIXEL_BPC_8_BITS * VIDEO_LINK_BPP_YCbCr422);
		} else {
			dptx_err("Invalid encoding type(%d)", iEncodingType);
			return DPTX_RETURN_EINVAL;
		}
		break;
	default:
		dptx_err("Invalid Bpc(%d)", iBpc);
		return DPTX_RETURN_EINVAL;
	}

	iH_AverageSymbol_Per_TU = ((8 * iColor_Depth * iPixel_Clock) / (iNumOfLane * iLink_Rate));
	iAverageSymbol_Per_TU  = (iH_AverageSymbol_Per_TU / 1000);
	if (iAverageSymbol_Per_TU > DPTX_MAX_LINK_SYMBOLS) {
		dptx_warn("iTransfer_Unit(%d) > DPTX_MAX_LINK_SYMBOLS", iAverageSymbol_Per_TU);
		return DPTX_RETURN_ENODEV;
	}

	iTransfer_Unit_Fraction = ((iH_AverageSymbol_Per_TU / 100) - (iAverageSymbol_Per_TU * 10));

	pstVideoParams->ucAverage_BytesPerTu	= iAverageSymbol_Per_TU;
	pstVideoParams->ucAver_BytesPer_Tu_Frac = iTransfer_Unit_Fraction;

	if (pstVideoParams->ucMultiPixel == MULTI_PIXEL_TYPE_SINGLE) {
		if (iAverageSymbol_Per_TU < 6)
			pstVideoParams->ucInit_Threshold = 32;
		else if (pstDtd->h_blanking <= 80)
			pstVideoParams->ucInit_Threshold = 12;
		else
			pstVideoParams->ucInit_Threshold = 16;
	} else {
		switch ((enum VIDEO_PIXEL_BIT_DEPTH)iBpc) {
		case PIXEL_BPC_8_BITS:
			if ((iEncodingType == PIXEL_ENCODING_TYPE_RGB) || (iEncodingType == PIXEL_ENCODING_TYPE_YCBCR444)) {
				if (pstVideoParams->ucMultiPixel == MULTI_PIXEL_TYPE_DUAL)
					iT1 = ((1000 / 3) * iNumOfLane);
				else
					iT1 = ((3000 / 16) * iNumOfLane);
			} else if (iEncodingType == PIXEL_ENCODING_TYPE_YCBCR422) {
				iT1 = ((1000 / 2) * iNumOfLane);
			} else {
				dptx_err("Invalid encoding type(%d)", iEncodingType);
				return DPTX_RETURN_EINVAL;
			}
			break;
		default:
			dptx_err("Invalid param iBpc = %d", iBpc);
			return DPTX_RETURN_EINVAL;
		}

		iT2 = ((iLink_Clock * 1000) / iPixel_Clock);

		pstVideoParams->ucInit_Threshold = (iT1 * iT2 * iAverageSymbol_Per_TU / (1000 * 1000));
	}

	iH_AverageSymbol_Per_TU = ((8 * iColor_Depth * iPixel_Clock) / (iNumOfLane * iLink_Rate));

	if (!pstDptx->bMultStreamTransport) {
		dptx_notice("SST %s Pixel => ", pstVideoParams->ucMultiPixel == MULTI_PIXEL_TYPE_SINGLE ? "Single" :
										pstVideoParams->ucMultiPixel == MULTI_PIXEL_TYPE_DUAL ? "Dual" : "Quad");
		dptx_notice(" -.Pixel clock: %d", iPixel_Clock);
		dptx_notice(" -.Num of Lanes: %d ", iNumOfLane);
		dptx_notice(" -.Link rate: %s", iLinkRate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR" :
										(iLinkRate == DPTX_PHYIF_CTRL_RATE_HBR) ?	"HBR" :
										(iLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HB2" : "HBR3");
		dptx_notice(" -.Hblanking: %d", pstDtd->h_blanking);
		dptx_notice(" -.Link Bpc: %d", iLink_Rate);
		dptx_notice(" -.Link Clk: %d", iLink_Clock);
		dptx_notice(" -.Average Symbols per TU: %d <- (( 8 * %d * %d ) / ( %d * %d ))",
										pstVideoParams->ucAverage_BytesPerTu,
										iColor_Depth,
										iPixel_Clock,
										iNumOfLane,
										iLink_Rate);
		dptx_notice(" -.Fraction per TU: %d", pstVideoParams->ucAver_BytesPer_Tu_Frac);
		dptx_notice(" -.Init thresh: %d",	pstVideoParams->ucInit_Threshold);
	}

	if (pstDptx->bMultStreamTransport) {
		u32	uiAverage_BytesPerTu, uiAver_BytesPer_Tu_Frac;
		int iNumerator, iDenominator;

		if (iEncodingType == PIXEL_ENCODING_TYPE_YCBCR422)
			iNumerator	 = (iPixel_Clock * 2 * 64);
		else
			iNumerator	 = (iPixel_Clock * 3 * 64);

		iDenominator = (iLink_Rate * iNumOfLane);

		uiAverage_BytesPerTu = ((iNumerator / iDenominator) / 1000);
		uiAver_BytesPer_Tu_Frac = ((((iNumerator / iDenominator) - (uiAverage_BytesPerTu * 1000)) * 64) / 1000);

		pstVideoParams->ucAverage_BytesPerTu	= (u8)uiAverage_BytesPerTu;
		pstVideoParams->ucAver_BytesPer_Tu_Frac = (u8)uiAver_BytesPer_Tu_Frac;

		dptx_notice("MST %s Pixel : ", pstVideoParams->ucMultiPixel == MULTI_PIXEL_TYPE_SINGLE ? "Single" :
									pstVideoParams->ucMultiPixel == MULTI_PIXEL_TYPE_DUAL ? "Dual" : "Quad");
		dptx_notice(" -.Pixel clock: %d", iPixel_Clock);
		dptx_notice(" -.Num of Lanes: %d", iNumOfLane);
		dptx_notice(" -.Link rate: %s", iLinkRate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR" :
										(iLinkRate == DPTX_PHYIF_CTRL_RATE_HBR) ? "HBR" :
										(iLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HB2" : "HBR3");
		dptx_notice(" -.VCP Id: %d", pstDptx->aucVCP_Id[ucStream_Index]);
		dptx_notice(" -.Pixel clock: %d", iPixel_Clock);
		dptx_notice(" -.Hblanking: %d", pstDtd->h_blanking);
		dptx_notice(" -.Link Bpc: %d", iLink_Rate);
		dptx_notice(" -.Link Clk: %d", iLink_Clock);
		dptx_notice(" -.Average Symbols Per TU: %d ", pstVideoParams->ucAverage_BytesPerTu);
		dptx_notice(" -.Fraction Per TU: %d", pstVideoParams->ucAver_BytesPer_Tu_Frac);
		dptx_notice(" -.Init thresh: %d",	pstVideoParams->ucInit_Threshold);
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_VidIn_Get_VIC_From_Dtd(struct Dptx_Params *pstDptx, u8 ucStream_Index, u32 *puiVideo_Code)
{
	int32_t	iRetVal;
	u32	uiVideoCode;
	struct Dptx_Dtd_Params	stDtd_Params;
	struct Dptx_Dtd_Params	stDtd_Timing;

	iRetVal = Dptx_VidIn_Get_Configured_Timing(pstDptx, ucStream_Index, &stDtd_Timing);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	for (uiVideoCode = 0; uiVideoCode < (u32)DP_CUSTOM_MAX_DTD_VIC;  uiVideoCode++) {
		iRetVal = Dptx_VidIn_Fill_Dtd(&stDtd_Params, uiVideoCode, 60000, (u8)VIDEO_FORMAT_CEA_861);
		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			if ((stDtd_Params.interlaced		== stDtd_Timing.interlaced) &&
				(stDtd_Params.h_sync_polarity	== stDtd_Timing.h_sync_polarity) &&
				(stDtd_Params.h_active			== stDtd_Timing.h_active) &&
				(stDtd_Params.h_blanking		== stDtd_Timing.h_blanking) &&
				(stDtd_Params.h_sync_offset	== stDtd_Timing.h_sync_offset) &&
				(stDtd_Params.h_sync_pulse_width	== stDtd_Timing.h_sync_pulse_width) &&
				(stDtd_Params.v_sync_polarity		== stDtd_Timing.v_sync_polarity) &&
				(stDtd_Params.v_active			== stDtd_Timing.v_active) &&
				(stDtd_Params.v_blanking		== stDtd_Timing.v_blanking) &&
				(stDtd_Params.v_sync_offset	== stDtd_Timing.v_sync_offset) &&
				(stDtd_Params.v_sync_pulse_width	== stDtd_Timing.v_sync_pulse_width)) {
				*puiVideo_Code = uiVideoCode;
				break;
			}
		}
	}

	dptx_notice("CONFIG_DRM_TCC_LCD_VIC uiVideoCode %d", uiVideoCode);
	if (uiVideoCode == DP_CUSTOM_MAX_DTD_VIC) {
#if defined(CONFIG_DRM_TCC_LCD_VIC)  //jbhyun: For TOST
		if (CONFIG_DRM_TCC_LCD_VIC == 0) {
			dptx_notice("Not found VIC from dtd");
			return DPTX_RETURN_NO_ERROR;
		}
#endif
		return DPTX_RETURN_EINVAL;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_VidIn_Fill_Dtd(struct Dptx_Dtd_Params *pstDtd, u32 uiVideo_Code, u32 uiRefreshRate, u8 ucVideoFormat)
{
	dptx_vidin_reset_dtd(pstDtd);

	pstDtd->h_image_size = 16;
	pstDtd->v_image_size = 9;

	if (ucVideoFormat == VIDEO_FORMAT_CEA_861) {
		switch (uiVideo_Code) {
		case DP_CUSTOM_1025_DTD_VIC:
			pstDtd->h_active = 1024;
			pstDtd->v_active = 600;
			pstDtd->h_blanking = 320;
			pstDtd->v_blanking = 35;
			pstDtd->h_sync_offset = 150;
			pstDtd->v_sync_offset = 15;
			pstDtd->h_sync_pulse_width = 20;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 51200;
			break;
		case DP_CUSTOM_1026_DTD_VIC:
			pstDtd->h_active = 5760;
			pstDtd->v_active = 900;
			pstDtd->h_blanking = 160;
			pstDtd->v_blanking = 26;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 297000;
			break;
		case DP_CUSTOM_1027_DTD_VIC:
			pstDtd->h_active = 1920;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 64;
			pstDtd->v_blanking = 21;
			pstDtd->h_sync_offset = 30;
			pstDtd->v_sync_offset = 10;
			pstDtd->h_sync_pulse_width = 4;
			pstDtd->v_sync_pulse_width = 2;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 88200;
			break;
		case 1: /* 640x480p @ 59.94/60Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 640;
			pstDtd->v_active = 480;
			pstDtd->h_blanking = 160;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 16;
			pstDtd->v_sync_offset = 10;
			pstDtd->h_sync_pulse_width = 96;
			pstDtd->v_sync_pulse_width = 2;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 25175;
			break;
		case 2: /* 720x480p @ 59.94/60Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 720;
			pstDtd->v_active = 480;
			pstDtd->h_blanking = 138;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 16;
			pstDtd->v_sync_offset = 9;
			pstDtd->h_sync_pulse_width = 62;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 27000;
			break;
		case 3: /* 720x480p @ 59.94/60Hz 16:9 */
			pstDtd->h_active = 720;
			pstDtd->v_active = 480;
			pstDtd->h_blanking = 138;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 16;
			pstDtd->v_sync_offset = 9;
			pstDtd->h_sync_pulse_width = 62;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 27000;
			break;
		case 69:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 370;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 110;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 4: /* 1280x720p @ 59.94/60Hz 16:9 */
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 370;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 110;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 5: /* 1920x1080i @ 59.94/60Hz 16:9 */
			pstDtd->h_active = 1920;
			pstDtd->v_active = 540;
			pstDtd->h_blanking = 280;
			pstDtd->v_blanking = 22;
			pstDtd->h_sync_offset = 88;
			pstDtd->v_sync_offset = 2;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 6: /* 720(1440)x480i @ 59.94/60Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1440;
			pstDtd->v_active = 240;
			pstDtd->h_blanking = 276;
			pstDtd->v_blanking = 22;
			pstDtd->h_sync_offset = 38;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 124;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 27000;
			break;
		case 7: /* 720(1440)x480i @ 59.94/60Hz 16:9 */
			pstDtd->h_active = 1440;
			pstDtd->v_active = 240;
			pstDtd->h_blanking = 276;
			pstDtd->v_blanking = 22;
			pstDtd->h_sync_offset = 38;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 124;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 27000;
			break;
		case 8: /* 720(1440)x240p @ 59.826/60.054/59.886/60.115Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1440;
			pstDtd->v_active = 240;
			pstDtd->h_blanking = 276;
			pstDtd->v_blanking = (uiRefreshRate == 59940) ? 22 : 23;
			pstDtd->h_sync_offset = 38;
			pstDtd->v_sync_offset = (uiRefreshRate == 59940) ? 4 : 5;
			pstDtd->h_sync_pulse_width = 124;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 27000;
			break;
		case 9: /* 720(1440)x240p @59.826/60.054/59.886/60.115Hz 16:9 */
			pstDtd->h_active = 1440;
			pstDtd->v_active = 240;
			pstDtd->h_blanking = 276;
			pstDtd->v_blanking = (uiRefreshRate == 59940) ? 22 : 23;
			pstDtd->h_sync_offset = 38;
			pstDtd->v_sync_offset = (uiRefreshRate == 59940) ? 4 : 5;
			pstDtd->h_sync_pulse_width = 124;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 27000;
			break;
		case 10: /* 2880x480i @ 59.94/60Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 2880;
			pstDtd->v_active = 240;
			pstDtd->h_blanking = 552;
			pstDtd->v_blanking = 22;
			pstDtd->h_sync_offset = 76;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 248;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 11: /* 2880x480i @ 59.94/60Hz 16:9 */
			pstDtd->h_active = 2880;
			pstDtd->v_active = 240;
			pstDtd->h_blanking = 552;
			pstDtd->v_blanking = 22;
			pstDtd->h_sync_offset = 76;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 248;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 12: /* 2880x240p @ 59.826/60.054/59.886/60.115Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 2880;
			pstDtd->v_active = 240;
			pstDtd->h_blanking = 552;
			pstDtd->v_blanking = (uiRefreshRate == 60054) ? 22 : 23;
			pstDtd->h_sync_offset = 76;
			pstDtd->v_sync_offset = (uiRefreshRate == 60054) ? 4 : 5;
			pstDtd->h_sync_pulse_width = 248;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 13: /* 2880x240p @ 59.826/60.054/59.886/60.115Hz 16:9 */
			pstDtd->h_active = 2880;
			pstDtd->v_active = 240;
			pstDtd->h_blanking = 552;
			pstDtd->v_blanking = (uiRefreshRate == 60054) ? 22 : 23;
			pstDtd->h_sync_offset = 76;
			pstDtd->v_sync_offset = (uiRefreshRate == 60054) ? 4 : 5;
			pstDtd->h_sync_pulse_width = 248;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 14: /* 1440x480p @ 59.94/60Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1440;
			pstDtd->v_active = 480;
			pstDtd->h_blanking = 276;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 32;
			pstDtd->v_sync_offset = 9;
			pstDtd->h_sync_pulse_width = 124;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 15: /* 1440x480p @ 59.94/60Hz 16:9 */
			pstDtd->h_active = 1440;
			pstDtd->v_active = 480;
			pstDtd->h_blanking = 276;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 32;
			pstDtd->v_sync_offset = 9;
			pstDtd->h_sync_pulse_width = 124;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 76:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 280;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 88;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 148500;
			break;
		case 16: /* 1920x1080p @ 59.94/60Hz 16:9 */
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 280;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 88;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 148500;
			break;
		case 17: /* 720x576p @ 50Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 720;
			pstDtd->v_active = 576;
			pstDtd->h_blanking = 144;
			pstDtd->v_blanking = 49;
			pstDtd->h_sync_offset = 12;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 64;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 27000;
			break;
		case 18: /* 720x576p @ 50Hz 16:9 */
			pstDtd->h_active = 720;
			pstDtd->v_active = 576;
			pstDtd->h_blanking = 144;
			pstDtd->v_blanking = 49;
			pstDtd->h_sync_offset = 12;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 64;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 27000;
			break;
		case 68:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 700;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 440;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 19: /* 1280x720p @ 50Hz 16:9 */
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 700;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 440;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 20: /* 1920x1080i @ 50Hz 16:9 */
			pstDtd->h_active = 1920;
			pstDtd->v_active = 540;
			pstDtd->h_blanking = 720;
			pstDtd->v_blanking = 22;
			pstDtd->h_sync_offset = 528;
			pstDtd->v_sync_offset = 2;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 21: /* 720(1440)x576i @ 50Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1440;
			pstDtd->v_active = 288;
			pstDtd->h_blanking = 288;
			pstDtd->v_blanking = 24;
			pstDtd->h_sync_offset = 24;
			pstDtd->v_sync_offset = 2;
			pstDtd->h_sync_pulse_width = 126;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 27000;
			break;
		case 22: /* 720(1440)x576i @ 50Hz 16:9 */
			pstDtd->h_active = 1440;
			pstDtd->v_active = 288;
			pstDtd->h_blanking = 288;
			pstDtd->v_blanking = 24;
			pstDtd->h_sync_offset = 24;
			pstDtd->v_sync_offset = 2;
			pstDtd->h_sync_pulse_width = 126;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 27000;
			break;
		case 23: /* 720(1440)x288p @ 50Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1440;
			pstDtd->v_active = 288;
			pstDtd->h_blanking = 288;
			pstDtd->v_blanking = (uiRefreshRate == 50080) ? 24 : ((uiRefreshRate == 49920) ? 25 : 26);
			pstDtd->h_sync_offset = 24;
			pstDtd->v_sync_offset = (uiRefreshRate == 50080) ? 2 : ((uiRefreshRate == 49920) ? 3 : 4);
			pstDtd->h_sync_pulse_width = 126;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 27000;
			break;
		case 24: /* 720(1440)x288p @ 50Hz 16:9 */
			pstDtd->h_active = 1440;
			pstDtd->v_active = 288;
			pstDtd->h_blanking = 288;
			pstDtd->v_blanking = (uiRefreshRate == 50080) ? 24 : ((uiRefreshRate == 49920) ? 25 : 26);
			pstDtd->h_sync_offset = 24;
			pstDtd->v_sync_offset = (uiRefreshRate == 50080) ? 2 : ((uiRefreshRate == 49920) ? 3 : 4);
			pstDtd->h_sync_pulse_width = 126;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 27000;
			break;
		case 25: /* 2880x576i @ 50Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 2880;
			pstDtd->v_active = 288;
			pstDtd->h_blanking = 576;
			pstDtd->v_blanking = 24;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 2;
			pstDtd->h_sync_pulse_width = 252;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 26: /* 2880x576i @ 50Hz 16:9 */
			pstDtd->h_active = 2880;
			pstDtd->v_active = 288;
			pstDtd->h_blanking = 576;
			pstDtd->v_blanking = 24;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 2;
			pstDtd->h_sync_pulse_width = 252;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 27: /* 2880x288p @ 50Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 2880;
			pstDtd->v_active = 288;
			pstDtd->h_blanking = 576;
			pstDtd->v_blanking = (uiRefreshRate == 50080) ? 24 : ((uiRefreshRate == 49920) ? 25 : 26);
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = (uiRefreshRate == 50080) ? 2 : ((uiRefreshRate == 49920) ? 3 : 4);
			pstDtd->h_sync_pulse_width = 252;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 28: /* 2880x288p @ 50Hz 16:9 */
			pstDtd->h_active = 2880;
			pstDtd->v_active = 288;
			pstDtd->h_blanking = 576;
			pstDtd->v_blanking = (uiRefreshRate == 50080) ? 24 : ((uiRefreshRate == 49920) ? 25 : 26);
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = (uiRefreshRate == 50080) ? 2 : ((uiRefreshRate == 49920) ? 3 : 4);
			pstDtd->h_sync_pulse_width = 252;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 29: /* 1440x576p @ 50Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1440;
			pstDtd->v_active = 576;
			pstDtd->h_blanking = 288;
			pstDtd->v_blanking = 49;
			pstDtd->h_sync_offset = 24;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 128;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 30: /* 1440x576p @ 50Hz 16:9 */
			pstDtd->h_active = 1440;
			pstDtd->v_active = 576;
			pstDtd->h_blanking = 288;
			pstDtd->v_blanking = 49;
			pstDtd->h_sync_offset = 24;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 128;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 75:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 720;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 528;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 148500;
			break;
		case 31: /* 1920x1080p @ 50Hz 16:9 */
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 720;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 528;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 148500;
			break;
		case 72:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 830;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 638;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 32: /* 1920x1080p @ 23.976/24Hz 16:9 */
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 830;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 638;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 73:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 720;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 528;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 33: /* 1920x1080p @ 25Hz 16:9 */
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 720;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 528;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 74:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 280;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 88;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 34: /* 1920x1080p @ 29.97/30Hz 16:9 */
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 280;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 88;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 35: /* 2880x480p @ 60Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 2880;
			pstDtd->v_active = 480;
			pstDtd->h_blanking = 552;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 64;
			pstDtd->v_sync_offset = 9;
			pstDtd->h_sync_pulse_width = 248;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 36: /* 2880x480p @ 60Hz 16:9 */
			pstDtd->h_active = 2880;
			pstDtd->v_active = 480;
			pstDtd->h_blanking = 552;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 64;
			pstDtd->v_sync_offset = 9;
			pstDtd->h_sync_pulse_width = 248;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 37: /* 2880x576p @ 50Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 2880;
			pstDtd->v_active = 576;
			pstDtd->h_blanking = 576;
			pstDtd->v_blanking = 49;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 256;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 38: /* 2880x576p @ 50Hz 16:9 */
			pstDtd->h_active = 2880;
			pstDtd->v_active = 576;
			pstDtd->h_blanking = 576;
			pstDtd->v_blanking = 49;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 256;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 39: /* 1920x1080i (1250 total) @ 50Hz 16:9 */
			pstDtd->h_active = 1920;
			pstDtd->v_active = 540;
			pstDtd->h_blanking = 384;
			pstDtd->v_blanking = 85;
			pstDtd->h_sync_offset = 32;
			pstDtd->v_sync_offset = 23;
			pstDtd->h_sync_pulse_width = 168;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 72000;
			break;
		case 40: /* 1920x1080i @ 100Hz 16:9 */
			pstDtd->h_active = 1920;
			pstDtd->v_active = 540;
			pstDtd->h_blanking = 720;
			pstDtd->v_blanking = 22;
			pstDtd->h_sync_offset = 528;
			pstDtd->v_sync_offset = 2;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 148500;
			break;
		case 70:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 700;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 440;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 148500;
			break;
		case 41: /* 1280x720p @ 100Hz 16:9 */
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 700;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 440;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 148500;
			break;
		case 42: /* 720x576p @ 100Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 720;
			pstDtd->v_active = 576;
			pstDtd->h_blanking = 144;
			pstDtd->v_blanking = 49;
			pstDtd->h_sync_offset = 12;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 64;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 43: /* 720x576p @ 100Hz 16:9 */
			pstDtd->h_active = 720;
			pstDtd->v_active = 576;
			pstDtd->h_blanking = 144;
			pstDtd->v_blanking = 49;
			pstDtd->h_sync_offset = 12;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 64;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 44: /* 720(1440)x576i @ 100Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1440;
			pstDtd->v_active = 288;
			pstDtd->h_blanking = 288;
			pstDtd->v_blanking = 24;
			pstDtd->h_sync_offset = 24;
			pstDtd->v_sync_offset = 2;
			pstDtd->h_sync_pulse_width = 126;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 45: /* 720(1440)x576i @ 100Hz 16:9 */
			pstDtd->h_active = 1440;
			pstDtd->v_active = 288;
			pstDtd->h_blanking = 288;
			pstDtd->v_blanking = 24;
			pstDtd->h_sync_offset = 24;
			pstDtd->v_sync_offset = 2;
			pstDtd->h_sync_pulse_width = 126;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 46: /* 1920x1080i @ 119.88/120Hz 16:9 */
			pstDtd->h_active = 1920;
			pstDtd->v_active = 540;
			pstDtd->h_blanking = 288;
			pstDtd->v_blanking = 22;
			pstDtd->h_sync_offset = 88;
			pstDtd->v_sync_offset = 2;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 148500;
			break;
		case 71:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 370;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 110;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 148500;
			break;
		case 47: /* 1280x720p @ 119.88/120Hz 16:9 */
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 370;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 110;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 148500;
			break;
		case 48: /* 720x480p @ 119.88/120Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 720;
			pstDtd->v_active = 480;
			pstDtd->h_blanking = 138;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 16;
			pstDtd->v_sync_offset = 9;
			pstDtd->h_sync_pulse_width = 62;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 49: /* 720x480p @ 119.88/120Hz 16:9 */
			pstDtd->h_active = 720;
			pstDtd->v_active = 480;
			pstDtd->h_blanking = 138;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 16;
			pstDtd->v_sync_offset = 9;
			pstDtd->h_sync_pulse_width = 62;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 50: /* 720(1440)x480i @ 119.88/120Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1440;
			pstDtd->v_active = 240;
			pstDtd->h_blanking = 276;
			pstDtd->v_blanking = 22;
			pstDtd->h_sync_offset = 38;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 124;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 51: /* 720(1440)x480i @ 119.88/120Hz 16:9 */
			pstDtd->h_active = 1440;
			pstDtd->v_active = 240;
			pstDtd->h_blanking = 276;
			pstDtd->v_blanking = 22;
			pstDtd->h_sync_offset = 38;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 124;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 54000;
			break;
		case 52: /* 720X576p @ 200Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 720;
			pstDtd->v_active = 576;
			pstDtd->h_blanking = 144;
			pstDtd->v_blanking = 49;
			pstDtd->h_sync_offset = 12;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 64;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 53: /* 720X576p @ 200Hz 16:9 */
			pstDtd->h_active = 720;
			pstDtd->v_active = 576;
			pstDtd->h_blanking = 144;
			pstDtd->v_blanking = 49;
			pstDtd->h_sync_offset = 12;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 64;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 54: /* 720(1440)x576i @ 200Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1440;
			pstDtd->v_active = 288;
			pstDtd->h_blanking = 288;
			pstDtd->v_blanking = 24;
			pstDtd->h_sync_offset = 24;
			pstDtd->v_sync_offset = 2;
			pstDtd->h_sync_pulse_width = 126;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 55: /* 720(1440)x576i @ 200Hz 16:9 */
			pstDtd->h_active = 1440;
			pstDtd->v_active = 288;
			pstDtd->h_blanking = 288;
			pstDtd->v_blanking = 24;
			pstDtd->h_sync_offset = 24;
			pstDtd->v_sync_offset = 2;
			pstDtd->h_sync_pulse_width = 126;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 56: /* 720x480p @ 239.76/240Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 720;
			pstDtd->v_active = 480;
			pstDtd->h_blanking = 138;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 16;
			pstDtd->v_sync_offset = 9;
			pstDtd->h_sync_pulse_width = 62;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 57: /* 720x480p @ 239.76/240Hz 16:9 */
			pstDtd->h_active = 720;
			pstDtd->v_active = 480;
			pstDtd->h_blanking = 138;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 16;
			pstDtd->v_sync_offset = 9;
			pstDtd->h_sync_pulse_width = 62;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 58: /* 720(1440)x480i @ 239.76/240Hz 4:3 */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1440;
			pstDtd->v_active = 240;
			pstDtd->h_blanking = 276;
			pstDtd->v_blanking = 22;
			pstDtd->h_sync_offset = 38;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 124;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 59: /* 720(1440)x480i @ 239.76/240Hz 16:9 */
			pstDtd->h_active = 1440;
			pstDtd->v_active = 240;
			pstDtd->h_blanking = 276;
			pstDtd->v_blanking = 22;
			pstDtd->h_sync_offset = 38;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 124;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 1;
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 65:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 2020;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 1760;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 594000;
			break;
		case 60: /* 1280x720p @ 23.97/24Hz 16:9 */
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 2020;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 1760;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 594000;
			break;
		case 66:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 2680;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 2420;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 61: /* 1280x720p @ 25Hz 16:9 */
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 2680;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 2420;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 67:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 2020;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 1760;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 62: /* 1280x720p @ 29.97/30Hz  16:9 */
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 2020;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 1760;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 74250;
			break;
		case 78:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 280;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 88;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 297000;
			break;
		case 63: /* 1920x1080p @ 119.88/120Hz 16:9 */
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 280;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 88;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 297000;
			break;
		case 77:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 720;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 528;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 297000;
			break;
		case 64: /* 1920x1080p @ 100Hz 16:9 */
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 720;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 528;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 297000;
			break;
		case 79:
			pstDtd->h_active = 1680;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 1620;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 1360;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 594000;
			break;
		case 80:
			pstDtd->h_active = 1680;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 1488;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 1228;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 594000;
			break;
		case 81:
			pstDtd->h_active = 1680;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 960;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 700;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 594000;
			break;
		case 82:
			pstDtd->h_active = 1680;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 520;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 260;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 82500;
			break;
		case 83:
			pstDtd->h_active = 1680;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 520;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 260;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 99000;
			break;
		case 84:
			pstDtd->h_active = 1680;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 320;
			pstDtd->v_blanking = 105;
			pstDtd->h_sync_offset = 60;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 165000;
			break;
		case 85:
			pstDtd->h_active = 1680;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 320;
			pstDtd->v_blanking = 105;
			pstDtd->h_sync_offset = 60;
			pstDtd->v_sync_offset = 5;
			pstDtd->h_sync_pulse_width = 40;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 198000;
			break;
		case 86:
			pstDtd->h_active = 2560;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 1190;
			pstDtd->v_blanking = 20;
			pstDtd->h_sync_offset = 998;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 99000;
			break;
		case 87:
			pstDtd->h_active = 2560;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 640;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 448;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 90000;
			break;
		case 88:
			pstDtd->h_active = 2560;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 960;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 768;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 118800;
			break;
		case 89:
			pstDtd->h_active = 2560;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 740;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 548;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 185625;
			break;
		case 90:
			pstDtd->h_active = 2560;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 440;
			pstDtd->v_blanking = 20;
			pstDtd->h_sync_offset = 248;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 198000;
			break;
		case 91:
			pstDtd->h_active = 2560;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 410;
			pstDtd->v_blanking = 170;
			pstDtd->h_sync_offset = 218;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 371250;
			break;
		case 92:
			pstDtd->h_active = 2560;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 740;
			pstDtd->v_blanking = 170;
			pstDtd->h_sync_offset = 548;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 495000;
			break;
		case 101:
			pstDtd->h_active = 4096;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 1184;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 968;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 59400;
			break;
		case 100:
			pstDtd->h_active = 4096;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 304;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 88;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 297000;
			break;
		case 99:
			pstDtd->h_active = 4096;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 1184;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 968;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 297000;
			break;
		case 102:
			pstDtd->h_active = 4096;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 304;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 88;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 594000;
			break;
		case 103:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 3840;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 1660;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 1276;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 297000;
			break;
		case 93:		/* 4k x 2k, 30Hz */
			pstDtd->h_active = 3840;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 1660;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 1276;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 297000;
			break;
		case 104:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 3840;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 1440;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 1056;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 297000;
			break;
		case 94:
			pstDtd->h_active = 3840;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 1440;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 1056;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 297000;
			break;
		case 105:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 3840;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 560;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 176;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 297000;
			break;
		case 95:
			pstDtd->h_active = 3840;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 560;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 176;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 297000;
			break;
		case 106:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 3840;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 1440;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 1056;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 594000;
			break;
		case 96:
			pstDtd->h_active = 3840;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 1440;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 1056;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 594000;
			break;
		case 107:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 3840;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 560;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 176;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 594000;
			break;
		case 97:
			pstDtd->h_active = 3840;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 560;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 176;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 594000;
			break;
		case 98:
			pstDtd->h_active = 4096;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 1404;
			pstDtd->v_blanking = 90;
			pstDtd->h_sync_offset = 1020;
			pstDtd->v_sync_offset = 8;
			pstDtd->h_sync_pulse_width = 88;
			pstDtd->v_sync_pulse_width = 10;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;
			pstDtd->uiPixel_Clock = 297000;
			break;
		default:
			//dptx_err("Invalid video code = %d ", uiVideo_Code );
			return DPTX_RETURN_EINVAL;
	}
	}  else if (ucVideoFormat == VIDEO_FORMAT_VESA_CVT)  {
		switch (uiVideo_Code) {
		case 1:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 640;
			pstDtd->v_active = 480;
			pstDtd->h_blanking = 160;
			pstDtd->v_blanking = 20;
			pstDtd->h_sync_offset = 8;
			pstDtd->v_sync_offset = 1;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 8;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 23750;
			break;
		case 2:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 800;
			pstDtd->v_active = 600;
			pstDtd->h_blanking = 224;
			pstDtd->v_blanking = 24;
			pstDtd->h_sync_offset = 31;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 81;
			pstDtd->v_sync_pulse_width = 4;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 38250;
			break;
		case 3:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1024;
			pstDtd->v_active = 768;
			pstDtd->h_blanking = 304;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 104;
			pstDtd->v_sync_pulse_width = 4;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 63500;
			break;
		case 4:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 960;
			pstDtd->h_blanking = 416;
			pstDtd->v_blanking = 36;
			pstDtd->h_sync_offset = 80;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 128;
			pstDtd->v_sync_pulse_width = 4;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 101250;
			break;
		case 5:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1400;
			pstDtd->v_active = 1050;
			pstDtd->h_blanking = 464;
			pstDtd->v_blanking = 39;
			pstDtd->h_sync_offset = 88;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 144;
			pstDtd->v_sync_pulse_width = 4;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 121750;
			break;
		case 6:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1600;
			pstDtd->v_active = 1200;
			pstDtd->h_blanking = 560;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 112;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 68;
			pstDtd->v_sync_pulse_width = 4;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 161000;
			break;
		case 12:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 1024;
			pstDtd->h_blanking = 432;
			pstDtd->v_blanking = 39;
			pstDtd->h_sync_offset = 80;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 136;
			pstDtd->v_sync_pulse_width = 7;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 109000;
			break;
		case 13:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 768;
			pstDtd->h_blanking = 384;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 64;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 128;
			pstDtd->v_sync_pulse_width = 7;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 79500;
			break;
		case 16:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 720;
			pstDtd->h_blanking = 384;
			pstDtd->v_blanking = 28;
			pstDtd->h_sync_offset = 64;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 128;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 74500;
			break;
		case 17:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1360;
			pstDtd->v_active = 768;
			pstDtd->h_blanking = 416;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 72;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 136;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 84750;
			break;
		case 20:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 656;
			pstDtd->v_blanking = 40;
			pstDtd->h_sync_offset = 128;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 200;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 173000;
			break;
		case 22:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 2560;
			pstDtd->v_active = 1440;
			pstDtd->h_blanking = 928;
			pstDtd->v_blanking = 53;
			pstDtd->h_sync_offset = 192;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 272;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 312250;
			break;
		case 28:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 800;
			pstDtd->h_blanking = 400;
			pstDtd->v_blanking = 31;
			pstDtd->h_sync_offset = 72;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 128;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 83500;
			break;
		case 34:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1200;
			pstDtd->h_blanking = 672;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 136;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 200;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 193250;
			break;
		case 38:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 3840;
			pstDtd->v_active = 2400;
			pstDtd->h_blanking = 80;
			pstDtd->v_blanking = 69;
			pstDtd->h_sync_offset = 320;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 424;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 580128;
			break;
		case 40:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1600;
			pstDtd->v_active = 1200;
			pstDtd->h_blanking = 160;
			pstDtd->v_blanking = 35;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 4;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 124076;
			break;
		case 41:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 2048;
			pstDtd->v_active = 1536;
			pstDtd->h_blanking = 160;
			pstDtd->v_blanking = 44;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 4;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 208000;
			break;
		default:
			//dptx_err("Invalid video code = %d\n", uiVideo_Code);
			return DPTX_RETURN_EINVAL;
		}
	}  else if (ucVideoFormat == VIDEO_FORMAT_VESA_DMT) {
		switch (uiVideo_Code) {
		case 1: // HISilicon timing
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 3600;
			pstDtd->v_active = 1800;
			pstDtd->h_blanking = 120;
			pstDtd->v_blanking = 128;
			pstDtd->h_sync_offset = 20;
			pstDtd->v_sync_offset = 2;
			pstDtd->h_sync_pulse_width = 20;
			pstDtd->v_sync_pulse_width = 2;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 645500;
			break;
		case 2:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 3840;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 160;
			pstDtd->v_blanking = 62;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 533000;
			break;
		case 4:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 640;
			pstDtd->v_active = 480;
			pstDtd->h_blanking = 144;
			pstDtd->v_blanking = 29;
			pstDtd->h_sync_offset = 8;
			pstDtd->v_sync_offset = 2;
			pstDtd->h_sync_pulse_width = 96;
			pstDtd->v_sync_pulse_width = 2;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 25175;
			break;
		case 13:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 800;
			pstDtd->v_active = 600;
			pstDtd->h_blanking = 160;
			pstDtd->v_blanking = 36;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 4;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 73250;
			break;
		case 14: /* 848x480p@60Hz */
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 848;
			pstDtd->v_active = 480;
			pstDtd->h_blanking = 240;
			pstDtd->v_blanking = 37;
			pstDtd->h_sync_offset = 16;
			pstDtd->v_sync_offset = 6;
			pstDtd->h_sync_pulse_width = 112;
			pstDtd->v_sync_pulse_width = 8;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI)  */;
			pstDtd->uiPixel_Clock = 33750;
			break;
		case 22:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 768;
			pstDtd->h_blanking = 160;
			pstDtd->v_blanking = 22;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 7;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 68250;
			break;
		case 35:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 1024;
			pstDtd->h_blanking = 408;
			pstDtd->v_blanking = 42;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 1;
			pstDtd->h_sync_pulse_width = 112;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 39:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1360;
			pstDtd->v_active = 768;
			pstDtd->h_blanking = 432;
			pstDtd->v_blanking = 27;
			pstDtd->h_sync_offset = 64;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 112;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 85500;
			break;
		case 40:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1360;
			pstDtd->v_active = 768;
			pstDtd->h_blanking = 160;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 148250;
			break;
		case 81:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1366;
			pstDtd->v_active = 768;
			pstDtd->h_blanking = 426;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 70;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 142;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 85500;
			break;
		case 86:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1366;
			pstDtd->v_active = 768;
			pstDtd->h_blanking = 134;
			pstDtd->v_blanking = 32;
			pstDtd->h_sync_offset = 14;
			pstDtd->v_sync_offset = 1;
			pstDtd->h_sync_pulse_width = 56;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 72000;
			break;
		case 87:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 4096;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 80;
			pstDtd->v_blanking = 62;
			pstDtd->h_sync_offset = 8;
			pstDtd->v_sync_offset = 48;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 8;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 556744;
			break;
		case 88:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 4096;
			pstDtd->v_active = 2160;
			pstDtd->h_blanking = 80;
			pstDtd->v_blanking = 62;
			pstDtd->h_sync_offset = 8;
			pstDtd->v_sync_offset = 48;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 8;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 556188;
			break;
		case 41:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1400;
			pstDtd->v_active = 1050;
			pstDtd->h_blanking = 160;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 4;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 101000;
			break;
		case 42:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1400;
			pstDtd->v_active = 1050;
			pstDtd->h_blanking = 464;
			pstDtd->v_blanking = 39;
			pstDtd->h_sync_offset = 88;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 144;
			pstDtd->v_sync_pulse_width = 4;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 121750;
			break;
		case 46:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1440;
			pstDtd->v_active = 900;
			pstDtd->h_blanking = 160;
			pstDtd->v_blanking = 26;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 88750;
			break;
		case 47:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1440;
			pstDtd->v_active = 900;
			pstDtd->h_blanking = 464;
			pstDtd->v_blanking = 34;
			pstDtd->h_sync_offset = 80;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 152;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 106500;
			break;
		case 51:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1600;
			pstDtd->v_active = 1200;
			pstDtd->h_blanking = 560;
			pstDtd->v_blanking = 50;
			pstDtd->h_sync_offset = 64;
			pstDtd->v_sync_offset = 1;
			pstDtd->h_sync_pulse_width = 192;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 162000;
			break;
		case 57:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1680;
			pstDtd->v_active = 1050;
			pstDtd->h_blanking = 160;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 119000;
			break;
		case 58:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1680;
			pstDtd->v_active = 1050;
			pstDtd->h_blanking = 560;
			pstDtd->v_blanking = 39;
			pstDtd->h_sync_offset = 104;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 176;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 146250;
			break;
		case 68:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1200;
			pstDtd->h_blanking = 160;
			pstDtd->v_blanking = 35;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 154000;
			break;
		case 69:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1200;
			pstDtd->h_blanking = 672;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 136;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 200;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 193250;
			break;
		case 82:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1080;
			pstDtd->h_blanking = 280;
			pstDtd->v_blanking = 45;
			pstDtd->h_sync_offset = 88;
			pstDtd->v_sync_offset = 4;
			pstDtd->h_sync_pulse_width = 44;
			pstDtd->v_sync_pulse_width = 5;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 148500;
			break;
		case 83:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1600;
			pstDtd->v_active = 900;
			pstDtd->h_blanking = 200;
			pstDtd->v_blanking = 100;
			pstDtd->h_sync_offset = 24;
			pstDtd->v_sync_offset = 1;
			pstDtd->h_sync_pulse_width = 80;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 9:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 800;
			pstDtd->v_active = 600;
			pstDtd->h_blanking = 256;
			pstDtd->v_blanking = 28;
			pstDtd->h_sync_offset = 40;
			pstDtd->v_sync_offset = 1;
			pstDtd->h_sync_pulse_width = 128;
			pstDtd->v_sync_pulse_width = 4;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 40000;
			break;
		case 16:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1024;
			pstDtd->v_active = 768;
			pstDtd->h_blanking = 320;
			pstDtd->v_blanking = 38;
			pstDtd->h_sync_offset = 24;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 136;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 65000;
			break;
		case 23:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 768;
			pstDtd->h_blanking = 384;
			pstDtd->v_blanking = 30;
			pstDtd->h_sync_offset = 64;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 128;
			pstDtd->v_sync_pulse_width = 7;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 79500;
			break;
		case 62:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active =  1792;
			pstDtd->v_active = 1344;
			pstDtd->h_blanking = 656;
			pstDtd->v_blanking =  50;
			pstDtd->h_sync_offset = 128;
			pstDtd->v_sync_offset = 1;
			pstDtd->h_sync_pulse_width = 200;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0; /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 204750;
			break;
		case 32:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 960;
			pstDtd->h_blanking = 520;
			pstDtd->v_blanking = 40;
			pstDtd->h_sync_offset = 96;
			pstDtd->v_sync_offset = 1;
			pstDtd->h_sync_pulse_width = 112;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;  /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 108000;
			break;
		case 73:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1920;
			pstDtd->v_active = 1440;
			pstDtd->h_blanking = 680;
			pstDtd->v_blanking = 60;
			pstDtd->h_sync_offset = 128;
			pstDtd->v_sync_offset = 1;
			pstDtd->h_sync_pulse_width = 208;
			pstDtd->v_sync_pulse_width = 3;
			pstDtd->h_sync_polarity = 0;
			pstDtd->v_sync_polarity = 1;
			pstDtd->interlaced = 0;  /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 234000;
			break;
		case 27:
			pstDtd->h_image_size = 4;
			pstDtd->v_image_size = 3;
			pstDtd->h_active = 1280;
			pstDtd->v_active = 800;
			pstDtd->h_blanking = 160;
			pstDtd->v_blanking = 23;
			pstDtd->h_sync_offset = 48;
			pstDtd->v_sync_offset = 3;
			pstDtd->h_sync_pulse_width = 32;
			pstDtd->v_sync_pulse_width = 6;
			pstDtd->h_sync_polarity = 1;
			pstDtd->v_sync_polarity = 0;
			pstDtd->interlaced = 0;  /* (progressive_nI) */
			pstDtd->uiPixel_Clock = 71000;
			break;
		default:
			//dptx_err("Invalid video code = %d ", uiVideo_Code );
			return DPTX_RETURN_EINVAL;
		}
	} else {
		dptx_err("Invalid video format = %d\n", ucVideoFormat);
		return DPTX_RETURN_EINVAL;
	}

	return DPTX_RETURN_NO_ERROR;
}




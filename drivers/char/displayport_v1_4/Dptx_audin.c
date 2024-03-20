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

#define	IEC60958_3_CH_STATUS_MODE0_LEFT							0x80
#define	IEC60958_3_CH_STATUS_MODE0_RIGHT						0x40

#define	IEC60958_3_CH_STATUS_MAX_WORD_LENGTH_20BITS				0x00
#define	IEC60958_3_CH_STATUS_MAX_WORD_LENGTH_24BITS				0x08

#define	IEC60958_3_CH_STATUS_SAMPLE_WORD_LENGTH_20BITS			0x04
#define	IEC60958_3_CH_STATUS_SAMPLE_WORD_LENGTH_21BITS			0x03
#define	IEC60958_3_CH_STATUS_SAMPLE_WORD_LENGTH_22BITS			0x02
#define	IEC60958_3_CH_STATUS_SAMPLE_WORD_LENGTH_23BITS			0x01
#define	IEC60958_3_CH_STATUS_SAMPLE_WORD_LENGTH_24BITS			0x05

#define AUDIO_INFOFREAME_HEADER				0x481B8400

static void dptx_audin_active_audio_channels(struct Dptx_Params *dptx, uint8_t ucStream_Index, uint8_t ucNumOfChannels, uint8_t ucEnable)
{
	u32 uiEnabledChannels = 0, uiRegMap_EnableAudioChannels = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_EnableAudioChannels = Dptx_Reg_Readl(dptx, uiReg_Offset);
	uiRegMap_EnableAudioChannels &= ~DPTX_AUD_CONFIG1_DATA_EN_IN_MASK;

	if (ucEnable) {
		switch (ucNumOfChannels) {
		case INPUT_MAX_1_CH:
			uiEnabledChannels = DPTX_EN_AUDIO_CH_1;
			break;
		case INPUT_MAX_2_CH:
			uiEnabledChannels = DPTX_EN_AUDIO_CH_2;
			break;
		case INPUT_MAX_3_CH:
			uiEnabledChannels = DPTX_EN_AUDIO_CH_3;
			break;
		case INPUT_MAX_4_CH:
			uiEnabledChannels = DPTX_EN_AUDIO_CH_4;
			break;
		case INPUT_MAX_5_CH:
			uiEnabledChannels = DPTX_EN_AUDIO_CH_5;
			break;
		case INPUT_MAX_6_CH:
			uiEnabledChannels = DPTX_EN_AUDIO_CH_6;
			break;
		case INPUT_MAX_7_CH:
			uiEnabledChannels = DPTX_EN_AUDIO_CH_7;
			break;
		case INPUT_MAX_8_CH:
		default:
			uiEnabledChannels = DPTX_EN_AUDIO_CH_8;
			break;
		}
		uiRegMap_EnableAudioChannels |= (uiEnabledChannels << DPTX_AUD_CONFIG1_DATA_EN_IN_SHIFT);
	} else {
		switch (ucNumOfChannels) {
		case INPUT_MAX_1_CH:
			uiEnabledChannels = ~DPTX_EN_AUDIO_CH_1;
			break;
		case INPUT_MAX_2_CH:
			uiEnabledChannels = ~DPTX_EN_AUDIO_CH_2;
			break;
		case INPUT_MAX_3_CH:
			uiEnabledChannels = ~DPTX_EN_AUDIO_CH_3;
			break;
		case INPUT_MAX_4_CH:
			uiEnabledChannels = ~DPTX_EN_AUDIO_CH_4;
			break;
		case INPUT_MAX_5_CH:
			uiEnabledChannels = ~DPTX_EN_AUDIO_CH_5;
			break;
		case INPUT_MAX_6_CH:
			uiEnabledChannels = ~DPTX_EN_AUDIO_CH_6;
			break;
		case INPUT_MAX_7_CH:
			uiEnabledChannels = ~DPTX_EN_AUDIO_CH_7;
			break;
		case INPUT_MAX_8_CH:
		default:
			uiEnabledChannels = ~DPTX_EN_AUDIO_CH_8;
			break;
		}
		uiRegMap_EnableAudioChannels &= (uiEnabledChannels << DPTX_AUD_CONFIG1_DATA_EN_IN_SHIFT);
	}

	Dptx_Reg_Writel(dptx, uiReg_Offset, uiRegMap_EnableAudioChannels);
}

void Dptx_AudIn_Configure(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, struct Dptx_Audio_Params *pstAudioParams)
{
	struct Dptx_Audio_Params *pstDptxAudioParams = &pstDptx->stAudioParams;

	memcpy(pstDptxAudioParams, pstAudioParams, sizeof(struct Dptx_Audio_Params));

	Dptx_AudIn_Set_Mute(pstDptx, ucStream_Index, (uint8_t)AUDIO_INPUT_SET_MUTE_FLAG_VBID);

	Dptx_AudIn_Set_Input_InterfaceType(pstDptx, ucStream_Index, pstAudioParams->ucInput_InterfaceType);
	Dptx_AudIn_Set_HBR_En(pstDptx, ucStream_Index, pstAudioParams->ucInput_HBR_Mode);
	Dptx_AudIn_Set_MaxNumOfChannels(pstDptx, ucStream_Index, pstAudioParams->ucInput_Max_NumOfchannels);
	Dptx_AudIn_Set_DataWidth(pstDptx, ucStream_Index, pstAudioParams->ucInput_DataWidth);
	Dptx_AudIn_Set_Timestamp_Ver(pstDptx, ucStream_Index, pstAudioParams->ucInput_TimestampVersion);
	Dptx_AudIn_Set_SDP_InforFrame(pstDptx,
												pstAudioParams->ucIEC_Sampling_Freq,
												pstAudioParams->ucIEC_OriginSamplingFreq,
												pstAudioParams->ucInput_Max_NumOfchannels,
												pstAudioParams->ucInput_DataWidth);
	Dptx_AudIn_Enable_SDP(pstDptx, ucStream_Index, 1);
	Dptx_AudIn_Enable_Timestamp(pstDptx, ucStream_Index, 1);

	Dptx_AudIn_Set_Mute(pstDptx, ucStream_Index, (uint8_t)AUDIO_INPUT_CLEAR_MUTE_FLAG_VBID);

	dptx_info("\n DP %d : ", ucStream_Index);
	dptx_info("  -.Interface type : %d", (u32)pstAudioParams->ucInput_InterfaceType);
	dptx_info("  -.Max num of input channels : %d", (u32)pstAudioParams->ucInput_Max_NumOfchannels);
	dptx_info("  -.Input data width : %d", (u32)pstAudioParams->ucInput_DataWidth);
	dptx_info("  -.HBR Mode : %d", (u32)pstAudioParams->ucInput_HBR_Mode);
	dptx_info("  -.Input version of time stamp : %d", (u32)pstAudioParams->ucInput_TimestampVersion);
	dptx_info("  -.Sampling Freq : %d", (u32)pstAudioParams->ucIEC_Sampling_Freq);
	dptx_info("  -.Orginal Sampling Freq : %d", (u32)pstAudioParams->ucIEC_OriginSamplingFreq);
}

void Dptx_AudIn_Set_MaxNumOfChannels(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucInput_Max_NumOfCh)
{
	uint32_t uiRegMap_NumOfChannels = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_NumOfChannels = Dptx_Reg_Readl(pstDptx, uiReg_Offset);
	uiRegMap_NumOfChannels &= ~DPTX_AUD_CONFIG1_NCH_MASK;
	uiRegMap_NumOfChannels |= (ucInput_Max_NumOfCh << DPTX_AUD_CONFIG1_NCH_SHIFT);

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_NumOfChannels);

	dptx_audin_active_audio_channels(pstDptx, ucStream_Index, ucInput_Max_NumOfCh, 1);
}

void Dptx_AudIn_Get_MaxNumOfChannels(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t *pucInput_Max_NumOfCh)
{
	uint32_t uiRegMap_NumOfChannels = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_NumOfChannels = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	*pucInput_Max_NumOfCh = (uint8_t)((uiRegMap_NumOfChannels & DPTX_AUD_CONFIG1_NCH_MASK) >> DPTX_AUD_CONFIG1_NCH_SHIFT);
}

void Dptx_AudIn_Set_DataWidth(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucInput_DataWidth)
{
	u32 uiRegMap_AudioSamplingWidth = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_AudioSamplingWidth = Dptx_Reg_Readl(pstDptx, uiReg_Offset);
	uiRegMap_AudioSamplingWidth &= ~DPTX_AUD_CONFIG1_DATA_WIDTH_MASK;
	uiRegMap_AudioSamplingWidth |= (ucInput_DataWidth << DPTX_AUD_CONFIG1_DATA_WIDTH_SHIFT);

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudioSamplingWidth);
	}

void Dptx_AudIn_Get_DataWidth(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t *pucInput_DataWidth)
{
	u32 uiRegMap_AudioSamplingWidth = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_AudioSamplingWidth = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	*pucInput_DataWidth = (uint32_t)((uiRegMap_AudioSamplingWidth & DPTX_AUD_CONFIG1_DATA_WIDTH_MASK) >> DPTX_AUD_CONFIG1_DATA_WIDTH_SHIFT);
}

void Dptx_AudIn_Get_SamplingFreq(struct Dptx_Params *pstDptx, uint8_t *pucSamplingFreq, uint8_t	*pucOrgSamplingFreq)
{
	struct Dptx_Audio_Params *pstAudioParams = &pstDptx->stAudioParams;

	*pucSamplingFreq = pstAudioParams->ucIEC_Sampling_Freq;
	*pucOrgSamplingFreq = pstAudioParams->ucIEC_OriginSamplingFreq;
}

void Dptx_AudIn_Set_Input_InterfaceType(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucAudInput_InterfaceType)
{
	u32 uiRegMap_AudINFType = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_AudINFType = Dptx_Reg_Readl(pstDptx, uiReg_Offset);
	uiRegMap_AudINFType &= ~DPTX_AUD_CONFIG1_INF_TYPE_MASK;
	uiRegMap_AudINFType |= (ucAudInput_InterfaceType << DPTX_AUD_CONFIG1_INF_TYPE_SHIFT);

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudINFType);
}

void Dptx_AudIn_Get_Input_InterfaceType(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t *pucAudInput_InterfaceType)
{
	u32 uiRegMap_AudINFType = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_AudINFType = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	*pucAudInput_InterfaceType = (uiRegMap_AudINFType & DPTX_AUD_CONFIG1_INF_TYPE_MASK) ? 1 : 0;
}

void Dptx_AudIn_Set_HBR_En(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucEnable)
{
	uint32_t uiRegMap_AudHBR = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_AudHBR = Dptx_Reg_Readl(pstDptx, uiReg_Offset);
	uiRegMap_AudHBR &= ~DPTX_AUD_CONFIG1_HBR_MODE_ENABLE_MASK;
	uiRegMap_AudHBR |= (ucEnable << DPTX_AUD_CONFIG1_HBR_MODE_ENABLE_SHIFT);

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudHBR);
}

void Dptx_AudIn_Get_HBR_En(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t *pucEnable)
{
	uint32_t uiRegMap_AudHBR = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_AudHBR = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	*pucEnable = (uiRegMap_AudHBR & DPTX_AUD_CONFIG1_HBR_MODE_ENABLE_MASK) ? 1 : 0;
}

void Dptx_AudIn_Set_Timestamp_Ver(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucAudInput_TimestampVersion)
{
	u32 uiRegMap_AudConf1 = 0,    uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_AudConf1 = Dptx_Reg_Readl(pstDptx, uiReg_Offset);
	uiRegMap_AudConf1 &= ~DPTX_AUD_CONFIG1_ATS_VER_MASK;
	uiRegMap_AudConf1 |= (ucAudInput_TimestampVersion << DPTX_AUD_CONFIG1_ATS_VER_SHFIT);

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudConf1);
}

void Dptx_AudIn_Set_SDP_InforFrame(struct Dptx_Params *pstDptx,
																uint8_t ucIEC_Sampling_Freq,
																uint8_t ucIEC_OriginSamplingFreq,
																uint8_t ucInput_Max_NumOfchannels,
																uint8_t ucInput_DataWidth)
{
	uint32_t uiAudio_SdpInfoframe_header = AUDIO_INFOFREAME_HEADER;
	uint32_t auiAudio_SdpInfoframe_data[3] = {0x00000710, 0x0, 0x0};
#if 1
/*
 *	Hardcoding based on SoC guilde.
 */
	auiAudio_SdpInfoframe_data[0] = 0x00000901;
	auiAudio_SdpInfoframe_data[1] = 0x00000001;

	Dptx_Reg_Writel(pstDptx, DPTX_SDP_BANK, uiAudio_SdpInfoframe_header);
	Dptx_Reg_Writel(pstDptx, DPTX_SDP_BANK + 4, auiAudio_SdpInfoframe_data[0]);
	Dptx_Reg_Writel(pstDptx, DPTX_SDP_BANK + 8, auiAudio_SdpInfoframe_data[1]);
	Dptx_Reg_Writel(pstDptx, DPTX_SDP_BANK + 12, auiAudio_SdpInfoframe_data[2]);
#else /* Synopsys reference */
	if (ucIEC_OriginSamplingFreq == (u8)IEC60958_3_ORIGINAL_SAMPLE_FREQ_176_4 && ucIEC_Sampling_Freq == (u8)IEC60958_3_SAMPLE_FREQ_176_4)
		auiAudio_SdpInfoframe_data[0] = 0x00000710;
	else if (ucIEC_OriginSamplingFreq == (u8)IEC60958_3_ORIGINAL_SAMPLE_FREQ_44_1 && ucIEC_Sampling_Freq == (u8)IEC60958_3_SAMPLE_FREQ_44_1)
		auiAudio_SdpInfoframe_data[0] = 0x00000B10;
	else if (ucIEC_OriginSamplingFreq == (u8)IEC60958_3_ORIGINAL_SAMPLE_FREQ_22_05 && ucIEC_Sampling_Freq == (u8)IEC60958_3_SAMPLE_FREQ_22_05)
		auiAudio_SdpInfoframe_data[0] = 0x00000F10;
	else if (ucIEC_OriginSamplingFreq == 7 && ucIEC_Sampling_Freq == 8)
		auiAudio_SdpInfoframe_data[0] = 0x00001310;
	else if (ucIEC_OriginSamplingFreq == (u8)IEC60958_3_ORIGINAL_SAMPLE_FREQ_11_025 && ucIEC_Sampling_Freq == 10)
		auiAudio_SdpInfoframe_data[0] = 0x00001710;
	else if (ucIEC_OriginSamplingFreq == (u8)IEC60958_3_ORIGINAL_SAMPLE_FREQ_32 && ucIEC_Sampling_Freq == (u8)IEC60958_3_SAMPLE_FREQ_32)
		auiAudio_SdpInfoframe_data[0] = 0x00001B10;
	else
		auiAudio_SdpInfoframe_data[0] = 0x00001F10;

	auiAudio_SdpInfoframe_data[0] |= (ucInput_Max_NumOfchannels - 1);

	if (ucInput_Max_NumOfchannels == 3)
		auiAudio_SdpInfoframe_data[0] |= 0x02000000;
	else if (ucInput_Max_NumOfchannels == 4)
		auiAudio_SdpInfoframe_data[0] |= 0x03000000;
	else if (ucInput_Max_NumOfchannels == 5)
		auiAudio_SdpInfoframe_data[0] |= 0x07000000;
	else if (ucInput_Max_NumOfchannels == 6)
		auiAudio_SdpInfoframe_data[0] |= 0x0b000000;
	else if (ucInput_Max_NumOfchannels == 7)
		auiAudio_SdpInfoframe_data[0] |= 0x0f000000;
	else if (ucInput_Max_NumOfchannels == 8)
		auiAudio_SdpInfoframe_data[0] |= 0x13000000;

	switch (ucInput_DataWidth) {
	case MAX_INPUT_DATA_WIDTH_16BIT:
		dptx_dbg("ucInput_DataWidth = 16");
		auiAudio_SdpInfoframe_data[0] &= ~(GENMASK(9, 8));
		auiAudio_SdpInfoframe_data[0] |= 1 << 8;
		break;
	case MAX_INPUT_DATA_WIDTH_20BIT:
		dptx_dbg("ucInput_DataWidth = 20");
		auiAudio_SdpInfoframe_data[0] &= ~(GENMASK(9, 8));
		auiAudio_SdpInfoframe_data[0] |= 2 << 8;
		break;
	case MAX_INPUT_DATA_WIDTH_24BIT:
		dptx_dbg("ucInput_DataWidth = 24");
		auiAudio_SdpInfoframe_data[0] &= ~(GENMASK(9, 8));
		auiAudio_SdpInfoframe_data[0] |= 3 << 8;
		break;
	default:
		dptx_err("ucInput_DataWidth not found");
		break;
	}

	dptx_dbg("auiAudio_SdpInfoframe_data[0] after = %x", auiAudio_SdpInfoframe_data[0]);

	Dptx_Reg_Writel(pstDptx, DPTX_SDP_BANK, uiAudio_SdpInfoframe_header);
	Dptx_Reg_Writel(pstDptx, DPTX_SDP_BANK + 4, auiAudio_SdpInfoframe_data[0]);
	Dptx_Reg_Writel(pstDptx, DPTX_SDP_BANK + 8, auiAudio_SdpInfoframe_data[1]);
	Dptx_Reg_Writel(pstDptx, DPTX_SDP_BANK + 12, auiAudio_SdpInfoframe_data[2]);
#endif
}

void Dptx_AudIn_Set_Mute(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucMute)
{
	u32 uiRegMap_AudioMute = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_AudioMute = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	if (ucMute)
		uiRegMap_AudioMute |= DPTX_AUDIO_MUTE;
	else
		uiRegMap_AudioMute &= ~DPTX_AUDIO_MUTE;

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudioMute);
}

void Dptx_AudIn_Get_Mute(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t *pucMute)
{
	u32 uiRegMap_AudioMute = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_AudioMute = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	*pucMute = (uiRegMap_AudioMute & DPTX_AUDIO_MUTE) ? 1 : 0;
}


void Dptx_AudIn_Set_Stream_Enable(struct Dptx_Params *pstDptx, u8 ucSatrem_Index, bool bEnable)
{
	u32	uiRegMap_AudioMute = 0;

	uiRegMap_AudioMute = Dptx_Reg_Readl(pstDptx, DPTX_AUD_CONFIG1_N(ucSatrem_Index));

	if (bEnable)
		uiRegMap_AudioMute &= ~DPTX_AUDIO_MUTE;
	else
		uiRegMap_AudioMute |= DPTX_AUDIO_MUTE;

	Dptx_Reg_Writel(pstDptx, DPTX_AUD_CONFIG1_N(ucSatrem_Index), uiRegMap_AudioMute);
}

void Dptx_AudIn_Enable_SDP(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucEnable)
{
	u32	uiRegMap_AudEnableSDP, uiReg_Offset = 0;

	uiReg_Offset = DPTX_SDP_VERTICAL_CTRL_N(ucStream_Index);

	uiRegMap_AudEnableSDP = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	if (ucEnable)
		uiRegMap_AudEnableSDP |= DPTX_EN_AUDIO_STREAM_SDP;
	else
		uiRegMap_AudEnableSDP &= ~DPTX_EN_AUDIO_STREAM_SDP;

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudEnableSDP);


	uiReg_Offset = DPTX_SDP_HORIZONTAL_CTRL_N(ucStream_Index);

	uiRegMap_AudEnableSDP = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	if (ucEnable)
		uiRegMap_AudEnableSDP |= DPTX_EN_AUDIO_STREAM_SDP;
	else
		uiRegMap_AudEnableSDP &= ~DPTX_EN_AUDIO_STREAM_SDP;

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudEnableSDP);
}

void Dptx_AudIn_Enable_Timestamp(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucEnable)
{
	u32	uiRegMap_AudEnableTimestamp,     uiReg_Offset = 0;

	uiReg_Offset = DPTX_SDP_VERTICAL_CTRL_N(ucStream_Index);

	uiRegMap_AudEnableTimestamp = Dptx_Reg_Readl(pstDptx, DPTX_SDP_VERTICAL_CTRL);

	if (ucEnable)
		uiRegMap_AudEnableTimestamp |= DPTX_EN_AUDIO_TIMESTAMP_SDP;
	else
		uiRegMap_AudEnableTimestamp &= ~DPTX_EN_AUDIO_TIMESTAMP_SDP;

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudEnableTimestamp);


	uiReg_Offset = DPTX_SDP_HORIZONTAL_CTRL_N(ucStream_Index);

	uiRegMap_AudEnableTimestamp = Dptx_Reg_Readl(pstDptx, DPTX_SDP_HORIZONTAL_CTRL);

	if (ucEnable)
		uiRegMap_AudEnableTimestamp |= DPTX_EN_AUDIO_TIMESTAMP_SDP;
	else
		uiRegMap_AudEnableTimestamp &= ~DPTX_EN_AUDIO_TIMESTAMP_SDP;

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudEnableTimestamp);
}


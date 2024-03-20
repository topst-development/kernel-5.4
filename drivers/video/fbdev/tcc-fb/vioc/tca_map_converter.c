// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/div64.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_mc.h>
#include <video/tcc/vioc_config.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#endif

//#define CHROMA_8X4	//two 8x4 grouping (block-interleaving) for compression
//unit
#define RECALC_BASE_ADDRESS // To prevent broken image when cropping with 480,0
			    // ~ 2880x2160 in 4K(3840x2160)
#include <video/tcc/tca_map_converter.h>

#define MAX_WAIT_TIEM 400U //0xF0000


void tca_map_convter_wait_done(unsigned int component_num)
{
	unsigned int loop = 0, upd_loop = 0;
	const void __iomem *HwVIOC_MC = VIOC_MC_GetAddress(component_num);
	unsigned int value;

	if (HwVIOC_MC == NULL) {
		(void)pr_err("[ERR][MAPC] %s: HwVIOC_MC is null\n", __func__);
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		for (loop = 0U; loop <= MAX_WAIT_TIEM; loop++) {
			value = __raw_readl(HwVIOC_MC + MC_STAT);
			if ((value & MC_STAT_BUSY_MASK) == 0U) {
				/* Prevent KCS warning */
				break;
			}
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			udelay(100);
		}

		for (upd_loop = 0U; upd_loop < MAX_WAIT_TIEM; upd_loop++) {
			value = __raw_readl(HwVIOC_MC + MC_CTRL);
			if ((value & MC_CTRL_UPD_MASK) == 0U) {
				/* Prevent KCS warning */
				break;
			}
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */

			udelay(100);
		}

		if ((loop >= MAX_WAIT_TIEM) || (upd_loop >= MAX_WAIT_TIEM)) {
			/* Prevent KCS warning */
			(void)pr_err("[ERR][MAPC] %s  Loop (EOF :%d  UPD:0x%x)\n",
				__func__, loop, upd_loop);
		}
	}
}

void tca_map_convter_swreset(unsigned int component_num)
{
	VIOC_CONFIG_SWReset(component_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(component_num, VIOC_CONFIG_CLEAR);
}

void tca_map_convter_onoff(unsigned int component_num, unsigned int onoff,
			   unsigned int wait_done)
{
	void __iomem *HwVIOC_MC = VIOC_MC_GetAddress(component_num);

	if (onoff == 0U) {
		VIOC_MC_FRM_SIZE(HwVIOC_MC, 0, 0);
		/* prevent KCS warning */
	}

	VIOC_MC_Start_OnOff(HwVIOC_MC, onoff);
	if ((onoff == 0U) && (wait_done != 0U)) {
		 // no need to wait because
		 // VIOC_CONFIG_DMAPath_UnSet() will check it.
		tca_map_convter_wait_done(component_num);
		//tca_map_convter_swreset(component_num);
	}
}

static unsigned int tca_convert_bit_depth(unsigned int in_bitDepth)
{
	unsigned int out_bitDepth = 0U;

	if (in_bitDepth == 8U) {
		/* Prevent KCS warning */
		out_bitDepth = 0U;
	} else if (in_bitDepth == 16U) {
		/* Prevent KCS warning */
		out_bitDepth = 1U;
	} else {
		/* Prevent KCS warning */
		out_bitDepth = 2U;
	}

	return out_bitDepth;
}

void tca_map_convter_set(unsigned int component_num,
			 const struct tcc_lcdc_image_update *ImageInfo, int y2r)
{
	uint pic_height;
	uint line_height;
	uint bit_depth_y;
	uint bit_depth_c;
	uint offset_base_y, offset_base_c;
	uint frame_base_y, frame_base_c;
	void __iomem *HwVIOC_MC =
		VIOC_MC_GetAddress(component_num);
	unsigned long long temp_longlong = 0U; /* avoid CERT-C Integers Rule INT31-C */

	(void)HwVIOC_MC;

	//ImageInfo->private_data.mapConv_info.m_CompressedY[0] = 0x31900000;
	//ImageInfo->private_data.mapConv_info.m_CompressedCb[0]
	//	= 0x31900000 + 0x7F8000;
	//ImageInfo->private_data.mapConv_info.m_FbcYOffsetAddr[0] = 0x32900000;
	//ImageInfo->private_data.mapConv_info.m_FbcCOffsetAddr[0]
	//	= 0x32900000 + 0x272000;

	/* cropping test */
	//ImageInfo->crop_left = 480;
	//ImageInfo->crop_right = ImageInfo->crop_left + 2880;
	//ImageInfo->crop_top = 0;
	//ImageInfo->crop_bottom = ImageInfo->crop_top + 2160;

	//pr_info("[DBG][MAPC] MC[0x%x] >> R[0x%lx/0x%lx/0x%lx] M[%d] idx[%d], ID[%d] Buff:[%dx%d]%dx%d F:%dx%d I:%dx%d Str(%d/%d) C:0x%08x/0x%08x T:0x%08x/0x%08x bpp(%d/%d) crop(%d/%d~%dx%d)\n",
	//	component_num,
	//	__raw_readl(HwVIOC_MC+MC_CTRL),
	//	__raw_readl(HwVIOC_MC+MC_FRM_BASE_Y),
	//	__raw_readl(HwVIOC_MC+MC_STAT),
	//	ImageInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO],
	//	ImageInfo->private_data.optional_info[VID_OPT_DISP_OUT_IDX],
	//	ImageInfo->private_data.optional_info[VID_OPT_BUFFER_ID],
	//	ImageInfo->private_data.optional_info[VID_OPT_FRAME_WIDTH],
	//	ImageInfo->private_data.optional_info[VID_OPT_FRAME_HEIGHT],
	//	ImageInfo->private_data.optional_info[VID_OPT_BUFFER_WIDTH],
	//	ImageInfo->private_data.optional_info[VID_OPT_BUFFER_HEIGHT],
	//	ImageInfo->Frame_width,
	//	ImageInfo->Frame_height,
	//	ImageInfo->Image_width,
	//	ImageInfo->Image_height,
	//	ImageInfo->private_data.mapConv_info.m_uiLumaStride,
	//	ImageInfo->private_data.mapConv_info.m_uiChromaStride,
	//	ImageInfo->private_data.mapConv_info.m_CompressedY[0],
	//	ImageInfo->private_data.mapConv_info.m_CompressedCb[0],
	//	ImageInfo->private_data.mapConv_info.m_FbcYOffsetAddr[0],
	//	ImageInfo->private_data.mapConv_info.m_FbcCOffsetAddr[0],
	//	ImageInfo->private_data.mapConv_info.m_uiLumaBitDepth,
	//	ImageInfo->private_data.mapConv_info.m_uiChromaBitDepth,
	//	ImageInfo->crop_left,
	//	ImageInfo->crop_top,
	//	(ImageInfo->crop_right-ImageInfo->crop_left),
	//	(ImageInfo->crop_bottom-ImageInfo->crop_top));

	bit_depth_y = tca_convert_bit_depth(
		ImageInfo->private_data.mapConv_info.m_uiLumaBitDepth);
	bit_depth_c = tca_convert_bit_depth(
		ImageInfo->private_data.mapConv_info.m_uiChromaBitDepth);

#if defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
	if (ImageInfo->private_data.mapConv_info.m_Reserved[0] == 16U) { //VP9
		/* Prevent KCS warning */
		pic_height = (((ImageInfo->Frame_height + 63U) >> 6U) << 6U);
	} else
#endif
	{
		/* Prevent KCS warning */
		pic_height = (((ImageInfo->Frame_height + 15U) >> 4U) << 4U);
	}
#if defined(RECALC_BASE_ADDRESS)
	//--------------------------------------
	// Offset table (based on x/ypos)
	line_height = pic_height / 4U; // 4 line --> 1 line

	/* avoid CERT-C Integers Rule INT31-C */
	if ((ImageInfo->crop_left < 0) || (ImageInfo->crop_top < 0)) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	/* avoid CERT-C Integers Rule INT30-C */
	if (!((line_height < UINT_MAX) &&
			((uint)ImageInfo->crop_left < UINT_MAX) &&
			((uint)ImageInfo->crop_top < UINT_MAX))) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	/* avoid CERT-C Integers Rule INT31-C */
	if ((ImageInfo->crop_right < 0) || (ImageInfo->crop_bottom < 0)) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	/* avoid CERT-C Integers Rule INT31-C */
	temp_longlong = ImageInfo->private_data.mapConv_info.m_FbcYOffsetAddr[0] & UINT_MAX;
	/* avoid CERT-C Integers Rule INT30-C */
	if (!((uint)temp_longlong < UINT_MAX)) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	offset_base_y =
		(uint)temp_longlong +
		((((((uint)ImageInfo->crop_left / 256U) * line_height) * 2U) +
		 (((uint)ImageInfo->crop_top / 4U) * 2U)) * 16U);
#ifdef CHROMA_8X4
	height = ((((pic_height / 2U) + 15U) / 16U) * 16U) / 4U;
	offset_base_c =
		ImageInfo->private_data.mapConv_info.m_FbcCOffsetAddr[0] +
		(((((((uint)ImageInfo->crop_left / 2U) / 128U) * height) * 2U) +
		 ((((uint)ImageInfo->crop_top / 2U) / 4U) * 2U)) * 16U);
#else
	/* avoid CERT-C Integers Rule INT31-C */
	temp_longlong = ImageInfo->private_data.mapConv_info.m_FbcCOffsetAddr[0] & UINT_MAX;
	/* avoid CERT-C Integers Rule INT30-C */
	if (!((uint)temp_longlong < UINT_MAX)) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	offset_base_c =
		(uint)temp_longlong +
		(((((((uint)ImageInfo->crop_left / 2U) / 256U) * line_height) * 2U) +
		 ((((uint)ImageInfo->crop_top / 2U) / 2U) * 2U)) * 16U); // 16x2
#endif

	//--------------------------------------
	// Comp. frame (based on ypos)
	/* avoid CERT-C Integers Rule INT31-C */
	temp_longlong = ImageInfo->private_data.mapConv_info.m_CompressedY[0] & UINT_MAX;
	/* avoid CERT-C Integers Rule INT30-C */
	if (!((uint)temp_longlong < UINT_MAX)) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	frame_base_y =
		(uint)temp_longlong +
		(((uint)ImageInfo->crop_top / 4U) *
			(uint)ImageInfo->private_data.mapConv_info.m_uiLumaStride);

	/* avoid CERT-C Integers Rule INT31-C */
	temp_longlong = ImageInfo->private_data.mapConv_info.m_CompressedCb[0] & UINT_MAX;
	/* avoid CERT-C Integers Rule INT30-C */
	if (!((uint)temp_longlong < UINT_MAX)) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	frame_base_c =
		(uint)temp_longlong +
		(((uint)ImageInfo->crop_top / 4U) *
			(uint)ImageInfo->private_data.mapConv_info.m_uiChromaStride);

#else
	offset_base_y =
		(uint)ImageInfo->private_data.mapConv_info.m_FbcYOffsetAddr[0];
	offset_base_c =
		(uint)ImageInfo->private_data.mapConv_info.m_FbcCOffsetAddr[0];
	frame_base_y = (uint)ImageInfo->private_data.mapConv_info.m_CompressedY[0];
	frame_base_c = (uint)ImageInfo->private_data.mapConv_info.m_CompressedCb[0];
#endif

	VIOC_MC_ENDIAN(HwVIOC_MC,
		       ImageInfo->private_data.mapConv_info.m_uiFrameEndian,
		       ImageInfo->private_data.mapConv_info.m_uiFrameEndian);
	VIOC_MC_OFFSET_BASE(HwVIOC_MC, offset_base_y, offset_base_c);
	VIOC_MC_FRM_BASE(HwVIOC_MC, frame_base_y, frame_base_c);
	VIOC_MC_Start_BitDepth(HwVIOC_MC, bit_depth_c, bit_depth_y);
	VIOC_MC_FRM_POS(HwVIOC_MC, (uint)ImageInfo->crop_left, (uint)ImageInfo->crop_top);

	/* avoid CERT-C Integers Rule INT31-C */
	if ((ImageInfo->crop_right < 0) || (ImageInfo->crop_bottom < 0)) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	VIOC_MC_FRM_SIZE(HwVIOC_MC,
		(((uint)ImageInfo->crop_right) - ((uint)ImageInfo->crop_left)),
		(((uint)ImageInfo->crop_bottom) - ((uint)ImageInfo->crop_top)));
	VIOC_MC_FRM_SIZE_MISC(
		HwVIOC_MC, pic_height,
		ImageInfo->private_data.mapConv_info.m_uiLumaStride,
		ImageInfo->private_data.mapConv_info.m_uiChromaStride);

	VIOC_MC_DITH_CONT(HwVIOC_MC, 0U, 0U);

	/* avoid CERT-C Integers Rule INT31-C */
	if (y2r < 0) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	VIOC_MC_Y2R_OnOff(HwVIOC_MC, (uint)y2r, 0x2U);
	VIOC_MC_SetDefaultAlpha(HwVIOC_MC, 0x3FFU);

	VIOC_MC_Start_OnOff(HwVIOC_MC, 1U);

FUNC_EXIT:
	return;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(tca_map_convter_set);

void tca_map_convter_driver_set(
				unsigned int component_num, unsigned int Fwidth,
				unsigned int Fheight, unsigned int pos_x,
				unsigned int pos_y, unsigned int Cwidth,
				unsigned int Cheight, unsigned int y2r,
				const hevc_MapConv_info_t *mapConv_info)
{
	uint pic_height;
	uint line_height;
	uint bit_depth_y;
	uint bit_depth_c;
	uint offset_base_y, offset_base_c;
	uint frame_base_y, frame_base_c;
	void __iomem *HwVIOC_MC =
		VIOC_MC_GetAddress(component_num);
	unsigned long long temp_longlong = 0U; /* avoid CERT-C Integers Rule INT31-C */

	/* avoid MISRA C-2012 Rule 2.7 */
    (void)Fwidth;
	//pr_info("[DBG][MAPC] MC[%d] >> R[0x%lx/0x%lx/0x%lx] M[%d] F:%dx%d Str(%d/%d) C:0x%08x/0x%08x T:0x%08x/0x%08x bpp(%d/%d) crop(%d/%d~%dx%d) Reserved(%d)\n",
	//	get_vioc_index(component_num),
	//	__raw_readl(HwVIOC_MC+MC_CTRL),
	//	__raw_readl(HwVIOC_MC+MC_FRM_BASE_Y),
	//	__raw_readl(HwVIOC_MC+MC_STAT),
	//	1,
	//	Fwidth,
	//	Fheight,
	//	mapConv_info->m_uiLumaStride,
	//	mapConv_info->m_uiChromaStride,
	//	mapConv_info->m_CompressedY[0],
	//	mapConv_info->m_CompressedCb[0],
	//	mapConv_info->m_FbcYOffsetAddr[0],
	//	mapConv_info->m_FbcCOffsetAddr[0],
	//	mapConv_info->m_uiLumaBitDepth,
	//	mapConv_info->m_uiChromaBitDepth,
	//	pos_x,
	//	pos_y,
	//	Cwidth,
	//	Cheight,
	//	mapConv_info->m_Reserved[0]);

	bit_depth_y = tca_convert_bit_depth(mapConv_info->m_uiLumaBitDepth);
	bit_depth_c = tca_convert_bit_depth(mapConv_info->m_uiChromaBitDepth);

	/* avoid CERT-C Integers Rule INT30-C */
	if (!(Fheight < UINT_MAX)) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
#if defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
	if (mapConv_info->m_Reserved[0] == 16U) { //VP9
		/* Prevent KCS warning */
		pic_height = (((Fheight + 63U) >> 6U) << 6U);
	} else
#endif
	{
		/* Prevent KCS warning */
		pic_height = (((Fheight + 15U) >> 4U) << 4U);
	}

#if defined(RECALC_BASE_ADDRESS)
	//--------------------------------------
	// Offset table (based on x/ypos)
	line_height = pic_height / 4U; // 4 line --> 1 line

	/* avoid CERT-C Integers Rule INT30-C */
	if (!((line_height < UINT_MAX) &&
			(pos_x < UINT_MAX) &&
			(pos_y < UINT_MAX))) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	/* avoid CERT-C Integers Rule INT31-C */
	temp_longlong = mapConv_info->m_FbcYOffsetAddr[0] & UINT_MAX;
	/* avoid CERT-C Integers Rule INT30-C */
	if (!((uint)temp_longlong < UINT_MAX)) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	offset_base_y =
		(uint)temp_longlong +
		(((((pos_x / 256U) * line_height) * 2U) + ((pos_y / 4U) * 2U)) * 16U);

#ifdef CHROMA_8X4
	height = ((((pic_height / 2U) + 15U) / 16U) * 16U) / 4U;
	offset_base_c =
		(uint)mapConv_info->m_FbcCOffsetAddr[0] +
		((((((pos_x / 2U) / 128U) * height) * 2U) + (((pos_y / 2U) / 4U) * 2U)) * 16U);
#else
	/* avoid CERT-C Integers Rule INT31-C */
	temp_longlong = mapConv_info->m_FbcCOffsetAddr[0] & UINT_MAX;
	/* avoid CERT-C Integers Rule INT30-C */
	if (!((uint)temp_longlong < UINT_MAX)) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	offset_base_c =
		(uint)temp_longlong +
		((((((pos_x / 2U) / 256U) * line_height) * 2U) + (((pos_y / 2U) / 2U) * 2U)) * 16U); // 16x2
#endif

	//--------------------------------------
	// Comp. frame (based on ypos)
	/* avoid CERT-C Integers Rule INT31-C */
	temp_longlong = mapConv_info->m_CompressedY[0] & UINT_MAX;
	/* avoid CERT-C Integers Rule INT30-C */
	if (!((uint)temp_longlong < UINT_MAX)) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	frame_base_y = ((uint)temp_longlong +
		       ((pos_y / 4U) * (uint)mapConv_info->m_uiLumaStride));

	/* avoid CERT-C Integers Rule INT31-C */
	temp_longlong = mapConv_info->m_CompressedCb[0] & UINT_MAX;
	/* avoid CERT-C Integers Rule INT30-C */
	if (!((uint)temp_longlong < UINT_MAX)) {
		(void)pr_err("[ERR][MAPC] %s invalid variable\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	frame_base_c = ((uint)temp_longlong +
		       ((pos_y / 4U) * (uint)mapConv_info->m_uiChromaStride));
#else
	offset_base_y = (uint)mapConv_info->m_FbcYOffsetAddr[0];
	offset_base_c = (uint)mapConv_info->m_FbcCOffsetAddr[0];
	frame_base_y = (uint)mapConv_info->m_CompressedY[0];
	frame_base_c = (uint)mapConv_info->m_CompressedCb[0];
#endif

	VIOC_MC_ENDIAN(HwVIOC_MC, mapConv_info->m_uiFrameEndian,
		       mapConv_info->m_uiFrameEndian);
	VIOC_MC_OFFSET_BASE(HwVIOC_MC, offset_base_y, offset_base_c);
	VIOC_MC_FRM_BASE(HwVIOC_MC, frame_base_y, frame_base_c);
	VIOC_MC_Start_BitDepth(HwVIOC_MC, bit_depth_c, bit_depth_y);
	VIOC_MC_FRM_POS(HwVIOC_MC, pos_x, pos_y);
	VIOC_MC_FRM_SIZE(HwVIOC_MC, Cwidth, Cheight);
	VIOC_MC_FRM_SIZE_MISC(HwVIOC_MC, pic_height,
			      mapConv_info->m_uiLumaStride,
			      mapConv_info->m_uiChromaStride);
	VIOC_MC_DITH_CONT(HwVIOC_MC, 0, 0U);
	VIOC_MC_Y2R_OnOff(HwVIOC_MC, (uint)y2r, 0x2U);
	VIOC_MC_SetDefaultAlpha(HwVIOC_MC, 0x3FFU);

FUNC_EXIT:
	return;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(tca_map_convter_driver_set);

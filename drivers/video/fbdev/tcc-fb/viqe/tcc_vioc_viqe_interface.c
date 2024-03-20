// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/fb.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/of_reserved_mem.h>
#include <uapi/misc/tccmisc_drv.h>

#include <video/tcc/tcc_types.h>
//#include <video/tcc/tccfb.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_viqe.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_vin.h>
#include <video/tcc/vioc_deintls.h>
//#include <video/tcc/tccfb_address.h>

#include "tcc_vioc_viqe_interface.h"

#ifdef CONFIG_VIOC_MAP_DECOMP
#include <video/tcc/tca_map_converter.h>
#endif
#ifdef CONFIG_VIOC_DTRC
#include <video/tcc/tca_dtrc_converter.h>
#endif

#include "../../../../media/platform/tccvin2/basic_operation.h"

/*------TODO: VIQE PORTING Temporary---------*/
//#include "../tcc_vsync.h"
//#include "../tcc_vioc_interface.h"
//#include "../tcc_vioc_fb.h"
//#include "tcc_vioc_viqe.h"

//tcc_vsync.h
typedef enum {
	VSYNC_MAIN = 0,
	VSYNC_SUB0,
	VSYNC_SUB1,
	VSYNC_SUB2,
	VSYNC_SUB3,
	VSYNC_MAX
} VSYNC_CH_TYPE;

//tcc_vsync.c
static int tcc_vsync_isVsyncRunning(VSYNC_CH_TYPE type)
{
	return (int)type;
}

//tccfb.h
struct tcc_dp_device {
	unsigned int dummy;
};

//tcc_vioc_interface.c
void tcc_video_post_process(struct tcc_lcdc_image_update *ImageInfo)
{
	ImageInfo->enable = 1U;
}

//tcc_vioc_fb.c
struct tcc_dp_device *tca_fb_get_displayType(enum TCC_OUTPUT_TYPE check_type);
struct tcc_dp_device *tca_fb_get_displayType(enum TCC_OUTPUT_TYPE check_type)
{
	//(void)check_type;
	//return NULL;
	return ((struct tcc_dp_device *)check_type);
}

//tcc_vioc_interface.c
void tca_scale_display_update(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo);
void tca_scale_display_update(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo)
{
	pdp_data->dummy = 1U;
	ImageInfo->enable = 1U;
}
/*----------------__AK__--------------*/

#define MAIN_USED (1U)
#define SUB_USED  (2U)

#ifdef CONFIG_TCC_VIOCMG
#include <video/tcc/viocmg.h>
#endif

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/vioc_dv_cfg.h>

#define DEF_DV_CHECK_NUM (1U) //(2U)
static int bStep_Check = DEF_DV_CHECK_NUM;
static unsigned int nFrame;
#endif

//#define USE_DEINTERLACE_S_IN30Hz //for external M2M
//#define USE_DEINTERLACE_S_IN60Hz //for internal M2M and on-the-fly

//#define DYNAMIC_USE_DEINTL_COMPONENT
//#define MAX_VIQE_FRAMEBUFFER

#define BUFFER_CNT_FOR_M2M_MODE (3U)

#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M	//TODO:AlanK check KConfig
static unsigned int gPMEM_VIQE_SUB_BASE;
static unsigned int gPMEM_VIQE_SUB_SIZE;

static int gDI_sub_mode_60Hz = VIOC_VIQE_DEINTL_MODE_2D;
static atomic_t gFrmCnt_Sub_60Hz;
static int gVIQE1_Init_State;
#endif

static unsigned int gPMEM_VIQE_BASE;
static unsigned int gPMEM_VIQE_SIZE;
//#define MAX_MAIN_VIQE (((1920 * (1088 / 2) * 4 * 3) / 2) * 4)

static struct tcc_viqe_common_virt_addr_info_t viqe_common_info;
static struct tcc_viqe_m2m_virt_addr_info_t main_m2m_info;
static struct tcc_viqe_m2m_virt_addr_info_t sub_m2m_info;
static struct tcc_viqe_60hz_virt_addr_info_t viqe_60hz_lcd_info;
static struct tcc_viqe_60hz_virt_addr_info_t viqe_60hz_external_info;
static struct tcc_viqe_60hz_virt_addr_info_t viqe_60hz_external_sub_info;
static struct tcc_viqe_m2m_scaler_type_t *scaler;
static struct tcc_viqe_m2m_scaler_type_t *scaler_sub;

static struct tcc_lcdc_image_update output_m2m_image;
static struct tcc_lcdc_image_update output_m2m_image_sub;

static unsigned int overlay_buffer[2][BUFFER_CNT_FOR_M2M_MODE];
static unsigned int current_buffer_idx[2];

static unsigned int gFrmCnt_30Hz;
static int gUse_sDeintls;

static enum TCC_OUTPUT_TYPE gOutputMode;
static atomic_t gFrmCnt_60Hz;
static unsigned int gLcdc_layer_60Hz;
static unsigned int gViqe_fmt_60Hz;
static unsigned int gImg_fmt_60Hz = (unsigned int)TCC_LCDC_IMG_FMT_MAX;
static unsigned int gDeintlS_Use_60Hz;
static unsigned int gDeintlS_Use_Plugin;

static unsigned int not_exist_viqe1;
#ifdef CONFIG_VIOC_MAP_DECOMP
static unsigned int gUse_MapConverter;
#endif
#ifdef CONFIG_VIOC_DTRC
static unsigned int gUse_DtrcConverter;
#endif

#ifndef USE_DEINTERLACE_S_IN30Hz
static int gusingDI_S;
static int gbfield_30Hz;
static VIOC_VIQE_DEINTL_MODE gDI_mode_30Hz = VIOC_VIQE_DEINTL_MODE_2D;
#else
static int gusingDI_S;
#endif

#ifndef USE_DEINTERLACE_S_IN60Hz
static VIOC_VIQE_DEINTL_MODE gDI_mode_60Hz = VIOC_VIQE_DEINTL_MODE_2D;
#else
static VIOC_VIQE_DEINTL_MODE gDI_mode_60Hz = VIOC_VIQE_DEINTL_S;
#endif

#ifdef CONFIG_USE_SUB_MULTI_FRAME
struct list_head video_queue_list;
static struct tcc_lcdc_image_update  sub_m2m_image[VSYNC_MAX];
#endif
static atomic_t viqe_sub_m2m_init;

static int gVIQE_Init_State;

//#define DBG_VIQE_INTERFACE
#ifdef DBG_VIQE_INTERFACE
#define dprintk(msg...) (void)pr_info("[DBG][VIQE] " msg)
#define iprintk(msg...) (void)pr_info("[DBG][VIQE] " msg)
#define dvprintk(msg...) (void)pr_info("[DBG][VIQE] DV_M2M: " msg)
#else
#define dprintk(msg...)
#define iprintk(msg...)
#define dvprintk(msg...)
#endif

//#define PRINT_OUT_M2M_MS
#ifdef PRINT_OUT_M2M_MS
static struct timeval t1;
static unsigned long time_gap_max_us;
static unsigned long time_gap_min_us;
static unsigned long time_gap_total_us;
static unsigned long time_gap_total_count;

static void tcc_GetTimediff_ms(struct timeval time1)
{
	unsigned int time_diff_us = 0;
	struct timeval t2;

	do_gettimeofday(&t2);

	if (t2.tv_sec <= time1.tv_sec && t2.tv_usec <= time1.tv_usec) {
		(void)pr_warn("[WAR][VIQE] strange Time-diff %2d.%2d <= %2d.%2d\n",
			t2.tv_sec, t2.tv_usec, time1.tv_sec, time1.tv_usec);
	}

	time_diff_us = (t2.tv_sec - time1.tv_sec) * 1000 * 1000;
	time_diff_us += (t2.tv_usec - time1.tv_usec);

	if (time_gap_min_us == 0U)
		time_gap_min_us = time_diff_us;

	if (time_gap_max_us < time_diff_us)
		time_gap_max_us = time_diff_us;

	if (time_gap_min_us > time_diff_us)
		time_gap_min_us = time_diff_us;

	if (time_gap_total_count % 60 == 0U) {
		dprintk("t-diff: max(%2d.%2d)/min(%2d.%2d) => avg(%2d.%2d)\n",
			(unsigned int)(time_gap_max_us / 1000),
			(unsigned int)(time_gap_max_us % 1000),
			(unsigned int)(time_gap_min_us / 1000),
			(unsigned int)(time_gap_min_us % 1000),
			(unsigned int)((time_gap_total_us
					/ time_gap_total_count) / 1000),
			(unsigned int)((time_gap_total_us
					/ time_gap_total_count) % 1000));
	}
	time_gap_total_us += time_diff_us;
	time_gap_total_count++;
}
#endif

static void tcc_sdintls_swreset(void)
{
	VIOC_CONFIG_SWReset(VIOC_DEINTLS0, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(VIOC_DEINTLS0, VIOC_CONFIG_CLEAR);
}

static void tcc_viqe_swreset(unsigned int viqe_num)
{
	if (get_vioc_index(viqe_num) < VIOC_VIQE_MAX) {
		VIOC_CONFIG_SWReset(viqe_num, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(viqe_num, VIOC_CONFIG_CLEAR);
	}
}

static void tcc_scaler_swreset(unsigned int scaler_num)
{
	if (get_vioc_index(scaler_num) < VIOC_SCALER_MAX) {
		VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_CLEAR);
	}
}

static unsigned int viqe_y2r_mode_check(void)
{
	unsigned int y2r_on = 0U;

#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
	y2r_on = 0U;
#else
	#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV)
	if ((gOutputMode != TCC_OUTPUT_HDMI) || hdmi_get_hdmimode())
		y2r_on = 0U;
	else
		y2r_on = 1U;
#elif defined(CONFIG_TCC_COMPOSITE_COLOR_SPACE_YUV)
	if (gOutputMode == TCC_OUTPUT_COMPOSITE)
		y2r_on = 0U;
	else
		y2r_on = 1U;
#else
	y2r_on = 1U;
#endif
#endif

	return y2r_on;
}

void TCC_VIQE_DI_PlugInOut_forAlphablending(int plugIn)
{
	if ((bool)gUse_sDeintls) {
		if ((bool)plugIn) {
			VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 1);
			(void)VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, main_m2m_info.gVIQE_RDMA_num_m2m);
		} else {
			(void)VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
			VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 0);
		}
	} else {
		if ((bool)plugIn) {
			(void)VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_VIQE1, main_m2m_info.gVIQE_RDMA_num_m2m);
			VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 1);
		} else {
			VIOC_RDMA_SetImageDisable(main_m2m_info.pRDMABase_m2m);
			(void)VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_VIQE1);
			VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 0);
			gFrmCnt_30Hz = 0U;
			atomic_set(&gFrmCnt_60Hz, 0);
		}
	}
}

void TCC_VIQE_DI_Init(struct VIQE_DI_TYPE *viqe_arg)
{
#ifndef USE_DEINTERLACE_S_IN30Hz
	unsigned int deintl_dma_base0, deintl_dma_base1;
	unsigned int deintl_dma_base2, deintl_dma_base3;
	unsigned int imgSize;
	/* If this value is 0, The size information is get from VIOC modules. */
	unsigned int top_size_dont_use = 0U;
#endif
	unsigned int framebufWidth, framebufHeight;
	VIOC_VIQE_FMT_TYPE img_fmt = VIOC_VIQE_FMT_YUV420;
	void __iomem *pRDMA, *pVIQE;
	unsigned int nRDMA = 0, nVIQE = 0;

	unsigned int y2r_on = 0U;

	if (TCC_VIQE_Scaler_Init_Buffer_M2M() < 0) {
		(void)pr_err("[ERR][VIQE] %s-%d : memory allocation is failed\n", __func__, __LINE__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	if (viqe_arg->nRDMA != 0U) {
		nRDMA = VIOC_RDMA00 + viqe_arg->nRDMA;
		pRDMA = VIOC_RDMA_GetAddress(nRDMA);
	} else {
		pRDMA = main_m2m_info.pRDMABase_m2m;
		nRDMA = main_m2m_info.gVIQE_RDMA_num_m2m;
	}

	viqe_arg->crop_top = (viqe_arg->crop_top >> 1) << 1;
#ifdef MAX_VIQE_FRAMEBUFFER
	framebufWidth = 1920;
	framebufHeight = 1088;
#else
	framebufWidth = ((viqe_arg->srcWidth - viqe_arg->crop_left - viqe_arg->crop_right) >> 3) << 3; // 8bit align
	framebufHeight = ((viqe_arg->srcHeight - viqe_arg->crop_top - viqe_arg->crop_bottom) >> 2) << 2; // 4bit align
#endif

	(void)pr_info("[INF][VIQE] %s, W:%d, H:%d, FMT:%s, OddFirst:%d, %s-%d\n",
		__func__,
		framebufWidth, framebufHeight,
		((bool)img_fmt ? "YUV422" : "YUV420"),
		viqe_arg->OddFirst,
		(bool)viqe_arg->use_sDeintls ? "S-Deintls" : "VIQE",
		(bool)viqe_arg->use_Viqe0 ? 0 : 1);

	if (viqe_common_info.gBoard_stb == 1U) {
		VIOC_RDMA_SetImageDisable(pRDMA);
		//TCC_OUTPUT_FB_ClearVideoImg();
	}

	VIOC_RDMA_SetImageY2REnable(pRDMA, 0U);
	/* Y2RMode Default 0 (Studio Color) */
	VIOC_RDMA_SetImageY2RMode(pRDMA, 0x02);
	VIOC_RDMA_SetImageIntl(pRDMA, 1);
	VIOC_RDMA_SetImageBfield(pRDMA, viqe_arg->OddFirst);

	if ((bool)viqe_arg->use_sDeintls) {
		gUse_sDeintls = 1;
		tcc_sdintls_swreset();
		(void)VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, nRDMA);
		(void)pr_info("[INF][VIQE] DEINTL-Simple\n");
	} else {
		gUse_sDeintls = 0;
#ifndef USE_DEINTERLACE_S_IN30Hz
		check_wrap(framebufWidth);
		check_wrap(framebufHeight);
		imgSize = (framebufWidth * (framebufHeight / 2U) * 4U * 3U / 2U);
		check_wrap(imgSize);

		if ((bool)not_exist_viqe1 || (bool)viqe_arg->use_Viqe0) {
			nVIQE = viqe_common_info.gVIOC_VIQE0;
			pVIQE = VIOC_VIQE_GetAddress(nVIQE);

			if (gPMEM_VIQE_SIZE < (imgSize * 4U)) {
				(void)pr_warn("[WAR][VIQE] 0: Increase VIQE pmap size for VIQE!! Current(0x%x)/Need(0x%x)\n",
					gPMEM_VIQE_SIZE, (imgSize * 4U));
			}
			deintl_dma_base0 = gPMEM_VIQE_BASE;
		} else {
			nVIQE = viqe_common_info.gVIOC_VIQE1;
			pVIQE = VIOC_VIQE_GetAddress(nVIQE);

			if ((gPMEM_VIQE_SIZE / 2U) < (imgSize * 4U)) {
				(void)pr_warn("[WAR][VIQE] 1: Increase VIQE pmap size for VIQE1!! Current(0x%x)/Need(0x%x)\n",
					gPMEM_VIQE_SIZE, (imgSize * 4U));
				deintl_dma_base0 = gPMEM_VIQE_BASE;
			} else {
				deintl_dma_base0 =
					gPMEM_VIQE_BASE + (gPMEM_VIQE_SIZE / 2U);
			}
		}

		tcc_viqe_swreset(nVIQE);
		check_wrap(deintl_dma_base0);
		deintl_dma_base1 = deintl_dma_base0 + imgSize;
		check_wrap(deintl_dma_base1);
		deintl_dma_base2 = deintl_dma_base1 + imgSize;
		check_wrap(deintl_dma_base2);
		deintl_dma_base3 = deintl_dma_base2 + imgSize;

		/* NOW top_size_dont_use is must 0, but this if statement is the intended code */
		/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
		if (top_size_dont_use == 0U) {
			framebufWidth = 0U;
			framebufHeight = 0U;
		}

		VIOC_VIQE_IgnoreDecError(pVIQE, 1U, 1U, 1U);
		VIOC_VIQE_SetControlRegister(pVIQE, framebufWidth, framebufHeight, (unsigned int)img_fmt);
		VIOC_VIQE_SetDeintlRegister(pVIQE, (unsigned int)img_fmt, top_size_dont_use,
			framebufWidth, framebufHeight, gDI_mode_30Hz,
			deintl_dma_base0, deintl_dma_base1,
			deintl_dma_base2, deintl_dma_base3);
		VIOC_VIQE_SetControlEnable(pVIQE, 0U, 0U, 0U, 0U, 1U);

		if (get_vioc_index(nRDMA) == get_vioc_index(VIOC_RDMA03)) {
			y2r_on = viqe_y2r_mode_check();
			VIOC_VIQE_SetImageY2RMode(pVIQE, 0x02);
			VIOC_VIQE_SetImageY2REnable(pVIQE, y2r_on);
		}

		(void)VIOC_CONFIG_PlugIn(nVIQE, nRDMA);

		if ((bool)viqe_arg->OddFirst) {
			gbfield_30Hz = 1;
			/* prevent KCS warning */
		} else {
			gbfield_30Hz = 0;
			/* prevent KCS warning */
		}

		(void)pr_info("[INF][VIQE] DEINTL-VIQE(%d)\n", nVIQE);
#else
		tcc_sdintls_swreset();
		(void)VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, nRDMA);
		gusingDI_S = 1;
		(void)pr_info("[INF][VIQE] DEINTL-S\n");
#endif
	}

	gFrmCnt_30Hz = 0U;
	gVIQE_Init_State = 1;

out:
	dprintk("%s---\n", __func__);
}

void TCC_VIQE_DI_Run(const struct VIQE_DI_TYPE *viqe_arg)
{
#ifndef USE_DEINTERLACE_S_IN30Hz
	VIOC_PlugInOutCheck VIOC_PlugIn;
	unsigned int JudderCnt = 0U;
#endif
	void __iomem *pRDMA, *pVIQE;
	unsigned int nRDMA = 0, nVIQE;

	if (viqe_arg->nRDMA != 0U) {
		nRDMA = VIOC_RDMA00 + viqe_arg->nRDMA;
		pRDMA = VIOC_RDMA_GetAddress(nRDMA);
	} else {
		pRDMA = main_m2m_info.pRDMABase_m2m;
		nRDMA = main_m2m_info.gVIQE_RDMA_num_m2m;
	}


	if (viqe_common_info.gBoard_stb == 1U) {
		if (gVIQE_Init_State == 0) {
			dprintk("%s VIQE block isn't initailized\n", __func__);
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto out;
		}
	}

	if ((bool)gUse_sDeintls) {
		VIOC_RDMA_SetImageY2REnable(pRDMA, 0U);
		/* Y2RMode Default 0 (Studio Color) */
		VIOC_RDMA_SetImageY2RMode(pRDMA, 0x02);

		if ((bool)viqe_arg->DI_use) {
			VIOC_RDMA_SetImageIntl(pRDMA, 1);
			(void)VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, nRDMA);
		} else {
			(void)VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
			VIOC_RDMA_SetImageIntl(pRDMA, 0);
		}
	} else {
	#ifndef USE_DEINTERLACE_S_IN30Hz
		if ((bool)not_exist_viqe1 || (bool)viqe_arg->use_Viqe0) {
			nVIQE = viqe_common_info.gVIOC_VIQE0;
			pVIQE = VIOC_VIQE_GetAddress(nVIQE);
		} else {
			nVIQE = viqe_common_info.gVIOC_VIQE1;
			pVIQE = VIOC_VIQE_GetAddress(nVIQE);
		}

		if (VIOC_CONFIG_Device_PlugState(nVIQE, &VIOC_PlugIn) == VIOC_DEVICE_INVALID) {
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto out;
		}

		if (!(bool)VIOC_PlugIn.enable) {
			(void)pr_warn("[WAR][VIQE] %s VIQE block isn't pluged!!!\n", __func__);
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto out;
		}

		if ((bool)viqe_arg->DI_use) {
			if ((bool)gbfield_30Hz) { // end fied of bottom field
				// change the bottom to top field
				VIOC_RDMA_SetImageBfield(pRDMA, 1);
				// if you want to change the base address, you call the RDMA SetImageBase function in this line.
				gbfield_30Hz = 0;
			} else {
				// change the top to bottom field
				VIOC_RDMA_SetImageBfield(pRDMA, 0);
				gbfield_30Hz = 1;
			}
			VIOC_RDMA_SetImageY2REnable(pRDMA, 0U);
			/* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageY2RMode(pRDMA, 0x02);

			if (gFrmCnt_30Hz >= 3U) {
				VIOC_VIQE_SetDeintlMode(pVIQE, VIOC_VIQE_DEINTL_MODE_3D);
				// Process JUDDER CNTS
				JudderCnt = ((viqe_arg->srcWidth + 64U) / 64U) - 1U;
				VIOC_VIQE_SetDeintlJudderCnt(pVIQE, JudderCnt);
				gDI_mode_30Hz = VIOC_VIQE_DEINTL_MODE_3D;
				dprintk("VIQE 3D and BField(%d)\n", gbfield_30Hz);
			} else {
				VIOC_VIQE_SetDeintlMode(pVIQE, VIOC_VIQE_DEINTL_MODE_2D);
				gDI_mode_30Hz = VIOC_VIQE_DEINTL_MODE_2D;
				dprintk("VIQE 2D and BField(%d)\n", gbfield_30Hz);
			}
			VIOC_VIQE_SetControlMode(pVIQE, 0U, 0U, 0U, 0U, 1U);
			VIOC_RDMA_SetImageIntl(pRDMA, 1);
		} else {
			VIOC_RDMA_SetImageY2REnable(pRDMA, 1U);
			/* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageY2RMode(pRDMA, 0x02);
			VIOC_VIQE_SetControlMode(pVIQE, 0U, 0U, 0U, 0U, 0U);
			VIOC_RDMA_SetImageIntl(pRDMA, 0);
			gFrmCnt_30Hz = 0U;
		}
	#else
		VIOC_RDMA_SetImageY2REnable(pRDMA, 0U);
		/* Y2RMode Default 0 (Studio Color) */
		VIOC_RDMA_SetImageY2RMode(pRDMA, 0x02);
		if (viqe_arg->DI_use) {
			VIOC_RDMA_SetImageIntl(pRDMA, 1);
			if (!(bool)gusingDI_S) {
				(void)VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, nRDMA);
				gusingDI_S = 1;
			}
		} else {
			VIOC_RDMA_SetImageIntl(pRDMA, 0);
			if ((bool)gusingDI_S) {
				(void)VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
				gusingDI_S = 0;
			}
		}
	#endif
	}

	check_wrap(gFrmCnt_30Hz);
	gFrmCnt_30Hz++;

out:
	dprintk("%s---\n", __func__);
}

void TCC_VIQE_DI_DeInit(const struct VIQE_DI_TYPE *viqe_arg)
{
	const void __iomem *pVIQE = NULL;
	unsigned int nVIQE = 0;

	gFrmCnt_30Hz = 0U;

	(void)pr_info("[INF][VIQE] %s\n", __func__);
	if ((bool)gUse_sDeintls) {
		(void)VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
		tcc_sdintls_swreset();
		gUse_sDeintls = 0;
	} else {
#ifndef USE_DEINTERLACE_S_IN30Hz
		if ((bool)not_exist_viqe1 || (bool)viqe_arg->use_Viqe0) {
			nVIQE = viqe_common_info.gVIOC_VIQE0;
			pVIQE = VIOC_VIQE_GetAddress(nVIQE);
		} else {
			nVIQE = viqe_common_info.gVIOC_VIQE1;
			pVIQE = VIOC_VIQE_GetAddress(nVIQE);
		}

		(void)VIOC_CONFIG_PlugOut(nVIQE);
		tcc_viqe_swreset(nVIQE);
#else
		(void)VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
		gusingDI_S = 0;
		tcc_sdintls_swreset();
#endif
	}

	gVIQE_Init_State = 0;
}

#ifdef CONFIG_USE_SUB_MULTI_FRAME
static int video_queue_delete(struct video_queue_t *q)
{
	int ret = 0;

	if (q == NULL) {
		(void)pr_err("[ERR][VIQE] queue data is NULL\n");
		ret = -1;
	} else {
		list_del(&q->list);
		kfree(q);
	}

	return ret;
}

static struct video_queue_t *video_queue_pop(void)
{
	struct video_queue_t *q;

	q = list_first_entry(&video_queue_list, struct video_queue_t, list);

	return q;
}

static int video_queue_delete_all(void)
{
	struct video_queue_t *q;
	int ret;

	while (!list_empty(&video_queue_list)) {
		q = video_queue_pop();
		ret = video_queue_delete(q);
	}

	return ret;
}

static int video_queue_push(int type)
{
	struct video_queue_t *q;
	int ret = 0;

	q = kzalloc(sizeof(struct video_queue_t), GFP_KERNEL);
	if (q == NULL) {
		(void)pr_err("[ERR][VIQE] %s can not alloc queue\n", __func__);
		ret = -1;
	} else {
		q->type = type;
		list_add_tail(&q->list, &video_queue_list);
	}

	return ret;
}

void TCC_VIQE_DI_Push60Hz_M2M(
	struct tcc_lcdc_image_update *input_image, int type)
{
	video_queue_push(type);
	(void)memcpy(&sub_m2m_image[type], input_image, sizeof(struct tcc_lcdc_image_update));
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(TCC_VIQE_DI_Push60Hz_M2M);
#endif

///////////////////////////////////////////////////////////////////////////////
void TCC_VIQE_DI_Init60Hz_M2M(enum TCC_OUTPUT_TYPE outputMode,
	const struct tcc_lcdc_image_update *input_image)
{
	unsigned int deintl_dma_base0, deintl_dma_base1;
	unsigned int deintl_dma_base2, deintl_dma_base3;
	unsigned int imgSize;
	/* If this value is OFF, The size information is get from VIOC modules. */
	unsigned int top_size_dont_use = 0U;
	unsigned int framebufWidth, framebufHeight;
	unsigned int img_fmt = (unsigned int)VIOC_VIQE_FMT_YUV420;
#ifdef CONFIG_TCC_VIOCMG
	unsigned int viqe_lock = 0;
#endif

#ifdef PRINT_OUT_M2M_MS
	time_gap_max_us = 0;
	time_gap_min_us = 0;
	time_gap_total_us = 0;
	time_gap_total_count = 0;
#endif

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_INIT, input_image, input_image->Lcdc_layer);
#endif

	if (TCC_VIQE_Scaler_Init_Buffer_M2M() < 0) {
		(void)pr_err("[ERR][VIQE] %s-%d : memory allocation is failed\n", __func__, __LINE__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	gOutputMode = outputMode;
	gUse_sDeintls = 0;

	if (input_image->deinterlace_mode == 0) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto m2m_init_scaler;
	}

#ifdef MAX_VIQE_FRAMEBUFFER
	framebufWidth = 1920;
	framebufHeight = 1088;
#else
	framebufWidth = ((input_image->Frame_width) >> 3) << 3; // 8bit align
	framebufHeight = ((input_image->Frame_height) >> 2) << 2; // 4bit align
#endif

	dprintk("%s, W:%d, H:%d, FMT:%s, OddFirst:%d\n", __func__,
		framebufWidth, framebufHeight,
		(img_fmt?"YUV422":"YUV420"), input_image->odd_first_flag);

	if (viqe_common_info.gBoard_stb == 1U) {
		if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
			VIOC_RDMA_SetImageDisable(sub_m2m_info.pRDMABase_m2m);
			/* prevent KCS warning */
		} else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
			VIOC_RDMA_SetImageDisable(main_m2m_info.pRDMABase_m2m);
		} else {
			dprintk("%s check layer\n", __func__);
		}

	    //TCC_OUTPUT_FB_ClearVideoImg();
	}

	if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
		VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, 0U);
		/* Y2RMode Default 0 (Studio Color) */
		VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02);
		VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
		VIOC_RDMA_SetImageBfield(sub_m2m_info.pRDMABase_m2m,
			input_image->odd_first_flag);

		tcc_viqe_swreset(viqe_common_info.gVIOC_VIQE1);

		// If you use 3D(temporal) De-interlace mode,
		// you have to set physical address for using DMA register.
		// If 2D(spatial) mode, these registers are ignored
		imgSize = (framebufWidth * (framebufHeight / 2U) * 4U * 3U / 2U);
		if (gPMEM_VIQE_SUB_SIZE < (imgSize*4)) {
			(void)pr_err("[ERR][VIQE] 2: Increase VIQE pmap size for Sub_m2m!! Current(0x%x)/Need(0x%x)\n",
				gPMEM_VIQE_SUB_SIZE, (imgSize*4));
			deintl_dma_base0 = gPMEM_VIQE_BASE;
		} else {
			deintl_dma_base0 = gPMEM_VIQE_SUB_BASE;
		}
		deintl_dma_base1 = deintl_dma_base0 + imgSize;
		deintl_dma_base2 = deintl_dma_base1 + imgSize;
		deintl_dma_base3 = deintl_dma_base2 + imgSize;

		/* NOW top_size_dont_use is must 0, but this if statement is the intended code */
		/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
		if (top_size_dont_use == 0U) {
			framebufWidth = 0U;
			framebufHeight = 0U;
		}

		VIOC_VIQE_SetControlRegister(viqe_common_info.pVIQE1, framebufWidth, framebufHeight, img_fmt);
		VIOC_VIQE_SetDeintlRegister(viqe_common_info.pVIQE1, img_fmt, top_size_dont_use,
			framebufWidth, framebufHeight, gDI_mode_60Hz,
			deintl_dma_base0, deintl_dma_base1,
			deintl_dma_base2, deintl_dma_base3);
		VIOC_VIQE_SetControlEnable(viqe_common_info.pVIQE1, 0U, 0U, 0U, 0U, 1U);

		(void)VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_VIQE1, sub_m2m_info.gVIQE_RDMA_num_m2m);

		atomic_set(&gFrmCnt_Sub_60Hz, 0);
		gVIQE1_Init_State = 1;
		(void)pr_info("[INF][VIQE] %s - VIQE1\n", __func__);
#else
		VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, 1U);
		/* Y2RMode Default 0 (Studio Color) */
		VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02);
		VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
		VIOC_RDMA_SetImageBfield(sub_m2m_info.pRDMABase_m2m, input_image->odd_first_flag);

		tcc_sdintls_swreset();
		(void)VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, sub_m2m_info.gDEINTLS_RDMA_num_m2m);
		gusingDI_S = 1;
		dprintk("%s - DEINTL-S\n", __func__);
#endif
	} else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
		VIOC_RDMA_SetImageY2REnable(main_m2m_info.pRDMABase_m2m, 0U);
		/* Y2RMode Default 0 (Studio Color) */
		VIOC_RDMA_SetImageY2RMode(main_m2m_info.pRDMABase_m2m, 0x02);
		VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 1);
		VIOC_RDMA_SetImageBfield(main_m2m_info.pRDMABase_m2m, input_image->odd_first_flag);

		#ifdef CONFIG_TCC_VIOCMG
		viqe_lock = viocmg_lock_viqe(VIOCMG_CALLERID_VOUT);
		if (viqe_lock == 0U)
		#endif
		{
			tcc_viqe_swreset(viqe_common_info.gVIOC_VIQE0);

			// If you use 3D(temporal) De-interlace mode,
			// you have to set physical address for using DMA reg.
			// If 2D(spatial) mode, these registers are ignored
			check_wrap(framebufWidth);
			check_wrap(framebufHeight);
			imgSize = (framebufWidth * (framebufHeight / 2U) * 4U * 3U / 2U);
			check_wrap(imgSize);
			if (gPMEM_VIQE_SIZE < (imgSize * 4U)) {
				(void)pr_warn("[WAR][VIQE] 3: Increase VIQE pmap size for Main_m2m!! Current(0x%x)/Need(0x%x)\n",
					gPMEM_VIQE_SIZE, (imgSize * 4U));
			}
			deintl_dma_base0 = gPMEM_VIQE_BASE;
			check_wrap(deintl_dma_base0);
			deintl_dma_base1 = deintl_dma_base0 + imgSize;
			check_wrap(deintl_dma_base1);
			deintl_dma_base2 = deintl_dma_base1 + imgSize;
			check_wrap(deintl_dma_base2);
			deintl_dma_base3 = deintl_dma_base2 + imgSize;

			/* NOW top_size_dont_use is must 0, but this if statement is the intended code */
			/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
			if (top_size_dont_use == 0U) {
				framebufWidth  = 0U;
				framebufHeight = 0U;
			}

			VIOC_VIQE_IgnoreDecError(viqe_common_info.pVIQE0, 1U, 1U, 1U);
			VIOC_VIQE_SetControlRegister(viqe_common_info.pVIQE0, framebufWidth, framebufHeight, img_fmt);
			VIOC_VIQE_SetDeintlRegister(viqe_common_info.pVIQE0, img_fmt, top_size_dont_use,
				framebufWidth, framebufHeight, gDI_mode_60Hz,
				deintl_dma_base0, deintl_dma_base1, deintl_dma_base2, deintl_dma_base3);
			VIOC_VIQE_SetControlEnable(viqe_common_info.pVIQE0, 0U, 0U, 0U, 0U, 1U);

			(void)VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_VIQE0, main_m2m_info.gVIQE_RDMA_num_m2m);

			atomic_set(&gFrmCnt_60Hz, 0);
			gVIQE_Init_State = 1;
		}
		(void)pr_info("[INF][VIQE] %s - VIQE0\n", __func__);

		#ifdef CONFIG_TCC_VIOCMG
		if (viqe_lock == 0U)
			viocmg_free_viqe(VIOCMG_CALLERID_VOUT);
		#endif
	} else {
		dprintk("%s check layer\n", __func__);
	}

m2m_init_scaler:
	if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
		TCC_VIQE_Scaler_Sub_Init60Hz_M2M(input_image);
	} else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
		TCC_VIQE_Scaler_Init60Hz_M2M(input_image);
#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
		if (input_image->private_data.dolbyVision_info.el_offset[0] != 0x00) {
			struct tcc_lcdc_image_update ImageInfo_sub;

			(void)memcpy(&ImageInfo_sub, input_image, sizeof(struct tcc_lcdc_image_update));
			ImageInfo_sub.Lcdc_layer = (unsigned int)RDMA_VIDEO_SUB;
			ImageInfo_sub.private_data.optional_info[VID_OPT_HAVE_MC_INFO] = 0;
			ImageInfo_sub.private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] = 0;
			TCC_VIQE_Scaler_Sub_Init60Hz_M2M(&ImageInfo_sub);
			DV_PROC_CHECK = DV_DUAL_MODE;
			dvprintk("%s-%d: Done(0x%x)\n", __func__, __LINE__, DV_PROC_CHECK);
		}
#endif
	} else {
		dprintk("%s check layer\n", __func__);
		/* prevent KCS warning */
	}

out:
	dprintk("%s---\n", __func__);
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(TCC_VIQE_DI_Init60Hz_M2M);

void TCC_VIQE_DI_Run60Hz_M2M(
	const struct tcc_lcdc_image_update *input_image, int reset_frmCnt)
{
	VIOC_PlugInOutCheck VIOC_PlugIn;
	unsigned int JudderCnt = 0;

#ifdef CONFIG_TCC_VIOCMG
	unsigned int viqe_lock = 0;
#endif

#ifdef PRINT_OUT_M2M_MS
	do_gettimeofday(&t1);
#endif

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_ON, input_image, input_image->Lcdc_layer);
#endif

	/*------TODO: VIQE PORTING Temporary---------*/
	#if 0
	if (Output_SelectMode == TCC_OUTPUT_NONE) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	#endif

	if (!(bool)input_image->deinterlace_mode) {
		if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, 1U);
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 0);
			TCC_VIQE_Scaler_Sub_Run60Hz_M2M(input_image);
		} else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
			VIOC_RDMA_SetImageY2REnable(main_m2m_info.pRDMABase_m2m, 0U);
			VIOC_RDMA_SetImageY2RMode(main_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 0);
			TCC_VIQE_Scaler_Run60Hz_M2M(input_image);
		} else {
			dprintk("%s check layer\n", __func__);
		}
	} else {
		if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, 1U);
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
			VIOC_RDMA_SetImageBfield(sub_m2m_info.pRDMABase_m2m,
				input_image->odd_first_flag);
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			if (gVIQE1_Init_State == 0) {
				(void)pr_err("[ERR][VIQE] %s VIQE1 isn't init\n", __func__);
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto out;
			}

			(void)VIOC_CONFIG_Device_PlugState(viqe_common_info.gVIOC_VIQE1, &VIOC_PlugIn);
			if (VIOC_PlugIn.enable == 0U) {
				(void)pr_err("[ERR][VIQE] %s VIQE1 isn't pluged\n", __func__);
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto out;
			}

			if ((bool)reset_frmCnt) {
				atomic_set(&gFrmCnt_Sub_60Hz, 0);
				dprintk("VIQE1 3D -> 2D\n");
			}

			VIOC_RDMA_SetImageBfield(sub_m2m_info.pRDMABase_m2m, input_image->odd_first_flag);
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, 0U);
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */

			if ((bool)input_image->frameInfo_interlace) {
				if (atomic_read(&gFrmCnt_Sub_60Hz) >= 3) {
					VIOC_VIQE_SetDeintlMode(viqe_common_info.pVIQE1, VIOC_VIQE_DEINTL_MODE_3D);
					// Process JUDDER CNTS
					JudderCnt = ((input_image->Frame_width + 64) / 64) - 1;
					VIOC_VIQE_SetDeintlJudderCnt(viqe_common_info.pVIQE1, JudderCnt);
					gDI_sub_mode_60Hz = VIOC_VIQE_DEINTL_MODE_3D;
				} else {
					VIOC_VIQE_SetDeintlMode(viqe_common_info.pVIQE1, VIOC_VIQE_DEINTL_MODE_2D);
					gDI_sub_mode_60Hz = VIOC_VIQE_DEINTL_MODE_2D;
				}
				VIOC_VIQE_SetControlMode(viqe_common_info.pVIQE1, 0U, 0U, 0U, 0U, 1U);
				VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
			} else {
				VIOC_VIQE_SetDeintlMode(viqe_common_info.pVIQE1, VIOC_VIQE_DEINTL_MODE_BYPASS);
				gDI_sub_mode_60Hz = VIOC_VIQE_DEINTL_MODE_BYPASS;
				VIOC_VIQE_SetControlMode(viqe_common_info.pVIQE1, 0U, 0U, 0U, 0U, 0U);
				VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 0);
				atomic_set(&gFrmCnt_Sub_60Hz, 0);
			}
			TCC_VIQE_Scaler_Sub_Run60Hz_M2M(input_image);
			atomic_inc(&gFrmCnt_Sub_60Hz);
#else
			if ((bool)input_image->frameInfo_interlace) {
				if (!(bool)gusingDI_S) {
					(void)VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, sub_m2m_info.gDEINTLS_RDMA_num_m2m);
					gusingDI_S = 1;
				}
				VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
			} else {
				if ((bool)gusingDI_S) {
					(void)VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
					gusingDI_S = 0;
				}
				VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 0);
			}
			TCC_VIQE_Scaler_Sub_Run60Hz_M2M(input_image);
#endif
		} else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
			#ifdef CONFIG_TCC_VIOCMG
			if (viocmg_lock_viqe(VIOCMG_CALLERID_VOUT) < 0) {
				if ((bool)gVIQE_Init_State) {
					if (scaler->data->dev_opened > 0) {
						scaler->data->dev_opened--;
						/* prevent KCS warning */
					}
					gVIQE_Init_State = 0;
				}
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto out;
			}

			viqe_lock = 1;
			if (gVIQE_Init_State == 0) {
				TCC_VIQE_DI_Init60Hz_M2M(gOutputMode, input_image);
				/* prevent KCS warning */
			}
			#endif

			if (gVIQE_Init_State == 0) {
				(void)pr_err("[ERR][VIQE] %s VIQE isn't init\n", __func__);
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto out;
			}

			(void)VIOC_CONFIG_Device_PlugState(viqe_common_info.gVIOC_VIQE0, &VIOC_PlugIn);
			if (!(bool)VIOC_PlugIn.enable) {
				(void)pr_err("[ERR][VIQE] %s VIQE isn't pluged\n", __func__);
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto out;
			}

			if ((bool)reset_frmCnt) {
				atomic_set(&gFrmCnt_60Hz, 0);
				dprintk("VIQE 3D -> 2D\n");
			}

			VIOC_RDMA_SetImageBfield(main_m2m_info.pRDMABase_m2m, input_image->odd_first_flag);
			VIOC_RDMA_SetImageY2REnable(main_m2m_info.pRDMABase_m2m, 0U);
			VIOC_RDMA_SetImageY2RMode(main_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */

			if ((bool)input_image->frameInfo_interlace) {
				if (atomic_read(&gFrmCnt_60Hz) >= 3) {
					VIOC_VIQE_SetDeintlMode(viqe_common_info.pVIQE0, VIOC_VIQE_DEINTL_MODE_3D);
					// Process JUDDER CNTS
					JudderCnt = ((input_image->Frame_width + 64U) / 64U) - 1U;
					VIOC_VIQE_SetDeintlJudderCnt(viqe_common_info.pVIQE0, JudderCnt);
					gDI_mode_60Hz = VIOC_VIQE_DEINTL_MODE_3D;
				} else {
					VIOC_VIQE_SetDeintlMode(viqe_common_info.pVIQE0, VIOC_VIQE_DEINTL_MODE_2D);
					gDI_mode_60Hz = VIOC_VIQE_DEINTL_MODE_2D;
				}
				VIOC_VIQE_SetControlMode(viqe_common_info.pVIQE0, 0U, 0U, 0U, 0U, 1U);
				VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 1);
			} else {
				VIOC_VIQE_SetDeintlMode(viqe_common_info.pVIQE0, VIOC_VIQE_DEINTL_MODE_BYPASS);
				gDI_mode_60Hz = VIOC_VIQE_DEINTL_MODE_BYPASS;
				VIOC_VIQE_SetControlMode(viqe_common_info.pVIQE0, 0U, 0U, 0U, 0U, 0U);
				VIOC_RDMA_SetImageIntl(main_m2m_info.pRDMABase_m2m, 0);
				atomic_set(&gFrmCnt_60Hz, 0);
			}

			TCC_VIQE_Scaler_Run60Hz_M2M(input_image);

			atomic_inc(&gFrmCnt_60Hz);
		} else {
			dprintk("%s check layer\n", __func__);
			/* prevent KCS warning */
		}
	}

	#ifdef CONFIG_TCC_VIOCMG
	if ((bool)viqe_lock) {
		viocmg_free_viqe(VIOCMG_CALLERID_VOUT);
		/* prevent KCS warning */
	}
	#endif

out:
	dprintk("%s---\n", __func__);
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(TCC_VIQE_DI_Run60Hz_M2M);

void TCC_VIQE_DI_DeInit60Hz_M2M(unsigned int layer)
{
	(void)pr_info("[INF][VIQE] %s - Layer:%d +\n", __func__, layer);
	#ifdef PRINT_OUT_M2M_MS
	(void)pr_info("[INF][VIQE] Time Diff(ms) Max(%2d.%2d)/Min(%2d.%2d) => Avg(%2d.%2d)\n",
		(unsigned int)(time_gap_max_us/1000),
		(unsigned int)(time_gap_max_us%1000),
		(unsigned int)(time_gap_min_us/1000),
		(unsigned int)(time_gap_min_us%1000),
		(unsigned int)((time_gap_total_us / time_gap_total_count)
			/ 1000),
		(unsigned int)((time_gap_total_us / time_gap_total_count)
			% 1000));
	#endif
	if (layer == (unsigned int)RDMA_VIDEO_SUB) {
		VIOC_RDMA_SetImageDisable(sub_m2m_info.pRDMABase_m2m);
		VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, 1U);
		/* Y2RMode Default 0 (Studio Color) */
		VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02);

		if ((bool)gusingDI_S) {
			(void)VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
			tcc_sdintls_swreset();

			gusingDI_S = 0;
		}

		#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
		atomic_set(&gFrmCnt_Sub_60Hz, 0);

		if ((bool)gVIQE1_Init_State) {
			(void)VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_VIQE1);
			tcc_viqe_swreset(viqe_common_info.gVIOC_VIQE1);

			gVIQE1_Init_State = 0;
		}
		#endif
		TCC_VIQE_Scaler_Sub_Release60Hz_M2M();
	}

	if (layer == (unsigned int)RDMA_VIDEO) {
		atomic_set(&gFrmCnt_60Hz, 0);

		VIOC_RDMA_SetImageDisable(main_m2m_info.pRDMABase_m2m);
		VIOC_RDMA_SetImageY2REnable(main_m2m_info.pRDMABase_m2m, 1U);
		/* Y2RMode Default 0 (Studio Color) */
		VIOC_RDMA_SetImageY2RMode(main_m2m_info.pRDMABase_m2m, 0x02);

		if ((bool)gVIQE_Init_State) {
			#ifdef CONFIG_TCC_VIOCMG
			if (viocmg_lock_viqe(VIOCMG_CALLERID_VOUT) == 0) {
				(void)VIOC_CONFIG_PlugOut(
					viqe_common_info.gVIOC_VIQE0);
				tcc_viqe_swreset(viqe_common_info.gVIOC_VIQE0);

				viocmg_free_viqe(VIOCMG_CALLERID_VOUT);
			}
			#else
			(void)VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_VIQE0);
			tcc_viqe_swreset(viqe_common_info.gVIOC_VIQE0);
			#endif
			gVIQE_Init_State = 0;
		}

		TCC_VIQE_Scaler_Release60Hz_M2M();
		#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
		dvprintk("%s-%d : Done(0x%x)\n",
			__func__, __LINE__, DV_PROC_CHECK);
		if ((DV_PROC_CHECK & DV_DUAL_MODE) == DV_DUAL_MODE) {
			(void)pr_info("[INF][VIQE] %s - Layer:%d +\n",
				__func__, RDMA_VIDEO_SUB);
			VIOC_RDMA_SetImageDisable(sub_m2m_info.pRDMABase_m2m);
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m,
				1U);
			/* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m,
				0x02);

			TCC_VIQE_Scaler_Sub_Release60Hz_M2M();
			(void)pr_info("[INF][VIQE] %s - Layer:%d -\n",
				__func__, RDMA_VIDEO_SUB);
			// will be inited tcc_scale_display_update()
			//DV_PROC_CHECK = 0;
			dvprintk("%s-%d : DV_PROC_CHECK(%d)\n",
				__func__, __LINE__, DV_PROC_CHECK);
		}
		#endif
	}
	#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_OFF, NULL, layer);
	#endif

	(void)pr_info("[INF][VIQE] %s - Layer:%d -\n", __func__, layer);
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(TCC_VIQE_DI_DeInit60Hz_M2M);

void TCC_VIQE_DI_Sub_Init60Hz_M2M(
	enum TCC_OUTPUT_TYPE outputMode,
	const struct tcc_lcdc_image_update *input_image)
{
#ifdef CONFIG_USE_SUB_MULTI_FRAME
	int type = VSYNC_MAIN;
#endif
#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_INIT, input_image, input_image->Lcdc_layer);
#endif

	if (atomic_read(&viqe_sub_m2m_init) == 0) {
		gOutputMode = outputMode;
		gUse_sDeintls = 0;

		if (TCC_VIQE_Scaler_Init_Buffer_M2M() < 0) {
			(void)pr_err("[ERR][VIQE] %s-%d: memory alloc is failed\n",
				__func__, __LINE__);
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto m2m_init_scaler;
		}

		if (!(bool)input_image->deinterlace_mode) {
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto m2m_init_scaler;
		}

		if (viqe_common_info.gBoard_stb == 1U) {
			if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
				VIOC_RDMA_SetImageDisable(sub_m2m_info.pRDMABase_m2m);
				/* prevent KCS warning */
			} else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
				VIOC_RDMA_SetImageDisable(main_m2m_info.pRDMABase_m2m);
				/* prevent KCS warning */
			} else {
				dprintk("%s check layer\n", __func__);
				/* prevent KCS warning */
			}
		}

		if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m,
				1U);
			/* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m,
				0x02);
			VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
			VIOC_RDMA_SetImageBfield(sub_m2m_info.pRDMABase_m2m,
				input_image->odd_first_flag);

			tcc_sdintls_swreset();
			(void)VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls,
				sub_m2m_info.gDEINTLS_RDMA_num_m2m);
			gusingDI_S = 1;
			dprintk("%s - DEINTL-S\n", __func__);
		}
m2m_init_scaler:
		TCC_VIQE_Scaler_Sub_Init60Hz_M2M(input_image);
		atomic_set(&viqe_sub_m2m_init, 1);

#ifdef CONFIG_USE_SUB_MULTI_FRAME
		if (input_image->mosaic_mode) {
			for (type = VSYNC_MAIN; type < VSYNC_MAX; type++)
				memset(&sub_m2m_image[type], 0x00,
					sizeof(struct tcc_lcdc_image_update));
		}
#endif
	}
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(TCC_VIQE_DI_Sub_Init60Hz_M2M);

void TCC_VIQE_DI_Sub_Run60Hz_M2M(
	const struct tcc_lcdc_image_update *input_image,
	int reset_frmCnt)
{
	(void)reset_frmCnt;

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_ON, input_image, input_image->Lcdc_layer);
#endif

	if ((bool)atomic_read(&viqe_sub_m2m_init)) {
		/*------TODO: VIQE PORTING Temporary---------*/
		#if 0
		if (Output_SelectMode == TCC_OUTPUT_NONE) {
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto out;
		}
		#endif

		if (!(bool)input_image->deinterlace_mode) {
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, 1U);
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 0);
			TCC_VIQE_Scaler_Sub_Run60Hz_M2M(input_image);
		} else {
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m, 1U);
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m, 0x02); /* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
			VIOC_RDMA_SetImageBfield(sub_m2m_info.pRDMABase_m2m, input_image->odd_first_flag);
			if ((bool)input_image->frameInfo_interlace) {
				if (!(bool)gusingDI_S) {
					(void)VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, sub_m2m_info.gDEINTLS_RDMA_num_m2m);
					gusingDI_S = 1;
				}
				VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 1);
			} else {
				if ((bool)gusingDI_S) {
					(void)VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
					gusingDI_S = 0;
				}
				VIOC_RDMA_SetImageIntl(sub_m2m_info.pRDMABase_m2m, 0);
			}
			TCC_VIQE_Scaler_Sub_Run60Hz_M2M(input_image);
		}
	}

//out:
	dprintk("%s---\n", __func__);
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(TCC_VIQE_DI_Sub_Run60Hz_M2M);

void TCC_VIQE_DI_Sub_DeInit60Hz_M2M(int layer)
{
	(void)pr_info("[INF][VIQE] %s - Layer:%d +\n", __func__, layer);

	if ((bool)atomic_read(&viqe_sub_m2m_init)) {
		if (layer == (int)RDMA_VIDEO_SUB) {
			VIOC_RDMA_SetImageDisable(sub_m2m_info.pRDMABase_m2m);
			VIOC_RDMA_SetImageY2REnable(sub_m2m_info.pRDMABase_m2m,
				1U);
			/* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageY2RMode(sub_m2m_info.pRDMABase_m2m,
				0x02);

			if ((bool)gusingDI_S) {
				(void)VIOC_CONFIG_PlugOut(
					viqe_common_info.gVIOC_Deintls);
				tcc_sdintls_swreset();

				gusingDI_S = 0;
			}

			TCC_VIQE_Scaler_Sub_Release60Hz_M2M();
		}

		atomic_set(&viqe_sub_m2m_init, 0);
	}

	(void)pr_info("[INF][VIQE] %s - Layer:%d -\n", __func__, layer);
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(TCC_VIQE_DI_Sub_DeInit60Hz_M2M);

int TCC_VIQE_Scaler_Init_Buffer_M2M(void)
{
	/* coverity[misra_c_2012_rule_5_7_violation : FALSE] */
	struct tccmisc_user_t pmap;
	unsigned int szBuffer = 0U;
	unsigned int buffer_cnt = 0U;
	int ret = 0;

	(void)scnprintf(pmap.name, sizeof(pmap.name), "%s", "viqe");
	if (tccmisc_pmap(&pmap) < 0) {
		(void)pr_err("[ERR][VIQE] get pmap viqe\n");
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	gPMEM_VIQE_BASE = u64_to_u32(pmap.base);
	gPMEM_VIQE_SIZE = u64_to_u32(pmap.size);

#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
	(void)scnprintf(pmap.name, sizeof(pmap.name), "%s", "viqe1");
	if (tccmisc_pmap(&pmap) < 0) {
		(void)pr_err("[ERR][VIQE] get pmap viqe1\n");
		ret = -2;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	gPMEM_VIQE_SUB_BASE = u64_to_u32(pmap.base);
	gPMEM_VIQE_SUB_SIZE = u64_to_u32(pmap.size);
	(void)pr_info("[INF][VIQE] pmap :: 0: 0x%x/0x%x, 1: 0x%x/0x%x\n", gPMEM_VIQE_BASE, gPMEM_VIQE_SIZE, gPMEM_VIQE_SUB_BASE, gPMEM_VIQE_SUB_SIZE);
#else
	(void)pr_info("[INF][VIQE] pmap :: 0: 0x%x/0x%x, 1: 0x%x/0x%x\n", gPMEM_VIQE_BASE, gPMEM_VIQE_SIZE, 0, 0);
#endif

	(void)scnprintf(pmap.name, sizeof(pmap.name), "%s", "overlay");
	if (tccmisc_pmap(&pmap) < 0) {
		(void)pr_err("[ERR][VIQE] get pmap overlay\n");
		ret = -3;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	if ((bool)pmap.size) {
		szBuffer = (u32)pmap.size / BUFFER_CNT_FOR_M2M_MODE;

		for (buffer_cnt = 0; buffer_cnt < BUFFER_CNT_FOR_M2M_MODE; buffer_cnt++) {
			overlay_buffer[0][buffer_cnt] = u64_to_u32(pmap.base) + (szBuffer * buffer_cnt);

			(void)pr_info("[INF][VIQE] Overlay-0 :: %d/%d = 0x%x\n",
				buffer_cnt, BUFFER_CNT_FOR_M2M_MODE,
				overlay_buffer[0][buffer_cnt]);
		}
	}

	(void)scnprintf(pmap.name, sizeof(pmap.name), "%s", "overlay1");
	if (tccmisc_pmap(&pmap) < 0) {
		(void)pr_err("[ERR][VIQE] get pmap overlay1\n");
		ret = -4;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	if ((bool)pmap.size) {
		szBuffer = (u32)pmap.size / BUFFER_CNT_FOR_M2M_MODE;

		for (buffer_cnt = 0; buffer_cnt < BUFFER_CNT_FOR_M2M_MODE; buffer_cnt++) {
			overlay_buffer[1][buffer_cnt] = u64_to_u32(pmap.base) + (szBuffer * buffer_cnt);
			(void)pr_info("[INF][VIQE] Overlay-1 :: %d/%d = 0x%x\n", buffer_cnt, BUFFER_CNT_FOR_M2M_MODE, overlay_buffer[1][buffer_cnt]);
		}
	}

	current_buffer_idx[0] = 0U;
	current_buffer_idx[1] = 0U;

out:
	return ret;
}

static unsigned int TCC_VIQE_Scaler_Get_Buffer_M2M(
	const struct tcc_lcdc_image_update *input_image)
{
	unsigned int buffer_addr = 0;
	unsigned int ret;

	if (input_image->Lcdc_layer != (unsigned int)RDMA_VIDEO) {
		ret = 0x00;
	} else {
		buffer_addr = overlay_buffer[0][current_buffer_idx[0]];
		check_wrap(current_buffer_idx[0]);
		current_buffer_idx[0] += 1U;
		if (current_buffer_idx[0] >= BUFFER_CNT_FOR_M2M_MODE) {
			current_buffer_idx[0] = 0U;
			/* prevent KCS warning */
		}

		ret = buffer_addr;
	}

	return ret;
}

static unsigned int TCC_VIQE_Scaler_Sub_Get_Buffer_M2M(
	const struct tcc_lcdc_image_update *input_image)
{
	unsigned int buffer_addr = 0;
	unsigned int ret;

	if (input_image->Lcdc_layer != (unsigned int)RDMA_VIDEO_SUB) {
		ret = 0x00;
	} else {
		buffer_addr = overlay_buffer[1][current_buffer_idx[1]];
		check_wrap(current_buffer_idx[1]);
		current_buffer_idx[1] += 1U;

		if (current_buffer_idx[1] >= BUFFER_CNT_FOR_M2M_MODE) {
			current_buffer_idx[1] = 0U;
			/* prevent KCS warning */
		}

		ret = buffer_addr;
	}

	return ret;
}

void TCC_VIQE_Scaler_Init60Hz_M2M(
	const struct tcc_lcdc_image_update *input_image)
{
	int ret = 0;

	check_wrap(scaler->vioc_intr->id);
	check_wrap(scaler->irq);

	if (!(bool)scaler->data->irq_reged) {
		// VIOC_CONFIG_StopRequest(1);
		synchronize_irq(scaler->irq);
		(void)vioc_intr_clear((int)scaler->vioc_intr->id, scaler->vioc_intr->bits);
		ret = request_irq(scaler->irq, TCC_VIQE_Scaler_Handler60Hz_M2M, IRQF_SHARED, "m2m-main", scaler);
		if ((bool)ret) {
			(void)pr_err("[ERR][VIQE] failed to acquire scaler1 request_irq\n");
			/* prevent KCS warning */
		}

		(void)vioc_intr_enable((int)scaler->irq, (int)scaler->vioc_intr->id, scaler->vioc_intr->bits);
		scaler->data->irq_reged = 1;
		(void)pr_info("[INF][VIQE] %s() : Out - M2M-Main ISR(%d) registered(%d)\n",
			__func__, scaler->irq, scaler->data->irq_reged);

#ifdef CONFIG_VIOC_MAP_DECOMP
		if (!(bool)gUse_MapConverter && (input_image->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0U)) {
			tca_map_convter_swreset(VIOC_MC1);
			gUse_MapConverter |= MAIN_USED;
			(void)pr_info("[INF][VIQE] Main-M2M :: MC Used\n");
		}
#endif
#ifdef CONFIG_VIOC_DTRC
		if (!(bool)gUse_DtrcConverter && (input_image->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0U)) {
			tca_dtrc_convter_swreset(VIOC_DTRC1);
			gUse_DtrcConverter |= MAIN_USED;
			(void)pr_info("[INF][VIQE] Main-M2M :: DTRC Used\n");
		}
#endif
	}

	scaler->data->dev_opened++;

	current_buffer_idx[0] = 0U;

	dvprintk("%s() : Out - open(%d)\n", __func__, scaler->data->dev_opened);
}

void TCC_VIQE_DI_GET_SOURCE_INFO(
	struct tcc_lcdc_image_update *input_image,
	unsigned int layer)
{
	if (layer == (unsigned int)RDMA_VIDEO) {
		(void)memcpy(input_image, scaler->info, sizeof(struct tcc_lcdc_image_update));
		/* prevent KCS warning */
	} else {
		(void)memcpy(input_image, scaler_sub->info, sizeof(struct tcc_lcdc_image_update));
		/* prevent KCS warning */
	}
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(TCC_VIQE_DI_GET_SOURCE_INFO);

void TCC_VIQE_Scaler_Run60Hz_M2M(
	const struct tcc_lcdc_image_update *input_image)
{
	unsigned int pSrcBase0 = 0, pSrcBase1 = 0, pSrcBase2 = 0;
	unsigned int crop_width;
	unsigned int compressed_type;
	unsigned int dest_fmt = (unsigned int)TCC_LCDC_IMG_FMT_YUYV;
	//unsigned int dest_fmt = (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL0;
	//unsigned int dest_fmt = (unsigned int)TCC_LCDC_IMG_FMT_COMP;

	void __iomem *pSC_RDMABase = scaler->rdma->reg;
	void __iomem *pSC_WMIXBase = scaler->wmix->reg;
	void __iomem *pSC_WDMABase = scaler->wdma->reg;
	void __iomem *pSC_SCALERBase = scaler->sc->reg;

	dvprintk("%s() : IN\n", __func__);

	(void)memcpy(scaler->info, input_image, sizeof(struct tcc_lcdc_image_update));
	(void)memcpy(&output_m2m_image, input_image, sizeof(struct tcc_lcdc_image_update));
	#ifdef CONFIG_VIOC_MAP_DECOMP
	output_m2m_image.private_data.optional_info[VID_OPT_HAVE_MC_INFO] = 0U;
	output_m2m_image.private_data.optional_info[VID_OPT_BIT_DEPTH] = 0U;
	#endif
	#ifdef CONFIG_VIOC_DTRC
	output_m2m_image.private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] = 0U;
	output_m2m_image.private_data.optional_info[VID_OPT_BIT_DEPTH] = 0U;
	#endif

	dprintk("%s : %d(%d), %d(%d), intl(%d/%d), addr(0x%x)\n", __func__,
		output_m2m_image.buffer_unique_id,
		output_m2m_image.odd_first_flag,
		input_image->buffer_unique_id, input_image->odd_first_flag,
		input_image->frameInfo_interlace, input_image->deinterlace_mode,
		input_image->addr0);

	check_wrap(scaler->info->crop_right);
	check_wrap(scaler->info->crop_left);
	crop_width = scaler->info->crop_right - scaler->info->crop_left;

	/* For Test
	 * Please check it - inbest
	 */
	scaler->info->offset_x = (scaler->info->offset_x >> 1U) << 1U;
	scaler->info->offset_y = (scaler->info->offset_y >> 1U) << 1U;
	scaler->info->Image_width = (scaler->info->Image_width >> 1U) << 1U;
	scaler->info->Image_height = (scaler->info->Image_height >> 1U) << 1U;

	#ifdef CONFIG_VIOC_MAP_DECOMP
	if (!(bool)gUse_MapConverter && (input_image->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0U)) {
		//tca_map_convter_swreset(VIOC_MC1);
		gUse_MapConverter |= MAIN_USED;
		(void)pr_info("[INF][VIQE] Main-M2M :: MC Used\n");
	}

	if ((scaler->info->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0U) && !(bool)(gUse_MapConverter & MAIN_USED)) {
		(void)pr_err("[ERR][VIQE] Main Map-Converter can't work because of gUse_MapConverter(0x%x)\n", gUse_MapConverter);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	#endif
	#ifdef CONFIG_VIOC_DTRC
	if (!(bool)gUse_DtrcConverter && (input_image->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0U)) {
		//tca_dtrc_convter_swreset(VIOC_DTRC1);
		gUse_DtrcConverter |= MAIN_USED;
		(void)pr_info("[INF][VIQE] Main-M2M :: DTRC Used\n");
	}

	if ((scaler->info->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0U) && !(bool)(gUse_DtrcConverter & MAIN_USED)) {
		(void)pr_err("[ERR][VIQE] Dtrc-Converter can't work because of gUse_DtrcConverter(0x%x)\n", gUse_DtrcConverter);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	#endif

	/*
	 * compressed_type
	 * ---------------
	 *  0 is non-compressed frame (priority 3)
	 *  VID_OPT_HAVE_DTRC_INFO is DRTC (priority 2)
	 *  VID_OPT_HAVE_MC_INFO is MapConv (priority 1)
	 */
	compressed_type = 0;
	#ifdef CONFIG_VIOC_DTRC
	if ((bool)(gUse_DtrcConverter & MAIN_USED) && (scaler->info->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0)) {
		compressed_type = VID_OPT_HAVE_DTRC_INFO;
		/* prevent KCS warning */
	}
	#endif
	#ifdef CONFIG_VIOC_MAP_DECOMP
	if ((bool)(gUse_MapConverter & MAIN_USED) && (scaler->info->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0U)) {
		compressed_type = (unsigned int)VID_OPT_HAVE_MC_INFO;
		/* prevent KCS warning */
	}
	#endif

	check_wrap(scaler->sc->id);

	#ifdef CONFIG_VIOC_MAP_DECOMP
	if (compressed_type == (unsigned int)VID_OPT_HAVE_MC_INFO) {
		dprintk("%s : Map-Converter Operation %d\n", __func__,
		scaler->info->private_data.optional_info[VID_OPT_HAVE_MC_INFO]);

		// scaler
		VIOC_SC_SetBypass(pSC_SCALERBase, 0);
		VIOC_SC_SetDstSize(pSC_SCALERBase, scaler->info->Image_width, scaler->info->Image_height);
		VIOC_SC_SetOutSize(pSC_SCALERBase, scaler->info->Image_width, scaler->info->Image_height);
		VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
		if ((int)scaler->sc->id != VIOC_CONFIG_GetScaler_PluginToRDMA(scaler->rdma->id)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			if ((bool)(__raw_readl(pSC_RDMABase + RDMACTRL) & RDMACTRL_IEN_MASK)) {
				VIOC_RDMA_SetImageDisable(pSC_RDMABase);
				/* prevent KCS warning */
			}
			(void)VIOC_CONFIG_PlugIn(scaler->sc->id, scaler->rdma->id);
		}
		VIOC_SC_SetUpdate(pSC_SCALERBase);

		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			(void)VIOC_CONFIG_DMAPath_UnSet((int)scaler->rdma->id);
			(void)VIOC_CONFIG_DMAPath_Set(scaler->rdma->id, VIOC_MC1);
		} else {
			#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			(void)VIOC_CONFIG_MCPath(scaler->wmix->id, VIOC_MC1);
			#endif
		}

		tca_map_convter_set(VIOC_MC1, scaler->info, 0);
	}
	#endif

	#ifdef CONFIG_VIOC_DTRC
	if (compressed_type == VID_OPT_HAVE_DTRC_INFO) {
		dprintk("%s : Dtrc-Converter Operation %d\n", __func__, scaler->info->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO]);

		// scaler
		VIOC_SC_SetBypass(pSC_SCALERBase, 0);
		VIOC_SC_SetDstSize(pSC_SCALERBase, scaler->info->Image_width, scaler->info->Image_height);
		VIOC_SC_SetOutSize(pSC_SCALERBase, scaler->info->Image_width, scaler->info->Image_height);
		VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
		if ((int)scaler->sc->id != VIOC_CONFIG_GetScaler_PluginToRDMA(scaler->rdma->id)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			if ((bool)(__raw_readl(pSC_RDMABase + RDMACTRL) & RDMACTRL_IEN_MASK)) {
				VIOC_RDMA_SetImageDisable(pSC_RDMABase);
				/* prevent KCS warning */
			}
			(void)VIOC_CONFIG_PlugIn(scaler->sc->id, scaler->rdma->id);
		}
		VIOC_SC_SetUpdate(pSC_SCALERBase);

		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			(void)VIOC_CONFIG_DMAPath_UnSet((int)scaler->rdma->id);
			(void)VIOC_CONFIG_DMAPath_Set(scaler->rdma->id, VIOC_DTRC1);
		}

		tca_dtrc_convter_set(VIOC_DTRC1, scaler->info, 0);
	}
	#endif

	if (compressed_type == 0U) {
		#ifdef CONFIG_VIOC_MAP_DECOMP
		if ((bool)(gUse_MapConverter & MAIN_USED)) {
			if ((bool)VIOC_CONFIG_DMAPath_Support()) {
				(void)VIOC_CONFIG_DMAPath_UnSet((int)VIOC_MC1);
				(void)VIOC_CONFIG_DMAPath_Set(scaler->rdma->id, scaler->rdma->id);
			} else {
				#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
				(void)VIOC_CONFIG_MCPath(scaler->wmix->id, scaler->rdma->id);
				#endif
			}
			gUse_MapConverter &= ~(MAIN_USED);
		}
		#endif
		#ifdef CONFIG_VIOC_DTRC
		if ((bool)(gUse_DtrcConverter & MAIN_USED)) {
			if ((bool)VIOC_CONFIG_DMAPath_Support()) {
				(void)VIOC_CONFIG_DMAPath_UnSet((int)VIOC_DTRC1);
				(void)VIOC_CONFIG_DMAPath_Set(scaler->rdma->id, scaler->rdma->id);
			}
			gUse_DtrcConverter &= ~(MAIN_USED);
		}
		#endif
		dprintk("%s : Normal Operation %d, RDMA(0x%p/0x%p)\n",
			__func__,
		scaler->info->private_data.optional_info[VID_OPT_BIT_DEPTH],
			pSC_RDMABase,
			VIOC_RDMA_GetAddress(scaler->rdma->id));

		if ((bool)scaler->settop_support) {
			VIOC_RDMA_SetImageAlphaSelect(pSC_RDMABase, 1);
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		} else {
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		}
		VIOC_RDMA_SetImageFormat(pSC_RDMABase, scaler->info->fmt);

		//interlaced frame process ex) MPEG2
		if ((bool)0/*scaler->info->interlaced*/) {
			//VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler->info->crop_right - scaler->info->crop_left), (scaler->info->crop_bottom - scaler->info->crop_top) / 2U);
			//VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler->info->fmt, scaler->info->Frame_width * 2U);
		}
		#ifdef CONFIG_VIOC_10BIT
		else if (scaler->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10) {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler->info->crop_right - scaler->info->crop_left), (scaler->info->crop_bottom - scaler->info->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler->info->fmt, scaler->info->Frame_width * 2U);
		} else if (scaler->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11) {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler->info->crop_right - scaler->info->crop_left), (scaler->info->crop_bottom - scaler->info->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler->info->fmt, scaler->info->Frame_width * 125U / 100U);
		}
		#endif//
		else {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler->info->crop_right - scaler->info->crop_left), (scaler->info->crop_bottom - scaler->info->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler->info->fmt, scaler->info->Frame_width);
		}

		pSrcBase0 = (unsigned int)scaler->info->addr0;
		pSrcBase1 = (unsigned int)scaler->info->addr1;
		pSrcBase2 = (unsigned int)scaler->info->addr2;

		if (scaler->info->fmt >= VIOC_IMG_FMT_444SEP) {
			// address limitation!!
			dprintk("%s():  src addr is not allocate\n", __func__);
			scaler->info->crop_left = (scaler->info->crop_left >> 3U) << 3U;
			scaler->info->crop_right = scaler->info->crop_left + crop_width;
			scaler->info->crop_right = scaler->info->crop_left + (scaler->info->crop_right - scaler->info->crop_left);
			tcc_get_addr_yuv(scaler->info->fmt, scaler->info->addr0,
				scaler->info->Frame_width, scaler->info->Frame_height,
				scaler->info->crop_left, scaler->info->crop_top,
				&pSrcBase0, &pSrcBase1, &pSrcBase2);
		}
		VIOC_RDMA_SetImageY2REnable(pSC_RDMABase, 0);
		VIOC_RDMA_SetImageBase(pSC_RDMABase, (unsigned int)pSrcBase0, (unsigned int)pSrcBase1, (unsigned int)pSrcBase2);

		#ifdef CONFIG_VIOC_10BIT
		if (scaler->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10) {
			/* YUV 10bit support */
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x1, 0x1);
		} else if (
		scaler->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11) {
			/* YUV real 10bit support */
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x3, 0x0);
		} else {
			/* YUV 8bit support */
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x0, 0x0);
		}
		#endif

		VIOC_SC_SetBypass(pSC_SCALERBase, 0);
		VIOC_SC_SetDstSize(pSC_SCALERBase, scaler->info->Image_width, scaler->info->Image_height);
		VIOC_SC_SetOutSize(pSC_SCALERBase, scaler->info->Image_width, scaler->info->Image_height);
		VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
		if ((int)scaler->sc->id != VIOC_CONFIG_GetScaler_PluginToRDMA(scaler->rdma->id)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			if ((bool)(__raw_readl(pSC_RDMABase + RDMACTRL) & RDMACTRL_IEN_MASK)) {
				VIOC_RDMA_SetImageDisable(pSC_RDMABase);
				/* prevent KCS warning */
			}
			(void)VIOC_CONFIG_PlugIn(scaler->sc->id, scaler->rdma->id);
		}
		VIOC_SC_SetUpdate(pSC_SCALERBase);

		VIOC_RDMA_SetImageEnable(pSC_RDMABase); // SoC guide info.
	}

	(void)VIOC_CONFIG_WMIXPath(scaler->rdma->id, 1); // wmixer op is mixing mode.
	VIOC_WMIX_SetSize(pSC_WMIXBase, scaler->info->Image_width, scaler->info->Image_height);
	VIOC_WMIX_SetUpdate(pSC_WMIXBase);

	VIOC_WDMA_SetImageFormat(pSC_WDMABase, dest_fmt);
	VIOC_WDMA_SetImageSize(pSC_WDMABase, scaler->info->Image_width, scaler->info->Image_height);
	VIOC_WDMA_SetImageOffset(pSC_WDMABase, dest_fmt, scaler->info->Image_width);
	if (scaler->info->dst_addr0 == 0U) {
		scaler->info->dst_addr0 = TCC_VIQE_Scaler_Get_Buffer_M2M(input_image);
		/* prevent KCS warning */
	}
	VIOC_WDMA_SetImageBase(pSC_WDMABase,
		(unsigned int)scaler->info->dst_addr0,
		(unsigned int)scaler->info->dst_addr1,
		(unsigned int)scaler->info->dst_addr2);

	VIOC_WDMA_SetImageR2YEnable(pSC_WDMABase, 0);
	VIOC_WDMA_SetImageEnable(pSC_WDMABase, 0);
	// wdma status register all clear.
	VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK);

	output_m2m_image.Frame_width = scaler->info->Image_width;
	output_m2m_image.Frame_height = scaler->info->Image_height;
	output_m2m_image.Image_width = scaler->info->Image_width;
	output_m2m_image.Image_height = scaler->info->Image_height;
	output_m2m_image.crop_left = 0U;
	output_m2m_image.crop_top = 0U;
	output_m2m_image.crop_right = scaler->info->Image_width;
	output_m2m_image.crop_bottom = scaler->info->Image_height;
	output_m2m_image.fmt = dest_fmt;
	output_m2m_image.addr0 = scaler->info->dst_addr0;
	output_m2m_image.addr1 = scaler->info->dst_addr1;
	output_m2m_image.addr2 = scaler->info->dst_addr2;

	#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	dvprintk("%s-%d : Done(0x%x)\n", __func__, __LINE__, DV_PROC_CHECK);
	if ((DV_PROC_CHECK & DV_DUAL_MODE) == DV_DUAL_MODE) {
		struct tcc_lcdc_image_update el_ImageInfo;
		unsigned int ratio = 1U;

		if (tca_edr_el_configure(input_image, &el_ImageInfo, &ratio) >= 0) {
			TCC_VIQE_Scaler_Sub_Run60Hz_M2M(&el_ImageInfo);
		}
	}
	#endif

#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC)
out:
#endif
	dvprintk("%s() : OUT\n", __func__);
}

void TCC_VIQE_Scaler_Release60Hz_M2M(void)
{
	dprintk("%s():  In -open(%d), irq(%d)\n",
		__func__, scaler->data->dev_opened, scaler->data->irq_reged);

	if (scaler->data->dev_opened > 0U) {
		scaler->data->dev_opened--;
		/* prevent KCS warning */
	}

	check_wrap(scaler->irq);
	check_wrap(scaler->vioc_intr->id);
	check_wrap(scaler->irq);

	if (scaler->data->dev_opened == 0U) {
		if ((bool)scaler->data->irq_reged) {
			(void)vioc_intr_disable((int)scaler->irq, (int)scaler->vioc_intr->id, scaler->vioc_intr->bits);
			(void)free_irq(scaler->irq, scaler);
			(void)vioc_intr_clear((int)scaler->vioc_intr->id, scaler->vioc_intr->bits);
			scaler->data->irq_reged = 0;
		}

	#ifdef CONFIG_VIOC_MAP_DECOMP
		if ((bool)(gUse_MapConverter & MAIN_USED)) {
			if ((bool)VIOC_CONFIG_DMAPath_Support()) {
				(void)VIOC_CONFIG_DMAPath_UnSet((int)VIOC_MC1);
				(void)VIOC_CONFIG_DMAPath_Set(scaler->rdma->id, scaler->rdma->id);
			} else {
				#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
				(void)VIOC_CONFIG_MCPath(scaler->wmix->id, scaler->rdma->id);
				#endif
			}
		}
	#endif
	#ifdef CONFIG_VIOC_DTRC
		if ((bool)(gUse_DtrcConverter & MAIN_USED)) {
			if ((bool)VIOC_CONFIG_DMAPath_Support()) {
				(void)VIOC_CONFIG_DMAPath_UnSet((int)VIOC_DTRC1);
				(void)VIOC_CONFIG_DMAPath_Set(scaler->rdma->id, scaler->rdma->id);
			}
		}
	#endif
		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			(void)VIOC_CONFIG_DMAPath_Set(scaler->rdma->id, scaler->rdma->id);
			/* prevent KCS warning */
		} else {
			#if defined(CONFIG_VIOC_MAP_DECOMP) && (defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X))
			(void)VIOC_CONFIG_MCPath(scaler->wmix->id, scaler->rdma->id);
			#endif
		}

		(void)VIOC_CONFIG_PlugOut(scaler->sc->id);

		VIOC_CONFIG_SWReset(scaler->wdma->id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->wmix->id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->sc->id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->rdma->id, VIOC_CONFIG_RESET);

		VIOC_CONFIG_SWReset(scaler->rdma->id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->sc->id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->wmix->id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->wdma->id, VIOC_CONFIG_CLEAR);

	#ifdef CONFIG_VIOC_MAP_DECOMP
		if ((bool)(gUse_MapConverter & MAIN_USED)) {
			tca_map_convter_swreset(VIOC_MC1);
			gUse_MapConverter &= ~(MAIN_USED);
		}
	#endif
	#ifdef CONFIG_VIOC_DTRC
		if ((bool)(gUse_DtrcConverter & MAIN_USED)) {
			tca_dtrc_convter_swreset(VIOC_DTRC1);
			gUse_DtrcConverter &= ~(MAIN_USED);
		}
	#endif
	}

	dprintk("%s() : Out - open(%d)\n", __func__, scaler->data->dev_opened);
}

void TCC_VIQE_Scaler_Sub_Init60Hz_M2M(
	const struct tcc_lcdc_image_update *input_image)
{
	int ret = 0;

	check_wrap(scaler_sub->vioc_intr->id);
	check_wrap(scaler_sub->irq);

	if (!(bool)scaler_sub->data->irq_reged) {
		// VIOC_CONFIG_StopRequest(1);

		synchronize_irq(scaler_sub->irq);
		(void)vioc_intr_clear((int)scaler_sub->vioc_intr->id, scaler_sub->vioc_intr->bits);

		ret = request_irq(scaler_sub->irq, TCC_VIQE_Scaler_Sub_Handler60Hz_M2M, IRQF_SHARED, "m2m-sub", scaler_sub);
		if ((bool)ret) {
			(void)pr_info("failed to acquire scaler3 request_irq\n");
			/* prevent KCS warning */
		}

		(void)vioc_intr_enable((int)scaler_sub->irq, (int)scaler_sub->vioc_intr->id, scaler_sub->vioc_intr->bits);
		scaler_sub->data->irq_reged = 1;
		(void)pr_info("[INF][VIQE] %s() : Out - M2M-Sub ISR(%d) registered(%d)\n", __func__, scaler_sub->irq, scaler_sub->data->irq_reged);

#ifdef CONFIG_VIOC_MAP_DECOMP
		if (!(bool)gUse_MapConverter && (input_image->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0U)) {
			tca_map_convter_swreset(VIOC_MC1);
			gUse_MapConverter |= SUB_USED;
			(void)pr_info("[INF][VIQE] Sub-M2M :: MC Used\n");
		}
#endif
#ifdef CONFIG_VIOC_DTRC
		if (!(bool)gUse_DtrcConverter && (input_image->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0U)) {
			tca_dtrc_convter_swreset(VIOC_DTRC1);
			gUse_DtrcConverter |= SUB_USED;
			(void)pr_info("[INF][VIQE] Sub-M2M :: DTRC Used\n");
		}
#endif
	}

	scaler_sub->data->dev_opened++;

	current_buffer_idx[1] = 0U;

	dvprintk("%s() : Out - open(%d)\n", __func__, scaler_sub->data->dev_opened);
}

#ifdef CONFIG_USE_SUB_MULTI_FRAME
void TCC_VIQE_Scaler_Sub_Repeat60Hz_M2M(
	struct tcc_lcdc_image_update *input_image)
{
	unsigned int pSrcBase0 = 0, pSrcBase1 = 0, pSrcBase2 = 0;
	unsigned int crop_width;
	unsigned int dest_fmt = TCC_LCDC_IMG_FMT_YUYV;
	//unsigned int dest_fmt = TCC_LCDC_IMG_FMT_YUV420ITL0;
	//unsigned int dest_fmt = TCC_LCDC_IMG_FMT_COMP;

	void __iomem *pSC_RDMABase = scaler_sub->rdma->reg;
	//void __iomem *pSC_WMIXBase = scaler_sub->wmix->reg;
	void __iomem *pSC_WDMABase = scaler_sub->wdma->reg;
	void __iomem *pSC_SCALERBase = scaler_sub->sc->reg;

	dvprintk("%s() : IN\n", __func__);

	if ((bool)input_image->mosaic_mode) {
		if ((bool)scaler_sub->settop_support) {
			VIOC_RDMA_SetImageAlphaSelect(pSC_RDMABase, 1);
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		} else {
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		}
		VIOC_RDMA_SetImageFormat(pSC_RDMABase, input_image->fmt);

#ifdef CONFIG_VIOC_10BIT
		if (input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10U) {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (input_image->crop_right - input_image->crop_left), (input_image->crop_bottom - input_image->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, input_image->fmt, input_image->Frame_width * 2);
		} else if (
		input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11U) {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (input_image->crop_right - input_image->crop_left), (input_image->crop_bottom - input_image->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, input_image->fmt, input_image->Frame_width * 125 / 100);
		}
#endif
		else {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (input_image->crop_right - input_image->crop_left), (input_image->crop_bottom - input_image->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, input_image->fmt, input_image->Frame_width);
		}

		pSrcBase0 = (unsigned int)input_image->addr0;
		pSrcBase1 = (unsigned int)input_image->addr1;
		pSrcBase2 = (unsigned int)input_image->addr2;

		// address limitation!!
		if (input_image->fmt >= VIOC_IMG_FMT_444SEP) {
			dprintk("%s():  src addr is not allocate\n", __func__);
			input_image->crop_left = (input_image->crop_left >> 3) << 3;
			input_image->crop_right = input_image->crop_left + crop_width;
			input_image->crop_right = input_image->crop_left + (input_image->crop_right
					- input_image->crop_left);
			tcc_get_addr_yuv(input_image->fmt, input_image->addr0,
				input_image->Frame_width, input_image->Frame_height,
				input_image->crop_left, input_image->crop_top,
				&pSrcBase0, &pSrcBase1, &pSrcBase2);
		}
		VIOC_RDMA_SetImageY2REnable(pSC_RDMABase, 0);
		VIOC_RDMA_SetImageBase(pSC_RDMABase, (unsigned int)pSrcBase0, (unsigned int)pSrcBase1, (unsigned int)pSrcBase2);

#ifdef CONFIG_VIOC_10BIT
		if (input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10U) {
			/* YUV 10bit support */
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x1, 0x1);
		} else if (
			input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11U) {
			/* YUV real 10bit support */
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x3, 0x0);
		} else {
			/* YUV 8bit support */
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x0, 0x0);
		}
#endif

		VIOC_SC_SetBypass(pSC_SCALERBase, 0);
		VIOC_SC_SetDstSize(pSC_SCALERBase, input_image->buffer_Image_width, input_image->buffer_Image_height);
		VIOC_SC_SetOutSize(pSC_SCALERBase, input_image->buffer_Image_width, input_image->buffer_Image_height);
		VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
		(void)VIOC_CONFIG_PlugIn(scaler_sub->sc->id, scaler_sub->rdma->id);
		VIOC_SC_SetUpdate(pSC_SCALERBase);
		// SoC guide info.
		VIOC_RDMA_SetImageEnable(pSC_RDMABase);
		// wmixer op is bypass mode.
		(void)VIOC_CONFIG_WMIXPath(scaler_sub->rdma->id, 0);

		VIOC_WDMA_SetImageFormat(pSC_WDMABase, dest_fmt);
		VIOC_WDMA_SetImageSize(pSC_WDMABase, input_image->buffer_Image_width, input_image->buffer_Image_height);
		VIOC_WDMA_SetImageOffset(pSC_WDMABase, dest_fmt, input_image->Image_width);

		input_image->dst_addr0 = scaler_sub->info->dst_addr0;
		if ((bool)input_image->dst_addr0) {
			unsigned int pDstBase0, pDstBase1, pDstBase2;

			pDstBase0 = pDstBase1 = pDstBase2 = 0;
			tcc_get_addr_yuv(dest_fmt, input_image->dst_addr0,
				input_image->Image_width, input_image->Image_height,
				((input_image->buffer_offset_x - input_image->offset_x) >> 1) << 1,
				((input_image->buffer_offset_y - input_image->offset_y) >> 1) << 1,
				&pDstBase0, &pDstBase1, &pDstBase2);
			VIOC_WDMA_SetImageBase(pSC_WDMABase, pDstBase0, pDstBase1, pDstBase2);
		}
		VIOC_WDMA_SetImageR2YEnable(pSC_WDMABase, 0);
		VIOC_WDMA_SetImageEnable(pSC_WDMABase, 0);
		// wdma status register all clear.
		VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK);
	}

	dvprintk("%s() : OUT\n", __func__);
}
#endif

void TCC_VIQE_Scaler_Sub_Run60Hz_M2M(
	const struct tcc_lcdc_image_update *input_image)
{
	unsigned int pSrcBase0 = 0U, pSrcBase1 = 0U, pSrcBase2 = 0U;
	unsigned int crop_width;
	unsigned int compressed_type;
	unsigned int dest_fmt = (unsigned int)TCC_LCDC_IMG_FMT_YUYV;
	//unsigned int dest_fmt = TCC_LCDC_IMG_FMT_YUV420ITL0;
	//unsigned int dest_fmt = TCC_LCDC_IMG_FMT_COMP;

	void __iomem *pSC_RDMABase = scaler_sub->rdma->reg;
	//void __iomem *pSC_WMIXBase = scaler_sub->wmix->reg;
	void __iomem *pSC_WDMABase = scaler_sub->wdma->reg;
	void __iomem *pSC_SCALERBase = scaler_sub->sc->reg;

	dvprintk("%s() : IN\n", __func__);

	(void)memcpy(scaler_sub->info, input_image, sizeof(struct tcc_lcdc_image_update));
	(void)memcpy(&output_m2m_image_sub, input_image, sizeof(struct tcc_lcdc_image_update));
#ifdef CONFIG_VIOC_MAP_DECOMP
	output_m2m_image_sub.private_data.optional_info[VID_OPT_HAVE_MC_INFO] = 0;
	output_m2m_image_sub.private_data.optional_info[VID_OPT_BIT_DEPTH] = 0U;
#endif
#ifdef CONFIG_VIOC_DTRC
	output_m2m_image_sub.private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] = 0;
	output_m2m_image_sub.private_data.optional_info[VID_OPT_BIT_DEPTH] = 0U;
#endif

	dvprintk("%s : %d(%d), %d(%d), intl(%d/%d)\n", __func__,
		output_m2m_image_sub.buffer_unique_id,
		output_m2m_image_sub.odd_first_flag,
		input_image->buffer_unique_id,
		input_image->odd_first_flag,
		input_image->frameInfo_interlace,
		input_image->deinterlace_mode);

	check_wrap(scaler_sub->info->crop_right);
	check_wrap(scaler_sub->info->crop_left);
	crop_width = scaler_sub->info->crop_right - scaler_sub->info->crop_left;

#ifdef CONFIG_VIOC_MAP_DECOMP
	if (!(bool)gUse_MapConverter && (input_image->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0U)) {
		//tca_map_convter_swreset(VIOC_MC1);
		gUse_MapConverter |= SUB_USED;
		(void)pr_info("[INF][VIQE] Sub-M2M :: MC Used\n");
	}

	if ((scaler_sub->info->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0U) && !(bool)(gUse_MapConverter & SUB_USED)) {
		(void)pr_err("[ERR][VIQE] Sub Map-Converter can't work because of gUse_MapConverter(0x%x)\n", gUse_MapConverter);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
#endif
#ifdef CONFIG_VIOC_DTRC
	if (!(bool)gUse_DtrcConverter && (input_image->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0U)) {
		//tca_dtrc_convter_swreset(VIOC_DTRC1);
		gUse_DtrcConverter |= SUB_USED;
		(void)pr_info("[INF][VIQE] Sub-M2M :: DTRC Used\n");
	}

	if ((scaler_sub->info->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0U) && !(bool)(gUse_DtrcConverter & SUB_USED)) {
		(void)pr_err("[ERR][VIQE] Dtrc-Converter can't work because of gUse_MapConverter(0x%x)\n", gUse_DtrcConverter);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
#endif

	/*
	 * compressed_type
	 * ---------------
	 *  0 is non-compressed frame (priority 3)
	 *  VID_OPT_HAVE_DTRC_INFO is DRTC (priority 2)
	 *  VID_OPT_HAVE_MC_INFO is MapConv (priority 1)
	 */
	compressed_type = 0U;
	#ifdef CONFIG_VIOC_DTRC
 	if ((bool)(gUse_DtrcConverter & SUB_USED) && (scaler_sub->info->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0U)) {
		compressed_type = VID_OPT_HAVE_DTRC_INFO;
		/* prevent KCS warning */
	}
	#endif
	#ifdef CONFIG_VIOC_MAP_DECOMP
	if ((bool)(gUse_MapConverter & SUB_USED) && (scaler_sub->info->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0U)) {
		compressed_type = (unsigned int)VID_OPT_HAVE_MC_INFO;
		/* prevent KCS warning */
	}
	#endif

	check_wrap(scaler_sub->sc->id);

	#ifdef CONFIG_VIOC_MAP_DECOMP
	if (compressed_type == (unsigned int)VID_OPT_HAVE_MC_INFO) {
		// scaler
		VIOC_SC_SetBypass(pSC_SCALERBase, 0);
		VIOC_SC_SetDstSize(pSC_SCALERBase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
		VIOC_SC_SetOutSize(pSC_SCALERBase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
		VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
		if ((int)scaler_sub->sc->id != VIOC_CONFIG_GetScaler_PluginToRDMA(scaler_sub->rdma->id)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			if ((bool)(__raw_readl(pSC_RDMABase + RDMACTRL) & RDMACTRL_IEN_MASK)) {
				VIOC_RDMA_SetImageDisable(pSC_RDMABase);
				/* prevent KCS warning */
			}
			(void)VIOC_CONFIG_PlugIn(scaler_sub->sc->id, scaler_sub->rdma->id);
		}
		VIOC_SC_SetUpdate(pSC_SCALERBase);

		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			(void)VIOC_CONFIG_DMAPath_UnSet((int)scaler_sub->rdma->id);
			(void)VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, VIOC_MC1);
		} else {
			#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			(void)VIOC_CONFIG_MCPath(scaler_sub->wmix->id, VIOC_MC1);
			#endif
		}

		tca_map_convter_set(VIOC_MC1, scaler_sub->info, 0);
	}
	#endif

	#ifdef CONFIG_VIOC_DTRC
	if (compressed_type == VID_OPT_HAVE_DTRC_INFO) {
		// scaler
		VIOC_SC_SetBypass(pSC_SCALERBase, 0);
		VIOC_SC_SetDstSize(pSC_SCALERBase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
		VIOC_SC_SetOutSize(pSC_SCALERBase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
		VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
		if ((int)scaler_sub->sc->id != VIOC_CONFIG_GetScaler_PluginToRDMA(scaler_sub->rdma->id)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			if (__raw_readl(pSC_RDMABase + RDMACTRL) & RDMACTRL_IEN_MASK) {
				VIOC_RDMA_SetImageDisable(pSC_RDMABase);
				/* prevent KCS warning */
			}
			(void)VIOC_CONFIG_PlugIn(scaler_sub->sc->id, scaler_sub->rdma->id);
		}
		VIOC_SC_SetUpdate(pSC_SCALERBase);

		if (VIOC_CONFIG_DMAPath_Support()) {
			(void)VIOC_CONFIG_DMAPath_UnSet((int)scaler_sub->rdma->id);
			(void)VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, VIOC_DTRC1);
		}

		tca_dtrc_convter_set(VIOC_DTRC1, scaler_sub->info, 0);
	}
	#endif

	if (compressed_type == 0U) {
		#ifdef CONFIG_VIOC_MAP_DECOMP
		if ((bool)(gUse_MapConverter & SUB_USED)) {
			if ((bool)VIOC_CONFIG_DMAPath_Support()) {
				(void)VIOC_CONFIG_DMAPath_UnSet((int)VIOC_MC1);
				(void)VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, scaler_sub->rdma->id);
			} else {
				#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
				(void)VIOC_CONFIG_MCPath(scaler_sub->wmix->id, scaler_sub->rdma->id);
				#endif
			}
			gUse_MapConverter &= ~(SUB_USED);
		}
		#endif

		#ifdef CONFIG_VIOC_DTRC
		if ((bool)(gUse_DtrcConverter & SUB_USED)) {
			if ((bool)VIOC_CONFIG_DMAPath_Support()) {
				(void)VIOC_CONFIG_DMAPath_UnSet((int)VIOC_DTRC1);
				(void)VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, scaler_sub->rdma->id);
			}
			gUse_DtrcConverter &= ~(SUB_USED);
		}
		#endif

		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			(void)VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, scaler_sub->rdma->id);
		}

		if ((bool)scaler_sub->settop_support) {
			VIOC_RDMA_SetImageAlphaSelect(pSC_RDMABase, 1);
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		} else {
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
			/* prevent KCS warning */
		}
		VIOC_RDMA_SetImageFormat(pSC_RDMABase, scaler_sub->info->fmt);

		//interlaced frame process ex) MPEG2
		if ((bool)0/*scaler_sub->info->interlaced*/) {
			//VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler_sub->info->crop_right - scaler_sub->info->crop_left), (scaler_sub->info->crop_bottom - scaler_sub->info->crop_top) / 2U);
			//VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler_sub->info->fmt, scaler_sub->info->Frame_width * 2U);
		}
		#ifdef CONFIG_VIOC_10BIT
		else if (scaler_sub->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10) {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler_sub->info->crop_right - scaler_sub->info->crop_left), (scaler_sub->info->crop_bottom - scaler_sub->info->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler_sub->info->fmt, scaler_sub->info->Frame_width * 2U);
		} else if (scaler_sub->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11) {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler_sub->info->crop_right - scaler_sub->info->crop_left), (scaler_sub->info->crop_bottom - scaler_sub->info->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler_sub->info->fmt, scaler_sub->info->Frame_width * 125 / 100);
		}
		#endif
		else {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler_sub->info->crop_right - scaler_sub->info->crop_left), (scaler_sub->info->crop_bottom - scaler_sub->info->crop_top));
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler_sub->info->fmt, scaler_sub->info->Frame_width);
		}

		pSrcBase0 = (unsigned int)scaler_sub->info->addr0;
		pSrcBase1 = (unsigned int)scaler_sub->info->addr1;
		pSrcBase2 = (unsigned int)scaler_sub->info->addr2;
		if (scaler_sub->info->fmt >= VIOC_IMG_FMT_444SEP) {
			// address limitation!!
			dprintk("%s():  src addr is not allocate\n", __func__);
			scaler_sub->info->crop_left = (scaler_sub->info->crop_left >> 3U) << 3U;
			scaler_sub->info->crop_right = scaler_sub->info->crop_left + crop_width;
			scaler_sub->info->crop_right = scaler_sub->info->crop_left + (scaler_sub->info->crop_right - scaler_sub->info->crop_left);
			tcc_get_addr_yuv(scaler_sub->info->fmt, scaler_sub->info->addr0, scaler_sub->info->Frame_width, scaler_sub->info->Frame_height,
				scaler_sub->info->crop_left, scaler_sub->info->crop_top, &pSrcBase0, &pSrcBase1, &pSrcBase2);
		}
		VIOC_RDMA_SetImageY2REnable(pSC_RDMABase, 0);
		VIOC_RDMA_SetImageBase(pSC_RDMABase, (unsigned int)pSrcBase0, (unsigned int)pSrcBase1, (unsigned int)pSrcBase2);

		#ifdef CONFIG_VIOC_10BIT
		if (scaler_sub->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10) {
			/* YUV 10bit support */
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x1, 0x1);
		} else if (scaler_sub->info->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11) {
			/* YUV real 10bit support */
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x3, 0x0);
		} else {
			/* YUV 8bit support */
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x0, 0x0);
		}
		#endif

		VIOC_SC_SetBypass(pSC_SCALERBase, 0);
		#ifdef CONFIG_USE_SUB_MULTI_FRAME
		if (scaler_sub->info->mosaic_mode) {
			VIOC_SC_SetDstSize(pSC_SCALERBase, scaler_sub->info->buffer_Image_width, scaler_sub->info->buffer_Image_height);
			VIOC_SC_SetOutSize(pSC_SCALERBase, scaler_sub->info->buffer_Image_width, scaler_sub->info->buffer_Image_height);
		} else
		#endif
		{
			VIOC_SC_SetDstSize(pSC_SCALERBase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
			VIOC_SC_SetOutSize(pSC_SCALERBase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
		}
		VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
		if ((int)scaler_sub->sc->id != VIOC_CONFIG_GetScaler_PluginToRDMA(scaler_sub->rdma->id)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			if ((bool)(__raw_readl(pSC_RDMABase + RDMACTRL) & RDMACTRL_IEN_MASK)) {
				// rdma
				VIOC_RDMA_SetImageDisable(pSC_RDMABase);
			}
			(void)VIOC_CONFIG_PlugIn(scaler_sub->sc->id,
				scaler_sub->rdma->id);
		}
		VIOC_SC_SetUpdate(pSC_SCALERBase);

		VIOC_RDMA_SetImageEnable(pSC_RDMABase); // SoC guide info.
	}

#if 0
	 // wmixer op is mixing mode.
	(void)VIOC_CONFIG_WMIXPath(scaler_sub->rdma->id, 1);
	VIOC_WMIX_SetSize(pSC_WMIXBase, scaler_sub->info->Image_width,
		scaler_sub->info->Image_height);
	VIOC_WMIX_SetUpdate(pSC_WMIXBase);
#else
	// wmixer op is bypass mode.
	(void)VIOC_CONFIG_WMIXPath(scaler_sub->rdma->id, 0);
#endif

	VIOC_WDMA_SetImageFormat(pSC_WDMABase, dest_fmt);
#ifdef CONFIG_USE_SUB_MULTI_FRAME
	if ((bool)scaler_sub->info->mosaic_mode) {
		VIOC_WDMA_SetImageSize(pSC_WDMABase, scaler_sub->info->buffer_Image_width, scaler_sub->info->buffer_Image_height);
		VIOC_WDMA_SetImageOffset(pSC_WDMABase, dest_fmt, scaler_sub->info->Image_width);
		if (scaler_sub->info->dst_addr0 == 0U) {
			scaler_sub->info->dst_addr0 = TCC_VIQE_Scaler_Sub_Get_Buffer_M2M(input_image);
			/* prevent KCS warning */
		}
		if (scaler_sub->info->dst_addr0) {
			unsigned int pDstBase0, pDstBase1, pDstBase2;

			pDstBase0 = pDstBase1 = pDstBase2 = 0U;
			tcc_get_addr_yuv(dest_fmt, scaler_sub->info->dst_addr0, scaler_sub->info->Image_width, scaler_sub->info->Image_height,
				((scaler_sub->info->buffer_offset_x - scaler_sub->info->offset_x) >> 1) << 1,
				((scaler_sub->info->buffer_offset_y - scaler_sub->info->offset_y) >> 1) << 1,
				&pDstBase0, &pDstBase1, &pDstBase2);
			VIOC_WDMA_SetImageBase(pSC_WDMABase, pDstBase0, pDstBase1, pDstBase2);
			video_queue_delete_all();
		}
	} else
#endif
	{
		VIOC_WDMA_SetImageSize(pSC_WDMABase, scaler_sub->info->Image_width, scaler_sub->info->Image_height);
		VIOC_WDMA_SetImageOffset(pSC_WDMABase, dest_fmt, scaler_sub->info->Image_width);
		if (scaler_sub->info->dst_addr0 == 0U) {
			scaler_sub->info->dst_addr0 = TCC_VIQE_Scaler_Sub_Get_Buffer_M2M(input_image);
			/* prevent KCS warning */
		}
		VIOC_WDMA_SetImageBase(pSC_WDMABase, (unsigned int)scaler_sub->info->dst_addr0, (unsigned int)scaler_sub->info->dst_addr1, (unsigned int)scaler_sub->info->dst_addr2);
	}

	VIOC_WDMA_SetImageR2YEnable(pSC_WDMABase, 0);
	VIOC_WDMA_SetImageEnable(pSC_WDMABase, 0);
	// wdma status register all clear.
	VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK);

	output_m2m_image_sub.Frame_width = scaler_sub->info->Image_width;
	output_m2m_image_sub.Frame_height = scaler_sub->info->Image_height;
	output_m2m_image_sub.Image_width = scaler_sub->info->Image_width;
	output_m2m_image_sub.Image_height = scaler_sub->info->Image_height;
	output_m2m_image_sub.crop_left = 0;
	output_m2m_image_sub.crop_top = 0;
	output_m2m_image_sub.crop_right = scaler_sub->info->Image_width;
	output_m2m_image_sub.crop_bottom = scaler_sub->info->Image_height;
	output_m2m_image_sub.fmt = dest_fmt;
	output_m2m_image_sub.addr0 = scaler_sub->info->dst_addr0;
	output_m2m_image_sub.addr1 = scaler_sub->info->dst_addr1;
	output_m2m_image_sub.addr2 = scaler_sub->info->dst_addr2;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	dvprintk("%s-%d : Done(0x%x)\n", __func__, __LINE__, DV_PROC_CHECK);
	if ((DV_PROC_CHECK & DV_DUAL_MODE) == DV_DUAL_MODE) {
		output_m2m_image.private_data.dolbyVision_info.el_offset[0] = output_m2m_image_sub.addr0;
		output_m2m_image.private_data.dolbyVision_info.el_buffer_width = output_m2m_image_sub.Frame_width;
		output_m2m_image.private_data.dolbyVision_info.el_buffer_height = output_m2m_image_sub.Frame_height;
		output_m2m_image.private_data.dolbyVision_info.el_frame_width = (output_m2m_image_sub.crop_right - output_m2m_image_sub.crop_left);
		output_m2m_image.private_data.dolbyVision_info.el_frame_height = (output_m2m_image_sub.crop_bottom - output_m2m_image_sub.crop_top);
	}
#endif

#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC)
out:
#endif
	dvprintk("%s() : OUT\n", __func__);
}

void TCC_VIQE_Scaler_Sub_Release60Hz_M2M(void)
{
	dprintk("%s():  In -open(%d), irq(%d)\n", __func__, scaler_sub->data->dev_opened, scaler_sub->data->irq_reged);
	check_wrap(scaler_sub->irq);
	check_wrap(scaler_sub->vioc_intr->id);

	if (scaler_sub->data->dev_opened > 0U) {
		scaler_sub->data->dev_opened--;
		/* prevent KCS warning */
	}

	if (scaler_sub->data->dev_opened == 0U) {
		if ((bool)scaler_sub->data->irq_reged) {
			(void)vioc_intr_disable((int)scaler_sub->irq, (int)scaler_sub->vioc_intr->id, scaler_sub->vioc_intr->bits);
			(void)free_irq(scaler_sub->irq, scaler_sub);
			(void)vioc_intr_clear((int)scaler_sub->vioc_intr->id, scaler_sub->vioc_intr->bits);
			scaler_sub->data->irq_reged = 0;
		}

#ifdef CONFIG_VIOC_MAP_DECOMP
		if ((bool)(gUse_MapConverter & SUB_USED)) {
			if ((bool)VIOC_CONFIG_DMAPath_Support()) {
				(void)VIOC_CONFIG_DMAPath_UnSet((int)VIOC_MC1);
				(void)VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, scaler_sub->rdma->id);
			} else {
				#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
				(void)VIOC_CONFIG_MCPath(scaler_sub->wmix->id, scaler_sub->rdma->id);
				#endif
			}
		}
#endif

#ifdef CONFIG_VIOC_DTRC
		if ((bool)(gUse_DtrcConverter & SUB_USED)) {
			if ((bool)VIOC_CONFIG_DMAPath_Support()) {
				(void)VIOC_CONFIG_DMAPath_UnSet((int)VIOC_DTRC1);
				(void)VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, scaler_sub->rdma->id);
			}
		}
#endif

		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			(void)VIOC_CONFIG_DMAPath_Set(scaler_sub->rdma->id, scaler_sub->rdma->id);
			/* prevent KCS warning */
		}

		(void)VIOC_CONFIG_PlugOut(scaler_sub->sc->id);

		VIOC_CONFIG_SWReset(scaler_sub->wdma->id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler_sub->wmix->id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler_sub->sc->id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler_sub->rdma->id, VIOC_CONFIG_RESET);

		VIOC_CONFIG_SWReset(scaler_sub->rdma->id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler_sub->sc->id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler_sub->wmix->id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler_sub->wdma->id, VIOC_CONFIG_CLEAR);

#ifdef CONFIG_VIOC_MAP_DECOMP
		if ((bool)(gUse_MapConverter & SUB_USED)) {
			tca_map_convter_swreset(VIOC_MC1);
			gUse_MapConverter &= ~(SUB_USED);
		}
#endif
#ifdef CONFIG_VIOC_DTRC
		if ((bool)(gUse_DtrcConverter & SUB_USED)) {
			tca_dtrc_convter_swreset(VIOC_DTRC1);
			gUse_DtrcConverter &= ~(SUB_USED);
		}
#endif

	}

	dprintk("%s() : Out - open(%d)\n", __func__, scaler_sub->data->dev_opened);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
irqreturn_t TCC_VIQE_Scaler_Handler60Hz_M2M(
	int irq, void *client_data)
{
	void __iomem *pSC_WDMABase = scaler->wdma->reg;
	irqreturn_t ret = IRQ_HANDLED;

	(void)irq;
	(void)client_data;
	check_wrap(scaler->vioc_intr->id);

	if (is_vioc_intr_activatied((int)scaler->vioc_intr->id, scaler->vioc_intr->bits) == false) {
		//dprintk("%s-%d\n", __func__, __LINE__);
		ret = IRQ_NONE;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	if ((bool)((__raw_readl(pSC_WDMABase + WDMAIRQSTS_OFFSET)) & VIOC_WDMA_IREQ_EOFR_MASK)) {
		dprintk("Main WDMA Interrupt is VIOC_WDMA_IREQ_EOFR_MASK\n");
		//(void)vioc_intr_clear((int)scaler->vioc_intr->id, scaler->vioc_intr->bits);
		// wdma status register all clear.
		VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK);

#if !defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
		if ((bool)tcc_vsync_isVsyncRunning(VSYNC_MAIN))
#endif
		{
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
			dvprintk("%s-%d : Done(0x%x)\n", __func__, __LINE__, DV_PROC_CHECK);
			if ((DV_PROC_CHECK & DV_DUAL_MODE) == DV_DUAL_MODE) {
				DV_PROC_CHECK |= DV_MAIN_DONE;
				dvprintk("%s-%d : Done(0x%x)\n", __func__, __LINE__, DV_PROC_CHECK);
				if ((DV_PROC_CHECK & DV_ALL_DONE) == DV_ALL_DONE) {
					TCC_VIQE_Display_Update60Hz_M2M(&output_m2m_image);
					/* prevent KCS warning */
				}
			} else
#endif
			{
				TCC_VIQE_Display_Update60Hz_M2M(&output_m2m_image);
				/* prevent KCS warning */
			}
		}
	}

#ifdef PRINT_OUT_M2M_MS
	tcc_GetTimediff_ms(t1);
#endif

out:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
irqreturn_t TCC_VIQE_Scaler_Sub_Handler60Hz_M2M(
	int irq, void *client_data)
{
	void __iomem *pSC_WDMABase = scaler_sub->wdma->reg;
	irqreturn_t ret = IRQ_HANDLED;

	(void)irq;
	(void)client_data;
	check_wrap(scaler_sub->vioc_intr->id);

	if (is_vioc_intr_activatied((int)scaler_sub->vioc_intr->id, scaler_sub->vioc_intr->bits) == false) {
		ret = IRQ_NONE;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	if ((bool)((__raw_readl(pSC_WDMABase + WDMAIRQSTS_OFFSET)) & VIOC_WDMA_IREQ_EOFR_MASK)) {
		dprintk("Sub WDMA intr is EOFR. VSync_Run(%d)\n", tcc_vsync_isVsyncRunning(VSYNC_SUB0));
		//(void)vioc_intr_clear((int)scaler_sub->vioc_intr->id, scaler_sub->vioc_intr->bits);
		// wdma status register all clear.
		VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK);

		if ((bool)tcc_vsync_isVsyncRunning(VSYNC_SUB0)) {
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
			dvprintk("%s-%d : Done(0x%x)\n", __func__, __LINE__, DV_PROC_CHECK);
			if ((DV_PROC_CHECK & DV_DUAL_MODE) == DV_DUAL_MODE) {
				DV_PROC_CHECK |= DV_SUB_DONE;
				dvprintk("%s-%d : Done(0x%x)\n", __func__, __LINE__, DV_PROC_CHECK);
				if ((DV_PROC_CHECK & DV_ALL_DONE) == DV_ALL_DONE) {
					TCC_VIQE_Display_Update60Hz_M2M(&output_m2m_image);
					/* prevent KCS warning */
				}
				ret = IRQ_HANDLED;
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto out;
			}
#endif

#ifdef CONFIG_USE_SUB_MULTI_FRAME
			if ((bool)(scaler_sub->info->mosaic_mode && !list_empty(&video_queue_list))) {
				struct video_queue_t *q;

				q = video_queue_pop();
				if ((bool)q) {
					TCC_VIQE_Scaler_Sub_Repeat60Hz_M2M(&sub_m2m_image[q->type]);
					video_queue_delete(q);
				}
				ret = IRQ_HANDLED;
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto out;
			}
#endif
			TCC_VIQE_Display_Update60Hz_M2M(&output_m2m_image_sub);
		}
	}

out:
	return ret;
}

void TCC_VIQE_Display_Update60Hz_M2M(
	struct tcc_lcdc_image_update *input_image)
{
	struct tcc_dp_device *dp_device = tca_fb_get_displayType(gOutputMode);

	dprintk("update IN : %d\n", input_image->first_frame_after_seek);

	if (!(bool)input_image->first_frame_after_seek) {
		switch (gOutputMode) {
		case TCC_OUTPUT_NONE:
			break;

		#ifdef TCC_LCD_VIDEO_DISPLAY_BY_VSYNC_INT
		case TCC_OUTPUT_LCD:
			tca_scale_display_update(dp_device, input_image);
			break;
		#endif

		case TCC_OUTPUT_HDMI:
		case TCC_OUTPUT_COMPONENT:
		case TCC_OUTPUT_COMPOSITE:
			dprintk("%s : Layer(%d) update %d, %d, %d\n", __func__,
				input_image->Lcdc_layer,
				input_image->buffer_unique_id,
				input_image->addr0,
				input_image->odd_first_flag);
			tca_scale_display_update(dp_device, input_image);
			break;

		default:
			(void)pr_err("[ERR][VIQE] %s: invalid gOutputMode(%d)\n", __func__, gOutputMode);
			break;
		}
	}
//	(void)pr_info("[INF][VIQE] update OUT\n");
}
///////////////////////////////////////////////////////////////////////////////

#if 0
	//img_fmt
	TCC_LCDC_IMG_FMT_YUV420SP = 24,
	TCC_LCDC_IMG_FMT_YUV422SP = 25,
	TCC_LCDC_IMG_FMT_YUYV = 26,
	TCC_LCDC_IMG_FMT_YVYU = 27,

	TCC_LCDC_IMG_FMT_YUV420ITL0 = 28,
	TCC_LCDC_IMG_FMT_YUV420ITL1 = 29,
	TCC_LCDC_IMG_FMT_YUV422ITL0 = 30,
	TCC_LCDC_IMG_FMT_YUV422ITL1 = 31,
#endif

///////////////////////////////////////////////////////////////////////////////
void TCC_VIQE_DI_Init60Hz(
	enum TCC_OUTPUT_TYPE outputMode,
	int lcdCtrlNum,
	const struct tcc_lcdc_image_update *input_image)
{
	unsigned int deintl_dma_base0, deintl_dma_base1;
	unsigned int deintl_dma_base2, deintl_dma_base3;
	unsigned int imgSize;
	/* If this value is OFF, The size information is get from VIOC modules. */
	unsigned int top_size_dont_use = 0U;
	unsigned int framebufWidth, framebufHeight;
	unsigned int lcd_width = 0, lcd_height = 0;

	const struct tcc_viqe_60hz_virt_addr_info_t *pViqe_60hz_info = NULL;
	void __iomem *pVIQE_Info = NULL;
	unsigned int nVIOC_VIQE = 0;

	#ifdef DBG_VIQE_INTERFACE
	unsigned int cropWidth, cropHeight;
	unsigned int crop_top = input_image->crop_top;
	unsigned int crop_bottom = input_image->crop_bottom;
	unsigned int crop_left = input_image->crop_left;
	unsigned int crop_right = input_image->crop_right;
	#endif

	unsigned int y2r_on = 0U;

	(void)lcdCtrlNum;

	if (TCC_VIQE_Scaler_Init_Buffer_M2M() < 0) {
		(void)pr_err("[ERR][VIQE] %s-%d : memory allocation is failed\n", __func__, __LINE__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_INIT, input_image, input_image->Lcdc_layer);
#endif

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	bStep_Check = DEF_DV_CHECK_NUM;
#endif

	gLcdc_layer_60Hz = input_image->Lcdc_layer;

	if ((input_image->fmt == 24U) || (input_image->fmt == 28U) || (input_image->fmt == 29U)) {
		gViqe_fmt_60Hz = (unsigned int)VIOC_VIQE_FMT_YUV420;
		/* prevent KCS warning */
	} else {
		gViqe_fmt_60Hz = (unsigned int)VIOC_VIQE_FMT_YUV422;
		/* prevent KCS warning */
	}
	gImg_fmt_60Hz = input_image->fmt;

	gOutputMode = outputMode;
	if (gOutputMode == TCC_OUTPUT_LCD) {
		pVIQE_Info = viqe_common_info.pVIQE0;
		nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		pViqe_60hz_info = &viqe_60hz_lcd_info;
	} else {
		if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
			pViqe_60hz_info = &viqe_60hz_external_info;
		} else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
		#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			pVIQE_Info = viqe_common_info.pVIQE1;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE1;
		#else
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		#endif
			pViqe_60hz_info = &viqe_60hz_external_sub_info;
		} else {
			(void)pr_err("ERROR: %s check layer\n", __func__);
			goto out;
		}
	}

#ifdef MAX_VIQE_FRAMEBUFFER
	framebufWidth = 1920U;
	framebufHeight = 1088U;
#else
	framebufWidth = ((input_image->Frame_width) >> 3U) << 3U; // 8bit align
	framebufHeight = ((input_image->Frame_height) >> 2U) << 2U; // 4bit align
#endif

	#ifdef DBG_VIQE_INTERFACE
	crop_top = (crop_top >> 1) << 1;
	crop_left = (crop_left >> 1) << 1;
	check_wrap(crop_right);
	check_wrap(crop_left);
	check_wrap(crop_top);
	check_wrap(crop_bottom);
	cropWidth = ((crop_right - crop_left) >> 3) << 3; // 8bit align
	cropHeight = ((crop_bottom - crop_top) >> 2) << 2; // 4bit align

	dprintk("%s: FB:%dx%d SRC:%dx%d DST:%dx%d(%d,%d) %d, %s, %d\n",
		__func__, framebufWidth, framebufHeight,
		cropWidth, cropHeight,
		input_image->Image_width, input_image->Image_height,
		input_image->offset_x, input_image->offset_y,
		input_image->fmt, (gViqe_fmt_60Hz ? "YUV422" : "YUV420"),
		input_image->odd_first_flag);
	#endif

	VIOC_DISP_GetSize(pViqe_60hz_info->pDISPBase_60Hz, &lcd_width, &lcd_height);
	if ((!(bool)lcd_width) || (!(bool)lcd_height)) {
		(void)pr_err("[ERR][VIQE] %s invalid lcd size\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	#if defined(USE_DEINTERLACE_S_IN60Hz)
		gDeintlS_Use_60Hz = 1;
	#else
		#if defined(DYNAMIC_USE_DEINTL_COMPONENT)
		if ((framebufWidth >= 1280) && (framebufHeight >= 720)) {
			gDeintlS_Use_60Hz = 1;
			gDI_mode_60Hz = VIOC_VIQE_DEINTL_S;
		} else {
			gDeintlS_Use_60Hz = 0;
			gDI_mode_60Hz = VIOC_VIQE_DEINTL_MODE_2D;
		}
		#endif
	#endif

	if (viqe_common_info.gBoard_stb == 1U) {
		VIOC_RDMA_SetImageDisable(pViqe_60hz_info->pRDMABase_60Hz);
		//TCC_OUTPUT_FB_ClearVideoImg();
	}

	//RDMA SETTING
	if ((bool)gDeintlS_Use_60Hz) {
		VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, 1U);
		/* Y2RMode Default 0 (Studio Color) */
		VIOC_RDMA_SetImageY2RMode(pViqe_60hz_info->pRDMABase_60Hz, 0x02);
	} else {
		VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, 0U);
		/* Y2RMode Default 0 (Studio Color) */
		VIOC_RDMA_SetImageY2RMode(pViqe_60hz_info->pRDMABase_60Hz, 0x02);
	}

	VIOC_RDMA_SetImageOffset(pViqe_60hz_info->pRDMABase_60Hz, input_image->fmt, framebufWidth);
	VIOC_RDMA_SetImageFormat(pViqe_60hz_info->pRDMABase_60Hz, input_image->fmt);
	VIOC_RDMA_SetImageSize(pViqe_60hz_info->pRDMABase_60Hz, framebufWidth, framebufHeight);
	VIOC_RDMA_SetImageIntl(pViqe_60hz_info->pRDMABase_60Hz, 1);
	VIOC_RDMA_SetImageBfield(pViqe_60hz_info->pRDMABase_60Hz, input_image->odd_first_flag);

	if ((bool)gDeintlS_Use_60Hz) {
		dprintk("%s, DeinterlacerS is used!!\n", __func__);
		tcc_sdintls_swreset();
		(void)VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, pViqe_60hz_info->gVIQE_RDMA_num_60Hz);
		gDeintlS_Use_Plugin = 1U;
	} else {
		dprintk("%s, VIQE%d is used!!\n", __func__, get_vioc_index(nVIOC_VIQE));

		// If you use 3D(temporal) De-interlace mode,
		// you have to set physical address for using DMA register.
		// If 2D(spatial) mode, these registers are ignored
		check_wrap(framebufWidth);
		check_wrap(framebufHeight);
		imgSize = (framebufWidth * (framebufHeight / 2U) * 4U * 3U / 2U);
		check_wrap(imgSize);
		if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
			deintl_dma_base0 = gPMEM_VIQE_BASE;
			/* prevent KCS warning */
		}
	#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
		else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
			deintl_dma_base0 = gPMEM_VIQE_SUB_BASE;
			/* prevent KCS warning */
		}
	#endif
		else {
			dprintk("%s check layer\n", __func__);
			deintl_dma_base0 = 0;
		}

		check_wrap(deintl_dma_base0);
		deintl_dma_base1 = deintl_dma_base0 + imgSize;
		check_wrap(deintl_dma_base1);
		deintl_dma_base2 = deintl_dma_base1 + imgSize;
		check_wrap(deintl_dma_base2);
		deintl_dma_base3 = deintl_dma_base2 + imgSize;

		VIOC_VIQE_IgnoreDecError(pVIQE_Info, 1U, 1U, 1U);
		VIOC_VIQE_SetControlRegister(pVIQE_Info, framebufWidth, framebufHeight, gViqe_fmt_60Hz);
		VIOC_VIQE_SetDeintlRegister(pVIQE_Info, gViqe_fmt_60Hz, top_size_dont_use, framebufWidth, framebufHeight,
			gDI_mode_60Hz, deintl_dma_base0, deintl_dma_base1, deintl_dma_base2, deintl_dma_base3);
		// for bypass path on progressive frame
		//VIOC_VIQE_SetDenoise(pVIQE_Info, gViqe_fmt_60Hz, framebufWidth, framebufHeight, 1, 0, deintl_dma_base0, deintl_dma_base1);
		VIOC_VIQE_SetControlEnable(pVIQE_Info, 0U, 0U, 0U, 0U, 1U);

		y2r_on = viqe_y2r_mode_check();
		VIOC_VIQE_SetImageY2RMode(pVIQE_Info, 0x02);

	#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
		if ((VIOC_CONFIG_DV_GET_EDR_PATH() || (vioc_v_dv_get_stage() != DV_OFF))
			&& (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO)) {
			y2r_on = 0U;
		}
	#endif

		VIOC_VIQE_SetImageY2REnable(pVIQE_Info, y2r_on);
		(void)VIOC_CONFIG_PlugIn(nVIOC_VIQE, pViqe_60hz_info->gVIQE_RDMA_num_60Hz);
	}

	//SCALER SETTING
	if ((bool)input_image->on_the_fly) {
		VIOC_RDMA_SetImageDisable(pViqe_60hz_info->pRDMABase_60Hz);
		if (VIOC_CONFIG_GetScaler_PluginToRDMA(
			pViqe_60hz_info->gVIQE_RDMA_num_60Hz) != -1) {
			(void)VIOC_CONFIG_PlugOut(pViqe_60hz_info->gSCALER_num_60Hz);
		}
		tcc_scaler_swreset(pViqe_60hz_info->gSCALER_num_60Hz);
		(void)VIOC_CONFIG_PlugIn(pViqe_60hz_info->gSCALER_num_60Hz,
			pViqe_60hz_info->gVIQE_RDMA_num_60Hz);
		VIOC_SC_SetBypass(pViqe_60hz_info->pSCALERBase_60Hz, 0U);
		// set destination size in scaler
		VIOC_SC_SetDstSize(pViqe_60hz_info->pSCALERBase_60Hz,
			input_image->Image_width, input_image->Image_height);
		// set output size in scaer
		VIOC_SC_SetOutSize(pViqe_60hz_info->pSCALERBase_60Hz,
			input_image->Image_width, input_image->Image_height);
		VIOC_SC_SetUpdate(pViqe_60hz_info->pSCALERBase_60Hz);
	}

	VIOC_WMIX_SetPosition(pViqe_60hz_info->pWMIXBase_60Hz, gLcdc_layer_60Hz, input_image->offset_x, input_image->offset_y);
	VIOC_WMIX_SetUpdate(pViqe_60hz_info->pWMIXBase_60Hz);

	if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
		atomic_set(&gFrmCnt_60Hz, 0);
		gVIQE_Init_State = 1;
	}
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
	else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
		atomic_set(&gFrmCnt_Sub_60Hz, 0);
		gVIQE1_Init_State = 1;
	}
#endif
	else {
		dprintk("%s check layer\n", __func__);
		/* prevent KCS warning */
	}

out:
	dprintk("%s---\n", __func__);
}

void TCC_VIQE_DI_Swap60Hz(int mode)
{
	VIOC_PlugInOutCheck VIOC_PlugIn;
	void __iomem *pVIQE_Info = NULL;
	unsigned int nVIOC_VIQE = 0;

	if (gOutputMode == TCC_OUTPUT_LCD) {
		pVIQE_Info = viqe_common_info.pVIQE0;
		nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
	} else {
		if (gLcdc_layer_60Hz == (unsigned int)RDMA_VIDEO) {
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		} else if (gLcdc_layer_60Hz == (unsigned int)RDMA_VIDEO_SUB) {
			#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			pVIQE_Info = viqe_common_info.pVIQE1;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE1;
			#else
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
			#endif
		} else {
			dprintk("%s check layer\n", __func__);
			/* prevent KCS warning */
		}
	}

	(void)VIOC_CONFIG_Device_PlugState(nVIOC_VIQE, &VIOC_PlugIn);
	dprintk("%s: %d, %d, %d, %d\n", __func__, gOutputMode, gLcdc_layer_60Hz, VIOC_PlugIn.enable, mode);

	if ((bool)VIOC_PlugIn.enable) {
		VIOC_VIQE_SwapDeintlBase(pVIQE_Info, mode);
		/* prevent KCS warning */
	}
}

void TCC_VIQE_DI_SetFMT60Hz(unsigned int enable)
{
	VIOC_PlugInOutCheck VIOC_PlugIn;
	void __iomem *pVIQE_Info = NULL;
	unsigned int nVIOC_VIQE = 0;

	if (gOutputMode == TCC_OUTPUT_LCD) {
		pVIQE_Info = viqe_common_info.pVIQE0;
		nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
	} else {
		if (gLcdc_layer_60Hz == (unsigned int)RDMA_VIDEO) {
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		} else if (gLcdc_layer_60Hz == (unsigned int)RDMA_VIDEO_SUB) {
			#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			pVIQE_Info = viqe_common_info.pVIQE1;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE1;
			#else
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
			#endif
		} else {
			dprintk("%s check layer\n", __func__);
			/* prevent KCS warning */
		}
	}

	(void)VIOC_CONFIG_Device_PlugState(nVIOC_VIQE, &VIOC_PlugIn);
	if ((bool)VIOC_PlugIn.enable) {
		VIOC_VIQE_SetDeintlFMT(pVIQE_Info, enable);
		/* prevent KCS warning */
	}
}

void TCC_VIQE_DI_Run60Hz(struct tcc_lcdc_image_update *input_image, int reset_frmCnt)
{
	unsigned int lcd_width = 0, lcd_height = 0;
	unsigned int cropWidth, cropHeight;
	unsigned int crop_top = input_image->crop_top;
	unsigned int crop_bottom = input_image->crop_bottom;
	unsigned int crop_left = input_image->crop_left;
	unsigned int crop_right = input_image->crop_right;
	unsigned int addr_Y = (unsigned int)input_image->addr0;
	unsigned int addr_U = (unsigned int)input_image->addr1;
	unsigned int addr_V = (unsigned int)input_image->addr2;

	const struct tcc_viqe_60hz_virt_addr_info_t *pViqe_60hz_info = NULL;
	void __iomem *pVIQE_Info = NULL;
	unsigned int nVIOC_VIQE = 0;

#ifndef USE_DEINTERLACE_S_IN60Hz
	VIOC_PlugInOutCheck VIOC_PlugIn;
	unsigned int JudderCnt = 0U;
#endif

#ifdef CONFIG_TCC_VIOCMG
	unsigned int viqe_lock = 0U;
#endif

	unsigned int y2r_on = 0U;

	int frmCnt_60hz = 0;

	if (gOutputMode == TCC_OUTPUT_LCD) {
		pVIQE_Info = viqe_common_info.pVIQE0;
		nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		pViqe_60hz_info = &viqe_60hz_lcd_info;
	} else {
		if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
			pViqe_60hz_info = &viqe_60hz_external_info;
		} else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
		#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			pVIQE_Info = viqe_common_info.pVIQE1;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE1;
		#else
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		#endif
			pViqe_60hz_info = &viqe_60hz_external_sub_info;
		} else {
			dprintk("%s check layer\n", __func__);
			/* prevent KCS warning */
		}
	}

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_ON, input_image, input_image->Lcdc_layer);
#endif

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	if (input_image->private_data.optional_info[VID_OPT_HAVE_DOLBYVISION_INFO] != 0U) {
		if (!(bool)bStep_Check) {
			nFrame++;
			/* prevent KCS warning */
		} else {
			if (bStep_Check == DEF_DV_CHECK_NUM) {
				nFrame = 1U; //0;
				bStep_Check--;
			} else {
				bStep_Check--;
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto out;
			}
		}
	}
#endif

	if (viqe_common_info.gBoard_stb == 1U) {
		if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
			if (gVIQE_Init_State == 0) {
				dprintk("%s VIQE block isn't initailized\n", __func__);
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto out;
			}
		}
		#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
		else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
			if (gVIQE1_Init_State == 0) {
				dprintk("%s VIQE block isn't initailized\n", __func__);
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto out;
			}
		}
		#endif
		else {
			dprintk("%s check layer\n", __func__);
			/* prevent KCS warning */
		}
	}

#ifndef USE_DEINTERLACE_S_IN60Hz
	(void)VIOC_CONFIG_Device_PlugState(nVIOC_VIQE, &VIOC_PlugIn);
	if (VIOC_PlugIn.enable == 0U) {
		dprintk("%s VIQE block isn't pluged!!!\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
#endif

	VIOC_DISP_GetSize(pViqe_60hz_info->pDISPBase_60Hz, &lcd_width, &lcd_height);
	if ((!(bool)lcd_width) || (!(bool)lcd_height)) {
		(void)pr_err("[ERR][VIQE] %s invalid lcd size\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	#ifdef CONFIG_TCC_VIOCMG
	if (!(bool)gDeintlS_Use_60Hz) {
		VIOC_PlugInOutCheck plug_status;

		if (viocmg_lock_viqe(VIOCMG_CALLERID_VOUT) < 0) {
			unsigned int enable = 0;

			VIOC_RDMA_GetImageEnable(pViqe_60hz_info->pRDMABase_60Hz, &enable);
			if ((bool)enable) {
				VIOC_RDMA_SetImageDisable(pViqe_60hz_info->pRDMABase_60Hz);
				/* prevent KCS warning */
			}

			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto out;
		}

		viqe_lock = 1;

		(void)VIOC_CONFIG_Device_PlugState(nVIOC_VIQE, &plug_status);
		if ((!(bool)plug_status.enable) || (plug_status.connect_statue != VIOC_PATH_CONNECTED)) {
			reset_frmCnt = VIQE_RESET_RECOVERY;
			/* prevent KCS warning */
		}
	}
	#endif

	if ((reset_frmCnt != (int)VIQE_RESET_NONE) && (reset_frmCnt != (int)VIQE_RESET_CROP_CHANGED)) {
		if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
			atomic_set(&gFrmCnt_60Hz, 0);
			/* prevent KCS warning */
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
		} else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
			atomic_set(&gFrmCnt_Sub_60Hz, 0);
			/* prevent KCS warning */
#endif
		} else {
			dprintk("%s check layer\n", __func__);
			/* prevent KCS warning */
		}
		dprintk("VIQE%d 3D -> 2D :: %d -> %dx%d\n", get_vioc_index(nVIOC_VIQE), reset_frmCnt, (crop_right - crop_left), (crop_bottom - crop_top));
	}

	if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
		frmCnt_60hz = atomic_read(&gFrmCnt_60Hz);
		/* prevent KCS warning */
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
	} else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
		frmCnt_60hz = atomic_read(&gFrmCnt_Sub_60Hz);
		/* prevent KCS warning */
#endif
	} else {
		dprintk("%s check layer\n", __func__);
		/* prevent KCS warning */
	}

	crop_top = (crop_top >> 1) << 1;
	crop_left = (crop_left >> 1) << 1;
	check_wrap(crop_right);
	check_wrap(crop_left);
	check_wrap(crop_top);
	check_wrap(crop_bottom);
	cropWidth = ((crop_right - crop_left) >> 3) << 3;	//8bit align
	cropHeight = ((crop_bottom - crop_top) >> 2) << 2;	//4bit align

	// Prevent Display Block from FIFO under-run,
	// when current_crop size is smaller than prev_crop size
	// (screen mode: PANSCAN<->BOX)
	// So, Soc team suggested VIQE block resetting
	if ((frmCnt_60hz == 0) && (gDeintlS_Use_60Hz == 0U)) {
		unsigned int deintl_dma_base0, deintl_dma_base1;
		unsigned int deintl_dma_base2, deintl_dma_base3;
		/* If this value is OFF, The size is get from VIOC modules. */
		unsigned int top_size_dont_use = 0U;
		unsigned int imgSize;

		dprintk("VIQE%d reset 3D -> 2D :: %dx%d\n", get_vioc_index(nVIOC_VIQE), (crop_right - crop_left), (crop_bottom - crop_top));
		VIOC_RDMA_SetImageDisable(pViqe_60hz_info->pRDMABase_60Hz);
		(void)VIOC_CONFIG_PlugOut(nVIOC_VIQE);

		tcc_viqe_swreset(nVIOC_VIQE);

		// If you use 3D(temporal) De-interlace mode,
		// you have to set physical address for using DMA register.
		// If 2D(spatial) mode, these registers are ignored
		imgSize = (input_image->Frame_width * (input_image->Frame_height / 2U) * 4U * 3U / 2U);

		if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
			deintl_dma_base0 = gPMEM_VIQE_BASE;
			/* prevent KCS warning */
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
		} else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
			deintl_dma_base0 = gPMEM_VIQE_SUB_BASE;
			/* prevent KCS warning */
#endif
		} else {
			dprintk("%s check layer\n", __func__);
			deintl_dma_base0 = 0;
		}

		deintl_dma_base1 = deintl_dma_base0 + imgSize;
		deintl_dma_base2 = deintl_dma_base1 + imgSize;
		deintl_dma_base3 = deintl_dma_base2 + imgSize;

		VIOC_VIQE_IgnoreDecError(pVIQE_Info, 1U, 1U, 1U);
		VIOC_VIQE_SetControlRegister(pVIQE_Info, input_image->Frame_width, input_image->Frame_height, gViqe_fmt_60Hz);
		VIOC_VIQE_SetDeintlRegister(pVIQE_Info, gViqe_fmt_60Hz, top_size_dont_use, input_image->Frame_width,
			input_image->Frame_height, gDI_mode_60Hz,
			deintl_dma_base0, deintl_dma_base1,
			deintl_dma_base2, deintl_dma_base3);
		// for bypass path on progressive frame
		//VIOC_VIQE_SetDenoise(pVIQE_Info, gViqe_fmt_60Hz, srcWidth, srcHeight, 1, 0, deintl_dma_base0, deintl_dma_base1);

		y2r_on = viqe_y2r_mode_check();
		VIOC_VIQE_SetImageY2RMode(pVIQE_Info, 0x02);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if ((VIOC_CONFIG_DV_GET_EDR_PATH() || (vioc_v_dv_get_stage() != DV_OFF)) && (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO)) {
		y2r_on = 0U;
		/* prevent KCS warning */
	}
#endif

		VIOC_VIQE_SetImageY2REnable(pVIQE_Info, y2r_on);
		(void)VIOC_CONFIG_PlugIn(nVIOC_VIQE, pViqe_60hz_info->gVIQE_RDMA_num_60Hz);
	}

	tcc_get_addr_yuv(gImg_fmt_60Hz, input_image->addr0, input_image->Frame_width, input_image->Frame_height,
		crop_left, crop_top, &addr_Y, &addr_U, &addr_V);
	VIOC_RDMA_SetImageSize(pViqe_60hz_info->pRDMABase_60Hz, cropWidth, cropHeight);
	VIOC_RDMA_SetImageBase(pViqe_60hz_info->pRDMABase_60Hz, addr_Y, addr_U, addr_V);

#ifdef CONFIG_VIOC_10BIT
	if (input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10U) {
		/* YUV 10bit support */
		VIOC_RDMA_SetDataFormat(pViqe_60hz_info->pRDMABase_60Hz, 0x1, 0x1);
	} else if (input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11) {
		/* YUV real 10bit support */
		VIOC_RDMA_SetDataFormat(pViqe_60hz_info->pRDMABase_60Hz, 0x3, 0x0);
	} else {
		/* YUV 8bit support */
		VIOC_RDMA_SetDataFormat(pViqe_60hz_info->pRDMABase_60Hz, 0x0, 0x0);
	}

	if (input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10U) {
		VIOC_RDMA_SetImageOffset(pViqe_60hz_info->pRDMABase_60Hz, gImg_fmt_60Hz, input_image->Frame_width * 2);
		/* prevent KCS warning */
	} else if (input_image->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11U) {
		VIOC_RDMA_SetImageOffset(pViqe_60hz_info->pRDMABase_60Hz, gImg_fmt_60Hz, input_image->Frame_width * 125 / 100);
		/* prevent KCS warning */
	} else
#endif
	{
		VIOC_RDMA_SetImageOffset(pViqe_60hz_info->pRDMABase_60Hz, gImg_fmt_60Hz, input_image->Frame_width);
		/* prevent KCS warning */
	}

	VIOC_RDMA_SetImageBfield(pViqe_60hz_info->pRDMABase_60Hz, input_image->odd_first_flag);

	if ((bool)input_image->odd_first_flag) {
		dprintk("ID[%03d] Layer[%d] Otf:%d, Addr:0x%x/0x%x\n",
			input_image->private_data.optional_info[VID_OPT_BUFFER_ID],
			input_image->Lcdc_layer, input_image->on_the_fly, addr_Y, addr_U);
		dprintk("Crop[%d, %d, %d, %d, %dx%d] odd:%d frm-I:%d bpp:%d\n",
			crop_left, crop_right, crop_top, crop_bottom, cropWidth, cropHeight,
			input_image->odd_first_flag, input_image->frameInfo_interlace,
			input_image->private_data.optional_info[VID_OPT_BIT_DEPTH]);
	}

	if ((bool)gDeintlS_Use_60Hz) {
		//VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, 0U);
		//VIOC_RDMA_SetImageY2RMode(pViqe_60hz_info->pRDMABase_60Hz, 0x02);

		if ((bool)input_image->frameInfo_interlace) {
			VIOC_RDMA_SetImageIntl(pViqe_60hz_info->pRDMABase_60Hz, 1);
			if (!(bool)gDeintlS_Use_Plugin) {
				(void)VIOC_CONFIG_PlugIn(viqe_common_info.gVIOC_Deintls, pViqe_60hz_info->gVIQE_RDMA_num_60Hz);
				gDeintlS_Use_Plugin = 1U;
			}
		} else {
			VIOC_RDMA_SetImageIntl(pViqe_60hz_info->pRDMABase_60Hz, 0);
			if ((bool)gDeintlS_Use_Plugin) {
				(void)VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
				gDeintlS_Use_Plugin = 0U;
			}
		}
	} else {
		if ((bool)input_image->frameInfo_interlace) {
			int frmCnt60hz = 0;

			if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
				frmCnt60hz = atomic_read(&gFrmCnt_60Hz);
				/* prevent KCS warning */
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			} else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
				frmCnt60hz = atomic_read(&gFrmCnt_Sub_60Hz);
				/* prevent KCS warning */
#endif
			} else {
				dprintk("%s check layer\n", __func__);
				/* prevent KCS warning */
			}

			VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, 0U);
			/* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageY2RMode(pViqe_60hz_info->pRDMABase_60Hz, 0x02);
			if (frmCnt60hz >= 3) {
				VIOC_VIQE_SetDeintlMode(pVIQE_Info, VIOC_VIQE_DEINTL_MODE_3D);
#ifndef USE_DEINTERLACE_S_IN60Hz
				// Process JUDDER CNTS
				JudderCnt = ((input_image->Frame_width + 64U) / 64U) - 1U;
				VIOC_VIQE_SetDeintlJudderCnt(pVIQE_Info, JudderCnt);
#endif
				gDI_mode_60Hz = VIOC_VIQE_DEINTL_MODE_3D;
				dprintk("VIQE%d 3D %d x %d\n", get_vioc_index(nVIOC_VIQE), cropWidth, cropHeight);
			} else {
				VIOC_VIQE_SetDeintlMode(pVIQE_Info, VIOC_VIQE_DEINTL_MODE_2D);
				gDI_mode_60Hz = VIOC_VIQE_DEINTL_MODE_2D;
				dprintk("VIQE%d 2D %d x %d\n", get_vioc_index(nVIOC_VIQE), cropWidth, cropHeight);
			}

			VIOC_VIQE_SetControlMode(pVIQE_Info, 0U, 0U, 0U, 0U, 1U);
			VIOC_RDMA_SetImageIntl(pViqe_60hz_info->pRDMABase_60Hz, 1);
		} else {
			VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, 1U);
			/* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageY2RMode(pViqe_60hz_info->pRDMABase_60Hz, 0x02);
			VIOC_VIQE_SetControlMode(pVIQE_Info, 0U, 0U, 0U, 0U, 0U);
			VIOC_RDMA_SetImageIntl(pViqe_60hz_info->pRDMABase_60Hz, 0);
			if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
				atomic_set(&gFrmCnt_60Hz, 0);
				/* prevent KCS warning */
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			} else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
				atomic_set(&gFrmCnt_Sub_60Hz, 0);
				/* prevent KCS warning */
#endif
			} else {
				dprintk("%s check layer\n", __func__);
				/* prevent KCS warning */
			}
		}
	}

	if ((bool)input_image->on_the_fly) {
		check_wrap(pViqe_60hz_info->gSCALER_num_60Hz);
		if (VIOC_CONFIG_GetScaler_PluginToRDMA(pViqe_60hz_info->gVIQE_RDMA_num_60Hz) != (int)pViqe_60hz_info->gSCALER_num_60Hz) {
			(void)VIOC_CONFIG_PlugIn(pViqe_60hz_info->gSCALER_num_60Hz, pViqe_60hz_info->gVIQE_RDMA_num_60Hz);
			VIOC_SC_SetBypass(pViqe_60hz_info->pSCALERBase_60Hz, 0U);
		}

		VIOC_SC_SetDstSize(pViqe_60hz_info->pSCALERBase_60Hz, input_image->Image_width, input_image->Image_height);
		VIOC_SC_SetOutSize(pViqe_60hz_info->pSCALERBase_60Hz, input_image->Image_width, input_image->Image_height);
		VIOC_SC_SetUpdate(pViqe_60hz_info->pSCALERBase_60Hz);
	} else {
		check_wrap(pViqe_60hz_info->gSCALER_num_60Hz);
		if (VIOC_CONFIG_GetScaler_PluginToRDMA(pViqe_60hz_info->gVIQE_RDMA_num_60Hz) == (int)pViqe_60hz_info->gSCALER_num_60Hz) {
			VIOC_RDMA_SetImageDisable(pViqe_60hz_info->pRDMABase_60Hz);
			(void)VIOC_CONFIG_PlugOut(pViqe_60hz_info->gSCALER_num_60Hz);
		}
	}
	// position
	VIOC_WMIX_SetPosition(pViqe_60hz_info->pWMIXBase_60Hz, gLcdc_layer_60Hz, input_image->offset_x, input_image->offset_y);
	VIOC_WMIX_SetUpdate(pViqe_60hz_info->pWMIXBase_60Hz);
	VIOC_RDMA_SetImageEnable(pViqe_60hz_info->pRDMABase_60Hz);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
		VIOC_RDMA_SetImageUVIEnable(pViqe_60hz_info->pRDMABase_60Hz, 0);
		VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, 0);

		if ((input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) || (input_image->Lcdc_layer == RDMA_LASTFRM)) {

			if ((input_image->Lcdc_layer == RDMA_LASTFRM)
				&& ((input_image->fmt >= TCC_LCDC_IMG_FMT_444SEP) && (input_image->fmt <= TCC_LCDC_IMG_FMT_YUV422ITL1))) {
				VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, 1U);
				VIOC_RDMA_SetImageY2RMode(pViqe_60hz_info->pRDMABase_60Hz, 0x2);
			}

			if (vioc_get_out_type() == input_image->private_data.dolbyVision_info.reg_out_type) {
				VIOC_V_DV_SetSize(NULL, pViqe_60hz_info->pRDMABase_60Hz,
					input_image->offset_x, input_image->offset_y, Hactive, Vactive);
				vioc_v_dv_prog(input_image->private_data.dolbyVision_info.md_hdmi_addr,
					input_image->private_data.dolbyVision_info.reg_addr,
					input_image->private_data.optional_info[VID_OPT_CONTENT_TYPE], 1);
			}
		} else {
			(void)pr_err("[ERR][VIQE] -4- Should be implement other layer configuration\n");
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto out;
		}
	}
#endif

#ifdef CONFIG_TCC_VIOCMG
	if (viqe_lock) {
		viocmg_free_viqe(VIOCMG_CALLERID_VOUT);
		/* prevent KCS warning */
	}
#endif

	tcc_video_post_process(input_image);
	if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO) {
		atomic_inc(&gFrmCnt_60Hz);
		/* prevent KCS warning */
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
	} else if (input_image->Lcdc_layer == (unsigned int)RDMA_VIDEO_SUB) {
		atomic_inc(&gFrmCnt_Sub_60Hz);
		/* prevent KCS warning */
#endif
	} else {
		dprintk("%s check layer\n", __func__);
		/* prevent KCS warning */
	}

out:
	dprintk("%s---\n", __func__);
}

void TCC_VIQE_DI_DeInit60Hz(void)
{
	const struct tcc_viqe_60hz_virt_addr_info_t *pViqe_60hz_info = NULL;
	const void __iomem *pVIQE_Info = NULL;
	unsigned int nVIOC_VIQE = 0;

	if (gOutputMode == TCC_OUTPUT_LCD) {
		pVIQE_Info = viqe_common_info.pVIQE0;
		nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		pViqe_60hz_info = &viqe_60hz_lcd_info;
	} else {
		if (gLcdc_layer_60Hz == (unsigned int)RDMA_VIDEO) {
			atomic_set(&gFrmCnt_60Hz, 0);
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
			pViqe_60hz_info = &viqe_60hz_external_info;
		} else if (gLcdc_layer_60Hz == (unsigned int)RDMA_VIDEO_SUB) {
		#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
			atomic_set(&gFrmCnt_Sub_60Hz, 0);
			pVIQE_Info = viqe_common_info.pVIQE1;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE1;
		#else
			pVIQE_Info = viqe_common_info.pVIQE0;
			nVIOC_VIQE = viqe_common_info.gVIOC_VIQE0;
		#endif
			pViqe_60hz_info = &viqe_60hz_external_sub_info;
		} else {
			(void)pr_err("ERROR: %s check layer\n", __func__);
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto out;
		}
	}

	(void)pr_info("[INF][VIQE] %s\n", __func__);

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	set_hdmi_drm(DRM_OFF, NULL, (unsigned int)RDMA_VIDEO);
#endif

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	bStep_Check = DEF_DV_CHECK_NUM;
#endif

	VIOC_RDMA_SetImageDisable(pViqe_60hz_info->pRDMABase_60Hz);
	VIOC_RDMA_SetImageY2REnable(pViqe_60hz_info->pRDMABase_60Hz, 1U);
	/* Y2RMode Default 0 (Studio Color) */
	VIOC_RDMA_SetImageY2RMode(pViqe_60hz_info->pRDMABase_60Hz, 0x02);

	if ((bool)gDeintlS_Use_60Hz) {
		(void)VIOC_CONFIG_PlugOut(viqe_common_info.gVIOC_Deintls);
		tcc_sdintls_swreset();
		gDeintlS_Use_Plugin = 0;
	} else {
		#ifdef CONFIG_TCC_VIOCMG
		if (viocmg_lock_viqe(VIOCMG_CALLERID_VOUT) == 0) {
			(void)VIOC_CONFIG_PlugOut(nVIOC_VIQE);
			tcc_viqe_swreset(nVIOC_VIQE);

			viocmg_free_viqe(VIOCMG_CALLERID_VOUT);
		}
		#else
		(void)VIOC_CONFIG_PlugOut(nVIOC_VIQE);
		tcc_viqe_swreset(nVIOC_VIQE);
		#endif
	}

	(void)VIOC_CONFIG_PlugOut(pViqe_60hz_info->gSCALER_num_60Hz);
	VIOC_CONFIG_SWReset(pViqe_60hz_info->gSCALER_num_60Hz, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pViqe_60hz_info->gSCALER_num_60Hz, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(pViqe_60hz_info->gVIQE_RDMA_num_60Hz, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pViqe_60hz_info->gVIQE_RDMA_num_60Hz, VIOC_CONFIG_CLEAR);

out:
	gVIQE_Init_State = 0;
}

static unsigned int tcc_viqe_commom_dt_parse(
	const struct device_node *np,
	struct tcc_viqe_common_virt_addr_info_t *viqe_common)
{
	const struct device_node *viqe_node0, *viqe_node1, *deintls_node;

	viqe_node0 = of_parse_phandle(np, "telechips,viqe,0", 0);
	(void)of_property_read_u32_index(np, "telechips,viqe,0", 1, &viqe_common->gVIOC_VIQE0);
	if (viqe_node0 == NULL) {
		(void)pr_err("[ERR][VIQE] could not find viqe_video viqe0 node\n");
		/* prevent KCS warning */
	} else {
		viqe_common->pVIQE0 = VIOC_VIQE_GetAddress(viqe_common->gVIOC_VIQE0);
		//(void)pr_info("[INF][VIQE] viqe_video viqe0 %d %px\n", viqe_common->gVIOC_VIQE0, viqe_common->pVIQE0);
	}

	viqe_node1 = of_parse_phandle(np, "telechips,viqe,1", 0);
	(void)of_property_read_u32_index(np, "telechips,viqe,1", 1, &viqe_common->gVIOC_VIQE1);
	if (viqe_node1 == NULL) {
		(void)pr_info("[INF][VIQE] this soc has only one viqe node\n");

		viqe_common->pVIQE1 = viqe_common->pVIQE0;
		viqe_common->gVIOC_VIQE1 = viqe_common->gVIOC_VIQE0;
		not_exist_viqe1 = 1;
		//(void)pr_info("[INF][VIQE] viqe_video viqe1(viqe0) %d %px\n", index, viqe_common->pVIQE1);
	} else {
		viqe_common->pVIQE1 = VIOC_VIQE_GetAddress(viqe_common->gVIOC_VIQE1);
		//(void)pr_info("[INF][VIQE] viqe_video viqe1 %d %px\n", viqe_common->gVIOC_VIQE1, viqe_common->pVIQE1);
	}

	deintls_node = of_find_compatible_node(NULL, NULL, "telechips,vioc_deintls");
	(void)of_property_read_u32(np, "vioc_deintls", &viqe_common->gVIOC_Deintls);
	if (deintls_node == NULL) {
		(void)pr_err("[ERR][VIQE] can not find vioc deintls\n");
		/* prevent KCS warning */
	} else {
		viqe_common->pDEINTLS = VIOC_DEINTLS_GetAddress();
		if (viqe_common->pDEINTLS == NULL) {
			(void)pr_err("can not find vioc deintls address\n");
			/* prevent KCS warning */
		}
		//(void)pr_info("[INF][VIQE] deintls 0x%px\n", viqe_common->pDEINTLS);
	}

	(void)of_property_read_u32(np, "board_num", &viqe_common->gBoard_stb);

	iprintk("VIQE Common Information\n");
	iprintk("Deintls(0x%x) Viqe0(0x%x) Viqe1(0x%x) gBoard_stb(%d)\n",
		viqe_common->gVIOC_Deintls, viqe_common->gVIOC_VIQE0,
		viqe_common->gVIOC_VIQE1, viqe_common->gBoard_stb);

	return 0;
}

static unsigned int tcc_viqe_60hz_dt_parse(
	const struct device_node *np,
	struct tcc_viqe_60hz_virt_addr_info_t *viqe_60hz_info)
{
	unsigned int index;
	const struct device_node *rdma_node, *wmixer_node, *disp_node, *scaler_node;

	rdma_node = of_parse_phandle(np, "telechips,rdma,60", 0);
	(void)of_property_read_u32_index(np, "telechips,rdma,60", 1, &viqe_60hz_info->gVIQE_RDMA_num_60Hz);
	if (rdma_node == NULL) {
		(void)pr_err("[ERR][VIQE] could not find viqe_video rdma_60hz node\n");
		/* prevent KCS warning */
	} else {
		viqe_60hz_info->pRDMABase_60Hz = VIOC_RDMA_GetAddress(viqe_60hz_info->gVIQE_RDMA_num_60Hz);
		//(void)pr_info("[INF][VIQE] viqe_video rdma_60hz %d %x\n", viqe_60hz_info->gVIQE_RDMA_num_60Hz, viqe_60hz_info->pRDMABase_60Hz);
	}

	wmixer_node = of_parse_phandle(np, "telechips,wmixer", 0);
	(void)of_property_read_u32_index(np, "telechips,wmixer", 1, &index);
	if (wmixer_node == NULL) {
		(void)pr_err("[ERR][VIQE] could not find viqe_video wmixer node\n");
		/* prevent KCS warning */
	} else {
		viqe_60hz_info->pWMIXBase_60Hz = VIOC_WMIX_GetAddress(index);
		//(void)pr_info("[INF][VIQE] viqe_video wmixe_60hz %d %x\n", index, viqe_60hz_info->pWMIXBase_60Hz);
	}

	disp_node = of_parse_phandle(np, "telechips,disp", 0);
	(void)of_property_read_u32_index(np, "telechips,disp", 1, &index);
	if (disp_node == NULL) {
		(void)pr_err("[ERR][VIQE] could not find viqe_video display node\n");
		/* prevent KCS warning */
	} else {
		viqe_60hz_info->pDISPBase_60Hz = VIOC_DISP_GetAddress(index);
		//(void)pr_info("[INF][VIQE] viqe_video display_60hz 0x%x %p\n", index, viqe_60hz_info->pDISPBase_60Hz);
	}

	scaler_node = of_parse_phandle(np, "telechips,sc", 0);
	(void)of_property_read_u32_index(np, "telechips,sc", 1, &viqe_60hz_info->gSCALER_num_60Hz);
	if (scaler_node == NULL) {
		(void)pr_err("[ERR][VIQE] could not find viqe_video scaler node\n");
		/* prevent KCS warning */
	} else {
		viqe_60hz_info->pSCALERBase_60Hz = VIOC_SC_GetAddress(viqe_60hz_info->gSCALER_num_60Hz);
		//(void)pr_info("[INF][VIQE] viqe_video scaler_60hz %d %x\n", viqe_60hz_info->gSCALER_num_60Hz, viqe_60hz_info->pSCALERBase_60Hz);
	}

	iprintk("VIQE_60Hz Information\n");
	iprintk("Reg_Base: RDMA/SC/WMIX/DISP = 0x%p / 0x%p / 0x%p / 0x%p\n",
		viqe_60hz_info->pRDMABase_60Hz, viqe_60hz_info->pSCALERBase_60Hz,
		viqe_60hz_info->pWMIXBase_60Hz, viqe_60hz_info->pDISPBase_60Hz);
	iprintk("NUM: RDMA/SC = 0x%x / 0x%x\n",
		viqe_60hz_info->gVIQE_RDMA_num_60Hz, viqe_60hz_info->gSCALER_num_60Hz);

	return 0;
}

static int tcc_viqe_m2m_dt_parse(
	const struct device_node *main_deIntl, const struct device_node *sub_deIntl,
	const struct device_node *sc_main, const struct device_node *sc_sub,
	struct tcc_viqe_m2m_virt_addr_info_t *main_m2m,
	struct tcc_viqe_m2m_virt_addr_info_t *sub_m2m)
{
	const struct device_node *main_deintl_rdma_node, *sub_deintl_rdma_node;
	const struct device_node *sc_rdma_node, *sc_wmixer_node;
	const struct device_node *sc_scaler_node;
	struct device_node *sc_wdma_node;
	int ret = -ENOMEM;

	//Main :: m2m_deIntl_info
	main_deintl_rdma_node = of_parse_phandle(main_deIntl, "telechips,main_rdma,m2m", 0);
	(void)of_property_read_u32_index(main_deIntl, "telechips,main_rdma,m2m", 1, &main_m2m->gVIQE_RDMA_num_m2m);
	if (main_deintl_rdma_node == NULL) {
		(void)pr_err("[ERR][VIQE] could not find viqe_video main_rdma_m2m node\n");
		/* prevent KCS warning */
	} else {
		main_m2m->pRDMABase_m2m = VIOC_RDMA_GetAddress(main_m2m->gVIQE_RDMA_num_m2m);
		//(void)pr_info("[INF][VIQE] main_video rdma_m2m %d %x\n", index, main_m2m->pRDMABase_m2m);
	}

	//Sub :: m2m_deIntl_info
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
	// VIQE1
	sub_deintl_rdma_node = of_parse_phandle(sub_deIntl, "telechips,sub_rdma,m2m", 0);
	(void)of_property_read_u32_index(sub_deIntl, "telechips,sub_rdma,m2m", 1, &sub_m2m->gVIQE_RDMA_num_m2m);
	if (sub_deintl_rdma_node == NULL) {
		(void)pr_err("[ERR][VIQE] could not find viqe_video sub_rdma_m2m node\n");
		/* prevent KCS warning */
	} else {
		sub_m2m->pRDMABase_m2m = VIOC_RDMA_GetAddress(sub_m2m->gVIQE_RDMA_num_m2m);
		//(void)pr_info("[INF][VIQE] sub_video rdma_m2m %d %x\n", index, main_m2m->pRDMABase_m2m);
	}
#else
	// Simple DeInterlacer
	sub_deintl_rdma_node = of_parse_phandle(sub_deIntl, "telechips,sub_rdma,m2m", 0);
	(void)of_property_read_u32_index(sub_deIntl, "telechips,sub_rdma,m2m", 1, &sub_m2m->gDEINTLS_RDMA_num_m2m);
	if (sub_deintl_rdma_node == NULL) {
		(void)pr_err("[ERR][VIQE] could not find deintls_video sub_rdma_m2m node\n");
		/* prevent KCS warning */
	} else {
		sub_m2m->pRDMABase_m2m = VIOC_RDMA_GetAddress(sub_m2m->gDEINTLS_RDMA_num_m2m);
		//(void)pr_info("[INF][VIQE] sub_video rdma_m2m %d %x\n", index, sub_m2m->pRDMABase_m2m);
	}
#endif

	//Main :: m2m_deIntl_scaler
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_type_t), GFP_KERNEL);
	if (scaler == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler->info = kzalloc(sizeof(struct tcc_lcdc_image_update), GFP_KERNEL);
	if (scaler->info == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler->info kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler->data = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_data), GFP_KERNEL);
	if (scaler->data == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler->data kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler->vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
	if (scaler->vioc_intr == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler->vioc_intr kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler->rdma = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler->rdma == NULL) {
		(void)pr_err("[ERR][VIQE] ail scaler->rdma kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler->wmix = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler->wmix == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler->wmix kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler->sc = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler->sc == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler->sc kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler->wdma = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler->wdma == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler->wdma kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	sc_rdma_node = of_parse_phandle(sc_main, "m2m_sc_rdma", 0);
	(void)of_property_read_u32_index(sc_main, "m2m_sc_rdma", 1, &scaler->rdma->id);
	if (sc_rdma_node == NULL) {
		(void)pr_info("[INF][VIQE] could not find viqe_video rdma node of scaler\n");
	} else {
		scaler->rdma->reg = VIOC_RDMA_GetAddress(scaler->rdma->id);
		//(void)pr_info("[INF][VIQE] main_video scaler->rdma[%d] %x\n", scaler->rdma->id, scaler->rdma->reg);
	}

	sc_wmixer_node = of_parse_phandle(sc_main, "m2m_sc_wmix", 0);
	(void)of_property_read_u32_index(sc_main, "m2m_sc_wmix", 1, &scaler->wmix->id);
	if (sc_wmixer_node == NULL) {
		(void)pr_info("[INF][VIQE] could not find viqe_video wmixer node of scaler\n");
	} else {
		scaler->wmix->reg = VIOC_WMIX_GetAddress(scaler->wmix->id);
		//(void)of_property_read_u32(sc_main, "m2m_sc_wmix_path", &scaler->wmix->path);
		//(void)pr_info("[INF][VIQE] main_video scaler->wmix[%d] %x\n", scaler->wmix->id, scaler->wmix->reg);
	}

	sc_scaler_node = of_parse_phandle(sc_main, "m2m_sc_scaler", 0);
	(void)of_property_read_u32_index(sc_main, "m2m_sc_scaler", 1, &scaler->sc->id);
	if (sc_scaler_node == NULL) {
		(void)pr_info("[INF][VIQE] could not find viqe_video scaler node of scaler\n");
	} else {
		scaler->sc->reg = VIOC_SC_GetAddress(scaler->sc->id);
		//(void)pr_info("[INF][VIQE] main_video scaler->scalers[%d] %x\n", scaler->sc->id, scaler->sc->reg);
	}

	sc_wdma_node = of_parse_phandle(sc_main, "m2m_sc_wdma", 0);
	(void)of_property_read_u32_index(sc_main, "m2m_sc_wdma", 1, &scaler->wdma->id);
	if (sc_wdma_node == NULL) {
		(void)pr_info("[INF][VIQE] could not find viqe_videowdma node of scaler\n");
	} else {
		scaler->wdma->reg = VIOC_WDMA_GetAddress(scaler->wdma->id);
		/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
		scaler->irq = irq_of_parse_and_map(sc_wdma_node, (int)get_vioc_index(scaler->wdma->id));
		scaler->vioc_intr->id = (unsigned int)VIOC_INTR_WD0 + get_vioc_index(scaler->wdma->id);
		scaler->vioc_intr->bits = VIOC_WDMA_IREQ_EOFR_MASK;
		//(void)pr_info("[INF][VIQE] main_video scaler->wdmas[%d] %x\n", scaler->wdma->id, scaler->wdma->reg);
	}

	(void)of_property_read_u32(sc_main, "m2m_sc_settop_support", &scaler->settop_support);

	//Sub :: m2m_deIntl_scaler
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler_sub = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_type_t), GFP_KERNEL);
	if (scaler_sub == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler_sub kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler_sub->info = kzalloc(sizeof(struct tcc_lcdc_image_update), GFP_KERNEL);
	if (scaler_sub->info == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler_sub->info kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler_sub->data = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_data), GFP_KERNEL);
	if (scaler_sub->data == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler_sub->data kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler_sub->vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
	if (scaler_sub->vioc_intr == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler_sub->vioc_intr kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler_sub->rdma = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler_sub->rdma == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler_sub->rdma kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler_sub->wmix = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler_sub->wmix == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler_sub->wmix kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler_sub->sc = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler_sub->sc == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler_sub->sc kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler_sub->wdma = kzalloc(sizeof(struct tcc_viqe_m2m_scaler_vioc), GFP_KERNEL);
	if (scaler_sub->wdma == NULL) {
		(void)pr_err("[ERR][VIQE] fail scaler_sub->wdma kzalloc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	sc_rdma_node = of_parse_phandle(sc_sub, "m2m_sc_rdma", 0);
	(void)of_property_read_u32_index(sc_sub, "m2m_sc_rdma", 1, &scaler_sub->rdma->id);
	if (sc_rdma_node == NULL) {
		(void)pr_info("[INF][VIQE] could not find viqe_video rdma node of scaler_sub\n");
	} else {
		scaler_sub->rdma->reg = VIOC_RDMA_GetAddress(scaler_sub->rdma->id);
		//(void)pr_info("[INF][VIQE] sub_video scaler_sub->rdma[%d] %x\n", scaler_sub->rdma->id, scaler_sub->rdma->reg);
	}

	sc_wmixer_node = of_parse_phandle(sc_sub, "m2m_sc_wmix", 0);
	(void)of_property_read_u32_index(sc_sub, "m2m_sc_wmix", 1, &scaler_sub->wmix->id);
	if (sc_wmixer_node == NULL) {
		(void)pr_info("[INF][VIQE] could not find viqe_video wmixer node of scaler_sub\n");
	} else {
		scaler_sub->wmix->reg = VIOC_WMIX_GetAddress(scaler_sub->wmix->id);
		//(void)of_property_read_u32(sc_sub, "m2m_sc_wmix_path", &scaler_sub->wmix->path);
		//(void)pr_info("[INF][VIQE] sub_video scaler_sub->wmix[%d] %x\n", scaler_sub->wmix->id, scaler_sub->wmix->reg);
	}

	sc_scaler_node = of_parse_phandle(sc_sub, "m2m_sc_scaler", 0);
	(void)of_property_read_u32_index(sc_sub, "m2m_sc_scaler", 1, &scaler_sub->sc->id);
	if (sc_scaler_node == NULL) {
		(void)pr_info("[INF][VIQE] could not find viqe_video scaler_sub node of scaler_sub\n");
		/* prevent KCS warning */
	} else {
		scaler_sub->sc->reg = VIOC_SC_GetAddress(scaler_sub->sc->id);
		//(void)pr_info("[INF][VIQE] sub_video scaler_sub->scalers[%d] %x\n", scaler_sub->sc->id, scaler_sub->sc->reg);
	}

	sc_wdma_node = of_parse_phandle(sc_sub, "m2m_sc_wdma", 0);
	(void)of_property_read_u32_index(sc_sub, "m2m_sc_wdma", 1, &scaler_sub->wdma->id);
	if (sc_wdma_node == NULL) {
		(void)pr_info("[INF][VIQE] could not find viqe_video wdma node of scaler_sub\n");
	} else {
		scaler_sub->wdma->reg = VIOC_WDMA_GetAddress(scaler_sub->wdma->id);
		/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
		scaler_sub->irq = irq_of_parse_and_map(sc_wdma_node, (int)(get_vioc_index(scaler_sub->wdma->id)));
		scaler_sub->vioc_intr->id = (unsigned int)VIOC_INTR_WD0 + get_vioc_index(scaler_sub->wdma->id);
		scaler_sub->vioc_intr->bits = VIOC_WDMA_IREQ_EOFR_MASK;
		//(void)pr_info("[INF][VIQE] sub_video scaler_sub->wdmas[%d] %x\n", scaler_sub->wdma->id, scaler_sub->wdma->reg);
	}

	(void)of_property_read_u32(sc_sub, "m2m_sc_settop_support", &scaler_sub->settop_support);

	iprintk("Main/Sub M2M Information\n");
	iprintk("RDMA  : (0x%x)0x%p / (0x%x)0x%p\n", scaler->rdma->id, scaler->rdma->reg, scaler_sub->rdma->id, scaler_sub->rdma->reg);
	iprintk("Scaler: (0x%x)0x%p / (0x%x)0x%p\n", scaler->sc->id, scaler->sc->reg, scaler_sub->sc->id, scaler_sub->sc->reg);
	iprintk("WMIX  : (0x%x)0x%p / (0x%x)0x%p\n", scaler->wmix->id, scaler->wmix->reg, scaler_sub->wmix->id, scaler_sub->wmix->reg);
	iprintk("WDMA  : (0x%x)0x%p / (0x%x)0x%p\n", scaler->wdma->id, scaler->wdma->reg, scaler_sub->wdma->id, scaler_sub->wdma->reg);
	iprintk("Intr  : (%d)0x%08x / (%d)0x%08x\n", scaler->vioc_intr->id, scaler->irq, scaler_sub->vioc_intr->id, scaler_sub->irq);

	iprintk("VIQE  : (0x%x)0x%p\n", main_m2m->gVIQE_RDMA_num_m2m, main_m2m->pRDMABase_m2m);
#ifdef CONFIG_TCC_VIQE_FOR_SUB_M2M
	iprintk("VIQE1 : (0x%x)0x%p\n", sub_m2m->gVIQE_RDMA_num_m2m, sub_m2m->pRDMABase_m2m);
#else
	iprintk("sDeIntl: (0x%x)0x%p\n", sub_m2m->gDEINTLS_RDMA_num_m2m, sub_m2m->pRDMABase_m2m);
#endif

	/* return success */
	ret = 0;

out:
	return ret;
}

/*****************************************************************************
 * TCC_VIQE Probe
 ******************************************************************************/
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tcc_viqe_probe(struct platform_device *pdev)
{
	const struct device *dev = &pdev->dev;
	const struct device_node *viqe_common, *lcd_60hz_node;
	const struct device_node *external_60Hz_node, *external_sub_60Hz_node;
	const struct device_node *m2m_deintl_main_node, *m2m_deintl_sub_node;
	const struct device_node *m2m_scaler_main_node, *m2m_scaler_sub_node;
	int ret = 0;

	//viqe_common
	viqe_common = of_find_node_by_name(dev->of_node, "tcc_viqe_viqe_common");
	if (viqe_common == NULL) {
		(void)pr_err("[ERR][VIQE] could not find viqe_common node\n");
		ret = -ENODEV;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	(void)tcc_viqe_commom_dt_parse(viqe_common, &viqe_common_info);

	//viqe_60hz_info
	lcd_60hz_node = of_find_node_by_name(dev->of_node, "tcc_video_viqe_lcd");
	external_60Hz_node = of_find_node_by_name(dev->of_node, "tcc_video_viqe_external");
	external_sub_60Hz_node = of_find_node_by_name(dev->of_node, "tcc_video_viqe_external_sub");

	if (viqe_common_info.gBoard_stb == 0U) {
		if (lcd_60hz_node == NULL) {
			(void)pr_err("[ERR][VIQE] could not find viqe_lcd node\n");
			ret = -ENODEV;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto out;
		}
	} else {
		if (external_60Hz_node == NULL) {
			(void)pr_err("[ERR][VIQE] could not find viqe_external node\n");
			ret = -ENODEV;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto out;
		}
		if (external_sub_60Hz_node == NULL) {
			(void)pr_err("[ERR][VIQE] could not find viqe_external node\n");
			ret = -ENODEV;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto out;
		}
	}

	#ifdef CONFIG_USE_SUB_MULTI_FRAME
	atomic_set(&viqe_sub_m2m_init, 0);
	INIT_LIST_HEAD(&video_queue_list);
	#endif

	(void)tcc_viqe_60hz_dt_parse(lcd_60hz_node, &viqe_60hz_lcd_info);
	(void)tcc_viqe_60hz_dt_parse(external_60Hz_node, &viqe_60hz_external_info);
	(void)tcc_viqe_60hz_dt_parse(external_sub_60Hz_node, &viqe_60hz_external_sub_info);

	//m2m_viqe
	m2m_deintl_main_node = of_find_node_by_name(dev->of_node, "tcc_video_main_m2m");
	m2m_deintl_sub_node = of_find_node_by_name(dev->of_node, "tcc_video_sub_m2m");

	m2m_scaler_main_node = of_find_node_by_name(dev->of_node, "tcc_video_scaler_main_m2m");
	m2m_scaler_sub_node = of_find_node_by_name(dev->of_node, "tcc_video_scaler_sub_m2m");

	if ((m2m_deintl_main_node == NULL) || (m2m_scaler_main_node == NULL) || (m2m_deintl_sub_node == NULL) || (m2m_scaler_sub_node == NULL)) {
		(void)pr_err("[ERR][VIQE] could not find m2m_viqe node\n");
		ret = -ENODEV;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	(void)tcc_viqe_m2m_dt_parse(m2m_deintl_main_node, m2m_deintl_sub_node, m2m_scaler_main_node, m2m_scaler_sub_node, &main_m2m_info, &sub_m2m_info);

out:
	return ret;
}

static const struct of_device_id tcc_viqe_of_match[] = {
	{.compatible = "telechips,tcc_viqe", },
	{ },
};
MODULE_DEVICE_TABLE(of, tcc_viqe_of_match);

static struct platform_driver tcc_viqe = {
	.probe = tcc_viqe_probe,
	.driver = {
		.name = "tcc_viqe",
		.owner = THIS_MODULE,
		.of_match_table = tcc_viqe_of_match,
	},
};

static int __init tcc_viqe_inf_init(void)
{
	return platform_driver_register(&tcc_viqe);
}

static void __exit tcc_viqe_inf_exit(void)
{
	platform_driver_unregister(&tcc_viqe);
}

/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
module_init(tcc_viqe_inf_init);
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
module_exit(tcc_viqe_inf_exit);

/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_5_9_violation : FALSE] */
MODULE_AUTHOR("Telechips Inc.");
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_5_9_violation : FALSE] */
MODULE_DESCRIPTION("TCC viqe driver");
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_5_9_violation : FALSE] */
MODULE_LICENSE("GPL");

// SPDX-License-Identifier: GPL-2.0-or-later

/* tcc_drm_lcd.c
 *
 * Copyright (c) 2016 Telechips Inc.
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors:
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <drm/drmP.h>

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/component.h>
#include <linux/regmap.h>
#include <video/of_display_timing.h>
#include <video/of_videomode.h>
#include <drm/tcc_drm.h>
#include <drm/drm_crtc_helper.h>

#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_ddicfg.h>
#include <video/tcc/vioc_config.h>
#if defined(CONFIG_VIOC_PVRIC_FBDC)
#include <video/tcc/vioc_pvric_fbdc.h>
#include <pvrsrvkm/img_drm_fourcc_internal.h>
#endif
#include <tcc_drm_address.h>
#include <tcc_drm_drv.h>
#include <tcc_drm_dpi.h>
#include <tcc_drm_fb.h>
#include <tcc_drm_crtc.h>
#include <tcc_drm_plane.h>
#include <tcc_drm_lcd_base.h>

#define LOG_TAG "DRMLCD"

/*
 * LCD stands for Fully Interactive Various Display and
 * as a display controller, it transfers contents drawn on memory
 * to a LCD Panel through Display Interfaces such as RGB or
 * CPU Interface.
 */

#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
struct lcd_chromakeys {
	unsigned int chromakey_enable;
	struct drm_chromakey_t value;
	struct drm_chromakey_t mask;
};
#endif

struct vioc_fmt_t {
	unsigned int y2r;
	unsigned int swap;
	unsigned int fmt;
	#if defined(CONFIG_VIOC_PVRIC_FBDC)
	VIOC_PVRICCTRL_SWIZZ_MODE swizz;
	#endif
};

struct lcd_context {
	struct device *dev;
	struct drm_device *drm;
	struct tcc_drm_crtc *crtc;
	struct drm_encoder *encoder;
	struct tcc_drm_plane planes[CRTC_WIN_NR_MAX];

	unsigned long crtc_flags;

	/* VSYNC */
	wait_queue_head_t wait_vsync_queue;
	atomic_t wait_vsync_event;

	/* Display Disable Done */
	wait_queue_head_t wait_display_done_queue;
	atomic_t wait_display_done_event;

	spinlock_t irq_lock;

	/*
	 * keep_logo
	 * If this flag is set to 1, DRM driver skips process that fills its
	 * plane with block colorwhen it is binded. In other words, If so any
	 * Logo image was displayed on screen at booting time, it will is keep
	 * dislayed on screen until the DRM application is executed.
	 */
	int keep_logo;


	#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
	struct mutex chromakey_mutex;
	struct lcd_chromakeys lcd_chromakeys[3];
	#endif

	/* count for fifo-underrun */
	unsigned int cnt_underrun;

	/* Device driver data */
	struct tcc_drm_device_data *data;

	/* H/W data */
	struct tcc_hw_device hw_data;
	#if defined(CONFIG_SMP)
	int last_cpu;
	#endif
};

static const uint32_t lcd_formats[] = {
	DRM_FORMAT_BGR565,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
};

static const uint32_t lcd_formats_vrdma[] = {
	DRM_FORMAT_BGR565,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YVU420,
};

static irqreturn_t lcd_irq_handler(int irq, void *dev_id)
{
	unsigned int display_device_blk_num;

	struct lcd_context *lctx = (struct lcd_context *)dev_id;
	u32 dispblock_status = 0;

	if (lctx == NULL) {
		dev_warn(lctx->dev,
				"[WARN][%s] %s lctx is NULL\r\n",
					LOG_TAG, __func__);
		return IRQ_HANDLED;
	}

	display_device_blk_num =
		get_vioc_index(lctx->hw_data.display_device.blk_num);

	if (
		is_vioc_display_device_intr_masked(
			lctx->hw_data.display_device.blk_num,
			VIOC_DISP_INTR_DISPLAY)) {
		dev_dbg(lctx->dev,
				"[DEBUG][%s] %s interrupt was not enabled\r\n",
				LOG_TAG, __func__);
		return IRQ_HANDLED;
	}

	/* Get TCC VIOC block status register */
	dispblock_status = vioc_intr_get_status(display_device_blk_num);

	if (dispblock_status & (1 << VIOC_DISP_INTR_RU)) {
		vioc_intr_clear(
			display_device_blk_num, (1 << VIOC_DISP_INTR_RU));

		/* check the crtc is detached already from encoder */
		if (!lctx->drm) {
			dev_warn(
				lctx->dev,
				"[WARN][%s] %s drm is not binded\r\n",
				LOG_TAG, __func__);
			goto out;
		}

		/* set wait vsync event to zero and wake up queue. */
		if (atomic_read(&lctx->wait_vsync_event)) {
			atomic_set(&lctx->wait_vsync_event, 0);
			wake_up_all(&lctx->wait_vsync_queue);
		}
		tcc_drm_crtc_vblank_handler(&lctx->crtc->base);
	}

	/* Check FIFO underrun. */
	if (dispblock_status & (1 << VIOC_DISP_INTR_FU)) {
		vioc_intr_clear(
			display_device_blk_num, (1 << VIOC_DISP_INTR_FU));
		if (
			VIOC_DISP_Get_TurnOnOff(
				lctx->hw_data.display_device.virt_addr)) {
			if ((lctx->cnt_underrun++ % 120) == 0)
				dev_err(
					lctx->dev,
					"[ERROR][%s] FIFO UNDERRUN status(0x%x) %s\n",
					LOG_TAG, dispblock_status, __func__);
		}
	}

	if (dispblock_status & (1 << VIOC_DISP_INTR_DD)) {
		if (atomic_read(&lctx->wait_display_done_event)) {
			atomic_set(&lctx->wait_display_done_event, 0);
			wake_up_all(&lctx->wait_display_done_queue);
		}
		vioc_intr_clear(
			display_device_blk_num, (1 << VIOC_DISP_INTR_DD));
	}

	if (dispblock_status & (1 << VIOC_DISP_INTR_SREQ))
		vioc_intr_clear(
			display_device_blk_num,
			(1 << VIOC_DISP_INTR_SREQ));
out:
	return IRQ_HANDLED;
}

static int lcd_enable_vblank(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *lctx = crtc->context;
	unsigned long irqflags;
	int ret = 0;

	spin_lock_irqsave(&lctx->irq_lock, irqflags);
	if (!test_and_set_bit(CRTC_FLAGS_IRQ_BIT, &lctx->crtc_flags)) {
		vioc_intr_clear(
			lctx->hw_data.display_device.blk_num,
			VIOC_DISP_INTR_DISPLAY);

		#if defined(CONFIG_SMP)
		irq_set_affinity(lctx->hw_data.display_device.irq_num,
				 cpumask_of(lctx->last_cpu));
		#endif
		vioc_intr_enable(
			lctx->hw_data.display_device.irq_num,
			get_vioc_index(lctx->hw_data.display_device.blk_num),
			VIOC_DISP_INTR_DISPLAY);
	}
	spin_unlock_irqrestore(&lctx->irq_lock, irqflags);
	return ret;
}

static void lcd_disable_vblank(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *lctx = crtc->context;
	unsigned long irqflags;

	spin_lock_irqsave(&lctx->irq_lock, irqflags);
	if (test_and_clear_bit(CRTC_FLAGS_IRQ_BIT, &lctx->crtc_flags)) {
		vioc_intr_disable(
			lctx->hw_data.display_device.irq_num,
			get_vioc_index(lctx->hw_data.display_device.blk_num),
			VIOC_DISP_INTR_DISPLAY);
		vioc_intr_clear(lctx->hw_data.display_device.blk_num,
			VIOC_DISP_INTR_DISPLAY);
	}
	spin_unlock_irqrestore(&lctx->irq_lock, irqflags);
}

#if defined(CONFIG_TCCDRM_USES_WAIT_VBLANK)
static void lcd_wait_for_vblank(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *lctx = crtc->context;

	atomic_set(&lctx->wait_vsync_event, 1);

	/*
	 * wait for LCD to signal VSYNC interrupt or return after
	 * timeout which is set to 50ms (refresh rate of 20).
	 */
	if (test_bit(CRTC_FLAGS_IRQ_BIT, &lctx->crtc_flags)) {
		if (
			!wait_event_interruptible_timeout(
				lctx->wait_vsync_queue,
				!atomic_read(&lctx->wait_vsync_event),
				msecs_to_jiffies(50)))
			DRM_DEBUG_KMS("vblank wait timed out.\n");
	}
}
#endif

static void lcd_set_video_output(struct lcd_context *lctx, unsigned int win,
					bool enable)
{
	if (win >= lctx->hw_data.rdma_counts) {
		dev_dbg(lctx->dev, "[DEBUG][%s] %s win(%d) is out of range (%d)\r\n",
			LOG_TAG, __func__, win, lctx->hw_data.rdma_counts);
		return;
	}
	if (lctx->hw_data.rdma[win].virt_addr == NULL) {
		dev_warn(lctx->dev, "[WARN][%s] %s virtual address of win(%d) is NULL\r\n",
			LOG_TAG, __func__, win);
		return;
	}
	if (enable) {
		VIOC_RDMA_SetImageEnable(lctx->hw_data.rdma[win].virt_addr);
	} else {
		if (test_bit(CRTC_FLAGS_PCLK_BIT, &lctx->crtc_flags)) {
			VIOC_RDMA_SetImageDisable(
				lctx->hw_data.rdma[win].virt_addr);
		} else {
			unsigned int enabled;

			VIOC_RDMA_GetImageEnable(
				lctx->hw_data.rdma[win].virt_addr, &enabled);
			if (enabled) {
				dev_info(
					lctx->dev,
					"[INFO][%s] %s win(%d) Disable RDMA with NoWait\r\n",
					LOG_TAG, __func__, win);
				VIOC_RDMA_SetImageDisableNW(
					lctx->hw_data.rdma[win].virt_addr);
			}
		}
	}
}

static void lcd_enable_video_output(struct lcd_context *lctx, unsigned int win)
{
	lcd_set_video_output(lctx, win, true);
}

static void lcd_disable_video_output(struct lcd_context *lctx, unsigned int win)
{
	lcd_set_video_output(lctx, win, false);
}

#if defined(CONFIG_VIOC_PVRIC_FBDC)
static int lcd_connect_pvric_fbdc(
	struct lcd_context *lctx, unsigned int win, unsigned int base_addr,
	unsigned int fmt, unsigned int width, unsigned int height)
{
	void __iomem *fbdc_base;
	unsigned int fbdc_id, rdma_id;
	int plugged_rdma;
	int ret = 0;

	fbdc_base = lctx->hw_data.fbdc[win].virt_addr;
	fbdc_id = lctx->hw_data.fbdc[win].blk_num;
	rdma_id = lctx->hw_data.rdma[win].blk_num;

	if (
		test_bit(
			TCC_PLANE_FLAG_FBDC_PLUGGED,
			&lctx->planes[win].flags)) {
		goto out;
	}

	plugged_rdma = VIOC_CONFIG_GetFBDCPath(fbdc_id);
	if (plugged_rdma < 0) {
		ret = -EINVAL;
		dev_err(lctx->dev,
			"[ERROR][%s] %s failed VIOC_CONFIG_GetFBDCPath\r\n",
			LOG_TAG, __func__);
		goto out;
	}

	/* 0x1F means pvric_fbcd is not connected */
	if (get_vioc_index(plugged_rdma) != 0x1F && plugged_rdma != rdma_id) {
		ret = -EINVAL;
		dev_err(lctx->dev,
			"[ERROR][%s] %s FBDC[%d] is already used by RDMA[%d]\r\n",
			LOG_TAG, __func__, get_vioc_index(fbdc_id),
			get_vioc_index(plugged_rdma));
		goto out;
	}

	/* Disable RDMA */
	lcd_disable_video_output(lctx, win);

	/* swreset fbdc and rdma */
	VIOC_CONFIG_SWReset(fbdc_id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(fbdc_id, VIOC_CONFIG_CLEAR);

	VIOC_CONFIG_SWReset(rdma_id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(rdma_id, VIOC_CONFIG_CLEAR);

	VIOC_CONFIG_FBCDECPath(
		fbdc_id, lctx->hw_data.rdma[win].blk_num, 1);
	VIOC_PVRIC_FBDC_SetBasicConfiguration(
		lctx->hw_data.fbdc[win].virt_addr,
		base_addr, fmt, width, height, 0);

	vioc_pvric_fbdc_set_val0_cr_ch0123(fbdc_base, 0, 0, 0, 0);
	vioc_pvric_fbdc_set_val1_cr_ch0123(fbdc_base, 0, 0, 0, 0);
	vioc_pvric_fbdc_set_cr_filter_enable(fbdc_base);

	set_bit(TCC_PLANE_FLAG_FBDC_PLUGGED, &lctx->planes[win].flags);
out:
	return ret;
}

static int lcd_disconnect_pvric_fbdc(
	struct lcd_context *lctx, unsigned int win)
{
	unsigned int fbdc_id, rdma_id;
	int ret = 0;

	if (win >= lctx->hw_data.rdma_counts) {
		dev_dbg(lctx->dev, "[DEBUG][%s] %s win(%d) is out of range (%d)\r\n",
			LOG_TAG, __func__, win, lctx->hw_data.rdma_counts);
		goto out;
	}
	if (
		!test_bit(TCC_PLANE_FLAG_FBDC_PLUGGED,
			  &lctx->planes[win].flags)) {
		goto out;
	}

	fbdc_id = lctx->hw_data.fbdc[win].blk_num;
	rdma_id = lctx->hw_data.rdma[win].blk_num;

	/* Disable RDMA */
	lcd_disable_video_output(lctx, win);

	/* Stop FBDC */
	VIOC_PVRIC_FBDC_TurnOFF(
		lctx->hw_data.fbdc[win].virt_addr);
	VIOC_PVRIC_FBDC_SetUpdateInfo(
		lctx->hw_data.fbdc[win].virt_addr, 1);
	VIOC_CONFIG_FBCDECPath(fbdc_id, rdma_id, 0);

	clear_bit(TCC_PLANE_FLAG_FBDC_PLUGGED, &lctx->planes[win].flags);
out:
	return ret;
}

static int lcd_check_pvric_fbdc(unsigned int fbdc_id)
{
	unsigned int fbdc_status, fbdc_intr_id;
	int ret = 0;

	fbdc_intr_id = get_vioc_index(fbdc_id) + VIOC_INTR_AFBCDEC0;

	fbdc_status = vioc_intr_get_status(fbdc_intr_id);

	if (fbdc_status & VIOC_PVRIC_FBDC_INT_MASK) {
		vioc_intr_clear(fbdc_intr_id,
				fbdc_status & VIOC_PVRIC_FBDC_INT_MASK);

		if (fbdc_status & PVRICSTS_TILE_ERR_MASK) {
			ret = -1;
			pr_err("%s FBDC[%d] PVRICSTS_TILE_ERR_MASK\r\n",
				__func__, fbdc_id -VIOC_FBCDEC0);
		}
		if (fbdc_status & PVRICSTS_ADDR_ERR_MASK) {
			ret = -1;
			pr_err("%s FBDC[%d] PVRICSTS_ADDR_ERR_MASK\r\n",
				__func__, fbdc_id -VIOC_FBCDEC0);
		}
		if (fbdc_status & PVRICSTS_EOF_ERR_MASK) {
			ret = -1;
			pr_err("%s FBDC[%d] PVRICSTS_EOF_ERR_MASK\r\n",
				__func__, fbdc_id -VIOC_FBCDEC0);
		}
	}
	return ret;
}

static int lcd_set_pvric_fbdc(struct lcd_context *lctx,
			      unsigned int win, unsigned int base_addr,
			      struct drm_framebuffer *fb,
			      struct vioc_fmt_t *vioc_fmt, unsigned int width,
			      unsigned int height)
{
	void __iomem *fbdc_base;
	unsigned int fbdc_id, rdma_id;
	int ret = 0;

	fbdc_base = lctx->hw_data.fbdc[win].virt_addr;
	if (fbdc_base == NULL) {
		if (fb->modifier == DRM_FORMAT_MOD_PVR_FBCDC_8x8_V10) {
			ret = -ENODEV;
			pr_err("%s TCC_PLANE_FLAG_MOD_FBDC bug virt_addr is NULL\r\n", __func__);
		}

		goto out;
	}

	fbdc_id = lctx->hw_data.fbdc[win].blk_num;
	rdma_id = lctx->hw_data.rdma[win].blk_num;

	if (fb->modifier != DRM_FORMAT_MOD_PVR_FBCDC_8x8_V10) {
		lcd_disconnect_pvric_fbdc(lctx, win);
		goto out;
	}

	if (lcd_check_pvric_fbdc(fbdc_id) < 0)
		lcd_disconnect_pvric_fbdc(lctx, win);

	ret = lcd_connect_pvric_fbdc(lctx, win, base_addr, vioc_fmt->fmt,
				     width, height);
	if (ret < 0)
		goto out;

	VIOC_PVRIC_FBDC_SetRequestBase(fbdc_base, base_addr);
	VIOC_PVRIC_FBDC_SetOutBufBase(fbdc_base, base_addr);
	VIOC_PVRIC_FBDC_SetARGBSwizzMode(fbdc_base, vioc_fmt->swizz);
	VIOC_PVRIC_FBDC_TurnOn(fbdc_base);
	VIOC_PVRIC_FBDC_SetUpdateInfo(fbdc_base, 1);
out:
	return ret;
}
#endif

static int lcd_atomic_check(struct tcc_drm_crtc *crtc,
				struct drm_crtc_state *state)
{
	void __iomem *ddi_config;
	struct drm_display_mode *mode = &state->adjusted_mode;
	struct lcd_context *lctx = crtc->context;
	unsigned long ideal_clk, lcd_rate;
	int pixel_clock_from_hdmi = 0;
	u32 clkdiv;

	ddi_config = VIOC_DDICONFIG_GetAddress();
	#if !defined(CONFIG_ARCH_TCC897X)
	if (
		VIOC_DDICONFIG_GetPeriClock(
			ddi_config,
			get_vioc_index(lctx->hw_data.display_device.blk_num)))
		pixel_clock_from_hdmi = 1;
	#endif

	if (mode->clock == 0) {
		dev_warn(
			lctx->dev,
			"[WARN][%s] %s Mode has zero clock value.\n",
			LOG_TAG, __func__);
		return -EINVAL;
	}

	if (!pixel_clock_from_hdmi) {
		ideal_clk = mode->clock * 1000;

		lcd_rate = clk_get_rate(lctx->hw_data.ddc_clock);
		if (2 * lcd_rate < ideal_clk) {
			dev_warn(
				lctx->dev,
				"[WARN][%s] %s sclk_lcd clock too low(%lu) for requested pixel clock(%lu)\n",
				LOG_TAG, __func__, lcd_rate, ideal_clk);
		}

		/*
		 * Find the clock divider value that gets us closest
		 * to ideal_clk
		 */
		clkdiv = DIV_ROUND_CLOSEST(lcd_rate, ideal_clk);
		if (clkdiv >= 0x200) {
			dev_warn(lctx->dev,
				"[WARN][%s] %s requested pixel clock(%lu) too low\n",
				LOG_TAG, __func__, ideal_clk);
		}
	}

	return 0;
}

static void lcd_drm_fmt_to_vioc_fmt(uint32_t pixel_format,
				 struct vioc_fmt_t *vioc_fmt)
{
	memset(vioc_fmt, 0, sizeof(*vioc_fmt));

	switch (pixel_format) {
	case DRM_FORMAT_BGR565:
		vioc_fmt->swap = VIOC_SWAP_BGR;  /* B-G-R */
		vioc_fmt->fmt = TCC_LCDC_IMG_FMT_RGB565;
		#if defined(CONFIG_VIOC_PVRIC_FBDC)
		vioc_fmt->swizz = VIOC_PVRICCTRL_SWIZZ_ABGR;
		#endif
		break;
	case DRM_FORMAT_RGB565:
		vioc_fmt->fmt = TCC_LCDC_IMG_FMT_RGB565;
		break;
	case DRM_FORMAT_BGR888:
		vioc_fmt->swap = VIOC_SWAP_BGR;  /* B-G-R */
		vioc_fmt->fmt = TCC_LCDC_IMG_FMT_RGB888_3;
		#if defined(CONFIG_VIOC_PVRIC_FBDC)
		vioc_fmt->swizz = VIOC_PVRICCTRL_SWIZZ_ABGR;
		#endif
		break;
	case DRM_FORMAT_RGB888:
		vioc_fmt->fmt = TCC_LCDC_IMG_FMT_RGB888_3;
		break;
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
		vioc_fmt->swap = VIOC_SWAP_BGR;  /* B-G-R */
		vioc_fmt->fmt = TCC_LCDC_IMG_FMT_RGB888;
		#if defined(CONFIG_VIOC_PVRIC_FBDC)
		vioc_fmt->swizz = VIOC_PVRICCTRL_SWIZZ_ABGR;
		#endif
		break;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		vioc_fmt->fmt = TCC_LCDC_IMG_FMT_RGB888;
		break;
	case DRM_FORMAT_NV12:
		vioc_fmt->fmt = TCC_LCDC_IMG_FMT_YUV420ITL0;
		vioc_fmt->y2r = 1;
		break;
	case DRM_FORMAT_NV21:
		vioc_fmt->fmt = TCC_LCDC_IMG_FMT_YUV420ITL1;
		vioc_fmt->y2r = 1;
		break;
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV420:
		vioc_fmt->fmt = TCC_LCDC_IMG_FMT_YUV420SP;
		vioc_fmt->y2r = 1;
		break;
	default:
		vioc_fmt->fmt = TCC_LCDC_IMG_FMT_RGB888;
		break;
	}
}

static void lcd_win_set_pixfmt(void __iomem *pRDMA,
			       struct vioc_fmt_t *vioc_fmt, uint32_t width)
{
	/* Default RGB SWAP */
	VIOC_RDMA_SetImageY2REnable(pRDMA, vioc_fmt->y2r);
	VIOC_RDMA_SetImageRGBSwapMode(pRDMA, vioc_fmt->swap);
	VIOC_RDMA_SetImageOffset(pRDMA, vioc_fmt->fmt, width);
	VIOC_RDMA_SetImageFormat(pRDMA, vioc_fmt->fmt);
}

static void lcd_atomic_begin(struct tcc_drm_crtc *crtc)
{
	//struct lcd_context *lctx = crtc->context;
}

static void lcd_atomic_flush(struct tcc_drm_crtc *crtc)
{
	//struct lcd_context *lctx = crtc->context;
	tcc_crtc_handle_event(crtc);
}

static void lcd_update_plane(struct tcc_drm_crtc *crtc,
			      struct tcc_drm_plane *plane)
{
	struct tcc_drm_plane_state *state =
				to_tcc_plane_state(plane->base.state);
	struct lcd_context *lctx = crtc->context;
	struct drm_framebuffer *fb = state->base.fb;
	struct vioc_fmt_t vioc_fmt;

	dma_addr_t dma_addr;
	unsigned int val, size, offset;
	unsigned int win = plane->win;
	unsigned int cpp = fb->format->cpp[0];
	unsigned int pitch = fb->pitches[0];
	unsigned int enabled;

	/* TCC specific structure */
	void __iomem *pWMIX;
	void __iomem *pRDMA;

	if (lctx->keep_logo) {
		dev_info(
			lctx->dev,
			"[INFO][%s] %s skip logo\r\n",
			LOG_TAG, __func__);
		lctx->keep_logo--;
		return;
	}
	if (win >= lctx->hw_data.rdma_counts) {
		pr_err(
			"[ERROR][%s] %s win(%d) is out of range (%d)\r\n",
			LOG_TAG,
			__func__, win, lctx->hw_data.rdma_counts);
		return;
	}
	if (lctx->hw_data.rdma[win].virt_addr == NULL) {
		pr_err(
			"[ERROR][%s] %s virtual address of win(%d) is NULL\r\n",
			LOG_TAG, __func__, win);
		return;
	}

	/*
	 * DRM v1.2.8
	 * Add name "overlay_skip_yuv" of plane properties to support
	 * Android hwrendere.
	 * If this property is set on plane, This plane will be skip process
	 * of atomic_update when input format is YUV and returns success.
	 */
	if (
		DRM_PLANE_TYPE(lctx->hw_data.rdma_plane_type[win]) ==
		DRM_PLANE_TYPE_OVERLAY &&
		DRM_PLANE_FLAG(lctx->hw_data.rdma_plane_type[win]) ==
		DRM_PLANE_FLAG(DRM_PLANE_FLAG_SKIP_YUV_FORMAT) &&
		(
			fb->format->format == DRM_FORMAT_NV12 ||
			fb->format->format == DRM_FORMAT_NV21 ||
			fb->format->format == DRM_FORMAT_YUV420 ||
			fb->format->format == DRM_FORMAT_YVU420))
		return;

	pWMIX = lctx->hw_data.wmixer.virt_addr;
	pRDMA = lctx->hw_data.rdma[win].virt_addr;

	offset = state->src.x * cpp;
	offset += state->src.y * pitch;

	dma_addr = tcc_drm_fb_dma_addr(fb, 0) + offset;
	if (dma_addr == (dma_addr_t)0) {
		DRM_ERROR(
			"[ERROR][%s] %s dma address of win(%d) is NULL\r\n",
			LOG_TAG, __func__, win);
		return;
	}
	if (upper_32_bits(dma_addr) > 0) {
		DRM_ERROR(
			"[ERROR][%s] %s dma address of win(%d) is out of range\r\n",
			LOG_TAG, __func__, win);
		return;
	}

	/* buffer start address */
	val = lower_32_bits(dma_addr);
	#if defined(CONFIG_VIOC_PVRIC_FBDC)
	/* Skip Header */
	if (fb->modifier == DRM_FORMAT_MOD_PVR_FBCDC_8x8_V10)
		val += ALIGN(DIV_ROUND_UP(state->src.w *
					  state->src.h, 64), 256);
	#endif

	/* Swreset RDMAs to prevent fifo-underrun if RDMA was disabled */
	VIOC_RDMA_GetImageEnable(
		lctx->hw_data.rdma[win].virt_addr, &enabled);
	if (!enabled) {
		if (
			get_vioc_type(
				lctx->hw_data.rdma[win].blk_num) ==
				get_vioc_type(VIOC_RDMA)) {
			dev_dbg(lctx->dev,
				"[DEBUG][%s] %s win(%d) swreset RDMA, because rdma was disabled\r\n",
				LOG_TAG, __func__, win);
			VIOC_CONFIG_SWReset(
				lctx->hw_data.rdma[win].blk_num,
				VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(
				lctx->hw_data.rdma[win].blk_num,
				VIOC_CONFIG_CLEAR);
		}
	}

	/* Set RDMA formats */
	lcd_drm_fmt_to_vioc_fmt(fb->format->format, &vioc_fmt);

	/* Set PVRIC FBDC */
	#if defined(CONFIG_VIOC_PVRIC_FBDC)
	if (lcd_set_pvric_fbdc(
		lctx, win, val, fb, &vioc_fmt, state->crtc.w,
		state->crtc.h) < 0) {
		goto out;
	}
	#endif

	/* Using the pixel alpha */
	VIOC_RDMA_SetImageAlphaSelect(pRDMA, 1);
	if (
		fb->format->format == DRM_FORMAT_ARGB8888 ||
		fb->format->format == DRM_FORMAT_ABGR8888)
		VIOC_RDMA_SetImageAlphaEnable(pRDMA, 1);
	else
		VIOC_RDMA_SetImageAlphaEnable(pRDMA, 0);

	switch (fb->format->format) {
	case DRM_FORMAT_YVU420:
		VIOC_RDMA_SetImageBase(
			pRDMA, val, val+fb->offsets[2], val+fb->offsets[1]);
		break;
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_YUV420:
		VIOC_RDMA_SetImageBase(
			pRDMA, val, val+fb->offsets[1], val+fb->offsets[2]);
		break;
	default:
		VIOC_RDMA_SetImageBase(pRDMA, val, 0, 0);
		break;
	}
	VIOC_RDMA_SetImageSize(pRDMA, state->crtc.w, state->crtc.h);

	lcd_win_set_pixfmt(pRDMA, &vioc_fmt, state->src.w);
	lcd_enable_video_output(lctx, win);
	#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
	mutex_lock(&lctx->chromakey_mutex);
	#endif
	VIOC_WMIX_SetPosition(pWMIX, win, state->crtc.x, state->crtc.y);
	VIOC_WMIX_SetUpdate(pWMIX);
	#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
	mutex_unlock(&lctx->chromakey_mutex);
	#endif

#if defined(CONFIG_VIOC_PVRIC_FBDC)
out:
#endif
	/* buffer end address */
	size = pitch * state->crtc.h;
	val = lower_32_bits(dma_addr) + size;

        #if defined(CONFIG_ARCH_DMA_ADDR_T_64BIT)
        DRM_DEBUG_KMS(
                "start addr = 0x%llx, end addr = 0x%x, size = 0x%x\n",
                dma_addr, val, size);
        #else
        DRM_DEBUG_KMS(
                "start addr = 0x%x, end addr = 0x%x, size = 0x%x\n",
                dma_addr, val, size);
        #endif
	DRM_DEBUG_KMS(
		"ovl_width = %d, ovl_height = %d\n",
		state->crtc.w, state->crtc.h);
}

static void lcd_disable_plane(struct tcc_drm_crtc *crtc,
			       struct tcc_drm_plane *plane)
{
	struct lcd_context *lctx = crtc->context;
	unsigned int win = plane->win;

	lcd_disable_video_output(lctx, win);
	#if defined(CONFIG_VIOC_PVRIC_FBDC)
	lcd_disconnect_pvric_fbdc(lctx, win);
	#endif
}

static void lcd_enable(struct tcc_drm_crtc *crtc)
{
	int ret;
	struct lcd_context *lctx = crtc->context;

	if (lctx->hw_data.connector_type == DRM_MODE_CONNECTOR_LVDS) {
		if (!test_and_set_bit(CRTC_FLAGS_PCLK_BIT, &lctx->crtc_flags)) {
			dev_info(
				lctx->dev,
				"[INFO][%s] %s Enable LVDS-PCLK %ldHz \r\n",
				LOG_TAG, __func__,
				clk_get_rate(lctx->hw_data.ddc_clock));
			ret = clk_prepare_enable(lctx->hw_data.ddc_clock);
			if  (ret < 0)
				dev_warn(
					lctx->dev,
					"[WARN][%s] %s failed to enable the lcd clk\r\n",
					LOG_TAG, __func__);
		}
	}

	if (!crtc->enabled) {
		dev_info(
			lctx->dev,
			"[INFO][%s] %s Turn on\r\n",
			LOG_TAG, __func__);
		#if defined(CONFIG_PM)
		pm_runtime_get_sync(lctx->dev);
		#endif
		crtc->enabled = 1;
		VIOC_DISP_TurnOn(lctx->hw_data.display_device.virt_addr);
	}

	lctx->cnt_underrun = 0;
}

static void lcd_disable(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *lctx = crtc->context;
	int i;

	for (i = 0; i < RDMA_MAX_NUM; i++) {
		lcd_disable_video_output(lctx, i);
		#if defined(CONFIG_VIOC_PVRIC_FBDC)
		lcd_disconnect_pvric_fbdc(lctx, i);
		#endif
	}

	dev_info(
		lctx->dev, "[INFO][%s] %s Turn off\r\n",
		LOG_TAG, __func__);
	if (VIOC_DISP_Get_TurnOnOff(lctx->hw_data.display_device.virt_addr)) {
		atomic_set(&lctx->wait_display_done_event, 1);
		VIOC_DISP_TurnOff(lctx->hw_data.display_device.virt_addr);
		if (!test_bit(CRTC_FLAGS_IRQ_BIT, &lctx->crtc_flags)) {
			msleep(30);
			dev_info(
				lctx->dev,
				"[INFO][%s] %s It will be wait 30ms because interrupt is not enabled.\n",
				LOG_TAG, __func__);
		} else {
			if (
				!wait_event_interruptible_timeout(
					lctx->wait_display_done_queue,
					!atomic_read(
						&lctx->wait_display_done_event),
					msecs_to_jiffies(30)))
				dev_warn(
					lctx->dev,
					"[WARN][%s] %s A timeout occurred while waiting for the display controller is shut down.\n",
					LOG_TAG, __func__);
		}
	}

	#if defined(CONFIG_PM)
	if (crtc->enabled)
		pm_runtime_put_sync(lctx->dev);
	#endif

	if (test_and_clear_bit(CRTC_FLAGS_PCLK_BIT, &lctx->crtc_flags)) {
		dev_info(
			lctx->dev,
			"[INFO][%s] %s Disable PCLK\r\n",
			LOG_TAG, __func__);
		clk_disable_unprepare(lctx->hw_data.ddc_clock);
	}
	crtc->enabled = 0;
}

#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
static int lcd_set_chromakey(struct tcc_drm_crtc *crtc,
	unsigned int chromakey_layer, unsigned int chromakey_enable,
	struct drm_chromakey_t *value, struct drm_chromakey_t *mask)
{
	struct lcd_context *lctx = crtc->context;

	mutex_lock(&lctx->chromakey_mutex);
	if (crtc->enabled) {
		VIOC_WMIX_SetChromaKey(
			lctx->hw_data.wmixer.virt_addr, chromakey_layer,
			chromakey_enable,
			value->red, value->green, value->blue,
			mask->red, mask->green, mask->blue);
		VIOC_WMIX_SetUpdate(lctx->hw_data.wmixer.virt_addr);
	}
	/* store information */
	lctx->lcd_chromakeys[
		chromakey_layer].chromakey_enable = chromakey_enable;
	memcpy(
		&lctx->lcd_chromakeys[chromakey_layer].value,
		value, sizeof(*value));
	memcpy(
		&lctx->lcd_chromakeys[chromakey_layer].mask,
		mask, sizeof(*mask));
	mutex_unlock(&lctx->chromakey_mutex);
	return 0;
}

static int lcd_get_chromakey(struct tcc_drm_crtc *crtc,
	unsigned int chromakey_layer, unsigned int *chromakey_enable,
	struct drm_chromakey_t *value, struct drm_chromakey_t *mask)
{
	int ret = -1;
	struct lcd_context *lctx = crtc->context;

	mutex_lock(&lctx->chromakey_mutex);
	if (crtc->enabled) {
		VIOC_WMIX_GetChromaKey(
			lctx->hw_data.wmixer.virt_addr, chromakey_layer,
			chromakey_enable, &value->red,
			&value->green, &value->blue,
			&mask->red, &mask->green,
			&mask->blue);
		ret = 0;
	} else {
		*chromakey_enable =
		lctx->lcd_chromakeys[chromakey_layer].chromakey_enable;
		memcpy(
			value, &lctx->lcd_chromakeys[chromakey_layer].value,
			sizeof(*value));
		memcpy(
			mask, &lctx->lcd_chromakeys[chromakey_layer].mask,
			sizeof(*mask));
	}
	mutex_unlock(&lctx->chromakey_mutex);
	return ret;
}
#endif

static void lcd_mode_set_nofb(struct tcc_drm_crtc *crtc)
{
	struct lcd_context *lctx = crtc->context;
	#if defined(CONFIG_ARCH_TCC805X)
	struct drm_crtc_state *state = crtc->base.state;
	#endif
	#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
	int i;
	#endif

	#if defined(CONFIG_ARCH_TCC805X)
	(void)tcc_drm_crtc_set_display_timing(&crtc->base,
			(const struct drm_display_mode *)&state->adjusted_mode,
			(const struct tcc_hw_device *)&lctx->hw_data);
	#endif
	#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
	mutex_lock(&lctx->chromakey_mutex);
	for (i = 0; i < 3; i++) {
		VIOC_WMIX_SetChromaKey(lctx->hw_data.wmixer.virt_addr, i,
			lctx->lcd_chromakeys[i].chromakey_enable,
			lctx->lcd_chromakeys[i].value.red,
			lctx->lcd_chromakeys[i].value.green,
			lctx->lcd_chromakeys[i].value.blue,
			lctx->lcd_chromakeys[i].mask.red,
			lctx->lcd_chromakeys[i].mask.green,
			lctx->lcd_chromakeys[i].mask.blue);
		VIOC_WMIX_SetUpdate(lctx->hw_data.wmixer.virt_addr);
	}
	mutex_unlock(&lctx->chromakey_mutex);
	#endif
	if (lctx->hw_data.connector_type != DRM_MODE_CONNECTOR_LVDS) {
		int ret;

		if (
			!test_and_set_bit(
				CRTC_FLAGS_PCLK_BIT, &lctx->crtc_flags)) {
			dev_info(
				lctx->dev,
				"[INFO][%s] %s Enable PCLK  %ldHz\r\n",
				LOG_TAG, __func__,
				clk_get_rate(lctx->hw_data.ddc_clock));
			ret = clk_prepare_enable(lctx->hw_data.ddc_clock);
			if  (ret < 0) {
				dev_warn(
					lctx->dev,
					"[WARN][%s] %s failed to enable the lcd clk\r\n",
					LOG_TAG, __func__);
			}
		}
	}
}

static const struct tcc_drm_crtc_ops lcd_crtc_ops = {
	.enable = lcd_enable, /* drm_crtc_helper_funcs->atomic_enable */
	.disable = lcd_disable, /* drm_crtc_helper_funcs->atomic_disable */
	.enable_vblank = lcd_enable_vblank,
	.disable_vblank = lcd_disable_vblank,
	.atomic_begin = lcd_atomic_begin,
	.update_plane = lcd_update_plane,
	.disable_plane = lcd_disable_plane,
	.atomic_flush = lcd_atomic_flush,
	.atomic_check = lcd_atomic_check,
	#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
	.set_chromakey = lcd_set_chromakey,
	.get_chromakey = lcd_get_chromakey,
	#endif
	.mode_set_nofb = lcd_mode_set_nofb,
};

static int lcd_bind(struct device *dev, struct device *master, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *drm = dev_get_drvdata(master);
	struct drm_plane *primary = NULL;
	struct drm_plane *cursor = NULL;
	struct lcd_context *lctx;

	const uint32_t *formats_list;
	int formats_list_size;
	int ret;
	int i;

	if (!dev->of_node) {
		dev_err(
			dev,
			"[ERROR][%s] %s failed to get the device node\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
		goto out;
	}
	/* Create device lctx */
	lctx = devm_kzalloc(dev, sizeof(*lctx), GFP_KERNEL);
	if (!lctx) {
		ret = -ENOMEM;
		goto out;
	}
	lctx->dev = dev;
	lctx->data =
		(struct tcc_drm_device_data *)of_device_get_match_data(
			&pdev->dev);
	if (!lctx->data) {
		dev_err(
			dev,
			"[ERROR][%s] %s failed to get match data\r\n",
			LOG_TAG, __func__);
		ret = -ENOMEM;
		goto out;
	}

	#if defined(CONFIG_SMP)
	for_each_online_cpu(i) {
		lctx->last_cpu = i;
	}
	dev_info(dev, "[INFO][%s] %s last cpu id is = %d\r\n",
		 LOG_TAG, __func__, i);
	#endif

	lctx->drm = drm;
	if (
		tcc_drm_address_dt_parse(
			pdev, &lctx->hw_data, lctx->data->version) < 0) {
		dev_err(
			dev,
			"[ERROR][%s] %s failed to parse device tree\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
		goto out;
	}
	switch (lctx->data->output_type) {
	case TCC_DISPLAY_TYPE_LCD:
		lctx->data->name = kstrdup("drm_lcd", GFP_KERNEL);
		#if defined(CONFIG_DRM_TCC_LCD_VIC)
		lctx->hw_data.vic = CONFIG_DRM_TCC_LCD_VIC;
		#endif
		break;
	case TCC_DISPLAY_TYPE_EXT:
		lctx->data->name = kstrdup("drm_ext", GFP_KERNEL);
		#if defined(CONFIG_DRM_TCC_EXT_VIC)
		lctx->hw_data.vic = CONFIG_DRM_TCC_EXT_VIC;
		#endif
		break;
	case TCC_DISPLAY_TYPE_THIRD:
		lctx->data->name = kstrdup("drm_third", GFP_KERNEL);
		#if defined(CONFIG_DRM_TCC_THIRD_VIC)
		lctx->hw_data.vic = CONFIG_DRM_TCC_THIRD_VIC;
		#endif
		break;
	case TCC_DISPLAY_TYPE_FOURTH:
		lctx->data->name = kstrdup("drm_fourth", GFP_KERNEL);
		#if defined(CONFIG_DRM_TCC_FOURTH_VIC)
		lctx->hw_data.vic = CONFIG_DRM_TCC_FOURTH_VIC;
		#endif
		break;
	default:
		ret = -ENODEV;
		goto out;
	}
	dev_info(
		dev,
		"[INFO][%s] %s %s start probe with pdev(%p), dev(%p)\r\n",
		LOG_TAG, __func__, lctx->data->name, pdev, dev);
	spin_lock_init(&lctx->irq_lock);

	/* Disable & clean interrupt */
	vioc_intr_disable(lctx->hw_data.display_device.irq_num,
		  get_vioc_index(lctx->hw_data.display_device.blk_num),
		  VIOC_DISP_INTR_DISPLAY);
	vioc_intr_clear(lctx->hw_data.display_device.blk_num,
		VIOC_DISP_INTR_DISPLAY);

	init_waitqueue_head(&lctx->wait_vsync_queue);
	atomic_set(&lctx->wait_vsync_event, 0);

	init_waitqueue_head(&lctx->wait_display_done_queue);
	atomic_set(&lctx->wait_display_done_event, 0);
	platform_set_drvdata(pdev, lctx);
	lctx->encoder = tcc_dpi_probe(dev, &lctx->hw_data);
	if (lctx->encoder == NULL) {
		ret = PTR_ERR(lctx->encoder);
		goto out;
	}
	#if defined(CONFIG_DRM_TCC_KEEP_LOGO)
	#if defined(CONFIG_DRM_FBDEV_EMULATION)
	/* If fbdev emulation was selected then FB core calls set_par */
	lctx->keep_logo = 1;
	#if defined(CONFIG_LOGO)
	/* If logo was selected then FB core calls set_par twice */
	lctx->keep_logo++;
	#endif // CONFIG_LOGO
	#endif // CONFIG_DRM_FBDEV_EMULATION
	#endif // CONFIG_DRM_TCC_KEEP_LOGO

	#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
	mutex_init(&lctx->chromakey_mutex);
	#endif

	/* Activate CRTC clocks */
	if (!test_and_set_bit(CRTC_FLAGS_VCLK_BIT, &lctx->crtc_flags)) {
		ret = clk_prepare_enable(lctx->hw_data.vioc_clock);
		if (ret < 0) {
			dev_err(
				dev,
				"[ERROR][%s] %s failed to enable the bus clk\n",
				LOG_TAG, __func__);
			goto out;
		}
	}
	if (!test_and_set_bit(CRTC_FLAGS_PCLK_BIT, &lctx->crtc_flags)) {
		ret = clk_prepare_enable(lctx->hw_data.ddc_clock);
		if (ret < 0) {
			dev_err(
				dev,
				"[ERROR][%s] %s failed to enable the ddc clk\n",
				LOG_TAG, __func__);
			goto out;
		}
	}
	for (i = 0; i < lctx->hw_data.rdma_counts; i++) {
		if (lctx->hw_data.fbdc[i].virt_addr != NULL)
			set_bit(TCC_DRM_PLANE_CAP_FBDC,
				&lctx->planes[i].caps);
		if (VIOC_RDMA_IsVRDMA(lctx->hw_data.rdma[i].blk_num)) {
			formats_list = lcd_formats_vrdma;
			formats_list_size = ARRAY_SIZE(lcd_formats_vrdma);
		} else {
			formats_list = lcd_formats;
			formats_list_size = ARRAY_SIZE(lcd_formats);
		}
		ret = tcc_plane_init(
			drm,
			&lctx->planes[i].base,
			DRM_PLANE_TYPE(lctx->hw_data.rdma_plane_type[i]),
			formats_list, formats_list_size);
		if (ret) {
			dev_err(
				dev,
				"[ERROR][%s] %s failed to initizliaed the planes\n",
				LOG_TAG, __func__);
			goto out;
		}

		lctx->planes[i].win = i;
		if (
			DRM_PLANE_TYPE(lctx->hw_data.rdma_plane_type[i]) ==
			DRM_PLANE_TYPE_PRIMARY)
			primary = &lctx->planes[i].base;
		if (
			DRM_PLANE_TYPE(lctx->hw_data.rdma_plane_type[i]) ==
			DRM_PLANE_TYPE_CURSOR)
			cursor = &lctx->planes[i].base;
	}

	lctx->crtc = tcc_drm_crtc_create(
		drm, primary, cursor, lctx->data->output_type, &lcd_crtc_ops,
		lctx);
	if (lctx->crtc == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	if (lctx->encoder)
		ret = tcc_dpi_bind(drm, lctx->encoder, &lctx->hw_data);

	if (ret)
		goto out;
	#if defined(CONFIG_SMP)
	irq_set_affinity(lctx->hw_data.display_device.irq_num,
			 cpumask_of(lctx->last_cpu));
	#endif
	ret = devm_request_irq(
		lctx->dev, lctx->hw_data.display_device.irq_num,
		lcd_irq_handler, IRQF_SHARED, lctx->data->name, lctx);
	if (ret < 0) {
		dev_err(
			lctx->dev,
			"[ERROR][%s] %s failed to request irq\r\n",
			LOG_TAG, __func__);
		goto out;
	}

	#if defined(CONFIG_PM)
	pm_runtime_enable(dev);
	#endif

	return 0;
out:
	return ret;
}

static void lcd_unbind(struct device *dev, struct device *master,
			void *data)
{
	struct lcd_context *lctx = dev_get_drvdata(dev);
	unsigned long irqflags;

	if (lctx->encoder)
		tcc_dpi_remove(lctx->encoder);

	spin_lock_irqsave(&lctx->irq_lock, irqflags);
	if (test_and_clear_bit(CRTC_FLAGS_IRQ_BIT, &lctx->crtc_flags)) {
		vioc_intr_disable(
			lctx->hw_data.display_device.irq_num,
			get_vioc_index(lctx->hw_data.display_device.blk_num),
			VIOC_DISP_INTR_DISPLAY);
		vioc_intr_clear(lctx->hw_data.display_device.blk_num,
			VIOC_DISP_INTR_DISPLAY);
	}
	spin_unlock_irqrestore(&lctx->irq_lock, irqflags);

	devm_free_irq(lctx->dev, lctx->hw_data.display_device.irq_num, lctx);

	/* Deactivate CRTC clock */
	if (test_and_clear_bit(CRTC_FLAGS_VCLK_BIT, &lctx->crtc_flags))
		clk_disable_unprepare(lctx->hw_data.vioc_clock);

	#if defined(CONFIG_PM)
	pm_runtime_disable(dev);
	#endif

	kfree(lctx->data->name);
	devm_kfree(dev, lctx);
}

static const struct component_ops lcd_component_ops = {
	.bind	= lcd_bind,
	.unbind = lcd_unbind,
};

int tcc_drm_lcd_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &lcd_component_ops);
}
EXPORT_SYMBOL(tcc_drm_lcd_probe);

int tcc_drm_lcd_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &lcd_component_ops);
	return 0;
}
EXPORT_SYMBOL(tcc_drm_lcd_remove);

#ifdef CONFIG_PM
static int tcc_lcd_suspend(struct device *dev)
{
	struct lcd_context *lctx = dev_get_drvdata(dev);

	dev_info(dev, "[INFO][%s] %s \r\n",
				LOG_TAG, __func__);

	if (test_bit(CRTC_FLAGS_VCLK_BIT, &lctx->crtc_flags)) {
		dev_info(
			dev, "[INFO][%s] %s Disable vclk\r\n",
			LOG_TAG, __func__);
		clk_disable_unprepare(lctx->hw_data.vioc_clock);
	}

	return 0;
}

static int tcc_lcd_resume(struct device *dev)
{
	struct lcd_context *lctx = dev_get_drvdata(dev);

	dev_info(dev, "[INFO][%s] %s \r\n",
				LOG_TAG, __func__);
	if (test_bit(CRTC_FLAGS_VCLK_BIT, &lctx->crtc_flags)) {
		dev_info(
			dev,
			"[INFO][%s] %s Enable vclk\r\n", LOG_TAG, __func__);
		clk_prepare_enable(lctx->hw_data.vioc_clock);
	}
	return 0;
}
#endif

const struct dev_pm_ops tcc_lcd_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tcc_lcd_suspend, tcc_lcd_resume)
};
EXPORT_SYMBOL(tcc_lcd_pm_ops);


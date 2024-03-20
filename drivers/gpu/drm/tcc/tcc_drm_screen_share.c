// SPDX-License-Identifier: GPL-2.0-or-later

/* tcc_drm_screen_share.c
 *
 * Copyright (c) 2016 Telechips Inc.
 * Authors:
 *	Jayden Kim <kimdy@telechips.com>
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
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/component.h>
#include <linux/regmap.h>
#include <linux/timer.h>

#include <video/of_display_timing.h>
#include <video/of_videomode.h>
#include <drm/tcc_drm.h>
#include <drm/drm_crtc_helper.h>

#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_ddicfg.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_timer.h>

#include <tcc_drm_address.h>
#include <tcc_drm_drv.h>
#include <tcc_drm_fb.h>
#include <tcc_drm_crtc.h>
#include <tcc_drm_plane.h>
#include <tcc_drm_dpi.h>


#include <tcc_screen_share.h>

#define LOG_TAG "DRMLCD"

struct screen_share_context {
	struct device *dev;
	struct drm_device *drm;
	struct tcc_drm_crtc *crtc;
	struct drm_encoder *encoder;
	struct tcc_drm_plane plane;

	unsigned long crtc_flags;

	/* VSYNC */
	int vrefresh;

	unsigned int timer_id;
	#if defined(CONFIG_CHECK_DRM_TCC_VSYNC_TIME_GAP)
	struct timeval prev_time;
	#endif

	spinlock_t irq_lock;

	/* Device driver data */
	struct tcc_drm_device_data *data;

	/* H/W data */
	struct tcc_hw_device hw_data;

	/* timer irq */
	unsigned int irq_num;
};

static struct tcc_drm_device_data screen_share_data = {
	.version = TCC_DRM_DT_VERSION_1_0,
	.output_type = TCC_DISPLAY_TYPE_SCREEN_SHARE,
};

static const struct of_device_id screen_share_driver_dt_match[] = {
	{
		.compatible = "telechips,tcc-drm-screen-share",
		.data = (const void *)&screen_share_data,
	}, {
		/* sentinel */
	},
}
MODULE_DEVICE_TABLE(of, screen_share_driver_dt_match);

static const u32 screen_share_formats[] = {
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

#if defined(CONFIG_CHECK_DRM_TCC_VSYNC_TIME_GAP)
static unsigned int tcc_time_diff_ms(
	struct timeval *pre_timeval, struct timeval *current_timeval)
{
	unsigned int  diff_time_ms = 0;
	long diff_sec, diff_usec;

	if (current_timeval->tv_usec >= pre_timeval->tv_usec) {
		diff_sec = current_timeval->tv_sec - pre_timeval->tv_sec;
		diff_usec = current_timeval->tv_usec - pre_timeval->tv_usec;
		diff_usec = (diff_usec > 1000)?(diff_usec/1000):0;
		diff_time_ms = (1000 * diff_sec) + diff_usec;
	} else {
		diff_sec = current_timeval->tv_sec - pre_timeval->tv_sec - 1;
		diff_usec = current_timeval->tv_usec - pre_timeval->tv_usec;
		diff_usec += 1000000;
		diff_usec = (diff_usec > 1000)?(diff_usec/1000):0;
		diff_time_ms = (1000 * diff_sec) + diff_usec;
	}
	return diff_time_ms;
}
#endif

static irqreturn_t screen_share_irq_handler(int irq, void *dev_id)
{
	struct screen_share_context *sctx =
		(struct screen_share_context *)dev_id;
	#if defined(CONFIG_CHECK_DRM_TCC_VSYNC_TIME_GAP)
	unsigned int diff_ms;
	struct timeval cur_time;
	#endif

	/* check the crtc is detached already from encoder */
	if (!sctx->drm) {
		dev_warn(
			sctx->dev,
			"[WARN][%s] %s drm is not binded\r\n",
			LOG_TAG, __func__);
		goto out;
	}

	if (vioc_timer_is_interrupted(sctx->timer_id)) {
		tcc_drm_crtc_vblank_handler(&sctx->crtc->base);

		#if defined(CONFIG_CHECK_DRM_TCC_VSYNC_TIME_GAP)
		do_gettimeofday(&cur_time);
		diff_ms = tcc_time_diff_ms(&sctx->prev_time, &cur_time);
		if (
			diff_ms < (DIV_ROUND_UP(1000, sctx->vrefresh) - 1) ||
			diff_ms > (DIV_ROUND_UP(1000, sctx->vrefresh) + 1)) {
			pr_err(
				"DIFF %dms %d00us, <%d>",


				diff_ms, vioc_timer_get_curtime(),
				DIV_ROUND_UP(1000, sctx->vrefresh));
		}
		memcpy(&sctx->prev_time, &cur_time, sizeof(cur_time));
		#endif
		vioc_timer_clear_irq_status(sctx->timer_id);
	}
out:
	return IRQ_HANDLED;
}

static int screen_share_enable_vblank(struct tcc_drm_crtc *crtc)
{
	struct screen_share_context *sctx = crtc->context;
	unsigned long irqflags;
	int ret = 0;

	spin_lock_irqsave(&sctx->irq_lock, irqflags);
	if (!test_and_set_bit(CRTC_FLAGS_IRQ_BIT, &sctx->crtc_flags)) {
		ret = devm_request_irq(
			sctx->dev, sctx->irq_num,
			screen_share_irq_handler,
			IRQF_SHARED, sctx->data->name, sctx);
		if (ret < 0) {
			dev_err(
				sctx->dev,
				"[ERROR][%s] %s failed to request irq\r\n",
				LOG_TAG, __func__);
			goto err_irq_request;
		}
		#if defined(CONFIG_CHECK_DRM_TCC_VSYNC_TIME_GAP)
		do_gettimeofday(&sctx->prev_time);
		#endif
		vioc_timer_clear_irq_status(sctx->timer_id);
		vioc_timer_clear_irq_mask(sctx->timer_id);
		vioc_intr_enable(sctx->irq_num, VIOC_INTR_TIMER, 0);
		if (sctx->vrefresh >= 24 && sctx->vrefresh <= 120) {
			dev_info(
				sctx->dev,
				"[INFO][%s] %s with vrefresh(%d)\r\n",
				LOG_TAG, __func__, sctx->vrefresh);
			vioc_timer_set_timer(sctx->timer_id, 1, sctx->vrefresh);
		} else {
			dev_info(
				sctx->dev,
				"[INFO][%s] %s with force 30Hz\r\n",
				LOG_TAG, __func__);
			vioc_timer_set_timer(sctx->timer_id, 1, 30);
		}
	}
err_irq_request:
	spin_unlock_irqrestore(&sctx->irq_lock, irqflags);
	return ret;
}

static void screen_share_disable_vblank(struct tcc_drm_crtc *crtc)
{
	struct screen_share_context *sctx = crtc->context;
	unsigned long irqflags;

	spin_lock_irqsave(&sctx->irq_lock, irqflags);
	if (test_and_clear_bit(CRTC_FLAGS_IRQ_BIT, &sctx->crtc_flags)) {
		vioc_intr_disable(sctx->irq_num, VIOC_INTR_TIMER, 0);
		vioc_timer_set_irq_mask(sctx->timer_id);
		devm_free_irq(
			sctx->dev, sctx->irq_num, sctx);
	}
	spin_unlock_irqrestore(&sctx->irq_lock, irqflags);
}

static int screen_share_atomic_check(
	struct tcc_drm_crtc *crtc, struct drm_crtc_state *state)
{
	struct screen_share_context *sctx = crtc->context;
	struct drm_display_mode *mode = &state->adjusted_mode;

	if (mode->clock == 0) {
		dev_warn(
			sctx->dev,
			"[WARN][%s] %s Mode has zero clock value.\n",
			LOG_TAG, __func__);
		return -EINVAL;
	}

	sctx->vrefresh = drm_mode_vrefresh(mode);
	return 0;
}

static void screen_share_atomic_begin(struct tcc_drm_crtc *crtc)
{
	//struct screen_share_context *sctx = crtc->context;
}

static void screen_share_atomic_flush(struct tcc_drm_crtc *crtc)
{
	//struct screen_share_context *sctx = crtc->context;
	tcc_crtc_handle_event(crtc);
}

static void screen_share_update_plane(
	struct tcc_drm_crtc *crtc, struct tcc_drm_plane *plane)
{
	struct tcc_drm_plane_state *state =
				to_tcc_plane_state(plane->base.state);
	struct screen_share_context *sctx = crtc->context;
	struct drm_framebuffer *fb = state->base.fb;
	dma_addr_t dma_addr;
	unsigned int offset;
	unsigned int win = plane->win;
	unsigned int cpp = fb->format->cpp[0];
	unsigned int pitch = fb->pitches[0];
	unsigned int vioc_fmt;
	unsigned int vioc_swap = VIOC_SWAP_RGB;

	if (win >= sctx->hw_data.rdma_counts) {
		pr_err(
			"[ERROR][%s] %s win(%d) is out of range (%d)\r\n",
			LOG_TAG,
			__func__, win, sctx->hw_data.rdma_counts);
		return;
	}

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

	switch (fb->format->format) {
	case DRM_FORMAT_BGR565:
		vioc_swap = VIOC_SWAP_BGR;  /* B-G-R */
		vioc_fmt = TCC_LCDC_IMG_FMT_RGB565;
		break;
	case DRM_FORMAT_RGB565:
		vioc_fmt = TCC_LCDC_IMG_FMT_RGB565;
		break;
	case DRM_FORMAT_BGR888:
		vioc_swap = VIOC_SWAP_BGR;  /* B-G-R */
		vioc_fmt = TCC_LCDC_IMG_FMT_RGB888_3;
		break;
	case DRM_FORMAT_RGB888:
		vioc_fmt = TCC_LCDC_IMG_FMT_RGB888_3;
		break;
	case DRM_FORMAT_XBGR8888:
		vioc_swap = VIOC_SWAP_BGR;  /* B-G-R */
		vioc_fmt = TCC_LCDC_IMG_FMT_RGB888;
		break;
	case DRM_FORMAT_ABGR8888:
		vioc_swap = VIOC_SWAP_BGR;  /* B-G-R */
		vioc_fmt = TCC_LCDC_IMG_FMT_RGB888;
		break;
	case DRM_FORMAT_XRGB8888:
		vioc_fmt = TCC_LCDC_IMG_FMT_RGB888;
		break;
	case DRM_FORMAT_ARGB8888:
		vioc_fmt = TCC_LCDC_IMG_FMT_RGB888;
		break;
	case DRM_FORMAT_NV12:
		vioc_fmt = TCC_LCDC_IMG_FMT_YUV420ITL0;
		break;
	case DRM_FORMAT_NV21:
		vioc_fmt = TCC_LCDC_IMG_FMT_YUV420ITL1;
		break;
	case DRM_FORMAT_YVU420:
		vioc_fmt = TCC_LCDC_IMG_FMT_YUV420SP;
		break;
	case DRM_FORMAT_YUV420:
		vioc_fmt = TCC_LCDC_IMG_FMT_YUV420SP;
		break;
	default:
		vioc_fmt = TCC_LCDC_IMG_FMT_RGB888;
	}

	tcc_scrshare_set_sharedBuffer(
		lower_32_bits(dma_addr),
		state->crtc.w, state->crtc.h, vioc_fmt, vioc_swap);
}

static void screen_share_enable(struct tcc_drm_crtc *crtc)
{
	struct screen_share_context *sctx = crtc->context;

	if (!crtc->enabled) {
		dev_info(
			sctx->dev,
			"[INFO][%s] %s Turn on\r\n",
			LOG_TAG, __func__);
		#if defined(CONFIG_PM)
		pm_runtime_get_sync(sctx->dev);
		#endif
		crtc->enabled = 1;
	}
}

static void screen_share_disable(struct tcc_drm_crtc *crtc)
{
	struct screen_share_context *sctx = crtc->context;

	dev_info(
		sctx->dev, "[INFO][%s] %s Turn off\r\n",
		LOG_TAG, __func__);

	#if defined(CONFIG_PM)
	if (crtc->enabled)
		pm_runtime_put_sync(sctx->dev);
	#endif
}

static const struct tcc_drm_crtc_ops screen_share_crtc_ops = {
	.enable = screen_share_enable,
	.disable = screen_share_disable,
	.enable_vblank = screen_share_enable_vblank,
	.disable_vblank = screen_share_disable_vblank,
	.atomic_begin = screen_share_atomic_begin,
	.update_plane = screen_share_update_plane,
	.atomic_flush = screen_share_atomic_flush,
	.atomic_check = screen_share_atomic_check,
};

static int screen_share_parse(
	struct device *dev, struct screen_share_context *sctx)
{
	struct device_node *np, *current_node;
	int ret;


	if (!dev) {
		ret = -EINVAL;
		goto out;
	}

	np = dev->of_node;
	if (!np) {
		ret = -EINVAL;
		goto out;
	}

	current_node = of_parse_phandle(np, "timer_device", 0);
	if (!current_node) {
		dev_err(
			dev,
			"[ERROR][%s] %s could not find timer_device node\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
		goto out;
	}

	sctx->irq_num = irq_of_parse_and_map(current_node, 0);
	dev_info(
		dev,
		"[INFO][%s]  irq_num of timer device is %d\r\n",
		LOG_TAG, sctx->irq_num);
	return 0;
out:
	return ret;
}

static int screen_share_bind(
	struct device *dev, struct device *master, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *drm = dev_get_drvdata(master);
	struct screen_share_context *sctx;
	int ret;

	dev_info(
		dev,
		"[INFO][%s] %s %s start probe with pdev(%p), dev(%p)\r\n",
		LOG_TAG, __func__, dev->driver ? dev->driver->name : "Unknown",
		pdev, dev);
	if (!dev->of_node) {
		dev_err(
			dev,
			"[ERROR][%s] %s failed to get the device node\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
		goto out;
	}
	/* Create device context */
	sctx = devm_kzalloc(dev, sizeof(*sctx), GFP_KERNEL);
	if (!sctx) {
		ret = -ENOMEM;
		goto out;
	}
	sctx->dev = dev;
	sctx->data =
		(struct tcc_drm_device_data *)of_device_get_match_data(
			dev);
	if (!sctx->data) {
		dev_err(
			dev,
			"[ERROR][%s] %s failed to get match data\r\n",
			LOG_TAG, __func__);
		ret = -ENOMEM;
		goto out;
	}
	sctx->drm = drm;
	if (screen_share_parse(dev, sctx) < 0) {
		dev_err(
			dev,
			"[ERROR][%s] %s failed to parse device tree\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
		goto out;
	}
	sctx->data->name = kstrdup("screen_share", GFP_KERNEL);
	sctx->hw_data.vic = CONFIG_DRM_TCC_SCREEN_SHARE_VIC;
	sctx->hw_data.connector_type = DRM_MODE_CONNECTOR_VIRTUAL;
	sctx->hw_data.rdma_counts = 1;

	spin_lock_init(&sctx->irq_lock);
	platform_set_drvdata(pdev, sctx);
	sctx->encoder = tcc_dpi_probe(dev, &sctx->hw_data);
	if (IS_ERR(sctx->encoder)) {
		ret = PTR_ERR(sctx->encoder);
		goto out;
	}

	/* Activate CRTC clocks */
	ret = tcc_plane_init(
		drm, &sctx->plane.base, DRM_PLANE_TYPE_PRIMARY,
		screen_share_formats, ARRAY_SIZE(screen_share_formats));
	if (ret) {
		dev_err(
			dev,
			"[ERROR][%s] %s failed to initizliaed the planes\n",
			LOG_TAG, __func__);
		goto out;
	}
	sctx->plane.win = 0;

	sctx->crtc = tcc_drm_crtc_create(
		drm, &sctx->plane.base, NULL, sctx->data->output_type,
		&screen_share_crtc_ops, sctx);
	if (sctx->crtc == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	if (sctx->encoder)
		ret = tcc_dpi_bind(drm, sctx->encoder, &sctx->hw_data);

	if (ret)
		goto out;

	#if defined(CONFIG_PM)
	pm_runtime_enable(dev);
	#endif

	sctx->timer_id = VIOC_TIMER_TIMER0;

	return 0;
out:
	return ret;
}

static void screen_share_unbind(
	struct device *dev, struct device *master, void *data)
{
	struct screen_share_context *sctx = dev_get_drvdata(dev);

	if (sctx->encoder)
		tcc_dpi_remove(sctx->encoder);


	#if defined(CONFIG_PM)
	pm_runtime_disable(dev);
	#endif

	kfree(sctx->data->name);
	devm_kfree(dev, sctx);

	/* Deactivate CRTC clock */
}

static const struct component_ops screen_share_component_ops = {
	.bind	= screen_share_bind,
	.unbind = screen_share_unbind,
};

static int screen_share_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &screen_share_component_ops);
}

static int screen_share_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &screen_share_component_ops);
	return 0;
}

#ifdef CONFIG_PM
static int tcc_screen_share_suspend(struct device *dev)
{
	//struct screen_share_context *sctx = dev_get_drvdata(dev);
	vioc_timer_suspend();
	dev_info(dev, "[INFO][%s] %s \r\n", LOG_TAG, __func__);
	return 0;
}

static int tcc_screen_share_resume(struct device *dev)
{
	//struct screen_share_context *sctx = dev_get_drvdata(dev);
	vioc_timer_resume();
	dev_info(dev, "[INFO][%s] %s \r\n", LOG_TAG, __func__);
	return 0;
}
#endif

static const struct dev_pm_ops tcc_screen_share_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		tcc_screen_share_suspend, tcc_screen_share_resume)
};

struct platform_driver screen_share_driver = {
	.probe		= screen_share_probe,
	.remove		= screen_share_remove,
	.driver		= {
		.name	= "tcc-drm-screen-share",
		.owner	= THIS_MODULE,
		.pm	= &tcc_screen_share_pm_ops,
		.of_match_table = of_match_ptr(screen_share_driver_dt_match),
	},
};

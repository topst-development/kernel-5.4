// SPDX-License-Identifier: GPL-2.0-or-later

/* tcc_drm_crtc.c
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_edid.h>
#include <drm/tcc_drm.h>

#include <linux/clk.h>
#include <video/display_timing.h>
#include <video/videomode.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_config.h>

#include <tcc_drm_crtc.h>
#include <tcc_drm_dpi.h>
#include <tcc_drm_drv.h>
#include <tcc_drm_plane.h>
#include <tcc_drm_edid.h>

#define LOG_TAG "DRMCRTC"

/* Definitions for overlay priority of WMIX */
#define VIOC_WMIX_POR_OVP 5U
#define VIOC_WMIX_DRM_DEFAULT_OVP 24U

enum tcc_drm_crtc_flip_status {
	TCC_DRM_CRTC_FLIP_STATUS_NONE = 0,
	TCC_DRM_CRTC_FLIP_STATUS_PENDING,
	TCC_DRM_CRTC_FLIP_STATUS_DONE,
};

#define tcc_drm_crtc_dump_event(dev, in_event) \
do {									\
	struct drm_pending_vblank_event *vblank_event = (in_event);	\
	if (vblank_event != NULL) {					\
		switch (vblank_event->event.base.type) {		\
		case DRM_EVENT_VBLANK:					\
			dev_info(					\
				dev,					\
				"[INFO][%s] %s DRM_EVENT_VBLANK\r\n",	\
				LOG_TAG, __func__);			\
			break;						\
		case DRM_EVENT_FLIP_COMPLETE:				\
			dev_info(					\
				dev,					\
				"[INFO][%s] %s DRM_EVENT_FLIP_COMPLETE\r\n", \
				LOG_TAG, __func__);			\
			break;						\
		}							\
	}								\
} while (0)								\


static void
tcc_drm_crtc_atomic_enable(struct drm_crtc *crtc,
			   struct drm_crtc_state *old_state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
	bool active;

	/*
	 * Fix: misra_c_2012_rule_8_13_violation: The pointer variable
	 * old_state points to a non-constant type but does not modify the
	 * object it points to. Consider adding const qualifier to the
	 * points-to type.
	 */
	active = old_state->active;
	old_state->active = active;

	if (tcc_crtc->ops->enable != NULL) {
		tcc_crtc->ops->enable(tcc_crtc);
	}

	drm_crtc_vblank_on(crtc);

	if (crtc->state->event != NULL) {
		unsigned long flags;

		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		WARN_ON(drm_crtc_vblank_get(crtc) != 0);

		/* coverity[cert_dcl37_c_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		tcc_crtc->flip_event = crtc->state->event;
		crtc->state->event = NULL;
		//tcc_drm_crtc_dump_event(tcc_crtc->flip_event);

		atomic_set(&tcc_crtc->flip_status,
			   (int)TCC_DRM_CRTC_FLIP_STATUS_DONE);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	}
}

static void tcc_drm_crtc_atomic_disable(struct drm_crtc *crtc,
					   struct drm_crtc_state *old_state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
	bool active;

	/*
	 * Fix: misra_c_2012_rule_8_13_violation: The pointer variable
	 * old_state points to a non-constant type but does not modify the
	 * object it points to. Consider adding const qualifier to the
	 * points-to type.
	 */
	active = old_state->active;
	old_state->active = active;

	if (tcc_crtc->ops->disable != NULL) {
		tcc_crtc->ops->disable(tcc_crtc);
	}

	drm_crtc_vblank_off(crtc);

	if (crtc->state->event != NULL) {
		unsigned long flags;

		/* coverity[cert_dcl37_c_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
		crtc->state->event = NULL;
	}
}

static int tcc_crtc_atomic_check(struct drm_crtc *crtc,
				     struct drm_crtc_state *state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
	int ret = 0;

	if (state->enable &&
		(tcc_crtc->ops->atomic_check != NULL)) {
		ret = tcc_crtc->ops->atomic_check(tcc_crtc, state);
	}

	return 0;
}

static void tcc_crtc_atomic_begin(struct drm_crtc *crtc,
				     struct drm_crtc_state *old_crtc_state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
	bool active;

	/*
	 * Fix: misra_c_2012_rule_8_13_violation: The pointer variable
	 * old_state points to a non-constant type but does not modify the
	 * object it points to. Consider adding const qualifier to the
	 * points-to type.
	 */
	active = old_crtc_state->active;
	old_crtc_state->active = active;

	if (tcc_crtc->ops->atomic_begin != NULL) {
		tcc_crtc->ops->atomic_begin(tcc_crtc);
	}
}

static void tcc_crtc_atomic_flush(struct drm_crtc *crtc,
				     struct drm_crtc_state *old_crtc_state)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
	bool active;
	int ret = 0;

	/*
	 * Fix: misra_c_2012_rule_8_13_violation: The pointer variable
	 * old_state points to a non-constant type but does not modify the
	 * object it points to. Consider adding const qualifier to the
	 * points-to type.
	 */
	active = old_crtc_state->active;
	old_crtc_state->active = active;

	if (!old_crtc_state->active) {
		ret = -EINVAL;
	}

	if ((ret == 0) && (tcc_crtc->ops->atomic_flush != NULL)) {
		tcc_crtc->ops->atomic_flush(tcc_crtc);
	}
}

static enum drm_mode_status tcc_crtc_mode_valid(struct drm_crtc *crtc,
	const struct drm_display_mode *mode)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
	enum drm_mode_status status = MODE_OK;

	if (tcc_crtc->ops->mode_valid != NULL) {
		status = tcc_crtc->ops->mode_valid(tcc_crtc, mode);
	}

	return status;
}

static void tcc_crtc_set_nofb(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->mode_set_nofb != NULL) {
		tcc_crtc->ops->mode_set_nofb(tcc_crtc);
	}
}

static const struct drm_crtc_helper_funcs tcc_crtc_helper_funcs = {
	.mode_valid	= tcc_crtc_mode_valid,
	.mode_set_nofb = tcc_crtc_set_nofb,
	.atomic_check	= tcc_crtc_atomic_check,
	.atomic_begin	= tcc_crtc_atomic_begin,
	.atomic_flush	= tcc_crtc_atomic_flush,
	.atomic_enable	= tcc_drm_crtc_atomic_enable,
	.atomic_disable	= tcc_drm_crtc_atomic_disable,
};

static void tcc_drm_crtc_flip_complete(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
	unsigned long flags;

	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	spin_lock_irqsave(&crtc->dev->event_lock, flags);

	atomic_set(&tcc_crtc->flip_status, (int)TCC_DRM_CRTC_FLIP_STATUS_NONE);
	tcc_crtc->flip_async = (bool)false;

	if (tcc_crtc->flip_event != NULL) {
		drm_crtc_send_vblank_event(crtc, tcc_crtc->flip_event);
		tcc_crtc->flip_event = NULL;
	}

	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

void tcc_crtc_handle_event(struct tcc_drm_crtc *tcc_crtc)
{
	unsigned long flags;
	struct drm_crtc *crtc = &tcc_crtc->base;
	const struct drm_crtc_state *new_crtc_state = crtc->state;
	const struct drm_pending_vblank_event *event = new_crtc_state->event;
	bool finish = (bool)false;

	if (!new_crtc_state->active) {
		finish = (bool)true;
	}

	if (!finish &&
	    (event == NULL)) {
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		dev_dbg(crtc->dev->dev,
			"[DEBUG][%s] %s event is NULL\r\n", LOG_TAG, __func__);
		finish = (bool)true;
	}

	if (!finish) {
		#if defined(CONFIG_REFCODE_PRE_K54)
		tcc_crtc->flip_async =
			!!(new_crtc_state->pageflip_flags &
			   DRM_MODE_PAGE_FLIP_ASYNC);
		#else
		tcc_crtc->flip_async = new_crtc_state->async_flip;
		#endif

		if (tcc_crtc->flip_async) {
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			WARN_ON(drm_crtc_vblank_get(crtc) != 0);
		}

		/* coverity[cert_dcl37_c_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		tcc_crtc->flip_event = crtc->state->event;
		crtc->state->event = NULL;
		atomic_set(&tcc_crtc->flip_status,
			   (int)TCC_DRM_CRTC_FLIP_STATUS_DONE);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
		if (tcc_crtc->flip_async) {
			tcc_drm_crtc_flip_complete(crtc);
		}
	}
}

static void tcc_drm_crtc_destroy(struct drm_crtc *crtc)
{
	const struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	drm_crtc_cleanup(crtc);
	kfree(tcc_crtc);
}

static int tcc_drm_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);
	int ret = 0;

	if (tcc_crtc->ops->enable_vblank != NULL) {
		ret = tcc_crtc->ops->enable_vblank(tcc_crtc);
	}

	return ret;
}

static void tcc_drm_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(crtc);

	if (tcc_crtc->ops->disable_vblank != NULL) {
		tcc_crtc->ops->disable_vblank(tcc_crtc);
	}
}

static const struct drm_crtc_funcs tcc_crtc_funcs = {
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.destroy = tcc_drm_crtc_destroy,
	.reset = drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
	.enable_vblank = tcc_drm_crtc_enable_vblank,
	.disable_vblank = tcc_drm_crtc_disable_vblank,
};

static bool tcc_drm_crtc_check_pixelclock_match(unsigned long res,
			unsigned long data, unsigned int clk_div)
{
	unsigned long bp;
	unsigned long le, gt;

	bool match = (bool)false;

	if (clk_div > 0U) {
		clk_div <<= 1U;

		if ((ULONG_MAX / (unsigned long)clk_div) <= data) {
			data *= (unsigned long)clk_div;
		}
	}

	if ((ULONG_MAX - 10UL) >= data) {
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		bp = (unsigned long)DIV_ROUND_UP(data, 10UL);
	} else {
		bp = 0;
	}

	le = (data > bp) ? (data - bp) : 0UL;
	gt = ((ULONG_MAX - bp) > data) ? (data + bp) : ULONG_MAX;

	if ((res > le) && (res < gt)) {
		match = (bool)true;
	}
	return match;
}

#if defined(CONFIG_ARCH_TCC805X)
static  u32 tcc_drm_crtc_calc_vactive(const struct drm_display_mode *mode)
{
	u32 vactive = 0;
	u32 cflags;

	if (mode != NULL) {
		vactive = (mode->vdisplay >= 0) ? ((u32)mode->vdisplay) : 0U;
		#if defined(CONFIG_DRM_TCC_SUPPORT_3D)
		cflags = DRM_MODE_FLAG_3D_FRAME_PACKING;;
		if (((u32)mode->flags & (u32)cflags) == (u32)cflags) {
			u32 vblank = mode->vtotal - mode->vdisplay;

			cflags = DRM_MODE_FLAG_INTERLACE;
			if (((u32)mode->flags & cflags) == cflags) {
				vactive = (vactive << 2U) + (3U * vblank + 2U);
			} else {
				vactive = (vactive << 1U) + vblank;
			}
		} else {
		#endif
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			cflags = DRM_MODE_FLAG_INTERLACE;
			if (((u32)mode->flags & cflags) == cflags) {
				vactive <<= 1U;
			}
		#if defined(CONFIG_DRM_TCC_SUPPORT_3D)
		}
		#endif
	}
	return vactive;
}

static void tcc_drm_crtc_force_disable(struct drm_crtc *crtc,
				       const struct tcc_hw_device *hw_data)
{
	struct drm_encoder *encoder;
	bool find_crtc = (bool)false;

	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	drm_for_each_encoder(encoder, crtc->dev)
		if (encoder->crtc == crtc) {
			find_crtc = (bool)true;
			break;
		}

	if (find_crtc) {
		dev_info(
			crtc->dev->dev,
			"[INFO][%s] %s for display device(%d)\r\n",
			LOG_TAG, __func__,
			get_vioc_index(hw_data->display_device.blk_num));

		if (encoder->helper_private != NULL) {
			const struct drm_encoder_helper_funcs *funcs;

			/* Disable panels if exist */
			funcs = encoder->helper_private;
			if (funcs->disable != NULL) {
				funcs->disable(encoder);
			}
		}
		if (crtc->helper_private != NULL) {
			const struct drm_crtc_helper_funcs *funcs;
			struct drm_crtc_state old_crtc_state;

			funcs = crtc->helper_private;
			if (funcs->atomic_disable != NULL) {
				/*
				* This valud indicates that atomic_disable is called
				* by tccdrm not by drm core.
				* Note: This value cannot be 0 in normal cases.
				*/
				old_crtc_state.active = (bool)0;
				funcs->atomic_disable(crtc, &old_crtc_state);
			}
		}
	}
}

static bool tcc_drm_crtc_check_reset_display_controller(struct drm_crtc *crtc,
				     const struct drm_display_mode *mode,
				     const struct tcc_hw_device *hw_data)
{
	struct DisplayBlock_Info ddinfo = {0U, };
	bool need_reset = (bool)false;
	bool need_turnoff = (bool)false;
	u32 vactive;

	VIOC_DISP_GetDisplayBlock_Info(
		hw_data->display_device.virt_addr, &ddinfo);
	/* Check turn on status of display device */
	if (ddinfo.enable == 0U) {
		dev_info(
			crtc->dev->dev,
			"[INFO][%s] %s display device(%d) is disabled\r\n",
			LOG_TAG, __func__,
			get_vioc_index(hw_data->display_device.blk_num));
		need_reset = (bool)true;
	}

	/* Check width and height */
	if (!need_reset) {
		unsigned int hdisplay =
			(mode->hdisplay < 0) ? 0U :
			(unsigned int)mode->hdisplay;

		vactive = tcc_drm_crtc_calc_vactive(mode);

		if ((ddinfo.width != hdisplay) ||
		    (ddinfo.height != vactive)) {
			dev_info(
				crtc->dev->dev,
				"[INFO][%s] %s display device(%d) size is not match %dx%d : %dx%d\r\n",
				LOG_TAG, __func__,
				get_vioc_index(hw_data->display_device.blk_num),
				ddinfo.width, ddinfo.height, mode->hdisplay,
				vactive);
			need_reset = (bool)true;
			need_turnoff = (bool)true;
		}
	}

	/* Check pixel clock */
	if (!need_reset) {
		unsigned long clocks =
				((mode->clock < 0) ? 0UL :
				(unsigned long)mode->clock);
		if (
			!tcc_drm_crtc_check_pixelclock_match(
				clk_get_rate(
					hw_data->ddc_clock),
					clocks * 1000UL,
					vioc_disp_get_clkdiv(
					hw_data->display_device.virt_addr))) {
			dev_info(
				crtc->dev->dev,
				"[INFO][%s] %s display device(%d) clock is not match %ldHz : %dHz\r\n",
				LOG_TAG, __func__,
				get_vioc_index(hw_data->display_device.blk_num),
				clk_get_rate(hw_data->ddc_clock),
				mode->clock * 1000);
			need_reset = (bool)true;
			need_turnoff = (bool)true;
		}
	}

	if (need_turnoff) {
		tcc_drm_crtc_force_disable(
			crtc,
			(const struct tcc_hw_device *)hw_data);
	}

	return need_reset;
}
#endif

struct tcc_drm_crtc *tcc_drm_crtc_create(struct drm_device *drm_dev,
					struct drm_plane *primary,
					struct drm_plane *cursor,
					enum tcc_drm_output_type type,
					const struct tcc_drm_crtc_ops *ops,
					void *context)
{
	struct tcc_drm_crtc *tcc_crtc;
	struct drm_crtc *crtc;
	int ret = 0;

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	tcc_crtc = (struct tcc_drm_crtc *)kzalloc(sizeof(*tcc_crtc),
						  GFP_KERNEL);
	if (tcc_crtc == NULL) {
		ret = -ENOMEM;
	}

	if (ret == 0) {
		tcc_crtc->type = type;
		tcc_crtc->ops = ops;
		tcc_crtc->context = context;

		crtc = &tcc_crtc->base;

		atomic_set(&tcc_crtc->flip_status,
			   (int)TCC_DRM_CRTC_FLIP_STATUS_NONE);

		ret = drm_crtc_init_with_planes(drm_dev, crtc, primary, cursor,
						&tcc_crtc_funcs, NULL);
	}

	if (ret == 0) {
		drm_crtc_helper_add(crtc, &tcc_crtc_helper_funcs);
	} else {
		/* cleans */
		if ((primary != NULL) &&
		    (primary->funcs->destroy != NULL)) {
			primary->funcs->destroy(primary);
		}
		if ((cursor != NULL) &&
		    (cursor->funcs->destroy != NULL)) {
			cursor->funcs->destroy(cursor);
		}
		if (tcc_crtc != NULL) {
			kfree(tcc_crtc);
		}
		tcc_crtc = NULL;
	}

	return tcc_crtc;
}


static struct tcc_drm_crtc *tcc_drm_crtc_get_by_type(const struct drm_device *drm_dev,
				       enum tcc_drm_output_type out_type)
{
	struct tcc_drm_crtc *find_crtc = NULL;
	struct drm_crtc *crtc;

	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	drm_for_each_crtc(crtc, drm_dev)
		if (to_tcc_crtc(crtc)->type == out_type) {
			find_crtc = to_tcc_crtc(crtc);
			break;
		}

	return find_crtc;
}

int tcc_drm_set_possible_crtcs(struct drm_encoder *encoder,
		enum tcc_drm_output_type out_type)
{
	int ret = 0;
	const struct tcc_drm_crtc *crtc =
		(const struct tcc_drm_crtc *)tcc_drm_crtc_get_by_type(
			(const struct drm_device *)encoder->dev, out_type);

	if (crtc == NULL) {
		ret = -EPERM;
	}

	if (ret == 0) {
		encoder->possible_crtcs = drm_crtc_mask(&crtc->base);
	}

	return ret;
}

void tcc_drm_crtc_vblank_handler(struct drm_crtc *crtc)
{
	const struct tcc_drm_crtc *tcc_crtc =
		(const struct tcc_drm_crtc *)to_tcc_crtc(crtc);
	enum tcc_drm_crtc_flip_status status;

	(void)drm_handle_vblank(crtc->dev, drm_crtc_index(crtc));

	status = (enum tcc_drm_crtc_flip_status)atomic_read(
						&tcc_crtc->flip_status);
	if (status == TCC_DRM_CRTC_FLIP_STATUS_DONE) {
		if (!tcc_crtc->flip_async) {
			tcc_drm_crtc_flip_complete(crtc);
		}
	}
}

int tcc_crtc_parse_edid_ioctl(
	struct drm_device *dev, void *data, struct drm_file *infile)
{
	struct drm_crtc *crtc;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct drm_tcc_edid *args = (void __user *)data;
	const struct drm_property_blob *edid_blob = NULL;
	const struct drm_connector *connector;
	struct edid *edid_ptr = NULL;
	int ret = 0;
	bool internal_ok = (bool)true;

	if (dev == NULL) {
		ret = -EINVAL;
		internal_ok = (bool)false;
	}

	if ((ret == 0) &&
	    (dev->dev == NULL)) {
		ret = -EINVAL;
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		dev_info(dev->dev,
			 "[INFO][%s] %s Ioctl called\n", LOG_TAG, __func__);

		/* get crtc */
		#if defined(CONFIG_REFCODE_PRE_K54)
		crtc = drm_crtc_find(dev, args->crtc_id);
		#else
		crtc = drm_crtc_find(dev, infile, args->crtc_id);
		#endif
		if (crtc == NULL) {
			dev_err(
				dev->dev,
				"[ERR][DRMCRTC] %sInvalid crtc ID \r\n",
				__func__);
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		dev_info(
			dev->dev, "[INFO][%s] %s crtc_id : [%d]\n", LOG_TAG, __func__,
			args->crtc_id);

		connector =
			tcc_dpi_find_connector_from_crtc(
				(const struct drm_crtc *)crtc);
		if (connector != NULL) {
			edid_blob = connector->edid_blob_ptr;
		}

		if (edid_blob != NULL) {
			(void)memcpy(args->data, edid_blob->data, edid_blob->length);
		} else {
			/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
			/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
			edid_ptr = devm_kzalloc(dev->dev, EDID_LENGTH,
						GFP_KERNEL);

			if (edid_ptr == NULL) {
				ret = -ENOMEM;
				internal_ok = (bool)false;
			}

			if (internal_ok) {
				struct drm_crtc_state *crtc_state = crtc->state;
				const struct drm_display_mode *d_mode =
					(const struct drm_display_mode *)&crtc_state->mode;
				if (
					tcc_make_edid_from_display_mode(
						edid_ptr, d_mode) < 0) {
					ret = -EINVAL;
					internal_ok = (bool)false;
				}
			}

			if (internal_ok) {
				(void)memcpy(args->data, edid_ptr, EDID_LENGTH);
				devm_kfree(dev->dev, edid_ptr);
			}
		}
	}
	if (edid_ptr != NULL) {
		devm_kfree(dev->dev, edid_ptr);
		edid_ptr = NULL;
	}
	return ret;
}

int tcc_drm_crtc_set_display_timing(struct drm_crtc *crtc,
				     const struct drm_display_mode *mode,
				     const struct tcc_hw_device *hw_data)
{
	#if defined(CONFIG_ARCH_TCC805X)
	stLTIMING stTimingParam;
	stLCDCTR stCtrlParam;
	bool interlace;
	u32 tmp_sync;
	u32 vactive;
	u32 ovp;
	#endif
	unsigned long clk_rate;
	struct videomode vm;
	int ret = 0;

	if (crtc == NULL) {
		ret = -EINVAL;
	}
	if ((ret == 0) &&
	    (crtc->dev == NULL)) {
		ret = -EINVAL;
	}
	if ((ret == 0) &&
	    (crtc->dev->dev == NULL)) {
		ret = -EINVAL;
	}
	if (mode == NULL) {
		ret = -EINVAL;
	}
	if (hw_data == NULL) {
		ret = -EINVAL;
	}

	if (ret == 0) {
		#if defined(CONFIG_ARCH_TCC805X)
		bool need_reset;
		#endif

		drm_display_mode_to_videomode(mode, &vm);
		#if defined(CONFIG_ARCH_TCC805X)
		need_reset =
			tcc_drm_crtc_check_reset_display_controller(crtc,
								    mode,
								    hw_data);

		if (need_reset) {
			/* Display MUX */
			(void)VIOC_CONFIG_LCDPath_Select(
				get_vioc_index(
					hw_data->display_device.blk_num),
					hw_data->lcdc_mux);
			dev_info(
				crtc->dev->dev,
				"[INFO][%s] %s display device(%d) to connect mux(%d)\r\n",
				LOG_TAG, __func__,
				get_vioc_index(hw_data->display_device.blk_num),
				hw_data->lcdc_mux);

			(void)memset(&stCtrlParam, 0, sizeof(stLCDCTR));
			(void)memset(&stTimingParam, 0, sizeof(stLTIMING));
			interlace = (((u32)vm.flags & (u32)DISPLAY_FLAGS_INTERLACED) ==
					(u32)DISPLAY_FLAGS_INTERLACED) ?
					(bool)true : (bool)false;
			stCtrlParam.iv = (((u32)vm.flags & (u32)DISPLAY_FLAGS_VSYNC_LOW) ==
					(u32)DISPLAY_FLAGS_VSYNC_LOW) ? 1U : 0U;
			stCtrlParam.ih = (((u32)vm.flags & (u32)DISPLAY_FLAGS_HSYNC_LOW) ==
					(u32)DISPLAY_FLAGS_HSYNC_LOW) ? 1U : 0U;
			stCtrlParam.dp = (((u32)vm.flags & (u32)DISPLAY_FLAGS_DOUBLECLK) ==
					(u32)DISPLAY_FLAGS_DOUBLECLK) ? 1U : 0U;

			if (interlace) {
				stCtrlParam.tv = 1U;
				stCtrlParam.advi = 1U;
			} else {
				stCtrlParam.ni = 1U;
			}

			/* Calc vactive */
			vactive = tcc_drm_crtc_calc_vactive(mode);

			stTimingParam.lpc = vm.hactive;
			stTimingParam.lewc = vm.hfront_porch;
			stTimingParam.lpw = (vm.hsync_len > 0U) ? (vm.hsync_len - 1U) : 0U;
			stTimingParam.lswc = vm.hback_porch;

			if (interlace) {
				tmp_sync = vm.vsync_len << 1;
				stTimingParam.fpw = (tmp_sync > 0U) ? (tmp_sync - 1U) : 0U;
				tmp_sync = vm.vback_porch << 1;
				stTimingParam.fswc = (tmp_sync > 0U) ? (tmp_sync - 1U) : 0U;
				stTimingParam.fewc = vm.vfront_porch << 1U;
				stTimingParam.fswc2 = stTimingParam.fswc + 1U;
				stTimingParam.fewc2 = (stTimingParam.fewc > 0U) ?
						(stTimingParam.fewc - 1U) : 0U;
				if (
					(mode->vtotal == (int)1250) &&
					(vm.hactive == 1920U) &&
					(vm.vactive == 540U)) {
					/* VIC 1920x1080@50i 1250 vtotal */
					stTimingParam.fewc -= 2U;
					}
			} else {
				stTimingParam.fpw = (vm.vsync_len > 0U) ?
						(vm.vsync_len - 1U) : 0U;
				stTimingParam.fswc = (vm.vback_porch > 0U) ?
						(vm.vback_porch - 1U) : 0U;
				stTimingParam.fewc = (vm.vfront_porch > 0U) ?
						(vm.vfront_porch - 1U) : 0U;
				stTimingParam.fswc2 = stTimingParam.fswc;
				stTimingParam.fewc2 = stTimingParam.fewc;
			}

			// Check 3D Frame Packing
			#if defined(CONFIG_DRM_TCC_SUPPORT_3D)
			if (mode->flags & DRM_MODE_FLAG_3D_FRAME_PACKING) {
				stTimingParam.framepacking = 1U;
			} else if (mode->flags & DRM_MODE_FLAG_3D_SIDE_BY_SIDE_HALF) {
				stTimingParam.framepacking = 2U;
			} else if (mode->flags & DRM_MODE_FLAG_3D_TOP_AND_BOTTOM) {
				stTimingParam.framepacking = 3U;
			}
			#endif

			/* Common Timing Parameters */
			stTimingParam.flc = (vactive > 0U) ? (vactive - 1U) : 0U;
			stTimingParam.fpw2 = stTimingParam.fpw;
			stTimingParam.flc2 = stTimingParam.flc;

			/* swreset display device */
			VIOC_CONFIG_SWReset(
				hw_data->display_device.blk_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(
				hw_data->display_device.blk_num, VIOC_CONFIG_CLEAR);

			VIOC_WMIX_GetOverlayPriority(hw_data->wmixer.virt_addr, &ovp);
			/* Restore ovp value of WMIX to drm default ovp value */
			if (ovp == VIOC_WMIX_POR_OVP) {
				ovp = VIOC_WMIX_DRM_DEFAULT_OVP;
			}
			VIOC_CONFIG_SWReset(hw_data->wmixer.blk_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(hw_data->wmixer.blk_num, VIOC_CONFIG_CLEAR);

			//vioc_reset_rdma_on_display_path(pDisplayInfo->DispNum);

			VIOC_WMIX_SetOverlayPriority(hw_data->wmixer.virt_addr, ovp);
			VIOC_WMIX_SetSize(hw_data->wmixer.virt_addr, vm.hactive, vactive);
			VIOC_WMIX_SetUpdate(hw_data->wmixer.virt_addr);

			VIOC_DISP_SetTimingParam(
				hw_data->display_device.virt_addr, &stTimingParam);
			VIOC_DISP_SetControlConfigure(
				hw_data->display_device.virt_addr, &stCtrlParam);

			/* PXDW
			* YCC420 with stb pxdw is 27
			* YCC422 with stb is pxdw 21, with out stb is 8
			* YCC444 && RGB with stb is 23, with out stb is 12
			* TCCDRM can only support RGB as format of the display device.
			*/
			VIOC_DISP_SetPXDW(hw_data->display_device.virt_addr, 12);

			VIOC_DISP_SetSize(
				hw_data->display_device.virt_addr, vm.hactive, vactive);
			VIOC_DISP_SetBGColor(hw_data->display_device.virt_addr, 0, 0, 0, 0);
		}
		#endif

		clk_rate = clk_get_rate(hw_data->ddc_clock);
		/* Set pixel clocks */
		if (
			!tcc_drm_crtc_check_pixelclock_match(
				clk_rate, vm.pixelclock,
				vioc_disp_get_clkdiv(
					hw_data->display_device.virt_addr))) {
			dev_info(
				crtc->dev->dev,
				"[INFO][%s] %s display device(%d) clock is not match %ldHz : %dHz\r\n",
				LOG_TAG, __func__,
				get_vioc_index(hw_data->display_device.blk_num),
				clk_rate, mode->clock * 1000);
			(void)clk_set_rate(hw_data->ddc_clock, vm.pixelclock);
		}
	}

	return ret;
}

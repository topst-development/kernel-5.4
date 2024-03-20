// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) 2016 Telechips Inc.
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <drm/drmP.h>
#include <linux/tcc_math.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/tcc_drm.h>
#if defined(CONFIG_VIOC_PVRIC_FBDC)
#include <pvrsrvkm/img_drm_fourcc_internal.h>
#endif
#include "tcc_drm_drv.h"
#include "tcc_drm_crtc.h"
#include "tcc_drm_fb.h"
#include "tcc_drm_gem.h"
#include "tcc_drm_plane.h"
#define LOG_TAG "DRMPANEL"

#if defined(CONFIG_DRM_TCC_USES_PLANE_GET_SIZE)
/*
 * This function is to get X or Y size shown via screen. This needs length and
 * start position of CRTC.
 *
 *      <--- length --->
 * CRTC ----------------
 *      ^ start        ^ end
 *
 * There are six cases from a to f.
 *
 *             <----- SCREEN ----->
 *             0                 last
 *   ----------|------------------|----------
 * CRTCs
 * a -------
 *        b -------
 *        c --------------------------
 *                 d --------
 *                           e -------
 *                                  f -------
 */
static int tcc_plane_get_size(
	int start, unsigned int length, unsigned int last)
{
	int end = start + length;
	int size = 0;

	if (start <= 0) {
		if (end > 0) {
			size = min_t(unsigned int, end, last);
		}
	} else if (start <= last) {
		size = min_t(unsigned int, last - start, length);
	}

	return size;
}
#endif

static void tcc_plane_mode_set(struct tcc_drm_plane_state *tcc_state)
{
	const struct drm_plane_state *state =
		(const struct drm_plane_state *)&tcc_state->base;

	int crtc_x, crtc_y, itmp;
	unsigned int crtc_w, crtc_h, utmp;
	unsigned int src_x, src_y, src_w, src_h;

	/*
	 * The original src/dest coordinates are stored in tcc_state->base,
	 * but we want to keep another copy internal to our driver that we can
	 * clip/modify ourselves.
	 */
	crtc_x = state->crtc_x;
	crtc_y = state->crtc_y;
	crtc_w = state->crtc_w;
	crtc_h = state->crtc_h;

	/* Source parameters given in 16.16 fixed point, ignore fractional. */
	src_x = state->src_x >> 16;
	src_y = state->src_y >> 16;
	src_w = state->src_w >> 16;
	src_h = state->src_h >> 16;

	//pr_info("Before: src: %d, %d, %dx%d dst: %d, %d, %dx%d\r\n",
	//		src_x, src_y, src_w, src_h,
	//		crtc_x, crtc_y, crtc_w, crtc_h);

	if (crtc_x < 0) {
		//src_x += -crtc_x;
		itmp = -crtc_x;
		utmp = tcc_safe_int2uint(itmp);
		src_x = tcc_safe_uint_pluse(src_x, utmp);

		//crtc_w += crtc_x;
		crtc_w = tcc_safe_uint_minus(crtc_w, utmp);
		crtc_x = 0;
	}

	if (crtc_y < 0) {
		//src_y += -crtc_y;
		itmp = -crtc_y;
		utmp = tcc_safe_int2uint(itmp);
		src_y = tcc_safe_uint_pluse(src_y, utmp);

		//crtc_h += crtc_y;
		crtc_h = tcc_safe_uint_minus(crtc_h, utmp);
		crtc_y = 0;
	}

	/* set drm framebuffer data. */
	tcc_state->src.x = src_x;
	tcc_state->src.y = src_y;
	tcc_state->src.w = src_w;
	tcc_state->src.h = src_h;

	/* set plane range to be displayed. */
	tcc_state->crtc.x = tcc_safe_int2uint(crtc_x);
	tcc_state->crtc.y = tcc_safe_int2uint(crtc_y);
	tcc_state->crtc.w = crtc_w;
	tcc_state->crtc.h = crtc_h;

	//pr_info("Fixed: src: %d, %d, %dx%d dst: %d, %d, %dx%d\r\n",
	//		tcc_state->src.x, tcc_state->src.y,
	//		tcc_state->src.w, tcc_state->src.h,
	//		tcc_state->crtc.x, tcc_state->crtc.y,
	//		tcc_state->crtc.w, tcc_state->crtc.h);
}

#if defined(CONFIG_DRM_TCC_USES_PLANE_DUMP)
static void tcc_plane_mode_set_dump(struct tcc_drm_plane_state *tcc_state)
{
	struct drm_plane_state *state = &tcc_state->base;

	int crtc_x, crtc_y;
	unsigned int crtc_w, crtc_h;
	unsigned int src_x, src_y, src_w, src_h;

	/*
	 * The original src/dest coordinates are stored in tcc_state->base,
	 * but we want to keep another copy internal to our driver that we can
	 * clip/modify ourselves.
	 */
	crtc_x = state->crtc_x;
	crtc_y = state->crtc_y;
	crtc_w = state->crtc_w;
	crtc_h = state->crtc_h;

	/* Source parameters given in 16.16 fixed point, ignore fractional. */
	src_x = state->src_x >> 16;
	src_y = state->src_y >> 16;
	src_w = state->src_w >> 16;
	src_h = state->src_h >> 16;

	pr_info("Before: src: %d, %d, %dx%d dst: %d, %d, %dx%d\r\n",
			src_x, src_y, src_w, src_h,
			crtc_x, crtc_y, crtc_w, crtc_h);

	if (crtc_x < 0) {
		src_x += -crtc_x;
		crtc_w += crtc_x;
		crtc_x = 0;
	}

	if (crtc_y < 0) {
		src_y += -crtc_y;
		crtc_h += crtc_y;
		crtc_y = 0;
	}

	/* set drm framebuffer data. */
	tcc_state->src.x = src_x;
	tcc_state->src.y = src_y;
	tcc_state->src.w = src_w;
	tcc_state->src.h = src_h;

	/* set plane range to be displayed. */
	tcc_state->crtc.x = crtc_x;
	tcc_state->crtc.y = crtc_y;
	tcc_state->crtc.w = crtc_w;
	tcc_state->crtc.h = crtc_h;

	pr_info("Fixed: src: %d, %d, %dx%d dst: %d, %d, %dx%d\r\n",
			tcc_state->src.x, tcc_state->src.y,
			tcc_state->src.w, tcc_state->src.h,
			tcc_state->crtc.x, tcc_state->crtc.y,
			tcc_state->crtc.w, tcc_state->crtc.h);
}
#endif

static void tcc_drm_plane_reset(struct drm_plane *plane)
{
	struct tcc_drm_plane_state *tcc_state;

	if (plane->state != NULL) {
		tcc_state = to_tcc_plane_state(plane->state);
		__drm_atomic_helper_plane_destroy_state(plane->state);
		kfree(tcc_state);
	}

	/* DP no.13 */
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	tcc_state = kzalloc(sizeof(*tcc_state), GFP_KERNEL);
	#if defined(CONFIG_REFCODE_PRE_K54)
	if (tcc_state != NULL) {
		plane->state = &tcc_state->base;
		plane->state->plane = plane;
	}
	#else
	if (tcc_state != NULL) {
		__drm_atomic_helper_plane_reset(plane, &tcc_state->base);
	}
	#endif
}

static struct drm_plane_state *
tcc_drm_plane_duplicate_state(struct drm_plane *plane)
{
	struct tcc_drm_plane_state *duplicate_state;
	struct drm_plane_state *ptr = NULL;

	/* DP no.13 */
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	duplicate_state = kzalloc(
		sizeof(struct tcc_drm_plane_state), GFP_KERNEL);
	if (duplicate_state != NULL) {
		__drm_atomic_helper_plane_duplicate_state(
					plane, &duplicate_state->base);
		ptr = &duplicate_state->base;
	}
	return ptr;
}


/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void tcc_drm_plane_destroy_state(struct drm_plane *plane,
					   struct drm_plane_state *old_state)
{
	const struct tcc_drm_plane_state *old_tcc_state =
					/* DP no.10 */
					/* coverity[cert_arr39_c_violation : FALSE] */
					/* coverity[cert_dcl37_c_violation : FALSE] */
					/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */ //
					/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */ //
					/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
					/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
					/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
					/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
					/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
					/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
					/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
					/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
					to_tcc_plane_state(old_state);


	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter plane is not used in
	 * the function.
	 */
	(void)plane;

	__drm_atomic_helper_plane_destroy_state(old_state);
	kfree(old_tcc_state);
}

static struct drm_plane_funcs tcc_plane_funcs = {
	.update_plane	= drm_atomic_helper_update_plane,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.destroy	= drm_plane_cleanup,
	.reset		= tcc_drm_plane_reset,
	.atomic_duplicate_state = tcc_drm_plane_duplicate_state,
	.atomic_destroy_state = tcc_drm_plane_destroy_state,
};

static int tcc_plane_atomic_check(struct drm_plane *plane,
				     struct drm_plane_state *state)
{
	struct tcc_drm_plane_state *tcc_state;
	const struct drm_crtc_state *crtc_state;
	/* DP no.10 */
	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */ //
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */ //
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);
	bool internal_ok = (bool)true;
	int ret = 0;

	if (state == NULL) {
		internal_ok = (bool)false;
		ret = -EINVAL;
		dev_warn(
			plane->dev->dev,
			"[WARN][%s] %s state is NULL with err(%d)\r\n",
			LOG_TAG, __func__, ret);
	}

	if (internal_ok) {
		tcc_state =
			(struct tcc_drm_plane_state *)to_tcc_plane_state(state);
		if (state->state == NULL) {
			internal_ok = (bool)false;
			ret = -EINVAL;
			dev_warn(
				plane->dev->dev,
				"[WARN][%s] %s state->state is NULL with err(%d)\r\n",
				LOG_TAG, __func__, ret);
		}
	}

	if (internal_ok &&
		((state->crtc == NULL) || (state->fb == NULL))) {
		/*
		 * There is no need for further checks if the plane is
		 * being disabled
		 */
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		crtc_state =
			drm_atomic_get_existing_crtc_state(state->state, state->crtc);
		if (crtc_state == NULL) {
			internal_ok = (bool)false;
			ret = -EINVAL;
			dev_warn(
				plane->dev->dev,
				"[WARN][%s] %s crtc_state is NULL with err(%d)\r\n",
				LOG_TAG, __func__, ret);
		}
	}

	if (internal_ok) {
		switch (state->fb->modifier) {
		#if defined(CONFIG_VIOC_PVRIC_FBDC)
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		case DRM_FORMAT_MOD_PVR_FBCDC_8x8_V10:
			if (!test_bit(
				TCC_DRM_PLANE_CAP_FBDC,
				&tcc_plane->caps)) {
				dev_warn(plane->dev->dev,
					"[WARN][%s] %s crtc[%d] plane[%d] is not support PVR_FBCDC_8x8_V10\r\n",
					LOG_TAG, __func__, state->crtc->base.id,
					tcc_plane->win);
				internal_ok = (bool)false;
				ret = -EINVAL;
			}
			if (internal_ok) {
				/* DP no.2 */
				/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
				/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
				/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
				/* coverity[cert_dcl37_c_violation : FALSE] */
				dev_dbg(plane->dev->dev,
					"[INFO][%s] %s crtc[%d] plane[%d] is support PVR_FBCDC_8x8_V10\r\n",
					LOG_TAG, __func__, state->crtc->base.id,
					tcc_plane->win);
			}
			break;
		#endif
		case DRM_FORMAT_MOD_LINEAR:
			/* DP no.2 */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			dev_dbg(plane->dev->dev,
				"[INFO][%s] %s plane[%d] is support DRM_FORMAT_MOD_LINEAR\r\n",
				LOG_TAG, __func__, tcc_plane->win);
			break;
		default:
			dev_warn(
				plane->dev->dev,
				"[WARN][%s] %s %llx is not supported DRM_FORMAT_MOD\r\n",
				LOG_TAG, __func__, state->fb->modifier);
			internal_ok = (bool)false;
			ret = -EINVAL;
			break;
		}
	}

	if (internal_ok &&
		((state->crtc == NULL) || (state->fb == NULL))) {
		dev_warn(
			plane->dev->dev,
			"[WARN][%s] %s err(%d)\r\n", LOG_TAG, __func__, ret);
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		int crtc_w = tcc_safe_uint2int(state->crtc_w);
		unsigned int hdisplay =
			tcc_safe_int2uint(crtc_state->adjusted_mode.hdisplay);
		crtc_w = tcc_safe_int_pluse(crtc_w, state->crtc_x);
		if (crtc_w < 0) {
			dev_warn(plane->dev->dev,
				"[WARN][%s] %s crtc_x(%d)\r\n",
				LOG_TAG, __func__, crtc_w);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
		if (tcc_safe_int2uint(crtc_w) > hdisplay) {
			dev_warn(plane->dev->dev,
				"[WARN][%s] %s crtc_x(%u) hdisplay(%u)\r\n",
				LOG_TAG, __func__, tcc_safe_int2uint(crtc_w),
				hdisplay);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok) {
		int crtc_h = tcc_safe_uint2int(state->crtc_h);
		unsigned int vdisplay =
			tcc_safe_int2uint(crtc_state->adjusted_mode.vdisplay);

		crtc_h = tcc_safe_int_pluse(crtc_h, state->crtc_y);
		if (crtc_h < 0) {
			dev_warn(plane->dev->dev,
				"[WARN][%s] %s crtc_h(%d)\r\n",
				LOG_TAG, __func__, crtc_h);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
		if (tcc_safe_int2uint(crtc_h) > vdisplay) {
			dev_warn(plane->dev->dev,
				"[WARN][%s] %s crtc_h(%u) vdisplay(%u)\r\n",
				LOG_TAG, __func__, tcc_safe_int2uint(crtc_h),
				vdisplay);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok &&
		((state->src_w >> 16) != state->crtc_w)) {
		dev_warn(
			plane->dev->dev,
			"[WARN][%s] %s mismatch %d with %d scaling mode is not supported\r\n",
			LOG_TAG, __func__, state->src_w >> 16, state->crtc_w);
		internal_ok = (bool)false;
		ret = -ENOTSUPP;
	}

	if (internal_ok &&
		((state->src_h >> 16) != state->crtc_h)) {
		dev_warn(
			plane->dev->dev,
			"[WARN][%s] %s mismatch %d with %d scaling mode is not supported\r\n",
			LOG_TAG, __func__, state->src_h >> 16, state->crtc_h);
		internal_ok = (bool)false;
		ret = -ENOTSUPP;
	}

	if (internal_ok) {
		/* translate state into tcc_state */
		#if defined(CONFIG_DRM_TCC_USES_PLANE_DUMP)
		tcc_plane_mode_set_dump(tcc_state);
		#endif
		tcc_plane_mode_set(tcc_state);
	}
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */ //
static void tcc_plane_atomic_update(struct drm_plane *plane,
				       struct drm_plane_state *old_state)
{
	const struct drm_plane_state *state =
		(const struct drm_plane_state *)plane->state;
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(state->crtc);
	/* DP no.10 */
	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */ //
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */ //
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter old_state is not
	 * used in the function.
	 */
	(void)old_state;

	if (state->crtc != NULL) {
		#if defined(CONFIG_REFCODE_PRE_K54)
		plane->crtc = state->crtc;
		#endif

		if (tcc_crtc->ops->update_plane != NULL) {
			tcc_crtc->ops->update_plane(tcc_crtc, tcc_plane);
		}
	}
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */ //
static void tcc_plane_atomic_disable(struct drm_plane *plane,
					struct drm_plane_state *old_state)
{
	/* DP no.10 */
	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */ //
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */ //
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);
	struct tcc_drm_crtc *tcc_crtc = to_tcc_crtc(old_state->crtc);

	if (old_state->crtc != NULL) {
		if (tcc_crtc->ops->disable_plane != NULL) {
			tcc_crtc->ops->disable_plane(tcc_crtc, tcc_plane);
		}
	}
}

static const struct drm_plane_helper_funcs plane_helper_funcs = {
	.prepare_fb =  drm_gem_fb_prepare_fb,
	.atomic_check = tcc_plane_atomic_check,
	.atomic_update = tcc_plane_atomic_update,
	.atomic_disable = tcc_plane_atomic_disable,
};

int tcc_plane_init(struct drm_device *dev,
			struct drm_plane *plane,
			enum drm_plane_type plane_type,
			const uint32_t *pixel_formats,
			unsigned int num_pixel_formats)
{
	unsigned int num_crtc;
	int ret;

	if (dev->mode_config.num_crtc < 0) {
		ret = -EINVAL;
	} else {
		num_crtc = tcc_safe_int2uint(dev->mode_config.num_crtc);
		num_crtc = (unsigned int)1U << num_crtc;

		ret = drm_universal_plane_init(dev, plane,
					num_crtc,
					&tcc_plane_funcs,
					pixel_formats,
					num_pixel_formats,
					NULL, plane_type, NULL);
	}

	if (ret < 0) {
		DRM_ERROR("failed to initialize plane\n");
	} else {
		drm_plane_helper_add(plane, &plane_helper_funcs);
	}
	return ret;
}

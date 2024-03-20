// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * TCC DRM Parallel output support.
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * Contacts: Andrzej Hajda <a.hajda@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>

#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/tcc_math.h>

#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include <video/tcc/vioc_global.h>
#include <tcc_drm_address.h>
#include <tcc_drm_crtc.h>
#include <tcc_drm_dpi.h>
#include <tcc_drm_edid.h>

#define LOG_TAG "DRMDPI"

#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
/* property */
enum tcc_prop_audio_freq {
	PROP_36_HZ = 0,
	PROP_44_1HZ,
	PROP_48HZ,
	PROP_88_2HZ,
	PROP_96HZ,
	PROP_192HZ,
};

static const struct drm_prop_enum_list tcc_prop_audio_freq_names[] = {
	{ (int)PROP_36_HZ, "36hz" },
	{ (int)PROP_44_1HZ, "44.1hz" },
	{ (int)PROP_48HZ, "48hz" },
	{ (int)PROP_88_2HZ, "88.2hz" },
	{ (int)PROP_96HZ, "96hz" },
	{ (int)PROP_192HZ, "192hz" },
};

enum tcc_prop_audio_type {
	PROP_TYPE_PCM = 0,
	PROP_TYPE_DD,
	PROP_TYPE_DDP,
	PROP_TYPE_DTS,
	PROP_TYPE_DTS_HD,
};

static const struct drm_prop_enum_list tcc_prop_audio_type_names[] = {
	{ (int)PROP_TYPE_PCM, "pcm" },
	{ (int)PROP_TYPE_DD, "dd" },
	{ (int)PROP_TYPE_DDP, "ddp" },
	{ (int)PROP_TYPE_DTS, "dts" },
	{ (int)PROP_TYPE_DTS_HD, "dts-hd" },
};
#endif

struct drm_detailed_timing_t {
	int vic;
	unsigned int pixelrepetions;
	unsigned int pixelclock;
	unsigned int interlaced;
	unsigned int hactive;
	unsigned int hblanking;
	unsigned int hsyncoffset;
	unsigned int hsyncwidth;
	unsigned int hsyncpolarity;
	unsigned int vactive;
	unsigned int vblanking;
	unsigned int vsyncoffset;
	unsigned int vsyncwidth;
	unsigned int vsyncpolarity;
};

#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
struct tcc_dpi_dp {
	int dp_id;
	struct dptx_drm_helper_funcs *funcs;
};

struct tcc_dp_prop {
	struct drm_property *audio_freq;
	struct drm_property *audio_type;
};

struct tcc_dp_prop_data {
	enum tcc_prop_audio_freq audio_freq;
	enum tcc_prop_audio_type audio_type;
};
#endif

struct tcc_dpi {
	struct drm_encoder encoder;
	struct device *dev;

	struct drm_panel *panel;
	struct device_node *panel_node;
	struct drm_connector connector;

	struct display_timings *timings;

	struct tcc_hw_device *hw_device;

	#if defined(CONFIG_DRM_TCC_DPI_PROC)
	struct proc_dir_entry *proc_dir;
	struct proc_dir_entry *proc_hpd;
	struct proc_dir_entry *proc_edid;
	enum drm_connector_status manual_hpd;
	#endif
	#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
	struct tcc_dpi_dp *dp;
	struct tcc_dp_prop dp_prop;
	struct tcc_dp_prop_data dp_prop_data;
	#endif
};

static const struct drm_detailed_timing_t drm_detailed_timing[] = {
/*  vic pixelrepetion  hactive     hsyncoffset  vactive    vsyncoffset
 * |   |   pixelclock(KHz)  hblanking  hsyncwidth    vblanking    vsyncpolarity
 * |   |   |       interlaced     |    |     hsyncpolarity|    vsyncwidth
 * |   |   |       |  |     |     |    |     | |      |   |    |   |
 */
	/* CEA-861 VIC 4 1280x720@60p */
	{    4,  0,  74250, 0, 1280,  370,  110,  40, 1,  720,  30,  5,  5, 1},
	/* CEA-861 VIC16 1920x1080@60p */
	{   16,  0, 148500, 0, 1920,  280,   88,  44, 1, 1080,  45,  4,  5, 1},

	/* CUSTOM TM123XDHP90 1920x720@60p */
	{ 1024,  0,  88200, 0, 1920,   64,   30,   4, 0,  720,  21, 10,  2, 0},
	/* CUSTOM AV080WSM-NW2 1024x600@60p */
	{ 1025,  0,  51200, 0, 1024,  313,  147,  19, 0,  600,  37, 25,  2, 0},
	/* CUSTOM VIRTUAL DEVICE 1920x720@30p */
	{ 1026,  0,  44100, 0, 1920,   64,   30,   4, 0,  720,  21, 10,  2, 0},
};

static struct tcc_dpi *connector_to_dpi(struct drm_connector *c)
{
	/* DP no.10 */
	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */	//
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */	//
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	return container_of(c, struct tcc_dpi, connector);
}
static struct tcc_dpi *encoder_to_dpi(struct drm_encoder *e)
{
	/* DP no.10 */
	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */	//
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */	//
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	return container_of(e, struct tcc_dpi, encoder);
}

static int drm_detailed_timing_find_index(int vic)
{
	int i;
	int table_index_max =
		(int)sizeof(drm_detailed_timing) /
		(int)sizeof(struct drm_detailed_timing_t);
	for (i = 0; i < table_index_max; i++) {
		if (vic == drm_detailed_timing[i].vic) {
			break;
		}
	}

	if (i == table_index_max) {
		i = -1;
	}
	return i;
}

static int drm_detailed_timing_convert_to_drm_mode(
			const struct drm_detailed_timing_t *in,
			struct drm_display_mode *out)
{
	unsigned int tmp, tmp2, tmp3;
	bool internal_ok = (bool)true;
	int ret = 0;

	if (in == NULL) {
		ret = -EINVAL;
		internal_ok = (bool)false;
	}
	if (internal_ok && (out == NULL)) {
		ret = -EINVAL;
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		out->hdisplay = tcc_safe_uint2int(in->hactive);

		/* hsync_start = hdisplay + hsyncoffset */
		tmp2 = tcc_safe_int2uint(out->hdisplay);
		tmp = tcc_safe_uint_pluse(tmp2, in->hsyncoffset);
		out->hsync_start = tcc_safe_uint2int(tmp);

		/* hsync_end = hsync_start + hsyncwidth */
		tmp2 = tcc_safe_int2uint(out->hsync_start);
		tmp = tcc_safe_uint_pluse(tmp2, in->hsyncwidth);
		out->hsync_end = tcc_safe_uint2int(tmp);

		/* htotal = hsync_end + (hblanking - (hsyncoffset +  hsyncwidth)) */
		tmp3 = tcc_safe_uint_pluse(in->hsyncoffset, in->hsyncwidth);
		if (tmp3 > in->hblanking) {
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		tmp3 = in->hblanking - tmp3;
		tmp2 = tcc_safe_int2uint(out->hsync_end);
		tmp = tcc_safe_uint_pluse(tmp2, tmp3);
		out->htotal = tcc_safe_uint2int(tmp);

		out->vdisplay = tcc_safe_uint2int(in->vactive);

		/* vsync_start = vdisplay + vsyncoffset */
		tmp2 = tcc_safe_int2uint(out->vdisplay);
		tmp = tcc_safe_uint_pluse(tmp2, in->vsyncoffset);
		out->vsync_start = tcc_safe_uint2int(tmp);

		/* vsync_end = vsync_start + vsyncwidth */
		tmp2 = tcc_safe_int2uint(out->vsync_start);
		tmp = tcc_safe_uint_pluse(tmp2, in->vsyncwidth);
		out->vsync_end = tcc_safe_uint2int(tmp);

		/* vtotal = vsync_end + (vblanking - (vsyncoffset + vsyncwidth)) */
		tmp3 = 	tcc_safe_uint_pluse(in->vsyncoffset, in->vsyncwidth);
		if (tmp3 > in->vblanking) {
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		tmp3 = in->vblanking - tmp3;
		tmp2 = tcc_safe_int2uint(out->vsync_end);
		tmp = tcc_safe_uint_pluse(tmp2, tmp3);
		out->vtotal = tcc_safe_uint2int(tmp);

		out->clock = tcc_safe_uint2int(in->pixelclock);

		out->flags = 0;
		if ((bool)in->hsyncpolarity) {
			/* DP no.4 */
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			out->flags |= DRM_MODE_FLAG_PHSYNC;
		} else {
			/* DP no.4 */
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			out->flags |= DRM_MODE_FLAG_NHSYNC;
		}
		if ((bool)in->vsyncpolarity) {
			/* DP no.4 */
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			out->flags |= DRM_MODE_FLAG_PVSYNC;
		} else {
			/* DP no.4 */
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			out->flags |= DRM_MODE_FLAG_NVSYNC;
		}
		if ((bool)in->interlaced) {
			/* DP no.4 */
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			out->flags |= DRM_MODE_FLAG_INTERLACE;
		}
		if (in->pixelrepetions > 0U) {
			/* DP no.4 */
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_2_violation : FALSE] */
			out->flags |= DRM_MODE_FLAG_DBLCLK;
		}
		drm_mode_set_name(out);
	}
	return ret;
}

#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
#if defined(CONFIG_TCC_DRM_SUPPORT_REAL_HPD)
static int tcc_drm_dp_get_hpd_state(struct tcc_dpi *ctx)
{
	unsigned char hpd_state = 0;
	bool internal_ok = (bool)true;
	int ret = 0;

	if (ctx->dp == NULL) {
		internal_ok = (bool)false;
	}
	if (internal_ok && (ctx->dp->funcs == NULL)) {
		internal_ok = (bool)false;
	}
	if (internal_ok && (ctx->dp->funcs->get_hpd_state == NULL)) {
		internal_ok = (bool)false;
	}
	if (internal_ok &&
		(ctx->dp->funcs->get_hpd_state(ctx->dp->dp_id, &hpd_state) < 0)) {
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		if (hpd_state != 0) {
			ret = 1;
		}
	}

	return ret;
}
#endif

static int tcc_drm_encoder_set_video(
	const struct tcc_dpi *ctx, const struct drm_crtc_state *crtc_state)
{
	struct dptx_detailed_timing_t timing;
	bool internal_ok = (bool)true;
	unsigned long clk_val;
	struct videomode vm;
	int ret = 0;

	if (ctx->dp == NULL) {
		ret = -EINVAL;
		internal_ok = (bool)false;
	}
	if (internal_ok && (ctx->dp->funcs == NULL)) {
		ret = -EINVAL;
		internal_ok = (bool)false;
	}
	if (internal_ok && (ctx->dp->funcs->set_video == NULL)) {
		ret = -EINVAL;
		internal_ok = (bool)false;
	}
	if (internal_ok &&
		(ctx->hw_device->connector_type != DRM_MODE_CONNECTOR_DisplayPort)) {
		ret = -EINVAL;
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		unsigned int vm_flags, cmp_flags;
		drm_display_mode_to_videomode(&crtc_state->adjusted_mode, &vm);

		(void)memset(&timing, 0, sizeof(timing));

		vm_flags = (unsigned int)vm.flags;
		/* DP no.16 */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		cmp_flags = (unsigned int)DISPLAY_FLAGS_INTERLACED;
		timing.interlaced =
			((vm_flags & cmp_flags) == cmp_flags) ? 1U : 0U;
		/* DP no.16 */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		cmp_flags = (unsigned int)DISPLAY_FLAGS_DOUBLECLK;
		timing.pixel_repetition_input =
			((vm_flags & cmp_flags) == cmp_flags) ? 1U: 0U;
		timing.h_active = vm.hactive;
		/* h_blanking = hfront_porch + hsync_len + hback_porch; */
		timing.h_blanking = tcc_safe_uint_pluse(vm.hfront_porch,
							vm.hsync_len);
		timing.h_blanking = tcc_safe_uint_pluse(timing.h_blanking,
							vm.hback_porch);
		timing.h_sync_offset = vm.hfront_porch;
		timing.h_sync_pulse_width = vm.hsync_len;
		/* DP no.16 */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		cmp_flags = (unsigned int)DISPLAY_FLAGS_HSYNC_LOW;

#if defined(CONFIG_DRM_TCC_LCD_VIC)  //jbhyun: For TOST
		if(CONFIG_DRM_TCC_LCD_VIC == 16) {
			timing.h_sync_polarity =
				((vm_flags & cmp_flags) == cmp_flags) ? 0U : 1U;
		}
		else if (CONFIG_DRM_TCC_LCD_VIC == 0) {
			timing.h_sync_polarity =
			    ((vm_flags & cmp_flags) == cmp_flags) ? 1U : 0U;
		}
		(void)pr_info(
		"[INFO][%s] %s:%d CONFIG_DRM_TCC_LCD_VIC %d h_sync_polarity %d  \r\n",
				LOG_TAG, __func__, __LINE__, CONFIG_DRM_TCC_LCD_VIC, timing.h_sync_polarity);
#else
		timing.h_sync_polarity =
			((vm_flags & cmp_flags) == cmp_flags) ? 0U : 1U;
#endif
		timing.v_active = vm.vactive;

		/* v_blanking = vfront_porch + vsync_len + vback_porch */
		timing.v_blanking = tcc_safe_uint_pluse(vm.vfront_porch,
							vm.vsync_len);
		timing.v_blanking = tcc_safe_uint_pluse(timing.v_blanking,
							vm.vback_porch);
		timing.v_sync_offset = vm.vfront_porch;
		timing.v_sync_pulse_width = vm.vsync_len;
		/* DP no.16 */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		cmp_flags = (unsigned int)DISPLAY_FLAGS_VSYNC_LOW;

#if defined(CONFIG_DRM_TCC_LCD_VIC)  //jbhyun: For TOST
		if(CONFIG_DRM_TCC_LCD_VIC == 16) {
			timing.v_sync_polarity =
				((vm_flags & cmp_flags) == cmp_flags) ? 0U : 1U;
		}
		else if (CONFIG_DRM_TCC_LCD_VIC == 0) {
			timing.v_sync_polarity =
			    ((vm_flags & cmp_flags) == cmp_flags) ? 1U : 0U;
		}
		(void)pr_info(
		"[INFO][%s] %s:%d CONFIG_DRM_TCC_LCD_VIC %d v_sync_polarity %d  \r\n",
				LOG_TAG, __func__, __LINE__, CONFIG_DRM_TCC_LCD_VIC, timing.v_sync_polarity);
#else
		timing.v_sync_polarity =
			((vm_flags & cmp_flags) == cmp_flags) ? 0U : 1U;
#endif

		/* DP pixel clock in kHz */
		clk_val = clk_get_rate(ctx->hw_device->ddc_clock);
		clk_val = ((ULONG_MAX - clk_val) < 1000UL) ?
			  (ULONG_MAX - 1000UL): clk_val;

		/* DP no.14 */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		clk_val = DIV_ROUND_UP(clk_val, 1000UL);
		timing.pixel_clock = tcc_safe_ulong2uint(clk_val);

		if (ctx->dp->funcs->set_video(ctx->dp->dp_id, &timing) < 0) {
			ret = -ENODEV;
		}
	}

	return ret;
}

static int tcc_drm_encoder_enable_video(
	const struct tcc_dpi *ctx, unsigned char enable)
{
	bool internal_ok = (bool)true;
	int ret = -EINVAL;

	if (ctx->dp == NULL) {
		internal_ok = (bool)false;
	}
	if (internal_ok && (ctx->dp->funcs == NULL)) {
		internal_ok = (bool)false;
	}
	if (internal_ok && (ctx->dp->funcs->set_enable_video == NULL)) {
		internal_ok = (bool)false;
	}
	if (internal_ok &&
		(ctx->hw_device->connector_type != DRM_MODE_CONNECTOR_DisplayPort)) {
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		ret = ctx->dp->funcs->set_enable_video(ctx->dp->dp_id, enable);
	}
	return ret;
}
#else
#define tcc_drm_encoder_set_video(ctx, crtc_state) (0)
#define tcc_drm_encoder_enable_video(ctx, enable) (0)
#endif

static enum drm_connector_status
tcc_dpi_detect(struct drm_connector *connector, bool force)
{
	enum drm_connector_status connector_status =
				connector_status_connected;
	struct tcc_dpi *ctx = connector_to_dpi(connector);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter force is not used
	 * in the function
	 */
	(void)force;

	if ((ctx->panel != NULL) && (ctx->panel->connector == NULL)) {
		(void)drm_panel_attach(ctx->panel, &ctx->connector);
	}

	#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
	if (ctx->hw_device->connector_type == DRM_MODE_CONNECTOR_DisplayPort) {
		#if defined(CONFIG_TCC_DRM_SUPPORT_REAL_HPD)
		/**
		 * The HAL(Hardware Abstraction layer) must be able to
		 * handle HPD events provided by DRM.
		 * However, the HAL provided by Telechips can not
		 * handle this HPD events, so until the HAL is ready,
		 * DRM always returns HPD status to true.
		 */
		if (ctx->dp != NULL) {
			if (tcc_drm_dp_get_hpd_state(ctx) == 0) {
				connector_status =
					connector_status_disconnected;
			}
		}
		#endif
	}
	#endif
	#if defined(CONFIG_DRM_TCC_DPI_PROC)
	if (ctx->manual_hpd != connector_status_unknown) {
		connector_status = ctx->manual_hpd;
	}
	#endif
	/* DP no.2 */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	dev_dbg(ctx->dev,
			"[WARN][%s] %s status = %s\r\n",
			LOG_TAG, __func__,
			(connector_status == connector_status_connected) ?
			"connected" : "disconnected");

	return connector_status;
}

static void tcc_dpi_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
static int tcc_dpi_connector_atomic_get_property(
	struct drm_connector *connector,
	/* DP no.6 */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	const struct drm_connector_state *state,
	/* DP no.6 */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct drm_property *out_property,
	uint64_t *val)
{
	const struct tcc_dpi *ctx = connector_to_dpi(connector);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter state is not used
	 * in the function.
	 */
	(void)state;

	if (out_property == ctx->dp_prop.audio_freq) {
		uint64_t audio_freq;

		//audio_freq = ctx->dp->funcs->get_audio_freq();
		audio_freq = (uint64_t)ctx->dp_prop_data.audio_freq;
		*val = audio_freq;
	} else {
		if (out_property == ctx->dp_prop.audio_type) {
			uint64_t audio_type;

			/* *val = ctx->dp->funcs->get_audio_type(); */
			audio_type = (uint64_t)ctx->dp_prop_data.audio_type;
			*val = audio_type;
		}
	}
	return 0;
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int
tcc_dpi_connector_atomic_set_property(struct drm_connector *connector,
					/* DP no.6 */
					/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
					 struct drm_connector_state *state,
					 /* DP no.6 */
					/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
					 struct drm_property *in_property,
					 uint64_t val)
{
	struct tcc_dpi *ctx = connector_to_dpi(connector);
	unsigned int uval = tcc_safe_u642uint(val);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter state is not used in
	 * the function.
	 */
	(void)state;

	if (in_property == ctx->dp_prop.audio_freq) {
		if (uval <= (unsigned int)PROP_192HZ) {
			//ctx->dp->funcs->set_audio_freq(val);
			ctx->dp_prop_data.audio_freq =
				(enum tcc_prop_audio_freq)uval;
		}
	} else {
		if (in_property == ctx->dp_prop.audio_type) {
			if (uval <= (unsigned int)PROP_TYPE_DTS_HD) {
				//ctx->dp->funcs->set_audio_freq(val);
				ctx->dp_prop_data.audio_type =
					(enum tcc_prop_audio_type)uval;
			}
		}
	}
	return 0;
}
#endif
static const struct drm_connector_funcs tcc_dpi_connector_funcs = {
	.detect = tcc_dpi_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = tcc_dpi_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
	.atomic_get_property = tcc_dpi_connector_atomic_get_property,
	.atomic_set_property = tcc_dpi_connector_atomic_set_property,
	#endif
};


static int tcc_dpi_register_mode(struct drm_connector *connector,
				  struct drm_display_mode *mode)
{
	const struct tcc_dpi *ctx;
	bool internal_ok = (bool)true;
	int tmp_val;
	int ret = 0;

	if (connector == NULL) {
		(void)pr_err(
			"[ERROR][%s] %s connector is NULL\r\n",
			LOG_TAG, __func__);
		ret = -EINVAL;
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		ctx = (const struct tcc_dpi *)connector_to_dpi(connector);
		if (ctx == NULL) {
			(void)pr_err(
				"[ERROR][%s] %s ctx is NULL\r\n",
				LOG_TAG, __func__);
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		/*
		* Physical size as value that display
		* resolution divided by 10.
		*/
		if (mode->hdisplay > 1) {
			/* DP no.14 */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			tmp_val = DIV_ROUND_UP(mode->hdisplay, 10);
		} else {
			tmp_val = mode->hdisplay;
		}
		connector->display_info.width_mm =
			tcc_safe_int2uint(tmp_val);

		if (mode->vdisplay > 1) {
			/* DP no.14 */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			tmp_val = DIV_ROUND_UP(mode->vdisplay, 10);
		} else {
			tmp_val = mode->vdisplay;
		}
		connector->display_info.height_mm = tcc_safe_int2uint(tmp_val);

		drm_mode_probed_add(connector, mode);
	}

	return ret;
}

/*
 * This function called in drm_helper_probe_single_connector_modes
 * return is count
 */
static int tcc_dpi_get_modes(struct drm_connector *connector)
{
	int count = 0;
	int table_index;
	const struct tcc_dpi *ctx;
	struct drm_display_mode *mode = NULL;
	const struct drm_detailed_timing_t *dtd;
	bool internal_ok = (bool)true;
	bool find_modes = (bool)false;

	if (connector == NULL) {
		(void)pr_err(
			"[ERROR][%s] %s connector is NULL\r\n",
			LOG_TAG, __func__);
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		ctx = (const struct tcc_dpi *)connector_to_dpi(connector);
		if (ctx == NULL) {
			(void)pr_err(
				"[ERROR][%s] %s ctx is NULL\r\n", LOG_TAG, __func__);
			internal_ok = (bool)false;
		}
	}

	/* step1: Check panels */
	if (internal_ok) {
		if (ctx->panel != NULL) {
			count = drm_panel_get_modes(ctx->panel);
			if (count > 0) {
				find_modes = (bool)true;
			}
		}
	}

	/* step2: Check detailed timing from device tree */
	if (internal_ok && !find_modes) {
		if (ctx->timings != NULL) {
			const struct display_timings *timings = ctx->timings;
			int num_timings =
				tcc_safe_uint2int(timings->num_timings);
			bool for_ok = (bool)true;
			struct videomode vm;

			for (count = 0; count < num_timings; count++) {
				if (videomode_from_timings(timings,
							   &vm,
							   tcc_safe_int2uint(count)) < 0) {
					for_ok = (bool)false;
				}
				if (for_ok) {
					mode = drm_mode_create(connector->dev);
					if (mode == NULL) {
						(void)pr_err(
							"[ERROR][%s] %s failed to create drm_mode\r\n",
							LOG_TAG, __func__);
						for_ok = (bool)false;
					}
				}
				if (!for_ok) {
					break;
				}
				drm_display_mode_from_videomode(&vm, mode);

				/* DP no.16 */
				/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
				mode->type = DRM_MODE_TYPE_DRIVER;
				drm_mode_set_name(mode);

				if (timings->native_mode ==
					tcc_safe_int2uint(count)) {

					/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
					/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
					mode->type |= DRM_MODE_TYPE_PREFERRED;

					/* DP no.16 */
					/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
					dev_info(
						ctx->dev,
						"[INFO][%s] Native mode is detected at index [%d] name [%s]\r\n",
						LOG_TAG, count, mode->name);
				}

				(void)tcc_dpi_register_mode(connector, mode);
			}
			if (count > 0) {
				find_modes = (bool)true;
			}
		}
	}

	/* step3: Check kernel config */
	if (internal_ok && !find_modes) {
		if (ctx->hw_device == NULL) {
			(void)pr_err(
				"[ERROR][%s] %s hw_device is NULL\r\n",
				LOG_TAG, __func__);
			internal_ok = (bool)false;
		}
	}

	if (internal_ok && !find_modes) {
		/* clearn edid property */
		#if defined(CONFIG_REFCODE_PRE_K54)
		(void)drm_mode_connector_update_edid_property(connector, NULL);
		#else
		(void)drm_connector_update_edid_property(connector, NULL);
		#endif
		(void)pr_info(
			"[INFO][%s] %s:%d vic %d ==> 0 (temporary), connector_type %d  \r\n",
			LOG_TAG, __func__, __LINE__, ctx->hw_device->vic, ctx->hw_device->connector_type);

		switch (ctx->hw_device->vic)  {
		#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
		case 0:
			if (
				ctx->hw_device->connector_type ==
				DRM_MODE_CONNECTOR_DisplayPort) {

				struct edid *target_edid =
					devm_kzalloc(
						ctx->dev,
						(int)sizeof(*target_edid) * 4,
						/* DP no.13 */
						/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
						/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
						GFP_KERNEL);
				if (target_edid == NULL) {
					internal_ok = (bool)false;
				}
				if (internal_ok && (ctx->dp == NULL)) {
					internal_ok = (bool)false;
				}
				if (internal_ok && (ctx->dp->funcs == NULL)) {
					internal_ok = (bool)false;
				}
				if (internal_ok &&
				    (ctx->dp->funcs->get_edid == NULL)) {
					internal_ok = (bool)false;
				}
				if (internal_ok &&
					(ctx->dp->funcs->get_edid(
						ctx->dp->dp_id,
					    	(unsigned char *)target_edid,
					    	(int)sizeof(*target_edid) * 4) < 0)) {
					internal_ok = (bool)false;
				}
				if (internal_ok) {
					#if defined(CONFIG_REFCODE_PRE_K54)
					(void)drm_mode_connector_update_edid_property(
						connector, target_edid);
					#else
					(void)drm_connector_update_edid_property(
						connector, target_edid);
					#endif
					count =
						drm_add_edid_modes(connector,
								   target_edid);
					#if defined(CONFIG_REFCODE_PRE_K54)
					drm_edid_to_eld(connector, target_edid);
					#endif
					devm_kfree(ctx->dev, target_edid);
					find_modes = (bool)true;
				}
			}
			break;
		#endif
		default:
			table_index =
				drm_detailed_timing_find_index(ctx->hw_device->vic);
			if (table_index < 0) {
				dev_warn(
					ctx->dev,
					"[INFO][%s] %s failed to get detailed timing table index, It sets table index to zero temporarily.\r\n",
					LOG_TAG, __func__);
				table_index = 0;
			}
			mode = drm_mode_create(connector->dev);
			if (mode == NULL) {
				(void)pr_err(
					"[ERROR][%s] %s failed to create drm_mode\r\n",
					LOG_TAG, __func__);
				internal_ok = (bool)false;
			}
			if (internal_ok) {
				dtd = (const struct drm_detailed_timing_t *)&drm_detailed_timing[table_index];
				if (drm_detailed_timing_convert_to_drm_mode(
					dtd, mode) == 0) {
					count = 1;
					find_modes = (bool)true;

					mode->type |= DRM_MODE_TYPE_PREFERRED;
					(void)tcc_dpi_register_mode(connector, mode);
				}
			}
			break;
		}
	}
	if (!find_modes) {
		count = 0;
	}
	return count;
}

#if defined(CONFIG_REFCODE_PRE_K54)
#else
static struct drm_encoder *tcc_dpi_best_single_encoder(
	struct drm_connector *connector)
{
	struct tcc_dpi *ctx = connector_to_dpi(connector);

	return &ctx->encoder;
}
#endif
static const struct
drm_connector_helper_funcs tcc_dpi_connector_helper_funcs = {
	.get_modes = tcc_dpi_get_modes,
	#if defined(CONFIG_REFCODE_PRE_K54)
	.best_encoder = drm_atomic_helper_best_encoder,
	#else
	.best_encoder = tcc_dpi_best_single_encoder,
	#endif
};

static int tcc_dpi_create_connector(
	struct drm_encoder *encoder, const struct tcc_hw_device *hw_data)
{
	struct tcc_dpi *ctx = encoder_to_dpi(encoder);
	struct drm_connector *connector = &ctx->connector;
	bool internal_ok = (bool)true;
	int ret = 0;

	/* DP no.16 */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	connector->polled = DRM_CONNECTOR_POLL_HPD;

	dev_info(
		ctx->dev, "[INFO][%s] %s connector type is %d\n",
		LOG_TAG, __func__, hw_data->connector_type);
	ret = drm_connector_init(encoder->dev, connector,
				&tcc_dpi_connector_funcs,
				hw_data->connector_type);

	if (ret < 0) {
		DRM_ERROR("failed to initialize connector with drm\n");
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
		int num_values;

		if (ctx->hw_device->connector_type ==
		    DRM_MODE_CONNECTOR_DisplayPort) {
			/* audio_freq */
			/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			num_values = (int)ARRAY_SIZE(tcc_prop_audio_freq_names);
			ctx->dp_prop.audio_freq =
				drm_property_create_enum(connector->dev, 0,
					"audio_freq",
					tcc_prop_audio_freq_names,
					num_values);
			drm_object_attach_property(&connector->base,
					ctx->dp_prop.audio_freq, 0);
			/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			num_values = (int)ARRAY_SIZE(tcc_prop_audio_type_names);
			ctx->dp_prop.audio_type =
				drm_property_create_enum(connector->dev, 0,
					"audio_type",
					tcc_prop_audio_type_names,
					num_values);
			drm_object_attach_property(&connector->base,
					ctx->dp_prop.audio_type, 0);
		}
		#endif

		drm_connector_helper_add(connector,
					 &tcc_dpi_connector_helper_funcs);
		#if defined(CONFIG_REFCODE_PRE_K54)
		(void)drm_mode_connector_attach_encoder(connector, encoder);
		#else
		(void)drm_connector_attach_encoder(connector, encoder);
		#endif
	}

	return ret;
}

static void
tcc_dpi_atomic_mode_set(struct drm_encoder *encoder,
			/* DP no.6 */
			/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
			struct drm_crtc_state *crtc_state,
			/* DP no.6 */
			/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
			struct drm_connector_state *connector_state)
{
	const struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter connector_state is
	 * not used in the function.
	 */
	(void)connector_state;

	if (ctx->panel != NULL) {
		(void)drm_panel_prepare(ctx->panel);
		/*  panel->funcs->prepare(panel) */
	}
	(void)tcc_drm_encoder_set_video(ctx,
				  (const struct drm_crtc_state *)crtc_state);
	(void)tcc_drm_encoder_enable_video(ctx, 1);
}

static void tcc_dpi_enable(struct drm_encoder *encoder)
{
	const struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	if (ctx->panel != NULL) {
		(void)drm_panel_enable(ctx->panel);
		/* panel->funcs->enable(panel); */
	}
}

static void tcc_dpi_disable(struct drm_encoder *encoder)
{
	const struct tcc_dpi *ctx =
		(const struct tcc_dpi *)encoder_to_dpi(encoder);

	/* DP no.2 */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	dev_dbg(ctx->dev,
		"[DEBUG][%s] %s disable encoder\r\n",
					LOG_TAG, __func__);
	if (ctx->panel != NULL) {
		/* DP no.2 */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		dev_dbg(ctx->dev,
			"[DEBUG][%s] %s disable panel\r\n",
						LOG_TAG, __func__);
		(void)drm_panel_disable(ctx->panel);
		/* panel->funcs->disable(panel); */

		(void)drm_panel_unprepare(ctx->panel);
		/* panel->funcs->unprepare(panel); */
	}

	(void)tcc_drm_encoder_enable_video((const struct tcc_dpi *)ctx, 0);
}

static const struct drm_encoder_helper_funcs tcc_dpi_encoder_helper_funcs = {
	.atomic_mode_set = tcc_dpi_atomic_mode_set,
	.enable = tcc_dpi_enable,
	.disable = tcc_dpi_disable,
};

static const struct drm_encoder_funcs tcc_dpi_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int tcc_dpi_parse_dt(struct tcc_dpi *ctx)
{
	const struct device *dev = ctx->dev;
	const struct device_node *dn = dev->of_node;
	struct device_node *np;
	int ret = 0;

	np = of_get_child_by_name(dn, "display-timings");
	if (np != NULL) {
		of_node_put(np);

		ctx->timings = of_get_display_timings(dn);
		if (ctx->timings == NULL) {
			dev_warn(dev,
				"[WARN][%s] %s failed to of_get_display_timings\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	} else {
		/* DP no.2 */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		dev_dbg(dev,
			"[DEBUG][%s] %s cannot find display-timings node\n",
			LOG_TAG, __func__);
	}

	return ret;
}
#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
static int tcc_drm_dp_attach(struct drm_encoder *encoder, int dp_id, int flags)
{
	const struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter flags is not used
	 * in the function.
	 */
	(void)dp_id;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter flags is not used
	 * in the function.
	 */
	(void)flags;

	(void)drm_helper_hpd_irq_event(ctx->encoder.dev);
	return 0;
}

static int tcc_drm_dp_detach(struct drm_encoder *encoder, int dp_id, int flags)
{
	const struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter dp_id is not used
	 * in the function.
	 */
	(void)dp_id;
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter flags is not used
	 * in the function.
	 */
	(void)flags;


	(void)drm_helper_hpd_irq_event(ctx->encoder.dev);
	return 0;
}

static int tcc_drm_dp_register_helper_funcs(struct drm_encoder *encoder,
				struct dptx_drm_helper_funcs *helper_funcs)
{
	const struct tcc_dpi *ctx =
		(const struct tcc_dpi *)encoder_to_dpi(encoder);
	int ret = -EINVAL;

	if (ctx->dp != NULL) {
		ctx->dp->funcs = helper_funcs;
		ret = 0;
	}
	return ret;
}

static struct tcc_drm_dp_callback_funcs tcc_dp_callback_funcs = {
	.attach = tcc_drm_dp_attach,
	.detach = tcc_drm_dp_detach,
	.register_helper_funcs = tcc_drm_dp_register_helper_funcs,
};
#endif

int tcc_dpi_bind(
	struct drm_device *dev, struct drm_encoder *encoder,
	const struct tcc_hw_device *hw_data)
{
	int encoder_type;
	int i, ret = 0;

	switch (hw_data->connector_type) {
	case DRM_MODE_CONNECTOR_DisplayPort:
	case DRM_MODE_CONNECTOR_HDMIA:
	case DRM_MODE_CONNECTOR_HDMIB:
		encoder_type = DRM_MODE_ENCODER_TMDS;
		break;
	case DRM_MODE_CONNECTOR_VIRTUAL:
		encoder_type = DRM_MODE_ENCODER_VIRTUAL;
		break;
	default:
		encoder_type = DRM_MODE_ENCODER_LVDS;
		break;
	}

	/* DP no.2 */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	dev_dbg(
		dev->dev,
		"[DEBUG][%s] %s encoder type is %d\n",
		LOG_TAG, __func__, encoder_type);

	(void)drm_encoder_init(
		dev, encoder, &tcc_dpi_encoder_funcs, encoder_type, NULL);

	drm_encoder_helper_add(encoder, &tcc_dpi_encoder_helper_funcs);

	for (
		i = (int)TCC_DISPLAY_TYPE_SCREEN_SHARE;
		i > (int)TCC_DISPLAY_TYPE_NONE ; i--) {
		if (
			tcc_drm_set_possible_crtcs(
				encoder, (enum tcc_drm_output_type)i) == 0) {
			break;
		}
	}
	if (i == (int)TCC_DISPLAY_TYPE_NONE) {
		dev_err(dev->dev,
			"[ERROR][%s] %s faild to get possible crcts\r\n",
			LOG_TAG, __func__);
	}

	dev_info(
		dev->dev, "[INFO][%s] %s %s- possible_crtcs=0x%08x, dev=%d\r\n",
		LOG_TAG, __func__,
		(i == (int)TCC_DISPLAY_TYPE_SCREEN_SHARE) ? "screen_share" :
		(i == (int)TCC_DISPLAY_TYPE_FOURTH) ? "fourth" :
		(i == (int)TCC_DISPLAY_TYPE_THIRD) ? "triple" :
		(i == (int)TCC_DISPLAY_TYPE_EXT) ? "ext" :
		(i == (int)TCC_DISPLAY_TYPE_LCD) ? "lcd" : "none",
		get_vioc_index(hw_data->display_device.blk_num),
		encoder->possible_crtcs);

	ret = tcc_dpi_create_connector(encoder, hw_data);
	if (ret < 0) {
		DRM_ERROR("failed to create connector ret = %d\n", ret);
		drm_encoder_cleanup(encoder);
	}

	return ret;
}

#if defined(CONFIG_DRM_TCC_DPI_PROC)
/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int proc_open(struct inode *finode, struct file *filp)
{
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter finode is not
	 * used in the function.
	 */
	(void)finode;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter filp is not
	 * used in the function.
	 */
	(void)filp;

	(void)try_module_get(THIS_MODULE);

	return 0;
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int proc_close(struct inode *finode, struct file *filp)
{
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter finode is not
	 * used in the function.
	 */
	(void)finode;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter filp is not
	 * used in the function.
	 */
	(void)filp;

	module_put(THIS_MODULE);

	return 0;
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t proc_write_hpd(struct file *filp, const char __user *buffer,
						size_t cnt, loff_t *off_set)
{
	bool internal_ok = (bool)true;
	ssize_t writed = 0;
	int ret, hpd = 0;
	size_t alloc_cnt;
	char *hpd_buff = NULL;

	/* DP no.15 */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_dpi *ctx = PDE_DATA(file_inode(filp));

	if ((ULONG_MAX - cnt) < 1U) {
		alloc_cnt = ULONG_MAX;
	} else {
		alloc_cnt = cnt + 1U;
	}
	/* DP no.13 */
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	hpd_buff = devm_kzalloc(ctx->dev, alloc_cnt, GFP_KERNEL);

	if (hpd_buff == NULL) {
		writed = -ENOMEM;
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		writed = simple_write_to_buffer(hpd_buff, cnt, off_set, buffer, cnt);
		writed = (writed < 0) ? 0 : writed;
		if ((size_t)writed != cnt) {
			devm_kfree(ctx->dev, hpd_buff);
			writed = -EIO;
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		hpd_buff[cnt] = '\0';
		ret = kstrtoint(hpd_buff, 10, &hpd);
		devm_kfree(ctx->dev, hpd_buff);
		if (ret < 0) {
			writed = -EINVAL;
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		switch (hpd) {
		case 0:
			ctx->manual_hpd = connector_status_disconnected;
			break;
		case 1:
			ctx->manual_hpd = connector_status_connected;
			break;
		case -1:
		default:
			ctx->manual_hpd = connector_status_unknown;
			break;
		}
		(void)drm_helper_hpd_irq_event(ctx->encoder.dev);
	}
	return writed;
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t proc_read_edid(
	struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	/* DP no.15 */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_dpi *ctx = (const struct tcc_dpi *)PDE_DATA(file_inode(filp));
	const struct drm_crtc *crtc = (ctx != NULL) ? ctx->encoder.crtc : NULL;
	bool internal_ok = (bool)true;
	ssize_t bytes_read = 0;
	unsigned int u;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter cnt is not used
	 * in the function.
	 */
	(void)cnt;
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter off_set is not used
	 * in the function.
	 */
	(void)off_set;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter usr_buf is not used
	 * in the function.
	 */
	(void)usr_buf;

	if (crtc != NULL) {
		const struct drm_property_blob *edid_blob = NULL;
		const struct drm_connector *connector =
			(const struct drm_connector *)tcc_dpi_find_connector_from_crtc(
				(const struct drm_crtc *)crtc);
		const unsigned char *data;

		if (connector == NULL) {
			internal_ok =  (bool)false;
		}

		if (internal_ok) {
			edid_blob = (const struct drm_property_blob *)connector->edid_blob_ptr;
			if (edid_blob == NULL) {
				internal_ok =  (bool)false;
			}
		}
		if (internal_ok) {
			/* DP no.15 */
			/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
			data = edid_blob->data;
			if (data == NULL) {
				internal_ok =  (bool)false;
			}
		}
		if (internal_ok) {
			(void)pr_info(
				"[INFO][%s] CRTC_ID[%ud] length = %zu",
				LOG_TAG, drm_crtc_index(crtc),
				edid_blob->length);
			for (u = 0U; u < edid_blob->length; u += 8U) {
				(void)pr_info(
					"%02x %02x %02x %02x %02x %02x %02x %02x\r\n",
					data[u+0U],
					data[u+1U],
					data[u+2U],
					data[u+3U],
					data[u+4U],
					data[u+5U],
					data[u+6U],
					data[u+7U]);
			}
			bytes_read = (ssize_t)edid_blob->length;
		}
	}
	return bytes_read;
}

static const struct file_operations proc_fops_hpd = {
	.owner   = THIS_MODULE,
	.open    = proc_open,
	.release = proc_close,
	.write   = proc_write_hpd,
};

static const struct file_operations proc_fops_edid = {
	.owner   = THIS_MODULE,
	.open    = proc_open,
	.release = proc_close,
	.read	 = proc_read_edid,
};
#endif

struct drm_encoder *tcc_dpi_probe(
	struct device *dev, struct tcc_hw_device *hw_device)
{
	bool internal_ok = (bool)true;
	struct tcc_dpi *ctx;
	struct drm_encoder *encoder = NULL;

	#if defined(CONFIG_DRM_TCC_DPI_PROC)
	char proc_name[255];
	const char *drm_name;
	size_t drm_name_len;
	#endif

	/* DP no.13 */
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (ctx == NULL) {
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		ctx->timings = NULL;
		ctx->panel = NULL;
		ctx->dev = dev;
		ctx->hw_device = hw_device;
		#if defined(CONFIG_DRM_TCC_DPI_PROC)
		ctx->manual_hpd = connector_status_unknown;
		#endif
		(void)tcc_dpi_parse_dt(ctx);
		#if defined(CONFIG_ARCH_TCC805X)
		ctx->panel_node = of_graph_get_remote_node(dev->of_node, 0, 0);
		if (ctx->panel_node != NULL) {
			ctx->panel = of_drm_find_panel(ctx->panel_node);
		} else {
			dev_err(
				dev,
				"[ERROR][%s] %s failed to get panel\r\n",
				LOG_TAG, __func__);
		}
		if (ctx->panel != NULL) {
			dev_info(
				ctx->panel->dev, "[INFO][%s] %s has DRM panel\r\n",
				LOG_TAG, __func__);
		}
		#endif
	}

	#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
	if (internal_ok) {
		if (ctx->hw_device->connector_type == DRM_MODE_CONNECTOR_DisplayPort) {
			/* DP no.13 */
			/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
			/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
			ctx->dp = devm_kzalloc(dev, sizeof(*ctx->dp), GFP_KERNEL);
			if (ctx->dp == NULL) {
				internal_ok = (bool)false;
			} else {
				ctx->dp->dp_id =
					tcc_dp_register_drm(
						&ctx->encoder,
						&tcc_dp_callback_funcs);
			}
		}
	}
	#endif

	#if defined(CONFIG_DRM_TCC_DPI_PROC)
	if (internal_ok) {
		drm_name = strrchr(dev_name(dev), (int)'-');
		if (drm_name == NULL) {
			drm_name = dev_name(dev);
		} else {
			drm_name++;
		}
		drm_name_len = strlen(drm_name);

		/* INT30-C */
		if (((ULONG_MAX - drm_name_len) < 4U) ||
		    ((drm_name_len + 4U) > 255U)) {
			internal_ok = (bool)false;
		} else {
			(void)scnprintf(proc_name, 255, "dpi_%s", drm_name);
		}
	}
	if (internal_ok) {
		ctx->proc_dir = proc_mkdir(proc_name, NULL);
		if (ctx->proc_dir != NULL) {
			/* DP no.23B */
			/* coverity[misra_c_2012_rule_7_1_violation : FALSE] */
			ctx->proc_hpd = proc_create_data(
				"hpd", (unsigned int)S_IFREG | 0444U,
				ctx->proc_dir, &proc_fops_hpd, ctx);
			if (ctx->proc_hpd == NULL) {
				dev_warn(
					ctx->dev,
					"[WARN][%s] %s: Could not create file system @ /%s/%s/hpd\r\n",
					LOG_TAG, __func__, proc_name,
					dev_name(dev));
			}
			/* DP no.23B */
			/* coverity[misra_c_2012_rule_7_1_violation : FALSE] */
			ctx->proc_edid = proc_create_data(
				"edid", (unsigned int)S_IFREG | 0555U,
				ctx->proc_dir, &proc_fops_edid, ctx);
			if (ctx->proc_edid == NULL) {
				dev_warn(
					ctx->dev,
					"[WARN][%s] %s: Cofd not create file system @ /%s/%s/edid\r\n",
					LOG_TAG, __func__, proc_name,
					dev_name(dev));
			}
		} else {
			dev_warn(
				ctx->dev,
				"[WARN][%s] %s: Could not create file system @ %s\r\n",
				LOG_TAG, __func__, proc_name);
		}
	}
	#endif
	if (internal_ok) {
		encoder = &ctx->encoder;
	} else {
		if (ctx != NULL) {
			#if defined(CONFIG_DRM_TCC_DPI_PROC)
			if (ctx->proc_hpd != NULL) {
				proc_remove(ctx->proc_hpd);
			}
			if (ctx->proc_edid != NULL) {
				proc_remove(ctx->proc_edid);
			}
			if (ctx->proc_dir != NULL) {
				proc_remove(ctx->proc_dir);
			}
			#endif

			#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
			if (ctx->dp != NULL) {
				devm_kfree(dev, ctx->dp);
			}
			#endif
			devm_kfree(dev, ctx);
		}
	}

	return encoder;
}

int tcc_dpi_remove(struct drm_encoder *encoder)
{
	struct tcc_dpi *ctx = encoder_to_dpi(encoder);

	tcc_dpi_disable(&ctx->encoder);

	if (ctx->panel != NULL) {
		drm_panel_detach(ctx->panel);
	}

	#if defined(CONFIG_DRM_TCC_DPI_PROC)
	if (ctx->proc_hpd != NULL) {
		proc_remove(ctx->proc_hpd);
	}
	if (ctx->proc_dir != NULL) {
		proc_remove(ctx->proc_dir);
	}
	#endif

	devm_kfree(ctx->dev, ctx);
	return 0;
}

struct drm_encoder *tcc_dpi_find_encoder_from_crtc(const struct drm_crtc *crtc)
{
	struct drm_encoder *encoder = NULL;

	/* DP no.11 */
	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */	//
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */	//
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
			break;
		}
	return encoder;
}

struct drm_connector *tcc_dpi_find_connector_from_crtc(const struct drm_crtc *crtc)
{
	struct drm_connector *connector = NULL;
	struct drm_encoder *encoder;
	struct tcc_dpi *ctx;

	/* DP no.11 */
	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */	//
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */	//
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
			ctx = encoder_to_dpi(encoder);
			connector = &ctx->connector;
		}
	return connector;
}


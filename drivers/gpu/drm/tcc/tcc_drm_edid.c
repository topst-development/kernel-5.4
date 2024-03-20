// SPDX-License-Identifier: GPL-2.0-or-later

/**************************************************************************
 * Copyright (C) 2020 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <drm/drmP.h>
#include <drm/drm_edid.h>

#include <linux/tcc_math.h>

#include <video/of_videomode.h>
#include <video/videomode.h>

#include <tcc_drm_edid.h>

#define LOG_TAG "DRMEDID"

static const u8 base_edid[EDID_LENGTH] = {
	0x00U, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0x00U,
	0x50U, 0x63U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
	0x01U, 0x1EU, 0x01U, 0x03U, 0x80U, 0x50U, 0x2DU, 0x78U,
	0x1AU, 0x0DU, 0xC9U, 0xA0U, 0x57U, 0x47U, 0x98U, 0x27U,
	0x12U, 0x48U, 0x4CU, 0x00U, 0x00U, 0x00U, 0x01U, 0x01U,
	0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U,
	0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x70U, 0x17U,
	0x80U, 0x00U, 0x70U, 0xD0U, 0x00U, 0x20U, 0x00U, 0x00U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x18U,
	0x00U, 0x00U, 0x00U, 0xFCU, 0x00U, 0x42U, 0x4FU, 0x45U,
	0x20U, 0x57U, 0x4CU, 0x43U, 0x44U, 0x20U, 0x31U, 0x32U,
	0x2EU, 0x33U, 0x00U, 0x00U, 0x00U, 0x10U, 0x00U, 0x00U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x10U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
};

int tcc_make_edid_from_display_mode(
	struct edid *target_edid, const struct drm_display_mode *mode)
{
	struct detailed_pixel_timing *pixel_data;
	bool internal_ok = (bool)true;
	const u8 *raw_edid = NULL;
	int i, itmp, ret = 0;
	u8 csum;
	u32 blank, utmp;

	if (target_edid == NULL) {
		ret = -EINVAL;
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		raw_edid = (u8 *)target_edid;
		pixel_data = &target_edid->detailed_timings[0].data.pixel_data;

		/* Duplicate edid from base_edid */
		(void)memcpy(target_edid, base_edid, EDID_LENGTH);

		/* Set detailed timing information from display mode */
		if (mode->clock < 0) {
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		itmp = tcc_safe_div_round_up_int(mode->clock, 10);
		utmp = tcc_safe_int2uint(itmp);
		utmp &= 0xffffU;
		target_edid->detailed_timings[0].pixel_clock = cpu_to_le16((short)utmp);

		utmp = tcc_safe_int2uint(mode->hdisplay);
		pixel_data->hactive_lo = (u8)(utmp & 0xffU);

		itmp = mode->htotal - mode->hdisplay;
		if(itmp < 0) {
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		blank = tcc_safe_int2uint(itmp);
		pixel_data->hblank_lo = (u8)(blank & 0xffU);

		utmp = tcc_safe_int2uint(mode->hdisplay);
		pixel_data->hactive_hblank_hi = (u8)((utmp >> 4U) & 0xf0U);

		pixel_data->hactive_hblank_hi |= (u8)((blank >> 8U) & 0xfU);

		utmp = tcc_safe_int2uint(mode->vdisplay);
		pixel_data->vactive_lo = (u8)(utmp & 0xffU);

		itmp = mode->vtotal - mode->vdisplay;
		if(itmp < 0) {
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		blank = tcc_safe_int2uint(itmp);
		pixel_data->vblank_lo = (u8)(blank & 0xffU);

		utmp = tcc_safe_int2uint(mode->vdisplay);
		pixel_data->vactive_vblank_hi = (u8)((utmp >> 4U) & 0xf0U);
		pixel_data->vactive_vblank_hi |= (u8)((blank >> 8U) & 0xfU);

		itmp = mode->hsync_start - mode->hdisplay;
		if(itmp < 0) {
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		pixel_data->hsync_offset_lo =
			(u8)(tcc_safe_int2uint(itmp) & 0xffU);

		itmp = mode->hsync_end - mode->hsync_start;
		if(itmp < 0) {
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		pixel_data->hsync_pulse_width_lo =
			(u8)(tcc_safe_int2uint(itmp) & 0xffU);

		itmp = mode->vsync_start - mode->vdisplay;
		if(itmp < 0) {
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		pixel_data->vsync_offset_pulse_width_lo =
			(u8)(((tcc_safe_int2uint(itmp) & 0xfU) << 4U) & 0xffU);
		itmp = mode->vsync_end - mode->vsync_start;
		if(itmp < 0) {
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		pixel_data->vsync_offset_pulse_width_lo |=
			(u8)(tcc_safe_int2uint(itmp) & 0xfU);

		itmp = mode->hsync_start - mode->hdisplay;
		if(itmp < 0) {
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		pixel_data->hsync_vsync_offset_pulse_width_hi =
			(u8)(((tcc_safe_int2uint(itmp) >> 8U) << 6U) & 0xffU);

		itmp = mode->hsync_end - mode->hsync_start;
		if(itmp < 0) {
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		pixel_data->hsync_vsync_offset_pulse_width_hi |=
			(u8)(((tcc_safe_int2uint(itmp) >> 8U) << 4U) & 0xffU);

		itmp = mode->vsync_start - mode->vdisplay;
		if(itmp < 0) {
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		pixel_data->hsync_vsync_offset_pulse_width_hi |=
			(u8)(((tcc_safe_int2uint(itmp) >> 4U) << 2U) & 0xffU);

		itmp = mode->vsync_end - mode->vsync_start;
		if(itmp < 0) {
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		pixel_data->hsync_vsync_offset_pulse_width_hi |=
			(u8)((tcc_safe_int2uint(itmp) >> 4U) & 0xffU);

		pixel_data->width_mm_lo =
			(u8)(tcc_safe_int2uint(mode->width_mm) & 0xffU);
		pixel_data->height_mm_lo =
			(u8)(tcc_safe_int2uint(mode->height_mm) & 0xffU);
		pixel_data->width_height_mm_hi =
			(u8)(((tcc_safe_int2uint(mode->width_mm) >> 8U) << 4U) & 0xffU) |
			(u8)((tcc_safe_int2uint(mode->height_mm) >> 8U) & 0xffU);
		pixel_data->hborder = 0x0U;
		pixel_data->vborder = 0x0U;

		/* DP no.16 */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
		utmp = DRM_EDID_PT_SEPARATE_SYNC;
		pixel_data->misc = (u8)(utmp & 0xffU);

		/* DP no.16 */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
		utmp = DRM_MODE_FLAG_PHSYNC;
		if ((mode->flags & utmp) == utmp) {
			/* DP no.16 */
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
			utmp = DRM_EDID_PT_HSYNC_POSITIVE;
			pixel_data->misc |= (u8)(utmp & 0xffU);
		}
		/* DP no.16 */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
		utmp = DRM_MODE_FLAG_PVSYNC;
		if ((mode->flags & utmp) == utmp) {
			/* DP no.16 */
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
			utmp = DRM_EDID_PT_VSYNC_POSITIVE;
			pixel_data->misc |= (u8)(utmp & 0xffU);
		}

		/* Checksum */
		csum = 0;
		for (i = 0; i < EDID_LENGTH; i++) {
			utmp = tcc_safe_uint_pluse(csum, raw_edid[i]);
			csum = (u8)(utmp & 0xffU);
		}

		if (csum == (u8)0U) {
			target_edid->checksum = 1;
		} else {
			target_edid->checksum = ((u8)0xFFU - csum)+ (u8)1U;
		}
	}

	return ret;
}
/* DP no.5 */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL_GPL(tcc_make_edid_from_display_mode);

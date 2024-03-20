/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * tcc_drm_crtc.h
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

#ifndef TCC_DRM_CRTC_H
#define TCC_DRM_CRTC_H


#include "tcc_drm_drv.h"

#define CRTC_FLAGS_IRQ_BIT	0
#define CRTC_FLAGS_VCLK_BIT	1 /* Display BUS - pwdn/swreset for VIOC*/
#define CRTC_FLAGS_PCLK_BIT	2 /* Display BUS - Pixel Clock for LCDn (0-3) */

/* Display has totally four hardware windows. */
#define CRTC_WIN_NR_MAX		4


struct tcc_drm_crtc_ops;
/*
 * TCC specific crtc structure.
 *
 * @base: crtc object.
 * @type: one of TCC_DISPLAY_TYPE_LCD and HDMI.
 * @ops: pointer to callbacks for tcc drm specific functionality
 * @ctx: A pointer to the crtc's implementation specific context
 * @pipe_clk: A pointer to the crtc's pipeline clock.
 */
struct tcc_drm_crtc {
	struct drm_crtc base;
	enum tcc_drm_output_type type;
	const struct tcc_drm_crtc_ops *ops;
	void *context;

	/* support to fence */
	atomic_t flip_status;
	struct drm_pending_vblank_event *flip_event;
	bool flip_async;
	/* Whether the CRTC enabled - 0 disabled, 1 enabled */
	int enabled;
};

/*
 * TCC drm crtc ops
 *
 * @enable: enable the device
 * @disable: disable the device
 * @enable_vblank: specific driver callback for enabling vblank interrupt.
 * @disable_vblank: specific driver callback for disabling vblank interrupt.
 * @mode_valid: specific driver callback for mode validation
 * @atomic_check: validate state
 * @atomic_begin: prepare device to receive an update
 * @atomic_flush: mark the end of device update
 * @update_plane: apply hardware specific overlay data to registers.
 * @disable_plane: disable hardware specific overlay.
 * @te_handler: trigger to transfer video image at the tearing effect
 *	synchronization signal if there is a page flip request.
 */
struct tcc_drm_crtc_ops {
	void (*enable)(struct tcc_drm_crtc *crtc);
	void (*disable)(struct tcc_drm_crtc *crtc);
	int (*enable_vblank)(struct tcc_drm_crtc *crtc);
	void (*disable_vblank)(struct tcc_drm_crtc *crtc);
	u32 (*get_vblank_counter)(struct tcc_drm_crtc *crtc);
	enum drm_mode_status (*mode_valid)(struct tcc_drm_crtc *crtc,
		const struct drm_display_mode *mode);
	void (*mode_set_nofb)(struct tcc_drm_crtc *crtc);
	int (*atomic_check)(struct tcc_drm_crtc *crtc,
			    struct drm_crtc_state *state);
	void (*atomic_begin)(struct tcc_drm_crtc *crtc);
	void (*update_plane)(struct tcc_drm_crtc *crtc,
			     struct tcc_drm_plane *plane);
	void (*disable_plane)(struct tcc_drm_crtc *crtc,
			      struct tcc_drm_plane *plane);
	void (*atomic_flush)(struct tcc_drm_crtc *crtc);
	#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
	int (*get_chromakey)(struct tcc_drm_crtc *crtc,
		unsigned int layer,
		unsigned int *enable,
		struct drm_chromakey_t *value,
		struct drm_chromakey_t *mask);
	int (*set_chromakey)(struct tcc_drm_crtc *crtc,
		unsigned int layer,
		unsigned int enable,
		struct drm_chromakey_t *value,
		struct drm_chromakey_t *mask);
	#endif
};

static inline struct tcc_drm_crtc *to_tcc_crtc(struct drm_crtc *crtc) {
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
	return (struct tcc_drm_crtc *)container_of(crtc,
						   struct tcc_drm_crtc, base);
}

struct tcc_drm_crtc *tcc_drm_crtc_create(struct drm_device *drm_dev,
					struct drm_plane *primary,
					struct drm_plane *cursor,
					enum tcc_drm_output_type type,
					const struct tcc_drm_crtc_ops *ops,
					void *context);

/* This function gets crtc device matched with out_type. */
int tcc_drm_set_possible_crtcs(struct drm_encoder *encoder,
		enum tcc_drm_output_type out_type);

void tcc_drm_crtc_vblank_handler(struct drm_crtc *crtc);
void tcc_crtc_handle_event(struct tcc_drm_crtc *tcc_crtc);
int tcc_crtc_parse_edid_ioctl(
	struct drm_device *dev, void *data, struct drm_file *infile);
int tcc_drm_crtc_set_display_timing(struct drm_crtc *crtc,
				     const struct drm_display_mode *mode,
				     const struct tcc_hw_device *hw_data);
#endif

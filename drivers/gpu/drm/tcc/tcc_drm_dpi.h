/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 *
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
#ifndef TCC_DRM_DPI_HEADER
#define TCC_DRM_DPI_HEADER

#if defined(CONFIG_TCC_DP_DRIVER_V1_4)
#include <include/dptx_drm.h>
struct tcc_drm_dp_callback_funcs {
	int (*attach)(struct drm_encoder *encoder, int dp_id, int flags);
	int (*detach)(struct drm_encoder *encoder, int dp_id, int flags);
	int (*register_helper_funcs)(
		struct drm_encoder *encoder,
		struct dptx_drm_helper_funcs *dptx_ops);
};
#endif
struct drm_encoder *tcc_dpi_find_encoder_from_crtc(const struct drm_crtc *crtc);
struct drm_connector *tcc_dpi_find_connector_from_crtc(const struct drm_crtc *crtc);

#include <tcc_drm_address.h>
#ifdef CONFIG_DRM_TCC_DPI
struct drm_encoder *tcc_dpi_probe(
	struct device *dev, struct tcc_hw_device *hw_device);
int tcc_dpi_remove(struct drm_encoder *encoder);
int tcc_dpi_bind(
	struct drm_device *dev, struct drm_encoder *encoder,
	const struct tcc_hw_device *hw_data);
#else
static inline struct drm_encoder *
tcc_dpi_probe(
	struct device *dev, struct tcc_hw_device *hw_device)
{
	(void)dev;
	(void)hw_device;

	return NULL;
}

static inline int tcc_dpi_remove(struct drm_encoder *encoder)
{
	(void)encoder;

	return 0;
}
static inline int tcc_dpi_bind(
	struct drm_device *dev, struct drm_encoder *encoder,
	const struct tcc_hw_device *hw_data)
{
	(void)dev;
	(void)encoder;
	(void)hw_device;

	return 0;
}
#endif
#endif

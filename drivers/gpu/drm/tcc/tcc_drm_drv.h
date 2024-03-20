/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * tcc_drm_drv.h
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

#ifndef _TCC_DRM_DRV_H_
#define _TCC_DRM_DRV_H_

#include <drm/drmP.h>
#include <linux/module.h>
#include "tcc_drm_address.h"

#define MAX_CRTC	3
#define MAX_PLANE	4
#define MAX_FB_BUFFER	3

#define DEFAULT_WIN	0

#define to_tcc_plane(x)	container_of(x, struct tcc_drm_plane, base)

struct tcc_drm_rect {
	unsigned int x, y;
	unsigned int w, h;
};

/*
 * TCC drm plane state structure.
 *
 * @base: plane_state object (contains drm_framebuffer pointer)
 * @src: rectangle of the source image data to be displayed (clipped to
 *       visible part).
 * @crtc: rectangle of the target image position on hardware screen
 *       (clipped to visible part).
 * @h_ratio: horizontal scaling ratio, 16.16 fixed point
 * @v_ratio: vertical scaling ratio, 16.16 fixed point
 *
 * this structure consists plane state data that will be applied to hardware
 * specific overlay info.
 */

struct tcc_drm_plane_state {
	struct drm_plane_state base;
	struct tcc_drm_rect crtc;
	struct tcc_drm_rect src;
	unsigned int h_ratio;
	unsigned int v_ratio;
};

static inline struct tcc_drm_plane_state *
to_tcc_plane_state(struct drm_plane_state *state)
{
	return container_of(state, struct tcc_drm_plane_state, base);
}

/*
 * TCC drm common overlay structure.
 *
 * @base: plane object
 * @index: hardware index of the overlay layer
 *
 * this structure is common to tcc SoC and its contents would be copied
 * to hardware specific overlay info.
 */

struct tcc_drm_plane {
	struct drm_plane base;
	unsigned int win;
	unsigned long caps;
	unsigned long flags;
};

#define TCC_DRM_PLANE_CAP_DOUBLE	(1 << 0)
#define TCC_DRM_PLANE_CAP_SCALE		(1 << 1)
#define TCC_DRM_PLANE_CAP_ZPOS		(1 << 2)
#define TCC_DRM_PLANE_CAP_TILE		(1 << 3)

#define TCC_DRM_PLANE_CAP_FBDC		(1 << 4)

/* plane flgas */
#define TCC_PLANE_FLAG_FBDC_PLUGGED 	(1 << 0)

/*
 * TCC DRM plane configuration structure.
 *
 * @zpos: initial z-position of the plane.
 * @type: type of the plane (primary, cursor or overlay).
 * @pixel_formats: supported pixel formats.
 * @num_pixel_formats: number of elements in 'pixel_formats'.
 * @capabilities: supported features (see TCC_DRM_PLANE_CAP_*)
 * @virt_addr: address of telechips rdma node register
 */

struct tcc_drm_plane_config {
	unsigned int zpos;
	enum drm_plane_type type;
	const uint32_t *pixel_formats;
	unsigned int num_pixel_formats;
	unsigned int capabilities;

	void __iomem *virt_addr;
};


#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
struct drm_chromakey_t {
	unsigned int red;
	unsigned int green;
	unsigned int blue;
};

struct drm_ioctl_chromakey_t {
	unsigned int crtc_index;
	unsigned int chromakey_layer;
	unsigned int chromakey_enable;
	struct drm_chromakey_t chromakey_value;
	struct drm_chromakey_t chromakey_mask;
};
#endif

struct tcc_drm_clk {
	void (*enable)(struct tcc_drm_clk *clk, bool enable);
};

struct drm_tcc_file_private {
	struct device *ipp_dev;
};

/*
 * TCC drm private structure.
 *
 * @da_start: start address to device address space.
 *	with iommu, device address space starts from this address
 *	otherwise default one.
 * @da_space_size: size of device address space.
 *	if 0 then default value is used for it.
 * @pending: the crtcs that have pending updates to finish
 * @lock: protect access to @pending
 * @wait: wait an atomic commit to finish
 */
struct tcc_drm_private {
	struct drm_fb_helper *fb_helper;
	struct drm_atomic_state *suspend_state;

	struct device *dma_dev;
	void *mapping;
	/*
	 * This is needed for devices that don't already have their own dma
	 * parameters structure, e.g. platform devices, and, if necessary, will
	 * be assigned to the 'struct device' during device initialisation. It
	 * should therefore never be accessed directly via this structure as
	 * this may not be the version of dma parameters in use.
	 */
	struct device_dma_parameters dma_parms;

	/* for atomic commit */
	u32 pending;
	spinlock_t lock;
	wait_queue_head_t wait;
};

static inline struct device *to_dma_dev(struct drm_device *dev)
{
	struct tcc_drm_private *priv = dev->dev_private;

	return priv->dma_dev;
}

extern struct platform_driver lcd_driver;
extern struct platform_driver ext_driver;
extern struct platform_driver third_driver;
extern struct platform_driver fourth_driver;
extern struct platform_driver screen_share_driver;
#endif

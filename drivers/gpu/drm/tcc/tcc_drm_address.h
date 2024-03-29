/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_DRM_ADDRESS_H
#define TCC_DRM_ADDRESS_H

#define RDMA_MAX_NUM 4

#define DRM_PLAME_TYPE_MASK     0xFFFFU
#define DRM_PLAME_TYPE_SHIFT    0U
#define DRM_PLAME_FLAG_MASK     0x0FFFU
#define DRM_PLAME_FLAG_SHIFT    16U

#define DRM_PLANE_TYPE(x)	\
	(((uint32_t)(x) >> DRM_PLAME_TYPE_SHIFT) & DRM_PLAME_TYPE_MASK)
#define DRM_PLANE_FLAG(x) 	\
	(((uint32_t)(x) >> DRM_PLAME_FLAG_SHIFT) & DRM_PLAME_FLAG_MASK)

#define DRM_PLANE_FLAG_NONE ((uint32_t)0x00U <<  DRM_PLAME_FLAG_SHIFT)
#define DRM_PLANE_FLAG_TRANSPARENT \
	((uint32_t)0x01U <<  DRM_PLAME_FLAG_SHIFT)
#define DRM_PLANE_FLAG_SKIP_YUV_FORMAT \
	((uint32_t)0x02U <<  DRM_PLAME_FLAG_SHIFT)
#define DRM_PLANE_FLAG_NOT_DEFINED ((uint32_t)0x800U <<  DRM_PLAME_FLAG_SHIFT)

enum {
	TCC_DRM_DT_VERSION_OLD = 0,
	TCC_DRM_DT_VERSION_1_0,
};

/* this enumerates display type. */
enum tcc_drm_output_type {
	TCC_DISPLAY_TYPE_NONE,
	/* Primary display Interface. */
	TCC_DISPLAY_TYPE_LCD,
	/* Second display Interface. */
	TCC_DISPLAY_TYPE_EXT,
	/* Third display Interface. */
	TCC_DISPLAY_TYPE_THIRD,
	/* Foutrh display Interface. */
	TCC_DISPLAY_TYPE_FOURTH,
	/* Screen shared display Interface. */
	TCC_DISPLAY_TYPE_SCREEN_SHARE,
};

struct tcc_drm_device_data {
	unsigned long version;
	enum tcc_drm_output_type output_type;
	char *name;
};

struct tcc_hw_block {
	void __iomem *virt_addr;
	unsigned int irq_num;
	unsigned int blk_num;
};

struct tcc_hw_device {
	//unsigned long version;
	//enum tcc_drm_output_type output_type;
	struct clk *vioc_clock;
	struct clk *ddc_clock;
	struct tcc_hw_block display_device;
	struct tcc_hw_block wmixer;
	struct tcc_hw_block wdma;
	struct tcc_hw_block fbdc[RDMA_MAX_NUM];
	struct tcc_hw_block rdma[RDMA_MAX_NUM];
	unsigned int rdma_plane_type[RDMA_MAX_NUM];

	/* video identification code */
	int vic;
	int connector_type;
	/* rdma valid counts */
	int rdma_counts;

	/* DDIBUS LCD MUX */
	u32 lcdc_mux;
};

extern int tcc_drm_address_dt_parse(struct platform_device *pdev,
					struct tcc_hw_device *hw_data,
					unsigned long version);

#endif

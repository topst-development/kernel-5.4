// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *      tccvin_video.c  --  Telechips Video-Input Path Driver
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2020


 *   Description : Video handling


 *****************************************************************************/

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_graph.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/unaligned.h>

#include <media/v4l2-common.h>
#include <media/videobuf2-dma-contig.h>

#include "tccvin_video.h"

struct tccvin_mbus_vin_format {
	unsigned int				mbus_fmt;
	unsigned int				data_format;
	unsigned int				data_order;
};

struct tccvin_format {
	unsigned int				pixelformat;
	unsigned int				guid;
};

/* ------------------------------------------------------------------------
 * helper macro
 */

#define IS_SET(value, mask)		(((unsigned int)(value) & (unsigned int)(mask)) != 0U)

/* ------------------------------------------------------------------------
 * DEFINITION
 */

static struct tccvin_mbus_vin_format	tccvin_mbus_vin_format_list[] = {
	{ .mbus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,	.data_format = FMT_YUV422_8BIT,		.data_order = ORDER_RGB },
	{ .mbus_fmt = MEDIA_BUS_FMT_UYVY8_1X16,	.data_format = FMT_YUV422_16BIT,	.data_order = ORDER_RGB },
	{ .mbus_fmt = MEDIA_BUS_FMT_Y8_1X8,	.data_format = FMT_YUV422_16BIT,	.data_order = ORDER_RBG },
	{ .mbus_fmt = MEDIA_BUS_FMT_Y10_1X10,	.data_format = FMT_YUV422_16BIT,	.data_order = ORDER_RBG },
	{ .mbus_fmt = MEDIA_BUS_FMT_Y12_1X12,	.data_format = FMT_YUV422_16BIT,	.data_order = ORDER_RBG },
};

#if 1
static struct tccvin_format tccvin_format_list[] = {
	/* RGB formats */
	{ .pixelformat = V4L2_PIX_FMT_RGB24,    .guid = VIOC_IMG_FMT_RGB888,    },
	{ .pixelformat = V4L2_PIX_FMT_RGB32,    .guid = VIOC_IMG_FMT_ARGB8888,  },

	/* sequential (YUV packed) */
	{ .pixelformat = V4L2_PIX_FMT_UYVY, .guid = VIOC_IMG_FMT_UYVY,  },
	{ .pixelformat = V4L2_PIX_FMT_VYUY, .guid = VIOC_IMG_FMT_VYUY,  },
	{ .pixelformat = V4L2_PIX_FMT_YUYV, .guid = VIOC_IMG_FMT_YUYV,  },
	{ .pixelformat = V4L2_PIX_FMT_YVYU, .guid = VIOC_IMG_FMT_YVYU,  },

	/* two planes -- one Y, one Cr + Cb interleaved  */
	{ .pixelformat = V4L2_PIX_FMT_NV12, .guid = VIOC_IMG_FMT_YUV420IL0, },
	{ .pixelformat = V4L2_PIX_FMT_NV21, .guid = VIOC_IMG_FMT_YUV420IL1, },
	{ .pixelformat = V4L2_PIX_FMT_NV16, .guid = VIOC_IMG_FMT_YUV422IL0, },
	{ .pixelformat = V4L2_PIX_FMT_NV61, .guid = VIOC_IMG_FMT_YUV422IL1, },

	/* two non contiguous planes - one Y, one Cr + Cb interleaved  */
	{ .pixelformat = V4L2_PIX_FMT_NV12M,    .guid = VIOC_IMG_FMT_YUV420IL0, },
	{ .pixelformat = V4L2_PIX_FMT_NV21M,    .guid = VIOC_IMG_FMT_YUV420IL1, },
	{ .pixelformat = V4L2_PIX_FMT_NV16M,    .guid = VIOC_IMG_FMT_YUV422IL0, },
	{ .pixelformat = V4L2_PIX_FMT_NV61M,    .guid = VIOC_IMG_FMT_YUV422IL1, },

	/* three planes - Y Cb, Cr */
	{ .pixelformat = V4L2_PIX_FMT_YUV420,   .guid = VIOC_IMG_FMT_YUV420SEP, },
	{ .pixelformat = V4L2_PIX_FMT_YUV422P,  .guid = VIOC_IMG_FMT_YUV422SEP, },

	/* three non contiguous planes - Y, Cb, Cr */
	{ .pixelformat = V4L2_PIX_FMT_YUV420M,  .guid = VIOC_IMG_FMT_YUV420SEP, },
	{ .pixelformat = V4L2_PIX_FMT_YUV422M,  .guid = VIOC_IMG_FMT_YUV422SEP, },

};
#else
/* supported color formats */
static struct tccvin_format tccvin_format_list[] = {
	/* RGB */
	{ .pixelformat = V4L2_PIX_FMT_RGB24,	.guid = VIOC_IMG_FMT_RGB888,	},
	{ .pixelformat = V4L2_PIX_FMT_RGB32,	.guid = VIOC_IMG_FMT_ARGB8888,	},

	/* sequential (YUV packed) */
	{ .pixelformat = V4L2_PIX_FMT_UYVY,	.guid = VIOC_IMG_FMT_UYVY,	},
	{ .pixelformat = V4L2_PIX_FMT_VYUY,	.guid = VIOC_IMG_FMT_VYUY,	},
	{ .pixelformat = V4L2_PIX_FMT_YUYV,	.guid = VIOC_IMG_FMT_YUYV,	},
	{ .pixelformat = V4L2_PIX_FMT_YVYU,	.guid = VIOC_IMG_FMT_YVYU,	},

	/* interleaved (Y planar, UV planar) */
	{ .pixelformat = V4L2_PIX_FMT_NV12,	.guid = VIOC_IMG_FMT_YUV420IL0,	},
	{ .pixelformat = V4L2_PIX_FMT_NV21,	.guid = VIOC_IMG_FMT_YUV420IL1,	},
	{ .pixelformat = V4L2_PIX_FMT_NV16,	.guid = VIOC_IMG_FMT_YUV422IL0,	},
	{ .pixelformat = V4L2_PIX_FMT_NV61,	.guid = VIOC_IMG_FMT_YUV422IL1,	},

	/* separated (Y, U, V planar) */
	{ .pixelformat = V4L2_PIX_FMT_YVU420,	.guid = VIOC_IMG_FMT_YUV420SEP,	},
	{ .pixelformat = V4L2_PIX_FMT_YUV420,	.guid = VIOC_IMG_FMT_YUV420SEP,	},
	{ .pixelformat = V4L2_PIX_FMT_YUV422P,	.guid = VIOC_IMG_FMT_YUV422SEP,	},
};
#endif

static void tccvin_get_vs_info_format_by_mbus_format(const struct tccvin_stream *vstream,
	unsigned int mbus_pixelcode, struct tccvin_vs_info *vs_fmt)
{
	const struct device			*dev		= NULL;
	const struct tccvin_mbus_vin_format	*format		= NULL;
	unsigned int				idxList		= 0;
	unsigned int				nList		= 0;

	dev		= stream_to_device(vstream);

	/* default format */
	vs_fmt->data_format	= FMT_YUV422_8BIT;
	vs_fmt->data_order	= ORDER_RGB;

	/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	nList = ARRAY_SIZE(tccvin_mbus_vin_format_list);
	for (idxList = 0; idxList < nList; idxList++) {
		format = &tccvin_mbus_vin_format_list[idxList];
		if (mbus_pixelcode == format->mbus_fmt) {
			vs_fmt->data_format	= format->data_format;
			vs_fmt->data_order	= format->data_order;
		}
	}
}

static unsigned int tccvin_get_data_enable_pol_by_mbus_conf(unsigned int conf)
{
	return IS_SET(conf, V4L2_MBUS_DATA_ACTIVE_HIGH)
		? (unsigned int)DE_ACTIVE_HIGH
		: (unsigned int)DE_ACTIVE_LOW;
}

static unsigned int tccvin_get_vsync_pol_by_mbus_conf(unsigned int conf)
{
	return IS_SET(conf, V4L2_MBUS_VSYNC_ACTIVE_HIGH)
		? (unsigned int)VS_ACTIVE_HIGH
		: (unsigned int)VS_ACTIVE_LOW;
}

static unsigned int tccvin_get_hsync_pol_by_mbus_conf(unsigned int conf)
{
	return IS_SET(conf, V4L2_MBUS_HSYNC_ACTIVE_HIGH)
		? (unsigned int)HS_ACTIVE_HIGH
		: (unsigned int)HS_ACTIVE_LOW;
}

static unsigned int tccvin_get_pclk_pol_by_mbus_conf(unsigned int conf)
{
	return IS_SET(conf, V4L2_MBUS_PCLK_SAMPLE_RISING)
		? (unsigned int)PCLK_ACTIVE_HIGH
		: (unsigned int)PCLK_ACTIVE_LOW;
}

static unsigned int tccvin_get_embedded_sync_by_mbus_conf(enum v4l2_mbus_type conf)
{
	return IS_SET(conf, V4L2_MBUS_BT656) ? 1U : 0U;
}

static const struct tccvin_format *tccvin_get_tccvin_format_by_pixelformat(unsigned int pixelformat)
{
	unsigned int				idxList		= 0;
	unsigned int				nList		= 0;
	const struct tccvin_format		*tformat	= NULL;

	/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	nList = ARRAY_SIZE(tccvin_format_list);
	for (idxList = 0; idxList < nList; idxList++) {
		tformat = &tccvin_format_list[idxList];
		if (pixelformat == tformat->pixelformat) {
			/* matched */
			break;
		}
	}

	return tformat;
}

static bool tccvin_handover_flagged(unsigned int flags, unsigned int mask)
{
	return (IS_SET(flags, V4L2_CAP_CTRL_SKIP_ALL) ||
		IS_SET(flags, mask));
}

static void tccvin_print_handover_flags(const struct tccvin_stream *vstream,
	unsigned int flags)
{
	const struct device			*dev		= NULL;

	dev		= stream_to_device(vstream);

	switch (flags) {
	case (unsigned int)V4L2_CAP_CTRL_SKIP_NONE:
		logi(dev, "V4L2_CAP_CTRL_SKIP_NONE\n");
		break;
	case (unsigned int)V4L2_CAP_CTRL_SKIP_ALL:
		logi(dev, "V4L2_CAP_CTRL_SKIP_ALL\n");
		break;
	case (unsigned int)V4L2_CAP_CTRL_SKIP_SUBDEV:
		logi(dev, "V4L2_CAP_CTRL_SKIP_SUBDEV\n");
		break;
	case (unsigned int)V4L2_CAP_CTRL_SKIP_DEV:
		logi(dev, "V4L2_CAP_CTRL_SKIP_DEV\n");
		break;
	default:
		loge(dev, "flags(0x%08x) is wrong\n", flags);
		break;
	}
}

static int tccvin_get_clock(struct tccvin_stream *vstream,
	struct device_node *dev_node)
{
	const struct device			*dev		= NULL;
	int					ret		= 0;

	dev		= stream_to_device(vstream);

	/* get the ddi clock */
	vstream->cif.vioc_clk = of_clk_get(dev_node, 0);
	if (vstream->cif.vioc_clk == NULL) {
		loge(dev, "of_clk_get\n");
		ret = -ENODEV;
	}

	return ret;
}

static void tccvin_put_clock(const struct tccvin_stream *vstream)
{
	clk_put(vstream->cif.vioc_clk);
}

static int tccvin_enable_clock(const struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	int					ret		= 0;

	dev		= stream_to_device(vstream);

	ret = clk_prepare_enable(vstream->cif.vioc_clk);
	if (ret != 0) {
		/* failure of clk_prepare_enable */
		loge(dev, "clk_prepare_enable, ret: %d\n", ret);
	}

	return ret;
}

static void tccvin_disable_clock(const struct tccvin_stream *vstream)
{
	clk_disable_unprepare(vstream->cif.vioc_clk);
}

/*
 * tccvin_parse_ddibus
 *
 * - DESCRIPTION:
 *	Parse video-input path's device tree
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 *	-ENODEV:	a certain device node is not found.
 */
/* coverity[HIS_metric_violation : FALSE] */
static int tccvin_parse_ddibus(struct tccvin_stream *vstream,
	const struct device_node *dev_node)
{
	const struct device			*dev		= NULL;
	struct device_node			*vioc_node	= NULL;
	struct vioc_comp			*vin_path	= NULL;
	struct vioc_comp			*pvioc		= NULL;
	const char				*const name_list[VIN_COMP_MAX] = {
		[VIN_COMP_VIN]		= "vin",
		[VIN_COMP_VIQE]		= "viqe",
		[VIN_COMP_SDEINTL]	= "deintls",
		[VIN_COMP_SCALER]	= "scaler",
		[VIN_COMP_PGL]		= "rdma",
		[VIN_COMP_WMIX]		= "wmixer",
		[VIN_COMP_WDMA]		= "wdma",
	};
	const char				*name		= NULL;
	unsigned int				idxVioc		= 0;
	unsigned int				prop_val	= 0;
	unsigned int				vioc_id		= 0;
	int					ret		= 0;

	dev		= stream_to_device(vstream);
	vin_path	= vstream->cif.vin_path;

	/* cif port */
	vioc_node = of_parse_phandle(dev_node, "cifport", 0);
	if (!IS_ERR_OR_NULL(vioc_node)) {
		(void)of_property_read_u32_index(dev_node, "cifport", 1, &prop_val);
		vstream->cif.cif_port = prop_val;
		vstream->cif.cifport_addr = of_iomap(vioc_node, 0);
		logd(dev, "%10s: %u\n", "CIF Port", vstream->cif.cif_port);
	} else {
		loge(dev, "\"cifport\" node is not found.\n");
		ret = -ENODEV;
	}

	for (idxVioc = 0; idxVioc < (unsigned int)VIN_COMP_MAX; idxVioc++) {
		name = name_list[idxVioc];
		vioc_node = of_parse_phandle(dev_node, name, 0);
		if (!IS_ERR_OR_NULL(vioc_node)) {
			pvioc = &vin_path[idxVioc];

			/* vioc node */
			pvioc->np = vioc_node;

			/* vioc index */
			(void)of_property_read_u32_index(dev_node, name, 1, &prop_val);
			pvioc->index = prop_val;

			/*get vioc id */
			if (idxVioc == (unsigned int)VIN_COMP_VIN) {
				/* if vin, index / 2 */
				vioc_id	= get_vioc_index(pvioc->index) / 2U;
			} else {
				/* if not vin, index as it is */
				vioc_id	= get_vioc_index(pvioc->index);
			}
			logd(dev, "%10s: %u\n", name, vioc_id);

			/* extra property */
			if (idxVioc == (unsigned int)VIN_COMP_PGL) {
				/* Parking Guide Line */
				(void)of_property_read_u32_index(dev_node, "use_pgl", 0, &prop_val);
				vstream->cif.use_pgl = prop_val;
				logd(dev, "%10s: %u\n", "use_pgl", vstream->cif.use_pgl);
			}

			/* vioc interrupt */
			if ((idxVioc == (unsigned int)VIN_COMP_VIN) ||
			    (idxVioc == (unsigned int)VIN_COMP_WDMA)) {
				prop_val = irq_of_parse_and_map(vioc_node, (int)vioc_id);
				pvioc->intr.num = u32_to_s32(prop_val);
				logd(dev, "%6s irq: %u\n", name, pvioc->intr.num);
			}
		} else {
			if ((idxVioc == (unsigned int)VIN_COMP_VIN) ||
			    (idxVioc == (unsigned int)VIN_COMP_WDMA)) {
				loge(dev, "\"%s\" node is not found.\n", name);
				ret = -ENODEV;
			}
		}
	}

	return ret;
}

static int tccvin_parse_reserved_memory(struct tccvin_stream *vstream,
	const struct device_node *dev_node)
{
	const struct device			*dev		= NULL;
	struct device_node			*mem_node	= NULL;
	/* the ordor of reserved memorys depends on enum reserved_memory */
	const char				*const name_list[RESERVED_MEM_MAX] = {
		[RESERVED_MEM_PGL]	= "pmap_pgl",
		[RESERVED_MEM_VIQE]	= "pmap_viqe",
		[RESERVED_MEM_PREV]	= "pmap_prev",
		[RESERVED_MEM_LFRAME]	= "pmap_lframe",
	};
	const char				*name		= NULL;
	unsigned int				idxMem		= 0;
	unsigned int				base		= 0;
	unsigned int				size		= 0;
	int					ret		= 0;

	dev		= stream_to_device(vstream);

	for (idxMem = 0; idxMem < (unsigned int)RESERVED_MEM_MAX; idxMem++) {
		mem_node = of_parse_phandle(dev_node, "memory-region", u32_to_s32(idxMem));
		name = name_list[idxMem];
		if (!IS_ERR_OR_NULL(mem_node)) {
			vstream->cif.rsvd_mem[idxMem] = of_reserved_mem_lookup(mem_node);
			if (!IS_ERR_OR_NULL(vstream->cif.rsvd_mem[idxMem])) {
#if defined(CONFIG_ARM64)
				base = u64_to_u32(vstream->cif.rsvd_mem[idxMem]->base);
				size = u64_to_u32(vstream->cif.rsvd_mem[idxMem]->size);
#else
				base = vstream->cif.rsvd_mem[idxMem]->base;
				size = vstream->cif.rsvd_mem[idxMem]->size;
#endif//defined(CONFIG_ARM64)
				logd(dev, "%20s: 0x%08x ~ 0x%08x (0x%08x)\n",
					name, base, base + size, size);
			}
		} else {
			logd(dev, "\"%s\" node is not found.\n", name);
			if (idxMem == (unsigned int)RESERVED_MEM_PREV) {
				/* error in case of RESERVED_MEM_PREV */
				ret = -1;
			}
		}
		of_node_put(mem_node);
	}

	return ret;
}

static int tccvin_parse_fwnode_expand(struct tccvin_stream *vstream,
	const struct fwnode_handle *fwnode)
{
	const struct device			*dev		= NULL;
	struct tccvin_vs_info			*vs_info	= NULL;
	int					ret		= 0;

	dev		= stream_to_device(vstream);
	vs_info		= &vstream->vs_info;

	if (vs_info != NULL) {
		struct tccvin_fwnode_property_map {
			char		*name;
			unsigned int	*val;
		};
		const struct tccvin_fwnode_property_map	prop_list[] = {
			{ "data-order",		&vs_info->data_order		},
			{ "data-format",	&vs_info->data_format		},
#if	defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			{ "stream-enable",	&vs_info->stream_enable		},
#endif	/* defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	 * defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	 */
			{ "gen-field-en",	&vs_info->gen_field_en		},
			{ "vs-mask",		&vs_info->vs_mask		},
			{ "hsde-connect-en",	&vs_info->hsde_connect_en	},
			{ "intpl-en",		&vs_info->intpl_en		},
#if	defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			{ "flush-vsync",	&vs_info->flush_vsync		},
#endif	/* defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	 * defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	 */
		};
		unsigned int				idxList		= 0;
		unsigned int				nList		= 0;
		u32					v;

		/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		nList = ARRAY_SIZE(prop_list);
		for (idxList = 0; idxList < nList; idxList++) {
			ret = fwnode_property_read_u32(fwnode, prop_list[idxList].name, &v);
			if (ret == 0) {
				/* update value */
				*prop_list[idxList].val = v;
			}
		}
	}

	return ret;
}

static int tccvin_parse_fwnode(struct tccvin_stream *vstream,
	const struct device_node *dev_node)
{
	const struct device			*dev		= NULL;
	struct device_node			*loc_ep		= NULL;
	struct fwnode_handle			*fwnode		= NULL;
	struct v4l2_fwnode_endpoint		fw_ep		= { 0, };
	int					ret_call	= 0;
	int					ret		= 0;

	dev		= stream_to_device(vstream);

	/* get phandle of subdev */
	loc_ep = of_graph_get_endpoint_by_regs(dev_node, -1, -1);
	if (loc_ep == NULL) {
		loge(dev, "No subdev is found\n");
		ret = -EINVAL;
	} else {
		/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		fwnode = of_fwnode_handle(loc_ep);

		fw_ep.bus_type	= V4L2_MBUS_UNKNOWN;
		ret_call = v4l2_fwnode_endpoint_parse(fwnode, &fw_ep);
		if (ret_call != 0) {
			loge(dev, "v4l2_fwnode_endpoint_parse, ret: %d\n", ret_call);
			ret = -1;
		}

		ret_call = tccvin_parse_fwnode_expand(vstream, fwnode);
		if (ret_call != 0) {
			loge(dev, "tccvin_parse_fwnode_expand, ret: %d\n", ret_call);
			ret = -1;
		}
	};

	return ret;
}

static bool tccvin_video_is_framesize_supported(const struct tccvin_stream *vstream,
	unsigned int width, unsigned int height)
{
	const struct device			*dev		= NULL;
	bool					ret		= TRUE;

	dev		= stream_to_device(vstream);

	logd(dev, "frmaesize(%u * %u)\n", width, height);

	if ((width == 0U) || (height == 0U)) {
		loge(dev, "width or height is 0\n");
		ret = FALSE;
	}

	if ((width * height) >= (MAX_FRAMEWIDTH * MAX_FRAMEHEIGHT)) {
		loge(dev, "frmaesize(%u * %u) exceeds the maximum size(%u * %u)\n",
			width, height, MAX_FRAMEWIDTH, MAX_FRAMEHEIGHT);
		ret = FALSE;
	}

	return ret;
}

/*
 * tccvin_reset_vioc_path
 *
 * - DESCRIPTION:
 *	Reset or clear a certain vioc component.
 *	If vioc manager is enabled, try to ask to main core to reset or clear.
 *	If no response comes from main core, do it directly.
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 *	-1:		The vioc manager device is not found.
 */
static int tccvin_reset_vioc_path(const struct tccvin_stream *vstream)
{
	const struct vioc_comp			*vin_path	= NULL;
	int					idxVioc		= 0;
	int					ret		= 0;

	vin_path	= vstream->cif.vin_path;

	/* vioc reset */
	for (idxVioc = (int)VIN_COMP_MAX - 1; idxVioc >= 0; idxVioc--) {
		if (!IS_ERR_OR_NULL(vin_path[idxVioc].np)) {
			/* vioc reset */
			VIOC_CONFIG_SWReset(vin_path[idxVioc].index, VIOC_CONFIG_RESET);
		}
	}

	/* vioc reset clear */
	for (idxVioc = 0; idxVioc < (int)VIN_COMP_MAX; idxVioc++) {
		if (!IS_ERR_OR_NULL(vin_path[idxVioc].np)) {
			/* vioc reset clear */
			VIOC_CONFIG_SWReset(vin_path[idxVioc].index, VIOC_CONFIG_CLEAR);
		}
	}

	return ret;
}

/*
 * tccvin_map_cif_port
 *
 * - DESCRIPTION:
 *	Map cif port to receive video data
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
static int tccvin_map_cif_port(const struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	void __iomem				*addr		= NULL;
	unsigned int				mask		= 0xF;
	unsigned int				vin_index	= 0;
	unsigned int				value		= 0;

	dev		= stream_to_device(vstream);
	addr		= vstream->cif.cifport_addr;

	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	vin_index = (get_vioc_index(vstream->cif.vin_path[VIN_COMP_VIN].index) / 2U);
	value = ((__raw_readl(addr) & ~(mask << (vin_index * 4U))) |
		(vstream->cif.cif_port << (vin_index * 4U)));
	__raw_writel(value, addr);

	value = __raw_readl(addr);
	logd(dev, "CIF Port: %d, VIN Index: %d, Register Value: 0x%08x\n",
		vstream->cif.cif_port, vin_index, value);

	return 0;
}

#if defined(CONFIG_VIDEO_TCCVIN2_DIAG)
/*
 * tccvin_check_cif_port_mapping
 *
 * - DESCRIPTION:
 *	Check cif port mapping to receive video data
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 *	-1:		Failure
 */
static int tccvin_check_cif_port_mapping(struct tccvin_stream *vstream)
{
	struct device				*dev		= NULL;
	const void __iomem			*addr		= NULL;
	unsigned int				mask		= 0xF;
	unsigned int				vin_index	= 0;
	unsigned int				cif_port	= 0;
	int					ret		= 0;

	dev		= stream_to_device(vstream);
	addr		= vstream->cif.cifport_addr;

	vin_index = (get_vioc_index(vstream->cif.vin_path[VIN_COMP_VIN].index) / 2U);
	cif_port = ((__raw_readl(addr) >> (vin_index * 4U)) & mask);

	logi(dev, "cif port: %d -> %d\n", vstream->cif.cif_port, cif_port);
	if (cif_port != vstream->cif.cif_port) {
		loge(dev, "cif port: %d != %d\n", dev->cif.cif_port, cif_port);
		ret = -1;
	}

	return ret;
}
#endif//defined(CONFIG_VIDEO_TCCVIN2_DIAG)

#if defined(CONFIG_OVERLAY_PGL)
/*
 * tccvin_set_pgl
 *
 * - DESCRIPTION:
 *	Set rdma component to read parking guideline image
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
/* coverity[HIS_metric_violation : FALSE] */
static int tccvin_set_pgl(const struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	void __iomem				*rdma		= NULL;
	const struct v4l2_pix_format_mplane	*pix_mp		= NULL;
	unsigned int				width		= 0;
	unsigned int				height		= 0;
	unsigned int				format		= 0;
	unsigned int				buf_addr	= 0;

	dev		= stream_to_device(vstream);
	rdma		= VIOC_RDMA_GetAddress(vstream->cif.vin_path[VIN_COMP_PGL].index);

	pix_mp		= &vstream->format.fmt.pix_mp;

	width		= pix_mp->width;
	height		= pix_mp->height;
	format		= PGL_FORMAT;

#if defined(CONFIG_ARM64)
	buf_addr	= u64_to_u32(vstream->cif.rsvd_mem[RESERVED_MEM_PGL]->base);
#else
	buf_addr	= vstream->cif.rsvd_mem[RESERVED_MEM_PGL]->base;
#endif//defined(CONFIG_ARM64)

	logd(dev, "RDMA: 0x%08x, size[%d x %d], format[%d]\n",
		ptr_to_u32(rdma), width, height, format);

	VIOC_RDMA_SetImageFormat(rdma, format);
	VIOC_RDMA_SetImageSize(rdma, width, height);
	VIOC_RDMA_SetImageOffset(rdma, format, width);
	VIOC_RDMA_SetImageBase(rdma, buf_addr, 0, 0);
	if (vstream->cif.use_pgl == 1U) {
		VIOC_RDMA_SetImageEnable(rdma);
		VIOC_RDMA_SetImageUpdate(rdma);
	} else {
		VIOC_RDMA_SetImageDisable(rdma);
	}

	return 0;
}
#endif/* defined(CONFIG_OVERLAY_PGL) */

/*
 * tccvin_set_vin
 *
 * - DESCRIPTION:
 *	Set vin component to receive video data via cif port
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
/* coverity[HIS_metric_violation : FALSE] */
static int tccvin_set_vin(const struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	void __iomem				*vin		= NULL;
	const struct tccvin_vs_info		*vs_info	= NULL;
	const struct v4l2_rect			*crop_rect	= NULL;
	unsigned int				data_order	= 0;
	unsigned int				data_format	= 0;
	unsigned int				gen_field_en	= 0;
	unsigned int				de_low		= 0;
	unsigned int				field_low	= 0;
	unsigned int				vs_low		= 0;
	unsigned int				hs_low		= 0;
	unsigned int				pxclk_pol	= 0;
	unsigned int				vs_mask		= 0;
	unsigned int				hsde_connect_en	= 0;
	unsigned int				intpl_en	= 0;
	unsigned int				conv_en		= 0;
	unsigned int				interlaced	= 0;
	unsigned int				width		= 0;
	unsigned int				height		= 0;
	unsigned int				offset_x	= 0;
	unsigned int				offset_y	= 0;
	unsigned int				offset_y_intl	= 0;
	unsigned int				crop_width	= 0;
	unsigned int				crop_height	= 0;
	unsigned int				crop_offset_x	= 0;
	unsigned int				crop_offset_y	= 0;
#if	defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	unsigned int				stream_enable	= 0;
	unsigned int				flush_vsync	= 0;
#endif	/* defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	 * defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	 */

	dev		= stream_to_device(vstream);
	vin		= VIOC_VIN_GetAddress(vstream->cif.vin_path[VIN_COMP_VIN].index);

	vs_info		= &vstream->vs_info;
	crop_rect	= &vstream->rect_crop;

	data_order	= vs_info->data_order;
	data_format	= vs_info->data_format;
	gen_field_en	= vs_info->gen_field_en;
	de_low		= vs_info->de_low;
	field_low	= vs_info->field_low;
	vs_low		= vs_info->vs_low;
	hs_low		= vs_info->hs_low;
	pxclk_pol	= vs_info->pclk_polarity;
	vs_mask		= vs_info->vs_mask;
	hsde_connect_en	= vs_info->hsde_connect_en;
	intpl_en	= vs_info->intpl_en;
	interlaced	= vs_info->interlaced;
	conv_en		= vs_info->conv_en;

	width		= vs_info->width;
	height		= rshift(vs_info->height, interlaced);

	if ((crop_rect->width  == 0U) &&
	    (crop_rect->height == 0U)) {
		crop_width	= width;
		crop_height	= height;
		crop_offset_x	= 0;
		crop_offset_y	= 0;
	} else {
		crop_width	= crop_rect->width;
		crop_height	= rshift(crop_rect->height, interlaced);
		crop_offset_x	= s32_to_u32(crop_rect->left);
		crop_offset_y	= rshift(s32_to_u32(crop_rect->top), interlaced);
	}

#if	defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	stream_enable	= vs_info->stream_enable;
	flush_vsync	= vs_info->flush_vsync;
#endif	/* defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	 * defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	 */

	logd(dev, "VIN: 0x%08x, Size - (%u, %u) %u * %u -> (%u, %u) %u * %u\n",
		ptr_to_u32(vin),
		offset_x, offset_y, width, height,
		crop_offset_x, crop_offset_y, crop_width, crop_height);

	logd(dev, "%20s: %u\n", "data_order",		data_order);
	logd(dev, "%20s: %u\n", "data_format",		data_format);
	logd(dev, "%20s: %u\n", "gen_field_en",		gen_field_en);
	logd(dev, "%20s: %u\n", "de_low",		de_low);
	logd(dev, "%20s: %u\n", "vs_mask",		vs_mask);
	logd(dev, "%20s: %u\n", "hsde_connect_en",	hsde_connect_en);
	logd(dev, "%20s: %u\n", "intpl_en",		intpl_en);
	logd(dev, "%20s: %u\n", "interlaced",		interlaced);
	logd(dev, "%20s: %u\n", "conv_en",		conv_en);
#if	defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	logd(dev, "%20s: %u\n", "stream_enable",	stream_enable);
	logd(dev, "%20s: %u\n", "flush_vsync",		flush_vsync);
#endif	/* defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	 * defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	 */

	VIOC_VIN_SetSyncPolarity(vin, hs_low, vs_low, field_low, de_low, gen_field_en, pxclk_pol);
	VIOC_VIN_SetCtrl(vin, conv_en, hsde_connect_en, vs_mask, data_format, data_order);
	VIOC_VIN_SetInterlaceMode(vin, interlaced, intpl_en);
	VIOC_VIN_SetImageSize(vin, width, height);
	VIOC_VIN_SetImageOffset(vin, offset_x, offset_y, offset_y_intl);
	VIOC_VIN_SetImageCropSize(vin, crop_width, crop_height);
	VIOC_VIN_SetImageCropOffset(vin, crop_offset_x, crop_offset_y);
	VIOC_VIN_SetY2RMode(vin, 2);

#if	defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	VIOC_VIN_SetSEEnable(vin, stream_enable);
	VIOC_VIN_SetFlushBufferEnable(vin, flush_vsync);
#endif	/* defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	 * defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	 */

	logd(dev, "v4l2 preview format(pixelformat): 0x%08x\n", vstream->format.fmt.pix_mp.pixelformat);
	if (((data_format == (unsigned int)FMT_YUV422_16BIT)	||
	    (data_format == (unsigned int)FMT_YUV422_8BIT)	||
	    (data_format == (unsigned int)FMT_YUVK4444_16BIT)	||
	    (data_format == (unsigned int)FMT_YUVK4224_24BIT)) &&
		((vstream->format.fmt.pix_mp.pixelformat == (unsigned int)V4L2_PIX_FMT_RGB24) ||
		 (vstream->format.fmt.pix_mp.pixelformat == (unsigned int)V4L2_PIX_FMT_RGB32))) {
		if (!((interlaced == 1U) &&
		      !IS_ERR_OR_NULL(vstream->cif.vin_path[VIN_COMP_VIQE].np))) {
			logd(dev, "y2r is ENABLED\n");
			VIOC_VIN_SetY2REnable(vin, ON);
		}
	} else {
		logd(dev, "y2r is DISABLED\n");
		VIOC_VIN_SetY2REnable(vin, OFF);
	}

	/* set vin lut */
	if ((vstream->cif.vin_internal_lut.lut0_en == 1U)	||
	    (vstream->cif.vin_internal_lut.lut1_en == 1U)	||
	    (vstream->cif.vin_internal_lut.lut2_en == 1U)) {
		VIOC_VIN_SetLUT(vin, vstream->cif.vin_internal_lut.lut);
		VIOC_VIN_SetLUTEnable(vin,
			vstream->cif.vin_internal_lut.lut0_en,
			vstream->cif.vin_internal_lut.lut1_en,
			vstream->cif.vin_internal_lut.lut2_en);
	}

	VIOC_VIN_SetEnable(vin, ON);

	return 0;
}

/*
 * tccvin_set_deinterlacer
 *
 * - DESCRIPTION:
 *	Set viqe component to deinterlace interlaced video data
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
/* coverity[HIS_metric_violation : FALSE] */
static int tccvin_set_deinterlacer(const struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	const struct vioc_comp			*vin_path	= NULL;
	void __iomem				*viqe		= NULL;
	const struct tccvin_vs_info		*vs_info	= NULL;
	unsigned int				interlaced	= 0;
	unsigned int				width		= 0;
	unsigned int				height		= 0;
	unsigned int				viqe_width	= 0;
	unsigned int				viqe_height	= 0;
	unsigned int				format		= 0;
	VIOC_VIQE_DEINTL_MODE			bypass_deintl	= 0;
	unsigned int				offset		= 0;
	unsigned int				deintl_base0	= 0;
	unsigned int				deintl_base1	= 0;
	unsigned int				deintl_base2	= 0;
	unsigned int				deintl_base3	= 0;
	unsigned int				cdf_lut_en	= 0;
	unsigned int				his_en		= 0;
	unsigned int				gamut_en	= 0;
	unsigned int				d3d_en		= 0;
	unsigned int				deintl_en	= 0;
	unsigned int				vioc_format	= 0;
	unsigned int				pixelformat	= 0;
	int					ret		= 0;

	dev		= stream_to_device(vstream);
	vin_path	= vstream->cif.vin_path;

	if (!IS_ERR_OR_NULL(vin_path[VIN_COMP_VIQE].np)) {
		viqe		= VIOC_VIQE_GetAddress(vin_path[VIN_COMP_VIQE].index);
		vs_info		= &vstream->vs_info;

		interlaced	= vs_info->interlaced;
		width		= (vstream->rect_crop.width != 0U) ? vstream->rect_crop.width  : vs_info->width;
		height		= rshift(((vstream->rect_crop.height != 0U) ? vstream->rect_crop.height : vs_info->height), interlaced);

		viqe_width	= 0;
		viqe_height	= 0;
		format		= (unsigned int)VIOC_VIQE_FMT_YUV422;
		bypass_deintl	= VIOC_VIQE_DEINTL_MODE_3D;
		offset		= div_u32(mul_u32(mul_u32(mul_u32(width, height), 4U), 3U), 2U);
		deintl_base0	= u64_to_u32(vstream->cif.rsvd_mem[RESERVED_MEM_VIQE]->base);
		deintl_base1	= add_u32(deintl_base0, offset);
		deintl_base2	= add_u32(deintl_base1, offset);
		deintl_base3	= add_u32(deintl_base2, offset);

		cdf_lut_en	= OFF;
		his_en		= OFF;
		gamut_en	= OFF;
		d3d_en		= OFF;
		deintl_en	= ON;

		vioc_format	= vstream->vs_info.data_format;
		pixelformat	= vstream->format.fmt.pix_mp.pixelformat;

		logd(dev, "VIQE: 0x%08x, Source Size - width: %d, height: %d\n",
			ptr_to_u32(viqe), width, height);

		ret = VIOC_CONFIG_PlugIn(vin_path[VIN_COMP_VIQE].index, vin_path[VIN_COMP_VIN].index);

		if (((vioc_format == (unsigned int)FMT_YUV422_16BIT) ||
		     (vioc_format == (unsigned int)FMT_YUV422_8BIT) ||
		     (vioc_format == (unsigned int)FMT_YUVK4444_16BIT) ||
		     (vioc_format == (unsigned int)FMT_YUVK4224_24BIT)) &&
			((pixelformat == V4L2_PIX_FMT_RGB24) ||
			 (pixelformat == V4L2_PIX_FMT_RGB32))) {
			VIOC_VIQE_SetImageY2RMode(viqe, 2);
			VIOC_VIQE_SetImageY2REnable(viqe, ON);
		}
		VIOC_VIQE_SetControlRegister(viqe, viqe_width, viqe_height, format);
		VIOC_VIQE_SetDeintlRegister(viqe, format, OFF,
			viqe_width, viqe_height, bypass_deintl,
			deintl_base0, deintl_base1, deintl_base2, deintl_base3);
		VIOC_VIQE_SetControlEnable(viqe, cdf_lut_en, his_en, gamut_en, d3d_en, deintl_en);
		VIOC_VIQE_SetDeintlModeWeave(viqe);
		VIOC_VIQE_IgnoreDecError(viqe, ON, ON, ON);
	} else if (!IS_ERR_OR_NULL(vin_path[VIN_COMP_SDEINTL].np)) {
		/* can be pluged in */
		ret = VIOC_CONFIG_PlugIn(vin_path[VIN_COMP_SDEINTL].index, vin_path[VIN_COMP_VIN].index);
	} else {
		logi(dev, "There is no available deinterlacer\n");
		ret = -1;
	}

	return ret;
}

/*
 * tccvin_set_scaler
 *
 * - DESCRIPTION:
 *	Set sclaer component to scale up or down
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
/* coverity[HIS_metric_violation : FALSE] */
static int tccvin_set_scaler(const struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	const struct tccvin_cif			*cif		= NULL;
	void __iomem				*sc		= NULL;
	const struct v4l2_pix_format_mplane	*pix_mp		= NULL;
	unsigned int				width		= 0;
	unsigned int				height		= 0;
	int					ret		= 0;

	dev		= stream_to_device(vstream);
	cif		= &vstream->cif;
	sc		= VIOC_SC_GetAddress(cif->vin_path[VIN_COMP_SCALER].index);

	pix_mp		= &vstream->format.fmt.pix_mp;

	if ((vstream->rect_compose.width  == 0U) &&
	    (vstream->rect_compose.height == 0U)) {
		width  = pix_mp->width;
		height = pix_mp->height;
	} else {
		width  = vstream->rect_compose.width;
		height = vstream->rect_compose.height;
	}

	logd(dev, "SCaler: 0x%08x, Size - width: %d, height: %d\n",
		ptr_to_u32(sc), width, height);

	/* Plug the scaler in */
	ret = VIOC_CONFIG_PlugIn(cif->vin_path[VIN_COMP_SCALER].index, cif->vin_path[VIN_COMP_VIN].index);

	/* Configure the scaler */
	VIOC_SC_SetBypass(sc, OFF);
	VIOC_SC_SetDstSize(sc, width, height);
	VIOC_SC_SetOutPosition(sc, 0U, 0U);
	/* workaround: scaler margin */
	VIOC_SC_SetOutSize(sc, width, add_u32(height, 1U));
	VIOC_SC_SetUpdate(sc);

	return 0;
}

/*
 * tccvin_set_wmixer
 *
 * - DESCRIPTION:
 *	Set video-input wmixer component to mix two or more video data
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
/* coverity[HIS_metric_violation : FALSE] */
static int tccvin_set_wmixer(const struct tccvin_stream *vstream)
{
	void __iomem				*wmixer		= NULL;
	const struct v4l2_pix_format_mplane	*pix_mp		= NULL;
	unsigned int				width		= 0;
	unsigned int				height		= 0;
	unsigned int				out_posx	= 0;
	unsigned int				out_posy	= 0;
	unsigned int				ovp		= 5;
	unsigned int				vs_ch		= 0;
	const struct device			*dev		= NULL;
#if defined(CONFIG_OVERLAY_PGL)
	unsigned int				pgl_ch		= 1;
	unsigned int				chrom_layer	= 0;
	unsigned int				chroma_r	= PGL_BG_R;
	unsigned int				chroma_g	= PGL_BG_G;
	unsigned int				chroma_b	= PGL_BG_B;
	unsigned int				masked_chroma_r	= PGL_BGM_R;
	unsigned int				masked_chroma_g	= PGL_BGM_G;
	unsigned int				masked_chroma_b	= PGL_BGM_B;
#endif/* defined(CONFIG_OVERLAY_PGL) */
	int					ret		= 0;

	dev		= stream_to_device(vstream);
	wmixer		= VIOC_WMIX_GetAddress(vstream->cif.vin_path[VIN_COMP_WMIX].index);

	pix_mp		= &vstream->format.fmt.pix_mp;

	width		= pix_mp->width;
	height		= pix_mp->height;

	if ((vstream->rect_compose.left != 0) &&
	    (vstream->rect_compose.top  != 0)) {
		out_posx	= (unsigned int)vstream->rect_compose.left;
		out_posy	= s32_to_u32(vstream->rect_compose.top);
	}

	logd(dev, "WMIXer: 0x%08x, Size - width: %d, height: %d\n",
		ptr_to_u32(wmixer), width, height);
	logd(dev, " . CH0: (%d, %d)\n", out_posx, out_posy);

	/* Configure the wmixer */
	VIOC_WMIX_SetSize(wmixer, width, height);
	VIOC_WMIX_SetOverlayPriority(wmixer, ovp);
	VIOC_WMIX_SetPosition(wmixer, vs_ch, out_posx, out_posy);
#if defined(CONFIG_OVERLAY_PGL)
	VIOC_WMIX_SetPosition(wmixer, pgl_ch, 0, 0);
	VIOC_WMIX_SetChromaKey(wmixer, chrom_layer, ON,
		chroma_r, chroma_g, chroma_b,
		masked_chroma_r, masked_chroma_g, masked_chroma_b);
#endif/*defined(CONFIG_OVERLAY_PGL)*/
	VIOC_WMIX_SetUpdate(wmixer);
	ret = VIOC_CONFIG_WMIXPath(vstream->cif.vin_path[VIN_COMP_VIN].index, ON);

	return ret;
}

/*
 * tccvin_set_wdma
 *
 * - DESCRIPTION:
 *	Set wdma component to write video data
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
/* coverity[HIS_metric_violation : FALSE] */
static int tccvin_set_wdma(struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	struct tccvin_queue			*queue		= NULL;
	const struct tccvin_cif			*cif		= NULL;
	struct tccvin_buffer			*buf		= NULL;
	unsigned long				flags		= 0;
	void __iomem				*wdma		= NULL;
	const struct v4l2_pix_format_mplane	*pix_mp		= NULL;
	unsigned int				width		= 0;
	unsigned int				height		= 0;
	const struct tccvin_format		*pformat	= NULL;
	unsigned int				format		= 0;
	struct vb2_buffer			*vb		= NULL;
	unsigned int				dma_addrs[MAX_PLANES];

	dev		= stream_to_device(vstream);
	queue		= &vstream->queue;
	cif		= &vstream->cif;

	wdma		= VIOC_WDMA_GetAddress(vstream->cif.vin_path[VIN_COMP_WDMA].index);

	pix_mp		= &vstream->format.fmt.pix_mp;

	width		= pix_mp->width;
	height		= pix_mp->height;
	pformat		= tccvin_get_tccvin_format_by_pixelformat(pix_mp->pixelformat);
	format		= pformat->guid;

	logd(dev, "WDMA: 0x%08x, size[%d x %d], format[%d]\n",
		ptr_to_u32(wdma), width, height, format);

	VIOC_WDMA_SetImageFormat(wdma, format);
	VIOC_WDMA_SetImageSize(wdma, width, height);
	VIOC_WDMA_SetImageOffset(wdma, format, width);

	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	spin_lock_irqsave(&queue->slock, flags);
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	if (!list_empty(&queue->buf_list)) {
		/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
		/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
		/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
		/* coverity[cert_arr39_c_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		buf = list_first_entry(&queue->buf_list,
			struct tccvin_buffer, entry);
	}
	spin_unlock_irqrestore(&queue->slock, flags);

	if (buf != NULL) {
		vb = &buf->buf.vb2_buf;
		(void)memset(dma_addrs, 0, sizeof(dma_addrs));
		tccvin_get_dma_addrs(vstream, vb, dma_addrs);
		tccvin_print_dma_addrs(vstream, vb, dma_addrs);

		VIOC_WDMA_SetImageBase(wdma, dma_addrs[0], dma_addrs[1], dma_addrs[2]);
		VIOC_WDMA_SetImageEnable(wdma, OFF);
	} else {
		loge(dev, "Buffer is NOT initialized\n");
	}

	return 0;
}

static void tccvin_get_and_update_time(const struct tccvin_stream *vstream,
	struct timespec *ts_prev, struct timespec *ts_next, unsigned int debug)
{
	const struct device			*dev		= NULL;
	struct timespec				ts_diff		= { 0, };

	dev		= stream_to_device(vstream);

	getrawmonotonic(ts_next);

	if (debug == 1U) {
		ts_diff.tv_sec  = ts_next->tv_sec  - ts_prev->tv_sec;
		ts_diff.tv_nsec = ts_next->tv_nsec - ts_prev->tv_nsec;
		if (ts_diff.tv_nsec < 0) {
			ts_diff.tv_sec  -= 1;
			ts_diff.tv_nsec += 1000000000;
		}

		logi(dev, "timestamp curr: %9ld.%09ld, diff: %9ld.%09ld\n",
			ts_next->tv_sec, ts_next->tv_nsec,
			ts_diff.tv_sec, ts_diff.tv_nsec);
	}

	*ts_prev = *ts_next;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[HIS_metric_violation : FALSE] */
static irqreturn_t tccvin_vin_isr(int irq, void *data)
{
	struct tccvin_stream			*vstream	= NULL;
	const struct device			*dev		= NULL;
	struct tccvin_queue			*queue		= NULL;
	const struct vioc_comp			*pvioc		= NULL;
	const void __iomem			*vin		= NULL;
	void __iomem				*wdma		= NULL;
	const struct vioc_intr			*intr		= NULL;
	const struct vioc_intr_type		*intr_src	= NULL;
	unsigned long				flags		= 0;
	unsigned int				status		= 0;
	struct vb2_buffer			*vb		= NULL;
	unsigned int				dma_addrs[MAX_PLANES];
	bool					ret_bool	= FALSE;
	int					ret_call	= 0;
	irqreturn_t				ret		= IRQ_NONE;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	vstream		= (struct tccvin_stream *)data;
	dev		= stream_to_device(vstream);
	queue		= &vstream->queue;

	pvioc		= &vstream->cif.vin_path[VIN_COMP_WDMA];
	wdma		= VIOC_WDMA_GetAddress(pvioc->index);

	pvioc		= &vstream->cif.vin_path[VIN_COMP_VIN];
	vin		= VIOC_VIN_GetAddress(pvioc->index);

	intr		= &pvioc->intr;
	intr_src	= &intr->source;

	/* coverity[cert_int31_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	ret_bool = is_vioc_intr_activatied(intr_src->id, intr_src->bits);
	if (ret_bool) {
		VIOC_VIN_GetStatus(vin, &status);
		if ((status & VIN_INT_VS_MASK) != 0U) {
			/* coverity[cert_int31_c_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			ret_call = vioc_intr_clear(intr_src->id, VIN_INT_VS_MASK);
			if (ret_call != 0) {
				/* failure of vioc_intr_clear */
				loge(dev, "interrupt is not cleared\n");
			}

			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			spin_lock_irqsave(&queue->slock, flags);

			vstream->prev_buf = NULL;
			vstream->next_buf = NULL;

			/* check if frameskip is needed */
			if (vstream->skip_frame_cnt > 0) {
				logd(dev, "skip frame count: 0x%08x\n",
					vstream->skip_frame_cnt);
				vstream->skip_frame_cnt--;
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto wdma_update;
			}

			/* check if the incoming buffer list is empty */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			if (list_empty(&vstream->queue.buf_list)) {
				loge(dev, "The incoming buffer list is empty\n");
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto wdma_update;
			}

			/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
			/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
			/* coverity[cert_arr39_c_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			vstream->prev_buf = list_first_entry(&queue->buf_list,
				struct tccvin_buffer, entry);

			ret_call = ((queue->flags & (unsigned int)TCCVIN_QUEUE_DROP_CORRUPTED) != 0U) ? 1 : 0;
			if (ret_call != 0) {
				loge(dev, "The buffer is corrupted\n");
				vstream->prev_buf = NULL;
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto wdma_update;
			}

			/* check if the incoming buffer list has only one entry */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			if (list_is_last(&vstream->prev_buf->entry, &queue->buf_list)) {
				logd(dev, "driver has only one buffer\n");
				vstream->prev_buf = NULL;
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto wdma_update;
			}

			/* The incoming buffer list has two or more entries. */
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
			/* coverity[cert_arr39_c_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			vstream->next_buf = list_next_entry(vstream->prev_buf, entry);

			if (IS_ERR_OR_NULL(vstream->prev_buf) ||
			    IS_ERR_OR_NULL(vstream->next_buf)) {
				loge(dev, "prev(0x%p) or next(0x%p) is wrong\n",
					vstream->prev_buf, vstream->next_buf);
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto wdma_update;
			}

			logd(dev, "bufidx: %d, type: 0x%08x, memory: 0x%08x\n",
				vstream->next_buf->buf.vb2_buf.index,
				vstream->next_buf->buf.vb2_buf.type,
				vstream->next_buf->buf.vb2_buf.memory);

			vb = &vstream->next_buf->buf.vb2_buf;
			(void)memset(dma_addrs, 0, sizeof(dma_addrs));
			tccvin_get_dma_addrs(vstream, vb, dma_addrs);
			tccvin_print_dma_addrs(vstream, vb, dma_addrs);

			VIOC_WDMA_SetImageBase(wdma, dma_addrs[0], dma_addrs[1], dma_addrs[2]);
wdma_update:
			spin_unlock_irqrestore(&queue->slock, flags);

			VIOC_WDMA_SetImageEnable(wdma, OFF);
		}

		ret = IRQ_HANDLED;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[HIS_metric_violation : FALSE] */
static irqreturn_t tccvin_wdma_isr(int irq, void *data)
{
	struct tccvin_stream			*vstream	= NULL;
	const struct device			*dev		= NULL;
	struct tccvin_queue			*queue		= NULL;
	const struct vioc_comp			*pvioc		= NULL;
	const void __iomem			*wdma		= NULL;
	const struct vioc_intr			*intr		= NULL;
	const struct vioc_intr_type		*intr_src	= NULL;
	unsigned long				flags;
	unsigned int				status		= 0;
	bool					ret_bool	= FALSE;
	int					ret_call	= 0;
	irqreturn_t				ret		= IRQ_NONE;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	vstream		= (struct tccvin_stream *)data;
	dev		= stream_to_device(vstream);
	queue		= &vstream->queue;

	pvioc		= &vstream->cif.vin_path[VIN_COMP_WDMA];
	wdma		= VIOC_WDMA_GetAddress(pvioc->index);

	intr		= &pvioc->intr;
	intr_src	= &intr->source;

	/* coverity[cert_int31_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	ret_bool = is_vioc_intr_activatied(intr_src->id, intr_src->bits);
	if (ret_bool) {
		VIOC_WDMA_GetStatus(wdma, &status);
		if ((status & VIOC_WDMA_IREQ_EOFR_MASK) != 0U) {
			/* coverity[cert_int31_c_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			ret_call = vioc_intr_clear(intr_src->id, VIOC_WDMA_IREQ_EOFR_MASK);
			if (ret_call != 0) {
				/* failure of vioc_intr_clear */
				loge(dev, "interrupt is not cleared\n");
			}

			tccvin_get_and_update_time(vstream, &vstream->ts_prev, &vstream->ts_next,
				vstream->timestamp);

			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			spin_lock_irqsave(&queue->slock, flags);

			if (!IS_ERR_OR_NULL(vstream->prev_buf) &&
			    !IS_ERR_OR_NULL(vstream->next_buf)) {

				/* fill the buffer info */
				vstream->prev_buf->buf.vb2_buf.timestamp =
					s64_to_u64(timespec_to_ns(&vstream->ts_next));
				vstream->prev_buf->buf.field = (unsigned int)V4L2_FIELD_NONE;
				vstream->sequence = add_u32(vstream->sequence, 1U);
				vstream->prev_buf->buf.sequence = vstream->sequence;

				/* dequeue prev_buf from the incoming buffer list */
				list_del(&vstream->prev_buf->entry);

				/* queue the prev_buf to the outgoing buffer list */
				vb2_buffer_done(&vstream->prev_buf->buf.vb2_buf,
					VB2_BUF_STATE_DONE);

				vstream->prev_buf = NULL;
				vstream->next_buf = NULL;
			}

			spin_unlock_irqrestore(&queue->slock, flags);
		}

		ret = IRQ_HANDLED;
	}

	return ret;
}

/* coverity[HIS_metric_violation : FALSE] */
static int tccvin_start_stream(struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	const struct tccvin_cif			*cif		= NULL;
	const struct vioc_comp			*vin_path	= NULL;
	const struct tccvin_vs_info		*vs_info	= NULL;
	const struct v4l2_pix_format_mplane	*pix_mp		= NULL;
	int					ret_call	= 0;
	int					ret		= 0;

	dev		= stream_to_device(vstream);
	cif		= &vstream->cif;
	vin_path	= cif->vin_path;
	vs_info		= &vstream->vs_info;

	pix_mp		= &vstream->format.fmt.pix_mp;

	/* size info */
	logd(dev, "preview size: %d * %d\n", pix_mp->width, pix_mp->height);

	/* map cif-port */
	ret_call = tccvin_map_cif_port(vstream);
	if (ret_call < 0) {
		loge(dev, "tccvin_map_cif_port, ret: %d\n", ret_call);
		ret = -1;
	}

#if defined(CONFIG_VIDEO_TCCVIN2_DIAG)
	/* check cif-port mapping */
	ret_call = tccvin_check_cif_port_mapping(vstream);
	if (ret_call < 0) {
		loge(dev, "tccvin_check_cif_port_mapping, ret: %d\n", ret_call);
		ret = -1;
	}
#endif//defined(CONFIG_VIDEO_TCCVIN2_DIAG)

#if defined(CONFIG_OVERLAY_PGL)
	/* set rdma for Parking Guide Line */
	if (vstream->cif.use_pgl == 1U) {
		/* enable rdma */
		ret_call = tccvin_set_pgl(vstream);
		if (ret_call < 0) {
			loge(dev, "tccvin_set_pgl, ret: %d\n", ret_call);
			ret = -1;
		}
	}
#endif/* defined(CONFIG_OVERLAY_PGL) */

	/* set vin */
	ret_call = tccvin_set_vin(vstream);
	if (ret_call < 0) {
		loge(dev, "tccvin_set_vin, ret: %d\n", ret_call);
		ret = -1;
	}

	/* set deinterlacer */
	if (vs_info->interlaced == (unsigned int)V4L2_DV_INTERLACED) {
		/* set Deinterlacer */
		ret_call = tccvin_set_deinterlacer(vstream);
		if (ret_call < 0) {
			loge(dev, "tccvin_set_deinterlacer, ret: %d\n", ret_call);
			ret = -1;
		}

		if (!IS_ERR_OR_NULL(vin_path[VIN_COMP_VIQE].np)) {
			/*
			 * set skip 1 frame when use wdma frame by frame mode
			 * set skip 3 frames in case of viqe 3d mode
			 */
			vstream->skip_frame_cnt = 4;
		}
	} else {
		/* set skip 1 frame when use wdma frame by frame mode */
		vstream->skip_frame_cnt = 1;
	}

	/* set scaler */
	if (!IS_ERR_OR_NULL(vin_path[VIN_COMP_SCALER].np)) {
		/* scaler exists */
		ret_call = tccvin_set_scaler(vstream);
		if (ret_call < 0) {
			loge(dev, "tccvin_set_scaler, ret: %d\n", ret_call);
			ret = -1;
		}
	}

	/* set wmixer */
	if (!IS_ERR_OR_NULL(vin_path[VIN_COMP_WMIX].np)) {
		/* wmix exists */
		ret_call = tccvin_set_wmixer(vstream);
		if (ret_call < 0) {
			loge(dev, "tccvin_set_wmixer, ret: %d\n", ret_call);
			ret = -1;
		}
	}

	/* set wdma */
	ret_call = tccvin_set_wdma(vstream);
	if (ret_call < 0) {
		loge(dev, "tccvin_set_wdma, ret: %d\n", ret_call);
		ret = -1;
	}

	return ret;
}

/* coverity[HIS_metric_violation : FALSE] */
static int tccvin_stop_stream(const struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	const struct vioc_comp			*vin_path	= NULL;
	const struct vioc_comp			*vioc		= NULL;
	const struct tccvin_vs_info		*vs_info	= NULL;

#if defined(CONFIG_OVERLAY_PGL)
	void __iomem				*pgl		= NULL;
#endif/* defined(CONFIG_OVERLAY_PGL) */
	void __iomem				*wdma		= NULL;
	VIOC_PlugInOutCheck			state		= { 0, };

	dev		= stream_to_device(vstream);
	vin_path	= vstream->cif.vin_path;
	vs_info		= &vstream->vs_info;

#if defined(CONFIG_OVERLAY_PGL)
	pgl		= VIOC_RDMA_GetAddress(vin_path[VIN_COMP_PGL].index);
#endif/* defined(CONFIG_OVERLAY_PGL) */
	wdma		= VIOC_WDMA_GetAddress(vin_path[VIN_COMP_WDMA].index);

	VIOC_WDMA_SetIreqMask(wdma, VIOC_WDMA_IREQ_ALL_MASK, 0x1);
	VIOC_WDMA_SetImageDisable(wdma);

	if (VIOC_WDMA_Get_CAddress(wdma) > 0UL) {
		int		idxLoop;
		unsigned int	status;

		/* We set a criteria for a worst-case within 10-fps and
		 * time-out to disable WDMA as 200ms. To be specific, in the
		 * 10-fps environment, the worst case we assume, it has to
		 * wait 200ms at least for 2 frame.
		 */
		for (idxLoop = 0; idxLoop < 10; idxLoop++) {
			VIOC_WDMA_GetStatus(wdma, &status);
			if (IS_SET(status, VIOC_WDMA_IREQ_EOFR_MASK)) {
				/* eof occurs */
				break;
			}
			/* 20msec is minimum in msleep() */
			msleep(20);
		}
	}

	vioc = &vin_path[VIN_COMP_SCALER];
	if (!IS_ERR_OR_NULL(vioc->np)) {
		(void)VIOC_CONFIG_Device_PlugState(vioc->index, &state);
		if ((state.enable == 1U) &&
		    (state.connect_statue == (unsigned int)VIOC_PATH_CONNECTED)) {
			(void)VIOC_CONFIG_PlugOut(vioc->index);
		}
	}

	if (vs_info->interlaced == (unsigned int)V4L2_DV_INTERLACED) {
		vioc = &vin_path[VIN_COMP_VIQE];
		if (!IS_ERR_OR_NULL(vioc->np)) {
			(void)VIOC_CONFIG_Device_PlugState(vioc->index, &state);
			if ((state.enable == 1U) &&
			    (state.connect_statue == VIOC_PATH_CONNECTED)) {
				(void)VIOC_CONFIG_PlugOut(vioc->index);
			}
		} else {
			vioc = &vin_path[VIN_COMP_SDEINTL];
			if (!IS_ERR_OR_NULL(vioc->np)) {
				(void)VIOC_CONFIG_Device_PlugState(vioc->index, &state);
				if ((state.enable == 1U) &&
				    (state.connect_statue == VIOC_PATH_CONNECTED)) {
					(void)VIOC_CONFIG_PlugOut(vioc->index);
				}
			} else {
				/* no available de-interlacer */
				loge(dev, "There is no available deinterlacer\n");
			}
		}
	}

	VIOC_VIN_SetEnable(VIOC_VIN_GetAddress(vin_path[VIN_COMP_VIN].index), OFF);

#if defined(CONFIG_OVERLAY_PGL)
	/* disable pgl */
	if (vstream->cif.use_pgl == 1U) {
		/* disable rdma */
		VIOC_RDMA_SetImageDisable(pgl);
	}
#endif/* defined(CONFIG_OVERLAY_PGL) */

	return 0;
}

/* coverity[HIS_metric_violation : FALSE] */
static int tccvin_request_irq(struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	struct vioc_comp			*vin_path	= NULL;
	struct vioc_intr			*intr		= NULL;
	struct vioc_intr_type			*intr_src	= NULL;
	unsigned int				intr_base_id	= 0;
	unsigned int				intr_src_id	= 0;
	unsigned int				vioc_base_id	= 0;
	unsigned int				vioc_vin_id	= 0;
	unsigned int				vioc_wdma_id	= 0;
	int					ret_call	= 0;
	int					ret		= 0;

	dev		= stream_to_device(vstream);

	/* vin interrupt */
	vin_path	= &vstream->cif.vin_path[VIN_COMP_VIN];
	intr		= &vin_path->intr;
	intr_src	= &intr->source;
	if (intr->reg == 0U) {
		intr_src->id   = 0;
		intr_src->bits = 0;

		vioc_base_id	= VIOC_VIN00;
		intr_base_id	= VIOC_INTR_VIN0;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)

		if (vin_path->index >= VIOC_VIN40) {
			vioc_base_id	= VIOC_VIN40;
			intr_base_id	= VIOC_INTR_VIN4;
		}
#endif

		/* convert to raw index */
		vioc_base_id	= get_vioc_index(vioc_base_id)    / 2U;
		vioc_vin_id	= get_vioc_index(vin_path->index) / 2U;
		/* coverity[cert_int02_c_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		intr_src_id	= add_u32(intr_base_id, sub_u32(vioc_vin_id, vioc_base_id));
		intr_src->id	= u32_to_s32(intr_src_id);
		intr_src->bits	= VIN_INT_VS_MASK;
		logd(dev, "vin - vioc_base_id: %u, vioc_vin_id: %u\n", vioc_base_id, vioc_vin_id);
		logd(dev, "vin irq num: %u, src.id: %u, src.bits: 0x%08x\n",
			intr->num, intr_src->id, intr_src->bits);

		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		(void)vioc_intr_disable(intr->num, intr_src->id, 0xFFFFFFFFU);
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		(void)vioc_intr_clear(intr_src->id, 0xFFFFFFFFU);
		ret_call = request_irq(s32_to_u32(intr->num),
			tccvin_vin_isr, IRQF_SHARED, vstream->tdev->vdev.name, vstream);
		if (ret_call < 0) {
			loge(dev, "vin - request_irq, ret: %d\n", ret_call);
			ret = -1;
		}
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		(void)vioc_intr_enable(intr->num, intr_src->id, intr_src->bits);
		intr->reg = 1;
	} else {
		loge(dev, "The irq(%d) is already registered.\n", intr->num);
		ret = -1;
	}

	/* wdma interrupt */
	vin_path	= &vstream->cif.vin_path[VIN_COMP_WDMA];
	intr		= &vin_path->intr;
	intr_src	= &intr->source;
	if (intr->reg == 0U) {
		intr_src->id   = 0;
		intr_src->bits = 0;

		vioc_base_id	= VIOC_WDMA00;
		intr_base_id	= VIOC_INTR_WD0;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		if (vin_path->index >= VIOC_WDMA09) {
			vioc_base_id	= VIOC_WDMA09;
			intr_base_id	= VIOC_INTR_WD9;
		}
#endif/* defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC805X) */

		/* convert to raw index */
		vioc_base_id	= get_vioc_index(vioc_base_id);
		vioc_wdma_id	= get_vioc_index(vin_path->index);
		/* coverity[cert_int31_c_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		intr_src_id	= add_u32(intr_base_id, sub_u32(vioc_wdma_id, vioc_base_id));
		intr_src->id	= u32_to_s32(intr_src_id);
		intr_src->bits	= VIOC_WDMA_IREQ_EOFR_MASK;
		logd(dev, "wdma - vioc_base_id: %u, vioc_wdma_id: %u\n", vioc_base_id, vioc_wdma_id);
		logd(dev, "wdma irq num: %u, src.id: %d, src.bits: 0x%08x\n",
			intr->num, intr_src->id, intr_src->bits);

		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		(void)vioc_intr_disable(intr->num, intr_src->id, VIOC_WDMA_IREQ_ALL_MASK);
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		(void)vioc_intr_clear(intr_src->id, VIOC_WDMA_IREQ_ALL_MASK);
		ret_call = request_irq(s32_to_u32(intr->num),
			tccvin_wdma_isr, IRQF_SHARED, vstream->tdev->vdev.name, vstream);
		if (ret_call < 0) {
			loge(dev, "wdma - request_irq, ret: %d\n", ret_call);
			ret = -1;
		}
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		(void)vioc_intr_enable(intr->num, intr_src->id, intr_src->bits);
		intr->reg = 1;
	} else {
		loge(dev, "The irq(%d) is already registered.\n", intr->num);
		ret = -1;
	}

	return ret;
}

static int tccvin_free_irq(struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	struct vioc_comp			*vin_path	= NULL;
	struct vioc_intr			*intr		= NULL;
	const struct vioc_intr_type		*intr_src	= NULL;
	int					idxVioc		= 0;
	int					ret		= 0;

	dev		= stream_to_device(vstream);

	for (idxVioc = (int)VIN_COMP_MAX - 1; idxVioc >= 0; idxVioc--) {
		vin_path	= &vstream->cif.vin_path[idxVioc];
		intr		= &vin_path->intr;
		intr_src	= &intr->source;

		if (intr->reg == 1U) {
			/* coverity[cert_int02_c_violation : FALSE] */
			/* coverity[cert_int31_c_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			(void)vioc_intr_clear(intr_src->id, intr_src->bits);
			/* coverity[cert_int02_c_violation : FALSE] */
			/* coverity[cert_int31_c_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			(void)vioc_intr_disable(intr->num, intr_src->id, intr_src->bits);
			(void)free_irq(s32_to_u32(intr->num), vstream);
			intr->reg = 0;
		}
	}

	return ret;
}

static int tccvin_video_check_subdev_status(const struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	const struct tccvin_device		*tdev		= NULL;
	struct v4l2_subdev			*subdev		= NULL;
	unsigned int				delay		= 0;
	unsigned int				idxTry		= 0;
	unsigned int				nTry		= 3;
	unsigned int				status		= 0;
	int					ret		= 0;

	dev		= stream_to_device(vstream);
	tdev		= vstream->tdev;
	subdev		= tdev->tsubdev.sd;

	/* wait for additional stabilization time */
	delay = vstream->vs_stabilization;
	if (delay != 0U) {
		logi(dev, "additional stabization time is %d ms\n", delay);
		msleep(delay);
	}

	/* check v4l2 subdev's status */
	idxTry = 0;
	status = (unsigned int)V4L2_IN_ST_NO_SIGNAL;
	do {
		logd(dev, "g_input_status (%d / %d)\n", idxTry, nTry);

		ret = tccvin_subdev_video_g_input_status(subdev, &status);
		if (ret != 0) {
			logw(dev, "video.g_input_status is not supported\n");
			status = 0;
			ret = 0;
		} else {
			if (IS_SET(status, V4L2_IN_ST_NO_SIGNAL)) {
				logd(dev, "subdev is not stable\n");

				/* 20msec is minimum in msleep() */
				msleep(20);

				idxTry = idxTry + 1U;
			} else {
				logd(dev, "subdev is stable\n");
				status = 0;
			}
		}
	} while ((status != 0U) && (idxTry < nTry));

	return ret;
}

/* coverity[HIS_metric_violation : FALSE] */
static int tccvin_start_subdevs(struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	struct v4l2_subdev			*subdev		= NULL;
	struct v4l2_subdev_format		*fmt		= NULL;
	struct tccvin_vs_info			*vs_info	= NULL;
	struct v4l2_dv_timings			timings		= { 0, };
	unsigned int				mbus_flags	= 0;
	int					ret_call	= 0;
	struct v4l2_subdev_pad_config		*pad_cfg	= NULL;
	int					ret		= 0;

	dev		= stream_to_device(vstream);
	subdev		= vstream->tdev->tsubdev.sd;
	fmt		= &vstream->tdev->tsubdev.fmt;
	vs_info		= &vstream->vs_info;

	ret_call = tccvin_subdev_core_s_power(subdev, 1);
	if (ret_call != 0) {
		/* error: tccvin_subdev_core_s_power */
		logd(dev, "tccvin_subdev_core_s_power, ret: %d\n", ret_call);
	}

	ret_call = tccvin_subdev_core_init(subdev, 1);
	if (ret_call != 0) {
		/* error: tccvin_subdev_core_init */
		logd(dev, "tccvin_subdev_core_init, ret: %d\n", ret_call);
	}

	if (!IS_ERR_OR_NULL(subdev) && (strncmp(subdev->name, "tcc-isp", 7) == 0)) {
		ret_call = tccvin_subdev_core_load_fw(subdev);
		if (ret_call != 0) {
			/* error: tccvin_subdev_core_load_fw */
			logd(dev, "tccvin_subdev_core_load_fw, ret: %d\n", ret_call);
		}
	}

	ret_call = tccvin_subdev_video_s_stream(subdev, 1);
	if (ret_call != 0) {
		/* error: tccvin_subdev_video_s_stream */
		logd(dev, "tccvin_subdev_video_s_stream, ret: %d\n", ret_call);
	}

	ret_call = tccvin_video_check_subdev_status(vstream);
	if (ret_call != 0) {
		/* error: tccvin_video_check_subdev_status */
		logd(dev, "tccvin_video_check_subdev_status, ret: %d\n", ret_call);
	}

	ret_call = tccvin_subdev_video_g_dv_timings(subdev, &timings);
	if (ret_call != 0) {
		/* error: tccvin_subdev_video_g_dv_timings */
		logd(dev, "tccvin_subdev_video_g_dv_timings, ret: %d\n", ret_call);
	} else {
		/* size */
		vs_info->height		= timings.bt.height;
		vs_info->width		= timings.bt.width;
		logd(dev, "width: %d, height: %d\n", vs_info->height, vs_info->width);

		/* interlaced */
		vs_info->interlaced	= (timings.bt.interlaced == (unsigned int)V4L2_DV_INTERLACED) ? 1U : 0U;
		logd(dev, "interalced: %u\n", vs_info->interlaced);
	}

	fmt = &vstream->tdev->tsubdev.fmt;
	fmt->which = (unsigned int)V4L2_SUBDEV_FORMAT_ACTIVE;
	ret_call = tccvin_subdev_pad_get_fmt(subdev, pad_cfg, fmt);
	if (ret_call != 0) {
		/* error: tccvin_subdev_pad_get_fmt */
		logd(dev, "tccvin_subdev_pad_get_fmt, ret: %d\n", ret_call);
	} else {
		tccvin_get_vs_info_format_by_mbus_format(vstream, fmt->format.code, &vstream->vs_info);
		logd(dev, "data_format: %u, data_order: %u\n", vs_info->data_format, vs_info->data_order);
	}

	ret_call = tccvin_subdev_video_g_mbus_config(subdev, &vstream->mbus_config);
	if (ret_call != 0) {
		/* error: tccvin_subdev_video_g_mbus_config */
		logd(dev, "tccvin_subdev_video_g_mbus_config, ret: %d\n", ret_call);
	} else {
		mbus_flags = vstream->mbus_config.flags;
		vs_info->de_low		= tccvin_get_data_enable_pol_by_mbus_conf(mbus_flags);
		vs_info->vs_low		= tccvin_get_vsync_pol_by_mbus_conf(mbus_flags);
		vs_info->hs_low		= tccvin_get_hsync_pol_by_mbus_conf(mbus_flags);
		vs_info->pclk_polarity	= tccvin_get_pclk_pol_by_mbus_conf(mbus_flags);
		vs_info->conv_en	= tccvin_get_embedded_sync_by_mbus_conf(vstream->mbus_config.type);
		logd(dev, "de: %u, vs: %u, hs: %u, pclk: %u, conv: %u\n",
			vs_info->de_low, vs_info->vs_low, vs_info->hs_low, vs_info->pclk_polarity, vs_info->conv_en);
	}

	return ret;
}

static int tccvin_stop_subdevs(const struct tccvin_stream *vstream)
{
	struct v4l2_subdev			*subdev		= NULL;
	int					ret		= 0;

	subdev		= vstream->tdev->tsubdev.sd;

	(void)tccvin_subdev_video_s_stream(subdev, 0);
	(void)tccvin_subdev_core_init(subdev, 0);
	(void)tccvin_subdev_core_s_power(subdev, 0);

	return ret;
}

int tccvin_video_init(struct tccvin_stream *vstream,
	struct device_node *dev_node)
{
	const struct device			*dev		= NULL;
	int					ret_call	= 0;
	int					ret		= 0;

	dev		= stream_to_device(vstream);

	ret_call = tccvin_get_clock(vstream, dev_node);
	if (ret_call != 0) {
		loge(dev, "tccvin_get_clock, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	ret_call = tccvin_parse_ddibus(vstream, dev_node);
	if (ret_call != 0) {
		loge(dev, "tccvin_parse_ddibus, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	ret_call = tccvin_parse_reserved_memory(vstream, dev_node);
	if (ret_call != 0) {
		loge(dev, "tccvin_parse_reserved_memory, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	ret_call = tccvin_parse_fwnode(vstream, dev_node);
	if (ret_call != 0) {
		logd(dev, "tccvin_parse_fwnode, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	return ret;
}

int tccvin_video_deinit(const struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	int					ret		= 0;

	dev		= stream_to_device(vstream);

	tccvin_put_clock(vstream);

	return ret;
}

bool tccvin_video_is_pixelformat_supported(const struct tccvin_stream *vstream,
	unsigned int pixelformat)
{
	const struct device			*dev		= NULL;
	const struct tccvin_format		*tformat	= NULL;
	bool					ret		= TRUE;

	dev		= stream_to_device(vstream);

	tformat = tccvin_get_tccvin_format_by_pixelformat(pixelformat);
	if (tformat == NULL) {
		loge(dev, "pixelformat(0x%08x) is not supported\n", pixelformat);
		ret = FALSE;
	}

	return ret;
}

unsigned int tccvin_video_get_pixelformat_by_index(const struct tccvin_stream *vstream,
	unsigned int index)
{
	const struct device			*dev		= NULL;
	unsigned int				nList		= 0;
	unsigned int				pixelformat	= 0;

	dev		= stream_to_device(vstream);

	/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	nList = ARRAY_SIZE(tccvin_format_list);
	if (index < nList) {
		/* get pixelformat */
		pixelformat = tccvin_format_list[index].pixelformat;
	}

	return pixelformat;
}

bool tccvin_video_is_format_supported(const struct tccvin_stream *vstream,
	const struct v4l2_format *format)
{
	const struct device			*dev		= NULL;
	const struct v4l2_pix_format_mplane	*pix_mp		= NULL;
	bool					ret_bool	= TRUE;
	bool					ret		= TRUE;

	dev		= stream_to_device(vstream);
	pix_mp		= &format->fmt.pix_mp;

	if (format->type != (unsigned int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		/* the format is supported */
		ret = FALSE;
	}

	ret_bool = tccvin_video_is_framesize_supported(vstream, pix_mp->width, pix_mp->height);
	if (!ret_bool) {
		loge(dev, "tccvin_video_is_framesize_supported\n");
		ret = FALSE;
	}

	ret_bool = tccvin_video_is_pixelformat_supported(vstream, pix_mp->pixelformat);
	if (!ret_bool) {
		loge(dev, "tccvin_video_is_pixelformat_supported\n");
		ret = FALSE;
	}

	return ret;
}

int tccvin_video_declare_coherent_dma_memory(const struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	int					ret		= 0;

	dev		= stream_to_device(vstream);

	ret = dma_declare_coherent_memory(vstream->tdev->v4ldev.dev,
		vstream->cif.rsvd_mem[RESERVED_MEM_PREV]->base,
		vstream->cif.rsvd_mem[RESERVED_MEM_PREV]->base,
		vstream->cif.rsvd_mem[RESERVED_MEM_PREV]->size);
	logd(dev, "dma_declare_coherent_memory, ret: %d\n", ret);

	return ret;
}

/* coverity[HIS_metric_violation : FALSE] */
int tccvin_video_streamon(struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	unsigned int				flags		= 0;
	int					ret_call	= 0;
	int					ret		= 0;

	dev		= stream_to_device(vstream);
	flags		= vstream->handover_flags;

	/* print handover flags */
	tccvin_print_handover_flags(vstream, flags);

	/* skip to handle video source */
	if (!tccvin_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_SUBDEV)) {
		/* start v4l2-subdev */
		(void)tccvin_start_subdevs(vstream);
	}

	ret_call = tccvin_enable_clock(vstream);
	if (ret_call < 0) {
		loge(dev, "tccvin_enable_clock, ret: %d\n", ret_call);
		ret = -1;
	}

	/* skip to handle video-capture */
	if (!tccvin_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_DEV)) {
		/* reset vioc path */
		(void)tccvin_reset_vioc_path(vstream);
	}

	/* IMPORTANT: VIOC Interrupt MUST BE Requested after VIOC RESET Sequence */
	ret_call = tccvin_request_irq(vstream);
	if (ret_call < 0) {
		loge(dev, "tccvin_request_irq, ret: %d\n", ret_call);
		ret = -1;
	}

	/* skip to handle video-capture */
	if (!tccvin_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_DEV)) {
		ret_call = tccvin_start_stream(vstream);
		if (ret_call < 0) {
			loge(dev, "tccvin_start_stream, ret: %d\n", ret_call);
			ret = -1;
		}
	}

	return ret;
}

/* coverity[HIS_metric_violation : FALSE] */
int tccvin_video_streamoff(struct tccvin_stream *vstream)
{
	const struct device			*dev		= NULL;
	unsigned int				flags		= 0;
	int					ret_call	= 0;
	int					ret		= 0;

	dev		= stream_to_device(vstream);
	flags		= vstream->handover_flags;

	/* print handover flags */
	tccvin_print_handover_flags(vstream, flags);

	/* skip to handle video-capture */
	if (!tccvin_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_DEV)) {
		ret_call = tccvin_stop_stream(vstream);
		if (ret_call < 0) {
			loge(dev, "tccvin_stop_stream, ret: %d\n", ret_call);
			ret = -1;
		}
	}

	ret_call = tccvin_free_irq(vstream);
	if (ret_call < 0) {
		loge(dev, "tccvin_free_irq, ret: %d\n", ret_call);
		ret = -1;
	}

	/* skip to handle video-capture */
	if (!tccvin_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_DEV)) {
		/* reset vioc path */
		(void)tccvin_reset_vioc_path(vstream);
	}

	tccvin_disable_clock(vstream);

	/* skip to handle video source */
	if (!tccvin_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_SUBDEV)) {
		/* stop v4l2-subdev */
		(void)tccvin_stop_subdevs(vstream);
	}

	return ret;
}

void tccvin_video_check_path_status(const struct tccvin_stream *vstream,
	unsigned int *status)
{
	const struct device			*dev		= NULL;
	const struct vioc_comp			*vin_path	= NULL;
	const void __iomem			*pWDMA		= NULL;
	unsigned int				prev_addr	= 0;
	unsigned int				curr_addr	= 0;
	unsigned int				nCheck		= 0;
	unsigned int				idxCheck	= 0;
	unsigned int				delay		= 20;

	dev		= stream_to_device(vstream);
	vin_path	= vstream->cif.vin_path;
	pWDMA		= VIOC_WDMA_GetAddress(vin_path[VIN_COMP_WDMA].index);

	curr_addr	= VIOC_WDMA_Get_CAddress(pWDMA);
	msleep(delay);

	nCheck		= 4;
	for (idxCheck = 0; idxCheck < nCheck; idxCheck++) {
		prev_addr = curr_addr;
		msleep(delay);
		curr_addr = VIOC_WDMA_Get_CAddress(pWDMA);

		if (prev_addr != curr_addr) {
			/* path status is okay */
			*status = V4L2_CAP_PATH_WORKING;
		} else {
			*status = V4L2_CAP_PATH_NOT_WORKING;
			logd(dev, "[%d] prev_addr: 0x%08x, curr_addr: 0x%08x\n",
				idxCheck, prev_addr, curr_addr);
		}
	}
}

int tccvin_video_get_lastframe_addrs(const struct tccvin_stream *vstream,
	unsigned int *addrs)
{
	const struct device			*dev		= NULL;
	int					ret		= 0;

	dev		= stream_to_device(vstream);

#if defined(CONFIG_ARM64)
	*addrs = u64_to_u32(vstream->cif.rsvd_mem[RESERVED_MEM_LFRAME]->base);
#else
	*addrs = vstream->cif.rsvd_mem[RESERVED_MEM_LFRAME]->base;
#endif//defined(CONFIG_ARM64)
	logi(dev, "addrs of lastframe is 0x%08x\n", *addrs);

	return ret;
}

int tccvin_video_create_lastframe(const struct tccvin_stream *vstream,
	unsigned int *addrs)
{
	const struct device			*dev		= NULL;
	void __iomem				*wdma		= NULL;
	unsigned int				addrsLframe[3]	= { 0, 0, 0 };
	unsigned int				addrsPrev[3]	= { 0, 0, 0 };
	int					ret		= 0;

	dev		= stream_to_device(vstream);
	wdma		= VIOC_WDMA_GetAddress(vstream->cif.vin_path[VIN_COMP_WDMA].index);

	(void)memset(addrsLframe, 0, sizeof(addrsLframe));
	(void)memset(addrsPrev,   0, sizeof(addrsPrev));

#if defined(CONFIG_ARM64)
	addrsLframe[0]	= u64_to_u32(vstream->cif.rsvd_mem[RESERVED_MEM_LFRAME]->base);
	addrsPrev[0]	= u64_to_u32(vstream->cif.rsvd_mem[RESERVED_MEM_PREV]->base);
#else
	addrsLframe[0]	= vstream->cif.rsvd_mem[RESERVED_MEM_LFRAME]->base;
	addrsPrev[0]	= vstream->cif.rsvd_mem[RESERVED_MEM_PREV]->base;
#endif//defined(CONFIG_ARM64)

	logd(dev, "addrs of lastframe: 0x%08x, 0x%08x, 0x%08x\n",
		addrsLframe[0], addrsLframe[1], addrsLframe[2]);
	logd(dev, "addfs of  preview: 0x%08x, 0x%08x, 0x%08x\n",
		addrsPrev[0], addrsPrev[1], addrsPrev[2]);

	if (addrsLframe[0] == 0U) {
		loge(dev, "addrs of lastframe is wrong\n");
		ret = -1;
	} else {
		/* switch to write to lastframe buffer */
		VIOC_WDMA_SetImageBase(wdma, addrsLframe[0], addrsLframe[1], addrsLframe[2]);
		VIOC_WDMA_SetImageUpdate(wdma);

		/* wait until writing is done */
		msleep(60);

		/* switch to write to preview buffer */
		VIOC_WDMA_SetImageBase(wdma, addrsPrev[0], addrsPrev[1], addrsPrev[2]);
		VIOC_WDMA_SetImageUpdate(wdma);

		/* get address of lastframe */
		*addrs = addrsLframe[0];
	}

	return ret;
}

int tccvin_video_s_handover(struct tccvin_stream *vstream,
	const unsigned int *flag)
{
	int					ret		= 0;

	vstream->handover_flags = *flag;

	tccvin_print_handover_flags(vstream, vstream->handover_flags);

	return ret;
}

int tccvin_video_s_lut(struct tccvin_stream *vstream,
	const struct vin_lut *plut)
{
	const struct device			*dev		= NULL;
	int					ret		= 0;

	dev		= stream_to_device(vstream);

	if (!IS_ERR_OR_NULL(plut)) {
		/* update look-up table */
		(void)memcpy(&vstream->cif.vin_internal_lut, plut, sizeof(*plut));
	} else {
		loge(dev, "look-up table is null\n");
		ret = -1;
	}

	return ret;
}

int tccvin_video_trigger_recovery(const struct tccvin_stream *vstream,
	unsigned int index)
{
	const struct device			*dev		= NULL;
	const struct tccvin_cif			*cif		= NULL;
	void __iomem				*wdma		= NULL;
	int					ret		= 0;

	dev		= stream_to_device(vstream);
	cif		= &vstream->cif;
	wdma		= VIOC_WDMA_GetAddress(cif->vin_path[VIN_COMP_WDMA].index);

	switch (index) {
	case (unsigned int)TCCVIN_TEST_STOP_SUBDEV:
		break;
	case (unsigned int)TCCVIN_TEST_STOP_WDMA:
		/* force to disable wdma */
		VIOC_WDMA_SetImageSize(wdma, 0, 0);
		VIOC_WDMA_SetImageEnable(wdma, OFF);
		break;
	default:
		loge(dev, "recovery case (%u) is not supported\n", index);
		ret = -EINVAL;
		break;
	}

	return ret;
}

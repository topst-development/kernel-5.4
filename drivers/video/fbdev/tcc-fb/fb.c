/*
 * Copyright (C) 2008-2019 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/of_graph.h>
#include <linux/console.h>
#include <video/videomode.h>
#include <linux/tcc_math.h>

#include "panel/panel_helper.h"

#include <linux/of_reserved_mem.h>

#ifdef CONFIG_DMA_SHARED_BUFFER
#include <linux/dma-buf.h>
#endif

#ifdef CONFIG_PM
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

//#include <soc/tcc/pmap.h>

#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/tcc_scaler_ioctrl.h>
//#include <video/tcc/tcc_attach_ioctrl.h>
#include <linux/uaccess.h>

#if defined(CONFIG_VIOC_PVRIC_FBDC)
#include <video/tcc/vioc_pvric_fbdc.h>
#define FBDC_ALIGNED(w, mul) (((unsigned int)w + (mul - 1)) & ~(mul - 1))
#endif

#define FB_VERSION "1.1.2.1"
#define FB_BUF_MAX_NUM (3)
#define BYTE_TO_PAGES(prange) (((prange) + (PAGE_SIZE - 1)) >> PAGE_SHIFT)

#if defined(CONFIG_VIOC_PVRIC_FBDC)
static unsigned int fbdc_dec_1st_cfg;
//static unsigned int fbdc_wdma_need_on; /* 0: no, 1: need */
#endif

enum fb_power_status {
	/* The display controller was turned off. */
	FB_EM_POWERDOWN = 0,
	/* The display controller was turned on but rdma was disabled. */
	FB_EM_BLANK,
	/* The display controller was turned on and rdma was enabled. */
	FB_EM_UNBLANK
};

struct fb_vioc_waitq {
	wait_queue_head_t waitq;
	atomic_t state;
};

struct fb_vioc_block {
	void __iomem *virt_addr; // virtual address
	unsigned int irq_num;
	unsigned int blk_num; // block number like dma number or mixer number
};

struct fb_scale {
	/* source size */
	unsigned int srcwidth;
	unsigned int srcheight;

	/* scaler out and dest size */
	unsigned int outwidth;
	unsigned int outheight;
	unsigned int destwidth;
	unsigned int destheight;
};

struct fb_region {
	/* region information */
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;

	struct fb_scale scale;
};

struct fb_chroma_info {
	unsigned int chroma_key[3];
	unsigned int chroma_mkey[3];
};

struct fb_dp_device {
	enum fb_power_status fp_status; // true or false
	unsigned int FbDeviceType;
	unsigned int FbUpdateType;
	unsigned int FbLayerOrder;
	unsigned int FbWdmaPath;	// 0: Display device path, 1: Wdma path
	/*
	 * fb_no_pixel_alphablend
	 *  This variable indicates whether disable or enable pixel alpha blend
	 *  If it is set to 1, fb disabled pixel-alphablend function even if
	 *  rdma format is RGB888.
	 */
	unsigned int fb_no_pixel_alphablend;
	struct clk *vioc_clock;		// vioc blcok clock
	struct clk *ddc_clock;		// display blcok clock
	struct fb_vioc_block ddc_info;  // display controller address
	struct fb_vioc_block wdma_info; // display controller address
	struct fb_vioc_block wmixer_info; // wmixer address
	struct fb_vioc_block scaler_info; // scaler address
	struct fb_vioc_block rdma_info;   // rdma address
	struct fb_vioc_block fbdc_info;   // rdma address
	struct fb_region region;
	struct fb_chroma_info chroma_info;
	struct file *filp;
	unsigned int dst_addr[FB_BUF_MAX_NUM];
	unsigned int buf_idx;
#if defined(CONFIG_FB_PANEL_LVDS_TCC)
	u32 lcdc_mux;
	struct videomode vm;
#endif
};

struct fbX_par {
	dma_addr_t map_dma;
	dma_addr_t screen_dma;
	unsigned long map_size;
	void __iomem *map_cpu;
	unsigned char *screen_cpu;
	struct fb_dp_device pdata;

	int pm_state;
#if defined(CONFIG_FB_PANEL_LVDS_TCC)
	struct fb_panel *panel;
#endif
};

/*
 * Here we define the default structs fb_fix_screeninfo and fb_var_screeninfo
 * if we don't use modedb. If we do use modedb see fb_init how to use it
 * to get a fb_var_screeninfo. Otherwise define a default var as well.
 */
static struct fb_fix_screeninfo fbX_fix = {
	.id = "telechips,fb",
	.type = FB_TYPE_PACKED_PIXELS,
	.visual = FB_VISUAL_TRUECOLOR,
	.type_aux = 0,
	.xpanstep = 0,
	.ypanstep = 1,
	.ywrapstep = 0,
	.accel = FB_ACCEL_NONE,
};

enum { FB_DEV_0 = 0,
	FB_DEV_1,
	FB_DEV_2,
	FB_DEV_3,
	FB_DEV_4,
	FB_DEV_MAX,
};

static struct fb_vioc_waitq fb_waitq[FB_DEV_MAX];
static void fbX_prepare_vin_path_rdma(const struct fbX_par *par);
/* below check routine is added temporarily */
static int fbX_check_clock_ready(void);

static int fbx_turn_on_resource(struct fb_info *info);
static int fbx_turn_off_resource(struct fb_info *info);
static int fbx_blank_resource(struct fb_info *info);
static void dmabuf_vm_open(struct vm_area_struct *vma);
struct dma_buf *tsvfb_dmabuf_export(struct fb_info *info);

/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
irqreturn_t fbX_display_handler(int irq, void *dev_id)
{
	const struct fb_info *info;
	const struct fbX_par *par;
	unsigned int block_status;
	irqreturn_t ret = IRQ_HANDLED;
	#if defined(CONFIG_VIOC_PVRIC_FBDC)
	unsigned int fbdc_status;
	#endif

	if (dev_id == NULL) {
		(void)pr_err(
			"[ERROR][TSVFB] %s irq: %d dev_id:%p\n", __func__, irq,
			dev_id);
		ret = IRQ_NONE;
	}

	if (ret != IRQ_NONE) {
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		info = (struct fb_info const*)dev_id;
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		par = (struct fbX_par const*) info->par;

#if defined(CONFIG_VIOC_PVRIC_FBDC)
		fbdc_status = VIOC_PVRIC_FBDC_GetStatus(
						par->pdata.fbdc_info.virt_addr);
		if (fbdc_status & PVRICSYS_IRQ_ALL) {
			// pr_info("[INFO][VIOC_I] FBDC(%d) INT(0x%x) ------\n",
			// fbdc_dec_1st_cfg, fbdc_status);
			if (fbdc_status & PVRICSTS_IDLE_MASK) {
				VIOC_PVRIC_FBDC_ClearIrq(
						par->pdata.fbdc_info.virt_addr,
						PVRICSTS_IDLE_MASK);
			}
			if (fbdc_status & PVRICSTS_UPD_MASK) {
				VIOC_PVRIC_FBDC_ClearIrq(
						par->pdata.fbdc_info.virt_addr,
						PVRICSTS_UPD_MASK);
			}
			if (fbdc_status & PVRICSTS_TILE_ERR_MASK) {
				unsigned int rdma_enable;

				VIOC_PVRIC_FBDC_ClearIrq(
						par->pdata.fbdc_info.virt_addr,
						PVRICSTS_TILE_ERR_MASK);
				/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
				(void)pr_info("[INFO][VIOC_I] FBDC(%d) INT(0x%x) ------\n",
				 fbdc_dec_1st_cfg, PVRICSTS_TILE_ERR_MASK);
				VIOC_RDMA_GetImageEnable(
					par->pdata.rdma_info.virt_addr,
					&rdma_enable);
				if (rdma_enable) {
					VIOC_RDMA_SetImageDisable(
						par->pdata.rdma_info.virt_addr);
				}
				VIOC_CONFIG_SWReset(
						par->pdata.fbdc_info.blk_num,
						VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(
						par->pdata.fbdc_info.blk_num,
						VIOC_CONFIG_CLEAR);
				fbdc_dec_1st_cfg = 0;
			}
			if (fbdc_status & PVRICSTS_ADDR_ERR_MASK) {
				unsigned int rdma_enable;

				VIOC_PVRIC_FBDC_ClearIrq(
						par->pdata.fbdc_info.virt_addr,
						PVRICSTS_ADDR_ERR_MASK);
				/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
				(void)pr_info("[INFO][VIOC_I] FBDC(%d) INT(0x%x) ------\n",
				 fbdc_dec_1st_cfg, PVRICSTS_ADDR_ERR_MASK);
				VIOC_RDMA_GetImageEnable(
						par->pdata.rdma_info.virt_addr,
						&rdma_enable);
				if (rdma_enable) {
					VIOC_RDMA_SetImageDisable(
						par->pdata.rdma_info.virt_addr);
				}
				VIOC_CONFIG_SWReset(
						par->pdata.fbdc_info.blk_num,
						VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(
						par->pdata.fbdc_info.blk_num,
						VIOC_CONFIG_CLEAR);
				fbdc_dec_1st_cfg = 0;
			}

			if (fbdc_status & PVRICSTS_EOF_ERR_MASK) {
				unsigned int rdma_enable;

				VIOC_PVRIC_FBDC_ClearIrq(
						par->pdata.fbdc_info.virt_addr,
						PVRICSTS_EOF_ERR_MASK);
				/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
				(void)pr_info("[INFO][VIOC_I] FBDC(%d) INT(0x%x) ------\n",
				 fbdc_dec_1st_cfg, PVRICSTS_EOF_ERR_MASK);
				VIOC_RDMA_GetImageEnable(
						par->pdata.rdma_info.virt_addr,
						&rdma_enable);
				if (rdma_enable) {
					VIOC_RDMA_SetImageDisable(
						par->pdata.rdma_info.virt_addr);
				}
				VIOC_CONFIG_SWReset(
						par->pdata.fbdc_info.blk_num,
						VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(
						par->pdata.fbdc_info.blk_num,
						VIOC_CONFIG_CLEAR);
				fbdc_dec_1st_cfg = 0;
			}
		}

#endif
#if !defined(CONFIG_VIOC_PVRIC_FBDC)
		if (par->pdata.FbUpdateType == FBX_OVERLAY_UPDATE) {
			unsigned int rdma_idx = get_vioc_index(par->pdata.rdma_info.blk_num);

			block_status = vioc_intr_get_status((int)rdma_idx + VIOC_INTR_RD0);
			if ((block_status & (0x1U << (u32)VIOC_RDMA_INTR_EOFR)) != 0U) {
				(void)vioc_intr_clear((int)rdma_idx + VIOC_INTR_RD0,
						(0x1U << (u32)VIOC_RDMA_INTR_EOFR));
				if (atomic_read(&fb_waitq[
						tcc_safe_int2uint(
						info->node)].state) == 0) {
					atomic_set(&fb_waitq[
						tcc_safe_int2uint(
						info->node)].state, 1);
					wake_up_interruptible(
						&fb_waitq[
						tcc_safe_int2uint(
						info->node)].waitq);
				}
			}
		}
		if ((par->pdata.FbUpdateType != FBX_OVERLAY_UPDATE) &&
			(par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)) {
			unsigned int ddc_idx = get_vioc_index(par->pdata.ddc_info.blk_num);
			unsigned int ddc_irq_num = par->pdata.ddc_info.irq_num;

			block_status = vioc_intr_get_status((int)ddc_idx);
			if ((block_status & (0x1U << (u32)VIOC_DISP_INTR_RU)) != 0U) {
				(void)vioc_intr_clear((int)ddc_idx,
				(0x1U << VIOC_DISP_INTR_RU));

				if (atomic_read(&fb_waitq[
						tcc_safe_int2uint(
						info->node)].state)
				    == 0) {
					atomic_set(&fb_waitq[
						tcc_safe_int2uint(
						info->node)].state,
						1);
					wake_up_interruptible(
						&fb_waitq[
						tcc_safe_int2uint(
						info->node)].waitq);
				}
			}

			if ((block_status & (1U << (u32)VIOC_DISP_INTR_FU)) != 0U) {
				(void)vioc_intr_disable(
					tcc_safe_uint2int(ddc_irq_num),
					(int)ddc_idx,
					(0x1U << (u32)VIOC_DISP_INTR_FU));
				(void)vioc_intr_clear((int)ddc_idx,
						(0x1U << VIOC_DISP_INTR_FU));
			}

			if ((block_status & (0x1U << (u32)VIOC_DISP_INTR_DD)) != 0U) {
				(void)vioc_intr_clear((int)ddc_idx,
				(0x1U << VIOC_DISP_INTR_DD));
			}

			if ((block_status & (1U << (u32)VIOC_DISP_INTR_SREQ)) != 0U) {
				(void)vioc_intr_clear((int)ddc_idx,
				(1U << VIOC_DISP_INTR_SREQ));
			}
		}
#else
			if (par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE) {
			unsigned int ddc_idx = get_vioc_index(par->pdata.ddc_info.blk_num);
			unsigned int ddc_irq_num = par->pdata.ddc_info.irq_num;

			block_status = vioc_intr_get_status((int)ddc_idx);
			if ((block_status & (0x1U << (u32)VIOC_DISP_INTR_RU)) != 0U) {
				(void)vioc_intr_clear((int)ddc_idx,
				(0x1U << (u32)VIOC_DISP_INTR_RU));

				if (atomic_read(&fb_waitq[tcc_safe_int2uint(info->node)].state)
				    == 0) {
					atomic_set(&fb_waitq[tcc_safe_int2uint(info->node)].state,
						   1);
					wake_up_interruptible(
						&fb_waitq[tcc_safe_int2uint(info->node)].waitq);
				}
			}

			if ((block_status & (1U << (u32)VIOC_DISP_INTR_FU)) != 0U) {
				(void)vioc_intr_disable(
					tcc_safe_uint2int(ddc_irq_num),
					(int)ddc_idx,
					(0x1U << VIOC_DISP_INTR_FU));
				(void)vioc_intr_clear((int)ddc_idx,
						(0x1U << VIOC_DISP_INTR_FU));
			}

			if ((block_status & (0x1U << (u32)VIOC_DISP_INTR_DD)) != 0U) {
				(void)vioc_intr_clear((int)ddc_idx,
				(0x1U << VIOC_DISP_INTR_DD));
			}

			if ((block_status & (1U << (u32)VIOC_DISP_INTR_SREQ)) != 0U) {
				(void)vioc_intr_clear((int)ddc_idx,
				(1U << VIOC_DISP_INTR_SREQ));
			}
		}
#endif
	}
	return ret;
}

/**
 *      fb_check_var - Optional function. Validates a var passed in.
 *      @var: frame buffer variable screen structure
 *      @info: frame buffer structure that represents a single frame buffer
 *
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int fbX_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{

	(void)info;
	/* validate bpp */
	if (var->bits_per_pixel > 32U) {
		var->bits_per_pixel = 32U;
	} else if (var->bits_per_pixel < 16U) {
		var->bits_per_pixel = 16U;
	} else {
		//(void)var->bits_per_pixel;
	}

	/* set r/g/b positions */
	switch (var->bits_per_pixel) {
	case 16:
		var->red.offset = 11U;
		var->green.offset = 5U;
		var->blue.offset = 0U;
		var->red.length = 5U;
		var->green.length = 6U;
		var->blue.length = 5U;
		var->transp.length = 0U;
		break;
	case 32:
		var->red.offset = 16U;
		var->green.offset = 8U;
		var->blue.offset = 0U;
		var->transp.offset = 24U;
		var->red.length = 8U;
		var->green.length = 8U;
		var->blue.length = 8U;
		var->transp.length = 8U;
		break;
	default:
		var->red.length = var->bits_per_pixel;
		var->red.offset = 0U;
		var->green.length = var->bits_per_pixel;
		var->green.offset = 0U;
		var->blue.length = var->bits_per_pixel;
		var->blue.offset = 0U;
		var->transp.length = 0U;
		break;
	}

	if (var->yres_virtual < (var->yoffset + var->yres)) {
		var->yres_virtual = var->yoffset + var->yres;
	}

	return 0;
}

/**
 *      fb_set_par - Optional function. Alters the hardware state.
 *      @info: frame buffer structure that represents a single frame buffer
 *
 */
static int fbX_set_par(struct fb_info *info)
{
	int ret = 0;

	if (info->var.bits_per_pixel > 32U) {
		(void)pr_err("[ERR][TSVFB] %s : bits_per_pixel[%u] value too large\n",
		       __func__, info->var.bits_per_pixel);
		ret = -EINVAL;
	}

	switch (info->var.bits_per_pixel) {
	case 16:
		info->fix.visual = FB_VISUAL_TRUECOLOR;
		break;
	case 32:
		info->fix.visual = FB_VISUAL_TRUECOLOR;
		break;
	default:
		info->fix.visual = FB_VISUAL_PSEUDOCOLOR;
		break;
	}

	info->fix.line_length =
		info->var.xres * (info->var.bits_per_pixel / 8U);
	if (info->var.rotate != 0U) {
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		(void)pr_info(
			"[INFO][TSVFB] %s: do not support rotation\n",
			__func__);
		ret = -1;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void schedule_palette_update(
	struct fb_info *info, unsigned int regno, unsigned int val)
{
	unsigned long flags;

	(void)info;
	(void)regno;
	(void)val;

	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	local_irq_save(flags);
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	local_irq_restore(flags);
}

/* from pxafb.c */
static inline unsigned int
chan_to_field(unsigned int chan, const struct fb_bitfield *bf)
{
	chan &= 0xffffU;
	chan >>= 16U - bf->length;
	/* coverity[cert_int34_c_violation : FALSE] */
	return chan << bf->offset;
}

/**
 * fb_setcolreg - Optional function. Sets a color register.
 *	@regno: Which register in the CLUT we are programming
 *	@red: The red value which can be up to 16 bits wide
 *	@green: The green value which can be up to 16 bits wide
 *	@blue:  The blue value which can be up to 16 bits wide.
 *	@transp: If supported, the alpha value which can be up to 16 bits wide.
 *      @info: frame buffer info structure
 *
 */
static int fbX_setcolreg(
	unsigned int regno, unsigned int red, unsigned int green,
	unsigned int blue, unsigned int transp, struct fb_info *info)
{
	unsigned int val;
	int ret = 0;

	(void)transp;

	if (regno >= 256U) {/* no. of hw registers */
		ret = -EINVAL;
	}

	if (ret == 0) {
		switch (info->fix.visual) {
		case FB_VISUAL_TRUECOLOR:
			/* true-colour, use pseuo-palette */
			if (regno < 16U) {
				/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
				u32 *pal = info->pseudo_palette;

				val = chan_to_field(red, &info->var.red);
				val |= chan_to_field(green, &info->var.green);
				val |= chan_to_field(blue, &info->var.blue);

				pal[regno] = val;
			}
			break;
		case FB_VISUAL_PSEUDOCOLOR:
			/* currently assume RGB 5-6-5 mode */
			val = ((red >> 0U) & 0xf800U);
			val |= ((green >> 5U) & 0x07e0U);
			val |= ((blue >> 11U) & 0x001fU);

			// writel(val, S3C2410_TFTPAL(regno));
			schedule_palette_update(info, regno, val);
			break;

		default:
			(void)pr_err(
				     "[ERROR][TSVFB] error in %s: unknown type %d\n",
				     __func__, info->fix.visual);
			ret = -EINVAL; /* unknown type */
			break;
		}
	}

	return ret;
}


/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void fbX_vsync_activate(struct fb_info *info)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct fbX_par *par = info->par;

#if !defined(CONFIG_VIOC_PVRIC_FBDC)
	/* coverity[misra_c_2012_rule_15_7_violation : FALSE] */
	if (par->pdata.FbUpdateType == FBX_OVERLAY_UPDATE) {
		unsigned int rdma_idx = get_vioc_index(par->pdata.rdma_info.blk_num);

		(void)vioc_intr_clear((int)rdma_idx + VIOC_INTR_RD0,
			(0x1U << VIOC_RDMA_INTR_EOFR));
	}
	if ((par->pdata.FbUpdateType != FBX_OVERLAY_UPDATE) &&
	    (par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)) {
		unsigned int ddc_idx = get_vioc_index(par->pdata.ddc_info.blk_num);

		(void)vioc_intr_clear((int)ddc_idx,
				VIOC_DISP_INTR_DISPLAY);
	}
#else
	if (par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE) {
		unsigned int ddc_idx = get_vioc_index(par->pdata.ddc_info.blk_num);

		(void)vioc_intr_clear((int)ddc_idx,
				VIOC_DISP_INTR_DISPLAY);
	}
#endif
	atomic_set(&fb_waitq[tcc_safe_int2uint(info->node)].state, 0);
}

static int tsvfb_check_regions(
	const struct fb_info *info, const struct fb_var_screeninfo *var,
	unsigned int hw_width, unsigned int hw_height)
{
	struct fb_region *region;
	struct fb_scale *scale;
	struct fbX_par *par;
	unsigned int tmp;
	int ret = 0;

	if ((info == NULL) || (info->par == NULL)) {
		ret = -ENODEV;
	}

	if (ret == 0) {
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		par = info->par;
		region = &par->pdata.region;
		scale = &region->scale;

		if (region->x > 0x0FFFFFFFU) {
			ret = -EINVAL;
		}

		if ((region->x + 1U) >= hw_width) {
			ret = -EINVAL;
			dev_err(info->dev,
				"[ERROR][TSVFB] %s: position x (%u) is invalid.\r\n",
				__func__, region->x);
		}
	}

	if (ret == 0) {
		if (region->y > 0x0FFFFFFFU) {
			ret = -EINVAL;
		}
		if ((region->y + 1U) >= hw_height) {
			ret = -EINVAL;
			dev_err(
				info->dev,
				"[ERROR][TSVFB] %s: position y (%u) is invalid.\r\n",
				__func__, region->y);
		}
	}

	if (ret == 0) {
		if (par->pdata.scaler_info.virt_addr != NULL) {
			scale->srcwidth = var->xres;
			scale->srcheight = var->yres;

			scale->outwidth = region->width;
			scale->outheight = region->height;

			scale->destwidth = region->width;
			scale->destheight = region->height;

			/*
			 * Scaler limitation:
			 * width of input and output should be even value.
			 */
			if ((scale->srcwidth & 1U) != 0U) {
				ret = -EINVAL;
				dev_err(info->dev,
					"[ERROR][TSVFB] %s: The scalar does not work if the width of source image is odd.\r\n",
					__func__);
			}

			if (ret == 0) {
				if ((scale->destwidth & 1U) != 0U) {
					ret = -EINVAL;
					dev_err(info->dev,
						"[ERROR][TSVFB] %s: The scalar does not work if the width of destination image is odd.\r\n",
						__func__);
				}
			}

			if (ret == 0) {
				if ((region->x + region->width) > hw_width) {
					tmp = region->x + region->width;
					if (tmp >= hw_width) {
						tmp -= hw_width;
					}
					scale->outwidth = region->width - tmp;
				}
				if ((region->y + region->height) > hw_height) {
					tmp = region->y + region->height;
					if (tmp >= hw_width) {
						tmp -= hw_height;
					}
					scale->outheight = region->height - tmp;
				}
			}
		} else {
			scale->srcwidth = region->width;
			if ((region->x + region->width) > hw_width) {
				tmp = region->x + region->width;
				if (tmp >= hw_width) {
					tmp -= hw_width;
				}
				scale->srcwidth = region->width - tmp;
			}
			scale->srcheight = region->height;
			if ((region->y + region->height) > hw_height) {
				tmp = region->y + region->height;
				if (tmp >= hw_width) {
					tmp -= hw_height;
				}
				scale->srcheight = region->height - tmp;
			}
		}
	}
	return ret;
}


#if defined(CONFIG_VIOC_PVRIC_FBDC)
static void tca_vioc_configure_FBCDEC(
	unsigned int vioc_dec_id, unsigned int base_addr,
	void __iomem *pFBDC, void __iomem *pRDMA,
	void __iomem *pWDMA, unsigned int fmt, unsigned int rdmaPath,
	unsigned char bSet_Comp, unsigned int width, unsigned int height)
{
	if (bSet_Comp) {
		if (!fbdc_dec_1st_cfg) {
			#if defined(CONFIG_TSVFB_USES_TEST_CODE)
			if (
				VIOC_WDMA_IsImageEnable(pWDMA) &&
				VIOC_WDMA_IsContinuousMode(pWDMA)) {
				attach_data.flag = 0;
				afbc_wdma_need_on = 1;
				VIOC_WDMA_SetImageDisable(pWDMA);
			}
			#endif
			VIOC_RDMA_SetImageDisable(pRDMA);
			VIOC_CONFIG_FBCDECPath(vioc_dec_id, rdmaPath, 1);
			VIOC_PVRIC_FBDC_SetBasicConfiguration(
				pFBDC, base_addr, fmt, width, height, 0);
			VIOC_PVRIC_FBDC_SetRequestBase(pFBDC, base_addr);
			VIOC_PVRIC_FBDC_TurnOn(pFBDC);
			VIOC_PVRIC_FBDC_SetUpdateInfo(pFBDC, 1);
			fbdc_dec_1st_cfg = 1;
		}

		else {
			VIOC_PVRIC_FBDC_SetRequestBase(pFBDC, base_addr);
			VIOC_PVRIC_FBDC_SetUpdateInfo(pFBDC, 1);
			fbdc_dec_1st_cfg++;
		}
	} else {
		if (fbdc_dec_1st_cfg) {
		#if defined(CONFIG_TSVFB_USES_TEST_CODE)
			if (
				VIOC_WDMA_IsImageEnable(pWDMA) &&
				VIOC_WDMA_IsContinuousMode(pWDMA)) {
				attach_data.flag = 0;
				afbc_wdma_need_on = 1;
				VIOC_WDMA_SetImageDisable(pWDMA);
			}
		#endif
			VIOC_RDMA_SetImageDisable(pRDMA);
			VIOC_PVRIC_FBDC_TurnOFF(pFBDC);
			VIOC_CONFIG_FBCDECPath(vioc_dec_id, rdmaPath, 0);

			// VIOC_CONFIG_SWReset(vioc_dec_id, VIOC_CONFIG_RESET);
			// VIOC_CONFIG_SWReset(vioc_dec_id, VIOC_CONFIG_CLEAR);
			fbdc_dec_1st_cfg = 0;
		}
	}
}
#endif

static int fbX_activate_var(
	unsigned long dma_addr, const struct fb_var_screeninfo *var,
	const struct fb_info *info)
{
	unsigned int width, height, format, channel;
	int layer;
	const struct fb_region *region;
	const struct fb_scale *scale;
	const struct fbX_par *par;
	unsigned int dma_addr_u32;
	int ret = 0;

	if ((info == NULL) || (info->par == NULL)) {
		ret = -ENODEV;
	}

	dma_addr_u32 = tcc_safe_ulong2uint(dma_addr);

	if (ret == 0) {
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		par = info->par;
		region = &par->pdata.region;
		scale = &region->scale;

		if (par->pdata.FbWdmaPath == 0U) {
			VIOC_DISP_GetSize(par->pdata.ddc_info.virt_addr,
					  &width, &height);
		} else {
			/* WDMA Path */
			width = var->xres;
			height = var->yres;
		}

		if ((width == 0U) || (height == 0U)) {
			(void)pr_err(
				     "[ERROR][TSVFB] error in %s: vioc invalid status (%dx%d)\n",
				     __func__, width, height);
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		if ((par->pdata.FbWdmaPath == 1U)
		    && (par->pdata.wdma_info.virt_addr != NULL)) {
			if (!VIOC_WDMA_IsImageEnable(par->pdata.wdma_info.virt_addr)) {
				/* WDMA Path is not Ready */
				ret = -ENODEV;
			}
		}
	}

	if (ret == 0) {
		ret = tsvfb_check_regions(info, var, width, height);
		if (ret < 0) {
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		switch (var->bits_per_pixel) {
		case 16U:
			format = VIOC_IMG_FMT_RGB565;
			break;
		case 32U:
			format = VIOC_IMG_FMT_ARGB8888;
			break;
		default:
			(void)pr_err("[ERROR][TSVFB] error in %s: can not support bpp %d\n",
				     __func__, var->bits_per_pixel);
			ret = -EINVAL;
			break;
		}
	}

	if (ret == 0) {
		if (par->pdata.scaler_info.virt_addr != NULL) {
			if (
			    VIOC_CONFIG_GetScaler_PluginToRDMA(
					par->pdata.rdma_info.blk_num) < 0) {
				unsigned int enable;
				VIOC_RDMA_GetImageEnable(
				 par->pdata.rdma_info.virt_addr, &enable);
				if (enable != 0U) {
					VIOC_RDMA_SetImageDisable(
						par->pdata.rdma_info.virt_addr);
				}
				(void)VIOC_CONFIG_PlugIn(
					par->pdata.scaler_info.blk_num,
					par->pdata.rdma_info.blk_num);
			}
		}

#if defined(CONFIG_VIOC_PVRIC_FBDC)
		tca_vioc_configure_FBCDEC(
		 par->pdata.fbdc_info.blk_num, dma_addr_u32,
		 par->pdata.fbdc_info.virt_addr, par->pdata.rdma_info.virt_addr,
		 par->pdata.wdma_info.virt_addr, format,
		 par->pdata.rdma_info.blk_num, var->reserved[3], width, height);
#endif
		VIOC_RDMA_SetImageIntl(par->pdata.rdma_info.virt_addr, 0);
		VIOC_RDMA_SetImageFormat(par->pdata.rdma_info.virt_addr, format);

		VIOC_RDMA_SetImageSize(
				par->pdata.rdma_info.virt_addr,
				scale->srcwidth, scale->srcheight);
		VIOC_RDMA_SetImageOffset(
			par->pdata.rdma_info.virt_addr, format, var->xres);
#if defined(CONFIG_VIOC_PVRIC_FBDC)
		if (fbdc_dec_1st_cfg == 1)
#endif
			VIOC_RDMA_SetImageBase(
				par->pdata.rdma_info.virt_addr, dma_addr_u32, 0, 0);
		if (format == VIOC_IMG_FMT_ARGB8888) {
			VIOC_RDMA_SetImageAlphaSelect(
					par->pdata.rdma_info.virt_addr, 1);
			VIOC_RDMA_SetImageAlphaEnable(
				par->pdata.rdma_info.virt_addr,
				(par->pdata.fb_no_pixel_alphablend != 0U) ?
					0U : 1U);
		} else {
			VIOC_RDMA_SetImageAlphaEnable(
					par->pdata.rdma_info.virt_addr, 0);
		}
		VIOC_RDMA_SetImageY2REnable(par->pdata.rdma_info.virt_addr, 0);
		VIOC_RDMA_SetImageR2YEnable(par->pdata.rdma_info.virt_addr, 0);

		/* Display Device Path */
		if (par->pdata.FbWdmaPath == 0U) {
			int image_num;
			channel = get_vioc_index(par->pdata.rdma_info.blk_num)
				- (4U * get_vioc_index(par->pdata.ddc_info.blk_num));
			VIOC_WMIX_SetPosition(
				par->pdata.wmixer_info.virt_addr, channel,
				region->x, region->y);

			image_num = VIOC_RDMA_GetImageNum(
						par->pdata.rdma_info.blk_num);

			layer = VIOC_WMIX_GetLayer(
						par->pdata.wmixer_info.blk_num,
						tcc_safe_int2uint(image_num));
			switch (layer) {
			case 3:
				channel = 0U;
				break;
			case 2:
				channel = 1U;
				break;
			case 1:
				if (VIOC_WMIX_GetMixerType(
					par->pdata.wmixer_info.blk_num) != 0) {
					/* 4 to 2 mixer */
					channel = 2U;
				} else {
					/* 2 to 2 mixer */
					channel = 0U;
				}
				break;
			default:
				(void)pr_err("[ERROR][TSVFB] %s: layer[%d] is not valid\n",
					     __func__, layer);
				break;
			}

			if (format == VIOC_IMG_FMT_RGB565) {
				VIOC_WMIX_SetChromaKey(
					par->pdata.wmixer_info.virt_addr,
					channel, 1 /*ON*/,
					par->pdata.chroma_info.chroma_key[0],
					par->pdata.chroma_info.chroma_key[1],
					par->pdata.chroma_info.chroma_key[2],
					par->pdata.chroma_info.chroma_mkey[0],
					par->pdata.chroma_info.chroma_mkey[1],
					par->pdata.chroma_info.chroma_mkey[2]);

				/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
				/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
				/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
				/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
				/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
				/* coverity[cert_dcl37_c_violation : FALSE] */
				pr_debug("[INF][FBX][%s]RGB565 Set Chromakey\n"
					 "key R : 0x%x\nkey G : 0x%x\nkey B : 0x%x\n"
					 "mkey R : 0x%x\nmkey G : 0x%x\nmkey B : 0x%x\n",
					 __func__,
					 par->pdata.chroma_info.chroma_key[0],
					 par->pdata.chroma_info.chroma_key[1],
					 par->pdata.chroma_info.chroma_key[2],
					 par->pdata.chroma_info.chroma_mkey[0],
					 par->pdata.chroma_info.chroma_mkey[1],
					 par->pdata.chroma_info.chroma_mkey[2]);
			} else { // VIOC_IMG_FMT_ARGB8888
				VIOC_WMIX_SetChromaKey(
						       par->pdata.wmixer_info.virt_addr,
						       channel, 0 /*OFF*/,
						       0, 0, 0, 0, 0, 0);
				/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
				/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
				/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
				/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
				/* coverity[cert_dcl37_c_violation : FALSE] */
				pr_debug("[INF][FBX]ARGB888 Disable Chromakey\n");
			}
			VIOC_WMIX_SetUpdate(par->pdata.wmixer_info.virt_addr);
		}

		if (par->pdata.scaler_info.virt_addr != NULL) {
			VIOC_SC_SetBypass(par->pdata.scaler_info.virt_addr, 0);
			VIOC_SC_SetDstSize(
					   par->pdata.scaler_info.virt_addr,
					   scale->destwidth, scale->destheight);
			VIOC_SC_SetOutSize(
					   par->pdata.scaler_info.virt_addr,
					   scale->outwidth, scale->outheight);
			VIOC_SC_SetOutPosition(par->pdata.scaler_info.virt_addr, 0, 0);
			VIOC_SC_SetUpdate(par->pdata.scaler_info.virt_addr);
		}

		VIOC_RDMA_SetImageEnable(par->pdata.rdma_info.virt_addr);
	}

	return ret;
}

/**
 *      fb_pan_display - NOT a required function. Pans the display.
 *      @var: frame buffer variable screen structure
 *      @info: frame buffer structure that represents a single frame buffer
 *
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int fbX_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct fbX_par *par = info->par;
	unsigned long dma_addr;
	int ret = 0;
	int no_wait = 0;

	if (par->pdata.fp_status != FB_EM_UNBLANK) {
		ret = -ENODEV;
	}

	if (ret == 0) {
		if (var->xres > 0xFFFFU) { //65536
			(void)pr_err("[ERR][TSVFB] %s : xres[%u] value too large\n",
			       __func__, var->xres);
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		if (var->bits_per_pixel > 32U) {
			(void)pr_err("[ERR][TSVFB] %s : bits_per_pixel[%u] value too large\n",
			       __func__, var->bits_per_pixel);
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
#if defined(CONFIG_VIOC_PVRIC_FBDC)
		if (var->reserved[3] != 0) {
			unsigned int bufID = 0;
			unsigned int FBCBufSize =
				BYTE_TO_PAGES(
					      (var->xres * var->yres
					       * (var->bits_per_pixel / 8))
					      + FBDC_ALIGNED(
							     (var->xres * var->yres / 64), 256))
				* PAGE_SIZE;

			if (var->yoffset == 0)
				bufID = 0;
			else if (var->yoffset == var->yres)
				bufID = 1;
			else
				bufID = 2;
			dma_addr = par->map_dma + FBCBufSize * bufID
				+ FBDC_ALIGNED((var->xres * var->yres / 64), 256);
			// dma_addr = par->map_dma + (FBDC_ALIGNED((var->xres *
			// var->yres/64), 256)*bufnum);
			(void)pr_err(
				     "%s yoffset:%d yres:%d dma_addr:0x%lx, FBCBufSize:%d bufnum:%d\n",
				     __func__, var->yoffset, var->yres, dma_addr, FBCBufSize,
				     bufID);
		} else {
			dma_addr = par->map_dma
				+ ((u64)var->xres * var->yoffset * (var->bits_per_pixel
				   / 8U));
		}
#else
		dma_addr = par->map_dma
			+ ((u64)var->xres * var->yoffset * (var->bits_per_pixel / 8U));
#endif
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		dev_dbg(info->dev,
			"[INFO][TSVFB] %s: fb%d addr:0x%lx - %s updateType:0x%x\n",
			__func__, info->node, dma_addr,
			((var->activate == FB_ACTIVATE_VBL) ? "VBL" : "NOPE"),
			par->pdata.FbUpdateType);

		switch (par->pdata.FbUpdateType) {
		case FBX_RDMA_UPDATE:
		case FBX_OVERLAY_UPDATE:
		case FBX_NOWAIT_UPDATE:
			ret = fbX_activate_var(dma_addr, var, info);
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}

	if (ret == 0) {
#if defined(CONFIG_FB_PANEL_LVDS_TCC)
		if (par->panel != NULL) {
			VIOC_DISP_TurnOn(par->pdata.ddc_info.virt_addr);
		}
#endif

		if (par->pdata.fp_status == FB_EM_POWERDOWN) {
			no_wait = 1;
		}
		if (no_wait == 0) {
			if ((par->pdata.fp_status == FB_EM_BLANK)
			    && (par->pdata.FbUpdateType != FBX_RDMA_UPDATE)) {
				no_wait = 1;
			}
		}
		if (no_wait == 0) {
			if ((var->activate & FB_ACTIVATE_VBL) != 0U) {
				fbX_vsync_activate(info);
				if (atomic_read(&fb_waitq[
				tcc_safe_int2uint(info->node)].state) != 0) {
					no_wait = 1;
				}
			}
		}
		if (no_wait == 0) {
			if ((var->activate & (u32)FB_ACTIVATE_VBL) != 0U) {
				/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
				/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
				/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
				/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
				/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				/* coverity[misra_c_2012_rule_15_4_violation : FALSE] */
				/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
				/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
				/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
				/* coverity[cert_dcl37_c_violation : FALSE] */
				/* coverity[cert_int31_c_violation : FALSE] */
				/* coverity[cert_int02_c_violation : FALSE] */
				/* coverity[cert_pre31_c_violation : FALSE] */
				if (wait_event_interruptible_timeout(
				  fb_waitq[tcc_safe_int2uint(info->node)].waitq,
				  atomic_read(&fb_waitq[
				  tcc_safe_int2uint(info->node)].state) == 1,
				  msecs_to_jiffies(50)) == 0) {
					dev_err(info->dev,
						"[INFO][TSVFB] %s: vsync wait queue timeout\n",
						__func__);
				}
			}
		}
	}
	return ret;
}

/**
 *      fb_blank - NOT a required function. Blanks the display.
 *      @blank_mode: the blank mode we want.
 *      @info: frame buffer structure that represents a single frame buffer
 *
 */
static int fbX_blank(int blank, struct fb_info *info)
{
#if defined(CONFIG_FB_PANEL_LVDS_TCC)
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct fbX_par *par = info->par;
#endif
	int ret = 0;

#ifdef CONFIG_PM
	(void)pm_runtime_get_sync(info->dev);

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		/* screen: blanked,   hsync: off, vsync: off */
		dev_info(info->dev, "FB_BLANK_POWERDOWN\r\n");
#if defined(CONFIG_FB_PANEL_LVDS_TCC)
		if (par->panel != NULL) {
			(void)fb_panel_disable(par->panel);
			(void)fb_panel_unprepare(par->panel);
		}
#endif
		ret = fbx_turn_off_resource(info);
		break;
	case FB_BLANK_NORMAL:
		/* screen: blanked,   hsync: on,  vsync: on */
		dev_info(info->dev, "FB_BLANK_NORMAL\r\n");
		ret = fbx_blank_resource(info);
		break;
	case FB_BLANK_UNBLANK:
		/* screen: unblanked, hsync: on,  vsync: on */
		dev_info(info->dev, "FB_BLANK_UNBLANK\r\n");
#if defined(CONFIG_FB_PANEL_LVDS_TCC)
		if (par->panel != NULL) {
			(void)fb_panel_prepare(par->panel);
		}
#endif
		ret = fbx_turn_on_resource(info);
#if defined(CONFIG_FB_PANEL_LVDS_TCC)
		if (par->panel != NULL) {
			(void)fb_panel_enable(par->panel);
		}
#endif
		break;
	case FB_BLANK_HSYNC_SUSPEND:
	/* screen: blanked,   hsync: off, vsync: on */
	case FB_BLANK_VSYNC_SUSPEND:
		/* screen: blanked,   hsync: on,  vsync: off */
		dev_err(info->dev, " not support to blank h/v sync \r\n");
		ret = -EINVAL;
		break;
	default:
		dev_err(info->dev,
			"[ERROR][TSVFB] error in %s: Invaild blank_mode %d\n",
			__func__, blank);
		ret = -EINVAL;
		break;
	}

	(void)pm_runtime_put_sync(info->dev);
#endif

	return ret;
}

static int tsvfb_ioctl(
	struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct fbX_par *par = info->par;
	int ret;

	switch (cmd) {
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	case FBIOSET_NO_PIXELALPHABLEND:
		{
			unsigned int no_pixel_alphablend;

			if (
				copy_from_user(
					&no_pixel_alphablend,
					/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
					/* coverity[cert_int36_c_violation : FALSE] */
					(void __user *)arg,
					sizeof(unsigned int)) != 0U) {
				dev_err(
				info->dev,
				"[ERROR][TSVFB] %s: FBIOSET_NO_PIXELALPHABLEND - Failed get user data\n",
				__func__);
				ret =  -EFAULT;
				break;
			}
			par->pdata.fb_no_pixel_alphablend = no_pixel_alphablend;
			ret = fbX_pan_display(&info->var, info);
		}
		break;
	default:
		dev_err(
			info->dev,
			"[ERROR][TSVFB] %s: ioctl: Unknown [%u/0x%X]\n",
			__func__, cmd, cmd);
		ret = -ENODEV;
		break;
	}

	return ret;
}

#ifdef CONFIG_DMA_SHARED_BUFFER
#ifdef CONFIG_ANDROID
static struct sg_table *fb_ion_map_dma_buf(
	struct dma_buf_attachment *attachment,
	enum dma_data_direction direction)
{
	int err;
	struct sg_table *table;
	struct fb_info *info = (struct fb_info *)attachment->dmabuf->priv;

	if (!info) {
		(void)pr_err("[ERROR][TSVFB] %s: info is null\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);

	err = sg_alloc_table(table, 1, GFP_KERNEL);
	if (err) {
		kfree(table);
		return ERR_PTR(err);
	}

	sg_set_page(table->sgl, NULL, info->fix.smem_len, 0);
	sg_dma_address(table->sgl) = info->fix.smem_start;
	debug_dma_map_sg(
		NULL, table->sgl, table->nents, table->nents,
		DMA_BIDIRECTIONAL);

	return table;
}

static void fb_ion_unmap_dma_buf(
	struct dma_buf_attachment *attachment, struct sg_table *table,
	enum dma_data_direction direction)
{
	debug_dma_unmap_sg(NULL, table->sgl, table->nents, DMA_BIDIRECTIONAL);
	sg_free_table(table);

	kfree(table);
}

static int fb_ion_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	return -ENOMEM;
}

static void fb_ion_dma_buf_release(struct dma_buf *dmabuf)
{
	/* Nothing TODO */
}

static void *fb_ion_dma_buf_kmap(struct dma_buf *dmabuf, unsigned long offset)
{
	return 0;
}

struct dma_buf_ops new_fb_dma_buf_ops = {
	.map_dma_buf = fb_ion_map_dma_buf,
	.unmap_dma_buf = fb_ion_unmap_dma_buf,
	.mmap = fb_ion_mmap,
	.release = fb_ion_dma_buf_release,
	.map_atomic = fb_ion_dma_buf_kmap,
	.map = fb_ion_dma_buf_kmap,
};

struct dma_buf *tsvfb_dmabuf_export(struct fb_info *info)
{
	struct dma_buf *dmabuf;

	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	exp_info.ops = &new_fb_dma_buf_ops;
	exp_info.size = info->fix.smem_len;
	exp_info.flags = O_RDWR;
	exp_info.priv = info;
	dmabuf = dma_buf_export(&exp_info);
	return dmabuf;
}
#else
static int dmabuf_attach(
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct dma_buf *buf,
	struct dma_buf_attachment *attach)
{
	(void)buf;
	(void)attach;
	return 0;
}

static void
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
dmabuf_detach(struct dma_buf *buf, struct dma_buf_attachment *attach)
{
	(void)buf;
	(void)attach;
}

static struct sg_table *dmabuf_map_dma_buf(
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct dma_buf_attachment *attach, enum dma_data_direction dir)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct fb_info *priv = attach->dmabuf->priv;
	struct sg_table *sgt;
	int sg_ret = 0;

	(void)dir;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	sgt = (struct sg_table *)kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (sgt == NULL) {
		sg_ret = -1;
	}

	if (sg_ret == 0) {
		/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
		if (sg_alloc_table(sgt, 1, GFP_KERNEL) != 0) {
			kfree(sgt);
			sg_ret = -1;
			sgt = NULL;
		}
	}

	if(sg_ret == 0) {
		sg_dma_address(sgt->sgl) = priv->fix.smem_start;
		sg_dma_len(sgt->sgl) = priv->fix.smem_len;
	}

	return sgt;
}

static void dmabuf_unmap_dma_buf(
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct dma_buf_attachment *attach, struct sg_table *sgt,
	enum dma_data_direction dir)
{
	(void)attach;
	(void)dir;

	sg_free_table(sgt);
	kfree(sgt);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void dmabuf_release(struct dma_buf *buf)
{
	(void)buf;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void *dmabuf_map(struct dma_buf *buf, unsigned long page_l)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct fb_info *priv = buf->priv;
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return priv->screen_base + page_l;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void dmabuf_unmap(struct dma_buf *buf, unsigned long page_l, void *vaddr)
{
	(void)buf;
	(void)page_l;
	(void)vaddr;
}

//static void *dmabuf_map_atomic(struct dma_buf *buf, unsigned long page)
//{
//	return NULL;
//}

//static void
//dmabuf_unmap_atomic(struct dma_buf *buf, unsigned long page, void *vaddr)
//{
//}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */ //
static void dmabuf_vm_open(struct vm_area_struct *vma)
{
	//unused variable
	(void)vma;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */ //
static void dmabuf_vm_close(struct vm_area_struct *vma)
{
	//unused variable
	(void)vma;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */ //
static vm_fault_t dmabuf_vm_fault(struct vm_fault *vmf)
{
	(void)vmf;
	return 0;
}

static const struct vm_operations_struct dmabuf_vm_ops = {
	.open = dmabuf_vm_open,
	.close = dmabuf_vm_close,
	.fault = dmabuf_vm_fault,
};

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int dmabuf_mmap(struct dma_buf *buf, struct vm_area_struct *vma)
{
	pgprot_t prot = vm_get_page_prot(vma->vm_flags);
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct fb_info *priv = buf->priv;

	vma->vm_flags |= (u32)VM_IO | (u32)VM_PFNMAP
		| (u32)VM_DONTEXPAND | (u32)VM_DONTDUMP;
	vma->vm_ops = &dmabuf_vm_ops;
	vma->vm_private_data = priv;
	vma->vm_page_prot = pgprot_writecombine(prot);

	return remap_pfn_range(
		vma, vma->vm_start, priv->fix.smem_start >> PAGE_SHIFT,
		/* coverity[cert_int30_c_violation : FALSE] */
		vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void *dmabuf_vmap(struct dma_buf *buf)
{
	(void)buf;
	return NULL;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void dmabuf_vunmap(struct dma_buf *buf, void *vaddr)
{
	(void)buf;
	(void)vaddr;
}

static const struct dma_buf_ops dmabuf_ops = {
	.attach = dmabuf_attach,
	.detach = dmabuf_detach,
	.map_dma_buf = dmabuf_map_dma_buf,
	.unmap_dma_buf = dmabuf_unmap_dma_buf,
	.release = dmabuf_release,
	.map = dmabuf_map,
	.unmap = dmabuf_unmap,
	.mmap = dmabuf_mmap,
	.vmap = dmabuf_vmap,
	.vunmap = dmabuf_vunmap,
};

struct dma_buf *tsvfb_dmabuf_export(struct fb_info *info)
{
	struct dma_buf *dmabuf;

	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	exp_info.ops = &dmabuf_ops;
	exp_info.size = info->fix.smem_len;
	exp_info.flags = O_RDWR;
	exp_info.priv = info;
	dmabuf = dma_buf_export(&exp_info);
	return dmabuf;
}
#endif
#endif // CONFIG_DMA_SHARED_BUFFER

/*
 *  Frame buffer operations
 */
static struct fb_ops tsvfb_fb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = fbX_check_var,
	.fb_set_par = fbX_set_par,
	.fb_blank = fbX_blank,
	.fb_setcolreg = fbX_setcolreg,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_pan_display = fbX_pan_display,
	.fb_ioctl = tsvfb_ioctl,
#ifdef CONFIG_DMA_SHARED_BUFFER
	.fb_dmabuf_export = tsvfb_dmabuf_export,
#endif
};

#ifdef CONFIG_OF
static const struct of_device_id fbX_of_match[] = {
	{
		.compatible = "telechips,fb"},
	{ }
};
MODULE_DEVICE_TABLE(of, fbX_of_match);
#endif

/*
 * fb_map_video_memory():
 *	Allocates the DRAM memory for the frame buffer.  This buffer is
 *	remapped into a non-cached, non-buffered, memory region to
 *	allow palette and pixel writes to occur without flushing the
 *	cache.  Once this area is remapped, all virtual memory
 *	access to the video memory should occur at the new region.
 */
static int __init fb_map_video_memory(struct fb_info *info)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct fbX_par *par = info->par;
	struct device_node *mem_region;
	const char *name;
	const struct reserved_mem *rmem;
	int ret = 0;

	mem_region = of_parse_phandle(info->dev->of_node, "memory-region", 0);
	if (mem_region != NULL) {
		rmem = of_reserved_mem_lookup(mem_region);
		if (rmem == NULL) {
			(void)pr_err("[ERR][TSVFB] %s: memory allocation is failed\n",
			       __func__);
			ret = -ENOMEM;
		}

		if (ret == 0) {
			if (of_property_read_string(mem_region, "pmap-name", &name) != 0) {
				(void)pr_err("[ERR][TSVFB] %s: can't find pmap name\n",
					     __func__);
				ret = -EINVAL;
			}
		}

		if (ret == 0) {
			par->map_dma = rmem->base;
			par->map_size = rmem->size;
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_13_1_violation : FALSE] */
			par->map_cpu = ioremap_nocache(par->map_dma, par->map_size);
			#if defined(CONFIG_ARM64)
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			(void)pr_info("[INFO][TSVFB] %s: %s: 0x%08llx ~ 0x%08llx (0x%08llx)\n",
				      __func__, name, rmem->base, rmem->base + rmem->size,
				      rmem->size);
			#else
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			(void)pr_info("[INFO][TSVFB] %s: %s: 0x%08x ~ 0x%08x (0x%08x)\n",
				      __func__, name, rmem->base, rmem->base + rmem->size,
				      rmem->size);
			#endif
		}
	} else {
		(void)pr_err("[ERR][TSVFB] %s : can't find pmap,tsvfb\n", __func__);
		ret = -EINVAL;
	}

	if (ret == 0) {
		if (par->map_cpu != NULL) {
			/*
			 * prevent initial garbage on screen
			 */
#if !defined(CONFIG_TCC803X_CA7S) && !defined(CONFIG_TCC805X_CA53Q)
			memset_io(par->map_cpu, 0x00, par->map_size);
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			(void)pr_info("[INFO][TSVFB] %s: clear fb mem\n", __func__);
#endif
			par->screen_dma = par->map_dma;
			/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
			info->screen_base = (char __iomem*)par->map_cpu;
			info->fix.smem_start = par->screen_dma;
			info->fix.smem_len = (u32)(par->map_size & UINT_MAX);
		} else{ //par->map_cpu == NULL
			ret = -ENOMEM;
		}
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static inline void fb_unmap_video_memory(struct fb_info *info)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct fbX_par *par = info->par;
	if (par->map_cpu != NULL){
	//	dma_free_writecombine(
	//		info->dev, par->map_size, par->map_cpu, par->map_dma);
	}
}

static void fb_unregister_isr(struct fb_info *info)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct fbX_par *par = info->par;

#if !defined(CONFIG_VIOC_PVRIC_FBDC)
	if (par->pdata.FbUpdateType == FBX_OVERLAY_UPDATE) {
		unsigned int rdma_idx = get_vioc_index(par->pdata.rdma_info.blk_num);
		unsigned int rdma_irq_num = par->pdata.rdma_info.irq_num;

		(void)vioc_intr_clear((int)rdma_idx,
			(0x1U << VIOC_RDMA_INTR_EOFR));
		(void)vioc_intr_disable(
			tcc_safe_uint2int(rdma_irq_num),
			(int)rdma_idx + VIOC_INTR_RD0,
			(0x1U << VIOC_RDMA_INTR_EOFR));
		(void)free_irq(par->pdata.rdma_info.irq_num, info);
	}
	if ((par->pdata.FbUpdateType != FBX_OVERLAY_UPDATE) &&
	    (par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)) {
		unsigned int ddc_idx = get_vioc_index(par->pdata.ddc_info.blk_num);
		unsigned int ddc_irq_num = par->pdata.ddc_info.irq_num;

		(void)vioc_intr_clear((int)ddc_idx,
			VIOC_DISP_INTR_DISPLAY);
		(void)vioc_intr_disable(
			tcc_safe_uint2int(ddc_irq_num),
			(int)ddc_idx,
			VIOC_DISP_INTR_DISPLAY);
		(void)free_irq(par->pdata.ddc_info.irq_num, info);
	}
#else
	if (par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE) {
		unsigned int ddc_idx = get_vioc_index(par->pdata.ddc_info.blk_num);
		unsigned int ddc_irq_num = par->pdata.ddc_info.irq_num;

		(void)vioc_intr_clear(tcc_safe_uint2int(ddc_idx),
			VIOC_DISP_INTR_DISPLAY);
		(void)vioc_intr_disable(
			tcc_safe_uint2int(ddc_irq_num),
			(int)ddc_idx,
			VIOC_DISP_INTR_DISPLAY);
		free_irq(par->pdata.ddc_info.irq_num, info);
	}
#endif
}

static int fb_register_isr(struct fb_info *info)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct fbX_par *par = info->par;
	int ret = 0;

#if !defined(CONFIG_VIOC_PVRIC_FBDC)
	if (par->pdata.FbUpdateType == FBX_OVERLAY_UPDATE) {
		unsigned int rdma_idx = get_vioc_index(par->pdata.rdma_info.blk_num);
		unsigned int rdma_irq_num = par->pdata.rdma_info.irq_num;

		(void)vioc_intr_clear((int)rdma_idx + VIOC_INTR_RD0,
			(0x1U << (u32)VIOC_RDMA_INTR_EOFR));
		if (request_irq(
			    par->pdata.rdma_info.irq_num, fbX_display_handler,
			    IRQF_SHARED, info->fix.id, info)
		    < 0) {
			(void)pr_err(
				"[ERROR][TSVFB] error in %s: can not register isr\n",
				__func__);
			ret = -EINVAL;
		}
		if (ret == 0) {
			(void)vioc_intr_enable(
				tcc_safe_uint2int(rdma_irq_num),
				(int)rdma_idx + VIOC_INTR_RD0,
				(0x1U << (u32)VIOC_RDMA_INTR_EOFR));
		}
	}
	if (ret == 0) {
		if ((par->pdata.FbUpdateType != FBX_OVERLAY_UPDATE) &&
		    (par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE)) {
			unsigned int ddc_idx = get_vioc_index(par->pdata.ddc_info.blk_num);
			unsigned int ddc_irq_num = par->pdata.ddc_info.irq_num;

			(void)vioc_intr_clear((int)ddc_idx,
					VIOC_DISP_INTR_DISPLAY);
			if (request_irq(
			    par->pdata.ddc_info.irq_num, fbX_display_handler,
			    IRQF_SHARED, info->fix.id, info)
			    < 0) {
				(void)pr_err("[ERROR][TSVFB] error in %s: can not register isr\n",
				__func__);
				ret = -EINVAL;
			}
			if (ret == 0) {
				(void)vioc_intr_enable(
					tcc_safe_uint2int(ddc_irq_num),
					(int)ddc_idx,
					VIOC_DISP_INTR_DISPLAY);
			}
		}
	}
#else
	if (par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE) {
		unsigned int ddc_idx = get_vioc_index(par->pdata.ddc_info.blk_num);
		unsigned int ddc_irq_num = par->pdata.ddc_info.irq_num;

		vioc_intr_clear((int)ddc_idx,
			VIOC_DISP_INTR_DISPLAY);
		if (request_irq(
			    par->pdata.ddc_info.irq_num, fbX_display_handler,
			    IRQF_SHARED, info->fix.id, info)
		    < 0) {
			(void)pr_err(
				"[ERROR][TSVFB] error in %s: can not register isr\n",
				__func__);
			ret = -EINVAL;
		}
		if (ret == 0) {
			vioc_intr_enable(
				tcc_safe_uint2int(ddc_irq_num),
				(int)ddc_idx,
				VIOC_DISP_INTR_DISPLAY);
		}
	}
#endif
	return ret;
}

#ifdef CONFIG_OF
static int fb_dt_parse_data(struct fb_info *info)
{
	int ret = 0;
	unsigned int index;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct fbX_par *par = info->par;
	struct device_node *np = NULL;
	unsigned int property_idx = 1U;
	int chroma_idx;
#ifdef CONFIG_ARCH_TCC803X
#ifdef CONFIG_FB_NEW_DISP1
	if (strcmp("/fb@0", of_node_full_name(info->dev->of_node)) == 0) {
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		(void)pr_info("%s fb0 disp1 support\n", __func__);
		property_idx = 2U;
	}
#endif
#endif

	if (info->dev->of_node == NULL) {
		ret = -ENODEV;
	}

	if (ret == 0) {
		if (of_property_read_u32(
			    info->dev->of_node, "xres", &info->var.xres) < 0) {
			(void)pr_err(
				"[ERROR][TSVFB] error in %s: can nod find xres\n",
				__func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		info->var.xres_virtual = info->var.xres;

		if (of_property_read_u32(
			    info->dev->of_node, "yres", &info->var.yres) < 0) {
			(void)pr_err(
				"[ERROR][TSVFB] error in %s: can nod find yres\n",
				__func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		if (of_property_read_u32(
			    info->dev->of_node, "bpp",
			    &info->var.bits_per_pixel) < 0) {
			(void)pr_err(
				"[ERROR][TSVFB] error in %s: can nod find bpp\n",
				__func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		//get chroma key value only when bpp is 16
		if (info->var.bits_per_pixel == 16U) {
			for (chroma_idx = 0 ; chroma_idx < 3 ; chroma_idx++) {
				if (of_property_read_u32_index(
					info->dev->of_node, "chroma-key",
					(u32)chroma_idx,
					&par->pdata.chroma_info.chroma_key[chroma_idx])
					< 0) {
					(void)pr_err("[ERROR][FBX] error in %s: can nod find chroma-key\n",
						__func__);
					ret = -ENODEV;
				}
			}
		}
	}

	if (ret == 0) {
		if (info->var.bits_per_pixel == 16U) {
			for (chroma_idx = 0 ; chroma_idx < 3 ; chroma_idx++) {
				if (of_property_read_u32_index(
					info->dev->of_node, "chroma-mkey",
					(u32)chroma_idx,
					&par->pdata.chroma_info.chroma_mkey[chroma_idx]
					) < 0) {
					(void)pr_err("[ERROR][FBX] error in %s: can nod find chroma-mkey\n",
						__func__);
					ret = -ENODEV;
				}
			}
		}
	}

	if (ret == 0) {
		if (of_property_read_u32(info->dev->of_node, "mode", &index) != 0) {
			(void)pr_err(
				"[ERROR][TSVFB] error in %s: can nod find mode\n",
				__func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		switch (index) {
		case FBX_SINGLE:
			info->var.yres_virtual = info->var.yres;
			break;
		case FBX_DOUBLE:
			info->var.yres_virtual = info->var.yres * 2U;
			break;
		case FBX_TRIPLE:
			info->var.yres_virtual = info->var.yres * 3U;
			break;
		default:
			(void)pr_err(
				"[ERROR][TSVFB] error in %s: Invaild fb mode(%d)\n",
				__func__, index);
			break;
		}
		info->fix.line_length =
			info->var.xres * info->var.bits_per_pixel / 8U;

		if (of_property_read_u32(
			    info->dev->of_node, "update-type",
			    &par->pdata.FbUpdateType) < 0) {
			(void)pr_err(
				"[ERROR][TSVFB] error in %s: can nod find update-type\n",
				__func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		if (of_property_read_u32_index(
			    info->dev->of_node, "device-priority", 0,
			    &par->pdata.FbDeviceType) < 0) {
			(void)pr_err(
				"[ERROR][TSVFB] error in %s: can nod find update-type\n",
				__func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		np = of_parse_phandle(info->dev->of_node, "telechips,disp", 0);
		if (np != NULL) {
			(void)of_property_read_u32_index(
				info->dev->of_node, "telechips,disp",
				property_idx, &par->pdata.ddc_info.blk_num);
			par->pdata.ddc_info.virt_addr = VIOC_DISP_GetAddress(
				par->pdata.ddc_info.blk_num);
#if !defined(CONFIG_VIOC_PVRIC_FBDC)
			if (par->pdata.FbUpdateType != FBX_OVERLAY_UPDATE)
#endif
			{
				unsigned int ddc_idx = get_vioc_index(
						par->pdata.ddc_info.blk_num);
				par->pdata.ddc_info.irq_num =
					irq_of_parse_and_map(np,
						tcc_safe_uint2int(ddc_idx));
			}
			par->pdata.vioc_clock =
				of_clk_get_by_name(np, "ddi-clk");
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_11_9_violation : FALSE] */
			BUG_ON(par->pdata.vioc_clock == NULL);
			par->pdata.FbWdmaPath = 0;
		} else {
			np = of_parse_phandle(
				info->dev->of_node, "telechips,wdma", 0);
			if (np == NULL) {
				(void)pr_err(
					"[ERROR][TSVFB] error in %s: can not find telechips,wdma\n",
					__func__);
				ret = -ENODEV;
			}

			if (ret == 0) {
				if (par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE) {
					(void)pr_err("[ERROR][TSVFB] error in %s: can not support update type [%d]\n",
						     __func__,
						     par->pdata.FbUpdateType);
					ret = -ENODEV;
				}
			}

			if (ret == 0) {
				(void)of_property_read_u32_index(
						info->dev->of_node,
						"telechips,wdma", 1,
						&par->pdata.wdma_info.blk_num);
				par->pdata.wdma_info.virt_addr =
						VIOC_WDMA_GetAddress(
						par->pdata.wdma_info.blk_num);
				par->pdata.vioc_clock = of_clk_get_by_name(
						info->dev->of_node, "ddi-clk");

				/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
				/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
				/* coverity[misra_c_2012_rule_11_9_violation : FALSE] */
				BUG_ON(par->pdata.vioc_clock == NULL);
				par->pdata.FbWdmaPath = 1;
				property_idx = 1U;
			}
		}
	}

	if (ret == 0) {
		if (par->pdata.ddc_info.virt_addr != NULL) {
			switch (par->pdata.ddc_info.blk_num) {
			case VIOC_DISP0:
				par->pdata.ddc_clock =
					of_clk_get_by_name(np, "disp0-clk");
				break;
			case VIOC_DISP1:
				par->pdata.ddc_clock =
					of_clk_get_by_name(np, "disp1-clk");
				break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			case VIOC_DISP2:
				par->pdata.ddc_clock =
					of_clk_get_by_name(np, "disp2-clk");
				break;
#endif
#ifdef CONFIG_ARCH_TCC805X
			case VIOC_DISP3:
				par->pdata.ddc_clock =
					of_clk_get_by_name(np, "disp3-clk");
				break;
#endif
			default:
				(void)pr_err(
					"[ERROR][TSVFB] error in %s: can not get ddc clock\n",
					__func__);
				par->pdata.ddc_clock = NULL;
				ret = -ENODEV;
				break;
			}
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_11_9_violation : FALSE] */
			BUG_ON(par->pdata.ddc_clock == NULL);
		}

#if defined(CONFIG_VIOC_PVRIC_FBDC)
		np = of_parse_phandle(info->dev->of_node, "telechips,fbdc", 0);
		if (np == NULL) {
			(void)pr_err(
				"[ERROR][TSVFB] error in %s: can not find telechips,fbdc\n",
				__func__);
			ret = -ENODEV;
		}
		if (ret == 0) {
			(void)of_property_read_u32_index(
				info->dev->of_node, "telechips,fbdc", property_idx,
				&par->pdata.fbdc_info.blk_num);
			par->pdata.fbdc_info.virt_addr = VIOC_PVRIC_FBDC_GetAddress(
				get_vioc_index(par->pdata.fbdc_info.blk_num));
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			(void)pr_info("%s fbdc %p\n", __func__,
				      par->pdata.fbdc_info.virt_addr);
		}
#endif
		if (ret == 0) {
			np = of_parse_phandle(info->dev->of_node, "telechips,rdma", 0);
			if (np == NULL) {
				(void)pr_err(
					     "[ERROR][TSVFB] error in %s: can not find telechips,rdma\n",
					     __func__);
				ret = -ENODEV;
			}
		}
	}

	if (ret == 0) {
		unsigned int rdma_idx;

		(void)of_property_read_u32_index(
			info->dev->of_node, "telechips,rdma", property_idx,
			&par->pdata.rdma_info.blk_num);
		rdma_idx = get_vioc_index(par->pdata.rdma_info.blk_num);
		par->pdata.rdma_info.virt_addr = VIOC_RDMA_GetAddress(
			rdma_idx);
#if !defined(CONFIG_VIOC_PVRIC_FBDC)
		if (par->pdata.FbUpdateType == FBX_OVERLAY_UPDATE) {
			par->pdata.rdma_info.irq_num = irq_of_parse_and_map(
				np,
				(int32_t)rdma_idx);
		}
#endif
		np = of_parse_phandle(
			info->dev->of_node, "telechips,wmixer", 0);
		if (np == NULL) {
			(void)pr_err(
				"[ERROR][TSVFB] error in %s: can not find telechips,wmixer\n",
				__func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		(void)of_property_read_u32_index(
			info->dev->of_node, "telechips,wmixer", property_idx,
			&par->pdata.wmixer_info.blk_num);
		par->pdata.wmixer_info.virt_addr = VIOC_WMIX_GetAddress(
			get_vioc_index(par->pdata.wmixer_info.blk_num));
		VIOC_WMIX_GetOverlayPriority(
			par->pdata.wmixer_info.virt_addr,
			&par->pdata.FbLayerOrder);
		np = of_parse_phandle(
			info->dev->of_node, "telechips,scaler", 0);
		if (np != NULL) {
			(void)of_property_read_u32_index(
				info->dev->of_node, "telechips,scaler",
				property_idx, &par->pdata.scaler_info.blk_num);
			par->pdata.scaler_info.virt_addr = VIOC_SC_GetAddress(
				get_vioc_index(par->pdata.scaler_info.blk_num));
		} else {
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			(void)pr_info(
				"[INFO][TSVFB] %s: therer is no telechips,scaler\n",
				__func__);
		}
		np = of_get_child_by_name(info->dev->of_node, "fbx_region");
		if (np == NULL) {
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			(void)pr_info(
				"[INFO][TSVFB] %s: there is no fbx_region\n",
				__func__);
		}

		if (np != NULL) {
			if (of_property_read_u32(
				    np, "x", &par->pdata.region.x) < 0) {
				(void)pr_err(
					"[ERROR][TSVFB] error in %s: can nod find 'x' of fbx_region\n",
					__func__);
				ret = -ENODEV;
			}

			if (ret == 0) {
				if (of_property_read_u32(
					np, "y", &par->pdata.region.y) < 0) {
					(void)pr_err(
						     "[ERROR][TSVFB] error in %s: can nod find 'y' of fbx_region\n",
						     __func__);
					ret = -ENODEV;
				}
			}

			if (ret == 0) {
				if (of_property_read_u32(
				np, "width", &par->pdata.region.width) < 0) {
					(void)pr_err("[ERROR][TSVFB] error in %s: can nod find 'width' of fbx_region\n",
						     __func__);
					ret = -ENODEV;
				}
			}

			if (ret == 0) {
				if (of_property_read_u32(
				np, "height", &par->pdata.region.height) < 0) {
					(void)pr_err("[ERROR][TSVFB] error in %s: can nod find 'height' of fbx_region\n",
						     __func__);
					ret = -ENODEV;
				}
			}
		} else {
			par->pdata.region.x = 0U;
			par->pdata.region.y = 0U;
			par->pdata.region.width = info->var.xres;
			par->pdata.region.height = info->var.yres;
		}
	}

	if (ret == 0) {
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		(void)pr_info("[INFO][TSVFB] %s: region x:%d y:%d width:%d height:%d\r\n",
			      __func__,
			      par->pdata.region.x,
			      par->pdata.region.y,
			      par->pdata.region.width,
			      par->pdata.region.height);

		if (par->pdata.vioc_clock != NULL) {
			(void)clk_prepare_enable(par->pdata.vioc_clock);
		}
		if (par->pdata.ddc_clock != NULL) {
			(void)clk_prepare_enable(par->pdata.ddc_clock);
		}

		par->pdata.fp_status = FB_EM_UNBLANK;
	}

	return ret;
}
#endif

static ssize_t
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
fbX_ovp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	struct fb_info *info = platform_get_drvdata(to_platform_device(dev));
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct fbX_par *par = info->par;
	ssize_t ret;

	(void)attr;

	VIOC_WMIX_GetOverlayPriority(
		par->pdata.wmixer_info.virt_addr, &par->pdata.FbLayerOrder);

	/* coverity[misra_c_2012_rule_21_6_violation : FALSE] */
	ret = (ssize_t)snprintf(buf, 4, "%u\n", par->pdata.FbLayerOrder);
	if (ret < 0) {
		(void)pr_err("[ERROR][TSVFB] %s: sprintf failed.\n",
			     __func__);
	}

	return ret;
}

static ssize_t fbX_ovp_store(
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	struct fb_info *info = platform_get_drvdata(to_platform_device(dev));
	struct fbX_par *par = info->par;
	unsigned long value = 0;
	int done = 0;

	(void)attr;

	value = simple_strtoul(buf, NULL, 10);
	if (value > 29UL) {
		(void)pr_err("[ERROR][TSVFB] %s: invalid ovp%ld\n",
			     __func__, value);
		done = 1;
	}

	if (done == 0) {
		if (par->pdata.FbLayerOrder != tcc_safe_u642uint(value)) {
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			(void)pr_info("[INFO][TSVFB] %s: ovp%d -> %ld\n",
				      __func__,
				      par->pdata.FbLayerOrder, value);

			par->pdata.FbLayerOrder = tcc_safe_u642uint(value);
			VIOC_WMIX_SetOverlayPriority(
					par->pdata.wmixer_info.virt_addr,
					par->pdata.FbLayerOrder);
			VIOC_WMIX_SetUpdate(par->pdata.wmixer_info.virt_addr);
		}
	}

	/* coverity[cert_int02_c_violation : FALSE] */
	/* coverity[cert_int31_c_violation : FALSE] */
	return (ssize_t)count;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
static DEVICE_ATTR(ovp, (S_IRUGO | S_IWUSR), fbX_ovp_show, fbX_ovp_store);

static struct attribute *fbX_dev_attrs[] = {
	&dev_attr_ovp.attr,
	NULL,
};

static struct attribute_group fbX_dev_attgrp = {
	.name = NULL,
	.attrs = fbX_dev_attrs,
};

static int __init fbX_probe(struct platform_device *pdev)
{
#if defined(CONFIG_FB_PANEL_LVDS_TCC)
	const struct device_node *panel_node;
#endif
	unsigned int no_kernel_logo = 0;
	struct fb_info *info;
	struct fbX_par *par;
	int retval = 0;
	unsigned int node_num = 0;

	info = framebuffer_alloc(sizeof(struct fbX_par), &pdev->dev);
	if (info == NULL) {
		(void)pr_err(
			"[ERROR][TSVFB] error in %s: can not allocate fb\n",
			__func__);
		retval = -ENOMEM;
	}

	if (retval == 0) {
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		par = info->par;
		info->dev = &pdev->dev;
		platform_set_drvdata(pdev, info);

		#ifdef CONFIG_OF
		(void)fb_dt_parse_data(info);
		#endif
		if (of_property_read_u32(pdev->dev.of_node,
					 "no-kernel-logo", &no_kernel_logo) < 0) {
			(void)pr_warn("[WARN][TSVFB] %s There is no no-kernel-logo property\n",
				__func__);
		}

		#if defined(CONFIG_FB_PANEL_LVDS_TCC)
		panel_node = of_graph_get_remote_node(pdev->dev.of_node, 0U, 0xFFFFFFFFU);
		if (panel_node != NULL) {
			par->panel = of_fb_find_panel(panel_node);
		}
		if ((panel_node != NULL) && (par->panel != NULL)) {
			dev_info(
				 info->dev,
				 "[INFO][TSVFB] %s fb%d has framebuffer panel\r\n",
				 __func__, of_alias_get_id(info->dev->of_node, "fb"));
			if (of_property_read_u32(panel_node,
						 "lcdc-mux-select",
						 &par->pdata.lcdc_mux)
			    == 0) {
				dev_info(
					 info->dev,
					 "[INFO][TSVFB] %s lcdc-mux-select=%d from remote node\r\n",
					 __func__, par->pdata.lcdc_mux);
			} else {
				dev_err(info->dev,
					"[ERROR][TSVFB] %s can not found lcdc-mux-select from remote node\r\n",
					__func__);
				retval = -ENODEV;
				framebuffer_release(info);
			}
		}
		#endif
	}

	if (retval == 0) {
		/* coverity[misra_c_2012_rule_21_6_violation : FALSE] */
		(void)snprintf(fbX_fix.id, sizeof(fbX_fix.id), "telechips,fb%d",
			 of_alias_get_id(info->dev->of_node, "fb"));
		info->fix = fbX_fix;

		info->var.nonstd = 0;
		info->var.activate = (par->pdata.FbUpdateType ==
			FBX_NOWAIT_UPDATE) ?
			(u32)FB_ACTIVATE_NOW :
			(u32)FB_ACTIVATE_VBL;
		info->var.accel_flags = 0;
		info->var.vmode = FB_VMODE_NONINTERLACED;

		info->fbops = &tsvfb_fb_ops;
		info->flags = FBINFO_DEFAULT;

		info->pseudo_palette = devm_kzalloc(&pdev->dev,
					sizeof(unsigned int) * 16U, GFP_KERNEL);
		if (info->pseudo_palette == NULL) {
			(void)pr_err(
				     "[ERROR][TSVFB] error in %s: can not allocate pseudo_palette\n",
				     __func__);
			retval = -ENOMEM;
			framebuffer_release(info);
		}
	}

	if (retval == 0) {
		retval = fb_map_video_memory(info);
		if (retval < 0) {
			(void)pr_err(
				     "[ERROR][TSVFB] error in %s: can not remap framebuffer\n",
				     __func__);
			retval = -ENOMEM;
			framebuffer_release(info);
		}
	}

	if (retval == 0) {
		(void)fbX_check_var(&info->var, info);
		(void)fbX_set_par(info);

		if (register_framebuffer(info) < 0) {
			(void)pr_err(
				     "[ERROR][TSVFB] error in %s: can not register framebuffer device\n",
				     __func__);
			retval = -EINVAL;
			devm_kfree(&pdev->dev, info->pseudo_palette);
			framebuffer_release(info);
		}
	}

	if (retval == 0) {
		if (no_kernel_logo != 0U) {
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			(void)pr_info("[INFO][TSVFB] SKIP fb_show_logo\n");
		} else {
			if (fb_prepare_logo(info, FB_ROTATE_UR) != 0) {
				/* Start display and show logo on boot */
				/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
				(void)pr_info("[INFO][TSVFB] fb_show_logo\n");
				// So, we use fb_alloc_cmap_gfp
				// function(fb_default_camp(default_16_colors))
				/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
				(void)fb_alloc_cmap_gfp(&info->cmap, 16, 0, GFP_KERNEL);
				/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
				(void)fb_alloc_cmap_gfp(&info->cmap, 16, 0, GFP_KERNEL);
				(void)fb_show_logo(info, FB_ROTATE_UR);
			}
		}

		if (info->node >= 0) {
			node_num = (u32)info->node;
		} else {
			retval = -EINVAL;
			(void)pr_err("[ERROR][TSVFB] error in %s: node number is invalid\n",
				     __func__);
		}
	}

	if (retval == 0) {
		atomic_set(&fb_waitq[node_num].state, 0);
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		init_waitqueue_head(&fb_waitq[node_num].waitq);
		if (par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE) {
			(void)fb_register_isr(info);
		}

		(void)fb_info(info, "%s frame buffer device v%s\n", info->fix.id, FB_VERSION);
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		(void)pr_info("%s : update_type = %d ", __func__, par->pdata.FbUpdateType);
		if (sysfs_create_group(&pdev->dev.kobj, &fbX_dev_attgrp) != 0) {
			(void)fb_warn(info, "failed to register attributes\n");
		}

#if !defined(CONFIG_TCC803X_CA7S) && !defined(CONFIG_TCC805X_CA53Q)
		retval = fbX_activate_var(
				 (par->map_dma
				  + ((u64)info->var.xres * info->var.yoffset
				     * (info->var.bits_per_pixel / 8U))),
				 &info->var, info);
#endif
		(void)fbX_set_par(info);

		if ((par->pdata.FbWdmaPath == 1U)
		    && (par->pdata.wdma_info.virt_addr
		    != NULL)) { // vin path RDMA(such as DPGL)
			    fbX_prepare_vin_path_rdma(par);
		    }
	}
	return retval;
}

/*
 *  Cleanup
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int fbX_remove(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	struct fb_info *info = platform_get_drvdata(pdev);
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct fbX_par *par = info->par;
	int retval = 0;

	if (par != NULL) {
		switch (par->pdata.FbUpdateType) {
		case FBX_RDMA_UPDATE:
			fb_unregister_isr(info);
			break;
		case FBX_OVERLAY_UPDATE:
			fb_unregister_isr(info);
			break;
		case FBX_NOWAIT_UPDATE:
			break;
		default:
			(void)pr_err("[ERROR][TSVFB] %s: fbUpdateType not valid[%d] \n",
			       __func__, par->pdata.FbUpdateType);
			retval = -EINVAL;
			break;
		}

		atomic_set(&fb_waitq[tcc_safe_int2uint(info->node)].state, 0);

		fb_unmap_video_memory(info);
		if (par->pdata.vioc_clock != NULL) {
			clk_put(par->pdata.vioc_clock);
		}
		if (par->pdata.ddc_clock != NULL) {
			clk_put(par->pdata.ddc_clock);
		}

		unregister_framebuffer(info);
		framebuffer_release(info);
	}
	return retval;
}

#if defined(CONFIG_FB_PANEL_LVDS_TCC)
static int
fbx_set_display_controller(const struct fb_info *info, struct videomode *vm)
{
	stLTIMING stTimingParam;
	stLCDCTR stCtrlParam;
	bool interlace;
	u32 tmp_sync;
	u32 vactive;
	const struct fbX_par *par;
	int ret = 0;
	int layer;

	u32 channel;
	unsigned int ddc_idx;

	if (info == NULL) {
		ret = -EINVAL;
	}

	if (ret == 0) {
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		par = info->par;
		if (par == NULL) {
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		int image_num;
		/* Display MUX */
		ddc_idx = get_vioc_index(par->pdata.ddc_info.blk_num);

		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		(void)VIOC_CONFIG_LCDPath_Select((int)ddc_idx,
					   par->pdata.lcdc_mux);
		dev_info(
			 info->dev,
			 "[INFO][TSVFB] %s display device(%d) to connect mux(%d)\r\n",
			 __func__, get_vioc_index(par->pdata.ddc_info.blk_num),
			 par->pdata.lcdc_mux);

		(void)memset(&stCtrlParam, 0, sizeof(stLCDCTR));
		(void)memset(&stTimingParam, 0, sizeof(stLTIMING));
		if(((u32)vm->flags &
		    (u32)DISPLAY_FLAGS_INTERLACED) != 0U) {
			interlace = (bool)true;
		} else {
			interlace = (bool)false;
		}
		stCtrlParam.iv = (((u32)vm->flags &
				(u32)DISPLAY_FLAGS_VSYNC_LOW) != 0U) ?
				1U : 0U;
		stCtrlParam.ih = (((u32)vm->flags &
				(u32)DISPLAY_FLAGS_HSYNC_LOW) != 0U) ?
				1U : 0U;
		stCtrlParam.dp = (((u32)vm->flags &
				(u32)DISPLAY_FLAGS_DOUBLECLK) != 0U) ?
				1U : 0U;

		if (interlace) {
			stCtrlParam.tv = 1U;
			stCtrlParam.advi = 1U;
			vm->vactive = vm->vactive << 1U;
		} else {
			stCtrlParam.ni = 1U;
		}

		/* Check vactive */
		vactive = vm->vactive;

		dev_info(
			 info->dev, "[INFO][TSVFB] %s %dx%d panel\r\n", __func__,
			 vm->hactive, vactive);

		stTimingParam.lpc = vm->hactive;
		stTimingParam.lewc = vm->hfront_porch;
		stTimingParam.lpw = (vm->hsync_len > 0U) ?
					(vm->hsync_len - 1U) : 0U;
		stTimingParam.lswc = vm->hback_porch;

		if (interlace) {
			tmp_sync = vm->vsync_len << 1U;
			stTimingParam.fpw = (tmp_sync > 0U) ?
						(tmp_sync - 1U) : 0U;
			tmp_sync = vm->vback_porch << 1U;
			stTimingParam.fswc = (tmp_sync > 0U) ?
						(tmp_sync - 1U) : 0U;
			stTimingParam.fewc = vm->vfront_porch << 1U;
			stTimingParam.fswc2 = stTimingParam.fswc + 1U;
			stTimingParam.fewc2 = (stTimingParam.fewc > 0U) ?
						(stTimingParam.fewc - 1U) : 0U;
		} else {
			stTimingParam.fpw = (vm->vsync_len > 0U) ?
						(vm->vsync_len - 1U) : 0U;
			stTimingParam.fswc = (vm->vback_porch > 0U) ?
						(vm->vback_porch - 1U) : 0U;
			stTimingParam.fewc = (vm->vfront_porch > 0U) ?
						(vm->vfront_porch - 1U) : 0U;
			stTimingParam.fswc2 = stTimingParam.fswc;
			stTimingParam.fewc2 = stTimingParam.fewc;
		}

		/* Common Timing Parameters */
		stTimingParam.flc = (vactive > 0U) ? (vactive - 1U) : 0U;
		stTimingParam.fpw2 = stTimingParam.fpw;
		stTimingParam.flc2 = stTimingParam.flc;

		/* swreset display device */
		VIOC_CONFIG_SWReset(par->pdata.ddc_info.blk_num,
					VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(par->pdata.ddc_info.blk_num,
					VIOC_CONFIG_CLEAR);

		VIOC_CONFIG_SWReset(par->pdata.wmixer_info.blk_num,
					VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(par->pdata.wmixer_info.blk_num,
					VIOC_CONFIG_CLEAR);

		// vioc_reset_rdma_on_display_path(par->PDATA.DispNum);
		channel = get_vioc_index(par->pdata.rdma_info.blk_num)
			- (4U * get_vioc_index(par->pdata.ddc_info.blk_num));
		VIOC_WMIX_SetPosition(par->pdata.wmixer_info.virt_addr,
				      channel, par->pdata.region.x,
				      par->pdata.region.y);

		image_num = VIOC_RDMA_GetImageNum(
					par->pdata.rdma_info.blk_num);
		layer = VIOC_WMIX_GetLayer(par->pdata.wmixer_info.blk_num,
					   tcc_safe_int2uint(image_num));
		switch (layer) {
		case 3:
			channel = 0U;
			break;
		case 2:
			channel = 1U;
			break;
		case 1:
			if (VIOC_WMIX_GetMixerType(
				par->pdata.wmixer_info.blk_num) != 0) {
				/* 4 to 2 mixer */
				channel = 2U;
			} else {
				/* 2 to 2 mixer */
				channel = 0U;
			}
			break;
		default:
			(void)pr_err("[ERROR][TSVFB] %s: layer[%d] is not valid\n",
			       __func__, layer);
			break;
		}
		if (info->var.bits_per_pixel == VIOC_IMG_FMT_RGB565) {
			VIOC_WMIX_SetChromaKey(
				par->pdata.wmixer_info.virt_addr, channel,
				1 /*ON*/, par->pdata.chroma_info.chroma_key[0],
				par->pdata.chroma_info.chroma_key[1],
				par->pdata.chroma_info.chroma_key[2],
				par->pdata.chroma_info.chroma_mkey[0],
				par->pdata.chroma_info.chroma_mkey[1],
				par->pdata.chroma_info.chroma_mkey[2]);
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			(void)pr_info("[INF][FBX]RGB565 Set Chromakey\n"
				      "key R : 0x%x\nkey G : 0x%x\nkey B : 0x%x\n"
				      "mkey R : 0x%x\nmkey G : 0x%x\nmkey B : 0x%x\n",
				      par->pdata.chroma_info.chroma_key[0],
				      par->pdata.chroma_info.chroma_key[1],
				      par->pdata.chroma_info.chroma_key[2],
				      par->pdata.chroma_info.chroma_mkey[0],
				      par->pdata.chroma_info.chroma_mkey[1],
				      par->pdata.chroma_info.chroma_mkey[2]);
		} else { // VIOC_IMG_FMT_ARGB8888
			VIOC_WMIX_SetChromaKey(
					       par->pdata.wmixer_info.virt_addr,
					       channel, 0 /*OFF*/,
					       0, 0, 0, 0, 0, 0);
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			(void)pr_info("[INF][FBX]ARGB888 Disable Chromakey\n");
		}

		VIOC_WMIX_SetOverlayPriority(
					     par->pdata.wmixer_info.virt_addr, par->pdata.FbLayerOrder);
		VIOC_WMIX_SetSize(
				  par->pdata.wmixer_info.virt_addr, vm->hactive, vactive);

		VIOC_WMIX_SetUpdate(par->pdata.wmixer_info.virt_addr);

		VIOC_DISP_SetTimingParam(par->pdata.ddc_info.virt_addr, &stTimingParam);
		VIOC_DISP_SetControlConfigure(
					      par->pdata.ddc_info.virt_addr, &stCtrlParam);

		/* PXDW
		 * YCC420 with stb pxdw is 27
		 * YCC422 with stb is pxdw 21, with out stb is 8
		 * YCC444 && RGB with stb is 23, with out stb is 12
		 * TSVFB can only support RGB as format of the display device.
		 */
		VIOC_DISP_SetPXDW(par->pdata.ddc_info.virt_addr, 12);

		VIOC_DISP_SetSize(par->pdata.ddc_info.virt_addr, vm->hactive, vactive);
		VIOC_DISP_SetBGColor(par->pdata.ddc_info.virt_addr, 0, 0, 0, 0);
	}

	return ret;
}
#endif

static int fbx_turn_on_resource(struct fb_info *info)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct fbX_par *par = info->par;
	int done = 0;
#if defined(CONFIG_FB_PANEL_LVDS_TCC)
	int skip_step = 0;
	struct videomode vm;
#endif

	if (par->pdata.fp_status == FB_EM_UNBLANK) {
		done = 1;
	}

	if (done == 0) {
#if defined(CONFIG_FB_PANEL_LVDS_TCC)
		if (par->pdata.fp_status == FB_EM_POWERDOWN) {
			if (par->panel != NULL) {
				if (fb_panel_get_mode(par->panel, &vm) < 0) {
					skip_step = 1;
				}

				if (skip_step == 0) {
					(void)fbx_set_display_controller(info, &vm);
					if (par->pdata.ddc_clock != NULL) {
						(void)clk_set_rate(par->pdata.ddc_clock,
							     vm.pixelclock);
						dev_info(info->dev,
						 "%s set ddc clock to %ldHz\r\n",
						 __func__,
						 clk_get_rate(par->pdata.ddc_clock));
					}
				}
			}
		}
#endif

		switch (par->pdata.fp_status) {
		case FB_EM_POWERDOWN:
#if defined(CONFIG_FB_PANEL_LVDS_TCC)
			if (par->panel != NULL) {
				if (par->pdata.ddc_clock != NULL) {
					(void)clk_prepare_enable(par->pdata.ddc_clock);
				}
			}
#endif

			if (par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE) {
				(void)fb_register_isr(info);
			}
			break;

		case FB_EM_BLANK:
			if (par->pdata.FbUpdateType == FBX_OVERLAY_UPDATE) {
				(void)fb_register_isr(info);
			}
			break;
		default:
			(void)pr_err("[ERROR][TSVFB] %s: unknown fp_status [%d].\n",
				__func__, (int)par->pdata.fp_status);
			break;
		}

		par->pdata.fp_status = FB_EM_UNBLANK;
		(void)fbX_pan_display(&info->var, info);
	}

	return 0;
}

static int fbx_turn_off_resource_internal(
	struct fb_info *info, enum fb_power_status fp_status)
{
	int done = 0;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct fbX_par *par = info->par;
	int scnum = -1;

	if (fp_status == par->pdata.fp_status) {
		done = 1;
	}

	if (done == 0) {
		if (fp_status >= par->pdata.fp_status) {
			done = 1;
		}
	}

	if (done == 0) {
		switch (fp_status) {
		case FB_EM_POWERDOWN:
			if (par->pdata.FbUpdateType != FBX_NOWAIT_UPDATE) {
				if ((par->pdata.fp_status == FB_EM_BLANK)
				    && (par->pdata.FbUpdateType ==
							FBX_OVERLAY_UPDATE)) {
					dev_info(info->dev,
						 "[DEBUG] This isr is already unregistered");
				} else {
					fb_unregister_isr(info);
				}
			}
			break;
		case FB_EM_BLANK:
			if (par->pdata.FbUpdateType == FBX_OVERLAY_UPDATE) {
				fb_unregister_isr(info);
			}
			break;
		default:
			done = 1;
			break;
		}
	}

	if (done == 0) {
		/* Disable RDMA */
		if (par->pdata.ddc_info.virt_addr != NULL) {
			if (VIOC_DISP_Get_TurnOnOff(
					par->pdata.ddc_info.virt_addr) != 0U) {
				VIOC_RDMA_SetImageDisable(
					par->pdata.rdma_info.virt_addr);
			} else {
				VIOC_RDMA_SetImageDisableNW(
					par->pdata.rdma_info.virt_addr);
			}
		} else {
			VIOC_RDMA_SetImageDisableNW(par->pdata.rdma_info.virt_addr);
			/* Wait for rdma to be disabled. */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */ //
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */ //
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			mdelay(10);
		}

#if defined(CONFIG_FB_PANEL_LVDS_TCC)
		if (fp_status == FB_EM_POWERDOWN) {
			if (par->panel != NULL) {
				/* Turn off display controller */
				if (VIOC_DISP_Get_TurnOnOff(
					par->pdata.ddc_info.virt_addr) != 0U) {
					VIOC_DISP_TurnOff(
					par->pdata.ddc_info.virt_addr);
					(void)VIOC_DISP_sleep_DisplayDone(
						par->pdata.ddc_info.virt_addr);
				}
				VIOC_CONFIG_SWReset(
				par->pdata.ddc_info.blk_num, VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(
				par->pdata.ddc_info.blk_num, VIOC_CONFIG_CLEAR);
			}
		}
#endif

		if (par->pdata.scaler_info.virt_addr != NULL) {
			scnum = VIOC_CONFIG_GetScaler_PluginToRDMA(
						par->pdata.rdma_info.blk_num);
		}
		if (scnum > 0) {
			(void)VIOC_CONFIG_PlugOut((unsigned int)scnum);
			VIOC_CONFIG_SWReset((unsigned int)scnum,
						VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset((unsigned int)scnum,
						VIOC_CONFIG_CLEAR);
			dev_info(info->dev,
				 "[INFO][TSVFB] reset rdma_%d with Scaler-%d\n",
				 get_vioc_index(par->pdata.rdma_info.blk_num),
				 get_vioc_index((unsigned int)scnum));
		}

		VIOC_CONFIG_SWReset(par->pdata.rdma_info.blk_num,
					VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(par->pdata.rdma_info.blk_num,
					VIOC_CONFIG_CLEAR);
		dev_info(info->dev, "[INFO][TSVFB] reset rdma_%d\n",
			 get_vioc_index(par->pdata.rdma_info.blk_num));

#if defined(CONFIG_FB_PANEL_LVDS_TCC)
		if ((par->panel != NULL) &&
		    (fp_status == FB_EM_POWERDOWN)) {
			if (par->pdata.ddc_clock != NULL) {
				clk_disable_unprepare(par->pdata.ddc_clock);
			}
		}
#endif
		par->pdata.fp_status = fp_status;
	}
	return 0;
}

static int fbx_turn_off_resource(struct fb_info *info)
{
	return fbx_turn_off_resource_internal(info, FB_EM_POWERDOWN);
}
static int fbx_blank_resource(struct fb_info *info)
{
	return fbx_turn_off_resource_internal(info, FB_EM_BLANK);
}

/* below check routine is added temporarily */
static int fbX_check_clock_ready(void)
{
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_13_1_violation : FALSE] */
	const void __iomem* p_ddi_cfg = ioremap_nocache(0x12380000, 0x4); //DDI_CFG CLKEN
	int cnt;
	unsigned int value;
	int ret = -1;

	for (cnt = 0 ; cnt <= 1000 ; cnt++) {
		value = __raw_readl(p_ddi_cfg);
		if ((value & 0x1U) == 0x1U) {
			ret = cnt;
			break;
		}
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		mdelay(1);
	}
	return ret;
}

#ifdef CONFIG_PM
/**
 *	fb_suspend - Optional but recommended function. Suspend the device.
 *	@dev: platform device
 *	@msg: the suspend event code.
 *
 *      See Documentation/power/devices.txt for more information
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int fbX_suspend(struct platform_device *dev, pm_message_t msg)
{
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	struct fb_info *info = platform_get_drvdata(dev);
	struct fbX_par *par = info->par;
	int do_suspend = 1;

	if (msg.event == par->pm_state) {
		do_suspend = 0; //don't suspend
	}

	if (do_suspend == 1) {
		console_lock();
		fb_set_suspend(info, 1);
#if defined(CONFIG_FB_PANEL_LVDS_TCC)
		if (par->panel != NULL) {
			(void)fb_panel_disable(par->panel);
			(void)fb_panel_unprepare(par->panel);
		}
#endif
		(void)fbx_turn_off_resource(info);
		par->pm_state = msg.event;
		console_unlock();
	}
	return 0;
}

/**
 *	fb_resume - Optional but recommended function. Resume the device.
 *	@dev: platform device
 *
 *      See Documentation/power/devices.txt for more information
 */

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int fbX_resume(struct platform_device *dev)
{
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	struct fb_info *info = platform_get_drvdata(dev);
	struct fbX_par *par = info->par;
	int cnt = 0;
	int ret = 0;

	console_lock();
	/* below check routine is added temporarily */
	cnt = fbX_check_clock_ready();
	if (cnt < 0) {
		(void)pr_err("[ERROR][TSVFB] %s: ddi clock is not enabled. resume failed\n",
		       __func__);
		ret = -EIO;
	} else {
		(void)pr_info("[INFO][TSVFB] %s: ddi clock is enabled. cnt[%d]\n",
			__func__, cnt);
	}

	if (ret == 0) {
		#if defined(CONFIG_FB_PANEL_LVDS_TCC)
		if (par->panel != NULL) {
			(void)fb_panel_prepare(par->panel);
		}
		#endif
		(void)fbx_turn_on_resource(info);
		fb_set_suspend(info, 0);

		#if defined(CONFIG_FB_PANEL_LVDS_TCC)
		if (par->panel != NULL) {
			(void)fb_panel_enable(par->panel);
		}
		#endif

		par->pm_state = PM_EVENT_ON;
		console_unlock();
		/* resume here */
	}
	return ret;
}
#else
#define fbX_suspend NULL
#define fbX_resume NULL
#endif /* CONFIG_PM */

/* the below code is the solution of FIFO underrun issue after corereset */
static void fbX_prepare_vin_path_rdma(const struct fbX_par *par)
{
	unsigned int rdma_enable;
	#if defined(CONFIG_FB_CHECK_RDMA_CADDRESS)
	unsigned int prev = 0;
	unsigned int cur = 0;
	int idx;
	int status = 0;
	#endif

	VIOC_RDMA_GetImageEnable(par->pdata.rdma_info.virt_addr, &rdma_enable);
	#if defined(CONFIG_FB_CHECK_RDMA_CADDRESS)
	cur = VIOC_RDMA_Get_CAddress(par->pdata.rdma_info.virt_addr);
	for (idx = 0; idx < 10; idx++) {
		prev = cur;
		msleep(20);
		cur = VIOC_RDMA_Get_CAddress(par->pdata.rdma_info.virt_addr);
		if (prev != cur)
			status++;
	}
	#endif

	if (rdma_enable != 0U) {
		VIOC_RDMA_SetImageDisable(par->pdata.rdma_info.virt_addr);
	}

	#if defined(CONFIG_TSVFB_DUMP_VIN_RDMA)
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	(void)pr_info(
		"[INFO][TSVFB] rdma_enable = [%s], status = [%s]\n",
		(rdma_enable != 0U) ? "enable" : "disable",
		(status != 0U) ? "working" : "not working");
	#endif
	VIOC_CONFIG_SWReset(
		par->pdata.rdma_info.blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(
		par->pdata.rdma_info.blk_num, VIOC_CONFIG_CLEAR);
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	(void)pr_info("[INFO][TSVFB] rdma[%u] SWReset.\n",
		(par->pdata.rdma_info.blk_num >= VIOC_RDMA) ?
		(par->pdata.rdma_info.blk_num - VIOC_RDMA) : 99U);
}

static struct platform_driver fbX_driver = {
	.probe = fbX_probe,
	.remove = fbX_remove,
	.suspend = fbX_suspend, /* optional but recommended */
	.resume = fbX_resume,   /* optional but recommended */
	.driver = {
		.name = "fb",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(fbX_of_match),
#endif
	},
};

static int __init fbX_init(void)
{
	int ret;
	ret = platform_driver_register(&fbX_driver);
	return ret;
}

static void __exit fbX_exit(void)
{
	platform_driver_unregister(&fbX_driver);
}

/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */ //
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */ //
module_init(fbX_init);
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */ //
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */ //
module_exit(fbX_exit);

/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */ //
MODULE_AUTHOR("inbest <inbest@telechips.com>");
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */ //
MODULE_DESCRIPTION("telechips framebuffer driver");
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */ //
MODULE_LICENSE("GPL");

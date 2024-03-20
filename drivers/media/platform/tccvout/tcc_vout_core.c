// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips, Inc.
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
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include "tcc_vout.h"
#include "tcc_vout_core.h"
#include "tcc_vout_dbg.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#endif

#include <linux/of_reserved_mem.h>

#ifdef CONFIG_TCC_VIOCMG
#include <video/tcc/viocmg.h>
#endif

#ifdef CONFIG_VOUT_DISPLAY_LASTFRAME
static struct pmap lastframe_pbuf;
static int enable_LastFrame;
static WMIXER_ALPHASCALERING_INFO_TYPE fbmixer;
#endif

//#define MAX_VIQE_FRAMEBUFFER

#ifdef CONFIG_VIOC_MAP_DECOMP
#include <video/tcc/tca_map_converter.h>
#endif

#ifdef CONFIG_VIOC_DTRC_DECOMP
#include <video/tcc/tca_dtrc_converter.h>
#endif

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
#include <video/tcc/tccfb.h>

#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
#define RDMA_UVI_MAX_WIDTH             3072
#endif

#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/vioc_dv_cfg.h>
#define DEF_DV_CHECK_NUM 1 //2
static int bStep_Check = DEF_DV_CHECK_NUM;
static unsigned int nFrame;
static unsigned int nFrame_t0;
static unsigned int nFrame_t1;
static unsigned int nFrame_t2;
#define dvprintk(msg...) //pr_info("[WAR][VOUT-DV] " msg);
#endif

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
DRM_Packet_t gDRM_packet;
#endif

static signed int pmap_get_info(const char *name, struct pmap *mem)
{
	struct device_node *np;
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct reserved_mem *rmem;
	int ret = 1;

	char buf[32];

	ret = scnprintf(buf, sizeof(buf), "pmap,%s", name);

	np = of_find_compatible_node(NULL, NULL, buf);
	if (np != NULL) {
		rmem = of_reserved_mem_lookup(np);
		if (rmem == NULL) {
			(void)pr_err("[ERR][VOUT] %s: %s allocation is failed\n",
				__func__, name);
			ret = -1;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto get_pmap_exit;
		}

		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		if (of_property_read_string(np, "pmap-name", &name)) {
			(void)pr_err("error\n");
			ret = -1;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto get_pmap_exit;
		}

		mem->base = rmem->base;
		mem->size = rmem->size;
		dprintk("%s - base=%4llx, size=%4llx\n", rmem->name, mem->base, mem->size);
	}
get_pmap_exit:
	return ret;
}

// buffer
static int tcc_get_base_address(
		unsigned int viocfmt, unsigned int base0_addr,
		unsigned int width, unsigned int height,
		unsigned int pos_x, unsigned int pos_y,
		unsigned int *base0, unsigned int *base1, unsigned int *base2)
{
	unsigned int u_addr, v_addr, y_offset, uv_offset;

	//dprintk("src: 0x%08x, 0x%08x, 0x%08x\n", *base0, *base1, *base2);
	//dprintk("fmt(%d) %d x %d ~ %d x %d\n", viocfmt, pos_x, pos_y, width, height);

	/*
	 * Calculation of base0 address
	 */
	y_offset = (width * pos_y) + pos_x;

	if ((viocfmt >= VIOC_IMG_FMT_RGB332) && (viocfmt <= VIOC_IMG_FMT_ARGB6666_3)) {
		unsigned int bpp;

		if (viocfmt == VIOC_IMG_FMT_RGB332) {
			bpp = 1U;
		} else if (viocfmt <= VIOC_IMG_FMT_ARGB1555) {
			bpp = 2U;
		} else {
			bpp = 4U;
		}

		*base0 = base0_addr + (y_offset * bpp);
	}

	if ((viocfmt == VIOC_IMG_FMT_UYVY)
		|| (viocfmt == VIOC_IMG_FMT_VYUY)
		|| (viocfmt == VIOC_IMG_FMT_YUYV)
		|| (viocfmt == VIOC_IMG_FMT_YVYU)) {
		y_offset = y_offset * 2U;
	}

	*base0 = base0_addr + y_offset; // Set base0 address for image

	/*
	 * Calculation of base1 and base2 address
	 */

	if ((*base1 == 0U) && (*base2 == 0U)) {
		u_addr = GET_ADDR_YUV42X_spU(base0_addr, width, height);
		if (viocfmt == VIOC_IMG_FMT_YUV420SEP) {
			v_addr = GET_ADDR_YUV420_spV(u_addr, width, height);
		} else {
			v_addr = GET_ADDR_YUV422_spV(u_addr, width, height);
		}
	} else {
		u_addr = *base1;
		v_addr = *base2;
	}

	if ((viocfmt == VIOC_IMG_FMT_YUV420SEP)
		|| (viocfmt == VIOC_IMG_FMT_YUV420IL0)
		|| (viocfmt == VIOC_IMG_FMT_YUV420IL1)) {
		if (viocfmt == VIOC_IMG_FMT_YUV420SEP) {
			uv_offset = ((width * pos_y) / 4U) + (pos_x / 2U);
		} else {
			uv_offset = ((width * pos_y) / 2U) + (pos_x);
		}
	} else {
		if (viocfmt == VIOC_IMG_FMT_YUV422IL1) {
			uv_offset = (width * pos_y) + (pos_x);
		} else {
			uv_offset = ((width * pos_y) / 2U) + (pos_x / 2U);
		}
	}

	*base1 = u_addr + uv_offset;		// Set base1 address for image
	*base2 = v_addr + uv_offset;		// Set base2 address for image

	return 0;
}

static int tcc_get_base_address_of_image(
		unsigned int pixelformat, unsigned int base0_addr,
		unsigned int width, unsigned int height,
		unsigned int *base0, unsigned int *base1, unsigned int *base2)
{
	unsigned int y_offset = 0, uv_offset = 0;

	/* gstv4l2object.c - gst_v4l2_object_get_caps_info()
	 */

	check_wrap(width);
	check_wrap(height);

	switch (pixelformat) {
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		y_offset = ROUND_UP_4(width) * ROUND_UP_2(height);
		uv_offset = (ROUND_UP_4(width) / 2U) * (ROUND_UP_2(height) / 2U);
		break;
	case V4L2_PIX_FMT_Y41P:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_YVYU:
		y_offset = (ROUND_UP_2(width) * 2U) * height;
		break;
	case V4L2_PIX_FMT_YUV411P:
		y_offset = ROUND_UP_4(width) * height;
		uv_offset = (ROUND_UP_4(width) / 4U) * height;
		break;
	case V4L2_PIX_FMT_YUV422P:
		y_offset = ROUND_UP_4(width) * height;
		uv_offset = (ROUND_UP_4(width) / 2U) * height;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		y_offset = ROUND_UP_4(width) * ROUND_UP_2(height);
		uv_offset = (ROUND_UP_4(width) * height) / 2U;
		break;
	default:
		/* prevent KCS */
		break;
	}

	check_wrap(y_offset);
	check_wrap(uv_offset);

	*base0 = base0_addr;
	if((UINT_MAX - y_offset) > *base0) {
		*base1 = *base0 + y_offset;
	}
	if((UINT_MAX - uv_offset) > *base0) {
		*base2 = *base1 + uv_offset;
	}

	return 0;
}

static void vout_check_format(struct vioc_rdma *rdma, unsigned int fmt)
{
	int pixelformat = -1;

	switch (fmt) {
	case (unsigned int)DEC_FMT_420IL0:
		pixelformat = (int)VIOC_IMG_FMT_YUV420IL0;
		break;
	case (unsigned int)DEC_FMT_422SEP:
		pixelformat = (int)VIOC_IMG_FMT_YUV422SEP;
		break;
	case (unsigned int)DEC_FMT_420SEP:
		pixelformat = (int)VIOC_IMG_FMT_YUV420SEP;
		break;
	case (unsigned int)DEC_FMT_444SEP:
		pixelformat = (int)VIOC_IMG_FMT_444SEP;
		break;
	case (unsigned int)DEC_FMT_422V:
	case (unsigned int)DEC_FMT_400:
	default:
		(void)pr_err("[ERR][VOUT] unknown format(%d)\n", fmt);
		break;
	}

	if ((pixelformat > 0) && (rdma->fmt != (unsigned int)pixelformat)) {
		dprintk("DEC_FMT 0x%x -> 0x%x\n", rdma->fmt, pixelformat);
		rdma->fmt = (unsigned int)pixelformat;
		VIOC_RDMA_SetImageFormat(rdma->addr, rdma->fmt);
		VIOC_RDMA_SetImageOffset(rdma->addr, rdma->fmt, rdma->width);
	}
}

void vout_pop_all_buffer(struct tcc_vout_device *vout)
{
	vout->clearFrameMode = 1;	// enable clear frame mode

	atomic_add(atomic_read(&vout->readable_buff_count), &vout->displayed_buff_count);
	atomic_set(&vout->readable_buff_count, 0);

	if (atomic_read(&vout->displayed_buff_count) == 0) {
		vout->clearFrameMode = 0;	// disable clear frame mode
	}

	// for interlace
	vout->firstFieldFlag = 0;
	vout->frame_count = 0;

	dprintk("\n");
}

#ifdef CONFIG_VOUT_USE_VSYNC_INT
static struct v4l2_buffer *vout_get_buffer(struct tcc_vout_device *vout)
{
	return (struct v4l2_buffer *)(&vout->qbufs[vout->popIdx].buf);
}

static void vout_pop_buffer(struct tcc_vout_device *vout)
{
	if (++vout->popIdx >= vout->nr_qbufs)
		vout->popIdx = 0;

	atomic_dec(&vout->readable_buff_count);
	if (atomic_read(&vout->readable_buff_count) < 0)
		atomic_set(&vout->readable_buff_count, 0);
//	pr_debug("[DBG][VOUT] %s: popIdx: %d readable_buff_count: %d\n", __func__,vout->popIdx, atomic_read(&vout->readable_buff_count));
}

static void vout_clear_buffer(struct tcc_vout_device *vout, struct v4l2_buffer *buf)
{
	/* update last displayed buffer index */
	vout->last_displayed_buf_idx = buf->index;
	atomic_inc(&vout->displayed_buff_count);
}
#endif

/* coverity[misra_c_2012_rule_5_7_violation : FALSE] */
int vout_get_pmap(struct pmap *pmap)
{
	int ret = 0;
	if (pmap_get_info(pmap->name, pmap) > 0) {
		dprintk("[PMAP] %s: 0x%08llx ~ 0x%08llx (0x%08llx)\n",
		pmap->name, pmap->base, pmap->base + pmap->size, pmap->size);
		ret = 0;
	} else {
		ret = -1;
	}
	return ret;
}

int vout_set_m2m_path(int deintl_default, struct tcc_vout_device *vout)
{
	struct device_node *dev_np;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int deinterlacer = 0;
	int ret = 0;

	/* get pmap for deintl_bufs */
	(void)memcpy(&vout->deintl_pmap.name, kasprintf(GFP_KERNEL, "v4l2_vout%d", vout->id), sizeof(VOUT_MEM_PATH_PMAP_NAME));
	if (vout_get_pmap(&vout->deintl_pmap) != 0) {
		(void)pr_err("[ERR][VOUT] vout_get_pmap(%s)\n", vout->deintl_pmap.name);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_exit;
	}

	if (deintl_default != 0) {
		/* default 4 buffers */
		#if defined(CONFIG_TCC_DUAL_DISPLAY)
		vout->deintl_nr_bufs = 4;
		vout->m2m_dual_nr_bufs = 4;
		#else
		vout->deintl_nr_bufs = 4;
		#endif

		/* bottom-field first */
		vioc->m2m_rdma.bf = 1;

		// set m2m rdma
		dev_np = of_parse_phandle(vout->v4l2_np, "m2m_rdma", 0);
		if (dev_np != NULL) {
			/* swreser bit */
			ret = of_property_read_u32_index(vout->v4l2_np, "m2m_rdma", 1, &vioc->m2m_rdma.id);
			vioc->m2m_rdma.addr = VIOC_RDMA_GetAddress(get_vioc_index(vioc->m2m_rdma.id));
			dprintk("[M2M] RDMA < vir_addr = 0x%p , id = %d ,\n", vioc->m2m_rdma.addr, get_vioc_index(vioc->m2m_rdma.id));
		} else {
			dprintk("[M2M] could not find rdma node of vout driver.\n");
		}

		// set scaler
		dev_np = of_parse_phandle(vout->v4l2_np, "scaler", 0);
		if (dev_np != NULL) {
			/* swreser bit */
			ret = of_property_read_u32_index(vout->v4l2_np, "scaler", 1, &vioc->sc.id);
			vioc->sc.addr = VIOC_SC_GetAddress(get_vioc_index(vioc->sc.id));
			dprintk("[M2M] SCALER < vir_addr = 0x%p , id = %d\n", vioc->sc.addr, get_vioc_index(vioc->sc.id));
		} else {
			dprintk("[M2M] could not find scaler node of vout driver.\n");
		}
	} else {
		if (vout->opened == 0) {
			(void)pr_err("[ERR][VOUT] not open vout driver.\n");
			ret = -EBUSY;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto err_exit;
		}

	}

	switch (vioc->m2m_rdma.id) {
	case VIOC_RDMA07:
		vioc->m2m_wmix.pos = 3;				// rdma pos 3 is VRDMA
		vioc->m2m_subplane_wmix.pos = 1;			// rdma pos 1 is GRDMA
		vioc->m2m_wmix.ovp = 24;
		vioc->m2m_subplane_wmix.ovp = 24;		// ovp 24: 0-1-2-3
		break;
#if !defined(CONFIG_ARCH_TCC897X)
	case VIOC_RDMA11:
		vioc->m2m_wmix.pos = 3;				// rdma pos 3 is VRDMA
		vioc->m2m_subplane_wmix.pos = 2;			// rdma pos 0/1/2 is GRDMA8/9/10
		vioc->m2m_wmix.ovp = 24;
		vioc->m2m_subplane_wmix.ovp = 24;		// ovp 24: 0-1-2-3
		break;
#endif
	case VIOC_RDMA12:
		vioc->m2m_wmix.pos = 0;				// rdma pos 0 is VRDMA
		vioc->m2m_subplane_wmix.pos = 1;			// rdma pos 1 is GRDMA
		vioc->m2m_wmix.ovp = 5;
		vioc->m2m_subplane_wmix.ovp = 5;			// ovp 5: 3-2-1-0
		break;
	case VIOC_RDMA13:
		vioc->m2m_wmix.pos = 1;				// rdma pos 1 is VRDMA
		vioc->m2m_subplane_wmix.pos = 0;			// rdma pos 0 is GRDMA
		vioc->m2m_wmix.ovp = 24;
		vioc->m2m_subplane_wmix.ovp = 24;		// ovp 24: 0-1-2-3
		break;
	#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_RDMA14:
		vioc->m2m_wmix.pos = 0;
		vioc->m2m_subplane_wmix.pos = 1;
		vioc->m2m_wmix.ovp = 24;
		vioc->m2m_subplane_wmix.ovp = 24;
		break;
	case VIOC_RDMA15:
		vioc->m2m_wmix.pos = 1;
		vioc->m2m_subplane_wmix.pos = 0;
		vioc->m2m_wmix.ovp = 24;
		vioc->m2m_subplane_wmix.ovp = 24;
		break;
	#endif
	case VIOC_RDMA16:
		vioc->m2m_wmix.pos = 1;
		vioc->m2m_subplane_wmix.pos = 0;
		vioc->m2m_wmix.ovp = 24;
		vioc->m2m_subplane_wmix.ovp = 24;
		break;

	#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_RDMA17:
		vioc->m2m_wmix.pos = 1;
		vioc->m2m_subplane_wmix.pos = 0;
		vioc->m2m_wmix.ovp = 24;
		vioc->m2m_subplane_wmix.ovp = 24;
		break;
	#endif

	default:
		(void)pr_err("[ERR][VOUT] invalid m2m_rdma(%d) index\n", vioc->m2m_rdma.id);
		ret = -1;
		break;
	}

	if(ret == -1) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_exit;
	}

	// set m2m wmix
	dev_np = of_parse_phandle(vout->v4l2_np, "m2m_wmix", 0);
	if (dev_np != NULL) {
		/* swreser bit */
		ret = of_property_read_u32_index(vout->v4l2_np, "m2m_wmix", 1, &vioc->m2m_wmix.id);
		vioc->m2m_wmix.addr = VIOC_WMIX_GetAddress(get_vioc_index(vioc->m2m_wmix.id));
		dprintk("[M2M] WMIX < vir_addr = 0x%p , id = %d\n", vioc->m2m_wmix.addr, get_vioc_index(vioc->m2m_wmix.id));
	} else {
		dprintk("[M2M] could not find wmix node of vout driver.\n");
	}

	// set m2m wdma
	dev_np = of_parse_phandle(vout->v4l2_np, "m2m_wdma", 0);
	if (dev_np != NULL) {
		/* swreser bit */
		ret = of_property_read_u32_index(vout->v4l2_np, "m2m_wdma", 1, &vioc->m2m_wdma.id);
		vioc->m2m_wdma.addr = VIOC_WDMA_GetAddress(get_vioc_index(vioc->m2m_wdma.id));
		vioc->m2m_wdma.irq = irq_of_parse_and_map(dev_np, u32_to_s32(get_vioc_index(vioc->m2m_wdma.id)));

		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		vioc->m2m_wdma.vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
		if (vioc->m2m_wdma.vioc_intr == NULL) {
			(void)pr_err("[ERR][VOUT] memory allocation faiil (vioc->m2m_wdma)\n");
			ret = -ENOMEM;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto err_exit;
		}
		vioc->m2m_wdma.vioc_intr->id = s32_to_u32(VIOC_INTR_WD0) + get_vioc_index(vioc->m2m_wdma.id);
		vioc->m2m_wdma.vioc_intr->bits = VIOC_WDMA_IREQ_EOFR_MASK;
		dprintk("[M2M] WDMA < vir_addr = 0x%p , id = %d, irq = %d\n", vioc->m2m_wdma.addr, get_vioc_index(vioc->m2m_wdma.id), vioc->m2m_wdma.irq);
	} else {
		dprintk("[M2M] could not find wdma node of vout driver.\n");
	}

	#ifdef CONFIG_VOUT_USE_SUB_PLANE
	// set m2m subplane rdma
	dev_np = of_parse_phandle(vout->v4l2_np, "m2m_subplane_rdma", 0);
	if (dev_np != NULL) {
		/* swreser bit */
		ret = of_property_read_u32_index(vout->v4l2_np, "m2m_subplane_rdma", 1, &vioc->m2m_subplane_rdma.id);
		vioc->m2m_subplane_rdma.addr = VIOC_RDMA_GetAddress(get_vioc_index(vioc->m2m_subplane_rdma.id));
		dprintk("[M2M] SUB_RDMA < vir_addr = 0x%p , id = %d\n", vioc->m2m_subplane_rdma.addr, get_vioc_index(vioc->m2m_subplane_rdma.id));
	} else {
		dprintk("[M2M] could not find rdma node of vout driver.\n");
	}

	// set m2m subtitle wmixer
	vioc->m2m_subplane_wmix.id = vioc->m2m_wmix.id;
	vioc->m2m_subplane_wmix.addr =  vioc->m2m_wmix.addr;
	#endif

	// set deinterlacer information
	if (!(bool)of_property_read_u32_index(vout->v4l2_np, "deintl_block", 1, &deinterlacer)) {
		if (get_vioc_type(deinterlacer) == get_vioc_type(VIOC_DEINTLS)) {
			vout->deinterlace = VOUT_DEINTL_S;

			/* path plugin */
			vioc->deintl_s.id = deinterlacer;
			dprintk("DEINTL_S < id = %d\n", vioc->deintl_s.id);
		} else if (get_vioc_type(deinterlacer) == get_vioc_type(VIOC_VIQE)) {
			dev_np = of_parse_phandle(vout->v4l2_np, "deintl_block", 0);
			if (dev_np != NULL) {
				vout->deinterlace = VOUT_DEINTL_VIQE_3D;

				vioc->viqe.id = deinterlacer;
				vioc->viqe.addr = VIOC_VIQE_GetAddress(get_vioc_index(vioc->viqe.id));
				#ifdef CONFIG_VOUT_KEEP_VIDEO_LAYER
				vioc->deintl_s.id = VIOC_DEINTLS0
				#endif
				dprintk("VIQE < vir_addr = 0x%p , id = %d\n", vioc->viqe.addr, get_vioc_index(vioc->viqe.id));
			} else {
				(void)pr_err("[ERR][VOUT] could not find viqe node of vout driver.\n");
			}
		} else {
			/* prevent KCS */
		}
	} else {
		vout->deinterlace = VOUT_DEINTL_NONE;
		dprintk("Does not support deinterlacing (v4l2_vout%d device)\n", vout->id);
	}

	dprintk("RDMA%d%s - WMIX%d - SC%d - WDMA%d\n", get_vioc_index(vioc->m2m_rdma.id),
		vout->deinterlace == VOUT_DEINTL_VIQE_3D ? " - VIQE" : vout->deinterlace == VOUT_DEINTL_S ? " - DEINTL" : "",
		get_vioc_index(vioc->m2m_wmix.id), get_vioc_index(vioc->sc.id), get_vioc_index(vioc->m2m_wdma.id));

	#ifdef CONFIG_VIOC_MAP_DECOMP
	dev_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_mc");
	if (dev_np != NULL) {
		vioc->map_converter.id = VIOC_MC + vout->id;
		dprintk("MC < id = %d\n", get_vioc_index(vioc->map_converter.id));
	}
	#endif

	#ifdef CONFIG_VIOC_DTRC_DECOMP
	dev_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_dtrc");
	if (dev_np != NULL) {
		vioc->dtrc.id = VIOC_DTRC + vout->id;
		dprintk("DTRC < id = %d\n", get_vioc_index(vioc->dtrc.id));
	}
	#endif

	ret = 0;
err_exit:
	return ret;
}
#if defined(CONFIG_TCC_DUAL_DISPLAY)
int vout_set_m2m_dual_path(struct tcc_vout_device *vout)
{
	struct device_node *dev_np;
	struct tcc_vout_vioc *vioc = vout->vioc;
	int i, ret;

	/* get pmap for m2m_dual_bufs */
	(void)memcpy(&vout->m2m_dual_pmap.name, kasprintf(GFP_KERNEL, "dual_display"),
		sizeof(VOUT_DUAL_PATH_PMAP_NAME));
	if (vout_get_pmap(&vout->m2m_dual_pmap)) {
		(void)pr_err("[ERR][VOUT] vout_get_pmap(%s)\n", vout->m2m_dual_pmap.name);
		return -1;
	}

	for (i = M2M_DUAL_0; i < M2M_DUAL_MAX; i++) {
		/* bottom-field first */
		vioc->m2m_dual_rdma[i].bf = 1;

		// set m2m rdma
		dev_np = of_parse_phandle(vout->v4l2_np, "m2m_dual_rdma", 0);
		if (dev_np != NULL) {
			/* swreser bit */
			ret = of_property_read_u32_index(vout->v4l2_np, "m2m_dual_rdma",
				i+1, &vioc->m2m_dual_rdma[i].id);
			vioc->m2m_dual_rdma[i].addr =
				VIOC_RDMA_GetAddress(get_vioc_index(vioc->m2m_dual_rdma[i].id));
			dprintk("[EXT] RDMA < vir_addr = 0x%p , id = %d ,\n",
				vioc->m2m_dual_rdma[i].addr, get_vioc_index(vioc->m2m_dual_rdma[i].id));
		} else {
			dprintk("[EXT] could not find rdma node of vout driver.\n");
		}

		// set scaler
		dev_np = of_parse_phandle(vout->v4l2_np, "m2m_dual_scaler", 0);
		if (dev_np != NULL) {
			/* swreser bit */
			ret = of_property_read_u32_index(vout->v4l2_np, "m2m_dual_scaler",
				i+1, &vioc->m2m_dual_sc[i].id);
			vioc->m2m_dual_sc[i].addr = VIOC_SC_GetAddress(
				get_vioc_index(vioc->m2m_dual_sc[i].id));
			dprintk("[M2M] ext_scaler < vir_addr = 0x%p , id = %d\n",
				vioc->m2m_dual_sc[i].addr, get_vioc_index(vioc->m2m_dual_sc[i].id));
		} else {
			dprintk("[M2M] could not find ext_scaler node of vout driver.\n");
		}

		switch (vioc->m2m_dual_rdma[i].id) {
		case VIOC_RDMA07:
			vioc->m2m_dual_wmix[i].pos = 3;				// rdma pos 3 is VRDMA
			vioc->m2m_subplane_wmix.pos = 1;			// rdma pos 1 is GRDMA
			vioc->m2m_dual_wmix[i].ovp = vioc->m2m_subplane_wmix.ovp = 24;		// ovp 24: 0-1-2-3
			break;
		case VIOC_RDMA11:
			vioc->m2m_dual_wmix[i].pos = 3;				// rdma pos 3 is VRDMA
			vioc->m2m_subplane_wmix.pos = 2;			// rdma pos 0/1/2 is GRDMA8/9/10
			vioc->m2m_dual_wmix[i].ovp = vioc->m2m_subplane_wmix.ovp = 24;		// ovp 24: 0-1-2-3
			break;
		case VIOC_RDMA12:
			vioc->m2m_dual_wmix[i].pos = 0;				// rdma pos 0 is VRDMA
			vioc->m2m_subplane_wmix.pos = 1;			// rdma pos 1 is GRDMA
			vioc->m2m_dual_wmix[i].ovp = vioc->m2m_subplane_wmix.ovp = 5;			// ovp 5: 3-2-1-0
			break;
		case VIOC_RDMA13:
			vioc->m2m_dual_wmix[i].pos = 1;				// rdma pos 1 is VRDMA
			vioc->m2m_subplane_wmix.pos = 0;			// rdma pos 0 is GRDMA
			vioc->m2m_dual_wmix[i].ovp = vioc->m2m_subplane_wmix.ovp = 24;		// ovp 24: 0-1-2-3
		#ifdef CONFIG_ARCH_TCC803X
		case VIOC_RDMA14:
			vioc->m2m_dual_wmix[i].pos = 0;
			vioc->m2m_subplane_wmix.pos = 1;
			vioc->m2m_dual_wmix[i].ovp = vioc->m2m_subplane_wmix.ovp = 24;
			break;
		case VIOC_RDMA15:
			vioc->m2m_dual_wmix[i].pos = 1;
			vioc->m2m_subplane_wmix.pos = 0;
			vioc->m2m_dual_wmix[i].ovp = vioc->m2m_subplane_wmix.ovp = 24;
			break;
		#endif
		case VIOC_RDMA16:
			vioc->m2m_dual_wmix[i].pos = 1;
			vioc->m2m_subplane_wmix.pos = 0;
			vioc->m2m_dual_wmix[i].ovp = vioc->m2m_subplane_wmix.ovp = 24;
			break;
		default:
			(void)pr_err("[ERR][VOUT] invalid m2m_dual_rdma[i](%d) index\n", vioc->m2m_dual_rdma[i].id);
			return -1;
		}

		// set m2m wmix
		dev_np = of_parse_phandle(vout->v4l2_np, "m2m_dual_wmix", 0);
		if (dev_np != NULL) {
			/* swreser bit */
			ret = of_property_read_u32_index(vout->v4l2_np, "m2m_dual_wmix", i+1, &vioc->m2m_dual_wmix[i].id);
			vioc->m2m_dual_wmix[i].addr = VIOC_WMIX_GetAddress(get_vioc_index(vioc->m2m_dual_wmix[i].id));
			dprintk("[EXT] WMIX < vir_addr = 0x%p , id = %d\n", vioc->m2m_dual_wmix[i].addr, get_vioc_index(vioc->m2m_dual_wmix[i].id));
		} else {
			dprintk("[EXT] could not find wmix node of vout driver.\n");
		}

		// set m2m wdma
		dev_np = of_parse_phandle(vout->v4l2_np, "m2m_dual_wdma", 0);
		if (dev_np != NULL) {
			/* swreser bit */
			ret = of_property_read_u32_index(vout->v4l2_np, "m2m_dual_wdma", i+1, &vioc->m2m_dual_wdma[i].id);
			vioc->m2m_dual_wdma[i].addr = VIOC_WDMA_GetAddress(get_vioc_index(vioc->m2m_dual_wdma[i].id));
			vioc->m2m_dual_wdma[i].irq = irq_of_parse_and_map(dev_np, get_vioc_index(vioc->m2m_dual_wdma[i].id));

			vioc->m2m_dual_wdma[i].vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
			if (vioc->m2m_dual_wdma[i].vioc_intr == 0) {
				(void)pr_err("[ERR][VOUT] memory allocation faiil (vioc->m2m_dual_wdma[i])\n");
				return -ENOMEM;
			}
			vioc->m2m_dual_wdma[i].vioc_intr->id = VIOC_INTR_WD0 + get_vioc_index(vioc->m2m_dual_wdma[i].id);
			vioc->m2m_dual_wdma[i].vioc_intr->bits = VIOC_WDMA_IREQ_EOFR_MASK;
			dprintk("[M2M] WDMA < vir_addr = 0x%p , id = %d, irq = %d\n", vioc->m2m_dual_wdma[i].addr, get_vioc_index(vioc->m2m_dual_wdma[i].id), vioc->m2m_dual_wdma[i].irq);
		} else {
			dprintk("[M2M] could not find wdma node of vout driver.\n");
		}

		dprintk("RDMA%d%s - WMIX%d - SC%d - WDMA%d\n", get_vioc_index(vioc->m2m_dual_rdma[i].id),
			vout->deinterlace == VOUT_DEINTL_VIQE_3D ? " - VIQE" : vout->deinterlace == VOUT_DEINTL_S ? " - DEINTL" : "",
			get_vioc_index(vioc->m2m_dual_wmix[i].id), get_vioc_index(vioc->m2m_dual_sc[i].id), get_vioc_index(vioc->m2m_dual_wdma[i].id));
	}

	return 0;
}

int vout_set_vout_dual_path(struct tcc_vout_device *vout)
{
	struct device_node *dev_np;
	struct tcc_vout_vioc *vioc = vout->vioc;

	// set display rdma
	dev_np = of_parse_phandle(vout->v4l2_np, "rdma", 0);
	if (dev_np != NULL) {
		/* swreser bit */
		ret = of_property_read_u32_index(vout->v4l2_np, "rdma", 2, &vioc->rdma_dual.id);
		vioc->rdma_dual.addr = VIOC_RDMA_GetAddress(get_vioc_index(vioc->rdma_dual.id));
		dprintk("[DISP] RDMA_dual < vir_addr = 0x%p , id = %d\n", vioc->rdma_dual.addr, get_vioc_index(vioc->rdma_dual.id));
	} else {
		dprintk("[DISP] could not find rdma_dual node of vout driver.\n");
	}

	// set display wmix
	dev_np = of_parse_phandle(vout->v4l2_np, "wmix", 0);
	if (dev_np != NULL) {
		/* swreser bit */
		ret = of_property_read_u32_index(vout->v4l2_np, "wmix", 2, &vioc->wmix_dual.id);
		vioc->wmix_dual.addr = VIOC_WMIX_GetAddress(get_vioc_index(vioc->wmix_dual.id));
		dprintk("[DISP] WMIX_dual < vir_addr = 0x%p , id = %d\n",
			vioc->wmix_dual.addr, get_vioc_index(vioc->wmix_dual.id));
	} else {
		dprintk("[DISP] could not find wmix_dual node of vout driver.\n");
	}

	switch (vioc->rdma_dual.id) {
	case VIOC_RDMA01:
	case VIOC_RDMA02:
	case VIOC_RDMA03:
	case VIOC_RDMA05:
#if !defined(CONFIG_ARCH_TCC899X)
	case VIOC_RDMA06:
#endif
	case VIOC_RDMA07:
	case VIOC_RDMA09:
	case VIOC_RDMA10:
	case VIOC_RDMA11:
		if (get_vioc_index(vioc->wmix_dual.id))
			vioc->wmix_dual.pos = get_vioc_index(vioc->rdma_dual.id) - (0x4 * get_vioc_index(vioc->wmix_dual.id));
		else
			vioc->wmix_dual.pos = get_vioc_index(vioc->rdma_dual.id);
		break;
	default:
		(void)pr_err("[ERR][VOUT] invalid rdma(%d) index\n", get_vioc_index(vioc->rdma_dual.id));
		return -1;
	}

	dev_np = of_parse_phandle(vout->v4l2_np, "disp", 0);
	if (dev_np != NULL) {
		ret = of_property_read_u32_index(vout->v4l2_np, "disp", 2, &vioc->disp_dual.id);
		vioc->disp_dual.addr = VIOC_DISP_GetAddress(get_vioc_index(vioc->disp_dual.id));

		dprintk("[DISP] DISP < vir_addr = 0x%p , id = %d\n", vioc->disp_dual.addr, get_vioc_index(vioc->disp_dual.id));
	} else {
		dprintk("[DISP_dual] could not find disp_dual node of vout driver.\n");
	}

	dprintk("RDMA%d - WMIX%d - DISP%d\n", get_vioc_index(vioc->rdma_dual.id), get_vioc_index(vioc->wmix_dual.id), get_vioc_index(vioc->disp_dual.id));
	return 0;
}
#endif

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
int vout_set_vout_path(struct tcc_vout_device *vout)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_node *dev_np;
	struct tcc_vout_vioc *vioc = vout->vioc;
	int ret;

	// set display rdma
	dev_np = of_parse_phandle(vout->v4l2_np, "rdma", 0);
	if (dev_np != NULL) {
		/* swreser bit */
		ret = of_property_read_u32_index(vout->v4l2_np, "rdma", 1, &vioc->rdma.id);
		vioc->rdma.addr = VIOC_RDMA_GetAddress(get_vioc_index(vioc->rdma.id));
		dprintk("[DISP] RDMA < vir_addr = 0x%p , id = %d\n", vioc->rdma.addr, get_vioc_index(vioc->rdma.id));
	} else {
		dprintk("[DISP] could not find rdma node of vout driver.\n");
	}

	// set display wmix
	dev_np = of_parse_phandle(vout->v4l2_np, "wmix", 0);
	if (dev_np != NULL) {
		/* swreser bit */
		ret = of_property_read_u32_index(vout->v4l2_np, "wmix", 1, &vioc->wmix.id);
		vioc->wmix.addr = VIOC_WMIX_GetAddress(get_vioc_index(vioc->wmix.id));
		dprintk("[DISP] WMIX < vir_addr = 0x%p , id = %d\n", vioc->wmix.addr, get_vioc_index(vioc->wmix.id));
	} else {
		dprintk("[DISP] could not find wmix node of vout driver.\n");
	}

	if (get_vioc_index(vioc->wmix.id) != 0U) {
		vioc->wmix.pos = get_vioc_index(vioc->rdma.id) - (0x4U * get_vioc_index(vioc->wmix.id));
	} else {
		vioc->wmix.pos = get_vioc_index(vioc->rdma.id);
	}

	dev_np = of_parse_phandle(vout->v4l2_np, "disp", 0);
	if (dev_np != NULL) {
		ret = of_property_read_u32_index(vout->v4l2_np, "disp", 1, &vioc->disp.id);
		vioc->disp.addr = VIOC_DISP_GetAddress(get_vioc_index(vioc->disp.id));
		#ifdef CONFIG_VOUT_USE_VSYNC_INT
		vioc->disp.vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
		if (vioc->disp.vioc_intr == 0) {
			(void)pr_err("[ERR][VOUT] memory allocation faiil (vioc->disp)\n");
			return -ENOMEM;
		}
		vioc->disp.irq = irq_of_parse_and_map(dev_np, get_vioc_index(vioc->disp.id));
		vioc->disp.vioc_intr->id = get_vioc_index(vioc->disp.id);
		vioc->disp.vioc_intr->bits = (1<<VIOC_DISP_INTR_VSF);
		#endif

		dprintk("[DISP] DISP < vir_addr = 0x%p , id = %d\n", vioc->disp.addr, get_vioc_index(vioc->disp.id));
	} else {
		dprintk("[DISP] could not find disp node of vout driver.\n");
	}

	#ifdef CONFIG_VOUT_DISPLAY_LASTFRAME
	dev_np = of_parse_phandle(vout->v4l2_np, "lastframe_rdma", 0);
	if (dev_np != NULL) {
		/* swreser bit */
		ret = of_property_read_u32_index(vout->v4l2_np, "lastframe_rdma", 1, &vioc->lastframe_rdma.id);
		vioc->lastframe_rdma.addr = VIOC_RDMA_GetAddress(get_vioc_index(vioc->lastframe_rdma.id));
		dprintk("[DISP] LASTFRAME_RDMA < vir_addr = 0x%p , id = %d\n", vioc->lastframe_rdma.addr, get_vioc_index(vioc->lastframe_rdma.id));
	} else {
		dprintk("[DISP] could not find lastframe_rdma node of vout driver.\n");
	}
	#endif

	dprintk("RDMA%d - WMIX%d - DISP%d\n", get_vioc_index(vioc->rdma.id), get_vioc_index(vioc->wmix.id), get_vioc_index(vioc->disp.id));
	return 0;
}

/* call by tcc_vout_v4l2_probe()
 */
int vout_vioc_set_default(struct tcc_vout_device *vout)
{
	int ret;
	struct tcc_vout_vioc *vioc = vout->vioc;

	vioc->sc.id = 0/*VIOC_SC_00*/;

	/* |========= WMIX Overlay Priority =========|
	 * | OVP | LAYER3 | LAYER2 | LAYER1 | LAYER0 |
	 * |-----------------------------------------|
	 * |  24 | image0 | image1 | image2 | image3 |
	 * |  25 | image0 | image2 | image1 | image3 |
	 * |=========================================|
	 * (LAYER3 is highest layer)
	 */
	vioc->wmix.ovp = 24;

	/* get pmap for disp_bufs (only V4L2_MEMORY_MMAP) */
	(void)memcpy(&vout->pmap.name, VOUT_DISP_PATH_PMAP_NAME, sizeof(VOUT_DISP_PATH_PMAP_NAME));

#if defined(CONFIG_TCC_DUAL_DISPLAY)
	ret = vout_set_vout_path(vout);
	ret |= vout_set_vout_dual_path(vout);

	ret |= vout_set_m2m_path(1, vout);	//m2m path
	ret |= vout_set_m2m_dual_path(vout);	//m2m dual path
#else
	ret = vout_set_vout_path(vout);
	ret = vout_set_m2m_path(1, vout);
#endif

	#ifdef CONFIG_VOUT_USE_VSYNC_INT
	if (vout->vout_timer == NULL) {
		vout->vout_timer = tcc_register_timer(NULL, 1000/*msec*/, NULL);
		if (IS_ERR(vout->vout_timer)) {
			(void)pr_err("[ERR][VOUT] %s: cannot register tcc timer. ret:0x%p\n", __func__, vout->vout_timer);
			vout->vout_timer = NULL;
		}
	}
	#endif

	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void vout_disp_ctrl(struct tcc_vout_vioc *vioc, int enable)
{
	if (enable != 0) {
		VIOC_RDMA_SetImageEnable(vioc->rdma.addr);
	} else {
		VIOC_RDMA_SetImageDisable(vioc->rdma.addr);
	}
	dprintk("%d\n", enable);

#if defined(CONFIG_TCC_DUAL_DISPLAY)
	if (enable)
		VIOC_RDMA_SetImageEnable(vioc->rdma_dual.addr);
	else
		VIOC_RDMA_SetImageDisable(vioc->rdma_dual.addr);
#endif
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void vout_rdma_setup(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	struct vioc_rdma *disp_rdma = &vioc->rdma;

	disp_rdma->width = vioc->m2m_wdma.width;
	disp_rdma->height = vioc->m2m_wdma.height;
	VIOC_RDMA_SetImageSize(disp_rdma->addr, disp_rdma->width, disp_rdma->height);
	VIOC_RDMA_SetImageFormat(disp_rdma->addr, disp_rdma->fmt);
	VIOC_RDMA_SetImageOffset(disp_rdma->addr, disp_rdma->fmt, disp_rdma->width);
	VIOC_RDMA_SetImageY2REnable(disp_rdma->addr, disp_rdma->y2r);
	VIOC_RDMA_SetImageY2RMode(disp_rdma->addr, disp_rdma->y2rmd);
	VIOC_RDMA_SetImageDisable(disp_rdma->addr);

#if defined(CONFIG_TCC_DUAL_DISPLAY)
	VIOC_RDMA_SetImageBfield(vioc->rdma_dual.addr, disp_rdma->bf);
	VIOC_RDMA_SetImageIntl(vioc->rdma_dual.addr, disp_rdma->intl);
	VIOC_RDMA_SetImageFormat(vioc->rdma_dual.addr, disp_rdma->fmt);
	VIOC_RDMA_SetImageY2REnable(vioc->rdma_dual.addr, disp_rdma->y2r);
	VIOC_RDMA_SetImageDisable(vioc->rdma_dual.addr);
#endif
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void vout_wmix_setup(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;

	vioc->wmix.left = (vout->disp_rect.left < 0) ? 0U : s32_to_u32(vout->disp_rect.left);
	vioc->wmix.top = (vout->disp_rect.top < 0) ? 0U : s32_to_u32(vout->disp_rect.top);

	VIOC_WMIX_SetPosition(vioc->wmix.addr, vioc->wmix.pos,
			vioc->wmix.left, vioc->wmix.top);
	#ifdef CONFIG_TCC_VIOCMG
	viocmg_set_wmix_ovp(VIOCMG_CALLERID_VOUT, get_vioc_index(vioc->wmix.id), vioc->wmix.ovp);
	#endif
	VIOC_WMIX_SetUpdate(vioc->wmix.addr);

	dprintk("wmix position(%d,%d)\n", vout->disp_rect.left, vout->disp_rect.top);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void vout_wmix_getsize(struct tcc_vout_device *vout, unsigned int *w, unsigned int *h)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct tcc_vout_vioc *vioc = vout->vioc;

	VIOC_WMIX_GetSize(vioc->wmix.addr, w, h);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void vout_path_reset(struct tcc_vout_vioc *vioc)
{
	VIOC_CONFIG_SWReset(vioc->rdma.id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(vioc->rdma.id, VIOC_CONFIG_CLEAR);
}

#if defined(CONFIG_TCC_DUAL_DISPLAY)
void m2m_dual_path_reset(struct tcc_vout_vioc *vioc, int m2m_dual_index)
{
	/* reset state */
	VIOC_CONFIG_SWReset(vioc->m2m_dual_wdma[m2m_dual_index].id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(vioc->m2m_dual_sc[m2m_dual_index].id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(vioc->m2m_dual_wmix[m2m_dual_index].id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(vioc->m2m_dual_rdma[m2m_dual_index].id, VIOC_CONFIG_RESET);

	/* normal state */
	VIOC_CONFIG_SWReset(vioc->m2m_dual_rdma[m2m_dual_index].id, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(vioc->m2m_dual_wmix[m2m_dual_index].id, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(vioc->m2m_dual_sc[m2m_dual_index].id, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(vioc->m2m_dual_wdma[m2m_dual_index].id, VIOC_CONFIG_CLEAR);
}
#endif

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void m2m_path_reset(struct tcc_vout_vioc *vioc)
{
	/* reset state */
	VIOC_CONFIG_SWReset(vioc->m2m_wdma.id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(vioc->sc.id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(vioc->m2m_wmix.id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(vioc->m2m_rdma.id, VIOC_CONFIG_RESET);

	/* normal state */
	VIOC_CONFIG_SWReset(vioc->m2m_rdma.id, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(vioc->m2m_wmix.id, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(vioc->sc.id, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(vioc->m2m_wdma.id, VIOC_CONFIG_CLEAR);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void m2m_rdma_setup(struct vioc_rdma *rdma)
{
	VIOC_RDMA_SetImageRGBSwapMode(rdma->addr, rdma->rgbswap);
	VIOC_RDMA_SetImageBfield(rdma->addr, rdma->bf);
	VIOC_RDMA_SetImageIntl(rdma->addr, rdma->intl);

	VIOC_RDMA_SetImageY2REnable(rdma->addr, rdma->y2r);
	VIOC_RDMA_SetImageY2RMode(rdma->addr, rdma->y2rmd);

	VIOC_RDMA_SetImageBase(rdma->addr, rdma->img.base0, rdma->img.base1, rdma->img.base2);
	VIOC_RDMA_SetImageSize(rdma->addr, rdma->width, rdma->height);
	VIOC_RDMA_SetImageFormat(rdma->addr, rdma->fmt);

	if (rdma->y_stride != 0U) {
		if (rdma->fmt > VIOC_IMG_FMT_ARGB6666_3) {
			VIOC_RDMA_SetImageOffset(rdma->addr, rdma->fmt, rdma->y_stride);
		} else {
			VIOC_RDMA_SetImageOffset(rdma->addr, (unsigned int)TCC_LCDC_IMG_FMT_RGB332, rdma->y_stride);
		}
	} else {
		VIOC_RDMA_SetImageOffset(rdma->addr, rdma->fmt, rdma->width);
	}
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void m2m_wmix_setup(struct vioc_wmix *wmix)
{
	#ifdef CONFIG_TCC_VIOCMG
	viocmg_set_wmix_ovp(VIOCMG_CALLERID_VOUT, get_vioc_index(wmix->id), wmix->ovp);
	#else
	VIOC_WMIX_SetOverlayPriority(wmix->addr, wmix->ovp);
	#endif
	VIOC_WMIX_SetSize(wmix->addr, wmix->width, wmix->height);
	VIOC_WMIX_SetPosition(wmix->addr, wmix->pos, wmix->left, wmix->top);
	VIOC_WMIX_SetUpdate(wmix->addr);
}

#if defined(CONFIG_TCC_DUAL_DISPLAY)
static void m2m_scaler_setup(struct tcc_vout_device *vout, int m2m_dual_index)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int sw, sh;	// src image size in rdma
	unsigned int dw, dh;	// destination size in scaler
	int i, bypass = 0;

	sw = vioc->m2m_wdma.width;
	sh = vioc->m2m_wdma.height;
	dw = vout->disp_rect.width;
	dh = vout->disp_rect.height;

	if ((dw == sw) && (dh == sh)) {
		dprintk("Bypass scaler (same size)\n");
		bypass = 1;
	}

	VIOC_SC_SetBypass(vioc->m2m_dual_sc[m2m_dual_index].addr, bypass);
	VIOC_SC_SetDstSize(vioc->m2m_dual_sc[m2m_dual_index].addr,
		vout->disp_rect.left < 0 ? dw + abs(vout->disp_rect.left) : dw,
		vout->disp_rect.top < 0 ? dh + abs(vout->disp_rect.top) : dh);		// set destination size
	VIOC_SC_SetOutPosition(vioc->sc.addr,
		vout->disp_rect.left < 0 ? abs(vout->disp_rect.left) : 0,
		vout->disp_rect.top < 0 ? abs(vout->disp_rect.top) : 0);			// set output position
	VIOC_SC_SetOutSize(vioc->m2m_dual_sc[m2m_dual_index].addr, dw, dh);								// set output size in scaler

	ret = VIOC_CONFIG_PlugIn(vioc->m2m_dual_sc[m2m_dual_index].id, vioc->m2m_dual_rdma[m2m_dual_index].id);	// plugin position in scaler

	dprintk("%dx%d->[scaler]->%dx%d\n", sw, sh, dw, dh);
}
#endif

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void scaler_setup(struct tcc_vout_device *vout)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int sw, sh;	// src image size in rdma
	unsigned int dw, dh;	// destination size in scaler
	int bypass = 0;
	int ret;

	sw = (vout->src_pix.width == vout->crop_src.width) ? vout->src_pix.width : vout->crop_src.width;
	sh = (vout->src_pix.height == vout->crop_src.height) ? vout->src_pix.height : vout->crop_src.height;
	dw = vout->disp_rect.width;
	dh = vout->disp_rect.height;

	if ((dw == sw) && (dh == sh)) {
		dprintk("Bypass scaler (same size)\n");
		bypass = 1;
	}

	VIOC_SC_SetBypass(vioc->sc.addr, s32_to_u32(bypass));
	VIOC_SC_SetDstSize(vioc->sc.addr,
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		(vout->disp_rect.left < 0) ? (dw + s32_to_u32(abs(vout->disp_rect.left))) : dw,
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		(vout->disp_rect.top < 0) ? (dh + s32_to_u32(abs(vout->disp_rect.top))) : dh);		// set destination size
	VIOC_SC_SetOutPosition(vioc->sc.addr,
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		(vout->disp_rect.left < 0) ? s32_to_u32(abs(vout->disp_rect.left)) : 0U,
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		(vout->disp_rect.top < 0) ? s32_to_u32(abs(vout->disp_rect.top)) : 0U);			// set output position
	VIOC_SC_SetOutSize(vioc->sc.addr, dw, dh);								// set output size in scaler

	if (vout->onthefly_mode != 0) {
		ret = VIOC_CONFIG_PlugIn(vioc->sc.id, vioc->rdma.id);	// plugin position in scaler
	} else {
		ret = VIOC_CONFIG_PlugIn(vioc->sc.id, vioc->m2m_wdma.id);	// plugin position in scaler
	}

	dprintk("%dx%d->[scaler]->%dx%d\n", sw, sh, dw, dh);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void m2m_wdma_setup(struct vioc_wdma *wdma)
{
	VIOC_WDMA_SetImageR2YEnable(wdma->addr, wdma->r2y);
	VIOC_WDMA_SetImageR2YMode(wdma->addr, wdma->r2ymd);
	VIOC_WDMA_SetImageBase(wdma->addr, wdma->img.base0, wdma->img.base1, wdma->img.base2);
	VIOC_WDMA_SetImageSize(wdma->addr, wdma->width, wdma->height);
	VIOC_WDMA_SetImageFormat(wdma->addr, wdma->fmt);
	VIOC_WDMA_SetImageOffset(wdma->addr, wdma->fmt, wdma->width);
	VIOC_WDMA_SetContinuousMode(wdma->addr, wdma->cont);
	VIOC_WDMA_SetImageUpdate(wdma->addr);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int deintl_s_setup(struct tcc_vout_device *vout)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct tcc_vout_vioc *vioc = vout->vioc;
	int ret = 0;

	/* reset deintl_s */
	VIOC_CONFIG_SWReset(vioc->deintl_s.id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(vioc->deintl_s.id, VIOC_CONFIG_CLEAR);

	if (vout->onthefly_mode != 0) {
		ret = VIOC_CONFIG_PlugIn(vioc->deintl_s.id, vioc->rdma.id);
	} else {
		ret = VIOC_CONFIG_PlugIn(vioc->deintl_s.id, vioc->m2m_rdma.id);
	}

	dprintk("%s\n", ret ? "failed" : "success");
	return (ret == 0) ? 0 : -EFAULT;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int deintl_viqe_setup(struct tcc_vout_device *vout, enum deintl_type deinterlace, int plugin)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct tcc_vout_vioc *vioc = vout->vioc;
	struct pmap pmap_viqe;
	unsigned int viqe_deintl_base[4] = {0};
	unsigned int framebufferWidth, framebufferHeight, img_size = 0U;
	int ret = 0;
	unsigned int deintl_en = ON;

	/* viqe config variable */
	int vmisc_tsdu = 0;									// 0: viqe size is get from vioc module
	VIOC_VIQE_DEINTL_MODE di_mode;
	VIOC_VIQE_FMT_TYPE di_fmt = VIOC_VIQE_FMT_YUV420;	// DI_DECn_MISC.FMT, DI_FMT.F422 register
														//TODO: VIOC_VIQE_FMT_YUV420 = 0, VIOC_VIQE_FMT_YUV422 = 1
#ifdef MAX_VIQE_FRAMEBUFFER
	framebufferWidth = 1920;
	framebufferHeight = 1088;
#else
	framebufferWidth = (vout->src_pix.width >> 3) << 3;
	framebufferHeight = (vout->src_pix.height >> 2) << 2;
#endif

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	if (VIOC_CONFIG_DV_GET_EDR_PATH())
		deintl_en = OFF;
#endif

	/* reset viqe */
	VIOC_CONFIG_SWReset(vioc->viqe.id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(vioc->viqe.id, VIOC_CONFIG_CLEAR);

	if((framebufferWidth <= 8192U) && (framebufferHeight <= 8192U)) {
		img_size = (framebufferWidth * framebufferHeight * 2U);
	}

	(void)pmap_get_info("viqe", &pmap_viqe);

	if (deinterlace == VOUT_DEINTL_VIQE_3D) {
		di_mode = VIOC_VIQE_DEINTL_MODE_2D;

		// ret = pmap_get_info("viqe", &pmap_viqe);
		dprintk("[PMAP] %s: 0x%08llx ~ 0x%08llx (0x%08llx)\n",
			pmap_viqe.name, pmap_viqe.base, pmap_viqe.base + pmap_viqe.size, pmap_viqe.size);

		viqe_deintl_base[0] = u64_to_u32(pmap_viqe.base);
		viqe_deintl_base[1] = viqe_deintl_base[0] + img_size;
		viqe_deintl_base[2] = viqe_deintl_base[1] + img_size;
		viqe_deintl_base[3] = viqe_deintl_base[2] + img_size;

		if (((viqe_deintl_base[3] + img_size) > u64_to_u32(pmap_viqe.base + pmap_viqe.size))
			&& (vioc->m2m_rdma.width <= 2048U)) {
			(void)pr_err("[ERR][VOUT] pmap_viqe no space\n");
			//return -ENOBUFS;
		}
	} else if (deinterlace == VOUT_DEINTL_VIQE_2D) {
		di_mode = VIOC_VIQE_DEINTL_MODE_2D;
	} else {
		di_mode = VIOC_VIQE_DEINTL_MODE_BYPASS;
	}

	// if (vmisc_tsdu == 0)
	{
		framebufferWidth = 0U;
		framebufferHeight = 0U;
	}

	VIOC_VIQE_IgnoreDecError(vioc->viqe.addr, ON, ON, ON);
	VIOC_VIQE_SetControlRegister(vioc->viqe.addr, framebufferWidth, framebufferHeight, (unsigned int)di_fmt);
	VIOC_VIQE_SetDeintlRegister(vioc->viqe.addr,
								(unsigned int)di_fmt, (unsigned int)vmisc_tsdu, framebufferWidth, framebufferHeight, di_mode,
								viqe_deintl_base[0], viqe_deintl_base[1], viqe_deintl_base[2], viqe_deintl_base[3]);
	VIOC_VIQE_SetControlEnable(vioc->viqe.addr,
								OFF,	/* histogram CDF or LUT */
								OFF,	/* histogram */
								OFF,	/* gamut mapper */
								OFF,	/* de-noiser */
								deintl_en		/* de-interlacer */
								);
	VIOC_VIQE_SetImageY2REnable(vioc->viqe.addr, vioc->viqe.y2r);
	VIOC_VIQE_SetImageY2RMode(vioc->viqe.addr, vioc->viqe.y2rmd);

	if ((plugin != 0) && (vioc->m2m_rdma.width <= 2048U)) {
		if (vout->onthefly_mode != 0) {
			ret = VIOC_CONFIG_PlugIn(vioc->viqe.id, vioc->rdma.id);
		} else {
			ret = VIOC_CONFIG_PlugIn(vioc->viqe.id, vioc->m2m_rdma.id);
		}
	}

	return (ret == 0) ? 0 : -EFAULT;
}

#ifdef CONFIG_VOUT_USE_VSYNC_INT
static inline void tcc_vout_set_time(struct tcc_vout_device *vout, int time)
{
	vout->baseTime = tcc_get_timer_count(vout->vout_timer) - time;
}

static inline int tcc_vout_get_time(struct tcc_vout_device *vout)
{
	return tcc_get_timer_count(vout->vout_timer) - vout->baseTime;
}

static int vout_check_syncTime(struct tcc_vout_device *vout, struct v4l2_buffer *buf, unsigned long base_time)
{
	unsigned long current_time;
	int diff_time, time_zone;
	#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	unsigned int display_hz = hdmi_get_refreshrate();
	#else
	unsigned int display_hz = HDMI_video_hz ? HDMI_video_hz : 60;
	#endif

	if (display_hz == 59)
		display_hz = 60;
	else if (display_hz == 23)
		display_hz = 24;
	vout->update_gap_time = 1000/(display_hz ? display_hz : 60);
	time_zone = vout->update_gap_time / 2;

	current_time = ((buf->timestamp.tv_sec*1000)+(buf->timestamp.tv_usec/1000));
	diff_time = (int)(current_time - base_time);

	if (abs(diff_time) <= time_zone/*max time gap*/) {
		dbprintk("[%d]buf->timestamp(%ld msec) base_time(%ld msec) diff_time(%d)\n", buf->index, current_time, base_time, diff_time);
	} else {
		if (diff_time <= -(time_zone)/*limit delay time*/) {
			dprintk("exception status!!! frame drop(%dmsec)\n", diff_time);
			if (diff_time <= -(vout->update_gap_time * 2))
				vout->force_sync = ON;	// to recalculate kernel time
		#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
			return VOUT_DRV_NOERR;
		#else
			return VOUT_DRV_ERR_DROPFRM;
		#endif
		}
		return VOUT_DRV_ERR_WAITFRM;
	}
	return VOUT_DRV_NOERR;
}
#endif

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
void vout_onthefly_dv_update(struct tcc_vout_device *vout, struct v4l2_buffer *buf)
{
	if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
		void __iomem *pDisp_DV = VIOC_DV_GetAddress((enum DV_DISP_TYPE)EDR_BL);

		if (vioc_get_out_type() == buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_REG_OUT_TYPE]) {
			vioc_v_dv_prog(buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_MD_HDMI_ADDR],
							buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_REG_ADDR],
							buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_CONTENT_TYPE],
							1);

			VIOC_V_DV_SetPXDW(pDisp_DV, NULL, VIOC_PXDW_FMT_24_RGB888);
			VIOC_V_DV_SetSize(pDisp_DV, NULL, vout->disp_rect.left, vout->disp_rect.top, Hactive, Vactive);
			VIOC_V_DV_Turnon(pDisp_DV, NULL);
		} else {
			(void)pr_err("[ERR][VOUT] %s-%d type mismatch(%d != %d)\n", __func__, __LINE__, vioc_get_out_type(), buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_REG_OUT_TYPE]);
		}
	}
}

void vout_onthefly_convert_video_info(struct tcc_vout_device *vout, struct v4l2_buffer *buf, struct tcc_lcdc_image_update *ImageInfo)
{
	memset(ImageInfo, 0x00, sizeof(struct tcc_lcdc_image_update));

	ImageInfo->enable	= 1;
	ImageInfo->on_the_fly	= 1;
	ImageInfo->Lcdc_layer	= RDMA_VIDEO;

	ImageInfo->Frame_width	= buf->m.planes[MPLANE_VID].reserved[VID_WIDTH];
	ImageInfo->Frame_height	= buf->m.planes[MPLANE_VID].reserved[VID_HEIGHT];
	ImageInfo->crop_left	= 0;
	ImageInfo->crop_top	= 0;
	ImageInfo->crop_right	= ImageInfo->Frame_width;
	ImageInfo->crop_bottom	= ImageInfo->Frame_height;

	if ((ImageInfo->crop_left != buf->m.planes[MPLANE_VID].reserved[VID_CROP_LEFT]
			|| ImageInfo->crop_top != buf->m.planes[MPLANE_VID].reserved[VID_CROP_TOP]
			|| (ImageInfo->crop_right - ImageInfo->crop_left) != buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH]
			|| (ImageInfo->crop_bottom - ImageInfo->crop_top) != buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT])
			&& (buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH] && buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT])
			&& (ImageInfo->Frame_width >= buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH] && ImageInfo->Frame_height >= buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT])) {
		ImageInfo->crop_left	= buf->m.planes[MPLANE_VID].reserved[VID_CROP_LEFT];
		ImageInfo->crop_top	= buf->m.planes[MPLANE_VID].reserved[VID_CROP_TOP];
		ImageInfo->crop_right	= ImageInfo->crop_left + buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH];
		ImageInfo->crop_bottom	= ImageInfo->crop_top + buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT];
	}

	ImageInfo->offset_x	= vout->disp_rect.left;
	ImageInfo->offset_y	= vout->disp_rect.top;
	ImageInfo->Image_width	= vout->disp_rect.width;
	ImageInfo->Image_height	= vout->disp_rect.height;

#ifdef CONFIG_VIOC_10BIT
	if (buf->flags & V4L2_BUFFER_FLAG_YUV_10BIT_MODE0)
		ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH]	= 10;
	else if (buf->flags & V4L2_BUFFER_FLAG_YUV_10BIT_MODE1)
		ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH]	= 11;
	else
		ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH]	= 0;
#endif

	ImageInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] = buf->m.planes[MPLANE_VID].reserved[VID_CONVERTER_EN];
	//dvprintk("############# MC Info(%d)\n", ImageInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO]);
	if (buf->m.planes[MPLANE_VID].reserved[VID_CONVERTER_EN] == 1) {
		ImageInfo->private_data.mapConv_info.m_uiLumaStride = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_LUMA_STRIDE];
		ImageInfo->private_data.mapConv_info.m_uiChromaStride = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_CHROMA_STRIDE];
		ImageInfo->private_data.mapConv_info.m_uiLumaBitDepth = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_LUMA_BIT_DEPTH];
		ImageInfo->private_data.mapConv_info.m_uiChromaBitDepth = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_CHROMA_BIT_DEPTH];
		ImageInfo->private_data.mapConv_info.m_uiFrameEndian = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRMAE_ENDIAN];

		ImageInfo->private_data.mapConv_info.m_CompressedY[0] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRAME_BASE_Y0];
		ImageInfo->private_data.mapConv_info.m_CompressedY[1] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRAME_BASE_Y1];
		ImageInfo->private_data.mapConv_info.m_CompressedCb[0] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRAME_BASE_C0];
		ImageInfo->private_data.mapConv_info.m_CompressedCb[1] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRAME_BASE_C1];

		ImageInfo->private_data.mapConv_info.m_FbcYOffsetAddr[0] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_OFFSET_BASE_Y0];
		ImageInfo->private_data.mapConv_info.m_FbcYOffsetAddr[1] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_OFFSET_BASE_Y1];
		ImageInfo->private_data.mapConv_info.m_FbcCOffsetAddr[0] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_OFFSET_BASE_C0];
		ImageInfo->private_data.mapConv_info.m_FbcCOffsetAddr[1] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_OFFSET_BASE_C1];

		dvprintk("Comp :: Y(0x%8x) Cb(0x%8x) FBC-Offset :: Y(0x%8x) Cb(0x%8x) Stride(Luma : %d Chroma %d) bitDepth(Luma:%d Croma:%d) Endian(%d)\n",
					ImageInfo->private_data.mapConv_info.m_CompressedY[0],
					ImageInfo->private_data.mapConv_info.m_CompressedCb[0],
					ImageInfo->private_data.mapConv_info.m_FbcYOffsetAddr[0],
					ImageInfo->private_data.mapConv_info.m_FbcCOffsetAddr[0],
					ImageInfo->private_data.mapConv_info.m_uiLumaStride,
					ImageInfo->private_data.mapConv_info.m_uiChromaStride,
					ImageInfo->private_data.mapConv_info.m_uiLumaBitDepth,
					ImageInfo->private_data.mapConv_info.m_uiChromaBitDepth,
					ImageInfo->private_data.mapConv_info.m_uiFrameEndian);
	}

	dvprintk("^%s:%d^^^^^^ Disp-Region(%d,%d ~ %dx%d) ^^^^^^^ @@@ EL Info(0x%x-0x%x-0x%x, %dx%d (%dx%d), %d bpp) #Reg(0x%x) #Meta(0x%x)\n", __func__, __LINE__,
				ImageInfo->offset_x, ImageInfo->offset_y, ImageInfo->Image_width, ImageInfo->Image_height,
				buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EL_OFFSET0], buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EL_OFFSET1], buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EL_OFFSET2],
				buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EL_FRAME_WIDTH], buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EL_FRAME_HEIGHT],
				buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EL_BUFF_WIDTH], buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EL_BUFF_HEIGHT],
				ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH],
				buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_REG_ADDR], buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_MD_HDMI_ADDR]);

	ImageInfo->fmt = ImageInfo->private_data.format = buf->m.planes[MPLANE_VID].reserved[VID_MJPEG_FORMAT] == DEC_FMT_420IL0 ? TCC_LCDC_IMG_FMT_YUV420ITL0 : TCC_LCDC_IMG_FMT_YUV420SP;//vioc->rdma.fmt;
	ImageInfo->addr0 = ImageInfo->private_data.offset[0]		= buf->m.planes[MPLANE_VID].m.userptr;
	ImageInfo->addr1 = ImageInfo->private_data.offset[1]		= buf->m.planes[MPLANE_VID].reserved[VID_BASE1];
	ImageInfo->addr2 = ImageInfo->private_data.offset[2]		= buf->m.planes[MPLANE_VID].reserved[VID_BASE2];
	ImageInfo->private_data.optional_info[VID_OPT_BUFFER_WIDTH]	= buf->m.planes[MPLANE_VID].reserved[VID_WIDTH];
	ImageInfo->private_data.optional_info[VID_OPT_BUFFER_HEIGHT]	= buf->m.planes[MPLANE_VID].reserved[VID_HEIGHT];
	ImageInfo->private_data.optional_info[VID_OPT_FRAME_WIDTH]	= buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH];
	ImageInfo->private_data.optional_info[VID_OPT_FRAME_HEIGHT]	= buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT];

	ImageInfo->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO]		= 0;
	ImageInfo->private_data.optional_info[VID_OPT_HAVE_DOLBYVISION_INFO]	= buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EN];
	ImageInfo->private_data.optional_info[VID_OPT_CONTENT_TYPE]		= buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_CONTENT_TYPE];

	ImageInfo->private_data.dolbyVision_info.reg_addr		= buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_REG_ADDR];
	ImageInfo->private_data.dolbyVision_info.md_hdmi_addr		= buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_MD_HDMI_ADDR];
	ImageInfo->private_data.dolbyVision_info.el_offset[0]		= buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EL_OFFSET0];
	ImageInfo->private_data.dolbyVision_info.el_offset[1]		= buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EL_OFFSET1];
	ImageInfo->private_data.dolbyVision_info.el_offset[2]		= buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EL_OFFSET2];
	ImageInfo->private_data.dolbyVision_info.el_buffer_width	= buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EL_BUFF_WIDTH];
	ImageInfo->private_data.dolbyVision_info.el_buffer_height	= buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EL_BUFF_HEIGHT];

	ImageInfo->private_data.dolbyVision_info.el_frame_width		= buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EL_FRAME_WIDTH];
	ImageInfo->private_data.dolbyVision_info.el_frame_height	= buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EL_FRAME_HEIGHT];
	ImageInfo->private_data.dolbyVision_info.osd_addr[0]		= NULL;
	ImageInfo->private_data.dolbyVision_info.osd_addr[1]		= NULL;
	ImageInfo->private_data.dolbyVision_info.reg_out_type		= buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_REG_OUT_TYPE];
}

void vout_onthefly_el_update(struct tcc_vout_device *vout, struct v4l2_buffer *buf)
{
	struct tcc_dp_device *pdp_data = tca_get_displayType(TCC_OUTPUT_HDMI);
	struct tcc_lcdc_image_update ImageInfo;

	vout_onthefly_convert_video_info(vout, buf, &ImageInfo);
	tca_edr_el_display_update(pdp_data, &ImageInfo);
}

#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
static struct tcc_lcdc_image_update nCopy_ImageInfo[150];
static unsigned long nTS_Prev = 0x999;
static unsigned int nIdx_copy;
static unsigned int bl_w, bl_h, el_w, el_h;
void vout_edr_certification_display(struct tcc_vout_device *vout, struct v4l2_buffer *buf)
{
	struct tcc_lcdc_image_update ImageInfo;
	struct tcc_vout_vioc *vioc = vout->vioc;
	struct tcc_dp_device *pdp_data = tca_get_displayType(TCC_OUTPUT_HDMI);

	unsigned long current_time = ((buf->timestamp.tv_sec*1000)+(buf->timestamp.tv_usec/1000));

	{
		vout_onthefly_convert_video_info(vout, buf, &ImageInfo);

		//Same resolution with HDMI out.
		ImageInfo.offset_x = 0;
		ImageInfo.offset_y = 0;

		if (ImageInfo.private_data.optional_info[VID_OPT_FRAME_WIDTH] == 0 || ImageInfo.private_data.optional_info[VID_OPT_FRAME_HEIGHT] == 0) {
			ImageInfo.private_data.optional_info[VID_OPT_FRAME_WIDTH]  = bl_w;
			ImageInfo.private_data.optional_info[VID_OPT_FRAME_HEIGHT] = bl_h;
		} else {
			bl_w = ImageInfo.private_data.optional_info[VID_OPT_FRAME_WIDTH];
			bl_h = ImageInfo.private_data.optional_info[VID_OPT_FRAME_HEIGHT];
		}

		if (ImageInfo.private_data.dolbyVision_info.el_frame_width == 0 || ImageInfo.private_data.dolbyVision_info.el_frame_height == 0) {
			ImageInfo.private_data.dolbyVision_info.el_frame_width  = el_w;
			ImageInfo.private_data.dolbyVision_info.el_frame_height = el_h;
		} else {
			el_w = ImageInfo.private_data.dolbyVision_info.el_frame_width;
			el_h = ImageInfo.private_data.dolbyVision_info.el_frame_height;
		}
		ImageInfo.private_data.dolbyVision_info.osd_addr[0] = buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_OSD_OFFSET0];
		ImageInfo.private_data.dolbyVision_info.osd_addr[1] = 0x00;

		if (nIdx_copy < 120) {//nTS_Prev != current_time) {
			ImageInfo.private_data.optional_info[VID_OPT_TIMESTAMP]  = (unsigned int)current_time;
			ImageInfo.private_data.optional_info[VID_OPT_PLAYER_IDX] = atomic_read(&vout->readable_buff_count);
			(void)memcpy(&nCopy_ImageInfo[nIdx_copy], &ImageInfo, sizeof(struct tcc_lcdc_image_update));
			nIdx_copy++;
			nTS_Prev = current_time;
		}

		dvprintk("^@New^^^^^^^^^^^^^ @@@ %d/%d, %03d :: TS: %04ld  %d bpp #BL(0x%x, %dx%d (%dx%d), 0x%x fmt) #EL(0x%x, %dx%d (%dx%d)) #OSD(0x%x/0x%x) #Reg(0x%x) #Meta(0x%x)\n",
				bStep_Check, DEF_DV_CHECK_NUM, nFrame, current_time,
				ImageInfo.private_data.optional_info[VID_OPT_BIT_DEPTH],
				ImageInfo.private_data.offset[0],
				ImageInfo.private_data.optional_info[VID_OPT_FRAME_WIDTH], ImageInfo.private_data.optional_info[VID_OPT_FRAME_HEIGHT],
				ImageInfo.private_data.optional_info[VID_OPT_BUFFER_WIDTH], ImageInfo.private_data.optional_info[VID_OPT_BUFFER_HEIGHT], ImageInfo.private_data.format,
				ImageInfo.private_data.dolbyVision_info.el_offset[0],
				ImageInfo.private_data.dolbyVision_info.el_frame_width, ImageInfo.private_data.dolbyVision_info.el_frame_height,
				ImageInfo.private_data.dolbyVision_info.el_buffer_width, ImageInfo.private_data.dolbyVision_info.el_buffer_height,
				ImageInfo.private_data.dolbyVision_info.osd_addr[0], ImageInfo.private_data.dolbyVision_info.osd_addr[1],
				ImageInfo.private_data.dolbyVision_info.reg_addr, ImageInfo.private_data.dolbyVision_info.md_hdmi_addr);
	}
	tca_edr_display_update(pdp_data, &ImageInfo);
	#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI // No UI-Blending
	VIOC_RDMA_PreventEnable_for_UI(1, ImageInfo.private_data.dolbyVision_info.osd_addr[0] == 0x00 ? 1 : 0);
	#endif
	if (nFrame != 1) {
		vout->display_done = ON;
		vout->last_cleared_buffer = buf;
	}
}
#endif
#endif

void vout_onthefly_display_update(struct tcc_vout_device *vout, struct v4l2_buffer *buf)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int base0 = 0, base1 = 0, base2 = 0;
	unsigned int res_change = OFF;
	#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC_DECOMP)
	unsigned int bY2R = ON;
	#endif
	int ret;

	#ifdef CONFIG_TCC_VIOCMG
	unsigned int viqe_locked = 0;
	unsigned int force_process = 0;
	#endif

	#ifdef CONFIG_VOUT_USE_SUB_PLANE
	if (buf->m.planes[MPLANE_VID].reserved[VID_NUM] == 2) {
		(void)memcpy(&vioc->subplane_alpha, buf->m.planes[MPLANE_SUB].reserved, sizeof(struct vioc_alpha));
		vioc->subplane_rdma.img.base0 = buf->m.planes[MPLANE_SUB].m.mem_offset;
		vout_subplane_onthefly_qbuf(vout);
	} else {
		if (vioc->subplane_enable) {
			VIOC_RDMA_SetImageDisableNW(vioc->subplane_rdma.addr);
			vioc->subplane_enable = OFF;
		}
	}
	#endif

	/* get base address */
	if (vout->memory == V4L2_MEMORY_USERPTR) {
		if (buf->timecode.type != STILL_IMGAGE) {
			base0 = u64_to_u32(buf->m.planes[MPLANE_VID].m.userptr);
			base1 = u64_to_u32(buf->m.planes[MPLANE_VID].reserved[VID_BASE1]);
			base2 = u64_to_u32(buf->m.planes[MPLANE_VID].reserved[VID_BASE2]);
		} else {
			base0 = u64_to_u32(buf->m.planes[MPLANE_VID].m.userptr);
			base1 = 0;
			base2 = 0;

			/*
			 * If the input is YUV format.
			 */
			if (vout->pfmt != (int)TCC_PFMT_RGB) {
				ret = tcc_get_base_address_of_image(
					vout->src_pix.pixelformat, base0,
					vout->src_pix.width, vout->src_pix.height,
					&base0, &base1, &base2);
			}
		}
	} else if (vout->memory == V4L2_MEMORY_MMAP) {
		base0 = vout->qbufs[buf->index].img_base0;
		base1 = vout->qbufs[buf->index].img_base1;
		base2 = vout->qbufs[buf->index].img_base2;
	} else {
		(void)pr_err("[ERR][VOUT] invalid qbuf v4l2_memory\n");
	}

	if (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_BT) {
		vioc->rdma.bf = 1U;
	} else {
		vioc->rdma.bf = 0U;
	}

	/* To prevent frequent switching of the scan type */
	if (vout->first_frame != 0) {
		vout->first_field = (enum v4l2_field)vout->src_pix.field;
		vout->first_frame = 0;
		dprintk("first_field(%s)\n", v4l2_field_names[vout->first_field]);
	}

	#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	if (buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EN] != 0) {
		//dvprintk("@@@@ Dolby Vision %d[0x%08x 0x%08x]\n", buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_EN], buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_REG_ADDR], buf->m.planes[MPLANE_VID].reserved[VID_DOLBY_MD_HDMI_ADDR]);
		if (!bStep_Check) {
			nFrame++;
			vout->dolby_frame_count = nFrame;
		} else {
			if (bStep_Check == DEF_DV_CHECK_NUM) {
				vout->dolby_frame_count = nFrame = 1;//0;
				bStep_Check--;
			} else {
				bStep_Check--;
				if (vout->src_pix.field == V4L2_FIELD_INTERLACED_TB
					|| vout->src_pix.field == V4L2_FIELD_INTERLACED_BT
					|| vout->src_pix.field == V4L2_FIELD_INTERLACED) {
					vout->firstFieldFlag = 0;
				}
				vout->display_done = ON;
				vout->last_cleared_buffer = buf;
				//dvprintk("2^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ @@@@@@@@@@@@@@@\n");
				return;
			}
		}

		if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
		#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
			vout_edr_certification_display(vout, buf);
			return;
		#endif
		}
	}

	vout_onthefly_el_update(vout, buf);
	#endif

	/* HDR support */
	#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	if (buf->m.planes[MPLANE_VID].reserved[VID_HDR_TC] == 16) {
		DRM_Packet_t drmparm;

		memset(&drmparm, 0x0, sizeof(DRM_Packet_t));

		drmparm.mInfoFrame.version = buf->m.planes[MPLANE_VID].reserved[VID_HDR_VERSION];
		drmparm.mInfoFrame.length = buf->m.planes[MPLANE_VID].reserved[VID_HDR_STRUCT_SIZE];

		drmparm.mDescriptor_type1.EOTF = 2;//buf->m.planes[MPLANE_VID].reserved[VID_HDR_EOTF];
		drmparm.mDescriptor_type1.Descriptor_ID = 0;//buf->m.planes[MPLANE_VID].reserved[VID_HDR_DESCRIPTOR_ID];
		drmparm.mDescriptor_type1.disp_primaries_x[0] = buf->m.planes[MPLANE_VID].reserved[VID_HDR_PRIMARY_X0];
		drmparm.mDescriptor_type1.disp_primaries_x[1] = buf->m.planes[MPLANE_VID].reserved[VID_HDR_PRIMARY_X1];
		drmparm.mDescriptor_type1.disp_primaries_x[2] = buf->m.planes[MPLANE_VID].reserved[VID_HDR_PRIMARY_X2];
		drmparm.mDescriptor_type1.disp_primaries_y[0] = buf->m.planes[MPLANE_VID].reserved[VID_HDR_PRIMARY_Y0];
		drmparm.mDescriptor_type1.disp_primaries_y[1] = buf->m.planes[MPLANE_VID].reserved[VID_HDR_PRIMARY_Y1];
		drmparm.mDescriptor_type1.disp_primaries_y[2] = buf->m.planes[MPLANE_VID].reserved[VID_HDR_PRIMARY_Y2];
		drmparm.mDescriptor_type1.white_point_x = buf->m.planes[MPLANE_VID].reserved[VID_HDR_WPOINT_X];
		drmparm.mDescriptor_type1.white_point_y = buf->m.planes[MPLANE_VID].reserved[VID_HDR_WPOINT_Y];
		drmparm.mDescriptor_type1.max_disp_mastering_luminance = buf->m.planes[MPLANE_VID].reserved[VID_HDR_MAX_LUMINANCE];
		drmparm.mDescriptor_type1.min_disp_mastering_luminance = buf->m.planes[MPLANE_VID].reserved[VID_HDR_MIN_LUMINANCE];
		drmparm.mDescriptor_type1.max_content_light_level = buf->m.planes[MPLANE_VID].reserved[VID_HDR_MAX_CONTENT];
		drmparm.mDescriptor_type1.max_frame_avr_light_level = buf->m.planes[MPLANE_VID].reserved[VID_HDR_MAX_PIC_AVR];

		if (memcmp(&gDRM_packet, &drmparm, sizeof(DRM_Packet_t))) {
			hdmi_set_drm(&drmparm);
			(void)memcpy(&gDRM_packet, &drmparm, sizeof(DRM_Packet_t));
		}
	}
	#endif

	/*
	 *	Change source info
	 */
	if (((vout->src_pix.width != buf->m.planes[MPLANE_VID].reserved[VID_WIDTH])
				|| (vout->src_pix.height != buf->m.planes[MPLANE_VID].reserved[VID_HEIGHT]))
			&& ((buf->m.planes[MPLANE_VID].reserved[VID_WIDTH] != 0U) && (buf->m.planes[MPLANE_VID].reserved[VID_HEIGHT] != 0U))) {
		dprintk("changing source (%dx%d)->(%dx%d)\n",
				vout->src_pix.width, vout->src_pix.height,
				buf->m.planes[MPLANE_VID].reserved[VID_WIDTH], buf->m.planes[MPLANE_VID].reserved[VID_HEIGHT]);

		vout->src_pix.width = buf->m.planes[MPLANE_VID].reserved[VID_WIDTH];
		vout->src_pix.height = buf->m.planes[MPLANE_VID].reserved[VID_HEIGHT];
		vout->src_pix.bytesperline = vout->src_pix.width * (unsigned int)tcc_vout_try_bpp(vout->src_pix.pixelformat, &vout->src_pix.colorspace);

		/* reinit crop_src */
		vout->crop_src.left = 0;
		vout->crop_src.top = 0;
		vout->crop_src.width = vout->src_pix.width;
		vout->crop_src.height = vout->src_pix.height;

		switch (tcc_vout_try_pix(vout->src_pix.pixelformat)) {
		case (int)TCC_PFMT_YUV422:
			vout->src_pix.sizeimage = vout->src_pix.width * vout->src_pix.height * 2U;
			break;
		case (int)TCC_PFMT_YUV420:
			vout->src_pix.sizeimage = vout->src_pix.width * vout->src_pix.height * 3U / 2U;
			break;
		case (int)TCC_PFMT_RGB:
		default:
			vout->src_pix.sizeimage = vout->src_pix.bytesperline * vout->src_pix.height;
			break;
		}
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		vout->src_pix.sizeimage = PAGE_ALIGN(vout->src_pix.sizeimage);

		vioc->rdma.width = vout->src_pix.width;
		if ((vout->src_pix.height % 4U) != 0U) {
			vioc->rdma.height = ROUND_UP_4(vout->src_pix.height);
			dprintk(" 4-line align: %d -> %d\n", vout->src_pix.height, vioc->rdma.height);
		} else {
			vioc->rdma.height = vout->src_pix.height;
		}

		vioc->rdma.y_stride = (buf->m.planes[MPLANE_VID].bytesused != 0U) ? (buf->m.planes[MPLANE_VID].bytesused) : (vout->src_pix.bytesperline & 0x0000FFFFU);
		VIOC_RDMA_SetImageSize(vioc->rdma.addr, vioc->rdma.width, vioc->rdma.height);
		VIOC_RDMA_SetImageOffset(vioc->rdma.addr, vioc->rdma.fmt, vioc->rdma.y_stride);

		res_change = ON;
	}

	/*
	 *	Change crop info
	 */
	if (((vout->crop_src.left != u32_to_s32(buf->m.planes[MPLANE_VID].reserved[VID_CROP_LEFT]))
				|| (vout->crop_src.top != u32_to_s32(buf->m.planes[MPLANE_VID].reserved[VID_CROP_TOP]))
				|| (vout->crop_src.width != buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH])
				|| (vout->crop_src.height != buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT]))
			&& ((buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH] != 0U) && (buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT] != 0U))
			&& ((vout->src_pix.width >= buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH])
			&& (vout->src_pix.height >= buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT]))) {
		dprintk("changing crop (%d,%d)(%dx%d)->(%d,%d)(%dx%d)\n",
				vout->crop_src.left, vout->crop_src.top, vout->crop_src.width, vout->crop_src.height,
				buf->m.planes[MPLANE_VID].reserved[VID_CROP_LEFT], buf->m.planes[MPLANE_VID].reserved[VID_CROP_TOP],
				buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH], buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT]);

		vout->crop_src.left = u32_to_s32(buf->m.planes[MPLANE_VID].reserved[VID_CROP_LEFT]);
		vout->crop_src.top = u32_to_s32(buf->m.planes[MPLANE_VID].reserved[VID_CROP_TOP]);
		vout->crop_src.width = buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH];
		vout->crop_src.height = buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT];

		ret = tcc_get_base_address(vioc->rdma.fmt, base0,
				(vioc->rdma.y_stride != 0U) ? vioc->rdma.y_stride : vioc->rdma.width,
				vioc->rdma.height,
				s32_to_u32(vout->crop_src.left), s32_to_u32(vout->crop_src.top),
				&base0, &base1, &base2);

		vioc->rdma.width = vout->crop_src.width;
		if ((vout->crop_src.height % 4U) != 0U) {
			vioc->rdma.height = ROUND_UP_4(vout->crop_src.height);
			dprintk(" 4-line align: %d -> %d\n", vout->crop_src.height, vioc->rdma.height);
		} else {
			vioc->rdma.height = vout->crop_src.height;
		}

		VIOC_RDMA_SetImageSize(vioc->rdma.addr, vioc->rdma.width, vioc->rdma.height);
		res_change = ON;
	} else if ((vout->src_pix.width != vout->crop_src.width) || (vout->src_pix.height != vout->crop_src.height)) {
		ret = tcc_get_base_address(vioc->m2m_rdma.fmt, base0,
				(vioc->rdma.y_stride != 0U )? vioc->rdma.y_stride : vioc->rdma.width,
				vioc->rdma.height,
				s32_to_u32(vout->crop_src.left), s32_to_u32(vout->crop_src.top),
				&base0, &base1, &base2);
	} else {
		/* prevent KCS */
	}

	/* rdma stride */
	if ((buf->m.planes[MPLANE_VID].bytesused != 0U) && (vioc->rdma.y_stride != buf->m.planes[MPLANE_VID].bytesused)) {
		dprintk("update rdma stride(%d -> %d)\n", vioc->rdma.y_stride, buf->m.planes[MPLANE_VID].bytesused);

		/*  change rdma stride */
		vioc->rdma.y_stride = buf->m.planes[MPLANE_VID].bytesused;
		vout->src_pix.bytesperline = buf->m.planes[MPLANE_VID].bytesused;
		VIOC_RDMA_SetImageOffset(vioc->rdma.addr, vioc->rdma.fmt, vioc->rdma.y_stride);
	}

	if (buf->m.planes[MPLANE_VID].reserved[VID_CONVERTER_EN] == 1U) {
		#ifdef CONFIG_VIOC_MAP_DECOMP
		VIOC_RDMA_SetImageDisable(vioc->rdma.addr);

		if ((vout->deinterlace == VOUT_DEINTL_VIQE_3D) || (vout->deinterlace == VOUT_DEINTL_VIQE_2D)) {
			ret = VIOC_CONFIG_PlugOut(vioc->viqe.id);
			VIOC_CONFIG_SWReset(vioc->viqe.id, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(vioc->viqe.id, VIOC_CONFIG_CLEAR);
		} else if (vout->deinterlace == VOUT_DEINTL_S) {
			ret = VIOC_CONFIG_PlugOut(vioc->deintl_s.id);
			VIOC_CONFIG_SWReset(vioc->deintl_s.id, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(vioc->deintl_s.id, VIOC_CONFIG_CLEAR);
		} else {
			/* prevent KCS */
		}

		ret = VIOC_CONFIG_PlugOut(vioc->sc.id);
		VIOC_CONFIG_SWReset(vioc->sc.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(vioc->sc.id, VIOC_CONFIG_CLEAR);

		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			if (VIOC_CONFIG_DMAPath_Select(vioc->rdma.id) != u32_to_s32(vioc->map_converter.id)) {
				(void)VIOC_CONFIG_DMAPath_UnSet((int)vioc->rdma.id);
				tca_map_convter_swreset(vioc->map_converter.id);
				ret = VIOC_CONFIG_DMAPath_Set(vioc->rdma.id, vioc->map_converter.id);
			}
		} else {
			#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			tca_map_convter_swreset(VIOC_MC0);
			if (VIOC_CONFIG_MCPath(vioc->wmix.id, VIOC_MC0) < 0) {
				(void)pr_err("[ERR][VOUT] %s[%d]: HW Decompresser can not be connected on %s\n",
						__func__, __LINE__, vout->vdev->name);
				return;
			}
			#endif
		}
		/* update map_converter info */
		vioc->map_converter.mapConv_info.m_uiLumaStride = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_LUMA_STRIDE];
		vioc->map_converter.mapConv_info.m_uiChromaStride = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_CHROMA_STRIDE];
		vioc->map_converter.mapConv_info.m_uiLumaBitDepth = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_LUMA_BIT_DEPTH];
		vioc->map_converter.mapConv_info.m_uiChromaBitDepth = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_CHROMA_BIT_DEPTH];
		vioc->map_converter.mapConv_info.m_uiFrameEndian = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRMAE_ENDIAN];
		vioc->map_converter.mapConv_info.m_Reserved[0] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_RESERVED0];

		vioc->map_converter.mapConv_info.m_CompressedY[0] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRAME_BASE_Y0];
		vioc->map_converter.mapConv_info.m_CompressedY[1] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRAME_BASE_Y1];
		vioc->map_converter.mapConv_info.m_CompressedCb[0] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRAME_BASE_C0];
		vioc->map_converter.mapConv_info.m_CompressedCb[1] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRAME_BASE_C1];

		vioc->map_converter.mapConv_info.m_FbcYOffsetAddr[0] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_OFFSET_BASE_Y0];
		vioc->map_converter.mapConv_info.m_FbcYOffsetAddr[1] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_OFFSET_BASE_Y1];
		vioc->map_converter.mapConv_info.m_FbcCOffsetAddr[0] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_OFFSET_BASE_C0];
		vioc->map_converter.mapConv_info.m_FbcCOffsetAddr[1] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_OFFSET_BASE_C1];

//		dprintk(" m_uiLumaStride %d m_uiChromaStride %d\n m_uiLumaBitDepth %d m_uiChromaBitDepth %d m_uiFrameEndian %d\n m_CompressedY[0] 0x%08x m_CompressedCb[0] 0x%08x m_CompressedY[1] 0x%08x m_CompressedCb[1] 0x%08x\n m_FbcYOffsetAddr[0] 0x%08x m_FbcCOffsetAddr[0] 0x%08x m_FbcYOffsetAddr[1] 0x%08x m_FbcCOffsetAddr[1] 0x%08x\n",
//			vioc->map_converter.mapConv_info.m_uiLumaStride, vioc->map_converter.mapConv_info.m_uiChromaStride,
//			vioc->map_converter.mapConv_info.m_uiLumaBitDepth, vioc->map_converter.mapConv_info.m_uiChromaBitDepth, vioc->map_converter.mapConv_info.m_uiFrameEndian,
//			vioc->map_converter.mapConv_info.m_CompressedY[0], vioc->map_converter.mapConv_info.m_CompressedCb[0],
//			vioc->map_converter.mapConv_info.m_CompressedY[1], vioc->map_converter.mapConv_info.m_CompressedCb[1],
//			vioc->map_converter.mapConv_info.m_FbcYOffsetAddr[0], vioc->map_converter.mapConv_info.m_FbcCOffsetAddr[0],
//			vioc->map_converter.mapConv_info.m_FbcYOffsetAddr[1], vioc->map_converter.mapConv_info.m_FbcCOffsetAddr[1]);

//		dprintk("%s map converter: size: %dx%d\n", __func__, vout->src_pix.width, vout->src_pix.height);

		if (VIOC_CONFIG_GetScaler_PluginToRDMA(vioc->rdma.id) != u32_to_s32(vioc->sc.id)) {
			scaler_setup(vout);
		}

		#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
		if (VIOC_CONFIG_DV_GET_EDR_PATH())
			bY2R = OFF;
		#elif defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
		bY2R = OFF;
		#endif

		VIOC_SC_SetUpdate(vioc->sc.addr);
		#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		tca_map_convter_driver_set(VIOC_MC0, vout->src_pix.width, vout->src_pix.height,
				s32_to_u32(vout->crop_src.left), s32_to_u32(vout->crop_src.top),
				vout->crop_src.width, vout->crop_src.height,
				bY2R, &vioc->map_converter.mapConv_info);
		tca_map_convter_onoff(VIOC_MC0, 1, 0);
		#else
		tca_map_convter_driver_set(vioc->map_converter.id, vout->src_pix.width, vout->src_pix.height,
				s32_to_u32(vout->crop_src.left), s32_to_u32(vout->crop_src.top), vout->crop_src.width, vout->crop_src.height,
				bY2R, &vioc->map_converter.mapConv_info);
		tca_map_convter_onoff(vioc->map_converter.id, 1, 0);
		#endif
		#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
		if (VIOC_CONFIG_DV_GET_EDR_PATH())
			vout_onthefly_dv_update(vout, buf);
		#endif
		#endif
	} else if (buf->m.planes[MPLANE_VID].reserved[VID_CONVERTER_EN] == 2U) {
		#ifdef CONFIG_VIOC_DTRC_DECOMP
		if (VIOC_CONFIG_DMAPath_Support()) {
			if (VIOC_CONFIG_DMAPath_Select(vioc->rdma.id) != vioc->dtrc.id) {
				VIOC_RDMA_SetImageDisable(vioc->rdma.addr);

				if ((vout->deinterlace == VOUT_DEINTL_VIQE_3D) || (vout->deinterlace == VOUT_DEINTL_VIQE_2D)) {
					ret = VIOC_CONFIG_PlugOut(vioc->viqe.id);
					VIOC_CONFIG_SWReset(vioc->viqe.id, VIOC_CONFIG_RESET);
					VIOC_CONFIG_SWReset(vioc->viqe.id, VIOC_CONFIG_CLEAR);
				} else if (vout->deinterlace == VOUT_DEINTL_S) {
					ret = VIOC_CONFIG_PlugOut(vioc->deintl_s.id);
					VIOC_CONFIG_SWReset(vioc->deintl_s.id, VIOC_CONFIG_RESET);
					VIOC_CONFIG_SWReset(vioc->deintl_s.id, VIOC_CONFIG_CLEAR);
				}

				ret = VIOC_CONFIG_PlugOut(vioc->sc.id);
				VIOC_CONFIG_SWReset(vioc->sc.id, VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(vioc->sc.id, VIOC_CONFIG_CLEAR);

				dprintk("____________________________________dtrc converter connect to RDMA%d\n", VIOC_RDMA_03);
				(void)VIOC_CONFIG_DMAPath_UnSet(vioc->rdma.id);
				tca_dtrc_convter_swreset(vioc->dtrc.id);
				ret = VIOC_CONFIG_DMAPath_Set(vioc->rdma.id, vioc->dtrc.id);
			}

			/* update dtrc info */
			vioc->dtrc.dtrcConv_info.m_CompressedY[0] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_FRAME_BASE_Y0];
			vioc->dtrc.dtrcConv_info.m_CompressedY[1] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_FRAME_BASE_Y1];
			vioc->dtrc.dtrcConv_info.m_CompressedCb[0] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_FRAME_BASE_C0];
			vioc->dtrc.dtrcConv_info.m_CompressedCb[1] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_FRAME_BASE_C1];
			vioc->dtrc.dtrcConv_info.m_CompressionTableLuma[0] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_TABLE_LUMA0];
			vioc->dtrc.dtrcConv_info.m_CompressionTableLuma[1] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_TABLE_LUMA1];
			vioc->dtrc.dtrcConv_info.m_CompressionTableChroma[0] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_TABLE_CHROMA0];
			vioc->dtrc.dtrcConv_info.m_CompressionTableChroma[1] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_TABLE_CHROMA1];
			vioc->dtrc.dtrcConv_info.m_iCompressionTableLumaSize = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_LUMA_TABLE_SIZE];
			vioc->dtrc.dtrcConv_info.m_iCompressionTableChromaSize = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_CHROMA_TABLE_SIZE];
			vioc->dtrc.dtrcConv_info.m_iBitDepthLuma = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_LUMA_BIT_DEPTH];
			vioc->dtrc.dtrcConv_info.m_iBitDepthChroma = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_CHROMA_BIT_DEPTH];
			vioc->dtrc.dtrcConv_info.m_iHeight = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_FRAME_HEIGHT];
			vioc->dtrc.dtrcConv_info.m_iWidth = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_FRAME_WIDTH];
			vioc->dtrc.dtrcConv_info.m_iPicStride = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_FRAME_STRIDE];

	//		dprintk("Size:%dx%d Stride:%d Bitdepth:%d/%d\n", vioc->dtrc.dtrcConv_info.m_iWidth, vioc->dtrc.dtrcConv_info.m_iHeight, vioc->dtrc.dtrcConv_info.m_iPicStride, vioc->dtrc.dtrcConv_info.m_iBitDepthLuma, vioc->dtrc.dtrcConv_info.m_iBitDepthChroma);
	//		dprintk("m_CompressedY 0x%08x 0x%08x\n m_CompressedCb 0x%08x 0x%08x\n m_CompressionTableLuma 0x%08x 0x%08x\n m_CompressionTableChroma 0x%08x 0x%08x\n",
	//			vioc->dtrc.dtrcConv_info.m_CompressedY[0], vioc->dtrc.dtrcConv_info.m_CompressedY[1],
	//			vioc->dtrc.dtrcConv_info.m_CompressedCb[0], vioc->dtrc.dtrcConv_info.m_CompressedCb[1],
	//			vioc->dtrc.dtrcConv_info.m_CompressionTableLuma[0], vioc->dtrc.dtrcConv_info.m_CompressionTableLuma[1],
	//			vioc->dtrc.dtrcConv_info.m_CompressionTableChroma[0], vioc->dtrc.dtrcConv_info.m_CompressionTableChroma[1]);

			if (VIOC_CONFIG_GetScaler_PluginToRDMA(vioc->rdma.id) != vioc->sc.id)
				scaler_setup(vout);

			#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
			if (VIOC_CONFIG_DV_GET_EDR_PATH())
				bY2R = OFF;
			#elif defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
			bY2R = OFF;
			#endif

			VIOC_SC_SetUpdate(vioc->sc.addr);
			tca_dtrc_convter_driver_set(vioc->dtrc.id, vout->crop_src.width, vout->crop_src.height,
					vout->crop_src.left, vout->crop_src.top, bY2R, &vioc->dtrc.dtrcConv_info);
			tca_dtrc_convter_onoff(vioc->dtrc.id, ON, 0);
			#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
			if (VIOC_CONFIG_DV_GET_EDR_PATH())
				vout_onthefly_dv_update(vout, buf);
			#endif
		} else
			(void)pr_err("[ERR][VOUT] Not support for DTRC on this chipset type!!\n");
		#endif
	} else {
		#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC_DECOMP)
		if (VIOC_CONFIG_DMAPath_Support() != 0) {
			unsigned int component_num = s32_to_u32(VIOC_CONFIG_DMAPath_Select(vioc->rdma.id));

			(void)VIOC_CONFIG_DMAPath_UnSet(u32_to_s32(component_num));

			// It is default path selection(VRDMA)
			ret = VIOC_CONFIG_DMAPath_Set(vioc->rdma.id, vioc->rdma.id);
		} else {
			#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			ret = VIOC_CONFIG_MCPath(vioc->wmix.id, vioc->rdma.id);
			#endif
		}
		#endif

		/* dec output format */
		if (vout->memory == V4L2_MEMORY_USERPTR) {
			vout_check_format(&vioc->rdma, buf->m.planes[MPLANE_VID].reserved[VID_MJPEG_FORMAT]);
		}

		#ifdef CONFIG_VIOC_10BIT
		if (buf->flags & V4L2_BUFFER_FLAG_YUV_10BIT_MODE0)
			VIOC_RDMA_SetDataFormat(vioc->rdma.addr, 0x1, 0x1);	/* YUV 10bit support(16bit format type) */
		else if (buf->flags & V4L2_BUFFER_FLAG_YUV_10BIT_MODE1) {
			VIOC_RDMA_SetDataFormat(vioc->rdma.addr, 0x3, 0x0);	/* YUV 10bit support(10bit format type) */
			VIOC_RDMA_SetImageOffset(vioc->rdma.addr, vioc->rdma.fmt, (vioc->rdma.width*125)/100);
		} else
			VIOC_RDMA_SetDataFormat(vioc->rdma.addr, 0x0, 0x0);	/* YUV 8bit support */
		#endif

		/*
		 * Onthefly Scaler
		 */
		if (res_change != OFF) {
			if ((vout->src_pix.field == (unsigned int)V4L2_FIELD_INTERLACED_TB)
				|| (vout->src_pix.field == (unsigned int)V4L2_FIELD_INTERLACED_BT)
				|| (vout->src_pix.field == (unsigned int)V4L2_FIELD_INTERLACED)) {
				vout->frame_count = 0;
			}

			if ((vioc->wmix.width == vioc->rdma.width) && (vioc->wmix.height == vioc->rdma.height)) {
				dprintk("BYPASS         src(%dx%d) -> dst(%dx%d)\n", vioc->rdma.width, vioc->rdma.height, vioc->wmix.width, vioc->wmix.height);
				VIOC_SC_SetBypass(vioc->sc.addr, ON);
				if (VIOC_CONFIG_GetScaler_PluginToRDMA(vioc->rdma.id) != u32_to_s32(vioc->sc.id)) {
					ret = VIOC_CONFIG_PlugIn(vioc->sc.id, vioc->rdma.id); // plugin position in scaler
				}
				VIOC_SC_SetUpdate(vioc->sc.addr);
			} else {
				dprintk("SCALING        src(%dx%d) -> dst(%dx%d)\n", vioc->rdma.width, vioc->rdma.height, vioc->wmix.width, vioc->wmix.height);
				VIOC_SC_SetBypass(vioc->sc.addr, OFF);
				VIOC_SC_SetDstSize(vioc->sc.addr, vioc->wmix.width, vioc->wmix.height); // set destination size in scaler
				VIOC_SC_SetOutSize(vioc->sc.addr, vioc->wmix.width, vioc->wmix.height); // set output size in scaler
				if (VIOC_CONFIG_GetScaler_PluginToRDMA(vioc->rdma.id) != u32_to_s32(vioc->sc.id)) {
					ret = VIOC_CONFIG_PlugIn(vioc->sc.id, vioc->rdma.id); // plugin position in scaler
				}
				VIOC_SC_SetUpdate(vioc->sc.addr);
			}
		}

		#ifdef CONFIG_TCC_VIOCMG
		if (vout->deinterlace == VOUT_DEINTL_VIQE_3D || vout->deinterlace == VOUT_DEINTL_VIQE_2D) {
			if (viocmg_lock_viqe(VIOCMG_CALLERID_VOUT) < 0) {
				// VIQE may be used for rear camera !!
				viqe_locked = 0;
				vout->last_cleared_buffer = buf;
				vout->display_done = ON;
				if (vout->src_pix.field == V4L2_FIELD_INTERLACED_TB
						|| vout->src_pix.field == V4L2_FIELD_INTERLACED_BT
						|| vout->src_pix.field == V4L2_FIELD_INTERLACED)
					vout->firstFieldFlag = 0;

				if (vioc->rdma.addr->uCTRL.nREG & Hw28) {
					VIOC_RDMA_SetImageDisable(vioc->rdma.addr);
					dprintk("RDMA%d is disabled by force\n", vioc->rdma.id);
				}
				return;
			} else {
				VIOC_PlugInOutCheck plugin_state;

				VIOC_CONFIG_Device_PlugState(vioc->viqe.id, &plugin_state);
				if (!plugin_state.enable || plugin_state.connect_statue != VIOC_PATH_CONNECTED) {
					if (deintl_viqe_setup(vout, vout->deinterlace, 1) < 0)
						(void)pr_err("[ERR][VOUT] failed VIQE%d reconnection\n", vioc->viqe.id);
					dprintk("reconnect VIQE%d\n", vioc->viqe.id);
					vout->frame_count = 0;
					force_process = 1;
				}
				viqe_locked = 1;
			}
		}
		#endif

		#ifdef CONFIG_TCC_VIOCMG
		if (res_change || force_process || (vout->previous_field != vout->src_pix.field))
		#else
		if ((res_change != 0U) || (vout->previous_field != (enum v4l2_field)vout->src_pix.field))
		#endif
		{
			vout->previous_field = (enum v4l2_field)vout->src_pix.field;
			if ((vout->src_pix.field == (unsigned int)V4L2_FIELD_INTERLACED_TB)
				|| (vout->src_pix.field == (unsigned int)V4L2_FIELD_INTERLACED_BT)
				|| (vout->src_pix.field == (unsigned int)V4L2_FIELD_INTERLACED)) {
				switch (vout->deinterlace) {
				case VOUT_DEINTL_VIQE_3D:
				case VOUT_DEINTL_VIQE_2D:
					if (res_change != OFF) {
						ret = deintl_viqe_setup(vout, vout->deinterlace, 0);
					} else {
						VIOC_VIQE_SetDeintlMode(vioc->viqe.addr, VIOC_VIQE_DEINTL_MODE_2D);
						VIOC_VIQE_SetControlMode(vioc->viqe.addr, OFF, OFF, OFF, OFF, ON);
					}

					if (vout->src_pix.colorspace == (unsigned int)V4L2_COLORSPACE_JPEG) {
						VIOC_RDMA_SetImageY2REnable(vioc->rdma.addr, 0);	// force disable y2r
					}
					break;
				case VOUT_DEINTL_S:
					ret = deintl_s_setup(vout);
					break;
				default:
					/* prevent KCS */
					break;
				}
				VIOC_RDMA_SetImageIntl(vioc->rdma.addr, 1);
				dprintk("enable deintl(%d)\n", vout->deinterlace);
			} else {
				switch (vout->deinterlace) {
				case VOUT_DEINTL_VIQE_3D:
				case VOUT_DEINTL_VIQE_2D:
					VIOC_VIQE_SetDeintlMode(vioc->viqe.addr, VIOC_VIQE_DEINTL_MODE_BYPASS);
					VIOC_VIQE_SetControlMode(vioc->viqe.addr, OFF, OFF, OFF, OFF, OFF);
					vout->frame_count = 0;

					if (vout->src_pix.colorspace == (unsigned int)V4L2_COLORSPACE_JPEG) {
						#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
						VIOC_RDMA_SetImageY2REnable(vioc->rdma.addr, 0); // disable y2r @stomlee
						#else
						VIOC_RDMA_SetImageY2REnable(vioc->rdma.addr, 1); // force enable y2r
						#endif
					}
					break;
				case VOUT_DEINTL_S:
					ret = VIOC_CONFIG_PlugOut(vioc->deintl_s.id);
					break;
				default:
					/* prevent KCS */
					break;
				}
				VIOC_RDMA_SetImageIntl(vioc->rdma.addr, 0);
				dprintk("disable deintl(%d)\n", vout->deinterlace);
			}
		}

		#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
		if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
			if (vout->src_pix.field == V4L2_FIELD_INTERLACED_TB
				|| vout->src_pix.field == V4L2_FIELD_INTERLACED_BT
				|| vout->src_pix.field == V4L2_FIELD_INTERLACED) {
				switch (vout->deinterlace) {
				case VOUT_DEINTL_VIQE_3D:
				case VOUT_DEINTL_VIQE_2D:
						VIOC_VIQE_SetControlMode(vioc->viqe.addr, OFF, OFF, OFF, OFF, ON);
					break;
				default:
					/* prevent KCS */
					break;
				}
			}
		}
		#endif

		VIOC_RDMA_SetImageBfield(vioc->rdma.addr, vioc->rdma.bf);
		VIOC_RDMA_SetImageBase(vioc->rdma.addr, base0, base1, base2);
		VIOC_SC_SetUpdate(vioc->sc.addr);
		#ifdef CONFIG_VOUT_USE_VSYNC_INT
		if (vout->clearFrameMode == OFF)
		#endif
		{
			VIOC_RDMA_SetImageEnable(vioc->rdma.addr);
			#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
			if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
				VIOC_RDMA_SetImageUVIEnable(vioc->rdma.addr, 0);
				VIOC_RDMA_SetImageY2REnable(vioc->rdma.addr, 0);
				VIOC_RDMA_SetImageR2YEnable(vioc->rdma.addr, 0);
				VIOC_RDMA_SetImageIntl(vioc->rdma.addr, 0);

				vout_onthefly_dv_update(vout, buf);
			}
			#endif
		}
	}

	if ((vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_TB)
		|| (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_BT)
		|| (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED)) {
		dtprintk("%s-field done\n", vioc->rdma.bf ? "bot" : "top");

		if ((vout->deinterlace == VOUT_DEINTL_VIQE_3D) && (vout->frame_count != -1)) {
			if (vout->frame_count >= 3) {
				VIOC_VIQE_SetDeintlMode(vioc->viqe.addr, VIOC_VIQE_DEINTL_MODE_3D);
				vout->frame_count = -1;
				dprintk("Change VIQE_3D mode\n");

				/* enhancement de-interlace */
				{
					unsigned int deintl_judder_cnt = ((vioc->rdma.width + 64U) / 64U) - 1U;

					VIOC_VIQE_SetDeintlJudderCnt(vioc->viqe.addr, deintl_judder_cnt);
				}
			} else {
				VIOC_VIQE_SetDeintlMode(vioc->viqe.addr, VIOC_VIQE_DEINTL_MODE_2D);
				vout->frame_count++;
			}
		}
		vout->last_cleared_buffer = buf;

		#ifdef CONFIG_VOUT_USE_VSYNC_INT
		if (vout->clearFrameMode) {
			vout->firstFieldFlag = 0;
			vout_clear_buffer(vout, vout->last_cleared_buffer);

			/* update DD flag */
			vout->display_done = OFF;
		}
		#endif
	} else {
		// update DD flag
		vout->display_done = 1;
		vout->last_cleared_buffer = buf;

		#ifdef CONFIG_VOUT_USE_VSYNC_INT
		if (vout->clearFrameMode) {
			vout_clear_buffer(vout, vout->last_cleared_buffer);

			/* update DD flag */
			vout->display_done = OFF;
		}
		#endif
	}

	#ifdef CONFIG_TCC_VIOCMG
	if (viqe_locked)
		viocmg_free_viqe(VIOCMG_CALLERID_VOUT);
	#endif

	#ifdef CONFIG_VOUT_DISPLAY_LASTFRAME
	vout_video_post_process(vout);
	#endif
}

void vout_m2m_display_update(struct tcc_vout_device *vout, struct v4l2_buffer *buf)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int base0 = 0, base1 = 0, base2 = 0;
	unsigned int index = 0;
	unsigned int res_change = OFF;
	unsigned int tmp;
	enum v4l2_colorspace color;

	#ifdef CONFIG_TCC_VIOCMG
	unsigned int viqe_locked = 0;
	unsigned int force_process = 0;
	#endif

	#ifdef CONFIG_VOUT_KEEP_VIDEO_LAYER
	unsigned int previous_deinterlace = 0;
	#endif

	#ifndef CONFIG_VOUT_USE_VSYNC_INT
	/* This is the code for only m2m path without vsync */
	if (vout->deintl_force == 0) {
		vout->src_pix.field = buf->field;
	}

	if (vout->firstFieldFlag == 0) {		// first field
		if ((vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_TB)
			|| (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_BT)
			|| (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED)) {
			vout->firstFieldFlag++;
		}
	}
	#endif

	#ifdef CONFIG_VOUT_USE_SUB_PLANE
	if (buf->m.planes[MPLANE_VID].reserved[VID_NUM] == 2) {
		(void)memcpy(&vioc->subplane_alpha, buf->m.planes[MPLANE_SUB].reserved, sizeof(struct vioc_alpha));
		vioc->m2m_subplane_rdma.img.base0 = buf->m.planes[MPLANE_SUB].m.mem_offset;
		vout_subplane_m2m_qbuf(vout, &vioc->subplane_alpha);
	} else {
		if (vioc->subplane_enable) {
			VIOC_RDMA_SetImageDisableNW(vioc->m2m_subplane_rdma.addr);
			vioc->subplane_enable = OFF;
		}
	}
	#endif

	/* get base address */
	if (vout->memory  == V4L2_MEMORY_USERPTR) {
		if (buf->timecode.type != STILL_IMGAGE) {
			base0 = u64_to_u32(buf->m.planes[MPLANE_VID].m.userptr);
			base1 = u64_to_u32(buf->m.planes[MPLANE_VID].reserved[VID_BASE1]);
			base2 = u64_to_u32(buf->m.planes[MPLANE_VID].reserved[VID_BASE2]);
		} else {
			base0 = u64_to_u32(buf->m.planes[MPLANE_VID].m.userptr);
			base1 = 0;
			base2 = 0;

			/*
			 * If the input is YUV format.
			 */
			if (vout->pfmt != (int)TCC_PFMT_RGB) {
				/*
				 * Re-store src_pix.height from "The VIQE need 4-line align"
				 */
				if (vioc->m2m_rdma.height != vout->src_pix.height) {
					vioc->m2m_rdma.height = vout->src_pix.height;
					m2m_rdma_setup(&vioc->m2m_rdma);
				}

				(void)tcc_get_base_address_of_image(
					vout->src_pix.pixelformat, base0,
					vout->src_pix.width, vout->src_pix.height,
					&base0, &base1, &base2);
			}
		}
	} else if (vout->memory == V4L2_MEMORY_MMAP) {
		base0 = vout->qbufs[buf->index].img_base0;
		base1 = vout->qbufs[buf->index].img_base1;
		base2 = vout->qbufs[buf->index].img_base2;
	} else {
		(void)pr_err("[ERR][VOUT] invalid qbuf v4l2_memory\n");
	}

	if (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_BT) {
		vioc->m2m_rdma.bf = 1;
	} else {
		vioc->m2m_rdma.bf = 0;
	}

	/* To prevent frequent switching of the scan type */
	if (vout->first_frame != 0) {
		vout->first_field = (enum v4l2_field)vout->src_pix.field;
		vout->first_frame = 0;
		dprintk("first_field(%s)\n", v4l2_field_names[vout->first_field]);
	}

	tmp = vout->deintl_nr_bufs_count++;
	index = (tmp % vout->deintl_nr_bufs);
	if (vout->deintl_nr_bufs_count == vout->deintl_nr_bufs) {
		vout->deintl_nr_bufs_count = 0;
	}

	/*
	 *	Change source info
	 */
	if (((vout->src_pix.width != buf->m.planes[MPLANE_VID].reserved[VID_WIDTH])
		|| (vout->src_pix.height != buf->m.planes[MPLANE_VID].reserved[VID_HEIGHT]))
		&& ((buf->m.planes[MPLANE_VID].reserved[VID_WIDTH] != 0U) && (buf->m.planes[MPLANE_VID].reserved[VID_HEIGHT] != 0U))) {
		dprintk("changing source (%dx%d)->(%dx%d)\n",
			vout->src_pix.width, vout->src_pix.height,
			buf->m.planes[MPLANE_VID].reserved[VID_WIDTH], buf->m.planes[MPLANE_VID].reserved[VID_HEIGHT]);

		vout->src_pix.width = buf->m.planes[MPLANE_VID].reserved[VID_WIDTH];
		vout->src_pix.height = buf->m.planes[MPLANE_VID].reserved[VID_HEIGHT];

		color = (enum v4l2_colorspace)u32_to_s32(vout->src_pix.colorspace);
		vout->src_pix.bytesperline = vout->src_pix.width * (unsigned int)tcc_vout_try_bpp(vout->src_pix.pixelformat, &color);

		/* reinit crop_src */
		vout->crop_src.left = 0;
		vout->crop_src.top = 0;
		vout->crop_src.width = vout->src_pix.width;
		vout->crop_src.height = vout->src_pix.height;

		switch (tcc_vout_try_pix(vout->src_pix.pixelformat)) {
		case (int)TCC_PFMT_YUV422:
			vout->src_pix.sizeimage = vout->src_pix.width * vout->src_pix.height * 2U;
			break;
		case (int)TCC_PFMT_YUV420:
			vout->src_pix.sizeimage = vout->src_pix.width * vout->src_pix.height * 3U / 2U;
			break;
		case (int)TCC_PFMT_RGB:
		default:
			vout->src_pix.sizeimage = vout->src_pix.bytesperline * vout->src_pix.height;
			break;
		}
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		vout->src_pix.sizeimage = PAGE_ALIGN(vout->src_pix.sizeimage);

		vioc->m2m_rdma.width = vout->src_pix.width;
		if ((vout->src_pix.height % 4U) != 0U) {
			vioc->m2m_rdma.height = ROUND_UP_4(vout->src_pix.height);
			dprintk(" 4-line align: %d -> %d\n", vout->src_pix.height, vioc->m2m_rdma.height);
		} else {
			vioc->m2m_rdma.height = vout->src_pix.height;
		}
		vioc->m2m_rdma.y_stride = (buf->m.planes[MPLANE_VID].bytesused != 0U) ? (buf->m.planes[MPLANE_VID].bytesused) : (vout->src_pix.bytesperline & 0x0000FFFFU);
		VIOC_RDMA_SetImageSize(vioc->m2m_rdma.addr, vioc->m2m_rdma.width, vioc->m2m_rdma.height);
		VIOC_RDMA_SetImageOffset(vioc->m2m_rdma.addr, vioc->m2m_rdma.fmt, vioc->m2m_rdma.y_stride);

		vioc->m2m_wmix.width = vioc->m2m_rdma.width;
		vioc->m2m_wmix.height = vioc->m2m_rdma.height;
		VIOC_WMIX_SetSize(vioc->m2m_wmix.addr, vioc->m2m_wmix.width, vioc->m2m_wmix.height);

		res_change = ON;
	}

	/*
	 *	Change crop info
	 */
	if (((vout->crop_src.left != u32_to_s32(buf->m.planes[MPLANE_VID].reserved[VID_CROP_LEFT]))
		|| (vout->crop_src.top != u32_to_s32(buf->m.planes[MPLANE_VID].reserved[VID_CROP_TOP]))
		|| (vout->crop_src.width != buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH])
		|| (vout->crop_src.height != buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT]))
		&& ( (buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH] != 0U) && (buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT] != 0U))
		&& ((vout->src_pix.width >= buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH]) && (vout->src_pix.height >= buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT]))) {
		dprintk("changing crop (%d,%d)(%dx%d)->(%d,%d)(%dx%d)\n",
			vout->crop_src.left, vout->crop_src.top, vout->crop_src.width, vout->crop_src.height,
			buf->m.planes[MPLANE_VID].reserved[VID_CROP_LEFT], buf->m.planes[MPLANE_VID].reserved[VID_CROP_TOP],
			buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH], buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT]);

		check_wrap(buf->m.planes[MPLANE_VID].reserved[VID_CROP_TOP]);
		vout->crop_src.left = u32_to_s32(ROUND_UP_2(buf->m.planes[MPLANE_VID].reserved[VID_CROP_LEFT]));
		vout->crop_src.top = u32_to_s32(ROUND_UP_2(buf->m.planes[MPLANE_VID].reserved[VID_CROP_TOP]));
		vout->crop_src.width = ROUND_UP_2(buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH]);
		vout->crop_src.height = ROUND_UP_2(buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT]);

		(void)tcc_get_base_address(vioc->m2m_rdma.fmt, base0,
			(vioc->m2m_rdma.y_stride != 0U) ? vioc->m2m_rdma.y_stride : vioc->m2m_rdma.width,
			vioc->m2m_rdma.height,
			(unsigned int)vout->crop_src.left, (unsigned int)vout->crop_src.top,
			&base0, &base1, &base2);

		vioc->m2m_rdma.width = vout->crop_src.width;
		if ((vout->crop_src.height % 4U) != 0U) {
			vioc->m2m_rdma.height = ROUND_UP_4(vout->crop_src.height);
			dprintk(" 4-line align: %d -> %d\n", vout->crop_src.height, vioc->m2m_rdma.height);
		} else {
			vioc->m2m_rdma.height = vout->crop_src.height;
		}
		vioc->m2m_rdma.y_stride = (buf->m.planes[MPLANE_VID].bytesused != 0U) ? (buf->m.planes[MPLANE_VID].bytesused) : (vout->src_pix.bytesperline & 0x0000FFFFU);
		VIOC_RDMA_SetImageSize(vioc->m2m_rdma.addr, vioc->m2m_rdma.width, vioc->m2m_rdma.height);
		VIOC_RDMA_SetImageOffset(vioc->m2m_rdma.addr, vioc->m2m_rdma.fmt, vioc->m2m_rdma.y_stride);

		vioc->m2m_wmix.width = vioc->m2m_rdma.width;
		vioc->m2m_wmix.height = vioc->m2m_rdma.height;
		VIOC_WMIX_SetSize(vioc->m2m_wmix.addr, vioc->m2m_wmix.width, vioc->m2m_wmix.height);

		res_change = ON;
	} else if ((vout->src_pix.width != vout->crop_src.width) || (vout->src_pix.height != vout->crop_src.height)) {
		(void)tcc_get_base_address(vioc->m2m_rdma.fmt, base0,
			(vioc->m2m_rdma.y_stride != 0U) ? vioc->m2m_rdma.y_stride : vioc->m2m_rdma.width,
			vioc->m2m_rdma.height,
			(unsigned int)vout->crop_src.left, (unsigned int)vout->crop_src.top,
			&base0, &base1, &base2);
	} else {
		/* prevent KCS */
	}

	if (res_change != OFF) {
		if ((vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_TB)
			|| (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_BT)
			|| (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED)) {
			vout->frame_count = 0;
		}

		VIOC_WMIX_SetUpdate(vioc->m2m_wmix.addr);

		if ((vioc->m2m_wmix.width == vioc->m2m_wdma.width) && (vioc->m2m_wmix.height == vioc->m2m_wdma.height)) {
			dprintk("BYPASS         src(%dx%d) -> dst(%dx%d)\n", vioc->m2m_wmix.width, vioc->m2m_wmix.height, vioc->m2m_wdma.width, vioc->m2m_wdma.height);
			VIOC_SC_SetBypass(vioc->sc.addr, ON);
			VIOC_SC_SetUpdate(vioc->sc.addr);
		} else {
			dprintk("SCALING        src(%dx%d) -> dst(%dx%d)\n", vioc->m2m_wmix.width, vioc->m2m_wmix.height, vioc->m2m_wdma.width, vioc->m2m_wdma.height);
			VIOC_SC_SetBypass(vioc->sc.addr, OFF);
			VIOC_SC_SetDstSize(vioc->sc.addr, vioc->m2m_wdma.width, vioc->m2m_wdma.height);	// set destination size in scaler
			VIOC_SC_SetOutSize(vioc->sc.addr, vioc->m2m_wdma.width, vioc->m2m_wdma.height);	// set output size in scaler
			VIOC_SC_SetUpdate(vioc->sc.addr);
		}

#if defined(CONFIG_TCC_DUAL_DISPLAY)
		vioc->m2m_wdma.width = vioc->m2m_rdma.width;
		vioc->m2m_wdma.height = vioc->m2m_rdma.height;
		VIOC_WDMA_SetImageSize(vioc->m2m_wdma.addr, vioc->m2m_wdma.width, vioc->m2m_wdma.height);
		VIOC_WDMA_SetImageOffset(vioc->m2m_wdma.addr, vioc->m2m_wdma.fmt, vioc->m2m_wdma.width);
		VIOC_WDMA_SetImageUpdate(vioc->m2m_wdma.addr);
#endif
	}

	/* rdma stride */
	if ((buf->m.planes[MPLANE_VID].bytesused != 0U) && (vioc->m2m_rdma.y_stride != buf->m.planes[MPLANE_VID].bytesused)) {
		dprintk("update rdma stride(%d -> %d)\n", vioc->m2m_rdma.y_stride, buf->m.planes[MPLANE_VID].bytesused);

		/*  change rdma stride */
		vioc->m2m_rdma.y_stride = buf->m.planes[MPLANE_VID].bytesused;
		vout->src_pix.bytesperline = buf->m.planes[MPLANE_VID].bytesused;
		VIOC_RDMA_SetImageOffset(vioc->m2m_rdma.addr, vioc->m2m_rdma.fmt, vioc->m2m_rdma.y_stride);
	}

	/* dec output format */
	if (vout->memory == V4L2_MEMORY_USERPTR) {
		vout_check_format(&vioc->m2m_rdma, buf->m.planes[MPLANE_VID].reserved[VID_MJPEG_FORMAT]);
	}

	#ifdef CONFIG_VIOC_10BIT
	if (buf->flags & V4L2_BUFFER_FLAG_YUV_10BIT_MODE0) {
		VIOC_RDMA_SetDataFormat(vioc->m2m_rdma.addr, 0x1, 0x1);	/* YUV 10bit support(16bit format type) */
	} else if (buf->flags & V4L2_BUFFER_FLAG_YUV_10BIT_MODE1) {
		VIOC_RDMA_SetDataFormat(vioc->m2m_rdma.addr, 0x3, 0x0);	/* YUV 10bit support(10bit format type) */
		VIOC_RDMA_SetImageOffset(vioc->m2m_rdma.addr, vioc->m2m_rdma.fmt, (vioc->m2m_rdma.width*125)/100);
	} else {
		VIOC_RDMA_SetDataFormat(vioc->m2m_rdma.addr, 0x0, 0x0);	/* YUV 8bit support */
	}
	#endif

	/* HDR support */
	#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	if (buf->m.planes[MPLANE_VID].reserved[VID_HDR_TC] == 16) {
		DRM_Packet_t drmparm;

		memset(&drmparm, 0x0, sizeof(DRM_Packet_t));

		drmparm.mInfoFrame.version = buf->m.planes[MPLANE_VID].reserved[VID_HDR_VERSION];
		drmparm.mInfoFrame.length = buf->m.planes[MPLANE_VID].reserved[VID_HDR_STRUCT_SIZE];

		drmparm.mDescriptor_type1.EOTF = 2;//buf->m.planes[MPLANE_VID].reserved[VID_HDR_EOTF];
		drmparm.mDescriptor_type1.Descriptor_ID = 0;//buf->m.planes[MPLANE_VID].reserved[VID_HDR_DESCRIPTOR_ID];
		drmparm.mDescriptor_type1.disp_primaries_x[0] = buf->m.planes[MPLANE_VID].reserved[VID_HDR_PRIMARY_X0];
		drmparm.mDescriptor_type1.disp_primaries_x[1] = buf->m.planes[MPLANE_VID].reserved[VID_HDR_PRIMARY_X1];
		drmparm.mDescriptor_type1.disp_primaries_x[2] = buf->m.planes[MPLANE_VID].reserved[VID_HDR_PRIMARY_X2];
		drmparm.mDescriptor_type1.disp_primaries_y[0] = buf->m.planes[MPLANE_VID].reserved[VID_HDR_PRIMARY_Y0];
		drmparm.mDescriptor_type1.disp_primaries_y[1] = buf->m.planes[MPLANE_VID].reserved[VID_HDR_PRIMARY_Y1];
		drmparm.mDescriptor_type1.disp_primaries_y[2] = buf->m.planes[MPLANE_VID].reserved[VID_HDR_PRIMARY_Y2];
		drmparm.mDescriptor_type1.white_point_x = buf->m.planes[MPLANE_VID].reserved[VID_HDR_WPOINT_X];
		drmparm.mDescriptor_type1.white_point_y = buf->m.planes[MPLANE_VID].reserved[VID_HDR_WPOINT_Y];
		drmparm.mDescriptor_type1.max_disp_mastering_luminance = buf->m.planes[MPLANE_VID].reserved[VID_HDR_MAX_LUMINANCE];
		drmparm.mDescriptor_type1.min_disp_mastering_luminance = buf->m.planes[MPLANE_VID].reserved[VID_HDR_MIN_LUMINANCE];
		drmparm.mDescriptor_type1.max_content_light_level = buf->m.planes[MPLANE_VID].reserved[VID_HDR_MAX_CONTENT];
		drmparm.mDescriptor_type1.max_frame_avr_light_level = buf->m.planes[MPLANE_VID].reserved[VID_HDR_MAX_PIC_AVR];

		if (memcmp(&gDRM_packet, &drmparm, sizeof(DRM_Packet_t))) {
			hdmi_set_drm(&drmparm);
			(void)memcpy(&gDRM_packet, &drmparm, sizeof(DRM_Packet_t));
		}
	}
	#endif

	/* rdma base address */
	vioc->m2m_rdma.img.base0 = base0;
	vioc->m2m_rdma.img.base1 = base1;
	vioc->m2m_rdma.img.base2 = base2;
	/* wdma base address */
	vioc->m2m_wdma.img.base0 = vout->deintl_bufs[index].img_base0;
	vioc->m2m_wdma.img.base1 = vout->deintl_bufs[index].img_base1;
	vioc->m2m_wdma.img.base2 = vout->deintl_bufs[index].img_base2;

#if defined(CONFIG_TCC_DUAL_DISPLAY)
	/* m2m wdma base address */
	vioc->m2m_dual_wdma[M2M_DUAL_0].img.base0 = vout->m2m_dual_bufs[index].img_base0;
	vioc->m2m_dual_wdma[M2M_DUAL_0].img.base1 = vout->m2m_dual_bufs[index].img_base1;
	vioc->m2m_dual_wdma[M2M_DUAL_0].img.base2 = vout->m2m_dual_bufs[index].img_base2;
	vioc->m2m_dual_wdma[M2M_DUAL_1].img.base0 = vout->m2m_dual_bufs_hdmi[index].img_base0;
	vioc->m2m_dual_wdma[M2M_DUAL_1].img.base1 = vout->m2m_dual_bufs_hdmi[index].img_base1;
	vioc->m2m_dual_wdma[M2M_DUAL_1].img.base2 = vout->m2m_dual_bufs_hdmi[index].img_base2;
#endif
	if (buf->m.planes[MPLANE_VID].reserved[VID_CONVERTER_EN] == 1U) {
		#ifdef CONFIG_VIOC_MAP_DECOMP
		VIOC_RDMA_SetImageDisable(vioc->m2m_rdma.addr);

		if ((vout->deinterlace == VOUT_DEINTL_VIQE_3D) || (vout->deinterlace == VOUT_DEINTL_VIQE_2D)) {
			(void)VIOC_CONFIG_PlugOut(vioc->viqe.id);
			VIOC_CONFIG_SWReset(vioc->viqe.id, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(vioc->viqe.id, VIOC_CONFIG_CLEAR);
		} else if (vout->deinterlace == VOUT_DEINTL_S) {
			(void)VIOC_CONFIG_PlugOut(vioc->deintl_s.id);
			VIOC_CONFIG_SWReset(vioc->deintl_s.id, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(vioc->deintl_s.id, VIOC_CONFIG_CLEAR);
		} else {
			/* prevent KCS */
		}

		(void)VIOC_CONFIG_PlugOut(vioc->sc.id);
		VIOC_CONFIG_SWReset(vioc->sc.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(vioc->sc.id, VIOC_CONFIG_CLEAR);
		if (VIOC_CONFIG_DMAPath_Support() != 0) {
			if (VIOC_CONFIG_DMAPath_Select(vioc->m2m_rdma.id) != u32_to_s32(vioc->map_converter.id)) {
				(void)VIOC_CONFIG_DMAPath_UnSet((int)vioc->m2m_rdma.id);
				tca_map_convter_swreset(vioc->map_converter.id);
				(void)VIOC_CONFIG_DMAPath_Set(vioc->m2m_rdma.id, vioc->map_converter.id);
			}
		} else {
			#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			tca_map_convter_swreset(VIOC_MC1);
			if (VIOC_CONFIG_MCPath(vioc->m2m_wmix.id, VIOC_MC1) < 0) {
				(void)pr_err("[ERR][VOUT] %s[%d]: HW Decompresser can not be connected on %s\n",
						__func__, __LINE__, vout->vdev->name);
				return;
			}
			#endif
		}

		/* update map_converter info */
		vioc->map_converter.mapConv_info.m_uiLumaStride = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_LUMA_STRIDE];
		vioc->map_converter.mapConv_info.m_uiChromaStride = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_CHROMA_STRIDE];
		vioc->map_converter.mapConv_info.m_uiLumaBitDepth = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_LUMA_BIT_DEPTH];
		vioc->map_converter.mapConv_info.m_uiChromaBitDepth = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_CHROMA_BIT_DEPTH];
		vioc->map_converter.mapConv_info.m_uiFrameEndian = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRMAE_ENDIAN];
		vioc->map_converter.mapConv_info.m_Reserved[0] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_RESERVED0];

		vioc->map_converter.mapConv_info.m_CompressedY[0] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRAME_BASE_Y0];
		vioc->map_converter.mapConv_info.m_CompressedY[1] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRAME_BASE_Y1];
		vioc->map_converter.mapConv_info.m_CompressedCb[0] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRAME_BASE_C0];
		vioc->map_converter.mapConv_info.m_CompressedCb[1] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_FRAME_BASE_C1];

		vioc->map_converter.mapConv_info.m_FbcYOffsetAddr[0] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_OFFSET_BASE_Y0];
		vioc->map_converter.mapConv_info.m_FbcYOffsetAddr[1] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_OFFSET_BASE_Y1];
		vioc->map_converter.mapConv_info.m_FbcCOffsetAddr[0] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_OFFSET_BASE_C0];
		vioc->map_converter.mapConv_info.m_FbcCOffsetAddr[1] = buf->m.planes[MPLANE_VID].reserved[VID_HEVC_OFFSET_BASE_C1];

//		dprintk(" m_uiLumaStride %d m_uiChromaStride %d\n m_uiLumaBitDepth %d m_uiChromaBitDepth %d m_uiFrameEndian %d\n
//			m_CompressedY[0] 0x%08x m_CompressedCb[0] 0x%08x m_CompressedY[1] 0x%08x m_CompressedCb[1] 0x%08x\n
//			m_FbcYOffsetAddr[0] 0x%08x m_FbcCOffsetAddr[0] 0x%08x m_FbcYOffsetAddr[1] 0x%08x m_FbcCOffsetAddr[1] 0x%08x\n",
//			vioc->map_converter.mapConv_info.m_uiLumaStride, vioc->map_converter.mapConv_info.m_uiChromaStride,
//			vioc->map_converter.mapConv_info.m_uiLumaBitDepth, vioc->map_converter.mapConv_info.m_uiChromaBitDepth,
//			vioc->map_converter.mapConv_info.m_uiFrameEndian,
//			vioc->map_converter.mapConv_info.m_CompressedY[0], vioc->map_converter.mapConv_info.m_CompressedCb[0],
//			vioc->map_converter.mapConv_info.m_CompressedY[1], vioc->map_converter.mapConv_info.m_CompressedCb[1],
//			vioc->map_converter.mapConv_info.m_FbcYOffsetAddr[0], vioc->map_converter.mapConv_info.m_FbcCOffsetAddr[0],
//			vioc->map_converter.mapConv_info.m_FbcYOffsetAddr[1], vioc->map_converter.mapConv_info.m_FbcCOffsetAddr[1]);
//		dprintk("%s map converter: size: %dx%d pos: (0,0)\n", __func__, vout->src_pix.width, vout->src_pix.height);

		#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		tca_map_convter_driver_set(VIOC_MC1, vout->src_pix.width, vout->src_pix.height,
				s32_to_u32(vout->crop_src.left), s32_to_u32(vout->crop_src.top), vout->crop_src.width, vout->crop_src.height, ON,
				&vioc->map_converter.mapConv_info);
		tca_map_convter_onoff(VIOC_MC1, 1, 0);
		#else
		tca_map_convter_driver_set(vioc->map_converter.id, vout->src_pix.width, vout->src_pix.height,
				s32_to_u32(vout->crop_src.left), s32_to_u32(vout->crop_src.top), vout->crop_src.width, vout->crop_src.height, ON,
				&vioc->map_converter.mapConv_info);
		tca_map_convter_onoff(vioc->map_converter.id, 1, 0);
		#endif
		scaler_setup(vout);

		/* update wdma base address */
		VIOC_WDMA_SetImageBase(vioc->m2m_wdma.addr,
			vioc->m2m_wdma.img.base0, vioc->m2m_wdma.img.base1, vioc->m2m_wdma.img.base2);
		vout_m2m_ctrl(vioc, 1);
		#endif
	} else if (buf->m.planes[MPLANE_VID].reserved[VID_CONVERTER_EN] == 2U) {
		#ifdef CONFIG_VIOC_DTRC_DECOMP
		if (VIOC_CONFIG_DMAPath_Support()) {
			if (VIOC_CONFIG_DMAPath_Select(vioc->m2m_rdma.id) != vioc->dtrc.id) {
				VIOC_RDMA_SetImageDisable(vioc->m2m_rdma.addr);

				if ((vout->deinterlace == VOUT_DEINTL_VIQE_3D) || (vout->deinterlace == VOUT_DEINTL_VIQE_2D)) {
					(void)VIOC_CONFIG_PlugOut(vioc->viqe.id);
					VIOC_CONFIG_SWReset(vioc->viqe.id, VIOC_CONFIG_RESET);
					VIOC_CONFIG_SWReset(vioc->viqe.id, VIOC_CONFIG_CLEAR);
				} else if (vout->deinterlace == VOUT_DEINTL_S) {
					(void)VIOC_CONFIG_PlugOut(vioc->deintl_s.id);
					VIOC_CONFIG_SWReset(vioc->deintl_s.id, VIOC_CONFIG_RESET);
					VIOC_CONFIG_SWReset(vioc->deintl_s.id, VIOC_CONFIG_CLEAR);
				}

				(void)VIOC_CONFIG_PlugOut(vioc->sc.id);
				VIOC_CONFIG_SWReset(vioc->sc.id, VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(vioc->sc.id, VIOC_CONFIG_CLEAR);

				dprintk("____________________________________dtrc converter connect to RDMA%d\n", vioc->m2m_rdma.id);

				(void)VIOC_CONFIG_DMAPath_UnSet(vioc->m2m_rdma.id);
				tca_dtrc_convter_swreset(vioc->dtrc.id);
				(void)VIOC_CONFIG_DMAPath_Set(vioc->m2m_rdma.id, vioc->dtrc.id);
			}

			/* update dtrc info */
			vioc->dtrc.dtrcConv_info.m_CompressedY[0] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_FRAME_BASE_Y0];
			vioc->dtrc.dtrcConv_info.m_CompressedY[1] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_FRAME_BASE_Y1];
			vioc->dtrc.dtrcConv_info.m_CompressedCb[0] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_FRAME_BASE_C0];
			vioc->dtrc.dtrcConv_info.m_CompressedCb[1] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_FRAME_BASE_C1];
			vioc->dtrc.dtrcConv_info.m_CompressionTableLuma[0] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_TABLE_LUMA0];
			vioc->dtrc.dtrcConv_info.m_CompressionTableLuma[1] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_TABLE_LUMA1];
			vioc->dtrc.dtrcConv_info.m_CompressionTableChroma[0] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_TABLE_CHROMA0];
			vioc->dtrc.dtrcConv_info.m_CompressionTableChroma[1] = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_TABLE_CHROMA1];
			vioc->dtrc.dtrcConv_info.m_iCompressionTableLumaSize = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_LUMA_TABLE_SIZE];
			vioc->dtrc.dtrcConv_info.m_iCompressionTableChromaSize = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_CHROMA_TABLE_SIZE];
			vioc->dtrc.dtrcConv_info.m_iBitDepthLuma = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_LUMA_BIT_DEPTH];
			vioc->dtrc.dtrcConv_info.m_iBitDepthChroma = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_CHROMA_BIT_DEPTH];
			vioc->dtrc.dtrcConv_info.m_iHeight = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_FRAME_HEIGHT];
			vioc->dtrc.dtrcConv_info.m_iWidth = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_FRAME_WIDTH];
			vioc->dtrc.dtrcConv_info.m_iPicStride = buf->m.planes[MPLANE_VID].reserved[VID_DTRC_FRAME_STRIDE];

//			dprintk("Size:%dx%d Stride:%d Bitdepth:%d/%d\n", vioc->dtrc.dtrcConv_info.m_iWidth, vioc->dtrc.dtrcConv_info.m_iHeight,
//					vioc->dtrc.dtrcConv_info.m_iPicStride, vioc->dtrc.dtrcConv_info.m_iBitDepthLuma, vioc->dtrc.dtrcConv_info.m_iBitDepthChroma);
//			dprintk("m_CompressedY 0x%08x 0x%08x\n m_CompressedCb 0x%08x 0x%08x\n
//					m_CompressionTableLuma 0x%08x 0x%08x\n m_CompressionTableChroma 0x%08x 0x%08x\n",
//					vioc->dtrc.dtrcConv_info.m_CompressedY[0], vioc->dtrc.dtrcConv_info.m_CompressedY[1],
//					vioc->dtrc.dtrcConv_info.m_CompressedCb[0], vioc->dtrc.dtrcConv_info.m_CompressedCb[1],
//					vioc->dtrc.dtrcConv_info.m_CompressionTableLuma[0], vioc->dtrc.dtrcConv_info.m_CompressionTableLuma[1],
//					vioc->dtrc.dtrcConv_info.m_CompressionTableChroma[0], vioc->dtrc.dtrcConv_info.m_CompressionTableChroma[1]);

			tca_dtrc_convter_driver_set(vioc->dtrc.id, vout->crop_src.width, vout->crop_src.height,
					vout->crop_src.left, vout->crop_src.top, ON, &vioc->dtrc.dtrcConv_info);
			tca_dtrc_convter_onoff(vioc->dtrc.id, ON, 0);

			if (VIOC_CONFIG_GetScaler_PluginToRDMA(vioc->m2m_rdma.id) != vioc->sc.id)
				scaler_setup(vout);

			/* update wdma base address */
			VIOC_WDMA_SetImageBase(vioc->m2m_wdma.addr,
					vioc->m2m_wdma.img.base0, vioc->m2m_wdma.img.base1, vioc->m2m_wdma.img.base2);
			vout_m2m_ctrl(vioc, 1);
		} else {
			(void)pr_err("[ERR][VOUT] Not support for DTRC on this chipset type!!\n");
		}
		#endif
	} else {
		#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC_DECOMP)
		if (VIOC_CONFIG_DMAPath_Support() != 0) {
			unsigned int component_num = s32_to_u32(VIOC_CONFIG_DMAPath_Select(vioc->m2m_rdma.id));

			(void)VIOC_CONFIG_DMAPath_UnSet(u32_to_s32(component_num));

			// It is default path selection(VRDMA)
			(void)VIOC_CONFIG_DMAPath_Set(vioc->m2m_rdma.id, vioc->m2m_rdma.id);
		} else {
			#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			(void)VIOC_CONFIG_MCPath(vioc->m2m_wmix.id, vioc->m2m_rdma.id);
			#endif
		}
		#endif

		#ifdef CONFIG_TCC_VIOCMG
		if (vout->deinterlace == VOUT_DEINTL_VIQE_3D || vout->deinterlace == VOUT_DEINTL_VIQE_2D) {
			if (viocmg_lock_viqe(VIOCMG_CALLERID_VOUT) < 0) {
				// VIQE may be used for rear camera !!
				viqe_locked = 0;

				#ifdef CONFIG_VOUT_KEEP_VIDEO_LAYER
				if (vout->src_pix.field == V4L2_FIELD_INTERLACED_TB
						|| vout->src_pix.field == V4L2_FIELD_INTERLACED_BT
						|| vout->src_pix.field == V4L2_FIELD_INTERLACED) {
					// backup the deinterlace mode
					previous_deinterlace = vout->deinterlace;
					vout->deinterlace = VOUT_DEINTL_S;

					// force disable wdma r2y
					VIOC_WDMA_SetImageR2YEnable(vioc->m2m_wdma.addr, OFF);

					// fill information..!
					if (vout->is_viqe_shared == OFF) {
						vout->is_viqe_shared = ON;
						force_process = 1;
						dprintk(" force_process = 1\r\n");
					}
				}
				#else
				vout->last_cleared_buffer = buf;
				vout->display_done = ON;

				vout->firstFieldFlag = 0;

				#ifndef CONFIG_VOUT_USE_VSYNC_INT
				vout_clear_buffer(vout, vout->last_cleared_buffer);
				#endif
				return;
				#endif
			} else {
				VIOC_PlugInOutCheck plugin_state;

				#ifdef CONFIG_VOUT_KEEP_VIDEO_LAYER
				if (vout->is_viqe_shared) {
					vout->is_viqe_shared = OFF;

					/* reset deintl_s */
					(void)VIOC_CONFIG_PlugOut(vioc->deintl_s.id);
					VIOC_CONFIG_SWReset(vioc->deintl_s.id, VIOC_CONFIG_RESET);
					VIOC_CONFIG_SWReset(vioc->deintl_s.id, VIOC_CONFIG_CLEAR);
				}
				#endif

				VIOC_CONFIG_Device_PlugState(vioc->viqe.id, &plugin_state);
				if (!plugin_state.enable || plugin_state.connect_statue != VIOC_PATH_CONNECTED) {
					if (deintl_viqe_setup(vout, vout->deinterlace, 1) < 0)
						(void)pr_err("[ERR][VOUT] failed VIQE%d reconnection\n", vioc->viqe.id);
					dprintk("reconnect VIQE%d\n", vioc->viqe.id);
					vout->frame_count = 0;
					force_process = 1;
				}
				viqe_locked = 1;
			}
		}
		#endif

		/* change deinterlace
		 *	case1: VOUT_DEINTL_VIQE_3D/2D <-> VOUT_DEINTL_VIQE_BYPASS
		 *	case2: VOUT_DEINTL_S <-> VOUT_DEINTL_NONE
		 */
		#ifdef CONFIG_TCC_VIOCMG
		if (res_change || force_process || vout->previous_field != vout->src_pix.field)
		#else
		if ((res_change != 0U) || (vout->previous_field != (enum v4l2_field)vout->src_pix.field))
		#endif
		{
			vout->previous_field = (enum v4l2_field)vout->src_pix.field;
			if ((vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_TB)
				|| (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_BT)
				|| (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED)) {
				switch (vout->deinterlace) {
				case VOUT_DEINTL_VIQE_3D:
				case VOUT_DEINTL_VIQE_2D:
					if (res_change != OFF) {
						(void)deintl_viqe_setup(vout, vout->deinterlace, 0);
					} else {
						VIOC_VIQE_SetDeintlMode(vioc->viqe.addr, VIOC_VIQE_DEINTL_MODE_2D);
						VIOC_VIQE_SetControlMode(vioc->viqe.addr, OFF, OFF, OFF, OFF, ON);
					}

					if (vout->src_pix.colorspace == (__u32)V4L2_COLORSPACE_JPEG) {
						VIOC_RDMA_SetImageY2REnable(vioc->m2m_rdma.addr, 0U);	// force disable y2r
					}
					break;
				case VOUT_DEINTL_S:
					(void)deintl_s_setup(vout);
					break;
				default:
					/* prevent KCS */
					break;
				}
				VIOC_RDMA_SetImageIntl(vioc->m2m_rdma.addr, 1);
				dprintk("enable deintl(%d)\n", vout->deinterlace);
			} else {
				switch (vout->deinterlace) {
				case VOUT_DEINTL_VIQE_3D:
				case VOUT_DEINTL_VIQE_2D:
					#ifdef CONFIG_TCC_VIOCMG
					if (viqe_locked)
					#endif
					{
						VIOC_VIQE_SetDeintlMode(vioc->viqe.addr, VIOC_VIQE_DEINTL_MODE_BYPASS);
						VIOC_VIQE_SetControlMode(vioc->viqe.addr, OFF, OFF, OFF, OFF, OFF);
						vout->frame_count = 0;

						if (vout->src_pix.colorspace == (__u32)V4L2_COLORSPACE_JPEG) {
							VIOC_RDMA_SetImageY2REnable(vioc->m2m_rdma.addr, 1); // force enable y2r
						}
					}
					break;
				case VOUT_DEINTL_S:
					(void)VIOC_CONFIG_PlugOut(vioc->deintl_s.id);
					break;
				default:
					/* prevent KCS */
					break;
				}
				VIOC_RDMA_SetImageIntl(vioc->m2m_rdma.addr, 0U);
				dprintk("disable deintl(%d)\n", vout->deinterlace);
			}
		}

		/* update rdma (bfield and src addr.).
		 */
		if (vioc->m2m_rdma.width <= 3072U) {
			VIOC_RDMA_SetImageUVIEnable(vioc->m2m_rdma.addr, 1U);	//true
		} else {
			VIOC_RDMA_SetImageUVIEnable(vioc->m2m_rdma.addr, 0U);	//false
		}
		VIOC_RDMA_SetImageBfield(vioc->m2m_rdma.addr, vioc->m2m_rdma.bf);
		VIOC_RDMA_SetImageBase(vioc->m2m_rdma.addr,
			vioc->m2m_rdma.img.base0, vioc->m2m_rdma.img.base1, vioc->m2m_rdma.img.base2);

		#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
		if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
			if (vout->src_pix.field == V4L2_FIELD_INTERLACED_TB
				|| vout->src_pix.field == V4L2_FIELD_INTERLACED_BT
				|| vout->src_pix.field == V4L2_FIELD_INTERLACED) {
				switch (vout->deinterlace) {
				case VOUT_DEINTL_VIQE_3D:
				case VOUT_DEINTL_VIQE_2D:
						VIOC_VIQE_SetControlMode(vioc->viqe.addr, OFF, OFF, OFF, OFF, ON);
					break;
				default:
					/* prevent KCS */
					break;
				}
			}
		}
		#endif

		VIOC_RDMA_SetImageEnable(vioc->m2m_rdma.addr);

		/* update wdma (dst addr.)
		 * the wdma was enabled only by vout_m2m_ctrl
		 */
		VIOC_WDMA_SetImageBase(vioc->m2m_wdma.addr,
			vioc->m2m_wdma.img.base0, vioc->m2m_wdma.img.base1, vioc->m2m_wdma.img.base2);
		vout_m2m_ctrl(vioc, 1);
	}

	vout->last_cleared_buffer = buf;

	#ifndef CONFIG_VOUT_USE_VSYNC_INT
	if ((vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_TB)
				|| (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_BT)
				|| (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED)) {
next_field:
		/* coverity[cert_dcl37_c_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
		/* coverity[cert_int31_c_violation : FALSE] */
		if (wait_event_interruptible_timeout(vout->frame_wait, vout->wakeup_int == 1, msecs_to_jiffies(200U)) <= 0) {
			(void)pr_err("[ERR][VOUT] [interlace] handler timeout\n");
			if (vout->firstFieldFlag == 0) {
				atomic_inc(&vout->displayed_buff_count);
			}
		}
		vout->wakeup_int = 0;

		if (vout->firstFieldFlag != 0) {
			unsigned int idx = vout->deintl_nr_bufs_count++ % vout->deintl_nr_bufs;

			if (vout->deintl_nr_bufs_count == vout->deintl_nr_bufs) {
				vout->deintl_nr_bufs_count = 0;
			}

			/* wdma base address */
			vioc->m2m_wdma.img.base0 = vout->deintl_bufs[idx].img_base0;
			vioc->m2m_wdma.img.base1 = vout->deintl_bufs[idx].img_base1;
			vioc->m2m_wdma.img.base2 = vout->deintl_bufs[idx].img_base2;
			VIOC_WDMA_SetImageBase(vioc->m2m_wdma.addr, vioc->m2m_wdma.img.base0, vioc->m2m_wdma.img.base1, vioc->m2m_wdma.img.base2);

			/* change rdma bfield */
			VIOC_RDMA_SetImageBfield(vioc->m2m_rdma.addr, (vioc->m2m_rdma.bf > 0U) ? 0U : 1U);
			VIOC_RDMA_SetImageUpdate(vioc->m2m_rdma.addr);

			vout->firstFieldFlag = 0;

			/* enable wdma */
			vout_m2m_ctrl(vioc, 1);
			goto next_field;
		}
	} else {
		/* coverity[cert_dcl37_c_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
		/* coverity[cert_int31_c_violation : FALSE] */
		if (wait_event_interruptible_timeout(vout->frame_wait, vout->wakeup_int == 1, msecs_to_jiffies(200U)) <= 0) {
			(void)pr_err("[ERR][VOUT] [progressive] handler timeout\n");
			atomic_inc(&vout->displayed_buff_count);
		}
		vout->wakeup_int = 0;
	}
	#endif

	#ifdef CONFIG_TCC_VIOCMG
	#ifdef CONFIG_VOUT_KEEP_VIDEO_LAYER
	if (vout->is_viqe_shared) {
		// restore interlace
		vout->deinterlace = previous_deinterlace;

		if (vout->src_pix.field == V4L2_FIELD_INTERLACED_TB
				|| vout->src_pix.field == V4L2_FIELD_INTERLACED_BT
				|| vout->src_pix.field == V4L2_FIELD_INTERLACED) {
			// restore wdma r2y
			VIOC_WDMA_SetImageR2YEnable(vioc->m2m_wdma.addr, vioc->m2m_wdma.r2y);
		}
	}
	#endif
	if (viqe_locked)
		viocmg_free_viqe(VIOCMG_CALLERID_VOUT);
	#endif
}

#ifdef CONFIG_VOUT_USE_VSYNC_INT
static void display_update(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	struct v4l2_buffer *pNextBuffer = NULL;
	#ifdef CONFIG_TCC_VIOCMG
	unsigned int viqe_locked = 0;
	#endif
	int ret = 0;

search_nextfrm:
	if (vout->display_done && vout->last_cleared_buffer) {
		vout_clear_buffer(vout, vout->last_cleared_buffer);

		/* update DD flag */
		vout->display_done = OFF;
	}
	#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
	if (vout->last_cleared_buffer == NULL && (HDMI_video_width == 720 && HDMI_video_height == 480) && atomic_read(&vout->readable_buff_count) < 7)
		return;
	#endif

	if ((atomic_read(&vout->readable_buff_count) > 0) || vout->firstFieldFlag) {
		pNextBuffer = vout_get_buffer(vout);	/* get next buffer */
		if (vout->firstFieldFlag == 0) {
			/* first field */
			if (pNextBuffer->reserved2 || vout->force_sync) {
				if (vout->force_sync)
					vout->force_sync = OFF;
				tcc_vout_set_time(vout, ((pNextBuffer->timestamp.tv_sec*1000)+(pNextBuffer->timestamp.tv_usec/1000)));
			}

			/* check video time */
			ret = vout_check_syncTime(vout, pNextBuffer, tcc_vout_get_time(vout));
			if (ret == VOUT_DRV_ERR_DROPFRM) {
				if ((atomic_read(&vout->readable_buff_count)-1) == 0)
					goto force_disp;

				vout_pop_buffer(vout);

				vout->last_cleared_buffer = pNextBuffer;
				vout->display_done = ON;
				goto search_nextfrm;
			} else if (ret == VOUT_DRV_ERR_WAITFRM) {
				#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
				if (nFrame > 1) {
					nFrame_t1++;
					if (vout->onthefly_mode)
						vout_onthefly_display_update(vout, vout->last_cleared_buffer);
					else
						vout_m2m_display_update(vout, vout->last_cleared_buffer);
				}
				#endif
				return;
			}
force_disp:
			dbprintk("d(%d %d)(%d %d)\n", vout->popIdx, atomic_read(&vout->readable_buff_count), pNextBuffer->index, pNextBuffer->reserved2);

			if (!vout->deintl_force)
				vout->src_pix.field = pNextBuffer->field;

			if (vout->src_pix.field == V4L2_FIELD_INTERLACED_TB
				|| vout->src_pix.field == V4L2_FIELD_INTERLACED_BT
				|| vout->src_pix.field == V4L2_FIELD_INTERLACED)
				vout->firstFieldFlag++;

			if (vout->onthefly_mode)
				vout_onthefly_display_update(vout, pNextBuffer);
			else
				vout_m2m_display_update(vout, pNextBuffer);

			#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
			nFrame_t0++;
			if (nFrame != 1)
			#endif
			{
				vout_pop_buffer(vout);
			}
		} else {
			/* next field */
			vout->firstFieldFlag = 0;

			#ifdef CONFIG_TCC_VIOCMG
			if (vout->deinterlace == VOUT_DEINTL_VIQE_3D || vout->deinterlace == VOUT_DEINTL_VIQE_2D) {
				if (viocmg_lock_viqe(VIOCMG_CALLERID_VOUT) < 0) {
					// VIQE may be used for rear camera !!
					viqe_locked = 0;

					// buffer release
					vout->display_done = ON;
					if (vout->src_pix.field == V4L2_FIELD_INTERLACED_TB
							|| vout->src_pix.field == V4L2_FIELD_INTERLACED_BT
							|| vout->src_pix.field == V4L2_FIELD_INTERLACED)
						vout->firstFieldFlag = 0;
					return;
				} else {
					VIOC_PlugInOutCheck plugin_state;

					VIOC_CONFIG_Device_PlugState(vioc->viqe.id, &plugin_state);
					if (!plugin_state.enable || plugin_state.connect_statue != VIOC_PATH_CONNECTED) {
						if (deintl_viqe_setup(vout, vout->deinterlace, 1) < 0)
							(void)pr_err("[ERR][VOUT] failed VIQE%d reconnection\n", vioc->viqe.id);
						dprintk("reconnect VIQE%d\n", vioc->viqe.id);
						vout->frame_count = 0;
					}
					viqe_locked = 1;
				}
			}
			#endif

			if (vout->onthefly_mode) {
				if (vout->src_pix.field == V4L2_FIELD_INTERLACED_TB
					|| vout->src_pix.field == V4L2_FIELD_INTERLACED_BT
					|| vout->src_pix.field == V4L2_FIELD_INTERLACED) {
					/* change rdma bfield */
					VIOC_RDMA_SetImageBfield(vioc->rdma.addr, !(vioc->rdma.bf));
					VIOC_RDMA_SetImageUpdate(vioc->rdma.addr);

					dtprintk("%s-field done\n", vioc->rdma.bf ? "bot" : "top");

					if ((vout->deinterlace == VOUT_DEINTL_VIQE_3D) && (vout->frame_count != -1)) {
						if (vout->frame_count >= 3) {
							VIOC_VIQE_SetDeintlMode(vioc->viqe.addr, VIOC_VIQE_DEINTL_MODE_3D);
							vout->frame_count = -1;
							dprintk("Change VIQE_3D mode\n");

							/* enhancement de-interlace */
							{
								int deintl_judder_cnt = (vioc->rdma.width + 64) / 64 - 1;

								VIOC_VIQE_SetDeintlJudderCnt(vioc->viqe.addr, deintl_judder_cnt);
							}
						} else {
							VIOC_VIQE_SetDeintlMode(vioc->viqe.addr, VIOC_VIQE_DEINTL_MODE_2D);
							vout->frame_count++;
						}
					}

					if (vout->firstFieldFlag == 0)
						vout->display_done = ON;

					if (vout->clearFrameMode) {
						vout_clear_buffer(vout, vout->last_cleared_buffer);

						/* update DD flag */
						vout->display_done = OFF;
					}
				}
			} else {
				int index = (vout->deintl_nr_bufs_count++ % vout->deintl_nr_bufs);

				if (vout->deintl_nr_bufs_count == vout->deintl_nr_bufs)
					vout->deintl_nr_bufs_count = 0;

				/* wdma base address */
				vioc->m2m_wdma.img.base0 = vout->deintl_bufs[index].img_base0;
				vioc->m2m_wdma.img.base1 = vout->deintl_bufs[index].img_base1;
				vioc->m2m_wdma.img.base2 = vout->deintl_bufs[index].img_base2;

				VIOC_WDMA_SetImageBase(vioc->m2m_wdma.addr, vioc->m2m_wdma.img.base0, vioc->m2m_wdma.img.base1, vioc->m2m_wdma.img.base2);

				/* change rdma bfield */
				VIOC_RDMA_SetImageBfield(vioc->m2m_rdma.addr, !(vioc->m2m_rdma.bf));
				VIOC_RDMA_SetImageUpdate(vioc->m2m_rdma.addr);

				/* enable wdma */
				vout_m2m_ctrl(vioc, 1);
			}
			#ifdef CONFIG_TCC_VIOCMG
			if (viqe_locked)
				viocmg_free_viqe(VIOCMG_CALLERID_VOUT);
			#endif
		}
	}
#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
	else {
		if (!vout->firstFieldFlag && nFrame > 1 && vout->last_cleared_buffer != NULL) {
			nFrame_t2++;
			if (vout->onthefly_mode)
				vout_onthefly_display_update(vout, vout->last_cleared_buffer);
			else
				vout_m2m_display_update(vout, vout->last_cleared_buffer);
		}
	}
#endif
}

static irqreturn_t vsync_irq_handler(int irq, void *client_data)
{
	struct tcc_vout_device *vout = (struct tcc_vout_device *)client_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
		void __iomem *pDV_Cfg = VIOC_DV_VEDR_GetAddress(VDV_CFG);
		unsigned int status = 0;

		VIOC_V_DV_GetInterruptPending(pDV_Cfg, &status);

		if (status & INT_PEND_F_TX_VS_MASK)
			VIOC_V_DV_ClearInterrupt(pDV_Cfg, INT_CLR_F_TX_VS_MASK);
		else
			return IRQ_NONE;
	} else
#endif
	{
		if (!is_vioc_intr_activatied(vioc->disp.vioc_intr->id, vioc->disp.vioc_intr->bits))
			return IRQ_NONE;
		ret = vioc_intr_clear(vioc->disp.vioc_intr->id, vioc->disp.vioc_intr->bits);
	}

	if (vout->clearFrameMode)
		return IRQ_HANDLED;

	if (vout->status == TCC_VOUT_RUNNING)
		display_update(vout);	// update video frame

	return IRQ_HANDLED;
}
#endif

#if defined(CONFIG_TCC_DUAL_DISPLAY)
static irqreturn_t hdmi_wdma_irq_handler(int irq, void *client_data)
{
	struct tcc_vout_device *vout = (struct tcc_vout_device *)client_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int status;

	if (!is_vioc_intr_activatied(vioc->m2m_dual_wdma[M2M_DUAL_1].vioc_intr->id,
		vioc->m2m_dual_wdma[M2M_DUAL_1].vioc_intr->bits)) {
		return IRQ_NONE;
	}

	VIOC_WDMA_GetStatus(vioc->m2m_dual_wdma[M2M_DUAL_1].addr, &status);
	if (status & VIOC_WDMA_IREQ_EOFR_MASK) {
		ret = vioc_intr_clear(vioc->m2m_dual_wdma[M2M_DUAL_1].vioc_intr->id, vioc->m2m_dual_wdma[M2M_DUAL_1].vioc_intr->bits);
		if (VIOC_DISP_Get_TurnOnOff(vioc->disp_dual.addr)) {
			VIOC_WMIX_SetPosition(vioc->wmix_dual.addr, vioc->wmix_dual.pos, vout->dual_disp_rect.left, vout->dual_disp_rect.top);
			VIOC_WMIX_SetUpdate(vioc->wmix_dual.addr);

			VIOC_RDMA_SetImageSize(vioc->rdma_dual.addr, vioc->m2m_dual_wmix[M2M_DUAL_1].width, vioc->m2m_dual_wmix[M2M_DUAL_1].height);
			VIOC_RDMA_SetImageOffset(vioc->rdma_dual.addr, vioc->m2m_wdma.fmt, vioc->m2m_dual_wmix[M2M_DUAL_1].width);
			VIOC_RDMA_SetImageBase(vioc->rdma_dual.addr, vioc->m2m_dual_wdma[M2M_DUAL_1].img.base0, vioc->m2m_dual_wdma[M2M_DUAL_1].img.base1, vioc->m2m_dual_wdma[M2M_DUAL_1].img.base2);

			if (vout->clearFrameMode == OFF) {
				if (vout->disp_mode == 0 || vout->disp_mode == 2)
					VIOC_RDMA_SetImageEnable(vioc->rdma_dual.addr);
				else if (vout->disp_mode == 1 || vout->disp_mode == 3)
					VIOC_RDMA_SetImageDisable(vioc->rdma_dual.addr);
			}
		}

		if (vout->src_pix.field == V4L2_FIELD_INTERLACED_TB
			|| vout->src_pix.field == V4L2_FIELD_INTERLACED_BT
			|| vout->src_pix.field == V4L2_FIELD_INTERLACED) {
			vout->hdmi_wakeup_int = 1;
			wake_up_interruptible(&vout->hdmi_frame_wait);
		} else {
			vout->hdmi_wakeup_int = 1;
			wake_up_interruptible(&vout->hdmi_frame_wait);
		}
	}
	return IRQ_HANDLED;
}

static irqreturn_t ext_wdma_irq_handler(int irq, void *client_data)
{
	struct tcc_vout_device *vout = (struct tcc_vout_device *)client_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int status;
	int ret = 0;

	if (!is_vioc_intr_activatied(vioc->m2m_dual_wdma[M2M_DUAL_0].vioc_intr->id,
		vioc->m2m_dual_wdma[M2M_DUAL_0].vioc_intr->bits)) {
		ret = IRQ_NONE;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto handler_exit:
	}

	VIOC_WDMA_GetStatus(vioc->m2m_dual_wdma[M2M_DUAL_0].addr, &status);
	if (status & VIOC_WDMA_IREQ_EOFR_MASK) {
		ret = vioc_intr_clear(vioc->m2m_dual_wdma[M2M_DUAL_0].vioc_intr->id, vioc->m2m_dual_wdma[M2M_DUAL_0].vioc_intr->bits);

		if (VIOC_DISP_Get_TurnOnOff(vioc->disp.addr)) {
			VIOC_WMIX_SetPosition(vioc->wmix.addr, vioc->wmix.pos, vioc->wmix.left, vioc->wmix.top);
			VIOC_WMIX_SetUpdate(vioc->wmix.addr);

			VIOC_RDMA_SetImageSize(vioc->rdma.addr, vioc->m2m_dual_wmix[M2M_DUAL_0].width, vioc->m2m_dual_wmix[M2M_DUAL_0].height);
			VIOC_RDMA_SetImageOffset(vioc->rdma.addr, vioc->m2m_wdma.fmt, vioc->m2m_dual_wmix[M2M_DUAL_0].width);
			VIOC_RDMA_SetImageBase(vioc->rdma.addr, vioc->m2m_dual_wdma[M2M_DUAL_0].img.base0, vioc->m2m_dual_wdma[M2M_DUAL_0].img.base1, vioc->m2m_dual_wdma[M2M_DUAL_0].img.base2);

			if (vout->clearFrameMode == OFF) {
				if (vout->disp_mode == 0 || vout->disp_mode == 1)
					VIOC_RDMA_SetImageEnable(vioc->rdma.addr);
				else if (vout->disp_mode == 2 || vout->disp_mode == 3)
					VIOC_RDMA_SetImageDisable(vioc->rdma.addr);
			}
		}

		if (vout->src_pix.field == V4L2_FIELD_INTERLACED_TB
			|| vout->src_pix.field == V4L2_FIELD_INTERLACED_BT
			|| vout->src_pix.field == V4L2_FIELD_INTERLACED) {
			vout->ext_wakeup_int = 1;
			wake_up_interruptible(&vout->ext_frame_wait);
		} else {
			vout->ext_wakeup_int = 1;
			wake_up_interruptible(&vout->ext_frame_wait);
		}
	}
handler_exit:
	return IRQ_HANDLED;
}
#endif

static irqreturn_t wdma_irq_handler(int irq, void *client_data)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_vout_device *vout = (struct tcc_vout_device *)client_data;
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int status;
	irqreturn_t ret = 0;
	(void)irq;

	if (!is_vioc_intr_activatied(u32_to_s32(vioc->m2m_wdma.vioc_intr->id), vioc->m2m_wdma.vioc_intr->bits)) {
		ret = IRQ_NONE;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto handler_exit;
	}

	VIOC_WDMA_GetStatus(vioc->m2m_wdma.addr, &status);
	if ((status & VIOC_WDMA_IREQ_EOFR_MASK) != 0U) {
		/* clear wdma interrupt status */
		(void)vioc_intr_clear(u32_to_s32(vioc->m2m_wdma.vioc_intr->id), vioc->m2m_wdma.vioc_intr->bits);

	#if defined(CONFIG_TCC_DUAL_DISPLAY)
		if (VIOC_DISP_Get_TurnOnOff(vioc->disp.addr)) {
			VIOC_WMIX_SetPosition(vioc->m2m_dual_wmix[M2M_DUAL_0].addr, vioc->m2m_dual_wmix[M2M_DUAL_0].pos, vioc->m2m_dual_wmix[M2M_DUAL_0].left, vioc->m2m_dual_wmix[M2M_DUAL_0].top);
			VIOC_WMIX_SetUpdate(vioc->m2m_dual_wmix[M2M_DUAL_0].addr);

			VIOC_RDMA_SetImageSize(vioc->m2m_dual_rdma[M2M_DUAL_0].addr, vioc->m2m_wdma.width, vioc->m2m_wdma.height);
			VIOC_RDMA_SetImageOffset(vioc->m2m_dual_rdma[M2M_DUAL_0].addr, vioc->m2m_wdma.fmt, vioc->m2m_wdma.width);
			VIOC_RDMA_SetImageBase(vioc->m2m_dual_rdma[M2M_DUAL_0].addr, vioc->m2m_wdma.img.base0, vioc->m2m_wdma.img.base1, vioc->m2m_wdma.img.base2);
			if (vout->clearFrameMode == OFF)
				VIOC_RDMA_SetImageEnable(vioc->m2m_dual_rdma[M2M_DUAL_0].addr);

			VIOC_WMIX_SetPosition(vioc->m2m_dual_wmix[M2M_DUAL_1].addr, vioc->m2m_dual_wmix[M2M_DUAL_0].pos, vioc->m2m_dual_wmix[M2M_DUAL_0].left, vioc->m2m_dual_wmix[M2M_DUAL_0].top);
			VIOC_WMIX_SetUpdate(vioc->m2m_dual_wmix[M2M_DUAL_1].addr);

			VIOC_RDMA_SetImageSize(vioc->m2m_dual_rdma[M2M_DUAL_1].addr, vioc->m2m_wdma.width, vioc->m2m_wdma.height);
			VIOC_RDMA_SetImageOffset(vioc->m2m_dual_rdma[M2M_DUAL_1].addr, vioc->m2m_wdma.fmt, vioc->m2m_wdma.width);
			VIOC_RDMA_SetImageBase(vioc->m2m_dual_rdma[M2M_DUAL_1].addr, vioc->m2m_wdma.img.base0, vioc->m2m_wdma.img.base1, vioc->m2m_wdma.img.base2);
			if (vout->clearFrameMode == OFF)
				VIOC_RDMA_SetImageEnable(vioc->m2m_dual_rdma[M2M_DUAL_1].addr);
		}
	#else
		if (VIOC_DISP_Get_TurnOnOff(vioc->disp.addr) != 0U) {
			VIOC_WMIX_SetPosition(vioc->wmix.addr, vioc->wmix.pos, vioc->wmix.left, vioc->wmix.top);
			VIOC_WMIX_SetUpdate(vioc->wmix.addr);

			VIOC_RDMA_SetImageSize(vioc->rdma.addr, vioc->rdma.width, vioc->rdma.height);
			VIOC_RDMA_SetImageOffset(vioc->rdma.addr, vioc->rdma.fmt, vioc->rdma.width);
			VIOC_RDMA_SetImageBase(vioc->rdma.addr, vioc->m2m_wdma.img.base0, vioc->m2m_wdma.img.base1, vioc->m2m_wdma.img.base2);
			if (vout->clearFrameMode == 0) {
				VIOC_RDMA_SetImageEnable(vioc->rdma.addr);
			}
		}
	#endif

		if ((vout->src_pix.field == (unsigned int)V4L2_FIELD_INTERLACED_TB)
			|| (vout->src_pix.field == (unsigned int)V4L2_FIELD_INTERLACED_BT)
			|| (vout->src_pix.field == (unsigned int)V4L2_FIELD_INTERLACED)) {
			dtprintk("%s-field done\n", vioc->m2m_rdma.bf ? "bot" : "top");

			if ((vout->deinterlace == VOUT_DEINTL_VIQE_3D) && (vout->frame_count != -1)) {
				if (vout->frame_count >= 3) {
					VIOC_VIQE_SetDeintlMode(vioc->viqe.addr, VIOC_VIQE_DEINTL_MODE_3D);
					vout->frame_count = -1;
					dprintk("Change VIQE_3D mode\n");

					/* enhancement de-interlace */
					{
						int deintl_judder_cnt = u32_to_s32(((vioc->m2m_rdma.width + 64U) / 64U) - 1U);

						VIOC_VIQE_SetDeintlJudderCnt(vioc->viqe.addr, (unsigned int)deintl_judder_cnt);
					}
				} else {
					VIOC_VIQE_SetDeintlMode(vioc->viqe.addr, VIOC_VIQE_DEINTL_MODE_2D);
					vout->frame_count++;
				}
			}

			#ifdef CONFIG_VOUT_USE_VSYNC_INT
			if (vout->firstFieldFlag == 0)
				vout->display_done = ON;

			if (vout->clearFrameMode) {
				vout->firstFieldFlag = 0;
				vout_clear_buffer(vout, vout->last_cleared_buffer);

				/* update DD flag */
				vout->display_done = OFF;
			}
			#else
			if (vout->firstFieldFlag == 0) {
				atomic_inc(&vout->displayed_buff_count);
			}
			vout->wakeup_int = 1;
			wake_up_interruptible(&vout->frame_wait);
			#endif
		} else {
			#ifdef CONFIG_VOUT_USE_VSYNC_INT
			/* update DD flag */
			vout->display_done = ON;
			if (vout->clearFrameMode) {
				vout_clear_buffer(vout, vout->last_cleared_buffer);

				/* update DD flag */
				vout->display_done = OFF;
			}
			#else
			atomic_inc(&vout->displayed_buff_count);
			vout->wakeup_int = 1;
			wake_up_interruptible(&vout->frame_wait);
			#endif
		}

		#ifdef CONFIG_VOUT_DISPLAY_LASTFRAME
		vout_video_post_process(vout);
		#endif
	}
handler_exit:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void vout_intr_onoff(char on, struct tcc_vout_device *vout)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct tcc_vout_vioc *vioc = vout->vioc;
	int ret;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
		if (on) {
			ret = vioc_intr_enable(VIOC_INTR_TYPE0, VIOC_INTR_V_DV, 1<<FALL_HDMITX_VS);
		} else {
			void __iomem *pDV_Cfg = VIOC_DV_VEDR_GetAddress(VDV_CFG);

			ret = vioc_intr_disable(vioc->disp.irq, VIOC_INTR_V_DV, 1<<FALL_HDMITX_VS);
			VIOC_V_DV_ClearInterrupt(pDV_Cfg, INT_CLR_F_TX_VS_MASK);
		}
	} else
#endif
	{
		if (on != (char)0) {
			ret = vioc_intr_enable(u32_to_s32(vioc->disp.irq), u32_to_s32(vioc->disp.vioc_intr->id), vioc->disp.vioc_intr->bits);
		} else {
			ret = vioc_intr_disable(u32_to_s32(vioc->disp.irq), u32_to_s32(vioc->disp.vioc_intr->id), vioc->disp.vioc_intr->bits);
		}
	}
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(vout_intr_onoff);

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int vout_video_display_enable(struct tcc_vout_device *vout)
{
	int ret = 0;
	#ifdef CONFIG_VOUT_USE_VSYNC_INT
	struct tcc_vout_vioc *vioc = vout->vioc;

	if (vioc->disp.irq_enable == 0) {
		vioc->disp.irq_enable++;        // set interrupt flag
		vout_intr_onoff(0, vout);
		ret = request_irq(vioc->disp.irq, vsync_irq_handler, IRQF_SHARED, vout->vdev->name, vout);
		if (ret)
			(void)pr_err("[ERR][VOUT] vsync_irq_handler failed\n");
		vout_intr_onoff(1, vout);
	} else {
		dprintk("disp%d interrupt has already been enabled\n", get_vioc_index(vioc->disp.id));
	}

	if (vout->vout_timer)
		tcc_timer_enable(vout->vout_timer);
	#else
	(void)vout;
	#endif
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void vout_video_display_disable(struct tcc_vout_device *vout)
{
	#ifdef CONFIG_VOUT_USE_VSYNC_INT
	struct tcc_vout_vioc *vioc = vout->vioc;

	if (vioc->disp.irq_enable) {
		vioc->disp.irq_enable--;
		vout_intr_onoff(0, vout);
		(void)free_irq(vioc->disp.irq, vout);
	} else {
		dprintk("disp%d interrupt has already been disabled\n", vioc->disp.id);
	}
	if (vout->vout_timer)
		tcc_timer_disable(vout->vout_timer);
	#else
	(void)vout;
	#endif
}

#if defined(CONFIG_TCC_DUAL_DISPLAY)
void vout_m2m_dual_ctrl(struct tcc_vout_vioc *vioc, int enable, int m2m_dual_index)
{
	if (enable) {
		VIOC_SC_SetUpdate(vioc->m2m_dual_sc[m2m_dual_index].addr);
		VIOC_WDMA_SetImageEnable(vioc->m2m_dual_wdma[m2m_dual_index].addr, vioc->m2m_dual_wdma[m2m_dual_index].cont);
	} else {
		VIOC_WDMA_SetImageDisable(vioc->m2m_dual_wdma[m2m_dual_index].addr);
	}
}
#endif

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void vout_m2m_ctrl(struct tcc_vout_vioc *vioc, int enable)
{
	if (enable != 0) {
		VIOC_SC_SetUpdate(vioc->sc.addr);
		VIOC_WDMA_SetImageEnable(vioc->m2m_wdma.addr, vioc->m2m_wdma.cont);
	} else {
		VIOC_WDMA_SetImageDisable(vioc->m2m_wdma.addr);
	}
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void vout_video_overlay(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int sw, sh;	// src image size in rdma
	unsigned int dw, dh;	// destination size in scaler
	int bypass = 0;

	/* deintl_path
	 * [rdma]-[viqe]-[wmix]-[SC]-[WDMA]-->
	 */
	/* sc */
	sw = (vout->onthefly_mode != 0) ? vioc->rdma.width : vioc->m2m_rdma.width;
	sh = (vout->onthefly_mode != 0) ? vioc->rdma.height : vioc->m2m_rdma.height;
	dw = vout->disp_rect.width;
	dh = vout->disp_rect.height;

	if ((dw == sw) && (dh == sh)) {
		bypass = 1;
	}
	VIOC_SC_SetBypass(vioc->sc.addr, s32_to_u32(bypass));

	if (vout->onthefly_mode != 0) {	// onthefly_mode
		VIOC_SC_SetDstSize(vioc->sc.addr,
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			(vout->disp_rect.left < 0) ? (vout->disp_rect.width + s32_to_u32(abs(vout->disp_rect.left))) : dw,
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			(vout->disp_rect.top < 0) ? (vout->disp_rect.height + s32_to_u32(abs(vout->disp_rect.top))) : dh);	// set destination size in scaler
		VIOC_SC_SetOutPosition(vioc->sc.addr,
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			(vout->disp_rect.left < 0) ? abs(vout->disp_rect.left) : 0,
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			(vout->disp_rect.top < 0) ? abs(vout->disp_rect.top) : 0);
		VIOC_SC_SetOutSize(vioc->sc.addr, dw, dh);	// set output size in scaler
		VIOC_SC_SetUpdate(vioc->sc.addr);

		// wmix
		vioc->wmix.left = (vout->disp_rect.left < 0) ? 0U : s32_to_u32(vout->disp_rect.left);
		vioc->wmix.top = (vout->disp_rect.top < 0) ? 0U : s32_to_u32(vout->disp_rect.top);
		VIOC_WMIX_SetPosition(vioc->wmix.addr, vioc->wmix.pos, vioc->wmix.left, vioc->wmix.top);
		VIOC_WMIX_SetUpdate(vioc->wmix.addr);
	} else {	// m2m
	#if defined(CONFIG_TCC_DUAL_DISPLAY)
		VIOC_SC_SetDstSize(vioc->m2m_dual_sc[M2M_DUAL_0].addr,
			vout->disp_rect.left < 0 ? vout->disp_rect.width + abs(vout->disp_rect.left) : dw,
			vout->disp_rect.top < 0 ? vout->disp_rect.height + abs(vout->disp_rect.top) : dh);	// set destination size in scaler
		VIOC_SC_SetOutPosition(vioc->m2m_dual_sc[M2M_DUAL_0].addr,
			vout->disp_rect.left < 0 ? abs(vout->disp_rect.left) : 0,
			vout->disp_rect.top < 0 ? abs(vout->disp_rect.top) : 0);
		VIOC_SC_SetOutSize(vioc->m2m_dual_sc[M2M_DUAL_0].addr, dw, dh);	// set output size in scaler
		VIOC_SC_SetUpdate(vioc->m2m_dual_sc[M2M_DUAL_0].addr);

		// wdma
		vioc->m2m_dual_wmix[M2M_DUAL_0].width = dw;
		vioc->m2m_dual_wmix[M2M_DUAL_0].height = dh;
		vioc->m2m_dual_wdma[M2M_DUAL_0].width = dw;
		vioc->m2m_dual_wdma[M2M_DUAL_0].height = dh;

		VIOC_WDMA_SetImageSize(vioc->m2m_dual_wdma[M2M_DUAL_0].addr, vioc->m2m_dual_wdma[M2M_DUAL_0].width, vioc->m2m_dual_wdma[M2M_DUAL_0].height);
		VIOC_WDMA_SetImageOffset(vioc->m2m_dual_wdma[M2M_DUAL_0].addr, vioc->m2m_dual_wdma[M2M_DUAL_0].fmt, vioc->m2m_dual_wdma[M2M_DUAL_0].width);
		VIOC_WDMA_SetImageUpdate(vioc->m2m_dual_wdma[M2M_DUAL_0].addr);

		dw = vout->dual_disp_rect.width;
		dh = vout->dual_disp_rect.height;

		vioc->m2m_dual_wmix[M2M_DUAL_1].width = dw;
		vioc->m2m_dual_wmix[M2M_DUAL_1].height = dh;
		vioc->m2m_dual_wdma[M2M_DUAL_1].width = dw;
		vioc->m2m_dual_wdma[M2M_DUAL_1].height = dh;

		VIOC_SC_SetDstSize(vioc->m2m_dual_sc[M2M_DUAL_1].addr,
			vout->disp_rect.left < 0 ? vout->disp_rect.width + abs(vout->disp_rect.left) : dw,
			vout->disp_rect.top < 0 ? vout->disp_rect.height + abs(vout->disp_rect.top) : dh);	// set destination size in scaler
		VIOC_SC_SetOutPosition(vioc->m2m_dual_sc[M2M_DUAL_1].addr,
			vout->disp_rect.left < 0 ? abs(vout->disp_rect.left) : 0,
			vout->disp_rect.top < 0 ? abs(vout->disp_rect.top) : 0);
		VIOC_SC_SetOutSize(vioc->m2m_dual_sc[M2M_DUAL_1].addr, dw, dh);	// set output size in scaler
		VIOC_SC_SetUpdate(vioc->m2m_dual_sc[M2M_DUAL_1].addr);

		VIOC_WDMA_SetImageSize(vioc->m2m_dual_wdma[M2M_DUAL_1].addr, vioc->m2m_dual_wdma[M2M_DUAL_1].width, vioc->m2m_dual_wdma[M2M_DUAL_1].height);
		VIOC_WDMA_SetImageOffset(vioc->m2m_dual_wdma[M2M_DUAL_1].addr, vioc->m2m_dual_wdma[M2M_DUAL_1].fmt, vioc->m2m_dual_wdma[M2M_DUAL_1].width);
		VIOC_WDMA_SetImageUpdate(vioc->m2m_dual_wdma[M2M_DUAL_1].addr);

		// rdma
		vioc->rdma.width = vout->disp_rect.width;
		vioc->rdma.height = vout->disp_rect.height;

		vioc->rdma_dual.width = vout->dual_disp_rect.width;
		vioc->rdma_dual.height = vout->dual_disp_rect.height;

		// wmix
		vioc->wmix.left = vout->disp_rect.left < 0 ? 0 : vout->disp_rect.left;
		vioc->wmix.top = vout->disp_rect.top < 0 ? 0 : vout->disp_rect.top;

		vioc->wmix_dual.left = vout->disp_rect.left < 0 ? 0 : vout->disp_rect.left;
		vioc->wmix_dual.top = vout->disp_rect.top < 0 ? 0 : vout->disp_rect.top;
	#else
		VIOC_SC_SetDstSize(vioc->sc.addr,
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			(vout->disp_rect.left < 0) ? (vout->disp_rect.width + s32_to_u32(abs(vout->disp_rect.left))) : dw,
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			(vout->disp_rect.top < 0) ? (vout->disp_rect.height + s32_to_u32(abs(vout->disp_rect.top))) : dh);	// set destination size in scaler
		VIOC_SC_SetOutPosition(vioc->sc.addr,
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			(vout->disp_rect.left < 0) ? s32_to_u32(abs(vout->disp_rect.left)) : 0U,
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			(vout->disp_rect.top < 0) ? s32_to_u32(abs(vout->disp_rect.top)) : 0U);
		VIOC_SC_SetOutSize(vioc->sc.addr, dw, dh);	// set output size in scaler
		VIOC_SC_SetUpdate(vioc->sc.addr);

		// wdma
		vioc->m2m_wdma.width = dw;
		vioc->m2m_wdma.height = dh;
		VIOC_WDMA_SetImageSize(vioc->m2m_wdma.addr, vioc->m2m_wdma.width, vioc->m2m_wdma.height);
		VIOC_WDMA_SetImageOffset(vioc->m2m_wdma.addr, vioc->m2m_wdma.fmt, vioc->m2m_wdma.width);
		VIOC_WDMA_SetImageUpdate(vioc->m2m_wdma.addr);

		// rdma
		vioc->rdma.width = vout->disp_rect.width;
		vioc->rdma.height = vout->disp_rect.height;

		// wmix
		vioc->wmix.left = (vout->disp_rect.left < 0) ? 0U : s32_to_u32(vout->disp_rect.left);
		vioc->wmix.top = (vout->disp_rect.top < 0) ? 0U : s32_to_u32(vout->disp_rect.top);
	#endif
	}
}

int vout_otf_init(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	struct vioc_rdma *rdma = &vioc->rdma;
	int ret = 0;

	vout->wakeup_int = 0;
	vout->frame_count = 0;

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	bStep_Check = DEF_DV_CHECK_NUM;
	#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
	/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
	goto init_exit;
	#endif
#endif

	/* 0. swreset  */
	VIOC_CONFIG_SWReset(vioc->sc.id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(vioc->rdma.id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(vioc->rdma.id, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(vioc->sc.id, VIOC_CONFIG_CLEAR);

	/* 1. rdma */
	rdma->intl = (vout->deinterlace > VOUT_DEINTL_VIQE_BYPASS) ? 1U : 0U;
	rdma->img.base0 = 0;
	rdma->img.base1 = 0;
	rdma->img.base2 = 0;
	rdma->width = vout->src_pix.width;

	/*
	 * The VIQE need 4-line align
	 */
	if (((vout->src_pix.height % 4U) != 0U) &&
		((vout->deinterlace == VOUT_DEINTL_VIQE_3D) || (vout->deinterlace == VOUT_DEINTL_VIQE_2D))) {
		rdma->height = ROUND_UP_4(vout->src_pix.height);
		dprintk("viqe 4-line align: %d -> %d\n", vout->src_pix.height, rdma->height);
	} else {
		rdma->height = vout->src_pix.height;
	}

	rdma->y_stride = vout->src_pix.bytesperline & 0x0000ffffU;
	dprintk("Y-stride(%d) UV-stride(%d)\n", rdma->y_stride,
			(vout->src_pix.bytesperline & 0xffff0000) >> 16);
	m2m_rdma_setup(rdma);

	/* 2. deinterlacer */
	switch (vout->deinterlace) {
	case VOUT_DEINTL_VIQE_3D:
	case VOUT_DEINTL_VIQE_2D:
	case VOUT_DEINTL_VIQE_BYPASS:
		vout->frame_count = 0;
		ret = deintl_viqe_setup(vout, vout->deinterlace, 1);
		break;
	case VOUT_DEINTL_S:
		ret = deintl_s_setup(vout);
		break;
	case VOUT_DEINTL_NONE:
		/* only use rdma-wmix */
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if(ret == -EINVAL) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto init_exit;
	}

	/* 3. scaler */
	scaler_setup(vout);

init_exit:
	return ret;
}
#if defined(CONFIG_TCC_DUAL_DISPLAY)
int vout_m2m_dual_init(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	struct vioc_rdma *rdma;
	struct vioc_wdma *wdma;

	int i, ret = 0;

	vout->ext_wakeup_int = 0;
	vout->frame_count = 0;

	for (i = M2M_DUAL_0; i < M2M_DUAL_MAX; i++) {
		rdma = &vioc->m2m_dual_rdma[i];
		wdma = &vioc->m2m_dual_wdma[i];

		/* 0. reset deintl_path */
		m2m_dual_path_reset(vioc, i);

		/* 1. rdma */
		rdma->intl = 0;
		rdma->bf = 0;
		rdma->fmt = VIOC_IMG_FMT_YUV420IL0;
		rdma->y2r = 1;
		rdma->img.base0 = 0;
		rdma->img.base1 = 0;
		rdma->img.base2 = 0;
		rdma->width = vout->src_pix.width;

		rdma->y_stride = vout->src_pix.bytesperline & 0x0000ffffU;
		dprintk("Y-stride(%d) UV-stride(%d)\n", rdma->y_stride,
				(vout->src_pix.bytesperline & 0xffff0000) >> 16);

		m2m_rdma_setup(rdma);

		/* 3. wmixer */
		vioc->m2m_dual_wmix[i].left = 0;		// default: to assume sub-plane size same video size
		vioc->m2m_dual_wmix[i].top = 0;

		vioc->m2m_dual_wmix[i].width = vout->disp_rect.width;
		vioc->m2m_dual_wmix[i].height = vout->disp_rect.height;

		ret = VIOC_CONFIG_WMIXPath(vioc->m2m_dual_rdma[i].id, 1);
		m2m_wmix_setup(&vioc->m2m_dual_wmix[i]);

		/* 4. scaler */
		m2m_scaler_setup(vout, i);

		/* 5. wdma */
		wdma->width = vout->disp_rect.width;	// depend on scaled image
		wdma->height = vout->disp_rect.height;
		wdma->cont = 0;							// 0: frame-by-frame mode
		m2m_wdma_setup(wdma);

		/* wdma interrupt */
		VIOC_CONFIG_StopRequest(0);
		if (vioc->m2m_dual_wdma[i].irq_enable == 0) {
			vioc->m2m_dual_wdma[i].irq_enable++;		// set interrupt flag
			synchronize_irq(vioc->m2m_dual_wdma[i].irq);
			ret = vioc_intr_clear(vioc->m2m_dual_wdma[i].vioc_intr->id, vioc->m2m_dual_wdma[i].vioc_intr->bits);

			if (i == M2M_DUAL_0)
				request_irq(vioc->m2m_dual_wdma[i].irq,
					ext_wdma_irq_handler, IRQF_SHARED, "ext_wdma_interrupt", vout);
			else if (i == M2M_DUAL_1)
				request_irq(vioc->m2m_dual_wdma[i].irq,
					hdmi_wdma_irq_handler, IRQF_SHARED, "hdmi_wdma_interrupt", vout);

			if (ret)
				(void)pr_err("[ERR][VOUT] wdma_irq_handler failed(%d)\n", ret);
			ret = vioc_intr_enable(vioc->m2m_dual_wdma[i].irq,
				vioc->m2m_dual_wdma[i].vioc_intr->id,
				vioc->m2m_dual_wdma[i].vioc_intr->bits);
		}
	}

	print_vioc_deintl_path(vout, "vout_m2m_dual_init");
	return ret;
}
#endif
int vout_m2m_init(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	struct vioc_rdma *rdma = &vioc->m2m_rdma;
	struct vioc_wdma *wdma = &vioc->m2m_wdma;
	int ret = 0;

	vout->wakeup_int = 0;
	vout->frame_count = 0;

	/* 0. reset deintl_path */
	m2m_path_reset(vioc);

	/* 1. rdma */
	rdma->intl = (vout->deinterlace > VOUT_DEINTL_VIQE_BYPASS) ? 1U : 0U;
	rdma->img.base0 = 0;
	rdma->img.base1 = 0;
	rdma->img.base2 = 0;
	rdma->width = vout->src_pix.width;

	/*
	 * The VIQE need 4-line align
	 */
	if (((vout->src_pix.height % 4U) != 0U) &&
		((vout->deinterlace == VOUT_DEINTL_VIQE_3D) || (vout->deinterlace == VOUT_DEINTL_VIQE_2D))) {
		rdma->height = ROUND_UP_4(vout->src_pix.height);
		dprintk("viqe 4-line align: %d -> %d\n", vout->src_pix.height, rdma->height);
	} else {
		rdma->height = vout->src_pix.height;
	}

	rdma->y_stride = vout->src_pix.bytesperline & 0x0000ffffU;
	dprintk("Y-stride(%d) UV-stride(%d)\n", rdma->y_stride,
			(vout->src_pix.bytesperline & 0xffff0000) >> 16);

	m2m_rdma_setup(rdma);

	/* 2. deinterlacer */
	switch (vout->deinterlace) {
	case VOUT_DEINTL_VIQE_3D:
	case VOUT_DEINTL_VIQE_2D:
	case VOUT_DEINTL_VIQE_BYPASS:
		#ifdef CONFIG_TCC_VIOCMG
		dprintk("skip deintl_viqe_setup\r\n");
		#else
		ret = deintl_viqe_setup(vout, vout->deinterlace, 1);
		#endif
		break;
	case VOUT_DEINTL_S:
		ret = deintl_s_setup(vout);
		break;
	case VOUT_DEINTL_NONE:
		/* only use rdma-wmix-wdma */
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if(ret == -EINVAL) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto m2m_init_exit;
	}

	/* 3. wmixer */
	vioc->m2m_wmix.left = 0;		// default: to assume sub-plane size same video size
	vioc->m2m_wmix.top = 0;
	vioc->m2m_wmix.width = vout->src_pix.width;
	vioc->m2m_wmix.height = vout->src_pix.height;
	ret = VIOC_CONFIG_WMIXPath(vioc->m2m_rdma.id, 1);
	m2m_wmix_setup(&vioc->m2m_wmix);

	/* 4. scaler */
#if defined(CONFIG_TCC_DUAL_DISPLAY)
	// scaler_setup(vout);
#else
	scaler_setup(vout);
#endif

	/* 5. wdma */
#if defined(CONFIG_TCC_DUAL_DISPLAY)
	wdma->width = rdma->width;	// depend on scaled image
	wdma->height = rdma->height;
#else
	wdma->width = vout->disp_rect.width;	// depend on scaled image
	wdma->height = vout->disp_rect.height;
#endif
	wdma->cont = 0;							// 0: frame-by-frame mode
	m2m_wdma_setup(wdma);

	/* wdma interrupt */
	VIOC_CONFIG_StopRequest(0);
	if (vioc->m2m_wdma.irq_enable == 0U) {
		vioc->m2m_wdma.irq_enable++;		// set interrupt flag
		synchronize_irq(vioc->m2m_wdma.irq);
		ret = vioc_intr_clear(u32_to_s32(vioc->m2m_wdma.vioc_intr->id), vioc->m2m_wdma.vioc_intr->bits);
		ret = request_irq(vioc->m2m_wdma.irq, wdma_irq_handler, IRQF_SHARED, vout->vdev->name, vout);
		if (ret != 0) {
			(void)pr_err("[ERR][VOUT] wdma_irq_handler failed(%d)\n", ret);
		}
		ret = vioc_intr_enable(u32_to_s32(vioc->m2m_wdma.irq), u32_to_s32(vioc->m2m_wdma.vioc_intr->id), vioc->m2m_wdma.vioc_intr->bits);
	}

	print_vioc_deintl_path(vout, "vout_m2m_init");

m2m_init_exit:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void vout_otf_deinit(struct tcc_vout_device *vout)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct tcc_vout_vioc *vioc = vout->vioc;
	int ret;

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	nFrame = 0;
	bStep_Check = DEF_DV_CHECK_NUM;

	#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
	pr_debug("[DBG][VOUT] &&&&&&&&&&&&&&&&&&&&&& [[[%d = %d/%d/%d]]]\n", nFrame, nFrame_t0, nFrame_t1, nFrame_t2);

	for (nFrame = 0; nFrame < nIdx_copy; nFrame++) {
		pr_debug("[DBG][VOUT] ^@New^^^^^^^^^^^^^ @@@ available(%d), %03d :: TS: %04ld  %d bpp #BL(0x%x, %dx%d (%dx%d), 0x%x fmt) #EL(0x%x, %dx%d (%dx%d)) #OSD(0x%x/0x%x) #Reg(0x%x) #Meta(0x%x)\n",
				nCopy_ImageInfo[nFrame].private_data.optional_info[VID_OPT_PLAYER_IDX], nFrame, nCopy_ImageInfo[nFrame].private_data.optional_info[VID_OPT_TIMESTAMP],
				nCopy_ImageInfo[nFrame].private_data.optional_info[VID_OPT_BIT_DEPTH],
				nCopy_ImageInfo[nFrame].private_data.offset[0],
				nCopy_ImageInfo[nFrame].private_data.optional_info[VID_OPT_FRAME_WIDTH], nCopy_ImageInfo[nFrame].private_data.optional_info[VID_OPT_FRAME_HEIGHT],
				nCopy_ImageInfo[nFrame].private_data.optional_info[VID_OPT_BUFFER_WIDTH], nCopy_ImageInfo[nFrame].private_data.optional_info[VID_OPT_BUFFER_HEIGHT], nCopy_ImageInfo[nFrame].private_data.format,
				nCopy_ImageInfo[nFrame].private_data.dolbyVision_info.el_offset[0],
				nCopy_ImageInfo[nFrame].private_data.dolbyVision_info.el_frame_width, nCopy_ImageInfo[nFrame].private_data.dolbyVision_info.el_frame_height,
				nCopy_ImageInfo[nFrame].private_data.dolbyVision_info.el_buffer_width, nCopy_ImageInfo[nFrame].private_data.dolbyVision_info.el_buffer_height,
				nCopy_ImageInfo[nFrame].private_data.dolbyVision_info.osd_addr[0], nCopy_ImageInfo[nFrame].private_data.dolbyVision_info.osd_addr[1],
				nCopy_ImageInfo[nFrame].private_data.dolbyVision_info.reg_addr, nCopy_ImageInfo[nFrame].private_data.dolbyVision_info.md_hdmi_addr);
	}
	nIdx_copy = 0;
	nTS_Prev = 0x999;

	if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
		struct tcc_lcdc_image_update ImageInfo;
		struct tcc_dp_device *pdp_data = tca_get_displayType(TCC_OUTPUT_HDMI);

		ImageInfo.enable = 0;

		tca_edr_display_update(pdp_data, &ImageInfo);
		#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI // No UI-Blending
		VIOC_RDMA_PreventEnable_for_UI(0, 0);
		#endif
		voic_v_dv_osd_ctrl(/*EDR_OSD3*/RDMA_FB, 1);
		//vioc_v_dv_prog(dv_md_phyaddr, dv_reg_phyaddr, 1);
		return;
	}
	#endif
#endif

	VIOC_RDMA_SetImageDisable(vioc->rdma.addr);
	VIOC_CONFIG_SWReset(vioc->rdma.id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(vioc->rdma.id, VIOC_CONFIG_CLEAR);

	#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC_DECOMP)
	if (VIOC_CONFIG_DMAPath_Support() != 0) {
		switch (VIOC_CONFIG_DMAPath_Select(vioc->rdma.id)) {
		#ifdef CONFIG_VIOC_MAP_DECOMP
		case (int)VIOC_MC0:
		case (int)VIOC_MC1:
			dprintk("MAP_CONV%d plug-out\n", get_vioc_index(vioc->map_converter.id));

			tca_map_convter_onoff(vioc->map_converter.id, 0U, 1U);
			(void)VIOC_CONFIG_DMAPath_UnSet(u32_to_s32(vioc->map_converter.id));
			ret = VIOC_CONFIG_DMAPath_Set(vioc->rdma.id, vioc->rdma.id);

			VIOC_CONFIG_SWReset(vioc->map_converter.id, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(vioc->map_converter.id, VIOC_CONFIG_CLEAR);
			break;
		#endif
		#ifdef CONFIG_VIOC_DTRC_DECOMP
		case (int)VIOC_DTRC0:
		case (int)VIOC_DTRC1:
			dprintk("DTRC_CONV%d plug-out\n", get_vioc_index(vioc->dtrc.id));

			tca_dtrc_convter_onoff(vioc->dtrc.id, 0, 1);
			(void)VIOC_CONFIG_DMAPath_UnSet(vioc->dtrc.id);
			ret = VIOC_CONFIG_DMAPath_Set(vioc->rdma.id, vioc->rdma.id);

			VIOC_CONFIG_SWReset(vioc->dtrc.id, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(vioc->dtrc.id, VIOC_CONFIG_CLEAR);
			break;
		#endif
		default:
			/* prevent KCS */
			break;
		}
	} else {
		#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		ret = VIOC_CONFIG_MCPath(vioc->wmix.id, vioc->rdma.id);
		#endif
	}
	#endif

	ret = VIOC_CONFIG_PlugOut(vioc->sc.id);
	VIOC_CONFIG_SWReset(vioc->sc.id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(vioc->sc.id, VIOC_CONFIG_CLEAR);

	switch (vout->deinterlace) {
	case VOUT_DEINTL_VIQE_2D:
	case VOUT_DEINTL_VIQE_3D:
	case VOUT_DEINTL_VIQE_BYPASS:
		ret = VIOC_CONFIG_PlugOut(vioc->viqe.id);
		VIOC_CONFIG_SWReset(vioc->viqe.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(vioc->viqe.id, VIOC_CONFIG_CLEAR);
		break;
	case VOUT_DEINTL_S:
		ret = VIOC_CONFIG_PlugOut(vioc->deintl_s.id);
		VIOC_CONFIG_SWReset(vioc->deintl_s.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(vioc->deintl_s.id, VIOC_CONFIG_CLEAR);
		break;
	case VOUT_DEINTL_NONE:
	default:
		/* prevent KCS */
		break;
	}

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
		struct tcc_lcdc_image_update ImageInfo;
		struct tcc_dp_device *pdp_data = tca_get_displayType(TCC_OUTPUT_HDMI);

		ImageInfo.enable = 0;
		tca_edr_el_display_update(pdp_data, &ImageInfo);
	}
#endif
}

#if defined(CONFIG_TCC_DUAL_DISPLAY)
void vout_m2m_dual_deinit(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	int ret;

	vout_m2m_dual_ctrl(vioc, 0, M2M_DUAL_0);
	vout_m2m_dual_ctrl(vioc, 0, M2M_DUAL_1);

	ret = VIOC_CONFIG_PlugOut(vioc->m2m_dual_sc[M2M_DUAL_0].id);
	ret = VIOC_CONFIG_PlugOut(vioc->m2m_dual_sc[M2M_DUAL_1].id);

	if (vioc->m2m_dual_wdma[M2M_DUAL_0].irq_enable) {
		vioc->m2m_dual_wdma[M2M_DUAL_0].irq_enable--;
		ret = vioc_intr_disable(vioc->m2m_dual_wdma[M2M_DUAL_0].irq,
			vioc->m2m_dual_wdma[M2M_DUAL_0].vioc_intr->id,
			vioc->m2m_dual_wdma[M2M_DUAL_0].vioc_intr->bits);
		(void)free_irq(vioc->m2m_dual_wdma[M2M_DUAL_0].irq, vout);
	} else {
		dprintk("wdma%d interrupt has already been disabled\n",
			get_vioc_index(vioc->m2m_dual_wdma[M2M_DUAL_0].id));
	}

	if (vioc->m2m_dual_wdma[M2M_DUAL_1].irq_enable) {
		vioc->m2m_dual_wdma[M2M_DUAL_1].irq_enable--;
		ret = vioc_intr_disable(vioc->m2m_dual_wdma[M2M_DUAL_1].irq,
			vioc->m2m_dual_wdma[M2M_DUAL_1].vioc_intr->id,
			vioc->m2m_dual_wdma[M2M_DUAL_1].vioc_intr->bits);
		(void)free_irq(vioc->m2m_dual_wdma[M2M_DUAL_1].irq, vout);
	} else {
		dprintk("wdma%d interrupt has already been disabled\n",
			get_vioc_index(vioc->m2m_dual_wdma[M2M_DUAL_1].id));
	}
}
#endif
void vout_m2m_deinit(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	int ret;

	vout_m2m_ctrl(vioc, 0);

	#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC_DECOMP)
	if (VIOC_CONFIG_DMAPath_Support() != 0) {
		switch (VIOC_CONFIG_DMAPath_Select(vioc->m2m_rdma.id)) {
		#ifdef CONFIG_VIOC_MAP_DECOMP
		case (int)VIOC_MC0:
		case (int)VIOC_MC1:
			dprintk("MAP_CONV%d plug-out\n", get_vioc_index(vioc->map_converter.id));

			tca_map_convter_onoff(vioc->map_converter.id, 0, 1);
			(void)VIOC_CONFIG_DMAPath_UnSet(u32_to_s32(vioc->map_converter.id));
			ret = VIOC_CONFIG_DMAPath_Set(vioc->m2m_rdma.id, vioc->m2m_rdma.id);

			VIOC_CONFIG_SWReset(vioc->map_converter.id, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(vioc->map_converter.id, VIOC_CONFIG_CLEAR);
			break;
		#endif
		#ifdef CONFIG_VIOC_DTRC_DECOMP
		case (int)VIOC_DTRC0:
		case (int)VIOC_DTRC1:
			dprintk("DTRC_CONV%d plug-out\n", get_vioc_index(vioc->dtrc.id));

			tca_dtrc_convter_onoff(vioc->dtrc.id, 0, 1);
			(void)VIOC_CONFIG_DMAPath_UnSet(vioc->dtrc.id);
			ret = VIOC_CONFIG_DMAPath_Set(vioc->m2m_rdma.id, vioc->m2m_rdma.id);

			VIOC_CONFIG_SWReset(vioc->dtrc.id, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(vioc->dtrc.id, VIOC_CONFIG_CLEAR);
			break;
		#endif
		default:
			/* prevent KCS */
			break;
		}
	} else {
		#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		ret = VIOC_CONFIG_MCPath(vioc->m2m_wmix.id, vioc->m2m_rdma.id);
		#endif
	}
	#endif

	ret = VIOC_CONFIG_PlugOut(vioc->sc.id);

	switch (vout->deinterlace) {
	case VOUT_DEINTL_VIQE_2D:
	case VOUT_DEINTL_VIQE_3D:
	case VOUT_DEINTL_VIQE_BYPASS:
		#ifdef CONFIG_TCC_VIOCMG
		if (viocmg_lock_viqe(VIOCMG_CALLERID_VOUT) == 0) {
			ret = VIOC_CONFIG_PlugOut(vioc->viqe.id);
			viocmg_free_viqe(VIOCMG_CALLERID_VOUT);
		}
		#else
		ret = VIOC_CONFIG_PlugOut(vioc->viqe.id);
		#endif
		break;
	case VOUT_DEINTL_S:
		ret = VIOC_CONFIG_PlugOut(vioc->deintl_s.id);
		break;
	case VOUT_DEINTL_NONE:
	default:
		/* prevent KCS */
		break;
	}

	if (vioc->m2m_wdma.irq_enable != 0U) {
		vioc->m2m_wdma.irq_enable--;
		ret = vioc_intr_disable(u32_to_s32(vioc->m2m_wdma.irq), u32_to_s32(vioc->m2m_wdma.vioc_intr->id), vioc->m2m_wdma.vioc_intr->bits);
		(void)free_irq(vioc->m2m_wdma.irq, vout);
	} else {
		dprintk("wdma%d interrupt has already been disabled\n", get_vioc_index(vioc->m2m_wdma.id));
	}
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void sub_rdma_setup(struct vioc_rdma *sub_rdma)
{
	//sub_rdma->addr->uCTRL.bREG.SWAP = sub_rdma->rgbswap;
	VIOC_RDMA_SetImageSize(sub_rdma->addr, sub_rdma->width, sub_rdma->height);
	VIOC_RDMA_SetImageFormat(sub_rdma->addr, sub_rdma->fmt);
	VIOC_RDMA_SetImageOffset(sub_rdma->addr, sub_rdma->fmt, sub_rdma->width);
	VIOC_RDMA_SetImageAlphaSelect(sub_rdma->addr, RDMA_ALPHA_ASEL_PIXEL);
	VIOC_RDMA_SetImageAlphaEnable(sub_rdma->addr, 1);
	VIOC_RDMA_SetImageUpdate(sub_rdma->addr);
}


#ifdef CONFIG_VOUT_DISPLAY_LASTFRAME
int vout_capture_last_frame(struct tcc_vout_device *vout, struct v4l2_buffer *buf)
{
	int ret = 0;

	if (buf) {
		struct tcc_vout_vioc *vioc = vout->vioc;
		struct file    *file;
		unsigned int base0, base1, base2;

		/* get destination address */
		ret = pmap_get_info("fb_wmixer", &lastframe_pbuf);
		if (ret <= 0) {
			(void)pr_err("[ERR][VOUT] %s - Does not exist fb_wmixer pmap\n", __func__);
			ret = -1;
			goto ERR_CAPTURE_PROCESS;
		}
		dprintk("wmixer base:0x%08x size:%d\n", lastframe_pbuf.base, lastframe_pbuf.size);

		if (buf->m.planes == NULL) {
			(void)pr_err("[ERR][VOUT] %s - Does not allocate lastframe memory\n", __func__);
			ret = -1;
			goto ERR_CAPTURE_PROCESS;
		}

		/* get base address */
		if (vout->memory == V4L2_MEMORY_USERPTR) {
			if (buf->timecode.type != STILL_IMGAGE) {
				base0 = buf->m.planes[MPLANE_VID].m.userptr;
				base1 = buf->m.planes[MPLANE_VID].reserved[VID_BASE1];
				base2 = buf->m.planes[MPLANE_VID].reserved[VID_BASE2];
			} else {
				base0 = buf->m.planes[MPLANE_VID].m.userptr;
				base1 = base2 = 0;
				/*
				 * If the input is YUV format.
				 */
				if (vout->pfmt != TCC_PFMT_RGB) {
					ret = tcc_get_base_address_of_image(
							vout->src_pix.pixelformat, base0,
							vout->src_pix.width, vout->src_pix.height,
							&base0, &base1, &base2);
				}
			}
		} else if (vout->memory == V4L2_MEMORY_MMAP) {
			base0 = vout->qbufs[buf->index].img_base0;
			base1 = vout->qbufs[buf->index].img_base1;
			base2 = vout->qbufs[buf->index].img_base2;
		} else {
			(void)pr_err("[ERR][VOUT] Invalid v4l2_memory(%d)\n", vout->memory);
			return -100;
		}

		memset(&fbmixer, 0x00, sizeof(fbmixer));

		fbmixer.rsp_type		= WMIXER_POLLING;
		fbmixer.src_img_width	= buf->m.planes[MPLANE_VID].reserved[VID_WIDTH];
		fbmixer.src_img_height	= buf->m.planes[MPLANE_VID].reserved[VID_HEIGHT];

		fbmixer.src_win_left    = buf->m.planes[MPLANE_VID].reserved[VID_CROP_LEFT];
		fbmixer.src_win_top     = buf->m.planes[MPLANE_VID].reserved[VID_CROP_TOP];
		fbmixer.src_win_right   = buf->m.planes[MPLANE_VID].reserved[VID_CROP_LEFT] + buf->m.planes[MPLANE_VID].reserved[VID_CROP_WIDTH];
		fbmixer.src_win_bottom  = buf->m.planes[MPLANE_VID].reserved[VID_CROP_TOP] + buf->m.planes[MPLANE_VID].reserved[VID_CROP_HEIGHT];

		fbmixer.src_y_addr      = base0;
		fbmixer.src_u_addr      = base1;
		fbmixer.src_v_addr      = base2;
		fbmixer.src_fmt         = vout->onthefly_mode ? vioc->rdma.fmt : vioc->m2m_rdma.fmt;

		fbmixer.dst_img_width   = vout->disp_rect.width;
		fbmixer.dst_img_height  = vout->disp_rect.height;
		fbmixer.dst_win_left    = 0;
		fbmixer.dst_win_top     = 0;
		fbmixer.dst_win_right   = vout->disp_rect.width;
		fbmixer.dst_win_bottom  = vout->disp_rect.height;

		if (vout->src_pix.field == V4L2_FIELD_INTERLACED_BT ||
				vout->src_pix.field == V4L2_FIELD_INTERLACED_TB ||
				vout->src_pix.field == V4L2_FIELD_INTERLACED)
			fbmixer.interlaced	= true;
		else
			fbmixer.interlaced	= false;

		if (lastframe_pbuf.size < (fbmixer.dst_img_width*fbmixer.dst_img_height*2)) {
			(void)pr_err("[ERR][VOUT] Not enough wmixer memory(addr:0x%08x size:%d)\n", lastframe_pbuf.base, lastframe_pbuf.size);
			ret = -100;
			goto ERR_CAPTURE_PROCESS;
		}

		fbmixer.dst_y_addr = lastframe_pbuf.base;
		fbmixer.dst_fmt = TCC_LCDC_IMG_FMT_RGB565;

		dprintk("src(%d,%d %dx%d)(%d) (0x%08x 0x%08x 0x%08x)-> dst(%dx%d)(%d) (0x%08x)\n", fbmixer.src_win_left, fbmixer.src_win_top, fbmixer.src_img_width, fbmixer.src_img_height
				, fbmixer.src_fmt, fbmixer.src_y_addr, fbmixer.src_u_addr, fbmixer.src_v_addr, fbmixer.dst_img_width, fbmixer.dst_img_height, fbmixer.dst_fmt, fbmixer.dst_y_addr);

		file = filp_open(WMIXER_PATH, O_RDWR, 0666);
		if (IS_ERR(file)) {
			(void)pr_err("[ERR][VOUT] Can not open %s device\n", WMIXER_PATH);
			ret = -100;
			goto ERR_CAPTURE_PROCESS;
		}

		ret = file->f_op->unlocked_ioctl(file, TCC_WMIXER_ALPHA_SCALING_KERNEL, (unsigned long)&fbmixer);
		filp_close(file, 0);
		if (ret <= 0) {
			(void)pr_err("[ERR][VOUT] Fail TCC_WMIXER_ALPHA_SCALING_KERNEL(%d)\n", ret);
			enable_LastFrame = OFF;
			ret = -100;
			goto ERR_CAPTURE_PROCESS;
		}

		enable_LastFrame = ON;
	} else {
		(void)pr_err("[ERR][VOUT] Null argument\n");
		ret = -100;
		enable_LastFrame = OFF;
	}

ERR_CAPTURE_PROCESS:
	return ret;
}

void vout_disp_last_frame(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;

	if (enable_LastFrame) {
		if (vioc->lastframe_rdma.addr) {
			VIOC_RDMA_SetImageSize(vioc->lastframe_rdma.addr, fbmixer.dst_img_width, fbmixer.dst_img_height);
			VIOC_RDMA_SetImageFormat(vioc->lastframe_rdma.addr, fbmixer.dst_fmt);
			VIOC_RDMA_SetImageOffset(vioc->lastframe_rdma.addr, fbmixer.dst_fmt, fbmixer.dst_img_width);
			VIOC_RDMA_SetImageBase(vioc->lastframe_rdma.addr, fbmixer.dst_y_addr, fbmixer.dst_u_addr, fbmixer.dst_v_addr);
			#ifdef CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV
			VIOC_RDMA_SetImageR2YMode(vioc->lastframe_rdma.addr, 0x2);
			VIOC_RDMA_SetImageR2YEnable(vioc->lastframe_rdma.addr, ON);
			#endif

			VIOC_WMIX_SetPosition(vioc->wmix.addr, vioc->lastframe_rdma.id > VIOC_RDMA03 ? get_vioc_index((vioc->lastframe_rdma.id - VIOC_RDMA03)) : get_vioc_index(vioc->lastframe_rdma.id),
					vout->disp_rect.left, vout->disp_rect.top);
			VIOC_WMIX_SetUpdate(vioc->wmix.addr);

			/* enable hw last-frame layer */
			VIOC_RDMA_SetImageEnable(vioc->lastframe_rdma.addr);
			dprintk("(%d)\n", enable_LastFrame);
		} else {
			(void)pr_err("[ERR][VOUT] Not Allocated HW-resource\n");
		}
	}
}

void vout_video_post_process(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;

	if (vioc->lastframe_rdma.addr) {
		unsigned int enable;

		VIOC_RDMA_GetImageEnable(vioc->lastframe_rdma.addr, &enable);
		if (enable) {
			/* disable hw last-frame layer */
			VIOC_RDMA_SetImageDisableNW(vioc->lastframe_rdma.addr);

			/* reset the position of hw last-frame layer */
			VIOC_WMIX_SetPosition(vioc->wmix.addr, vioc->lastframe_rdma.id > VIOC_RDMA03 ? get_vioc_index((vioc->lastframe_rdma.id - VIOC_RDMA03)) : get_vioc_index(vioc->lastframe_rdma.id), 0, 0);
			VIOC_WMIX_SetUpdate(vioc->wmix.addr);

			dprintk(" Clear HW last-frame layer (0x%p)\n", vioc->lastframe_rdma.addr);
		}
	} else {
		(void)pr_err("[ERR][VOUT] Not Allocated HW-resource\n");
	}
}
#endif

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void vout_subplane_onthefly_init(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;

	vioc->subplane_rdma.fmt = VIOC_IMG_FMT_ARGB8888;
	vioc->subplane_rdma.width = vioc->subplane_alpha.width;
	vioc->subplane_rdma.height = vioc->subplane_alpha.height;

	VIOC_RDMA_SetImageSize(vioc->subplane_rdma.addr, vioc->subplane_rdma.width, vioc->subplane_rdma.height);
	VIOC_RDMA_SetImageFormat(vioc->subplane_rdma.addr, vioc->subplane_rdma.fmt);
	VIOC_RDMA_SetImageOffset(vioc->subplane_rdma.addr, vioc->subplane_rdma.fmt, vioc->subplane_rdma.width);
	VIOC_RDMA_SetImageAlphaSelect(vioc->subplane_rdma.addr, RDMA_ALPHA_ASEL_PIXEL);
	VIOC_RDMA_SetImageAlphaEnable(vioc->subplane_rdma.addr, ON);

	VIOC_WMIX_GetOverlayPriority(vioc->subplane_wmix.addr, &vioc->subplane_wmix.ovp);
}

int vout_subplane_onthefly_qbuf(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	int ret = 0;

	/* check alpha on/off */
	if (vioc->subplane_alpha.on == 0U) {
		vout_subplane_ctrl(vout, 0);
		ret = 1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto onthefly_qbuf_exit;
	}

	if (vioc->subplane_init == 0) {
		vioc->subplane_init = 1;
		vout_subplane_onthefly_init(vout);
	}

	/* check buf changing */
	if (vioc->subplane_buf_index == u32_to_s32(vioc->subplane_alpha.buf_index)) {
		ret = 2;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto onthefly_qbuf_exit;
	}

	vioc->subplane_buf_index = (int)vioc->subplane_alpha.buf_index;

	/* check subtitle size */
	if ((vioc->subplane_rdma.width != vioc->subplane_alpha.width) || (vioc->subplane_rdma.height != vioc->subplane_alpha.height)) {
		vioc->subplane_rdma.width = vioc->subplane_alpha.width;
		vioc->subplane_rdma.height = vioc->subplane_alpha.height;

		/* change rdma base address */
		VIOC_RDMA_SetImageSize(vioc->subplane_rdma.addr, ((vioc->subplane_rdma.width >> 1) << 1), vioc->subplane_rdma.height);
		VIOC_RDMA_SetImageOffset(vioc->subplane_rdma.addr, vioc->subplane_rdma.fmt, vioc->subplane_rdma.width);
	}

	VIOC_RDMA_SetImageBase(vioc->subplane_rdma.addr, vioc->subplane_rdma.img.base0, vioc->subplane_rdma.img.base1, vioc->subplane_rdma.img.base2);

	/* check subtitle start position (x,y) */
	if ((vioc->subplane_wmix.left != vioc->subplane_alpha.offset_x) || (vioc->subplane_wmix.top != vioc->subplane_alpha.offset_y)) {
		vioc->subplane_wmix.left = vioc->subplane_alpha.offset_x;
		vioc->subplane_wmix.top = vioc->subplane_alpha.offset_y;
		VIOC_WMIX_SetPosition(vioc->subplane_wmix.addr, vioc->subplane_wmix.pos, vioc->subplane_wmix.left, vioc->subplane_wmix.top);
		VIOC_WMIX_SetUpdate(vioc->subplane_wmix.addr);
	}
	dprintk("(%d, %d)(%dx%d)\n", vioc->subplane_wmix.left, vioc->subplane_wmix.top, vioc->subplane_rdma.width, vioc->subplane_rdma.height);

	vout_subplane_ctrl(vout, 1);	// on

onthefly_qbuf_exit:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void vout_subplane_m2m_init(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;

	vioc->subplane_buf_index = -1;

	/* reset rdma */
	VIOC_CONFIG_SWReset(vioc->m2m_subplane_rdma.id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(vioc->m2m_subplane_rdma.id, VIOC_CONFIG_CLEAR);

	/* 1. rdma */
	vioc->m2m_subplane_rdma.fmt = VIOC_IMG_FMT_ARGB8888;		// default: ARGB8888
	vioc->m2m_subplane_rdma.width = vout->src_pix.width;		// default: video size
	vioc->m2m_subplane_rdma.height = vout->src_pix.height;
	vioc->m2m_subplane_rdma.rgbswap = 5;							// RGB3, or 5 (BGR3)
	sub_rdma_setup(&vioc->m2m_subplane_rdma);

	/* 2. wmix */
	vioc->m2m_subplane_wmix.left = 0;		// default: to assume sub-plane size same video size
	vioc->m2m_subplane_wmix.top = 0;
	vioc->m2m_subplane_wmix.width = vioc->m2m_rdma.width;
	vioc->m2m_subplane_wmix.height = vioc->m2m_rdma.height;
	m2m_wmix_setup(&vioc->m2m_subplane_wmix);
	print_vioc_subplane_info(vout, "vout_subplane_m2m_init");
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
int vout_subplane_m2m_qbuf(struct tcc_vout_device *vout, struct vioc_alpha *alpha)
{
	int ret = 0;
	struct tcc_vout_vioc *vioc = vout->vioc;

	/* check alpha on/off */
	if (alpha->on == 0U) {
		vout_subplane_ctrl(vout, 0);
		ret = 1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto m2m_qbuf_exit;
	}

	if (vioc->subplane_init == 0) {
		vioc->subplane_init = 1;
		vout_subplane_m2m_init(vout);
	}

	/* check buf changing */
	if (vioc->subplane_buf_index == u32_to_s32(alpha->buf_index)) {
		ret = 2;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto m2m_qbuf_exit;
	}

	vioc->subplane_buf_index = u32_to_s32(alpha->buf_index);

	/* check subtitle size */
	if ((vioc->m2m_subplane_rdma.width != alpha->width) || (vioc->m2m_subplane_rdma.height != alpha->height)) {
		vioc->m2m_subplane_rdma.width = alpha->width;
		vioc->m2m_subplane_rdma.height = alpha->height;

		/* change rdma base address */
		VIOC_RDMA_SetImageSize(vioc->m2m_subplane_rdma.addr, ((vioc->m2m_subplane_rdma.width >> 1) << 1), vioc->m2m_subplane_rdma.height);
		VIOC_RDMA_SetImageOffset(vioc->m2m_subplane_rdma.addr, vioc->m2m_subplane_rdma.fmt, vioc->m2m_subplane_rdma.width);
	}
	/* change rdma base address */
	VIOC_RDMA_SetImageBase(vioc->m2m_subplane_rdma.addr, vioc->m2m_subplane_rdma.img.base0, vioc->m2m_subplane_rdma.img.base1, vioc->m2m_subplane_rdma.img.base2);

	/* check subtitle start position (x,y) */
	if ((vioc->m2m_subplane_wmix.left != alpha->offset_x) || (vioc->m2m_subplane_wmix.top != alpha->offset_y)) {
		vioc->m2m_subplane_wmix.left = alpha->offset_x;
		vioc->m2m_subplane_wmix.top = alpha->offset_y;
		m2m_wmix_setup(&vioc->m2m_subplane_wmix);
	}
	dprintk("(%d, %d)(%dx%d)\n", vioc->m2m_subplane_wmix.left, vioc->m2m_subplane_wmix.top, vioc->m2m_subplane_rdma.width, vioc->m2m_subplane_rdma.height);

	vout_subplane_ctrl(vout, 1);	// on

m2m_qbuf_exit:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void vout_subplane_deinit(struct tcc_vout_device *vout)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct tcc_vout_vioc *vioc = vout->vioc;

	VIOC_RDMA_SetImageDisable((vout->onthefly_mode != 0) ? vioc->subplane_rdma.addr : vioc->m2m_subplane_rdma.addr);
	vioc->subplane_init = 0;
	vioc->subplane_buf_index = -1;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void vout_subplane_ctrl(struct tcc_vout_device *vout, int enable)
{
	struct tcc_vout_vioc *vioc = vout->vioc;

	vioc->subplane_enable = enable;
	if (enable != 0) {
		VIOC_RDMA_SetImageEnable((vout->onthefly_mode != 0)? vioc->subplane_rdma.addr : vioc->m2m_subplane_rdma.addr);
	} else {
		VIOC_RDMA_SetImageDisable((vout->onthefly_mode != 0) ? vioc->subplane_rdma.addr : vioc->m2m_subplane_rdma.addr);
	}
	dprintk("%d\n", enable);
}

#ifdef CONFIG_VOUT_USE_VSYNC_INT
/*
 * kernel timer functions
 */
void ktimer_handler(unsigned long arg)
{
	struct tcc_vout_device *vout = (struct tcc_vout_device *)arg;

next_frame:
	if (vout->firstFieldFlag) {
		vout->firstFieldFlag = 0;
		vout->display_done = ON;
	}

	if (vout->display_done && vout->last_cleared_buffer) {
		vout_clear_buffer(vout, vout->last_cleared_buffer);

		/* update DD flag */
		vout->display_done = OFF;
	}

	if ((atomic_read(&vout->readable_buff_count) > 0)) {
		int ret = 0;
		struct v4l2_buffer *pNextBuffer = vout_get_buffer(vout);

		if (pNextBuffer->reserved2 || vout->force_sync) {
			if (vout->force_sync)
				vout->force_sync = OFF;
			tcc_vout_set_time(vout, ((pNextBuffer->timestamp.tv_sec*1000)+(pNextBuffer->timestamp.tv_usec/1000)));
		}

		/* check video time */
		ret = vout_check_syncTime(vout, pNextBuffer, tcc_vout_get_time(vout));

		if (ret == VOUT_DRV_ERR_DROPFRM) {
			if ((atomic_read(&vout->readable_buff_count)-1) == 0)
				goto force_clear;

			vout_pop_buffer(vout);
			vout->last_cleared_buffer = pNextBuffer;
			vout->display_done = ON;
			goto next_frame;
		} else if (ret == VOUT_DRV_ERR_WAITFRM) {
			goto end_ktimer;
		}
force_clear:
		vout_pop_buffer(vout);
		vout->last_cleared_buffer = pNextBuffer;
		vout->display_done = ON;
	}
end_ktimer:
	if (vout->ktimer_enable) {
		vout->ktimer.expires = get_jiffies_64() + 3*HZ/1000;
		add_timer(&vout->ktimer);
	}
}

void vout_ktimer_ctrl(struct tcc_vout_device *vout, unsigned int onoff)
{
	if (onoff) {
		memset(&vout->ktimer, 0x0, sizeof(struct timer_list));

		init_timer(&vout->ktimer);
		vout->ktimer.expires = get_jiffies_64() + 3*HZ/1000;
		vout->ktimer.data = (unsigned long)vout;
		vout->ktimer.function = ktimer_handler;

		vout->ktimer_enable = ON;
		add_timer(&vout->ktimer);

		// for interlace
		vout->frame_count = 0;
	} else {
		vout->ktimer_enable = OFF;
		msleep(10);
		del_timer(&vout->ktimer);
	}
	dprintk("(%d)\n", onoff);
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(vout_ktimer_ctrl);
#endif

/* call by tcc_vout_open()
 */
int vout_vioc_init(struct tcc_vout_device *vout)
{
	int ret = 0;
	struct tcc_vout_vioc *vioc = vout->vioc;

	ret = vout_vioc_set_default(vout);
	if (ret < 0) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_device_off;
	}

	if (VIOC_DISP_Get_TurnOnOff(vioc->disp.addr) == OFF) {
		dprintk("Please turn on the output device.\n");
		ret = -EBUSY;
		goto err_device_off;
	}

	/* Get WMIX size
	 */
	vout_wmix_getsize(vout, &vioc->wmix.width, &vioc->wmix.height);
	vout->disp_rect.left = 0;
	vioc->wmix.left = 0;
	vout->disp_rect.top = 0;
	vioc->wmix.top = 0;
	vout->disp_rect.width = vioc->wmix.width;
	vout->disp_rect.height = vioc->wmix.height;
	if ((vout->disp_rect.width == 0U) || (vout->disp_rect.height == 0U)) {
		dprintk("output device size(%dx%d)\n", vout->disp_rect.width, vout->disp_rect.height);
		ret = -EBUSY;
		goto err_device_off;
	}

	/* first field flag */
	vout->firstFieldFlag = 0;

	vout->nr_qbufs = 0; // current number of created buffers
	vout->mapped = 0;

	// onthefly
	vout->display_done = 0;
	vout->last_displayed_buf_idx = 0;

	if (vout_video_display_enable(vout) < 0) {
		ret = -EBUSY;
		goto err_buf_init;
	}

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	memset(&gDRM_packet, 0x0, sizeof(DRM_Packet_t));
#endif

	#ifdef CONFIG_VOUT_KEEP_VIDEO_LAYER
	vout->is_viqe_shared = 0;
	#endif

	dprintk("\n");
	/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
	goto init_exit;

err_buf_init:
	vout_video_display_disable(vout);

err_device_off:
	#ifdef CONFIG_VOUT_USE_VSYNC_INT
	if (vout->vout_timer) {
		tcc_unregister_timer(vout->vout_timer);
		vout->vout_timer = NULL;
	}
	kfree(vioc->disp.vioc_intr);
	#endif
	kfree(vioc->m2m_wdma.vioc_intr);
init_exit:
	return ret;
}

void vout_deinit(struct tcc_vout_device *vout)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct tcc_vout_vioc *vioc = vout->vioc;

	VIOC_RDMA_SetImageSize(vioc->rdma.addr, 0, 0);
	VIOC_RDMA_SetImageDisable(vioc->rdma.addr);
	#ifdef CONFIG_VOUT_USE_VSYNC_INT
	if (vout->ktimer_enable) {
		vout->ktimer_enable = OFF;
		del_timer(&vout->ktimer);
	}
	#endif

	vout_video_display_disable(vout);

	#ifdef CONFIG_VOUT_USE_VSYNC_INT
	if (vout->vout_timer) {
		tcc_unregister_timer(vout->vout_timer);
		vout->vout_timer = NULL;
	}
	#endif

#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
	{
		DRM_Packet_t drmparm;

		memset(&drmparm, 0x0, sizeof(DRM_Packet_t));
		hdmi_set_drm(&drmparm);
	}
#endif
	dprintk("\n");
}


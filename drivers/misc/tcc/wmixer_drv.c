// SPDX-License-Identifier: GPL-2.0-or-later
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/limits.h>
#include <linux/compat.h>

#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/tcc_wmixer_ioctrl.h>


#include <video/tcc/vioc_intr.h>
#include <video/tcc/tcc_types.h>

#define WMIXER_DEBUG   0
#ifdef WMIXER_DEBUG
#define dprintk(msg...) pr_info("[DBG][WMIXER] " msg)
#else
#define dprintk(msg...)
#endif

#ifdef CONFIG_VIOC_MAP_DECOMP
#include <video/tcc/tca_map_converter.h>
static unsigned int gMC_NUM;
#endif

struct wmixer_data {
	// wait for poll
	wait_queue_head_t   poll_wq;
	spinlock_t      poll_lock;
	unsigned int        poll_count;

	// wait for ioctl command
	wait_queue_head_t   cmd_wq;
	spinlock_t      cmd_lock;
	unsigned int        cmd_count;

	struct mutex        io_mutex;
	unsigned char       block_operating;
	unsigned char       block_waiting;
	unsigned char       irq_reged;
	unsigned int        dev_opened;
};

struct wmixer_drv_vioc {
	void __iomem *reg;
	unsigned int id;
	unsigned int vioc_path;
};

struct wmixer_drv_type {
	struct vioc_intr_type   *vioc_intr;

	unsigned int        id;
	unsigned int        irq;

	struct miscdevice   *misc;

	struct wmixer_drv_vioc  rdma0;
	struct wmixer_drv_vioc  rdma1;
	struct wmixer_drv_vioc  wmix;
	struct wmixer_drv_vioc  sc;
	struct wmixer_drv_vioc  wdma;

	struct clk      *wmix_clk;
	struct wmixer_data  *data;
	WMIXER_INFO_TYPE    *info;
	WMIXER_ALPHASCALERING_INFO_TYPE alpha_scalering;
	WMIXER_ALPHABLENDING_TYPE   alpha_blending;
	unsigned char       scaler_plug_status;
};

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int wmixer_drv_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = 0;

	(void)filp;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if(vma->vm_end >= vma->vm_start) {
		if ((bool)remap_pfn_range(
			vma, vma->vm_start, vma->vm_pgoff,
			vma->vm_end - vma->vm_start,
			vma->vm_page_prot)) {
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				ret = -EAGAIN;
				goto err_remap_pfn_range;
			}
	}

	vma->vm_ops =  NULL;
	vma->vm_flags |= (unsigned long)VM_IO;
	vma->vm_flags |= (unsigned long)VM_DONTEXPAND | (unsigned long)VM_PFNMAP;

err_remap_pfn_range:
	return ret;
}

static int wmixer_drv_ctrl(struct wmixer_drv_type *wmixer)
{
	WMIXER_INFO_TYPE    *wmix_info = wmixer->info;
	void __iomem *pWMIX_rdma_base = wmixer->rdma0.reg;
	void __iomem *pWMIX_wmix_base = wmixer->wmix.reg;
	void __iomem *pWMIX_wdma_base = wmixer->wdma.reg;

	int ret = 0;
	unsigned int pSrcBase0 = 0, pSrcBase1 = 0, pSrcBase2 = 0;
	unsigned int pDstBase0 = 0, pDstBase1 = 0, pDstBase2 = 0;
	unsigned int crop_width;

	(void)dprintk("%s() : name:%s\n", __func__, wmixer->misc->name);
	(void)dprintk(
		"Src  : addr:0x%x 0x%x 0x%x  fmt:%d (swap:ext=%d/%d), crop(%d,%d ~ %dx%d)\n",
		wmix_info->src_y_addr, wmix_info->src_u_addr,
		wmix_info->src_v_addr, wmix_info->src_fmt,
		wmix_info->src_rgb_swap, wmix_info->src_fmt_ext_info,
		wmix_info->src_win_left, wmix_info->src_win_top,
		wmix_info->src_win_right, wmix_info->src_win_bottom);
	(void)dprintk(
		"Dest : addr:0x%x 0x%x 0x%x  fmt:%d (swap:ext=%d/%d), crop(%d,%d ~ %dx%d)\n",
		wmix_info->dst_y_addr, wmix_info->dst_u_addr,
		wmix_info->dst_v_addr, wmix_info->dst_fmt,
		wmix_info->dst_rgb_swap, wmix_info->dst_fmt_ext_info,
		wmix_info->dst_win_left, wmix_info->dst_win_top,
		wmix_info->dst_win_right, wmix_info->dst_win_bottom);
	(void)dprintk(
		"Size : SRC_W:%d  SRC_H:%d DST_W:%d DST_H:%d.\n",
		wmix_info->src_img_width, wmix_info->src_img_height,
		wmix_info->dst_img_width, wmix_info->dst_img_height);

	if (wmixer->scaler_plug_status == 1U) {
		wmixer->scaler_plug_status = 0;
		(void)VIOC_CONFIG_PlugOut(wmixer->sc.id);
	}

	pSrcBase0 = wmix_info->src_y_addr;
	pSrcBase1 = wmix_info->src_u_addr;
	pSrcBase2 = wmix_info->src_v_addr;
	if (((wmix_info->src_win_right) > (wmix_info->src_win_left)) && (wmix_info->src_fmt < 256U)) {
		/* Prevent KCS */
		if ((wmix_info->src_img_width !=
			(wmix_info->src_win_right - wmix_info->src_win_left))
			|| (wmix_info->src_img_height !=
			(wmix_info->src_win_bottom - wmix_info->src_win_top))) {
			crop_width = wmix_info->src_win_right - wmix_info->src_win_left;
			wmix_info->src_win_left = (wmix_info->src_win_left >> 3) << 3;
			wmix_info->src_win_right = wmix_info->src_win_left + crop_width;
			tcc_get_addr_yuv(
				wmix_info->src_fmt, wmix_info->src_y_addr,
				wmix_info->src_img_width, wmix_info->src_img_height,
				wmix_info->src_win_left, wmix_info->src_win_top,
				&pSrcBase0, &pSrcBase1, &pSrcBase2);
		}
	}
	if (((wmix_info->dst_win_right) > (wmix_info->dst_win_left)) && (wmix_info->src_fmt < 256U)) {
		/* Prevent KCS */
		if ((wmix_info->dst_img_width !=
			(wmix_info->dst_win_right - wmix_info->dst_win_left)) ||
			(wmix_info->dst_img_height !=
			(wmix_info->dst_win_bottom - wmix_info->dst_win_top))) {
			crop_width = wmix_info->dst_win_right - wmix_info->dst_win_left;
			wmix_info->dst_win_left = (wmix_info->dst_win_left >> 3) << 3;
			wmix_info->dst_win_right = wmix_info->dst_win_left + crop_width;
			tcc_get_addr_yuv(
				wmix_info->dst_fmt, wmix_info->dst_y_addr,
				wmix_info->dst_img_width, wmix_info->dst_img_height,
				wmix_info->dst_win_left, wmix_info->dst_win_top,
				&pDstBase0, &pDstBase1, &pDstBase2);
		}
	}

	spin_lock_irq(&(wmixer->data->cmd_lock));

	#ifdef CONFIG_VIOC_MAP_DECOMP
	if (wmix_info->src_fmt_ext_info == 0x10U) {
		gMC_NUM = 1;
		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			int component_num =
				VIOC_CONFIG_DMAPath_Select(wmixer->rdma0.id);
			if(component_num >= 0) {
				/* prevent KCS */
				if ((VIOC_MC0 + gMC_NUM) != (unsigned int)component_num) {
					(void)VIOC_CONFIG_DMAPath_UnSet(component_num);
					tca_map_convter_swreset(VIOC_MC0 + gMC_NUM);
				}
			}

			(void)VIOC_CONFIG_DMAPath_Set(
				wmixer->rdma0.id, (VIOC_MC0 + gMC_NUM));
		} else {
			#if defined(CONFIG_ARCH_TCC803X) \
				|| defined(CONFIG_ARCH_TCC805X)
			(void)VIOC_CONFIG_MCPath(
				wmixer->wmix.id, (VIOC_MC0 + gMC_NUM));
			#endif
		}
		tca_map_convter_driver_set(
			VIOC_MC0 + gMC_NUM,
			wmix_info->src_img_width,
			wmix_info->src_img_height,
			wmix_info->src_win_left,
			wmix_info->src_win_top,
			(wmix_info->src_win_right - wmix_info->src_win_left),
			(wmix_info->src_win_bottom - wmix_info->src_win_top),
			0, &wmix_info->mapConv_info);
		tca_map_convter_onoff(VIOC_MC0 + gMC_NUM, 1, 0);
	} else
	#endif
	{
		VIOC_RDMA_SetImageOffset(pWMIX_rdma_base,
				wmix_info->src_fmt, wmix_info->src_img_width);

		// set to RDMA
		VIOC_RDMA_SetImageFormat(pWMIX_rdma_base, wmix_info->src_fmt);
		VIOC_RDMA_SetImageSize(pWMIX_rdma_base,
			(wmix_info->src_win_right - wmix_info->src_win_left),
			(wmix_info->src_win_bottom - wmix_info->src_win_top));
		VIOC_RDMA_SetImageBase(
			pWMIX_rdma_base, pSrcBase0, pSrcBase1,  pSrcBase2);
	}

	// set to WMIX
	VIOC_WMIX_SetSize(pWMIX_wmix_base,
		(wmix_info->dst_win_right - wmix_info->dst_win_left),
		(wmix_info->dst_win_bottom - wmix_info->dst_win_top));
	VIOC_WMIX_SetUpdate(pWMIX_wmix_base);

	// set to RDMA
	if (((wmix_info->src_fmt > VIOC_IMG_FMT_COMP)
		&& (wmix_info->dst_fmt > VIOC_IMG_FMT_COMP))
		|| ((wmix_info->src_fmt < VIOC_IMG_FMT_COMP)
		&& (wmix_info->dst_fmt < VIOC_IMG_FMT_COMP))) {
		VIOC_RDMA_SetImageY2REnable(pWMIX_rdma_base, 0);
	} else {
		if (((wmix_info->src_fmt) > VIOC_IMG_FMT_COMP)
			&& ((wmix_info->dst_fmt) < VIOC_IMG_FMT_COMP)) {
			VIOC_RDMA_SetImageY2REnable(pWMIX_rdma_base, 1);
			VIOC_RDMA_SetImageY2RMode(pWMIX_rdma_base, 0x2);
		}
	}

	if (wmix_info->src_fmt < VIOC_IMG_FMT_COMP) {
		/* Prevent KCS */
		VIOC_RDMA_SetImageRGBSwapMode(
			pWMIX_rdma_base, wmix_info->src_rgb_swap);
	}
	else {
		/* Prevent KCS */
		VIOC_RDMA_SetImageRGBSwapMode(pWMIX_rdma_base, 0);
	}

	VIOC_RDMA_SetImageEnable(pWMIX_rdma_base); // Soc guide info.

	VIOC_WDMA_SetImageOffset(pWMIX_wdma_base,
			wmix_info->dst_fmt, wmix_info->dst_img_width);


	VIOC_WDMA_SetImageFormat(pWMIX_wdma_base,
		wmix_info->dst_fmt);
	VIOC_WDMA_SetImageSize(pWMIX_wdma_base,
		(wmix_info->dst_win_right - wmix_info->dst_win_left),
		(wmix_info->dst_win_bottom - wmix_info->dst_win_top));

	VIOC_WDMA_SetImageBase(pWMIX_wdma_base,
		wmix_info->dst_y_addr, wmix_info->dst_u_addr,
		wmix_info->dst_v_addr);

	if (wmix_info->src_fmt == VIOC_IMG_FMT_COMP) {
		if (wmix_info->dst_fmt < VIOC_IMG_FMT_COMP) {
			VIOC_WDMA_SetImageR2YEnable(pWMIX_wdma_base, 0);
			VIOC_WDMA_SetImageY2REnable(pWMIX_wdma_base, 1);
			VIOC_WDMA_SetImageY2RMode(pWMIX_wdma_base, 2);
		} else {
			VIOC_WDMA_SetImageR2YEnable(pWMIX_wdma_base, 0);
			VIOC_WDMA_SetImageY2REnable(pWMIX_wdma_base, 0);
		}
	} else if (((wmix_info->src_fmt > VIOC_IMG_FMT_COMP)
		&& (wmix_info->dst_fmt > VIOC_IMG_FMT_COMP))
		|| ((wmix_info->src_fmt < VIOC_IMG_FMT_COMP)
		&& (wmix_info->dst_fmt < VIOC_IMG_FMT_COMP))) {
		VIOC_WDMA_SetImageR2YEnable(pWMIX_wdma_base, 0);
		VIOC_WDMA_SetImageY2REnable(pWMIX_wdma_base, 0);
	} else {
		if ((wmix_info->src_fmt < VIOC_IMG_FMT_COMP)
			&& (wmix_info->dst_fmt > VIOC_IMG_FMT_COMP)) {
			VIOC_WDMA_SetImageY2REnable(pWMIX_wdma_base, 0);
			VIOC_WDMA_SetImageR2YEnable(pWMIX_wdma_base, 1);
			VIOC_WDMA_SetImageR2YMode(pWMIX_wdma_base, 1);
		} else {
			VIOC_WDMA_SetImageR2YEnable(pWMIX_wdma_base, 0);
			VIOC_WDMA_SetImageY2REnable(pWMIX_wdma_base, 0);
		}
	}

	if (wmix_info->dst_fmt < VIOC_IMG_FMT_COMP) {
		/* Prevent KCS */
		VIOC_WDMA_SetImageRGBSwapMode(
			pWMIX_wdma_base, wmix_info->dst_rgb_swap);
	}
	else {
		/* Prevent KCS */
		VIOC_WDMA_SetImageRGBSwapMode(pWMIX_wdma_base, 0);
	}

	VIOC_WDMA_SetImageEnable(pWMIX_wdma_base, 0/*OFF*/);
	(void)vioc_intr_clear(wmixer->vioc_intr->id, wmixer->vioc_intr->bits);
	spin_unlock_irq(&(wmixer->data->cmd_lock));

	if (wmix_info->rsp_type == (unsigned int)WMIXER_POLLING) {
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
		/* coverity[cert_int02_c_violation : FALSE] */
		/* coverity[cert_int31_c_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		ret = wait_event_interruptible_timeout(
			wmixer->data->poll_wq,
			wmixer->data->block_operating == 0U,
			msecs_to_jiffies(500));
		if (ret <= 0) {
			wmixer->data->block_operating = 0;
			(void)pr_warn(
				"[WAR][WMIXER] %s(): %s time-out: %d, line: %d.\n",
				__func__, wmixer->misc->name, ret, __LINE__);
		}
	}

//VIOC_RDMA_DUMP(pWMIX_rdma_base);
//VIOC_WMIX_DUMP(pWMIX_wmix_base);
//VIOC_WDMA_DUMP(pWMIX_wdma_base);

	return ret;
}

static int wmixer_drv_alpha_scaling_ctrl(struct wmixer_drv_type *wmixer)
{
	WMIXER_ALPHASCALERING_INFO_TYPE *aps_info =
		&wmixer->alpha_scalering;
	void __iomem *pWMIX_rdma_base = wmixer->rdma0.reg;
	void __iomem *pWMIX_wmix_base = wmixer->wmix.reg;
	void __iomem *pWMIX_wdma_base = wmixer->wdma.reg;
	void __iomem *pWMIX_sc_base = wmixer->sc.reg;

	int ret = 0;
	unsigned int pSrcBase0 = 0, pSrcBase1 = 0, pSrcBase2 = 0;
	unsigned int pDstBase0 = 0, pDstBase1 = 0, pDstBase2 = 0;
	unsigned int crop_width;

	(void)dprintk(
		"%s() : name: %s\n", __func__,
		wmixer->misc->name);
#if defined(CONFIG_VIOC_MAP_DECOMP)
	(void)dprintk(
		"etc  : mc_num:%d, fmt_info:%d/%d, inter:%d.\n",
		aps_info->mc_num, aps_info->src_fmt_ext_info,
		aps_info->dst_fmt_ext_info, aps_info->interlaced);
#endif
	(void)dprintk(
		"Src  : addr:0x%x, 0x%x, 0x%x,  fmt:%d.\n",
		aps_info->src_y_addr, aps_info->src_u_addr,
		aps_info->src_v_addr, aps_info->src_fmt);
	(void)dprintk(
		"Dst  : addr:0x%x, 0x%x, 0x%x,  fmt:%d.\n",
		aps_info->dst_y_addr, aps_info->dst_u_addr,
		aps_info->dst_v_addr, aps_info->dst_fmt);
	(void)dprintk(
		"Size : src_w:%d, src_h:%d, src_crop_left:%d, src_crop_top:%d, src_crop_right:%d, src_crop_bottom:%d\n dst_w:%d, dst_h:%d, dst_crop_left:%d, dst_crop_top:%d, dst_crop_right:%d, dst_crop_bottom:%d.\n",
		aps_info->src_img_width, aps_info->src_img_height,
		aps_info->src_win_left, aps_info->src_win_top,
		aps_info->src_win_right, aps_info->src_win_bottom,
		aps_info->dst_img_width, aps_info->dst_img_height,
		aps_info->dst_win_left, aps_info->dst_win_top,
		aps_info->dst_win_right, aps_info->dst_win_bottom);

	pSrcBase0 = aps_info->src_y_addr;
	pSrcBase1 = aps_info->src_u_addr;
	pSrcBase2 = aps_info->src_v_addr;
	if ((aps_info->src_win_right > aps_info->src_win_left) && (aps_info->src_fmt < 256U)) {
		/* prevent KCS */
		if ((aps_info->src_img_width !=
			(aps_info->src_win_right - aps_info->src_win_left)) ||
			(aps_info->src_img_height !=
			(aps_info->src_win_bottom - aps_info->src_win_top))) {
			crop_width = aps_info->src_win_right - aps_info->src_win_left;
			aps_info->src_win_left = (aps_info->src_win_left >> 3) << 3;
			aps_info->src_win_right = aps_info->src_win_left + crop_width;
			tcc_get_addr_yuv(
				aps_info->src_fmt, aps_info->src_y_addr,
				aps_info->src_img_width, aps_info->src_img_height,
				aps_info->src_win_left, aps_info->src_win_top,
				&pSrcBase0, &pSrcBase1, &pSrcBase2);
		}
	}

	pDstBase0 = aps_info->dst_y_addr;
	pDstBase1 = aps_info->dst_u_addr;
	pDstBase2 = aps_info->dst_v_addr;
	if ((aps_info->dst_img_width !=
		(aps_info->dst_win_right - aps_info->dst_win_left)) ||
		(aps_info->dst_img_height !=
		(aps_info->dst_win_bottom - aps_info->dst_win_top))) {
		crop_width = aps_info->dst_win_right - aps_info->dst_win_left;
		aps_info->dst_win_left = (aps_info->dst_win_left >> 3) << 3;
		aps_info->dst_win_right = aps_info->dst_win_left + crop_width;
		tcc_get_addr_yuv(
			aps_info->dst_fmt, aps_info->dst_y_addr,
			aps_info->dst_img_width, aps_info->dst_img_height,
			aps_info->dst_win_left, aps_info->dst_win_top,
			&pDstBase0, &pDstBase1, &pDstBase2);
	}
	(void)dprintk(
		"=> Src  : addr:0x%x, 0x%x, 0x%x,  fmt:%d.\n",
		pSrcBase0, pSrcBase1,
		pSrcBase2, aps_info->src_fmt);
	(void)dprintk(
		"=> Dst  : addr:0x%x, 0x%x, 0x%x,  fmt:%d.\n",
		pDstBase0, pDstBase1,
		pDstBase2, aps_info->dst_fmt);

	spin_lock_irq(&(wmixer->data->cmd_lock));

#ifdef CONFIG_VIOC_MAP_DECOMP
	if (aps_info->src_fmt_ext_info == 0x10U) {
		gMC_NUM = aps_info->mc_num;
		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			int component_num =
				VIOC_CONFIG_DMAPath_Select(wmixer->rdma0.id);
			if(component_num >= 0) {
				/* Prevent KCS */
				if ((VIOC_MC0 + gMC_NUM) != (unsigned int)component_num) {
					(void)VIOC_CONFIG_DMAPath_UnSet(component_num);
					tca_map_convter_swreset((VIOC_MC0 + gMC_NUM));
				}
			}

			(void)VIOC_CONFIG_DMAPath_Set(
				wmixer->rdma0.id, (VIOC_MC0 + gMC_NUM));

		} else {
			#if defined(CONFIG_ARCH_TCC803X) \
				|| defined(CONFIG_ARCH_TCC805X)
			(void)VIOC_CONFIG_MCPath(
				wmixer->wmix.id, (VIOC_MC0 + gMC_NUM));
			#endif
		}

		tca_map_convter_driver_set(
			VIOC_MC0 + gMC_NUM, aps_info->src_img_width,
			aps_info->src_img_height,
			aps_info->src_win_left, aps_info->src_win_top,
			(aps_info->src_win_right - aps_info->src_win_left),
			(aps_info->src_win_bottom - aps_info->src_win_top),
			0, &aps_info->mapConv_info);
		tca_map_convter_onoff(VIOC_MC0 + gMC_NUM, 1, 0);
	} else
#endif
	{
		VIOC_RDMA_SetImageAlphaSelect(pWMIX_rdma_base, 1);
		VIOC_RDMA_SetImageAlphaEnable(pWMIX_rdma_base, 1);
		VIOC_RDMA_SetImageFormat(pWMIX_rdma_base, aps_info->src_fmt);

		//interlaced frame process ex) MPEG2
		if (aps_info->interlaced != 0U) {
			/* Prevent KCS */
			VIOC_RDMA_SetImageOffset(pWMIX_rdma_base,
				aps_info->src_fmt,
				aps_info->src_img_width*2U);
		}
		else {
			/* Prevent KCS */
			VIOC_RDMA_SetImageOffset(pWMIX_rdma_base,
				aps_info->src_fmt, aps_info->src_img_width);
		}

		if (aps_info->interlaced != 0U) {
			/* Prevent KCS */
			VIOC_RDMA_SetImageSize(pWMIX_rdma_base,
				(aps_info->src_win_right -
				aps_info->src_win_left),
				(aps_info->src_win_bottom -
				aps_info->src_win_top)/2U);
		}
		else {
			/* Prevent KCS */
			VIOC_RDMA_SetImageSize(pWMIX_rdma_base,
			(aps_info->src_win_right - aps_info->src_win_left),
			(aps_info->src_win_bottom - aps_info->src_win_top));
		}

		VIOC_RDMA_SetImageBase(pWMIX_rdma_base,
			pSrcBase0, pSrcBase1,  pSrcBase2);
		VIOC_RDMA_SetImageEnable(pWMIX_rdma_base);
	}

	wmixer->scaler_plug_status = 1;
	VIOC_SC_SetBypass(pWMIX_sc_base, 0U);
	VIOC_SC_SetOutSize(pWMIX_sc_base,
	(aps_info->dst_win_right - aps_info->dst_win_left),
	(aps_info->dst_win_bottom - aps_info->dst_win_top));
	VIOC_SC_SetOutPosition(
		pWMIX_sc_base, 0, 0);
	VIOC_SC_SetDstSize(pWMIX_sc_base,
			(aps_info->dst_win_right - aps_info->dst_win_left),
			(aps_info->dst_win_bottom - aps_info->dst_win_top));
	(void)VIOC_CONFIG_PlugIn(wmixer->sc.id, wmixer->rdma0.id);
	VIOC_SC_SetUpdate(pWMIX_sc_base);

	// wmixer op is by-pass mode.
	(void)VIOC_CONFIG_WMIXPath(wmixer->rdma0.id, 0);

	VIOC_WMIX_SetSize(pWMIX_wmix_base,
		(aps_info->dst_win_right - aps_info->dst_win_left),
		(aps_info->dst_win_bottom - aps_info->dst_win_top));
	VIOC_WMIX_SetUpdate(pWMIX_wmix_base);

	VIOC_WDMA_SetImageOffset(
			pWMIX_wdma_base, aps_info->dst_fmt,
			aps_info->dst_img_width);

	VIOC_WDMA_SetImageFormat(pWMIX_wdma_base, aps_info->dst_fmt);
	VIOC_WDMA_SetImageSize(pWMIX_wdma_base,
		(aps_info->dst_win_right - aps_info->dst_win_left),
		(aps_info->dst_win_bottom - aps_info->dst_win_top));
	VIOC_WDMA_SetImageBase(pWMIX_wdma_base,
		pDstBase0, pDstBase1, pDstBase2);
	if ((aps_info->src_fmt < VIOC_IMG_FMT_COMP)
		&& (aps_info->dst_fmt > VIOC_IMG_FMT_COMP)) {
		VIOC_WDMA_SetImageR2YEnable(pWMIX_wdma_base, 1);
		VIOC_WDMA_SetImageY2REnable(pWMIX_wdma_base, 0);
	} else if ((aps_info->src_fmt >= VIOC_IMG_FMT_COMP)
		&& (aps_info->dst_fmt < VIOC_IMG_FMT_COMP)) {
		VIOC_WDMA_SetImageR2YEnable(pWMIX_wdma_base, 0);
		VIOC_WDMA_SetImageY2REnable(pWMIX_wdma_base, 1);
	} else {
		VIOC_WDMA_SetImageR2YEnable(pWMIX_wdma_base, 0);
		VIOC_WDMA_SetImageY2REnable(pWMIX_wdma_base, 0);
	}

	VIOC_WDMA_SetImageEnable(pWMIX_wdma_base, 0/*OFF*/);
	(void)vioc_intr_clear(wmixer->vioc_intr->id, wmixer->vioc_intr->bits);
	spin_unlock_irq(&(wmixer->data->cmd_lock));

	if (aps_info->rsp_type == (unsigned int)WMIXER_POLLING) {
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
		/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
		/* coverity[cert_int02_c_violation : FALSE] */
		/* coverity[cert_int31_c_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		ret = wait_event_interruptible_timeout(
			wmixer->data->poll_wq,
			wmixer->data->block_operating == 0U,
			msecs_to_jiffies(500));
		if (ret <= 0) {
			wmixer->data->block_operating = 0;
#if defined(CONFIG_VIOC_MAP_DECOMP)
			(void)pr_warn(
				"[WAR][WMIXER] %s(): %s time-out: %d, line: %d compressor_num 0x%x/%d.\n",
				__func__, wmixer->misc->name, ret,
				__LINE__, aps_info->src_fmt_ext_info,
				aps_info->mc_num);
#else
			(void)pr_warn(
				"[WAR][WMIXER] %s(): %s time-out: %d, line: %d.\n",
				__func__, wmixer->misc->name, ret, __LINE__);
#endif
		}
	}

	return ret;
}

static int wmixer_drv_alpha_mixing_ctrl(struct wmixer_drv_type *wmixer)
{
	const WMIXER_ALPHABLENDING_TYPE *apb_info =
		(WMIXER_ALPHABLENDING_TYPE *)&wmixer->alpha_blending;
	void __iomem *pWMIX_rdma_base = wmixer->rdma0.reg;
	void __iomem *pWMIX_rdma1_base = wmixer->rdma1.reg;
	void __iomem *pWMIX_wmix_base = wmixer->wmix.reg;
	void __iomem *pWMIX_wdma_base = wmixer->wdma.reg;
	void __iomem *pWMIX_sc_base = wmixer->sc.reg;
	int ret = 0;

	(void)dprintk("%s(): name:%s\n", __func__, wmixer->misc->name);

	spin_lock_irq(&(wmixer->data->cmd_lock));

	VIOC_RDMA_SetImageAlphaEnable(pWMIX_rdma_base, 1);
	VIOC_RDMA_SetImageAlphaSelect(pWMIX_rdma_base, 1);
	VIOC_RDMA_SetImageFormat(pWMIX_rdma_base, apb_info->src0_fmt);

	if (apb_info->src0_fmt > VIOC_IMG_FMT_COMP) {
		VIOC_RDMA_SetImageY2REnable(pWMIX_rdma_base, 1);
		VIOC_RDMA_SetImageR2YEnable(pWMIX_rdma_base, 0);
	} else {
		VIOC_RDMA_SetImageY2REnable(pWMIX_rdma_base, 0);
	}

	VIOC_RDMA_SetImageSize(pWMIX_rdma_base,
		apb_info->src0_width, apb_info->src0_height);
	VIOC_RDMA_SetImageOffset(pWMIX_rdma_base,
		apb_info->src0_fmt, apb_info->src0_width);
	VIOC_RDMA_SetImageBase(pWMIX_rdma_base,
		apb_info->src0_Yaddr,
		apb_info->src0_Uaddr, apb_info->src0_Vaddr);

	VIOC_RDMA_SetImageAlphaEnable(pWMIX_rdma1_base, 1);
	VIOC_RDMA_SetImageAlphaSelect(pWMIX_rdma1_base, 1);
	VIOC_RDMA_SetImageFormat(pWMIX_rdma1_base, apb_info->src1_fmt);

	if (apb_info->src1_fmt > VIOC_IMG_FMT_COMP) {
		VIOC_RDMA_SetImageY2REnable(pWMIX_rdma1_base, 1);
		VIOC_RDMA_SetImageR2YEnable(pWMIX_rdma1_base, 0);
	} else {
		VIOC_RDMA_SetImageY2REnable(pWMIX_rdma1_base, 0);
	}

	VIOC_RDMA_SetImageSize(pWMIX_rdma1_base,
		apb_info->src1_width, apb_info->src1_height);
	VIOC_RDMA_SetImageOffset(pWMIX_rdma1_base,
		apb_info->src1_fmt, apb_info->src1_width);
	VIOC_RDMA_SetImageBase(pWMIX_rdma1_base,
		apb_info->src1_Yaddr, apb_info->src1_Uaddr,
		apb_info->src1_Vaddr);

	if (wmixer->sc.reg != NULL) {
		wmixer->scaler_plug_status = 1;
		if (apb_info->src0_use_scaler == 0U) {
			VIOC_SC_SetBypass(pWMIX_sc_base, 1U);
			VIOC_SC_SetDstSize(pWMIX_sc_base,
				apb_info->dst_width, apb_info->dst_height);
			VIOC_SC_SetOutSize(pWMIX_sc_base,
				apb_info->dst_width, apb_info->dst_height);
		} else {
			VIOC_SC_SetBypass(pWMIX_sc_base, 0U);
			VIOC_SC_SetDstSize(pWMIX_sc_base,
				apb_info->src0_dst_width, apb_info->src0_dst_height);
			VIOC_SC_SetOutSize(pWMIX_sc_base,
				apb_info->src0_dst_width, apb_info->src0_dst_height);
		}
		VIOC_SC_SetOutPosition(pWMIX_sc_base, 0, 0);
		(void)VIOC_CONFIG_PlugIn(wmixer->sc.id, wmixer->rdma0.id);
		VIOC_SC_SetUpdate(pWMIX_sc_base);
		VIOC_RDMA_SetImageEnable(pWMIX_rdma_base); // SoC guide info.
		VIOC_RDMA_SetImageEnable(pWMIX_rdma1_base);
	} else {
		VIOC_RDMA_SetImageEnable(pWMIX_rdma_base); // SoC guide info.
		VIOC_RDMA_SetImageEnable(pWMIX_rdma1_base);
	}

	// wmixer op is mixing mode.
	(void)VIOC_CONFIG_WMIXPath(wmixer->rdma0.id, 1);

	VIOC_WMIX_SetSize(pWMIX_wmix_base,
		apb_info->dst_width, apb_info->dst_height);
	VIOC_WMIX_SetOverlayPriority(pWMIX_wmix_base, 24);

	// set to layer0 of WMIX.
	VIOC_WMIX_ALPHA_SetColorControl(
		pWMIX_wmix_base, apb_info->src0_layer,
		apb_info->region,
		apb_info->src0_ccon0, apb_info->src0_ccon1);
	VIOC_WMIX_ALPHA_SetAlphaValueControl(
		pWMIX_wmix_base, apb_info->src0_layer,
		apb_info->region,
		apb_info->src0_acon0, apb_info->src0_acon1);
	VIOC_WMIX_ALPHA_SetAlphaValue(
		pWMIX_wmix_base, apb_info->src0_layer,
		apb_info->src0_alpha0,
		apb_info->src0_alpha1);
	VIOC_WMIX_ALPHA_SetAlphaSelection(
		pWMIX_wmix_base, apb_info->src0_layer,
		apb_info->src0_asel);
	VIOC_WMIX_ALPHA_SetROPMode(
		pWMIX_wmix_base, apb_info->src0_layer,
		apb_info->src0_rop_mode);

	// set to layer1 of WMIX.
	//VIOC_WMIX_ALPHA_SetAlphaValueControl(
	// pWMIX_wmix_base, apb_info->src1_layer,
	// apb_info->region,
	// apb_info->src1_acon0, apb_info->src1_acon1);
	//VIOC_WMIX_ALPHA_SetColorControl(
	// pWMIX_wmix_base, apb_info->src1_layer,
	// apb_info->region,
	// apb_info->src1_ccon0, apb_info->src1_ccon1);
	//VIOC_WMIX_ALPHA_SetROPMode(
	// pWMIX_wmix_base, apb_info->src1_layer,
	// apb_info->src1_rop_mode);
	//VIOC_WMIX_ALPHA_SetAlphaSelection(
	// pWMIX_wmix_base, apb_info->src1_layer,
	// apb_info->src1_asel);
	//VIOC_WMIX_ALPHA_SetAlphaValue(
	// pWMIX_wmix_base, apb_info->src1_layer,
	// apb_info->src1_alpha0,
	// apb_info->src1_alpha1);

	//position
	VIOC_WMIX_SetPosition(pWMIX_wmix_base, 0,
		apb_info->src0_winLeft, apb_info->src0_winTop);
	VIOC_WMIX_SetPosition(pWMIX_wmix_base, 1,
		apb_info->src1_winLeft, apb_info->src1_winTop);

	// update WMIX.
	VIOC_WMIX_SetUpdate(pWMIX_wmix_base);

	VIOC_WDMA_SetImageFormat(pWMIX_wdma_base,
		apb_info->dst_fmt);
	VIOC_WDMA_SetImageSize(pWMIX_wdma_base,
		apb_info->dst_width, apb_info->dst_height);
	VIOC_WDMA_SetImageOffset(pWMIX_wdma_base,
		apb_info->dst_fmt, apb_info->dst_width);
	VIOC_WDMA_SetImageBase(pWMIX_wdma_base,
		apb_info->dst_Yaddr, apb_info->dst_Uaddr,
		apb_info->dst_Vaddr);

	if (apb_info->dst_fmt > VIOC_IMG_FMT_COMP) {
		VIOC_WDMA_SetImageR2YEnable(pWMIX_wdma_base, 1);
		VIOC_WDMA_SetImageY2REnable(pWMIX_wdma_base, 0);
	} else {
		VIOC_WDMA_SetImageR2YEnable(pWMIX_wdma_base, 0);
	}

	VIOC_WDMA_SetImageEnable(pWMIX_wdma_base, 0 /* OFF */);
	(void)vioc_intr_clear(wmixer->vioc_intr->id,
		wmixer->vioc_intr->bits);

	spin_unlock_irq(&(wmixer->data->cmd_lock));

	if (apb_info->rsp_type == (unsigned int)WMIXER_POLLING) {
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
		/* coverity[cert_int02_c_violation : FALSE] */
		/* coverity[cert_int31_c_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		ret = wait_event_interruptible_timeout(
			wmixer->data->poll_wq,
			wmixer->data->block_operating == 0U,
			msecs_to_jiffies(500));
		if (ret <= 0) {
			wmixer->data->block_operating = 0;
			(void)pr_warn(
				"[WAR][WMIXER] %s(): %s time-out: %d, line: %d.\n",
				__func__, wmixer->misc->name, ret, __LINE__);
		}
	}

	return ret;
}

static unsigned int wmixer_drv_poll(struct file *filp, poll_table *wait)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct wmixer_drv_type *wmixer = dev_get_drvdata(misc->parent);
	unsigned int ret = 0;

	if (wmixer->data == NULL) {
		//ret = -EFAULT;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_poll;
	}

	poll_wait(filp, &(wmixer->data->poll_wq), wait);
	spin_lock_irq(&(wmixer->data->poll_lock));

	if (wmixer->data->block_operating == 0U) {
		/* prevent KCS */
		ret = (int)((unsigned int)POLLIN|(unsigned int)POLLRDNORM);
	}

	spin_unlock_irq(&(wmixer->data->poll_lock));
	/* coverity[misra_c_2012_rule_15_5_violation : FALSE] */

err_poll:
	return ret;
}

static irqreturn_t wmixer_drv_handler(int irq, void *client_data)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct wmixer_drv_type *wmixer = (struct wmixer_drv_type *)client_data;
	irqreturn_t ret;
	(void)irq;
	if (is_vioc_intr_activatied(
		wmixer->vioc_intr->id, wmixer->vioc_intr->bits) == false) {
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			ret = IRQ_NONE;
			goto err_handler;
	}
	(void)vioc_intr_clear(wmixer->vioc_intr->id,
		wmixer->vioc_intr->bits);

	if (wmixer->data->block_operating >= 1U) {
		/* Prevent KCS */
		wmixer->data->block_operating = 0;
	}

	wake_up_interruptible(&(wmixer->data->poll_wq));

	if ((bool)wmixer->data->block_waiting) {
		/* prevent KCS */
		wake_up_interruptible(&wmixer->data->cmd_wq);
	}
	ret = IRQ_HANDLED;
/* coverity[misra_c_2012_rule_15_5_violation : FALSE] */
err_handler:
	return ret;
}

static long wmixer_drv_ioctl(
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct wmixer_drv_type *wmixer = dev_get_drvdata(misc->parent);
	WMIXER_INFO_TYPE *wmix_info = wmixer->info;
	WMIXER_ALPHASCALERING_INFO_TYPE *alpha_scalering =
		&wmixer->alpha_scalering;
	WMIXER_ALPHABLENDING_TYPE *alpha_blending =
		&wmixer->alpha_blending;

	int ret = 0;

	(void)dprintk(
		"%s(): %s: cmd(%d), block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d).\n",
		__func__, wmixer->misc->name, cmd,
		wmixer->data->block_operating,
		wmixer->data->block_waiting,
		wmixer->data->cmd_count,
		wmixer->data->poll_count);

	switch (cmd) {
	case TCC_WMIXER_IOCTRL:
	case TCC_WMIXER_IOCTRL_KERNEL:
		mutex_lock(&wmixer->data->io_mutex);
		if (wmixer->data->block_operating != 0U) {
			wmixer->data->block_waiting = 1;
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
			/* coverity[cert_int02_c_violation : FALSE] */
			/* coverity[cert_int31_c_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			ret = wait_event_interruptible_timeout(
				wmixer->data->cmd_wq,
				wmixer->data->block_operating == 0U,
				msecs_to_jiffies(200));
			if (ret <= 0) {
				wmixer->data->block_operating = 0;
				(void)pr_warn(
					"[WAR][WMIXER] [%d]: %s: timed_out block_operation:%d!! cmd_count:%d\n",
					ret, wmixer->misc->name,
					wmixer->data->block_waiting,
					wmixer->data->cmd_count);
			}
			ret = 0;
		}

		if (cmd == TCC_WMIXER_IOCTRL_KERNEL) {
			(void)memcpy(wmix_info,
				(WMIXER_INFO_TYPE *)arg,
				sizeof(WMIXER_INFO_TYPE));
		} else {
			if (copy_from_user(
				wmix_info, (WMIXER_INFO_TYPE *)arg,
				sizeof(WMIXER_INFO_TYPE)) != 0U) {
				(void)pr_err(
					"[ERR][WMIXER] Not Supported copy_from_user(%d).\n",
					cmd);
				ret = -EFAULT;
				break;
			}
		}

		if (wmixer->data->block_operating >= 1U) {
			(void)pr_info(
				"[INF][WMIXER] scaler + :: block_operating(%d) - block_waiting(%d) - cmd_count(%d) - poll_count(%d)!!!\n",
				wmixer->data->block_operating,
				wmixer->data->block_waiting,
				wmixer->data->cmd_count,
				wmixer->data->poll_count);
		}

		wmixer->data->block_waiting = 0;
		wmixer->data->block_operating = 1;
		ret = wmixer_drv_ctrl(wmixer);
		if (ret < 0) {
			/* Prevent KCS */
			wmixer->data->block_operating = 0;
		}

		mutex_unlock(&wmixer->data->io_mutex);
		break;

	case TCC_WMIXER_ALPHA_SCALING:
	case TCC_WMIXER_ALPHA_SCALING_KERNEL:
		if (wmixer->sc.reg == NULL) {
			(void)pr_err("[ERR][WMIXER] TCC_WMIXER_ALPHA_SCALING ioctl needs a sc\n");
			ret = -1;
			break;
		}

		mutex_lock(&wmixer->data->io_mutex);
		if ((bool)wmixer->data->block_operating) {
			wmixer->data->block_waiting = 1;
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
			/* coverity[cert_int02_c_violation : FALSE] */
			/* coverity[cert_int31_c_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			ret = wait_event_interruptible_timeout(
				wmixer->data->cmd_wq,
				wmixer->data->block_operating == 0U,
				msecs_to_jiffies(200));
			if (ret <= 0) {
				wmixer->data->block_operating = 0;
				(void)pr_warn(
					"[WAR][WMIXER] [%d]: %s: timed_out block_operation:%d!! cmd_count:%d\n",
					ret, wmixer->misc->name,
					wmixer->data->block_waiting,
					wmixer->data->cmd_count);
			}
			ret = 0;
		}

		if (cmd == TCC_WMIXER_ALPHA_SCALING_KERNEL) {
			(void)memcpy(
				alpha_scalering,
				(WMIXER_ALPHASCALERING_INFO_TYPE *)arg,
				sizeof(WMIXER_ALPHASCALERING_INFO_TYPE));
		}
		else {
			if ((bool)copy_from_user(
				(void *)alpha_scalering,
				/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
				(const void *)arg,
				sizeof(
				WMIXER_ALPHASCALERING_INFO_TYPE))) {
				(void)pr_err(
					"[ERR][WMIXER] Not Supported copy_from_user(%d).\n",
					cmd);
				ret = -EFAULT;
			}
		}

		if (ret >= 0) {
			if (wmixer->data->block_operating >= 1U) {
				(void)pr_info(
					"[INF][WMIXER] scaler + :: block_operating(%d) - block_waiting(%d) - cmd_count(%d) - poll_count(%d)!!!\n",
					wmixer->data->block_operating,
					wmixer->data->block_waiting,
					wmixer->data->cmd_count,
					wmixer->data->poll_count);
			}

			wmixer->data->block_waiting = 0;
			wmixer->data->block_operating = 1;
			ret = wmixer_drv_alpha_scaling_ctrl(wmixer);
			if (ret < 0) {
				/* Prevent KCS */
				wmixer->data->block_operating = 0;
			}
		}
		mutex_unlock(&wmixer->data->io_mutex);
		break;

	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case TCC_WMIXER_VIOC_INFO:
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case TCC_WMIXER_VIOC_INFO_KERNEL:
		{
			WMIXER_VIOC_INFO wmixer_id;

			(void)memset(&wmixer_id, 0xFF, sizeof(wmixer_id));
			wmixer_id.rdma[0] = (unsigned char)get_vioc_index(wmixer->rdma0.id);
			wmixer_id.wdma = (unsigned char)get_vioc_index(wmixer->wmix.id);
			wmixer_id.wmixer = (unsigned char)get_vioc_index(wmixer->wmix.id);
				/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
				/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
				/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
				/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
				/* coverity[misra_c_2012_rule_12_2_violation : FALSE] */
				/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
			if (cmd == TCC_WMIXER_VIOC_INFO) {
				/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
				if ((bool)copy_to_user((void *)arg,
					(void *)&wmixer_id, sizeof(wmixer_id))) {
					ret = -EFAULT;
					break;
					}
			} else {
				/* Prevent KCS */
				/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
				(void)memcpy((void *)arg,
					(void *)&wmixer_id, sizeof(wmixer_id));
			}
		}
		ret = 0;
		break;

	case TCC_WMIXER_ALPHA_MIXING:
		if (wmixer->rdma1.reg == NULL) {
			(void)pr_err("[ERR][WMIXER] TCC_WMIXER_ALPHA_MIXING ioctl needs a rdma1\n");
			ret = -EFAULT;
			break;
		}

		mutex_lock(&wmixer->data->io_mutex);
		if ((bool)wmixer->data->block_operating) {
			wmixer->data->block_waiting = 1;
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
			/* coverity[cert_int02_c_violation : FALSE] */
			/* coverity[cert_int31_c_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			ret = wait_event_interruptible_timeout(
				wmixer->data->cmd_wq,
				wmixer->data->block_operating == 0U,
				msecs_to_jiffies(200));
			if (ret <= 0) {
				wmixer->data->block_operating = 0;
				(void)pr_warn("[WAR][WMIXER] [%d]: %s: timed_out block_operation:%d!! cmd_count:%d\n",
					ret, wmixer->misc->name,
					wmixer->data->block_waiting,
					wmixer->data->cmd_count);
			}
			ret = 0;
		}

		if ((bool)copy_from_user(
			alpha_blending,
			(WMIXER_ALPHABLENDING_TYPE *)arg,
			sizeof(WMIXER_ALPHABLENDING_TYPE))) {
			(void)pr_err(
				"[ERR][WMIXER] Not Supported copy_from_user(%d).\n",
				cmd);
			ret = -EFAULT;
		}

		if (wmixer->sc.reg == NULL) {
			if ((alpha_blending->src0_width !=
				alpha_blending->src1_width)
				&& (alpha_blending->src0_height !=
				alpha_blending->src1_height)
				&& (alpha_blending->dst_width !=
				alpha_blending->src0_width)
				&& (alpha_blending->dst_height !=
				alpha_blending->src0_height)) {
				(void)pr_err("[ERR][WMIXER] Cannot run hw-memcpy !!!\n");
				ret = -EINVAL;
			}
		}

		if (ret >= 0) {
			if (wmixer->data->block_operating >= 1U) {
				(void)pr_info(
					"[INF][WMIXER] scaler + :: block_operating(%d) - block_waiting(%d) - cmd_count(%d) - poll_count(%d)!!!\n",
					wmixer->data->block_operating,
					wmixer->data->block_waiting,
					wmixer->data->cmd_count,
					wmixer->data->poll_count);
			}

			wmixer->data->block_waiting = 0;
			wmixer->data->block_operating = 1;
			ret = wmixer_drv_alpha_mixing_ctrl(wmixer);
			if (ret < 0) {
				/* Prevent KCS */	
				wmixer->data->block_operating = 0;
			}
		}
		mutex_unlock(&wmixer->data->io_mutex);
		break;

	default:
		(void)pr_err(
			"[ERR][WMIXER] not supported %s IOCTL(0x%x).\n",
			wmixer->misc->name, cmd);
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long wmixer_drv_compat_ioctl(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	return wmixer_drv_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_5_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int wmixer_drv_release(struct inode *inode, struct file *filp)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct wmixer_drv_type *wmixer = dev_get_drvdata(misc->parent);

	int ret = 1;
	(void)dprintk("%s_release IN:  %d'th, block(%d/%d), cmd(%d), irq(%d).\n",
		wmixer->misc->name,
		wmixer->data->dev_opened,
		wmixer->data->block_operating,
		wmixer->data->block_waiting,
		wmixer->data->cmd_count,
		wmixer->data->irq_reged);

	if (wmixer->data->dev_opened > 0U) {
		/* Prevent KCS */
		wmixer->data->dev_opened--;
	}

	if (wmixer->data->dev_opened == 0U) {
		if ((bool)wmixer->data->block_operating) {
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
			/* coverity[cert_int02_c_violation : FALSE] */
			/* coverity[cert_int31_c_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			ret = wait_event_interruptible_timeout(
				wmixer->data->cmd_wq,
				wmixer->data->block_operating == 0U,
				msecs_to_jiffies(200));
		}

		if (ret <= 0) {
			(void)pr_warn(
				"[WAR][WMIXER] [%d]: %s timed_out block_operation: %d, cmd_count: %d.\n",
				ret, wmixer->misc->name,
				wmixer->data->block_waiting,
				wmixer->data->cmd_count);
		}

		if ((bool)wmixer->data->irq_reged) {
			(void)vioc_intr_clear(wmixer->vioc_intr->id,
				wmixer->vioc_intr->bits);
			if(wmixer->irq <= (UINT_MAX/2U)) {
				/* KCS */
				(void)vioc_intr_disable(
					(signed)wmixer->irq, wmixer->vioc_intr->id,
					wmixer->vioc_intr->bits);
			}
			(void)free_irq(wmixer->irq, wmixer);
			wmixer->data->irq_reged = 0;
		}

		// VIOC_CONFIG_PlugOut(wmixer->sc.id);
		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			#ifdef CONFIG_VIOC_MAP_DECOMP
			int component_num =
				VIOC_CONFIG_DMAPath_Select(wmixer->rdma0.id);
			if(component_num >= 0) {
				/* prevent KCS */
				if (((unsigned int)component_num >= VIOC_MC0) &&
					((unsigned int)component_num <= (VIOC_MC0 + VIOC_MC_MAX))) {
					(void)VIOC_CONFIG_DMAPath_UnSet(component_num);
					if((UINT_MAX - gMC_NUM) >= VIOC_MC0) {
						/* KCS */
				//		if((VIOC_MC0 + gMC_NUM) > 0U) {
							tca_map_convter_swreset((VIOC_MC0 + gMC_NUM));
//						}
					}
					(void)VIOC_CONFIG_DMAPath_Set(
						wmixer->rdma0.id, wmixer->rdma0.id);
				}
			}
			#endif
		} else {
			#if defined(CONFIG_VIOC_MAP_DECOMP) \
				&& (defined(CONFIG_ARCH_TCC803X) \
				|| defined(CONFIG_ARCH_TCC805X))
			(void)VIOC_CONFIG_MCPath(wmixer->wmix.id, wmixer->rdma0.id);
			#endif
		}

		if (wmixer->wdma.reg != NULL) {
			/* Prevent KCS */
			VIOC_CONFIG_SWReset(
				wmixer->wdma.id, VIOC_CONFIG_RESET);
		}
		if (wmixer->wmix.reg != NULL) {
			/* Prevent KCS */
			VIOC_CONFIG_SWReset(
				wmixer->wmix.id, VIOC_CONFIG_RESET);
		}
		if (wmixer->rdma0.reg != NULL) {
			/* Prevent KCS */
			VIOC_CONFIG_SWReset(
				wmixer->rdma0.id, VIOC_CONFIG_RESET);
		}
		if (wmixer->rdma1.reg != NULL) {
			/* Prevent KCS */
			VIOC_CONFIG_SWReset(
				wmixer->rdma1.id, VIOC_CONFIG_RESET);
		}
		if (wmixer->rdma1.reg != NULL) {
			/* Prevent KCS */
			VIOC_CONFIG_SWReset(
				wmixer->rdma1.id, VIOC_CONFIG_RESET);
		}
		if (wmixer->rdma0.reg != NULL) {
			/* Prevent KCS */
			VIOC_CONFIG_SWReset(
				wmixer->rdma0.id, VIOC_CONFIG_CLEAR);
		}
		if (wmixer->wmix.reg != NULL) {
			/* Prevent KCS */
			VIOC_CONFIG_SWReset(
				wmixer->wmix.id, VIOC_CONFIG_CLEAR);
		}
		if (wmixer->wdma.reg != NULL) {
			/* Prevent KCS */
			VIOC_CONFIG_SWReset(
				wmixer->wdma.id, VIOC_CONFIG_CLEAR);
		}
		wmixer->data->block_waiting = 0;
		wmixer->data->block_operating = wmixer->data->block_waiting;

		wmixer->data->cmd_count = 0;
		wmixer->data->poll_count = wmixer->data->cmd_count;
	}

	if (wmixer->wmix_clk != NULL) {
		/* Prevent KCS */
		clk_disable_unprepare(wmixer->wmix_clk);
	}
	(void)dprintk(
		"%s_release OUT:  %d'th.\n",
		wmixer->misc->name, wmixer->data->dev_opened);

	return 0;
}

/* coverity[misra_c_2012_rule_5_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int wmixer_drv_open(struct inode *inode, struct file *filp)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct miscdevice   *misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct wmixer_drv_type  *wmixer = dev_get_drvdata(misc->parent);

	int ret = 0;

	(void)dprintk(
		"%s_open IN:  %d'th, block(%d/%d), cmd(%d), irq(%d :: %d/0x%x).\n",
		wmixer->misc->name, wmixer->data->dev_opened,
		wmixer->data->block_operating,
		wmixer->data->block_waiting, wmixer->data->cmd_count,
		wmixer->data->irq_reged, wmixer->vioc_intr->id,
		wmixer->vioc_intr->bits);

	if (wmixer->wmix_clk != NULL) {
		/* Prevent KCS */
		(void)clk_prepare_enable(wmixer->wmix_clk);
	}

	if (!(bool)wmixer->data->irq_reged) {
		if (wmixer->wdma.reg != NULL) {
			/* Prevent KCS */
			VIOC_CONFIG_SWReset(
				wmixer->wdma.id, VIOC_CONFIG_RESET);
		}
		if (wmixer->wmix.reg != NULL) {
			/* Prevent KCS */
			VIOC_CONFIG_SWReset(
				wmixer->wmix.id, VIOC_CONFIG_RESET);
		}
//      if (wmixer->sc.reg)
//          VIOC_CONFIG_SWReset(wmixer->sc.id, VIOC_CONFIG_RESET);
		if (wmixer->rdma0.reg != NULL) {
			/* Prevent KCS */
			VIOC_CONFIG_SWReset(
				wmixer->rdma0.id, VIOC_CONFIG_RESET);
		}

		if (wmixer->rdma0.reg != NULL) {
			/* Prevent KCS */
			VIOC_CONFIG_SWReset(
				wmixer->rdma0.id, VIOC_CONFIG_CLEAR);
		}
//      if (wmixer->sc.reg)
//          VIOC_CONFIG_SWReset(wmixer->sc.id, VIOC_CONFIG_CLEAR);
		if (wmixer->wmix.reg != NULL) {
			/* Prevent KCS */
			VIOC_CONFIG_SWReset(wmixer->wmix.id, VIOC_CONFIG_CLEAR);
		}
		if (wmixer->wdma.reg != NULL) {
			/* Prevent KCS */
			VIOC_CONFIG_SWReset(wmixer->wdma.id, VIOC_CONFIG_CLEAR);
		}

		// VIOC_CONFIG_StopRequest(1);
		synchronize_irq(wmixer->irq);

		(void)vioc_intr_clear(
			wmixer->vioc_intr->id, wmixer->vioc_intr->bits);
		ret = request_irq(
			wmixer->irq, wmixer_drv_handler,
			IRQF_SHARED, wmixer->misc->name, wmixer);
		if ((bool)ret) {
			if (wmixer->wmix_clk != NULL) {
				/* Prevent KCS */
				clk_disable_unprepare(wmixer->wmix_clk);
			}
			(void)pr_err(
				"[ERR][WMIXER] failed to aquire %s request_irq.\n",
				wmixer->misc->name);
			ret = -EFAULT;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto err_wmixer_drv_open;
		}
		if(wmixer->irq <= (UINT_MAX/2U)) {
			/* KCS */
			(void)vioc_intr_enable(
				(int)wmixer->irq, wmixer->vioc_intr->id,
				wmixer->vioc_intr->bits);
		}

		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			unsigned int component_num = 0;
			int itemp;
			itemp = VIOC_CONFIG_DMAPath_Select((unsigned)wmixer->rdma0.id);
			if(itemp >= 0) {
				/* KCS */
				component_num = (unsigned int)itemp;
			}

			if ((component_num < VIOC_RDMA00) ||
				(component_num > (VIOC_RDMA00 + VIOC_RDMA_MAX))) {
					/* Prevent KCS*/
					(void)VIOC_CONFIG_DMAPath_UnSet(itemp);
			} else {
				/* Prevent KCS*/
				// do notthing
			}
			// It is default path selection(VRDMA)
			(void)VIOC_CONFIG_DMAPath_Set(
				wmixer->rdma0.id, wmixer->rdma0.id);
		}

		wmixer->data->irq_reged = 1;
	}

	wmixer->data->dev_opened++;
	(void)dprintk(
		"%s_open OUT:  %d'th.\n",
		wmixer->misc->name,
		wmixer->data->dev_opened);


err_wmixer_drv_open:
	return ret;
}

static const struct file_operations wmixer_drv_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = wmixer_drv_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = wmixer_drv_compat_ioctl,
	// for 32 bit OMX ioctl
#endif
	.mmap           = wmixer_drv_mmap,
	.open           = wmixer_drv_open,
	.release        = wmixer_drv_release,
	.poll           = wmixer_drv_poll,
};

static int wmixer_drv_probe(struct platform_device *pdev)
{
	struct wmixer_drv_type *wmixer;
	struct device_node *dev_np;
	unsigned int index;
	int ret = -ENODEV;
	int itemp;
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	wmixer = kzalloc(sizeof(struct wmixer_drv_type), GFP_KERNEL);
	if (wmixer == NULL) {
		/* Prevent KCS */
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_misc_alloc;
	}

	wmixer->wmix_clk = of_clk_get(pdev->dev.of_node, 0);
	/* coverity[misra_c_2012_rule_11_2_violation : FALSE] */
	if (IS_ERR(wmixer->wmix_clk)) {
		/* Prevent KCS */
		wmixer->wmix_clk = NULL;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	wmixer->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (wmixer->misc == NULL) {
		/* Prevent KCS */
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_misc_alloc;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	wmixer->info = kzalloc(sizeof(WMIXER_INFO_TYPE), GFP_KERNEL);
	if (wmixer->info == NULL) {
		/* Prevent KCS */
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_info_alloc;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	wmixer->data = kzalloc(sizeof(struct wmixer_data), GFP_KERNEL);
	if (wmixer->data == NULL) {
		/* Prevent KCS */
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_data_alloc;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	wmixer->vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
	if (wmixer->vioc_intr == NULL) {
		/* Prevent KCS */
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_vioc_intr_alloc;
	}

	wmixer->scaler_plug_status = 0;
	itemp = of_alias_get_id(
		pdev->dev.of_node, "wmixer-drv");
	if(itemp >= 0) {
		/* KCS */
		wmixer->id = (unsigned int)itemp;
	} else {
		/* KCS */
		wmixer->id = 0;
	}

	/* register wmixer discdevice */
	wmixer->misc->minor = MISC_DYNAMIC_MINOR;
	wmixer->misc->fops = &wmixer_drv_fops;
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	wmixer->misc->name = kasprintf(
		GFP_KERNEL, "wmixer%d", wmixer->id);
	wmixer->misc->parent = &pdev->dev;
	ret = misc_register(wmixer->misc);
	if ((bool)ret) {
		/* Prevent KCS */
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_misc_register;
	}

	dev_np = of_parse_phandle(pdev->dev.of_node, "rdmas", 0);
	if (dev_np !=NULL) {
		if (of_property_read_u32_index(
			pdev->dev.of_node, "rdmas", 1, &index) == 0) {
			wmixer->rdma0.reg = VIOC_RDMA_GetAddress(index);
			wmixer->rdma0.id = index;
		} else {
			/* Prevent KCS */
			wmixer->rdma0.reg = NULL;
		}
	} else {
		(void)pr_warn(
			"[WAR][WMIXER] could not find rdma0 node of %s driver.\n",
			wmixer->misc->name);
		wmixer->rdma0.reg = NULL;
	}

	dev_np = of_parse_phandle(
		pdev->dev.of_node, "rdmas", 0);
	if (dev_np != NULL) {
		if (of_property_read_u32_index(
			pdev->dev.of_node, "rdmas", 2, &index) == 0) {
			wmixer->rdma1.reg = VIOC_RDMA_GetAddress(index);
			wmixer->rdma1.id = index;
		} else {
			wmixer->rdma1.reg = NULL;
		}
	} else {
		(void)pr_warn(
			"[WAR][WMIXER] could not find rdma1 node of %s driver.\n",
			wmixer->misc->name);
		wmixer->rdma1.reg = NULL;
	}

	dev_np = of_parse_phandle(
		pdev->dev.of_node, "scalers", 0);
	if (dev_np != NULL) {
		(void)of_property_read_u32_index(
			pdev->dev.of_node, "scalers", 1, &index);
		wmixer->sc.reg = VIOC_SC_GetAddress(index);
		wmixer->sc.id = index;
	} else {
		(void)pr_warn(
			"[WAR][WMIXER] could not find scaler node of %s driver.\n",
			wmixer->misc->name);
		wmixer->sc.reg = NULL;
	}

	dev_np = of_parse_phandle(pdev->dev.of_node, "wmixs", 0);
	if (dev_np != NULL) {
		(void)of_property_read_u32_index(
			pdev->dev.of_node, "wmixs", 1, &index);
		wmixer->wmix.reg = VIOC_WMIX_GetAddress(index);
		wmixer->wmix.id = index;
		(void)of_property_read_u32_index(
			pdev->dev.of_node, "wmixs", 2, &wmixer->wmix.vioc_path);
	} else {
		(void)pr_warn(
			"[WAR][WMIXER] could not find wmix node of %s driver.\n",
			wmixer->misc->name);
		wmixer->wmix.reg = NULL;
	}

	dev_np = of_parse_phandle(pdev->dev.of_node, "wdmas", 0);
	if (dev_np != NULL) {
		unsigned int uitemp;
		(void)of_property_read_u32_index(
			pdev->dev.of_node,
			"wdmas", 1, &index);
		wmixer->wdma.reg = VIOC_WDMA_GetAddress(index);
		wmixer->wdma.id = index;
		uitemp = get_vioc_index(index);
		itemp = (int)uitemp;
		wmixer->irq = irq_of_parse_and_map(
						dev_np, itemp);
		uitemp = (unsigned int)VIOC_INTR_WD0 + get_vioc_index(wmixer->wdma.id);
		itemp = (int)uitemp;
		wmixer->vioc_intr->id = itemp;
		wmixer->vioc_intr->bits = VIOC_WDMA_IREQ_EOFR_MASK;
	} else {
		(void)pr_warn(
			"[WAR][WMIXER] could not find wdma node of %s driver.\n",
			wmixer->misc->name);
		wmixer->wdma.reg = NULL;
	}

	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_17_7_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	spin_lock_init(&(wmixer->data->poll_lock));
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_17_7_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	spin_lock_init(&(wmixer->data->cmd_lock));
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_17_7_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	mutex_init(&(wmixer->data->io_mutex));

	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	init_waitqueue_head(&(wmixer->data->poll_wq));
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	init_waitqueue_head(&(wmixer->data->cmd_wq));

	platform_set_drvdata(pdev, wmixer);

	(void)pr_info(
		"[INF][WMIXER] %s: id:%d, Wmixer Driver Initialized\n",
		pdev->name, wmixer->id);

	ret = 0;
	/* coverity[misra_c_2012_rule_15_5_violation : FALSE] */
	goto probe_return;
err_misc_register:
	kfree(wmixer->vioc_intr);
err_vioc_intr_alloc:
	kfree(wmixer->data);
err_data_alloc:
	kfree(wmixer->info);
err_info_alloc:
	kfree(wmixer->misc);
err_misc_alloc:
	kfree(wmixer);

	(void)pr_err("[ERR][WMIXER] %s: %s: err ret:%d\n", __func__, pdev->name, ret);
probe_return:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int wmixer_drv_remove(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct wmixer_drv_type *wmixer =
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		(struct wmixer_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(wmixer->misc);
	kfree(wmixer->vioc_intr);
	kfree(wmixer->data);
	kfree(wmixer->info);
	kfree(wmixer->misc);
	kfree(wmixer);
	return 0;
}

/*
static int wmixer_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int wmixer_drv_resume(struct platform_device *pdev)
{
	return 0;
}
*/
static const struct of_device_id wmixer_of_match[] = {
	{ .compatible = "telechips,wmixer_drv" },
	{}
};
MODULE_DEVICE_TABLE(of, wmixer_of_match);

static struct platform_driver wmixer_driver = {
	.probe      = wmixer_drv_probe,
	.remove     = wmixer_drv_remove,
	//.suspend    = wmixer_drv_suspend,
	//.resume     = wmixer_drv_resume,
	.driver     = {
		.name   = "wmixer_pdev",
		.owner  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(wmixer_of_match),
#endif
	},
};

static int __init wmixer_drv_init(void)
{
	return platform_driver_register(&wmixer_driver);
}

static void __exit wmixer_drv_exit(void)
{
	platform_driver_unregister(&wmixer_driver);
}

/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
module_init(wmixer_drv_init);
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
module_exit(wmixer_drv_exit);

/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_AUTHOR("linux <linux@telechips.com>");
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_DESCRIPTION("Telechips WMIXER Driver");
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_LICENSE("GPL");


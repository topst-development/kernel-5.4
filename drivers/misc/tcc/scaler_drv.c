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
#include <linux/compat.h>

#include <video/tcc/vioc_intr.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/tcc_scaler_ioctrl.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_lut.h>
#include <video/tcc/vioc_mc.h>
#include <video/tcc/vioc_config.h>

#ifdef CONFIG_VIOC_MAP_DECOMP
#include <video/tcc/tcc_video_private.h>
#include <video/tcc/tca_map_converter.h>
#endif

#ifdef CONFIG_VIOC_DTRC_DECOMP
#include <video/tcc/tca_dtrc_converter.h>
#endif

#if defined(CONFIG_VIOC_SAR)
#include <video/tcc/vioc_sar.h>
extern int CheckSarPathSelection(unsigned int component);
#endif//

#include "../../drivers/media/platform/tccvin2/basic_operation.h"

#define dprintk(msg...)	//#define dprintk(msg...) pr_info("[DBG][SCALER] " msg)

#define TCC_PA_VIOC_CFGINT	(HwVIOC_BASE + 0xA000)

struct scaler_data {
	// wait for poll
	wait_queue_head_t	poll_wq;
	spinlock_t		poll_lock;
	unsigned int		poll_count;

	// wait for ioctl command
	wait_queue_head_t	cmd_wq;
	spinlock_t		cmd_lock;
	unsigned int		cmd_count;

	struct mutex		io_mutex;
	unsigned char		block_operating;
	unsigned char		block_waiting;
	unsigned char		irq_reged;
	unsigned int		dev_opened;
};

struct scaler_drv_vioc {
	void __iomem *reg;
	unsigned int id;
//	unsigned int path;
};

#if defined(CONFIG_VIOC_SAR)
struct vioc_sar_block_res_type {
	unsigned int enable;
	unsigned int id;
	unsigned int level;
};
#endif//
struct scaler_drv_type {
	struct vioc_intr_type	*vioc_intr;

	unsigned int		id;
	unsigned int		irq;

	struct miscdevice	*misc;

	struct scaler_drv_vioc rdma;
	struct scaler_drv_vioc wmix;
	struct scaler_drv_vioc sc;
	struct scaler_drv_vioc wdma;

	struct clk		*pclk;
	struct scaler_data	*data;
	struct SCALER_TYPE		*info;

	unsigned int		settop_support;
#if defined(CONFIG_VIOC_SAR)
	struct vioc_sar_block_res_type	sar;
#endif
};

enum dma_type {
	DMA_TYPE_RDMA = 0,
	DMA_TYPE_MC,
	DMA_TYPE_DTRC
};

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int scaler_drv_mmap(struct file *filp,
	struct vm_area_struct *vma)
{
	int ret = 0;
	(void)filp;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if(vma->vm_end >= vma->vm_start) {
		if ((bool)remap_pfn_range(vma, vma->vm_start,
			vma->vm_pgoff, vma->vm_end - vma->vm_start,
			vma->vm_page_prot)) {
			ret = -EAGAIN;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto err_remap_pfn_range;
		}
	}

	vma->vm_ops = NULL;
	vma->vm_flags |= (unsigned long)VM_IO;
	vma->vm_flags |= (unsigned long)VM_DONTEXPAND | (unsigned long)VM_PFNMAP;

err_remap_pfn_range:
	return ret;
}

static int tcc_scaler_run(const struct scaler_drv_type *scaler)
{
	int ret = 0;
	unsigned int pSrcBase0 = 0, pSrcBase1 = 0, pSrcBase2 = 0;
	enum dma_type type;

	void __iomem *pSC_RDMABase = scaler->rdma.reg;
	void __iomem *pSC_WMIXBase = scaler->wmix.reg;
	void __iomem *pSC_WDMABase = scaler->wdma.reg;
	void __iomem *pSC_SCALERBase = scaler->sc.reg;

	dprintk("%s():  IN.\n", __func__);

	if (VIOC_CONFIG_GetViqeDeintls_PluginToRDMA
		(get_vioc_index(scaler->rdma.id)) != -1) {
		scaler->info->src_winBottom =
			(scaler->info->src_winBottom >> 2) << 2;
	}

	spin_lock_irq(&(scaler->data->cmd_lock));

	dprintk(
		"scaler %d src   add : 0x%x 0x%x 0x%x, fmt :0x%x IMG:(%d %d)%d %d %d %d\n",
		scaler->sc.id, scaler->info->src_Yaddr, scaler->info->src_Uaddr,
		scaler->info->src_Vaddr, scaler->info->src_fmt,
		scaler->info->src_ImgWidth, scaler->info->src_ImgHeight,
		scaler->info->src_winLeft, scaler->info->src_winTop,
		scaler->info->src_winRight, scaler->info->src_winBottom);
	dprintk(
		"scaler %d dest  add : 0x%x 0x%x 0x%x, fmt :0x%x IMG:(%d %d)%d %d %d %d\n",
		scaler->sc.id, scaler->info->dest_Yaddr,
		scaler->info->dest_Uaddr,
		scaler->info->dest_Vaddr,
		scaler->info->dest_fmt,
		scaler->info->dest_ImgWidth,
		scaler->info->dest_ImgHeight,
		scaler->info->dest_winLeft,
		scaler->info->dest_winTop,
		scaler->info->dest_winRight,
		scaler->info->dest_winBottom);
	dprintk(
		"scaler %d interlace:%d\n",
		scaler->sc.id,  scaler->info->interlaced);

	type = DMA_TYPE_RDMA;
	#ifdef CONFIG_VIOC_DTRC_DECOMP
	if (scaler->info->dtrcConv_info.m_CompressedY[0] != 0UL)
		type = DMA_TYPE_DTRC;
	#endif
	#ifdef CONFIG_VIOC_MAP_DECOMP
	if (scaler->info->mapConv_info.m_CompressedY[0] != 0UL) {
		type = DMA_TYPE_MC;
	}
	#endif

	if(type == DMA_TYPE_RDMA) {
		if (scaler->settop_support != 0U) {
			VIOC_RDMA_SetImageAlphaSelect(pSC_RDMABase, 1);
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		} else {
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		}

		VIOC_RDMA_SetImageFormat(pSC_RDMABase, scaler->info->src_fmt);

		#ifdef CONFIG_VIOC_10BIT
		if (scaler->info->src_bit_depth == 10)
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x1, 0x1);
		else if (scaler->info->src_bit_depth == 11)
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x3, 0x0);
		else
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x0, 0x0);
		#endif

		//interlaced frame process ex) MPEG2
		if (scaler->info->interlaced != 0U) {
			VIOC_RDMA_SetImageSize(
				pSC_RDMABase,
				(scaler->info->src_winRight -
				scaler->info->src_winLeft),
				(scaler->info->src_winBottom -
				scaler->info->src_winTop)/2U);
			VIOC_RDMA_SetImageOffset(
				pSC_RDMABase, scaler->info->src_fmt,
				scaler->info->src_ImgWidth*2U);
		} else {
			VIOC_RDMA_SetImageSize(
				pSC_RDMABase,
				(scaler->info->src_winRight -
				scaler->info->src_winLeft),
				(scaler->info->src_winBottom -
				scaler->info->src_winTop));

			#ifdef CONFIG_VIOC_10BIT
			if (scaler->info->src_bit_depth == 10)
				VIOC_RDMA_SetImageOffset(pSC_RDMABase,
					scaler->info->src_fmt,
					scaler->info->src_ImgWidth*2);
			else
			#endif
			{
				VIOC_RDMA_SetImageOffset(
					pSC_RDMABase, scaler->info->src_fmt,
					scaler->info->src_ImgWidth);
			}
		}

		pSrcBase0 = (unsigned int)scaler->info->src_Yaddr;
		pSrcBase1 = (unsigned int)scaler->info->src_Uaddr;
		pSrcBase2 = (unsigned int)scaler->info->src_Vaddr;

		if ((scaler->info->src_fmt > VIOC_IMG_FMT_COMP)
			&& (scaler->info->dest_fmt < VIOC_IMG_FMT_COMP)) {
			VIOC_RDMA_SetImageY2REnable(pSC_RDMABase, 1);
		} else {
			VIOC_RDMA_SetImageY2REnable(pSC_RDMABase, 0);
		}

		if (scaler->info->src_fmt < VIOC_IMG_FMT_COMP) {
			VIOC_RDMA_SetImageRGBSwapMode(
				pSC_RDMABase, scaler->info->src_rgb_swap);
		} else {
			VIOC_RDMA_SetImageRGBSwapMode(pSC_RDMABase, 0);
		}

		VIOC_RDMA_SetImageBase(
			pSC_RDMABase, (unsigned int)pSrcBase0,
			(unsigned int)pSrcBase1, (unsigned int)pSrcBase2);
		VIOC_RDMA_SetImageEnable(pSC_RDMABase); // SoC guide info.
	} else if(type == DMA_TYPE_MC) {
		#ifdef CONFIG_VIOC_MAP_DECOMP
		unsigned int y2r = 0U;

		dprintk(
			"scaler %d path-id(rdma:%d)\n",
			scaler->sc.id, scaler->rdma.id);
		dprintk(
			"scaler %d src  map converter: size: %d %d pos:%d %d\n",
			scaler->sc.id,
			scaler->info->src_winRight - scaler->info->src_winLeft,
			scaler->info->src_winBottom - scaler->info->src_winTop,
			scaler->info->src_winLeft, scaler->info->src_winTop);

		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			int component_num =
				VIOC_CONFIG_DMAPath_Select(scaler->rdma.id);

			if ((component_num < u32_to_s32(VIOC_MC0)) ||
				(component_num > u32_to_s32(VIOC_MC0 + VIOC_MC_MAX))) {
				(void)VIOC_CONFIG_DMAPath_UnSet(component_num);

				tca_map_convter_swreset(VIOC_MC1);
				//disable it to prevent system-hang!!
			}
			(void)VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, VIOC_MC1);
		} else {
			#if defined(CONFIG_ARCH_TCC803X) \
				|| defined(CONFIG_ARCH_TCC805X)
			(void)VIOC_CONFIG_MCPath(scaler->wmix.id, VIOC_MC1);
			#endif
		}

		if (scaler->info->dest_fmt < s32_to_u32(SCLAER_COMPRESS_DATA)) {
			y2r = 1U;
		}

		// scaler limitation
		tca_map_convter_driver_set(
			VIOC_MC1,
			scaler->info->src_ImgWidth,
			scaler->info->src_ImgHeight,
			scaler->info->src_winLeft,
			scaler->info->src_winTop,
			scaler->info->src_winRight - scaler->info->src_winLeft,
			scaler->info->src_winBottom - scaler->info->src_winTop,
			y2r, &scaler->info->mapConv_info);

		tca_map_convter_onoff(VIOC_MC1, 1, 0);
		#endif

	} else {
		#ifdef CONFIG_VIOC_DTRC_DECOMP
		int y2r = 0;

		dprintk(
			"scaler %d src dtrc converter: size: %d %d pos:%d %d\n",
			scaler->sc.id,
			scaler->info->src_winRight - scaler->info->src_winLeft,
			scaler->info->src_winBottom - scaler->info->src_winTop,
			scaler->info->src_winLeft, scaler->info->src_winTop);

		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			int component_num =
				VIOC_CONFIG_DMAPath_Select(u32_to_s32(scaler->rdma.id));

			if ((component_num < VIOC_DTRC0) ||
				(component_num >
					(VIOC_DTRC0 + VIOC_DTRC_MAX))) {
				(void)VIOC_CONFIG_DMAPath_UnSet(u32_to_s32(component_num));
				//tca_dtrc_convter_swreset(VIOC_DTRC1);
				//disable it to prevent system-hang!!
			}
			(void)VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, VIOC_DTRC1);
		}

		if (scaler->info->dest_fmt < SCLAER_COMPRESS_DATA)
			y2r = 1;

		// scaler limitation
		tca_dtrc_convter_driver_set(
			VIOC_DTRC1,
			scaler->info->src_winRight -
			scaler->info->src_winLeft,
			scaler->info->src_winBottom -
			scaler->info->src_winTop,
			scaler->info->src_winLeft, scaler->info->src_winTop,
			y2r, &scaler->info->dtrcConv_info);
		tca_dtrc_convter_onoff(VIOC_DTRC1, 1, 0);
		#endif
	}
	// look up table use
	if (scaler->info->lut.use_lut != 0U) {
		(void)tcc_set_lut_plugin(
			VIOC_LUT + scaler->info->lut.use_lut_number,
			scaler->rdma.id);
		tcc_set_lut_enable(
			VIOC_LUT + scaler->info->lut.use_lut_number,
			1U);
	}
#if defined(CONFIG_VIOC_SAR)
	if (scaler->info->sar.enable) {
		int ret = -1;

		scaler->sar.id = VIOC_SAR0;
		scaler->sar.enable = scaler->info->sar.enable;
		scaler->sar.level = scaler->info->sar.strength;

		ret = CheckSarPathSelection(scaler->rdma.id);

		if (ret >= 0) {
			VIOC_SAR_POWER_ONOFF(1);
			(void)VIOC_CONFIG_PlugIn(VIOC_SAR, scaler->rdma.id);

			VIOC_CONFIG_SWReset(VIOC_SAR, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(VIOC_SAR, VIOC_CONFIG_CLEAR);

			vioc_sar_on(
				(scaler->info->src_winRight -
					scaler->info->src_winLeft),
				(scaler->info->src_winBottom -
					scaler->info->src_winTop),
				scaler->sar.level);
		}

	}
#endif//

	VIOC_SC_SetBypass(pSC_SCALERBase, 0);
#if defined(CONFIG_MC_WORKAROUND)
	if (!system_rev && scaler->info->mapConv_info.m_CompressedY[0] != 0) {
		unsigned int plus_height =
			VIOC_SC_GetPlusSize(
				(scaler->info->src_winBottom -
				scaler->info->src_winTop),
				(scaler->info->dest_winBottom -
				scaler->info->dest_winTop));

		VIOC_SC_SetDstSize(
			pSC_SCALERBase,
			(scaler->info->dest_winRight -
			scaler->info->dest_winLeft),
			(scaler->info->dest_winBottom -
			scaler->info->dest_winTop) + plus_height);
	} else
#endif
	VIOC_SC_SetDstSize(
		pSC_SCALERBase,
		scaler->info->dest_ImgWidth,
		scaler->info->dest_ImgHeight);
	VIOC_SC_SetOutSize(
		pSC_SCALERBase,
		(scaler->info->dest_winRight -
		scaler->info->dest_winLeft),
		(scaler->info->dest_winBottom -
		scaler->info->dest_winTop));
	VIOC_SC_SetOutPosition(pSC_SCALERBase,
		scaler->info->dest_winLeft,
		scaler->info->dest_winTop);
	(void)VIOC_CONFIG_PlugIn(scaler->sc.id, scaler->rdma.id);
	VIOC_SC_SetUpdate(pSC_SCALERBase);

	if (get_vioc_index(scaler->rdma.id) == get_vioc_index(VIOC_RDMA07)) {
		(void)VIOC_CONFIG_WMIXPath(scaler->rdma.id, 0);
	} else {
		(void)VIOC_CONFIG_WMIXPath(scaler->rdma.id, 1);
	}

	VIOC_WMIX_SetSize(
		pSC_WMIXBase,
		(scaler->info->dest_winRight - scaler->info->dest_winLeft),
		(scaler->info->dest_winBottom - scaler->info->dest_winTop));
	VIOC_WMIX_SetUpdate(pSC_WMIXBase);

	VIOC_WDMA_SetImageFormat(pSC_WDMABase, scaler->info->dest_fmt);

	if (scaler->info->dest_fmt < VIOC_IMG_FMT_COMP) {
		VIOC_WDMA_SetImageRGBSwapMode(pSC_WDMABase,
			scaler->info->dst_rgb_swap);
	} else {
		VIOC_WDMA_SetImageRGBSwapMode(pSC_WDMABase, 0);
	}

	VIOC_WDMA_SetImageSize(
		pSC_WDMABase,
		(scaler->info->dest_winRight - scaler->info->dest_winLeft),
		(scaler->info->dest_winBottom - scaler->info->dest_winTop));

	VIOC_WDMA_SetImageOffset(pSC_WDMABase,
		scaler->info->dest_fmt,
		(scaler->info->dest_winRight - scaler->info->dest_winLeft));

	VIOC_WDMA_SetImageBase(
		pSC_WDMABase, (unsigned int)scaler->info->dest_Yaddr,
		(unsigned int)scaler->info->dest_Uaddr,
		(unsigned int)scaler->info->dest_Vaddr);
	if ((scaler->info->src_fmt < VIOC_IMG_FMT_COMP) &&
		(scaler->info->dest_fmt > VIOC_IMG_FMT_COMP)) {
		VIOC_WDMA_SetImageR2YEnable(pSC_WDMABase, 1);
	} else {
		VIOC_WDMA_SetImageR2YEnable(pSC_WDMABase, 0);
	}

	VIOC_WDMA_SetImageEnable(pSC_WDMABase, 0);
	VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK);
	 // wdma status register all clear.
	spin_unlock_irq(&(scaler->data->cmd_lock));

	if (scaler->info->responsetype  == SCALER_POLLING) {
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule__3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[cert_int02_c_violation : FALSE] */
		/* coverity[cert_int31_c_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		ret = wait_event_interruptible_timeout(
			scaler->data->poll_wq,
			scaler->data->block_operating == 0U,
			msecs_to_jiffies(500));
		if (ret <= 0) {
			scaler->data->block_operating = 0;
			(void)pr_warn(
				"[WAR][SCALER] %s():  time out(%d), line(%d).\n",
				__func__, ret, __LINE__);
		}
		// look up table use
		if (scaler->info->lut.use_lut != 0U) {
			tcc_set_lut_enable(
				VIOC_LUT + scaler->info->lut.use_lut_number,
				0U);
			scaler->info->lut.use_lut = 0U;
		}
	} else if (scaler->info->responsetype == SCALER_NOWAIT) {
		if ((scaler->info->viqe_onthefly & 0x2U) != 0U) {
			scaler->data->block_operating = 0U;
		}
	} else {
		/* avoid MISRA C-2012 Rule 15.7 */
	}

	return ret;
}

static int tcc_scaler_data_copy_run(
	const struct scaler_drv_type *scaler, const struct SCALER_DATA_COPY_TYPE *copy_info)
{
	int ret = 0;
	void __iomem *pSC_RDMABase = scaler->rdma.reg;
	void __iomem *pSC_WMIXBase = scaler->wmix.reg;
	void __iomem *pSC_WDMABase = scaler->wdma.reg;
	void __iomem *pSC_SCALERBase = scaler->sc.reg;

	dprintk("%s():\n", __func__);
	dprintk(
		"Src  : addr:0x%x 0x%x 0x%x  fmt:%d\n",
		copy_info->src_y_addr, copy_info->src_u_addr,
		copy_info->src_v_addr, copy_info->src_fmt);
	dprintk(
		"Dest: addr:0x%x 0x%x 0x%x  fmt:%d\n",
		copy_info->dst_y_addr, copy_info->dst_u_addr,
		copy_info->dst_v_addr, copy_info->dst_fmt);
	dprintk(
		"Size : W:%d  H:%d\n", copy_info->img_width,
		copy_info->img_height);


	spin_lock_irq(&(scaler->data->cmd_lock));

	VIOC_RDMA_SetImageFormat(pSC_RDMABase,
		copy_info->src_fmt);
	VIOC_RDMA_SetImageSize(pSC_RDMABase,
		copy_info->img_width, copy_info->img_height);
	VIOC_RDMA_SetImageOffset(pSC_RDMABase,
		copy_info->src_fmt, copy_info->img_width);
	VIOC_RDMA_SetImageBase(pSC_RDMABase,
		(unsigned int)copy_info->src_y_addr,
		(unsigned int)copy_info->src_u_addr,
		(unsigned int)copy_info->src_v_addr);

	VIOC_SC_SetBypass(pSC_SCALERBase, 0U);
	VIOC_SC_SetDstSize(pSC_SCALERBase,
		copy_info->img_width, copy_info->img_height);
	VIOC_SC_SetOutSize(pSC_SCALERBase,
		copy_info->img_width, copy_info->img_height);
	VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
	(void)VIOC_CONFIG_PlugIn(scaler->sc.id, scaler->rdma.id);
	VIOC_SC_SetUpdate(pSC_SCALERBase);
	VIOC_RDMA_SetImageEnable(pSC_RDMABase); // SoC guide info.


	if (get_vioc_index(scaler->rdma.id) == get_vioc_index(VIOC_RDMA07)) {
		ret = VIOC_CONFIG_WMIXPath(scaler->rdma.id, 0);
	} else {
		ret = VIOC_CONFIG_WMIXPath(scaler->rdma.id, 1);
	}

	VIOC_WMIX_SetSize(pSC_WMIXBase,
		copy_info->img_width, copy_info->img_height);
	VIOC_WMIX_SetUpdate(pSC_WMIXBase);

	VIOC_WDMA_SetImageFormat(pSC_WDMABase,
		copy_info->dst_fmt);
	VIOC_WDMA_SetImageSize(pSC_WDMABase,
		copy_info->img_width, copy_info->img_height);
	VIOC_WDMA_SetImageOffset(pSC_WDMABase,
		copy_info->dst_fmt, copy_info->img_width);
	VIOC_WDMA_SetImageBase(pSC_WDMABase,
		(unsigned int)copy_info->dst_y_addr,
		(unsigned int)copy_info->dst_u_addr,
		(unsigned int)copy_info->dst_v_addr);
	VIOC_WDMA_SetImageEnable(pSC_WDMABase, 0/*OFF*/);
	VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK);
	// wdma status register all clear.

	spin_unlock_irq(&(scaler->data->cmd_lock));

	if (copy_info->rsp_type == (unsigned int)SCALER_POLLING) {
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
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[cert_int02_c_violation : FALSE] */
		/* coverity[cert_int31_c_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		ret = wait_event_interruptible_timeout(
			scaler->data->poll_wq,
			scaler->data->block_operating == 0,
			msecs_to_jiffies(500));
		if (ret <= 0) {
			scaler->data->block_operating = 0;
			(void)pr_warn(
				"[WAR][SCALER] wmixer time out: %d, Line: %d.\n",
				ret, __LINE__);
		}
	}

	return ret;
}

static unsigned int scaler_drv_poll(struct file *filp, poll_table *wait)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct scaler_drv_type *scaler = dev_get_drvdata(misc->parent);
	unsigned int ret = 0;

	if (scaler->data == NULL) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_poll;
	}

	poll_wait(filp, &(scaler->data->poll_wq), wait);
	spin_lock_irq(&(scaler->data->poll_lock));
	if (scaler->data->block_operating == 0U) {
		ret = ((unsigned int)POLLIN|(unsigned int)POLLRDNORM);
	}
	spin_unlock_irq(&(scaler->data->poll_lock));

err_poll:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static irqreturn_t scaler_drv_handler(int irq, void *client_data)
{
	irqreturn_t ret;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct scaler_drv_type *scaler =
		(struct scaler_drv_type *)client_data;

	(void)irq;

	if (is_vioc_intr_activatied(
		u32_to_s32(scaler->vioc_intr->id), scaler->vioc_intr->bits) == false) {
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			ret = IRQ_NONE;
			goto err_handler;
	}
	(void)vioc_intr_clear(
		u32_to_s32(scaler->vioc_intr->id),
		scaler->vioc_intr->bits);

	dprintk(
		"%s(): block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d).\n",
		__func__, scaler->data->block_operating,
		scaler->data->block_waiting,
		scaler->data->cmd_count,
		scaler->data->poll_count);

	if (scaler->data->block_operating >= 1U) {
		/* Prevent KCS */
		scaler->data->block_operating = 0U;
	}

	wake_up_interruptible(&(scaler->data->poll_wq));

	if (scaler->data->block_waiting != 0U) {
		wake_up_interruptible(&scaler->data->cmd_wq);
	}

	// look up table use
	if (scaler->info->lut.use_lut != 0U)		{
		tcc_set_lut_enable(
			VIOC_LUT + scaler->info->lut.use_lut_number,
			0U);
		scaler->info->lut.use_lut = 0U;
	}

#if defined(CONFIG_VIOC_SAR)
	if (scaler->info->sar.enable) {

		scaler->sar.enable = 0;
		(void)VIOC_CONFIG_PlugOut(VIOC_SAR0);
		VIOC_SAR_POWER_ONOFF(0);
	}
#endif//
	if ((bool)VIOC_CONFIG_DMAPath_Support()) {
		#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC_DECOMP)
		unsigned int component_num = s32_to_u32(VIOC_CONFIG_DMAPath_Select(scaler->rdma.id));
		#endif
		#ifdef CONFIG_VIOC_MAP_DECOMP
		if ((component_num >= VIOC_MC0) && (component_num <= (VIOC_MC0 + VIOC_MC_MAX))) {
			(void)VIOC_CONFIG_DMAPath_UnSet(u32_to_s32(VIOC_MC1));
			(void)VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, scaler->rdma.id);
		}
		#endif
		#ifdef CONFIG_VIOC_DTRC_DECOMP
		if ((component_num >= VIOC_DTRC0) && (component_num <= (VIOC_DTRC0 + VIOC_DTRC_MAX))) {
			(void)VIOC_CONFIG_DMAPath_UnSet(u32_to_s32(VIOC_DTRC1));
			ret = VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, scaler->rdma.id);
		}
		#endif
	} else {
		#if defined(CONFIG_VIOC_MAP_DECOMP) && (defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X))
		(void)VIOC_CONFIG_MCPath(scaler->wmix.id, scaler->rdma.id);
		#endif
	}

	ret = IRQ_HANDLED;

/* coverity[misra_c_2012_rule_15_5_violation : FALSE] */
err_handler:
	return ret;

}

static long scaler_drv_ioctl(
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_5_9_violation : FALSE] */
	struct scaler_drv_type	*scaler = dev_get_drvdata(misc->parent);
	// long timeout_jiff = msecs_to_jiffies(200U);

//	void __iomem *pSC_RDMABase = scaler->rdma.reg;
//	SCALER_PLUGIN_Type scaler_plugin;

	struct SCALER_DATA_COPY_TYPE copy_info;
	int ret = 0;

	dprintk(
		"%s(): cmd(%d), block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d).\n",
		__func__, cmd, scaler->data->block_operating,
		scaler->data->block_waiting,
		scaler->data->cmd_count,
		scaler->data->poll_count);

	switch (cmd) {
	case TCC_SCALER_IOCTRL:
	case TCC_SCALER_IOCTRL_KERENL:
		mutex_lock(&scaler->data->io_mutex);
		if (scaler->data->block_operating != 0U) {
			scaler->data->block_waiting = 1U;
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
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[cert_int02_c_violation : FALSE] */
			/* coverity[cert_int31_c_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			ret = wait_event_interruptible_timeout(
				scaler->data->cmd_wq,
				scaler->data->block_operating == 0,
				msecs_to_jiffies(200));
			if (ret <= 0) {
				scaler->data->block_operating = 0;
				(void)pr_warn(
					"[WAR][SCALER] %s(%d): timed_out block_operation(%d), cmd_count(%d).\n",
					__func__, ret,
					scaler->data->block_waiting,
					scaler->data->cmd_count);
			}
			ret = 0;
		}

		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
		if (cmd == TCC_SCALER_IOCTRL_KERENL) {
			(void)memcpy(scaler->info,
				(struct SCALER_TYPE *)arg,
				sizeof(struct SCALER_TYPE));
		} else {
			if ((bool)copy_from_user(scaler->info,
				(struct SCALER_TYPE *)arg,
				sizeof(struct SCALER_TYPE))) {
				(void)pr_err(
					"[ERR][SCALER] %s(): Not Supported copy_from_user(%d).\n",
					__func__, cmd);
				ret = -EFAULT;
			}
		}

		if (ret >= 0) {
			if (scaler->data->block_operating >= 1U) {
				(void)pr_info(
					"[INF][SCALER] %s(): block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d).\n",
					__func__, scaler->data->block_operating,
					scaler->data->block_waiting,
					scaler->data->cmd_count,
					scaler->data->poll_count);
			}

//				convert_image_format(scaler);

			scaler->data->block_waiting = 0U;
			scaler->data->block_operating = 1U;
			ret = tcc_scaler_run(scaler);
			if (ret < 0) {
				scaler->data->block_operating = 0U;
			}
		}
		mutex_unlock(&scaler->data->io_mutex);
		break;

	case TCC_SCALER_VIOC_DATA_COPY:
		mutex_lock(&scaler->data->io_mutex);
		if (scaler->data->block_operating != 0U) {
			scaler->data->block_waiting = 1U;
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
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[cert_int02_c_violation : FALSE] */
			/* coverity[cert_int31_c_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			ret = wait_event_interruptible_timeout(
				scaler->data->cmd_wq,
				scaler->data->block_operating == 0U,
				msecs_to_jiffies(200));
			if (ret <= 0) {
				scaler->data->block_operating = 0;
				(void)pr_warn(
					"[WAR][SCALER] %s(%d): wmixer 0 timed_out block_operation(%d), cmd_count(%d).\n",
					__func__, ret,
					scaler->data->block_waiting,
					scaler->data->cmd_count);
			}
			ret = 0;
		}

		/* coverity[cert_int36_c_violation : FALSE] */
		if ((bool)copy_from_user(
			&copy_info, (struct SCALER_DATA_COPY_TYPE *)arg,
			sizeof(struct SCALER_DATA_COPY_TYPE))) {
			(void)pr_err(
				"[ERR][SCALER] %s(): Not Supported copy_from_user(%d)\n",
				__func__, cmd);
			ret = -EFAULT;
		}

		if (ret >= 0) {
			if (scaler->data->block_operating >= 1U) {
				(void)pr_info("[INF][SCALER] %s(): block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d).\n",
				__func__, scaler->data->block_operating,
				scaler->data->block_waiting,
				scaler->data->cmd_count,
				scaler->data->poll_count);
			}

			scaler->data->block_waiting = 0U;
			scaler->data->block_operating = 1U;
			ret = tcc_scaler_data_copy_run(scaler, &copy_info);
			if (ret < 0) {
				scaler->data->block_operating = 0U;
			}
		}
		mutex_unlock(&scaler->data->io_mutex);
		break;

	default:
		(void)pr_err(
			"[ERR][SCALER] %s(): Not Supported SCALER0_IOCTL(%d).\n",
			__func__, cmd);
		break;
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long scaler_drv_compat_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	/* coverity[cert_exp37_c_violation : FALSE] */
	/* coverity[cert_int31_c_violation : FALSE] */
	/* coverity[cert_int02_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	return scaler_drv_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int scaler_drv_release(struct inode *pinode, struct file *filp)
{
	int ret = 0;
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct scaler_drv_type	*pscaler = dev_get_drvdata(misc->parent);

	(void)pinode;

	dprintk(
		"%s(): In -release(%d), block(%d), wait(%d), cmd(%d), irq(%d)\n",
		__func__, pscaler->data->dev_opened,
		pscaler->data->block_operating,
		pscaler->data->block_waiting,
		pscaler->data->cmd_count,
		pscaler->data->irq_reged);

	if (pscaler->data->dev_opened > 0U) {
		pscaler->data->dev_opened--;
	}

	if (pscaler->data->dev_opened == 0U) {
		if (pscaler->data->block_operating != 0U) {
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
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[cert_int02_c_violation : FALSE] */
			/* coverity[cert_int31_c_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			ret = wait_event_interruptible_timeout(
				pscaler->data->cmd_wq,
				pscaler->data->block_operating == 0U,
				msecs_to_jiffies(200));
			if (ret <= 0) {
				(void)pr_warn(
					 "[WAR][SCALER] %s(%d): timed_out block_operation:%d, cmd_count:%d.\n",
					 __func__, ret,
					 pscaler->data->block_waiting,
					 pscaler->data->cmd_count);
			}
		}

		if ((bool)pscaler->data->irq_reged) {
			(void)vioc_intr_clear(
				u32_to_s32(pscaler->vioc_intr->id),
				pscaler->vioc_intr->bits);
			(void)vioc_intr_disable(
				u32_to_s32(pscaler->irq),
				u32_to_s32(pscaler->vioc_intr->id),
				pscaler->vioc_intr->bits);
			(void)free_irq(pscaler->irq, pscaler);
			pscaler->data->irq_reged = 0;
		}

		(void)VIOC_CONFIG_PlugOut(pscaler->sc.id);

		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC_DECOMP)
			int component_num = VIOC_CONFIG_DMAPath_Select(pscaler->rdma.id);
			#endif

			#ifdef CONFIG_VIOC_MAP_DECOMP
			if ((component_num >= u32_to_s32(VIOC_MC0)) && (component_num <= u32_to_s32(VIOC_MC0 + VIOC_MC_MAX))) {
				(void)VIOC_CONFIG_DMAPath_UnSet(u32_to_s32(pscaler->rdma.id));
				tca_map_convter_swreset(VIOC_MC1);
			}
			#endif
			#ifdef CONFIG_VIOC_DTRC_DECOMP
			if ((component_num >= VIOC_DTRC0) && (component_num <= (VIOC_DTRC0 + VIOC_DTRC_MAX))) {
				(void)VIOC_CONFIG_DMAPath_UnSet(pscaler->rdma.id);
				tca_dtrc_convter_swreset(VIOC_DTRC1);
			}
			#endif

			(void)VIOC_CONFIG_DMAPath_Set(pscaler->rdma.id, pscaler->rdma.id);
		} else {
			#if defined(CONFIG_VIOC_MAP_DECOMP) && (defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X))
			(void)VIOC_CONFIG_MCPath(pscaler->wmix.id, pscaler->rdma.id);
			#endif
		}

		VIOC_CONFIG_SWReset(pscaler->wdma.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(pscaler->wmix.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(pscaler->sc.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(pscaler->rdma.id, VIOC_CONFIG_RESET);

		VIOC_CONFIG_SWReset(pscaler->rdma.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(pscaler->sc.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(pscaler->wmix.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(pscaler->wdma.id, VIOC_CONFIG_CLEAR);

		pscaler->data->block_operating = 0U;
		pscaler->data->block_waiting = 0U;
		pscaler->data->poll_count = 0U;
		pscaler->data->cmd_count = 0U;
	}

	if (pscaler->pclk != NULL) {
		clk_disable_unprepare(pscaler->pclk);
	}
	dprintk(
		"%s():  Out - release(%d).\n",
		__func__, pscaler->data->dev_opened);

	return 0;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int scaler_drv_open(struct inode *pinode, struct file *filp)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct scaler_drv_type	*scaler = dev_get_drvdata(misc->parent);

	int ret = 0;
	(void)pinode;

	dprintk(
		"%s():  In -open(%d), block(%d), wait(%d), cmd(%d), irq(%d :: %d/0x%x)\n",
		__func__, scaler->data->dev_opened,
		scaler->data->block_operating,
		scaler->data->block_waiting, scaler->data->cmd_count,
		scaler->data->irq_reged,
		scaler->vioc_intr->id, scaler->vioc_intr->bits);

	if (scaler->pclk != NULL) {
		(void)clk_prepare_enable(scaler->pclk);
	}

	if (!(bool)scaler->data->irq_reged) {
		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_RESET);
		#ifdef CONFIG_VIOC_MAP_DECOMP
		tca_map_convter_swreset(VIOC_MC1);
		#endif
		#ifdef CONFIG_VIOC_DTRC_DECOMP
		tca_dtrc_convter_swreset(VIOC_DTRC1);
		#endif
		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_CLEAR);

		// VIOC_CONFIG_StopRequest(1);
		synchronize_irq(scaler->irq);
		(void)vioc_intr_clear(
			u32_to_s32(scaler->vioc_intr->id), scaler->vioc_intr->bits);
		ret = request_irq(
			scaler->irq, scaler_drv_handler,
			IRQF_SHARED, scaler->misc->name, scaler);
		if ((bool)ret) {
			if (scaler->pclk != NULL) {
				clk_disable_unprepare(scaler->pclk);
			}
			(void)pr_err("[ERR][SCALER] failed(ret %d) to aquire %s request_irq(%d).\n",
				ret, scaler->misc->name, scaler->irq);
			ret = -EFAULT;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto err_scaler_drv_open;
		}
		dprintk(
			"success(ret %d) to aquire %s request_irq(%d).\n",
			ret, scaler->misc->name, scaler->irq);
		(void)vioc_intr_enable(u32_to_s32(scaler->irq),
			u32_to_s32(scaler->vioc_intr->id), scaler->vioc_intr->bits);

		if ((bool)VIOC_CONFIG_DMAPath_Support()) {
			int component_num =
				VIOC_CONFIG_DMAPath_Select(scaler->rdma.id);

			if (component_num < 0) {
				(void)pr_info("[INF][SCALER] %s: RDMA :%d dma path selection none\n",
					__func__, scaler->rdma.id);
			} else if ((component_num < u32_to_s32(VIOC_RDMA00)) &&
				(component_num > u32_to_s32(VIOC_RDMA00 + VIOC_RDMA_MAX))) {
				(void)VIOC_CONFIG_DMAPath_UnSet(component_num);
			} else {
				/* avoid MISRA C-2012 Rule 15.7 */
			}

			// It is default path selection(VRDMA)
			(void)VIOC_CONFIG_DMAPath_Set(
				scaler->rdma.id, scaler->rdma.id);
		} else {
			#if defined(CONFIG_VIOC_MAP_DECOMP) \
				&& (defined(CONFIG_ARCH_TCC803X) \
				|| defined(CONFIG_ARCH_TCC805X))
			(void)VIOC_CONFIG_MCPath(scaler->wmix.id, scaler->rdma.id);
			#endif
		}

		scaler->data->irq_reged = 1;
	}

	scaler->data->dev_opened++;
	dprintk(
		"%s():  Out - open(%d).\n",
		__func__, scaler->data->dev_opened);

err_scaler_drv_open:
	return ret;
}

static const struct file_operations scaler_drv_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl		= scaler_drv_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= scaler_drv_compat_ioctl,
#endif
	.mmap			= scaler_drv_mmap,
	.open			= scaler_drv_open,
	.release		= scaler_drv_release,
	.poll			= scaler_drv_poll,
};

static int scaler_drv_probe(struct platform_device *pdev)
{
	struct scaler_drv_type *scaler;
	struct device_node *dev_np;
	unsigned int index;
	int ret = -ENODEV;
	int itemp;

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler = kzalloc(sizeof(
		struct scaler_drv_type), GFP_KERNEL);
	if (scaler == NULL) {
		/* Prevent KCS */
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_misc_alloc;
	}

	scaler->pclk = of_clk_get(pdev->dev.of_node, 0);
	/* coverity[misra_c_2012_rule_11_2_violation : FALSE] */
	if (IS_ERR(scaler->pclk)) {
		scaler->pclk = NULL;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler->misc = kzalloc(
		sizeof(struct miscdevice), GFP_KERNEL);
	if (scaler->misc == NULL) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_misc_alloc;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler->info = kzalloc(
		sizeof(struct SCALER_TYPE), GFP_KERNEL);
	if (scaler->info == NULL) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_info_alloc;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler->data = kzalloc(
		sizeof(struct scaler_data), GFP_KERNEL);
	if (scaler->data == NULL) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_data_alloc;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	scaler->vioc_intr = kzalloc(
		sizeof(struct vioc_intr_type), GFP_KERNEL);
	if (scaler->vioc_intr == NULL) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_vioc_intr_alloc;
	}

	itemp = of_alias_get_id(pdev->dev.of_node, "scaler-drv");
	if(itemp >= 0) {
		scaler->id = (unsigned int)itemp;
	} else {
		scaler->id = 0;
	}

	/* register scaler discdevice */
	scaler->misc->minor = MISC_DYNAMIC_MINOR;
	scaler->misc->fops = &scaler_drv_fops;
	scaler->misc->name =
		kasprintf(GFP_KERNEL, "scaler%d", scaler->id);
	scaler->misc->parent = &pdev->dev;

	ret = misc_register(scaler->misc);
	if ((bool)ret) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_misc_register;
	}

	dev_np = of_parse_phandle(
		pdev->dev.of_node, "rdmas", 0);
	if (dev_np != NULL) {
		(void)of_property_read_u32_index(
			pdev->dev.of_node, "rdmas", 1, &index);
		scaler->rdma.reg = VIOC_RDMA_GetAddress(index);
		scaler->rdma.id = index;
	} else {
		(void)pr_warn(
			"[WAR][SCALER] could not find rdma node of %s driver.\n",
			scaler->misc->name);
		scaler->rdma.reg = NULL;
	}

	dev_np = of_parse_phandle(
		pdev->dev.of_node, "scalers", 0);
	if (dev_np != NULL) {
		(void)of_property_read_u32_index(
			pdev->dev.of_node, "scalers", 1, &index);
		scaler->sc.reg = VIOC_SC_GetAddress(index);
		scaler->sc.id = index;
	} else {
		(void)pr_warn(
			"[WAR][SCALER] could not find scaler node of %s driver.\n",
			scaler->misc->name);
		scaler->sc.reg = NULL;
	}

	dev_np = of_parse_phandle(
		pdev->dev.of_node, "wmixs", 0);
	if (dev_np != NULL) {
		(void)of_property_read_u32_index(
			pdev->dev.of_node, "wmixs", 1, &index);
		scaler->wmix.reg = VIOC_WMIX_GetAddress(index);
		scaler->wmix.id = index;
	} else {
		(void)pr_warn(
			"[WAR][SCALER] could not find wmix node of %s driver.\n",
			scaler->misc->name);
		scaler->wmix.reg = NULL;
	}

	dev_np = of_parse_phandle(pdev->dev.of_node, "wdmas", 0);
	if (dev_np != NULL) {
		unsigned int uitemp;
		(void)of_property_read_u32_index(
			pdev->dev.of_node, "wdmas", 1, &index);
		scaler->wdma.reg = VIOC_WDMA_GetAddress(index);
		scaler->wdma.id = index;
		uitemp = get_vioc_index(index);
		itemp = (int)uitemp;
		scaler->irq = irq_of_parse_and_map(
						dev_np, itemp);
		uitemp = s32_to_u32(VIOC_INTR_WD0) + get_vioc_index(scaler->wdma.id);
		itemp = (int)uitemp;
		scaler->vioc_intr->id = s32_to_u32(itemp);
		scaler->vioc_intr->bits = VIOC_WDMA_IREQ_EOFR_MASK;
	} else {
		(void)pr_warn(
			"[WAR][SCALER] could not find wdma node of %s driver.\n",
			scaler->misc->name);
		scaler->wdma.reg = NULL;
	}

	of_property_read_u32(
		pdev->dev.of_node, "settop_support",
		&scaler->settop_support);

	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_17_7_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	spin_lock_init(&(scaler->data->poll_lock));
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_17_7_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	spin_lock_init(&(scaler->data->cmd_lock));

	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_17_7_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	mutex_init(&(scaler->data->io_mutex));

	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	init_waitqueue_head(&(scaler->data->poll_wq));
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	init_waitqueue_head(&(scaler->data->cmd_wq));

	platform_set_drvdata(pdev, scaler);

	(void)pr_info(
		"[INF][SCALER] %s: id:%d, Scaler Driver Initialized\n",
		pdev->name, scaler->id);
	ret = 0;
	/* coverity[misra_c_2012_rule_15_5_violation : FALSE] */
	goto probe_return;

	// misc_deregister(scaler->misc);
err_misc_register:
	kfree(scaler->vioc_intr);
err_vioc_intr_alloc:
	kfree(scaler->data);
err_data_alloc:
	kfree(scaler->info);
err_info_alloc:
	kfree(scaler->misc);
err_misc_alloc:
	kfree(scaler);

	(void)pr_err(
		"[ERR][SCALER] %s: %s: err ret:%d\n",
		__func__, pdev->name, ret);
probe_return:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int scaler_drv_remove(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct scaler_drv_type *pscaler =
		(struct scaler_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(pscaler->misc);
	kfree(pscaler->vioc_intr);
	kfree(pscaler->data);
	kfree(pscaler->info);
	kfree(pscaler->misc);
	kfree(pscaler);
	return 0;
}

static int scaler_drv_suspend(
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct platform_device *pdev, pm_message_t state)
{
	(void)pdev;
	(void)state;
	(void)pr_info("[INF][SCALER] %s\n", __func__);
	return 0;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int scaler_drv_resume(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct scaler_drv_type *scaler =
		(struct scaler_drv_type *)platform_get_drvdata(pdev);

	if (scaler->data->dev_opened > 0U) {
		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_RESET);

		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_CLEAR);
	}

	return 0;
}

static const struct of_device_id scaler_of_match[] = {
	{ .compatible = "telechips,scaler_drv" },
	{}
};
MODULE_DEVICE_TABLE(of, scaler_of_match);

static struct platform_driver scaler_driver = {
	.probe = scaler_drv_probe,
	.remove = scaler_drv_remove,
	.suspend = scaler_drv_suspend,
	.resume = scaler_drv_resume,
	.driver = {
		.name = "scaler_pdev",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(scaler_of_match),
#endif
	},
};

static int __init scaler_drv_init(void)
{
	return platform_driver_register(&scaler_driver);
}

static void __exit scaler_drv_exit(void)
{
	platform_driver_unregister(&scaler_driver);
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
module_init(scaler_drv_init);
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
module_exit(scaler_drv_exit);

/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_AUTHOR("Telechips.");
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_DESCRIPTION("Telechips Scaler Driver");
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_LICENSE("GPL");

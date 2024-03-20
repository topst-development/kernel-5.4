// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
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
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_vin.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_ddicfg.h> // is_VIOC_REMAP

#if defined(CONFIG_ARCH_TCC805X)
#include <video/tcc/vioc_pvric_fbdc.h>
#endif
static int vioc_base_irq_num[4] = {
	0,
};

int vioc_intr_enable(int irq, int id, unsigned int mask)
{
	void __iomem *reg;
	int sub_id;
	unsigned int type_clr_offset;
	int ret = -1;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#endif
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3) {
			/* Prevent KCS warning */
			sub_id = VIOC_INTR_DEV3 - VIOC_INTR_DISP_OFFSET;
		} else {
			/* Prevent KCS warning */
			sub_id = id - VIOC_INTR_DEV0;
		}

		reg = VIOC_DISP_GetAddress((unsigned int)sub_id);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			(__raw_readl(reg + DIM) & ~(mask & VIOC_DISP_INT_MASK)),
			reg + DIM);
		ret = 0;
		break;
#else
		sub_id = id - VIOC_INTR_DEV0;

		reg = VIOC_DISP_GetAddress((unsigned int)sub_id);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			(__raw_readl(reg + DIM) & ~(mask & VIOC_DISP_INT_MASK)),
			reg + DIM);
		ret = 0;
		break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		/* clera irq status */
		sub_id = id - VIOC_INTR_RD0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_RDMA_GetAddress((unsigned int)sub_id) + RDMASTAT;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((mask & VIOC_RDMA_INT_MASK), reg);

		/* enable irq */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_RDMA_GetAddress((unsigned int)sub_id) + RDMAIRQMSK;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) & ~(mask & VIOC_RDMA_INT_MASK), reg);
		ret = 0;
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		sub_id = id - VIOC_INTR_WD0;

		/* clera irq status */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)sub_id) + WDMAIRQSTS_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);

		/* enable irq */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)sub_id) + WDMAIRQMSK_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) & ~(mask & VIOC_WDMA_INT_MASK), reg);
		ret = 0;
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		sub_id = id - VIOC_INTR_WD_OFFSET
			- VIOC_INTR_WD0;

		/* clera irq status */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)sub_id) + WDMAIRQSTS_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);

		/* enable irq */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)sub_id) + WDMAIRQMSK_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) & ~(mask & VIOC_WDMA_INT_MASK), reg);
		ret = 0;
		break;
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		sub_id = id - VIOC_INTR_WD_OFFSET
			- VIOC_INTR_WD_OFFSET2 - VIOC_INTR_WD0;

		/* clera irq status */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)sub_id) + WDMAIRQSTS_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);

		/* enable irq */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)sub_id) + WDMAIRQMSK_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) & ~(mask & VIOC_WDMA_INT_MASK), reg);
		ret = 0;
		break;
#endif
	/*
	 * VIN_INT[31]: Not Used
	 * VIN_INT[19]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[18]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[17]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[16]: Enable interrupt if 1 / Disable interrupt if 0
	 */
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		sub_id = id - VIOC_INTR_VIN0;

		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_VIN_GetAddress((unsigned int)sub_id * 2U) + VIN_INT;

		/* clera irq status */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((__raw_readl(reg) | (mask & VIOC_VIN_INT_MASK)), reg);

		/* enable irq */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) | ((mask & VIOC_VIN_INT_ENABLE) << 16U),
			reg);
		ret = 0;
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		sub_id = id - VIOC_INTR_VIN_OFFSET
			- VIOC_INTR_VIN0;

		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_VIN_GetAddress((unsigned int)sub_id * 2U) + VIN_INT;

		/* clera irq status */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((__raw_readl(reg) | (mask & VIOC_VIN_INT_MASK)), reg);

		/* enable irq */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) | ((mask & VIOC_VIN_INT_ENABLE) << 16U),
			reg);
		ret = 0;
		break;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		sub_id = id - VIOC_INTR_AFBCDEC0;

		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_PVRIC_FBDC_GetAddress((unsigned int)sub_id) + PVRICSTS;

		/* clera irq status */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((__raw_readl(reg) | (mask & VIOC_PVRIC_FBDC_INT_MASK)),
			reg);

		/* enable irq */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) & (~(mask & VIOC_PVRIC_FBDC_INT_MASK) << 16U), reg);
		ret = 0;
		break;
#endif // defined(CONFIG_VIOC_PVRIC_FBDC)
	default:
		(void)pr_err("[ERR][VIOC_INTR] %s: id(%d) is wrong.\n",
		       __func__, id);
		ret = -1;
		break;
	}

	if (ret < 0) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	if (irq == vioc_base_irq_num[0]) {
		/* Prevent KCS warning */
		type_clr_offset = IRQMASKCLR0_0_OFFSET;
	} else if (irq == vioc_base_irq_num[1]) {
		/* Prevent KCS warning */
		type_clr_offset = IRQMASKCLR1_0_OFFSET;
	} else if (irq == vioc_base_irq_num[2]) {
		/* Prevent KCS warning */
		type_clr_offset = IRQMASKCLR2_0_OFFSET;
	} else if (irq == vioc_base_irq_num[3]) {
		/* Prevent KCS warning */
		type_clr_offset = IRQMASKCLR3_0_OFFSET;
	} else {
		(void)pr_err("[ERR][VIOC_INTR] %s-%d :: irq(%d) is wierd.\n",
		       __func__, __LINE__, irq);
		ret = -1;
	}
#else
	type_clr_offset = IRQMASKCLR0_0_OFFSET;
	ret = 0;
#endif
	if (ret < 0) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	reg = VIOC_IREQConfig_GetAddress();

#if defined(CONFIG_ARCH_TCC897X)
	if (id < 32) {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((u32)1U << (unsigned int)id, reg + type_clr_offset);
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((u32)1U << ((unsigned int)id - 32U), reg + type_clr_offset + 0x4);
	}
	ret = 0;
#else
	if (id >= 64) {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((u32)1U << ((unsigned int)id - 64U), reg + type_clr_offset + 0x8U);
	} else if (id >= 32) {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((u32)1U << ((unsigned int)id - 32U), reg + type_clr_offset + 0x4U);
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((u32)1U << (unsigned int)id, reg + type_clr_offset);
	}
	ret = 0;
#endif

FUNC_EXIT:
	return ret;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(vioc_intr_enable);

int vioc_intr_disable(int irq, int id, unsigned int mask)
{
	void __iomem *reg;
	int sub_id;
	unsigned int do_irq_mask = 1U;
	unsigned int type_set_offset;
	int ret = -1;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#endif
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3) {
			/* Prevent KCS warning */
			sub_id = VIOC_INTR_DEV3 - VIOC_INTR_DISP_OFFSET;
		}
		else {
			/* Prevent KCS warning */
			sub_id = id - VIOC_INTR_DEV0;
		}

		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_DISP_GetAddress((unsigned int)sub_id) + DIM;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_DISP_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_DISP_INT_MASK)
		    != VIOC_DISP_INT_MASK) {
		    /* Prevent KCS warning */
			do_irq_mask = 0U;
		}
		ret = 0;
		break;
#else
		sub_id = id - VIOC_INTR_DEV0;

		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_DISP_GetAddress((unsigned int)sub_id) + DIM;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_DISP_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_DISP_INT_MASK)
		    != VIOC_DISP_INT_MASK) {
		    /* Prevent KCS warning */
			do_irq_mask = 0U;
		}
		ret = 0;
		break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		sub_id = id - VIOC_INTR_RD0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_RDMA_GetAddress((unsigned int)sub_id) + RDMAIRQMSK;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_RDMA_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_RDMA_INT_MASK)
		    != VIOC_RDMA_INT_MASK) {
		    /* Prevent KCS warning */
			do_irq_mask = 0U;
		}
		ret = 0;
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		sub_id = id - VIOC_INTR_WD0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)sub_id) + WDMAIRQMSK_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_WDMA_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_WDMA_INT_MASK)
		    != VIOC_WDMA_INT_MASK) {
		    /* Prevent KCS warning */
			do_irq_mask = 0U;
		}
		ret = 0;
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		sub_id = id - VIOC_INTR_WD_OFFSET
			- VIOC_INTR_WD0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)sub_id) + WDMAIRQMSK_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_WDMA_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_WDMA_INT_MASK)
		    != VIOC_WDMA_INT_MASK) {
		    /* Prevent KCS warning */
			do_irq_mask = 0U;
		}
		ret = 0;
		break;
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		sub_id = id - VIOC_INTR_WD_OFFSET - VIOC_INTR_WD_OFFSET2
			- VIOC_INTR_WD0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)sub_id) + WDMAIRQMSK_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_WDMA_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_WDMA_INT_MASK)
		    != VIOC_WDMA_INT_MASK) {
		    /* Prevent KCS warning */
			do_irq_mask = 0U;
		}
		ret = 0;
		break;
#endif
	/*
	 * VIN_INT[31]: Not Used
	 * VIN_INT[19]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[18]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[17]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[16]: Enable interrupt if 1 / Disable interrupt if 0
	 */
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		sub_id = id - VIOC_INTR_VIN0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_VIN_GetAddress((unsigned int)sub_id * 2U) + VIN_INT;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) & ~((mask & VIOC_VIN_INT_ENABLE) << 16),
			reg);
		//if ((__raw_readl(reg) & VIOC_VIN_INT_MASK) !=
		// VIOC_VIN_INT_MASK) do_irq_mask = 0;
		ret = 0;
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		sub_id = id - VIOC_INTR_VIN_OFFSET
			- VIOC_INTR_VIN0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_VIN_GetAddress((unsigned int)sub_id * 2U) + VIN_INT;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			__raw_readl(reg) & ~((mask & VIOC_VIN_INT_ENABLE) << 16U),
			reg);
		//if ((__raw_readl(reg) & VIOC_VIN_INT_MASK) !=
		//VIOC_VIN_INT_MASK) do_irq_mask = 0;
		ret = 0;
		break;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		sub_id = id - VIOC_INTR_AFBCDEC0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_PVRIC_FBDC_GetAddress((unsigned int)sub_id) + PVRICSTS;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(__raw_readl(reg)
			& ((mask & VIOC_PVRIC_FBDC_INT_MASK) << 16U), reg);
		//if ((__raw_readl(reg) & (VIOC_PVRIC_FBDC_INT_MASK << 16U))
		//	!= (VIOC_PVRIC_FBDC_INT_MASK << 16))
		//	do_irq_mask = 0;
		ret = 0;
		break;
#endif // defined(CONFIG_VIOC_PVRIC_FBDC)
	default:
		(void)pr_err("[ERR][VIOC_INTR] %s: id(%d) is wrong.\n",
			__func__, id);
		ret = 0;
		break;
	}

	if (do_irq_mask == 1U) {
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		if (irq == vioc_base_irq_num[0]) {
			/* Prevent KCS warning */
			type_set_offset = IRQMASKSET0_0_OFFSET;
		} else if (irq == vioc_base_irq_num[1]) {
			/* Prevent KCS warning */
			type_set_offset = IRQMASKSET1_0_OFFSET;
		} else if (irq == vioc_base_irq_num[2]) {
			/* Prevent KCS warning */
			type_set_offset = IRQMASKSET2_0_OFFSET;
		} else if (irq == vioc_base_irq_num[3]) {
			/* Prevent KCS warning */
			type_set_offset = IRQMASKSET3_0_OFFSET;
		} else {
			(void)pr_err("[ERR][VIOC_INTR] %s-%d :: irq(%d) is wierd.\n",
			       __func__, __LINE__, irq);
			ret = -1;
		}
#else
		type_set_offset = IRQMASKSET0_0_OFFSET;
		ret = 0;
#endif
		if (ret < 0) {
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto FUNC_EXIT;
		}
		reg = VIOC_IREQConfig_GetAddress();
#if defined(CONFIG_ARCH_TCC897X)
		if (id < 32) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel((u32)1U << (unsigned int)id, reg + type_set_offset);
		} else {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel(
				(u32)1U << ((unsigned int)id - 32U), reg + type_set_offset + 0x4);
		}
#else
		if (id >= 64) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel(
				(u32)1U << ((unsigned int)id - 64U), reg + type_set_offset + 0x8U);
		} else if (id >= 32) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel(
				(u32)1U << ((unsigned int)id - 32U), reg + type_set_offset + 0x4U);
		} else {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel((u32)1U << (unsigned int)id, reg + type_set_offset);
		}
#endif
	}
	ret = 0;

FUNC_EXIT:
	return ret;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(vioc_intr_disable);

/* coverity[HIS_metric_violation : FALSE] */
unsigned int vioc_intr_get_status(int id)
{
	const void __iomem *reg;
	unsigned int ret = 0U;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = 0U;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#endif
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3) {
			/* Prevent KCS warning */
			id -= VIOC_INTR_DISP_OFFSET;
		}
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_DISP_GetAddress((unsigned int)id) + DSTATUS;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		ret = (__raw_readl(reg) & VIOC_DISP_INT_MASK);
		break;
#else
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_DISP_GetAddress((unsigned int)id) + DSTATUS;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		ret = (__raw_readl(reg) & VIOC_DISP_INT_MASK);
		break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		id -= VIOC_INTR_RD0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_RDMA_GetAddress((unsigned int)id) + RDMASTAT;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		ret = (__raw_readl(reg) & VIOC_RDMA_INT_MASK);
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		id -= VIOC_INTR_WD0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQSTS_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		ret = (__raw_readl(reg) & VIOC_WDMA_INT_MASK);
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		id -= (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQSTS_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		ret = (__raw_readl(reg) & VIOC_WDMA_INT_MASK);
		break;
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		id -= (VIOC_INTR_WD_OFFSET - VIOC_INTR_WD_OFFSET2
		       - VIOC_INTR_WD0);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQSTS_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		ret = (__raw_readl(reg) & VIOC_WDMA_INT_MASK);
		break;
#endif
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		id -= VIOC_INTR_VIN0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		ret = (__raw_readl(reg) & VIOC_VIN_INT_MASK);
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		id -= (VIOC_INTR_VIN_OFFSET + VIOC_INTR_VIN0);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		ret = (__raw_readl(reg) & VIOC_VIN_INT_MASK);
		break;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		id -= VIOC_INTR_AFBCDEC0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_PVRIC_FBDC_GetAddress((unsigned int)id) + PVRICSTS;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		ret = (__raw_readl(reg) & VIOC_PVRIC_FBDC_INT_MASK);
		break;
#endif // defined(CONFIG_VIOC_PVRIC_FBDC)
	default:
		(void)pr_err("[ERR][VIOC_INTR] %s: id(%d) is wrong.\n",
			   __func__, id);
		ret = 0U;
		break;
	}
FUNC_EXIT:
	return ret;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(vioc_intr_get_status);

bool check_vioc_irq_status(const void __iomem *reg, int id)
{
	unsigned int flag;
	bool ret = (bool)false;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = (bool)false;
	} else {
		if (id < 32) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			flag = ((__raw_readl(reg + IRQSELECT0_0_OFFSET) & ((u32)1U << (unsigned int)id)) != 0U) ?
				(__raw_readl(reg + SYNCSTATUS0_OFFSET) & ((u32)1U << (unsigned int)id)) :
				(__raw_readl(reg + RAWSTATUS0_OFFSET) & ((u32)1U << (unsigned int)id));
		} else if (id < 64) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			flag = ((__raw_readl(reg + IRQMASKCLR0_1_OFFSET)
				& ((u32)1U << ((unsigned int)id - 32U))) != 0U) ?
				(__raw_readl(reg + SYNCSTATUS1_OFFSET)
				& ((u32)1U << ((unsigned int)id - 32U))) :
				(__raw_readl(reg + RAWSTATUS1_OFFSET)
				& ((u32)1U << ((unsigned int)id - 32U)));
		}
#if !defined(CONFIG_ARCH_TCC897X)
		else {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			flag = ((__raw_readl(reg + IRQMASKCLR0_2_OFFSET)
				& ((u32)1U << ((unsigned int)id - 64U))) != 0U) ?
				(__raw_readl(reg + SYNCSTATUS2_OFFSET)
				& ((u32)1U << ((unsigned int)id - 64U))) :
				(__raw_readl(reg + RAWSTATUS2_OFFSET)
				& ((u32)1U << ((unsigned int)id - 64U)));
		}
#endif

		if (flag != 0U) {
			ret = (bool)true;
		} else {
			ret = (bool)false;
		}
	}
	return ret;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(check_vioc_irq_status);

/* coverity[HIS_metric_violation : FALSE] */
bool is_vioc_intr_activatied(int id, unsigned int mask)
{
	const void __iomem *reg;
	bool ret = (bool)false;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = (bool)false;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#endif
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3) {
			/* Prevent KCS warning */
			id -= VIOC_INTR_DISP_OFFSET;
		}

		reg = VIOC_DISP_GetAddress((unsigned int)id);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg + DSTATUS) & (mask & VIOC_DISP_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#else
		reg = VIOC_DISP_GetAddress((unsigned int)id);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg + DSTATUS) & (mask & VIOC_DISP_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		id -= VIOC_INTR_RD0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_RDMA_GetAddress((unsigned int)id) + RDMASTAT;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg) & (mask & VIOC_RDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		id -= VIOC_INTR_WD0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQSTS_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		id -= (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQSTS_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		id -= (VIOC_INTR_WD_OFFSET - VIOC_INTR_WD_OFFSET2
		       - VIOC_INTR_WD0);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQSTS_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#endif
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		id -= VIOC_INTR_VIN0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg) & (mask & VIOC_VIN_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		id -= (VIOC_INTR_VIN_OFFSET + VIOC_INTR_VIN0);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg) & (mask & VIOC_VIN_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			ret = (bool)false;
		}
		break;
#endif
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		id -= VIOC_INTR_AFBCDEC0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_PVRIC_FBDC_GetAddress((unsigned int)id) + PVRICSTS;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg) & (mask & VIOC_PVRIC_FBDC_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			ret = (bool)false;
		}
		break;
#endif // defined(CONFIG_VIOC_PVRIC_FBDC)
	default:
		(void)pr_err("[ERR][VIOC_INTR] %s: id(%d) is wrong.\n",
			   __func__, id);
		ret = (bool)false;
		break;
	}
FUNC_EXIT:
	return ret;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(is_vioc_intr_activatied);

/* coverity[HIS_metric_violation : FALSE] */
bool is_vioc_intr_unmasked(int id, unsigned int mask)
{
	const void __iomem *reg;
	bool ret = (bool)false;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = (bool)false;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#endif
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3) {
			/* Prevent KCS warning */
			id -= VIOC_INTR_DISP_OFFSET;
		}

		reg = VIOC_DISP_GetAddress((unsigned int)id);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg + DIM) & (mask & VIOC_DISP_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
#else
		reg = VIOC_DISP_GetAddress((unsigned int)id);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg + DIM) & (mask & VIOC_DISP_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		id -= VIOC_INTR_RD0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_RDMA_GetAddress((unsigned int)id) + RDMAIRQMSK;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg) & (mask & VIOC_RDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		id -= VIOC_INTR_WD0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQMSK_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		id -= (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQMSK_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		id -= (VIOC_INTR_WD_OFFSET - VIOC_INTR_WD_OFFSET2
		       - VIOC_INTR_WD0);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQMSK_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
#endif
	/*
	 * VIN_INT[31]: Not Used
	 * VIN_INT[19]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[18]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[17]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[16]: Enable interrupt if 1 / Disable interrupt if 0
	 */
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		id -= VIOC_INTR_VIN0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg) & ((mask & VIOC_VIN_INT_ENABLE) << 16U)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		id -= (VIOC_INTR_VIN_OFFSET + VIOC_INTR_VIN0);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg) & ((mask & VIOC_VIN_INT_ENABLE) << 16U)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		id -= VIOC_INTR_AFBCDEC0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_PVRIC_FBDC_GetAddress((unsigned int)id) + PVRICSTS;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((__raw_readl(reg) & ((mask & VIOC_PVRIC_FBDC_INT_MASK) << 16U)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
#endif // defined(CONFIG_VIOC_PVRIC_FBDC)
	default:
		(void)pr_err("[ERR][VIOC_INTR] %s: id(%d) is wrong.\n",
			   __func__, id);
		ret = (bool)false;
		break;
	}
FUNC_EXIT:
	return ret;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(is_vioc_intr_unmasked);

/* coverity[HIS_metric_violation : FALSE] */
bool is_vioc_display_device_intr_masked(int id, unsigned int mask)
{
	const void __iomem *reg;
	bool ret = (bool)false;
	u32 reg_val;

	if (id < 0) {
		ret = (bool)true;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	} else {
		if (get_vioc_type((unsigned int)id) != get_vioc_type(VIOC_DISP)) {
			ret = (bool)true;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto FUNC_EXIT;
		}
	}

	switch (get_vioc_index((unsigned int)id)) {
	case get_vioc_index(VIOC_DISP0):
	case get_vioc_index(VIOC_DISP1):
	#if defined(VIOC_DISP2)
	case get_vioc_index(VIOC_DISP2):
	#endif
	#if defined(VIOC_DISP3)
	case get_vioc_index(VIOC_DISP3):
	#endif
		ret = (bool)false;
		break;
	default:
		ret = (bool)true;
		break;
	}
	if (ret == (bool)true) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	reg = (void __iomem *)VIOC_DISP_GetAddress((unsigned int)id);
	if (reg == NULL) {
		ret = (bool)true;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	reg_val = __raw_readl(reg + DIM);
	if (((reg_val & mask) & VIOC_DISP_INT_MASK) != 0U) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		ret = (bool)true;
	} else {
		ret = (bool)false;
	}
FUNC_EXIT:
	return ret;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(is_vioc_display_device_intr_masked);

int vioc_intr_clear(int id, unsigned int mask)
{
	void __iomem *reg;
	int ret = -1;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#endif
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3) {
			/* Prevent KCS warning */
			id -= VIOC_INTR_DISP_OFFSET;
		}

		reg = VIOC_DISP_GetAddress((unsigned int)id);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((mask & VIOC_DISP_INT_MASK), reg + DSTATUS);
		ret = 0;
		break;
#else
		reg = VIOC_DISP_GetAddress((unsigned int)id);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((mask & VIOC_DISP_INT_MASK), reg + DSTATUS);
		ret = 0;
		break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		id -= VIOC_INTR_RD0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_RDMA_GetAddress((unsigned int)id) + RDMASTAT;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((mask & VIOC_RDMA_INT_MASK), reg);
		ret = 0;
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		id -= VIOC_INTR_WD0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQSTS_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);
		ret = 0;
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		id -= (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQSTS_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);
		ret = 0;
		break;
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		id -= (VIOC_INTR_WD_OFFSET - VIOC_INTR_WD_OFFSET2
		       - VIOC_INTR_WD0);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQSTS_OFFSET;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);
		ret = 0;
		break;
#endif
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		id -= VIOC_INTR_VIN0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((__raw_readl(reg) | (mask & VIOC_VIN_INT_MASK)), reg);
		ret = 0;
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		id -= (VIOC_INTR_VIN_OFFSET + VIOC_INTR_VIN0);
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((__raw_readl(reg) | (mask & VIOC_VIN_INT_MASK)), reg);
		ret = 0;
		break;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		id -= VIOC_INTR_AFBCDEC0;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = VIOC_PVRIC_FBDC_GetAddress((unsigned int)id) + PVRICSTS;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((__raw_readl(reg) | (mask & VIOC_PVRIC_FBDC_INT_MASK)),
			reg);
		ret = 0;
		break;
#endif // defined(CONFIG_VIOC_PVRIC_FBDC)
	default:
	(void)pr_err("[ERR][VIOC_INTR] %s: id(%d) is wrong.\n",
		       __func__, id);
		ret = -1;
		break;
	}

FUNC_EXIT:
	return ret;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(vioc_intr_clear);

void vioc_intr_initialize(void)
{
	void __iomem *reg = VIOC_IREQConfig_GetAddress();
	int i;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(0xffffffff, reg + IRQMASKCLR0_0_OFFSET);
	__raw_writel(0xffffffff, reg + IRQMASKCLR0_1_OFFSET);
#if !defined(CONFIG_ARCH_TCC897X)
	__raw_writel(0xffffffff, reg + IRQMASKCLR0_2_OFFSET);
#endif

	/* disp irq mask & status clear */
#if defined(CONFIG_ARCH_TCC805X)
	for (i = 0;
	     i < (VIOC_INTR_DEV3 - (VIOC_INTR_DISP_OFFSET + VIOC_INTR_DEV0));
	     i++) {
#else
	for (i = 0; i < (VIOC_INTR_DEV2 - VIOC_INTR_DEV0); i++) {
#endif
		reg = VIOC_DISP_GetAddress((unsigned int)i);
		if (reg == NULL) {
			/* Prevent KCS warning */
			continue;
		}
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(VIOC_DISP_INT_MASK, reg + DIM);
		__raw_writel(VIOC_DISP_INT_MASK, reg + DSTATUS);
	}

	/* rdma irq mask & status clear */
	for (i = 0; i < (VIOC_INTR_RD11 - VIOC_INTR_RD0); i++) {
		reg = VIOC_RDMA_GetAddress((unsigned int)i);
		if (reg == NULL) {
			/* Prevent KCS warning */
			continue;
		}
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(VIOC_RDMA_INT_MASK, reg + RDMAIRQMSK);
		__raw_writel(VIOC_RDMA_INT_MASK, reg + RDMASTAT);
	}

		/* wdma irq mask & status clear */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#if defined(CONFIG_ARCH_TCC805X)
	for (i = 0;
		i < (
			VIOC_INTR_WD13 - (
				VIOC_INTR_WD_OFFSET + VIOC_INTR_WD_OFFSET2 +
				VIOC_INTR_WD0)); i++) {
#else
	for (i = 0;
	     i < (VIOC_INTR_WD12 - (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0));
	     i++) {
#endif
		reg = VIOC_WDMA_GetAddress((unsigned int)i);
		if (reg == NULL) {
			/* Prevent KCS warning */
			continue;
		}
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQMSK_OFFSET);
		__raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQSTS_OFFSET);
	}
#else
	for (i = 0; i < (VIOC_INTR_WD8 - VIOC_INTR_WD0); i++) {
		reg = VIOC_WDMA_GetAddress((unsigned int)i);
		if (reg == NULL) {
			continue;
		}
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQMSK_OFFSET);
		__raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQSTS_OFFSET);
	}
#endif
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(vioc_intr_initialize);

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC803X)
static void vioc_intr_disable_core_intr(void)
{
	void __iomem *reg = VIOC_IREQConfig_GetAddress();

	#if defined(CONFIG_TCC803X_CA7S) || defined(CONFIG_TCC805X_CA53Q)
	(void)pr_info("[INF][VIOC_INTR] disable all VIOC interrupts of VIOC0_IRQI\n");
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(0xffffffff, reg + IRQMASKSET0_0_OFFSET);
	__raw_writel(0xffffffff, reg + IRQMASKSET0_1_OFFSET);
	__raw_writel(0xffffffff, reg + IRQMASKSET0_2_OFFSET);
	#else
	(void)pr_info("[INF][VIOC_INTR] disable all VIOC interrupts of VIOC1_IRQI\n");
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(0xffffffff, reg + IRQMASKSET1_0_OFFSET);
	__raw_writel(0xffffffff, reg + IRQMASKSET1_1_OFFSET);
	__raw_writel(0xffffffff, reg + IRQMASKSET1_2_OFFSET);
	#endif
}
#endif

static int __init vioc_intr_init(void)
{
	struct device_node *ViocIntr_np;

	ViocIntr_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_intr");

	if (ViocIntr_np == NULL) {
		(void)pr_info("[INF][VIOC_INTR] disabled [this is mandatory for vioc display]\n");
	} else {
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		int i = 0;
		unsigned int temp = 0;/* avoid CERT-C Integers Rule INT31-C */
		for (i = 0; i < (int)VIOC_IRQ_MAX; i++) {
			temp = irq_of_parse_and_map(ViocIntr_np, i);
			if (temp < (UINT_MAX / 2U)) {
  				vioc_base_irq_num[i] = (int)temp;
				(void)pr_info("[INF][VIOC_INTR] vioc-intr%d: irq %d\n", i,
					vioc_base_irq_num[i]);
			}
		}
#else
		vioc_base_irq_num[0] = irq_of_parse_and_map(ViocIntr_np, 0);
		(void)pr_info("[INF][VIOC_INTR] vioc-intr%d : %d\n",
			0, vioc_base_irq_num[0]);
#endif

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC803X)
	vioc_intr_disable_core_intr();
#endif
	}
	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
fs_initcall(vioc_intr_init);

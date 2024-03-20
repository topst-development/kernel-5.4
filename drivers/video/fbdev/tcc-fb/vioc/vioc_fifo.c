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
#include <linux/kernel.h>
#include <linux/of_address.h>

#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_ddicfg.h> // is_VIOC_REMAP
#include <video/tcc/vioc_fifo.h>

static struct device_node *ViocFifo_np;
static void __iomem *pFIFO_reg[VIOC_FIFO_MAX] = {0};

void VIOC_FIFO_ConfigEntry(void __iomem *reg, const unsigned int *buf)
{
	unsigned int E_EMPTY = 1U; // emergency empty
	unsigned int E_FULL = 1U;  // emergency full
	unsigned int WMT = 0U;    // wdma mode - time
	unsigned int NENTRY =
		4U;	    // frame memory number  ->  max. frame count is 4.
	unsigned int RMT = 0U; // rdma mode - time
	unsigned int idxBuf;
	u32 val;

	for (idxBuf = 0U; idxBuf < NENTRY; idxBuf++) {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(buf[idxBuf], reg + (CH0_BASE + (idxBuf * 0x04U)));
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + CH0_CTRL0)
	       & ~(CH_CTRL_EEMPTY_MASK | CH_CTRL_EFULL_MASK | CH_CTRL_WMT_MASK
		   | CH_CTRL_NENTRY_MASK | CH_CTRL_RMT_MASK));
	val |= (((E_EMPTY & 0x3U) << CH_CTRL_EEMPTY_SHIFT)
		| ((E_FULL & 0x3U) << CH_CTRL_EFULL_SHIFT)
		| ((WMT & 0x3U) << CH_CTRL_WMT_SHIFT)
		| ((NENTRY & 0x3FU) << CH_CTRL_NENTRY_SHIFT)
		| ((RMT & 0x1U) << CH_CTRL_RMT_SHIFT));
	__raw_writel(val, reg + CH0_CTRL0);
}

void VIOC_FIFO_ConfigDMA(
	void __iomem *reg, unsigned int nWDMA, unsigned int nRDMA0,
	unsigned int nRDMA1, unsigned int nRDMA2)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + CH0_CTRL1)
	       & ~(CH_CTRL1_RDMA2SEL_MASK | CH_CTRL1_RDMA1SEL_MASK
		   | CH_CTRL1_RDMA0SEL_MASK | CH_CTRL1_WDMASEL_MASK));
	val |= (((nRDMA2 & 0xFU) << CH_CTRL1_RDMA2SEL_SHIFT)
		| ((nRDMA1 & 0xFU) << CH_CTRL1_RDMA1SEL_SHIFT)
		| ((nRDMA0 & 0xFU) << CH_CTRL1_RDMA0SEL_SHIFT)
		| ((nWDMA & 0xFU) << CH_CTRL1_WDMASEL_SHIFT));
	__raw_writel(val, reg + CH0_CTRL1);
}

void VIOC_FIFO_SetEnable(
	void __iomem *reg, unsigned int nWDMA, unsigned int nRDMA0,
	unsigned int nRDMA1, unsigned int nRDMA2)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + CH0_CTRL0)
	       & ~(CH_CTRL_REN2_MASK | CH_CTRL_REN1_MASK | CH_CTRL_REN0_MASK
		   | CH_CTRL_WEN_MASK));
	val |= (((nRDMA2 & 0x1U) << CH_CTRL_REN2_SHIFT)
		| ((nRDMA1 & 0x1U) << CH_CTRL_REN1_SHIFT)
		| ((nRDMA0 & 0x1U) << CH_CTRL_REN0_SHIFT)
		| ((nWDMA & 0x1U) << CH_CTRL_WEN_SHIFT));
	__raw_writel(val, reg + CH0_CTRL0);
}

void __iomem *VIOC_FIFO_GetAddress(unsigned int vioc_id)
{
	unsigned int Num = get_vioc_index(vioc_id);
	void __iomem *ret = NULL;

	if ((Num >= VIOC_FIFO_MAX) || (pFIFO_reg[Num] == NULL)) {
		(void)pr_err("[ERR][FIFO] %s num:%d Max fifo num:%d\n", __func__, Num,
	       VIOC_FIFO_MAX);
		ret = NULL;
	} else {
		ret = pFIFO_reg[Num];
	}

	return ret;
}

static int __init vioc_fifo_init(void)
{
	unsigned int i = 0;

	ViocFifo_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_fifo");
	if (ViocFifo_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][FIFO] vioc-fifo: disabled\n");
	} else {
		for (i = 0; i < VIOC_FIFO_MAX; i++) {
			pFIFO_reg[i] = (void __iomem *)of_iomap(
				ViocFifo_np,
				(is_VIOC_REMAP != 0U) ? ((int)i + (int)VIOC_FIFO_MAX) : (int)i);

			if (pFIFO_reg[i] != NULL) {
				/* Prevent KCS warning */
				(void)pr_info("[INF][FIFO] vioc-fifo%d\n", i);
			}
		}
	}
	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_fifo_init);

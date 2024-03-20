// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * linux/drivers/video/fbdev/tcc-fb/vioc/vioc_pxdemux.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2008-2009 Telechips
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
#include <video/tcc/vioc_pxdemux.h>

static void __iomem *pPXDEMUX_reg;

/* VIOC_PXDEMUX_SetConfigure
 * Set pixel demuxer configuration
 * idx : pixel demuxer idx
 * lr : pixel demuxer output - mode 0: even/odd, 1: left/right
 * bypass : pixel demuxer bypass mode
 * width : pixel demuxer width - single port: real width, dual port: half width
 */
void VIOC_PXDEMUX_SetConfigure(
	unsigned int idx, unsigned int lr, unsigned int bypass,
	unsigned int width)
{
	void __iomem *reg = VIOC_PXDEMUX_GetAddress();

	if (idx >= (unsigned int)PD_IDX_MAX) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][P_DEMUX] %s in error, invalid parameter(idx: %d)\n",
	       __func__, idx);
	} else {
		if (reg != NULL) {
			unsigned int offset = ((idx != 0U) ? PD1_CFG : PD0_CFG);
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			unsigned int val =
				(__raw_readl(reg + offset)
				& ~(PD_CFG_WIDTH_MASK | PD_CFG_MODE_MASK
					| PD_CFG_LR_MASK | PD_CFG_BP_MASK));

			val |= (((width & 0xFFFU) << PD_CFG_WIDTH_SHIFT)
				| ((bypass & 0x1U) << PD_CFG_BP_SHIFT)
				| ((lr & 0x1U) << PD_CFG_LR_SHIFT));
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel(val, reg + offset);
		}
	}
}

/* VIOC_PXDEMUX_SetDataSwap
 * Set pixel demuxter output data swap
 * idx: pixel demuxer idx
 * ch : pixel demuxer output channel(0, 1, 2, 3)
 * set : pixel demuxer data swap mode
 */
void VIOC_PXDEMUX_SetDataSwap(
	unsigned int idx, unsigned int ch, unsigned int set)
{
	void __iomem *reg = VIOC_PXDEMUX_GetAddress();

	if ((idx >= (unsigned int)PD_IDX_MAX)) {
		(void)pr_err("[ERR][P_DEMUX] %s: invalid parameter(idx: %d, ch: %d, set: 0x%08x)\n",
			__func__, idx, ch, set);
	} else {
		if (reg != NULL) {
			unsigned int offset, val;

			offset = (idx != 0U) ? PD1_CFG : PD0_CFG;
			switch (ch) {
			case 0:
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				val = (__raw_readl(reg + offset)
					& ~(PD_CFG_SWAP0_MASK));
				val |= ((set & 0x3U) << PD_CFG_SWAP0_SHIFT);
				__raw_writel(val, reg + offset);
				break;
			case 1:
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				val = (__raw_readl(reg + offset)
					& ~(PD_CFG_SWAP1_MASK));
				val |= ((set & 0x3U) << PD_CFG_SWAP1_SHIFT);
				__raw_writel(val, reg + offset);
				break;
			case 2:
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				val = (__raw_readl(reg + offset)
					& ~(PD_CFG_SWAP2_MASK));
				val |= ((set & 0x3U) << PD_CFG_SWAP2_SHIFT);
				__raw_writel(val, reg + offset);
				break;
			case 3:
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				val = (__raw_readl(reg + offset)
					& ~(PD_CFG_SWAP3_MASK));
				val |= ((set & 0x3U) << PD_CFG_SWAP3_SHIFT);
				__raw_writel(val, reg + offset);
				break;
			default:
				(void)pr_err("[ERR][P_DEMUX] %s: invalid parameter(%d, %d)\n",
					__func__, ch, set);
				break;
			}
		}
	}
}

/* VIOC_PXDEMUX_SetMuxOutput
 * Set MUX output selection
 * mux: the type of mux (PD_MUX3TO1_TYPE, PD_MUX5TO1_TYPE)
 * select : the selecti
 */
void VIOC_PXDEMUX_SetMuxOutput(
	PD_MUX_TYPE mux, unsigned int ch, unsigned int select,
	unsigned int enable)
{
	void __iomem *reg = VIOC_PXDEMUX_GetAddress();
	unsigned int val;
	int ret = -1;


	if (reg != NULL) {
		switch (mux) {
		case PD_MUX3TO1_TYPE:
			switch (ch) {
			case 0:
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				val = (__raw_readl(reg + MUX3_1_SEL0)
				       & ~(MUX3_1_SEL_SEL_MASK));
				val |= ((select & 0x3U) << MUX3_1_SEL_SEL_SHIFT);
				__raw_writel(val, reg + MUX3_1_SEL0);
				val = (__raw_readl(reg + MUX3_1_EN0)
				       & ~(MUX3_1_EN_EN_MASK));
				val |= ((enable & 0x1U) << MUX3_1_EN_EN_SHIFT);
				__raw_writel(val, reg + MUX3_1_EN0);
				ret = 0;
				break;
			case 1:
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				val = (__raw_readl(reg + MUX3_1_SEL1)
				       & ~(MUX3_1_SEL_SEL_MASK));
				val |= ((select & 0x3U) << MUX3_1_SEL_SEL_SHIFT);
				__raw_writel(val, reg + MUX3_1_SEL1);
				val = (__raw_readl(reg + MUX3_1_EN1)
				       & ~(MUX3_1_EN_EN_MASK));
				val |= ((enable & 0x1U) << MUX3_1_EN_EN_SHIFT);
				__raw_writel(val, reg + MUX3_1_EN1);
				ret = 0;
				break;
			default:
				ret = -1;
				break;
			}
			break;
		case PD_MUX5TO1_TYPE:
			switch (ch) {
			case 0:
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				val = (__raw_readl(reg + MUX5_1_SEL0)
				       & ~(MUX5_1_SEL_SEL_MASK));
				val |= ((select & 0x7U) << MUX5_1_SEL_SEL_SHIFT);
				__raw_writel(val, reg + MUX5_1_SEL0);
				val = (__raw_readl(reg + MUX5_1_EN0)
				       & ~(MUX5_1_EN_EN_MASK));
				val |= ((enable & 0x1U) << MUX5_1_EN_EN_SHIFT);
				__raw_writel(val, reg + MUX5_1_EN0);
				ret = 0;
				break;
			case 1:
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				val = (__raw_readl(reg + MUX5_1_SEL1)
				       & ~(MUX5_1_SEL_SEL_MASK));
				val |= ((select & 0x7U) << MUX5_1_SEL_SEL_SHIFT);
				__raw_writel(val, reg + MUX5_1_SEL1);
				val = (__raw_readl(reg + MUX5_1_EN1)
				       & ~(MUX5_1_EN_EN_MASK));
				val |= ((enable & 0x1U) << MUX5_1_EN_EN_SHIFT);
				__raw_writel(val, reg + MUX5_1_EN1);
				ret = 0;
				break;
			case 2:
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				val = (__raw_readl(reg + MUX5_1_SEL2)
				       & ~(MUX5_1_SEL_SEL_MASK));
				val |= ((select & 0x7U) << MUX5_1_SEL_SEL_SHIFT);
				__raw_writel(val, reg + MUX5_1_SEL2);
				val = (__raw_readl(reg + MUX5_1_EN2)
				       & ~(MUX5_1_EN_EN_MASK));
				val |= ((enable & 0x1U) << MUX5_1_EN_EN_SHIFT);
				__raw_writel(val, reg + MUX5_1_EN2);
				ret = 0;
				break;
			case 3:
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				val = (__raw_readl(reg + MUX5_1_SEL3)
				       & ~(MUX5_1_SEL_SEL_MASK));
				val |= ((select & 0x7U) << MUX5_1_SEL_SEL_SHIFT);
				__raw_writel(val, reg + MUX5_1_SEL3);
				val = (__raw_readl(reg + MUX5_1_EN3)
				       & ~(MUX5_1_EN_EN_MASK));
				val |= ((enable & 0x1U) << MUX5_1_EN_EN_SHIFT);
				__raw_writel(val, reg + MUX5_1_EN3);
				ret = 0;
				break;
			default:
				ret = -1;
				break;
			}
			break;
		case PD_MUX_TYPE_MAX:
		default:
			ret = -1;
			break;
		}
	}
	if (ret < 0) {
		(void)pr_err("[ERR][P_DEMUX] %s: invalid parameter(mux: %d, ch: %d)\n",
	       __func__, mux, ch);
	}
}

/* VIOC_PXDEMUX_SetDataPath
 * Set Data output format of pixel demuxer
 * ch : channel number of pixel demuxer 5x1
 * path : path number of pixel demuxer 5x1
 * set : data output format of pixel demuxer 5x1
 */
void VIOC_PXDEMUX_SetDataPath(
	unsigned int ch, unsigned int data_path, unsigned int set)
{
	void __iomem *reg = VIOC_PXDEMUX_GetAddress();
	unsigned int offset;
	int ret = -1;

	if ((data_path >= (unsigned int)PD_TXOUT_SEL_MAX)) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][P_DEMUX] %s: invalid parameter(ch: %d, path: %d)\n",
			__func__, ch, data_path);
	} else {
		if (reg != NULL) {
			switch (ch) {
			case 0:
				offset = TXOUT_SEL0_0;
				ret = 0;
				break;
			case 1:
				offset = TXOUT_SEL0_1;
				ret = 0;
				break;
			case 2:
				offset = TXOUT_SEL0_2;
				ret = 0;
				break;
			case 3:
				offset = TXOUT_SEL0_3;
				ret = 0;
				break;
			default:
				ret = -1;
				break;
			}
			if (ret < 0) {
				(void)pr_err("[ERR][P_DEMUX] %s: invalid parameter(ch: %d, path: %d)\n",
					__func__, ch, data_path);
			} else {
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				__raw_writel((set & 0xFFFFFFFFU),
					reg + (offset + (0x4U * data_path)));
			}
		}
	}
}

/* VIOC_PXDEMUX_SetDataArray
 * Set the data output format of pixel demuxer 5x1
 * ch : channel number of pixel demuxer 5x1
 * data : the array included the data output format
 */
void VIOC_PXDEMUX_SetDataArray(
	unsigned int ch, unsigned int data[TXOUT_MAX_LINE][TXOUT_DATA_PER_LINE])
{
	const void __iomem *reg = VIOC_PXDEMUX_GetAddress();
	const unsigned int *lvdsdata = (unsigned int *)data;
	unsigned int idx, value, vioc_path;
	unsigned int data_0, data_1, data_2, data_3;

	if (ch >= (unsigned int)PD_MUX5TO1_IDX_MAX) {
		/* Prevent KCS warning */
		(void)pr_err("%s in error, invalid parameter(ch: %d)\n", __func__, ch);
	} else {
		if (reg != NULL) {
			for (idx = 0U; idx < (TXOUT_MAX_LINE * TXOUT_DATA_PER_LINE);
				idx += 4U) {
				data_0 = TXOUT_GET_DATA(idx);
				data_1 = TXOUT_GET_DATA(idx + 1U);
				data_2 = TXOUT_GET_DATA(idx + 2U);
				data_3 = TXOUT_GET_DATA(idx + 3U);

				vioc_path = idx / 4U;
				value =
					((lvdsdata[data_3] << 24U)
					| (lvdsdata[data_2] << 16U)
					| (lvdsdata[data_1] << 8U) | (lvdsdata[data_0]));
				VIOC_PXDEMUX_SetDataPath(ch, vioc_path, value);
			}
		}
	}
}

void __iomem *VIOC_PXDEMUX_GetAddress(void)
{
	if (pPXDEMUX_reg == NULL) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][P_DEMUX] %s pPD\n", __func__);
	}

	return pPXDEMUX_reg;
}

static int __init vioc_pxdemux_init(void)
{
	struct device_node *ViocPXDEMUX_np;

	ViocPXDEMUX_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_pxdemux");

	if (ViocPXDEMUX_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][P_DEMUX] vioc-pxdemux: disabled\n");
	} else {
		pPXDEMUX_reg =
			(void __iomem *)of_iomap(ViocPXDEMUX_np, 0);

		if (pPXDEMUX_reg != NULL) {
			/* Prevent KCS warning */
			(void)pr_info("[INF][P_DEMUX] vioc-pxdemux\n");
		}
	}
	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_pxdemux_init);

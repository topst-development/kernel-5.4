/* SPDX-License-Identifier: GPL-2.0-or-later */
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
#ifndef VIOC_FIFO_H
#define	VIOC_FIFO_H

#define	VIOC_FIFO_RDMA00	(0)
#define	VIOC_FIFO_RDMA01	(1)
#define	VIOC_FIFO_RDMA02	(2)
#define	VIOC_FIFO_RDMA03	(3)
#define	VIOC_FIFO_RDMA04	(4)
#define	VIOC_FIFO_RDMA05	(5)
#define	VIOC_FIFO_RDMA06	(6)
#define	VIOC_FIFO_RDMA07	(7)
#define	VIOC_FIFO_RDMA08	(8)
#define	VIOC_FIFO_RDMA09	(9)
#define	VIOC_FIFO_RDMA10	(10)
#define	VIOC_FIFO_RDMA11	(11)
#define	VIOC_FIFO_RDMA12	(12)
#define	VIOC_FIFO_RDMA13	(13)
#define	VIOC_FIFO_RDMA14	(14)
#define	VIOC_FIFO_RDMA15	(15)

#define	VIOC_FIFO_WDMA00	(0)
#define	VIOC_FIFO_WDMA01	(1)
#define	VIOC_FIFO_WDMA02	(2)
#define	VIOC_FIFO_WDMA03	(3)
#define	VIOC_FIFO_WDMA04	(4)
#define	VIOC_FIFO_WDMA05	(5)
#define	VIOC_FIFO_WDMA06	(6)
#define	VIOC_FIFO_WDMA07	(7)
#define	VIOC_FIFO_WDMA08	(8)

/*
 * Register offset
 */
#define CH0_CTRL0		(0x00U)
#define CH0_CTRL1		(0x04U)
#define CH0_IRQSTAT		(0x08U)
#define CH0_FIFOSTAT	(0x0CU)
#define CH1_CTRL0		(0x10U)
#define CH1_CTRL1		(0x14U)
#define CH1_IRQSTAT		(0x18U)
#define CH1_FIFOSTAT	(0x1CU)
#define CH0_BASE		(0x20U)
#define CH1_BASE		(0x60U)

/*
 *	CHk Control 0 Register
 */
#define CH_CTRL_IRE2_SHIFT		(31U) // Read EOF 2 Interrupt
#define CH_CTRL_IRE1_SHIFT		(30U) // Read EOF 1 Interrupt
#define CH_CTRL_IRE0_SHIFT		(29U) // Read EOF 0 Interrupt
#define CH_CTRL_IWE_SHIFT		(28U) // Write EOF Interrupt
#define CH_CTRL_IEE_SHIFT		(27U) // Emergency Empty Interrupt
#define CH_CTRL_IE_SHIFT		(26U) // Empty Interrupt
#define CH_CTRL_IEF_SHIFT		(25U) // Emergency Full Interrupt
#define CH_CTRL_IF_SHIFT		(24U) // Full Interrupt
#define CH_CTRL_EEMPTY_SHIFT	(18U) // Emergency Empty
#define CH_CTRL_EFULL_SHIFT		(16U) // Emergency Full
#define CH_CTRL_WMT_SHIFT		(14U) // WDMA mode
#define CH_CTRL_NENTRY_SHIFT	(8U)  // Frame Memory Number
#define CH_CTRL_RMT_SHIFT		(4U)  // RDMA Mode
#define CH_CTRL_REN2_SHIFT		(3U)  // Read Enable2
#define CH_CTRL_REN1_SHIFT		(2U)  // Read Enable1
#define CH_CTRL_REN0_SHIFT		(1U)  // Read Enable0
#define CH_CTRL_WEN_SHIFT		(0U)  //	Write Enable

#define CH_CTRL_IRE2_MASK		((u32)0x1U << CH_CTRL_IRE2_SHIFT)
#define CH_CTRL_IRE1_MASK		((u32)0x1U << CH_CTRL_IRE1_SHIFT)
#define CH_CTRL_IRE0_MASK		((u32)0x1U << CH_CTRL_IRE0_SHIFT)
#define CH_CTRL_IWE_MASK		((u32)0x1U << CH_CTRL_IWE_SHIFT)
#define CH_CTRL_IEE_MASK		((u32)0x1U << CH_CTRL_IEE_SHIFT)
#define CH_CTRL_IE_MASK			((u32)0x1U << CH_CTRL_IE_SHIFT)
#define CH_CTRL_IEF_MASK		((u32)0x1U << CH_CTRL_IEF_SHIFT)
#define CH_CTRL_IF_MASK			((u32)0x1U << CH_CTRL_IF_SHIFT)
#define CH_CTRL_EEMPTY_MASK		((u32)0x3U << CH_CTRL_EEMPTY_SHIFT)
#define CH_CTRL_EFULL_MASK		((u32)0x3U << CH_CTRL_EFULL_SHIFT)
#define CH_CTRL_WMT_MASK		((u32)0x3U << CH_CTRL_WMT_SHIFT)
#define CH_CTRL_NENTRY_MASK		((u32)0x3FU << CH_CTRL_NENTRY_SHIFT)
#define CH_CTRL_RMT_MASK		((u32)0x1U << CH_CTRL_RMT_SHIFT)
#define CH_CTRL_REN2_MASK		((u32)0x1U << CH_CTRL_REN2_SHIFT)
#define CH_CTRL_REN1_MASK		((u32)0x1U << CH_CTRL_REN1_SHIFT)
#define CH_CTRL_REN0_MASK		((u32)0x1U << CH_CTRL_REN0_SHIFT)
#define CH_CTRL_WEN_MASK		((u32)0x1U << CH_CTRL_WEN_SHIFT)

/*
 * CHk Control 1 Register
 */
#define CH_CTRL1_FRDMA_SHIFT		(16U)
#define CH_CTRL1_RDMA2SEL_SHIFT		(12U)
#define CH_CTRL1_RDMA1SEL_SHIFT		(8U)
#define CH_CTRL1_RDMA0SEL_SHIFT		(4U)
#define CH_CTRL1_WDMASEL_SHIFT		(0U)

#define CH_CTRL1_FRDMA_MASK			((u32)0x3U << CH_CTRL1_FRDMA_SHIFT)
#define CH_CTRL1_RDMA2SEL_MASK		((u32)0xFU << CH_CTRL1_RDMA2SEL_SHIFT)
#define CH_CTRL1_RDMA1SEL_MASK		((u32)0xFU << CH_CTRL1_RDMA1SEL_SHIFT)
#define CH_CTRL1_RDMA0SEL_MASK		((u32)0xFU << CH_CTRL1_RDMA0SEL_SHIFT)
#define CH_CTRL1_WDMASEL_MASK		((u32)0xFU << CH_CTRL1_WDMASEL_SHIFT)

/*
 * CHk Interrupt Status Register
 */
#define CH_IRQSTAT_REOF2_SHIFT		(7U)
#define CH_IRQSTAT_REOF1_SHIFT		(6U)
#define CH_IRQSTAT_REOF0_SHIFT		(5U)
#define CH_IRQSTAT_WEOF_SHIFT		(4U)
#define CH_IRQSTAT_EEMP_SHIFT		(3U)
#define CH_IRQSTAT_EMP_SHIFT		(2U)
#define CH_IRQSTAT_EFULL_SHIFT		(1U)
#define CH_IRQSTAT_FULL_SHIFT		(0U)

#define CH_IRQSTAT_REOF2_MASK		((u32)0x1U << CH_IRQSTAT_REOF2_SHIFT)
#define CH_IRQSTAT_REOF1_MASK		((u32)0x1U << CH_IRQSTAT_REOF1_SHIFT)
#define CH_IRQSTAT_REOF0_MASK		((u32)0x1U << CH_IRQSTAT_REOF0_SHIFT)
#define CH_IRQSTAT_WEOF_MASK		((u32)0x1U << CH_IRQSTAT_WEOF_SHIFT)
#define CH_IRQSTAT_EEMP_MASK		((u32)0x1U << CH_IRQSTAT_EEMP_SHIFT)
#define CH_IRQSTAT_EMP_MASK			((u32)0x1U << CH_IRQSTAT_EMP_SHIFT)
#define CH_IRQSTAT_EFULL_MASK		((u32)0x1U << CH_IRQSTAT_EFULL_SHIFT)
#define CH_IRQSTAT_FULL_MASK		((u32)0x1U << CH_IRQSTAT_FULL_SHIFT)

/*
 * CHk FIFO Status Register
 */
#define CH_FIFOSTAT_FILLED_SHIFT	(24U)
#define CH_FIFOSTAT_REOF2_SHIFT		(7U)
#define CH_FIFOSTAT_REOF1_SHIFT		(6U)
#define CH_FIFOSTAT_REOF0_SHIFT		(5U)
#define CH_FIFOSTAT_WEOF_SHIFT		(4U)
#define CH_FIFOSTAT_EEMP_SHIFT		(3U)
#define CH_FIFOSTAT_EMP_SHIFT		(2U)
#define CH_FIFOSTAT_EFULL_SHIFT		(1U)
#define CH_FIFOSTAT_FULL_SHIFT		(0U)

#define CH_FIFOSTAT_FILLED_MASK		((u32)0xFFU << CH_FIFOSTAT_FILLED_SHIFT)
#define CH_FIFOSTAT_REOF2_MASK		((u32)0x1U << CH_FIFOSTAT_REOF2_SHIFT)
#define CH_FIFOSTAT_REOF1_MASK		((u32)0x1U << CH_FIFOSTAT_REOF1_SHIFT)
#define CH_FIFOSTAT_REOF0_MASK		((u32)0x1U << CH_FIFOSTAT_REOF0_SHIFT)
#define CH_FIFOSTAT_WEOF_MASK		((u32)0x1U << CH_FIFOSTAT_WEOF_SHIFT)
#define CH_FIFOSTAT_EEMP_MASK		((u32)0x1U << CH_FIFOSTAT_EEMP_SHIFT)
#define CH_FIFOSTAT_EMP_MASK		((u32)0x1U << CH_FIFOSTAT_EMP_SHIFT)
#define CH_FIFOSTAT_EFULL_MASK		((u32)0x1U << CH_FIFOSTAT_EFULL_SHIFT)
#define CH_FIFOSTAT_FULL_MASK		((u32)0x1U << CH_FIFOSTAT_FULL_SHIFT)

/*
 * CHk BASE0~16 Base Register
 */
#define CH_BASE_BASE_SHIFT (0U)

#define CH_BASE_BASE_MASK ((u32)0xFFFFFFFFU << CH_BASE_BASE_SHIFT)

extern void VIOC_FIFO_ConfigEntry(
	void __iomem *reg, const unsigned int *buf);
extern void VIOC_FIFO_ConfigDMA(
	void __iomem *reg,
	unsigned int nWDMA, unsigned int nRDMA0,
	unsigned int nRDMA1, unsigned int nRDMA2);
extern void VIOC_FIFO_SetEnable(
	void __iomem *reg,
	unsigned int nWDMA, unsigned int nRDMA0,
	unsigned int nRDMA1, unsigned int nRDMA2);
extern void __iomem *VIOC_FIFO_GetAddress(
	unsigned int vioc_id);

#endif

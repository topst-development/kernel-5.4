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
#ifndef VIOC_TIMER_H
#define	VIOC_TIMER_H

#include <clk-tcc805x.h>
/*
 * Register offset
 */
#define USEC		(0x00U)
#define CURTIME		(0x04U)
#define TIMER0		(0x08U)
#define TIMER1		(0x0CU)
#define TIREQ0		(0x10U)
#define TIREQ1		(0x14U)
#define IRQMASK		(0x18U)
#define IRQSTAT		(0x1CU)

/*
 * Micro-second Control Register
 */
#define USEC_EN_SHIFT			(31U)
#define USEC_UINTDIV_SHIFT		(8U)
#define USEC_USECDIV_SHIFT		(0U)

#define USEC_EN_MASK ((u32)0x1U << USEC_EN_SHIFT)
#define USEC_UINTDIV_MASK ((u32)0xFFU << USEC_UINTDIV_SHIFT)
#define USEC_USECDIV_MASK ((u32)0xFFU << USEC_USECDIV_SHIFT)

/*
 * Current Time Register
 */
#define CURTIME_CURTIME_SHIFT		(0U)

#define CURTIME_CURTIME_MASK		((u32)0xFFFFFFFFU << CURTIME_CURTIME_SHIFT)

/*
 * Timer Core k Register
 */
#define TIMER_EN_SHIFT			(31U)
#define TIMER_COUNTER_SHIFT		(0U)

#define TIMER_EN_MASK			((u32)0x1U << TIMER_EN_SHIFT)
#define TIMER_COUNTER_MASK		((u32)0xFFFFFU << TIMER_COUNTER_SHIFT)

/*
 * Timer Interrupt k Register
 */
#define TIREQ_EN_SHIFT		(31U)
#define TIREQ_TIME_SHIFT	(0U)

#define TIREQ_EN_MASK		((u32)0x1U << TIREQ_EN_SHIFT)
#define TIREQ_TIME_MASK		((u32)0xFFFFU << TIREQ_TIME_SHIFT)

/*
 * Interrupt Mask Register
 */
#define IRQMASK_TIREQ1_SHIFT	(3U)
#define IRQMASK_TIREQ0_SHIFT	(2U)
#define IRQMASK_TIMER1_SHIFT	(1U)
#define IRQMASK_TIMER0_SHIFT	(0U)

#define IRQMASK_TIREQ1_MASK		((u32)0x1U << IRQMASK_TIREQ1_SHIFT)
#define IRQMASK_TIREQ0_MASK		((u32)0x1U << IRQMASK_TIREQ0_SHIFT)
#define IRQMASK_TIMER1_MASK		((u32)0x1U << IRQMASK_TIMER1_SHIFT)
#define IRQMASK_TIMER0_MASK		((u32)0x1U << IRQMASK_TIMER0_SHIFT)

/*
 * Interrupt Status Register
 */
#define IRQSTAT_TIREQ1_SHIFT	(3U)
#define IRQSTAT_TIREQ0_SHIFT	(2U)
#define IRQSTAT_TIMER1_SHIFT	(1U)
#define IRQSTAT_TIMER0_SHIFT	(0U)

#define IRQSTAT_TIREQ1_MASK		((u32)0x1U << IRQSTAT_TIREQ1_SHIFT)
#define IRQSTAT_TIREQ0_MASK		((u32)0x1U << IRQSTAT_TIREQ0_SHIFT)
#define IRQSTAT_TIMER1_MASK		((u32)0x1U << IRQSTAT_TIMER1_SHIFT)
#define IRQSTAT_TIMER0_MASK		((u32)0x1U << IRQSTAT_TIMER0_SHIFT)

#define VIOC_TIMER_IRQSTAT_MASK ( \
	IRQSTAT_TIREQ1_MASK |\
	IRQSTAT_TIREQ0_MASK |\
	IRQSTAT_TIMER1_MASK |\
	IRQSTAT_TIMER0_MASK)


enum vioc_timer_id {
	VIOC_TIMER_TIMER0 = 0,
	VIOC_TIMER_TIMER1,
	VIOC_TIMER_TIREQ0,
	VIOC_TIMER_TIREQ1
};

unsigned int vioc_timer_get_unit_us(void);
unsigned int vioc_timer_get_curtime(void);
int vioc_timer_set_timer(
	enum vioc_timer_id id,
	int enable,
	unsigned int timer_hz);
int vioc_timer_set_timer_req(
	enum vioc_timer_id id,
	int enable,
	unsigned int units);
int vioc_timer_set_irq_mask(enum vioc_timer_id id);
int vioc_timer_clear_irq_mask(enum vioc_timer_id id);
unsigned int vioc_timer_get_irq_status(void);
int vioc_timer_is_interrupted(enum vioc_timer_id id);
int vioc_timer_clear_irq_status(enum vioc_timer_id id);
int vioc_timer_suspend(void);
int vioc_timer_resume(void);

#endif

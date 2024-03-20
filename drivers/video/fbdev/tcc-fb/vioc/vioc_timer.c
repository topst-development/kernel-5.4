// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2020 Telechips Inc.
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
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <video/tcc/vioc_timer.h>
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP

struct vioc_timer_context {
	void __iomem *reg;
	struct clk *pclk;
	unsigned int unit_us;
	unsigned int suspend;
};
static struct vioc_timer_context ctx;

/* coverity[HIS_metric_violation : FALSE] */
static int vioc_timer_set_usec_enable(int enable, unsigned int unit_us)
{
	unsigned int xin_mhz;
	unsigned int reg_val;
	int ret;
	int temp = 0U; /* avoid MISRA C-2012 Rule 10.8 */

	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	temp = DIV_ROUND_UP(XIN_CLK_RATE, 1000000UL);
	xin_mhz = (unsigned int)temp;
	xin_mhz = (xin_mhz - 1U) << USEC_USECDIV_SHIFT;

	if (ctx.reg == NULL) {
		ret = -EINVAL;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	/* coverity[misra_c_2012_rule_11_2_violation : FALSE] */
	if (IS_ERR(ctx.pclk)) {
		ret = -ENODEV;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	if (unit_us == 0U) {
		ret = -EINVAL;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	unit_us = (unit_us - 1U) << USEC_UINTDIV_SHIFT;

	if (enable == 1) {
		(void)clk_set_rate(ctx.pclk, (unsigned long)XIN_CLK_RATE);
		(void)clk_prepare_enable(ctx.pclk);

		reg_val = ((u32)1U << USEC_EN_SHIFT) |
			(unit_us & USEC_UINTDIV_MASK) |
			(xin_mhz & USEC_USECDIV_MASK);
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg_val = __raw_readl(ctx.reg + CURTIME);
		reg_val &= ~USEC_EN_MASK;

		clk_disable_unprepare(ctx.pclk);
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(reg_val, ctx.reg + USEC);
	ret = 0;
out:
	return ret;
}

/* coverity[HIS_metric_violation : FALSE] */
static int vioc_timer_get_usec_enable(void)
{
	int enable = 0;
	unsigned int reg_val;

	if (ctx.reg == NULL) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	reg_val = __raw_readl(ctx.reg + CURTIME);
	if ((reg_val & USEC_EN_MASK) != 0U) {
		/* Prevent KCS warning */
		enable = 1;
	}
out:
	return enable;
}

/* coverity[HIS_metric_violation : FALSE] */
unsigned int vioc_timer_get_unit_us(void)
{
	unsigned int reg_val;
	unsigned int unit_us = 0;

	if (ctx.reg == NULL) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	/* coverity[misra_c_2012_rule_11_2_violation : FALSE] */
	if (IS_ERR(ctx.pclk)) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	reg_val = __raw_readl(ctx.reg + CURTIME);

	unit_us = 1U + ((reg_val & USEC_UINTDIV_MASK) >> USEC_UINTDIV_SHIFT);

out:
	return unit_us;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL_GPL(vioc_timer_get_unit_us);

/* coverity[HIS_metric_violation : FALSE] */
unsigned int vioc_timer_get_curtime(void)
{
	unsigned int curtime = 0;

	if (ctx.reg == NULL) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	curtime = __raw_readl(ctx.reg + CURTIME);
out:
	return curtime;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL_GPL(vioc_timer_get_curtime);

/* coverity[HIS_metric_violation : FALSE] */
int vioc_timer_set_timer(enum vioc_timer_id id, int enable, unsigned int timer_hz)
{
	int ret = -1;
	unsigned int timer_offset;
	unsigned int reg_val;

	if (ctx.reg == NULL) {
		ret = -ENODEV;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	switch (id) {
	case VIOC_TIMER_TIMER0:
		timer_offset = TIMER0;
		break;
	case VIOC_TIMER_TIMER1:
		timer_offset = TIMER1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if(ret == -EINVAL) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	if (enable == 1) {
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
		/* coverity[cert_int30_c_violation : FALSE] */
		reg_val =
			(DIV_ROUND_UP(XIN_CLK_RATE, timer_hz) - 1) &
			TIMER_COUNTER_MASK;
		reg_val |= ((u32)1U << TIMER_EN_SHIFT);
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg_val = __raw_readl(ctx.reg + timer_offset);
		reg_val &= ~((u32)1U << TIMER_EN_SHIFT);
	}
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(reg_val, ctx.reg + timer_offset);
	ret = 0;
out:
	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL_GPL(vioc_timer_set_timer);

/* coverity[HIS_metric_violation : FALSE] */
int vioc_timer_set_timer_req(
	enum vioc_timer_id id, int enable, unsigned int units)
{
	unsigned int tireq_offset;
	unsigned int reg_val;
	int ret = -1;

	if (ctx.reg == NULL) {
		ret = -ENODEV;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	switch (id) {
	case VIOC_TIMER_TIREQ0:
		tireq_offset = TIREQ0;
		break;
	case VIOC_TIMER_TIREQ1:
		tireq_offset = TIREQ1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if(ret == -EINVAL) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	if (enable == 1) {
		reg_val = units & TIREQ_TIME_MASK;
		reg_val |= ((u32)1U << TIREQ_EN_SHIFT);
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg_val = __raw_readl(ctx.reg + tireq_offset);
		reg_val &= ~((u32)1U << TIREQ_EN_SHIFT);
	}
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(reg_val, ctx.reg + tireq_offset);
	ret = 0;
out:
	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL_GPL(vioc_timer_set_timer_req);

/* coverity[HIS_metric_violation : FALSE] */
int vioc_timer_set_irq_mask(enum vioc_timer_id id)
{
	unsigned int reg_val;
	int ret = -1;

	if (ctx.reg == NULL) {
		ret = -ENODEV;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	if (id > VIOC_TIMER_TIREQ1) {
		ret = -EINVAL;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	reg_val = __raw_readl(ctx.reg + IRQMASK);
	reg_val |= ((u32)1U << (unsigned int)id);

	__raw_writel(reg_val, ctx.reg + IRQMASK);
	ret = 0;
out:
	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL_GPL(vioc_timer_set_irq_mask);

/* coverity[HIS_metric_violation : FALSE] */
int vioc_timer_clear_irq_mask(enum vioc_timer_id id)
{
	unsigned int reg_val;
	int ret = -1;

	if (ctx.reg == NULL) {
		ret = -ENODEV;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	if (id > VIOC_TIMER_TIREQ1) {
		ret = -EINVAL;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	reg_val = __raw_readl(ctx.reg + IRQMASK);
	reg_val &= ~((u32)1U << (unsigned int)id);

	__raw_writel(reg_val, ctx.reg + IRQMASK);
	ret = 0;
out:
	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL_GPL(vioc_timer_clear_irq_mask);

/* coverity[HIS_metric_violation : FALSE] */
unsigned int vioc_timer_get_irq_status(void)
{
	unsigned int reg_val;
	unsigned int ret = 0U;

	if (ctx.reg == NULL) {
		ret = 0U;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
		/* prevent KCS warning */
	}
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	reg_val = __raw_readl(ctx.reg + IRQSTAT);
	ret = reg_val;
out:
	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL_GPL(vioc_timer_get_irq_status);

/* coverity[HIS_metric_violation : FALSE] */
int vioc_timer_is_interrupted(enum vioc_timer_id id)
{
	unsigned int reg_val;
	int ret = 0;

	if (ctx.reg == NULL) {
		ret = 0;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	if (id > VIOC_TIMER_TIREQ1) {
		ret = 0;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	reg_val = __raw_readl(ctx.reg + IRQSTAT);
	ret = (((reg_val & ((u32)1U << (unsigned int)id)) != 0U) ? 1 : 0);
out:
	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL_GPL(vioc_timer_is_interrupted);

/* coverity[HIS_metric_violation : FALSE] */
int vioc_timer_clear_irq_status(enum vioc_timer_id id)
{
	int ret = -1;

	if (ctx.reg == NULL) {
		ret = -ENODEV;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	if (id > VIOC_TIMER_TIREQ1) {
		ret = -EINVAL;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel((u32)1U << (unsigned int)id, ctx.reg + IRQSTAT);

	ret = 0;
out:
	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL_GPL(vioc_timer_clear_irq_status);

int vioc_timer_suspend(void)
{
	if (ctx.suspend == 0U) {
		ctx.suspend = 1U;
		(void)vioc_timer_set_usec_enable(0, 0);
	}
	return 0;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL_GPL(vioc_timer_suspend);

int vioc_timer_resume(void)
{
	if (ctx.suspend == 1U) {
		ctx.suspend = 0U;
		(void)vioc_timer_set_usec_enable(1, 100);
	}
	return 0;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL_GPL(vioc_timer_resume);

static int __init vioc_timer_init(void)
{
	struct device_node *np;

	(void)memset(&ctx, 0, sizeof(ctx));
	np = of_find_compatible_node(NULL, NULL, "telechips,vioc_timer");

	if (np == NULL) {
		(void)pr_info("[INFO][TIMER] disabled\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}
	ctx.reg =
		(void __iomem *)of_iomap(np, (is_VIOC_REMAP != 0U) ? 1 : 0);
	if (ctx.reg != NULL) {
		/* Prevent KCS warning */
		(void)pr_info(
			"[INFO][TIMER] vioc-timer\n");
	}

	ctx.pclk  = of_clk_get_by_name(np, "timer-clk");
	/* coverity[misra_c_2012_rule_11_2_violation : FALSE] */
	if (IS_ERR(ctx.pclk)) {
		/* Prevent KCS warning */
		(void)pr_err("[ERROR][TIMER] Please check timer-clk on DT\r\n");
	}

	/* Set interrupt mask for all interrupt source */
	(void)vioc_timer_set_irq_mask(VIOC_TIMER_TIMER0);
	(void)vioc_timer_set_irq_mask(VIOC_TIMER_TIMER1);
	(void)vioc_timer_set_irq_mask(VIOC_TIMER_TIREQ0);
	(void)vioc_timer_set_irq_mask(VIOC_TIMER_TIREQ1);

	if (vioc_timer_get_usec_enable() == 0) {
		(void)vioc_timer_set_usec_enable(1, 100U);
	} else {
		unsigned int unit_us = vioc_timer_get_unit_us();

		if (unit_us != 100U) {
			/* disable vioc timer */
			(void)vioc_timer_set_usec_enable(0, 0);

			/* reset vioc timer */
			(void)vioc_timer_set_usec_enable(1, 100U);
		}
	}
out:
	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_timer_init);
/* EOF */

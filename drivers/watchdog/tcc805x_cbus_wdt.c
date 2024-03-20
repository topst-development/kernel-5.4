// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) Telechips Inc.
 */

#include <linux/bitops.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/watchdog.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/arm-smccc.h>
#include <linux/kernel.h>


#define wdt_readl	readl
#define wdt_writel	writel

/* Kick Timer */
#define KICK_TIMER_ID_MAX	2U
#define TCKSEL_MAX		7U
extern unsigned int tcksel_factor[TCKSEL_MAX];
unsigned int tcksel_factor[TCKSEL_MAX] = {
	2, 4, 8, 16, 32, 1024, 4096
};

enum {
	TCC_TCFG,
	TCC_TCNT,
	TCC_TREF,
	TCC_TMREF
};
/* CBUS Watchdog */
enum{
	CBUS_WDT_EN,		/* WDT_EN */
	CBUS_WDT_CLEAR,		/* WDT_CLEAR */
	CBUS_WDT_IRQCNT,	/* WDT_CVALUE */
	CBUS_WDT_RSTCNT		/* WDT_RST_CVALUE */
};

struct tcc_cb_wdt {
	struct watchdog_device wdd;
	struct clk *wdt_clk;
	struct clk *timer_clk;

	unsigned int __iomem *base;
	unsigned int __iomem *timer_base;
	unsigned int __iomem *timer_irq_base;

	unsigned int kick_irq;

	unsigned int clk_rate;
	spinlock_t lock;
	unsigned char timer_id; // SMU only have 5 timers
	unsigned char timer_tcksel;
};

static int tcc_cb_wdt_start(struct watchdog_device *wdd);
static int tcc_cb_wdt_stop(struct watchdog_device *wdd);
static int tcc_cb_wdt_ping(struct watchdog_device *wdd);
static unsigned int tcc_cb_wdt_get_status(struct watchdog_device *wdd);


static void tcc_cb_wdt_enable_kick_timer(const struct tcc_cb_wdt* cb_wdt)
{
	unsigned int val;
	unsigned int tcksel = cb_wdt->timer_tcksel;

	wdt_writel(0x0, &cb_wdt->timer_base[TCC_TCNT]);
	/* [DR]
	 * Kernel API readl has defects it's inside.
	 */
	val = wdt_readl(&cb_wdt->timer_base[TCC_TCFG]);
	// Enable timer & timer interrupt
	// Timer mode : not stop & not continuos
	val |= ((tcksel << 4U) | (1U << 3U) | (1U << 0U));

	wdt_writel(val, &cb_wdt->timer_base[TCC_TCFG]);
}

static void tcc_cb_wdt_disable_kick_timer(const struct tcc_cb_wdt* cb_wdt)
{
	unsigned int val;

	/* [DR]
	 * Kernel API readl has defects it's inside.
	 */
	val = wdt_readl(&cb_wdt->timer_base[TCC_TCFG]);
	// Disable timer & timer interrupt
	val &= ~((1U << 3U) | (1U << 0U));
	wdt_writel(val, &cb_wdt->timer_base[TCC_TCFG]);
	wdt_writel(0x0U, &cb_wdt->timer_base[TCC_TCNT]);
}

static void tcc_cb_wdt_do_kick(const struct device *dev,
					struct tcc_cb_wdt *cb_wdt)
{

	/* [DR]
	 * Kernel API dev_dbg has defects
	 * it's inside.
	 */
	dev_dbg(dev, "%s\n", __func__);
	if (cb_wdt->timer_id > 5U) {
		BUG();
	} else {
		wdt_writel(((unsigned int)1U << (cb_wdt->timer_id+8U))
			   | ((unsigned int)1U << cb_wdt->timer_id),
			   cb_wdt->timer_irq_base);
		(void)tcc_cb_wdt_ping(&cb_wdt->wdd);
	}
}

/* [DR]
 * tcc_cb_wdt_kick is call back of kick timer irq
 * irq handler function is declare as
 * ‘irqreturn_t (*irq_handler_t)(int, void *)’
 */
static irqreturn_t tcc_cb_wdt_kick(int irq, void* dev_id)
{
	/* [DR]
	 *
	 */
	const struct device* dev = (struct device *)dev_id;
	/* [DR]
	 * Function family of get_drvdata usually need to cast void*
	 * to others.
	 */
	struct tcc_cb_wdt *cb_wdt = (dev == NULL) ? NULL :
		(struct tcc_cb_wdt *)dev_get_drvdata(dev);
	irqreturn_t irq_ret = IRQ_HANDLED;

	(void)irq;

	if (cb_wdt != NULL) {
		/* [DR]
		 * Kernel API readl has defects it's inside.
		 */
		if ((wdt_readl(cb_wdt->timer_irq_base) &
			((unsigned int)1U << (cb_wdt->timer_id&0x7U))) == 0U) {
			irq_ret = IRQ_NONE;
		} else {
			tcc_cb_wdt_do_kick(dev, cb_wdt);
		}
	} else {
		irq_ret = IRQ_NONE;
	}

	return irq_ret;
}

static int tcc_cb_wdt_set_pretimeout(struct watchdog_device *wdd,
					unsigned int pretimeout)
{
	unsigned char tcksel;
	unsigned int max_pretimeout, timer_tick;
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	struct tcc_cb_wdt *cb_wdt = watchdog_get_drvdata(wdd);
	unsigned long clk_rate = clk_get_rate(cb_wdt->timer_clk);
	int ret = 0;

	wdt_writel(0x0U, &cb_wdt->timer_base[TCC_TCNT]);
	wdt_writel(0x0U, &cb_wdt->timer_base[TCC_TMREF]);

	for (tcksel = 0; tcksel < TCKSEL_MAX; tcksel++) {
		timer_tick = (unsigned int)((clk_rate > UINT_MAX) ?
				UINT_MAX : clk_rate);
		timer_tick /= tcksel_factor[tcksel];
		max_pretimeout = 0xFFFFU / timer_tick;
		if (max_pretimeout >= pretimeout) {
			break;
		}
	}

	if (tcksel < TCKSEL_MAX) {
		/*  [DR]
		 * tcksel << 4 will not wrap. tcksel always in range of 0 ~ 7
		 */
		wdt_writel((tcksel<<4U),
				&cb_wdt->timer_base[TCC_TCFG]);
		/*  [DR]
		 *  pretimeout * timer_tick will not wrap.
		 *  max_pretieout * timer_tick <= 0xFFFF
		 *  pretimeout < max_pretimout
		 */
		wdt_writel((pretimeout * timer_tick),
				&cb_wdt->timer_base[TCC_TREF]);

		cb_wdt->timer_tcksel = tcksel;
		wdd->pretimeout = pretimeout;
	} else {
		ret = -EINVAL;
		dev_err(wdd->parent,
			"[%s] pretimeout value is too long. (max: %u)\n",
			__func__, max_pretimeout);
	}

	return ret;
}

static int tcc_cb_wdt_set_timeout(struct watchdog_device *wdd,
					 unsigned int timeout)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_cb_wdt* cb_wdt = watchdog_get_drvdata(wdd);
	int ret = 0;
	unsigned int reset_cnt = 0;
	bool is_active = watchdog_active(wdd);

	if (is_active) {
		(void)tcc_cb_wdt_stop(wdd);
	}

	if ((timeout >= wdd->min_timeout) && (timeout <= wdd->max_timeout)) {

		reset_cnt = ((UINT_MAX / timeout) > cb_wdt->clk_rate) ?
			(timeout * cb_wdt->clk_rate) :
			((UINT_MAX / cb_wdt->clk_rate) * cb_wdt->clk_rate);

		// Set WDT Reset IRQ CNT
		wdt_writel(reset_cnt, &cb_wdt->base[CBUS_WDT_IRQCNT]);

		wdd->timeout = timeout;
		if (wdd->pretimeout >= wdd->timeout) {
			// minimum timeout : 5 second
			ret = tcc_cb_wdt_set_pretimeout(wdd, 1U);
			dev_err(wdd->parent, "[%s] %u is smaller than pretimeout. pretimeout will change 1 second\n",
					__func__, timeout);
		}

	} else {
		ret = -EINVAL;
	}

	if (is_active) {
		(void)tcc_cb_wdt_start(wdd);
	}

	return ret;
}

static int tcc_cb_wdt_start(struct watchdog_device *wdd)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	struct tcc_cb_wdt *cb_wdt = watchdog_get_drvdata(wdd);
	unsigned long flags;
	int ret = 0;

	/* [DR]
	 * Kernel API spin_lock_irqsave has defects
	 * it's inside.
	 */
	spin_lock_irqsave((&cb_wdt->lock), (flags));

	tcc_cb_wdt_enable_kick_timer(cb_wdt);
	wdt_writel(0x1, &cb_wdt->base[CBUS_WDT_EN]);

	set_bit(WDOG_ACTIVE, &wdd->status);
	set_bit(WDOG_HW_RUNNING, &wdd->status);

	spin_unlock_irqrestore(&cb_wdt->lock, flags);

	return ret;
}

static int tcc_cb_wdt_stop(struct watchdog_device *wdd)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	struct tcc_cb_wdt *cb_wdt = watchdog_get_drvdata(wdd);
	unsigned long flags;
	int ret = 0;

	/* [DR]
	 * Kernel API spin_lock_irqsave has defects
	 * it's inside.
	 */
	spin_lock_irqsave((&cb_wdt->lock), (flags));

	wdt_writel(0x0, &cb_wdt->base[CBUS_WDT_EN]);

	tcc_cb_wdt_disable_kick_timer(cb_wdt);
	clear_bit(WDOG_ACTIVE, &wdd->status);
	clear_bit(WDOG_HW_RUNNING, &wdd->status);

	spin_unlock_irqrestore(&cb_wdt->lock, flags);

	return ret;
}

static int tcc_cb_wdt_ping(struct watchdog_device *wdd)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	struct tcc_cb_wdt* const cb_wdt = watchdog_get_drvdata(wdd);

	wdt_writel(1U, &cb_wdt->base[CBUS_WDT_CLEAR]);

	return 0;
}

/* [DR]
 * tcc_cb_Wdt_get_status is call back of tcc_cb_wdt_ops.status
 * Watchdog core driver declare status function as
 * 'unsigned int (*)(struct watchdog_device *)'
 */
static unsigned int tcc_cb_wdt_get_status(struct watchdog_device* wdd)
{
	/* [DR]
	 * Type mismatch in watchdog core driver.
	 * wdd->status only use 5 bits now.
	 * No need to worry about data loss.
	 */
	return wdd->status;
}

static const struct watchdog_ops tcc_cb_wdt_ops = {
	.owner			= THIS_MODULE,
	.start			= tcc_cb_wdt_start,
	.stop			= tcc_cb_wdt_stop,
	.ping			= tcc_cb_wdt_ping,
	.set_timeout		= tcc_cb_wdt_set_timeout,
	.set_pretimeout		= tcc_cb_wdt_set_pretimeout,
	.status			= tcc_cb_wdt_get_status,
};

static const struct watchdog_info tcc_cb_wdt_info = {
	.options = (int)WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
	.firmware_version = 0,
	.identity = "TCC805X CBUS Watchdog",
};

static int tcc_cb_wdt_ioremap(struct tcc_cb_wdt *cb_wdt,
		struct platform_device *pdev)
{
	const struct resource *res;
	int ret = 0;
	unsigned int i;
	unsigned int** const base_addr[3] = {
		&(cb_wdt->base),
		&(cb_wdt->timer_base),
		&(cb_wdt->timer_irq_base)
	};

	/* [DR]
	 * Kernel API ARRAY_SIZE has defects it's inside.
	 */
	for (i = 0; i < ARRAY_SIZE(base_addr); i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res == NULL) {
			dev_err(&pdev->dev,
				"[%s] failed to get register address. register index: %u\n",
				__func__, i);
			ret = -ENOMEM;
		}

		if (ret == 0) {
			/* [DR]
			 *
			 */
			*base_addr[i] = devm_ioremap_resource(&pdev->dev, res);
			if (IS_ERR(cb_wdt->base) != (bool)0) {
				dev_err(&pdev->dev,
					"[%s] register address ioremap failed. reg index: %u  err_ptr: %p\n",
					__func__, i, *base_addr[i]);
			}
		}
	}

	return ret;
}

static int tcc_cb_wdt_set_clock_rate(const struct tcc_cb_wdt* cb_wdt)
{
	int ret;

	// PERI_CPU_WATCHDOG already disabled at this point. Just set rate.
	ret = clk_set_rate(cb_wdt->wdt_clk, cb_wdt->clk_rate);
	if (ret == 0) {
		ret = clk_prepare_enable(cb_wdt->wdt_clk);
		dev_info(cb_wdt->wdd.parent, "[%s] watchdog clock rate: %lu\n",
				__func__, clk_get_rate(cb_wdt->wdt_clk));
	}

	return ret;
}

static int tcc_cb_wdt_init_clock(struct tcc_cb_wdt *cb_wdt,
		struct platform_device *pdev)
{
	int ret = 0;
	const struct device_node* np = pdev->dev.of_node;

	cb_wdt->wdt_clk = devm_clk_get(&pdev->dev, "clk_wdt");
	/* [DR]
	 * Kernel API IS_ERR has defects
	 * it's inside.
	 */
	if (IS_ERR(cb_wdt->wdt_clk) != (bool)0) {
		ret = -ENODEV;
	}

	if (ret == 0) {
		if (of_property_read_u32(np, "clock-frequency",
					 &cb_wdt->clk_rate) != 0){
			/* default : 24MHz */
			cb_wdt->clk_rate = 24000000;
		}
	}

	if (ret == 0) {
		ret = tcc_cb_wdt_set_clock_rate(cb_wdt);
		if (ret != 0) {
			dev_err(&pdev->dev,
				"[%s] Fail to set CPU Watchdog clock rate : %u err_code: %d\n",
				__func__, cb_wdt->clk_rate, ret);
		}
	}

	cb_wdt->timer_clk = devm_clk_get(&pdev->dev, "clk_kick");
	/* [DR]
	 * Kernel API IS_ERR has defects it's inside.
	 */
	if (IS_ERR(cb_wdt->timer_clk) != (bool)0) {
		ret = -ENODEV;
	}

	return ret;
}

static void tcc_cb_wdt_read_of_property_kick_timer(struct tcc_cb_wdt *cb_wdt,
		const struct platform_device* pdev)
{
	const struct device_node* np = pdev->dev.of_node;
	struct watchdog_device *wdd = &cb_wdt->wdd;

	if (of_property_read_u32(np, "pretimeout-sec", &wdd->pretimeout) != 0) {
		wdd->pretimeout = 5U;
		dev_err(&pdev->dev,
			"[%s] Cannot find pretimeout-sec in device tree. Set as default (%u sec)\n",
			__func__, wdd->pretimeout);
	}

	if (of_property_read_u8(np, "kick-timer-id",
				&cb_wdt->timer_id) != 0) {
		dev_err(&pdev->dev,
			"[%s] Cannot find kick-timer-id in device tree.\n",
			__func__);
		BUG();
	}
}

static int tcc_cb_wdt_init_kick_timer(struct tcc_cb_wdt *cb_wdt,
		struct platform_device *pdev)
{
	int ret = 0;

	ret = platform_get_irq(pdev, 0);
	if (ret >= 0) {
		cb_wdt->kick_irq = (unsigned int)ret;
		ret = 0;
	}

	tcc_cb_wdt_read_of_property_kick_timer(cb_wdt, pdev);

	if (ret == 0) {
		ret = request_irq(cb_wdt->kick_irq, tcc_cb_wdt_kick,
				(IRQF_SHARED | IRQF_TIMER),
				"tcc_cb_wdt_kick", &pdev->dev);
		if (ret != 0) {
			dev_err(&pdev->dev, "[%s] failed to request kick_irq\n",
					__func__);
		} else {
			ret = tcc_cb_wdt_set_pretimeout(&cb_wdt->wdd,
				cb_wdt->wdd.pretimeout);
		}
	}

	return ret;
}

static int tcc_cb_wdt_init_wdt_timer(struct tcc_cb_wdt *cb_wdt,
		struct platform_device *pdev)
{
	int ret = 0;

	cb_wdt->wdd.min_timeout = 5;
	cb_wdt->wdd.max_timeout = UINT_MAX / cb_wdt->clk_rate; // 178 sec (24MHz)
	cb_wdt->wdd.timeout = 20U; // Set default value
	// If timeout-sec is exist in device tree,
	// wdd.timeout will be overwrite by watchdog_init_timeout().
	ret = watchdog_init_timeout(&cb_wdt->wdd, 0, &pdev->dev);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"[%s] Device tree has invalid timeout value. (min: %u max: %u)\n",
			__func__, cb_wdt->wdd.min_timeout,
			cb_wdt->wdd.max_timeout);
	}

	ret = tcc_cb_wdt_set_timeout(&cb_wdt->wdd, cb_wdt->wdd.timeout);
	dev_info(&pdev->dev,
		"[%s] Watchdog timeout: %u, pretimeout: %u\n",
		__func__, cb_wdt->wdd.timeout, cb_wdt->wdd.pretimeout);

	return ret;
}

static int tcc_cb_wdt_init_timer(struct tcc_cb_wdt* cb_wdt,
		struct platform_device *pdev)
{
	int ret = 0;

	cb_wdt->wdd.info = &tcc_cb_wdt_info;
	cb_wdt->wdd.ops = &tcc_cb_wdt_ops;
	cb_wdt->wdd.parent = &pdev->dev;

	ret  = tcc_cb_wdt_init_kick_timer(cb_wdt, pdev);

	if (ret == 0) {
		ret = tcc_cb_wdt_init_wdt_timer(cb_wdt, pdev);
	}

	return ret;
}

static int tcc_cb_wdt_init(struct tcc_cb_wdt *cb_wdt,
		struct platform_device *pdev)
{
	int ret;

	ret = tcc_cb_wdt_ioremap(cb_wdt, pdev);

	if (ret == 0) {
		ret = tcc_cb_wdt_init_clock(cb_wdt, pdev);
	}

	if (ret == 0) {
		ret = tcc_cb_wdt_init_timer(cb_wdt, pdev);
	}

	/* [DR]
	 * Kernel API spin_lock_init has defects it's inside.
	 */
	spin_lock_init(&cb_wdt->lock);

	return ret;
}

static int tcc_cb_wdt_register(struct tcc_cb_wdt *cb_wdt,
		struct platform_device *pdev)
{
	int ret;
	struct watchdog_device *wdd;

	wdd = &cb_wdt->wdd;

	ret = devm_watchdog_register_device(&pdev->dev, wdd);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"[%s] failed to register wdt device\n", __func__);
	}

	watchdog_stop_on_reboot(wdd);
	watchdog_stop_on_unregister(wdd);


	return ret;
}

static int tcc_cb_wdt_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tcc_cb_wdt *cb_wdt;
	/* [DR]
	 * 1. Function family of kmalloc usually need to cast void* to others.
	 * 2. sizeof has defects it's inside
	 */
	cb_wdt = devm_kzalloc(&pdev->dev,
			sizeof(struct tcc_cb_wdt), GFP_KERNEL);
	if (cb_wdt == NULL) {
		ret = -ENOMEM;
	} else {
		platform_set_drvdata(pdev, cb_wdt);
		watchdog_set_drvdata(&cb_wdt->wdd, cb_wdt);
	}

	if (ret == 0) {
		ret = tcc_cb_wdt_init(cb_wdt, pdev);
	}

	if (ret == 0) {
		ret = tcc_cb_wdt_register(cb_wdt, pdev);
	}

	if (ret == 0) {
		(void)tcc_cb_wdt_ping(&cb_wdt->wdd);
		ret = tcc_cb_wdt_start(&cb_wdt->wdd);
	}

	return ret;
}

/* [DR]
 * tcc_cb_wdt_remove is call back of tcc_cb_wdt_driver.remove
 * platform driver declare remove function as
 * ‘int (*)(struct platform_device *)’
 */
static int tcc_cb_wdt_remove(struct platform_device* pdev)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	struct tcc_cb_wdt *cb_wdt = platform_get_drvdata(pdev);
	int ret = 0;

	if (cb_wdt == NULL) {
		dev_err(&pdev->dev, "[%s] cb_wdt is null", __func__);
		ret = -ENODEV;
	} else {
		if (watchdog_active(&cb_wdt->wdd)) {
			(void)tcc_cb_wdt_stop(&cb_wdt->wdd);
		}
	}

	return ret;
}

/* [DR]
 * tcc_cb_wdt_shutdown is call back of tcc_cb_wdt_driver.shutdown
 * platform driver declare shutdown function as
 * ‘void (*)(struct platform_device *)’
 */
static void tcc_cb_wdt_shutdown(struct platform_device* pdev)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	struct tcc_cb_wdt *cb_wdt = platform_get_drvdata(pdev);

	if (cb_wdt == NULL) {
		dev_err(&pdev->dev, "[%s] cb_wdt is null", __func__);
	} else {
		if (watchdog_active(&cb_wdt->wdd)) {
			(void)tcc_cb_wdt_stop(&cb_wdt->wdd);
		}
	}
}

#ifdef CONFIG_PM
/* [DR]
 * tcc_cb_wdt_suspend is call back of tcc_cb_wdt_driver.suspend
 * platform driver declare suspend function as
 * ‘int (*)(struct platform_device *, pm_message_t)’
 */
/* Do we still need none pm suspend / resume function? */
static int tcc_cb_wdt_suspend(struct platform_device* pdev,
				pm_message_t state)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	struct tcc_cb_wdt *cb_wdt = platform_get_drvdata(pdev);
	int ret = 0;

	(void)state;

	if (cb_wdt == NULL) {
		dev_err(&pdev->dev, "[%s] cb_wdt is null", __func__);
		ret = -ENODEV;
	} else {
		if (watchdog_active(&cb_wdt->wdd)) {
			ret = tcc_cb_wdt_stop(&cb_wdt->wdd);
		}
	}

	return ret;
}

/* [DR]
 * tcc_cb_wdt_resume is call back of tcc_cb_wdt_driver.resume
 * platform driver declare resume function as
 * ‘int (*)(struct platform_device *)’
 */
static int tcc_cb_wdt_resume(struct platform_device* pdev)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	struct tcc_cb_wdt *cb_wdt = platform_get_drvdata(pdev);
	int ret = 0;

	if (cb_wdt == NULL) {
		dev_err(&pdev->dev, "[%s] cb_wdt is null", __func__);
		ret = -ENODEV;
	} else {
		if (!watchdog_active(&cb_wdt->wdd)) {
			ret = tcc_cb_wdt_set_pretimeout(&cb_wdt->wdd,
					cb_wdt->wdd.pretimeout);
			if (ret == 0) {
				ret = tcc_cb_wdt_set_timeout(&cb_wdt->wdd,
						cb_wdt->wdd.timeout);
			}
			if (ret == 0) {
				(void)tcc_cb_wdt_start(&cb_wdt->wdd);
			} else {
				dev_err(&pdev->dev, "[%s] Fail to resume watchdog timeout. Watchdog disabled.",
					__func__);
			}
		}
	}

	return ret;
}

/* [DR]
 * tcc_cb_wdt_pm_suspend is call back of tcc_cb_wdt_pm_ops.suspend
 * pm driver declare suspend function as
 * ‘int (*)(struct device *)’
 */
static int tcc_cb_wdt_pm_suspend(struct device* dev)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	struct tcc_cb_wdt *cb_wdt = dev_get_drvdata(dev);
	int ret = 0;

	if (cb_wdt == NULL) {
		ret = -ENODEV;
	} else {
		if (watchdog_active(&cb_wdt->wdd)) {
			ret = tcc_cb_wdt_stop(&cb_wdt->wdd);
		}
	}

	return ret;
}

/* [DR]
 * tcc_cb_wdt_pm_resume is call back of tcc_cb_wdt_pm_ops.resume
 * pm driver declare resume function as
 * ‘int (*)(struct platform_device *, pm_message_t)’
 */
static int tcc_cb_wdt_pm_resume(struct device* dev)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	struct tcc_cb_wdt *cb_wdt = dev_get_drvdata(dev);
	int ret = 0;

	if (cb_wdt == NULL) {
		dev_err(dev, "[%s] cb_wdt is null", __func__);
		ret = -ENODEV;
	} else {
		if (!watchdog_active(&cb_wdt->wdd)) {
			ret = tcc_cb_wdt_set_pretimeout(&cb_wdt->wdd,
					cb_wdt->wdd.pretimeout);
			if (ret == 0) {
				ret = tcc_cb_wdt_set_timeout(&cb_wdt->wdd,
						cb_wdt->wdd.timeout);
			}
			if (ret == 0) {
				(void)tcc_cb_wdt_start(&cb_wdt->wdd);
			} else {
				dev_err(dev, "[%s] Fail to resume watchdog timeout. Watchdog disabled.",
					__func__);
			}
		}
	}

	return ret;
}
#else
#define tcc_cb_wdt_suspend		NULL
#define tcc_cb_wdd_resume		NULL
#define tcc_cb_wdt_pm_suspend	NULL
#define tcc_cb_wdt_pm_resume	NULL
#endif

static const struct of_device_id tcc_cb_wdt_of_match[] = {
	{.compatible = "telechips,tcc-cb-wdt",},
	{ },
};

MODULE_DEVICE_TABLE(of, tcc_cb_wdt_of_match);

static const struct dev_pm_ops tcc_cb_wdt_pm_ops = {
	.suspend	= tcc_cb_wdt_pm_suspend,
	.resume		= tcc_cb_wdt_pm_resume,
};

static struct platform_driver tcc_cb_wdt_driver = {
	.probe		= tcc_cb_wdt_probe,
	.remove		= tcc_cb_wdt_remove,
	.shutdown	= tcc_cb_wdt_shutdown,
	.suspend	= tcc_cb_wdt_suspend,
	.resume		= tcc_cb_wdt_resume,
	.driver		= {
		.owner  = THIS_MODULE,
		.name   = "tcc-cb-wdt",
		.pm     = &tcc_cb_wdt_pm_ops,
		.of_match_table = tcc_cb_wdt_of_match,
	},
};

static int __init tcc_cb_wdt_init_module(void)
{
	int ret = 0;

	ret = platform_driver_register(&tcc_cb_wdt_driver);

	return ret;
}

static void __exit tcc_cb_wdt_exit_module(void)
{
	platform_driver_unregister(&tcc_cb_wdt_driver);
}

/* [DR]
 * Kernel API module_init & module_exit has defects it's inside.
 */
module_init(tcc_cb_wdt_init_module);
module_exit(tcc_cb_wdt_exit_module);

/* [DR]
 * Kernel API MODULE_xxxx  has defects it's inside.
 */
MODULE_AUTHOR("Telechips Corporation");
MODULE_DESCRIPTION("Telechips CBUS Watchdog Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:tcc-cbus-wdt");

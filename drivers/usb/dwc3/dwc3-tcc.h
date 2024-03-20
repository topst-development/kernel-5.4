/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __LINUX_DWC3_TCC_H
#define __LINUX_DWC3_TCC_H

#include <linux/usb/tcc.h>

struct pcfg_unit U30_FPCFG1[PCFG_MAX] = {
	/* name, offset, mask */
	{"TXVRT  ",  0, (0xF << 0)},
	{"CDT    ",  4, (0x7 << 4)},
	{"TXPPT  ",  7, (0x1 << 7)},
	{"TP     ",  8, (0x3 << 8)},
	{"TXRT   ", 10, (0x3 << 10)},
	{"TXREST ", 12, (0x3 << 12)},
	{"TXHSXVT", 14, (0x3 << 14)},
};

struct dwc3_tcc_hcd {
	struct clk		*hclk;
	struct device		*dev;
	struct platform_device	*usb3_phy;
	struct usb_phy		*dwc3_phy;

	int32_t			vbus_ctrl_able;
	int32_t			vbus_pm_status;
};

static void dwc3_tcc_vbus_ctrl(struct dwc3_tcc_hcd *dwc3_tcc, int32_t on_off);
static ssize_t vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t vbus_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static char *dwc3_pcfg_display(uint32_t old_reg, uint32_t new_reg, char *str);
static ssize_t dwc3_pcfg_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t dwc3_pcfg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static int32_t dwc3_tcc_remove_child(struct device *dev, void *unused);
static int32_t dwc3_tcc_phy_register(struct dwc3_tcc_hcd *dwc3_tcc);
static void dwc3_tcc_phy_unregister(struct dwc3_tcc_hcd *dwc3_tcc);

static int32_t dwc3_tcc_probe(struct platform_device *pdev);
static int32_t dwc3_tcc_remove(struct platform_device *pdev);

#if defined(CONFIG_PM_SLEEP)
static int32_t dwc3_tcc_suspend(struct device *dev);
static int32_t dwc3_tcc_resume(struct device *dev);
#endif /* CONFIG_PM_SLEEP */
#endif /* __LINUX_DWC3_TCC_H */

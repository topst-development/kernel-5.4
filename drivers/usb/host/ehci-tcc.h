/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __LINUX_EHCI_TCC_H
#define __LINUX_EHCI_TCC_H

#include <linux/usb/tcc.h>

#define DRIVER_DESC "EHCI Telechips driver"

static struct hc_driver __read_mostly tcc_ehci_hc_driver;

struct pcfg_unit USB_PCFG[PCFG_MAX] = {
	/* name, offset, mask */
	{"TXVRT  ",  0, (0xF << 0)},
	{"CDT    ",  4, (0x7 << 4)},
	{"TXPPT  ",  7, (0x1 << 7)},
	{"TP     ",  8, (0x3 << 8)},
	{"TXRT   ", 10, (0x3 << 10)},
	{"TXREST ", 12, (0x3 << 12)},
	{"TXHSXVT", 14, (0x3 << 14)},
};

struct tcc_ehci_hcd {
	struct device	*dev;
	struct ehci_hcd	*ehci;
	struct usb_phy	*phy;

	void __iomem	*phy_regs;

	int32_t		vbus_status;
	uint32_t	hcd_tpl_support;
};

int32_t ehci_phy_set = -1;
EXPORT_SYMBOL_GPL(ehci_phy_set);

static ssize_t vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t vbus_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t testmode_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t testmode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static char *tcc_ehci_display_pcfg(uint32_t old_reg, uint32_t new_reg,
		char *str);
static ssize_t ehci_pcfg_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t ehci_pcfg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static int32_t tcc_ehci_parse_dt(struct platform_device *pdev,
		struct tcc_ehci_hcd *tcc_ehci);
static int32_t tcc_ehci_ctrl_phy(struct tcc_ehci_hcd *tcc_ehci, int32_t on_off);
static int32_t tcc_ehci_init_phy(struct tcc_ehci_hcd *tcc_ehci);
static int32_t tcc_ehci_ctrl_vbus(struct tcc_ehci_hcd *tcc_ehci,
		int32_t on_off);

static int32_t tcc_ehci_probe(struct platform_device *pdev);
static int32_t tcc_ehci_remove(struct platform_device *pdev);
static void tcc_ehci_hcd_shutdown(struct platform_device *pdev);

static int32_t tcc_ehci_suspend(struct device *dev);
static int32_t tcc_ehci_resume(struct device *dev);

static int32_t __init tcc_ehci_init(void);
static void __exit tcc_ehci_cleanup(void);

#endif /* __LINUX_EHCI_TCC_H */

/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __LINUX_OHCI_TCC_H
#define __LINUX_OHCI_TCC_H

#include <linux/usb/tcc.h>

#define DRIVER_DESC "OHCI Telechips driver"

#define HcRhDescriptorA		(0x48)
#define PMM_PERPORT_MODE	(1)

static struct hc_driver __read_mostly tcc_ohci_hc_driver;

struct tcc_ohci_hcd {
	struct device	*dev;
	struct usb_hcd	*hcd;

	void __iomem	*phy_regs;

	resource_size_t	phy_rsrc_start;
	resource_size_t	phy_rsrc_len;
	uint32_t	hcd_tpl_support;
};

extern int32_t ehci_phy_set;

static void tcc_ohci_parse_dt(struct platform_device *pdev,
		struct tcc_ohci_hcd *tcc_ohci);
static void tcc_ohci_ctrl_phy(struct tcc_ohci_hcd *tcc_ohci, int32_t on_off);
static int32_t tcc_ohci_select_pmm(void __iomem *base, int32_t mode);

static int32_t tcc_ohci_probe(struct platform_device *pdev);
static int32_t tcc_ohci_remove(struct platform_device *pdev);

static int32_t __init tcc_ohci_init(void);
static void __exit tcc_ohci_cleanup(void);

#endif /* __LINUX_OHCI_TCC_H  */

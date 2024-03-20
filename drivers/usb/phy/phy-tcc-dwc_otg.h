/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __LINUX_PHY_TCC_DWC_OTG_H
#define __LINUX_PHY_TCC_DWC_OTG_H

#include <linux/usb/tcc.h>

// USB Host/Device PHY Configuration Register0 (U20DH_DEV_PCFG0)
// base address: 0x11DA0100
#define PHY_POR		(BIT(31))
#define SIDDQ		(BIT(24))
#define IRQ_PHYVLDEN	(BIT(20))
#define RCS		(BIT(5)) // The PLL uses CLKCORE as reference
#define FSEL		(BIT(2) | BIT(0)) // 24MHz, reference clock frequency

#define PCFG0_RST /* reset value: 0x83000025 */ \
	(PHY_POR | BIT(25) | SIDDQ | RCS | FSEL)

// USB Host/Device PHY Configuration Register2 (USB20DH_PCFG2)
// base address: 0x11DA0108
#define ACAENB		(BIT(13))

// USB Host/Device PHY Configuration 4 Register (USB20DH_PCFG4)
// base address: 0x11DA0110
#define IRQ_PHYVALID	(BIT(27))
#define DPPD		(BIT(12))
#define DMPD		(BIT(10))

// USB Host/Device LINK Configuration 0 Register (USB20DH_LCFG0)
// base address: 0x11DA0118
#define PRSTN		(BIT(29))

struct tcc_dwc_otg_phy {
	struct device		*dev;
	struct usb_phy		phy;
	struct regulator	*vbus_supply;

	void __iomem		*base_addr;

	bool			vbus_enabled;
};

static int32_t tcc_dwc_otg_phy_create(struct device *dev,
		struct tcc_dwc_otg_phy *tcc_dwc_otg);
static int32_t tcc_dwc_otg_phy_init(struct usb_phy *phy);
static int32_t tcc_dwc_otg_phy_set_vbus(struct usb_phy *phy, int32_t on_off);

static void __iomem *tcc_dwc_otg_phy_get_base(struct usb_phy *phy);
static int32_t tcc_dwc_otg_phy_set_vbus_resource(struct usb_phy *phy);

#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
static int32_t tcc_dwc_otg_phy_get_dc_level(struct usb_phy *phy);
static int32_t tcc_dwc_otg_phy_set_dc_level(struct usb_phy *phy, uint32_t level);
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

static int32_t tcc_dwc_otg_phy_probe(struct platform_device *pdev);
static int32_t tcc_dwc_otg_phy_remove(struct platform_device *pdev);

static int32_t __init tcc_dwc_otg_phy_drv_init(void);
static void __exit tcc_dwc_otg_phy_drv_cleanup(void);

#endif /* __LINUX_PHY_TCC_DWC_OTG_H */

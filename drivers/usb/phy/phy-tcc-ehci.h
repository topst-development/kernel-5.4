/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __LINUX_PHY_TCC_EHCI_H
#define __LINUX_PHY_TCC_EHCI_H

#include <linux/usb/tcc.h>

// USB20H PHY Configuration 0 Register (USB20H_PCFG0)
// base address: 0x11DA0014
#define PHY_POR		(BIT(31))
#define SIDDQ		(BIT(24))
#define REFCLKSEL	(BIT(5)) // The PLL uses CLKCORE as reference
#define FSEL		(BIT(2) | BIT(0)) // 24MHz, reference clock frequency

#define PCFG0_RST /* reset value: 0x83000025 */ \
	(PHY_POR | BIT(25) | SIDDQ | REFCLKSEL | FSEL)

// USB20H PHY Configuration 2 Register(USB20H_PCFG2)
// base address: 0x11DA001C
#define CHGDET		(BIT(22))
#define CHRGSEL		(BIT(10))
#define VDATSRCENB	(BIT(9))
#define VDATDETENB	(BIT(8))

// USB20H PHY Configuration 4 Register (USB20H_PCFG4)
// base address: 0x11DA0024
#define IRQ_CLR		(BIT(31))
#define IRQ_PHYVALIDEN	(BIT(30))
#define IRQ_CHGDETEN	(BIT(28))
#define IRQ_PHYVALID	(BIT(27))
#define DPPULLDOWN	(BIT(12))
#define DMPULLDOWN	(BIT(10))

#define DP_DM_PULLDOWN	(DPPULLDOWN | DMPULLDOWN)

// USB20H LINK Configuration Register 0(USB20H_LCFG0)
// base address: 0x11DA002C
#define PHYRSTN		(BIT(29))
#define UTMIRSTN	(BIT(28))
#define FLADJ_VAL	(BIT(15)) // should be the same as FLADJ_VAL_HOST(0x20)
#define FLADJ_VAL_HOST	(BIT(5)) // Frame Length: 60000, FLADJ Value: 32(0x20)

#define LCFG0_RST /* reset value: 0x30048020 */ \
	(PHYRSTN | UTMIRSTN | BIT(18) | FLADJ_VAL | FLADJ_VAL_HOST)

struct USB20H_PHY { // base address: 0x11DA0010
	volatile uint32_t BCFG;		// 0x10
	volatile uint32_t PCFG0;	// 0x14
	volatile uint32_t PCFG1;	// 0x18
	volatile uint32_t PCFG2;	// 0x1C
	volatile uint32_t PCFG3;	// 0x20
	volatile uint32_t PCFG4;	// 0x24
	volatile uint32_t STS;		// 0x28
	volatile uint32_t LCFG0;	// 0x2C
	volatile uint32_t LCFG1;	// 0x30
};

struct tcc_ehci_phy {
	struct device		*dev;
	struct usb_phy		phy;
	struct regulator	*vbus_supply;

	void __iomem		*base_addr;

	bool			vbus_enabled;
	int32_t			mux_port;

#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
	struct task_struct	*ehci_chgdet_thread;
	struct work_struct	chgdet_work;

	bool			chg_ready;
	int			irq;
#endif /* CONFIG_ENABLE_BC_20_HOST || CONFIG_ENABLE_BC_20_DRD */
};

static int32_t tcc_ehci_phy_create(struct device *dev,
		struct tcc_ehci_phy *tcc_ehci);
static int32_t tcc_ehci_phy_init(struct usb_phy *phy);
static void tcc_ehci_phy_shutdown(struct usb_phy *phy);
static int32_t tcc_ehci_phy_set_vbus(struct usb_phy *phy, int32_t on_off);

static void __iomem *tcc_ehci_phy_get_base(struct usb_phy *phy);
static void tcc_ehci_phy_select_mux(struct usb_phy *phy, int32_t is_mux);
static int32_t tcc_ehci_phy_set_state(struct usb_phy *phy, int32_t on_off);
static int32_t tcc_ehci_phy_set_vbus_resource(struct usb_phy *phy);

#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
static int tcc_ehci_phy_get_dc_level(struct usb_phy *phy);
static int tcc_ehci_phy_set_dc_level(struct usb_phy *phy, uint32_t level);
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
static irqreturn_t tcc_ehci_phy_chg_det_irq(int irq, void *data);
static void tcc_ehci_phy_chg_det_monitor(struct work_struct *data);
static int tcc_ehci_phy_chg_det_thread(void *work);
static void tcc_ehci_phy_set_chg_det(struct usb_phy *phy);
static void tcc_ehci_phy_stop_chg_det(struct usb_phy *phy);
#endif /* CONFIG_ENABLE_BC_20_HOST || CONFIG_ENABLE_BC_20_DRD */

static int32_t tcc_ehci_phy_probe(struct platform_device *pdev);
static int32_t tcc_ehci_phy_remove(struct platform_device *pdev);

static int32_t __init tcc_ehci_phy_drv_init(void);
static void __exit tcc_ehci_phy_drv_cleanup(void);

#endif /* __LINUX_PHY_TCC_EHCI_H */

/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __LINUX_PHY_TCC_DWC3_H
#define __LINUX_PHY_TCC_DWC3_H

#include <linux/usb/tcc.h>

// USB30 PHY Configuration Register 0 (U30_PCFG0)
// base address: 0x11D90010
#define PHY_RESET		(BIT(30))
#define PD_SS			(BIT(25))
#define SRAM_INIT_DONE		(BIT(5))
#define SRAM_EXT_LD_DONE	(BIT(3))
#define PHY_STABLE		(BIT(2))

// USB30 PHY Configuration Register 4 (U30_PCFG4)
// base address: 0x11D90020
#define PHY_EXT_CTRL_SEL	(BIT(0))

// USB30 PHY Interrupt Register (U30_PINT)
// base address: 0x11D9007C
#define PINT_EN			(BIT(31))
#define PINT_STS		(BIT(22)) // 64(0x40)
#define PINT_MSK_RESERVED	(BIT(7) | BIT(3) | BIT(2) | BIT(1) | BIT(0))
#define PINT_MSK_CHGDET		(BIT(6))
#define PINT_MSK_PHY		(BIT(5))
#define PINT_MSK_BC_CHIRP_ON	(BIT(4))

// USB30 Link Configuration Register 0 (U30_LCFG)
// base address: 0x11D90080
#define VCC_RESET_N		(BIT(31))
#define PPC			(BIT(7))
#define HUB_PORT_PERM_ATTACH \
	(BIT(27) | BIT(26)) // USB2.0 & 3.0 Port Permanently Attached
#define FLADJ			(BIT(17)) // HS Jitter Adjust: 32(0x20)

// USB 3.0 High-speed PHY Configuration Register Set 0 (U30_FPCFG0)
// base address: 0x11D900A0
#define PHY_POR			(BIT(31))
#define SIDDQ			(BIT(24))

// USB 3.0 High-speed PHY Configuration Register Set 2 (U30_FPCFG2)
// base address: 0x11D900A8
#define CHGDET			(BIT(22))
#define CHRGSEL			(BIT(10))
#define VDATSRCENB		(BIT(9))
#define VDATDETENB		(BIT(8))

// USB 3.0 High-speed PHY Configuration Register Set 3 (U30_FPCFG3)
// base address: 0x11D900AC
#define TDOSEL			(BIT(29))
#define TCK			(BIT(28))
#define TAD \
	(BIT(26) | BIT(25)) // Test Address(0x6)
#define TDI \
	(BIT(23) | BIT(22) | BIT(21) | BIT(20)) // Test Data In(0xC0)
#define TDO \
	(BIT(15) | BIT(14) | BIT(13) | BIT(12)) // Test Data Out(0xF)

// USB 3.0 High-speed PHY Configuration Register Set 4 (U30_FPCFG4)
// base address: 0x11D900B0
#define	IRQ_CLR			(BIT(31))
#define IRQ_PHYVALIDEN		(BIT(30))
#define IRQ_PHYVALID		(BIT(28))

#define RETRY_CNT		(10000)

struct tcc_dwc3_phy {
	struct device		*dev;
	struct usb_phy		phy;
	struct regulator	*vbus_supply;

	void __iomem		*base_addr;
	void __iomem		*h_base;
	void __iomem		*ref_base;

	bool			vbus_enabled;

#if defined(CONFIG_ENABLE_BC_30_HOST)
	struct work_struct	dwc3_work;
	struct task_struct	*dwc3_chgdet_thread;

	bool			chg_ready;
	int32_t			irq;
#endif /* CONFIG_ENABLE_BC_30_HOST */
};

static int32_t is_suspended = 1;

static int32_t tcc_dwc3_create_phy(struct device *dev,
		struct tcc_dwc3_phy *phy_dev);
static int32_t tcc_dwc3_phy_init(struct usb_phy *phy);
static void tcc_dwc3_phy_shutdown(struct usb_phy *phy);
static int32_t tcc_dwc3_phy_set_vbus(struct usb_phy *phy, int32_t on_off);
static int32_t tcc_dwc3_phy_set_suspend(struct usb_phy *phy, int32_t suspend);

static void __iomem *tcc_dwc3_phy_get_base(struct usb_phy *phy);
static int32_t tcc_dwc3_phy_set_vbus_resource(struct usb_phy *phy);

#if defined(CONFIG_ENABLE_BC_30_HOST)
static irqreturn_t tcc_dwc3_phy_chg_det_irq(int32_t irq, void *data);
static void tcc_dwc3_phy_chg_det_monitor(struct work_struct *data);
static int tcc_dwc3_phy_chg_det_thread(void *work);
static void tcc_dwc3_phy_set_chg_det(struct usb_phy *phy);
static void tcc_dwc3_phy_stop_chg_det(struct usb_phy *phy);
#endif /* CONFIG_ENABLE_BC_30_HOST */

static int32_t tcc_dwc3_phy_probe(struct platform_device *pdev);
static int32_t tcc_dwc3_phy_remove(struct platform_device *pdev);

static int32_t __init tcc_dwc3_phy_drv_init(void);
static void __exit tcc_dwc3_phy_drv_cleanup(void);

#endif /* __LINUX_PHY_TCC_DWC3_H */

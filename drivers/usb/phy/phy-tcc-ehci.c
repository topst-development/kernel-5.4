// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/usb/otg.h>

#include "phy-tcc-ehci.h"

static int32_t tcc_ehci_phy_create(struct device *dev,
		struct tcc_ehci_phy *tcc_ehci)
{
	int32_t ret = 0;

	if ((dev == NULL) || (tcc_ehci == NULL))
		return -ENODEV;

	tcc_ehci->phy.otg =
		devm_kzalloc(dev, sizeof(*tcc_ehci->phy.otg), GFP_KERNEL);
	if (tcc_ehci->phy.otg == NULL)
		return -ENOMEM;

	tcc_ehci->mux_port = (of_find_property(
				dev->of_node, "mux_port", NULL) !=
			NULL) ? 1 : 0;

	tcc_ehci->dev			= dev;

	tcc_ehci->phy.dev		= tcc_ehci->dev;
	tcc_ehci->phy.label		= "tcc_ehci_phy";
	tcc_ehci->phy.type		= USB_PHY_TYPE_USB2;

	tcc_ehci->phy.init		= tcc_ehci_phy_init;
	tcc_ehci->phy.shutdown		= tcc_ehci_phy_shutdown;
	tcc_ehci->phy.set_vbus		= tcc_ehci_phy_set_vbus;

	tcc_ehci->phy.get_base		= tcc_ehci_phy_get_base;
	tcc_ehci->phy.select_mux	= tcc_ehci_phy_select_mux;
	tcc_ehci->phy.set_state		= tcc_ehci_phy_set_state;
	tcc_ehci->phy.set_vbus_resource	= tcc_ehci_phy_set_vbus_resource;

#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
	tcc_ehci->phy.get_dc_level	= tcc_ehci_phy_get_dc_level;
	tcc_ehci->phy.set_dc_level	= tcc_ehci_phy_set_dc_level;
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
#if !defined(CONFIG_ENABLE_BC_20_DRD)
	if (tcc_ehci->mux_port != 0)
		goto skip;
#endif /* !defined(CONFIG_ENABLE_BC_20_DRD) */

#if !defined(CONFIG_ENABLE_BC_20_HOST)
	if (tcc_ehci->mux_port == 0)
		goto skip;
#endif /* !defined(CONFIG_ENABLE_BC_20_HOST) */

	tcc_ehci->phy.set_chg_det	= tcc_ehci_phy_set_chg_det;
	tcc_ehci->phy.stop_chg_det	= tcc_ehci_phy_stop_chg_det;

#if !defined(CONFIG_ENABLE_BC_20_DRD) || !defined(CONFIG_ENABLE_BC_20_HOST)
skip:
#endif
#endif /* CONFIG_ENABLE_BC_20_HOST || CONFIG_ENABLE_BC_20_DRD */

	tcc_ehci->phy.otg->usb_phy	= &tcc_ehci->phy;

	if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		ret = tcc_ehci_phy_set_vbus_resource(&tcc_ehci->phy);
	}

	return ret;
}

static int32_t tcc_ehci_phy_init(struct usb_phy *phy)
{
	struct tcc_ehci_phy *tcc_ehci =
		container_of(phy, struct tcc_ehci_phy, phy);
	struct USB20H_PHY *usb20h;
	uint32_t mux_sel_val;
	int32_t i = 0;

	if (tcc_ehci == NULL)
		return -ENODEV;

	dev_info(tcc_ehci->dev, "[INFO][USB] %s() call SUCCESS\n", __func__);

	usb20h = (struct USB20H_PHY *)tcc_ehci->base_addr;

	if (tcc_ehci->mux_port != 0) {
		/* get otg control cfg register */
		mux_sel_val = readl(phy->otg->mux_cfg_addr);
		BIT_CLR_SET(mux_sel_val, SEL, MUX_HST_SEL);
		writel(mux_sel_val, phy->otg->mux_cfg_addr);
	}

	// Reset PHY Registers
	if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		writel(0x03000115, &usb20h->PCFG0);
	} else {
		writel(PCFG0_RST, &usb20h->PCFG0);
	}

	if (tcc_ehci->mux_port != 0) {
		// EHCI MUX Host PHY Configuration
		writel(0xE31C243A, &usb20h->PCFG1);
	} else {
		// EHCI PHY Configuration
		writel(0xE31C243A, &usb20h->PCFG1);
	}

	if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		writel(0x4, &usb20h->PCFG2);
	} else {
		writel(0x0, &usb20h->PCFG2);
	}
	writel(0x0, &usb20h->PCFG3);
	writel(0x0, &usb20h->PCFG4);
	writel(LCFG0_RST, &usb20h->LCFG0);

	// Set the POR
	writel(readl(&usb20h->PCFG0) | PHY_POR, &usb20h->PCFG0);
	// Set the Core Reset
	writel(readl(&usb20h->LCFG0) & ~(PHYRSTN | UTMIRSTN), &usb20h->LCFG0);

	if (!IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		// Clear SIDDQ
		writel(readl(&usb20h->PCFG0) & ~SIDDQ, &usb20h->PCFG0);
	}
	udelay(30);

	// Release POR
	writel(readl(&usb20h->PCFG0) & ~PHY_POR, &usb20h->PCFG0);

	if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		// Clear SIDDQ
		writel(readl(&usb20h->PCFG0) & ~SIDDQ, &usb20h->PCFG0);
	}

	// Set Phyvalid en
	writel(readl(&usb20h->PCFG4) | IRQ_PHYVALIDEN, &usb20h->PCFG4);
	// Set DP/DM (pull down)
	writel(readl(&usb20h->PCFG4) | DP_DM_PULLDOWN, &usb20h->PCFG4);

	// Wait Phy Valid Interrupt
	while (i < 10000) {
		if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
			if ((readl(&usb20h->PCFG0) & BIT(21)) != 0U)
				break;
		} else {
			if ((readl(&usb20h->PCFG4) & IRQ_PHYVALID) != 0U)
				break;
		}

		i++;
		udelay(5);
	}

	dev_info(tcc_ehci->dev, "[INFO][USB] EHCI PHY valid check %s\n",
			(i >= 9999) ? "FAIL!" : "SUCCESS");

	// Release Core Reset
	writel(readl(&usb20h->LCFG0) | (PHYRSTN | UTMIRSTN), &usb20h->LCFG0);

#if defined(CONFIG_USB_HS_DC_VOLTAGE_LEVEL)
	phy->set_dc_level(phy, CONFIG_USB_HS_DC_VOLTAGE_LEVEL);
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
#if !defined(CONFIG_ENABLE_BC_20_DRD)
	if (tcc_ehci->mux_port != 0) {
		dev_info(tcc_ehci->dev, "[INFO][USB] Not support BC1.2 for 2.0 DRD port\n");
		return 0;
	}
#endif /* !defined(CONFIG_ENABLE_BC_20_DRD) */

#if !defined(CONFIG_ENABLE_BC_20_HOST)
	if (tcc_ehci->mux_port == 0) {
		dev_info(tcc_ehci->dev, "[INFO][USB] Not support BC1.2 for 2.0 HOST port\n");
		return 0;
	}
#endif /* !defined(CONFIG_ENABLE_BC_20_HOST) */

	dev_info(tcc_ehci->dev, "[INFO][USB] Setting for Charging Detection\n");

	// clear irq
	writel(readl(&usb20h->PCFG4) | IRQ_CLR, &usb20h->PCFG4);

	// Disable VBUS Detect
	writel(readl(&usb20h->PCFG4) & ~IRQ_PHYVALIDEN, &usb20h->PCFG4);

	// clear irq
	writel(readl(&usb20h->PCFG4) & ~IRQ_CLR, &usb20h->PCFG4);
	udelay(1);

	writel(readl(&usb20h->PCFG2) | (CHRGSEL | VDATDETENB), &usb20h->PCFG2);
	udelay(1);

	// enable CHG_DET interrupt
	writel(readl(&usb20h->PCFG4) | IRQ_CHGDETEN, &usb20h->PCFG4);

	INIT_WORK(&tcc_ehci->chgdet_work, tcc_ehci_phy_chg_det_monitor);

	enable_irq(tcc_ehci->irq);
#endif /* CONFIG_ENABLE_BC_20_HOST || CONFIG_ENABLE_BC_20_DRD */

	return 0;
}

static void tcc_ehci_phy_shutdown(struct usb_phy *phy)
{
	if (phy == NULL) {
		return;
	}

	phy->set_state(phy, OFF);
}

static int32_t tcc_ehci_phy_set_vbus(struct usb_phy *phy, int32_t on_off)
{
	struct tcc_ehci_phy *tcc_ehci =
		container_of(phy, struct tcc_ehci_phy, phy);
	struct device *dev;
	int32_t ret = 0;

	if (tcc_ehci == NULL)
		return -ENODEV;

	dev = tcc_ehci->dev;

	/*
	 * Check that the "vbus-ctrl-able" property for the USB PHY driver node
	 * is declared in the device tree.
	 */
	if (of_find_property(dev->of_node, "vbus-ctrl-able", 0) == NULL) {
		dev_err(dev, "[ERROR][USB] vbus-ctrl-able property is not declared.\n");
		return -ENODEV;
	}

	if (tcc_ehci->vbus_supply == NULL) {
		dev_err(dev, "[ERROR][USB] Vbus Supply is not valid.\n");
		return -ENODEV;
	}

	if ((on_off == ON) && (tcc_ehci->vbus_enabled == false)) {
		ret = regulator_enable(tcc_ehci->vbus_supply);
		if (ret)
			goto err;

		ret = regulator_set_voltage(tcc_ehci->vbus_supply,
				ON_VOLTAGE, ON_VOLTAGE);
		if (ret)
			goto err;

		tcc_ehci->vbus_enabled = true;
	} else if ((on_off == OFF) && (tcc_ehci->vbus_enabled == true)) {
		ret = regulator_set_voltage(tcc_ehci->vbus_supply,
				OFF_VOLTAGE, OFF_VOLTAGE);
		if (ret)
			goto err;

		regulator_disable(tcc_ehci->vbus_supply);
		tcc_ehci->vbus_enabled = false;
	} else if ((on_off == ON) && (tcc_ehci->vbus_enabled == true)) {
		dev_info(dev, "[INFO][USB] Vbus is already ENABLED!\n");
	} else if ((on_off == OFF) && (tcc_ehci->vbus_enabled == false)) {
		dev_info(dev, "[INFO][USB] Vbus is already DISABLED!\n");
	}
err:
	if (ret) {
		dev_err(dev, "[ERROR][USB] Vbus Supply is not controlled.\n");
		regulator_disable(tcc_ehci->vbus_supply);
	} else {
		usleep_range(3000, 5000);
	}

	return ret;
}

static void __iomem *tcc_ehci_phy_get_base(struct usb_phy *phy)
{
	struct tcc_ehci_phy *tcc_ehci =
		container_of(phy, struct tcc_ehci_phy, phy);

	if (tcc_ehci == NULL)
		return (void __iomem *)0;

	return tcc_ehci->base_addr;
}

static void tcc_ehci_phy_select_mux(struct usb_phy *phy, int32_t is_mux)
{
	struct tcc_ehci_phy *tcc_ehci =
		container_of(phy, struct tcc_ehci_phy, phy);
	uint32_t mux_sel_val;

	if (tcc_ehci != NULL) {
		if (tcc_ehci->mux_port != 0) {
			mux_sel_val = readl(phy->otg->mux_cfg_addr);

			if (is_mux != 0) {
				BIT_CLR_SET(mux_sel_val, SEL, MUX_HST_SEL);
				writel(mux_sel_val, phy->otg->mux_cfg_addr);
			} else {
				BIT_SET(mux_sel_val,
						(SEL |
						 MUX_HST_SEL | MUX_DEV_SEL));
				writel(mux_sel_val, phy->otg->mux_cfg_addr);
			}
		}
	}
}

static int32_t tcc_ehci_phy_set_state(struct usb_phy *phy, int32_t on_off)
{
	struct tcc_ehci_phy *tcc_ehci =
		container_of(phy, struct tcc_ehci_phy, phy);
	struct USB20H_PHY *usb20h = NULL;

	if (tcc_ehci == NULL)
		return -ENODEV;

	usb20h = (struct USB20H_PHY *)tcc_ehci->base_addr;

	if (on_off == 1) {
		BIT_CLR(usb20h->PCFG0, (PHY_POR | SIDDQ));
		dev_info(tcc_ehci->dev, "[INFO][USB] EHCI PHY start\n");
	} else if (on_off == 0) {
		BIT_SET(usb20h->PCFG0, (PHY_POR | SIDDQ));
		dev_info(tcc_ehci->dev, "[INFO][USB] EHCI PHY stop\n");
	}

	dev_info(tcc_ehci->dev, "[INFO][USB] EHCI PHY PCFG0 : 0x%08X\n",
			usb20h->PCFG0);
	return 0;
}

static int32_t tcc_ehci_phy_set_vbus_resource(struct usb_phy *phy)
{
	struct tcc_ehci_phy *tcc_ehci =
		container_of(phy, struct tcc_ehci_phy, phy);
	struct device *dev = tcc_ehci->dev;

	/*
	 * Check that the "vbus-ctrl-able" property for the USB PHY driver node
	 * is declared in the device tree.
	 */
	if (of_find_property(dev->of_node, "vbus-ctrl-able", 0) != NULL) {
		/*
		 * Get the vbus regulator declared in the "vbus-supply" property
		 * for the USB PHY driver node.
		 */
		if (tcc_ehci->vbus_supply == NULL) {
			tcc_ehci->vbus_supply =
				devm_regulator_get_optional(dev, "vbus");
			if (IS_ERR(tcc_ehci->vbus_supply)) {
				dev_err(dev, "[ERROR][USB] VBus Supply is not valid.\n");
				return -ENODEV;
			}
		}
	} else {
		dev_info(dev, "[INFO][USB] vbus-ctrl-able property is not declared.\n");
	}

	return 0;
}

#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
static int tcc_ehci_phy_get_dc_level(struct usb_phy *phy)
{
	struct tcc_ehci_phy *tcc_ehci =
		container_of(phy, struct tcc_ehci_phy, phy);
	struct USB20H_PHY *usb20h = (struct USB20H_PHY *)tcc_ehci->base_addr;
	uint32_t pcfg1_val;

	pcfg1_val = readl(&usb20h->PCFG1);

	return BIT_MSK(pcfg1_val, PCFG_TXVRT);
}

static int tcc_ehci_phy_set_dc_level(struct usb_phy *phy, uint32_t level)
{
	struct tcc_ehci_phy *tcc_ehci =
		container_of(phy, struct tcc_ehci_phy, phy);
	struct USB20H_PHY *usb20h = (struct USB20H_PHY *)tcc_ehci->base_addr;
	uint32_t pcfg1_val;

	pcfg1_val = readl(&usb20h->PCFG1);
	BIT_CLR_SET(pcfg1_val, PCFG_TXVRT, level);
	writel(pcfg1_val, &usb20h->PCFG1);

	dev_info(tcc_ehci->dev, "[INFO][USB] current DC voltage level: %d\n",
			BIT_MSK(pcfg1_val, PCFG_TXVRT));

	return 0;
}
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
static irqreturn_t tcc_ehci_phy_chg_det_irq(int irq, void *data)
{
	struct tcc_ehci_phy *tcc_ehci = (struct tcc_ehci_phy *)data;
	struct USB20H_PHY *usb20h = (struct USB20H_PHY *)tcc_ehci->base_addr;

	writel(readl(&usb20h->PCFG4) & ~IRQ_CHGDETEN, &usb20h->PCFG4);

	dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Charging Detection!\n");

	// clear irq
	writel(readl(&usb20h->PCFG4) | IRQ_CLR, &usb20h->PCFG4);
	udelay(1);

	// clear irq
	writel(readl(&usb20h->PCFG4) & ~IRQ_CLR, &usb20h->PCFG4);

	schedule_work(&tcc_ehci->chgdet_work);

	return IRQ_HANDLED;
}

static void tcc_ehci_phy_chg_det_monitor(struct work_struct *data)
{
	struct tcc_ehci_phy *tcc_ehci =
			container_of(data, struct tcc_ehci_phy, chgdet_work);
	struct USB20H_PHY *usb20h = (struct USB20H_PHY *)tcc_ehci->base_addr;
	uint32_t pcfg2 = 0;
	int32_t timeout_count = 500;

	tcc_ehci->chg_ready = true;

	pcfg2 = readl(&usb20h->PCFG2);
	writel((pcfg2 | VDATSRCENB), &usb20h->PCFG2);

	while (timeout_count > 0) {
		pcfg2 = readl(&usb20h->PCFG2);

		if ((pcfg2 & CHGDET) != 0) { // Check VDP_SRC signal
			usleep_range(1000, 1100);
			timeout_count--;
		} else {
			break;
		}
	}

	if (timeout_count == 0) {
		dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Timeout - VDM_SRC is still High\n");
	} else {
		dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Time count(%d) - VDM_SRC is low\n",
				(500 - timeout_count));
	}

	writel(readl(&usb20h->PCFG2) & ~(VDATSRCENB | VDATDETENB),
			&usb20h->PCFG2);

	if (tcc_ehci->chg_ready == true) {
		dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Enable chg det monitor!\n");

		if (tcc_ehci->ehci_chgdet_thread != NULL) {
			kthread_stop(tcc_ehci->ehci_chgdet_thread);
			tcc_ehci->ehci_chgdet_thread = NULL;
		}

		dev_dbg(tcc_ehci->dev, "[DEBUG][USB] start chg det thread!\n");

		tcc_ehci->ehci_chgdet_thread =
			kthread_run(tcc_ehci_phy_chg_det_thread,
					(void *)tcc_ehci, "ehci-chgdet");
		if (IS_ERR(tcc_ehci->ehci_chgdet_thread))
			dev_err(tcc_ehci->dev, "[ERROR][USB] failed to run tcc_ehci_phy_chg_det_thread\n");
	} else {
		dev_dbg(tcc_ehci->dev, "[DEBUG][USB] No need to start chg det monitor\n");
	}
}

static int tcc_ehci_phy_chg_det_thread(void *work)
{
	struct tcc_ehci_phy *tcc_ehci = (struct tcc_ehci_phy *)work;
	struct USB20H_PHY *usb20h = (struct USB20H_PHY *)tcc_ehci->base_addr;
	uint32_t pcfg2 = 0;

	int timeout = 1000;

	dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Start to check CHGDET\n");

	while (!kthread_should_stop() && timeout > 0) {
		pcfg2 = readl(&usb20h->PCFG2);
		usleep_range(1000, 1100);
		timeout--;
	}

	if (timeout <= 0) {
		writel(readl(&usb20h->PCFG2) | VDATDETENB, &usb20h->PCFG2);
		writel(readl(&usb20h->PCFG4) | IRQ_CHGDETEN, &usb20h->PCFG4);
		dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Enable VDATDETENB\n");
	}

	tcc_ehci->ehci_chgdet_thread = NULL;

	dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Monitoring is finished(%d)\n",
			timeout);

	return 0;
}

static void tcc_ehci_phy_set_chg_det(struct usb_phy *phy)
{
	struct tcc_ehci_phy *tcc_ehci =
		container_of(phy, struct tcc_ehci_phy, phy);
	struct USB20H_PHY *usb20h = (struct USB20H_PHY *)tcc_ehci->base_addr;

	dev_dbg(tcc_ehci->dev, "[DEBUG][USB] start chg det!\n");

	// clear irq
	writel(readl(&usb20h->PCFG4) | IRQ_CLR, &usb20h->PCFG4);
	udelay(1);

	// clear irq
	writel(readl(&usb20h->PCFG4) & ~IRQ_CLR, &usb20h->PCFG4);

	// enable chg det
	writel(readl(&usb20h->PCFG2) | VDATDETENB, &usb20h->PCFG2);
	writel(readl(&usb20h->PCFG4) | IRQ_CHGDETEN, &usb20h->PCFG4);
}

static void tcc_ehci_phy_stop_chg_det(struct usb_phy *phy)
{
	struct tcc_ehci_phy *tcc_ehci =
		container_of(phy, struct tcc_ehci_phy, phy);
	struct USB20H_PHY *usb20h = (struct USB20H_PHY *)tcc_ehci->base_addr;

	tcc_ehci->chg_ready = false;

	dev_dbg(tcc_ehci->dev, "[DEBUG][USB] stop chg det!\n");

	if (tcc_ehci->ehci_chgdet_thread != NULL) {
		dev_dbg(tcc_ehci->dev, "[DEBUG][USB] kill chg det thread!\n");
		kthread_stop(tcc_ehci->ehci_chgdet_thread);
		tcc_ehci->ehci_chgdet_thread = NULL;
	}

	dev_dbg(tcc_ehci->dev, "[DEBUG][USB] PCFG2= 0x%x/PCFG4=0x%x\n",
			readl(&usb20h->PCFG2), readl(&usb20h->PCFG4));

	// disable chg det
	writel(readl(&usb20h->PCFG4) & ~IRQ_CHGDETEN, &usb20h->PCFG4);
	writel(readl(&usb20h->PCFG2) & ~(VDATSRCENB | VDATDETENB),
			&usb20h->PCFG2);

	dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Disable chg det! PCFG2= 0x%x/PCFG4=0x%x\n",
			readl(&usb20h->PCFG2), readl(&usb20h->PCFG4));
}
#endif /* CONFIG_ENABLE_BC_20_HOST || CONFIG_ENABLE_BC_20_DRD */

static int32_t tcc_ehci_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_ehci_phy *tcc_ehci;
	int32_t ret;

#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
	int32_t irq = 0;
#endif /* CONFIG_ENABLE_BC_20_HOST || CONFIG_ENABLE_BC_20_DRD */

	dev_info(dev, "[INFO][USB] %s() call SUCCESS\n", __func__);

	tcc_ehci = devm_kzalloc(dev, sizeof(*tcc_ehci), GFP_KERNEL);

	ret = tcc_ehci_phy_create(dev, tcc_ehci);
	if (ret != 0) {
		dev_err(dev, "[ERROR][USB] tcc_ehci_phy_create() FAIL!\n");
		return ret;
	}

#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(dev, "[ERROR][USB] Found HC with no IRQ. Check %s setup!\n",
				dev_name(dev));
		ret = -ENODEV;
	} else {
		dev_info(dev, "[INFO][USB] platform_get_irq() SUCCESS, irq: %d\n",
				irq);
		tcc_ehci->irq = irq;
	}
#endif /* CONFIG_ENABLE_BC_20_HOST || CONFIG_ENABLE_BC_20_DRD */

	if (request_mem_region(pdev->resource[0].start,
				pdev->resource[0].end -
				pdev->resource[0].start + 1,
				"ehci_phy") == NULL) {
		dev_dbg(dev, "[DEBUG][USB] error reserving mapped memory\n");
		ret = -EFAULT;
	}

	tcc_ehci->base_addr = (void __iomem *)ioremap_nocache(
			(resource_size_t)pdev->resource[0].start,
			pdev->resource[0].end - pdev->resource[0].start + 1U);
	tcc_ehci->phy.base = tcc_ehci->base_addr;

#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
	ret = devm_request_irq(dev, tcc_ehci->irq, tcc_ehci_phy_chg_det_irq,
			IRQF_SHARED, pdev->dev.kobj.name, tcc_ehci);
	if (ret) {
		dev_err(dev, "[ERROR][USB] devm_request_irq() FAIL!, ret: %d\n",
				ret);
	}

	disable_irq(tcc_ehci->irq);
#endif /* CONFIG_ENABLE_BC_20_HOST || CONFIG_ENABLE_BC_20_DRD */

	platform_set_drvdata(pdev, tcc_ehci);

	ret = usb_add_phy_dev(&tcc_ehci->phy);
	if (ret != 0) {
		dev_err(dev, "[ERROR][USB] usb_add_phy_dev() FAIL!, ret: %d\n",
				ret);
		return ret;
	}

	return ret;
}

static int32_t tcc_ehci_phy_remove(struct platform_device *pdev)
{
	struct tcc_ehci_phy *tcc_ehci = platform_get_drvdata(pdev);

	usb_remove_phy(&tcc_ehci->phy);

	return 0;
}

static const struct of_device_id tcc_ehci_phy_match[] = {
	{ .compatible = "telechips,tcc_ehci_phy" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_ehci_phy_match);

static struct platform_driver tcc_ehci_phy_driver = {
	.probe			= tcc_ehci_phy_probe,
	.remove			= tcc_ehci_phy_remove,
	.driver = {
		.name		= "ehci_phy",
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(tcc_ehci_phy_match),
	},
};

static int32_t __init tcc_ehci_phy_drv_init(void)
{
	int32_t ret = 0;

	ret = platform_driver_register(&tcc_ehci_phy_driver);
	if (ret < 0)
		pr_err("[ERROR][USB] %s() FAIL! ret: %d\n", __func__, ret);

	return ret;
}
subsys_initcall_sync(tcc_ehci_phy_drv_init);

static void __exit tcc_ehci_phy_drv_cleanup(void)
{
	platform_driver_unregister(&tcc_ehci_phy_driver);
}
module_exit(tcc_ehci_phy_drv_cleanup);

MODULE_DESCRIPTION("Telechips EHCI USB transceiver driver");
MODULE_LICENSE("GPL v2");

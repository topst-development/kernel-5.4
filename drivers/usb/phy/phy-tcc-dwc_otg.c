// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>

#include "phy-tcc-dwc_otg.h"

static int32_t tcc_dwc_otg_phy_create(struct device *dev,
		struct tcc_dwc_otg_phy *tcc_dwc_otg)
{
	int32_t ret = 0;

	if ((dev == NULL) || (tcc_dwc_otg == NULL))
		return -ENODEV;

	tcc_dwc_otg->phy.otg =
		devm_kzalloc(dev, sizeof(*tcc_dwc_otg->phy.otg), GFP_KERNEL);
	if (tcc_dwc_otg->phy.otg == NULL)
		return -ENOMEM;

	tcc_dwc_otg->dev			= dev;

	tcc_dwc_otg->phy.dev			= tcc_dwc_otg->dev;
	tcc_dwc_otg->phy.label			= "tcc_dwc_otg_phy";
	tcc_dwc_otg->phy.type			= USB_PHY_TYPE_USB2;

	tcc_dwc_otg->phy.init			= &tcc_dwc_otg_phy_init;
	tcc_dwc_otg->phy.set_vbus		= &tcc_dwc_otg_phy_set_vbus;

	tcc_dwc_otg->phy.get_base		= &tcc_dwc_otg_phy_get_base;
	tcc_dwc_otg->phy.set_vbus_resource	=
		&tcc_dwc_otg_phy_set_vbus_resource;

#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
	tcc_dwc_otg->phy.get_dc_level		= &tcc_dwc_otg_phy_get_dc_level;
	tcc_dwc_otg->phy.set_dc_level		= &tcc_dwc_otg_phy_set_dc_level;
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

	tcc_dwc_otg->phy.otg->usb_phy		= &tcc_dwc_otg->phy;

	if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		ret = tcc_dwc_otg_phy_set_vbus_resource(&tcc_dwc_otg->phy);
	}

	return ret;
}

static int32_t tcc_dwc_otg_phy_init(struct usb_phy *phy)
{
	struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(phy, struct tcc_dwc_otg_phy, phy);
	struct U20DH_DEV_PHY *u20dh_dev;
	int32_t i = 0;

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	uint32_t mux_sel_val;
#endif

	if (tcc_dwc_otg == NULL)
		return -ENODEV;

	u20dh_dev = (struct U20DH_DEV_PHY *)tcc_dwc_otg->base_addr;

	dev_info(tcc_dwc_otg->dev, "[INFO][USB] %s() call SUCCESS\n", __func__);

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	writel(0x0, &u20dh_dev->MUX_SEL);
	udelay(10);

	mux_sel_val = readl(&u20dh_dev->MUX_SEL);
	BIT_SET(mux_sel_val, (SEL | MUX_HST_SEL | MUX_DEV_SEL));
	writel(mux_sel_val, &u20dh_dev->MUX_SEL);
#endif /* CONFIG_USB_DWC2_TCC_MUX */

	if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		writel(0x83000015, &u20dh_dev->PCFG0);
	} else {
		writel(PCFG0_RST, &u20dh_dev->PCFG0);
	}

	if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		writel(0x0330D643, &u20dh_dev->PCFG1);
	} else {
		writel(0xE31C243A, &u20dh_dev->PCFG1);
	}

#if defined(CONFIG_TCC_EH_ELECT_TST)
	writel((ACAENB | BIT(2)), &u20dh_dev->PCFG2);
#else
	if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		writel(BIT(2), &u20dh_dev->PCFG2);
	} else {
		writel(0x75852004, &u20dh_dev->PCFG2);
	}
#endif /* CONFIG_TCC_EH_ELECT_TST */

	writel(0x0, &u20dh_dev->PCFG3);
	writel(0x0, &u20dh_dev->PCFG4);

	if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		writel(PRSTN | BIT(28), &u20dh_dev->LCFG0);
	} else {
		writel(PRSTN, &u20dh_dev->LCFG0);
	}

	if (!IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		// Set the POR
		writel(readl(&u20dh_dev->PCFG0) | PHY_POR, &u20dh_dev->PCFG0);
	}

	// Set the Core Reset
	writel(readl(&u20dh_dev->LCFG0) & ~PRSTN, &u20dh_dev->LCFG0);
	if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		writel(readl(&u20dh_dev->LCFG0) & ~BIT(28), &u20dh_dev->LCFG0);
#if !defined(CONFIG_TCC_EH_ELECT_TST)
		writel(readl(&u20dh_dev->LCFG0) | (BIT(22) | BIT(21)),
				&u20dh_dev->LCFG0);
#endif
	}
	udelay(30);

	// Release POR
	writel(readl(&u20dh_dev->PCFG0) & ~PHY_POR, &u20dh_dev->PCFG0);
	// Clear SIDDQ
	writel(readl(&u20dh_dev->PCFG0) & ~SIDDQ, &u20dh_dev->PCFG0);

	if (!IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		// Set Phyvalid en
		writel(readl(&u20dh_dev->PCFG0) | IRQ_PHYVLDEN, &u20dh_dev->PCFG0);
		// Set DP/DM (pull down)
		writel(readl(&u20dh_dev->PCFG4) | (DPPD | DMPD), &u20dh_dev->PCFG4);

		// Wait Phy Valid Interrupt
		while (i < 10000) {
			if ((readl(&u20dh_dev->PCFG4) & IRQ_PHYVALID) != 0U)
				break;

			i++;
			udelay(5);
		}

		dev_info(tcc_dwc_otg->dev, "[INFO][USB] DWC OTG PHY valid check %s\n",
				(i >= 9999) ? "FAIL!" : "SUCCESS");
	}

	// disable PHYVALID_EN -> no irq
	writel(readl(&u20dh_dev->PCFG0) & ~IRQ_PHYVLDEN, &u20dh_dev->PCFG0);
	writel(readl(&u20dh_dev->PCFG0) & ~BIT(25), &u20dh_dev->PCFG0);

	if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		usleep_range(10000, 20000);
	}

	// Release Core Reset
	writel(readl(&u20dh_dev->LCFG0) | PRSTN, &u20dh_dev->LCFG0);

#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
	tcc_dwc_otg_phy_set_dc_level(phy, CONFIG_USB_HS_DC_VOLTAGE_LEVEL);
	dev_info(tcc_dwc_otg->dev, "[INFO][USB] PCFG1: 0x%x, TXVRT: 0x%x\n",
			u20dh_dev->PCFG1, CONFIG_USB_HS_DC_VOLTAGE_LEVEL);
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

	return 0;
}

static int32_t tcc_dwc_otg_phy_set_vbus(struct usb_phy *phy, int32_t on_off)
{
	struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(phy, struct tcc_dwc_otg_phy, phy);
	struct device *dev;
	int32_t ret = 0;

	if (tcc_dwc_otg == NULL)
		return -ENODEV;

	dev = tcc_dwc_otg->dev;

	/*
	 * Check that the "vbus-ctrl-able" property for the USB PHY driver node
	 * is declared in the device tree.
	 */
	if (of_find_property(dev->of_node, "vbus-ctrl-able", 0) == NULL) {
		dev_err(dev, "[ERROR][USB] vbus-ctrl-able property is not declared.\n");
		return -ENODEV;
	}

	if (tcc_dwc_otg->vbus_supply == NULL) {
		dev_err(dev, "[ERROR][USB] Vbus Supply is not valid.\n");
		return -ENODEV;
	}

	if ((on_off == ON) && (tcc_dwc_otg->vbus_enabled == false)) {
		ret = regulator_enable(tcc_dwc_otg->vbus_supply);
		if (ret)
			goto err;

		ret = regulator_set_voltage(tcc_dwc_otg->vbus_supply,
				ON_VOLTAGE, ON_VOLTAGE);
		if (ret)
			goto err;

		tcc_dwc_otg->vbus_enabled = true;
	} else if ((on_off == OFF) && (tcc_dwc_otg->vbus_enabled == true)) {
		ret = regulator_set_voltage(tcc_dwc_otg->vbus_supply,
				OFF_VOLTAGE, OFF_VOLTAGE);
		if (ret)
			goto err;

		regulator_disable(tcc_dwc_otg->vbus_supply);
		tcc_dwc_otg->vbus_enabled = false;
	} else if ((on_off == ON) && (tcc_dwc_otg->vbus_enabled == true)) {
		dev_info(dev, "[INFO][USB] Vbus is already ENABLED!\n");
	} else if ((on_off == OFF) && (tcc_dwc_otg->vbus_enabled == false)) {
		dev_info(dev, "[INFO][USB] Vbus is already DISABLED!\n");
	}
err:
	if (ret) {
		dev_err(dev, "[ERROR][USB] Vbus Supply is not controlled.\n");
		regulator_disable(tcc_dwc_otg->vbus_supply);
	} else {
		usleep_range(3000, 5000);
	}

	return ret;
}

static void __iomem *tcc_dwc_otg_phy_get_base(struct usb_phy *phy)
{
	struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(phy, struct tcc_dwc_otg_phy, phy);

	if (tcc_dwc_otg == NULL)
		return (void __iomem *)0;

	return tcc_dwc_otg->base_addr;
}

static int32_t tcc_dwc_otg_phy_set_vbus_resource(struct usb_phy *phy)
{
	struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(phy, struct tcc_dwc_otg_phy, phy);
	struct device *dev = tcc_dwc_otg->dev;

	/*
	 * Check that the "vbus-ctrl-able" property for the USB PHY driver node
	 * is declared in the device tree.
	 */
	if (of_find_property(dev->of_node, "vbus-ctrl-able", 0) != NULL) {
		/*
		 * Get the vbus regulator declared in the "vbus-supply" property
		 * for the USB PHY driver node.
		 */
		if (tcc_dwc_otg->vbus_supply == NULL) {
			tcc_dwc_otg->vbus_supply =
				devm_regulator_get_optional(dev, "vbus");
			if (IS_ERR(tcc_dwc_otg->vbus_supply)) {
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
static int32_t tcc_dwc_otg_phy_get_dc_level(struct usb_phy *phy)
{
	struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(phy, struct tcc_dwc_otg_phy, phy);
	struct U20DH_DEV_PHY *u20dh_dev =
		(struct U20DH_DEV_PHY *)tcc_dwc_otg->base_addr;
	uint32_t pcfg1_val;

	pcfg1_val = readl(&u20dh_dev->PCFG1);

	return BIT_MSK(pcfg1_val, PCFG_TXVRT);
}

static int32_t tcc_dwc_otg_phy_set_dc_level(struct usb_phy *phy, uint32_t level)
{
	struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(phy, struct tcc_dwc_otg_phy, phy);
	struct U20DH_DEV_PHY *u20dh_dev =
		(struct U20DH_DEV_PHY *)tcc_dwc_otg->base_addr;
	uint32_t pcfg1_val;

	pcfg1_val = readl(&u20dh_dev->PCFG1);
	BIT_CLR_SET(pcfg1_val, PCFG_TXVRT, level);
	writel(pcfg1_val, &u20dh_dev->PCFG1);

	dev_info(tcc_dwc_otg->dev, "[INFO][USB] current DC voltage level: %d\n",
			BIT_MSK(pcfg1_val, PCFG_TXVRT));

	return 0;
}
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

static int32_t tcc_dwc_otg_phy_probe(struct platform_device *pdev)
{
	struct device *dev;
	struct tcc_dwc_otg_phy *tcc_dwc_otg;
	int32_t ret;

	if (pdev == NULL)
		return -ENODEV;

	dev = &pdev->dev;

	dev_info(dev, "[INFO][USB] %s() call SUCCESS\n", __func__);

	tcc_dwc_otg = devm_kzalloc(dev, sizeof(*tcc_dwc_otg), GFP_KERNEL);

	ret = tcc_dwc_otg_phy_create(dev, tcc_dwc_otg);
	if (ret != 0) {
		dev_err(dev, "[ERROR][USB] tcc_dwc_otg_phy_create() FAIL!\n");
		return ret;
	}

	if (request_mem_region(pdev->resource[0].start,
				pdev->resource[0].end -
				pdev->resource[0].start + 1,
				"dwc_otg_phy") == NULL) {
		dev_dbg(dev, "[DEBUG][USB] error reserving mapped memory\n");
		ret = -EFAULT;
	}

	tcc_dwc_otg->base_addr = (void __iomem *)ioremap_nocache(
			(resource_size_t)pdev->resource[0].start,
			pdev->resource[0].end - pdev->resource[0].start + 1U);
	tcc_dwc_otg->phy.base = tcc_dwc_otg->base_addr;

	platform_set_drvdata(pdev, tcc_dwc_otg);

	ret = usb_add_phy_dev(&tcc_dwc_otg->phy);
	if (ret != 0) {
		dev_err(dev, "[ERROR][USB] usb_add_phy_dev() FAIL!, ret: %d\n",
				ret);
		return ret;
	}

	return ret;
}

static int32_t tcc_dwc_otg_phy_remove(struct platform_device *pdev)
{
	struct tcc_dwc_otg_phy *tcc_dwc_otg = platform_get_drvdata(pdev);

	usb_remove_phy(&tcc_dwc_otg->phy);

	return 0;
}

static const struct of_device_id tcc_dwc_otg_phy_match[] = {
	{ .compatible = "telechips,tcc_dwc_otg_phy" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_dwc_otg_phy_match);

static struct platform_driver tcc_dwc_otg_phy_driver = {
	.probe			= tcc_dwc_otg_phy_probe,
	.remove			= tcc_dwc_otg_phy_remove,
	.driver = {
		.name		= "dwc_otg_phy",
		.owner		= THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_dwc_otg_phy_match),
	},
};

static int32_t __init tcc_dwc_otg_phy_drv_init(void)
{
	int32_t ret = 0;

	ret = platform_driver_register(&tcc_dwc_otg_phy_driver);
	if (ret < 0)
		pr_err("[ERROR][USB] %s() FAIL! ret: %d\n", __func__, ret);

	return ret;
}
subsys_initcall_sync(tcc_dwc_otg_phy_drv_init);

static void __exit tcc_dwc_otg_phy_drv_cleanup(void)
{
	platform_driver_unregister(&tcc_dwc_otg_phy_driver);
}
module_exit(tcc_dwc_otg_phy_drv_cleanup);

MODULE_DESCRIPTION("Telechips DWC OTG USB transceiver driver");
MODULE_LICENSE("GPL v2");

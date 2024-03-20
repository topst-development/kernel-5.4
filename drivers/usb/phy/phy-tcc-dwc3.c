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
#include "../dwc3/core.h"
#include "../dwc3/io.h"
#include <linux/kthread.h>
#include <soc/tcc/chipinfo.h>

#include "phy-tcc-dwc3.h"

static int32_t tcc_dwc3_create_phy(struct device *dev,
		struct tcc_dwc3_phy *phy_dev)
{
	int32_t ret = 0;

	if ((dev == NULL) || (phy_dev == NULL))
		return -ENODEV;

	phy_dev->phy.otg =
		devm_kzalloc(dev, sizeof(*phy_dev->phy.otg), GFP_KERNEL);
	if (phy_dev->phy.otg == NULL)
		return -ENOMEM;

	phy_dev->dev			= dev;

	phy_dev->phy.dev		= phy_dev->dev;
	phy_dev->phy.label		= "tcc_dwc3_phy";
	phy_dev->phy.type		= USB_PHY_TYPE_USB3;

	phy_dev->phy.init		= tcc_dwc3_phy_init;
	phy_dev->phy.shutdown		= tcc_dwc3_phy_shutdown;
	phy_dev->phy.set_vbus		= tcc_dwc3_phy_set_vbus;
	phy_dev->phy.set_suspend	= tcc_dwc3_phy_set_suspend;

	phy_dev->phy.get_base		= tcc_dwc3_phy_get_base;
	phy_dev->phy.set_vbus_resource	= tcc_dwc3_phy_set_vbus_resource;

#if defined(CONFIG_ENABLE_BC_30_HOST)
	phy_dev->phy.set_chg_det	= tcc_dwc3_phy_set_chg_det;
	phy_dev->phy.stop_chg_det	= tcc_dwc3_phy_stop_chg_det;
#endif /* CONFIG_ENABLE_BC_30_HOST */

	phy_dev->phy.otg->usb_phy	= &phy_dev->phy;

	return ret;
}

static int32_t tcc_dwc3_phy_init(struct usb_phy *phy)
{
	struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(phy, struct tcc_dwc3_phy, phy);
	struct U30_PHY *u30;
	int32_t is_sram_init_done = 0;
	int32_t retry = 0;
	uint32_t tmp = 0;
	uint32_t tmp_data = 0;

	if (tcc_dwc3 == NULL)
		return -ENODEV;

	u30 = (struct U30_PHY *)tcc_dwc3->base_addr;
	if (u30 == NULL)
		return -ENODEV;

	if (is_suspended != 0)
	{
		// PHY Configuration Setting per Protocol
		BIT_SET(u30->PCFG4, PHY_EXT_CTRL_SEL);

		// Controller Software Reset(Reset Controller)
		BIT_CLR(u30->LCFG, VCC_RESET_N);

		// PHY Reset(Release Reset)
		BIT_CLR(u30->PCFG0, PHY_RESET);

		// PHY Power-on Reset(Reset USB2.0 Host PHY)
		BIT_SET(u30->FPCFG0, PHY_POR);

		// Power-on SuperSpeed Circuit
		BIT_CLR(u30->PCFG0, PD_SS);
		usleep_range(1000, 2000);

		// PHY Reset(PHY Set to Reset)
		BIT_SET(u30->PCFG0, PHY_RESET);

		// Waiting for SRAM Initialization Done
		while (is_sram_init_done == 0U)
			is_sram_init_done = BIT_MSK(u30->PCFG0, SRAM_INIT_DONE);

		// SRAM External Load Done
		BIT_SET(u30->PCFG0, SRAM_EXT_LD_DONE);

		// Set TXVRT to 0xC
		writel(0xE31C243C, &u30->FPCFG1);

		// Set External TX VBoost Level to 0x7
		writel(0x31C71457, &u30->PCFG13);

		// Set External TX IBoost Level to 0xA
		writel(0xA4C4302A, &u30->PCFG15);

		// SIDDQ is released
		BIT_CLR(u30->FPCFG0, SIDDQ);

		// PHY Power-on Reset(Normal Function)
		BIT_CLR(u30->FPCFG0, PHY_POR);

		// Controller Software Reset(Controller is in Normal Operation)
		BIT_SET(u30->LCFG, VCC_RESET_N);

		// Waiting for USB 3.0 PHY to operate in SuperSpeed Mode
		while (retry < RETRY_CNT) {
			if (BIT_MSK(u30->PCFG0, PHY_STABLE) != 0U)
				break;

			retry++;
			udelay(5);
		}

		dev_info(tcc_dwc3->dev, "[INFO][USB] Checking PHY validity... %s\n",
				(retry >= RETRY_CNT) ? "FAIL!" : "SUCCESS");
		retry = 0;

		// Initialize LCFG
		BIT_SET(u30->LCFG, (HUB_PORT_PERM_ATTACH | BIT(19) | BIT(18) |
					FLADJ | PPC));

		/* Set 2.0phy REXT */
		if (IS_ENABLED(CONFIG_ARCH_TCC803X) ||
				(get_chip_name() == 0x8059)) {
			do {
				// Read calculated value
				writel(BIT(26)|BIT(25), tcc_dwc3->ref_base);
				tmp = readl(tcc_dwc3->ref_base);
				dev_info(tcc_dwc3->dev, "[INFO][USB] 2.0H status bus = 0x%08x\n",
						tmp);
				tmp_data = 0x0000F000U & tmp;

				tmp_data = tmp_data << 4; // set TESTDATAIN

				//Read Status Bus
				writel(TAD, &u30->FPCFG3);

				//Read Override Bus
				BIT_SET(u30->FPCFG3, TDOSEL);

				//Write Override Bus
				BIT_SET(u30->FPCFG3, (TDI | tmp_data));
				udelay(1);

				BIT_SET(u30->FPCFG3, TCK);
				udelay(1);

				BIT_CLR(u30->FPCFG3, TCK);
				udelay(1);

				//Read Status Bus
				writel(TAD, &u30->FPCFG3);

				//Read Override Bus
				BIT_SET(u30->FPCFG3, TDOSEL);

				dev_info(tcc_dwc3->dev, "[INFO][USB] 2.0 REXT = 0x%08lx\n",
						BIT_MSK(u30->FPCFG3, TDO));

				retry++;
			} while ((BIT_MSK(u30->FPCFG3, TDO) == 0U) &&
					(retry < 5));
		}

#if defined(CONFIG_ENABLE_BC_30_HOST)
		// Clear PHY IRQ
		BIT_SET(u30->FPCFG4, IRQ_CLR);

		// Disable IRQ for PHY Valid
		BIT_CLR(u30->FPCFG4, IRQ_PHYVALIDEN);

		// Clear PHY IRQ
		BIT_CLR(u30->FPCFG4, IRQ_CLR);
		udelay(1);

		/*
		 * Data Source Voltage(VDAT_SRC) is sourced onto DM and sunk
		 * from DP. And Data Detect Voltage(CHG_DET) is enabled.
		 */
		BIT_SET(u30->FPCFG2, (CHRGSEL | VDATDETENB));
		udelay(1);

		// Event for PHY Valid is pending
		BIT_SET(u30->FPCFG4, IRQ_PHYVALID);

		// Enable CHGDET Interrupt
		BIT_CLR(u30->PINT, PINT_MSK_CHGDET);

		// Disable interrupts
		BIT_SET(u30->PINT, (PINT_EN | PINT_MSK_RESERVED | PINT_MSK_PHY |
					PINT_MSK_BC_CHIRP_ON));

		enable_irq(tcc_dwc3->irq);
#endif /* CONFIG_ENABLE_BC_30_HOST */

		// Controller Software Reset(Reset Controller)
		BIT_CLR(u30->LCFG, VCC_RESET_N);

		/*
		 * disable usb20mode -> removed in DWC_usb3 2.60a, but use as
		 * interrupt
		 */
		BIT_CLR(u30->LCFG, BIT(28));

		// Power-on SuperSpeed Circuit
		BIT_CLR(u30->PCFG0, PD_SS);

		// PHY Reset(Release Reset)
		BIT_CLR(u30->PCFG0, PHY_RESET);
		mdelay(1);

		// PHY Reset(PHY Set to Reset)
		BIT_SET(u30->PCFG0, PHY_RESET);

		// Controller Software Reset(Controller is in Normal Operation)
		BIT_SET(u30->LCFG, VCC_RESET_N);
		mdelay(10);

		is_suspended = 0;
	}

	return 0;
}

static void tcc_dwc3_phy_shutdown(struct usb_phy *phy)
{
	struct tcc_dwc3_phy *tcc_dwc3;
	struct U30_PHY *u30;

	if (phy == NULL)
		return;

	tcc_dwc3 = container_of(phy, struct tcc_dwc3_phy, phy);
	if (tcc_dwc3 == NULL)
		return;

	u30 = (struct U30_PHY *)tcc_dwc3->base_addr;
	if (u30 == NULL)
		return;

	if (is_suspended == 0) {
		dev_info(tcc_dwc3->dev, "[INFO][USB] SuperSpeed circuit power down in %s()\n",
				__func__);

		BIT_SET(u30->PCFG0, PD_SS);
		mdelay(10);

		is_suspended = 1;
	}
}

static int32_t tcc_dwc3_phy_set_vbus(struct usb_phy *phy, int32_t on_off)
{
	struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(phy, struct tcc_dwc3_phy, phy);
	struct device *dev;
	struct property *prop;
	int32_t ret = 0;

	if (tcc_dwc3 == NULL)
		return -ENODEV;

	dev = tcc_dwc3->dev;

	/*
	 * Check that the "vbus-ctrl-able" property for the USB PHY driver node
	 * is declared in the device tree.
	 */
	prop = of_find_property(dev->of_node, "vbus-ctrl-able", NULL);
	if (prop == NULL) {
		dev_err(dev, "[ERROR][USB] vbus-ctrl-able property is not declared.\n");
		return -ENODEV;
	}

	if (tcc_dwc3->vbus_supply == NULL) {
		dev_err(dev, "[ERROR][USB] Vbus Supply is not valid.\n");
		return -ENODEV;
	}

	if ((on_off == ON) && (tcc_dwc3->vbus_enabled == false)) {
		ret = regulator_enable(tcc_dwc3->vbus_supply);
		if (ret)
			goto err;

		ret = regulator_set_voltage(tcc_dwc3->vbus_supply,
				ON_VOLTAGE, ON_VOLTAGE);
		if (ret)
			goto err;

		tcc_dwc3->vbus_enabled = true;
	} else if ((on_off == OFF) && (tcc_dwc3->vbus_enabled == true)) {
		ret = regulator_set_voltage(tcc_dwc3->vbus_supply,
				OFF_VOLTAGE, OFF_VOLTAGE);
		if (ret)
			goto err;

		regulator_disable(tcc_dwc3->vbus_supply);
		tcc_dwc3->vbus_enabled = false;
        } else if ((on_off == ON) && (tcc_dwc3->vbus_enabled == true)) {
                dev_info(dev, "[INFO][USB] Vbus is already ENABLED!\n");
        } else if ((on_off == OFF) && (tcc_dwc3->vbus_enabled == false)) {
                dev_info(dev, "[INFO][USB] Vbus is already DISABLED!\n");
	}
err:
	if (ret) {
		dev_err(dev, "[ERROR][USB] Vbus Supply is not controlled.\n");
		regulator_disable(tcc_dwc3->vbus_supply);
	} else {
		usleep_range(3000, 5000);
	}

	return ret;
}

static int32_t tcc_dwc3_phy_set_suspend(struct usb_phy *phy, int32_t suspend)
{
	struct tcc_dwc3_phy *tcc_dwc3;
	struct U30_PHY *u30;

	if (phy == NULL)
		return -ENODEV;

	tcc_dwc3 = container_of(phy, struct tcc_dwc3_phy, phy);
	if (tcc_dwc3 == NULL)
		return -ENODEV;

	u30 = (struct U30_PHY *)tcc_dwc3->base_addr;
	if (u30 == NULL)
		return -ENODEV;

	if (suspend == 0) {
		if (is_suspended == 1) {
			dev_info(tcc_dwc3->dev, "[INFO][USB] SuperSpeed circuit power on\n");

			BIT_CLR(u30->PCFG0, ~PD_SS);
			is_suspended = 0;
		}
	} else {
		if (is_suspended == 0) {
			dev_info(tcc_dwc3->dev, "[INFO][USB] SuperSpeed circuit power down in %s()\n",
					__func__);

			BIT_SET(u30->PCFG0, PD_SS);
			mdelay(10);

			is_suspended = 1;
		}
	}

	return 0;
}

static void __iomem *tcc_dwc3_phy_get_base(struct usb_phy *phy)
{
	struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(phy, struct tcc_dwc3_phy, phy);

	if (tcc_dwc3 != NULL)
		return tcc_dwc3->base_addr;
	else
		return (void __iomem *)0;
}

static int32_t tcc_dwc3_phy_set_vbus_resource(struct usb_phy *phy)
{
	struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(phy, struct tcc_dwc3_phy, phy);
	struct device *dev;

	if (tcc_dwc3 == NULL)
		return -ENODEV;

	dev = tcc_dwc3->dev;

	/*
	 * Check that the "vbus-ctrl-able" property for the USB PHY driver node
	 * is declared in the device tree.
	 */
	if (of_find_property(dev->of_node, "vbus-ctrl-able", 0) != NULL) {
		/*
		 * Get the vbus regulator declared in the "vbus-supply" property
		 * for the USB PHY driver node.
		 */
		if (tcc_dwc3->vbus_supply == NULL) {
			tcc_dwc3->vbus_supply =
				devm_regulator_get_optional(dev, "vbus");
			if (IS_ERR(tcc_dwc3->vbus_supply)) {
				dev_err(dev, "[ERROR][USB] VBus Supply is not valid.\n");
				return -ENODEV;
			}
		}
	} else {
		dev_info(dev, "[INFO][USB] vbus-ctrl-able property is not declared.\n");
	}

	return 0;
}

#if defined(CONFIG_ENABLE_BC_30_HOST)
static irqreturn_t tcc_dwc3_phy_chg_det_irq(int32_t irq, void *data)
{
	struct tcc_dwc3_phy *tcc_dwc3 = (struct tcc_dwc3_phy *)data;
	struct U30_PHY *u30 = (struct U30_PHY *)tcc_dwc3->base_addr;

	dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] %s : CHGDET\n", __func__);

	// clear irq
	BIT_SET(u30->PINT, PINT_STS);
	udelay(1);

	// clear irq
	BIT_CLR(u30->PINT, PINT_STS);

	schedule_work(&tcc_dwc3->dwc3_work);

	return IRQ_HANDLED;
}

static void tcc_dwc3_phy_chg_det_monitor(struct work_struct *data)
{
	struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(data, struct tcc_dwc3_phy, dwc3_work);
	struct U30_PHY *u30 = (struct U30_PHY *)tcc_dwc3->base_addr;
	int32_t count = 3;
	int32_t timeout_count = 500;

	tcc_dwc3->chg_ready = true;

	while (count > 0) {
		if (BIT_MSK(u30->FPCFG2, CHGDET) != 0)
			break;

		usleep_range(1000, 1100);
		count--;
	}

	if (count == 0) {
		dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Charge Detection FAIL!\n");
	} else {
		BIT_SET(u30->FPCFG2, VDATSRCENB);

		while (timeout_count > 0) {
			if (BIT_MSK(u30->FPCFG2, CHGDET) != 0) {
				usleep_range(1000, 1100);
				timeout_count--;
			} else {
				break;
			}
		}

		if (timeout_count == 0)
			dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Timeout - VDM_SRC on\n");

		// Data Source Voltage & Data Detect Voltage is disabled
		BIT_CLR(u30->FPCFG2, (VDATSRCENB | VDATDETENB));

		if (tcc_dwc3->chg_ready == true) {
			dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Enable chg det monitor!\n");

			if (tcc_dwc3->dwc3_chgdet_thread != NULL) {
				kthread_stop(tcc_dwc3->dwc3_chgdet_thread);
				tcc_dwc3->dwc3_chgdet_thread = NULL;
			}

			dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] start chg det thread!\n");

			tcc_dwc3->dwc3_chgdet_thread =
				kthread_run(tcc_dwc3_phy_chg_det_thread,
						(void *)tcc_dwc3, "dwc3-chgdet");
			if (IS_ERR(tcc_dwc3->dwc3_chgdet_thread))
				dev_err(tcc_dwc3->dev, "[ERROR][USB] failed to run tcc_dwc3_phy_chg_det_thread\n");
		} else {
			dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] No need to start chg det monitor!\n");
		}
	}
}

static int tcc_dwc3_phy_chg_det_thread(void *work)
{
	struct tcc_dwc3_phy *tcc_dwc3 = (struct tcc_dwc3_phy *) work;
	struct U30_PHY *u30 = (struct U30_PHY *)tcc_dwc3->base_addr;
	int32_t timeout = 500;

	dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Start to check CHGDET\n");

	while (!kthread_should_stop() && timeout > 0) {
		usleep_range(1000, 1100);
		timeout--;
	}

	if (timeout <= 0) {
		BIT_SET(u30->FPCFG2, VDATDETENB);
		dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Data Detect Voltage is enabled\n");
	}

	tcc_dwc3->dwc3_chgdet_thread = NULL;

	dev_info(tcc_dwc3->dev, "[INFO][USB] Monitoring is finished(%d)\n", timeout);

	return 0;
}

static void tcc_dwc3_phy_set_chg_det(struct usb_phy *phy)
{
	struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(phy, struct tcc_dwc3_phy, phy);
	struct U30_PHY *u30 = (struct U30_PHY *)tcc_dwc3->base_addr;

	dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] %s() call SUCCESS\n", __func__);

	// Data Detect Voltage(CHG_DET) is enabled
	BIT_SET(u30->FPCFG2, VDATDETENB);
}

static void tcc_dwc3_phy_stop_chg_det(struct usb_phy *phy)
{
	struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(phy, struct tcc_dwc3_phy, phy);
	struct U30_PHY *u30 = (struct U30_PHY *)tcc_dwc3->base_addr;

	dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] %s() call SUCCESS\n", __func__);

	tcc_dwc3->chg_ready = false;

	if (tcc_dwc3->dwc3_chgdet_thread != NULL) {
		dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Kill Charging Detection Thread\n");

		kthread_stop(tcc_dwc3->dwc3_chgdet_thread);
		tcc_dwc3->dwc3_chgdet_thread = NULL;
	}

	dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Before\nU30_FPCFG2: 0x%x / U30_FPCFG4: 0x%x\n",
			readl(&u30->FPCFG2), readl(&u30->FPCFG4));

	// Data Source Voltage & Data Detect Voltage is disabled
	BIT_CLR(u30->FPCFG2, (VDATSRCENB | VDATDETENB));

	dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Charging Detection is disabled\n");
	dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] After\nU30_FPCFG2: 0x%x / U30_FPCFG4: 0x%x\n",
			readl(&u30->FPCFG2), readl(&u30->FPCFG4));
}
#endif

static int32_t tcc_dwc3_phy_probe(struct platform_device *pdev)
{
	struct device *dev;
	struct tcc_dwc3_phy *phy_dev;
	int32_t ret;

#if defined(CONFIG_ENABLE_BC_30_HOST)
	int32_t irq = 0;
#endif /* CONFIG_ENABLE_BC_30_HOST */

	if (pdev == NULL)
		return -ENODEV;

	dev = &pdev->dev;

	dev_info(dev, "[INFO][USB] %s() call SUCCESS\n", __func__);

	phy_dev = devm_kzalloc(dev, sizeof(*phy_dev), GFP_KERNEL);

	ret = tcc_dwc3_create_phy(dev, phy_dev);
	if (ret != 0) {
		dev_err(dev, "[ERROR][USB] tcc_dwc3_phy_create() FAIL!\n");
		return ret;
	}

#if defined(CONFIG_ENABLE_BC_30_HOST)
	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(dev, "[ERROR][USB] Found HC with no IRQ. Check %s setup!\n",
				dev_name(dev));
		ret = -ENODEV;
	} else {
		dev_info(dev, "[INFO][USB] platform_get_irq() SUCCESS, irq: %d\n",
				irq);
		phy_dev->irq = irq;
	}
#endif /* CONFIG_ENABLE_BC_30_HOST */

	if (request_mem_region(pdev->resource[0].start,
				pdev->resource[0].end -
				pdev->resource[0].start + 1,
				"dwc3_phy") == NULL) {
		dev_dbg(dev, "[DEBUG][USB] error reserving mapped memory\n");
		ret = -EFAULT;
	}

	phy_dev->base_addr = (void __iomem *)ioremap_nocache(
			(resource_size_t)pdev->resource[0].start,
			pdev->resource[0].end - pdev->resource[0].start + 1U);
	phy_dev->phy.base = phy_dev->base_addr;

#if defined(CONFIG_ENABLE_BC_30_HOST)
	ret = devm_request_irq(dev, phy_dev->irq, tcc_dwc3_phy_chg_det_irq,
			IRQF_SHARED, pdev->dev.kobj.name, phy_dev);
	if (ret) {
		dev_err(dev, "[ERROR][USB] devm_request_irq() FAIL!, ret: %d\n",
				ret);
	}

	disable_irq(phy_dev->irq);

	INIT_WORK(&phy_dev->dwc3_work, tcc_dwc3_phy_chg_det_monitor);
#endif /* CONFIG_ENABLE_BC_30_HOST */

	phy_dev->ref_base = (void __iomem *)ioremap_nocache(
			(resource_size_t)pdev->resource[1].start,
			pdev->resource[1].end - pdev->resource[1].start + 1U);
	phy_dev->phy.ref_base = phy_dev->ref_base;

	platform_set_drvdata(pdev, phy_dev);

	ret = usb_add_phy_dev(&phy_dev->phy);
	if (ret != 0) {
		dev_err(dev, "[ERROR][USB] usb_add_phy_dev() FAIL!, ret: %d\n",
				ret);
		return ret;
	}

	return ret;
}

static int32_t tcc_dwc3_phy_remove(struct platform_device *pdev)
{
	struct tcc_dwc3_phy *phy_dev;

	if (pdev == NULL)
		return -ENODEV;

	phy_dev = platform_get_drvdata(pdev);

	usb_remove_phy(&phy_dev->phy);

	release_mem_region(pdev->resource[0].start, // dwc3 base
			pdev->resource[0].end - pdev->resource[0].start + 1U);
	release_mem_region(pdev->resource[1].start, // dwc3 phy base
			pdev->resource[1].end - pdev->resource[1].start + 1U);

	return 0;
}

static const struct of_device_id tcc_dwc3_phy_match[] = {
	{ .compatible = "telechips,tcc_dwc3_phy" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_dwc3_phy_match);

static struct platform_driver tcc_dwc3_phy_driver = {
	.probe			= tcc_dwc3_phy_probe,
	.remove			= tcc_dwc3_phy_remove,
	.driver = {
		.name		= "dwc3_phy",
		.owner		= THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_dwc3_phy_match),
	},
};

static int32_t __init tcc_dwc3_phy_drv_init(void)
{
	int32_t ret = 0;

	ret = platform_driver_register(&tcc_dwc3_phy_driver);
	if (ret < 0)
		pr_err("[ERROR][USB] %s() FAIL! ret: %d\n", __func__, ret);

	return ret;
}
subsys_initcall_sync(tcc_dwc3_phy_drv_init);

static void __exit tcc_dwc3_phy_drv_cleanup(void)
{
	platform_driver_unregister(&tcc_dwc3_phy_driver);
}
module_exit(tcc_dwc3_phy_drv_cleanup);

MODULE_DESCRIPTION("Telechips DWC3 USB transceiver driver");
MODULE_LICENSE("GPL v2");

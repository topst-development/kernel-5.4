// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/otg.h>

#include "ehci.h"
#include "ehci-tcc.h"

#if defined(CONFIG_VBUS_CTRL_DEF_ENABLE)
static uint32_t vbus_control_enable = ENABLE;
#else /* CONFIG_VBUS_CTRL_DEF_ENABLE */
static uint32_t vbus_control_enable = DISABLE;
#endif /* !defined(CONFIG_VBUS_CTRL_DEF_ENABLE) */
module_param(vbus_control_enable, uint, 0644);
MODULE_PARM_DESC(vbus_control_enable, "TCC EHCI VBus control enable");

static ssize_t vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);

	return sprintf(buf, "TCC EHCI vbus: %s\n",
			(tcc_ehci->vbus_status == ON) ? "on" : "off");
}

static ssize_t vbus_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);

	if (strncmp(buf, "on", 2) == 0)
		tcc_ehci_ctrl_vbus(tcc_ehci, ON);
	else if (strncmp(buf, "off", 3) == 0)
		tcc_ehci_ctrl_vbus(tcc_ehci, OFF);

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(vbus);

static ssize_t testmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_ehci_hcd *tcc_ehci =	dev_get_drvdata(dev);
	struct ehci_hcd *ehci = tcc_ehci->ehci;
	u32 reg;

	if (ehci == NULL) {
		pr_info("[INFO][USB] TCC EHCI testmode show failed!\n");
		pr_info("[INFO][USB] No TCC EHCI HCD\n");

		return 0;
	}

	reg = ehci_readl(ehci, &ehci->regs->port_status[0]);
	reg &= (0xf << 16); // EHCI_PORTPMSC_TESTMODE_MASK
	reg >>= 16;

	switch (reg) {
	case 0:
		pr_info("no test\n");
		break;
	case TEST_J:
		pr_info("test_j\n");
		break;
	case TEST_K:
		pr_info("test_k\n");
		break;
	case TEST_SE0_NAK:
		pr_info("test_se0_nak\n");
		break;
	case TEST_PACKET:
		pr_info("test_packet\n");
		break;
	case TEST_FORCE_EN:
		pr_info("test_force_enable\n");
		break;
	default:
		pr_info("UNKNOWN test mode\n");
	}

	return 0;
}

static ssize_t testmode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_ehci_hcd *tcc_ehci =	dev_get_drvdata(dev);
	struct ehci_hcd *ehci = tcc_ehci->ehci;
	u32 testmode = 0;

	if (!strncmp(buf, "test_j", 6))
		testmode = TEST_J;
	else if (!strncmp(buf, "test_k", 6))
		testmode = TEST_K;
	else if (!strncmp(buf, "test_se0_nak", 12))
		testmode = TEST_SE0_NAK;
	else if (!strncmp(buf, "test_packet", 11))
		testmode = TEST_PACKET;
	else if (!strncmp(buf, "test_force_enable", 17))
		testmode = TEST_FORCE_EN;
	else
		testmode = 0;

	ehci_set_test_mode(ehci, testmode);

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(testmode);

static char *tcc_ehci_display_pcfg(uint32_t old_reg, uint32_t new_reg,
		char *str)
{
	ulong old_val, new_val;
	int32_t i;

	for (i = 0; i < (int32_t)PCFG_MAX; i++) {
		old_val = (BIT_MSK(old_reg, USB_PCFG[i].mask)) >>
			(USB_PCFG[i].offset);
		new_val = (BIT_MSK(new_reg, USB_PCFG[i].mask)) >>
			(USB_PCFG[i].offset);

		if (old_val != new_val) {
			sprintf(USB_PCFG[i].str, "%s: 0x%lX -> 0x%lX\n",
					USB_PCFG[i].reg_name, old_val, new_val);
		} else {
			sprintf(USB_PCFG[i].str, "%s: 0x%lX\n",
					USB_PCFG[i].reg_name, old_val);
		}

		strncat(str, USB_PCFG[i].str, 256 - strlen(str) - 1);
	}

	return str;
}

static ssize_t ehci_pcfg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);
	uint32_t pcfg1_val = readl(tcc_ehci->phy_regs + PHY_CFG1);
	char str[256] = {0};

	return sprintf(buf, "USB20H_PCFG1: 0x%08X\n%s", pcfg1_val,
			tcc_ehci_display_pcfg(pcfg1_val, pcfg1_val, str));
}

static ssize_t ehci_pcfg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);
	int32_t i;
	int ret;
	uint32_t old_reg = readl(tcc_ehci->phy_regs + PHY_CFG1);
	uint32_t new_reg;
	char str[256] = {0};

	if (buf == NULL)
		return -ENXIO;

	if ((buf[0] != '0') || (buf[1] != 'x')) {
		pr_info("[INFO][USB]\n\techo %c%cXXXXXXXX is not 0x\n\n",
				buf[0], buf[1]);
		pr_info("\tUsage : echo 0xXXXXXXXX > ehci_pcfg\n\n");
		pr_info("\t\t1) 0 is binary number)\n\n");

		return (ssize_t)count;
	}

	for (i = 2; i < 10; i++) {
		if (((buf[i] >= '0') && (buf[i] <= '9')) ||
				((buf[i] >= 'a') && (buf[i] <= 'f')) ||
				((buf[i] >= 'A') && (buf[i] <= 'F'))) {
			continue;
		} else {
			pr_info("[INFO][USB]\necho 0x%c%c%c%c%c%c%c%c is not hex\n\n",
					buf[2], buf[3], buf[4], buf[5],
					buf[6], buf[7], buf[8], buf[9]);
			pr_info("\tUsage : echo 0xXXXXXXXX > ehci_pcfg\n\n");
			pr_info("\t\t2) X is hex number(0 to f)\n\n");

			return (ssize_t)count;
		}
	}

	if (((count - 1U) < 10U) || (10U < (count - 1U))) {
		pr_info("[INFO][USB]\nThis argument length is not 10\n\n");
		pr_info("\tUsage : echo 0xXXXXXXXX > ehci_pcfg\n\n");
		pr_info("\t\t1) length of 0xXXXXXXXX is 10\n");
		pr_info("\t\t2) X is hex number(0 to f)\n\n");

		return (ssize_t)count;
	}

	ret = kstrtoul(buf, 0, (unsigned long *)&new_reg);
	if (ret)
		return -EINVAL;

	pr_info("[INFO][USB] Before\nUSB20H_PCFG1: 0x%08X\n", old_reg);
	writel(new_reg, tcc_ehci->phy_regs + PHY_CFG1);
	new_reg = readl(tcc_ehci->phy_regs + PHY_CFG1);

	tcc_ehci_display_pcfg(old_reg, new_reg, str);
	pr_info("\n[INFO][USB] After\n%sUSB20H_PCFG1: 0x%08X\n", str, new_reg);

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(ehci_pcfg);

static int32_t tcc_ehci_parse_dt(struct platform_device *pdev,
		struct tcc_ehci_hcd *tcc_ehci)
{
	int32_t ret = 0;

	// Check vbus enable pin
	if (of_find_property(pdev->dev.of_node, "telechips,ehci_phy", NULL) !=
			NULL) {
		tcc_ehci->phy = devm_usb_get_phy_by_phandle(&pdev->dev,
				"telechips,ehci_phy", 0);

		if (!IS_ENABLED(CONFIG_ARCH_TCC897X)) {
			ret = tcc_ehci->phy->set_vbus_resource(tcc_ehci->phy);
			if (ret != 0)
				return ret;
		}

		if (IS_ERR(tcc_ehci->phy)) {
			if (PTR_ERR(tcc_ehci->phy) == -EPROBE_DEFER)
				ret = -EPROBE_DEFER;
			else
				ret = -ENODEV;

			tcc_ehci->phy = NULL;

			return ret;
		}

		tcc_ehci->phy_regs = tcc_ehci->phy->base;
	}

	// Check TPL Support
	if (of_usb_host_tpl_support(pdev->dev.of_node))
		tcc_ehci->hcd_tpl_support = ENABLE;
	else
		tcc_ehci->hcd_tpl_support = DISABLE;

	return ret;
}

static int32_t tcc_ehci_ctrl_phy(struct tcc_ehci_hcd *tcc_ehci, int32_t on_off)
{
	struct usb_phy *phy = tcc_ehci->phy;
	int32_t ret = 0;

	if ((phy == NULL) || (phy->set_state == NULL)) {
		pr_info("[INFO][USB] TCC EHCI PHY control failed!\n");
		pr_info("[INFO][USB] No TCC EHCI PHY driver or No set_state function\n");
		ret = -ENODEV;
	} else {
		ehci_phy_set = 1;
		ret = phy->set_state(phy, on_off);
	}

	return ret;
}

static int32_t tcc_ehci_init_phy(struct tcc_ehci_hcd *tcc_ehci)
{
	struct usb_phy *phy = tcc_ehci->phy;
	int32_t ret = 0;

	if ((phy == NULL) || (phy->init == NULL)) {
		pr_info("[INFO][USB] TCC EHCI PHY init failed!\n");
		pr_info("[INFO][USB] No TCC EHCI PHY driver or No init function\n");
		ret = -ENODEV;
	} else {
		ret = phy->init(phy);
	}

	return ret;
}

static int32_t tcc_ehci_ctrl_vbus(struct tcc_ehci_hcd *tcc_ehci, int32_t on_off)
{
	struct usb_phy *phy = tcc_ehci->phy;
	int32_t ret = 0;

	if ((phy == NULL) || (phy->set_vbus == NULL)) {
		dev_err(tcc_ehci->dev, "[ERROR][USB] PHY driver does NOT exist!\n");
		ret = -ENODEV;
	} else {
		if (vbus_control_enable == DISABLE) {
			dev_info(tcc_ehci->dev, "[INFO][USB] VBus control is DISABLED!\n");
			ret = -ENOENT;
		} else {
			tcc_ehci->vbus_status = on_off;
			ret = phy->set_vbus(phy, on_off);
		}
	}

	return ret;
}

static int32_t tcc_ehci_probe(struct platform_device *pdev)
{
	struct tcc_ehci_hcd *tcc_ehci;
	struct usb_hcd *hcd;
	struct resource *res;

	int32_t ret = 0;
	int32_t irq;

	if (usb_disabled() != 0)
		return -ENODEV;

	ret = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret != 0)
		goto err0;

	tcc_ehci = devm_kzalloc(&pdev->dev, sizeof(struct tcc_ehci_hcd),
			GFP_KERNEL);
	if (!tcc_ehci) {
		ret = -ENOMEM;
		goto err0;
	}

	/* Parsing the device table */
	ret = tcc_ehci_parse_dt(pdev, tcc_ehci);
	if (ret != 0) {
		/*
		 * If the return value of tcc_ehci_parse_dt() is -EPROBE_DEFER,
		 * the VBus GPIO number is set to an invalid number in
		 * set_vbus_resource() called in tcc_ehci_parse_dt(). In such a
		 * case, set the value of the ehci_phy_set variable to
		 * -EPROBE_DEFER and tcc_ohci_probe() in ohci-tcc.c check the
		 * value of the ehci_phy_set variable. If the value of the
		 * ehci_phy_set variable is -EPROBE_DEFER, the tcc_ohci_probe()
		 * also fails.
		 */
		if (ret == -EPROBE_DEFER) {
			ehci_phy_set = -EPROBE_DEFER;
		} else {
			dev_err(&pdev->dev, "[ERROR][USB] Device table parsing failed.\n");
			ret = -EIO;
		}

		goto err0;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(&pdev->dev, "[ERROR][USB] Found HC with no IRQ. Check %s setup!\n",
				dev_name(&pdev->dev));
		ret = -ENODEV;
		goto err0;
	}

	hcd = usb_create_hcd(&tcc_ehci_hc_driver, &pdev->dev,
			dev_name(&pdev->dev));
	if (!hcd) {
		ret = -ENOMEM;
		goto err0;
	}

	platform_set_drvdata(pdev, tcc_ehci);
	tcc_ehci->dev = &(pdev->dev);

	/* USB ECHI Base Address*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "[ERROR][USB] Found HC with no register addr. Check %s setup!\n",
				dev_name(&pdev->dev));
		ret = -ENODEV;
		goto err1;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	hcd->regs = devm_ioremap(&pdev->dev, res->start, hcd->rsrc_len);

	/* USB HS Phy Enable */
	tcc_ehci_ctrl_phy(tcc_ehci, ON);
	tcc_ehci_init_phy(tcc_ehci);
	tcc_ehci_ctrl_vbus(tcc_ehci, ON);

	/* ehci setup */
	tcc_ehci->ehci = hcd_to_ehci(hcd);

	/* registers start at offset 0x0 */
	tcc_ehci->ehci->caps = hcd->regs;
	tcc_ehci->ehci->regs = hcd->regs + HC_LENGTH(tcc_ehci->ehci,
			ehci_readl(tcc_ehci->ehci,
				&tcc_ehci->ehci->caps->hc_capbase));

	/* cache this readonly data; minimize chip reads */
	tcc_ehci->ehci->hcs_params = ehci_readl(tcc_ehci->ehci,
			&tcc_ehci->ehci->caps->hcs_params);

	/* connect the hcd phy pointer */
	hcd->usb_phy = tcc_ehci->phy;

	/* TPL Support Set */
	hcd->tpl_support = tcc_ehci->hcd_tpl_support;

	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][USB] Failed to add USB HCD\n");
		goto err1;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_vbus);
	if (ret < 0) {
		pr_err("[ERROR][USB] Cannot create vbus sysfs : %d\n", ret);
		goto err1;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_testmode);
	if (ret < 0) {
		pr_err("[ERROR][USB] Cannot create SQ testmode sysfs : %d\n",
				ret);
		goto err1;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_ehci_pcfg);
	if (ret < 0) {
		pr_err("[ERROR][USB] Cannot create SQ ehci_pcfg sysfs : %d\n",
				ret);
		goto err1;
	}

	return ret;

err1:
	usb_put_hcd(hcd);
err0:
	dev_err(&pdev->dev, "[ERROR][USB] init %s fail, %d\n",
			dev_name(&pdev->dev), ret);

	return ret;
}

static int32_t tcc_ehci_remove(struct platform_device *pdev)
{
	struct tcc_ehci_hcd *tcc_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	device_remove_file(&pdev->dev, &dev_attr_vbus);
	device_remove_file(&pdev->dev, &dev_attr_testmode);
	device_remove_file(&pdev->dev, &dev_attr_ehci_pcfg);

	ehci_shutdown(hcd);
	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);

	tcc_ehci_ctrl_phy(tcc_ehci, OFF);
	tcc_ehci_ctrl_vbus(tcc_ehci, OFF);

	return 0;
}

static void tcc_ehci_hcd_shutdown(struct platform_device *pdev)
{
	struct tcc_ehci_hcd *tcc_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	tcc_ehci_ctrl_vbus(tcc_ehci, OFF);

	if (hcd == NULL)
		return;

	if (hcd->driver->shutdown != NULL)
		hcd->driver->shutdown(hcd);
}

static int32_t tcc_ehci_suspend(struct device *dev)
{
	struct tcc_ehci_hcd *tcc_ehci =	dev_get_drvdata(dev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);
	bool do_wakeup = device_may_wakeup(dev);

	ehci_suspend(hcd, do_wakeup);

	/* Telechips specific routine */
	tcc_ehci_ctrl_phy(tcc_ehci, OFF);
	tcc_ehci_ctrl_vbus(tcc_ehci, OFF);

	return 0;
}

static int32_t tcc_ehci_resume(struct device *dev)
{
	struct tcc_ehci_hcd *tcc_ehci =	dev_get_drvdata(dev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	/* Telechips specific routine */
	tcc_ehci_ctrl_phy(tcc_ehci, ON);
	tcc_ehci_ctrl_vbus(tcc_ehci, ON);
	tcc_ehci_init_phy(tcc_ehci);

	/*
	 * for compatibility issue(suspend/resume). Some USB devices are failed
	 * to connect when resume.
	 */
	usleep_range(1000, 2000);
	ehci_resume(hcd, false);

	return 0;
}

static SIMPLE_DEV_PM_OPS(tcc_ehci_pm_ops, tcc_ehci_suspend, tcc_ehci_resume);

static const struct of_device_id tcc_ehci_match[] = {
	{ .compatible = "telechips,tcc-ehci" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_ehci_match);

static struct platform_driver tcc_ehci_driver = {
	.probe			= tcc_ehci_probe,
	.remove			= tcc_ehci_remove,
	.shutdown		= tcc_ehci_hcd_shutdown,
	.driver = {
		.name		= "tcc-ehci",
		.pm		= &tcc_ehci_pm_ops,
		.of_match_table = of_match_ptr(tcc_ehci_match),
	}
};

static int32_t __init tcc_ehci_init(void)
{
	int32_t ret = 0;

	ehci_init_driver(&tcc_ehci_hc_driver, NULL);
	set_bit(USB_EHCI_LOADED, &usb_hcds_loaded);

	ret = platform_driver_register(&tcc_ehci_driver);
	if (ret < 0)
		clear_bit(USB_EHCI_LOADED, &usb_hcds_loaded);

	return ret;
}
module_init(tcc_ehci_init);

static void __exit tcc_ehci_cleanup(void)
{
	platform_driver_unregister(&tcc_ehci_driver);
	clear_bit(USB_EHCI_LOADED, &usb_hcds_loaded);
}
module_exit(tcc_ehci_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

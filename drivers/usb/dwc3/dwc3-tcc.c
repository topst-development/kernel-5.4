// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/cpufreq.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/usb/usb_phy_generic.h>

#include "dwc3-tcc.h"

#if defined(CONFIG_VBUS_CTRL_DEF_ENABLE)
static uint32_t vbus_control_enable = ENABLE;
#else /* CONFIG_VBUS_CTRL_DEF_ENABLE */
static uint32_t vbus_control_enable = DISABLE;
#endif /* !defined(CONFIG_VBUS_CTRL_DEF_ENABLE) */
module_param(vbus_control_enable, uint, 0644);
MODULE_PARM_DESC(vbus_control_enable, "TCC DWC3 VBus control enable");

static void dwc3_tcc_vbus_ctrl(struct dwc3_tcc_hcd *dwc3_tcc, int32_t on_off)
{
	struct usb_phy *phy;

	if (dwc3_tcc == NULL)
		return;

	phy = dwc3_tcc->dwc3_phy;

	if (vbus_control_enable == DISABLE) {
		dev_info(dwc3_tcc->dev, "[INFO][USB] DWC3 VBus control is DISABLED!\n");
		return;
	}

	if (phy == NULL) {
		dev_info(dwc3_tcc->dev, "[INFO][USB] DWC3 PHY driver does NOT exist!\n");
		return;
	}

	phy->vbus_status = on_off;
	phy->set_vbus(phy, on_off);
}

static ssize_t vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc_hcd *dwc3_tcc =
		platform_get_drvdata(to_platform_device(dev));
	struct usb_phy *phy = dwc3_tcc->dwc3_phy;

	return sprintf(buf, "TCC DWC3 vbus: %s\n",
			(phy->vbus_status == ON) ? "on" : "off");
}

static ssize_t vbus_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc3_tcc_hcd *dwc3_tcc =
		platform_get_drvdata(to_platform_device(dev));

	if (strncmp(buf, "on", 2) == 0)
		dwc3_tcc_vbus_ctrl(dwc3_tcc, ON);
	else if (strncmp(buf, "off", 3) == 0)
		dwc3_tcc_vbus_ctrl(dwc3_tcc, OFF);

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(vbus);

static char *dwc3_pcfg_display(uint32_t old_reg, uint32_t new_reg, char *str)
{
	ulong old_val, new_val;
	int32_t i;

	for (i = 0; i < (int32_t)PCFG_MAX; i++) {
		old_val = (BIT_MSK(old_reg, U30_FPCFG1[i].mask)) >>
			(U30_FPCFG1[i].offset);
		new_val = (BIT_MSK(new_reg, U30_FPCFG1[i].mask)) >>
			(U30_FPCFG1[i].offset);

		if (old_val != new_val) {
			sprintf(U30_FPCFG1[i].str, "%s = 0x%lX -> 0x%lX\n",
					U30_FPCFG1[i].reg_name, old_val,
					new_val);
		} else {
			sprintf(U30_FPCFG1[i].str, "%s = 0x%lX\n",
					U30_FPCFG1[i].reg_name, old_val);
		}

		strncat(str, U30_FPCFG1[i].str, 256 - strlen(str) - 1);
	}

	return str;
}

// Show the current value of the USB30 PHY Configuration Register (FPCFG1)
static ssize_t dwc3_pcfg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc3_tcc_hcd *dwc3_tcc =
		platform_get_drvdata(to_platform_device(dev));
	struct U30_PHY *pUSBPHYCFG =
		(struct U30_PHY *)dwc3_tcc->dwc3_phy->get_base(dwc3_tcc->dwc3_phy);
	uint32_t reg = readl(&pUSBPHYCFG->FPCFG1);
	char str[256] = {0};

	return sprintf(buf, "U30_FPCFG1 = 0x%08X\n%s", reg,
			dwc3_pcfg_display(reg, reg, str));
}

// HS DC Voltage Level is set
static ssize_t dwc3_pcfg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc3_tcc_hcd *dwc3_tcc =
		platform_get_drvdata(to_platform_device(dev));
	struct U30_PHY *pUSBPHYCFG =
		(struct U30_PHY *)dwc3_tcc->dwc3_phy->get_base(dwc3_tcc->dwc3_phy);
	int32_t i;
	int ret;
	uint32_t old_reg = readl(&pUSBPHYCFG->FPCFG1);
	uint32_t new_reg;
	char str[256] = {0};

	if (buf == NULL)
		return -ENXIO;

	if ((buf[0] != '0') || (buf[1] != 'x')) {
		pr_info("[INFO][USB]\n\techo %c%cXXXXXXXX is not 0x\n\n",
				buf[0], buf[1]);
		pr_info("\tUsage : echo 0xXXXXXXXX > dwc3_pcfg\n\n");
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
			pr_info("\tUsage : echo 0xXXXXXXXX > dwc3_pcfg\n\n");
			pr_info("\t\t2) X is hex number(0 to f)\n\n");
			return (ssize_t)count;
		}
	}

	if (((count - 1U) < 10U) || (10U < (count - 1U))) {
		pr_info("[INFO][USB]\nThis argument length is not 10\n\n");
		pr_info("\tUsage : echo 0xXXXXXXXX > dwc3_pcfg\n\n");
		pr_info("\t\t1) length of 0xXXXXXXXX is 10\n");
		pr_info("\t\t2) X is hex number(0 to f)\n\n");
		return (ssize_t)count;
	}

	ret = kstrtoul(buf, 0, (unsigned long *)&new_reg);
	if (ret)
		return -EINVAL;

	pr_info("[INFO][USB] Before\nU30_FPCFG1 = 0x%08X\n", old_reg);
	writel(new_reg, &pUSBPHYCFG->FPCFG1);
	new_reg = readl(&pUSBPHYCFG->FPCFG1);

	dwc3_pcfg_display(old_reg, new_reg, str);
	pr_info("\n[INFO][USB] After\n%sU30_FPCFG1 = 0x%08X\n", str, new_reg);

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(dwc3_pcfg);

static int32_t dwc3_tcc_remove_child(struct device *dev, void *unused)
{
	struct platform_device *pdev = to_platform_device(dev);

	platform_device_unregister(pdev);

	return 0;
}

static int32_t dwc3_tcc_phy_register(struct dwc3_tcc_hcd *dwc3_tcc)
{
	struct property *prop;
	int32_t ret;

	prop = of_find_property(dwc3_tcc->dev->of_node, "telechips,dwc3_phy",
			NULL);
	if (prop != NULL) {
		dwc3_tcc->dwc3_phy = devm_usb_get_phy_by_phandle(dwc3_tcc->dev,
				"telechips,dwc3_phy", 0);

		if (IS_ERR(dwc3_tcc->dwc3_phy)) {
			dwc3_tcc->dwc3_phy = NULL;
			pr_info("[INFO][USB] [%s:%d]PHY driver is needed\n",
					__func__, __LINE__);
			return -1;
		}
	}

	if (dwc3_tcc->dwc3_phy == NULL)
		return -ENODEV;

	ret = dwc3_tcc->dwc3_phy->set_vbus_resource(dwc3_tcc->dwc3_phy);
	if (ret != 0)
		return ret;

	return dwc3_tcc->dwc3_phy->init(dwc3_tcc->dwc3_phy);
}

static void dwc3_tcc_phy_unregister(struct dwc3_tcc_hcd *dwc3_tcc)
{
	if (dwc3_tcc == NULL)
		return;

	usb_phy_shutdown(dwc3_tcc->dwc3_phy);
	usb_phy_set_suspend(dwc3_tcc->dwc3_phy, 1);
}

static int32_t dwc3_tcc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node;
	struct dwc3_tcc_hcd *dwc3_tcc;
	struct property *prop;
	int32_t ret;

	if (dev == NULL)
		return -ENODEV;

	node = dev->of_node;

	dwc3_tcc = devm_kzalloc(dev, sizeof(*dwc3_tcc), GFP_KERNEL);
	if (dwc3_tcc == NULL)
		return -ENOMEM;

	/*
	 * Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we move to full device tree support this will vanish off.
	 */
	platform_set_drvdata(pdev, dwc3_tcc);

	dwc3_tcc->dev = dev;

	// Check Host enable pin
	prop = of_find_property(pdev->dev.of_node, "vbus-ctrl-able", NULL);
	if (prop != NULL)
		dwc3_tcc->vbus_ctrl_able = ENABLE;
	else
		dwc3_tcc->vbus_ctrl_able = DISABLE;

	ret = dwc3_tcc_phy_register(dwc3_tcc);
	if (ret != 0) {
		dev_err(dev, "[ERROR][USB] couldn't register out PHY\n");
		goto populate_err;
	}

#if defined(CONFIG_DWC3_DUAL_FIRST_HOST) || defined(CONFIG_USB_DWC3_HOST)
	dwc3_tcc_vbus_ctrl(dwc3_tcc, ON);
#endif // defined(CONFIG_DWC3_DUAL_FIRST_HOST) || defined(CONFIG_USB_DWC3_HOST)

	if (node != NULL) {
		ret = of_platform_populate(node, NULL, NULL, dev);
		if (ret != 0) {
			dev_err(dev, "[ERROR][USB] failed to add dwc3 core\n");
			goto populate_err;
		}
	} else {
		dev_err(dev, "[ERROR][USB] no device node, failed to add dwc3 core\n");
		ret = -ENODEV;
		goto populate_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_vbus);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][USB] failed to create vbus\n");
		goto populate_err;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_dwc3_pcfg);
	if (ret != 0) {
		dev_err(&pdev->dev,
				"[ERROR][USB] failed to create dwc3_pcfg\n");
		goto populate_err;
	}

	return 0;

populate_err:
	//platform_device_unregister(dwc3_tcc->usb2_phy);
	platform_device_unregister(dwc3_tcc->usb3_phy);

	return ret;
}

static int32_t dwc3_tcc_remove(struct platform_device *pdev)
{
	struct dwc3_tcc_hcd *dwc3_tcc = platform_get_drvdata(pdev);

	if (dwc3_tcc == NULL)
		return -ENODEV;

	of_platform_depopulate(dwc3_tcc->dev);
	device_remove_file(&pdev->dev, &dev_attr_dwc3_pcfg);
	device_remove_file(&pdev->dev, &dev_attr_vbus);

	device_for_each_child(&pdev->dev, NULL, &dwc3_tcc_remove_child);

	dwc3_tcc_phy_unregister(dwc3_tcc);
	//platform_device_unregister(dwc3_tcc->usb2_phy);
	platform_device_unregister(dwc3_tcc->usb3_phy);

	dwc3_tcc_vbus_ctrl(dwc3_tcc, OFF);

	return 0;
}

static void dwc3_tcc_shutdown(struct platform_device *dev)
{
	struct dwc3_tcc_hcd *dwc3_tcc = platform_get_drvdata(dev);

	dwc3_tcc_vbus_ctrl(dwc3_tcc, OFF);
}

#if defined(CONFIG_PM_SLEEP)
static int32_t dwc3_tcc_suspend(struct device *dev)
{
	struct dwc3_tcc_hcd *dwc3_tcc =
		platform_get_drvdata(to_platform_device(dev));
	struct usb_phy *phy = dwc3_tcc->dwc3_phy;

	// VBus current status backup for resume
	dwc3_tcc->vbus_pm_status = phy->vbus_status;

	dwc3_tcc_vbus_ctrl(dwc3_tcc, OFF);

	return 0;
}

static int32_t dwc3_tcc_resume(struct device *dev)
{
	struct dwc3_tcc_hcd *dwc3_tcc =
		platform_get_drvdata(to_platform_device(dev));

	dwc3_tcc_vbus_ctrl(dwc3_tcc, dwc3_tcc->vbus_pm_status);

	return 0;
}

static const struct dev_pm_ops dwc3_tcc_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc3_tcc_suspend, dwc3_tcc_resume)
};

#define DEV_PM_OPS	(&dwc3_tcc_dev_pm_ops)
#else
#define DEV_PM_OPS	(NULL)
#endif /* CONFIG_PM_SLEEP */

static const struct of_device_id dwc3_tcc_match[] = {
	{ .compatible = "telechips,tcc-dwc3" },
	{ },
};
MODULE_DEVICE_TABLE(of, dwc3_tcc_match);

static struct platform_driver dwc3_tcc_driver = {
	.probe			= dwc3_tcc_probe,
	.remove			= dwc3_tcc_remove,
	.shutdown		= dwc3_tcc_shutdown,
	.driver = {
		.name		= "tcc-dwc3",
		.owner		= THIS_MODULE,
		.of_match_table	= dwc3_tcc_match,
		.pm		= DEV_PM_OPS,
	},
};

module_platform_driver(dwc3_tcc_driver);

MODULE_ALIAS("platform:tcc-dwc3");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DesignWare USB3 Telechips Glue Layer");

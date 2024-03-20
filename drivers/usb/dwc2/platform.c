// SPDX-License-Identifier: (GPL-2.0+ OR BSD-3-Clause)
/*
 * platform.c - DesignWare HS OTG Controller platform driver
 *
 * Copyright (C) Matthijs Kooijman <matthijs@stdin.nl>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the above-listed copyright holders may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_data/s3c-hsotg.h>
#include <linux/reset.h>

#include <linux/usb/of.h>

#if defined(CONFIG_USB_DWC2_TCC)
#include <linux/kthread.h>
#include <linux/usb/ehci_pdriver.h>
#include <linux/usb/ohci_pdriver.h>
#include <linux/usb/hcd.h>
#endif /* CONFIG_USB_DWC2_TCC */

#include "core.h"
#include "hcd.h"
#include "debug.h"

static const char dwc2_driver_name[] = "dwc2";

/*
 * Check the dr_mode against the module configuration and hardware
 * capabilities.
 *
 * The hardware, module, and dr_mode, can each be set to host, device,
 * or otg. Check that all these values are compatible and adjust the
 * value of dr_mode if possible.
 *
 *                      actual
 *    HW  MOD dr_mode   dr_mode
 *  ------------------------------
 *   HST  HST  any    :  HST
 *   HST  DEV  any    :  ---
 *   HST  OTG  any    :  HST
 *
 *   DEV  HST  any    :  ---
 *   DEV  DEV  any    :  DEV
 *   DEV  OTG  any    :  DEV
 *
 *   OTG  HST  any    :  HST
 *   OTG  DEV  any    :  DEV
 *   OTG  OTG  any    :  dr_mode
 */

#if defined(CONFIG_VBUS_CTRL_DEF_ENABLE)
static unsigned int vbus_control_enable = ENABLE;
#else /* CONFIG_VBUS_CTRL_DEF_ENABLE */
static unsigned int vbus_control_enable = DISABLE;
#endif /* !defined(CONFIG_VBUS_CTRL_DEF_ENABLE) */
module_param(vbus_control_enable, uint, 0644);
MODULE_PARM_DESC(vbus_control_enable, "TCC DWC2 VBus control enable");

#if defined(CONFIG_USB_DWC2_TCC)
struct pcfg_unit U20DH_PCFG1[PCFG_MAX] = {
	/* name, offset, mask */
	{"TXVRT  ",  0, (0xF << 0)},
	{"CDT    ",  4, (0x7 << 4)},
	{"TXPPT  ",  7, (0x1 << 7)},
	{"TP     ",  8, (0x3 << 8)},
	{"TXRT   ", 10, (0x3 << 10)},
	{"TXREST ", 12, (0x3 << 12)},
	{"TXHSXVT", 14, (0x3 << 14)},
};

static const struct usb_ohci_pdata ohci_pdata = {
};

static const struct usb_ehci_pdata ehci_pdata = {
};

#if defined(CONFIG_USB_DWC2_TCC_MUX)
static void dwc2_mux_hcd_remove(struct dwc2_hsotg *tcc_dwc2);
static int dwc2_mux_hcd_init(struct dwc2_hsotg *tcc_dwc2);
#endif /* CONFIG_USB_DWC2_TCC_MUX */

int tcc_dwc2_ctrl_vbus(struct dwc2_hsotg *tcc_dwc2, int on_off)
{
	struct usb_phy *phy = tcc_dwc2->uphy;
	int ret = 0;

	if ((phy == NULL) || (phy->set_vbus == NULL)) {
		dev_err(tcc_dwc2->dev, "[ERROR][USB] PHY driver does NOT exist!\n");
		ret = -ENODEV;
	} else {
		if (vbus_control_enable == DISABLE) {
			dev_info(tcc_dwc2->dev, "[INFO][USB] VBus control is DISABLED!\n");
		} else {
			tcc_dwc2->vbus_status = on_off;
			ret = phy->set_vbus(phy, on_off);
		}
	}

	return ret;
}

#if defined(CONFIG_USB_DWC2_DUAL_ROLE)
/**
 * dwc2_hsotg_read_suspend_state - read current state for suspend
 * @hsotg: The device instance
 *
 * Return the current state for suspend
 */
static u32 dwc2_hsotg_read_suspend_state(struct dwc2_hsotg *hsotg)
{
	u32 dsts;

	dsts = dwc2_readl(hsotg, DSTS);
	dsts &= DSTS_SUSPSTS;

	return dsts;
}

static int tcc_dwc2_soffn_monitor_thread(void *work)
{
	struct dwc2_hsotg *tcc_dwc2 = (struct dwc2_hsotg *)work;
	bool is_disconnected = false;
	int retry = 100; // Wait for the iPhone to switch roles
	unsigned long flags;

	while (!kthread_should_stop() && (retry > 0)) {
		usleep_range(10000, 10020);

		/*
		 * Monitoring starts when the frame number of
		 * SoF(Start of Frame) is not NULL or is not in suspend state.
		 * If ithe host is not connected for 1 second after role
		 * switching, it is deternined as disconnection
		 */
		if (!dwc2_hsotg_read_frameno(tcc_dwc2) ||
				dwc2_hsotg_read_suspend_state(tcc_dwc2) ||
				dwc2_hsotg_read_erratic_error(tcc_dwc2)) {
			retry--;

			if ((is_disconnected == true) && (retry <= 0))
				break;

			continue;
		}

		is_disconnected = true;
		retry = 3;
	}

	tcc_dwc2_ctrl_vbus(tcc_dwc2, OFF);

	if (tcc_dwc2->dr_mode == USB_DR_MODE_PERIPHERAL) {
		spin_lock_irqsave(&tcc_dwc2->lock, flags);
		dwc2_hsotg_disconnect(tcc_dwc2);
		spin_unlock_irqrestore(&tcc_dwc2->lock, flags);

		if (tcc_dwc2->driver && tcc_dwc2->driver->disconnect_tcc)
			tcc_dwc2->driver->disconnect_tcc();
	} else {
		dev_info(tcc_dwc2->dev, "[INFO][USB] Current USB mode: host\n");
	}

	tcc_dwc2->soffn_thread = NULL;
	msleep(200);

	dev_info(tcc_dwc2->dev, "[INFO][USB] Current USB mode: device, Host disconnected\n");

	return 0;
}

static void tcc_dwc2_change_dr_mode(struct work_struct *work)
{
	struct dwc2_hsotg *tcc_dwc2 =
		container_of(work, struct dwc2_hsotg, drd_work);
	int retry = 0;
	unsigned long flags;

	if (tcc_dwc2->dr_mode == USB_DR_MODE_PERIPHERAL) {
#if defined(CONFIG_USB_DWC2_TCC_MUX)
		dwc2_mux_hcd_remove(tcc_dwc2);
#endif /* CONFIG_USB_DWC2_TCC_MUX */
	} else {
		if (tcc_dwc2->soffn_thread != NULL) {
			kthread_stop(tcc_dwc2->soffn_thread);
			tcc_dwc2->soffn_thread = NULL;

			spin_lock_irqsave(&tcc_dwc2->lock, flags);
			dwc2_hsotg_disconnect(tcc_dwc2);
			spin_unlock_irqrestore(&tcc_dwc2->lock, flags);
		}

		do {
			if ((tcc_dwc2_ctrl_vbus(tcc_dwc2, ON) == 0) ||
					(vbus_control_enable == OFF)) {
				break;
			}

			retry++;
			msleep(50);

			dev_info(tcc_dwc2->dev, "[INFO][USB] Retrying VBus control for %d times\n",
					retry);
		} while (retry < VBUS_CTRL_RETRY_MAX);
	}

	dwc2_manual_change(tcc_dwc2);

	if (tcc_dwc2->dr_mode == USB_DR_MODE_HOST) {
#if defined(CONFIG_USB_DWC2_TCC_MUX)
		dwc2_mux_hcd_init(tcc_dwc2);
#endif /* CONFIG_USB_DWC2_TCC_MUX */
	} else {
		if (tcc_dwc2->vbus_status == ON) {
			if (tcc_dwc2->soffn_thread != NULL) {
				kthread_stop(tcc_dwc2->soffn_thread);
				tcc_dwc2->soffn_thread = NULL;
			}

			tcc_dwc2->soffn_thread =
				kthread_run(tcc_dwc2_soffn_monitor_thread,
						(void *)tcc_dwc2, "dwc2-soffn");
			if (IS_ERR(tcc_dwc2->soffn_thread))
				dev_warn(tcc_dwc2->dev, "[WARN][USB] Running soffn_thread is FAILED!\n");
		}

		dev_info(tcc_dwc2->dev, "[INFO][USB] Current USB mode: %s\n",
				(tcc_dwc2->dr_mode == USB_DR_MODE_PERIPHERAL) ?
				"Device" : "Host");
	}
}

static ssize_t drdmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc2_hsotg *tcc_dwc2 = dev_get_drvdata(dev);

	return sprintf(buf, "TCC DWC2 drdmode: %s\n",
			(tcc_dwc2->dr_mode == USB_DR_MODE_PERIPHERAL) ?
			"Device" : "Host");
}

static ssize_t drdmode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc2_hsotg *tcc_dwc2 = dev_get_drvdata(dev);
	struct work_struct *work;

	if (strncmp(buf, "host", 4) == 0) {
		if (tcc_dwc2->dr_mode == USB_DR_MODE_HOST ||
				tcc_dwc2->dr_mode == USB_DR_MODE_OTG) {
			dev_info(tcc_dwc2->dev, "[INFO][USB] Already in HOST mode!\n");

			goto fail;
		} else {
			tcc_dwc2->dr_mode = USB_DR_MODE_HOST;
		}
	} else if (strncmp(buf, "device", 6) == 0) {
		if (tcc_dwc2->dr_mode == USB_DR_MODE_PERIPHERAL) {
			dev_info(tcc_dwc2->dev, "[INFO][USB] Already in DEVICE mode!\n");

			goto fail;
		} else {
			tcc_dwc2->dr_mode = USB_DR_MODE_PERIPHERAL;
		}
	} else {
fail:
		dev_info(tcc_dwc2->dev, "[INFO][USB] Invalid drdmode: %s\n",
				buf);

		return (ssize_t)count;
	}

	work = &tcc_dwc2->drd_work;
	if (work_pending(work)) {
		dev_warn(tcc_dwc2->dev, "[WARN][USB] tcc_dwc2_change_dr_mode() is PENDING!\n");

		return (ssize_t)count;
	}

	queue_work(tcc_dwc2->drd_wq, work);
	flush_work(work); // Wait for operation to complete

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(drdmode);
#endif /* CONFIG_USB_DWC2_DUAL_ROLE */

static ssize_t vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc2_hsotg *tcc_dwc2 = dev_get_drvdata(dev);

	return sprintf(buf, "TCC DWC2 vbus: %s\n",
			(tcc_dwc2->vbus_status == ON) ? "on" : "off");
}

static ssize_t vbus_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc2_hsotg *tcc_dwc2 = dev_get_drvdata(dev);

	if (strncmp(buf, "on", 2) == 0)
		tcc_dwc2_ctrl_vbus(tcc_dwc2, ON);
	else if (strncmp(buf, "off", 3) == 0)
		tcc_dwc2_ctrl_vbus(tcc_dwc2, OFF);

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(vbus);

static ssize_t device_sq_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc2_hsotg *tcc_dwc2 = dev_get_drvdata(dev);
	unsigned long flags;
	int dctl;

	spin_lock_irqsave(&tcc_dwc2->lock, flags);
	dctl = dwc2_readl(tcc_dwc2, DCTL);
	dctl &= DCTL_TSTCTL_MASK;
	dctl >>= DCTL_TSTCTL_SHIFT;
	spin_unlock_irqrestore(&tcc_dwc2->lock, flags);

	switch (dctl) {
	case 0:
		pr_info("no test\n");
		break;
	case TEST_PACKET:
		pr_info("test_packet\n");
		break;
	default:
		pr_info("UNKNOWN test mode\n");
	}

	return 0;
}

static ssize_t device_sq_test_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc2_hsotg *tcc_dwc2 = dev_get_drvdata(dev);
	unsigned long flags;
	u32 testmode = 0;

	if (!strncmp(buf, "on", 2))
		testmode = TEST_PACKET;
	else
		testmode = 0;

	spin_lock_irqsave(&tcc_dwc2->lock, flags);
	dwc2_hsotg_set_test_mode(tcc_dwc2, testmode);
	spin_unlock_irqrestore(&tcc_dwc2->lock, flags);

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(device_sq_test);

static char *tcc_dwc2_display_pcfg(uint32_t old_reg, uint32_t new_reg,
		char *str)
{
	uint32_t old_val, new_val;
	int i;

	for (i = 0; i < PCFG_MAX; i++) {
		old_val = (BIT_MSK(old_reg, U20DH_PCFG1[i].mask)) >>
			(U20DH_PCFG1[i].offset);
		new_val = (BIT_MSK(new_reg, U20DH_PCFG1[i].mask)) >>
			(U20DH_PCFG1[i].offset);

		if (old_val != new_val) {
			sprintf(U20DH_PCFG1[i].str, "%s: 0x%X -> 0x%X\n",
					U20DH_PCFG1[i].reg_name, old_val,
					new_val);
		} else {
			sprintf(U20DH_PCFG1[i].str, "%s: 0x%X\n",
					U20DH_PCFG1[i].reg_name, old_val);
		}

		strncat(str, U20DH_PCFG1[i].str, 256 - strlen(str) - 1);
	}

	return str;
}

static ssize_t dwc2_dev_pcfg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc2_hsotg *tcc_dwc2 = dev_get_drvdata(dev);
	struct U20DH_DEV_PHY *u20dh_dev =
		(struct U20DH_DEV_PHY *)(tcc_dwc2->uphy->get_base(tcc_dwc2->uphy));
	uint32_t dev_pcfg1_val = readl(&u20dh_dev->PCFG1);
	char str[256] = {0};

	return sprintf(buf, "U20DH_DEV_PCFG1: 0x%08X\n%s", dev_pcfg1_val,
			tcc_dwc2_display_pcfg(dev_pcfg1_val, dev_pcfg1_val,
				str));
}

static ssize_t dwc2_dev_pcfg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc2_hsotg *tcc_dwc2 = dev_get_drvdata(dev);
	struct U20DH_DEV_PHY *u20dh_dev =
		(struct U20DH_DEV_PHY *)(tcc_dwc2->uphy->get_base(tcc_dwc2->uphy));
	int i, ret;
	uint32_t old_reg = readl(&u20dh_dev->PCFG1);
	uint32_t new_reg;
	char str[256] = {0};

	if ((buf[0] != '0') || (buf[1] != 'x')) {
		pr_info("[INFO][USB]\n\techo %c%cXXXXXXXX is not 0x\n\n",
				buf[0], buf[1]);
		pr_info("\tUsage : echo 0xXXXXXXXX > dwc2_dev_pcfg\n\n");
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
			pr_info("\tUsage : echo 0xXXXXXXXX > dwc2_dev_pcfg\n\n");
			pr_info("\t\t2) X is hex number(0 to f)\n\n");

			return (ssize_t)count;
		}
	}

	if (((count - 1) < 10) || (10 < (count - 1))) {
		pr_info("[INFO][USB]\nThis argument length is not 10\n\n");
		pr_info("\tUsage : echo 0xXXXXXXXX > dwc2_dev_pcfg\n\n");
		pr_info("\t\t1) length of 0xXXXXXXXX is 10\n");
		pr_info("\t\t2) X is hex number(0 to f)\n\n");

		return (ssize_t)count;
	}

	ret = kstrtoul(buf, 0, (unsigned long *)&new_reg);
	if (ret)
		return -EINVAL;

	pr_info("[INFO][USB] Before\nU20DH_DEV_PCFG1: 0x%08X\n", old_reg);
	writel(new_reg, &u20dh_dev->PCFG1);
	new_reg = readl(&u20dh_dev->PCFG1);

	tcc_dwc2_display_pcfg(old_reg, new_reg, str);
	pr_info("\n[INFO][USB] After\n%sU20DH_DEV_PCFG1: 0x%08X\n", str,
			new_reg);

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(dwc2_dev_pcfg);

#if defined(CONFIG_USB_DWC2_TCC_MUX)
static ssize_t dwc2_mux_host_pcfg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc2_hsotg *tcc_dwc2 = dev_get_drvdata(dev);
	struct U20DH_HST_PHY *u20dh_hst =
		(struct U20DH_HST_PHY *)
		(tcc_dwc2->mhst_uphy->get_base(tcc_dwc2->mhst_uphy));
	uint32_t hst_pcfg1_val = readl(&u20dh_hst->PCFG1);
	char str[256] = {0};

	return sprintf(buf, "U20DH_HST_PCFG1: 0x%08X\n%s", hst_pcfg1_val,
			tcc_dwc2_display_pcfg(hst_pcfg1_val, hst_pcfg1_val,
				str));
}

static ssize_t dwc2_mux_host_pcfg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc2_hsotg *tcc_dwc2 = dev_get_drvdata(dev);
	struct U20DH_HST_PHY *u20dh_hst =
		(struct U20DH_HST_PHY *)
		(tcc_dwc2->mhst_uphy->get_base(tcc_dwc2->mhst_uphy));
	int i, ret;
	uint32_t old_reg = readl(&u20dh_hst->PCFG1);
	uint32_t new_reg;
	char str[256] = {0};

	if ((buf[0] != '0') || (buf[1] != 'x')) {
		pr_info("[INFO][USB]\n\techo %c%cXXXXXXXX is not 0x\n\n",
				buf[0], buf[1]);
		pr_info("\tUsage : echo 0xXXXXXXXX > dwc2_mux_host_pcfg\n\n");
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
			pr_info("\tUsage : echo 0xXXXXXXXX > dwc2_mux_host_pcfg\n\n");
			pr_info("\t\t2) X is hex number(0 to f)\n\n");

			return (ssize_t)count;
		}
	}

	if (((count - 1) < 10) || (10 < (count - 1))) {
		pr_info("[INFO][USB]\nThis argument length is not 10\n\n");
		pr_info("\tUsage : echo 0xXXXXXXXX > dwc2_mux_host_pcfg\n\n");
		pr_info("\t\t1) length of 0xXXXXXXXX is 10\n");
		pr_info("\t\t2) X is hex number(0 to f)\n\n");

		return (ssize_t)count;
	}

	ret = kstrtoul(buf, 0, (unsigned long *)&new_reg);
	if (ret)
		return -EINVAL;

	pr_info("[INFO][USB] Before\nU20DH_HST_PCFG1: 0x%08X\n", old_reg);
	writel(new_reg, &u20dh_hst->PCFG1);
	new_reg = readl(&u20dh_hst->PCFG1);

	tcc_dwc2_display_pcfg(old_reg, new_reg, str);
	pr_info("\n[INFO][USB] After\n%sU20DH_HST_PCFG1: 0x%08X\n", str,
			new_reg);

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(dwc2_mux_host_pcfg);

static struct platform_device *tcc_dwc2_hcd_create_pdev(
		struct dwc2_hsotg *tcc_dwc2, bool ohci,
		unsigned long res_start, u32 size)
{
	struct resource hci_res[2];
	struct platform_device *hci_dev;
	struct usb_hcd *hcd;
	int ret = -ENOMEM;

	memset(hci_res, 0, sizeof(hci_res));

	hci_res[0].start = res_start;
	hci_res[0].end = res_start + size - 1;
	hci_res[0].flags = IORESOURCE_MEM;

	hci_res[1].start = tcc_dwc2->ehci_irq;
	hci_res[1].flags = IORESOURCE_IRQ;

	hci_dev = platform_device_alloc(ohci ? "ohci-mux" : "ehci-mux", 0);
	if (!hci_dev)
		return NULL;

	hci_dev->dev.parent = tcc_dwc2->dev;
	hci_dev->dev.dma_mask = &hci_dev->dev.coherent_dma_mask;

	ret = platform_device_add_resources(hci_dev, hci_res,
			ARRAY_SIZE(hci_res));
	if (ret)
		goto err_alloc;

	if (ohci)
		ret = platform_device_add_data(hci_dev, &ohci_pdata,
				sizeof(ohci_pdata));
	else
		ret = platform_device_add_data(hci_dev, &ehci_pdata,
				sizeof(ehci_pdata));
	if (ret)
		goto err_alloc;

	ret = platform_device_add(hci_dev);
	if (ret)
		goto err_alloc;

	hcd = dev_get_drvdata(&hci_dev->dev);
	if (hcd == NULL) {
		dev_err(&hci_dev->dev, "[ERROR][USB] hcd is NULL!\n");

		while (1)
			;
	}

	if (!ohci && tcc_dwc2->mhst_uphy)
		hcd->usb_phy = tcc_dwc2->mhst_uphy;

	return hci_dev;

err_alloc:
	platform_device_put(hci_dev);

	return ERR_PTR(ret);
}

static int dwc2_mux_hcd_init(struct dwc2_hsotg *tcc_dwc2)
{
	struct tcc_mux_hcd *tcc_mux;
	unsigned long start;
	int size;
	int err;

	tcc_mux = kzalloc(sizeof(struct tcc_mux_hcd), GFP_KERNEL);
	if (!tcc_mux)
		return -ENOMEM;

	start = (unsigned long)tcc_dwc2->ohci_regs;
	size = tcc_dwc2->ohci_regs_size;
	tcc_mux->ohci_dev = tcc_dwc2_hcd_create_pdev(tcc_dwc2, true, start, size);
	if (IS_ERR(tcc_mux->ohci_dev)) {
		err = PTR_ERR(tcc_mux->ohci_dev);
		goto err_free_usb_dev;
	}

	start = (unsigned long)tcc_dwc2->ehci_regs;
	size = tcc_dwc2->ehci_regs_size;
	tcc_mux->ehci_dev = tcc_dwc2_hcd_create_pdev(tcc_dwc2, false, start, size);
	if (IS_ERR(tcc_mux->ehci_dev)) {
		err = PTR_ERR(tcc_mux->ehci_dev);
		goto err_unregister_ohci_dev;
	}

	tcc_dwc2->mhst_dev = tcc_mux;

	return 0;

err_unregister_ohci_dev:
	platform_device_unregister(tcc_mux->ohci_dev);

err_free_usb_dev:
	kfree(tcc_mux);

	return err;
}

static void dwc2_mux_hcd_remove(struct dwc2_hsotg *tcc_dwc2)
{
	struct tcc_mux_hcd *tcc_mux;
	struct platform_device *ohci_dev;
	struct platform_device *ehci_dev;

	tcc_mux = tcc_dwc2->mhst_dev;
	ohci_dev = tcc_mux->ohci_dev;
	ehci_dev = tcc_mux->ehci_dev;

	if (tcc_dwc2->irq) {
		irq_set_affinity_hint(tcc_dwc2->irq, NULL);
	}

	if (ohci_dev)
		platform_device_unregister(ohci_dev);

	if (ehci_dev)
		platform_device_unregister(ehci_dev);

	kfree(tcc_mux);
}
#endif /* CONFIG_USB_DWC2_TCC_MUX */
#endif /* CONFIG_USB_DWC2_TCC */

static int dwc2_get_dr_mode(struct dwc2_hsotg *hsotg)
{
	enum usb_dr_mode mode;

	hsotg->dr_mode = usb_get_dr_mode(hsotg->dev);
	if (hsotg->dr_mode == USB_DR_MODE_UNKNOWN)
		hsotg->dr_mode = USB_DR_MODE_OTG;

	mode = hsotg->dr_mode;

	if (dwc2_hw_is_device(hsotg)) {
		if (IS_ENABLED(CONFIG_USB_DWC2_HOST)) {
			dev_err(hsotg->dev,
				"Controller does not support host mode.\n");
			return -EINVAL;
		}
		mode = USB_DR_MODE_PERIPHERAL;
	} else if (dwc2_hw_is_host(hsotg)) {
		if (IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL)) {
			dev_err(hsotg->dev,
				"Controller does not support device mode.\n");
			return -EINVAL;
		}
		mode = USB_DR_MODE_HOST;
	} else {
		if (IS_ENABLED(CONFIG_USB_DWC2_HOST))
			mode = USB_DR_MODE_HOST;
		else if (IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL))
			mode = USB_DR_MODE_PERIPHERAL;
	}

	if (mode != hsotg->dr_mode) {
		dev_warn(hsotg->dev,
			 "Configuration mismatch. dr_mode forced to %s\n",
			mode == USB_DR_MODE_HOST ? "host" : "device");

		hsotg->dr_mode = mode;
	}

	return 0;
}

static int __dwc2_lowlevel_hw_enable(struct dwc2_hsotg *hsotg)
{
	struct platform_device *pdev = to_platform_device(hsotg->dev);
	int ret;

#if !defined(CONFIG_USB_DWC2_TCC)
	ret = regulator_bulk_enable(ARRAY_SIZE(hsotg->supplies),
				    hsotg->supplies);
	if (ret)
		return ret;

	if (hsotg->clk) {
		ret = clk_prepare_enable(hsotg->clk);
		if (ret)
			return ret;
	}
#endif /* !defined(CONFIG_USB_DWC2_TCC) */

	if (hsotg->uphy) {
		ret = usb_phy_init(hsotg->uphy);
	} else if (hsotg->plat && hsotg->plat->phy_init) {
		ret = hsotg->plat->phy_init(pdev, hsotg->plat->phy_type);
	} else {
		ret = phy_power_on(hsotg->phy);
		if (ret == 0)
			ret = phy_init(hsotg->phy);
	}

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	if (hsotg->mhst_uphy) {
		if ((hsotg->dr_mode == USB_DR_MODE_HOST) ||
				(hsotg->dr_mode == USB_DR_MODE_OTG)) {
			ret = usb_phy_init(hsotg->mhst_uphy);
		}
	}
#endif /* CONFIG_USB_DWC2_TCC_MUX */

	return ret;
}

/**
 * dwc2_lowlevel_hw_enable - enable platform lowlevel hw resources
 * @hsotg: The driver state
 *
 * A wrapper for platform code responsible for controlling
 * low-level USB platform resources (phy, clock, regulators)
 */
int dwc2_lowlevel_hw_enable(struct dwc2_hsotg *hsotg)
{
	int ret = __dwc2_lowlevel_hw_enable(hsotg);

	if (ret == 0)
		hsotg->ll_hw_enabled = true;
	return ret;
}

static int __dwc2_lowlevel_hw_disable(struct dwc2_hsotg *hsotg)
{
	struct platform_device *pdev = to_platform_device(hsotg->dev);
	int ret = 0;

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	if (hsotg->mhst_uphy)
		usb_phy_shutdown(hsotg->mhst_uphy);
#endif /* CONFIG_USB_DWC2_TCC_MUX */

	if (hsotg->uphy) {
		usb_phy_shutdown(hsotg->uphy);
	} else if (hsotg->plat && hsotg->plat->phy_exit) {
		ret = hsotg->plat->phy_exit(pdev, hsotg->plat->phy_type);
	} else {
		ret = phy_exit(hsotg->phy);
		if (ret == 0)
			ret = phy_power_off(hsotg->phy);
	}
	if (ret)
		return ret;

#if defined(CONFIG_USB_DWC2_TCC)
	ret = tcc_dwc2_ctrl_vbus(hsotg, 0);
#else /* CONFIG_USB_DWC2_TCC */
	if (hsotg->clk)
		clk_disable_unprepare(hsotg->clk);

	ret = regulator_bulk_disable(ARRAY_SIZE(hsotg->supplies),
				     hsotg->supplies);
#endif /* !defined(CONFIG_USB_DWC2_TCC) */

	return ret;
}

/**
 * dwc2_lowlevel_hw_disable - disable platform lowlevel hw resources
 * @hsotg: The driver state
 *
 * A wrapper for platform code responsible for controlling
 * low-level USB platform resources (phy, clock, regulators)
 */
int dwc2_lowlevel_hw_disable(struct dwc2_hsotg *hsotg)
{
	int ret = __dwc2_lowlevel_hw_disable(hsotg);

	if (ret == 0)
		hsotg->ll_hw_enabled = false;
	return ret;
}

static int dwc2_lowlevel_hw_init(struct dwc2_hsotg *hsotg)
{
	int ret;
#if !defined(CONFIG_USB_DWC2_TCC)
	int i;
#endif /* !defined(CONFIG_USB_DWC2_TCC) */

	hsotg->reset = devm_reset_control_get_optional(hsotg->dev, "dwc2");
	if (IS_ERR(hsotg->reset)) {
		ret = PTR_ERR(hsotg->reset);
		dev_err(hsotg->dev, "error getting reset control %d\n", ret);
		return ret;
	}

	reset_control_deassert(hsotg->reset);

	hsotg->reset_ecc = devm_reset_control_get_optional(hsotg->dev, "dwc2-ecc");
	if (IS_ERR(hsotg->reset_ecc)) {
		ret = PTR_ERR(hsotg->reset_ecc);
		dev_err(hsotg->dev, "error getting reset control for ecc %d\n", ret);
		return ret;
	}

	reset_control_deassert(hsotg->reset_ecc);

	/*
	 * Attempt to find a generic PHY, then look for an old style
	 * USB PHY and then fall back to pdata
	 */
	hsotg->phy = devm_phy_get(hsotg->dev, "usb2-phy");
	if (IS_ERR(hsotg->phy)) {
		ret = PTR_ERR(hsotg->phy);
		switch (ret) {
		case -ENODEV:
		case -ENOSYS:
			hsotg->phy = NULL;
			break;
		case -EPROBE_DEFER:
			return ret;
		default:
			dev_err(hsotg->dev, "error getting phy %d\n", ret);
			return ret;
		}
	}

	if (!hsotg->phy) {
#if defined(CONFIG_USB_DWC2_TCC)
		hsotg->uphy = devm_usb_get_phy_by_phandle(hsotg->dev, "phy", 0);

		if (!IS_ENABLED(CONFIG_ARCH_TCC897X)) {
			ret = hsotg->uphy->set_vbus_resource(hsotg->uphy);
			if (ret)
				return ret;
		}
#else /* CONFIG_USB_DWC2_TCC */
		hsotg->uphy = devm_usb_get_phy(hsotg->dev, USB_PHY_TYPE_USB2);
#endif /* !defined(CONFIG_USB_DWC2_TCC) */
		if (IS_ERR(hsotg->uphy)) {
			ret = PTR_ERR(hsotg->uphy);
			switch (ret) {
			case -ENODEV:
			case -ENXIO:
				hsotg->uphy = NULL;
				break;
			case -EPROBE_DEFER:
				return ret;
			default:
				dev_err(hsotg->dev, "error getting usb phy %d\n",
					ret);
				return ret;
			}
		}
	}

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	hsotg->mhst_uphy = devm_usb_get_phy_by_phandle(hsotg->dev,
			"telechips,mhst_phy", 0);
	if (IS_ERR(hsotg->mhst_uphy)) {
		dev_warn(hsotg->dev, "[WARN][USB] hsotg->mhst_uphy is NULL\n");

		ret = PTR_ERR(hsotg->uphy);
		switch (ret) {
		case -ENODEV:
		case -ENXIO:
			hsotg->mhst_uphy = NULL;
			break;
		case -EPROBE_DEFER:
			return ret;
		default:
			dev_err(hsotg->dev, "[ERROR][USB] error getting usb mux host phy %d\n",
					ret);
			return ret;
		}
	}

	hsotg->mhst_uphy->otg->mux_cfg_addr = hsotg->uphy->base + MUX_SEL;
#endif /* CONFIG_USB_DWC2_TCC_MUX */

	hsotg->plat = dev_get_platdata(hsotg->dev);

#if !defined(CONFIG_USB_DWC2_TCC)
	/* Regulators */
	for (i = 0; i < ARRAY_SIZE(hsotg->supplies); i++)
		hsotg->supplies[i].supply = dwc2_hsotg_supply_names[i];

	ret = devm_regulator_bulk_get(hsotg->dev, ARRAY_SIZE(hsotg->supplies),
				      hsotg->supplies);
	if (ret) {
		dev_err(hsotg->dev, "failed to request supplies: %d\n", ret);
		return ret;
	}
#endif /* !defined(CONFIG_USB_DWC2_TCC) */

	return 0;
}

/**
 * dwc2_driver_remove() - Called when the DWC_otg core is unregistered with the
 * DWC_otg driver
 *
 * @dev: Platform device
 *
 * This routine is called, for example, when the rmmod command is executed. The
 * device may or may not be electrically present. If it is present, the driver
 * stops device processing. Any resources used on behalf of this device are
 * freed.
 */
static int dwc2_driver_remove(struct platform_device *dev)
{
	struct dwc2_hsotg *hsotg = platform_get_drvdata(dev);

	dwc2_debugfs_exit(hsotg);
	if (hsotg->hcd_enabled)
#if defined(CONFIG_USB_DWC2_TCC_MUX)
		if ((hsotg->dr_mode == USB_DR_MODE_HOST) ||
				(hsotg->dr_mode == USB_DR_MODE_OTG)) {
			dwc2_mux_hcd_remove(hsotg);
		}
#else
	dwc2_hcd_remove(hsotg);
#endif

	if (hsotg->gadget_enabled)
		dwc2_hsotg_remove(hsotg);

	if (hsotg->ll_hw_enabled)
		dwc2_lowlevel_hw_disable(hsotg);

	reset_control_assert(hsotg->reset);
	reset_control_assert(hsotg->reset_ecc);

#if defined(CONFIG_USB_DWC2_TCC)
#if defined(CONFIG_USB_DWC2_DUAL_ROLE)
	cancel_work_sync(&hsotg->drd_work);
	destroy_workqueue(hsotg->drd_wq);

	device_remove_file(&dev->dev, &dev_attr_drdmode);
#endif /* CONFIG_USB_DWC2_DUAL_ROLE */

	device_remove_file(&dev->dev, &dev_attr_vbus);
	device_remove_file(&dev->dev, &dev_attr_device_sq_test);
	device_remove_file(&dev->dev, &dev_attr_dwc2_dev_pcfg);

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	device_remove_file(&dev->dev, &dev_attr_dwc2_mux_host_pcfg);
#endif /* CONFIG_USB_DWC2_TCC_MUX */
#endif /* CONFIG_USB_DWC2_TCC */

	return 0;
}

/**
 * dwc2_driver_shutdown() - Called on device shutdown
 *
 * @dev: Platform device
 *
 * In specific conditions (involving usb hubs) dwc2 devices can create a
 * lot of interrupts, even to the point of overwhelming devices running
 * at low frequencies. Some devices need to do special clock handling
 * at shutdown-time which may bring the system clock below the threshold
 * of being able to handle the dwc2 interrupts. Disabling dwc2-irqs
 * prevents reboots/poweroffs from getting stuck in such cases.
 */
static void dwc2_driver_shutdown(struct platform_device *dev)
{
	struct dwc2_hsotg *hsotg = platform_get_drvdata(dev);

	if (hsotg->ll_hw_enabled)
		dwc2_lowlevel_hw_disable(hsotg);

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	disable_irq(hsotg->ehci_irq);
#endif /* CONFIG_USB_DWC2_TCC_MUX */

	dwc2_disable_global_interrupts(hsotg);
	synchronize_irq(hsotg->irq);
}

/**
 * dwc2_check_core_endianness() - Returns true if core and AHB have
 * opposite endianness.
 * @hsotg:	Programming view of the DWC_otg controller.
 */
static bool dwc2_check_core_endianness(struct dwc2_hsotg *hsotg)
{
	u32 snpsid;

	snpsid = ioread32(hsotg->regs + GSNPSID);
	if ((snpsid & GSNPSID_ID_MASK) == DWC2_OTG_ID ||
	    (snpsid & GSNPSID_ID_MASK) == DWC2_FS_IOT_ID ||
	    (snpsid & GSNPSID_ID_MASK) == DWC2_HS_IOT_ID)
		return false;
	return true;
}

/**
 * dwc2_driver_probe() - Called when the DWC_otg core is bound to the DWC_otg
 * driver
 *
 * @dev: Platform device
 *
 * This routine creates the driver components required to control the device
 * (core, HCD, and PCD) and initializes the device. The driver components are
 * stored in a dwc2_hsotg structure. A reference to the dwc2_hsotg is saved
 * in the device private data. This allows the driver to access the dwc2_hsotg
 * structure on subsequent calls to driver methods for this device.
 */
static int dwc2_driver_probe(struct platform_device *dev)
{
	struct dwc2_hsotg *hsotg;
	struct resource *res;
	int retval;
#if defined(CONFIG_USB_DWC2_TCC)
	unsigned int cpu = 1;
#endif /* CONFIG_USB_DWC2_TCC */

	hsotg = devm_kzalloc(&dev->dev, sizeof(*hsotg), GFP_KERNEL);
	if (!hsotg)
		return -ENOMEM;

	hsotg->dev = &dev->dev;

	/*
	 * Use reasonable defaults so platforms don't have to provide these.
	 */
	if (!dev->dev.dma_mask)
		dev->dev.dma_mask = &dev->dev.coherent_dma_mask;
	retval = dma_set_coherent_mask(&dev->dev, DMA_BIT_MASK(32));
	if (retval) {
		dev_err(&dev->dev, "can't set coherent DMA mask: %d\n", retval);
		return retval;
	}

	res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	hsotg->regs = devm_ioremap_resource(&dev->dev, res);
	if (IS_ERR(hsotg->regs))
		return PTR_ERR(hsotg->regs);

	dev_dbg(&dev->dev, "mapped PA %08lx to VA %p\n",
		(unsigned long)res->start, hsotg->regs);

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	// Get EHCI register base for MUX
	hsotg->ehci_regs = (void __iomem *)(long)dev->resource[1].start;
	hsotg->ehci_regs_size =
		dev->resource[1].end - dev->resource[1].start + 1;
	if (IS_ERR(hsotg->ehci_regs))
		return PTR_ERR(hsotg->ehci_regs);

	dev_info(&dev->dev,
			"[INFO][USB] EHCI mapped PA %08lx to VA %p, size %d\n",
			(unsigned long)res->start,
			hsotg->ehci_regs, hsotg->ehci_regs_size);

	// Get OHCI register base for MUX
	hsotg->ohci_regs = (void __iomem *)(long)dev->resource[2].start;
	hsotg->ohci_regs_size =
		dev->resource[2].end - dev->resource[2].start + 1;
	if (IS_ERR(hsotg->ohci_regs))
		return PTR_ERR(hsotg->ohci_regs);

	dev_info(&dev->dev,
			"[INFO][USB] OHCI mapped PA %08lx to VA %p, size %d\n",
			(unsigned long)res->start,
			hsotg->ohci_regs, hsotg->ohci_regs_size);
#endif /* CONFIG_USB_DWC2_TCC_MUX */

	retval = dwc2_lowlevel_hw_init(hsotg);
	if (retval)
		return retval;

	spin_lock_init(&hsotg->lock);

	hsotg->irq = platform_get_irq(dev, 0);
	if (hsotg->irq < 0)
		return hsotg->irq;

	dev_dbg(hsotg->dev, "registering common handler for irq%d\n",
		hsotg->irq);

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	hsotg->ehci_irq = platform_get_irq(dev, 1);
	if (hsotg->ehci_irq < 0) {
		dev_err(&dev->dev, "[ERROR][USB] Getting IRQ FAIL!\n");
		return hsotg->ehci_irq;
	}

	dev_dbg(hsotg->dev, "[DEBUG][USB] registering common handler for EHCI irq%d\n",
			hsotg->ehci_irq);
#endif /* CONFIG_USB_DWC2_TCC_MUX */

#if defined(CONFIG_USB_DWC2_TCC)
	// Set IRQ affinity to handle the IRQ more stably
	retval = irq_set_affinity_hint(hsotg->irq, cpumask_of(cpu));
	if (retval) {
		dev_err(hsotg->dev, "[ERROR][USB] Setting IRQ affinity FAIL!, cpu: %d irq: %d err: %d\n",
				cpu, hsotg->irq, retval);
		return retval;
	}
#endif /* CONFIG_USB_DWC2_TCC */

	retval = devm_request_irq(hsotg->dev, hsotg->irq,
				  dwc2_handle_common_intr, IRQF_SHARED,
				  dev_name(hsotg->dev), hsotg);
	if (retval)
		return retval;

	hsotg->vbus_supply = devm_regulator_get_optional(hsotg->dev, "vbus");
	if (IS_ERR(hsotg->vbus_supply)) {
		retval = PTR_ERR(hsotg->vbus_supply);
		hsotg->vbus_supply = NULL;
		if (retval != -ENODEV)
			return retval;
	}

	retval = dwc2_get_dr_mode(hsotg);
	if (retval)
		goto error;

	retval = dwc2_lowlevel_hw_enable(hsotg);
	if (retval)
		return retval;

	hsotg->needs_byte_swap = dwc2_check_core_endianness(hsotg);

	hsotg->need_phy_for_wake =
		of_property_read_bool(dev->dev.of_node,
				      "snps,need-phy-for-wake");

	/*
	 * Reset before dwc2_get_hwparams() then it could get power-on real
	 * reset value form registers.
	 */
	retval = dwc2_core_reset(hsotg, false);
	if (retval)
		goto error;

	/* Detect config values from hardware */
	retval = dwc2_get_hwparams(hsotg);
	if (retval)
		goto error;

	/*
	 * For OTG cores, set the force mode bits to reflect the value
	 * of dr_mode. Force mode bits should not be touched at any
	 * other time after this.
	 */
	dwc2_force_dr_mode(hsotg);

	retval = dwc2_init_params(hsotg);
	if (retval)
		goto error;

	if (hsotg->dr_mode != USB_DR_MODE_HOST) {
		retval = dwc2_gadget_init(hsotg);
		if (retval)
			goto error;
		hsotg->gadget_enabled = 1;
	}

	/*
	 * If we need PHY for wakeup we must be wakeup capable.
	 * When we have a device that can wake without the PHY we
	 * can adjust this condition.
	 */
	if (hsotg->need_phy_for_wake)
		device_set_wakeup_capable(&dev->dev, true);

	hsotg->reset_phy_on_wake =
		of_property_read_bool(dev->dev.of_node,
				      "snps,reset-phy-on-wake");
	if (hsotg->reset_phy_on_wake && !hsotg->phy) {
		dev_warn(hsotg->dev,
			 "Quirk reset-phy-on-wake only supports generic PHYs\n");
		hsotg->reset_phy_on_wake = false;
	}

	if (hsotg->dr_mode != USB_DR_MODE_PERIPHERAL) {
#if defined(CONFIG_USB_DWC2_TCC_MUX)
		retval = dwc2_mux_hcd_init(hsotg);
#else /* CONFIG_USB_DWC2_TCC_MUX */
		retval = dwc2_hcd_init(hsotg);
#endif /* !defined(CONFIG_USB_DWC2_TCC_MUX) */

		if (retval) {
			if (hsotg->gadget_enabled)
				dwc2_hsotg_remove(hsotg);
			goto error;
		}
		hsotg->hcd_enabled = 1;
	}

	platform_set_drvdata(dev, hsotg);
	hsotg->hibernated = 0;

	dwc2_debugfs_init(hsotg);

	/* Gadget code manages lowlevel hw on its own */
	if (hsotg->dr_mode == USB_DR_MODE_PERIPHERAL)
		dwc2_lowlevel_hw_disable(hsotg);

#if defined(CONFIG_USB_DWC2_TCC)
#if defined(CONFIG_USB_DWC2_DUAL_ROLE)
	hsotg->drd_wq = create_singlethread_workqueue("dwc2");
	if (!hsotg->drd_wq)
		goto error;

	INIT_WORK(&hsotg->drd_work, tcc_dwc2_change_dr_mode);

#if defined(CONFIG_USB_DWC2_TCC_FIRST_HOST)
#if !defined(CONFIG_USB_DWC2_TCC_MUX)
	hsotg->dr_mode = USB_DR_MODE_HOST;
	dwc2_manual_change(hsotg);
#endif /* !defined(CONFIG_USB_DWC2_TCC_MUX) */
#elif defined(CONFIG_USB_DWC2_TCC_FIRST_PERIPHERAL)
	hsotg->dr_mode = USB_DR_MODE_PERIPHERAL;
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	struct work_struct *work = &hsotg->drd_work;

	if (work_pending(work))
		dev_warn(hsotg->dev, "[WARN][USB] tcc_dwc2_change_dr_mode() is pending\n");

	queue_work(hsotg->drd_wq, work);
	flush_work(work); // Waiting for operation to complete
#endif /* CONFIG_USB_DWC2_TCC_MUX */
#endif /* CONFIG_USB_DWC2_TCC_FIRST_PERIPHERAL */

	retval = device_create_file(&dev->dev, &dev_attr_drdmode);
	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] Failed to create drdmode sysfs\n");
#endif /* CONFIG_USB_DWC2_DUAL_ROLE */

	if (IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL) ||
			IS_ENABLED(CONFIG_USB_DWC2_TCC_FIRST_PERIPHERAL)) {
		tcc_dwc2_ctrl_vbus(hsotg, 0);
	} else {
		tcc_dwc2_ctrl_vbus(hsotg, 1);
	}

	retval = device_create_file(&dev->dev, &dev_attr_vbus);
	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] Failed to create vbus sysfs\n");

	retval = device_create_file(&dev->dev, &dev_attr_device_sq_test);
	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] Failed to create device_sq_test sysfs\n");

	retval = device_create_file(&dev->dev, &dev_attr_dwc2_dev_pcfg);
	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] Failed to create dwc2_dev_pcfg sysfs\n");

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	retval = device_create_file(&dev->dev, &dev_attr_dwc2_mux_host_pcfg);
	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] Failed to create dwc2_mux_host_pcfg sysfs\n");
#endif /* CONFIG_USB_DWC2_TCC_MUX */
#endif /* CONFIG_USB_DWC2_TCC */

#if IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL) || \
	IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
	/* Postponed adding a new gadget to the udc class driver list */
	if (hsotg->gadget_enabled) {
		retval = usb_add_gadget_udc(hsotg->dev, &hsotg->gadget);
		if (retval) {
			hsotg->gadget.udc = NULL;
			dwc2_hsotg_remove(hsotg);
			goto error;
		}
	}
#endif /* CONFIG_USB_DWC2_PERIPHERAL || CONFIG_USB_DWC2_DUAL_ROLE */
	return 0;

error:
	if (hsotg->dr_mode != USB_DR_MODE_PERIPHERAL)
		dwc2_lowlevel_hw_disable(hsotg);
	return retval;
}

static int __maybe_unused dwc2_suspend(struct device *dev)
{
	struct dwc2_hsotg *dwc2 = dev_get_drvdata(dev);
	bool is_device_mode = dwc2_is_device_mode(dwc2);
	int ret = 0;

	if (is_device_mode)
		dwc2_hsotg_suspend(dwc2);

	if (dwc2->ll_hw_enabled &&
	    (is_device_mode || dwc2_host_can_poweroff_phy(dwc2))) {
		ret = __dwc2_lowlevel_hw_disable(dwc2);
		dwc2->phy_off_for_suspend = true;
	}

	return ret;
}

static int __maybe_unused dwc2_resume(struct device *dev)
{
	struct dwc2_hsotg *dwc2 = dev_get_drvdata(dev);
	int ret = 0;

	if (dwc2->phy_off_for_suspend && dwc2->ll_hw_enabled) {
		ret = __dwc2_lowlevel_hw_enable(dwc2);
		if (ret)
			return ret;
	}
	dwc2->phy_off_for_suspend = false;

	if (dwc2_is_device_mode(dwc2))
		ret = dwc2_hsotg_resume(dwc2);

	return ret;
}

static const struct dev_pm_ops dwc2_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc2_suspend, dwc2_resume)
};

static struct platform_driver dwc2_platform_driver = {
	.driver = {
		.name = dwc2_driver_name,
		.of_match_table = dwc2_of_match_table,
		.pm = &dwc2_dev_pm_ops,
	},
	.probe = dwc2_driver_probe,
	.remove = dwc2_driver_remove,
	.shutdown = dwc2_driver_shutdown,
};

module_platform_driver(dwc2_platform_driver);

MODULE_DESCRIPTION("DESIGNWARE HS OTG Platform Glue");
MODULE_AUTHOR("Matthijs Kooijman <matthijs@stdin.nl>");
MODULE_LICENSE("Dual BSD/GPL");

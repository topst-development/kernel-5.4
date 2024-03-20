// SPDX-License-Identifier: GPL-2.0
/*
 *
 * Copyright (C) Telechips Inc.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fs.h>

#include "hdcp_interface.h"

#define HDCP_DRV_VERSION		"0.4.0"

/* Internal Options */
// #define HDCP22_INT_USED

/* DP Link register */
#define SWRESET				(0x0204)
#define SWRESET_HDCP			(1<<2)

/* Register offsets */
#define HDCPCFG				(0x0E00)
#define HDCPOBS				(0x0E04)
#define HDCPAPIINTCLR			(0x0E08)
#define HDCPAPIINTSTAT			(0x0E0C)
#define HDCPAPIINTMSK			(0x0E10)
#define HDCPKSVMEMCTRL			(0x0E18)
#define HDCP_REVOC_RAM			(0x0E20)
#define HDCP_REVOC_LIST			(0x10B8)
#define HDCPREG_BKSV0			(0x3600)
#define HDCPREG_BKSV1			(0x3604)
#define HDCPREG_ANCONF			(0x3608)
#define HDCPREG_AN0			(0x360C)
#define HDCPREG_AN1			(0x3610)
#define HDCPREG_RMLCTL			(0x3614)
#define HDCPREG_RMLSTS			(0x3618)
#define HDCPREG_SEED			(0x361C)
#define HDCPREG_DPK0			(0x3620)
#define HDCPREG_DPK1			(0x3624)
#define HDCP22GPIOSTS			(0x3628)
#define HDCP22GPIOCHNGSTS		(0x362C)
#define HDCPREG_DPK_CRC			(0x3630)

/* HDCP Configures HDCP controller Register bits filed */
#define HDCPCFG_DPCD12PLUS		(1<<7)
#define HDCPCFG_CP_IRQ			(1<<6)
#define HDCPCFG_BYPENCRYPTION		(1<<5)
#define HDCPCFG_HDCP_LOCK		(1<<4)
#define HDCPCFG_ENCRYPTIONDISABLE	(1<<3)
#define HDCPCFG_ENABLE_HDCP_13		(1<<2)
#define HDCPCFG_ENABLE_HDCP		(1<<1)

/* HDCP Observation Register bits filed */
#define HDCPOBS_BSTATUS_OFFSET		(19)
#define HDCPOBS_BSTATUS_MASK		(0xF)
#define HDCPOBS_BCAPS_OFFSET		(17)
#define HDCPOBS_BCAPS_MASK		(0x3)

/* Interrupt bits field */
#define INT_HDCP22_GPIO			(1<<8)
#define INT_HDCP_ENGAGED		(1<<7)
#define INT_HDCP_FAILED			(1<<6)
#define INT_KSVSHA1CALCDONE		(1<<5)
#define INT_AUXRESPNACK7TIMES		(1<<4)
#define INT_AUXRESPTIMEOUT		(1<<3)
#define INT_AUXRESPDEFER7TIMES		(1<<2)
#define INT_KSVACCESS			(1<<0)
#define INT_MASK_VALUE			(0x1FF)
#ifdef HDCP22_INT_USED
#define INT_UNMASK_VALUE		(0x000)
#else
#define INT_UNMASK_VALUE		(0x100)
#endif


#define HDCP_MODE_MANUAL		(0)
#define HDCP_MODE_AUTO			(1)

#define HDCP_CONTROL_DISABLE		(0)
#define HDCP_CONTROL_ENABLE		(1)

#define HDCP_PROTOCOL_NONE		(0x00)
#define HDCP_PROTOCOL_HDCP_1_X		(0x01)
#define HDCP_PROTOCOL_HDCP_2_2		(0x02)

#define HDCP_CONTENT_TYPE0		(0x00)
#define HDCP_CONTENT_TYPE1		(0x01)

struct hdcp_device {
	void __iomem	*reg;
	struct device	*dev;
	uint8_t		open_cs;	/* dev open count */
	uint8_t		status;		/* hdcp status */
	uint8_t		mode;		/* 0[manual], 1[auto] */
	uint8_t		protocol;	/* 0[none], 1[1.3] 2[2.2] */
	uint8_t		control;	/* 0[none], 1[dis], 2[en] */
	uint8_t		req_srm;	/* srm update is requested */
};

static struct hdcp2_driver *hdcp2_drv;

static uint8_t hdcp_get_bcaps(struct hdcp_device *hdcp)
{
	uint8_t bcaps;

	if (!hdcp)
		return 0;

	/*
	 * HDCP on DP Bcaps
	 *
	 * bits 7-2: Reserved (must be zero)
	 * bit 1: REPEATER, HDCP Repeater capability
	 * bit 0: HDCP_CAPABLE
	 */
	bcaps = (ioread32(hdcp->reg + HDCPOBS) >> HDCPOBS_BCAPS_OFFSET)
		      & HDCPOBS_BCAPS_MASK;

	return bcaps;
}

static uint16_t hdcp_get_bstatus(struct hdcp_device *hdcp)
{
	uint16_t bstatus;

	if (!hdcp)
		return 0;

	/*
	 * HDCP on DP Bstatus
	 *
	 * bits 7-4: Reserved. Read as zero
	 * bit 3: REAUTHENTICATION_REQUEST
	 * bit 2: LINK_INTEGRITY_FAILURE
	 * bit 1: R0'_AVAILABLE
	 * bit 0: READY
	 */
	bstatus = (ioread32(hdcp->reg + HDCPOBS)
			  >> HDCPOBS_BSTATUS_OFFSET) & HDCPOBS_BSTATUS_MASK;

	return bstatus;
}

static int hdcp_send_srm_list(struct hdcp_device *hdcp, uint8_t *buf,
			      uint32_t size)
{
	if (!hdcp || !buf || (size == 0))
		return -EINVAL;

	return 0;
}

static int hdcp_control(struct hdcp_device *hdcp, uint8_t control)
{
	uint32_t val;
	int ret = -EPERM;

	if (!hdcp)
		return -EINVAL;

	if (hdcp->control == control)
		return 0;

	if (control == HDCP_CONTROL_ENABLE) {
		if (hdcp->protocol == HDCP_PROTOCOL_HDCP_1_X) {
			val = ioread32(hdcp->reg + HDCPCFG);
			iowrite32(val | HDCPCFG_ENABLE_HDCP,
				  hdcp->reg + HDCPCFG);
			ret = 0;
		} else if (hdcp->protocol == HDCP_PROTOCOL_HDCP_2_2) {
			if (hdcp2_drv->enable && hdcp2_drv)
				ret = hdcp2_drv->enable();
		}
	} else if (control == HDCP_CONTROL_DISABLE) {
		if (hdcp->protocol == HDCP_PROTOCOL_HDCP_1_X) {
			val = ioread32(hdcp->reg + HDCPCFG);
			iowrite32(val & ~HDCPCFG_ENABLE_HDCP,
				  hdcp->reg + HDCPCFG);
			dev_info(hdcp->dev, "HDCP_DISABLED\n");
			ret = 0;
		} else if (hdcp->protocol == HDCP_PROTOCOL_HDCP_2_2) {
			if (hdcp2_drv->disable && hdcp2_drv)
				ret = hdcp2_drv->disable();
		}
	}

	if (ret == 0)
		hdcp->control = control;

	return ret;
}

static int hdcp_set_protocol(struct hdcp_device *hdcp, uint32_t protocol)
{
	uint32_t hdcpcfg_val;
	uint32_t val;

	if (!hdcp)
		return -EINVAL;

	if (protocol == hdcp->protocol)
		return 0;

	switch (protocol) {
	case HDCP_PROTOCOL_HDCP_1_X:
		hdcpcfg_val = HDCPCFG_DPCD12PLUS | HDCPCFG_ENABLE_HDCP_13;
		break;
	case HDCP_PROTOCOL_HDCP_2_2:
		/* must be eanble hdcp */
		hdcpcfg_val = HDCPCFG_DPCD12PLUS | HDCPCFG_ENABLE_HDCP;
		break;
	default:
		return -EINVAL;
	}

	if (hdcp2_drv->deinit && hdcp2_drv)
		hdcp2_drv->deinit();

	/* software reset */
	val = ioread32(hdcp->reg + SWRESET);
	iowrite32(val | SWRESET_HDCP, hdcp->reg + SWRESET);
	udelay(10); /* at least 5usec to insure a successful reset. */
	iowrite32(val & ~SWRESET_HDCP, hdcp->reg + SWRESET);

	/* clear interrupts */
	iowrite32(INT_MASK_VALUE, hdcp->reg + HDCPAPIINTCLR);

	/* unmask insterrupt */
	iowrite32(INT_UNMASK_VALUE, hdcp->reg + HDCPAPIINTMSK);

	/* set protocol */
	iowrite32(hdcpcfg_val, hdcp->reg + HDCPCFG);
	if (protocol == HDCP_PROTOCOL_HDCP_2_2) {
		if (hdcp2_drv->init && hdcp2_drv)
			hdcp2_drv->init();
	}
	hdcp->status = HDCP_STATUS_IDLE;
	hdcp->protocol = protocol;

	if (hdcp->control == HDCP_CONTROL_ENABLE) {
		hdcp_control(hdcp, HDCP_CONTROL_DISABLE);
		hdcp_control(hdcp, HDCP_CONTROL_ENABLE);
	}

	return 0;
}

static int hdcp_get_rxversion(struct hdcp_device *hdcp)
{
	if (!hdcp)
		return -EINVAL;

	return -EPERM;
}

static int hdcp_set_content_stream_type(struct hdcp_device *hdcp,
					uint32_t content_type)
{
	if (!hdcp)
		return -EINVAL;

	return -EPERM;
}

static irqreturn_t hdcp_irq(int irq, void *dev_id)
{
	struct hdcp_device *hdcp = dev_id;

	if (hdcp) {
		if (ioread32(hdcp->reg + HDCPAPIINTSTAT) & ~INT_UNMASK_VALUE) {
			/* mask interrupt */
			iowrite32(INT_MASK_VALUE, hdcp->reg + HDCPAPIINTMSK);
			return IRQ_WAKE_THREAD;
		}
	} else {
		dev_err(hdcp->dev, "hdcp resource is not exist\n");
	}

	return IRQ_NONE;
}

static irqreturn_t hdcp_isr(int irq, void *dev_id)
{
	struct hdcp_device *hdcp = dev_id;
	uint32_t int_stat;

	if (!hdcp)
		return IRQ_NONE;

	int_stat = ioread32(hdcp->reg + HDCPAPIINTSTAT);

	/* clear interrupt signals */
	iowrite32(int_stat, hdcp->reg + HDCPAPIINTCLR);

	// TODO: Check HPD and rxsense

#ifdef HDCP22_INT_USED
	if (int_stat & INT_HDCP22_GPIO) {
		uint32_t hdcp22_gpio = ioread32(hdcp->reg + HDCP22GPIOCHNGSTS);

		iowrite32(hdcp22_gpio, hdcp->reg + HDCP22GPIOCHNGSTS);
		dev_dbg(hdcp->dev, "HDCP22: sts:0x%x\n", hdcp22_gpio);

		if (hdcp22_gpio & (1<<0)) {
			dev_info(hdcp->dev, "HDCP22: %s.\n",
				 "Capable");
		}
		if (hdcp22_gpio & (1<<1)) {
			dev_info(hdcp->dev, "HDCP22: %s.\n",
				 "Does not support");
		}
		if (hdcp22_gpio & (1<<2)) {
			dev_info(hdcp->dev, "HDCP22: %s.\n",
				 "Successfully completed authentication");
		}
		if (hdcp22_gpio & (1<<3)) {
			dev_info(hdcp->dev, "HDCP22: %s.\n",
				 "Not able to authenticate");
		}
		if (hdcp22_gpio & (1<<4)) {
			dev_info(hdcp->dev, "HDCP22: %s.\n",
				 "Cipher syncronization has been lost");
		}
		if (hdcp22_gpio & (1<<5)) {
			dev_info(hdcp->dev, "HDCP22: %s.\n",
				 "receiver is requesting re-authentication");
		}
	}
#endif

	if (int_stat & INT_HDCP_ENGAGED) {
		dev_info(hdcp->dev, "HDCP_ENGAGED\n");
		hdcp->status = HDCP_STATUS_1_x_AUTENTICATED;
	} else if (int_stat & INT_HDCP_FAILED) {
		dev_info(hdcp->dev, "HDCP_FAILED\n");
		hdcp->status = HDCP_STATUS_1_x_AUTHENTICATION_FAILED;
	} else if (int_stat & INT_KSVSHA1CALCDONE) {
		dev_info(hdcp->dev, "KSVSHA1CALCDONE\n");
	} else if (int_stat & INT_AUXRESPNACK7TIMES) {
		dev_info(hdcp->dev, "AUXRESPNACK7TIMES\n");
	} else if (int_stat & INT_AUXRESPTIMEOUT) {
		dev_info(hdcp->dev, "AUXRESPTIMEOUT\n");
	} else if (int_stat & INT_AUXRESPDEFER7TIMES) {
		dev_info(hdcp->dev, "AUXRESPDEFER7TIMES\n");
	} else if (int_stat & INT_KSVACCESS) {
		dev_info(hdcp->dev, "KSVACCESS\n");
	}

	/* unmask insterrupt */
	iowrite32(INT_UNMASK_VALUE, hdcp->reg + HDCPAPIINTMSK);

	return IRQ_HANDLED;
}

/*
 * sysfs mode
 */
static ssize_t mode_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);
	ssize_t ret;

	if (!hdcp)
		return 0;

	if (hdcp->mode)
		ret = sprintf(buf, "auto\n");
	else
		ret = sprintf(buf, "manual\n");

	return ret;
}
static ssize_t mode_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);

	if (!hdcp)
		return strnlen(buf, PAGE_SIZE);

	if (!strncmp(buf, "auto", 4))
		hdcp->mode = HDCP_MODE_AUTO;
	else if (!strncmp(buf, "manual", 6))
		hdcp->mode = HDCP_MODE_MANUAL;

	return strnlen(buf, PAGE_SIZE);
}
static DEVICE_ATTR_RW(mode);

/*
 * sysfs protocol
 */
static ssize_t protocol_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);
	ssize_t ret;

	if (!hdcp)
		return 0;

	switch (hdcp->protocol) {
	case HDCP_PROTOCOL_HDCP_1_X:
		ret = snprintf(buf, 5, "1.3\n");
		break;
	case HDCP_PROTOCOL_HDCP_2_2:
		ret = snprintf(buf, 5, "2.2\n");
		break;
	default:
		ret = snprintf(buf, 6, "none\n");
		break;
	}

	return ret;
}
static ssize_t protocol_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);
	uint8_t protocol;

	if (!hdcp)
		return strnlen(buf, PAGE_SIZE);

	if (!strncmp(buf, "1.3", 3))
		protocol = HDCP_PROTOCOL_HDCP_1_X;
	else if (!strncmp(buf, "2.2", 3))
		protocol = HDCP_PROTOCOL_HDCP_2_2;
	else
		protocol = HDCP_PROTOCOL_NONE;

	hdcp_set_protocol(hdcp, protocol);

	return strnlen(buf, PAGE_SIZE);
}
static DEVICE_ATTR_RW(protocol);

/*
 * sysfs control
 */
static ssize_t control_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);

	if (!hdcp)
		return 0;

	switch (hdcp->control) {
	case HDCP_CONTROL_DISABLE:
		return sprintf(buf, "disable\n");
	case HDCP_CONTROL_ENABLE:
		return sprintf(buf, "enable\n");
	}

	return 0;
}
static ssize_t control_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);
	uint8_t control = 0xFF;

	if (!hdcp)
		return strnlen(buf, PAGE_SIZE);

	if (hdcp->mode != HDCP_MODE_MANUAL) {
		dev_warn(hdcp->dev, "Please set the mode to manual.\n");
		return strnlen(buf, PAGE_SIZE);
	}

	if (!strncmp(buf, "enable", 6))
		control = HDCP_CONTROL_ENABLE;
	else if (!strncmp(buf, "disable", 7))
		control = HDCP_CONTROL_DISABLE;

	hdcp_control(hdcp, control);

	return strnlen(buf, PAGE_SIZE);
}
static DEVICE_ATTR_RW(control);

/*
 * sysfs status
 */
static ssize_t status_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);
	ssize_t ret;

	if (!hdcp)
		return 0;

	if (hdcp->protocol == HDCP_PROTOCOL_HDCP_2_2) {
		if (hdcp2_drv->status && hdcp2_drv)
			hdcp->status = hdcp2_drv->status();
	}

	switch (hdcp->status) {
	case HDCP_STATUS_IDLE:
		ret = sprintf(buf, "Idle.\n");
		break;
	case HDCP_STATUS_1_x_AUTENTICATED:
		ret = sprintf(buf, "Authenticated.(1.3)\n");
		break;
	case HDCP_STATUS_1_x_AUTHENTICATION_FAILED:
		ret = sprintf(buf, "Authentication failed.(1.3)\n");
		break;
	case HDCP_STATUS_1_x_ENCRYPTED:
		ret = sprintf(buf, "Encrypted.(1.3)\n");
		break;
	case HDCP_STATUS_1_x_ENCRYPT_DISABLED:
		ret = sprintf(buf, "Encrypt disabled.(1.3)\n");
		break;
	case HDCP_STATUS_2_2_AUTHENTICATED:
		ret = sprintf(buf, "Authenticated.(2.2)\n");
		break;
	case HDCP_STATUS_2_2_AUTHENTICATION_FAILED:
		ret = sprintf(buf, "Authentication failed.(2.2)\n");
		break;
	case HDCP_STATUS_2_2_ENCRYPTED:
		ret = sprintf(buf, "Encrypted.(2.2)\n");
		break;
	case HDCP_STATUS_2_2_ENCRYPT_DISABLED:
		ret = sprintf(buf, "Encrypt disabled.(2.2)\n");
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}
static DEVICE_ATTR_RO(status);

/*
 * sysfs content_type
 */
static ssize_t content_type_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);

	if (!hdcp)
		return 0;

	return 0;
}
static ssize_t content_type_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);

	if (!hdcp)
		return strnlen(buf, PAGE_SIZE);

	hdcp_set_content_stream_type(hdcp, 0);
	return strnlen(buf, PAGE_SIZE);
}
static DEVICE_ATTR_RW(content_type);

/*
 * sysfs srm
 */
static ssize_t send_srm_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);

	if (!hdcp)
		goto exit;

	if (hdcp->protocol == HDCP_PROTOCOL_NONE) {
		dev_warn(hdcp->dev, "Please set protocol before send srm.\n");
		goto exit;
	}

	dev_info(hdcp->dev, "srm file: %s\n", buf);
	hdcp_send_srm_list(hdcp, NULL, 0);

exit:
	return strnlen(buf, PAGE_SIZE);
}
static DEVICE_ATTR_WO(send_srm);

/*
 * sysfs bcaps
 */
static ssize_t bcaps_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);

	if (!hdcp)
		return 0;

	if (hdcp->protocol == HDCP_PROTOCOL_HDCP_2_2) {
		dev_info(hdcp->dev, "Not supported at HDCP 2.2");
		return 0;
	}

	return sprintf(buf, "0x%x\n", hdcp_get_bcaps(hdcp));
}
static DEVICE_ATTR_RO(bcaps);

/*
 * sysfs bstatus
 */
static ssize_t bstatus_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);

	if (!hdcp)
		return 0;

	if (hdcp->protocol == HDCP_PROTOCOL_HDCP_2_2) {
		dev_info(hdcp->dev, "Not supported at HDCP 2.2");
		return 0;
	}

	return sprintf(buf, "0x%x\n", hdcp_get_bstatus(hdcp));
}
static DEVICE_ATTR_RO(bstatus);

/*
 * sysfs rx_version
 */
static ssize_t rx_version_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct hdcp_device *hdcp = dev_get_drvdata(dev->parent);
	int version;

	if (!hdcp)
		return 0;

	version = hdcp_get_rxversion(hdcp);

	return 0;
}
static DEVICE_ATTR_RO(rx_version);

static int hdcp_create_sysfs(struct device *dev)
{
	int ret;

	ret = device_create_file(dev, &dev_attr_mode);
	if (ret)
		goto exit;

	ret = device_create_file(dev, &dev_attr_protocol);
	if (ret)
		goto exit;

	ret = device_create_file(dev, &dev_attr_control);
	if (ret)
		goto exit;

	ret = device_create_file(dev, &dev_attr_status);
	if (ret)
		goto exit;

	ret = device_create_file(dev, &dev_attr_content_type);
	if (ret)
		goto exit;

	ret = device_create_file(dev, &dev_attr_send_srm);
	if (ret)
		goto exit;

	ret = device_create_file(dev, &dev_attr_bcaps);
	if (ret)
		goto exit;

	ret = device_create_file(dev, &dev_attr_bstatus);
	if (ret)
		goto exit;

	ret = device_create_file(dev, &dev_attr_rx_version);

exit:
	return ret;
}

static void hdcp_destroy_sysfs(struct device *dev)
{
	device_remove_file(dev, &dev_attr_mode);
	device_remove_file(dev, &dev_attr_protocol);
	device_remove_file(dev, &dev_attr_control);
	device_remove_file(dev, &dev_attr_status);
	device_remove_file(dev, &dev_attr_content_type);
	device_remove_file(dev, &dev_attr_send_srm);
	device_remove_file(dev, &dev_attr_bcaps);
	device_remove_file(dev, &dev_attr_bstatus);
	device_remove_file(dev, &dev_attr_rx_version);
}

static struct miscdevice misc_hdcp_driver = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hdcp",
};

static int hdcp_probe(struct platform_device *pdev)
{
	struct hdcp_device *hdcp;
	int ret, irq;

	hdcp = kzalloc(sizeof(struct hdcp_device), GFP_KERNEL);
	if (!hdcp)
		return -ENOMEM;

	hdcp->dev = &pdev->dev;
	hdcp->reg = of_iomap(pdev->dev.of_node, 0);
	if (!hdcp->reg) {
		dev_err(&pdev->dev, "failed to get controller\n");
		ret = -ENODEV;
		goto err;
	}

	/* hdcp irq shared with dp */
	irq = platform_get_irq(pdev, 0);
	ret = devm_request_threaded_irq(&pdev->dev, irq, hdcp_irq, hdcp_isr,
					IRQF_SHARED, "hdcp", hdcp);
	if (ret) {
		dev_err(&pdev->dev, "Could not register interrupt\n");
		goto err;
	}

	misc_hdcp_driver.parent = &pdev->dev;
	ret = misc_register(&misc_hdcp_driver);
	if (ret) {
		dev_err(&pdev->dev, "failed to register misc device.\n");
		goto err;
	}

	ret = hdcp_create_sysfs(misc_hdcp_driver.this_device);
	if (ret)
		goto err_create_sysfs;

	platform_set_drvdata(pdev, hdcp);
	dev_info(&pdev->dev, "driver probed. (ver: %s)\n", HDCP_DRV_VERSION);

	return 0;

err_create_sysfs:
	hdcp_destroy_sysfs(misc_hdcp_driver.this_device);
	misc_deregister(&misc_hdcp_driver);

err:
	kfree(hdcp);

	return ret;
}

static int hdcp_remove(struct platform_device *pdev)
{
	struct hdcp_device *hdcp = platform_get_drvdata(pdev);

	hdcp_destroy_sysfs(misc_hdcp_driver.this_device);
	misc_deregister(&misc_hdcp_driver);
	kfree(hdcp);
	hdcp2_drv = NULL;

	return 0;
}

static int hdcp_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct hdcp_device *hdcp = platform_get_drvdata(pdev);

	/* clear interrupts */
	iowrite32(INT_MASK_VALUE, hdcp->reg + HDCPAPIINTCLR);

	/* mask insterrupt */
	iowrite32(INT_MASK_VALUE, hdcp->reg + HDCPAPIINTMSK);

	return 0;
}

static int hdcp_resume(struct platform_device *pdev)
{
	struct hdcp_device *hdcp = platform_get_drvdata(pdev);

	/* clear interrupts */
	iowrite32(INT_MASK_VALUE, hdcp->reg + HDCPAPIINTCLR);

	/* unmask insterrupt */
	iowrite32(INT_UNMASK_VALUE, hdcp->reg + HDCPAPIINTMSK);

	return 0;
}

/*
 * This API is called by snps-hld module.
 */
int hdcp2_set_driver(struct hdcp2_driver *driver)
{
	if ((hdcp2_drv != driver) && hdcp2_drv)
		return -EBUSY;

	hdcp2_drv = driver;
	return 0;
}
EXPORT_SYMBOL(hdcp2_set_driver);

int hdcp2_release_driver(struct hdcp2_driver *driver)
{
	if (hdcp2_drv == driver)
		hdcp2_drv = NULL;
	else
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(hdcp2_release_driver);

static const struct of_device_id hdcp_of_match[] = {
	{ .compatible =	"snps,hdcp-dp" },
	{ }
};
MODULE_DEVICE_TABLE(of, hdcp_of_match);

static struct platform_driver __refdata snps_hdcp_dp_driver = {
	.driver		= {
		.name	= "snps,hdcp-dp",
		.of_match_table = hdcp_of_match,
	},
	.probe		= hdcp_probe,
	.remove		= hdcp_remove,
	.suspend	= hdcp_suspend,
	.resume		= hdcp_resume,
};
module_platform_driver(snps_hdcp_dp_driver);

MODULE_VERSION(HDCP_DRV_VERSION);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("HDCP for Synopsys DesignWare Cores DisplayPort");
MODULE_LICENSE("GPL");

// SPDX-License-Identifier: GPL-2.0-or-later
/*******************************************************************************
 *
 * Copyright (C) 2010 Telechips Inc.
 *
 ******************************************************************************/

#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#if defined(CONFIG_OF)
#include <linux/of_gpio.h>
#endif/* defined(CONFIG_OF) */

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <uapi/misc/switch.h>

static int				debug;

#define MODULE_NAME			("switch_gpio_reverse")

#define LOG_TAG				("SWITCH")

/* coverity[misra_c_2012_rule_20_10_violation : SUPPRESS] */
#define loge(fmt, args...)		\
	{ (void)pr_err("[ERROR][%s] %s - "  fmt, LOG_TAG, __func__, ##args); }
/* coverity[misra_c_2012_rule_20_10_violation : SUPPRESS] */
#define logw(fmt, args...)		\
	{ (void)pr_warn("[WARN][%s] %s - "  fmt, LOG_TAG, __func__, ##args); }
/* coverity[misra_c_2012_rule_20_10_violation : SUPPRESS] */
#define logd(fmt, args...)		\
	{ (void)pr_info("[DEBUG][%s] %s - " fmt, LOG_TAG, __func__, ##args); }
/* coverity[misra_c_2012_rule_20_10_violation : SUPPRESS] */
#define logi(fmt, args...)		\
	{ (void)pr_info("[INFO][%s] %s - "  fmt, LOG_TAG, __func__, ##args); }
/* coverity[misra_c_2012_rule_20_10_violation : SUPPRESS] */
#define dlog(fmt, args...)		\
	{ do { if (debug > 0) { ; logd(fmt, ##args); } } while (false); }

struct switch_dev {
	dev_t				chrdev_region;
	struct class			*chrdev_class;
	struct cdev			chrdev;

	unsigned int			enabled;
	atomic_t			type;
	int				switch_gpio;
	int				switch_active;
	atomic_t			status;

	atomic_t			loglevel;
};

static int switch_get_type(const struct switch_dev *vdev)
{
	int				type		= 0;

	type = atomic_read(&vdev->type);

	return type;
}

static int switch_set_type(struct switch_dev *vdev, int type)
{
	int				ret		= 0;

	atomic_set(&vdev->type, type);

	return ret;
}

static ssize_t switch_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/* avoid MISRA C-2012 Rule 8.13 */
	struct device			*tmpdev		= NULL;
	struct device_attribute		*tmpattr	= NULL;

	const struct switch_dev		*vdev		= NULL;
	int				type		= 0;
	ssize_t				ret		= 0;

	/* avoid MISRA C-2012 Rule 2.7 */
	(void)attr;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpdev = dev;
	dev = tmpdev;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpattr = attr;
	attr = tmpattr;

	/* coverity[misra_c_2012_rule_11_5_violation : SUPPRESS] */
	vdev	= (struct switch_dev *)dev->platform_data;

	type	= switch_get_type(vdev);
	ret	= scnprintf(buf, PAGE_SIZE, "%d\n", type);
	if (ret < 0) {
		/* failure of scnprintf */
		loge("scnprintf, ret: %zd\n", ret);
	}

	return ret;
}

static ssize_t switch_type_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* avoid MISRA C-2012 Rule 8.13 */
	struct device			*tmpdev		= NULL;
	struct device_attribute		*tmpattr	= NULL;

	struct switch_dev		*vdev		= NULL;
	int				type		= 0;
	int				ret		= 0;
	ssize_t				retcnt		= 0;

	/* avoid MISRA C-2012 Rule 2.7 */
	(void)attr;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpdev = dev;
	dev = tmpdev;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpattr = attr;
	attr = tmpattr;

	/* coverity[misra_c_2012_rule_11_5_violation : SUPPRESS] */
	vdev = (struct switch_dev *)dev->platform_data;

	ret = kstrtoint(buf, 10, &type);
	if (ret != 0) {
		/* failure of kstrtoul */
		retcnt = 0;
	} else {
		ret = switch_set_type(vdev, type);

		/* coverity[misra_c_2012_rule_10_4_violation : SUPPRESS] */
		/* coverity[misra_c_2012_rule_10_8_violation : SUPPRESS] */
		/* coverity[misra_c_2012_rule_14_3_violation : SUPPRESS] */
		retcnt = (count > LONG_MAX) ? 0: (ssize_t)count;
	}

	return retcnt;
}

/* coverity[misra_c_2012_rule_6_1_violation : SUPPRESS] */
/* coverity[misra_c_2012_rule_8_4_violation : SUPPRESS] */
/* coverity[misra_c_2012_rule_10_3_violation : SUPPRESS] */
DEVICE_ATTR_RW(switch_type);

static int switch_get_status(struct switch_dev *vdev)
{
	int				type		= 0;
	int				gpio_num	= 0;
	int				gpio_value	= 0;
	int				gpio_active	= 0;
	int				status		= 0;

	/* get the switch type */
	type = switch_get_type(vdev);

	switch (type) {
	/* sw switch */
	case 0:
		status = atomic_read(&vdev->status);
		break;

	/* hw switch */
	case 1:
		gpio_num = vdev->switch_gpio;
		if (gpio_num != -1) {
			gpio_value	= gpio_get_value((unsigned int)gpio_num);
			gpio_active	= vdev->switch_active;
			if (gpio_active == -1) {
				/* set gpio_value to status as an initialization */
				status = gpio_value;
			} else {
				/* set status value */
				status = (gpio_value == gpio_active) ? 1 : 0;
			}
			dlog("gpio_num: %d, value: %d, active: %d, status: %d\n",
				gpio_num, gpio_value, gpio_active, status);

			atomic_set(&vdev->status, status);
		} else {
			loge("HW Switch is not supported, gpio_num: %d\n", gpio_num);
			status = atomic_read(&vdev->status);
		}
		break;

	default:
		loge("switch type(%d) is WRONG\n", type);
		break;
	}

	dlog("switch status: %d\n", status);

	return status;
}

static int switch_set_status(struct switch_dev *vdev, int status)
{
	int				type		= 0;
	int				ret		= 0;

	/* get the switch type */
	type = switch_get_type(vdev);

	switch (type) {
	/* sw switch */
	case 0:
		atomic_set(&vdev->status, status);
		break;

	/* hw switch */
	case 1:
		logd("Not supported in case of switch type(%d)\n", type);
		break;

	default:
		loge("switch type(%d) is WRONG\n", type);
		break;
	}

	logd("switch status: %d\n", status);

	return ret;
}

static ssize_t switch_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/* avoid MISRA C-2012 Rule 8.13 */
	struct device			*tmpdev		= NULL;
	struct device_attribute		*tmpattr	= NULL;

	const struct switch_dev		*vdev		= NULL;
	int				status		= 0;
	ssize_t				ret		= 0;

	/* avoid MISRA C-2012 Rule 2.7 */
	(void)attr;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpdev = dev;
	dev = tmpdev;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpattr = attr;
	attr = tmpattr;

	/* coverity[misra_c_2012_rule_11_5_violation : SUPPRESS] */
	vdev	= (struct switch_dev *)dev->platform_data;
	status	= atomic_read(&vdev->status);
	ret	= scnprintf(buf, PAGE_SIZE, "%d\n", status);
	if (ret < 0) {
		/* failure of scnprintf */
		loge("scnprintf, ret: %zd\n", ret);
	}

	return ret;
}

static ssize_t switch_status_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* avoid MISRA C-2012 Rule 8.13 */
	struct device			*tmpdev		= NULL;
	struct device_attribute		*tmpattr	= NULL;

	struct switch_dev		*vdev		= NULL;
	int				status		= 0;
	int				ret		= 0;
	ssize_t				retcnt		= 0;

	/* avoid MISRA C-2012 Rule 2.7 */
	(void)attr;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpdev = dev;
	dev = tmpdev;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpattr = attr;
	attr = tmpattr;

	/* coverity[misra_c_2012_rule_11_5_violation : SUPPRESS] */
	vdev = (struct switch_dev *)dev->platform_data;
	ret = kstrtoint(buf, 10, &status);
	if (ret != 0) {
		/* return if error occurs */
		retcnt = 0;
	} else {
		ret = switch_set_status(vdev, status);

		/* coverity[misra_c_2012_rule_10_4_violation : SUPPRESS] */
		/* coverity[misra_c_2012_rule_10_8_violation : SUPPRESS] */
		/* coverity[misra_c_2012_rule_14_3_violation : SUPPRESS] */
		retcnt = (count > LONG_MAX) ? 0: (ssize_t)count;
	}

	return retcnt;
}

/* coverity[misra_c_2012_rule_6_1_violation : SUPPRESS] */
/* coverity[misra_c_2012_rule_8_4_violation : SUPPRESS] */
/* coverity[misra_c_2012_rule_10_3_violation : SUPPRESS] */
DEVICE_ATTR_RW(switch_status);

static ssize_t loglevel_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/* avoid MISRA C-2012 Rule 8.13 */
	struct device			*tmpdev		= NULL;
	struct device_attribute		*tmpattr	= NULL;

	const struct switch_dev		*vdev		= NULL;
	int				level		= 0;
	ssize_t				ret		= 0;

	/* avoid MISRA C-2012 Rule 2.7 */
	(void)attr;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpdev = dev;
	dev = tmpdev;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpattr = attr;
	attr = tmpattr;

	/* coverity[misra_c_2012_rule_11_5_violation : SUPPRESS] */
	vdev	= (struct switch_dev *)dev->platform_data;
	level	= atomic_read(&vdev->loglevel);
	ret	= scnprintf(buf, PAGE_SIZE, "%d\n", level);
	if (ret < 0) {
		/* failure of scnprintf */
		loge("scnprintf, ret: %zd\n", ret);
	}

	return ret;
}

static ssize_t loglevel_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* avoid MISRA C-2012 Rule 8.13 */
	struct device			*tmpdev		= NULL;
	struct device_attribute		*tmpattr	= NULL;

	struct switch_dev		*vdev		= NULL;
	int				level		= 0;
	int				ret		= 0;
	ssize_t				retcnt		= 0;

	/* avoid MISRA C-2012 Rule 2.7 */
	(void)attr;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpdev = dev;
	dev = tmpdev;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpattr = attr;
	attr = tmpattr;

	/* coverity[misra_c_2012_rule_11_5_violation : SUPPRESS] */
	vdev = (struct switch_dev *)dev->platform_data;
	ret = kstrtoint(buf, 10, &level);
	if (ret != 0) {
		/* return if error occurs */
		retcnt = 0;
	} else {
		atomic_set(&vdev->loglevel, level);
		debug = level;

		/* coverity[misra_c_2012_rule_10_4_violation : SUPPRESS] */
		/* coverity[misra_c_2012_rule_10_8_violation : SUPPRESS] */
		/* coverity[misra_c_2012_rule_14_3_violation : SUPPRESS] */
		retcnt = (count > LONG_MAX) ? 0: (ssize_t)count;
	}

	return retcnt;
}

/* coverity[misra_c_2012_rule_6_1_violation : SUPPRESS] */
/* coverity[misra_c_2012_rule_8_4_violation : SUPPRESS] */
/* coverity[misra_c_2012_rule_10_3_violation : SUPPRESS] */
DEVICE_ATTR_RW(loglevel);

static int switch_parse_device_tree(struct switch_dev *vdev, struct device *dev)
{
	struct device_node		*root_node	= NULL;
	const struct device_node	*dev_node	= NULL;
	const struct pinctrl		*pin_ctrl	= NULL;

	unsigned int			active		= 0;
	unsigned int			active_max	= 10;
	int				ret_call	= 0;
	int				ret		= 0;

	root_node		= dev->of_node;

	vdev->switch_gpio	= -1;
	vdev->switch_active	= -1;

	/* parse device tree */
	dev_node = of_parse_phandle(root_node, "switch-gpios", 0);
	if (dev_node == NULL) {
		logd("\"switch-gpios\" node is not found.\n");
		ret = -1;
	} else {
		vdev->switch_gpio = of_get_named_gpio(root_node, "switch-gpios", 0);
		ret_call = of_property_read_u32_index(root_node, "switch-active", 0, &active);
		if (active < active_max) {
			/* explicit cast */
			vdev->switch_active = (int)active;
		}
		logd("switch-gpios: %d, switch-active: %d\n",
			vdev->switch_gpio, vdev->switch_active);
	}

	/* pinctl */
	if (vdev->switch_gpio == -1) {
		pin_ctrl = pinctrl_get_select(dev, "default");
		if (IS_ERR_OR_NULL((const void *)pin_ctrl)) {
			/* print out the error */
			logd("\"pinctrl\" node is not found.\n");
			ret = -1;
		}
	}

	return ret;
}

static int switch_register_all_sysfs(struct device *dev)
{
	int				ret_call	= 0;
	int				ret		= 0;

	/* create a sysfs "type" */
	ret_call = device_create_file(dev, &dev_attr_switch_type);
	if (ret_call < 0) {
		/* print out the error */
		loge("failed create sysfs(type), ret: %d\n", ret);
		ret = -1;
	}

	/* create a sysfs "status" */
	ret_call = device_create_file(dev, &dev_attr_switch_status);
	if (ret_call < 0) {
		/* print out the error */
		loge("failed create sysfs(status), ret: %d\n", ret);
		ret = -1;
	}

	/* create a sysfs "loglevel" */
	ret_call = device_create_file(dev, &dev_attr_loglevel);
	if (ret_call < 0) {
		/* print out the error */
		loge("failed create sysfs(loglevel), ret: %d\n", ret);
		ret = -1;
	}

	return ret;
}

static int switch_init_private_data(struct switch_dev *vdev)
{
	int				ret		= 0;

	/* set switch type */
	if (vdev->switch_gpio != -1) {
		/* Set type as 1 if there is a hw switch node */
		ret = switch_set_type(vdev, 1);
	}

	return ret;
}

static long switch_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg)
{
	/* avoid MISRA C-2012 Rule 8.13 */
	struct file			*tmpf		= NULL;

	struct switch_dev		*vdev		= NULL;
	int				status		= 0;
	unsigned long			byte		= 0;
	int				ret		= 0;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpf = pfile;
	pfile = tmpf;

	/* coverity[misra_c_2012_rule_11_5_violation : SUPPRESS] */
	vdev	= (struct switch_dev *)pfile->private_data;

	switch (cmd) {
	case SWITCH_IOCTL_CMD_ENABLE:
		vdev->enabled	= 1;
		logd("enabled: %u\n", vdev->enabled);
		break;

	case SWITCH_IOCTL_CMD_DISABLE:
		vdev->enabled	= 0;
		logd("enabled: %u\n", vdev->enabled);
		break;

	case SWITCH_IOCTL_CMD_GET_STATE:
		status		= switch_get_status(vdev);
		dlog("status: %d\n", status);

		/* coverity[misra_c_2012_rule_11_6_violation : SUPPRESS] */
		/* coverity[cert_int36_c_violation : SUPPRESS] */
		byte = copy_to_user((void *)arg, (const void *)&status, sizeof(status));
		if (byte != 0UL) {
			loge("FAILED: copy_to_user\n");
			ret = -1;
			break;
		}
		break;

	default:
		loge("FAILED: Unsupported command\n");
		ret = -1;
		break;
	}
	return ret;
}

static int switch_open(struct inode *pinode, struct file *pfile)
{
	/* avoid MISRA C-2012 Rule 8.13 */
	struct inode			*tmpinode	= NULL;

	struct switch_dev		*vdev		= NULL;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpinode = pinode;
	pinode = tmpinode;

	/* coverity[misra_c_2012_rule_8_5_violation : SUPPRESS] */
	/* coverity[misra_c_2012_rule_8_6_violation : SUPPRESS] */
	/* coverity[misra_c_2012_rule_8_13_violation : SUPPRESS] */
	/* coverity[misra_c_2012_rule_10_1_violation : SUPPRESS] */
	/* coverity[misra_c_2012_rule_11_5_violation : SUPPRESS] */
	/* coverity[misra_c_2012_rule_14_4_violation : SUPPRESS] */
	/* coverity[misra_c_2012_rule_15_6_violation : SUPPRESS] */
	/* coverity[misra_c_2012_rule_18_4_violation : SUPPRESS] */
	/* coverity[misra_c_2012_rule_20_7_violation : SUPPRESS] */
	/* coverity[misra_c_2012_rule_21_2_violation : SUPPRESS] */
	/* coverity[cert_dcl37_c_violation : SUPPRESS] */
	/* coverity[cert_arr39_c_violation : SUPPRESS] */
	vdev = container_of(pinode->i_cdev, struct switch_dev, chrdev);
	pfile->private_data = vdev;

	return 0;
}

static int switch_release(struct inode *pinode, struct file *pfile)
{
	/* avoid MISRA C-2012 Rule 8.13 */
	struct inode			*tmpinode	= NULL;

	/* avoid MISRA C-2012 Rule 2.7 */
	(void)pinode;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpinode = pinode;
	pinode = tmpinode;

	pfile->private_data = NULL;

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int switch_suspend(struct device *dev)
{
	/* avoid MISRA C-2012 Rule 8.13 */
	struct device			*tmpdev		= NULL;

	/* avoid MISRA C-2012 Rule 2.7 */
	(void)dev;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpdev = dev;
	dev = tmpdev;

	return 0;
}

static int switch_resume(struct device *dev)
{
	const struct pinctrl		*pin_ctrl	= NULL;

	pin_ctrl = pinctrl_get_select(dev, "default");
	if (IS_ERR((const void *)pin_ctrl)) {
		/* print out the error */
		logd("\"pinctrl\" node is not found.\n");
	}

	return 0;
}

static const struct dev_pm_ops swich_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS((switch_suspend), (switch_resume))
};

#define DEV_PM_OPS  (&swich_pm_ops)
#else
#define DEV_PM_OPS  (NULL)
#endif

static const struct file_operations switch_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= switch_ioctl,
	.open		= switch_open,
	.release	= switch_release,
};

static int switch_get_device_name(char **name, struct device_node *root_node)
{
	/* avoid MISRA C-2012 Rule 8.13 */
	char				**tmpname	= NULL;

	int				index		= 0;
	int				ret_call	= 0;
	int				ret		= 0;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmpname	= name;
	name = tmpname;

	/* get the index from its alias */
	index = of_alias_get_id(root_node, /*MODULE_NAME*/"switch");

	/* set the character device name */
	if ((index == -ENODEV) || (index == 0)) {
		/* if the device name is like "/dev/name" */
		ret_call = scnprintf(*name, PAGE_SIZE, "%s", MODULE_NAME);
	} else {
		/* if the device name is like "/dev/name{n}" */
		ret_call = scnprintf(*name, PAGE_SIZE, "%s%d", MODULE_NAME, index);
	}

	return ret;
}

static int switch_register_all_devices(struct switch_dev *vdev,
	struct device_node *root_node)
{
	const struct device		*dev_ret	= NULL;
	char				name[32]	= "";
	char				*pname		= NULL;
	int				ret_call	= 0;
	int				ret		= 0;

	/* get device name */
	pname = &name[0];
	ret_call = switch_get_device_name(&pname, root_node);
	if (ret_call < 0) {
		loge("switch_get_device_name, ret: %d\n", ret);
		ret = -1;
	}

	/* allocate a character device region */
	ret_call = alloc_chrdev_region(&vdev->chrdev_region, 0, 1, name);
	if (ret_call < 0) {
		loge("alloc_chrdev_region \"%s\"), ret: %d\n", name, ret);
		ret = -1;
	}

	if (ret >= 0) {
		/* create a class */
		/* coverity[cert_dcl37_c_violation : SUPPRESS] */
		vdev->chrdev_class = class_create(THIS_MODULE, name);
		if (vdev->chrdev_class == NULL) {
			loge("class_create \"%s\"\n", name);
			ret = -1;
		}
	}

	if (ret >= 0) {
		/* create a device file system */
		dev_ret = device_create(vdev->chrdev_class,
			NULL, vdev->chrdev_region, NULL, name);
		if (dev_ret == NULL) {
			loge("device_create \"%s\"\n", name);
			ret = -1;
		}
	}

	if (ret >= 0) {
		/* register a device as a character device */
		cdev_init(&vdev->chrdev, &switch_fops);
		ret_call = cdev_add(&vdev->chrdev, vdev->chrdev_region, 1);
		if (ret_call < 0) {
			loge("cdev_add \"%s\"\n", name);
			ret = -1;
		}
	}

	return ret;
}

static int switch_unregister_all_devices(struct switch_dev *vdev)
{
	int				ret		= 0;

	/* unregister the device */
	cdev_del(&vdev->chrdev);

	/* destroy the device file system */
	device_destroy(vdev->chrdev_class, vdev->chrdev_region);

	/* destroy the class */
	class_destroy(vdev->chrdev_class);

	/* unregister the character device region */
	unregister_chrdev_region(vdev->chrdev_region, 1);

	return ret;
}

static int switch_probe(struct platform_device *pdev)
{
	struct switch_dev		*vdev		= NULL;
	int				ret_call	= 0;
	int				ret		= 0;

	/* allocate memory for switch device data */
	/* coverity[misra_c_2012_rule_10_8_violation : SUPPRESS] */
	/* coverity[misra_c_2012_rule_11_5_violation : SUPPRESS] */
	vdev = kzalloc(sizeof(struct switch_dev), GFP_KERNEL);
	if (vdev == NULL) {
		loge("Allocate a videosource device struct.\n");
		ret = -1;
	} else {
		/* assign vdev as platform data */
		pdev->dev.platform_data = vdev;
	}

	if (ret >= 0) {
		/* parse device tree */
		ret_call = switch_parse_device_tree(vdev, &pdev->dev);
		if (ret_call < 0) {
			/* print out the error */
			logd("switch_parse_device_tree, ret: %d\n", ret);
			ret = -1;
		}
	}

	if (ret >= 0) {
		/* register all devices */
		ret_call = switch_register_all_devices(vdev, pdev->dev.of_node);
		if (ret_call < 0) {
			/* print out the error */
			loge("switch_register_all_devices, ret: %d\n", ret);
			ret = -1;
		}
	}

	if (ret >= 0) {
		/* register all sysfs */
		ret_call = switch_register_all_sysfs(&pdev->dev);
		if (ret_call < 0) {
			/* print out the error */
			loge("switch_register_all_sysfs, ret: %d\n", ret);
			ret = -1;
		}
	}

	if (ret >= 0) {
		/* init private data */
		ret_call = switch_init_private_data(vdev);
		if (ret_call < 0) {
			/* print out the error */
			loge("switch_init_private_data, ret: %d\n", ret);
			ret = -1;
		}
	}

	return ret;
}

static int switch_remove(struct platform_device *pdev)
{
	/* avoid MISRA C-2012 Rule 8.13 */
	struct platform_device		*tmppdev	= NULL;

	struct switch_dev		*vdev		= NULL;
	int				ret_call	= 0;
	int				ret		= 0;

	/* avoid MISRA C-2012 Rule 8.13 */
	tmppdev = pdev;
	pdev = tmppdev;

	/* coverity[misra_c_2012_rule_11_5_violation : SUPPRESS] */
	vdev = (struct switch_dev *)pdev->dev.platform_data;

	/* register all devices */
	ret_call = switch_unregister_all_devices(vdev);
	if (ret_call < 0) {
		/* print out the error */
		loge("switch_unregister_all_devices, ret: %d\n", ret);
		ret = -1;
	}

	/* free memory for switch device data */
	kfree(vdev);

	return ret;
}

#if defined(CONFIG_OF)
static const struct of_device_id switch_of_match[] = {
	{ .compatible = "telechips,switch", }
};

MODULE_DEVICE_TABLE(of, switch_of_match);
#endif/* defined(CONFIG_OF) */

static struct platform_driver switch_driver = {
	.probe		= switch_probe,
	.remove		= switch_remove,
	.driver		= {
		.name		= MODULE_NAME,
		.owner		= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table	= switch_of_match,
#endif/* defined(CONFIG_OF) */
		.pm		= DEV_PM_OPS,
	},
};

/* coverity[misra_c_2012_rule_21_2_violation : SUPPRESS] */
/* coverity[cert_dcl37_c_violation : SUPPRESS] */
module_platform_driver(switch_driver);

/* coverity[misra_c_2012_rule_21_2_violation : SUPPRESS] */
/* coverity[cert_dcl37_c_violation : SUPPRESS] */
MODULE_AUTHOR("Jay HAN<jjongspi@telechips.com>");
/* coverity[misra_c_2012_rule_21_2_violation : SUPPRESS] */
/* coverity[cert_dcl37_c_violation : SUPPRESS] */
MODULE_DESCRIPTION("SWITCH Driver");
/* coverity[misra_c_2012_rule_21_2_violation : SUPPRESS] */
/* coverity[cert_dcl37_c_violation : SUPPRESS] */
MODULE_LICENSE("GPL");

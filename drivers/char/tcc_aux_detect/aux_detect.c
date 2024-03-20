// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/types.h>

#define MODULE_NAME			("aux_detect")

#define AUX_IOCTL_CMD_GET_STATE		(0x10)

static dev_t			aux_detect_dev_region;
static struct class		*aux_detect_class;
static struct cdev		aux_detect_cdev;

struct aux_detect_data {
	int32_t aux_detect_gpio;
	uint32_t aux_active;
	int32_t enabled;
};

static struct aux_detect_data *pdata;

static int32_t pre_aux_status;

static ssize_t aux_detect_status_show(
		struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct aux_detect_data *aux_data = pdata;
	uint32_t aux_value = UINT_MAX;
	int32_t	value = -1;
	int32_t	ret	= -1;

	(void)dev;
	(void)attr;

	if ((aux_data->aux_detect_gpio != -1) &&
		(aux_data->aux_active != UINT_MAX)) {
		value = gpio_get_value((uint32_t)aux_data->aux_detect_gpio);

		if (value >= 0) {
			aux_value = (uint32_t)value;
		}
		if (aux_value == aux_data->aux_active) {
			ret = 1;
		} else {
			ret = 0;
		}

		pr_debug("gpio: %d, value: %u, active: %u, result: %d\n",
			aux_data->aux_detect_gpio,
			aux_value,
			aux_data->aux_active,
			ret);

		if (pre_aux_status != ret) {
			pr_notice("aux status change: %d -> %d\n",
				pre_aux_status, ret);
		}
		pre_aux_status = ret;

	} else {
		pr_err("HW Switch is not supported, gpio: %d, active: %d\n",
			aux_data->aux_detect_gpio,
			aux_data->aux_active);
		ret = 0;
	}

	pr_debug("aux detect status: %d\n", ret);

	return scnprintf(buf, sizeof(int32_t), "%d\n", ret);
}

static ssize_t aux_detect_status_store(
			struct device *dev,
			struct device_attribute *attr,
			const char *buf,
			size_t count)
{
	(void)dev;
	(void)attr;
	(void)buf;
	(void)count;
	pr_info("Not support write to aux detect");

	return 0;
}

static DEVICE_ATTR_RW(aux_detect_status);

static int32_t aux_detect_check_state(void)
{

	struct aux_detect_data	*aux_data = pdata;
	uint32_t aux_value = UINT_MAX;
	int32_t	value = -1;
	int32_t	ret	= -1;

	if ((aux_data->aux_detect_gpio != -1) &&
		(aux_data->aux_active != UINT_MAX)) {
		value = gpio_get_value((uint32_t)aux_data->aux_detect_gpio);

		if (value >= 0) {
			aux_value = (uint32_t)value;
		}
		if (aux_value == aux_data->aux_active) {
			ret = 1;
		} else {
			ret = 0;
		}

		pr_debug("gpio: %d, value: %d, active: %d, result: %d\n",
			aux_data->aux_detect_gpio,
			aux_value,
			aux_data->aux_active,
			ret);

		if (pre_aux_status != ret) {
			pr_notice("aux status change: %d -> %d\n",
				pre_aux_status, ret);
		}
		pre_aux_status = ret;
	} else {
		pr_err("HW Switch is not supported, gpio: %d, active: %d\n",
			aux_data->aux_detect_gpio,
			aux_data->aux_active);
	}

	pr_debug("aux detect status: %d\n", ret);

	return ret;
}

static long aux_detect_ioctl(
			struct file *filp,
			uint32_t cmd,
			ulong arg)
{

	struct aux_detect_data *aux_data = pdata;
	int32_t state = 0;
	int32_t ret = 0;

	(void)filp;
	(void)aux_data;

	switch (cmd) {

	case AUX_IOCTL_CMD_GET_STATE:
		state = (int32_t)aux_detect_check_state();

		if (copy_to_user((void __user *)arg,
				(const void *)&state,
				(ulong)sizeof(state))
				!= (ulong)0) {
			pr_err("FAILED: copy_to_user\n");
			ret = -1;
			break;
		}
		break;

	default:
		pr_err("FAILED: Unsupported command\n");
		ret = -1;
		break;
	}
	return ret;
}

static int32_t aux_detect_open(struct inode *aux_inode, struct file *filp)
{
	(void)aux_inode;
	(void)filp;

	return 0;
}

static int32_t aux_detect_release(struct inode *aux_inode, struct file *filp)
{
	(void)aux_inode;
	(void)filp;

	return 0;
}

static const struct file_operations aux_detect_fops = {
	.owner			= THIS_MODULE,
	.compat_ioctl	= aux_detect_ioctl,
	.unlocked_ioctl	= aux_detect_ioctl,
	.open			= aux_detect_open,
	.release		= aux_detect_release,
};

static int32_t aux_detect_create_cdev(struct platform_device *pdev)
{
	struct aux_detect_data	*aux_data	= NULL;
	struct pinctrl *aux_pinctrl = NULL;
	int32_t ret = 0;

	if (pdev != NULL) {

		/* Register  charactor device */
		cdev_init(&aux_detect_cdev, &aux_detect_fops);

		ret = cdev_add(&aux_detect_cdev,
			aux_detect_dev_region, 1);

		if (ret < 0) {
			pr_err("Register the %s device as a charactor device\n",
				(const char *)MODULE_NAME);
		} else {
			/* Allocate memory for aux detect Platform Data.*/
			aux_data = kzalloc(sizeof(struct aux_detect_data),
					GFP_KERNEL);
			if (aux_data == NULL) {
				ret = -ENOMEM;
			}
		}

		if (ret > -1) {

			pdata = aux_data;

			pdev->dev.platform_data = aux_data;

			/* pinctrl */
			aux_pinctrl = pinctrl_get_select(&pdev->dev, "default");
			if (IS_ERR(aux_pinctrl)) {
				pr_err("%s: pinctrl select failed\n",
					(const char *)MODULE_NAME);
				ret = -1;
			} else {
				u32 out_value;

				pinctrl_put(aux_pinctrl);
				aux_data->aux_detect_gpio = -1;
				aux_data->aux_active = UINT_MAX;

				if (of_parse_phandle(
						pdev->dev.of_node,
						"aux-gpios",
						0)
					!= NULL) {

					aux_data->aux_detect_gpio =
						of_get_named_gpio(
							pdev->dev.of_node,
							"aux-gpios", 0);

					ret = of_property_read_u32_index(
						pdev->dev.of_node,
						"aux-active",
						0,
						&out_value);
					if (ret == 0) {
						aux_data->aux_active = out_value;
					}


					pr_info("switch-gpios:%d\n",
						aux_data->aux_detect_gpio);
					pr_info("switch-active:%u\n",
						aux_data->aux_active);


					aux_data->enabled = 0;

					/* Create the aux_detect_status sysfs */
					ret = device_create_file(&pdev->dev,
						&dev_attr_aux_detect_status);
					if (ret < 0)
						pr_err("failed create sysfs\r\n");
				} else {
					pr_err("switch-gpios is not found.\n");
					ret = -1;
				}
			}
		}
	}
	return ret;
}

static int32_t aux_detect_probe(struct platform_device *pdev)
{
	int32_t ret = 0;

	if (pdev != NULL) {
		pre_aux_status = 0;
		/* Allocate a charactor device region */
		ret = alloc_chrdev_region(&aux_detect_dev_region,
							0, 1, MODULE_NAME);

		if (ret < 0) {
			pr_err("Allocate a cdev for \"%s\"\n",
				(const char *)MODULE_NAME);
		} else {
			/* Create class */
			aux_detect_class = class_create(
								THIS_MODULE,
								MODULE_NAME);

			if (aux_detect_class == NULL) {
				pr_err("Create the %s class\n",
					(const char *)MODULE_NAME);
				ret = -1;
			} else {
			/* Create the Reverse Switch device file system */
				if (device_create(aux_detect_class,
						NULL,
						aux_detect_dev_region,
						NULL,
						(const char *)MODULE_NAME)
						== NULL) {

					pr_err("Create the %s device file\n",
						(const char *) MODULE_NAME);
					ret = -1;
				} else {

					ret = aux_detect_create_cdev(pdev);
					if (ret < 0) {
						device_destroy(
							aux_detect_class,
							aux_detect_dev_region);
					}
				}
			}

			if (ret < 0) {
				if (aux_detect_class != NULL) {
					class_destroy(aux_detect_class);
					aux_detect_class = NULL;
				}

				unregister_chrdev_region(
					aux_detect_dev_region, 1);
			}
		}
	} else {
		ret = -1;
	}

	return ret;
}

static int32_t aux_detect_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_aux_detect_status);
	cdev_del(&aux_detect_cdev);
	device_destroy(aux_detect_class, aux_detect_dev_region);
	class_destroy(aux_detect_class);
	unregister_chrdev_region(aux_detect_dev_region, 1);

	return 0;
}

#if defined(CONFIG_PM)
static int32_t aux_detect_suspend(struct platform_device *pdev, pm_message_t state)
{
	(void)pdev;
	(void)state;
	return 0;
}

static int32_t aux_detect_resume(struct platform_device *pdev)
{
	struct pinctrl	*aux_pinctrl = NULL;
	int32_t ret = 0;

	if (pdev == NULL) {
		pr_err("aux device is null\n");
		ret = -ENODEV;
	} else {
		/* pinctrl */
		aux_pinctrl = pinctrl_get_select(&pdev->dev, "default");
		if (IS_ERR(aux_pinctrl)) {
			ret = -EFAULT;
			pr_err("pinctrl select failed\n");
		}
	}
	return ret;
}


#endif


#ifdef CONFIG_OF
static const struct of_device_id aux_detect_of_match[] = {
	{ .compatible = "telechips,aux_detect", },
	{ },
};
MODULE_DEVICE_TABLE(of, aux_detect_of_match);
#endif

static struct platform_driver aux_detect_driver = {
	.probe		= aux_detect_probe,
	.remove		= aux_detect_remove,
#if defined(CONFIG_PM)
	.suspend = aux_detect_suspend,
	.resume = aux_detect_resume,
#endif
	.driver		= {
		.name			= MODULE_NAME,
		.owner			= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aux_detect_of_match,
#endif
	},
};

module_platform_driver(aux_detect_driver);

MODULE_AUTHOR("<limdh3@telechips.com>");
MODULE_DESCRIPTION("Aux detect driver");
MODULE_LICENSE("GPL");

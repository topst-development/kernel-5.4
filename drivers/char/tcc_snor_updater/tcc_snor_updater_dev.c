// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/cpufreq.h>
#include <linux/err.h>

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/kdev_t.h>
#include <linux/kthread.h>


#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <linux/cdev.h>
#include <linux/atomic.h>

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>

#include "tcc_snor_updater_typedef.h"
#include <linux/tcc_snor_updater_dev.h>
#include "tcc_snor_updater_mbox.h"
#include "tcc_snor_updater_cmd.h"
#include "tcc_snor_updater.h"

#define SNOR_UPDATER_DEV_NAME        ("tcc_snor_updater")
#define SNOR_UPDATER_DEV_MINOR       (0)

int32_t updater_verbose_mode;

static void snor_updater_receive_message(struct mbox_client *client,
	void *message);

static void snor_updater_receive_message(struct mbox_client *client,
	void *message)
{

	if ((client != NULL) && (message != NULL)) {
		const struct tcc_mbox_data *msg = (struct tcc_mbox_data *)message;
		const struct platform_device *pdev =
			to_platform_device(client->dev);
		struct snor_updater_device *updater_dev =
			platform_get_drvdata(pdev);
		int32_t i;

		for (i = 0; i < MBOX_CMD_FIFO_SIZE; i++) {
			dprintk(updater_dev->dev,
				"cmd[%d] = [0x%02x]\n", i, msg->cmd[i]);
		}

		dprintk(updater_dev->dev,
		"data size (%d)\n", msg->data_len);

		snor_updater_wake_up(updater_dev, msg);
	}
}

static int32_t snor_updater_open(struct inode *inodp, struct file *filp)
{
	int32_t ret = SNOR_UPDATER_SUCCESS;
	struct snor_updater_device *snor_updater_dev = NULL;

	if (inodp != NULL) {
		snor_updater_dev =
			container_of(inodp->i_cdev,
			struct snor_updater_device, updater_cdev);
	}

	if ((snor_updater_dev != NULL) && (filp != NULL)) {

		mutex_lock(&snor_updater_dev->devMutex);
		if (snor_updater_dev->isOpened == 0) {
			snor_updater_event_create(snor_updater_dev);

			snor_updater_dev->mbox_ch =
				snor_updater_request_channel(
					snor_updater_dev->pdev,
					snor_updater_dev->mbox_name,
					&snor_updater_receive_message);

			if (snor_updater_dev->mbox_ch == NULL) {
				eprintk(snor_updater_dev->dev,
					"snor_updater_request_channel error");
				ret = -EPROBE_DEFER;
			} else {
				filp->private_data = snor_updater_dev;
			}

			if (ret == SNOR_UPDATER_SUCCESS) {
				snor_updater_dev->isOpened = 1;
			}
		} else {
			eprintk(snor_updater_dev->dev,
				"snor updater open fail - already open\n");
			ret = -EBUSY;
		}
		mutex_unlock(&snor_updater_dev->devMutex);
	} else {
		(void)pr_err("[ERROR][%s]%s: device not init\n",
			(const char *)LOG_TAG, __func__);
		ret = -ENXIO;
	}

	return ret;

}

static int32_t snor_updater_release(struct inode *inodp, struct file *filp)
{
	if (inodp != NULL) {
		struct snor_updater_device *snor_updater_dev =
			container_of(inodp->i_cdev,
			struct snor_updater_device, updater_cdev);

		(void)filp;

		mutex_lock(&snor_updater_dev->devMutex);
		iprintk(snor_updater_dev->dev, "In\n");

		if (snor_updater_dev->mbox_ch != NULL) {
			mbox_free_channel(snor_updater_dev->mbox_ch);
			snor_updater_dev->mbox_ch = NULL;
		}

		snor_updater_event_delete(snor_updater_dev);

		snor_updater_dev->isOpened = 0;
		mutex_unlock(&snor_updater_dev->devMutex);
	}

	return 0;
}

static long snor_updater_ioctl(struct file *filp,
			uint32_t cmd, ulong arg)
{
	long ret = -EINVAL;


	if (filp != NULL) {
		struct snor_updater_device *updater_dev =
			(struct snor_updater_device *)filp->private_data;

		switch (cmd) {
		case IOCTL_UPDATE_START:
		{
			int32_t ack;

			/* send mbox & wait ack */
			ack = snor_update_start(updater_dev);
			if (ack == SNOR_UPDATER_SUCCESS) {
				ret = 0;
			} else {
				ret = -EAGAIN;
			}
		}
		break;
		case IOCTL_UPDATE_DONE:
		{
			int32_t ack;

			/* send mbox & wait ack */
			ack = snor_update_done(updater_dev);
			if (ack == SNOR_UPDATER_SUCCESS) {
				ret = 0;
			} else {
				ret = -EAGAIN;
			}
		}
		break;
		case IOCTL_FW_UPDATE:
		{
			tcc_snor_update_param	fwInfo;

			if (copy_from_user((void *)&fwInfo,
				(const void *)arg,
				sizeof(tcc_snor_update_param)) ==
				(ulong)0) {

				const u8 *userBuffer = (const u8 *)fwInfo.image;
				uint32_t image_size = fwInfo.image_size;

				if (image_size > (uint32_t)0)	{
					fwInfo.image =
						kmalloc(image_size, GFP_KERNEL);
				}

				if (fwInfo.image  != NULL) {
					/* update fw image */
					if (copy_from_user(
						(void __user *)fwInfo.image,
						(const void *)userBuffer,
						(ulong)image_size)
						== (ulong)0) {
						ret = snor_update_fw(
							updater_dev, &fwInfo);

					}
					kfree(fwInfo.image);
				} else {
					eprintk(updater_dev->dev,
						"alloc memory fail\n");
					ret = -ENOMEM;
				}
			}
		}
			break;
		default:
			eprintk(updater_dev->dev,
				"unrecognized ioctl (%d)\n", cmd);
			break;
		}
	}
	return ret;
}


static const struct file_operations snor_updater_ctrl_fops = {
	.owner          = THIS_MODULE,
	.open           = snor_updater_open,
	.release        = snor_updater_release,
	.unlocked_ioctl = snor_updater_ioctl,
};

static int32_t create_snor_updater_device(struct platform_device *pdev,
	struct snor_updater_device *updater_dev)
{
	int32_t ret = -1;

	if ((pdev != NULL) && (updater_dev != NULL)) {

		updater_dev->updater_class = class_create(THIS_MODULE,
			SNOR_UPDATER_DEV_NAME);

		if (IS_ERR(updater_dev->updater_class)) {
			ret = -ENODEV;
			eprintk(&pdev->dev,	"class_create error\n");

		} else {
			updater_dev->dev = device_create(
					updater_dev->updater_class,
					&pdev->dev,
					updater_dev->devnum,
					NULL,
					SNOR_UPDATER_DEV_NAME);

			if (IS_ERR(updater_dev->dev)) {
				ret = -ENODEV;
				class_destroy(updater_dev->updater_class);
				eprintk(&pdev->dev, "device_create error\n");
			} else {
				ret = 0;
			}
		}
	}
	return ret;
}


static int32_t int_snor_updater_device(struct platform_device *pdev,
	struct snor_updater_device *updater_dev)
{
	int32_t ret = -1;

	if ((pdev != NULL) && (updater_dev != NULL)) {

		platform_set_drvdata(pdev, updater_dev);
		(void)of_property_read_string(pdev->dev.of_node,
			"mbox-names", &updater_dev->mbox_name);

		ret = alloc_chrdev_region(&updater_dev->devnum,
				SNOR_UPDATER_DEV_MINOR,
				1,
				SNOR_UPDATER_DEV_NAME);

		if (ret != 0) {
			eprintk(&pdev->dev,
				"alloc_chrdev_region error %d\n", ret);
		} else {
			cdev_init(&updater_dev->updater_cdev, &snor_updater_ctrl_fops);
			updater_dev->updater_cdev.owner = THIS_MODULE;

			ret = cdev_add(&updater_dev->updater_cdev, updater_dev->devnum, 1);
			if (ret != 0) {
				eprintk(&pdev->dev,	"cdev_add error %d\n", ret);
			} else	{
				ret = create_snor_updater_device(pdev, updater_dev);
				if (ret != 0) {
					cdev_del(&updater_dev->updater_cdev);
				}
			}

			if (ret != 0) {
				unregister_chrdev_region(
					updater_dev->devnum, 1);
			}
		}
	}
	return ret;
}

static int32_t snor_updater_probe(struct platform_device *pdev)
{
	int32_t ret = 0;
	struct snor_updater_device *updater_dev = NULL;

	iprintk(&pdev->dev, "in\n");
	updater_verbose_mode = 0;

	if (pdev != NULL) {
		updater_dev = devm_kzalloc(
			&pdev->dev,
			sizeof(struct snor_updater_device),
			GFP_KERNEL);
	}

	if (updater_dev == NULL) {
		ret = -ENOMEM;
	} else {
		ret = int_snor_updater_device(pdev, updater_dev);

		if (ret == 0) {
			updater_dev->pdev = pdev;
			mutex_init(&updater_dev->devMutex);
			updater_dev->isOpened = 0;
			updater_dev->waitQueue.condition = 0;

			iprintk(updater_dev->dev,
				"Successfully registered\n");
		}
	}
	return ret;
}

static int32_t snor_updater_remove(struct platform_device *pdev)
{
	struct snor_updater_device *updater_dev = platform_get_drvdata(pdev);

	if (updater_dev != NULL) {
		device_destroy(updater_dev->updater_class, updater_dev->devnum);
		class_destroy(updater_dev->updater_class);
		cdev_del(&updater_dev->updater_cdev);
		unregister_chrdev_region(updater_dev->devnum, 1);
	}
	return 0;
}


#if defined(CONFIG_PM)
static int32_t snor_updater_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	struct snor_updater_device *updater_dev = platform_get_drvdata(pdev);

	(void)state;

	if (updater_dev != NULL) {
		mutex_lock(&updater_dev->devMutex);

		if (updater_dev->isOpened == 1) {
			if (updater_dev->mbox_ch != NULL) {
				mbox_free_channel(updater_dev->mbox_ch);
				updater_dev->mbox_ch = NULL;
			}
			snor_updater_event_delete(updater_dev);
		}
		mutex_unlock(&updater_dev->devMutex);
	}
	return 0;
}

static int32_t snor_updater_resume(struct platform_device *pdev)
{
	struct snor_updater_device *updater_dev = platform_get_drvdata(pdev);
	int32_t ret = 0;

	if (updater_dev != NULL) {
		mutex_lock(&updater_dev->devMutex);
		if (updater_dev->isOpened == 1)	{

			updater_dev->mbox_ch =
				snor_updater_request_channel(
				updater_dev->pdev,
				updater_dev->mbox_name,
				&snor_updater_receive_message);

			if (updater_dev->mbox_ch == NULL) {
				eprintk(updater_dev->dev,
					"snor_updater_request_channel error");
				ret = -EPROBE_DEFER;
			}

		}
		mutex_unlock(&updater_dev->devMutex);
	}

	return ret;
}

#endif

#ifdef CONFIG_OF
static const struct of_device_id snor_updater_ctrl_of_match[] = {
	{.compatible = "telechips,snor_updater", },
	{ },
};

MODULE_DEVICE_TABLE(of, snor_updater_ctrl_of_match);
#endif

static struct platform_driver snor_updater_ctrl = {
	.probe	= snor_updater_probe,
	.remove	= snor_updater_remove,
#if defined(CONFIG_PM)
	.suspend = snor_updater_suspend,
	.resume = snor_updater_resume,
#endif
	.driver	= {
		.name	= SNOR_UPDATER_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = snor_updater_ctrl_of_match,
#endif
	},
};

static int32_t __init snor_updater_init(void)
{
	return platform_driver_register(&snor_updater_ctrl);
}

static void __exit snor_updater_exit(void)
{
	platform_driver_unregister(&snor_updater_ctrl);
}

module_init(snor_updater_init);
module_exit(snor_updater_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC Snor update driver");
MODULE_LICENSE("GPL");



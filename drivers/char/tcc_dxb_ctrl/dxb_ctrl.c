// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include "tcc_dxb_control.h"
#include "dxb_ctrl_defs.h"
#include "dxb_ctrl_gpio.h"
#include "amfmtuner.h"

/*****************************************************************************
 * Variables
 ******************************************************************************/
static struct tcc_dxb_ctrl_t *gDxbCtrl;

/*****************************************************************************
 * Baseband Functions
 ******************************************************************************/
#define DEFAULT_BOARD_TYPE (BOARD_AMFM_TUNER)

struct baseband {
	int32_t type;
	long_t (*ioctl)(const struct tcc_dxb_ctrl_t *ctrl, uint32_t cmd,
			 ulong arg);
};

static struct baseband BB[] = {
	{BOARD_AMFM_TUNER, amfmtuner_ioctl},
/* don't not modify or remove next line.
 * when add new device, add to above this line
 */
	{BOARD_DXB_UNDEFINED, NULL}
};

/* return : index of BB which has given type. if not found, return (-1) */
static int32_t get_bb_index(int32_t type)
{
	int32_t found_index = -1;
	int32_t index = 0;

	do {
		if (BB[index].type == type) {
			found_index = index;
			break;
		}
		index++;
	} while (BB[index].type != (int32_t) BOARD_DXB_UNDEFINED);

	return found_index;
}

/*****************************************************************************
 * TCC_DXB_CTRL Functions
 ******************************************************************************/
static long_t tcc_dxb_ctrl_ioctl(struct file *filp, uint32_t cmd, ulong arg)
{
	long_t ret;
	const struct baseband *bb;

	(void)filp;
	(void)pr_info("[INFO][TCC_DXB_CTRL] %s cmd[0x%X]\n", __func__, cmd);

	ret = 0;

	if (gDxbCtrl == NULL) {
		ret = (long_t) (-ENODEV);
	} else {
		switch (cmd) {
		case IOCTL_DXB_CTRL_SET_BOARD:
			{
				int32_t new_board_type;
				ulong result;

				if (arg == (ulong) 0) {
					ret = (long_t) (-EINVAL);
					break;
				}

				result =
				    copy_from_user((void *)&new_board_type,
						   (const void *)arg,
						   sizeof(int32_t));
				if (result != (ulong) 0) {
					break;
				}

				gDxbCtrl->board_type = new_board_type;
				gDxbCtrl->bb_index =
				    get_bb_index(new_board_type);
				if (gDxbCtrl->bb_index == (-1)) {
					/* undefined board type */
					(void)pr_err
					    ("[ERROR][TCC_DXB_CTRL] %s cmd[0x%X] undefined board type %d\n",
					     __func__, cmd, new_board_type);
					ret = (long_t) (-1);
					break;
				}

				bb = &BB[gDxbCtrl->bb_index];
				(void)pr_info
				    ("[INFO][TCC_DXB_CTRL] new_board_type=%d, bb_index=%d\n",
				     gDxbCtrl->board_type, gDxbCtrl->bb_index);

				if (bb->ioctl != NULL) {
					ret =
					    (long_t) bb->ioctl(gDxbCtrl, cmd,
							       arg);
				}
			}
			break;
		default:
			{
				if (gDxbCtrl->bb_index != (-1)) {
					bb = &BB[gDxbCtrl->bb_index];

					if (bb->ioctl != NULL) {
						ret =
						    (long_t) bb->ioctl(gDxbCtrl,
								       cmd,
								       arg);
					} else {
						ret = (long_t) (-2);
					}
				} else {
					ret = (long_t) (-1);
				}
			}
			break;
		}
	}
	return ret;
}

static int32_t tcc_dxb_ctrl_release(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;

	dxb_ctrl_gpio_free(gDxbCtrl->gpio_tuner_rst);
	dxb_ctrl_gpio_free(gDxbCtrl->gpio_tuner_pwr);

	(void)pr_info("[INFO][TCC_DXB_CTRL] %s::%d\n", __func__, __LINE__);

	return 0;
}

static int32_t tcc_dxb_ctrl_open(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;

	(void)pr_info("[INFO][TCC_DXB_CTRL]%s::%d\n", __func__, __LINE__);

	return 0;
}

static const struct file_operations tcc_dxb_ctrl_fops = {
	.owner = THIS_MODULE,
	.open = tcc_dxb_ctrl_open,
	.release = tcc_dxb_ctrl_release,
	.unlocked_ioctl = tcc_dxb_ctrl_ioctl,
};

/*****************************************************************************
 * TCC_DXB_CTRL Probe/Remove
 ******************************************************************************/
static int32_t tcc_dxb_ctrl_probe(struct platform_device *pdev)
{
	int32_t ret;
	int32_t err_pos;
	struct device *dev;

	ret = 0;
	err_pos = 0;

	if (pdev == NULL) {
		ret = (int32_t) (-ENODEV);
	}

	if (ret == 0) {
		dev = &pdev->dev;

		gDxbCtrl = (struct tcc_dxb_ctrl_t *)
		    kzalloc(sizeof(struct tcc_dxb_ctrl_t), GFP_KERNEL);
		if (gDxbCtrl == NULL) {
			ret = (int32_t) (-ENOMEM);
			err_pos = 1;
		}
	}

	if (ret == 0) {
		ret =
		    alloc_chrdev_region(&gDxbCtrl->devnum, 0, 1,
					DXB_CTRL_DEV_NAME);
		if (ret != 0) {
			(void)pr_err
			    ("[ERROR][TCC_DXB_CTRL] %s alloc_chrdev_region failed\n",
			     __func__);
			err_pos = 2;
		}
	}

	/* Add character device */
	if (ret == 0) {
		cdev_init(&gDxbCtrl->cdev, &tcc_dxb_ctrl_fops);
		gDxbCtrl->cdev.owner = THIS_MODULE;
		ret = cdev_add(&gDxbCtrl->cdev, gDxbCtrl->devnum, 1);
		if (ret != 0) {
			(void)
			    pr_err("[ERROR][TCC_DXB_CTRL] %s cdev_add failed\n",
				   __func__);
			err_pos = 3;
		}
	}

	/* Create a class */
	if (ret == 0) {
		gDxbCtrl->class = class_create(THIS_MODULE, DXB_CTRL_DEV_NAME);
		if (IS_ERR(gDxbCtrl->class)) {
			ret = (int32_t) PTR_ERR(gDxbCtrl->class);
			(void)
			    pr_err
			    ("[ERROR][TCC_DXB_CTRL] %s class_create failed\n",
			     __func__);
			err_pos = 4;
		}
	}

	/* Create a device node */
	if (ret == 0) {
		gDxbCtrl->dev =
		    device_create(gDxbCtrl->class, dev, gDxbCtrl->devnum,
				  gDxbCtrl, DXB_CTRL_DEV_NAME);
		if (IS_ERR(gDxbCtrl->dev)) {
			ret = (int32_t) PTR_ERR(gDxbCtrl->dev);
			(void)
			    pr_err
			    ("[ERROR][TCC_DXB_CTRL] %s device_create failed\n",
			     __func__);
			err_pos = 5;
		}
	}

	if (ret == 0) {
#ifdef CONFIG_OF
		gDxbCtrl->gpio_tuner_rst =
		    of_get_named_gpio(dev->of_node, "tuner-gpios", 0);
		gDxbCtrl->gpio_tuner_pwr =
		    of_get_named_gpio(dev->of_node, "tuner-gpios", 1);
#endif
		(void)pr_info("[INFO][TCC_DXB_CTRL] %s tuner [%d][%d]\n",
			      __func__, gDxbCtrl->gpio_tuner_rst,
			      gDxbCtrl->gpio_tuner_pwr);

		gDxbCtrl->board_type = (int32_t) DEFAULT_BOARD_TYPE;
		gDxbCtrl->bb_index = get_bb_index(gDxbCtrl->board_type);
		if (gDxbCtrl->bb_index == -1) {
			(void)pr_err
			    ("[ERROR][TCC_DXB_CTRL] [%s] undefined board type\n",
			     __func__);
		}
	}

	if (err_pos >= 5) {
		class_destroy(gDxbCtrl->class);
	}

	if (err_pos >= 4) {
		cdev_del(&gDxbCtrl->cdev);
	}

	if (err_pos >= 3) {
		unregister_chrdev_region(gDxbCtrl->devnum, 1);
	}

	if (err_pos >= 2) {
		kfree(gDxbCtrl);
		gDxbCtrl = NULL;
	}

	return ret;
}

static int32_t tcc_dxb_ctrl_remove(struct platform_device *pdev)
{
	(void)pdev;

	if (gDxbCtrl != NULL) {
		device_destroy(gDxbCtrl->class, gDxbCtrl->devnum);
		class_destroy(gDxbCtrl->class);
		cdev_del(&gDxbCtrl->cdev);
		unregister_chrdev_region(gDxbCtrl->devnum, 1);

		kfree(gDxbCtrl);
		gDxbCtrl = NULL;
	}

	return 0;
}

/*****************************************************************************
 * TCC_DXB_CTRL Module Init/Exit
 ******************************************************************************/
#ifdef CONFIG_OF
static const struct of_device_id tcc_dxb_ctrl_of_match[] = {
	{.compatible = "telechips,tcc_dxb_ctrl",},
	{},
};

MODULE_DEVICE_TABLE(of, tcc_dxb_ctrl_of_match);
#endif

static struct platform_driver tcc_dxb_ctrl = {
	.probe = tcc_dxb_ctrl_probe,
	.remove = tcc_dxb_ctrl_remove,
	.driver = {
		   .name = DXB_CTRL_DEV_NAME,
#ifdef CONFIG_OF
		   .of_match_table = tcc_dxb_ctrl_of_match,
#endif
		   },
};

module_platform_driver(tcc_dxb_ctrl);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC Radio GPIO Control(Power, Reset..)");
MODULE_LICENSE("GPL");

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * linux/drivers/char/tcc_lut.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_lut_3d.h>
#include <video/tcc/tcc_lut_3d_ioctl.h>

#define LUT_VERSION "v1.0"

struct lut_3d_drv_d1_type {
	struct miscdevice *misc;

	/* proc fs */
	struct proc_dir_entry *proc_dir;
	struct proc_dir_entry *proc_debug;
};

static struct lut_3d_drv_d1_type *lut_3d_api_d1;

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static long lut_3d_drv_d1_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	int ret = -EFAULT;

	(void)filp;

	switch (cmd) {
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case TCC_LUT_3D_SET_TABLE:
		{
			struct VIOC_LUT_3D_SET_TABLE *lut_cmd;

			lut_cmd =
			/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
			    kmalloc(sizeof(struct VIOC_LUT_3D_SET_TABLE),
				/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
				    GFP_KERNEL);
			if(lut_cmd == NULL) {
				ret = -EFAULT;
				break;
			}
			if ((bool)copy_from_user
			/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
			    ((void *)lut_cmd, (const void *)arg,
			     sizeof(struct VIOC_LUT_3D_SET_TABLE))) {
				kfree(lut_cmd);
				break;
			}

			(void)vioc_lut_3d_set_table(LUT_3D_DISP1, lut_cmd->table);
			kfree(lut_cmd);
			ret = 0;
		}
		break;
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case TCC_LUT_3D_ONOFF:
		{
			struct VIOC_LUT_3D_ONOFF lut_cmd;

			if ((bool)copy_from_user
			/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
			/* coverity[cert_int36_c_violation : FALSE] */
			    ((void *)&lut_cmd, (const void *)arg,
			     sizeof(struct VIOC_LUT_3D_ONOFF))) {
				break;
			}
			ret =
			    vioc_lut_3d_bypass(LUT_3D_DISP1,
					       lut_cmd.lut_3d_onoff);
		}
		break;

	default:
		(void)pr_err
		    ("[ERR][3D LUT] not supported 3D LUT IOCTL\n");
		break;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
/* coverity[misra_c_2012_rule_5_7_violation : FALSE] */
static int lut_3d_drv_d1_open(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;
	return 0;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int lut_3d_drv_d1_release(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;
	return 0;
}

static const struct file_operations lut_3d_drv_d1_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = lut_3d_drv_d1_ioctl,
	.open = lut_3d_drv_d1_open,
	.release = lut_3d_drv_d1_release,
};

static int lut_3d_drv_d1_probe(struct platform_device *pdev)
{
	struct lut_3d_drv_d1_type *lut_3d;
	int ret = -ENODEV;

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	lut_3d = kzalloc(sizeof(struct lut_3d_drv_d1_type), GFP_KERNEL);

	if (lut_3d == NULL) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_misc;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	lut_3d->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);

	if (lut_3d->misc == NULL) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_misc_alloc;
	}

	/* register lut 3d misc device */
	lut_3d->misc->minor = MISC_DYNAMIC_MINOR;
	lut_3d->misc->fops = &lut_3d_drv_d1_fops;
	lut_3d->misc->name = "tcc_lut_3d_d1";
	lut_3d->misc->parent = &pdev->dev;
	ret = misc_register(lut_3d->misc);

	if (ret != 0) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_misc_register;
	}
	//lut_drv_fill_mapping_table();

	platform_set_drvdata(pdev, lut_3d);

	/* Copy lut to lut_3d_api_d1 to support external APIs */
	lut_3d_api_d1 = lut_3d;
	(void)pr_info(
		"[INF][LUT 3D] %s: :%s, Driver %s Initialized\n",
			__func__, LUT_VERSION, pdev->name);
	/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
	goto normal_return;

err_misc_register:
	misc_deregister(lut_3d->misc);

err_misc_alloc:
	kfree(lut_3d->misc);
err_misc:
	kfree(lut_3d);
	(void)pr_err("[ERR][3D LUT]%s: %s: err ret:%d\n",
	       __func__, pdev->name, ret);
normal_return:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int lut_3d_drv_d1_remove(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct lut_3d_drv_d1_type *lut_3d =
	    (struct lut_3d_drv_d1_type *)platform_get_drvdata(pdev);

	misc_deregister(lut_3d->misc);
	kfree(lut_3d);

	lut_3d_api_d1 = NULL;
	return 0;
}

static const struct of_device_id lut_3d_d1_of_match[] = {
	{.compatible = "telechips,vioc_lut_3d_d1"},
	{}
};

MODULE_DEVICE_TABLE(of, lut_3d_d1_of_match);

static struct platform_driver lut_3d_driver_d1 = {
	.probe = lut_3d_drv_d1_probe,
	.remove = lut_3d_drv_d1_remove,
	.driver = {
		   .name = "tcc_lut_3d_d1",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(lut_3d_d1_of_match),
#endif
		   },
};

static int __init lut_3d_drv_d1_init(void)
{
	return platform_driver_register(&lut_3d_driver_d1);
}

static void __exit lut_3d_drv_d1_exit(void)
{
	platform_driver_unregister(&lut_3d_driver_d1);
}

/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
module_init(lut_3d_drv_d1_init);
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
module_exit(lut_3d_drv_d1_exit);

/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_5_9_violation : FALSE] */
MODULE_AUTHOR("Telechips.");
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_5_9_violation : FALSE] */
MODULE_DESCRIPTION("Telechips 3D look up table Driver 1");
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_5_9_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_LICENSE("GPL");

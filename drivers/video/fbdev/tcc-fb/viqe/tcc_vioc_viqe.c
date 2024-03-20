// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/compat.h>
#include <asm/div64.h>

#include <video/tcc/vioc_viqe.h>

#include "tcc_vioc_viqe.h"
#include "tcc_vioc_viqe_interface.h"

#if 0
#define dprintk(msg...) pr_info("[DBG][VIQE_D] " msg)
#else
#define dprintk(msg...)
#endif

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static long tcc_viqe_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct VIQE_DI_TYPE viqe_arg;

	dprintk("%s: (0x%x)\n", __func__, cmd);

	(void)filp;

	switch (cmd) {
	case IOCTL_VIQE_INITIALIZE:
	case IOCTL_VIQE_INITIALIZE_KERNEL:
		if (cmd == IOCTL_VIQE_INITIALIZE_KERNEL) {
			/* coverity[cert_int36_c_violation : FALSE] */
			/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
			(void)memcpy(&viqe_arg, (void *)arg, sizeof(viqe_arg));
		} else {
			if (copy_from_user(&viqe_arg, (void *)arg, sizeof(viqe_arg)) != 0U) {
				(void)pr_err("%s: Error copy_from_user(%d)\n", __func__, cmd);
				ret = -EINVAL;
			}
		}

		if (ret == 0) {
			dprintk("IOCTL_VIQE_INITIALIZE: disp%d, sc%d, otf(%d) %dx%d\n",
				__func__, viqe_arg.lcdCtrlNo,
				viqe_arg.scalerCh, viqe_arg.onTheFly,
				viqe_arg.srcWidth, viqe_arg.srcHeight);

			TCC_VIQE_DI_Init(&viqe_arg);
		}

		break;

	case IOCTL_VIQE_EXCUTE:
	case IOCTL_VIQE_EXCUTE_KERNEL:
		if (cmd == IOCTL_VIQE_EXCUTE_KERNEL) {
			(void)memcpy(&viqe_arg, (void *)arg, sizeof(struct VIQE_DI_TYPE));
		} else {
			if (copy_from_user(&viqe_arg, (void *)arg, sizeof(struct VIQE_DI_TYPE)) != 0U) {
				(void)pr_err("%s: Error copy_from_user(%d)\n", __func__, cmd);
				ret = -EINVAL;
			}
		}

		if (ret == 0) {
			dprintk("IOCTL_VIQE_EXCUTE:\n");
			dprintk(" sc%d, addr(%x %x %x %x %x %x) %dx%d\n",
				viqe_arg.scalerCh,
				viqe_arg.address[0], viqe_arg.address[1],
				viqe_arg.address[2], viqe_arg.address[3],
				viqe_arg.address[4], viqe_arg.address[5],
				viqe_arg.srcWidth, viqe_arg.srcHeight
				);
			dprintk(" disp%d, dstAddr %x, m2m %d, OddFirst %d, DI %d\n",
				viqe_arg.lcdCtrlNo, viqe_arg.dstAddr,
				viqe_arg.deinterlaceM2M, viqe_arg.OddFirst,
				viqe_arg.DI_use);

			TCC_VIQE_DI_Run(&viqe_arg);
		}

		break;

	case IOCTL_VIQE_DEINITIALIZE:
	case IOCTL_VIQE_DEINITIALIZE_KERNEL:
		if (cmd == IOCTL_VIQE_DEINITIALIZE_KERNEL) {
			(void)memcpy(&viqe_arg, (void *)arg, sizeof(struct VIQE_DI_TYPE));
		} else {
			if (copy_from_user(&viqe_arg, (void *)arg, sizeof(struct VIQE_DI_TYPE)) != 0U) {
				(void)pr_err("%s: Error copy_from_user(%d)\n", __func__, cmd);
				ret = -EINVAL;
			}
		}

		if (ret == 0) {
			TCC_VIQE_DI_DeInit(&viqe_arg);
		}

		break;

	default:
		(void)pr_err("[WAN][VIQE_D] unrecognized ioctl (0x%x)\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long tcc_viqe_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* coverity[cert_exp37_c_violation : FALSE] */
	/* coverity[cert_int31_c_violation : FALSE] */
	/* coverity[cert_int02_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	return tcc_viqe_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tcc_viqe_release(struct inode *inodp, struct file *filp)
{
	(void)inodp;
	(void)filp;

	(void)pr_info("[INF][VIQE_D] %s\n", __func__);
	return 0;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tcc_viqe_open(struct inode *inodp, struct file *filp)
{
	(void)inodp;
	(void)filp;

	(void)pr_info("[INF][VIQE_D] %s\n", __func__);
	return 0;
}


static const struct file_operations tcc_viqe_fops = {
	.owner		= THIS_MODULE,
	.open		= tcc_viqe_open,
	.release	= tcc_viqe_release,
	.unlocked_ioctl	= tcc_viqe_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= tcc_viqe_compat_ioctl,
#endif
};

static struct class *viqe_class;

static int __init tcc_viqe_init(void)
{
	int ret = 0;

	ret = register_chrdev(VIQE_DEV_MAJOR, VIQE_DEV_NAME, &tcc_viqe_fops);
	if (ret != 0) {
		(void)pr_err("unable to get major %d for %s\n", VIQE_DEV_MAJOR, VIQE_DEV_NAME);
	} else {
		/* coverity[cert_dcl37_c_violation : FALSE] */
		viqe_class = class_create(THIS_MODULE, VIQE_DEV_NAME);

		/* coverity[misra_c_2012_rule_12_2_violation : FALSE] */
		(void)device_create(viqe_class, NULL, MKDEV(VIQE_DEV_MAJOR, VIQE_DEV_MINOR),
				NULL, VIQE_DEV_NAME);

		(void)pr_info("[INF][VIQE_D] %s\n", __func__);
	}

	return ret;
}

static void __exit tcc_viqe_exit(void)
{
	unregister_chrdev(VIQE_DEV_MAJOR, VIQE_DEV_NAME);
	(void)pr_info("[INF][VIQE_D] %s\n", __func__);
}

/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
module_init(tcc_viqe_init);
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
module_exit(tcc_viqe_exit);

/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_5_9_violation : FALSE] */
MODULE_AUTHOR("Telechips Inc.");
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_5_9_violation : FALSE] */
MODULE_DESCRIPTION("TCCxxx viqe driver");
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_5_9_violation : FALSE] */
MODULE_LICENSE("GPL");

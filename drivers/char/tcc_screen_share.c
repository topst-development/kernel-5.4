// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * linux/drivers/char/tcc_screen_share.c
 *
 * Copyright (c) 2019 Telechips, Inc.
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

#include <linux/platform_device.h>

#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>

#include <linux/io.h>

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>

#include <video/tcc/tcc_screen_share.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/tcc_overlay_ioctl.h>
#include "tcc_screen_share.h"

#define TCC_SCRSHARE_DEV_MINOR		0
#define OVERLAY_DRIVER "/dev/overlay1"

struct tcc_scrshare_tx_t {
	struct mutex lock;
	atomic_t seq;
};

struct tcc_scrshare_rx_t {
	struct mutex lock;
	atomic_t seq;
};

struct tcc_scrshare_device {
	struct platform_device *pdev;
	struct device *dev;
	struct cdev scdev;
	struct class *pclass;
	dev_t devt;
	const char *name;
	const char *mbox_name;
	const char *overlay_driver_name;
#if defined(CONFIG_ARCH_TCC805X)
	const char *mbox_id;
#endif
	struct mbox_chan *mbox_ch;
	struct mbox_client cl;

	atomic_t status;

	struct tcc_scrshare_tx_t tx;
	struct tcc_scrshare_rx_t rx;
};

static struct tcc_scrshare_device *stcc_scrshare_device;
static wait_queue_head_t mbox_waitq;
static int mbox_done;
static struct tcc_scrshare_info *stcc_scrshare_info;

static struct file *overlay_file;

static void tcc_scrshare_on(const struct tcc_scrshare_device *stcc_scrshare)
{
	stcc_scrshare_info->share_enable = 1U;

	if (IS_ERR_OR_NULL(overlay_file)) {
		overlay_file =
			filp_open(
				stcc_scrshare->overlay_driver_name, (int)O_RDWR,
				/* DP no.23B */
				/* coverity[misra_c_2012_rule_7_1_violation : FALSE] */
				0666);
		if (IS_ERR_OR_NULL(overlay_file)) {
			/* Prevent KCS warning */
			(void)pr_err(
				"open fail (%s)\n",
				stcc_scrshare->overlay_driver_name);
		}
	}
}

static void tcc_scrshare_off(void)
{
	stcc_scrshare_info->share_enable = 0U;

	if (!IS_ERR_OR_NULL(overlay_file)) {
		(void)filp_close(overlay_file, NULL);
		overlay_file = NULL;
	}
}

static void tcc_scrshare_get_dstinfo(struct tcc_mbox_data *mssg)
{
	mssg->data[0] = stcc_scrshare_info->dstinfo->x;
	mssg->data[1] = stcc_scrshare_info->dstinfo->y;
	mssg->data[2] = stcc_scrshare_info->dstinfo->width;
	mssg->data[3] = stcc_scrshare_info->dstinfo->height;
	mssg->data[4] = stcc_scrshare_info->dstinfo->img_num;
	mssg->data_len = 5;
	(void)pr_info("%s x:%d, y:%d, w:%d, h:%d, img_num:%d\n",
		 __func__, mssg->data[0], mssg->data[1], mssg->data[2],
		 mssg->data[3], mssg->data[4]);
}

static void
tcc_scrshare_display(
	const struct tcc_scrshare_device *stcc_scrshare, const struct tcc_mbox_data *mssg)
{
	long ret = 0;
	overlay_shared_buffer_t buffer_cfg;

	(void)pr_info("%s shared_enable:%d base:%d, frm_w:%d, frm_h:%d, fmt:%d, rgb_swap:%d\n",
		 __func__, stcc_scrshare_info->share_enable, mssg->data[0],
		 mssg->data[1], mssg->data[2], mssg->data[3], mssg->data[4]);
	(void)pr_info("%s dst x:%d, y:%d, w:%d, h:%d\n", __func__,
		 stcc_scrshare_info->dstinfo->x,
		 stcc_scrshare_info->dstinfo->y,
		 stcc_scrshare_info->dstinfo->width,
		 stcc_scrshare_info->dstinfo->height);
	if (stcc_scrshare_info->share_enable == 1U) {
		if (IS_ERR_OR_NULL(overlay_file)) {
			overlay_file =
				filp_open(
					stcc_scrshare->overlay_driver_name,
					(int)O_RDWR,
					/* DP no.23B */
					/* coverity[misra_c_2012_rule_7_1_violation : FALSE] */
					0666);
			if (IS_ERR_OR_NULL(overlay_file)) {
				ret = -ENODEV;
				(void)pr_err(
					"open fail (%s)\n",
					stcc_scrshare->overlay_driver_name);
			}
		}

		buffer_cfg.src_addr = mssg->data[0];
		buffer_cfg.frm_w = mssg->data[1];
		buffer_cfg.frm_h = mssg->data[2];
		buffer_cfg.fmt = mssg->data[3];
		buffer_cfg.rgb_swap = mssg->data[4];
		buffer_cfg.dst_x = stcc_scrshare_info->dstinfo->x;
		buffer_cfg.dst_y = stcc_scrshare_info->dstinfo->y;
		buffer_cfg.dst_w = stcc_scrshare_info->dstinfo->width;
		buffer_cfg.dst_h = stcc_scrshare_info->dstinfo->height;
		buffer_cfg.layer = stcc_scrshare_info->dstinfo->img_num;

		if (!IS_ERR_OR_NULL(overlay_file)) {
			/* Prevent KCS warning */
			ret = overlay_file->f_op->unlocked_ioctl(overlay_file,
						 OVERLAY_PUSH_SHARED_BUFFER,
						 (unsigned long)&buffer_cfg);
		}
		if (ret < 0) {
			/* Prevent KCS warning */
			(void)pr_err("OVERLAY_PUSH_SHARED_BUFFER fail\n");
		}
	}
}

static void tcc_scrshare_send_message(const struct tcc_scrshare_device *stcc_scrshare,
				      struct tcc_mbox_data *mssg)
{
	if (stcc_scrshare != NULL) {
		int ret;

		ret = mbox_send_message(stcc_scrshare->mbox_ch, mssg);
#if defined(CONFIG_ARCH_TCC805X)
		if (ret < 0) {
			/* Prevent KCS warning */
			(void)pr_err("screen share mbox send error(%d)\n", ret);
		}
#else
		mbox_client_txdone(stcc_scrshare->mbox_ch, ret);
#endif
	}
}

static void tcc_scrshare_cmd_handler(struct tcc_scrshare_device *stcc_scrshare,
				     struct tcc_mbox_data *data)
{
	int ret = -1; /* avoid CERT-C Integers Rule INT31-C */
	if ((data != NULL) && (stcc_scrshare != NULL)) {

		unsigned int cmd = ((data->cmd[1] >> 16U) & 0xFFFFU);

		if (data->cmd[0] != 0U) {
			int temp = 0;
			unsigned int read_data = 0U;

			temp = atomic_read(&stcc_scrshare->rx.seq);
			if(temp >= 0) {
				read_data = (unsigned int)temp;

				if (read_data > data->cmd[0]) {
					(void)pr_err("%s: already processed command(%d,%d)\n",
					       __func__,
					       read_data,
					       data->cmd[1]);
					/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
					goto FUNC_EXIT;
				}
			} else {
				(void)pr_err("%s: atomic_read err(%d)\n",
				       __func__, temp);
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto FUNC_EXIT;
			}
		}

		switch (cmd) {
		case SCRSHARE_CMD_GET_DSTINFO:
			tcc_scrshare_get_dstinfo(data);
			ret = 0;
			break;
		case SCRSHARE_CMD_SET_SRCINFO:
			tcc_scrshare_display(stcc_scrshare, data);
			ret = 0;
			break;
		case SCRSHARE_CMD_ON:
			ret = 0;
			tcc_scrshare_on(stcc_scrshare);
			break;
		case SCRSHARE_CMD_OFF:
			ret = 0;
			tcc_scrshare_off();
			break;
		case SCRSHARE_CMD_READY:
		case SCRSHARE_CMD_NULL:
		case SCRSHARE_CMD_MAX:
		default:
			ret = -1;
			(void)pr_warn("warning in %s: Invalid command(%d)\n",
				__func__, cmd);
			break;
		}
		if (ret < 0) {
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto FUNC_EXIT;
		}

		/* Update rx-sequence ID */
		if (data->cmd[0] != 0U) {
			unsigned temp_uint = 0U; /* avoid CERT-C Integers Rule INT31-C */

			data->cmd[1] |= TCC_SCRSHARE_ACK;
			tcc_scrshare_send_message(stcc_scrshare, data);

			/* avoid CERT-C Integers Rule INT31-C */
			temp_uint = data->cmd[0];
			if (temp_uint < (UINT_MAX / 2U)) {
				atomic_set(&stcc_scrshare->rx.seq, (int)temp_uint);
				(void)pr_info("%s rx&cmd[0]:0x%x, cmd[1]:0x%x\n", __func__,
					data->cmd[0], data->cmd[1]);
			} else {
				(void)pr_err("%s: data->cmd[0] err(%d)\n",
					__func__, data->cmd[0]);
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto FUNC_EXIT;
			}
		}
	}
FUNC_EXIT:
	return;
}

static void tcc_scrshare_receive_message(struct mbox_client *client, void *mssg)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_mbox_data *msg = (struct tcc_mbox_data *)mssg;
	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	struct tcc_scrshare_device *stcc_scrshare =
	    container_of(client, struct tcc_scrshare_device, cl);
	unsigned int command = ((msg->cmd[1] >> 16U) & 0xFFFFU);

	switch (command) {
	case SCRSHARE_CMD_GET_DSTINFO:
	case SCRSHARE_CMD_SET_SRCINFO:
	case SCRSHARE_CMD_ON:
	case SCRSHARE_CMD_OFF:
		if (atomic_read(&stcc_scrshare->status) == SCRSHARE_STS_READY) {
			if ((msg->cmd[1] & TCC_SCRSHARE_ACK) != 0U) {
				if (command == (unsigned int)SCRSHARE_CMD_GET_DSTINFO) {
					(void)pr_info("%s x:%d, y:%d, w:%d, h:%d\n",
					     __func__, msg->data[0],
					     msg->data[1], msg->data[2],
					     msg->data[3]);
					stcc_scrshare_info->dstinfo->x =
					    msg->data[0];
					stcc_scrshare_info->dstinfo->y =
					    msg->data[1];
					stcc_scrshare_info->dstinfo->width =
					    msg->data[2];
					stcc_scrshare_info->dstinfo->height =
					    msg->data[3];
					stcc_scrshare_info->dstinfo->img_num =
					    msg->data[4];
				} else if (command == (unsigned int)SCRSHARE_CMD_ON) {
					(void)pr_info("%s SCRSHARE_CMD_ON ok\n",
						__func__);
					stcc_scrshare_info->share_enable = 1U;
				} else if (command == (unsigned int)SCRSHARE_CMD_OFF) {
					(void)pr_info("%s SCRSHARE_CMD_OFF ok\n",
						__func__);
					stcc_scrshare_info->share_enable = 0U;
				} else {
					 /* avoid MISRA C-2012 Rule 15.7 */
				}

				mbox_done = 1;
				wake_up_interruptible(&mbox_waitq);
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto FUNC_EXIT;
			}
			mutex_lock(&stcc_scrshare->rx.lock);
			tcc_scrshare_cmd_handler(stcc_scrshare, msg);
			mutex_unlock(&stcc_scrshare->rx.lock);
		}
		break;
	case SCRSHARE_CMD_READY:
		if (atomic_read(&stcc_scrshare->status) == SCRSHARE_STS_READY) {
			tcc_scrshare_off();
			atomic_set(&stcc_scrshare->status, SCRSHARE_STS_INIT);
			atomic_set(&stcc_scrshare->rx.seq, 0);
		}

		if (atomic_read(&stcc_scrshare->status) == SCRSHARE_STS_INIT) {
			atomic_set(&stcc_scrshare->status, SCRSHARE_STS_READY);
			if ((msg->cmd[1] & TCC_SCRSHARE_ACK) == 0U) {
				msg->cmd[1] |= TCC_SCRSHARE_ACK;
				tcc_scrshare_send_message(stcc_scrshare, msg);
			}
		}
		break;
	case SCRSHARE_CMD_NULL:
	case SCRSHARE_CMD_MAX:
	default:
		(void)pr_warn("%s :Invalid command(%d)\n", __func__, msg->cmd[1]);
		break;
	}

FUNC_EXIT:
	return;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static long tcc_scrshare_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_scrshare_device *stcc_scrshare = filp->private_data;
	struct tcc_mbox_data data;
	long ret = 0;
	unsigned int ext_flag = 0U;
	unsigned long temp_long = 0U; /* avoid MISRA C-2012 Rule 10.3 */

	if (atomic_read(&stcc_scrshare->status) != SCRSHARE_STS_READY) {
		(void)pr_err("%s: Not ready to send message status:%d\n",
			 __func__, atomic_read(&stcc_scrshare->status));
		ret = -100;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	mutex_lock(&stcc_scrshare->tx.lock);

	switch (cmd) {
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	case (unsigned int)IOCTL_TCC_SCRSHARE_SET_DSTINFO:	//set in sub-core
		temp_long = copy_from_user(stcc_scrshare_info->dstinfo, (void *)arg,
				   (unsigned long)sizeof(struct tcc_scrshare_dstinfo));
		if (temp_long < (ULONG_MAX / 2U)) {
			/* avoid MISRA C-2012 Rule 10.3 */
			ret = (long)temp_long;
		}

		if (ret != 0) {
			(void)pr_err("%s: unable to copy the paramter(%ld)\n",
				__func__, ret);
		} else {
			(void)pr_info("%s SET_DSTINFO x:%d, y:%d, w:%d, h:%d, img_num:%d\n",
				__func__, stcc_scrshare_info->dstinfo->x,
				stcc_scrshare_info->dstinfo->y,
				stcc_scrshare_info->dstinfo->width,
				stcc_scrshare_info->dstinfo->height,
				stcc_scrshare_info->dstinfo->img_num);
		}
		ext_flag = 1U;
		break;
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	case (unsigned int)IOCTL_TCC_SCRSHARE_GET_DSTINFO:	//call in main core
	case (unsigned int)IOCTL_TCC_SCRSHARE_GET_DSTINFO_KERNEL:
		(void)memset(&data, 0x0, sizeof(struct tcc_mbox_data));
		data.cmd[1] = ((unsigned int)SCRSHARE_CMD_GET_DSTINFO & 0xFFFFU) << 16U;
		ext_flag = 0U;
		break;
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	case (unsigned int)IOCTL_TCC_SCRSHARE_SET_SRCINFO:	//set in main core

		temp_long = copy_from_user(stcc_scrshare_info->srcinfo, (void *)arg,
				   (unsigned long)sizeof(struct tcc_scrshare_srcinfo));
		if (temp_long < (ULONG_MAX / 2U)) {
			/* avoid MISRA C-2012 Rule 10.3 */
			ret = (long)temp_long;
		}
		if (ret != 0) {
			(void)pr_err("%s: unable to copy the paramter(%ld)\n",
				__func__, ret);
		} else {
			(void)pr_info("%s SET_SRCINFO x:%d, y:%d, w:%d, h:%d\n",
				__func__, stcc_scrshare_info->srcinfo->x,
				stcc_scrshare_info->srcinfo->y,
				stcc_scrshare_info->srcinfo->width,
				stcc_scrshare_info->srcinfo->height);
		}
		ext_flag = 1U;
		break;
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	case (unsigned int)IOCTL_TCC_SCRSHARE_ON:	//set in main core
	case (unsigned int)IOCTL_TCC_SCRSHARE_ON_KERNEL:	//set in main core
		(void)memset(&data, 0x0, sizeof(struct tcc_mbox_data));
		data.cmd[1] = ((unsigned int)SCRSHARE_CMD_ON & 0xFFFFU) << 16U;
		ext_flag = 0U;
		break;
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	case (unsigned int)IOCTL_TCC_SCRSHARE_OFF:	//set in main core
	case (unsigned int)IOCTL_TCC_SCRSHARE_OFF_KERNEL:	//set in main core
		(void)memset(&data, 0x0, sizeof(struct tcc_mbox_data));
		data.cmd[1] = ((unsigned int)SCRSHARE_CMD_OFF & 0xFFFFU) << 16U;
		ext_flag = 0U;
		break;
	default:
		(void)pr_warn("%s: Invalid command (%d)\n", __func__, cmd);
		ret = -EINVAL;
		ext_flag = 1U;
		break;
	}
	if (ext_flag == 1U) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_ioctl;
	}

	/* Increase tx-sequence ID */
	atomic_inc(&stcc_scrshare->tx.seq);

	{
		int temp_int = 0;

		temp_int = atomic_read(&stcc_scrshare->tx.seq);
		if(temp_int >= 0) {
			data.cmd[0] = (unsigned int)temp_int;
		} else {
			(void)pr_err("%s: atomic_read err(%d)\n",
					__func__, temp_int);
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto err_ioctl;
		}
	}
	/* Initialize tx-wakup condition */
	mbox_done = 0;

	tcc_scrshare_send_message(stcc_scrshare, &data);
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
    /* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
    /* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
    /* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
    /* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
    /* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_4_violation : FALSE] */
    /* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
    /* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
    /* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[cert_int31_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	ret =
	    wait_event_interruptible_timeout(mbox_waitq, mbox_done == 1,
					     msecs_to_jiffies(100));
	if (ret <= 0) {
		/* Prevent KCS warning */
		(void)pr_err("%s: Timeout send_message(%ld)(%d)\n",
			__func__, ret, mbox_done);
	}
	else {
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_12_2_violation : FALSE] */
		if (cmd == (unsigned int)IOCTL_TCC_SCRSHARE_GET_DSTINFO) {
			unsigned long temp = 0; /* avoid MISRA C-2012 Rule 10.3 */

			/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
			/* coverity[cert_int36_c_violation : FALSE] */
			temp = copy_to_user((void *)arg,
					 stcc_scrshare_info->dstinfo,
					 (long)sizeof(struct tcc_scrshare_dstinfo));
			if (temp < (ULONG_MAX / 2U)) {
				/* avoid MISRA C-2012 Rule 10.3 */
				ret = (long)temp;
			}

			if (ret != 0) {
				(void)pr_err("%s: copy to user fail (%ld)\n",
				       __func__, ret);
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto err_ioctl;
			}
		}
	}
	mbox_done = 0;

err_ioctl:
	mutex_unlock(&stcc_scrshare->tx.lock);
FUNC_EXIT:
	return ret;
}

void tcc_scrshare_set_sharedBuffer(unsigned int addr, unsigned int frameWidth,
				   unsigned int frameHeight, unsigned int fmt, unsigned int rgb_swap)
{
	if ((stcc_scrshare_info == NULL) || (addr <= 0U) || (frameWidth <= 0U)
	    || (frameHeight <= 0U) || (fmt <= 0U)) {
		(void)pr_err("%s Invalid args addr:0x%08x, w:%d, h:%d, fmt:0x%x, rgb_swap:0x%x\n",
			__func__, addr, frameWidth, frameHeight, fmt, rgb_swap);
	} else {
		if ((stcc_scrshare_info->share_enable == 1U)
		    && (stcc_scrshare_info->srcinfo->width > 0U)) {
			long ret = 0;
			struct tcc_mbox_data data;
			unsigned int base0 = 0U, base1 = 0U, base2 =0U;

			(void)pr_info("%s addr:0x%08x, w:%d, h:%d, fmt:0x%x\n",
				__func__, addr, frameWidth, frameHeight, fmt);
			tcc_get_addr_yuv(fmt, addr, frameWidth, frameHeight,
				stcc_scrshare_info->srcinfo->x,
				stcc_scrshare_info->srcinfo->y, &base0, &base1, &base2);

			stcc_scrshare_info->src_addr = base0;
			stcc_scrshare_info->frm_w = frameWidth;
			stcc_scrshare_info->frm_h = frameHeight;
			stcc_scrshare_info->fmt = fmt;

			mutex_lock(&stcc_scrshare_device->tx.lock);
			(void)memset(&data, 0x0, sizeof(struct tcc_mbox_data));
			data.cmd[1] = ((unsigned int)SCRSHARE_CMD_SET_SRCINFO & 0xFFFFU) << 16U;
			data.data[0] = stcc_scrshare_info->src_addr;
			data.data[1] = stcc_scrshare_info->frm_w;
			data.data[2] = stcc_scrshare_info->frm_h;
			data.data[3] = stcc_scrshare_info->fmt;
			data.data[4] = rgb_swap;
			data.data_len = 5U;

			/* Increase tx-sequence ID */
			atomic_inc(&stcc_scrshare_device->tx.seq);

			{ /* avoid CERT-C Integers Rule INT31-C */
				int temp = 0;
				temp = atomic_read(&stcc_scrshare_device->tx.seq);
				if(temp >= 0) {
					data.cmd[0] = (unsigned int)temp;
				} else {
					(void)pr_err("%s: atomic_read err(%d)\n",
					       __func__, temp);
				}
			}

			/* Initialize tx-wakup condition */
			mbox_done = 0;

			tcc_scrshare_send_message(stcc_scrshare_device, &data);
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		    /* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		    /* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		    /* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		    /* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		    /* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_4_violation : FALSE] */
		    /* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		    /* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
		    /* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
			/* coverity[cert_int31_c_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			ret =
			    wait_event_interruptible_timeout(mbox_waitq, mbox_done == 1,
							     msecs_to_jiffies(100));
			if (ret <= 0) {
				(void)pr_err("%s: Timeout send_message(%ld)(%d)\n",
					 __func__, ret, mbox_done);
				(void)pr_err("sub-core seems to have some problem\n");
			}
			mbox_done = 0;
			mutex_unlock(&stcc_scrshare_device->tx.lock);
		}
	}
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tcc_scrshare_release(struct inode *inode_release, struct file *filp)
{
	/* avoid MISRA C-2012 Rule 2.7 */
    (void)filp;
	(void)inode_release;

	return 0;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tcc_scrshare_open(struct inode *inode_open, struct file *filp)
{
	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	struct tcc_scrshare_device *stcc_scrshare =
	    container_of(inode_open->i_cdev, struct tcc_scrshare_device, scdev);

	filp->private_data = stcc_scrshare;
	return 0;
}

static const struct file_operations tcc_scrshare_fops = {
	.owner = THIS_MODULE,
	.open = tcc_scrshare_open,
	.release = tcc_scrshare_release,
	.unlocked_ioctl = tcc_scrshare_ioctl,
};

static struct mbox_chan *
tcc_scrshare_request_channel(struct tcc_scrshare_device *stcc_scrshare,
	const char *name)
{
	struct mbox_chan *channel;

	stcc_scrshare->cl.dev = &stcc_scrshare->pdev->dev;
	stcc_scrshare->cl.rx_callback = tcc_scrshare_receive_message;
	stcc_scrshare->cl.tx_done = NULL;
#if defined(CONFIG_ARCH_TCC805X)
	stcc_scrshare->cl.tx_block = (bool)true;
	stcc_scrshare->cl.tx_tout = CLIENT_MBOX_TX_TIMEOUT;
#else
	stcc_scrshare->cl.tx_block = (bool)false;
	stcc_scrshare->cl.tx_tout = 0;	/*  doesn't matter here */
#endif
	stcc_scrshare->cl.knows_txdone = (bool)false;
	channel = mbox_request_channel_byname(&stcc_scrshare->cl, name);
	/* coverity[misra_c_2012_rule_11_2_violation : FALSE] */
	if (IS_ERR(channel)) {
		(void)pr_err("%s: Fail mbox_request_channel_byname(%s)\n",
			 __func__, name);
		channel = NULL;
	}

	return channel;
}

static int tcc_scrshare_rx_init(struct tcc_scrshare_device *stcc_scrshare)
{
	struct tcc_mbox_data data;
	int ret = 0;

	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	mutex_init(&stcc_scrshare->rx.lock);
	atomic_set(&stcc_scrshare->rx.seq, 0);

	stcc_scrshare->mbox_ch =
	    tcc_scrshare_request_channel(stcc_scrshare, stcc_scrshare->mbox_name);
	/* coverity[misra_c_2012_rule_11_2_violation : FALSE] */
	if (IS_ERR(stcc_scrshare->mbox_ch)) {
		/* coverity[misra_c_2012_rule_11_2_violation : FALSE] */
		ret = (int)PTR_ERR(stcc_scrshare->mbox_ch);
		(void)pr_err("%s: Fail tcc_scrshare_request_channel(%d)\n",
			 __func__, ret);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	(void)memset(&data, 0x0, sizeof(struct tcc_mbox_data));
	{	/* avoid CERT-C Integers Rule INT31-C */
		int temp = 0;

		temp = atomic_read(&stcc_scrshare->tx.seq);
		if(temp >= 0) {
			data.cmd[0] = (unsigned int)temp;
		} else {
			(void)pr_err("%s: atomic_read err(%d)\n",
					__func__, temp);
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto FUNC_EXIT;
		}
	}
	data.cmd[1] = (((unsigned int)SCRSHARE_CMD_READY & 0xFFFFU) << 16U) | TCC_SCRSHARE_SEND;
	tcc_scrshare_send_message(stcc_scrshare, &data);

FUNC_EXIT:
	return ret;
}

static void tcc_scrshare_tx_init(struct tcc_scrshare_device *stcc_scrshare)
{
	mutex_init(&stcc_scrshare->tx.lock);
	atomic_set(&stcc_scrshare->tx.seq, 0);

	mbox_done = 0;
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	init_waitqueue_head(&mbox_waitq);
}

static int tcc_scrshare_probe(struct platform_device *pdev)
{
	int ret = -ENODEV;
	struct tcc_scrshare_device *stcc_scrshare;
	struct tcc_scrshare_info *tcc_scrshare_disp_info;

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	stcc_scrshare =
	    devm_kzalloc(&pdev->dev, sizeof(struct tcc_scrshare_device),
			 GFP_KERNEL);
	if (stcc_scrshare == NULL) {
		ret = -ENOMEM;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	platform_set_drvdata(pdev, stcc_scrshare);

	(void)of_property_read_string(pdev->dev.of_node, "device-name",
				&stcc_scrshare->name);
	(void)of_property_read_string(pdev->dev.of_node, "mbox-names",
				&stcc_scrshare->mbox_name);
#if defined(CONFIG_ARCH_TCC805X)
	(void)of_property_read_string(pdev->dev.of_node, "mbox-id",
				&stcc_scrshare->mbox_id);
	(void)of_property_read_string(pdev->dev.of_node, "overlay_driver_names",
				&stcc_scrshare->overlay_driver_name);
#endif
	if (stcc_scrshare->overlay_driver_name != NULL) {
		/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		stcc_scrshare->overlay_driver_name =
			devm_kstrdup(&pdev->dev, OVERLAY_DRIVER, GFP_KERNEL);
	}

	ret = alloc_chrdev_region(&stcc_scrshare->devt, TCC_SCRSHARE_DEV_MINOR,
				  1, stcc_scrshare->name);
	if (ret != 0) {
		(void)pr_err("%s: Fail alloc_chrdev_region(%d)\n", __func__, ret);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	cdev_init(&stcc_scrshare->scdev, &tcc_scrshare_fops);
	stcc_scrshare->scdev.owner = THIS_MODULE;
	ret = cdev_add(&stcc_scrshare->scdev, stcc_scrshare->devt, 1);
	if (ret != 0) {
		(void)pr_err("%s: Fail cdev_add(%d)\n", __func__, ret);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	/* coverity[cert_dcl37_c_violation : FALSE] */
	stcc_scrshare->pclass = class_create(THIS_MODULE, stcc_scrshare->name);
	if (IS_ERR(stcc_scrshare->pclass)) {
		ret = (int)PTR_ERR(stcc_scrshare->pclass);
		(void)pr_err("%s: Fail class_create(%d)\n", __func__, ret);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	stcc_scrshare->dev = device_create(stcc_scrshare->pclass, &pdev->dev,
					  stcc_scrshare->devt, NULL,
					  stcc_scrshare->name);
	if (IS_ERR(stcc_scrshare->dev)) {
		ret = (int)PTR_ERR(stcc_scrshare->dev);
		(void)pr_err(" %s: Fail device_create(%d)\n", __func__, ret);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	stcc_scrshare->pdev = pdev;

	atomic_set(&stcc_scrshare->status, SCRSHARE_STS_INIT);

	tcc_scrshare_tx_init(stcc_scrshare);
	if (tcc_scrshare_rx_init(stcc_scrshare) != 0) {
		(void)pr_err(" %s: Fail tcc_scrshare_rx_init\n", __func__);
		ret = -EFAULT;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	stcc_scrshare_device = stcc_scrshare;

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	tcc_scrshare_disp_info =
	    kzalloc(sizeof(struct tcc_scrshare_info), GFP_KERNEL);
	if (tcc_scrshare_disp_info == NULL) {
		ret = -ENOMEM;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	tcc_scrshare_disp_info->srcinfo =
	    kzalloc(sizeof(struct tcc_scrshare_srcinfo), GFP_KERNEL);
	if (tcc_scrshare_disp_info->srcinfo == NULL) {
		kfree(tcc_scrshare_disp_info->srcinfo);
		kfree(tcc_scrshare_disp_info);
		(void)pr_err(" err_scrshare_mem.\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	tcc_scrshare_disp_info->dstinfo =
	    kzalloc(sizeof(struct tcc_scrshare_dstinfo), GFP_KERNEL);
	if (tcc_scrshare_disp_info->dstinfo == NULL) {
		kfree(tcc_scrshare_disp_info->srcinfo);
		kfree(tcc_scrshare_disp_info);
		(void)pr_err(" err_scrshare_mem.\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	tcc_scrshare_disp_info->dstinfo->x = 640;
	tcc_scrshare_disp_info->dstinfo->y = 0;
	tcc_scrshare_disp_info->dstinfo->width = 640;
	tcc_scrshare_disp_info->dstinfo->height = 720;
	tcc_scrshare_disp_info->dstinfo->img_num = 1;
	stcc_scrshare_info = tcc_scrshare_disp_info;

	(void)pr_info(" %s:%s Driver Initialized\n", __func__, stcc_scrshare->name);
	/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
	ret = 0;

FUNC_EXIT:
	return ret;
}

static int tcc_scrshare_remove(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_scrshare_device *stcc_scrshare = platform_get_drvdata(pdev);

	kfree(stcc_scrshare_info->dstinfo);
	kfree(stcc_scrshare_info->srcinfo);
	kfree(stcc_scrshare_info);

	device_destroy(stcc_scrshare->pclass, stcc_scrshare->devt);
	class_destroy(stcc_scrshare->pclass);
	cdev_del(&stcc_scrshare->scdev);
	unregister_chrdev_region(stcc_scrshare->devt, 1);

	devm_kfree(&pdev->dev, stcc_scrshare);
	return 0;
}

#ifdef CONFIG_PM
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tcc_scrshare_suspend(struct platform_device *pdev,
				 pm_message_t state)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_scrshare_device *stcc_scrshare = platform_get_drvdata(pdev);
	int ret = 0;

	/* avoid MISRA C-2012 Rule 2.7 */
    (void)state;

	/* unregister mailbox client */
	if (stcc_scrshare->mbox_ch != NULL) {
		mbox_free_channel(stcc_scrshare->mbox_ch);
		stcc_scrshare->mbox_ch = NULL;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tcc_scrshare_resume(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_scrshare_device *stcc_scrshare = platform_get_drvdata(pdev);
	int ret = 0;

	/* register mailbox client */
	stcc_scrshare->mbox_ch =
	 tcc_scrshare_request_channel(stcc_scrshare, stcc_scrshare->mbox_name);

	/* coverity[misra_c_2012_rule_11_2_violation : FALSE] */
	if (IS_ERR(stcc_scrshare->mbox_ch)) {
		/* coverity[misra_c_2012_rule_11_2_violation : FALSE] */
		ret = (int)PTR_ERR(stcc_scrshare->mbox_ch);
		(void)pr_err("%s: Fail request_channel (%d)\n", __func__, ret);
	}

	return ret;
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id tcc_scrshare_of_match[] = {
	{.compatible = "telechips,tcc_scrshare",},
	{},
};

MODULE_DEVICE_TABLE(of, tcc_scrshare_ctrl_of_match);
#endif

static struct platform_driver tcc_scrshare = {
	.probe = tcc_scrshare_probe,
	.remove = tcc_scrshare_remove,
	.driver = {
		   .name = "tcc_scrshare",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = tcc_scrshare_of_match,
#endif
		   },
#ifdef CONFIG_PM
	.suspend = tcc_scrshare_suspend,
	.resume = tcc_scrshare_resume,
#endif
};

static int __init tcc_scrshare_init(void)
{
	return platform_driver_register(&tcc_scrshare);
}

static void __exit tcc_scrshare_exit(void)
{
	platform_driver_unregister(&tcc_scrshare);
}

/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
module_init(tcc_scrshare_init);
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
module_exit(tcc_scrshare_exit);

/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_AUTHOR("Telechips Inc.");
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_DESCRIPTION("Telechips SCREEN_SHARE Driver");
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_LICENSE("GPL");

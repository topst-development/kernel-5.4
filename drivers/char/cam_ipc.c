// SPDX-License-Identifier: GPL-2.0-or-later
/*******************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 ******************************************************************************/

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

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/io.h>

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>

#include <video/tcc/cam_ipc.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_config.h>

#define LOG_TAG			("CAM_IPC")

/* coverity[cert_arr39_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_10_violation : FALSE] */
#define loge(fmt, ...)		{ (void)pr_err("[ERROR][%s] %s - " fmt, LOG_TAG, \
					__func__, ##__VA_ARGS__); }
/* coverity[cert_arr39_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_10_violation : FALSE] */
#define logw(fmt, ...)		{ (void)pr_warn("[WARN][%s] %s - " fmt, LOG_TAG, \
					__func__, ##__VA_ARGS__); }
/* coverity[cert_arr39_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_10_violation : FALSE] */
#define logd(fmt, ...)		{ (void)pr_debug("[DEBUG][%s] %s - " fmt, LOG_TAG, \
					__func__, ##__VA_ARGS__); }
/* coverity[cert_arr39_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_10_violation : FALSE] */
#define logi(fmt, ...)		{ (void)pr_info("[INFO][%s] %s - " fmt, LOG_TAG, \
					__func__, ##__VA_ARGS__); }

#define CAM_IPC_DEV_MINOR		(0)

struct cam_ipc_tx {
	struct mutex lock;
	atomic_t seq;
};

struct cam_ipc_rx {
	struct mutex lock;
	atomic_t seq;
};

struct cam_ipc_device {
	struct platform_device *pdev;
	struct device *dev;
	struct cdev m_cdev;
	struct class *m_class;
	dev_t devt;
	const char *name;
	const char *mbox_name;
#if defined(CONFIG_ARCH_TCC805X)
	const char *mbox_id;
#endif
	struct mbox_chan *mbox_ch;
	struct mbox_client cl;

	atomic_t status;

	struct cam_ipc_tx tx;
	struct cam_ipc_rx rx;
};

struct cam_ipc_ovp_data {
	uint32_t	cmd;
	uint32_t	wmix_ch;
	uint32_t	ovp;
	uint32_t	data_len;
};

static struct cam_ipc_device *cam_ipc_dev;
static wait_queue_head_t mbox_waitq;
static int mbox_done;

/* Function: cam_ipc_set_ovp
 * Description: Set the layer-order of WMIXx block
 * data[0]: WMIX Block Number
 * data[1]: ovp (Please refer to the full spec)
 */
static void cam_ipc_set_ovp(const struct tcc_mbox_data *msg)
{
	if (msg != NULL) {
		VIOC_WMIX_SetOverlayPriority(
			VIOC_WMIX_GetAddress(msg->data[0]), msg->data[1]);
		VIOC_WMIX_SetUpdate(
			VIOC_WMIX_GetAddress(msg->data[0]));
	} else {
		loge("tcc_mbox_data struct is NULL\n");
	}
}

/* Function : cam_ipc_set_pos
 * Description : Set the position of WMIXx block
 * data[0] : WMIX block number
 * data[1] : the input channel
 * data[2] : x-position
 * data[3] : y-position
 */
static void cam_ipc_set_pos(const struct tcc_mbox_data *msg)
{
	if (msg != NULL) {
		VIOC_WMIX_SetPosition(VIOC_WMIX_GetAddress(msg->data[0]),
			msg->data[1], msg->data[2], msg->data[3]);
		VIOC_WMIX_SetUpdate(VIOC_WMIX_GetAddress(msg->data[0]));
	} else {
		/* msg is NULL */
		loge("tcc_mbox_data struct is NULL\n");
	}
}

/* Function : cam_ipc_set_reset
 * Description : VIOC Block SWReset
 * data[0] : VIOC Block Number
 * data[1] : mode (0: clear, 1: reset)
 */
static void cam_ipc_set_reset(const struct tcc_mbox_data *msg)
{
	if (msg != NULL) {
		/* msg is available */
		VIOC_CONFIG_SWReset(msg->data[0], msg->data[1]);
	} else {
		/* msg is unavailable */
		loge("tcc_mbox_data struct is NULL\n");
	}
}

static void cam_ipc_send_message(const struct cam_ipc_device *p_cam_ipc,
	struct tcc_mbox_data *msg)
{
	if (p_cam_ipc != NULL) {
		int32_t ret;

		ret = mbox_send_message(p_cam_ipc->mbox_ch, msg);
#if defined(CONFIG_ARCH_TCC805X)
		if (ret < 0)
			loge("camipc manager mbox send error(%d)\n", ret);
#else
		mbox_client_txdone(p_cam_ipc->mbox_ch, ret);
#endif
	}
}

static int cam_ipc_check_rx_seq(const struct cam_ipc_device *p_cam_ipc,
	const struct tcc_mbox_data *data)
{
	int ret = 0;
	int status = 0;
	if (data->cmd[0] > 0U) {
		status = atomic_read(&p_cam_ipc->rx.seq);

		if ((status > 0) && ((unsigned)status > data->cmd[0])) {
			loge("already processed command(%d,%d)\n",
				atomic_read(&p_cam_ipc->rx.seq),
				data->cmd[1]);
			ret = -1;
		}
	} else {
		ret = -1;
	}
	return ret;
}

static void cam_ipc_update_rx_seq_id(struct cam_ipc_device *p_cam_ipc,
	struct tcc_mbox_data *data)
{
	int val;
	uint32_t temp;

	temp = data->cmd[0];

	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	if (temp < INT_MAX) {
		val = (int)temp;
	} else {
		val = 0;
		loge("Failed to convert data->cmd[0]\n");
	}

	/* Update rx-sequence ID */
	if (data->cmd[0] != 0U) {
		data->cmd[1] |= (uint32_t)CAM_IPC_ACK;
		cam_ipc_send_message(p_cam_ipc, data);
		atomic_set(&p_cam_ipc->rx.seq, val);
	}
}

static void cam_ipc_cmd_handler(struct cam_ipc_device *p_cam_ipc,
				struct tcc_mbox_data *data)
{
	int validate = 1;
	uint32_t cmd = (uint32_t)((data->cmd[1] >> 16) & 0xFFFFU);

	if (cam_ipc_check_rx_seq(p_cam_ipc, data) == 0) {
		switch (cmd) {
		case (uint32_t)CAM_IPC_CMD_OVP:
			cam_ipc_set_ovp(data);
			break;
		case (uint32_t)CAM_IPC_CMD_POS:
			cam_ipc_set_pos(data);
			break;
		case (uint32_t)CAM_IPC_CMD_RESET:
			cam_ipc_set_reset(data);
			break;
		default:
			logi("Invalid command(%d)\n", cmd);
			validate = 0;
			break;
		}

		if (validate == 1) {
			cam_ipc_update_rx_seq_id(p_cam_ipc, data);
		}
	}
}

static void cam_ipc_check_sts_to_handle(struct cam_ipc_device *p_cam_ipc,
	uint32_t pstate, struct tcc_mbox_data *msg,
	void (*handle)(struct cam_ipc_device *p_cam_ipc, struct tcc_mbox_data *data))
{
	uint32_t status = 0;
	int ret;

	ret = atomic_read(&p_cam_ipc->status);

	if (ret >= 0) {
		status = (uint32_t)ret;
	} else {
		loge("Wrong status value: conversion error\n");
	}
	if ((status & pstate) != 0U) {
		handle(p_cam_ipc, msg);
	}
}

static void cam_ipc_handle_cmd_ops(struct cam_ipc_device *p_cam_ipc,
				struct tcc_mbox_data *msg)
{
	if ((p_cam_ipc != NULL) && (msg != NULL)) {
		if ((msg->cmd[1] & (uint32_t)CAM_IPC_ACK) != 0U) {
			mbox_done = 1;
			wake_up_interruptible(&mbox_waitq);
		} else {
			mutex_lock(&p_cam_ipc->rx.lock);
			cam_ipc_cmd_handler(p_cam_ipc, msg);
		}
		mutex_unlock(&p_cam_ipc->rx.lock);
	} else {
		loge("Device pointers are NULL\n");
	}
}

static void cam_ipc_handle_cmd_ready(struct cam_ipc_device *p_cam_ipc,
	struct tcc_mbox_data *msg)
{
	atomic_set(&p_cam_ipc->status, CAM_IPC_STS_READY);
	if ((msg->cmd[1] & (uint32_t)CAM_IPC_ACK) == 0U) {
		msg->cmd[1] |= (uint32_t)CAM_IPC_ACK;
		cam_ipc_send_message(p_cam_ipc, msg);
	}
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void cam_ipc_handle_cmd_status(struct cam_ipc_device *p_cam_ipc,
	struct tcc_mbox_data *msg)
{
	msg->cmd[1] |= (uint32_t)CAM_IPC_STATUS;
	cam_ipc_send_message(p_cam_ipc, msg);
}

static void cam_ipc_receive_message(struct mbox_client *client, void *p_msg)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_mbox_data *msg = (struct tcc_mbox_data *)p_msg;
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	struct cam_ipc_device *p_cam_ipc =
		container_of(client, struct cam_ipc_device, cl);
	uint32_t command  = (uint32_t)((msg->cmd[1] >> 16) & 0xFFFFU);

	switch (command) {
	case (uint32_t)CAM_IPC_CMD_OVP:
	case (uint32_t)CAM_IPC_CMD_POS:
	case (uint32_t)CAM_IPC_CMD_RESET:
		cam_ipc_check_sts_to_handle(p_cam_ipc, (unsigned)CAM_IPC_STS_READY,
			msg,
			cam_ipc_handle_cmd_ops);
		break;
	case (uint32_t)CAM_IPC_CMD_READY:
		cam_ipc_check_sts_to_handle(p_cam_ipc,
			(unsigned)CAM_IPC_STS_INIT | (unsigned)CAM_IPC_STS_READY,
			msg,
			cam_ipc_handle_cmd_ready);
		break;
	case (uint32_t)CAM_IPC_CMD_STATUS:
		cam_ipc_check_sts_to_handle(p_cam_ipc,
			(unsigned)CAM_IPC_STS_READY, msg,
			cam_ipc_handle_cmd_status);
		break;
	case (uint32_t)CAM_IPC_CMD_NULL:
	case (uint32_t)CAM_IPC_CMD_MAX:
	default:
		logi("Invalid command(%d)\n", msg->cmd[1]);
		break;
	}
}

static struct cam_ipc_device *cam_ipc_get_dev_obj(const struct file *fp)
{
	struct cam_ipc_device *ret = NULL;

	if (fp != NULL) {
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		ret = fp->private_data;
	} else {
		loge("Passed file pointer is NULL\n");
	}
	return ret;
}

static int cam_ipc_is_initialized(const struct cam_ipc_device *p_cam_ipc)
{
	uint32_t status = 0;
	int temp = 0;
	int ret = 0;

	temp = atomic_read(&p_cam_ipc->status);
	if (temp >= 0) {
		status = (uint32_t)temp;

		if (status != (uint32_t)CAM_IPC_STS_READY) {
			loge("Not ready to send message\n");
			ret = -100;
		}
	}

	return ret;
}

static void cam_ipc_copy_ioctl_args(struct tcc_mbox_data *data,
	unsigned long arg)
{
	/* coverity[cert_int36_c_violation : FALSE] */
	(void)memcpy(data, (struct tcc_mbox_data *)arg,
		sizeof(struct tcc_mbox_data));
}

static void cam_ipc_handle_ioctl_setovp(void *to, const void *from)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_mbox_data *dst = to;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct cam_ipc_ovp_data *src = from;

	/* coverity[misra_c_2012_rule_17_7_violation : FALSE] */
	(void)memset(dst, 0x0, sizeof(struct tcc_mbox_data));

	dst->cmd[1] = src->cmd;
	dst->data[0] = src->wmix_ch;
	dst->data[1] = src->ovp;
	dst->data_len = src->data_len;
}

static void cam_ipc_handle_ioctl_convert(void *data,
				uintptr_t arg, size_t data_size,
				struct tcc_mbox_data *msg,
				void (*convert)(void *to, const void *from))
{
	unsigned long ret = 0;
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	if (data_size < (unsigned)INT_MAX) {
		/* coverity[cert_int36_c_violation : FALSE] */
		/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
		ret = copy_from_user(data, (void *)arg, data_size);
		if (ret > 0UL) {
			loge("Failed to copy_from_user");
		} else {
			convert(msg, data);
		}
	} else {
		loge("Wrong conversion of data size to integer type\n");
	}
}

static void cam_ipc_handle_ioctl(void *data,
				uintptr_t arg, size_t data_size,
				struct tcc_mbox_data *msg,
				void (*convert)(void *to, const void *from))
{
	unsigned long ret;
	if (convert != NULL) {
		cam_ipc_handle_ioctl_convert(data, arg, data_size, msg, convert);
	} else {
		/* coverity[cert_int36_c_violation : FALSE] */
		/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
		ret = copy_from_user(data, (void *)arg, data_size);
		if (ret > 0UL) {
			loge("Failed to copy_from_user");
		}
	}
}

static void cam_ipc_inc_tx_seq(struct cam_ipc_device *p_cam_ipc,
	struct tcc_mbox_data *data)
{
	int ret;
	/* Increase tx-sequence ID */
	atomic_inc(&p_cam_ipc->tx.seq);
	ret = atomic_read(&p_cam_ipc->tx.seq);
	if (ret >= 0) {
		data->cmd[0] = (uint32_t)ret;
	}

	/* Initialize tx-wakup condition */
	mbox_done = 0;
}

/* coverity[HIS_metric_violation : FALSE] */
static void cam_ipc_send_and_check_timeout(const struct cam_ipc_device *p_cam_ipc,
	struct tcc_mbox_data *data)
{
	int ret;
	cam_ipc_send_message(p_cam_ipc, data);
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_int02_c_violation : FALSE] */
	/* coverity[cert_int31_c_violation : FALSE] */
	ret = wait_event_interruptible_timeout(mbox_waitq,
		(mbox_done == 1), msecs_to_jiffies(100));

	if (ret <= 0) {
		loge("Timeout cam_ipc_send_message(%d)(%d)\n",
			ret, mbox_done);
	}
	mbox_done = 0;
}

static void cam_ipc_send_mbox_msg(struct cam_ipc_device *p_cam_ipc,
	struct tcc_mbox_data *data)
{
	cam_ipc_inc_tx_seq(p_cam_ipc, data);
	cam_ipc_send_and_check_timeout(p_cam_ipc, data);
}

static int cam_ipc_do_send(struct cam_ipc_device *p_cam_ipc,
	uint32_t cmd, unsigned long arg)
{
	struct tcc_mbox_data data;
	struct cam_ipc_ovp_data ovp_data;
	int ret = 0;

	mutex_lock(&p_cam_ipc->tx.lock);

	switch (cmd) {
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	case (unsigned)IOCTL_CAM_IPC_SET_OVP:
		cam_ipc_handle_ioctl(&ovp_data, arg,
				sizeof(struct cam_ipc_ovp_data),
				&data, cam_ipc_handle_ioctl_setovp);
		break;
	case (unsigned)IOCTL_CAM_IPC_SET_POS:
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	case (unsigned)IOCTL_CAM_IPC_SET_RESET:
		cam_ipc_handle_ioctl(&data, arg, sizeof(struct tcc_mbox_data),
				&data, NULL);
		break;
	case (unsigned)IOCTL_CAM_IPC_SET_OVP_KERNEL:
	case (unsigned)IOCTL_CAM_IPC_SET_POS_KERNEL:
	case (unsigned)IOCTL_CAM_IPC_SET_RESET_KERNEL:
		cam_ipc_copy_ioctl_args(&data, arg);
		break;
	default:
		logi("Invalid command (%d)\n", cmd);
		ret = -EINVAL;
		break;
	}
	cam_ipc_send_mbox_msg(p_cam_ipc, &data);

	mutex_unlock(&p_cam_ipc->tx.lock);

	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static long cam_ipc_ioctl(struct file *filp,
	uint32_t cmd, unsigned long arg)
{
	struct cam_ipc_device *p_cam_ipc = cam_ipc_get_dev_obj(filp);
	int ret;

	if (p_cam_ipc == NULL) {
		ret = -ENODEV;
	} else {
		 if (cam_ipc_is_initialized(p_cam_ipc) == 0) {
			ret = cam_ipc_do_send(p_cam_ipc, cmd, arg);
		 } else {
			 ret = -EINVAL;
		 }
	}

	return (long)ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int cam_ipc_open(struct inode *pnode, struct file *filp)
{
	struct cam_ipc_device *p_cam_ipc = NULL;
	int ret = 0;

	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	p_cam_ipc = container_of(pnode->i_cdev, struct cam_ipc_device, m_cdev);
	if (filp != NULL) {
		filp->private_data = p_cam_ipc;
	} else {
		loge("An object of filp is NULL\n");
		ret = -ENODEV;
	}

	return ret;
}

const static struct file_operations cam_ipc_fops = {
	.owner    = THIS_MODULE,
	.open     = cam_ipc_open,
	.unlocked_ioctl = cam_ipc_ioctl,
};

static struct mbox_chan *cam_ipc_request_channel(
	struct cam_ipc_device *p_cam_ipc, const char *name)
{
	struct mbox_chan *channel;

	p_cam_ipc->cl.dev = &p_cam_ipc->pdev->dev;
	p_cam_ipc->cl.rx_callback = cam_ipc_receive_message;
	p_cam_ipc->cl.tx_done = NULL;
#if defined(CONFIG_ARCH_TCC805X)
	p_cam_ipc->cl.tx_block = (bool)true;
	p_cam_ipc->cl.tx_tout = 500;
#else
	p_cam_ipc->cl.tx_block = (bool)false;
	p_cam_ipc->cl.tx_tout = 0; /*  doesn't matter here*/
#endif
	/* coverity[misra_c_2012_rule_11_2_violation : FALSE] */
	p_cam_ipc->cl.knows_txdone = (bool)false;
	channel = mbox_request_channel_byname(&p_cam_ipc->cl, name);
	/* coverity[misra_c_2012_rule_11_2_violation : FALSE] */
	if (IS_ERR(channel)) {
		loge("Fail mbox_request_channel_byname(%s)\n", name);
		channel = NULL;
	}

	return channel;
}

static int cam_ipc_send_ready(struct cam_ipc_device *p_cam_ipc)
{
	int32_t ret = 0;
	int32_t temp;
	struct tcc_mbox_data data;

	p_cam_ipc->mbox_ch =
		cam_ipc_request_channel(p_cam_ipc, p_cam_ipc->mbox_name);

	/* coverity[misra_c_2012_rule_11_2_violation : FALSE] */
	if (IS_ERR(p_cam_ipc->mbox_ch)) {
		ret = (int32_t)PTR_ERR(p_cam_ipc->mbox_ch);
		loge("Fail cam_ipc_request_channel(%d)\n", ret);
	} else {
		/* coverity[misra_c_2012_rule_17_7_violation : FALSE] */
		memset(&data, 0x0, sizeof(struct tcc_mbox_data));

		temp = atomic_read(&p_cam_ipc->tx.seq);
		if (temp >= 0) {
			data.cmd[0] = (uint32_t)temp;
		}

		data.cmd[1] = (((unsigned)CAM_IPC_CMD_READY & 0xFFFFU) << 16U)
			| (unsigned)CAM_IPC_SEND;
		cam_ipc_send_message(p_cam_ipc, &data);
	}
	return ret;
}

static void cam_ipc_rx_init(struct cam_ipc_device *p_cam_ipc)
{
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	mutex_init(&p_cam_ipc->rx.lock);
	atomic_set(&p_cam_ipc->rx.seq, 0);
}

static void cam_ipc_tx_init(struct cam_ipc_device *p_cam_ipc)
{
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	mutex_init(&p_cam_ipc->tx.lock);
	atomic_set(&p_cam_ipc->tx.seq, 0);

	mbox_done = 0;
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	init_waitqueue_head(&mbox_waitq);
}

static int cam_ipc_probe_f_init(struct platform_device *pdev)
{
	struct cam_ipc_device *p_cam_ipc = NULL;
	int ret = 0;

	if (pdev == NULL) {
		loge("pdev is NULL");
		ret = -ENODEV;
	} else {
		/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		p_cam_ipc = devm_kzalloc(&pdev->dev,
				sizeof(struct cam_ipc_device), GFP_KERNEL);
		if (p_cam_ipc == NULL) {
			ret = -ENOMEM;
		} else {
			platform_set_drvdata(pdev, p_cam_ipc);
		}
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int cam_ipc_probe_f_parse_dt(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct cam_ipc_device *p_cam_ipc = platform_get_drvdata(pdev);

	bool ret;
	ret = (of_property_read_string(pdev->dev.of_node,
				"device-name", &p_cam_ipc->name) == 0) &&
		(of_property_read_string(pdev->dev.of_node,
				"mbox-names", &p_cam_ipc->mbox_name) == 0);

#if defined(CONFIG_ARCH_TCC805X)
	ret = ret && (of_property_read_string(pdev->dev.of_node,
				"mbox-id", &p_cam_ipc->mbox_id) == 0);
#endif
	return (int)ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int cam_ipc_probe_f_init_cdev(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct cam_ipc_device *p_cam_ipc = platform_get_drvdata(pdev);

	int ret;

	ret = alloc_chrdev_region(&p_cam_ipc->devt, CAM_IPC_DEV_MINOR,
				1, p_cam_ipc->name);
	if (ret > 0) {
		loge("Fail alloc_chrdev_region(%d)\n", ret);
	} else {
		cdev_init(&p_cam_ipc->m_cdev, &cam_ipc_fops);
		p_cam_ipc->m_cdev.owner = THIS_MODULE;
		ret = cdev_add(&p_cam_ipc->m_cdev, p_cam_ipc->devt, 1);
		if (ret > 0) {
			loge("Fail cdev_add(%d)\n", ret);
		}
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int cam_ipc_probe_f_init_class(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct cam_ipc_device *p_cam_ipc = platform_get_drvdata(pdev);

	int ret = 0;

	/* coverity[cert_dcl37_c_violation : FALSE] */
	p_cam_ipc->m_class = class_create(THIS_MODULE, p_cam_ipc->name);
	if (IS_ERR(p_cam_ipc->m_class)) {
		ret = (int)PTR_ERR(p_cam_ipc->m_class);
		loge("Fail class_create(%d)\n", ret);
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FASLE] */
static int cam_ipc_probe_f_init_device(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct cam_ipc_device *p_cam_ipc = platform_get_drvdata(pdev);

	int ret = 0;

	p_cam_ipc->dev = device_create(p_cam_ipc->m_class, &pdev->dev,
				     p_cam_ipc->devt, NULL, p_cam_ipc->name);
	if (IS_ERR(p_cam_ipc->dev)) {
		ret = (int)PTR_ERR(p_cam_ipc->dev);
		loge("Fail device_create(%d)\n", ret);
	} else {
		p_cam_ipc->pdev = pdev;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int cam_ipc_probe_f_init_txrx(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct cam_ipc_device *p_cam_ipc = platform_get_drvdata(pdev);

	int ret;

	atomic_set(&p_cam_ipc->status, CAM_IPC_STS_INIT);

	cam_ipc_tx_init(p_cam_ipc);
	cam_ipc_rx_init(p_cam_ipc);

	ret = cam_ipc_send_ready(p_cam_ipc);
	if (ret > 0) {
		loge("Failed on cam_ipc_rx_init\n");
		ret = -EFAULT;
	} else {
		cam_ipc_dev = p_cam_ipc;
	}

	return ret;
}


static int (*cam_ipc_probe_func[CAM_IPC_PROBE_MAX])(
	struct platform_device *pdev) = {
	cam_ipc_probe_f_init,
	cam_ipc_probe_f_parse_dt,
	cam_ipc_probe_f_init_cdev,
	cam_ipc_probe_f_init_class,
	cam_ipc_probe_f_init_device,
	cam_ipc_probe_f_init_txrx
};

static int cam_ipc_probe(struct platform_device *pdev)
{
	const struct cam_ipc_device *p_cam_ipc;
	int ret = 0;
	int step;

	for (step = CAM_IPC_PROBE_INIT_OBJ; step < CAM_IPC_PROBE_MAX; step++) {
		ret = cam_ipc_probe_func[step](pdev);
		if (ret < 0) {
			break;
		}
	}

	if (ret == 0) {
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		p_cam_ipc = platform_get_drvdata(pdev);
		logi("%s Driver Initialized\n", p_cam_ipc->name);
	} else {
		loge("Failed to initialize cam_ipc driver\n");
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int cam_ipc_remove(struct platform_device *pdev)
{
	struct cam_ipc_device *p_cam_ipc = NULL;
	int ret = 0;

	if (pdev == NULL) {
		loge("pdev is NULL");
	} else {
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		p_cam_ipc = platform_get_drvdata(pdev);
		if (p_cam_ipc != NULL) {
			device_destroy(p_cam_ipc->m_class, p_cam_ipc->devt);
			class_destroy(p_cam_ipc->m_class);
			cdev_del(&p_cam_ipc->m_cdev);
			unregister_chrdev_region(p_cam_ipc->devt, 1);
		} else {
			loge("cam_ipc struct is NULL");
			ret = -1;
		}
	}

	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id cam_ipc_of_match[] = {
	{.compatible = "telechips,cam_ipc", },
	{ },
};
MODULE_DEVICE_TABLE(of, cam_ipc_ctrl_of_match);
#endif

static struct platform_driver cam_ipc = {
	.probe	= cam_ipc_probe,
	.remove	= cam_ipc_remove,
	.driver	= {
		.name	= "cam_ipc",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = cam_ipc_of_match,
#endif
	},
};

static int __init cam_ipc_init(void)
{
	return platform_driver_register(&cam_ipc);
}

static void __exit cam_ipc_exit(void)
{
	platform_driver_unregister(&cam_ipc);
}

/* coverity[cert_dcl37_c_violation : SUPPRESS] */
/* coverity[misra_c_2012_rule_20_7_violation : SUPPRESS] */
/* coverity[misra_c_2012_rule_21_2_violation : SUPPRESS] */
module_init(cam_ipc_init);

/* coverity[cert_dcl37_c_violation : SUPPRESS] */
/* coverity[misra_c_2012_rule_20_7_violation : SUPPRESS] */
/* coverity[misra_c_2012_rule_21_2_violation : SUPPRESS] */
module_exit(cam_ipc_exit);

/* coverity[cert_dcl37_c_violation : SUPPRESS] */
/* coverity[misra_c_2012_rule_21_2_violation : SUPPRESS] */
MODULE_AUTHOR("Telechips Inc.");

/* coverity[cert_dcl37_c_violation : SUPPRESS] */
/* coverity[misra_c_2012_rule_21_2_violation : SUPPRESS] */
MODULE_DESCRIPTION("CAMIPC Manager Driver");

/* coverity[cert_dcl37_c_violation : SUPPRESS] */
/* coverity[misra_c_2012_rule_21_2_violation : SUPPRESS] */
MODULE_LICENSE("GPL");

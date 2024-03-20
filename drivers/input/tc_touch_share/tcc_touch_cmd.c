// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include "tcc_touch_cmd.h"

static void touch_mbox_send(struct mbox_chan *chan,
		struct tcc_mbox_data *msg)
{
	if ((chan != NULL) && (msg != NULL)) {
		(void)mbox_send_message(chan, msg);
	}
}

void touch_send_init(struct touch_mbox *touch_dev, uint32_t touch_state)
{
	if (touch_dev != NULL) {
		struct tcc_mbox_data sendMsg;

		(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));
		sendMsg.cmd[0] = (int32_t)TOUCH_INIT;
		sendMsg.cmd[1] = touch_state;

		mutex_lock(&touch_dev->lock);
		touch_mbox_send(touch_dev->touch_mbox_channel,
				&sendMsg);
		mutex_unlock(&touch_dev->lock);
	}
}

void touch_send_change(struct touch_mbox *touch_dev, uint32_t touch_state)
{
	if (touch_dev != NULL) {
		struct tcc_mbox_data sendMsg;

		(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));
		sendMsg.cmd[0] = (int32_t)TOUCH_STATE;
		sendMsg.cmd[1] = touch_state;

		mutex_lock(&touch_dev->lock);
		touch_mbox_send(touch_dev->touch_mbox_channel,
				&sendMsg);
		mutex_unlock(&touch_dev->lock);
	}
}

void touch_send_data(struct touch_mbox *touch_dev, struct tcc_mbox_data *msg)
{
	if (touch_dev != NULL) {
		mutex_lock(&touch_dev->lock);
		touch_mbox_send(touch_dev->touch_mbox_channel, msg);
		mutex_unlock(&touch_dev->lock);
	}
}

void touch_send_ack(struct touch_mbox *touch_dev, uint32_t cmd,
		uint32_t touch_state)
{
	if (touch_dev != NULL) {
		struct tcc_mbox_data sendMsg;

		(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));
		sendMsg.cmd[0] = (int32_t)TOUCH_ACK;
		sendMsg.cmd[1] = cmd;
		sendMsg.cmd[2] = touch_state;

		mutex_lock(&touch_dev->lock);
		touch_mbox_send(touch_dev->touch_mbox_channel,
				&sendMsg);
		mutex_unlock(&touch_dev->lock);
	}

}

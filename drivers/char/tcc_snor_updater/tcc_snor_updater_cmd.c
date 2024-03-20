// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/of_device.h>

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include <linux/tcc_ipc.h>
#include "tcc_snor_updater_typedef.h"
#include "tcc_snor_updater_mbox.h"
#include "tcc_snor_updater_cmd.h"

#define MBOX_TIMEOUT		(1000)	//ms
#define ACK_TIMEOUT			(5000U)	//ms
#define MBOX_ERASE_TIMEOUT	(30000U)	//ms

void snor_updater_event_create(
	struct snor_updater_device *updater_dev)
{
	if (updater_dev != NULL) {
		init_waitqueue_head(&updater_dev->waitQueue.cmdQueue);
		updater_dev->waitQueue.condition = 0;
	}
}

void snor_updater_event_delete(
	struct snor_updater_device *updater_dev)
{
	if (updater_dev != NULL) {
		updater_dev->waitQueue.condition = 0;
	}
}

int32_t snor_updater_wait_event_timeout(
	struct snor_updater_device *updater_dev,
	struct tcc_mbox_data *receiveMsg,
	uint32_t timeOut)
{
	int32_t ret = -EINVAL;

	if (updater_dev != NULL) {
		long wait_ret;

		wait_ret = wait_event_interruptible_timeout(
			updater_dev->waitQueue.cmdQueue,
			updater_dev->waitQueue.condition == 0,
			msecs_to_jiffies(timeOut));

		if ((updater_dev->waitQueue.condition == 1)
			|| (wait_ret <= 0L)) {
			/* timeout */
			ret = SNOR_UPDATER_ERR_TIMEOUT;
		} else	{
			int32_t i;

			for (i = 0; i < MBOX_CMD_FIFO_SIZE; i++) {
				receiveMsg->cmd[i] =
				updater_dev->waitQueue.receiveMsg.cmd[i];
			}
			ret = SNOR_UPDATER_SUCCESS;
		}

		/* clear flag */
		updater_dev->waitQueue.condition = 0;
	}
	return ret;
}

void snor_updater_wake_preset(struct snor_updater_device *updater_dev,
		uint32_t reqeustCMD)
{
	if (updater_dev != NULL) {
		updater_dev->waitQueue.condition = 1;
		updater_dev->waitQueue.reqeustCMD = reqeustCMD;
	}
}

void snor_updater_wake_up(
	struct snor_updater_device *updater_dev,
	const struct tcc_mbox_data *receiveMsg)
{
	if ((updater_dev != NULL) && (receiveMsg != NULL)) {
		dprintk(updater_dev->dev,
			"wait command(%d), receive cmd(%d)\n",
			updater_dev->waitQueue.reqeustCMD, receiveMsg->cmd[0]);

		if (updater_dev->waitQueue.reqeustCMD ==
			receiveMsg->cmd[0]) {

			int32_t i;

			for (i = 0; i < MBOX_CMD_FIFO_SIZE; i++)	{
				updater_dev->waitQueue.receiveMsg.cmd[i]
					= receiveMsg->cmd[i];
			}

			updater_dev->waitQueue.condition = 0;
			wake_up_interruptible(
				&updater_dev->waitQueue.cmdQueue);
		}
	}
}

int32_t send_update_start(struct snor_updater_device *updater_dev)
{
	int32_t ret = SNOR_UPDATER_ERR_COMMON;
	struct tcc_mbox_data sendMsg;
	struct tcc_mbox_data receiveMsg;

	(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));

	sendMsg.cmd[0] = (uint32_t)UPDATE_START;

	snor_updater_wake_preset(updater_dev, (uint32_t)UPDATE_READY);

	ret = snor_updater_mailbox_send(updater_dev, &sendMsg);
	if (ret == SNOR_UPDATER_SUCCESS) {

		ret = snor_updater_wait_event_timeout(updater_dev,
			&receiveMsg,
			(uint32_t)ACK_TIMEOUT);

		if (ret != SNOR_UPDATER_SUCCESS) {
			eprintk(updater_dev->dev, "cmd ack timeout\n");
		}
	}

	if (ret == SNOR_UPDATER_SUCCESS) {
		if (receiveMsg.cmd[0] ==
			(uint32_t)UPDATE_READY) {

			if (receiveMsg.cmd[1] !=
				(uint32_t)SNOR_UPDATE_ACK)	{

				if (receiveMsg.cmd[2]  ==
					(uint32_t)NACK_FIRMWARE_LOAD_FAIL) {

					eprintk(updater_dev->dev,
						"SNOR subfirmware load fail\n");

					ret =
					SNOR_UPDATER_ERR_SNOR_FIRMWARE_FAIL;

				} else if (receiveMsg.cmd[2]  ==
					(uint32_t)NACK_SNOR_INIT_FAIL) {

					eprintk(updater_dev->dev,
						"SNOR init fail\n");
					ret = SNOR_UPDATER_ERR_SNOR_INIT_FAIL;
				} else {

					eprintk(updater_dev->dev,
						"SNOR Nack cmd[2] : (%d)\n",
						receiveMsg.cmd[2]);
					ret = SNOR_UPDATER_ERR_UNKNOWN_CMD;
				}
			}
		} else {

			eprintk(updater_dev->dev,
				"receive unknown cmd:[%d][%d][%d]\n",
				receiveMsg.cmd[0],
				receiveMsg.cmd[1],
				receiveMsg.cmd[2]);
			ret = SNOR_UPDATER_ERR_TIMEOUT;
		}
	}
	return ret;
}

int32_t send_update_done(struct snor_updater_device *updater_dev)
{
	int32_t ret = SNOR_UPDATER_ERR_COMMON;
	struct tcc_mbox_data sendMsg;
	struct tcc_mbox_data receiveMsg;

	(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));

	sendMsg.cmd[0] = (uint32_t)UPDATE_DONE;

	snor_updater_wake_preset(updater_dev, (uint32_t)UPDATE_COMPLETE);

	ret = snor_updater_mailbox_send(updater_dev, &sendMsg);

	if (ret == SNOR_UPDATER_SUCCESS) {

		ret = snor_updater_wait_event_timeout(updater_dev,
			&receiveMsg,
			ACK_TIMEOUT);
		if (ret != SNOR_UPDATER_SUCCESS) {
			eprintk(updater_dev->dev,
				"cmd ack timeout\n");
		} else {
			if (receiveMsg.cmd[0] == (uint32_t)UPDATE_COMPLETE) {
				if (receiveMsg.cmd[1] !=
					(uint32_t)SNOR_UPDATE_ACK) {

					eprintk(updater_dev->dev,
						"receive nack cmd - %d\n",

						receiveMsg.cmd[2]);
					ret = SNOR_UPDATER_ERR_NACK;
				}
			} else {
				eprintk(updater_dev->dev,
					"receive unknown cmd:[%d][%d][%d]\n",
					receiveMsg.cmd[0],
					receiveMsg.cmd[1],
					receiveMsg.cmd[2]);
				ret = SNOR_UPDATER_ERR_TIMEOUT;
			}
		}
	}

	return ret;
}

int32_t send_fw_start(struct snor_updater_device *updater_dev,
	uint32_t fwStartAddress, uint32_t fwPartitionSize,
	uint32_t fwDataSize)
{
	int32_t ret = SNOR_UPDATER_ERR_COMMON;
	struct tcc_mbox_data sendMsg;
	struct tcc_mbox_data receiveMsg;

	(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));

	sendMsg.cmd[0] = (uint32_t)UPDATE_FW_START;
	sendMsg.cmd[1] = fwStartAddress;
	sendMsg.cmd[2] = fwPartitionSize;
	sendMsg.cmd[3] = fwDataSize;

	snor_updater_wake_preset(updater_dev, (uint32_t)UPDATE_FW_READY);

	ret = snor_updater_mailbox_send(updater_dev, &sendMsg);
	if (ret == SNOR_UPDATER_SUCCESS) {
		ret = snor_updater_wait_event_timeout(
			updater_dev,
			&receiveMsg,
			MBOX_ERASE_TIMEOUT);

		if (ret != SNOR_UPDATER_SUCCESS) {
			eprintk(updater_dev->dev,
				"cmd ack timeout\n");
		}
	}

	if (ret == SNOR_UPDATER_SUCCESS) {
		if (receiveMsg.cmd[0] ==
			(uint32_t)UPDATE_FW_READY) {

			if (receiveMsg.cmd[1] !=
				(uint32_t)SNOR_UPDATE_ACK)	{

				eprintk(updater_dev->dev,
					"receive NACK : [%d][%d]\n",
					receiveMsg.cmd[1], receiveMsg.cmd[2]);

				if (receiveMsg.cmd[2]  ==
					(uint32_t)NACK_SNOR_ACCESS_FAIL) {
					ret = SNOR_UPDATER_ERR_SNOR_ACCESS_FAIL;

				} else if (receiveMsg.cmd[2]  ==
					(uint32_t)NACK_CRC_ERROR) {
					ret =
					SNOR_UPDATER_ERR_CRC_ERROR;
				} else	{
					ret =
					SNOR_UPDATER_ERR_UNKNOWN_CMD;
				}
			}
		} else {
			eprintk(updater_dev->dev,
				"receive unknown cmd:[%d][%d][%d]\n",
				receiveMsg.cmd[0],
				receiveMsg.cmd[1],
				receiveMsg.cmd[2]);

			ret = SNOR_UPDATER_ERR_TIMEOUT;
		}
	}

	return ret;
}

int32_t send_fw_send(struct snor_updater_device *updater_dev,
	const struct snor_fw_send_info *fw_send_info, const char *fwData)
{
	int32_t ret;
	struct tcc_mbox_data sendMsg;
	struct tcc_mbox_data receiveMsg;

	if (fw_send_info->fwDataSize > MAX_FW_BUF_SIZE) {
		ret = SNOR_UPDATER_ERR_ARGUMENT;
	} else {
		ret = SNOR_UPDATER_SUCCESS;
	}

	if (ret == SNOR_UPDATER_SUCCESS) {
		(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));

		sendMsg.cmd[0] = (uint32_t)UPDATE_FW_SEND;
		sendMsg.cmd[1] = fw_send_info->fwStartAddress;
		sendMsg.cmd[2] = fw_send_info->currentCount;
		sendMsg.cmd[3] = fw_send_info->totalCount;
		sendMsg.cmd[4] = fw_send_info->fwDataSize;
		sendMsg.cmd[5] = fw_send_info->fwdataCRC;

		(void)memcpy(&sendMsg.data, fwData,
			(ulong)fw_send_info->fwDataSize);

		sendMsg.data_len =
			(fw_send_info->fwDataSize + (uint32_t)3)/(uint32_t)4;

		snor_updater_wake_preset(updater_dev,
			(uint32_t)UPDATE_FW_SEND_ACK);

		ret = snor_updater_mailbox_send(
			updater_dev,
			&sendMsg);
	}

	if (ret == SNOR_UPDATER_SUCCESS) {
		ret = snor_updater_wait_event_timeout(
			updater_dev,
			&receiveMsg,
			(uint32_t)ACK_TIMEOUT);
		if (ret != SNOR_UPDATER_SUCCESS) {
			eprintk(updater_dev->dev,
				"cmd ack timeout\n");
		} else	{
			if (receiveMsg.cmd[0] ==
				(uint32_t)UPDATE_FW_SEND_ACK) {

				if (receiveMsg.cmd[1] !=
					(uint32_t)SNOR_UPDATE_ACK) {
					eprintk(updater_dev->dev,
						"receive NACK:[%d][%d][%d]\n",
						receiveMsg.cmd[0],
						receiveMsg.cmd[1],
						receiveMsg.cmd[2]);
					ret = SNOR_UPDATE_NACK;
				}
			} else {

				eprintk(updater_dev->dev,
					"receive unknown cmd:[%d][%d][%d]\n",
					receiveMsg.cmd[0],
					receiveMsg.cmd[1],
					receiveMsg.cmd[2]);
				ret = SNOR_UPDATER_ERR_TIMEOUT;
			}
		}
	}

	return ret;
}

int32_t send_fw_done(struct snor_updater_device *updater_dev)
{
	int32_t ret = SNOR_UPDATER_ERR_COMMON;
	struct tcc_mbox_data sendMsg;
	struct tcc_mbox_data receiveMsg;

	(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));

	sendMsg.cmd[0] = (uint32_t)UPDATE_FW_DONE;

	snor_updater_wake_preset(updater_dev, (uint32_t)UPDATE_FW_COMPLETE);

	ret = snor_updater_mailbox_send(updater_dev, &sendMsg);
	if (ret == SNOR_UPDATER_SUCCESS) {

		ret = snor_updater_wait_event_timeout(
			updater_dev,
			&receiveMsg,
			(uint32_t)ACK_TIMEOUT);

		if (ret != SNOR_UPDATER_SUCCESS) {
			eprintk(updater_dev->dev,
				"cmd ack timeout\n");
		} else {
			if (receiveMsg.cmd[0] ==
				(uint32_t)UPDATE_FW_COMPLETE)	{

				if (receiveMsg.cmd[1] !=
					(uint32_t)SNOR_UPDATE_ACK) {
					ret = SNOR_UPDATER_ERR_NACK;
				}

			} else {
				eprintk(updater_dev->dev,
					"receive unknown cmd:[%d][%d][%d]\n",
					receiveMsg.cmd[0],
					receiveMsg.cmd[1],
					receiveMsg.cmd[2]);
				ret = SNOR_UPDATER_ERR_TIMEOUT;
			}
		}
	}

	return ret;
}


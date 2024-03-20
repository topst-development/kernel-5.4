// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/of_device.h>

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include <linux/tcc_ipc.h>
#include "tcc_ipc_typedef.h"
#include "tcc_ipc_os.h"
#include "tcc_ipc_mbox.h"

static void ipc_cmd_event_create(struct ipc_device *ipc_dev);
static void ipc_cmd_event_delete(struct ipc_device *ipc_dev);
static void ipc_read_event_create(struct ipc_device *ipc_dev);
static void ipc_read_event_delete(struct ipc_device *ipc_dev);

static void ipc_cmd_event_create(struct ipc_device *ipc_dev)
{
	if (ipc_dev != NULL) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		IPC_INT32 cmd_id;

		for (cmd_id = 0; cmd_id < (IPC_INT32)MAX_CMD_TYPE; cmd_id++) {
			init_waitqueue_head(
				&ipc_handle->ipcWaitQueue[cmd_id].cmdQueue);
			ipc_handle->ipcWaitQueue[cmd_id].seqID = 0xFFFFFFFFU;
			ipc_handle->ipcWaitQueue[cmd_id].condition = 0;
		}
	}
}

static void ipc_cmd_event_delete(struct ipc_device *ipc_dev)
{
	if (ipc_dev != NULL) {
		struct IpcHandler *ipc_handler = &ipc_dev->ipc_handler;
		IPC_INT32 cmd_id;

		for (cmd_id = 0; cmd_id < (IPC_INT32)MAX_CMD_TYPE; cmd_id++) {
			ipc_handler->ipcWaitQueue[cmd_id].seqID = 0xFFFFFFFFU;
			ipc_handler->ipcWaitQueue[cmd_id].condition = 0;
		}
	}
}

IPC_INT32 ipc_cmd_wait_event_timeout(
			struct ipc_device *ipc_dev,
			IpcCmdType cmdType,
			IPC_UINT32 seqID,
			IPC_UINT32 timeOut)
{
	IPC_INT32 ret = -1;

	(void)seqID;

	if (ipc_dev != NULL) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		struct ipc_wait_queue *waitQueue;
		IPC_LONG wait_ret;

		waitQueue = &ipc_handle->ipcWaitQueue[cmdType];

		wait_ret = wait_event_interruptible_timeout(
			waitQueue->cmdQueue,
			waitQueue->condition == (IPC_UINT32)0,
			msecs_to_jiffies(timeOut));

		if (wait_ret >= (IPC_LONG)1) {
			ret = 1;
		} else {
			ret = 0;
		}

		if ((waitQueue->condition == (IPC_UINT32)1)
			|| (ret <= 0)) {
			/* timeout */
			ret = IPC_ERR_TIMEOUT;
		} else {

			if (waitQueue->result == (IPC_UINT32)0) {
				ret = IPC_SUCCESS;
			} else if (waitQueue->result == NACK_BUF_FULL) {
				ret = IPC_ERR_NACK_BUF_FULL;
			} else if (waitQueue->result == NACK_BUF_ERR) {
				ret = IPC_ERR_BUFFER;
			} else {
				ret = IPC_ERR_COMMON;
			}
		}

		/* clear flag */
		ipc_handle->ipcWaitQueue[cmdType].condition = 0;
		ipc_handle->ipcWaitQueue[cmdType].seqID = 0xFFFFFFFFU;
		ipc_handle->ipcWaitQueue[cmdType].result = 0;
	}
	return ret;
}

void ipc_cmd_wake_preset(struct ipc_device *ipc_dev,
							IpcCmdType cmdType,
							IPC_UINT32 seqID)
{
	if (ipc_dev != NULL) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

		ipc_handle->ipcWaitQueue[cmdType].condition = 1;
		ipc_handle->ipcWaitQueue[cmdType].seqID = seqID;
		ipc_handle->ipcWaitQueue[cmdType].result = 0;
	}
}

void ipc_cmd_wake_up(struct ipc_device *ipc_dev,
						IpcCmdType cmdType,
						IPC_UINT32 seqID,
						IPC_UINT32 result)
{
	if (ipc_dev != NULL) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

		if (ipc_handle->ipcWaitQueue[cmdType].seqID == seqID) {

			ipc_handle->ipcWaitQueue[cmdType].condition = 0;
			ipc_handle->ipcWaitQueue[cmdType].result = result;

			wake_up_interruptible(
				&ipc_handle->ipcWaitQueue[cmdType].cmdQueue);
		}
	}
}

void ipc_cmd_all_wake_up(struct ipc_device *ipc_dev)
{
	if (ipc_dev != NULL) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		IPC_INT32 cmd_id;

		for (cmd_id = 0; cmd_id < (IPC_INT32)MAX_CMD_TYPE; cmd_id++) {
			ipc_handle->ipcWaitQueue[cmd_id].condition = 0;
			wake_up_interruptible(
				&ipc_handle->ipcWaitQueue[cmd_id].cmdQueue);
		}
	}
}

static void ipc_read_event_create(struct ipc_device *ipc_dev)
{
	if (ipc_dev != NULL) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

		init_waitqueue_head(&ipc_handle->ipcReadQueue.cmdQueue);
		ipc_handle->ipcReadQueue.condition = 0;
	}
}

static void ipc_read_event_delete(struct ipc_device *ipc_dev)
{
	if (ipc_dev != NULL) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

		ipc_handle->ipcReadQueue.condition = 0;
	}
}

IPC_INT32 ipc_read_wait_event_timeout(
			struct ipc_device *ipc_dev,
			IPC_UINT32 timeOut)
{
	IPC_INT32 ret = IPC_ERR_ARGUMENT;

	if (ipc_dev != NULL) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		IPC_LONG wait_ret;

		ipc_handle->ipcReadQueue.condition = 1;
		wait_ret = wait_event_interruptible_timeout(
			ipc_handle->ipcReadQueue.cmdQueue,
			ipc_handle->ipcReadQueue.condition == (IPC_UINT32)0,
			msecs_to_jiffies(timeOut));

		if (wait_ret == (IPC_LONG)1) {
			ret = 1;
		} else {
			ret = 0;
		}

		if ((ipc_handle->ipcReadQueue.condition == (IPC_UINT32)1)
			|| (ret <= 0)) {
			/* timeout */
			ret = IPC_ERR_TIMEOUT;
		} else {
			ret = IPC_SUCCESS;
		}

		/* clear flag */
		ipc_handle->ipcReadQueue.condition = 0;
	}
	return ret;
}

void ipc_read_wake_up(struct ipc_device *ipc_dev)
{
	if (ipc_dev != NULL) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;

		ipc_handle->ipcReadQueue.condition = 0;
		wake_up_interruptible(&ipc_handle->ipcReadQueue.cmdQueue);
	}
}

void ipc_os_resouce_init(struct ipc_device *ipc_dev)
{
	if (ipc_dev != NULL) {
		ipc_cmd_event_create(ipc_dev);
		ipc_read_event_create(ipc_dev);
	}
}

void ipc_os_resouce_release(struct ipc_device *ipc_dev)
{
	if (ipc_dev != NULL) {
		ipc_cmd_event_delete(ipc_dev);
		ipc_read_event_delete(ipc_dev);
	}
}


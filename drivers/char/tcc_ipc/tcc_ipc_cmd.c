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
#include "tcc_ipc_typedef.h"
#include "tcc_ipc_os.h"
#include "tcc_ipc_mbox.h"
#include "tcc_ipc_cmd.h"

#define MBOX_TIMEOUT		(100)	//ms
#define ACK_TIMEOUT			(500)	//ms

IPC_UINT32 get_sequential_ID(struct ipc_device *ipc_dev)
{
	spin_lock(&ipc_dev->ipc_handler.spinLock);

	if (ipc_dev->ipc_handler.seqID >= 0xFFFFFFFEU) {
		ipc_dev->ipc_handler.seqID = 1;
	} else {
		ipc_dev->ipc_handler.seqID++;
	}

	spin_unlock(&ipc_dev->ipc_handler.spinLock);

	return ipc_dev->ipc_handler.seqID;
}

IPC_INT32 ipc_send_open(struct ipc_device *ipc_dev)
{
	IPC_INT32 ret = IPC_ERR_COMMON;
	struct tcc_mbox_data sendMsg;

	(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));

	sendMsg.cmd[0] = get_sequential_ID(ipc_dev);
	sendMsg.cmd[1] = ((IPC_UINT32)CTL_CMD << (IPC_UINT32)16)
		|((IPC_UINT32)IPC_OPEN);
	sendMsg.cmd[2] = (uint32_t)USE_NACK;

	ipc_dev->ipc_handler.openSeqID = sendMsg.cmd[0];
	ipc_dev->ipc_handler.requestConnectTime = get_jiffies_64();

	ret = ipc_mailbox_send(ipc_dev, &sendMsg);
	return ret;
}

IPC_INT32 ipc_send_close(struct ipc_device *ipc_dev)
{
	IPC_INT32 ret = IPC_ERR_COMMON;
	struct tcc_mbox_data sendMsg;

	(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));

	sendMsg.cmd[0] = get_sequential_ID(ipc_dev);
	sendMsg.cmd[1] = ((IPC_UINT32)CTL_CMD << (IPC_UINT32)16)
		|((IPC_UINT32)IPC_CLOSE);

	ret = ipc_mailbox_send(ipc_dev, &sendMsg);

	return ret;
}

IPC_INT32 ipc_send_write(struct ipc_device *ipc_dev,
							const IPC_UCHAR *ipc_data,
							IPC_UINT32 size)
{
	IPC_INT32 ret = IPC_ERR_COMMON;
	struct tcc_mbox_data sendMsg;

	if ((size <= IPC_TXBUFFER_SIZE) && (ipc_data != NULL)) {
		(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));

		sendMsg.cmd[0] = get_sequential_ID(ipc_dev);
		sendMsg.cmd[1] = ((IPC_UINT32)WRITE_CMD << (IPC_UINT32)16)
			|((IPC_UINT32)IPC_WRITE);
		sendMsg.cmd[2] = size;

		(void)memcpy((void *)&sendMsg.data,
			(const void *)ipc_data,
			(size_t)size);

		sendMsg.data_len = ((size + 3U)/4U);
		ipc_cmd_wake_preset(ipc_dev, WRITE_CMD, sendMsg.cmd[0]);

		ret = ipc_mailbox_send(ipc_dev, &sendMsg);
		if (ret == (IPC_INT32)IPC_SUCCESS) {
			ret = ipc_cmd_wait_event_timeout(ipc_dev,
								WRITE_CMD,
								sendMsg.cmd[0],
								ACK_TIMEOUT);
			if (ret != IPC_SUCCESS) {
				wprintk(ipc_dev->dev, "write fail %d\n", ret);
			}
		}
	} else {
		wprintk(ipc_dev->dev,
			"Data write error : input size(%d)\n",
			size);
		ret = IPC_ERR_ARGUMENT;
	}
	return ret;
}


IPC_INT32 ipc_send_ping(struct ipc_device *ipc_dev)
{
	IPC_INT32 ret = IPC_ERR_COMMON;
	struct tcc_mbox_data sendMsg;

	(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));

	sendMsg.cmd[0] = get_sequential_ID(ipc_dev);
	sendMsg.cmd[1] = ((IPC_UINT32)CTL_CMD << (IPC_UINT32)16U)
		|((IPC_UINT32)IPC_SEND_PING);

	ipc_cmd_wake_preset(ipc_dev, CTL_CMD, sendMsg.cmd[0]);

	ret = ipc_mailbox_send(ipc_dev, &sendMsg);

	if (ret == (IPC_INT32)IPC_SUCCESS) {
		ret = ipc_cmd_wait_event_timeout(ipc_dev,
								CTL_CMD,
								sendMsg.cmd[0],
								ACK_TIMEOUT);
		if (ret != IPC_SUCCESS) {
			wprintk(ipc_dev->dev, "cmd ack timeout\n");
		}
	}
	return ret;
}


IPC_INT32 ipc_send_ack(struct ipc_device *ipc_dev,
							IPC_UINT32 seqID,
							IpcCmdType cmdType,
							IPC_UINT32 sourceCmd)
{
	IPC_INT32 ret = IPC_ERR_COMMON;
	struct tcc_mbox_data sendMsg;

	(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));

	sendMsg.cmd[0] = seqID;
	sendMsg.cmd[1] = ((IPC_UINT32)cmdType << (IPC_UINT32)16U)
		|((IPC_UINT32)IPC_ACK);
	sendMsg.cmd[2] = sourceCmd;

	ret = ipc_mailbox_send(ipc_dev, &sendMsg);

	return ret;
}

IPC_INT32 ipc_send_open_ack(struct ipc_device *ipc_dev,
							IPC_UINT32 seqID,
							IpcCmdType cmdType,
							IPC_UINT32 sourceCmd,
							IPC_UINT32 feature)
{
	IPC_INT32 ret = IPC_ERR_COMMON;
	struct tcc_mbox_data sendMsg;

	(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));

	sendMsg.cmd[0] = seqID;
	sendMsg.cmd[1] = ((IPC_UINT32)cmdType << (IPC_UINT32)16U)
		|((IPC_UINT32)IPC_ACK);
	sendMsg.cmd[2] = sourceCmd;
	sendMsg.cmd[3] = feature;

	ret = ipc_mailbox_send(ipc_dev, &sendMsg);

	return ret;
}

IPC_INT32 ipc_send_nack(struct ipc_device *ipc_dev,
							IPC_UINT32 seqID,
							IpcCmdType cmdType,
							IPC_UINT32 sourceCmd,
							IPC_UINT32 reason)
{
	IPC_INT32 ret = IPC_ERR_COMMON;
	struct tcc_mbox_data sendMsg;

	(void)memset(&sendMsg, 0x00, sizeof(struct tcc_mbox_data));

	sendMsg.cmd[0] = seqID;
	sendMsg.cmd[1] = ((IPC_UINT32)cmdType << (IPC_UINT32)16U)
		|((IPC_UINT32)IPC_NACK);
	sendMsg.cmd[2] = sourceCmd;
	sendMsg.cmd[3] = reason;

	ret = ipc_mailbox_send(ipc_dev, &sendMsg);

	return ret;
}


// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/of_device.h>

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include <linux/tcc_ipc.h>
#include "tcc_ipc_typedef.h"
#include "tcc_ipc_buffer.h"

void ipc_buffer_init(struct IPC_RINGBUF *pBufCtrl,
						IPC_UCHAR *buff,
						IPC_UINT32 size)
{
	pBufCtrl->head = 0;
	pBufCtrl->tail = 0;
	pBufCtrl->maxBufferSize = size;
	pBufCtrl->pBuffer = buff;
}

IPC_INT32 ipc_push_one_byte(struct IPC_RINGBUF  *pBufCtrl, IPC_UCHAR data)
{
	IPC_INT32 ret = IPC_BUFFER_ERROR;
	IPC_UINT32 temp;

	if (pBufCtrl != NULL) {
		temp = pBufCtrl->tail;
		temp++;
		temp %= pBufCtrl->maxBufferSize;

		if (temp == pBufCtrl->head) {
			ret = IPC_BUFFER_FULL;
		} else {
			pBufCtrl->pBuffer[pBufCtrl->tail] = data;
			pBufCtrl->tail = (IPC_UINT32)temp;
			ret = IPC_BUFFER_OK;
		}
	}
	return ret;
}

IPC_INT32 ipc_push_one_byte_overwrite(
			struct IPC_RINGBUF *pBufCtrl,
			IPC_UCHAR data)
{
	IPC_INT32 ret = IPC_BUFFER_ERROR;
	IPC_UINT32 temp;

	if (pBufCtrl != NULL) {
		temp = pBufCtrl->tail;
		temp++;
		temp %= pBufCtrl->maxBufferSize;

		if (temp == pBufCtrl->head) {
			pBufCtrl->head++;
			pBufCtrl->pBuffer[pBufCtrl->tail] = data;
			pBufCtrl->tail = (IPC_UINT32)temp;
			ret = IPC_BUFFER_OK;
		} else {
			pBufCtrl->pBuffer[pBufCtrl->tail] = data;
			pBufCtrl->tail = (IPC_UINT32)temp;
			ret = IPC_BUFFER_OK;
		}
	}
	return ret;
}

IPC_INT32 ipc_pop_one_byte(struct IPC_RINGBUF  *pBufCtrl, IPC_UCHAR *data)
{
	IPC_UINT32 temp;
	IPC_INT32 ret = IPC_BUFFER_ERROR;

	if ((pBufCtrl != NULL) && (data != NULL)) {
		if (pBufCtrl->tail == pBufCtrl->head) {
			ret = IPC_BUFFER_EMPTY;
		} else {
			temp = pBufCtrl->head;
			temp++;
			temp %= pBufCtrl->maxBufferSize;
			*data = pBufCtrl->pBuffer[pBufCtrl->head];
			pBufCtrl->head = temp;
			ret = IPC_BUFFER_OK;
		}
	}
	return ret;
}

void ipc_buffer_flush(struct IPC_RINGBUF  *pBufCtrl)
{
	if (pBufCtrl != NULL) {
		pBufCtrl->head = 0;
		pBufCtrl->tail = 0;
	}
}

void ipc_buffer_flush_byte(struct IPC_RINGBUF  *pBufCtrl, IPC_UINT32 flushSize)
{
	IPC_UINT32  temp;

	if (pBufCtrl != NULL) {
		if (pBufCtrl->tail < pBufCtrl->head) {
			temp = pBufCtrl->head + flushSize;
			if (temp < pBufCtrl->maxBufferSize) {
				pBufCtrl->head = temp;
			} else {
				temp %= pBufCtrl->maxBufferSize;

				if (pBufCtrl->tail <= temp) {
					pBufCtrl->head = pBufCtrl->tail;
				} else {
					pBufCtrl->head = temp;
				}
			}
		} else {
			temp = pBufCtrl->head + flushSize;

			if (pBufCtrl->tail <= temp) {
				pBufCtrl->head = pBufCtrl->tail;
			} else {
				pBufCtrl->head = temp;
			}
		}
	}
}

IPC_UINT32 ipc_buffer_data_available(const struct IPC_RINGBUF  *pBufCtrl)
{
	IPC_UINT32 iRet = 0;
	IPC_UINT32 iRead;
	IPC_UINT32 iWrite;

	if (pBufCtrl != NULL) {

		iRead = pBufCtrl->head;
		iWrite = pBufCtrl->tail;

		if (iWrite >= iRead) {
		// The read pointer is before the write pointer in the bufer.
			iRet = iWrite -	iRead;
		} else {
		// The write pointer is before the read pointer in the buffer.
			iRet = pBufCtrl->maxBufferSize
					- (iRead - iWrite);
		}

	}
	return iRet;
}

IPC_UINT32 ipc_buffer_free_space(const struct IPC_RINGBUF  *pBufCtrl)
{
	IPC_UINT32 iRet = 0;
	IPC_UINT32 iRead;
	IPC_UINT32 iWrite;

	if (pBufCtrl != NULL) {
		iRead = pBufCtrl->head;
		iWrite = pBufCtrl->tail;

		if (iWrite < iRead)	{
		// The write pointer is before the read pointer in the buffer.
			iRet = iRead - iWrite - 1U;
		} else {
		// The read pointer is before the write pointer in the bufer.
			iRet = pBufCtrl->maxBufferSize
					- (iWrite - iRead) - 1U;
		}
	}

	return iRet;
}

void ipc_buffer_set_head(struct IPC_RINGBUF  *pBufCtrl, IPC_INT32 head)
{
	if ((pBufCtrl != NULL) && (head >= 0)) {
		pBufCtrl->head = (IPC_UINT32)head;
	}
}

void ipc_buffer_set_tail(struct IPC_RINGBUF  *pBufCtrl, IPC_INT32 tail)
{
	if ((pBufCtrl != NULL) && (tail >= 0)) {
		pBufCtrl->tail = (IPC_UINT32)tail;
	}
}

IPC_INT32 ipc_push_buffer(struct IPC_RINGBUF  *pBufCtrl,
							const IPC_UCHAR *buffer,
							IPC_UINT32 size)
{
	IPC_INT32 ret = IPC_BUFFER_ERROR;
	IPC_UINT32 freeSpace;

	if ((pBufCtrl != NULL) && (buffer != NULL)) {
		freeSpace = ipc_buffer_free_space(pBufCtrl);
		if (freeSpace >= size) {
			IPC_UINT32 continuousSize;

			if (pBufCtrl->maxBufferSize >
				pBufCtrl->tail) {

				continuousSize =
					((IPC_UINT32)pBufCtrl->maxBufferSize
					- (IPC_UINT32)pBufCtrl->tail);

				if (continuousSize > size) {

					(void)memcpy((void *)
					&pBufCtrl->pBuffer[pBufCtrl->tail],
				     (const void *)buffer,
				     (size_t) size);

					pBufCtrl->tail += size;
					ret = (IPC_INT32)size;

				} else {
					IPC_UINT32 remainSize;

					(void)memcpy((void *)
				     &pBufCtrl->pBuffer[pBufCtrl->tail],
				     (const void *)buffer,
				     (size_t)continuousSize);

					remainSize = size - continuousSize;

					(void)memcpy((void *)
				     &pBufCtrl->pBuffer[0],
				     (const void *)
				     &buffer[continuousSize],
				     (size_t) remainSize);

					pBufCtrl->tail = remainSize;

					ret = (IPC_INT32)size;
				}
			}
		} else {
			ret = IPC_BUFFER_FULL;
		}
	}

	return ret;
}

IPC_INT32 ipc_push_buffer_overwrite(struct IPC_RINGBUF  *pBufCtrl,
					const IPC_UCHAR *buffer,
					IPC_UINT32 size)
{
	IPC_INT32 ret = IPC_BUFFER_ERROR;
	IPC_UINT32 continuousSize;

	if ((pBufCtrl != NULL) && (buffer != NULL)) {
		if (pBufCtrl->maxBufferSize >= size) {
			continuousSize =
				(pBufCtrl->maxBufferSize - pBufCtrl->tail);

			if (continuousSize > size) {

				(void)memcpy((void *)
				&pBufCtrl->pBuffer[pBufCtrl->tail],
				(const void *)buffer,
				(size_t) size);

				pBufCtrl->tail += size;
				pBufCtrl->head = ((pBufCtrl->tail + 1U)
						   % pBufCtrl->maxBufferSize);

				ret = (IPC_INT32) size;

			} else {
				IPC_UINT32 remainSize;

				(void)memcpy((void *)
			       &pBufCtrl->pBuffer[pBufCtrl->tail],
			       (const void *)buffer,
			       (size_t) continuousSize);

				remainSize = size - continuousSize;

				(void)memcpy((void *)
				&pBufCtrl->pBuffer[0],
		       (const void *)&buffer[continuousSize],
		       (size_t) remainSize);

				pBufCtrl->tail = remainSize;
				pBufCtrl->head = ((pBufCtrl->tail + 1U)
						   % pBufCtrl->maxBufferSize);

				ret = (IPC_INT32)size;
			}
		}
	}

	return ret;
}

IPC_INT32 ipc_pop_buffer(struct IPC_RINGBUF  *pBufCtrl,
							IPC_CHAR __user *buffer,
							IPC_UINT32 size)
{
	IPC_INT32 ret = IPC_BUFFER_ERROR;

	if (pBufCtrl->tail == pBufCtrl->head) {
		ret = IPC_BUFFER_EMPTY;
	} else {

		IPC_UINT32 dataSize;

		dataSize = ipc_buffer_data_available(pBufCtrl);

		if (dataSize >= size) {
			IPC_UINT32 continuousSize;

			continuousSize =
			    pBufCtrl->maxBufferSize - pBufCtrl->head;
			if (continuousSize > size) {
				if (copy_to_user((void *)buffer,
				 (const void *)
				 &pBufCtrl->pBuffer[pBufCtrl->head],
				 (IPC_ULONG) size) == (IPC_ULONG)0) {

					pBufCtrl->head += size;
					ret = IPC_BUFFER_OK;
				} else {
					(void)
				    pr_err
				    ("[ERROR][%s]%s:W error.cont-%d,size-%d\n",
				     (const IPC_CHAR *)LOG_TAG,
				     __func__, continuousSize, size);
				}
			} else {
				IPC_UINT32 remainSize;

				if (copy_to_user((void *)buffer,
					 (const void *)
					 &pBufCtrl->pBuffer[pBufCtrl->head],
					 (IPC_ULONG)continuousSize)
				    == (IPC_ULONG)0) {

					remainSize = (size - continuousSize);

					if (copy_to_user((void *)
					 &buffer[continuousSize],
					 (const void *)&pBufCtrl->pBuffer[0],
					 (IPC_ULONG)remainSize)
					    == (IPC_ULONG)0) {

						pBufCtrl->head = remainSize;
						ret = IPC_BUFFER_OK;
					}
				}
				if (ret != IPC_BUFFER_OK) {

					(void)
				    pr_err
				    ("[ERROR][%s]%s:W error.cont-%d, size-%d\n",
				     (const IPC_UCHAR *)LOG_TAG,
				     __func__, continuousSize, size);
				}
			}
		}
	}
	return ret;
}



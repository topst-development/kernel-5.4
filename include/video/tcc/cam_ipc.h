/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 ******************************************************************************/

#ifndef VIOC_MGR_H
#define VIOC_MGR_H

#define CAM_IPC_SEND		(0x1)
#define CAM_IPC_ACK			(0x10)
#define CAM_IPC_STATUS		(0x11)

/* control commands */
enum {
	CAM_IPC_CMD_NULL = 0,
	CAM_IPC_CMD_OVP,
	CAM_IPC_CMD_POS,
	CAM_IPC_CMD_RESET,
	CAM_IPC_CMD_READY,
	CAM_IPC_CMD_STATUS,
	CAM_IPC_CMD_MAX,
};

/* driver status */
enum {
	CAM_IPC_STS_NULL = 0,
	CAM_IPC_STS_INIT,
	CAM_IPC_STS_READY,
	CAM_IPC_MAX_STS,
};

enum {
	CAM_IPC_PROBE_INIT_OBJ = 0,
	CAM_IPC_PROBE_PARSE_DT,
	CAM_IPC_PROBE_INIT_CDEV,
	CAM_IPC_PROBE_INIT_CLASS,
	CAM_IPC_PROBE_INIT_DEVICE,
	CAM_IPC_PROBE_INIT_TX_RX,
	CAM_IPC_PROBE_MAX
};

#define CAM_IPC_MAGIC ('I')
#define IOCTL_CAM_IPC_SET_OVP				(_IO(CAM_IPC_MAGIC, 1))
#define IOCTL_CAM_IPC_SET_POS				(_IO(CAM_IPC_MAGIC, 2))
#define IOCTL_CAM_IPC_SET_RESET				(_IO(CAM_IPC_MAGIC, 3))
#define IOCTL_CAM_IPC_SET_READY				(_IO(CAM_IPC_MAGIC, 4))

#define IOCTL_CAM_IPC_SET_OVP_KERNEL		(0x100)
#define IOCTL_CAM_IPC_SET_POS_KERNEL		(0x101)
#define IOCTL_CAM_IPC_SET_RESET_KERNEL		(0x102)
#define IOCTL_CAM_IPC_SET_READY_KERNEL		(0x103)
#endif

/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __TCC_CP_DEV_H__
#define __TCC_CP_DEV_H__

#define CP_DEV_MAJOR_NUM                237
#define CP_DEV_MINOR_NUM                1

#define IOCTL_CP_CTRL_INIT              _IO(CP_DEV_MAJOR_NUM, 100)
#define IOCTL_CP_CTRL_PWR               _IO(CP_DEV_MAJOR_NUM, 101)
#define IOCTL_CP_CTRL_RESET             _IO(CP_DEV_MAJOR_NUM, 102)
#define IOCTL_CP_CTRL_ALL               _IO(CP_DEV_MAJOR_NUM, 103)
#define IOCTL_CP_GET_VERSION            _IO(CP_DEV_MAJOR_NUM, 104)
#define IOCTL_CP_GET_CHANNEL            _IO(CP_DEV_MAJOR_NUM, 105)
#define IOCTL_CP_SET_STATE              _IO(CP_DEV_MAJOR_NUM, 106)
#define IOCTL_CP_GET_STATE              _IO(CP_DEV_MAJOR_NUM, 107)
#define IOCTL_CP_GET_WITH_IAP2          _IO(CP_DEV_MAJOR_NUM, 108)

#endif /* __TCC_CP_DEV_H__ */

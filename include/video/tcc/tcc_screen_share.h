/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * linux/include/video/tcc/tcc_screen_share.h
 * Description: Telechips Screen Share Driver
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
#ifndef TCC_SCRSHARE_H
#define TCC_SCRSHARE_H

#define TCC_SCRSHARE_SEND		0x1U
#define TCC_SCRSHARE_ACK		0x10U

struct tcc_scrshare_srcinfo {
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
};

struct tcc_scrshare_dstinfo {
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	unsigned int img_num;
};

struct tcc_scrshare_info {
	struct tcc_scrshare_srcinfo *srcinfo;
	struct tcc_scrshare_dstinfo *dstinfo;

	unsigned int frm_w;
	unsigned int frm_h;
	unsigned int fmt;
	unsigned int src_addr;
	unsigned int share_enable;
	unsigned int rgb_swap;
};

/* control commands */
enum {
	SCRSHARE_CMD_NULL = 0,
	SCRSHARE_CMD_GET_DSTINFO,
	SCRSHARE_CMD_SET_SRCINFO,
	SCRSHARE_CMD_ON,
	SCRSHARE_CMD_OFF,
	SCRSHARE_CMD_READY,
	SCRSHARE_CMD_MAX,
};

/* driver status */
enum {
	SCRSHARE_STS_NULL = 0,
	SCRSHARE_STS_INIT,
	SCRSHARE_STS_READY,
	SCRSHARE_MAX_STS,
};

#define TCC_SCRSHARE_MAGIC 'S'
#define IOCTL_TCC_SCRSHARE_SET_DSTINFO		_IO(TCC_SCRSHARE_MAGIC, 1U)
#define IOCTL_TCC_SCRSHARE_GET_DSTINFO		_IO(TCC_SCRSHARE_MAGIC, 2U)
#define IOCTL_TCC_SCRSHARE_SET_SRCINFO		_IO(TCC_SCRSHARE_MAGIC, 3U)
#define IOCTL_TCC_SCRSHARE_ON			_IO(TCC_SCRSHARE_MAGIC, 4)
#define IOCTL_TCC_SCRSHARE_OFF			_IO(TCC_SCRSHARE_MAGIC, 5)

#define IOCTL_TCC_SCRSHARE_SET_DSTINFO_KERNEL		0x100U
#define IOCTL_TCC_SCRSHARE_GET_DSTINFO_KERNEL		0x101U
#define IOCTL_TCC_SCRSHARE_SET_SRCINFO_KERNEL		0x102U
#define IOCTL_TCC_SCRSHARE_ON_KERNEL			0x103U
#define IOCTL_TCC_SCRSHARE_OFF_KERNEL			0x104U
#endif

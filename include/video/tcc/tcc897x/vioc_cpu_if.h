/*
 * Copyright (C) Telechips, Inc.
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
#ifndef __VIOC_CPU_IF_H__
#define	__VIOC_CPU_IF_H__

/************************************************************************
 *   CPU Interface (Base Addr = 0x12100000)
 ************************************************************************
 */
struct VIOC_CPUIF_CTRL {
	unsigned int RD_HLD :  3;
	unsigned int RD_PW  :  9;
	unsigned int RD_STP :  3;
	unsigned int RD_B16 :  1;
	unsigned int WR_HLD :  3;
	unsigned int WR_PW  :  9;
	unsigned int WR_STP :  3;
	unsigned int WR_B16 :  1;
};

union VIOC_CPUIF_CTRL_u {
	unsigned	long			nREG;
	struct VIOC_CPUIF_CTRL		bREG;
};

struct VIOC_CPUIF_BSWAP {
	unsigned int XA00_RDBS	: 8;
	unsigned int XA00_WRBS	: 8;
	unsigned int XA01_RDBS	: 8;
	unsigned int XA01_WRBS	: 8;
	unsigned int XA10_RDBS	: 8;
	unsigned int XA10_WRBS	: 8;
	unsigned int XA11_RDBS	: 8;
	unsigned int XA11_WRBS	: 8;
};

union VIOC_CPUIF_BSWAP_u {
	unsigned	long			nREG[2];
	struct VIOC_CPUIF_BSWAP	bREG;
};

struct VIOC_CPUIF_TYPE {
	unsigned int MODE68   : 1;
	unsigned int RESERVED :31;
};

union VIOC_CPUIF_TYPE_u {
	unsigned	long			nREG;
	struct VIOC_CPUIF_TYPE		bREG;
};

union VIOC_CPUIF_AREA_u {
	unsigned char		b08[16];
	unsigned short	b16[8];
	unsigned	int		b32[4];
};

struct VIOC_CPUIF_CHANNEL {
	volatile union VIOC_CPUIF_CTRL_u	uCS0_CMD0_CTRL; // 0x00
	volatile union VIOC_CPUIF_CTRL_u	uCS0_DAT0_CTRL; // 0x04
	volatile union VIOC_CPUIF_CTRL_u	uCS0_CMD1_CTRL; // 0x08
	volatile union VIOC_CPUIF_CTRL_u	uCS0_DAT1_CTRL; // 0x0C
	volatile union VIOC_CPUIF_CTRL_u	uCS1_CMD0_CTRL; // 0x10
	volatile union VIOC_CPUIF_CTRL_u	uCS1_DAT0_CTRL; // 0x14
	volatile union VIOC_CPUIF_CTRL_u	uCS1_CMD1_CTRL; // 0x18
	volatile union VIOC_CPUIF_CTRL_u	uCS1_DAT1_CTRL; // 0x1C
	volatile union VIOC_CPUIF_BSWAP_u	uCS0_BSWAP;     // 0x20~0x24
	volatile union VIOC_CPUIF_BSWAP_u	uCS1_BSWAP;     // 0x28~0x2C

	unsigned int reserved0[2];                  // 0x30~0x34

	volatile union VIOC_CPUIF_TYPE_u	uTYPE;          // 0x38

	unsigned int reserved1;                     // 0x3C

	volatile union VIOC_CPUIF_AREA_u	uCS0_CMD0;      // 0x40~0x4C
	volatile union VIOC_CPUIF_AREA_u	uCS0_CMD1;      // 0x50~0x5C
	volatile union VIOC_CPUIF_AREA_u	uCS0_DAT0;      // 0x60~0x6C
	volatile union VIOC_CPUIF_AREA_u	uCS0_DAT1;      // 0x70~0x7C
	volatile union VIOC_CPUIF_AREA_u	uCS1_CMD0;      // 0x80~0x8C
	volatile union VIOC_CPUIF_AREA_u	uCS1_CMD1;      // 0x90~0x9C
	volatile union VIOC_CPUIF_AREA_u	uCS1_DAT0;      // 0xA0~0xAC
	volatile union VIOC_CPUIF_AREA_u	uCS1_DAT1;      // 0xB0~0xBC
	unsigned int reserved2[16];
};

#endif

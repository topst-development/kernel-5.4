/* SPDX-License-Identifier: GPL-2.0+ */
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
#ifndef TCC_SCALER_H
#define TCC_SCALER_H

#include "tcc_video_private.h"
#include "tcc_sar.h"

#define TCC_SCALER_IOCTRL          0x1
#define TCC_SCALER_IOCTRL_KERENL   0x10
#define TCC_SCALER_VIOC_PLUGIN     0x100
#define TCC_SCALER_VIOC_PLUGOUT    0x101
#define TCC_SCALER_VIOC_ALPHA_INIT 0x102
#define TCC_SCALER_VIOC_ALPHA_RUN  0x103
#define TCC_SCALER_VIOC_DATA_COPY  0x200

enum Scaler_ResponseType {
	SCALER_POLLING,
	SCALER_INTERRUPT,
	SCALER_NOWAIT
};

enum SCALER_DATA_FM {
	SCALER_RGB454 = 8,
	SCALER_RGB444 = 9,
	SCALER_RGB565 = 10,
	SCALER_RGB555 = 11,
	SCALER_ARGB8888 = 12,
	SCALER_ARGB6666_4 = 13,
	SCALER_RGB888 = 14,
	SCALER_ARGB6666_3 = 15,
	SCLAER_COMPRESS_DATA = 16,
	SCALER_YUV420_sp = 24,
	SCALER_YUV422_sp = 25,
	SCALER_YUV422_sq0 = 26,
	SCALER_YUV422_sq1 = 27,
	SCALER_YUV420_inter = 28,
	SCALER_YUV420_inter_NV21 = 29, // TODO: android code, Linux platform N/A
	SCALER_YUV422_inter = 30,
	SCALER_FMT_MAX
};

struct SCALER_LUT {
	unsigned int use_lut; //enable /disable
	unsigned int use_lut_number; // lut number.
};

struct SCALER_TYPE {
	enum Scaler_ResponseType responsetype; //scaler response type
	unsigned int src_Yaddr;		// source address
	unsigned int src_Uaddr;		// source address
	unsigned int src_Vaddr;		// source address
	unsigned int src_fmt;		// source image format
	unsigned int src_rgb_swap;
	unsigned int src_bit_depth;
	unsigned int src_ImgWidth;	// source image width
	unsigned int src_ImgHeight;	// source image height
	unsigned int src_winLeft;
	unsigned int src_winTop;
	unsigned int src_winRight;
	unsigned int src_winBottom;
	unsigned int dest_Yaddr;	// destination image address
	unsigned int dest_Uaddr;	// destination image address
	unsigned int dest_Vaddr;	// destination image address
	unsigned int dest_fmt;		// destination image format
	unsigned int dst_rgb_swap;
	unsigned int dest_bit_depth;
	unsigned int dest_ImgWidth;	// destination image width
	unsigned int dest_ImgHeight; // destination image height
	unsigned int dest_winLeft;
	unsigned int dest_winTop;
	unsigned int dest_winRight;
	unsigned int dest_winBottom;
	unsigned int viqe_onthefly; // 0:m2m, 0x1:R otf, 0x2:W otf, 0x3:R/W otf
	unsigned int interlaced;

	unsigned char plugin_path;
	unsigned char divide_path; // 0:Normal, 1:SideBySide, 2:Top&Bottom
	hevc_MapConv_info_t mapConv_info;
	vp9_compressed_info_t dtrcConv_info;

	unsigned int color_base; // 0: YUV, 1: RGB
	unsigned char wdma_aligned_offset; // 0:Normal, 1:Aligned WDMA offset
	struct SCALER_LUT lut; // look up table

	struct sar_cmd sar;
};

struct SCALER_PLUGIN_Type {
	unsigned short scaler_no;   // scaler number : 0, 1, 2
	unsigned short bypass_mode; // scaler bypass option
	unsigned short plugin_path; // scaler plug-in path

	unsigned short src_width;   // scaler input data width
	unsigned short src_height;  // scaler input data height
	unsigned short src_win_left;
	unsigned short src_win_top;
	unsigned short src_win_right;
	unsigned short src_win_bottom;

	unsigned short dst_width;  // scaler output data width
	unsigned short dst_height; // scaler output data height
	unsigned short dst_win_left;
	unsigned short dst_win_top;
	unsigned short dst_win_right;
	unsigned short dst_win_bottom;
};

struct SCALER_ALPHABLENDING_TYPE {
	unsigned char rsp_type;
	unsigned char region;

	unsigned char src0_fmt;
	unsigned char src0_layer;
	unsigned short src0_acon0;
	unsigned short src0_acon1;
	unsigned short src0_ccon0;
	unsigned short src0_ccon1;
	unsigned short src0_rop_mode;
	unsigned short src0_asel;
	unsigned short src0_alpha0;
	unsigned short src0_alpha1;
	unsigned int src0_Yaddr;
	unsigned int src0_Uaddr;
	unsigned int src0_Vaddr;
	unsigned short src0_width;
	unsigned short src0_height;
	unsigned short src0_winLeft;
	unsigned short src0_winTop;
	unsigned short src0_winRight;
	unsigned short src0_winBottom;

	unsigned char src1_fmt;
	unsigned char src1_layer;
	unsigned short src1_acon0;
	unsigned short src1_acon1;
	unsigned short src1_ccon0;
	unsigned short src1_ccon1;
	unsigned short src1_rop_mode;
	unsigned short src1_asel;
	unsigned short src1_alpha0;
	unsigned short src1_alpha1;
	unsigned int src1_Yaddr;
	unsigned int src1_Uaddr;
	unsigned int src1_Vaddr;
	unsigned short src1_width;
	unsigned short src1_height;
	unsigned short src1_winLeft;
	unsigned short src1_winTop;
	unsigned short src1_winRight;
	unsigned short src1_winBottom;

	unsigned char dst_fmt;
	unsigned int dst_Yaddr;
	unsigned int dst_Uaddr;
	unsigned int dst_Vaddr;
	unsigned short dst_width;
	unsigned short dst_height;
	unsigned short dst_winLeft;
	unsigned short dst_winTop;
	unsigned short dst_winRight;
	unsigned short dst_winBottom;
};

struct SCALER_DATA_COPY_TYPE {
	unsigned int rsp_type;   // wmix response type

	unsigned int src_y_addr; // source y address
	unsigned int src_u_addr; // source u address
	unsigned int src_v_addr; // source v address
	unsigned int src_fmt;    // source image format

	unsigned int dst_y_addr; // destination image address
	unsigned int dst_u_addr; // destination image address
	unsigned int dst_v_addr; // destination image address
	unsigned int dst_fmt;    // destination image format

	unsigned int img_width;  // source image width
	unsigned int img_height; // source image height
};

#endif
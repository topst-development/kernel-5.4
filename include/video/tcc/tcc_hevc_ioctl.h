// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 * FileName   : tcc_hevc_ioctl.h
 * Description: TCC VPU h/w block
 */

#include "tcc_video_common.h"

#ifndef _HEVC_IOCTL_H_
#define _HEVC_IOCTL_H_

#define HEVC_MAX (VPU_ENC)

typedef struct {
	int result;
	codec_handle_t gsHevcDecHandle;
	hevc_dec_init_t gsHevcDecInit;
} HEVC_INIT_t;

typedef struct {
	int result;
	unsigned int stream_size;
	hevc_dec_initial_info_t gsHevcDecInitialInfo;
} HEVC_SEQ_HEADER_t;

typedef struct {
	int result;
	hevc_dec_buffer_t gsHevcDecBuffer;
} HEVC_SET_BUFFER_t;

typedef struct {
	int result;
	hevc_dec_input_t gsHevcDecInput;
	hevc_dec_output_t gsHevcDecOutput;
	hevc_dec_initial_info_t gsHevcDecInitialInfo;
} HEVC_DECODE_t;

typedef struct {
	int result;
	hevc_dec_ring_buffer_status_out_t gsHevcDecRingStatus;
} HEVC_RINGBUF_GETINFO_t;

typedef struct {
	int result;
	hevc_dec_init_t gsHevcDecInit;
	hevc_dec_ring_buffer_setting_in_t gsHevcDecRingFeed;
} HEVC_RINGBUF_SETBUF_t;

typedef struct {
	int result;
	int iCopiedSize;
	int iFlushBuf;
} HEVC_RINGBUF_SETBUF_PTRONLY_t;

typedef struct {
	int result;
	char *pszVersion;
	char *pszBuildData;
} HEVC_GET_VERSION_t;


//------------------------------------------------------------------------------
// binaray structure bearer for 32/64 userspace
//  and 64/32 kernel space environment
//------------------------------------------------------------------------------
typedef struct {
	int result;
	codec_handle_t gsHevcDecHandle;
	hevc_dec_init_64bit_t gsHevcDecInitEx;
} HEVC_INIT_BEARER_t;

typedef struct {
	int result;
	hevc_dec_init_64bit_t gsHevcDecInitEx;
	hevc_dec_ring_buffer_setting_in_t gsHevcDecRingFeed;
} HEVC_RINGBUF_SETBUF_BEARER_t;

typedef struct {
	int result;
	unsigned long long cVersionAddr;
	unsigned long long cBuildDataAddr;
} HEVC_GET_VERSION_BEARER_t;


#endif // _HEVC_IOCTL_H_
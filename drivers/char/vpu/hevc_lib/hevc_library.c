// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/moduleparam.h>
#include <linux/module.h>

typedef long codec_handle_t; //!< handle

int tcc_hevc_dec(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2)
{
	return 1;
}
EXPORT_SYMBOL(tcc_hevc_dec);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC hevc library");
MODULE_LICENSE("Proprietary");

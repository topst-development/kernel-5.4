/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef PLAT_CHIPINFO_H
#define PLAT_CHIPINFO_H

#include <linux/types.h>

#define HAVE_PLAT_GET_CHIP_REV
u32 plat_get_chip_rev(void);

#define HAVE_PLAT_GET_CHIP_NAME
u32 plat_get_chip_name(void);

#endif

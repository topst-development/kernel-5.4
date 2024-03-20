/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef MACH_TCC897X_COMMON_H
#define MACH_TCC897X_COMMON_H

extern struct smp_operations tcc_smp_ops;
void __init tcc_map_common_io(void);
void __init tcc_mem_reserve(void);

#endif

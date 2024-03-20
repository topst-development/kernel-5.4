// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <asm/setup.h>
#include <linux/input.h>
#include <linux/debugfs.h>

#include "dwmac-tcc-v2.h"

struct tcc_dwmac *_gmac_debugfs;

static int dwmac_tcc_debugfs_enable_get(void *data, u64 *val)
{
	return 0;
}

static int dwmac_tcc_debugfs_enable_set(void *data, u64 val)
{
	tcc_dwmac_tuning_timing(_gmac_debugfs);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(
	dwmac_tcc_debugfs_enable_fops,
	dwmac_tcc_debugfs_enable_get,
	dwmac_tcc_debugfs_enable_set, "%llu\n");

#define TCC_DEBUGFS_SET_FOPS(name) \
static int name##_get(void *data, u64 *val) \
{\
	*val = _gmac_debugfs->name;\
	return 0;\
} \
static int name##_set(void *data, u64 val)\
{\
	_gmac_debugfs->name = (uint32_t)val;\
	return 0;\
} \
DEFINE_SIMPLE_ATTRIBUTE(\
	name##_fops, \
	name##_get, \
	name##_set, "%llu\n")

TCC_DEBUGFS_SET_FOPS(txclk_i_dly);
TCC_DEBUGFS_SET_FOPS(txclk_i_inv);
TCC_DEBUGFS_SET_FOPS(txclk_o_dly);
TCC_DEBUGFS_SET_FOPS(txclk_o_inv);
TCC_DEBUGFS_SET_FOPS(txen_dly);
TCC_DEBUGFS_SET_FOPS(txer_dly);
TCC_DEBUGFS_SET_FOPS(txd0_dly);
TCC_DEBUGFS_SET_FOPS(txd1_dly);
TCC_DEBUGFS_SET_FOPS(txd2_dly);
TCC_DEBUGFS_SET_FOPS(txd3_dly);
TCC_DEBUGFS_SET_FOPS(txd4_dly);
TCC_DEBUGFS_SET_FOPS(txd5_dly);
TCC_DEBUGFS_SET_FOPS(txd6_dly);
TCC_DEBUGFS_SET_FOPS(txd7_dly);
TCC_DEBUGFS_SET_FOPS(rxclk_i_dly);
TCC_DEBUGFS_SET_FOPS(rxclk_i_inv);
TCC_DEBUGFS_SET_FOPS(rxdv_dly);
TCC_DEBUGFS_SET_FOPS(rxer_dly);
TCC_DEBUGFS_SET_FOPS(rxd0_dly);
TCC_DEBUGFS_SET_FOPS(rxd1_dly);
TCC_DEBUGFS_SET_FOPS(rxd2_dly);
TCC_DEBUGFS_SET_FOPS(rxd3_dly);
TCC_DEBUGFS_SET_FOPS(rxd4_dly);
TCC_DEBUGFS_SET_FOPS(rxd5_dly);
TCC_DEBUGFS_SET_FOPS(rxd6_dly);
TCC_DEBUGFS_SET_FOPS(rxd7_dly);
TCC_DEBUGFS_SET_FOPS(crs_dly);
TCC_DEBUGFS_SET_FOPS(col_dly);

#define TCC_DEBUG_FS_CREATE_FILE(name) \
debugfs_create_file(#name, \
	0600, \
	dbgfs_dir, \
	NULL, \
	&(name##_fops) \
)

void tcc_dwmac_debugfs_init(struct tcc_dwmac *gmac)
{
	static struct dentry *dbgfs_dir;

	_gmac_debugfs = gmac;
	dbgfs_dir = debugfs_create_dir("dwmac_tcc_debugfs", NULL);
	gmac->dbgfs_dir = dbgfs_dir;
	if (dbgfs_dir != NULL) {
		TCC_DEBUG_FS_CREATE_FILE(txclk_i_dly);
		TCC_DEBUG_FS_CREATE_FILE(txclk_i_inv);
		TCC_DEBUG_FS_CREATE_FILE(txclk_o_dly);
		TCC_DEBUG_FS_CREATE_FILE(txclk_o_inv);
		TCC_DEBUG_FS_CREATE_FILE(txen_dly);
		TCC_DEBUG_FS_CREATE_FILE(txer_dly);
		TCC_DEBUG_FS_CREATE_FILE(txd0_dly);
		TCC_DEBUG_FS_CREATE_FILE(txd1_dly);
		TCC_DEBUG_FS_CREATE_FILE(txd2_dly);
		TCC_DEBUG_FS_CREATE_FILE(txd3_dly);
		TCC_DEBUG_FS_CREATE_FILE(txd4_dly);
		TCC_DEBUG_FS_CREATE_FILE(txd5_dly);
		TCC_DEBUG_FS_CREATE_FILE(txd6_dly);
		TCC_DEBUG_FS_CREATE_FILE(txd7_dly);
		TCC_DEBUG_FS_CREATE_FILE(rxclk_i_dly);
		TCC_DEBUG_FS_CREATE_FILE(rxclk_i_inv);
		TCC_DEBUG_FS_CREATE_FILE(rxdv_dly);
		TCC_DEBUG_FS_CREATE_FILE(rxer_dly);
		TCC_DEBUG_FS_CREATE_FILE(rxd0_dly);
		TCC_DEBUG_FS_CREATE_FILE(rxd1_dly);
		TCC_DEBUG_FS_CREATE_FILE(rxd2_dly);
		TCC_DEBUG_FS_CREATE_FILE(rxd3_dly);
		TCC_DEBUG_FS_CREATE_FILE(rxd4_dly);
		TCC_DEBUG_FS_CREATE_FILE(rxd5_dly);
		TCC_DEBUG_FS_CREATE_FILE(rxd6_dly);
		TCC_DEBUG_FS_CREATE_FILE(rxd7_dly);
		TCC_DEBUG_FS_CREATE_FILE(crs_dly);
		TCC_DEBUG_FS_CREATE_FILE(col_dly);

		debugfs_create_file(
			"enable", 0600, dbgfs_dir, NULL,
			&dwmac_tcc_debugfs_enable_fops);
	} else {
		pr_err("[ERR][GMAC] debugfs for ethernet phy tuning init fail..\n");
	}
}

void tcc_dwmac_debugfs_exit(struct tcc_dwmac *gmac)
{
	if (gmac->dbgfs_dir != NULL) {
		debugfs_remove_recursive(gmac->dbgfs_dir);
	}
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * linux/drivers/char/tcc_lut.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2008-2009 Telechips
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/limits.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_lut.h>
#include <video/tcc/tcc_lut_ioctl.h>
#include "tcc_lut.h"

#define LUT_VERSION "v1.8"

#define TCC_LUT_DEBUG	0

#define DEFAULT_DEV_MAX		2	/* 0: DEV0, 1: DEV1 and 2: DEV2 */

/* 3: VIOC_LUT0, 4: VIOC_LUT1, 5: VIOC_LUT2 and 6: VIOC_LUT3 */
#define DEFAULT_VIOC_MAX	6

struct lut_drv_type {
	unsigned int dev_opened;
	unsigned int dev_max;
	unsigned int vioc_max;
	struct miscdevice *misc;

	/* proc fs */
	struct proc_dir_entry *proc_dir;
	struct proc_dir_entry *proc_debug;
#if defined(CONFIG_TCC_LUT_DEBUG_DUMP)
	unsigned int Gamma_vioc_lut_3[1024];
	unsigned int Gamma_vioc_lut_4[1024];
	unsigned int Gamma_vioc_lut_5[1024];
	unsigned int Gamma_vioc_lut_6[1024];
#endif
};

enum {
	MAPPING_INDEX_RDMA00 = 0,	/*0x00   VRDMA00 WMIX0 path */
	MAPPING_INDEX_RDMA01 ,	/*0x00   VRDMA00 WMIX0 path */
	MAPPING_INDEX_RDMA02,	/*0x02   VRDMA02 WMIX0 path */
	MAPPING_INDEX_RDMA03,	/*0x03   VRDMA03 WMIX0 path */
	MAPPING_INDEX_RDMA04,	/*0x04   GRDMA04 WMIX1 path */
	MAPPING_INDEX_RDMA05,	/*0x05   GRDMA05 WMIX1 path */
	MAPPING_INDEX_RDMA06,	/*0x06   VRDMA06 WMIX1 path */
	MAPPING_INDEX_RDMA07,	/*0x07   VRDMA07 WMIX1 path */
	MAPPING_INDEX_RDMA08,	/*0x08   GRDMA08 WMIX2 path */
	MAPPING_INDEX_RDMA09,	/*0x09   GRDMA09 WMIX2 path */
	MAPPING_INDEX_RDMA10 = 10,	/*0x0A   VRDMA10 WMIX2 path */
	MAPPING_INDEX_RDMA11,	/*0x0B   VRDMA11 WMIX2 path */
	MAPPING_INDEX_RDMA12,	/*0x0C   VRDMA12 WMIX3 path */
	MAPPING_INDEX_RDMA13,	/*0x0D   GRDMA13 WMIX3 path */
	MAPPING_INDEX_RDMA14,	/*0x0E   VRDMA14 WMIX4 path */
	MAPPING_INDEX_RDMA15,	/*0x0F   GRDMA15 WMIX4 path */
	MAPPING_INDEX_VIN00,	/*0x10   VIDEOIN0 WMIX5 path */
	MAPPING_INDEX_RDMA16,	/*0x11   GRDMA16 WMIX5 path */
	MAPPING_INDEX_VIN01,	/*0x12   VIDEOIN1 WMIX6 path */
	MAPPING_INDEX_RDMA17,	/*0x13   GRDMA17 WMIX6 path */
	MAPPING_INDEX_WDMA00 = 20,	/*0x14   WMIX0 VWDMA0 path */
	MAPPING_INDEX_WDMA01,	/*0x15   WMIX1 VWDMA1 path */
	MAPPING_INDEX_WDMA02,	/*0x16   WMIX2V WDMA2 path */
	MAPPING_INDEX_WDMA03,	/*0x17   WMIX3 VWDMA3 path */
	MAPPING_INDEX_WDMA04,	/*0x18   WMIX4 VWDMA4 path */
	MAPPING_INDEX_WDMA05,	/*0x19   WMIX5 VWDMA5 path */
	MAPPING_INDEX_WDMA06,	/*0x1A   WMIX5 VWDMA6 path */
	MAPPING_INDEX_WDMA07,	/*0x1B   WMIX6 VWDMA7 path */
	MAPPING_INDEX_WDMA08,	/*0x1C   WMIX6 VWDMA8 path */
	MAPPING_INDEX_MAX,
};

static const struct lut_drv_type *lut_api;

static unsigned int lut_mapping_data[MAPPING_INDEX_MAX];

static void lut_drv_fill_mapping_table(void)
{
#if defined(VIOC_RDMA00)
	lut_mapping_data[MAPPING_INDEX_RDMA00] = VIOC_RDMA00;
#endif
#if defined(VIOC_RDMA01)
	lut_mapping_data[MAPPING_INDEX_RDMA01] = VIOC_RDMA01;
#endif
#if defined(VIOC_RDMA02)
	lut_mapping_data[MAPPING_INDEX_RDMA02] = VIOC_RDMA02;
#endif
#if defined(VIOC_RDMA03)
	lut_mapping_data[MAPPING_INDEX_RDMA03] = VIOC_RDMA03;
#endif
#if defined(VIOC_RDMA04)
	lut_mapping_data[MAPPING_INDEX_RDMA04] = VIOC_RDMA04;
#endif
#if defined(VIOC_RDMA05)
	lut_mapping_data[MAPPING_INDEX_RDMA05] = VIOC_RDMA05;
#endif
#if defined(VIOC_RDMA06)
	lut_mapping_data[MAPPING_INDEX_RDMA06] = VIOC_RDMA06;
#endif
#if defined(VIOC_RDMA07)
	lut_mapping_data[MAPPING_INDEX_RDMA07] = VIOC_RDMA07;
#endif
#if defined(VIOC_RDMA08)
	lut_mapping_data[MAPPING_INDEX_RDMA08] = VIOC_RDMA08;
#endif
#if defined(VIOC_RDMA09)
	lut_mapping_data[MAPPING_INDEX_RDMA09] = VIOC_RDMA09;
#endif
#if defined(VIOC_RDMA10)
	lut_mapping_data[MAPPING_INDEX_RDMA10] = VIOC_RDMA10;
#endif
#if defined(VIOC_RDMA11)
	lut_mapping_data[MAPPING_INDEX_RDMA11] = VIOC_RDMA11;
#endif
#if defined(VIOC_RDMA12)
	lut_mapping_data[MAPPING_INDEX_RDMA12] = VIOC_RDMA12;
#endif
#if defined(VIOC_RDMA13)
	lut_mapping_data[MAPPING_INDEX_RDMA13] = VIOC_RDMA13;
#endif
#if defined(VIOC_RDMA14)
	lut_mapping_data[MAPPING_INDEX_RDMA14] = VIOC_RDMA14;
#endif
#if defined(VIOC_RDMA15)
	lut_mapping_data[MAPPING_INDEX_RDMA15] = VIOC_RDMA15;
#endif
#if defined(VIOC_VIN00)
	lut_mapping_data[MAPPING_INDEX_VIN00] = VIOC_VIN00;
#endif
#if defined(VIOC_RDMA16)
	lut_mapping_data[MAPPING_INDEX_RDMA16] = VIOC_RDMA16;
#endif
#if defined(VIOC_VIN01)
	lut_mapping_data[MAPPING_INDEX_VIN01] = VIOC_VIN01;
#endif
#if defined(VIOC_RDMA17)
	lut_mapping_data[MAPPING_INDEX_RDMA17] = VIOC_RDMA17;
#endif
#if defined(VIOC_WDMA00)
	lut_mapping_data[MAPPING_INDEX_WDMA00] = VIOC_WDMA00;
#endif
#if defined(VIOC_WDMA01)
	lut_mapping_data[MAPPING_INDEX_WDMA01] = VIOC_WDMA01;
#endif
#if defined(VIOC_WDMA02)
	lut_mapping_data[MAPPING_INDEX_WDMA02] = VIOC_WDMA02;
#endif
#if defined(VIOC_WDMA03)
	lut_mapping_data[MAPPING_INDEX_WDMA03] = VIOC_WDMA03;
#endif
#if defined(VIOC_WDMA04)
	lut_mapping_data[MAPPING_INDEX_WDMA04] = VIOC_WDMA04;
#endif
#if defined(VIOC_WDMA05)
	lut_mapping_data[MAPPING_INDEX_WDMA05] = VIOC_WDMA05;
#endif
#if defined(VIOC_WDMA06)
	lut_mapping_data[MAPPING_INDEX_WDMA06] = VIOC_WDMA06;
#endif
#if defined(VIOC_WDMA07)
	lut_mapping_data[MAPPING_INDEX_WDMA07] = VIOC_WDMA07;
#endif
#if defined(VIOC_WDMA08)
	lut_mapping_data[MAPPING_INDEX_WDMA08] = VIOC_WDMA08;
#endif
}
//int lut_drv_api_get_plugin(unsigned int lut_number);
static int lut_drv_set_plugin(const struct lut_drv_type *lut, unsigned int lut_number,
			      unsigned int plugin, unsigned int plug_in_ch);
//int lut_drv_api_set_plugin(unsigned int lut_number, int plugin, int plug_in_ch);
static int lut_get_real_lut_table_number(unsigned int input_lut_number);
//int lut_drv_api_set_plugin_with_vioc_type(unsigned int lut_number, int plugin,
//					  unsigned int plugComp);

/* External APIs */
/* if lut is not plugged then return value will be -1 */
int lut_drv_api_get_plugin(unsigned int lut_number)
{
	int ret = -1;
	int enable;

	if (lut_api != NULL) {
		enable = tcc_get_lut_enable(lut_number);
		if (enable > 0) {
			ret = tcc_get_lut_plugin(lut_number);
			if (ret < 0) {	// lut not vioc type
				(void)pr_err(
				       "[ERR][LUT] %s lut number %d is out of range\n",
				       __func__, lut_number);
			}
		} else if (enable < 0) {
			(void)pr_err(
			       "[ERR][LUT] %s lut number %d is out of range\n",
			       __func__, lut_number);
		} else {	// lut not enabled
		}
	} else {
		(void)pr_err(
		       "[ERR][LUT] %s may be lut driver does not probed\n",
		       __func__);
	}
	return ret;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(lut_drv_api_get_plugin);

int lut_drv_api_set_plugin(unsigned int lut_number, int plugin, int plug_in_ch)
{
	int ret = -1;

	if ((lut_api != NULL) && (plug_in_ch >= 0) && (plugin >=0)) {
		ret =
		    lut_drv_set_plugin(lut_api, lut_number, (unsigned int)plugin, (unsigned int)plug_in_ch);
	} else {
		(void)pr_err(
		       "[ERR][LUT] %s may be lut driver does not probed\n",
		       __func__);
	}
	return ret;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(lut_drv_api_set_plugin);

int lut_drv_api_set_plugin_with_vioc_type(unsigned int lut_number, int plugin,
					  unsigned int plugComp)
{
	int ret = -1;
	int mapping_index;

	if (lut_api != NULL) {
		for (mapping_index = 0; mapping_index < MAPPING_INDEX_MAX;
		     mapping_index++) {
			if (plugComp == lut_mapping_data[mapping_index]) {
				/* Prevent KCS warning */
				break;
			}
		}

		if ((mapping_index < MAPPING_INDEX_MAX) && (plugin >= 0)) {
			ret =
			    lut_drv_set_plugin(lut_api, lut_number, (unsigned int)plugin,
					       (unsigned int)mapping_index);
		}
	} else {
		(void)pr_err(
		       "[ERR][LUT] %s may be lut driver does not probed\n",
		       __func__);
	}
	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(lut_drv_api_set_plugin_with_vioc_type);

/* Internal APIs */
static int lut_get_real_lut_table_number(unsigned int input_lut_number)
{
	int lut_number;

	switch (input_lut_number) {
	case LUT_DEV0:
	case LUT_DEV1:
		lut_number = (int)input_lut_number;
		break;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case LUT_DEV2:
		lut_number = (int)input_lut_number;
		break;
#if defined(CONFIG_ARCH_TCC805X)
	case LUT_DEV3:
		lut_number = (int)input_lut_number;
		break;
#endif
#endif

	case LUT_COMP0:
		lut_number = (int)input_lut_number;
		break;

	case LUT_COMP1:
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) \
			|| defined(CONFIG_ARCH_TCC901X)
		lut_number = 5;	/* Number of LUT in VIOC1 RGB is 5 */
#else
		lut_number = (int)input_lut_number;
#endif
		break;

#if defined(CONFIG_ARCH_TCC897X)
	case LUT_COMP2:
	case LUT_COMP3:
		lut_number = (int)input_lut_number;
		break;
#endif
	default:
		lut_number = -1;
		break;
	}

	return lut_number;
}

/*
 * This api plugin RDMA/WDMA/VIN to LUT
 */
static int lut_drv_set_plugin(const struct lut_drv_type *lut, unsigned int lut_number,
			      unsigned int plugin, unsigned int plug_in_ch)
{
	int ret = -1;
	int is_dev = -1;

	unsigned int plugComp;

	//do {
		if (lut == NULL) {
			/* Prevent KCS warnings */
			ret = -1;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto plugin_ret;
			//break;
		}

		(void)lut_get_address(lut_number, &is_dev);
		// check lut_number is VIOC_LUT

		if (is_dev != 0) {
			(void)pr_err
			    (
				"[ERR][LUT] %s lut number %d is out of range\n",
			     __func__, lut_number);
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto plugin_ret;
			//break;
		}

		if (plugin == 0U) {
			if((UINT_MAX - lut_number) >= VIOC_LUT) {
				/* prevent KCS */
				tcc_set_lut_enable((VIOC_LUT + lut_number), 0);
			}
#if TCC_LUT_DEBUG
			(void)pr_info(
			       "[INF][LUT] %s disable lut_number(%d)\n",
			       __func__, VIOC_LUT + lut_number);
#endif
		} else {
			if ((plug_in_ch >= (unsigned int)MAPPING_INDEX_MAX)) {
				(void)pr_info(
				       "[INF][LUT] %s (%d) is out of range on LUT\n",
				       __func__, plug_in_ch);
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto plugin_ret;
				//break;
			}
			plugComp = lut_mapping_data[plug_in_ch];
			if (get_vioc_type(plugComp) == 0U) {
				(void)pr_err(
				       "[ERR][LUT] %s does not support ch(%d)\n",
				       __func__, plugComp);
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto plugin_ret;
				//break;
			}
			if((UINT_MAX - lut_number) >= VIOC_LUT) {
				if (tcc_set_lut_plugin((VIOC_LUT + lut_number), plugComp)
					< 0) {
					(void)pr_err(
						"[ERR][LUT] %s lut number %d is out of range\n",
						__func__, lut_number);
					/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
					goto plugin_ret;
					//break;
				}
				tcc_set_lut_enable((VIOC_LUT + lut_number), 1);
			}
#if TCC_LUT_DEBUG
			(void)pr_info(
			       "[INF][LUT] %s enable lut_number(%d)\n",
			       __func__, (VIOC_LUT + lut_number));
#endif
		}
		ret = 0;
	//} while ((bool)0);
plugin_ret:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int lut_drv_set_onoff(struct lut_drv_type *lut, unsigned int lut_number,
			     unsigned int onoff)
{
	int ret = -1;

	do {
		if (lut == NULL) {
			/* Prevent KCS warn */	
			break;
		}

#if TCC_LUT_DEBUG
		(void)pr_info(
		       "[INF][LUT] %s lut num:%d enable:%d\n",
		       __func__, VIOC_LUT + lut_number, onoff);
#endif
		if((UINT_MAX - lut_number) >= VIOC_LUT) {
		//if((VIOC_LUT + lut_number) < UINT_MAX){
			/* Prvent KCS */
			tcc_set_lut_enable((VIOC_LUT + lut_number), onoff);
		}
		ret = 0;
	} while (false);
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static long lut_drv_ioctl(struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	int ret = -EFAULT;
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct lut_drv_type *lut = dev_get_drvdata(misc->parent);

	switch (cmd) {
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case TCC_LUT_SET:
		{
			int lut_number;
			struct VIOC_LUT_VALUE_SET *lut_cmd;

			lut_cmd =
			/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
			    kmalloc(sizeof(struct VIOC_LUT_VALUE_SET),
				/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
				    GFP_KERNEL);
			if (lut_cmd == NULL) {
					(void)pr_err(
					       "[ERR][LUT] %s TCC_LUT_SET_EX out of memory\n",
					       __func__);
					break;
			}
			if ((bool)copy_from_user
				/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
			    ((void *)lut_cmd, (const void *)arg,
			     sizeof(struct VIOC_LUT_VALUE_SET))) {
				kfree(lut_cmd);
				break;
			}
			lut_number =
			    lut_get_real_lut_table_number(lut_cmd->lut_number);
			if (lut_number < 0) {
				(void)pr_err(
				       "[ERR][LUT] %s TCC_LUT_SET invalid lut number[%d]\n",
				       __func__, lut_cmd->lut_number);
				kfree(lut_cmd);
				break;
			}
#if defined(CONFIG_TCC_LUT_DEBUG_DUMP)
			switch (lut_number) {
			case 3:
#if TCC_LUT_DEBUG
				(void)pr_info(
				       "[INF][LUT]  TCC_LUT_SET LUT_COMP0R\n");
#endif
				memcpy(lut->Gamma_vioc_lut_3, lut_cmd->Gamma,
				       sizeof(unsigned int) * 1024);
				break;
			case 4:
#if TCC_LUT_DEBUG
				(void)pr_info(
				       "[INF][LUT]  TCC_LUT_SET LUT_COMP0Y\n");
#endif
				memcpy(lut->Gamma_vioc_lut_4, lut_cmd->Gamma,
				       sizeof(unsigned int) * 1024);
				break;
			case 5:
#if TCC_LUT_DEBUG
				(void)pr_info(
				       "[INF][LUT]  TCC_LUT_SET LUT_COMP1R\n");
#endif
				memcpy(lut->Gamma_vioc_lut_5, lut_cmd->Gamma,
				       sizeof(unsigned int) * 1024);
				break;
			case 6:
#if TCC_LUT_DEBUG
				(void)pr_info(
				       "[INF][LUT]  TCC_LUT_SET LUT_COMP1Y\n");
#endif
				memcpy(lut->Gamma_vioc_lut_6, lut_cmd->Gamma,
				       sizeof(unsigned int) * 1024);
				break;
			}
#endif

			tcc_set_lut_table((VIOC_LUT + (unsigned int)lut_number),
					  lut_cmd->Gamma);
			kfree(lut_cmd);
			ret = 0;
		}
		break;
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case TCC_LUT_SET_EX:
		{
			int lut_number;
			int ret_ex = 0;
			struct VIOC_LUT_VALUE_SET_EX *lut_value_set_ex = NULL;

			lut_value_set_ex =
			/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
			    kmalloc(sizeof(struct VIOC_LUT_VALUE_SET_EX),
			/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
				    GFP_KERNEL);

			do {
				if (lut_value_set_ex == NULL) {
					(void)pr_err(
					       "[ERR][LUT] %s TCC_LUT_SET_EX out of memory\n",
					       __func__);
					ret_ex = -1;
					//break;
				}

				if(ret_ex != -1) {
					if ((bool)copy_from_user((void *)lut_value_set_ex,
				/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
						(const void *)arg,
						sizeof(struct
						VIOC_LUT_VALUE_SET_EX))) {
						(void)pr_err(
							"[ERR][LUT] %s TCC_LUT_SET_EX failed copy from user\n",
							__func__);
						ret_ex = -1;
						//break;
					}
				}
				
				if(ret_ex != -1) {
					if (LUT_TABLE_SIZE !=
						lut_value_set_ex->lut_size) {
						(void)pr_err(
							"[ERR][LUT] %s TCC_LUT_SET_EX table size mismatch %d != %d\n",
							__func__, LUT_TABLE_SIZE,
							lut_value_set_ex->lut_size);
						ret_ex = -1;
						//break;
					}
				}
				if(ret_ex != -1) {
					lut_number =
						lut_get_real_lut_table_number
						(lut_value_set_ex->lut_number);
					if (lut_number == -1) {
						(void)pr_err(
							"[ERR][LUT] %s TCC_LUT_SET_EX invalid lut number[%d]\n",
							__func__,
							lut_value_set_ex->lut_number);
						ret_ex = -1;
						//break;
					}
				}
#if TCC_LUT_DEBUG
				(void)pr_info(
				       "[INF][LUT] %s TCC_LUT_SET_EX lut_sel = %d\n",
				       __func__, lut_number);
#endif

#if defined(CONFIG_TCC_LUT_DEBUG_DUMP)
				switch (lut_number) {
				case 3:
					(void)pr_info(
					       "[INF][LUT]  TCC_LUT_SET LUT_COMP0R\n");
					memcpy(lut->Gamma_vioc_lut_3,
					       lut_value_set_ex->Gamma,
					       sizeof(unsigned int) * 1024);
					break;
				case 4:
					(void)pr_info(
					       "[INF][LUT]  TCC_LUT_SET LUT_COMP0Y\n");
					memcpy(lut->Gamma_vioc_lut_4,
					       lut_value_set_ex->Gamma,
					       sizeof(unsigned int) * 1024);
					break;
				case 5:
					(void)pr_info(
					       "[INF][LUT]  TCC_LUT_SET LUT_COMP1R\n");
					memcpy(lut->Gamma_vioc_lut_5,
					       lut_value_set_ex->Gamma,
					       sizeof(unsigned int) * 1024);
					break;
				case 6:
					(void)pr_info(
					       "[INF][LUT]  TCC_LUT_SET LUT_COMP1Y\n");
					memcpy(lut->Gamma_vioc_lut_6,
					       lut_value_set_ex->Gamma,
					       sizeof(unsigned int) * 1024);
					break;
				}
#endif
				if(ret_ex != -1) {
					if(lut_number >= 0) {
						/* Prevent KCS */
						tcc_set_lut_table(VIOC_LUT + (unsigned int)lut_number,
							lut_value_set_ex->Gamma);
					}
				}
			} while ((bool)0);

			if (lut_value_set_ex != NULL) {
				/* Prevent KCS */
				kfree(lut_value_set_ex);
			}
			ret = ret_ex; // ret 0 on success
		}
		break;
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case TCC_LUT_ONOFF:
		{
			struct VIOC_LUT_ONOFF_SET lut_cmd;

			if ((bool)copy_from_user
			/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
			    ((void *)&lut_cmd, (const void *)arg,
			     sizeof(struct VIOC_LUT_ONOFF_SET))) {
				break;
			}
			ret =
			    lut_drv_set_onoff(lut, lut_cmd.lut_number,
					      lut_cmd.lut_onoff);
		}
		break;
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case TCC_LUT_PLUG_IN:
		{
			struct VIOC_LUT_PLUG_IN_SET lut_cmd;

			if ((bool)copy_from_user
			/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
			    ((void *)&lut_cmd, (const void *)arg,
			     sizeof(lut_cmd))) {
					 /* Prevent KCS */
					break;
				}
				/*
			if (lut_cmd.lut_number < 0U) {
				(void)pr_err(
				       "[ERR][LUT] %s TCC_LUT_ONOFF invalid lut number[%d]\n",
				       __func__, lut_cmd.lut_number);
				break;
			}*/
			ret =
			    lut_drv_set_plugin(lut, lut_cmd.lut_number,
					       lut_cmd.enable,
					       lut_cmd.lut_plug_in_ch);
		}
		break;
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case TCC_LUT_GET_DEPTH:
		{
			unsigned int lut_depth = LUT_COLOR_DEPTH;

			if ((bool)copy_to_user
			    ((void __user *)arg, &lut_depth,
			     sizeof(lut_depth))) {
				break;
			}
#if TCC_LUT_DEBUG
			(void)pr_info(
			       "[INF][LUT] %sTCC_LUT_GET_DEPTH LUT depth is  %d\n",
			       __func__, lut_depth);
#endif
			ret = 0;
		}
		break;

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)\
	 || defined(CONFIG_ARCH_TCC901X)
	case TCC_LUT_SET_CSC_COEFF:
		{
			if ((void *)arg == NULL) {
				tcc_set_default_lut_csc_coeff();
			} else {
				struct VIOC_LUT_CSC_COEFF csc_coeff;
				unsigned int lut_csc_11_12, lut_csc_13_21,
				    lut_csc_22_23, lut_csc_31_32, lut_csc_33;
				if (copy_from_user
				    ((void *)&csc_coeff, (const void *)arg,
				     sizeof(struct VIOC_LUT_CSC_COEFF)))
					break;

				lut_csc_11_12 =
				    csc_coeff.csc_coeff_1[0]
						| (csc_coeff.csc_coeff_1[1] <<
							16);
				lut_csc_13_21 =
				    csc_coeff.csc_coeff_1[2]
						| (csc_coeff.csc_coeff_2[0] <<
							16);
				lut_csc_22_23 =
				    csc_coeff.csc_coeff_2[1]
						| (csc_coeff.csc_coeff_2[2] <<
							16);
				lut_csc_31_32 =
				    csc_coeff.csc_coeff_3[0]
						| (csc_coeff.csc_coeff_3[1] <<
							16);
				lut_csc_33 = csc_coeff.csc_coeff_3[2];
#if TCC_LUT_DEBUG
				(void)pr_info(
				       "[INF][LUT] %sTCC_LUT_PRESET_SET csc (0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n",
				       __func__, lut_csc_11_12, lut_csc_13_21,
				       lut_csc_22_23, lut_csc_31_32,
				       lut_csc_33);
#endif
				tcc_set_lut_csc_coeff(lut_csc_11_12,
						      lut_csc_13_21,
						      lut_csc_22_23,
						      lut_csc_31_32,
						      lut_csc_33);
			}
			ret = 0;
		}
		break;

	case TCC_LUT_SET_MIX_CONIG:
		{
			struct VIOC_LUT_MIX_CONFIG mix_config;

			if (copy_from_user
			    ((void *)&mix_config, (const void *)arg,
			     sizeof(struct VIOC_LUT_MIX_CONFIG)))
				break;
#if TCC_LUT_DEBUG
			(void)pr_info(
			       "[INF][LUT] %sTCC_LUT_SET_MIX_CONIG r2y_sel(%d), bypass(%d)\n",
			       __func__, mix_config.r2y_sel, mix_config.bypass);
#endif
			tcc_set_mix_config(mix_config.r2y_sel,
					   mix_config.bypass);
			ret = 0;
		}
		break;
#endif
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case TCC_LUT_GET_UPDATE_PEND:
		{
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)\
	|| defined(CONFIG_ARCH_TCC901X)
			int lut_number = 0;
#endif
			struct VIOC_LUT_UPDATE_PEND lut_update_pend;

			if ((bool)copy_from_user((void *)&lut_update_pend,
			/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
					   (const void *)arg,
					   sizeof
					   (struct VIOC_LUT_UPDATE_PEND))) {
				(void)pr_err(
				       "[ERR][LUT] %s TCC_LUT_GET_UPDATE_PEND failed copy from user\n",
				       __func__);
				break;
			}
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)\
	|| defined(CONFIG_ARCH_TCC901X)
			lut_number =
			    lut_get_real_lut_table_number
			    (lut_update_pend.lut_number);
			if (lut_number == -1) {
				pr_err(
				       "[ERR][LUT] %s TCC_LUT_GET_UPDATE_PEND invalid lut number[%d]\n",
				       __func__, lut_update_pend.lut_number);
				break;
			}

			/* Y Table? */
			if (lut_update_pend.param & 1) {
				if (lut_number == 3 || lut_number == 5) {
					lut_number++;
#if TCC_LUT_DEBUG
					(void)pr_info(
					       "[INF][LUT] %sTCC_LUT_GET_UPDATE_PEND y-lut lut_sel = %d\n",
					       __func__, lut_number);
#endif
				}
			}

			//pr_info("%s LUT\n",
			//(lut_number == 4 || lut_number == 6) ? "Y" : "RGB");

			lut_update_pend.update_pend =
			    tcc_get_lut_update_pend(lut_number);
#else
			lut_update_pend.update_pend = 0;
#endif
			if ((bool)copy_to_user
			/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
			/* coverity[cert_int36_c_violation : FALSE] */
			    ((void __user *)arg, &lut_update_pend,
			     sizeof(struct VIOC_LUT_UPDATE_PEND))) {
				(void)pr_err(
				       "[ERR][LUT] %s TCC_LUT_GET_UPDATE_PEND failed copy to user\n",
				       __func__);
				break;
			}
			ret = 0;
		}
		break;

	default:
		(void)pr_err(
		       "[ERR][LUT]  not supported LUT IOCTL(0x%x).\n",
		       cmd);
		break;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_5_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int lut_drv_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct lut_drv_type *lut = dev_get_drvdata(misc->parent);
	(void)inode;
	(void)filp;
#if TCC_LUT_DEBUG
	(void)pr_info("[INF][LUT] %s In -open(%d)\n",
	       __func__, lut->dev_opened);
#endif

	if (lut != NULL) {
		/* Prevent KCS */
		lut->dev_opened++;
	}
	return ret;
}

/* coverity[misra_c_2012_rule_5_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int lut_drv_release(struct inode *inode, struct file *filp)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct lut_drv_type *lut = dev_get_drvdata(misc->parent);

	(void)inode;
	(void)filp;
	if ((lut != NULL) && (lut->dev_opened > 0U)) {
		/* Prevent KCS */
		lut->dev_opened--;
	}
#if TCC_LUT_DEBUG
	(void)pr_info("[INF][LUT] %s open(%d).\n",
	       __func__, lut->dev_opened);
#endif

	return 0;
}

static const struct file_operations lut_drv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = lut_drv_ioctl,
	.open = lut_drv_open,
	.release = lut_drv_release,
};

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t proc_lut_write_debug(struct file *filp,
				    const char __user *buffer, size_t cnt,
				    loff_t *off_set)
{
	ssize_t size;
	unsigned int debug_param[2];
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct lut_drv_type *lut = PDE_DATA(file_inode(filp));

	char *debug_buffer = NULL;
		/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		if((UINT_MAX - 1U) >= cnt) {
			/* KCS */
	    	debug_buffer =
			devm_kzalloc(lut->misc->parent, (cnt + 1U), GFP_KERNEL);
		}

	//do {
		if (debug_buffer == NULL) {
			size = -ENOMEM;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto proc_write_ret;
			//break;
		}
		size =
		    simple_write_to_buffer(debug_buffer, cnt, off_set, buffer,
					   cnt);
		if (size != (ssize_t)cnt) {

			if (size >= 0) {
				size = -EIO;
				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto proc_write_ret;
				//break;
			}
		}
		debug_buffer[cnt] = '\0';

		/* coverity[misra_c_2012_rule_21_6_violation : FALSE] */
		(void)sscanf(debug_buffer, "%u, %u", &debug_param[0],
		       &debug_param[1]);
		devm_kfree(lut->misc->parent, debug_buffer);

		(void)pr_info(
		       "[INF][LUT]  Debug Param  <%d, %d>\n",
		       debug_param[0], debug_param[1]);
		switch (debug_param[0]) {
		case 0:
			/* Nothing */
			break;
		case 1:{
#if defined(CONFIG_TCC_LUT_DEBUG_DUMP)
				int i;
#endif
				/* Dump */
				int lug_enable[2];
				int plugin_in_ch[2];

				lug_enable[0] =
				    tcc_get_lut_enable(VIOC_LUT_COMP0);
				lug_enable[1] =
				    tcc_get_lut_enable(VIOC_LUT_COMP1);

				plugin_in_ch[0] =
				    tcc_get_lut_plugin(VIOC_LUT_COMP0);
				plugin_in_ch[1] =
				    tcc_get_lut_plugin(VIOC_LUT_COMP1);

				if (lug_enable[0] == 0) {
					(void)pr_info(
					       "[INF][LUT] %s LUT0 is not plugined\n",
					       __func__);
				} else {
					(void)pr_info(
					       "[INF][LUT] %s LUT is plugined to 0x%x\n",
					       __func__, plugin_in_ch[0]);
#if defined(CONFIG_TCC_LUT_DEBUG_DUMP)
					(void)pr_info(
					       "[INF][LUT]  Dump RGB Table\n");
					for (i = 0; i < 16; i++) {
						(void)pr_info(
						       "[INF][LUT]  0x%08x ",
						       lut->Gamma_vioc_lut_3
						       [i]);
					}
					(void)pr_info(
					       "[INF][LUT]  Dump Y Table\n");
					for (i = 0; i < 16; i++) {
						(void)pr_info(
						       "[INF][LUT]  0x%08x ",
						       lut->Gamma_vioc_lut_4
						       [i]);
					}
					(void)pr_info(
					       "[INF][LUT]\n");
#endif
				}
				if (lug_enable[1] == 0) {
					(void)pr_info(
					       "[INF][LUT] %s LUT1 is not plugined\n",
					       __func__);
				} else {
					(void)pr_info(
					       "[INF][LUT] %s LUT is plugined to 0x%x\n",
					       __func__, plugin_in_ch[1]);
#if defined(CONFIG_TCC_LUT_DEBUG_DUMP)
					(void)pr_info(
					       "[INF][LUT]  Dump RGB Table\n");
					for (i = 0; i < 16; i++) {
						(void)pr_info(
						       "[INF][LUT]  0x%08x ",
						       lut->Gamma_vioc_lut_3
						       [i]);
					}
					(void)pr_info(
					       "[INF][LUT]\nDump Y Table\n");
					for (i = 0; i < 16; i++) {
						(void)pr_info(
						       "[INF][LUT]  0x%08x ",
						       lut->Gamma_vioc_lut_4
						       [i]);
					}
					(void)pr_info(
					       "[INF][LUT]\n");
#endif
				}
			}
			break;
			default :
				/* Nothing */
				(void)pr_info(
					       "[INF][LUT]\n");
				break;
		}
	//} while ((bool)0);

proc_write_ret:
	return size;
}

/* coverity[misra_c_2012_rule_6_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int proc_open(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;
	(void)try_module_get(THIS_MODULE);
	return 0;
}

/* coverity[misra_c_2012_rule_6_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int proc_close(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;
	module_put(THIS_MODULE);
	return 0;
}

static const struct file_operations proc_fops_lut_debug = {
	.owner = THIS_MODULE,
	.open = proc_open,
	.release = proc_close,
	.write = proc_lut_write_debug,
};

static int lut_drv_probe(struct platform_device *pdev)
{
	struct lut_drv_type *lut;
	int ret = -ENODEV;

	lut = kzalloc(sizeof(struct lut_drv_type), GFP_KERNEL);
	if (lut == NULL) {
		/* Prevent KCS */
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_lut_alloc;
	}

	if (of_property_read_u32(pdev->dev.of_node, "dev_max", &lut->dev_max) <
	    0) {
		lut->dev_max = DEFAULT_DEV_MAX;
	}

	if (of_property_read_u32(pdev->dev.of_node, "vioc_max", &lut->vioc_max)
	    < 0) {
		lut->vioc_max = DEFAULT_VIOC_MAX;
	}
	(void)pr_info(
	       "[INF][LUT]  dev_max is %d and vioc_max is %d\n",
	       lut->dev_max, lut->vioc_max);
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	lut->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (lut->misc == NULL) {
		/* Prevent KCS */
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_misc_alloc;
	}

	/* register scaler discdevice */
	lut->misc->minor = MISC_DYNAMIC_MINOR;
	lut->misc->fops = &lut_drv_fops;
	lut->misc->name = "tcc_lut";
	lut->misc->parent = &pdev->dev;
	ret = misc_register(lut->misc);
	if (ret != 0) {
		/* prevent KCS warnings */
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_misc_register;
	}

	lut_drv_fill_mapping_table();

	platform_set_drvdata(pdev, lut);

	/* ProcFS */
	lut->proc_dir = proc_mkdir("tcc_lut", NULL);
	if (lut->proc_dir == NULL) {
		(void)pr_err(
		       "[ERR][LUT] %s:Could not create file system @ /proc/tcc_lut\n",
		       __func__);
	} else {
		/* coverity[misra_c_2012_rule_7_1_violation : FALSE] */
		lut->proc_debug = proc_create_data("debug", (unsigned int)S_IFREG | (unsigned int)0222,
						   lut->proc_dir,
						   &proc_fops_lut_debug, lut);
		if (lut->proc_debug == NULL) {
			(void)pr_err(
			       "[ERR][LUT] %s:Could not create file system @ /proc/tcc_lut/debug\n",
			       __func__);
		}
	}

	/* Copy lut to lut_api to support external APIs */
	lut_api = lut;

	(void)pr_info(
	       "[INF][LUT] %s: :%s, Driver %s Initialized\n",
	       __func__, LUT_VERSION, pdev->name);
	ret = 0;
	/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
	goto normal_return;

err_misc_register:
	misc_deregister(lut->misc);

err_misc_alloc:
	kfree(lut->misc);
err_lut_alloc:
	kfree(lut);
	(void)pr_err("[ERR][LUT] %s: %s: err ret:%d\n",
	       __func__, pdev->name, ret);
normal_return:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int lut_drv_remove(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct lut_drv_type *lut =
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	    (struct lut_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(lut->misc);
	kfree(lut);

	lut_api = NULL;
	return 0;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int lut_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	(void)pdev;
	(void)state;
	return 0;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int lut_drv_resume(struct platform_device *pdev)
{
	(void)pdev;
	return 0;
}

static const struct of_device_id lut_of_match[] = {
	{.compatible = "telechips,vioc_lut"},
	{}
};

MODULE_DEVICE_TABLE(of, lut_of_match);

static struct platform_driver lut_driver = {
	.probe = lut_drv_probe,
	.remove = lut_drv_remove,
	.suspend = lut_drv_suspend,
	.resume = lut_drv_resume,
	.driver = {
		   .name = "tcc_lut",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(lut_of_match),
#endif
		   },
};

static int __init lut_drv_init(void)
{
	return platform_driver_register(&lut_driver);
}

static void __exit lut_drv_exit(void)
{
	platform_driver_unregister(&lut_driver);
}

/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
module_init(lut_drv_init);
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
module_exit(lut_drv_exit);
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_AUTHOR("Telechips.");
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_DESCRIPTION("Telechips look up table Driver");

/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_LICENSE("GPL");

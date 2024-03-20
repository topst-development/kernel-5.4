// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
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
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <soc/tcc/chipinfo.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_ddicfg.h> // is_VIOC_REMAP
#include <video/tcc/vioc_viqe.h>

static int debug_config;
#define dprintk(msg...)						\
	do {							\
		if (debug_config == 1) {					\
			(void)pr_info("[DBG][VIOC_CONFIG] " msg);	\
		}						\
	} while ((bool)0)

static int vioc_config_sc_rdma_sel[] = {
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,  0x8,
	0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x11, 0x13,
};

static int vioc_config_sc_vin_sel[] = {
	0x10,
	0x12,
	0x0C,
	0x0E,
};

static int vioc_config_sc_wdma_sel[] = {
	0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
};

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC897X)
static int vioc_config_viqe_rdma_sel[] = {
	0x00, 0x01, 0x02, 0x03, -1, -1,  0x4, 0x5,  -1,
	-1,   0x06, 0x07, 0x08, -1, 0x9, -1,  0x0B, 0x0D,
};
#elif defined(CONFIG_ARCH_TCC805X)
static int vioc_config_viqe_rdma_sel[] = {
	0x00, 0x01, 0x02, 0x03, -1,   -1,  0x4, 0x5,  -1,
	-1,   0x06, 0x07, 0x08, 0x0E, 0x9, 0xF, 0x0B, 0x0D,
};
#endif

static int vioc_config_viqe_vin_sel[] = {
	0x0A,
	0x0C,
	0x08,
	0x09,
};

//static int vioc_config_lut_rdma_sel[] = {
//	0,    0x1,  0x2,  0x03, 0x04, 0x05, -1, 0x07, 0x08, // rdma 0 ~ 8
//	0x09, 0x0A, 0x0B, 0x0C, 0x0D, -1,   -1, 0x11, 0x13,
//};

//static int vioc_config_lut_vin_sel[] = {
//	0x10,
//	-1,
//};
//static int vioc_config_lut_wdma_sel[] = {
//	0x14, 0x15, 0x16, 0x17, -1, 0x19, 0x1A, 0x1B, 0x1C,
//};

#ifdef CONFIG_VIOC_PIXEL_MAPPER
static int vioc_config_pixel_mapper_rdma_sel[] = {
	0x00, 0x01, 0x02, 0x03, -1,   -1, -1, 0x5, -1, // rdma 0 ~ 8
	-1,   0x06, 0x07, 0x08, 0x09, -1, -1, 0xC, 0xD,
};

static int vioc_config_pixel_mapper_vin_sel[] = {
	0x0B,
	-1,
};
#endif //

#if defined(CONFIG_VIOC_AFBCDEC) || defined(CONFIG_VIOC_PVRIC_FBDC)
static int vioc_config_FBCDec_rdma_sel[] = {
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,  0x8,
	0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11,
};
#endif

static void __iomem *pIREQ_reg;

static void __iomem *CalcAddressViocComponent(unsigned int component)
{
	void __iomem *reg;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		reg = NULL;
		/* If necessary, add code and use it */
		//switch (get_vioc_index(component)) {
		//default:
		//	reg = NULL;
		//	break;
		//}
		break;
	case get_vioc_type(VIOC_SCALER):
		switch (get_vioc_index(component)) {
		case 0:
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			reg = (pIREQ_reg + CFG_PATH_SC0_OFFSET);
			break;
		case 1:
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			reg = (pIREQ_reg + CFG_PATH_SC1_OFFSET);
			break;
		case 2:
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			reg = (pIREQ_reg + CFG_PATH_SC2_OFFSET);
			break;
		case 3:
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			reg = (pIREQ_reg + CFG_PATH_SC3_OFFSET);
			break;
#if !defined(CONFIG_ARCH_TCC897X)
		case 4:
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			reg = (pIREQ_reg + CFG_PATH_SC4_OFFSET);
			break;
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		case 5:
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			reg = (pIREQ_reg + CFG_PATH_SC5_OFFSET);
			break;
		case 6:
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			reg = (pIREQ_reg + CFG_PATH_SC6_OFFSET);
			break;
		case 7:
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			reg = (pIREQ_reg + CFG_PATH_SC7_OFFSET);
			break;
#endif
		default:
			reg = NULL;
			break;
		}
		break;
	case get_vioc_type(VIOC_VIQE):
		switch (get_vioc_index(component)) {
		case 0:
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			reg = (pIREQ_reg + CFG_PATH_VIQE0_OFFSET);
			break;
#if !(defined(CONFIG_ARCH_TCC897X))
		case 1:
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			reg = (pIREQ_reg + CFG_PATH_VIQE1_OFFSET);
			break;
#endif
		default:
			reg = NULL;
			break;
		}
		break;
	case get_vioc_type(VIOC_DEINTLS):
		switch (get_vioc_index(component)) {
		case 0:
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			reg = (pIREQ_reg + CFG_PATH_DEINTLS_OFFSET);
			break;
		default:
			reg = NULL;
			break;
		}
		break;

#ifdef CONFIG_VIOC_MAP_DECOMP
	case get_vioc_type(VIOC_MC):
		reg = NULL;
		/* If necessary, add code and use it */
		//switch (get_vioc_index(component)) {
		//default:
		//	reg = NULL;
		//	break;
		//}
		break;
#endif

#ifdef CONFIG_VIOC_PIXEL_MAPPER
	case get_vioc_type(VIOC_PIXELMAP):
		switch (get_vioc_index(component)) {
		case 0:
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			reg = (pIREQ_reg + CFG_PATH_PM0_OFFSET);
			break;
		case 1:
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			reg = (pIREQ_reg + CFG_PATH_PM1_OFFSET);
			break;
		default:
			reg = NULL;
			break;
		}
		break;
#endif //
	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s-%d: wierd component(0x%x) type(0x%x) index(%d)\n",
		       __func__, __LINE__, component, get_vioc_type(component),
		       get_vioc_index(component));

		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		WARN_ON(1);
		reg = NULL;
		break;
	}

	return reg;
}

static int CheckScalerPathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_sc_rdma_sel[get_vioc_index(component)];
		break;
	case get_vioc_type(VIOC_VIN):
		ret = vioc_config_sc_vin_sel[get_vioc_index(component) / 2U];
		break;
	case get_vioc_type(VIOC_WDMA):
		ret = vioc_config_sc_wdma_sel[get_vioc_index(component)];
		break;
	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
	return ret;
}

static int CheckViqePathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_viqe_rdma_sel[get_vioc_index(component)];
		break;
	case get_vioc_type(VIOC_VIN):
		ret = vioc_config_viqe_vin_sel[get_vioc_index(component) / 2U];
		break;
	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
	return ret;
}

#ifdef CONFIG_VIOC_PIXEL_MAPPER
int CheckPixelMapPathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_pixel_mapper_rdma_sel[get_vioc_index(
			component)];
		break;
	case get_vioc_type(VIOC_VIN):
		ret = vioc_config_pixel_mapper_vin_sel
			[get_vioc_index(component) / 2U];
		break;
	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
	return ret;
}
#endif //

#if defined(CONFIG_VIOC_AFBCDEC) || defined(CONFIG_VIOC_PVRIC_FBDC)
static int CheckFBCDecPathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_FBCDec_rdma_sel[get_vioc_index(component)];
		break;
	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
	return ret;
}
#endif

#ifdef CONFIG_VIOC_MAP_DECOMP
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
static int CheckMCPathSelection(unsigned int component, unsigned int mc)
{
	int ret = 0;

	if (get_vioc_type(component) != get_vioc_type(VIOC_WMIX)) {
		/* Prevent KCS warning */
		ret = -1;
	}

	if (get_vioc_type(mc) == VIOC_MC) {
		if (get_vioc_index(mc) != 0U) {
			if (component == VIOC_WMIX0) {
				/* Prevent KCS warning */
				ret = -2;
			}
		} else {
			if (component > VIOC_WMIX0) {
				/* Prevent KCS warning */
				ret = -2;
			}
		}
	}

	if (ret < 0) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][VIOC_CONFIG] %s, ret:%d wrong path parameter(Path: 0x%08x MC: 0x%08x)\n",
		       __func__, ret, component, mc);
	}

	return ret;
}
#endif
#endif

/* coverity[HIS_metric_violation : FALSE] */
int VIOC_AUTOPWR_Enalbe(unsigned int component, unsigned int onoff)
{
	int shift_bit = -1;
	u32 value = 0U;
	int ret = -1;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		shift_bit = (int)PWR_AUTOPD_RDMA_SHIFT;
		ret = 0;
		break;
#ifdef CONFIG_VIOC_MAP_DECOMP
	case get_vioc_type(VIOC_MC):
		shift_bit = (int)PWR_AUTOPD_MC_SHIFT;
		ret = 0;
		break;
#endif //
	case get_vioc_type(VIOC_WMIX):
		shift_bit = (int)PWR_AUTOPD_MIX_SHIFT;
		ret = 0;
		break;
	case get_vioc_type(VIOC_WDMA):
		shift_bit = (int)PWR_AUTOPD_WDMA_SHIFT;
		ret = 0;
		break;
	case get_vioc_type(VIOC_SCALER):
		shift_bit = (int)PWR_AUTOPD_SC_SHIFT;
		ret = 0;
		break;
	case get_vioc_type(VIOC_VIQE):
		shift_bit = (int)PWR_AUTOPD_VIQE_SHIFT;
		ret = 0;
		break;
	case get_vioc_type(VIOC_DEINTLS):
		shift_bit = (int)PWR_AUTOPD_DEINTS_SHIFT;
		ret = 0;
		break;
#ifdef CONFIG_VIOC_PIXEL_MAPPER
	case get_vioc_type(VIOC_PIXELMAP):
		shift_bit = (int)PWR_AUTOPD_PM_SHIFT;
		ret = 0;
		break;
#endif
	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s, in error, Type :0x%x onoff:%d\n",
	       __func__, component, onoff);
		ret = -1;
		break;
	}

	if (ret < 0) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	/* shift_bit >= 0, Always */
	if (onoff != 0U) {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(pIREQ_reg + PWR_AUTOPD_OFFSET)
			 | (((u32)1U) << (unsigned int)shift_bit));
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(pIREQ_reg + PWR_AUTOPD_OFFSET)
			 & ~(((u32)1U) << (unsigned int)shift_bit));
	}

	__raw_writel(value, pIREQ_reg + PWR_AUTOPD_OFFSET);

	ret = 0;

FUNC_EXIT:
	return ret;
}

int VIOC_CONFIG_PlugIn(unsigned int component, unsigned int select)
{
	u32 value = 0;
	unsigned int loop = 0;
	void __iomem *reg;
	int plugin_path;
	int ret = (int)VIOC_PATH_DISCONNECTED;

	reg = CalcAddressViocComponent(component);
	if (reg == NULL) {
		/* Prevent KCS warning */
		ret = VIOC_DEVICE_INVALID;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	/* Check selection has type value. If has, select value is invalid */
	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_SCALER):
		plugin_path = CheckScalerPathSelection(select);
		if (plugin_path < 0) {
			/* Prevent KCS warning */
			ret = VIOC_DEVICE_INVALID;
		} else {
			ret = (int)VIOC_PATH_DISCONNECTED;
		}
		break;
	case get_vioc_type(VIOC_VIQE):
	case get_vioc_type(VIOC_DEINTLS):
		plugin_path = CheckViqePathSelection(select);
		if (plugin_path < 0) {
			/* Prevent KCS warning */
			ret = VIOC_DEVICE_INVALID;
		} else {
			ret = (int)VIOC_PATH_DISCONNECTED;
		}
		break;

#ifdef CONFIG_VIOC_PIXEL_MAPPER
	case get_vioc_type(VIOC_PIXELMAP):
		plugin_path = CheckPixelMapPathSelection(select);
		if (plugin_path < 0) {
			/* Prevent KCS warning */
			ret = VIOC_DEVICE_INVALID;
		} else {
			ret = (int)VIOC_PATH_DISCONNECTED;
			VIOC_AUTOPWR_Enalbe(component, 0); // for setting of lut table
		}
		break;
#endif //

	default:
		ret = VIOC_DEVICE_INVALID;
		break;
	}
	if (ret == VIOC_DEVICE_INVALID) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = ((__raw_readl(reg) & CFG_PATH_STS_MASK) >> CFG_PATH_STS_SHIFT);
	if (value == VIOC_PATH_CONNECTED) {
		value =
			((__raw_readl(reg) & CFG_PATH_SEL_MASK)
			 >> CFG_PATH_SEL_SHIFT);
		if (value != (unsigned int)plugin_path) {
			(void)pr_warn("[WAN][VIOC_CONFIG] %s, VIOC(T:%d I:%d) is plugged-out by force (from 0x%08x to %d)!!\n",
				__func__, get_vioc_type(component),
				get_vioc_index(component), value, plugin_path);
			(void)VIOC_CONFIG_PlugOut(component);
		}
	}

	value = (__raw_readl(reg) & ~(CFG_PATH_SEL_MASK | CFG_PATH_EN_MASK));
	value |= (((unsigned int)plugin_path << CFG_PATH_SEL_SHIFT) | ((u32)0x1U << CFG_PATH_EN_SHIFT));
	__raw_writel(value, reg);

	if ((__raw_readl(reg) & CFG_PATH_ERR_MASK) != 0U) {
		//(void)pr_err("[ERR][VIOC_CONFIG] %s, path configuration error(ERR_MASK). device is busy. VIOC(T:%d I:%d) Path:%d\n",
		//       __func__, get_vioc_type(component),
		//       get_vioc_index(component), path);
		value = (__raw_readl(reg) & ~(CFG_PATH_EN_MASK));
		__raw_writel(value, reg);
	}

	loop = 50U;
	while ((bool)1) {
		unsigned int ext_flag = 0;

		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		mdelay(1);
		loop--;
		value =
			((__raw_readl(reg) & CFG_PATH_STS_MASK)
			 >> CFG_PATH_STS_SHIFT);
		if (value == VIOC_PATH_CONNECTED) {
			ret = (int)VIOC_PATH_CONNECTED;
			ext_flag = 1U;
		} else {
			if (loop < 1U) {
				//(void)pr_err("[ERR][VIOC_CONFIG] %s, path configuration error(TIMEOUT). device is busy. VIOC(T:%d I:%d) Path:%d\n",
				//       __func__, get_vioc_type(component),
				//       get_vioc_index(component), path);
				ret = (int)VIOC_DEVICE_BUSY;
				ext_flag = 1U;
			}
		}
		if (ext_flag == 1U) {
			break;
		}
	}

FUNC_EXIT:
	if (ret != (int)VIOC_PATH_CONNECTED) {
		(void)pr_err("[ERR][VIOC_CONFIG] %s, in error, Type :0x%x Select:0x%x cfg_reg:0x%x\n",
			__func__, component, select, __raw_readl(reg));

		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		WARN_ON(1);
	}
	return ret;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_CONFIG_PlugIn);

int VIOC_CONFIG_PlugOut(unsigned int component)
{
	u32 value = 0U;
	unsigned int loop = 0U;
	void __iomem *reg;
	int ret = VIOC_PATH_CONNECTED;

	reg = CalcAddressViocComponent(component);
	if (reg == NULL) {
		ret = VIOC_DEVICE_INVALID;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = ((__raw_readl(reg) & CFG_PATH_STS_MASK) >> CFG_PATH_STS_SHIFT);
	if (value == VIOC_PATH_DISCONNECTED) {
		__raw_writel(0x00000000, reg);
		(void)pr_warn("[WAN][VIOC_CONFIG] %s, VIOC(T:%d I:%d) was already plugged-out!!\n",
			__func__, get_vioc_type(component),
			get_vioc_index(component));
		ret = (int)VIOC_PATH_DISCONNECTED;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	value = (__raw_readl(reg) & ~(CFG_PATH_EN_MASK));
	__raw_writel(value, reg);

	if ((__raw_readl(reg) & CFG_PATH_ERR_MASK) != 0U) {
		(void)pr_err("[ERR][VIOC_CONFIG] %s, path configuration error(ERR_MASK). device is busy. VIOC(T:%d I:%d)\n",
		       __func__, get_vioc_type(component),
		       get_vioc_index(component));
		value = (__raw_readl(reg) & ~(CFG_PATH_EN_MASK));
		__raw_writel(value, reg);
		ret = (int)VIOC_DEVICE_BUSY;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	loop = 100U;
	while ((bool)1) {
		unsigned int ext_flag = 0;

		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		mdelay(1);
		loop--;
		value =
			((__raw_readl(reg) & CFG_PATH_STS_MASK)
			 >> CFG_PATH_STS_SHIFT);
		if (value == VIOC_PATH_DISCONNECTED) {
			__raw_writel(0x00000000, reg);
			ret = (int)VIOC_PATH_DISCONNECTED;
			ext_flag = 1U;
		} else {
			if (loop < 1U) {
				(void)pr_err("[ERR][VIOC_CONFIG] %s, path configuration error(TIMEOUT). device is busy. VIOC(T:%d I:%d)\n",
				       __func__, get_vioc_type(component),
				       get_vioc_index(component));
				ret = VIOC_DEVICE_BUSY;
				ext_flag = 1U;
			}
		}
		if (ext_flag == 1U) {
			break;
		}
	}

FUNC_EXIT:
	if (ret != (int)VIOC_PATH_DISCONNECTED) {
		(void)pr_err("[ERR][VIOC_CONFIG] %s, in error, Type :0x%x cfg_reg:0x%x\n",
			__func__, component, __raw_readl(reg));

		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		WARN_ON(1);
	}
	return ret;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_CONFIG_PlugOut);

/* support bypass mode DMA */
static const unsigned int bypassDMA[] = {
#if defined(CONFIG_ARCH_TCC897X)
	/* RDMA */
	VIOC_RDMA00, VIOC_RDMA03, VIOC_RDMA04, VIOC_RDMA07, VIOC_RDMA12,
	VIOC_RDMA14,

	/* VIDEO-IN */
	VIOC_VIN00, VIOC_VIN30,
#else
	/* RDMA */
	VIOC_RDMA00, VIOC_RDMA03, VIOC_RDMA04, VIOC_RDMA07,
	VIOC_RDMA08, VIOC_RDMA14,
	VIOC_RDMA11, VIOC_RDMA12,

	/* VIDEO-IN */
	VIOC_VIN00,
	VIOC_VIN10,
	VIOC_VIN20, VIOC_VIN30,
#endif

	0x00 // just for final recognition
};

int VIOC_CONFIG_WMIXPath(unsigned int component_num, unsigned int wmixmode)
{
	/* mode - 0: BY-PSSS PATH, 1: WMIX PATH */
	u32 value;
	unsigned int i, shift_mix_path, shift_vin_rdma_path, support_bypass = 0U;
	void __iomem *config_reg = pIREQ_reg;
	int ret = -1;

	for (i = 0; i < (sizeof(bypassDMA) / sizeof(unsigned int)); i++) {
		if (component_num == bypassDMA[i]) {
			support_bypass = 1U;
			break;
		}
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	if (support_bypass == 1U) {
		shift_mix_path = 0xFFU; // ignore value
		shift_vin_rdma_path = 0xFFU;

		switch (get_vioc_type(component_num)) {
		case get_vioc_type(VIOC_RDMA):
			switch (get_vioc_index(component_num)) {
			case get_vioc_index(VIOC_RDMA00):
				shift_mix_path = CFG_MISC0_MIX00_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA03):
				shift_mix_path = CFG_MISC0_MIX03_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA04):
				shift_mix_path = CFG_MISC0_MIX10_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA07):
				shift_mix_path = CFG_MISC0_MIX13_SHIFT;
				break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			case get_vioc_index(VIOC_RDMA08):
				shift_mix_path = CFG_MISC0_MIX20_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA11):
				shift_mix_path = CFG_MISC0_MIX23_SHIFT;
				break;
#endif
			case get_vioc_index(VIOC_RDMA12):
				shift_mix_path = CFG_MISC0_MIX30_SHIFT;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
				shift_vin_rdma_path = CFG_MISC0_RD12_SHIFT;
#endif
				break;
			case get_vioc_index(VIOC_RDMA14):
				shift_mix_path = CFG_MISC0_MIX40_SHIFT;
				shift_vin_rdma_path = CFG_MISC0_RD14_SHIFT;
				break;
			default:
				(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
				__func__, get_vioc_type(component_num), get_vioc_index(component_num));
				break;
			}
			break;
		case get_vioc_type(VIOC_VIN):
			switch (get_vioc_index(component_num)) {
			case get_vioc_index(VIOC_VIN00):
				shift_mix_path = CFG_MISC0_MIX50_SHIFT;
				break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			case get_vioc_index(VIOC_VIN10):
				shift_mix_path = CFG_MISC0_MIX60_SHIFT;
				break;
			case get_vioc_index(VIOC_VIN20):
				shift_mix_path = CFG_MISC0_MIX30_SHIFT;
				shift_vin_rdma_path = CFG_MISC0_RD12_SHIFT;
				break;
#endif
			case get_vioc_index(VIOC_VIN30):
				shift_mix_path = CFG_MISC0_MIX40_SHIFT;
				shift_vin_rdma_path = CFG_MISC0_RD14_SHIFT;
				break;
			default:
				(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
				__func__, get_vioc_type(component_num), get_vioc_index(component_num));
				break;
			}
			break;
		default:
			(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
		       __func__, get_vioc_type(component_num), get_vioc_index(component_num));
			break;
		}

		if (shift_mix_path != 0xFFU) {
#if defined(CONFIG_ARCH_TCC805X)
			VIOC_CONFIG_WMIXPathReset(component_num, 1);
#endif
			value = __raw_readl(config_reg + CFG_MISC0_OFFSET)
				& ~((u32)1U << shift_mix_path);
			if (wmixmode != 0U) {
				/* Prevent KCS warning */
				value |= (u32)1U << shift_mix_path;
			}
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel(value, config_reg + CFG_MISC0_OFFSET);
#if defined(CONFIG_ARCH_TCC805X)
			VIOC_CONFIG_WMIXPathReset(component_num, 0);
#endif
		}

		if (shift_vin_rdma_path != 0xFFU) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			value = __raw_readl(config_reg + CFG_MISC0_OFFSET)
				& ~((u32)1U << shift_vin_rdma_path);

			if (get_vioc_type(component_num)
			    == get_vioc_type(VIOC_RDMA)) {
			    /* Prevent KCS warning */
				value |= (u32)0U << shift_vin_rdma_path;
			} else {
				/* Prevent KCS warning */
				value |= (u32)1U << shift_vin_rdma_path;
			}
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel(value, config_reg + CFG_MISC0_OFFSET);
		}

		ret = 0;
	} else {
		dprintk("%s: vioc(0x%x) doesn't support mixer bypass(%d) mode\n",
			__func__, component_num, support_bypass);

		ret = -1;
	}

	return ret;
}

int VIOC_CONFIG_GetWMIXPath(unsigned int component_num)
{
	/* mode - 0: BY-PSSS PATH, 1: WMIX PATH */
	int ret = -1;
	unsigned int i, shift_mix_path, mask_mix_path, support_bypass = 0U;
	void __iomem *config_reg = pIREQ_reg;

	for (i = 0; i < (sizeof(bypassDMA) / sizeof(unsigned int)); i++) {
		if (component_num == bypassDMA[i]) {
			support_bypass = 1U;
			break;
		}
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	if (support_bypass == 1U) {
		shift_mix_path = 0xFFU; // ignore value
		mask_mix_path = 0U;

		switch (get_vioc_type(component_num)) {
		case get_vioc_type(VIOC_RDMA):
			switch (get_vioc_index(component_num)) {
			case get_vioc_index(VIOC_RDMA00):
				shift_mix_path = CFG_MISC0_MIX00_SHIFT;
				mask_mix_path = CFG_MISC0_MIX00_MASK;
				break;
			case get_vioc_index(VIOC_RDMA03):
				shift_mix_path = CFG_MISC0_MIX03_SHIFT;
				mask_mix_path = CFG_MISC0_MIX03_MASK;
				break;
			case get_vioc_index(VIOC_RDMA04):
				shift_mix_path = CFG_MISC0_MIX10_SHIFT;
				mask_mix_path = CFG_MISC0_MIX10_MASK;
				break;
			case get_vioc_index(VIOC_RDMA07):
				shift_mix_path = CFG_MISC0_MIX13_SHIFT;
				mask_mix_path = CFG_MISC0_MIX13_MASK;
				break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			case get_vioc_index(VIOC_RDMA08):
				shift_mix_path = CFG_MISC0_MIX20_SHIFT;
				mask_mix_path = CFG_MISC0_MIX20_MASK;
				break;
			case get_vioc_index(VIOC_RDMA11):
				shift_mix_path = CFG_MISC0_MIX23_SHIFT;
				mask_mix_path = CFG_MISC0_MIX23_MASK;
				break;
#endif
			case get_vioc_index(VIOC_RDMA12):
				shift_mix_path = CFG_MISC0_MIX30_SHIFT;
				mask_mix_path = CFG_MISC0_MIX30_MASK;
				break;
			case get_vioc_index(VIOC_RDMA14):
				shift_mix_path = CFG_MISC0_MIX40_SHIFT;
				mask_mix_path = CFG_MISC0_MIX40_MASK;
				break;
			default:
				(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
				__func__, get_vioc_type(component_num), get_vioc_index(component_num));
				ret = -1;
				break;
			}
			break;
		case get_vioc_type(VIOC_VIN):
			switch (get_vioc_index(component_num)) {
			case get_vioc_index(VIOC_VIN00):
				shift_mix_path = CFG_MISC0_MIX50_SHIFT;
				mask_mix_path = CFG_MISC0_MIX50_MASK;
				break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			case get_vioc_index(VIOC_VIN10):
				shift_mix_path = CFG_MISC0_MIX60_SHIFT;
				mask_mix_path = CFG_MISC0_MIX60_MASK;
				break;
			case get_vioc_index(VIOC_VIN20):
				shift_mix_path = CFG_MISC0_MIX30_SHIFT;
				mask_mix_path = CFG_MISC0_MIX30_MASK;
				break;
#endif
			case get_vioc_index(VIOC_VIN30):
				shift_mix_path = CFG_MISC0_MIX40_SHIFT;
				mask_mix_path = CFG_MISC0_MIX40_MASK;
				break;
			default:
				(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
				__func__, get_vioc_type(component_num), get_vioc_index(component_num));
				ret = -1;
				break;
			}
			break;
		default:
			(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
		       __func__, get_vioc_type(component_num), get_vioc_index(component_num));
			ret = -1;
			break;
		}

		if (shift_mix_path != 0xFFU) {
			ret = (__raw_readl(config_reg + CFG_MISC0_OFFSET)
				& mask_mix_path) >> shift_mix_path;
		}
	} else {
		dprintk("%s: vioc(0x%x) doesn't support mixer bypass(%d) mode\n",
			__func__, component_num, support_bypass);
		ret = -1;
	}

	return ret;
}


#if defined(CONFIG_ARCH_TCC805X)
void VIOC_CONFIG_WMIXPathReset(unsigned int component_num, unsigned int reset)
{
	/* reset - 0 :Normal, 1:Mixing PATH reset */
	u32 value;
	unsigned int i, shift_mix_path, support_bypass = 0U;
	//unsigned int shift_vin_rdma_path;
	void __iomem *config_reg = pIREQ_reg;

	for (i = 0; i < (sizeof(bypassDMA) / sizeof(unsigned int)); i++) {
		if (component_num == bypassDMA[i]) {
			support_bypass = 1;
			break;
		}
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	if (support_bypass == 1U) {
		shift_mix_path = 0xFFU; // ignore value
		//shift_vin_rdma_path = 0xFF;

		switch (get_vioc_type(component_num)) {
		case get_vioc_type(VIOC_RDMA):
			switch (get_vioc_index(component_num)) {
			case get_vioc_index(VIOC_RDMA00):
				shift_mix_path = WMIX_PATH_SWR_MIX00_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA03):
				shift_mix_path = WMIX_PATH_SWR_MIX03_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA04):
				shift_mix_path = WMIX_PATH_SWR_MIX10_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA07):
				shift_mix_path = WMIX_PATH_SWR_MIX13_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA08):
				shift_mix_path = WMIX_PATH_SWR_MIX20_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA11):
				shift_mix_path = WMIX_PATH_SWR_MIX23_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA12):
				shift_mix_path = WMIX_PATH_SWR_MIX30_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA14):
				shift_mix_path = WMIX_PATH_SWR_MIX40_SHIFT;
				break;
			default:
				(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
				__func__, get_vioc_type(component_num), get_vioc_index(component_num));
				break;
			}
			break;
		case get_vioc_type(VIOC_VIN):
			switch (get_vioc_index(component_num)) {
			case get_vioc_index(VIOC_VIN00):
				shift_mix_path = WMIX_PATH_SWR_MIX50_SHIFT;
				break;
			case get_vioc_index(VIOC_VIN10):
				shift_mix_path = WMIX_PATH_SWR_MIX60_SHIFT;
				break;
			case get_vioc_index(VIOC_VIN20):
				shift_mix_path = WMIX_PATH_SWR_MIX30_SHIFT;
				break;
			case get_vioc_index(VIOC_VIN30):
				shift_mix_path = WMIX_PATH_SWR_MIX40_SHIFT;
				break;
			default:
				(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
				__func__, get_vioc_type(component_num), get_vioc_index(component_num));
				break;
			}
			break;
		default:
			(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
				__func__, get_vioc_type(component_num), get_vioc_index(component_num));
			break;
		}

		if (shift_mix_path != 0xFFU) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			value = __raw_readl(
					config_reg + CFG_WMIX_PATH_SWR_OFFSET)
					& ~((u32)1U << shift_mix_path);
			if (reset == 1U) {
				/* Prevent KCS warning */
				value |= (u32)1U << shift_mix_path;
			}
			__raw_writel(
				value, config_reg + CFG_WMIX_PATH_SWR_OFFSET);
		}
	}
}
#endif

#if defined(CONFIG_VIOC_AFBCDEC) || defined(CONFIG_VIOC_PVRIC_FBDC)
/* coverity[HIS_metric_violation : FALSE] */
int VIOC_CONFIG_FBCDECPath(
	unsigned int FBCDecPath, unsigned int rdmaPath, unsigned int on)
{
	u32 value = 0;
	int select = 0;
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	void __iomem *reg = (pIREQ_reg + CFG_FBC_DEC_SEL_OFFSET);
	int ret = (int)VIOC_PATH_DISCONNECTED;

	/* Check selection has type value. If has, select value is invalid */
	select = CheckFBCDecPathSelection(rdmaPath);
	if (select < 0) {
		ret = VIOC_DEVICE_INVALID;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	/*Check RDMA and AFBC_DEC operations.*/
	if (on == 1U) {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(reg)
			 & ~(((u32)0x1U << (unsigned int)select)
			     << CFG_FBC_DEC_SEL_AXSM_SEL_SHIFT));
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(reg)
			 | (((u32)0x1U << (unsigned int)select)
			    << CFG_FBC_DEC_SEL_AXSM_SEL_SHIFT));
	}

	switch (FBCDecPath) {
	case VIOC_FBCDEC0:
		if (on == 1U) {
			value &= ~CFG_FBC_DEC_SEL_AD_PATH_SEL00_MASK;
			value |=
				((unsigned int)select
				 << CFG_FBC_DEC_SEL_AD_PATH_SEL00_SHIFT);
		} else {
			/* Prevent KCS warning */
			value |= ((u32)0x1F << CFG_FBC_DEC_SEL_AD_PATH_SEL00_SHIFT);
		}
		break;
	case VIOC_FBCDEC1:
		if (on == 1U) {
			value &= ~CFG_FBC_DEC_SEL_AD_PATH_SEL01_MASK;
			value |=
				((unsigned int)select
				 << CFG_FBC_DEC_SEL_AD_PATH_SEL01_SHIFT);
		} else {
			/* Prevent KCS warning */
			value |= ((u32)0x1FU << CFG_FBC_DEC_SEL_AD_PATH_SEL01_SHIFT);
		}
		break;
	default:
		ret = VIOC_DEVICE_INVALID;
		break;
	};

	if (ret == VIOC_DEVICE_INVALID) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	// pr_debug("[DBG][VIOC_CONFIG] %s RDMA(%d) with FBC(%d) :: On(%d)-
	// 0x%08lx \n", __func__, get_vioc_index(rdmaPath),
	// get_vioc_index(FBCDecPath), on, value);
	__raw_writel(value, reg);
	ret = (int)VIOC_PATH_CONNECTED;

FUNC_EXIT:
	if (ret == VIOC_DEVICE_INVALID) {
		(void)pr_err("[ERR][VIOC_CONFIG] %s, in error, FBCDecPath(%d) :0x%x rdmaPath:0x%x cfg_reg:0x%x\n",
			__func__, on, get_vioc_index(FBCDecPath), rdmaPath,
			__raw_readl(reg));

		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		WARN_ON(1);
	}
	return ret;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_CONFIG_FBCDECPath);

int VIOC_CONFIG_GetFBDCPath(unsigned int fbcdec_id)
{
	int ret = -1;
	unsigned int mask;
	unsigned int shift;
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	const void __iomem *reg = (pIREQ_reg + CFG_FBC_DEC_SEL_OFFSET);

	switch (fbcdec_id) {
	case VIOC_FBCDEC0:
		mask = CFG_FBC_DEC_SEL_AD_PATH_SEL00_MASK;
		shift = CFG_FBC_DEC_SEL_AD_PATH_SEL00_SHIFT;
		ret = 0;
		break;
	case VIOC_FBCDEC1:
		mask = CFG_FBC_DEC_SEL_AD_PATH_SEL01_MASK;
		shift = CFG_FBC_DEC_SEL_AD_PATH_SEL01_SHIFT;
		ret = 0;
		break;
	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s, in error, FBCDEC_ID :0x%x \n",
			__func__, fbcdec_id);
		ret = -1;
		break;
	}

	if (ret == 0) {
		unsigned int temp = 0U; /* avoid MISRA C-2012 Rule 10.8 */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		temp = ((__raw_readl(reg) & mask) >> shift) + VIOC_RDMA00;
		ret = (int)temp;
	}
	return (int)ret;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_CONFIG_GetFBDCPath);
#endif

#ifdef CONFIG_VIOC_MAP_DECOMP
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
/*
 * VIOC_CONFIG_MCPath
 * Connect Mapconverter or RDMA block on component path
 * component : VIOC_RDMA03, VIOC_RDMA07, VIOC_RDMA11, VIOC_RDMA13, VIOC_RDMA15,
 * VIOC_RDMA16, VIOC_RDMA17
 * mc : VIOC_MC0, VIOC_MC1
 */
int VIOC_CONFIG_MCPath(unsigned int component, unsigned int mc)
{
	int ret = 0;
	u32 value;
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	void __iomem *reg = (pIREQ_reg + CFG_PATH_MC_OFFSET);

	if (CheckMCPathSelection(component, mc) < 0) {
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	switch (component) {
	case VIOC_WMIX0:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD03_MASK) != 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD03_MASK));
				value |= ((u32)0x1U << CFG_PATH_MC_RD03_SHIFT);
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD03_MASK) == 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD03_MASK));
				__raw_writel(value, reg);
			}
		}
		break;
	case VIOC_WMIX1:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD07_MASK) != 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD07_MASK));
				value |= ((u32)0x1U << CFG_PATH_MC_RD07_SHIFT);
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD07_MASK) == 0U)  {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD07_MASK));
				__raw_writel(value, reg);
			}
		}
		break;
	case VIOC_WMIX2:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD11_MASK) != 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD11_MASK));
				value |=
					(((u32)0x1U << CFG_PATH_MC_RD11_SHIFT)
					 | ((u32)0x1U << CFG_PATH_MC_MC1_SEL_SHIFT));
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD11_MASK) == 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD11_MASK));
				__raw_writel(value, reg);
			}
		}
		break;
	case VIOC_WMIX3:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD13_MASK) != 0U)  {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD13_MASK));
				value |=
					(((u32)0x1U << CFG_PATH_MC_RD13_SHIFT)
					 | ((u32)0x2U << CFG_PATH_MC_MC1_SEL_SHIFT));
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD13_MASK) == 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD13_MASK));
				__raw_writel(value, reg);
			}
		}
		break;
	case VIOC_WMIX4:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD15_MASK) != 0U)  {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD15_MASK));
				value |=
					(((u32)0x1U << CFG_PATH_MC_RD15_SHIFT)
					 | ((u32)0x3U << CFG_PATH_MC_MC1_SEL_SHIFT));
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD15_MASK) == 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD15_MASK));
				__raw_writel(value, reg);
			}
		}
		break;
	case VIOC_WMIX5:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD16_MASK) != 0U)  {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD16_MASK));
				value |=
					(((u32)0x1U << CFG_PATH_MC_RD16_SHIFT)
					 | ((u32)0x4U << CFG_PATH_MC_MC1_SEL_SHIFT));
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD16_MASK) == 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD16_MASK));
				__raw_writel(value, reg);
			}
		}
		break;
	case VIOC_WMIX6:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD17_MASK) != 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD17_MASK));
				value |=
					(((u32)0x1U << CFG_PATH_MC_RD17_SHIFT)
					 | ((u32)0x5U << CFG_PATH_MC_MC1_SEL_SHIFT));
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD17_MASK) == 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD17_MASK));
				__raw_writel(value, reg);
			}
		}
		break;
	default:
		ret = -1;
		break;
	}

FUNC_EXIT:
	if (ret < 0) {
		(void)pr_err("[ERR][VIOC_CONFIG] %s, in error, Path: 0x%x MC: %x cfg_reg:0x%x\n",
		       __func__, component, mc, __raw_readl(reg));
	}
	return ret;
}
#endif
#endif

void VIOC_CONFIG_SWReset(unsigned int component, unsigned int resetmode)
{
	u32 value;
	void __iomem *reg = pIREQ_reg;

	if ((resetmode != VIOC_CONFIG_RESET) && (resetmode != VIOC_CONFIG_CLEAR)) {
		(void)pr_err("[ERR][VIOC_CONFIG] %s, in error, invalid mode:%d\n",
		       __func__, resetmode);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_DISP):
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
			 & ~(PWR_BLK_SWR1_DEV_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR1_DEV_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;

	case get_vioc_type(VIOC_WMIX):
#if defined(CONFIG_ARCH_TCC805X)
		/* Read wmix mode (bypass or mixing) */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value = (__raw_readl(reg + CFG_MISC0_OFFSET)
			& ((u32)0x1U << (CFG_MISC0_MIX00_SHIFT
			+ (get_vioc_index(component) * 2U))));

		/* Mix Path reset */
		if (value != 0U) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			value = (__raw_readl(reg + PWR_BLK_SWR1_OFFSET) &
				~(PWR_BLK_SWR1_WMIX_MASK));
			value |= (resetmode << (PWR_BLK_SWR1_WMIX_SHIFT +
				get_vioc_index(component)));
			__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
			break;
		}

		/* Bypass Path reset */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value = (__raw_readl(reg + CFG_WMIX_PATH_SWR_OFFSET) &
			~((u32)0x1U << (get_vioc_index(component) * 2U)));
		value |= (resetmode << (get_vioc_index(component) * 2U));
		__raw_writel(value, (reg + CFG_WMIX_PATH_SWR_OFFSET));
		break;
#else
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value = (__raw_readl(reg + PWR_BLK_SWR1_OFFSET) &
			~(PWR_BLK_SWR1_WMIX_MASK));
		value |= (resetmode << (PWR_BLK_SWR1_WMIX_SHIFT +
			get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;
#endif
	case get_vioc_type(VIOC_WDMA):
#if !defined(CONFIG_ARCH_TCC803X) && !defined(CONFIG_ARCH_TCC805X)
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
			 & ~(PWR_BLK_SWR1_WDMA_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR1_WDMA_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
#else /* CONFIG_ARCH_TCC803X || CONFIG_ARCH_TCC805X */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				 reg
				 + ((component > VIOC_WDMA08) ?
					    PWR_BLK_SWR4_OFFSET :
					    PWR_BLK_SWR1_OFFSET))
			 & ~((component > VIOC_WDMA08) ?
				      (u32)PWR_BLK_SWR4_WD_MASK :
				      (u32)PWR_BLK_SWR1_WDMA_MASK));
		value |=
			(resetmode
			 << ((component > VIOC_WDMA08) ? (PWR_BLK_SWR4_WD_SHIFT
					     + (get_vioc_index(
						       component
						       - VIOC_WDMA09))) :
						       (PWR_BLK_SWR1_WDMA_SHIFT
					     + get_vioc_index(component))));
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			value,
			reg
				+ ((component > VIOC_WDMA08) ?
					   PWR_BLK_SWR4_OFFSET :
					   PWR_BLK_SWR1_OFFSET));
#endif
		break;

	case get_vioc_type(VIOC_FIFO):
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
			 & ~(PWR_BLK_SWR1_FIFO_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR1_FIFO_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;

	case get_vioc_type(VIOC_RDMA):
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(reg + PWR_BLK_SWR0_OFFSET)
			 & ~(PWR_BLK_SWR0_RDMA_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR0_RDMA_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR0_OFFSET));
		break;

	case get_vioc_type(VIOC_SCALER):
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(reg + PWR_BLK_SWR3_OFFSET)
			 & ~(PWR_BLK_SWR3_SC_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR3_SC_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR3_OFFSET));
#endif
#if defined(CONFIG_ARCH_TCC897X)
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(reg + PWR_BLK_SWR0_OFFSET)
			 & ~(PWR_BLK_SWR0_SC_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR0_SC_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR0_OFFSET));
#endif
		break;

	case get_vioc_type(VIOC_VIQE):
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if (get_vioc_index(component) == 0U) {
			value =
				(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
				 & ~(PWR_BLK_SWR1_VIQE0_MASK));
			value |= (resetmode << PWR_BLK_SWR1_VIQE0_SHIFT);
			__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		} else if (get_vioc_index(component) == 1U) {
			value =
				(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
				 & ~(PWR_BLK_SWR1_VIQE1_MASK));
			value |= (resetmode << PWR_BLK_SWR1_VIQE1_SHIFT);
			__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
#endif
		} else {
			/* Prevent KCS warning */
		}
		break;

	case get_vioc_type(VIOC_VIN):
#if defined(CONFIG_ARCH_TCC897X)
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(reg + PWR_BLK_SWR0_OFFSET)
			 & ~(PWR_BLK_SWR0_VIN_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR0_VIN_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR0_OFFSET));
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(
				 reg
				 + ((component > VIOC_VIN30) ?
					    PWR_BLK_SWR4_OFFSET :
					    PWR_BLK_SWR0_OFFSET))
			 & ~((component > VIOC_VIN30) ? PWR_BLK_SWR4_VIN_MASK :
						      PWR_BLK_SWR0_VIN_MASK));
		value |=
			(resetmode
			 << ((component > VIOC_VIN30) ? (PWR_BLK_SWR4_VIN_SHIFT
					     + (get_vioc_index(
							component - VIOC_VIN40)
						/ 2U)) :
						     (PWR_BLK_SWR0_VIN_SHIFT
					     + (get_vioc_index(component)
						/ 2U))));
		__raw_writel(
			value,
			reg
				+ ((component > VIOC_VIN30) ?
					   PWR_BLK_SWR4_OFFSET :
					   PWR_BLK_SWR0_OFFSET));
#endif
		break;

#ifdef CONFIG_VIOC_MAP_DECOMP
	case get_vioc_type(VIOC_MC):
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
			 & ~(PWR_BLK_SWR1_MC_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR1_MC_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;
#endif

	case get_vioc_type(VIOC_DEINTLS):
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
			 & ~(PWR_BLK_SWR1_DINTS_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR1_DINTS_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;

#if defined(CONFIG_VIOC_AFBCDEC) || defined(CONFIG_VIOC_PVRIC_FBDC)
	case get_vioc_type(VIOC_FBCDEC):
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
#if defined(CONFIG_ARCH_TCC803X)
		value =
			(__raw_readl(reg + PWR_BLK_SWR2_OFFSET)
			 & ~(PWR_BLK_SWR2_AD_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR2_AD_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR2_OFFSET));
#else /* CONFIG_ARCH_TCC805X */
		value = (__raw_readl(reg + CFG_WMIX_PATH_SWR_OFFSET) &
			 ~(WMIX_PATH_SWR_PF_MASK));
		value |= (resetmode << (WMIX_PATH_SWR_PF_SHIFT +
				   get_vioc_index(component)));
		__raw_writel(value, (reg + CFG_WMIX_PATH_SWR_OFFSET));

#endif
		break;
#endif

#ifdef CONFIG_VIOC_PIXEL_MAPPER
	case get_vioc_type(VIOC_PIXELMAP):
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(reg + PWR_BLK_SWR2_OFFSET)
			 & ~(PWR_BLK_SWR2_PM_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR2_PM_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, reg + PWR_BLK_SWR2_OFFSET);
		break;
#endif

#ifdef CONFIG_TCC_DV_IN
	case get_vioc_type(VIOC_DV_IN):
		value =
			(__raw_readl(reg + PWR_BLK_SWR2_OFFSET)
			 & ~(PWR_BLK_SWR2_DV_IN_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR2_DV_IN_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, reg + PWR_BLK_SWR2_OFFSET);
		break;
#endif

	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s, wrong component(0x%08x)\n",
		       __func__, component);

		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		WARN_ON(1);
		break;
	}

FUNC_EXIT:
	return;
}
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_CONFIG_SWReset);

/*
 * VIOC_CONFIG_Device_PlugState
 * Check PlugInOut status of VIOC SCALER, VIQE, DEINTLS.
 * component : VIOC_SC0, VIOC_SC1, VIOC_SC2, VIOC_VIQE, VIOC_DEINTLS
 * pDstatus : Pointer of status value.
 * return value : Device name of Plug in.
 */
int VIOC_CONFIG_Device_PlugState(
	unsigned int component, VIOC_PlugInOutCheck *VIOC_PlugIn)
{
	//	u32 value;
	const void __iomem *reg = NULL;
	int ret = -1;

	reg = CalcAddressViocComponent(component);
	if (reg == NULL) {
		VIOC_PlugIn->enable = 0;
		ret = VIOC_DEVICE_INVALID;
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		VIOC_PlugIn->enable =
			(__raw_readl(reg) & CFG_PATH_EN_MASK) >> CFG_PATH_EN_SHIFT;
		VIOC_PlugIn->connect_device =
			(__raw_readl(reg) & CFG_PATH_SEL_MASK) >> CFG_PATH_SEL_SHIFT;
		VIOC_PlugIn->connect_statue =
			(__raw_readl(reg) & CFG_PATH_STS_MASK) >> CFG_PATH_STS_SHIFT;
		ret = VIOC_DEVICE_CONNECTED;
	}
	return ret;
}

static unsigned int CalcPathSelectionInScaler(unsigned int RdmaNum)
{
	unsigned int ret;

	/* In our register, RDMA16/17 offsets are diffrent. */
	if (RdmaNum == VIOC_RDMA16) {
		/* prevent KCS warning */
		ret = VIOC_SC_RDMA_16;
#if !defined(CONFIG_ARCH_TCC897X)
	} else if (RdmaNum == VIOC_RDMA17) {
		/* prevent KCS warning */
		ret = VIOC_SC_RDMA_17;
#endif
	} else {
		/* prevent KCS warning */
		ret = get_vioc_index(RdmaNum);
	}

	return ret;
}

static unsigned int CalcPathSelectionInViqeDeinter(unsigned int RdmaNum)
{
	unsigned int ret;

	/* In our register, RDMA16/17 offsets are diffrent. */
	if (RdmaNum == VIOC_RDMA16) {
		/* prevent KCS warning */
		ret = VIOC_VIQE_RDMA_16;
#if !defined(CONFIG_ARCH_TCC897X)
	} else if (RdmaNum == VIOC_RDMA17) {
		/* prevent KCS warning */
		ret = VIOC_VIQE_RDMA_17;
#endif
	} else {
		/* prevent KCS warning */
		ret = get_vioc_index(RdmaNum);
	}

	return ret;
}

/* coverity[HIS_metric_violation : FALSE] */
int VIOC_CONFIG_GetScaler_PluginToRDMA(unsigned int RdmaNum)
{
	unsigned int i;
	unsigned int rdma_idx;
	VIOC_PlugInOutCheck VIOC_PlugIn;
	int ret = -1;

	rdma_idx = CalcPathSelectionInScaler(RdmaNum);

	for (i = get_vioc_index(VIOC_SCALER0); i <= VIOC_SCALER_MAX; i++) {
		if (VIOC_CONFIG_Device_PlugState(
			    (VIOC_SCALER0 + i), &VIOC_PlugIn)
		    == VIOC_DEVICE_INVALID) {
			/* prevent KCS warning */
			continue;
		}

		if ((VIOC_PlugIn.enable == 1U)
		    && (VIOC_PlugIn.connect_device == rdma_idx))  {
		    ret = ((int)VIOC_SCALER0 + (int)i);
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto FUNC_EXIT;
		}
	}
	ret = -1;

FUNC_EXIT:
	return ret;
}

int VIOC_CONFIG_GetScaler_PluginToWDMA(unsigned int WdmaNum)
{
	unsigned int i;
	VIOC_PlugInOutCheck VIOC_PlugIn;
	int ret = -1;

	for (i = get_vioc_index(VIOC_SCALER0); i < VIOC_SCALER_MAX; i++) {
		if (VIOC_CONFIG_Device_PlugState(
				(VIOC_SCALER0 + i), &VIOC_PlugIn)
				== VIOC_DEVICE_INVALID) {
				/* prevent KCS warning */
			continue;
		}

		if ((VIOC_PlugIn.enable == 0U) || VIOC_PlugIn.connect_device < 0x14) {
			//disabled or connected device is not WDMA
			continue;
		}
#if defined(CONFIG_ARCH_TCC805X)
		if (VIOC_PlugIn.connect_device == 0x20U &&
		    WdmaNum == VIOC_WDMA13) {
			ret = ((int)VIOC_SCALER0 + (int)i);
			break;
		}
#endif
		if ((VIOC_PlugIn.connect_device - 0x14U) == get_vioc_index(WdmaNum)) {
			ret = ((int)VIOC_SCALER0 + (int)i);
			break;
		}
	}
	return ret;
}

/* coverity[HIS_metric_violation : FALSE] */
int VIOC_CONFIG_GetViqeDeintls_PluginToRDMA(unsigned int RdmaNum)
{
	unsigned int i;
	unsigned int rdma_idx;
	VIOC_PlugInOutCheck VIOC_PlugIn;
	int ret = -1;

	rdma_idx = CalcPathSelectionInViqeDeinter(RdmaNum);

	for (i = get_vioc_index(VIOC_VIQE0); i <= VIOC_VIQE_MAX; i++) {
		if (VIOC_CONFIG_Device_PlugState((VIOC_VIQE0 + i), &VIOC_PlugIn)
		    == VIOC_DEVICE_INVALID) {
			/* prevent KCS warning */
			continue;
		}

		if ((VIOC_PlugIn.enable == 1U)
		    && (VIOC_PlugIn.connect_device == rdma_idx)) {
			ret = ((int)VIOC_VIQE0 + (int)i);
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto FUNC_EXIT;
		}
	}

	for (i = get_vioc_index(VIOC_DEINTLS0); i <= VIOC_DEINTLS_MAX; i++) {
		if (VIOC_CONFIG_Device_PlugState(
			    (VIOC_DEINTLS0 + i), &VIOC_PlugIn)
		    == VIOC_DEVICE_INVALID) {
			/* prevent KCS warning */
			continue;
		}

		if ((VIOC_PlugIn.enable == 1U)
		    && (VIOC_PlugIn.connect_device == rdma_idx)) {
			ret = ((int)VIOC_DEINTLS0 + (int)i);
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto FUNC_EXIT;
		}
	}
	ret = -1;

FUNC_EXIT:
	return ret;
}

int VIOC_CONFIG_GetRdma_PluginToComponent(
	unsigned int ComponentNum /*Viqe, Mc, Dtrc*/)
{
	VIOC_PlugInOutCheck VIOC_PlugIn;
	int ret = -1;

	if (VIOC_CONFIG_Device_PlugState(ComponentNum, &VIOC_PlugIn)
	    == VIOC_DEVICE_INVALID) {
		/* prevent KCS warning */
		ret = -1;
	} else {
		if (VIOC_PlugIn.enable != 0U) {
			/* prevent KCS warning */
			if (VIOC_PlugIn.connect_device < (UINT_MAX / 2U)) {/* avoid CERT-C Integers Rule INT31-C */
				ret = ((int)VIOC_RDMA + (int)VIOC_PlugIn.connect_device);
			}
		} else {
			/* prevent KCS warning */
			ret = -1;
		}
	}

	return ret;
}

void VIOC_CONFIG_StopRequest(unsigned int en)
{
	u32 value;
	void __iomem *reg = pIREQ_reg;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + CFG_MISC1_OFFSET) & ~(CFG_MISC1_S_REQ_MASK));

	if (en != 0U) {
		/* prevent KCS warning */
		value |= ((u32)0x0U << CFG_MISC1_S_REQ_SHIFT);
	} else {
		/* prevent KCS warning */
		value |= ((u32)0x1U << CFG_MISC1_S_REQ_SHIFT);
	}

	__raw_writel(value, reg + CFG_MISC1_OFFSET);
}

/* coverity[HIS_metric_violation : FALSE] */
int VIOC_CONFIG_DMAPath_Select(unsigned int DMApath)
{
	unsigned int i;
	u32 value;
	const void __iomem *reg = (void __iomem *)CalcAddressViocComponent(DMApath);
	int ret = -1;

	if (reg == NULL) {
		dprintk("%s-%d :: INFO :: path(0x%x) is not configurable\n",
			__func__, __LINE__, DMApath);
		if (DMApath < (UINT_MAX / 2U)) { /* avoid CERT-C Integers Rule INT31-C */
			ret = (int)DMApath;
		} else {
			ret = -1;
		}
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	// check vrdma path
	for (i = 0U; i < VIOC_RDMA_MAX; i++) {
		reg = (void __iomem *)CalcAddressViocComponent(VIOC_RDMA00 + i);

		if (reg == NULL) {
			/* Prevent KCS warning */
			continue;
		}

		value = __raw_readl(reg);
		if (((value & CFG_PATH_EN_MASK) != 0U)
		    && ((value & CFG_PATH_SEL_MASK) == (get_vioc_index(DMApath)))) {
			ret = ((int)VIOC_RDMA00 + (int)i);
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto FUNC_EXIT;
		}
	}

#ifdef CONFIG_VIOC_MAP_DECOMP
	// check map converter
	for (i = 0U; i < VIOC_MC_MAX; i++) {
		reg = (void __iomem *)CalcAddressViocComponent(VIOC_MC0 + i);
		if (reg == NULL) {
			/* Prevent KCS warning */
			continue;
		}

		value = __raw_readl(reg);

		if (((value & CFG_PATH_EN_MASK) != 0U)
		    && ((value & CFG_PATH_SEL_MASK) == (get_vioc_index(DMApath)))) {
			ret = ((int)VIOC_MC0 + (int)i);
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto FUNC_EXIT;
		}
	}
#endif

	dprintk("%s-%d :: Info path(0x%x) doesn't have plugged-in component\n",
		__func__, __LINE__, DMApath);
	ret = -1;

FUNC_EXIT:
	return ret;
}

int VIOC_CONFIG_DMAPath_Set(unsigned int DMApath, unsigned int dma)
{
	int loop = 0;
	u32 value = 0x0U;
	unsigned int path_sel = 0;
	unsigned int en, sel, status;
	void __iomem *cfg_path_reg;
	int ret = -1;

	cfg_path_reg = CalcAddressViocComponent(dma);
	if (cfg_path_reg == NULL) {
		dprintk("%s-%d :: ERR :: cfg_path_reg for dma(0x%x) is NULL\n",
			__func__, __LINE__, dma);
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	switch (get_vioc_type(dma)) {
	case get_vioc_type(VIOC_RDMA):
		ret = 0;
		break;
#ifdef CONFIG_VIOC_MAP_DECOMP
	case get_vioc_type(VIOC_MC): {
		// Disalbe Auto power-ctrl
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(pIREQ_reg + PWR_AUTOPD_OFFSET)
			 & ~(PWR_AUTOPD_MC_MASK));
		value |= ((u32)0x0U << PWR_AUTOPD_MC_SHIFT);
		__raw_writel(value, pIREQ_reg + PWR_AUTOPD_OFFSET);
		ret = 0;
	} break;
#endif
	default:
		(void)pr_info("[INF][VIOC_CONFIG] %s-%d :: ERROR :: cfg_path_reg for dma(0x%x) doesn't support\n",
			__func__, __LINE__, dma);
		ret = -1;
		break;
	}
	if (ret < 0) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	// check path status.
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = __raw_readl(cfg_path_reg);

	en = ((value & CFG_PATH_EN_MASK) != 0U) ? 1U : 0U;
	status = (value & CFG_PATH_STS_MASK) >> CFG_PATH_STS_SHIFT;
	sel = (value & CFG_PATH_SEL_MASK) >> CFG_PATH_SEL_SHIFT;

	// path selection.
	if (get_vioc_type(DMApath) == get_vioc_type(VIOC_RDMA)) {
		/* Prevent KCS warning */
		path_sel = get_vioc_index(DMApath);
		ret = 0;
	} else {
		(void)pr_info("[INF][VIOC_CONFIG] %s-%d :: ERROR :: path(0x%x) is very wierd.\n",
			__func__, __LINE__, DMApath);
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	if ((en == 1U) && (sel != path_sel)) {
		loop = 50;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(value & (~CFG_PATH_EN_MASK), cfg_path_reg);
		while (loop-- != 0) {
			value = __raw_readl(cfg_path_reg);
			status = (value & CFG_PATH_STS_MASK)
				>> CFG_PATH_STS_SHIFT;
			if (status == VIOC_PATH_DISCONNECTED) {
				ret = 0;
				break;
			}
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			mdelay(1);
		}

		if (loop <= 0) {
			ret = -1;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto FUNC_EXIT;
		}
	}

	loop = 50;
	dprintk("W:: %s CFG_RDMA: 0x%x\n", __func__,
		(CFG_PATH_EN_MASK
		 | ((path_sel << CFG_PATH_SEL_SHIFT) & CFG_PATH_SEL_MASK)));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(
		CFG_PATH_EN_MASK
			| ((path_sel << CFG_PATH_SEL_SHIFT)
			   & CFG_PATH_SEL_MASK),
		cfg_path_reg);
	while (loop-- != 0) {
		value = __raw_readl(cfg_path_reg);
		status = (value & CFG_PATH_STS_MASK) >> CFG_PATH_STS_SHIFT;
		if (status != VIOC_PATH_DISCONNECTED) {
			ret = 0;
			break;
		}
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		mdelay(1);
	}

	if (loop <= 0) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		ret = -1;
		goto FUNC_EXIT;
	}

	dprintk("R:: %s CFG_RDMA= 0x%x\n", __func__,
		__raw_readl(cfg_path_reg));

	ret = 0;

FUNC_EXIT:
	if (ret < 0) {
		/* coverity[cert_dcl37_c_violation : FALSE] */
		(void)pr_err("[ERR][VIOC_CONFIG] vioc config plug in error : setting path : 0x%x dma:%x cfg_path_reg:0x%x before registe : 0x%08x\n",
		       DMApath, dma, __raw_readl(cfg_path_reg), value);
	}
	return ret;
}

/* coverity[HIS_metric_violation : FALSE] */
int VIOC_CONFIG_DMAPath_UnSet(int dma)
{
	unsigned int en = 0;
	int loop = 0;
	u32 value = 0;
	void __iomem *cfg_path_reg;
	int ret = -1;

	if (dma < 0) {
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	// select config register.
	cfg_path_reg = CalcAddressViocComponent((unsigned int)dma);
	if (cfg_path_reg == NULL) {
		(void)pr_info("[INF][VIOC_CONFIG] %s-%d :: ERROR :: cfg_path_reg for dma(0x%x) is NULL\n",
			__func__, __LINE__, dma);
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	switch (get_vioc_type((unsigned int)dma)) {
	case get_vioc_type(VIOC_RDMA):
		ret = 0;
		break;
#ifdef CONFIG_VIOC_MAP_DECOMP
	case get_vioc_type(VIOC_MC):
		ret = 0;
		break;
#endif
	default:
		(void)pr_info("[INF][VIOC_CONFIG] %s-%d :: ERROR :: cfg_path_reg for dma(0x%x) doesn't support\n",
			__func__, __LINE__, dma);
		ret = -1;
		break;
	}
	if (ret < 0) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	// check path status.
	value = __raw_readl(cfg_path_reg);
	en = ((value & CFG_PATH_EN_MASK) != 0U) ? 1U : 0U;

	if (en == 1U) {
		// disable dma
		loop = 50;
		__raw_writel(value & (~CFG_PATH_EN_MASK), cfg_path_reg);

		// wait dma disconnected status.
		while (loop-- != 0) {
			if (((__raw_readl(cfg_path_reg) & CFG_PATH_STS_MASK)
			     >> CFG_PATH_STS_SHIFT)
			    == VIOC_PATH_DISCONNECTED) {
			    ret = 0;
				break;
			}
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			mdelay(1);
		}

		if (loop <= 0) {
			ret = -1;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto FUNC_EXIT;
		}
	}

	ret = 0;
FUNC_EXIT:
	if (ret < 0) {
		/* coverity[cert_dcl37_c_violation : FALSE] */
		(void)pr_err("[ERR][VIOC_CONFIG] %s  in error : setting  dma:%x cfg_path_cfg_path_reg:0x%x  before cfg_path_registe : 0x%08x\n",
		       __func__, dma, __raw_readl(cfg_path_reg), value);
	}
	return ret;
}

int VIOC_CONFIG_DMAPath_Support(void)
{
	// TCC897X, TCC802X, TCC570X, TCC803X
	return 0;
}

void VIOC_CONFIG_DMAPath_Iint(void)
{
	unsigned int i;
	const void __iomem *cfg_path_reg;

	if (VIOC_CONFIG_DMAPath_Support() != 0) {
		for (i = 0U; i < VIOC_RDMA_MAX; i++) {
			cfg_path_reg = (void __iomem *)CalcAddressViocComponent(
				VIOC_RDMA00 + i);

			if (cfg_path_reg == NULL) {
				/* Prevent KCS warning */
				continue;
			}
			(void)VIOC_CONFIG_DMAPath_Set(
				VIOC_RDMA00 + i, VIOC_RDMA00 + i);
		}
	}
}

/* coverity[HIS_metric_violation : FALSE] */
int VIOC_CONFIG_LCDPath_Select(unsigned int lcdx_sel, unsigned int lcdx_if)
{
	int ret = 0;
	unsigned int i, cnt;
	unsigned int lcdx_bak;
	uint32_t val;
#if defined(CONFIG_ARCH_TCC805X)
	unsigned int sel_lcd[4];

	cnt = 3;
#else
	unsigned int sel_lcd[3];

	cnt = 2;
#endif

#if defined(CONFIG_ARCH_TCC805X)
	if ((lcdx_sel > 3U) || (lcdx_if > 3U)) {
		ret = -EINVAL;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
#else
	if ((lcdx_sel > 2U) || (lcdx_if > 2U)) {
		ret = -EINVAL;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
#endif

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = __raw_readl(pIREQ_reg + CFG_MISC1_OFFSET);
	sel_lcd[0] =
		(val & CFG_MISC1_LCD0_SEL_MASK) >> CFG_MISC1_LCD0_SEL_SHIFT;
	sel_lcd[1] =
		(val & CFG_MISC1_LCD1_SEL_MASK) >> CFG_MISC1_LCD1_SEL_SHIFT;
	sel_lcd[2] =
		(val & CFG_MISC1_LCD2_SEL_MASK) >> CFG_MISC1_LCD2_SEL_SHIFT;
#if defined(CONFIG_ARCH_TCC805X)
	sel_lcd[3] =
		(val & CFG_MISC1_LCD3_SEL_MASK) >> CFG_MISC1_LCD3_SEL_SHIFT;
#endif

	if (sel_lcd[lcdx_if] != lcdx_sel) {
		lcdx_bak = sel_lcd[lcdx_if];
		sel_lcd[lcdx_if] = lcdx_sel;
	} else {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	for (i = 0; i <= cnt; i++) {
		if ((i != lcdx_if) && (sel_lcd[i] == lcdx_sel)) {
			sel_lcd[i] = lcdx_bak;
			break;
		}
	}
#if defined(CONFIG_ARCH_TCC805X)
	val &=
		~(CFG_MISC1_LCD3_SEL_MASK | CFG_MISC1_LCD2_SEL_MASK
		  | CFG_MISC1_LCD1_SEL_MASK | CFG_MISC1_LCD0_SEL_MASK);
	val |= ((sel_lcd[3] << CFG_MISC1_LCD3_SEL_SHIFT)
		| (sel_lcd[2] << CFG_MISC1_LCD2_SEL_SHIFT)
		| (sel_lcd[1] << CFG_MISC1_LCD1_SEL_SHIFT)
		| (sel_lcd[0] << CFG_MISC1_LCD0_SEL_SHIFT));
#else
	val &=
		~(CFG_MISC1_LCD2_SEL_MASK | CFG_MISC1_LCD1_SEL_MASK
		  | CFG_MISC1_LCD0_SEL_MASK);
	val |= ((sel_lcd[2] << CFG_MISC1_LCD2_SEL_SHIFT)
		| (sel_lcd[1] << CFG_MISC1_LCD1_SEL_SHIFT)
		| (sel_lcd[0] << CFG_MISC1_LCD0_SEL_SHIFT));
#endif
	__raw_writel(val, pIREQ_reg + CFG_MISC1_OFFSET);

	dprintk("%s, CFG_MISC1: 0x%08x\n", __func__,
		__raw_readl(pIREQ_reg + CFG_MISC1_OFFSET));

FUNC_EXIT:
	return ret;
}

void __iomem *VIOC_IREQConfig_GetAddress(void)
{
	if (pIREQ_reg == NULL) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][VIOC_CONFIG] %s VIOC_IREQConfig\n", __func__);
	}

	return pIREQ_reg;
}

void VIOC_IREQConfig_DUMP(unsigned int offset, unsigned int size)
{
	unsigned int cnt = 0;

	if (pIREQ_reg == NULL) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][VIOC_CONFIG] %s, VIOC_IREQConfig\n", __func__);
	} else {
		dprintk(
			"[DBG][VIOC_CONFIG] VIOC_IREQConfig ::\n");
		while (cnt < size) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			dprintk(
				"[DBG][VIOC_CONFIG] VIOC_IREQConfig + offset(%x) + 0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				offset, cnt,
				__raw_readl(pIREQ_reg + offset + cnt),
				__raw_readl(pIREQ_reg + offset + cnt + 0x4U),
				__raw_readl(pIREQ_reg + offset + cnt + 0x8U),
				__raw_readl(pIREQ_reg + offset + cnt + 0xCU));
			cnt += 0x10U;
		}
	}
}

static int __init vioc_config_init(void)
{
	struct device_node *ViocConfig_np;

	ViocConfig_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_config");

	if (ViocConfig_np == NULL) {
		(void)pr_info("[INF][VIOC_CONFIG] disabled [this is mandatory for vioc display]\n");
	} else {
		pIREQ_reg = of_iomap(ViocConfig_np, (is_VIOC_REMAP != 0U) ? 1 : 0);

		if (pIREQ_reg != NULL) {
			(void)pr_info("[INF][VIOC_CONFIG] CONFIG\n");
			VIOC_CONFIG_DMAPath_Iint();
		}
	}
	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_config_init);

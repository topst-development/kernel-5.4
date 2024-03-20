// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/limits.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/cpu_cooling.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include "thermal_common.h"
#include "../thermal_core.h"

#define CS_POLICY_CORE          (0)
#define CLUST0_POLICY_CORE      (0)
#define CLUST1_POLICY_CORE      (2)

#define TCC_ZONE_COUNT          (1)
#define TCC_THERMAL_COUNT       (4)

#define THERMAL_MODE_CONT       (0x3)
#define THERMAL_MODE_ONESHOT    (0x1)
#define THERMAL_MIN_DATA        ((u8)0x00)
#define THERMAL_MAX_DATA        ((u8)0xFF)

#define PASSIVE_INTERVAL        (0)
#define ACTIVE_INTERVAL         (300)
#define IDLE_INTERVAL           (60000)
#define MCELSIUS                (1000)

#define MIN_TEMP                (15)
#define MAX_TEMP                (125)
#define MIN_TEMP_CODE           (0x00011111)
#define MAX_TEMP_CODE           (0x10010010)

#if defined (CONFIG_ARCH_TCC897X)
#define ECID_CONF		(0x74200290)
#else
#define ECID_CONF		(0x14200290)
#endif
#define ECID_USER_REG0		(0x8U)
#define ECID_USER_REG1		(0xCU)
#define TEMP_CODE		(0x30U)
#define SLOPE_SEL		(0xCU)
#define VREF_SEL		(0x10U)

#define TYPE_ONE_POINT_TRIMMING	0
#define TYPE_TWO_POINT_TRIMMING	1
#define TYPE_NONE		2

static int32_t code_to_temp(const struct tcc_thermal_platform_data *pdata,
		int32_t temp_trim1, int32_t temp_code)
{
        int32_t temp;

        switch (pdata->calib_sel) {
        case TYPE_ONE_POINT_TRIMMING:
                temp = (int32_t)(temp_code - temp_trim1) + 25;
                break;
        case TYPE_NONE:
                temp = (int32_t)temp_code - 21;
                break;
        default:
                temp = (int32_t)(temp_code - temp_trim1) + 25;
                break;
        }

        return temp;
}

static int32_t tccxxxx_thermal_read(struct tcc_thermal_platform_data *pdata)
{
	int32_t celsius_temp = 0;
	uint32_t udigit = 0;
	int32_t digit = 0;

	udigit = readl_relaxed(pdata->base + TEMP_CODE);

	if (udigit < 0x7FFFFFFFU) {
		digit = (int32_t)udigit;
	} else {
		digit = __INT_MAX__;
	}

	celsius_temp = code_to_temp(pdata, pdata->temp_trim1[0], digit);

	return celsius_temp;
}


int32_t tccxxxx_get_temp(void *tz, int32_t *temp)
{
	struct tcc_thermal_platform_data *pdata = tz;

	*temp = tccxxxx_thermal_read(pdata);
	if (*temp < (__INT_MAX__ / MCELSIUS)) {
		*temp = *temp * MCELSIUS;
	}

	return 0;
}

int32_t tccxxxx_get_trend(void *tz, int32_t trip, enum thermal_trend *trend)
{
	struct tcc_thermal_platform_data *pdata = tz;
	int32_t cur_trip_temp;

	if (pdata->tz == NULL) {
		*trend = THERMAL_TREND_STABLE;
	} else {
		(void)pdata->tz->ops->get_trip_temp(pdata->tz,
						trip, &cur_trip_temp);

		if (pdata->tz->temperature > cur_trip_temp) {
			*trend = THERMAL_TREND_RAISING;
		} else if (pdata->tz->temperature < cur_trip_temp) {
			*trend = THERMAL_TREND_DROPPING;
		} else {
			*trend = THERMAL_TREND_STABLE;
		}
	}

	return 0;
}

static int32_t tccxxxx_thermal_init(const struct tcc_thermal_platform_data *data)
{
	u32 v_temp;

	v_temp = readl_relaxed(data->base);

	v_temp |= 0x3U;

	writel(v_temp, data->base);

	v_temp = data->buf_vref_sel_ts;
	if (v_temp) {
		writel(v_temp, data->base + VREF_SEL);
	}
	v_temp = data->buf_slope_sel_ts;
	if (v_temp) {
		writel(v_temp, data->base + SLOPE_SEL);
	}

	return 0;
}

static int32_t tccxxxx_get_efuse(const struct platform_device *pdev)
{
	struct tcc_thermal_platform_data *data = platform_get_drvdata(pdev);
	uint32_t reg_temp = 0;
	uint32_t ecid_reg1_temp;
	uint32_t ecid_reg0_temp;
	int32_t i;
	void __iomem *ecid_conf;

	ecid_conf = ioremap(ECID_CONF, 0x10);
	// Read ECID - USER0
	reg_temp |= (1 << 31);
	writel(reg_temp, ecid_conf); // enable
	reg_temp |= (1 << 30);
	writel(reg_temp, ecid_conf); // CS:1, Sel : 0

	for (i = 0 ; i < 8; i++) {
		reg_temp &= ~(0x3F<<17);
		writel(reg_temp, ecid_conf);
		reg_temp |= (i<<17);
		writel(reg_temp, ecid_conf);
		reg_temp |= (1<<23);
		writel(reg_temp, ecid_conf);
		reg_temp |= (1<<27);
		writel(reg_temp, ecid_conf);
		reg_temp |= (1<<29);
		writel(reg_temp, ecid_conf);
		reg_temp &= ~(1<<23);
		writel(reg_temp, ecid_conf);
		reg_temp &= ~(1<<27);
		writel(reg_temp, ecid_conf);
		reg_temp &= ~(1<<29);
		writel(reg_temp, ecid_conf);
	}

	reg_temp &= ~(1 << 30);
	writel(reg_temp, ecid_conf);
	data->temp_trim1[0] = (readl(ecid_conf + ECID_USER_REG1) &
			0x0000FF00) >> 8;
#if defined(CONFIG_ARCH_TCC803X) && defined(CONFIG_ARCH_TCC899X)
	data->buf_slope_sel_ts = (readl(ecid_conf + ECID_USER_REG1) &
			0x000000F0) >> 4;
	data->buf_vref_sel_ts =
		((readl(ecid_conf + ECID_USER_REG1) &
		  0x0000000F) << 1) |
		((readl(ecid_conf + ECID_USER_REG0) &
		  0x80000000) >> 31);
#endif
	ecid_reg1_temp = (readl(ecid_conf + ECID_USER_REG1));
	ecid_reg0_temp = (readl(ecid_conf + ECID_USER_REG0));
	reg_temp &= ~(1 << 31);
	writel(reg_temp, ecid_conf);
	(void)pr_info("%s.--ecid register read --\n", __func__);
	(void)pr_info("%s.ecid reg1 : %08x\n", __func__, ecid_reg1_temp);
	(void)pr_info("%s.ecid reg0 : %08x\n", __func__, ecid_reg0_temp);
	(void)pr_info("%s.data->temp_trim1 : %08x\n", __func__,
			data->temp_trim1[0]);
	(void)pr_info("%s.data->buf_vref_sel_ts1 : %08x\n", __func__,
			data->buf_vref_sel_ts);
	(void)pr_info("%s.data->slope_trim1 : %08x\n", __func__,
			data->buf_slope_sel_ts);

	// ~Read ECID - USER0

	(void)pr_info("[INFO][TSENSOR] %s. trim_val: %08x\n",
			__func__, data->temp_trim1[0]);
	(void)pr_info("[INFO][TSENSOR] %s. cal_type: %d\n",
			__func__, data->calib_sel);

	iounmap(ecid_conf);

	return 0;
}

int32_t tccxxxx_parse_dt(const struct platform_device *pdev,
		struct tcc_thermal_platform_data *pdata)
{
	struct device_node *np;
	const char *tmp_str;
	int32_t ret = 0;

	if (pdev->dev.of_node != NULL) {
		np = pdev->dev.of_node;
	} else {
		(void)pr_err(
				"[ERROR][TSENSOR]%s: failed to get device node\n",
				__func__);
		ret = -ENODEV;
		goto retval;
	}

	/*tcc803x, tcc802x, tcc897x, tcc899x has only one porbe for t-sensor*/
        pdata->core = 0;

	ret = of_property_read_string(np, "cal_type", &tmp_str);
	if (ret != 0) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s:failed to get cal_type from dt\n",
				__func__);
	}

	ret = strncmp(tmp_str, "TYPE_ONE_POINT_TRIMMING", strnlen(tmp_str, 30));
	if (ret == 0) {
		pdata->calib_sel = TYPE_ONE_POINT_TRIMMING;
	} else {
		ret = strncmp(tmp_str, "TYPE_TWO_POINT_TRIMMING",
				strnlen(tmp_str, 30));
		if (ret == 0) {
			/**/
			pdata->calib_sel = TYPE_TWO_POINT_TRIMMING;
		} else {
			/**/
			pdata->calib_sel = TYPE_NONE;
		}
	}

	ret = of_property_read_u32(np, "threshold_temp",
			&pdata->threshold_high_temp);
	if (ret != 0) {
		(void)pr_err("%s:failed to get threshold_temp\n", __func__);
		pdata->threshold_high_temp = MAX_TEMP_CODE;
	}
retval:
	return ret;
}
static struct thermal_zone_of_device_ops tccxxxx_dev_ops = {
	.get_temp = tccxxxx_get_temp,
	.get_trend = tccxxxx_get_trend,
};
const struct thermal_ops tccxxxx_ops = {
	.init			= tccxxxx_thermal_init,
	.parse_dt		= tccxxxx_parse_dt,
	.get_fuse_data		= tccxxxx_get_efuse,
	.t_ops			= &tccxxxx_dev_ops,
};

struct tcc_thermal_platform_data tccxxxx_pdata = {
	.name		= "tccxxxx",
	.ops		= &tccxxxx_ops,
};

const struct tcc_thermal_data tccxxxx_data = {
	.irq		= 40,
	.pdata		= &tccxxxx_pdata,
};
//dummy
const struct tcc_thermal_data tcc805x_data = {
	.irq		= 40,
	.pdata		= NULL,
};
MODULE_AUTHOR("jay.kim@telechips.com");
MODULE_DESCRIPTION("Telechips thermal driver");
MODULE_LICENSE("GPL");

/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_THERMAL_H
#define TCC_THERMAL_H


#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/cpu_cooling.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>

struct tcc_thermal_data {
	struct tcc_thermal_platform_data *pdata;
	int32_t irq;
};

struct thermal_ops {
	int32_t (*init)(const struct tcc_thermal_platform_data *pdata);
	int32_t (*parse_dt)(const struct platform_device *pdev,
			struct tcc_thermal_platform_data *pdata);
	int32_t (*get_fuse_data)(const struct platform_device *pdev);
	struct thermal_zone_of_device_ops *t_ops;
};

struct tcc_thermal_platform_data {
	char name[16];
	struct device *dev;
	bool init;
	struct thermal_zone_device *tz;
	struct resource *res;
	struct mutex lock;
	void __iomem *base;
	const struct thermal_ops *ops;
	struct thermal_zone_params *tcc_gov;
	int32_t core;
	int32_t threshold_low_temp;
	int32_t threshold_high_temp;
	uint32_t interval_time;
	int32_t probe_num;
	int32_t temp_trim1[6];
	int32_t temp_trim2[6];
	int32_t ts_test_info_high;
	int32_t ts_test_info_low;
	int32_t buf_slope_sel_ts;
	int32_t d_otp_slope;
	int32_t calib_sel;
	int32_t buf_vref_sel_ts;
	int32_t trim_bgr_ts;
	int32_t trim_vlsb_ts;
	int32_t trim_bjt_cur_ts;
	uint32_t delay_passive;
	uint32_t delay_idle;
	uint32_t status;
};

extern const struct tcc_thermal_data tcc805x_data;
extern const struct tcc_thermal_data tccxxxx_data;

#if defined(CONFIG_ARCH_TCC805X)
#if defined (CONFIG_TCC_GPU_THERMAL_SUPPORT)
extern const struct tcc_thermal_data tcc805x_gpu_data;
#endif
int32_t tcc805x_parse_dt(const struct platform_device *pdev,
                struct tcc_thermal_platform_data *pdata);
int32_t tcc805x_get_fuse_data(const struct platform_device *pdev);

int32_t tcc805x_get_temp(void *tz, int32_t *temp);
int32_t tcc805x_get_trend(void *tz, int32_t trip, enum thermal_trend *trend);
#else
int32_t tccxxxx_parse_dt(const struct platform_device *pdev,
                struct tcc_thermal_platform_data *pdata);
int32_t tccxxxx_get_fuse_data(struct platform_device *pdev);

int32_t tccxxxx_get_temp(void *tz, int32_t *temp);
int32_t tccxxxx_get_trend(void *tz, int32_t trip, enum thermal_trend *trend);
#endif
#endif

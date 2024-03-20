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
#include <linux/arm-smccc.h>
#include <soc/tcc/tcc-sip.h>
#include <linux/soc/telechips/tcc_sc_protocol.h>
#include "thermal_common.h"
#include "../thermal_core.h"

#define CS_POLICY_CORE          (0)

#define TCC_ZONE_COUNT          (1)
#define TCC_THERMAL_COUNT       (4)

#define PASSIVE_INTERVAL        (1000)
#define ACTIVE_INTERVAL         (1000)
#define IDLE_INTERVAL           (60000)
#define MCELSIUS                (1000)

#define THRESHOLD_MIN_TEMP      (-40000)
#define THRESHOLD_MAX_TEMP      (125000)
#define MIN_TEMP_CODE           ((int32_t)0x248)
#define MAX_TEMP_CODE           ((int32_t)0xC7C)
#define CAPTURE_IRQ             (0x111111UL)
#define HIGH_TEMP_IRQ           (0x222222UL)
#define LOW_TEMP_IRQ            (0x444444UL)
#define CA53_IRQ_EN             (0x6UL)
#define CA72_IRQ_EN             (0x60UL)
#define DEFAULT_INTERVAL	(262143)

//Register
#define CA53_PROBE		(0xD0)
#define CA72_PROBE0		(0xD4)
#define CA72_PROBE1		(0xD8)
#define CA72_PROBE2		(0xDC)
#define CA72_PROBE3		(0xE0)
#define GPU_PROBE		(0xE4)
#define THRESHOLD_UP		(0x50U)
#define THRESHOLD_DOWN		(0x54U)
#define T_EN_REG		(0x0U)
#define CONTROL_REG		(0x4U)
#define PROBE_SEL		(0xCU)
#define TB			(0x10U)
#define TV			(0x14U)
#define TBC			(0x18U)
#define BSS			(0x1CU)
#define BVS			(0x20U)
#define IRQ_INTV		(0x8U)
#define INT_EN			(0x24U)
#define INT_MASK		(0x2CU)
#define INT_STATUS		(0x34U)
#define INT_CLEAR		(0x44U)
#define T_TIME_INTV		(0x154U)
#define DEBUG

static const struct tcc_sc_fw_handle *sc_fw_handle_for_otp;

// decimal value => register value
static int32_t temp_to_code(const struct tcc_thermal_platform_data *pdata,
		int32_t temp_trim1, int32_t temp_trim2, int32_t temp)
{
	/*Always apply two point calibration*/
	int32_t temp_code;
	int64_t l_pre_fuse;
	int32_t pre_fuse;
	int64_t res;
	int64_t l_trim_val;
	int32_t trim_val;

	l_pre_fuse = (int64_t)pdata->ts_test_info_high -
			(int64_t)pdata->ts_test_info_low;

	if ((l_pre_fuse > __INT_MAX__) || (l_pre_fuse < INT_MIN)) {
		pre_fuse = 6000;
	} else {
		pre_fuse = (int32_t)l_pre_fuse;
	}

	l_trim_val = ((int64_t)temp_trim2 - (int64_t)temp_trim1);
	if ((l_trim_val > __INT_MAX__) || (l_trim_val < INT_MIN)) {
		trim_val = 956;
	} else {
		trim_val = (int32_t)l_trim_val;
	}

	if ((pdata->calib_sel == 0) ||
			(pdata->calib_sel == 1)) {
		if (temp >= 25) {
			temp_code = temp - 25;
			temp_code = temp_code * 16;
		} else {
			temp_code = (temp - 25);
			temp_code = temp_code *	(57 + pdata->d_otp_slope);
			temp_code = temp_code / 65;
			temp_code = temp_code * 16;
		}
	} else {
		if (temp >= 25) {
			temp_code = temp - 25;
			temp_code = temp_code * trim_val;
			temp_code = temp_code / (pre_fuse / 100);
		} else {
			temp_code = (temp - 25);
			temp_code = temp_code *	(57 + pdata->d_otp_slope);
			temp_code = temp_code / 65;
			temp_code = temp_code * trim_val;
			temp_code = temp_code / (pre_fuse / 100);
		}
	}

	res = ((int64_t)temp_code + (int64_t)temp_trim1);
	if ((res > __INT_MAX__) || (res < INT_MIN)) {
		temp_code = 2500;
	} else {
		temp_code = (int32_t)res;
	}

	return temp_code;
}

// register value > decimal value
static int32_t code_to_temp(const struct tcc_thermal_platform_data *pdata,
		int32_t temp_trim1, int32_t temp_trim2, int32_t temp_code)
{
	/*Always apply two point calibration*/
	int32_t temp;
	int64_t code;
	int64_t l_d_otp_temp;
	int32_t d_otp_temp;
	int64_t l_pre_fuse;
	int32_t pre_fuse;

	code = (int64_t)temp_code - (int64_t)temp_trim1;
	if ((code > __INT_MAX__) || (code < INT_MIN)) {
		temp = 200;
	} else {
		temp = (int32_t)code;
	}
	l_d_otp_temp = (int64_t)temp_trim2 - (int64_t)temp_trim1;
	if ((l_d_otp_temp > __INT_MAX__) || (l_d_otp_temp < INT_MIN)) {
		d_otp_temp = 956;
	} else {
		d_otp_temp = (int32_t)l_d_otp_temp;
	}

	l_pre_fuse = (int64_t)pdata->ts_test_info_high -
			(int64_t)pdata->ts_test_info_low;

	if ((l_pre_fuse > __INT_MAX__) || (l_pre_fuse < INT_MIN)) {
		pre_fuse = 6000;
	} else {
		pre_fuse = (int32_t)l_pre_fuse;
	}

	if ((pdata->calib_sel == 1) || (pdata->calib_sel == 0)) {
		if (temp_code >= temp_trim1) {
			temp = temp * 625;
			temp = temp / 100;
		} else {
			temp = temp * 625;
			temp = temp / 100;
			temp = temp * (6500 / (57 + pdata->d_otp_slope));
			temp = temp / 100;
		}
	} else {
		if (temp_code >= temp_trim1) {
			temp = temp  * pre_fuse;
			temp = temp / d_otp_temp;
		} else {
			temp = temp  * pre_fuse;
			temp = temp / d_otp_temp;
			temp = temp * (6500 / (57 + pdata->d_otp_slope));
			temp = temp / 100;
		}
	}

	if (temp < (__INT_MAX__ - 2500)) {
		temp = temp + 2500;
	}

	return temp;
}

static uint32_t request_otp_to_sc(u32 offset)
{
	int32_t temp = -EINVAL;
	uint32_t ret = 0U;
	struct tcc_sc_fw_tfuse_cmd tfuse_cmd;

	if (sc_fw_handle_for_otp != NULL) {
		temp = sc_fw_handle_for_otp
			->ops.tfuse_ops->get_tfuse
			(sc_fw_handle_for_otp, &tfuse_cmd, offset);
	}

	if ((sc_fw_handle_for_otp != NULL)
			&& (sc_fw_handle_for_otp->version.minor > 0U)
			&& (sc_fw_handle_for_otp->version.patch > 26U)) {
		if ((temp == 0) && (tfuse_cmd.resp[2] == 1U)) {
			ret = tfuse_cmd.resp[1];
		} else {
			(void)pr_info("[request OTP] Cannot get otp value");
			ret = 0U;
		}
	} else {
		if (temp == 0) {
			ret = tfuse_cmd.resp[1];
		} else {
			(void)pr_info("[request OTP] Cannot get otp value");
			ret = 0U;
		}
	}

	return ret;
}

static void thermal_otp_read(struct tcc_thermal_platform_data *data,
		int32_t probe)
{
	uint32_t ret;

	switch (probe) {
	case 0:
		ret = request_otp_to_sc(0x3210U);
		data->temp_trim1[0] = (int32_t)((uint16_t)(ret & 0xFFFU));
		data->temp_trim2[0] =
				(int32_t)((uint16_t)((ret >> 16U) & 0xFFFU));
		break;
	case 1:
		ret = request_otp_to_sc(0x3214U);
		data->temp_trim1[1] = (int32_t)((uint16_t)(ret & 0xFFFU));
		data->temp_trim2[1] =
				(int32_t)((uint16_t)((ret >> 16U) & 0xFFFU));
		break;
	case 2:
		ret = request_otp_to_sc(0x3218U);
		data->temp_trim1[2] = (int32_t)((uint16_t)(ret & 0xFFFU));
		data->temp_trim2[2] =
				(int32_t)((uint16_t)((ret >> 16U) & 0xFFFU));
		break;
	case 3:
		ret = request_otp_to_sc(0x321CU);
		data->temp_trim1[3] = (int32_t)((uint16_t)(ret & 0xFFFU));
		data->temp_trim2[3] =
				(int32_t)((uint16_t)((ret >> 16U) & 0xFFFU));
		break;
	case 4:
		ret = request_otp_to_sc(0x3220U);
		data->temp_trim1[4] = (int32_t)((uint16_t)(ret & 0xFFFU));
		data->temp_trim2[4] =
				(int32_t)((uint16_t)((ret >> 16U) & 0xFFFU));
		break;
	case 5:
		ret = request_otp_to_sc(0x3224U);
		data->temp_trim1[5] = (int32_t)((uint16_t)(ret & 0xFFFU));
		data->temp_trim2[5] =
				(int32_t)((uint16_t)((ret >> 16U) & 0xFFFU));
		break;
	case 6:
		ret = request_otp_to_sc(0x3228U);
		data->ts_test_info_high =
				(int32_t)((uint16_t)((ret & 0xFFF000U) >> 12U));

		data->ts_test_info_high =
			(int32_t)(uint16_t)
				(((((uint32_t)data->ts_test_info_high &
						0xFF0U) >> 4U) * 100U) +
			(((uint32_t)data->ts_test_info_high & 0xFU) * 10U));

		data->ts_test_info_low = (int32_t)((uint16_t)(ret & 0xFFFU));
		data->ts_test_info_low =
			(int32_t)(uint16_t)
				(((((uint32_t)data->ts_test_info_low &
						0xFF0U) >> 4U) * 100U) +
			(((uint32_t)data->ts_test_info_low & 0xFU) * 10U));
		break;
	case 7:
		ret = request_otp_to_sc(0x322CU);
		data->buf_slope_sel_ts  =
				(int32_t)(uint16_t)((ret >> 28U) & 0xFU);
		data->d_otp_slope	=
				(int32_t)(uint16_t)((ret >> 24U) & 0xFU);
		data->calib_sel		=
				(int32_t)(uint16_t)((ret >> 14U) & 0x3U);
		data->buf_vref_sel_ts	=
				(int32_t)(uint16_t)((ret >> 12U) & 0x3U);
		data->trim_bgr_ts	=
				(int32_t)(uint16_t)((ret >> 8U) & 0xFU);
		data->trim_vlsb_ts	=
				(int32_t)(uint16_t)((ret >> 4U) & 0xFU);
		data->trim_bjt_cur_ts	=
				(int32_t)(uint16_t)(ret & 0xFU);
		break;
	default:
		for (ret = 0U; ret < 6U; ret++) {
			data->temp_trim1[ret] = 1596;
			data->temp_trim2[ret] = 2552;
		}
		data->calib_sel = 2;
		data->d_otp_slope = 6;
		data->ts_test_info_high = 10500;
		data->ts_test_info_low = 2500;
		break;
	}
}


static void tcc805x_otp_data_check(struct tcc_thermal_platform_data *data)
{
	int32_t i;

	for (i = 0; i < 6; i++) {
		if (data->temp_trim1[i] == 0) {
			data->temp_trim1[i] = 1596;
		}

		if (data->temp_trim2[i] == 0) {
			data->temp_trim2[i] = 2552;
		}
	}
	if (data->calib_sel > 4) {
		data->calib_sel = 2;
	}

	if (data->d_otp_slope == 0) {
		data->d_otp_slope = 6;
	}

	if (data->ts_test_info_high == 0) {
		data->ts_test_info_high = 10500;
	}

	if (data->ts_test_info_low == 0) {
		data->ts_test_info_low = 2500;
	}
	if (data->core == 1) {
		(void)pr_info("[INFO][TSENSOR] %s. main_temp_trim1: %d\n",
				__func__, data->temp_trim1[0]);
		(void)pr_info("[INFO][TSENSOR] %s. main_temp_trim2: %d\n",
				__func__, data->temp_trim2[0]);

		for (i = 1; i < 6; i++) {
			(void)pr_info(
				"[INFO][TSENSOR] %s. probe%d_temp_trim1: %d\n",
					__func__, i, data->temp_trim1[i]);
			(void)pr_info(
				"[INFO][TSENSOR] %s. probe%d_temp_trim2: %d\n",
					__func__, i, data->temp_trim2[i]);
		}
		(void)pr_info("[INFO][TSENSOR] %s. cal_type: %d\n",
				__func__, data->calib_sel);
		(void)pr_info("[INFO][TSENSOR] %s. ts_test_info_high: %d\n",
				__func__, data->ts_test_info_high);
		(void)pr_info("[INFO][TSENSOR] %s. ts_test_info_low: %d\n",
				__func__, data->ts_test_info_low);
	}

}


static int32_t tcc805x_get_otp(const struct platform_device *pdev)
{
	struct tcc_thermal_platform_data *pdata = platform_get_drvdata(pdev);
	int32_t ret = 0;
	int32_t i;

	if (ret != 0) {
		(void)pr_err(
				"[TSENSOR]%s:Fail to read Thermal Data from OTP ROM!!",
				__func__);
	}

	if (pdata != NULL) {
		mutex_lock(&pdata->lock);

		for (i = 0; i < 8; i++) {
			thermal_otp_read(pdata, i);
		}
		tcc805x_otp_data_check(pdata);

		mutex_unlock(&pdata->lock);
	} else {
		(void)pr_err(
				"[TSENSOR] %s: otp data read error(NULL pointer)!!\n",
				__func__);
		ret = -EINVAL;
	}
	return ret;
}
int32_t tcc805x_get_fuse_data(const struct platform_device *pdev)
{
	struct tcc_thermal_platform_data *data = platform_get_drvdata(pdev);
	struct device_node *fw_np;
	int ret;

	fw_np = of_parse_phandle(pdev->dev.of_node, "sc-firmware", 0);
	if (fw_np == NULL) {
		dev_err(&(pdev->dev),
				"[ERROR][TSENSOR] fw_np == NULL\n");
		tcc805x_otp_data_check(data);
		ret = -EINVAL;
	}

	sc_fw_handle_for_otp = tcc_sc_fw_get_handle(fw_np);
	if (sc_fw_handle_for_otp == NULL) {
		dev_err(&(pdev->dev),
				"[ERROR][TSENSOR] sc_fw_handle == NULL\n");
		tcc805x_otp_data_check(data);
		ret = -EINVAL;
	}

	if ((sc_fw_handle_for_otp->version.major == 0U)
			&& (sc_fw_handle_for_otp->version.minor == 0U)
			&& (sc_fw_handle_for_otp->version.patch < 21U)) {
		dev_err(&(pdev->dev),
				"[ERROR][TSENSOR] The version of SCFW is low. So, register cannot be set through SCFW.\n"
		       );
		dev_err(&(pdev->dev),
				"[ERROR][TSENSOR] SCFW Version : %d.%d.%d\n",
				sc_fw_handle_for_otp->version.major,
				sc_fw_handle_for_otp->version.minor,
				sc_fw_handle_for_otp->version.patch);
		tcc805x_otp_data_check(data);
		ret = -EINVAL;
	} else {
		(void)pr_info("[INFO][TSENSOR] SCFW Version : %d.%d.%d\n",
				sc_fw_handle_for_otp->version.major,
				sc_fw_handle_for_otp->version.minor,
				sc_fw_handle_for_otp->version.patch);
		ret = tcc805x_get_otp(pdev);
	}
	return ret;
}

static int32_t tcc805x_thermal_read(
			const struct tcc_thermal_platform_data *pdata)
{
	int32_t celsius_temp = 0;
	int32_t i = 0;
	uint32_t udigit = 0;
	int32_t digit = 0;

	if (pdata->probe_num >= 0xD0) {
		i = ((pdata->probe_num - 0xD0) / 4);
	} else {
		i = 0;
	}

	udigit = readl_relaxed(pdata->base + pdata->probe_num);
	if (udigit < 0x7FFFFFFFU) {
		digit = (int32_t)udigit;
	} else {
		digit = __INT_MAX__;
	}

	celsius_temp = code_to_temp(pdata,
			pdata->temp_trim1[i],
			pdata->temp_trim2[i],
			digit);

	return celsius_temp;
}


int32_t tcc805x_get_temp(void *tz, int32_t *temp)
{
	const struct tcc_thermal_platform_data *pdata = tz;

	*temp = tcc805x_thermal_read(pdata);
	if (*temp < (__INT_MAX__ / 10)) {
		*temp = *temp * 10;
	}

	return 0;
}

int32_t tcc805x_get_trend(void *tz, int32_t trip, enum thermal_trend *trend)
{
	const struct tcc_thermal_platform_data *pdata = tz;
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

static int32_t tcc805x_thermal_init(
			const struct tcc_thermal_platform_data *data)
{
	u32 v_temp;
	int32_t threshold_low_temp[6];
	int32_t threshold_high_temp[6];
	int32_t ret = 0;
	uint32_t i;

	if (data == NULL) {
		(void)pr_err("[TSENSOR] %s: tcc_thermal_data error!!\n",
				__func__);
		ret = -EINVAL;
	}

	if ((data != NULL) && (data->init)) {
		if (data->base != NULL) {
			writel(0U, data->base + T_EN_REG);
			writel(0x3FU, data->base + PROBE_SEL);
			if (data->trim_bgr_ts >= 0) {
				writel((uint32_t)data->trim_bgr_ts,
							data->base + TB);
			}
			if (data->trim_vlsb_ts >= 0) {
				writel((uint32_t)data->trim_vlsb_ts,
							data->base + TV);
			}
			if (data->trim_bjt_cur_ts >= 0) {
				writel((uint32_t)data->trim_bjt_cur_ts,
							data->base + TBC);
			}
			if (data->buf_slope_sel_ts >= 0) {
				writel((uint32_t)data->buf_slope_sel_ts,
							data->base + BSS);
			}
			if (data->buf_vref_sel_ts >= 0) {
				writel((uint32_t)data->buf_vref_sel_ts,
							data->base + BVS);
			}
		}
		for (i = 0U; i < 6U; i++) {
			threshold_high_temp[i] = temp_to_code(data,
					data->temp_trim1[i],
					data->temp_trim2[i],
					data->threshold_high_temp);
			if (threshold_high_temp[i] >= 0) {
				writel((uint32_t)threshold_high_temp[i],
						data->base +
						THRESHOLD_UP + (i*IRQ_INTV));
			} else {
				writel(2552, data->base +
					THRESHOLD_UP + (i*IRQ_INTV));
			}
		}

		for (i = 0U; i < 6U; i++) {
			threshold_low_temp[i] = temp_to_code(data,
					data->temp_trim1[i],
					data->temp_trim2[i],
					data->threshold_low_temp);
			if (threshold_low_temp[i] >= 0) {
				writel((uint32_t)threshold_low_temp[i],
						data->base +
						THRESHOLD_DOWN + (i*IRQ_INTV));
			} else {
				writel(1596, data->base +
					THRESHOLD_DOWN + (i*IRQ_INTV));
			}
		}

		(void)pr_info("%s.thermal maincore threshold high temp: %d\n",
				__func__, threshold_high_temp[1]);
		(void)pr_info("%s.thermal maincore threshold low temp: %d\n",
				__func__, threshold_low_temp[1]);
		(void)pr_info("%s.thermal subcore threshold high temp: %d\n",
				__func__, threshold_high_temp[0]);
		(void)pr_info("%s.thermal subcore threshold low temp: %d\n",
				__func__, threshold_low_temp[0]);
		writel(0, data->base + INT_EN); // Default interrupt disable

		writel(data->interval_time, data->base + T_TIME_INTV);
		writel((CAPTURE_IRQ | HIGH_TEMP_IRQ | LOW_TEMP_IRQ),
				data->base + INT_CLEAR);
		writel(((~(CA53_IRQ_EN | CA72_IRQ_EN)) &
					(LOW_TEMP_IRQ | HIGH_TEMP_IRQ)),
				data->base + INT_MASK);
		writel((HIGH_TEMP_IRQ | LOW_TEMP_IRQ),
				data->base + INT_EN);
		v_temp = readl_relaxed(data->base + CONTROL_REG);
		v_temp = (v_temp & 0x1EU);
		v_temp |= (uint32_t)0x1; // Default mode : Continuous mode
		v_temp |= ((uint32_t)0x1 << 5); // interval time enable

		writel(v_temp, data->base + CONTROL_REG);
		writel(1, data->base + T_EN_REG);
	} else {
		(void)pr_info("[TSENSOR] Not set configuration register\n");
	}

	return ret;
}

int32_t tcc805x_parse_dt(const struct platform_device *pdev,
		struct tcc_thermal_platform_data *pdata)
{
	const struct device_node *np;
	const char *tmp_str;
	int32_t ret = 0;

	mutex_init(&pdata->lock);
	if (pdev->dev.of_node != NULL) {
		np = pdev->dev.of_node;
	} else {
		(void)pr_err(
				"[ERROR][TSENSOR]%s: failed to get device node\n",
				__func__);
		ret = -ENODEV;
		goto retval;
	}
	if (pdata == NULL) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s: failed to get platform data\n",
				__func__);
		ret = -EINVAL;
		goto retval;
	}

	ret = of_property_read_string(np, "init", &tmp_str);
	if (ret != 0) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s:failed to get init from dt\n",
				__func__);
		tmp_str = "false";
	}

	ret = strncmp(tmp_str, "okay", strnlen(tmp_str, 30));
	if (ret == 0) {
		pdata->init = (bool)true;
	} else {
		pdata->init = (bool)false;
	}

	ret = of_property_read_s32(np, "threshold_low_temp",
			&pdata->threshold_low_temp);
	if (ret != 0) {
		(void)pr_err(
				"%s:failed to get threshold_low_temp\n",
				__func__);
		pdata->threshold_low_temp = THRESHOLD_MIN_TEMP;
	}

	ret = of_property_read_s32(np, "threshold_high_temp",
			&pdata->threshold_high_temp);
	if (ret != 0) {
		(void)pr_err("%s:failed to get threshold_high_temp\n",
				__func__);
		pdata->threshold_high_temp = THRESHOLD_MAX_TEMP;
	}

	ret = of_property_read_s32(np, "core", &pdata->core);
	if (ret != 0) {
		(void)pr_err("%s:failed to get thermal probe number\n",
				__func__);
		pdata->core = 0;
		pdata->probe_num = CA53_PROBE;
	} else {
		pdata->probe_num = (pdata->core * 4) + CA53_PROBE;
	}

	ret = of_property_read_u32(np, "interval_time",
			&pdata->interval_time);
	if (ret != 0) {
		(void)pr_err(
				"%s:failed to get interval_time\n",
				__func__);
		pdata->interval_time = DEFAULT_INTERVAL;
	}

retval:
	return ret;
}
static struct thermal_zone_of_device_ops tcc805x_dev_ops = {
	.get_temp = tcc805x_get_temp,
	.get_trend = tcc805x_get_trend,
};
static const struct thermal_ops tcc805x_ops = {
	.init			= tcc805x_thermal_init,
	.parse_dt		= tcc805x_parse_dt,
	.get_fuse_data		= tcc805x_get_fuse_data,
	.t_ops			= &tcc805x_dev_ops,
};

static struct tcc_thermal_platform_data tcc805x_pdata = {
	.name		= "tcc805x",
	.ops		= &tcc805x_ops,
};

const struct tcc_thermal_data tcc805x_data = {
	.irq		= 40,
	.pdata		= &tcc805x_pdata,
};

//dummy
const struct tcc_thermal_data tccxxxx_data = {
	.irq            = 40,
	.pdata          = NULL,
};

MODULE_AUTHOR("jay.kim@telechips.com");
MODULE_DESCRIPTION("Telechips thermal driver");
MODULE_LICENSE("GPL");

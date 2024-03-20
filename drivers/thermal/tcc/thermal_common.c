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
#include "thermal_common.h"
#include "../thermal_core.h"

#define CS_POLICY_CORE          (0)

#define TCC_ZONE_COUNT          (1)
#define TCC_THERMAL_COUNT       (4)

#define DEBUG

static void tcc_unregister_thermal(const struct tcc_thermal_platform_data *pdata);
static int32_t tcc_register_thermal(struct tcc_thermal_platform_data *pdata);

static const struct of_device_id tcc_thermal_id_table[] = {
	{
		.compatible = "telechips,tcc805x",
		.data = &tcc805x_data,
	},
	{
		.compatible = "telechips,tcc-thermal",
		.data = &tccxxxx_data,
	},
#if defined (CONFIG_TCC_GPU_THERMAL_SUPPORT)
	{
		.compatible = "telechips,tcc805x-gpu",
		.data = &tcc805x_gpu_data,
	},
#endif
	{}
};
MODULE_DEVICE_TABLE(of, tcc_thermal_id_table);

static int32_t tcc_register_thermal(struct tcc_thermal_platform_data *pdata)
{
	int32_t ret = 0;
	long err = 0;
	if (pdata == NULL) {
		(void)pr_err(
			"[ERROR][TSENSOR] %s: Temperature sensor not initialised\n",
			__func__);
		ret = -EINVAL;
		goto retval;
	}

	pdata->tz = devm_thermal_zone_of_sensor_register(pdata->dev,
							pdata->core,
							pdata,
							pdata->ops->t_ops);

	ret = (int32_t)IS_ERR(pdata->tz);
	if (ret != 0) {
		(void)pr_err(
			"[ERROR][TSENSOR] %s: Failed to register thermal zone device\n",
								__func__);
		err = PTR_ERR(pdata->tz);
		if ((err > __INT_MAX__) || (err < INT_MIN)) {
			ret = -EINVAL;
		} else {
			ret = (int32_t)err;
		}
		goto err_unregister;
	}

	if (pdata->tcc_gov != NULL) {
		ret = thermal_zone_device_set_policy(pdata->tz,
					pdata->tcc_gov->governor_name);
		if (ret != 0) {
			(void)pr_info("[INFO][TSENSOR] failed to set policy");
		} else {
			(void)pr_info(
				"[INFO][TSENSOR] TSENSOR policy is set to %s",
				pdata->tcc_gov->governor_name);
		}
	}

	(void)pr_info(
		"[INFO][TSENSOR] %s: Kernel Thermal management registered\n",
							__func__);
	goto retval;
err_unregister:
	tcc_unregister_thermal(pdata);
retval:
	return ret;
}

static void tcc_unregister_thermal(
			const struct tcc_thermal_platform_data *pdata)
{
	if (pdata->tz != NULL)
		devm_thermal_zone_of_sensor_unregister(pdata->dev, pdata->tz);
	(void)pr_info(
			"[INFO][TSENSOR] %s: TCC: Kernel Thermal management unregistered\n",
								__func__);
}

static int32_t tcc_thermal_probe(struct platform_device *pdev)
{
	const struct tcc_thermal_data *data;
	struct tcc_thermal_platform_data *pdata = NULL;
	struct device_node *use_dt;
	const struct of_device_id *id;
	struct device *dev;
	int32_t ret = 0;

	if (pdev == NULL) {
		(void)pr_err("[ERROR][TSENSOR]%s: No platform device.\n",
				__func__);
		goto error_eno;
	}

	if (pdev->dev.of_node != NULL) {
		dev = &pdev->dev;
	} else {
		dev = pdev->dev.parent;
	}

	use_dt = dev->of_node;
	id = of_match_node(tcc_thermal_id_table, use_dt);
	if (id != NULL) {
		data = id->data;
	} else {
		(void)pr_err(
			"[ERROR][TSENSOR]cannot find thermal data.\n");
		goto error_eno;
	}

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct tcc_thermal_platform_data), GFP_KERNEL);
	if (memcpy(pdata, data->pdata,
			sizeof(struct tcc_thermal_platform_data)) == NULL) {
		(void)pr_err(
			"[ERROR][TSENSOR]cannot copy platform data.\n");
		goto error_eno;
	}

	pdata->dev = dev;
	ret = pdata->ops->parse_dt(pdev, pdata);

	if (ret != 0) {
		(void)pr_err(
			"[ERROR][TSENSOR]No platform init thermal data supplied.\n");
		goto error_parse_dt;
	}
	//enable register
	if (use_dt != NULL) {
		pdata->base = of_iomap(use_dt, 0);
		if (pdata->base == NULL) {
			(void)pr_err(
				"[ERROR][TSENSOR]Failed to get tsensor base address\n");
			goto error_parse_dt;
		}
	} else {
		(void)pr_err(
			"[ERROR][TSENSOR]Failed to get tsensor base address\n");
		goto error_parse_dt;
	}

	platform_set_drvdata(pdev, pdata);
	if (pdata->ops->get_fuse_data != NULL) {
		ret = pdata->ops->get_fuse_data(pdev);
	}
	if (ret != 0) {
		(void)pr_err("[ERROR][TSENSOR]cannot get fuse data");
	}

	ret = pdata->ops->init(pdata);
	if (ret != 0) {
		(void)pr_err("[ERROR][TSENSOR]Failed to initialize thermal\n");
		goto error_init;
	}

	ret = tcc_register_thermal(pdata);
	if (ret != 0) {
		(void)pr_err(
			"[ERROR][TSENSOR]Failed to register tcc_thermal\n");
		goto error_register;
	}

	goto retval;

error_register:
error_init:
	platform_set_drvdata(pdev, NULL);
error_parse_dt:
error_eno:
	ret = -ENODEV;
retval:
	return ret;
}
static int32_t tcc_thermal_remove(struct platform_device *pdev)
{
	const struct tcc_thermal_platform_data *pdata =
					dev_get_drvdata(&pdev->dev);

	tcc_unregister_thermal(pdata);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int32_t tcc_thermal_suspend(struct device *dev)
{
	return 0;
}

static int32_t tcc_thermal_resume(struct device *dev)
{
	const struct tcc_thermal_platform_data *pdata = dev_get_drvdata(dev);
	int32_t ret = 0;

	ret = pdata->ops->init(pdata);
	if (ret != 0) {
		dev_err(dev, "[ERROR][TSENSOR]Failed to initialize thermal\n");
	}

	return ret;
}

SIMPLE_DEV_PM_OPS(tcc_thermal_pm_ops, tcc_thermal_suspend, tcc_thermal_resume);
#endif


static struct platform_driver tcc_thermal_driver = {
	.probe = tcc_thermal_probe,
	.remove = tcc_thermal_remove,
	.driver = {
		.name = "tcc-thermal",
#ifdef CONFIG_PM_SLEEP
		.pm = &tcc_thermal_pm_ops,
#endif
		.of_match_table = tcc_thermal_id_table,
	},
};

module_platform_driver(tcc_thermal_driver);

MODULE_AUTHOR("jay.kim@telechips.com");
MODULE_DESCRIPTION("Telechips thermal driver");
MODULE_LICENSE("GPL");

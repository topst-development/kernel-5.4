// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#define pr_fmt(fmt) "tcc-reset: " fmt

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <soc/tcc/tcc-sip.h>

union of_data {
	const void *priv;
	ulong data;
};

struct reset_data {
	struct reset_controller_dev rcdev;
	spinlock_t lock;
	union of_data fn;
};

static inline void reset_ctrl(ulong fn, ulong id, ulong op, spinlock_t *lock)
{
	struct arm_smccc_res res;
	ulong flags;

	spin_lock_irqsave(lock, flags);
	arm_smccc_smc(fn, id, op, 0, 0, 0, 0, 0, &res);
	spin_unlock_irqrestore(lock, flags);
}

static int reset_common(struct reset_controller_dev *rcdev, ulong id, ulong op)
{
	struct reset_data *data = container_of(rcdev, struct reset_data, rcdev);
	const char *desc = (op == 1UL) ? "Assert" : "Release";

	pr_debug("%s soft reset %lu\n", desc, id);
	reset_ctrl(data->fn.data, id, op, &data->lock);

	return 0;
}

static int reset_assert(struct reset_controller_dev *rcdev, unsigned long id)
{
	return reset_common(rcdev, id, 1UL);
}

static int reset_deassert(struct reset_controller_dev *rcdev, unsigned long id)
{
	return reset_common(rcdev, id, 0UL);
}

static const struct reset_control_ops reset_ops = {
	.assert		= reset_assert,
	.deassert	= reset_deassert,
};

static int tcc_reset_data_init(struct device *dev, struct reset_data *data)
{
	s32 ret;
	const char *fail = NULL;

	/* Get SiP service call ID for reset control */
	data->fn.priv = of_device_get_match_data(dev);
	if (data->fn.priv == NULL) {
		fail = "Failed to get SiP service call";
		ret = -EINVAL;
	} else {
		/* Register reset controller */
		data->rcdev.owner = THIS_MODULE;
		data->rcdev.nr_resets = 32;
		data->rcdev.ops = &reset_ops;
		data->rcdev.of_node = dev->of_node;

		spin_lock_init(&data->lock);

		ret = devm_reset_controller_register(dev, &data->rcdev);
		if (ret != 0) {
			fail = "Failed to register reset controller";
		}
	}

	if (fail != NULL) {
		(void)pr_err("%s (err: %d)\n", fail, ret);
	}

	return ret;
}

static int tcc_reset_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct reset_data *data;
	s32 ret;

	/* Allocate memory for driver data */
	data = devm_kzalloc(dev, sizeof(struct reset_data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		(void)pr_err("Failed to allocate driver data (err: %d)\n", ret);
	} else {
		/* Initialize tcc-reset driver data */
		ret = tcc_reset_data_init(dev, data);
		if (ret != 0) {
			devm_kfree(dev, data);
		} else {
			platform_set_drvdata(pdev, data);
		}
	}

	return ret;
}

static int tcc_reset_remove(struct platform_device *pdev)
{
	struct reset_data *data = platform_get_drvdata(pdev);

	reset_controller_unregister(&data->rcdev);

	return 0;
}

static const struct of_device_id tcc_reset_match[6] = {
	{
		.compatible = "telechips,reset",
		.data = (void *)SIP_CLK_SWRESET,
	},
	{
		.compatible = "telechips,vpubus-reset",
		.data = (void *)SIP_CLK_RESET_VPUBUS,
	},
	{
		.compatible = "telechips,ddibus-reset",
		.data = (void *)SIP_CLK_RESET_DDIBUS,
	},
	{
		.compatible = "telechips,iobus-reset",
		.data = (void *)SIP_CLK_RESET_IOBUS,
	},
	{
		.compatible = "telechips,hsiobus-reset",
		.data = (void *)SIP_CLK_RESET_HSIOBUS,
	},
	{ .compatible = "" }
};

MODULE_DEVICE_TABLE(of, tcc_reset_match);

static struct platform_driver tcc_reset_driver = {
	.probe = tcc_reset_probe,
	.remove = tcc_reset_remove,
	.driver = {
		.name = "tcc-reset",
		.of_match_table = of_match_ptr(tcc_reset_match),
	},
};

static int __init tcc_reset_init(void)
{
	return platform_driver_register(&tcc_reset_driver);
}

static void __exit tcc_reset_exit(void)
{
	return platform_driver_unregister(&tcc_reset_driver);
}

postcore_initcall(tcc_reset_init);
module_exit(tcc_reset_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips reset driver");

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#define pr_fmt(fmt) "tcc: pm: " fmt

#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/regmap.h>
#include <linux/mfd/da9062/core.h>
#include <linux/regulator/consumer.h>

struct regcfg {
	u32 reg;
	u32 mask;
	u32 val;
};

static struct regmap *pmic_da9062;
#if defined(CONFIG_PM_TCC805X_DA9131_SW_WORKAROUND)
static struct regmap *pmic_da9131_d0;
static struct regmap *pmic_da9131_d2;
#endif

static inline int config_reg(struct regmap *map, const struct regcfg *cfg)
{
	return regmap_update_bits(map, cfg->reg, cfg->mask, cfg->val);
}

static int config_regs(struct regmap *map, const struct regcfg *cfg, s32 num)
{
	s32 i;
	s32 ret = 0;

	if (map != NULL) {
		for (i = 0; i < num; i++) {
			ret = config_reg(map, &cfg[i]);
			if (ret < 0) {
				break;
			}
		}
	}

	return ret;
}

static int tcc_pm_ctrl_str_mode(s32 enter)
{
	/*
	 * Keep BUCK1/LDO2 on in power-down for STR support.
	 * Must be reset back to default config on wake-up.
	 */
	struct regcfg da9062_str_mode_cfg[2] = {
		{ 0x21, 0x08, 0x08 }, /* BUCK1_CONT   */
		{ 0x27, 0x80, 0x80 }, /* LDO2_CONT    */
	};

	s32 ret;

	if (enter == 0) {
		da9062_str_mode_cfg[0].val = 0;
		da9062_str_mode_cfg[1].val = 0;
	}

	ret = config_regs(pmic_da9062, da9062_str_mode_cfg, 2);

	return ret;
}

static int tcc_pm_init_pmic(void)
{
#if defined(CONFIG_PM_TCC805X_DA9062_SW_WORKAROUND)
	/*
	 * S/W Workaround for power sequence issue (DA9062)
	 * - Delay CORE0P8 power off timing for 16.4 msec
	 * - Unmask SYS_EN IRQ to get wake-up signal
	 */
	const struct regcfg da9062_cfg[3] = {
		{ 0x97, 0xff, 0x96 }, /* WAIT         */
		{ 0x92, 0xff, 0x03 }, /* ID_32_31     */
		{ 0x0c, 0x10, 0x00 }, /* IRQ_MASK_C   */
	};
#endif
#if defined(CONFIG_PM_TCC805X_DA9131_SW_WORKAROUND)
	/*
	 * S/W Workaround for power sequence issue (DA9131 OTP-42/43 v1)
	 * - Adjust voltage slew rate correctly
	 * - Enable GPIO1/2 pull-up/pull-down
	 */
	const struct regcfg da9131_cfg[6] = {
		{ 0x20, 0xff, 0x49 }, /* BUCK_BUCK1_0 */
		{ 0x21, 0xff, 0x49 }, /* BUCK_BUCK1_1 */
		{ 0x28, 0xff, 0x49 }, /* BUCK_BUCK2_0 */
		{ 0x29, 0xff, 0x49 }, /* BUCK_BUCK2_1 */
		{ 0x13, 0xff, 0x08 }, /* SYS_GPIO_1_1 */
		{ 0x15, 0xff, 0x08 }, /* SYS_GPIO_2_1 */
	};
#endif
	s32 ret = 0;

#if defined(CONFIG_PM_TCC805X_DA9062_SW_WORKAROUND)
	ret = config_regs(pmic_da9062, da9062_cfg, 3);
#endif
#if defined(CONFIG_PM_TCC805X_DA9131_SW_WORKAROUND)
	if (ret == 0) {
		ret = config_regs(pmic_da9131_d0, da9131_cfg, 6);
		if (ret == 0) {
			ret = config_regs(pmic_da9131_d2, da9131_cfg, 6);
		}
	}
#endif

	return ret;
}

static int tcc_pm_notifier_call(struct notifier_block *nb, unsigned long action,
				void *data)
{
	s32 ret = 0;
	const char *fail = NULL;

	if (action == (ulong)PM_SUSPEND_PREPARE) {
		ret = tcc_pm_ctrl_str_mode(1);
		if (ret < 0) {
			fail = "Failed to control pmic for entering STR mode";
		}
	}

	if (action == (ulong)PM_POST_SUSPEND) {
		ret = tcc_pm_ctrl_str_mode(0);
		if (ret < 0) {
			fail = "Failed to control pmic for exiting STR mode";
		} else {
			ret = tcc_pm_init_pmic();
			if (ret < 0) {
				fail = "Failed to init pmic";
			}
		}
	}

	if (fail != NULL) {
		(void)pr_err("%s (err: %d)\n", fail, ret);
	}

	return (ret < 0) ? NOTIFY_BAD : NOTIFY_OK;
}

static struct notifier_block tcc_pm_notifier_block = {
	.notifier_call = &tcc_pm_notifier_call,
};

static inline struct regmap * __init tcc_pm_get_pmic_one(const char *name)
{
	struct regulator *reg = regulator_get_exclusive(NULL, name);
	s32 ret = PTR_ERR_OR_ZERO(reg);
	struct regmap *map = NULL;

	if (ret == 0) {
		map = regulator_get_regmap(reg);
		regulator_put(reg);
		ret = PTR_ERR_OR_ZERO(map);
		if (ret != 0) {
			map = NULL;
		}
	}

	return map;
}

static int __init tcc_pm_get_pmic(void)
{
	pmic_da9062 = tcc_pm_get_pmic_one("memq_1p1");
#if defined(CONFIG_PM_TCC805X_DA9131_SW_WORKAROUND)
	pmic_da9131_d0 = tcc_pm_get_pmic_one("core_0p8");
	pmic_da9131_d2 = tcc_pm_get_pmic_one("cpu_1p0");
#endif

	return (pmic_da9062 == NULL) ? -ENOENT : 0;
}

static int __init tcc_pm_init(void)
{
	s32 ret;
	const char *fail = NULL;

	/* Get DA9062/9131 PMIC regmap */
	ret = tcc_pm_get_pmic();
	if (ret != 0) {
		fail = "Failed to get da9062 pmic regmap";
	} else {
		/* Initialize DA9062/9131 PMIC (software workaround) */
		ret = tcc_pm_init_pmic();
		if (ret != 0) {
			fail = "Failed to init pmic";
		} else {
			/* Initialize notifier for suspend and resume */
			ret = register_pm_notifier(&tcc_pm_notifier_block);
			if (ret != 0) {
				fail = "Failed to register notifier";
			}
		}
	}

	if (fail != NULL) {
		(void)pr_err("%s (err: %d)\n", fail, ret);
	}

	return ret;
}

late_initcall(tcc_pm_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC805x power management driver");

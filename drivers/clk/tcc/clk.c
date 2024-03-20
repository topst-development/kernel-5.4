// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/clkdev.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clocksource.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/arm-smccc.h>
#include <soc/tcc/tcc-sip.h>
#include <linux/clk/tcc.h>
#include <dt-bindings/clock/telechips,clk-common.h>

struct tcc_clk {
	struct clk_hw hw;
	const struct clk_ops *ops;
	u32 id;
	struct list_head list;
};

static LIST_HEAD(tcc_clk_list);

static struct tcc_ckc_ops *ckc_ops;

#define to_tcc_clk(tcc) container_of(tcc, struct tcc_clk, hw)

#define IGNORE_CLK_DISABLE

#ifdef CONFIG_DEBUG_FS
static s32 tcc_debugfs_clk_enable_show(void *data, u64 *val)
{
	struct tcc_clk *tcc = (struct tcc_clk *)data;
	s32 en_ret = 0;

	if (tcc->ops->is_enabled == NULL) {
		en_ret = -ENOENT;
	} else {
		*val = (u64)tcc->ops->is_enabled(&tcc->hw);
	}

	return en_ret;
}

static s32 tcc_debugfs_clk_enable_store(void *data, u64 val)
{
	const struct tcc_clk *tcc = (struct tcc_clk *)data;
	s32 ret = 0;


	if (val == (u64) 0) {
		clk_disable_unprepare(tcc->hw.clk);
	} else {
		ret = clk_prepare_enable(tcc->hw.clk);
	}

	return ret;
}

DEFINE_DEBUGFS_ATTRIBUTE(tcc_clk_enable_fops, tcc_debugfs_clk_enable_show,
			 tcc_debugfs_clk_enable_store, "%llu\n");
#endif

static void tcc_clk_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
#ifdef CONFIG_DEBUG_FS
	struct tcc_clk *tcc;
	char clk_en_str[] = "clk_enable";
	ushort dbg_fs_md = (ushort)0x124; //S_IRUSR | S_IRGRP | S_IROTH (0444)
	struct dentry *dt;

	tcc = to_tcc_clk((hw));
	dt = debugfs_create_file(clk_en_str, dbg_fs_md, dentry,
				   tcc, &tcc_clk_enable_fops);

	if (dt == NULL) {
		(void)pr_err("[ERROR][tcc_clk][%s]"
			" clk debug file system not created : %s\n",
			__func__, dentry->d_iname);
	}
#endif
}

static struct clk *tcc_onecell_get(struct of_phandle_args *clkspec, void *data)
{
	struct clk_onecell_data *clk_data = data;
	u32 idx = clkspec->args[0];
	u32 i;
	struct clk *ptr_clk = NULL;
	struct tcc_clk *ptr_tcc_clk;

	for (i = 0; i < clk_data->clk_num; i++) {
		ptr_clk = clk_data->clks[i];
		ptr_tcc_clk = to_tcc_clk((__clk_get_hw(ptr_clk)));

		if (idx == ptr_tcc_clk->id) {
			break;
		} else {
			ptr_clk = NULL;
		}
	}

	return ptr_clk;
}

static long tcc_clk_register(struct device_node *np, const struct clk_ops *ops)
{
	struct clk_onecell_data *clk_data;
	struct tcc_clk *ptr_tcc_clk;
	struct clk *ptr_clk;
	ulong num_clks;
	long ret;
	u32 i;

	num_clks = (ulong)of_property_count_strings(np, "clock-output-names");
	pr_debug("[DEBUG][tcc_clk][%s]%pOFfp: # of clks = %ld\n",
		 __func__, np, num_clks);

	clk_data = kzalloc(sizeof(struct clk_onecell_data), GFP_KERNEL);
	if (clk_data == NULL) {
		return -ENOMEM;
	}

	clk_data->clks = kcalloc(num_clks, sizeof(struct clk *), GFP_KERNEL);
	if ((clk_data->clks) == NULL) {
		kfree(clk_data);
		return -ENOMEM;
	}

	for (i = 0; i < num_clks; i++) {
		struct clk_init_data init;
		const char **parent_names;
		const char *clk_name;
		u32 index;
		u32 flags;
		bool nl_chk;

		ptr_tcc_clk = kzalloc(sizeof(struct tcc_clk), GFP_KERNEL);
		if (ptr_tcc_clk == NULL) {
			ret = PTR_ERR(ptr_tcc_clk);
			goto err;
		}

		ret = of_property_read_string_index(np, "clock-output-names",
						    (s32)i, &clk_name);
		if (ret != 0) {
			goto err;
		}

		ret = of_property_read_u32_index(np, "clock-indices",
						 i, &index);
		if (ret != 0) {
			index = i;
		}

		/* Optional properties for the clock flags */
		ret = of_property_read_u32_index(np, "telechips,clock-flags",
					   i, &flags);
		if (ret != 0) {
			flags = i;
		}
		init.name = clk_name;
		init.num_parents = (u8) of_clk_get_parent_count(np);
		parent_names = kcalloc(init.num_parents, sizeof(char *),
				       GFP_KERNEL);
		ret = of_clk_parent_fill(np, parent_names, init.num_parents);
		if (ret != 0) {
			pr_debug("[DEBUG][tcc_clk][%s] %ld num of parents exists\n",
				__func__, ret);
		}
		init.parent_names = parent_names;
		init.flags = flags;
#ifdef CONFIG_ARM64
		/* XXX: Do we need it? */
		init.flags |= CLK_IGNORE_UNUSED;
#endif
		init.ops = ops;
		ptr_tcc_clk->hw.init = &init;
		ptr_tcc_clk->ops = ops;
		ptr_tcc_clk->id = index;

		ptr_clk = clk_register(NULL, &ptr_tcc_clk->hw);
		nl_chk = IS_ERR_OR_NULL((ptr_clk));

		if (nl_chk != (bool)0) {
			/* This code shared by 32, 64bit architecture
			 * PTR_ERR return type = long
			 * = Cannot be fixed by one type(s64 or s32)
			 */
			long ret_ptr = PTR_ERR((ptr_clk));

			(void)pr_err("[ERROR][tcc_clk][%s] failed to register clk '%s' (%ld)\n",
				__func__, clk_name, ret_ptr);
			kfree((void *)ptr_tcc_clk);
			continue;
		}
		ret = clk_register_clkdev(ptr_clk, clk_name, NULL);
		if (ret != 0) {
			(void)pr_err("[ERROR][tcc_clk][%s] failed to register clkdev '%s' (%ld)\n",
				__func__, clk_name, ret);
			clk_unregister(ptr_clk);
			kfree((void *)ptr_tcc_clk);
			continue;
		}
		clk_data->clks[i] = ptr_clk;

		list_add_tail(&(ptr_tcc_clk->list), &tcc_clk_list);

		pr_debug("[DEBUG][tcc_clk][%s] %pOFfp: '%s' registered (index=%d)\n",
			 __func__, np, clk_name, index);
	}
	clk_data->clk_num = i;
	return of_clk_add_provider(np, tcc_onecell_get, (void *)clk_data);
err:
	return ret;
}

void tcc_ckc_set_ops(struct tcc_ckc_ops *ops)
{
	if (ops == NULL) {
		(void)pr_warn("[WARN][tcc_clk][%s] 'ops' is NULL.\n", __func__);
	}
#if !defined(CONFIG_TCC803X_CA7S)
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || \
		defined(CONFIG_ARCH_TCC901X) || defined(CONFIG_ARCH_TCC805X)
	ckc_ops = NULL;
#else
	ckc_ops = ops;
#endif
#else
	ckc_ops = ops;
#endif
}

/* common function */
static long tcc_round_rate(struct clk_hw *hw, unsigned long rate,
			   unsigned long *best_parent_rate)
{
	/* return type is defined on open source prototype. (long) */
	long ret = (rate > (ULONG_MAX / 2UL)) ? LONG_MAX : (long)rate;
	(void)hw;
	(void)best_parent_rate;

	return ret;
}

static s32 tcc_clkctrl_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_pmu_pwdn) != NULL) {
			ret = ckc_ops->ckc_pmu_pwdn(id, (bool)false);
		}
		if (((ckc_ops->ckc_swreset) != NULL) && (ret == 0)) {
			ret = ckc_ops->ckc_swreset(id, (bool)false);
		}
		if (((ckc_ops->ckc_clkctrl_enable) != NULL) && (ret == 0)) {
			ret = ckc_ops->ckc_clkctrl_enable(id);
		}
	} else {
		arm_smccc_smc(SIP_CLK_ENABLE_CLKCTRL, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
		/* Return Nothing */
	}

	return ret;
}

#ifndef IGNORE_CLK_DISABLE
static void tcc_clkctrl_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_clkctrl_disable) != NULL) {
			(void)ckc_ops->ckc_clkctrl_disable(id);
		}
		if ((ckc_ops->ckc_swreset) != NULL) {
			(void)ckc_ops->ckc_swreset(id, (bool)true);
		}
		if ((ckc_ops->ckc_pmu_pwdn) != NULL) {
			(void)ckc_ops->ckc_pmu_pwdn(id, (bool)true);
		}
	} else {
		arm_smccc_smc(SIP_CLK_DISABLE_CLKCTRL, tcc->id,
			      0, 0, 0, 0, 0, 0, &res);
	}
}
#endif

static ulong tcc_clkctrl_recalc_rate(struct clk_hw *hw,
					     ulong parent_rate)
{
	struct arm_smccc_res res;
	ulong rate = 0;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;

	if (parent_rate == 0UL) {
		pr_debug("[DEBUG][tcc_clk][%s]'parent_rate' is zero.\n",
			 __func__);
	}

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_clkctrl_get_rate) != NULL) {
			rate = ckc_ops->ckc_clkctrl_get_rate(id);
		}
	} else {
		arm_smccc_smc(SIP_CLK_GET_CLKCTRL, tcc->id, 0, 0,
			      0, 0, 0, 0, &res);
		rate = res.a0;
	}
	return rate;
}

static s32 tcc_clkctrl_set_rate(struct clk_hw *hw, ulong rate,
				ulong parent_rate)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (parent_rate == 0UL) {
		pr_debug("[DEBUG][tcc_clk][%s]'parent_rate' is zero.\n",
			 __func__);
	}

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_clkctrl_set_rate) != NULL) {
			ret = ckc_ops->ckc_clkctrl_set_rate(id, rate);
		}
	} else {
		arm_smccc_smc(SIP_CLK_SET_CLKCTRL, tcc->id, 1,
			      rate, 0, 0, 0, 0, &res);
		ret = (res.a0 == 0UL) ? 0 : -1;
	}

	return ret;
}

static s32 tcc_clkctrl_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_is_clkctrl_enabled) != NULL) {
			ret = ckc_ops->ckc_is_clkctrl_enabled(id);

			ret = (ret != 0) ? 1 : 0;
		}
	} else {
		arm_smccc_smc(SIP_CLK_IS_CLKCTRL, tcc->id, 0, 0,
			      0, 0, 0, 0, &res);
		ret = (res.a0 == 1UL) ? 1 : 0;
	}

	return ret;
}

static const struct clk_ops tcc_clkctrl_ops = {
	.enable = tcc_clkctrl_enable,
#ifndef IGNORE_CLK_DISABLE
	.disable = tcc_clkctrl_disable,
#endif
	.is_enabled = tcc_clkctrl_is_enabled,
	.recalc_rate = tcc_clkctrl_recalc_rate,
	.round_rate = tcc_round_rate,
	.set_rate = tcc_clkctrl_set_rate,
	.debug_init = tcc_clk_debug_init,
};

static void __init tcc_fbus_init(struct device_node *np)
{
	long ret_val;

	ret_val = tcc_clk_register(np, &tcc_clkctrl_ops);
	if (ret_val != 0) {
		(void)pr_err("[ERROR][tcc_clk][%s] ret_val is not zero.\n",
			__func__);
	}
}

CLK_OF_DECLARE(tcc_clk_fbus, "telechips,clk-fbus", tcc_fbus_init);

static s32 tcc_peri_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_peri_enable) != NULL) {
			ret = ckc_ops->ckc_peri_enable(id);
		}
	} else {
		arm_smccc_smc(SIP_CLK_ENABLE_PERI, tcc->id, 0, 0,
			      0, 0, 0, 0, &res);
		/* Return Nothing */
	}
	return ret;
}

static void tcc_peri_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_peri_disable) != NULL) {
			(void)ckc_ops->ckc_peri_disable(id);
		}
	} else {
		arm_smccc_smc(SIP_CLK_DISABLE_PERI, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
	}
}

static ulong tcc_peri_recalc_rate(struct clk_hw *hw,
					  ulong parent_rate)
{
	struct arm_smccc_res res;
	ulong rate = 0;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;

	if (parent_rate == 0UL) {
		pr_debug("[DEBUG][tcc_clk][%s]'parent_rate' is zero.\n",
			 __func__);
	}

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_peri_get_rate) != NULL) {
			rate = ckc_ops->ckc_peri_get_rate(id);
		}
	} else {
		arm_smccc_smc(SIP_CLK_GET_PCLKCTRL, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
		rate = res.a0;
	}
	return rate;
}

static s32 tcc_peri_set_rate(struct clk_hw *hw, ulong rate,
			     ulong parent_rate)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	ulong clk_flags = __clk_get_flags(hw->clk);
	ulong vflags = 0;
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (parent_rate == 0UL) {
		pr_debug("[DEBUG][tcc_clk][%s]'parent_rate' is zero.\n",
			 __func__);
	}

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_peri_set_rate) != NULL) {
#if defined(CONFIG_ARCH_TCC897X)
			ret = ckc_ops->ckc_peri_set_rate(id, rate);
#else
			ret = ckc_ops->ckc_peri_set_rate(id, rate, clk_flags);
#endif
		}
	} else {
		/* We care only about vendor-specific flags */
		if ((clk_flags & CLK_F_FIXED) != 0UL) {
			vflags = (clk_flags & (CLK_F_SRC_CLK_MASK <<
					    CLK_F_SRC_CLK_SHIFT)) >>
			    CLK_F_SRC_CLK_SHIFT;
		}
		if ((clk_flags & CLK_F_DCO_MODE) != 0UL) {
			vflags |= CLK_F_DCO_MODE;
		}
		if ((clk_flags & CLK_F_SKIP_SSCG) != 0UL) {
			vflags |= CLK_F_SKIP_SSCG;
		}
		if ((clk_flags & CLK_F_DIV_MODE) != 0UL) {
			vflags |= CLK_F_DIV_MODE;
		}
		arm_smccc_smc(SIP_CLK_SET_PCLKCTRL, tcc->id, 1UL,
			      rate, vflags, 0, 0, 0, &res);
		ret = (res.a0 == 0UL) ? 0 : -1;
	}
	return ret;
}

static s32 tcc_peri_set_src(struct clk_hw *hw, ulong src, ulong div)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	ulong clk_flags = __clk_get_flags(hw->clk);
	s32 ret = 0;

	if (ckc_ops != NULL) {
		(void)pr_err("[ERROR][tcc_clk][%s] not support in this platform.\n",
				__func__);

		ret = -1;
	} else {
		arm_smccc_smc(SIP_CLK_GET_PCLKCTRL, tcc->id, 0,
				0, 0, 0, 0, 0, &res);
		if (res.a1 != src) {
			if ((clk_flags & CLK_IS_PCLKDCO) == 0UL) {
				arm_smccc_smc(SIP_CLK_SET_PCLKCTRL_DIV,	tcc->id,
					src, (div > 0UL) ? (div - 1UL) : 0UL,
					0, 0, 0, 0, &res);
			} else {
				arm_smccc_smc(SIP_CLK_SET_PCLKCTRL_DCO,	tcc->id,
					src, (div > 1UL) ? (div - 1UL) : 1UL,
					0, 0, 0, 0, &res);

			}
			ret = (res.a0 == 0UL) ? 0 : -1;
		}
	}

	return ret;
}

static s32 tcc_peri_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_is_peri_enabled) != NULL) {
			ret = ckc_ops->ckc_is_peri_enabled(id);

			ret = (ret != 0) ? 1 : 0;
		}
	} else {
		arm_smccc_smc(SIP_CLK_IS_PERI, tcc->id, 0, 0, 0,
			      0, 0, 0, &res);
		ret = (res.a0 == 1UL) ? 1 : 0;
	}

	return ret;
}

#ifdef CONFIG_DEBUG_FS
static s32 debugfs_peri_clk_src_get(void *data, u64 *val)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = (struct tcc_clk *)data;

	if (ckc_ops != NULL) {
		// TODO
	} else {
		arm_smccc_smc(SIP_CLK_GET_PCLKCTRL, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
		*val = res.a1;
	}
	return (s32)0;
}

DEFINE_DEBUGFS_ATTRIBUTE(tcc_peri_clk_src_fops, debugfs_peri_clk_src_get,
			 NULL, "%llu\n");

static s32 debugfs_peri_clk_div_get(void *data, u64 *val)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = (struct tcc_clk *)data;

	if (ckc_ops != NULL) {
		// TODO
	} else {
		arm_smccc_smc(SIP_CLK_GET_PCLKCTRL, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
		*val = res.a2;
	}
	return (s32)0;
}

DEFINE_DEBUGFS_ATTRIBUTE(tcc_peri_clk_div_fops, debugfs_peri_clk_div_get,
			 NULL, "%llu\n");
#endif

static void tcc_peri_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
#ifdef CONFIG_DEBUG_FS
	struct tcc_clk *tcc = to_tcc_clk((hw));
	char clk_src_str[] = "clk_src";
	char clk_div_str[] = "clk_div";
	struct dentry *ret_ptr;
	ushort dbg_fs_md = (ushort)0x124; //S_IRUSR | S_IRGRP | S_IROTH (0444)

	ret_ptr = debugfs_create_file(clk_src_str, dbg_fs_md, dentry,
					tcc, &tcc_peri_clk_src_fops);

	if (ret_ptr == NULL) {
		(void)pr_err("[ERROR][tcc_clk][%s] clk_debugfs_add_file returned NULL\n",
			__func__);
	}

	ret_ptr = debugfs_create_file(clk_div_str, dbg_fs_md, dentry,
					tcc, &tcc_peri_clk_div_fops);

	if (ret_ptr == NULL) {
		(void)pr_err("[ERROR][tcc_clk][%s] clk_debugfs_add_file returned NULL\n",
			__func__);
	}

	return tcc_clk_debug_init(hw, dentry);
#endif
}

static const struct clk_ops tcc_peri_ops = {
	.enable = tcc_peri_enable,
	.disable = tcc_peri_disable,
	.recalc_rate = tcc_peri_recalc_rate,
	.round_rate = tcc_round_rate,
	.set_rate = tcc_peri_set_rate,
	.set_src = tcc_peri_set_src,
	.is_enabled = tcc_peri_is_enabled,
	.debug_init = tcc_peri_debug_init,
};

static void __init tcc_peri_init(struct device_node *np)
{
	long ret_val;

	ret_val = tcc_clk_register(np, &tcc_peri_ops);
	if (ret_val != 0) {
		(void)pr_err("[ERROR][tcc_clk][%s] ret_val is not zero.\n",
			__func__);
	}
}

CLK_OF_DECLARE(tcc_clk_peri, "telechips,clk-peri", tcc_peri_init);

static s32 tcc_isoip_top_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_isoip_top_pwdn) != NULL) {
			ret = ckc_ops->ckc_isoip_top_pwdn(id, (bool)false);
		}
	} else {
		arm_smccc_smc(SIP_CLK_ENABLE_ISOTOP, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
		/* Return Nothing */
	}

	return ret;
}

#ifndef IGNORE_CLK_DISABLE
static void tcc_isoip_top_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_isoip_top_pwdn) != NULL) {
			(void)ckc_ops->ckc_isoip_top_pwdn(id, (bool)true);
		}
	} else {
		arm_smccc_smc(SIP_CLK_DISABLE_ISOTOP, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
	}
}
#endif

static s32 tcc_isoip_top_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_is_isoip_top_pwdn) != NULL) {
			ret = ckc_ops->ckc_is_isoip_top_pwdn(id);
			ret = (ret == 0) ? 1 : 0;
		}
	} else {
		arm_smccc_smc(SIP_CLK_IS_ISOTOP, tcc->id, 0, 0,
			      0, 0, 0, 0, &res);

		ret = (res.a0 == 0UL) ? 1 : 0;
	}

	return ret;
}

static const struct clk_ops tcc_isoip_top_ops = {
	.enable = tcc_isoip_top_enable,
#ifndef IGNORE_CLK_DISABLE
	.disable = tcc_isoip_top_disable,
#endif
	.is_enabled = tcc_isoip_top_is_enabled,
	.debug_init = tcc_clk_debug_init,
};

static void __init tcc_isoip_top_init(struct device_node *np)
{
	long ret_val;

	ret_val = tcc_clk_register(np, &tcc_isoip_top_ops);
	if (ret_val != 0) {
		(void)pr_err("[ERROR][tcc_clk][%s] ret_val is not zero.\n",
			__func__);
	}
}

CLK_OF_DECLARE(tcc_clk_isoip_top, "telechips,clk-isoip_top",
	       tcc_isoip_top_init);

static s32 tcc_isoip_ddi_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_isoip_ddi_pwdn) != NULL) {
			ret = ckc_ops->ckc_isoip_ddi_pwdn(id, (bool)false);
			ret = (ret == 0) ? 0 : -1;
		}
	} else {
		arm_smccc_smc(SIP_CLK_ENABLE_ISODDI, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
		/* Return Nothing */
	}

	return ret;
}

#ifndef IGNORE_CLK_DISABLE
static void tcc_isoip_ddi_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_isoip_ddi_pwdn) != NULL) {
			ckc_ops->ckc_isoip_ddi_pwdn(id, (bool)true);
		}
	} else {
		arm_smccc_smc(SIP_CLK_DISABLE_ISODDI, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
	}
}
#endif

static s32 tcc_isoip_ddi_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_is_isoip_ddi_pwdn) != NULL) {
			ret = ckc_ops->ckc_is_isoip_ddi_pwdn(id);
			ret = (ret == 0) ? 1 : 0;
		}
	} else {
		arm_smccc_smc(SIP_CLK_IS_ISODDI, tcc->id, 0, 0,
			      0, 0, 0, 0, &res);

		ret = (res.a0 == 0UL) ? 1 : 0;
	}

	return ret;
}

static const struct clk_ops tcc_isoip_ddi_ops = {
	.enable = tcc_isoip_ddi_enable,
#ifndef IGNORE_CLK_DISABLE
	.disable = tcc_isoip_ddi_disable,
#endif
	.is_enabled = tcc_isoip_ddi_is_enabled,
	.debug_init = tcc_clk_debug_init,
};

static void __init tcc_isoip_ddi_init(struct device_node *np)
{
	long ret_val;

	ret_val = tcc_clk_register(np, &tcc_isoip_ddi_ops);
	if (ret_val != 0) {
		(void)pr_err("[ERROR][tcc_clk][%s] ret_val is not zero.\n",
			__func__);
	}
}

CLK_OF_DECLARE(tcc_clk_isoip_ddi, "telechips,clk-isoip_ddi",
	       tcc_isoip_ddi_init);

static s32 tcc_ddibus_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_ddibus_pwdn) != NULL) {
			ret = ckc_ops->ckc_ddibus_pwdn(id, (bool)false);		}
		if (((ckc_ops->ckc_ddibus_swreset) != NULL) && (ret == 0)) {
			ret = ckc_ops->ckc_ddibus_swreset(id, (bool)false);
		}
	} else {
		arm_smccc_smc(SIP_CLK_ENABLE_DDIBUS, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
		/* Return Nothing */
	}
	return ret;
}

static void tcc_ddibus_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_ddibus_swreset) != NULL) {
			(void)ckc_ops->ckc_ddibus_swreset(id, (bool)true);
		}
		if ((ckc_ops->ckc_ddibus_pwdn) != NULL) {
			(void)ckc_ops->ckc_ddibus_pwdn(id, (bool)true);
		}
	} else {
		arm_smccc_smc(SIP_CLK_DISABLE_DDIBUS, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
	}
}

static s32 tcc_ddibus_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_is_ddibus_pwdn) != NULL) {
			ret = ckc_ops->ckc_is_ddibus_pwdn(id);
			ret = (ret == 0) ? 1 : 0;
		}
	} else {
		arm_smccc_smc(SIP_CLK_IS_DDIBUS, tcc->id, 0, 0,
			      0, 0, 0, 0, &res);

		ret = (res.a0 == 0UL) ? 1 : 0;
	}

	return ret;
}

static const struct clk_ops tcc_ddibus_ops = {
	.enable = tcc_ddibus_enable,
	.disable = tcc_ddibus_disable,
	.is_enabled = tcc_ddibus_is_enabled,
	.debug_init = tcc_clk_debug_init,
};

static void __init tcc_ddibus_init(struct device_node *np)
{
	long ret_val;

	ret_val = tcc_clk_register(np, &tcc_ddibus_ops);
	if (ret_val != 0) {
		(void)pr_err("[ERROR][tcc_clk][%s] ret_val is not zero.\n",
			__func__);
	}
}

CLK_OF_DECLARE(tcc_clk_ddibus, "telechips,clk-ddibus", tcc_ddibus_init);

static s32 tcc_iobus_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_iobus_pwdn) != NULL) {
			ret = ckc_ops->ckc_iobus_pwdn(id, (bool)false);
		}
		if (((ckc_ops->ckc_iobus_swreset) != NULL) && (ret == 0)) {
			ret = ckc_ops->ckc_iobus_swreset(id, (bool)false);
		}
	} else {
		arm_smccc_smc(SIP_CLK_ENABLE_IOBUS, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
		/* Return Nothing */
	}

	return ret;
}

static void tcc_iobus_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_iobus_swreset) != NULL) {
			(void)ckc_ops->ckc_iobus_swreset(id, (bool)true);
		}
		if ((ckc_ops->ckc_iobus_pwdn) != NULL) {
			(void)ckc_ops->ckc_iobus_pwdn(id, (bool)true);
		}
	} else {
		arm_smccc_smc(SIP_CLK_DISABLE_IOBUS, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
	}
}

static s32 tcc_iobus_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_is_iobus_pwdn) != NULL) {
			ret = ckc_ops->ckc_is_iobus_pwdn(id);
			ret = (ret == 0) ? 1 : 0;
		}
	} else {
		arm_smccc_smc(SIP_CLK_IS_IOBUS, tcc->id, 0, 0, 0,
			      0, 0, 0, &res);

		ret = (res.a0 == 0UL) ? 1 : 0;
	}

	return ret;
}

static const struct clk_ops tcc_iobus_ops = {
	.enable = tcc_iobus_enable,
	.disable = tcc_iobus_disable,
	.is_enabled = tcc_iobus_is_enabled,
	.debug_init = tcc_clk_debug_init,
};

static void __init tcc_iobus_init(struct device_node *np)
{
	long ret_val;

	ret_val = tcc_clk_register(np, &tcc_iobus_ops);

	if (ret_val != 0) {
		(void)pr_err("[ERROR][tcc_clk][%s] ret_val is not zero.\n",
			__func__);
	}
}

CLK_OF_DECLARE(tcc_clk_iobus, "telechips,clk-iobus", tcc_iobus_init);

static s32 tcc_vpubus_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_vpubus_pwdn) != NULL) {
			ret = ckc_ops->ckc_vpubus_pwdn(id, (bool)false);
		}
		if (((ckc_ops->ckc_vpubus_swreset) != NULL) && (ret == 0)) {
			ret = ckc_ops->ckc_vpubus_swreset(id, (bool)false);
		}
	} else {
		arm_smccc_smc(SIP_CLK_ENABLE_VPUBUS, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
		/* Return Nothing */
	}

	return ret;
}

static void tcc_vpubus_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_vpubus_swreset) != NULL) {
			(void)ckc_ops->ckc_vpubus_swreset(id, (bool)true);
		}
		if ((ckc_ops->ckc_vpubus_pwdn) != NULL) {
			(void)ckc_ops->ckc_vpubus_pwdn(id, (bool)true);
		}
	} else {
		arm_smccc_smc(SIP_CLK_DISABLE_VPUBUS, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
	}
}

static s32 tcc_vpubus_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_is_vpubus_pwdn) != NULL) {
			ret = ckc_ops->ckc_is_vpubus_pwdn(id);
			ret = (ret == 1) ? 1 : 0;
		}
	} else {
		arm_smccc_smc(SIP_CLK_IS_VPUBUS, tcc->id, 0, 0,
			      0, 0, 0, 0, &res);

		ret = (res.a0 == 1UL) ? 1 : 0;
	}

	return ret;
}

static const struct clk_ops tcc_vpubus_ops = {
	.enable = tcc_vpubus_enable,
	.disable = tcc_vpubus_disable,
	.is_enabled = tcc_vpubus_is_enabled,
	.debug_init = tcc_clk_debug_init,
};

static void __init tcc_vpubus_init(struct device_node *np)
{
	long ret_val;

	ret_val = tcc_clk_register(np, &tcc_vpubus_ops);

	if (ret_val != 0) {
		(void)pr_err("[ERROR][tcc_clk][%s] ret_val is not zero.\n",
			__func__);
	}
}

CLK_OF_DECLARE(tcc_clk_vpubus, "telechips,clk-vpubus", tcc_vpubus_init);

static s32 tcc_hsiobus_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_hsiobus_pwdn) != NULL) {
			ret = ckc_ops->ckc_hsiobus_pwdn(id, (bool)false);
		}
		if (((ckc_ops->ckc_hsiobus_swreset) != NULL) && (ret == 0)) {
			ret = ckc_ops->ckc_hsiobus_swreset(id, (bool)false);
		}
	} else {
		arm_smccc_smc(SIP_CLK_ENABLE_HSIOBUS, tcc->id, 0,
			      0, 0, 0, 0, 0, &res);
		/* Return Nothing */
	}

	return ret;
}

static void tcc_hsiobus_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_hsiobus_swreset) != NULL) {
			(void)ckc_ops->ckc_hsiobus_swreset(id, (bool)true);
		}
		if ((ckc_ops->ckc_hsiobus_pwdn) != NULL) {
			(void)ckc_ops->ckc_hsiobus_pwdn(id, (bool)true);
		}
	} else {
		arm_smccc_smc(SIP_CLK_DISABLE_HSIOBUS, tcc->id,
			      0, 0, 0, 0, 0, 0, &res);
	}
}

static s32 tcc_hsiobus_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_is_hsiobus_pwdn) != NULL) {
			ret = ckc_ops->ckc_is_hsiobus_pwdn(id);
			ret = (ret == 1) ? 1 : 0;
		}
	} else {
		arm_smccc_smc(SIP_CLK_IS_HSIOBUS, tcc->id, 0, 0,
			      0, 0, 0, 0, &res);

		ret = (res.a0 == 1UL) ? 1 : 0;
	}

	return ret;
}

static const struct clk_ops tcc_hsiobus_ops = {
	.enable = tcc_hsiobus_enable,
	.disable = tcc_hsiobus_disable,
	.is_enabled = tcc_hsiobus_is_enabled,
	.debug_init = tcc_clk_debug_init,
};

static void __init tcc_hsiobus_init(struct device_node *np)
{
	long ret_val;

	ret_val = tcc_clk_register(np, &tcc_hsiobus_ops);

	if (ret_val != 0) {
		(void)pr_err("[ERROR][tcc_clk][%s] ret_val is not zero.\n",
			__func__);
	}
}

CLK_OF_DECLARE(tcc_clk_hsiobus, "telechips,clk-hsiobus", tcc_hsiobus_init);

static s32 tcc_pll_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;
	s32 ret = 0;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_is_pll_enabled) != NULL) {
			ret = ckc_ops->ckc_is_pll_enabled(id);

			ret = (ret != 0) ? 0 : 1;
		}
	} else {
		arm_smccc_smc(SIP_CLK_IS_PLL_ENABLED, tcc->id,
			      0, 0, 0, 0, 0, 0, &res);
		ret = (res.a0 == 1UL) ? 1 : 0;
	}

	return ret;
}

static ulong tcc_pll_recalc_rate(struct clk_hw *hw,
				 ulong parent_rate)
{
	struct arm_smccc_res res;
	ulong rate = 0;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 id = (tcc->id < (UINT_MAX / 2U)) ? (s32)(tcc->id) : __INT_MAX__;

	if (ckc_ops != NULL) {
		if ((ckc_ops->ckc_clkctrl_get_rate) != NULL) {
			rate = ckc_ops->ckc_pll_get_rate(id);
		}
	} else {
		arm_smccc_smc(SIP_CLK_GET_PLL, tcc->id, 0, 0, 0,
			      0, 0, 0, &res);
		rate = res.a0;
	}
	return rate;
}

static const struct clk_ops tcc_pll_ops = {
	.is_enabled = tcc_pll_is_enabled,
	.recalc_rate = tcc_pll_recalc_rate,
	.debug_init = tcc_clk_debug_init,
};

static void __init tcc_pll_init(struct device_node *np)
{
	long ret_val;

	ret_val = tcc_clk_register(np, &tcc_pll_ops);

	if (ret_val != 0) {
		(void)pr_err("[ERROR][tcc_clk][%s] ret_val is not zero.\n",
			__func__);
	}
}

CLK_OF_DECLARE(tcc_clk_pll, "telechips,clk-pll", tcc_pll_init);

static s32 tcc_cpubus_enable(struct clk_hw *hw) {
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;

	if (ckc_ops != NULL) {
		(void)pr_err("[ERROR][tcc_clk][%s] Function not support.\n",
			__func__);
		ret = -1;
	} else {
		arm_smccc_smc(SIP_CLK_ENABLE_CPUBUS, tcc->id, 0,
				0, 0, 0, 0, 0, &res);
		/* Return Nothing */
	}

	return ret;
}

static void tcc_cpubus_disable(struct clk_hw *hw) {
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));

	if (ckc_ops != NULL) {
		(void)pr_err("[ERROR][tcc_clk][%s] Function not support.\n",
			__func__);
	} else {
		arm_smccc_smc(SIP_CLK_DISABLE_CPUBUS, tcc->id, 0,
				0, 0, 0, 0, 0, &res);
	}
}

static s32 tcc_cpubus_is_enabled(struct clk_hw *hw) {
	struct arm_smccc_res res;
	struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;

	if (ckc_ops != NULL) {
		(void)pr_err("[ERROR][tcc_clk][%s] Function not support.\n",
			__func__);
	} else {
		arm_smccc_smc(SIP_CLK_IS_CPUBUS, tcc->id, 0,
				0, 0, 0, 0, 0, &res);

		ret = (res.a0 == 1UL) ? 1 : 0;
	}

	return ret;
}

static const struct clk_ops tcc_cpubus_ops = {
	.enable = tcc_cpubus_enable,
	.disable = tcc_cpubus_disable,
	.is_enabled = tcc_cpubus_is_enabled,
	.debug_init = tcc_clk_debug_init,
};

static void __init tcc_cpubus_init(struct device_node *np) {
	long ret;

	ret = tcc_clk_register(np, &tcc_cpubus_ops);

	if (ret != 0) {
		(void)pr_err("[ERROR][tcc_clk][%s] ret is not zero.\n",
			__func__);
	}
}

CLK_OF_DECLARE(tcc_clk_cpubus, "telechips,clk-cpubus", tcc_cpubus_init);

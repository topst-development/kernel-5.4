// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#define pr_fmt(fmt) "bootstage: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/suspend.h>
#include <soc/tcc/tcc-sip.h>

#if !defined(CONFIG_HAVE_TCC_SIP_SERVICE)
#  include <clocksource/arm_arch_timer.h>
#  include <asm/arch_timer.h>
#endif

/* TODO: Parse descriptions and number of those from device tree? */
#if !defined(CONFIG_HAVE_TCC_SIP_SERVICE)
#  define NR_BOOT_STAGES (1U)
#elif defined(CONFIG_ARCH_TCC805X)
#  define NR_BOOT_STAGES (25U)
#elif defined(CONFIG_ARCH_TCC803X)
#  define NR_BOOT_STAGES (23U)
#else
#  error "Bootstage is not supported on this platform!"
#endif

static const char *const bootstage_desc[NR_BOOT_STAGES] = {
#if !defined(CONFIG_HAVE_TCC_SIP_SERVICE)
	/* Boot Stages on Kernel */
	"kernel init",
#elif defined(CONFIG_ARCH_TCC805X)
	/* Boot Stages on Storage Core BL0 */
	"setup sc bl0",
	"init storage",
	"init storage (post)",
	"reset hsm",
	"load sc fw header",
	"load sc fw",
	"wait hsm",
	"verify sc fw",
	"set hsm ready",
	"set sc ready",
	/* Boot Stages on AP Boot Firmware */
	"load bl1",
	"setup bl1",
	"load dram params",
	"init dram",
	"load bl2",
	"load secure f/w",
	"load bl3",
	"setup bl2",
	/* Boot Stages on AP U-Boot */
	"setup bl3",
	"board init f",
	"relocation",
	"board init r",
	"main loop",
	"boot kernel",
	/* Boot Stages on Kernel */
	"kernel init",
#elif defined(CONFIG_ARCH_TCC803X)
	/* Boot Stages on MCU Bootloader */
	"setup mcu bl0",
	"setup mcu bl1",
	NULL,
	/* Boot Stages on AP Boot Firmware */
	NULL,
	"setup bl1",
	"load hsm (early)",
	"init storage",
	"load key table",
	"load dram params",
	"init dram",
	"load subcore boot code",
	"load hsm",
	"load bl2",
	"load secure f/w",
	"load bl3",
	/* Boot Stages on AP U-Boot */
	"setup bl3",
	"board init f",
	"relocation",
	"board init r",
	"reset subcore",
	"main loop",
	"boot kernel",
	/* Boot Stages on Kernel */
	"kernel init",
#endif
};

static struct proc_dir_entry *bootstage_pdir;

#if !defined(CONFIG_HAVE_TCC_SIP_SERVICE)
static u32 bootstage_stamp[NR_BOOT_STAGES];
static u32 bootstage_count;

static void add_boot_timestamp(void)
{
	if (bootstage_count < NR_BOOT_STAGES) {
		u32 cntfrq = arch_timer_get_cntfrq();
		u32 cntvct = arch_timer_read_counter();

		bootstage_stamp[bootstage_count] = cntvct / (cntfrq / 1000000);
		++bootstage_count;
	}
}

static inline u32 get_boot_timestamp(ulong index)
{
	return (index < NR_BOOT_STAGES) ? bootstage_stamp[index] : 0U;
}

static inline u32 get_boot_timestamp_num(void)
{
	return bootstage_count;
}
#else
static void add_boot_timestamp(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_ADD_BOOTTIME, 0, 0, 0, 0, 0, 0, 0, &res);
}

static inline u32 get_boot_timestamp(ulong index)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_GET_BOOTTIME, index, 0, 0, 0, 0, 0, 0, &res);

	return ensure_u32(res.a0);
}

static inline u32 get_boot_timestamp_num(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_GET_BOOTTIME_NUM, 0, 0, 0, 0, 0, 0, 0, &res);

	return ensure_u32(res.a0);
}
#endif /* CONFIG_HAVE_TCC_SIP_SERVICE */

#ifndef MODULE
#  define add_boot_timestamp_if_builtin add_boot_timestamp
#else
#  define add_boot_timestamp_if_builtin
#endif

static inline void validate_boot_timestamp_num(struct seq_file *m, u32 num)
{
	if (num != NR_BOOT_STAGES) {
		seq_printf(m, "WARNING: Number of stages does not match. (actual: %u, expected: %u)\n\n",
			   num, NR_BOOT_STAGES);
	}
}

static inline const char *u32_to_str_in_format(u32 num, char *buf)
{
	s32 idx = 0;
	const char *ret = NULL;

	if (buf != NULL) {
		(void)scnprintf(buf, 12U, "%03u,%03u,%03u",
				(num / 1000000U) % 1000U,
				(num / 1000U) % 1000U, (num / 1U) % 1000U);

		while (((u8)buf[idx] == (u8)',') || ((u8)buf[idx] == (u8)'0')) {
			++idx;
		}

		if ((u8)buf[idx] == (u8)'\0') {
			--idx;
		}

		ret = &buf[idx];
	}

	return ret;
}

static int bootstage_report_show(struct seq_file *m, void *v)
{
	u32 num;
	u32 n;
	u32 prev = 0;
	u32 nr_actual = 0;

	num = get_boot_timestamp_num();
	validate_boot_timestamp_num(m, num);

	seq_printf(m, "Timer summary in microseconds (%u records):\n", num);

	/* format:           11s        11s      s */
	seq_puts(m, "       Mark    Elapsed  Stage\n");
	seq_puts(m, "          0          0  reset\n");

	for (n = 0; n < num; n++) {
		char buf[12]; /* "000,000,000" */
		const char *desc;
		const char *fmt;
		u32 stamp;

		desc = (n < NR_BOOT_STAGES) ? bootstage_desc[n] : "-";
		stamp = get_boot_timestamp(n);

		if ((stamp == 0U) || (desc == NULL)) {
			continue;
		}

		fmt = u32_to_str_in_format(stamp, buf);
		seq_printf(m, "%11s", fmt);

		if (stamp >= prev) {
			fmt = u32_to_str_in_format(stamp - prev, buf);
		} else {
			fmt = "overflow";
		}

		seq_printf(m, "%11s  %s\n", fmt, desc);

		prev = stamp;
		++nr_actual;
	}

	seq_printf(m, "\n%d stages out of %u stages were proceeded. (%u stages skipped)\n",
		   nr_actual, num, num - nr_actual);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(bootstage_report);

static int bootstage_data_show(struct seq_file *m, void *v)
{
	u32 num;
	u32 n;
	u32 prev = 0;

	num = get_boot_timestamp_num();
	validate_boot_timestamp_num(m, num);

	seq_puts(m, "reset,0\n");

	for (n = 0; n < num; n++) {
		const char *desc;
		u32 stamp;

		desc = (n < NR_BOOT_STAGES) ? bootstage_desc[n] : "-";
		stamp = get_boot_timestamp(n);

		if (desc == NULL) {
			continue;
		}

		if (stamp == 0U) {
			seq_printf(m, "%s,0\n", desc);
			continue;
		}

		if (stamp >= prev) {
			seq_printf(m, "%s,%u\n", desc, stamp - prev);
		} else {
			seq_printf(m, "%s,overflow\n", desc);
		}

		prev = stamp;
	}

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(bootstage_data);

static int bootstage_pm_notifier_call(struct notifier_block *nb,
				      unsigned long action, void *data)
{
	if (action == (ulong)PM_POST_SUSPEND) {
		/* Treat this point as the end of kernel resume sequence */
		add_boot_timestamp();
	}

	return 0;
}

static struct notifier_block bootstage_pm_notifier = {
	.notifier_call = bootstage_pm_notifier_call,
};

static struct proc_dir_entry *bootstage_procfs_init(void)
{
	struct proc_dir_entry *pdir;
	const struct proc_dir_entry *pent;
	const struct file_operations *fops;

	/* Create bootstage procfs directory */
	pdir = proc_mkdir("bootstage", NULL);
	if (pdir != NULL) {
		/* Create report procfs entry under bootstage */
		fops = &bootstage_report_fops;
		pent = proc_create("report", 0444, pdir, fops);
		if (pent == NULL) {
			remove_proc_entry("bootstage", NULL);
			pdir = NULL;
		} else {
			/* Create data procfs entry under bootstage */
			fops = &bootstage_data_fops;
			pent = proc_create("data", 0444, pdir, fops);
			if (pent == NULL) {
				remove_proc_entry("report", pdir);
				remove_proc_entry("bootstage", NULL);
				pdir = NULL;
			}
		}
	}

	return pdir;
}

static void bootstage_procfs_free(struct proc_dir_entry *pdir)
{
	remove_proc_entry("data", pdir);
	remove_proc_entry("report", pdir);
	remove_proc_entry("bootstage", NULL);
}

static int __init bootstage_init(void)
{
	s32 ret = 0;
	const char *fail = NULL;

	/* Create bootstage procfs entry tree */
	bootstage_pdir = bootstage_procfs_init();
	if (bootstage_pdir == NULL) {
		fail = "Failed to create bootstage procfs entry";
		ret = -ENOMEM;
	} else {
		/* Register PM notifier for boot time until resumption */
		ret = register_pm_notifier(&bootstage_pm_notifier);
		if (ret != 0) {
			fail = "Failed to register pm notifier";
			bootstage_procfs_free(bootstage_pdir);
		}
	}

#if !defined(CONFIG_HAVE_TCC_SIP_SERVICE)
	memset(bootstage_stamp, 0, sizeof(u32) * NR_BOOT_STAGES);
	bootstage_count = 0U;
#endif

	/* Treat here as the end of kernel init (built-in only) */
	add_boot_timestamp_if_builtin();

	if (fail != NULL) {
		(void)pr_err("%s (err: %d)\n", fail, ret);
	}

	return ret;
}

static void __exit bootstage_exit(void)
{
	(void)unregister_pm_notifier(&bootstage_pm_notifier);
	bootstage_procfs_free(bootstage_pdir);
}

/*
 * Because we treat the end of this initcall as the end of kernel init,
 * bootstage_init() must be called as late as possible.
 */
late_initcall_sync(bootstage_init);
module_exit(bootstage_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips bootstage driver");

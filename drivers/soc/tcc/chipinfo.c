// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#define pr_fmt(fmt) "chipinfo: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <soc/tcc/chipinfo.h>

#define desc_len (55U)			/* 80 cols - 26 prefix + 1 null */

static char desc_cinfo[desc_len];	/* TCC%04x Rev%02u */
static char desc_binfo[desc_len];	/* %s-core (%s Boot) */
static char desc_tver[desc_len];	/* v%u.%u.%u-g%07x%s */

static inline void proc_chipinfo_show_one(struct seq_file *m, const char *name,
					  const char *desc)
{
	if (desc[0] != '\0') {
		/* Print if description exists */
		seq_printf(m, "%-24s: %s\n", name, desc);
	}
}

static int proc_chipinfo_show(struct seq_file *m, void *v)
{
	proc_chipinfo_show_one(m, "Chip Variant", desc_cinfo);
	proc_chipinfo_show_one(m, "Core Identity", desc_binfo);
	proc_chipinfo_show_one(m, "Trusted Firmware", desc_tver);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(proc_chipinfo);

static void __init chipinfo_init_chip_info(void)
{
	struct chip_info cinfo = { INFO_UNK, INFO_UNK };

	get_chip_info(&cinfo);

	/* Build description string for chip information */
	(void)scnprintf(desc_cinfo, desc_len, "TCC%x Rev%02u",
			cinfo.name, cinfo.rev);
}

static void __init chipinfo_init_boot_info(void)
{
	struct boot_info binfo = { INFO_UNK, INFO_UNK };

	get_boot_info(&binfo);

	/* Build description string for boot information */
	if ((binfo.bootsel == INFO_UNK) || (binfo.coreid == INFO_UNK)) {
		desc_binfo[0] = '\0';
	} else {
		const bool is_main = is_main_core(binfo.coreid);
		const bool is_dual = is_dual_boot(binfo.bootsel);

		(void)scnprintf(desc_binfo, desc_len, "%s-core (%s Boot)",
				is_main ? "Main" : "Sub",
				is_dual ? "Dual" : "Single");
	}
}

static void __init chipinfo_init_tver_info(void)
{
	struct version_info tver = { INFO_UNK, INFO_UNK, INFO_UNK };

	get_tf_version(&tver);

	/* Build description string for Trusted Firmware version */
	if (tver.major == INFO_UNK) {
		desc_tver[0] = '\0';
	} else if (tver.id == INFO_UNK) {
		(void)scnprintf(desc_tver, desc_len, "v%u.%u.%u",
				tver.major, tver.minor, tver.patch);
	} else {
		(void)scnprintf(desc_tver, desc_len, "v%u.%u.%u-g%07x%s",
				tver.major, tver.minor, tver.patch, tver.id,
				tver.dirty ? "-dirty" : "");
	}
}

static int __init chipinfo_init(void)
{
	const struct proc_dir_entry *pent;
	s32 ret = 0;

	chipinfo_init_chip_info();
	chipinfo_init_boot_info();
	chipinfo_init_tver_info();

	/* Create chipinfo procfs entry for user interface */
	pent = proc_create("chipinfo", 0444, NULL, &proc_chipinfo_fops);
	if (pent == NULL) {
		(void)pr_err("Failed to create chipinfo procfs entry\n");
		ret = -ENOMEM;
	}

	/* Print-out chip and boot information */
	(void)pr_info("%s %s\n", desc_cinfo, desc_binfo);

	return ret;
}

static void __exit chipinfo_exit(void)
{
	remove_proc_entry("chipinfo", NULL);
}

core_initcall(chipinfo_init);
module_exit(chipinfo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips chipinfo driver");

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Telechips Inc
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "optee_version.h"

/*
 * Version information at /proc/optee
 */
struct tee_version {
	char name[6];
	struct tee_ioctl_version_tcc *ver;
	uint32_t id;
};

/* 0:os, 1:supp, 2:teec lib */
static struct tee_version tee_proc_ver[] = {
	{"teeos", NULL},
	{"suppl", NULL},
	{"teec ", NULL},
};

void tee_set_version(unsigned int cmd, struct tee_ioctl_version_tcc *ver,
		     uint32_t id)
{
	int idx;

	switch (cmd) {
	case TEE_IOC_OS_VERSION:
		idx = 0;
		break;
	case TEE_IOC_SUPP_VERSION:
		idx = 1;
		break;
	case TEE_IOC_CLIENT_VERSION:
		idx = 2;
		break;
	default:
		return;
	}

	if (tee_proc_ver[idx].ver) {
		pr_info("%s version already set\n", tee_proc_ver[idx].name);
		return;
	}

	tee_proc_ver[idx].ver =
		kmalloc(sizeof(struct tee_ioctl_version_tcc), GFP_KERNEL);
	if (tee_proc_ver[idx].ver) {
		memcpy(tee_proc_ver[idx].ver, ver,
		       sizeof(struct tee_ioctl_version_tcc));
		tee_proc_ver[idx].id = id;
	}
}

static void *tee_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

static void *tee_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void tee_stop(struct seq_file *m, void *v)
{
}

static int tee_show(struct seq_file *m, void *v)
{
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(tee_proc_ver); idx++) {
		if (tee_proc_ver[idx].ver == NULL) {
			seq_printf(m, "%s: Not Implemented\n",
				tee_proc_ver[idx].name);
			continue;
		}

		seq_printf(m, "%s: v%d.%d.%d", tee_proc_ver[idx].name,
					       tee_proc_ver[idx].ver->major,
					       tee_proc_ver[idx].ver->minor,
					       tee_proc_ver[idx].ver->tcc_rev);
		if (tee_proc_ver[idx].ver->date) {
			seq_printf(m, " - %04x/%02x/%02x %02x:%02x:%02x",
				(uint32_t)(
				(tee_proc_ver[idx].ver->date >> 40) & 0xFFFF),
				(uint32_t)(
				(tee_proc_ver[idx].ver->date >> 32) & 0xFF),
				(uint32_t)(
				(tee_proc_ver[idx].ver->date >> 24) & 0xFF),
				(uint32_t)(
				(tee_proc_ver[idx].ver->date >> 16) & 0xFF),
				(uint32_t)(
				(tee_proc_ver[idx].ver->date >> 8) & 0xFF),
				(uint32_t)((tee_proc_ver[idx].ver->date)
					& 0xFF));
		}
		if (tee_proc_ver[idx].id) {
			seq_printf(m, " (%08x)", tee_proc_ver[idx].id);
		}
		seq_printf(m, "%c", '\n');
	}

	return 0;
}

static const struct seq_operations tee_op = {
	.start	= tee_start,
	.next	= tee_next,
	.stop	= tee_stop,
	.show	= tee_show
};

static int tee_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &tee_op);
}

static const struct file_operations proc_tee_operations = {
	.open		= tee_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int __init tee_proc_init(void)
{
	proc_create("optee", 0644, NULL, &proc_tee_operations);
	return 0;
}

device_initcall(tee_proc_init);

/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __TCCMISC_DRV_H__
#define __TCCMISC_DRV_H__

#include <linux/types.h>

/* Ioctl commands */
#define IOCTL_TCCMISC_PMAP _IOWR('T', 1, struct tccmisc_user_t)
#define IOCTL_TCCMISC_PMAP_KERNEL _IOWR('T', 2, struct tccmisc_user_t)
#define IOCTL_TCCMISC_PHYS _IOWR('T', 3, struct tccmisc_phys_t)
#define IOCTL_TCCMISC_PHYS_KERNEL _IOWR('T', 4, struct tccmisc_phys_t)

/* Ioctl data */
struct tccmisc_user_t {
	char name[32];	/* input: pmap name */
	__u64 base;	/* output: pmap base address */
	__u64 size;	/* output: pmap size */
};

struct tccmisc_phys_t {
	int dmabuf_fd;
	dma_addr_t addr;
	__u64 len;
};

/* Only kernel function */
int tccmisc_pmap(struct tccmisc_user_t *pmap);
int tccmisc_phys(struct device *dev, struct tccmisc_phys_t *phys);
#endif

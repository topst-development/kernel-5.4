// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_PMAP_H
#define VPU_PMAP_H

#include <linux/types.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)

#define VPU_PMAP_NAME_LEN    (16U)

#define VPU_PMAP_SECURED     ((u32)1U << 1)
#define VPU_PMAP_SHARED      ((u32)1U << 2)
#define VPU_PMAP_CMA_ALLOC   ((u32)1U << 3)

#define pmap_is_secured(pmap) (((pmap)->flags & VPU_PMAP_SECURED) != 0U)
#define pmap_is_shared(pmap) (((pmap)->flags & VPU_PMAP_SHARED) != 0U)
#define pmap_is_cma_alloc(pmap) (((pmap)->flags & VPU_PMAP_CMA_ALLOC) != 0U)

struct pmap {
    char name[VPU_PMAP_NAME_LEN];
    unsigned long base;
    unsigned long size;
    unsigned int groups;
    unsigned int rc;
    unsigned int flags;
};

int pmap_check_region(unsigned    long base, unsigned long size);
int pmap_get_info(const char *name, struct pmap *mem);
int pmap_release_info(const char *name);

#ifdef CONFIG_PMAP_TO_CMA
void *pmap_cma_remap(unsigned      long base, unsigned long size);
void pmap_cma_unmap(void *virt, unsigned long size);
#else
#define pmap_cma_remap(base, size) NULL
#define pmap_cma_unmap(base, size)
#endif

#endif // for LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)

#endif  /* VPU_PMAP_H */

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_RM_H
#define VPU_RM_H

#include <linux/types.h>

#define VRM_NAME_LEN           (16U)

#define MAX_VRMS               (64U)
#define MAX_VRM_GROUPS	       (4U)
#define MAX_VRM_VMEM           (22U)
#define MAX_VRM_VSW            (27U)

#define VRM_FLAG_SECURED       ((u32)1U << 1)
#define VRM_FLAG_SHARED        ((u32)1U << 2)
#define VRM_FLAG_VSTORED       ((u32)1U << 3)
#define VRM_FLAG_VSWED         ((u32)1U << 4)
#define VRM_FLAG_VSHARED       ((u32)1U << 5)
#define VRM_FLAG_VREAREND      ((u32)1U << 6)

#define vrm_is_secured(p)      (((p)->flags & VRM_FLAG_SECURED) != 0U)
#define vrm_is_shared(p)       (((p)->flags & VRM_FLAG_SHARED) != 0U)
#define vrm_is_vstored(p)      (((p)->flags & VRM_FLAG_VSTORED) != 0U)
#define vrm_is_vswed(p)        (((p)->flags & VRM_FLAG_VSWED) != 0U)
#define vrm_is_vshared(p)      (((p)->flags & VRM_FLAG_VSHARED) != 0U)
#define vrm_is_vrearend(p)     (((p)->flags & VRM_FLAG_VREAREND) != 0U)

struct vpmap {
	char name[VRM_NAME_LEN];
	u64 base;
	u64 size;
	u32 groups;
	u32 rc;
	u32 flags;
};

struct vrm {
	char name[VRM_NAME_LEN];
	u64 base;
	u64 size;
	u64 pa;
	u64 used;
	void *va;
	u32 groups;
	u32 rc;
	u32 flags;
	u32 ip;
	s32 sidx[MAX_VRM_VMEM];
};

int vrm_probe(struct platform_device *pdev);
int vrm_remove(struct platform_device *pdev);

#endif  /* VPU_RM_H */

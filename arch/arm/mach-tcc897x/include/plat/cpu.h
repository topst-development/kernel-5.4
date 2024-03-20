/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef __ARCH_ARM_TCC_PLAT_CPU_H
#define __ARCH_ARM_TCC_PLAT_CPU_H

/* IDs for TCC897x SoCs */
#define TCC897X_ARCH_ID		0x1011
#define TCC8970_CPU_ID		0x0000
#define TCC8975_CPU_ID		0x0001
#define TCC8971_CPU_ID		0x0002

extern unsigned int __arch_id;
extern unsigned int __cpu_id;

#define DEFINE_CPU_ID(name, arch, cpu)			\
static inline int cpu_is_##name(void)			\
{							\
	if (__arch_id == arch && __cpu_id == cpu)	\
		return 1;				\
	else						\
		return 0;				\
}

DEFINE_CPU_ID(tcc8970, TCC897X_ARCH_ID, TCC8970_CPU_ID);
DEFINE_CPU_ID(tcc8975, TCC897X_ARCH_ID, TCC8975_CPU_ID);
DEFINE_CPU_ID(tcc8971, TCC897X_ARCH_ID, TCC8971_CPU_ID);

#endif

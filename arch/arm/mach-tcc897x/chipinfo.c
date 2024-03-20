// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/export.h>
#include <linux/io.h>
#include <linux/types.h>
#include <mach/iomap.h>

#include <soc/tcc/chipinfo.h>

#define PMU_CONFIG		(0x14U)
#define PMU_CONFIG_PKGI_SHIFT	(0x16U)
#define PMU_CONFIG_PKGI_MASK	(0x7U)

#define BOOTROM_VADDR		(0xF0010000U)

#define BOOTROM_VER		(0x1CU)
#define BOOTROM_VER_SHIFT	(0x0U)
#define BOOTROM_VER_MASK	(0xFFU)

u32 plat_get_chip_rev(void)
{
	void __iomem *bootrom_ver = __io(BOOTROM_VADDR + BOOTROM_VER);
	u32 reg;

	reg = readl(bootrom_ver);
	reg = (reg >> BOOTROM_VER_SHIFT) & BOOTROM_VER_MASK;

	return reg;
}
EXPORT_SYMBOL(plat_get_chip_rev);

u32 plat_get_chip_name(void)
{
	void __iomem *pmu_cfg = IOMEM(io_p2v(TCC_PA_PMU + PMU_CONFIG));
	u32 reg;
	u32 name;

	reg = readl(pmu_cfg);
	reg = (reg >> PMU_CONFIG_PKGI_SHIFT) & PMU_CONFIG_PKGI_MASK;

	switch (reg) {
	case 0U:
		name = 0x8970U; /* or 8973, 8974 */
		break;
	case 1U:
		name = 0x8975U;
		break;
	case 2U:
		name = 0x8971U; /* Or 8972, 8977, Clavis2 */
		break;
	default:
		name = INFO_UNK;
		break;
	}

	return name;
}
EXPORT_SYMBOL(plat_get_chip_name);

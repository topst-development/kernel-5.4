// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/of_platform.h>
#include <linux/io.h>
#include <asm/mach/arch.h>
#include <plat/platsmp.h>
#include <mach/iomap.h>

#include "common.h"

/* Wrapper of 'OF_DEV_AUXDATA' to parenthesize arguments */
#define OF_DEV_AUXDATA_P(_compat, _phys, _name) \
	OF_DEV_AUXDATA((_compat), (_phys), (_name), NULL)

static struct of_dev_auxdata tcc897x_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA_P("telechips,vioc-fb", TCC_PA_VIOC, "tccfb"),
	OF_DEV_AUXDATA_P("telechips,tcc897x-sdhc", TCC_PA_SDMMC0, "tcc-sdhc.0"),
	OF_DEV_AUXDATA_P("telechips,tcc897x-sdhc", TCC_PA_SDMMC2, "tcc-sdhc.2"),
	OF_DEV_AUXDATA_P("telechips,tcc897x-sdhc", TCC_PA_SDMMC3, "tcc-sdhc.3"),
	OF_DEV_AUXDATA_P("telechips,nand-v8", TCC_PA_NFC, "tcc_nand"),
	OF_DEV_AUXDATA_P("telechips,tcc-ehci", TCC_PA_EHCI, "tcc-ehci"),
	OF_DEV_AUXDATA_P("telechips,tcc-ohci", TCC_PA_OHCI, "tcc-ohci"),
	OF_DEV_AUXDATA_P("telechips,dwc_otg", TCC_PA_DWC_OTG, "dwc_otg"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C0, "i2c.0"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C1, "i2c.1"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C2, "i2c.2"),
	OF_DEV_AUXDATA_P("telechips,i2c", TCC_PA_I2C3, "i2c.3"),
	OF_DEV_AUXDATA_P("telechips,wmixer_drv", 0, "wmixer0"),
	OF_DEV_AUXDATA_P("telechips,wmixer_drv", 1, "wmixer1"),
	OF_DEV_AUXDATA_P("telechips,scaler_drv", 1, "scaler1"),
	OF_DEV_AUXDATA_P("telechips,scaler_drv", 3, "scaler3"),
	OF_DEV_AUXDATA_P("telechips,tcc_wdma", 0, "wdma"),
	OF_DEV_AUXDATA_P("telechips,tcc_overlay", 0, "overlay"),
	{},
};

static void __init tcc897x_dt_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
			tcc897x_auxdata_lookup, NULL);
	platform_device_register_simple("tcc-cpufreq", -1, NULL, 0);
}

static char const *tcc897x_dt_compat[] __initconst = {
	"telechips,tcc897x",
	NULL
};

#define PMU_CONFIG		(0x14U)
#define PMU_CONFIG_REMAP	((u32)3 << 28U)

#define PMU_WDTCTRL		(0x8U)
#define PMU_WDTCTRL_EN		((u32)1 << 31U)

static void tcc897x_reset_cpu(enum reboot_mode mode, const char *cmd)
{
	void __iomem *pmu_cfg = IOMEM(io_p2v(TCC_PA_PMU + PMU_CONFIG));
	void __iomem *pmu_wdtctrl = IOMEM(io_p2v(TCC_PA_PMU + PMU_WDTCTRL));
	u32 reg;

	/* Set Remap register to Boot-ROM */
	reg = readl(pmu_cfg);
	reg &= ~PMU_CONFIG_REMAP;
	writel(reg, pmu_cfg);

	/* Forcely incur PMU watchdog reset */
	writel(0x0U, pmu_wdtctrl);
	writel(0x1U, pmu_wdtctrl);

	while (true) {
		writel(PMU_WDTCTRL_EN, pmu_wdtctrl);
	}
}

DT_MACHINE_START(TCC897X_DT, "Telechips TCC897x (Flattened Device Tree)")
	.dt_compat	= tcc897x_dt_compat,
	.smp		= smp_ops(tcc_smp_ops),
	.smp_init	= smp_init_ops(tcc_smp_init_ops),
	.reserve	= tcc_mem_reserve,
	.map_io		= tcc_map_common_io,
	.init_machine	= tcc897x_dt_init,
	.restart	= tcc897x_reset_cpu,
MACHINE_END

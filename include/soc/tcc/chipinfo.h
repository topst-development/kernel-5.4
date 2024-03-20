/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef SOC_TCC_CHIPINFO_H
#define SOC_TCC_CHIPINFO_H

#include <linux/types.h>

#define INFO_UNK (~((u32)0))

#if defined(CONFIG_HAVE_TCC_SIP_SERVICE)
#include <soc/tcc/tcc-sip.h>
#elif defined(CONIFG_HAVE_PLAT_TCC_CHIPINFO)
#include <plat/chipinfo.h>
#endif

/* Data structure for chip info and getters for it */
struct chip_info {
	u32 rev;
	u32 name;
};

static inline u32 get_chip_rev(void)
{
#if defined(CONFIG_HAVE_TCC_SIP_SERVICE)
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_REV, 0, 0, 0, 0, 0, 0, 0, &res);

	return ensure_u32(res.a0);
#elif defined(HAVE_PLAT_GET_CHIP_REV)
	return plat_get_chip_rev();
#else
	return INFO_UNK;
#endif
}

static inline u32 get_chip_name(void)
{
#if defined(CONFIG_HAVE_TCC_SIP_SERVICE)
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_NAME, 0, 0, 0, 0, 0, 0, 0, &res);

	return ensure_u32(res.a0);
#elif defined(HAVE_PLAT_GET_CHIP_NAME)
	return plat_get_chip_name();
#else
	return INFO_UNK;
#endif
}

static inline u32 get_chip_family(void)
{
	u32 name = get_chip_name();

	return name & 0xFFF0U;
}

static inline void get_chip_info(struct chip_info *info)
{
	info->rev = get_chip_rev();
	info->name = get_chip_name();
}

/* Data structure for boot info and getters for it */
struct boot_info {
	u32 bootsel;
	u32 coreid;
};

static inline u32 get_boot_sel(void)
{
#if defined(CONFIG_HAVE_TCC_SIP_SERVICE)
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_GET_BOOT_INFO, 0, 0, 0, 0, 0, 0, 0, &res);

	return (res.a0 == SMC_OK) ? ensure_u32(res.a1) : INFO_UNK;
#elif defined(HAVE_PLAT_GET_BOOT_SEL)
	return plat_get_boot_sel();
#else
	return INFO_UNK;
#endif
}

static inline bool is_dual_boot(u32 bootsel)
{
	return bootsel == 0U; /* Dual boot is default boot selection */
}

static inline u32 get_core_identity(void)
{
#if defined(CONFIG_HAVE_TCC_SIP_SERVICE)
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CHIP_GET_BOOT_INFO, 0, 0, 0, 0, 0, 0, 0, &res);

	return (res.a0 == SMC_OK) ? ensure_u32(res.a2) : INFO_UNK;
#elif defined(HAVE_PLAT_GET_CORE_IDENTITY)
	return plat_get_core_identity();
#else
	return INFO_UNK;
#endif
}

static inline bool is_main_core(u32 coreid)
{
	return coreid == 0U; /* Main-core is default core identity */
}

static inline void get_boot_info(struct boot_info *info)
{
	info->bootsel = get_boot_sel();
	info->coreid = get_core_identity();
}

/* Data structure for Trusted Firmware version info and getters for it */
struct version_info {
	u32 major;
	u32 minor;
	u32 patch;
	u32 id;
	bool dirty;
};

static inline bool version_compat(const struct version_info *ver,
				  const u32 x, const u32 y, const u32 z)
{
	return (ver->major > x) ||
		((ver->major == x) && ((ver->minor > y) ||
		((ver->minor == y) && (ver->patch >= z))));
}

static inline void get_tf_version(struct version_info *ver)
{
#if defined(CONFIG_HAVE_TCC_SIP_SERVICE)
	struct arm_smccc_res res;
	bool extra_info;

	arm_smccc_smc(SIP_SVC_VERSION, 0, 0, 0, 0, 0, 0, 0, &res);

	ver->major = ensure_u32(res.a0);
	ver->minor = ensure_u32(res.a1);
	ver->patch = ensure_u32(res.a2);

#if defined(CONFIG_ARCH_TCC805X)
	extra_info = version_compat(ver, 0U, 1U, 32U);
#elif defined(CONFIG_ARCH_TCC803X)
	extra_info = version_compat(ver, 0U, 1U, 10U);
#else
	extra_info = (bool)false;
#endif

	if (extra_info) {
		ver->id = ensure_u32(res.a3) & 0x0FFFFFFFU;
		ver->dirty = ((ensure_u32(res.a3) & 0x80000000U) != 0U);
	} else {
		ver->id = INFO_UNK;
		ver->dirty = (bool)false;
	}
#else
	ver->major = INFO_UNK;
	ver->minor = INFO_UNK;
	ver->patch = INFO_UNK;
	ver->id = INFO_UNK;
	ver->dirty = (bool)false;
#endif
}

#endif /* SOC_TCC_CHIPINFO_H */

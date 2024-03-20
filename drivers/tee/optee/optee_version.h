/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Telechips Inc
 */

#ifndef OPTEE_VERSION_H
#define OPTEE_VERSION_H

#include <linux/tee_drv.h>


/**
 * struct tee_ioctl_version_tcc - send sdk version to kernel
 * @major:	[in] Major revision
 * @minor:	[in] Minor revision
 * @rev0:	[in] reserved0
 * @rev1:	[in] reserved1
 */
struct tee_ioctl_version_tcc {
	__u32 major;
	__u32 minor;
	__u32 tcc_rev;
	__u64 date;
};

/**
 * TEE_IOC_CALIB_VERSION - calib libranry version
 */
#define TEE_IOC_OS_VERSION	_IOWR(TEE_IOC_MAGIC, TEE_IOC_BASE + 21, \
				      struct tee_ioctl_version_tcc)

/**
 * TEE_IOC_SUPP_VERSION - tee supplicant version
 */
#define TEE_IOC_SUPP_VERSION	_IOWR(TEE_IOC_MAGIC, TEE_IOC_BASE + 22, \
				      struct tee_ioctl_version_tcc)

/**
 * TEE_IOC_CLIENT_VERSION - tee client library version
 */
#define TEE_IOC_CLIENT_VERSION	_IOWR(TEE_IOC_MAGIC, TEE_IOC_BASE + 23, \
				      struct tee_ioctl_version_tcc)


void tee_set_version(unsigned int cmd, struct tee_ioctl_version_tcc *ver, \
		     uint32_t id);

#endif /* OPTEE_VERSION_H */

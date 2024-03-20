/*
 * Copyright (C) 2015-2016 Intel Corp. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */
#ifndef __RPMB_H__
#define __RPMB_H__

#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <uapi/linux/rpmb.h>
#include <linux/kref.h>

/**
 * enum rpmb_type - type of underlaying storage technology
 *
 * @RPMB_TYPE_ANY   : any type used for search only
 * @RPMB_TYPE_EMMC  : emmc (JESD84-B50.1)
 * @RPMB_TYPE_UFS   : UFS (JESD220)
 * @RPMB_TYPE_MAX   : upper sentinel
 */
enum rpmb_type {
	RPMB_TYPE_ANY = 0,
	RPMB_TYPE_EMMC,
	RPMB_TYPE_UFS,
	RPMB_TYPE_MAX = RPMB_TYPE_UFS
};

extern struct class rpmb_class;

/**
 * struct rpmb_data - rpmb data be transmitted in RPMB request
 *
 * @in_frames     :  list of input frames
 * @out_frames    :  list of result frames
 * @in_frames_cnt :  count of the input frames
 * @out_frames_cnt:  count of the output frames
 * @req_type      :  request type (program key, read, write, write counter)
 */
struct rpmb_data {
	struct rpmb_frame *in_frames;
	struct rpmb_frame *out_frames;
	u32 in_frames_cnt;
	u32 out_frames_cnt;
	u16 req_type;
};

struct ufs_desc_info {
       char model[17];
       u8 bRPMB_ReadWriteSize;
       u8 bRPMBRegion0Size;
};

struct rpmb_info_data {
	struct ufs_desc_info *ufs_info;
	u16 req_type;
};
/**
 * struct rpmb_ops - RPMB ops to be implemented by underlaying block device
 *
 * @send_rpmb_req  : send RPMB request to RPBM partition backed by the disk
 * @type           : block device type
 * @dev_id         : unique device identifier
 * @dev_id_len     : unique device identifier length
 * @reliable_wr_cnt: number of sectors that can be written in one access
 */
struct rpmb_ops {
	int (*send_rpmb_req)(struct device *dev, struct rpmb_data *req);
	int (*send_rpmb_info_req)(struct device *dev, struct rpmb_info_data *req);
	enum rpmb_type type;
	const u8 *dev_id;
	size_t dev_id_len;
	u16 reliable_wr_cnt;
};

/**
 * struct rpmb_dev - device which can support RPMB partition
 *
 * @lock       : lock
 * @dev        : device
 * @id         : device id
 * @cdev       : character dev
 * @status     : device status
 * @ops        : operation exported by block layer
 */
struct rpmb_dev {
	struct mutex lock;
	struct device dev;
	int    id;
#ifdef CONFIG_RPMB_INTF_DEV
	struct cdev cdev;
	unsigned long status;
#endif /* CONFIG_RPMB_INTF_DEV */
	const struct rpmb_ops *ops;
};

#define to_rpmb_dev(x) container_of((x), struct rpmb_dev, dev)

#if IS_ENABLED(CONFIG_RPMB)
struct rpmb_dev *rpmb_dev_get(struct rpmb_dev *rdev);
void rpmb_dev_put(struct rpmb_dev *rdev);
struct rpmb_dev *rpmb_dev_find_by_device(struct device *parent);
struct rpmb_dev *rpmb_dev_get_by_type(enum rpmb_type type);
struct rpmb_dev *rpmb_dev_register(struct device *dev,
				   const struct rpmb_ops *ops);
struct rpmb_dev *rpmb_dev_find_device(void *data,
			int (*match)(struct device *dev, const void *data));
int rpmb_dev_unregister(struct device *dev);
int rpmb_send_req(struct rpmb_dev *rdev, struct rpmb_data *data);
int rpmb_send_info_req(struct rpmb_dev *rdev, struct rpmb_info_data *data);

#else
static inline struct rpmb_dev *rpmb_dev_get(struct rpmb_dev *rdev)
{
	return NULL;
}
static inline void rpmb_dev_put(struct rpmb_dev *rdev) { }
static inline struct rpmb_dev *rpmb_dev_find_by_device(struct device *parent)
{
	return NULL;
}
static inline
struct rpmb_dev *rpmb_dev_get_by_type(enum rpmb_type type)
{
	return NULL;
}
static inline struct rpmb_dev *
rpmb_dev_register(struct device *dev, const struct rpmb_ops *ops)
{
	return NULL;
}
static inline int rpmb_dev_unregister(struct device *dev)
{
	return 0;
}
static inline int rpmb_send_req(struct rpmb_dev *rdev, struct rpmb_data *data)
{
	return 0;
}
#endif /* CONFIG_RPMB */

#endif /* __RPMB_H__ */
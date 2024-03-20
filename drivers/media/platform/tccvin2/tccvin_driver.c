// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *      tccvin_driver.c  --  Telechips Video-Input Path Driver
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2020


 *   Description : Driver management


 *****************************************************************************/

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <asm/unaligned.h>

#include <media/v4l2-common.h>

#include "tccvin_video.h"
#include "tccvin_diagnostics.h"

/* coverity[HIS_metric_violation : FALSE] */
static inline struct platform_device *
device_to_platform_device(struct device *dev)
{
	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	return container_of(dev, struct platform_device, dev);
}

/* coverity[HIS_metric_violation : FALSE] */
static inline struct tccvin_device *
device_to_tccvin_device(struct device *dev)
{
	const struct platform_device		*pdev		= NULL;

	pdev		= device_to_platform_device(dev);

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	return (struct tccvin_device *)platform_get_drvdata(pdev);
}

/* coverity[HIS_metric_violation : FALSE] */
static inline struct tccvin_stream *
device_to_tccvin_stream(struct device *dev)
{
	struct tccvin_device			*tdev		= NULL;

	tdev		= device_to_tccvin_device(dev);

	return &tdev->vstream;
}

/*
 * Delete the tccvin device.
 *
 * Called by the kernel when the last reference to the tccvin_device structure
 * is released.
 */
static void tccvin_delete(struct kref *ref)
{
	struct tccvin_device			*tdev		= NULL;
	const struct tccvin_stream		*vstream	= NULL;

	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	tdev		= container_of(ref, struct tccvin_device, ref);
	vstream		= &tdev->vstream;

	if (tdev->v4ldev.dev != NULL) {
		/* unregister v4l2 device */
		v4l2_device_unregister(&tdev->v4ldev);
	}

	kfree(tdev);
}

static void tccvin_release(struct video_device *vdev)
{
	const struct device			*dev		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	struct tccvin_device			*tdev		= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	vstream		= (struct tccvin_stream *)video_get_drvdata(vdev);
	dev		= stream_to_device(vstream);
	tdev		= vstream->tdev;

	/* decrease reference count if device is unregistered */
	ret = kref_put(&tdev->ref, tccvin_delete);
	if (ret == 1) {
		/* the object was removed */
		logd(dev, "the object was removed\n");
	}
}

static int tccvin_init_stream_data(struct tccvin_stream *vstream,
	const struct platform_device *pdev)
{
	const struct device			*dev		= NULL;
	int					ret_call	= 0;
	int					ret		= 0;

	dev		= stream_to_device(vstream);

	ret_call = tccvin_video_init(vstream, pdev->dev.of_node);
	if (ret_call < 0) {
		loge(dev, "tccvin_video_init, ret: %d\n", ret_call);
		ret = -1;
	}

	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	mutex_init(&vstream->mlock);

	/* Set the driver data before calling video_register_device, otherwise
	 * tccvin_v4l2_open might race us.
	 */
	video_set_drvdata(&vstream->tdev->vdev, vstream);

	return ret;
}

static int tccvin_init_device_data(struct tccvin_device *tdev,
	struct platform_device *pdev)
{
	const struct device			*dev		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	int					ret_call	= 0;
	int					ret		= 0;

	dev		= &pdev->dev;
	vstream		= &tdev->vstream;

	tdev->pdev = pdev;
	tdev->id = of_alias_get_id(pdev->dev.of_node, "videoinput");
	(void)scnprintf((char *)tdev->name, PAGE_SIZE, pdev->name);
	logi(dev, "id: %d, name: %s\n", tdev->id, tdev->name);

	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	mutex_init(&tdev->mlock);
	kref_init(&tdev->ref);

	vstream->tdev = tdev;
	ret_call = tccvin_init_stream_data(vstream, pdev);
	if (ret_call < 0) {
		loge(dev, "tccvin_init_stream_data, ret: %d\n", ret_call);
		ret = -1;
	}

	return ret;
}

static int tccvin_register_video_device(struct tccvin_device *tdev,
	struct platform_device *pdev)
{
	const struct device			*dev		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	struct video_device			*vdev		= NULL;
	bool					drop_corrupted	= FALSE;
	int					ret_call	= 0;
	int					ret		= 0;

	vstream		= &tdev->vstream;
	dev		= stream_to_device(vstream);
	vdev		= &vstream->tdev->vdev;

	ret_call = v4l2_device_register(&pdev->dev, &tdev->v4ldev);
	if (ret_call != 0) {
		loge(dev, "v4l2_device_register, ret: %d\n", ret_call);
		ret = -1;
	}

	ret_call = tccvin_v4l2_init_queue(&vstream->queue, drop_corrupted);
	if (ret_call != 0) {
		loge(dev, "tccvin_v4l2_init_queue, ret: %d\n", ret_call);
		ret = -1;
	}

	/* We already hold a reference to dev->udev. The video device will be
	 * unregistered before the reference is released, so we don't need to
	 * get another one.
	 */
	vdev->v4l2_dev		= &tdev->v4ldev;
	vdev->fops		= &tccvin_fops;
	vdev->ioctl_ops		= &tccvin_ioctl_ops;
	vdev->minor		= tdev->id;
	vdev->release		= tccvin_release;
	vdev->device_caps	= (unsigned int)V4L2_CAP_STREAMING |
				  (unsigned int)V4L2_CAP_VIDEO_CAPTURE_MPLANE;
	(void)strscpy(vdev->name, (const char *)&tdev->name[0], sizeof(vdev->name));

	ret_call = video_register_device(vdev, VFL_TYPE_GRABBER, vdev->minor);
	if (ret_call < 0) {
		loge(dev, "video_register_device, ret: %d\n", ret_call);
		ret = -1;
	} else {
		/* increase reference count if device is registered */
		kref_get(&tdev->ref);
	}

	return ret;
}

static void tccvin_unregister_video_device(struct tccvin_device *tdev)
{
	const struct device			*dev		= NULL;
	const struct tccvin_stream		*vstream	= NULL;

	vstream		= &tdev->vstream;
	dev		= stream_to_device(vstream);

	if (video_is_registered(&vstream->tdev->vdev) != 0) {
		/* unregister video device  */
		video_unregister_device(&vstream->tdev->vdev);
	}

	if (tdev->v4ldev.dev != NULL) {
		/* unregister v4l2 device */
		v4l2_device_unregister(&tdev->v4ldev);
	}
}

static int tccvin_probe_device(struct tccvin_device *tdev,
	struct platform_device *pdev)
{
	const struct device			*dev		= NULL;
	int					ret_call	= 0;
	int					ret		= 0;

	dev		= &pdev->dev;

	ret_call = tccvin_init_device_data(tdev, pdev);
	if (ret_call != 0) {
		loge(dev, "tccvin_init_device_data, ret: %d\n", ret_call);
		ret = -1;
	} else {
		ret_call = tccvin_register_video_device(tdev, pdev);
		if (ret_call != 0) {
			loge(dev, "tccvin_register_video_device, ret: %d\n", ret_call);
			ret = -1;
		}
	}

	return ret;
}

static void tccvin_remove_device(struct tccvin_device *tdev)
{
	const struct device			*dev		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	int					ret_call	= 0;

	vstream		= &tdev->vstream;
	dev		= stream_to_device(vstream);

	tccvin_unregister_video_device(tdev);

	ret_call = tccvin_video_deinit(vstream);
	if (ret_call != 0) {
		/* error: tccvin_video_deinit */
		loge(dev, "tccvin_video_deinit, ret: %d\n", ret_call);
	}
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_notifier_bound(struct v4l2_async_notifier *notifier,
	struct v4l2_subdev *subdev, struct v4l2_async_subdev *asd)
{
	const struct device			*dev		= NULL;
	struct tccvin_subdev			*tsubdev	= NULL;
	const struct tccvin_device		*tdev		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	tsubdev		= container_of(notifier, struct tccvin_subdev, notifier);
	tdev		= tsubdev->tdev;
	vstream		= &tdev->vstream;
	dev		= stream_to_device(vstream);

	logi(dev, "v4l2-subdev %s is bounded\n", subdev->name);

	/* register subdevice here */
	tsubdev->sd = subdev;

#if defined(CONFIG_VIDEO_TCCVIN2_DIAG)
	ret = tccvin_diag_cif_port(subdev);
#endif/* defined(CONFIG_VIDEO_TCCVIN2_DIAG) */

	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_notifier_complete(struct v4l2_async_notifier *notifier)
{
	const struct device			*dev		= NULL;
	const struct tccvin_subdev		*tsubdev	= NULL;
	struct tccvin_device			*tdev		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	int					ret_call	= 0;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	tsubdev		= container_of(notifier, struct tccvin_subdev, notifier);
	tdev		= tsubdev->tdev;
	vstream		= &tdev->vstream;
	dev		= stream_to_device(vstream);

	ret_call = v4l2_device_register_subdev_nodes(&tdev->v4ldev);
	if (ret_call != 0) {
		loge(dev, "v4l2_device_register_subdev_nodes, ret: %d\n", ret_call);
		ret = -1;
	}

	return ret;
}

static const struct v4l2_async_notifier_operations tccvin_notifier_ops = {
	.bound		= tccvin_notifier_bound,
	.complete	= tccvin_notifier_complete,
};

static int tccvin_parse_subdevs(struct tccvin_device *tdev, const struct device_node *root_node)
{
	const struct device			*dev		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	struct device_node			*loc_ep		= NULL;
	struct device_node			*rem_ep		= NULL;
	int					ret		= 0;

	vstream		= &tdev->vstream;
	dev		= stream_to_device(vstream);

	/* get phandle of subdev */
	loc_ep = of_graph_get_endpoint_by_regs(root_node, -1, -1);
	if (loc_ep == NULL) {
		loge(dev, "No subdev is found\n");
		ret = -EINVAL;
	} else {
		/* get phandle of remote subdev */
		rem_ep = of_graph_get_remote_endpoint(loc_ep);

		tdev->tsubdev.asd.match_type = V4L2_ASYNC_MATCH_FWNODE;
		/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		tdev->tsubdev.asd.match.fwnode = of_fwnode_handle(rem_ep);

		of_node_put(rem_ep);
		of_node_put(loc_ep);
	}

	return ret;
}

static int tccvin_register_subdevs(struct tccvin_device *tdev)
{
	const struct device			*dev		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	struct tccvin_subdev			*tsubdev	= NULL;
	int					ret_call	= 0;
	int					ret		= 0;

	vstream		= &tdev->vstream;
	dev		= stream_to_device(vstream);
	tsubdev		= &tdev->tsubdev;

	v4l2_async_notifier_init(&tsubdev->notifier);

	tsubdev->notifier.ops	= &tccvin_notifier_ops;
	tsubdev->tdev		= tdev;

	/* add a v4l2-subdev to a notifier */
	ret_call = v4l2_async_notifier_add_subdev(&tsubdev->notifier, &tsubdev->asd);
	if (ret_call != 0) {
		loge(dev, "v4l2_async_notifier_add_subdev, ret: %d\n", ret_call);
		ret = -1;
	} else {
		/* register a notifier */
		ret_call = v4l2_async_notifier_register(vstream->tdev->vdev.v4l2_dev, &tsubdev->notifier);
		if (ret_call != 0) {
			loge(dev, "v4l2_async_notifier_register, ret: %d\n", ret_call);
			v4l2_async_notifier_cleanup(&tsubdev->notifier);
			ret = -1;
		}
	}

	return ret;
}

static void tccvin_unregister_subdevs(struct tccvin_device *tdev)
{
	struct tccvin_subdev			*tsubdev	= NULL;

	tsubdev		= &tdev->tsubdev;

	v4l2_async_notifier_unregister(&tsubdev->notifier);

	v4l2_async_notifier_cleanup(&tsubdev->notifier);
}

static int tccvin_probe_subdevs(struct tccvin_device *tdev, const struct device_node *root_node)
{
	const struct device			*dev		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	int					ret_call	= 0;
	int					ret		= 0;

	vstream		= &tdev->vstream;
	dev		= stream_to_device(vstream);

	ret_call = tccvin_parse_subdevs(tdev, root_node);
	if (ret_call != 0) {
		loge(dev, "tccvin_parse_subdevs, ret: %d\n", ret_call);
		ret = -1;
	} else {
		ret_call = tccvin_register_subdevs(tdev);
		if (ret_call != 0) {
			loge(dev, "tccvin_register_subdevs, ret: %d\n", ret_call);
			ret = -1;
		}
	}

	return ret;
}

static void tccvin_remove_subdevs(struct tccvin_device *tdev)
{
	tccvin_unregister_subdevs(tdev);
}

/* ------------------------------------------------------------------------
 * sysfs
 */

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t recovery_trigger_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	const struct tccvin_stream		*vstream	= NULL;

	vstream		= device_to_tccvin_stream(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", vstream->cif.recovery_trigger);
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
/* coverity[HIS_metric_violation : FALSE] */
static ssize_t recovery_trigger_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tccvin_stream			*vstream	= NULL;
	struct tccvin_cif			*cif		= NULL;
	const void __iomem			*wdma		= NULL;
	unsigned int				val		= 0;
	int					ret_call	= 0;
	int					ret		= 0;

	vstream		= device_to_tccvin_stream(dev);
	cif		= &vstream->cif;
	wdma		= VIOC_WDMA_GetAddress(cif->vin_path[VIN_COMP_WDMA].index);

	ret = kstrtou32(buf, 0, &val);
	if (ret == 0) {
		/* update */
		if (val != 0U) {
			/* trigger recovery */
			ret_call = tccvin_video_trigger_recovery(vstream, val);
			if (ret_call != 0) {
				/* failed to trigger recovery */
				loge(dev, "tccvin_video_trigger_recovery, ret: %d\n", ret_call);
			}
		}

		cif->recovery_trigger = val;
	}

	return (ret > 0) ? (ssize_t)ret : u64_to_s64(count);
}

/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
static DEVICE_ATTR_RW(recovery_trigger);

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t vs_stabilization_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	const struct tccvin_stream		*vstream	= NULL;

	vstream		= device_to_tccvin_stream(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", vstream->vs_stabilization);
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t vs_stabilization_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tccvin_stream			*vstream	= NULL;
	unsigned int				val		= 0;
	int					ret		= 0;

	vstream		= device_to_tccvin_stream(dev);

	ret = kstrtou32(buf, 0, &val);
	if (ret == 0) {
		/* update */
		vstream->vs_stabilization = val;
	}

	return (ret > 0) ? (ssize_t)ret : u64_to_s64(count);
}

/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
static DEVICE_ATTR_RW(vs_stabilization);

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t timestamp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	const struct tccvin_stream		*vstream	= NULL;

	vstream		= device_to_tccvin_stream(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", vstream->timestamp);
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t timestamp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tccvin_stream			*vstream	= NULL;
	unsigned int				val		= 0;
	int					ret		= 0;

	vstream		= device_to_tccvin_stream(dev);

	ret = kstrtou32(buf, 0, &val);
	if (ret == 0) {
		/* update */
		vstream->timestamp = val;
	}

	return (ret > 0) ? (ssize_t)ret : u64_to_s64(count);
}

/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
static DEVICE_ATTR_RW(timestamp);

static struct attribute *tccvin_attrs[] = {
	&dev_attr_recovery_trigger.attr,
	&dev_attr_vs_stabilization.attr,
	&dev_attr_timestamp.attr,
	NULL,
};
ATTRIBUTE_GROUPS(tccvin);

/* ------------------------------------------------------------------------
 * Driver initialization and cleanup
 */

static int tccvin_probe(struct platform_device *pdev)
{
	const struct device			*dev		= NULL;
	struct tccvin_device			*tdev		= NULL;
	int					ret_call	= 0;
	int					ret		= 0;

	dev		= &pdev->dev;

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	tdev = kzalloc(sizeof(*tdev), GFP_KERNEL);
	if (tdev == NULL) {
		loge(dev, "kzalloc(tdev)\n");
		ret = -ENOMEM;
	} else {
		ret_call = tccvin_probe_device(tdev, pdev);
		if (ret_call != 0) {
			loge(dev, "tccvin_probe_device, ret: %d\n", ret_call);
			ret = -1;
		} else {
			(void)tccvin_v4l2_init_format(tdev);

			ret_call = tccvin_probe_subdevs(tdev, pdev->dev.of_node);
			if (ret_call != 0) {
				loge(dev, "tccvin_probe_subdevs, ret: %d\n", ret_call);
				ret = -1;
			}
		}

		platform_set_drvdata(pdev, tdev);
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_remove(struct platform_device *pdev)
{
	const struct device			*dev		= NULL;
	struct tccvin_device			*tdev		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	int					ret_call	= 0;
	int					ret		= 0;

	dev		= &pdev->dev;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	tdev		= (struct tccvin_device *)platform_get_drvdata(pdev);
	vstream		= &tdev->vstream;

	tccvin_remove_subdevs(tdev);

	tccvin_remove_device(tdev);

	ret_call = kref_put(&tdev->ref, tccvin_delete);
	if (ret_call != 0) {
		loge(dev, "kref_put, ret: %d\n", ret);
		ret = -1;
	}

	return ret;
}

const static struct of_device_id tccvin_of_match[] = {
	{ .compatible = "telechips,video-input" },
};

MODULE_DEVICE_TABLE(of, tccvin_of_match);

static struct platform_driver tccvin_driver = {
	.probe		= tccvin_probe,
	.remove		= tccvin_remove,
	.driver		= {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(tccvin_of_match),
		.dev_groups	= tccvin_groups,
	},
};

/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
module_platform_driver(tccvin_driver);

/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_AUTHOR("Telechips.Co.Ltd");
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_DESCRIPTION("Telechips Video-Input Path(V4L2-Capture) Driver");
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_LICENSE("GPL");
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
MODULE_VERSION(DRIVER_VERSION);

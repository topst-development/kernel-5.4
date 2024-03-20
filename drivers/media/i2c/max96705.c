// SPDX-License-Identifier: GPL-2.0
/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of_graph.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/v4l2-async.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <linux/kdev_t.h>
#include <linux/of_gpio.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-subdev.h>
#include <video/tcc/vioc_vin.h>

#define LOG_TAG				"VSRC:MAX96705"

#define loge(fmt, ...)			\
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)			\
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)			\
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)			\
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)



/*
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct max96705 {
	struct v4l2_subdev		sd;
	struct v4l2_mbus_framefmt	fmt;

	struct fwnode_handle		*fwnode;
	struct v4l2_async_subdev	asd;
	struct v4l2_async_notifier	notifier;
	struct v4l2_subdev		*remote_sd;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;
};

/* for yuv422 HD camera module*/
const struct reg_sequence max96705_reg_defaults[] = {
};

/* for raw12 ispless camera module */
const struct reg_sequence max96705_reg_defaults_raw12[] = {
};

const struct reg_sequence max96705_reg_defaults_raw8[] = {
	//MAX96705 SER
	{0x04, 0x47, 10*1000},	/* CLINK EN = 1, Delay 10ms */
	{0x07, 0xA4, 0},	/* SER BWS=1, HVEN=1 */
	{0x20, 0x04, 0},	/* Bit Shift */
	{0x21, 0x05, 0},	/* Bit Shift */
	{0x22, 0x06, 0},	/* Bit Shift */
	{0x23, 0x07, 0},	/* Bit Shift */
	{0x24, 0x08, 0},	/* Bit Shift */
	{0x25, 0x09, 0},	/* Bit Shift */
	{0x26, 0x0A, 0},	/* Bit Shift */
	{0x27, 0x0B, 0},	/* Bit Shift */
	{0x30, 0x04, 0},	/* Bit Shift */
	{0x31, 0x05, 0},	/* Bit Shift */
	{0x32, 0x06, 0},	/* Bit Shift */
	{0x33, 0x07, 0},	/* Bit Shift */
	{0x34, 0x08, 0},	/* Bit Shift */
	{0x35, 0x09, 0},	/* Bit Shift */
	{0x36, 0x0A, 0},	/* Bit Shift */
	{0x37, 0x0B, 0},	/* Bit Shift */
	{0x67, 0xc4, 0},	/* DBL Align */
	{0x0f, 0x3c, 10*1000},	/* RESET "L", Delay 10ms */
	{0x0f, 0x3e, 0},	/* RESET "H" */
};

static const struct regmap_config max96705_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

static int max96705_notifier_bound(struct v4l2_async_notifier *notifier,
	struct v4l2_subdev *subdev,
	struct v4l2_async_subdev *asd)
{
	struct max96705		*dev		= NULL;
	int			ret		= 0;

	dev = container_of(notifier, struct max96705, notifier);

	logi("v4l2-subdev %s is bounded\n", subdev->name);

	dev->remote_sd = subdev;

	return ret;
}

static void max96705_notifier_unbind(struct v4l2_async_notifier *notifier,
			struct v4l2_subdev *subdev,
			struct v4l2_async_subdev *asd)
{
	struct max96705	*dev		= NULL;

	dev = container_of(notifier, struct max96705, notifier);

	logi("v4l2-subdev %s is unbounded\n", subdev->name);
}

static const struct v4l2_async_notifier_operations max96705_notifier_ops = {
	.bound = max96705_notifier_bound,
	.unbind = max96705_notifier_unbind,
};

static int max96705_register_subdev_notifier(struct max96705 *dev, struct device_node *node)
{
	struct v4l2_async_notifier *notifier = &dev->notifier;
	struct device_node *local_ep = NULL;
	struct device_node *remote_ep = NULL;
	struct device_node *remote_node = NULL;
	struct v4l2_async_subdev *asd;
	int ret = 0;

	v4l2_async_notifier_init(&dev->notifier);
	notifier->ops = &max96705_notifier_ops;

	/* get phandle of subdev */
	local_ep = of_graph_get_endpoint_by_regs(node, -1, -1);
	if (!local_ep) {
		loge("Not connected to subdevice\n");
		return -EINVAL;
	}

	/* get phandle of video-input path's fwnode */
	dev->fwnode = of_fwnode_handle(local_ep);

	/* get phandle of remote subdev */
	remote_ep = of_graph_get_remote_endpoint(local_ep);

	/* get phandle of remote subdev's parent */
	remote_node = of_graph_get_remote_port_parent(local_ep);
	logi("remote node: %s\n", remote_node->name);

	asd = &dev->asd;

	asd->match_type = V4L2_ASYNC_MATCH_FWNODE;
	asd->match.fwnode = of_fwnode_handle(remote_node);

	of_node_put(remote_node);
	of_node_put(remote_ep);

	ret = v4l2_async_notifier_add_subdev(&dev->notifier, &dev->asd);
	if (ret) {
		loge("v4l2_async_notifier_add_subdev, ret: %d\n", ret);
		goto end;
	}

	ret = v4l2_async_subdev_notifier_register(&dev->sd, notifier);
	if (ret < 0) {
		loge("v4l2_async_subdev_notifier_register, ret: %d\n", ret);
		v4l2_async_notifier_cleanup(notifier);
		ret = -EINVAL;
		goto end;
	}

	logi("Succeed to register async notifier\n");

end:
	of_node_put(local_ep);
	return ret;
}

static void max96705_init_format(struct max96705 *dev)
{
	dev->fmt.width		= 1280;
	dev->fmt.height		= 800,
	dev->fmt.code		= MEDIA_BUS_FMT_Y8_1X8;
	dev->fmt.field		= V4L2_FIELD_NONE;
	dev->fmt.colorspace	= V4L2_COLORSPACE_SMPTE170M;
}

/*
 * Helper functions for reflection
 */
static inline struct max96705 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max96705, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int max96705_init(struct v4l2_subdev *sd, u32 enable)
{
	struct max96705		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if (enable == 1) {
		if (dev->i_cnt == 0) {
			if (dev->fmt.code == MEDIA_BUS_FMT_SGRBG12_1X12) {
				logi("input format is bayer raw\n");
				ret = regmap_multi_reg_write(dev->regmap,
					max96705_reg_defaults_raw12,
					ARRAY_SIZE(max96705_reg_defaults_raw12));
			} else if (dev->fmt.code == MEDIA_BUS_FMT_Y8_1X8) {
				/* format is MEDIA_BUS_FMT_Y8_1X8 */
				logi("input format is RAW8\n");
				ret = regmap_multi_reg_write(dev->regmap,
					max96705_reg_defaults_raw8,
					ARRAY_SIZE(max96705_reg_defaults_raw8));
			} else {
				logi("input format is yuv422\n");
				ret = regmap_multi_reg_write(dev->regmap,
					max96705_reg_defaults,
					ARRAY_SIZE(max96705_reg_defaults));
			}
			if (ret < 0)
				loge("Fail initializing max96705 device\n");
		}
		dev->i_cnt++;
	} else {
		dev->i_cnt--;
		if (dev->i_cnt == 0) {
			/* de-init */
			/* ret = regmap_write(dev->regmap, 0x15, 0x93); */
		}
	}

	mutex_unlock(&dev->lock);

	return ret;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int max96705_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct max96705		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if (enable == 1) {
		if (dev->s_cnt == 0) {
			ret = regmap_write(dev->regmap, 0x04, 0x87);
			if (ret < 0) {
				/* failure of s_stream */
				loge("Fail Serialization max96705 device\n");
			}
		}
		/* count up */
		dev->s_cnt++;
	} else {
		/* count down */
		dev->s_cnt--;
		if (dev->s_cnt == 0) {
			/* stream off */
			ret = regmap_write(dev->regmap, 0x04, 0x47);
		}
	}

	msleep(30);

	mutex_unlock(&dev->lock);
	return ret;
}

static int max96705_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct max96705		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	memcpy((void *)&format->format, (const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

static int max96705_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct max96705		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	memcpy((void *)&dev->fmt, (const void *)&format->format,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static const struct v4l2_subdev_core_ops max96705_core_ops = {
	.init			= max96705_init,
};

static const struct v4l2_subdev_video_ops max96705_video_ops = {
	.s_stream		= max96705_s_stream,
};

static const struct v4l2_subdev_pad_ops max96705_pad_ops = {
	.get_fmt		= max96705_get_fmt,
	.set_fmt		= max96705_set_fmt,
};

static const struct v4l2_subdev_ops max96705_ops = {
	.core			= &max96705_core_ops,
	.video			= &max96705_video_ops,
	.pad			= &max96705_pad_ops,
};

struct max96705 max96705_data = {
};

static const struct i2c_device_id max96705_id[] = {
	{ "max96705", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max96705_id);

#if IS_ENABLED(CONFIG_OF)
const static struct of_device_id max96705_of_match[] = {
	{
		.compatible	= "maxim,max96705",
		.data		= &max96705_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max96705_of_match);
#endif

int max96705_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max96705			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct max96705), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(max96705_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &max96705_ops);

	/* register a v4l2 sub device */
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret)
		loge("Failed to register subdevice\n");
	else
		logi("%s is registered as a v4l2 sub device.\n", dev->sd.name);

	ret = max96705_register_subdev_notifier(dev, client->dev.of_node);
	if (ret < 0) {
		/* Failure of registering subdev notifier */
		loge("max96705_register_subdev_notifier, ret: %d\n", ret);
	}

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &max96705_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	/* init format info */
	max96705_init_format(dev);

	goto goto_end;

goto_free_device_data:
	/* free the videosource data */
	kfree(dev);

goto_end:
	return ret;
}

int max96705_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max96705		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver max96705_driver = {
	.probe		= max96705_probe,
	.remove		= max96705_remove,
	.driver		= {
		.name		= "max96705",
		.of_match_table	= of_match_ptr(max96705_of_match),
	},
	.id_table	= max96705_id,
};

module_i2c_driver(max96705_driver);

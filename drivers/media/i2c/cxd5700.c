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

#define LOG_TAG				"VSRC:CXD5700"

#define loge(fmt, ...)			\
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)			\
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)			\
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)			\
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define DEFAULT_WIDTH			(1920)
#define DEFAULT_HEIGHT			(1080)

#define	DEFAULT_FRAMERATE		(30)

struct frame_size {
	u32 width;
	u32 height;
};

/*
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct cxd5700 {
	struct v4l2_subdev		sd;

	struct v4l2_async_subdev	asd;
	struct v4l2_async_notifier	notifier;
	struct v4l2_subdev		*remote_sd;

	struct v4l2_mbus_framefmt	fmt;
	int				framerate;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int i_cnt;
};

const char cxd5700_reg_defaults[] = {
	/* 0x0A, */0x01, 0x07, 0x02, 0x01, 0x00, 0x00, 0x30, 0x80, 0xC5,
};

static const struct regmap_config cxd5700_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

static struct frame_size cxd5700_framesizes[] = {
	{	1920,	1080	},
};

static u32 cxd5700_framerates[] = {
	30,
};

static void cxd5700_init_format(struct cxd5700 *dev)
{
	dev->fmt.width = DEFAULT_WIDTH;
	dev->fmt.height	= DEFAULT_HEIGHT;
	dev->fmt.code = MEDIA_BUS_FMT_UYVY8_1X16;//MEDIA_BUS_FMT_UYVY8_2X8;
	dev->fmt.field = V4L2_FIELD_NONE;
	dev->fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
}

static int cxd5700_parse_device_tree(struct cxd5700 *dev, struct device_node *node)
{
	struct device_node	*loc_ep	= NULL;
	int			ret	= 0;

	if (node == NULL) {
		loge("the device tree is empty\n");
		ret = -ENODEV;
		goto err;
	}

	/*
	 * Parsing input port information
	 */
	loc_ep = of_graph_get_next_endpoint(node, NULL);
	if (!loc_ep) {
		loge("input endpoint does not exist");
		ret = -EINVAL;
		goto err;
	}

	/*
	 * parsing all endpoint to alloc subdevice and async subdevice
	 */
	do {
		const char *io = NULL;
		struct device_node *rem_ep = NULL;

		ret = of_property_read_string(loc_ep, "io-direction", &io);
		if (ret) {
			loge("Problem in io-direction property\n");
			goto err;
		}

		if (!strcmp(io, "input")) {
			struct device_node *remt_dev = NULL;
			/*
			 * init aysnc subdev instance for remote device
			 */
			rem_ep = of_graph_get_remote_endpoint(loc_ep);
			if (!rem_ep) {
				loge("Problem in Remote endpoint");
				ret = -ENODEV;
				goto err;
			}

			remt_dev = of_graph_get_port_parent(rem_ep);
			if (!remt_dev) {
				loge("Problem in Remote device node");
				ret = -ENODEV;
				of_node_put(rem_ep);
				goto err;
			}
			logi("linked remote device - %s, remote ep - %s\n",
				remt_dev->name, rem_ep->full_name);

			dev->asd.match_type =
				V4L2_ASYNC_MATCH_FWNODE;
			dev->asd.match.fwnode =
				of_fwnode_handle(rem_ep);

			of_node_put(remt_dev);
			of_node_put(rem_ep);
		} else if (!strcmp(io, "output")) {
			/*
			 * init subdev instance for this device
			 */
			/* set fwnode of output endpoint */
			dev->sd.fwnode =
				of_fwnode_handle(loc_ep);

		} else {
			loge("Wrong io-direction property value");
			ret = -EINVAL;
			goto err;
		}

		loc_ep = of_graph_get_next_endpoint(node, loc_ep);
	} while (loc_ep != NULL);
err:
	of_node_put(loc_ep);

	return ret;
}

/*
 * Helper functions for reflection
 */
static inline struct cxd5700 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct cxd5700, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int cxd5700_init(struct v4l2_subdev *sd, u32 enable)
{
	struct cxd5700		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if (enable == 1) {
		if (dev->i_cnt == 0) {
			ret = regmap_bulk_write(dev->regmap,
					0x0a,
					cxd5700_reg_defaults,
					ARRAY_SIZE(cxd5700_reg_defaults));
			if (ret < 0) {
				/* err status */
				loge("regmap_multi_reg_write returned %d\n", ret);
			}
			msleep(100);	/* stabilization time */
		}
		dev->i_cnt++;
	} else {
		dev->i_cnt--;
		if (dev->i_cnt == 0) {
			/* de-init */
			;
		}
	}

	mutex_unlock(&dev->lock);

	return ret;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int cxd5700_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct cxd5700		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	mutex_unlock(&dev->lock);

	return ret;
}

static int cxd5700_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct cxd5700		*dev	= NULL;

	dev = to_dev(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		return -EINVAL;
	}

	interval->pad = 0;
	interval->interval.numerator = 1;
	interval->interval.denominator = dev->framerate;

	return 0;
}

static int cxd5700_s_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct cxd5700		*dev	= NULL;

	dev = to_dev(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		return -EINVAL;
	}

	/* set framerate with i2c setting if supported */

	dev->framerate = interval->interval.denominator;

	return 0;
}

/*
 * v4l2_subdev_pad_ops implementations
 */
static int cxd5700_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct frame_size	*size		= NULL;

	if (ARRAY_SIZE(cxd5700_framesizes) <= fse->index) {
		logd("index(%u) is wrong\n", fse->index);
		return -EINVAL;
	}

	size = &cxd5700_framesizes[fse->index];
	logd("size: %u * %u\n", size->width, size->height);

	fse->min_width = fse->max_width = size->width;
	fse->min_height	= fse->max_height = size->height;
	logd("max size: %u * %u\n", fse->max_width, fse->max_height);

	return 0;
}

static int cxd5700_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	if (ARRAY_SIZE(cxd5700_framerates) <= fie->index) {
		logd("index(%u) is wrong\n", fie->index);
		return -EINVAL;
	}

	fie->interval.numerator = 1;
	fie->interval.denominator = cxd5700_framerates[fie->index];
	logd("framerate: %u / %u\n",
		fie->interval.numerator, fie->interval.denominator);

	return 0;
}

static int cxd5700_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct cxd5700		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	memcpy((void *)&format->format, (const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	logi("size: %d * %d\n",
		format->format.width, format->format.height);
	logi("code: 0x%08xn",
		format->format.code);

	mutex_unlock(&dev->lock);
	return ret;
}

static int cxd5700_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct cxd5700		*dev	= to_dev(sd);
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
static const struct v4l2_subdev_core_ops cxd5700_core_ops = {
	.init			= cxd5700_init,
};

static const struct v4l2_subdev_video_ops cxd5700_video_ops = {
	.s_stream		= cxd5700_s_stream,
	.g_frame_interval	= cxd5700_g_frame_interval,
	.s_frame_interval	= cxd5700_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops cxd5700_pad_ops = {
	.enum_frame_size	= cxd5700_enum_frame_size,
	.enum_frame_interval	= cxd5700_enum_frame_interval,
	.get_fmt		= cxd5700_get_fmt,
	.set_fmt		= cxd5700_set_fmt,
};

static const struct v4l2_subdev_ops cxd5700_ops = {
	.core			= &cxd5700_core_ops,
	.video			= &cxd5700_video_ops,
	.pad			= &cxd5700_pad_ops,
};

struct cxd5700 cxd5700_data = {
};

static const struct i2c_device_id cxd5700_id[] = {
	{ "cxd5700", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cxd5700_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id cxd5700_of_match[] = {
	{
		.compatible	= "sony,cxd5700",
		.data		= &cxd5700_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cxd5700_of_match);
#endif

int cxd5700_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct cxd5700			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct cxd5700), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(cxd5700_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* init parameters */
	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &cxd5700_ops);

	/* parse device tree */
	ret = cxd5700_parse_device_tree(dev, client->dev.of_node);
	if (ret < 0) {
		loge("cxd5700_parse_device_tree, ret: %d\n", ret);
		return ret;
	}

	/*
	 * register subdev instance.
	 * The subdev instance is this device.
	 */
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret) {
		/* failure of v4l2_async_register_subdev */
		loge("v4l2_async_register_subdev, ret: %d\n", ret);
	} else {
		/* success of v4l2_async_register_subdev */
		logi("%s is registered as v4l2-subdev\n", dev->sd.name);
	}

	/* init framerate */
	dev->framerate = DEFAULT_FRAMERATE;

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &cxd5700_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	/* init format info */
	cxd5700_init_format(dev);

	goto goto_end;

goto_free_device_data:
	/* free the videosource data */
	kfree(dev);

goto_end:
	return ret;
}

int cxd5700_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct cxd5700		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver cxd5700_driver = {
	.probe		= cxd5700_probe,
	.remove		= cxd5700_remove,
	.driver		= {
		.name		= "cxd5700",
		.of_match_table	= of_match_ptr(cxd5700_of_match),
	},
	.id_table	= cxd5700_id,
};

module_i2c_driver(cxd5700_driver);

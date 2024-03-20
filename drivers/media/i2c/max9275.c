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

#define LOG_TAG				"VSRC:MAX9275"

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
struct max9275 {
	struct v4l2_subdev		sd;

	struct v4l2_async_subdev	asd;
	struct v4l2_async_notifier	notifier;
	struct v4l2_subdev		*remote_sd;

	struct v4l2_mbus_framefmt	fmt;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;
};

const struct reg_sequence max9275_reg_defaults[] = {
	{0x04, 0x43, 50*1000},

};

static const struct regmap_config max9275_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

static int max9275_notifier_bound(struct v4l2_async_notifier *notifier,
	struct v4l2_subdev *subdev,
	struct v4l2_async_subdev *asd)
{
	struct max9275		*dev		= NULL;
	int			ret		= 0;

	dev = container_of(notifier, struct max9275, notifier);

	logi("v4l2-subdev %s is bounded\n", subdev->name);

	dev->remote_sd = subdev;

	return ret;
}

static void max9275_notifier_unbind(struct v4l2_async_notifier *notifier,
	struct v4l2_subdev *subdev,
	struct v4l2_async_subdev *asd)
{
	struct max9275	*dev		= NULL;

	dev = container_of(notifier, struct max9275, notifier);

	logi("v4l2-subdev %s is unbounded\n", subdev->name);
}

static const struct v4l2_async_notifier_operations max9275_notifier_ops = {
	.bound = max9275_notifier_bound,
	.unbind = max9275_notifier_unbind,
};

static int max9275_parse_device_tree(struct max9275 *dev, struct device_node *node)
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

			dev->asd.match_type = V4L2_ASYNC_MATCH_FWNODE;
			dev->asd.match.fwnode = of_fwnode_handle(rem_ep);

			of_node_put(remt_dev);
			of_node_put(rem_ep);
		} else if (!strcmp(io, "output")) {
			/*
			 * init subdev instance for this device
			 */
			/* set fwnode of output endpoint */
			dev->sd.fwnode = of_fwnode_handle(loc_ep);

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

static void max9275_init_format(struct max9275 *dev)
{
	dev->fmt.width		= 1920;
	dev->fmt.height		= 1080;
	dev->fmt.code		= MEDIA_BUS_FMT_UYVY8_1X16;
	dev->fmt.field		= V4L2_FIELD_NONE;
	dev->fmt.colorspace	= V4L2_COLORSPACE_SMPTE170M;
}

/*
 * Helper functions for reflection
 */
static inline struct max9275 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max9275, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int max9275_init(struct v4l2_subdev *sd, u32 enable)
{
	struct max9275		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if (enable == 1) {
		if (dev->i_cnt == 0) {
			ret = regmap_multi_reg_write(dev->regmap,
					max9275_reg_defaults,
					ARRAY_SIZE(max9275_reg_defaults));
			if (ret < 0)
				loge("Fail initializing max9275 device\n");

			/* init of remote subdev */
			ret = v4l2_subdev_call(dev->remote_sd, core, init, enable);
			if (ret < 0) {
				/* failure of init */
				logd("init, ret: %d\n", ret);
			}
		}
		dev->i_cnt++;
	} else {
		dev->i_cnt--;
		if (dev->i_cnt == 0) {
			/* init of remote subdev */
			ret = v4l2_subdev_call(dev->remote_sd, core, init, enable);
			if (ret < 0) {
				/* failure of init */
				logd("init, ret: %d\n", ret);
			}
		}
	}

	mutex_unlock(&dev->lock);

	return ret;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int max9275_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct max9275		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if (enable == 1) {
		if (dev->s_cnt == 0) {
			ret = regmap_write(dev->regmap, 0x04, 0x83);
			if (ret < 0) {
				/* failure of s_stream */
				loge("Fail Serialization max9275 device\n");
			}

			ret = v4l2_subdev_call(dev->remote_sd, video, s_stream, enable);
			if (ret < 0) {
				/* failure of s_stream */
				logd("s_stream, ret: %d\n", ret);
			}
		}
		/* count up */
		dev->s_cnt++;
	} else {
		/* count down */
		dev->s_cnt--;
		if (dev->s_cnt == 0) {
			ret = v4l2_subdev_call(dev->remote_sd, video, s_stream, enable);
			if (ret < 0) {
				/* failure of s_stream */
				logd("s_stream, ret: %d\n", ret);
			}

			ret = regmap_write(dev->regmap, 0x04, 0x43);
		}
	}

	mutex_unlock(&dev->lock);
	return ret;
}

static int max9275_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct max9275		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	ret = v4l2_subdev_call(dev->remote_sd, video, g_frame_interval, interval);
	if (ret < 0) {
		/* failure of g_frame_interval */
		logd("g_frame_interval, ret: %d\n", ret);
	}

	mutex_unlock(&dev->lock);

	return ret;
}

static int max9275_s_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct max9275		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	ret = v4l2_subdev_call(dev->remote_sd, video, s_frame_interval, interval);
	if (ret < 0) {
		/* failure of s_frame_interval */
		logd("s_frame_interval, ret: %d\n", ret);
	}

	mutex_unlock(&dev->lock);

	return ret;
}

/*
 * v4l2_subdev_pad_ops implementations
 */
static int max9275_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct max9275		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	ret = v4l2_subdev_call(dev->remote_sd, pad, enum_frame_size, NULL, fse);
	if (ret < 0) {
		/* failure of enum_frame_size */
		logd("enum_frame_size, ret: %d\n", ret);
	}

	mutex_unlock(&dev->lock);

	return ret;
}

static int max9275_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	struct max9275		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	ret = v4l2_subdev_call(dev->remote_sd, pad, enum_frame_interval, NULL, fie);
	if (ret < 0) {
		/* failure of enum_frame_interval */
		logd("enum_frame_interval, ret: %d\n", ret);
	}

	mutex_unlock(&dev->lock);

	return ret;
}

static int max9275_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct max9275		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	ret = v4l2_subdev_call(dev->remote_sd, pad, get_fmt, cfg, format);
	if (ret < 0) {
		/* failure of get_fmt */
		logd("get_fmt, ret: %d\n", ret);
	}

	logi("size: %d * %d\n", format->format.width, format->format.height);
	logi("code: 0x%08x\n", format->format.code);

	mutex_unlock(&dev->lock);
	return ret;
}

static int max9275_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct max9275		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	ret = v4l2_subdev_call(dev->remote_sd, pad, set_fmt, cfg, format);
	if (ret < 0) {
		/* failure of set_fmt */
		logd("set_fmt, ret: %d\n", ret);
	} else {
		memcpy((void *)&dev->fmt, (const void *)&format->format,
			sizeof(struct v4l2_mbus_framefmt));
	}

	mutex_unlock(&dev->lock);
	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static const struct v4l2_subdev_core_ops max9275_core_ops = {
	.init			= max9275_init,
};

static const struct v4l2_subdev_video_ops max9275_video_ops = {
	.s_stream		= max9275_s_stream,
	.g_frame_interval	= max9275_g_frame_interval,
	.s_frame_interval	= max9275_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops max9275_pad_ops = {
	.enum_frame_size	= max9275_enum_frame_size,
	.enum_frame_interval	= max9275_enum_frame_interval,
	.get_fmt		= max9275_get_fmt,
	.set_fmt		= max9275_set_fmt,
};

static const struct v4l2_subdev_ops max9275_ops = {
	.core			= &max9275_core_ops,
	.video			= &max9275_video_ops,
	.pad			= &max9275_pad_ops,
};

struct max9275 max9275_data = {
};

static const struct i2c_device_id max9275_id[] = {
	{ "max9275", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max9275_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id max9275_of_match[] = {
	{
		.compatible	= "maxim,max9275",
		.data		= &max9275_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max9275_of_match);
#endif

int max9275_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max9275			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct max9275), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(max9275_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &max9275_ops);

	/* init notifier */
	v4l2_async_notifier_init(&dev->notifier);
	dev->notifier.ops = &max9275_notifier_ops;

	/* parse device tree */
	ret = max9275_parse_device_tree(dev, client->dev.of_node);
	if (ret < 0) {
		loge("max9275_parse_device_tree, ret: %d\n", ret);
		return ret;
	}

	/*
	 * add allocated async subdev instance to a notifier.
	 * The async subdev is the linked device
	 * in front of this device.
	 */
	ret = v4l2_async_notifier_add_subdev(&dev->notifier, &dev->asd);
	if (ret) {
		loge("v4l2_async_notifier_add_subdev, ret: %d\n", ret);
		goto goto_end;
	}

	/* register a notifier */
	ret = v4l2_async_subdev_notifier_register(&dev->sd, &dev->notifier);
	if (ret < 0) {
		loge("v4l2_async_subdev_notifier_register, ret: %d\n", ret);
		v4l2_async_notifier_cleanup(&dev->notifier);
		goto goto_end;
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

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &max9275_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	/* init format info */
	max9275_init_format(dev);

	goto goto_end;

goto_free_device_data:
	/* free the videosource data */
	kfree(dev);

goto_end:
	return ret;
}

int max9275_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max9275		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver max9275_driver = {
	.probe		= max9275_probe,
	.remove		= max9275_remove,
	.driver		= {
		.name		= "max9275",
		.of_match_table	= of_match_ptr(max9275_of_match),
	},
	.id_table	= max9275_id,
};

module_i2c_driver(max9275_driver);

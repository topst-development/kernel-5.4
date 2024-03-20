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

#define LOG_TAG				"VSRC:MAX9286"

#define loge(fmt, ...)			\
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)			\
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)			\
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)			\
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define	NUM_CHANNELS			(4)

#define MAX9286_REG_STATUS_1		(0x1E)
#define MAX9286_VAL_STATUS_1		(0x40)

struct power_sequence {
	int				pwr_port;
	int				pwd_port;
	int				rst_port;
	int				intb_port;

	enum of_gpio_flags		pwr_value;
	enum of_gpio_flags		pwd_value;
	enum of_gpio_flags		rst_value;
	enum of_gpio_flags		intb_value;
};

/*
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct max9286_channel {
	int				id;
	struct v4l2_subdev		sd;

	struct v4l2_async_subdev	asd;
	struct v4l2_async_notifier	notifier;
	struct v4l2_subdev		*remote_sd;

	struct max9286			*dev;
};

struct max9286 {
	struct max9286_channel		channel[NUM_CHANNELS];

	struct v4l2_mbus_framefmt	fmt;

	struct power_sequence		gpio;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;

	bool				broadcasting_mode;
};

const struct reg_sequence max9286_reg_defaults[] = {
	// init
	// enable 4ch
	{0X0A, 0X0F, 0},	/* Disable all Forward control channel */
	{0X34, 0X35, 0},	/* Disable auto acknowledge */
	{0X15, 0X83, 0},	/*
				 * Select the combined camera line format
				 * for CSI2 output
				 */
	{0X12, 0XF3, 5*1000},	/* MIPI Output setting(DBL ON, YUV422) */
	{0X63, 0X00, 0},	/* Widows off */
	{0X64, 0X00, 0},	/* Widows off */
	{0X62, 0X1F, 0},	/* FRSYNC Diff off */

	{0x01, 0xc0, 0},	/* manual mode */
	{0X08, 0X25, 0},	/* FSYNC-period-High */
	{0X07, 0XC3, 0},	/* FSYNC-period-Mid */
	{0X06, 0XF8, 5*1000},	/* FSYNC-period-Low */
	{0X00, 0XEF, 5*1000},	/* Port 0~3 used */
#if defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
	{0X15, 0X03, 0},	/* (line concatenation) */
#else
	{0X15, 0X93, 0},	/* (line interleave) */
#endif
	{0X69, 0XF0, 0},	/* Auto mask & comabck enable */
	{0x01, 0x00, 0},
	{0X0A, 0XFF, 0},	/* All forward channel enable */
};

/* for raw12 1ch input(mcnex ar0147) */
const struct reg_sequence max9286_reg_defaults_raw12[] = {
	{0X0A, 0X0F, 0},	/* Disable all Forward control channel */
	{0X34, 0Xb5, 0},	/* Enable auto acknowledge */
	{0X15, 0X83, 0},	/*
				 * Select the combined camera line format
				 * for CSI2 output
				 */
	{0X12, 0Xc7, 0},	/* Write DBL OFF, MIPI Output setting(RAW12) */
	{0X1C, 0xf6, 5*1000},	/* BWS: 27bit */
	{0X63, 0X00, 0},	/* Widows off */
	{0X64, 0X00, 0},	/* Widows off */
	{0X62, 0X1F, 0},	/* FRSYNC Diff off */

	{0X00, 0XE1, 0},	/* Port 0 used */
	{0x0c, 0x11, 5*1000},	/* disable HS/VS encoding */
	{0X15, 0X93, 0},	/* (line interleave) */
	{0X69, 0XF0, 0},	/* Auto mask & comabck enable */
	{0X0A, 0XFF, 0},	/* All forward channel enable */
};

static const struct regmap_config max9286_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

static int max9286_notifier_bound(struct v4l2_async_notifier *notifier,
	struct v4l2_subdev *subdev,
	struct v4l2_async_subdev *asd)
{
	struct max9286_channel	*ch		= NULL;
	int			ret		= 0;

	ch = container_of(notifier, struct max9286_channel, notifier);

	logi("v4l2-subdev %s is bounded\n", subdev->name);

	ch->remote_sd = subdev;

	if (ch->dev->broadcasting_mode) {
		int idx = 1;

		logi("broadcasting mode\n");
		for (idx = 1; idx < NUM_CHANNELS; idx++) {
			/* copy remote subdev pointer */
			ch[idx].remote_sd = subdev;
		}
	}
	return ret;
}

static void max9286_notifier_unbind(struct v4l2_async_notifier *notifier,
	struct v4l2_subdev *subdev,
	struct v4l2_async_subdev *asd)
{
	struct max9286_channel	*ch		= NULL;

	ch = container_of(notifier, struct max9286_channel, notifier);

	logi("v4l2-subdev %s is unbounded\n", subdev->name);
}

static const struct v4l2_async_notifier_operations max9286_notifier_ops = {
	.bound = max9286_notifier_bound,
	.unbind = max9286_notifier_unbind,
};

static int max9286_parse_device_tree(struct max9286 *dev, struct device_node *node)
{
	struct device_node	*loc_ep	= NULL;
	int			ret	= 0;

	if (node == NULL) {
		loge("the device tree is empty\n");
		return -ENODEV;
	}

	dev->gpio.pwr_port = of_get_named_gpio_flags(node,
		"pwr-gpios", 0, &dev->gpio.pwr_value);
	dev->gpio.pwd_port = of_get_named_gpio_flags(node,
		"pwd-gpios", 0, &dev->gpio.pwd_value);
	dev->gpio.rst_port = of_get_named_gpio_flags(node,
		"rst-gpios", 0, &dev->gpio.rst_value);
	dev->gpio.intb_port = of_get_named_gpio_flags(node,
		"intb-gpios", 0, &dev->gpio.intb_value);

	dev->broadcasting_mode =
		of_property_read_bool(node, "broadcasting-mode");
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
		int channel = 0;
		struct device_node *rem_ep = NULL;

		ret = of_property_read_string(loc_ep, "io-direction", &io);
		if (ret) {
			loge("Problem in io-direction property\n");
			goto err;
		}

		ret = of_property_read_u32(loc_ep, "channel", &channel);
		if (ret) {
			loge("Problem in channel property\n");
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

			dev->channel[channel].asd.match_type = V4L2_ASYNC_MATCH_FWNODE;
			dev->channel[channel].asd.match.fwnode = of_fwnode_handle(rem_ep);

			of_node_put(remt_dev);
			of_node_put(rem_ep);
		} else if (!strcmp(io, "output")) {
			/*
			 * init subdev instance for this device
			 */
			/* set fwnode of output endpoint */
			dev->channel[channel].sd.fwnode = of_fwnode_handle(loc_ep);

			/* print fwnode */
			logd("output[%d]\n", channel);
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
 * gpio functions
 */
void max9286_request_gpio(struct max9286 *dev)
{
	if (dev->gpio.pwr_port > 0) {
		/* power */
		gpio_request(dev->gpio.pwr_port, "max9286 power");
	}
	if (dev->gpio.pwd_port > 0) {
		/* power-down */
		gpio_request(dev->gpio.pwd_port, "max9286 power-down");
	}
	if (dev->gpio.rst_port > 0) {
		/* reset */
		gpio_request(dev->gpio.rst_port, "max9286 reset");
	}
	if (dev->gpio.intb_port > 0) {
		/* intb */
		gpio_request(dev->gpio.intb_port, "max9286 interrupt");
	}
}

void max9286_free_gpio(struct max9286 *dev)
{
	if (dev->gpio.pwr_port > 0) {
		/* power */
		gpio_free(dev->gpio.pwr_port);
	}
	if (dev->gpio.pwd_port > 0) {
		/* power-down */
		gpio_free(dev->gpio.pwd_port);
	}
	if (dev->gpio.rst_port > 0) {
		/* reset */
		gpio_free(dev->gpio.rst_port);
	}
	if (dev->gpio.intb_port > 0) {
		/* intb */
		gpio_free(dev->gpio.intb_port);
	}
}

/*
 * Helper functions for reflection
 */
static inline struct max9286_channel *to_channel(struct v4l2_subdev *sd)
{
	return v4l2_get_subdevdata(sd);
}

static inline struct max9286 *to_dev(struct v4l2_subdev *sd)
{
	struct max9286_channel	*ch	= to_channel(sd);

	return ch->dev;
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int max9286_init(struct v4l2_subdev *sd, u32 enable)
{
	struct max9286_channel	*ch	= to_channel(sd);
	struct max9286		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if (enable == 1) {
		if (dev->i_cnt == 0) {
			ret = regmap_multi_reg_write(dev->regmap,
				max9286_reg_defaults,
				ARRAY_SIZE(max9286_reg_defaults));
			if (ret < 0) {
				/* failed to write i2c */
				loge("regmap_multi_reg_write returned %d\n", ret);
			}

			ret = v4l2_subdev_call(ch->remote_sd, core, init, enable);
			if (ret < 0) {
				/* failure of init */
				logd("init, ret: %d\n", ret);
			}
		}
		dev->i_cnt++;
	} else {
		dev->i_cnt--;
		if (dev->i_cnt == 0) {
			ret = v4l2_subdev_call(ch->remote_sd, core, init, enable);
			if (ret < 0) {
				/* failure of init */
				logd("init, ret: %d\n", ret);
			}

			/* ret = regmap_write(dev->regmap, 0x15, 0x93); */
		}
	}

	mutex_unlock(&dev->lock);

	return ret;
}

static int max9286_set_power(struct v4l2_subdev *sd, int on)
{
	struct max9286		*dev	= to_dev(sd);
	struct power_sequence	*gpio	= &dev->gpio;

	mutex_lock(&dev->lock);

	if (on) {
		if (dev->p_cnt == 0) {
			/* port configuration */
			if (dev->gpio.pwr_port > 0) {
				gpio_direction_output(dev->gpio.pwr_port,
					dev->gpio.pwr_value);
				logd("[pwr] gpio: %3d, val: %d, cur val: %d\n",
					dev->gpio.pwr_port,
					dev->gpio.pwr_value,
					gpio_get_value(dev->gpio.pwr_port));
			}
			if (dev->gpio.pwd_port > 0) {
				gpio_direction_output(dev->gpio.pwd_port,
					dev->gpio.pwd_value);
				logd("[pwd] gpio: %3d, val: %d, cur val: %d\n",
					dev->gpio.pwd_port,
					dev->gpio.pwd_value,
					gpio_get_value(dev->gpio.pwd_port));
			}
			if (dev->gpio.rst_port > 0) {
				gpio_direction_output(dev->gpio.rst_port,
					dev->gpio.rst_value);
				logd("[rst] gpio: %3d, val: %d, cur val: %d\n",
					dev->gpio.rst_port,
					dev->gpio.rst_value,
					gpio_get_value(dev->gpio.rst_port));
			}
			if (dev->gpio.intb_port > 0) {
				gpio_direction_input(dev->gpio.intb_port);
				logd("[int] gpio: %3d, val: %d, cur val: %d\n",
					dev->gpio.intb_port,
					dev->gpio.intb_value,
					gpio_get_value(dev->gpio.intb_port));
			}

			/* power-up sequence */
			if (dev->gpio.pwd_port > 0) {
				gpio_set_value_cansleep(gpio->pwd_port, 1);
				msleep(20);
			}
			if (dev->gpio.rst_port > 0) {
				gpio_set_value_cansleep(gpio->rst_port, 1);
				msleep(20);
			}
		}
		dev->p_cnt++;
	} else {
		dev->p_cnt--;
		if (dev->p_cnt == 0) {
			/* power-down sequence */
			if (dev->gpio.rst_port > 0) {
				gpio_set_value_cansleep(gpio->rst_port, 0);
				msleep(20);
			}
			if (dev->gpio.pwd_port > 0) {
				gpio_set_value_cansleep(gpio->pwd_port, 0);
				msleep(20);
			}
		}
	}

	mutex_unlock(&dev->lock);

	return 0;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int max9286_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct max9286		*dev	= to_dev(sd);
	unsigned int		val	= 0;
	int			ret	= 0;

	mutex_lock(&dev->lock);

	/* reset status */
	*status	= 0;

	/* check V4L2_IN_ST_NO_SIGNAL */
	ret = regmap_read(dev->regmap, MAX9286_REG_STATUS_1, &val);
	if (ret < 0) {
		loge("failure to check MAX9286_REG_STATUS_1\n");
		*status =
			V4L2_IN_ST_NO_POWER |
			V4L2_IN_ST_NO_SIGNAL |
			V4L2_IN_ST_NO_COLOR;
		goto end;
	} else {
		logd("status 1: 0x%08x\n", val);

		if (val != MAX9286_VAL_STATUS_1) {
			logw("STATUS_1 is V4L2_IN_ST_NO_SIGNAL\n");
			*status |= V4L2_IN_ST_NO_SIGNAL;
			goto end;
		}
	}

end:
	mutex_unlock(&dev->lock);

	logi("status: 0x%08x\n", *status);
	return ret;
}

static int max9286_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct max9286_channel	*ch	= to_channel(sd);
	struct max9286		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if (enable == 1) {
		if (dev->s_cnt == 0) {
#if defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
			ret = regmap_write(dev->regmap, 0x15, 0x0B);
#else
			ret = regmap_write(dev->regmap, 0x15, 0x9B);
#endif
			if (ret < 0) {
				/* failure of enabling output  */
				loge("Fail enable output of max9286 device\n");
			}

			ret = v4l2_subdev_call(ch->remote_sd, video, s_stream, enable);
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
			ret = v4l2_subdev_call(ch->remote_sd, video, s_stream, enable);
			if (ret < 0) {
				/* failure of s_stream */
				logd("s_stream, ret: %d\n", ret);
			}

#if defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
			ret = regmap_write(dev->regmap, 0x15, 0x03);
#else
			ret = regmap_write(dev->regmap, 0x15, 0x93);
#endif
			if (ret < 0) {
				/* failure of disabling output  */
				loge("Fail disable output of max9286 device\n");
			}
		}
	}

	msleep(30);

	mutex_unlock(&dev->lock);
	return ret;
}

static int max9286_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct max9286_channel	*ch	= to_channel(sd);
	struct max9286		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	ret = v4l2_subdev_call(ch->remote_sd, video, g_frame_interval, interval);
	if (ret < 0) {
		/* failure of g_frame_interval */
		logd("g_frame_interval, ret: %d\n", ret);
	}

	mutex_unlock(&dev->lock);

	return ret;
}

static int max9286_s_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct max9286_channel	*ch	= to_channel(sd);
	struct max9286		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	ret = v4l2_subdev_call(ch->remote_sd, video, s_frame_interval, interval);
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
static int max9286_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct max9286_channel	*ch	= to_channel(sd);
	struct max9286		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	ret = v4l2_subdev_call(ch->remote_sd, pad, enum_frame_size, NULL, fse);
	if (ret < 0) {
		/* failure of enum_frame_size */
		logd("enum_frame_size, ret: %d\n", ret);
	}

	mutex_unlock(&dev->lock);

	return ret;
}

static int max9286_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	struct max9286_channel	*ch	= to_channel(sd);
	struct max9286		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	ret = v4l2_subdev_call(ch->remote_sd, pad, enum_frame_interval, NULL, fie);
	if (ret < 0) {
		/* failure of enum_frame_interval */
		logd("enum_frame_interval, ret: %d\n", ret);
	}

	mutex_unlock(&dev->lock);

	return ret;
}

static int max9286_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct max9286_channel	*ch	= to_channel(sd);
	struct max9286		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	ret = v4l2_subdev_call(ch->remote_sd, pad, get_fmt, cfg, format);
	if (ret < 0) {
		/* failure of get_fmt */
		logd("get_fmt, ret: %d\n", ret);
	}

	logi("size: %d * %d\n", format->format.width, format->format.height);
	logi("code: 0x%08x\n", format->format.code);

	mutex_unlock(&dev->lock);
	return ret;
}

static int max9286_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct max9286_channel	*ch	= to_channel(sd);
	struct max9286		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	ret = v4l2_subdev_call(ch->remote_sd, pad, set_fmt, cfg, format);
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
static const struct v4l2_subdev_core_ops max9286_core_ops = {
	.init			= max9286_init,
	.s_power		= max9286_set_power,
};

static const struct v4l2_subdev_video_ops max9286_video_ops = {
	.g_input_status		= max9286_g_input_status,
	.s_stream		= max9286_s_stream,
	.g_frame_interval	= max9286_g_frame_interval,
	.s_frame_interval	= max9286_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops max9286_pad_ops = {
	.enum_frame_size	= max9286_enum_frame_size,
	.enum_frame_interval	= max9286_enum_frame_interval,
	.get_fmt		= max9286_get_fmt,
	.set_fmt		= max9286_set_fmt,
};

static const struct v4l2_subdev_ops max9286_ops = {
	.core			= &max9286_core_ops,
	.video			= &max9286_video_ops,
	.pad			= &max9286_pad_ops,
};

struct max9286 max9286_data = {
};

static const struct i2c_device_id max9286_id[] = {
	{ "max9286", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max9286_id);

#if IS_ENABLED(CONFIG_OF)
const static struct of_device_id max9286_of_match[] = {
	{
		.compatible	= "maxim,max9286",
		.data		= &max9286_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max9286_of_match);
#endif

int max9286_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max9286			*dev	= NULL;
	struct max9286_channel		*ch	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				idxch	= 0;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct max9286), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(max9286_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* init parameters */
	for (idxch = 0; idxch < NUM_CHANNELS; idxch++) {
		ch = &dev->channel[idxch];

		ch->id = idxch;

		/* Register with V4L2 layer as a slave device */
		v4l2_i2c_subdev_init(&ch->sd, client, &max9286_ops);
		v4l2_set_subdevdata(&ch->sd, ch);

		/* init notifier */
		v4l2_async_notifier_init(&ch->notifier);
		ch->notifier.ops = &max9286_notifier_ops;

		ch->dev = dev;
	}

	/* parse device tree */
	ret = max9286_parse_device_tree(dev, client->dev.of_node);
	if (ret < 0) {
		loge("max9286_parse_device_tree, ret: %d\n", ret);
		return ret;
	}

	/*
	 * add allocated async subdev instance to a notifier.
	 * The async subdev is the linked device
	 * in front of this device.
	 */
	for (idxch = 0; idxch < NUM_CHANNELS; idxch++) {
		ch = &dev->channel[idxch];

		if (ch->asd.match.fwnode == NULL) {
			/* async subdev to register is not founded */
			continue;
		}

		ret = v4l2_async_notifier_add_subdev(&ch->notifier, &ch->asd);
		if (ret) {
			loge("v4l2_async_notifier_add_subdev, ret: %d\n", ret);
			goto goto_end;
		}

		/* register a notifier */
		ret = v4l2_async_subdev_notifier_register(&ch->sd, &ch->notifier);
		if (ret < 0) {
			loge("v4l2_async_subdev_notifier_register, ret: %d\n", ret);
			v4l2_async_notifier_cleanup(&ch->notifier);
			goto goto_end;
		}
	}

	/*
	 * register subdev instance.
	 * The subdev instance is this device.
	 */
	for (idxch = 0; idxch < NUM_CHANNELS; idxch++) {
		ch = &dev->channel[idxch];
		/* register a v4l2-subdev */

		if (ch->sd.fwnode == NULL) {
			/* subdev to register is not founded */
			continue;
		}

		ret = v4l2_async_register_subdev(&ch->sd);
		if (ret) {
			/* failure of v4l2_async_register_subdev */
			loge("v4l2_async_register_subdev, ret: %d\n", ret);
		} else {
			/* success of v4l2_async_register_subdev */
			logi("%s.%d is registered as v4l2-subdev\n", ch->sd.name, idxch);
		}
	}

	/* request gpio */
	max9286_request_gpio(dev);

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &max9286_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	goto goto_end;

goto_free_device_data:
	/* free the videosource data */
	kfree(dev);

goto_end:
	return ret;
}

int max9286_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max9286		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	/* gree gpio */
	max9286_free_gpio(dev);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver max9286_driver = {
	.probe		= max9286_probe,
	.remove		= max9286_remove,
	.driver		= {
		.name		= "max9286",
		.of_match_table	= of_match_ptr(max9286_of_match),
	},
	.id_table	= max9286_id,
};

module_i2c_driver(max9286_driver);

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

#define LOG_TAG				"VSRC:ISL79988"

#define loge(fmt, ...)			\
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)			\
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)			\
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)			\
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define DEFAULT_WIDTH			(720)
#define DEFAULT_HEIGHT			(480)

#define	DEFAULT_FRAMERATE		(60)

#define ISL79988_STATUS_REG		(0x1B)
#define ISL79988_STATUS_VAL		(0x03)

struct power_sequence {
	int				pwr_port;
	int				pwd_port;
	int				rst_port;

	enum of_gpio_flags		pwr_value;
	enum of_gpio_flags		pwd_value;
	enum of_gpio_flags		rst_value;
};

struct frame_size {
	u32 width;
	u32 height;
};

/*
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct isl79988 {
	struct v4l2_subdev		sd;
	struct v4l2_mbus_framefmt	fmt;
	int				framerate;
	struct v4l2_ctrl_handler	hdl;

	struct power_sequence		gpio;

	/* Regmaps */
	struct regmap			*regmap;
};

const struct reg_sequence isl79988_reg_defaults[] = {
	{0xff, 0x00, 0},	/* page 0 */
	{0x02, 0x00, 0},
	{0x03, 0x00, 0},	/* Disable Tri-state */
	{0x04, 0x0A, 0},	/* Invert Clock */
	{0xff, 0x01, 0},	/* page 1 */
	{0x1C, 0x07, 0},	/* auto dection */
	{0x37, 0x06, 0},
	{0x39, 0x18, 0},
	{0x33, 0x85, 0},	/* Free-run 60Hz */
	{0x2f, 0xe6, 0},	/* auto blue screen */
	{0xff, 0x00, 0},	/* page 0 */
	{0x07, 0x00, 0},	/* 1 ch mode */
	{0x09, 0x4f, 0},	/* PLL = 27MHz */
	{0x0B, 0x42, 0},	/* PLL = 27MHz */
	{0xff, 0x05, 0},	/* page 5 */
	{0x05, 0x42, 0},	/* byte interleave */
	{0x06, 0x61, 0},	/* byte interleave */
	{0x0E, 0x00, 0},
	{0x11, 0xa0, 0},	/* Packet cnt = 1440 (EVB only) */
	{0x13, 0x1B, 0},
	{0x33, 0x40, 0},
	{0x34, 0x18, 0},	/* PLL normal */
	{0x00, 0x02, 0},	/* Decoder on */
};

static const struct regmap_config isl79988_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

static struct frame_size isl79988_framesizes[] = {
	{	720,	480	},
};

static u32 isl79988_framerates[] = {
	60,
};

struct v4l2_dv_timings isl79988_dv_timings = {
	.type		= V4L2_DV_BT_656_1120,
	.bt		= {
		.width		= DEFAULT_WIDTH,
		.height		= DEFAULT_HEIGHT,
		.interlaced	= V4L2_DV_INTERLACED,
		/* IMPORTANT
		 * The below field "polarities" is not used
		 * becasue polarities for vsync and hsync are supported only.
		 * So, use flags of "struct v4l2_mbus_config".
		 */
		.polarities	= 0,
	},
};

static u32 isl79988_codes[] = {
	MEDIA_BUS_FMT_UYVY8_2X8,
};

struct v4l2_mbus_config isl79988_mbus_config = {
	.type		= V4L2_MBUS_BT656,
	/* de: high, vs: high, hs: low, pclk: high */
	.flags		=
		V4L2_MBUS_DATA_ACTIVE_LOW	|
		V4L2_MBUS_VSYNC_ACTIVE_LOW	|
		V4L2_MBUS_HSYNC_ACTIVE_LOW	|
		V4L2_MBUS_PCLK_SAMPLE_FALLING	|
		V4L2_MBUS_MASTER,
};

struct v4l2_mbus_framefmt isl79988_mbus_framefmt = {
	.width		= DEFAULT_WIDTH,
	.height		= DEFAULT_HEIGHT,
	.code		= MEDIA_BUS_FMT_UYVY8_2X8,
};

static int isl79988_parse_device_tree(struct isl79988 *dev, struct device_node *node)
{
	struct device_node	*loc_ep	= NULL;
	struct fwnode_handle	*fwnode = NULL;
	const char		*io_dir	= NULL;
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

	/* get phandle of subdev */
	loc_ep = of_graph_get_endpoint_by_regs(node, 0, -1);
	if (!loc_ep) {
		logd("no endpoint\n");
		return -ENODEV;
	}

	fwnode = of_fwnode_handle(loc_ep);
	if (!fwnode_property_read_string(fwnode, "io-direction", &io_dir)) {
		/* print io-direction */
		logd("ep.in_direction: %s\n", io_dir);

		if (!strcmp(io_dir, "output")) {
			/* set fwnode of output endpoint */
			dev->sd.fwnode = fwnode;

			/* print fwnode */
			logd("fwnode: 0x%p\n", dev->sd.fwnode);
		}
	}

	of_node_put(loc_ep);

	return ret;
}

/*
 * gpio functions
 */
void isl79988_request_gpio(struct isl79988 *dev)
{
	if (dev->gpio.pwr_port > 0) {
		/* power */
		gpio_request(dev->gpio.pwr_port, "isl79988 power");
	}
	if (dev->gpio.pwd_port > 0) {
		/* power-down */
		gpio_request(dev->gpio.pwd_port, "isl79988 power-down");
	}
	if (dev->gpio.rst_port > 0) {
		/* reset */
		gpio_request(dev->gpio.rst_port, "isl79988 reset");
	}
}

void isl79988_free_gpio(struct isl79988 *dev)
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
}

/*
 * Helper functions for reflection
 */
static inline struct isl79988 *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct isl79988, sd);
}

/*
 * v4l2_ctrl_ops implementations
 */
static int isl79988_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int			ret	= 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CONTRAST:
	case V4L2_CID_SATURATION:
	case V4L2_CID_HUE:
	case V4L2_CID_DO_WHITE_BALANCE:
	default:
		loge("V4L2_CID_BRIGHTNESS is not implemented yet.\n");
		ret = -EINVAL;
	}

	return ret;
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int isl79988_set_power(struct v4l2_subdev *sd, int on)
{
	struct isl79988		*dev	= to_state(sd);
	struct power_sequence	*gpio	= &dev->gpio;

	if (on) {
		/* port configuration */
		if (dev->gpio.pwr_port > 0) {
			gpio_direction_output(dev->gpio.pwr_port,
				dev->gpio.pwr_value);
			logd("[pwr] gpio: %3d, new val: %d, cur val: %d\n",
				dev->gpio.pwr_port, dev->gpio.pwr_value,
				gpio_get_value(dev->gpio.pwr_port));
		}
		if (dev->gpio.pwd_port > 0) {
			gpio_direction_output(dev->gpio.pwd_port,
				dev->gpio.pwd_value);
			logd("[pwd] gpio: %3d, new val: %d, cur val: %d\n",
				dev->gpio.pwd_port, dev->gpio.pwd_value,
				gpio_get_value(dev->gpio.pwd_port));
		}
		if (dev->gpio.rst_port > 0) {
			gpio_direction_output(dev->gpio.rst_port,
				dev->gpio.rst_value);
			logd("[rst] gpio: %3d, new val: %d, cur val: %d\n",
				dev->gpio.rst_port, dev->gpio.rst_value,
				gpio_get_value(dev->gpio.rst_port));
		}

		/* power-up sequence */
		if (dev->gpio.rst_port > 0) {
			gpio_set_value_cansleep(gpio->rst_port, 1);
			msleep(20);
		}
	} else {
		/* power-down sequence */
		if (dev->gpio.rst_port > 0) {
			gpio_set_value_cansleep(gpio->rst_port, 0);
			msleep(20);
		}
	}

	return 0;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int isl79988_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct isl79988		*dev	= to_state(sd);
	unsigned int		val	= 0;
	int			ret	= 0;

	/* check V4L2_IN_ST_NO_SIGNAL */
	ret = regmap_write(dev->regmap, 0xFF, 0x00);
	if (ret < 0)
		loge("Failed to set to use page 0\n");

	ret = regmap_read(dev->regmap, ISL79988_STATUS_REG, &val);
	if (ret < 0) {
		loge("failure to check V4L2_IN_ST_NO_SIGNAL\n");
	} else {
		logd("status: 0x%08x\n", val);

		if ((val & 0x0F) != ISL79988_STATUS_VAL) {
			*status &= ~V4L2_IN_ST_NO_SIGNAL;
		} else {
			logw("V4L2_IN_ST_NO_SIGNAL\n");
			*status |= V4L2_IN_ST_NO_SIGNAL;
		}
	}

	return ret;
}

static int isl79988_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct isl79988		*dev	= NULL;
	struct i2c_client	*client	= NULL;
	struct pinctrl		*pctrl	= NULL;
	int			ret	= 0;

	dev = to_state(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		ret = -EINVAL;
	} else {
		ret = regmap_multi_reg_write(dev->regmap,
			isl79988_reg_defaults,
			ARRAY_SIZE(isl79988_reg_defaults));
		msleep(600);
	}

	/* pinctrl */
	client = v4l2_get_subdevdata(&dev->sd);
	if (client == NULL) {
		loge("client is NULL\n");
		ret = -1;
	}

	pctrl = pinctrl_get_select(&client->dev, "default");
	if (IS_ERR(pctrl)) {
		pinctrl_put(pctrl);
		ret = -1;
	}

	return ret;
}

static int isl79988_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct isl79988		*dev	= NULL;

	dev = to_state(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		return -EINVAL;
	}

	if (interval->pad != 0) {
		logd("pad(%u) is wrong\n", interval->pad);
		return -EINVAL;
	}

	interval->pad = 0;
	interval->interval.numerator = 1;
	interval->interval.denominator = dev->framerate;

	return 0;
}

static int isl79988_s_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct isl79988		*dev	= NULL;

	dev = to_state(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		return -EINVAL;
	}

	if (interval->pad != 0) {
		logd("pad(%u) is wrong\n", interval->pad);
		return -EINVAL;
	}

	/* set framerate with i2c setting if supported */

	dev->framerate = interval->interval.denominator;

	return 0;
}

static int isl79988_g_dv_timings(struct v4l2_subdev *sd,
	struct v4l2_dv_timings *timings)
{
	int			ret	= 0;

	memcpy((void *)timings, (const void *)&isl79988_dv_timings,
		sizeof(*timings));

	return ret;
}

static int isl79988_g_mbus_config(struct v4l2_subdev *sd,
	struct v4l2_mbus_config *cfg)
{
	memcpy((void *)cfg, (const void *)&isl79988_mbus_config,
		sizeof(*cfg));

	return 0;
}

/*
 * v4l2_subdev_pad_ops implementations
 */
static int isl79988_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct frame_size	*size		= NULL;

	if (ARRAY_SIZE(isl79988_framesizes) <= fse->index) {
		logd("index(%u) is wrong\n", fse->index);
		return -EINVAL;
	}

	size = &isl79988_framesizes[fse->index];
	logd("size: %u * %u\n", size->width, size->height);

	fse->min_width = fse->max_width = size->width;
	fse->min_height	= fse->max_height = size->height;
	logd("max size: %u * %u\n", fse->max_width, fse->max_height);

	return 0;
}

static int isl79988_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	if (ARRAY_SIZE(isl79988_framerates) <= fie->index) {
		logd("index(%u) is wrong\n", fie->index);
		return -EINVAL;
	}

	fie->interval.numerator = 1;
	fie->interval.denominator = isl79988_framerates[fie->index];
	logd("framerate: %u / %u\n",
		fie->interval.numerator, fie->interval.denominator);

	return 0;
}

static int isl79988_enum_mbus_code(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_mbus_code_enum *code)
{
	if ((code->pad != 0) || (ARRAY_SIZE(isl79988_codes) <= code->index))
		return -EINVAL;

	code->code = isl79988_codes[code->index];

	return 0;
}

static int isl79988_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct isl79988		*dev	= to_state(sd);
	int			ret	= 0;

	memcpy((void *)&format->format, (const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	return ret;
}

static int isl79988_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct isl79988		*dev	= to_state(sd);
	int			ret	= 0;

	memcpy((void *)&dev->fmt, (const void *)&format->format,
		sizeof(struct v4l2_mbus_framefmt));

	isl79988_dv_timings.bt.width = format->format.width;
	isl79988_dv_timings.bt.height = format->format.height;

	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static const struct v4l2_ctrl_ops isl79988_ctrl_ops = {
	.s_ctrl			= isl79988_s_ctrl,
};

static const struct v4l2_subdev_core_ops isl79988_v4l2_subdev_core_ops = {
	.s_power		= isl79988_set_power,
};

static const struct v4l2_subdev_video_ops isl79988_v4l2_subdev_video_ops = {
	.g_input_status		= isl79988_g_input_status,
	.s_stream		= isl79988_s_stream,
	.g_frame_interval	= isl79988_g_frame_interval,
	.s_frame_interval	= isl79988_s_frame_interval,
	.g_dv_timings		= isl79988_g_dv_timings,
	.g_mbus_config		= isl79988_g_mbus_config
};

static const struct v4l2_subdev_pad_ops isl79988_v4l2_subdev_pad_ops = {
	.enum_mbus_code		= isl79988_enum_mbus_code,
	.enum_frame_size	= isl79988_enum_frame_size,
	.enum_frame_interval	= isl79988_enum_frame_interval,
	.get_fmt		= isl79988_get_fmt,
	.set_fmt		= isl79988_set_fmt,
};

static const struct v4l2_subdev_ops isl79988_ops = {
	.core			= &isl79988_v4l2_subdev_core_ops,
	.video			= &isl79988_v4l2_subdev_video_ops,
	.pad			= &isl79988_v4l2_subdev_pad_ops,
};

struct isl79988 isl79988_data = {
};

static const struct i2c_device_id isl79988_id[] = {
	{ "isl79988", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, isl79988_id);

#if IS_ENABLED(CONFIG_OF)
const static struct of_device_id isl79988_of_match[] = {
	{
		.compatible	= "isil,isl79988",
		.data		= &isl79988_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, isl79988_of_match);
#endif

int isl79988_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct isl79988			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct isl79988), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(isl79988_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &isl79988_ops);

	/* parse device tree */
	ret = isl79988_parse_device_tree(dev, client->dev.of_node);
	if (ret < 0) {
		loge("isl79988_parse_device_tree, ret: %d\n", ret);
		return ret;
	}

	/* regitster v4l2 control handlers */
	v4l2_ctrl_handler_init(&dev->hdl, 2);
	v4l2_ctrl_new_std(&dev->hdl, &isl79988_ctrl_ops,
		V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);
	v4l2_ctrl_new_std_menu(&dev->hdl, &isl79988_ctrl_ops,
		V4L2_CID_DV_RX_IT_CONTENT_TYPE, V4L2_DV_IT_CONTENT_TYPE_NO_ITC,
		0, V4L2_DV_IT_CONTENT_TYPE_NO_ITC);
	dev->sd.ctrl_handler = &dev->hdl;
	if (dev->hdl.error) {
		loge("v4l2_ctrl_handler_init is wrong\n");
		ret = dev->hdl.error;
		goto goto_free_device_data;
	}

	/* register a v4l2 sub device */
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret)
		loge("Failed to register subdevice\n");
	else
		logi("%s is registered as a v4l2 sub device.\n", dev->sd.name);

	/* init mbus format */
	memcpy((void *)&dev->fmt, (const void *)&isl79988_mbus_framefmt,
		sizeof(dev->fmt));

	/* init framerate */
	dev->framerate = DEFAULT_FRAMERATE;

	/* request gpio */
	isl79988_request_gpio(dev);

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &isl79988_regmap);
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

int isl79988_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct isl79988		*dev	= to_state(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	/* gree gpio */
	isl79988_free_gpio(dev);

	v4l2_ctrl_handler_free(&dev->hdl);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver isl79988_driver = {
	.probe		= isl79988_probe,
	.remove		= isl79988_remove,
	.driver		= {
		.name		= "isl79988",
		.of_match_table	= of_match_ptr(isl79988_of_match),
	},
	.id_table	= isl79988_id,
};

module_i2c_driver(isl79988_driver);

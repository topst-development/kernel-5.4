/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 *the terms of the GNU General Public License as published by the Free Software
 *Foundation; either version 2 of the License, or (at your option) any later
 *version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 *ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 *Place, Suite 330, Boston, MA 02111-1307 USA
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

#include "tp2855_reg.h"

#define LOG_TAG			"VSRC:TP2855"

#define loge(fmt, ...)		\
		pr_err("[ERROR][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)		\
		pr_warn("[WARN][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)		\
		pr_debug("[DEBUG][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)		\
		pr_info("[INFO][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define TP2855_WIDTH	1920
#define TP2855_HEIGHT	1080

static struct i2c_client        *tp2855_client;

enum {
	CH_1=0,   //
	CH_2=1,    //
	CH_3=2,    //
	CH_4=3,     //
	CH_ALL=4,
	MIPI_PAGE=8,
};

enum{
	STD_TVI, //TVI
	STD_HDA, //AHD
};

enum{
	PAL,
	NTSC,
	HD25,
	HD30,
	FHD25,
	FHD30,
};

enum{
	MIPI_4CH4LANE_297M, //up to 4x720p25/30
	MIPI_4CH4LANE_594M, //up to 4x1080p25/30
	MIPI_2CH4LANE_297M, //up to 2x1080p25/30
	MIPI_2CH4LANE_594M, //up to 2x1080p50/60
	MIPI_1CH4LANE_297M, //up to 1x1080P50/60
	MIPI_4CH4LANE_148M, //up to 4xhalf 720p25/30
	MIPI_2CH4LANE_74M, //up to 2xHalf 720P25/30
};


struct power_sequence {
	int			pwr_port;
	int			pwd_port;
	int			rst_port;
	int			intb_port;

	enum of_gpio_flags	pwr_value;
	enum of_gpio_flags	pwd_value;
	enum of_gpio_flags	rst_value;
	enum of_gpio_flags	intb_value;
};

#define NUM_CHANNELS (4)
struct tp2855_channel {
    int             id;
    struct v4l2_subdev      sd;

    struct v4l2_async_subdev    asd;
    struct v4l2_async_notifier  notifier;
    struct v4l2_subdev      *remote_sd;

    struct tp2855         *dev;
};

/*
 * This object contains essential v4l2 objects such as sub-device and
 * ctrl_handler
 */
struct tp2855 {
	struct v4l2_mbus_framefmt	fmt;

	struct power_sequence		gpio;

	/* Regmaps */
	struct regmap				*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;

    struct tp2855_channel     channel[NUM_CHANNELS];
};

static const struct regmap_config tp2855_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register	= 0xFF,
	.cache_type		= REGCACHE_NONE,
};


static void tp2855_init_format(struct tp2855 *dev)
{
	dev->fmt.width	= TP2855_WIDTH;
	dev->fmt.height	= TP2855_HEIGHT;
	dev->fmt.code	= MEDIA_BUS_FMT_UYVY8_1X16;
	dev->fmt.field	= V4L2_FIELD_NONE;
}

static inline struct tp2855_channel *to_channel(struct v4l2_subdev *sd)
{
    return v4l2_get_subdevdata(sd);
}


/*
 * Helper fuctions for reflection
 */
static inline struct tp2855 *to_dev(struct v4l2_subdev *sd)
{
    struct tp2855_channel  *ch = to_channel(sd);

    return ch->dev;
}

int tp2855_write_reg(struct v4l2_subdev *client, unsigned int _reg, unsigned int _val)
{
	struct tp2855	*dev	= to_dev(client);
	int				ret		= 0;

	ret = regmap_write(dev->regmap, _reg, _val);
	if (ret) {
		loge("Sensor I2C !!!! \n");
	}

	return ret;
}

int tp2855_read_reg(struct v4l2_subdev *client, unsigned int _reg)
{
	struct tp2855	*dev	= to_dev(client);
	unsigned int	data	= 0;
	int				ret		= 0;

	ret = regmap_read(dev->regmap, _reg, &data);
	if (ret != 0) {
		loge("Sensor I2C !!!! \n");
	}

	return data;
}

void tp2855_mipi_out(struct v4l2_subdev *client, unsigned char output)
{
    logd("%d",__LINE__);
	//mipi setting
	tp2855_write_reg(client, 0x40, 0x08); //MIPI page
	tp2855_write_reg(client, 0x01, 0xf8); // diff 0xF0 clock output invert
	tp2855_write_reg(client, 0x02, 0x01);
	tp2855_write_reg(client, 0x08, 0x0f);

	tp2855_write_reg(client, 0x10, 0x20);
	tp2855_write_reg(client, 0x11, 0x47);
	tp2855_write_reg(client, 0x12, 0x54);
	tp2855_write_reg(client, 0x13, 0xef);

	if( MIPI_4CH4LANE_594M == output ) {
		tp2855_write_reg(client, 0x20, 0x44);
		tp2855_write_reg(client, 0x34, 0xe4);

		tp2855_write_reg(client, 0x14, 0x06);
		tp2855_write_reg(client, 0x15, 0x00); //  tp2855_write_reg(client, 0x14, 0x06);
		tp2855_write_reg(client, 0x25, 0x08);
		tp2855_write_reg(client, 0x26, 0x06);
		tp2855_write_reg(client, 0x27, 0x11);
		tp2855_write_reg(client, 0x29, 0x0a);

		tp2855_write_reg(client, 0x0a, 0x80); // TEST
		tp2855_write_reg(client, 0x33, 0x0f); //   tp2855_write_reg(client, 0x14, 0x06);
		tp2855_write_reg(client, 0x33, 0x00);
		tp2855_write_reg(client, 0x14, 0x86); //
		tp2855_write_reg(client, 0x14, 0x06); //
	} else if( MIPI_2CH4LANE_594M == output ) {
		tp2855_write_reg(client, 0x20, 0x24);
		//tp2855_write_reg(client, 0x34, 0xe4); //output vin1&vin3
		tp2855_write_reg(client, 0x34, 0xd4); //output vin1&vin2

		tp2855_write_reg(client, 0x14, 0x06);
		tp2855_write_reg(client, 0x15, 0x00);
		tp2855_write_reg(client, 0x25, 0x08);
		tp2855_write_reg(client, 0x26, 0x06);
		tp2855_write_reg(client, 0x27, 0x11);
		tp2855_write_reg(client, 0x29, 0x0a);

		tp2855_write_reg(client, 0x0a, 0x80);
		tp2855_write_reg(client, 0x33, 0x0f);
		tp2855_write_reg(client, 0x33, 0x00);
		tp2855_write_reg(client, 0x14, 0x86);
		tp2855_write_reg(client, 0x14, 0x06);
	} else if( MIPI_4CH4LANE_297M == output) {
		tp2855_write_reg(client, 0x20, 0x44);
		tp2855_write_reg(client, 0x34, 0xe4);

		tp2855_write_reg(client, 0x14, 0x47);
		tp2855_write_reg(client, 0x15, 0x01);
		tp2855_write_reg(client, 0x25, 0x04);
		tp2855_write_reg(client, 0x26, 0x03);
		tp2855_write_reg(client, 0x27, 0x09);
		tp2855_write_reg(client, 0x29, 0x02);

		tp2855_write_reg(client, 0x0a, 0x80);
		tp2855_write_reg(client, 0x33, 0x0f);
		tp2855_write_reg(client, 0x33, 0x00);
		tp2855_write_reg(client, 0x14, 0xc7);
		tp2855_write_reg(client, 0x14, 0x47);
	} else if( MIPI_2CH4LANE_297M == output ) {
		tp2855_write_reg(client, 0x20, 0x24);
		//tp2855_write_reg(client, 0x34, 0xe4); //output vin1&vin3
		tp2855_write_reg(client, 0x34, 0xd4); //output vin1&vin2

		tp2855_write_reg(client, 0x14, 0x47);
		tp2855_write_reg(client, 0x15, 0x01);
		tp2855_write_reg(client, 0x25, 0x04);
		tp2855_write_reg(client, 0x26, 0x03);
		tp2855_write_reg(client, 0x27, 0x09);
		tp2855_write_reg(client, 0x27, 0x11);
		tp2855_write_reg(client, 0x29, 0x02);

		tp2855_write_reg(client, 0x0a, 0x80);
		tp2855_write_reg(client, 0x33, 0x0f);
		tp2855_write_reg(client, 0x33, 0x00);
		tp2855_write_reg(client, 0x14, 0xc7);
		tp2855_write_reg(client, 0x14, 0x47);
	} else if( MIPI_1CH4LANE_297M == output ) {
		tp2855_write_reg(client, 0x20, 0x14);
		tp2855_write_reg(client, 0x34, 0xe4);

		tp2855_write_reg(client, 0x14, 0x47);
		tp2855_write_reg(client, 0x15, 0x01);
		tp2855_write_reg(client, 0x25, 0x04);
		tp2855_write_reg(client, 0x26, 0x03);
		tp2855_write_reg(client, 0x27, 0x09);
		tp2855_write_reg(client, 0x29, 0x02);

		tp2855_write_reg(client, 0x0a, 0x80);
		tp2855_write_reg(client, 0x33, 0x0f);
		tp2855_write_reg(client, 0x33, 0x00);
		tp2855_write_reg(client, 0x14, 0xc7);
		tp2855_write_reg(client, 0x14, 0x47);

	} else if( MIPI_4CH4LANE_148M == output ) {
		tp2855_write_reg(client, 0x20, 0x44);
		tp2855_write_reg(client, 0x34, 0xe4);

		tp2855_write_reg(client, 0x14, 0x53);
		tp2855_write_reg(client, 0x15, 0x02);
		tp2855_write_reg(client, 0x25, 0x02);
		tp2855_write_reg(client, 0x26, 0x01);
		tp2855_write_reg(client, 0x27, 0x05);
		tp2855_write_reg(client, 0x29, 0x02);

		tp2855_write_reg(client, 0x0a, 0x80);
		tp2855_write_reg(client, 0x33, 0x0f);
		tp2855_write_reg(client, 0x33, 0x00);
		tp2855_write_reg(client, 0x14, 0xd3);
		tp2855_write_reg(client, 0x14, 0x53);
	} else if( MIPI_2CH4LANE_74M == output ) {
		tp2855_write_reg(client, 0x20, 0x24);
		tp2855_write_reg(client, 0x34, 0x10);
		tp2855_write_reg(client, 0x14, 0x53);
		tp2855_write_reg(client, 0x15, 0x12);
		tp2855_write_reg(client, 0x25, 0x02);
		tp2855_write_reg(client, 0x26, 0x01);
		tp2855_write_reg(client, 0x27, 0x05);
		tp2855_write_reg(client, 0x29, 0x02);

		tp2855_write_reg(client, 0x0a, 0x80);
		tp2855_write_reg(client, 0x33, 0x0f);
		tp2855_write_reg(client, 0x33, 0x00);
		tp2855_write_reg(client, 0x14, 0xd3);
		tp2855_write_reg(client, 0x14, 0x53);
	}

	/* Disable MIPI CSI2 output */
	tp2855_write_reg(client, 0x23, 0x02);
}

void tp2855_decoder_init(struct v4l2_subdev *client, unsigned char ch,unsigned char fmt,unsigned char std, unsigned char output)
{
	unsigned char tmp;
	const unsigned char SYS_MODE[5] = { 0x01, 0x02, 0x04, 0x08, 0x0f };

    logd("%d",__LINE__);
	tp2855_write_reg(client, 0x40, ch);

	if(PAL == fmt) {
		tmp = tp2855_read_reg(client, 0xf5);
		tmp |= SYS_MODE[ch];
		tp2855_write_reg(client, 0xf5, tmp);

		if( MIPI_2CH4LANE_74M == output) {
			tmp = tp2855_read_reg(client, 0xf5);
			tmp &= ~SYS_MODE[ch];
			tp2855_write_reg(client, 0xf5, tmp);
		}

		tp2855_write_reg(client, 0x02, 0x47);
		tp2855_write_reg(client, 0x0c, 0x13);
		tp2855_write_reg(client, 0x0d, 0x51);

		tp2855_write_reg(client, 0x15, 0x13);
		tp2855_write_reg(client, 0x16, 0x76);
		tp2855_write_reg(client, 0x17, 0x80);
		tp2855_write_reg(client, 0x18, 0x17);
		tp2855_write_reg(client, 0x19, 0x20);
		tp2855_write_reg(client, 0x1a, 0x17);
		tp2855_write_reg(client, 0x1c, 0x09);
		tp2855_write_reg(client, 0x1d, 0x48);

		tp2855_write_reg(client, 0x20, 0x48);
		tp2855_write_reg(client, 0x21, 0x84);
		tp2855_write_reg(client, 0x22, 0x37);
		tp2855_write_reg(client, 0x23, 0x3f);

		tp2855_write_reg(client, 0x2b, 0x70);
		tp2855_write_reg(client, 0x2c, 0x2a);
		tp2855_write_reg(client, 0x2d, 0x64);
		tp2855_write_reg(client, 0x2e, 0x56);

		tp2855_write_reg(client, 0x30, 0x7a);
		tp2855_write_reg(client, 0x31, 0x4a);
		tp2855_write_reg(client, 0x32, 0x4d);
		tp2855_write_reg(client, 0x33, 0xf0);

		tp2855_write_reg(client, 0x35, 0x65);
		tp2855_write_reg(client, 0x38, 0x00);
		tp2855_write_reg(client, 0x39, 0x04);
	} else if(NTSC == fmt) {
		tmp = tp2855_read_reg(client, 0xf5);
		tmp |= SYS_MODE[ch];
		tp2855_write_reg(client, 0xf5, tmp);

		if( MIPI_2CH4LANE_74M == output) {
			tmp = tp2855_read_reg(client, 0xf5);
			tmp &= ~SYS_MODE[ch];
			tp2855_write_reg(client, 0xf5, tmp);
		}

		tp2855_write_reg(client, 0x02, 0x47);
		tp2855_write_reg(client, 0x0c, 0x13);
		tp2855_write_reg(client, 0x0d, 0x50);

		tp2855_write_reg(client, 0x15, 0x13);
		tp2855_write_reg(client, 0x16, 0x60);
		tp2855_write_reg(client, 0x17, 0x80);
		tp2855_write_reg(client, 0x18, 0x12);
		tp2855_write_reg(client, 0x19, 0xf0);
		tp2855_write_reg(client, 0x1a, 0x07);
		tp2855_write_reg(client, 0x1c, 0x09);
		tp2855_write_reg(client, 0x1d, 0x38);

		tp2855_write_reg(client, 0x20, 0x40);
		tp2855_write_reg(client, 0x21, 0x84);
		tp2855_write_reg(client, 0x22, 0x36);
		tp2855_write_reg(client, 0x23, 0x3c);

		tp2855_write_reg(client, 0x2b, 0x70);
		tp2855_write_reg(client, 0x2c, 0x2a);
		tp2855_write_reg(client, 0x2d, 0x68);
		tp2855_write_reg(client, 0x2e, 0x57);

		tp2855_write_reg(client, 0x30, 0x62);
		tp2855_write_reg(client, 0x31, 0xbb);
		tp2855_write_reg(client, 0x32, 0x96);
		tp2855_write_reg(client, 0x33, 0xc0);

		tp2855_write_reg(client, 0x35, 0x65);
		tp2855_write_reg(client, 0x38, 0x00);
		tp2855_write_reg(client, 0x39, 0x04);
	} else if(HD25 == fmt) {
		tmp = tp2855_read_reg(client, 0xf5);
		tmp |= SYS_MODE[ch];
		tp2855_write_reg(client, 0xf5, tmp);

		if( MIPI_2CH4LANE_74M == output) {
			tmp = tp2855_read_reg(client, 0xf5);
			tmp &= ~SYS_MODE[ch];
			tp2855_write_reg(client, 0xf5, tmp);
		}

		tp2855_write_reg(client, 0x02, 0x42);
		tp2855_write_reg(client, 0x07, 0xc0);
		tp2855_write_reg(client, 0x0b, 0xc0);
		tp2855_write_reg(client, 0x0c, 0x13);
		tp2855_write_reg(client, 0x0d, 0x50);

		tp2855_write_reg(client, 0x15, 0x13);
		tp2855_write_reg(client, 0x16, 0x15);
		tp2855_write_reg(client, 0x17, 0x00);
		tp2855_write_reg(client, 0x18, 0x19);
		tp2855_write_reg(client, 0x19, 0xd0);
		tp2855_write_reg(client, 0x1a, 0x25);
		tp2855_write_reg(client, 0x1c, 0x07);  //1280*720, 25fps
		tp2855_write_reg(client, 0x1d, 0xbc);  //1280*720, 25fps

		tp2855_write_reg(client, 0x20, 0x30);
		tp2855_write_reg(client, 0x21, 0x84);
		tp2855_write_reg(client, 0x22, 0x36);
		tp2855_write_reg(client, 0x23, 0x3c);

		tp2855_write_reg(client, 0x2b, 0x60);
		tp2855_write_reg(client, 0x2c, 0x0a);
		tp2855_write_reg(client, 0x2d, 0x30);
		tp2855_write_reg(client, 0x2e, 0x70);

		tp2855_write_reg(client, 0x30, 0x48);
		tp2855_write_reg(client, 0x31, 0xbb);
		tp2855_write_reg(client, 0x32, 0x2e);
		tp2855_write_reg(client, 0x33, 0x90);

		if( MIPI_4CH4LANE_148M == output ||  MIPI_2CH4LANE_74M == output)
			tp2855_write_reg(client, 0x35, 0x65);
		else
			tp2855_write_reg(client, 0x35, 0x25);

		tp2855_write_reg(client, 0x38, 0x00);
		tp2855_write_reg(client, 0x39, 0x18);

		if(STD_HDA == std) {
			tp2855_write_reg(client, 0x02, 0x46);

			tp2855_write_reg(client, 0x0d, 0x71);

			tp2855_write_reg(client, 0x20, 0x40);
			tp2855_write_reg(client, 0x21, 0x46);

			tp2855_write_reg(client, 0x25, 0xfe);
			tp2855_write_reg(client, 0x26, 0x01);

			tp2855_write_reg(client, 0x2c, 0x3a);
			tp2855_write_reg(client, 0x2d, 0x5a);
			tp2855_write_reg(client, 0x2e, 0x40);

			tp2855_write_reg(client, 0x30, 0x9e);
			tp2855_write_reg(client, 0x31, 0x20);
			tp2855_write_reg(client, 0x32, 0x10);
			tp2855_write_reg(client, 0x33, 0x90);
		}
	} else if(HD30 == fmt) {
		tmp = tp2855_read_reg(client, 0xf5);
		tmp |= SYS_MODE[ch];
		tp2855_write_reg(client, 0xf5, tmp);

		if( MIPI_2CH4LANE_74M == output) {
			tmp = tp2855_read_reg(client, 0xf5);
			tmp &= ~SYS_MODE[ch];
			tp2855_write_reg(client, 0xf5, tmp);
		}

		tp2855_write_reg(client, 0x02, 0x42);
		tp2855_write_reg(client, 0x07, 0xc0);
		tp2855_write_reg(client, 0x0b, 0xc0);
		tp2855_write_reg(client, 0x0c, 0x13);
		tp2855_write_reg(client, 0x0d, 0x50);

		tp2855_write_reg(client, 0x15, 0x13);
		tp2855_write_reg(client, 0x16, 0x15);
		tp2855_write_reg(client, 0x17, 0x00);
		tp2855_write_reg(client, 0x18, 0x19);
		tp2855_write_reg(client, 0x19, 0xd0);
		tp2855_write_reg(client, 0x1a, 0x25);
		tp2855_write_reg(client, 0x1c, 0x06);  //1280*720, 30fps
		tp2855_write_reg(client, 0x1d, 0x72);  //1280*720, 30fps

		tp2855_write_reg(client, 0x20, 0x30);
		tp2855_write_reg(client, 0x21, 0x84);
		tp2855_write_reg(client, 0x22, 0x36);
		tp2855_write_reg(client, 0x23, 0x3c);

		tp2855_write_reg(client, 0x2b, 0x60);
		tp2855_write_reg(client, 0x2c, 0x0a);
		tp2855_write_reg(client, 0x2d, 0x30);
		tp2855_write_reg(client, 0x2e, 0x70);

		tp2855_write_reg(client, 0x30, 0x48);
		tp2855_write_reg(client, 0x31, 0xbb);
		tp2855_write_reg(client, 0x32, 0x2e);
		tp2855_write_reg(client, 0x33, 0x90);

		if( MIPI_4CH4LANE_148M == output ||  MIPI_2CH4LANE_74M == output)
			tp2855_write_reg(client, 0x35, 0x65);
		else
			tp2855_write_reg(client, 0x35, 0x25);

		tp2855_write_reg(client, 0x38, 0x00);
		tp2855_write_reg(client, 0x39, 0x18);

		if(STD_HDA == std) {
			tp2855_write_reg(client, 0x02, 0x46);

			tp2855_write_reg(client, 0x0d, 0x70);

			tp2855_write_reg(client, 0x20, 0x40);
			tp2855_write_reg(client, 0x21, 0x46);

			tp2855_write_reg(client, 0x25, 0xfe);
			tp2855_write_reg(client, 0x26, 0x01);

			tp2855_write_reg(client, 0x2c, 0x3a);
			tp2855_write_reg(client, 0x2d, 0x5a);
			tp2855_write_reg(client, 0x2e, 0x40);

			tp2855_write_reg(client, 0x30, 0x9d);
			tp2855_write_reg(client, 0x31, 0xca);
			tp2855_write_reg(client, 0x32, 0x01);
			tp2855_write_reg(client, 0x33, 0xd0);
		}
	} else if(FHD30 == fmt) {
		tmp = tp2855_read_reg(client, 0xf5);
		tmp &= ~SYS_MODE[ch];
		tp2855_write_reg(client, 0xf5, tmp);

		tp2855_write_reg(client, 0x02, 0x40);
		tp2855_write_reg(client, 0x07, 0xc0);
		tp2855_write_reg(client, 0x0b, 0xc0);
		tp2855_write_reg(client, 0x0c, 0x03);
		tp2855_write_reg(client, 0x0d, 0x50);

		tp2855_write_reg(client, 0x15, 0x03);
		tp2855_write_reg(client, 0x16, 0xd2);
		tp2855_write_reg(client, 0x17, 0x80);
		tp2855_write_reg(client, 0x18, 0x29);
		tp2855_write_reg(client, 0x19, 0x38);
		tp2855_write_reg(client, 0x1a, 0x47);
		tp2855_write_reg(client, 0x1c, 0x08);  //1920*1080, 30fps
		tp2855_write_reg(client, 0x1d, 0x98);  //

		tp2855_write_reg(client, 0x20, 0x30);
		tp2855_write_reg(client, 0x21, 0x84);
		tp2855_write_reg(client, 0x22, 0x36);
		tp2855_write_reg(client, 0x23, 0x3c);

		tp2855_write_reg(client, 0x2b, 0x60);
		tp2855_write_reg(client, 0x2c, 0x0a);
		tp2855_write_reg(client, 0x2d, 0x30);
		tp2855_write_reg(client, 0x2e, 0x70);

		tp2855_write_reg(client, 0x30, 0x48);
		tp2855_write_reg(client, 0x31, 0xbb);
		tp2855_write_reg(client, 0x32, 0x2e);
		tp2855_write_reg(client, 0x33, 0x90);

		tp2855_write_reg(client, 0x35, 0x05);
		tp2855_write_reg(client, 0x38, 0x00);
		tp2855_write_reg(client, 0x39, 0x1C);

		if(STD_HDA == std) {
			tp2855_write_reg(client, 0x02, 0x44);

			tp2855_write_reg(client, 0x0d, 0x72);

			tp2855_write_reg(client, 0x15, 0x01);
			tp2855_write_reg(client, 0x16, 0xf0);

			tp2855_write_reg(client, 0x20, 0x38);
			tp2855_write_reg(client, 0x21, 0x46);

			tp2855_write_reg(client, 0x25, 0xfe);
			tp2855_write_reg(client, 0x26, 0x0d);

			tp2855_write_reg(client, 0x2c, 0x3a);
			tp2855_write_reg(client, 0x2d, 0x54);
			tp2855_write_reg(client, 0x2e, 0x40);

			tp2855_write_reg(client, 0x30, 0xa5);
			tp2855_write_reg(client, 0x31, 0x95);
			tp2855_write_reg(client, 0x32, 0xe0);
			tp2855_write_reg(client, 0x33, 0x60);
		}
	} else if(FHD25 == fmt) {
		tmp = tp2855_read_reg(client, 0xf5);
		tmp &= ~SYS_MODE[ch];
		tp2855_write_reg(client, 0xf5, tmp);

		tp2855_write_reg(client, 0x02, 0x40);
		tp2855_write_reg(client, 0x07, 0xc0);
		tp2855_write_reg(client, 0x0b, 0xc0);
		tp2855_write_reg(client, 0x0c, 0x03);
		tp2855_write_reg(client, 0x0d, 0x50);

		tp2855_write_reg(client, 0x15, 0x03);
		tp2855_write_reg(client, 0x16, 0xd2);
		tp2855_write_reg(client, 0x17, 0x80);
		tp2855_write_reg(client, 0x18, 0x29);
		tp2855_write_reg(client, 0x19, 0x38);
		tp2855_write_reg(client, 0x1a, 0x47);

		tp2855_write_reg(client, 0x1c, 0x0a);  //1920*1080, 25fps
		tp2855_write_reg(client, 0x1d, 0x50);  //

		tp2855_write_reg(client, 0x20, 0x30);
		tp2855_write_reg(client, 0x21, 0x84);
		tp2855_write_reg(client, 0x22, 0x36);
		tp2855_write_reg(client, 0x23, 0x3c);

		tp2855_write_reg(client, 0x2b, 0x60);
		tp2855_write_reg(client, 0x2c, 0x0a);
		tp2855_write_reg(client, 0x2d, 0x30);
		tp2855_write_reg(client, 0x2e, 0x70);

		tp2855_write_reg(client, 0x30, 0x48);
		tp2855_write_reg(client, 0x31, 0xbb);
		tp2855_write_reg(client, 0x32, 0x2e);
		tp2855_write_reg(client, 0x33, 0x90);

		tp2855_write_reg(client, 0x35, 0x05);
		tp2855_write_reg(client, 0x38, 0x00);
		tp2855_write_reg(client, 0x39, 0x1C);

		if(STD_HDA == std) {
			tp2855_write_reg(client, 0x02, 0x44);

			tp2855_write_reg(client, 0x0d, 0x73);

			tp2855_write_reg(client, 0x15, 0x01);
			tp2855_write_reg(client, 0x16, 0xf0);

			tp2855_write_reg(client, 0x20, 0x3c);
			tp2855_write_reg(client, 0x21, 0x46);

			tp2855_write_reg(client, 0x25, 0xfe);
			tp2855_write_reg(client, 0x26, 0x0d);

			tp2855_write_reg(client, 0x2c, 0x3a);
			tp2855_write_reg(client, 0x2d, 0x54);
			tp2855_write_reg(client, 0x2e, 0x40);

			tp2855_write_reg(client, 0x30, 0xa5);
			tp2855_write_reg(client, 0x31, 0x86);
			tp2855_write_reg(client, 0x32, 0xfb);
			tp2855_write_reg(client, 0x33, 0x60);
		}
	}
}


#if 0
static int tp2855_common_init(struct v4l2_subdev *client) {
	int ret = 0;

    logd("%d",__LINE__);
	tp2855_write_reg(client, 0x40, CH_ALL);
	tp2855_write_reg(client, 0x4e, 0x00);

	tp2855_decoder_init(client, CH_1, FHD30, STD_HDA, MIPI_4CH4LANE_594M);
	tp2855_decoder_init(client, CH_2, FHD30, STD_HDA, MIPI_4CH4LANE_594M);
	tp2855_decoder_init(client, CH_3, FHD30, STD_HDA, MIPI_4CH4LANE_594M);
	tp2855_decoder_init(client, CH_4, FHD30, STD_HDA, MIPI_4CH4LANE_594M);

	tp2855_mipi_out(client, MIPI_4CH4LANE_594M);

	/* Enable MIPI CSI2 output */
	msleep(100);
	tp2855_write_reg(client, 0x23, 0x00);

	tp2855_write_reg(client, 0x40, CH_ALL);
	return ret;
}
#endif

enum camera_mode {
    MODE_INIT         = 0,
    MODE_SERDES_FSYNC, 
    MODE_SERDES_INTERRUPT,
    MODE_SERDES_REMOTE_SER,
};  

static struct videosource_reg * videosource_reg_table_list[] = {
    sensor_camera_yuv422_8bit_4ch,
    sensor_camera_enable_frame_sync,
    sensor_camera_enable_interrupt,
    sensor_camera_enable_serializer,
};

static int write_regs_tp2855(struct v4l2_subdev *sd, const struct videosource_reg * list, unsigned int mode) {
    unsigned char data[4];
    unsigned char bytes;
    int ret, err_cnt = 0;

    logd("%d",__LINE__);
    while (!((list->reg == REG_TERM) && (list->val == VAL_TERM))) {
        if(list->reg == 0xFF && list->val != 0xFF) {
            mdelay(list->val);
            list++;
        }
        else {
            bytes = 0;
            data[bytes++]= (unsigned char)list->reg&0xff;
            data[bytes++]= (unsigned char)list->val&0xff;

            if(mode == MODE_SERDES_REMOTE_SER) {
				 ret = tp2855_write_reg(sd, list->reg, list->val);
            }
            else {
				 ret = tp2855_write_reg(sd, list->reg, list->val);
            }

            if(ret) {
                if(4 <= ++err_cnt) {
                    printk("ERROR: Sensor I2C !!!! \n");
                    return ret;
                }
            } else {
                err_cnt = 0;
                list++;
            }
        }
    }
    return 0;
}

int tp2855_change_mode( struct v4l2_subdev *sd, int mode) {
    int entry   = sizeof(videosource_reg_table_list) / sizeof(videosource_reg_table_list[0]);
    int ret     = 0;

    logd("%d",__LINE__);
    if((entry <= 0) || (mode < 0) || (entry <= mode)) {
        printk("The list(%d) or the mode index(%d) is wrong\n", entry, mode);
        return -1;
    }

    ret = write_regs_tp2855(sd, videosource_reg_table_list[mode], mode);

    mdelay(10);

    return ret;
}

static int tp2855_parse_device_tree_io(struct tp2855 *dev, struct device_node *node)
{
    struct device_node  *loc_ep = NULL;
    int         ret = 0;

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
        int channel = 0;

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
            rem_ep = (struct device_node *)of_graph_get_remote_endpoint(loc_ep);
            if (!rem_ep) {
                loge("Problem in Remote endpoint");
                ret = -ENODEV;
                goto err;
            }

            remt_dev = (struct device_node *)of_graph_get_port_parent(rem_ep);
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
        dev->channel[channel].sd.fwnode =
            of_fwnode_handle(loc_ep);

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
 * gpio fuctions
 */
int tp2855_parse_device_tree(struct tp2855 *dev, struct i2c_client *client)
{
	struct device_node	*node	= client->dev.of_node;
	int			ret	= 0;

    logd("%d",__LINE__);
	if (node) {
		dev->gpio.pwr_port = of_get_named_gpio_flags(node,
			"pwr-gpios", 0, &dev->gpio.pwr_value);
		dev->gpio.pwd_port = of_get_named_gpio_flags(node,
			"pwd-gpios", 0, &dev->gpio.pwd_value);
		dev->gpio.rst_port = of_get_named_gpio_flags(node,
			"rst-gpios", 0, &dev->gpio.rst_value);
		dev->gpio.intb_port = of_get_named_gpio_flags(node,
			"intb-gpios", 0, &dev->gpio.intb_value);
	} else {
		loge("could not find node!! n");
		ret = -ENODEV;
	}

	return ret;
}

void tp2855_request_gpio(struct tp2855 *dev)
{
    logd("%d",__LINE__);
	if (dev->gpio.pwr_port > 0) {
		/* request power */
		gpio_request(dev->gpio.pwr_port, "tp2855 power");
	}
	if (dev->gpio.pwd_port > 0) {
		/* request power-down */
		gpio_request(dev->gpio.pwd_port, "tp2855 power-down");
	}
	if (dev->gpio.rst_port > 0) {
		/* request reset */
		gpio_request(dev->gpio.rst_port, "tp2855 reset");
	}
	if (dev->gpio.intb_port > 0) {
		/* request intb */
		gpio_request(dev->gpio.intb_port, "tp2855 interrupt");
	}
}

void tp2855_free_gpio(struct tp2855 *dev)
{
    logd("%d",__LINE__);
	if (dev->gpio.pwr_port > 0)
		gpio_free(dev->gpio.pwr_port);
	if (dev->gpio.pwd_port > 0)
		gpio_free(dev->gpio.pwd_port);
	if (dev->gpio.rst_port > 0)
		gpio_free(dev->gpio.rst_port);
	if (dev->gpio.intb_port > 0)
		gpio_free(dev->gpio.intb_port);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int tp2855_init(struct v4l2_subdev *sd, u32 enable)
{
	struct tp2855		*dev	= to_dev(sd);
	int			ret	= 0;

    logd("%d",__LINE__);
	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		//ret = tp2855_common_init(sd);
		ret = tp2855_change_mode(sd, 0);
		ret = tp2855_change_mode(sd, 1);
		if (ret < 0) {
			/* failed to write i2c */
			loge("Fail initializing tp2855 device\n");
		}
	} else if ((dev->i_cnt == 1) && (enable == 0)) {
		/* ret = regmap_write(dev->regmap, 0x15, 0x93); */
	}

	if (enable)
		dev->i_cnt++;
	else
		dev->i_cnt--;

	mutex_unlock(&dev->lock);

	return ret;
}

static int tp2855_set_power(struct v4l2_subdev *sd, int on)
{
	struct tp2855		*dev	= to_dev(sd);
	struct power_sequence	*gpio	= &dev->gpio;

    logd("%d",__LINE__);
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
		if (dev->p_cnt == 1) {
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
		dev->p_cnt--;
	}

	mutex_unlock(&dev->lock);

	return 0;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int tp2855_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct tp2855		*dev	= to_dev(sd);
	unsigned int		val[2];
	unsigned int		data;
	int					ret	= 0;

    logd("%d",__LINE__);
	mutex_lock(&dev->lock);
	
	data = 0;
	ret = regmap_read(dev->regmap, 0xFE, &data);
	if (ret < 0) {
		logd("ERROR: Sensor I2C !!!! \n");
		return -1;
	} else {
		logd("status 1: 0x%02x\n", data);
		val[0] = data;
	}

	data = 0;	
	ret = regmap_read(dev->regmap, 0xFF, &data);
	if (ret < 0) {
		loge("ERROR: Sensor I2C !!!! \n");
		return -1;
	} else {
		logd("status 2: 0x%02x\n", data);
		val[1] = data;
	}
	
	mutex_unlock(&dev->lock);

	if ( val[0] == 0x28 && val[1] == 0x54 )
		return 1;
	else
		return 0;
}

static int tp2855_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct tp2855	*dev	= to_dev(sd);
	int			ret	= 0;

    logd("%d",__LINE__);
	mutex_lock(&dev->lock);

	if ((dev->s_cnt == 0) && (enable == 1)) {
	} else if ((dev->s_cnt == 1) && (enable == 0)) {

			/* issue : mipi overflow error */
			tp2855_write_reg(sd, 0x40, 0x08); //MIPI page
			tp2855_write_reg(sd, 0x23, 0x02); // disable mipi output
			logd("mipi output disabled\n");
	}

	if (enable) {
		/* count up */
		dev->s_cnt++;
	} else {
		/* count down */
		dev->s_cnt--;
	}

	msleep(30);

	mutex_unlock(&dev->lock);
	return ret;
}

static int tp2855_get_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *format)
{
	struct tp2855	*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	tp2855_init_format(dev);
	
	memcpy((void *)&format->format, (const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
    loge("%d : width : %d height : %d",__LINE__, format->format.width, format->format.height);
	return ret;
}

static int tp2855_set_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *format)
{
	struct tp2855	*dev	= to_dev(sd);
	int			ret	= 0;

    loge("%d : width : %d height : %d",__LINE__, format->format.width, format->format.height);
	mutex_lock(&dev->lock);

	memcpy((void *)&dev->fmt, (const void *)&format->format,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static const struct v4l2_subdev_core_ops tp2855_core_ops = {
	.init			= tp2855_init,
	.s_power		= tp2855_set_power,
};

static const struct v4l2_subdev_video_ops tp2855_video_ops = {
	.g_input_status	= tp2855_g_input_status,
	.s_stream		= tp2855_s_stream,
};

static const struct v4l2_subdev_pad_ops tp2855_pad_ops = {
	.get_fmt		= tp2855_get_fmt,
	.set_fmt		= tp2855_set_fmt,
};

static const struct v4l2_subdev_ops tp2855_ops = {
	.core			= &tp2855_core_ops,
	.video			= &tp2855_video_ops,
	.pad			= &tp2855_pad_ops,
};

struct tp2855 tp2855_data = {
};

static const struct i2c_device_id tp2855_id[] = {
	{ "tp2855", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tp2855_id);

#if IS_ENABLED(CONFIG_OF)
const static struct of_device_id tp2855_of_match[] = {
	{
		.compatible	= "techpoint,tp2855",
		.data		= &tp2855_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, tp2855_of_match);
#endif

static int tp2855_notifier_bound(struct v4l2_async_notifier *notifier,
		struct v4l2_subdev *subdev,
		struct v4l2_async_subdev *asd)
{
    struct tp2855_channel *ch     = NULL;
	int         ret     = 0;

    ch = container_of(notifier, struct tp2855_channel, notifier);

	loge("v4l2-subdev %s is bounded\n", subdev->name);

	ch->remote_sd = subdev;

	return ret;
}

static void tp2855_notifier_unbind(struct v4l2_async_notifier *notifier,
		struct v4l2_subdev *subdev,
		struct v4l2_async_subdev *asd)
{
	struct tp2855_channel  *dev        = NULL;

	dev = container_of(notifier, struct tp2855_channel, notifier);

	logi("v4l2-subdev %s is unbounded\n", subdev->name);
}

static const struct v4l2_async_notifier_operations tp2855_notifier_ops = {
	.bound = tp2855_notifier_bound,
	.unbind = tp2855_notifier_unbind,
};

int tp2855_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tp2855 *dev = NULL;
	const struct of_device_id *dev_id = NULL;
    struct tp2855_channel     *ch = NULL;
    int             idxch   = 0;
    int ret = 0;

    logd("%d v1.0",__LINE__);
	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct tp2855), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(tp2855_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* parse the device tree */
	ret = tp2855_parse_device_tree(dev, client);
	if (ret < 0) {
		loge("cannot initialize gpio port\n");
		return ret;
	}


    /* init parameters */
    for (idxch = 0; idxch < NUM_CHANNELS; idxch++) {
        ch = &dev->channel[idxch];

        ch->id = idxch;

        /* Register with V4L2 layer as a slave device */
        v4l2_i2c_subdev_init(&ch->sd, client, &tp2855_ops);
        v4l2_set_subdevdata(&ch->sd, ch);
        tp2855_client = client;

        /* init notifier */
        v4l2_async_notifier_init(&ch->notifier);
        ch->notifier.ops = &tp2855_notifier_ops;

        pr_err("%d channel notifier\n", ch->id);
        ch->dev = dev;
    }

	ret = tp2855_parse_device_tree_io(dev, client->dev.of_node);
    if (ret < 0) {
        loge("tp2855_parse_device_tree_io, ret: %d\n", ret);
        return ret;
    }
	loge("parse device  tree (%d)", ret);


#if 0
	/* register a v4l2 sub device */
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret)
		loge("Failed to register subdevice\n");
	else
		logi("%s is registered as a v4l2 sub device.\n", dev->sd.name);
#else
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

#endif

	/* request gpio */
	tp2855_request_gpio(dev);

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &tp2855_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	tp2855_init_format(dev);

	goto goto_end;

goto_free_device_data:
	/* free the videosource data */
	kfree(dev);

goto_end:
	return ret;
}

int tp2855_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct tp2855		*dev	= to_dev(sd);

    logd("%d",__LINE__);
	/* release regmap */
	regmap_exit(dev->regmap);

	/* gree gpio */
	tp2855_free_gpio(dev);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver tp2855_driver = {
	.probe		= tp2855_probe,
	.remove		= tp2855_remove,
	.driver		= {
		.name		= "tp2855",
		.of_match_table	= of_match_ptr(tp2855_of_match),
	},
	.id_table	= tp2855_id,
};

module_i2c_driver(tp2855_driver);

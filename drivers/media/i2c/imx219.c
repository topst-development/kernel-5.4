// SPDX-License-Identifier: GPL-2.0
/*
 * A V4L2 driver for Sony IMX219 cameras.
 * Copyright (C) 2019, Raspberry Pi (Trading) Ltd
 *
 * Based on Sony imx258 camera driver
 * Copyright (C) 2018 Intel Corporation
 *
 * DT / fwnode changes, and regulator / GPIO control taken from imx214 driver
 * Copyright 2018 Qtechnology A/S
 *
 * Flip handling taken from the Sony IMX319 driver.
 * Copyright (C) 2018 Intel Corporation
 *
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-async.h>
#include <media/v4l2-common.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mediabus.h>
#include <asm/unaligned.h>
#include <linux/of_graph.h>


//#define __FIX__		0

#define IMX219_REG_VALUE_08BIT		1
#define IMX219_REG_VALUE_16BIT		2

#define IMX219_REG_MODE_SELECT		0x0100
#define IMX219_MODE_STANDBY		0x00
#define IMX219_MODE_STREAMING		0x01

/* Chip ID */
#define IMX219_REG_CHIP_ID		0x0000
#define IMX219_CHIP_ID			0x0219

/* External clock frequency is 24.0M */
#define IMX219_XCLK_FREQ		24000000

/* Pixel rate is fixed at 182.4M for all the modes */
#define IMX219_PIXEL_RATE		182400000

#define IMX219_DEFAULT_LINK_FREQ	456000000

/* V_TIMING internal */
#define IMX219_REG_VTS			0x0160
#define IMX219_VTS_15FPS		0x0dc6
#define IMX219_VTS_30FPS_1080P		0x06e3
#define IMX219_VTS_30FPS_BINNED		0x06e3
#define IMX219_VTS_30FPS_640x480	0x06e3
#define IMX219_VTS_MAX			0xffff

#define IMX219_VBLANK_MIN		4

/*Frame Length Line*/
#define IMX219_FLL_MIN			0x08a6
#define IMX219_FLL_MAX			0xffff
#define IMX219_FLL_STEP			1
#define IMX219_FLL_DEFAULT		0x0c98

/* HBLANK control - read only */
#define IMX219_PPL_DEFAULT		3448

/* Exposure control */
#define IMX219_REG_EXPOSURE		0x015a
#define IMX219_EXPOSURE_MIN		4
#define IMX219_EXPOSURE_STEP		1
#define IMX219_EXPOSURE_DEFAULT		0x640
#define IMX219_EXPOSURE_MAX		65535

/* Analog gain control */
#define IMX219_REG_ANALOG_GAIN		0x0157
#define IMX219_ANA_GAIN_MIN		0
#define IMX219_ANA_GAIN_MAX		232
#define IMX219_ANA_GAIN_STEP		1
#define IMX219_ANA_GAIN_DEFAULT		0x0

/* Digital gain control */
#define IMX219_REG_DIGITAL_GAIN		0x0158
#define IMX219_DGTL_GAIN_MIN		0x0100
#define IMX219_DGTL_GAIN_MAX		0x0fff
#define IMX219_DGTL_GAIN_DEFAULT	0x0100
#define IMX219_DGTL_GAIN_STEP		1

#define IMX219_REG_ORIENTATION		0x0172

/* Test Pattern Control */
#define IMX219_REG_TEST_PATTERN		0x0600
#define IMX219_TEST_PATTERN_DISABLE	0
#define IMX219_TEST_PATTERN_SOLID_COLOR	1
#define IMX219_TEST_PATTERN_COLOR_BARS	2
#define IMX219_TEST_PATTERN_GREY_COLOR	3
#define IMX219_TEST_PATTERN_PN9		4

/* Test pattern colour components */
#define IMX219_REG_TESTP_RED		0x0602
#define IMX219_REG_TESTP_GREENR		0x0604
#define IMX219_REG_TESTP_BLUE		0x0606
#define IMX219_REG_TESTP_GREENB		0x0608
#define IMX219_TESTP_COLOUR_MIN		0
#define IMX219_TESTP_COLOUR_MAX		0x03ff
#define IMX219_TESTP_COLOUR_STEP	1
#define IMX219_TESTP_RED_DEFAULT	IMX219_TESTP_COLOUR_MAX
#define IMX219_TESTP_GREENR_DEFAULT	0
#define IMX219_TESTP_BLUE_DEFAULT	0
#define IMX219_TESTP_GREENB_DEFAULT	0


/* Set default resolution */
#define IMX219_DEFAULT_RESOLUTION	1

#define LOG_TAG			"VSRC:IMX219"

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


struct imx219_reg {
	u16 address;
	u8 val;
};

struct imx219_reg_list {
	unsigned int num_of_regs;
	const struct imx219_reg *regs;
};

/* Mode : resolution and related config&values */
struct imx219_mode {
	/* Frame width */
	unsigned int width;
	/* Frame height */
	unsigned int height;

	/* V-timing */
	unsigned int vts_def;

	/* Default register values */
	struct imx219_reg_list reg_list;
};

/*
 * Register sets lifted off the i2C interface from the Raspberry Pi firmware
 * driver.
 * 3280x2464 = mode 2, 1920x1080 = mode 1, 1640x1232 = mode 4, 640x480 = mode 7.
 */
static const struct imx219_reg mode_3280x2464_regs[] = {
//Set-up Registers – [0x0100-0x0147]
	{0x0100, 0x00},  //0: SW standby, 1: Streaming
//Manufacturer Specific Registers – [0x3000-0x5FFF]
	{0x30eb, 0x0c},
	{0x30eb, 0x05},
	{0x300a, 0xff},
	{0x300b, 0xff},
	{0x30eb, 0x05},
	{0x30eb, 0x09},

//Output Set-up Registers – [0x0110-0x0147]
	{0x0114, 0x01},  //CSI_lane_mode  0: Reserved, 1: 2-Lane, 2: Reserved, 3: 4-Lane
	// 0x0144 defines polarity of V-sync signal.  ignal.0: Lo-active, 1: Hi-active
	{0x0128, 0x00}, //MIPI Global timing setting  0: auto mode, 1: manual mode
	{0x012a, 0x18}, //INCK frequency [MHz] EXCK_FREQ[15:8]
	{0x012b, 0x00}, //INCK frequency [MHz] EXCK_FREQ[7:0]

//Frame Bank Control and Group “A” – [0x0150-0x018D]
	{0x0164, 0x00}, //x_addr_start
	{0x0165, 0x00},
	{0x0166, 0x0c}, //x_addr_end
	{0x0167, 0xcf},
	{0x0168, 0x00}, //y_addr_start
	{0x0169, 0x00},
	{0x016a, 0x09}, //y_addr_end
	{0x016b, 0x9f},
	{0x016c, 0x0c}, //output image size (X-direction)
	{0x016d, 0xd0},
	{0x016e, 0x09}, //output image size (Y-direction)
	{0x016f, 0xa0},
	{0x0170, 0x01}, //x_odd_inc
	{0x0171, 0x01}, //y_odd_inc
	{0x0174, 0x00}, //defines binning mode (H-direction).
	{0x0175, 0x00}, //defines binning mode (V-direction).
//0x018C  CSI-2 data format
//Clock Set-up Registers – [0x0300-0x0313]
	{0x0301, 0x05}, //Video Timing Pixel Clock Divider Value
	{0x0303, 0x01}, //Video Timing System Clock Divider Value
	{0x0304, 0x03}, //Pre PLL clock Video Timing System Divider
	{0x0305, 0x03}, //Pre PLL clock Output System Divider Value
	{0x0306, 0x00}, //PLL Video Timing System multiplier Value
	{0x0307, 0x39}, //
	{0x030b, 0x01}, //Output System Clock Divider Value
	{0x030c, 0x00}, //PLL Output System multiplier Value
	{0x030d, 0x72},

//Test Pattern Registers – [0x0600-0x0627]
	{0x0624, 0x0c}, //test_pattern_window_width
	{0x0625, 0xd0},
	{0x0626, 0x09}, //test_pattern_window_height
	{0x0627, 0xa0},

	{0x455e, 0x00},
	{0x471e, 0x4b},
	{0x4767, 0x0f},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47b4, 0x14},
	{0x4713, 0x30},
	{0x478b, 0x10},
	{0x478f, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0e},
	{0x479b, 0x0e},
//Frame Bank Control and Group “A” – [0x0150-0x018D]
	{0x0162, 0x0d}, //line_length_pck
	{0x0163, 0x78},
};

static const struct imx219_reg mode_1920_1080_regs[] = {
	{0x0100, 0x00},
	{0x30eb, 0x05},
	{0x30eb, 0x0c},
	{0x300a, 0xff},
	{0x300b, 0xff},
	{0x30eb, 0x05},
	{0x30eb, 0x09},

	{0x0114, 0x01},
	//{0x0144, 0x00},	// 0x0144 defines polarity of V-sync signalignal.0: Lo-active(default), 1: Hi-active

	{0x0128, 0x00},
	{0x012a, 0x18},
	{0x012b, 0x00},
	//{0x0162, 0x0d},
	//{0x0163, 0x78},
	{0x0164, 0x02},
	{0x0165, 0xa8},
	{0x0166, 0x0a},
	{0x0167, 0x27},
	{0x0168, 0x02},
	{0x0169, 0xb4},
	{0x016a, 0x06},
	{0x016b, 0xeb},
	{0x016c, 0x07},
	{0x016d, 0x80},
	{0x016e, 0x04},
	{0x016f, 0x38},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x39},
	{0x030b, 0x01},
	{0x030c, 0x00},
	{0x030d, 0x72},
	{0x0624, 0x07},
	{0x0625, 0x80},
	{0x0626, 0x04},
	{0x0627, 0x38},
	{0x455e, 0x00},
	{0x471e, 0x4b},
	{0x4767, 0x0f},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47b4, 0x14},
	{0x4713, 0x30},
	{0x478b, 0x10},
	{0x478f, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0e},
	{0x479b, 0x0e},
	//{0x0162, 0x0d},
	//{0x0163, 0x78},
};

static const struct imx219_reg mode_1640_1232_regs[] = {
	{0x0100, 0x00},
	{0x30eb, 0x0c},
	{0x30eb, 0x05},
	{0x300a, 0xff},
	{0x300b, 0xff},
	{0x30eb, 0x05},
	{0x30eb, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012a, 0x18},
	{0x012b, 0x00},
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0c},
	{0x0167, 0xcf},
	{0x0168, 0x00},
	{0x0169, 0x00},
	{0x016a, 0x09},
	{0x016b, 0x9f},
	{0x016c, 0x06},
	{0x016d, 0x68},
	{0x016e, 0x04},
	{0x016f, 0xd0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x01},
	{0x0175, 0x01},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x39},
	{0x030b, 0x01},
	{0x030c, 0x00},
	{0x030d, 0x72},
	{0x0624, 0x06},
	{0x0625, 0x68},
	{0x0626, 0x04},
	{0x0627, 0xd0},
	{0x455e, 0x00},
	{0x471e, 0x4b},
	{0x4767, 0x0f},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47b4, 0x14},
	{0x4713, 0x30},
	{0x478b, 0x10},
	{0x478f, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0e},
	{0x479b, 0x0e},
	{0x0162, 0x0d},
	{0x0163, 0x78},
};

static const struct imx219_reg mode_640_480_regs[] = {
	{0x0100, 0x00},
	{0x30eb, 0x05},
	{0x30eb, 0x0c},
	{0x300a, 0xff},
	{0x300b, 0xff},
	{0x30eb, 0x05},
	{0x30eb, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012a, 0x18},
	{0x012b, 0x00},
	{0x0162, 0x0d},
	{0x0163, 0x78},
	{0x0164, 0x03},
	{0x0165, 0xe8},
	{0x0166, 0x08},
	{0x0167, 0xe7},
	{0x0168, 0x02},
	{0x0169, 0xf0},
	{0x016a, 0x06},
	{0x016b, 0xaf},
	{0x016c, 0x02},
	{0x016d, 0x80},
	{0x016e, 0x01},
	{0x016f, 0xe0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x03},
	{0x0175, 0x03},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x39},
	{0x030b, 0x01},
	{0x030c, 0x00},
	{0x030d, 0x72},
	{0x0624, 0x06},
	{0x0625, 0x68},
	{0x0626, 0x04},
	{0x0627, 0xd0},
	{0x455e, 0x00},
	{0x471e, 0x4b},
	{0x4767, 0x0f},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47b4, 0x14},
	{0x4713, 0x30},
	{0x478b, 0x10},
	{0x478f, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0e},
	{0x479b, 0x0e},
};

static const struct imx219_reg raw8_framefmt_regs[] = {
	{0x018c, 0x08}, //CSI-2 data format 8
	{0x018d, 0x08},
	{0x0309, 0x08}, //Output Pixel Clock Divider Value
};

static const struct imx219_reg raw10_framefmt_regs[] = {
	{0x018c, 0x0a}, //CSI-2 data format 10
	{0x018d, 0x0a},
	{0x0309, 0x0a}, //Output Pixel Clock Divider Value
};

static const char * const imx219_test_pattern_menu[] = {
	"Disabled",
	"Color Bars",
	"Solid Color",
	"Grey Color Bars",
	"PN9"
};

#if defined(__FIX__)


#define IMX219_TABLE_WAIT_MS 0
#define IMX219_TABLE_END 1
#define IMX219_WAIT_MS    10

#if 0
static struct imx219_reg imx219_start[] = {
  { 0x0100, 0x01 }, /* mode select streaming on */
  { IMX219_TABLE_END, 0x00 }
};

 /* mode select streaming off */
static struct imx219_reg imx219_stop[] = {
  { 0x0100, 0x00 },
  { IMX219_TABLE_END, 0x00 }
};
#endif

/* 0.912 GHz */
static struct imx219_reg mode_1920x1080[] = {
  {IMX219_TABLE_WAIT_MS, 10},

  /* analogue gain setting */
  {0x0157, 0x00},
  /* 0x0158, 0x159 Digital gain */

  /* line length: 3448 */
  //{0x0162, 0x0D}, {0x0163, 0x78},
  /* line length: 3559 */
  {0x0162, 0x0D}, {0x0163, 0xE7},

  /* Frame Length: 1766 */
  //{0x0160, 0x06}, {0x0161, 0xE6},
  {0x0160, 0x0D}, {0x0161, 0x09},

  /* Coarse Time */
  {0x015A, 0x0D}, {0x015B, 0x05},


  /* Image Width: 2599 - 680 = 1920 - 1 */
  /* crop_rect.left: 680 Offset */
  {0x0164, 0x02}, {0x0165, 0xA8},
  //{0x0164, 0x02}, {0x0165, 0xAA},
  /* crop_rect.width - 1: 2599 */
  {0x0166, 0x0A}, {0x0167, 0x27},
  //{0x0166, 0x0A}, {0x0167, 0x29},


  /* Image Height: 1771 - 692 = 1080 - 1 */
  /* crop_rect.top: 692 */
  {0x0168, 0x02}, {0x0169, 0xB4},
  /* crop_rect.height - 1: 1771 */
  {0x016A, 0x06}, {0x016B, 0xEB},


  /* crop_rect.width: 1920 */
  {0x016C, 0x07}, {0x016D, 0x80},
  /* crop_rect.height: 1080 */
  {0x016E, 0x04}, {0x016F, 0x38},


  /* X odd increment: 1 */
  {0x0170, 0x01},

  /* Y odd increment: 1 */
  {0x0171, 0x01},

  /* Binning Mode Horizontal: 1 (no binning) */
  //{0x0174, 0x00},
  {0x0174, 0x00},

  /* Binning Mode Horizontal: 1 (no binning) */
  //{0x0175, 0x00},
  {0x0175, 0x00},

  {IMX219_TABLE_WAIT_MS, IMX219_WAIT_MS},
  {IMX219_TABLE_END, 0x00 }
};

static struct imx219_reg mode_table_common[] = {
  /* software reset */
  //{0x0103, 0x01},
  {0x0100, 0x00},

  //{0x6620, 0x01}, {0x6621, 0x01},
  //{0x6622, 0x01}, {0x6623, 0x01},

  /* Access Code to registers over 0x3000 */
  {0x30EB, 0x05},
  {0x30EB, 0x0C},
  {0x300A, 0xFF},
  {0x300B, 0xFF},
  {0x30EB, 0x05},
  {0x30EB, 0x09},

  /* number of csi lanes = 2 (1 + 1) */
  {0x0114, 0x01},

  /* dphy control: Auto */
  {0x0128, 0x00},

  /* external clock frequency = 24MHz */
  {0x012A, 0x18}, // 24 MHz
  {0x012B, 0x00}, // 000 KHz

  {IMX219_TABLE_END, 0x00 }
};

static struct imx219_reg mode_table_clocks[] = {
  //Clock Settings

  /* CSI data format */
  {0x018C, 0x0A}, {0x018D, 0x0A},

  // PIXEL CLOCK USED TO DRIVE SENSOR)
  // VTPXCK_DIV: 4: 1/4, 5: 1/5, 8: 1/8, 10: 1/10
  {0x0301, 0x05},
  // VTSYS_DIV = 1: Divide by 1
  {0x0303, 0x01},

  // PREPLLCK_VT_DIV: 1: 1/1, 2: 1/2, 3: 1/3
  {0x0304, 0x03}, //MUST BE 3 FOR 24MHz

  // PREPLLCK_OP_DIV: 1: 1/1, 2: 1/2, 3: 1/3
  {0x0305, 0x03}, //MUST BE 3 FOR 24MHz

  // PLL_VT_MPY = 57 (Multiplier)
  {0x0306, 0x00}, {0x0307, 0x39},
  //{0x0306, 0x00}, {0x0307, 0x2E},
  //{0x0306, 0x00}, {0x0307, 0x57},

  // OPPXCK_DIV (pixel bit depth): 8:1/8 10:1/10
  {0x0309, 0x0A},
  // OPSYSCK_DIV (divide by two for double data rate): 1:1/2
  {0x030B, 0x01},

  // PLL_OP_MPY = 114 (Multiplier)
  {0x030C, 0x00}, {0x030D, 0x72},
  //{0x030C, 0x00}, {0x030D, 0x5C},
  //{0x030C, 0x00}, {0x030D, 0x5A},


  //Sensor Pixel Clock: EXCLK_FREQ / PREPLLLCK_VT_DIV * PLL_VT_MPY / VTPXCK_DIV: 24MHz / 3 * 57 / 5 = 91 MHz

  // CIS tunning
  {0x455E, 0x00},
  {0x471E, 0x4B},
  {0x4767, 0x0F},
  {0x4750, 0x14},
  {0x4540, 0x00},
  {0x47B4, 0x14},
  {0x4713, 0x30},
  {0x478B, 0x10},
  {0x478F, 0x10},
  {0x4793, 0x10},
  {0x4797, 0x0E},
  {0x479B, 0x0E},

  {IMX219_TABLE_END, 0x00 }
};


#elif defined(__FIX_1_)
/* 0.912 GHz */
static struct imx219_reg mode_1920x1080_TEST[] = {
  /* software reset */
 // {0x0103, 0x01},
  {0x0100, 0x00},

  //{0x6620, 0x01}, {0x6621, 0x01},
  //{0x6622, 0x01}, {0x6623, 0x01},

  /* Access Code to registers over 0x3000 */
  {0x30EB, 0x05},
  {0x30EB, 0x0C},
  {0x300A, 0xFF},
  {0x300B, 0xFF},
  {0x30EB, 0x05},
  {0x30EB, 0x09},

  /* number of csi lanes = 2 (1 + 1) */
  {0x0114, 0x01},

  /* dphy control: Auto */
  {0x0128, 0x00},

  /* external clock frequency = 24MHz */
  {0x012A, 0x18}, // 24 MHz
  {0x012B, 0x00}, // 000 KHz



  /* analogue gain setting */
  {0x0157, 0x00},
  /* 0x0158, 0x159 Digital gain */

  /* line length: 3448 */
  //{0x0162, 0x0D}, {0x0163, 0x78},
  /* line length: 3559 */
  {0x0162, 0x0D}, {0x0163, 0xE7},

  /* Frame Length: 1766 */
  //{0x0160, 0x06}, {0x0161, 0xE6},
  {0x0160, 0x0D}, {0x0161, 0x09},

  /* Coarse Time */
  {0x015A, 0x0D}, {0x015B, 0x05},


  /* Image Width: 2599 - 680 = 1920 - 1 */
  /* crop_rect.left: 680 Offset */
  {0x0164, 0x02}, {0x0165, 0xA8},
  //{0x0164, 0x02}, {0x0165, 0xAA},
  /* crop_rect.width - 1: 2599 */
  {0x0166, 0x0A}, {0x0167, 0x27},
  //{0x0166, 0x0A}, {0x0167, 0x29},


  /* Image Height: 1771 - 692 = 1080 - 1 */
  /* crop_rect.top: 692 */
  {0x0168, 0x02}, {0x0169, 0xB4},
  /* crop_rect.height - 1: 1771 */
  {0x016A, 0x06}, {0x016B, 0xEB},


  /* crop_rect.width: 1920 */
  {0x016C, 0x07}, {0x016D, 0x80},
  /* crop_rect.height: 1080 */
  {0x016E, 0x04}, {0x016F, 0x38},


  /* X odd increment: 1 */
  {0x0170, 0x01},

  /* Y odd increment: 1 */
  {0x0171, 0x01},

  /* Binning Mode Horizontal: 1 (no binning) */
  //{0x0174, 0x00},
  {0x0174, 0x00},

  /* Binning Mode Horizontal: 1 (no binning) */
  //{0x0175, 0x00},
  {0x0175, 0x00},




 //Clock Settings

  /* CSI data format */
  {0x018C, 0x0A}, {0x018D, 0x0A},

  // PIXEL CLOCK USED TO DRIVE SENSOR)
  // VTPXCK_DIV: 4: 1/4, 5: 1/5, 8: 1/8, 10: 1/10
  {0x0301, 0x05},
  // VTSYS_DIV = 1: Divide by 1
  {0x0303, 0x01},

  // PREPLLCK_VT_DIV: 1: 1/1, 2: 1/2, 3: 1/3
  {0x0304, 0x03}, //MUST BE 3 FOR 24MHz

  // PREPLLCK_OP_DIV: 1: 1/1, 2: 1/2, 3: 1/3
  {0x0305, 0x03}, //MUST BE 3 FOR 24MHz

  // PLL_VT_MPY = 57 (Multiplier)
  {0x0306, 0x00}, {0x0307, 0x39},
  //{0x0306, 0x00}, {0x0307, 0x2E},
  //{0x0306, 0x00}, {0x0307, 0x57},

  // OPPXCK_DIV (pixel bit depth): 8:1/8 10:1/10
  {0x0309, 0x0A},
  // OPSYSCK_DIV (divide by two for double data rate): 1:1/2
  {0x030B, 0x01},

  // PLL_OP_MPY = 114 (Multiplier)
  {0x030C, 0x00}, {0x030D, 0x72},
  //{0x030C, 0x00}, {0x030D, 0x5C},
  //{0x030C, 0x00}, {0x030D, 0x5A},


  //Sensor Pixel Clock: EXCLK_FREQ / PREPLLLCK_VT_DIV * PLL_VT_MPY / VTPXCK_DIV: 24MHz / 3 * 57 / 5 = 91 MHz

  // CIS tunning
  {0x455E, 0x00},
  {0x471E, 0x4B},
  {0x4767, 0x0F},
  {0x4750, 0x14},
  {0x4540, 0x00},
  {0x47B4, 0x14},
  {0x4713, 0x30},
  {0x478B, 0x10},
  {0x478F, 0x10},
  {0x4793, 0x10},
  {0x4797, 0x0E},
  {0x479B, 0x0E},

};

#elif defined(__FIX_RK_)

/* MCLK:24MHz  3280x2464  21.2fps   MIPI LANE2 */
static const struct imx219_reg mode_3280x2464_regs[] = {
	{0x30EB, 0x05},		/* Access Code for address over 0x3000 */
	{0x30EB, 0x0C},		/* Access Code for address over 0x3000 */
	{0x300A, 0xFF},		/* Access Code for address over 0x3000 */
	{0x300B, 0xFF},		/* Access Code for address over 0x3000 */
	{0x30EB, 0x05},		/* Access Code for address over 0x3000 */
	{0x30EB, 0x09},		/* Access Code for address over 0x3000 */
	{0x0114, 0x01},		/* CSI_LANE_MODE[1:0} */
	{0x0128, 0x00},		/* DPHY_CNTRL */
	{0x012A, 0x18},		/* EXCK_FREQ[15:8] */
	{0x012B, 0x00},		/* EXCK_FREQ[7:0] */
//	{0x015A, 0x01},		/* INTEG TIME[15:8] */
//	{0x015B, 0xF4},		/* INTEG TIME[7:0] */
	{0x0160, 0x09},		/* FRM_LENGTH_A[15:8] */
	{0x0161, 0xD7},		/* FRM_LENGTH_A[7:0] */
	{0x0162, 0x0D},		/* LINE_LENGTH_A[15:8] */
	{0x0163, 0x78},		/* LINE_LENGTH_A[7:0] */
	{0x0260, 0x09},		/* FRM_LENGTH_B[15:8] */
	{0x0261, 0xC4},		/* FRM_LENGTH_B[7:0] */
	{0x0262, 0x0D},		/* LINE_LENGTH_B[15:8] */
	{0x0263, 0x78},		/* LINE_LENGTH_B[7:0] */
	{0x0170, 0x01},		/* X_ODD_INC_A[2:0] */
	{0x0171, 0x01},		/* Y_ODD_INC_A[2:0] */
	{0x0270, 0x01},		/* X_ODD_INC_B[2:0] */
	{0x0271, 0x01},		/* Y_ODD_INC_B[2:0] */
	{0x0174, 0x00},		/* BINNING_MODE_H_A */
	{0x0175, 0x00},		/* BINNING_MODE_V_A */
	{0x0274, 0x00},		/* BINNING_MODE_H_B */
	{0x0275, 0x00},		/* BINNING_MODE_V_B */
	{0x018C, 0x0A},		/* CSI_DATA_FORMAT_A[15:8] */
	{0x018D, 0x0A},		/* CSI_DATA_FORMAT_A[7:0] */
	{0x028C, 0x0A},		/* CSI_DATA_FORMAT_B[15:8] */
	{0x028D, 0x0A},		/* CSI_DATA_FORMAT_B[7:0] */
	{0x0301, 0x05},		/* VTPXCK_DIV */
	{0x0303, 0x01},		/* VTSYCK_DIV */
	{0x0304, 0x03},		/* PREPLLCK_VT_DIV[3:0] */
	{0x0305, 0x03},		/* PREPLLCK_OP_DIV[3:0] */
	{0x0306, 0x00},		/* PLL_VT_MPY[10:8] */
	{0x0307, 0x39},		/* PLL_VT_MPY[7:0] */
	{0x0309, 0x0A},		/* OPPXCK_DIV[4:0] */
	{0x030B, 0x01},		/* OPSYCK_DIV */
	{0x030C, 0x00},		/* PLL_OP_MPY[10:8] */
	{0x030D, 0x72},		/* PLL_OP_MPY[7:0] */
	{0x455E, 0x00},		/* CIS Tuning */
	{0x471E, 0x4B},		/* CIS Tuning */
	{0x4767, 0x0F},		/* CIS Tuning */
	{0x4750, 0x14},		/* CIS Tuning */
	{0x47B4, 0x14},		/* CIS Tuning */
	{IMX219_TABLE_END, 0x00}
};

/* MCLK:24MHz  1920x1080  30fps   MIPI LANE2 */
static const struct imx219_reg mode_1920_1080_regs[] = {
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012A, 0x18},
	{0x012B, 0x00},
	{0x0160, 0x06},
	{0x0161, 0xE6},
	{0x0162, 0x0D},
	{0x0163, 0x78},
	{0x0164, 0x02},
	{0x0165, 0xA8},
	{0x0166, 0x0A},
	{0x0167, 0x27},
	{0x0168, 0x02},
	{0x0169, 0xB4},
	{0x016A, 0x06},
	{0x016B, 0xEB},
	{0x016C, 0x07},
	{0x016D, 0x80},
	{0x016E, 0x04},
	{0x016F, 0x38},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x39},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x72},
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{IMX219_TABLE_END, 0x00}
};

#endif //__FIX__





static const int imx219_test_pattern_val[] = {
	IMX219_TEST_PATTERN_DISABLE,
	IMX219_TEST_PATTERN_COLOR_BARS,
	IMX219_TEST_PATTERN_SOLID_COLOR,
	IMX219_TEST_PATTERN_GREY_COLOR,
	IMX219_TEST_PATTERN_PN9,
};

/* regulator supplies */
static const char * const imx219_supply_name[] = {
	/* Supplies can be enabled in any order */
	"VANA",  /* Analog (2.8V) supply */
	"VDIG",  /* Digital Core (1.8V) supply */
	"VDDL",  /* IF (1.2V) supply */
};

#define IMX219_NUM_SUPPLIES ARRAY_SIZE(imx219_supply_name)

/*
 * The supported formats.
 * This table MUST contain 4 entries per format, to cover the various flip
 * combinations in the order
 * - no flip
 * - h flip
 * - v flip
 * - h&v flips
 */
static const u32 codes[] = {
	MEDIA_BUS_FMT_UYVY8_1X16,
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SBGGR10_1X10,

	MEDIA_BUS_FMT_SRGGB8_1X8,
	MEDIA_BUS_FMT_SGRBG8_1X8,
	MEDIA_BUS_FMT_SGBRG8_1X8,
	MEDIA_BUS_FMT_SBGGR8_1X8,
};

/*
 * Initialisation delay between XCLR low->high and the moment when the sensor
 * can start capture (i.e. can leave software stanby) must be not less than:
 *   t4 + max(t5, t6 + <time to initialize the sensor register over I2C>)
 * where
 *   t4 is fixed, and is max 200uS,
 *   t5 is fixed, and is 6000uS,
 *   t6 depends on the sensor external clock, and is max 32000 clock periods.
 * As per sensor datasheet, the external clock must be from 6MHz to 27MHz.
 * So for any acceptable external clock t6 is always within the range of
 * 1185 to 5333 uS, and is always less than t5.
 * For this reason this is always safe to wait (t4 + t5) = 6200 uS, then
 * initialize the sensor over I2C, and then exit the software standby.
 *
 * This start-up time can be optimized a bit more, if we start the writes
 * over I2C after (t4+t6), but before (t4+t5) expires. But then sensor
 * initialization over I2C may complete before (t4+t5) expires, and we must
 * ensure that capture is not started before (t4+t5).
 *
 * This delay doesn't account for the power supply startup time. If needed,
 * this should be taken care of via the regulator framework. E.g. in the
 * case of DT for regulator-fixed one should define the startup-delay-us
 * property.
 */
#define IMX219_XCLR_MIN_DELAY_US	6200
#define IMX219_XCLR_DELAY_RANGE_US	1000

/* Mode configs */
static const struct imx219_mode supported_modes[] = {
	{
		/* 8MPix 15fps mode */
		.width = 3280,
		.height = 2464,
		.vts_def = IMX219_VTS_15FPS,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_3280x2464_regs),
			.regs = mode_3280x2464_regs,
		},
	},
	{
		/* 1080P 30fps cropped */
		.width = 1920,
		.height = 1080,
		.vts_def = IMX219_VTS_30FPS_1080P,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1920_1080_regs),
			.regs = mode_1920_1080_regs,
		},
	},
	{
		/* 2x2 binned 30fps mode */
		.width = 1640,
		.height = 1232,
		.vts_def = IMX219_VTS_30FPS_BINNED,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1640_1232_regs),
			.regs = mode_1640_1232_regs,
		},
	},
	{
		/* 640x480 30fps mode */
		.width = 640,
		.height = 480,
		.vts_def = IMX219_VTS_30FPS_640x480,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_640_480_regs),
			.regs = mode_640_480_regs,
		},
	},
};

/*
struct power_sequence {
	int			pwr_port;
	int			rst_port;

	enum of_gpio_flags	pwr_value;
	enum of_gpio_flags	rst_value;
};
*/

#define NUM_CHANNELS (1)

struct imx219_channel {
    int             id;
    struct v4l2_subdev      sd;

    struct v4l2_async_subdev    asd;
    struct v4l2_async_notifier  notifier;
    struct v4l2_subdev      *remote_sd;

    struct imx219         *dev;
};

struct imx219 {
	struct media_pad pad;

	struct v4l2_mbus_framefmt fmt;

	struct clk *xclk; /* system clock to IMX219 */
	u32 xclk_freq;

	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[IMX219_NUM_SUPPLIES];

	struct v4l2_ctrl_handler ctrl_handler;
	/* V4L2 Controls */
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *vflip;
	struct v4l2_ctrl *hflip;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;

	/* Current mode */
	const struct imx219_mode *mode;

	/*
	 * Mutex for serialized access:
	 * Protect sensor module set pad format and start/stop streaming safely.
	 */
	struct mutex mutex;

	/* Streaming on/off */
	bool streaming;

	struct imx219_channel     channel[NUM_CHANNELS];
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;

	struct i2c_client *client;
};


static inline struct imx219_channel *to_channel(struct v4l2_subdev *sd)
{
    return v4l2_get_subdevdata(sd);
}

static inline struct imx219 *to_dev(struct v4l2_subdev *sd)
{
    struct imx219_channel  *ch = to_channel(sd);
    return ch->dev;
}

/* Read registers up to 2 at a time */
static int imx219_read_reg(struct imx219 *imx219, u16 reg, u32 len, u32 *val)
{
	struct i2c_client *client = imx219->client;
	struct i2c_msg msgs[2];
	u8 addr_buf[2] = { reg >> 8, reg & 0xff };
	u8 data_buf[4] = { 0, };
	int ret;

	if (len > 4) {
		loge("len %d client->addr 0x%x\n",len, client->addr);
		return -EINVAL;
	}

	logi("len %d client->addr 0x%x\n",len, client->addr);

	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = ARRAY_SIZE(addr_buf);
	msgs[0].buf = addr_buf;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_buf[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs)) {
		loge("\n i2c_transfer \n");
		return -EIO;
	}

	*val = get_unaligned_be32(data_buf);

	return 0;
}

/* Write registers up to 2 at a time */
static int imx219_write_reg(struct imx219 *imx219, u16 reg, u32 len, u32 val)
{
	struct i2c_client *client = imx219->client;
	u8 buf[6];

	if (len > 4) {
		loge("[%d] len %d client->addr 0x%x\n",__LINE__, len, client->addr);
		return -EINVAL;
	}

	put_unaligned_be16(reg, buf);
	put_unaligned_be32(val << (8 * (4 - len)), buf + 2);
	//logi("[%d] buf: %x%x%x%x%x%x\n",__LINE__, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
	//logi("[%d] reg %d len %d val %d client->addr 0x%x\n",__LINE__, reg, len, val, client->addr);

	if (i2c_master_send(client, buf, len + 2) != len + 2) {
		loge("[%d] buf: %x%x%x%x%x%x\n",__LINE__, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
		loge("[%d] reg %d len %d val %d client->addr 0x%x\n",__LINE__, reg, len, val, client->addr);
		return -EIO;
	}

	return 0;
}

/* Write a list of registers */
static int imx219_write_regs(struct imx219 *imx219,
			     const struct imx219_reg *regs, u32 len)
{
	struct i2c_client *client = imx219->client;
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		//logi("len %d, regs[%d].address 0x%x\n", len, i, regs[i].address);

		ret = imx219_write_reg(imx219, regs[i].address, 1, regs[i].val);
		if (ret) {
			dev_err_ratelimited(&client->dev,
					    "Failed to write reg 0x%4.4x. error = %d\n",
					    regs[i].address, ret);

			logi("len %d client->addr 0x%x\n",len, client->addr);
			return ret;
		}
	}

	return 0;
}

/* Get bayer order based on flip setting. */
static u32 imx219_get_format_code(struct imx219 *imx219, u32 code)
{
	unsigned int i;
	lockdep_assert_held(&imx219->mutex);

	logi("code 0x%8x\n", code);

	for (i = 0; i < ARRAY_SIZE(codes); i++)
		if (codes[i] == code)
			break;

	if (i >= ARRAY_SIZE(codes))
		i = 0;

	i = (i & ~3) | (imx219->vflip->val ? 2 : 0) |
	    (imx219->hflip->val ? 1 : 0);

	logi("codes[%d] 0x%8x\n", i, codes[i]);
	return codes[i];
}

static void imx219_set_default_format(struct imx219 *imx219)
{
	struct v4l2_mbus_framefmt *fmt;
	fmt = &imx219->fmt;
	//fmt->code = MEDIA_BUS_FMT_SRGGB10_1X10;
	fmt->code = MEDIA_BUS_FMT_YUYV8_2X8;
	fmt->code = MEDIA_BUS_FMT_UYVY8_1X16;

	fmt->colorspace = V4L2_COLORSPACE_RAW;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_MAP_QUANTIZATION_DEFAULT(true,
							  fmt->colorspace,
							  fmt->ycbcr_enc);
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
	fmt->width = supported_modes[IMX219_DEFAULT_RESOLUTION].width;
	fmt->height = supported_modes[IMX219_DEFAULT_RESOLUTION].height;
	fmt->field = V4L2_FIELD_NONE;
	logi("\n");
}

static int imx219_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx219 *imx219 = to_dev(sd);
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->pad, 0);

	mutex_lock(&imx219->mutex);

	/* Initialize try_fmt */
	try_fmt->width = supported_modes[IMX219_DEFAULT_RESOLUTION].width;
	try_fmt->height = supported_modes[IMX219_DEFAULT_RESOLUTION].height;
	try_fmt->code = imx219_get_format_code(imx219,
					       MEDIA_BUS_FMT_UYVY8_1X16);//MEDIA_BUS_FMT_SRGGB10_1X10);
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&imx219->mutex);
	logi("\n");

	return 0;
}

static int imx219_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx219 *imx219 =
		container_of(ctrl->handler, struct imx219, ctrl_handler);
	struct i2c_client *client = imx219->client;
	int ret;

	logi("ctrl->id 0x%8x\n", ctrl->id);
	if (ctrl->id == V4L2_CID_VBLANK) {
		int exposure_max, exposure_def;

		/* Update max exposure while meeting expected vblanking */
		exposure_max = imx219->mode->height + ctrl->val - 4;
		exposure_def = (exposure_max < IMX219_EXPOSURE_DEFAULT) ?
			exposure_max : IMX219_EXPOSURE_DEFAULT;
		__v4l2_ctrl_modify_range(imx219->exposure,
					 imx219->exposure->minimum,
					 exposure_max, imx219->exposure->step,
					 exposure_def);
	}

	/*
	 * Applying V4L2 control value only happens
	 * when power is up for streaming
	 */
	if (pm_runtime_get_if_in_use(&client->dev) == 0) {
		loge("error\n");
		return 0;
	}

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret = imx219_write_reg(imx219, IMX219_REG_ANALOG_GAIN,
				       IMX219_REG_VALUE_08BIT, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret = imx219_write_reg(imx219, IMX219_REG_EXPOSURE,
				       IMX219_REG_VALUE_16BIT, ctrl->val);
		break;
	case V4L2_CID_DIGITAL_GAIN:
		ret = imx219_write_reg(imx219, IMX219_REG_DIGITAL_GAIN,
				       IMX219_REG_VALUE_16BIT, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = imx219_write_reg(imx219, IMX219_REG_TEST_PATTERN,
				       IMX219_REG_VALUE_16BIT,
				       imx219_test_pattern_val[ctrl->val]);
		break;
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
		ret = imx219_write_reg(imx219, IMX219_REG_ORIENTATION, 1,
				       imx219->hflip->val |
				       imx219->vflip->val << 1);
		break;
	case V4L2_CID_VBLANK:
		ret = imx219_write_reg(imx219, IMX219_REG_VTS,
				       IMX219_REG_VALUE_16BIT,
				       imx219->mode->height + ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN_RED:
		ret = imx219_write_reg(imx219, IMX219_REG_TESTP_RED,
				       IMX219_REG_VALUE_16BIT, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN_GREENR:
		ret = imx219_write_reg(imx219, IMX219_REG_TESTP_GREENR,
				       IMX219_REG_VALUE_16BIT, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN_BLUE:
		ret = imx219_write_reg(imx219, IMX219_REG_TESTP_BLUE,
				       IMX219_REG_VALUE_16BIT, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN_GREENB:
		ret = imx219_write_reg(imx219, IMX219_REG_TESTP_GREENB,
				       IMX219_REG_VALUE_16BIT, ctrl->val);
		break;
	default:
		dev_info(&client->dev,
			 "ctrl(id:0x%x,val:0x%x) is not handled\n",
			 ctrl->id, ctrl->val);
		loge("error\n");
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx219_ctrl_ops = {
	.s_ctrl = imx219_set_ctrl,
};

static int imx219_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx219 *imx219 = to_dev(sd);

	if (code->index >= (ARRAY_SIZE(codes) / 4)) {
		loge("code->index %d\n", code->index);
		return -EINVAL;
	}

	code->code = imx219_get_format_code(imx219, codes[code->index * 4]);

	return 0;
}

static int imx219_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx219 *imx219 = to_dev(sd);

	if (fse->index >= ARRAY_SIZE(supported_modes)) {
		loge("fse->index %d\n", fse->index);
		return -EINVAL;
	}

	if (fse->code != imx219_get_format_code(imx219, imx219->fmt.code)){
		loge("fse->code %d\n", fse->code);
		return -EINVAL;
	}

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = supported_modes[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

static void imx219_reset_colorspace(struct v4l2_mbus_framefmt *fmt)
{
	logi("\n");
	/*fmt->colorspace = V4L2_COLORSPACE_RAW;//V4L2_COLORSPACE_SRGB;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_MAP_QUANTIZATION_DEFAULT(true,
							  fmt->colorspace,
							  fmt->ycbcr_enc);
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
	*/
}

static void imx219_update_pad_format(struct imx219 *imx219,
				     const struct imx219_mode *mode,
				     struct v4l2_subdev_format *fmt)
{
	logi("mode->width %d, mode->height %d\n", mode->width, mode->height);
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	fmt->format.code	= MEDIA_BUS_FMT_UYVY8_1X16;

	imx219_reset_colorspace(&fmt->format);
}

static int __imx219_get_pad_format(struct imx219 *imx219,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_format *fmt)
{
	logi("fmt->which %d\n", fmt->which);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(&imx219->channel[0].sd, cfg, fmt->pad);
		/* update the code which could change due to vflip or hflip: */
		try_fmt->code = imx219_get_format_code(imx219, try_fmt->code);
		fmt->format = *try_fmt;
	} else {
		imx219_update_pad_format(imx219, imx219->mode, fmt);
		fmt->format.code = imx219_get_format_code(imx219,
							  imx219->fmt.code);
	}

	return 0;
}

static int imx219_get_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *fmt)
{
	struct imx219 *imx219 = to_dev(sd);
	int ret;
	logi("\n");
	mutex_lock(&imx219->mutex);
	ret = __imx219_get_pad_format(imx219, cfg, fmt);
	mutex_unlock(&imx219->mutex);

	return ret;
}

static int imx219_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *fmt)
{
	struct imx219 *imx219 = to_dev(sd);
	const struct imx219_mode *mode;
	struct v4l2_mbus_framefmt *framefmt;
	int exposure_max, exposure_def, hblank;
	unsigned int i;
	logi("\n");

	mutex_lock(&imx219->mutex);

	for (i = 0; i < ARRAY_SIZE(codes); i++)
		if (codes[i] == fmt->format.code)
			break;
	if (i >= ARRAY_SIZE(codes))
		i = 0;

	/* Bayer order varies with flips */
	fmt->format.code = imx219_get_format_code(imx219, codes[i]);

	mode = v4l2_find_nearest_size(supported_modes,
				      ARRAY_SIZE(supported_modes),
				      width, height,
				      fmt->format.width, fmt->format.height);
	imx219_update_pad_format(imx219, mode, fmt);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		framefmt = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		*framefmt = fmt->format;
	} else if (imx219->mode != mode ||
		   imx219->fmt.code != fmt->format.code) {
		imx219->fmt = fmt->format;
		imx219->mode = mode;
		/* Update limits and set FPS to default */
		__v4l2_ctrl_modify_range(imx219->vblank, IMX219_VBLANK_MIN,
					 IMX219_VTS_MAX - mode->height, 1,
					 mode->vts_def - mode->height);
		__v4l2_ctrl_s_ctrl(imx219->vblank,
				   mode->vts_def - mode->height);
		/* Update max exposure while meeting expected vblanking */
		exposure_max = mode->vts_def - 4;
		exposure_def = (exposure_max < IMX219_EXPOSURE_DEFAULT) ?
			exposure_max : IMX219_EXPOSURE_DEFAULT;
		__v4l2_ctrl_modify_range(imx219->exposure,
					 imx219->exposure->minimum,
					 exposure_max, imx219->exposure->step,
					 exposure_def);
		/*
		 * Currently PPL is fixed to IMX219_PPL_DEFAULT, so hblank
		 * depends on mode->width only, and is not changeble in any
		 * way other than changing the mode.
		 */
		hblank = IMX219_PPL_DEFAULT - mode->width;
		__v4l2_ctrl_modify_range(imx219->hblank, hblank, hblank, 1,
					 hblank);
	}

	mutex_unlock(&imx219->mutex);

	return 0;
}

#if !defined(__FIX__)
static int imx219_set_framefmt(struct imx219 *imx219)
{
	logi("imx219->fmt.code 0x%x\n", imx219->fmt.code);
	switch (imx219->fmt.code) {
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_UYVY8_1X16:
		return imx219_write_regs(imx219, raw8_framefmt_regs,
					ARRAY_SIZE(raw8_framefmt_regs));

	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
		return imx219_write_regs(imx219, raw10_framefmt_regs,
					ARRAY_SIZE(raw10_framefmt_regs));
	}

	return -EINVAL;
}
#endif

static int imx219_start_streaming(struct imx219 *imx219)
{
	struct i2c_client *client = imx219->client;
	const struct imx219_reg_list *reg_list;
	int ret;


	/* Apply default values of current mode */
	reg_list = &imx219->mode->reg_list;

	logi("reg_list->num_of_regs %d\n", reg_list->num_of_regs);

#if !defined(__FIX__)

	ret = imx219_write_regs(imx219, reg_list->regs, reg_list->num_of_regs);
	if (ret) {
		dev_err(&client->dev, "%s failed to set mode\n", __func__);
		return ret;
	}

	ret = imx219_set_framefmt(imx219);
	if (ret) {
		dev_err(&client->dev, "%s failed to set frame format: %d\n",
			__func__, ret);
		return ret;
	}

#elif defined(__FIX_1_)
	ret = imx219_write_regs(imx219, reg_list->regs, reg_list->num_of_regs);
	if (ret) {
		dev_err(&client->dev, "%s failed to set mode\n", __func__);
		return ret;
	}

	ret = imx219_set_framefmt(imx219);
	if (ret) {
		dev_err(&client->dev, "%s failed to set frame format: %d\n",
			__func__, ret);
		return ret;
	}
#else
/*
	ret = imx219_write_regs(imx219, imx219_stop, ARRAY_SIZE(imx219_stop));
	if (ret) {
		dev_err(&client->dev, "%s failed to imx219_stop\n", __func__);
		return ret;
	}
*/
	ret = imx219_write_regs(imx219, mode_table_common, ARRAY_SIZE(mode_table_common));
	if (ret) {
		dev_err(&client->dev, "%s failed to mode_table_common\n", __func__);
		return ret;
	}

	ret = imx219_write_regs(imx219, mode_1920x1080, ARRAY_SIZE(mode_1920x1080));
	if (ret) {
		dev_err(&client->dev, "%s failed to mode_1920x1080\n", __func__);
		return ret;
	}

	ret = imx219_write_regs(imx219, mode_table_clocks, ARRAY_SIZE(mode_table_clocks));
	if (ret) {
		dev_err(&client->dev, "%s failed to mode_table_clocks\n", __func__);
		return ret;
	}

	/*ret = imx219_write_regs(imx219, imx219_start, ARRAY_SIZE(imx219_start));
	if (ret) {
		dev_err(&client->dev, "%s failed to imx219_start\n", __func__);
		return ret;
	}*/
#endif

	/* Apply customized values from user */
	ret =  __v4l2_ctrl_handler_setup(imx219->channel[0].sd.ctrl_handler);
	if (ret)
		return ret;

	/* set stream on register */
	return imx219_write_reg(imx219, IMX219_REG_MODE_SELECT,
				IMX219_REG_VALUE_08BIT, IMX219_MODE_STREAMING);
}

static void imx219_stop_streaming(struct imx219 *imx219)
{
	struct i2c_client *client = imx219->client;
	int ret;

	logi("imx219->s_cnt %d\n", imx219->s_cnt);

	if (imx219->s_cnt > 0)
		imx219->s_cnt--;

	/* set stream off register */
	ret = imx219_write_reg(imx219, IMX219_REG_MODE_SELECT,
			       IMX219_REG_VALUE_08BIT, IMX219_MODE_STANDBY);
	if (ret)
		dev_err(&client->dev, "%s failed to set stream\n", __func__);
}


/*
 * v4l2_subdev_video_ops implementations
 */


static int imx219_init(struct v4l2_subdev *sd, u32 enable)
{
	struct imx219 *imx219 = to_dev(sd);
	mutex_lock(&imx219->mutex);

	logi("\n");

	mutex_unlock(&imx219->mutex);

	return 1;
}

static int imx219_set_power(struct v4l2_subdev *sd, int on)
{
	struct imx219 *imx219 = to_dev(sd);
	mutex_lock(&imx219->mutex);

	logi("\n");

	mutex_unlock(&imx219->mutex);

	return 1;
}

static int imx219_g_input_status(struct v4l2_subdev *sd, unsigned int *status)
{
	struct imx219 *imx219 = to_dev(sd);
	mutex_lock(&imx219->mutex);

	logi("\n");
	*status = 1;

	mutex_unlock(&imx219->mutex);

	return 1;
}

static int imx219_s_frame_interval(struct v4l2_subdev *sd, struct v4l2_subdev_frame_interval *interval)
{
	struct imx219 *imx219 = to_dev(sd);
	mutex_lock(&imx219->mutex);

	logi("\n");

	interval->pad = 0;
	interval->interval.numerator = 1;
	//interval->interval.denominator = dev->framerate;

	mutex_unlock(&imx219->mutex);

	return 1;
}

static int imx219_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx219 *imx219 = to_dev(sd);
	//struct i2c_client *client = imx219->client;
	int ret = 0;

	mutex_lock(&imx219->mutex);

	logi("imx219->streaming %d enable %d imx219->s_cnt %d \n", imx219->streaming, enable, imx219->s_cnt);

	if ((imx219->s_cnt == 0) && (enable == 1)) {
	} else if ((imx219->s_cnt == 1) && (enable == 0)) {

		ret = imx219_start_streaming(imx219);
		if (ret) {
			loge("imx219_start_streaming error\n");
			//goto err_rpm_put;
		}
	}

	if (enable) {
		/* count up */
		imx219->s_cnt++;
	} else {
		/* count down */
		imx219->s_cnt--;
	}
	mutex_unlock(&imx219->mutex);

#if 0
	if (imx219->s_cnt++ > 0) {
		mutex_unlock(&imx219->mutex);
		return 0;
	}

	if (imx219->streaming == enable) {
		mutex_unlock(&imx219->mutex);
		return 0;
	}

	if (1){//enable) {
		/*ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			loge("[%d] ret %d \n", __LINE__, ret);
			goto err_unlock;
		}
		*/

			logi("[%d] ret %d \n", __LINE__, ret);
		/*
		 * Apply default & customized values
		 * and then start streaming.
		 */
		ret = imx219_start_streaming(imx219);
		if (ret)
			goto err_rpm_put;
	} else {
		imx219_stop_streaming(imx219);
		//pm_runtime_put(&client->dev);
	}

	imx219->streaming = enable;

	/* vflip and hflip cannot change during streaming */
	__v4l2_ctrl_grab(imx219->vflip, enable);
	__v4l2_ctrl_grab(imx219->hflip, enable);

	mutex_unlock(&imx219->mutex);

	return ret;

err_rpm_put:
	pm_runtime_put(&client->dev);
//err_unlock:
	mutex_unlock(&imx219->mutex);
#endif

	loge("ret %d \n", ret);
	return ret;
}

/* Power/clock management functions */
static int imx219_power_on(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx219 *imx219 = to_dev(sd);
	int ret;

	logi("imx219->streaming %d imx219->s_cnt %d \n", imx219->streaming, imx219->p_cnt++);

	ret = regulator_bulk_enable(IMX219_NUM_SUPPLIES,
				    imx219->supplies);
	if (ret) {
		dev_err(&client->dev, "%s: failed to enable regulators\n",
			__func__);
		return ret;
	}

	ret = clk_prepare_enable(imx219->xclk);
	if (ret) {
		dev_err(&client->dev, "%s: failed to enable clock\n",
			__func__);
		goto reg_off;
	}

	gpiod_set_value_cansleep(imx219->reset_gpio, 1);
	usleep_range(IMX219_XCLR_MIN_DELAY_US,
		     IMX219_XCLR_MIN_DELAY_US + IMX219_XCLR_DELAY_RANGE_US);

	return 0;

reg_off:
	regulator_bulk_disable(IMX219_NUM_SUPPLIES, imx219->supplies);

	return ret;
}

static int imx219_power_off(struct device *dev)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx219 *imx219 = to_dev(sd);

	logi("imx219->streaming %d imx219->s_cnt %d \n", imx219->streaming, imx219->p_cnt--);

	gpiod_set_value_cansleep(imx219->reset_gpio, 0);
	regulator_bulk_disable(IMX219_NUM_SUPPLIES, imx219->supplies);
	clk_disable_unprepare(imx219->xclk);

	return 0;
}

static int __maybe_unused imx219_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx219 *imx219 = to_dev(sd);
	logi("imx219->streaming %d\n", imx219->streaming);
	if (imx219->streaming)
		imx219_stop_streaming(imx219);

	return 0;
}

static int __maybe_unused imx219_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx219 *imx219 = to_dev(sd);
	int ret;
	logi("imx219->streaming %d\n", imx219->streaming);

	if (imx219->streaming) {
		ret = imx219_start_streaming(imx219);
		if (ret)
			goto error;
	}

	return 0;

error:
	imx219_stop_streaming(imx219);
	imx219->streaming = 0;

	return ret;
}

static int imx219_get_regulators(struct imx219 *imx219)
{
	unsigned int i;
	struct i2c_client *client = imx219->client;
	logi("\n");

	for (i = 0; i < IMX219_NUM_SUPPLIES; i++)
		imx219->supplies[i].supply = imx219_supply_name[i];

	return devm_regulator_bulk_get(&client->dev,
				       IMX219_NUM_SUPPLIES,
				       imx219->supplies);
}

/* Verify chip ID */
static int imx219_identify_module(struct imx219 *imx219)
{
	struct i2c_client *client = imx219->client;
	int ret;
	u32 val;
	logi("\n");

	ret = imx219_read_reg(imx219, IMX219_REG_CHIP_ID,
			      IMX219_REG_VALUE_16BIT, &val);



	if (ret) {
		dev_err(&client->dev, "failed to read chip id %x\n",
			IMX219_CHIP_ID);
		return ret;
	}


	if (val != IMX219_CHIP_ID) {
		dev_err(&client->dev, "chip id mismatch: %x!=%x\n",
			IMX219_CHIP_ID, val);
		return -EIO;
	}

	return 0;
}

static const struct v4l2_subdev_core_ops imx219_core_ops = {
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
	.s_power		= imx219_set_power,
	.init			= imx219_init,
};

static const struct v4l2_subdev_video_ops imx219_video_ops = {
	.g_input_status	= imx219_g_input_status,
	.s_stream = imx219_set_stream,
	.s_frame_interval = imx219_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx219_pad_ops = {
	.enum_mbus_code = imx219_enum_mbus_code,
	.get_fmt = imx219_get_pad_format,
	.set_fmt = imx219_set_pad_format,
	.enum_frame_size = imx219_enum_frame_size,
};

static const struct v4l2_subdev_ops imx219_subdev_ops = {
	.core = &imx219_core_ops,
	.video = &imx219_video_ops,
	.pad = &imx219_pad_ops,
};

static const struct v4l2_subdev_internal_ops imx219_internal_ops = {
	.open = imx219_open,
};


/* Initialize control handlers */
static int imx219_init_controls(struct imx219 *imx219)
{
	struct i2c_client *client = imx219->client;
	struct v4l2_ctrl_handler *ctrl_hdlr;
	unsigned int height = imx219->mode->height;
	int exposure_max, exposure_def, hblank;
	int i, ret;
	logi("\n");

	ctrl_hdlr = &imx219->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 9);
	if (ret) {
		loge("error \n");
		return ret;
	}

	mutex_init(&imx219->mutex);
	ctrl_hdlr->lock = &imx219->mutex;

	/* By default, PIXEL_RATE is read only */
	imx219->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       IMX219_PIXEL_RATE,
					       IMX219_PIXEL_RATE, 1,
					       IMX219_PIXEL_RATE);

	/* Initial vblank/hblank/exposure parameters based on current mode */
	imx219->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops,
					   V4L2_CID_VBLANK, IMX219_VBLANK_MIN,
					   IMX219_VTS_MAX - height, 1,
					   imx219->mode->vts_def - height);
	hblank = IMX219_PPL_DEFAULT - imx219->mode->width;
	imx219->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops,
					   V4L2_CID_HBLANK, hblank, hblank,
					   1, hblank);
	if (imx219->hblank)
		imx219->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	exposure_max = imx219->mode->vts_def - 4;
	exposure_def = (exposure_max < IMX219_EXPOSURE_DEFAULT) ?
		exposure_max : IMX219_EXPOSURE_DEFAULT;
	imx219->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     IMX219_EXPOSURE_MIN, exposure_max,
					     IMX219_EXPOSURE_STEP,
					     exposure_def);

	v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  IMX219_ANA_GAIN_MIN, IMX219_ANA_GAIN_MAX,
			  IMX219_ANA_GAIN_STEP, IMX219_ANA_GAIN_DEFAULT);

	v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  IMX219_DGTL_GAIN_MIN, IMX219_DGTL_GAIN_MAX,
			  IMX219_DGTL_GAIN_STEP, IMX219_DGTL_GAIN_DEFAULT);

	imx219->hflip = v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops,
					  V4L2_CID_HFLIP, 0, 1, 1, 0);
	if (imx219->hflip)
		imx219->hflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	imx219->vflip = v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops,
					  V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (imx219->vflip)
		imx219->vflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &imx219_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(imx219_test_pattern_menu) - 1,
				     0, 0, imx219_test_pattern_menu);
	for (i = 0; i < 4; i++) {
		/*
		 * The assumption is that
		 * V4L2_CID_TEST_PATTERN_GREENR == V4L2_CID_TEST_PATTERN_RED + 1
		 * V4L2_CID_TEST_PATTERN_BLUE   == V4L2_CID_TEST_PATTERN_RED + 2
		 * V4L2_CID_TEST_PATTERN_GREENB == V4L2_CID_TEST_PATTERN_RED + 3
		 */
		v4l2_ctrl_new_std(ctrl_hdlr, &imx219_ctrl_ops,
				  V4L2_CID_TEST_PATTERN_RED + i,
				  IMX219_TESTP_COLOUR_MIN,
				  IMX219_TESTP_COLOUR_MAX,
				  IMX219_TESTP_COLOUR_STEP,
				  IMX219_TESTP_COLOUR_MAX);
		/* The "Solid color" pattern is white by default */
	}

	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&client->dev, "%s control init failed (%d)\n",
			__func__, ret);
		goto error;
	}

	imx219->channel[0].sd.ctrl_handler = ctrl_hdlr;
	return 0;

error:
	v4l2_ctrl_handler_free(ctrl_hdlr);
	mutex_destroy(&imx219->mutex);

	return ret;
}

static void imx219_free_controls(struct imx219 *imx219)
{
	logi("\n");
	v4l2_ctrl_handler_free(imx219->channel[0].sd.ctrl_handler);
	mutex_destroy(&imx219->mutex);
}

static int imx219_check_hwcfg(struct device *dev)
{
	struct fwnode_handle *endpoint;
	struct v4l2_fwnode_endpoint ep_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	int ret = -EINVAL;
	return 0;
	//Do not free remark due to kernel panic issues.
	//logi("\n");

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!endpoint) {
		dev_err(dev, "endpoint node not found\n");
		return -EINVAL;
	}

	if (v4l2_fwnode_endpoint_alloc_parse(endpoint, &ep_cfg)) {
		dev_err(dev, "could not parse endpoint\n");
		goto error_out;
	}

	/* Check the number of MIPI CSI2 data lanes */
	if (ep_cfg.bus.mipi_csi2.num_data_lanes != 2) {
		dev_err(dev, "only 2 data lanes are currently supported >>>\n");
		goto error_out;
	}

	/* Check the link frequency set in device tree */
	if (!ep_cfg.nr_of_link_frequencies) {
		dev_err(dev, "link-frequency property not found in DT\n");
		goto error_out;
	}

	if (ep_cfg.nr_of_link_frequencies != 1 ||
	    ep_cfg.link_frequencies[0] != IMX219_DEFAULT_LINK_FREQ) {
		dev_err(dev, "Link frequency not supported: %lld\n",
			ep_cfg.link_frequencies[0]);
		goto error_out;
	}

	ret = 0;

error_out:
	v4l2_fwnode_endpoint_free(&ep_cfg);
	fwnode_handle_put(endpoint);

	return ret;
}


static int imx219_notifier_bound(struct v4l2_async_notifier *notifier,
		struct v4l2_subdev *subdev,
		struct v4l2_async_subdev *asd)
{
    struct imx219_channel *ch     = NULL;
	int         ret     = 0;

    ch = container_of(notifier, struct imx219_channel, notifier);

	logi("v4l2-subdev %s is bounded\n", subdev->name);

	ch->remote_sd = subdev;

	return ret;
}

static int imx219_parse_device_tree_io(struct imx219 *dev, struct device_node *node)
{
    struct device_node  *loc_ep = NULL;
    int         ret = 0;

    if (node == NULL) {
        loge("the device tree is empty\n");
        ret = -ENODEV;
        goto err;
    }
	logi("\n");

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
            logi("output[%d]\n", channel);

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


static void imx219_notifier_unbind(struct v4l2_async_notifier *notifier,
		struct v4l2_subdev *subdev,
		struct v4l2_async_subdev *asd)
{
	struct imx219_channel  *dev        = NULL;

	dev = container_of(notifier, struct imx219_channel, notifier);

	logi("v4l2-subdev %s is unbounded\n", subdev->name);
}

static const struct v4l2_async_notifier_operations imx219_notifier_ops = {
	.bound = imx219_notifier_bound,
	.unbind = imx219_notifier_unbind,
};

static int imx219_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct imx219 *imx219 = NULL;
    struct imx219_channel     *ch = NULL;
    int             idxch   = 0;

	int ret;

	logi("start ===========================\n");

	imx219 = devm_kzalloc(&client->dev, sizeof(*imx219), GFP_KERNEL);
	if (!imx219)
		return -ENOMEM;

    /* init parameters */
    for (idxch = 0; idxch < NUM_CHANNELS; idxch++) {
        ch = &imx219->channel[idxch];

        ch->id = idxch;

        /* Register with V4L2 layer as a slave device */
        v4l2_i2c_subdev_init(&ch->sd, client, &imx219_subdev_ops);
        v4l2_set_subdevdata(&ch->sd, ch);
		imx219->client = client;

        /* init notifier */
        v4l2_async_notifier_init(&ch->notifier);
        ch->notifier.ops = &imx219_notifier_ops;

        logi("%d channel notifier\n", ch->id);
        ch->dev = imx219;
    }

	ret = imx219_parse_device_tree_io(imx219, client->dev.of_node);
    if (ret < 0) {
        loge("imx219_parse_device_tree_io, ret: %d\n", ret);
        goto error_handler_free;
    }
	logi("parse device  tree (%d)", ret);

 	/*
     * add allocated async subdev instance to a notifier.
     * The async subdev is the linked device
     * in front of this device.
     */
    for (idxch = 0; idxch < NUM_CHANNELS; idxch++) {
        ch = &imx219->channel[idxch];

        if (ch->asd.match.fwnode == NULL) {
            /* async subdev to register is not founded */
           loge("ch->asd.match.fwnode == NULL\n");
           continue;
        }

        ret = v4l2_async_notifier_add_subdev(&ch->notifier, &ch->asd);
        if (ret) {
            loge("v4l2_async_notifier_add_subdev, ret: %d\n", ret);
            goto error_handler_free;
        }

        /* register a notifier */
        ret = v4l2_async_subdev_notifier_register(&ch->sd, &ch->notifier);
        if (ret < 0) {
            loge("v4l2_async_subdev_notifier_register, ret: %d\n", ret);
            v4l2_async_notifier_cleanup(&ch->notifier);
            goto error_handler_free;
        }
    }

    /*
     * register subdev instance.
     * The subdev instance is this device.
     */
    for (idxch = 0; idxch < NUM_CHANNELS; idxch++) {
        ch = &imx219->channel[idxch];
        /* register a v4l2-subdev */

        if (ch->sd.fwnode == NULL) {
            /* subdev to register is not founded */
        	loge("ch->sd.fwnode == NULL\n");
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

	/* Check the hardware configuration in device tree */
	if (imx219_check_hwcfg(dev)) {
		dev_err(dev, "failed imx219_check_hwcfg\n");
		goto error_handler_free;
    }

	/* Get system clock (xclk) */
	imx219->xclk = devm_clk_get(dev, NULL);
	if (IS_ERR(imx219->xclk)) {
		dev_err(dev, "failed to get xclk\n");
		return PTR_ERR(imx219->xclk);
	}

	imx219->xclk_freq = clk_get_rate(imx219->xclk);
	if (imx219->xclk_freq != IMX219_XCLK_FREQ) {
		dev_err(dev, "xclk frequency not supported: %d Hz\n",
			imx219->xclk_freq);
		goto error_handler_free;
	}

	ret = imx219_get_regulators(imx219);
	if (ret) {
		dev_err(dev, "failed to get regulators\n");
		goto error_handler_free;
	}

	/* Request optional enable pin */
	imx219->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						     GPIOD_OUT_HIGH);


	if (IS_ERR(imx219->reset_gpio)) {
		dev_err(dev, "failed to get reset_gpio\n");
		goto error_handler_free;
	}

	/*
	 * The sensor must be powered for imx219_identify_module()
	 * to be able to read the CHIP_ID register
	 */
	ret = imx219_power_on(dev);
	if (ret)
		goto error_handler_free;

	ret = imx219_identify_module(imx219);
	if (ret)
		goto error_power_off;

	/* Set default mode to max resolution */
	imx219->mode = &supported_modes[IMX219_DEFAULT_RESOLUTION];

	/* sensor doesn't enter LP-11 state upon power up until and unless
	 * streaming is started, so upon power up switch the modes to:
	 * streaming -> standby
	 */
	ret = imx219_write_reg(imx219, IMX219_REG_MODE_SELECT,
			       IMX219_REG_VALUE_08BIT, IMX219_MODE_STREAMING);
	if (ret < 0)
		goto error_power_off;
	usleep_range(100, 110);

	/* put sensor back to standby mode */
	ret = imx219_write_reg(imx219, IMX219_REG_MODE_SELECT,
			       IMX219_REG_VALUE_08BIT, IMX219_MODE_STANDBY);
	if (ret < 0)
		goto error_power_off;
	usleep_range(100, 110);

	ret = imx219_init_controls(imx219);
	if (ret)
		goto error_power_off;


	/* Initialize subdev */
	ch->sd.internal_ops = &imx219_internal_ops;
	ch->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ch->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pad */
	imx219->pad.flags = MEDIA_PAD_FL_SOURCE;

	/* Initialize default format */
	imx219_set_default_format(imx219);

	ret = media_entity_pads_init(&ch->sd.entity, 1, &imx219->pad);
	if (ret) {
		dev_err(dev, "failed to init entity pads: %d\n", ret);
		goto error_handler_free;
	}

	//imx219_set_stream(&ch->sd, 1);

#if 0
	ret = v4l2_async_register_subdev_sensor_common(&ch->sd);
	if (ret < 0) {
		dev_err(dev, "failed to register sensor sub-device: %d\n", ret);
		//goto error_media_entity;
		return 0;
	}
#endif

	/* Enable runtime PM and turn off the device */
	//pm_runtime_set_active(dev);
	//pm_runtime_enable(dev);
	//pm_runtime_idle(dev);

	logi("Loaded ============== \n");

	return 0;

//error_media_entity:
//	media_entity_cleanup(&imx219->channel[0].sd.entity);

error_handler_free:
	imx219_free_controls(imx219);

error_power_off:
	imx219_power_off(dev);

	return ret;
}

static int imx219_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx219 *imx219 = to_dev(sd);
	logi("\n");
	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	imx219_free_controls(imx219);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		imx219_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

static const struct of_device_id imx219_dt_ids[] = {
	{ .compatible = "sony,imx219" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx219_dt_ids);

static const struct dev_pm_ops imx219_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(imx219_suspend, imx219_resume)
	SET_RUNTIME_PM_OPS(imx219_power_off, imx219_power_on, NULL)
};

static struct i2c_driver imx219_i2c_driver = {
	.driver = {
		.name = "imx219",
		.of_match_table	= imx219_dt_ids,
		.pm = &imx219_pm_ops,
	},
	.probe_new = imx219_probe,
	.remove = imx219_remove,
};

module_i2c_driver(imx219_i2c_driver);

MODULE_AUTHOR("Dave Stevenson <dave.stevenson@raspberrypi.com");
MODULE_DESCRIPTION("Sony IMX219 sensor driver");
MODULE_LICENSE("GPL v2");
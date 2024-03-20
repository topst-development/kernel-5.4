// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - present Telechips.co and/or its affiliates.
 */

#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_panel.h>

#include <video/display_timing.h>
#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include <video/tcc/vioc_lvds.h>

#include <tcc_drm_edid.h>

#include <linux/backlight.h>

#include "panel-tcc.h"

#if defined(CONFIG_DRM_PANEL_MAX968XX)
#include <linux/i2c.h>
#include <linux/regmap.h>

#include "dptx_drm.h"
#include "Dptx_dbg.h"
#include "panel-tcc-dpv14.h"

#if defined(CONFIG_TOUCHSCREEN_INIT_SERDES)
#include <linux/input/tcc_tsc_serdes.h>
#endif

#endif

#define LOG_DPV14_TAG "DRM_DPV14"

#define DRIVER_DATE	"20210810"
#define DRIVER_MAJOR	2
#define DRIVER_MINOR	0
#define DRIVER_PATCH	0


#if defined(CONFIG_DRM_PANEL_MAX968XX)
#define SER_DES_I2C_REG_ADD_LEN			2
#define SER_DES_I2C_DATA_LEN			1

#define DP0_PANEL_SER_I2C_DEV_ADD		0xC0	/* 0xC0 >> 1 = 0x60 */
#define DP0_PANEL_DES_I2C_DEV_ADD		0x90	/* 0x90 >> 1 = 0x48 */
#define DP1_PANEL_DES_I2C_DEV_ADD		0x94	/* 0x94 >> 1 = 0x4A */
#define DP2_PANEL_DES_I2C_DEV_ADD		0x98	/* 0x98 >> 1 = 0x4C */
#define DP3_PANEL_DES_I2C_DEV_ADD		0xD0	/* 0xD0 >> 1 = 0x68 */

#define	SER_DEV_REV						0x000E
#define SER_REV_ES2						0x01
#define SER_REV_ES4						0x03
#define SER_REV_ALL						0x0F

#define	SER_MISC_CONFIG_B1				0x7019
#define MST_FUNCTION_DISABLE		    0x00
#define MST_FUNCTION_ENABLE				0x01

#define	SER_LANE_REMAP_B0				0x7030
#define	SER_LANE_REMAP_B1				0x7031

#define	DES_DEV_REV						0x000E
#define DES_REV_ES2						0x01
#define DES_REV_ES3						0x02
#define DES_STREAM_SELECT				0x00A0
#define DES_DROP_VIDEO					0x0307

#define	DES_VIDEO_RX8					0x0108
#define DES_VID_LOCK					0x40
#define DES_VID_PKT_DET					0x20

#define TCC8059_EVB_TYPE				0
#define TCC8050_EVB_TYPE				1
#define TCC_ALL_EVB_TYPE				0x0F

#define MAX968XX_DELAY_ADDR				0xEFFF
#define MAX968XX_INVALID_REG_ADDR		0xFFFF


enum SERDES_INPUT_INDEX {
	SER_INPUT_INDEX_0		= 0,
	DES_INPUT_INDEX_0		= 1,
	DES_INPUT_INDEX_1		= 2,
	DES_INPUT_INDEX_2		= 3,
	DES_INPUT_INDEX_3		= 4,
	INPUT_INDEX_MAX			= 5
};

enum TCC805X_EVB_REV {
	TCC8059_EVB_01			= 0,
	TCC8050_SV_01			= 1,
	TCC8050_SV_10			= 2,
	TCC805X_EVB_UNKNOWN		= 0xFE
};

enum PHY_INPUT_STREAM_IDX {
	PHY_INPUT_STREAM_0		= 0,
	PHY_INPUT_STREAM_1		= 1,
	PHY_INPUT_STREAM_2		= 2,
	PHY_INPUT_STREAM_3		= 3,
	PHY_INPUT_STREAM_MAX	= 4
};

struct panel_max968xx {
	uint8_t	activated;
	uint8_t	mst_mode;
	uint8_t	ser_laneswap;
	uint8_t	ser_revision;
	uint8_t	des_revision;
	uint8_t	evb_type;
	uint8_t	vcp_id[4];
	struct i2c_client	*client;
};

struct panel_max968xx_reg_data {
	unsigned int	uiDev_Addr;
	unsigned int	uiReg_Addr;
	unsigned int	uiReg_Val;
	unsigned char	ucDeviceType;
	unsigned char	ucSER_Revision;
};

#endif /* #if defined(CONFIG_DRM_PANEL_MAX968XX) */


struct dp_pins {
	struct pinctrl *p;
	struct pinctrl_state *pwr_port;
	struct pinctrl_state *pwr_on;
	struct pinctrl_state *reset_off;
	struct pinctrl_state *blk_on;
	struct pinctrl_state *blk_off;
	struct pinctrl_state *pwr_off;
};

struct panel_dp14 {
	struct drm_panel panel;
	struct device *dev;
	struct videomode video_mode;

#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	struct backlight_device *backlight;
#endif

	const struct dp_match_data *data;
	struct dp_pins st_dp_pins;

	/* Version ---------------------*/
	/** @major: driver major number */
	int major;
	/** @minor: driver minor number */
	int minor;
	/** @patchlevel: driver patch level */
	int patchlevel;
	/** @date: driver date */
	char *date;
};

struct dp_match_data {
	const char *name;
};

static const struct dp_match_data dpv14_panel_0 = {
	.name = "DP PANEL-0",
};

static const struct dp_match_data dpv14_panel_1 = {
	.name = "DP PANEL-1",
};

static const struct dp_match_data dpv14_panel_2 = {
	.name = "DP PANEL-2",
};

static const struct dp_match_data dpv14_panel_3 = {
	.name = "DP PANEL-3",
};

#if defined(CONFIG_DRM_PANEL_MAX968XX)

static struct panel_max968xx	stpanel_max968xx[INPUT_INDEX_MAX];

static struct panel_max968xx_reg_data stPanel_1027_DesES3_RegVal[] = {
	{0xD0, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	10, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* AGC CR Init 8G1 */
	{0xC0, 0x60AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* BST CR Init 8G1 */
	{0xC0, 0x60B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* AGC CR Init 5G4 */
	{0xC0, 0x60A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* BST CR Init 5G4 */
	{0xC0, 0x60B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* AGC CR Init 2G7 */
	{0xC0, 0x60A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* BST CR Init 2G7 */
	{0xC0, 0x60B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
/* Set 8G1 Error Channel Phase */
	{0xC0, 0x6070, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6071, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6170, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6171, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6270, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6271, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6370, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6371, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},

/***** MST Setting *****/
	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A14, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7000, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, SER_MISC_CONFIG_B1, MST_FUNCTION_ENABLE,
	TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x70A0, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7074, 0x1E, TCC_ALL_EVB_TYPE, SER_REV_ES4},
	{0xC0, 0x7074, 0x0A, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x7070, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, SER_LANE_REMAP_B0, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, SER_LANE_REMAP_B1, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	1, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7000, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	50, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7A14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7A14, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x11, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x11, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* VID_LINK_SEL_X, Y, Z, U of SER will be written 01 */
	{0xC0, 0x0100, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0110, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0120, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0130, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x04CF, 0xBF, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x05CF, 0xBF, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x06CF, 0xBF, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x07CF, 0xBF, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/*****************************************/
/* Configure GM03 DP_RX Payload IDs      */
/*****************************************/
/*Sets the MST payload ID of the video stream for video output port 0, 1, 2, 3*/
	{0xC0, 0x7904, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7908, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x790C, 0x03, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7910, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A10, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A00, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
/* DMA mode enable */
	{0xC0, 0x7A18, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A24, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A26, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B10, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B00, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B18, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B24, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B26, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7B14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C10, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C00, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C18, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C24, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C26, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7C14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D10, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D00, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D18, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D24, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D26, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7D14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x6184, 0x0F, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, DES_DROP_VIDEO, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, DES_DROP_VIDEO, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, DES_DROP_VIDEO, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xD0, DES_DROP_VIDEO, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, DES_STREAM_SELECT, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, DES_STREAM_SELECT, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, DES_STREAM_SELECT, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xD0, DES_STREAM_SELECT, 0x03, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x1F, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/********** Des & GPIO & I2C Setting *************/
	{0x90, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* 1st LCD */
	{0x94, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* 2nd LCD */
	{0x98, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},/* 3rd LCD */
	{0xD0, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},/* 4th LCD */

/* LCD Reset 1 : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset #1 */
	{0xC0, 0x0208, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0209, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0233, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0234, 0xB1, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0235, 0x61, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* LCD Reset 2( TCC8059 ) : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset 1 */
	{0x94, 0x0233, 0x84, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0234, 0xB1, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0235, 0x61, TCC8059_EVB_TYPE, SER_REV_ALL},

/* LCD Reset 3( TCC8059 ) : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset 3 */
	{0x98, 0x0233, 0x84, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0234, 0xB1, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0235, 0x61, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x020B, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x020B, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* LCD Reset 2( TCC8050 ) : Ser GPIO #11 RX/TX RX ID 11	--> LCD Reset #2 */
	{0xC0, 0x0258, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0259, 0x0B, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0233, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0234, 0xB1, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0235, 0x6B, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x025B, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x025B, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},

/* LCD Reset 3 : Ser GPIO #15 RX/TX RX ID 15	--> LCD Reset 3 */
	{0xC0, 0x0278, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0279, 0x0F, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0233, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0234, 0xB1, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0235, 0x6F, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x027B, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x027B, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},

/* LCD Reset 4 : Ser GPIO #22 RX/TX RX ID 22	--> LCD Reset #4 */
	{0xC0, 0x02B0, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02B1, 0x16, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0233, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0234, 0xB1, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0235, 0x6F, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02B3, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02B3, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},

/* LCD on : Ser GPIO #24 RX/TX RX ID 24	--> LCD On #1, 2, 3, 4 */
	{0xC0, 0x02C0, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02C1, 0x18, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0236, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0237, 0xB2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0238, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0236, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0237, 0xB2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0238, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0236, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0237, 0xB2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0238, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0236, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0237, 0xB2, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0238, 0x78, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02C3, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02C3, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL}, /* Toggle */

/* Backlight on 1 : Ser GPIO #0 RX/TX RX ID 0 --> Backlight On 1 */
	{0xC0, 0x0200, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0201, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0068, 0x48, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0069, 0x48, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0206, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0207, 0xA2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0208, 0x60, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0048, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0049, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* Backlight on 2( TCC8059 ) : Ser GPIO #0 RX/TX RX ID 0 --> Backlight On 2 */
	{0x94, 0x0206, 0x84, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0207, 0xA2, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0208, 0x60, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0048, 0x08, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0049, 0x08, TCC8059_EVB_TYPE, SER_REV_ALL},

/* Backlight on 3( TCC8059 ) : Ser GPIO #0 RX/TX RX ID 0 --> Backlight On 2 */
	{0x98, 0x0206, 0x84, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0207, 0xA2, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0208, 0x60, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0048, 0x08, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0049, 0x08, TCC8059_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0203, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0203, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},

/* Backlight on 2 : Ser GPIO #0 RX/TX RX ID 5 --> Backlight On 2 */
	{0xC0, 0x0228, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0229, 0x05, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0206, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0207, 0xA2, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0208, 0x65, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0048, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0049, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x022B, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x022B, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},

/* Backlight on 3 : Ser GPIO #0 RX/TX RX ID 14 --> Backlight On 3*/
	{0xC0, 0x0270, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0271, 0x0E, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0206, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0207, 0xA2, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0208, 0x6E, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0048, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0049, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0273, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0273, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},

/* Backlight on 4 : Ser GPIO #0 RX/TX RX ID 21 --> Backlight On 4*/
	{0xC0, 0x02A8, 0x01, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02A9, 0x15, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0206, 0x84, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0207, 0xA2, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0208, 0x75, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x0048, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0049, 0x08, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0273, 0x20, TCC8050_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0273, 0x21, TCC8050_EVB_TYPE, SER_REV_ALL},
/*****************************************/
/*	I2C Setting							 */
/*****************************************/
/* Des1, 2, 3 GPIO #14 I2C Driving */

	{0x90, 0x020C, 0x90, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x020C, 0x90, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x020C, 0x90, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xD0, 0x020C, 0x90, TCC8050_EVB_TYPE, SER_REV_ALL},

	{0x0, 0x0, 0x0, 0, SER_REV_ALL}
};

static struct panel_max968xx_reg_data stPanel_1027_DesES2_RegVal[] = {
	{0x90, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x0308, 0x03, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x03E0, 0x07, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x14A6, 0x0F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x1460, 0x87, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x141F, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x1431, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x141D, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x14E1, 0x22, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x04D4, 0x43, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0423, 0x47, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x04E1, 0x22, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x03E0, 0x07, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x0050, 0x66, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x001A, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0022, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x0029, 0x02, TCC_ALL_EVB_TYPE},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	300, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x6421, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7019, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A14, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x60AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63AA, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63B6, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63A9, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63B5, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63A8, 0x78, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x60B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x61B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x62B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x63B4, 0x20, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6070, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6071, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6170, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6171, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6270, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6271, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6370, 0xA5, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x6371, 0x65, TCC_ALL_EVB_TYPE, SER_REV_ES2},

	{0xC0, 0x70A0, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6064, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6065, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6164, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6165, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6264, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6265, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6364, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6365, 0x06, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7000, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7054, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	1, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7074, 0x0A, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7070, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7030, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7031, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	1, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7000, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	1, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7A18, 0x05, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A28, 0xFF, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A2A, 0xFF, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A24, 0xFF, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A27, 0x0F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7A14, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x11, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x10, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x6420, 0x11, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x0005, 0x70, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x01CE, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x0210, 0x40, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0211, 0x40, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0212, 0x0F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0213, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0220, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0221, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0223, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0208, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0209, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x020B, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02C0, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02C1, 0x18, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x02C3, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0200, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0201, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0203, 0x21, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0068, 0x48, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0069, 0x48, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x022D, 0x43, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x022E, 0x6f, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x022F, 0x6f, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0230, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0231, 0xb0, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0232, 0x44, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0233, 0x8c, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0234, 0xb1, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0235, 0x41, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0236, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0237, 0xb2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0238, 0x58, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0206, 0x84, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0207, 0xA2, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0208, 0x40, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0048, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0049, 0x08, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x009E, 0x00, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0079, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0006, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0071, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0210, 0x60, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0212, 0x4F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0213, 0x02, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x022D, 0x63, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x022E, 0x6F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x022F, 0x2F, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x022A, 0x18, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x020C, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x020C, 0x90, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x020C, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x020C, 0x90, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x0, 0x0, 0x0, 0, SER_REV_ALL}
};
#endif /* #if defined(CONFIG_DRM_PANEL_MAX968XX) */


static inline struct panel_dp14 *to_dp_drv_panel(const struct drm_panel *pdrm_panel)
{
	struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)container_of(pdrm_panel, struct panel_dp14, panel);

	return dp14;
}

static int panel_dpv14_disable(struct drm_panel *pdrm_panel)
{
	const struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)to_dp_drv_panel(pdrm_panel);

	dev_info(dp14->dev, "[INFO][%s:%d]To %s..\r\n", __func__, __LINE__, dp14->data->name);
#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	if (dp14->backlight) {
		dp14->backlight->props.power = FB_BLANK_POWERDOWN;
		dp14->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(dp14->backlight);
	}
#else
	if (panel_tcc_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.blk_off) < 0) {
		/* For Coverity */
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed set pinctrl to blk_off\r\n", __func__, __LINE__, dp14->data->name);
	}
#endif
	if (panel_tcc_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.pwr_off) < 0) {
		/* For Coverity */
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed set pinctrl to blk_off\r\n", __func__, __LINE__, dp14->data->name);
	}

	return 0;
}

static int panel_dpv14_enable(struct drm_panel *pdrm_panel)
{
	const struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)to_dp_drv_panel(pdrm_panel);

	dev_info(dp14->dev, "[INFO][%s:%d]To %s..\r\n", __func__, __LINE__, dp14->data->name);

#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	if (dp14->backlight) {
		dp14->backlight->props.state &= ~BL_CORE_FBBLANK;
		dp14->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(dp14->backlight);
	}
#else
	if (panel_tcc_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.blk_on) < 0) {
		/* For Coverity */
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed set pinctrl to blk_on\r\n", __func__, __LINE__, dp14->data->name);
	}
#endif

	return 0;
}

static int panel_dpv14_unprepare(struct drm_panel *pdrm_panel)
{
	const struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)to_dp_drv_panel(pdrm_panel);

	dev_info(dp14->dev, "[INFO][%s:%d]To %s..\r\n", __func__, __LINE__, dp14->data->name);

	return 0;
}

static int panel_dpv14_prepare(struct drm_panel *pdrm_panel)
{
	const struct panel_dp14 *dp14;

	dp14 = to_dp_drv_panel(pdrm_panel);

	dev_info(dp14->dev, "[INFO][%s:%d]To %s..\r\n", __func__, __LINE__, dp14->data->name);

	if (panel_tcc_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.pwr_on) < 0) {
		/* For Coverity */
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed set pinctrl to pwr_on\r\n", __func__, __LINE__, dp14->data->name);
	}

	if (panel_tcc_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.reset_off) < 0) {
		/* For Coverity */
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed set pinctrl to reset_off\r\n", __func__, __LINE__, dp14->data->name);
	}

	return 0;
}

static int panel_dpv14_get_modes(struct drm_panel *pdrm_panel)
{
	const struct panel_dp14 *dp14;
	struct drm_connector *connector;
	struct drm_display_mode *mode;
	struct edid *pst_edid = NULL;
	int32_t count = 0;
	int32_t ret = 0;

	dp14 = (struct panel_dp14 *)to_dp_drv_panel(pdrm_panel);
	connector = (struct drm_connector *)dp14->panel.connector;

	if (connector == NULL) {
		/* For Coverity */
		goto return_funcs;
	}

	mode = drm_mode_create(dp14->panel.drm);
	if (mode == NULL) {
		/* For Coverity */
		goto return_funcs;
	}

	pst_edid = (struct edid *)kzalloc(EDID_LENGTH, GFP_KERNEL);
	if (pst_edid == NULL) {
		/* For Coverity */
		goto return_funcs;
	}

	drm_display_mode_from_videomode(&dp14->video_mode, mode);

	ret = tcc_make_edid_from_display_mode(pst_edid, mode);
	if (ret != 0) {
		kfree(pst_edid);
		goto return_funcs;
	}

	ret = drm_connector_update_edid_property(connector, pst_edid);
	if (ret != 0) {
		kfree(pst_edid);
		goto return_funcs;
	}

	count = drm_add_edid_modes(connector, pst_edid);

return_funcs:
	return count;
}

static const struct drm_panel_funcs panel_dpv14_funcs = {
	.disable = panel_dpv14_disable,
	.unprepare = panel_dpv14_unprepare,
	.prepare = panel_dpv14_prepare,
	.enable = panel_dpv14_enable,
	.get_modes = panel_dpv14_get_modes,
};

static int panel_pinctrl_parse_dt(struct panel_dp14 *dp14)
{
	struct device_node *dn;
	struct device_node *np;
	int ret = 0;

	dn = dp14->dev->of_node;

	np = of_get_child_by_name(dn, "display-timings");
	if (np != NULL) {
		of_node_put(np);
		ret = of_get_videomode(dn, &dp14->video_mode, OF_USE_NATIVE_MODE);
		if (ret < 0) {
			/* For coverity */
			dev_err(dp14->dev, "[ERROR][%s:%d]%s : failed to get of_get_videomode\r\n", __func__, __LINE__, dp14->data->name);
		}
	} else {
		/* For coverity */
		dev_err(dp14->dev, "[ERROR][%s:%d]%s : failed to display-timings property\r\n", __func__, __LINE__, dp14->data->name);
	}

	dp14->st_dp_pins.p = devm_pinctrl_get(dp14->dev);
	if (IS_ERR(dp14->st_dp_pins.p)) {
		dev_info(dp14->dev, "[INFO][%s:%d]%s : there is no pinctrl\r\n", __func__, __LINE__, dp14->data->name);
		dp14->st_dp_pins.p = NULL;
		goto return_funcs;
	}

	dp14->st_dp_pins.pwr_port = pinctrl_lookup_state(dp14->st_dp_pins.p, "default");
	if (IS_ERR(dp14->st_dp_pins.pwr_port)) {
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed to find default\r\n", __func__, __LINE__, dp14->data->name);
		dp14->st_dp_pins.pwr_port = NULL;
		goto return_funcs;
	}

	dp14->st_dp_pins.pwr_on = pinctrl_lookup_state(dp14->st_dp_pins.p, "power_on");
	if (IS_ERR(dp14->st_dp_pins.pwr_on)) {
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed to find power_on\r\n", __func__, __LINE__, dp14->data->name);
		dp14->st_dp_pins.pwr_on = NULL;
	}

	dp14->st_dp_pins.reset_off = pinctrl_lookup_state(dp14->st_dp_pins.p, "reset_off");
	if (dp14->st_dp_pins.reset_off == NULL) {
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed to find reset_off\r\n", __func__, __LINE__, dp14->data->name);
		dp14->st_dp_pins.reset_off = NULL;
	}

	dp14->st_dp_pins.blk_on = pinctrl_lookup_state(	dp14->st_dp_pins.p, "blk_on");
	if (IS_ERR(dp14->st_dp_pins.blk_on)) {
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed to find blk_on\r\n", __func__, __LINE__, dp14->data->name);
		dp14->st_dp_pins.blk_on = NULL;
	}

	dp14->st_dp_pins.blk_off = pinctrl_lookup_state(dp14->st_dp_pins.p, "blk_off");
	if (IS_ERR(dp14->st_dp_pins.blk_off)) {
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed to find blk_off\r\n", __func__, __LINE__, dp14->data->name);
		dp14->st_dp_pins.blk_off = NULL;
	}

	dp14->st_dp_pins.pwr_off = pinctrl_lookup_state(dp14->st_dp_pins.p, "power_off");
	if (IS_ERR(dp14->st_dp_pins.pwr_off)) {
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed to find power_off\r\n", __func__, __LINE__, dp14->data->name);
		dp14->st_dp_pins.pwr_off = NULL;
	}

#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	np = of_parse_phandle(dp14->dev->of_node, "backlight", 0);
	if (np != NULL) {
		dp14->backlight = of_find_backlight_by_node(np);

		of_node_put(np);

		if (!dp14->backlight) {
			//backlight node is not valid
			dev_err(dp14->dev, "[ERROR][%s:%d]%s : backlight driver not valid\r\n", __func__, __LINE__, dp14->data->name);
		} else {
			/* For coverity */
			dev_info(dp14->dev, "[INFO][%s:%d]%s : max brightness[%d] from External backlight driver\r\n", __func__, __LINE__, dp14->data->name, dp14->backlight->props.max_brightness);
		}
	} else {
		/* For coverity */
		dev_info(dp14->dev, "[INFO][%s:%d]%s : use pinctrl backlight...\r\n", __func__, __LINE__, dp14->data->name);
	}
#else
	dev_info(dp14->dev, "[INFO][%s:%d]%s : use pinctrl backlight\r\n", __func__, __LINE__, dp14->data->name);
#endif

return_funcs:
	return ret;
}

static int panel_pinctrl_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)devm_kzalloc(&pdev->dev, sizeof(*dp14), GFP_KERNEL);
	if (dp14 == NULL) {
		ret = -ENODEV;
		goto return_funcs;
	}

	dp14->dev = &pdev->dev;
	dp14->data = (const struct dp_match_data *)of_device_get_match_data(&pdev->dev);
	if (dp14->data == NULL) {
		dev_err(dp14->dev, "[ERROR][%s:%d]%s : failed to find match_data\r\n", __func__, __LINE__, dp14->data->name);
		ret = -ENODEV;
		goto return_funcs;
	}

	ret = panel_pinctrl_parse_dt(dp14);
	if (ret < 0) {
		dev_err(dp14->dev, "[ERROR][%s:%d]%s : failed to parse device tree\r\n", __func__, __LINE__, dp14->data->name);
		goto return_funcs;
	}

	drm_panel_init(&dp14->panel);

	dp14->panel.dev = dp14->dev;
	dp14->panel.funcs = &panel_dpv14_funcs;

	ret = drm_panel_add(&dp14->panel);
	if (ret < 0) {
		dev_err(dp14->dev, "[ERROR][%s:%d]%s : failed to drm_panel_init\r\n", __func__, __LINE__, dp14->data->name);

#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
		put_device(&dp14->backlight->dev);
#endif
		goto return_funcs;
	}

	dev_set_drvdata(dp14->dev, dp14);

	/* Version */
	dp14->major = DRIVER_MAJOR;
	dp14->minor = DRIVER_MINOR;
	dp14->patchlevel = DRIVER_PATCH;

	dev_info(dp14->dev, "[INFO][%s:%d]%s : %d.%d.%d for %s\r\n", __func__, __LINE__,
				dp14->data->name,
				dp14->major, dp14->minor, dp14->patchlevel,
				(dp14->dev != NULL) ? dev_name(dp14->dev) : "unknown device");

return_funcs:
	return ret;
}

static int panel_pinctrl_remove(struct platform_device *pdev)
{
	int32_t ret = 0;
	struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)dev_get_drvdata(&pdev->dev);

	devm_pinctrl_put(dp14->st_dp_pins.p);

	drm_panel_detach(&dp14->panel);
	drm_panel_remove(&dp14->panel);

	ret = panel_dpv14_disable(&dp14->panel);

#if defined(CONFIG_DRM_TCC_CTRL_BACKLIGHT)
	if (dp14->backlight) {
		/* For Coverity */
		put_device(&dp14->backlight->dev);
	}
#endif

	devm_kfree(&pdev->dev, dp14);

	return ret;
}

#ifdef CONFIG_PM
static int panel_pinctrl_suspend(struct device *dev)
{
	const struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)dev_get_drvdata(dev);

	dev_info(dp14->dev, "[INFO][%s:%s] %s \r\n", LOG_DPV14_TAG, dp14->data->name, __func__);

	return 0;
}

static int panel_pinctrl_resume(struct device *dev)
{
	const struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)dev_get_drvdata(dev);

	dev_info(dp14->dev, "[INFO][%s:%s] %s \r\n", LOG_DPV14_TAG, dp14->data->name, __func__);

	if (panel_tcc_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.pwr_port) < 0) {
		dev_warn(dp14->dev,"[WARN][%s:%s] %s failed set pinctrl to pwr_port\r\n", LOG_DPV14_TAG, dp14->data->name, __func__);
	}
	if (panel_tcc_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.pwr_off) < 0) {
		dev_warn(dp14->dev, "[WARN][%s:%s] %s failed set pinctrl to pwr_off\r\n", LOG_DPV14_TAG, dp14->data->name, __func__);
	}
	if (panel_tcc_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.blk_off) < 0) {
		dev_warn(dp14->dev, "[WARN][%s:%s] %s failed set pinctrl to blk_off\r\n", LOG_DPV14_TAG, dp14->data->name, __func__);
	}

	return 0;
}

static const struct dev_pm_ops panel_pinctrl_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(panel_pinctrl_suspend, panel_pinctrl_resume)
};
#endif


#if defined(CONFIG_DRM_PANEL_MAX968XX)
#if defined(CONFIG_TOUCHSCREEN_INIT_SERDES)
static int panel_touch_max968XX_update_reg(void)
{
	uint8_t num_of_ports;
	int ret = 0;
	const struct panel_max968xx  *pstpanel_max968xx;

	pstpanel_max968xx = &stpanel_max968xx[SER_INPUT_INDEX_0];
	if (pstpanel_max968xx->client == NULL) {
		dptx_err("There is no Serializer connnected ");
		ret = -ENODEV;
	}

	if (ret == 0) {
		ret = panel_max968xx_get_topology(&num_of_ports);
		if (ret == 0) {
			ret = tcc_tsc_serdes_update(pstpanel_max968xx->client,
											NULL,
											num_of_ports,
											(int)pstpanel_max968xx->evb_type,
											(unsigned int)pstpanel_max968xx->ser_revision);
			if (ret < 0) {
				dptx_err("failed to update register for touch");
				ret = -ENODEV;
			}
		} else {
			/* For Coverity */
			ret = -ENODEV;
		}
	}

	return ret;
}
#endif

static int panel_max968xx_i2c_write(struct i2c_client *client, unsigned short usRegAdd, unsigned char ucValue)
{
	uint8_t wbuf[3]	= {0,};
	int32_t rw_len;
	int32_t iLen_To_W;
	int32_t iRet_Val = 0;

	wbuf[0] = (uint8_t)(usRegAdd >> 8);
	wbuf[1] = (uint8_t)(usRegAdd & 0xFFU);
	wbuf[2] = ucValue;

	iLen_To_W = (SER_DES_I2C_REG_ADD_LEN + SER_DES_I2C_DATA_LEN);

	rw_len = i2c_master_send((const struct i2c_client *)client, (const char *)wbuf, iLen_To_W);
	if (rw_len != iLen_To_W) {
		dptx_err("i2c device %s: error to write address 0x%x.. len %d", client->name, client->addr, rw_len);
		iRet_Val = -ENODEV;
	} else {
                /* For coverity */ 
		//dptx_info("Write I2C Dev 0x%x: Reg add 0x%x -> 0x%x", (client->addr << 1), usRegAdd, ucValue);
	}

	return iRet_Val;
}

static uint8_t panel_max968xx_convert_add_to_index(uint8_t ucI2C_DevAdd)
{
	uint8_t elements;
	const struct panel_max968xx *pstpanel_max968xx;

	for (elements = 0; elements < (uint8_t)INPUT_INDEX_MAX; elements++) {
		pstpanel_max968xx = &stpanel_max968xx[elements];
		if ((pstpanel_max968xx->client != NULL) && ((pstpanel_max968xx->client->addr << 1) == ucI2C_DevAdd)) {
			/* For Coverity */
			break;
		}
	}

	return elements;
}

int panel_max968xx_get_topology(uint8_t *num_of_ports)
{
	uint8_t elements;
	int32_t iRetVal = 0;
	const struct panel_max968xx  *pstpanel_max968xx;

	if (num_of_ports == NULL) {
		dptx_err("Err [%s:%d]num_of_ports == NULL", __func__, __LINE__);
		iRetVal = -EINVAL;
	}

	if (iRetVal == 0) {
		for (elements = (uint8_t)DES_INPUT_INDEX_0; elements < (uint8_t)INPUT_INDEX_MAX; elements++) {
			pstpanel_max968xx = &stpanel_max968xx[elements];
			if (pstpanel_max968xx->client == NULL) {
				/* For Coverity */
				break;
			}
		}

		if (elements <= (u8)INPUT_INDEX_MAX) {
			*num_of_ports = (elements - 1U);
		} else {
			*num_of_ports = 0;
			dptx_warn("Invalid num of ports as %d", elements);
		}
	}

	return iRetVal;
}

int panel_max968xx_reset(void)
{
	uint8_t input_index;
	uint8_t ser_revision = 0, des_revision = 0;
	uint8_t dev_add, rw_data, evb_tpye;
	uint32_t elements;
	int32_t iRetVal = 0;
	const struct panel_max968xx *pstpanel_max968xx;
	const struct panel_max968xx_reg_data *pstpanel_reg_data;

	pstpanel_max968xx = &stpanel_max968xx[SER_INPUT_INDEX_0];
	if (pstpanel_max968xx->client == NULL) {
		dptx_err("There is no Serializer connnected ");
		iRetVal = -ENOENT;
	}
	ser_revision = pstpanel_max968xx->ser_revision;

	pstpanel_max968xx = &stpanel_max968xx[DES_INPUT_INDEX_0];
	if (pstpanel_max968xx->client == NULL) {
		dptx_err("There is no 1st Deserializer connnected ");
		iRetVal = -ENOENT;
	}
	des_revision = pstpanel_max968xx->des_revision;

	if (iRetVal == 0) {
		if (des_revision == (uint8_t)DES_REV_ES2) {
			pstpanel_reg_data = stPanel_1027_DesES2_RegVal;
			dptx_info("Updating DES ES2 Tables...Rev Num(%d)<->Ser Rev(%d)", des_revision, ser_revision);
		} else {
			pstpanel_reg_data = stPanel_1027_DesES3_RegVal;
			dptx_info("Updating DES ES3 Tables...Rev Num(%d)<->Ser Rev(%d)", des_revision, ser_revision);
		}

		evb_tpye = pstpanel_max968xx->evb_type;
		if (evb_tpye != (uint8_t)TCC8059_EVB_TYPE) {
			/* For Coverity */
			evb_tpye = (uint8_t)TCC8050_EVB_TYPE;
		}

		for (elements = 0;
				!((pstpanel_reg_data[elements].uiDev_Addr == 0U) && 	(pstpanel_reg_data[elements].uiReg_Addr == 0U) && (pstpanel_reg_data[elements].uiReg_Val == 0U));
				elements++) {
			if (pstpanel_reg_data[elements].uiDev_Addr == (uint32_t)MAX968XX_DELAY_ADDR) {
				mdelay(pstpanel_reg_data[elements].uiReg_Val);
				continue;
			}

			dev_add = (uint8_t)pstpanel_reg_data[elements].uiDev_Addr;

			input_index = panel_max968xx_convert_add_to_index(dev_add);
			if (input_index >= (uint8_t)INPUT_INDEX_MAX) {
				//dptx_dbg("Invalid SerDes Index returned %d as device address 0x%x", input_index, dev_add);
				continue;
			}

			pstpanel_max968xx = &stpanel_max968xx[input_index];
			if (pstpanel_max968xx->client == NULL) {
				/* For Coverity */
				continue;
			}

			if ((pstpanel_reg_data[elements].ucDeviceType != (uint8_t)TCC_ALL_EVB_TYPE) && (evb_tpye != pstpanel_reg_data[elements].ucDeviceType)) {
	//			dptx_info("[%d]EVB %s isn't matched with %d", elements, (evb_tpye == TCC8059_EVB_TYPE) ? "TCC8059":"TCC8059/3", pstpanel_reg_data[elements].ucDeviceType);
				continue;
			}

			if ((pstpanel_reg_data[elements].ucSER_Revision != (uint8_t)SER_REV_ALL) && (ser_revision != pstpanel_reg_data[elements].ucSER_Revision)) {
	//			dptx_info("[%d]Ser Revision %d isn't matched with %d", elements, ser_revision, pstpanel_reg_data[elements].ucSER_Revision);
				continue;
			}

			if ((pstpanel_max968xx->mst_mode == 0U) &&
				((pstpanel_reg_data[elements].uiReg_Addr == (uint32_t)DES_DROP_VIDEO) || (pstpanel_reg_data[elements].uiReg_Addr == (uint32_t)DES_STREAM_SELECT))) {
				/* For Coverity */
				continue;
			}

			if ((pstpanel_max968xx->ser_laneswap == 0U) &&
				((pstpanel_reg_data[elements].uiReg_Addr == (uint32_t)SER_LANE_REMAP_B0) || (pstpanel_reg_data[elements].uiReg_Addr == (uint32_t)SER_LANE_REMAP_B1))) {
				/* For Coverity */
				continue;
			}

			rw_data = (uint8_t)pstpanel_reg_data[elements].uiReg_Val;

			if ((pstpanel_max968xx->mst_mode == 0U) && (pstpanel_reg_data[elements].uiReg_Addr == (uint32_t)SER_MISC_CONFIG_B1)) {
				/* For Coverity */
				rw_data = MST_FUNCTION_DISABLE;
			}

			if (pstpanel_reg_data[elements].uiReg_Addr == (uint32_t)DES_STREAM_SELECT) {
				switch (input_index) {
				case (uint8_t)DES_INPUT_INDEX_0:
					rw_data = pstpanel_max968xx->vcp_id[0];
					break;
				case (uint8_t)DES_INPUT_INDEX_1:
					rw_data = pstpanel_max968xx->vcp_id[1];
					break;
				case (uint8_t)DES_INPUT_INDEX_2:
					rw_data = pstpanel_max968xx->vcp_id[2];
					break;
				case (uint8_t)DES_INPUT_INDEX_3:
					rw_data = pstpanel_max968xx->vcp_id[3];
					break;
				default:
					rw_data = 1;
					dptx_err("Invalid VCP Index as %d", input_index);
					break;
				}

				rw_data = (rw_data - 1U);

				dptx_info("Set VCP id %d to device 0x%x", rw_data, pstpanel_reg_data[elements].uiReg_Addr);
			}

			iRetVal = panel_max968xx_i2c_write(pstpanel_max968xx->client, (uint16_t)pstpanel_reg_data[elements].uiReg_Addr, rw_data);
			if (iRetVal != 0) {
				/* For Coverity */
				continue;
			}
		}

		dptx_info("\nSerDes %d Resisters update is successfully done!!!\n", elements);
	}
#if defined(CONFIG_TOUCHSCREEN_INIT_SERDES)
	iRetVal = panel_touch_max968XX_update_reg();
	if (iRetVal < 0) {
		dptx_err("failed to update Ser/Des Register for Touch");
	}
#endif

	return 0;
}

static int panel_max968xx_parse_dt(struct panel_max968xx *pstpanel_max968xx)
{
	uint32_t evb_type, ser_laneswap;
	uint32_t vcp_id;
	int32_t iRetVal = 0;
	const struct device_node *dn;

	dn = of_find_compatible_node(NULL, NULL, "telechips,max968xx_configuration");
	if (dn == NULL) {
		/* For Coverity */
		dptx_warn("Can't find SerDes node\n");
	}

	iRetVal = of_property_read_u32(dn, "max968xx_evb_type", &evb_type);
	if (iRetVal < 0) {
		dptx_warn("Can't get EVB type.. set to TCC8050_SV_10(default)");
		evb_type = (u32)TCC8050_SV_10;
	}

	iRetVal = of_property_read_u32(dn, "max96851_lane_02_13_swap", &ser_laneswap);
	if (iRetVal < 0) {
		dptx_warn("Can't max96851 lane swap option.. set to 1(default)");
		ser_laneswap = (u32)1;
	}

	pstpanel_max968xx->evb_type = (uint8_t)(evb_type & 0xFFU);
	pstpanel_max968xx->ser_laneswap = (ser_laneswap == 0U) ? (uint8_t)0U:(uint8_t)1U;

	dn = of_find_compatible_node(NULL, NULL, "telechips,dpv14-tx");
	if (dn == NULL) {
		/* For Coverity */
		dptx_warn("Can't find dpv14-tx node\n");
	}

	iRetVal = of_property_read_u32(dn, "vcp_id_1st_sink", &vcp_id);
	if (iRetVal < 0) {
		dptx_err("Can't get 1st Sink VCP Id.. set to 1(default)");
		vcp_id = (u32)1;
	}
	pstpanel_max968xx->vcp_id[PHY_INPUT_STREAM_0] = (uint8_t)(vcp_id & 0xFFU);

	iRetVal = of_property_read_u32(dn, "vcp_id_2nd_sink", &vcp_id);
	if (iRetVal < 0) {
		dptx_err("Can't get 2nd Sink VCP Id.. set to 2(default)");
		vcp_id = (u32)2;
	}
	pstpanel_max968xx->vcp_id[PHY_INPUT_STREAM_1] = (uint8_t)(vcp_id & 0xFFU);

	iRetVal = of_property_read_u32(dn, "vcp_id_3rd_sink", &vcp_id);
	if (iRetVal < 0) {
		dptx_err("Can't get 3rd Sink VCP Id.. set to 3(default)");
		vcp_id = (u32)3;
	}
	pstpanel_max968xx->vcp_id[PHY_INPUT_STREAM_2] = (uint8_t)(vcp_id & 0xFFU);

	iRetVal = of_property_read_u32(dn, "vcp_id_4th_sink", &vcp_id);
	if (iRetVal < 0) {
		dptx_err("Can't get 4th Sink VCP Id.. set to 4(default)");
		vcp_id = (u32)4;
	}
	pstpanel_max968xx->vcp_id[PHY_INPUT_STREAM_3] = (uint8_t)(vcp_id & 0xFFU);

	return iRetVal;
}

static int panel_max968xx_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	uint8_t addr_buf[2] = {0,};
	uint8_t data_buf, elements, num_of_ports;
	int32_t rw_len;
	int32_t iRetVal = 0, iRet = 0;
	struct panel_max968xx *pstpanel_max968xx;

	for (elements = 0; elements < (uint8_t)INPUT_INDEX_MAX; elements++) {
		pstpanel_max968xx = &stpanel_max968xx[elements];

		if (pstpanel_max968xx->activated == 0U) {
			pstpanel_max968xx->activated = (uint8_t)1U;
			break;
		}
	}
	if (elements == (uint8_t)INPUT_INDEX_MAX) {
		dptx_err("It's limited to 4 SerDes ports connected.. Dev add 0x%x", client->addr << 1);
		iRetVal = -ENOMEM;
	}

	if (iRetVal == 0) {
		addr_buf[0] = (uint8_t)((uint16_t)SER_DEV_REV >> 8);
		addr_buf[1] = (uint8_t)((uint16_t)SER_DEV_REV & 0xFFU);

		rw_len = i2c_master_send((const struct i2c_client *)client, (const char *)addr_buf, (int)SER_DES_I2C_REG_ADD_LEN);
		if (rw_len != (int32_t)SER_DES_I2C_REG_ADD_LEN) {
			dptx_info("Dev add 0x%x isn't connected", client->addr << 1);
			iRetVal = -ENXIO;
		}
	}

	if (iRetVal == 0) {
		rw_len = i2c_master_recv((const struct i2c_client *)client, (char *)&data_buf, 1);
		if (rw_len != 1) {
			dptx_info("Dev add 0x%x isn't connected", client->addr << 1);
			iRetVal = -ENXIO;
		}

		if (iRetVal == 0) {
			pstpanel_max968xx->client = client;

			if (elements == (uint8_t)SER_INPUT_INDEX_0) {
			/* For Coverity */
				pstpanel_max968xx->ser_revision = data_buf;
			} else {
				/* For Coverity */
				pstpanel_max968xx->des_revision = data_buf;
			}
		}
	}

	if (iRetVal == 0) {
		/* For Coverity */
		pstpanel_max968xx->client = client;
	}

	if (elements == (uint8_t)DES_INPUT_INDEX_3) {
		iRet = panel_max968xx_get_topology(&num_of_ports);
		if (iRet == 0) {
			pstpanel_max968xx->mst_mode = (num_of_ports > 1U) ? 1U:0U;

			iRet = dpv14_set_num_of_panels(num_of_ports);
			if (iRet != 0) {
				/* For Coverity */
				dptx_err("From dpv14_set_num_of_panels()");
			}
		} else {
			/* For Coverity */
			dptx_err("From panel_max968xx_get_topology()");
		}

#if defined(CONFIG_TOUCHSCREEN_INIT_SERDES)
		iRet = panel_touch_max968XX_update_reg();
		if (iRet < 0) {
			/* For Coverity */
			dptx_err("failed to update Ser/Des Register for Touch");
		}
#endif
	}

	if (iRetVal == 0) {
		i2c_set_clientdata(client, pstpanel_max968xx);

		iRetVal = panel_max968xx_parse_dt(pstpanel_max968xx);

		if (iRetVal == 0) {
			dptx_info("[%d]%s: %s I2C name : %s I2C Dev name : %s, address 0x%x : Rev %d..SerDes Lane %s",
								elements,
								(pstpanel_max968xx->evb_type == (uint8_t)TCC8059_EVB_01) ? "TCC8059 EVB" :
								(pstpanel_max968xx->evb_type == (uint8_t)TCC8050_SV_01) ? "TCC8050/3 sv0.1" :
								(pstpanel_max968xx->evb_type == (uint8_t)TCC8050_SV_10) ? "TCC8050/3 sv1.0":"Unknown",
								((client->addr << 1) == (uint16_t)DP0_PANEL_SER_I2C_DEV_ADD) ? "Ser":"Des",
								client->name,
								id->name,
								((client->addr) << 1),
								data_buf,
								(pstpanel_max968xx->ser_laneswap == 0U) ? "isn't swapped":"is swapped");
		}

		if (pstpanel_max968xx->evb_type != (uint8_t)TCC8059_EVB_01) {
			/* For Coverity */
			pstpanel_max968xx->evb_type = (uint8_t)TCC8050_SV_01;
		}
	}

	return iRetVal;
}

static int panel_max968xx_i2c_remove(struct i2c_client *client)
{
	const void *pvRet;
	uint8_t ucMax_Input;
	int32_t ret = 0;

	ucMax_Input = (uint8_t)INPUT_INDEX_MAX;
	
	pvRet = memset(&stpanel_max968xx[SER_INPUT_INDEX_0], 0, (sizeof(struct panel_max968xx) * ucMax_Input));
	if (pvRet == NULL) {
		dptx_err("Can't memset to 0 in Dev memory.. dev add(0x%x)", (client->addr) << 1);
		ret = -ENOMEM;
	}

	return ret;
}
#endif /* #if defined(CONFIG_DRM_PANEL_MAX968XX) */



static const struct of_device_id panel_pinctrl_of_table[] = {
	{ .compatible = "telechips,drm-lvds-dpv14-0",
	  .data = &dpv14_panel_0,
	},
	{ .compatible = "telechips,drm-lvds-dpv14-1",
	  .data = &dpv14_panel_1,
	},
	{ .compatible = "telechips,drm-lvds-dpv14-2",
	  .data = &dpv14_panel_2,
	},
	{ .compatible = "telechips,drm-lvds-dpv14-3",
	  .data = &dpv14_panel_3,
	},
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(of, panel_pinctrl_of_table);

static struct platform_driver pintctrl_driver = {
	.probe		= panel_pinctrl_probe,
	.remove		= panel_pinctrl_remove,
	.driver		= {
		.name	= "panel-dpv14",
#ifdef CONFIG_PM
		.pm	= &panel_pinctrl_pm_ops,
#endif
		.of_match_table = panel_pinctrl_of_table,
	},
};


#if defined(CONFIG_DRM_PANEL_MAX968XX)
static const struct of_device_id max968xx_match[] = {
	{.compatible = "maxim,serdes"},
	{},
};
MODULE_DEVICE_TABLE(of, max968xx_match);


static const struct i2c_device_id max968xx_id[] = {
	{ "Max968XX", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, max968xx_id);


static struct i2c_driver max968xx_i2c_drv = {
	.probe = panel_max968xx_i2c_probe,
	.remove = panel_max968xx_i2c_remove,
	.id_table = max968xx_id,
	.driver = {
			.name = "telechips,Max96851_78",
			.owner = THIS_MODULE,

#if defined(CONFIG_OF)
			.of_match_table = of_match_ptr(max968xx_match),
#endif
		},
};

static int __init panel_dpv14_init(void)
{
	const void *pvRet;
	uint8_t ucMax_Input;
	int ret;

	ucMax_Input = (uint8_t)INPUT_INDEX_MAX;

	pvRet = memset(&stpanel_max968xx[SER_INPUT_INDEX_0], 0, (sizeof(struct panel_max968xx) * ucMax_Input));
	if (pvRet == NULL) {
		dptx_err("Can't memset to 0 in Dev memory");
		ret = -ENOMEM;
	}

	ret = i2c_add_driver(&max968xx_i2c_drv);
	if (ret != 0) {
		/*For Coverity*/
		dptx_err("Max96851_78 I2C registration failed %d\n", ret);
	} else {
		/*For Coverity*/
		ret = platform_driver_register(&pintctrl_driver);
	}

	return ret;
}
module_init(panel_dpv14_init);

static void __exit panel_dpv14_exit(void)
{
	i2c_del_driver(&max968xx_i2c_drv);

	return platform_driver_unregister(&pintctrl_driver);
}
module_exit(panel_dpv14_exit);

#else

module_platform_driver(pintctrl_driver);

#endif /* #if defined(CONFIG_DRM_PANEL_MAX968XX) */

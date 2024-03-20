// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
/*
* Copyright (C) Telechips Inc.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>

#include "Dptx_v14.h"
#include "Dptx_drm_dp_addition.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"

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

#define DP_SER_DES_DELAY_DEV_ADDR			0xEFFF
#define DP_SER_DES_INVALID_REG_ADDR			0xFFFF


struct Max968xx_dev {
	bool				bActivated;
	bool				bDes_VidLocked;
	bool				bSer_LaneSwap;
	unsigned char		ucSER_Revision_Num;
	unsigned char		ucDES_Revision_Num;
	unsigned char		ucEVB_Type;
	struct i2c_client	*pstClient;
};

struct DP_V14_SER_DES_Reg_Data {
	unsigned int					uiDev_Addr;
	unsigned int					uiReg_Addr;
	unsigned int					uiReg_Val;
	unsigned char					ucDeviceType;
	unsigned char					ucSER_Revision;
};


static struct Max968xx_dev	stMax968xx_dev[SER_DES_INPUT_INDEX_MAX];

static struct DP_V14_SER_DES_Reg_Data pstDP_Panel_VIC_1027_DesES3_RegVal[] = {
	{0xD0, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x98, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x94, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x90, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
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

	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x70A0, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7074, 0x1E, TCC_ALL_EVB_TYPE, SER_REV_ES4},
	{0xC0, 0x7074, 0x0A, TCC_ALL_EVB_TYPE, SER_REV_ES2},
	{0xC0, 0x7070, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, SER_LANE_REMAP_B0, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, SER_LANE_REMAP_B1, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	1, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7000, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
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
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
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

	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
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

static struct DP_V14_SER_DES_Reg_Data pstDP_Panel_VIC_1027_DesES2_RegVal[] = {
	{0x90, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x0010, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
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
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
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
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	1, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7074, 0x0A, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7070, 0x04, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7030, 0x4E, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0xC0, 0x7031, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	1, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0xC0, 0x7000, 0x01, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
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
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x020C, 0x90, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x020C, 0x80, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	100, TCC_ALL_EVB_TYPE, SER_REV_ALL},

	{0x90, 0x020C, 0x90, TCC_ALL_EVB_TYPE, SER_REV_ALL},
	{0x0, 0x0, 0x0, 0, SER_REV_ALL}
};

static int of_parse_serdes_dt(struct Max968xx_dev *pstDev)
{
	int iRetVal;
	u32 uiEVB_Type, uiLane_Swap;
	const struct device_node *pstSerDes_DN;

	pstSerDes_DN = of_find_compatible_node(NULL, NULL, "telechips,max968xx_configuration");
	if (pstSerDes_DN == NULL) {
		/* For Coverity */
		dptx_warn("Can't find SerDes node\n");
	}

	iRetVal = of_property_read_u32(pstSerDes_DN, "max968xx_evb_type", &uiEVB_Type);
	if (iRetVal < 0) {
		dptx_warn("Can't get EVB type.. set to 'TCC8050_SV_01' by default");
		uiEVB_Type = (uint32_t)TCC8050_SV_01;
	}

	iRetVal = of_property_read_u32(pstSerDes_DN, "max96851_lane_02_13_swap", &uiLane_Swap);
	if (iRetVal < 0) {
		dptx_warn("Can't max96851 lane swap option.. set to 'Enable by default");
		uiLane_Swap = (uint32_t)1U;
	}

	pstDev->ucEVB_Type = (uint8_t)(uiEVB_Type & 0xFFU);
	pstDev->bSer_LaneSwap = (uiLane_Swap == 0U) ? false : true;

	return DPTX_RETURN_NO_ERROR;
}

static int Dptx_Max968XX_I2C_Write(struct i2c_client *client, unsigned short usRegAdd, unsigned char ucValue)
{
	unsigned char aucWBuf[3]	= {0,};
	int32_t iRW_Len, iLen_To_W;
	int32_t iRet_Val = DPTX_RETURN_NO_ERROR;

	aucWBuf[0] = (uint8_t)(usRegAdd >> 8);
	aucWBuf[1] = (uint8_t)(usRegAdd & 0xFFU);
	aucWBuf[2] = ucValue;

	iLen_To_W = (SER_DES_I2C_REG_ADD_LEN + SER_DES_I2C_DATA_LEN);

	iRW_Len = i2c_master_send((const struct i2c_client *)client, (const char *)aucWBuf, (int)iLen_To_W);
	if (iRW_Len != iLen_To_W) {
		dptx_err("i2c device %s: error to write address 0x%x.. len %d", client->name, client->addr, iRW_Len);
		iRet_Val = DPTX_RETURN_ENODEV;
	} else {
		dptx_dbg("Write I2C Dev 0x%x: Reg add 0x%x -> 0x%x", (client->addr << 1), usRegAdd, ucValue);
	}

	return iRet_Val;
}

static unsigned char Dptx_Max968XX_Convert_DevAdd_To_Index(unsigned char ucI2C_DevAdd)
{
	u8 ucElements;
	const struct Max968xx_dev *pstMax968xx_dev;

	for (ucElements = 0; ucElements < (uint8_t)SER_DES_INPUT_INDEX_MAX; ucElements++) {
		pstMax968xx_dev = &stMax968xx_dev[ucElements];
		if ((pstMax968xx_dev->pstClient != NULL) && ((pstMax968xx_dev->pstClient->addr << 1) == ucI2C_DevAdd)) {
			/* For Coverity */
			break;
		}
	}

	return ucElements;
}

int32_t Dptx_Max968XX_Get_TopologyState(u8 *pucNumOfPluggedPorts)
{
	uint8_t ucElements;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	const struct Max968xx_dev *pstMax968xx_dev;

	if (pucNumOfPluggedPorts == NULL) {
		dptx_info("pucNumOfPluggedPorts == NULL");
		iRetVal = DPTX_RETURN_EINVAL;
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		for (ucElements = (uint8_t)DES_INPUT_INDEX_0; ucElements < (uint8_t)SER_DES_INPUT_INDEX_MAX; ucElements++) {
			pstMax968xx_dev = &stMax968xx_dev[ucElements];
			if (pstMax968xx_dev->pstClient == NULL) {
				/* For Coverity */
				break;
			}
		}

		if (ucElements <= (u8)SER_DES_INPUT_INDEX_MAX) {
			*pucNumOfPluggedPorts = (uint8_t)(ucElements - 1U);
		} else {
			*pucNumOfPluggedPorts = 0;

			dptx_info("Invalid num of ports as %d", (ucElements - 1U));
		}
	}

	return iRetVal;
}

/* B190164 - Update SerDes Register for Touch Init */
int32_t Touch_Max968XX_update_reg(const struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	const struct Max968xx_dev *pstMax968xx_dev;

#if defined(CONFIG_TOUCHSCREEN_INIT_SERDES)
	int ret;
#endif

	pstMax968xx_dev = &stMax968xx_dev[SER_INPUT_INDEX_0];
	if (pstMax968xx_dev->pstClient == NULL) {
		dptx_err("There is no Serializer connnected ");
		iRetVal = DPTX_RETURN_EPERM;
	}

#if defined(CONFIG_TOUCHSCREEN_INIT_SERDES)
	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		ret = tcc_tsc_serdes_update(	pstMax968xx_dev->pstClient,
										NULL,
										pstDptx->ucNumOfPorts,
										(int32_t)pstMax968xx_dev->ucEVB_Type,
										pstMax968xx_dev->ucSER_Revision_Num);
		if (ret < 0) {
			dptx_err("failed to update register for touch");
			iRetVal = DPTX_RETURN_EPERM;
		}
	}
#endif

	return iRetVal;
}

int32_t Dptx_Max968XX_Reset(const struct Dptx_Params *pstDptx)
{
	uint8_t ucSerDes_Index;
	uint8_t ucSER_Revision_Num = 0, ucDES_Revision_Num = 0;
	uint8_t ucRW_Data, ucEVB_Tpye;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiElements;
	const struct Max968xx_dev *pstMax968xx_dev;
	const struct DP_V14_SER_DES_Reg_Data *pstSERDES_Reg_Info;

	pstMax968xx_dev = &stMax968xx_dev[SER_INPUT_INDEX_0];
	if (pstMax968xx_dev->pstClient == NULL) {
		dptx_err("There is no Serializer connnected ");
		iRetVal = DPTX_RETURN_ENOENT;
	}
	ucSER_Revision_Num = pstMax968xx_dev->ucSER_Revision_Num;

	pstMax968xx_dev = &stMax968xx_dev[DES_INPUT_INDEX_0];
	if (pstMax968xx_dev->pstClient == NULL) {
		dptx_err("There is no 1st Deserializer connnected ");
		iRetVal = DPTX_RETURN_ENOENT;
	}
	ucDES_Revision_Num = pstMax968xx_dev->ucDES_Revision_Num;

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		if (ucDES_Revision_Num == (uint8_t)DES_REV_ES2) {
			pstSERDES_Reg_Info = pstDP_Panel_VIC_1027_DesES2_RegVal;
			dptx_info("Updating DES ES2 Tables...Rev Num(%d)<->Ser Rev(%d)",
						ucDES_Revision_Num,
						ucSER_Revision_Num);
		} else {
			pstSERDES_Reg_Info = pstDP_Panel_VIC_1027_DesES3_RegVal;
			dptx_info("Updating DES ES3 Tables...Rev Num(%d)<->Ser Rev(%d)",
						ucDES_Revision_Num,
						ucSER_Revision_Num);
		}

		ucEVB_Tpye = pstMax968xx_dev->ucEVB_Type;
		if (ucEVB_Tpye != (uint8_t)TCC8059_EVB_01) {
			/* For Coverity */
			ucEVB_Tpye = (uint8_t)TCC8050_SV_01;
		}

		for (uiElements = 0;
			!((pstSERDES_Reg_Info[uiElements].uiDev_Addr == 0U) &&
				(pstSERDES_Reg_Info[uiElements].uiReg_Addr == 0U) &&
				(pstSERDES_Reg_Info[uiElements].uiReg_Val == 0U));
				uiElements++) {
			if (pstSERDES_Reg_Info[uiElements].uiDev_Addr == (uint32_t)DP_SER_DES_DELAY_DEV_ADDR) {
				mdelay(pstSERDES_Reg_Info[uiElements].uiReg_Val);
				continue;
			}

			ucSerDes_Index = Dptx_Max968XX_Convert_DevAdd_To_Index((uint8_t)pstSERDES_Reg_Info[uiElements].uiDev_Addr);
			if (ucSerDes_Index >= (uint8_t)SER_DES_INPUT_INDEX_MAX) {
				dptx_dbg("Invalid SerDes Index returned %d as device address 0x%x",
							ucSerDes_Index,
							pstSERDES_Reg_Info[uiElements].uiDev_Addr);
				continue;
			}

			pstMax968xx_dev = &stMax968xx_dev[ucSerDes_Index];
			if (pstMax968xx_dev->pstClient == NULL) {
				/* For Coverity */
				continue;
			}

			if ((pstSERDES_Reg_Info[uiElements].ucDeviceType != (uint8_t)TCC_ALL_EVB_TYPE) &&
				(ucEVB_Tpye != pstSERDES_Reg_Info[uiElements].ucDeviceType)) {
				dptx_dbg("[%d]EVB Type %d isn't matched with %d",
							uiElements,
							ucEVB_Tpye,
							pstSERDES_Reg_Info[uiElements].ucDeviceType);
				continue;
			}

			if ((pstSERDES_Reg_Info[uiElements].ucSER_Revision != (uint8_t)SER_REV_ALL) &&
				(ucSER_Revision_Num != pstSERDES_Reg_Info[uiElements].ucSER_Revision)) {
				dptx_dbg("[%d]Ser Revision %d isn't matched with %d",
							uiElements,
							ucSER_Revision_Num,
							pstSERDES_Reg_Info[uiElements].ucSER_Revision);
				continue;
			}

			if ((pstDptx->bMultStreamTransport == false) &&
				((pstSERDES_Reg_Info[uiElements].uiReg_Addr == (uint32_t)DES_DROP_VIDEO) ||
				(pstSERDES_Reg_Info[uiElements].uiReg_Addr == (uint32_t)DES_STREAM_SELECT))) {
				/* For Coverity */
				continue;
			}

			if ((pstMax968xx_dev->bSer_LaneSwap == false) &&
				((pstSERDES_Reg_Info[uiElements].uiReg_Addr == (uint32_t)SER_LANE_REMAP_B0) ||
				(pstSERDES_Reg_Info[uiElements].uiReg_Addr == (uint32_t)SER_LANE_REMAP_B1))) {
				/* For Coverity */
				continue;
			}

			ucRW_Data = (uint8_t)pstSERDES_Reg_Info[uiElements].uiReg_Val;

			if ((pstDptx->bMultStreamTransport == false) &&
				(pstSERDES_Reg_Info[uiElements].uiReg_Addr == (uint32_t)SER_MISC_CONFIG_B1)) {
				dptx_dbg("Set to SST...");
				ucRW_Data = MST_FUNCTION_DISABLE;
			}

			if (pstSERDES_Reg_Info[uiElements].uiReg_Addr == (uint32_t)DES_STREAM_SELECT) {
				switch (ucSerDes_Index) {
				case (uint8_t)DES_INPUT_INDEX_0:
					ucRW_Data = pstDptx->aucVCP_Id[0];
					break;
				case (uint8_t)DES_INPUT_INDEX_1:
					ucRW_Data = pstDptx->aucVCP_Id[1];
					break;
				case (uint8_t)DES_INPUT_INDEX_2:
					ucRW_Data = pstDptx->aucVCP_Id[2];
					break;
				case (uint8_t)DES_INPUT_INDEX_3:
					ucRW_Data = pstDptx->aucVCP_Id[3];
					break;
				default:
					dptx_err("Invalid VCP Index as %d", ucSerDes_Index);
					break;
				}

				dptx_notice("Set VCP id %d to device 0x%x",
							ucRW_Data,
							pstSERDES_Reg_Info[uiElements].uiDev_Addr);
			}

			iRetVal = Dptx_Max968XX_I2C_Write(pstMax968xx_dev->pstClient,
												(uint16_t)pstSERDES_Reg_Info[uiElements].uiReg_Addr,
												ucRW_Data);
			if (iRetVal != DPTX_RETURN_NO_ERROR) {
				/* For Coverity */
				continue;
			}
		}

		dptx_info("\nSerDes %d Resisters update is successfully done!!!\n", uiElements);
	}

	iRetVal = Touch_Max968XX_update_reg(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* For Coverity */
		dptx_err("failed to update Ser/Des Register for Touch");
	}

	return iRetVal;

}

static int32_t Dptx_Max968XX_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	uint8_t aucAddr_buf[2] = {0,};
	uint8_t ucData_buf, ucElements;
	int32_t iRW_Len;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	struct Max968xx_dev *pstMax968xx_dev;

	for (ucElements = 0; ucElements < (uint8_t)SER_DES_INPUT_INDEX_MAX; ucElements++) {
		pstMax968xx_dev = &stMax968xx_dev[ucElements];

		if (!pstMax968xx_dev->bActivated) {
			pstMax968xx_dev->bActivated = (bool)true;
			break;
		}
	}

	if (ucElements == (uint8_t)SER_DES_INPUT_INDEX_MAX) {
		dptx_err("It's limited to 4 SerDes ports connected.. Dev add 0x%x", client->addr << 1);
		iRetVal = -ENOMEM;
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		aucAddr_buf[0] = (uint8_t)((uint16_t)SER_DEV_REV >> 8);
		aucAddr_buf[1] = (uint8_t)((uint16_t)SER_DEV_REV & 0xFFU);

		iRW_Len = i2c_master_send( (const struct i2c_client *)client, (const char *)aucAddr_buf, (int)SER_DES_I2C_REG_ADD_LEN);
		if (iRW_Len != (int32_t)SER_DES_I2C_REG_ADD_LEN) {
			dptx_info("DP %d isn't connected->Dev name '%s', Dev add 0x%x",
						ucElements,
						client->name,
						client->addr);
			iRetVal = -ENXIO;
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		iRW_Len = i2c_master_recv((const struct i2c_client *)client, (char *)&ucData_buf, 1);
		if (iRW_Len != 1) {
			dptx_info("DP %d isn't connected->Dev name '%s', Dev add 0x%x",
						ucElements,
						client->name,
						client->addr);
			iRetVal = -ENXIO;
		}

		if (ucElements == (uint8_t)SER_INPUT_INDEX_0) {
			/* For Coverity */
			pstMax968xx_dev->ucSER_Revision_Num = ucData_buf;
		} else {
			/* For Coverity */
			pstMax968xx_dev->ucDES_Revision_Num = ucData_buf;
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		pstMax968xx_dev->pstClient = client;

		i2c_set_clientdata(client, pstMax968xx_dev);

		iRetVal = of_parse_serdes_dt(pstMax968xx_dev);
		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			dptx_info("[%d]%s: %s I2C name : %s I2C Dev name : %s, address 0x%x : Rev %d..SerDes Lane %s",
						ucElements,
						(pstMax968xx_dev->ucEVB_Type == (uint8_t)TCC8059_EVB_01) ? "TCC8059 EVB" :
						(pstMax968xx_dev->ucEVB_Type == (uint8_t)TCC8050_SV_01) ? "TCC8050/3 sv0.1" :
						(pstMax968xx_dev->ucEVB_Type == (uint8_t)TCC8050_SV_10) ? "TCC8050/3 sv1.0" : "Unknown",
						((client->addr << 1) == (uint16_t)DP0_PANEL_SER_I2C_DEV_ADD) ? "Ser":"Des",
						client->name,
						id->name,
						((client->addr) << 1),
						ucData_buf,
						(pstMax968xx_dev->bSer_LaneSwap) ? "is swapped":"is not swapped");
		}

		if (pstMax968xx_dev->ucEVB_Type != (uint8_t)TCC8059_EVB_01) {
			/* For Coverity */
			pstMax968xx_dev->ucEVB_Type = (uint8_t)TCC8050_SV_01;
		}
	}

	return iRetVal;
}

static int Dptx_Max968XX_remove(struct i2c_client *client)
{
	const void *pRetVal;

	pRetVal = memset(&stMax968xx_dev[SER_INPUT_INDEX_0],
						0,
						(sizeof(struct Max968xx_dev) * (uint16_t)SER_DES_INPUT_INDEX_MAX));
	if (pRetVal == NULL) {
		/* For Coverity */
		dptx_err("memset failed\n");
	}

	dptx_notice("[%s:%d]%s of add 0x%x is removed",__func__, __LINE__, client->name, client->addr);

	return DPTX_RETURN_NO_ERROR;
}

static const struct of_device_id max_96851_78_match[] = {
	{.compatible = "maxim,serdes"},
	{},
};
MODULE_DEVICE_TABLE(of, max_96851_78_match);


static const struct i2c_device_id max_96851_78_id[] = {
	{ "Max968XX", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, max_96851_78_id);


static struct i2c_driver stMax96851_78_drv = {
	.probe = Dptx_Max968XX_probe,
	.remove = Dptx_Max968XX_remove,
	.id_table = max_96851_78_id,
	.driver = {
			.name = "telechips,Max96851_78",
			.owner = THIS_MODULE,

#if defined(CONFIG_OF)
			.of_match_table = of_match_ptr(max_96851_78_match),
#endif
		},
};

static int __init Max968XX_Drv_init(void)
{
	int iRetVal = DPTX_RETURN_NO_ERROR;

	const void *pRetVal;

	pRetVal = memset(&stMax968xx_dev[SER_INPUT_INDEX_0],
						0,
						(sizeof(struct Max968xx_dev) * (uint16_t)SER_DES_INPUT_INDEX_MAX));
	if (pRetVal == NULL) {
		/* For Coverity */
		dptx_err("memset failed\n");
	}

	iRetVal = i2c_add_driver(&stMax96851_78_drv);
	if (iRetVal != 0) {
		dptx_err("Max96851_78 I2C registration failed");
	}

	return iRetVal;
}
module_init(Max968XX_Drv_init);

static void __exit Max968XX_Drv_exit(void)
{
	i2c_del_driver(&stMax96851_78_drv);
}
module_exit(Max968XX_Drv_exit);


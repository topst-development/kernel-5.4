// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <linux/of_graph.h>
#include <video/tcc/vioc_ddicfg.h>

#include "tcc-mipi-csi2-reg.h"
#include "tcc-mipi-cfg-reg.h"
#include "tcc-mipi-ckc-reg.h"
#include "tcc-mipi-csi2.h"

#define LOG_TAG				TCC_MIPI_CSI2_DRIVER_NAME

#define loge(dev_ptr, fmt, ...) \
	dev_err(dev_ptr, "[ERROR][%s] %s - " \
		fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(dev_ptr, fmt, ...) \
	dev_warn(dev_ptr, "[WARN][%s] %s - " \
		fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(dev_ptr, fmt, ...) \
	dev_info(dev_ptr, "[INFO][%s] %s - " fmt, \
		LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(dev_ptr, fmt, ...) \
	dev_dbg(dev_ptr, "[DEBUG][%s] %s - " \
		fmt, LOG_TAG, __func__, ##__VA_ARGS__)

//#define DEFAULT_WIDTH			(1280U)
//#define DEFAULT_HEIGHT			(720U)

#define DEFAULT_WIDTH			(1920U)
#define DEFAULT_HEIGHT			(1080U)
#define	MAX_CHANNEL			(4U)

#define DEFAULT_CSIS_FREQ		(300000000UL)

struct isp_state {
	struct v4l2_mbus_framefmt fmt;

	unsigned int data_format;

	/* output */
	unsigned int pixel_mode;
};

struct tcc_mipi_csi2_channel {
	unsigned int			id;
	struct v4l2_subdev		sd;

	struct v4l2_async_subdev	asd;
	struct v4l2_async_notifier	notifier;
	struct v4l2_subdev		*remote_sd;

	struct tcc_mipi_csi2_state	*state;
};

struct tcc_mipi_csi2_state {
	struct platform_device *pdev;
	struct v4l2_dv_timings dv_timings;

	struct tcc_mipi_csi2_channel	channel[MAX_CHANNEL];

	void __iomem *csi_base;
	void __iomem *ckc_base;
	void __iomem *cfg_base;
	void __iomem *ddicfg_base;

	int irq;

	struct clk *pixel_clock;
	u32 clk_frequency;

	unsigned int mipi_chmux[CSI_CFG_MIPI_CHMUX_MAX];
	unsigned int isp_bypass[CSI_CFG_ISP_BYPASS_MAX];

	/* CSIS common control parameter */
	unsigned int deskew_level;
	unsigned int deskew_enable;
	unsigned int interleave_mode;
	unsigned int data_lane_num;
	unsigned int update_shadow_ctrl;

	/* DPHY common control parameter */
	unsigned int hssettle;
	unsigned int s_clksettlectl;
	unsigned int s_dpdn_swap_clk;
	unsigned int s_dpdn_swap_dat;

	/* ISP configuration parameter */
	unsigned int input_ch_num;
	unsigned int pixel_mode;

	struct isp_state isp_info[MAX_VC];

	struct mutex lock;
	unsigned int use_cnt;
};

static u32 code_to_csi_dt(u32 mbus_code)
{
	u32 dt = 0U;

	switch (mbus_code) {
	case MEDIA_BUS_FMT_UYVY8_2X8:
	case MEDIA_BUS_FMT_VYUY8_2X8:
	case MEDIA_BUS_FMT_YUYV8_2X8:
	case MEDIA_BUS_FMT_YVYU8_2X8:
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_VYUY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_YVYU8_1X16:
		dt = DATA_FORMAT_YUV422_8BIT;
		break;
	case MEDIA_BUS_FMT_RGB565_1X16:
		dt = DATA_FORMAT_RGB565;
		break;
	case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
		dt = DATA_FORMAT_RGB666;
		break;
	case MEDIA_BUS_FMT_RBG888_1X24:
	case MEDIA_BUS_FMT_BGR888_1X24:
	case MEDIA_BUS_FMT_GBR888_1X24:
	case MEDIA_BUS_FMT_RGB888_1X24:
		dt = DATA_FORMAT_RGB888;
		break;
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_Y8_1X8:
		dt = DATA_FORMAT_RAW8;
		break;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_Y10_1X10:
		dt = DATA_FORMAT_RAW10;
		break;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_Y12_1X12:
		dt = DATA_FORMAT_RAW12;
		break;
	default:
		dt = DATA_FORMAT_YUV422_8BIT;
	}

	return dt;
}

static u32 tcc_mipi_csi2_codes[] = {
	MEDIA_BUS_FMT_UYVY8_2X8,
};

struct v4l2_mbus_config mipi_csi2_mbus_config = {
	.type			= V4L2_MBUS_PARALLEL,
	/* de: high, vs: high, hs: high, pclk: high */
	.flags			=
		V4L2_MBUS_DATA_ACTIVE_HIGH	|
#if defined(CONFIG_ARCH_TCC803X)
		V4L2_MBUS_VSYNC_ACTIVE_HIGH	|
#else
		V4L2_MBUS_VSYNC_ACTIVE_LOW	|
#endif
		V4L2_MBUS_HSYNC_ACTIVE_HIGH	|
		V4L2_MBUS_PCLK_SAMPLE_RISING	|
		V4L2_MBUS_MASTER,
};

/*
 * Helper functions for reflection
 */
static inline struct tcc_mipi_csi2_channel *to_channel(struct v4l2_subdev *sd)
{
	return v4l2_get_subdevdata(sd);
}

static inline struct tcc_mipi_csi2_state *to_state(struct v4l2_subdev *sd)
{
	struct tcc_mipi_csi2_channel	*ch	= to_channel(sd);

	return ch->state;
}
static void MIPI_CSIS_Set_DPHY_B_Control(const struct tcc_mipi_csi2_state *state,
		unsigned int high_part,
		unsigned int low_part)
{
	unsigned int val_l = low_part;
	unsigned int val_h = high_part;
	void __iomem *reg_l = state->csi_base + DPHY_BCTRL_L;
	void __iomem *reg_h = state->csi_base + DPHY_BCTRL_H;

	__raw_writel(val_l, reg_l);
	__raw_writel(val_h, reg_h);
}

static void MIPI_CSIS_Set_DPHY_S_Control(const struct tcc_mipi_csi2_state *state,
		unsigned int high_part,
		unsigned int low_part)
{
	unsigned int val_l = low_part;
	unsigned int val_h = high_part;
	void __iomem *reg_l = state->csi_base + DPHY_SCTRL_L;
	void __iomem *reg_h = state->csi_base + DPHY_SCTRL_H;

	__raw_writel(val_l, reg_l);
	__raw_writel(val_h, reg_h);
}

static void MIPI_CSIS_Set_DPHY_Common_Control(const struct tcc_mipi_csi2_state *state)
{
	unsigned int val = 0U;
	unsigned int data_lane = 0U;
	void __iomem *reg = state->csi_base + DPHY_CMN_CTRL;

	switch (state->data_lane_num) {
	case 4U:
		data_lane = (DCCTRL_ENABLE_DATA_LANE_3 |
			     DCCTRL_ENABLE_DATA_LANE_2 |
			     DCCTRL_ENABLE_DATA_LANE_1 |
			     DCCTRL_ENABLE_DATA_LANE_0);
		break;
	case 3U:
		data_lane = (DCCTRL_ENABLE_DATA_LANE_2 |
			     DCCTRL_ENABLE_DATA_LANE_1 |
			     DCCTRL_ENABLE_DATA_LANE_0);
		break;
	case 2U:
		data_lane = (DCCTRL_ENABLE_DATA_LANE_1 |
			     DCCTRL_ENABLE_DATA_LANE_0);
		break;
	case 1U:
		data_lane = DCCTRL_ENABLE_DATA_LANE_0;
		break;
	default:
		data_lane = DCCTRL_ENABLE_DATA_LANE_0;
		break;
	}

	val = __raw_readl(reg);

	val &= ~(DCCTRL_HSSETTLE_MASK |
		DCCTRL_S_CLKSETTLECTL_MASK |
		DCCTRL_S_BYTE_CLK_ENABLE_MASK |
		DCCTRL_S_DPDN_SWAP_CLK_MASK |
		DCCTRL_S_DPDN_SWAP_DAT_MASK |
		DCCTRL_ENABLE_DAT_MASK |
		DCCTRL_ENABLE_CLK_MASK);

	val |= ((state->hssettle << DCCTRL_HSSETTLE_SHIFT) |
		(state->s_clksettlectl << DCCTRL_S_CLKSETTLECTL_SHIFT) |
		(1U << DCCTRL_S_BYTE_CLK_ENABLE_SHIFT) |
		(state->s_dpdn_swap_clk << DCCTRL_S_DPDN_SWAP_CLK_SHIFT) |
		(state->s_dpdn_swap_dat << DCCTRL_S_DPDN_SWAP_DAT_SHIFT) |
		(data_lane << DCCTRL_ENABLE_DAT_SHIFT) |
		(1U << DCCTRL_ENABLE_CLK_SHIFT));

	__raw_writel(val, reg);
}

static inline void MIPI_CSIS_Set_DPHY(const struct tcc_mipi_csi2_state *state)
{
	/*
	 * Set D-PHY control(Master, Slave)
	 * Refer to 7.2.3(B_DPHYCTL) in D-PHY datasheet
	 * 500 means ULPS EXIT counter value.
	 */
	MIPI_CSIS_Set_DPHY_B_Control(state, 0x00000000U, 0x000001f4U);

	/*
	 * Set D-PHY control(Slave)
	 * Refer to 7.2.5(S_DPHYCTL) in D-PHY datasheet
	 */
	MIPI_CSIS_Set_DPHY_S_Control(state, 0x92U, 0xfd008000U);

	/*
	 * Set D-PHY Common control
	 */
	logi(&(state->pdev->dev), "hssettle value : 0x%x\n", state->hssettle);

	MIPI_CSIS_Set_DPHY_Common_Control(state);
}

static void MIPI_CSIS_Set_ISP_Configuration(const struct tcc_mipi_csi2_state *state,
		unsigned int ch)
{
	unsigned int val = 0U;
	void __iomem *reg = state->csi_base + ISP_CONFIG_CH0;
	unsigned int reg_offset = ISP_CONFIG_CH1 - ISP_CONFIG_CH0;

	if (ch >= MAX_VC) {
		loge(&(state->pdev->dev), "fail - invalid ch(%d)\n", ch);
	} else {
		val = __raw_readl(reg);

		val &= ~(ICON_PIXEL_MODE_MASK |
			 ICON_PARALLEL_MASK |
			 ICON_RGB_SWAP_MASK |
			 ICON_DATAFORMAT_MASK |
			 ICON_VIRTUAL_CHANNEL_MASK);

		val |= ((state->isp_info[ch].pixel_mode << ICON_PIXEL_MODE_SHIFT) |
			/* Do not align 32bit data */
			(0U << ICON_PARALLEL_SHIFT) |
			/* MSB is R and LSB is B */
			(0U << ICON_RGB_SWAP_SHIFT) |
			(state->isp_info[ch].data_format << ICON_DATAFORMAT_SHIFT) |
			(ch << ICON_VIRTUAL_CHANNEL_SHIFT));

		__raw_writel(val, (reg + (reg_offset * ch)));
	}
}

static void MIPI_CSIS_Set_ISP_Resolution(const struct tcc_mipi_csi2_state *state,
		unsigned int ch,
		unsigned int width,
		unsigned int height)
{
	unsigned int val = 0U;
	void __iomem *reg = state->csi_base + ISP_RESOL_CH0;
	unsigned int reg_offset = ISP_RESOL_CH1 - ISP_RESOL_CH0;

	val = __raw_readl(reg);

	val &= ~(IRES_VRESOL_MASK | IRES_HRESOL_MASK);

	val |= (width << IRES_HRESOL_SHIFT | height << IRES_VRESOL_SHIFT);

	__raw_writel(val, (reg + (reg_offset * ch)));
}

static void MIPI_CSIS_Set_CSIS_CMM_CTRL(const struct tcc_mipi_csi2_state *state)
{
	unsigned int val = 0U;
	unsigned int lane_number = 0U;
	void __iomem *reg = state->csi_base + CSIS_CMN_CTRL;

	/* for subtraction (-1)*/
	if (state->data_lane_num == 0U) {
		/* default value */
		lane_number = 1U;
	} else {
		/* actual data lane number */
		lane_number = state->data_lane_num;
	}

	val = __raw_readl(reg);

	val &= ~(CCTRL_UPDATE_SHADOW_MASK |
		CCTRL_DESKEW_LEVEL_MASK |
		CCTRL_DESKEW_ENABLE_MASK |
		CCTRL_INTERLEAVE_MODE_MASK |
		CCTRL_LANE_NUMBER_MASK |
		CCTRL_UPDATE_SHADOW_CTRL_MASK |
		CCTRL_SW_RESET_MASK |
		CCTRL_CSI_EN_MASK);

	val |= ((CCTRL_UPDATE_SHADOW_MASK) |
		(state->deskew_level << CCTRL_DESKEW_LEVEL_SHIFT) |
		(state->deskew_enable << CCTRL_DESKEW_ENABLE_SHIFT) |
		(state->interleave_mode << CCTRL_INTERLEAVE_MODE_SHIFT) |
		((lane_number - 1U) << CCTRL_LANE_NUMBER_SHIFT) |
		(state->update_shadow_ctrl << CCTRL_UPDATE_SHADOW_CTRL_SHIFT) |
		(0U << CCTRL_SW_RESET_SHIFT) |
		(1U << CCTRL_CSI_EN_SHIFT));

	__raw_writel(val, reg);
}

static void MIPI_CSIS_Set_CSIS_Reset(const struct tcc_mipi_csi2_state *state,
		unsigned int reset)
{
	unsigned int val = 0U, count = 0U;
	void __iomem *reg = state->csi_base + CSIS_CMN_CTRL;

	val = __raw_readl(reg);

	val &= ~(CCTRL_SW_RESET_MASK);

	if (reset)
		val |= (1U << CCTRL_SW_RESET_SHIFT);

	__raw_writel(val, reg);

	while (__raw_readl(reg) & CCTRL_SW_RESET_MASK) {
		if (count > 50U) {
			loge(&(state->pdev->dev), "fail - MIPI_CSI2 reset\n");
			break;
		}
		mdelay(1U);
		count++;
	}
}

static void MIPI_CSIS_Set_Enable(const struct tcc_mipi_csi2_state *state,
		unsigned int enable)
{
	unsigned int val = 0U;
	void __iomem *reg = state->csi_base + CSIS_CMN_CTRL;

	val = __raw_readl(reg);

	val &= ~(CCTRL_CSI_EN_MASK);

	if (enable)
		val |= (1U << CCTRL_CSI_EN_SHIFT);

	__raw_writel(val, reg);
}

static void MIPI_CSIS_Set_CSIS_Clock_Control(
		const struct tcc_mipi_csi2_state *state,
		unsigned int Clkgate_trail,
		unsigned int Clkgate_en)
{
	unsigned int val = 0U;
	void __iomem *reg = state->csi_base + CSIS_CLK_CTRL;

	val = __raw_readl(reg);

	val &= ~(CCTRL_CLKGATE_TRAIL_MASK |
		CCTRL_CLKGATE_EN_MASK);

	val |= (Clkgate_trail << CCTRL_CLKGATE_TRAIL_SHIFT |
		Clkgate_en << CCTRL_CLKGATE_EN_SHIFT);

	__raw_writel(val, reg);
}

static inline void MIPI_CSIS_Set_CSI(const struct tcc_mipi_csi2_state *state)
{
	unsigned int idx = 0U;

	for (idx = 0U; idx < state->input_ch_num ; idx++) {
		MIPI_CSIS_Set_ISP_Configuration(state, idx);
		MIPI_CSIS_Set_ISP_Resolution(state,
				idx,
				state->isp_info[idx].fmt.width,
				state->isp_info[idx].fmt.height);
	}

	MIPI_CSIS_Set_CSIS_Clock_Control(state, 0x0U, 0xfU);
	MIPI_CSIS_Set_CSIS_CMM_CTRL(state);
}

static void MIPI_CSIS_Set_CSIS_Interrupt_Mask(
		const struct tcc_mipi_csi2_state *state,
		unsigned int page,
		unsigned int mask,
		unsigned int set)
{
	unsigned int val = 0U;
	void __iomem *reg = 0; //state->csi_base + CSIS_INT_MSK0;

	if (page == 0U)
		reg = state->csi_base + CSIS_INT_MSK0;
	else
		reg = state->csi_base + CSIS_INT_MSK1;

	/*
	 * set 0 : interrupt enable(unmask)
	 * set 1 : interrput disable(mask)
	 */

	val = (__raw_readl(reg) & ~(mask));

	if (set == 0U) /* Interrupt enable*/
		val |= mask;

	__raw_writel(val, reg);
}

static unsigned int MIPI_CSIS_Get_CSIS_Interrupt_Mask(
		const struct tcc_mipi_csi2_state *state,
		unsigned int page)
{
	unsigned int val = 0U;
	void __iomem *reg = 0; //state->csi_base + CSIS_INT_MSK0;

	if (page == 0U)
		reg = state->csi_base + CSIS_INT_MSK0;
	else
		reg = state->csi_base + CSIS_INT_MSK1;

	/*
	 * set 0 : interrupt enable(unmask)
	 * set 1 : interrput disable(mask)
	 */

	val = __raw_readl(reg);

	return val;
}

static void MIPI_CSIS_Set_CSIS_Interrupt_Src(
		const struct tcc_mipi_csi2_state *state,
		unsigned int page, unsigned int mask)
{
	unsigned int val = 0U;
	void __iomem *reg = 0; //state->csi_base + CSIS_INT_MSK0;

	if (page == 0U)
		reg = state->csi_base + CSIS_INT_SRC0;
	else
		reg = state->csi_base + CSIS_INT_SRC1;

	val = __raw_readl(reg);

	val |= mask;

	__raw_writel(val, reg);
}

static unsigned int MIPI_CSIS_Get_CSIS_Interrupt_Src(
		const struct tcc_mipi_csi2_state *state,
		unsigned int page)
{
	unsigned int val = 0U;
	void __iomem *reg = 0; //state->csi_base + CSIS_INT_MSK0;

	if (page == 0U)
		reg = state->csi_base + CSIS_INT_SRC0;
	else
		reg = state->csi_base + CSIS_INT_SRC1;

	/*
	 * set 0 : interrupt enable(unmask)
	 * set 1 : interrput disable(mask)
	 */

	val = __raw_readl(reg);

	return val;
}

#if defined(CONFIG_ARCH_TCC805X)
static void MIPI_WRAP_Set_PLL_DIV(
		const struct tcc_mipi_csi2_state *state,
		unsigned int onOff, unsigned int pdiv)
{
	unsigned int val = 0U, target = 0U;
	void __iomem *reg = 0;

	reg = state->ckc_base + CLKDIVC;
	target = (((onOff) << CLKDIVC_PE_SHIFT) |
		  ((pdiv) << CLKDIVC_PDIV_SHIFT));

	val = __raw_readl(reg);

	if (val != target) {
		val &= ~(CLKDIVC_PE_MASK | CLKDIVC_PDIV_MASK);
		val |= target;

		__raw_writel(val, reg);
	} else {
		/* pll divisor is already set */
		logi(&(state->pdev->dev), "skip setting divisor\n");
	}
}

static int MIPI_WRAP_Set_PLL_PMS(
		const struct tcc_mipi_csi2_state *state,
		unsigned int p, unsigned int m, unsigned int s)
{
	unsigned int val = 0U, target;
	void __iomem *reg = 0;
	int retval = 0;

	reg = state->ckc_base + PLLPMS;
	target = (((1U) << PLLPMS_RESETB_SHIFT) |
		  (PLLPMS_LOCK_MASK) |
		  ((p) << PLLPMS_P_SHIFT) |
		  ((m) << PLLPMS_M_SHIFT) |
		  ((s) << PLLPMS_S_SHIFT));
	val = __raw_readl(reg);

	if ((val & target) != target) {
		val &= ~(PLLPMS_S_MASK | PLLPMS_M_MASK | PLLPMS_P_MASK);

		val |= (((p) << PLLPMS_P_SHIFT) |
			((m) << PLLPMS_M_SHIFT) |
			((s) << PLLPMS_S_SHIFT));

		__raw_writel(val, reg);

		usleep_range(1000U, 2000U);

		val |= ((1U) << PLLPMS_RESETB_SHIFT);

		__raw_writel(val, reg);

		usleep_range(1000U, 2000U);

		val = __raw_readl(reg);

		if (val & PLLPMS_LOCK_MASK)
			retval = 0;
		else
			retval = -EBUSY;
	} else {
		/* PMS is already set */
		logi(&(state->pdev->dev), "skip setting PMS\n");
		retval = 0;
	}

	return retval;
}

static void MIPI_WRAP_Set_CLKSRC(const struct tcc_mipi_csi2_state *state,
	unsigned int src, unsigned int out)
{
	unsigned int val = 0, target = 0;
	void __iomem *reg = 0;

	reg = state->ckc_base + out;
	target = (src << CLKCTRL_SEL_SHIFT);

	val = __raw_readl(reg);

	if ((val & target) != target) {
		val &= ~(CLKCTRL_CHGRQ_MASK | CLKCTRL_SEL_MASK);
		val |= (src << CLKCTRL_SEL_SHIFT);
		__raw_writel(val, reg);
	} else {
		/* CLKSRC selection is already set */
		logi(&(state->pdev->dev), "skip setting %s SRC\n",
		    (out == MIPI_BUS_CLK) ? "MIPI_BUS_CLK" : "MIPI_PIXEL_CLK");
	}
}

static int MIPI_WRAP_Set_CKC(const struct tcc_mipi_csi2_state *state)
{
	int ret = 0;
	unsigned int pll_div = 5, pll_p = 2, pll_m = 200, pll_s = 2;
	unsigned int sel_pclk = CLKCTRL_SEL_PLL_DIRECT;
	unsigned int sel_bclk = CLKCTRL_SEL_PLL_DIVIDER;
	const char * const clksrc_sel[] = {
		"XIN", "PLL_DIRECT_OUTPUT", "PLL_DIVIDER_OUTPUT"};

	/*
	 * XIN is 24Mhz
	 * set pixel clock 600MHz
	 * set bus clock 100MHz
	 */
	logi(&(state->pdev->dev), "DIV(%d), PMS(%d %d %d)\n",
		pll_div, pll_p, pll_m, pll_s);
	MIPI_WRAP_Set_PLL_DIV(state, ON, pll_div);
	ret = MIPI_WRAP_Set_PLL_PMS(state, pll_p, pll_m, pll_s);
	if (ret < 0) {
		loge(&(state->pdev->dev), "FAIL - MIPI WRAP PLL SETTING\n");
	} else {
		/*
		 * set source clock to PLL
		 */
		logi(&(state->pdev->dev), "BUSCLK SRC(%s), PIXELCLK SRC(%s)\n",
				clksrc_sel[sel_bclk], clksrc_sel[sel_pclk]);
		MIPI_WRAP_Set_CLKSRC(state, sel_bclk, MIPI_BUS_CLK);
		MIPI_WRAP_Set_CLKSRC(state, sel_pclk, MIPI_PIXEL_CLK);
	}

	return ret;
}

static void MIPI_WRAP_Set_Reset_DPHY(const struct tcc_mipi_csi2_state *state,
		unsigned int reset)
{
	unsigned int val;
	void __iomem *reg = 0;

	reg = state->cfg_base + CSI0_CFG + (state->pdev->id * 0x4);

	val = (__raw_readl(reg) & ~(CSI_CFG_S_RESETN_MASK));

	// 0x1 : release, 0x0 : reset
	if (!reset)
		val |= (0x1U << CSI_CFG_S_RESETN_SHIFT);
	__raw_writel(val, reg);
}

static void MIPI_WRAP_Set_Reset_GEN(const struct tcc_mipi_csi2_state *state,
		unsigned int reset)
{
	unsigned int val;
	void __iomem *reg = 0;

	reg = state->cfg_base + CSI0_CFG + (state->pdev->id * 0x4);

	val = (__raw_readl(reg) &
		~(CSI_CFG_GEN_PX_RST_MASK | CSI_CFG_GEN_APB_RST_MASK));
	if (reset) {
		val |= ((0x1U << CSI_CFG_GEN_PX_RST_SHIFT) |
			(0x1U << CSI_CFG_GEN_APB_RST_SHIFT));
	}

	__raw_writel(val, reg);
}

static void MIPI_WRAP_Set_VSync_Polarity(const struct tcc_mipi_csi2_state *state,
	unsigned int ch,
	unsigned int pol)
{
	unsigned int val;
	void __iomem *reg = 0;

	if (ch >= MAX_VC) {
		loge(&(state->pdev->dev),
			"ch(%d) is not valid\n", ch);
	} else {
		reg = state->cfg_base + CSI0_CFG + (state->pdev->id * 0x4);

		val = (__raw_readl(reg) & ~(CSI_CFG_VSYNC_INV0_MASK << ch));

		val |= (pol << (CSI_CFG_VSYNC_INV0_SHIFT + ch));

		__raw_writel(val, reg);
	}
}

static void MIPI_WRAP_Set_Output_Mux(const struct tcc_mipi_csi2_state *state,
		unsigned int mux, unsigned int sel)
{
	unsigned int val, csi = 0U;
	void __iomem *reg = 0;

	csi = ((mux > 3U) ? (1U) : (0U));
	mux %= 4U;

	reg = state->cfg_base + CSI0_CFG + (csi * 0x4);

	val = (__raw_readl(reg) & ~(CSI_CFG_MIPI_CHMUX_0_MASK << mux));
	val |= (sel << (CSI_CFG_MIPI_CHMUX_0_SHIFT + mux));
	__raw_writel(val, reg);
}

static void MIPI_WRAP_Set_ISP_BYPASS(const struct tcc_mipi_csi2_state *state,
	unsigned int ch, unsigned int bypass)
{
	unsigned int val;
	void __iomem *reg = 0;

	reg = state->cfg_base + ISP_BYPASS;

	val = (__raw_readl(reg) & ~(ISP_BYPASS_ISP0_BYP_MASK << ch));
	val |= (bypass << (ISP_BYPASS_ISP0_BYP_SHIFT + ch));
	__raw_writel(val, reg);

}

static inline void MIPI_WRAP_Set_path(
			const struct tcc_mipi_csi2_state *state)
{
	unsigned int idx = 0U;

	for (idx = 0U; idx < CSI_CFG_MIPI_CHMUX_MAX; idx++) {
		MIPI_WRAP_Set_Output_Mux(state,
			idx, state->mipi_chmux[idx]);
	}

	for (idx = 0U; idx < MAX_VC; idx++) {
		/* set vsync polarity */
		MIPI_WRAP_Set_VSync_Polarity(state, idx, 1U);
	}

	for (idx = 0U; idx < CSI_CFG_ISP_BYPASS_MAX; idx++) {
		MIPI_WRAP_Set_ISP_BYPASS(state,
			idx, state->isp_bypass[idx]);
	}

}

#endif

static void tcc_mipi_csi2_set_interface(const struct tcc_mipi_csi2_state *state,
		unsigned int onOff)
{
	if (onOff) {
#if IS_ENABLED(CONFIG_ARCH_TCC803X)
		// S/W reset D-PHY
		VIOC_DDICONFIG_MIPI_Reset_DPHY(state->ddicfg_base, 0U);
		// S/W reset Generic buffer interface
		VIOC_DDICONFIG_MIPI_Reset_GEN(state->ddicfg_base, 0U);
#elif defined CONFIG_ARCH_TCC805X
		MIPI_WRAP_Set_Reset_DPHY(state, OFF);
		MIPI_WRAP_Set_Reset_GEN(state, OFF);
		MIPI_WRAP_Set_path(state);
#endif
		// S/W reset CSI2
		MIPI_CSIS_Set_CSIS_Reset(state, OFF);

		MIPI_CSIS_Set_DPHY(state);
		MIPI_CSIS_Set_CSI(state);
	} else {
		MIPI_CSIS_Set_Enable(state, OFF);

		// S/W reset CSI2
		MIPI_CSIS_Set_CSIS_Reset(state, ON);

#if IS_ENABLED(CONFIG_ARCH_TCC803X)
		// S/W reset D-PHY
		VIOC_DDICONFIG_MIPI_Reset_DPHY(state->ddicfg_base, 1U);
		// S/W reset Generic buffer interface
		VIOC_DDICONFIG_MIPI_Reset_GEN(state->ddicfg_base, 1U);
#else
		MIPI_WRAP_Set_Reset_DPHY(state, ON);
		MIPI_WRAP_Set_Reset_GEN(state, ON);
#endif
	}
}

static int tcc_mipi_csi2_set_interrupt(const struct tcc_mipi_csi2_state *state,
		unsigned int onOff)
{
	if (onOff) {
		/*
		 * clear interrupt
		 */
		MIPI_CSIS_Set_CSIS_Interrupt_Src(state, 0U,
			CIM_MSK_FrameStart_MASK | CIM_MSK_FrameEnd_MASK |
			CIM_MSK_ERR_SOT_HS_MASK | CIM_MSK_ERR_LOST_FS_MASK |
			CIM_MSK_ERR_LOST_FE_MASK | CIM_MSK_ERR_OVER_MASK |
			CIM_MSK_ERR_WRONG_CFG_MASK | CIM_MSK_ERR_ECC_MASK |
			CIM_MSK_ERR_CRC_MASK | CIM_MSK_ERR_ID_MASK);

		MIPI_CSIS_Set_CSIS_Interrupt_Src(state, 1U,
			CIM_MSK_LINE_END_MASK);

		/*
		 * unmask interrupt
		 */
		MIPI_CSIS_Set_CSIS_Interrupt_Mask(state, 0U,
			CIM_MSK_ERR_SOT_HS_MASK | CIM_MSK_ERR_LOST_FS_MASK |
			CIM_MSK_ERR_LOST_FE_MASK | CIM_MSK_ERR_OVER_MASK |
			CIM_MSK_ERR_WRONG_CFG_MASK | CIM_MSK_ERR_ECC_MASK |
			CIM_MSK_ERR_CRC_MASK | CIM_MSK_ERR_ID_MASK,
			0U);
		MIPI_CSIS_Set_CSIS_Interrupt_Mask(state, 1U,
			CIM_MSK_LINE_END_MASK,
			0U);
	} else {
		/*
		 * mask interrupt
		 */
		MIPI_CSIS_Set_CSIS_Interrupt_Mask(state, 0U,
			CIM_MSK_ERR_SOT_HS_MASK | CIM_MSK_ERR_LOST_FS_MASK |
			CIM_MSK_ERR_LOST_FE_MASK | CIM_MSK_ERR_OVER_MASK |
			CIM_MSK_ERR_WRONG_CFG_MASK | CIM_MSK_ERR_ECC_MASK |
			CIM_MSK_ERR_CRC_MASK | CIM_MSK_ERR_ID_MASK,
			1U);

		MIPI_CSIS_Set_CSIS_Interrupt_Mask(state, 1U,
			CIM_MSK_LINE_END_MASK,
			1U);
	}

	return 0;
}

static irqreturn_t tcc_mipi_csi2_irq_handler(int irq, void *client_data)
{
	struct tcc_mipi_csi2_state *state =
		(struct tcc_mipi_csi2_state *) client_data;
	unsigned int intr_status0 = 0U, intr_status1 = 0U,
		     intr_mask0 = 0U, intr_mask1 = 0U;
	unsigned int idx = 0U;

	intr_status0 = MIPI_CSIS_Get_CSIS_Interrupt_Src(state, 0U);
	intr_status1 = MIPI_CSIS_Get_CSIS_Interrupt_Src(state, 1U);

	intr_mask0 = MIPI_CSIS_Get_CSIS_Interrupt_Mask(state, 0U);
	intr_mask1 = MIPI_CSIS_Get_CSIS_Interrupt_Mask(state, 1U);

	intr_status0 &= intr_mask0;
	intr_status1 &= intr_mask1;

	/* interruptsource register 0 */
	if (intr_status0 & CIM_MSK_FrameStart_MASK) {
		for (idx = 0U; idx < MAX_VC; idx++) {
			if (intr_status0 &
			  ((1U << idx) << CIM_MSK_FrameStart_SHIFT))
				loge(&(state->pdev->dev),
					"[CH%d]FrameStart packet is received\n",
					idx);
		}
	}
	if (intr_status0 & CIM_MSK_FrameEnd_MASK) {
		for (idx = 0U; idx < MAX_VC; idx++) {
			if (intr_status0 &
			  ((1U << idx) << CIM_MSK_FrameEnd_SHIFT))
				loge(&(state->pdev->dev),
					"[CH%d]FrameEnd packet is received\n",
					idx);
		}
	}
	if (intr_status0 & CIM_MSK_ERR_SOT_HS_MASK) {
		for (idx = 0U; idx < MAX_VC; idx++) {
			if (intr_status0 &
			  ((1U << idx) << CIM_MSK_ERR_SOT_HS_SHIFT))
				loge(&(state->pdev->dev),
					"[Lane%d]Start of transmission error\n",
					idx);
		}
	}
	if (intr_status0 & CIM_MSK_ERR_LOST_FS_MASK) {
		for (idx = 0U; idx < MAX_VC; idx++) {
			if (intr_status0 &
			  ((1U << idx) << CIM_MSK_ERR_LOST_FS_SHIFT))
				loge(&(state->pdev->dev),
					"[CH%d]Lost of Frame Start packet\n",
					idx);
		}
	}
	if (intr_status0 & CIM_MSK_ERR_LOST_FE_MASK) {
		for (idx = 0U; idx < 4U; idx++) {
			if (intr_status0 &
			  ((1U << idx) << CIM_MSK_ERR_LOST_FE_SHIFT))
				loge(&(state->pdev->dev),
					"[CH%d]Lost of Frame End packet\n",
					idx);
		}
	}
	if (intr_status0 & CIM_MSK_ERR_OVER_MASK)
		loge(&(state->pdev->dev), "Image FIFO overflow interrupt\n");

	if (intr_status0 & CIM_MSK_ERR_WRONG_CFG_MASK)
		loge(&(state->pdev->dev), "Wrong configuration\n");

	if (intr_status0 & CIM_MSK_ERR_ECC_MASK)
		loge(&(state->pdev->dev), "ECC error\n");

	if (intr_status0 & CIM_MSK_ERR_CRC_MASK)
		loge(&(state->pdev->dev), "CRC error\n");

	if (intr_status0 & CIM_MSK_ERR_ID_MASK)
		loge(&(state->pdev->dev), "Unknown ID error\n");

	/* interruptsource register 1 */
	if (intr_status1 & CIM_MSK_LINE_END_MASK) {
		for (idx = 0U; idx < MAX_VC; idx++) {
			if (intr_status1 &
			  ((1U << idx) << CIM_MSK_LINE_END_SHIFT))
				loge(&(state->pdev->dev),
					"[CH%d] End of specific line\n",
					idx);
		}
	}

	// clear interrupt
	MIPI_CSIS_Set_CSIS_Interrupt_Src(state, 0U, intr_status0);
	MIPI_CSIS_Set_CSIS_Interrupt_Src(state, 1U, intr_status1);

	return IRQ_HANDLED;
}

static int tcc_mipi_csi2_notifier_bound(struct v4l2_async_notifier *notifier,
	struct v4l2_subdev *subdev,
	struct v4l2_async_subdev *asd)
{
	struct tcc_mipi_csi2_state	*state		= NULL;
	struct tcc_mipi_csi2_channel	*ch		= NULL;
	int				ret		= 0;

	ch = container_of(notifier, struct tcc_mipi_csi2_channel, notifier);
	state = ch->state;

	logi(&(state->pdev->dev), "v4l2-subdev %s is bounded\n", subdev->name);

	ch->remote_sd = subdev;

	return ret;
}

static void tcc_mipi_csi2_notifier_unbind(struct v4l2_async_notifier *notifier,
	struct v4l2_subdev *subdev,
	struct v4l2_async_subdev *asd)
{
	struct tcc_mipi_csi2_state	*state		= NULL;
	struct tcc_mipi_csi2_channel	*ch		= NULL;

	ch = container_of(notifier, struct tcc_mipi_csi2_channel, notifier);
	state = ch->state;

	logi(&(state->pdev->dev), "v4l2-subdev %s is unbounded\n", subdev->name);
}

static const struct v4l2_async_notifier_operations tcc_mipi_csi2_notifier_ops = {
	.bound = tcc_mipi_csi2_notifier_bound,
	.unbind = tcc_mipi_csi2_notifier_unbind,
};

static int tcc_mipi_csi2_parse_dt(struct tcc_mipi_csi2_state *state)
{
	struct platform_device		*pdev		= state->pdev;
	struct device			*dev		= &pdev->dev;
	struct device_node		*node		= pdev->dev.of_node;

#if IS_ENABLED(CONFIG_ARCH_TCC803X)
	struct device_node		*ddicfg_node	= NULL;
#endif
	struct device_node		*loc_ep		= NULL;
	struct v4l2_fwnode_endpoint	endpoint = { 0, };
	struct resource			*mem_res;
#if defined(CONFIG_ARCH_TCC805X)
	char				prop_name[32]	= {0, };
	struct device_node		*p_node		= NULL;
	unsigned int			index		= 0U;
#endif
	int				ret		= 0;

	/*
	 * Get MIPI CSI-2 base address
	 */
	mem_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "csi");
	state->csi_base = devm_ioremap_resource(dev, mem_res);
	if (IS_ERR((const void *)state->csi_base))
		return PTR_ERR((const void *)state->csi_base);

#if defined(CONFIG_ARCH_TCC805X)
	/*
	 * Get CKC base address
	 */
	state->ckc_base = (void __iomem *)of_iomap(node, 2);
	if (IS_ERR((const void *)state->ckc_base))
		return PTR_ERR((const void *)state->ckc_base);

	/*
	 * Get CFG base address
	 */
	state->cfg_base = (void __iomem *)of_iomap(node, 3);
	if (IS_ERR((const void *)state->cfg_base))
		return PTR_ERR((const void *)state->cfg_base);

	/*
	 * Get mipi_chmux_X selection
	 */
	p_node = of_get_parent(node);
	if (IS_ERR((const void *)p_node)) {
		loge(&(state->pdev->dev), "can not find mipi_wrap node\n");
		return PTR_ERR((const void *)p_node);
	}
	for (index = 0U; index < CSI_CFG_MIPI_CHMUX_MAX; index++) {
		ret = scnprintf(prop_name, sizeof(prop_name),
				"mipi-chmux-%d", index);
		if (ret == 0) {
			loge(&(state->pdev->dev),
				"Fail parsing mipi-chmux-%d property\n", index);
			ret = -EINVAL;
			goto err;
		}

		of_property_read_u32_index(p_node,
			prop_name, 0U, &(state->mipi_chmux[index]));
	}

	/*
	 * Get ISPX_BYP selection
	 */
	for (index = 0U; index < CSI_CFG_ISP_BYPASS_MAX; index++) {
		ret = scnprintf(prop_name, sizeof(prop_name),
				"isp%d-bypass", index);
		if (ret == 0) {
			loge(&(state->pdev->dev),
				"Fail parsing isp%d-bypass property\n", index);
			ret = -EINVAL;
			goto err;
		}

		of_property_read_u32_index(p_node,
			prop_name, 0U, &(state->isp_bypass[index]));
	}
#endif
	/*
	 * get interrupt number
	 */
	state->irq = platform_get_irq_byname(pdev, "csi");
	if (state->irq < 0) {
		loge(&(state->pdev->dev), "fail - get irq\n");
		ret = -ENODEV;
		goto err;
	}

	logd(&(state->pdev->dev), "csi irq number is %d\n", state->irq);

#if IS_ENABLED(CONFIG_ARCH_TCC803X)
	// ddi config
	ddicfg_node =
		of_find_compatible_node(NULL, NULL, "telechips,ddi_config");
	if (ddicfg_node == NULL) {
		loge(&(state->pdev->dev), "cann't find DDI Config node\n");
		ret = -ENODEV;
		goto err;
	} else {
		state->ddicfg_base =
			(void __iomem *)of_iomap(ddicfg_node, 0);
		logd(&(state->pdev->dev), "ddicfg addr: %p\n",
			state->ddicfg_base);
	}
#endif

	/*
	 * Parsing input port information
	 */
	loc_ep = of_graph_get_next_endpoint(node, NULL);
	if (!loc_ep) {
		loge(&(state->pdev->dev), "input endpoint does not exist");
		ret = -EINVAL;
		goto err;
	}

	/* Get input MIPI CSI2 bus info */
	ret = of_property_read_u32(loc_ep,
		"num-channel", &state->input_ch_num);
	if (ret) {
		/* default value */
		state->input_ch_num = 1U;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(loc_ep), &endpoint);
	if (ret)
		goto err;
	state->data_lane_num = endpoint.bus.mipi_csi2.num_data_lanes;

	if (state->data_lane_num == 0U) {
		/* default value */
		state->data_lane_num = 1U;
	}

	/* CSIS common control parameter */
	ret = of_property_read_u32(loc_ep,
		"deskew-level", &state->deskew_level);
	if (ret) {
		/* default value */
		state->deskew_level = 0U;
	}

	ret = of_property_read_u32(loc_ep,
		"deskew-enable", &state->deskew_enable);
	if (ret) {
		/* default value */
		state->deskew_enable = 1U;
	}

	ret = of_property_read_u32(loc_ep,
		"interleave-mode", &state->interleave_mode);
	if (ret) {
		/* default value */
		state->interleave_mode = 0U;
	}

	ret = of_property_read_u32(loc_ep,
		"update-shadow-ctrl", &state->update_shadow_ctrl);
	if (ret) {
		/* default value */
		state->update_shadow_ctrl = 0U;
	}

	logi(&(state->pdev->dev),
		"input_ch_num %d\n", state->input_ch_num);
	logi(&(state->pdev->dev),
		"data_lane_num %d\n", state->data_lane_num);
	logi(&(state->pdev->dev),
		"deskew_level %d\n", state->deskew_level);
	logi(&(state->pdev->dev),
		"deskew_enable %d\n", state->deskew_enable);
	logi(&(state->pdev->dev),
		"interleave_mode %d\n", state->interleave_mode);
	logi(&(state->pdev->dev),
		"update_shadow_ctrl %d\n", state->update_shadow_ctrl);


	/* DPHY common control parameter */
	ret = of_property_read_u32(loc_ep,
		"hs-settle", &state->hssettle);
	if (ret) {
		/* default value */
		state->hssettle = 0U;
	}

	/* depends on the DPHY sepecification version */
	state->s_clksettlectl = 0U;

	ret = of_property_read_u32(loc_ep,
		"s-dpdn-swap-clk", &state->s_dpdn_swap_clk);
	if (ret) {
		/* default value */
		state->s_dpdn_swap_clk = 0U;
	}

	ret = of_property_read_u32(loc_ep,
		"s-dpdn-swap-dat", &state->s_dpdn_swap_dat);
	if (ret) {
		/* default value */
		state->s_dpdn_swap_dat = 0U;
	}

	logi(&(state->pdev->dev),
		"hs-settle %d\n", state->hssettle);
	logi(&(state->pdev->dev),
		"s-dpdn-swap-clk %d\n", state->s_dpdn_swap_clk);
	logi(&(state->pdev->dev),
		"s-dpdn-swap-dat %d\n", state->s_dpdn_swap_dat);

	/*
	 * parsing all endpoint to alloc subdevice and async subdevice
	 */
	do {
		const char *io = NULL;
		unsigned int channel = 0;
		struct device_node *rem_ep = NULL;

		ret = of_property_read_string(loc_ep, "io-direction", &io);
		if (ret) {
			loge(&(state->pdev->dev),
				"io-direction property does not exist\n");
			goto err;
		}

		ret = of_property_read_u32(loc_ep, "channel", &channel);
		if (ret) {
			loge(&(state->pdev->dev),
				"Channel property does not exist\n");
			goto err;
		}

		if (channel >= MAX_CHANNEL) {
			loge(&(state->pdev->dev),
				"Channel property is out of bound(>=%d)\n",
				MAX_CHANNEL);
			goto err;
		}

		if (!strcmp(io, "input")) {
			struct device_node *remt_dev = NULL;
			/*
			 * init aysnc subdev instance for remote device
			 */
			rem_ep = of_graph_get_remote_endpoint(loc_ep);
			if (!rem_ep) {
				loge(&(state->pdev->dev),
					"Problem in Remote endpoint");
				ret = -ENODEV;
				goto err;
			}

			remt_dev = of_graph_get_port_parent(rem_ep);
			if (!remt_dev) {
				loge(&(state->pdev->dev),
					"Problem in Remote device node");
				ret = -ENODEV;
				of_node_put(rem_ep);
				goto err;
			}
			logi(&(state->pdev->dev),
				"linked remote device - %s, remote ep - %s\n",
				remt_dev->name, rem_ep->full_name);

			state->channel[channel].asd.match_type =
				V4L2_ASYNC_MATCH_FWNODE;
			state->channel[channel].asd.match.fwnode =
				of_fwnode_handle(rem_ep);

			of_node_put(remt_dev);
			of_node_put(rem_ep);
		} else if (!strcmp(io, "output")) {
			struct isp_state *isp_info;

			/*
			 * init subdev instance for this device
			 */
			/* set fwnode of output endpoint */
			state->channel[channel].sd.fwnode =
				of_fwnode_handle(loc_ep);

			/* print fwnode */
			logd(&(state->pdev->dev),
				"output[%d]\n", channel);

			isp_info = &state->isp_info[channel];
			ret = of_property_read_u32(loc_ep,
				"pixel-mode", &isp_info->pixel_mode);
			if (ret) {
				/* set default value */
				loge(&(state->pdev->dev),
					"pixel-mode property does not exist\n");
				isp_info->pixel_mode = 0U;
				goto err;
			} else {
				/* print parsing value */
				logi(&(state->pdev->dev),
					"ch %d pixel_mode is %d\n",
					channel, state->isp_info->pixel_mode);
			}
		} else {
			loge(&(state->pdev->dev),
				"Wrong io-direction property value");
			ret = -EINVAL;
			goto err;
		}

		loc_ep = of_graph_get_next_endpoint(node, loc_ep);
	} while (loc_ep != NULL);
err:
	of_node_put(loc_ep);

	return ret;
}

#if IS_ENABLED(CONFIG_ARCH_TCC803X)
static void tcc_mipi_csi2_clk_put(struct tcc_mipi_csi2_state *state)
{
	if (!IS_ERR(state->pixel_clock)) {
		clk_unprepare(state->pixel_clock);
		clk_put(state->pixel_clock);
		state->pixel_clock = ERR_PTR(-EINVAL);
	}
}

static int tcc_mipi_csi2_clk_get(struct tcc_mipi_csi2_state *state)
{
	int ret;
	struct device *dev = &state->pdev->dev;

	struct device_node *node = dev->of_node;

	if (of_property_read_u32(node,
		"clock-frequency",
		&state->clk_frequency)) {
		state->clk_frequency = DEFAULT_CSIS_FREQ;
		goto err;
	}

	state->pixel_clock = clk_get(dev, "mipi-csi-clk");
	if (IS_ERR(state->pixel_clock)) {
		loge(&(state->pdev->dev), "Failed to clk_get\n");
		ret = PTR_ERR(state->pixel_clock);
		goto err;
	}

	ret = clk_prepare(state->pixel_clock);
	if (ret < 0) {
		loge(&(state->pdev->dev), "Failed to clk_prepare\n");

		clk_put(state->pixel_clock);
		state->pixel_clock = ERR_PTR(-EINVAL);
		goto err;
	}

	return ret;

err:
	loge(&(state->pdev->dev), "fail - get clock\n");
	return ret;
}
#endif // IS_ENABLED(CONFIG_ARCH_TCC803X)

static void tcc_mipi_csi2_init_format(struct tcc_mipi_csi2_state *state)
{
	unsigned int i = 0U;

	state->dv_timings.type = V4L2_DV_BT_656_1120;
	state->dv_timings.bt.width =  DEFAULT_WIDTH;
	state->dv_timings.bt.height = DEFAULT_HEIGHT;
	state->dv_timings.bt.interlaced = V4L2_DV_PROGRESSIVE;
	/* IMPORTANT
	 * The below field "polarities" is not used
	 * becasue polarities for vsync and hsync are supported only.
	 * So, use flags of "struct v4l2_mbus_config".
	 */
	state->dv_timings.bt.polarities = 0U;

	for (i = 0U; i < MAX_VC; i++) {
		state->isp_info[i].fmt.width = DEFAULT_WIDTH;
		state->isp_info[i].fmt.height = DEFAULT_HEIGHT;
		state->isp_info[i].fmt.code = MEDIA_BUS_FMT_YUYV8_1X16;
		state->isp_info[i].fmt.field = V4L2_FIELD_NONE;
		state->isp_info[i].fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
		state->isp_info[i].data_format =
			code_to_csi_dt(state->isp_info[i].fmt.code);
	}
}

int tcc_mipi_csi2_enable(const struct tcc_mipi_csi2_state *state,
		unsigned int enable)
{
	int ret = 0;

	tcc_mipi_csi2_set_interface(state, enable);

	ret = tcc_mipi_csi2_set_interrupt(state, enable);
	if (ret < 0) {
		loge(&(state->pdev->dev),
			"fail - tcc_mipi_csi2_set_interrupt\n");
		goto err;
	}

err:
	return ret;
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int tcc_mipi_csi2_init(struct v4l2_subdev *sd, u32 enable)
{
	struct tcc_mipi_csi2_channel	*ch	= to_channel(sd);
	struct tcc_mipi_csi2_state	*state	= to_state(sd);
	int				ret	= 0;

	mutex_lock(&state->lock);

	if ((state->use_cnt == 0U) && (enable == 1U)) {
		tcc_mipi_csi2_enable(state, enable);

		/* init of remote subdev */
		ret = v4l2_subdev_call(ch->remote_sd, core, init, enable);
		if (ret < 0) {
			/* failure of init */
			logd(&(state->pdev->dev), "init, ret: %d\n", ret);
		}
	} else if ((state->use_cnt == 1U) && (enable == 0U)) {
		/* init of remote subdev */
		ret = v4l2_subdev_call(ch->remote_sd, core, init, enable);
		if (ret < 0) {
			/* failure of init */
			logd(&(state->pdev->dev), "init, ret: %d\n", ret);
		}

		tcc_mipi_csi2_enable(state, enable);
	}

	if (enable)
		state->use_cnt++;
	else
		state->use_cnt--;

	logi(&(state->pdev->dev), "use_cnt is %d\n", state->use_cnt);

	mutex_unlock(&state->lock);

	return ret;
}

static int tcc_mipi_csi2_s_power(struct v4l2_subdev *sd, int on)
{
	struct tcc_mipi_csi2_channel	*ch	= to_channel(sd);
	struct tcc_mipi_csi2_state	*state	= to_state(sd);
	int				ret	= 0;

	mutex_lock(&state->lock);

	/* init of remote subdev */
	ret = v4l2_subdev_call(ch->remote_sd, core, s_power, on);
	if (ret < 0) {
		/* failure of s_stream */
		logd(&(state->pdev->dev), "s_power, ret: %d\n", ret);
	}

	mutex_unlock(&state->lock);

	return ret;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int tcc_mipi_csi2_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct tcc_mipi_csi2_channel	*ch	= to_channel(sd);
	struct tcc_mipi_csi2_state	*state	= to_state(sd);
	int				ret	= 0;

	mutex_lock(&state->lock);

	/* s_stream of remote subdev */
	ret = v4l2_subdev_call(ch->remote_sd, video, s_stream, enable);
	if (ret < 0) {
		/* failure of s_stream */
		logd(&(state->pdev->dev), "s_stream, ret: %d\n", ret);
	}

	mutex_unlock(&state->lock);

	return ret;
}

static int tcc_mipi_csi2_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct tcc_mipi_csi2_channel	*ch	= to_channel(sd);
	struct tcc_mipi_csi2_state	*state	= to_state(sd);
	int				ret	= 0;

	mutex_lock(&state->lock);

	ret = v4l2_subdev_call(ch->remote_sd, video, g_frame_interval, interval);
	if (ret < 0) {
		/* failure of g_frame_interval */
		logd(&(state->pdev->dev), "g_frame_interval, ret: %d\n", ret);
	}

	mutex_unlock(&state->lock);

	return ret;
}

static int tcc_mipi_csi2_s_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct tcc_mipi_csi2_channel	*ch	= to_channel(sd);
	struct tcc_mipi_csi2_state	*state	= to_state(sd);
	int				ret	= 0;

	mutex_lock(&state->lock);

	ret = v4l2_subdev_call(ch->remote_sd, video, s_frame_interval, interval);
	if (ret < 0) {
		/* failure of s_frame_interval */
		logd(&(state->pdev->dev), "s_frame_interval, ret: %d\n", ret);
	}

	mutex_unlock(&state->lock);

	return ret;
}

static int tcc_mipi_csi2_g_dv_timings(struct v4l2_subdev *sd,
				      struct v4l2_dv_timings *timings)
{
	struct tcc_mipi_csi2_state	*state	= to_state(sd);
	int				ret	= 0;

	mutex_lock(&state->lock);

	memcpy((void *)timings,
		(const void *)&state->dv_timings,
		sizeof(*timings));

	mutex_unlock(&state->lock);

	return ret;
}

static int tcc_mipi_csi2_g_mbus_config(struct v4l2_subdev *sd,
				       struct v4l2_mbus_config *cfg)
{
	memcpy((void *)cfg, (const void *)&mipi_csi2_mbus_config, sizeof(*cfg));

	return 0;
}

/*
 * v4l2_subdev_pad_ops implementations
 */
static int tcc_mipi_csi2_enum_mbus_code(struct v4l2_subdev *sd,
					struct v4l2_subdev_pad_config *cfg,
					struct v4l2_subdev_mbus_code_enum *code)
{
	struct tcc_mipi_csi2_state	*state	= to_state(sd);
	int				ret	= 0;

	mutex_lock(&state->lock);

	if ((code->pad != 0) || (ARRAY_SIZE(tcc_mipi_csi2_codes) <= code->index)) {
		/* pad is null or index is wrong */
		ret = -EINVAL;
	} else {
		/* get code */
		code->code = tcc_mipi_csi2_codes[code->index];
	}

	mutex_unlock(&state->lock);

	return ret;
}

static int tcc_mipi_csi2_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct tcc_mipi_csi2_channel	*ch	= to_channel(sd);
	struct tcc_mipi_csi2_state	*state	= to_state(sd);
	int				ret	= 0;

	mutex_lock(&state->lock);

	ret = v4l2_subdev_call(ch->remote_sd, pad, enum_frame_size, NULL, fse);
	if (ret < 0) {
		/* failure of enum_frame_size */
		logd(&(state->pdev->dev), "enum_frame_size, ret: %d\n", ret);
	}

	mutex_unlock(&state->lock);

	return ret;
}

static int tcc_mipi_csi2_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	struct tcc_mipi_csi2_channel	*ch	= to_channel(sd);
	struct tcc_mipi_csi2_state	*state	= to_state(sd);
	int				ret	= 0;

	mutex_lock(&state->lock);

	ret = v4l2_subdev_call(ch->remote_sd, pad, enum_frame_interval, NULL, fie);
	if (ret < 0) {
		/* failure of enum_frame_interval */
		logd(&(state->pdev->dev), "enum_frame_interval, ret: %d\n", ret);
	}

	mutex_unlock(&state->lock);

	return ret;
}

static int tcc_mipi_csi2_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct tcc_mipi_csi2_channel	*ch	= to_channel(sd);
	struct tcc_mipi_csi2_state	*state	= to_state(sd);
	int ret	= 0;

	mutex_lock(&state->lock);

	/* get_fmt of remote subdev */
	ret = v4l2_subdev_call(ch->remote_sd, pad, get_fmt, cfg, format);
	if (ret < 0) {
		/* failure of get_fmt */
		logd(&(state->pdev->dev), "get_fmt, ret: %d\n", ret);
	} else {
		logd(&(state->pdev->dev), "size: %d * %d\n",
			format->format.width, format->format.height);
		logd(&(state->pdev->dev), "code: 0x%08x\n",
			format->format.code);
	}

	mutex_unlock(&state->lock);

	return ret;
}

static int tcc_mipi_csi2_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct tcc_mipi_csi2_channel	*ch	= to_channel(sd);
	struct tcc_mipi_csi2_state	*state	= to_state(sd);
	int ret	= 0;
	unsigned int i = 0U;

	mutex_lock(&state->lock);

	/* set_fmt of remote subdev */
	ret = v4l2_subdev_call(ch->remote_sd, pad, set_fmt, cfg, format);
	if (ret < 0) {
		/* failure of set_fmt */
		logd(&(state->pdev->dev), "set_fmt, ret: %d\n", ret);
	} else {
		for (i = 0U; i < MAX_VC; i++) {
			memcpy((void *)&state->isp_info[i].fmt,
				(const void *)&format->format,
				sizeof(struct v4l2_mbus_framefmt));

			state->isp_info[i].data_format =
				code_to_csi_dt(state->isp_info[i].fmt.code);
		}

		state->dv_timings.bt.width = format->format.width;
		state->dv_timings.bt.height = format->format.height;
	}

	mutex_unlock(&state->lock);

	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static const struct v4l2_subdev_core_ops tcc_mipi_csi2_core_ops = {
	.init			= tcc_mipi_csi2_init,
	.s_power		= tcc_mipi_csi2_s_power,
};

static const struct v4l2_subdev_video_ops tcc_mipi_csi2_video_ops = {
	.s_stream		= tcc_mipi_csi2_s_stream,
	.g_frame_interval	= tcc_mipi_csi2_g_frame_interval,
	.s_frame_interval	= tcc_mipi_csi2_s_frame_interval,
	.g_dv_timings		= tcc_mipi_csi2_g_dv_timings,
	.g_mbus_config		= tcc_mipi_csi2_g_mbus_config,
};

static const struct v4l2_subdev_pad_ops tcc_mipi_csi2_pad_ops = {
	.enum_mbus_code		= tcc_mipi_csi2_enum_mbus_code,
	.enum_frame_size	= tcc_mipi_csi2_enum_frame_size,
	.enum_frame_interval	= tcc_mipi_csi2_enum_frame_interval,
	.get_fmt		= tcc_mipi_csi2_get_fmt,
	.set_fmt		= tcc_mipi_csi2_set_fmt,
};

static const struct v4l2_subdev_ops tcc_mipi_csi2_ops = {
	.core			= &tcc_mipi_csi2_core_ops,
	.video			= &tcc_mipi_csi2_video_ops,
	.pad			= &tcc_mipi_csi2_pad_ops,
};

static const struct of_device_id tcc_mipi_csi2_of_match[];

static int tcc_mipi_csi2_probe(struct platform_device *pdev)
{
	struct tcc_mipi_csi2_state	*state	= NULL;
	struct tcc_mipi_csi2_channel	*ch	= NULL;
	const struct of_device_id	*of_id	= NULL;
	unsigned int			idxch	= 0U;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	state = devm_kzalloc(&pdev->dev, sizeof(*state), GFP_KERNEL);
	if (WARN_ON(state == NULL)) {
		ret = -ENOMEM;
		goto err;
	}

	mutex_init(&state->lock);
	platform_set_drvdata(pdev, state);

	/* set the specific information */
	of_id = of_match_node(tcc_mipi_csi2_of_match, pdev->dev.of_node);
	if (WARN_ON(of_id == NULL)) {
		ret = -EINVAL;
		goto err;
	}

	pdev->id = of_alias_get_id(pdev->dev.of_node, "mipi-csi2-");
	state->pdev = pdev;

	/* init subdevice instances */
	for (idxch = 0U; idxch < MAX_CHANNEL; idxch++) {
		ch = &state->channel[idxch];
		ch->id = idxch;

		/* Register with V4L2 layer as a slave device */
		v4l2_subdev_init(&ch->sd, &tcc_mipi_csi2_ops);
		ch->sd.owner = pdev->dev.driver->owner;
		ch->sd.dev = &pdev->dev;
		v4l2_set_subdevdata(&ch->sd, ch);

		ret = scnprintf(ch->sd.name, sizeof(ch->sd.name),
				"%s.%d", dev_name(&pdev->dev), idxch);
		if (ret == 0) {
			loge(&(state->pdev->dev),
				"Fail setting subdevice name\n");
			ret = -EINVAL;
			goto err;
		}

		/* init notifier */
		v4l2_async_notifier_init(&ch->notifier);
		ch->notifier.ops = &tcc_mipi_csi2_notifier_ops;

		ch->state = state;
	}

	/* parse device tree */
	ret = tcc_mipi_csi2_parse_dt(state);
	if (ret < 0) {
		loge(&(state->pdev->dev), "tcc_mipi_csi2_parse_dt, ret: %d\n", ret);
		goto err;
	}

	/*
	 * set clock
	 */
#if IS_ENABLED(CONFIG_ARCH_TCC803X)
	ret = tcc_mipi_csi2_clk_get(state);
	if (ret < 0)
		goto err;

	ret = clk_set_rate(state->pixel_clock, state->clk_frequency);
	if (ret < 0)
		goto e_clkput;

	ret = clk_enable(state->pixel_clock);
	if (ret < 0)
		goto e_clkput;

	logi(&(state->pdev->dev), "csi pixel_clock is %d Hz\n", state->clk_frequency);
#else
	if (MIPI_WRAP_Set_CKC(state)) {
		loge(&(state->pdev->dev), "fail - mipi wrap clock setting\n");
		ret = -ENODEV;
		goto err;
	}
#endif
	/*
	 * request irq
	 */
	ret = devm_request_irq(&pdev->dev, state->irq, tcc_mipi_csi2_irq_handler,
			0U, dev_name(&pdev->dev), state);
	if (ret) {
		loge(&(state->pdev->dev), "fail - Interrupt request\n");
		goto e_clkdis;
	}

	/*
	 * add allocated async subdev instance to a notifier.
	 * The async subdev is the linked device
	 * in front of this device.
	 */
	for (idxch = 0U; idxch < state->input_ch_num; idxch++) {
		ch = &state->channel[idxch];

		if (ch->asd.match.fwnode == NULL) {
			/* async subdev to register is not founded */
			continue;
		}

		ret = v4l2_async_notifier_add_subdev(&ch->notifier, &ch->asd);
		if (ret) {
			loge(&(state->pdev->dev),
				"v4l2_async_notifier_add_subdev(%d)\n", ret);
			goto end;
		}

		/* register a notifier */
		ret = v4l2_async_subdev_notifier_register(&ch->sd, &ch->notifier);
		if (ret < 0) {
			loge(&(state->pdev->dev),
				"v4l2_async_subdev_notifier_register(%d)\n", ret);
			v4l2_async_notifier_cleanup(&ch->notifier);
			goto end;
		}
	}

	/*
	 * register subdev instance.
	 * The subdev instance is this device.
	 */
	for (idxch = 0U; idxch < state->input_ch_num; idxch++) {
		ch = &state->channel[idxch];
		/* register a v4l2-subdev */

		if (ch->sd.fwnode == NULL) {
			/* subdev to register is not founded */
			continue;
		}

		ret = v4l2_async_register_subdev(&ch->sd);
		if (ret) {
			/* failure of v4l2_async_register_subdev */
			loge(&(state->pdev->dev),
				"v4l2_async_register_subdev(%d)\n", ret);
			goto e_clkdis;
		} else {
			/* success of v4l2_async_register_subdev */
			logi(&(state->pdev->dev),
				"%s is registered as v4l2-subdev\n",
				state->channel[idxch].sd.name);
		}
	}

	tcc_mipi_csi2_init_format(state);

	logi(&(state->pdev->dev), "Success proving MIPI-CSI2-%d\n", pdev->id);

	goto end;

e_clkdis:
#if IS_ENABLED(CONFIG_ARCH_TCC803X)
	clk_disable(state->pixel_clock);
e_clkput:
	tcc_mipi_csi2_clk_put(state);
#endif
err:
end:
	return ret;
}

static int tcc_mipi_csi2_remove(struct platform_device *pdev)
{
	struct tcc_mipi_csi2_state *state = platform_get_drvdata(pdev);

	int ret = 0;

	logi(&(state->pdev->dev), "%s in\n", __func__);

#if IS_ENABLED(CONFIG_ARCH_TCC803X)

	clk_disable(state->pixel_clock);
	tcc_mipi_csi2_clk_put(state);
#endif
	logi(&(state->pdev->dev), "%s out\n", __func__);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int tcc_mipi_csi2_suspend(struct device *dev)
{
	struct platform_device		*pdev	= to_platform_device(dev);
	struct tcc_mipi_csi2_state	*state	= platform_get_drvdata(pdev);
	int ret = 0;

	logi(&(state->pdev->dev), "%s in\n", __func__);

#if IS_ENABLED(CONFIG_ARCH_TCC803X)
	clk_disable(state->pixel_clock);
	tcc_mipi_csi2_clk_put(state);
#endif

	return ret;
}

static int tcc_mipi_csi2_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tcc_mipi_csi2_state *state = platform_get_drvdata(pdev);
	int ret = 0;

	logi(&(state->pdev->dev), "%s in\n", __func__);

#if IS_ENABLED(CONFIG_ARCH_TCC803X)
	ret = tcc_mipi_csi2_clk_get(state);
	if (ret < 0)
		goto err;

	ret = clk_set_rate(state->pixel_clock, state->clk_frequency);
	if (ret < 0)
		goto e_clkput;

	ret = clk_enable(state->pixel_clock);
	if (ret < 0)
		goto e_clkput;

	logi(&(state->pdev->dev), "csi pixel_clock is %d Hz\n", state->clk_frequency);
#else
	if (MIPI_WRAP_Set_CKC(state)) {
		loge(&(state->pdev->dev), "fail - mipi wrap clock setting\n");
		ret = -ENODEV;
		goto err;
	}
#endif

	return ret;

#if IS_ENABLED(CONFIG_ARCH_TCC803X)
	clk_disable(state->pixel_clock);
e_clkput:
	tcc_mipi_csi2_clk_put(state);
#endif
err:
	return ret;
}
#endif

static const struct dev_pm_ops tcc_mipi_csi2_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tcc_mipi_csi2_suspend, tcc_mipi_csi2_resume)
};

static const struct of_device_id tcc_mipi_csi2_of_match[] = {
	{
		.compatible	= "telechips,tcc803x-mipi-csi2",
	},
	{
		.compatible	= "telechips,tcc805x-mipi-csi2",
	},
	{
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, tcc_mipi_csi2_of_match);

static struct platform_driver tcc_mipi_csi2_driver = {
	.probe = tcc_mipi_csi2_probe,
	.remove = tcc_mipi_csi2_remove,
	.driver = {
		.name = TCC_MIPI_CSI2_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= tcc_mipi_csi2_of_match,
		.pm = &tcc_mipi_csi2_pm_ops,
	},
};
module_platform_driver(tcc_mipi_csi2_driver);

MODULE_AUTHOR("Telechips <www.telechips.com>");
MODULE_DESCRIPTION("Telechips TCCXXXX SoC MIPI-CSI2 receiver driver");
MODULE_LICENSE("GPL");

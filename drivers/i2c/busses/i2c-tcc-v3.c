// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <asm/irq.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/pinctrl/consumer.h>

/* I2C Configuration Reg. */
#define I2C_PRES	0x00
#define I2C_CTRL	0x04
#define I2C_TXR		0x08
#define I2C_CMD		0x0C
#define I2C_RXR		0x10
#define I2C_SR		0x14
#define I2C_TR0		0x18
#define I2C_TR1		0x24
#define I2C_ACR		0xFC

/* I2C CTRL Reg */
#define CTRL_EN		((u32)BIT(7))
#define CTRL_IEN	((u32)BIT(6))

/* I2C CMD Reg */
#define CMD_START	((u32)BIT(7))
#define CMD_STOP	((u32)BIT(6))
#define CMD_RD		((u32)BIT(5))
#define CMD_WR		((u32)BIT(4))
#define CMD_NACK	((u32)BIT(3))
#define CMD_IACK	((u32)BIT(0))

/* I2C SR Reg */
#define SR_RX_ACK	((u32)BIT(7))
#define SR_BUSY		((u32)BIT(6))
#define SR_AL		((u32)BIT(5))
#define SR_TIP		((u32)BIT(1))
#define SR_IF		((u32)BIT(0))

/* I2C Port Configuration Reg. */
#define I2C_PORT_CFG0	0x00
#define I2C_PORT_CFG1	0x04
#define I2C_PORT_CFG2	0x08
#define I2C_IRQ_STS	0x10

/* I2C7 Arbitration Timeout ms */
#define I2C7_ARB_TIMEOUT (msecs_to_jiffies(5))

#define I2C_PERI_CLK	4000000
#define I2C_TR0_FC_MASK ((u32)0xFFFFFFE0U)
#define I2C_TR1_MAX	4096U

/* I2C transfer state */
enum {
	STATE_IDLE,		/* Before Start */
	STATE_START,	/* Slave address write ended*/
	STATE_TRANSFER,	/* I2C data trnasfer in current msg*/
	STATE_FINISH,	/* Last msg send completed */
	STATE_COMPLETE,	/* Return to Driver */
};

/* I2C Share core state */
enum {
	NOT_USE_ALL_CPU,
	USE_CURRENT_CPU,
	USE_OTHER_CPU,
};

enum {
	TCC805X,
	TCC803X,
};

enum {
	I2C_NONE_RELEASE,
	I2C_IRQ_RELEASE,
	I2C_CORE_RELEASE,
};

#define i2c_readl  __raw_readl
#define i2c_writel __raw_writel

static uint8_t i2c_controller_enable_flag;

struct i2c_msg_flags {
	bool is_rw_reverse;
	bool is_transfer_read;
	bool is_ack_ignore;
	bool is_force_stop;
	bool is_nostop;
};

struct tcc_i2c_struct {
	void                           __iomem *base;
	void                           __iomem *port_cfg;
	void                           __iomem *share_core;

	bool                          is_last_msg;
	bool                          is_last_data;
	bool                          is_end_data;
	bool                          burst_mode;
	uint8_t                       core;
	uint16_t                      cur_data_index;
	int32_t                       all_msg_cnt;
	int32_t                       cur_msg_index;
	uint32_t                      port_mux;
	uint32_t                      i2c_clk_rate;
	uint32_t                      pwh;
	uint32_t                      pwl;
	int32_t	                      result;	/* transfer result */
	int32_t	                      state;
	uint32_t                      irq;
	uint32_t                      noise_filter;
	uint32_t                      xfer_timeout;
	uint32_t                      max_ack_timeout;
	uint32_t                      cur_ack_timeout;

	struct clk                    *pclk; /* I2C Peri */
	struct clk                    *hclk; /* I2C IO config*/
	struct clk                    *fclk; /* FBUS_IO */
	struct pinctrl                *pinctl; /* Pin-control */
	struct i2c_msg                *msgs;

	struct completion             msg_complete;

	struct device                 *dev;
	struct i2c_adapter            adap;
	struct i2c_msg_flags          msg_flags;
	const struct tcc_i2c_soc_info *soc_info;
};

struct tcc_i2c_soc_info {
	uint32_t id;
};

static ssize_t tcc_i2c_show(struct device *dev,
		struct device_attribute *attr,
		char *buf);

static DEVICE_ATTR(tcci2c_v3,
		(u16)S_IRUGO,
		tcc_i2c_show,
		NULL);

static void tcc_i2c_enable_flag_set(
		const struct tcc_i2c_struct *tcc_i2c,
		bool enable)
{
	if (tcc_i2c->core < 8U) {
		if (enable == (bool)true) {
			i2c_controller_enable_flag |=
				(u8)((1U << tcc_i2c->core) & 0xFFU);
		} else {
			i2c_controller_enable_flag &=
				~(u8)((1U << tcc_i2c->core) & 0xFFU);
		}
	} else {
		dev_err(tcc_i2c->dev,
			"[ERROR][I2C] %s: invalid i2c core [%d]\n",
			__func__,
			tcc_i2c->core);
	}
}

static int32_t tcc_i2c7_perm_request(void __iomem *base, bool on)
{
	ulong timeout_jiffies = 0;
	int32_t ret = 0;

	if (!on) {
		/* clear permission */
		i2c_writel(0, base + I2C_ACR);
	} else {
		/* Read status whether i2c 7 is idle */
		if (jiffies < (ULONG_MAX - I2C7_ARB_TIMEOUT)) {
			timeout_jiffies = jiffies + I2C7_ARB_TIMEOUT;
		} else {
			timeout_jiffies = I2C7_ARB_TIMEOUT;
		}

		while ((i2c_readl(base + I2C_ACR) & 0xF0UL) != 0U) {
			if (time_after(jiffies, timeout_jiffies)) {
				(void)pr_err("[ERROR][I2C] I2C 7 is busy - timeout 0\n");
				ret = -ETIMEDOUT;
				break;
			}
		}

		if (ret == 0) {
			/* request access perimission */
			i2c_writel(1, base + I2C_ACR);

			/* Check status for permission */
			if (jiffies < (ULONG_MAX - I2C7_ARB_TIMEOUT)) {
				timeout_jiffies = jiffies + I2C7_ARB_TIMEOUT;
			} else {
				timeout_jiffies = I2C7_ARB_TIMEOUT;
			}
			while ((i2c_readl(base + I2C_ACR) & 0x10UL) == 0U) {
				if (time_after(jiffies, timeout_jiffies)) {
					/* clear permission */
					i2c_writel(0, base + I2C_ACR);
					(void)pr_err("[ERROR][I2C] I2C 7 is busy - timeout 1\n");
					ret = -ETIMEDOUT;
					break;
				}
			}
		}
	}

	return ret;
}

#if defined(CONFIG_ARCH_TCC805X)
static void __iomem *i2c7_base;

int32_t tcc_i2c7_set_trfc(uint32_t fc)
{
	int32_t ret = 0;
	uint32_t reg_data;

	if ((i2c_controller_enable_flag >> 7) != 0U) {
		/* i2c7 permission request */
		ret = tcc_i2c7_perm_request(i2c7_base, (bool)1);
		if (ret == 0) {
			reg_data = i2c_readl(i2c7_base + I2C_TR0);
			reg_data &= I2C_TR0_FC_MASK;
			reg_data |= (fc & ~I2C_TR0_FC_MASK);
			i2c_writel(reg_data, i2c7_base + I2C_TR0);

			/* i2c7 permission release */
			(void)tcc_i2c7_perm_request(i2c7_base, (bool)0);
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL(tcc_i2c7_set_trfc);

int32_t tcc_i2c7_get_trfc(void)
{
	int32_t ret = 0;
	uint32_t reg_data;

	if ((i2c_controller_enable_flag >> 7) != 0U) {
		/* i2c7 permission request */
		ret = tcc_i2c7_perm_request(i2c7_base, (bool)1);
		if (ret == 0) {
			reg_data = i2c_readl(i2c7_base + I2C_TR0);
			reg_data &= ~I2C_TR0_FC_MASK;
			ret = (s32)reg_data;

			/* i2c7 permission release */
			(void)tcc_i2c7_perm_request(i2c7_base, (bool)0);
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL(tcc_i2c7_get_trfc);
#endif


/* Before disable clock, TCC805x have to check corresponding i2c core.
 * Because peripheral clock of i2c controller 0 ~ 3 is connected with 4 ~ 7.
 */
static int32_t tcc_i2c_check_share_core(const struct tcc_i2c_struct *tcc_i2c)
{
	int32_t ret = 0;
	uint8_t shared_core;
	uint32_t is_enabled = 0;

	if (tcc_i2c->soc_info->id == (u32)TCC805X) {
		/* i2c-7 is shared by ISP and AP,
		 * i2c-3 and i2c-7 share the clock
		 */
		if ((tcc_i2c->core == (u8)3) || (tcc_i2c->core == (u8)7)) {
			dev_info(tcc_i2c->dev,
				"[INFO][I2C] Clock shared core is using in another cpu\n");
			ret = USE_OTHER_CPU;
		}
	}

	if (ret == 0) {
		if (tcc_i2c->core > (u8)3) {
			shared_core = tcc_i2c->core - (u8)4;
		} else {
			shared_core = tcc_i2c->core + (u8)4;
		}

		if (shared_core <= (u8)7) {
			/* Check whether i2c share core is enabled */
			is_enabled = i2c_readl(
					tcc_i2c->share_core + I2C_CTRL) &
					CTRL_EN;
			if (is_enabled != 0U) {
				if ((i2c_controller_enable_flag &
					(1U << shared_core)) > 0U) {
					dev_info(tcc_i2c->dev,
						"[INFO][I2C] Clock shared core is using in current cpu\n");
					ret = USE_CURRENT_CPU;
				} else {
					dev_info(tcc_i2c->dev,
						"[INFO][I2C] Clock shared core is using in another cpu\n");
					ret = USE_OTHER_CPU;
				}
			} else {
				dev_info(tcc_i2c->dev, "[INFO][I2C] Clock shared core is not using\n");
				ret = NOT_USE_ALL_CPU;
			}
		} else {
			dev_err(tcc_i2c->dev, "[ERROR][I2C] Invalid core\n");
		}
	}

	return ret;
}

static void tcc_i2c_clock_disable(const struct tcc_i2c_struct *tcc_i2c)
{
	int32_t share_core_state;

	share_core_state = tcc_i2c_check_share_core(tcc_i2c);
	switch (share_core_state) {
	case USE_OTHER_CPU:
		break;
	case NOT_USE_ALL_CPU:
	case USE_CURRENT_CPU:
		if (tcc_i2c->pclk != NULL) {
			clk_disable_unprepare(tcc_i2c->pclk);
		}
		if (tcc_i2c->hclk != NULL) {
			clk_disable_unprepare(tcc_i2c->hclk);
		}
		break;
	default:
		dev_err(tcc_i2c->dev, "[ERROR][I2C] Unknown cpu status\n");
		break;
	}
}

static void tcc_i2c_v1_set_noise_filter(const struct tcc_i2c_struct *tcc_i2c,
	uint32_t filter_cnt)
{
	uint32_t tr0_reg;

	tr0_reg = i2c_readl(tcc_i2c->base + I2C_TR0);
	tr0_reg |= ((filter_cnt & 0x0000001FU) << 0);

	i2c_writel(tr0_reg, tcc_i2c->base + I2C_TR0);
}

static void tcc_i2c_v2_set_noise_filter(const struct tcc_i2c_struct *tcc_i2c,
	uint32_t filter_cnt)
{
	uint32_t tr0_reg;

	tr0_reg = i2c_readl(tcc_i2c->base + I2C_TR0);
	tr0_reg |= ((filter_cnt & 0x000001FFU) << 20);

	i2c_writel(tr0_reg, tcc_i2c->base + I2C_TR0);
}


static int32_t tcc_i2c_set_noise_filter(const struct tcc_i2c_struct *tcc_i2c)
{
	uint32_t fclk_Mhz, filter_cnt = 0;
	uint64_t temp_64 = 0;
	int32_t ret = 0;

	/* Calculate noise filter counter load value */
	temp_64 = clk_get_rate(tcc_i2c->fclk);

	if (temp_64 <= UINT_MAX) {
		fclk_Mhz = (u32)temp_64 / (u32)1000000U;

		if ((UINT_MAX / fclk_Mhz) < tcc_i2c->noise_filter) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] %s: noise filter %dns is too much\n",
				__func__,
				tcc_i2c->noise_filter);
			ret = -EINVAL;
		} else {
			filter_cnt = fclk_Mhz * tcc_i2c->noise_filter;
			filter_cnt = (filter_cnt / 1000U) + 2U;

			switch (tcc_i2c->soc_info->id) {
			case TCC803X:
				if (tcc_i2c->core < 4U) {
					tcc_i2c_v1_set_noise_filter(
							tcc_i2c,
							filter_cnt);
				} else {
					tcc_i2c_v2_set_noise_filter(
							tcc_i2c,
							filter_cnt);
				}
				break;
			case TCC805X:
				tcc_i2c_v2_set_noise_filter(
						tcc_i2c,
						filter_cnt);
				break;
			default:
				dev_err(tcc_i2c->dev,
					"[ERROR][I2C] Invalid SoC id\n");
				ret = -ENXIO;
				break;
			}

			dev_dbg(tcc_i2c->dev,
				"[DEBUG][I2C] %s: fbus_io clk: %uMHz, noise filter load value: %d\n",
				__func__,
				fclk_Mhz,
				filter_cnt);
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static int32_t tcc_i2c_v1_set_prescale(const struct tcc_i2c_struct *tcc_i2c)
{
	uint32_t prescale = 0;
	uint64_t peri_clk = clk_get_rate(tcc_i2c->pclk);
	int32_t ret = 0;

	if (peri_clk <= UINT_MAX) {
		prescale = 5U * tcc_i2c->i2c_clk_rate;
		prescale = ((u32)peri_clk / prescale);
	} else {
		ret = -EINVAL;
	}

	if (ret == 0) {
		if (prescale >= 1U) {
			prescale -= 1U;
			i2c_writel(prescale, tcc_i2c->base + I2C_PRES);
		} else{
			ret = -EINVAL;
		}
	}

	return ret;
}

static int32_t tcc_i2c_v2_set_prescale(const struct tcc_i2c_struct *tcc_i2c)
{
	uint32_t prescale = 0, temp_32 = 0;
	uint64_t peri_clk = clk_get_rate(tcc_i2c->pclk);
	uint64_t temp_64 = 0;
	int32_t ret = 0;
	/*
	 * TR0.CKSEL = 0
	 * TR0.STD = 0
	 */
	i2c_writel(i2c_readl(tcc_i2c->base + I2C_TR0) & (~0x000000E0U),
			tcc_i2c->base + I2C_TR0);

	if ((tcc_i2c->pwh <= I2C_TR1_MAX) &&
		(tcc_i2c->pwl <= I2C_TR1_MAX)) {
		temp_32 = tcc_i2c->pwh + tcc_i2c->pwl;
		temp_64 = (u64)tcc_i2c->i2c_clk_rate * temp_32;
	} else {
		ret = -EINVAL;
	}

	if (ret == 0) {
		if ((peri_clk <= UINT_MAX) &&
			(temp_64 <= UINT_MAX)) {
			prescale = ((u32)peri_clk / (u32)temp_64);
		} else {
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		if (prescale >= 1U) {
			prescale -= 1U;
			i2c_writel(prescale, tcc_i2c->base + I2C_PRES);
			i2c_writel(((tcc_i2c->pwh << 16) | (tcc_i2c->pwl)),
				tcc_i2c->base + I2C_TR1);
		} else{
			ret = -EINVAL;
		}
	}

	return ret;
}

static int32_t tcc_i2c_set_prescale(const struct tcc_i2c_struct *tcc_i2c)
{
	int32_t ret = 0;
	ulong peri_clk, fbus_ioclk;

	peri_clk = clk_get_rate(tcc_i2c->pclk);
	fbus_ioclk = clk_get_rate(tcc_i2c->fclk);

	/* bus clock must be four times faster than the peri clk
	 * master frequency for the internal synchronization
	 */
	if (fbus_ioclk < (peri_clk << (u64)2)) {
		dev_err(tcc_i2c->dev,
			"[ERROR][I2C] fbus io clk(%ldHz) must be four times faster than the peri clk(%ldHz)\n",
			fbus_ioclk,
			peri_clk);

		ret = -EIO;
	} else {
		switch (tcc_i2c->soc_info->id) {
		case TCC803X:
			if (tcc_i2c->core < 4U) {
				ret = tcc_i2c_v1_set_prescale(tcc_i2c);
			} else {
				ret = tcc_i2c_v2_set_prescale(tcc_i2c);
			}
			break;
		case TCC805X:
			ret = tcc_i2c_v2_set_prescale(tcc_i2c);
			break;
		default:
			dev_err(tcc_i2c->dev, "[ERROR][I2C] Invalid SoC id\n");
			ret = -ENXIO;
			break;
		}

		if (ret == 0) {
			dev_info(tcc_i2c->dev,
				"[INFO][I2C] port %d, pulse-width-high: %d, pulse-width-low: %d\n",
				tcc_i2c->port_mux, tcc_i2c->pwh, tcc_i2c->pwl);
		} else {
			dev_err(tcc_i2c->dev, "[ERROR][I2C] Invalid SCL frequency\n");
		}
	}

	return ret;
}

static void tcc_i2c_clear_pend_irq(const struct tcc_i2c_struct *tcc_i2c)
{
	i2c_writel((i2c_readl(tcc_i2c->base + I2C_CMD) | CMD_IACK),
			tcc_i2c->base + I2C_CMD);
}

static void tcc_i2c_intr_enable(const struct tcc_i2c_struct *tcc_i2c,
				bool enable)
{
	if (enable) {
		i2c_writel(i2c_readl(tcc_i2c->base + I2C_CTRL) | CTRL_IEN,
				tcc_i2c->base + I2C_CTRL);
	} else {
		i2c_writel(i2c_readl(tcc_i2c->base + I2C_CTRL) & ~CTRL_IEN,
				tcc_i2c->base + I2C_CTRL);
	}
}

static void tcc_i2c_start(const struct tcc_i2c_struct *tcc_i2c)
{
	uint16_t addr;
	const struct i2c_msg *msg = &tcc_i2c->msgs[tcc_i2c->cur_msg_index];

	addr = (msg->addr & (u16)0x7F) << (u16)1;
	if (tcc_i2c->msg_flags.is_transfer_read) {
		addr |= (u16)0x01;
	}
	if (tcc_i2c->msg_flags.is_rw_reverse) {
		addr ^= (u16)0x01;
	}

	dev_dbg(tcc_i2c->dev,
		"[DEBUG][I2C] slave address: 0x%02x, current message num: %d, message data length: %d\n",
		msg->addr,
		tcc_i2c->cur_msg_index,
		msg->len);
	i2c_writel((u32)addr, tcc_i2c->base + I2C_TXR);
	i2c_writel((CMD_START | CMD_WR), tcc_i2c->base + I2C_CMD);
}

static void tcc_i2c_read(const struct tcc_i2c_struct *tcc_i2c)
{
	if (tcc_i2c->is_last_data) {
		i2c_writel((CMD_RD | CMD_NACK),
			tcc_i2c->base + I2C_CMD);
	} else {
		i2c_writel(CMD_RD, tcc_i2c->base + I2C_CMD);
	}
}

static void tcc_i2c_read_to_buffer(const struct tcc_i2c_struct *tcc_i2c)
{
	const struct i2c_msg *msg = &tcc_i2c->msgs[tcc_i2c->cur_msg_index];
	uint32_t rxr;

	if (tcc_i2c->state == STATE_TRANSFER) {
		if (tcc_i2c->cur_data_index > 0U) {
			rxr = (i2c_readl(tcc_i2c->base + I2C_RXR) & 0xFFU);
			msg->buf[tcc_i2c->cur_data_index - 1U] = (u8)rxr;
			dev_dbg(tcc_i2c->dev,
				"[DEBUG][I2C] READ Data: 0x%02x\n",
				rxr);
		}
	}
}

static void tcc_i2c_write(const struct tcc_i2c_struct *tcc_i2c)
{
	const struct i2c_msg *msg = &tcc_i2c->msgs[tcc_i2c->cur_msg_index];

	i2c_writel(msg->buf[tcc_i2c->cur_data_index], tcc_i2c->base + I2C_TXR);
	i2c_writel(CMD_WR, tcc_i2c->base + I2C_CMD);
}


static bool tcc_i2c_ack_check(const struct tcc_i2c_struct *tcc_i2c)
{
	bool ret = (bool)true;
	uint32_t sr;

	if ((tcc_i2c->state == STATE_START) ||
		((tcc_i2c->state == STATE_TRANSFER) &&
		!tcc_i2c->msg_flags.is_transfer_read)) {
		sr = i2c_readl(tcc_i2c->base + I2C_SR);
		ret = ((sr & SR_RX_ACK) != 0U) ? (bool)false : (bool)true;
	}

	return ret;
}

static void tcc_i2c_stop(struct tcc_i2c_struct *tcc_i2c)
{
	if (tcc_i2c->msg_flags.is_nostop) {
		complete(&tcc_i2c->msg_complete);
	} else {
		i2c_writel(CMD_STOP, tcc_i2c->base + I2C_CMD);
	}
}

static void tcc_i2c_force_stop(const struct tcc_i2c_struct *tcc_i2c)
{
	if (!tcc_i2c->is_last_msg && tcc_i2c->is_last_data) {
		i2c_writel((i2c_readl(tcc_i2c->base + I2C_CMD) | CMD_STOP),
				tcc_i2c->base + I2C_CMD);
	}
}

/* I2C transfer force exit */
static void tcc_i2c_force_exit(struct tcc_i2c_struct *tcc_i2c)
{
	tcc_i2c_intr_enable(tcc_i2c, (bool)false);
	i2c_writel(CMD_STOP, tcc_i2c->base + I2C_CMD);
	complete(&tcc_i2c->msg_complete);
}

/* I2C bus arbitration check */
static int32_t tcc_i2c_arbitration_check(const struct tcc_i2c_struct *tcc_i2c)
{
	int32_t ret = 0;

	if ((i2c_readl(tcc_i2c->base + I2C_SR) & SR_AL) != 0U) {
		ret = -EIO;
	}
	return ret;
}

/* i2c flag parsing in current msg */
static int32_t tcc_i2c_parse_flag(struct tcc_i2c_struct *tcc_i2c)
{
	int32_t ret = 0;
	const struct i2c_msg *msg = &tcc_i2c->msgs[tcc_i2c->cur_msg_index];
	uint16_t flags = msg->flags;

	if ((flags & (u16)I2C_M_REV_DIR_ADDR) != 0U) {
		dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] I2C_M_REV_DIR_ADDR flag set\n");
		tcc_i2c->msg_flags.is_rw_reverse = (bool)true;
	} else {
		tcc_i2c->msg_flags.is_rw_reverse = (bool)false;
	}

	if ((flags & (u16)I2C_M_RD) != 0U) {
		dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] I2C_M_RD flag set\n");
		tcc_i2c->msg_flags.is_transfer_read = (bool)true;
	} else {
		tcc_i2c->msg_flags.is_transfer_read = (bool)false;
	}

	if ((flags & (u16)I2C_M_IGNORE_NAK) != 0U) {
		dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] I2C_M_IGNORE_NAK flag set\n");
		tcc_i2c->msg_flags.is_ack_ignore = (bool)true;
	} else {
		tcc_i2c->msg_flags.is_ack_ignore = (bool)false;
	}

	if ((flags & (u16)I2C_M_STOP) != 0U) {
		dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] I2C_M_STOP flag set\n");
		tcc_i2c->msg_flags.is_force_stop = (bool)true;
	} else {
		tcc_i2c->msg_flags.is_force_stop = (bool)false;
	}

	if ((flags & (u16)I2C_M_TEN) != 0U) {
		/* Invalid request */
		dev_dbg(tcc_i2c->dev, "[INFO][I2C] I2C_M_TEN flag not support\n");
		ret = -EBADRQC;
	}

	if ((flags & (u16)I2C_M_NOSTART) != 0U) {
		/* Invalid request */
		dev_dbg(tcc_i2c->dev, "[INFO][I2C] I2C_M_NOSTART flag not support\n");
		ret = -EBADRQC;
	}

	if ((flags & (u16)I2C_M_NO_RD_ACK) != 0U) {
		/* Invalid request */
		dev_dbg(tcc_i2c->dev, "[INFO][I2C] I2C_M_NO_RD_ACK flag not support\n");
		ret = -EBADRQC;
	}

	if ((flags & (u16)I2C_M_RECV_LEN) != 0U) {
		/* Invalid request */
		dev_dbg(tcc_i2c->dev, "[INFO][I2C] I2C_M_RECV_LEN flag not support\n");
		ret = -EBADRQC;
	}

	if ((flags & (u16)I2C_M_NO_STOP) != 0U) {
		dev_dbg(tcc_i2c->dev,
			"[INFO][I2C] I2C_M_NO_STOP flag set (Telechips Defined Flag)\n");
		tcc_i2c->msg_flags.is_nostop  = (bool)true;
	} else {
		tcc_i2c->msg_flags.is_nostop  = (bool)false;
	}

	return ret;
}

static void tcc_i2c_set_state(struct tcc_i2c_struct *tcc_i2c)
{
	dev_dbg(tcc_i2c->dev,
		"[DEBUG][I2C] current state: %d\n",
		tcc_i2c->state);

	switch (tcc_i2c->state) {
	case STATE_IDLE:
		tcc_i2c->state = STATE_START;
		break;
	case STATE_START:
	case STATE_TRANSFER:
		if ((tcc_i2c->is_last_msg &&
			tcc_i2c->is_end_data) ||
			(tcc_i2c->result < 0)) {
			/* transfer ended or transfer failure */
			tcc_i2c->state = STATE_FINISH;
		} else if (tcc_i2c->is_end_data) {
		/* complete sending current msg, msgs remains. */
			if (tcc_i2c->burst_mode == (bool)true) {
				tcc_i2c->state = STATE_START;
			} else {
				tcc_i2c->state = STATE_COMPLETE;
			}
		} else {
		/* data left to send in the current msg */
			tcc_i2c->state = STATE_TRANSFER;
		}
		break;
	case STATE_FINISH:
		tcc_i2c->state = STATE_COMPLETE;
		break;
	default:
		dev_err(tcc_i2c->dev,
			"[ERROR][I2C] Invalid operation state: %d\n",
			tcc_i2c->state);
		break;
	}

	dev_dbg(tcc_i2c->dev,
		"[DEBUG][I2C] next state: %d\n",
		tcc_i2c->state);
}

static int32_t tcc_i2c_doxfer(struct tcc_i2c_struct *tcc_i2c)
{
	const struct i2c_msg *msg;

	if (tcc_i2c->msgs == NULL) {
		dev_err(tcc_i2c->dev,
			"[DEBUG][ERROR] i2c message is not set\n");
		tcc_i2c->result = -EINVAL;
		goto out;
	}

	dev_dbg(tcc_i2c->dev,
		"[DEBUG][I2C] I2C_SR : 0x%08x\n",
		i2c_readl(tcc_i2c->base + I2C_SR));
	dev_dbg(tcc_i2c->dev,
		"[DEBUG][I2C] I2C_CMD: 0x%08x\n",
		i2c_readl(tcc_i2c->base + I2C_CMD));

	/* I2C bus arbitration lost check */
	if (tcc_i2c_arbitration_check(tcc_i2c) < 0) {
		dev_err(tcc_i2c->dev,
			"[ERROR][I2C] Arbitration lost, state: %d\n",
			tcc_i2c->state);
		tcc_i2c->result = -EIO;
		goto out;
	}

	if (!tcc_i2c->msg_flags.is_ack_ignore) {
		if (!tcc_i2c_ack_check(tcc_i2c)) {
			if (tcc_i2c->cur_ack_timeout > 0U) {
				/* check ACK repeatedly for recovery */
				tcc_i2c->cur_ack_timeout--;
				tcc_i2c_start(tcc_i2c);
			} else {
				dev_dbg(tcc_i2c->dev,
					"[DEBUG][I2C] No ACK from slave device\n");
				tcc_i2c->result = -EAGAIN;
			}
			goto out;
		}
	}

	/* Save the read data resulting from the previous read command */
	if (tcc_i2c->msg_flags.is_transfer_read) {
		tcc_i2c_read_to_buffer(tcc_i2c);
	}

	if ((tcc_i2c->state == STATE_START) ||
		(tcc_i2c->state == STATE_TRANSFER)) {
		/* Get next message */
		msg = &tcc_i2c->msgs[tcc_i2c->cur_msg_index];

		/* Check if the data to send is the last data */
		if (msg->len <= (tcc_i2c->cur_data_index + 1U)) {
			tcc_i2c->is_last_data = (bool)true;
		} else {
			tcc_i2c->is_last_data = (bool)false;
		}

		/* Check whether the data contained
		 * in the current msg has been transferred
		 */
		if (msg->len <= tcc_i2c->cur_data_index) {
			tcc_i2c->is_end_data = (bool)true;
			tcc_i2c->cur_msg_index++;
			tcc_i2c->cur_data_index = 0;
		} else {
			tcc_i2c->is_end_data = (bool)false;
		}

		/* Check if the msg to send is the last msg */
		if (tcc_i2c->all_msg_cnt == tcc_i2c->cur_msg_index) {
			tcc_i2c->is_last_msg = (bool)true;
		} else {
			tcc_i2c->is_last_msg = (bool)false;
		}
	}

	tcc_i2c_set_state(tcc_i2c);

	switch (tcc_i2c->state) {
	case STATE_START:
		if (tcc_i2c_parse_flag(tcc_i2c) < 0) {
			tcc_i2c->result = -EBADRQC;
			goto out;
		}
		tcc_i2c_start(tcc_i2c);
		break;
	case STATE_TRANSFER:
		if (tcc_i2c->msg_flags.is_transfer_read) {
			tcc_i2c_read(tcc_i2c);
		} else {
			tcc_i2c_write(tcc_i2c);
		}

		if (tcc_i2c->msg_flags.is_force_stop) {
			tcc_i2c_force_stop(tcc_i2c);
		}

		tcc_i2c->cur_data_index++;
		break;
	case STATE_FINISH:
		if (tcc_i2c->result == 0) {
			tcc_i2c->result = tcc_i2c->cur_msg_index;
			dev_dbg(tcc_i2c->dev,
				"[DEBUG][I2C] i2c xfer finish, msgs cnt: %d\n",
				tcc_i2c->result);
		}

		tcc_i2c_stop(tcc_i2c);
		break;
	case STATE_COMPLETE:
		complete(&tcc_i2c->msg_complete);
		break;
	default:
		dev_err(tcc_i2c->dev,
			"[ERROR][I2C] Invalid state: %d\n",
			tcc_i2c->state);
		tcc_i2c->result = -EINVAL;
		break;
	}
out:

	return tcc_i2c->result;
}

static irqreturn_t tcc_i2c_isr(int irq, void *dev_id)
{
	struct tcc_i2c_struct *tcc_i2c = dev_id;
	uint32_t reg_data = 0;
	irqreturn_t ret = IRQ_HANDLED;

	reg_data = i2c_readl(tcc_i2c->base + I2C_SR);

	/* check status register - interrupt flag */
	if ((reg_data & SR_IF) == 0U) {
		ret = IRQ_NONE;
	} else {
		if (tcc_i2c->state == STATE_IDLE) {
			dev_dbg(tcc_i2c->dev,
				"[DEBUG][I2C] i2c-%d is enabled for both main and sub core\n",
				tcc_i2c->core);
		} else {
			/* clear pending interrupt */
			tcc_i2c_clear_pend_irq(tcc_i2c);

			/* if i2c command register complete */
			if ((reg_data & SR_TIP) == 0U) {
				if (tcc_i2c_doxfer(tcc_i2c) < 0) {
					tcc_i2c_force_exit(tcc_i2c);
				}
			} else {
				dev_err(tcc_i2c->dev,
					"[ERROR][I2C] i2c-%d command not complete\n",
					tcc_i2c->core);
				tcc_i2c->result = -EIO;
			}
		}
	}

	return ret;
}


/* tcc_i2c_xfer
 *
 * first port of call from the i2c bus code when an message needs
 * transferring across the i2c bus.
 */
static int32_t tcc_i2c_xfer(
		struct i2c_adapter *adap,
		struct i2c_msg *msgs,
		int32_t num)
{
	int32_t ret = 0;
	unsigned long time_left, timeout_xfer, timeout_jiffies = 0;

	struct tcc_i2c_struct *tcc_i2c =
		(struct tcc_i2c_struct *)adap->algo_data;

	dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] i2c xfer call, msg cnt: %d\n", num);

	timeout_xfer = msecs_to_jiffies(tcc_i2c->xfer_timeout);
	if (jiffies < (ULONG_MAX - timeout_xfer)) {
		timeout_jiffies = jiffies + timeout_xfer;
	} else {
		timeout_jiffies = timeout_xfer;
	}

	while ((tcc_i2c->state != STATE_IDLE) ||
		((i2c_readl(tcc_i2c->base + I2C_SR) & SR_BUSY) != 0U)) {
		if (time_after(jiffies, timeout_jiffies)) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] failed to get i2c controller[%d] state: %d\n",
				tcc_i2c->core, tcc_i2c->state);
			tcc_i2c->result = -ETIMEDOUT;
			goto out;
		}
	}

	if (tcc_i2c->soc_info->id == (u32)TCC805X) {
		if (tcc_i2c->core == 7U) {
			ret = tcc_i2c7_perm_request(tcc_i2c->base, (bool)true);
			if (ret < 0) {
				dev_err(tcc_i2c->dev,
					"[ERROR][I2C] %s, failed to i2c-7 access\n",
					__func__);
				tcc_i2c->result = ret;
				goto out;
			}
		}
	}

	tcc_i2c->result = 0;
	tcc_i2c->msgs = msgs;
	tcc_i2c->all_msg_cnt = num;
	tcc_i2c->cur_msg_index = 0;
	tcc_i2c->cur_data_index = 0;
	tcc_i2c->cur_ack_timeout = tcc_i2c->max_ack_timeout;

	do {
		/* Interrupt flag clear */
		tcc_i2c_clear_pend_irq(tcc_i2c);
		/* Interrupt enable */
		tcc_i2c_intr_enable(tcc_i2c, (bool)true);

		reinit_completion(&tcc_i2c->msg_complete);
		if (tcc_i2c_doxfer(tcc_i2c) < 0) {
			tcc_i2c_force_exit(tcc_i2c);
		}

		/* wait for i2c transfer end */
		time_left = wait_for_completion_timeout(&tcc_i2c->msg_complete,
				msecs_to_jiffies(tcc_i2c->xfer_timeout));

		/* Interrupt disable */
		tcc_i2c_intr_enable(tcc_i2c, (bool)false);

		if (time_left == 0U) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] i2c transfer timeout, addr: %x\n",
				tcc_i2c->msgs[tcc_i2c->cur_msg_index].addr);
			tcc_i2c->result = -ETIMEDOUT;

			i2c_writel(CMD_STOP, tcc_i2c->base + I2C_CMD);
		}

		if (tcc_i2c->result < 0) {
			break;
		}

		if (!tcc_i2c->burst_mode) {
			tcc_i2c->state = STATE_IDLE;
			schedule();
		}
	} while (tcc_i2c->all_msg_cnt != tcc_i2c->cur_msg_index);

	if (tcc_i2c->soc_info->id == (u32)TCC805X) {
		if (tcc_i2c->core == 7U) {
			(void)tcc_i2c7_perm_request(tcc_i2c->base, (bool)false);
		}
	}
	tcc_i2c->state = STATE_IDLE;
	tcc_i2c->msgs = NULL;
	tcc_i2c_clear_pend_irq(tcc_i2c);
out:
	return tcc_i2c->result;
}

static uint32_t tcc_i2c_func(struct i2c_adapter *adap)
{
	return (u32)I2C_FUNC_I2C |
		(u32)I2C_FUNC_PROTOCOL_MANGLING |
		(u32)I2C_FUNC_SMBUS_EMUL;
}

/* i2c bus registration info */
static const struct i2c_algorithm tcc_i2c_algo = {
	.master_xfer	= tcc_i2c_xfer,
	.functionality	= tcc_i2c_func,
};

static uint32_t tcc_i2c_get_pcfg(const struct tcc_i2c_struct *tcc_i2c,
				 uint8_t core)
{
	uint32_t val;
	uint8_t shift;
	const void __iomem *port_reg;

	if (core < (u8)4) {
		port_reg = tcc_i2c->port_cfg + I2C_PORT_CFG0;
	} else if (core < (u8)8) {
		port_reg = tcc_i2c->port_cfg + I2C_PORT_CFG2;
	} else {
		port_reg = tcc_i2c->port_cfg + I2C_PORT_CFG1;
	}

	core %= (u8)4;
	val = readl(port_reg);
	shift = core << (u8)3;
	val >>= shift;

	return val & 0xFFU;
}

static void tcc_i2c_set_pcfg(const struct tcc_i2c_struct *tcc_i2c,
			     uint8_t core,
			     uint32_t port)
{
	uint32_t val;
	uint8_t shift;
	void  __iomem *port_reg;

	if (core < (u8)4) {
		port_reg = tcc_i2c->port_cfg + I2C_PORT_CFG0;
	} else if (core < (u8)8) {
		port_reg = tcc_i2c->port_cfg + I2C_PORT_CFG2;
	} else {
		port_reg = tcc_i2c->port_cfg + I2C_PORT_CFG1;
	}

	core %= (u8)4;
	val = readl(port_reg);
	shift = core << (u8)3;

	if (shift < (u8)(sizeof(val) * (u8)BITS_PER_BYTE)) {
		val &= (~((u32)0xFF << shift));
		val |= (port << shift);
		writel(val, port_reg);
	}
}

/* tcc_i2c_set_port
 * set the port of i2c
 */
static int32_t tcc_i2c_set_port(const struct tcc_i2c_struct *tcc_i2c)
{
	uint32_t port, conflict;
	uint8_t i;
	int32_t ret = 0;

	port = tcc_i2c->port_mux;

	if (port != 0xFFU) {
		tcc_i2c_set_pcfg(tcc_i2c, tcc_i2c->core, port);

		/* clear conflict for each i2c master/slave core,
		 * 0~7: master, 8~11: slave
		 */
		for (i = 0U; i < 12U; i++) {
			if (i == tcc_i2c->core) {
				continue;
			}
			conflict = tcc_i2c_get_pcfg(tcc_i2c, i);
			if (port == conflict) {
				dev_dbg(tcc_i2c->dev,
					"[DEBUG][I2C] i2c port[%d] conflict\n",
					port);
				tcc_i2c_set_pcfg(tcc_i2c, i, 0xFFU);
			}
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

/* tcc_i2c_config
 *
 * Configuration the I2C controller, set the IO lines and frequency
 */
static int32_t tcc_i2c_config(struct tcc_i2c_struct *tcc_i2c)
{
	int32_t ret = 0;

	ret = clk_prepare_enable(tcc_i2c->hclk);
	if (ret < 0) {
		tcc_i2c->hclk = NULL;
		tcc_i2c->pclk = NULL;
		dev_err(tcc_i2c->dev,
			"[ERROR][I2C] can't do i2c_hclk clock enable\n");
	}

	if (ret == 0) {
		ret = clk_prepare_enable(tcc_i2c->pclk);
		if (ret < 0) {
			tcc_i2c->pclk = NULL;
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] can't do i2c_pclk clock enable\n");
		}
	}

	if (ret == 0) {
		ret = clk_set_rate(tcc_i2c->pclk, I2C_PERI_CLK);
		if (ret < 0) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] can't set i2c_pclk\n");
		}
	}

	if (ret == 0) {
		/* set pinctrl default state */
		tcc_i2c->pinctl = devm_pinctrl_get_select(tcc_i2c->dev,
							"default");
		if (IS_ERR(tcc_i2c->pinctl)) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] Failed to get pinctrl (default state)\n");
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		if ((tcc_i2c->soc_info->id == (u32)TCC805X) &&
		    (tcc_i2c->core == 7U)) {
			ret = tcc_i2c7_perm_request(tcc_i2c->base, (bool)true);
			if (ret < 0) {
				dev_err(tcc_i2c->dev,
					"[ERROR][I2C] %s, failed to i2c-7 access\n",
					__func__);
			}
		}
	}

	if (ret == 0) {
		ret = tcc_i2c_set_prescale(tcc_i2c);
		if (ret < 0) {
			dev_err(tcc_i2c->dev, "[ERROR][I2C] Failed to set prescale\n");
		}
	}

	if (ret == 0) {
		ret = tcc_i2c_set_noise_filter(tcc_i2c);
		if (ret < 0) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] %s: failed to set i2c noise filter.\n",
				__func__);
		}
	}

	if (ret == 0) {
		/* set port mux */
		ret = tcc_i2c_set_port(tcc_i2c);
		if (ret < 0) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] %s: failed to set port configuration\n",
				__func__);
		}
	}

	if (ret == 0) {
		/* enable core */
		i2c_writel(CTRL_EN, tcc_i2c->base + I2C_CTRL);

		if ((tcc_i2c->soc_info->id == (u32)TCC805X) &&
		    (tcc_i2c->core == 7U)) {
			(void)tcc_i2c7_perm_request(tcc_i2c->base, (bool)false);
		}
	}

	if (ret < 0) {
		tcc_i2c_clock_disable(tcc_i2c);
	}

	return ret;
}

static int32_t tcc_i2c_parse_dt(struct platform_device *pdev,
		struct tcc_i2c_struct *tcc_i2c)
{
	struct device_node *np = pdev->dev.of_node;
	const struct resource *res;
	int32_t ret = 0, ret1 = 0, ret2 = 0;

	tcc_i2c->soc_info = of_device_get_match_data(&pdev->dev);
	if (tcc_i2c->soc_info == NULL) {
		ret = -EINVAL;
	}

	if (ret == 0) {
		/* get base register */
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		tcc_i2c->base = devm_ioremap_resource(&pdev->dev, res);

		if (IS_ERR(tcc_i2c->base)) {
			ret = (s32)PTR_ERR(tcc_i2c->base);
		}
	}

	if (ret == 0) {
		/* get i2c port config register */
		tcc_i2c->port_cfg = of_iomap(np, 1);

		if (IS_ERR(tcc_i2c->port_cfg)) {
			ret = (s32)PTR_ERR(tcc_i2c->port_cfg);
		}
	}

	if (ret == 0) {
		/* get i2c clock shared core register */
		tcc_i2c->share_core = of_iomap(np, 2);

		if (IS_ERR(tcc_i2c->share_core)) {
			ret = (s32)PTR_ERR(tcc_i2c->share_core);
		}
	}

	if (ret == 0) {
		tcc_i2c->fclk = of_clk_get(np, 2);
		tcc_i2c->hclk = of_clk_get(np, 1);
		tcc_i2c->pclk = of_clk_get(np, 0);

		/* Get SCL Speed */
		ret = of_property_read_u32(np,
					"clock-frequency",
					&tcc_i2c->i2c_clk_rate);
		if (ret < 0) {
			tcc_i2c->i2c_clk_rate = 100000;
		}
		if (tcc_i2c->i2c_clk_rate > 400000U) {
			dev_warn(&pdev->dev, "[WARN][I2C] SCL %dHz is not supported\n",
					tcc_i2c->i2c_clk_rate);
			tcc_i2c->i2c_clk_rate = 400000;
		}

		/* Get Port mux */
		ret = of_property_read_u32(np, "port-mux", &tcc_i2c->port_mux);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"[ERROR][I2C] ch %d - failed to get port-mux\n",
				tcc_i2c->core);
		}
	}

	if (ret == 0) {
		/* Get Pulse Width of I2C */
		ret1 = of_property_read_u32(np,
				"pulse-width-high",
				&tcc_i2c->pwh);
		ret2 = of_property_read_u32(np,
				"pulse-width-low",
				&tcc_i2c->pwl);
		if ((ret1 != 0) || (ret2 != 0) ||
			(tcc_i2c->pwh > I2C_TR1_MAX) ||
			(tcc_i2c->pwl > I2C_TR1_MAX)) {
			tcc_i2c->pwh = 2;
			tcc_i2c->pwl = 3;
		}

		/* Get Noise Filtering Time */
		ret = of_property_read_u32(np,
				"noise_filter",
				&tcc_i2c->noise_filter);
		if (ret != 0) {
			tcc_i2c->noise_filter = 50;
		}
		if (tcc_i2c->noise_filter < 50U) {
			dev_warn(&pdev->dev, "[WARN][I2C] I2C must suppress noise of less than 50ns.\n");
			tcc_i2c->noise_filter = 50;
		}

		/* Transfer wait timeout (ms) */
		ret = of_property_read_u32(np,
				"xfer-timeout",
				&tcc_i2c->xfer_timeout);
		if (ret != 0) {
			tcc_i2c->xfer_timeout = 500;
		}

		/* ack timeout max 1000 times */
		ret = of_property_read_u32(np,
				"ack-timeout",
				&tcc_i2c->max_ack_timeout);
		if (ret != 0) {
			tcc_i2c->max_ack_timeout = 0;
		}
		if (tcc_i2c->max_ack_timeout > 1000U) {
			tcc_i2c->max_ack_timeout = 1000U;
		}

		/*i2c burst mode */
		tcc_i2c->burst_mode = of_property_read_bool(np, "burst-mode");
		ret = 0;
	}

	return ret;
}

static int32_t tcc_i2c_probe(struct platform_device *pdev)
{
	struct tcc_i2c_struct *tcc_i2c;
	int32_t ret = 0, id = 0, i2c_release_state = I2C_NONE_RELEASE;

	if (pdev->dev.of_node == NULL) {
		ret = -EINVAL;
	}

	if (ret == 0) {
		tcc_i2c = devm_kzalloc(&pdev->dev,
					sizeof(struct tcc_i2c_struct),
					GFP_KERNEL);
		if (tcc_i2c == NULL) {
			ret = -ENOMEM;
		}
	}

	if (ret == 0) {
		ret = tcc_i2c_parse_dt(pdev, tcc_i2c);
		if (ret < 0) {
			dev_err(&pdev->dev, "[ERROR][I2C] failed to device tree parsing\n");
		}
	}

	if (ret == 0) {
		ret = platform_get_irq(pdev, 0);
		if (ret < 0) {
			dev_err(&pdev->dev, "[ERROR][I2C] no irq resource\n");
		} else {
			tcc_i2c->irq = (u32)ret;
			ret = 0;
		}
	}

	if (ret == 0) {
		tcc_i2c_clear_pend_irq(tcc_i2c);
		(void)strlcpy(tcc_i2c->adap.name,
				pdev->name,
				sizeof(tcc_i2c->adap.name));
		ret = request_irq(tcc_i2c->irq,
				  tcc_i2c_isr,
				  IRQF_SHARED,
				  tcc_i2c->adap.name,
				  tcc_i2c);

		if (ret < 0) {
			dev_err(&pdev->dev,
				"[ERROR][I2C] Failed to request irq %d\n",
				tcc_i2c->irq);
			i2c_release_state = I2C_IRQ_RELEASE;
		}
	}

	if (ret == 0) {
		id = of_alias_get_id(pdev->dev.of_node, "i2c");
		if ((id >= 0) && (id <= 7)) {
			pdev->id = id;
			tcc_i2c->core = (u8)id;
		} else {
			dev_err(&pdev->dev,
				"[ERROR][I2C] Invalid I2C id: %d\n",
				id);
			ret = -EINVAL;
			i2c_release_state = I2C_IRQ_RELEASE;
		}
	}

	if (ret == 0) {
		tcc_i2c->adap.owner = THIS_MODULE;
		tcc_i2c->adap.algo = &tcc_i2c_algo;
		tcc_i2c->dev = &(pdev->dev);
		dev_info(&pdev->dev,
			"[INFO][I2C] sclk: %dkHz retry: %d, noise filter: %dns, xfer_timeout: %dms, max_ack_timeout: %d, burst_mode: %d\n",
			(tcc_i2c->i2c_clk_rate/1000U),
			tcc_i2c->adap.retries,
			tcc_i2c->noise_filter,
			tcc_i2c->xfer_timeout,
			tcc_i2c->max_ack_timeout,
			tcc_i2c->burst_mode);

		/* set clock & port_mux */
		ret = tcc_i2c_config(tcc_i2c);
		if (ret < 0) {
			dev_err(&pdev->dev, "[ERROR][I2C] I2C config fail\n");
			i2c_release_state = I2C_CORE_RELEASE;
		}
	}

	if (ret == 0) {
		tcc_i2c->adap.algo_data = tcc_i2c;
		tcc_i2c->adap.dev.parent = &pdev->dev;
		tcc_i2c->adap.class = (u32)(I2C_CLASS_HWMON | I2C_CLASS_SPD);
		tcc_i2c->adap.nr = pdev->id;
		tcc_i2c->adap.dev.of_node = pdev->dev.of_node;

		init_completion(&tcc_i2c->msg_complete);
		platform_set_drvdata(pdev, tcc_i2c);

		ret = i2c_add_numbered_adapter(&(tcc_i2c->adap));
		if (ret < 0) {
			dev_err(&pdev->dev,
				"[ERROR][I2C] %s: failed to add bus\n",
				tcc_i2c->adap.name);
			i2c_del_adapter(&tcc_i2c->adap);
			i2c_release_state = I2C_CORE_RELEASE;
		} else {
			tcc_i2c_enable_flag_set(tcc_i2c, (bool)true);
		}
	}

	if (ret == 0) {
		ret = device_create_file(&pdev->dev, &dev_attr_tcci2c_v3);
		if (ret < 0) {
			dev_warn(&pdev->dev, "[WARN][I2C] failed to create i2c device file\n");
			ret = 0;
		}
	}

#if defined(CONFIG_ARCH_TCC805X)
	if (ret == 0) {
		if (tcc_i2c->core == 7U) {
			i2c7_base = tcc_i2c->base;
		}
	}
#endif

	if (ret < 0) {
		switch (i2c_release_state) {
		case I2C_CORE_RELEASE:
			(void)free_irq(tcc_i2c->irq, tcc_i2c);

			/* Disable I2C Core */
			i2c_writel(i2c_readl(tcc_i2c->base + I2C_CTRL) &
					~CTRL_EN,
					tcc_i2c->base + I2C_CTRL);
			tcc_i2c_clock_disable(tcc_i2c);
			tcc_i2c_enable_flag_set(tcc_i2c, (bool)false);
			break;
		case I2C_IRQ_RELEASE:
			(void)free_irq(tcc_i2c->irq, tcc_i2c);
			break;
		default:
			dev_err(&pdev->dev, "[ERROR][I2C] I2C-V3 DriverProbe fail\n");
			break;
		}
	}

	return ret;
}

static int32_t tcc_i2c_remove(struct platform_device *pdev)
{
	struct tcc_i2c_struct *tcc_i2c = platform_get_drvdata(pdev);

	i2c_del_adapter(&tcc_i2c->adap);

	/* I2C clock disable */
	tcc_i2c_clock_disable(tcc_i2c);

	(void)free_irq(tcc_i2c->irq, tcc_i2c);

	platform_set_drvdata(pdev, NULL);
	device_remove_file(&pdev->dev, &dev_attr_tcci2c_v3);

	/* Disable I2C Core */
	i2c_writel(i2c_readl(tcc_i2c->base + I2C_CTRL) & ~CTRL_EN,
				tcc_i2c->base + I2C_CTRL);

	tcc_i2c_enable_flag_set(tcc_i2c, (bool)false);

	dev_info(tcc_i2c->dev,
		"[INFO][I2C] i2c controller[%d] is removed\n",
		tcc_i2c->core);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int32_t tcc_i2c_suspend(struct device *dev)
{
	struct tcc_i2c_struct *tcc_i2c = dev_get_drvdata(dev);

	i2c_mark_adapter_suspended(&tcc_i2c->adap);
	/* Disable I2C Core */
	i2c_writel(i2c_readl(tcc_i2c->base + I2C_CTRL) & ~CTRL_EN,
			tcc_i2c->base + I2C_CTRL);

	tcc_i2c_clock_disable(tcc_i2c);

	return 0;
}

static int32_t tcc_i2c_resume(struct device *dev)
{
	struct tcc_i2c_struct *tcc_i2c = dev_get_drvdata(dev);
	int32_t ret;

	ret = tcc_i2c_config(tcc_i2c);
	i2c_mark_adapter_resumed(&tcc_i2c->adap);

	return ret;
}

static const struct dev_pm_ops tcc_i2c_pm = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(tcc_i2c_suspend, tcc_i2c_resume)
};
#define TCC_I2C_PM (&tcc_i2c_pm)
#else
#define TCC_I2C_PM NULL
#endif

static ssize_t tcc_i2c_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	const struct tcc_i2c_struct *tcc_i2c = dev_get_drvdata(dev);
	uint32_t port_value;
	bool is_busy;
	int32_t ret;

	(void)attr;
	port_value = tcc_i2c_get_pcfg(tcc_i2c, tcc_i2c->core);
	is_busy = ((readl(tcc_i2c->base + I2C_SR) & SR_BUSY) !=
			(u32)0U) ? true : false;

	ret = scnprintf(buf,
			100,
			"[DEBUG][I2C] channel %u, speed %u kHz, PORT: %u, STATE: %d, BUSY: %d\n",
			tcc_i2c->core,
			tcc_i2c->i2c_clk_rate / (u32)1000,
			port_value,
			tcc_i2c->state,
			is_busy);

	if (ret <= 0) {
		ret = 0;
	}

	return ret;
}

static const struct tcc_i2c_soc_info tcc805x_soc_info = {
	.id = TCC805X,
};

static const struct tcc_i2c_soc_info tcc803x_soc_info = {
	.id = TCC803X,
};

static const struct of_device_id tcc_i2c_of_match[] = {
	{ .compatible = "telechips,tcc805x-i2c-v3",
			.data = &tcc805x_soc_info, },
	{ .compatible = "telechips,tcc803x-i2c-v3",
			.data = &tcc803x_soc_info, },
	{},
};

MODULE_DEVICE_TABLE(of, tcc_i2c_of_match);

static struct platform_driver tcc_i2c_driver = {
	.probe                  = tcc_i2c_probe,
	.remove                 = tcc_i2c_remove,
	.driver                 = {
		.name           = "tcc-i2c-v3",
		.pm             = TCC_I2C_PM,
		.of_match_table = of_match_ptr(tcc_i2c_of_match),
	},
};

static int __init i2c_adap_tcc_init(void)
{
	return platform_driver_register(&tcc_i2c_driver);
}

static void __exit i2c_adap_tcc_exit(void)
{
	platform_driver_unregister(&tcc_i2c_driver);
}

subsys_initcall(i2c_adap_tcc_init);
module_exit(i2c_adap_tcc_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips H/W I2C driver");
MODULE_LICENSE("GPL");

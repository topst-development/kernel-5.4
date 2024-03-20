// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _TCA_GMAC_H_
#define _TCA_GMAC_H_

#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/phy.h>
#include <linux/delay.h>
#include <linux/stmmac.h>

#define MAC_MDIO_DATA_RA_SHIFT	(16)
#define MAC_MDIO_DATA_RA_MASK	(0xFFFF << MAC_MDIO_DATA_RA_SHIFT)
#define MAC_MDIO_DATA_RDA_SHIFT	(16)
#define MAC_MDIO_DATA_RDA_MASK	(0x1F << MAC_MDIO_DATA_RA_SHIFT)

struct tcc_dwmac {
	int phy_mode;
	uintptr_t hsio_block;
	uintptr_t gmac_block;
	struct clk *hsio_clk;
	struct clk *gmac_clk;
	struct clk *ptp_clk;
	struct clk *gmac_hclk;
	u32 phy_on;
	u32 phy_rst;
	u32 txclk_i_dly;
	u32 txclk_i_inv;
	u32 txclk_o_dly;
	u32 txclk_o_inv;
	u32 txen_dly;
	u32 txer_dly;
	u32 txd0_dly;
	u32 txd1_dly;
	u32 txd2_dly;
	u32 txd3_dly;
	u32 txd4_dly;
	u32 txd5_dly;
	u32 txd6_dly;
	u32 txd7_dly;
	u32 rxclk_i_dly;
	u32 rxclk_i_inv;
	u32 rxdv_dly;
	u32 rxer_dly;
	u32 rxd0_dly;
	u32 rxd1_dly;
	u32 rxd2_dly;
	u32 rxd3_dly;
	u32 rxd4_dly;
	u32 rxd5_dly;
	u32 rxd6_dly;
	u32 rxd7_dly;
	u32 crs_dly;
	u32 col_dly;

	// Clause 45 register
	unsigned int c45_dev_addr_shift;
	unsigned int c45_dev_addr_mask;
	unsigned int c45_reg_addr_shift;
	unsigned int c45_reg_addr_mask;

	bool use_ecid_mac_addr;
	bool rmii_tx_clk_off;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dbgfs_dir;
#endif
};

void tcc_dwmac_phy_reset(void *priv);

#ifdef CONFIG_DEBUG_FS
void tcc_dwmac_tuning_timing(void *priv);
void tcc_dwmac_debugfs_init(struct tcc_dwmac *gmac);
void tcc_dwmac_debugfs_exit(struct tcc_dwmac *gmac);
#endif
#endif /*_TCA_GMAC_H_*/

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/of_address.h>
#include <linux/of_net.h>
#include <linux/if_ether.h>

#include "dwmac-tcc-ecid.h"

static void tcc_read_ecid(unsigned int ecid[])
{
	void __iomem *pgpio;
	unsigned int i;

	pgpio = ioremap((unsigned int)TCC_PA_GPIO, 0x100000);

	writel(MODE, pgpio + ECID0_OFFSET);
	writel(MODE | CS | ECID_CON_SELECT, pgpio + ECID0_OFFSET);

	for (i = 0; i < 8; i++) {
		writel(MODE | CS | (i << 17) | ECID_CON_SELECT,
		       pgpio + ECID0_OFFSET);
		writel(MODE | CS | (i << 17) | SIGDEV | ECID_CON_SELECT,
		       pgpio + ECID0_OFFSET);
		writel(MODE | CS | (i << 17) | SIGDEV | PRCHG | ECID_CON_SELECT,
		       pgpio + ECID0_OFFSET);
		writel(MODE | CS | (i << 17) | SIGDEV | PRCHG | FSET
			       | ECID_CON_SELECT,
		       pgpio + ECID0_OFFSET);
		writel(MODE | CS | (i << 17) | PRCHG | FSET | ECID_CON_SELECT,
		       pgpio + ECID0_OFFSET);
		writel(MODE | CS | (i << 17) | FSET | ECID_CON_SELECT,
		       pgpio + ECID0_OFFSET);
		writel(MODE | CS | (i << 17) | ECID_CON_SELECT,
		       pgpio + ECID0_OFFSET);
	}

	ecid[0] = readl(pgpio + ECID2_OFFSET); // Low	32 Bit
	ecid[1] = readl(pgpio + ECID3_OFFSET); // High 16 Bit

	writel(0x00000000, pgpio + ECID0_OFFSET); // ECID Closed
}

int tcc_read_mac_addr_from_ecid(const char *stmmac_mac_addr)
{
	unsigned int ecid[2] = {0, 0};
	unsigned int id_bit = 0;
	int ret = 0;
	unsigned char mac_addr[ETH_ALEN];

	memset(mac_addr, 0, sizeof(mac_addr));
	tcc_read_ecid(ecid);

	id_bit = (ecid[0] >> 22) & ECID_MAC_ID_BIT_MASK;
	if (ecid[0] != 0 || ecid[1] != 0) {
		switch (id_bit) {
		case OUI_TCC_2011:
			mac_addr[0] = 0xF4;
			mac_addr[1] = 0x50;
			mac_addr[2] = 0xEB;
			break;
		case OUI_TCC_2016:
			mac_addr[0] = 0x3C;
			mac_addr[1] = 0x7F;
			mac_addr[2] = 0x6F;
			break;
		case OUI_TCC_2013:
			mac_addr[0] = 0x88;
			mac_addr[1] = 0x46;
			mac_addr[2] = 0x2A;
			break;
		case OUI_TCC_2018:
			mac_addr[0] = 0x7C;
			mac_addr[1] = 0x24;
			mac_addr[2] = 0x0C;
			break;
		default:
			break;
		}
		mac_addr[3] = (ecid[1] >> 8) & 0xFF;
		mac_addr[4] = (ecid[1] & 0xFF);
		mac_addr[5] = (ecid[0] >> 24) & 0xFF;
	} else {
		ret = -EINVAL;
	}

	if (stmmac_mac_addr != NULL) {
		memcpy((void *)stmmac_mac_addr, mac_addr, ETH_ALEN);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

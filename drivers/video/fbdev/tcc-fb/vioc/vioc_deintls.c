// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * linux/drivers/video/fbdev/tcc-fb/vioc/vioc_deintls.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP
#include <video/tcc/vioc_deintls.h>

static void __iomem *pDEINTLS_reg;

void __iomem *VIOC_DEINTLS_GetAddress(void)
{
	if (pDEINTLS_reg == NULL) {
		/* Prevent KSC Warning */
		(void)pr_err("[ERR][DEINTLS] %s: address NULL\n", __func__);
	}

	return pDEINTLS_reg;
}

static int __init vioc_deintls_init(void)
{
	struct device_node *dev_np;

	dev_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_deintls");
	if (dev_np == NULL) {
		/* Prevent KSC Warning */
		(void)pr_info("[INF][DEINTLS] disabled\n");
	} else {
		pDEINTLS_reg = (void __iomem *)of_iomap(dev_np,
						((is_VIOC_REMAP != 0U) ? 1 : 0));
		if (pDEINTLS_reg != NULL) {
			/* Prevent KSC Warning */
			(void)pr_info("[INF][DEINTLS] DEINTLS\n");
		}
	}

	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_deintls_init);

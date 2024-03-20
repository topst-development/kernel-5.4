// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2020 Telechips Inc.
 */

#include <linux/clocksource.h>
//#include <linux/of_address.h>
#include <linux/clk/tcc.h>
//#include <linux/clk-provider.h>
//#include <linux/irqflags.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>

#include <soc/tcc/chipinfo.h>
#include <soc/tcc/tcc-sip.h>

#define TCC805X_CKC_DRIVER
#include "clk-tcc805x.h"


#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define MAX_TCC_PLL 5UL
#define MAX_TCC_VPLL 2UL

#define TCC_SUBCATEGORY	__func__

#define TCC_CKC_SMU		(0UL)
#define TCC_CKC_SMU_END		(3UL)
#define TCC_CKC_DDI		(CKC_REG_OFFSET(PERI_DDIB_BASE_OFFSET))
#define	TCC_CKC_DDI_END		(CKC_REG_OFFSET(PERI_DDIB_BASE_OFFSET) + 7UL)
#define TCC_CKC_HSIO		(CKC_REG_OFFSET(PERI_HSIOB_BASE_OFFSET))
#define TCC_CKC_HSIO_END	(CKC_REG_OFFSET(PERI_HSIOB_BASE_OFFSET) + 8UL)
#define TCC_CKC_IO		(CKC_REG_OFFSET(PERI_IOB_BASE_OFFSET))
#define TCC_CKC_IO_END		(CKC_REG_OFFSET(PERI_IOB_BASE_OFFSET) + 57UL)
#define TCC_CKC_CM		(CKC_REG_OFFSET(PERI_CMB_BASE_OFFSET))
#define TCC_CKC_CM_END		(CKC_REG_OFFSET(PERI_CMB_BASE_OFFSET) + 9UL)
#define TCC_CKC_CPU		(CKC_REG_OFFSET(PERI_CPUB_BASE_OFFSET))
#define TCC_CKC_CPU_END		(CKC_REG_OFFSET(PERI_CPUB_BASE_OFFSET) + 2UL)
#define TCC_CKC_EXT		(CKC_REG_OFFSET(PERI_EXT_BASE_OFFSET))
#define TCC_CKC_EXT_END		(CKC_REG_OFFSET(PERI_EXT_BASE_OFFSET) + 6UL)
#define TCC_CKC_HSM		(CKC_REG_OFFSET(PERI_HSMB_BASE_OFFSET))
#define TCC_CKC_HSM_END		(CKC_REG_OFFSET(PERI_HSMB_BASE_OFFSET) + 1UL)

#define VPLL_DIV(x, y)  (((unsigned long)(y) << (unsigned long)8) \
			 | (unsigned long)(x))

struct ckc_backup_sts {
	unsigned long rate;
	unsigned long en;
};

struct ckc_backup_reg {
	struct ckc_backup_sts	pll[MAX_TCC_PLL];
	struct ckc_backup_sts	vpll[MAX_TCC_VPLL];
	struct ckc_backup_sts	bclk[FBUS_MAX];
	struct ckc_backup_sts	peri[PERI_MAX];
	unsigned long	ddi_sub[DDIBUS_MAX];
	unsigned long	io_sub[IOBUS_MAX];
	unsigned long	hsio_sub[HSIOBUS_MAX];
	unsigned long	video_sub[VIDEOBUS_MAX];
};

static struct ckc_backup_reg *ckc_backup = NULL;

#define CLOCK_RESUME_READY	((u32)0x01000000)
#define PMU_USSTATUS(reg)	((reg) + 0x1C)

extern void tcc_ckc_save(unsigned int clk_down);

void tcc_ckc_save(unsigned int clk_down)
{
	struct arm_smccc_res res;
	unsigned long i, j;

	ckc_backup = kzalloc(sizeof(struct ckc_backup_reg), GFP_KERNEL);

	if(ckc_backup == NULL){
		(void)pr_err("[ERR][tcc_clk] Clock driver suspended Failed\n");
		return;
	}

	for (i = 0; i < PERI_MAX; i++) {
		if (((TCC_CKC_SMU_END <= i) && (i < TCC_CKC_DDI))	||
			((TCC_CKC_DDI_END <= i) && (i < TCC_CKC_HSIO))	 ||
			((TCC_CKC_HSIO_END <= i) && (i < TCC_CKC_IO))	 ||
			((TCC_CKC_IO_END <= i) && (i < TCC_CKC_CM))	 ||
			((TCC_CKC_CM_END <= i) && (i < TCC_CKC_CPU))	 ||
			((TCC_CKC_CPU_END <= i) && (i < TCC_CKC_EXT))	 ||
			((TCC_CKC_EXT_END <= i) && (i < TCC_CKC_HSM))) {
				continue;
		}

		arm_smccc_smc(SIP_CLK_GET_PCLKCTRL,
			      i, 0, 0, 0, 0, 0, 0, &res);
		ckc_backup->peri[i].rate = res.a0;
		arm_smccc_smc(SIP_CLK_IS_PERI, i,
			      0, 0, 0, 0, 0, 0, &res);
		ckc_backup->peri[i].en = res.a0;

		if (clk_down != (unsigned int)0) {
			arm_smccc_smc(SIP_CLK_SET_PCLKCTRL, i,
				ckc_backup->peri[i].en,
				XIN_CLK_RATE, 0, 0, 0, 0, &res);
		}
	}

	for (i = 0; i < FBUS_MAX; i++) {
		if ((i == FBUS_MEM) || (i == FBUS_MEM_SUB)
		    || (i == FBUS_MEM_PHY_USER) || (i == FBUS_MEM_PHY_PERI)) {
			continue;
		}

		arm_smccc_smc(SIP_CLK_GET_CLKCTRL, i,
			      0, 0, 0, 0, 0, 0, &res);
		ckc_backup->bclk[i].rate = res.a0;
		arm_smccc_smc(SIP_CLK_IS_CLKCTRL, i, 0,
			      0, 0, 0, 0, 0, &res);
		ckc_backup->bclk[i].en = res.a0;

		 /* Save Clock pwdn attribute */
		if (ckc_backup->bclk[i].en == 1UL) {
			switch (i) {
			/*
			 * case FBUS_VBUS:
			 *	for (j = 0; j < VIDEOBUS_MAX; j++) {
			 *		arm_smccc_smc(
			 *		SIP_CLK_IS_VPUBUS,
			 *		j, 0, 0, 0, 0, 0, 0, &res);
			 *		ckc_backup->video_sub[j] = res.a0;
			 *	}
			 *	break;
			 */
			case FBUS_HSIO:
				for (j = 0; j < HSIOBUS_MAX; j++) {
					arm_smccc_smc(
					SIP_CLK_IS_HSIOBUS,
						j, 0, 0, 0, 0, 0, 0, &res);
					ckc_backup->hsio_sub[j] = res.a0;
				}
				break;
			case FBUS_DDI:
				for (j = 0; j < DDIBUS_MAX; j++) {
					arm_smccc_smc(
					SIP_CLK_IS_DDIBUS,
						j, 0, 0, 0, 0, 0, 0, &res);
					ckc_backup->ddi_sub[j] = res.a0;
				}
				break;
			case FBUS_IO:
				for (j = 0; j < IOBUS_MAX; j++) {
					arm_smccc_smc(
					SIP_CLK_IS_IOBUS,
						j, 0, 0, 0, 0, 0, 0, &res);
					ckc_backup->io_sub[j] = res.a0;
				}
				break;
			default:
				/* Do nothing */
				break;
			}
		}
	}
	/* Save Video BUS PLL */
	for (i = 0; i < MAX_TCC_VPLL; i++) {
		arm_smccc_smc(SIP_CLK_GET_PLL,
			      i + PLL_VIDEO_BASE_ID, 0, 0, 0, 0, 0, 0, &res);
		ckc_backup->vpll[i].rate = res.a0;
	}

	/* Save SMU/PMU PLL */
	for (i = 0; i < MAX_TCC_PLL; i++) {
		arm_smccc_smc(SIP_CLK_GET_PLL, i,
			      0, 0, 0, 0, 0, 0, &res);
		ckc_backup->pll[i].rate = res.a0;
	}
}

extern void tcc_ckc_restore(void);

static void tcc_ckc_init_for_resume(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_CLK_INIT, 0, 0, 0, 0, 0, 0, 0, &res);
}

void tcc_ckc_restore(void)
{
	struct arm_smccc_res res;
	unsigned long i, j;

	arm_smccc_smc(SIP_CLK_SET_CLKCTRL,
		      FBUS_IO, 1UL,
		      XIN_CLK_RATE/2UL, 0, 0, 0, 0, &res);
	arm_smccc_smc(SIP_CLK_SET_CLKCTRL,
		      FBUS_SMU, 1UL,
		      XIN_CLK_RATE/2UL, 0, 0, 0, 0, &res);
	arm_smccc_smc(SIP_CLK_SET_CLKCTRL,
		      FBUS_HSIO, 1UL,
		      XIN_CLK_RATE/2UL, 0, 0, 0, 0, &res);

	/* Restore SMU/PMU PLL */
	for (i = 0; i < MAX_TCC_PLL; i++) {
		arm_smccc_smc(SIP_CLK_SET_PLL,
			      i, ckc_backup->pll[i].rate,
			      3UL, 0, 0, 0, 0, &res);
	}

	/* Restore Video BUS PLL */
	arm_smccc_smc(SIP_CLK_SET_PLL,
		      PLL_VIDEO_BASE_ID, ckc_backup->vpll[0].rate,
		      VPLL_DIV(4, 6), 0, 0, 0, 0, &res);
	arm_smccc_smc(SIP_CLK_SET_PLL,
		      1UL + PLL_VIDEO_BASE_ID, ckc_backup->vpll[1].rate,
		      VPLL_DIV(4, 1), 0, 0, 0, 0, &res);

	for (i = 0; i < FBUS_MAX; i++) {
		if ((i == FBUS_MEM) || (i == FBUS_MEM_SUB)
		    || (i == FBUS_MEM_PHY_USER) || (i == FBUS_MEM_PHY_PERI)) {
			continue;
		}

		arm_smccc_smc(SIP_CLK_SET_CLKCTRL,
			      i, ckc_backup->bclk[i].en,
			      ckc_backup->bclk[i].rate, 0, 0, 0, 0, &res);

		/* Restore Clock pwdn attribute */
		if (ckc_backup->bclk[i].en == 1UL) {
			switch (i) {
		/*
		 *       case FBUS_VBUS:
		 *		for (j = 0; j < VIDEOBUS_MAX; j++) {
		 *			if (ckc_backup->video_sub[j] == 1) {
		 *				arm_smccc_smc(
		 *				SIP_CLK_DISABLE_VPUBUS,
		 *				j, 0, 0, 0, 0, 0, 0, &res);
		 *			} else {
		 *				arm_smccc_smc(
		 *				SIP_CLK_ENABLE_VPUBUS,
		 *				j, 0, 0, 0, 0, 0, 0, &res);
		 *			}
		 *		}
		 *		break;
		 */
			case FBUS_HSIO:
				for (j = 0; j < HSIOBUS_MAX; j++) {

					if (ckc_backup->hsio_sub[j]
					    == 1UL) {
						arm_smccc_smc(
							SIP_CLK_DISABLE_HSIOBUS,
							j, 0, 0, 0, 0, 0, 0,
							&res);
					} else {
						arm_smccc_smc(
							SIP_CLK_ENABLE_HSIOBUS,
							j, 0, 0, 0, 0, 0, 0,
							&res);
					}

				}
				break;
			case FBUS_DDI:
				for (j = 0; j < DDIBUS_MAX; j++) {
					if (ckc_backup->ddi_sub[j]
					    == 1UL) {
						arm_smccc_smc(
							SIP_CLK_DISABLE_DDIBUS,
							j, 0, 0, 0, 0, 0, 0,
							&res);
					} else {
						arm_smccc_smc(
							SIP_CLK_ENABLE_DDIBUS,
							j, 0, 0, 0, 0, 0, 0,
							&res);
					}

				}
				break;
			case FBUS_IO:
				for (j = 0; j < IOBUS_MAX; j++) {
					if (ckc_backup->io_sub[j]
					    == 1UL) {
						arm_smccc_smc(
							SIP_CLK_DISABLE_IOBUS,
							j, 0, 0, 0, 0, 0, 0,
							&res);
					} else {
						arm_smccc_smc(
							SIP_CLK_ENABLE_IOBUS,
							j, 0, 0, 0, 0, 0, 0,
							&res);
					}

				}
				break;
			default:
				/* Do nothing */
				break;
			}
		}
	}

	for (i = 0; i < PERI_MAX; i++) {
		if (((i >= TCC_CKC_SMU_END) && (i < TCC_CKC_DDI)) ||
			((i >= TCC_CKC_DDI_END) && (i < TCC_CKC_HSIO)) ||
			((i >= TCC_CKC_HSIO_END) && (i < TCC_CKC_IO)) ||
			((i >= TCC_CKC_IO_END) && (i < TCC_CKC_CM)) ||
			((i >= TCC_CKC_CM_END) && (i < TCC_CKC_CPU)) ||
			((i >= TCC_CKC_CPU_END) && (i < TCC_CKC_EXT)) ||
			((i >= TCC_CKC_EXT_END) && (i < TCC_CKC_HSM))) {
				continue;
		}
		arm_smccc_smc(SIP_CLK_SET_PCLKCTRL, i,
			      ckc_backup->peri[i].en,
			      ckc_backup->peri[i].rate,
			      0, 0, 0, 0, &res);
	}

	kfree(ckc_backup);
	ckc_backup = NULL;
}

static int tcc_clk_show(struct seq_file *s, void *v)
{
	struct arm_smccc_res res;
	unsigned long rate, enabled;
	char dis_str[] = "(disabled)";
	(void)v;

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_CPU0, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_CPU0, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "CPU(CA72) : %15lu Hz %s\n", rate,
		   (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_CPU1, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_CPU1, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "CPU(CA53) : %15lu Hz %s\n", rate,
		   (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_CMBUS, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_CMBUS, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "CM BUS : %15lu Hz %s\n", rate,
		   (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_CBUS, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_CBUS, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "CPU BUS : %15lu Hz %s\n", rate,
		   (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_MEM, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_MEM, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "MEMBUS Core Clock : %15lu Hz %s\n",
		   rate, (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_MEM_SUB, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_MEM_SUB, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "MEMBUS Sub System Clock : %15lu Hz %s\n", rate,
		   (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_MEM_LPDDR4, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_MEM_LPDDR4, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "LPDDR4 PHY PLL Clock : %15lu Hz %s\n",
		   rate, (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_SMU, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_SMU, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "SMU BUS : %15lu Hz %s\n",
		   rate, (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_IO, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_IO, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "IO BUS : %15lu Hz %s\n", rate,
		   (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_HSIO, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_HSIO, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "HSIO BUS : %15lu Hz %s\n", rate,
		   (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_DDI, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_DDI, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "DISPLAY BUS(DDIBUS) : %15lu Hz %s\n",
		   rate, (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_GPU, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_GPU, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "Graphic 3D BUS : %15lu Hz %s\n",
		   rate, (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_G2D, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_G2D, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "Graphic 2D BUS : %15lu Hz %s\n",
		   rate, (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_VBUS, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_VBUS, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "Video BUS : %15lu Hz %s\n", rate,
		   (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_CODA, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_CODA, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "CODA Clock : %15lu Hz %s\n",
		   rate, (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_CHEVCDEC, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_CHEVCDEC, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "HEVC_DECODER(CCLK) : %15lu Hz %s\n",
		   rate, (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_BHEVCDEC, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_BHEVCDEC, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "HEVC_DECODER(BCLK) : %15lu Hz %s\n",
		   rate, (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_CHEVCENC, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_CHEVCENC, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "HEVC_ENCODER(CCLK) : %15lu Hz %s\n",
		   rate, (enabled == 1UL)?"":dis_str);

	arm_smccc_smc(SIP_CLK_GET_CLKCTRL,
		      FBUS_BHEVCENC, 0, 0, 0, 0, 0, 0, &res);
	rate = res.a0;
	arm_smccc_smc(SIP_CLK_IS_CLKCTRL,
		      FBUS_BHEVCENC, 0, 0, 0, 0, 0, 0, &res);
	enabled = res.a0;
	seq_printf(s, "HEVC_ENCODER(BCLK) : %15lu Hz %s\n",
		   rate, (enabled == 1UL)?"":dis_str);

	return 0;

}

static int tcc_clk_open(struct inode *node, struct file *fp)
{
	(void)node;
	return single_open(fp, tcc_clk_show, NULL);
}

static const struct file_operations proc_tcc_clk_operations = {
	.owner		= THIS_MODULE,
	.open		= tcc_clk_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int tcc_clk_suspend(void) {
	u32 coreid = get_core_identity();

	if (is_main_core(coreid)) {
		tcc_ckc_save(0);
	}

	(void)pr_info("[INFO][tcc_clk][%s] Clock driver suspended.\n",
		TCC_SUBCATEGORY);

	return 0;
}

static void __iomem *pmureg;

static void tcc_clk_resume(void)
{
	u32 coreid = get_core_identity();
	u32 val = 0;

	(void)pr_info("[INFO][tcc_clk][%s] Clock driver restoration...\n",
		TCC_SUBCATEGORY);

	tcc_ckc_init_for_resume();

	/*
	 * TODO: This code block is a workaround to avoid sync issue
	 *       between main/subcore.  It will be removed after clock
	 *       configuration sequence be improved.
	 */
	if (is_main_core(coreid)) {
		tcc_ckc_restore();

		/* Set "clock resume ready" flag */
		val = readl(PMU_USSTATUS(pmureg));
		writel(val | CLOCK_RESUME_READY, PMU_USSTATUS(pmureg));
	} else {
		/* Wait "clock resume ready" be flagged */
		do {
			val = readl(PMU_USSTATUS(pmureg));
		} while ((val & CLOCK_RESUME_READY) == 0);

		/* Clear "clock resume ready" flag */
		writel(val & ~CLOCK_RESUME_READY, PMU_USSTATUS(pmureg));
	}

	(void)pr_info("[INFO][tcc_clk][%s] Clock driver restoration Completed.\n",
		TCC_SUBCATEGORY);
}

static struct syscore_ops tcc_clk_syscore_ops = {
	.suspend	= tcc_clk_suspend,
	.resume		= tcc_clk_resume,
};

static int __init tcc_clk_proc_init(void)
{
	struct proc_dir_entry *proc_entry;
	umode_t mode = 0x124; // (S_IRUSR | S_IRGRP | S_IROTH)
	proc_entry = proc_create("clocks", mode, NULL,
			&proc_tcc_clk_operations);

	if (proc_entry == NULL) {
		(void)pr_err("[ERR][tcc_clk] Create /proc/clocks is failed\n");
	}

	{
		/*
		 * TODO: This code block is a workaround to avoid sync issue
		 *       between main/subcore.  It will be removed after clock
		 *       configuration sequence be improved.
		 */
		pmureg = ioremap(0x14400000, SZ_4K);
	}

	register_syscore_ops(&tcc_clk_syscore_ops);
	return 0;
}

__initcall(tcc_clk_proc_init);

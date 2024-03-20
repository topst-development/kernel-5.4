/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips, Inc.
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
#ifndef VIOC_VIQE_H
#define	VIOC_VIQE_H

#ifndef ON
#define ON 1U
#endif
#ifndef OFF
#define OFF 0U
#endif

#define	NORMAL_MODE 0  // normal mode
#define	DUPLI_MODE  1  // duplicate mode
#define	SKIP_MODE   2  // skip mode

typedef enum {
	VIOC_VIQE_DEINTL_MODE_BYPASS = 0,
	VIOC_VIQE_DEINTL_MODE_2D,
	VIOC_VIQE_DEINTL_MODE_3D,
	VIOC_VIQE_DEINTL_S
} VIOC_VIQE_DEINTL_MODE;

typedef enum {
	VIOC_VIQE_FMT_YUV420 = 0,
	VIOC_VIQE_FMT_YUV422
} VIOC_VIQE_FMT_TYPE;

/*
 * Register offset
 */
#define VCTRL				(0x000U)
#define VSIZE				(0x004U)
#define VTIMEGEN			(0x008U)
#define VINT				(0x00CU)
#define VMISC				(0x010U)
#define DI_BASE0			(0x080U)
#define DI_BASE1			(0x084U)
#define DI_BASE2			(0x088U)
#define DI_BASE3			(0x08CU)
#define DI_SIZE				(0x090U)
#define DI_OFFS				(0x094U)
#define DI_CTRL				(0x098U)
#define DI_BASE0A			(0x0A0U)
#define DI_BASE1A			(0x0A4U)
#define DI_BASE2A			(0x0A8U)
#define DI_BASE3A			(0x0ACU)
#define DI_BASE0B			(0x0B0U)
#define DI_BASE1B			(0x0B4U)
#define DI_BASE2B			(0x0B8U)
#define DI_BASE3B			(0x0BCU)
#define DI_BASE0C			(0x0C0U)
#define DI_BASE1C			(0x0C4U)
#define DI_BASE2C			(0x0C8U)
#define DI_BASE3C			(0x0CCU)
#define DI_CUR_BASE0		(0x0D0U)
#define DI_CUR_BASE1		(0x0D4U)
#define DI_CUR_BASE2		(0x0D8U)
#define DI_CUR_BASE3		(0x0DCU)
#define DI_CUR_WADDR		(0x0E0U)
#define DI_CUR_RADDR		(0x0E4U)
#define DI_DEC0_MISC		(0x100U)
#define DI_DEC0_SIZE		(0x104U)
#define DI_DEC0_STS			(0x108U)
#define DI_DEC0_CTRL		(0x10CU)
#define DI_DEC1_MISC		(0x120U)
#define DI_DEC1_SIZE		(0x124U)
#define DI_DEC1_STS			(0x128U)
#define DI_DEC1_CTRL		(0x12CU)
#define DI_DEC2_MISC		(0x140U)
#define DI_DEC2_SIZE		(0x144U)
#define DI_DEC2_STS			(0x148U)
#define DI_DEC2_CTRL		(0x14CU)
#define DI_COM0_MISC		(0x160U)
#define DI_COM0_NU			(0x164U)
#define DI_COM0_STS			(0x168U)
#define DI_COM0_CTRL		(0x16CU)
#define DI_COM0_AC			(0x170U)
#define DI_CTRL2			(0x280U)
#define DI_ENGINE0			(0x284U)
#define DI_ENGINE1			(0x288U)
#define DI_ENGINE2			(0x28CU)
#define DI_ENGINE3			(0x290U)
#define DI_ENGINE4			(0x294U)
#define PD_THRES0			(0x298U)
#define PD_THRES1			(0x29CU)
#define PD_JUDDER			(0x2A0U)
#define PD_JUDDER_M			(0x2A4U)
#define DI_MISCC			(0x2A8U)
#define DI_STATUS			(0x2ACU)
#define PD_STSTUS			(0x2B0U)
#define DI_REGION0			(0x2B4U)
#define DI_REGION1			(0x2B8U)
#define DI_INT				(0x2BCU)
#define DI_PD_SAW			(0x2E0U)
#define DI_CSIZE			(0x2E4U)
#define DI_FMT				(0x2E8U)

/*
 * VIQE Control Register
 */
#define VCTRL_CGPMD_SHIFT		(26U)
#define VCTRL_CGDND_SHIFT		(25U) // (data sheet error)
#define VCTRL_CGDID_SHIFT		(24U)
#define VCTRL_CFGUPD_SHIFT		(21U)
#define VCTRL_UPD_SHIFT			(20U)
#define VCTRL_FCF_SHIFT			(18U)
#define VCTRL_FCDUF_SHIFT		(17U)
#define VCTRL_FCD_SHIFT			(16U)
#define VCTRL_NHINTPL_SHIFT		(8U)
#define VCTRL_DIEN_SHIFT		(4U)
#define VCTRL_DNEN_SHIFT		(3U)
#define VCTRL_GMEN_SHIFT		(2U)
#define VCTRL_HIEN_SHIFT		(1U)
#define VCTRL_HILUT_SHIFT		(0U)

#define VCTRL_CGPMD_MASK		((u32)0x1U << VCTRL_CGPMD_SHIFT)
#define VCTRL_CGDND_MASK		((u32)0x1U << VCTRL_CGDND_SHIFT)
#define VCTRL_CGDID_MASK		((u32)0x1U << VCTRL_CGDID_SHIFT)
#define VCTRL_CFGUPD_MASK		((u32)0x1U << VCTRL_CFGUPD_SHIFT)
#define VCTRL_UPD_MASK			((u32)0x1U << VCTRL_UPD_SHIFT)
#define VCTRL_FCF_MASK			((u32)0x1U << VCTRL_FCF_SHIFT)
#define VCTRL_FCDUF_MASK		((u32)0x1U << VCTRL_FCDUF_SHIFT)
#define VCTRL_FCD_MASK			((u32)0x1U << VCTRL_FCD_SHIFT)
#define VCTRL_NHINTPL_MASK		((u32)0x1U << VCTRL_NHINTPL_SHIFT)
#define VCTRL_DIEN_MASK			((u32)0x1U << VCTRL_DIEN_SHIFT)
#define VCTRL_DNEN_MASK			((u32)0x1U << VCTRL_DNEN_SHIFT)
#define VCTRL_GMEN_MASK			((u32)0x1U << VCTRL_GMEN_SHIFT)
#define VCTRL_HIEN_MASK			((u32)0x1U << VCTRL_HIEN_SHIFT)
#define VCTRL_HILUT_MASK		((u32)0x1U << VCTRL_HILUT_SHIFT)

/*
 * VIQE Size Register
 */
#define VSIZE_HEIGHT_SHIFT		(16U) // Input Image Height by pixel
#define VSIZE_WIDTH_SHIFT		(0U)  // Input Image Width by pixel

#define VSIZE_HEIGHT_MASK		((u32)0x7FFU << VSIZE_HEIGHT_SHIFT)
#define VSIZE_WIDTH_MASK		((u32)0x7FFU << VSIZE_WIDTH_SHIFT)

/*
 * VIQE Time Generator Register
 */
#define VTIMEGEN_Y2RMD_SHIFT		(9U)
#define VTIMEGEN_Y2REN_SHIFT		(8U)
#define VTIMEGEN_H2H_SHIFT			(0U)

#define VTIMEGEN_Y2RMD_MASK			((u32)0x3U << VTIMEGEN_Y2RMD_SHIFT)
#define VTIMEGEN_Y2REN_MASK			((u32)0x1U << VTIMEGEN_Y2REN_SHIFT)
#define VTIMEGEN_H2H_MASK			((u32)0xFFU << VTIMEGEN_H2H_SHIFT)

/*
 * VIQE Interrupt Register
 */
#define VINT_MPMI_SHIFT			(10U) // SHOULD BE 0
#define VINT_MDNI_SHIFT			(9U)  // SHOULD BE 0
#define VINT_MDII_SHIFT			(8U)  // De-interlacer Interrupt enable
#define VINT_PMI_SHIFT			(2U)  // SHOULD BE 0
#define VINT_DNI_SHIFT			(1U)  // SHOULD BE 0
#define VINT_DII_SHIFT			(0U)  // De-interlacer Interrupt status

#define VINT_MPMI_MASK			((u32)0x1U << VINT_MPMI_SHIFT)
#define VINT_MDNI_MASK			((u32)0x1U << VINT_MDNI_SHIFT)
#define VINT_MDII_MASK			((u32)0x1U << VINT_MDII_SHIFT)
#define VINT_PMI_MASK			((u32)0x1U << VINT_PMI_SHIFT)
#define VINT_DNI_MASK			((u32)0x1U << VINT_DNI_SHIFT)
#define VINT_DII_MASK			((u32)0x1U << VINT_DII_SHIFT)

/*
 * VIQE Miscellaneous Register
 */
#define VMISC_GED_SHIFT		(3U) // Don't generate EOF Signal
#define VMISC_SDDU_SHIFT	(2U) // Don't use Stream Info. for de-interlacer
#define VMISC_TSDU_SHIFT	(1U) // Don't use Size Info.
#define VMISC_GENDU_SHIFT	(0U) // Don't use Global Enable

#define VMISC_GED_MASK				((u32)0x1U << VMISC_GED_SHIFT)
#define VMISC_SDDU_MASK				((u32)0x1U << VMISC_SDDU_SHIFT)
#define VMISC_TSDU_MASK				((u32)0x1U << VMISC_TSDU_SHIFT)
#define VMISC_GENDU_MASK			((u32)0x1U << VMISC_GENDU_SHIFT)

/*
 * De-interlacer Base n Register
 */
#define DI_BASE_BASE_SHIFT		(0U)

#define DI_BASE_BASE_MASK		((u32)0xFFFFFFFFU << DI_BASE_BASE_SHIFT)

/*
 * De-interlacer Size Register
 */
#define DI_SIZE_HEIGHT_SHIFT		(16U)
#define DI_SIZE_WIDTH_SHIFT		(0U)

#define DI_SIZE_HEIGHT_MASK		((u32)0x7FFU<< DI_SIZE_HEIGHT_SHIFT)
#define DI_SIZE_WIDTH_MASK		((u32)0x7FFU << DI_SIZE_WIDTH_SHIFT)

/*
 * De-interlacer Offset Register
 */
#define DI_OFFS_OFFS1_SHIFT		(16U)
#define DI_OFFS_OFFS0_SHIFT		(0U)

#define DI_OFFS_OFFS1_MASK		((u32)0xFFFFU << DI_OFFS_OFFS1_SHIFT)
#define DI_OFFS_OFFS0_MASK		((u32)0xFFFFU << DI_OFFS_OFFS0_SHIFT)

/*
 * De-interlacer Control Register
 */
#define DI_CTRL_H2H_SHIFT		(24U)
#define DI_CTRL_FRD_SHIFT		(18U)
#define DI_CTRL_CFGUPD_SHIFT		(17U)
#define DI_CTRL_EN_SHIFT		(16U)
#define DI_CTRL_FRMNUM_SHIFT		(8U)
#define DI_CTRL_UVINTPL_SHIFT		(2U)
#define DI_CTRL_SDDU_SHIFT		(1U)
#define DI_CTRL_TSDU_SHIFT		(0U)

#define DI_CTRL_H2H_MASK		((u32)0xFFU << DI_CTRL_H2H_SHIFT)
#define DI_CTRL_FRD_MASK		((u32)0x1U << DI_CTRL_FRD_SHIFT)
#define DI_CTRL_CFGUPD_MASK		((u32)0x1U << DI_CTRL_CFGUPD_SHIFT)
#define DI_CTRL_EN_MASK			((u32)0x1U << DI_CTRL_EN_SHIFT)
#define DI_CTRL_FRMNUM_MASK		((u32)0x3U << DI_CTRL_FRMNUM_SHIFT)
#define DI_CTRL_UVINTPL_MASK		((u32)0x1U << DI_CTRL_UVINTPL_SHIFT)
#define DI_CTRL_SDDU_MASK		((u32)0x1U << DI_CTRL_SDDU_SHIFT)
#define DI_CTRL_TSDU_MASK		((u32)0x1U << DI_CTRL_TSDU_SHIFT)

/*
 * De-interlacer Decomp. n MISC. Register
 */
#define DI_DEC_MISC_FMT_SHIFT			(12U)
#define DI_DEC_MISC_DEC_DIV_SHIFT		(8U)
#define DI_DEC_MISC_SF_SHIFT			(3U)
#define DI_DEC_MISC_ECR_SHIFT			(2U)
#define DI_DEC_MISC_FLUSH_SHIFT			(1U)
#define DI_DEC_MISC_DE_SHIFT			(0U)

#define DI_DEC_MISC_FMT_MASK		((u32)0x1FU << DI_DEC_MISC_FMT_SHIFT)
#define DI_DEC_MISC_DEC_DIV_MASK	((u32)0x3U << DI_DEC_MISC_DEC_DIV_SHIFT)
#define DI_DEC_MISC_SF_MASK		((u32)0x1U << DI_DEC_MISC_SF_SHIFT)
#define DI_DEC_MISC_ECR_MASK		((u32)0x1U << DI_DEC_MISC_ECR_SHIFT)
#define DI_DEC_MISC_FLUSH_MASK		((u32)0x1U << DI_DEC_MISC_FLUSH_SHIFT)
#define DI_DEC_MISC_DE_MASK		((u32)0x1U << DI_DEC_MISC_DE_SHIFT)

/*
 * De-interlacer Decomp. n Size Register
 */
#define DI_DEC_SIZE_HEIGHT_SHIFT	(16U)
#define DI_DEC_SIZE_WIDTH_SHIFT		(0U)

#define DI_DEC_SIZE_HEIGHT_MASK		((u32)0x7FFU << DI_DEC_SIZE_HEIGHT_SHIFT)
#define DI_DEC_SIZE_WIDTH_MASK		((u32)0x7FFU << DI_DEC_SIZE_WIDTH_SHIFT)

/*
 * De-interlacer Decomp. n Status Register
 */
#define DI_DEC_STS_DEC_STS_SHIFT				(28U)
#define DI_DEC_STS_DEC_ER_SHIFT					(27U)
#define DI_DEC_STS_HR_ER_SHIFT					(26U)
#define DI_DEC_STS_EOFO_SHIFT					(25U)
#define DI_DEC_STS_EOFI_SHIFT					(24U)
#define DI_DEC_STS_EMPTY_SHIFT					(20U)
#define DI_DEC_STS_FULL_SHIFT					(16U)
#define DI_DEC_STS_WIDTH_DWALIGN_SHIFT			(0U)

#define DI_DEC_STS_DEC_STS_MASK ((u32)0xFU << DI_DEC_STS_DEC_STS_SHIFT)
#define DI_DEC_STS_DEC_ER_MASK  ((u32)0x1U << DI_DEC_STS_DEC_ER_SHIFT)
#define DI_DEC_STS_HR_ER_MASK   ((u32)0x1U << DI_DEC_STS_HR_ER_SHIFT)
#define DI_DEC_STS_EOFO_MASK    ((u32)0x1U << DI_DEC_STS_EOFO_SHIFT)
#define DI_DEC_STS_EOFI_MASK    ((u32)0x1U << DI_DEC_STS_EOFI_SHIFT)
#define DI_DEC_STS_EMPTY_MASK   ((u32)0xFU << DI_DEC_STS_EMPTY_SHIFT)
#define DI_DEC_STS_FULL_MASK    ((u32)0xFU << DI_DEC_STS_FULL_SHIFT)
#define DI_DEC_STS_WIDTH_DWALIGN_MASK \
	((u32)0xFFFF << DI_DEC_STS_WIDTH_DWALIGN_SHIFT)

/*
 * De-intelracer Decomp. n Contrl Register
 */
#define DI_DEC_CTRL_EN_SHIFT			(31U)
#define DI_DEC_CTRL_STS_SHIFT			(16U)
#define DI_DEC_CTRL_HEADER_EN_SHIFT		(9U)
#define DI_DEC_CTRL_ER_CK_SHIFT			(8U)
#define DI_DEC_CTRL_SELECT_SHIFT		(0U)

#define DI_DEC_CTRL_EN_MASK        ((u32)0x1U << DI_DEC_CTRL_EN_SHIFT)
#define DI_DEC_CTRL_STS_MASK       ((u32)0xFFU << DI_DEC_CTRL_STS_SHIFT)
#define DI_DEC_CTRL_HEADER_EN_MASK ((u32)0x1U << DI_DEC_CTRL_HEADER_EN_SHIFT)
#define DI_DEC_CTRL_ER_CK_MASK     ((u32)0x1U << DI_DEC_CTRL_ER_CK_SHIFT)
#define DI_DEC_CTRL_SELECT_MASK    ((u32)0xFFU << DI_DEC_CTRL_SELECT_SHIFT)

/*
 * De-interlacer Comp. 0 MISC. Register
 */
#define DI_COM0_MISC_FMT_SHIFT			(12U)
#define DI_COM0_MISC_ENC_DIV_SHIFT		(8U)
#define DI_COM0_MISC_SF_SHIFT			(3U)
#define DI_COM0_MISC_FLUSH_SHIFT		(1U)
#define DI_COM0_MISC_DE_SHIFT			(0U)

#define DI_COM0_MISC_FMT_MASK     ((u32)0x1FU << DI_COM0_MISC_FMT_SHIFT)
#define DI_COM0_MISC_ENC_DIV_MASK ((u32)0x3U << DI_COM0_MISC_ENC_DIV_SHIFT)
#define DI_COM0_MISC_SF_MASK      ((u32)0x1U << DI_COM0_MISC_SF_SHIFT)
#define DI_COM0_MISC_FLUSH_MASK   ((u32)0x1U << DI_COM0_MISC_FLUSH_SHIFT)
#define DI_COM0_MISC_DE_MASK      ((u32)0x1U << DI_COM0_MISC_DE_SHIFT)

/*
 * De-intelracer Comp. 0 Status Register
 */
#define DI_COM0_STS_EOFO_SHIFT			(25U)
#define DI_COM0_STS_EOFI_SHIFT			(24U)
#define DI_COM0_STS_EMPTY_SHIFT			(20U)
#define DI_COM0_STS_FULL_SHIFT			(16U)

#define DI_COM0_STS_EOFO_MASK			((u32)0x1U << DI_COM0_STS_EOFO_SHIFT)
#define DI_COM0_STS_EOFI_MASK			((u32)0x1U << DI_COM0_STS_EOFI_SHIFT)
#define DI_COM0_STS_EMPTY_MASK			((u32)0xFU << DI_COM0_STS_EMPTY_SHIFT)
#define DI_COM0_STS_FULL_MASK			((u32)0xFU << DI_COM0_STS_FULL_SHIFT)

/*
 * De-intelracer Comp. 0 Control Register
 */
#define DI_COM0_CTRL_EN_SHIFT			(31U)
#define DI_COM0_CTRL_STS_SHIFT			(16U)
#define DI_COM0_CTRL_SELECT_SHIFT		(0U)

#define DI_COM0_CTRL_EN_MASK     ((u32)0x1U << DI_COM0_CTRL_EN_SHIFT)
#define DI_COM0_CTRL_STS_MASK    ((u32)0xFFU << DI_COM0_CTRL_STS_SHIFT)
#define DI_COM0_CTRL_SELECT_MASK ((u32)0xFFU << DI_COM0_CTRL_SELECT_SHIFT)

/*
 * De-interlacer Comp. 0 AC_Length Register
 */
#define DI_COM0_AC_K2_AC_SHIFT			(16U)
#define DI_COM0_AC_K1_AC_SHIFT			(8U)
#define DI_COM0_AC_K0_AC_SHIFT			(0U)

#define DI_COM0_AC_K2_AC_MASK			((u32)0x3FU << DI_COM0_AC_K2_AC_SHIFT)
#define DI_COM0_AC_K1_AC_MASK			((u32)0x3FU << DI_COM0_AC_K1_AC_SHIFT)
#define DI_COM0_AC_K0_AC_MASK			((u32)0x3FU << DI_COM0_AC_K0_AC_SHIFT)

/*
 * De-interlacer Control Register #2
 */
#define DI_CTRL2_CLRIR_SHIFT  (31U)
#define DI_CTRL2_FLSFF_SHIFT  (30U)
#define DI_CTRL2_BYPASS_SHIFT (29U)
#define DI_CTRL2_PDCLFI_SHIFT (15U)
#define DI_CTRL2_PDPF_SHIFT   (11U)
#define DI_CTRL2_PDJUD_SHIFT  (9U)
#define DI_CTRL2_PDEN_SHIFT   (8U)
#define DI_CTRL2_YDM_SHIFT    (7U)
#define DI_CTRL2_GTHSJ_SHIFT  (6U)
#define DI_CTRL2_MRSP_SHIFT   (5U)
#define DI_CTRL2_MRTM_SHIFT   (4U)
#define DI_CTRL2_JT_SHIFT     (2U)
#define DI_CTRL2_JSP_SHIFT    (1U)
#define DI_CTRL2_JS_SHIFT     (0U)

#define DI_CTRL2_CLRIR_MASK  ((u32)0x1U << DI_CTRL2_CLRIR_SHIFT)
#define DI_CTRL2_FLSFF_MASK  ((u32)0x1U << DI_CTRL2_FLSFF_SHIFT)
#define DI_CTRL2_BYPASS_MASK ((u32)0x1U << DI_CTRL2_BYPASS_SHIFT)
#define DI_CTRL2_PDCLFI_MASK ((u32)0x1U << DI_CTRL2_PDCLFI_SHIFT)
#define DI_CTRL2_PDPF_MASK   ((u32)0x1U << DI_CTRL2_PDPF_SHIFT)
#define DI_CTRL2_PDJUD_MASK  ((u32)0x1U << DI_CTRL2_PDJUD_SHIFT)
#define DI_CTRL2_PDEN_MASK   ((u32)0x1U << DI_CTRL2_PDEN_SHIFT)
#define DI_CTRL2_YDM_MASK    ((u32)0x1U << DI_CTRL2_YDM_SHIFT)
#define DI_CTRL2_GTHSJ_MASK  ((u32)0x1U << DI_CTRL2_GTHSJ_SHIFT)
#define DI_CTRL2_MRSP_MASK   ((u32)0x1U << DI_CTRL2_MRSP_SHIFT)
#define DI_CTRL2_MRTM_MASK   ((u32)0x1U << DI_CTRL2_MRTM_SHIFT)
#define DI_CTRL2_JT_MASK     ((u32)0x1U << DI_CTRL2_JT_SHIFT)
#define DI_CTRL2_JSP_MASK    ((u32)0x1U << DI_CTRL2_JSP_SHIFT)
#define DI_CTRL2_JS_MASK     ((u32)0x1U << DI_CTRL2_JS_SHIFT)

/*
 * De-interlacer Engine 0 Register
 */
#define DI_ENGINE0_DMTSADC_SHIFT (24U)
#define DI_ENGINE0_DMTPXLC_SHIFT (16U)
#define DI_ENGINE0_DMTPXL_SHIFT  (8U)
#define DI_ENGINE0_DMTSAD_SHIFT  (0U)

#define DI_ENGINE0_DMTSADC_MASK ((u32)0xFFU << DI_ENGINE0_DMTSADC_SHIFT)
#define DI_ENGINE0_DMTPXLC_MASK ((u32)0xFFU << DI_ENGINE0_DMTPXLC_SHIFT)
#define DI_ENGINE0_DMTPXL_MASK  ((u32)0xFFU << DI_ENGINE0_DMTPXL_SHIFT)
#define DI_ENGINE0_DMTSAD_MASK  ((u32)0xFFU << DI_ENGINE0_DMTSAD_SHIFT)

/*
 * De-interlacer Engine 1 Register
 */
#define DI_ENGINE1_THSJMAX_SHIFT (24U)
#define DI_ENGINE1_THSJMIN_SHIFT (16U)
#define DI_ENGINE1_LRT_SHIFT     (14U)
#define DI_ENGINE1_LRL_SHIFT     (9U)
#define DI_ENGINE1_LRD_SHIFT     (8U)
#define DI_ENGINE1_DIRLT_SHIFT   (4U)
#define DI_ENGINE1_GLD_SHIFT     (3U)
#define DI_ENGINE1_JDH_SHIFT     (2U)

#define DI_ENGINE1_THSJMAX_MASK ((u32)0xFFU << DI_ENGINE1_THSJMAX_SHIFT)
#define DI_ENGINE1_THSJMIN_MASK ((u32)0xFFU << DI_ENGINE1_THSJMIN_SHIFT)
#define DI_ENGINE1_LRT_MASK     ((u32)0x3U << DI_ENGINE1_LRT_SHIFT)
#define DI_ENGINE1_LRL_MASK     ((u32)0x1FU << DI_ENGINE1_LRL_SHIFT)
#define DI_ENGINE1_LRD_MASK     ((u32)0x1U << DI_ENGINE1_LRD_SHIFT)
#define DI_ENGINE1_DIRLT_MASK   ((u32)0xFU << DI_ENGINE1_DIRLT_SHIFT)
#define DI_ENGINE1_GLD_MASK     ((u32)0x1U << DI_ENGINE1_GLD_SHIFT)
#define DI_ENGINE1_JDH_MASK     ((u32)0x1U << DI_ENGINE1_JDH_SHIFT)

/*
 * De-interlacer Engine 2 Register
 */
#define DI_ENGINE2_A_THS_SHIFT  (20U)
#define DI_ENGINE2_E_THS_SHIFT  (8U)
#define DI_ENGINE2_EP2_SHIFT    (7U)
#define DI_ENGINE2_EP1_SHIFT    (6U)
#define DI_ENGINE2_OPPD_SHIFT   (2U)
#define DI_ENGINE2_EARLYD_SHIFT (1U)
#define DI_ENGINE2_ADAPD_SHIFT  (0U)

#define DI_ENGINE2_A_THS_MASK  ((u32)0xFFFU << DI_ENGINE2_A_THS_SHIFT)
#define DI_ENGINE2_E_THS_MASK  ((u32)0xFFFU << DI_ENGINE2_E_THS_SHIFT)
#define DI_ENGINE2_EP2_MASK	   ((u32)0x1U << DI_ENGINE2_EP2_SHIFT)
#define DI_ENGINE2_EP1_MASK	   ((u32)0x1U << DI_ENGINE2_EP1_SHIFT)
#define DI_ENGINE2_OPPD_MASK   ((u32)0x1U << DI_ENGINE2_OPPD_SHIFT)
#define DI_ENGINE2_EARLYD_MASK ((u32)0x1U << DI_ENGINE2_EARLYD_SHIFT)
#define DI_ENGINE2_ADAPD_MASK  ((u32)0x1U << DI_ENGINE2_ADAPD_SHIFT)

/*
 * De-interlacer Engine 3 Register
 */
#define DI_ENGINE3_STTHW_SHIFT (20U)
#define DI_ENGINE3_STTHM_SHIFT (8U)
#define DI_ENGINE3_STTHD_SHIFT (0U)

#define DI_ENGINE3_STTHW_MASK ((u32)0xFFFU << DI_ENGINE3_STTHW_SHIFT)
#define DI_ENGINE3_STTHM_MASK ((u32)0xFFFU << DI_ENGINE3_STTHM_SHIFT)
#define DI_ENGINE3_STTHD_MASK ((u32)0x1U << DI_ENGINE3_STTHD_SHIFT)

/*
 * De-interlacer Engine 4 Register
 */
#define DI_ENGINE4_VARLG_SHIFT  (28U)
#define DI_ENGINE4_VARR1_SHIFT  (24U)
#define DI_ENGINE4_VARR0_SHIFT  (20U)
#define DI_ENGINE4_DYNVAR_SHIFT (16U)
#define DI_ENGINE4_HPFVAR_SHIFT (12U)
#define DI_ENGINE4_HPFFLD_SHIFT (8U)
#define DI_ENGINE4_HPFFRM_SHIFT (4U)
#define DI_ENGINE4_DMDC_SHIFT   (1U)
#define DI_ENGINE4_DMDL_SHIFT   (0U)

#define DI_ENGINE4_VARLG_MASK  ((u32)0xFU << DI_ENGINE4_VARLG_SHIFT)
#define DI_ENGINE4_VARR1_MASK  ((u32)0xFU << DI_ENGINE4_VARR1_SHIFT)
#define DI_ENGINE4_VARR0_MASK  ((u32)0xFU << DI_ENGINE4_VARR0_SHIFT)
#define DI_ENGINE4_DYNVAR_MASK ((u32)0xFU << DI_ENGINE4_DYNVAR_SHIFT)
#define DI_ENGINE4_HPFVAR_MASK ((u32)0xFU << DI_ENGINE4_HPFVAR_SHIFT)
#define DI_ENGINE4_HPFFLD_MASK ((u32)0xFU << DI_ENGINE4_HPFFLD_SHIFT)
#define DI_ENGINE4_HPFFRM_MASK ((u32)0xFU << DI_ENGINE4_HPFFRM_SHIFT)
#define DI_ENGINE4_DMDC_MASK   ((u32)0x1U << DI_ENGINE4_DMDC_SHIFT)
#define DI_ENGINE4_DMDL_MASK   ((u32)0x1U << DI_ENGINE4_DMDL_SHIFT)

/*
 * Pulldown Detector Threshold 0 Register
 */
#define PD_THRES0_ZP_SHIFT      (24U)
#define PD_THRES0_CNTSCO_SHIFT  (20U)
#define PD_THRES0_CO_SHIFT      (16U)
#define PD_THRES0_UVALDIS_SHIFT (15U)
#define PD_THRES0_WEIGHT_SHIFT  (10U)
#define PD_THRES0_CNTS_SHIFT    (0U)

#define PD_THRES0_ZP_MASK      ((u32)0x1U << PD_THRES0_ZP_SHIFT)
#define PD_THRES0_CNTSCO_MASK  ((u32)0xFU << PD_THRES0_CNTSCO_SHIFT)
#define PD_THRES0_CO_MASK      ((u32)0x3U << PD_THRES0_CO_SHIFT)
#define PD_THRES0_UVALDIS_MASK ((u32)0x1U << PD_THRES0_UVALDIS_SHIFT)
#define PD_THRES0_WEIGHT_MASK  ((u32)0x1FU << PD_THRES0_WEIGHT_SHIFT)
#define PD_THRES0_CNTS_MASK    ((u32)0x3FFU << PD_THRES0_CNTS_SHIFT)

/*
 * Pulldown Detector Threshold 1 Register
 */
#define PD_THRES1_THRES2_SHIFT (16U)
#define PD_THRES1_THRES1_SHIFT (8U)
#define PD_THRES1_THRES0_SHIFT (0U)

#define PD_THRES1_THRES2_MASK ((u32)0xFFU << PD_THRES1_THRES2_SHIFT)
#define PD_THRES1_THRES1_MASK ((u32)0xFFU << PD_THRES1_THRES1_SHIFT)
#define PD_THRES1_THRES0_MASK ((u32)0xFFU << PD_THRES1_THRES0_SHIFT)

/*
 * Pulldown Detector Judder Register
 */
#define PD_JUDDER_THSJDMAX_SHIFT (24U)
#define PD_JUDDER_THSJDMIN_SHIFT (16U)
#define PD_JUDDER_HORLINE_SHIFT  (12U)
#define PD_JUDDER_DNLINE_SHIFT   (8U)
#define PD_JUDDER_CNTS_SHIFT     (0U)

#define PD_JUDDER_THSJDMAX_MASK ((u32)0xFFU << PD_JUDDER_THSJDMAX_SHIFT)
#define PD_JUDDER_THSJDMIN_MASK ((u32)0xFFU << PD_JUDDER_THSJDMIN_SHIFT)
#define PD_JUDDER_HORLINE_MASK  ((u32)0xFU << PD_JUDDER_HORLINE_SHIFT)
#define PD_JUDDER_DNLINE_MASK   ((u32)0xFU << PD_JUDDER_DNLINE_SHIFT)
#define PD_JUDDER_CNTS_MASK     ((u32)0xFFU << PD_JUDDER_CNTS_SHIFT)

/*
 * Pulldown Detector Judder Misc. Register
 */
#define PD_JUDDER_M_MULDMT_SHIFT  (17U)
#define PD_JUDDER_M_NOJDS_SHIFT   (16U)
#define PD_JUDDER_M_THSJDS2_SHIFT (8U)
#define PD_JUDDER_M_JDH_SHIFT     (0U)

#define PD_JUDDER_M_MULDMT_MASK  ((u32)0x7FU << PD_JUDDER_M_MULDMT_SHIFT)
#define PD_JUDDER_M_NOJDS_MASK   ((u32)0x1U << PD_JUDDER_M_NOJDS_SHIFT)
#define PD_JUDDER_M_THSJDS2_MASK ((u32)0xFFU << PD_JUDDER_M_THSJDS2_SHIFT)
#define PD_JUDDER_M_JDH_MASK     ((u32)0x1U << PD_JUDDER_M_JDH_SHIFT)

/*
 * De-interlacer Status Register
 */
#define DI_STATUS_POS_Y_SHIFT		(19U) // Current Vertical Position
#define DI_STATUS_POS_X_SHIFT		(8U)  // Current Horizontal Position
#define DI_STATUS_BUSY_SHIFT		(0U)  // De-interlacer Busy

#define DI_STATUS_POS_Y_MASK		((u32)0x7FFU << DI_STATUS_POS_Y_SHIFT)
#define DI_STATUS_POS_X_MASK		((u32)0x7FFU << DI_STATUS_POS_X_SHIFT)
#define DI_STATUS_BUSY_MASK			((u32)0x1U << DI_STATUS_BUSY_SHIFT)

/*
 * Pulldown Detector Status Register
 */
#define PD_STATUS_MPOFF_SHIFT		(8U)
#define PD_STATUS_CO_SHIFT			(4U)
#define PD_STATUS_STATE_SHIFT		(0U)

#define PD_STATUS_MPOFF_MASK		((u32)0x7U << PD_STATUS_MPOFF_SHIFT)
#define PD_STATUS_CO_MASK			((u32)0x3U << PD_STATUS_CO_SHIFT)
#define PD_STATUS_STATE_MASK		((u32)0xFU << PD_STATUS_STATE_SHIFT)

/*
 * De-interlacer Region 0 Register
 */
#define DI_REGION0_EN_SHIFT			(31U)
#define DI_REGION0_XEND_SHIFT		(16U)
#define DI_REGION0_XSTART_SHIFT		(0U)

#define DI_REGION0_EN_MASK			((u32)0x1U << DI_REGION0_EN_SHIFT)
#define DI_REGION0_XEND_MASK		((u32)0x3FFU << DI_REGION0_XEND_SHIFT)
#define DI_REGION0_XSTART_MASK		((u32)0x3FFU << DI_REGION0_XSTART_SHIFT)

/*
 * De-interlacer Region 1 Register
 */
#define DI_REGION1_YEND_SHIFT		(16U)
#define DI_REGION1_YSTART_SHIFT		(0U)

#define DI_REGION1_YEND_MASK		((u32)0x3FFU << DI_REGION1_YEND_SHIFT)
#define DI_REGION1_YSTART_MASK		((u32)0x3FFU << DI_REGION1_YSTART_SHIFT)

/*
 * De-interlacer Interrupt Register
 */
#define DI_INT_MINT_SHIFT		(16U) // Interrupt enable
#define DI_INT_INT_SHIFT		(0U)  // clear Interrupt

#define DI_INT_MINT_MASK		((u32)0x1U << DI_INT_MINT_SHIFT)
#define DI_INT_INT_MASK			((u32)0x1U << DI_INT_INT_SHIFT)

/*
 * De-interlace Pulldown SAW Register
 */
#define DI_PD_SAW_SAWEN_SHIFT		(31U)
#define DI_PD_SAW_SAW_DUR_SHIFT		(16U)
#define DI_PD_SAW_FRM_THR_SHIFT		(8U)
#define DI_PD_SAW_FLD_THR_SHIFT		(0U)

#define DI_PD_SAW_SAWEN_MASK		((u32)0x1U << DI_PD_SAW_SAWEN_SHIFT)
#define DI_PD_SAW_SAW_DUR_MASK		((u32)0xFFU << DI_PD_SAW_SAW_DUR_SHIFT)
#define DI_PD_SAW_FRM_THR_MASK		((u32)0xFFU << DI_PD_SAW_FRM_THR_SHIFT)
#define DI_PD_SAW_FLD_THR_MASK		((u32)0xFFU << DI_PD_SAW_FLD_THR_SHIFT)

/*
 * De-interlacer Core Size Register
 */
#define DI_CSIZE_HEIGHT_SHIFT (16U) // Input Image Width by pixel
#define DI_CSIZE_WIDTH_SHIFT  (0U)  // Input Image Height by pixel

#define DI_CSIZE_HEIGHT_MASK		((u32)0x7FFU << DI_CSIZE_HEIGHT_SHIFT)
#define DI_CSIZE_WIDTH_MASK			((u32)0x7FFU << DI_CSIZE_WIDTH_SHIFT)

/*
 * De-interlacer Format Register
 */
#define DI_FMT_TFCD_SHIFT (31U) // SHOULD BE 0
#define DI_FMT_TSDU_SHIFT (16U) // Don't use Size Info.
#define DI_FMT_F422_SHIFT (0U)  // De-interlacer format selection

#define DI_FMT_TFCD_MASK		((u32)0x1U << DI_FMT_TFCD_SHIFT)
#define DI_FMT_TSDU_MASK		((u32)0x1U << DI_FMT_TSDU_SHIFT)
#define DI_FMT_F422_MASK		((u32)0x1U << DI_FMT_F422_SHIFT)

/* Interface APIs */
extern void VIOC_VIQE_InitDeintlCoreBypass(void __iomem *reg);
extern void VIOC_VIQE_InitDeintlCoreSpatial(void __iomem *reg);
extern void VIOC_VIQE_InitDeintlCoreTemporal(
	void __iomem *reg);
extern void VIOC_VIQE_SetImageSize(void __iomem *reg,
	unsigned int width, unsigned int height);
extern void VIOC_VIQE_SetImageY2RMode(void __iomem *reg,
	unsigned int y2r_mode);
extern void VIOC_VIQE_SetImageY2REnable(void __iomem *reg,
	unsigned int enable);
extern void VIOC_VIQE_SetControlMisc(void __iomem *reg,
	unsigned int no_hor_intpl, unsigned int fmt_conv_disable,
	unsigned int fmt_conv_disable_using_fmt, unsigned int update_disable,
	unsigned int cfgupd, unsigned int h2h);
extern void VIOC_VIQE_SetControlDontUse(void __iomem *reg,
	unsigned int global_en_dont_use, unsigned int top_size_dont_use,
	unsigned int stream_deintl_info_dont_use);
extern void VIOC_VIQE_SetControlClockGate(void __iomem *reg,
	unsigned int deintl_dis, unsigned int d3d_dis, unsigned int pm_dis);
extern void VIOC_VIQE_SetControlEnable(void __iomem *reg,
	unsigned int his_cdf_or_lut_en, unsigned int his_en,
	unsigned int gamut_en, unsigned int denoise3d_en,
	unsigned int deintl_en);
extern void VIOC_VIQE_SetControlMode(void __iomem *reg,
	unsigned int his_cdf_or_lut_en, unsigned int his_en,
	unsigned int gamut_en, unsigned int denoise3d_en,
	unsigned int deintl_en);
extern void VIOC_VIQE_SetControlRegister(void __iomem *reg,
	unsigned int width, unsigned int height, unsigned int fmt);

extern void VIOC_VIQE_SetDeintlBase(void __iomem *reg,
	unsigned int frmnum, unsigned int base0, unsigned int base1,
	unsigned int base2, unsigned int base3);
extern void VIOC_VIQE_SwapDeintlBase(void __iomem *reg, int mode);
extern void VIOC_VIQE_SetDeintlSize(void __iomem *reg,
	unsigned int width, unsigned int height);
extern void VIOC_VIQE_SetDeintlMisc(void __iomem *reg,
	unsigned int uvintpl, unsigned int cfgupd, unsigned int dma_enable,
	unsigned int h2h, unsigned int top_size_dont_use);
extern void VIOC_VIQE_SetDeintlControl(void __iomem *reg,
	unsigned int fmt, unsigned int eof_control_ready,
	unsigned int dec_divisor, unsigned int ac_k0_limit,
	unsigned int ac_k1_limit, unsigned int ac_k2_limit);
extern void VIOC_VIQE_SetDeintlFMT(void __iomem *reg, unsigned int enable);
extern void VIOC_VIQE_SetDeintlMode(void __iomem *reg,
	VIOC_VIQE_DEINTL_MODE mode);
extern void VIOC_VIQE_SetDeintlModeWeave(void __iomem *reg);
extern void VIOC_VIQE_SetDeintlRegion(void __iomem *reg,
	unsigned int region_enable, unsigned int region_idx_x_start,
	unsigned int region_idx_x_end, unsigned int region_idx_y_start,
	unsigned int region_idx_y_end);
extern void VIOC_VIQE_SetDeintlCore(void __iomem *reg,
	unsigned int width, unsigned int height,
	unsigned int fmt, unsigned int bypass,
	unsigned int top_size_dont_use);
extern void VIOC_VIQE_SetDeintlRegister(void __iomem *reg,
	unsigned int fmt, unsigned int top_size_dont_use,
	unsigned int width, unsigned int height,
	VIOC_VIQE_DEINTL_MODE mode, unsigned int base0,
	unsigned int base1, unsigned int base2, unsigned int base3);
extern void VIOC_VIQE_SetDeintlJudderCnt(void __iomem *reg,
	unsigned int count);
extern void VIOC_VIQE_InitDeintlCoreVinMode(void __iomem *reg);
extern void VIOC_VIQE_IgnoreDecError(void __iomem *reg,
	unsigned int sf, unsigned int er_ck, unsigned int hrer_en);
extern void VIOC_VIQE_DUMP(const void __iomem *reg, unsigned int vioc_id);
extern void __iomem *VIOC_VIQE_GetAddress(unsigned int vioc_id);

#endif //__VIOC_VIQE_H__

/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __LINUX_USB_TCC_H
#define __LINUX_USB_TCC_H

#define PHY_CFG0		(0x4)
#define PHY_CFG1		(0x8)

// USB PHY Configuration Register
#define PCFG_TXVRT		(BIT(3) | BIT(2) | BIT(1) | BIT(0))

// HSIO Sub-system USB Host/Device SEL (USB20DH_SEL)
// base address: 0x11DA0128
#define H_SWRST			(BIT(4))
#define H_CLKMSK		(BIT(3))
#define D_SWRST			(BIT(2))
#define D_CLKMSK		(BIT(1))
#define SEL			(BIT(0))

#define MUX_HST_SEL		(H_SWRST | H_CLKMSK)
#define MUX_DEV_SEL		(D_SWRST | D_CLKMSK)

#define ENABLE			(1)
#define DISABLE			(0)

#define ON			(1)
#define OFF			(0)

#define ON_VOLTAGE		(5000000)
#define OFF_VOLTAGE		(1)

#define BIT_MSK(val, mask)	((val) & (mask))
#define BIT_SET(val, mask)	((val) |= (mask))
#define BIT_CLR(val, mask)	((val) &= ~(mask))
#define BIT_CLR_SET(val, clr_mask, set_mask) \
	((val) = (((val) & ~(clr_mask)) | (set_mask)))

struct U20DH_DEV_PHY { // base address: 0x11DA0100
	volatile uint32_t PCFG0;		// 0x00
	volatile uint32_t PCFG1;		// 0x04
	volatile uint32_t PCFG2;		// 0x08
	volatile uint32_t PCFG3;		// 0x0C
	volatile uint32_t PCFG4;		// 0x10
	volatile uint32_t LSTS;			// 0x14
	volatile uint32_t LCFG0;		// 0x18
	volatile uint32_t reserved[3];		// 0x1C ~ 24
	volatile uint32_t MUX_SEL;		// 0x28
	volatile uint32_t VBUSVLD_SEL;		// 0x2C
};

struct U30_PHY { // base address: 0x11D90000
	volatile uint32_t CLKMASK;              // 0x00
	volatile uint32_t SWRESETN;             // 0x04
	volatile uint32_t PWRCTRL;              // 0x08
	volatile uint32_t OVERCRNT_SEL;         // 0x0C
	volatile uint32_t PCFG0;                // 0x10
	volatile uint32_t PCFG1;                // 0x14
	volatile uint32_t PCFG2;                // 0x18
	volatile uint32_t PCFG3;                // 0x1C
	volatile uint32_t PCFG4;                // 0x20
	volatile uint32_t PCFG5;                // 0x24
	volatile uint32_t PCFG6;                // 0x28
	volatile uint32_t PCFG7;                // 0x2C
	volatile uint32_t PCFG8;                // 0x30
	volatile uint32_t PCFG9;                // 0x34
	volatile uint32_t PCFG10;               // 0x38
	volatile uint32_t PCFG11;               // 0x3C
	volatile uint32_t PCFG12;               // 0x40
	volatile uint32_t PCFG13;               // 0x44
	volatile uint32_t PCFG14;               // 0x48
	volatile uint32_t PCFG15;               // 0x4C
	volatile uint32_t PCFG16;               // 0x50
	volatile uint32_t reserved0[10];        // 0x54 ~ 0x78
	volatile uint32_t PINT;                 // 0x7C
	volatile uint32_t LCFG;                 // 0x80
	volatile uint32_t PCR0;                 // 0x84
	volatile uint32_t PCR1;                 // 0x88
	volatile uint32_t PCR2;                 // 0x8C
	volatile uint32_t reserved1[1];         // 0x90
	volatile uint32_t DBG0;                 // 0x94
	volatile uint32_t DBG1;                 // 0x98
	volatile uint32_t reserved2[1];         // 0x9C
	volatile uint32_t FPCFG0;               // 0xA0
	volatile uint32_t FPCFG1;               // 0xA4
	volatile uint32_t FPCFG2;               // 0xA8
	volatile uint32_t FPCFG3;               // 0xAC
	volatile uint32_t FPCFG4;               // 0xB0
	volatile uint32_t FLCFG0;               // 0xB4
};

enum {
	TXVRT,
	CDT,
	TXPPT,
	TP,
	TXRT,
	TXREST,
	TXHSXVT,
	PCFG_MAX
};

struct pcfg_unit {
	char *reg_name;
	uint32_t offset;
	uint32_t mask;
	char str[256];
};

extern bool of_usb_host_tpl_support(struct device_node *np);

#endif /* __LINUX_USB_TCC_H */

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __DWMAC_TCC_ECID_H__
#define __DWMAC_TCC_ECID_H__

#define ECID_MAC_ID_BIT_MASK (0x3) // ecid0 [22:23]

#define HW31		    (0x80000000)
#define HW30		    (0x40000000)
#define HW29		    (0x20000000)
#define HW28		    (0x10000000)
#define HW27		    (0x08000000)
#define HW26		    (0x04000000)
#define HW25		    (0x02000000)
#define HW24		    (0x01000000)
#define HW23		    (0x00800000)
#define HW22		    (0x00400000)
#define HW21		    (0x00200000)
#define HW20		    (0x00100000)
#define HW19		    (0x00080000)
#define HW18		    (0x00040000)
#define HW17		    (0x00020000)
#define HW16		    (0x00010000)
#define HW15		    (0x00008000)
#define HW14		    (0x00004000)
#define HW13		    (0x00002000)
#define HW12		    (0x00001000)
#define HW11		    (0x00000800)
#define HW10		    (0x00000400)
#define HW09		    (0x00000200)
#define HW08		    (0x00000100)
#define HW07		    (0x00000080)
#define HW06		    (0x00000040)
#define HW05		    (0x00000020)
#define HW04		    (0x00000010)
#define HW03		    (0x00000008)
#define HW02		    (0x00000004)
#define HW01		    (0x00000002)
#define HW00		    (0x00000001)
#define HWZERO		    (0x00000000)

#define ECID0_OFFSET		(0x290)
#define ECID1_OFFSET		(0x294)
#define ECID2_OFFSET		(0x298)
#define ECID3_OFFSET		(0x29C)

#define MODE                    HW31
#define CS                      HW30
#define FSET                    HW29
#define PRCHG                   HW27
#define PROG                    HW26
#define SCK                     HW25
#define SDI                     HW24
#define SIGDEV                  HW23
#define A2                      HW19
#define A1                      HW18
#define A0                      HW17

// ECID Physical Address Setting
#if defined(CONFIG_ARCH_TCC897X)
#define TCC_PA_GPIO     0x74200000
#else
#define TCC_PA_GPIO     0x14200000
#endif

#define SELECT_USER0 HWZERO

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X) ||\
	defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC803X)
#define SELECT_USER1 HW15 // Select [16:14] 3'b010
#elif defined(CONFIG_ARCH_TCC802X) || defined(CONFIG_ARCH_TCC897X) ||\
	defined(CONFIG_ARCH_TCC898X)
#define SELECT_USER1 HW16 // Select [16:14] 3'b100
#else
#define SELECT_USER1 HW16
#endif

#if defined(CONFIG_ARCH_TCC880X)
// TCC880X only use ECID USER0 to read mac address
#define ECID_CON_SELECT SELECT_USER0
#else
#define ECID_CON_SELECT SELECT_USER1
#endif

#define OUI_TCC_2011 0 // [23:22] : 2'b00
#define OUI_TCC_2013 2 // [23:22] : 2'b10
#define OUI_TCC_2016 1 // [23:22] : 2'b01
#define OUI_TCC_2018 3 // [23:22] : 2'b11

int tcc_read_mac_addr_from_ecid(const char *stmmac_mac_addr);

#endif /* __DWMAC_TCC_ECID_H__ */

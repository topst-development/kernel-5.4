/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/types.h>
#include <linux/serial_reg.h>

#include "mach/iomap.h"

#define UART_TCC_LSR	0x14

volatile u8 *uart_base;

static void putc(int c)
{
	/*
	 * Now, xmit each character
	 */
	while (!(uart_base[UART_TCC_LSR] & UART_LSR_THRE))
		barrier();
	uart_base[UART_TX] = c;
}

static inline void flush(void)
{
}

static inline void arch_decomp_setup(void)
{
#if defined(CONFIG_DEBUG_TCC_UART0)
	uart_base = (volatile u8 *) TCC_PA_UART0;
#elif defined(CONFIG_DEBUG_TCC_UART1)
	uart_base = (volatile u8 *) TCC_PA_UART1;
#elif defined(CONFIG_DEBUG_TCC_UART2)
	uart_base = (volatile u8 *) TCC_PA_UART2;
#elif defined(CONFIG_DEBUG_TCC_UART3)
	uart_base = (volatile u8 *) TCC_PA_UART3;
#else
	uart_base = 0;
#endif
}

/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef BASIC_OPERATION_H
#define BASIC_OPERATION_H

/* casting operation */
/* coverity[HIS_metric_violation : FALSE] */
static inline unsigned int s32_to_u32(int val)
{
	return (val < 0)
		? (unsigned int)val
		: (unsigned int)val;
}

static inline unsigned char u32_to_u8(unsigned int val)
{
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	return (val > ((unsigned char)255U))
		? (unsigned char)val
		: (unsigned char)val;
}

static inline int u32_to_s32(unsigned int val)
{
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	return (val > INT_MAX)
		? (int)val
		: (int)val;
}

static inline unsigned long s64_to_u64(long val)
{
	return (val < 0)
		? (unsigned long)val
		: (unsigned long)val;
}

/* coverity[HIS_metric_violation : FALSE] */
static inline unsigned int u64_to_u32(unsigned long val)
{
	return (val > UINT_MAX)
		? (unsigned int)val
		: (unsigned int)val;
}

static inline long u64_to_s64(unsigned long val)
{
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	return (val > LONG_MAX)
		? (long)val
		: (long)val;
}

static inline unsigned int ptr_to_u32(void *ptr)
{
	unsigned int ret = 0;

#if defined(CONFIG_ARM64)
	/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
	ret = u64_to_u32((unsigned long)(ptr));
#else
	ret = (unsigned int)(ptr);
#endif//defined(CONFIG_ARM64)

	return ret;
}

/* unary operation */
#define add_u32(a, b) ({				\
	unsigned int ui_a = (a);			\
	unsigned int ui_b = (b);			\
	((UINT_MAX - ui_a) < ui_b)			\
		? ((UINT_MAX - ui_a) + ui_b)		\
		: (ui_a + ui_b);			\
})

#define sub_u32(a, b) ({				\
	unsigned int ui_a = (a);			\
	unsigned int ui_b = (b);			\
	(ui_a < ui_b)					\
		? (ui_a + (UINT_MAX - ui_b))		\
		: (ui_a - ui_b);			\
})

#define mul_u32(a, b) ({				\
	unsigned int ui_a = (a);			\
	unsigned int ui_b = (b);			\
	((ui_b == 0U) || ((UINT_MAX / ui_b) < ui_a))	\
		? (unsigned int)0U 			\
		: (unsigned int)(ui_a * ui_b);		\
})

#define div_u32(a, b) ({				\
	unsigned int ui_a = (a);			\
	unsigned int ui_b = (b);			\
	(ui_a < ui_b)					\
		? (unsigned int)0U			\
		: (unsigned int)(ui_a / ui_b);		\
})

/* shift operation */
static inline unsigned int rshift(unsigned int val, unsigned int shift)
{
	/* coverity[cert_int34_c_violation : FALSE] */
	return (shift >= (8U * sizeof(shift)))		\
		? (unsigned int)0U			\
		: (val >> shift);			\
}

#endif/*BASIC_OPERATION_H*/
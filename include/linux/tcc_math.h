/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * tcc_math.h
 *
 * Copyright (C) 2021 Telechips Inc.
 * Authors:
 *	Jayden Kim
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef TELECHIPS_MATH_API_H
#define TELECHIPS_MATH_API_H

#define tcc_safe_ulong2uint(in) \
({	unsigned long ts_ulin = (in);		\
	unsigned int ts_uout;			\
	const unsigned int ts_umax = UINT_MAX;	\
						\
	if (ts_ulin > ts_umax) {		\
		ts_uout = (unsigned int)ts_umax;\
	} else {				\
		ts_uout = (unsigned int)ts_ulin;\
	}					\
	ts_uout;				})

/* unsigned int to int */
#define tcc_safe_uint2int(in) \
({	unsigned int ts_uin = (in);		\
	int ts_iout;				\
	const unsigned int ts_imax =		\
		(UINT_MAX >> 1); 		\
						\
	if (ts_uin > ts_imax) {			\
		ts_iout = (int)ts_imax;		\
	} else {				\
		ts_iout = (int)ts_uin;		\
	}					\
	ts_iout;				})

/* int to unsigned int */
#define tcc_safe_int2uint(in) \
({	unsigned int ts_uout;			\
	int ts_iin = (in);			\
						\
	ts_uout = (ts_iin < 0) ? 0U : (unsigned int)ts_iin;\
	ts_uout;	})

/* uint64_t to unsigned int */
#define tcc_safe_u642uint(in) \
({	uint64_t ts_uin64 = (in);		\
	unsigned int ts_uout;			\
	const unsigned int ts_umax = UINT_MAX;	\
						\
	ts_uin64 =				\
		(ts_uin64 > ts_umax) ? ts_umax : ts_uin64;\
	ts_uout = (unsigned int)ts_uin64;		\
	ts_uout;					})

/*
 * This macro defines pluse between int
 * tcc_safe_int_pluse(si_a, si_b)
 * ts_iout =  si_a + si_b
 * If si_a + si_b is larger than INT_MAX, the ts_iout is INT_MAX.
 * If si_a + si_b is less than INT_MIN, the ts_iout is INT_MIN.
 */
#define tcc_safe_int_pluse(si_a, si_b)				\
({	int ts_iout;						\
	int ts_ia = (si_a);					\
	int ts_ib = (si_b);					\
	const unsigned int ts_uimax = (UINT_MAX >> 1);		\
	const int ts_imax = (int)ts_uimax;			\
	const int ts_imin = (-ts_imax -1);			\
								\
	if ((ts_ib > 0) && (ts_ia > (ts_imax - ts_ib))) {	\
		ts_iout = ts_imax;				\
	} else if ((ts_ib < 0) && (ts_ia < (ts_imin - ts_ib))) {\
		ts_iout = ts_imin;				\
	} else {						\
		ts_iout = ts_ia + ts_ib;			\
	}							\
	ts_iout;						})


/*
 * This macro defines minus between int
 * tcc_safe_int_minus(si_a, si_b)
 * ts_iout =  si_a - si_b
 * If si_a - si_b is larger than INT_MAX, the ts_iout is INT_MAX.
 * If si_a - si_b is less than INT_MIN, the ts_iout is INT_MIN.
 */
#define tcc_safe_int_minus(si_a, si_b)				\
({	unsigned int ts_iout;					\
	unsigned int ts_ia = (si_a);				\
	unsigned int ts_ib = (si_b);				\
	const unsigned int ts_uimax = (UINT_MAX >> 1);		\
	const int ts_imax = (int)ts_uimax;			\
	const int ts_imin = (-ts_imax -1);			\
								\
	if (ts_ib > 0 && (si_a < (ts_imin + ts_ib))) {		\
		ts_iout = ts_imin;				\
	} else if (ts_ib < 0 && si_a > (ts_imax + ts_ib)) {	\
		ts_iout = ts_imax;				\
	} else {						\
		ts_iout = ts_ia - ts_ib;			\
	}							\
	ts_iout; 						})

#define tcc_safe_uint_pluse(in_a, in_b)		\
({	unsigned int ts_uout;			\
	unsigned int ts_ua = (in_a);		\
	unsigned int ts_ub = (in_b);		\
	const unsigned int ts_umax = UINT_MAX;	\
						\
	ts_uout = ((ts_umax - ts_ua) < ts_ub) ? ts_umax : (ts_ua + ts_ub);\
	ts_uout;							})
/*
 * This macro defines minus between unsigned int
 * tcc_safe_uint_minus(in_a, inb)
 * ts_uout =  in_a - in_b
 * If in_a is less than in_b, the ts_uout is 0.
 */
#define tcc_safe_uint_minus(in_a, in_b)		\
({	unsigned int ts_uout;			\
	unsigned int ts_ua = (in_a);		\
	unsigned int ts_ub = (in_b);		\
						\
	ts_uout = (ts_ua < ts_ub) ? 0U : (ts_ua - ts_ub);\
	ts_uout;				})

#define tcc_safe_uint_mul(in_a, in_b)		\
({	unsigned int ts_uout;			\
	unsigned int ts_ua = (in_a);		\
	unsigned int ts_ub = (in_b);		\
	const unsigned int ts_umax = UINT_MAX;	\
						\
	if ((ts_ua == 0U) || (ts_ub == 0U)) {		\
		ts_uout = 0U;				\
	} else if (ts_ua == 1U) {			\
		ts_uout = ts_ub;			\
	} else if (ts_ub == 1U) {			\
		ts_uout = ts_ua;			\
	} else {					\
		ts_uout = ((ts_umax / ts_ua) < ts_ub) ? \
			  ts_umax : (ts_ua * ts_ub);	\
	}						\
	ts_uout;					})

#define tcc_safe_ulong_mul(in_a, in_b)		\
({	unsigned long ts_ulout;			\
	unsigned long ts_ula = (in_a);		\
	unsigned long ts_ulb = (in_b);		\
	const unsigned long ts_umax = ULONG_MAX;\
						\
	if ((ts_ula == 0UL) || (ts_ulb == 0UL)) {	\
		ts_ulout = 0UL;				\
	} else if (ts_ula == 1UL) {			\
		ts_ulout = ts_ulb;			\
	} else if (ts_ulb == 1UL) {			\
		ts_ulout = ts_ula;			\
	} else {					\
		ts_ulout = ((ts_umax / ts_ula) < ts_ulb) ? \
			  ts_umax : (ts_ula * ts_ulb);	\
	}						\
	ts_ulout;					})

/* This api works only for positive int */
#define tcc_safe_div_round_up_int(in_a, in_b)	\
({									\
		int ts_iout;						\
		const unsigned int ts_uimax = (UINT_MAX >> 1);		\
		const int ts_imax = (int)ts_uimax;			\
									\
		if (((in_a) < 0) || ((in_b) < 0)) {			\
			ts_iout = 0;					\
		} else {						\
			ts_iout = ((ts_imax - (in_a)) < (in_b)) ? 	\
				(ts_imax - (in_b)) : (in_a);		\
			ts_iout = DIV_ROUND_UP((in_a), (in_b));		\
		}							\
		ts_iout;						})

#endif

/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 * FileName   : tcc_type.h
 * Description: TCC VPU h/w block
 */

#ifndef TCC_TYPES_H__
#define TCC_TYPES_H__

/*
 * bit operations
 */
#define Hw37	(1LL << 37)
#define Hw36	(1LL << 36)
#define Hw35	(1LL << 35)
#define Hw34	(1LL << 34)
#define Hw33	(1LL << 33)
#define Hw32	(1LL << 32)
#define Hw31	0x80000000U
#define Hw30	0x40000000U
#define Hw29	0x20000000U
#define Hw28	0x10000000U
#define Hw27	0x08000000U
#define Hw26	0x04000000U
#define Hw25	0x02000000U
#define Hw24	0x01000000U
#define Hw23	0x00800000U
#define Hw22	0x00400000U
#define Hw21	0x00200000U
#define Hw20	0x00100000U
#define Hw19	0x00080000U
#define Hw18	0x00040000U
#define Hw17	0x00020000U
#define Hw16	0x00010000U
#define Hw15	0x00008000U
#define Hw14	0x00004000U
#define Hw13	0x00002000U
#define Hw12	0x00001000U
#define Hw11	0x00000800U
#define Hw10	0x00000400U
#define Hw9	0x00000200U
#define Hw8	0x00000100U
#define Hw7	0x00000080U
#define Hw6	0x00000040U
#define Hw5	0x00000020U
#define Hw4	0x00000010U
#define Hw3	0x00000008U
#define Hw2	0x00000004U
#define Hw1	0x00000002U
#define Hw0	0x00000001U
#define HwZERO	0x00000000U

#ifndef BITSET
#define BITSET(X, MASK)	((X) |= (unsigned int)(MASK))
#endif
#ifndef BITSCLR
#define BITSCLR(X, SMASK, CMASK)	\
	((X) = ((((unsigned int)(X)) | ((unsigned int)(SMASK)))\
	& ~((unsigned int)(CMASK))))
#endif
#ifndef BITCSET
#define BITCSET(X, CMASK, SMASK)	\
	((X) = ((((unsigned int)(X)) & ~((unsigned int)(CMASK)))\
	| ((unsigned int)(SMASK))))
#endif
#ifndef BITCLR
#define BITCLR(X, MASK)	((X) &= ~((unsigned int)(MASK)))
#endif
#ifndef BITXOR
#define BITXOR(X, MASK)	((X) ^= (unsigned int)(MASK))
#endif
#ifndef ISZERO
#define ISZERO(X, MASK)	(!(((unsigned int)(X)) & ((unsigned int)(MASK))))
#endif
#ifndef ISSET
#define	ISSET(X, MASK)	((unsigned long)(X) & ((unsigned long)(MASK)))
#endif

#ifndef ALIGNED_BUFF
#define ALIGNED_BUFF(buf, mul) (((unsigned int)buf + (mul - 1)) & ~(mul - 1))
#endif

/* Enums */
typedef enum {
	TCC_LCDC_IMG_FMT_1BPP,
	TCC_LCDC_IMG_FMT_2BPP,
	TCC_LCDC_IMG_FMT_4BPP,
	TCC_LCDC_IMG_FMT_8BPP,
	TCC_LCDC_IMG_FMT_RGB332 = 8,
	TCC_LCDC_IMG_FMT_RGB444 = 9,
	TCC_LCDC_IMG_FMT_RGB565 = 10,
	TCC_LCDC_IMG_FMT_RGB555 = 11,
	TCC_LCDC_IMG_FMT_RGB888 = 12,
	TCC_LCDC_IMG_FMT_RGB666 = 13,
	TCC_LCDC_IMG_FMT_RGB888_3   = 14,
	TCC_LCDC_IMG_FMT_ARGB6666_3 = 15,
	TCC_LCDC_IMG_FMT_COMP = 16,
	TCC_LCDC_IMG_FMT_DECOMP = (TCC_LCDC_IMG_FMT_COMP),
	TCC_LCDC_IMG_FMT_444SEP = 21,
	TCC_LCDC_IMG_FMT_UYVY = 22,
	TCC_LCDC_IMG_FMT_VYUY = 23,

	TCC_LCDC_IMG_FMT_YUV420SP = 24,
	TCC_LCDC_IMG_FMT_YUV422SP = 25,
	TCC_LCDC_IMG_FMT_YUYV = 26,
	TCC_LCDC_IMG_FMT_YVYU = 27,

	TCC_LCDC_IMG_FMT_YUV420ITL0 = 28,
	TCC_LCDC_IMG_FMT_YUV420ITL1 = 29,
	TCC_LCDC_IMG_FMT_YUV422ITL0 = 30,
	TCC_LCDC_IMG_FMT_YUV422ITL1 = 31,
	TCC_LCDC_IMG_FMT_MAX
} TCC_LCDC_IMG_FMT_TYPE;

/* Display Device */
enum TCC_OUTPUT_TYPE {
	TCC_OUTPUT_NONE,
	TCC_OUTPUT_LCD,
	TCC_OUTPUT_HDMI,
	TCC_OUTPUT_COMPOSITE,
	TCC_OUTPUT_COMPONENT,
	TCC_OUTPUT_DP,
	TCC_OUTPUT_MAX
};

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
enum {
	RDMA_FB1       = 1, //framebuffer 1 driver
	RDMA_3D        = 1, //Layer0
	RDMA_LASTFRM   = 1, //Layer0
	RDMA_FB        = 0, //Layer1
	RDMA_VIDEO     = 2, //Layer2
	RDMA_MOUSE     = 3, //Layer3
	RDMA_VIDEO_3D  = 3, //Layer3
	RDMA_VIDEO_SUB = 3, //Layer3
	RDMA_MAX_NUM
};
#else
enum {
	RDMA_FB        = 0, //Layer0
	RDMA_FB1       = 1, //framebuffer 1 driver
	RDMA_3D        = 1, //Layer1
	RDMA_LASTFRM   = 1, //Layer1
	RDMA_MOUSE     = 2, //Layer2
	RDMA_VIDEO_3D  = 2, //Layer2
	RDMA_VIDEO_SUB = 2, //Layer2
	RDMA_VIDEO     = 3, //Layer3
	RDMA_MAX_NUM
};
#endif


/* COVERITY_CERT_INT30_C */
#if 1
#define check_wrap(x) ({ \
	((x) > MAX_A)    \
		? (x)    \
		: (x);   \
})
#else
#define check_wrap(x)
#endif

#ifndef ADDRESS_ALIGNED
#define ADDRESS_ALIGNED
#define ALIGN_BIT  (0x8U - 1U)
#define BIT_0      (3U)
#define MAX_W      (4096U * 2U)
#define MAX_H      (4096U * 2U)
#define MAX_A      (0xffff0000U)
#define RET_ERR    (0U)

static inline unsigned int GET_ADDR_YUV42X_spY(unsigned int Base_addr)
{
	/* #define GET_ADDR_YUV42X_spY(Base_addr) \
	 * (((((unsigned int)Base_addr) + ALIGN_BIT) >> BIT_0) << BIT_0)
	 */
	unsigned int ret;

	ret = Base_addr + ALIGN_BIT;

	//if (ret < MAX_A) {
		ret = ((ret >> BIT_0) << BIT_0);
	//} else {
	//	ret = ERR_RET;
	//}

	return ret;
}

static inline unsigned int GET_ADDR_YUV42X_spU(unsigned int Yaddr, unsigned int x, unsigned int y)
{
	/* #define GET_ADDR_YUV42X_spU(Yaddr, x, y) \
	 * (((((unsigned int)Yaddr+(x*y)) + ALIGN_BIT) >> BIT_0) << BIT_0)
	 */
	unsigned int ret;

	if ((x < MAX_W) && (y < MAX_H)) {
		ret = x * y;
	} else {
		ret = RET_ERR;
	}

	if (ret != RET_ERR) {
		ret = Yaddr + ret;
		ret = ret + ALIGN_BIT;
		ret = ((ret >> BIT_0) << BIT_0);
	}

	return ret;
}

static inline unsigned int GET_ADDR_YUV420_spV(unsigned int Uaddr, unsigned int x, unsigned int y)
{
	/* #define GET_ADDR_YUV420_spV(Uaddr, x, y) \
	 * (((((unsigned int)Uaddr+(x*y/4)) + ALIGN_BIT) >> BIT_0) << BIT_0)
	 */
	unsigned int ret;

	if ((x < MAX_W) && (y < MAX_H)) {
		ret = x * y;
	} else {
		ret = RET_ERR;
	}

	if (ret != RET_ERR) {
		ret = Uaddr + (ret / 4U);
		ret = ret + ALIGN_BIT;
		ret = ((ret >> BIT_0) << BIT_0);
	}

	return ret;
}

static inline unsigned int GET_ADDR_YUV422_spV(unsigned int Uaddr, unsigned int x, unsigned int y)
{
	/* #define GET_ADDR_YUV422_spV(Uaddr, x, y) \
	 * (((((unsigned int)Uaddr+(x*y/2)) + ALIGN_BIT) >> BIT_0) << BIT_0)
	 */
	unsigned int ret;

	if ((x < MAX_W) && (y < MAX_H)) {
		ret = x * y;
	} else {
		ret = RET_ERR;
	}

	if (ret != RET_ERR) {
		ret = Uaddr + (ret / 2U);
		ret = ret + ALIGN_BIT;
		ret = ((ret >> BIT_0) << BIT_0);
	}

	return ret;
}

static inline void tcc_get_addr_yuv(unsigned int format, unsigned int base_Yaddr,
	unsigned int src_imgx, unsigned int src_imgy,
	unsigned int start_x, unsigned int start_y,
	unsigned int *Y, unsigned int *U, unsigned int *V)
{
	unsigned int Uaddr, Vaddr, Yoffset, UVoffset, start_yPos, start_xPos;
	unsigned int ret = 1U;

	start_yPos = (start_y >> 1U) << 1U;
	start_xPos = (start_x >> 1U) << 1U;

	/*
	 * check: parameters may warp.
	 */
	if ((start_yPos < MAX_W) && (start_xPos < MAX_H) && (src_imgx < MAX_W) && (src_imgy < MAX_H)) {
		/*
		 * Yoffset = (src_imgx * start_yPos) + start_xPos;
		 */
		Yoffset = (src_imgx * start_yPos) + start_xPos;

		if (Yoffset > ((MAX_H * start_yPos) + start_xPos)) {
			ret = RET_ERR;
		}
	} else {
		ret = RET_ERR;
	}

	if (ret != RET_ERR) {
		if ((format >= (unsigned int)TCC_LCDC_IMG_FMT_RGB332) && (format <= (unsigned int)TCC_LCDC_IMG_FMT_ARGB6666_3)) {
			/*
			 * RGB formats
			 */
			unsigned int bpp, tmp;

			if (format == (unsigned int)TCC_LCDC_IMG_FMT_RGB332) {
				bpp = 1U;
			} else if (format <= (unsigned int)TCC_LCDC_IMG_FMT_RGB555) {
				bpp = 2U;
			} else {
				bpp = 4U;
			}

			/*
			 * *Y = base_Yaddr + (Yoffset * bpp);
			 */
			tmp = Yoffset * bpp;
			if ((tmp < MAX_A) && (base_Yaddr < MAX_A)) {
				*Y = base_Yaddr + tmp;
			}

		} else {
			/*
			 * YUV formats
			 */
			if ((format == (unsigned int)TCC_LCDC_IMG_FMT_UYVY)
				|| (format == (unsigned int)TCC_LCDC_IMG_FMT_VYUY)
				|| (format == (unsigned int)TCC_LCDC_IMG_FMT_YUYV)
				|| (format == (unsigned int)TCC_LCDC_IMG_FMT_YVYU)) {

				Yoffset = 2U * Yoffset;
			}

			/*
			 * *Y = base_Yaddr + Yoffset;
			 */
			check_wrap(base_Yaddr);
			check_wrap(Yoffset);
			*Y = base_Yaddr + Yoffset;

			if ((*U == 0U) && (*V == 0U)) {
				Uaddr = GET_ADDR_YUV42X_spU(base_Yaddr, src_imgx, src_imgy);
				if (format == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP) {
					Vaddr = GET_ADDR_YUV420_spV(Uaddr, src_imgx, src_imgy);
				} else {
					Vaddr = GET_ADDR_YUV422_spV(Uaddr, src_imgx, src_imgy);
				}
			} else {
				Uaddr = *U;
				Vaddr = *V;
			}

			if ((format == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP)
				|| (format == (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL0)
				|| (format == (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL1)) {

				if (format == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP) {
					UVoffset = (((src_imgx * start_yPos) / 4U) + (start_xPos / 2U));
				} else {
					UVoffset = (((src_imgx * start_yPos) / 2U) + start_xPos);
				}
			} else {
				if (format == (unsigned int)TCC_LCDC_IMG_FMT_YUV422ITL1) {
					UVoffset = ((src_imgx * start_yPos) + start_xPos);
				} else {
					UVoffset = (((src_imgx * start_yPos) / 2U) + (start_xPos / 2U));
				}
			}

			if ((Uaddr < MAX_A) && (Vaddr < MAX_A) && (UVoffset < MAX_A)) {
				*U = Uaddr + UVoffset;
				*V = Vaddr + UVoffset;
			} else {
				*U = 0;
				*V = 0;
				ret = RET_ERR;
			}

			//pr_debug("### %s Yoffset = [%d]\n", __func__, Yoffset);
		}
	}

	if (ret == RET_ERR) {
		/*
		 * ERROR
		 */
		*Y = 0U;
		*U = 0U;
		*V = 0U;
	}
}
#endif /*ADDRESS_ALIGNED*/

#endif //__TCC_TYPES_H__

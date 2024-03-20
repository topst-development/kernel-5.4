/*
 *******************************************************************************
 *
 *   FileName : tcc_hsm.c
 *
 *   Copyright (c) Telechips Inc.
 *
 *   Description : TCC HSM
 *
 *
 *******************************************************************************
 *
 *   TCC Version 1.0
 *
 *   This source code contains confidential information of Telechips.
 *
 *   Any unauthorized use without a written permission of Telechips including
 *not limited to re-distribution in source or binary form is strictly
 *prohibited.
 *
 *   This source code is provided "AS IS" and nothing contained in this source
 *code shall constitute any express or implied warranty of any kind, including
 *without limitation, any warranty of merchantability, fitness for a
 *particular purpose or non-infringement of any patent, copyright or other
 *third party intellectual property right. No warranty is made, express or
 *implied, regarding the information's accuracy,completeness, or performance.
 *
 *   In no event shall Telechips be liable for any claim, damages or other
 *liability arising from, out of or in connection with this source code or the
 *use in the source code.
 *
 *   This source code is provided subject to the terms of a Mutual
 *Non-Disclosure Agreement between Telechips and Company. This source code is
 *provided "AS IS" and nothing contained in this source code shall constitute
 *any express or implied warranty of any kind, including without limitation, any
 *warranty (of merchantability, fitness for a particular purpose or
 *non-infringement of any patent, copyright or other third party intellectual
 *property right. No warranty is made, express or implied, regarding the
 *information's accuracy, completeness, or performance. In no event shall
 *Telechips be liable for any claim, damages or other liability arising from,
 *out of or in connection with this source code or the use in the source code.
 *This source code is provided subject to the terms of a Mutual Non-Disclosure
 *Agreement between Telechips and Company.
 *
 *******************************************************************************
 */

#if 0
#define NDEBUG
#endif
#define TLOG_LEVEL (TLOG_DEBUG)
#include "tcc_hsm_log.h"

#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/mailbox/tcc_sec_ipc.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include <linux/timer.h>
#include <linux/delay.h>

#include <linux/io.h>
#include <asm/div64.h>

#include <linux/uaccess.h>
#include "tcc_hsm.h"
#include "tcc_hsm_cmd.h"

/****************************************************************************
 * DEFINITiON
 ****************************************************************************/
#define TCC_HSM_DESC_BUF_SIZE (512U)
#define TCC_HSM_DMA_BUF_SIZE (4096U)

#define HSM_REQUIRED_FW_VER_X (0u)
#define HSM_REQUIRED_FW_VER_Y (3u)
#define HSM_REQUIRED_FW_VER_Z (0u)

#define HSM_DRIVER_VER_X (0u)
#define HSM_DRIVER_VER_Y (1u)
#define HSM_DRIVER_VER_Z (0u)

/****************************************************************************
 * DEFINITION OF LOCAL VARIABLES
 ****************************************************************************/
static DEFINE_MUTEX(tcc_hsm_mutex);

static struct tcc_hsm_dma_buf {
	struct device *dev;
	dma_addr_t descPhy;
	uint32_t *descVir;
	dma_addr_t srcPhy;
	uint8_t *srcVir;
	dma_addr_t dstPhy;
	uint8_t *dstVir;
} *dma_buf;

static struct tcc_hsm_ioctl_version_param gVerParam;
/****************************************************************************
 * DEFINITION OF LOCAL FUNCTIONS
 ****************************************************************************/
/* To reduce codesonar warning message */
static uint32_t tcc_copy_from_user(void *param, ulong arg, uint32_t size)
{
	if (copy_from_user((void *)param, (const void *)arg, (ulong)size)
	    != (ulong)0) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	return TCCHSM_SUCCESS;
}

/* To reduce codesonar warning message */
static uint32_t tcc_copy_to_user(ulong arg, void *param, uint32_t size)
{
	if (copy_to_user((void *)arg, (const void *)param, (ulong)size)
	    != (ulong)0) {
		ELOG("copy_to_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	return TCCHSM_SUCCESS;
}

static uint32_t tcc_hsm_ioctl_set_key(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_set_key_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_SET_KEY_FROM_OTP_CMD) {
		req = REQ_HSM_SET_KEY_FROM_OTP;
	} else if (cmd == HSM_SET_KEY_FROM_SNOR_CMD) {
		req = REQ_HSM_SET_KEY_FROM_SNOR;
	} else {
		ELOG("Invalid CMD\n");
		return ret;
	}

	if (tcc_copy_from_user((void *)&param, arg, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	ret = tcc_hsm_cmd_set_key(MBOX_DEV_HSM, req, &param);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_set_key failed\n");
		return ret;
	}

	return ret;
}

static uint32_t tcc_hsm_ioctl_set_modn(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_set_modn_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_SET_MODN_FROM_OTP_CMD) {
		req = REQ_HSM_SET_MODN_FROM_OTP;
	} else if (cmd == HSM_SET_MODN_FROM_SNOR_CMD) {
		req = REQ_HSM_SET_MODN_FROM_SNOR;
	} else {
		ELOG("Invalid CMD\n");
		return ret;
	}

	if (tcc_copy_from_user((void *)&param, arg, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	ret = tcc_hsm_cmd_set_modn(MBOX_DEV_HSM, req, &param);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_set_modn failed\n");
		return ret;
	}

	return ret;
}

static uint32_t tcc_hsm_ioctl_run_aes(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_aes_param param;
	uint32_t *descDMA = NULL;
	uint32_t req = 0;
	ulong user_dst = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_RUN_AES_CMD) {
		req = REQ_HSM_RUN_AES;
	} else if (cmd == HSM_RUN_SM4_CMD) {
		req = REQ_HSM_RUN_SM4;
	} else {
		ELOG("Invalid CMD\n");
		return ret;
	}

	if (tcc_copy_from_user((void *)&param, arg, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	if (param.aad_size > 0u) {
		memset(dma_buf->descVir, 0, TCC_HSM_DESC_BUF_SIZE/4);
		descDMA = dma_buf->descVir;
		descDMA[2] = (uintptr_t)&descDMA[16];
		descDMA[3] = param.aad_size;
		descDMA[10] = (uint32_t)dma_buf->srcPhy;
		descDMA[11] = param.src_size;
		descDMA[14] = (uint32_t)dma_buf->dstPhy;
		descDMA[15] = param.dst_size;
		memcpy((uint8_t *)&descDMA[16], param.aad, param.aad_size);
	}

	if (param.dma == HSM_DMA) {
		ret = tcc_hsm_cmd_run_aes(MBOX_DEV_HSM, req, &param, dma_buf->descPhy);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_aes fail\n");
			return ret;
		}
		if (tcc_copy_to_user(arg, (void *)&param, sizeof(param))
		    != TCCHSM_SUCCESS) {
			ELOG("copy_to_user failed\n");
			return TCCHSM_ERR_INVALID_MEMORY;
		}
	} else {
		if ((param.src_size > TCC_HSM_DMA_BUF_SIZE)
		    || (param.dst_size > TCC_HSM_DMA_BUF_SIZE)) {
			ELOG("Size(src=0x%x,dst=0x%x) should not exceed 0x%x\n",
			     param.src_size, param.dst_size,
			     TCC_HSM_DMA_BUF_SIZE);
			return ret;
		}
		if (tcc_copy_from_user(
			    (void *)dma_buf->srcVir, (ulong)param.src,
			    param.src_size)
		    != TCCHSM_SUCCESS) {
			ELOG("copy_from_user failed\n");
			return TCCHSM_ERR_INVALID_MEMORY;
		}

		user_dst = param.dst;
		param.src = (ulong)dma_buf->srcPhy;
		param.dst = (ulong)dma_buf->dstPhy;
		ret = tcc_hsm_cmd_run_aes(MBOX_DEV_HSM, req, &param, dma_buf->descPhy);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_aes fail(%d)\n", ret);
			return ret;
		}

		dma_sync_single_for_cpu(
			dma_buf->dev, dma_buf->dstPhy, param.dst_size,
			DMA_FROM_DEVICE);

		if (tcc_copy_to_user(arg, (void *)&param, sizeof(param))
		    != TCCHSM_SUCCESS) {
			ELOG("copy_to_user failed\n");
			return TCCHSM_ERR_INVALID_MEMORY;
		}
		if (tcc_copy_to_user(
			    user_dst, (void *)dma_buf->dstVir, param.dst_size)
		    != TCCHSM_SUCCESS) {
			ELOG("copy_to_user failed\n");
			return TCCHSM_ERR_INVALID_MEMORY;
		}
	}

	return ret;
}

static uint32_t tcc_hsm_ioctl_run_aes_by_kt(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_aes_by_kt_param param;
	uint32_t *descDMA = NULL;
	uint32_t req = 0;
	ulong user_dst = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_RUN_AES_BY_KT_CMD) {
		req = REQ_HSM_RUN_AES_BY_KT;
	} else if (cmd == HSM_RUN_SM4_BY_KT_CMD) {
		req = REQ_HSM_RUN_SM4_BY_KT;
	} else {
		ELOG("Invalid CMD\n");
		return ret;
	}

	if (tcc_copy_from_user((void *)&param, arg, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	if (param.aad_size > 0u) {
		memset(dma_buf->descVir, 0, TCC_HSM_DESC_BUF_SIZE/4);
		descDMA = dma_buf->descVir;
		descDMA[2] = (uintptr_t)&descDMA[16];
		descDMA[3] = param.aad_size;
		descDMA[10] = (uint32_t)dma_buf->srcPhy;
		descDMA[11] = param.src_size;
		descDMA[14] = (uint32_t)dma_buf->dstPhy;
		descDMA[15] = param.dst_size;
		memcpy((uint8_t *)&descDMA[16], param.aad, param.aad_size);
	}

	if (param.dma == HSM_DMA) {
		ret = tcc_hsm_cmd_run_aes_by_kt(MBOX_DEV_HSM, req, &param, dma_buf->descPhy);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_aes_by_kt fail(%d)\n", ret);
			return ret;
		}
		if (tcc_copy_to_user(arg, (void *)&param, sizeof(param))
		    != TCCHSM_SUCCESS) {
			ELOG("copy_to_user failed\n");
			return TCCHSM_ERR_INVALID_MEMORY;
		}
	} else {
		if ((param.src_size > TCC_HSM_DMA_BUF_SIZE)
		    || (param.dst_size > TCC_HSM_DMA_BUF_SIZE)) {
			ELOG("Size(src=0x%x,dst=0x%x) should not exceed 0x%x\n",
			     param.src_size, param.dst_size,
			     TCC_HSM_DMA_BUF_SIZE);
			return ret;
		}
		if (tcc_copy_from_user(
			    (void *)dma_buf->srcVir, param.src, param.src_size)
		    != TCCHSM_SUCCESS) {
			ELOG("copy_from_user failed\n");
			return TCCHSM_ERR_INVALID_MEMORY;
		}

		user_dst = param.dst;
		param.src = (ulong)dma_buf->srcPhy;
		param.dst = (ulong)dma_buf->dstPhy;
		ret = tcc_hsm_cmd_run_aes_by_kt(MBOX_DEV_HSM, req, &param, dma_buf->descPhy);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_aes_by_kt fail(%d)\n", ret);
			return ret;
		}

		dma_sync_single_for_cpu(
			dma_buf->dev, dma_buf->dstPhy, param.dst_size,
			DMA_FROM_DEVICE);

		if (tcc_copy_to_user(arg, (void *)&param, sizeof(param))
		    != TCCHSM_SUCCESS) {
			ELOG("copy_to_user failed\n");
			return TCCHSM_ERR_INVALID_MEMORY;
		}
		if (tcc_copy_to_user(
			    user_dst, (void *)dma_buf->dstVir, param.dst_size)
		    != TCCHSM_SUCCESS) {
			ELOG("copy_to_user failed\n");
			return TCCHSM_ERR_INVALID_MEMORY;
		}
	}

	return ret;
}

static uint32_t tcc_hsm_ioctl_gen_mac(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_mac_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_GEN_CMAC_VERIFY_CMD) {
		req = REQ_HSM_VERIFY_CMAC;
	} else if (cmd == HSM_GEN_GMAC_CMD) {
		req = REQ_HSM_GEN_GMAC;
	} else if (cmd == HSM_GEN_HMAC_CMD) {
		req = REQ_HSM_GEN_HMAC;
	} else if (cmd == HSM_GEN_SM3_HMAC_CMD) {
		req = REQ_HSM_GEN_SM3_HMAC;
	} else {
		ELOG("Invalid CMD\n");
		return ret;
	}

	if (tcc_copy_from_user((void *)&param, arg, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	if (param.dma == HSM_DMA) {
		ret = tcc_hsm_cmd_gen_mac(MBOX_DEV_HSM, req, &param);
	} else {
		if (param.src_size > TCC_HSM_DMA_BUF_SIZE) {
			ELOG("The srcSize(0x%x) should not exceed 0x%x bytes\n",
			     param.src_size, TCC_HSM_DMA_BUF_SIZE);
			return ret;
		}

		if (tcc_copy_from_user(
			    (void *)dma_buf->srcVir, param.src, param.src_size)
		    != TCCHSM_SUCCESS) {
			ELOG("copy_from_user failed\n");
			return TCCHSM_ERR_INVALID_MEMORY;
		}
		param.src = (ulong)dma_buf->srcPhy;
		ret = tcc_hsm_cmd_gen_mac(MBOX_DEV_HSM, req, &param);
	}

	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_gen_mac fail(%d)\n", ret);
		return -EFAULT;
	}

	if (tcc_copy_to_user(arg, (void *)&param, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	return ret;
}

static uint32_t tcc_hsm_ioctl_gen_mac_by_kt(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_mac_by_kt_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_GEN_CMAC_VERIFY_BY_KT_CMD) {
		req = REQ_HSM_VERIFY_CMAC_BY_KT;
	} else if (cmd == HSM_GEN_GMAC_BY_KT_CMD) {
		req = REQ_HSM_GEN_GMAC_BY_KT;
	} else if (cmd == HSM_GEN_HMAC_BY_KT_CMD) {
		req = REQ_HSM_GEN_HMAC_BY_KT;
	} else if (cmd == HSM_GEN_SM3_HMAC_BY_KT_CMD) {
		req = REQ_HSM_GEN_SM3_HMAC_BY_KT;
	} else {
		ELOG("Invalid CMD\n");
		return ret;
	}

	if (tcc_copy_from_user((void *)&param, arg, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	if (param.dma == HSM_DMA) {
		ret = tcc_hsm_cmd_gen_mac_by_kt(MBOX_DEV_HSM, req, &param);
	} else {
		if (param.src_size > TCC_HSM_DMA_BUF_SIZE) {
			ELOG("The srcSize(0x%x) should not exceed 0x%x bytes\n",
			     param.src_size, TCC_HSM_DMA_BUF_SIZE);
			return ret;
		}

		if (tcc_copy_from_user(
			    (void *)dma_buf->srcVir, param.src, param.src_size)
		    != TCCHSM_SUCCESS) {
			ELOG("copy_from_user failed\n");
			return TCCHSM_ERR_INVALID_MEMORY;
		}
		param.src = (ulong)dma_buf->srcPhy;
		ret = tcc_hsm_cmd_gen_mac_by_kt(MBOX_DEV_HSM, req, &param);
	}

	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_gen_mac_by_kt fail(%d)\n", ret);
		return ret;
	}

	if (tcc_copy_to_user(arg, (void *)&param, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	return ret;
}

static uint32_t tcc_hsm_ioctl_gen_hash(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_hash_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_GEN_SHA_CMD) {
		req = REQ_HSM_GEN_SHA;
	} else if (cmd == HSM_GEN_SM3_CMD) {
		req = REQ_HSM_GEN_SM3;
	} else {
		ELOG("Invalid CMD\n");
		return ret;
	}

	if (tcc_copy_from_user((void *)&param, arg, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	if (param.dma == HSM_DMA) {
		ret = tcc_hsm_cmd_gen_hash(MBOX_DEV_HSM, req, &param);
	} else {
		if (param.src_size > TCC_HSM_DMA_BUF_SIZE) {
			ELOG("The srcSize(0x%x) should not exceed 0x%x bytes\n",
			     param.src_size, TCC_HSM_DMA_BUF_SIZE);
			return ret;
		}

		if (tcc_copy_from_user(
			    (void *)dma_buf->srcVir, param.src, param.src_size)
		    != TCCHSM_SUCCESS) {
			ELOG("copy_from_user failed\n");
			return TCCHSM_ERR_INVALID_MEMORY;
		}

		param.src = (ulong)dma_buf->srcPhy;
		ret = tcc_hsm_cmd_gen_hash(MBOX_DEV_HSM, req, &param);
	}

	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_run_aes fail(%d)\n", ret);
		return ret;
	}

	if (tcc_copy_to_user(arg, (void *)&param, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	return ret;
}

static uint32_t tcc_hsm_ioctl_run_ecdsa(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_ecdsa_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (tcc_copy_from_user((void *)&param, arg, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	switch (cmd) {
	case HSM_RUN_ECDSA_SIGN_CMD:
		req = REQ_HSM_RUN_ECDSA_SIGN;
		ret = tcc_hsm_cmd_run_ecdsa(MBOX_DEV_HSM, req, &param);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_ecdsa fail(%d)\n", ret);
			break;
		}
		if (tcc_copy_to_user(arg, (void *)&param, sizeof(param))
		    != TCCHSM_SUCCESS) {
			ELOG("copy_to_user failed\n");
			break;
		}
		break;

	case HSM_RUN_ECDSA_VERIFY_CMD:
		req = REQ_HSM_RUN_ECDSA_VERIFY;
		ret = tcc_hsm_cmd_run_ecdsa(MBOX_DEV_HSM, req, &param);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_ecdsa Err(0x%x)\n", ret);
			return ret;
		}
		break;

	default:
		ELOG("Invalid CMD\n");
		break;
	}

	return ret;
}

static uint32_t tcc_hsm_ioctl_run_ecdsa_by_kt(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_ecdsa_by_kt_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (tcc_copy_from_user((void *)&param, arg, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	switch (cmd) {
	case HSM_RUN_ECDSA_SIGN_BY_KT_CMD:
		req = REQ_HSM_RUN_ECDSA_SIGN_BY_KT;
		ret = tcc_hsm_cmd_run_ecdsa_by_kt(MBOX_DEV_HSM, req, &param);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_ecdsa_by_kt fail(%d)\n", ret);
			break;
		}
		if (tcc_copy_to_user(arg, (void *)&param, sizeof(param))
		    != TCCHSM_SUCCESS) {
			ELOG("copy_to_user failed\n");
			break;
		}
		break;

	case HSM_RUN_ECDSA_VERIFY_BY_KT_CMD:
		req = REQ_HSM_RUN_ECDSA_VERIFY_BY_KT;
		ret = tcc_hsm_cmd_run_ecdsa_by_kt(MBOX_DEV_HSM, req, &param);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_ecdsa_by_kt Err(0x%x)\n", ret);
			return ret;
		}
		break;

	default:
		ELOG("Invalid CMD\n");
		break;
	}

	return ret;
}

static uint32_t tcc_hsm_ioctl_run_rsa(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_rsassa_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_RUN_RSASSA_PKCS_SIGN_CMD) {
		req = REQ_HSM_RUN_RSASSA_PKCS_SIGN;
	} else if (cmd == HSM_RUN_RSASSA_PKCS_VERIFY_CMD) {
		req = REQ_HSM_RUN_RSASSA_PKCS_VERIFY;
	} else if (cmd == HSM_RUN_RSASSA_PSS_SIGN_CMD) {
		req = REQ_HSM_RUN_RSASSA_PSS_SIGN;
	} else if (cmd == HSM_RUN_RSASSA_PSS_VERIFY_CMD) {
		req = REQ_HSM_RUN_RSASSA_PSS_VERIFY;
	} else {
		ELOG("Invalid CMD\n");
		return ret;
	}

	if (tcc_copy_from_user((void *)&param, arg, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	if ((param.modN_size > TCC_HSM_RSA_MODN_SIZE)
		|| (param.key_size > TCC_HSM_RSA_MODN_SIZE)
		|| (param.digest_size > TCC_HSM_RSA_DIG_SIZE)
		|| (param.sig_size > TCC_HSM_RSA_SIG_SIZE)) {
		ELOG("Invalid Param\n");
		return TCCHSM_ERR_INVALID_PARAM;
	}

	/* To handle RSA 4096 */
	memcpy(
		(void *)dma_buf->srcVir, (const void *)param.modN,
		param.modN_size);
	memcpy(
		(void *)&dma_buf->srcVir[TCC_HSM_RSA_MODN_SIZE],
		(const void *)param.key, param.key_size);
	memcpy(
		(void *)&dma_buf->srcVir[TCC_HSM_RSA_MODN_SIZE * 2],
		(const void *)param.digest, param.digest_size);

	if ((req == REQ_HSM_RUN_RSASSA_PKCS_VERIFY)
		|| (req == REQ_HSM_RUN_RSASSA_PSS_VERIFY)) {
		memcpy(
			(void *)dma_buf->dstVir, (const void *)param.sig,
			param.sig_size);
	}

	ret = tcc_hsm_cmd_run_rsa(
		MBOX_DEV_HSM, req, &param, dma_buf->srcPhy,
		dma_buf->srcPhy + TCC_HSM_RSA_MODN_SIZE,
		dma_buf->srcPhy + (TCC_HSM_RSA_MODN_SIZE * 2),
		dma_buf->dstPhy);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_run_rsa fail(%d)\n", ret);
		return ret;
	}

	if ((req == REQ_HSM_RUN_RSASSA_PKCS_SIGN)
		|| (req == REQ_HSM_RUN_RSASSA_PSS_SIGN)) {
		dma_sync_single_for_cpu(
			dma_buf->dev, dma_buf->dstPhy, param.sig_size,
			DMA_FROM_DEVICE);

		memcpy(
			(void *)param.sig, (const void *)dma_buf->dstVir,
		    param.sig_size);
	}

	if (tcc_copy_to_user(arg, (void *)&param, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	return ret;
}

static uint32_t tcc_hsm_ioctl_run_rsa_by_kt(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_rsassa_by_kt_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_RUN_RSASSA_PKCS_SIGN_BY_KT_CMD) {
		req = REQ_HSM_RUN_RSASSA_PKCS_SIGN_BY_KT;
	} else if (cmd == HSM_RUN_RSASSA_PKCS_VERIFY_BY_KT_CMD) {
		req = REQ_HSM_RUN_RSASSA_PKCS_VERIFY_BY_KT;
	} else if (cmd == HSM_RUN_RSASSA_PSS_SIGN_BY_KT_CMD) {
		req = REQ_HSM_RUN_RSASSA_PSS_SIGN_BY_KT;
	} else if (cmd == HSM_RUN_RSASSA_PSS_VERIFY_BY_KT_CMD) {
		req = REQ_HSM_RUN_RSASSA_PSS_VERIFY_BY_KT;
	} else {
		ELOG("Invalid CMD\n");
		return ret;
	}

	if (tcc_copy_from_user((void *)&param, arg, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	if ((param.digest_size > TCC_HSM_RSA_DIG_SIZE)
		|| (param.sig_size > TCC_HSM_RSA_SIG_SIZE)) {
		ELOG("Invalid Param\n");
		return TCCHSM_ERR_INVALID_PARAM;
	}

	/* To handle RSA 4096 */
	memcpy(
		(void *)dma_buf->srcVir, (const void *)param.digest,
		param.digest_size);

	if ((req == REQ_HSM_RUN_RSASSA_PKCS_VERIFY_BY_KT)
		|| (req == REQ_HSM_RUN_RSASSA_PSS_VERIFY_BY_KT)) {
		memcpy(
			(void *)dma_buf->dstVir, (const void *)param.sig,
			param.sig_size);
	}

	ret = tcc_hsm_cmd_run_rsa_by_kt(
		MBOX_DEV_HSM, req, &param, dma_buf->srcPhy,
		dma_buf->dstPhy);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_run_rsa fail(%d)\n", ret);
		return ret;
	}


	if ((req == REQ_HSM_RUN_RSASSA_PKCS_SIGN_BY_KT)
		|| (req == REQ_HSM_RUN_RSASSA_PSS_SIGN_BY_KT)) {
		dma_sync_single_for_cpu(
			dma_buf->dev, dma_buf->dstPhy, param.sig_size,
			DMA_FROM_DEVICE);

		memcpy(
			(void *)param.sig, (const void *)dma_buf->dstVir,
		    param.sig_size);
	}

	if (tcc_copy_to_user(arg, (void *)&param, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	return ret;
}

static uint32_t tcc_hsm_ioctl_write(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_write_param param;
	uint32_t req = REQ_HSM_WRITE_OTP;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_WRITE_OTP_CMD) {
		req = REQ_HSM_WRITE_OTP;
	} else if (cmd == HSM_WRITE_SNOR_CMD) {
		req = REQ_HSM_WRITE_SNOR;
	} else {
		ELOG("Invalid CMD\n");
		return ret;
	}

	if (tcc_copy_from_user((void *)&param, arg, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	if (param.data_size > TCC_HSM_DMA_BUF_SIZE) {
		ELOG("The data_size(0x%x) should not exceed 0x%x bytes\n",
		     param.data_size, TCC_HSM_DMA_BUF_SIZE);
		return ret;
	}

	if (tcc_copy_from_user(
		    (void *)dma_buf->srcVir, param.data, param.data_size)
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	param.data = (ulong)dma_buf->srcVir;
	ret = tcc_hsm_cmd_write(MBOX_DEV_HSM, req, &param);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_write failed\n");
		return ret;
	}

	return ret;
}

static uint32_t tcc_hsm_ioctl_get_fw_version(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_version_param param;
	uint32_t req = REQ_HSM_GET_VER;
	uint32_t ret = TCCHSM_ERR;

	ret = tcc_hsm_cmd_get_version(MBOX_DEV_HSM, req, &param);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("failed to get version\n");
		return ret;
	}

	if (tcc_copy_to_user(arg, (void *)&param, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	return ret;
}

static uint32_t tcc_hsm_ioctl_get_driver_version(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_version_param param;

	param.x = HSM_DRIVER_VER_X;
	param.y = HSM_DRIVER_VER_Y;
	param.z = HSM_DRIVER_VER_Z;
	if (tcc_copy_to_user(arg, (void *)&param, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	return TCCHSM_SUCCESS;
}

static uint32_t tcc_hsm_ioctl_run_ecdh_phaseI(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_ecdh_key_param param;
	uint32_t req = REQ_HSM_RUN_ECDH_PHASE_I;
	uint32_t ret = TCCHSM_ERR;

	if (tcc_copy_from_user((void *)&param, arg, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	ret = tcc_hsm_cmd_run_ecdh_phaseI(MBOX_DEV_HSM, req, &param);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("failed to run ecdh phase I\n");
		return ret;
	}

	if (tcc_copy_to_user(arg, (void *)&param, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	return ret;
}

static uint32_t tcc_hsm_ioctl_get_rng(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_rng_param param;
	uint32_t req = REQ_HSM_GET_RNG;
	uint32_t ret = TCCHSM_ERR;

	if (tcc_copy_from_user((void *)&param, arg, sizeof(param))
	    != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	if (param.rng_size > TCC_HSM_DMA_BUF_SIZE) {
		ELOG("The rngSize(0x%x) should not exceed 0x%x bytes\n",
		     param.rng_size, TCC_HSM_DMA_BUF_SIZE);
		return ret;
	}

	ret = tcc_hsm_cmd_get_rand(
		MBOX_DEV_HSM, req, (uint32_t)dma_buf->dstPhy, param.rng_size);

	dma_sync_single_for_cpu(
		dma_buf->dev, dma_buf->dstPhy, param.rng_size,
		DMA_FROM_DEVICE);

	if (ret != TCCHSM_SUCCESS) {
		ELOG("failed to get random number\n");
		return ret;
	}
	if (tcc_copy_to_user(param.rng, (void *)dma_buf->dstVir, param.rng_size)
	    != TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		return TCCHSM_ERR_INVALID_MEMORY;
	}

	return ret;
}

static long tcc_hsm_ioctl(struct file *file, uint32_t cmd, ulong arg)
{
	uint32_t ret = TCCHSM_ERR;

	/* gVerParam is initialized in tcc_hsm_open
	 * Do not run ioctl cmd If HSM F/W version is lower than the required
	 * version */
	if ((gVerParam.x != HSM_REQUIRED_FW_VER_X)
	    || (gVerParam.y != HSM_REQUIRED_FW_VER_Y)
	    || (gVerParam.z < HSM_REQUIRED_FW_VER_Z)) {
		ELOG("HSM FW verison(%d.%d.%d) must be higher than equal to %d.%d.%d\n",
		     gVerParam.x, gVerParam.y, gVerParam.z,
		     HSM_REQUIRED_FW_VER_X, HSM_REQUIRED_FW_VER_Y,
		     HSM_REQUIRED_FW_VER_Z);
		return TCCHSM_ERR_VERSION_MISMATCH;
	}

	mutex_lock(&tcc_hsm_mutex);

	switch (cmd) {
	case HSM_SET_KEY_FROM_OTP_CMD:
	case HSM_SET_KEY_FROM_SNOR_CMD:
		ret = tcc_hsm_ioctl_set_key(cmd, arg);
		break;

	case HSM_SET_MODN_FROM_OTP_CMD:
	case HSM_SET_MODN_FROM_SNOR_CMD:
		ret = tcc_hsm_ioctl_set_modn(cmd, arg);
		break;

	case HSM_RUN_AES_CMD:
	case HSM_RUN_SM4_CMD:
		ret = tcc_hsm_ioctl_run_aes(cmd, arg);
		break;

	case HSM_RUN_AES_BY_KT_CMD:
	case HSM_RUN_SM4_BY_KT_CMD:
		ret = tcc_hsm_ioctl_run_aes_by_kt(cmd, arg);
		break;

	case HSM_GEN_CMAC_VERIFY_CMD:
	case HSM_GEN_GMAC_CMD:
	case HSM_GEN_HMAC_CMD:
	case HSM_GEN_SM3_HMAC_CMD:
		ret = tcc_hsm_ioctl_gen_mac(cmd, arg);
		break;

	case HSM_GEN_CMAC_VERIFY_BY_KT_CMD:
	case HSM_GEN_GMAC_BY_KT_CMD:
	case HSM_GEN_HMAC_BY_KT_CMD:
	case HSM_GEN_SM3_HMAC_BY_KT_CMD:
		ret = tcc_hsm_ioctl_gen_mac_by_kt(cmd, arg);
		break;

	case HSM_GEN_SHA_CMD:
	case HSM_GEN_SM3_CMD:
		ret = tcc_hsm_ioctl_gen_hash(cmd, arg);
		break;

	case HSM_RUN_ECDSA_SIGN_CMD:
	case HSM_RUN_ECDSA_VERIFY_CMD:
		ret = tcc_hsm_ioctl_run_ecdsa(cmd, arg);
		break;

	case HSM_RUN_ECDSA_SIGN_BY_KT_CMD:
	case HSM_RUN_ECDSA_VERIFY_BY_KT_CMD:
		ret = tcc_hsm_ioctl_run_ecdsa_by_kt(cmd, arg);
		break;

	case HSM_RUN_RSASSA_PKCS_SIGN_CMD:
	case HSM_RUN_RSASSA_PKCS_VERIFY_CMD:
	case HSM_RUN_RSASSA_PSS_SIGN_CMD:
	case HSM_RUN_RSASSA_PSS_VERIFY_CMD:
		ret = tcc_hsm_ioctl_run_rsa(cmd, arg);
		break;

	case HSM_RUN_RSASSA_PKCS_SIGN_BY_KT_CMD:
	case HSM_RUN_RSASSA_PKCS_VERIFY_BY_KT_CMD:
	case HSM_RUN_RSASSA_PSS_SIGN_BY_KT_CMD:
	case HSM_RUN_RSASSA_PSS_VERIFY_BY_KT_CMD:
		ret = tcc_hsm_ioctl_run_rsa_by_kt(cmd, arg);
		break;

	case HSM_WRITE_OTP_CMD:
	case HSM_WRITE_SNOR_CMD:
		ret = tcc_hsm_ioctl_write(cmd, arg);
		break;

	case HSM_GET_FW_VER_CMD:
		ret = tcc_hsm_ioctl_get_fw_version(cmd, arg);
		break;

	case HSM_GET_DRIVER_VER_CMD:
		ret = tcc_hsm_ioctl_get_driver_version(cmd, arg);
		break;

	case HSM_GET_RNG_CMD:
		ret = tcc_hsm_ioctl_get_rng(cmd, arg);
		break;

	case HSM_RUN_ECDH_PHASE_I_CMD:
		ret = tcc_hsm_ioctl_run_ecdh_phaseI(cmd, arg);
		break;

	case HSM_RUN_ECDH_PHASE_II_CMD:
		ELOG("Not yet supported!\n");
		break;

	case HSM_RUN_ECDH_PUBKEY_COMPUTE_CMD:
		ELOG("Not yet supported!\n");
		break;

	default:
		ELOG("unknown command(0x%x)\n", cmd);
		break;
	}

	mutex_unlock(&tcc_hsm_mutex);

	return ret;
}

int32_t tcc_hsm_open(struct inode *inode, struct file *filp)
{
	/*Get HSM F/W version to check version when running ioctl  */
	if (tcc_hsm_cmd_get_version(MBOX_DEV_HSM, REQ_HSM_GET_VER, &gVerParam)
	    != TCCHSM_SUCCESS) {
		ELOG("failed to get version\n");
		return -1;
	}

	return 0;
}

int32_t tcc_hsm_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations tcc_hsm_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tcc_hsm_ioctl,
	.compat_ioctl = tcc_hsm_ioctl,
	.open = tcc_hsm_open,
	.release = tcc_hsm_release,
};

static struct miscdevice tcc_hsm_miscdevice = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = TCCHSM_DEVICE_NAME,
	.fops = &tcc_hsm_fops,
};

static int32_t tcc_hsm_probe(struct platform_device *pdev)
{
	dma_buf = devm_kzalloc(
		&pdev->dev, sizeof(struct tcc_hsm_dma_buf), GFP_KERNEL);
	if (dma_buf == NULL) {
		ELOG("failed to allocate dma_buf\n");
		return -ENOMEM;
	}

	dma_buf->dev = &pdev->dev;

	dma_buf->descVir = dma_alloc_coherent(
		&pdev->dev, TCC_HSM_DESC_BUF_SIZE, &dma_buf->descPhy, GFP_KERNEL);
	if (dma_buf->descVir == NULL) {
		ELOG("failed to allocate dma_buf->descVir\n");
		devm_kfree(&pdev->dev, dma_buf);
		return -ENOMEM;
	}

	dma_buf->srcVir = dma_alloc_coherent(
		&pdev->dev, TCC_HSM_DMA_BUF_SIZE, &dma_buf->srcPhy, GFP_KERNEL);
	if (dma_buf->srcVir == NULL) {
		ELOG("failed to allocate dma_buf->srcVir\n");
		dma_free_coherent(
			&pdev->dev, TCC_HSM_DESC_BUF_SIZE, dma_buf->descVir,
			dma_buf->descPhy);
		devm_kfree(&pdev->dev, dma_buf);
		return -ENOMEM;
	}

	dma_buf->dstVir = dma_alloc_coherent(
		&pdev->dev, TCC_HSM_DMA_BUF_SIZE, &dma_buf->dstPhy, GFP_KERNEL);
	if (dma_buf->dstVir == NULL) {
		ELOG("failed to allocate dma_buf->dstVir\n");
		dma_free_coherent(
			&pdev->dev, TCC_HSM_DESC_BUF_SIZE, dma_buf->descVir,
			dma_buf->descPhy);
		dma_free_coherent(
			&pdev->dev, TCC_HSM_DMA_BUF_SIZE, dma_buf->srcVir,
			dma_buf->srcPhy);
		devm_kfree(&pdev->dev, dma_buf);
		return -ENOMEM;
	}

	if (misc_register(&tcc_hsm_miscdevice) != TCCHSM_SUCCESS) {
		ELOG("register device err\n");
		dma_free_coherent(
			&pdev->dev, TCC_HSM_DESC_BUF_SIZE, dma_buf->descVir,
			dma_buf->descPhy);
		dma_free_coherent(
			&pdev->dev, TCC_HSM_DMA_BUF_SIZE, dma_buf->srcVir,
			dma_buf->srcPhy);
		dma_free_coherent(
			&pdev->dev, TCC_HSM_DMA_BUF_SIZE, dma_buf->dstVir,
			dma_buf->dstPhy);
		devm_kfree(&pdev->dev, dma_buf);
		return -EBUSY;
	}

	return 0;
}

static int32_t tcc_hsm_remove(struct platform_device *pdev)
{
	misc_deregister(&tcc_hsm_miscdevice);

	dma_free_coherent(
		&pdev->dev, TCC_HSM_DESC_BUF_SIZE, dma_buf->descVir,
		dma_buf->descPhy);
	dma_buf->descVir = NULL;

	dma_free_coherent(
		&pdev->dev, TCC_HSM_DMA_BUF_SIZE, dma_buf->srcVir,
		dma_buf->srcPhy);
	dma_buf->srcVir = NULL;

	dma_free_coherent(
		&pdev->dev, TCC_HSM_DMA_BUF_SIZE, dma_buf->dstVir,
		dma_buf->dstPhy);
	dma_buf->dstVir = NULL;

	devm_kfree(&pdev->dev, dma_buf);

	return 0;
}

#ifdef CONFIG_PM
static int32_t tcc_hsm_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int32_t tcc_hsm_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define tcc_hsm_suspend (NULL)
#define tcc_hsm_resume (NULL)
#endif

#ifdef CONFIG_OF
static const struct of_device_id hsm_of_match[2] = {
	{.compatible = "telechips,tcc-hsm"},
	{"", "", "", NULL},
};

MODULE_DEVICE_TABLE(of, hsm_of_match);
#endif

static struct platform_driver tcc_hsm_driver = {
	.driver = {.name = "tcc_hsm",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(hsm_of_match)
#endif
	},
	.probe = tcc_hsm_probe,
	.remove = tcc_hsm_remove,
#ifdef CONFIG_PM
	.suspend = tcc_hsm_suspend,
	.resume = tcc_hsm_resume,
#endif
};

static int32_t __init tcc_hsm_init(void)
{
	int32_t ret = TCCHSM_ERR;

	ret = platform_driver_register(&tcc_hsm_driver);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("platform_driver_register err(%d)\n", ret);
		return 0;
	}

	return ret;
}

static void __exit tcc_hsm_exit(void)
{
	platform_driver_unregister(&tcc_hsm_driver);
}

module_init(tcc_hsm_init);
module_exit(tcc_hsm_exit);

MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC HSM driver");
MODULE_LICENSE("GPL");

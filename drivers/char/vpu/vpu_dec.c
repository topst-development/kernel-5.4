// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#if (defined(CONFIG_VDEC_CNT_1) || defined(CONFIG_VDEC_CNT_2) || \
	defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || \
	defined(CONFIG_VDEC_CNT_5))

#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/compat.h>

#include <linux/io.h>
#include <asm/div64.h>

#include "vpu_buffer.h"
#include "vpu_dec.h"
#include "vpu_mgr.h"

#ifdef CONFIG_SUPPORT_TCC_JPU
#include "jpu_mgr.h"
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
#include "vpu_4k_d2_mgr.h"
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
#include "hevc_mgr.h"
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
#include "vp9_mgr.h"
#endif

#define dprintk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_VPU_MGR: " msg)
#define detailk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_VPU_MGR: " msg)
#define err(msg...)      V_DBG(VPU_DBG_ERROR, "TCC_VPU_MGR [Err]: " msg)

static void _vdec_inter_add_list(struct _vpu_decoder_data *vdata,
					int cmd, void *args)
{
	struct VpuList *oper_data = &vdata->vdec_list[vdata->list_idx];

	oper_data->type          = vdata->gsDecType;
	oper_data->cmd_type      = cmd;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		oper_data->handle = vdata->gsJpuDecInit_Info.gsJpuDecHandle;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC:
	case STD_VP9:
		oper_data->handle = vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		oper_data->handle = vdata->gsHevcDecInit_Info.gsHevcDecHandle;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		oper_data->handle = vdata->gsVp9DecInit_Info.gsVp9DecHandle;
		break;
#endif
	default:
		oper_data->handle = vdata->gsVpuDecInit_Info.gsVpuDecHandle;
		break;
	}

	oper_data->args          = args;
	oper_data->comm_data     = &vdata->vComm_data;
	oper_data->vpu_result    = &vdata->gsCommDecResult;
	*oper_data->vpu_result   = 0;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		jmgr_list_manager(oper_data, LIST_ADD);
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC:
	case STD_VP9:
		vmgr_4k_d2_list_manager(oper_data, LIST_ADD);
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		hmgr_list_manager(oper_data, LIST_ADD);
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		vp9mgr_list_manager(oper_data, LIST_ADD);
		break;
#endif
	default:
		vmgr_list_manager(oper_data, LIST_ADD);
		break;
	}

	vdata->list_idx = ((vdata->list_idx+1) == LIST_MAX ?
			(0) : (vdata->list_idx+1));
}

static void _vdec_init_list(struct _vpu_decoder_data *vdata)
{
	int i;

	for (i = 0; i < LIST_MAX; i++)
		vdata->vdec_list[i].comm_data = NULL;
}

static int _vdec_proc_init(struct _vpu_decoder_data *vdata,
					void *arg, bool fromKernel)
{
	void *pArgs;

	V_DBG(VPU_DBG_SEQUENCE, "%s: enter !!", vdata->misc->name);

	_vdec_init_list(vdata);

	switch (vdata->gsCodecType) {
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsJpuDecInit_Info, arg,
				sizeof(JDEC_INIT_t));
		else
			if (copy_from_user(&vdata->gsJpuDecInit_Info, arg,
				sizeof(JDEC_INIT_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsJpuDecInit_Info;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsV4kd2DecInit_Info, arg,
				sizeof(VPU_4K_D2_INIT_t));
		else
			if (copy_from_user(&vdata->gsV4kd2DecInit_Info, arg,
				sizeof(VPU_4K_D2_INIT_t)))
				return -EFAULT;
		pArgs = (void *)&vdata->gsV4kd2DecInit_Info;
		vdata->gsIsDiminishedCopy
			= (vdata->gsV4kd2DecInit_Info.gsV4kd2DecInit
				.m_uiDecOptFlags & (1<<26)) ? 1 : 0;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsHevcDecInit_Info, arg,
				sizeof(HEVC_INIT_t));
		else
			if (copy_from_user(&vdata->gsHevcDecInit_Info, arg,
				sizeof(HEVC_INIT_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsHevcDecInit_Info;
		vdata->gsIsDiminishedCopy
			= (vdata->gsHevcDecInit_Info.gsHevcDecInit
			.m_uiDecOptFlags & (1<<26)) ? 1 : 0;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVp9DecInit_Info, arg,
				sizeof(VP9_INIT_t));
		else
			if (copy_from_user(&vdata->gsVp9DecInit_Info, arg,
				sizeof(VP9_INIT_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVp9DecInit_Info;
		vdata->gsIsDiminishedCopy
			= (vdata->gsVp9DecInit_Info.gsVp9DecInit
				.m_uiDecOptFlags & (1<<26)) ? 1 : 0;
		}
		break;
#endif
	default:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVpuDecInit_Info, arg,
				sizeof(VDEC_INIT_t));
		else
			if (copy_from_user(&vdata->gsVpuDecInit_Info, arg,
				sizeof(VDEC_INIT_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVpuDecInit_Info;
		vdata->gsIsDiminishedCopy
			= (vdata->gsVpuDecInit_Info.gsVpuDecInit
				.m_uiDecOptFlags & (1<<26)) ? 1 : 0;
		}
		break;
	}

	if (fromKernel)
		_vdec_inter_add_list(vdata, VPU_DEC_INIT_KERNEL, pArgs);
	else
		_vdec_inter_add_list(vdata, VPU_DEC_INIT, pArgs);

	return 0;
}

static int _vdec_proc_exit(struct _vpu_decoder_data *vdata,
					void *arg, bool fromKernel)
{
	void *pArgs;

	V_DBG(VPU_DBG_CLOSE,
		"%s :: vdec process exit!!", vdata->misc->name);

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsJpuDecInOut_Info, arg,
				sizeof(JPU_DECODE_t));
		else
			if (copy_from_user(&vdata->gsJpuDecInOut_Info, arg,
				sizeof(JPU_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsJpuDecInOut_Info;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t));
		else
			if (copy_from_user(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsV4kd2DecInOut_Info;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsHevcDecInOut_Info, arg,
				sizeof(HEVC_DECODE_t));
		else
			if (copy_from_user(&vdata->gsHevcDecInOut_Info, arg,
				sizeof(HEVC_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsHevcDecInOut_Info;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVp9DecInOut_Info, arg,
				sizeof(VP9_DECODE_t));
		else
			if (copy_from_user(&vdata->gsVp9DecInOut_Info, arg,
				sizeof(VP9_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVp9DecInOut_Info;
		}
		break;
#endif
	default:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVpuDecInOut_Info, arg,
				sizeof(VDEC_DECODE_t));
		else
			if (copy_from_user(&vdata->gsVpuDecInOut_Info, arg,
				sizeof(VDEC_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVpuDecInOut_Info;
		}
		break;
	}

	_vdec_inter_add_list(vdata, VPU_DEC_CLOSE, pArgs);

	return 0;
}

static int _vdec_proc_seq_header(struct _vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	void *pArgs;
	size_t copySize = 0;

	V_DBG(VPU_DBG_SEQUENCE,
		"%s :: enter!!", vdata->misc->name);

	switch (vdata->gsCodecType) {
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		{
#if defined(JPU_C6)
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsJpuDecInOut_Info :
			(void *)&vdata->gsJpuDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(JPU_DECODE_t) : sizeof(JDEC_SEQ_HEADER_t);
#else
		err("%s :: jpu not support this !! ", vdata->misc->name);
		return -0x999;
#endif
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsV4kd2DecInOut_Info :
			(void *)&vdata->gsV4kd2DecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VPU_4K_D2_DECODE_t) :
			sizeof(VPU_4K_D2_SEQ_HEADER_t);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsHevcDecInOut_Info :
			(void *)&vdata->gsHevcDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(HEVC_DECODE_t) : sizeof(HEVC_SEQ_HEADER_t);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsVp9DecInOut_Info :
			(void *)&vdata->gsVp9DecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VP9_DECODE_t) : sizeof(VP9_SEQ_HEADER_t);
		}
		break;
#endif
	default:
		{
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsVpuDecInOut_Info :
			(void *)&vdata->gsVpuDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VDEC_DECODE_t) : sizeof(VDEC_SEQ_HEADER_t);
		if (vdata->gsIsDiminishedCopy)
			V_DBG(VPU_DBG_IO_FB_INFO,
				"[DiminishedCopy] %d x %d",
				vdata->gsVpuDecInOut_Info.gsVpuDecInitialInfo
					.m_iPicWidth,
				vdata->gsVpuDecInOut_Info.gsVpuDecInitialInfo
					.m_iPicHeight);
		}
		break;
	}

	if (fromKernel)
		(void)memcpy(pArgs, arg, copySize);
	else
		if (copy_from_user(pArgs, arg, copySize))
			return -EFAULT;

	_vdec_inter_add_list(vdata, VPU_DEC_SEQ_HEADER, pArgs);

	return 0;
}

static int _vdec_proc_reg_framebuffer(struct _vpu_decoder_data *vdata,
					void *arg, bool fromKernel)
{
	void *pArgs;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsJpuDecBuffer_Info, arg,
				sizeof(JPU_SET_BUFFER_t));
		else
			if (copy_from_user(&vdata->gsJpuDecBuffer_Info, arg,
				sizeof(JPU_SET_BUFFER_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsJpuDecBuffer_Info;

		V_DBG(VPU_DBG_BUF_STATUS,
				"%s :: jpu_proc_reg_framebuffer :: phy = 0x%x, virt = 0x%x, cnt = 0x%x !!",
				vdata->misc->name,
				vdata->gsJpuDecBuffer_Info.gsJpuDecBuffer
					.m_FrameBufferStartAddr[0],
				vdata->gsJpuDecBuffer_Info.gsJpuDecBuffer
					.m_FrameBufferStartAddr[1],
				vdata->gsJpuDecBuffer_Info.gsJpuDecBuffer
					.m_iFrameBufferCount);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsV4kd2DecBuffer_Info, arg,
				sizeof(VPU_4K_D2_SET_BUFFER_t));
		else
			if (copy_from_user(&vdata->gsV4kd2DecBuffer_Info, arg,
				sizeof(VPU_4K_D2_SET_BUFFER_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsV4kd2DecBuffer_Info;

		V_DBG(VPU_DBG_BUF_STATUS,
				"%s :: vpu-4k-d2_proc_reg_framebuffer :: phy = 0x%x, virt = 0x%x, cnt = 0x%x !!",
				vdata->misc->name,
				vdata->gsV4kd2DecBuffer_Info.gsV4kd2DecBuffer
					.m_FrameBufferStartAddr[0],
				vdata->gsV4kd2DecBuffer_Info.gsV4kd2DecBuffer
					.m_FrameBufferStartAddr[1],
				vdata->gsV4kd2DecBuffer_Info.gsV4kd2DecBuffer
					.m_iFrameBufferCount);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsHevcDecBuffer_Info, arg,
				sizeof(HEVC_SET_BUFFER_t));
		else
			if (copy_from_user(&vdata->gsHevcDecBuffer_Info, arg,
				sizeof(HEVC_SET_BUFFER_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsHevcDecBuffer_Info;

		V_DBG(VPU_DBG_BUF_STATUS,
				"%s :: hevc_proc_reg_framebuffer :: phy = 0x%x, virt = 0x%x, cnt = 0x%x !!",
				vdata->misc->name,
				vdata->gsHevcDecBuffer_Info.gsHevcDecBuffer
					.m_FrameBufferStartAddr[0],
				vdata->gsHevcDecBuffer_Info.gsHevcDecBuffer
					.m_FrameBufferStartAddr[1],
				vdata->gsHevcDecBuffer_Info.gsHevcDecBuffer
					.m_iFrameBufferCount);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVp9DecBuffer_Info, arg,
				sizeof(VP9_SET_BUFFER_t));
		else
			if (copy_from_user(&vdata->gsVp9DecBuffer_Info, arg,
				sizeof(VP9_SET_BUFFER_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVp9DecBuffer_Info;

		V_DBG(VPU_DBG_BUF_STATUS,
				"%s :: vp9_proc_reg_framebuffer :: phy = 0x%x, virt = 0x%x, cnt = 0x%x !!",
				vdata->misc->name,
				vdata->gsVp9DecBuffer_Info.gsVp9DecBuffer
					.m_FrameBufferStartAddr[0],
				vdata->gsVp9DecBuffer_Info.gsVp9DecBuffer
					.m_FrameBufferStartAddr[1],
				vdata->gsVp9DecBuffer_Info.gsVp9DecBuffer
					.m_iFrameBufferCount);
		}
		break;
#endif
	default:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVpuDecBuffer_Info, arg,
				sizeof(VDEC_SET_BUFFER_t));
		else
			if (copy_from_user(&vdata->gsVpuDecBuffer_Info, arg,
				sizeof(VDEC_SET_BUFFER_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVpuDecBuffer_Info;

		V_DBG(VPU_DBG_BUF_STATUS,
				"%s :: _vdec proc reg framebuffer :: phy = 0x%x, virt = 0x%x, cnt = 0x%x!!",
				vdata->misc->name,
				vdata->gsVpuDecBuffer_Info.gsVpuDecBuffer
					.m_FrameBufferStartAddr[0],
				vdata->gsVpuDecBuffer_Info.gsVpuDecBuffer
					.m_FrameBufferStartAddr[1],
				vdata->gsVpuDecBuffer_Info.gsVpuDecBuffer
					.m_iFrameBufferCount);
		}
		break;
	}

	_vdec_inter_add_list(vdata, VPU_DEC_REG_FRAME_BUFFER, pArgs);

	return 0;
}

static int _vdec_proc_decode(struct _vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	void *pArgs;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsJpuDecInOut_Info, arg,
				sizeof(JPU_DECODE_t));
		else
			if (copy_from_user(&vdata->gsJpuDecInOut_Info, arg,
				sizeof(JPU_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsJpuDecInOut_Info;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::_jpu_proc_decode In !! handle = 0x%x, in_stream_size = 0x%x",
			vdata->misc->name,
			vdata->gsJpuDecInit_Info.gsJpuDecHandle,
			vdata->gsJpuDecInOut_Info.gsJpuDecInput
				.m_iBitstreamDataSize);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t));
		else
			if (copy_from_user(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsV4kd2DecInOut_Info;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s:_vpu-4k-d2_proc_decode In !! handle = 0x%x, in_stream_size = 0x%x",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle,
			vdata->gsV4kd2DecInOut_Info.gsV4kd2DecInput
				.m_iBitstreamDataSize);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsHevcDecInOut_Info, arg,
				sizeof(HEVC_DECODE_t));
		else
			if (copy_from_user(&vdata->gsHevcDecInOut_Info, arg,
				sizeof(HEVC_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsHevcDecInOut_Info;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s: _hevc_proc_decode In !! handle = 0x%x, in_stream_size = 0x%x",
			vdata->misc->name,
			vdata->gsHevcDecInit_Info.gsHevcDecHandle,
			vdata->gsHevcDecInOut_Info.gsHevcDecInput
				.m_iBitstreamDataSize);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVp9DecInOut_Info, arg,
				sizeof(VP9_DECODE_t));
		else
			if (copy_from_user(&vdata->gsVp9DecInOut_Info, arg,
				sizeof(VP9_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVp9DecInOut_Info;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::_vp9_proc_decode In !! handle = 0x%x, in_stream_size = 0x%x",
			vdata->misc->name,
			vdata->gsVp9DecInit_Info.gsVp9DecHandle,
			vdata->gsVp9DecInOut_Info.gsVp9DecInput
				.m_iBitstreamDataSize);
		}
		break;
#endif
	default:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVpuDecInOut_Info, arg,
				sizeof(VDEC_DECODE_t));
		else
			if (copy_from_user(&vdata->gsVpuDecInOut_Info, arg,
				sizeof(VDEC_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVpuDecInOut_Info;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s :: enter !! handle = 0x%x, in_stream_size = 0x%x",
			vdata->misc->name,
			vdata->gsVpuDecInit_Info.gsVpuDecHandle,
			vdata->gsVpuDecInOut_Info.gsVpuDecInput
				.m_iBitstreamDataSize);
		}
		break;
	}

	_vdec_inter_add_list(vdata, VPU_DEC_DECODE, pArgs);

	return 0;
}

static int _vdec_get_decode_output(struct _vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	void *pArgs;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		if (fromKernel)
			(void)memcpy(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t));
		else
			if (copy_from_user(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsV4kd2DecInOut_Info;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::_vpu-4k-d2_get_decode_output In !! handle = 0x%x, in_stream_size = 0x%x ",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle,
			vdata->gsV4kd2DecInOut_Info.gsV4kd2DecInput
				.m_iBitstreamDataSize);
		break;
#endif
	default:
		V_DBG(VPU_DBG_ERROR,
			"%s :: vdec get decode output In !! error command",
			vdata->misc->name);
		return -EFAULT;
	}

	_vdec_inter_add_list(vdata, VPU_DEC_GET_OUTPUT_INFO, pArgs);

	return 0;
}
static int _vdec_proc_clear_bufferflag(struct _vpu_decoder_data *vdata,
				void *arg, bool fromKernel)
{
	void *pArgs;

	if (fromKernel)
		(void)memcpy(&vdata->gsDecClearBuffer_index, arg,
			sizeof(unsigned int));
	else
		if (copy_from_user(&vdata->gsDecClearBuffer_index, arg,
			sizeof(unsigned int)))
			return -EFAULT;

	pArgs = (void *)&vdata->gsDecClearBuffer_index;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		V_DBG(VPU_DBG_CMD, "%s ::_jpu_proc_clear_bufferflag : %d !!",
			vdata->misc->name, vdata->gsDecClearBuffer_index);
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		V_DBG(VPU_DBG_CMD,
			"%s ::_vpu-4k-d2_proc_clear_bufferflag : %d !!",
			vdata->misc->name, vdata->gsDecClearBuffer_index);
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		V_DBG(VPU_DBG_CMD, "%s ::_hevc_proc_clear_bufferflag : %d !!",
			vdata->misc->name, vdata->gsDecClearBuffer_index);
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		V_DBG(VPU_DBG_CMD, "%s ::_vp9_proc_clear_bufferflag : %d !!",
			vdata->misc->name, vdata->gsDecClearBuffer_index);
		break;
#endif
	default:
		V_DBG(VPU_DBG_CMD, "%s ::_vdec proc clear bufferflag : %d !!",
			vdata->misc->name, vdata->gsDecClearBuffer_index);
		break;
	}

	_vdec_inter_add_list(vdata, VPU_DEC_BUF_FLAG_CLEAR, pArgs);

	return 0;
}

static int _vdec_proc_flush(struct _vpu_decoder_data *vdata,
					void *arg, bool fromKernel)
{
	void *pArgs;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsJpuDecInOut_Info, arg,
			sizeof(JPU_DECODE_t));
		else
			if (copy_from_user(&vdata->gsJpuDecInOut_Info, arg,
				sizeof(JPU_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsJpuDecInOut_Info;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::jpu_proc_flush In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsJpuDecInit_Info.gsJpuDecHandle);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t));
		else
			if (copy_from_user(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsV4kd2DecInOut_Info;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::vpu-4k-d2_proc_flush In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsHevcDecInOut_Info, arg,
				sizeof(HEVC_DECODE_t));
		else
			if (copy_from_user(&vdata->gsHevcDecInOut_Info, arg,
				sizeof(HEVC_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsHevcDecInOut_Info;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::hevc_proc_flush In !! handle = 0x%x ",
			vdata->misc->name,
			vdata->gsHevcDecInit_Info.gsHevcDecHandle);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVp9DecInOut_Info, arg,
				sizeof(VP9_DECODE_t));
		else
			if (copy_from_user(&vdata->gsVp9DecInOut_Info, arg,
				sizeof(VP9_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVp9DecInOut_Info;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::vp9_proc_flush In !! handle = 0x%x ",
			vdata->misc->name,
			vdata->gsVp9DecInit_Info.gsVp9DecHandle);
		}
		break;
#endif
	default:
	{
		if (fromKernel)
			(void)memcpy(&vdata->gsVpuDecInOut_Info, arg,
				sizeof(VDEC_DECODE_t));
		else
			if (copy_from_user(&vdata->gsVpuDecInOut_Info, arg,
				sizeof(VDEC_DECODE_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVpuDecInOut_Info;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s :: enter !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsVpuDecInit_Info.gsVpuDecHandle);
		}
		break;
	}

	_vdec_inter_add_list(vdata, VPU_DEC_FLUSH_OUTPUT, pArgs);

	return 0;
}

static int _vdec_result_general(struct _vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	if (fromKernel)
		(void)memcpy(arg, &vdata->gsCommDecResult, sizeof(int));
	else
		if (copy_to_user(arg, &vdata->gsCommDecResult, sizeof(int)))
			return -EFAULT;
	return 0;
}

static int _vdec_result_init(struct _vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		{
		vdata->gsJpuDecInit_Info.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsJpuDecInit_Info,
				sizeof(JDEC_INIT_t));
		else
			if (copy_to_user(arg, &vdata->gsJpuDecInit_Info,
				sizeof(JDEC_INIT_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		vdata->gsV4kd2DecInit_Info.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsV4kd2DecInit_Info,
				sizeof(VPU_4K_D2_INIT_t));
		else
			if (copy_to_user(arg, &vdata->gsV4kd2DecInit_Info,
				sizeof(VPU_4K_D2_INIT_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		vdata->gsHevcDecInit_Info.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsHevcDecInit_Info,
				sizeof(HEVC_INIT_t));
		else
			if (copy_to_user(arg, &vdata->gsHevcDecInit_Info,
				sizeof(HEVC_INIT_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		vdata->gsVp9DecInit_Info.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsVp9DecInit_Info,
				sizeof(VP9_INIT_t));
		else
			if (copy_to_user(arg, &vdata->gsVp9DecInit_Info,
				sizeof(VP9_INIT_t)))
				return -EFAULT;
		}
		break;
#endif
	default:
		{
		vdata->gsVpuDecInit_Info.result = vdata->gsCommDecResult;
		if (fromKernel)
			(void)memcpy(arg, &vdata->gsVpuDecInit_Info,
				sizeof(VDEC_INIT_t));
		else
			if (copy_to_user(arg, &vdata->gsVpuDecInit_Info,
				sizeof(VDEC_INIT_t)))
				return -EFAULT;
		}
		break;
	}

	return 0;
}

static int _vdec_result_seq_header(struct _vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	void *src = NULL;
	size_t copySize = 0;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		{
#if defined(JPU_C6)
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsJpuDecInOut_Info :
			(void *)&vdata->gsJpuDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(JPU_DECODE_t) : sizeof(JDEC_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy)
			vdata->gsJpuDecInOut_Info.result
				= vdata->gsCommDecResult;
		else
			vdata->gsJpuDecSeqHeader_Info.result
				= vdata->gsCommDecResult;
#else
		err("%s ::jpu not support this !!", vdata->misc->name);
		return -0x999;
#endif
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsV4kd2DecInOut_Info :
			(void *)&vdata->gsV4kd2DecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VPU_4K_D2_DECODE_t) :
			sizeof(VPU_4K_D2_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy)
			vdata->gsV4kd2DecInOut_Info.result
				= vdata->gsCommDecResult;
		else
			vdata->gsV4kd2DecSeqHeader_Info.result
				= vdata->gsCommDecResult;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsHevcDecInOut_Info :
			(void *)&vdata->gsHevcDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(HEVC_DECODE_t) :
			sizeof(HEVC_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy)
			vdata->gsHevcDecInOut_Info.result
				= vdata->gsCommDecResult;
		else
			vdata->gsHevcDecSeqHeader_Info.result
				= vdata->gsCommDecResult;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsVp9DecInOut_Info :
			(void *)&vdata->gsVp9DecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VP9_DECODE_t) :
			sizeof(VP9_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy)
			vdata->gsVp9DecInOut_Info.result
					= vdata->gsCommDecResult;
		else
			vdata->gsVp9DecSeqHeader_Info.result
					= vdata->gsCommDecResult;
		}
		break;
#endif
	default:
		{
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsVpuDecInOut_Info :
			(void *)&vdata->gsVpuDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VDEC_DECODE_t) :
			sizeof(VDEC_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy)
			vdata->gsVpuDecInOut_Info.result
				= vdata->gsCommDecResult;
		else
			vdata->gsVpuDecSeqHeader_Info.result
				= vdata->gsCommDecResult;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"vdata->gsIsDiminishedCopy %d",
			vdata->gsIsDiminishedCopy);

		if (vdata->gsIsDiminishedCopy) {
			V_DBG(VPU_DBG_IO_FB_INFO,
				"%d x %d",
				vdata->gsVpuDecInOut_Info
					.gsVpuDecInitialInfo.m_iPicWidth,
				vdata->gsVpuDecInOut_Info
					.gsVpuDecInitialInfo.m_iPicHeight);
		} else {
			V_DBG(VPU_DBG_IO_FB_INFO,
				"%d x %d",
				vdata->gsVpuDecSeqHeader_Info
					.gsVpuDecInitialInfo.m_iPicWidth,
				vdata->gsVpuDecSeqHeader_Info
					.gsVpuDecInitialInfo.m_iPicHeight);
		}
		}
		break;
	}

	if (fromKernel)
		(void)memcpy(arg, src, copySize);
	else
		if (copy_to_user(arg, src, copySize))
			return -EFAULT;

	return 0;
}

static int _vdec_result_decode(struct _vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		{
		vdata->gsJpuDecInOut_Info.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsJpuDecInOut_Info,
				sizeof(JPU_DECODE_t));
		else

			if (copy_to_user(arg, &vdata->gsJpuDecInOut_Info,
				sizeof(JPU_DECODE_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		vdata->gsV4kd2DecInOut_Info.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsV4kd2DecInOut_Info,
				sizeof(VPU_4K_D2_DECODE_t));
		else
			if (copy_to_user(arg, &vdata->gsV4kd2DecInOut_Info,
				sizeof(VPU_4K_D2_DECODE_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		vdata->gsHevcDecInOut_Info.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsHevcDecInOut_Info,
				sizeof(HEVC_DECODE_t));
		else
			if (copy_to_user(arg, &vdata->gsHevcDecInOut_Info,
				sizeof(HEVC_DECODE_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		vdata->gsVp9DecInOut_Info.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsVp9DecInOut_Info,
				sizeof(VP9_DECODE_t));
		else
			if (copy_to_user(arg, &vdata->gsVp9DecInOut_Info,
				sizeof(VP9_DECODE_t)))
				return -EFAULT;
		}
		break;
#endif
	default:
		{
		vdata->gsVpuDecInOut_Info.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsVpuDecInOut_Info,
				sizeof(VDEC_DECODE_t));
		else
			if (copy_to_user(arg, &vdata->gsVpuDecInOut_Info,
				sizeof(VDEC_DECODE_t)))
				return -EFAULT;
		}
		break;
	}

	return 0;
}

static int _vdec_result_flush(struct _vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		{
		vdata->gsJpuDecInOut_Info.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsJpuDecInOut_Info,
				sizeof(JPU_DECODE_t));
		else
			if (copy_to_user(arg, &vdata->gsJpuDecInOut_Info,
				sizeof(JPU_DECODE_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		vdata->gsV4kd2DecInOut_Info.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsV4kd2DecInOut_Info,
				sizeof(VPU_4K_D2_DECODE_t));
		else
			if (copy_to_user(arg, &vdata->gsV4kd2DecInOut_Info,
				sizeof(VPU_4K_D2_DECODE_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		vdata->gsHevcDecInOut_Info.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsHevcDecInOut_Info,
				sizeof(HEVC_DECODE_t));
		else
			if (copy_to_user(arg, &vdata->gsHevcDecInOut_Info,
				sizeof(HEVC_DECODE_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		vdata->gsVp9DecInOut_Info.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsVp9DecInOut_Info,
				sizeof(VP9_DECODE_t));
		else
			if (copy_to_user(arg, &vdata->gsVp9DecInOut_Info,
				sizeof(VP9_DECODE_t)))
				return -EFAULT;
		}
		break;
#endif
	default:
		{
		vdata->gsVpuDecInOut_Info.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsVpuDecInOut_Info,
				sizeof(VDEC_DECODE_t));
		else
			if (copy_to_user(arg, &vdata->gsVpuDecInOut_Info,
				sizeof(VDEC_DECODE_t)))
				return -EFAULT;

		}
		break;
	}

	return 0;
}

static int _vdec_proc_swreset(struct _vpu_decoder_data *vdata, bool fromKernel)
{
	_vdec_inter_add_list(vdata, VPU_DEC_SWRESET, (void *)NULL);

	return 0;
}

static int _vdec_proc_buf_status(struct _vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	void *pArgs;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
	err("%s ::jpu not support this !!", vdata->misc->name);
	return -0x999;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsV4kd2DecBufStatus, arg,
				sizeof(VPU_4K_D2_RINGBUF_GETINFO_t));
		else
			if (copy_from_user(&vdata->gsV4kd2DecBufStatus, arg,
				sizeof(VPU_4K_D2_RINGBUF_GETINFO_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsV4kd2DecBufStatus;

		V_DBG(VPU_DBG_BUF_STATUS,
			"%s ::vpu-4k-d2_proc_buf_status In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsHevcDecBufStatus, arg,
				sizeof(HEVC_RINGBUF_GETINFO_t));
		else
			if (copy_from_user(&vdata->gsHevcDecBufStatus, arg,
				sizeof(HEVC_RINGBUF_GETINFO_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsHevcDecBufStatus;

		V_DBG(VPU_DBG_BUF_STATUS,
			"%s ::hevc_proc_buf_status In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsHevcDecInit_Info.gsHevcDecHandle);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVp9DecBufStatus, arg,
				sizeof(VP9_RINGBUF_GETINFO_t));
		else
			if (copy_from_user(&vdata->gsVp9DecBufStatus, arg,
				sizeof(VP9_RINGBUF_GETINFO_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVp9DecBufStatus;

		V_DBG(VPU_DBG_BUF_STATUS,
			"%s: vp9_proc_buf_status In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsVp9DecInit_Info.gsVp9DecHandle);
		}
		break;
#endif
	default:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVpuDecBufStatus, arg,
				sizeof(VDEC_RINGBUF_GETINFO_t));
		else
			if (copy_from_user(&vdata->gsVpuDecBufStatus, arg,
				sizeof(VDEC_RINGBUF_GETINFO_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVpuDecBufStatus;

		V_DBG(VPU_DBG_BUF_STATUS,
			"%s :: vdec_proc_buf_status In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsVpuDecInit_Info.gsVpuDecHandle);
		}
		break;
	}

	_vdec_inter_add_list(vdata, GET_RING_BUFFER_STATUS, pArgs);

	return 0;
}

static int _vdec_proc_buf_fill(struct _vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	void *pArgs;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		err("%s: Not supported by JPU !!", vdata->misc->name);
		return -0x999;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		if (fromKernel)
			(void)memcpy(&vdata->gsV4kd2DecBufFill, arg,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_t));
		else
			if (copy_from_user(&vdata->gsV4kd2DecBufFill, arg,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsV4kd2DecBufFill;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s: vpu-4k-d2_proc_buf_fill In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle);
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		if (fromKernel)
			(void)memcpy(&vdata->gsHevcDecBufFill, arg,
				sizeof(HEVC_RINGBUF_SETBUF_t));
		else
			if (copy_from_user(&vdata->gsHevcDecBufFill, arg,
				sizeof(HEVC_RINGBUF_SETBUF_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsHevcDecBufFill;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s: hevc_proc_buf_fill In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsHevcDecInit_Info.gsHevcDecHandle);
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		err("%s: Not supported by G2V2 VP9 !!", vdata->misc->name);
		return -0x999;
		}
		break;
#endif
	default:
		if (fromKernel)
			(void)memcpy(&vdata->gsVpuDecBufFill, arg,
				sizeof(VDEC_RINGBUF_SETBUF_t));
		else
			if (copy_from_user(&vdata->gsVpuDecBufFill, arg,
				sizeof(VDEC_RINGBUF_SETBUF_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVpuDecBufFill;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s: enter !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsVpuDecInit_Info.gsVpuDecHandle);
		break;
	}

	_vdec_inter_add_list(vdata, FILL_RING_BUFFER_AUTO, pArgs);

	return 0;
}

static int _vdec_proc_update_wp(struct _vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	void *pArgs;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		err("%s ::jpu not support this !!", vdata->misc->name);
		return -0x999;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsV4kd2DecUpdateWP, arg,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t));
		else
			if (copy_from_user(&vdata->gsV4kd2DecUpdateWP, arg,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsV4kd2DecUpdateWP;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::vpu-4k-d2_proc_buf_fill In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsHevcDecUpdateWP, arg,
				sizeof(HEVC_RINGBUF_SETBUF_PTRONLY_t));
		else
			if (copy_from_user(&vdata->gsHevcDecUpdateWP, arg,
				sizeof(HEVC_RINGBUF_SETBUF_PTRONLY_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsHevcDecUpdateWP;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::hevc_proc_buf_fill In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsHevcDecInit_Info.gsHevcDecHandle);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVp9DecUpdateWP, arg,
				sizeof(VP9_RINGBUF_SETBUF_PTRONLY_t));
		else
			if (copy_from_user(&vdata->gsVp9DecUpdateWP, arg,
				sizeof(VP9_RINGBUF_SETBUF_PTRONLY_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVp9DecUpdateWP;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::vp9_proc_buf_fill In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsVp9DecInit_Info.gsVp9DecHandle);
		}
		break;
#endif
	default:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVpuDecUpdateWP, arg,
				sizeof(VDEC_RINGBUF_SETBUF_PTRONLY_t));
		else
			if (copy_from_user(&vdata->gsVpuDecUpdateWP, arg,
				sizeof(VDEC_RINGBUF_SETBUF_PTRONLY_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVpuDecUpdateWP;

		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s: _vdec_proc_buf_fill In !! handle = 0x%x",
			vdata->gsVpuDecInit_Info.gsVpuDecHandle);
		}
		break;
	}

	_vdec_inter_add_list(vdata, VPU_UPDATE_WRITE_BUFFER_PTR, pArgs);

	return 0;
}

static int _vdec_proc_seq_header_ring(struct _vpu_decoder_data *vdata,
					void *arg, bool fromKernel)
{
	void *pArgs;
	size_t copySize = 0;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		err("%s ::jpu not support this !!", vdata->misc->name);
		return -0x999;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsV4kd2DecInOut_Info :
			(void *)&vdata->gsV4kd2DecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VPU_4K_D2_DECODE_t) :
			sizeof(VPU_4K_D2_SEQ_HEADER_t);
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s: vpu-4k-d2_proc_buf_fill In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsHevcDecInOut_Info :
			(void *)&vdata->gsHevcDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(HEVC_DECODE_t) : sizeof(HEVC_SEQ_HEADER_t);
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s: hevc_proc_buf_fill In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsHevcDecInit_Info.gsHevcDecHandle);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsVp9DecInOut_Info :
			(void *)&vdata->gsVp9DecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VP9_DECODE_t) : sizeof(VP9_SEQ_HEADER_t);
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::vp9_proc_buf_fill In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsVp9DecInit_Info.gsVp9DecHandle);
		}
		break;
#endif
	default:
		{
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsVpuDecInOut_Info :
			(void *)&vdata->gsVpuDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VDEC_DECODE_t) : sizeof(VDEC_SEQ_HEADER_t);
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s: _vdec_proc_buf_fill In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsVpuDecInit_Info.gsVpuDecHandle);
		}
		break;
	}

	if (fromKernel)
		(void)memcpy(pArgs, arg, copySize);
	else
		if (copy_from_user(pArgs, arg, copySize))
			return -EFAULT;

	_vdec_inter_add_list(vdata,
		GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY,
		pArgs);

	return 0;
}

static int _vdec_proc_get_version(struct _vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	void *pArgs;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsJpuDecVersion, arg,
				sizeof(JPU_GET_VERSION_t));
		else
			if (copy_from_user(&vdata->gsJpuDecVersion, arg,
				sizeof(JPU_GET_VERSION_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsJpuDecVersion;

		V_DBG(VPU_DBG_VER_INFO,
			"%s: jpu_proc_get_version (handle: 0x%x)",
			vdata->misc->name,
			vdata->gsJpuDecInit_Info.gsJpuDecHandle);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsV4kd2DecVersion, arg,
				sizeof(VPU_4K_D2_GET_VERSION_t));
		else
			if (copy_from_user(&vdata->gsV4kd2DecVersion,
				arg, sizeof(VPU_4K_D2_GET_VERSION_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsV4kd2DecVersion;

		V_DBG(VPU_DBG_VER_INFO,
			"%s: vpu-4k-d2_proc_get_version (handle: 0x%x)",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsHevcDecVersion, arg,
				sizeof(HEVC_GET_VERSION_t));
		else
			if (copy_from_user(&vdata->gsHevcDecVersion, arg,
				sizeof(HEVC_GET_VERSION_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsHevcDecVersion;

		V_DBG(VPU_DBG_VER_INFO,
			"%s: hevc_proc_get_version(handle: 0x%x)",
			vdata->misc->name,
			vdata->gsHevcDecInit_Info.gsHevcDecHandle);
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVp9DecVersion, arg,
				sizeof(VP9_GET_VERSION_t));
		else
			if (copy_from_user(&vdata->gsVp9DecVersion, arg,
				sizeof(VP9_GET_VERSION_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVp9DecVersion;

		V_DBG(VPU_DBG_VER_INFO,
			"%s: vp9_proc_get_version (handle: 0x%x)",
			vdata->misc->name,
			vdata->gsVp9DecInit_Info.gsVp9DecHandle);
		}
		break;
#endif
	default:
		{
		if (fromKernel)
			(void)memcpy(&vdata->gsVpuDecVersion, arg,
				sizeof(VDEC_GET_VERSION_t));
		else
			if (copy_from_user(&vdata->gsVpuDecVersion,
				arg, sizeof(VDEC_GET_VERSION_t)))
				return -EFAULT;

		pArgs = (void *)&vdata->gsVpuDecVersion;

		V_DBG(VPU_DBG_VER_INFO,
			"%s: handle: 0x%x",
			vdata->misc->name,
			vdata->gsVpuDecInit_Info.gsVpuDecHandle);
		}
		break;
	}

	_vdec_inter_add_list(vdata, VPU_CODEC_GET_VERSION, pArgs);

	return 0;
}

static int _vdec_result_buf_status(struct _vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		err("%s ::jpu not support this !!", vdata->misc->name);
		return -0x999;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		vdata->gsV4kd2DecBufStatus.result = vdata->gsCommDecResult;
		if (fromKernel)
			(void)memcpy(arg, &vdata->gsV4kd2DecBufStatus,
				sizeof(VPU_4K_D2_RINGBUF_GETINFO_t));
		else
			if (copy_to_user(arg, &vdata->gsV4kd2DecBufStatus,
				sizeof(VPU_4K_D2_RINGBUF_GETINFO_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		vdata->gsHevcDecBufStatus.result = vdata->gsCommDecResult;
		if (fromKernel)
			(void)memcpy(arg, &vdata->gsHevcDecBufStatus,
				sizeof(HEVC_RINGBUF_GETINFO_t));
		else
			if (copy_to_user(arg, &vdata->gsHevcDecBufStatus,
				sizeof(HEVC_RINGBUF_GETINFO_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		vdata->gsVp9DecBufStatus.result = vdata->gsCommDecResult;
		if (fromKernel)
			(void)memcpy(arg, &vdata->gsVp9DecBufStatus,
				sizeof(VP9_RINGBUF_GETINFO_t));
		else
			if (copy_to_user(arg, &vdata->gsVp9DecBufStatus,
				sizeof(VP9_RINGBUF_GETINFO_t)))
				return -EFAULT;
		}
		break;
#endif
	default:
		{
		vdata->gsVpuDecBufStatus.result = vdata->gsCommDecResult;
		if (fromKernel)
			(void)memcpy(arg, &vdata->gsVpuDecBufStatus,
				sizeof(VDEC_RINGBUF_GETINFO_t));
		else
			if (copy_to_user(arg, &vdata->gsVpuDecBufStatus,
				sizeof(VDEC_RINGBUF_GETINFO_t)))
				return -EFAULT;
		}
		break;
	}

	return 0;
}

static int _vdec_result_buf_fill(struct _vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		err("%s ::jpu not support this !!", vdata->misc->name);
		return -0x999;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		vdata->gsV4kd2DecBufFill.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsV4kd2DecBufFill,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_t));
		else
			if (copy_to_user(arg, &vdata->gsV4kd2DecBufFill,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		vdata->gsHevcDecBufFill.result = vdata->gsCommDecResult;
		if (fromKernel)
			(void)memcpy(arg, &vdata->gsHevcDecBufFill,
				sizeof(HEVC_RINGBUF_SETBUF_t));
		else
			if (copy_to_user(arg, &vdata->gsHevcDecBufFill,
				sizeof(HEVC_RINGBUF_SETBUF_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		err("%s: Not supported by G2V2 VP9 !!", vdata->misc->name);
		return -0x999;

#endif
	default:
		{
		vdata->gsVpuDecBufFill.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsVpuDecBufFill,
				sizeof(VDEC_RINGBUF_SETBUF_t));
		else
			if (copy_to_user(arg, &vdata->gsVpuDecBufFill,
				sizeof(VDEC_RINGBUF_SETBUF_t)))
				return -EFAULT;
		}
		break;
	}

	return 0;
}

static int _vdec_result_update_wp(struct _vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		err("%s ::jpu not support this !! ", vdata->misc->name);
		return -0x999;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		vdata->gsV4kd2DecUpdateWP.result = vdata->gsCommDecResult;
		if (fromKernel)
			(void)memcpy(arg, &vdata->gsV4kd2DecUpdateWP,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t));
		else
			if (copy_to_user(arg, &vdata->gsV4kd2DecUpdateWP,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		vdata->gsHevcDecUpdateWP.result = vdata->gsCommDecResult;
		if (fromKernel)
			(void)memcpy(arg, &vdata->gsHevcDecUpdateWP,
				sizeof(HEVC_RINGBUF_SETBUF_PTRONLY_t));
		else
			if (copy_to_user(arg, &vdata->gsHevcDecUpdateWP,
				sizeof(HEVC_RINGBUF_SETBUF_PTRONLY_t)))
				return -EFAULT;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		vdata->gsVp9DecUpdateWP.result = vdata->gsCommDecResult;
		if (fromKernel)
			(void)memcpy(arg, &vdata->gsVp9DecUpdateWP,
				sizeof(VP9_RINGBUF_SETBUF_PTRONLY_t));
		else
			if (copy_to_user(arg, &vdata->gsVp9DecUpdateWP,
				sizeof(VP9_RINGBUF_SETBUF_PTRONLY_t)))
				return -EFAULT;
		}
		break;
#endif
	default:
		{
		vdata->gsVpuDecUpdateWP.result = vdata->gsCommDecResult;
		if (fromKernel)
			(void)memcpy(arg, &vdata->gsVpuDecUpdateWP,
				sizeof(VDEC_RINGBUF_SETBUF_PTRONLY_t));
		else
			if (copy_to_user(arg, &vdata->gsVpuDecUpdateWP,
				sizeof(VDEC_RINGBUF_SETBUF_PTRONLY_t)))
				return -EFAULT;
		}
		break;
	}

	return 0;
}

static int _vdec_result_seq_header_ring(struct _vpu_decoder_data *vdata,
					void *arg, bool fromKernel)
{
	void *src = NULL;
	size_t copySize = 0;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		err("%s: Not supported by JPU!!", vdata->misc->name);
		return -0x999;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsV4kd2DecInOut_Info :
			(void *)&vdata->gsV4kd2DecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VPU_4K_D2_DECODE_t) :
			sizeof(VPU_4K_D2_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy)
			vdata->gsV4kd2DecInOut_Info.result
				= vdata->gsCommDecResult;
		else
			vdata->gsV4kd2DecSeqHeader_Info.result
				= vdata->gsCommDecResult;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsHevcDecInOut_Info :
			(void *)&vdata->gsHevcDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(HEVC_DECODE_t) :
			sizeof(HEVC_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy)
			vdata->gsHevcDecInOut_Info.result
					= vdata->gsCommDecResult;
		else
			vdata->gsHevcDecSeqHeader_Info.result
					= vdata->gsCommDecResult;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		err("%s: Not supported by G2V2 VP9 !!", vdata->misc->name);
		return -0x999;
#endif
	default:
		{
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsVpuDecInOut_Info :
			(void *)&vdata->gsVpuDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VDEC_DECODE_t) :
			sizeof(VDEC_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy)
			vdata->gsVpuDecInOut_Info.result
					= vdata->gsCommDecResult;
		else
			vdata->gsVpuDecSeqHeader_Info.result
					= vdata->gsCommDecResult;
		}
		break;
	}

	if (fromKernel)
		(void)memcpy(arg, src, copySize);
	else
		if (copy_to_user(arg, src, copySize))
			return -EFAULT;

	return 0;
}

static int _vdec_result_get_version(struct _vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		{
		vdata->gsJpuDecVersion.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsJpuDecVersion,
				sizeof(JPU_GET_VERSION_t));
		else
			if (copy_to_user(arg, &vdata->gsJpuDecVersion,
				sizeof(JPU_GET_VERSION_t)))
				return -EFAULT;


		if ((*(vdata->gsJpuDecVersion.pszVersion) == -1) ||
			(*(vdata->gsJpuDecVersion.pszBuildData) == -1))
			vdata->gsJpuDecVersion.result =
				RETCODE_INVALID_COMMAND;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC: case STD_VP9:
		{
		vdata->gsV4kd2DecVersion.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsV4kd2DecVersion,
				sizeof(VPU_4K_D2_GET_VERSION_t));
		else
			if (copy_to_user(arg, &vdata->gsV4kd2DecVersion,
				sizeof(VPU_4K_D2_GET_VERSION_t)))
				return -EFAULT;

		if ((*(vdata->gsV4kd2DecVersion.pszVersion) == -1) ||
			(*(vdata->gsV4kd2DecVersion.pszBuildData) == -1))
			vdata->gsV4kd2DecVersion.result =
				RETCODE_INVALID_COMMAND;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
		vdata->gsHevcDecVersion.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsHevcDecVersion,
				sizeof(HEVC_GET_VERSION_t));
		else
			if (copy_to_user(arg, &vdata->gsHevcDecVersion,
				sizeof(HEVC_GET_VERSION_t)))
				return -EFAULT;

		if ((*(vdata->gsHevcDecVersion.pszVersion) == -1) ||
			(*(vdata->gsHevcDecVersion.pszBuildData) == -1))
			vdata->gsHevcDecVersion.result =
				RETCODE_INVALID_COMMAND;
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
		vdata->gsVp9DecVersion.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsVp9DecVersion,
				sizeof(VP9_GET_VERSION_t));
		else
			sizeof(VP9_GET_VERSION_t)))
			if (copy_to_user(arg, &vdata->gsVp9DecVersion,
				return -EFAULT;

		if ((*(vdata->gsVp9DecVersion.pszVersion) == -1) ||
			(*(vdata->gsVp9DecVersion.pszBuildData) == -1))
			vdata->gsVp9DecVersion.result =
				RETCODE_INVALID_COMMAND;
		}
		break;
#endif
	default:
		{
		vdata->gsVpuDecVersion.result = vdata->gsCommDecResult;

		if (fromKernel)
			(void)memcpy(arg, &vdata->gsVpuDecVersion,
				sizeof(VDEC_GET_VERSION_t));
		else
			if (copy_to_user(arg, &vdata->gsVpuDecVersion,
				sizeof(VDEC_GET_VERSION_t)))
				return -EFAULT;

		if ((*(vdata->gsVpuDecVersion.pszVersion) == -1) ||
			(*(vdata->gsVpuDecVersion.pszBuildData) == -1))
			vdata->gsVpuDecVersion.result =
				RETCODE_INVALID_COMMAND;
		}
		break;
	}

	return 0;
}

static void _vdec_force_close(struct _vpu_decoder_data *vdata)
{
	int ret = 0;
	struct VpuList *cmd_list = &vdata->vdec_list[vdata->list_idx];

	vdata->list_idx =
		(vdata->list_idx + 1) == LIST_MAX ? (0) : (vdata->list_idx + 1);

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		{
			if (!jmgr_get_close(vdata->gsDecType)
				&& jmgr_get_alive()) {

				int max_count = 100;

				jmgr_process_ex(cmd_list,
					vdata->gsDecType,
					VPU_DEC_CLOSE,
					&ret);
				while (!jmgr_get_close(
					vdata->gsDecType)) {
					max_count--;
					msleep(20);

					if (max_count <= 0)
						break;
				}
			}
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC:
	case STD_VP9:
		{
			if (!vmgr_4k_d2_get_close(vdata->gsDecType)
				&& vmgr_4k_d2_get_alive()) {

				int max_count = 100;

				vmgr_4k_d2_process_ex(cmd_list,
					vdata->gsDecType,
					VPU_DEC_CLOSE,
					&ret);
				while (!vmgr_4k_d2_get_close(
						vdata->gsDecType)) {
					max_count--;
					msleep(20);

					if (max_count <= 0)
						break;
				}
			}
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		{
			if (!hmgr_get_close(vdata->gsDecType)
				&& hmgr_get_alive()) {
				int max_count = 100;

				hmgr_process_ex(cmd_list,
					vdata->gsDecType,
					VPU_DEC_CLOSE,
					&ret);
				while (!hmgr_get_close(
						vdata->gsDecType)) {
					max_count--;
					msleep(20);

					if (max_count <= 0)
						break;
				}
			}
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		{
			if (!vp9mgr_get_close(vdata->gsDecType)
				&& vp9mgr_get_alive()) {

				int max_count = 100;

				vp9mgr_process_ex(cmd_list, vdata->gsDecType,
					VPU_DEC_CLOSE, &ret);
				while (!vp9mgr_get_close(vdata->gsDecType)) {
					max_count--;
					msleep(20);

					if (max_count <= 0)
						break;
				}
			}
		}
		break;
#endif
	default:
		{
			if (!vmgr_get_close(vdata->gsDecType)
				&& vmgr_get_alive()) {

				int max_count = 100;

				vmgr_process_ex(cmd_list, vdata->gsDecType,
					VPU_DEC_CLOSE, &ret);

				while (!vmgr_get_close(vdata->gsDecType)) {
					max_count--;
					msleep(20);

					if (max_count <= 0)
						break;
				}
			}
		}
		break;
	}
}

static int _vdev_init(struct _vpu_decoder_data *vdata,
					void *arg, bool fromKernel)
{
	if (vdata->vComm_data.dev_opened > 1) {

		switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
		case STD_MJPG:
			err(
				"Jpu(%s) has been already opened. Maybe there is exceptional stop!! Mgr(%d)/Dec(%d)",
				vdata->misc->name,
				jmgr_get_alive(),
				jmgr_get_close(vdata->gsDecType));
			break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
		case STD_HEVC: case STD_VP9:
			err(
				"VPU-4K-D2(%s) has been already opened. Maybe there is exceptional stop!! Mgr(%d)/Dec(%d)",
				vdata->misc->name,
				vmgr_4k_d2_get_alive(),
				vmgr_4k_d2_get_close(vdata->gsDecType));
			break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
		case STD_HEVC:
			err(
				"Hevc(%s) has been already opened. Maybe there is exceptional stop!! Mgr(%d)/Dec(%d)",
				vdata->misc->name,
				hmgr_get_alive(),
				hmgr_get_close(vdata->gsDecType));
			break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
		case STD_HEVC:
			err(
				"Vp9(%s) has been already opened. Maybe there is exceptional stop!! Mgr(%d)/Dec(%d)",
				vdata->misc->name,
				vp9mgr_get_alive(),
				vp9mgr_get_close(vdata->gsDecType));
			break;
#endif
		default:
			err(
				"Vpu(%s) has been already opened. Maybe there is exceptional stop!! Mgr(%d)/Dec(%d)",
				vdata->misc->name,
				vmgr_get_alive(),
				vmgr_get_close(vdata->gsDecType));
			break;
		}

		_vdec_force_close(vdata);

		switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
		case STD_MJPG:
			jmgr_set_close(vdata->gsDecType, 1, 1);
			break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
		case STD_HEVC: case STD_VP9:
			vmgr_4k_d2_set_close(vdata->gsDecType, 1, 1);
			break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
		case STD_HEVC:
			hmgr_set_close(vdata->gsDecType, 1, 1);
			break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
		case STD_VP9:
			vp9mgr_set_close(vdata->gsDecType, 1, 1);
			break;
#endif
		default:
			vmgr_set_close(vdata->gsDecType, 1, 1);
			break;
		}

		vdata->vComm_data.dev_opened--;
	}

	vdata->gsCodecType = -1;
	if (fromKernel)
		(void)memcpy(&vdata->gsCodecType, arg, sizeof(int));
	else
		if (copy_from_user(&vdata->gsCodecType, arg, sizeof(int)))
			return -EFAULT;

	switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		memset(&vdata->gsJpuDecInit_Info, 0x00,
			sizeof(JDEC_INIT_t));
		memset(&vdata->gsJpuDecBuffer_Info, 0x00,
			sizeof(JPU_SET_BUFFER_t));
		memset(&vdata->gsJpuDecInOut_Info, 0x00,
			sizeof(JPU_DECODE_t));
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC:
	case STD_VP9:
		memset(&vdata->gsV4kd2DecInit_Info, 0x00,
			sizeof(VPU_4K_D2_INIT_t));
		memset(&vdata->gsV4kd2DecSeqHeader_Info, 0x00,
			sizeof(VPU_4K_D2_SEQ_HEADER_t));
		memset(&vdata->gsV4kd2DecBuffer_Info, 0x00,
			sizeof(VPU_4K_D2_SET_BUFFER_t));
		memset(&vdata->gsV4kd2DecInOut_Info, 0x00,
			sizeof(VPU_4K_D2_DECODE_t));
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		memset(&vdata->gsHevcDecInit_Info, 0x00,
			sizeof(HEVC_INIT_t));
		memset(&vdata->gsHevcDecSeqHeader_Info, 0x00,
			sizeof(HEVC_SEQ_HEADER_t));
		memset(&vdata->gsHevcDecBuffer_Info, 0x00,
			sizeof(HEVC_SET_BUFFER_t));
		memset(&vdata->gsHevcDecInOut_Info, 0x00,
			sizeof(HEVC_DECODE_t));
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		memset(&vdata->gsVp9DecInit_Info, 0x00,
			sizeof(VP9_INIT_t));
		memset(&vdata->gsVp9DecSeqHeader_Info, 0x00,
			sizeof(VP9_SEQ_HEADER_t));
		memset(&vdata->gsVp9DecBuffer_Info, 0x00,
			sizeof(VP9_SET_BUFFER_t));
		memset(&vdata->gsVp9DecInOut_Info, 0x00,
			sizeof(VP9_DECODE_t));
		break;
#endif
	default:
		memset(&vdata->gsVpuDecInit_Info, 0x00,
			sizeof(VDEC_INIT_t));
		memset(&vdata->gsVpuDecSeqHeader_Info, 0x00,
			sizeof(VDEC_SEQ_HEADER_t));
		memset(&vdata->gsVpuDecBuffer_Info, 0x00,
			sizeof(VDEC_SET_BUFFER_t));
		memset(&vdata->gsVpuDecInOut_Info, 0x00,
			sizeof(VDEC_DECODE_t));
		break;
	}

	return 0;
}

int vdec_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct _vpu_decoder_data *vdata = dev_get_drvdata(misc->parent);

#if defined(CONFIG_PMAP)
	if (range_is_allowed(vma->vm_pgoff,
		vma->vm_end - vma->vm_start) < 0) {
		V_DBG(VPU_DBG_ERROR,
			"%s :: mmap: this address is not allowed",
			vdata->misc->name);
		return -EAGAIN;
	}
#endif

	vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
	if (remap_pfn_range(vma,
		vma->vm_start,
		vma->vm_pgoff,
		vma->vm_end - vma->vm_start,
		vma->vm_page_prot)) {
		V_DBG(VPU_DBG_ERROR,
			"%s :: mmap :: remap_pfn_range failed",
			vdata->misc->name);
		return -EAGAIN;
	}

	vma->vm_ops     = NULL;
	vma->vm_flags   |= VM_IO;
	vma->vm_flags   |= VM_DONTEXPAND | VM_PFNMAP;

	return 0;
}

unsigned int vdec_poll(struct file *filp, poll_table *wait)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct _vpu_decoder_data *vdata = dev_get_drvdata(misc->parent);

	if (vdata == NULL)
		return POLLERR | POLLNVAL;

	if (vdata->vComm_data.count == 0)
		poll_wait(filp, &(vdata->vComm_data.wq), wait);

	if (vdata->vComm_data.count > 0) {
		vdata->vComm_data.count--;
		return POLLIN;
	}

	return 0;
}

long vdec_poll_2(struct file *filp, int timeout_ms)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct _vpu_decoder_data *vdata = dev_get_drvdata(misc->parent);

	if (vdata == NULL)
		return POLLERR | POLLNVAL;

	(void) wait_event_interruptible_timeout(vdata->vComm_data.wq,
				vdata->vComm_data.count > 0,
				msecs_to_jiffies(timeout_ms));

	if (vdata->vComm_data.count > 0) {
		vdata->vComm_data.count--;
		return POLLIN;
	}

	return 0;
}
EXPORT_SYMBOL(vdec_poll_2);

static int _vdec_cmd_open(struct _vpu_decoder_data *vdata, char *str)
{
	V_DBG(VPU_DBG_SEQUENCE,
		"======> %s :: _vdec_%s_open(%d)!!",
		vdata->misc->name, str, vdata->vComm_data.dev_opened);

	if (vmem_get_free_memory(vdata->gsDecType) == 0) {
		V_DBG(VPU_DBG_ERROR,
			"VPU %s: Couldn't open device because of no-reserved memory.",
			vdata->misc->name);
		return -ENOMEM;
	}

	V_DBG(VPU_DBG_SEQUENCE,
		"======> %s :: _vdec_%s_open(%d)!!",
		vdata->misc->name, str, vdata->vComm_data.dev_opened);

	if (vdata->vComm_data.dev_opened == 0)
		vdata->vComm_data.count = 0;
	vdata->vComm_data.dev_opened++;

	return 0;
}

static int _vdec_cmd_release(struct _vpu_decoder_data *vdata, char *str)
{
	V_DBG(VPU_DBG_CLOSE,
		"======> %s :: _vdec_%s_release In(%d)!!",
		vdata->misc->name, str, vdata->vComm_data.dev_opened);

	if (vdata->vComm_data.dev_opened > 0)
		vdata->vComm_data.dev_opened--;

	if (vdata->vComm_data.dev_opened == 0) {

		vdec_clear_instance(vdata->gsDecType-VPU_DEC);
		_vdec_force_close(vdata);

		switch (vdata->gsCodecType) {

#ifdef CONFIG_SUPPORT_TCC_JPU
		case STD_MJPG:
			jmgr_set_close(vdata->gsDecType, 1, 1);
			break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
		case STD_HEVC:
		case STD_VP9:
			vmgr_4k_d2_set_close(vdata->gsDecType, 1, 1);
			break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
		case STD_HEVC:
			hmgr_set_close(vdata->gsDecType, 1, 1);
			break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
		case STD_VP9:
			vp9mgr_set_close(vdata->gsDecType, 1, 1);
			break;
#endif
		default:
			vmgr_set_close(vdata->gsDecType, 1, 1);
			break;
		}
	}

	V_DBG(VPU_DBG_CLOSE,
		"======> %s :: _vdec_%s_release Out(%d)!! ",
		vdata->misc->name, str, vdata->vComm_data.dev_opened);

	return 0;
}

long vdec_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct _vpu_decoder_data *vdata = dev_get_drvdata(misc->parent);

	if (cmd != DEVICE_INITIALIZE
		&& cmd != DEVICE_INITIALIZE_KERNEL
		&& cmd != V_DEC_GENERAL_RESULT
		&& cmd != V_DEC_GENERAL_RESULT_KERNEL
		&& vmgr_get_alive() == 0
	#ifdef CONFIG_SUPPORT_TCC_JPU
		&& jmgr_get_alive() == 0
	#endif
	#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
		&& vmgr_4k_d2_get_alive() == 0
	#endif
	#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
		&& hmgr_get_alive() == 0
	#endif
	#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
		&& vp9mgr_get_alive() == 0
	#endif
	) {
		V_DBG(VPU_DBG_ERROR,
			"vdec ioctl(name:%s, cmd: 0x%x) Vpu Manager aliveness error (v/4k-d2/h/j/v9 = %d/%d/%d/%d/%d) !!!",
				vdata->misc->name,
				cmd,
				vmgr_get_alive(),
			#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
				vmgr_4k_d2_get_alive(),
			#else
				0,
			#endif
			#if defined(CONFIG_SUPPORT_TCC_WAVE410_HEVC)
				hmgr_get_alive(),
			#else
				0,
			#endif
			#if defined(CONFIG_SUPPORT_TCC_JPU)
				jmgr_get_alive(),
			#else
				0,
			#endif
			#if defined(CONFIG_SUPPORT_TCC_G2V2_VP9)
				vp9mgr_get_alive()
			#else
				0
			#endif
		);

		return -EPERM;
	}

	switch (cmd) {

	case DEVICE_INITIALIZE:
	case DEVICE_INITIALIZE_KERNEL:
		return _vdev_init(vdata, (void *)arg,
			cmd == DEVICE_INITIALIZE_KERNEL);

	case V_DEC_INIT:
	case V_DEC_INIT_KERNEL:
		return _vdec_proc_init(vdata, (void *)arg,
			cmd == V_DEC_INIT_KERNEL);

	case V_DEC_SEQ_HEADER:
	case V_DEC_SEQ_HEADER_KERNEL:
		return _vdec_proc_seq_header(vdata, (void *)arg, cmd ==
					V_DEC_SEQ_HEADER_KERNEL);

	case V_DEC_REG_FRAME_BUFFER:
	case V_DEC_REG_FRAME_BUFFER_KERNEL:
		return _vdec_proc_reg_framebuffer(vdata, (void *)arg,
				cmd == V_DEC_REG_FRAME_BUFFER_KERNEL);

	case V_DEC_DECODE:
	case V_DEC_DECODE_KERNEL:
		return _vdec_proc_decode(vdata, (void *)arg,
				cmd == V_DEC_DECODE_KERNEL);

	case V_DEC_GET_OUTPUT_INFO:
	case V_DEC_GET_OUTPUT_INFO_KERNEL:
		return _vdec_get_decode_output(vdata, (void *)arg,
				cmd == V_DEC_GET_OUTPUT_INFO_KERNEL);

	case V_DEC_BUF_FLAG_CLEAR:
	case V_DEC_BUF_FLAG_CLEAR_KERNEL:
		return _vdec_proc_clear_bufferflag(vdata, (void *)arg,
				cmd == V_DEC_BUF_FLAG_CLEAR_KERNEL);

	case V_DEC_CLOSE:
	case V_DEC_CLOSE_KERNEL:
		return _vdec_proc_exit(vdata, (void *)arg,
				cmd == V_DEC_CLOSE_KERNEL);

	case V_DEC_FLUSH_OUTPUT:
	case V_DEC_FLUSH_OUTPUT_KERNEL:
		return _vdec_proc_flush(vdata, (void *)arg,
				cmd == V_DEC_FLUSH_OUTPUT_KERNEL);

	case V_DEC_SWRESET:
	case V_DEC_SWRESET_KERNEL:
		return _vdec_proc_swreset(vdata,
				cmd == V_DEC_SWRESET_KERNEL);

	case V_GET_RING_BUFFER_STATUS:
	case V_GET_RING_BUFFER_STATUS_KERNEL:
		return _vdec_proc_buf_status(vdata, (void *)arg,
				cmd == V_GET_RING_BUFFER_STATUS_KERNEL);

	case V_FILL_RING_BUFFER_AUTO:
	case V_FILL_RING_BUFFER_AUTO_KERNEL:
		return _vdec_proc_buf_fill(vdata, (void *)arg,
				cmd == V_FILL_RING_BUFFER_AUTO_KERNEL);

	case V_DEC_UPDATE_RINGBUF_WP:
	case V_DEC_UPDATE_RINGBUF_WP_KERNEL:
		return _vdec_proc_update_wp(vdata, (void *)arg,
				cmd == V_DEC_UPDATE_RINGBUF_WP_KERNEL);

	case V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY:
	case V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_KERNEL:
		return _vdec_proc_seq_header_ring(vdata, (void *)arg, cmd ==
		V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_KERNEL);

	case V_GET_VPU_VERSION:
	case V_GET_VPU_VERSION_KERNEL:
		return _vdec_proc_get_version(vdata, (void *)arg,
				cmd == V_GET_VPU_VERSION_KERNEL);

	case V_GET_RING_BUFFER_STATUS_RESULT:
	case V_GET_RING_BUFFER_STATUS_RESULT_KERNEL:
		return _vdec_result_buf_status(vdata, (void *)arg,
			cmd == V_GET_RING_BUFFER_STATUS_RESULT_KERNEL);

	case V_FILL_RING_BUFFER_AUTO_RESULT:
	case V_FILL_RING_BUFFER_AUTO_RESULT_KERNEL:
		return _vdec_result_buf_fill(vdata, (void *)arg,
			cmd == V_FILL_RING_BUFFER_AUTO_RESULT_KERNEL);

	case V_DEC_UPDATE_RINGBUF_WP_RESULT:
	case V_DEC_UPDATE_RINGBUF_WP_RESULT_KERNEL:
		return _vdec_result_update_wp(vdata, (void *)arg,
			cmd == V_DEC_UPDATE_RINGBUF_WP_RESULT_KERNEL);

	case V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_RESULT:
	case V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_RESULT_KERNEL:
		return _vdec_result_seq_header_ring(vdata, (void *)arg, cmd ==
		V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_RESULT_KERNEL);

	case V_GET_VPU_VERSION_RESULT:
	case V_GET_VPU_VERSION_RESULT_KERNEL:
		return _vdec_result_get_version(vdata, (void *)arg,
			cmd == V_GET_VPU_VERSION_RESULT_KERNEL);

	case V_DEC_GET_INFO:
	case V_DEC_GET_INFO_KERNEL:
	case V_DEC_REG_FRAME_BUFFER2:
	case V_DEC_REG_FRAME_BUFFER2_KERNEL:
		V_DBG(VPU_DBG_ERROR,
			"%s ::Not implemented ioctl - %d !!!",
			vdata->misc->name, cmd);
	break;

	case V_DEC_ALLOC_MEMORY:
	case V_DEC_ALLOC_MEMORY_KERNEL:
	{
		int ret;
		MEM_ALLOC_INFO_t alloc_info;

		if (cmd == V_DEC_ALLOC_MEMORY_KERNEL)
			(void)memcpy(&alloc_info, (MEM_ALLOC_INFO_t *)arg,
				sizeof(MEM_ALLOC_INFO_t));
		else
			if (copy_from_user(&alloc_info, (MEM_ALLOC_INFO_t *)arg,
				sizeof(MEM_ALLOC_INFO_t)))
				return -EFAULT;

		ret = vmem_proc_alloc_memory(vdata->gsCodecType,
				&alloc_info, vdata->gsDecType);

		if (cmd == V_DEC_ALLOC_MEMORY_KERNEL)
			(void)memcpy((MEM_ALLOC_INFO_t *)arg,
				&alloc_info, sizeof(MEM_ALLOC_INFO_t));
		else
			if (copy_to_user((MEM_ALLOC_INFO_t *)arg,
				&alloc_info, sizeof(MEM_ALLOC_INFO_t)))
				return -EFAULT;

		return ret;
	}
	break;

	case V_DEC_ALLOC_MEMORY_EX: // 32bit user space, 64bit kernel space
	{
		int ret;
		MEM_ALLOC_INFO_t alloc_info;
		MEM_ALLOC_INFO_EX_t alloc_info_ex;

		if (copy_from_user(&alloc_info_ex,
				(MEM_ALLOC_INFO_EX_t *)arg,
				sizeof(MEM_ALLOC_INFO_EX_t)))
			return -EFAULT;

		memcpy(&alloc_info, &alloc_info_ex, sizeof(MEM_ALLOC_INFO_t));

		ret = vmem_proc_alloc_memory(vdata->gsCodecType,
			&alloc_info, vdata->gsDecType);

		memcpy(&alloc_info_ex, &alloc_info,
			sizeof(MEM_ALLOC_INFO_EX_t));
		alloc_info_ex.kernel_remap_addr =
			(unsigned long long)(unsigned long)
				alloc_info.kernel_remap_addr;

		/* Always returned zero value in case of framebuffer request */
		if (alloc_info.buffer_type != BUFFER_FRAMEBUFFER)
			dprintk("[buffer_type: %d] kernel_remap_addr (%p)",
				(int) alloc_info.buffer_type,
				alloc_info.kernel_remap_addr);

		if (copy_to_user((MEM_ALLOC_INFO_EX_t *)arg,
			&alloc_info_ex, sizeof(MEM_ALLOC_INFO_EX_t)))
			return -EFAULT;

		return ret;
	}
	break;

	case V_DEC_FREE_MEMORY:
	case V_DEC_FREE_MEMORY_KERNEL:
		return vmem_proc_free_memory(vdata->gsDecType);

	case VPU_GET_FREEMEM_SIZE:
	case VPU_GET_FREEMEM_SIZE_KERNEL:
	{
		int szFreeMem = 0;

		szFreeMem = vmem_get_free_memory(vdata->gsDecType);

		if (cmd == VPU_GET_FREEMEM_SIZE_KERNEL)
			(void)memcpy((unsigned int *)arg,
				&szFreeMem,
				sizeof(szFreeMem));
		else
			if (copy_to_user((unsigned int *)arg,
				&szFreeMem, sizeof(szFreeMem)))
				return -EFAULT;
	}
	break;

	case V_DEC_GENERAL_RESULT:
	case V_DEC_GENERAL_RESULT_KERNEL:
		return _vdec_result_general(vdata, (int *)arg,
			cmd == V_DEC_GENERAL_RESULT_KERNEL);

	case V_DEC_INIT_RESULT:
	case V_DEC_INIT_RESULT_KERNEL:
		return _vdec_result_init(vdata, (void *)arg,
			cmd == V_DEC_INIT_RESULT_KERNEL);

	case V_DEC_SEQ_HEADER_RESULT:
	case V_DEC_SEQ_HEADER_RESULT_KERNEL:
		return _vdec_result_seq_header(vdata, (void *)arg,
			cmd == V_DEC_SEQ_HEADER_RESULT_KERNEL);

	case V_DEC_DECODE_RESULT:
	case V_DEC_DECODE_RESULT_KERNEL:
		return _vdec_result_decode(vdata, (void *)arg,
			cmd == V_DEC_DECODE_RESULT_KERNEL);

	case V_DEC_GET_OUTPUT_INFO_RESULT:
	case V_DEC_GET_OUTPUT_INFO_RESULT_KERNEL:
		return _vdec_result_decode(vdata, (void *)arg,
			cmd == V_DEC_GET_OUTPUT_INFO_RESULT_KERNEL);

	case V_DEC_FLUSH_OUTPUT_RESULT:
	case V_DEC_FLUSH_OUTPUT_RESULT_KERNEL:
		return _vdec_result_flush(vdata, (void *)arg,
			cmd == V_DEC_FLUSH_OUTPUT_RESULT_KERNEL);

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	case V_DEC_TRY_OPEN_DEV:
	case V_DEC_TRY_OPEN_DEV_KERNEL:
			_vdec_cmd_open(vdata, "cmd");
			break;

	case V_DEC_TRY_CLOSE_DEV:
	case V_DEC_TRY_CLOSE_DEV_KERNEL:
			_vdec_cmd_release(vdata, "cmd");
			break;
#endif

	default:
		err("[%s] Unsupported ioctl[%d]!!!",
			vdata->misc->name, cmd);
		break;
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long vdec_compat_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	return vdec_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

int vdec_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct _vpu_decoder_data *vdata = dev_get_drvdata(misc->parent);

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	vdata->vComm_data.dev_file_opened++;

	V_DBG(VPU_DBG_SEQUENCE, "%s :: open Out(%d)!!",
		vdata->misc->name, vdata->vComm_data.dev_file_opened);
#else
	_vdec_cmd_open(vdata, "file");
#endif

	return 0;
}

int vdec_release(struct inode *inode, struct file *filp)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct _vpu_decoder_data *vdata = dev_get_drvdata(misc->parent);

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	vdata->vComm_data.dev_file_opened--;

	V_DBG(VPU_DBG_CLOSE, "%s :: release Out(%d)!!",
		vdata->misc->name, vdata->vComm_data.dev_file_opened);
#else
	_vdec_cmd_release(vdata, "file");
#endif

	return 0;
}

static const struct file_operations vdev_dec_fops = {
	.owner              = THIS_MODULE,
	.open               = vdec_open,
	.release            = vdec_release,
	.mmap               = vdec_mmap,
	.unlocked_ioctl     = vdec_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl       = vdec_compat_ioctl,
#endif
	.poll               = vdec_poll,
};

int vdec_probe(struct platform_device *pdev)
{
	struct _vpu_decoder_data *vdata;
	int ret = -ENODEV;

	vdata = kzalloc(sizeof(struct _vpu_decoder_data), GFP_KERNEL);
	if (!vdata)
		return -ENOMEM;

	vdata->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (!vdata->misc) {
		ret = -ENOMEM;
		goto err_misc_alloc;
	}

	vdata->misc->minor = MISC_DYNAMIC_MINOR;
	vdata->misc->fops = &vdev_dec_fops;
	vdata->misc->name = pdev->name;
	vdata->misc->parent = &pdev->dev;

	vdata->gsDecType = pdev->id;
	memset(&vdata->vComm_data, 0, sizeof(struct _vpu_dec_data_t));
	spin_lock_init(&vdata->vComm_data.lock);
	init_waitqueue_head(&vdata->vComm_data.wq);

	if (misc_register(vdata->misc)) {
		pr_info("VPU %s: Couldn't register device.", pdev->name);
		ret = -EBUSY;
		goto err_misc_register;
	}

	platform_set_drvdata(pdev, vdata);
	pr_info("VPU %s Driver(id:%d) Initialized.", pdev->name, pdev->id);

	return 0;

err_misc_register:
	kfree(vdata->misc);
err_misc_alloc:
	kfree(vdata);

	return ret;
}
EXPORT_SYMBOL(vdec_probe);

int vdec_remove(struct platform_device *pdev)
{
	struct _vpu_decoder_data *vdata =
			(struct _vpu_decoder_data *) platform_get_drvdata(pdev);

	misc_deregister(vdata->misc);

	kfree(vdata->misc);
	kfree(vdata);

	return 0;
}
EXPORT_SYMBOL(vdec_remove);

#endif /*CONFIG_VDEC_CNT_X*/

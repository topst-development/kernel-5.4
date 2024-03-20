// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *      tccvin_v4l2.c  --  Telechips Video-Input Path Driver
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2020


 *   Description : v4l2 interface


 *****************************************************************************/

#include <linux/version.h>
#include <linux/compat.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/wait.h>
#include <linux/atomic.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#include "tccvin_video.h"

/* Rounds an integer value up to the next multiple of num */
#define ROUND_UP_2(num)		(add_u32((num), 1U) & ~1U)
#define ROUND_UP_4(num)		(add_u32((num), 3U) & ~3U)

/* coverity[HIS_metric_violation : FALSE] */
static inline struct tccvin_stream *
queue_to_stream(struct tccvin_queue *queue)
{
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	return container_of(queue, struct tccvin_stream, queue);
}

static inline struct tccvin_buffer *
vb2_v4l2_buf_to_buf(struct vb2_v4l2_buffer *buf)
{
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	return container_of(buf, struct tccvin_buffer, buf);
}

/*
 * Return all queued buffers to videobuf2 in the requested state.
 *
 * This function must be called with the queue spinlock held.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void tccvin_queue_return_buffers(struct tccvin_queue *queue,
	enum tccvin_buffer_state state)
{
	enum vb2_buffer_state			vb2_state;

	vb2_state = (state == TCCVIN_BUF_STATE_ERROR)
		? VB2_BUF_STATE_ERROR : VB2_BUF_STATE_QUEUED;

	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	while (list_empty(&queue->buf_list) != 1) {
		struct tccvin_buffer			*buf		= NULL;

		/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
		/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
		/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
		/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
		/* coverity[cert_arr39_c_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		buf = list_first_entry(&queue->buf_list, struct tccvin_buffer, entry);
		list_del(&buf->entry);
		vb2_buffer_done(&buf->buf.vb2_buf, vb2_state);
	}
}

/* -----------------------------------------------------------------------------
 * videobuf2 queue operations
 */

static int tccvin_queue_setup(struct vb2_queue *vq,
	/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	unsigned int *nbuffers, unsigned int *nplanes,
	/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	unsigned int sizes[], struct device *alloc_devs[])
{
	struct tccvin_queue			*queue		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	const struct device			*dev		= NULL;
	struct v4l2_pix_format_mplane		pix_mp		= { 0, };
	unsigned int				idxPlane	= 0;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	queue		= (struct tccvin_queue *)vb2_get_drv_priv(vq);
	vstream		= queue_to_stream(queue);
	dev		= stream_to_device(vstream);

	/* get current pixelformat, width and height */
	(void)memcpy(&pix_mp, &vstream->format.fmt.pix_mp, sizeof(pix_mp));

	/* fill other fileds */
	(int)v4l2_fill_pixfmt_mp(&pix_mp, pix_mp.pixelformat, pix_mp.width, pix_mp.height);

	/* update num_planes */
	if (*nplanes == 0U) {
		/* The initial value of nplanes is 0,
		 * so set it as the initial format's num_planes.
		 */
		*nplanes = pix_mp.num_planes;
		logd(dev, "num_planes: %u\n", *nplanes);
	}

	/* update sizeimage */
	for (idxPlane = 0; idxPlane < *nplanes; idxPlane++) {
		sizes[idxPlane] = pix_mp.plane_fmt[idxPlane].sizeimage;
		logd(dev, "plane[%u].sizeimage: %u\n", idxPlane, sizes[idxPlane]);
	}

	return ret;
}

/* coverity[HIS_metric_violation : FALSE] */
static int tccvin_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer			*vbuf		= NULL;
	struct tccvin_queue			*queue		= NULL;
	const struct tccvin_buffer		*buf		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	const struct device			*dev		= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	vbuf		= to_vb2_v4l2_buffer(vb);
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	queue		= (struct tccvin_queue *)vb2_get_drv_priv(vb->vb2_queue);
	buf		= vb2_v4l2_buf_to_buf(vbuf);
	vstream		= queue_to_stream(queue);
	dev		= stream_to_device(vstream);

	if (unlikely((queue->flags & (unsigned int)TCCVIN_QUEUE_DISCONNECTED) != 0U)) {
		/* queue->flags is wrong */
		ret = -ENODEV;
	}

	return ret;
}

/* coverity[HIS_metric_violation : FALSE] */
static void tccvin_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer			*vbuf		= NULL;
	struct tccvin_queue			*queue		= NULL;
	struct tccvin_buffer			*buf		= NULL;
	unsigned long				flags;

	/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
	/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	vbuf		= to_vb2_v4l2_buffer(vb);
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	queue		= (struct tccvin_queue *)vb2_get_drv_priv(vb->vb2_queue);
	buf		= vb2_v4l2_buf_to_buf(vbuf);

	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
	spin_lock_irqsave(&queue->slock, flags);
	if (likely((queue->flags & (unsigned int)TCCVIN_QUEUE_DISCONNECTED) == 0U)) {
		list_add_tail(&buf->entry, &queue->buf_list);
	} else {
		/* If the device is disconnected return the buffer to userspace
		 * directly. The next QBUF call will fail with -ENODEV.
		 */
		vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
	}

	spin_unlock_irqrestore(&queue->slock, flags);
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
static int tccvin_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct tccvin_queue			*queue		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	queue		= (struct tccvin_queue *)vb2_get_drv_priv(vq);
	vstream		= queue_to_stream(queue);

	ret = tccvin_video_streamon(vstream);
	if (ret != 0) {
		spin_lock_irq(&queue->slock);
		tccvin_queue_return_buffers(queue, TCCVIN_BUF_STATE_QUEUED);
		spin_unlock_irq(&queue->slock);
	}

	return ret;
}

static void tccvin_stop_streaming(struct vb2_queue *vq)
{
	struct tccvin_queue			*queue		= NULL;
	struct tccvin_stream			*vstream	= NULL;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	queue		= (struct tccvin_queue *)vb2_get_drv_priv(vq);
	vstream		= queue_to_stream(queue);

	(void)tccvin_video_streamoff(vstream);

	spin_lock_irq(&queue->slock);
	tccvin_queue_return_buffers(queue, TCCVIN_BUF_STATE_ERROR);
	spin_unlock_irq(&queue->slock);
}

static const struct vb2_ops tccvin_qops = {
	.queue_setup		= tccvin_queue_setup,
	.buf_prepare		= tccvin_buf_prepare,
	.buf_queue		= tccvin_buf_queue,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
	.start_streaming	= tccvin_start_streaming,
	.stop_streaming		= tccvin_stop_streaming,
};

/* coverity[HIS_metric_violation : FALSE] */
int tccvin_v4l2_init_queue(struct tccvin_queue *queue,
	bool drop_corrupted)
{
	const struct tccvin_stream		*vstream	= NULL;
	const struct device			*dev		= NULL;
	struct vb2_queue			*q		= NULL;
	int					ret		= 0;

	vstream		= queue_to_stream(queue);
	dev		= stream_to_device(vstream);
	q		= &queue->queue;

	/* init vb2_queue */
	q->type			= (unsigned int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes		= (unsigned int)VB2_MMAP |
				  (unsigned int)VB2_DMABUF;
	q->bidirectional	= 0;
	q->is_output		= (unsigned char)DMA_TO_DEVICE;
	q->drv_priv		= queue;
	q->buf_struct_size	= (unsigned int)sizeof(struct tccvin_buffer);
	q->ops			= &tccvin_qops;
	q->mem_ops		= &vb2_dma_contig_memops;
	q->timestamp_flags	= (unsigned int)V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC |
				  (unsigned int)V4L2_BUF_FLAG_TSTAMP_SRC_SOE;
	q->dev			= &vstream->tdev->pdev->dev;
	q->min_buffers_needed	= 1;

	ret = vb2_queue_init(&queue->queue);
	if (ret != 0) {
		/* failure of queue init */
		ret = -1;
	} else {
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_17_7_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		spin_lock_init(&queue->slock);
		INIT_LIST_HEAD(&queue->buf_list);
		queue->flags = drop_corrupted ? (unsigned int)TCCVIN_QUEUE_DROP_CORRUPTED : 0U;
		logd(dev, "drop_corrupted: %d, queue->flags: 0x%08x",
			drop_corrupted, queue->flags);
	}

	return ret;
}

void tccvin_get_dma_addrs(const struct tccvin_stream *vstream,
	struct vb2_buffer *vb, unsigned int addrs[])
{
	const struct device			*dev		= NULL;
	dma_addr_t				dma_addrs[MAX_PLANES];
	unsigned int				idxpln		= 0;

	dev		= stream_to_device(vstream);

	switch (vb->memory) {
	case (unsigned int)VB2_MEMORY_MMAP:
		for (idxpln = 0; idxpln < vb->num_planes; idxpln++) {
			dma_addrs[idxpln] = vb2_dma_contig_plane_dma_addr(vb, idxpln);
#if defined(CONFIG_ARM64)
			addrs[idxpln] = u64_to_u32(dma_addrs[idxpln]);
#else
			addrs[idxpln] = (unsigned int)dma_addrs[idxpln];
#endif//defined(CONFIG_ARM64)
		}
		break;

	default:
		loge(dev, "memory(0x%08x) is not supported\n", vb->memory);
		break;
	}
}

void tccvin_print_dma_addrs(const struct tccvin_stream *vstream,
	const struct vb2_buffer *vb, const unsigned int addrs[])
{
	const struct device			*dev		= NULL;
	unsigned int				idxpln		= 0;

	dev		= stream_to_device(vstream);

	/* misra_c_2012_rule_2_7_violation */
	(void)addrs;

	switch (vb->memory) {
	case (unsigned int)VB2_MEMORY_MMAP:
		for (idxpln = 0; idxpln < vb->num_planes; idxpln++) {
			/* dma addr */
			logd(dev, "planes[%u]: 0x%08x\n", idxpln, addrs[idxpln]);
		}
		break;

	default:
		loge(dev, "memory(0x%08x) is not supported\n", vb->memory);
		break;
	}
}

static unsigned int tccvin_get_mbus_code_by_pixelformat(unsigned int pixelformat)
{
	struct tccvin_mbus_fmt {
		unsigned int			pixelformat;
		unsigned int			mbus_fmt;
	};
	const struct tccvin_mbus_fmt		tccvin_mbus_fmt_list[] = {
		{ .pixelformat = V4L2_PIX_FMT_RGB24,	.mbus_fmt = MEDIA_BUS_FMT_RGB888_1X24	},
		{ .pixelformat = V4L2_PIX_FMT_RGB32,	.mbus_fmt = MEDIA_BUS_FMT_ARGB8888_1X32	},
		{ .pixelformat = V4L2_PIX_FMT_UYVY,	.mbus_fmt = MEDIA_BUS_FMT_UYVY8_1X16	},
		{ .pixelformat = V4L2_PIX_FMT_VYUY,	.mbus_fmt = MEDIA_BUS_FMT_VYUY8_1X16	},
		{ .pixelformat = V4L2_PIX_FMT_YUYV,	.mbus_fmt = MEDIA_BUS_FMT_YUYV8_1X16	},
		{ .pixelformat = V4L2_PIX_FMT_YVYU,	.mbus_fmt = MEDIA_BUS_FMT_YVYU8_1X16	},
		{ .pixelformat = V4L2_PIX_FMT_YUV422P,	.mbus_fmt = MEDIA_BUS_FMT_YUYV8_2X8	},
		{ .pixelformat = V4L2_PIX_FMT_NV16,	.mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8	},
		{ .pixelformat = V4L2_PIX_FMT_NV61,	.mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8	},
		{ .pixelformat = V4L2_PIX_FMT_YVU420,	.mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8	},
		{ .pixelformat = V4L2_PIX_FMT_YUV420,	.mbus_fmt = MEDIA_BUS_FMT_YUYV8_2X8	},
		{ .pixelformat = V4L2_PIX_FMT_NV12,	.mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8	},
		{ .pixelformat = V4L2_PIX_FMT_NV21,	.mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8	},
	};
	const struct tccvin_mbus_fmt		*fotmat		= NULL;
	unsigned int				idxList		= 0;
	unsigned int				nList		= 0;
	unsigned int				mbus_code	= 0;

	/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	nList = ARRAY_SIZE(tccvin_mbus_fmt_list);
	for (idxList = 0; idxList < nList; idxList++) {
		fotmat = &tccvin_mbus_fmt_list[idxList];
		if (pixelformat == fotmat->pixelformat) {
			mbus_code = fotmat->mbus_fmt;
			break;
		}
	}

	return mbus_code;
}

static struct v4l2_rect *tccvin_get_rect_by_target(struct tccvin_stream *vstream,
	unsigned int target)
{
	const struct device			*dev		= NULL;
	struct v4l2_rect			*rect		= NULL;

	dev		= stream_to_device(vstream);

	switch (target) {
	case V4L2_SEL_TGT_CROP:
	case V4L2_SEL_TGT_CROP_DEFAULT:
		rect = &vstream->rect_crop;
		break;
	case V4L2_SEL_TGT_COMPOSE:
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
		rect = &vstream->rect_compose;
		break;
	default:
		loge(dev, "target(0x%08x) is not supported\n", target);
		break;
	}

	return rect;
}

static void tccvin_print_v4l2_pix_format_mplane(const struct tccvin_stream *vstream,
	const struct v4l2_pix_format_mplane *format)
{
	const struct device			*dev		= NULL;
	char					fcc[4]		= { 0, };
	unsigned int				idxPlane	= 0;

	dev		= stream_to_device(vstream);

	/* convert pixelformat to fcc */
	(void)strscpy(fcc, (const char *)&format->pixelformat, sizeof(format->pixelformat));

	logd(dev, "width: %u, height: %u\n", format->width, format->height);
	logd(dev, "pixelformat: %c%c%c%c\n", fcc[0], fcc[1], fcc[2], fcc[3]);
	logd(dev, "field: %u, colorspace: %u\n", format->field, format->colorspace);
	logd(dev, "num_planes: %u\n", format->num_planes);
	for (idxPlane = 0; idxPlane < format->num_planes; idxPlane++) {
		logd(dev, " plane_fmt[%u].sizeimage: %u, .bytesperline: %u\n",
			idxPlane,
			format->plane_fmt[idxPlane].sizeimage,
			format->plane_fmt[idxPlane].bytesperline);
	}
}

static void tccvin_print_v4l2_format(const struct tccvin_stream *vstream,
	const struct v4l2_format *format)
{
	const struct device			*dev		= NULL;

	dev		= stream_to_device(vstream);

	switch (format->type) {
	case (unsigned int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		tccvin_print_v4l2_pix_format_mplane(vstream, &format->fmt.pix_mp);
		break;
	default:
		loge(dev, "type (0x%08x) is not supported\n", format->type);
		break;
	}
}

static void tccvin_print_v4l2_buffer_mplane(const struct tccvin_stream *vstream,
	const struct v4l2_buffer *buf)
{
	const struct device			*dev		= NULL;
	const struct v4l2_plane			*plane		= NULL;
	unsigned int				idxpln		= 0;

	dev		= stream_to_device(vstream);

	logd(dev, "index;: %u\n", buf->index);
	logd(dev, "type: %u\n", buf->type);
	logd(dev, "bytesused: %u\n", buf->bytesused);
	logd(dev, "flags: 0x%08x\n", buf->flags);
	logd(dev, "field: 0x%08x\n", buf->field);
	logd(dev, "sequence: %u\n", buf->sequence);
	logd(dev, "memory: %u\n", buf->memory);
	logd(dev, "length: 0x%08x\n", buf->length);
	for (idxpln = 0; idxpln < buf->length; idxpln++) {
		plane = &buf->m.planes[idxpln];
		switch (buf->memory) {
		case (unsigned int)V4L2_MEMORY_MMAP:
			logd(dev, "plane[%u]: 0x%08x\n", idxpln, plane->m.mem_offset);
			break;
		default:
			loge(dev, "memory(0x%08x) is not supported\n", buf->memory);
			break;
		}
	}
}

static void tccvin_print_v4l2_buffer(const struct tccvin_stream *vstream,
	const struct v4l2_buffer *buf)
{
	const struct device			*dev		= NULL;

	dev		= stream_to_device(vstream);

	switch (buf->type) {
	case (unsigned int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		tccvin_print_v4l2_buffer_mplane(vstream, buf);
		break;
	default:
		loge(dev, "type(0x%08x) is not supported\n", buf->type);
		break;
	}
}

/* ------------------------------------------------------------------------
 * v4l2_file_operations
 */
static int tccvin_fop_open(struct file *pfile)
{
	struct tccvin_stream			*vstream	= NULL;
	struct tccvin_fh			*handle		= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	vstream = (struct tccvin_stream *)video_drvdata(pfile);

	/* Create the device handle. */
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (handle == NULL) {
		/* failure of allocating memory */
		ret = -ENOMEM;
	} else {
		mutex_lock(&vstream->tdev->mlock);
		vstream->sequence = 0;
		mutex_unlock(&vstream->tdev->mlock);

		v4l2_fh_init(&handle->vfh, &vstream->tdev->vdev);
		v4l2_fh_add(&handle->vfh);
		handle->vstream = vstream;
		handle->state = TCCVIN_HANDLE_PASSIVE;
		pfile->private_data = handle;
	}

	return ret;
}

static int tccvin_fop_release(struct file *pfile)
{
	struct tccvin_fh			*handle		= NULL;
	struct tccvin_stream			*vstream	= NULL;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)pfile->private_data;
	vstream		= handle->vstream;

	vb2_queue_release(&vstream->queue.queue);

	/* Release the file handle. */
	v4l2_fh_del(&handle->vfh);
	v4l2_fh_exit(&handle->vfh);
	kfree(handle);
	pfile->private_data = NULL;

	mutex_lock(&vstream->tdev->mlock);
	mutex_unlock(&vstream->tdev->mlock);

	return 0;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_fop_mmap(struct file *pfile, struct vm_area_struct *vma)
{
	const struct tccvin_fh			*handle		= NULL;
	struct tccvin_stream			*vstream	= NULL;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)pfile->private_data;
	vstream		= handle->vstream;

	return vb2_mmap(&vstream->queue.queue, vma);
}

static __poll_t tccvin_fop_poll(struct file *pfile, poll_table *wait)
{
	const struct tccvin_fh			*handle		= NULL;
	struct tccvin_stream			*vstream	= NULL;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)pfile->private_data;
	vstream		= handle->vstream;

	return vb2_poll(&vstream->queue.queue, pfile, wait);
}

const struct v4l2_file_operations tccvin_fops = {
	.owner				= THIS_MODULE,
	.open				= tccvin_fop_open,
	.release			= tccvin_fop_release,
	.unlocked_ioctl			= video_ioctl2,
	.mmap				= tccvin_fop_mmap,
	.poll				= tccvin_fop_poll,
};

/* ------------------------------------------------------------------------
 * v4l2_ioctl_ops
 */
/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_querycap(struct file *pfile, void *fh,
	struct v4l2_capability *cap)
{
	const struct device			*dev		= NULL;
	const struct tccvin_fh			*handle		= NULL;
	struct video_device			*vdev		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	int					ret		= 0;

	handle		= (struct tccvin_fh *)fh;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);
	vdev		= &vstream->tdev->vdev;

	(void)scnprintf((char *)cap->driver, PAGE_SIZE, "%s", DRIVER_NAME);
	(void)scnprintf((char *)cap->card, PAGE_SIZE, "%s", vdev->name);
	(void)scnprintf((char *)cap->bus_info, PAGE_SIZE, "platform:videoinput:%d", vdev->num);
	/* coverity[misra_c_2012_rule_7_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_2_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	cap->version		= KERNEL_VERSION(5, 4, 00);
	cap->capabilities	= V4L2_CAP_DEVICE_CAPS | cap->device_caps;

	logd(dev, "driver: %s\n", cap->driver);
	logd(dev, "card: %s\n", cap->card);
	logd(dev, "bus_info: %s\n", cap->bus_info);
	logd(dev, "version: %u.%u.%u\n",
		(cap->version >> 16) & 0xFFU,
		(cap->version >>  8) & 0xFFU,
		(cap->version >>  0) & 0xFFU);
	logd(dev, "device_caps: 0x%08x\n", cap->device_caps);
	logd(dev, "capabilities: 0x%08x\n", cap->capabilities);

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_enum_fmt(struct file *pfile, void *fh,
	struct v4l2_fmtdesc *fmt)
{
	const struct device			*dev		= NULL;
	const struct tccvin_fh			*handle		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	unsigned int				index		= 0;
	unsigned int				pixelformat	= 0;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);

	pixelformat = tccvin_video_get_pixelformat_by_index(vstream, fmt->index);
	if (pixelformat == 0U) {
		logd(dev, "format of index(%u) is not supported\n", fmt->index);
		ret = -EINVAL;
	} else {
		index		= fmt->index;

		(void)memset(fmt, 0, sizeof(*fmt));

		fmt->index	= index;
		fmt->type	= (unsigned int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		fmt->pixelformat = pixelformat;
		(void)strscpy((char *)fmt->description, (const char *)&fmt->pixelformat, sizeof(fmt->pixelformat));

		logd(dev, "index: %u\n", fmt->index);
		logd(dev, "type: 0x%08x\n", fmt->type);
		logd(dev, "flags: 0x%08x\n", fmt->flags);
		logd(dev, "description: %s\n", fmt->description);
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_g_fmt(struct file *pfile, void *fh,
	struct v4l2_format *fmt)
{
	const struct tccvin_fh			*handle		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;

	(void)memcpy(&fmt, &vstream->format, sizeof(fmt));

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_s_fmt(struct file *pfile, void *fh,
	struct v4l2_format *fmt)
{
	const struct tccvin_fh			*handle		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	const struct device			*dev		= NULL;
	struct v4l2_pix_format_mplane		*pix_mp		= NULL;
	bool					ret_bool	= FALSE;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);

	ret_bool = tccvin_video_is_format_supported(vstream, fmt);
	if (!ret_bool) {
		loge(dev, "format is not supported\n");
		ret = -EINVAL;
	} else {
		mutex_lock(&vstream->mlock);
		if (vb2_is_busy(&vstream->queue.queue)) {
			loge(dev, "vb2_is_busy\n");
		} else {
			(void)memcpy(&vstream->format, fmt, sizeof(*fmt));

			/* additional set */
			pix_mp = &vstream->format.fmt.pix_mp;

			(int)v4l2_fill_pixfmt_mp(pix_mp, pix_mp->pixelformat, pix_mp->width, pix_mp->height);
			pix_mp->field		= (unsigned int)V4L2_FIELD_NONE;
			pix_mp->colorspace	= (unsigned int)V4L2_COLORSPACE_SRGB;

//			tccvin_print_v4l2_format(vstream, &vstream->format);
		}
		mutex_unlock(&vstream->mlock);
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_try_fmt(struct file *pfile, void *fh,
	struct v4l2_format *fmt)
{
	const struct tccvin_fh			*handle		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	const struct device			*dev		= NULL;
	bool					ret_bool	= FALSE;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);

	ret_bool = tccvin_video_is_format_supported(vstream, fmt);
	if (!ret_bool) {
		tccvin_print_v4l2_format(vstream, fmt);

		loge(dev, "format is not supported\n");
		ret = -EINVAL;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_reqbufs(struct file *pfile, void *fh,
	struct v4l2_requestbuffers *rb)
{
	const struct tccvin_fh			*handle		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	const struct device			*dev		= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);

	if (rb->type != (unsigned int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		loge(dev, "type (0x%08x) is not supported\n", rb->type);
		ret = -EINVAL;
	} else {
//		ret = tccvin_video_declare_coherent_dma_memory(vstream);
//		logd(dev, "tccvin_video_declare_coherent_dma_memory, ret: %d\n", ret);

		ret = vb2_reqbufs(&vstream->queue.queue, rb);
		if (ret >= 0) {
			logd(dev, "count: %d\n", rb->count);
			logd(dev, "type: 0x%08x\n", rb->type);
			logd(dev, "memory: 0x%08x\n", rb->memory);
			logd(dev, "capabilities: 0x%08x\n", rb->capabilities);
		}
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_querybuf(struct file *pfile, void *fh,
	struct v4l2_buffer *buf)
{
	const struct tccvin_fh			*handle		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	const struct device			*dev		= NULL;
	struct vb2_buffer			*vb		= NULL;
	unsigned int				dma_addrs[MAX_PLANES];
	unsigned int				idxpln		= 0;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);

	ret = vb2_querybuf(&vstream->queue.queue, buf);
	if (ret < 0) {
		loge(dev, "vb2_querybuf, ret: %d\n", ret);
		ret = -EINVAL;
	} else {
		/* print for debug */
		tccvin_print_v4l2_buffer(vstream, buf);

		vb = vstream->queue.queue.bufs[buf->index];
		(void)memset(dma_addrs, 0, sizeof(dma_addrs));
		tccvin_get_dma_addrs(vstream, vb, dma_addrs);
		tccvin_print_dma_addrs(vstream, vb, dma_addrs);

		for (idxpln = 0; idxpln < buf->length; idxpln++) {
			/* provid dma addrs */
			buf->m.planes[idxpln].reserved[0] = dma_addrs[idxpln];
		}
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_qbuf(struct file *pfile, void *fh,
	struct v4l2_buffer *buf)
{
	const struct tccvin_fh			*handle		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	const struct device			*dev		= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);

	/* device is available */
	ret = vb2_qbuf(&vstream->queue.queue, vstream->tdev->vdev.v4l2_dev->mdev, buf);
	if (ret < 0) {
		loge(dev, "vb2_qbuf, ret: %d\n", ret);
		ret = -EINVAL;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_expbuf(struct file *pfile, void *fh,
	struct v4l2_exportbuffer *buf)
{
	const struct tccvin_fh			*handle		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	const struct device			*dev		= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);

	ret = vb2_expbuf(&vstream->queue.queue, buf);
	if (ret < 0) {
		loge(dev, "vb2_expbuf, ret: %d\n", ret);
		ret = -EINVAL;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_dqbuf(struct file *pfile, void *fh,
	struct v4l2_buffer *buf)
{
	const struct tccvin_fh			*handle		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	const struct device			*dev		= NULL;
	bool					non_block	= FALSE;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);

	non_block = ((pfile->f_flags & (unsigned int)O_NONBLOCK) != 0U);
	ret = vb2_dqbuf(&vstream->queue.queue, buf, non_block);
	if (ret < 0) {
		loge(dev, "vb2_dqbuf, ret: %d\n", ret);
		ret = -EINVAL;
	} else {
		/* print for debug */
		tccvin_print_v4l2_buffer(vstream, buf);
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_streamon(struct file *pfile, void *fh,
	enum v4l2_buf_type type)
{
	const struct tccvin_fh			*handle		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	const struct device			*dev		= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);

	mutex_lock(&vstream->mlock);
	ret = vb2_streamon(&vstream->queue.queue, type);
	mutex_unlock(&vstream->mlock);
	if (ret < 0) {
		loge(dev, "vb2_streamon, ret: %d\n", ret);
		ret = -EINVAL;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_streamoff(struct file *pfile, void *fh,
	enum v4l2_buf_type type)
{
	const struct tccvin_fh			*handle		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	const struct device			*dev		= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);

	mutex_lock(&vstream->mlock);
	ret = vb2_streamoff(&vstream->queue.queue, type);
	mutex_unlock(&vstream->mlock);
	if (ret < 0) {
		loge(dev, "vb2_streamoff, ret: %d\n", ret);
		ret = -EINVAL;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_enum_input(struct file *pfile, void *fh,
	struct v4l2_input *input)
{
	const struct tccvin_fh			*handle		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	struct v4l2_subdev			*subdev		= NULL;
	const struct device			*dev		= NULL;
	unsigned int				index		= 0;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);

	index = input->index;
	if (index != 0U) {
		loge(dev, "input %d is not supported\n", index);
		ret = -EINVAL;
	} else {
		(void)memset(input, 0, sizeof(*input));

		input->index = index;
		(void)scnprintf((char *)input->name, PAGE_SIZE, "v4l2 subdev[%u]", index);
		input->type = V4L2_INPUT_TYPE_CAMERA;

		subdev = vstream->tdev->tsubdev.sd;
		ret = tccvin_subdev_video_g_input_status(subdev, &input->status);
		switch (ret) {
		case -ENODEV:
			loge(dev, "%s - subdev is null\n", input->name);
			break;
		case -ENOIOCTLCMD:
			logd(dev, "%s - video.g_input_status is not supported\n", input->name);
			ret = -ENOTTY;
			break;
		default:
			logd(dev, "%s - status: 0x%08x\n", input->name, input->status);
			break;
		}
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_g_input(struct file *pfile, void *fh,
	unsigned int *input)
{
	int					ret		= 0;

	/* support 0th input only */
	*input = 0;

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_s_input(struct file *pfile, void *fh,
	unsigned int input)
{
	const struct tccvin_fh			*handle		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	const struct device			*dev		= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);

	/* support 0th input only */
	if (input != 0U) {
		loge(dev, "input %d is not supported\n", input);
		ret = -EINVAL;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_g_pixelaspect(struct file *pfile, void *fh,
	int buf_type, struct v4l2_fract *aspect)
{
	const struct tccvin_fh			*handle		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	const struct device			*dev		= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);

	/* Note
	 * Unfortunately in the case of multiplanar buffer types
	 * (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE and
	 * V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) this API was messed up with
	 * regards to how the v4l2_cropcap type field should be filled in.
	 * Some drivers only accepted the _MPLANE buffer type while other
	 * drivers only accepted a non-multiplanar buffer type
	 * (i.e. without the _MPLANE at the end).
	 */
	if ((buf_type != (int)V4L2_BUF_TYPE_VIDEO_CAPTURE) &&
	    (buf_type != (int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) {
		loge(dev, "type: 0x%08x, is not supported\n", buf_type);
		ret = -EINVAL;
	}

	if (ret >= 0) {
		aspect->numerator = 1;
		aspect->denominator = 1;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_g_selection(struct file *pfile, void *fh,
	struct v4l2_selection *s)
{
	const struct tccvin_fh			*handle		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	const struct v4l2_rect			*r		= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;

	r = tccvin_get_rect_by_target(vstream, s->target);
	if (r == NULL) {
		/* error: tccvin_get_rect_by_target */
		ret = -EINVAL;
	} else {
		/* get target's rect info */
		(void)memcpy(&s->r, r, sizeof(*r));
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_s_selection(struct file *pfile, void *fh,
	struct v4l2_selection *s)
{
	const struct tccvin_fh			*handle		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	struct v4l2_rect			*r		= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;

	r = tccvin_get_rect_by_target(vstream, s->target);
	if (r == NULL) {
		/* error: tccvin_get_rect_by_target */
		ret = -EINVAL;
	} else {
		/* set target's rect info */
		(void)memcpy(r, &s->r, sizeof(*r));
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_g_parm(struct file *pfile, void *fh,
	struct v4l2_streamparm *a)
{
	const struct device			*dev		= NULL;
	const struct tccvin_fh			*handle		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	struct v4l2_captureparm			*capture	= NULL;
	struct v4l2_subdev			*subdev		= NULL;
	struct v4l2_subdev_frame_interval	interval	= { 0, };
	const struct v4l2_fract			*timeperframe	= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);
	subdev		= vstream->tdev->tsubdev.sd;

	(void)memset(&interval, 0, sizeof(interval));
	interval.interval.numerator	= 1;
	interval.interval.denominator	= 1;

	/* if subdev exists, then get its interval */
	if (subdev != NULL) {
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_11_9_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		ret = v4l2_subdev_call(subdev, video, g_frame_interval, &interval);
		if (ret != 0) {
			/* failure */
			logw(dev, "vide.g_frame_interval, ret: %d\n", ret);
		}
	}

	switch (a->type) {
	case (unsigned int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		capture = &a->parm.capture;
		capture->timeperframe = interval.interval;
		timeperframe = &capture->timeperframe;
		break;
	default:
		timeperframe = &interval.interval;
		break;
	}

	logd(dev, "framerate: %u / %u\n", timeperframe->numerator, timeperframe->denominator);

	return 0;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_s_parm(struct file *pfile, void *fh,
	struct v4l2_streamparm *a)
{
	const struct device			*dev		= NULL;
	const struct tccvin_fh			*handle		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	struct v4l2_captureparm			*capture	= NULL;
	struct v4l2_subdev			*subdev		= NULL;
	struct v4l2_subdev_frame_interval	interval	= { 0, };
	struct v4l2_fract			*timeperframe	= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);
	subdev		= vstream->tdev->tsubdev.sd;

	(void)memset(&interval, 0, sizeof(interval));

	switch (a->type) {
	case (unsigned int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		capture = &a->parm.capture;
		timeperframe = &capture->timeperframe;
		break;
	default:
		timeperframe = &interval.interval;
		break;
	}

	if ((timeperframe->numerator   == 0U) ||
	    (timeperframe->denominator == 0U)) {
		timeperframe->numerator   = 1;
		timeperframe->denominator = 1;
	}

	logd(dev, "framerate: %u / %u\n", timeperframe->numerator, timeperframe->denominator);

	/* if subdev exists, then set interval to its one */
	if (subdev != NULL) {
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_11_9_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		ret = v4l2_subdev_call(subdev, video, s_frame_interval, &interval);
		if (ret != 0) {
			/* error */
			loge(dev, "vide.s_frame_interval, ret: %d\n", ret);
		}
	}

	return 0;
}

/*
 * framesize depends on capabilities of video-input path, not video sources
 * because wdma can even save video data with black video data
 * framesize is bigger than video source's supported one and video-input path doesn't have any scaler.
 */
/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_enum_framesizes(struct file *pfile, void *fh,
	struct v4l2_frmsizeenum *fsize)
{
	const struct tccvin_fh			*handle		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	const struct device			*dev		= NULL;
	bool					ret_bool	= TRUE;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);

	if (fsize->index != 0U) {
		loge(dev, "index(%u) is not 0\n", fsize->index);
		ret = -EINVAL;
	}

	ret_bool = tccvin_video_is_pixelformat_supported(vstream, fsize->pixel_format);
	if (!ret_bool) {
		loge(dev, "tccvin_video_is_pixelformat_supported\n");
		ret = -EINVAL;
	}

	if (ret == 0) {
		fsize->type = (unsigned int)V4L2_FRMSIZE_TYPE_STEPWISE;
		fsize->stepwise.min_width = 4;
		fsize->stepwise.max_width = MAX_FRAMEWIDTH;
		fsize->stepwise.step_width = 4;
		fsize->stepwise.min_height = 4;
		fsize->stepwise.max_height = MAX_FRAMEHEIGHT;
		fsize->stepwise.step_height = 2;

		logd(dev, "framesize: %u * %u ~ %u * %u (step: %u, %u)\n",
			fsize->stepwise.min_width, fsize->stepwise.min_height,
			fsize->stepwise.max_width, fsize->stepwise.max_height,
			fsize->stepwise.step_width, fsize->stepwise.step_height);
	}

	return ret;
}

/*
 * framesize depends on capabilities of video sources, not video-input path
 * because video-input path can get video data from video source as much as possible.
 */
/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccvin_ioctl_enum_frameintervals(struct file *pfile, void *fh,
	struct v4l2_frmivalenum *fival)
{
	const struct tccvin_fh			*handle		= NULL;
	const struct tccvin_stream		*vstream	= NULL;
	const struct device			*dev		= NULL;
	struct v4l2_subdev			*subdev		= NULL;
	bool					ret_bool	= TRUE;
	int					ret_call	= 0;
	struct v4l2_subdev_frame_interval_enum	fie		= { 0, };
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;
	dev		= stream_to_device(vstream);
	subdev		= vstream->tdev->tsubdev.sd;

	ret_bool = tccvin_video_is_pixelformat_supported(vstream, fival->pixel_format);
	if (!ret_bool) {
		loge(dev, "tccvin_video_is_pixelformat_supported\n");
		ret = -EINVAL;
	}

	if (subdev != NULL) {
		fie.index	= fival->index;
		fie.pad		= 0;
		fie.code	= tccvin_get_mbus_code_by_pixelformat(fival->pixel_format);
		fie.width	= fival->width;
		fie.height	= fival->height;
		fie.which	= (unsigned int)V4L2_SUBDEV_FORMAT_ACTIVE;

		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		ret_call = v4l2_subdev_call(subdev, pad, enum_frame_interval, NULL, &fie);
		if (ret_call != 0) {
			logw(dev, "pad.enum_frame_interval, ret: %d\n", ret_call);
			ret = ret_call;
		} else {
			fival->type = (unsigned int)V4L2_FRMIVAL_TYPE_DISCRETE;
			(void)memcpy(&fival->discrete, &fie.interval, sizeof(fie.interval));
		}
	}

	if (ret == 0) {
		logd(dev, "index: %u, width: %u, height: %u\n",
			fival->index, fival->width, fival->height);
		logd(dev, " . numerator: %u, denominator: %u\n",
			fival->discrete.numerator, fival->discrete.denominator);
	}

	return ret;
}

/* coverity[misra_c_2012_rule_2_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static long tccvin_ioctl_default(struct file *pfile, void *fh, bool valid_prio,
	unsigned int cmd, void *arg)
{
	const struct tccvin_fh			*handle		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	int					ret		= 0;

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	handle		= (struct tccvin_fh *)fh;
	vstream		= handle->vstream;

	switch (cmd) {
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case (unsigned int)VIDIOC_CHECK_PATH_STATUS:
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		tccvin_video_check_path_status(vstream, (unsigned int *)arg);
		break;

	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case (unsigned int)VIDIOC_G_LASTFRAME_ADDRS:
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		ret = tccvin_video_get_lastframe_addrs(vstream, (unsigned int *)arg);
		break;

	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case (unsigned int)VIDIOC_CREATE_LASTFRAME:
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		ret = tccvin_video_create_lastframe(vstream, (unsigned int *)arg);
		break;

	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case (unsigned int)VIDIOC_S_HANDOVER:
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		ret = tccvin_video_s_handover(vstream, (unsigned int *)arg);
		break;

	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	case (unsigned int)VIDIOC_S_LUT:
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		ret = tccvin_video_s_lut(vstream, (struct vin_lut *)arg);
		break;

	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

const struct v4l2_ioctl_ops tccvin_ioctl_ops = {
	.vidioc_querycap		= tccvin_ioctl_querycap,
	.vidioc_enum_fmt_vid_cap	= tccvin_ioctl_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane	= tccvin_ioctl_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane	= tccvin_ioctl_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane	= tccvin_ioctl_try_fmt,
	.vidioc_reqbufs			= tccvin_ioctl_reqbufs,
	.vidioc_querybuf		= tccvin_ioctl_querybuf,
	.vidioc_qbuf			= tccvin_ioctl_qbuf,
	.vidioc_expbuf			= tccvin_ioctl_expbuf,
	.vidioc_dqbuf			= tccvin_ioctl_dqbuf,
	.vidioc_streamon		= tccvin_ioctl_streamon,
	.vidioc_streamoff		= tccvin_ioctl_streamoff,
	.vidioc_enum_input		= tccvin_ioctl_enum_input,
	.vidioc_g_input			= tccvin_ioctl_g_input,
	.vidioc_s_input			= tccvin_ioctl_s_input,
	.vidioc_g_pixelaspect		= tccvin_ioctl_g_pixelaspect,
	.vidioc_g_selection		= tccvin_ioctl_g_selection,
	.vidioc_s_selection		= tccvin_ioctl_s_selection,
	.vidioc_g_parm			= tccvin_ioctl_g_parm,
	.vidioc_s_parm			= tccvin_ioctl_s_parm,
	.vidioc_enum_framesizes		= tccvin_ioctl_enum_framesizes,
	.vidioc_enum_frameintervals	= tccvin_ioctl_enum_frameintervals,
	.vidioc_default			= tccvin_ioctl_default,
};

int tccvin_v4l2_init_format(struct tccvin_device *tdev)
{
	const struct device			*dev		= NULL;
	struct tccvin_stream			*vstream	= NULL;
	struct v4l2_format			*format		= NULL;
	struct v4l2_pix_format_mplane		*pix_mp		= NULL;
	int					ret		= 0;

	vstream		= &tdev->vstream;
	dev		= stream_to_device(vstream);

	format = &vstream->format;
	format->type	= (unsigned int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	pix_mp = &format->fmt.pix_mp;
	pix_mp->pixelformat	= (unsigned int)V4L2_PIX_FMT_RGB24;
	pix_mp->width		= DEFAULT_FRAMEWIDTH;
	pix_mp->height		= DEFAULT_FRAMEHEIGHT;

	(int)v4l2_fill_pixfmt_mp(pix_mp, pix_mp->pixelformat, pix_mp->width, pix_mp->height);
	pix_mp->field		= (unsigned int)V4L2_FIELD_NONE;
	pix_mp->colorspace	= (unsigned int)V4L2_COLORSPACE_SRGB;

	tccvin_print_v4l2_format(vstream, &vstream->format);

	return ret;
}

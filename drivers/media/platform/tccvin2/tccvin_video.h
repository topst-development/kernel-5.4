/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef TCCVIN_VIDEO_H
#define TCCVIN_VIDEO_H

#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/of_device.h>
#include <linux/videodev2.h>
#include <media/media-device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-fwnode.h>
#include <media/videobuf2-v4l2.h>

#include "basic_operation.h"

/* vioc path */
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_vin.h>
#include <video/tcc/vioc_viqe.h>
#include <video/tcc/vioc_deintls.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/tcc_cam_ioctrl.h>

/* reserved memory */
#include <linux/of_reserved_mem.h>

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/mutex.h>

/* ------------------------------------------------------------------------
 * Driver specific constants.
 */
#define DRIVER_NAME				"tccvin"
#define DRIVER_VERSION				"2.0.0"

/* vioc path */
#define MAX_BUFFERS				(4)
#define	MAX_PLANES				(3)

//#define	DEFAULT_FRAMEWIDTH			(640U)
//#define	DEFAULT_FRAMEHEIGHT			(480U)
#define	DEFAULT_FRAMEWIDTH			(1920U)
#define	DEFAULT_FRAMEHEIGHT			(1080U)

#define	MAX_FRAMEWIDTH				(8192U)
#define	MAX_FRAMEHEIGHT				(8192U)

#define PGL_FORMAT				(VIOC_IMG_FMT_ARGB8888)
#define PGL_BG_R				(0xFFU)
#define PGL_BG_G				(0xFFU)
#define PGL_BG_B				(0xFFU)
#define PGL_BGM_R				(PGL_BG_R & 0xF8U)
#define PGL_BGM_G				(PGL_BG_G & 0xF8U)
#define PGL_BGM_B				(PGL_BG_B & 0xF8U)

enum vin_comp {
	VIN_COMP_VIN,
	VIN_COMP_VIQE,
	VIN_COMP_SDEINTL,
	VIN_COMP_SCALER,
	VIN_COMP_PGL,
	VIN_COMP_WMIX,
	VIN_COMP_WDMA,
	VIN_COMP_MAX,
};

enum reserved_memory {
	RESERVED_MEM_PGL,
	RESERVED_MEM_VIQE,
	RESERVED_MEM_PREV,
	RESERVED_MEM_LFRAME,
	RESERVED_MEM_MAX,
};

enum tccvin_buffer_state {
	TCCVIN_BUF_STATE_IDLE,
	TCCVIN_BUF_STATE_QUEUED,
	TCCVIN_BUF_STATE_ACTIVE,
	TCCVIN_BUF_STATE_READY,
	TCCVIN_BUF_STATE_DONE,
	TCCVIN_BUF_STATE_ERROR,
};

enum tccvin_queue_state {
	TCCVIN_QUEUE_DISCONNECTED		= 1,	/* 1 << 0 */
	TCCVIN_QUEUE_DROP_CORRUPTED		= 2,	/* 1 << 1 */
};

enum tccvin_handle_state {
	TCCVIN_HANDLE_PASSIVE,
	TCCVIN_HANDLE_ACTIVE,
};

enum tccvin_test {
	TCCVIN_TEST_STOP_SUBDEV,
	TCCVIN_TEST_STOP_WDMA,
	TCCVIN_TEST_MAX,
};

/*
 * strucures for video source
 */
struct tccvin_vs_info {
	/* VIN_CTRL */
	unsigned int				data_order;		// [22:20]
	unsigned int				data_format;		// [19:16]
	unsigned int				stream_enable;		// [14]
	unsigned int				gen_field_en;		// [13]
	unsigned int				de_low;			// [12]
	unsigned int				field_low;		// [11]
	unsigned int				vs_low;			// [10]
	unsigned int				hs_low;			// [ 9]
	unsigned int				pclk_polarity;		// [ 8]
	unsigned int				vs_mask;		// [ 6]
	unsigned int				hsde_connect_en;	// [ 4]
	unsigned int				intpl_en;		// [ 3]
	unsigned int				interlaced;		// [ 2]
	unsigned int				conv_en;		// [ 1]

	/* VIN_MISC */
	unsigned int				flush_vsync;		// [16]

	/* VIN_SIZE */
	unsigned int				height;			// [31:16]
	unsigned int				width;			// [15: 0]
};

/* vioc interrupt */
struct vioc_intr {
	unsigned int				reg;
	int					num;
	struct vioc_intr_type			source;
};

/* vioc component */
struct vioc_comp {
	/* device node */
	struct device_node			*np;

	/* vioc type and index */
	unsigned int				type;
	unsigned int				index;

	/* vioc interrupt */
	struct vioc_intr			intr;
};

struct tccvin_device;

struct tccvin_cif {
	unsigned int				cif_port;
	void __iomem				*cifport_addr;

	struct clk				*vioc_clk;
	unsigned int				use_pgl;
	struct vioc_comp			vin_path[VIN_COMP_MAX];
	struct vin_lut				vin_internal_lut;

	struct reserved_mem			*rsvd_mem[RESERVED_MEM_MAX];

	unsigned int				recovery_trigger;
};

struct tccvin_buffer {
	struct vb2_v4l2_buffer			buf;
	struct list_head			entry;
};

struct tccvin_queue {
	struct vb2_queue			queue;
	unsigned int				flags;
	spinlock_t				slock;
	struct list_head			buf_list;
};

/* v4l2 structure */
struct tccvin_stream {
	struct tccvin_device			*tdev;

	struct tccvin_vs_info			vs_info;
	struct v4l2_subdev_mbus_code_enum	mbus_code;
	struct v4l2_mbus_config			mbus_config;
	struct v4l2_rect			rect_crop;
	struct v4l2_rect			rect_compose;
	struct v4l2_format			format;
	struct mutex				mlock;

	unsigned int				handover_flags;

	/* additional stabilization time */
	unsigned int				vs_stabilization;

	/* video-input path device */
	struct tccvin_cif			cif;

	int					skip_frame_cnt;

	struct tccvin_queue			queue;
	struct tccvin_buffer			*prev_buf;
	struct tccvin_buffer			*next_buf;

	unsigned int				timestamp;
	struct timespec				ts_prev;
	struct timespec				ts_next;
	struct timespec				ts_diff;

	unsigned int				sequence;
};

/* v4l2 subdev structure */
struct tccvin_subdev {
	struct v4l2_async_subdev		asd;
	struct v4l2_async_notifier		notifier;
	struct v4l2_subdev			*sd;
	struct v4l2_subdev_format		fmt;

	struct tccvin_device			*tdev;
};

struct tccvin_device {
	struct platform_device			*pdev;

	int					id;
	char					name[32];

	struct mutex				mlock;

	struct video_device			vdev;
#ifdef CONFIG_MEDIA_CONTROLLER
	struct media_device			mdev;
#endif
	struct v4l2_device			v4ldev;

	struct tccvin_subdev			tsubdev;
	struct tccvin_stream			vstream;
	struct kref				ref;
};

struct tccvin_fh {
	struct v4l2_fh				vfh;
	struct tccvin_stream			*vstream;
	enum tccvin_handle_state		state;
};

/* Macros */
/* (struct tccvin_stream *) to (struct device *) */
#define stream_to_device(ptr)			(&((((ptr)->tdev)->pdev)->dev))


/* ------------------------------------------------------------------------
 * Debugging, printing and logging
 */

#define LOG_TAG					"VIN"

/* coverity[misra_c_2012_rule_20_10_violation : FALSE] */
#define loge(dev, fmt, ...)			\
	{ dev_err(dev,  "[ERROR][%s] %s - " fmt, LOG_TAG, __func__, ##__VA_ARGS__); }
/* coverity[misra_c_2012_rule_20_10_violation : FALSE] */
#define logw(dev, fmt, ...)			\
	{ dev_warn(dev,  "[WARN][%s] %s - " fmt, LOG_TAG, __func__, ##__VA_ARGS__); }
/* coverity[misra_c_2012_rule_20_10_violation : FALSE] */
#define logd(dev, fmt, ...)			\
	{ /* dev_info(dev, "[DEBUG][%s] %s - " fmt, LOG_TAG, __func__, ##__VA_ARGS__); */ }
/* coverity[misra_c_2012_rule_20_10_violation : FALSE] */
#define logi(dev, fmt, ...)			\
	{ dev_info(dev,  "[INFO][%s] %s - " fmt, LOG_TAG, __func__, ##__VA_ARGS__); }


/* v4l2 subdev interfaces */
extern int tccvin_subdev_core_init(struct v4l2_subdev *sd, u32 val);
extern int tccvin_subdev_core_load_fw(struct v4l2_subdev *sd);
extern int tccvin_subdev_core_s_power(struct v4l2_subdev *sd, int on);

extern int tccvin_subdev_video_g_input_status(struct v4l2_subdev *sd, u32 *status);
extern int tccvin_subdev_video_s_stream(struct v4l2_subdev *sd, int enable);
extern int tccvin_subdev_video_g_dv_timings(struct v4l2_subdev *sd, struct v4l2_dv_timings *timings);
extern int tccvin_subdev_video_g_mbus_config(struct v4l2_subdev *sd, struct v4l2_mbus_config *cfg);

extern int tccvin_subdev_pad_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_format *format);
extern int tccvin_subdev_pad_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_format *format);

/* v4l2 interface */
extern const struct v4l2_ioctl_ops		tccvin_ioctl_ops;
extern const struct v4l2_file_operations	tccvin_fops;
extern int tccvin_v4l2_init_queue(struct tccvin_queue *queue, bool drop_corrupted);
extern int tccvin_v4l2_init_format(struct tccvin_device *tdev);
extern void tccvin_get_dma_addrs(const struct tccvin_stream *vstream, struct vb2_buffer *vb, unsigned int addrs[]);
extern void tccvin_print_dma_addrs(const struct tccvin_stream *vstream, const struct vb2_buffer *vb, const unsigned int addrs[]);

/* capture */
extern int tccvin_video_init(struct tccvin_stream *vstream, struct device_node *dev_node);
extern int tccvin_video_deinit(const struct tccvin_stream *vstream);
extern bool tccvin_video_is_pixelformat_supported(const struct tccvin_stream *vstream, unsigned int pixelformat);
extern unsigned int tccvin_video_get_pixelformat_by_index(const struct tccvin_stream *vstream, unsigned int index);
extern bool tccvin_video_is_format_supported(const struct tccvin_stream *vstream, const struct v4l2_format *format);
extern int tccvin_video_declare_coherent_dma_memory(const struct tccvin_stream *vstream);
extern int tccvin_video_streamon(struct tccvin_stream *vstream);
extern int tccvin_video_streamoff(struct tccvin_stream *vstream);

extern void tccvin_video_check_path_status(const struct tccvin_stream *vstream, unsigned int *status);
extern int tccvin_video_get_lastframe_addrs(const struct tccvin_stream *vstream, unsigned int *addrs);
extern int tccvin_video_create_lastframe(const struct tccvin_stream *vstream, unsigned int *addrs);
extern int tccvin_video_s_handover(struct tccvin_stream *vstream, const unsigned int *flag);
extern int tccvin_video_s_lut(struct tccvin_stream *vstream, const struct vin_lut *plut);
extern int tccvin_video_trigger_recovery(const struct tccvin_stream *vstream, unsigned int index);
#endif

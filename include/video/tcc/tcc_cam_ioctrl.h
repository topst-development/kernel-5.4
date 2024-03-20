// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __TCC_CAMERA_IOCTL_H__
#define __TCC_CAMERA_IOCTL_H__

enum v4l2_cap_dev_type {
	V4L2_CAP_ALL			= 0,
	V4L2_CAP_SUBDEV			= 1,
	V4L2_CAP_DEV			= 2,
};

#define V4L2_CAP_HANDOVER_NONE		0U
#define V4L2_CAP_HANDOVER_NEED		1U
#define V4L2_CAP_CTRL_SKIP_NONE		((unsigned int)0U)
#define V4L2_CAP_CTRL_SKIP_ALL		((unsigned int)1U << V4L2_CAP_ALL)
#define V4L2_CAP_CTRL_SKIP_SUBDEV	((unsigned int)1U << V4L2_CAP_SUBDEV)
#define V4L2_CAP_CTRL_SKIP_DEV		((unsigned int)1U << V4L2_CAP_DEV)
#define V4L2_CAP_PATH_NOT_WORKING	0U
#define V4L2_CAP_PATH_WORKING		1U

struct vin_lut {
    unsigned int lut0_en;
    unsigned int lut1_en;
    unsigned int lut2_en;
    /*	Range of RGB Color: 0 ~ 255
     *	Number of Field per Each Register: 4
     *	Number of Color: 3 (RGB or YUV)
     */
    unsigned int lut[256/4*3];
};

#define V4L2_CID_ISO			(V4L2_CID_PRIVATE_BASE+0)
#define V4L2_CID_EFFECT			(V4L2_CID_PRIVATE_BASE+1)
#define V4L2_CID_ZOOM			(V4L2_CID_PRIVATE_BASE+2)
#define V4L2_CID_FLIP			(V4L2_CID_PRIVATE_BASE+3)
#define V4L2_CID_SCENE			(V4L2_CID_PRIVATE_BASE+4)
#define V4L2_CID_METERING_EXPOSURE	(V4L2_CID_PRIVATE_BASE+5)
#define V4L2_CID_FLASH			(V4L2_CID_PRIVATE_BASE+6)
#define V4L2_CID_FOCUS_MODE		(V4L2_CID_PRIVATE_BASE+7)
#define V4L2_CID_LAST_PRIV		(V4L2_CID_FLASH)
#define V4L2_CID_MAX			(V4L2_CID_LAST_PRIV+1)

#define VIDIOC_USER_JPEG_CAPTURE		\
		_IOWR('V', BASE_VIDIOC_PRIVATE+1, int)
#define VIDIOC_USER_GET_CAPTURE_INFO		\
		_IOWR('V', BASE_VIDIOC_PRIVATE+2, struct TCCXXX_JPEG_ENC_DATA)
#define VIDIOC_USER_PROC_AUTOFOCUS		\
		_IOWR('V', BASE_VIDIOC_PRIVATE+3, int)
#define VIDIOC_USER_SET_CAMINFO_TOBEOPEN	\
		_IOWR('V', BASE_VIDIOC_PRIVATE+4, int)
#define VIDIOC_USER_GET_MAX_RESOLUTION		\
		_IOWR('V', BASE_VIDIOC_PRIVATE+5, int)
#define VIDIOC_USER_GET_SENSOR_FRAMERATE	\
		_IOWR('V', BASE_VIDIOC_PRIVATE+6, int)
#define VIDIOC_USER_GET_ZOOM_SUPPORT		\
		_IOWR('V', BASE_VIDIOC_PRIVATE+7, int)
#define VIDIOC_USER_SET_CAMERA_ADDR		\
		_IOWR('V', BASE_VIDIOC_PRIVATE+8, struct v4l2_requestbuffers)
#define VIDIOC_ASSIGN_ALLOCATED_BUF		\
		_IOWR('V', BASE_VIDIOC_PRIVATE+8, struct v4l2_buffer)
#define VIDIOC_USER_INT_CHECK			\
		_IOWR('V', BASE_VIDIOC_PRIVATE+9, int)
#define VIDIOC_CHECK_PATH_STATUS		\
		_IOWR('V', BASE_VIDIOC_PRIVATE+9, int)
#define VIDIOC_USER_GET_CAM_STATUS		\
		_IOWR('V', BASE_VIDIOC_PRIVATE+10, int)
#define VIDIOC_SET_WMIXER_OVP			\
		_IOWR('V', BASE_VIDIOC_PRIVATE+11, int)
#define VIDIOC_SET_ALPHA_BLENDING		\
		_IO('V', BASE_VIDIOC_PRIVATE+12)
/* for hdmi in sound sample rate */
#define VIDIOC_USER_AUDIO_SR_CHECK		\
		_IOWR('V', BASE_VIDIOC_PRIVATE+13, int)
/* for hdmi in audio type */
#define VIDIOC_USER_AUDIO_TYPE_CHECK		\
		_IOWR('V', BASE_VIDIOC_PRIVATE+14, int)
#define VIDIOC_USER_AUDIO_OUTPUT_CHECK		\
		_IOWR('V', BASE_VIDIOC_PRIVATE+15, int)
#define VIDIOC_G_LASTFRAME_ADDRS		\
		_IOWR('V', BASE_VIDIOC_PRIVATE+16, int)
#define VIDIOC_CREATE_LASTFRAME			\
		_IOWR('V', BASE_VIDIOC_PRIVATE+17, int)
#define VIDIOC_S_HANDOVER			\
		_IOWR('V', BASE_VIDIOC_PRIVATE+19, unsigned int)
#define VIDIOC_S_LUT				\
		_IOWR('V', BASE_VIDIOC_PRIVATE+20, struct vin_lut)

#endif//__TCC_CAMERA_IOCTL_H__


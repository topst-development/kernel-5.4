// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *      tccvin_subdev.c  --  sub video device
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2020


 *   Description : Sub Video Device handling


 *****************************************************************************/

#include <media/v4l2-subdev.h>

#include "tccvin_video.h"

/*
 * v4l2_subdev_core_ops
 */
int tccvin_subdev_core_init(struct v4l2_subdev *sd, u32 val)
{
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_9_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	return v4l2_subdev_call(sd, core, init, val);
}

int tccvin_subdev_core_load_fw(struct v4l2_subdev *sd)
{
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_9_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	return v4l2_subdev_call(sd, core, load_fw);
}

int tccvin_subdev_core_s_power(struct v4l2_subdev *sd, int on)
{
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_9_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	return v4l2_subdev_call(sd, core, s_power, on);
}


/*
 * v4l2_subdev_video_ops
 */
int tccvin_subdev_video_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_9_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	return v4l2_subdev_call(sd, video, g_input_status, status);
}

int tccvin_subdev_video_s_stream(struct v4l2_subdev *sd, int enable)
{
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_9_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	return v4l2_subdev_call(sd, video, s_stream, enable);
}

int tccvin_subdev_video_g_dv_timings(struct v4l2_subdev *sd, struct v4l2_dv_timings *timings)
{
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_9_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	return v4l2_subdev_call(sd, video, g_dv_timings, timings);
}

int tccvin_subdev_video_g_mbus_config(struct v4l2_subdev *sd, struct v4l2_mbus_config *cfg)
{
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_9_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	return v4l2_subdev_call(sd, video, g_mbus_config, cfg);
}


/*
 * v4l2_subdev_pad_ops
 */
int tccvin_subdev_pad_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_format *format)
{
	int					ret		= 0;

	if (cfg == NULL) {
		sd->entity.num_pads = 1;
		cfg = v4l2_subdev_alloc_pad_config(sd);
		if (IS_ERR_OR_NULL(cfg)) {
			/* error: v4l2_subdev_alloc_pad_config */
			ret = -ENOMEM;
		} else {
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_11_9_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			ret = v4l2_subdev_call(sd, pad, get_fmt, cfg, format);
			v4l2_subdev_free_pad_config(cfg);
		}
	}

	return ret;
}

int tccvin_subdev_pad_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_format *format)
{
	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_9_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	return v4l2_subdev_call(sd, pad, set_fmt, cfg, format);
}

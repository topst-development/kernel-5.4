// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
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
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/div64.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_viqe.h>

#include "tcc_vioc_viqe_interface.h"
#include "viqe.h"

//extern enum TCC_OUTPUT_TYPE Output_SelectMode; //TODO:AlanK It was defined in the tcc_vioc_fb.c
static struct viqe_queue_t g_viqe_render_queue;

static int viqe_get_next_ptr(int cptr)
{
	int nptr = cptr + 1;

	if (nptr >= (VIQE_QUEUE_MAX_ENTRY * 2)) {
		nptr = 0;
		/* prevent KCS warning */
	}

	return nptr;
}

static int viqe_get_index(int cindex)
{
	if (cindex >= VIQE_QUEUE_MAX_ENTRY) {
		cindex -= VIQE_QUEUE_MAX_ENTRY;
		/* prevent KCS warning */
	}

	return cindex;
}

static int viqe_queue_get_filled(const struct viqe_queue_t *p_queue)
{
	int w_ptr = p_queue->w_ptr;
	int r_ptr = p_queue->r_ptr;
	int ret;

	if (w_ptr >= r_ptr) {
		ret = w_ptr - r_ptr;
		/* prevent KCS warning */
	} else {
		ret = w_ptr + ((VIQE_QUEUE_MAX_ENTRY * 2) - r_ptr);
		/* prevent KCS warning */
	}

	return ret;
}

static struct viqe_queue_entry_t *viqe_queue_get_entry_to_push(
	struct viqe_queue_t *p_queue)
{
	int new_idx;

	new_idx = viqe_get_index(p_queue->w_ptr);
	p_queue->w_ptr = viqe_get_next_ptr(p_queue->w_ptr);

	return &p_queue->entry[new_idx];
}

static void viqe_swap_buffer(int mode)
{
	TCC_VIQE_DI_Swap60Hz(mode);
}


int viqe_queue_push_entry(struct viqe_queue_t *p_queue,
	struct viqe_queue_entry_t new_entry)
{
	int new_idx;

	new_idx = viqe_get_index(p_queue->w_ptr);
	(void)memcpy(&p_queue->entry[new_idx], &new_entry, sizeof(struct viqe_queue_entry_t));
	p_queue->w_ptr = viqe_get_next_ptr(p_queue->w_ptr);

	return 0;
}

struct viqe_queue_entry_t *viqe_queue_show_entry(struct viqe_queue_t *p_queue)
{
	int ridx;

	ridx = viqe_get_index(p_queue->r_ptr);

	return &p_queue->entry[ridx];
}

struct viqe_queue_entry_t *viqe_queue_pop_entry(struct viqe_queue_t *p_queue)
{
	int new_idx;

	new_idx = viqe_get_index(p_queue->r_ptr);
	//nRet = p_queue->entry[new_idx].data;
	p_queue->r_ptr = viqe_get_next_ptr(p_queue->r_ptr);

	return &p_queue->entry[new_idx];
}

int viqe_queue_is_empty(const struct viqe_queue_t *p_queue)
{
	int ret;

	ret = (p_queue->r_ptr == p_queue->w_ptr) ? 1 : 0;
	return ret;
}

int viqe_queue_is_full(const struct viqe_queue_t *p_queue)
{
	int ret;

	int w_ptr = p_queue->w_ptr;
	int r_ptr = p_queue->r_ptr;

	if (w_ptr >= r_ptr) {
		if ((w_ptr - r_ptr) < VIQE_QUEUE_MAX_ENTRY) {
			ret = 0;
			/* prevent KCS warning */
		} else {
			ret = 1;
			/* prevent KCS warning */
		}
	} else {
		if ((w_ptr + ((VIQE_QUEUE_MAX_ENTRY * 2) - r_ptr)) < VIQE_QUEUE_MAX_ENTRY) {
			ret = 0;
			/* prevent KCS warning */
		} else {
			ret = 1;
			/* prevent KCS warning */
		}
	}

	return ret;
}

void viqe_queue_show_entry_info(
	const struct viqe_queue_entry_t *p_entry,
	const char *prefix)
{
	(void)p_entry;
	(void)prefix;

#ifdef VIQE_DBG
	pr_debug("[%s] [0x%x,0x%x,0x%x] [BFIELD=%d] [TIMESTAMP=%d] [NEW:%d]"
			, prefix
			, p_entry->frame_base0
			, p_entry->frame_base1
			, p_entry->frame_base2
			, p_entry->bottom_field
			, p_entry->time_stamp
			, p_entry->new_field);
#endif
}

//void viqe_render_init (void * p_rdma_ctrl)
void viqe_render_init(void)
{
	(void)memset(&g_viqe_render_queue, 0, sizeof(struct viqe_queue_t));
	//g_viqe_render_queue.p_rdma_ctrl = p_rdma_ctrl;
}

void viqe_render_frame(const struct tcc_lcdc_image_update *input_image,
		unsigned int field_interval, int curTime, int reset_frmCnt)
{
#if 1
	struct viqe_queue_entry_t *p_entry;
	struct tcc_lcdc_image_update *p_entry_image;

	(void)field_interval;
	(void)curTime;

	//if (Output_SelectMode == TCC_OUTPUT_NONE) {
	//	return;
		/* prevent KCS warning */
	//}

	if ((bool)viqe_queue_is_full(&g_viqe_render_queue)) {
		//pr_info("[INF][VIQE] %s queue is full\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	// to insert first field
	p_entry = viqe_queue_get_entry_to_push(&g_viqe_render_queue);
	p_entry_image = &p_entry->input_image;
	(void)memcpy(p_entry_image, input_image, sizeof(struct tcc_lcdc_image_update));
	p_entry_image->odd_first_flag = (input_image->odd_first_flag == 1U) ? 1U : 0U;
	p_entry->reset_frmCnt = reset_frmCnt;
	p_entry->new_field = 1;

	// to insert second field
	p_entry = viqe_queue_get_entry_to_push(&g_viqe_render_queue);
	p_entry_image = &p_entry->input_image;
	(void)memcpy(p_entry_image, input_image, sizeof(struct tcc_lcdc_image_update));
	p_entry_image->odd_first_flag = (input_image->odd_first_flag == 0U) ? 0U : 1U;
	p_entry->reset_frmCnt = reset_frmCnt;
	p_entry->new_field = 1;

#else

	struct viqe_queue_entry_t p_entry;
	struct tcc_lcdc_image_update *p_entry_image;

	if (viqe_queue_is_full(&g_viqe_render_queue)) {
		//pr_err("[REND] queue is full ...\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto exit;
	}

	p_entry_image = &p_entry.input_image;

	p_entry_image->addr0 = input_image->addr0;
	p_entry_image->addr1 = input_image->addr1;
	p_entry_image->addr2 = input_image->addr2;
	p_entry_image->odd_first_flag = (input_image->odd_first_flag) ? 1 : 0;
	p_entry_image->time_stamp = curTime + 0;
	p_entry_image->Frame_width = input_image->Frame_width;
	p_entry_image->Frame_height = input_image->Frame_height;
	p_entry_image->crop_top = input_image->crop_top;
	p_entry_image->crop_bottom = input_image->crop_bottom;
	p_entry_image->crop_left = input_image->crop_left;
	p_entry_image->crop_right = input_image->crop_right;
	p_entry_image->Image_width = input_image->Image_width;
	p_entry_image->Image_height = input_image->Image_height;
	p_entry_image->offset_x = input_image->offset_x;
	p_entry_image->offset_y = input_image->offset_y;
	p_entry_image->frameInfo_interlace = input_image->frameInfo_interlace;
	p_entry_image->on_the_fly = 1;
	p_entry.reset_frmCnt = reset_frmCnt;
	p_entry.new_field = 1;
	// insert first field
	viqe_queue_push_entry(&g_viqe_render_queue, p_entry);

	p_entry_image->odd_first_flag = (input_image->odd_first_flag) ? 0 : 1;
	p_entry_image->time_stamp = curTime + field_interval;

	// insert second field
	viqe_queue_push_entry(&g_viqe_render_queue, p_entry);
 #endif

	if (g_viqe_render_queue.ready == 0) {
		//viqe_render_field (curTime);
		/* prevent KCS warning */
	}

out:
	;
}

void viqe_render_field(int curTime)
//viqe_render_field(struct tcc_lcdc_image_update *input_image, int curTime)
{
	struct viqe_queue_entry_t *p_entry;
	struct viqe_queue_entry_t *p_entry_next;
	//unsigned int curr_time;
	int next_frame_info;

	//pr_info("@@render_field ...");
	//curr_time = curTime;
	(void)curTime;

	if ((bool)viqe_queue_is_empty(&g_viqe_render_queue)) {
		if (g_viqe_render_queue.ready == 0) {
			//printf ("[RDMA] empty ...\n");
			next_frame_info = VIQE_FIELD_BROKEN;
		} else {
			p_entry = &g_viqe_render_queue.curr_entry;
			next_frame_info = VIQE_FIELD_DUP;
		}
	} else {
		p_entry = &g_viqe_render_queue.curr_entry;
		next_frame_info = VIQE_FIELD_NEW;
		p_entry_next = viqe_queue_show_entry(&g_viqe_render_queue);

		if ((bool)viqe_queue_is_empty(&g_viqe_render_queue)) {
			/* field duplicate */
			/* prevent KCS warning */
		} else if (g_viqe_render_queue.ready == 0) {
			g_viqe_render_queue.ready = 1;
			p_entry = p_entry_next;
			next_frame_info = VIQE_FIELD_NEW;
			(void)viqe_queue_pop_entry(&g_viqe_render_queue);
		} else {
			if (p_entry->input_image.odd_first_flag != p_entry_next->input_image.odd_first_flag) {
				/*
				 * normal condition : T->B->T->B->T->B->T->B->T
				 */
				if ((p_entry->input_image.odd_first_flag != 0U) && ((viqe_queue_get_filled(&g_viqe_render_queue)) >= 3)) {

					/*
					 * in case of source frame rate is higher than sink(display) frame rate.
					 * for example, 30i contents and 50p display
					 */

					/* 1. remove a field */
					(void)viqe_queue_pop_entry(&g_viqe_render_queue);
					p_entry = viqe_queue_show_entry(&g_viqe_render_queue);
					next_frame_info = VIQE_FIELD_SKIP;
					(void)viqe_queue_pop_entry(&g_viqe_render_queue);
				} else {
					p_entry = p_entry_next;
					next_frame_info = VIQE_FIELD_NEW;
					(void)viqe_queue_pop_entry(&g_viqe_render_queue);
				}
			} else {
				/*
				 * abnormal condition : T->B->B->T->T->B->B->T->T
				 */
				p_entry = p_entry_next;
				next_frame_info = VIQE_FIELD_SKIP;
				(void)viqe_queue_pop_entry(&g_viqe_render_queue);
			}
		}

		if (p_entry->new_field == 0) {
			next_frame_info = VIQE_FIELD_DUP;
			/* prevent KCS warning */
		}
	}

	if (next_frame_info == VIQE_FIELD_BROKEN) {
		// do nothing
		//pr_info("[RDMA:SKIP]\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto out;
	}

	if (g_viqe_render_queue.frame_cnt < 4) {
		next_frame_info = VIQE_FIELD_NEW;
		/* prevent KCS warning */
	}

	switch (next_frame_info) {
	case VIQE_FIELD_SKIP:
		//viqe_queue_show_entry_info(p_entry, "RDMA:SKIP");
		viqe_swap_buffer(SKIP_MODE);
		if ((g_viqe_render_queue.inout_rate++) >= 10) {
			g_viqe_render_queue.inout_rate = 10;
			/* prevent KCS warning */
		}
		break;
	case VIQE_FIELD_NEW:
		//viqe_queue_show_entry_info(p_entry, "RDMA:NEW");
		viqe_swap_buffer(NORMAL_MODE);
		break;
	case VIQE_FIELD_DUP:
		//viqe_queue_show_entry_info(p_entry, "RDMA:DUP");
		viqe_swap_buffer(DUPLI_MODE);
		if ((g_viqe_render_queue.inout_rate--) <= -10) {
			g_viqe_render_queue.inout_rate = -10;
			/* prevent KCS warning */
		}
		break;
	//case VIQE_FIELD_BROKEN:
	//	//viqe_queue_show_entry_info(p_entry, "RDMA:BROKEN");
	//	//pr_err("[ERROR] : BROKEN FIELD OCCURRED ....\n");
	//	//sim_fail();
	//	break;
	default:
		(void)pr_err("[ERR] default\n");
		break;
	}

//	sim_value (g_viqe_render_queue.inout_rate);
	if (g_viqe_render_queue.inout_rate <= -3) {
		TCC_VIQE_DI_SetFMT60Hz(0U);
		/* prevent KCS warning */
	} else if (g_viqe_render_queue.inout_rate >= 3) {
		TCC_VIQE_DI_SetFMT60Hz(1U);
		/* prevent KCS warning */
	} else {
		/* prevent KCS warning */
	}

	if ((bool)g_viqe_render_queue.ready) {
		//viqe_queue_show_entry_info (p_entry, "RDMA:WRITE");
		if (g_viqe_render_queue.frame_cnt < 4) {
			p_entry->input_image.odd_first_flag = (unsigned int)g_viqe_render_queue.frame_cnt & 0x1U;
			/* prevent KCS warning */
		}

	#if 0
		TCC_VIQE_DI_Run60Hz(1,
			p_entry->frame_base0, p_entry->frame_base1,
			p_entry->frame_base2, p_entry->crop_top,
			p_entry->crop_bottom, p_entry->crop_left,
			p_entry->crop_right,
			p_entry->Frame_width, p_entry->Frame_height,
			p_entry->Image_width, p_entry->Image_height,
			p_entry->offset_x, p_entry->offset_y,
			p_entry->bottom_field, p_entry->frameInfo_interlace,
			p_entry->reset_frmCnt);
	#else
		#if defined(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT) || defined(TCC_VIDEO_DISPLAY_BY_VSYNC_INT)
		if (p_entry->input_image.m2m_mode) {
			TCC_VIQE_DI_Run60Hz_M2M(&p_entry->input_image, p_entry->reset_frmCnt);
			/* prevent KCS warning */
		} else
		#endif
		{
			TCC_VIQE_DI_Run60Hz(&p_entry->input_image, p_entry->reset_frmCnt);
			/* prevent KCS warning */
		}
	#endif

		if (++g_viqe_render_queue.frame_cnt >= 6) {
			g_viqe_render_queue.frame_cnt = 6;
			/* prevent KCS warning */
		}
	}

	p_entry->new_field = 0;
	(void)memmove(&g_viqe_render_queue.curr_entry, p_entry, sizeof(struct viqe_queue_entry_t));

out:
	;
}

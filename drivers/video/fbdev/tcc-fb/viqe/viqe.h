/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef VIQE_H__
#define VIQE_H__

#define VIQE_QUEUE_MAX_ENTRY (12) //(8)

#define VIQE_FIELD_NEW       (0)
#define VIQE_FIELD_DUP       (1)
#define VIQE_FIELD_SKIP      (2)
#define VIQE_FIELD_BROKEN    (3)

struct viqe_queue_entry_t {
	struct tcc_lcdc_image_update input_image;
	int new_field;
	int reset_frmCnt;
};

struct viqe_queue_t {
	/* common information */
	int ready;
	struct viqe_queue_entry_t curr_entry;

	struct viqe_queue_entry_t entry[VIQE_QUEUE_MAX_ENTRY];
	int w_ptr;
	int r_ptr;

	/* frame rate vs. output rate
	 * + : input frame rate is greater than output frame rate
	 * - : output frame rate is greater than input frame rate
	 */
	int inout_rate;

	/* hardware releated */
	void *p_rdma_ctrl;
	int frame_cnt;
};

extern int viqe_queue_push_entry(struct viqe_queue_t *p_queue, struct viqe_queue_entry_t new_entry);
extern struct viqe_queue_entry_t *viqe_queue_pop_entry(struct viqe_queue_t *p_queue);
extern int viqe_queue_is_empty(const struct viqe_queue_t *p_queue);
extern int viqe_queue_is_full(const struct viqe_queue_t *p_queue);
extern struct viqe_queue_entry_t *viqe_queue_show_entry(struct viqe_queue_t *p_queue);
extern void viqe_queue_show_entry_info(const struct viqe_queue_entry_t *p_entry, const char *prefix);
extern void viqe_render_init(void);
extern void viqe_render_frame(const struct tcc_lcdc_image_update *input_image, unsigned int field_interval, int curTime, int reset_frmCnt);
extern void viqe_render_field(int curTime);

#endif /*VIQE_H__*/

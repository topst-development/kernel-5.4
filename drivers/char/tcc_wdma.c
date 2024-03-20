// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
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
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/limits.h>
#include <asm/div64.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/tcc_wdma_ioctrl.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/tcc_overlay_ioctl.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <linux/uaccess.h>

//#define WDMA_DEBUG
#ifdef WDMA_DEBUG
#define dprintk(msg...) (void)pr_info("[DBG][WDMA] " msg)
#else
#define dprintk(msg...)
#endif

/* To debug wdma image
 *  The image buffer set to 0x12 before writing the image data by WDMA block.
 */
//#define WDMA_IMAGE_DEBUG
#define CHECKING_NUM             (0x12)
#define CHECKING_START_POS(x, y) (x * (y / 3))
#define CHECKING_AREA(x, y)      (x * (y / 10))

enum {
	PREPARED_S,
	WRITING_S,
	WRITED_S,
	UPLOAD_S,
	WDMA_S_MAX
};

struct wdma_buffer_list {
	unsigned int status;
	unsigned int base_Yaddr;
	unsigned int base_Uaddr;
	unsigned int base_Vaddr;
	char *vbase_Yaddr;
};

struct wdma_queue_list {
	unsigned int q_max_cnt;
	unsigned int q_index;//curreunt writing buffer index
	struct wdma_buffer_list *wbuf_list;

	struct vioc_wdma_frame_info *data;
};

struct tcc_wdma_dev_vioc {
	void __iomem *reg;
	unsigned int id;
	//unsigned int path;
};

struct tcc_vioc_block {
	void __iomem *virt_addr; // virtual address
	unsigned int irq_num;
	unsigned int blk_num; //block number like dma number or mixer number
};

struct tcc_wdma_dev {
	struct vioc_intr_type *vioc_intr;
	unsigned int irq;

	struct miscdevice *misc;

	struct tcc_vioc_block disp_info;
	struct tcc_wdma_dev_vioc rdma;
	struct tcc_wdma_dev_vioc sc;
	struct tcc_wdma_dev_vioc wdma;
	struct tcc_wdma_dev_vioc wmix;

	// wait for poll
	wait_queue_head_t poll_wq;
	spinlock_t poll_lock;
	unsigned int poll_count;

	// wait for ioctl command
	wait_queue_head_t cmd_wq;
	spinlock_t cmd_lock;
	unsigned int cmd_count;

	struct mutex io_mutex;
	unsigned char block_operating;
	unsigned char block_waiting;
	unsigned char irq_reged;
	unsigned int dev_opened;

	unsigned char wdma_contiuous;
	struct wdma_queue_list frame_list;
	struct clk *wdma_clk;

	//extend infomation
	unsigned int fb_dd_num;
};

/*
 * sys-fs
 */
static char screencapture[60] = {0,};

int wdma_queue_list_init(struct wdma_queue_list *frame_list,
	struct vioc_wdma_frame_info *cmd_data);
unsigned int wdma_queue_list_exit(struct wdma_queue_list *frame_list);
unsigned int wdma_get_writable_buffer_index(struct wdma_queue_list *frame_list);
int wdma_set_index_of_status(struct wdma_queue_list *frame_list,
	unsigned int index, unsigned int wdma_s);
int tc_wdrv_scaler_set(struct tcc_wdma_dev *pwdma_data, unsigned int out_w,
	unsigned int out_h);
char tc_wdrv_wdma_path_set(struct tcc_wdma_dev *pwdma_data,
	struct vioc_wdma_frame_info *data_info, struct wdma_buffer_list *buffer);
char tc_wdrv_wdma_addr_set(struct tcc_wdma_dev *pwdma_data, unsigned int addrY,
	unsigned int addrU, unsigned int addrV);
long tccxxx_wdma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
int tccxxx_wdma_release(struct inode *inode, struct file *filp);
int tccxxx_wdma_open(struct inode *inode, struct file *filp);
char tccxxx_wdma_ctrl(unsigned long argp, struct tcc_wdma_dev *pwdma_data);
static ssize_t screen_capture_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t screen_capture_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

void wdma_attr_create(struct platform_device *pdev);
void wdma_attr_remove(struct platform_device *pdev);

static inline long wdma_msecs_to_jiffies(int msec)
{
	unsigned long ulmsecs;
	long lmsec;

	if (msec < 0) {
		lmsec = 0;
	} else {
		ulmsecs = msecs_to_jiffies((unsigned int)msec);

		if (ulmsecs > (ULONG_MAX >> 1)) {
			ulmsecs = (ULONG_MAX >> 1);
			lmsec = (long)ulmsecs;
		} else {
			lmsec = (long)ulmsecs;
		}
	}
	return lmsec;
}

/* coverity[misra_c_2012_rule_5_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tccxxx_wdma_mmap(struct file *filp, struct vm_area_struct *vma)
{
	//if (range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
	//	(void)pr_err("[ERR][WDMA] this address is not allowed.\n");
	//	return -EAGAIN;
	//}
	int ret = 0;
	(void)filp;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (vma->vm_end >= vma->vm_start){
		/* prevent KCS */
		if ((bool)remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto err_remap;
		}
	}
	vma->vm_ops =  NULL;
	vma->vm_flags |= (unsigned long)VM_IO;
	vma->vm_flags |= (unsigned long)VM_DONTEXPAND | (unsigned long)VM_PFNMAP;

err_remap:
	return ret;
}

int wdma_queue_list_init(struct wdma_queue_list *frame_list,
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct vioc_wdma_frame_info *cmd_data)
{
	unsigned int i, frame_size;
	unsigned int addrY = 0, addrU = 0, addrV = 0;
	int ret = 0;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	frame_list->data = kmalloc(sizeof(struct vioc_wdma_frame_info),
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
		GFP_KERNEL);

	if (frame_list->data == NULL) {
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto malloc_fail;
	}

	*frame_list->data = *cmd_data;

	// Frame format is set to  TCC_LCDC_IMG_FMT_YUV420SP.
	if ((UINT_MAX / cmd_data->frame_y) >= cmd_data->frame_x) {
		/* KCS */
		frame_size = (cmd_data->frame_x * cmd_data->frame_y * 3U) / 2U;
	} else {
		(void)pr_err("[ERR][WDMA][%d] frame size overflow\n", __LINE__);
		ret = -1;
		goto normal_return;
	}
	/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
	frame_list->q_max_cnt = cmd_data->buff_size / frame_size;
	frame_list->data->buffer_num = frame_list->q_max_cnt;

	frame_list->q_index = frame_list->q_max_cnt;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	frame_list->wbuf_list = kmalloc_array(frame_list->q_max_cnt,
		sizeof(struct wdma_buffer_list),
		/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
		GFP_KERNEL);

	if (frame_list->wbuf_list == NULL) {
		(void)pr_err("[ERR][WDMA] list alloc error\n");
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto list_alloc_fail;
	}

	for (i = 0; i < frame_list->q_max_cnt; i++) {
		unsigned char frame_fmt;
		unsigned int base_addr, offset;

		frame_list->wbuf_list[i].status = PREPARED_S;

		if (frame_size == 0) {
			offset = 0;
		} else if ((UINT_MAX / frame_size) >= i){
			offset = (i * frame_size);
		} else {
			/* offset is wrong */
			continue;
		}
		if ((UINT_MAX - cmd_data->buff_addr) < offset) {
			/* address is wrong */
			continue;
		} else {
			base_addr = cmd_data->buff_addr + offset;
		}

		addrY = 0U; addrU = 0U; addrV = 0U;
		if (cmd_data->frame_fmt < 0xFFU) {
			frame_fmt = (unsigned char)cmd_data->frame_fmt;
		} else {
			/* fmt is wrong */
			continue;
		}
		tcc_get_addr_yuv(frame_fmt, base_addr,
			cmd_data->frame_x, cmd_data->frame_y,
			0, 0, &addrY, &addrU, &addrV);

		frame_list->wbuf_list[i].base_Yaddr = addrY;
		frame_list->wbuf_list[i].base_Uaddr = addrU;
		frame_list->wbuf_list[i].base_Vaddr = addrV;
		#ifdef WDMA_IMAGE_DEBUG
		frame_list->wbuf_list[i].vbase_Yaddr =
			(char *)ioremap_nocache(addrY,
				cmd_data->frame_x * cmd_data->frame_y);
		#endif//
		dprintk("id : %3d addr : 0x%08x 0x%08x 0x%08x\n", i,
			frame_list->wbuf_list[i].base_Yaddr,
			frame_list->wbuf_list[i].base_Uaddr,
			frame_list->wbuf_list[i].base_Vaddr);
	}

	dprintk("start list :size : %d x %d  buffer_number:%d\n",
		frame_list->data->frame_y, frame_list->data->frame_x,
		frame_list->data->buffer_num);

	ret = (int)frame_list->q_max_cnt;
	/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
	goto normal_return;

list_alloc_fail:
	kfree(frame_list->data);
	(void)pr_err("[ERR][WDMA] queue list kmalloc() failed!\n");
malloc_fail:
	(void)pr_err("[ERR][WDMA] queue list kmalloc() failed!\n");
normal_return:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
unsigned int wdma_queue_list_exit(struct wdma_queue_list *frame_list)
{
	if (frame_list->wbuf_list != NULL) {
		kfree(frame_list->wbuf_list);
		/* prevent KCS warning */
	}

	if (frame_list->data != NULL) {
		kfree(frame_list->data);
		/* prevent KCS warning */
	}

	return 0;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
unsigned int wdma_get_writable_buffer_index(struct wdma_queue_list *frame_list)
{
	unsigned int i;
	unsigned int cur_index = 0;

	cur_index = frame_list->q_index;

	if ((UINT_MAX - frame_list->q_max_cnt) >= cur_index) {
		/* KCS */
		for (i = 0U; i < frame_list->q_max_cnt; i++) {
			cur_index++;
			if (cur_index >= frame_list->q_max_cnt) {
				/* Prevent KCS */
				cur_index = 0;
			}

			if ((frame_list->wbuf_list[cur_index].status == (unsigned int)PREPARED_S)
			|| (frame_list->wbuf_list[cur_index].status == (unsigned int)WRITED_S)) {
				break;
			}
		}
	}

	return cur_index;
}

// find index of wdma_s status buffer index.
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int wdma_find_index_of_status(struct wdma_queue_list *frame_list,
	unsigned int wdma_s)
{
	unsigned int find_index = 0;
	int ret;
	unsigned int i;

	if (wdma_s >= (unsigned int)WDMA_S_MAX){
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_index;
	}

	if (wdma_s == (unsigned int)WRITED_S) {
		if (frame_list->q_index == 0U) {
			/* Prevent KCS */
			find_index = (frame_list->q_max_cnt - 2U);
		}
		else if (frame_list->q_index == 1U) {
			/* Prevent KCS */
			find_index = (frame_list->q_max_cnt - 1U);
		}
		else {
			/* Prevent KCS */
			find_index = (frame_list->q_index - 2U);
		}
	} else {
		/* Prevent KCS */
		find_index = 0U;
	}

	for (i = 0U; i <= frame_list->q_max_cnt; i++) {
		if (frame_list->wbuf_list[find_index].status == wdma_s) {
			/* Prevent KCS */
			break;
		}

		if (find_index == 0U) {
			/* Prevent KCS */
			find_index = frame_list->q_max_cnt - 1U;
		}
		else {
			/* Prevent KCS */
			find_index--;
		}
	}

	if (i >= frame_list->q_max_cnt) {
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_index;
	}
	if (find_index > (UINT_MAX/2U)) {
		/* KCS */
		ret = 0;
	} else {
		/* KCS */
		ret = (int)find_index;
	}
err_index:
	return ret;
}
// set status
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
int wdma_set_index_of_status(struct wdma_queue_list *frame_list,
	unsigned int index, unsigned int wdma_s)
{
	int ret;

	if ((wdma_s >= (unsigned int)WDMA_S_MAX) || (frame_list->q_max_cnt <= index)) {
		/* prevent KCS warning */
		ret = -1;
	} else {
		/* KCS */
		ret = 0;
	}

	if (ret != -1) {
		/* KCS */
		frame_list->wbuf_list[index].status = wdma_s;
		if (wdma_s == (unsigned int)WRITING_S) {
			/* Prevent KCS */
			frame_list->q_index = index;
		}
		ret = 0;
	}
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
int tc_wdrv_scaler_set(struct tcc_wdma_dev *pwdma_data, unsigned int out_w,
	unsigned int out_h)
{
	uint32_t Wmix_Height = 0, Wmix_Width = 0;

	VIOC_WMIX_GetSize(pwdma_data->wmix.reg, &Wmix_Width, &Wmix_Height);

	if ((Wmix_Width == out_w) && (Wmix_Height == out_h)) {
		pr_info("Scaler bypass => w:%d h:%d\n",
			Wmix_Width,
			Wmix_Height);

		VIOC_SC_SetBypass(pwdma_data->sc.reg, 1);
	}
	else {
		/* Prevent KCS */
		VIOC_SC_SetBypass(pwdma_data->sc.reg, 0);
	}

	VIOC_SC_SetDstSize(pwdma_data->sc.reg, out_w, out_h);
	VIOC_SC_SetOutSize(pwdma_data->sc.reg, out_w, out_h);
	VIOC_SC_SetOutPosition(pwdma_data->sc.reg, 0, 0);
	(void)VIOC_CONFIG_PlugIn(pwdma_data->sc.id, pwdma_data->wdma.id);
	VIOC_SC_SetUpdate(pwdma_data->sc.reg);

	return 0;
}

char tc_wdrv_wdma_path_set(struct tcc_wdma_dev *pwdma_data,
	struct vioc_wdma_frame_info *data_info, struct wdma_buffer_list *buffer)
{
	unsigned int dd_rgb = 0;
	struct DisplayBlock_Info DDinfo;

	(void)pr_info("[INF][WDMA][%s] pwdma_data:%p, data_info: %p, buffer: %p\n",
		__func__, pwdma_data, data_info, buffer);

	spin_lock_irq(&(pwdma_data->cmd_lock));

	buffer->status = WRITING_S;

	if ((bool)pwdma_data->fb_dd_num) {
		(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA04, 1 /* Mixing */);
		(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA07, 1 /* Mixing */);
	} else {
		(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA00, 1 /* Mixing */);
		(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA03, 1 /* Mixing */);
	}

	VIOC_WDMA_SetImageSize(pwdma_data->wdma.reg, data_info->frame_x,
		data_info->frame_y);
	VIOC_WDMA_SetImageFormat(pwdma_data->wdma.reg, data_info->frame_fmt);
	VIOC_WDMA_SetImageOffset(pwdma_data->wdma.reg, data_info->frame_fmt,
		data_info->frame_x);
	VIOC_WDMA_SetImageBase(pwdma_data->wdma.reg, buffer->base_Yaddr,
		buffer->base_Uaddr, buffer->base_Vaddr);
	VIOC_WDMA_SetImageR2YMode(pwdma_data->wdma.reg, 0x2);

	// check display device format
	VIOC_DISP_GetDisplayBlock_Info(pwdma_data->disp_info.virt_addr,
		&DDinfo);
	dd_rgb = VIOC_DISP_FMT_isRGB(DDinfo.pCtrlParam.pxdw);

	if (data_info->frame_fmt <= (unsigned int)TCC_LCDC_IMG_FMT_ARGB6666_3) {
		if ((bool)dd_rgb) {
			VIOC_WDMA_SetImageY2RMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 0);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 0);
		} else {
			VIOC_WDMA_SetImageY2RMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 1);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 0);
		}
	} else {
		if ((bool)dd_rgb) {
			VIOC_WDMA_SetImageR2YMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 1);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 0);
		} else {
			VIOC_WDMA_SetImageR2YMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 0);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 0);
		}
	}

	VIOC_WDMA_SetImageRGBSwapMode(pwdma_data->wdma.reg, 0);
	VIOC_WDMA_SetImageEnable(pwdma_data->wdma.reg,
		pwdma_data->wdma_contiuous);
	spin_unlock_irq(&(pwdma_data->cmd_lock));

	return (char)0;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
char tc_wdrv_wdma_addr_set(struct tcc_wdma_dev *pwdma_data, unsigned int addrY,
	unsigned int addrU, unsigned int addrV)
{
//	dprintk("0x%x 0x%x 0x%x\n",addrY, addrU, addrV);
	VIOC_WDMA_SetImageBase(pwdma_data->wdma.reg, addrY, addrU, addrV);
	VIOC_WDMA_SetImageUpdate(pwdma_data->wdma.reg);
	return (char)0;
}


char tccxxx_wdma_ctrl(unsigned long argp, struct tcc_wdma_dev *pwdma_data)
{
	struct VIOC_WDMA_IMAGE_INFO_Type ImageCfg;
	unsigned int base_addr = 0U, Wmix_Height = 0U, Wmix_Width = 0;
	unsigned int DDevice = 0U;
	int ret = 0;
	unsigned int dd_rgb = 0;
	unsigned int addr_Y = 0U,  addr_U = 0U, addr_V = 0U;
	long ltemp = 0;
	struct DisplayBlock_Info DDinfo;

	(void)memset(&ImageCfg, 0x00, sizeof(ImageCfg));

	/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
	if ((bool)copy_from_user((void *)&ImageCfg, (const void __user *)argp,
		sizeof(ImageCfg))) {
		(void)pr_err("[ERR][WDMA][%s] error\n", __func__);
		ret = -EFAULT;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto return_funcs;
	}

	VIOC_WMIX_GetSize(pwdma_data->wmix.reg, &Wmix_Width, &Wmix_Height);

	DDevice = VIOC_DISP_Get_TurnOnOff(pwdma_data->disp_info.virt_addr);

	if ((Wmix_Width == 0U) || (Wmix_Height == 0U) || (DDevice == 0U)) {
		pwdma_data->block_operating = 0;

		(void)pr_err("[ERR][WDMA][%s] W:%d H:%d DD-Power:%d\n", __func__, Wmix_Width, Wmix_Height, DDevice);

		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto return_funcs;
	}

	ImageCfg.Interlaced = 0;
	ImageCfg.ContinuousMode = 0;
	ImageCfg.SyncMode = 0;
	ImageCfg.ImgSizeWidth = Wmix_Width;
	ImageCfg.ImgSizeHeight = Wmix_Height;

	base_addr = (unsigned int)ImageCfg.BaseAddress;

	tcc_get_addr_yuv(ImageCfg.ImgFormat, base_addr,
		ImageCfg.TargetWidth, ImageCfg.TargetHeight, 0, 0,
		&addr_Y, &addr_U, &addr_V);

	if ((ImageCfg.ImgFormat == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP)
		|| (ImageCfg.ImgFormat == (unsigned int)TCC_LCDC_IMG_FMT_RGB888)) {
		addr_U = GET_ADDR_YUV42X_spU(base_addr, ImageCfg.TargetWidth,
			ImageCfg.TargetHeight);

		if (ImageCfg.ImgFormat == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP) {
			addr_V = GET_ADDR_YUV420_spV(addr_U,
				ImageCfg.TargetWidth, ImageCfg.TargetHeight);
		} else {
			addr_V = GET_ADDR_YUV422_spV(addr_U,
				ImageCfg.TargetWidth, ImageCfg.TargetHeight);
		}
	}

	ImageCfg.BaseAddress = addr_Y;
	ImageCfg.BaseAddress1 = addr_U;
	ImageCfg.BaseAddress2 = addr_V;

	dprintk("Src  w:%d h:%d base:0x%08x\n",
			ImageCfg.ImgSizeWidth,
			ImageCfg.ImgSizeHeight,
			ImageCfg.BaseAddress);
	dprintk("Dst w:%d h:%d fb_dd_num:%d\n", ImageCfg.TargetWidth,
		ImageCfg.TargetHeight, pwdma_data->fb_dd_num);
	dprintk("Wmixer size %d %d :base1:0x%08x base2:0x%08x\n", Wmix_Width,
		Wmix_Height, ImageCfg.BaseAddress1, ImageCfg.BaseAddress2);
	dprintk("ImgFormat:%d  set effect : hue:%d, bright:%d contrast:%d\n",
		ImageCfg.ImgFormat, ImageCfg.Hue, ImageCfg.Bright,
		ImageCfg.Contrast);

	if (pwdma_data->sc.reg != NULL) {
		if ((Wmix_Width == ImageCfg.TargetWidth) && (Wmix_Height == ImageCfg.TargetHeight)) {
			/* For KCS */
			VIOC_SC_SetBypass(pwdma_data->sc.reg, 1);
		}
		else {
			/* For KCS */
			VIOC_SC_SetBypass(pwdma_data->sc.reg, 0);
		}

		VIOC_SC_SetDstSize(pwdma_data->sc.reg, ImageCfg.TargetWidth, ImageCfg.TargetHeight);
		VIOC_SC_SetOutSize(pwdma_data->sc.reg, ImageCfg.TargetWidth, ImageCfg.TargetHeight);
		VIOC_SC_SetOutPosition(pwdma_data->sc.reg, 0, 0);
		(void)VIOC_CONFIG_PlugIn(pwdma_data->sc.id, pwdma_data->wdma.id);
		VIOC_SC_SetUpdate(pwdma_data->sc.reg);
	} else {
		if ((Wmix_Width != ImageCfg.TargetWidth) || (Wmix_Height != ImageCfg.TargetHeight)) {
			(void)pr_err("ERR][WDMA]Frame size is same as %d x %d <-> %d x %d\n",
						Wmix_Width,
						Wmix_Height,
						ImageCfg.TargetWidth,
						ImageCfg.TargetHeight);
			ret = -EFAULT;

			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto return_funcs;
		}
	}

	spin_lock_irq(&(pwdma_data->cmd_lock));

	if ((bool)pwdma_data->fb_dd_num) {
		(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA04, 1 /* Mixing */);
		(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA07, 1 /* Mixing */);
	} else {
		(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA00, 1 /* Mixing */);
		(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA03, 1 /* Mixing */);
	}

	// check display device format
	VIOC_DISP_GetDisplayBlock_Info(pwdma_data->disp_info.virt_addr,
		&DDinfo);

	dd_rgb = VIOC_DISP_FMT_isRGB(DDinfo.pCtrlParam.pxdw);

	if (ImageCfg.ImgFormat <= (unsigned int)TCC_LCDC_IMG_FMT_ARGB6666_3) {
		if ((bool)dd_rgb) {
			VIOC_WDMA_SetImageY2RMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 0);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 0);
		} else {
			VIOC_WDMA_SetImageY2RMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 1);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 0);
		}
	} else {
		if ((bool)dd_rgb) {
			VIOC_WDMA_SetImageR2YMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 1);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 0);
		} else {
			VIOC_WDMA_SetImageR2YMode(pwdma_data->wdma.reg, 0x2);
			VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, 0);
			VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, 0);
		}
	}

	VIOC_WDMA_SetImageSize(pwdma_data->wdma.reg, ImageCfg.TargetWidth,
		ImageCfg.TargetHeight);
	VIOC_WDMA_SetImageFormat(pwdma_data->wdma.reg, ImageCfg.ImgFormat);
	VIOC_WDMA_SetImageOffset(pwdma_data->wdma.reg, ImageCfg.ImgFormat,
		ImageCfg.TargetWidth);
	VIOC_WDMA_SetImageBase(pwdma_data->wdma.reg, ImageCfg.BaseAddress,
		ImageCfg.BaseAddress1, ImageCfg.BaseAddress2);
	VIOC_WDMA_SetImageRGBSwapMode(pwdma_data->wdma.reg, 0);
	#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	VIOC_WDMA_SetImageEnhancer(pwdma_data->wdma.reg, ImageCfg.Contrast,
		ImageCfg.Bright, ImageCfg.Hue);
	#endif
	VIOC_WDMA_SetImageEnable(pwdma_data->wdma.reg, ImageCfg.ContinuousMode);

	spin_unlock_irq(&(pwdma_data->cmd_lock));
	pwdma_data->vioc_intr->bits = (1U << (unsigned int)VIOC_WDMA_INTR_EOFR);
	(void)vioc_intr_clear(pwdma_data->vioc_intr->id, pwdma_data->vioc_intr->bits);
	if (pwdma_data->irq <= (UINT_MAX/2U)) {
		/* KCS */
		(void)vioc_intr_enable((int)pwdma_data->irq, pwdma_data->vioc_intr->id,
			pwdma_data->vioc_intr->bits);
	}
		/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
		/* coverity[cert_dcl37_c_violation : FALSE] */
		/* coverity[cert_int31_c_violation : FALSE] */
	ltemp = wait_event_interruptible_timeout(pwdma_data->poll_wq,
		pwdma_data->block_operating == 0U, wdma_msecs_to_jiffies(100));
	if ((unsigned int)ltemp <= (UINT_MAX/2U)) {
		ret = (int)ltemp;
	}
	if (ret <= 0) {
		pwdma_data->block_operating = 0;
		(void)pr_warn("[WAR][WDMA] time out: %d, Line: %d.\n", ret, __LINE__);
	}

	if (pwdma_data->sc.reg != NULL) {
		/* KCS */
		(void)VIOC_CONFIG_PlugOut(pwdma_data->sc.id);
	}
	/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
	if ((bool)copy_to_user((void __user *)argp, &ImageCfg, sizeof(ImageCfg))) {
		/* prevent KCS warning */
		ret = -EFAULT;
	}

return_funcs:
	return (char)ret;
}

static unsigned int tccxxx_wdma_poll(struct file *filp, poll_table *wait)
{
	int ret = 0;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_wdma_dev *wdma_data = dev_get_drvdata(misc->parent);

	if (wdma_data == NULL) {
		/* KCS */
		ret = -EFAULT;
	}

	if (ret >= 0) {
		poll_wait(filp, &(wdma_data->poll_wq), wait);

		spin_lock_irq(&(wdma_data->poll_lock));

		if (wdma_data->block_operating == 0U) {
			/* KCS */
			ret = (int)((unsigned int)POLLIN|(unsigned int)POLLRDNORM);
		}

		spin_unlock_irq(&(wdma_data->poll_lock));
	}

	return (unsigned int)ret;
}

static irqreturn_t tccxxx_wdma_handler(int irq, void *client_data)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_wdma_dev *wdma_data = client_data;
	irqreturn_t ret;
	int itemp = 0;
	(void)irq;

	if (!is_vioc_intr_activatied(wdma_data->vioc_intr->id,
		wdma_data->vioc_intr->bits)) {
		ret = IRQ_NONE;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto irq_handle_return;
	}

	//if (ret != IRQ_NONE){
		if (wdma_data->block_operating >= 1U) {
			/* KCS */
			wdma_data->block_operating = 0;
		}

		wake_up_interruptible(&(wdma_data->poll_wq));

		if ((bool)wdma_data->block_waiting) {
			/* KCS */
			wake_up_interruptible(&wdma_data->cmd_wq);
		}


		if ((bool)wdma_data->wdma_contiuous) {
			unsigned int next_id, cur_id = 0;
			if (wdma_find_index_of_status(&wdma_data->frame_list,
				WRITING_S) >= 0) {
					/* KCS */
				itemp = wdma_find_index_of_status(&wdma_data->frame_list,
					WRITING_S);
				if (itemp >= 0) {
					/* KCS */
					cur_id = (unsigned int)itemp;
				} else {
					cur_id = 0;
					(void)pr_err("[WDMA][%d] cur_id is negative value\n", __LINE__);
				}
			}
			next_id = wdma_get_writable_buffer_index(
				&wdma_data->frame_list);
			(void)tc_wdrv_wdma_addr_set(wdma_data,
				wdma_data->frame_list.wbuf_list[next_id].base_Yaddr,
				wdma_data->frame_list.wbuf_list[next_id].base_Uaddr,
				wdma_data->frame_list.wbuf_list[next_id].base_Vaddr);
			(void)wdma_set_index_of_status(&wdma_data->frame_list, next_id,
				WRITING_S);
			(void)wdma_set_index_of_status(&wdma_data->frame_list, cur_id,
				WRITED_S);

			#ifdef WDMA_IMAGE_DEBUG
			memset(wdma_data->frame_list.wbuf_list[next_id].vbase_Yaddr +
				CHECKING_START_POS(wdma_data->frame_list.data->frame_x,
				wdma_data->frame_list.data->frame_y), CHECKING_NUM,
				CHECKING_AREA(wdma_data->frame_list.data->frame_x,
				wdma_data->frame_list.data->frame_y));
			#endif

			//if (wdma_data->block_waiting) {
			//	(void)pr_info("%s C:%d N:%d, Y:0x%08x U:0x%08x V:0x%08x\n",
			//	__func__, cur_id, next_id,
			//	wdma_data->frame_list.wbuf_list[next_id].base_Yaddr,
			//	wdma_data->frame_list.wbuf_list[next_id].base_Uaddr,
			//	wdma_data->frame_list.wbuf_list[next_id].base_Vaddr);
			//}
		}
		(void)vioc_intr_clear(wdma_data->vioc_intr->id, VIOC_WDMA_INT_MASK);

		ret = IRQ_HANDLED;
	//}
irq_handle_return :
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
long tccxxx_wdma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_wdma_dev *wdma_data = dev_get_drvdata(misc->parent);

	dprintk("%s: cmd(0x%x), bo(0x%x), bw(0x%x), cc(0x%x), pc(0x%x)\n",
		__func__,
		cmd, wdma_data->block_operating, wdma_data->block_waiting,
		wdma_data->cmd_count, wdma_data->poll_count);

	switch (cmd) {
	case TCC_WDMA_IOCTRL:
		mutex_lock(&wdma_data->io_mutex);

		if ((bool)wdma_data->block_operating) {
			wdma_data->block_waiting = 1;
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
			ret = wait_event_interruptible_timeout(
				wdma_data->cmd_wq,
				wdma_data->block_operating == 0U,
				wdma_msecs_to_jiffies(200));
			if (ret <= 0) {
				wdma_data->block_operating = 0;
				(void)pr_info("[INF][WDMA] ret: %d : wdma 0 timed_out block_operation:%d!! cmd_count:%d\n",
					ret, wdma_data->block_waiting,
					wdma_data->cmd_count);
			}
			ret = 0;
		}

		//if (ret >= 0) {
			if (wdma_data->block_operating >= 1U) {
				(void)pr_info("[INF][WDMA] wdma driver :: block_operating(%d) - block_waiting(%d) - cmd_count(%d) - poll_count(%d)!!!\n",
					wdma_data->block_operating,
					wdma_data->block_waiting,
					wdma_data->cmd_count,
					wdma_data->poll_count);
			}

			wdma_data->block_waiting = 0;
			wdma_data->block_operating = 1;

			ret = (int)tccxxx_wdma_ctrl(arg, wdma_data);// function call

			if (ret >= 0) {
				/* KCS */
				wdma_data->block_operating = 0;
			}
			ret = 0;
		//}

		mutex_unlock(&wdma_data->io_mutex);
		break;

	case TC_WDRV_COUNT_START:
	{
		uint32_t Wmix_Height = 0, Wmix_Width = 0;
		struct vioc_wdma_frame_info cmd_data;
		int itemp;
		mutex_lock(&wdma_data->io_mutex);

		if ((bool)wdma_data->block_operating) {
			wdma_data->block_waiting = 1;
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
			ret = wait_event_interruptible_timeout(
				wdma_data->cmd_wq,
				wdma_data->block_operating == 0U,
				wdma_msecs_to_jiffies(200));
			if (ret <= 0) {
				wdma_data->block_operating = 0;
				(void)pr_info("[INF][WDMA] [%d]: wdma 0 timed_out block_operation:%d!! cmd_count:%d\n",
					ret, wdma_data->block_waiting,
					wdma_data->cmd_count);
			}
			wdma_data->block_waiting = 0;
			ret = 0;
		}
		/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
		/* coverity[cert_dcl36_c_violation : FALSE] */
		if ((bool)copy_from_user(&cmd_data, (const void __user *)arg,
			sizeof(struct vioc_wdma_frame_info))) {
			mutex_unlock(&wdma_data->io_mutex);
			ret = -EFAULT;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto ioctl_end;
			//break;
		}

		if (!(bool)cmd_data.frame_x || !(bool)cmd_data.frame_y) {
			(void)pr_info("[INF][WDMA] TC_WDRV_COUNT_START size error : size : %d x %d\n",
				cmd_data.frame_x, cmd_data.frame_y);
			mutex_unlock(&wdma_data->io_mutex);
			ret = -EFAULT;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto ioctl_end;
			//break;
		}
		itemp = wdma_queue_list_init(&wdma_data->frame_list, &cmd_data);
		if(itemp >= 0) {
			/* KCS */
			cmd_data.buffer_num = (unsigned int)itemp;
		} else {
			(void)pr_err("[WDMA][%d] buffer_num is negative value\n", __LINE__);
			cmd_data.buffer_num = 0;
		}
		wdma_data->wdma_contiuous = 1;

		if (wdma_data->sc.reg != NULL) {
			/* KCS */
			(void)tc_wdrv_scaler_set(wdma_data, cmd_data.frame_x, cmd_data.frame_y);
		} else {
			VIOC_WMIX_GetSize(wdma_data->wmix.reg, &Wmix_Width, &Wmix_Height);

			if ((cmd_data.frame_x != Wmix_Width) || (cmd_data.frame_y != Wmix_Height)) {
				(void)pr_err("ERR][WDMA]Frame size is same as %d x %d <-> %d x %d\n",
							Wmix_Width,
							Wmix_Height,
							cmd_data.frame_x,
							cmd_data.frame_y);
				mutex_unlock(&wdma_data->io_mutex);

				ret = -EFAULT;

				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto ioctl_end;
			}
		}

		wdma_data->vioc_intr->bits = (1U << (unsigned int)VIOC_WDMA_INTR_UPD);
		(void)vioc_intr_clear(wdma_data->vioc_intr->id,
			wdma_data->vioc_intr->bits);
		if (wdma_data->irq <= (UINT_MAX/2U)) {
			(void)vioc_intr_enable((int)wdma_data->irq, wdma_data->vioc_intr->id,
				wdma_data->vioc_intr->bits);
		}
		(void)tc_wdrv_wdma_path_set(wdma_data, &cmd_data,
		(struct wdma_buffer_list *)&wdma_data->frame_list.wbuf_list[0]);
		/* coverity[ert_int36_c_violation : FALSE] */
		if ((bool)copy_to_user((struct vioc_wdma_frame_info *)arg,
			&cmd_data, sizeof(struct vioc_wdma_frame_info))) {
			mutex_unlock(&wdma_data->io_mutex);
			ret = -EFAULT;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto ioctl_end;
		}

		mutex_unlock(&wdma_data->io_mutex);
		break;
	}

	case TC_WDRV_COUNT_GET_DATA:
	{
		struct vioc_wdma_get_buffer wbuffer;
		unsigned int before_id;
		int itemp;

		(void)memset(&wbuffer, 0, sizeof(wbuffer));

		mutex_lock(&wdma_data->io_mutex);
		spin_lock_irq(&(wdma_data->cmd_lock));
		itemp = wdma_find_index_of_status(&wdma_data->frame_list, UPLOAD_S);
		if (itemp >= 0) {
			/* KCS */
			before_id = (unsigned int)itemp;
		} else {
			before_id = 0;
			(void)pr_err("[WDMA][%d] before_id is negative value\n", __LINE__);
		}
		wbuffer.index = wdma_find_index_of_status(
			&wdma_data->frame_list, WRITED_S);

		if (wbuffer.index >= 0) {
			wbuffer.buff_Yaddr =
		(unsigned int)wdma_data->frame_list.wbuf_list[wbuffer.index].base_Yaddr;
				wbuffer.buff_Uaddr =
		(unsigned int)wdma_data->frame_list.wbuf_list[wbuffer.index].base_Uaddr;
				wbuffer.buff_Vaddr =
		(unsigned int)wdma_data->frame_list.wbuf_list[wbuffer.index].base_Vaddr;
				wbuffer.frame_fmt =
					wdma_data->frame_list.data->frame_fmt;
				wbuffer.frame_x = wdma_data->frame_list.data->frame_x;
				wbuffer.frame_y = wdma_data->frame_list.data->frame_y;

		#ifdef WDMA_IMAGE_DEBUG
		{
			unsigned int checking_loop;

			for (checking_loop = 0;
			checking_loop <
			CHECKING_AREA(wdma_data->frame_list.data->frame_x,
				wdma_data->frame_list.data->frame_y);
				checking_loop++) {

				if (wdma_data->frame_list.wbuf_list[
					wbuffer.index].vbase_Yaddr[
					CHECKING_START_POS(
					wdma_data->frame_list.data->frame_x,
					wdma_data->frame_list.data->frame_y)
					+ checking_loop] != CHECKING_NUM) {

					break;
				}
			}

			if (checking_loop >= CHECKING_AREA(
				wdma_data->frame_list.data->frame_x,
				wdma_data->frame_list.data->frame_y)) {
				(void)pr_err("[ERR][WDMA] get buffer image is same with CHECKING_NUM checking_area : %d\n",
				CHECKING_AREA(
					wdma_data->frame_list.data->frame_x,
					wdma_data->frame_list.data->frame_y));
			}
		}
		#endif

			//(void)pr_info(
				//"%s index:%d Y:0x%08x U:0x%08x V:0x%08x fmt:%d X:%d Y:%d\n",
				//	__func__,
				//	wbuffer.buff_Yaddr, wbuffer.buff_Uaddr,
				//	wbuffer.buff_Vaddr, wbuffer.frame_fmt,
				//	wbuffer.frame_x, wbuffer.frame_y);
			(void)wdma_set_index_of_status(
				&wdma_data->frame_list,
				(unsigned int)wbuffer.index, UPLOAD_S);
		}

		//if (before_id >= 0U) {
			(void)wdma_set_index_of_status(
				&wdma_data->frame_list,
				before_id, PREPARED_S);
		//}

		spin_unlock_irq(&(wdma_data->cmd_lock));
		/* coverity[ert_int36_c_violation : FALSE] */
		if ((bool)copy_to_user(
			/* coverity[ert_int36_c_violation : FALSE] */
			(struct vioc_wdma_get_buffer *)arg,
			(struct vioc_wdma_get_buffer *)&wbuffer,
			sizeof(struct vioc_wdma_get_buffer))) {
			mutex_unlock(&wdma_data->io_mutex);
			ret = -EFAULT;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto ioctl_end;
		}
		mutex_unlock(&wdma_data->io_mutex);
	}
		break;

	case TC_WDRV_GET_CUR_DATA:
	{
		struct vioc_wdma_get_buffer wbuffer;

		(void)memset(&wbuffer, 0, sizeof(wbuffer));

		mutex_lock(&wdma_data->io_mutex);

		wdma_data->block_operating = 1;
		wdma_data->block_waiting = 1;
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
		ret = wait_event_interruptible_timeout(
			wdma_data->cmd_wq,
			wdma_data->block_operating == 0U,
			wdma_msecs_to_jiffies(50));

		wbuffer.index =
			wdma_find_index_of_status(&wdma_data->frame_list,
			WRITED_S);

		if (ret <= 0) {
			wdma_data->block_operating = 0;
			(void)pr_info("[INF][WDMA] ret: %d : wdma 0 timed_out block_operation:%d!! cmd_count:%d\n",
				ret, wdma_data->block_waiting,
				wdma_data->cmd_count);
		}
		wdma_data->block_waiting = 0;

		if (wbuffer.index >= 0) {
			wbuffer.buff_Yaddr = (unsigned int)
		wdma_data->frame_list.wbuf_list[wbuffer.index].base_Yaddr;
			wbuffer.buff_Uaddr = (unsigned int)
		wdma_data->frame_list.wbuf_list[wbuffer.index].base_Uaddr;
			wbuffer.buff_Vaddr = (unsigned int)
		wdma_data->frame_list.wbuf_list[wbuffer.index].base_Vaddr;
			wbuffer.frame_fmt =
				wdma_data->frame_list.data->frame_fmt;
			wbuffer.frame_x =
				wdma_data->frame_list.data->frame_x;
			wbuffer.frame_y =
				wdma_data->frame_list.data->frame_y;

			dprintk(
				"%s index:%d, Y:0x%08x U:0x%08x V:0x%08x, fmt:%d X:%d Y:%d\n",
				__func__,
				wbuffer.index,
				wbuffer.buff_Yaddr,
				wbuffer.buff_Uaddr,
				wbuffer.buff_Vaddr,
				wbuffer.frame_fmt,
				wbuffer.frame_x,
				wbuffer.frame_y);
		}

		/* coverity[ert_int36_c_violation : FALSE] */
		if ((bool)copy_to_user(
			(struct vioc_wdma_get_buffer *)arg,
			(struct vioc_wdma_get_buffer *)&wbuffer,
			sizeof(struct vioc_wdma_get_buffer))) {
			mutex_unlock(&wdma_data->io_mutex);
			ret = -EFAULT;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto ioctl_end;
		}
		mutex_unlock(&wdma_data->io_mutex);
		break;
	}

	case TC_WDRV_COUNT_END:
		mutex_lock(&wdma_data->io_mutex);

		wdma_data->wdma_contiuous = 0;
		VIOC_WDMA_SetImageDisable(wdma_data->wdma.reg);

		wdma_data->block_operating = 1;
		wdma_data->block_waiting = 1;
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
		ret = wait_event_interruptible_timeout(
			wdma_data->cmd_wq,
			wdma_data->block_operating == 0U,
			wdma_msecs_to_jiffies(30));

		if (ret <= 0)	{
			(void)pr_info("[INF][WDMA] [%d]  block_operation:%d!! cmd_count:%d\n",
				ret, wdma_data->block_waiting,
				wdma_data->cmd_count);
		}
		wdma_data->block_waiting = 0;
		wdma_data->block_operating = 0;

		(void)vioc_intr_clear(wdma_data->vioc_intr->id,
			wdma_data->vioc_intr->bits);
		if (wdma_data->irq <= (UINT_MAX/2U)) {
			/* KCS */
			(void)vioc_intr_disable((int)wdma_data->irq,
				wdma_data->vioc_intr->id,
				wdma_data->vioc_intr->bits);
		}

		ret = (int)wdma_queue_list_exit(&wdma_data->frame_list);
		mutex_unlock(&wdma_data->io_mutex);
		break;

	default:
		(void)pr_err("[ERR][WDMA] not supported WMIXER IOCTL(0x%x).\n", cmd);
		ret = -EINVAL;
		break;
	}
ioctl_end:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
/* coverity[misra_c_2012_rule_5_7_violation : FALSE] */
int tccxxx_wdma_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_wdma_dev *pwdma_data = dev_get_drvdata(misc->parent);
	(void)inode;
	dprintk("wdma_release IN:  %d'th, block(%d/%d), cmd(%d), irq(%d).\n",
		pwdma_data->dev_opened, pwdma_data->block_operating,
		pwdma_data->block_waiting, pwdma_data->cmd_count,
		pwdma_data->irq_reged);

	if (pwdma_data->dev_opened > 0U) {
		/* KCS */
		pwdma_data->dev_opened--;
	}

	if (pwdma_data->dev_opened == 0U) {
		if ((bool)pwdma_data->block_operating) {
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			ret = wait_event_interruptible_timeout(
				pwdma_data->cmd_wq,
				pwdma_data->block_operating == 0U,
				wdma_msecs_to_jiffies(200));
		}

		if (ret <= 0) {
			(void)pr_info("[INF][WDMA] [%d]: wdma timed_out block_operation: %d, block_waiting:%d  cmd_count: %d.\n",
				ret, (pwdma_data->block_operating),
				pwdma_data->block_waiting,
				pwdma_data->cmd_count);
		}

		if ((bool)pwdma_data->irq_reged) {
			(void)free_irq(pwdma_data->irq, pwdma_data);
			pwdma_data->irq_reged = 0;
		}
		pwdma_data->block_waiting = 0;
		pwdma_data->block_operating = pwdma_data->block_waiting;
		pwdma_data->cmd_count = 0;
		pwdma_data->poll_count = pwdma_data->cmd_count;
	}

	clk_disable_unprepare(pwdma_data->wdma_clk);

	dprintk("wdma_release OUT:  %d'th.\n", pwdma_data->dev_opened);
	return 0;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
/* coverity[misra_c_2012_rule_5_7_violation : FALSE] */
int tccxxx_wdma_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_wdma_dev *pwdma_data = dev_get_drvdata(misc->parent);
	(void)inode;
	dprintk("wdma_open IN:  %d'th, block(%d/%d), cmd(%d), irq(%d).\n",
		pwdma_data->dev_opened,
		pwdma_data->block_operating, pwdma_data->block_waiting,
		pwdma_data->cmd_count, pwdma_data->irq_reged);

	(void)clk_prepare_enable(pwdma_data->wdma_clk);

	if (!(bool)pwdma_data->irq_reged) {
		dprintk("wdma irq id:0x%x bits :0x%x\n",
			pwdma_data->vioc_intr->id,
			pwdma_data->vioc_intr->bits);
		(void)vioc_intr_clear(pwdma_data->vioc_intr->id,
			pwdma_data->vioc_intr->bits);
		if (pwdma_data->irq <= (UINT_MAX/2U)) {
			/* KCS */
			(void)vioc_intr_enable((int)pwdma_data->irq, pwdma_data->vioc_intr->id,
				pwdma_data->vioc_intr->bits);
		}

		if (request_irq(pwdma_data->irq, tccxxx_wdma_handler,
			IRQF_SHARED, "wdma", pwdma_data) < 0) {
			(void)pr_err("[ERR][WDMA] failed to aquire irq\n");
			ret = -EFAULT;
		}

		if ((bool)ret) {
			clk_disable_unprepare(pwdma_data->wdma_clk);
			(void)pr_err("[ERR][WDMA] failed to aquire wdma request_irq.\n");
			ret = -EFAULT;
		}

		pwdma_data->irq_reged = 1;
	}

	pwdma_data->dev_opened++;

	dprintk("wdma_open OUT:  %d'th.\n", pwdma_data->dev_opened);
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tcc_wdma_parse_dt(struct platform_device *pdev,
	struct tcc_wdma_dev *pwdma_data)
{
	int ret = 0;
	unsigned int index;
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_node *vioc_node, *disp_node;
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_node *np;

	vioc_node = of_parse_phandle(pdev->dev.of_node, "wdma-fbdisplay", 0);

	if (vioc_node != NULL) {
		if ((bool)of_property_read_u32(vioc_node, "telechips,fbdisplay_num",
			&pwdma_data->fb_dd_num)) {
			(void)pr_err(
		"[ERR][WDMA] could not find  telechips,fbdisplay_nubmer\n");
			ret = -ENODEV;
		}

		if ((bool)pwdma_data->fb_dd_num) {
			/* KCS */
			np = of_find_node_by_name(vioc_node, "fbdisplay1");
		}
		else {
			/* KCS */
			np = of_find_node_by_name(vioc_node, "fbdisplay0");
		}

		if (np == NULL) {
			(void)pr_err("[ERR][WDMA] %s could not fine fb node\n",
				__func__);
			ret = -ENODEV;
		}

		if (ret >= 0) {
			disp_node = of_parse_phandle(np, "telechips,disp", 0);
			(void)of_property_read_u32_index(np, "telechips,disp", 1, &index);

			if (disp_node == NULL) {
				(void)pr_err("[ERR][WDMA] could not find disp node\n");
				ret = -ENODEV;
			} else {
				pwdma_data->disp_info.virt_addr
					= VIOC_DISP_GetAddress(index);
			}
		}
	}

	return ret;
}

/*
 * wdma screen capture
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int wdma_screen_capture(char *Capture)
{
	int ret = 0;
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct miscdevice *misc;
	struct tcc_wdma_dev *wdma_data;
	struct file *filp;

	struct VIOC_WDMA_IMAGE_INFO_Type ImageCfg;
	unsigned int base_addr = 0, Wmix_Height = 0, Wmix_Width = 0;
	unsigned int DDevice = 0;
	unsigned int dd_rgb = 0;
	unsigned int addr_Y, addr_U, addr_V;
	struct DisplayBlock_Info DDinfo;
	unsigned int image_size;

	struct file *filp_mem;
	char pName[60] = {0,};
	mm_segment_t oldfs;
	dma_addr_t handle;
	void *image_virt_addr;

	// Open wdma device
	filp = filp_open("/dev/wdma_drv@0", O_RDWR, 0600);
	if (IS_ERR(filp)) {
		(void)pr_err("[ERR][WDMA] %s can't open wdma device\n", __func__);
		ret = -1;
	}

	if (ret < 0) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto return_ret;
	}
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	misc = (struct miscdevice *)filp->private_data;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	wdma_data = dev_get_drvdata(misc->parent);

	// Set Image Info
	(void)memset((char *)&ImageCfg, 0x00, sizeof(ImageCfg));
	ImageCfg.TargetWidth = 1920; // <- LCD width
	ImageCfg.TargetHeight = 720; // <- LCD height
	ImageCfg.ImgFormat = 0xC;  // Do not change (0x0C=ARGB888)
	image_size = ImageCfg.TargetWidth * ImageCfg.TargetHeight * 4U;

	// Set Memory
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	image_virt_addr = dma_alloc_coherent(misc->parent, image_size, &handle, GFP_KERNEL);
	if (image_virt_addr == NULL) {
		//(void)pr_err("[ERR][WDMA] %s can't alloc memory\n", __func__);
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto return_ret;
	}
	ImageCfg.BaseAddress = handle; // Use a physical address

	mutex_lock(&wdma_data->io_mutex);

	if ((bool)wdma_data->block_operating) {
		wdma_data->block_waiting = 1;
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
		ret = wait_event_interruptible_timeout(
			wdma_data->cmd_wq,
			wdma_data->block_operating == 0U,
			wdma_msecs_to_jiffies(200));
		if (ret <= 0) {
			wdma_data->block_operating = 0;
			(void)pr_info("[INF][WDMA] ret: %d : wdma 0 timed_out block_operation:%d!! cmd_count:%d\n",
				ret, wdma_data->block_waiting,
				wdma_data->cmd_count);
		}
		ret = 0;
	}

	//if (ret >= 0) {
		if (wdma_data->block_operating >= 1U) {
			(void)pr_info("[INF][WDMA] wdma driver :: block_operating(%d) - block_waiting(%d) - cmd_count(%d) - poll_count(%d)!!!\n",
				wdma_data->block_operating,
				wdma_data->block_waiting,
				wdma_data->cmd_count,
				wdma_data->poll_count);
		}

		wdma_data->block_waiting = 0;
		wdma_data->block_operating = 1;

		VIOC_WMIX_GetSize(wdma_data->wmix.reg, &Wmix_Width, &Wmix_Height);

		DDevice = VIOC_DISP_Get_TurnOnOff(wdma_data->disp_info.virt_addr);
		if ((Wmix_Width == 0U) || (Wmix_Height == 0U) || (DDevice == 0U)) {
			wdma_data->block_operating = 0;

			(void)pr_err("[ERR][WDMA][%s] W:%d H:%d DD-Power:%d\n", __func__, Wmix_Width, Wmix_Height, DDevice);

			ret = -1;
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			goto return_ret;
		}

		ImageCfg.Interlaced = 0;
		ImageCfg.ContinuousMode = 0;
		ImageCfg.SyncMode = 0;
		ImageCfg.ImgSizeWidth = Wmix_Width;
		ImageCfg.ImgSizeHeight = Wmix_Height;

		base_addr = (unsigned int)ImageCfg.BaseAddress;

		tcc_get_addr_yuv(ImageCfg.ImgFormat, base_addr,
			ImageCfg.TargetWidth, ImageCfg.TargetHeight, 0, 0,
			&addr_Y, &addr_U, &addr_V);

		if (ImageCfg.ImgFormat == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP
			|| ImageCfg.ImgFormat == (unsigned int)TCC_LCDC_IMG_FMT_RGB888) {
			addr_U = GET_ADDR_YUV42X_spU(base_addr, ImageCfg.TargetWidth,
				ImageCfg.TargetHeight);

			if (ImageCfg.ImgFormat == TCC_LCDC_IMG_FMT_YUV420SP) {
				addr_V = GET_ADDR_YUV420_spV(addr_U,
					ImageCfg.TargetWidth, ImageCfg.TargetHeight);
			} else {
				addr_V = GET_ADDR_YUV422_spV(addr_U,
					ImageCfg.TargetWidth, ImageCfg.TargetHeight);
			}
		}

		ImageCfg.BaseAddress = addr_Y;
		ImageCfg.BaseAddress1 = addr_U;
		ImageCfg.BaseAddress2 = addr_V;

		dprintk("src  w:%d h:%d base:0x%08x\n", ImageCfg.ImgSizeWidth,
			ImageCfg.ImgSizeHeight, ImageCfg.BaseAddress);
		dprintk("dest w:%d h:%d fb_dd_num:%d\n", ImageCfg.TargetWidth,
			ImageCfg.TargetHeight, wdma_data->fb_dd_num);
		dprintk("wmixer size %d %d :base1:0x%08x base2:0x%08x\n", Wmix_Width,
			Wmix_Height, ImageCfg.BaseAddress1, ImageCfg.BaseAddress2);
		dprintk("ImgFormat:%d  set effect : hue:%d, bright:%d contrast:%d\n",
			ImageCfg.ImgFormat, ImageCfg.Hue, ImageCfg.Bright,
			ImageCfg.Contrast);

		if (wdma_data->sc.reg != NULL) {
			if ((Wmix_Width == ImageCfg.TargetWidth) && (Wmix_Height == ImageCfg.TargetHeight)) {
				/* For KCS */
				VIOC_SC_SetBypass(wdma_data->sc.reg, 1);
			}
			else {
				/* For KCS */
				VIOC_SC_SetBypass(wdma_data->sc.reg, 0);
			}

			VIOC_SC_SetDstSize(wdma_data->sc.reg, ImageCfg.TargetWidth, ImageCfg.TargetHeight);
			VIOC_SC_SetOutSize(wdma_data->sc.reg, ImageCfg.TargetWidth, ImageCfg.TargetHeight);
			VIOC_SC_SetOutPosition(wdma_data->sc.reg, 0, 0);
			(void)VIOC_CONFIG_PlugIn(wdma_data->sc.id, wdma_data->wdma.id);
			VIOC_SC_SetUpdate(wdma_data->sc.reg);
		} else {
			if ((Wmix_Width != ImageCfg.TargetWidth) || (Wmix_Height != ImageCfg.TargetHeight)) {
				(void)pr_err("ERR][WDMA]Frame size error as %d x %d <-> %d x %d\n",
							Wmix_Width,
							Wmix_Height,
							ImageCfg.TargetWidth,
							ImageCfg.TargetHeight);
				ret = -EFAULT;

				/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
				goto return_ret;
			}
		}

		spin_lock_irq(&(wdma_data->cmd_lock));

		if ((bool)wdma_data->fb_dd_num) {
			(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA04, 1 /* Mixing */);
			(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA07, 1 /* Mixing */);
		} else {
			(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA00, 1 /* Mixing */);
			(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA03, 1 /* Mixing */);
		}

		// check display device format
		VIOC_DISP_GetDisplayBlock_Info(wdma_data->disp_info.virt_addr,
			&DDinfo);
		dd_rgb = VIOC_DISP_FMT_isRGB(DDinfo.pCtrlParam.pxdw);

		if (ImageCfg.ImgFormat <= (unsigned int)TCC_LCDC_IMG_FMT_ARGB6666_3) {
			if ((bool)dd_rgb) {
				VIOC_WDMA_SetImageY2RMode(wdma_data->wdma.reg, 0x2);
				VIOC_WDMA_SetImageY2REnable(wdma_data->wdma.reg, 0);
				VIOC_WDMA_SetImageR2YEnable(wdma_data->wdma.reg, 0);
			} else {
				VIOC_WDMA_SetImageY2RMode(wdma_data->wdma.reg, 0x2);
				VIOC_WDMA_SetImageY2REnable(wdma_data->wdma.reg, 1);
				VIOC_WDMA_SetImageR2YEnable(wdma_data->wdma.reg, 0);
			}
		} else {
			if ((bool)dd_rgb) {
				VIOC_WDMA_SetImageR2YMode(wdma_data->wdma.reg, 0x2);
				VIOC_WDMA_SetImageR2YEnable(wdma_data->wdma.reg, 1);
				VIOC_WDMA_SetImageY2REnable(wdma_data->wdma.reg, 0);
			} else {
				VIOC_WDMA_SetImageR2YMode(wdma_data->wdma.reg, 0x2);
				VIOC_WDMA_SetImageR2YEnable(wdma_data->wdma.reg, 0);
				VIOC_WDMA_SetImageY2REnable(wdma_data->wdma.reg, 0);
			}
		}

		VIOC_WDMA_SetImageSize(wdma_data->wdma.reg, ImageCfg.TargetWidth,
			ImageCfg.TargetHeight);
		VIOC_WDMA_SetImageFormat(wdma_data->wdma.reg, ImageCfg.ImgFormat);
		VIOC_WDMA_SetImageOffset(wdma_data->wdma.reg, ImageCfg.ImgFormat,
			ImageCfg.TargetWidth);
		VIOC_WDMA_SetImageBase(wdma_data->wdma.reg, ImageCfg.BaseAddress,
			ImageCfg.BaseAddress1, ImageCfg.BaseAddress2);
		VIOC_WDMA_SetImageRGBSwapMode(wdma_data->wdma.reg, 0);
	#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
		VIOC_WDMA_SetImageEnhancer(wdma_data->wdma.reg, ImageCfg.Contrast,
			ImageCfg.Bright, ImageCfg.Hue);
	#endif
		VIOC_WDMA_SetImageEnable(wdma_data->wdma.reg, ImageCfg.ContinuousMode);

		spin_unlock_irq(&(wdma_data->cmd_lock));
		wdma_data->vioc_intr->bits = (1U << (unsigned int)VIOC_WDMA_INTR_EOFR);
		(void)vioc_intr_clear(wdma_data->vioc_intr->id, wdma_data->vioc_intr->bits);
		(void)vioc_intr_enable((int)wdma_data->irq, wdma_data->vioc_intr->id,
			wdma_data->vioc_intr->bits);
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[misra_c_2012_rule_16_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
		ret = wait_event_interruptible_timeout(wdma_data->poll_wq,
			wdma_data->block_operating == 0U, wdma_msecs_to_jiffies(100));
		if (ret <= 0) {
			wdma_data->block_operating = 0;
			(void)pr_warn("[WAR][WDMA] time out: %d, Line: %d.\n", ret, __LINE__);
		}

		if (wdma_data->sc.reg != NULL) {
			/* KCS */
			(void)VIOC_CONFIG_PlugOut(wdma_data->sc.id);
		}

		if (ret >= 0) {
			/* KCS */
			wdma_data->block_operating = 0;
		}
		ret = 0;
	//}

	mutex_unlock(&wdma_data->io_mutex);

	// Write data to userspace
	(void)strncpy(pName, Capture, strlen(Capture));

	/* coverity[misra_c_2012_rule_11_3_violation : FALSE] */
	/* coverity[cert_exp39_c_violation : FALSE] */
	oldfs = get_fs();

	/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
	set_fs(KERNEL_DS);

	filp_mem = filp_open(pName, O_CREAT | O_WRONLY, 0600);
	if (IS_ERR(filp_mem)) {
		(void)pr_err("[ERR][WDMA] %s can't open %s\n", __func__, pName);
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto return_ret;
	}

	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	 (void)vfs_write(filp_mem,  (char __user *)image_virt_addr, image_size, &filp_mem->f_pos);

	//vfs_fsync(filp_mem, NULL);
	(void)vfs_fsync(filp_mem, 0);
	set_fs(oldfs);
	dma_free_coherent(misc->parent, image_size, image_virt_addr, handle);
	(void)filp_close(filp, NULL);
	(void)filp_close(filp_mem, NULL);

return_ret:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t screen_capture_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	(void)attr;
	(void)dev;
	/* coverity[misra_c_2012_rule_21_6_violation : FALSE] */
	return sprintf(buf, "%s\n", screencapture);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t screen_capture_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	(void)attr;
	(void)dev;
	/* coverity[misra_c_2012_rule_21_6_violation : FALSE] */
	ret = sscanf(buf, "%s", screencapture);

	ret = wdma_screen_capture(screencapture);
	if (ret >= 0) {
		/* Prevent KCS */
		(void)pr_info("[INF][WDMA] %s File saved : %s\n", __func__, screencapture);
	}
	else {
		/* Prevent KCS */
		(void)pr_info("[INF][WDMA] %s Capture failure\n", __func__);
	}

	return (ssize_t)count;
}
static DEVICE_ATTR_RW(screen_capture);

void wdma_attr_create(struct platform_device *pdev)
{
	(void)device_create_file(&pdev->dev, &dev_attr_screen_capture);
}

void wdma_attr_remove(struct platform_device *pdev)
{
	(void)device_remove_file(&pdev->dev, &dev_attr_screen_capture);
}

static const struct file_operations tcc_wdma_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tccxxx_wdma_ioctl,
	.mmap = tccxxx_wdma_mmap,
	.open = tccxxx_wdma_open,
	.release = tccxxx_wdma_release,
	.poll = tccxxx_wdma_poll,
};

static int  tcc_wdma_probe(struct platform_device *pdev)
{
	struct tcc_wdma_dev *wdma_data;
	struct device_node *dev_np;
	int ret = -ENOMEM;

	(void)pr_info("TCC WDMA Driver Initializing\n");
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	wdma_data = kzalloc(sizeof(struct tcc_wdma_dev), GFP_KERNEL);
	if (wdma_data == NULL) {
		(void)pr_err("[ERR][WDMA][%d] can not alloc wdma_data\n", __LINE__);
		ret = -ENOMEM;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto normal_return;
	}
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	wdma_data->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (wdma_data->misc == NULL) {
		(void)pr_err("[ERR][WDMA][%d] can not alloc wdma_data->misc\n", __LINE__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_misc_alloc;
	}
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	wdma_data->vioc_intr = kzalloc(sizeof(struct vioc_intr_type),
		GFP_KERNEL);
	if (wdma_data->vioc_intr == NULL) {
		(void)pr_err("[ERR][WDMA][%d] can not alloc wdma_data->vioc_intr\n", 
			__LINE__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_misc_regist;
	}

	(void)tcc_wdma_parse_dt(pdev, wdma_data);

	wdma_data->misc->minor = MISC_DYNAMIC_MINOR;
	wdma_data->misc->fops = &tcc_wdma_fops;
	wdma_data->misc->name = pdev->name;
	wdma_data->misc->parent = &pdev->dev;
	ret = misc_register(wdma_data->misc);
	if ((bool)ret) {
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto err_misc_regist;
	}

	dev_np = of_parse_phandle(pdev->dev.of_node, "scalers", 0);
	if (dev_np != NULL) {
		(void)of_property_read_u32_index(pdev->dev.of_node, "scalers", 1,
			&wdma_data->sc.id);
		wdma_data->sc.reg = VIOC_SC_GetAddress(wdma_data->sc.id);
	} else {
		(void)pr_info(
		"[INF][WDMA] could not find wdma_data node of %s driver.\n",
			wdma_data->misc->name);
		wdma_data->sc.reg = NULL;
	}

	dev_np = of_parse_phandle(pdev->dev.of_node, "wmixs", 0);
	if (dev_np != NULL) {
		(void)of_property_read_u32_index(pdev->dev.of_node, "wmixs", 1,
			&wdma_data->wmix.id);
		wdma_data->wmix.reg = VIOC_WMIX_GetAddress(wdma_data->wmix.id);
	} else {
		(void)pr_info("[INF][WDMA] could not find wmix node of %s driver.\n",
			wdma_data->misc->name);
		wdma_data->wmix.reg = NULL;
	}

	dev_np = of_parse_phandle(pdev->dev.of_node, "wdmas", 0);
	if (dev_np != NULL) {
		unsigned int wdma_id;

		if (of_property_read_u32_index(pdev->dev.of_node, "wdmas", 1,
			&wdma_data->wdma.id) != 0) {
			goto err_misc_regist;
		}
		if (wdma_data->wdma.id > (UINT_MAX >> 1)) {
			goto err_misc_regist;
		}
		wdma_id = get_vioc_index(wdma_data->wdma.id);
		wdma_data->wdma.reg = VIOC_WDMA_GetAddress(wdma_data->wdma.id);
		wdma_data->irq = irq_of_parse_and_map(dev_np, (int)wdma_id);
		wdma_data->vioc_intr->id = VIOC_INTR_WD0 + (int)wdma_id;
		wdma_data->vioc_intr->bits = VIOC_WDMA_IREQ_EOFR_MASK;
	} else {
		(void)pr_info("[INF][WDMA] could not find wdma node of %s driver.\n",
			wdma_data->misc->name);
		wdma_data->wdma.reg = NULL;
	}
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_17_7_violation : FALSE] */
	spin_lock_init(&(wdma_data->poll_lock));
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_17_7_violation : FALSE] */
	spin_lock_init(&(wdma_data->cmd_lock));
	/* coverity[cert_dcl37_c_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	mutex_init(&(wdma_data->io_mutex));

	init_waitqueue_head(&(wdma_data->poll_wq));
	init_waitqueue_head(&(wdma_data->cmd_wq));

	platform_set_drvdata(pdev, (void *)wdma_data);
	wdma_attr_create(pdev);
	ret = 0;
	/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
	goto normal_return;

err_misc_regist:
	kfree(wdma_data->misc);
err_misc_alloc:
	kfree(wdma_data);
normal_return:
	return ret;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tcc_wdma_remove(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct tcc_wdma_dev *pwdma_data
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		= (struct tcc_wdma_dev *)platform_get_drvdata(pdev);

	misc_deregister(pwdma_data->misc);
	kfree(pwdma_data->misc);
	kfree(pwdma_data);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tcc_wdma_of_match[] = {
	{.compatible = "telechips,tcc_wdma"},
	{}
};
MODULE_DEVICE_TABLE(of, tcc_wdma_of_match);
#endif

static struct platform_driver tcc_wdma_driver = {
	.probe  = tcc_wdma_probe,
	.remove = tcc_wdma_remove,
	.driver = {
	.name   = "wdma",
	.owner  = THIS_MODULE,
#ifdef CONFIG_OF
	.of_match_table = of_match_ptr(tcc_wdma_of_match),
#endif
	},
};

static int __init tcc_wdma_init(void)
{
	(void)pr_info("[INF][WDMA] %s\n", __func__);
	return platform_driver_register(&tcc_wdma_driver);
}

static void __exit tcc_wdma_exit(void)
{
	platform_driver_unregister(&tcc_wdma_driver);
}
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
MODULE_AUTHOR("Telechips.");
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
MODULE_DESCRIPTION("TCC WDMA Driver");
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
MODULE_LICENSE("GPL");

/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
module_init(tcc_wdma_init);
/* coverity[cert_dcl37_c_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
module_exit(tcc_wdma_exit);
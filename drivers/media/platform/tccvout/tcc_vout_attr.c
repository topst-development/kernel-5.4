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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include "tcc_vout.h"
#include "tcc_vout_core.h"
#include "tcc_vout_dbg.h"

/**
 * Show the vioc path of the v4l2 video output.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t vioc_path_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;
	const struct tcc_vout_vioc *pvioc = vout->vioc;
	char buf_sc[32];
	int ret;

	struct device_attribute *tmpattr;

	tmpattr = attr;
	attr = tmpattr;

	(void)attr;

	ret = scnprintf(buf_sc, 128, " - SC%d", pvioc->sc.id);

	return scnprintf(buf, 128, "RDMA%u - WMIX%u - DISP%u\n",
					pvioc->rdma.id,
					pvioc->wmix.id, pvioc->disp.id);
}
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
DEVICE_ATTR_RO(vioc_path);

/**
 * Show & Set the rdma of vioc path.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t vioc_rdma_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;
	const struct tcc_vout_vioc *vioc = vout->vioc;
	struct device_attribute *tmpattr;

	(void)attr;

	tmpattr = attr;
	attr = tmpattr;

	dprintk("rdma%d\n", vioc->rdma.id);
	return scnprintf(buf, 128, "%d\n", vioc->rdma.id);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t vioc_rdma_store(struct device *dev,
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	int ret;
	unsigned long val;

	(void)attr;

	if (vout->status != TCC_VOUT_IDLE) {
		(void)pr_err("[ERR][VOUT] status is not idle\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, &val);
	if ((bool)ret) {
		(void)pr_err("[ERR][VOUT] %s\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}
	if (val > 7UL) {
		(void)pr_err("[ERR][VOUT] invalid rdma\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}
	dprintk("rdma%d -> rdma%ld\n", vioc->rdma.id, val);

	vioc->rdma.id = u64_to_u32(val);
	(void)vout_set_vout_path(vout);
attr_exit:
	return u32_to_s32(u64_to_u32(count));
}
/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
DEVICE_ATTR_RW(vioc_rdma);

/**
 * Show & Set the scaler of vioc path.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t vioc_sc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;
	const struct tcc_vout_vioc *vioc = vout->vioc;
	struct device_attribute *tmpattr;

	(void)attr;

	tmpattr = attr;
	attr = tmpattr;

	dprintk("sc%d\n", vioc->sc.id);
	return scnprintf(buf, 128, "%d\n", vioc->sc.id);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t vioc_sc_store(struct device *dev,
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	struct device_attribute *tmpattr;
	int ret;
	unsigned long val;


	(void)attr;

	tmpattr = attr;
	attr = tmpattr;

	if (vout->status != TCC_VOUT_IDLE) {
		(void)pr_err("[ERR][VOUT] status is not idle\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, &val);
	if ((bool)ret) {
		(void)pr_err("[ERR][VOUT] %s\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}
	if (val > 3UL) {
		(void)pr_err("[ERR][VOUT] invalid sc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}
	dprintk("sc%d -> sc%ld\n", vioc->sc.id, val);

	vioc->sc.id = u64_to_u32(val);

attr_exit:
	return u32_to_s32(u64_to_u32(count));
}
/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
DEVICE_ATTR_RW(vioc_sc);

/**
 * Show & Set the ovp (overlay priority) of wmix.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t vioc_wmix_ovp_show(struct device *dev,
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;
	const struct tcc_vout_vioc *vioc = vout->vioc;

	(void)attr;

	dprintk("wmix%d ovp(%d)\n", vioc->wmix.id, vioc->wmix.ovp);
	return scnprintf(buf, 128, "%d\n", vioc->wmix.ovp);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t vioc_wmix_ovp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	struct device_attribute *tmpattr;
	unsigned long val;
	int ret;

	(void)attr;

	tmpattr = attr;
	attr = tmpattr;

	if (vout->status != TCC_VOUT_IDLE) {
		(void)pr_err("[ERR][VOUT] status is not idle\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, &val);
	if ((bool)ret) {
		(void)pr_err("[ERR][VOUT] %s\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}
	dprintk("wmix%d ovp(%d) -> ovp(%ld)\n",
		vioc->wmix.id, vioc->wmix.ovp, val);

	vioc->wmix.ovp = u64_to_u32(val);

attr_exit:
	return u32_to_s32(u64_to_u32(count));
}
/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
DEVICE_ATTR_RW(vioc_wmix_ovp);

/**
 * Show & Set the v4l2_memory.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t force_v4l2_memory_userptr_show(struct device *dev,
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;

	(void)attr;

	dprintk("force(%d) v4l2_memory(%d)\n",
		vout->force_userptr, vout->memory);
	return scnprintf(buf, 128, "%d\n", vout->force_userptr);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t force_v4l2_memory_userptr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_vout_device *vout = dev->platform_data;
	struct device_attribute *tmpattr;
	unsigned long val;
	int ret;

	(void)attr;

	tmpattr = attr;
	attr = tmpattr;

	if (vout->status != TCC_VOUT_IDLE) {
		(void)pr_err("[ERR][VOUT] status is not idle\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, &val);
	if ((bool)ret) {
		(void)pr_err("[ERR][VOUT] %s\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}
	vout->force_userptr = u64_to_u32(val);
	dprintk("set(%ld) -> force(%d)\n", val, vout->force_userptr);
attr_exit:
	return u32_to_s32(u64_to_u32(count));
}
/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
DEVICE_ATTR_RW(force_v4l2_memory_userptr);

/**
 * Show & Set the pmap name.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t vout_pmap_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;

	(void)attr;

	dprintk("vout_pmap(%s)\n", vout->pmap.name);
	return scnprintf(buf, 128, "%s\n", vout->pmap.name);
}
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t vout_pmap_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_vout_device *vout = dev->platform_data;
	struct device_attribute *tmpattr;

	(void)attr;

	tmpattr = attr;
	attr = tmpattr;

	if (vout->status != TCC_VOUT_IDLE) {
		(void)pr_err("[ERR][VOUT] status is not idle\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}

	(void)memset(vout->pmap.name, 0, sizeof(vout->pmap.name));
	if (count > 1UL) {
		(void)memcpy(vout->pmap.name, buf, count - 1UL);
	}
	dprintk("vout_pmap(%s)\n", vout->pmap.name);
attr_exit:
	return u32_to_s32(u64_to_u32(count));
}
/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
DEVICE_ATTR_RW(vout_pmap);

/*==============================================================================
* DE-INTERLACE
* ------------
* deinterlace_path_show
* deinterlace_type_show/store
* deinterlace_rdma_show/store
* deinterlace_pmap_show/store
* deinterlace_bufs_show/store
*==============================================================================
*/
/**
 * Show interlace information.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_path_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = (struct tcc_vout_device *)dev->platform_data;
	const struct tcc_vout_vioc *vioc = vout->vioc;
	struct device_attribute *tmpattr;
	int ret;

	(void)attr;

	tmpattr = attr;
	attr = tmpattr;

	switch (vout->deinterlace) {
	case VOUT_DEINTL_S:
		ret = scnprintf(buf, 128,
			"RDMA%d - DEINTL_S - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id, vioc->m2m_wmix.id,
			vioc->sc.id, vioc->m2m_wdma.id);
		break;
	case VOUT_DEINTL_VIQE_2D:
		ret = scnprintf(buf, 128,
			"RDMA%d - VIQE_2D - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id, vioc->m2m_wmix.id,
			vioc->sc.id, vioc->m2m_wdma.id);
		break;
	case VOUT_DEINTL_VIQE_3D:
		ret = scnprintf(buf, 128,
			"RDMA%d - VIQE_3D - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id, vioc->m2m_wmix.id,
			vioc->sc.id, vioc->m2m_wdma.id);
		break;
	case VOUT_DEINTL_VIQE_BYPASS:
		ret = scnprintf(buf, 128,
			"RDMA%d - VIQE_BYPASS - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id,  vioc->m2m_wmix.id,
			vioc->sc.id, vioc->m2m_wdma.id);
		break;
	case VOUT_DEINTL_NONE:
	default:
		ret = scnprintf(buf, 128, "RDMA%d - WMIX%d - SC%d - WDMA%d\n",
			vioc->m2m_rdma.id, vioc->m2m_wmix.id,
			vioc->sc.id, vioc->m2m_wdma.id);
		break;
	}

	return ret;
}
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
DEVICE_ATTR_RO(deinterlace_path);

/**
 * Select De-interlacer.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;
	struct device_attribute *tmpattr;

	(void)attr;

	tmpattr = attr;
	attr = tmpattr;

	return scnprintf(buf, 128, "%d\n", vout->deinterlace);
}
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_vout_device *vout = dev->platform_data;
	unsigned long val;
	unsigned int tmp;
	int ret;

	(void)attr;

	if (vout->status != TCC_VOUT_IDLE) {
		(void)pr_err("[ERR][VOUT] status is not idle\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}

	ret = kstrtoul(buf, 10, &val);
	tmp = u64_to_u32(val);
	if ((bool)ret) {
		(void)pr_err("[ERR][VOUT] %s\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}

	vout->deinterlace = (enum deintl_type)u32_to_s32(tmp);
	dprintk("set(%ld) -> deintl(%d)\n", val, vout->deinterlace);

	if ((bool)vout->deinterlace) {
		ret = vout_set_m2m_path(0, vout);
	}

attr_exit:
	return u32_to_s32(u64_to_u32(count));
}
/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
DEVICE_ATTR_RW(deinterlace);

/**
 * Select deinterlace's rdma.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_rdma_show(struct device *dev,
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;
	const struct tcc_vout_vioc *vioc = vout->vioc;

	(void)attr;

	return scnprintf(buf, 128, "%d\n", vioc->m2m_rdma.id);
}
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_rdma_store(struct device *dev,
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	int ret;
	unsigned long val;

	(void)attr;

	if (vout->status != TCC_VOUT_IDLE) {
		(void)pr_err("[ERR][VOUT] status is not idle\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, &val);
	if ((bool)ret) {
		(void)pr_err("[ERR][VOUT] %s\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}
	if (!((val == 16UL) || (val == 17UL))) {
		(void)pr_err("[ERR][VOUT] invalid rdma (use 16 or 17)\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}
	dprintk("set(%d) -> m2m_rdma(%ld)\n", vioc->m2m_rdma.id, val);

	vioc->m2m_rdma.id = u64_to_u32(val);
	ret = vout_set_m2m_path(0, vout);

attr_exit:
	return u32_to_s32(u64_to_u32(count));
}
/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
DEVICE_ATTR_RW(deinterlace_rdma);

/**
 * Show & Set the scaler of deintl_path.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_sc_show(struct device *dev,
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;
	const struct tcc_vout_vioc *vioc = vout->vioc;

	(void)attr;

	dprintk("sc%d\n", vioc->sc.id);
	return scnprintf(buf, 128, "%d\n", vioc->sc.id);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_sc_store(struct device *dev,
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	int ret;
	unsigned long val;

	(void)attr;

	if (vout->status != TCC_VOUT_IDLE) {
		(void)pr_err("[ERR][VOUT] status is not idle\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, &val);
	if ((bool)ret) {
		(void)pr_err("[ERR][VOUT] %s\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}
	if (val > 3UL) {
		(void)pr_err("[ERR][VOUT] invalid sc\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}
	dprintk("sc%d -> sc%ld\n", vioc->sc.id, val);

	vioc->sc.id = u64_to_u32(val);
attr_exit:
	return u32_to_s32(u64_to_u32(count));
}
/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
DEVICE_ATTR_RW(deinterlace_sc);

/**
 * Show & Set the deintl_pmap name.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_pmap_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_vout_device *vout = dev->platform_data;

	(void)attr;

	dprintk("deintl_pmap(%s)\n", vout->deintl_pmap.name);
	return scnprintf(buf, 128, "%s\n", vout->deintl_pmap.name);
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_pmap_store(struct device *dev,
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_vout_device *vout = dev->platform_data;

	(void)attr;

	if (vout->status != TCC_VOUT_IDLE) {
		(void)pr_err("[ERR][VOUT] status is not idle\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}

	(void)memset(vout->deintl_pmap.name, 0, sizeof(vout->deintl_pmap.name));
	if (count > 1UL) {
		(void)memcpy(vout->deintl_pmap.name, buf, count - 1UL);
	}
	dprintk("deintl_pmap(%s)\n", vout->deintl_pmap.name);

attr_exit:
	return u32_to_s32(u64_to_u32(count));
}
/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
DEVICE_ATTR_RW(deinterlace_pmap);

/**
 * Select De-interlacer buffers.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_bufs_show(struct device *dev,
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;

	(void)attr;

	return scnprintf(buf, 128, "%d\n", vout->deintl_nr_bufs);
}
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_bufs_store(struct device *dev,
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_vout_device *vout = dev->platform_data;
	int ret;
	unsigned long val;

	(void)attr;

	if (vout->status != TCC_VOUT_IDLE) {
		(void)pr_err("[ERR][VOUT] status is not idle\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, &val);
	if ((bool)ret) {
		(void)pr_err("[ERR][VOUT] %s\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}
	vout->deintl_nr_bufs = u64_to_u32(val);
	dprintk("set(%ld) -> deintl_nr_bufs(%d)\n", val, vout->deintl_nr_bufs);

attr_exit:
	return u32_to_s32(u64_to_u32(count));
}
/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
DEVICE_ATTR_RW(deinterlace_bufs);

/**
 * Select rdma first field.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_bfield_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;
	const struct tcc_vout_vioc *vioc = vout->vioc;

	(void)attr;

	return scnprintf(buf, 128, "%d\n", vioc->m2m_rdma.bf);
}
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_bfield_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned long val;
	int ret;

	(void)attr;

	if (vout->status != TCC_VOUT_IDLE) {
		(void)pr_err("[ERR][VOUT] status is not idle\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, &val);
	if ((bool)ret) {
		(void)pr_err("[ERR][VOUT] %s\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}
	if (val > 1UL) {
		(void)pr_err("[ERR][VOUT] invalid rdma's bfield. select 0(top-field first) or 1(bottom-field first)\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}
	dprintk("set(%d) -> m2m_rdma.bf(%ld)\n", vioc->m2m_rdma.bf, val);
	vioc->m2m_rdma.bf = u64_to_u32(val);

attr_exit:
	return u32_to_s32(u64_to_u32(count));
}
/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
DEVICE_ATTR_RW(deinterlace_bfield);

/**
 * Select De-interlacer.
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_force_show(struct device *dev,
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;

	(void)attr;

	return scnprintf(buf, 128, "%d\n", vout->deintl_force);
}
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t deinterlace_force_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_vout_device *vout = dev->platform_data;
	int ret;
	unsigned long val;

	(void)attr;

	if (vout->status != TCC_VOUT_IDLE) {
		(void)pr_err("[ERR][VOUT] status is not idle\n");
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, &val);
	if ((bool)ret) {
		(void)pr_err("[ERR][VOUT] %s\n", __func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto attr_exit;
	}
	vout->deintl_force = u32_to_s32(u64_to_u32(val));
	dprintk("set(%ld) -> deintl_force(%d)\n", val, vout->deintl_force);
attr_exit:
	return u32_to_s32(u64_to_u32(count));
}
/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
DEVICE_ATTR_RW(deinterlace_force);

/**
 * Select type of vout path.
 * 0 = m2m path
 * 1 = on-the-fly path
 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t otf_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct tcc_vout_device *vout = dev->platform_data;

	(void)attr;

	return scnprintf(buf, 128, "%d\n", vout->onthefly_mode);
}
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static ssize_t otf_mode_store(struct device *dev,
	/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
	struct device_attribute *attr, const char *buf, size_t count)
{
	(void)attr;
	(void)buf;
	(void)dev;

	#ifdef CONFIG_VOUT_USE_VSYNC_INT
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct tcc_vout_device *vout = dev->platform_data;
	int ret;
	unsigned long val;

	if (vout->status != TCC_VOUT_IDLE) {
		(void)pr_err("[ERR][VOUT] status is not idle\n");
		return count;
	}

	//val = simple_strtoul(buf, NULL, 10);
	ret = kstrtoul(buf, 10, &val);
	if (ret) {
		pr_err("[ERR][VOUT] %s\n", __func__);
		return count;
	}
	vout->onthefly_mode = !!val;
	dprintk("set(%d) -> onthefly_mode(%d)\n", val, vout->onthefly_mode);
	#endif
	return u32_to_s32(u64_to_u32(count));
}
/* coverity[misra_c_2012_rule_6_1_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_4_violation : FALSE] */
/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
DEVICE_ATTR_RW(otf_mode);
/**
 * Create the device files
 */
void tcc_vout_attr_create(struct platform_device *pdev)
{
	(void)device_create_file(&pdev->dev, &dev_attr_vioc_path);
	(void)device_create_file(&pdev->dev, &dev_attr_vioc_rdma);
	(void)device_create_file(&pdev->dev, &dev_attr_vioc_wmix_ovp);
	(void)device_create_file(&pdev->dev, &dev_attr_force_v4l2_memory_userptr);
	(void)device_create_file(&pdev->dev, &dev_attr_vout_pmap);
	/* deinterlace */
	(void)device_create_file(&pdev->dev, &dev_attr_deinterlace);
	(void)device_create_file(&pdev->dev, &dev_attr_deinterlace_path);
	(void)device_create_file(&pdev->dev, &dev_attr_deinterlace_rdma);
	(void)device_create_file(&pdev->dev, &dev_attr_deinterlace_pmap);
	(void)device_create_file(&pdev->dev, &dev_attr_deinterlace_bufs);
	(void)device_create_file(&pdev->dev, &dev_attr_deinterlace_bfield);
	(void)device_create_file(&pdev->dev, &dev_attr_deinterlace_force);
	(void)device_create_file(&pdev->dev, &dev_attr_deinterlace_sc);
	/* on-the-fly path */
	(void)device_create_file(&pdev->dev, &dev_attr_otf_mode);
}

/**
 * Remove the device files
 */
void tcc_vout_attr_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_vioc_path);
	device_remove_file(&pdev->dev, &dev_attr_vioc_rdma);
	device_remove_file(&pdev->dev, &dev_attr_vioc_wmix_ovp);
	device_remove_file(&pdev->dev, &dev_attr_force_v4l2_memory_userptr);
	device_remove_file(&pdev->dev, &dev_attr_vout_pmap);
	/* deinterlace */
	device_remove_file(&pdev->dev, &dev_attr_deinterlace);
	device_remove_file(&pdev->dev, &dev_attr_deinterlace_path);
	device_remove_file(&pdev->dev, &dev_attr_deinterlace_rdma);
	device_remove_file(&pdev->dev, &dev_attr_deinterlace_pmap);
	device_remove_file(&pdev->dev, &dev_attr_deinterlace_bufs);
	device_remove_file(&pdev->dev, &dev_attr_deinterlace_bfield);
	device_remove_file(&pdev->dev, &dev_attr_deinterlace_force);
	device_remove_file(&pdev->dev, &dev_attr_deinterlace_sc);
	/* on-the-fly path */
	device_remove_file(&pdev->dev, &dev_attr_otf_mode);
}

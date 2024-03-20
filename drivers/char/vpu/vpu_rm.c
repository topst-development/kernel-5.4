// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#ifdef CONFIG_PROC_FS
#include <linux/list_sort.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

#ifdef CONFIG_OPTEE
#include <linux/tee_drv.h>
#include <linux/time.h>
#endif

#include <video/tcc/tcc_vpu_wbuffer.h>

#include "vpu_comm.h"
#include "vpu_devices.h"
#include "vpu_rm.h"

#define VRM_FREED                (~((u64)0U))
#define VRM_DATA_NA              (~((u32)0U))

#ifdef CONFIG_PROC_FS
#define PROC_VRM_CMD_GET         (0x1001)
#define PROC_VRM_CMD_RELEASE     (0x1002)

const char *vname[MAX_VRM_VMEM] = {
	"video",                 // 00
	"video_rear",            // 01
	"video_ext",             // 02
	"video_ext_rear",        // 03
	"video_ext2",            // 04
	"enc_main",              // 05
	"enc_ext",               // 06
	"enc_ext2",              // 07
	"enc_ext3",              // 08
	"enc_ext4",              // 09
	"enc_ext5",              // 10
	"enc_ext6",              // 11
	"enc_ext7",              // 12
	"enc_ext8",              // 13
	"enc_ext9",              // 14
	"enc_ext10",             // 15
	"enc_ext11",             // 16
	"enc_ext12",             // 17
	"enc_ext13",             // 18
	"enc_ext14",             // 19
	"enc_ext15",             // 20
	"video_sw"               // 21
};

enum vnidx {
	video = 0,               // 00
	video_rear,              // 01
	video_ext,               // 02
	video_ext_rear,          // 03
	video_ext2,              // 04
	enc_main,                // 05
	enc_ext,                 // 06
	enc_ext2,                // 07
	enc_ext3,                // 08
	enc_ext4,                // 09
	enc_ext5,                // 10
	enc_ext6,                // 11
	enc_ext7,                // 12
	enc_ext8,                // 13
	enc_ext9,                // 14
	enc_ext10,               // 15
	enc_ext11,               // 16
	enc_ext12,               // 17
	enc_ext13,               // 18
	enc_ext14,               // 19
	enc_ext15,               // 20
	video_sw                 // 21
};

const char *vswmname[MAX_VRM_VSW] = {
	"D_USERDATA_0",          // 00
	"D_USERDATA_1",          // 01
	"D_USERDATA_2",          // 02
	"D_USERDATA_3",          // 03
	"D_USERDATA_4",          // 04
	"E_SEQ_H_0",             // 05
	"E_SEQ_H_1",             // 06
	"E_SEQ_H_2",             // 07
	"E_SEQ_H_3",             // 08
	"E_SEQ_H_4",             // 09
	"E_SEQ_H_5",             // 10
	"E_SEQ_H_6",             // 11
	"E_SEQ_H_7",             // 12
	"E_SEQ_H_8",             // 13
	"E_SEQ_H_9",             // 14
	"E_SEQ_H_10",            // 15
	"E_SEQ_H_11",            // 16
	"E_SEQ_H_12",            // 17
	"E_SEQ_H_13",            // 18
	"E_SEQ_H_14",            // 19
	"E_SEQ_H_15",            // 20
	"WAVE420L",              // 21
	"WAVE512",               // 22
	"WAVE410",               // 23
	"G2V2_VP9",              // 24
	"JPU",                   // 25
	"CODA960"                // 26
};

enum vswmidx {
	d_userdata_0 = 0,        // 00
	d_userdata_1,            // 01
	d_userdata_2,            // 02
	d_userdata_3,            // 03
	d_userdata_4,            // 04
	e_seq_h_0,               // 05
	e_seq_h_1,               // 06
	e_seq_h_2,               // 07
	e_seq_h_3,               // 08
	e_seq_h_4,               // 09
	e_seq_h_5,               // 10
	e_seq_h_6,               // 11
	e_seq_h_7,               // 12
	e_seq_h_8,               // 13
	e_seq_h_9,               // 14
	e_seq_h_10,              // 15
	e_seq_h_11,              // 16
	e_seq_h_12,              // 17
	e_seq_h_13,              // 18
	e_seq_h_14,              // 19
	e_seq_h_15,              // 20
	wave420l,                // 21
	wave512,                 // 22
	wave410,                 // 23
	g2v2_vp9,                // 24
	jpu,                     // 25
	coda960                  // 26
};

struct user_vrm {
	char name[VRM_NAME_LEN];
	// FIXME: Update the type to u64 with user-level library.
	u32 base;
	u32 size;
};
#endif

static bool vrm_list_sorted;
static u64 vrm_total_size;

struct vpmap_entry {
	struct vpmap info;
	struct vpmap_entry *parent;
	struct list_head list;
};

struct vrm_entry {
	struct vrm info;
	struct vrm_entry *parent;
	struct list_head list;
};

static struct vpmap_entry vrm_tb[MAX_VRMS];
static struct vpmap_entry secure_area_tb[MAX_VRM_GROUPS];

static struct vrm_entry vmem_tb[MAX_VRM_VMEM];
static struct vrm_entry vswm_tb[MAX_VRM_VSW];

static LIST_HEAD(vpmap_list_head);
static LIST_HEAD(vrm_list_head);

static inline struct vpmap_entry *vpmap_entry_of(struct vpmap *info)
{
	return container_of(info, struct vpmap_entry, info);
}

static inline struct vrm_entry *vrm_entry_of(struct vrm *info)
{
	return container_of(info, struct vrm_entry, info);
}

static void *vrm_get_va(phys_addr_t pa, u32 size)
{
	void *va = NULL;

	V_DBG(VPU_DBG_MEMORY, "physical region [0x%x - 0x%x]!!",
		pa, pa + size);

	va = (void *) ioremap_nocache((phys_addr_t) pa, PAGE_ALIGN(size));
	if (va == NULL) {
		V_DBG(VPU_DBG_ERROR, "fail to ioremap for 0x%x w/ %u.",
			pa, size);
	}

	return va;
}

static void vrm_release_va(void *va, phys_addr_t pa, u32 size)
{
	V_DBG(VPU_DBG_MEMORY, "physical region [0x%x - 0x%x]!!",
		pa, pa + size);

	iounmap(va);
}


/*
 * Search for vpmap info. from telechips,pmap-name in the reserved memory
 * area of DT in pmap linked list.
 * param : name (= telechips,pmap-name defined in node)
 *
 */
static struct vpmap *vrm_find_info_by_name(const char *name)
{
	struct vpmap_entry *entry = NULL;

	list_for_each_entry(entry, &vpmap_list_head, list) {
		int cmp = strncmp(name, entry->info.name, VRM_NAME_LEN);
		if (cmp == 0) {
			return &entry->info;
		}
	}

	return NULL;
}

/*
 * Search for vrm index from name defined in vname list.
 *
 * param : name (= string in vname struct)
 *
 */
static s32 vrm_find_vidx_by_name(const char *name)
{
	s32 i;
	for (i = 0; i < MAX_VRM_VMEM; i++) {
		int cmp = strncmp(name, vname[i], VRM_NAME_LEN);
		if (cmp == 0) {
			V_DBG(
				VPU_DBG_MEMORY,
				"[DEBUG][VRM] name=%s, index=%d.",
				name,
				i
			);
			return i;
		}
	}

	return -1;
}

/*
 * Search for vrm info from index defined in vnidx list.
 *
 * param : idx (= index in vnidx enum struct)
 *
 */
static struct vrm *vrm_find_vinfo_by_vidx(s32 idx)
{
	struct vrm_entry *entry = NULL;

	if ((idx < video) || (idx >= MAX_VRM_VMEM)) {
		V_DBG(VPU_DBG_ERROR, "Invaild index (%d).", idx);
		return NULL;
	}

	entry = &vmem_tb[idx];
	if (entry->info.base == VRM_FREED) {
		V_DBG(VPU_DBG_ERROR, "Invaild vpu info.");
		return NULL;
	}

	V_DBG(
		VPU_DBG_MEMORY,
		"[DEBUG][VRM] name=%s, size=%lu, used=%lu, base=0x%lx, pa=0x%lx, va=0x%p, groups=%lu, flags=%u, rc=%u.",
		entry->info.name,
		entry->info.size,
		entry->info.used,
		entry->info.base,
		entry->info.pa,
		entry->info.va,
		entry->info.groups,
		entry->info.flags,
		entry->info.rc
	);
	return &entry->info;
}

/*
 * Search for vrm info from index defined in vswmidx enum list.
 *
 * param : idx (= index in vswmidx enum struct)
 *
 */
static struct vrm *vrm_find_vswminfo_by_vidx(s32 idx)
{
	struct vrm_entry *entry = NULL;

	if ((idx < 0) || (idx >= MAX_VRM_VSW)) {
		V_DBG(VPU_DBG_ERROR, "Invaild index (%d).", idx);
		return NULL;
	}

	entry = &vswm_tb[idx];
	if (entry->info.base == VRM_FREED) {
		V_DBG(VPU_DBG_ERROR, "Invaild vswm info.");
		return NULL;
	}

	V_DBG(
		VPU_DBG_MEMORY,
		"[DEBUG][VRM] name=%s, size=%lu, used=%lu, base=0x%lx, pa=0x%lx, va=%p, groups=%u, flags=%u, rc=%u.",
		entry->info.name,
		entry->info.size,
		entry->info.used,
		entry->info.base,
		entry->info.pa,
		entry->info.va,
		entry->info.groups,
		entry->info.flags,
		entry->info.rc
	);
	return &entry->info;
}

/*
 * Search for size from index (for IP supported or for encoder or decoder)
 * defined in vswmidx enum list.
 * param : idx (= index in vswmidx enum struct)
 *
 */
static u32 vrm_find_vswm_size_by_idx(s32 idx)
{
	u32 size = 0;

	if ((idx < 0) || (idx >= MAX_VRM_VSW)) {
		V_DBG(VPU_DBG_ERROR, "Invaild index (%d).", idx);
		return -EINVAL;
	}

	switch (idx) {
	case wave420l:
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
		size = VPU_HEVC_ENC_WORK_BUF_SIZE;
#endif
		break;
	case wave512:
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
		size = WAVExxx_WORK_BUF_SIZE;
#endif
		break;
	case wave410:
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
		size = WAVExxx_WORK_BUF_SIZE;
#endif
		break;
	case g2v2_vp9:
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
		size = G2V2_VP9_WORK_BUF_SIZE;
#endif
		break;
	case jpu:
#ifdef CONFIG_SUPPORT_TCC_JPU
		size = JPU_WORK_BUF_SIZE;
#endif
		break;
	case coda960:
		size = VPU_WORK_BUF_SIZE;
		break;
	default:
		if ( (idx >= e_seq_h_0) &&
		   (idx < e_seq_h_0 + VPU_ENC_MAX_CNT))
			size = ENC_HEADER_BUF_SIZE;
		else if ((idx >= d_userdata_0) &&
		       (idx < d_userdata_0 + VPU_INST_MAX))
			size = USER_DATA_BUF_SIZE;
		break;
	}

	return size;
}

/*
 * Search for index defined in vswmidx enum list
 * from video codec index.
 * param : idx (= video codec index defined in TCCxxxx_VPU_CIDEC_COMMON.h)
 *
 */
static s32 vrm_find_ipidx_from_codec(s32 codec)
{
	enum vswmidx idx;

	if ( (codec < STD_AVC /*0*/) ||
	   (codec >= STD_HEVC_ENC) /*17*/) {
		V_DBG(VPU_DBG_ERROR,
			"Invaild codec index (%d).", codec);
		return -EFAULT;
	}

	switch (codec) {

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	case STD_HEVC_ENC:
		idx = wave420l;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		idx = jpu;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		idx = wave410;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC:
	case STD_VP9:
		idx = wave512;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		idx = g2v2_vp9;
		break;
#endif
	default:
		idx = coda960;
		break;
	}

	return (s32) idx;
}

static inline u64 vrm_get_base(struct vpmap *info)
{
	struct vpmap_entry *entry = vpmap_entry_of(info);
	struct vpmap_entry *iter = NULL;
	u64 base = 0;

	for (iter = entry; iter != NULL; iter = iter->parent) {
		if (iter->info.base == VRM_FREED) {
			return 0;
		}

		V_DBG(
			VPU_DBG_PMAP,
			"[DEBUG][VRM] name=%s, size=%lu, base=0x%lx, iter_base=0x%lx, groups=%lu, flags=%u",
			iter->info.name,
			iter->info.size,
			base,
			iter->info.base,
			iter->info.groups,
			iter->info.flags
		);

		if ((U64_MAX - base) >= iter->info.base) {
			base += iter->info.base;
		} else {
			/* XXX: Should not happen */
			return 0;
		}
	}

	return base;
}

static struct vrm_entry *vrm_alloc_vswm_from_base(struct vrm *info)
{
	s32 i;
	struct vrm_entry *entry;
	u64 pab = info->base;

	for (i = 0; i < MAX_VRM_VSW; i++) {
		entry = &vswm_tb[i];

		if (entry->info.va != NULL) {
			V_DBG(VPU_DBG_ERROR,
				"Is %u 'th va already remapped?", i);
			continue;
		}

		strncpy(entry->info.name, vswmname[i], VRM_NAME_LEN);

		entry->info.base = pab;
		entry->info.pa = entry->info.base;

		entry->info.groups = info->groups;
		entry->info.flags = info->flags;

		entry->info.size = vrm_find_vswm_size_by_idx(i);
		if (entry->info.size < 0) {
			return NULL;
		}

		if (entry->info.size == 0) {
			continue;
		}

		entry->info.va =
			vrm_get_va(
				(phys_addr_t) entry->info.base,
				entry->info.size
			);
		if (entry->info.va == NULL) {
			V_DBG(VPU_DBG_ERROR,
			    "VPU failed to get %u 'th virtual address in video_sw.", i);
			return NULL;
		}

		V_DBG(
			VPU_DBG_MEMORY,
			"%s (%d 'th) in VPU's FW memory succeeded in remapping va=0x%p w/ base=0x%lx, size=0x%u in video_sw area!!",
			entry->info.name,
			i,
			entry->info.va,
			pab,
			entry->info.size
		);

		pab += entry->info.size;
	}

	return entry;
}

static s32 vrm_update_info(struct device_node *np,
		struct vpmap *info,
		struct vrm *infov)
{
	struct vpmap_entry *entry = vpmap_entry_of(info);
	struct vrm_entry *ventry = NULL;

	if (infov->base != VRM_FREED) {
		ventry = vrm_entry_of(infov);

		V_DBG(
			VPU_DBG_MEMORY,
			"[DEBUG][VRM] iname=%s, name=%s, isize=%lu, size=%lu, ibase=0x%lx, base=0x%lx, igroups=%u, groups=%u, iflags=%u, flags=%u",
			infov->name,
			ventry->info.name,
			infov->size,
			ventry->info.size,
			infov->base,
			ventry->info.base,
			infov->groups,
			ventry->info.groups,
			info->flags,
			ventry->info.flags
		);
	}

	if (vrm_is_secured(info)) {
		if (info->base >= entry->parent->info.base) {
			info->base -= entry->parent->info.base;
		} else {
			/* XXX: Should not happen */
		}

		if ((U64_MAX - entry->parent->info.size) >= info->size) {
			entry->parent->info.size += info->size;
		} else {
			/* XXX: Should not happen */
		}
	} else if (vrm_is_shared(info)) {
		struct device_node *sn;

		struct vpmap *shared;
		struct vrm *vshared;

		const char *name;
		const __be32 *prop;
		s32 len;
		s32 ret;
		u64 offset = 0;
		u64 size = 0;

		sn = of_parse_phandle(np, "telechips,pmap-shared", 0);
		ret = of_property_read_string(sn, "telechips,pmap-name", &name);
		if (ret != 0) {
			return -1;
		}

		shared = vrm_find_info_by_name(name);
		if (shared == NULL) {
			return -1;
		}

		prop = of_get_property(np, "telechips,pmap-offset", &len);
		ret = of_n_addr_cells(np);
		if ((prop != NULL) && (ret == (len/(s32)sizeof(u32)))) {
			offset = of_read_number(prop, of_n_addr_cells(np));
		}

		prop = of_get_property(np, "telechips,pmap-shared-size", &len);
		ret = of_n_size_cells(np);
		if ((prop != NULL) && (ret == (len/(s32)sizeof(u32)))) {
			size = of_read_number(prop, of_n_size_cells(np));
		}

		if ((ventry != NULL) && vrm_is_vstored(info)) {
			s32 idx = vrm_find_vidx_by_name(name);
			if ((idx < 0) || (idx >= MAX_VRM_VMEM)) {
				return -1;
			}

			vshared = vrm_find_vinfo_by_vidx(idx);
			if (vshared == NULL) {
				return -1;
			}

			ventry->info.base = vshared->base + vshared->size;
			ventry->info.pa = ventry->info.base;
			ventry->info.size = size;
			ventry->info.flags |= VRM_FLAG_VREAREND;

			if ((size > 0) && vrm_is_vshared(vshared)) {
				struct vrm *vnext =
					vrm_find_vinfo_by_vidx(idx + 1);
				if (vnext == NULL) {
					return -1;
				}

				ventry->info.sidx[idx] = 1;
				ventry->info.sidx[idx + 1] = 1;

				idx = vrm_find_vidx_by_name(
					ventry->info.name);
				if (idx < 0) {
					return -1;
				}

				vshared->sidx[idx] = 1;

				vnext->base = vshared->base + offset;
				vnext->pa = vnext->base;
				vnext->size = offset;

				V_DBG(
					VPU_DBG_MEMORY,
					"[DEBUG][VRM] idx=%d, name=%s, size=%lu, used=%lu, base=0x%lx, pa=0x%lx, va=%p, groups=%u, rc=%u, flags=%u",
					idx,
					vnext->name,
					vnext->size,
					vnext->used,
					vnext->base,
					vnext->pa,
					vnext->va,
					vnext->groups,
					vnext->rc,
					vnext->flags
				);
			}

			V_DBG(
				VPU_DBG_MEMORY,
				"[DEBUG][VRM] sname=%s, vname=%s, iname=%s, ssize=%lu, vsize=%lu, isize=%lu, sbase=0x%lx, vbase=0x%lx, ibase=0x%lx, offset=0x%lx, pa=0x%lx, va=%p, used=%u, sgroups=%u, vgroups=%u, igroups=%u, sflags=%u, vflags=%u, iflags=%u\n",
				name,
				ventry->info.name,
				info->name,
				shared->size,
				ventry->info.size,
				info->size,
				shared->base,
				ventry->info.base,
				info->base,
				offset,
				ventry->info.pa,
				ventry->info.va,
				ventry->info.used,
				shared->groups,
				ventry->info.groups,
				info->groups,
				shared->flags,
				ventry->info.flags,
				info->flags
			);
		}

		entry->parent = vpmap_entry_of(shared);
		info->base = offset;
		info->size = size;

		V_DBG(
			VPU_DBG_PMAP,
			"[DEBUG][VRM] name=%s, size=%lu, base=0x%lx, groups=%lu, flags=%u",
			info->name,
			info->size,
			info->base,
			info->groups,
			info->flags
		);
	} else {
		if ( (ventry != NULL) &&
		   vrm_is_vstored(info) &&
		   vrm_is_vswed(info)) {
			ventry->parent = vrm_alloc_vswm_from_base(infov);

			if (ventry->parent == NULL) {
				return -1;
			}

			V_DBG(
				VPU_DBG_MEMORY,
				"[DEBUG][VRM] name=%s, vname=%s, size=%lu, vsize=%lu, base=0x%lx, vbase=0x%lx, groups=%u, vgroups=%u, flags=%u, vflags=%u",
				entry->info.name,
				ventry->info.name,
				entry->info.size,
				ventry->info.size,
				entry->info.base,
				ventry->info.base,
				entry->info.groups,
				ventry->info.groups,
				entry->info.flags,
				ventry->info.flags
			);
		}
	}

	/* Remove pmap with size 0 from entry list */
	if (entry->info.size == 0U) {
		list_del(&entry->list);
	}

	return 0;
}

static s32 vrm_get_info_internal(struct vpmap *info)
{
	struct vpmap_entry *entry = vpmap_entry_of(info);

	if (entry->parent != NULL) {
		/* XXX: MISRA-C rule does not allow recursion */
		s32 ret = vrm_get_info_internal(&entry->parent->info);
		if (ret == 0) {
			return 0;
		}
	}

	if (info->rc < UINT_MAX) {
		++info->rc;
	}

	return 1;
}

static void vrm_release_info_internal(struct vpmap *info)
{
	struct vpmap_entry *entry = vpmap_entry_of(info);

	if ((info->rc != 0U) && (entry->parent != NULL)) {
		/* XXX: MISRA-C rule does not allow recursion */
		vrm_release_info_internal(&entry->parent->info);
	}

	if (info->rc > 0U) {
		--info->rc;
	}
}

s32 vrm_get_info(const char *name, struct vpmap *mem)
{
	s32 ret;
	struct vpmap *info = vrm_find_info_by_name(name);

	if (mem != NULL) {
		mem->base = 0;
		mem->size = 0;
	}

	if ((mem == NULL) || (info == NULL)) {
		return 0;
	}

	ret = vrm_get_info_internal(info);
	if (ret == 0) {
		return -1;
	}

	(void) memcpy(mem, info, sizeof(struct vpmap));
	mem->base = vrm_get_base(info);

	return 1;
}
EXPORT_SYMBOL(vrm_get_info);

s32 vrm_release_info(const char *name)
{
	struct vpmap *info = vrm_find_info_by_name(name);
	if (info == NULL) {
		return 0;
	}

	vrm_release_info_internal(info);

	return 1;
}
EXPORT_SYMBOL(vrm_release_info);

 static s32 vrm_alloc_procmem(
			s32 codec,
			MEM_ALLOC_INFO_t *alloc_info,
			vputype type
		)
{
	struct vrm *info = NULL;
	MEM_ALLOC_INFO_t *uinfo = NULL;

	s32 bufidx = 0;
	s32 idx = (s32) type;
	s32 ipidx = vrm_find_ipidx_from_codec(codec);

	if (alloc_info == NULL) {
		V_DBG(VPU_DBG_ERROR, "info. to allocate memory is wrong.");
		return -EFAULT;
	}

	if ((ipidx < wave420l) || (ipidx > coda960)) {
		V_DBG(
			VPU_DBG_ERROR,
			"codec type[%d] is wrong and ip[%d] is not set for dev idx[%d].",
			codec,
			ipidx,
			idx
		);
		return -EFAULT;
	}

	if ((idx < VPU_DEC) || (idx >= VPU_MAX)) {
		V_DBG(VPU_DBG_ERROR, "dev idx[%d] is wrong.", idx);
		return -EFAULT;
	}

	V_DBG(
		VPU_DBG_MEMORY,
		" [DEBUG][VRM] run-time memory allocation w/ ip[%d], buffer[%d], dev idx[%d] enter!",
 		ipidx,
		alloc_info->buffer_type,
		idx
	);

	uinfo = alloc_info;
	bufidx = (s32) uinfo->buffer_type;

	switch (bufidx) {

	case BUFFER_WORK:
		info = vrm_find_vswminfo_by_vidx(ipidx);
		break;

	case BUFFER_USERDATA:     // for decoder
	case BUFFER_SEQHEADER:    // for encoder
		info = vrm_find_vswminfo_by_vidx(idx);
		break;

	default:
		info = vrm_find_vinfo_by_vidx(idx);
		break;
	}

	if (info == NULL) {
		return -EFAULT;
	}

	if (vrm_is_vrearend(info)) {
		info->ip = (u32) ipidx;
		info->pa -= uinfo->request_size;
		info->used += uinfo->request_size;

		uinfo->phy_addr = (phys_addr_t) info->pa;
	} else {
		uinfo->phy_addr = (phys_addr_t) info->pa;

		switch (bufidx) {

		case BUFFER_WORK:
			if (info->rc == 0) {
				info->pa += info->size;
				info->used += info->size;
			}
			break;
		case BUFFER_USERDATA:     // for decoder
		case BUFFER_SEQHEADER:    // for encoder
			info->pa += info->size;
			info->used += info->size;
			break;
		default:
			info->ip = (u32) ipidx;
			info->pa += uinfo->request_size;
			info->used += uinfo->request_size;
			break;
		}
	}

	if (uinfo->phy_addr == 0) {
		V_DBG(
			VPU_DBG_ERROR,
			"[DEBUG][VRM] vpu-%d failed to get the physical memory (0x%x) for buffer type-%d.",
			idx,
			uinfo->phy_addr,
			bufidx
		);

		return -EFAULT;
	}

	if (vrm_is_vswed(info)) {
		uinfo->request_size = info->size;
	} else {
		if (bufidx != BUFFER_FRAMEBUFFER) {
			info->va = vrm_get_va(
					uinfo->phy_addr,
					uinfo->request_size
				 );
			if (info->va == NULL) {
				return -EFAULT;
			}
		}
	}

	uinfo->kernel_remap_addr = info->va;

	if (info->rc < UINT_MAX) {
		if (vrm_is_vswed(info)) {
			++info->rc;
		} else {
			info->rc = 1U;
		}
	}

	V_DBG(
		VPU_DBG_MEM_USAGE,
		"[DEBUG][VRM] buf type=%d, name=%s, size=%lu, usize=%lu, used=%lu, base=0x%lx, pa=0x%lx, upa=0x%lx, va=0x%p, uva=0x%p, groups=%u, flags=%u, rc=%u, codec=%d.",
		uinfo->buffer_type,
		info->name,
		info->size,
		uinfo->request_size,
		info->used,
		info->base,
		info->pa,
		uinfo->phy_addr,
		info->va,
		uinfo->kernel_remap_addr,
		info->groups,
		info->flags,
		info->rc,
		info->ip
	);

	return 0;
}

static s32 vrm_free_procmem(vputype type)
{
	s32 idx = (s32) type;
	u32 ip = 0;

	struct vrm *info = NULL;
	struct vrm *swinfo = NULL;

	if ((idx < VPU_DEC) || (idx >= VPU_MAX)) {
		return -EFAULT;
	}

	info = vrm_find_vinfo_by_vidx(idx);
	if (info == NULL) {
		return -EFAULT;
	}

	V_DBG(
		VPU_DBG_MEM_USAGE,
		"[DEBUG][VRM] name=%s, size=%lu, used=%lu, base=0x%lx, pa=0x%lx, va=0x%p, groups=%u, flags=%u, rc=%u, ip=%u.",
		info ->name,
		info ->size,
		info ->used,
		info ->base,
		info ->pa,
		info ->va,
		info ->groups,
		info ->flags,
		info ->rc,
		info ->ip
	);

	if (info->rc == 0) {
		V_DBG(
			VPU_DBG_INFO,
			"dev idx[%d] is already freed (rc=%u).",
			idx,
			info->rc
		);
		return 0;
	}

	ip = info->ip;

	(void) vrm_release_va(info->va, info->pa, info->used);

	if (vrm_is_vrearend(info)) {
		info->pa += info->used;
		info->used = 0;
	} else {
		info->pa -= info->used;
		info->used = 0;
	}

	info->va = NULL;

	if (info->base != info->pa) {
		V_DBG(
			VPU_DBG_ERROR,
			"the memory address is wrong for dev idx[%d] (base=0x%lx != pa=0x%lx).",
			idx,
			info->base,
			info->pa
		);
		return -EINVAL;
	}

	if (ip == VRM_DATA_NA) {
		V_DBG(VPU_DBG_ERROR, "video ip is not set for vpu-%d.", idx);
		return -EFAULT;
	}

	if ((ip > 0U) && (ip < MAX_VRM_VSW)) {
		info->ip = VRM_DATA_NA;
	}

	if (info->rc > 0U) {
		--info->rc;
	}

	/*
	* To free video_sw memory used for one of encoders or decoders.
	*/
	swinfo = vrm_find_vswminfo_by_vidx(idx);
	if (swinfo == NULL) {
		return -EFAULT;
	}

	V_DBG(
		VPU_DBG_MEM_USAGE,
		"[[DEBUG][VRM] name=%s, size=%lu, used=%lu, base=0x%lx, pa=0x%lx, va=0x%p, groups=%u, flags=%u, rc=%u, ip=%u.",
		swinfo ->name,
		swinfo ->size,
		swinfo ->used,
		swinfo ->base,
		swinfo ->pa,
		swinfo ->va,
		swinfo ->groups,
		swinfo ->flags,
		swinfo ->rc,
		swinfo ->ip
	);
#if 0
	if (swinfo->rc == 0U) {
		V_DBG(VPU_DBG_ERROR, "vswm has not used for vpu-%d.", idx);
		return -EFAULT;
	}

	if (swinfo->rc > 0U) {
		--swinfo->rc;
		if (swinfo->rc > 0U) {
			/* should not happen */
			V_DBG(VPU_DBG_ERROR,
				"the same vswm is used for other vpu device.");
			return EFAULT;
		}
	}

	swinfo->pa -= swinfo->used;
	swinfo->used -= swinfo->size;

	if ( (swinfo->base != swinfo->pa) ||
	   (swinfo->used != 0)) {
		V_DBG(
			VPU_DBG_ERROR,
			"vswm-%d failed to be freed (base=0x%lx, pa=0x%lx, size=%lu, used=%lu).",
			idx,
			swinfo->base,
			swinfo->pa,
			swinfo->size,
			swinfo->used
		);
		return -EFAULT;
	}
#else
	if (swinfo->rc != 0U) {
		if (swinfo->rc > 0U) {
			--swinfo->rc;
			if (swinfo->rc > 0U) {
				/* should not happen */
				V_DBG(VPU_DBG_ERROR,
						"the same vswm is used for other vpu device.");
				return EFAULT;
			}
		}

		swinfo->pa -= swinfo->used;
		swinfo->used -= swinfo->size;

		if ( (swinfo->base != swinfo->pa) ||
				(swinfo->used != 0)) {
			V_DBG(
					VPU_DBG_ERROR,
					"vswm-%d failed to be freed (base=0x%lx, pa=0x%lx, size=%lu, used=%lu).",
					idx,
					swinfo->base,
					swinfo->pa,
					swinfo->size,
					swinfo->used
				 );
			return -EFAULT;
		}
	}
#endif
	/*
	 * To free video ip used.
	 */
	swinfo = vrm_find_vswminfo_by_vidx(ip);
	if (swinfo == NULL) {
		return -EFAULT;
	}

	V_DBG(
		VPU_DBG_MEM_USAGE,
		"[DEBUG][VRM] name=%s, size=%lu, base=0x%lx, va=0x%p, groups=%u, flags=%u, rc=%u, ip=%u.",
		swinfo ->name,
		swinfo ->size,
		swinfo ->base,
		swinfo ->va,
		swinfo ->groups,
		swinfo ->flags,
		swinfo ->rc,
		swinfo ->ip
	);
#if 0
	if (swinfo->rc == 0U) {
		V_DBG(VPU_DBG_ERROR, "video ip  has not used for vpu-%d.", idx);
		return -EFAULT;
	}

	if (swinfo->rc > 0U) {
		--swinfo->rc;
		if (swinfo->rc > 0U) {
			V_DBG(VPU_DBG_ERROR,
				"video ip is used for other vpu device.");
			return 0;
		}
	}

	swinfo->pa -= swinfo->used;
	swinfo->used -= swinfo->size;

	if ( (swinfo->base != swinfo->pa) ||
	   (swinfo->used != 0)) {
		V_DBG(
			VPU_DBG_ERROR,
			"vswm-%d failed to be freed (base=0x%lx, pa=0x%lx, size=%lu, used=%lu).",
			idx,
			swinfo->base,
			swinfo->pa,
			swinfo->size,
			swinfo->used
		);
		return -EFAULT;
	}

#else
	if (swinfo->rc != 0U) {
		if (swinfo->rc > 0U) {
			--swinfo->rc;
			if (swinfo->rc > 0U) {
				V_DBG(VPU_DBG_INFO,
						"video ip is used for other vpu device.");
				return 0;
			}
		}

		swinfo->pa -= swinfo->used;
		swinfo->used -= swinfo->size;

		if ( (swinfo->base != swinfo->pa) ||
				(swinfo->used != 0)) {
			V_DBG(
					VPU_DBG_ERROR,
					"vswm-%d failed to be freed (base=0x%lx, pa=0x%lx, size=%lu, used=%lu).",
					idx,
					swinfo->base,
					swinfo->pa,
					swinfo->size,
					swinfo->used
				 );
			return -EFAULT;
		}
	}

#endif
	return 0;
}

s32 vrm_get_freemem(s32 idx)
{
	s64 freed = 0;
	struct vrm *vshared = NULL;

	struct vrm *info = vrm_find_vinfo_by_vidx(idx);
	if (info == NULL) {
		return -EFAULT;
	}

	freed = info->size - info->used;

	if (vrm_is_vshared(info) || vrm_is_vrearend(info)) {
		for (idx = 0; idx < MAX_VRM_VMEM; idx++) {
			if (info->sidx[idx] == 1) {
				vshared = vrm_find_vinfo_by_vidx(idx);
				if (vshared == NULL) {
					return -EFAULT;
				}

				freed -= vshared->used;
			}
		}
	}

	if (freed < 0) {
		return -ENOMEM;
	}

	return (s32) freed;
}

u32 vrm_alloc_count(s32 idx)
{
	struct vrm *info;

	if ((idx < video) || (idx >= video_sw))
		return 0;

	info = vrm_find_vinfo_by_vidx(idx);
	if (info == NULL) {
		return -EFAULT;
	}

	return info->rc;
}

s32 vrm_get_instance(s32 idx)
{
	s32 i, nInstance = -1;
	struct vrm *info;

	if ((idx < 0) || (idx >= MAX_VRM_VMEM)) {
		V_DBG(VPU_DBG_ERROR,
			"Invaild instance number.");
		return -EFAULT;
	}

	if (vrm_get_freemem(idx) < 0) {
		V_DBG(VPU_DBG_ERROR,
			"vpu failed to get new instance for decoder.");
		return -ENOMEM;
	}

	info = vrm_find_vinfo_by_vidx(idx);
	if (info == NULL) {
		return -EFAULT;
	}

	if (info->rc == 0) {
		nInstance = idx;
	} else {
		for (i = 0; i < MAX_VRM_VMEM; i++) {
			info = vrm_find_vinfo_by_vidx(i);

			if (info->rc == 0) {
				nInstance = i;
				break;
			}
		}
	}

	V_DBG(VPU_DBG_INSTANCE, "Instance-#%d is taken (required-#%d).",
		idx, nInstance);

	return nInstance;
}

s32 vrm_check_instance_available(void)
{
	s32 i;
	s32 freed = 0;
	struct vrm *info;

	for (i = 0; i < MAX_VRM_VMEM; i++) {
		info = vrm_find_vinfo_by_vidx(i);

		if (info->rc == 0) {
			freed++;
		}
	}

	V_DBG(VPU_DBG_INSTANCE, "there are #%d instance numbers available.",
		freed);

	return freed;
}

void vrm_clear_instance(s32 idx)
{
	struct vrm *info;

	if ((idx < 0) || (idx >= MAX_VRM_VMEM)) {
		V_DBG(VPU_DBG_ERROR,
			"Invaild instance number.");
		return;
	}

	info = vrm_find_vinfo_by_vidx(idx);
	if (info == NULL) {
		return;
	}

	if (info->rc != 0) {
		V_DBG(VPU_DBG_ERROR,
			"vpu failed to clear instance-#%d for decoder.",
			idx);
	}

	V_DBG(VPU_DBG_INSTANCE, "vpu instance-#%d is cleared.",
		idx);
}
//*/

#define vrm_get_order(info) \
	(((info)->groups == VRM_DATA_NA) ? 2 : (vrm_is_shared(info) ? 1 : 0))

static int vrm_compare(void *p, struct list_head *a, struct list_head *b)
{
	struct vpmap *pa = &list_entry(a, struct vpmap_entry, list)->info;
	struct vpmap *pb = &list_entry(b, struct vpmap_entry, list)->info;

	s32 order_a;
	s32 order_b;

	u64 base_a;
	u64 base_b;

	if ((pa == NULL) || (pb == NULL)) {
		/* XXX: Should not happen */
		return 0;
	}

	order_a = vrm_get_order(pa);
	order_b = vrm_get_order(pb);

	if (order_a != order_b) {
		return (order_a < order_b) ? -1 : 1;
	}

	base_a = vrm_get_base(pa);
	base_b = vrm_get_base(pb);

	return (base_a <= base_b) ? -1 : 1;
}

#undef vrm_get_order

static int proc_vrm_show(struct seq_file *m, void *v)
{
	struct vpmap_entry *entry = NULL;

	s32 shared_info = 0;
	s32 secured_info = 0;

	if (!vrm_list_sorted) {
		list_sort(NULL, &vpmap_list_head, &vrm_compare);
		vrm_list_sorted = (bool)true;
	}

	/* format:   -10s       -10s        3s  3s    s */
	seq_puts(m, "base_addr  virt_addr          size       used       flags    ref   shared   name\n");

	list_for_each_entry(entry, &vpmap_list_head, list) {
		struct vrm *vinfo = NULL;
		struct vpmap *info = &entry->info;

		s32 i;
		//char *is_vshared = vrm_is_vshared(info) ? "*" : " ";
		int is_vshared = vrm_is_vshared(info) ? 1 : 0;

		u64 base = vrm_get_base(info);

		if (base == 0U) {
			continue;
		}

		if ((shared_info == 0) && vrm_is_shared(info)) {
			seq_puts(m, " ======= Shared Area Info. =======\n");
			shared_info = 1;
		}

		if ((secured_info == 0) && (info->groups == VRM_DATA_NA)) {
			seq_puts(m, " ======= Secured Area Info. =======\n");
			secured_info = 1;
		}

		if ((secured_info == 0) && vrm_is_vstored(info)) {
			s32 idx = vrm_find_vidx_by_name(info->name);
			if ((idx < 0) || (idx >= MAX_VRM_VMEM)) {
				return -1;
			}

			vinfo = vrm_find_vinfo_by_vidx(idx);
			if (vinfo == NULL) {
				return -1;
			}

			is_vshared = vrm_is_vshared(vinfo) | (vrm_is_vrearend(vinfo) ? 1 : 0);

			seq_printf(m,
				"0x%8llx 0x%16p 0x%8.8llx 0x%8.8llx   %-3u     %-3u    %s      %s\n",
				vinfo->base,
				vinfo->va,
				vinfo->size,
				vinfo->used,
				vinfo->flags,
				vinfo->rc,
				(is_vshared == 1) ? "*" : " ",
				vinfo->name
			);

			if (vrm_is_vshared(vinfo)) {
				vinfo = vrm_find_vinfo_by_vidx(idx + 1);
				if (vinfo == NULL) {
					return -1;
				}

				is_vshared = vrm_is_vrearend(vinfo) ? 1 : 0;

				seq_printf(m,
					"0x%8llx 0x%16p 0x%8.8llx 0x%8.8llx   %-3u     %-3u    %s      %s\n",
					vinfo->base,
					vinfo->va,
					vinfo->size,
					vinfo->used,
					vinfo->flags,
					vinfo->rc,
					(is_vshared == 1) ? "*" : " ",
					vinfo->name
				);
			}
		} else {
			seq_printf(m, "0x%8llx 0x%16llx 0x%8.8llx 0x%8.8llx   %-3u     %-3u    %s      %s\n",
				base,
				0LLU,
				info->size,
				0LLU,
				info->flags,
				info->rc,
				(is_vshared == 1) ? "*" : " ",
				info->name
			);
		}

		if (vrm_is_vswed(info)) {
			seq_puts(m, " --- Enter video_sw Area Info. ---\n");

			for (i = 0; i < MAX_VRM_VSW; i++) {
				vinfo = vrm_find_vswminfo_by_vidx(i);
				if (vinfo == NULL) {
					return -EFAULT;
				}

				if (vinfo->size == 0) {
					continue;
				}

				seq_printf(m,
					"0x%8llx 0x%16p 0x%8.8llx 0x%8.8llx   %-3u     %-3u    %s      %s\n",
					vinfo->base,
					vinfo->va,
					vinfo->size,
					vinfo->used,
					vinfo->flags,
					vinfo->rc,
					(is_vshared == 1) ? "*" : " ",
					vinfo->name
				);
			}

			seq_puts(m, " --- Out   video_sw Area Info. ---\n");
		}
	}

	seq_puts(m, " ======= Total Area Info. =======\n");
	seq_printf(m, "0x00000000                    0x%8.8llx (total)\n", vrm_total_size);

	return 0;
}

static int proc_vrm_open(struct inode *inode, struct file *file)
{
	return single_open(file, &proc_vrm_show, PDE_DATA(inode));
}

static long proc_vrm_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct user_vrm __user *uinfo = (struct user_vrm __user *) arg;
	struct user_vrm ret_info;
	struct vpmap info;
	s32 ret = -1;
	ulong copied;

	copied = copy_from_user(&ret_info, uinfo, sizeof(struct user_vrm));
	if (copied != 0UL) {
		return -1;
	}

	switch (cmd) {
	case PROC_VRM_CMD_GET:
		ret = vrm_get_info(ret_info.name, &info);

		ret_info.base = (u32)info.base;
		ret_info.size = (u32)info.size;
		break;
	case PROC_VRM_CMD_RELEASE:
		ret = vrm_release_info(ret_info.name);
		break;
	default:
		/* Nothing to do */
		break;
	}

	copied = copy_to_user(uinfo, &ret_info, sizeof(struct user_vrm));
	if (copied != 0UL) {
		return -1;
	}

	return ret;
}

static const struct file_operations vdev_rm_fops = {
	.owner              = THIS_MODULE,
	.open               = proc_vrm_open,
	.read               = seq_read,
	.llseek             = seq_lseek,
	.release            = single_release,
	.unlocked_ioctl     = proc_vrm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl       = proc_vrm_ioctl,
#endif
};

static struct miscdevice vrm_misc_device = {
	MISC_DYNAMIC_MINOR,
	VRM_NAME,
	&vdev_rm_fops,
};

int vrm_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;

	struct vpmap_entry *entry;
	struct vrm_entry *ventry;

#if defined(CONFIG_PROC_FS)
	struct proc_dir_entry *dir;
#endif
	static s32 num_vrms_left = (s32) MAX_VRMS;

	u32 i;

	vrm_list_sorted = (bool)false;
	vrm_total_size = (u64)0U;

	V_DBG(VPU_DBG_MEM_SEQ, "[DEBUG][VRM] enter");

#if defined(CONFIG_PROC_FS)
	dir = proc_create("vrm", 0444, NULL, &vdev_rm_fops);
	if (dir == NULL) {
		return -ENOMEM;
	}
#endif

	/*
	 * Initialize vpmap, secure area and vpu tables as 0  and set initial value for
	 * each secure area.
	 */
	(void)memset(vrm_tb, 0, sizeof(vrm_tb));
	(void)memset(secure_area_tb, 0, sizeof(secure_area_tb));

	(void)memset(vmem_tb, 0, sizeof(vmem_tb));
	(void)memset(vswm_tb, 0, sizeof(vswm_tb));

	for (i = 0; i < MAX_VRM_GROUPS; i++) {
		entry = &secure_area_tb[i];
		if (entry == NULL) {
			/* XXX: Should not happen */
			return -ENOMEM;
		}

		(void)sprintf(entry->info.name, "secure_area%d", (s32)i + 1);
		entry->info.base = VRM_FREED;
		entry->info.groups = VRM_DATA_NA;
	}

	for (i = 0; i < MAX_VRM_VMEM; i++) {
		ventry = &vmem_tb[i];
		if (ventry == NULL) {
			/* XXX: Should not happen */
			return -ENOMEM;
		}

		ventry->info.base = VRM_FREED;
		ventry->info.groups = VRM_DATA_NA;
	}

	for (i = 0; i < MAX_VRM_VSW; i++) {
		ventry = &vswm_tb[i];
		if (ventry == NULL) {
			/* XXX: Should not happen */
			return -ENOMEM;
		}

		ventry->info.base = VRM_FREED;
		ventry->info.groups = VRM_DATA_NA;
		ventry->info.ip = VRM_DATA_NA;
	}

	/*
	 * Add reg property for each device tree node if there are only
	 * size property with dynamic allocation.
	 */
	for_each_compatible_node(np, NULL, "telechips,pmap") {

		struct reserved_mem *rmem;

		const char *name;
		s32 ret;

		u64 base;
		u64 size;
		phys_addr_t end;

		const __be32 *prop;

		//s32 len;
		u64 groups = 0;
		u32 flags = 0;
		s32 sort;

		ret = of_property_read_string(np, "telechips,pmap-name", &name);
		if (ret != 0) {
			continue;
		}

		rmem = of_reserved_mem_lookup(np);
		if (rmem == NULL) {
			V_DBG(VPU_DBG_MEM_SEQ, "%s failed to read!", name);
			continue;
		}

		base = rmem->base;
		size = rmem->size;

		if ((rmem->size > 0U) && ((ULONG_MAX - rmem->base) >= rmem->size)) {
			end = rmem->base + rmem->size - 1U;
		} else {
			end = rmem->base;
		}

#ifdef CONFIG_PMAP_SECURED
		/*
		 * When CONFIG_PMAP_SECURED is not set, simply ignore "telechips,
		 * pmap-secured" property.
		 */
		prop = of_get_property(np, "telechips,pmap-secured", &len);
		if (prop != NULL) {
			groups = of_read_number(prop, len/MAX_VRM_GROUPS);
			if ((groups >= 1U) && (groups <= MAX_VRM_GROUPS)) {
				flags |= VRM_FLAG_SECURED;
			}
		}
#endif
		prop = of_get_property(np, "telechips,pmap-shared", NULL);
		if (prop != NULL) {
			flags |= VRM_FLAG_SHARED;
		}

		sort = vrm_find_vidx_by_name(name);
		if ((sort >= 0) && (sort < MAX_VRM_VMEM)) {
			flags |= VRM_FLAG_VSTORED;

			if ((sort == video) || (sort == video_ext)) {
				flags |= VRM_FLAG_VSHARED;
			} else if (sort == video_sw) {
				flags |= VRM_FLAG_VSWED;
			}
		}

		if ( (sort == video_sw) &&
		   (size < VPU_SW_ACCESS_REGION_SIZE) ) {
			V_DBG(
				VPU_DBG_ERROR,
				"video_sw (%d) doesn't have enough memory (size=0x%lx, VPU_SW_ACCESS_REGION_SIZE=0x%lx, Dec-instance=%d, Enc-instance=%d.",
				sort,
				size,
				VPU_SW_ACCESS_REGION_SIZE,
				VPU_INST_MAX,
				VPU_ENC_MAX_CNT
			);
			return -EINVAL;
		}

		/* Initialize pmap and register it into pmap list */
		if (num_vrms_left == 0) {
			return -ENOMEM;
		}

		entry = &vrm_tb[(s32)MAX_VRMS - num_vrms_left];
		--num_vrms_left;

		strncpy(entry->info.name, name, VRM_NAME_LEN);
		entry->info.base = base;
		entry->info.size = size;
		entry->info.groups = (u32) groups;
		entry->info.rc = 0;
		entry->info.flags = flags;

		list_add_tail(&entry->list, &vpmap_list_head);

		/*
		 * Register secure area as a parent for each secured pmap and
		 * calculate base address of each secure area.
		 */
		if (vrm_is_secured(&entry->info)) {
			s32 group_idx = (s32)entry->info.groups - 1;

			entry->parent = &secure_area_tb[group_idx];
			entry->parent->info.flags |= entry->info.flags;

			if (entry->info.base < entry->parent->info.base) {
				entry->parent->info.base = entry->info.base;
			}

			V_DBG(
				VPU_DBG_MEM_SEQ,
				"[DEBUG][VRM] name=%s, size=%lu, base=0x%lx, gbase=0x%lx, groups=%lu, ggroups=%lu, group_idx=%d, flags=%u, gflags=%u.",
				entry->info.name,
				entry->info.size,
				entry->info.base,
				entry->parent->info.base,
				entry->info.groups,
				entry->parent->info.groups,
				group_idx,
				entry->info.flags,
				entry->parent->info.flags
			);
		}

		/*
		 * Register secure area as a parent for each secured pmap and
		 * calculate base address of each secure area.
		 */
		if (vrm_is_vstored(&entry->info)) {
			ventry = &vmem_tb[sort];

			strncpy(ventry->info.name, name, VRM_NAME_LEN);
			ventry->info.base = entry->info.base;
			ventry->info.size = entry->info.size;
			ventry->info.pa = ventry->info.base;
			ventry->info.groups = entry->info.groups;
			ventry->info.flags |= entry->info.flags;

			if (vrm_is_vshared(&entry->info)) {
				ventry->parent = &vmem_tb[sort + 1];

				ventry->info.sidx[sort + 1] = 1;
				ventry->parent->info.sidx[sort] = 1;

				strncpy(ventry->parent->info.name,
					vname[sort + 1], VRM_NAME_LEN);
				ventry->parent->info.base =
					ventry->info.base + ventry->info.size;
				ventry->parent->info.size = ventry->info.size;
				ventry->parent->info.pa = ventry->parent->info.base;
				ventry->parent->info.groups = ventry->info.groups;
				ventry->parent->info.flags |= ventry->info.flags;
				ventry->parent->info.flags |= VRM_FLAG_VREAREND;

				V_DBG(
					VPU_DBG_MEM_SEQ,
					"[DEBUG][VRM] sort_idx=%d, name=%s, size=%lu, used=%lu, base=0x%lx, pa=0x%lx, va=%p, groups=%u, rc=%u, flags=%u.",
					sort,
					ventry->parent->info.name,
					ventry->parent->info.size,
					ventry->parent->info.used,
					ventry->parent->info.base,
					ventry->parent->info.pa,
					ventry->parent->info.va,
					ventry->parent->info.groups,
					ventry->parent->info.rc,
					ventry->parent->info.flags
				);
			}

			V_DBG(
				VPU_DBG_MEM_SEQ,
				"[DEBUG][VRM] name=%s, vname=%s, size=%lu, vsize=%lu, vused=%lu, base=0x%lx, vbase=0x%lx, vpa=0x%lx, vva=%p, groups=%u, vgroups=%u, sort_idx=%d, rc=%u, vrc=%u, flags=%u, vflags=%u.",
				entry->info.name,
				ventry->info.name,
				entry->info.size,
				ventry->info.size,
				ventry->info.used,
				entry->info.base,
				ventry->info.base,
				ventry->info.pa,
				ventry->info.va,
				entry->info.groups,
				ventry->info.groups,
				sort,
				entry->info.rc,
				ventry->info.rc,
				entry->info.flags,
				ventry->info.flags
			);
		}

		if ((U64_MAX - vrm_total_size) >= entry->info.size) {
			vrm_total_size += entry->info.size;
		} else {
			/* XXX: Should not happen */
		}

		/* Update info with pmap related properties */
		ret = vrm_update_info(np, &entry->info, &ventry->info);

		if (ret != 0) {
			V_DBG(
				VPU_DBG_ERROR,
				"[DEBUG][VRM] name=%s, size=%lu, base=0x%lx, groups=%u, flags=%u.",
				entry->info.name,
				entry->info.size,
				entry->info.base,
				entry->info.groups,
				entry->info.flags
			);
			return -ENOMEM;
		}
	}

	/*
	 * Register secure areas with size > 0 into pmap list to print
	 * out later when user requests.
	 */
	for (i = 0; i < MAX_VRM_GROUPS; i++) {
		entry = &secure_area_tb[i];
		if (entry->info.size != 0U) {
			list_add_tail(&entry->list, &vpmap_list_head);
		}
	}

	V_DBG(
		VPU_DBG_MEM_SEQ,
		"[DEBUG][VRM] Done - total reserved %llu bytes (%lluK, %lluM).",
		vrm_total_size,
		vrm_total_size >> 10U,
		vrm_total_size >> 20U
	);

	if (misc_register(&vrm_misc_device)) {
		dev_err(&pdev->dev, "VPU RM: failed to register misc device.\n");
		return -ENOMEM;
	}

	return 0;
}
EXPORT_SYMBOL(vrm_probe);

int vrm_remove(struct platform_device *pdev)
{
	misc_deregister(&vrm_misc_device);
	return 0;
}
EXPORT_SYMBOL(vrm_remove);

//
/*
 * Specify an external function to interface with user-space.
 * For reference, consider compatibility with the existing vpu_buffer.c
 * vmem_proc_alloc_memory, vmem_proc_free_memory,
 * vmem_get_free_memory
 */
int vmem_proc_alloc_memory(
			int codec_type,
			MEM_ALLOC_INFO_t *alloc_info,
			vputype type
		)
{
	s32 ret = vrm_alloc_procmem(codec_type, alloc_info, type);
	if (ret < 0) {
		return -ENOMEM;
	}

	return 0;
}
EXPORT_SYMBOL(vmem_proc_alloc_memory);

int vmem_proc_free_memory(vputype type)
{
	s32 ret = vrm_free_procmem(type);
	if (ret < 0) {
		return -ENOMEM;
	}

	return 0;
}
EXPORT_SYMBOL(vmem_proc_free_memory);

unsigned int vmem_get_free_memory(vputype type)
{
	s32 freed = vrm_get_freemem(type);
	if (freed < 0) {
		return -ENOMEM;
	}

	return (unsigned int) freed;
}
EXPORT_SYMBOL(vmem_get_free_memory);

pgprot_t vmem_get_pgprot(pgprot_t ulOldProt, unsigned long ulPageOffset)
{
	pgprot_t newProt = pgprot_writecombine(ulOldProt);

	V_DBG(VPU_DBG_MEM_SEQ, "[DEBUG][VRM] (write-combine).");

	return newProt;
}
EXPORT_SYMBOL(vmem_get_pgprot);

int vmem_alloc_count(int type)
{
	s32 alloced = vrm_alloc_count(type);
	if (alloced < 0) {
		return -EFAULT;
	}

	return (int) alloced;
}

void vdec_get_instance(int *nIdx)
{
	s32 idx = *nIdx;
	*nIdx = vrm_get_instance(idx);
}
EXPORT_SYMBOL(vdec_get_instance);

void vdec_check_instance_available(unsigned int *szfreed)
{
	s32 freed = vrm_check_instance_available();
	*szfreed = freed;
}
EXPORT_SYMBOL(vdec_check_instance_available);

void vdec_clear_instance(int nIdx)
{
	(void) vrm_clear_instance(nIdx);
}
EXPORT_SYMBOL(vdec_clear_instance);

void venc_get_instance(int *nIdx)
{
	s32 idx = *nIdx;
	*nIdx = vrm_get_instance(idx);
}
EXPORT_SYMBOL(venc_get_instance);

void venc_check_instance_available(unsigned int *szfreed)
{
	s32 freed = vrm_check_instance_available();
	*szfreed = freed;
}
EXPORT_SYMBOL(venc_check_instance_available);

void venc_clear_instance(int nIdx)
{
	(void) vrm_clear_instance(nIdx);
}
EXPORT_SYMBOL(venc_clear_instance);

void vmem_set_only_decode_mode(int bDec_only)
{
	return;
}

int _vmem_is_cma_allocated_virt_region(const void *start_virtaddr,
					unsigned int length)
{
	return 0;
}

int vmem_init(void)
{
	return 0;
}

int vmem_config(void)
{
	return 0;
}

void vmem_deinit(void)
{
}

unsigned int vmem_get_freemem_size(vputype type)
{
	return vmem_get_free_memory(type);
}
//*/

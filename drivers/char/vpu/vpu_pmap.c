// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifdef CONFIG_PMAP_DEBUG
#ifndef DEBUG
#  define DEBUG
#endif
#endif

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)

#include "vpu_comm.h"
#include "vpu_pmap.h"

#ifdef CONFIG_PROC_FS
#include <linux/list_sort.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

#ifdef CONFIG_OPTEE
#include <linux/tee_drv.h>
#include <linux/time.h>
#endif

#ifdef CONFIG_PMAP_TO_CMA
#include <linux/dma-mapping.h>
#include <linux/highmem.h>

struct device *pmap_device;
#endif

#define PMAP_FREED		(~((unsigned long)0U))
#define PMAP_DATA_NA		(~((unsigned int)0U))

#define MAX_PMAPS		(64U)
#define MAX_PMAP_GROUPS		(4U)

struct pmap_entry {
	struct pmap info;
	struct pmap_entry *parent;
	struct list_head list;
};

static LIST_HEAD(pmap_list_head);

#if 0 // Keep the code for future use
static bool pmap_list_sorted;
static unsigned long pmap_total_size;
static struct pmap_entry pmap_table[MAX_PMAPS];
static struct pmap_entry secure_area_table[MAX_PMAP_GROUPS];
#endif

static struct pmap *pmap_find_info_by_name(const char *name)
{
	struct pmap_entry *entry = NULL;

	list_for_each_entry(entry, &pmap_list_head, list) {
		int cmp = strncmp(name, entry->info.name, VPU_PMAP_NAME_LEN);

		if (cmp == 0) {
			return &entry->info;
		}
	}

	return NULL;
}

static inline struct pmap_entry *pmap_entry_of(struct pmap *info)
{
	return container_of(info, struct pmap_entry, info);
}

static void vpmap_release_info_internal(struct pmap *info)
{
	struct pmap_entry *entry = pmap_entry_of(info);

	if ((info->rc != 0U) && (entry->parent != NULL)) {
		/* XXX: MISRA-C rule does not allow recursion */
		vpmap_release_info_internal(&entry->parent->info);
	}

	if (info->rc > 0U) {
		--info->rc;
	}
}

int pmap_release_info(const char *name)
{
	struct pmap *info = pmap_find_info_by_name(name);
	if (info == NULL) {
		return 0;
	}

	vpmap_release_info_internal(info);

	return 1;
}
EXPORT_SYMBOL(pmap_release_info);

static int pmap_get_info_internal(
		const char *name,
		struct device_node *np,
		struct pmap *info)
{
	struct device_node *sn;
	const __be32 *prop;
	int len;
	int ret;

	sn = of_parse_phandle(np, "pmap-shared", 0);
	if (sn == NULL) {
		V_DBG(VPU_DBG_PMAP, "no 'pmap-shared' defined");
		return 0;
	}

	if (of_property_read_string(sn, "pmap-name", &name)) {
		V_DBG(VPU_DBG_PMAP, "no 'pmap-name' defined");
		return 0;
	}

	prop = of_get_property(np, "pmap-offset", &len);
	ret = of_n_addr_cells(np);

	if ((prop == NULL) || (ret != (len/(int)sizeof(int)))) {
		V_DBG(VPU_DBG_PMAP, "no 'pmap-offset' defined");
		return 0;
	}

	info->base = of_read_number(prop, of_n_addr_cells(np));

	prop = of_get_property(np, "pmap-shared-size", &len);
	ret = of_n_size_cells(np);

	if ((prop == NULL) || (ret != (len/(int)sizeof(int)))) {
		V_DBG(VPU_DBG_PMAP, "no 'pmap-shared-size' defined");
		return 0;
	}

	info->size = of_read_number(prop, of_n_size_cells(np));

	return 1;
}

int pmap_get_info(const char *name, struct pmap *mem)
{
	struct device_node *np;
	struct reserved_mem *rmem;

	char string[VPU_PMAP_NAME_LEN];

	if (mem == NULL) {
		return 0;
	}

	snprintf(string, sizeof(string), "pmap,%s", name);

	np = of_find_compatible_node(NULL, NULL, string);
	if (np != NULL) {
		struct pmap info;
		int ret;

		rmem = of_reserved_mem_lookup(np);
		if (rmem == NULL) {
 			V_DBG(VPU_DBG_ERROR,
				"failed to find vpu pmap node (%s)!!", name);
			return -1;
		}

		if (of_property_read_string(np, "pmap-name", &name)) {
			V_DBG(VPU_DBG_ERROR, "error");
			return -1;
		}

		ret = pmap_get_info_internal(name, np, &info);
		if (ret) {
			mem->base = info.base;
			mem->size = info.size;
		} else {
			mem->base = rmem->base;
			mem->size = rmem->size;
		}

		V_DBG(VPU_DBG_PMAP,
			"[%s] ret=%d, base=0x%ld, size=%lu",
			rmem->name, ret, mem->base, mem->size);
	}
	return 1;
}
EXPORT_SYMBOL(pmap_get_info);

#endif // for LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)

#if 0 // Keep the code for future use
static inline unsigned long pmap_get_base(struct pmap *info)
{
	struct pmap_entry *entry = pmap_entry_of(info);
	struct pmap_entry *iter = NULL;
	u64 base = 0;

	for (iter = entry; iter != NULL; iter = iter->parent) {
		if (iter->info.base == PMAP_FREED) {
			return 0;
		}

		if ((U64_MAX - base) >= iter->info.base) {
			base += iter->info.base;
		} else {
			/* XXX: Should not happen */
			return 0;
		}
	}

	return base;
}

static int vpmap_get_info_internal(struct pmap *info)
{
	struct pmap_entry *entry = pmap_entry_of(info);

	if (entry->parent != NULL) {
		/* XXX: MISRA-C rule does not allow recursion */
		int ret = vpmap_get_info_internal(&entry->parent->info);

		if (ret == 0) {
			return 0;
		}
	}

	if (info->rc < UINT_MAX) {
		++info->rc;
	}

	return 1;
}

int pmap_get_info(const char *name, struct pmap *mem)
{
	struct pmap *info = pmap_find_info_by_name(name);
	int ret;

	if (mem != NULL) {
		mem->base = 0;
		mem->size = 0;
	}

	if ((mem == NULL) || (info == NULL)) {
		return 0;
	}

	ret = vpmap_get_info_internal(info);

	if (ret == 0) {
		return -1;
	}

	(void)memcpy(mem, info, sizeof(struct pmap));
	mem->base = pmap_get_base(info);
	return 1;
}
EXPORT_SYMBOL(pmap_get_info);

int vpu_pmap_init(void)
{
	struct pmap_entry *entry;
	struct device_node *np;
	struct reserved_mem *rmem;

	static int num_pmaps_left = (int) MAX_PMAPS;
	unsigned int groups = 0;
	unsigned int len = 0;
	unsigned int flags = 0;
	unsigned long base = 0;
	unsigned long size = 0;

	unsigned int i;

	pmap_list_sorted = (bool)false;
	pmap_total_size = (unsigned long)0U;

	printk(KERN_ERR "[%s-%d] enter #0\n", __func__, __LINE__);

#if 0
	/*
	 * Initialize secure area table as 0 and set initial value for
	 * each secure area.
	 */
	(void)memset(secure_area_table, 0, sizeof(secure_area_table));

	for (i = 0; i < MAX_VPMAP_GROUPS; i++) {
		entry = &secure_area_table[i];

		if (entry == NULL) {
			/* XXX: Should not happen */
			return -ENOMEM;
		}

		(void)sprintf(entry->info.name, "secure_area%d", (int)i + 1);
		entry->info.base = PMAP_FREED;
		entry->info.groups = PMAP_DATA_NA;
	}

	printk(KERN_ERR "[%s-%d] #1\n", __func__, __LINE__);

	/*
	 * Register secure area as a parent for each secured vpmap and
	 * calculate base address of each secure area.
	 */

	for (i = 0; i < MAX_VPMAPS; i++) {
		entry = &pmap_table[i];

		if (vpmap_is_secured(&entry->info)) {
			int group_idx = (int)entry->info.groups-1;

			entry->parent = &secure_area_table[group_idx];
			entry->parent->info.flags |= entry->info.flags;

			if ((entry->info.base < entry->parent->info.base) &&
					(entry->info.size != 0U)) {
				entry->parent->info.base = entry->info.base;
			}
		}

		/* Calculate vpmap total size */
		if (!vpmap_is_cma_alloc(&entry->info)) {
			if ((U64_MAX - pmap_total_size) >= entry->info.size) {
				pmap_total_size += entry->info.size;
			} else {
				/* XXX: Should not happen */
			}
		}
	}

	printk(KERN_ERR "[%s-%d] #2\n", __func__, __LINE__);
#endif

	/*
	 * Add reg property for each device tree node if there are only
	 * size property with dynamic allocation.
	 */
	for_each_compatible_node(np, NULL, "telechips,pmap") {
		const char *name;
		struct vpmap *info;
		int ncell;
		int naddr;
		int nsize;
		int ret;
		struct property *prop;
		__be32 *pval;
		const void *reg;

		ret = of_property_read_string(np, "telechips,pmap-name", &name);
		if (ret != 0) {
			continue;
		}

		printk(KERN_ERR "[%s-%d]  #3 - ret=%d, name=%s\n",
			__func__, __LINE__, ret, name);

		naddr = of_n_addr_cells(np);
		nsize = of_n_size_cells(np);
		ncell = naddr + nsize;

		printk(KERN_ERR "[%s-%d]  #4 - addr=0x%lx, size=%d\n",
			__func__, __LINE__, naddr, nsize);

		prop = of_get_property(np, "size", &len);
		ret = of_n_addr_cells(np);
		if ((prop != NULL) && (ret == (len/sizeof(unsigned int)))) {
			size = of_read_number(prop, of_n_size_cells(np));
		}

		printk(KERN_ERR "[%s-%d]  #5 - size=%u\n",
			__func__, __LINE__, size);


		prop = of_get_property(np, "alloc-ranges", &len);
		ret = of_n_addr_cells(np);
		if ((prop != NULL) && (ret == (len/sizeof(unsigned int)))) {
			base = of_read_number(prop, of_n_size_cells(np));
		}

		printk(KERN_ERR "[%s-%d]  #6 - base=0x%lx\n",
			__func__, __LINE__, base);

		rmem = of_reserved_mem_lookup(np);
		printk(KERN_ERR "[%s-%d]  #7 - base=0x%lx, size=%u\n",
			__func__, __LINE__, rmem->base, rmem->size);

		/*
		 * When CONFIG_PMAP_SECURED is not set, simply ignore "telechips,
		 * pmap-secured" property.
		 */
		prop = of_get_property(np, "telechips,pmap-secured", &len);

		if (prop != NULL) {
			groups = of_read_number(prop, len/4);

			if ((groups >= 1U) && (groups <= MAX_VPMAP_GROUPS)) {
				flags |= VPMAP_FLAG_SECURED;
			}
		}

		printk(KERN_ERR "[%s-%d]  #8 - groups=%d\n",
			__func__, __LINE__, groups);

		prop = of_get_property(np, "telechips,pmap-shared", NULL);

		if (prop != NULL) {
			flags |= VPMAP_FLAG_SHARED;
		}

		printk(KERN_ERR "[%s-%d]  #9 - prop=0x%lx\n",
			__func__, __LINE__, prop);

		/* Initialize pmap and register it into pmap list */
		if (num_pmaps_left == 0) {
			return -ENOMEM;
		}

		entry = &pmap_table[(s32)MAX_VPMAPS - num_pmaps_left];
		--num_pmaps_left;

		(void)strncpy(entry->info.name, name, VPU_PMAP_NAME_LEN);
		entry->info.base = base;
		entry->info.size = size;
		entry->info.groups = (unsigned)groups;
		entry->info.rc = 0;
		entry->info.flags = flags;

		list_add_tail(&entry->list, &pmap_list_head);
	}

	/*
	 * Register secure areas with size > 0 into vpmap list to print
	 * out later when user requests.
	 */
	#if 0
	for (i = 0; i < MAX_VPMAP_GROUPS; i++) {
		entry = &secure_area_table[i];

		if (entry->info.size != 0U) {
			list_add_tail(&entry->list, &pmap_list_head);
		}
	}
	#endif

	printk(KERN_ERR "[INFO][PMAP] total reserved %llu bytes (%lluK, %lluM)\n",
		pmap_total_size, pmap_total_size >> 10U,
		pmap_total_size >> 20U);

	return 0;
}
EXPORT_SYMBOL(vpu_pmap_init);
#endif

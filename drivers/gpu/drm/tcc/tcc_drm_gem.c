// SPDX-License-Identifier: GPL-2.0-or-later

/* tcc_drm_gem.c
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Author: Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <drm/drmP.h>
#include <drm/drm_vma_manager.h>

#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include <linux/pfn_t.h>
#include <linux/tcc_math.h>
#include <drm/tcc_drm.h>

#include "tcc_drm_drv.h"
#include "tcc_drm_gem.h"


#if defined(CONFIG_REFCODE_PRE_K54)
static int tcc_drm_gem_prime_attach(struct dma_buf *in_buf,
				struct device *dev,
				struct dma_buf_attachment *attach)
#else
/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tcc_drm_gem_prime_attach(struct dma_buf *in_buf,
				    struct dma_buf_attachment *attach)
#endif
{
	/* DP no.15 */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	const struct drm_gem_object *obj = (struct drm_gem_object *)in_buf->priv;
	int ret = 0;

	/* Restrict access to Rogue */
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
	if (WARN_ON(obj->dev->dev->parent == NULL) ||
	    (obj->dev->dev->parent != attach->dev->parent)) {
		ret = -EPERM;
	}
	return ret;
}

static struct sg_table *
/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
tcc_drm_gem_prime_map_dma_buf(struct dma_buf_attachment *attach,
			      enum dma_data_direction dir)
{
	struct drm_gem_object *obj =
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		(struct drm_gem_object *)attach->dmabuf->priv;
	const struct tcc_drm_gem *tcc_gem =
		(const struct tcc_drm_gem *)to_tcc_gem(obj);
	bool internal_ok = (bool)true;
	bool need_free_sgt = (bool)false;
	struct sg_table *sgt = NULL;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter dir is not used in
	 * the function.
	 */
	(void)dir;

	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (sgt == NULL) {
		internal_ok = (bool)false;
	} else {
		need_free_sgt = (bool)true;
	}

	if (internal_ok) {
		/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		if (sg_alloc_table(sgt, 1, GFP_KERNEL) < 0) {
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		sg_dma_address(sgt->sgl) = tcc_gem->dma_addr;
		sg_dma_len(sgt->sgl) = tcc_safe_ulong2uint(obj->size);
	} else {

		if (need_free_sgt) {
			kfree(sgt);
			sgt = NULL;
		}
	}
	return sgt;
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void tcc_drm_gem_prime_unmap_dma_buf(struct dma_buf_attachment *attach,
					struct sg_table *sgt,
					enum dma_data_direction dir)
{
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter attach is not used
	 * in the function
	 */
	(void)attach;
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter dir is not used
	 * in the function
	 */
	(void)dir;
	sg_free_table(sgt);
	kfree(sgt);
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void *tcc_drm_gem_prime_kmap(struct dma_buf *in_buf,
				unsigned long page_num)
{
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter page_num is not used
	 * in the function
	 */
	(void)in_buf;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter page_num is not used
	 * in the function
	 */
	(void)page_num;

	return NULL;
}

static int tcc_drm_gem_mmap_buffer(const struct tcc_drm_gem *tcc_gem,
				      struct vm_area_struct *vma)
{
	struct drm_device *drm_dev = tcc_gem->base.dev;
	unsigned long vm_size;
	bool internal_ok = (bool)true;
	int ret = 0;
	const unsigned long vm_flags = VM_PFNMAP;

	vma->vm_flags &= ~vm_flags;
	vma->vm_pgoff = 0;

	vm_size = vma->vm_end - vma->vm_start;

	/* check if user-requested size is valid. */
	if (vm_size > tcc_gem->size) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		ret = dma_mmap_attrs(to_dma_dev(drm_dev), vma, tcc_gem->cookie,
				tcc_gem->dma_addr, tcc_gem->size,
				tcc_gem->dma_attrs);
		if (ret < 0) {
			DRM_ERROR("failed to mmap.\n");
		}
	}

	return ret;
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tcc_drm_gem_mmap_obj(struct drm_gem_object *obj,
				   struct vm_area_struct *vma)
{
	const struct tcc_drm_gem *tcc_gem = to_tcc_gem(obj);
	int ret;

	DRM_DEBUG_KMS("flags = 0x%x\n", tcc_gem->flags);

	/* non-cachable as default. */
	if ((tcc_gem->flags & TCC_BO_CACHABLE) == TCC_BO_CACHABLE) {
		vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	} else if ((tcc_gem->flags & TCC_BO_WC) == TCC_BO_WC) {
		vma->vm_page_prot =
			pgprot_writecombine(vm_get_page_prot(vma->vm_flags));
	} else {
		vma->vm_page_prot =
			pgprot_noncached(vm_get_page_prot(vma->vm_flags));
	}

	ret = tcc_drm_gem_mmap_buffer((const struct tcc_drm_gem *)tcc_gem, vma);
	if (ret < 0) {
		drm_gem_vm_close(vma);
	}

	return ret;
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int tcc_drm_gem_prime_mmap(struct dma_buf *in_buf,
				  struct vm_area_struct *vma)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct drm_gem_object *obj = (struct drm_gem_object *)in_buf->priv;
	int ret;

	mutex_lock(&obj->dev->struct_mutex);
	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	if (ret == 0) {
		ret = tcc_drm_gem_mmap_obj(obj, vma);
	}
	mutex_unlock(&obj->dev->struct_mutex);
	return ret;
}


static const struct dma_buf_ops tcc_drm_gem_prime_dmabuf_ops = {
	.attach = tcc_drm_gem_prime_attach,
	.map_dma_buf = tcc_drm_gem_prime_map_dma_buf,
	.unmap_dma_buf = tcc_drm_gem_prime_unmap_dma_buf,
	.release = drm_gem_dmabuf_release,
	#if defined(CONFIG_REFCODE_PRE_K54)
	.map_atomic = tcc_drm_gem_prime_kmap_atomic,
	#else
	.map = tcc_drm_gem_prime_kmap,
	#endif
	.mmap = tcc_drm_gem_prime_mmap,
};

static int tcc_drm_gem_lookup_object(struct drm_file *file_priv, u32 handle,
			  struct drm_gem_object **objp)
{
	struct drm_gem_object *obj;
	bool internal_ok = (bool)true;
	int ret = 0;

	obj = drm_gem_object_lookup(file_priv, handle);
	if (obj == NULL) {
		internal_ok = (bool)false;
		ret = -ENOENT;
	}

	if (internal_ok &&
	   (obj->import_attach != NULL)) {
		/*
		 * The dmabuf associated with the object is not one of ours.
		 * Our own buffers are handled differently on import.
		 */
		drm_gem_object_put_unlocked(obj);
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		*objp = obj;
	}
	return ret;
}

struct tcc_drm_gem *to_tcc_gem(struct drm_gem_object *obj)
{
	/* coverity[cert_arr39_c_violation : FALSE] */
	/* coverity[cert_dcl37_c_violation : FALSE] */
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
	return (struct tcc_drm_gem *)container_of(obj, struct tcc_drm_gem, base);
}

#if defined(CONFIG_REFCODE_PRE_K54)
struct dma_buf *tcc_drm_gem_prime_export(
				     struct drm_device *dev,
				     struct drm_gem_object *obj,
				     int flags)
#else
struct dma_buf *tcc_drm_gem_prime_export(
				     struct drm_gem_object *obj,
				     int flags)
#endif
{
	const struct tcc_drm_gem *tcc_gem = to_tcc_gem(obj);

	DEFINE_DMA_BUF_EXPORT_INFO(export_info);

	export_info.ops = &tcc_drm_gem_prime_dmabuf_ops;
	export_info.size = obj->size;
	export_info.flags = flags;
	export_info.resv = tcc_gem->resv;
	export_info.priv = obj;

	#if defined(CONFIG_REFCODE_PRE_K54)
	return drm_gem_dmabuf_export(dev, &export_info);
	#else
	return drm_gem_dmabuf_export(obj->dev, &export_info);
	#endif
}

struct drm_gem_object *tcc_drm_gem_prime_import(
		struct drm_device *dev, struct dma_buf *in_buf)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct drm_gem_object *obj = (struct drm_gem_object *)in_buf->priv;

	if (obj->dev == dev) {
		/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
		/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
		BUG_ON(in_buf->ops != &tcc_drm_gem_prime_dmabuf_ops);
		/*
		 * The dmabuf is one of ours, so return the associated
		 * TCC GEM object, rather than create a new one.
		 */
		drm_gem_object_get(obj);
	} else {
		obj = drm_gem_prime_import_dev(dev, in_buf, to_dma_dev(dev));
	}
	return obj;
}
/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
struct dma_resv *tcc_gem_prime_res_obj(struct drm_gem_object *obj)
{
	const struct tcc_drm_gem *tcc_gem = to_tcc_gem(obj);

	return tcc_gem->resv;
}

static int tcc_drm_alloc_buf(struct tcc_drm_gem *tcc_gem)
{
	struct drm_device *dev = tcc_gem->base.dev;
	unsigned long attr;
	unsigned int nr_pages;
	struct sg_table sgt;
	bool internal_ok = (bool)true;
	bool need_free = (bool)false;
	bool need_dma_free = (bool)false;
	bool need_sgt_free = (bool)false;
	int ret = 0;

	if (tcc_gem->dma_addr != 0UL) {
		DRM_DEBUG_KMS("already allocated.\n");
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		tcc_gem->dma_attrs = DMA_ATTR_NO_KERNEL_MAPPING;

		/*
		* if TCC_BO_CONTIG, fully physically contiguous memory
		* region will be allocated else physically contiguous
		* as possible.
		*/
		if (((tcc_gem->flags & TCC_BO_NONCONTIG) != TCC_BO_NONCONTIG)) {
			tcc_gem->dma_attrs |= DMA_ATTR_FORCE_CONTIGUOUS;
		}

		/*
		* if TCC_BO_WC or TCC_BO_NONCACHABLE, writecombine mapping
		* else cachable mapping.
		*/
		if (((tcc_gem->flags & TCC_BO_WC) == TCC_BO_WC) ||
		   ((tcc_gem->flags & TCC_BO_CACHABLE) != TCC_BO_CACHABLE)) {
			attr = DMA_ATTR_WRITE_COMBINE;
		} else {
			attr = DMA_ATTR_NON_CONSISTENT;
		}

		tcc_gem->dma_attrs |= attr;

		nr_pages = tcc_safe_ulong2uint(tcc_gem->size >> PAGE_SHIFT);

		tcc_gem->pages =
			/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
			(struct page **)kvmalloc_array(nr_pages,
						      sizeof(struct page *),
			/* DP no.13 */
			/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
			/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
							GFP_KERNEL | __GFP_ZERO);
		if (tcc_gem->pages == NULL) {
			DRM_ERROR("failed to allocate pages.\n");
			internal_ok = (bool)false;
			ret = -ENOMEM;
		} else {
			need_free = (bool)true;
		}
	}
	if (internal_ok) {
		tcc_gem->cookie =
			dma_alloc_attrs(to_dma_dev(dev), tcc_gem->size,
				/* DP no.13 */
				/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
				/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
				&tcc_gem->dma_addr, GFP_KERNEL,
				tcc_gem->dma_attrs);
		if (tcc_gem->cookie == NULL) {
			DRM_ERROR("failed to allocate buffer.\n");
			internal_ok = (bool)false;
			ret = -ENOMEM;
		} else {
			need_dma_free = (bool)true;
		}
	}
	if (internal_ok) {
		ret = dma_get_sgtable_attrs(to_dma_dev(dev), &sgt, tcc_gem->cookie,
					tcc_gem->dma_addr, tcc_gem->size,
					tcc_gem->dma_attrs);
		if (ret < 0) {
			DRM_ERROR("failed to get sgtable.\n");
			internal_ok = (bool)false;
			ret = -ENOMEM;
		} else {
			need_sgt_free = (bool)true;
		}
	}
	if (internal_ok) {
		if (drm_prime_sg_to_page_addr_arrays(&sgt, tcc_gem->pages, NULL,
						tcc_safe_uint2int(nr_pages)) != 0) {
			DRM_ERROR("invalid sgtable.\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		sg_free_table(&sgt);

		DRM_DEBUG_KMS("dma_addr(0x%lx), size(0x%lx)\n",
				(unsigned long)tcc_gem->dma_addr, tcc_gem->size);
	} else {
		/* Error process */
		if (need_sgt_free) {
			sg_free_table(&sgt);
		}
		if (need_dma_free) {
			dma_free_attrs(to_dma_dev(dev), tcc_gem->size, tcc_gem->cookie,
				tcc_gem->dma_addr, tcc_gem->dma_attrs);
		}
		if (need_free) {
			kvfree(tcc_gem->pages);
		}
	}
	return ret;
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static void tcc_drm_free_buf(struct tcc_drm_gem *tcc_gem)
{
	struct drm_device *dev = tcc_gem->base.dev;

	if (tcc_gem->dma_addr == 0UL) {
		DRM_DEBUG_KMS("dma_addr is invalid.\n");
	} else {
		DRM_DEBUG_KMS("dma_addr(0x%lx), size(0x%lx)\n",
				(unsigned long)tcc_gem->dma_addr, tcc_gem->size);

		if (tcc_gem->cookie != NULL) {
			dma_free_attrs(to_dma_dev(dev), tcc_gem->size,
				       tcc_gem->cookie,
				       (dma_addr_t)tcc_gem->dma_addr,
				       tcc_gem->dma_attrs);
		}
		tcc_gem->cookie = NULL;

		if (tcc_gem->pages != NULL) {
			kvfree(tcc_gem->pages);
		}
		tcc_gem->pages = NULL;
	}
}

static int tcc_drm_gem_handle_create(struct drm_gem_object *obj,
					struct drm_file *file_priv,
					unsigned int *handle)
{
	int ret = 0;

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv, obj, handle);
	if (ret == 0) {
		DRM_DEBUG_KMS("gem handle = 0x%x\n", *handle);

		/* drop reference from allocate - handle holds it now. */
		#if defined(CONFIG_REFCODE_PRE_K54)
		drm_gem_object_unreference_unlocked(obj);
		#else
		drm_gem_object_put_unlocked(obj);
		#endif
	}

	return ret;
}

void tcc_drm_gem_destroy(struct tcc_drm_gem *tcc_gem)
{
	struct drm_gem_object *obj;

	if (tcc_gem != NULL) {
		obj = &tcc_gem->base;

		DRM_DEBUG_KMS("handle count = %d\n", obj->handle_count);
		#if defined(CONFIG_REFCODE_PRE_K54)
		if (&tcc_gem->_resv == tcc_gem->resv) {
			dma_resv_fini(&tcc_gem->_resv);
		}
		#endif

		/*
		* do not release memory region from exporter.
		*
		* the region will be released by exporter
		* once dmabuf's refcount becomes 0.
		*/
		if (obj->import_attach != NULL) {
			drm_prime_gem_destroy(obj, tcc_gem->sgt);
		} else {
			tcc_drm_free_buf(tcc_gem);
		}

		/* release file pointer to gem object. */
		drm_gem_object_release(obj);

		kfree(tcc_gem);
	}
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
unsigned long tcc_drm_gem_get_size(struct drm_device *dev,
						unsigned int gem_handle,
						struct drm_file *file_priv)
{
	const struct tcc_drm_gem *tcc_gem;
	struct drm_gem_object *obj;
	unsigned long size = 0UL;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter dev is not used
	 * in the function
	 */
	(void)dev;

	obj = drm_gem_object_lookup(file_priv, gem_handle);
	if (obj != NULL) {
		tcc_gem = to_tcc_gem(obj);

		#if defined(CONFIG_REFCODE_PRE_K54)
		drm_gem_object_unreference_unlocked(obj);
		#else
		drm_gem_object_put_unlocked(obj);
		#endif
		size = tcc_gem->size;
	} else {
		DRM_ERROR("failed to lookup gem object.\n");
	}
	return size;
}

static struct tcc_drm_gem *tcc_drm_gem_init(
		struct drm_device *dev, struct dma_resv *resv,
		unsigned long size)
{
	struct tcc_drm_gem *tcc_gem;
	struct drm_gem_object *obj;
	bool internal_ok = (bool)true;
	int ret;

	/* DP no.13 */
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	tcc_gem = (struct tcc_drm_gem *)kzalloc(sizeof(*tcc_gem), GFP_KERNEL);
	if (tcc_gem == NULL) {
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		tcc_gem->size = size;
		obj = &tcc_gem->base;

		#if defined(CONFIG_REFCODE_PRE_K54)
		if (resv == NULL) {
			dma_resv_init(&tcc_gem->_resv);
		}
		tcc_gem->resv = &tcc_gem->_resv;
		#else
		tcc_gem->base.resv = resv;
		#endif
		ret = drm_gem_object_init(dev, obj, size);
		if (ret < 0) {
			DRM_ERROR("failed to initialize gem object\n");
			kfree(tcc_gem);
			internal_ok = (bool)false;
			tcc_gem = NULL;
		}
	}

	if (internal_ok) {
		#if defined(CONFIG_REFCODE_PRE_K54)
		#else
		tcc_gem->resv = tcc_gem->base.resv;
		#endif
		ret = drm_gem_create_mmap_offset(obj);
		if (ret < 0) {
			#if defined(CONFIG_REFCODE_PRE_K54)
			if (&tcc_gem->_resv == tcc_gem->resv)
				dma_resv_fini(&tcc_gem->_resv);
			#endif
			drm_gem_object_release(obj);
			kfree(tcc_gem);
			tcc_gem = NULL;
		}
	}

	return tcc_gem;
}

struct tcc_drm_gem *tcc_drm_gem_create(struct drm_device *dev,
					     unsigned int flags,
					     unsigned long size)
{
	struct tcc_drm_gem *tcc_gem = NULL;
	bool internal_ok = (bool)true;
	int ret;

	if ((flags & ~(TCC_BO_MASK)) != 0U) {
		DRM_ERROR("invalid GEM buffer flags: %u\n", flags);
		internal_ok = (bool)false;
	}

	if (internal_ok && (size == 0UL)) {
		DRM_ERROR("invalid GEM buffer size: %lu\n", size);
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		size = roundup(size, PAGE_SIZE);

		tcc_gem = tcc_drm_gem_init(dev, NULL, size);
		if (tcc_gem == NULL) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		if ((flags & TCC_BO_NONCONTIG) == TCC_BO_NONCONTIG) {
			/*
			* when no IOMMU is available, all allocated buffers are
			* contiguous anyway, so drop TCC_BO_NONCONTIG flag
			*/
			flags &= ~TCC_BO_NONCONTIG;
			/* coverity[misra_c_2012_rule_17_7_violation : FALSE] */
			DRM_WARN(
				"Non-contiguous allocation is not supported without IOMMU, falling back to contiguous buffer\n");
		}

		/* set memory type and cache attribute from user side. */
		tcc_gem->flags = flags;

		ret = tcc_drm_alloc_buf(tcc_gem);
		if (ret < 0) {
			drm_gem_object_release(&tcc_gem->base);
			kfree(tcc_gem);
			tcc_gem = NULL;
		}
	}
	return tcc_gem;
}

int tcc_drm_gem_create_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	/* DP no.15 */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct drm_tcc_gem_create *args = data;
	struct tcc_drm_gem *tcc_gem;
	bool internal_ok = (bool)true;
	int ret = 0;

	tcc_gem = tcc_drm_gem_create(dev, args->flags, args->size);
	if (tcc_gem == NULL) {
		internal_ok = (bool)false;
		ret = -ENOMEM;
	}

	if (internal_ok) {
		ret = tcc_drm_gem_handle_create(&tcc_gem->base, file_priv,
					&args->handle);
		if (ret != 0) {
			tcc_drm_gem_destroy(tcc_gem);
		}
	}

	return ret;
}

int tcc_drm_gem_map_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv)
{
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct drm_tcc_gem_map *args = (struct drm_tcc_gem_map *)data;

	return drm_gem_dumb_map_offset(file_priv, dev, args->handle,
				       &args->offset);
}


/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
int tcc_drm_gem_get_ioctl(struct drm_device *dev, void *data,
				      struct drm_file *file_priv)
{
	const struct tcc_drm_gem *tcc_gem;
	/* DP no.15 */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct drm_tcc_gem_info *args = (struct drm_tcc_gem_info *)data;
	struct drm_gem_object *obj;
	int ret = 0;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter dev is not used in
	 * the function.
	 */
	(void)dev;

	obj = drm_gem_object_lookup(file_priv, args->handle);
	if (obj == NULL) {
		DRM_ERROR("failed to lookup gem object.\n");
		ret = -EINVAL;
	} else {
		tcc_gem = (const struct tcc_drm_gem *)to_tcc_gem(obj);

		args->flags = tcc_gem->flags;
		args->size = tcc_gem->size;

		#if defined(CONFIG_REFCODE_PRE_K54)
		drm_gem_object_unreference_unlocked(obj);
		#else
		drm_gem_object_put_unlocked(obj);
		#endif
	}

	return ret;
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
int tcc_gem_cpu_prep_ioctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	const struct drm_tcc_gem_cpu_prep *args =
		/* DP no.15 */
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		(struct drm_tcc_gem_cpu_prep *)data;
	struct drm_gem_object *obj;
	struct tcc_drm_gem *tcc_gem;
	bool internal_ok = (bool)true;
	bool need_unlock = (bool)false;
	bool need_unref = (bool)false;
	bool arg_write, arg_wait;
	int err = 0;

	arg_write = ((args->flags & TCC_GEM_CPU_PREP_WRITE) ==
		TCC_GEM_CPU_PREP_WRITE) ? (bool)true : (bool)false;
	arg_wait = ((args->flags & TCC_GEM_CPU_PREP_NOWAIT) !=
		TCC_GEM_CPU_PREP_NOWAIT) ? (bool)true : (bool)false;

	if ((args->flags & ~TCC_GEM_CPU_PREP_FLAGS)  != 0U) {
		DRM_ERROR("invalid flags: %#08x\n", args->flags);
		internal_ok = (bool)false;
		err = -EINVAL;
	}

	if (internal_ok) {
		mutex_lock(&dev->struct_mutex);
		need_unlock = (bool)true;
		err = tcc_drm_gem_lookup_object(file_priv, args->handle, &obj);
		if (err < 0) {
			internal_ok = (bool)false;
		} else {
			need_unref = (bool)true;
		}
	}

	if (internal_ok) {
		tcc_gem = to_tcc_gem(obj);
		if (tcc_gem->cpu_prep) {
			internal_ok = (bool)false;
			err = -EBUSY;
		}
	}

	if (internal_ok) {
		#if defined(CONFIG_REFCODE_PRE_K54)
		#else
		if (tcc_gem->resv == NULL) {
			dev_err(dev->dev, "[ERROR][GEM] tcc_gem->resv is NULL\r\n");
			internal_ok = (bool)false;
			err = -EINVAL;
		}
		#endif
	}

	if (internal_ok) {
		if (arg_wait) {
			long lerr;

			lerr = dma_resv_wait_timeout_rcu(tcc_gem->resv,
							arg_write,
							(bool)true,
							30 * HZ);
			if (lerr <= 0L) {
				internal_ok = (bool)false;
				err = -EBUSY;
			}
		} else {
			if (!dma_resv_test_signaled_rcu(tcc_gem->resv,
							arg_write)) {
				internal_ok = (bool)false;
				err = -EBUSY;
			}
		}
	}

	if (internal_ok) {
		tcc_gem->cpu_prep = (bool)true;
	}
	if (need_unref) {
		drm_gem_object_put_unlocked(obj);
	}
	if (need_unlock) {
		mutex_unlock(&dev->struct_mutex);
	}
	return err;
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
int tcc_gem_cpu_fini_ioctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	const struct drm_tcc_gem_cpu_prep *args =
		/* DP no.15 */
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		(const struct drm_tcc_gem_cpu_prep *)data;
	bool internal_ok = (bool)true;
	bool need_unref = (bool)false;
	struct drm_gem_object *obj;
	struct tcc_drm_gem *tcc_gem;
	int err = 0;

	mutex_lock(&dev->struct_mutex);
	err = tcc_drm_gem_lookup_object(file_priv, args->handle, &obj);
	if (err < 0) {
		internal_ok = (bool)false;
	} else {
		need_unref = (bool)true;
	}
	if (internal_ok) {
		tcc_gem = to_tcc_gem(obj);
		if (!tcc_gem->cpu_prep) {
			internal_ok = (bool)false;
			err = -EINVAL;
		}
	}
	if (internal_ok) {
		tcc_gem->cpu_prep = (bool)false;
	}

	if (need_unref) {
		drm_gem_object_put_unlocked(obj);
	}
	mutex_unlock(&dev->struct_mutex);

	return err;
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void tcc_drm_gem_free_object(struct drm_gem_object *obj)
{
	tcc_drm_gem_destroy(to_tcc_gem(obj));
}

int tcc_drm_gem_dumb_create(struct drm_file *file_priv,
			       struct drm_device *dev,
			       struct drm_mode_create_dumb *args)
{
	struct tcc_drm_gem *tcc_gem;
	unsigned long pitch, height;
	unsigned int flags;
	bool internal_ok = (bool)true;
	int ret = 0;

	/*
	 * allocate memory to be used for framebuffer.
	 * - this callback would be called by user application
	 *	with DRM_IOCTL_MODE_CREATE_DUMB command.
	 */
	args->pitch = ALIGN(args->width * DIV_ROUND_UP(args->bpp, 8), 8);

	pitch = (unsigned long)args->pitch;
	height = (unsigned long)args->height;
	args->size = tcc_safe_ulong_mul(pitch, height);

	/*
	 * Default flags are contiguous and write combine
	 * for preformance.
	 */
	flags = TCC_BO_CONTIG | TCC_BO_WC;

	tcc_gem = tcc_drm_gem_create(dev, flags, args->size);
	if (tcc_gem == NULL) {
		dev_warn(dev->dev, "FB allocation failed.\n");
		internal_ok = (bool)false;
		ret = -ENOMEM;
	}

	if (internal_ok) {
		ret = tcc_drm_gem_handle_create(&tcc_gem->base, file_priv,
						&args->handle);
		if (ret != 0) {
			tcc_drm_gem_destroy(tcc_gem);
		}
	}

	return ret;
}

#if defined(CONFIG_REFCODE_PRE_K54)
int tcc_drm_gem_fault(struct vm_fault *vmf)
#else
/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
vm_fault_t tcc_drm_gem_fault(struct vm_fault *vmf)
#endif
{
	struct vm_area_struct *vma = vmf->vma;
	/* DP no.15 */
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct drm_gem_object *obj = vma->vm_private_data;
	const struct tcc_drm_gem *tcc_gem = to_tcc_gem(obj);
	bool internal_ok = (bool)true;
	unsigned long pfn;
	pgoff_t page_offsets;
	#if defined(CONFIG_REFCODE_PRE_K54)
	int ret;
	#else
	vm_fault_t ret;
	#endif

	if (vmf->address < vma->vm_start) {
		internal_ok = (bool)false;
		ret = (vm_fault_t)VM_FAULT_SIGBUS;
	}

	if (internal_ok) {
		page_offsets = (vmf->address - vma->vm_start) >> PAGE_SHIFT;

		if (page_offsets >= (tcc_gem->size >> PAGE_SHIFT)) {
			DRM_ERROR("invalid page offset\n");
			internal_ok = (bool)false;
			#if defined(CONFIG_REFCODE_PRE_K54)
			ret = -EINVAL;
			#else
			ret = (vm_fault_t)VM_FAULT_SIGBUS;
			#endif
		}
	}

	if (internal_ok) {
		/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
		/* coverity[cert_int31_c_violation : FALSE] */
		/* coverity[cert_int02_c_violation : FALSE] */
		pfn = page_to_pfn(tcc_gem->pages[page_offsets]);
		#if defined(CONFIG_REFCODE_PRE_K54)
		ret = vm_insert_mixed(
			vma, vmf->address, __pfn_to_pfn_t(pfn, PFN_DEV));
		#else
		ret = vmf_insert_mixed(
			vma, vmf->address, __pfn_to_pfn_t(pfn, PFN_DEV));
		#endif
	}

	#if defined(CONFIG_REFCODE_PRE_K54)
	switch (ret) {
	case 0:
	case -ERESTARTSYS:
	case -EINTR:
		ret = VM_FAULT_NOPAGE;
		break;
	case -ENOMEM:
		ret = VM_FAULT_OOM;
		break;
	default:
		ret = VM_FAULT_SIGBUS;
		break;
	}
	#endif
	return ret;
}

int tcc_drm_gem_mmap(struct file *filp,
			struct vm_area_struct *vma)
{
	struct drm_gem_object *obj;
	int ret;

	/* set vm_area_struct. */
	ret = drm_gem_mmap(filp, vma);
	if (ret < 0) {
		DRM_ERROR("failed to mmap.\n");
	} else {
		/* DP no.15 */
		/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
		obj = (struct drm_gem_object *)vma->vm_private_data;

		if (obj->import_attach != NULL) {
			ret = dma_buf_mmap(obj->dma_buf, vma, 0);
		} else {
			ret = tcc_drm_gem_mmap_obj(obj, vma);
		}
	}
	return ret;
}

/* low-level interface prime helpers */
/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
struct sg_table *tcc_drm_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	const struct tcc_drm_gem *tcc_gem = to_tcc_gem(obj);
	unsigned int npages;

	npages = tcc_safe_ulong2uint(tcc_gem->size >> (size_t)PAGE_SHIFT);

	return drm_prime_pages_to_sg(tcc_gem->pages, npages);
}

struct drm_gem_object *
tcc_drm_gem_prime_import_sg_table(struct drm_device *dev,
				/* DP no.6 */
				/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
				     struct dma_buf_attachment *attach,
				     struct sg_table *sgt)
{
	struct tcc_drm_gem *tcc_gem;
	/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
	struct drm_gem_object *obj = (struct drm_gem_object *)ERR_PTR(-EINVAL);
	size_t npages;
	int ret;
	bool internal_ok = (bool)true;
	bool need_free_large = (bool)false;
	bool need_release = (bool)false;

	tcc_gem = tcc_drm_gem_init(
			dev, attach->dmabuf->resv, attach->dmabuf->size);
	if (tcc_gem == NULL) {
		internal_ok = (bool)false;
	} else {
		need_release = (bool)true;
	}

	if (internal_ok) {
		tcc_gem->sgt = sgt;
		if (tcc_gem->sgt->nents != 1U) {
			internal_ok = (bool)false;
			/* coverity[misra_c_2012_rule_17_7_violation : FALSE] */
			pr_err("[ERR][DRMCRTC] %s nents is %d - Not continuous memory!!\r\n",
				__func__, tcc_gem->sgt->nents);
		}
	}
	if (internal_ok) {
		tcc_gem->dma_addr = sg_dma_address(sgt->sgl);

		npages = tcc_gem->size >> (size_t)PAGE_SHIFT;
		tcc_gem->pages =
			/* DP no.13 */
			/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
			/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
			kvmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
		if (tcc_gem->pages == NULL) {
			internal_ok = (bool)false;
			/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
			obj = (struct drm_gem_object *)ERR_PTR(-ENOMEM);
		} else  {
			need_free_large = (bool)true;
		}
	}
	if (internal_ok) {
		unsigned int tmp_pages_0 = tcc_safe_ulong2uint(npages);
		int tmp_pages_1 = tcc_safe_uint2int(tmp_pages_0);

		ret = drm_prime_sg_to_page_addr_arrays(sgt, tcc_gem->pages, NULL,
						tmp_pages_1);
		if (ret < 0) {
			internal_ok = (bool)false;
			/* coverity[misra_c_2012_rule_11_5_violation : FALSE] */
			obj = (struct drm_gem_object *)ERR_PTR(ret);
		}
	}
	if (internal_ok) {
		tcc_gem->sgt = sgt;

		if (sgt->nents != 1U) {
			/*
			* this case could be CONTIG or NONCONTIG type but for now
			* sets NONCONTIG.
			* TODO. we have to find a way that exporter can notify
			* the type of its own buffer to importer.
			*/
			tcc_gem->flags |= TCC_BO_NONCONTIG;
		}
		obj = &tcc_gem->base;
	}
	if (!internal_ok) {
		if (need_free_large) {
			kvfree(tcc_gem->pages);
		}
		if (need_release) {
			drm_gem_object_release(&tcc_gem->base);
			kfree(tcc_gem);
		}
	}
	return obj;
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void *tcc_drm_gem_prime_vmap(struct drm_gem_object *obj)
{
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter obj is not used
	 * in the function
	 */
	(void)obj;

	return NULL;
}

/* DP no.6 */
/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
void tcc_drm_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr)
{
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter obj is not used
	 * in the function
	 */
	(void)obj;
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter vaddr is not used
	 * in the function
	 */
	(void)vaddr;
}


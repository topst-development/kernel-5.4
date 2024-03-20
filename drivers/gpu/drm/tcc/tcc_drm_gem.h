/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * tcc_drm_gem.h
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authoer: Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef TCC_DRM_GEM_HEADER
#define TCC_DRM_GEM_HEADER
#include <linux/version.h>
#include <linux/dma-buf.h>
#include <drm/drm_gem.h>

#define IS_NONCONTIG_BUFFER(f)		(f & TCC_BO_NONCONTIG)

#include <linux/dma-resv.h>

/*
 * tcc drm buffer structure.
 *
 * @base: a gem object.
 *	- a new handle to this gem object would be created
 *	by drm_gem_handle_create().
 * @buffer: a pointer to tcc_drm_gem_buffer object.
 *	- contain the information to memory region allocated
 *	by user request or at framebuffer creation.
 *	continuous memory region allocated by user request
 *	or at framebuffer creation.
 * @flags: indicate memory type to allocated buffer and cache attruibute.
 * @size: size requested from user, in bytes and this size is aligned
 *	in page unit.
 * @cookie: cookie returned by dma_alloc_attrs
 * @kvaddr: kernel virtual address to allocated memory region.
 * @dma_addr: bus address(accessed by dma) to allocated memory region.
 *	- this address could be physical address without IOMMU and
 *	device address with IOMMU.
 * @pages: Array of backing pages.
 * @sgt: Imported sg_table.
 *
 * P.S. this object would be transferred to user as kms_bo.handle so
 *	user can access the buffer through kms_bo.handle.
 */
struct tcc_drm_gem {
	struct drm_gem_object	base;
	unsigned int		flags;
	unsigned long		size;
	void			*cookie;
	void __iomem		*kvaddr;
	dma_addr_t		dma_addr;
	unsigned long		dma_attrs;
	struct page		**pages;
	struct sg_table		*sgt;

	struct dma_resv *resv;

	bool cpu_prep;
};

struct tcc_drm_gem *to_tcc_gem(struct drm_gem_object *obj);

struct dma_resv *tcc_gem_prime_res_obj(struct drm_gem_object *obj);

/* destroy a buffer with gem object */
void tcc_drm_gem_destroy(struct tcc_drm_gem *tcc_gem);

struct dma_buf *tcc_drm_gem_prime_export(
		struct drm_gem_object *obj,
		int flags);

struct drm_gem_object *tcc_drm_gem_prime_import(
	struct drm_device *dev,
	struct dma_buf *in_buf);

/* create a new buffer with gem object */
struct tcc_drm_gem *tcc_drm_gem_create(struct drm_device *dev,
					     unsigned int flags,
					     unsigned long size);

/*
 * request gem object creation and buffer allocation as the size
 * that it is calculated with framebuffer information such as width,
 * height and bpp.
 */
int tcc_drm_gem_create_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);

/* get fake-offset of gem object that can be used with mmap. */
int tcc_drm_gem_map_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv);

/* get buffer information to memory region allocated by gem. */
int tcc_drm_gem_get_ioctl(struct drm_device *dev, void *data,
				      struct drm_file *file_priv);

int tcc_gem_cpu_prep_ioctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv);

int tcc_gem_cpu_fini_ioctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv);

/* get buffer size to gem handle. */
unsigned long tcc_drm_gem_get_size(struct drm_device *dev,
						unsigned int gem_handle,
						struct drm_file *file_priv);

/* free gem object. */
void tcc_drm_gem_free_object(struct drm_gem_object *obj);

/* create memory region for drm framebuffer. */
int tcc_drm_gem_dumb_create(struct drm_file *file_priv,
			       struct drm_device *dev,
			       struct drm_mode_create_dumb *args);

/* page fault handler and mmap fault address(virtual) to physical memory. */
vm_fault_t tcc_drm_gem_fault(struct vm_fault *vmf);

/* set vm_flags and we can change the vm attribute to other one at here. */
int tcc_drm_gem_mmap(struct file *filp,
				struct vm_area_struct *vma);

/* low-level interface prime helpers */
struct sg_table *tcc_drm_gem_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *
tcc_drm_gem_prime_import_sg_table(struct drm_device *dev,
				     struct dma_buf_attachment *attach,
				     struct sg_table *sgt);
void *tcc_drm_gem_prime_vmap(struct drm_gem_object *obj);
void tcc_drm_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);
#endif

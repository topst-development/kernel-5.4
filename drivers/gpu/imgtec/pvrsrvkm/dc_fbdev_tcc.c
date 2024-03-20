/*************************************************************************/ /*!
@File
@Copyright      Copyright (c) Telechips Inc.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/console.h>
#include <linux/module.h>
#include <linux/fb.h>

#include "kerneldisplay.h"
#include "imgpixfmts.h"
#include "pvrmodule.h" /* for MODULE_LICENSE() */
#if defined(CONFIG_VIOC_PVRIC_FBDC)
#include "di_server.h"
#include <video/tcc/vioc_pvric_fbdc.h>
#endif
#if !defined(CONFIG_FB)
#error dc_fbdev needs Linux framebuffer support. Enable it in your kernel.
#endif

#define DRVNAME					"dc_fbdev"
#define MAX_COMMANDS_IN_FLIGHT	2

#if defined(DC_FBDEV_NUM_PREFERRED_BUFFERS)
#define NUM_PREFERRED_BUFFERS	DC_FBDEV_NUM_PREFERRED_BUFFERS
#else
#define NUM_PREFERRED_BUFFERS	2
#endif

#define FALLBACK_REFRESH_RATE		60
#define FALLBACK_DPI				160

#if defined(CONFIG_VIOC_PVRIC_FBDC)
IMG_UINT32 gPVRPreEnableFBDC;
IMG_UINT32 gPVREnableFBDC;
module_param(gPVREnableFBDC, uint, 0644);
MODULE_PARM_DESC(gPVREnableFBDC, "Sets enableFBDC");

static DI_ENTRY *gpsEnableFBDCEntry;
#endif

typedef struct
{
	IMG_HANDLE			hSrvHandle;
	IMG_UINT32			ePixFormat;
	struct fb_info		*psLINFBInfo;
	bool				bCanFlip;
}
DC_FBDEV_DEVICE;

typedef struct
{
	DC_FBDEV_DEVICE		*psDeviceData;
	IMG_HANDLE			hLastConfigData;
	IMG_UINT32		ui32AllocUseMask;
}
DC_FBDEV_CONTEXT;

typedef struct
{
	DC_FBDEV_CONTEXT	*psDeviceContext;
	IMG_UINT32			ui32Width;
	IMG_UINT32			ui32Height;
	IMG_UINT32			ui32ByteStride;
	IMG_UINT32			ui32BufferID;
	IMG_BOOL			bFBComp;
	IMG_UINT32          ui32FBCBufSize;
}
DC_FBDEV_BUFFER;

static DC_FBDEV_DEVICE *gpsDeviceData[FB_MAX];

#if defined(CONFIG_VIOC_PVRIC_FBDC)
static void *EnableFBDCStart(OSDI_IMPL_ENTRY *psEntry, IMG_UINT64 *pui64Pos)
{
	if (*pui64Pos == 0)
		return DIGetPrivData(psEntry);

	return NULL;
}

static void EnableFBDCStop(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psEntry);
	PVR_UNREFERENCED_PARAMETER(pvData);
}

static void *EnableFBDCNext(OSDI_IMPL_ENTRY *psEntry,
					void *pvData,
					IMG_UINT64 *pui64Pos)
{
	PVR_UNREFERENCED_PARAMETER(psEntry);
	PVR_UNREFERENCED_PARAMETER(pvData);
	PVR_UNREFERENCED_PARAMETER(pui64Pos);

	return NULL;
}

static int EnableFBDCShow(OSDI_IMPL_ENTRY *psEntry, void *pvData)
{
	if (pvData != NULL) {
		IMG_UINT32 uiEnableFBDC = *((IMG_UINT32 *)pvData);

		DIPrintf(psEntry, "%u\n", uiEnableFBDC);

		return 0;
	}

	return -EINVAL;
}

static IMG_INT64 EnableFBDCSet(const IMG_CHAR *pcBuffer, IMG_UINT64 ui64Count,
				IMG_UINT64 *pui64Pos, void *pvData)
{
	IMG_UINT32 *uiEnableFBDC = pvData;

	if (pcBuffer == NULL)
		return -EIO;
	if (pui64Pos == NULL)
		return -EIO;

	if (sscanf(pcBuffer, "%u", &gPVREnableFBDC) == 0)
		return -EINVAL;

	/* As this is Linux the next line uses a GCC builtin function */
	(*uiEnableFBDC) &= (1 << __builtin_ffsl(0x100UL)) - 1;

	*pui64Pos += ui64Count;
	return ui64Count;
}

void PVRTestRemoveDIEntries(void)
{
	if (gpsEnableFBDCEntry != NULL)
		DIDestroyEntry(gpsEnableFBDCEntry);
}

int PVRTestCreateDIEntries(void)
{
	PVRSRV_ERROR eError;

	DI_ITERATOR_CB sIterator = {
		.pfnStart = EnableFBDCStart,
		.pfnStop = EnableFBDCStop,
		.pfnNext = EnableFBDCNext,
		.pfnShow = EnableFBDCShow,
		.pfnWrite = EnableFBDCSet
	};

	eError = DICreateEntry("enable_FBDC", NULL, &sIterator, &gPVREnableFBDC,
				DI_ENTRY_TYPE_GENERIC, &gpsEnableFBDCEntry);

	if (eError != PVRSRV_OK) {
		pr_err("%s DICreateEntry error\n", __func__);
		PVRTestRemoveDIEntries();
		return -EFAULT;
	}

	return 0;
}
#endif



#if defined(DC_FBDEV_FORCE_CONTEXT_CLEAN)
static
void DC_FBDEV_Clean(IMG_HANDLE hDisplayContext, IMG_HANDLE *ahBuffers)
{
	DC_FBDEV_DEVICE *psDeviceData;
	DC_FBDEV_BUFFER *psBuffer;
	IMG_CPU_PHYADDR sCPUPhysStart;
	void *pvOSDevice, *pvVirtStart;
	IMG_UINT32 ui32ByteSize, *pbPhysEnd;
	uintptr_t uiStartAddr, uiEndAddr, uiMaxLen;

	/* validate incoming parameters */
	if (!hDisplayContext ||
		!ahBuffers       ||
		!((DC_FBDEV_CONTEXT *)hDisplayContext)->psDeviceData ||
		!((DC_FBDEV_CONTEXT *)hDisplayContext)->psDeviceData->psLINFBInfo)
	{
		return;
	}

	/* lookup display context data, buffer and kernel device handle */
	psDeviceData = ((DC_FBDEV_CONTEXT *)hDisplayContext)->psDeviceData;
	pvOSDevice = psDeviceData->psLINFBInfo->dev;
	PVR_UNREFERENCED_PARAMETER(pvOSDevice);
	psBuffer = ahBuffers[0];

	/* calc. framebuffer start/end addresses */
	ui32ByteSize = psBuffer->ui32ByteStride * psBuffer->ui32Height;
	uiStartAddr  = (uintptr_t)psDeviceData->psLINFBInfo->screen_base;
	uiStartAddr += psBuffer->ui32BufferID * ui32ByteSize;
	uiMaxLen     = psDeviceData->psLINFBInfo->fix.smem_len;
	uiMaxLen    -= psBuffer->ui32BufferID * ui32ByteSize;
	uiEndAddr    = uiStartAddr + ui32MaxLen;

	/* outer framebuffer inter-page loop */
	pvVirtStart  = (IMG_BYTE *)((uintptr_t)uiStartAddr & (~(PAGE_SIZE-1)));
	while ((uintptr_t)pvVirtStart < uiEndAddr)
	{
#if !(defined(ARM64) && defined(X86))
		/* crossing (possibly discontiguous) page boundary, translate virt -> phys page */
		sCPUPhysStart.uiAddr =
		page_to_phys(vmalloc_to_page(pvVirtStart));
		pbPhysEnd = (IMG_UINT32 *)((uintptr_t)sCpuPAddr.uiAddr & (~(PAGE_SIZE-1)));
#endif

		/* inner intra-page loop */
		while (pvVirtStart < (pvVirtStart + PAGE_SIZE))
		{
			/* per-arch d-cache flush mechanism */
#if defined(CONFIG_X86)
			asm volatile("clflush %0" : "+m" (*pvVirtStart));
#elif defined(CONFIG_ARM64)
			asm volatile ("dc civac, %0" :: "r" (pvVirtStart));
#elif defined(CONFIG_ARM)
			unsigned long uiLockFlags = {0};
			spinlock_t spinlock = {0};
			flush_cache_all();
#if defined(CONFIG_OUTER_CACHE)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0))
			spin_lock_init(&spinlock);
			spin_lock_irqsave(&spinlock, uiLockFlags);
			outer_clean_range(0, ULONG_MAX);
			spin_unlock_irqrestore(&spinlock, uiLockFlags);
#else
			outer_clean_range(0, ULONG_MAX)
#endif
#endif
			PVR_UNREFERENCED_PARAMETER(uiLockFlags);
			PVR_UNREFERENCED_PARAMETER(spinlock);
			return;
#elif defined(CONFIG_MIPS)
			dma_cache_sync(pvOSDevice, (void *)pvVirtStart, PAGE_SIZE, DMA_TO_DEVICE);
#elif defined(CONFIG_META)
			writeback_dcache_region((void *)pbPhysEnd, PAGE_SIZE);
#else
			PVR_ASSERT(0);
#endif

			/* move to next d-cache line/page */
#if defined(ARM64) || defined(X86)
			pvVirtStart += cache_line_size();
#else
			pvVirtStart += PAGE_SIZE;
#endif
		}
	}
}
#endif

static
void DC_FBDEV_GetInfo(IMG_HANDLE hDeviceData,
                      DC_DISPLAY_INFO *psDisplayInfo)
{
	PVR_UNREFERENCED_PARAMETER(hDeviceData);

	strncpy(psDisplayInfo->szDisplayName, DRVNAME " 1", DC_NAME_SIZE);

	psDisplayInfo->ui32MinDisplayPeriod	= 0;
	psDisplayInfo->ui32MaxDisplayPeriod	= 1;
	psDisplayInfo->ui32MaxPipes			= 1;
	psDisplayInfo->bUnlatchedSupported	= IMG_FALSE;
}

static
PVRSRV_ERROR DC_FBDEV_PanelQueryCount(IMG_HANDLE hDeviceData,
									  IMG_UINT32 *pui32NumPanels)
{
	PVR_UNREFERENCED_PARAMETER(hDeviceData);
	*pui32NumPanels = 1;
	return PVRSRV_OK;
}

static
PVRSRV_ERROR DC_FBDEV_PanelQuery(IMG_HANDLE hDeviceData,
								 IMG_UINT32 ui32PanelsArraySize,
								 IMG_UINT32 *pui32NumPanels,
								 PVRSRV_PANEL_INFO *psPanelInfo)
{
	DC_FBDEV_DEVICE *psDeviceData = hDeviceData;
	struct fb_var_screeninfo *psVar = &psDeviceData->psLINFBInfo->var;
	struct fb_var_screeninfo sVar = { .pixclock = 0 };

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0))
	if (!lock_fb_info(psDeviceData->psLINFBInfo))
		return PVRSRV_ERROR_RETRY;
#else
	lock_fb_info(psDeviceData->psLINFBInfo);
#endif

	*pui32NumPanels = 1;

	psPanelInfo[0].sSurfaceInfo.sFormat.ePixFormat = psDeviceData->ePixFormat;
	psPanelInfo[0].sSurfaceInfo.sDims.ui32Width    = psVar->xres;
	psPanelInfo[0].sSurfaceInfo.sDims.ui32Height   = psVar->yres;
	psPanelInfo[0].sSurfaceInfo.sFormat.eMemLayout = PVRSRV_SURFACE_MEMLAYOUT_STRIDED;
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	psVar->reserved[3] = 1;
	gPVRPreEnableFBDC = 1;
	gPVREnableFBDC = 1;
	psPanelInfo[0].sSurfaceInfo.sFormat.eMemLayout
		 = PVRSRV_SURFACE_MEMLAYOUT_FBC;
	if ( psVar->xres <= VIOC_PVRICSIZE_MAX_WIDTH_8X8 )
		psPanelInfo[0].sSurfaceInfo.sFormat.u.sFBCLayout.eFBCompressionMode = IMG_FB_COMPRESSION_DIRECT_8x8;
	else if ( psVar->xres <= VIOC_PVRICSIZE_MAX_WIDTH_16X4 )
		psPanelInfo[0].sSurfaceInfo.sFormat.u.sFBCLayout.eFBCompressionMode = IMG_FB_COMPRESSION_DIRECT_16x4;
	else if ( psVar->xres <= VIOC_PVRICSIZE_MAX_WIDTH_32X2 )
		psPanelInfo[0].sSurfaceInfo.sFormat.u.sFBCLayout.eFBCompressionMode = IMG_FB_COMPRESSION_DIRECT_32x2;
	else {
		psVar->reserved[3]=0;
		pr_err("%s Not support : xres should be less than 7860\n"
			, __func__);
		psPanelInfo[0].sSurfaceInfo.sFormat.u.sFBCLayout.eFBCompressionMode = IMG_FB_COMPRESSION_NONE;
	}
#else
	psPanelInfo[0].sSurfaceInfo.sFormat.u.sFBCLayout.eFBCompressionMode = IMG_FB_COMPRESSION_NONE;
#endif

	/* Conformant fbdev drivers should have 'var' and mode in sync by now,
	 * but some don't (like drmfb), so try a couple of different ways to
	 * get the info before falling back to the default.
	 */
	if (psVar->xres > 0 && psVar->yres > 0 && psVar->pixclock > 0)
		sVar = *psVar;
	else if (psDeviceData->psLINFBInfo->mode)
		fb_videomode_to_var(&sVar, psDeviceData->psLINFBInfo->mode);

	/* Override the refresh rate when defined. */
#ifdef DC_FBDEV_REFRESH
	psPanelInfo[0].ui32RefreshRate = DC_FBDEV_REFRESH;
#else
	if (sVar.xres > 0 && sVar.yres > 0 && sVar.pixclock > 0)
	{
		psPanelInfo[0].ui32RefreshRate = 1000000000LU /
			((sVar.upper_margin + sVar.lower_margin +
			  sVar.yres + sVar.vsync_len) *
			 (sVar.left_margin  + sVar.right_margin +
			  sVar.xres + sVar.hsync_len) *
			 (sVar.pixclock / 1000));
	}
	else
		psPanelInfo[0].ui32RefreshRate = FALLBACK_REFRESH_RATE;
#endif

	psPanelInfo[0].ui32XDpi =
		((int)sVar.width > 0) ? (254000 / sVar.width * psVar->xres / 10000) : FALLBACK_DPI;

	psPanelInfo[0].ui32YDpi	=
		((int)sVar.height > 0) ? 254000 / sVar.height * psVar->yres / 10000 : FALLBACK_DPI;

	unlock_fb_info(psDeviceData->psLINFBInfo);
	return PVRSRV_OK;
}

static
PVRSRV_ERROR DC_FBDEV_FormatQuery(IMG_HANDLE hDeviceData,
								  IMG_UINT32 ui32NumFormats,
								  PVRSRV_SURFACE_FORMAT *pasFormat,
								  IMG_UINT32 *pui32Supported)
{
	DC_FBDEV_DEVICE *psDeviceData = hDeviceData;
	int i;

	for (i = 0; i < ui32NumFormats; i++)
	{
		pui32Supported[i] = 0;

		if (pasFormat[i].ePixFormat == psDeviceData->ePixFormat)
			pui32Supported[i]++;
	}

	return PVRSRV_OK;
}

static
PVRSRV_ERROR DC_FBDEV_DimQuery(IMG_HANDLE hDeviceData,
							   IMG_UINT32 ui32NumDims,
							   PVRSRV_SURFACE_DIMS *psDim,
							   IMG_UINT32 *pui32Supported)
{
	DC_FBDEV_DEVICE *psDeviceData = hDeviceData;
	struct fb_var_screeninfo *psVar = &psDeviceData->psLINFBInfo->var;
	int i;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0))
	if (!lock_fb_info(psDeviceData->psLINFBInfo))
		return PVRSRV_ERROR_RETRY;
#else
	lock_fb_info(psDeviceData->psLINFBInfo);
#endif

	for (i = 0; i < ui32NumDims; i++)
	{
		pui32Supported[i] = 0;

		if (psDim[i].ui32Width  == psVar->xres &&
		    psDim[i].ui32Height == psVar->yres)
		    pui32Supported[i]++;
	}

	unlock_fb_info(psDeviceData->psLINFBInfo);
	return PVRSRV_OK;
}

static
PVRSRV_ERROR DC_FBDEV_ContextCreate(IMG_HANDLE hDeviceData,
									IMG_HANDLE *hDisplayContext)
{
	DC_FBDEV_CONTEXT *psDeviceContext;
	PVRSRV_ERROR eError = PVRSRV_OK;

	psDeviceContext = kzalloc(sizeof(DC_FBDEV_CONTEXT), GFP_KERNEL);
	if (!psDeviceContext)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_out;
	}

	psDeviceContext->psDeviceData = hDeviceData;
	*hDisplayContext = psDeviceContext;

err_out:
	return eError;
}

static PVRSRV_ERROR
DC_FBDEV_ContextConfigureCheck(IMG_HANDLE hDisplayContext,
							   IMG_UINT32 ui32PipeCount,
							   PVRSRV_SURFACE_CONFIG_INFO *pasSurfAttrib,
							   IMG_HANDLE *ahBuffers)
{
	DC_FBDEV_CONTEXT *psDeviceContext = hDisplayContext;
	DC_FBDEV_DEVICE *psDeviceData = psDeviceContext->psDeviceData;
	struct fb_var_screeninfo *psVar = &psDeviceData->psLINFBInfo->var;
	DC_FBDEV_BUFFER *psBuffer;
	PVRSRV_ERROR eError;

	if (ui32PipeCount != 1)
	{
		eError = PVRSRV_ERROR_DC_TOO_MANY_PIPES;
		goto err_out;
	}

	if (!ahBuffers)
	{
		eError = PVRSRV_ERROR_DC_INVALID_CONFIG;
		goto err_out;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0))
	if (!lock_fb_info(psDeviceData->psLINFBInfo))
	{
		eError = PVRSRV_ERROR_RETRY;
		goto err_out;
	}
#else
	lock_fb_info(psDeviceData->psLINFBInfo);
#endif

	psBuffer = ahBuffers[0];

	if (pasSurfAttrib[0].sCrop.sDims.ui32Width  != psVar->xres ||
		pasSurfAttrib[0].sCrop.sDims.ui32Height != psVar->yres ||
		pasSurfAttrib[0].sCrop.i32XOffset != 0 ||
		pasSurfAttrib[0].sCrop.i32YOffset != 0)
	{
		eError = PVRSRV_ERROR_DC_INVALID_CROP_RECT;
		goto err_unlock;
	}

	if (pasSurfAttrib[0].sDisplay.sDims.ui32Width !=
		pasSurfAttrib[0].sCrop.sDims.ui32Width ||
		pasSurfAttrib[0].sDisplay.sDims.ui32Height !=
		pasSurfAttrib[0].sCrop.sDims.ui32Height ||
		pasSurfAttrib[0].sDisplay.i32XOffset !=
		pasSurfAttrib[0].sCrop.i32XOffset ||
		pasSurfAttrib[0].sDisplay.i32YOffset !=
		pasSurfAttrib[0].sCrop.i32YOffset)
	{
		eError = PVRSRV_ERROR_DC_INVALID_DISPLAY_RECT;
		goto err_unlock;
	}

	if (psBuffer->ui32Width  != psVar->xres &&
		psBuffer->ui32Height != psVar->yres)
	{
		eError = PVRSRV_ERROR_DC_INVALID_BUFFER_DIMS;
		goto err_unlock;
	}

	eError = PVRSRV_OK;
err_unlock:
	unlock_fb_info(psDeviceData->psLINFBInfo);
err_out:
	return eError;
}

static
void DC_FBDEV_ContextConfigure(IMG_HANDLE hDisplayContext,
								   IMG_UINT32 ui32PipeCount,
								   PVRSRV_SURFACE_CONFIG_INFO *pasSurfAttrib,
								   IMG_HANDLE *ahBuffers,
								   IMG_UINT32 ui32DisplayPeriod,
								   IMG_HANDLE hConfigData)
{
	DC_FBDEV_CONTEXT *psDeviceContext = hDisplayContext;
	DC_FBDEV_DEVICE *psDeviceData = psDeviceContext->psDeviceData;
	struct fb_var_screeninfo sVar = psDeviceData->psLINFBInfo->var;
	int err;

	PVR_UNREFERENCED_PARAMETER(ui32PipeCount);
	PVR_UNREFERENCED_PARAMETER(pasSurfAttrib);
	PVR_UNREFERENCED_PARAMETER(ui32DisplayPeriod);

	sVar.yoffset = 0;

	if (ui32PipeCount != 0)
	{
		BUG_ON(ahBuffers == NULL);

		if (psDeviceData->bCanFlip)
		{
			DC_FBDEV_BUFFER *psBuffer = ahBuffers[0];
			sVar.yoffset = sVar.yres * psBuffer->ui32BufferID;
		}
	}
#if defined(CONFIG_PVRIC_FBDC)
	if (gPVREnableFBDC != gPVRPreEnableFBDC) {
		sVar.reserved[3]=gPVREnableFBDC;
		gPVRPreEnableFBDC = gPVREnableFBDC;
		pr_info("%s set enableFBDC to %d\n", __func__, gPVREnableFBDC);
	}
#endif

#if defined(DC_FBDEV_FORCE_CONTEXT_CLEAN)
	DC_FBDEV_Clean(hDisplayContext, ahBuffers);
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0))
	if (!lock_fb_info(psDeviceData->psLINFBInfo))
	{
		pr_err("lock_fb_info failed\n");
		return;
	}
#else
	lock_fb_info(psDeviceData->psLINFBInfo);
#endif

	console_lock();

	/* If we're supposed to be able to flip, but the yres_virtual has been
	 * changed to an unsupported (smaller) value, we need to change it back
	 * (this is a workaround for some Linux fbdev drivers that seem to lose any
	 * modifications to yres_virtual after a blank.)
	 */
	if (psDeviceData->bCanFlip &&
		sVar.yres_virtual < sVar.yres * NUM_PREFERRED_BUFFERS)
	{
		sVar.activate = FB_ACTIVATE_NOW;
		sVar.yres_virtual = sVar.yres * NUM_PREFERRED_BUFFERS;

		err = fb_set_var(psDeviceData->psLINFBInfo, &sVar);
		if (err)
			pr_err("fb_set_var failed (err=%d)\n", err);
	}
	else
	{
		err = fb_pan_display(psDeviceData->psLINFBInfo, &sVar);
		if (err)
			pr_err("fb_pan_display failed (err=%d)\n", err);
	}

	console_unlock();
	unlock_fb_info(psDeviceData->psLINFBInfo);

	if (psDeviceContext->hLastConfigData)
	{
		DCDisplayConfigurationRetired(psDeviceContext->hLastConfigData);
	}

	if (ui32PipeCount != 0)
	{
		psDeviceContext->hLastConfigData = hConfigData;
	}
	else
	{
		/* If the pipe count is zero, we're tearing down. Don't record
		 * any new configurations.
		 */
		psDeviceContext->hLastConfigData = NULL;

		/* We still need to "retire" this NULL flip as that signals back to
		 * the DC core that we've finished doing what we need to do and it
		 * can destroy the display context
		 */
		DCDisplayConfigurationRetired(hConfigData);
	}
}

static
void DC_FBDEV_ContextDestroy(IMG_HANDLE hDisplayContext)
{
	DC_FBDEV_CONTEXT *psDeviceContext = hDisplayContext;

	BUG_ON(psDeviceContext->hLastConfigData != NULL);
	kfree(psDeviceContext);
}

static
IMG_BOOL DC_FBDEV_GetBufferID(DC_FBDEV_CONTEXT *psDeviceContext, IMG_UINT32 *pui32BufferID)
{
	IMG_UINT32 ui32IDLimit;
	IMG_UINT32 ui32BufferID;

	if (psDeviceContext->psDeviceData->bCanFlip)
	{
		ui32IDLimit = NUM_PREFERRED_BUFFERS;
	}
	else
	{
		ui32IDLimit = 1;
	}

	for (ui32BufferID = 0; ui32BufferID < ui32IDLimit; ++ui32BufferID)
	{
		if ((psDeviceContext->ui32AllocUseMask & (1UL << ui32BufferID)) == 0)
		{
			psDeviceContext->ui32AllocUseMask |= (1UL << ui32BufferID);

			*pui32BufferID = ui32BufferID;

			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
void DC_FBDEV_PutBufferID(DC_FBDEV_CONTEXT *psDeviceContext, IMG_UINT32 ui32BufferID)
{
	psDeviceContext->ui32AllocUseMask &= ~(1UL << ui32BufferID);
}

#define BYTE_TO_PAGES(range) (((range) + (PAGE_SIZE - 1)) >> PAGE_SHIFT)

static
PVRSRV_ERROR DC_FBDEV_BufferAlloc(IMG_HANDLE hDisplayContext,
								  DC_BUFFER_CREATE_INFO *psCreateInfo,
								  IMG_DEVMEM_LOG2ALIGN_T *puiLog2PageSize,
								  IMG_UINT32 *pui32PageCount,
								  IMG_UINT32 *pui32ByteStride,
								  IMG_HANDLE *phBuffer)
{
	DC_FBDEV_CONTEXT *psDeviceContext = hDisplayContext;
	DC_FBDEV_DEVICE *psDeviceData = psDeviceContext->psDeviceData;
	PVRSRV_SURFACE_INFO *psSurfInfo = &psCreateInfo->sSurface;
	PVRSRV_ERROR eError;
	DC_FBDEV_BUFFER *psBuffer;
	IMG_UINT32 ui32ByteSize;

	if (psSurfInfo->sFormat.ePixFormat != psDeviceData->ePixFormat)
	{
		eError = PVRSRV_ERROR_UNSUPPORTED_PIXEL_FORMAT;
		goto err_out;
	}

	psBuffer = kmalloc(sizeof(DC_FBDEV_BUFFER), GFP_KERNEL);
	if (!psBuffer)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_out;
	}

	psBuffer->psDeviceContext = psDeviceContext;
	psBuffer->ui32ByteStride =
		psSurfInfo->sDims.ui32Width * psCreateInfo->ui32BPP;

	psBuffer->ui32Width = psSurfInfo->sDims.ui32Width;
	psBuffer->ui32Height = psSurfInfo->sDims.ui32Height;

	if (!DC_FBDEV_GetBufferID(psDeviceContext, &psBuffer->ui32BufferID))
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_free;
	}

	if (psSurfInfo->sFormat.eMemLayout == PVRSRV_SURFACE_MEMLAYOUT_FBC) {
		//pr_info("%s ui32FBCBufSize:%d\n", __func__,
		//		 psCreateInfo->u.sFBC.ui32Size);
		psBuffer->bFBComp = IMG_TRUE;
		psBuffer->ui32FBCBufSize = psCreateInfo->sFBC.ui32Size;
		ui32ByteSize = psBuffer->ui32FBCBufSize;
	} else {
		psBuffer->bFBComp = IMG_FALSE;
		ui32ByteSize = psBuffer->ui32ByteStride * psBuffer->ui32Height;
	}

	*puiLog2PageSize = PAGE_SHIFT;
	*pui32PageCount	 = BYTE_TO_PAGES(ui32ByteSize);
	*pui32ByteStride = psBuffer->ui32ByteStride;
	*phBuffer	 = psBuffer;

	return PVRSRV_OK;

err_free:
	kfree(psBuffer);

err_out:
	return eError;
}

static
PVRSRV_ERROR DC_FBDEV_BufferAcquire(IMG_HANDLE hBuffer,
									IMG_DEV_PHYADDR *pasDevPAddr,
									void **ppvLinAddr)
{
	DC_FBDEV_BUFFER *psBuffer = hBuffer;
	DC_FBDEV_DEVICE *psDeviceData = psBuffer->psDeviceContext->psDeviceData;
	uintptr_t uiStartAddr;
	IMG_UINT32 i, ui32MaxLen;
	IMG_UINT32 ui32ByteSize;

	if (psBuffer->bFBComp)
		ui32ByteSize = psBuffer->ui32FBCBufSize;
	else
		ui32ByteSize = psBuffer->ui32ByteStride * psBuffer->ui32Height;

#if defined(DC_FBDEV_USE_SCREEN_BASE)
	uiStartAddr = (uintptr_t)psDeviceData->psLINFBInfo->screen_base;
#else
	uiStartAddr = psDeviceData->psLINFBInfo->fix.smem_start;
#endif

	uiStartAddr += psBuffer->ui32BufferID
			 * (BYTE_TO_PAGES(ui32ByteSize)*PAGE_SIZE);
	ui32MaxLen = psDeviceData->psLINFBInfo->fix.smem_len -
				 psBuffer->ui32BufferID * ui32ByteSize;

	//pr_info("%s uiStartAddr:0x%x bufferID:%d ui32FBCBufSize:%d\n", __func__, uiStartAddr, psBuffer->ui32BufferID, psBuffer->ui32FBCBufSize);
	for (i = 0; i < BYTE_TO_PAGES(ui32ByteSize); i++)
	{
		BUG_ON(i * PAGE_SIZE >= ui32MaxLen);
		pasDevPAddr[i].uiAddr = uiStartAddr + (i * PAGE_SIZE);
#if defined(DC_FBDEV_USE_SCREEN_BASE)
		pasDevPAddr[i].uiAddr =
		page_to_phys(vmalloc_to_page((void *)(uintptr_t)pasDevPAddr[i].uiAddr));
#endif
	}

	/* We're UMA, so services will do the right thing and make
	 * its own CPU virtual address mapping for the buffer.
	 */
	*ppvLinAddr = NULL;

	return PVRSRV_OK;
}

static void DC_FBDEV_BufferRelease(IMG_HANDLE hBuffer)
{
	PVR_UNREFERENCED_PARAMETER(hBuffer);
}

static void DC_FBDEV_BufferFree(IMG_HANDLE hBuffer)
{
	DC_FBDEV_BUFFER *psBuffer = hBuffer;

	DC_FBDEV_PutBufferID(psBuffer->psDeviceContext, psBuffer->ui32BufferID);

	kfree(psBuffer);
}

/* If we can flip, we need to make sure we have the memory to do so.
 *
 * We'll assume that the fbdev device provides extra space in
 * yres_virtual for panning; xres_virtual is theoretically supported,
 * but it involves more work.
 *
 * If the fbdev device doesn't have yres_virtual > yres, we'll try
 * requesting it before bailing. Userspace applications commonly do
 * this with an FBIOPUT_VSCREENINFO ioctl().
 *
 * Another problem is with a limitation in the services DC -- it
 * needs framebuffers to be page aligned (this is a SW limitation,
 * the HW can support non-page-aligned buffers). So we have to
 * check that stride * height for a single buffer is page aligned.
 */

static bool DC_FBDEV_FlipPossible(struct fb_info *psLINFBInfo)
{
	struct fb_var_screeninfo sVar = psLINFBInfo->var;
	int err;

	if (!psLINFBInfo->fix.xpanstep && !psLINFBInfo->fix.ypanstep &&
		!psLINFBInfo->fix.ywrapstep)
	{
		pr_err("The fbdev device detected does not support ypan/ywrap. "
			   "Flipping disabled.\n");
		return false;
	}

	if ((psLINFBInfo->fix.line_length * sVar.yres) % PAGE_SIZE != 0)
	{
		pr_err("Line length (in bytes) x yres is not a multiple of "
			   "page size. Flipping disabled.\n");
		return false;
	}

	/* We might already have enough space */
	if (sVar.yres * NUM_PREFERRED_BUFFERS <= sVar.yres_virtual)
		return true;

	pr_err("No buffer space for flipping; asking for more.\n");

	sVar.activate = FB_ACTIVATE_NOW;
	sVar.yres_virtual = sVar.yres * NUM_PREFERRED_BUFFERS;

	err = fb_set_var(psLINFBInfo, &sVar);
	if (err)
	{
		pr_err("fb_set_var failed (err=%d). Flipping disabled.\n", err);
		return false;
	}

	if (sVar.yres * NUM_PREFERRED_BUFFERS > sVar.yres_virtual)
	{
		pr_err("Failed to obtain additional buffer space. "
			   "Flipping disabled.\n");
		return false;
	}

	/* Some fbdev drivers allow the yres_virtual modification through,
	 * but don't actually update the fix. We need the fix to be updated
	 * and more memory allocated, so we can actually take advantage of
	 * the increased yres_virtual.
	 */
	if (psLINFBInfo->fix.smem_len < psLINFBInfo->fix.line_length * sVar.yres_virtual)
	{
		pr_err("'fix' not re-allocated with sufficient buffer space. "
			   "Flipping disabled.\n");
		return false;
	}

	return true;
}

static int __init DC_FBDEV_init(void)
{
	static DC_DEVICE_FUNCTIONS sDCFunctions =
	{
		.pfnGetInfo					= DC_FBDEV_GetInfo,
		.pfnPanelQueryCount			= DC_FBDEV_PanelQueryCount,
		.pfnPanelQuery				= DC_FBDEV_PanelQuery,
		.pfnFormatQuery				= DC_FBDEV_FormatQuery,
		.pfnDimQuery				= DC_FBDEV_DimQuery,
		.pfnSetBlank				= NULL,
		.pfnSetVSyncReporting		= NULL,
		.pfnLastVSyncQuery			= NULL,
		.pfnContextCreate			= DC_FBDEV_ContextCreate,
		.pfnContextDestroy			= DC_FBDEV_ContextDestroy,
		.pfnContextConfigure		= DC_FBDEV_ContextConfigure,
		.pfnContextConfigureCheck	= DC_FBDEV_ContextConfigureCheck,
		.pfnBufferAlloc				= DC_FBDEV_BufferAlloc,
		.pfnBufferAcquire			= DC_FBDEV_BufferAcquire,
		.pfnBufferRelease			= DC_FBDEV_BufferRelease,
		.pfnBufferFree				= DC_FBDEV_BufferFree,
	};

	struct fb_info *psLINFBInfo;
	IMG_PIXFMT ePixFormat;
	IMG_UINT32 ui32fb_devminor;
	IMG_UINT32 i;
	int err = -ENODEV;

	for (ui32fb_devminor = 0; ui32fb_devminor < num_registered_fb;
							 ui32fb_devminor++) {
		pr_info("%s ui32fb_devminor=%u\n", __func__, ui32fb_devminor);

		psLINFBInfo = registered_fb[ui32fb_devminor];

		if (!psLINFBInfo) 
		{
			pr_err("No Linux framebuffer (/dev/fbdev%u) device is registered!\n"
				"Check you have a framebuffer driver compiled into your "
				"kernel\nand that it is enabled on the cmdline.\n",
				ui32fb_devminor);
			goto err_out;
		}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0))
		if (!lock_fb_info(psLINFBInfo))
			goto err_out;
#else
		lock_fb_info(psLINFBInfo);
#endif

		console_lock();

		/* Filter out broken FB devices */
		if (!psLINFBInfo->fix.smem_len || !psLINFBInfo->fix.line_length)
		{
			pr_err("The fbdev device detected had a zero smem_len or "
				"line_length,\nwhich suggests it is a broken driver.\n");
			goto err_unlock;
		}

		if (psLINFBInfo->fix.type != FB_TYPE_PACKED_PIXELS ||
			psLINFBInfo->fix.visual != FB_VISUAL_TRUECOLOR)
		{
			pr_err("The fbdev device detected is not truecolor with packed "
				"pixels.\n");
			goto err_unlock;
		}

		if (psLINFBInfo->var.bits_per_pixel == 32)
		{
			if (psLINFBInfo->var.red.length   != 8  ||
				psLINFBInfo->var.green.length != 8  ||
				psLINFBInfo->var.blue.length  != 8  ||
				psLINFBInfo->var.red.offset   != 16 ||
				psLINFBInfo->var.green.offset != 8  ||
				psLINFBInfo->var.blue.offset  != 0)
			{
				pr_err("The fbdev device detected uses an unrecognized "
					"32bit pixel format (%u/%u/%u, %u/%u/%u)\n",
					psLINFBInfo->var.red.length,
					psLINFBInfo->var.green.length,
					psLINFBInfo->var.blue.length,
					psLINFBInfo->var.red.offset,
					psLINFBInfo->var.green.offset,
					psLINFBInfo->var.blue.offset);
				goto err_unlock;
			}
	#if defined(DC_FBDEV_FORCE_XRGB8888)
			ePixFormat = IMG_PIXFMT_B8G8R8X8_UNORM;
	#else
			ePixFormat = IMG_PIXFMT_B8G8R8A8_UNORM;
	#endif
		}
		else if (psLINFBInfo->var.bits_per_pixel == 16)
		{
			if (psLINFBInfo->var.red.length   != 5  ||
				psLINFBInfo->var.green.length != 6  ||
				psLINFBInfo->var.blue.length  != 5  ||
				psLINFBInfo->var.red.offset   != 11 ||
				psLINFBInfo->var.green.offset != 5  ||
				psLINFBInfo->var.blue.offset  != 0)
			{
				pr_err("The fbdev device detected uses an unrecognized "
					"16bit pixel format (%u/%u/%u, %u/%u/%u)\n",
					psLINFBInfo->var.red.length,
					psLINFBInfo->var.green.length,
					psLINFBInfo->var.blue.length,
					psLINFBInfo->var.red.offset,
					psLINFBInfo->var.green.offset,
					psLINFBInfo->var.blue.offset);
				goto err_unlock;
			}
			ePixFormat = IMG_PIXFMT_B5G6R5_UNORM;
		}
		else
		{
			pr_err("The fbdev device detected uses an unsupported "
				"bpp (%u).\n", psLINFBInfo->var.bits_per_pixel);
			goto err_unlock;
		}

		if (!try_module_get(psLINFBInfo->fbops->owner))
		{
			pr_err("try_module_get() failed");
			goto err_unlock;
		}

		if (psLINFBInfo->fbops->fb_open &&
			psLINFBInfo->fbops->fb_open(psLINFBInfo, 0) != 0)
		{
			pr_err("fb_open() failed");
			goto err_module_put;
		}

		gpsDeviceData[ui32fb_devminor]
			 = kmalloc(sizeof(DC_FBDEV_DEVICE), GFP_KERNEL);
		if (!gpsDeviceData[ui32fb_devminor])
			goto err_module_put;

		gpsDeviceData[ui32fb_devminor]->psLINFBInfo = psLINFBInfo;
		gpsDeviceData[ui32fb_devminor]->ePixFormat = ePixFormat;

		if (DCRegisterDevice(&sDCFunctions,
				 MAX_COMMANDS_IN_FLIGHT,
				 gpsDeviceData[ui32fb_devminor],
				 &gpsDeviceData[ui32fb_devminor]->hSrvHandle)
				 != PVRSRV_OK) {
			pr_err("DC register device fail\n");
			goto err_kfree;
		}

		gpsDeviceData[ui32fb_devminor]->bCanFlip
			 = DC_FBDEV_FlipPossible(psLINFBInfo);

		pr_info("Found usable fbdev device (%s):\n"
	#if defined(DC_FBDEV_USE_SCREEN_BASE)
				"range (virtual)  = 0x%lx-0x%lx\n"
	#else
				"range (physical) = 0x%lx-0x%lx\n"
	#endif
				"size (bytes)     = 0x%x\n"
				"xres x yres      = %ux%u\n"
				"xres x yres (v)  = %ux%u\n"
				"img pix fmt      = %u\n"
				"flipping?        = %d\n"
				"ui32fb_devminor  = %u\n",
				psLINFBInfo->fix.id,
				psLINFBInfo->fix.smem_start,
				psLINFBInfo->fix.smem_start + psLINFBInfo->fix.smem_len,
				psLINFBInfo->fix.smem_len,
				psLINFBInfo->var.xres, psLINFBInfo->var.yres,
				psLINFBInfo->var.xres_virtual, psLINFBInfo->var.yres_virtual,
				ePixFormat, gpsDeviceData[ui32fb_devminor]->bCanFlip,
				ui32fb_devminor);

	console_unlock();
	unlock_fb_info(psLINFBInfo);
	}
	err = 0;
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	err = PVRTestCreateDIEntries();
	if (err != 0)
		pr_err("%s PVRTestCreateDIEntries fail\n", __func__);
#endif
err_unlock:
	console_unlock();
	unlock_fb_info(psLINFBInfo);
err_out:
	return err;
err_kfree:
	for (i = 0; i <= ui32fb_devminor; i++)
		kfree(gpsDeviceData[i]);
err_module_put:
	module_put(psLINFBInfo->fbops->owner);
	goto err_unlock;
}

static void __exit DC_FBDEV_exit(void)
{
	DC_FBDEV_DEVICE *psDeviceData;
	struct fb_info *psLINFBInfo;
	IMG_UINT32 ui32fb_devminor;
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	PVRTestRemoveDIEntries();
#endif
	for (ui32fb_devminor = 0; ui32fb_devminor < num_registered_fb;
							 ui32fb_devminor++) {
		psDeviceData = gpsDeviceData[ui32fb_devminor];
		psLINFBInfo = psDeviceData->psLINFBInfo;

		lock_fb_info(psLINFBInfo);
		console_lock();

		if (psLINFBInfo->fbops->fb_release)
		psLINFBInfo->fbops->fb_release(psLINFBInfo, 0);

		module_put(psLINFBInfo->fbops->owner);

		console_unlock();
		unlock_fb_info(psLINFBInfo);

		DCUnregisterDevice(psDeviceData->hSrvHandle);
		kfree(psDeviceData);
	}
}

//module_init(DC_FBDEV_init);
//module_exit(DC_FBDEV_exit);
late_initcall(DC_FBDEV_init);

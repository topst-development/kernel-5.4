override HOST_PRIMARY_ARCH := host_x86_64
override HOST_32BIT_ARCH := host_i386
override HOST_FORCE_32BIT := -m32
override HOST_ALL_ARCH := host_x86_64 host_i386
override TARGET_PRIMARY_ARCH := target_aarch64
override TARGET_SECONDARY_ARCH :=
override TARGET_ALL_ARCH := target_aarch64
override TARGET_FORCE_32BIT :=
override PVR_ARCH := volcanic
override METAG_VERSION_NEEDED := 2.8.1.0.3
override RISCV_VERSION_NEEDED := 1.0.1
override KERNEL_ID := 4.14.202-tcc
override PVRSRV_MODULE_BASEDIR := /vendor/modules/
override KERNEL_COMPONENTS := srvkm
override KERNEL_CROSS_COMPILE := aarch64-linux-android-
override PVRSRV_MODNAME := pvrsrvkm
override PVRHMMU_MODNAME :=
override PVRSYNC_MODNAME := pvr_sync
override PVR_BUILD_DIR := tcc32_android
ifeq ($(CONFIG_POWERVR_DEBUG),y)
override PVR_BUILD_TYPE := debug
else
override PVR_BUILD_TYPE := release
endif
override SUPPORT_RGX := 1
override PVR_SYSTEM := tcc_9xtp
override PVR_LOADER :=
ifeq ($(CONFIG_POWERVR_DEBUG),y)
override BUILD := debug
else
override BUILD := release
override SORT_BRIDGE_STRUCTS := 1
override DEBUGLINK := 1
endif
override SUPPORT_PHYSMEM_TEST := 1
override COMPRESS_DEBUG_SECTIONS := 1
override RGX_BNC := 27.V.254.2
ifeq ($(CONFIG_POWERVR_VZ),y)
override RGX_NUM_OS_SUPPORTED := 2
else
override RGX_NUM_OS_SUPPORTED := 1
endif
override VMM_TYPE := stub
override SUPPORT_POWMON_COMPONENT := 1
override OPTIM := -O2
override RGX_TIMECORR_CLOCK := sched
override PDVFS_COM_HOST := 1
override PDVFS_COM_AP := 2
override PDVFS_COM_PMC := 3
override PDVFS_COM_IMG_CLKDIV := 4
override PDVFS_COM := PDVFS_COM_HOST
override PVR_GPIO_MODE_GENERAL := 1
override PVR_GPIO_MODE_POWMON_PIN := 2
override PVR_GPIO_MODE := PVR_GPIO_MODE_GENERAL
override PVR_HANDLE_BACKEND := idr
override SUPPORT_DMABUF_BRIDGE := 1
override SUPPORT_DI_BRG_IMPL := 1
override SUPPORT_NATIVE_FENCE_SYNC := 1
override SUPPORT_DMA_FENCE := 1
override SUPPORT_ANDROID_PLATFORM := 1
ifeq ($(CONFIG_ION),y)
override SUPPORT_ION := 1
endif
ifeq ($(CONFIG_DMABUF_HEAPS),y)
override SUPPORT_DMA_HEAP := 1
endif
override PVRSRV_ENABLE_PVR_ION_STATS := 1
override RK_INIT_V2 := 1
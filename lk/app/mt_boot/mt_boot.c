/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2015. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
* RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
* AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
* NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
* SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
* SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
* THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
* THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
* CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
* SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
* CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
* AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
* OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
* MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*/

#include <atags.h>              // for target_atag_masp_data()
#include <app.h>
#include <debug.h>
#include <arch.h>
#include <arch/arm.h>
#include <arch/arm/mmu.h>
#include <dev/udc.h>
#include <reg.h>
#include <string.h>
#include <stdlib.h>
#include <kernel/thread.h>
#include <arch/ops.h>
#include <ctype.h>
#include <target.h>
#include <platform.h>
#include <platform/mt_reg_base.h>
#include <platform/boot_mode.h>
#include <bootimg.h>
#ifdef MTK_GPT_SCHEME_SUPPORT
#include <platform/partition.h>
#else
#include <mt_partition.h>
#endif
#include <platform/mt_disp_drv.h>
#include <platform/env.h>
#include <target/cust_usb.h>
#include <platform/mt_gpt.h>
#if defined(MTK_SECURITY_SW_SUPPORT)
#include "oemkey.h"
#endif
#include <video.h>              // for video_printf()
#include <platform/mtk_wdt.h>   // for mtk_wdt_disable()
#include <mt_boot.h>
#include <libfdt.h>
#include <ufdt_overlay.h>       // for ufdt_install_blob(), ufdt_apply_overlay()
#include <mt_rtc.h>             // for Check_RTC_Recovery_Mode()
#include <part_interface.h>     // for partition_read()
#include <dev/mrdump.h>         // for mrdump_append_cmdline()
#include <iothread.h>
#include <RoT.h>
#include <odm_mdtbo.h>          // for load_overlay_dtbo()

#include <profiling.h>
#include <rnd.h>

#define BOOT_STATE_GREEN   0x0
#define BOOT_STATE_ORANGE  0x1
#define BOOT_STATE_YELLOW  0x2
#define BOOT_STATE_RED     0x3
#define TMPBUF_SIZE         200


#ifdef MBLOCK_LIB_SUPPORT
#include <mblock.h>
#endif

#ifdef MTK_AB_OTA_UPDATER
#include "bootctrl.h"
#endif

#include <platform/mtk_key.h>

// FIXME!!! The following function declaration should not exist, and appropriate
//          header files instead should be included.
void write_protect_flow();
int mboot_recovery_load_raw_part(char *part_name, unsigned long *addr,
				 unsigned int size);

extern int kernel_charging_boot(void) __attribute__((weak));
extern int pmic_detect_powerkey(void);
extern void mt65xx_backlight_off(void);

extern void jumparch64_smc(u32 addr, u32 arg1, u32 arg2, u32 arg3);

extern u32 memory_size(void);
extern unsigned *target_atag_devinfo_data(unsigned *ptr);
extern unsigned *target_atag_videolfb(unsigned *ptr, size_t buf_size);
extern unsigned *target_atag_mdinfo(unsigned *ptr);
extern unsigned *target_atag_ptp(unsigned *ptr);
extern void platform_uninit(void);
extern int mboot_android_load_bootimg_hdr(char *part_name, unsigned long addr);
extern int mboot_android_load_bootimg(char *part_name, unsigned long addr);
extern int mboot_android_load_recoveryimg_hdr(char *part_name,
		unsigned long addr);
extern int mboot_android_load_recoveryimg(char *part_name, unsigned long addr);
extern int mboot_android_load_factoryimg_hdr(char *part_name,
		unsigned long addr);
extern int mboot_android_load_factoryimg(char *part_name, unsigned long addr);
extern void custom_port_in_kernel(BOOTMODE boot_mode, char *command);
extern const char *mt_disp_get_lcm_id(void);
extern unsigned int DISP_GetVRamSize(void);
extern int mt_disp_is_lcm_connected(void);
extern int fastboot_init(void *base, unsigned size);
extern int sec_boot_check(int try_lock);
extern int seclib_set_oemkey(u8 *key, u32 key_size);
extern BI_DRAM bi_dram[MAX_NR_BANK];
#ifdef DEVICE_TREE_SUPPORT
#include <libfdt.h>
#ifdef USE_ITS_BOOTIMG
#include <platform/dtb.h>
#endif
extern unsigned int *device_tree, device_tree_size;
#endif
extern unsigned int g_boot_state;
extern int platform_skip_hibernation(void) __attribute__((weak));
extern int is_meta_log_disable(void)__attribute__((weak));

int g_is_64bit_kernel = 0;

#if defined(MBLOCK_LIB_SUPPORT)
#define BOOTIMG_ALLOCATE_FROM_MBLOCK
#endif

#ifdef BOOTIMG_ALLOCATE_FROM_MBLOCK
/* Occupy dtb/kernel/randisk from mblock */
u32 dtb_kernel_addr_mb = 0;
u32 kernel_addr_mb = 0;
u32 ramdisk_addr_mb = 0;
u32 kernel_sz_mb = 0;
u32 ramdisk_sz_mb = 0;
#endif

struct bootimg_hdr *g_boot_hdr = NULL;

#define CMDLINE_LEN   1024
static char g_cmdline[CMDLINE_LEN] = COMMANDLINE_TO_KERNEL;
char g_boot_reason[][16] = {"power_key", "usb", "rtc", "wdt", "wdt_by_pass_pwk", "tool_by_pass_pwk", "2sec_reboot", "unknown", "kernel_panic", "reboot", "watchdog"};
#if defined(MTK_SECURITY_SW_SUPPORT)
u8 g_oemkey[OEM_PUBK_SZ] = {OEM_PUBK};
#endif

/* battery driver related */
signed int fg_swocv_v;
signed int fg_swocv_i;
int shutdown_time;
int boot_voltage;
int two_sec_reboot;

#ifdef MTK_AB_OTA_UPDATER
static char *p_AB_suffix;
static uint8 AB_retry_count;
#endif /* MTK_AB_OTA_UPDATER */

/* Please define SN_BUF_LEN in cust_usb.h */
#ifndef SN_BUF_LEN
#define SN_BUF_LEN  19  /* fastboot use 13 bytes as default, max is 19 */
#endif

#define FDT_BUFF_SIZE		(1024)
#define FDT_CHECKER_SIZE	(8)
#define FDT_SPARE_SIZE		(FDT_BUFF_SIZE + FDT_CHECKER_SIZE)
#define FDT_BUFF_END	 	"BUFFEND"

#define DEFAULT_SERIAL_NUM "0123456789ABCDEF"

#define KERNEL_64BITS 1
#define KERNEL_32BITS 0

#define VIDEOLFB_PRE_HEADER_LENGTH  (5)

/* define meta init.rc path */
#if defined (MTK_RC_TO_VENDOR)
#define META_INIT_RC        "/vendor/etc/init/hw/meta_init.rc"
#define FACTORY_INIT_RC     "/vendor/etc/init/hw/factory_init.rc"
#else
#define META_INIT_RC        "/meta_init.rc"
#define FACTORY_INIT_RC     "/factory_init.rc"
#endif

/*
 * Support read barcode from /dev/pro_info to be serial number.
 * Then pass the serial number from cmdline to kernel.
 */
/* The following option should be defined in project make file. */
/* #define SERIAL_NUM_FROM_BARCODE */

#if defined(CONFIG_MTK_USB_UNIQUE_SERIAL) || (defined(MTK_SECURITY_SW_SUPPORT) && defined(MTK_SEC_FASTBOOT_UNLOCK_SUPPORT))
#define SERIALNO_LEN    38  /* from preloader */
char sn_buf[SN_BUF_LEN + 1] = ""; /* will read from EFUSE_CTR_BASE */
#else
#define SERIALNO_LEN    38
char sn_buf[SN_BUF_LEN + 1] = FASTBOOT_DEVNAME;
#endif

static struct udc_device surf_udc_device = {
	.vendor_id  = USB_VENDORID,
	.product_id = USB_PRODUCTID,
	.version_id = USB_VERSIONID,
	.manufacturer   = USB_MANUFACTURER,
	.product    = USB_PRODUCT_NAME,
};

typedef enum BUILD_TYPE {
	BUILD_TYPE_USER         = 0,
	BUILD_TYPE_USERDEBUG    = 1,
	BUILD_TYPE_ENG          = 2
} BUILD_TYPE_T;


/****************************************************************************
* Note that userdebug build defines both USERDEBUG_BUILD and USER_BUILD for
* backward compatibility for now.  Therefore, it is important to check
* USERDEBUG_BUILD before checking USER_BUILD.
****************************************************************************/
#ifdef USERDEBUG_BUILD
static BUILD_TYPE_T eBuildType = BUILD_TYPE_USERDEBUG;
#elif defined(USER_BUILD)
static BUILD_TYPE_T eBuildType = BUILD_TYPE_USER;
#elif defined(ENG_BUILD)
static BUILD_TYPE_T eBuildType = BUILD_TYPE_ENG;
#else
static BUILD_TYPE_T eBuildType = BUILD_TYPE_USER;
#endif


char *cmdline_get(void)
{
	return g_cmdline;
}

bool cmdline_append(const char *append_string)
{
	int cmdline_len = strlen(g_cmdline);
	int available_len = CMDLINE_LEN - cmdline_len;
	int result = snprintf(g_cmdline + cmdline_len, available_len, " %s",
			      append_string);

	if (result >= available_len) {
		dprintf(CRITICAL, "[MBOOT] ERROR CMDLINE overflow\n");
		video_printf("[MBOOT] ERROR CMDLINE overflow\n");
		assert(0);
	}
	return true;
}

bool cmdline_overwrite(const char *overwrite_string)
{
	int cmd_len;
	cmd_len = strlen(overwrite_string);
	if (cmd_len > CMDLINE_LEN - 1) {
		dprintf(CRITICAL, "[MBOOT] ERROR CMDLINE overflow\n");
		return false;
	}
	strncpy(g_cmdline, overwrite_string, CMDLINE_LEN - 1);
	return true;
}

void msg_header_error(char *img_name)
{
	dprintf(CRITICAL, "[MBOOT] Load '%s' partition Error\n", img_name);
	dprintf(CRITICAL,
		"\n*******************************************************\n");
	dprintf(CRITICAL, "ERROR.ERROR.ERROR.ERROR.ERROR.ERROR.ERROR.ERROR.ERROR\n");
	dprintf(CRITICAL, "*******************************************************\n");
	dprintf(CRITICAL, "> If you use NAND boot\n");
	dprintf(CRITICAL, "> (1) %s is wrong !!!! \n", img_name);
	dprintf(CRITICAL,
		"> (2) please make sure the image you've downloaded is correct\n");
	dprintf(CRITICAL, "\n> If you use MSDC boot\n");
	dprintf(CRITICAL, "> (1) %s is not founded in SD card !!!! \n", img_name);
	dprintf(CRITICAL, "> (2) please make sure the image is put in SD card\n");
	dprintf(CRITICAL, "*******************************************************\n");
	dprintf(CRITICAL, "ERROR.ERROR.ERROR.ERROR.ERROR.ERROR.ERROR.ERROR.ERROR\n");
	dprintf(CRITICAL, "*******************************************************\n");
	mtk_wdt_disable();
	mdelay(8000);
	mtk_arch_reset(1);
}

void msg_img_error(char *img_name)
{
	dprintf(CRITICAL, "[MBOOT] Load '%s' partition Error\n", img_name);
	dprintf(CRITICAL,
		"\n*******************************************************\n");
	dprintf(CRITICAL, "ERROR.ERROR.ERROR.ERROR.ERROR.ERROR.ERROR.ERROR.ERROR\n");
	dprintf(CRITICAL, "*******************************************************\n");
	dprintf(CRITICAL, "> Please check kernel and rootfs in %s are both correct.\n",
		img_name);
	dprintf(CRITICAL, "*******************************************************\n");
	dprintf(CRITICAL, "ERROR.ERROR.ERROR.ERROR.ERROR.ERROR.ERROR.ERROR.ERROR\n");
	dprintf(CRITICAL, "*******************************************************\n");
	mtk_wdt_disable();
	mdelay(8000);
	mtk_arch_reset(1);
}

//*********
//* Notice : it's kernel start addr (and not include any debug header)
extern unsigned int g_kmem_off;

//*********
//* Notice : it's rootfs start addr (and not include any debug header)
extern unsigned int g_rmem_off;
#ifdef USE_ITS_BOOTIMG
extern unsigned int g_smem_off;
#endif
extern unsigned int g_rimg_sz;
extern int g_nr_bank;
extern unsigned int boot_time;
extern BOOT_ARGUMENT *g_boot_arg;
extern bool g_boot_reason_change __attribute__((weak));
extern int has_set_p2u;
extern unsigned int g_fb_base;
extern unsigned int g_fb_size;
unsigned int logo_lk_t = 0;

void *g_fdt;


/******************************************************************************
******************************************************************************/
bool dtb_overlay(void *fdt, int size)
{
	size_t overlay_len = 0;
	g_fdt = fdt;

	if (fdt == NULL) {
		dprintf(CRITICAL, "fdt is NULL\n");
		return FALSE;
	}
	if (size == 0) {
		dprintf(CRITICAL, "fdt size is zero\n");
		return FALSE;
	}

#ifdef MTK_AB_OTA_UPDATER
	// Compose the partition name for A/B systems.
	char part_name[16] = "odmdtbo";
	int part_name_len = strlen(part_name);
	if (p_AB_suffix) {
		strncpy((void *)&part_name[part_name_len], (void *)p_AB_suffix,
			sizeof(part_name) - part_name_len);
	}
	part_name[sizeof(part_name) - 1] = '\0';
#else
	char part_name[] = "odmdtbo";
#endif  // MTK_AB_OTA_UPDATER

	// Note: A buffer is allocated in load_overlay_dtbo() to store the loaded
	//       odm dtb.
	char *overlay_buf = load_overlay_dtbo(part_name, &overlay_len);
	if ((overlay_buf == NULL) || (overlay_len == 0)) {
		dprintf(CRITICAL, "load overlay dtbo failed !\n");
		return FALSE;
	}

	struct fdt_header *fdth = (struct fdt_header *)g_fdt;
	fdth->totalsize = cpu_to_fdt32(size);

	int ret = fdt_open_into(g_fdt, g_fdt, size);
	if (ret) {
		dprintf(CRITICAL, "fdt_open_into failed \n");
		free(overlay_buf);
		return FALSE;
	}
	ret = fdt_check_header(g_fdt);
	if (ret) {
		dprintf(CRITICAL, "fdt_check_header check failed !\n");
		free(overlay_buf);
		return FALSE;
	}

	char *base_buf = fdt;
	size_t blob_len = size;
	struct fdt_header *blob = ufdt_install_blob(base_buf, blob_len);
	if (!blob) {
		dprintf(CRITICAL, "ufdt_install_blob() failed!\n");
		free(overlay_buf);
		return FALSE;
	}
	dprintf(INFO, "blob_len: 0x%x, overlay_len: 0x%x\n", blob_len, overlay_len);

	void *merged_fdt = NULL;
	PROFILING_START("Overlay");
	// Note: A buffer is allocated in ufdt_apply_overlay() to store the merge
	//       device tree.
	merged_fdt = ufdt_apply_overlay(blob, blob_len, overlay_buf, overlay_len);
	if (!merged_fdt) {
		dprintf(CRITICAL, "ufdt_apply_overlay() failed!\n");
		free(overlay_buf);
		assert(0);
		return FALSE;
	}
	PROFILING_END();

	// Compact the merged device tree so that the size of the device tree can
	// be known.
	ret = fdt_pack(merged_fdt);
	if (ret) {
		dprintf(CRITICAL, "fdt_pack(merged_fdt) failed !\n");
		free(merged_fdt);
		free(overlay_buf);
		return FALSE;
	}


	int merged_size = fdt_totalsize(merged_fdt);
	dprintf(INFO, "fdt merged_size: %d\n", merged_size);
	if (merged_size > DTB_MAX_SIZE) {
		dprintf(CRITICAL, "Error: merged size %d > DTB_MAX_SIZE!\n", merged_size);
		free(merged_fdt);
		free(overlay_buf);
		return FALSE;
	}

	// The memory pointed to by "g_fdt" is the location that the Linux kernel
	// expects to find the device tree, and it is at least a few mega-bytes
	// free. The merged device tree is therefore copied to that space.
	memcpy(g_fdt, merged_fdt, merged_size);

	// Make the totalsize of the device tree larger so that properties can
	// be inserted into the device tree.
	((struct fdt_header *)g_fdt)->totalsize = cpu_to_fdt32(DTB_MAX_SIZE);

	free(merged_fdt);
	free(overlay_buf);

	return TRUE;
}


static bool setup_fdt(void *fdt, int size)
{
	int ret;
	g_fdt = fdt;
#ifdef MTK_3LEVEL_PAGETABLE
	u32 addr = (u32)fdt;
	arch_mmu_map((uint64_t)ROUNDDOWN(addr, PAGE_SIZE),
		(uint32_t)ROUNDDOWN(addr, PAGE_SIZE),
		MMU_MEMORY_TYPE_NORMAL_WRITE_BACK | MMU_MEMORY_AP_P_RW_U_NA,
		ROUNDUP(size, PAGE_SIZE));
#endif
	ret = fdt_open_into(g_fdt, g_fdt, size); //DTB maximum size is 2MB
	if (ret) return FALSE;
	ret = fdt_check_header(g_fdt);
	if (ret) return FALSE;
	return TRUE;
}


bool bootimage_header_valid(char *buf)
{
	if (strncmp(buf, BOOTIMG_MAGIC, BOOTIMG_MAGIC_SZ) == 0)
		return TRUE;
	else
		return FALSE;
}

void boot_get_os_version(unsigned *os_version)
{
	if (NULL == os_version)
		return;

	if (g_boot_hdr && (TRUE == bootimage_header_valid((char *)g_boot_hdr))) {
		*os_version = g_boot_hdr->os_version;
		dprintf(CRITICAL, "boot_get_os_version(%x) (boot image header valid)\n",
			*os_version);
	} else {
		dprintf(CRITICAL, "boot_get_os_version(%x) (boot image header invalid)\n",
			*os_version);
		*os_version = 0;
	}

	return;
}

static void mboot_free_bootimg_from_mblock()
{
#ifdef BOOTIMG_ALLOCATE_FROM_MBLOCK
	/* Return the bootimg mb before stepping into kernel */
	if (dtb_kernel_addr_mb != 0) {
		mblock_create(&g_boot_arg->mblock_info,
			&g_boot_arg->orig_dram_info,
			(u64)dtb_kernel_addr_mb, (u64)DTB_MAX_SIZE);
	}

	if (kernel_addr_mb != 0) {
		mblock_create(&g_boot_arg->mblock_info,
			&g_boot_arg->orig_dram_info,
			(u64)kernel_addr_mb, (u64)kernel_sz_mb);
	}

	if (ramdisk_addr_mb != 0) {
		mblock_create(&g_boot_arg->mblock_info,
			&g_boot_arg->orig_dram_info,
			(u64)ramdisk_addr_mb, (u64)ramdisk_sz_mb);
	}
#endif
}


static void mboot_allocate_bootimg_from_mblock(struct bootimg_hdr *p_boot_hdr)
{
#ifdef BOOTIMG_ALLOCATE_FROM_MBLOCK

	/* never allocate mb more than once */
	if ((kernel_addr_mb == p_boot_hdr->kernel_addr)||
	(ramdisk_addr_mb == p_boot_hdr->ramdisk_addr)||
	(dtb_kernel_addr_mb == p_boot_hdr->tags_addr))
		return;

	#ifdef KERNEL_DECOMPRESS_SIZE
		kernel_sz_mb = KERNEL_DECOMPRESS_SIZE;
	#else
		kernel_sz_mb = 0x03200000;
	#endif

	ramdisk_sz_mb = ROUNDUP(p_boot_hdr->ramdisk_sz, PAGE_SIZE);

	/* To avoid dtb being corrupted, use mblock to claim it now */
	dtb_kernel_addr_mb = (u32)mblock_reserve_ext(&g_boot_arg->mblock_info,
		DTB_MAX_SIZE, DTB_MAX_SIZE, (p_boot_hdr->tags_addr+DTB_MAX_SIZE),
		0, "dtb_kernel_addr_mb");

	kernel_addr_mb = (u32)mblock_reserve_ext(&g_boot_arg->mblock_info,
		kernel_sz_mb, PAGE_SIZE,(p_boot_hdr->kernel_addr+kernel_sz_mb),
		0, "kernel_addr_mb");

	ramdisk_addr_mb = (u32)mblock_reserve_ext(&g_boot_arg->mblock_info,
		ramdisk_sz_mb, PAGE_SIZE,(p_boot_hdr->ramdisk_addr+ramdisk_sz_mb),
		0, "ramdisk_addr_mb");

        /* Check if the clarmed mb address the same as predefined one */
        if ((!dtb_kernel_addr_mb) || (dtb_kernel_addr_mb!=p_boot_hdr->tags_addr)) {
                dprintf(CRITICAL, "Warning! dtb_kernel_addr (0x%x) is not taken from mb (0x%x)\n", p_boot_hdr->tags_addr, dtb_kernel_addr_mb);
		assert(0);
	}

	 if ((!kernel_addr_mb) || (kernel_addr_mb!=p_boot_hdr->kernel_addr)) {
                dprintf(CRITICAL, "Warning! kernel_addr (0x%x) is not taken from mb (0x%x)\n", p_boot_hdr->kernel_addr, kernel_addr_mb);
		assert(0);
	}

	 if ((!ramdisk_addr_mb) || (ramdisk_addr_mb!=p_boot_hdr->ramdisk_addr)) {
                dprintf(CRITICAL, "Warning! ramdisk_addr (0x%x) is not taken from mb (0x%x)\n", p_boot_hdr->ramdisk_addr, ramdisk_addr_mb);
		assert(0);
	}

#endif
}

void platform_parse_bootopt(u8 *bootopt_str)
{
	int i = 0;
	int find = 0;
	/* search "bootopt" string */
	for (; i < (BOOTIMG_ARGS_SZ - 0x16); i++) {
		if (0 == strncmp((const char *)&bootopt_str[i], "bootopt=",
				 sizeof("bootopt=") - 1)) {
			/* skip SMC, LK option */
			//Kernel option offset 0x12
			if (0 == strncmp((const char *)&bootopt_str[i + 0x12], "64",
					 sizeof("64") - 1)) {
				g_is_64bit_kernel = 1;
				find = 1;
				break;
			}
			if (0 == strncmp((const char *)&bootopt_str[i + 0x12], "32",
					 sizeof("32") - 1)) {
				find = 1;
				break;
			}
		}
	}
	if (!find) {
		dprintf(CRITICAL, "Warning! No bootopt info!\n");
		//No endless loop here because it will stuck when boot partition data lost
	}

}


/******************************************************************************
* Please refer to bootimg.h for boot image header structure.
* bootimg header size is 0x800, the kernel img text is next to the header
******************************************************************************/
int bldr_load_dtb(char *boot_load_partition)
{
	int ret = 0;
#if defined(CFG_DTB_EARLY_LOADER_SUPPORT)
	char *ptr;
	char *dtb_sz;
	u32 dtb_kernel_addr;
	u32 zimage_addr, zimage_size, dtb_size, addr;
	u32 dtb_addr = 0;
	u32 tmp;
	u32 offset;
	unsigned char *magic;
	struct bootimg_hdr *p_boot_hdr;
	char part_name[16];

    dprintf(CRITICAL, "Loading DTB from partition %s\n", boot_load_partition);
	ptr = malloc(DTB_MAX_SIZE);
	if (ptr == NULL) {
		dprintf(CRITICAL, "malloc failed!\n");
		return -1;
	}
#if !defined(NO_BOOT_MODE_SEL)
	if (Check_RTC_Recovery_Mode() || unshield_recovery_detection())
		boot_load_partition = "recovery";
#endif
#ifdef MTK_AB_OTA_UPDATER
	/* no more recovery partition in A/B system update, instead choose boot_a or boot_b */
	snprintf(part_name, sizeof(part_name), "boot%s", p_AB_suffix);
#else
	/* use input argument as usaual */
	snprintf(part_name, sizeof(part_name), "%s", boot_load_partition);
#endif

	//load boot hdr
	ret = mboot_recovery_load_raw_part(part_name, (unsigned long *)ptr,
					   sizeof(struct bootimg_hdr) + 0x800);
	if (ret < 0) {
		dprintf(CRITICAL, "mboot_recovery_load_raw_part(%s, %d) failed, ret: 0x%x\n",
			part_name, sizeof(struct bootimg_hdr) + 0x800, ret);
		goto _end;
	}
	if (FALSE == bootimage_header_valid(ptr)) {
		dprintf(CRITICAL, "bootimage_header_valid failed\n");
		ret = -1;
		goto _end;
	}

	p_boot_hdr = (void *)ptr;
	dtb_kernel_addr = p_boot_hdr->tags_addr;

	/* Claim the DT/kernel/ramdisk addr from mblock */
	mboot_allocate_bootimg_from_mblock(p_boot_hdr);

	platform_parse_bootopt(p_boot_hdr->cmdline);

	if (!g_is_64bit_kernel) {
		//Offset into zImage    Value   Description
		//0x24  0x016F2818  Magic number used to identify this is an ARM Linux zImage
		//0x28  start address   The address the zImage starts at
		//0x2C  end address The address the zImage ends at
		/* bootimg header size is 0x800, the kernel img text is next to the header */
		zimage_addr = (u32)ptr + p_boot_hdr->page_sz;
		zimage_size = *(unsigned int *)((unsigned int)zimage_addr + 0x2c) -
			*(unsigned int *)((unsigned int)zimage_addr + 0x28);
		//dtb_addr = (unsigned int)zimage_addr + zimage_size;
		offset = p_boot_hdr->page_sz + zimage_size;
		tmp = ROUNDDOWN(p_boot_hdr->page_sz + zimage_size, p_boot_hdr->page_sz);
		ret = partition_read(part_name, (off_t)tmp, (u8 *)ptr, (size_t)DTB_MAX_SIZE);
		if (ret < 0) {
			dprintf(CRITICAL, "partition_read failed, ret: 0x%x\n", ret);
			goto _end;
		}
		dtb_addr = (u32)ptr + offset - tmp;
		dtb_size = fdt32_to_cpu(*(unsigned int *)(ptr + (offset - tmp) + 0x4));
	} else {
		/* bootimg header size is 0x800, the kernel img text is next to the header */
		int i;
		zimage_size = p_boot_hdr->kernel_sz;
		offset = p_boot_hdr->page_sz + p_boot_hdr->kernel_sz - DTB_MAX_SIZE;
		tmp = ROUNDUP(offset, p_boot_hdr->page_sz);
		ret = partition_read(part_name, (off_t)tmp, (u8 *)ptr, (size_t)DTB_MAX_SIZE);
		if (ret < 0) {
			dprintf(CRITICAL, "partition_read failed, ret: 0x%x\n", ret);
			goto _end;
		}
		dtb_addr = 0;
		dtb_size = 0;
		addr = (u32)ptr + DTB_MAX_SIZE - 4;
		for (i = 0; i < (DTB_MAX_SIZE - 4); i++, addr--) {
			//FDT_MAGIC 0xd00dfeed
			//dtb append after image.gz may not 4 byte alignment
			magic = (unsigned char *)addr;
			if (*(magic + 3) == 0xED && *(magic + 2) == 0xFE &&
				*(magic + 1) == 0x0D && *(magic + 0) == 0xD0) {
				dtb_addr = addr;
				break;
			}
		}
		if (dtb_addr == 0) {
			dprintf(CRITICAL, "can't find dtb\n");
			ret = -1;
			goto _end;
		}
		dtb_sz = (char *)(dtb_addr + 4);
		dtb_size = *(dtb_sz) * 0x1000000 + *(dtb_sz + 1) * 0x10000 +
			*(dtb_sz + 2) * 0x100 + *(dtb_sz + 3);
		dprintf(CRITICAL, "Kernel(%d) zimage_size:0x%x,dtb_addr:0x%x(dtb_size:0x%x)\n",
			g_is_64bit_kernel, zimage_size, dtb_addr, dtb_size);
	}

	if (dtb_size > DTB_MAX_SIZE) {
		dprintf(CRITICAL, "dtb_size too large: 0x%x\n", dtb_size);
		ret = -1;
		goto _end;
	}

#ifdef MTK_3LEVEL_PAGETABLE
	arch_mmu_map((uint64_t)ROUNDDOWN(dtb_kernel_addr, PAGE_SIZE),
		(uint32_t)ROUNDDOWN(dtb_kernel_addr, PAGE_SIZE),
		MMU_MEMORY_TYPE_NORMAL_WRITE_BACK | MMU_MEMORY_AP_P_RW_U_NA,
		ROUNDUP(dtb_size, PAGE_SIZE));
#endif
	dprintf(INFO, "Copy DTB from 0x%x to 0x%x(size: 0x%x)\n", dtb_addr,
		dtb_kernel_addr, dtb_size);
	memcpy((void *)dtb_kernel_addr, (void *)dtb_addr, dtb_size);

	// Place setup_fdt() after bldr_load_dtb() because it sets "fdt_header->totalsize".
	ret = setup_fdt((void *)dtb_kernel_addr, DTB_MAX_SIZE);
	dprintf(CRITICAL, "[LK] fdt setup addr:0x%x status:%d!!!\n", dtb_kernel_addr,
		ret);
	if (ret == FALSE) {
		dprintf(CRITICAL, "setup_fdt fail, ret: 0x%x!\n", ret);
		ret = -1;
	}

_end:
	free(ptr);
#endif

	return ret;
}


static void check_hibernation()
{
	int hibboot = 0;
	char tmpbuf[TMPBUF_SIZE];

	hibboot = get_env("hibboot") == NULL ? 0 : atoi(get_env("hibboot"));

	switch (g_boot_mode) {
	case RECOVERY_BOOT:
	case FACTORY_BOOT:
	case ALARM_BOOT:
#if defined(MTK_KERNEL_POWER_OFF_CHARGING) || defined(MTK_CHARGER_NEW_ARCH)
	case KERNEL_POWER_OFF_CHARGING_BOOT:
	case LOW_POWER_OFF_CHARGING_BOOT:
#endif
		goto SKIP_HIB_BOOT;

	default:
		break;
	}

	if (platform_skip_hibernation && platform_skip_hibernation())
		goto SKIP_HIB_BOOT;

	if (get_env("resume") != NULL) {
		if (1 == hibboot) {
			snprintf(tmpbuf, TMPBUF_SIZE, "%s%s", " resume=", get_env("resume"));
			cmdline_append(tmpbuf);
#if defined(MTK_MLC_NAND_SUPPORT)
			snprintf(tmpbuf, TMPBUF_SIZE, "%s%s", " ubi.mtd=", get_env("ubi_data_mtd"));
			cmdline_append(tmpbuf);
#endif
			//cmdline_append(" no_console_suspend");
		} else if (0 != hibboot)
			dprintf(CRITICAL, "resume = %s but hibboot = %s\n", get_env("resume"),
				get_env("hibboot"));
	} else
		dprintf(CRITICAL, "resume = NULL \n");

	return;

SKIP_HIB_BOOT:
	if (hibboot != 0)
		if (set_env("hibboot", "0") != 0)
			dprintf(CRITICAL, "lk_env hibboot set failed!!!\n");
	if (get_env("resume") != NULL)
		if (set_env("resume", '\0') != 0)
			dprintf(CRITICAL, "lk_evn resume set resume failed!!!\n");
}





#ifdef DEVICE_TREE_SUPPORT

void lk_jump64(u32 addr, u32 arg1, u32 arg2, u32 arg3)
{
	dprintf(CRITICAL, "\n[LK]jump to K64 0x%x\n", addr);
	dprintf(INFO, "smc jump\n");
	jumparch64_smc(addr, arg1, arg2, arg3);
	dprintf(CRITICAL, "Do nothing now! wait for SMC done\n");
	while (1)
		;
}

void memcpy_u8(unsigned char *dest, unsigned char *src, unsigned int size)
{
	unsigned int i;

	for (i = 0; i < size; i++)
		*(dest + i) = *(src + i);
}

extern bool decompress_kernel(unsigned char *in, void *out, int inlen,
			      int outlen);

#if WITH_GZ
static int create_gz_dt_node(void *fdt)
{
	const unsigned int reg_propsize = 4;
	unsigned int reg_property[reg_propsize];
	extern unsigned int g_bimg_sz;
	int err, offset;
	char compatible[] = "multiboot,linux\0multiboot,module";

	offset = fdt_add_subnode(fdt, 0, "gz-kernel");
	if (offset < 0)
		return -1;

	err = fdt_setprop(fdt, offset, "compatible",
			  compatible, sizeof(compatible));
	if (err)
		return -1;

	reg_property[0] = cpu_to_fdt32(0);
	reg_property[1] = cpu_to_fdt32((g_boot_hdr != NULL) ? g_boot_hdr->kernel_addr :
				       CFG_BOOTIMG_LOAD_ADDR);
	reg_property[2] = cpu_to_fdt32(0);
	reg_property[3] = cpu_to_fdt32(g_bimg_sz);

	err = fdt_setprop(fdt, offset, "reg",
			  &reg_property, sizeof(reg_property));
	if (err)
		return -1;

	return 0;
}
#endif

#if defined(MTK_GOOGLE_TRUSTY_SUPPORT)
int trusty_dts_append(void *fdt)
{
	int offset, ret = 0;
	int nodeoffset = 0;
	unsigned int trusty_reserved_mem[4] = {0};

	offset = fdt_path_offset(fdt, "/reserved-memory");
	nodeoffset = fdt_add_subnode(fdt, offset, "trusty-reserved-memory");
	if (nodeoffset < 0) {
		dprintf(CRITICAL,
			"Warning: can't add trusty-reserved-memory node in device tree\n");
		return 1;
	}
	ret = fdt_setprop_string(fdt, nodeoffset, "compatible",
				 "mediatek,trusty-reserved-memory");
	if (ret) {
		dprintf(CRITICAL,
			"Warning: can't add trusty compatible property in device tree\n");
		return 1;
	}
	ret = fdt_setprop(fdt, nodeoffset, "no-map", NULL, 0);
	if (ret) {
		dprintf(CRITICAL, "Warning: can't add trusty no-map property in device tree\n");
		return 1;
	}
	trusty_reserved_mem[0] = 0;
	trusty_reserved_mem[1] = (u32)cpu_to_fdt32(g_boot_arg->tee_reserved_mem.start);
	trusty_reserved_mem[2] = 0;
	trusty_reserved_mem[3] = (u32)cpu_to_fdt32(g_boot_arg->tee_reserved_mem.size);
	ret = fdt_setprop(fdt, nodeoffset, "reg", trusty_reserved_mem,
			  sizeof(unsigned int) * 4);
	if (ret) {
		dprintf(CRITICAL, "Warning: can't add trusty reg property in device tree\n");
		return 1;
	}
	dprintf(CRITICAL, "trusty-reserved-memory is appended (0x%llx, 0x%llx)\n",
		g_boot_arg->tee_reserved_mem.start, g_boot_arg->tee_reserved_mem.size);

	return ret;
}
#endif

int boot_linux_fdt(void *kernel, unsigned *tags,
		   unsigned machtype,
		   void *ramdisk, unsigned ramdisk_sz)
{
	void *fdt = tags;
	int ret;
	int offset;
	char tmpbuf[TMPBUF_SIZE];
	dt_dram_info mem_reg_property[128];

	int i;
	void (*entry)(unsigned, unsigned, unsigned *) = kernel;
	unsigned int lk_t = 0;
	unsigned int pl_t = 0;
	unsigned int boot_reason = 0;
	char *ptr;
	char spare[FDT_SPARE_SIZE],	/* SPARE_SIZE = BUFF_SIZE + CHCKER_SIZE */
	     *buf 	= spare,
		*checker 	= (spare + FDT_BUFF_SIZE);

	int decompress_outbuf_size = 0x1C00000;
#ifndef USE_ITS_BOOTIMG
	unsigned int zimage_size;
	unsigned int dtb_addr = 0;
	unsigned int dtb_size;
	u32 seed[2];
	void *seedp;
	int seed_len;

	if (g_is_64bit_kernel) {
		unsigned char *magic;
		unsigned int addr;
		unsigned int zimage_addr = (unsigned int)target_get_scratch_address();
		/* to boot k64*/
		dprintf(INFO, "64 bits kernel\n");

		dprintf(INFO, "g_boot_hdr=%p\n", g_boot_hdr);
		dprintf(INFO, "g_boot_hdr->kernel_sz=0x%08x\n", g_boot_hdr->kernel_sz);
		zimage_size = (g_boot_hdr->kernel_sz);

		if (g_boot_hdr->kernel_addr & 0x7FFFF) {
			dprintf(CRITICAL,
				"64 bit kernel can't boot at g_boot_hdr->kernel_addr=0x%08x\n",
				g_boot_hdr->kernel_addr);
			dprintf(CRITICAL, "Please check your bootimg setting\n");
			while (1)
				;
		}

		addr = (unsigned int)(zimage_addr + zimage_size);


		for (dtb_size = 0; dtb_size < zimage_size; dtb_size++, addr--) {
			//FDT_MAGIC 0xd00dfeed ... dtf
			//dtb append after image.gz may not 4 byte alignment
			magic = (unsigned char *)addr;
			if (*(magic + 3) == 0xED &&
			    *(magic + 2) == 0xFE &&
			    *(magic + 1) == 0x0D &&
			    *(magic + 0) == 0xD0)

			{
				dtb_addr = addr;
				dprintf(INFO, "get dtb_addr=0x%08x, dtb_size=0x%08x\n", dtb_addr, dtb_size);
				dprintf(INFO, "copy dtb, fdt=0x%08x\n", (unsigned int)fdt);

				//fix size to 4 byte alignment
				dtb_size = (dtb_size + 0x3) & (~0x3);
#if CFG_DTB_EARLY_LOADER_SUPPORT
				//skip dtb copy, we load dtb in preloader
#else
				memcpy_u8(fdt, (void *)dtb_addr, dtb_size);
#endif
				dtb_addr = (unsigned int)fdt;

				break;
			}
		}

		if (dtb_size != zimage_size)
			zimage_size -= dtb_size;

		else
			dprintf(CRITICAL, "can't find device tree\n");

		dprintf(INFO, "zimage_addr=0x%08x, zimage_size=0x%08x\n", zimage_addr,
			zimage_size);
		dprintf(INFO, "decompress kernel image...\n");

		/* for 64bit decompreesed size.
		 * LK start: 0x41E00000, Kernel Start: 0x40080000
		 * Max is 0x41E00000 - 0x40080000 = 0x1D80000.
		 * using 0x1C00000=28MB for decompressed kernel image size */
#ifdef KERNEL_DECOMPRESS_SIZE
		decompress_outbuf_size = KERNEL_DECOMPRESS_SIZE;
#endif
		if (decompress_kernel((unsigned char *)zimage_addr,
				      (void *)g_boot_hdr->kernel_addr, (int)zimage_size,
				      (int)decompress_outbuf_size)) {
			dprintf(CRITICAL, "decompress kernel image fail!!!\n");
			while (1)
				;
		}
	} else {
		dprintf(INFO, "32 bits kernel\n");
		zimage_size = *(unsigned int *)((unsigned int)kernel + 0x2c) - *
			      (unsigned int *)((unsigned int)kernel + 0x28);
		dtb_addr = (unsigned int)kernel + zimage_size;
		wake_up_iothread();
		wait_for_iothread();
	}

	if (fdt32_to_cpu(*(unsigned int *)dtb_addr) == FDT_MAGIC) {
#if CFG_DTB_EARLY_LOADER_SUPPORT
		dtb_size = fdt32_to_cpu(*(unsigned int *)(fdt + 0x4));
#else
		dtb_size = fdt32_to_cpu(*(unsigned int *)(dtb_addr + 0x4));
#endif
	} else {
		dprintf(CRITICAL, "Can't find device tree. Please check your kernel image\n");
		while (1)
			;
	}
	dprintf(INFO, "dtb_addr = 0x%08X, dtb_size = 0x%08X\n", dtb_addr, dtb_size);

	if (((unsigned int)fdt + dtb_size) > g_fb_base) {
		dprintf(CRITICAL,
			"[ERROR] dtb end address (0x%08X) is beyond the memory (0x%08X).\n",
			(unsigned int)fdt + dtb_size, g_fb_base);
		assert(0);
		return FALSE;
	}
#if CFG_DTB_EARLY_LOADER_SUPPORT
	//skip dtb copy, we load dtb in preloader
#else
	memcpy(fdt, (void *)dtb_addr, dtb_size);
#endif

#else // USE_ITS_BOOTIMG
	if (g_smem_off &&
	    !strncmp(((struct dtb_header *)g_smem_off)->magic, DTB_MAGIC,
		     sizeof(DTB_MAGIC))) {
		unsigned int platform_id = 0, soc_rev = 0,
			     version_id = 0; // These should be got from IDME???

		// multi-dtb found
		struct dtb_entry *dtb_ptr = (struct dtb_entry *)(g_smem_off + sizeof(
						    struct dtb_header));
		for (i = 0; i < (int)((struct dtb_header *)g_smem_off)->num_of_dtbs; i++) {
			// Compare platform/version/soc revision id with current dtb entry
			if (dtb_ptr->platform_id == platform_id && dtb_ptr->soc_rev == soc_rev &&
			    dtb_ptr->version_id == version_id) {
				memcpy(fdt, (void *)(g_smem_off + dtb_ptr->offset), dtb_ptr->size);
				break;
			}
			// Move to next dtb entry
			dtb_ptr ++;
		}
	} else if (g_smem_off)
		memcpy(fdt, (void *)g_smem_off, device_tree_size);
	else
		memcpy(fdt, (void *)&device_tree, device_tree_size);
#endif

	strncpy(checker, FDT_BUFF_END, FDT_CHECKER_SIZE - 1);
	checker[FDT_CHECKER_SIZE - 1] = '\0';

#if CFG_DTB_EARLY_LOADER_SUPPORT
	//skip dtb setup, we setup dtb in platform.c
#else
	ret = setup_fdt(fdt,
			DTB_MAX_SIZE);	//MIN(0x100000, (g_fb_base-(unsigned int)fdt)));
	if (ret == FALSE) {
		assert(0);
		return FALSE;
	}
#endif


	bool rtn = dtb_overlay(fdt, dtb_size);
	UNUSED(rtn);
	dtb_size = fdt32_to_cpu(*(unsigned int *)(fdt + 0x4));

	extern int target_fdt_jtag(void *fdt)__attribute__((weak));
	if (target_fdt_jtag)
		target_fdt_jtag(fdt);

	extern int target_fdt_model(void *fdt)__attribute__((weak));
	if (target_fdt_model)
		target_fdt_model(fdt);

	extern int target_fdt_cpus(void *fdt)__attribute__((weak));
	if (target_fdt_cpus)
		target_fdt_cpus(fdt);

	load_images(g_boot_mode);

#ifdef MTK_SECURITY_ANTI_ROLLBACK
	ret = sec_otp_ver_update();
	imgver_not_sync_warning(g_boot_arg->pl_imgver_status, ret);
#endif

	extern int setup_mem_property_use_mblock_info(dt_dram_info *,
			size_t) __attribute__((weak));
	if (setup_mem_property_use_mblock_info) {
		ret = setup_mem_property_use_mblock_info(
			      &mem_reg_property[0],
			      sizeof(mem_reg_property) / sizeof(dt_dram_info));
		if (ret) {
			assert(0);
			return FALSE;
		}
	} else {
		for (i = 0; i < g_nr_bank; ++i) {
			unsigned int fb_size = (i == g_nr_bank - 1) ? g_fb_size : 0;

#ifndef MTK_LM_MODE
			mem_reg_property[i].start_hi = cpu_to_fdt32(0);
			mem_reg_property[i].start_lo = cpu_to_fdt32(bi_dram[i].start);
			mem_reg_property[i].size_hi = cpu_to_fdt32(0);
			mem_reg_property[i].size_lo = cpu_to_fdt32(bi_dram[i].size - fb_size);

#else
			mem_reg_property[i].start_hi = cpu_to_fdt32(bi_dram[i].start >> 32);
			mem_reg_property[i].start_lo = cpu_to_fdt32(bi_dram[i].start);
			mem_reg_property[i].size_hi = cpu_to_fdt32((bi_dram[i].size - fb_size) >> 32);
			mem_reg_property[i].size_lo = cpu_to_fdt32(bi_dram[i].size - fb_size);

#endif
			dprintf(INFO, " mem_reg_property[%d].start_hi = 0x%08X\n", i,
				mem_reg_property[i].start_hi);
			dprintf(INFO, " mem_reg_property[%d].start_lo = 0x%08X\n", i,
				mem_reg_property[i].start_lo);
			dprintf(INFO, " mem_reg_property[%d].size_hi = 0x%08X\n", i,
				mem_reg_property[i].size_hi);
			dprintf(INFO, " mem_reg_property[%d].size_lo = 0x%08X\n", i,
				mem_reg_property[i].size_lo);
		}
	}

	extern int set_fdt_emi_info(void *fdt)__attribute((weak));
	if (set_fdt_emi_info) {
		ret = set_fdt_emi_info(fdt);
		if (ret)
			dprintf(CRITICAL, "ERROR: EMI info incorrect\n");
	}

	extern int target_fdt_dram_dummy_read(void *fdt,
					      unsigned int rank_num)__attribute__((weak));
	if (target_fdt_dram_dummy_read) {
		ret = target_fdt_dram_dummy_read(fdt, g_nr_bank);
		if (ret)
			dprintf(CRITICAL, "ERROR: DRAM dummy read address incorrect\n");
	}

	extern int set_fdt_dbg_info(void *fdt)__attribute__((weak));
	if (set_fdt_dbg_info) {
		ret = set_fdt_dbg_info(fdt);
		if (ret)
			dprintf(CRITICAL, "ERROR: debug info base and size incorrect\n");
	}
	/*
	 * if there is no memory node exist
	 * we will create a new one
	 */
#if defined(MBLOCK_LIB_SUPPORT) || defined(NEW_MEMORY_RESERVED_MODEL)
	{
		int nodeoffset;
		offset = fdt_path_offset(fdt, "/memory");
		if (offset < 0) {
			offset = fdt_path_offset(fdt, "/");
			if (offset < 0) {
				dprintf(CRITICAL, "ERROR: root node search failed , while(1)\n");
				while (1)
					;
			}
			nodeoffset = fdt_add_subnode(fdt, offset, "memory");
			if (nodeoffset < 0) {
				dprintf(CRITICAL, "ERROR: add subnode memory failed, while(1)\n");
				while (1)
					;
			} else {
				ret = fdt_setprop_string(fdt, nodeoffset, "device_type", "memory");
				dprintf(CRITICAL, "DTS:/memory node is not found create new memory node\n");
			}
		}
	}

	offset = fdt_path_offset(fdt, "/memory");
	if (offset < 0) {
		dprintf(CRITICAL, "ERROR: /memory node not exist, while(1)\n");
		while (1)
			;
	}
#endif

	extern int get_mblock_num(void) __attribute__((weak));

#if defined(MBLOCK_LIB_SUPPORT)
#if	defined(MBLOCK_LIB_SUPPORT) && (MBLOCK_EXPAND(MBLOCK_LIB_SUPPORT) == MBLOCK_EXPAND(2))
	dprintf(CRITICAL, "PASS memory DTS node\n");
	ret = fdt_setprop(fdt, offset, "reg", mem_reg_property, sizeof(dt_dram_info));
#else
	dprintf(CRITICAL, "PASS memory DTS node\n");
	ret = fdt_setprop(fdt, offset, "reg", mem_reg_property,
			  ((int)get_mblock_num ? get_mblock_num() : g_nr_bank) * sizeof(dt_dram_info));
#endif
#else
#if defined(NEW_MEMORY_RESERVED_MODEL)
	dprintf(CRITICAL, "PASS memory DTS node\n");
	ret = fdt_setprop(fdt, offset, "reg", mem_reg_property,
			  ((int)get_mblock_num ? get_mblock_num() : g_nr_bank) * sizeof(dt_dram_info));
#endif
#endif
	if (ret) {
		assert(0);
		return FALSE;
	}

	if (platform_atag_append) {
		ret = platform_atag_append(fdt);
		if (ret) {
			assert(0);
			return FALSE;
		}
	}
#ifdef MBLOCK_LIB_SUPPORT
	ret = fdt_memory_append(fdt);
	if (ret) {
		assert(0);
		return FALSE;
	}
#endif

#if defined(MTK_GOOGLE_TRUSTY_SUPPORT)
	ret = trusty_dts_append(fdt);
	if (ret) {
		assert(0);
		return FALSE;
	}
#endif

	offset = fdt_path_offset(fdt, "/chosen");
	ret = fdt_setprop_cell(fdt, offset, "linux,initrd-start",
			       (unsigned int) ramdisk);
	if (ret) {
		assert(0);
		return FALSE;
	}
	ret = fdt_setprop_cell(fdt, offset, "linux,initrd-end",
			       (unsigned int)ramdisk + ramdisk_sz);
	if (ret) {
		assert(0);
		return FALSE;
	}

	ptr = (char *)target_atag_boot((unsigned *)buf);
	ret = fdt_setprop(fdt, offset, "atag,boot", buf, ptr - buf);
	if (ret) {
		assert(0);
		return FALSE;
	}

	seedp = fdt_getprop(fdt, offset, "kaslr-seed", &seed_len);
	/* get random kaslr-seed if it is defined in the dtb */
	if (seedp) {
		if (seed_len != sizeof(u64)) {
			dprintf(CRITICAL, "incorrect kaslr-seed length=%d\n",
				seed_len);
			assert(0);
			return FALSE;
		}
		get_rnd(&seed[0]);
		get_rnd(&seed[1]);
		ret = fdt_setprop(fdt, offset, "kaslr-seed", seed, seed_len);
		if (ret) {
			assert(0);
			return FALSE;
		}
	}

#if defined(MTK_DLPT_SUPPORT)
	ptr = (char *)target_atag_imix_r((unsigned *)buf);
	ret = fdt_setprop(fdt, offset, "atag,imix_r", buf, ptr - buf);
	if (ret) {
		assert(0);
		return FALSE;
	}
#endif
	snprintf(buf, FDT_BUFF_SIZE, "%d", fg_swocv_v);
	ptr = buf + strlen(buf);
	ret = fdt_setprop(fdt, offset, "atag,fg_swocv_v", buf, ptr - buf);
	dprintf(CRITICAL, "fg_swocv_v buf [%s], [0x%x:0x%x:%d]\n", buf, (unsigned)buf,
		(unsigned)ptr, ptr - buf);

	snprintf(buf, FDT_BUFF_SIZE, "%d", fg_swocv_i);
	ptr = buf + strlen(buf);
	ret = fdt_setprop(fdt, offset, "atag,fg_swocv_i", buf, ptr - buf);
	dprintf(CRITICAL, "fg_swocv_i buf [%s], [0x%x:0x%x:%d]\n", buf, (unsigned)buf,
		(unsigned)ptr, ptr - buf);

	snprintf(buf, FDT_BUFF_SIZE, "%d", shutdown_time);
	ptr = buf + strlen(buf);
	ret = fdt_setprop(fdt, offset, "atag,shutdown_time", buf, ptr - buf);
	dprintf(CRITICAL, "shutdown_time buf [%s], [0x%x:0x%x:%d]\n", buf,
		(unsigned)buf, (unsigned)ptr, ptr - buf);

	snprintf(buf, FDT_BUFF_SIZE, "%d", boot_voltage);
	ptr = buf + strlen(buf);
	ret = fdt_setprop(fdt, offset, "atag,boot_voltage", buf, ptr - buf);
	dprintf(CRITICAL, "boot_voltage buf [%s], [0x%x:0x%x:%d]\n", buf, (unsigned)buf,
		(unsigned)ptr, ptr - buf);

	snprintf(buf, FDT_BUFF_SIZE, "%d", two_sec_reboot);
	ptr = buf + strlen(buf);
	ret = fdt_setprop(fdt, offset, "atag,two_sec_reboot", buf, ptr - buf);
	dprintf(CRITICAL, "boot_voltage buf [%s], [0x%x:0x%x:%d]\n", buf, (unsigned)buf,
		(unsigned)ptr, ptr - buf);

	ptr = (char *)target_atag_mem((unsigned *)buf);
	ret = fdt_setprop(fdt, offset, "atag,mem", buf, ptr - buf);
	if (ret) {
		assert(0);
		return FALSE;
	}

	if (target_atag_partition_data) {
		ptr = (char *)target_atag_partition_data((unsigned *)buf);
		if (ptr != buf) {
			ret = fdt_setprop(fdt, offset, "atag,mem", buf, ptr - buf);
			if (ret) {
				assert(0);
				return FALSE;
			}
		}
	}
#if !(defined(MTK_UFS_BOOTING) || defined(MTK_EMMC_SUPPORT))
	if (target_atag_nand_data) {
		ptr = (char *)target_atag_nand_data((unsigned *)buf);
		if (ptr != buf) {
			ret = fdt_setprop(fdt, offset, "atag,mem", buf, ptr - buf);
			if (ret) {
				assert(0);
				return FALSE;
			}
		}
	}
#endif
	extern unsigned int *target_atag_vcore_dvfs(unsigned * ptr)__attribute__((
				weak));
	if (target_atag_vcore_dvfs) {
		ptr = (char *)target_atag_vcore_dvfs((unsigned *)buf);
		ret = fdt_setprop(fdt, offset, "atag,vcore_dvfs", buf, ptr - buf);
		if (ret) {
			assert(0);
			return FALSE;
		}
	} else
		dprintf(CRITICAL, "Not Support VCORE DVFS\n");

	//some platform might not have this function, use weak reference for
	extern unsigned *target_atag_dfo(unsigned * ptr)__attribute__((weak));
	if (target_atag_dfo) {
		ptr = (char *)target_atag_dfo((unsigned *)buf);
		ret = fdt_setprop(fdt, offset, "atag,dfo", buf, ptr - buf);
		if (ret) {
			assert(0);
			return FALSE;
		}
	}

	if (g_boot_mode == META_BOOT || g_boot_mode == ADVMETA_BOOT ||
	    g_boot_mode == ATE_FACTORY_BOOT || g_boot_mode == FACTORY_BOOT) {
		ptr = (char *)target_atag_meta((unsigned *)buf);
		ret = fdt_setprop(fdt, offset, "atag,meta", buf, ptr - buf);
		if (ret) {
			assert(0);
			return FALSE;
		}
		unsigned int meta_com_id = g_boot_arg->meta_com_id;
		if (g_boot_mode == META_BOOT) {
			int adb = !(meta_com_id & 0x0001);
			int elt = !!(meta_com_id & 0x0004);

			if (!adb && !elt) {
				/*only META*/
				cmdline_append("androidboot.usbconfig=1");
			} else if (adb && !elt) {
				/*META + ADB*/
				cmdline_append("androidboot.usbconfig=0");
			} else if (!adb && elt) {
				/*META + ELT*/
				cmdline_append("androidboot.usbconfig=2");
			} else {
				/*META + ELT + ADB*/
				cmdline_append("androidboot.usbconfig=3");
			}
		} else {
			int adb = !(meta_com_id & 0x0001);
			if (!adb) {
				cmdline_append("androidboot.usbconfig=1");
			} else {
				cmdline_append("androidboot.usbconfig=0");
			}
		}
		if (g_boot_mode == META_BOOT || g_boot_mode == ADVMETA_BOOT) {
			snprintf(tmpbuf, TMPBUF_SIZE, "androidboot.init_rc=%s", META_INIT_RC);
			cmdline_append(tmpbuf);

			if ((meta_com_id & 0x0002) != 0)
				cmdline_append("androidboot.mblogenable=0");

			else
				cmdline_append("androidboot.mblogenable=1");
		} else {
			snprintf(tmpbuf, TMPBUF_SIZE, "androidboot.init_rc=%s", FACTORY_INIT_RC);
			cmdline_append(tmpbuf);
		}
	}

	ptr = (char *)target_atag_devinfo_data((unsigned *)buf);
	ret = fdt_setprop(fdt, offset, "atag,devinfo", buf, ptr - buf);
	if (ret) {
		assert(0);
		return FALSE;
	}

#ifndef MACH_FPGA_NO_DISPLAY
	ptr = (char *)target_atag_videolfb((unsigned *)buf, FDT_BUFF_SIZE);
	ret = fdt_setprop(fdt, offset, "atag,videolfb", buf, ptr - buf);
	if (ret) {
		assert(0);
		return FALSE;
	}

#if (MTK_DUAL_DISPLAY_SUPPORT == 2)
	ptr = (char *)target_atag_ext_videolfb((unsigned *)buf);
	ret = fdt_setprop(fdt, offset, "atag,ext_videolfb", buf, ptr - buf);
	if (ret) {
		assert(0);
		return FALSE;
	}
#endif
#else

	extern int mt_disp_config_frame_buffer(void *fdt)__attribute__((weak));
	if (mt_disp_config_frame_buffer)
		ret = mt_disp_config_frame_buffer(fdt);

#endif

	extern int lastpc_decode(void *fdt)__attribute__((weak));
	if (lastpc_decode) {
		ret = lastpc_decode(fdt);
		if (ret) {
			assert(0);
			return FALSE;
		}
	}

	if (target_atag_mdinfo) {
		ptr = (char *)target_atag_mdinfo((unsigned *)buf);
		ret = fdt_setprop(fdt, offset, "atag,mdinfo", buf, ptr - buf);
		if (ret) {
			assert(0);
			return FALSE;
		}
	} else
		dprintf(CRITICAL, "DFO_MODEN_INFO Only support in MT6582/MT6592\n");

	extern unsigned int *update_md_hdr_info_to_dt(unsigned int *ptr,
			void *fdt)__attribute__((weak));
	if (update_md_hdr_info_to_dt) {
		ptr = (char *)update_md_hdr_info_to_dt((unsigned int *)buf, fdt);
		ret = fdt_setprop(fdt, offset, "ccci,modem_info", buf, ptr - buf);
		if (ret) {
			assert(0);
			return FALSE;
		} else
			dprintf(CRITICAL, "[ccci] create modem mem info DT OK\n");
	} else
		dprintf(CRITICAL, "[ccci] modem mem info not support\n");

	extern unsigned int *update_lk_arg_info_to_dt(unsigned int *ptr,
			void *fdt)__attribute__((weak));
	if (update_lk_arg_info_to_dt) {
		ptr = (char *)update_lk_arg_info_to_dt((unsigned int *)buf, fdt);
		ret = fdt_setprop(fdt, offset, "ccci,modem_info_v2", buf, ptr - buf);
		if (ret) {
			assert(0);
			return FALSE;
		} else
			dprintf(CRITICAL, "[ccci] create modem arguments info DT OK\n");
	} else
		dprintf(CRITICAL, "[ccci] modem mem arguments info using v1\n");

	extern unsigned int *target_atag_ptp(unsigned * ptr)__attribute__((weak));
	if (target_atag_ptp) {
		ptr = (char *)target_atag_ptp((unsigned *)buf);
		ret = fdt_setprop(fdt, offset, "atag,ptp", buf, ptr - buf);
		if (ret) {
			assert(0);
			return FALSE;
		} else
			dprintf(CRITICAL, "Create PTP DT OK\n");
	} else
		dprintf(CRITICAL, "PTP_INFO Only support in MT6795\n");

	if (target_atag_masp_data) {
		ptr = (char *)target_atag_masp_data((unsigned *)buf);
		ret = fdt_setprop(fdt, offset, "atag,masp", buf, ptr - buf);
		if (ret) {
			assert(0);
			return FALSE;
		} else
			dprintf(CRITICAL, "create masp atag OK\n");
	} else
		dprintf(CRITICAL, "masp atag not support in this platform\n");

	extern unsigned int *target_atag_tee(unsigned * ptr)__attribute__((weak));
	if (target_atag_tee) {
		ptr = (char *)target_atag_tee((unsigned *)buf);
		ret = fdt_setprop(fdt, offset, "tee_reserved_mem", buf, ptr - buf);
		if (ret) {
			assert(0);
			return FALSE;
		}
	} else
		dprintf(CRITICAL, "tee_reserved_mem not supported\n");

	extern unsigned int *target_atag_isram(unsigned * ptr)__attribute__((weak));
	if (target_atag_isram) {
		ptr = (char *)target_atag_isram((unsigned *)buf);
		ret = fdt_setprop(fdt, offset, "non_secure_sram", buf, ptr - buf);
		if (ret) {
			assert(0);
			return FALSE;
		}
	} else
		dprintf(CRITICAL, "non_secure_sram not supported\n");

#ifdef MTK_AB_OTA_UPDATER
	/* when boot to recovery, do not append "skip_initramfs" to command line */
	if (g_boot_mode != RECOVERY_BOOT)
		cmdline_append("skip_initramfs");

	snprintf(tmpbuf, TMPBUF_SIZE, "system%s", p_AB_suffix);
	part_t *part = mt_part_get_partition(tmpbuf);

	#ifdef MTK_DM_VERITY_OFF
	snprintf(tmpbuf, TMPBUF_SIZE,
		 "rootwait ro init=/init root=PARTUUID=%s \" androidboot.slot_suffix=%s",
		 part->info->uuid, p_AB_suffix);
	#else
	snprintf(tmpbuf, TMPBUF_SIZE,
		 "rootwait ro init=/init root=/dev/dm-0 dm=\"system none ro,0 1 android-verity PARTUUID=%s \" androidboot.slot_suffix=%s",
		 part->info->uuid, p_AB_suffix);
	#endif

	cmdline_append(tmpbuf);
	switch (eBuildType) {
	case BUILD_TYPE_USER:
		cmdline_append("buildvariant=user");
		break;

	case BUILD_TYPE_USERDEBUG:
		cmdline_append("buildvariant=userdebug");
		break;

	case BUILD_TYPE_ENG:
		cmdline_append("buildvariant=eng");
		break;

	default:
		assert(0);
		break;
	}
#endif /* MTK_AB_OTA_UPDATER */

	if (!has_set_p2u) {
		switch (eBuildType) {
		case BUILD_TYPE_USER:
			if ((g_boot_mode == META_BOOT) && is_meta_log_disable &&
			    (is_meta_log_disable() == 0))
				cmdline_append("printk.disable_uart=0");
			else
				cmdline_append("printk.disable_uart=1");
			break;

		case BUILD_TYPE_USERDEBUG:
			if ((g_boot_mode == META_BOOT) && is_meta_log_disable &&
			    (is_meta_log_disable() == 1))
				cmdline_append("printk.disable_uart=1 slub_debug=O");
			else
				cmdline_append("printk.disable_uart=0");
			break;

		case BUILD_TYPE_ENG:
			if ((g_boot_mode == META_BOOT) && is_meta_log_disable &&
			    (is_meta_log_disable() == 1))
				cmdline_append("printk.disable_uart=1 slub_debug=O");
			else
				cmdline_append("printk.disable_uart=0 ddebug_query=\"file *mediatek* +p ; file *gpu* =_\"");
			break;

		default:
			assert(0);
			break;
		}

		/*Append pre-loader boot time to kernel command line*/
		pl_t = g_boot_arg->boot_time;
		snprintf(tmpbuf, TMPBUF_SIZE, "%s%d", "bootprof.pl_t=", pl_t);
		cmdline_append(tmpbuf);
		/*Append lk boot time to kernel command line*/
		lk_t = ((unsigned int)get_timer(boot_time));
		snprintf(tmpbuf, TMPBUF_SIZE, "%s%d", "bootprof.lk_t=", lk_t);
		cmdline_append(tmpbuf);

		if (logo_lk_t != 0) {
			dprintf(CRITICAL, "[PROFILE] 1st logo takes %d ms\n", logo_lk_t);
			snprintf(tmpbuf, TMPBUF_SIZE, "%s%d", "bootprof.logo_t=", logo_lk_t);
			cmdline_append(tmpbuf);
		}

		PROFILING_PRINTF("1st logo time %d ms", logo_lk_t);
		PROFILING_PRINTF("boot_time takes %d ms", lk_t);
	}
	/*Append pre-loader boot reason to kernel command line*/
#ifdef MTK_KERNEL_POWER_OFF_CHARGING
	if (g_boot_reason_change)
		boot_reason = 4;

	else
#endif
	{
		boot_reason = g_boot_arg->boot_reason;
	}
	snprintf(tmpbuf, TMPBUF_SIZE, "%s%d", "boot_reason=", boot_reason);
	cmdline_append(tmpbuf);

	/* Append androidboot.serialno=xxxxyyyyzzzz in cmdline */
	snprintf(tmpbuf, TMPBUF_SIZE, "%s%s", "androidboot.serialno=", sn_buf);
	cmdline_append(tmpbuf);

	snprintf(tmpbuf, TMPBUF_SIZE, "%s%s",  "androidboot.bootreason=",
		 g_boot_reason[boot_reason]);
	cmdline_append(tmpbuf);


	extern unsigned int *target_commandline_force_gpt(char *cmd)__attribute__((
				weak));
	if (target_commandline_force_gpt)
		target_commandline_force_gpt((char *)cmdline_get());

	extern void ccci_append_tel_fo_setting(char *cmdline)__attribute__((weak));
	if (ccci_append_tel_fo_setting)
		ccci_append_tel_fo_setting((char *)cmdline_get());

	if (eBuildType == BUILD_TYPE_ENG)
		cmdline_append("initcall_debug=1");

	extern int dfd_set_base_addr(void *fdt)__attribute__((weak));
	if (dfd_set_base_addr) {
		ret = dfd_set_base_addr(fdt);
		if (ret)
			dprintf(CRITICAL, "[DFD] failed to get base address (%d)\n", ret);
	}

	extern unsigned int get_usb2jtag(void) __attribute__((weak));
	extern unsigned int set_usb2jtag(unsigned int en)  __attribute__((weak));
	if (get_usb2jtag) {
		if (get_usb2jtag() == 1)
			cmdline_append("usb2jtag_mode=1");
		else
			cmdline_append("usb2jtag_mode=0");
	}

	check_hibernation();

	/*DTS memory will be modified during lk boot process
	 * so we need to put the cmdline in the last mile*/
	mrdump_append_cmdline();


	ptr = (char *)target_atag_commandline((u8 *)buf, FDT_BUFF_SIZE,
					      (const char *)cmdline_get());
	ret = fdt_setprop(fdt, offset, "atag,cmdline", buf, ptr - buf);
	if (ret) {
		assert(0);
		return FALSE;
	}

	ret = fdt_setprop_string(fdt, offset, "bootargs", (char *)cmdline_get());
	if (ret) {
		assert(0);
		return FALSE;
	}

	extern int *target_fdt_firmware(void *fdt, char *serialno)__attribute__((weak));
	if (target_fdt_firmware)
		target_fdt_firmware(fdt, sn_buf);

	/* This depends on target_fdt_firmware, must after target_fdt_firmware */
	extern int update_md_opt_to_fdt_firmware(void *fdt)__attribute__((weak));
	if (update_md_opt_to_fdt_firmware)
		update_md_opt_to_fdt_firmware(fdt);

	/* Return the bootimg mb before stepping into kernel */
	mboot_free_bootimg_from_mblock();

#if defined(MBLOCK_LIB_SUPPORT)
#if	defined(MBLOCK_LIB_SUPPORT) && (MBLOCK_EXPAND(MBLOCK_LIB_SUPPORT) == MBLOCK_EXPAND(2))
	/* this should be proper place for mblock memory santiy check*/
	ret = mblock_sanity_check(fdt, &g_boot_arg->mblock_info,
				  &g_boot_arg->orig_dram_info);
	if (ret) {
		assert(0);
		return FALSE;
	}

	ret = mblock_reserved_append(fdt);
	if (ret) {
		assert(0);
		return FALSE;
	}
#endif
#endif

#if WITH_GZ
	entry = g_boot_arg->gz_addr;
	create_gz_dt_node(fdt);
#endif

	ret = fdt_pack(fdt);
	if (ret) {
		assert(0);
		return FALSE;
	}

	dprintf(CRITICAL, "booting linux @ %p, ramdisk @ %p (%d)\n",
		kernel, ramdisk, ramdisk_sz);

	if (strncmp(checker, FDT_BUFF_END, FDT_CHECKER_SIZE) != 0) {
		dprintf(CRITICAL, "ERROR: fdt buff overflow\n");
		assert(0);
		return FALSE;
	}

	enter_critical_section();
	/* do any platform specific cleanup before kernel entry */
	platform_uninit();
#ifdef HAVE_CACHE_PL310
	l2_disable();
#endif

	arch_disable_cache(UCACHE);
	arch_disable_mmu();

#ifndef MACH_FPGA
	extern void platform_sec_post_init(void)__attribute__((weak));
	if (platform_sec_post_init)
		platform_sec_post_init();
#endif

	/*Prevent the system jumps to Kernel if we unplugged Charger/USB before*/
	if (kernel_charging_boot) {
		if (kernel_charging_boot() == -1) {
			dprintf(CRITICAL,
				"[%s] Unplugged Usb/Charger in Kernel Charging Mode Before Jumping to Kernel, Power Off\n",
				__func__);
#ifndef NO_POWER_OFF
			mt6575_power_off();
#endif
		}
		if (kernel_charging_boot() == 1) {
			if (pmic_detect_powerkey()) {
				dprintf(CRITICAL,
					"[%s] PowerKey Pressed in Kernel Charging Mode Before Jumping to Kernel, Reboot Os\n",
					__func__);
				//mt65xx_backlight_off();
				//mt_disp_power(0);
				mtk_arch_reset(1);
			}
		}
	}

	dprintf(CRITICAL, "DRAM Rank :%d\n", g_nr_bank);
	for (i = 0; i < g_nr_bank; i++) {
#ifndef MTK_LM_MODE
		dprintf(CRITICAL, "DRAM Rank[%d] Start = 0x%x, Size = 0x%x\n", i,
			(unsigned int)bi_dram[i].start, (unsigned int)bi_dram[i].size);
#else
		dprintf(CRITICAL, "DRAM Rank[%d] Start = 0x%llx, Size = 0x%llx\n", i,
			bi_dram[i].start, bi_dram[i].size);
#endif
	}

#ifdef MBLOCK_LIB_SUPPORT
	mblock_show_info();
#endif

	/*
	 * Kick watchdog before leaving lk to avoid watchdog reset if
	 * kernel initialization has longer execution time.
	 *
	 * Of course watchdog will still be triggered if kernel hangs
	 * because watchdog is still alive.
	 */
	mtk_wdt_restart();

	dprintf(CRITICAL, "cmdline: %s\n", (char *)cmdline_get());
	dprintf(CRITICAL, "lk boot time = %d ms\n", lk_t);
	dprintf(CRITICAL, "lk boot mode = %d\n", g_boot_mode);
	dprintf(CRITICAL, "lk boot reason = %s\n", g_boot_reason[boot_reason]);
	dprintf(CRITICAL, "lk finished --> jump to linux kernel %s\n\n",
		g_is_64bit_kernel ? "64Bit" : "32Bit");
	if (Debug_log_EMI_MPU)
		Debug_log_EMI_MPU();

	if (g_is_64bit_kernel)
		lk_jump64((u32)entry, (u32)tags, 0, KERNEL_64BITS);

	else {
#ifdef MTK_SMC_K32_SUPPORT
		lk_jump64((u32)entry, (u32)machtype, (u32)tags, KERNEL_32BITS);
#else
		entry(0, machtype, tags);
#endif
	}
	while (1);
	return 0;
}



#endif // DEVICE_TREE_SUPPORT

void boot_linux(void *kernel, unsigned *tags,
		unsigned machtype,
		void *ramdisk, unsigned ramdisk_sz)
{
	int i;
	unsigned *ptr = tags;
	void (*entry)(unsigned, unsigned, unsigned *) = kernel;
	unsigned int lk_t = 0;
	unsigned int pl_t = 0;
	unsigned int boot_reason = 0;
	char tmpbuf[TMPBUF_SIZE];

#ifdef DEVICE_TREE_SUPPORT
	boot_linux_fdt((void *)kernel, (unsigned *)tags,
		       machtype,
		       (void *)ramdisk, ramdisk_sz);

	while (1) ;
#endif

#ifdef MTK_SECURITY_ANTI_ROLLBACK
	int ret = sec_otp_ver_update();
	imgver_not_sync_warning(g_boot_arg->pl_imgver_status, ret);
#endif


	/* CORE */
	*ptr++ = 2;
	*ptr++ = 0x54410001;

	ptr = target_atag_boot(ptr);
	ptr = target_atag_mem(ptr);

	if (target_atag_partition_data)
		ptr = target_atag_partition_data(ptr);
#if !(defined(MTK_UFS_BOOTING) || defined(MTK_EMMC_SUPPORT))
	if (target_atag_nand_data)
		ptr = target_atag_nand_data(ptr);
#endif
	//some platform might not have this function, use weak reference for
	extern unsigned *target_atag_dfo(unsigned * ptr)__attribute__((weak));
	if (target_atag_dfo)
		ptr = target_atag_dfo(ptr);

	if (g_boot_mode == META_BOOT || g_boot_mode == ADVMETA_BOOT ||
	    g_boot_mode == ATE_FACTORY_BOOT || g_boot_mode == FACTORY_BOOT) {
		ptr = target_atag_meta(ptr);
		unsigned int meta_com_id = g_boot_arg->meta_com_id;
		if ((meta_com_id & 0x0001) != 0)
			cmdline_append("androidboot.usbconfig=1");

		else
			cmdline_append("androidboot.usbconfig=0");
		if (g_boot_mode == META_BOOT || g_boot_mode == ADVMETA_BOOT) {
			snprintf(tmpbuf, TMPBUF_SIZE, "androidboot.init_rc=%s", META_INIT_RC);
			cmdline_append(tmpbuf);
			if ((meta_com_id & 0x0002) != 0)
				cmdline_append("androidboot.mblogenable=0");

			else
				cmdline_append("androidboot.mblogenable=1");
		} else {
			snprintf(tmpbuf, TMPBUF_SIZE, "androidboot.init_rc=%s", FACTORY_INIT_RC);
			cmdline_append(tmpbuf);
		}
	}

	ptr = target_atag_devinfo_data(ptr);

	/*Append pre-loader boot time to kernel command line*/
	pl_t = g_boot_arg->boot_time;
	snprintf(tmpbuf, TMPBUF_SIZE, "%s%d", "bootprof.pl_t=", pl_t);
	cmdline_append(tmpbuf);

	/*Append lk boot time to kernel command line*/
	lk_t = ((unsigned int)get_timer(boot_time));
	snprintf(tmpbuf, TMPBUF_SIZE, "%s%d", "bootprof.lk_t=", lk_t);
	cmdline_append(tmpbuf);

	if (logo_lk_t != 0) {
		dprintf(CRITICAL, "[PROFILE] 1st logo takes %d ms\n", logo_lk_t);
		snprintf(tmpbuf, TMPBUF_SIZE, "%s%d", "bootprof.logo_t=", logo_lk_t);
		cmdline_append(tmpbuf);
	}

	PROFILING_PRINTF("1st logo %d ms", logo_lk_t);
	PROFILING_PRINTF("boot_time takes %d ms", lk_t);

	if (!has_set_p2u) {
		switch (eBuildType) {
		case BUILD_TYPE_USER:
			cmdline_append("printk.disable_uart=1");
			break;

		case BUILD_TYPE_USERDEBUG:
			cmdline_append("printk.disable_uart=0");
			break;

		case BUILD_TYPE_ENG:
			cmdline_append("printk.disable_uart=0 ddebug_query=\"file *mediatek* +p ; file *gpu* =_\"");
			break;

		default:
			assert(0);
			break;
		}
	}

	/*Append pre-loader boot reason to kernel command line*/
	if (g_boot_reason_change)
		boot_reason = 4;
	else
		boot_reason = g_boot_arg->boot_reason;

	snprintf(tmpbuf, TMPBUF_SIZE, "%s%d", "boot_reason=", boot_reason);
	cmdline_append(tmpbuf);

	/* Append androidboot.serialno=xxxxyyyyzzzz in cmdline */
	snprintf(tmpbuf, TMPBUF_SIZE, "%s%s", "androidboot.serialno=", sn_buf);
	cmdline_append(tmpbuf);

	snprintf(tmpbuf, TMPBUF_SIZE, "%s%s",  "androidboot.bootreason=",
		 g_boot_reason[boot_reason]);
	cmdline_append(tmpbuf);

	check_hibernation();

	/*
	 * FIXME or LIMITATION:
	 * ptr is a historical parameter, which is used to locate the
	 * ATAG in memory without information of length in memory.
	 * Here is compromised to the target_atag_commandline API with
	 * command length plus the size of the header.
	 *
	 * ROUNDUP(sizeof(struct tag_header) + strlen(cmdline_get())+1, 4)
	 */
	ptr = (unsigned *)target_atag_commandline((u8 *)ptr,
			ROUNDUP(sizeof(struct tag_header) + strlen(cmdline_get()) + 1, 4),
			(const char *)cmdline_get());

	ptr = target_atag_initrd(ptr, (unsigned long) ramdisk, ramdisk_sz);
	ptr = target_atag_videolfb(ptr,
				   ROUNDUP(VIDEOLFB_PRE_HEADER_LENGTH * sizeof(unsigned) + strlen(
						   mt_disp_get_lcm_id()) + 1, 4));
#if (MTK_DUAL_DISPLAY_SUPPORT == 2)
	ptr = target_atag_ext_videolfb(ptr);
#endif

	extern unsigned int *target_atag_mdinfo(unsigned * ptr)__attribute__((weak));
	if (target_atag_mdinfo)
		ptr = target_atag_mdinfo(ptr);

	else
		dprintf(CRITICAL, "DFO_MODEN_INFO Only support in MT6582/MT6592\n");

	if (target_atag_masp_data)
		ptr = target_atag_masp_data((unsigned *)ptr);

	else
		dprintf(CRITICAL, "masp atag not support in this platform\n");

	/* END */
	*ptr++ = 0;
	*ptr++ = 0;

#if 0
	dprintf(CRITICAL, "atag start:0x%08X, end:0x%08X\n", tags, ptr);
	for (unsigned int *scan = tags, i = 0; scan != ptr; scan++, i++)
		dprintf(CRITICAL, "0x%08X%c", *scan, (i % 4 == 0) ? '\n' : '\t');
	dprintf(CRITICAL, "\n");
#endif

	dprintf(CRITICAL, "booting linux @ %p, ramdisk @ %p (%d)\n",
		kernel, ramdisk, ramdisk_sz);

	enter_critical_section();
	/* do any platform specific cleanup before kernel entry */
	platform_uninit();
#ifdef HAVE_CACHE_PL310
	l2_disable();
#endif

	arch_disable_cache(UCACHE);
	arch_disable_mmu();

	extern void platform_sec_post_init(void)__attribute__((weak));
	if (platform_sec_post_init)
		platform_sec_post_init();

	arch_uninit();

	/*Prevent the system jumps to Kernel if we unplugged Charger/USB before*/
	if (kernel_charging_boot) {
		if (kernel_charging_boot() == -1) {
			dprintf(CRITICAL,
				"[%s] Unplugged Usb/Charger in Kernel Charging Mode Before Jumping to Kernel, Power Off\n",
				__func__);
#ifndef NO_POWER_OFF
			mt6575_power_off();
#endif
		}
		if (kernel_charging_boot() == 1) {
			if (pmic_detect_powerkey()) {
				dprintf(CRITICAL,
					"[%s] PowerKey Pressed in Kernel Charging Mode Before Jumping to Kernel, Reboot Os\n",
					__func__);
				//mt65xx_backlight_off();
				//mt_disp_power(0);
				mtk_arch_reset(1);
			}
		}
	}

	dprintf(CRITICAL, "DRAM Rank :%d\n", g_nr_bank);
	for (i = 0; i < g_nr_bank; i++) {
#ifndef MTK_LM_MODE
		dprintf(CRITICAL, "DRAM Rank[%d] Start = 0x%x, Size = 0x%x\n", i,
			(unsigned int)bi_dram[i].start, (unsigned int)bi_dram[i].size);
#else
		dprintf(CRITICAL, "DRAM Rank[%d] Start = 0x%llx, Size = 0x%llx\n", i,
			bi_dram[i].start, bi_dram[i].size);
#endif
	}

#ifdef MBLOCK_LIB_SUPPORT
	mblock_show_info();
#endif

	/*
	 * Kick watchdog before leaving lk to avoid watchdog reset if
	 * kernel initialization has longer execution time.
	 *
	 * Of course watchdog will still be triggered if kernel hangs
	 * because watchdog is still alive.
	 */
	mtk_wdt_restart();

	dprintf(CRITICAL, "cmdline: %s\n", (char *)cmdline_get());
	dprintf(CRITICAL, "lk boot time = %d ms\n", lk_t);
	if (logo_lk_t != 0)
		dprintf(CRITICAL, "[PROFILE] 1st logo takes %d ms\n", logo_lk_t);
	dprintf(CRITICAL, "lk boot mode = %d\n", g_boot_mode);
	dprintf(CRITICAL, "lk boot reason = %s\n", g_boot_reason[boot_reason]);
	dprintf(CRITICAL, "lk finished --> jump to linux kernel\n\n");
	entry(0, machtype, tags);
}

#ifdef MTK_AB_OTA_UPDATER
static void load_bootimg_by_suffix(void)
{
	int ret;
	unsigned int kimg_load_addr;
	char bootimg_name[16];
	int is_normal_boot;
#if defined(CFG_NAND_BOOT)
	char cmdline_tmpbuf[100];
#endif

	/* set normal mode when retry count > 0 */
	if (AB_retry_count > 0) {
		switch (g_boot_mode) {
		case NORMAL_BOOT:
		case ALARM_BOOT:
			is_normal_boot = 1;
			break;
		case META_BOOT:
		case ADVMETA_BOOT:
		case SW_REBOOT:
#if defined(MTK_KERNEL_POWER_OFF_CHARGING) || defined(MTK_CHARGER_NEW_ARCH)
		case KERNEL_POWER_OFF_CHARGING_BOOT:
		case LOW_POWER_OFF_CHARGING_BOOT:
#endif
		case FACTORY_BOOT:
		case ATE_FACTORY_BOOT:
		case RECOVERY_BOOT:
		case FASTBOOT:
		case DOWNLOAD_BOOT:
		case UNKNOWN_BOOT:
		default:
			is_normal_boot = 0;
			break;
		}

		ret = set_normal_boot(p_AB_suffix, is_normal_boot);
		dprintf(CRITICAL, "[%s:%d] set_normal_boot(%s, %d): %d\n", __func__, __LINE__,
			p_AB_suffix, is_normal_boot, ret);
	}

	/* load bootimg with suffix: boot_a or boot_b */
#ifdef MTK_GPT_SCHEME_SUPPORT
	snprintf(bootimg_name, sizeof(bootimg_name), "boot%s", p_AB_suffix);
#else
	snprintf(bootimg_name, sizeof(bootimg_name), "%s%s", PART_BOOTIMG, p_AB_suffix);
#endif
	PROFILING_START(bootimg_name);
	dprintf(INFO, "[%s:%d] bootimg_name: %s\n", __func__, __LINE__, bootimg_name);

#if defined(CFG_NAND_BOOT)
	snprintf(cmdline_tmpbuf, sizeof(cmdline_tmpbuf), "%s%x%s%x", NAND_MANF_CMDLINE,
		 nand_flash_man_code, NAND_DEV_CMDLINE, nand_flash_dev_id);
	cmdline_append(cmdline_tmpbuf);
#endif

	ret = mboot_android_load_bootimg_hdr(bootimg_name, CFG_BOOTIMG_LOAD_ADDR);

	if (ret < 0)
		msg_header_error("Android Boot Image");

	if (g_is_64bit_kernel)
		kimg_load_addr = (unsigned int)target_get_scratch_address();

	else
		kimg_load_addr = (g_boot_hdr != NULL) ? g_boot_hdr->kernel_addr :
				 CFG_BOOTIMG_LOAD_ADDR;

	ret = mboot_android_load_bootimg(bootimg_name, kimg_load_addr);

	if (ret < 0)
		msg_img_error("Android Boot Image");

	PROFILING_END();
}

void get_AB_OTA_param(void)
{
	p_AB_suffix = get_suffix();
	AB_retry_count = get_retry_count(p_AB_suffix);
	dprintf(CRITICAL, "[%s:%d] p_AB_suffix: %s, AB_retry_count: %d\n", __func__,
		__LINE__, p_AB_suffix, AB_retry_count);
}

void get_AB_OTA_name(char *part_name, int size)
{
	if (!p_AB_suffix)
		get_AB_OTA_param();
	snprintf(part_name, size, "%s%s", part_name, p_AB_suffix);
}
#endif /* MTK_AB_OTA_UPDATER */

int boot_linux_from_storage(void)
{
	int ret = 0;
#define CMDLINE_TMP_CONCAT_SIZE 110     //only for string concat, 200 bytes is enough
	char cmdline_tmpbuf[CMDLINE_TMP_CONCAT_SIZE];
	unsigned int kimg_load_addr;

#ifdef MTK_AB_OTA_UPDATER
	load_bootimg_by_suffix();
#else
	switch (g_boot_mode) {
	case NORMAL_BOOT:
	case META_BOOT:
	case ADVMETA_BOOT:
	case SW_REBOOT:
	case ALARM_BOOT:
#ifdef MTK_KERNEL_POWER_OFF_CHARGING
	case KERNEL_POWER_OFF_CHARGING_BOOT:
	case LOW_POWER_OFF_CHARGING_BOOT:
#endif
		PROFILING_START("load boot.img");
#if defined(CFG_NAND_BOOT)
		snprintf(cmdline_tmpbuf, CMDLINE_TMP_CONCAT_SIZE, "%s%x%s%x",
			 NAND_MANF_CMDLINE, nand_flash_man_code, NAND_DEV_CMDLINE, nand_flash_dev_id);
		cmdline_append(cmdline_tmpbuf);
#endif
#ifdef MTK_GPT_SCHEME_SUPPORT
        if (mtk_detect_key(17) && mtk_detect_key(8)) // 8 = POWER KEY
            ret = mboot_android_load_bootimg_hdr("boot3", CFG_BOOTIMG_LOAD_ADDR);
        else if (mtk_detect_key(17)) // 17 = SIDE BUTTON KEY
            ret = mboot_android_load_bootimg_hdr("boot2", CFG_BOOTIMG_LOAD_ADDR);
        else
            ret = mboot_android_load_bootimg_hdr("boot", CFG_BOOTIMG_LOAD_ADDR);
#else
		ret = mboot_android_load_bootimg_hdr(PART_BOOTIMG, CFG_BOOTIMG_LOAD_ADDR);
#endif
		if (ret < 0)
			msg_header_error("Android Boot Image");

		if (g_is_64bit_kernel)
			kimg_load_addr = (unsigned int)target_get_scratch_address();

		else
			kimg_load_addr = (g_boot_hdr != NULL) ? g_boot_hdr->kernel_addr :
					 CFG_BOOTIMG_LOAD_ADDR;

#ifdef MTK_GPT_SCHEME_SUPPORT
        if (mtk_detect_key(17) && mtk_detect_key(8))
            ret = mboot_android_load_bootimg("boot3", kimg_load_addr);
        else if (mtk_detect_key(17))
            ret = mboot_android_load_bootimg("boot2", kimg_load_addr);
        else
            ret = mboot_android_load_bootimg("boot", kimg_load_addr);
#else
		ret = mboot_android_load_bootimg(PART_BOOTIMG, kimg_load_addr);
#endif

		if (ret < 0)
			msg_img_error("Android Boot Image");

		PROFILING_END();
		break;

	case RECOVERY_BOOT:
		PROFILING_START("load recovery.img"); /* recovery */
#ifdef MTK_GPT_SCHEME_SUPPORT
		ret = mboot_android_load_recoveryimg_hdr("recovery", CFG_BOOTIMG_LOAD_ADDR);
#else
		ret = mboot_android_load_recoveryimg_hdr(PART_RECOVERY, CFG_BOOTIMG_LOAD_ADDR);
#endif
		if (ret < 0)
			msg_header_error("Android Recovery Image");

		if (g_is_64bit_kernel)
			kimg_load_addr = (unsigned int)target_get_scratch_address();

		else
			kimg_load_addr = (g_boot_hdr != NULL) ? g_boot_hdr->kernel_addr :
					 CFG_BOOTIMG_LOAD_ADDR;

#ifdef MTK_GPT_SCHEME_SUPPORT
		ret = mboot_android_load_recoveryimg("recovery", kimg_load_addr);
#else
		ret = mboot_android_load_recoveryimg(PART_RECOVERY, kimg_load_addr);
#endif
		if (ret < 0)
			msg_img_error("Android Recovery Image");

		PROFILING_END();
		break;

	case FACTORY_BOOT:
	case ATE_FACTORY_BOOT:
		PROFILING_START("load factory.img");
#if defined(CFG_NAND_BOOT)
		snprintf(cmdline_tmpbuf, CMDLINE_TMP_CONCAT_SIZE, "%s%x%s%x",
			 NAND_MANF_CMDLINE, nand_flash_man_code, NAND_DEV_CMDLINE, nand_flash_dev_id);
		cmdline_append(cmdline_tmpbuf);
#endif
#ifdef MTK_GPT_SCHEME_SUPPORT
        if (mtk_detect_key(17) && mtk_detect_key(8))
            ret = mboot_android_load_bootimg_hdr("boot3", CFG_BOOTIMG_LOAD_ADDR);
        else if (mtk_detect_key(17))
            ret = mboot_android_load_bootimg_hdr("boot2", CFG_BOOTIMG_LOAD_ADDR);
        else
            ret = mboot_android_load_bootimg_hdr("boot", CFG_BOOTIMG_LOAD_ADDR);
#else
		ret = mboot_android_load_bootimg_hdr(PART_BOOTIMG, CFG_BOOTIMG_LOAD_ADDR);
#endif
		if (ret < 0)
			msg_header_error("Android Boot Image");

		if (g_is_64bit_kernel)
			kimg_load_addr = (unsigned int)target_get_scratch_address();

		else
			kimg_load_addr = (g_boot_hdr != NULL) ? g_boot_hdr->kernel_addr :
					 CFG_BOOTIMG_LOAD_ADDR;

#ifdef MTK_GPT_SCHEME_SUPPORT
        if (mtk_detect_key(17) && mtk_detect_key(8))
            ret = mboot_android_load_bootimg("boot3", kimg_load_addr);
        else if (mtk_detect_key(17))
            ret = mboot_android_load_bootimg("boot2", kimg_load_addr);
        else
            ret = mboot_android_load_bootimg("boot", kimg_load_addr);
#else
		ret = mboot_android_load_bootimg(PART_BOOTIMG, kimg_load_addr);
#endif
		if (ret < 0)
			msg_img_error("Android Boot Image");

		PROFILING_END();
		break;

	case FASTBOOT:
	case DOWNLOAD_BOOT:
	case UNKNOWN_BOOT:
		break;

	}
#endif /* MTK_AB_OTA_UPDATER */

#ifndef SKIP_LOADING_RAMDISK
	if (g_rimg_sz == 0) {
		if (g_boot_hdr != NULL)
			g_rimg_sz = g_boot_hdr->ramdisk_sz;
	}
#ifdef MTK_3LEVEL_PAGETABLE
	/* rootfs addr */
	if (g_boot_hdr != NULL) {
		arch_mmu_map((uint64_t)g_boot_hdr->ramdisk_addr,
			(uint32_t)g_boot_hdr->ramdisk_addr,
			MMU_MEMORY_TYPE_NORMAL_WRITE_BACK | MMU_MEMORY_AP_P_RW_U_NA,
			ROUNDUP(g_rimg_sz, PAGE_SIZE));
	}
#endif
	/* relocate rootfs (ignore rootfs header) */
	memcpy((g_boot_hdr != NULL) ? (char *)g_boot_hdr->ramdisk_addr :
	       (char *)CFG_RAMDISK_LOAD_ADDR, (char *)(g_rmem_off), g_rimg_sz);
	g_rmem_off = (g_boot_hdr != NULL) ? g_boot_hdr->ramdisk_addr :
		     CFG_RAMDISK_LOAD_ADDR;
#endif


	// 2 weak function for mt6572 memory preserved mode
	platform_mem_preserved_load_img();
	platform_mem_preserved_dump_mem();

	custom_port_in_kernel(g_boot_mode, cmdline_get());

	if (g_boot_hdr != NULL) {
		cmdline_append(g_boot_hdr->cmdline);
	}

#ifdef SELINUX_STATUS
#if SELINUX_STATUS == 1
	cmdline_append("androidboot.selinux=disabled");
#elif SELINUX_STATUS == 2
	cmdline_append("androidboot.selinux=permissive");
#endif
#endif
	/* set verity mode to 'enforcing' in order to make dm-verity work. */
	/* after verity mode handling is implemented, please remove this line */
	cmdline_append("androidboot.veritymode=enforcing");

#if defined(MTK_POWER_ON_WRITE_PROTECT) && !defined(MACH_FPGA)
#if MTK_POWER_ON_WRITE_PROTECT == 1
#if defined(MTK_EMMC_SUPPORT) || defined(MTK_UFS_BOOTING)
	write_protect_flow();
#endif
#endif
#endif

	/* pass related root of trust info via SMC call */
	if (send_root_of_trust_info != NULL)
		send_root_of_trust_info();

	if (g_boot_hdr != NULL) {
		boot_linux((void *)g_boot_hdr->kernel_addr, (unsigned *)g_boot_hdr->tags_addr,
			   board_machtype(), (void *)g_boot_hdr->ramdisk_addr, g_rimg_sz);
	} else {
		boot_linux((void *)CFG_BOOTIMG_LOAD_ADDR, (unsigned *)CFG_BOOTARGS_ADDR,
			   board_machtype(), (void *)CFG_RAMDISK_LOAD_ADDR, g_rimg_sz);
	}

	while (1) ;

	return 0;
}

#if defined(CONFIG_MTK_USB_UNIQUE_SERIAL) || (defined(MTK_SECURITY_SW_SUPPORT) && defined(MTK_SEC_FASTBOOT_UNLOCK_SUPPORT))
static char udc_chr[32] = {"ABCDEFGHIJKLMNOPQRSTUVWSYZ456789"};

int get_serial(u64 hwkey, u32 chipid, char ser[SERIALNO_LEN])
{
	u16 hashkey[4];
	u32 idx, ser_idx;
	u32 digit, id;
	u64 tmp = hwkey;

	memset(ser, 0x00, SERIALNO_LEN);

	/* split to 4 key with 16-bit width each */
	tmp = hwkey;
	for (idx = 0; idx < ARRAY_SIZE(hashkey); idx++) {
		hashkey[idx] = (u16)(tmp & 0xffff);
		tmp >>= 16;
	}

	/* hash the key with chip id */
	id = chipid;
	for (idx = 0; idx < ARRAY_SIZE(hashkey); idx++) {
		digit = (id % 10);
		hashkey[idx] = (hashkey[idx] >> digit) | (hashkey[idx] << (16 - digit));
		id = (id / 10);
	}

	/* generate serail using hashkey */
	ser_idx = 0;
	for (idx = 0; idx < ARRAY_SIZE(hashkey); idx++) {
		ser[ser_idx++] = (hashkey[idx] & 0x001f);
		ser[ser_idx++] = (hashkey[idx] & 0x00f8) >> 3;
		ser[ser_idx++] = (hashkey[idx] & 0x1f00) >> 8;
		ser[ser_idx++] = (hashkey[idx] & 0xf800) >> 11;
	}
	for (idx = 0; idx < ser_idx; idx++)
		ser[idx] = udc_chr[(int)ser[idx]];
	ser[ser_idx] = 0x00;
	return 0;
}
#endif /* CONFIG_MTK_USB_UNIQUE_SERIAL */

#ifdef SERIAL_NUM_FROM_BARCODE
static inline int read_product_info(char *buf)
{
	int tmp = 0;

	if (!buf) return 0;

	mboot_recovery_load_raw_part("proinfo", buf, SN_BUF_LEN);

	for (; tmp < SN_BUF_LEN; tmp++) {
		if ((buf[tmp] == 0 || buf[tmp] == 0x20) && tmp > 0)
			break;

		else if (!isalpha(buf[tmp]) && !isdigit(buf[tmp]))
			break;
	}
	return tmp;
}
#endif

#ifdef CONFIG_MTK_USB_UNIQUE_SERIAL
static inline int read_product_usbid(char *serialno)
{
	u64 key;
	u32 hrid_size, ser_len;
	u32 i, chip_code, errcode = 0;
	char *cur_serialp = serialno;
	char serial_num[SERIALNO_LEN];

	/* read machine type */
	chip_code = board_machtype();

	/* read hrid */
	hrid_size = get_hrid_size();

	/* check ser_buf len. if need 128bit id, should defined into cust_usb.h */
	if (SN_BUF_LEN  < hrid_size * 8) {
		hrid_size = 2;
		errcode = 1;
	}

	for (i = 0; i < hrid_size / 2; i++) {
		key = get_devinfo_with_index(13 + i * 2); /* 13, 15 */
		key = (key << 32) | (unsigned int)get_devinfo_with_index(
			      12 + i * 2); /* 12, 14 */

		if (key != 0) {
			get_serial(key, chip_code, serial_num);
			ser_len = strlen(serial_num);
		} else {
			ser_len = strlen(DEFAULT_SERIAL_NUM);
			memcpy(serial_num, DEFAULT_SERIAL_NUM, ser_len);
			errcode = 2;
		}
		/* copy serial from serial_num to sn_buf */
		memcpy(cur_serialp, serial_num, ser_len);
		cur_serialp += ser_len;
	}
	cur_serialp = '\0';

	return errcode;
}
#endif


/******************************************************************************
******************************************************************************/
static void set_serial_num(void)
{
	unsigned int len;
	char *id_tmp = get_env("MTK_DEVICE_ID");
	if (!id_tmp) {
		dprintf(INFO, "Set serial # to default value.\n");
		len = strlen(DEFAULT_SERIAL_NUM);
		len = (len < SN_BUF_LEN) ? len : SN_BUF_LEN;
		strncpy(sn_buf, DEFAULT_SERIAL_NUM, len);
		sn_buf[len] = '\0';
	} else {
		dprintf(INFO, "Set serial # from para.\n");
		len = strlen(id_tmp);
		len = (len < SN_BUF_LEN) ? len : SN_BUF_LEN;
		strncpy(sn_buf, id_tmp, len);
		sn_buf[len] = '\0';
	}

#ifdef CONFIG_MTK_USB_UNIQUE_SERIAL
	int errcode = read_product_usbid(sn_buf);
	if (errcode)
		dprintf(CRITICAL, "Set serial # from efuse. error: %d\n", errcode);
	len = strlen(sn_buf);
	len = (len < SN_BUF_LEN) ? len : SN_BUF_LEN;
	sn_buf[len] = '\0';
#endif  // CONFIG_MTK_USB_UNIQUE_SERIAL

#ifdef SERIAL_NUM_FROM_BARCODE
	len = (unsigned int)read_product_info(sn_buf);  // sn_buf[] may be changed.
	if (len == 0) {
		len = strlen(DEFAULT_SERIAL_NUM);
		len = (len < SN_BUF_LEN) ? len : SN_BUF_LEN;
		strncpy(sn_buf, DEFAULT_SERIAL_NUM, len);
	} else
		len = (len < SN_BUF_LEN) ? len : SN_BUF_LEN;
	sn_buf[len] = '\0';
#endif  // SERIAL_NUM_FROM_BARCODE

	dprintf(CRITICAL, "Serial #: \"%s\"\n", sn_buf);
	surf_udc_device.serialno = sn_buf;
}


void mt_boot_init(const struct app_descriptor *app)
{
	unsigned usb_init = 0;
	unsigned sz = 0;
#ifdef MTK_AB_OTA_UPDATER
	int ret;
#endif

	set_serial_num();

	if (g_boot_mode == FASTBOOT)
		goto fastboot;

#ifdef MTK_SECURITY_SW_SUPPORT
#if MTK_FORCE_VERIFIED_BOOT_SIG_VFY
	/* verify oem image with android verified boot signature instead of mediatek proprietary signature */
	/* verification is postponed to boot image loading stage */
	/* note in this case, boot/recovery image will be verified even when secure boot is disabled */
	g_boot_state = BOOT_STATE_RED;
#else
	if (0 != sec_boot_check(0))
		g_boot_state = BOOT_STATE_RED;
#endif
#endif

	/* Will not return */
	boot_linux_from_storage();

fastboot:
#ifdef MTK_AB_OTA_UPDATER
	ret = set_normal_boot(p_AB_suffix, 0);
	dprintf(CRITICAL, "[%s:%d] set_normal_boot(%s, 0): %d\n", __func__, __LINE__,
		p_AB_suffix, ret);
#endif

	target_fastboot_init();
	if (!usb_init)
		/*Hong-Rong: wait for porting*/
		udc_init(&surf_udc_device);

	mt_part_dump();
	sz = target_get_max_flash_size();
	fastboot_init(target_get_scratch_address(), sz);
	udc_start();

}


APP_START(mt_boot)
.init = mt_boot_init,
 APP_END

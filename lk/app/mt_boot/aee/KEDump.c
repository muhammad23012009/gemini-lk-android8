/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2016. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arch/arm/mmu.h>
#ifdef MTK_GPT_SCHEME_SUPPORT
#include <platform/partition.h>
#else
#include <mt_partition.h>
#endif
#include <platform/boot_mode.h>
#include <platform/ram_console.h>
#include <platform/mt_reg_base.h>
#include <dev/aee_platform_debug.h>
#include "elf.h"
#include "KEHeader.h"
#include <lib/zlib.h>
#ifdef MTK_3LEVEL_PAGETABLE
#include <err.h>
#include <target.h>
#endif

enum {
	AEE_LKDUMP_CLEAR = 0,
	AEE_LKDUMP_MINI_RDUMP,
	AEE_LKDUMP_PROC_CUR_TSK,
	AEE_LKDUMP_KERNEL_LOG_RAW,
	AEE_LKDUMP_DISP_DEBUG_RAW,
	AEE_LKDUMP_RAMCONSOLE_RAW,	//5
	AEE_LKDUMP_PSTORE_RAW,
	AEE_LKDUMP_KEDUMP_CRC,
	AEE_LKDUMP_DFD20,
	AEE_LKDUMP_LAST_DRAM,
	AEE_LKDUMP_LAST_CPU_BUS,	//10
	AEE_LKDUMP_LAST_SPM_DATA,
	AEE_LKDUMP_ATF_LAST,
	AEE_LKDUMP_ATF_CRASH,
	AEE_LKDUMP_ATF_RAW,
	AEE_LKDUMP_CPU_HVFS_RAW,	//15
	AEE_LKDUMP_SSPM_COREDUMP,
	AEE_LKDUMP_SSPM_DATA,
	AEE_LKDUMP_SSPM_XFILE,
	AEE_LKDUMP_SSPM_LAST_LOG,
	AEE_LKDUMP_PLLK_LAST_LOG,	//20
	AEE_LKDUMP_MCDI_DATA,
	AEE_LKDUMP_ZAEE_LOG,
	AEE_LKDUMP_HEADER,
	AEE_LKDUMP_UNKNOWN		//24
};


static struct aee_db_file_info adfi[AEE_PLAT_DEBUG_NUM] = {
	[AEE_PLAT_DFD20] = { "DFD20.dfd", 0x40000, AEE_LKDUMP_DFD20},         /* 256 KB */
	[AEE_PLAT_DRAM] = { "SYS_LAST_DRAM", 0x2400, AEE_LKDUMP_LAST_DRAM},       /*   9 KB */
	[AEE_PLAT_CPU_BUS] = { "SYS_LAST_CPU_BUS", 0x7800, AEE_LKDUMP_LAST_CPU_BUS}, /*  30 KB */
	[AEE_PLAT_SPM_DATA] = { "SYS_LAST_SPM_DATA", 0x1000, AEE_LKDUMP_LAST_SPM_DATA}, /*  4 KB */
	[AEE_PLAT_ATF_LAST_LOG] = { "SYS_ATF_LAST", 0x20000, AEE_LKDUMP_ATF_LAST}, /* 128KB */
	[AEE_PLAT_ATF_CRASH_REPORT] = { "SYS_ATF_CRASH", 0x30000, AEE_LKDUMP_ATF_CRASH}, /* 64KB+128KB */
	[AEE_PLAT_ATF_RAW_LOG] = { "SYS_ATF_RAW_LOG", 0x60000, AEE_LKDUMP_ATF_RAW}, /* 384 KB */
	[AEE_PLAT_HVFS] = { "SYS_CPUHVFS_RAW", 0x3000, AEE_LKDUMP_CPU_HVFS_RAW}, /* 12 KB */
#ifdef MTK_TINYSYS_SSPM_SUPPORT
	[AEE_PLAT_SSPM_COREDUMP] = { "SYS_SSPM_COREDUMP", 0x40080, AEE_LKDUMP_SSPM_COREDUMP}, /* 256KB + 128Byte */
	[AEE_PLAT_SSPM_DATA] = { "SYS_SSPM_DATA", 0x400, AEE_LKDUMP_SSPM_DATA}, /* 1KB */
	[AEE_PLAT_SSPM_XFILE] = { "SYS_SSPM_XFILE", 0xA0000, AEE_LKDUMP_SSPM_XFILE}, /* 640KB */
	[AEE_PLAT_SSPM_LAST_LOG] = { "SYS_SSPM_LAST_LOG", 0x400, AEE_LKDUMP_SSPM_LAST_LOG}, /* 1KB */
#endif
	[AEE_PLAT_PLLK_LAST_LOG] = { "SYS_PLLK_LAST_LOG", 0x40000, AEE_LKDUMP_PLLK_LAST_LOG}, /* 256KB */
	[AEE_PLAT_LOG_DUR_LKDUMP] = { "SYS_LOG_DUR_LKDUMP", 0x40000, AEE_PLAT_LOG_DUR_LKDUMP}, /* 256KB */
	[AEE_PLAT_MCDI_DATA] = { "SYS_MCDI_DATA", 0x800, AEE_LKDUMP_MCDI_DATA} /* 2KB, size will modified by plat. */
};

struct aee_db_file_info* get_file_info(void)
{
	return &adfi;
}

/**************************/
/* ----------------   */
/* RAM_CONSOLE_DRAM_ADDR  */
/* (1M align)         */
/* +RAM_CONSOLE_DRAM_SIZE */
/*            */
/* ----------------   */
/* +0xf0000       */
/*            */
/* ----------------   */
/* KE_RESERVED_MEM_ADDR   */
/*            */
/* ----------------   */
/* RAMDISK_LOAD_ADDR      */
/**************************/
#define KE_RESERVED_MEM_ADDR (RAM_CONSOLE_DRAM_ADDR + 0xf0000)
#define EXPDB_RESERVED_OTHER    (3 * 1024 * 1024) //reserved expdb for control block and pl/lk log
#if defined(MTK_MLC_NAND_SUPPORT) || defined(MTK_TLC_NAND_SUPPORT)
#define MEM_EXPDB_SIZE 0x300000
static char *mem_expdb;
#endif
struct ke_dev {
	part_dev_t *dev;
	uint  part_id;
	u64 ptn;
	u64 part_size;
};

static struct ke_dev dev;

struct elfhdr {
	void *start;
	unsigned int e_machine;
	unsigned int e_phoff;
	unsigned int e_phnum;
};

struct kedump_crc {
	unsigned int ram_console_crc;
	unsigned int pstore_crc;
};

struct kedump_crc kc;

extern BOOT_ARGUMENT *g_boot_arg;
#define SZLOG 8192
static char logbuf[SZLOG];

extern void ram_console_addr_size(unsigned long *addr, unsigned long *size);
extern void ram_console_set_dump_step(unsigned int step);
extern int ram_console_get_dump_step(void);
#ifdef MTK_PMIC_FULL_RESET
extern bool ram_console_reboot_by_cold_reset(void);
#endif

int check_ram_console_is_abnormal_boot(void)
{
	return ram_console_is_abnormal_boot();
}

static unsigned int last_dump_step;

int sLOG(char *fmt, ...)
{
	va_list args;
	static int pos = 0;
	va_start(args, fmt);
	if (pos < SZLOG - 1) /* vsnprintf bug */
		pos += vsnprintf(logbuf + pos, SZLOG - pos - 1, fmt, args);
	va_end(args);
	return 0;
}

#define LOG(fmt, ...)           \
    do {    \
        sLOG(fmt, ##__VA_ARGS__);       \
        printf(fmt, ##__VA_ARGS__); \
    } while (0)

#define LOGD(fmt, ...)              \
    sLOG(fmt, ##__VA_ARGS__)

#define elf_note    elf32_note
#define PHDR_PTR(ehdr, phdr, mem)       \
    (ehdr->e_machine == EM_ARM ? ((struct elf32_phdr*)phdr)->mem : ((struct elf64_phdr*)phdr)->mem)

#define PHDR_TYPE(ehdr, phdr) PHDR_PTR(ehdr, phdr, p_type)
#define PHDR_VADDR(ehdr, phdr) PHDR_PTR(ehdr, phdr, p_vaddr)
#define PHDR_ADDR(ehdr, phdr) PHDR_PTR(ehdr, phdr, p_paddr)
#define PHDR_SIZE(ehdr, phdr) PHDR_PTR(ehdr, phdr, p_filesz)
#define PHDR_OFF(ehdr, phdr) PHDR_PTR(ehdr, phdr, p_offset)
#define PHDR_INDEX(ehdr, i)         \
    (ehdr->e_machine == EM_ARM ? ehdr->start + ehdr->e_phoff + sizeof(struct elf32_phdr) * i : ehdr->start + ehdr->e_phoff + sizeof(struct elf64_phdr) *i)

static unsigned int calculate_crc32(void *data, unsigned int len)
{
	unsigned int mycrc;
	unsigned int ret;

#ifdef MTK_3LEVEL_PAGETABLE
	{
		/*int ret;*/
		uint32_t start = ROUNDDOWN((uint32_t)data, SECTION_SIZE);
		uint32_t secsize = ROUNDUP(((uint32_t)data - start + len), SECTION_SIZE);
		if (start >= DRAM_PHY_ADDR) {
			/* minirdump: minirdump will dump memory in DRAM, we must allocate it first */
			arch_mmu_map((uint64_t) start, start,
				     MMU_MEMORY_TYPE_NORMAL_WRITE_BACK | MMU_MEMORY_AP_P_RW_U_NA, secsize);
		}
	}
#endif
	mycrc = crc32(0L, Z_NULL, 0);
	ret = crc32(mycrc, data, len);
	LOG("kedump: crc = 0x%x\n", ret);
	return ret;
}

static struct elfhdr* kedump_elf_hdr(void)
{
	char *ei;
	static struct elfhdr kehdr;
	static struct elfhdr *ehdr = (void*)-1;
	if (ehdr != (void*)-1)
		return ehdr;
	ehdr = NULL;
#ifdef MTK_3LEVEL_PAGETABLE
	{
		uint32_t start = KE_RESERVED_MEM_ADDR;
		/* KEDump need to use header in DRAZM, we must allocate it first */
		arch_mmu_map(ROUNDDOWN((uint64_t)start, SECTION_SIZE), ROUNDDOWN((uint32_t)start, SECTION_SIZE),
		             MMU_MEMORY_TYPE_NORMAL_WRITE_BACK | MMU_MEMORY_AP_P_RW_U_NA, ROUNDUP(sizeof(struct elfhdr), SECTION_SIZE));
	}
#endif
	kehdr.start = (void*)(KE_RESERVED_MEM_ADDR);
	LOG("kedump: KEHeader %p\n", kehdr.start);
	if (kehdr.start) {
		ei = (char*)kehdr.start; //elf_hdr.e_ident
		LOG("kedump: read header 0x%p[0x%x%x%x%x]\n", ei, ei[0], ei[1], ei[2], ei[3]);
		/* valid elf header */
		if (ei[0] == 0x7f && ei[1] == 'E' && ei[2] == 'L' && ei[3] == 'F') {
			kehdr.e_machine = ((struct elf32_hdr*)(kehdr.start))->e_machine;
			if (kehdr.e_machine == EM_ARM) {
				kehdr.e_phnum = ((struct elf32_hdr*)(kehdr.start))->e_phnum;
				kehdr.e_phoff = ((struct elf32_hdr*)(kehdr.start))->e_phoff;
				ehdr = &kehdr;
			} else if (kehdr.e_machine == EM_AARCH64) {
				kehdr.e_phnum = ((struct elf64_hdr*)(kehdr.start))->e_phnum;
				kehdr.e_phoff = ((struct elf64_hdr*)(kehdr.start))->e_phoff;
				ehdr = &kehdr;
			}
		}
		if (ehdr == NULL)
			LOG("kedump: invalid header[0x%x%x%x%x]\n", ei[0], ei[1], ei[2], ei[3]);
	}
	LOG("kedump: mach[0x%x], phnum[0x%x], phoff[0x%x]\n", kehdr.e_machine, kehdr.e_phnum, kehdr.e_phoff);
	return ehdr;
}

static int kedump_dev_open(void)
{
	int index;
	index = partition_get_index(AEE_IPANIC_PLABLE);
	dev.dev = mt_part_get_device();
	if (index == -1 || dev.dev == NULL) {
		LOG("kedump: no %s partition[%d]\n", AEE_IPANIC_PLABLE, index);
		return -1;
	}
#if (defined(MTK_UFS_BOOTING) || defined(MTK_NEW_COMBO_EMMC_SUPPORT))
	dev.part_id = partition_get_region(index);
#endif
	dev.ptn = partition_get_offset(index);
	dev.part_size = partition_get_size(index);
	if (dev.part_size < EXPDB_RESERVED_OTHER) {
		LOG("kedump: partition size(%llx) is lesser then reserved!(%llx)\n", dev.part_size, (unsigned long long)EXPDB_RESERVED_OTHER);
		return -1;
	}
	dev.part_size -= EXPDB_RESERVED_OTHER; //reserved expdb for others
	LOG("kedump: partiton %d[%llx - %llx]\n", index, dev.ptn, dev.part_size);

	return 0;
}

#if defined(MTK_MLC_NAND_SUPPORT) || defined(MTK_TLC_NAND_SUPPORT)
unsigned long long mem_expdb_write(void *data, unsigned long long offset, unsigned long sz)
{
	if ((offset + sz) > MEM_EXPDB_SIZE) {
		LOG("overflow!\n");
		return 0;
	}
	memcpy((mem_expdb + offset), data, (unsigned long)sz);

	return sz;
}
#endif

#define TRUNK 0x8000
static unsigned long long kedump_dev_write (unsigned long long offset, uint64_t data, unsigned long sz)
{
	unsigned long long size_wrote = 0;
	vaddr_t vaddr = (uint32_t)data;
	vaddr_t memsrc = vaddr;
	uint8_t *trunk = malloc(TRUNK);
	unsigned long rest = sz;
	if (trunk == NULL) {
		LOG("kedump: malloc failed\n");
		return 0;
	}
	LOG("kedump: offset:0x%llx, data:0x%llx, size:0x%lx\n", offset, data, sz);
#ifdef MTK_3LEVEL_PAGETABLE
	{
		/*int ret;*/
		uint64_t start = ROUNDDOWN((uint64_t)data, (uint64_t)SECTION_SIZE);
		vaddr = ROUNDUP((vaddr_t)target_get_scratch_address(), SECTION_SIZE);
		uint32_t secsize = ROUNDUP((uint32_t)(data - start + sz), SECTION_SIZE);
		if (start >= DRAM_PHY_ADDR) {
			/* minirdump: minirdump will dump memory in DRAM, we must allocate it first */

			int map_ok = arch_mmu_map((uint64_t) start, vaddr,
				     MMU_MEMORY_TYPE_NORMAL_WRITE_BACK | MMU_MEMORY_AP_P_RW_U_NA, secsize);
			if (map_ok != NO_ERROR)
				LOG("kedump: map error\n");
			memsrc = vaddr + (uint32_t)(data - start);
		}
		/*LOG("kedump: start:0x%llx, vaddr:0x%x, secsize:0x%x, memsrc:0x%x\n", start, vaddr, secsize, memsrc);*/
	}
#endif
	if (offset >= dev.part_size || sz > dev.part_size - offset) {
		if (trunk != NULL)
			free(trunk);
		LOG("kedump: write oversize %lx -> %llx > %llx\n", sz, offset, dev.part_size);
		return 0;
	}
	while (rest > 0) {
		unsigned long write_sz;
		memset(trunk, 0x0, TRUNK);
		if (rest <= TRUNK) {
			write_sz = rest;
		} else {
			write_sz = TRUNK;
		}
		memcpy(trunk, (void *)(memsrc + (sz - rest)), TRUNK);

#if (defined(MTK_UFS_BOOTING) || defined(MTK_EMMC_SUPPORT))
#if (defined(MTK_UFS_BOOTING) || defined(MTK_NEW_COMBO_EMMC_SUPPORT))
	size_wrote += dev.dev->write(dev.dev, trunk, dev.ptn + offset, write_sz, dev.part_id);
#else
	size_wrote += dev.dev->write(dev.dev, trunk, dev.ptn + offset, write_sz);
#endif
#elif defined(MTK_NAND_SUPPORT)
	size_wrote += dev.dev->write(dev.dev, trunk, (unsigned long)dev.ptn + offset, write_sz);
#endif

#if defined(MTK_MLC_NAND_SUPPORT) || defined(MTK_TLC_NAND_SUPPORT)
	size_wrote += mem_expdb_write(trunk, offset, write_sz);
#endif
		offset += write_sz;
		rest -= write_sz;
	}
	if ((long long)size_wrote <= 0) {
		LOG("kedump: write failed(%llx), %lx@%llx -> %llx\n", size_wrote, sz, data, offset);
		size_wrote = 0;
	}
	free(trunk);
#ifdef MTK_3LEVEL_PAGETABLE
	{
		/* restore vaddr */
		uint64_t start = ROUNDDOWN((uint64_t)data, (uint64_t)SECTION_SIZE);
		uint32_t secsize = ROUNDUP((uint32_t)(data - start + sz), SECTION_SIZE);
		if (start >= DRAM_PHY_ADDR) {
			int map_ok = arch_mmu_map(vaddr, vaddr, MMU_MEMORY_TYPE_NORMAL_WRITE_BACK | MMU_MEMORY_AP_P_RW_U_NA, secsize);
			if (map_ok != NO_ERROR) {
				LOG("kedump: arch_mmu_map restore error: map_ok=%d, vaddr=0x%08x\n",
					map_ok, vaddr);
			}
		}
	}
#endif
	return size_wrote;
}

static unsigned long long offset_plat_debug = 0;
static unsigned long length_plat_debug = 0;
static unsigned long long kedump_plat_write (void *data, unsigned long sz)
{
	unsigned long long datasize = 0;

	datasize = kedump_dev_write(offset_plat_debug, (uint64_t)((uint32_t)data), sz);
	offset_plat_debug += datasize;
	length_plat_debug += sz;

	return datasize;
}

static void kedump_dev_close(void)
{
	return;
}

/* the min offset reserved for the header's size. */
static unsigned long kedump_mrdump_header_size (struct elfhdr *ehdr)
{
	void *phdr = PHDR_INDEX(ehdr, 1);
	return ALIGN(PHDR_OFF(ehdr, phdr) + PHDR_SIZE(ehdr, phdr), PAGE_SIZE);
}

static unsigned int kedump_mini_rdump(struct elfhdr *ehdr, unsigned long long offset)
{
	void *phdr;
	unsigned long long addr;
	unsigned long size;
	unsigned int i;
	unsigned int total = 0;
	unsigned long elfoff = kedump_mrdump_header_size(ehdr);
	unsigned long sz_header = elfoff;
	for (i = 0; i < ehdr->e_phnum; i++) {
		phdr = PHDR_INDEX(ehdr, i);
		if (PHDR_SIZE(ehdr, phdr) != 0 || PHDR_TYPE(ehdr, phdr) != 0)
			LOGD("kedump: PT[%d] %x@%llx -> %x(%x)\n", PHDR_TYPE(ehdr, phdr), PHDR_SIZE(ehdr, phdr), PHDR_ADDR(ehdr, phdr), elfoff, (unsigned int)PHDR_OFF(ehdr, phdr));
		if (PHDR_TYPE(ehdr, phdr) != PT_LOAD)
			continue;
		addr = PHDR_ADDR(ehdr, phdr);
		if (addr < DRAM_PHY_ADDR) {
			LOG("kedump: skip dump non-dram PA:%llx, VA:%llx\n", addr, PHDR_VADDR(ehdr, phdr));
			continue;
		}
		size = PHDR_SIZE(ehdr, phdr);
		if (size == 0 || elfoff == 0)
			LOG("kedump: dump addr 0x%llx, size 0x%lx\n", addr, size);
		if (ehdr->e_machine == EM_ARM)
			((struct elf32_phdr*)phdr)->p_offset = elfoff;
		else
			((struct elf64_phdr*)phdr)->p_offset = elfoff;
		if (size != 0 && elfoff != 0)
			total += kedump_dev_write(offset + elfoff, (uint64_t)addr, size);
		elfoff += size;
	}
	total += kedump_dev_write(offset, (uint64_t)((uint32_t)ehdr->start), sz_header);
	return total;
}

static unsigned int kedump_misc(unsigned long long addr, unsigned int start, unsigned int size, unsigned long long offset)
{
	unsigned int total;
	LOG("kedump: misc data %x@%llx+%x\n", size, addr, start);
	if (start >= size)
		start = start % size;
	total = kedump_dev_write(offset, (uint64_t)(addr + start), size - start);
	if (start)
		total += kedump_dev_write(offset + total, (uint64_t)addr, start);
	return total;
}

static unsigned int kedump_misc32(struct mrdump_mini_misc_data32 *data, unsigned long long offset)
{
	unsigned int addr = data->paddr;
	unsigned int start = 0;
#ifdef MTK_3LEVEL_PAGETABLE
	vaddr_t vaddr;
	vaddr_t paddr = (vaddr_t)data->start;
	vaddr_t memsrc;

	if (paddr) {
		LOG("kedump: misc data map 0x%lx size:%d\n", paddr, sizeof(data->start));
		uint64_t start_new = ROUNDDOWN((uint64_t)paddr, (uint64_t)SECTION_SIZE);
		vaddr = ROUNDUP((vaddr_t)target_get_scratch_address(), SECTION_SIZE);
		uint32_t secsize = ROUNDUP((uint32_t)(paddr - start_new + sizeof(data->start)), SECTION_SIZE);
		if (start_new >= DRAM_PHY_ADDR) {
			int map_ok = arch_mmu_map((uint64_t) start_new, vaddr,
					 MMU_MEMORY_TYPE_NORMAL_WRITE_BACK | MMU_MEMORY_AP_P_RW_U_NA, secsize);
			if (map_ok != NO_ERROR)
				LOG("kedump: map error\n");
			else {
				memsrc = vaddr + (uint32_t)(paddr - start_new);
				start = *(unsigned int *)(unsigned long)memsrc;
			}
		}

		/* restore vaddr */
		start_new = ROUNDDOWN((uint64_t)paddr, (uint64_t)SECTION_SIZE);
		secsize = ROUNDUP((uint32_t)(paddr - start_new + sizeof(data->start)), SECTION_SIZE);
		if (start_new >= DRAM_PHY_ADDR) {
			int map_ok = arch_mmu_map(vaddr, vaddr, MMU_MEMORY_TYPE_NORMAL_WRITE_BACK | MMU_MEMORY_AP_P_RW_U_NA, secsize);
			if (map_ok != NO_ERROR) {
				LOG("kedump: arch_mmu_map restore error: map_ok=%d, vaddr=0x%08x\n",
					map_ok, vaddr);
			}
		}
	}
#else
	start = data->start ? *(unsigned int*)(unsigned long)(data->start) : 0;
#endif
	unsigned int size = data->size;
	return kedump_misc((uint64_t)((uint32_t)addr), start, size, offset);
}

static unsigned int kedump_misc64(struct mrdump_mini_misc_data64 *data, unsigned long long offset)
{
	unsigned long long addr = (unsigned long long)data->paddr;
	unsigned int start = 0;
#ifdef MTK_3LEVEL_PAGETABLE
	vaddr_t vaddr;
	vaddr_t paddr = (vaddr_t)data->start;
	vaddr_t memsrc;

	if (paddr) {
		LOG("kedump: misc data map 0x%lx size:%d\n", paddr, sizeof(data->start));
		uint64_t start_new = ROUNDDOWN((uint64_t)paddr, (uint64_t)SECTION_SIZE);
		vaddr = ROUNDUP((vaddr_t)target_get_scratch_address(), SECTION_SIZE);
		uint32_t secsize = ROUNDUP((uint32_t)(paddr - start_new + sizeof(data->start)), SECTION_SIZE);
		if (start_new >= DRAM_PHY_ADDR) {
			int map_ok = arch_mmu_map((uint64_t) start_new, vaddr,
					 MMU_MEMORY_TYPE_NORMAL_WRITE_BACK | MMU_MEMORY_AP_P_RW_U_NA, secsize);
			if (map_ok != NO_ERROR)
				LOG("kedump: map error\n");
			else {
				memsrc = vaddr + (uint32_t)(paddr - start_new);
				start = *(unsigned int *)(unsigned long)memsrc;
			}
		}

		/* restore vaddr */
		start_new = ROUNDDOWN((uint64_t)paddr, (uint64_t)SECTION_SIZE);
		secsize = ROUNDUP((uint32_t)(paddr - start_new + sizeof(data->start)), SECTION_SIZE);
		if (start_new >= DRAM_PHY_ADDR) {
			int map_ok = arch_mmu_map(vaddr, vaddr, MMU_MEMORY_TYPE_NORMAL_WRITE_BACK | MMU_MEMORY_AP_P_RW_U_NA, secsize);
			if (map_ok != NO_ERROR) {
				LOG("kedump: arch_mmu_map restore error: map_ok=%d, vaddr=0x%08x\n",
					map_ok, vaddr);
			}
		}
	}
#else
	start = data->start ? *(unsigned int*)(unsigned long)(data->start) : 0;
#endif
	unsigned int size = (unsigned int)data->size;
	return kedump_misc(addr, start, size, offset);
}

struct ipanic_header panic_header;
static unsigned long long header_off;
static void kedump_add2hdr(unsigned int offset, unsigned int size, unsigned datasize, char *name)
{
	struct ipanic_data_header *pdata;
	int i;
	for (i = 0; i < IPANIC_NR_SECTIONS; i++) {
		pdata = &panic_header.data_hdr[i];
		if (pdata->valid == 0)
			break;
	}
	LOG("kedump add: %s[%d] %x/%x@%x\n", name, i, datasize, size, offset);
	if (i < IPANIC_NR_SECTIONS) {
		pdata->offset = offset;
		pdata->total = size;
		pdata->used = datasize;
		strlcpy((char*)pdata->name, name, sizeof(pdata->name));
		pdata->valid = 1;
	}
	header_off += kedump_dev_write(header_off, (uint64_t)((uint32_t)(pdata)), sizeof(struct ipanic_data_header));
}

static int kedump_kernel_info(unsigned long long *offset)
{
	struct elfhdr *ehdr;
	unsigned long sz_misc;
	void *phdr_misc;
	struct elf_note *misc, *miscs;
	char *m_name;
	void *m_data;
	char name[32];
	unsigned int size, datasize;
	unsigned int i;

	ehdr = kedump_elf_hdr();
	if (0 == ehdr)
		return -1;
	ram_console_set_dump_step(AEE_LKDUMP_MINI_RDUMP);
	datasize = kedump_mini_rdump(ehdr, *offset);
	size = datasize;
	kedump_add2hdr(*offset, size, datasize, "SYS_MINI_RDUMP");
	*offset += datasize;

	phdr_misc = PHDR_INDEX(ehdr, 1);
	miscs = (struct elf_note*)(ehdr->start + PHDR_OFF(ehdr, phdr_misc));
	LOGD("kedump: misc[%p] %llx@%llx\n", phdr_misc, PHDR_SIZE(ehdr, phdr_misc), PHDR_OFF(ehdr, phdr_misc));
	sz_misc = sizeof(struct elf_note) + miscs->n_namesz + miscs->n_descsz;
	LOGD("kedump: miscs[%p], size %x\n", miscs, sz_misc);
	for (i = 0; i < (PHDR_SIZE(ehdr, phdr_misc)) / sz_misc; i++) {
		char klog_first[16];
		memset(klog_first, 0x0, sizeof(klog_first));
		misc = (struct elf_note*)((void*)miscs + sz_misc * i);
		m_name = (char*)misc + sizeof(struct elf_note);
		if (m_name[0] == 'N' && m_name[1] == 'A' && m_name[2] == '\0')
			break;
		m_data = (void*)misc + sizeof(struct elf_note) + misc->n_namesz;
		if (misc->n_descsz == sizeof(struct mrdump_mini_misc_data32)) {
			if (strcmp(m_name, "_KERNEL_LOG_") == 0) {
				sprintf(klog_first, "_%u", *(unsigned int *)(((struct mrdump_mini_misc_data32*)m_data)->start));
				((struct mrdump_mini_misc_data32*)m_data)->start = 0;
			}
			datasize = kedump_misc32((struct mrdump_mini_misc_data32*)m_data, *offset);
			size = ((struct mrdump_mini_misc_data32*)m_data)->size;
		} else {
			if (strcmp(m_name, "_KERNEL_LOG_") == 0) {
				sprintf(klog_first, "_%u", *(unsigned int *)(unsigned long)(((struct mrdump_mini_misc_data64*)m_data)->start));
				((struct mrdump_mini_misc_data64*)m_data)->start = 0;
			}
			datasize = kedump_misc64((struct mrdump_mini_misc_data64*)m_data, *offset);
			size = ((struct mrdump_mini_misc_data64*)m_data)->size;
		}
		/* [SYS_]MISC[_RAW] */
		if (m_name[0] == '_')
			strlcpy (name, "SYS", sizeof(name));
		else
			name[0] = 0;
		strlcat (name, m_name, sizeof(name));
		if (m_name[strlen(m_name)-1] == '_')
			strlcat (name,  "RAW", sizeof(name));
		if (klog_first[0] != 0)
			strlcat(name, klog_first, sizeof(name));
		kedump_add2hdr(*offset, size, datasize, name);
		*offset += datasize;
	}
	return 0;
}

static int kedump_ram_console(unsigned long long *offset)
{
	unsigned long sz_misc, addr_misc;
	unsigned int datasize;

	/* ram_console raw log */
	ram_console_set_dump_step(AEE_LKDUMP_RAMCONSOLE_RAW);
	ram_console_addr_size(&addr_misc, &sz_misc);
	if (addr_misc && sz_misc) {
		datasize = kedump_misc((unsigned int)addr_misc, 0, (unsigned int)sz_misc, *offset);
		kc.ram_console_crc = calculate_crc32((void *)addr_misc, sz_misc);
		kedump_add2hdr(*offset, (unsigned int)sz_misc, datasize, "SYS_RAMCONSOLE_RAW");
		*offset += datasize;
	}
#ifdef MTK_PMIC_FULL_RESET
	/* pstore raw log*/
	ram_console_set_dump_step(AEE_LKDUMP_PSTORE_RAW);
	datasize = kedump_misc((unsigned int)PSTORE_ADDR, 0, (unsigned int)PSTORE_SIZE, *offset);
	kc.pstore_crc = calculate_crc32((void *)PSTORE_ADDR, PSTORE_SIZE);
	kedump_add2hdr(*offset, (unsigned int)PSTORE_SIZE, datasize, "SYS_PSTORE_RAW");
	*offset += datasize;
#endif
	/* save crc data*/
	ram_console_set_dump_step(AEE_LKDUMP_KEDUMP_CRC);
	datasize = kedump_dev_write(*offset, (uint64_t)((uint32_t)(&kc)), sizeof(struct kedump_crc));
	kedump_add2hdr(*offset, sizeof(struct kedump_crc), datasize, "KEDUMP_CRC");
	*offset += datasize;
	return 0;
}

static int kedump_platform_debug(unsigned long long *offset)
{
	/* platform debug */
	int len = 0;
	unsigned int datasize;
	unsigned int i;

	for (i=0; i<AEE_PLAT_DEBUG_NUM; i++) {
		offset_plat_debug = *offset;
		length_plat_debug = 0;
		ram_console_set_dump_step(adfi[i].step);
		datasize = kedump_plat_savelog(i, offset_plat_debug, &len, kedump_plat_write);
		if ((datasize > 0) && (datasize <= adfi[i].filesize)) {
			kedump_add2hdr(*offset, length_plat_debug, datasize, adfi[i].filename);
			*offset += datasize;
		}
	}
	return 0;
}

static int kedump_to_expdb(void)
{
	unsigned long long offset;
	unsigned int datasize;
	part_dev_t *part_dev;

	part_dev = mt_part_get_device();

	if (!part_dev || !part_dev->blkdev) {
		LOG("kedump: device get error, dev:%p\n", part_dev);
		return -1;
	}

	if (kedump_dev_open() != 0)
		return -1;

	last_dump_step = ram_console_get_dump_step();
	if (last_dump_step != AEE_LKDUMP_CLEAR) {
		LOG("kedump: last lk dump is not finished at step %u\n", last_dump_step);
		return 0;
	}
#if defined(MTK_MLC_NAND_SUPPORT) || defined(MTK_TLC_NAND_SUPPORT)
	mem_expdb = malloc(MEM_EXPDB_SIZE);
	if (mem_expdb == NULL) {
		LOG("mem_expdb malloc fail!\n");
		return -1;
	}
	LOG("mem_expdb malloc success, 0x%x size is 0x%x\n", mem_expdb, MEM_EXPDB_SIZE);
	memset(mem_expdb, 0x0, MEM_EXPDB_SIZE);
#endif
	//write header firstly
	panic_header.magic = AEE_IPANIC_MAGIC;
	panic_header.version = AEE_IPANIC_PHDR_VERSION;
	panic_header.size = sizeof(panic_header);
	panic_header.blksize = part_dev->blkdev->blksz;
	panic_header.partsize = dev.part_size;
	kedump_dev_write(0, (uint64_t)((uint32_t)(&panic_header)), sizeof(panic_header));
	header_off = sizeof(panic_header) - sizeof(struct ipanic_data_header) * IPANIC_NR_SECTIONS;

	LOG("kedump: block size:0x%x\n", part_dev->blkdev->blksz);
	/* reserve space in expdb for panic header */
	offset = ALIGN(sizeof(panic_header), part_dev->blkdev->blksz);

	kedump_kernel_info(&offset);
	kedump_ram_console(&offset);
	kedump_platform_debug(&offset);

	/* save KEdump flow logs */
	datasize = kedump_dev_write(offset, (uint64_t)(uint32_t)logbuf, SZLOG);
	kedump_add2hdr(offset, SZLOG, datasize, "ZAEE_LOG");
	offset += datasize;

#if defined(MTK_MLC_NAND_SUPPORT) || defined(MTK_TLC_NAND_SUPPORT)
	unsigned long long size_wrote = dev.dev->write(dev.dev, mem_expdb, (unsigned long)dev.ptn, MEM_EXPDB_SIZE/*dev.part_size*/, dev.part_id);
	free(mem_expdb);
#endif
	ram_console_set_dump_step(AEE_LKDUMP_CLEAR);
	return 0;
}

#ifdef MTK_PMIC_FULL_RESET
static int kedump_restore_mem(void)
{
	int i;
	struct ipanic_header iheader;
	struct kedump_crc saved_crc = {0, 0};

	if (kedump_dev_open() != 0)
		return -1;
	dev.dev->read(dev.dev, dev.ptn, (unsigned char *)&iheader, sizeof(struct ipanic_header), dev.part_id);
	if (iheader.magic == AEE_IPANIC_MAGIC && iheader.version >= AEE_IPANIC_PHDR_VERSION) {
		LOG("kedump: found content in expdb\n");
		for (i = IPANIC_NR_SECTIONS - 1; i >= 0; i--) {
			if (strncmp((const char *)iheader.data_hdr[i].name, "KEDUMP_CRC", sizeof("KEDUMP_CRC") - 1) == 0) {
				LOG("kedump: read %s from offset 0x%x size 0x%x\n", iheader.data_hdr[i].name, iheader.data_hdr[i].offset, iheader.data_hdr[i].used);
				dev.dev->read(dev.dev, dev.ptn + iheader.data_hdr[i].offset, (uchar *)(&saved_crc), iheader.data_hdr[i].used, dev.part_id);
			}
			if (strncmp((const char *)iheader.data_hdr[i].name, "SYS_RAMCONSOLE_RAW", sizeof("SYS_RAMCONSOLE_RAW") - 1) == 0) {
				unsigned long sz_misc, addr_misc;
				ram_console_addr_size(&addr_misc, &sz_misc);
				if (addr_misc) {
					unsigned char *temp = malloc(sz_misc);
					unsigned int crc;
					if (!temp) {
						LOG("kedump: crc temp alloc fail\n");
					} else {
						LOG("kedump: read %s from offset 0x%x size 0x%x\n", iheader.data_hdr[i].name, iheader.data_hdr[i].offset, iheader.data_hdr[i].used);
						memset(temp, 0x0, sz_misc);
						//dev.dev->read(dev.dev, dev.ptn + iheader.data_hdr[i].offset, (uchar *)addr_misc, iheader.data_hdr[i].used, dev.part_id);
						dev.dev->read(dev.dev, dev.ptn + iheader.data_hdr[i].offset, temp, iheader.data_hdr[i].used, dev.part_id);
						crc = calculate_crc32(temp, sz_misc);
						if (crc != saved_crc.ram_console_crc) {
							LOG("kedump: ram_console crc fail\n");
						} else {
							memcpy((uchar *)addr_misc, temp, sz_misc);
						}
						free(temp);
						ram_console_set_dump_step(AEE_LKDUMP_CLEAR);
					}
				} else {
					LOG("kedump: ram_console not init\n");
				}
			} else if (strncmp((const char *)iheader.data_hdr[i].name, "SYS_PSTORE_RAW", sizeof("SYS_PSTORE_RAW") - 1) == 0) {
#ifdef MTK_3LEVEL_PAGETABLE
	{
		/*int ret;*/
		uint32_t start = ROUNDDOWN((uint32_t)PSTORE_ADDR, SECTION_SIZE);
		uint32_t secsize = ROUNDUP(((uint32_t)PSTORE_SIZE), SECTION_SIZE);
		if (start >= DRAM_PHY_ADDR) {
			/* minirdump: minirdump will dump memory in DRAM, we must allocate it first */
			arch_mmu_map((uint64_t) start, start,
				     MMU_MEMORY_TYPE_NORMAL_WRITE_BACK | MMU_MEMORY_AP_P_RW_U_NA, secsize);
		}
	}
#endif
				unsigned char *temp = malloc(PSTORE_SIZE);
				unsigned int crc;
				if (!temp) {
					LOG("kedump: crc temp alloc fail\n");
				} else {
					LOG("kedump: read %s from offset 0x%x size 0x%x\n", iheader.data_hdr[i].name, iheader.data_hdr[i].offset, iheader.data_hdr[i].used);
					memset(temp, 0x0, PSTORE_SIZE);
					//dev.dev->read(dev.dev, dev.ptn + iheader.data_hdr[i].offset, (uchar *)PSTORE_ADDR, iheader.data_hdr[i].used, dev.part_id);
					dev.dev->read(dev.dev, dev.ptn + iheader.data_hdr[i].offset, temp, iheader.data_hdr[i].used, dev.part_id);
					crc = calculate_crc32(temp, PSTORE_SIZE);
					if (crc != saved_crc.pstore_crc) {
						LOG("kedump: pstore crc fail\n");
					} else {
						memcpy((uchar *)PSTORE_ADDR, temp, PSTORE_SIZE);
					}
					free(temp);
				}
			}
		}
	}
	return 0;
}
#endif

static int kedump_skip(void)
{
	unsigned int boot_reason = g_boot_arg->boot_reason;
	static int kedump_dumped = 0;
	LOG("kedump: boot_reason(%d)\n", boot_reason);
	ram_console_init();
	/* this flow should be executed once only */
	if (kedump_dumped == 0) {
		kedump_dumped = 1;
		if (ram_console_is_abnormal_boot())
			return 0;
	}
#ifdef MTK_PMIC_FULL_RESET
	//if this reboot is full pmic reset, then restore the memory of ram_console and pstore
	if (ram_console_reboot_by_cold_reset()) {
		LOG("kedump: last is full pmic reset!\n");
		kedump_restore_mem();
	} else {
		LOG("kedump: last is not full pmic reset!\n");
	}
#endif
	return 1;
}

static int kedump_avail(void)
{
#ifdef MTK_3LEVEL_PAGETABLE
	vaddr_t vaddr = (vaddr_t)target_get_scratch_address();
	LOG("kedump: address:0x%lx, section size:0x%x\n", vaddr, SECTION_SIZE);
	{
		uint32_t start = KE_RESERVED_MEM_ADDR;
		/* KEDump need to use header in DRAZM, we must allocate it first */
		arch_mmu_map(ROUNDDOWN((uint64_t)start, SECTION_SIZE), ROUNDDOWN((uint32_t)start, SECTION_SIZE),
		             MMU_MEMORY_TYPE_NORMAL_WRITE_BACK | MMU_MEMORY_AP_P_RW_U_NA, ROUNDUP(sizeof(struct elfhdr), SECTION_SIZE));
	}
#endif
	void *flag = (void *)(KE_RESERVED_MEM_ADDR);

	if (((char *)flag)[0] == 0x81 && ((char *)flag)[1] == 'E' && ((char *)flag)[2] == 'L' && ((char *)flag)[3] == 'F') {
		LOG("kedump: already dumped in lk\n");
		return -1;
	}

	if (((char *)flag)[0] == 0x0 && ((char *)flag)[1] == 'E' && ((char *)flag)[2] == 'L' && ((char *)flag)[3] == 'F') {
		LOG("kedump: already dumped in kernel\n");
		return -1;
	}
	return 0;
}

static int kedump_done(void)
{
	void *flag = (void *)(KE_RESERVED_MEM_ADDR);

	((char *)flag)[0] = 0x81;
	((char *)flag)[1] = 'E';
	((char *)flag)[2] = 'L';
	((char *)flag)[3] = 'F';
	arch_clean_cache_range((addr_t)KE_RESERVED_MEM_ADDR, sizeof(struct elfhdr));
	return 0;
}

/* in case that platform didn't support smart_reset_check() */
const char *smart_reset_check(void) __attribute__((weak));
const char *smart_reset_check(void)
{
	return NULL;
}

/* Dump KE infomation to expdb */
/*  1: has expception, 0: has no exception */
int kedump_mini(void)
{
	const char *status = smart_reset_check();
	if (status != NULL)
		LOG("%s\n", status);

	LOG("kedump mini start\n");

	if (lkdump_debug_init())
		LOG("kedump: lkdump debug init ok\n");
	else
		LOG("kedump: lkdump debug not ready\n");

	if (kedump_skip())
		return 0;

	if (kedump_avail())
		return 0;

	kedump_to_expdb();

	kedump_done();
	LOG("kedump mini done\n");
	return 1;
}

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
#include <malloc.h>
#include <dev/aee_platform_debug.h>
#include <platform/mt_reg_base.h>
#include <platform/mt_typedefs.h>
#include <platform/mtk_wdt.h>
#include <platform/platform_debug.h>
#include <platform/log_store_lk.h>
#include <plat_debug_interface.h>
#include <reg.h>
#include <fdt.h>
#include <libfdt.h>
#include <debug.h>

int plt_get_cluster_id(unsigned int cpu_id, unsigned int *core_id_in_cluster)
{
	if (core_id_in_cluster == NULL)
		return -1;

	*core_id_in_cluster = (cpu_id % 4);
	return (cpu_id / 4);
}

unsigned long plt_get_cpu_power_status_at_wdt(void)
{
	unsigned long bitmask = 0, ret;

	ret = readl(SLEEP_BASE + cfg_pc_latch.spm_pwr_sts);

	/* CPU0 ~ CPU3 */
	bitmask |= (ret & (0xf << 9)) >> 9;
	/* CPU4 ~ CPU7 */
	bitmask |= (ret & (0xf << 16)) >> 12;

	return bitmask;
}

unsigned int plt_get_dfd_dump_type(void)
{
	/* for mt6757 with DFD 3.0 -> always dump to DRAM*/
	if (cfg_dfd.version >= DFD_V3_0)
		return DFD_DUMP_TO_DRAM;
	else
		return DFD_DUMP_NOT_SUPPORT;
}

static unsigned int save_cpu_bus_data(u64 offset, int *len, CALLBACK dev_write)
{
	char *buf = NULL;
	int ret;
	unsigned int datasize = 0;

	/* Save latch buffer */
	ret = latch_get((void **)&buf, len);
	if (buf != NULL) {
		if (*len > 0)
			datasize = dev_write(buf, *len);
		latch_put((void **)&buf);
	}

	/* Save systracker buffer */
	ret = systracker_get((void **)&buf, len);
	if (buf != NULL) {
		if (*len > 0)
			datasize += dev_write(buf, *len);
		systracker_put((void **)&buf);
	}

	return datasize;
}

static unsigned int save_dfd_data(u64 offset, int *len, CALLBACK dev_write)
{
	char *buf = NULL;
	unsigned int datasize = 0;

	/* Save dfd buffer */
	if (dfd_get((void **)&buf, len)) {
		datasize = dev_write(buf, *len);
		dfd_put((void **)&buf);
	}

	return datasize;
}


void platform_clear_cache_retention_select(void)
{
	rgu_release_rg_mcu_pwr_iso_dis();
	rgu_release_rg_mcu_pwr_on();
}

/* SPM2 Debug Features */
#define SPM2_WDT_LATCH0     (0x10227000 + 0x190)
#define SPM2_WDT_LATCH1     (0x10227000 + 0x194)
#define SPM2_WDT_LATCH2     (0x10227000 + 0x198)
#define SPM2_WDT_LATCH_NUM  (3)

static unsigned long get_spm2_wdt_latch(int index)
{
	unsigned long ret;

	switch (index) {
		case 0:
			ret = readl(SPM2_WDT_LATCH0);
			break;
		case 1:
			ret = readl(SPM2_WDT_LATCH1);
			break;
		case 2:
			ret = readl(SPM2_WDT_LATCH2);
			break;
		default:
			ret = 0;
			break;
	}

	return ret;
}

/* VCOREFS Debug Features */
#define VCOREFS_SRAM_BASE		(0x0011CF80)
#define VCOREFS_SRAM_DVS_UP_COUNT	(VCOREFS_SRAM_BASE + 0x54)
#define VCOREFS_SRAM_DFS_UP_COUNT	(VCOREFS_SRAM_BASE + 0x58)
#define VCOREFS_SRAM_DVS_DOWN_COUNT	(VCOREFS_SRAM_BASE + 0x5c)
#define VCOREFS_SRAM_DFS_DOWN_COUNT	(VCOREFS_SRAM_BASE + 0x60)
#define VCOREFS_SRAM_DVFS_UP_LATENCY	(VCOREFS_SRAM_BASE + 0x64)
#define VCOREFS_SRAM_DVFS_DOWN_LATENCY	(VCOREFS_SRAM_BASE + 0x68)
#define VCOREFS_SRAM_DVFS_LATENCY_SPEC	(VCOREFS_SRAM_BASE + 0x6c)
#define VCOREFS_SRAM_DRAM_GATING_CHECK	(VCOREFS_SRAM_BASE + 0x70)
#define VCOREFS_SRAM_NUM		(8)

static unsigned long get_vcorefs_sram(int index)
{
	unsigned long ret;

	switch (index) {
		case 0:
			ret = readl(VCOREFS_SRAM_DVS_UP_COUNT);
			break;
		case 1:
			ret = readl(VCOREFS_SRAM_DFS_UP_COUNT);
			break;
		case 2:
			ret = readl(VCOREFS_SRAM_DVS_DOWN_COUNT);
			break;
		case 3:
			ret = readl(VCOREFS_SRAM_DFS_DOWN_COUNT);
			break;
		case 4:
			ret = readl(VCOREFS_SRAM_DVFS_UP_LATENCY);
			break;
		case 5:
			ret = readl(VCOREFS_SRAM_DVFS_DOWN_LATENCY);
			break;
		case 6:
			ret = readl(VCOREFS_SRAM_DVFS_LATENCY_SPEC);
			break;
		case 7:
			ret = readl(VCOREFS_SRAM_DRAM_GATING_CHECK);
			break;
		default:
			ret = 0;
			break;
	}

	return ret;
}

/* SPM Debug Features */
#define PCM_WDT_LATCH_0     (SLEEP_BASE + 0x190)
#define PCM_WDT_LATCH_1     (SLEEP_BASE + 0x194)
#define PCM_WDT_LATCH_2     (SLEEP_BASE + 0x198)
#define PCM_WDT_LATCH_3     (SLEEP_BASE + 0x1C4)
#define DRAMC_DBG_LATCH     (SLEEP_BASE + 0x19C)
#define PCM_WDT_LATCH_NUM   (5)
#define SPM_DATA_BUF_LENGTH (2048)
static unsigned long get_spm_wdt_latch(int index)
{
	unsigned long ret;

	switch (index) {
		case 0:
			ret = readl(PCM_WDT_LATCH_0);
			break;
		case 1:
			ret = readl(PCM_WDT_LATCH_1);
			break;
		case 2:
			ret = readl(PCM_WDT_LATCH_2);
			break;
		case 3:
			ret = readl(PCM_WDT_LATCH_3);
			break;
		case 4:
			ret = readl(DRAMC_DBG_LATCH);
			break;
		default:
			ret = 0;
			break;
	}

	return ret;
}

static int spm_dump_data(char *buf, int *wp)
{
	int i;
	unsigned long val;

	if (buf == NULL || wp == NULL)
		return -1;

	/*
	 * Example output:
	 * SPM Suspend debug regs(index 1) = 0x8320535
	 * SPM Suspend debug regs(index 2) = 0xfe114200
	 * SPM Suspend debug regs(index 3) = 0x3920fffe
	 * SPM Suspend debug regs(index 4) = 0x3ac06f4f
	 */

	for (i = 0; i < PCM_WDT_LATCH_NUM; i++) {
		val = get_spm_wdt_latch(i);
		*wp += sprintf(buf + *wp,
		               "SPM Suspend debug regs(index %d) = 0x%x\n",
		               i + 1, val);
	}

	for (i = 0; i < VCOREFS_SRAM_NUM; i++) {
		val = get_vcorefs_sram(i);
		*wp += sprintf(buf + *wp,
		               "vcore dvfs debug regs(index %d) = 0x%x\n",
		               i + 1, val);
	}

	for (i = 0; i < SPM2_WDT_LATCH_NUM; i++) {
		val = get_spm2_wdt_latch(i);
		*wp += sprintf(buf + *wp,
		               "SPM2_WDT_Latch%d = 0x%x\n",
		               i, val);
	}

	*wp += sprintf(buf + *wp, "\n");
	return 1;
}

int spm_data_get(void **data, int *len)
{
	int ret;

	*len = 0;
	*data = malloc(SPM_DATA_BUF_LENGTH);
	if (*data == NULL)
		return 0;

	ret = spm_dump_data(*data, len);
	if (ret < 0 || *len > SPM_DATA_BUF_LENGTH) {
		*len = (*len > SPM_DATA_BUF_LENGTH) ? SPM_DATA_BUF_LENGTH : *len;
		return ret;
	}

	return 1;
}

void spm_data_put(void **data)
{
	free(*data);
}

static unsigned int save_spm_data(u64 offset, int *len, CALLBACK dev_write)
{
	char *buf = NULL;
	unsigned int datasize = 0;

	/* Save SPM buffer */
	spm_data_get((void **)&buf, len);
	if (buf != NULL) {
		if (*len > 0)
			datasize = dev_write(buf, *len);
		spm_data_put((void **)&buf);
	}

	return datasize;
}

/* FOR DRAMC data and DRAM Calibration Log
*
* This area is applied for DRAM related debug.
*/
/* DRAMC data */
#define DRAM_DEBUG_SRAM_ADDRESS 0x11D800
#define DRAM_DEBUG_SRAM_LENGTH  1024
static int plat_dram_debug_get(void **data, int *len)
{
	*data = (void *)DRAM_DEBUG_SRAM_ADDRESS;
	*len = DRAM_DEBUG_SRAM_LENGTH;
	return 1;
}

/* shared SRAM for DRAMLOG calibration log */
#define DRAM_KLOG_SRAM_ADDRESS 0x0011E000
#define DRAM_KLOG_SRAM_LENGTH  8192
#define DRAM_KLOG_VALID_ADDRESS 0x0011E00C
static int plat_dram_klog_get(void **data, int *len)
{
	*data = (void *)DRAM_KLOG_SRAM_ADDRESS;
	*len = DRAM_KLOG_SRAM_LENGTH;
	return 1;
}

static bool plat_dram_has_klog(void)
{
	if (*(volatile unsigned int*)DRAM_KLOG_VALID_ADDRESS)
		return true;

	return false;
}

static unsigned int save_dram_data(u64 offset, int *len, CALLBACK dev_write)
{
	char *buf = NULL;
	unsigned int datasize = 0, allsize = 0;

	if (plat_dram_debug_get((void **)&buf, len)) {
		datasize = dev_write(buf, *len);
		allsize = datasize;
	}

	if (plat_dram_klog_get((void **)&buf, len)) {
		buf = malloc(*len);
		if (buf) {
			mrdump_read_log(buf, *len, MRDUMP_EXPDB_DRAM_KLOG_OFFSET);
			datasize = dev_write(buf, *len);
			allsize += datasize;
			free(buf);
		}
	}

	return allsize;
}

static int plat_write_dram_klog(void)
{
	char *sram_base = NULL;
	int len = 0;

	if (plat_dram_klog_get((void **)&sram_base, (int *)&len)) {
		if (plat_dram_has_klog()) {
			mrdump_write_log(MRDUMP_EXPDB_DRAM_KLOG_OFFSET, sram_base, len);
		}
	}
	return 0;
}

/* SRAM for Hybrid CPU DVFS */
#define HVFS_SRAM_ADDRESS 0x0011c000
#define HVFS_SRAM_LENGTH  0xf80		/* 3968 bytes */
static int plat_hvfs_data_get(void **data, int *len)
{
	*data = (void *)HVFS_SRAM_ADDRESS;
	*len = HVFS_SRAM_LENGTH;
	return 1;
}

static unsigned int save_hvfs_data(u64 offset, int *len, CALLBACK dev_write)
{
	char *buf = NULL;
	unsigned int datasize = 0;

	if (plat_hvfs_data_get((void **)&buf, len)) {
		datasize = dev_write(buf, *len);
	}

	return datasize;
}

/* last pl_lk log */
static int plat_pllk_log_get(void **data, int *len)
{
	*data = (void *)mirror_buf_addr_get();
	*len = mirror_buf_pl_lk_log_size_get();
	if((*data == 0) || (*len == 0)) {
		dprintf(CRITICAL, "[LK_LOG_STORE] invalid address or log length(addr 0x%x, len 0x%x)\n", (unsigned int)*data, (unsigned int)*len);
		return 0;
	}
	dprintf(CRITICAL, "[LK_LOG_STORE] the mirror buf addr is 0x%x, log len is 0x%x\n", (unsigned int)*data, (unsigned int)*len);
	return 1;
}

static unsigned int save_pllk_last_log(u64 offset, int *len, CALLBACK dev_write)
{
	char *buf = NULL;
	unsigned int datasize = 0;

	if (plat_pllk_log_get((void **)&buf, len)) {
		datasize = dev_write(buf, *len);
	}

	return datasize;
}

/* platform initial function */
int platform_debug_init(void)
{
	/* function pointer assignment */
	plat_spm_data_get = save_spm_data;
	plat_dram_get = NULL;
	plat_cpu_bus_get = save_cpu_bus_data;
	plat_hvfs_get = save_hvfs_data;
	plat_pllk_last_log_get = save_pllk_last_log;

	return 1;
}

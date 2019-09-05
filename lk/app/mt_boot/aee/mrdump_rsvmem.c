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

#include <block_generic_interface.h>
#include <malloc.h>
#include <printf.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <video.h>
#include <dev/mrdump.h>
#include <platform/env.h>
#include <platform/mtk_key.h>
#include <platform/mtk_wdt.h>
#include <platform/mt_gpt.h>
#include <target/cust_key.h>
#include <platform/boot_mode.h>
#include <bootimg.h>
#include <platform/ram_console.h>
#include <arch/ops.h>
#include <libfdt.h>
#include <platform.h>
#include "aee.h"
#include "kdump.h"

extern BOOT_ARGUMENT *g_boot_arg;
extern void *g_fdt;
extern struct bootimg_hdr *g_boot_hdr;

#define RSV_MEM_LEN 128

void mrdump_reserve_memory(void)
{
	char *mrdump_rsv_mem = malloc(RSV_MEM_LEN);
	char *mrdump_rsv_tmp = malloc(RSV_MEM_LEN);

	/*
	 * mrdump_rsv_mem format , a pair of start address and size
	 * example 0x46000000 0x200000,0x47000000,0x100000
	 * current limitation is 16 pair in kernel
	 * */
	if (mrdump_rsv_mem && mrdump_rsv_tmp) {
		snprintf(mrdump_rsv_mem, RSV_MEM_LEN, "mrdump_rsvmem=0x%x,0x%x,0x%x,0x%x,0x%x,0x%x",
		         MEMBASE, AEE_MRDUMP_LK_RSV_SIZE, (unsigned int)BOOT_ARGUMENT_LOCATION&0xfff00000
		         , (g_boot_arg->dram_buf_size)?g_boot_arg->dram_buf_size:0x100000,
			g_boot_hdr->tags_addr, fdt_totalsize(g_fdt));

		if(platform_get_bootarg_addr && platform_get_bootarg_size) {
			snprintf(mrdump_rsv_tmp,RSV_MEM_LEN,",0x%x,0x%x", platform_get_bootarg_addr(), platform_get_bootarg_size());
			strncat(mrdump_rsv_mem, mrdump_rsv_tmp, RSV_MEM_LEN);
		}

		cmdline_append(mrdump_rsv_mem);
	} else
		dprintf(CRITICAL, "MT-RAMDUMP: mrdump_rsv_mem malloc memory failed");

	free(mrdump_rsv_mem);
	free(mrdump_rsv_tmp);
}

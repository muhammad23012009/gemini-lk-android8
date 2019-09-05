/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2017. All rights reserved.
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

#include <debug.h>
#include <iothread.h>
#include <profiling.h>
#include <spm_common.h>


/******************************************************************************
******************************************************************************/
event_t iothread_start_event;
event_t iothread_end_event;
extern BOOTMODE g_boot_mode;


/******************************************************************************
******************************************************************************/
extern int mcdi_load_mcupm(void);
/******************************************************************************
******************************************************************************/

static void do_load_spm(BOOTMODE mode)
{
#ifdef SPM_FW_USE_PARTITION
	PROFILING_START("load spm");
	load_spm();
	PROFILING_END();
#endif

}

static void do_load_scp(BOOTMODE mode)
{
#ifdef MTK_TINYSYS_SCP_SUPPORT
	PROFILING_START("load scp");
	if (load_scp() < 0) {
		/* if it load scp fail, turn off SCP hw*/
		disable_scp_hw();
	}
	PROFILING_END();
#endif
}

static void do_load_mcupm(BOOTMODE mode)
{
#ifdef MCUPM_FW_USE_PARTITION
	PROFILING_START("load mcupm");
	mcdi_load_mcupm();
	PROFILING_END();
#endif
}

static void do_load_modem(BOOTMODE mode)
{
	PROFILING_START("load modem image");
	/* NOTE: This MUST before setup_mem_property_use_mblock_info() !!! */
	if (mode != RECOVERY_BOOT) {
		extern void load_modem_image(void)__attribute__((weak));
		if (load_modem_image) {
			load_modem_image();
		} else {
			dprintf(CRITICAL,"[ccci] modem standalone not support\n");
		}
	} else {
		dprintf(CRITICAL,"[ccci] recovery mode, no need load modem\n");
	}
#if !defined(NO_MD_POWER_DOWN)
	/* just for 93: SBC on 93, need call power off after load modem image */
	extern void md_power_down(void)__attribute__((weak));
	if (md_power_down) {
		md_power_down();
	} else {
		dprintf(CRITICAL,"[ccci-off] later power down not needed or not ready!\n");
	}
#endif
	PROFILING_END();
}

static void do_load_vpu(BOOTMODE mode)
{
#ifdef MTK_VPU_SUPPORT
	int ret;
	PROFILING_START("load vpu");
	if (mode != RECOVERY_BOOT) {
		if ((ret = mt_load_vpu()) < 0) {
			dprintf(CRITICAL, "[LK ERROR] load VPU fail(ret = %d)!!!\n", ret);
		}
	}
	PROFILING_END();
#endif
}

static void do_load_images(BOOTMODE mode)
{
	do_load_vpu(mode);
	do_load_spm(mode);
	do_load_scp(mode);
	do_load_mcupm(mode);
	do_load_modem(mode);
}


/******************************************************************************
******************************************************************************/
#if !defined(ENABLE_IO_THREAD)
void load_images(BOOTMODE mode)
{
	do_load_images(mode);
}

#else

void load_images(BOOTMODE mode)
{
	/* Empty! */
}
#endif  // ENABLE_IO_THREAD


/******************************************************************************
******************************************************************************/
#if defined(ENABLE_IO_THREAD)
static void load_images_in_iothread(BOOTMODE mode)
{
	do_load_images(mode);
}

#else

static void load_images_in_iothread(BOOTMODE mode)
{
	/* Empty! */
}
#endif  // ENABLE_IO_THREAD


/******************************************************************************
******************************************************************************/
int iothread(void *arg)
{
	event_init(&iothread_start_event, false, 0);
	event_init(&iothread_end_event, false, 0);
	dprintf(INFO, "iothread starts.\n");

	wait_for_bootstrap2();

	load_images_in_iothread(g_boot_mode);

	wake_up_bootstrap2();

	return 0;
}



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
 * THAT IT IS RECEIVER\'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER\'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER\'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK\'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK\'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver\'s
 * applicable license agreements with MediaTek Inc.
 */

#if !defined(__KDUMP_USB_C__)
#define __KDUMP_USB_C__

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
#include <platform/mt_leds.h>
#include <platform/mt_gpt.h>
#include <target/cust_key.h>
#include <platform/boot_mode.h>
#include <platform/ram_console.h>
#include <arch/ops.h>
#include <libfdt.h>
#include <platform.h>
#include "aee.h"
#include "kdump.h"

/* USB Dump Menu */
extern BOOTMODE g_boot_mode;
#define MRDUMP_KEY_INTERVAL     250
#define MRDUMP_KEY_SAMPLES      4 /* 1000/250 */
#define MRDUMP_VOL_UP           MT65XX_MENU_SELECT_KEY
#define MRDUMP_VOL_DN           MT65XX_MENU_OK_KEY
#define MRDUMP_COUNTDOWN_TIME   20

static void mrdump_show_menu(int select, char *ud_msg, int cnt)
{
	/* select: 0(normal), 1(usb) */
	video_set_cursor(0, 0);
	video_printf(ud_msg);
	switch (select) {
		case 1:
			video_printf("[ NORMAL DUMP ]         \n");
			video_printf("[ USB    DUMP ]   <<==  \n");
			break;
		default:
			video_printf("[ NORMAL DUMP ]   <<==  \n");
			video_printf("[ USB    DUMP ]         \n");
			break;
	}
	video_printf("\nCount Down for NORMAL-DUMP: %02d\n\n", cnt);
	return;
}

void mrdump_usb_dump_select(const struct mrdump_control_block *mrdump_cb)
{
#ifdef MRDUMP_DEFAULT_USB_DUMP
	int select = 1;
#else
	int select = 0;
#endif
	int cnt = MRDUMP_COUNTDOWN_TIME, loop = 0;
	char ud_msg[128];

	mt65xx_backlight_on();
	video_clean_screen();

	snprintf(ud_msg, sizeof(ud_msg), "\n\n[Note] lbaooo = %u", mrdump_cb->output_fs_lbaooo);
	snprintf(ud_msg, sizeof(ud_msg), "%s\n[VOLUME_UP to select.  VOLUME_DOWN is ok]\n", ud_msg);
	snprintf(ud_msg, sizeof(ud_msg), "%s\nSelect Dump Mode:\n", ud_msg);
	mrdump_show_menu(select, ud_msg, cnt);

	while (1) {
		mtk_wdt_restart();

		if (mtk_detect_key(MRDUMP_VOL_DN))
			break;

		if (mtk_detect_key(MRDUMP_VOL_UP)) {
			select++;
			select%=2;
			mrdump_show_menu(select, ud_msg, cnt);
			cnt = MRDUMP_COUNTDOWN_TIME;
		}

		if (cnt == 0)
			break;

		if (loop == 0) {
			mrdump_show_menu(select, ud_msg, cnt);
			cnt--;
		}

		mdelay(MRDUMP_KEY_INTERVAL);
		loop++;
		loop%=MRDUMP_KEY_SAMPLES;
	}

	if (select) {
		g_boot_mode = FASTBOOT;
		set_env("mrdump_output", "usb");
	}

	return;
}

/* USB Connectivity */
extern int usb_write(void *buf, unsigned len);
extern int usb_read(void *buf, unsigned len);

/*
 * Note: for usb transmission
 *   QMU mode : MAX packet length: 63x1024 = 64512 byte. -> GPD_BUF_SIZE_ALIGN
 *   ZLP issue: EXSPACE should not be multiple size of 512 byte.
 */
#define EXSPACE      64256
#define SIZE_1MB     1048576ULL
#define SIZE_64MB    67108864ULL
#define MAX_RSP_SIZE 64

/* Flow control */
#define USB_RTS      "_RTS"
#define USB_CTS      "_CTS"
#define USB_FIN      "_FIN"
#define USBDONE      "DONE"

struct mrdump_usb_handle {
	unsigned int zipsize;
	uint8_t data[EXSPACE];
	int idx;
};

/* flow control of usb connection */
static bool usb_data_transfer(struct mrdump_usb_handle *handle, int length)
{
	int len;
	char cmd[MAX_RSP_SIZE];

	/* send RTS */
	memset(cmd, 0, MAX_RSP_SIZE);
	len = snprintf(cmd, MAX_RSP_SIZE, "%s", USB_RTS);

	len = usb_write(cmd, strlen(cmd));
	if (len > 0) {

		/* receive CTS */
		memset(cmd, 0, MAX_RSP_SIZE);
		len = usb_read(cmd, MAX_RSP_SIZE);
		if ((len == (int)strlen(USB_CTS))&&
		    (!strncmp(cmd, USB_CTS, strlen(USB_CTS)))) {

			/* send DATA */
			while (true) {
				len = usb_write(handle->data, length);
				if (len > 0) {

					/* get FIN */
					memset(cmd, 0, sizeof(cmd));
					len = usb_read(cmd, sizeof(cmd));
					if ((len == (int)strlen(USB_FIN)) &&
					    (!strncmp(cmd, USB_FIN, strlen(USB_FIN)))) {
						return true;
					} else {
						mdelay(5);
						continue;
					}

				} else {
					mdelay(5);
					continue;
				}
			}
		} else {
			voprintf_error("%s: Not CTS after RTS.\n", __func__);
		}
	} else {
		voprintf_error("%s: RTS write error.\n", __func__);
	}
	return false;
}

/* store data in pool (EXSPACE) and write when pool is full */
static int do_store_or_write(struct mrdump_usb_handle *handle, uint8_t *buf, uint32_t length)
{
	int total;
	unsigned int leftspace, mylen, reval;


	/* count for leftspace */
	total = EXSPACE;
	leftspace = total - handle->idx;

	/* check length */
	if (length > leftspace) {
		mylen = leftspace;
		reval = length - leftspace;
	} else {
		mylen = length;
		reval = 0;
	}

	/* store */
	while (mylen > 0) {
		handle->data[handle->idx] = *buf;
		handle->idx++;
		buf++;
		mylen--;
	}

	/* write */
	if (handle->idx == total) {
		if (!usb_data_transfer(handle, handle->idx)) {
			voprintf_error("%s: connection failed.(error idx: %d)\n",
			__func__, handle->idx);
			return -1;
		}
		handle->idx = 0;
	}

	return reval;
}

static int usb_write_cb(void *opaque_handle, void *buf, int size)
{
	unsigned int    len, moves;
	int             ret = 0;
	uint8_t         *ptr;

	struct mrdump_usb_handle *handle = opaque_handle;

	handle->zipsize += size;

	/* EOF, write the left Data in handle data buffer... */
	if ((buf == NULL) && (size == 0)) {
		if (!usb_data_transfer(handle, handle->idx)) {
			voprintf_error("%s: connection failed.(error idx: %d)\n",
			__func__, handle->idx);
			return -1;
		}

		/* send "MRDUMP ZLP" */
		size = snprintf((char *)handle->data, sizeof(handle->data), "%s_%s\0",
				MRDUMP_GO_DUMP, USBDONE);
		if (0 > usb_write(handle->data, strlen(handle->data))) {
			voprintf_error(" USB Dump: Write ZLP failed.\n");
			return -1;
		}

		return 0;
	}

	/* buf should not be NULL if not EOF */
	if (buf == NULL)
		return -1;

	/* process of Store and write */
	len = size;
	ptr = (uint8_t *)buf;
	while (1) {
		ret = do_store_or_write(handle, ptr, len);
		if (ret < 0) {
			voprintf_error(" USB Dump: store and write failed.\n");
			return -1;
		} else if (ret == 0) {
			break;
		} else {
			moves = len - ret;
			ptr += moves;
			len = ret;
		}
	}

	return size;
}

int kdump_usb_output(const struct mrdump_control_block *mrdump_cb,
		     const struct kzip_memlist *memlist)
{
	voprintf_info("Output by USB\n");

	struct mrdump_usb_handle *handle = memalign(16, sizeof(struct mrdump_usb_handle));
	if (handle == NULL) {
		voprintf_error("No enough memory.");
		return -1;
	}
	memset(handle, 0, sizeof(struct mrdump_usb_handle));

	mdelay(100);
	bool ok = true;
	mtk_wdt_restart();
	struct kzip_file *zf = kzip_open(handle, usb_write_cb);
	if (zf != NULL) {
		if (!kzip_add_file(zf, memlist, "SYS_COREDUMP")) {
			ok = false;
		}
		kzip_close(zf);
		usb_write_cb(handle, NULL, 0); /* really write the last part */
		zf = NULL;
	} else {
		ok = false;
	}
	free(handle);

	mtk_wdt_restart();
	if (ok) {
		mrdump_status_ok("OUTPUT:%s\nMODE:%s\n", "USB DUMP",
		mrdump_mode2string(mrdump_cb->crash_record.reboot_mode));
	}

	return ok ? 0 : -1;
}

#endif

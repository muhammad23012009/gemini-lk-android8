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

#ifndef __COMMANDS_H
#define __COMMANDS_H

void cmd_getvar(const char *arg, void *data, unsigned sz);
void cmd_boot(const char *arg, void *data, unsigned sz);
void cmd_reboot(const char *arg, void *data, unsigned sz);
void cmd_reboot_bootloader(const char *arg, void *data, unsigned sz);
void cmd_download(const char *arg, void *data, unsigned sz);
void cmd_overwirte_cmdline(const char *arg, void *data, unsigned sz);
void cmd_continue(const char *arg, void *data, unsigned sz);
void cmd_oem_p2u(const char *arg, void *data, unsigned sz);
void cmd_oem_reboot2recovery(const char *arg, void *data, unsigned sz);
void cmd_oem_append_cmdline(const char *arg, void *data, unsigned sz);
#ifdef MTK_JTAG_SWITCH_SUPPORT
void cmd_oem_ap_jtag(const char *arg, void *data, unsigned sz);
#endif
#ifdef MTK_TINYSYS_SCP_SUPPORT
void cmd_oem_scp_status(const char *arg, void *data, unsigned sz);
#endif
#ifdef MTK_OFF_MODE_CHARGE_SUPPORT
void cmd_oem_off_mode_charge(const char *arg, void *data, unsigned sz);
#endif
#ifdef MTK_MRDUMP_USB_DUMP
void cmd_oem_mtkreboot(const char *arg, void *data, unsigned sz);
void cmd_oem_mrdump(const char *arg, void *data, unsigned sz);
#endif
#ifdef MTK_TC7_COMMON_DEVICE_INTERFACE
void cmd_oem_ADB_Auto_Enable(const char *arg, void *data, unsigned sz);
#endif
#ifdef MTK_USB2JTAG_SUPPORT
void cmd_oem_usb2jtag(const char *arg, void *data, unsigned sz);
#endif
#ifdef MTK_AB_OTA_UPDATER
void cmd_set_active(const char *arg, void *data, unsigned sz);
#endif
#endif

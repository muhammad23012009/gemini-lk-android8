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
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver\'s
 * applicable license agreements with MediaTek Inc.
 */

#include <target/board.h>
#include <platform/mt_typedefs.h>
#include <platform/mt_reg_base.h>
#include <platform/mt_pmic.h>
#include <platform/mt_pmic_dlpt.h>
#include <platform/upmu_hw.h>
#include <platform/upmu_common.h>
#include <platform/boot_mode.h>
#include <platform/mt_gpt.h>
#include <platform/mt_rtc.h>
#include <platform/mt_rtc_hw.h>
#include <platform/mt_pmic_wrap_init.h>
#include <platform/mt_leds.h>
#include <printf.h>
#include <sys/types.h>
#include <mtk_battery.h>
#include <mtk_charger.h>

bool g_fg_is_charging = 0;

/*****************************************************************************
 *  Externl Variable
 ****************************************************************************/
extern bool g_boot_menu;
extern int fg_swocv_v;
extern int fg_swocv_i;
extern int shutdown_time;
extern int boot_voltage;

bool mtk_bat_allow_backlight_enable(void)
{
	int bat_vol = 0;
	bat_vol = get_bat_volt(1);
	if (bat_vol > (BATTERY_LOWVOL_THRESOLD + 150))
		return true;
	return false;
}

void fgauge_read_IM_current(void *data)
{
		unsigned short uvalue16 = 0;
		signed int dvalue = 0;
		signed int ori_curr = 0;
		signed int curr_rfg = 0;
		/*int m = 0;*/
		long long Temp_Value = 0;
		/*unsigned int ret = 0;*/

        /* FIXME: Need to replace with general PMIC interface */
		uvalue16 = pmic_get_register_value(PMIC_FG_R_CURR);

		/*calculate the real world data    */
		dvalue = (unsigned int) uvalue16;
		if (dvalue == 0) {
			Temp_Value = (long long) dvalue;
			g_fg_is_charging = false;
		} else if (dvalue > 32767) {
			/* > 0x8000 */
			Temp_Value = (long long) (dvalue - 65535);
			Temp_Value = Temp_Value - (Temp_Value * 2);
			g_fg_is_charging = false;
		} else {
			Temp_Value = (long long) dvalue;
			g_fg_is_charging = true;
		}

		Temp_Value = Temp_Value * UNIT_FGCURRENT;
		Temp_Value = Temp_Value/100000;
		dvalue = (unsigned int) Temp_Value;

		ori_curr = dvalue;
		curr_rfg = dvalue;

		/* Auto adjust value */
		if (R_FG_VALUE != 100) {
			dvalue = (dvalue * 100) / R_FG_VALUE;
			curr_rfg = dvalue;
		}

		/* dprintf(INFO,"[fgauge_read_IM_current] ori current=%d\n", dvalue); */

		dvalue = ((dvalue * CAR_TUNE_VALUE) / 1000);

		if (g_fg_is_charging == true) {
			dprintf(CRITICAL,"[fgauge_read_IM_current](charging)FG_CURRENT:0x%x,curr:[%d,%d,%d] mA, Rfg:%d ratio:%d\n",
				uvalue16, ori_curr, curr_rfg, dvalue, R_FG_VALUE, CAR_TUNE_VALUE);

		} else {
			dprintf(CRITICAL,"[fgauge_read_IM_current](discharg)FG_CURRENT:0x%x,curr:[%d,%d,%d] mA, Rfg:%d ratio:%d\n",
				uvalue16, ori_curr, curr_rfg, dvalue, R_FG_VALUE, CAR_TUNE_VALUE);
		}

		*(signed int *) (data) = dvalue;
}

void check_sw_ocv(void)
{
	unsigned int ptim_bat_vol = 0;
	signed int ptim_R_curr = 0;
	int bat_vol;

	if(!is_disable_charger()) {
		shutdown_time = g_boot_arg->shutdown_time;
		boot_voltage = g_boot_arg->boot_voltage;

		charger_enable_charging(false);
		bat_vol = get_bat_volt(1);

		if (upmu_is_chr_det() == true)
			mdelay(50);

		do_ptim_gauge(&ptim_bat_vol, &ptim_R_curr);

		fg_swocv_v = ptim_bat_vol;
		if (g_fg_is_charging == true)
			fg_swocv_i = ptim_R_curr;
		else
			fg_swocv_i = -ptim_R_curr;

		dprintf(CRITICAL, "[check_sw_ocv]%d ptim[%d %d] fg_swocv[%d %d] boot_vbat:%d shutdowntime:%d vbat:%d\n",
			g_fg_is_charging, ptim_bat_vol, ptim_R_curr, fg_swocv_v, fg_swocv_i,
			g_boot_arg->boot_voltage, g_boot_arg->shutdown_time, bat_vol);

		charger_enable_charging(true);
	}
}

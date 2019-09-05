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


/*=======================================================================*/
/* HEADER FILES                                                          */
/*=======================================================================*/


#include <string.h>
#include <stdlib.h>
#include <video_fb.h>
#include <platform/disp_drv_platform.h>
#include <target/board.h>
#include <platform/env.h>
#include "lcm_drv.h"
#include <platform/mt_gpt.h>
#include <platform/primary_display.h>
#include <arch/arm/mmu.h>

//#define DFO_DISP
#define FB_LAYER            0
#ifndef MTK_ROUND_CORNER_SUPPORT
#define BOOT_MENU_LAYER     3
#else
#define BOOT_MENU_LAYER     1
#define TOP_LAYER           2
#define BOTTOM_LAYER        3
#endif

unsigned long long  fb_addr_pa_k    = 0;
static void  *fb_addr_pa      = NULL;
static void  *fb_addr      = NULL;
static void  *logo_db_addr = NULL;
static void  *logo_db_addr_pa = NULL;
static UINT32 fb_size      = 0;
static UINT32 fb_offset_logo = 0; // counter of fb_size
static UINT32 fb_isdirty   = 0;
static UINT32 redoffset_32bit = 1; // ABGR

extern LCM_PARAMS *lcm_params;

extern void disp_log_enable(int enable);
extern void dbi_log_enable(int enable);
extern void * memset(void *,int,unsigned int);
extern void arch_clean_cache_range(addr_t start, size_t len);
extern UINT32 memory_size(void);
extern void mt_disp_update(UINT32 x, UINT32 y, UINT32 width, UINT32 height);
UINT32 mt_disp_get_vram_size(void)
{
	return DISP_GetVRamSize();
}


#ifdef DFO_DISP
static disp_dfo_item_t disp_dfo_setting[] = {
	{"LCM_FAKE_WIDTH",  0},
	{"LCM_FAKE_HEIGHT", 0},
	{"DISP_DEBUG_SWITCH",   0}
};

#define MT_DISP_DFO_DEBUG
#ifdef MT_DISP_DFO_DEBUG
#define disp_dfo_printf(string, args...) dprintf(INFO,"[DISP_DFO]"string, ##args)
#else
#define disp_dfo_printf(string, args...) ()
#endif

unsigned int mt_disp_parse_dfo_setting(void)
{
	unsigned int i, j=0 ;
	char tmp[11];
	char *buffer = NULL;
	char *ptr = NULL;

	buffer = (char *)get_env("DFO");
	disp_dfo_printf("env buffer = %s\n", buffer);

	if (buffer != NULL) {
		for (i = 0; i< (sizeof(disp_dfo_setting)/sizeof(disp_dfo_item_t)); i++) {
			j = 0;

			memset((void*)tmp, 0, sizeof(tmp)/sizeof(tmp[0]));

			ptr = strstr(buffer, disp_dfo_setting[i].name);

			if (ptr == NULL) continue;

			disp_dfo_printf("disp_dfo_setting[%d].name = [%s]\n", i, ptr);

			do {} while ((*ptr++) != ',');

			do {
				tmp[j++] = *ptr++;
			} while (*ptr != ',' && j < sizeof(tmp)/sizeof(tmp[0]));

			disp_dfo_setting[i].value = atoi((const char*)tmp);

			disp_dfo_printf("disp_dfo_setting[%d].name = [%s|%d]\n", i, tmp, disp_dfo_setting[i].value);
		}
	} else {
		disp_dfo_printf("env buffer = NULL\n");
	}

	return 0;
}


int mt_disp_get_dfo_setting(const char *string, unsigned int *value)
{
	char *disp_name;
	int  disp_value;
	unsigned int i = 0;

	if (string == NULL)
		return -1;

	for (i=0; i<(sizeof(disp_dfo_setting)/sizeof(disp_dfo_item_t)); i++) {
		disp_name = disp_dfo_setting[i].name;
		disp_value = disp_dfo_setting[i].value;
		if (!strcmp(disp_name, string)) {
			*value = disp_value;
			disp_dfo_printf("%s = [DEC]%d [HEX]0x%08x\n", disp_name, disp_value, disp_value);
			return 0;
		}
	}

	return 0;
}
#endif

static void _mtkfb_draw_block(unsigned int addr, unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int color)
{
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int start_addr = addr+ALIGN_TO(CFG_DISPLAY_WIDTH, MTK_FB_ALIGNMENT)*4*y+x*4;
	for (j=0; j<h; j++) {
		for (i = 0; i<w; i++) {
			*(unsigned int*)(start_addr + i*4 + j*ALIGN_TO(CFG_DISPLAY_WIDTH, MTK_FB_ALIGNMENT)*4) = color;
		}
	}
}
#if 0
static int _mtkfb_internal_test(unsigned int va, unsigned int w, unsigned int h)
{
	// this is for debug, used in bring up day
	dprintf(0,"_mtkfb_internal_test\n");
	unsigned int i = 0;
	unsigned int color = 0;
	int _internal_test_block_size =240;
	for (i=0; i<w*h/_internal_test_block_size/_internal_test_block_size; i++) {
		color = (i&0x1)*0xff;
		//color += ((i&0x2)>>1)*0xff00;
		//color += ((i&0x4)>>2)*0xff0000;
		color += 0xff000000U;
		_mtkfb_draw_block(va,
		                  i%(w/_internal_test_block_size)*_internal_test_block_size,
		                  i/(w/_internal_test_block_size)*_internal_test_block_size,
		                  _internal_test_block_size,
		                  _internal_test_block_size,
		                  color);
	}
	mt_disp_update(0, 0, CFG_DISPLAY_WIDTH, CFG_DISPLAY_HEIGHT);
	mdelay(100);
	primary_display_diagnose();
	return 0;

	_internal_test_block_size = 20;
	for (i=0; i<w*h/_internal_test_block_size/_internal_test_block_size; i++) {
		color = (i&0x1)*0xff;
		color += ((i&0x2)>>1)*0xff00;
		color += ((i&0x4)>>2)*0xff0000;
		color += 0xff000000U;
		_mtkfb_draw_block(va,
		                  i%(w/_internal_test_block_size)*_internal_test_block_size,
		                  i/(w/_internal_test_block_size)*_internal_test_block_size,
		                  _internal_test_block_size,
		                  _internal_test_block_size,
		                  color);
	}

	mt_disp_update(0, 0, CFG_DISPLAY_WIDTH, CFG_DISPLAY_HEIGHT);
	mdelay(100);
	primary_display_diagnose();
	return 0;
}
#endif

#ifdef MTK_ROUND_CORNER_SUPPORT
void mt_disp_load_image(void *dst, void *src, int h, int picture_w, int w) {
	int i = 0;
	dprintf(0,"Elvin: load_notch_image\n");
	for (i=0; i<h; i++)
		memcpy(dst+picture_w*i*4, src+w*i*4, picture_w*4);
}
void assemble_image(void *dst, void *left, void *right, int h, int picture_w, int w)
{
	int i = 0;
	for (i = 0; i < h; i++) {
		memcpy(dst+picture_w*i*CFG_DISPLAY_BPP / 8, left+w*i*CFG_DISPLAY_BPP / 8, w*CFG_DISPLAY_BPP / 8);
		memcpy(dst+(picture_w*(i+1)-w)*CFG_DISPLAY_BPP / 8, right+w*i*CFG_DISPLAY_BPP / 8, w*CFG_DISPLAY_BPP / 8);
	}
}

int round_corner_init(unsigned int *top_pa, unsigned int *bottom_pa, LCM_ROUND_CORNER *rc_params)
{
	unsigned int h = rc_params->h;
	unsigned int w = rc_params->w;
	unsigned int h_bot = rc_params->h_bot;
	unsigned int w_bot = rc_params->w_bot;
	unsigned int pitch = DISP_GetScreenWidth();
	unsigned char *left_top = rc_params->lt_addr;
	unsigned char *left_bottom = rc_params->lb_addr;
	unsigned char *right_top = rc_params->rt_addr;
	unsigned char *right_bottom = rc_params->rb_addr;
	unsigned int buf_size = 0;
	unsigned int buf_size_bot = 0;

	static void  *top_addr_va = NULL;
	static void  *bottom_addr_va = NULL;

	if (rc_params->full_content) {
		if (h == 0 || w == 0 || left_top == NULL || left_bottom == NULL
			|| h_bot == 0 || w_bot == 0) {
			dprintf(0,"the round corner params is invalid, please check the lcm config\n");
			return -1;
		}

		buf_size = h * pitch * 4;
		*top_pa = (unsigned int)(fb_addr_pa + DISP_GetVRamSize() - buf_size);
		top_addr_va = fb_addr + DISP_GetVRamSize() - buf_size;
		dprintf(0,"top_addr_va: 0x%08x, top_pa: 0x%08x\n",
						(unsigned int)top_addr_va, *top_pa);

		buf_size_bot = h_bot * pitch * 4;
		*bottom_pa = (unsigned int)(fb_addr_pa + DISP_GetVRamSize() - buf_size - buf_size_bot);
		bottom_addr_va = fb_addr + DISP_GetVRamSize() - buf_size - buf_size_bot;
		dprintf(0,"bottom_addr_va: 0x%08x, bottom_pa: 0x%08x\n",
					(unsigned int)bottom_addr_va, *bottom_pa);
		mt_disp_load_image((void *)top_addr_va, (void *)left_top, h, pitch, w);
		mt_disp_load_image((void *)bottom_addr_va, (void *)left_bottom, h_bot, pitch, w_bot);
		dprintf(0,"left_top: 0x%08x, left_bottom: 0x%08x,h:%d,pitch:%d,w:%d,h_bot:%d,w_bot:%d\n",
			(unsigned int)left_top, (unsigned int)left_bottom, h, pitch, w, h_bot, w_bot);
	} else {

		if (h == 0 || w == 0 || left_top == NULL || left_bottom == NULL
			|| right_top==NULL || right_bottom == NULL) {
			dprintf(0,"the round corner params is invalid, please check the lcm config\n");
			return -1;
		}

		buf_size = h * pitch * 4;
		*top_pa = (unsigned int)(fb_addr_pa + DISP_GetVRamSize() - 2 * buf_size);
		top_addr_va = fb_addr + DISP_GetVRamSize() - 2 * buf_size;
		dprintf(0,"top_addr_va: 0x%08x, top_pa: 0x%08x\n",
						(unsigned int)top_addr_va, *top_pa);

		*bottom_pa = (unsigned int)(fb_addr_pa + DISP_GetVRamSize() - buf_size);
		bottom_addr_va = fb_addr + DISP_GetVRamSize() - buf_size;
		dprintf(0,"bottom_addr_va: 0x%08x, bottom_pa: 0x%08x\n",
					(unsigned int)bottom_addr_va, *bottom_pa);

		assemble_image((void *)top_addr_va, (void *)left_top, (void *)right_top, h, pitch, w);
		assemble_image((void *)bottom_addr_va, (void *)left_bottom, (void *)right_bottom, h, pitch, w);
	}
	return 0;
}
#endif


void mt_disp_init(void *lcdbase)
{
	UINT32 boot_mode_addr = 0;
#ifdef MTK_ROUND_CORNER_SUPPORT
		int ret = -1;
		unsigned int top_addr_pa = 0;
		unsigned int bottom_addr_pa = 0;
		LCM_ROUND_CORNER *round_corner = primary_display_get_corner_params();
#endif
	/// fb base pa and va
	fb_addr_pa_k = arm_mmu_va2pa((unsigned int)lcdbase);

	fb_addr_pa   = (void *)(fb_addr_pa_k & 0xffffffffull);
	fb_addr      = lcdbase;

	dprintf(0,"fb_va: 0x%8p, fb_pa: 0x%8p, fb_pa_k: 0x%llx\n", fb_addr, fb_addr_pa, fb_addr_pa_k);

	fb_size = ALIGN_TO(CFG_DISPLAY_WIDTH, MTK_FB_ALIGNMENT) * CFG_DISPLAY_HEIGHT * CFG_DISPLAY_BPP / 8;
	// pa;
	boot_mode_addr = ((UINT32)fb_addr_pa + fb_size);
	logo_db_addr_pa = (void *)((UINT32) SCRATCH_ADDR + SCRATCH_SIZE);//for logo owner define .

	// va;
	logo_db_addr = (void *)((UINT32) SCRATCH_ADDR + SCRATCH_SIZE);//for logo owner define .

	fb_offset_logo = 0;

	primary_display_init(NULL);
	memset((void*)lcdbase, 0x0, DISP_GetVRamSize());

	disp_input_config input;
	input.security = 0;

#ifdef MTK_ROUND_CORNER_SUPPORT
		if (round_corner != NULL)
			ret = round_corner_init(&top_addr_pa, &bottom_addr_pa, round_corner);
			if (ret == 0) {
			memset(&input, 0, sizeof(disp_input_config));
			input.layer 	= TOP_LAYER;
			input.layer_en	= 1;
			input.fmt	= eRGBA8888;
			input.addr	= top_addr_pa;
			input.src_x 	= 0;
			input.src_y 	= 0;
			input.src_w 	= CFG_DISPLAY_WIDTH;
			input.src_h 	= round_corner->h;
			input.src_pitch = CFG_DISPLAY_WIDTH*CFG_DISPLAY_BPP / 8;
			input.dst_x 	= 0;
			input.dst_y 	= 0;
			input.dst_w 	= CFG_DISPLAY_WIDTH;
			input.dst_h 	= round_corner->h;
			input.aen	= 1;
			input.alpha 	= 0xff;

			primary_display_config_input(&input);

			memset(&input, 0, sizeof(disp_input_config));
			input.layer 	= BOTTOM_LAYER;
			input.layer_en	= 1;
			input.fmt	= eRGBA8888;
			input.addr	= bottom_addr_pa;
			input.src_x 	= 0;
			input.src_y 	= 0;
			input.src_w 	= CFG_DISPLAY_WIDTH;
			input.src_h 	= round_corner->h;
			input.src_pitch = CFG_DISPLAY_WIDTH*CFG_DISPLAY_BPP / 8;
			input.dst_x 	= 0;
			input.dst_y 	= CFG_DISPLAY_HEIGHT-round_corner->h;
			input.dst_w 	= CFG_DISPLAY_WIDTH;
			input.dst_h 	= round_corner->h;
			input.aen	= 1;
			input.alpha 	= 0xff;

			if (round_corner->full_content) {
				input.src_h     = round_corner->h_bot;
				input.dst_y     = CFG_DISPLAY_HEIGHT-round_corner->h_bot;
				input.dst_h     = round_corner->h_bot;
			}
			primary_display_config_input(&input);
		}
#endif
	memset(&input, 0, sizeof(disp_input_config));
	input.layer     = BOOT_MENU_LAYER;
	input.layer_en  = 1;
	input.fmt       = redoffset_32bit ? eBGRA8888 : eRGBA8888;
	input.addr      = boot_mode_addr;
	input.src_x     = 0;
	input.src_y     = 0;
	input.src_w     = CFG_DISPLAY_WIDTH;
	input.src_h     = CFG_DISPLAY_HEIGHT;
	input.src_pitch = CFG_DISPLAY_WIDTH*4;
	input.dst_x     = 0;
	input.dst_y     = 0;
	input.dst_w     = CFG_DISPLAY_WIDTH;
	input.dst_h     = CFG_DISPLAY_HEIGHT;
	input.aen       = 1;
	input.alpha     = 0xff;

	primary_display_config_input(&input);

	memset(&input, 0, sizeof(disp_input_config));
	input.layer     = FB_LAYER;
	input.layer_en  = 1;
	input.fmt       = redoffset_32bit ? eBGRA8888 : eRGBA8888;
	input.addr      = fb_addr_pa;
	input.src_x     = 0;
	input.src_y     = 0;
	input.src_w     = CFG_DISPLAY_WIDTH;
	input.src_h     = CFG_DISPLAY_HEIGHT;
	input.src_pitch = ALIGN_TO(CFG_DISPLAY_WIDTH, MTK_FB_ALIGNMENT)*4;
	input.dst_x     = 0;
	input.dst_y     = 0;
	input.dst_w     = CFG_DISPLAY_WIDTH;
	input.dst_h     = CFG_DISPLAY_HEIGHT;

	input.aen       = 1;
	input.alpha     = 0xff;
	primary_display_config_input(&input);


	//_mtkfb_internal_test(fb_addr, CFG_DISPLAY_WIDTH, CFG_DISPLAY_HEIGHT);

#ifdef DFO_DISP
	mt_disp_parse_dfo_setting();

	if ((0 == mt_disp_get_dfo_setting("LCM_FAKE_WIDTH", &lcm_fake_width)) && (0 == mt_disp_get_dfo_setting("LCM_FAKE_HEIGHT", &lcm_fake_height))) {
		if (DISP_STATUS_OK != DISP_Change_LCM_Resolution(lcm_fake_width, lcm_fake_height)) {
			dprintf(INFO,"[DISP_DFO]WARNING!!! Change LCM Resolution FAILED!!!\n");
		}
	}
#endif

}


void mt_disp_power(BOOL on)
{
	dprintf(0,"mt_disp_power %d\n",on);
	return;
}


void* mt_get_logo_db_addr(void)
{
	dprintf(0,"mt_get_logo_db_addr: 0x%8p\n",logo_db_addr);
	return logo_db_addr;
}

void* mt_get_logo_db_addr_pa(void)
{
	dprintf(0,"mt_get_logo_db_addr_pa: 0x%8p\n",logo_db_addr_pa);
	return logo_db_addr_pa;
}

void* mt_get_fb_addr(void)
{
	fb_isdirty = 1;
	return (void*)((UINT32)fb_addr + fb_offset_logo * fb_size);
}

void* mt_get_tempfb_addr(void)
{
	//use offset = 2 as tempfb for decompress logo
	dprintf(0,"mt_get_tempfb_addr: 0x%8p ,fb_addr 0x%8p\n",(void*)((UINT32)fb_addr + 2*fb_size),(void*)fb_addr);
	return (void*)((UINT32)fb_addr + 2*fb_size);
}

UINT32 mt_get_fb_size(void)
{
	return fb_size;
}
void mt_disp_update(UINT32 x, UINT32 y, UINT32 width, UINT32 height)
{
	unsigned int va = (unsigned int)fb_addr;
	dprintf(CRITICAL,"fb dump: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
	        *(unsigned int*)va, *(unsigned int*)(va+4), *(unsigned int*)(va+8), *(unsigned int*)(va+0xC));

	arch_clean_cache_range((unsigned int)fb_addr, DISP_GetFBRamSize());
	primary_display_trigger(TRUE);
	if (!primary_display_is_video_mode()) {
		/*video mode no need to wait*/
		dprintf(CRITICAL,"cmd mode trigger wait\n");
		mdelay(30);
	}
}

static void mt_disp_adjusting_hardware_addr(void)
{
	dprintf(CRITICAL,"mt_disp_adjusting_hardware_addr fb_offset_logo = %d fb_size=%d\n",fb_offset_logo,fb_size);
	if (fb_offset_logo == 0) {
		mt_get_fb_addr();
		memcpy(fb_addr,(void *)((UINT32)fb_addr + 3 * fb_size),fb_size);
		mt_disp_update(0, 0, CFG_DISPLAY_WIDTH, CFG_DISPLAY_HEIGHT);
	}
}

UINT32 mt_disp_get_lcd_time(void)
{
	unsigned int fps = 0;
	fps = primary_display_get_vsync_interval();
	dprintf(CRITICAL, "%s, fps=%d\n", __func__, fps);
	if (fps)
		return fps;
	else
		return 6000;
}

// Attention: this api indicates whether the lcm is connected
int DISP_IsLcmFound(void)
{
	return primary_display_is_lcm_connected();
}

const char* mt_disp_get_lcm_id(void)
{
	return primary_display_get_lcm_name();
}


void disp_get_fb_address(UINT32 *fbVirAddr, UINT32 *fbPhysAddr)
{
	*fbVirAddr = (UINT32)fb_addr;
	*fbPhysAddr = (UINT32)fb_addr;
}

UINT32 mt_disp_get_redoffset_32bit(void)
{
	return redoffset_32bit;
}


// ---------------------------------------------------------------------------
//  Export Functions - Console
// ---------------------------------------------------------------------------

#ifdef CONFIG_CFB_CONSOLE
//  video_hw_init -- called by drv_video_init() for framebuffer console

void *video_hw_init (void)
{
	static GraphicDevice s_mt65xx_gd;

	memset(&s_mt65xx_gd, 0, sizeof(GraphicDevice));

	s_mt65xx_gd.frameAdrs  = (UINT32)fb_addr+fb_size;
	s_mt65xx_gd.winSizeX   = CFG_DISPLAY_WIDTH;
	s_mt65xx_gd.winSizeY   = CFG_DISPLAY_HEIGHT;
	s_mt65xx_gd.gdfIndex   = CFB_X888RGB_32BIT;
	dprintf(0, "s_mt65xx_gd.gdfIndex=%d", s_mt65xx_gd.gdfIndex);
	s_mt65xx_gd.gdfBytesPP = CFG_DISPLAY_BPP / 8;
	s_mt65xx_gd.memSize    = s_mt65xx_gd.winSizeX * s_mt65xx_gd.winSizeY * s_mt65xx_gd.gdfBytesPP;

	return &s_mt65xx_gd;
}


void video_set_lut(unsigned int index,  /* color number */
                   unsigned char r,     /* red */
                   unsigned char g,     /* green */
                   unsigned char b)     /* blue */
{
	dprintf(CRITICAL, "%s\n", __func__);

}

#endif  // CONFIG_CFB_CONSOLE

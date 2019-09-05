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

#include <stdint.h>
#include <printf.h>
#include <malloc.h>
#include <string.h>
#include <platform/errno.h>
#include <platform/mmc_core.h>
#include <platform/partition.h>
#include <target.h>
#ifdef MTK_AB_OTA_UPDATER
#include "bootctrl.h"
#endif

#define TAG "[PART_LK]"

#define LEVEL_ERR   (0x0001)
#define LEVEL_INFO  (0x0004)

#define DEBUG_LEVEL (LEVEL_ERR | LEVEL_INFO)

#define part_err(fmt, args...)   \
do {    \
    if (DEBUG_LEVEL & LEVEL_ERR) {  \
        dprintf(CRITICAL, fmt, ##args); \
    }   \
} while (0)

#define part_info(fmt, args...)  \
do {    \
    if (DEBUG_LEVEL & LEVEL_INFO) {  \
        dprintf(CRITICAL, fmt, ##args);    \
    }   \
} while (0)


part_t tempart;
struct part_meta_info temmeta;

part_t *partition;
part_t *partition_all;

part_t* get_part(char *name)
{
	part_t *part = partition;
	part_t *ret = NULL;

	part_info("%s[%s] %s\n", TAG, __FUNCTION__, name);
	while (part->nr_sects) {
		if (part->info) {
			if (!strncmp(name, (char *)part->info->name, PART_META_INFO_NAMELEN)) {
				memcpy(&tempart, part, sizeof(part_t));
				memcpy(&temmeta, part->info, sizeof(struct part_meta_info));
				tempart.info = &temmeta;
				ret = &tempart;
				break;
			}
		}
		part++;

		//part_info("%s[%s] 0x%lx\n", TAG, __FUNCTION__, tempart.start_sect);
	}
	return ret;
}

void put_part(part_t *part)
{
	if (!part) {
		return;
	}

	if (part->info) {
		free(part->info);
	}
	free(part);
}

unsigned long mt_part_get_part_active_bit(part_t *part)
{
	return (part->part_attr & PART_ATTR_LEGACY_BIOS_BOOTABLE);
}

#if defined(MTK_EMMC_SUPPORT)
extern int mmc_get_boot_part(u32 *bootpart);
#endif
part_t *mt_part_get_partition(char *name)
{
	part_dev_t *dev = mt_part_get_device();
	u32 bootpart = dev->blkdev->part_boot1; //default bootpart = 1

	/* For fastboot backup preloader DL */
	if (!strncmp(name, "preloader2", PART_META_INFO_NAMELEN)) {
		tempart.start_sect = 0x0;
		tempart.nr_sects = 0x200;
		tempart.part_id = dev->blkdev->part_boot2;
		return &tempart;
	}
	if (!strncmp(name, "PRELOADER", PART_META_INFO_NAMELEN) || !strncmp(name, "preloader", PART_META_INFO_NAMELEN)) {
		tempart.start_sect = 0x0;
		tempart.nr_sects = 0x200;
		mmc_get_boot_part((u32 *)&bootpart); // get active boot partition
		if (bootpart == dev->blkdev->part_boot1) {
		    tempart.part_id = dev->blkdev->part_boot1;
		} else {
			tempart.part_id = dev->blkdev->part_boot2;
		}
		return &tempart;
	}
	return get_part(name);
}

#ifdef MTK_EMMC_SUPPORT
void mt_part_dump(void)
{
	part_t *part = partition;
	part_dev_t *dev = mt_part_get_device();
	if (!dev) {
		part_err("%s%s, err(no dev)\n", TAG, __func__);
		return;
	}
	part_info("Part Info.(1blk=%dB):\n", dev->blkdev->blksz);
	while (part->nr_sects) {
		part_info("[0x%016llx-0x%016llx] (%.8ld blocks): \"%s\"\n",
		          (u64)part->start_sect * dev->blkdev->blksz,
		          (u64)(part->start_sect + part->nr_sects) * dev->blkdev->blksz - 1,
		          part->nr_sects, (part->info) ? (char *)part->info->name : "unknown");
		part++;
	}
	part_info("\n");
}

extern int write_primary_gpt(void *data, unsigned sz);
extern int write_secondary_gpt(void *data, unsigned sz);
int gpt_partition_table_update(const char *arg, void *data, unsigned sz)
{
	int err;
	/* Currently always write pgpt and sgpt to avoid misjudge of efi signature after update gpt power-loss  */
	err = write_primary_gpt(data, sz);
	if (err) {
		part_err("[GPT_Update]write write_primary_gpt, err(%d)\n", err);
		return err;
	}
	err = write_secondary_gpt(data, sz);
	if (err) {
		part_err("[GPT_Update]write write_secondary_gpt, err(%d)\n", err);
		return err;
	}

	return 0;
}

#elif defined(MTK_NAND_SUPPORT)

void mt_part_dump(void)
{
	part_t *part = &partition_layout[0];
	part_dev_t *dev = mt_part_get_device();
	if (!dev) {
		part_err("%s%s, err(no dev)\n", TAG, __func__);
		return;
	}
	part_info("\nPart Info from compiler.(1blk=%dB):\n", dev->blkdev->blksz);
	part_info("\nPart Info.(1blk=%dB):\n", dev->blkdev->blksz);
	while (part->name) {
		part_info("[0x%.8x-0x%.8x] (%.8ld blocks): \"%s\"\n",
		          part->start_sect * dev->blkdev->blksz,
		          (part->start_sect + part->nr_sects) * dev->blkdev->blksz - 1,
		          part->nr_sects, part->name);
		part++;
	}
	part_info("\n");
}

#else

void mt_part_dump(void)
{
}


#endif

/**/
/*fastboot*/
/**/
unsigned int write_partition(unsigned size, unsigned char *partition)
{
	return 0;
}

int partition_get_index(const char * name)
{
	int i;
#ifdef MTK_AB_OTA_UPDATER
	char last_char[2];
	const char *part_suffix;
	char alt_name[PART_META_INFO_NAMELEN + BOOTCTRL_SUFFIX_MAXLEN];
	int check_alt_name = 0;
	/* FIXME: get_suffix will call partition_read to get para partition information, which will cause infinite function call */
	if (!strncmp(name, "para", PART_META_INFO_NAMELEN))
		part_suffix = NULL;
	else
		part_suffix = get_suffix();

	if ((int)strlen(name) - 2 >= 0) {
		last_char[0] = name[strlen(name) - 2];
		last_char[1] = name[strlen(name) - 1];
	} else {
		memset(last_char, 0, sizeof(last_char));
	}
	if (!(part_suffix != NULL && part_suffix[0] == last_char[0] && part_suffix[1] == last_char[1])) {
		snprintf(alt_name, PART_META_INFO_NAMELEN + BOOTCTRL_SUFFIX_MAXLEN, "%s%s", name, part_suffix);
		check_alt_name = 1;
	}
#endif
	for (i = 0; i < PART_MAX_COUNT; i++) {
		if (!partition_all[i].info)
			continue;
		if (!strncmp(name, (const char *)partition_all[i].info->name, PART_META_INFO_NAMELEN)) {
			dprintf(INFO, "[%s]find %s index %d\n", __FUNCTION__, name, i);
			return i;
		}
#ifdef MTK_AB_OTA_UPDATER
		if (check_alt_name == 1 && !strncmp(alt_name, (const char *)partition_all[i].info->name, PART_META_INFO_NAMELEN)) {
			dprintf(INFO, "[%s]find %s(add suffix for %s) index %d\n", __FUNCTION__, alt_name, name, i);
			return i;
		}
#endif
	}
	return -1;
}

unsigned int partition_get_region(int index)
{
	if (index < 0 || index >= PART_MAX_COUNT) {
		return -1;
	}

	if (!partition_all[index].nr_sects)
		return -1;

	return partition_all[index].part_id;
}

u64 partition_get_offset(int index)
{
	part_dev_t *dev = mt_part_get_device();
	if (!dev) {
		part_err("%s%s, err(no dev)\n", TAG, __func__);
		return -1;
	}

	if (index < 0 || index >= PART_MAX_COUNT) {
		return -1;
	}

	if (!partition_all[index].nr_sects)
		return -1;

	return (u64)partition_all[index].start_sect * dev->blkdev->blksz;
}

u64 partition_get_size(int index)
{
	part_dev_t *dev = mt_part_get_device();
	if (!dev) {
		part_err("%s%s, err(no dev)\n", TAG, __func__);
		return -1;
	}

	if (index < 0 || index >= PART_MAX_COUNT) {
		return -1;
	}

	if (!partition_all[index].nr_sects)
		return -1;

	return (u64)partition_all[index].nr_sects * dev->blkdev->blksz;
}

static int partition_map_search(const char *name)
{
	int i, ret;
	char last_char[2];
	char *new_name = (char *)strdup(name);
	int loops = sizeof(g_part_name_map) / sizeof(struct part_name_map);
	if ((int)strlen(name) - 2 >= 0) {
		last_char[0] = name[strlen(name) - 2];
		last_char[1] = name[strlen(name) - 1];
	} else {
		memset(last_char, 0, sizeof(last_char));
	}
	ret = -1;
	for (i = 0; i < loops; i++) {
		if (!g_part_name_map[i].fb_name[0])
			break;
		if (!strncmp(name, g_part_name_map[i].r_name, PART_META_INFO_NAMELEN)) {
			ret = i;
			break;
		}
		if ((last_char[1] == 'a' || last_char[1] == 'b') && last_char[0] == '_') {	// AB partition
			new_name[strlen(name) - 2] = 0;
		}
		if (last_char[1] <= '9' || last_char[1] >= '0') {	// xxx_1 or xxx1 partition
			if (last_char[0] == '_') { // ends with _[1-3]
				new_name[strlen(name) - 2] = 0;
			} else {	// ends with [1-3] without _ such as tee1 and tee2
				new_name[strlen(name) - 1] = 0;
			}
		}
		if (!strncmp(new_name, g_part_name_map[i].r_name, PART_META_INFO_NAMELEN)) {
			part_info("[%s] map partition %s(from %s) with %s\n", TAG, new_name, name, g_part_name_map[i].r_name);
			ret = i;
			break;
		}
	}
	free(new_name);
	return ret;
}

int partition_get_type(int index, char **p_type)
{
	int i;

	if (index < 0 || index >= PART_MAX_COUNT) {
		return -1;
	}

	if (!partition_all[index].nr_sects)
		return -1;

	if (!partition_all[index].info)
		return -1;

	i = partition_map_search((const char*)partition_all[index].info->name);
	if (i < 0)
		return -1;
	*p_type = g_part_name_map[i].partition_type;
	return 0;
}

int partition_get_name(int index, char **p_name)
{
	if (index < 0 || index >= PART_MAX_COUNT) {
		return -1;
	}

	if (!partition_all[index].nr_sects)
		return -1;

	if (!partition_all[index].info)
		return -1;

	*p_name = (char *) partition_all[index].info->name;
	return 0;
}

int is_support_erase(int index)
{
	int i;
	if (index < 0 || index >= PART_MAX_COUNT) {
		return -1;
	}

	if (!partition_all[index].nr_sects)
		return -1;

	if (!partition_all[index].info)
		return -1;

	i = partition_map_search((const char*)partition_all[index].info->name);
	if (i < 0)
		return -1;
	return g_part_name_map[i].is_support_erase;
}

int is_support_flash(int index)
{
	int i;
	if (index < 0 || index >= PART_MAX_COUNT) {
		return -1;
	}

	if (!partition_all[index].nr_sects)
		return -1;

	if (!partition_all[index].info)
		return -1;

	i = partition_map_search((const char*)partition_all[index].info->name);
	if (i < 0)
		return -1;
	return g_part_name_map[i].is_support_dl;
}

extern int read_gpt(part_t *part);
int part_init(part_dev_t *dev)
{
	part_t *part_ptr;

	dprintf(CRITICAL,"[partition init]\n");
	part_ptr = calloc(128 + 1, sizeof(part_t));
	if (!part_ptr)
		return -1;

	partition_all = part_ptr;
	partition = part_ptr + 1;

	partition->part_id = dev->blkdev->part_user;
	read_gpt(partition);

	partition_all[0].start_sect = 0x0;
	partition_all[0].nr_sects = 0x200;
	mmc_get_boot_part(&partition_all[0].part_id);
	partition_all[0].info = malloc(sizeof(struct part_meta_info));
	if (partition_all[0].info)
		snprintf((char*)partition_all[0].info->name, strlen("preloader")+1, "%s", "preloader");

	return 0;
}

#ifdef MTK_EMMC_SUPPORT
u64 emmc_write(u32 part_id, u64 offset, void *data, u64 size)
{
	part_dev_t *dev = mt_part_get_device();
	return (u64)dev->write(dev,data,offset,(int)size, part_id);
}

u64 emmc_read(u32 part_id, u64 offset, void *data, u64 size)
{
	part_dev_t *dev = mt_part_get_device();
	return (u64)dev->read(dev,offset,data,(int)size, part_id);
}
int emmc_erase(u32 part_id, u64 offset, u64 size)
{
	part_dev_t *dev = mt_part_get_device();
	if (!dev) {
		part_err("%s%s, err(no dev)\n", TAG, __func__);
		return -1;
	}
	return dev->erase(0,offset,size, part_id);
}
#endif

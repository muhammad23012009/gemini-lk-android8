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

#include <odm_mdtbo.h>                  // for get_odm_mdtbo_index()
#include <debug.h>                      // for dprintf()
#include <part_interface.h>             // for partition_read()
#include <block_generic_interface.h>    // for mt_part_get_device()
#include <stdlib.h>                     // for malloc()
#include <libfdt.h>


/******************************************************************************
* NOTE: Customers need to implement their own customized_get_odm_mdtbo_index()
*       function. If customized_get_odm_mdtbo_index() is not implemented, the
*       value 0 will be used as the default dtbo index. Each customer can
*       maintain their own implementation of customized_get_odm_mdtbo_index()
*       in a separate source file as long as it can be linked.
******************************************************************************/
int get_odm_mdtbo_index(void)
{
	int odm_dtbo_index = 0;

	if (customized_get_odm_mdtbo_index)
		odm_dtbo_index = customized_get_odm_mdtbo_index();

	return odm_dtbo_index;
}


/******************************************************************************
******************************************************************************/
static bool is_odm_mdtbo_header(const mdtbo_hdr_t *hdr2)
{
	if (hdr2->info.magic != ODM_MDTBO_MAGIC) {
		dprintf(INFO, "Single ODM DTBO.\n");
		return false;
	}
	dprintf(INFO, "Multiple ODM DTBO.\n");
	return true;
}


/******************************************************************************
* dtb_size:   It is the size of the device tree selected by dtbo_index.
* dtb_offset: It is the offset from the beginning of the partition.
******************************************************************************/
static void get_odm_mdtbo_offset_and_size(const mdtbo_hdr_t *hdr2,
	int mdtbo_index, unsigned int *dtbo_size, unsigned int *dtbo_offset)
{
	if ((unsigned int)mdtbo_index == hdr2->info.num_of_dtbo - 1)
		*dtbo_size = hdr2->info.dsize;
	else
		*dtbo_size = hdr2->info.dtbo_offset[mdtbo_index + 1];

	if (*dtbo_size <= hdr2->info.dtbo_offset[mdtbo_index]) {
		dprintf(CRITICAL, "Error: incorrect dtbo size\n");
		assert(0);
	}
	*dtbo_size -= hdr2->info.dtbo_offset[mdtbo_index];

	*dtbo_offset = hdr2->info.dtbo_offset[mdtbo_index];
	*dtbo_offset += PART_HDR_DATA_SIZE + ODM_MDTBO_HDR_SIZE;
}


/******************************************************************************
* Side effect: This function allocates a memory on success.
******************************************************************************/
static bool load_dtb_from_single_dtbo(char *part_name, part_hdr_t *part_hdr,
	char **handle)
{
	unsigned int dsize = part_hdr->info.dsize;
	*handle = malloc(dsize);
	if (*handle == NULL) {
		dprintf(CRITICAL, "Fail to malloc %d-byte buffer for dtbo!\n", dsize);
		assert(0);
		return false;
	}

	long len = partition_read(part_name, PART_HDR_DATA_SIZE, (uchar*)*handle, (size_t)dsize);
	if (len < 0) {
		dprintf(CRITICAL, "(L%d) Fail to read %s partition!\n", __LINE__, part_name);
		free(*handle);
		return false;
	}

	if ((*(unsigned int*)*handle) != PART_HDR_DTB_MGIC) {
		dprintf(CRITICAL, "Error: invalid dtbo image format!\n");
		free(*handle);
		// assert(0);  // Keep running instead of assert!!
		return false;
	}

	return true;
}


/******************************************************************************
* Side effect: This function allocates a memory on success.
******************************************************************************/
static bool load_dtb_from_multiple_dtbo(char *part_name,
	const mdtbo_hdr_t *hdr2, char **handle)
{
	if (hdr2->info.hdr_size != ODM_MDTBO_HDR_SIZE) {
		dprintf(CRITICAL, "Error: odm mdtbo header size %d mismatch!\n",
			hdr2->info.hdr_size);
		assert(0);
		return false;
	}

	if (hdr2->info.hdr_version != ODM_MDTBO_VERSION) {
		dprintf(CRITICAL, "Error: odm mdtbo header version %d mismatch!\n",
			hdr2->info.hdr_version);
		assert(0);
		return false;
	}

	int mdtbo_index = get_odm_mdtbo_index();
	if (mdtbo_index < 0 || mdtbo_index >= MAX_NUM_OF_ODM_MDTBO) {
		dprintf(CRITICAL, "Error: mdtbo index %d is out of range!\n", mdtbo_index);
		assert(0);
		return false;
	}

	unsigned int dtbo_size = 0, dtbo_offset = 0;
	if ((unsigned int)mdtbo_index >= hdr2->info.num_of_dtbo) {
		dprintf(CRITICAL, "Error: mdtbo_index %d >= num_of_dtbo %d.\n",
			mdtbo_index, hdr2->info.num_of_dtbo);
		dprintf(CRITICAL, "Set mdtbo_index to 0 for error handling!\n");
		mdtbo_index = 0;
	}
	get_odm_mdtbo_offset_and_size(hdr2, mdtbo_index, &dtbo_size, &dtbo_offset);
	dprintf(INFO, "ODM mdtbo_index: %d, dtbo_offset: %d, dtbo_size: %d\n",
		mdtbo_index, dtbo_offset, dtbo_size);

	*handle = malloc(dtbo_size);
	if (!*handle) {
		dprintf(CRITICAL, "Fail to malloc %d-byte buffer for dtbo!\n", dtbo_size);
		assert(0);
		return false;
	}

	long len = partition_read(part_name, dtbo_offset, (uchar*)*handle, (size_t)dtbo_size);
	if (len < 0) {
		dprintf(CRITICAL, "(L%d) Fail to read %s partition!\n", __LINE__, part_name);
		free(*handle);
		return false;
	}

	if ((*(unsigned int*)*handle) != PART_HDR_DTB_MGIC) {
		dprintf(CRITICAL, "Error: invalid dtbo image format!\n");
		free(*handle);
		// assert(0);  // Keep running instead of assert!!
		return false;
	}

	return true;
}


/******************************************************************************
******************************************************************************/
char *load_overlay_dtbo(char *part_name, unsigned int *size)
{
	if (sizeof(part_hdr_t) != PART_HDR_DATA_SIZE) {
		dprintf(CRITICAL, "Error: incorrect partition header size!\n");
		assert(0);
		return NULL;
	}

	if (size == NULL) {
		dprintf(CRITICAL, "(L%d) Error: size is NULL!\n", __LINE__);
		return NULL;
	}

	part_dev_t *dev = mt_part_get_device();
	if (!dev) {
		dprintf(CRITICAL, "(L%d) Fail to get device!\n", __LINE__);
		return NULL;
	}

	part_t *part = mt_part_get_partition(part_name);
	if (!part) {
		dprintf(CRITICAL, "(L%d) Fail to get partition %s!\n", __LINE__, part_name);
		return NULL;
	}

	/* Read the partition header & the trailing 32 bytes for possible prolog. */
	long read_size = (long)(PART_HDR_DATA_SIZE + ODM_MDTBO_HDR_SIZE);
	part_hdr_t *part_hdr = (part_hdr_t*)malloc(read_size);
	if (part_hdr == NULL) {
		dprintf(CRITICAL, "(L%d) Fail to allocate %d-byte memory!\n", __LINE__, (int)read_size);
		return NULL;
	}

	/* Read mkimage header. Treat it as the mdtbo header. */
	long len = partition_read(part_name, 0, (uchar*)part_hdr, read_size);
	if (len < 0) {
		dprintf(CRITICAL, "(L%d) Fail to read %s partition!\n", __LINE__, part_name);
		free(part_hdr);
		return NULL;
	}

	/* hdr2 points to the possible prolog. */
	const mdtbo_hdr_t* const hdr2 = (mdtbo_hdr_t*)(part_hdr + 1);
	char *ptr = NULL;
	bool success = false;
	if (is_odm_mdtbo_header(hdr2) == false)
		success = load_dtb_from_single_dtbo(part_name, part_hdr, &ptr);
	else
		success = load_dtb_from_multiple_dtbo(part_name, hdr2, &ptr);

	free(part_hdr);
	if (success) {
		*size = fdt32_to_cpu(*(unsigned int *)(ptr + 4));
		return ptr;
	}

	return NULL;
}



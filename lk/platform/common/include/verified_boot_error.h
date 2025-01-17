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

#ifndef __VERIFIED_BOOT_ERROR_H__
#define __VERIFIED_BOOT_ERROR_H__

#include <error.h>

#define MOD_ID ERR_MOD_SECURITY_ID

#define ERR_VB_LV1_GENERAL      (0x0000)
#define ERR_VB_LV1_AUTH         (0x0001)
#define ERR_VB_LV1_CRYPTO       (0x0002)
#define ERR_VB_LV1_SECCFG       (0x0003)
#define ERR_VB_LV1_ANTIROLLBACK (0x0004)
#define ERR_VB_LV1_REGION       (0x0005)
#define ERR_VB_LV1_TRNG         (0x0006)

#define ERR_VB_GENERAL_BASE      (MAKE_ERR_MOD_LV1_BASE(MOD_ID, ERR_VB_LV1_GENERAL))
#define ERR_VB_AUTH_BASE         (MAKE_ERR_MOD_LV1_BASE(MOD_ID, ERR_VB_LV1_AUTH))
#define ERR_VB_CRYPTO_BASE       (MAKE_ERR_MOD_LV1_BASE(MOD_ID, ERR_VB_LV1_CRYPTO))
#define ERR_VB_SECCFG_BASE       (MAKE_ERR_MOD_LV1_BASE(MOD_ID, ERR_VB_LV1_SECCFG))
#define ERR_VB_ANTIROLLBACK_BASE (MAKE_ERR_MOD_LV1_BASE(MOD_ID, ERR_VB_LV1_ANTIROLLBACK))
#define ERR_VB_REGION_BASE       (MAKE_ERR_MOD_LV1_BASE(MOD_ID, ERR_VB_LV1_REGION))
#define ERR_VB_TRNG_BASE         (MAKE_ERR_MOD_LV1_BASE(MOD_ID, ERR_VB_LV1_TRNG))

/* GENERAL */
#define ERR_VB_BUF_ADDR_INVALID           (ERR_VB_GENERAL_BASE + 0x000)
#define ERR_VB_BUF_OVERFLOW               (ERR_VB_GENERAL_BASE + 0x001)
#define ERR_VB_NOT_VALID_IMG_SZ           (ERR_VB_GENERAL_BASE + 0x002)
#define ERR_VB_NOT_VALID_IMG_ALIGN_SZ     (ERR_VB_GENERAL_BASE + 0x003)
#define ERR_VB_NOT_VALID_PAGE_SZ          (ERR_VB_GENERAL_BASE + 0x004)
#define ERR_VB_NOT_VALID_STRING           (ERR_VB_GENERAL_BASE + 0x005)

/* AUTH */
#define ERR_VB_SW_ID_MISMATCH             (ERR_VB_AUTH_BASE + 0x000)
#define ERR_VB_IMG_TYPE_INVALID           (ERR_VB_AUTH_BASE + 0x001)
#define ERR_VB_IMG_NOT_FOUND              (ERR_VB_AUTH_BASE + 0x002)
#define ERR_VB_IMG_HDR_HASH_VFY_FAIL      (ERR_VB_AUTH_BASE + 0x003)
#define ERR_VB_IMG_HASH_VFY_FAIL          (ERR_VB_AUTH_BASE + 0x004)
#define ERR_VB_OID_LEN_INVALID            (ERR_VB_AUTH_BASE + 0x005)
#define ERR_VB_OID_BUF_LEN_INVALID        (ERR_VB_AUTH_BASE + 0x006)
#define ERR_VB_SIG_LEN_INVALID            (ERR_VB_AUTH_BASE + 0x007)
#define ERR_VB_PADDING_LEN_INVALID        (ERR_VB_AUTH_BASE + 0x008)
#define ERR_VB_DA_VFY_FAIL                (ERR_VB_AUTH_BASE + 0x009)
#define ERR_VB_DA_KEY_INIT_FAIL           (ERR_VB_AUTH_BASE + 0x00a)
#define ERR_VB_DA_RELOCATE_LEN_NOT_ENOUGH (ERR_VB_AUTH_BASE + 0x00b)
#define ERR_VB_PUBK_NOT_INITIALIZED       (ERR_VB_AUTH_BASE + 0x00c)
#define ERR_VB_CERT_OBJ_ID_MISMATCH       (ERR_VB_AUTH_BASE + 0x00d)
#define ERR_VB_CERT_OBJ_LEN_MISMATCH      (ERR_VB_AUTH_BASE + 0x00e)
#define ERR_VB_CERT_TRAVERSE_MODE_UNKNOWN (ERR_VB_AUTH_BASE + 0x00f)
#define ERR_VB_CERT_OID_IDX_INVALID       (ERR_VB_AUTH_BASE + 0x010)
#define ERR_VB_CERT_OID_NOT_FOUND         (ERR_VB_AUTH_BASE + 0x011)
#define ERR_VB_CERT_OBJ_NOT_FOUND         (ERR_VB_AUTH_BASE + 0x012)
#define ERR_VB_CERT_E_KEY_LEN_MISMATCH    (ERR_VB_AUTH_BASE + 0x013)
#define ERR_VB_CERT_E_KEY_MISMATCH        (ERR_VB_AUTH_BASE + 0x014)
#define ERR_VB_CERT_N_KEY_LEN_MISMATCH    (ERR_VB_AUTH_BASE + 0x015)
#define ERR_VB_CERT_N_KEY_MISMATCH        (ERR_VB_AUTH_BASE + 0x016)
#define ERR_VB_PUBK_AUTH_FAIL             (ERR_VB_AUTH_BASE + 0x017)
#define ERR_VB_INVALID_IMG_HDR            (ERR_VB_AUTH_BASE + 0x018)
#define ERR_VB_NOT_EXPECTED_IMG_TYPE      (ERR_VB_AUTH_BASE + 0x019)

/* CRYPTO */
#define ERR_VB_CRYPTO_HACC_MODE_INVALID     (ERR_VB_CRYPTO_BASE + 0x000)
#define ERR_VB_CRYPTO_HACC_KEY_LEN_INVALID  (ERR_VB_CRYPTO_BASE + 0x001)
#define ERR_VB_CRYPTO_HACC_DATA_UNALIGNED   (ERR_VB_CRYPTO_BASE + 0x002)
#define ERR_VB_CRYPTO_HACC_SEED_LEN_INVALID (ERR_VB_CRYPTO_BASE + 0x003)
#define ERR_VB_CRYPTO_HASH_FAIL             (ERR_VB_CRYPTO_BASE + 0x004)
#define ERR_VB_CRYPTO_RSA_PSS_CHK_FAIL      (ERR_VB_CRYPTO_BASE + 0x005)
#define ERR_VB_CRYPTO_RSA_SIG_CHK_FAIL      (ERR_VB_CRYPTO_BASE + 0x006)

/* SECCFG */
#define ERR_VB_SECCFG_NOT_FOUND           (ERR_VB_SECCFG_BASE + 0x000)
#define ERR_VB_SECCFG_STATUS_INVALID      (ERR_VB_SECCFG_BASE + 0x001)
#define ERR_VB_SECCFG_ERASE_FAIL          (ERR_VB_SECCFG_BASE + 0x002)
#define ERR_VB_SECCFG_WRITE_FAIL          (ERR_VB_SECCFG_BASE + 0x003)
#define ERR_VB_SECCFG_BUF_OVERFLOW        (ERR_VB_SECCFG_BASE + 0x004)

/* ANTI ROLLBACK */
#define ERR_VB_CERT_IMG_VER_NOT_SYNC      (ERR_VB_ANTIROLLBACK_BASE + 0x000)
#define ERR_VB_IMG_VER_ROLLBACK           (ERR_VB_ANTIROLLBACK_BASE + 0x001)
#define ERR_VB_IMG_VER_OVERFLOW           (ERR_VB_ANTIROLLBACK_BASE + 0x002)
#define ERR_VB_UNKNOWN_NV_CNT_GROUP       (ERR_VB_ANTIROLLBACK_BASE + 0x003)
#define ERR_VB_GET_NV_CNT_FAIL            (ERR_VB_ANTIROLLBACK_BASE + 0x004)
#define ERR_VB_WRITE_NV_CNT_FAIL          (ERR_VB_ANTIROLLBACK_BASE + 0x005)
#define ERR_VB_SW_VER_MISMATCH            (ERR_VB_ANTIROLLBACK_BASE + 0x006)

/* REGION CHECK */
#define ERR_VB_REGION_INCLUDE             (ERR_VB_REGION_BASE + 0x000)
#define ERR_VB_REGION_OVERLAP             (ERR_VB_REGION_BASE + 0x001)
#define ERR_VB_REGION_OVERFLOW            (ERR_VB_REGION_BASE + 0x002)
#define ERR_VB_DA_ADDR_INVALID            (ERR_VB_REGION_BASE + 0x003)
#define ERR_VB_DA_LEN_INVALID             (ERR_VB_REGION_BASE + 0x004)
#define ERR_VB_INVALID_LOAD_ADDR          (ERR_VB_REGION_BASE + 0x005)

/* TRNG */
#define ERR_VB_TRNG_WRITE_DATA_FAIL       (ERR_VB_TRNG_BASE + 0x000)

#endif /* __VERIFIED_BOOT_ERROR_H__ */


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

#include <debug.h>
#include <string.h>
#include <stdlib.h>
#include <kernel/thread.h>
#include <kernel/timer.h>
#include <kernel/event.h>
#include <dev/udc.h>
#include <platform/mt_typedefs.h>
#include <platform/mtk_wdt.h>
#include <verified_boot_common.h>   // for sec_query_warranty()
#include "sys_commands.h"
#ifdef MTK_AB_OTA_UPDATER
#include <bootctrl.h>
#endif  // MTK_AB_OTA_UPDATER

extern int get_off_mode_charge_status(void);  // FIXME!!! #include <xxx.h>        // for get_off_mode_charge_status()


#include "dl_commands.h"
#include "fastboot.h"
#ifdef MTK_GPT_SCHEME_SUPPORT
#include <platform/partition.h>
#else
#include <mt_partition.h>
#endif
#if defined(MTK_SECURITY_SW_SUPPORT) && defined(MTK_SEC_FASTBOOT_UNLOCK_SUPPORT)
#include "sec_unlock.h"
#endif
#include <platform/boot_mode.h>
#ifdef FASTBOOT_WHOLE_FLASH_SUPPORT
#include "partition_parser.h"
#endif
#define MAX_RSP_SIZE 64
/* MAX_USBFS_BULK_SIZE: if use USB3 QMU GPD mode: cannot exceed 63 * 1024 */
#define MAX_USBFS_BULK_SIZE (16 * 1024)

#define EXPAND(NAME) #NAME
#define TARGET(NAME) EXPAND(NAME)

static event_t usb_online;
static event_t txn_done;
static unsigned char buffer[4096] __attribute__((aligned(64)));
static struct udc_endpoint *in, *out;
static struct udc_request *req;
int txn_status;

void *download_base;
unsigned download_max;
unsigned download_size;
extern int sec_usbdl_enabled (void);
extern void mtk_wdt_disable(void);
extern void mtk_wdt_restart(void);
extern unsigned get_secure_status(void);
extern unsigned get_unlocked_status(void);
unsigned fastboot_state = STATE_OFFLINE;

extern void fastboot_oem_key(const char *arg, void *data, unsigned sz);
extern void fastboot_oem_query_lock_state(const char *arg, void *data, unsigned sz);

timer_t wdt_timer;
struct fastboot_cmd *cmdlist;

extern BOOT_ARGUMENT *g_boot_arg;

static void req_complete(struct udc_request *req, unsigned actual, int status)
{
	txn_status = status;
	req->length = actual;
	event_signal(&txn_done, 0);
}


void fastboot_register(const char *prefix, void (*handle)(const char *arg, void *data, unsigned sz),
                       unsigned allowed_when_security_on, unsigned forbidden_when_lock_on)
{
	struct fastboot_cmd *cmd;

	cmd = malloc(sizeof(*cmd));
	if (cmd) {
		cmd->prefix = prefix;
		cmd->prefix_len = strlen(prefix);
		cmd->allowed_when_security_on = allowed_when_security_on;
		cmd->forbidden_when_lock_on = forbidden_when_lock_on;
		cmd->handle = handle;
		cmd->next = cmdlist;
		cmdlist = cmd;
	}
}

struct fastboot_var *varlist;

void fastboot_publish(const char *name, const char *value)
{
	struct fastboot_var *var;
	var = malloc(sizeof(*var));
	if (var) {
		var->name = name;
		var->value = value;
		var->next = varlist;
		varlist = var;
	}
}

void fastboot_update_var(const char *name, const char *value)
{
	struct fastboot_var *var = varlist;

	while (NULL != var) {
		if (!strcmp(name, var->name)) {
			var->value = value;
		}
		var = var->next;
	}

	return;
}

int usb_read(void *_buf, unsigned len)
{
	int r;
	unsigned xfer;
	unsigned char *buf = _buf;
	int count = 0;

	if (fastboot_state == STATE_ERROR)
		goto oops;

	while (len > 0) {
		xfer = (len > MAX_USBFS_BULK_SIZE) ? MAX_USBFS_BULK_SIZE : len;
		req->buf = buf;
		req->length = xfer;
		req->complete = req_complete;
		r = udc_request_queue(out, req);
		if (r < 0) {
			dprintf(INFO, "usb_read() queue failed\n");
			goto oops;
		}
		event_wait(&txn_done);

		if (txn_status < 0) {
			dprintf(INFO, "usb_read() transaction failed\n");
			goto oops;
		}

		count += req->length;
		buf += req->length;
		len -= req->length;

		/* short transfer? */
		if (req->length != xfer) break;
	}

	return count;

oops:
	fastboot_state = STATE_ERROR;
	return -1;
}

int usb_write(void *buf, unsigned len)
{
	int r;

	if (fastboot_state == STATE_ERROR)
		goto oops;

	req->buf = buf;
	req->length = len;
	req->complete = req_complete;
	r = udc_request_queue(in, req);
	if (r < 0) {
		dprintf(INFO, "usb_write() queue failed\n");
		goto oops;
	}
	event_wait(&txn_done);
	if (txn_status < 0) {
		dprintf(INFO, "usb_write() transaction failed\n");
		goto oops;
	}
	return req->length;

oops:
	fastboot_state = STATE_ERROR;
	return -1;
}

void fastboot_ack(const char *code, const char *reason)
{
	char response[MAX_RSP_SIZE];

	if (fastboot_state != STATE_COMMAND)
		return;

	if (reason == 0)
		reason = "";

	snprintf(response, MAX_RSP_SIZE, "%s%s", code, reason);
	fastboot_state = STATE_COMPLETE;

	usb_write(response, strlen(response));

}

void fastboot_info(const char *reason)
{
	char response[MAX_RSP_SIZE];

	if (fastboot_state != STATE_COMMAND)
		return;

	if (reason == 0)
		return;

	snprintf(response, MAX_RSP_SIZE, "INFO%s", reason);

	usb_write(response, strlen(response));
}

void fastboot_fail(const char *reason)
{
	fastboot_ack("FAIL", reason);
}

void fastboot_okay(const char *info)
{
	fastboot_ack("OKAY", info);
}

static void fastboot_command_loop(void)
{
	struct fastboot_cmd *cmd;
	int r;
	dprintf(ALWAYS,"fastboot: processing commands\n");

again:
	while (fastboot_state != STATE_ERROR) {
		memset(buffer, 0, sizeof(buffer));
		r = usb_read(buffer, MAX_RSP_SIZE);
		if (r < 0) break; //no input command
		buffer[r] = 0;
		dprintf(ALWAYS,"[fastboot: command buf]-[%s]-[len=%d]\n", buffer, r);
		dprintf(ALWAYS,"[fastboot]-[download_base:0x%x]-[download_size:0x%x]\n",(unsigned int)download_base,(unsigned int)download_size);

		/*Pick up matched command and handle it*/
		for (cmd = cmdlist; cmd; cmd = cmd->next) {
			fastboot_state = STATE_COMMAND;
			if (memcmp(buffer, cmd->prefix, cmd->prefix_len)) {
				continue;
			}

			dprintf(ALWAYS,"[Cmd process]-[buf:%s]-[lenBuf:%s]\n", buffer,  buffer + cmd->prefix_len);
#ifdef MTK_SECURITY_SW_SUPPORT
			extern unsigned int seclib_sec_boot_enabled(unsigned int);
			//if security boot enable, check cmd allowed
			if ( !(sec_usbdl_enabled() || seclib_sec_boot_enabled(1)) || cmd->allowed_when_security_on )
				if ((!cmd->forbidden_when_lock_on) || (0 != get_unlocked_status()))
#endif
				{
					cmd->handle((const char*) buffer + cmd->prefix_len, (void*) download_base, download_size);
				}
			if (fastboot_state == STATE_COMMAND) {
#ifdef MTK_SECURITY_SW_SUPPORT
				if ((sec_usbdl_enabled() || seclib_sec_boot_enabled(1)) && !cmd->allowed_when_security_on ) {
					fastboot_fail("not support on security");
				} else if ((cmd->forbidden_when_lock_on) && (0 == get_unlocked_status())) {
					fastboot_fail("not allowed in locked state");
				} else
#endif
				{
					fastboot_fail("unknown reason");
				}
			}
			goto again;
		}
		dprintf(ALWAYS,"[unknown command]*[%s]*\n", buffer);
		fastboot_fail("unknown command");

	}
	fastboot_state = STATE_OFFLINE;
	dprintf(ALWAYS,"fastboot: oops!\n");
}

static int fastboot_handler(void *arg)
{
	for (;;) {
		event_wait(&usb_online);
		fastboot_command_loop();
	}
	return 0;
}

static void fastboot_notify(struct udc_gadget *gadget, unsigned event)
{
	if (event == UDC_EVENT_ONLINE) {
		event_signal(&usb_online, 0);
	} else if (event == UDC_EVENT_OFFLINE) {
		event_unsignal(&usb_online);
	}
}

static struct udc_endpoint *fastboot_endpoints[2];

static struct udc_gadget fastboot_gadget = {
	.notify     = fastboot_notify,
	.ifc_class  = 0xff,
	.ifc_subclass   = 0x42,
	.ifc_protocol   = 0x03,
	.ifc_endpoints  = 2,
	.ifc_string = "fastboot",
	.ept        = fastboot_endpoints,
};

extern void fastboot_oem_register();
void register_partition_var(void)
{
	int i;
	unsigned long long p_size;
	char *type_buf;
	char *value_buf;
	char *var_name_buf;
	char *p_name_buf;

	for (i=0; i<PART_MAX_COUNT; i++) {
		p_size = partition_get_size(i);
		if ((long long)p_size == -1)
			continue;
		partition_get_name(i,&p_name_buf);

		partition_get_type(i,&type_buf);
		var_name_buf = malloc(30);
		sprintf(var_name_buf,"partition-type:%s",p_name_buf);
		fastboot_publish(var_name_buf,type_buf);
		//printf("%d %s %s\n",i,var_name_buf,type_buf);

		/*reserved for MTK security*/
		if (!strcmp(type_buf,"ext4")) {
			if (!strcmp(p_name_buf,"userdata")) {
				p_size -= (u64)1*1024*1024;
				if (p_size > 800*1024*1024) {
					p_size = 800*1024*1024;
				}
			}
		}
		value_buf = malloc(20);
		sprintf(value_buf,"%llx",p_size);
		var_name_buf = malloc(30);
		sprintf(var_name_buf,"partition-size:%s",p_name_buf);
		fastboot_publish(var_name_buf,value_buf);
		//printf("%d %s %s\n",i,var_name_buf,value_buf);
	}
}

/* return value: 1: option is valid, 0: option is invalid */
static unsigned int is_option_valid(unsigned int lower, unsigned int upper, unsigned int option)
{
	if (option < lower)
		return 0;

	if (option > upper)
		return 0;

	return 1;
}

#define NUM_SEC_OPTION (2)
static void register_secure_unlocked_var(void)
{
	static const char str_buf[NUM_SEC_OPTION][4] = {"no","yes"};
	unsigned secure_status = 0; /* default value is "no" */
	unsigned unlocked_status = 1; /* default value is "yes" */
	int warranty = 0; /* default value is "no" */

#ifdef MTK_SECURITY_SW_SUPPORT
	secure_status = get_secure_status();
	unlocked_status = get_unlocked_status();
	sec_query_warranty(&warranty);
#endif

	if (is_option_valid(0, NUM_SEC_OPTION - 1, secure_status))
		fastboot_publish("secure", str_buf[secure_status]);

	if (is_option_valid(0, NUM_SEC_OPTION - 1, unlocked_status))
		fastboot_publish("unlocked", str_buf[unlocked_status]);

	if (is_option_valid(0, NUM_SEC_OPTION - 1, (unsigned int)warranty))
		fastboot_publish("warranty", str_buf[warranty]);

	return;
}

#ifdef MTK_OFF_MODE_CHARGE_SUPPORT
static void register_off_mode_charge_var(void)
{
	//INIT VALUE
	static const char str_buf[2][2] = {"0", "1"};
	unsigned off_mode_status = 0; /* default value is "0" */

	off_mode_status = get_off_mode_charge_status();
	if ((off_mode_status == 0) || (off_mode_status == 1))
		fastboot_publish("off-mode-charge", str_buf[off_mode_status]);
	else
		dprintf(INFO, "off mode charge status is out of boundary\n");
}
#endif


#ifdef MTK_AB_OTA_UPDATER
/******************************************************************************
******************************************************************************/
static char *part_name_array[PART_MAX_COUNT] = {NULL};
static int part_name_index = 0;


/******************************************************************************
******************************************************************************/
static bool is_part_name_processed(char *name)
{
	for (int i = 0; i < PART_MAX_COUNT; i++) {
		if (!part_name_array[i]) {
			return false;
		}
		int len = strlen(name);
		if (!strncmp(part_name_array[i], name, len + 1))
			return true;
		else
			continue;
	}
	return false;
}


/******************************************************************************
******************************************************************************/
static void insert_part_name(char *name)
{
	if (part_name_index >= PART_MAX_COUNT)
		return false;

	part_name_array[part_name_index] = name;
	part_name_index++;

	return true;
}


/******************************************************************************
******************************************************************************/
static void release_part_name(void)
{
	for (int i = 0; i < part_name_index; i++) {
		free(part_name_array[i]);
		part_name_array[i] = NULL;
	}
}


/******************************************************************************
* This function publishes the following variables: "has-slot", "current-slot",
* "slot-count", "slot-successful", "slot-unbootable", and "slot-retry-count".
*
* Note: The "preloader" partition is the only exception that does not have
*       "_a" or "_b" suffix even when it supports A/B systems.
******************************************************************************/
static void publish_ab_variables(void)
{
	char *str_yes = "yes";
	char *str_no = "no";
	char *str_yes_or_no = NULL;
	char *str_slot_count = "2";
	char *str_has_slot = "has-slot:";
	const int has_slot_len = strlen(str_has_slot);
	char *str_successful_a = "slot-successful:_a";
	char *str_successful_b = "slot-successful:_b";
	char *str_unbootable_a = "slot-unbootable:_a";
	char *str_unbootable_b = "slot-unbootable:_b";
	char *str_retry_a = "slot-retry-count:_a";
	char *str_retry_b = "slot-retry-count:_b";
	static char str_retry_count_a[4] = {'\0'};
	static char str_retry_count_b[4] = {'\0'};
	char *str_preloader = "has-slot:preloader";

	for (int i = 0; i < PART_MAX_COUNT; i++) {
		char *part_name;
		int ret = partition_get_name(i, &part_name);
		if (ret < 0)
			break;  // early termination
		int len = has_slot_len + strlen(part_name) + 1;  // include NULL
		char *new_name = malloc(len);
		if (!new_name) {
			release_part_name();
			dprintf(CRITICAL, "%s (L%d) Error: failed to allocate memory!\n",
				__func__, __LINE__);
			return;
		}
		snprintf(new_name, len, "%s%s", str_has_slot, part_name);
		if (new_name[len - 3] == '_') {
			if (new_name[len - 2] == 'a' || new_name[len - 2] == 'b') {
				str_yes_or_no = str_yes;
				new_name[len - 3] = '\0';  // truncate suffix
			}
		} else {
			/* Preloader is the only exception that does not have A/B suffix. */
			if (!strncmp(new_name, str_preloader, strlen(str_preloader))) {
				if (get_suffix() != NULL)
					str_yes_or_no = str_yes;
			} else
				str_yes_or_no = str_no;
		}
		if (!is_part_name_processed(new_name)) {
			insert_part_name(new_name);
			fastboot_publish(new_name, str_yes_or_no);
		}
	}

	/* current-slot */
	fastboot_publish("current-slot", get_suffix());

	/* slot-count */
	fastboot_publish("slot-count", str_slot_count);

	int result;
	/* slot-successful */
	/* get_bootup_status() returns 1 for successful boot. Otherwise, return 0. */
	result = get_bootup_status("_a");
	if (result)
		str_yes_or_no = str_yes;
	else
		str_yes_or_no = str_no;
	fastboot_publish(str_successful_a, str_yes_or_no);

	result = get_bootup_status("_b");
	if (result)
		str_yes_or_no = str_yes;
	else
		str_yes_or_no = str_no;
	fastboot_publish(str_successful_b, str_yes_or_no);

	/* slot-unbootable */
	/* get_bootable_status() returns -1 for unbootable. Otherwise, return 0. */
	result = get_bootable_status("_a");
	if (result <= 0)
		str_yes_or_no = str_yes;
	else
		str_yes_or_no = str_no;
	fastboot_publish(str_unbootable_a, str_yes_or_no);

	result = get_bootable_status("_b");
	if (result <= 0)
		str_yes_or_no = str_yes;
	else
		str_yes_or_no = str_no;
	fastboot_publish(str_unbootable_b, str_yes_or_no);

	/* slot-retry-count */
	uint8_t retry_count = get_retry_count("_a");
	snprintf(str_retry_count_a, sizeof(str_retry_count_a), "%d", (int)retry_count);
	fastboot_publish(str_retry_a, str_retry_count_a);

	retry_count = get_retry_count("_b");
	snprintf(str_retry_count_b, sizeof(str_retry_count_b), "%d", (int)retry_count);
	fastboot_publish(str_retry_b, str_retry_count_b);

	// release_part_name();  // Do NOT release string here!
}

#else

static void publish_ab_variables(void)
{
	/* Empty! */
}

#endif  // MTK_AB_OTA_UPDATER


/******************************************************************************
******************************************************************************/
int fastboot_init(void *base, unsigned size)
{
	thread_t *thr;
	int ret;
	static char dl_max_str[11];  // Take NULL and leading 0x into account. SCRATCH_SIZE must be <= 32-bits

	dprintf(ALWAYS, "fastboot_init()\n");

	download_base = base;
	download_max = SCRATCH_SIZE;
	snprintf(dl_max_str, sizeof(dl_max_str), "0x%X", download_max);

	//mtk_wdt_disable(); /*It will re-enable during continue boot*/
	timer_initialize(&wdt_timer);
	timer_set_periodic(&wdt_timer, 5000, (timer_callback)mtk_wdt_restart, NULL);

	fastboot_register("getvar:", cmd_getvar, TRUE, FALSE);
	fastboot_publish("version", "0.5");
	fastboot_publish("version-preloader", g_boot_arg->pl_version);
	publish_ab_variables();
	fastboot_register("signature", cmd_install_sig, FALSE, TRUE);

#if (defined(MTK_EMMC_SUPPORT) || defined(MTK_UFS_BOOTING)) && defined(MTK_SPI_NOR_SUPPORT)
	dprintf(ALWAYS,"Init EMMC device in fastboot mode\n");
	mmc_legacy_init(1);
#endif
	fastboot_register("flash:", cmd_flash_mmc, TRUE, TRUE);
	fastboot_register("erase:", cmd_erase_mmc, TRUE, TRUE);

	fastboot_register("continue", cmd_continue, FALSE, FALSE);
	fastboot_register("reboot", cmd_reboot, TRUE, FALSE);
	fastboot_register("reboot-bootloader", cmd_reboot_bootloader, TRUE, FALSE);
	fastboot_publish("product", TARGET(BOARD));
	fastboot_publish("kernel", "lk");
	register_secure_unlocked_var();
#ifdef MTK_OFF_MODE_CHARGE_SUPPORT
	register_off_mode_charge_var();
#endif
	//fastboot_publish("serialno", sn_buf);

	register_partition_var();


	/*LXO: Download related command*/
	fastboot_register("download:", cmd_download, TRUE, FALSE);
	fastboot_publish("max-download-size", dl_max_str);
	/*LXO: END!Download related command*/

	fastboot_oem_register();
#if defined(MTK_SECURITY_SW_SUPPORT)
	fastboot_register("oem p2u", cmd_oem_p2u, TRUE, FALSE);
#endif
	fastboot_register("oem reboot-recovery",cmd_oem_reboot2recovery, TRUE, FALSE);
#ifdef MTK_OFF_MODE_CHARGE_SUPPORT
	fastboot_register("oem off-mode-charge",cmd_oem_off_mode_charge,FALSE, FALSE);
#endif
#if defined(MTK_SECURITY_SW_SUPPORT) && defined(MTK_SEC_FASTBOOT_UNLOCK_SUPPORT)
	fastboot_register("oem unlock",fastboot_oem_unlock, TRUE, FALSE);
	fastboot_register("oem lock",fastboot_oem_lock, TRUE, FALSE);
	fastboot_register("oem key",fastboot_oem_key,TRUE, FALSE);
	fastboot_register("oem lks",fastboot_oem_query_lock_state,TRUE, FALSE);
	/* allowed when secure boot and unlocked */
	fastboot_register("boot", cmd_boot, TRUE, TRUE);
	/* command rename */
	fastboot_register("flashing unlock",fastboot_oem_unlock, TRUE, FALSE);
	fastboot_register("flashing lock",fastboot_oem_lock, TRUE, FALSE);
	fastboot_register("flashing get_unlock_ability", fastboot_get_unlock_ability, TRUE, FALSE);
#endif
#ifdef MTK_JTAG_SWITCH_SUPPORT
	/* pin mux switch to ap_jtag */
	fastboot_register("oem ap_jtag",cmd_oem_ap_jtag, TRUE, FALSE);
#endif
#ifdef MTK_TINYSYS_SCP_SUPPORT
	fastboot_register("oem scp_status",cmd_oem_scp_status, FALSE, FALSE);
#endif
#ifdef MTK_USB2JTAG_SUPPORT
	fastboot_register("oem usb2jtag", cmd_oem_usb2jtag, TRUE, FALSE);
#endif
#ifdef MTK_MRDUMP_USB_DUMP
	fastboot_register("oem mtkreboot",cmd_oem_mtkreboot, FALSE, FALSE);
	fastboot_register("oem mrdump",cmd_oem_mrdump, FALSE, FALSE);
#endif
#ifdef MTK_AB_OTA_UPDATER
	fastboot_register("set_active", cmd_set_active, TRUE, FALSE);
#endif

	event_init(&usb_online, 0, EVENT_FLAG_AUTOUNSIGNAL);
	event_init(&txn_done, 0, EVENT_FLAG_AUTOUNSIGNAL);

	in = udc_endpoint_alloc(UDC_TYPE_BULK_IN, 512);
	if (!in) {
		ret = -1;
		goto fail_alloc_in;
	}

	out = udc_endpoint_alloc(UDC_TYPE_BULK_OUT, 512);
	if (!out) {
		ret = -2;
		goto fail_alloc_out;
	}

	fastboot_endpoints[0] = in;
	fastboot_endpoints[1] = out;

	req = udc_request_alloc();
	if (!req) {
		ret = -3;
		goto fail_alloc_req;
	}

	if (udc_register_gadget(&fastboot_gadget)) {
		ret = -4;
		goto fail_udc_register;
	}

	thr = thread_create("fastboot", fastboot_handler, 0, DEFAULT_PRIORITY, DEFAULT_STACK_SIZE);
	if (!thr) {
		ret = -1;
		goto fail_alloc_in;
	}
	thread_resume(thr);
	return 0;

fail_udc_register:
	udc_request_free(req);
fail_alloc_req:
	udc_endpoint_free(out);
fail_alloc_out:
	udc_endpoint_free(in);
fail_alloc_in:
	dprintf(CRITICAL, "%s fail: %d\n", __func__, ret);
	ASSERT(0);
	return ret;
}


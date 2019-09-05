LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += -I$(LOCAL_DIR)/../include
INCLUDES += -I$(LOCAL_DIR)/../../$(PLATFORM)/include
INCLUDES += -I$(LOCAL_DIR)/../../$(PLATFORM)/include/platform

ifeq ($(strip $(MTK_EMMC_SUPPORT)),yes)
OBJS += $(LOCAL_DIR)/storage_emmc_intf.o
else ifeq ($(strip $(MTK_UFS_BOOTING)),yes)
OBJS += $(LOCAL_DIR)/storage_ufs_intf.o
else
OBJS += $(LOCAL_DIR)/storage_nand_intf.o
endif

LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += -I$(LOCAL_DIR)/include
INCLUDES += -Iplatform/$(PLATFORM)/include/platform

##############################################################################
# Check build type. (End)
##############################################################################
MODULES += $(LOCAL_DIR)/blkdev
MODULES += $(LOCAL_DIR)/intf

ifeq ($(strip $(MTK_UFS_BOOTING)),yes)
MODULES += $(LOCAL_DIR)/ufs
endif


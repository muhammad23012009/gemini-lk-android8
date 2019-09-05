LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += -I$(LOCAL_DIR)/include
INCLUDES += -Iplatform/$(PLATFORM)/include/platform

##############################################################################
# Check build type. (End)
##############################################################################

ifeq ($(MTK_AB_OTA_UPDATER),yes)
DEFINES += MTK_AB_OTA_UPDATER
OBJS += $(LOCAL_DIR)/bootctrl_api.o
endif


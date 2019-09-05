LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += -I$(LOCAL_DIR)/include
INCLUDES += -Iplatform/$(PLATFORM)/include/platform

##############################################################################
# Check build type. (Begin)
##############################################################################
# For backward compatibility, USER_BUILD is also defined on userdebug build.
##############################################################################
ifneq ($(filter user, $(TARGET_BUILD_VARIANT)),)
DEFINES += USER_BUILD
else ifneq ($(filter userdebug, $(TARGET_BUILD_VARIANT)),)
DEFINES += USERDEBUG_BUILD USER_BUILD
else ifneq ($(filter eng, $(TARGET_BUILD_VARIANT)),)
DEFINES += ENG_BUILD
else
DEFINES += USER_BUILD   # If none of the keywords is found, define USER_BUILD.
endif
##############################################################################
# Check build type. (End)
##############################################################################

ifeq ($(PLATFORM_FASTBOOT_EMPTY_STORAGE), yes)
DEFINES += PLATFORM_FASTBOOT_EMPTY_STORAGE
endif

ifeq ($(MTK_RC_TO_VENDOR), yes)
DEFINES += MTK_RC_TO_VENDOR
endif

# RoT for ARMv8
ifneq ($(strip $(MTK_SECURITY_SW_SUPPORT)),no)
ifeq (,$(filter mt6570 mt6580,$(PLATFORM)))
OBJS += $(LOCAL_DIR)/RoT/RoT.o
endif
endif

OBJS += $(LOCAL_DIR)/sec_wrapper.o
OBJS += $(LOCAL_DIR)/profiling.o
OBJS += $(LOCAL_DIR)/meta.o
OBJS += $(LOCAL_DIR)/recovery.o
OBJS += $(LOCAL_DIR)/fpga_boot_argument.o
OBJS += $(LOCAL_DIR)/error.o

MODULES += $(LOCAL_DIR)/loader
MODULES += $(LOCAL_DIR)/storage
MODULES += $(LOCAL_DIR)/partition
MODULES += $(LOCAL_DIR)/plinfo
ifeq ($(MTK_AB_OTA_UPDATER),yes)
MODULES += $(LOCAL_DIR)/bootctrl
endif

# aee platform debug
ifeq ($(MTK_AEE_PLATFORM_DEBUG_SUPPORT),yes)
MODULES += $(LOCAL_DIR)/aee_platform_debug
MODULES += $(LOCAL_DIR)/spm
endif


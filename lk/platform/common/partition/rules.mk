LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += -I$(LOCAL_DIR)/include
INCLUDES += -Iplatform/$(PLATFORM)/include/platform

# partition interface
OBJS += $(LOCAL_DIR)/part_internal_wrapper.o \
	$(LOCAL_DIR)/part_common.o

ifeq ($(MTK_EMMC_SUPPORT),yes)
OBJS += $(LOCAL_DIR)/part_emmc.o
else ifeq ($(MTK_UFS_BOOTING),yes)
OBJS += $(LOCAL_DIR)/part_ufs.o
else
OBJS += $(LOCAL_DIR)/part_nand.o
endif


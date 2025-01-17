LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += -I$(LOCAL_DIR)
INCLUDES += -I$(LOCAL_DIR)/../include
INCLUDES += -I$(LOCAL_DIR)/../../$(PLATFORM)/include

OBJS += \
	$(LOCAL_DIR)/mtk_battery.o  \
	$(LOCAL_DIR)/mtk_charger.o  \
	$(LOCAL_DIR)/mtk_charger_intf.o

ifeq ($(MTK_CHARGER_NEW_ARCH),yes)
	DEFINES += MTK_CHARGER_NEW_ARCH
endif

ifeq ($(MTK_MT6370_PMU_CHARGER_SUPPORT),yes)
	OBJS += $(LOCAL_DIR)/mt6370_pmu_charger.o
	DEFINES += MTK_MT6370_PMU_CHARGER_SUPPORT
	DEFINES += SWCHR_POWER_PATH
endif

ifneq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

commands_recovery_local_path := $(LOCAL_PATH)

TARGET_RECOVERY_GUI := true

LOCAL_MODULE := recovery

LOCAL_C_INCLUDES += bionic external/stlport/stlport

LOCAL_SRC_FILES := \
    recovery.c \
    bootloader.c \
    install.c \
    roots.c \
    ui.c \
    verifier.c \
    encryptedfs_provisioning.c \
    extra-functions.c \
    ddftw.c \
    backstore.c \
    themes.c \
    format.c \
    data.cpp

ifeq ($(TARGET_RECOVERY_REBOOT_SRC),)
  LOCAL_SRC_FILES += reboot.c
else
  LOCAL_SRC_FILES += $(TARGET_RECOVERY_REBOOT_SRC)
endif

RECOVERY_API_VERSION := 2
LOCAL_CFLAGS += -DRECOVERY_API_VERSION=$(RECOVERY_API_VERSION)

ifeq ($(BOARD_HAS_NO_REAL_SDCARD), true)
    LOCAL_CFLAGS += -DBOARD_HAS_NO_REAL_SDCARD
endif

ifneq ($(SP1_NAME),)
	LOCAL_CFLAGS += -DSP1_NAME=$(SP1_NAME) -DSP1_BACKUP_METHOD=$(SP1_BACKUP_METHOD) -DSP1_MOUNTABLE=$(SP1_MOUNTABLE)
endif
ifneq ($(SP1_DISPLAY_NAME),)
	LOCAL_CFLAGS += -DSP1_DISPLAY_NAME=$(SP1_DISPLAY_NAME)
endif
ifneq ($(SP2_NAME),)
	LOCAL_CFLAGS += -DSP2_NAME=$(SP2_NAME) -DSP2_BACKUP_METHOD=$(SP2_BACKUP_METHOD) -DSP2_MOUNTABLE=$(SP2_MOUNTABLE)
endif
ifneq ($(SP2_DISPLAY_NAME),)
	LOCAL_CFLAGS += -DSP2_DISPLAY_NAME=$(SP2_DISPLAY_NAME)
endif
ifneq ($(SP3_NAME),)
	LOCAL_CFLAGS += -DSP3_NAME=$(SP3_NAME) -DSP3_BACKUP_METHOD=$(SP3_BACKUP_METHOD) -DSP3_MOUNTABLE=$(SP3_MOUNTABLE)
endif
ifneq ($(SP3_DISPLAY_NAME),)
	LOCAL_CFLAGS += -DSP3_DISPLAY_NAME=$(SP3_DISPLAY_NAME)
endif
ifneq ($(RECOVERY_SDCARD_ON_DATA),)
	LOCAL_CFLAGS += -DRECOVERY_SDCARD_ON_DATA
endif
ifneq ($(TW_INCLUDE_DUMLOCK),)
	LOCAL_CFLAGS += -DTW_INCLUDE_DUMLOCK
endif
ifneq ($(TW_INTERNAL_STORAGE_PATH),)
	LOCAL_CFLAGS += -DTW_INTERNAL_STORAGE_PATH=$(TW_INTERNAL_STORAGE_PATH)
endif
ifneq ($(TW_INTERNAL_STORAGE_MOUNT_POINT),)
	LOCAL_CFLAGS += -DTW_INTERNAL_STORAGE_MOUNT_POINT=$(TW_INTERNAL_STORAGE_MOUNT_POINT)
endif
ifneq ($(TW_EXTERNAL_STORAGE_PATH),)
	LOCAL_CFLAGS += -DTW_EXTERNAL_STORAGE_PATH=$(TW_EXTERNAL_STORAGE_PATH)
endif
ifneq ($(TW_EXTERNAL_STORAGE_MOUNT_POINT),)
	LOCAL_CFLAGS += -DTW_EXTERNAL_STORAGE_MOUNT_POINT=$(TW_EXTERNAL_STORAGE_MOUNT_POINT)
endif
ifeq ($(TW_HAS_NO_RECOVERY_PARTITION), true)
    LOCAL_CFLAGS += -DTW_HAS_NO_RECOVERY_PARTITION
endif
ifeq ($(TW_NO_REBOOT_BOOTLOADER), true)
    LOCAL_CFLAGS += -DTW_NO_REBOOT_BOOTLOADER
endif
ifeq ($(TW_NO_REBOOT_RECOVERY), true)
    LOCAL_CFLAGS += -DTW_NO_REBOOT_RECOVERY
endif
ifeq ($(TW_NO_BATT_PERCENT), true)
    LOCAL_CFLAGS += -DTW_NO_BATT_PERCENT
endif
ifneq ($(TW_CUSTOM_POWER_BUTTON),)
	LOCAL_CFLAGS += -DTW_CUSTOM_POWER_BUTTON=$(TW_CUSTOM_POWER_BUTTON)
endif


# This binary is in the recovery ramdisk, which is otherwise a copy of root.
# It gets copied there in config/Makefile.  LOCAL_MODULE_TAGS suppresses
# a (redundant) copy of the binary in /system/bin for user builds.
# TODO: Build the ramdisk image in a more principled way.

LOCAL_MODULE_TAGS := eng

LOCAL_STATIC_LIBRARIES :=
LOCAL_SHARED_LIBRARIES :=

LOCAL_STATIC_LIBRARIES += libminzip libunz libmincrypt
LOCAL_STATIC_LIBRARIES += libminui libpixelflinger_static libpng libjpeg
LOCAL_SHARED_LIBRARIES += libz libmtdutils libc libstlport libcutils libstdc++

ifeq ($(TARGET_RECOVERY_UI_LIB),)
  LOCAL_SRC_FILES += default_recovery_ui.c
else
  LOCAL_STATIC_LIBRARIES += $(TARGET_RECOVERY_UI_LIB)
endif

ifeq ($(TARGET_RECOVERY_GUI),true)
  LOCAL_STATIC_LIBRARIES += libgui
else
  LOCAL_SRC_FILES += gui_stub.c
endif

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := verifier_test.c verifier.c

LOCAL_MODULE := verifier_test

LOCAL_FORCE_STATIC_EXECUTABLE := true

LOCAL_MODULE_TAGS := tests

LOCAL_STATIC_LIBRARIES := libmincrypt libcutils libstdc++ libc

include $(BUILD_EXECUTABLE)

include $(commands_recovery_local_path)/htcdumlock/Android.mk
include $(commands_recovery_local_path)/minui/Android.mk
include $(commands_recovery_local_path)/minelf/Android.mk
include $(commands_recovery_local_path)/gui/Android.mk
include $(commands_recovery_local_path)/minzip/Android.mk
include $(commands_recovery_local_path)/mmcutils/Android.mk
include $(commands_recovery_local_path)/mtdutils/Android.mk
include $(commands_recovery_local_path)/bmlutils/Android.mk
include $(commands_recovery_local_path)/flashutils/Android.mk
include $(commands_recovery_local_path)/tools/Android.mk
include $(commands_recovery_local_path)/edify/Android.mk
include $(commands_recovery_local_path)/prebuilt/Android.mk
include $(commands_recovery_local_path)/updater/Android.mk
include $(commands_recovery_local_path)/applypatch/Android.mk
include $(commands_recovery_local_path)/htc-offmode-charge/Android.mk
commands_recovery_local_path :=

endif   # TARGET_ARCH == arm
endif    # !TARGET_SIMULATOR


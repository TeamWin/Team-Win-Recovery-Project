LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(TW_INCLUDE_BLOBPACK), true)
	LOCAL_SRC_FILES:= \
		blobpack.cpp
	LOCAL_CFLAGS:= -g -c -W
	LOCAL_MODULE:=blobpack
	LOCAL_MODULE_TAGS:= eng
	LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
	LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
	include $(BUILD_EXECUTABLE)
endif

#LOCAL_PATH:= $(call my-dir)

#include $(CLEAR_VARS)
#LOCAL_MODULE_TAGS := eng
#LOCAL_SRC_FILES := blobpack.cpp
#LOCAL_CFLAGS += -I. -Ishared -Wall -ggdb
#LOCAL_MODULE := blobpack_tfp
#LOCAL_MODULE_TAGS := optional
#include $(BUILD_HOST_EXECUTABLE)

#$(call dist-for-goals,dist_files,$(LOCAL_BUILT_MODULE))


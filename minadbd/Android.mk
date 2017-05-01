# Copyright 2005 The Android Open Source Project
#
# Android.mk for adb
#

LOCAL_PATH:= $(call my-dir)

# minadbd library
# =========================================================

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	adb.c \
	fdevent.c \
	transport.c \
	transport_usb.c \
	sockets.c \
	services.c \
	usb_linux_client.c \
	utils.c \
       ../../../system/core/adb/transport_local.c

LOCAL_CFLAGS := -O2 -g -DADB_HOST=0 -Wall -Wno-unused-parameter
LOCAL_CFLAGS += -D_XOPEN_SOURCE -D_GNU_SOURCE
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := libminadbd

LOCAL_SHARED_LIBRARIES := libcutils libc
include $(BUILD_SHARED_LIBRARY)




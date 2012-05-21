LOCAL_PATH := $(call my-dir)

ifneq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_ARCH),arm)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := flashutils.c
LOCAL_MODULE := libflashutils
LOCAL_MODULE_TAGS := eng
LOCAL_C_INCLUDES += bootable/recovery
LOCAL_SHARED_LIBRARIES := libc libmmcutils libbmlutils
LOCAL_STATIC_LIBRARIES := libmtdutils

BOARD_RECOVERY_DEFINES := BOARD_BML_BOOT BOARD_BML_RECOVERY

$(foreach board_define,$(BOARD_RECOVERY_DEFINES), \
  $(if $($(board_define)), \
    $(eval LOCAL_CFLAGS += -D$(board_define)=\"$($(board_define))\") \
  ) \
  )

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := flash_image
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := flash_image.c
LOCAL_SHARED_LIBRARIES := libflashutils libmmcutils libbmlutils libcutils libc
LOCAL_STATIC_LIBRARIES := libmtdutils
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := dump_image
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := dump_image.c
LOCAL_SHARED_LIBRARIES := libflashutils libmmcutils libbmlutils libcutils libc
LOCAL_STATIC_LIBRARIES := libmtdutils
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := erase_image
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := erase_image.c
LOCAL_SHARED_LIBRARIES := libflashutils libmmcutils libbmlutils libcutils libc
LOCAL_STATIC_LIBRARIES := libmtdutils
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := flash_image
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/utilities
LOCAL_SRC_FILES := flash_image.c
LOCAL_SHARED_LIBRARIES := libflashutils libmmcutils libbmlutils libcutils libc
LOCAL_STATIC_LIBRARIES := libmtdutils
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := dump_image
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/utilities
LOCAL_SRC_FILES := dump_image.c
LOCAL_SHARED_LIBRARIES := libflashutils libmmcutils libbmlutils libcutils libc
LOCAL_STATIC_LIBRARIES := libmtdutils
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := erase_image
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/utilities
LOCAL_SRC_FILES := erase_image.c
LOCAL_SHARED_LIBRARIES := libflashutils libmmcutils libbmlutils libcutils libc
LOCAL_STATIC_LIBRARIES := libmtdutils
include $(BUILD_EXECUTABLE)

endif # TARGET_ARCH == arm
endif # !TARGET_SIMULATOR

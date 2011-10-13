LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    gui.cpp \
    resources.cpp \
    pages.cpp \
    text.cpp \
    image.cpp \
    action.cpp \
    console.cpp \
    fill.cpp \
    button.cpp \
    checkbox.cpp \
    fileselector.cpp \
    progressbar.cpp \
    animation.cpp \
    conditional.cpp \
    tinyxml.cpp \
    tinyxmlerror.cpp \
    tinyxmlparser.cpp

LOCAL_MODULE := libgui

LOCAL_CFLAGS += -DTIXML_USE_STL

LOCAL_C_INCLUDES += bionic external/stlport/stlport $(TARGET_DEVICE_DIR)/recovery

include $(BUILD_STATIC_LIBRARY)


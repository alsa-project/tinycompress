# SPDX-License-Identifier: (LGPL-2.1-only OR BSD-3-Clause)

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= $(LOCAL_PATH)/include
LOCAL_SRC_FILES:= src/lib/compress.c
LOCAL_MODULE := libtinycompress
LOCAL_SHARED_LIBRARIES:= libcutils libutils
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= $(LOCAL_PATH)/include
LOCAL_SRC_FILES:= src/utils/cplay.c
LOCAL_MODULE := cplay
LOCAL_SHARED_LIBRARIES:= libcutils libutils libtinycompress
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= $(LOCAL_PATH)/include
LOCAL_SRC_FILES:= src/utils/crec.c
LOCAL_MODULE := crec
LOCAL_SHARED_LIBRARIES:= libcutils libutils libtinycompress
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)


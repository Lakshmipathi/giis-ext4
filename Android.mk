LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := eng
LOCAL_SRC_FILES := src/giis-ext4.c
LOCAL_C_INCLUDES := external/e2fsprogs/lib/ external/sqlite/dist/

LOCAL_MODULE := giis-ext4

LOCAL_SHARED_LIBRARIES += libsqlite  libext2fs

include $(BUILD_EXECUTABLE)

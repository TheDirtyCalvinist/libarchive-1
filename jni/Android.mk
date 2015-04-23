LOCAL_PATH := $(call my-dir)
BZ2_PATH := ../bzip2
ARCHIVE_PATH := ../archive

include $(CLEAR_VARS)



FILE_LIST := $(wildcard ${LOCAL_PATH}/../archive/*.c)

FILE_LIST += ${BZ2_PATH}/blocksort.c\
                                ${BZ2_PATH}/bzip2.c\
                                ${BZ2_PATH}/bzip2recover.c\
                                ${BZ2_PATH}/bzlib.c\
                                ${BZ2_PATH}/compress.c\
                                ${BZ2_PATH}/crctable.c\
                                ${BZ2_PATH}/decompress.c\
                                ${BZ2_PATH}/dlltest.c\
                                ${BZ2_PATH}/huffman.c\
                                ${BZ2_PATH}/mk251.c\
                                ${BZ2_PATH}/randtable.c\
                                ${BZ2_PATH}/spewG.c\
                                ${BZ2_PATH}/unzcrash.c\
                                \

LOCAL_MODULE := libarchive
LOCAL_CFLAGS += -DPLATFORM_CONFIG_H=\"libarchive_custom_config.h\"
LOCAL_C_INCLUDES += jni/${BZ2_PATH}
LOCAL_SRC_FILES := $(FILE_LIST:$(LOCAL_PATH)/%=%)


include $(BUILD_STATIC_LIBRARY)

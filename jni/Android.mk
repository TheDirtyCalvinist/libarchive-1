LOCAL_PATH := $(call my-dir)
BZ2_PATH := ../bzip2
ARCHIVE_PATH := ../archive

include $(CLEAR_VARS)

LOCAL_MODULE := libarchive
LOCAL_CFLAGS += -DPLATFORM_CONFIG_H=\"libarchive_custom_config.h\"
LOCAL_C_INCLUDES += jni/${BZ2_PATH}
LOCAL_SRC_FILES := ${BZ2_PATH}/blocksort.c\
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
                   ${ARCHIVE_PATH}/archive_check_magic.c\
                   ${ARCHIVE_PATH}/archive_entry_copy_bhfi.c\
                   ${ARCHIVE_PATH}/archive_entry_copy_stat.c\
                   ${ARCHIVE_PATH}/archive_entry_link_resolver.c\
                   ${ARCHIVE_PATH}/archive_entry_stat.c\
                   ${ARCHIVE_PATH}/archive_entry_strmode.c\
                   ${ARCHIVE_PATH}/archive_entry_xattr.c\
                   ${ARCHIVE_PATH}/archive_entry.c\
                   ${ARCHIVE_PATH}/archive_read_data_into_fd.c\
                   ${ARCHIVE_PATH}/archive_read_disk_entry_from_file.c\
                   ${ARCHIVE_PATH}/archive_read_disk_set_standard_lookup.c\
                   ${ARCHIVE_PATH}/archive_read_disk.c\
                   ${ARCHIVE_PATH}/archive_read_extract.c\
                   ${ARCHIVE_PATH}/archive_read_open_fd.c\
                   ${ARCHIVE_PATH}/archive_read_open_file.c\
                   ${ARCHIVE_PATH}/archive_read_open_filename.c\
                   ${ARCHIVE_PATH}/archive_read_open_memory.c\
                   ${ARCHIVE_PATH}/archive_read_support_compression_all.c\
                   ${ARCHIVE_PATH}/archive_read_support_compression_bzip2.c\
                   ${ARCHIVE_PATH}/archive_read_support_compression_compress.c\
                   ${ARCHIVE_PATH}/archive_read_support_compression_gzip.c\
                   ${ARCHIVE_PATH}/archive_read_support_compression_none.c\
                   ${ARCHIVE_PATH}/archive_read_support_compression_program.c\
                   ${ARCHIVE_PATH}/archive_read_support_compression_rpm.c\
                   ${ARCHIVE_PATH}/archive_read_support_compression_uu.c\
                   ${ARCHIVE_PATH}/archive_read_support_compression_xz.c\
                   ${ARCHIVE_PATH}/archive_read_support_format_all.c\
                   ${ARCHIVE_PATH}/archive_read_support_format_ar.c\
                   ${ARCHIVE_PATH}/archive_read_support_format_cpio.c\
                   ${ARCHIVE_PATH}/archive_read_support_format_empty.c\
                   ${ARCHIVE_PATH}/archive_read_support_format_iso9660.c\
                   ${ARCHIVE_PATH}/archive_read_support_format_mtree.c\
                   ${ARCHIVE_PATH}/archive_read_support_format_raw.c\
                   ${ARCHIVE_PATH}/archive_read_support_format_tar.c\
                   ${ARCHIVE_PATH}/archive_read_support_format_xar.c\
                   ${ARCHIVE_PATH}/archive_read_support_format_zip.c\
                   ${ARCHIVE_PATH}/archive_read.c\
                   ${ARCHIVE_PATH}/archive_string_sprintf.c\
                   ${ARCHIVE_PATH}/archive_string.c\
                   ${ARCHIVE_PATH}/archive_util.c\
                   ${ARCHIVE_PATH}/archive_virtual.c\
                   ${ARCHIVE_PATH}/archive_windows.c\
                   ${ARCHIVE_PATH}/archive_write_disk_set_standard_lookup.c\
                   ${ARCHIVE_PATH}/archive_write_disk.c\
                   ${ARCHIVE_PATH}/archive_write_open_fd.c\
                   ${ARCHIVE_PATH}/archive_write_open_file.c\
                   ${ARCHIVE_PATH}/archive_write_open_filename.c\
                   ${ARCHIVE_PATH}/archive_write_open_memory.c\
                   ${ARCHIVE_PATH}/archive_write_set_compression_bzip2.c\
                   ${ARCHIVE_PATH}/archive_write_set_compression_compress.c\
                   ${ARCHIVE_PATH}/archive_write_set_compression_gzip.c\
                   ${ARCHIVE_PATH}/archive_write_set_compression_none.c\
                   ${ARCHIVE_PATH}/archive_write_set_compression_program.c\
                   ${ARCHIVE_PATH}/archive_write_set_compression_xz.c\
                   ${ARCHIVE_PATH}/archive_write_set_format_ar.c\
                   ${ARCHIVE_PATH}/archive_write_set_format_by_name.c\
                   ${ARCHIVE_PATH}/archive_write_set_format_cpio_newc.c\
                   ${ARCHIVE_PATH}/archive_write_set_format_cpio.c\
                   ${ARCHIVE_PATH}/archive_write_set_format_mtree.c\
                   ${ARCHIVE_PATH}/archive_write_set_format_pax.c\
                   ${ARCHIVE_PATH}/archive_write_set_format_shar.c\
                   ${ARCHIVE_PATH}/archive_write_set_format_ustar.c\
                   ${ARCHIVE_PATH}/archive_write_set_format_zip.c\
                   ${ARCHIVE_PATH}/archive_write_set_format.c\
                   ${ARCHIVE_PATH}/archive_write.c\
                   ${ARCHIVE_PATH}/filter_fork.c

include $(BUILD_STATIC_LIBRARY)

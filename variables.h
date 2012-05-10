/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _VARIABLES_HEADER_
#define _VARIABLES_HEADER_

#define TW_VERSION_STR              "2.1.3"

#define TW_USE_COMPRESSION_VAR      "tw_use_compression"
#define TW_IGNORE_IMAGE_SIZE        "tw_ignore_image_size"
#define TW_FILENAME                 "tw_filename"
#define TW_ZIP_INDEX                "tw_zip_index"
#define TW_ZIP_QUEUE_COUNT			"tw_zip_queue_count"

#define TW_BACKUP_NAME		        "tw_backup_name"
#define TW_BACKUP_SYSTEM_VAR        "tw_backup_system"
#define TW_BACKUP_DATA_VAR          "tw_backup_data"
#define TW_BACKUP_BOOT_VAR          "tw_backup_boot"
#define TW_BACKUP_RECOVERY_VAR      "tw_backup_recovery"
#define TW_BACKUP_CACHE_VAR         "tw_backup_cache"
#define TW_BACKUP_ANDSEC_VAR        "tw_backup_andsec"
#define TW_BACKUP_SDEXT_VAR         "tw_backup_sdext"
#define TW_BACKUP_SP1_VAR           "tw_backup_sp1"
#define TW_BACKUP_SP2_VAR           "tw_backup_sp2"
#define TW_BACKUP_SP3_VAR           "tw_backup_sp3"
#define TW_BACKUP_AVG_IMG_RATE      "tw_backup_avg_img_rate"
#define TW_BACKUP_AVG_FILE_RATE     "tw_backup_avg_file_rate"
#define TW_BACKUP_AVG_FILE_COMP_RATE    "tw_backup_avg_file_comp_rate"
#define TW_BACKUP_SYSTEM_SIZE       "tw_backup_system_size"
#define TW_BACKUP_DATA_SIZE         "tw_backup_data_size"
#define TW_BACKUP_BOOT_SIZE         "tw_backup_boot_size"
#define TW_BACKUP_RECOVERY_SIZE     "tw_backup_recovery_size"
#define TW_BACKUP_CACHE_SIZE        "tw_backup_cache_size"
#define TW_BACKUP_ANDSEC_SIZE       "tw_backup_andsec_size"
#define TW_BACKUP_SDEXT_SIZE        "tw_backup_sdext_size"
#define TW_BACKUP_SP1_SIZE          "tw_backup_sp1_size"
#define TW_BACKUP_SP2_SIZE          "tw_backup_sp2_size"
#define TW_BACKUP_SP3_SIZE          "tw_backup_sp3_size"
#define TW_STORAGE_FREE_SIZE        "tw_storage_free_size"

#define TW_RESTORE_SYSTEM_VAR       "tw_restore_system"
#define TW_RESTORE_DATA_VAR         "tw_restore_data"
#define TW_RESTORE_BOOT_VAR         "tw_restore_boot"
#define TW_RESTORE_RECOVERY_VAR     "tw_restore_recovery"
#define TW_RESTORE_CACHE_VAR        "tw_restore_cache"
#define TW_RESTORE_ANDSEC_VAR       "tw_restore_andsec"
#define TW_RESTORE_SDEXT_VAR        "tw_restore_sdext"
#define TW_RESTORE_SP1_VAR          "tw_restore_sp1"
#define TW_RESTORE_SP2_VAR          "tw_restore_sp2"
#define TW_RESTORE_SP3_VAR          "tw_restore_sp3"
#define TW_RESTORE_AVG_IMG_RATE     "tw_restore_avg_img_rate"
#define TW_RESTORE_AVG_FILE_RATE    "tw_restore_avg_file_rate"
#define TW_RESTORE_AVG_FILE_COMP_RATE    "tw_restore_avg_file_comp_rate"

#define TW_SHOW_SPAM_VAR            "tw_show_spam"
#define TW_COLOR_THEME_VAR          "tw_color_theme"
#define TW_VERSION_VAR              "tw_version"
#define TW_SORT_FILES_BY_DATE_VAR   "tw_sort_files_by_date"
#define TW_GUI_SORT_ORDER           "tw_gui_sort_order"
#define TW_ZIP_LOCATION_VAR         "tw_zip_location"
#define TW_ZIP_INTERNAL_VAR         "tw_zip_internal"
#define TW_ZIP_EXTERNAL_VAR         "tw_zip_external"
#define TW_FORCE_MD5_CHECK_VAR      "tw_force_md5_check"
#define TW_SKIP_MD5_CHECK_VAR       "tw_skip_md5_check"
#define TW_SKIP_MD5_GENERATE_VAR    "tw_skip_md5_generate"
#define TW_SIGNED_ZIP_VERIFY_VAR    "tw_signed_zip_verify"
#define TW_REBOOT_AFTER_FLASH_VAR   "tw_reboot_after_flash_option"
#define TW_TIME_ZONE_VAR            "tw_time_zone"
#define TW_RM_RF_VAR                "tw_rm_rf"

#define TW_BACKUPS_FOLDER_VAR       "tw_backups_folder"

#define TW_SP1_PARTITION_NAME_VAR   "tw_sp1_name"
#define TW_SP2_PARTITION_NAME_VAR   "tw_sp2_name"
#define TW_SP3_PARTITION_NAME_VAR   "tw_sp3_name"

#define TW_SDEXT_SIZE               "tw_sdext_size"
#define TW_SWAP_SIZE                "tw_swap_size"
#define TW_SDPART_FILE_SYSTEM       "tw_sdpart_file_system"
#define TW_TIME_ZONE_GUISEL         "tw_time_zone_guisel"
#define TW_TIME_ZONE_GUIOFFSET      "tw_time_zone_guioffset"
#define TW_TIME_ZONE_GUIDST         "tw_time_zone_guidst"

#define TW_ACTION_BUSY              "tw_busy"

#define TW_ALLOW_PARTITION_SDCARD   "tw_allow_partition_sdcard"

#define TW_SCREEN_OFF               "tw_screen_off"

#define TW_REBOOT_SYSTEM            "tw_reboot_system"
#define TW_REBOOT_RECOVERY          "tw_reboot_recovery"
#define TW_REBOOT_POWEROFF          "tw_reboot_poweroff"
#define TW_REBOOT_BOOTLOADER        "tw_reboot_bootloader"

#define TW_HAS_DUAL_STORAGE         "tw_has_dual_storage"
#define TW_USE_EXTERNAL_STORAGE     "tw_use_external_storage"
#define TW_HAS_INTERNAL             "tw_has_internal"
#define TW_INTERNAL_PATH            "tw_internal_path"         // /data/media or /internal
#define TW_INTERNAL_MOUNT           "tw_internal_mount"        // /data or /internal
#define TW_INTERNAL_LABEL           "tw_internal_label"        // data or internal
#define TW_HAS_EXTERNAL             "tw_has_external"
#define TW_EXTERNAL_PATH            "tw_external_path"         // /sdcard or /external/sdcard2
#define TW_EXTERNAL_MOUNT           "tw_external_mount"        // /sdcard or /external
#define TW_EXTERNAL_LABEL           "tw_external_label"        // sdcard or external

#define TW_HAS_DATA_MEDIA           "tw_has_data_media"

#define TW_HAS_RECOVERY_PARTITION   "tw_has_recovery_partition"
#define TW_HAS_ANDROID_SECURE       "tw_has_android_secure"
#define TW_HAS_SDEXT_PARTITION      "tw_has_sdext_partition"
#define TW_HAS_USB_STORAGE          "tw_has_usb_storage"
#define TW_NO_BATTERY_PERCENT       "tw_no_battery_percent"
#define TW_POWER_BUTTON             "tw_power_button"
#define TW_SIMULATE_ACTIONS         "tw_simulate_actions"
#define TW_SIMULATE_FAIL            "tw_simulate_fail"
#define TW_DONT_UNMOUNT_SYSTEM      "tw_dont_unmount_system"
// #define TW_ALWAYS_RMRF              "tw_always_rmrf"

#define TW_SHOW_DUMLOCK             "tw_show_dumlock"
#define TW_HAS_INJECTTWRP           "tw_has_injecttwrp"
#define TW_INJECT_AFTER_ZIP         "tw_inject_after_zip"
#define TW_HAS_DATADATA             "tw_has_datadata"

// Also used:
//   tw_boot_is_mountable
//   tw_system_is_mountable
//   tw_data_is_mountable
//   tw_cache_is_mountable
//   tw_sdcext_is_mountable
//   tw_sdcint_is_mountable
//   tw_sd-ext_is_mountable
//   tw_sp1_is_mountable
//   tw_sp2_is_mountable
//   tw_sp3_is_mountable

#endif  // _VARIABLES_HEADER_

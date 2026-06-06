#define LOG_TAG "STORE" ///< "STORE" log tag for this file

#include "storage.h"
#include "esp_littlefs.h"
#include "esp_vfs.h"
#include "logger.h"

static const char *LITTLEFS_MOUNT_POINT =
    "/data"; ///< Mount point for LittleFS filesystem
/** @brief Partition label for the LittleFS filesystem. */
static const char *LITTLEFS_PARTITION_LABEL = "storage";
/** @brief Flag indicating if the filesystem is currently mounted. */
static bool is_mounted = false;

esp_err_t storage_init(void) {
  esp_vfs_littlefs_conf_t conf = {
      .base_path = LITTLEFS_MOUNT_POINT,
      .partition_label = LITTLEFS_PARTITION_LABEL,
      .format_if_mount_failed = false,
      .read_only = false,
  };

  esp_err_t ret = esp_vfs_littlefs_register(&conf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      LOGE("Failed to mount LittleFS or format filesystem");
    } else if (ret == ESP_ERR_INVALID_STATE) {
      LOGE("ESP_ERR_INVALID_STATE");
    } else {
      LOGE("Failed to initialize LittleFS: %s", esp_err_to_name(ret));
    }
    return ret;
  }

  is_mounted = true;

  size_t total = 0, used = 0;
  ret = esp_littlefs_info(LITTLEFS_PARTITION_LABEL, &total, &used);
  if (ret == ESP_OK) {
    LOGI("LittleFS mounted at %s. Total: %d bytes, Used: %d bytes",
         LITTLEFS_MOUNT_POINT, total, used);
  } else {
    LOGE("Failed to get LittleFS information");
  }

  return ESP_OK;
}

bool storage_is_mounted(void) { return is_mounted; }

const char *storage_get_mount_point(void) { return LITTLEFS_MOUNT_POINT; }

const char *storage_get_partition_label(void) {
  return LITTLEFS_PARTITION_LABEL;
}

esp_err_t storage_get_info(storage_info_t *info) {
  if (!info) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!is_mounted) {
    return ESP_ERR_INVALID_STATE;
  }

  size_t total = 0, used = 0;
  esp_err_t ret = esp_littlefs_info(LITTLEFS_PARTITION_LABEL, &total, &used);
  if (ret != ESP_OK) {
    return ret;
  }

  info->total_bytes = total;
  info->used_bytes = used;
  info->free_bytes = total - used;

  return ESP_OK;
}

void storage_print_info(void) {
  storage_info_t info;
  LOGI("==================================================");
  LOGI("STORAGE INFORMATION");
  LOGI("==================================================");
  LOGI("Mount Point      : %s", LITTLEFS_MOUNT_POINT);
  LOGI("Partition Label  : %s", LITTLEFS_PARTITION_LABEL);
  LOGI("Status           : %s", is_mounted ? "Mounted" : "Not Mounted");

  if (storage_get_info(&info) == ESP_OK) {
    float usage = (float)info.used_bytes / (float)info.total_bytes * 100.0f;
    LOGI("--------------------------------------------------");
    LOGI("Total Space      : %zu Bytes (%zu KB)", info.total_bytes,
         info.total_bytes / 1024);
    LOGI("Used Space       : %zu Bytes (%zu KB)", info.used_bytes,
         info.used_bytes / 1024);
    LOGI("Free Space       : %zu Bytes (%zu KB)", info.free_bytes,
         info.free_bytes / 1024);
    LOGI("Usage            : %.2f %%", usage);
  } else {
    LOGE("Failed to retrieve storage statistics");
  }
  LOGI("==================================================");
}

size_t storage_get_total_bytes(void) {
  storage_info_t info;
  if (storage_get_info(&info) != ESP_OK) {
    return 0;
  }
  return info.total_bytes;
}

size_t storage_get_used_bytes(void) {
  storage_info_t info;
  if (storage_get_info(&info) != ESP_OK) {
    return 0;
  }
  return info.used_bytes;
}

size_t storage_get_free_bytes(void) {
  storage_info_t info;
  if (storage_get_info(&info) != ESP_OK) {
    return 0;
  }
  return info.free_bytes;
}

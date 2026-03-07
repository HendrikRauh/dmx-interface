#define LOG_TAG "STORE"

#include "storage.h"
#include "esp_littlefs.h"
#include "esp_vfs.h"
#include "logger.h"

static const char *LITTLEFS_MOUNT_POINT = "/data";

esp_err_t storage_init(void) {
  esp_vfs_littlefs_conf_t conf = {
      .base_path = LITTLEFS_MOUNT_POINT,
      .partition_label = "storage",
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

  size_t total = 0, used = 0;
  ret = esp_littlefs_info(conf.partition_label, &total, &used);
  if (ret == ESP_OK) {
    LOGI("LittleFS mounted at %s. Total: %d bytes, Used: %d bytes",
         LITTLEFS_MOUNT_POINT, total, used);
  } else {
    LOGE("Failed to get LittleFS information");
  }

  return ESP_OK;
}

const char *storage_get_mount_point(void) { return LITTLEFS_MOUNT_POINT; }

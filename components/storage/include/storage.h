#ifndef STORAGE_H
#define STORAGE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and mount LittleFS filesystem
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t storage_init(void);

/**
 * @brief Get the mount point for the LittleFS filesystem
 *
 * @return Pointer to the mount point string (e.g., "/data")
 */
const char *storage_get_mount_point(void);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_H */

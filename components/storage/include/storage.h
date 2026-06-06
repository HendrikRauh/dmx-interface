#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Storage information structure
 */
typedef struct {
  size_t total_bytes; ///< Total size of the storage in bytes
  size_t used_bytes;  ///< Used size of the storage in bytes
  size_t free_bytes;  ///< Free size of the storage in bytes
} storage_info_t;

/**
 * @brief Initialize and mount LittleFS filesystem
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t storage_init(void);

/**
 * @brief Check if the storage is mounted and ready
 *
 * @return true if mounted, false otherwise
 */
bool storage_is_mounted(void);

/**
 * @brief Get the mount point for the LittleFS filesystem
 *
 * @return Pointer to the mount point string (e.g., "/data")
 */
const char *storage_get_mount_point(void);

/**
 * @brief Get the partition label for the LittleFS filesystem
 *
 * @return Pointer to the partition label string
 */
const char *storage_get_partition_label(void);

/**
 * @brief Get comprehensive storage information
 *
 * @param[out] info Pointer to storage_info_t structure to fill
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t storage_get_info(storage_info_t *info);

/**
 * @brief Print storage information to the log
 */
void storage_print_info(void);

/**
 * @brief Get the total size of the storage in bytes
 *
 * @return Total size in bytes, or 0 on error
 */
size_t storage_get_total_bytes(void);

/**
 * @brief Get the used size of the storage in bytes
 *
 * @return Used size in bytes, or 0 on error
 */
size_t storage_get_used_bytes(void);

/**
 * @brief Get the free size of the storage in bytes
 *
 * @return Free size in bytes, or 0 on error
 */
size_t storage_get_free_bytes(void);

#ifdef __cplusplus
}
#endif

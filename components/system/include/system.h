#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Structure to hold static chip configuration details.
 */
typedef struct {
  const char *model_name; /**< Chip model name (e.g., "ESP32-S2") */
  uint8_t cores;          /**< Number of CPU cores */
  uint32_t revision;      /**< Chip hardware revision number */
} sys_chip_info_t;

/**
 * @brief Initializes the system monitoring component.
 * Sets up the internal temperature sensor and tracks initial values.
 * @return ESP_OK on success, or an appropriate error code on failure.
 */
esp_err_t system_init(void);

/**
 * @brief Gets the current internal CPU temperature.
 * @return The current temperature in degrees Celsius, or 0.0f if the sensor
 * is uninitialized.
 */
float system_get_temperature(void);

/**
 * @brief Gets the highest recorded CPU temperature since boot.
 * @return The maximum temperature in degrees Celsius.
 */
float system_get_max_temperature(void);

/**
 * @brief Gets the current available free heap size (RAM).
 * @return Available heap size in bytes.
 */
uint32_t system_get_free_heap(void);

/**
 * @brief Gets the minimum ever free heap size since boot (watermark).
 * This is crucial for detecting close-to-OOM (Out Of Memory) conditions.
 * @return Minimum free heap size in bytes.
 */
uint32_t system_get_min_free_heap(void);

/**
 * @brief Calculates the total real-time CPU utilization.
 * Requires CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS to be enabled.
 * @return Current CPU usage as a percentage (0 to 100).
 */
uint8_t system_get_cpu_usage(void);

/**
 * @brief Formats raw FreeRTOS statistics into a human-readable task list.
 * Requires CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS to be enabled.
 * Writes a table containing task names, status, priority, and remaining stack
 * space.
 *
 * @warning vTaskList does not perform bounds checking. Ensure the buffer is
 * large enough (at least 40 bytes per task).
 *
 * @param[out] buffer Destination character buffer to write the table string.
 * @param[in] buffer_len Size of the destination buffer in bytes.
 */
void system_get_tasks_list(char *buffer, size_t buffer_len);

/**
 * @brief Gets the active firmware build version.
 * Matches the Git commit short hash (appends '-d' if local modifications
 * exist).
 * @return Pointer to a null-terminated string containing the version.
 */
const char *system_get_version(void);

/**
 * @brief Gets the system uptime since boot.
 * @return Total uptime in milliseconds.
 */
int64_t system_get_uptime_ms(void);

/**
 * @brief Gets the reason for the last system reset.
 * @return The reset reason code (maps to esp_reset_reason_t).
 */
int system_get_reset_reason(void);

/**
 * @brief Retrieves static chip hardware information.
 * @param[out] chip_info Pointer to the structure to be filled with chip
 * details.
 */
void system_get_chip_info(sys_chip_info_t *chip_info);

/**
 * @brief Prints a complete, formatted overview of all system statistics to the
 * console.
 * Internally uses ESP_LOGI or printf to output a comprehensive snapshot of
 * the device status.
 */
void system_print_info(void);

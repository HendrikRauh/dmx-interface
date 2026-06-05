/** @brief Log tag for the system component. */
#define LOG_TAG "SYSTEM"

#include "system.h"
#include "logger.h"

#include <stdio.h>
#include <string.h>

#include "driver/temperature_sensor.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system_version.h"

/** @brief Handle for the internal temperature sensor. */
static temperature_sensor_handle_t temp_sensor = NULL;
/** @brief Highest measured temperature since boot. */
static float max_measured_temp = -100.0f;

esp_err_t system_init(void) {
  temperature_sensor_config_t temp_cfg =
      TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
  esp_err_t err = temperature_sensor_install(&temp_cfg, &temp_sensor);

  if (err == ESP_OK) {
    err = temperature_sensor_enable(temp_sensor);
    if (err == ESP_OK) {
      temperature_sensor_get_celsius(temp_sensor, &max_measured_temp);
      LOGI("Component initialized successfully. Internal temp sensor active.");
    } else {
      LOGE("Failed to enable temperature sensor: %s", esp_err_to_name(err));
    }
  } else {
    LOGW("Temperature sensor installation failed (maybe not supported on this "
         "chip): %s",
         esp_err_to_name(err));
    // The component can still function without the temperature sensor
    err = ESP_OK;
  }
  return err;
}

float system_get_temperature(void) {
  if (!temp_sensor)
    return 0.0f;

  float current_temp = 0.0f;
  if (temperature_sensor_get_celsius(temp_sensor, &current_temp) == ESP_OK) {
    if (current_temp > max_measured_temp) {
      max_measured_temp = current_temp;
    }
    return current_temp;
  }
  return 0.0f;
}

float system_get_max_temperature(void) {
  system_get_temperature();
  return max_measured_temp;
}

uint32_t system_get_free_heap(void) { return esp_get_free_heap_size(); }

uint32_t system_get_min_free_heap(void) {
  return heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
}

uint8_t system_get_cpu_usage(void) {
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
  static uint32_t last_idle_time = 0;
  static uint64_t last_total_time = 0;

  TaskStatus_t idle_status;
  vTaskGetInfo(xTaskGetIdleTaskHandle(), &idle_status, pdFALSE, eRunning);

  uint32_t current_idle_time = idle_status.ulRunTimeCounter;
  uint64_t current_total_time = esp_timer_get_time();

  uint32_t idle_diff = current_idle_time - last_idle_time;
  uint64_t total_diff = current_total_time - last_total_time;

  last_idle_time = current_idle_time;
  last_total_time = current_total_time;

  if (total_diff == 0)
    return 0;

  // Use 64-bit math for percentage to avoid overflow if diffs are large
  uint8_t idle_percentage = (uint8_t)((idle_diff * 100ULL) / total_diff);
  if (idle_percentage > 100)
    idle_percentage = 100;

  return (uint8_t)(100 - idle_percentage);
#else
  return 0;
#endif
}

void system_get_tasks_list(char *buffer, size_t buffer_len) {
#if CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
  if (buffer && buffer_len > 0) {
    vTaskList(buffer);
  }
#else
  if (buffer && buffer_len > 0) {
    snprintf(buffer, buffer_len, "vTaskList disabled in sdkconfig");
  }
#endif
}

const char *system_get_version(void) { return SYS_VERSION; }

int64_t system_get_uptime_ms(void) { return esp_timer_get_time() / 1000; }

int system_get_reset_reason(void) { return (int)esp_reset_reason(); }

void system_get_chip_info(sys_chip_info_t *chip_info) {
  if (!chip_info)
    return;

  esp_chip_info_t info;
  esp_chip_info(&info);

  chip_info->cores = info.cores;
  chip_info->revision = info.revision;

  switch (info.model) {
  case CHIP_ESP32:
    chip_info->model_name = "ESP32";
    break;
  case CHIP_ESP32S2:
    chip_info->model_name = "ESP32-S2";
    break;
  case CHIP_ESP32S3:
    chip_info->model_name = "ESP32-S3";
    break;
  case CHIP_ESP32C3:
    chip_info->model_name = "ESP32-C3";
    break;
  case CHIP_ESP32C2:
    chip_info->model_name = "ESP32-C2";
    break;
  case CHIP_ESP32C6:
    chip_info->model_name = "ESP32-C6";
    break;
  case CHIP_ESP32H2:
    chip_info->model_name = "ESP32-H2";
    break;
  default:
    chip_info->model_name = "ESP32-Unknown";
    break;
  }
}

void system_print_info(void) {
  sys_chip_info_t chip;
  system_get_chip_info(&chip);

  const char *reason_str;
  switch ((esp_reset_reason_t)system_get_reset_reason()) {
  case ESP_RST_POWERON:
    reason_str = "Vat / Power-on reset";
    break;
  case ESP_RST_EXT:
    reason_str = "External pin reset";
    break;
  case ESP_RST_SW:
    reason_str = "Software reset via esp_restart";
    break;
  case ESP_RST_PANIC:
    reason_str = "Software panic / crash reset";
    break;
  case ESP_RST_INT_WDT:
    reason_str = "Interrupt watchdog reset";
    break;
  case ESP_RST_TASK_WDT:
    reason_str = "Task watchdog reset";
    break;
  case ESP_RST_DEEPSLEEP:
    reason_str = "Wakeup from deep sleep";
    break;
  case ESP_RST_BROWNOUT:
    reason_str = "Brownout reset (voltage drop)";
    break;
  default:
    reason_str = "Unknown reset reason";
    break;
  }
  LOGI("==================================================");
  LOGI("DEVICE SYSTEM INFO SNAPSHOT");
  LOGI("==================================================");
  LOGI("Firmware Version : %s", system_get_version());
  LOGI("Hardware SoC     : %s (Cores: %d, Rev: %lu)", chip.model_name,
       chip.cores, chip.revision);
  LOGI("Uptime           : %lld ms", system_get_uptime_ms());
  LOGI("Reset Reason     : %s", reason_str);
  LOGI("--------------------------------------------------");
  LOGI("CPU Temperature  : %.2f °C (Max: %.2f °C)", system_get_temperature(),
       system_get_max_temperature());
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
  LOGI("CPU Usage        : %d %%", system_get_cpu_usage());
#endif
  LOGI("Free Heap (RAM)  : %lu Bytes", system_get_free_heap());
  LOGI("Min Free Heap    : %lu Bytes", system_get_min_free_heap());
#if CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
  char *tasks_buffer = malloc(2048);
  if (tasks_buffer) {
    system_get_tasks_list(tasks_buffer, 2048);

    LOGI("==================================================");
    LOGI("Task Name              Status  Prio    Stack   ID");
    LOGI("--------------------------------------------------");

    char *line = strtok(tasks_buffer, "\n");
    while (line != NULL) {
      if (strlen(line) > 0) {
        LOGI("%s", line);
      }
      line = strtok(NULL, "\n");
    }
    free(tasks_buffer);
  }
#endif
  LOGI("==================================================");
}

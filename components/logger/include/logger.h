/**
 * @file logger.h
 * @brief Project-wide logging macros based on ESP-IDF's logging library.
 *
 * This header provides a set of simple logging macros (LOGE, LOGW, LOGI, etc.)
 * that wrap the underlying ESP-IDF logging functions (esp_log_e, esp_log_w,
 * etc.).
 *
 * @section usage Usage
 * To use these macros, a `LOG_TAG` should be defined before including this
 * header. The `LOG_TAG` is a string that identifies the source of the log
 * messages, typically the component or file name. If `LOG_TAG` is not defined,
 * a default tag "CHAOS" will be used.
 *
 * @example
 * #define LOG_TAG "MY_COMPONENT"
 * #include "logger.h"
 *
 * void my_function() {
 *     LOGI("This is an informational message.");
 *     LOGE("This is an error message with a value: %d", 42);
 * }
 */

#pragma once

#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOG_TAG
#define LOG_TAG "CHAOS" ///< Default log tag
#endif

/** @brief Log a message at Error level. */
#define LOGE(...) ESP_LOGE(LOG_TAG, __VA_ARGS__)
/** @brief Log a message at Warning level. */
#define LOGW(...) ESP_LOGW(LOG_TAG, __VA_ARGS__)
/** @brief Log a message at Info level. */
#define LOGI(...) ESP_LOGI(LOG_TAG, __VA_ARGS__)
/** @brief Log a message at Debug level. */
#define LOGD(...) ESP_LOGD(LOG_TAG, __VA_ARGS__)
/** @brief Log a message at Verbose level. */
#define LOGV(...) ESP_LOGV(LOG_TAG, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

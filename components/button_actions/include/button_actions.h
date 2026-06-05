/**
 * @file button_actions.h
 * @brief Hardware button abstraction layer for short and long press detection.
 *
 * This module wraps the official Espressif IoT button component to handle
 * custom actions.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the hardware button configuration and event callbacks.
 *
 * Configures the designated GPIO pin, sets up the debounce thresholds,
 * and registers callbacks for both a single/double/triple short click
 *
 * @return
 * - ESP_OK: Success
 * - ESP_FAIL: Failed to initialize the button or register callbacks
 */
esp_err_t button_init(void);

/**
 * @brief Checks if the button is currently physically pressed.
 *
 * @return true if pressed (active level), false otherwise.
 */
bool button_is_pressed(void);

#ifdef __cplusplus
}
#endif

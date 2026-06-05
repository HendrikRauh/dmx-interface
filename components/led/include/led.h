/**
 * @file led.h
 * @brief LED control component with flexible blinking and breathing modes.
 */

#pragma once

#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "logger.h"
#include <math.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Available operational modes for the status LED.
 */
typedef enum {
  LED_MODE_OFF,            /**< LED turned off */
  LED_MODE_BOOT_BREATHING, /**< Smooth fading/pulsing for startup */
  LED_MODE_RESET,          /**< Fast pulsing for resetting */
  LED_MODE_NORMAL,         /**< Constant solid light */
  LED_MODE_WARN,           /**< Slow blinking */
  LED_MODE_ERROR,          /**< Fast blinking */
} led_mode_t;

/**
 * @brief Initializes the LEDC peripheral for the status LED.
 *
 * Sets up the timer and channel configuration for the LED GPIO.
 *
 * @return ESP_OK on success, or an error code from the LEDC driver.
 */
esp_err_t led_init(void);

/**
 * @brief Sets the current operational mode of the LED.
 *
 * Modes include static, blinking, and breathing effects.
 *
 * @param[in] mode The desired @ref led_mode_t to activate.
 */
void led_set_mode(led_mode_t mode);

/**
 * @brief Dynamically updates the LED maximum brightness.
 *
 * @param[in] brightness Value from 0 (off) to 255 (max).
 */
void led_set_brightness(uint8_t brightness);

#ifdef __cplusplus
}
#endif

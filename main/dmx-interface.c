#define LOG_TAG "MAIN" ///< "MAIN" log tag for this file

#include <stdio.h>

#include "button_actions.h"
#include "config.h"
#include "esp_err.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "logger.h"
#include "nvs_flash.h"
#include "web_server.h"
#include "wifi.h"

/**
 * @brief Main entry point for the DMX Interface application.
 *
 * Initializes WiFi Access Point and starts the web server.
 * Keeps the application running indefinitely.
 */
void app_main(void) {
  LOGI("DMX Interface starting...");

  if (led_init() == ESP_OK) {
    led_set_mode(LED_MODE_BOOT_BREATHING);
  }

  // Basic system init needed for the button callback to work safely
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  if (config_init() != ESP_OK) {
    LOGE("Configuration initialization failed!");
    return;
  }

  // Initialize button handler (registers the 3s long-press callback)
  button_init();

  // Power-up Check:
  // If button is held, block boot and show WARN led.
  if (button_is_pressed()) {
    LOGW("Button detected as PRESSED on power-up! Waiting 3s for factory "
         "reset...");
    led_set_mode(LED_MODE_RESET);

    int hold_time_ms = 0;
    while (button_is_pressed()) {
      vTaskDelay(pdMS_TO_TICKS(100));
      hold_time_ms += 100;

      if (hold_time_ms % 500 == 0) {
        LOGI("Button still held... (%dms)", hold_time_ms);
      }

      if (hold_time_ms >= 3000) {
        LOGW("3000ms reached! Factory reset triggered.");

        led_set_mode(LED_MODE_OFF);
        config_reset_defaults();
        vTaskDelay(pdMS_TO_TICKS(2000));
        config_save();

        LOGW("Factory defaults applied. PLEASE RELEASE BUTTON TO REBOOT.");

        while (button_is_pressed()) {
          vTaskDelay(pdMS_TO_TICKS(100));
        }

        LOGI("Button released. Rebooting now...");
        esp_restart();
      }
    }

    LOGI("Button released after %dms, continuing normal boot.", hold_time_ms);
    led_set_mode(LED_MODE_BOOT_BREATHING);
  } else {
    LOGI("Button not pressed at startup.");
  }

  // Disable factory reset long-press for normal operation
  button_disable_factory_reset();

  err = wifi_start_ap("DMX", "ChaosDMX", 1, 4);
  if (err != ESP_OK) {
    LOGE("Failed to start WiFi AP: %s", esp_err_to_name(err));
    return;
  }

  httpd_handle_t server = webserver_start(NULL);
  if (server == NULL) {
    LOGE("Failed to start web server!");
    return;
  }

  LOGI("Web server started successfully");
  LOGI("Open http://192.168.4.1 in your browser");

  vTaskDelay(pdMS_TO_TICKS(5000));

  led_set_brightness(config_get_led_brightness());
  led_set_mode(LED_MODE_NORMAL);

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

#define LOG_TAG "MAIN" ///< "MAIN" log tag for this file

#include <stdio.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logger.h"
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

  esp_err_t wifi_err = wifi_start_ap("DMX", "mbgmbgmbg", 1, 4);
  if (wifi_err != ESP_OK) {
    LOGE("Failed to start WiFi AP: %s", esp_err_to_name(wifi_err));
    return;
  }

  // Start HTTP web server
  httpd_handle_t server = webserver_start(NULL);
  if (server == NULL) {
    LOGE("Failed to start web server!");
    return;
  }

  LOGI("Web server started successfully");
  LOGI("Open http://192.168.4.1 in your browser");

  // Keep the app running
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

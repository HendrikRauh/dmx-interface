#include <stdio.h>

#include "dmx.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "web_server.h"
#include "wifi.h"

static const char *TAG = "MAIN";

void app_main(void) {
  ESP_LOGI(TAG, "DMX Interface starting...");

  esp_err_t wifi_err = wifi_start_ap("DMX", "mbgmbgmbg", 1, 4);
  if (wifi_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WiFi AP: %s", esp_err_to_name(wifi_err));
    return;
  }

  // Start HTTP web server
  httpd_handle_t server = webserver_start(NULL);
  if (server == NULL) {
    ESP_LOGE(TAG, "Failed to start web server!");
    return;
  }

  ESP_LOGI(TAG, "Web server started successfully");
  ESP_LOGI(TAG, "Open http://192.168.4.1 in your browser");

  init_dmx(DMX_NUM_1, 21, 33);

  // Keep the app running
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

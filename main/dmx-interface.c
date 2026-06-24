#define LOG_TAG "MAIN" ///< "MAIN" log tag for this file

#include <stdio.h>

#include "button_actions.h"
#include "config.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "led.h"
#include "logger.h"
#include "network.h"
#include "nvs_flash.h"
#include "storage.h"
#include "system.h"
#include "web_server.h"

#define BIT_NETWORK_READY (1 << 0) ///< Event bit for network ready event

/**
 * @brief Handle to the event group used for synchronizing network events.
 */
static EventGroupHandle_t event_group;

/**
 * @brief Event handler for network events.
 * @param arg User-defined argument (not used)
 * @param event_base The base of the event
 * @param event_id The specific event ID within the base
 * @param event_data Pointer to event-specific data
 */
static void network_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data) {
  if (event_base == NETWORK_EVENT) {
    switch (event_id) {
    case NETWORK_EVENT_READY:
      if (webserver_start(NULL) == NULL) {
        LOGE("Failed to start web server.");
      }

      esp_netif_ip_info_t *ip_info = (esp_netif_ip_info_t *)event_data;
      LOGI("Open http://" IPSTR " in your browser", IP2STR(&ip_info->ip));

      if (event_group != NULL) {
        xEventGroupSetBits(event_group, BIT_NETWORK_READY);
      }
      break;
    case NETWORK_EVENT_DISCONNECTED:
      webserver_stop();
      break;
    case NETWORK_EVENT_CONNECTION_FAILED:
      webserver_stop();
      if (event_group != NULL) {
        xEventGroupClearBits(event_group, BIT_NETWORK_READY);
      }

      led_set_mode(LED_MODE_ERROR);
      LOGE("WiFi connection failed. Switching to AP mode...");

      network_stop_wifi();

      app_wifi_creds_t ap_creds;
      config_get_wifi_ap_config(&ap_creds);
      ESP_ERROR_CHECK(network_start_ap(ap_creds.ssid, ap_creds.password));
      break;
    }
  }
}

/**
 * @brief Main entry point for the DMX Interface application.
 *
 * Initializes the network stack and starts the web server.
 * Keeps the application running indefinitely.
 */
void app_main(void) {
  LOGI("DMX Interface starting...");

  ESP_ERROR_CHECK(led_init());
  led_set_mode(LED_MODE_BOOT_BREATHING);

  ESP_ERROR_CHECK(system_init());

  // Basic system init needed for the button callback to work safely
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  ESP_ERROR_CHECK(config_init());

  err = button_init();
  if (err != ESP_OK) {
    LOGE("Failed to initialize button: %s", esp_err_to_name(err));
  }


  // Power-up Check:
  // If button is held, block boot and show RESET led.
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

        ESP_ERROR_CHECK(config_reset_defaults());

        LOGI("Configuration reset to factory defaults in RAM.");

        led_set_mode(LED_MODE_OFF);

        vTaskDelay(pdMS_TO_TICKS(2000));

        ESP_ERROR_CHECK(config_save());

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

  event_group = xEventGroupCreate();
  if (event_group == NULL) {
    LOGE("Failed to create event group");
    return;
  }

  ESP_ERROR_CHECK(network_init());

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      NETWORK_EVENT, ESP_EVENT_ANY_ID, &network_event_handler, NULL, NULL));

  switch (config_get_connection()) {
  case APP_CONFIG_CONN_WIFI_AP:
    app_wifi_creds_t ap_creds;
    config_get_wifi_ap_config(&ap_creds);
    ESP_ERROR_CHECK(network_start_ap(ap_creds.ssid, ap_creds.password));
    break;
  case APP_CONFIG_CONN_WIFI_STA:
    app_wifi_creds_t sta_creds;
    config_get_wifi_sta_config(&sta_creds);
    ESP_ERROR_CHECK(network_start_sta(sta_creds.ssid, sta_creds.password));
    break;
  case APP_CONFIG_CONN_ETHERNET:
    LOGE("Ethernet is not yet supported");
    break;
  default:
    LOGE("Invalid connection type in config!");
    break;
  }

  // Wait for network to be ready before starting the web server
  xEventGroupWaitBits(event_group, BIT_NETWORK_READY, pdFALSE, pdTRUE,
                      portMAX_DELAY);

  vTaskDelay(pdMS_TO_TICKS(2000));

  led_set_brightness(config_get_led_brightness());
  led_set_mode(LED_MODE_NORMAL);

  storage_print_info();
  system_print_info();

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

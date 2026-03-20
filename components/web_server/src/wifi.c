#define LOG_TAG "WIFI" ///< "WIFI" log tag for this file

#include <string.h>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "logger.h"
#include "nvs_flash.h"
#include "wifi.h"

/**
 * @brief Indicates whether the WiFi AP is started.
 */
static bool s_wifi_started = false;

/**
 * @brief Start WiFi Access Point (AP) mode.
 *
 * Initializes and starts the WiFi AP with the given SSID and password.
 *
 * @param ssid SSID for the AP (1-32 characters)
 * @param password Password for the AP (min. 8 characters, optional)
 * @param channel WiFi channel to use
 * @param max_connections Maximum number of client connections
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or other error codes on
 * failure
 */
esp_err_t wifi_start_ap(const char *ssid, const char *password, uint8_t channel,
                        uint8_t max_connections) {
  if (s_wifi_started) {
    return ESP_OK;
  }

  if (!ssid || strlen(ssid) == 0 || strlen(ssid) > 32) {
    return ESP_ERR_INVALID_ARG;
  }

  const bool has_password = password && strlen(password) > 0;
  if (has_password && strlen(password) < 8) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    return err;
  }

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_config_t wifi_config = {
      .ap =
          {
              .channel = channel,
              .max_connection = max_connections,
              .authmode = has_password ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN,
              .pmf_cfg =
                  {
                      .required = false,
                  },
          },
  };

  strlcpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
  wifi_config.ap.ssid_len = strlen(ssid);

  if (has_password) {
    strlcpy((char *)wifi_config.ap.password, password,
            sizeof(wifi_config.ap.password));
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  s_wifi_started = true;
  LOGI("WiFi AP started: SSID=%s channel=%u", ssid, channel);
  return ESP_OK;
}

/**
 * @brief Stop WiFi Access Point (AP) mode.
 *
 * Deinitializes and stops the WiFi AP.
 */
void wifi_stop_ap(void) {
  if (!s_wifi_started) {
    return;
  }

  esp_wifi_stop();
  esp_wifi_deinit();
  s_wifi_started = false;
  LOGI("WiFi AP stopped");
}


#define LOG_TAG "MAC" ///< "MAC" log tag for this file

#include "mac.h"
#include "esp_mac.h"
#include "logger.h"

void get_mac(uint8_t *mac, config_connection_t connection) {
  esp_mac_type_t mac_type;

  switch (connection) {
  case APP_CONFIG_CONN_WIFI_AP:
    mac_type = ESP_MAC_WIFI_SOFTAP;
    break;
  case APP_CONFIG_CONN_WIFI_STA:
    mac_type = ESP_MAC_WIFI_STA;
    break;
  case APP_CONFIG_CONN_ETHERNET:
    mac_type = ESP_MAC_ETH;
    break;
  default:
    LOGW("Unknown connection type %d, defaulting to WiFi AP MAC",
         (int)connection);
    mac_type = ESP_MAC_WIFI_SOFTAP;
    break;
  }

  esp_read_mac(mac, mac_type);
}

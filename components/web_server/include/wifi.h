#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

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
                        uint8_t max_connections);

/**
 * @brief Stop WiFi Access Point (AP) mode.
 *
 * Deinitializes and stops the WiFi AP.
 */
void wifi_stop_ap(void);

#ifdef __cplusplus
}
#endif

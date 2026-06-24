/**
 * @file network.h
 * @brief Network management for WiFi AP and STA modes
 */

#pragma once

#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Base event for all network-related events
 */
ESP_EVENT_DECLARE_BASE(NETWORK_EVENT);

/**
 * @brief Network event IDs
 * These events are posted to the NETWORK_EVENT event loop when the network
 * status changes.
 */
typedef enum {
  NETWORK_EVENT_READY, ///< Network is ready (e.g. AP started or STA connected)
  NETWORK_EVENT_DISCONNECTED,     ///< Network got disconnected (e.g. STA
                                  ///< disconnected or AP stopped)
  NETWORK_EVENT_CONNECTION_FAILED ///< Network connection failed (e.g. STA
                                  ///< failed to connect)
} network_event_id_t;

/**
 * @brief Initialize the network stack
 * @return
 * - `ESP_OK` on success
 *
 * - `ESP_ERR_INVALID_STATE` if the network stack is already initialized
 *
 * - `ESP_ERR_NO_MEM` if there was a memory allocation failure
 *
 * @attention This function is not thread-safe and should be called from a
 * single task during application startup.
 *
 * @note If there is a severe failure during initialization, the function may
 * abort the program.
 *
 * This function must be called before any other network functions.
 * It sets up the necessary event handlers and initializes the WiFi driver.
 */
esp_err_t network_init(void);

/**
 * @brief Start the WiFi access point
 * @param ssid The SSID of the access point, 32 characters max
 * @param password The password of the access point, 63 characters max
 * (optional, can be `NULL` or empty for open AP)
 * @return
 * - `ESP_OK` on success
 *
 * - `ESP_ERR_INVALID_STATE` if the network stack is not initialized or already
 * connected
 *
 * - an other appropiate error code if there was a failure starting the AP
 */
esp_err_t network_start_ap(const char *ssid, const char *password);

/**
 * @brief Start the WiFi station
 * @param ssid The SSID of the network to connect to, 32 characters max
 * @param password The password of the network to connect to, 63 characters max
 * (optional, can be `NULL` or empty for open networks)
 * @return
 * - `ESP_OK` on success
 *
 * - `ESP_ERR_INVALID_STATE` if the network stack is not initialized or already
 * connected
 *
 * - an other appropiate error code if there was a failure starting the station
 */
esp_err_t network_start_sta(const char *ssid, const char *password);

/**
 * @brief Stop the WiFi interface, works for both AP and STA mode
 * @return
 * - `ESP_OK` on success
 *
 * - `ESP_ERR_INVALID_STATE` if the network stack is not initialized
 *
 * - an other appropiate error code if there was a failure stopping the WiFi
 */
esp_err_t network_stop_wifi(void);

#ifdef __cplusplus
}
#endif

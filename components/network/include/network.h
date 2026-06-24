/**
 * @file network.h
 * @brief Network management for WiFi AP and STA modes
 *
 * This component provides an abstraction layer for managing WiFi connections in
 * both Access Point (AP) and Station (STA) modes. It handles initialization,
 * connection management, and event handling for network state changes.
 *
 * The caller can subscribe to the `NETWORK_EVENT` event base to receive
 * notifications about network state changes, such as when the network is ready,
 * disconnected, or when a connection attempt fails.
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
  /**
   * @brief Network is ready
   *
   * This event is posted when the network is ready for use, such as when the
   * WiFi AP has started or the STA has successfully connected and obtained an
   * IP address.
   *
   * @note Event data: `esp_netif_ip_info_t *` Pointer to the IP information
   * structure
   */
  NETWORK_EVENT_READY,

  /**
   * @brief Network got disconnected
   *
   * This event is posted when the network connection is lost, such as when the
   * STA disconnects or the AP is stopped.
   *
   * @note Event data: `NULL` No payload
   */
  NETWORK_EVENT_DISCONNECTED,

  /**
   * @brief Network connection failed
   *
   * This event is posted when a connection attempt fails, such as when the STA
   * fails to connect to the configured AP.
   *
   * @note Event data: `NULL` No payload
   */
  NETWORK_EVENT_CONNECTION_FAILED
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

/**
 * @file config.h
 * @brief Thread-safe configuration management component utilizing NVS.
 * Provides an abstraction layer to read, write, and validate application
 * settings without exposing internal storage structures.
 */

#pragma once

#include "esp_err.h"
#include "esp_wifi_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- System Constants --- */

#define APP_CONFIG_DMX_PORT_COUNT                                              \
  2 /**< Number of physical DMX ports on the device */

#define APP_CONFIG_WIFI_SSID_MAX_LEN                                           \
  32 /**< Maximum length of Wi-Fi SSID including null-terminator */
#define APP_CONFIG_WIFI_PASS_MAX_LEN                                           \
  64 /**< Maximum length of Wi-Fi password including null-terminator */

#define APP_CONFIG_INVALID_UNIVERSE                                            \
  0xFFFF /**< Error/Invalid indicator for DMX universe */

/**
 * @brief IP assignment method configurations.
 */
typedef enum {
  APP_CONFIG_IP_STATIC = 0, /**< Use a static IP address config */
  APP_CONFIG_IP_DHCP        /**< Obtain IP address via DHCP */
} config_ip_method_t;

/**
 * @brief Network connection medium types.
 */
typedef enum {
  APP_CONFIG_CONN_WIFI_AP = 0, /**< Act as a Wi-Fi Access Point */
  APP_CONFIG_CONN_WIFI_STA, /**< Connect to an existing Wi-Fi network (Station)
                             */
  APP_CONFIG_CONN_ETHERNET  /**< Wired Ethernet connection */
} config_connection_t;

/**
 * @brief Data direction for dmx-port.
 */
typedef enum {
  APP_CONFIG_DIR_OUTPUT = 0, /**< Port acts as an output */
  APP_CONFIG_DIR_INPUT       /**< Port acts as an input */
} config_direction_t;

/**
 * @name Compile-time Factory Defaults
 * @{
 */
#define APP_CONFIG_DEFAULT_CONNECTION                                          \
  APP_CONFIG_CONN_WIFI_AP /**< Default connection mode */
#define APP_CONFIG_DEFAULT_IP_METHOD                                           \
  APP_CONFIG_IP_DHCP /**< Default IP assignment */
#define APP_CONFIG_DEFAULT_LED_BRIGHTNESS                                      \
  128 /**< Default status LED brightness (0-255) */

#define APP_CONFIG_DEFAULT_STA_SSID "" /**< Default STA SSID (empty) */
#define APP_CONFIG_DEFAULT_STA_PASSWORD                                        \
  "" /**< Default STA Password (empty)                                         \
      */

#define APP_CONFIG_DEFAULT_AP_PASSWORD                                         \
  "ChaosDMX" /**< Factory default AP password */
#define APP_CONFIG_DEFAULT_AP_SSID_PREFIX                                      \
  "ChaosDMX" /**< Prefix for runtime generated AP SSID */

#define APP_CONFIG_DEFAULT_DMX_DIR                                             \
  APP_CONFIG_DIR_OUTPUT /**< Fallback direction for all DMX ports */
#define APP_CONFIG_DEFAULT_START_UNIVERSE                                      \
  1 /**< First port starts at universe X, increments per port */
/** @} */

/* --- Lifecycle Functions --- */

/**
 * @brief Initializes the configuration component.
 *
 * Sets up internal mutexes and loads the persisted configuration blob from NVS.
 *
 * @note The NVS flash subsystem must be initialized (via `nvs_flash_init()`)
 * before calling this function.
 *
 * @return
 *  - ESP_OK on success.
 *  - ESP_ERR_NO_MEM if mutex creation failed.
 *  - Other NVS-related error codes if loading fails.
 */
esp_err_t config_init(void);

/**
 * @brief Resets all configuration settings back to factory defaults.
 * @note This stages the defaults in RAM. A call to config_save() is
 * required to permanently write them to flash.
 * @return
 * - ESP_OK on success
 * - ESP_FAIL if memory or initialization state is invalid
 */
esp_err_t config_reset_defaults(void);

/**
 * @brief Flushes all staged RAM changes permanently to the non-volatile
 * storage.
 * @note To prevent flash wear, this function returns early with ESP_OK if no
 * settings were mutated since the last call.
 * @return
 * - ESP_OK on success or if no write was required
 * - ESP_ERR_NVS_NOT_INITIALIZED if NVS system is down
 * - Other flash driver error codes on write failures
 */
esp_err_t config_save(void);

/* --- System Settings Get/Set --- */

/**
 * @brief Gets the current network connection type.
 * @return Current connection mode as #config_connection_t.
 */
config_connection_t config_get_connection(void);

/**
 * @brief Sets the network connection type in RAM.
 * @param[in] conn New connection mode to apply.
 * @return true if the value was modified, false if it was identical or invalid.
 */
bool config_set_connection(config_connection_t conn);

/**
 * @brief Gets the current IP allocation method.
 * @return Current IP method as #config_ip_method_t.
 */
config_ip_method_t config_get_ip_method(void);

/**
 * @brief Sets the IP assignment method in RAM.
 * @param[in] method New IP allocation method to apply.
 * @return true if the value was modified, false if identical or invalid.
 */
bool config_set_ip_method(config_ip_method_t method);

/**
 * @brief Gets the current status LED brightness level.
 * @return Brightness value ranging from 0 to 255.
 */
uint8_t config_get_led_brightness(void);

/**
 * @brief Sets the status LED brightness level in RAM.
 * @param[in] brightness Desired brightness (0 - 255).
 * @return true if value was updated, false if invalid (out of bounds) or
 * unchanged.
 */
bool config_set_led_brightness(uint8_t brightness);

/* --- DMX Port Settings Get/Set --- */

/**
 * @brief Gets the configured DMX universe for a specific port.
 * @param[in] port_index Index of the port (0 to APP_CONFIG_DMX_PORT_COUNT - 1).
 * @return Universe number (0-32768), or APP_CONFIG_INVALID_UNIVERSE if
 * port_index is invalid.
 */
uint16_t config_get_dmx_universe(uint8_t port_index);

/**
 * @brief Sets the DMX universe for a specific port in RAM.
 * @param[in] port_index Index of the port (0 to APP_CONFIG_DMX_PORT_COUNT - 1).
 * @param[in] universe DMX Universe number (typically 0 - 32768).
 * @return true if updated, false if index invalid or unchanged.
 */
bool config_set_dmx_universe(uint8_t port_index, uint16_t universe);

/**
 * @brief Gets the data direction for a specific port.
 * @param[in] port_index Index of the port (0 to APP_CONFIG_DMX_PORT_COUNT - 1).
 * @return Configuration direction (Defaults to OUTPUT if index invalid).
 */
config_direction_t config_get_dmx_direction(uint8_t port_index);

/**
 * @brief Sets the data direction for a specific port in RAM.
 * @param[in] port_index Index of the port (0 to APP_CONFIG_DMX_PORT_COUNT - 1).
 * @param[in] direction New direction (Input/Output).
 * @return true if updated, false if index invalid or unchanged.
 */
bool config_set_dmx_direction(uint8_t port_index, config_direction_t direction);

/**
 * @brief Gets the Wi-Fi Station mode configuration.
 * @param[out] dest Pointer to a wifi_config_t struct to receive the data.
 */
void config_get_wifi_sta_config(wifi_config_t *dest);

/**
 * @brief Sets the Wi-Fi Station mode configuration in RAM.
 * @param[in] src Pointer to a wifi_config_t struct containing the new config.
 * @return true if updated, false if invalid or unchanged.
 */
bool config_set_wifi_sta_config(const wifi_config_t *src);

/**
 * @brief Gets the Wi-Fi Access Point mode configuration.
 * @param[out] dest Pointer to a wifi_config_t struct to receive the data.
 */
void config_get_wifi_ap_config(wifi_config_t *dest);

/**
 * @brief Sets the Wi-Fi Access Point mode configuration in RAM.
 * @param[in] src Pointer to a wifi_config_t struct containing the new config.
 * @return true if updated, false if invalid or unchanged.
 */
bool config_set_wifi_ap_config(const wifi_config_t *src);


#ifdef __cplusplus
}
#endif

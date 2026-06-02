/**
 * @file config.h
 * @brief Configuration management component utilizing NVS.
 * Provides an abstraction layer to read, write, and validate application
 * settings without exposing internal storage structures.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- System Constants --- */

#define CONFIG_DMX_PORT_COUNT                                                  \
  2 /**< Number of physical DMX ports on the device */

#define CONFIG_WIFI_SSID_MAX_LEN                                               \
  32 /**< Maximum length of Wi-Fi SSID including null-terminator */
#define CONFIG_WIFI_PASS_MAX_LEN                                               \
  64 /**< Maximum length of Wi-Fi password including null-terminator */

#define CONFIG_INVALID_UNIVERSE                                                \
  0xFFFF /**< Error/Invalid indicator for DMX universe */

/**
 * @brief IP assignment method configurations.
 */
typedef enum {
  CONFIG_IP_STATIC = 0, /**< Use a static IP address config */
  CONFIG_IP_DHCP        /**< Obtain IP address via DHCP */
} config_ip_method_t;

/**
 * @brief Network connection medium types.
 */
typedef enum {
  CONFIG_CONN_WIFI_AP = 0, /**< Act as a Wi-Fi Access Point */
  CONFIG_CONN_WIFI_STA, /**< Connect to an existing Wi-Fi network (Station) */
  CONFIG_CONN_ETHERNET  /**< Wired Ethernet connection */
} config_connection_t;

/**
 * @brief Data direction for dmx-port.
 */
typedef enum {
  CONFIG_DIR_OUTPUT = 0, /**< Port acts as an output */
  CONFIG_DIR_INPUT       /**< Port acts as an input */
} config_direction_t;

/**
 * @name Compile-time Factory Defaults
 * @{
 */
#define CONFIG_DEFAULT_CONNECTION                                              \
  CONFIG_CONN_WIFI_AP                           /**< Default connection mode */
#define CONFIG_DEFAULT_IP_METHOD CONFIG_IP_DHCP /**< Default IP assignment */
#define CONFIG_DEFAULT_LED_BRIGHTNESS                                          \
  50 /**< Default status LED brightness (0-100%) */

#define CONFIG_DEFAULT_STA_SSID ""     /**< Default STA SSID (empty) */
#define CONFIG_DEFAULT_STA_PASSWORD "" /**< Default STA Password (empty) */

#define CONFIG_DEFAULT_AP_PASSWORD                                             \
  "ChaosDMX" /**< Factory default AP password */
#define CONFIG_DEFAULT_AP_SSID_PREFIX                                          \
  "ChaosDMX" /**< Prefix for runtime generated AP SSID */

#define CONFIG_DEFAULT_DMX_DIR                                                 \
  CONFIG_DIR_OUTPUT /**< Fallback direction for all DMX ports */
#define CONFIG_DEFAULT_START_UNIVERSE                                          \
  1 /**< First port starts at universe X, increments per port */
/** @} */

/* --- Lifecycle Functions --- */

/**
 * @brief Initializes the configuration component.
 * Sets up internal mutexes and initializes the underlying NVS flash storage.
 * If no configuration is present in flash, default values are automatically
 * loaded.
 * @return
 * - ESP_OK on success
 * - ESP_ERR_NO_MEM if mutex creation failed
 * - Other underlying flash error codes from nvs_flash_init
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
 * @return Brightness value ranging from 0 to 100.
 */
uint8_t config_get_led_brightness(void);

/**
 * @brief Sets the status LED brightness level in RAM.
 * @param[in] brightness Desired brightness percentage (0 - 100).
 * @return true if value was updated, false if invalid (out of bounds) or
 * unchanged.
 */
bool config_set_led_brightness(uint8_t brightness);

/* --- DMX Port Settings Get/Set --- */

/**
 * @brief Gets the configured DMX universe for a specific port.
 * @param[in] port_index Index of the port (0 to CONFIG_DMX_PORT_COUNT - 1).
 * @return Universe number (0-32768), or #CONFIG_INVALID_UNIVERSE if port_index
 * is invalid.
 */
uint16_t config_get_dmx_universe(uint8_t port_index);

/**
 * @brief Sets the DMX universe for a specific port in RAM.
 * @param[in] port_index Index of the port (0 to CONFIG_DMX_PORT_COUNT - 1).
 * @param[in] universe DMX Universe number (typically 0 - 32768).
 * @return true if updated, false if index invalid or unchanged.
 */
bool config_set_dmx_universe(uint8_t port_index, uint16_t universe);

/**
 * @brief Gets the data direction for a specific port.
 * @param[in] port_index Index of the port (0 to CONFIG_DMX_PORT_COUNT - 1).
 * @return Configuration direction (Defaults to OUTPUT if index invalid).
 */
config_direction_t config_get_dmx_direction(uint8_t port_index);

/**
 * @brief Sets the data direction for a specific port in RAM.
 * @param[in] port_index Index of the port (0 to CONFIG_DMX_PORT_COUNT - 1).
 * @param[in] direction New direction (Input/Output).
 * @return true if updated, false if index invalid or unchanged.
 */
bool config_set_dmx_direction(uint8_t port_index, config_direction_t direction);

/* --- Wi-Fi Credentials Get/Set --- */

/**
 * @brief Safely retrieves the currently configured Wi-Fi Station SSID.
 * @param[out] dest Pointer to the destination buffer where the SSID string will
 * be copied.
 * @param[in] max_len Maximum capacity of the destination buffer. Recommended:
 * #CONFIG_WIFI_SSID_MAX_LEN.
 */
void config_get_wifi_sta_ssid(char *dest, size_t max_len);

/**
 * @brief Safely retrieves the currently configured Wi-Fi Station password.
 * @param[out] dest Pointer to the destination buffer where the password string
 * will be copied.
 * @param[in] max_len Maximum capacity of the destination buffer. Recommended:
 * #CONFIG_WIFI_PASS_MAX_LEN.
 */
void config_get_wifi_sta_password(char *dest, size_t max_len);

/**
 * @brief Updates Wi-Fi Station credentials in RAM.
 * @param[in] ssid Pointer to the new null-terminated SSID string.
 * @param[in] password Pointer to the new null-terminated password string.
 * @return true if credentials were changed and valid, false otherwise.
 */
bool config_set_wifi_sta_creds(const char *ssid, const char *password);

/**
 * @brief Safely retrieves the currently configured Wi-Fi Access Point SSID.
 * @param[out] dest Pointer to the destination buffer where the AP SSID string
 * will be copied.
 * @param[in] max_len Maximum capacity of the destination buffer. Recommended:
 * #CONFIG_WIFI_SSID_MAX_LEN.
 */
void config_get_wifi_ap_ssid(char *dest, size_t max_len);

/**
 * @brief Safely retrieves the currently configured Wi-Fi Access Point password.
 * @param[out] dest Pointer to the destination buffer where the AP password
 * string will be copied.
 * @param[in] max_len Maximum capacity of the destination buffer. Recommended:
 * #CONFIG_WIFI_PASS_MAX_LEN.
 */
void config_get_wifi_ap_password(char *dest, size_t max_len);

/**
 * @brief Updates Wi-Fi Access Point credentials in RAM.
 * @param[in] ssid Pointer to the new null-terminated SSID string.
 * @param[in] password Pointer to the new null-terminated password string.
 * @return true if credentials were changed and valid, false otherwise.
 */
bool config_set_wifi_ap_creds(const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif

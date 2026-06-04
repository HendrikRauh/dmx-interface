/**
 * @file config.h
 * @brief Thread-safe configuration component utilizing NVS for ESP32.
 *
 * This header defines the public application configuration interface, including
 * Wi-Fi credentials, DMX port settings, and system-wide parameters like
 * LED brightness and button actions.
 */

#pragma once

#include "esp_err.h"
#include "esp_wifi.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Total number of physical DMX ports supported by the hardware.
 */
#define APP_CONFIG_DMX_PORT_COUNT 1

/**
 * @brief Error/Invalid indicator for DMX universe.
 */
#define APP_CONFIG_INVALID_UNIVERSE 0xFFFF

/**
 * @brief Supported button event types that can be configured in the system.
 */
typedef enum {
  APP_BUTTON_EVENT_SINGLE_CLICK,   /**< Triggered on a single short press */
  APP_BUTTON_EVENT_DOUBLE_CLICK,   /**< Triggered on a rapid double press */
  APP_BUTTON_EVENT_MULTIPLE_CLICK, /**< Triggered on multiple rapid presses */
  APP_BUTTON_EVENT_LONG_HOLD, /**< Triggered once long-press duration is reached
                               */
  APP_BUTTON_EVENT_MAX
} app_button_event_t;

/**
 * @brief Actions that can be dynamically assigned to button events.
 */
typedef enum {
  APP_BUTTON_ACTION_NONE = 0,   /**< Do nothing */
  APP_BUTTON_ACTION_TOGGLE_LED, /**< Toggle status LEDs or change brightness */
  APP_BUTTON_ACTION_REBOOT,     /**< Restart only the Web Server / Wi-Fi */
  APP_BUTTON_ACTION_MAX
} config_button_action_t;

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

#define APP_CONFIG_DEFAULT_SINGLE_CLICK_ACT                                    \
  APP_BUTTON_ACTION_TOGGLE_LED /**< Default action for single click */
#define APP_CONFIG_DEFAULT_DOUBLE_CLICK_ACT                                    \
  APP_BUTTON_ACTION_NONE /**< Default action for double click */
#define APP_CONFIG_DEFAULT_MULTI_CLICK_ACT                                     \
  APP_BUTTON_ACTION_REBOOT /**< Default action for multi click */
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

/**
 * @brief Retrieves the action assigned to a specific button event.
 * @param[in] event The button event to query.
 * @return The assigned @ref config_button_action_t.
 */
config_button_action_t config_get_button_action(app_button_event_t event);

/**
 * @brief Assigns a new action to a specific button event in RAM.
 * @param[in] event The button event to modify.
 * @param[in] action The @ref config_button_action_t to assign.
 * @return true if updated, false if uninitialized, unchanged, or invalid.
 */
bool config_set_button_action(app_button_event_t event,
                              config_button_action_t action);

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

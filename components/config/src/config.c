/**
 * @file config.c
 * @brief Thread-safe configuration component utilizing NVS for ESP32.
 * Implements the private storage structures, mutex protection mechanisms,
 * and input validation logic defined in config.h.
 */

#define LOG_TAG "CONFIG" ///< "CONFIG" log tag for this file

#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "led.h"
#include "logger.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief NVS storage namespace for configuration data.
 */
#define NVS_NAMESPACE "app_config"
#define NVS_BLOB_KEY                                                           \
  "config_blob" ///< Key under which the entire config blob is stored in NVS

/* --- Magic & Version Constants --- */
#define APP_CONFIG_MAGIC 0x43444D58 /**< ASCII for 'CDMX' */
#define APP_CONFIG_VERSION 3 /**< Incremented when struct layout changes */

/**
 * @brief WIFI credentials structure for both Station and Access Point modes.
 */
typedef struct {
  char ssid[32];
  char password[64];
} app_wifi_creds_t;

/**
 * @brief Internal configuration storage layout.
 * Mirrors the private runtime configuration context state in RAM.
 */
typedef struct {
  uint32_t magic;
  uint32_t version;
  config_connection_t connection;
  config_ip_method_t ip_method;
  uint16_t dmx_universes[APP_CONFIG_DMX_PORT_COUNT];
  config_direction_t dmx_directions[APP_CONFIG_DMX_PORT_COUNT];
  app_wifi_creds_t wifi_sta;
  app_wifi_creds_t wifi_ap;
  uint8_t led_brightness;

  uint8_t btn_action_single; /**< Assigned config_button_action_t for single
                                click */
  uint8_t btn_action_double; /**< Assigned config_button_action_t for double
                                click */
  uint8_t btn_action_multi;  /**< Assigned config_button_action_t for multiple
                                click */
} __attribute__((aligned(4))) config_storage_t;

/**
 * @brief Private runtime memory footprint of the active configuration.
 */
static config_storage_t s_config;

/**
 * @brief Mutex guard ensuring thread-safe operations on the shared
 * configuration context.
 */
static SemaphoreHandle_t s_config_mutex = NULL;

/**
 * @brief Tracks if the configuration component has been successfully
 * initialized.
 */
static bool s_is_initialized = false;

/**
 * @brief Set to true if any RAM values diverge from the persisted flash storage
 * state.
 */
static bool s_is_dirty = false;

/* --- Thread-Safety Helpers --- */
#define LOCK()                                                                 \
  xSemaphoreTake(                                                              \
      s_config_mutex,                                                          \
      portMAX_DELAY) ///< Locks the configuration mutex for exclusive access
#define UNLOCK()                                                               \
  xSemaphoreGive(                                                              \
      s_config_mutex) ///< Unlocks the configuration mutex after access

/**
 * @brief Restores all configuration fields to their factory-defined values in
 * RAM.
 *
 * Sets the magic number, version, and all functional parameters (Wi-Fi, DMX,
 * LED). Marks the configuration as dirty to trigger a save on next
 * config_save() call.
 */
static void load_factory_defaults(void) {
  s_config.magic = APP_CONFIG_MAGIC;
  s_config.version = APP_CONFIG_VERSION;
  s_config.connection = APP_CONFIG_DEFAULT_CONNECTION;
  s_config.ip_method = APP_CONFIG_DEFAULT_IP_METHOD;
  s_config.led_brightness = APP_CONFIG_DEFAULT_LED_BRIGHTNESS;

  s_config.btn_action_single = APP_CONFIG_DEFAULT_SINGLE_CLICK_ACT;
  s_config.btn_action_double = APP_CONFIG_DEFAULT_DOUBLE_CLICK_ACT;
  s_config.btn_action_multi = APP_CONFIG_DEFAULT_MULTI_CLICK_ACT;

  for (int i = 0; i < APP_CONFIG_DMX_PORT_COUNT; i++) {
    s_config.dmx_universes[i] = APP_CONFIG_DEFAULT_START_UNIVERSE + i;
    s_config.dmx_directions[i] = APP_CONFIG_DEFAULT_DMX_DIR;
  }

  memset(&s_config.wifi_sta, 0, sizeof(app_wifi_creds_t));
  memset(&s_config.wifi_ap, 0, sizeof(app_wifi_creds_t));

  snprintf(s_config.wifi_sta.ssid, sizeof(s_config.wifi_sta.ssid), "%s",
           APP_CONFIG_DEFAULT_STA_SSID);
  snprintf(s_config.wifi_sta.password, sizeof(s_config.wifi_sta.password), "%s",
           APP_CONFIG_DEFAULT_STA_PASSWORD);

  snprintf(s_config.wifi_ap.ssid, sizeof(s_config.wifi_ap.ssid), "%s",
           APP_CONFIG_DEFAULT_AP_SSID_PREFIX);
  snprintf(s_config.wifi_ap.password, sizeof(s_config.wifi_ap.password), "%s",
           APP_CONFIG_DEFAULT_AP_PASSWORD);

  s_is_dirty = true;
}

/* --- Lifecycle Functions --- */

esp_err_t config_init(void) {
  if (s_is_initialized) {
    LOGW("Component already initialized.");
    return ESP_OK;
  }

  s_config_mutex = xSemaphoreCreateMutex();
  if (s_config_mutex == NULL) {
    LOGE("Failed to create configuration mutex.");
    return ESP_ERR_NO_MEM;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    if (err == ESP_ERR_NVS_NOT_FOUND) {
      LOGW("NVS namespace missing. Staging factory defaults.");
      load_factory_defaults();
    } else {
      LOGE("Failed to open NVS namespace '%s': %s", NVS_NAMESPACE,
           esp_err_to_name(err));
      vSemaphoreDelete(s_config_mutex);
      s_config_mutex = NULL;
      return err;
    }
  } else {
    size_t size = sizeof(config_storage_t);
    err = nvs_get_blob(handle, NVS_BLOB_KEY, &s_config, &size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
      LOGI("No configuration blob found. Initializing defaults.");
      load_factory_defaults();
    } else if (err != ESP_OK) {
      LOGE("NVS read error: 0x%X. Falling back to defaults.", err);
      load_factory_defaults();
    } else if (size != sizeof(config_storage_t)) {
      LOGW("Configuration size mismatch (%u vs %u). Resetting to defaults.",
           (unsigned int)size, (unsigned int)sizeof(config_storage_t));
      load_factory_defaults();
    } else if (s_config.magic != APP_CONFIG_MAGIC) {
      LOGW("Configuration magic mismatch (0x%08X). Data corrupted. Resetting.",
           (unsigned int)s_config.magic);
      load_factory_defaults();
    } else if (s_config.version != APP_CONFIG_VERSION) {
      LOGW("Configuration version mismatch (%u vs %u). Upgrading/Resetting.",
           (unsigned int)s_config.version, (unsigned int)APP_CONFIG_VERSION);
      load_factory_defaults();
    } else {
      LOGI("Configuration loaded successfully. Magic and Version match.");
      s_is_dirty = false;
    }
    nvs_close(handle);
  }

  s_is_initialized = true;
  return ESP_OK;
}

esp_err_t config_reset_defaults(void) {
  if (!s_is_initialized)
    return ESP_FAIL;
  LOCK();
  load_factory_defaults();
  UNLOCK();
  LOGI("Factory defaults staged in RAM.");
  return ESP_OK;
}

esp_err_t config_save(void) {
  if (!s_is_initialized)
    return ESP_ERR_NVS_NOT_INITIALIZED;

  LOCK();
  if (!s_is_dirty) {
    UNLOCK();
    LOGI("Flash save skipped: No changes detected.");
    return ESP_OK;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    UNLOCK();
    LOGE("Failed to open NVS for saving: 0x%X", err);
    return err;
  }

  err = nvs_set_blob(handle, NVS_BLOB_KEY, &s_config, sizeof(config_storage_t));
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }

  nvs_close(handle);

  if (err == ESP_OK) {
    s_is_dirty = false;
    LOGI("Configuration successfully persisted to NVS.");
  } else {
    LOGE("Failed to write/commit config to NVS: 0x%X", err);
  }

  UNLOCK();
  return err;
}

/* --- System Settings Get/Set --- */

config_connection_t config_get_connection(void) {
  if (!s_is_initialized)
    return APP_CONFIG_DEFAULT_CONNECTION;
  LOCK();
  config_connection_t conn = s_config.connection;
  UNLOCK();
  return conn;
}

bool config_set_connection(config_connection_t conn) {
  LOGI("Setting connection type to %d", (int)conn);
  if (!s_is_initialized || conn > APP_CONFIG_CONN_ETHERNET)
    return false;
  LOCK();
  if (s_config.connection == conn) {
    UNLOCK();
    return false;
  }
  s_config.connection = conn;
  s_is_dirty = true;
  UNLOCK();
  return true;
}

config_ip_method_t config_get_ip_method(void) {
  if (!s_is_initialized)
    return APP_CONFIG_DEFAULT_IP_METHOD;
  LOCK();
  config_ip_method_t method = s_config.ip_method;
  UNLOCK();
  return method;
}

bool config_set_ip_method(config_ip_method_t method) {
  if (!s_is_initialized || method > APP_CONFIG_IP_DHCP)
    return false;
  LOCK();
  if (s_config.ip_method == method) {
    UNLOCK();
    return false;
  }
  s_config.ip_method = method;
  s_is_dirty = true;
  UNLOCK();
  return true;
}

uint8_t config_get_led_brightness(void) {
  if (!s_is_initialized)
    return APP_CONFIG_DEFAULT_LED_BRIGHTNESS;
  LOCK();
  uint8_t brightness = s_config.led_brightness;
  UNLOCK();
  return brightness;
}

bool config_set_led_brightness(uint8_t brightness) {
  if (!s_is_initialized)
    return false;
  LOGI("Setting LED brightness to %u", (unsigned int)brightness);
  LOCK();
  if (s_config.led_brightness == brightness) {
    UNLOCK();
    return false;
  }
  s_config.led_brightness = brightness;
  s_is_dirty = true;
  UNLOCK();

  led_set_brightness(brightness);
  return true;
}

/* --- DMX Port Settings Get/Set --- */

uint16_t config_get_dmx_universe(uint8_t port_index) {
  if (!s_is_initialized || port_index >= APP_CONFIG_DMX_PORT_COUNT)
    return APP_CONFIG_INVALID_UNIVERSE;
  LOCK();
  uint16_t universe = s_config.dmx_universes[port_index];
  UNLOCK();
  return universe;
}

bool config_set_dmx_universe(uint8_t port_index, uint16_t universe) {
  if (!s_is_initialized || port_index >= APP_CONFIG_DMX_PORT_COUNT ||
      universe == APP_CONFIG_INVALID_UNIVERSE)
    return false;
  LOCK();
  if (s_config.dmx_universes[port_index] == universe) {
    UNLOCK();
    return false;
  }
  s_config.dmx_universes[port_index] = universe;
  s_is_dirty = true;
  UNLOCK();
  return true;
}

config_direction_t config_get_dmx_direction(uint8_t port_index) {
  if (!s_is_initialized || port_index >= APP_CONFIG_DMX_PORT_COUNT)
    return APP_CONFIG_DEFAULT_DMX_DIR;
  LOCK();
  config_direction_t dir = s_config.dmx_directions[port_index];
  UNLOCK();
  return dir;
}

bool config_set_dmx_direction(uint8_t port_index,
                              config_direction_t direction) {
  if (!s_is_initialized || port_index >= APP_CONFIG_DMX_PORT_COUNT ||
      direction > APP_CONFIG_DIR_INPUT)
    return false;
  LOCK();
  if (s_config.dmx_directions[port_index] == direction) {
    UNLOCK();
    return false;
  }
  s_config.dmx_directions[port_index] = direction;
  s_is_dirty = true;
  UNLOCK();
  return true;
}

void config_get_wifi_sta_config(wifi_config_t *dest) {
  if (!s_is_initialized || !dest)
    return;
  LOCK();
  memset(dest, 0, sizeof(wifi_config_t));
  memcpy(dest->sta.ssid, s_config.wifi_sta.ssid,
         sizeof(s_config.wifi_sta.ssid));
  memcpy(dest->sta.password, s_config.wifi_sta.password,
         sizeof(s_config.wifi_sta.password));
  UNLOCK();
}

bool config_set_wifi_sta_config(const wifi_config_t *src) {
  if (!s_is_initialized || !src)
    return false;

  if (strnlen((const char *)src->sta.ssid, 32) == 32 ||
      strnlen((const char *)src->sta.password, 64) == 64) {
    LOGE("Wi-Fi credentials are not null-terminated or too long!");
    return false;
  }
  LOCK();
  if (memcmp(s_config.wifi_sta.ssid, src->sta.ssid,
             sizeof(s_config.wifi_sta.ssid)) == 0 &&
      memcmp(s_config.wifi_sta.password, src->sta.password,
             sizeof(s_config.wifi_sta.password)) == 0) {
    UNLOCK();
    return false;
  }
  memcpy(s_config.wifi_sta.ssid, src->sta.ssid, sizeof(s_config.wifi_sta.ssid));
  memcpy(s_config.wifi_sta.password, src->sta.password,
         sizeof(s_config.wifi_sta.password));
  s_is_dirty = true;
  UNLOCK();
  return true;
}

void config_get_wifi_ap_config(wifi_config_t *dest) {
  if (!s_is_initialized || !dest)
    return;
  LOCK();
  memset(dest, 0, sizeof(wifi_config_t));
  memcpy(dest->ap.ssid, s_config.wifi_ap.ssid, sizeof(s_config.wifi_ap.ssid));
  memcpy(dest->ap.password, s_config.wifi_ap.password,
         sizeof(s_config.wifi_ap.password));
  UNLOCK();
}

bool config_set_wifi_ap_config(const wifi_config_t *src) {
  if (!s_is_initialized || !src)
    return false;

  if (strnlen((const char *)src->ap.ssid, 32) == 32 ||
      strnlen((const char *)src->ap.password, 64) == 64) {
    LOGE("Wi-Fi credentials are not null-terminated or too long!");
    return false;
  }

  LOCK();
  if (memcmp(s_config.wifi_ap.ssid, src->ap.ssid,
             sizeof(s_config.wifi_ap.ssid)) == 0 &&
      memcmp(s_config.wifi_ap.password, src->ap.password,
             sizeof(s_config.wifi_ap.password)) == 0) {
    UNLOCK();
    return false;
  }
  memcpy(s_config.wifi_ap.ssid, src->ap.ssid, sizeof(s_config.wifi_ap.ssid));
  memcpy(s_config.wifi_ap.password, src->ap.password,
         sizeof(s_config.wifi_ap.password));
  s_is_dirty = true;
  UNLOCK();
  return true;
}

/**
 * @brief Retrieves the action assigned to a specific button event.
 * @param[in] event The button event to query.
 * @return The @ref config_button_action_t assigned to the event, or
 * APP_BUTTON_ACTION_NONE if uninitialized or invalid.
 */
config_button_action_t config_get_button_action(app_button_event_t event) {
  if (!s_is_initialized)
    return APP_BUTTON_ACTION_NONE;
  LOCK();
  config_button_action_t act = APP_BUTTON_ACTION_NONE;
  if (event == APP_BUTTON_EVENT_SINGLE_CLICK)
    act = (config_button_action_t)s_config.btn_action_single;
  if (event == APP_BUTTON_EVENT_DOUBLE_CLICK)
    act = (config_button_action_t)s_config.btn_action_double;
  if (event == APP_BUTTON_EVENT_MULTIPLE_CLICK)
    act = (config_button_action_t)s_config.btn_action_multi;
  UNLOCK();
  return act;
}

/**
 * @brief Assigns a new action to a specific button event in RAM.
 * @param[in] event The button event to modify.
 * @param[in] action The @ref config_button_action_t to assign.
 * @return true if the configuration was updated, false if uninitialized,
 * unchanged, or arguments were invalid.
 */
bool config_set_button_action(app_button_event_t event,
                              config_button_action_t action) {
  if (!s_is_initialized || action >= APP_BUTTON_ACTION_MAX)
    return false;
  LOCK();
  uint8_t *target = NULL;
  if (event == APP_BUTTON_EVENT_SINGLE_CLICK)
    target = &s_config.btn_action_single;
  if (event == APP_BUTTON_EVENT_DOUBLE_CLICK)
    target = &s_config.btn_action_double;
  if (event == APP_BUTTON_EVENT_MULTIPLE_CLICK)
    target = &s_config.btn_action_multi;

  if (target == NULL || *target == (uint8_t)action) {
    UNLOCK();
    return false;
  }

  *target = (uint8_t)action;
  s_is_dirty = true;
  UNLOCK();
  return true;
}

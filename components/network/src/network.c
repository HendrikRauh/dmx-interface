/**
 * @file network.c
 * @brief Implementation of network management for WiFi AP and STA modes
 */

#define LOG_TAG "NETWORK" ///< Log tag for this file

#include "network.h"

#include "config.h"
#include "esp_check.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/semphr.h"
#include "logger.h"

/**
 * @brief Base event for all network-related events
 */
ESP_EVENT_DEFINE_BASE(NETWORK_EVENT);

/**
 * @brief Mutex to ensure thread-safe access to network state and operations.
 */
static SemaphoreHandle_t network_mutex = NULL;

/**
 * @brief Flag indicating whether the network stack has been initialized.
 */
static bool network_initialized = false;

/**
 * @brief Flag indicating whether a network connection (AP or STA) has been
 * started.
 */
static bool connection_started = false;

/**
 * @brief Flag indicating whether the STA mode should attempt to maintain a
 * connection.
 *
 * This is set to false if the user explicitly disconnects or if
 * there is an authentication failure, to prevent infinite reconnect loops in
 * those cases.
 */
static volatile bool network_sta_wants_connection = false;

/**
 * @brief The currently active connection type, if any. Valid only if
 * `connection_started` is true.
 */
static config_connection_t connection_type;

/**
 * @brief Maximum number of retry attempts for WiFi STA connection before giving
 * up
 */
#define MAX_RETRY_ATTEMPTS 10

/**
 * @brief Delay between WiFi STA reconnect attempts in milliseconds
 */
#define RECONNECT_DELAY_MS 5000

/**
 * @brief Timer handle for scheduling WiFi STA reconnect attempts
 */
static TimerHandle_t reconnect_timer = NULL;

/**
 * @brief Helper macro for locking the network mutex
 */
#define LOCK_NETWORK() xSemaphoreTake(network_mutex, portMAX_DELAY)

/**
 * @brief Helper macro for unlocking the network mutex
 */
#define UNLOCK_NETWORK() xSemaphoreGive(network_mutex)

/**
 * @brief Callback function for the WiFi reconnect timer
 */
static void on_reconnect_timer(TimerHandle_t timer) {
  if (network_sta_wants_connection) {
    LOGI("Attempting to reconnect to WiFi...");
    esp_wifi_connect();
  }
}

/**
 * @brief Event handler for WiFi and IP events to manage network state changes
 * and post appropriate NETWORK_EVENTs.
 * @param arg User-defined argument (not used)
 * @param event_base The base of the event (e.g. WIFI_EVENT or IP_EVENT)
 * @param event_id The specific event ID within the base
 * @param event_data Pointer to event-specific data (e.g. IP address info on
 * IP_EVENT_STA_GOT_IP)
 */
static void network_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data) {
  static uint8_t retry_count = 0;

  if (event_base == WIFI_EVENT) {
    switch (event_id) {
    case WIFI_EVENT_AP_START:
      esp_event_post(NETWORK_EVENT, NETWORK_EVENT_READY, NULL, 0, 0);
      break;
    case WIFI_EVENT_STA_START:
      retry_count = 0;
      esp_wifi_connect();
      break;
    case WIFI_EVENT_STA_DISCONNECTED: {
      wifi_event_sta_disconnected_t *disconnected_event =
          (wifi_event_sta_disconnected_t *)event_data;
      LOGW("WiFi station disconnected. Reason code: %d",
           disconnected_event->reason);

      esp_event_post(NETWORK_EVENT, NETWORK_EVENT_DISCONNECTED, NULL, 0, 0);

      if (disconnected_event->reason == WIFI_REASON_AUTH_FAIL) {
        LOGE("Authentication failed, not attempting to reconnect");
        network_sta_wants_connection = false;
      }

      if (network_sta_wants_connection) {
        if (retry_count < MAX_RETRY_ATTEMPTS) {
          retry_count++;
          LOGI("Scheduling reconnect attempt %d/%d in %dms...", retry_count,
               MAX_RETRY_ATTEMPTS, RECONNECT_DELAY_MS);

          if (reconnect_timer) {
            xTimerStart(reconnect_timer, 0);
          }
        } else {
          LOGW("Max retry attempts reached. Giving up.");
          network_sta_wants_connection = false;
        }
      }
      break;
    }
    case WIFI_EVENT_AP_STOP:
    case WIFI_EVENT_STA_STOP:
      esp_event_post(NETWORK_EVENT, NETWORK_EVENT_DISCONNECTED, NULL, 0, 0);
      break;
    default:
      break;
    }
  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      LOGI("Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
      esp_event_post(NETWORK_EVENT, NETWORK_EVENT_READY, NULL, 0, 0);
      retry_count = 0;
    }
  }
}

/**
 * @brief Validates the provided WiFi credentials for SSID and password length
 * requirements.
 * @param ssid The SSID string to validate
 * @param password The password string to validate
 * @return `ESP_OK` if the credentials are valid, otherwise
 * `ESP_ERR_INVALID_ARG`
 *
 * The credential requirements are:
 *
 * - SSID must be non-null, non-empty, and at most 32 characters long
 *
 * - Password is optional (can be null or empty for open networks), but if
 * provided, it must be between 8 and 63 characters long
 */
static esp_err_t validate_credentials(const char *ssid, const char *password) {
  if (!ssid || strlen(ssid) == 0 || strlen(ssid) > 32) {
    return ESP_ERR_INVALID_ARG;
  }

  const bool has_password = password && strlen(password) > 0;
  if (has_password && (strlen(password) < 8 || strlen(password) > 63)) {
    return ESP_ERR_INVALID_ARG;
  }

  return ESP_OK;
}

/**
 * @brief Internal helper function to initialize the WiFi driver with default
 * configuration.
 * @return `ESP_OK` on success, otherwise an appropriate error code from the
 * WiFi initialization process.
 */
static esp_err_t network_init_wifi() {
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), LOG_TAG,
                      "Failed to initialize WiFi");
  return ESP_OK;
}

esp_err_t network_init() {
  if (network_initialized) {
    LOGW("Network is already initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (network_mutex == NULL) {
    network_mutex = xSemaphoreCreateMutex();
    if (network_mutex == NULL) {
      LOGE("Failed to create network mutex");
      return ESP_ERR_NO_MEM;
    }
  }

  if (reconnect_timer == NULL) {
    reconnect_timer =
        xTimerCreate("wifi_reconnect_timer", pdMS_TO_TICKS(RECONNECT_DELAY_MS),
                     pdFALSE, NULL, on_reconnect_timer);
    if (reconnect_timer == NULL) {
      LOGE("Failed to create reconnect timer");
      return ESP_ERR_NO_MEM;
    }
  }

  LOCK_NETWORK();

  ESP_ERROR_CHECK(esp_netif_init());
  esp_err_t err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    LOGE("Failed to create default event loop: %s", esp_err_to_name(err));
    abort();
  }

  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             network_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                             network_event_handler, NULL));


  ESP_ERROR_CHECK(network_init_wifi());

  network_initialized = true;
  UNLOCK_NETWORK();
  return ESP_OK;
}

esp_err_t network_start_ap(const char *ssid, const char *password) {
  LOCK_NETWORK();

  esp_err_t ret = ESP_OK;

  if (!network_initialized) {
    LOGW("Network is not initialized, cannot start WiFi AP");
    ret = ESP_ERR_INVALID_STATE;
    goto cleanup;
  }

  if (connection_started) {
    LOGW("Network is already connected, cannot start WiFi AP");
    ret = ESP_ERR_INVALID_STATE;
    goto cleanup;
  }

  ESP_GOTO_ON_ERROR(validate_credentials(ssid, password), cleanup, LOG_TAG,
                    "Invalid WiFi credentials");

  const bool has_password = password && strlen(password) > 0;
  wifi_config_t wifi_config = {
      .ap =
          {
              .channel = 0, // auto-select channel
              .max_connection = 4,
              .authmode = has_password ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN,
              .pmf_cfg = {.required = false},
          },
  };

  wifi_config.ap.ssid_len = strlen(ssid);
  memcpy(wifi_config.ap.ssid, ssid, wifi_config.ap.ssid_len);
  if (has_password) {
    strlcpy((char *)wifi_config.ap.password, password,
            sizeof(wifi_config.ap.password));
  }

  ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), cleanup, LOG_TAG,
                    "Failed to set WiFi mode to AP");
  ESP_GOTO_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wifi_config), cleanup,
                    LOG_TAG, "Failed to set WiFi AP configuration");

  ESP_GOTO_ON_ERROR(esp_wifi_start(), cleanup, LOG_TAG,
                    "Failed to start WiFi AP");
  LOGI("WiFi AP started: SSID=%s", ssid);

  connection_started = true;
  connection_type = APP_CONFIG_CONN_WIFI_AP;

cleanup:
  UNLOCK_NETWORK();
  return ret;
}

esp_err_t network_start_sta(const char *ssid, const char *password) {
  LOCK_NETWORK();

  esp_err_t ret = ESP_OK;

  if (!network_initialized) {
    LOGW("Network is not initialized, cannot start WiFi STA");
    ret = ESP_ERR_INVALID_STATE;
    goto cleanup;
  }

  if (connection_started) {
    LOGW("Network is already connected, cannot start WiFi STA");
    ret = ESP_ERR_INVALID_STATE;
    goto cleanup;
  }

  ESP_GOTO_ON_ERROR(validate_credentials(ssid, password), cleanup, LOG_TAG,
                    "Invalid WiFi credentials");

  const bool has_password = password && strlen(password) > 0;
  wifi_config_t wifi_config = {
      .sta =
          {
              .threshold.authmode =
                  has_password ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN,
              .pmf_cfg = {.capable = true, .required = false},
          },
  };

  memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));
  if (has_password) {
    strlcpy((char *)wifi_config.sta.password, password,
            sizeof(wifi_config.sta.password));
  }

  ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), cleanup, LOG_TAG,
                    "Failed to set WiFi mode to STA");
  ESP_GOTO_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), cleanup,
                    LOG_TAG, "Failed to set WiFi STA configuration");

  ESP_GOTO_ON_ERROR(esp_wifi_start(), cleanup, LOG_TAG,
                    "Failed to start WiFi STA");
  LOGI("WiFi Station initialized, attempting connection to %s...", ssid);

  connection_started = true;
  network_sta_wants_connection = true;
  connection_type = APP_CONFIG_CONN_WIFI_STA;

cleanup:
  UNLOCK_NETWORK();
  return ret;
}

esp_err_t network_stop_wifi() {
  LOCK_NETWORK();

  esp_err_t ret = ESP_OK;

  if (!connection_started) {
    LOGW("WiFi is not connected, nothing to stop");
    ret = ESP_ERR_INVALID_STATE;
    goto cleanup;
  }

  network_sta_wants_connection = false;

  ESP_GOTO_ON_ERROR(esp_wifi_stop(), cleanup, LOG_TAG, "Failed to stop WiFi");
  connection_started = false;
  LOGI("WiFi stopped");

  ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_NULL), cleanup, LOG_TAG,
                    "Failed to reset WiFi mode to NULL");

cleanup:
  UNLOCK_NETWORK();
  return ret;
}

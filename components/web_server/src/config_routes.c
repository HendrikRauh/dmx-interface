/**
 * @file config_routes.c
 * @brief Implementation of the web server configuration routes
 */

#define LOG_TAG "WEB_CFG" ///< Logging tag for this file

#include <cJSON.h>

#include "config.h"
#include "config_routes.h"
#include "esp_check.h"
#include "json_processor.h"
#include "logger.h"
#include "web_server_util.h"

#define KEY_CONNECTION "connection" ///< JSON key for network connection type
#define KEY_IP_METHOD "ip_method"   ///< JSON key for IP configuration method
#define KEY_LED_BRIGHTNESS "led_brightness" ///< JSON key for LED brightness
#define KEY_WIFI_STATION_CONFIG                                                \
  "station_config" ///< JSON key for WiFi station configuration
#define KEY_WIFI_AP_CONFIG                                                     \
  "ap_config"           ///< JSON key for WiFi access point configuration
#define KEY_SSID "ssid" ///< JSON key for WiFi SSID
#define KEY_PASSWORD "password"       ///< JSON key for WiFi password
#define KEY_DMX_PORTS "dmx_ports"     ///< JSON key for DMX ports configuration
#define KEY_DMX_PORT_INDEX "index"    ///< JSON key for DMX port index
#define KEY_UNIVERSE "universe"       ///< JSON key for DMX universe
#define KEY_DMX_DIRECTION "direction" ///< JSON key for DMX direction


static void action_set_connection(int connection);
static void action_set_ip_method(int ip_method);
static void action_set_led_brightness(int led_brightness);
static void action_set_port_config(const cJSON *array);
static void action_set_wifi_sta_config(const cJSON *json);
static void action_set_wifi_ap_config(const cJSON *json);

/**
 * @brief JSON processor definition for validating and processing the
 * configuration for WiFi station and access point setting.
 */
static const json_processor_object_entry_t wifi_config_entries_processor[] = {
    {
        .key = KEY_SSID,
        .required = false,
        .entry_processor = JSON_PROC_STR(0, 32, NULL),
    },
    {
        .key = KEY_PASSWORD,
        .required = false,
        .entry_processor = JSON_PROC_STR(0, 64, NULL),
    },
};

/**
 * @brief JSON processor definition for validating and processing the
 * configuration for a single DMX port.
 */
static const json_processor_t dmx_port_processor = {
    .type = JSON_PROCESSOR_TYPE_OBJECT,
    .processor.object = {
        .processors =
            (json_processor_object_entry_t[]){
                {.key = KEY_DMX_PORT_INDEX,
                 .required = true,
                 .entry_processor =
                     JSON_PROC_INT(0, APP_CONFIG_DMX_PORT_COUNT - 1, NULL)},
                {.key = KEY_UNIVERSE,
                 .required = false,
                 .entry_processor =
                     JSON_PROC_INT(0, APP_CONFIG_MAX_UNIVERSE, NULL)},
                {.key = KEY_DMX_DIRECTION,
                 .required = false,
                 .entry_processor = JSON_PROC_INT(APP_CONFIG_DIR_MIN,
                                                  APP_CONFIG_DIR_MAX, NULL)},
            },
        .num_processors = 3,
    }};

/**
 * @brief JSON processor definition for validating and processing the entire
 * configuration object received in POST /api/config requests.
 */
static const json_processor_t processor = {
    .type = JSON_PROCESSOR_TYPE_OBJECT,
    .processor.object = {
        .processors =
            (json_processor_object_entry_t[]){
                {.key = KEY_CONNECTION,
                 .required = false,
                 .entry_processor =
                     JSON_PROC_INT(APP_CONFIG_CONN_MIN, APP_CONFIG_CONN_MAX,
                                   action_set_connection)},
                {.key = KEY_IP_METHOD,
                 .required = false,
                 .entry_processor = JSON_PROC_INT(APP_CONFIG_IP_METHOD_MIN,
                                                  APP_CONFIG_IP_METHOD_MAX,
                                                  action_set_ip_method)},
                {.key = KEY_LED_BRIGHTNESS,
                 .required = false,
                 .entry_processor =
                     JSON_PROC_INT(0, APP_CONFIG_MAX_LED_BRIGHTNESS,
                                   action_set_led_brightness)},
                {.key = KEY_WIFI_STATION_CONFIG,
                 .required = false,
                 .entry_processor =
                     JSON_PROC_OBJ(wifi_config_entries_processor, 2,
                                   action_set_wifi_sta_config)},
                {.key = KEY_WIFI_AP_CONFIG,
                 .required = false,
                 .entry_processor =
                     JSON_PROC_OBJ(wifi_config_entries_processor, 2,
                                   action_set_wifi_ap_config)},
                {.key = KEY_DMX_PORTS,
                 .required = false,
                 .entry_processor = JSON_PROC_ARR(&dmx_port_processor, 0,
                                                  APP_CONFIG_DMX_PORT_COUNT,
                                                  action_set_port_config)},
            },
        .num_processors = 6,
    }};

/**
 * @brief Handler for GET requests to /api/config
 * @param req Pointer to the HTTP request object
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t get_config_handler(httpd_req_t *req) {
  LOGI("Received GET request for /api/config");

  cJSON *root = cJSON_CreateObject();
  if (!root) {
    goto fail;
  }

  // --- Root config fields ---
  cJSON_AddNumberToObject(root, KEY_CONNECTION, config_get_connection());
  cJSON_AddNumberToObject(root, KEY_IP_METHOD, config_get_ip_method());
  cJSON_AddNumberToObject(root, KEY_LED_BRIGHTNESS,
                          config_get_led_brightness());

  wifi_config_t wifi_config;
  char buf_ssid[33] = {0};
  char buf_pass[65] = {0};

  // --- WiFi Station Config ---
  config_get_wifi_sta_config(&wifi_config);
  cJSON *json_wifi_sta = cJSON_CreateObject();
  if (!json_wifi_sta) {
    goto fail;
  }
  cJSON_AddItemToObject(root, KEY_WIFI_STATION_CONFIG, json_wifi_sta);
  memcpy(buf_ssid, wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid));
  memcpy(buf_pass, wifi_config.sta.password, sizeof(wifi_config.sta.password));
  cJSON_AddStringToObject(json_wifi_sta, KEY_SSID, buf_ssid);
  cJSON_AddStringToObject(json_wifi_sta, KEY_PASSWORD, buf_pass);

  // --- WiFi AP Config ---
  config_get_wifi_ap_config(&wifi_config);
  cJSON *json_wifi_ap = cJSON_CreateObject();
  if (!json_wifi_ap) {
    goto fail;
  }
  cJSON_AddItemToObject(root, KEY_WIFI_AP_CONFIG, json_wifi_ap);
  memset(buf_ssid, 0, sizeof(buf_ssid));
  memset(buf_pass, 0, sizeof(buf_pass));
  memcpy(buf_ssid, wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid));
  memcpy(buf_pass, wifi_config.ap.password, sizeof(wifi_config.ap.password));
  cJSON_AddStringToObject(json_wifi_ap, KEY_SSID, buf_ssid);
  cJSON_AddStringToObject(json_wifi_ap, KEY_PASSWORD, buf_pass);

  // --- DMX Ports Array ---
  cJSON *dmx_array = cJSON_AddArrayToObject(root, KEY_DMX_PORTS);
  if (!dmx_array) {
    goto fail;
  }
  for (size_t i = 0; i < APP_CONFIG_DMX_PORT_COUNT; i++) {
    cJSON *port_config = cJSON_CreateObject();
    if (port_config) {
      cJSON_AddItemToArray(dmx_array, port_config);
      cJSON_AddNumberToObject(port_config, KEY_DMX_PORT_INDEX, i);
      cJSON_AddNumberToObject(port_config, KEY_UNIVERSE,
                              config_get_dmx_universe(i));
      cJSON_AddNumberToObject(port_config, KEY_DMX_DIRECTION,
                              config_get_dmx_direction(i));
    }
  }

  // Generate response string
  char *json_str = cJSON_PrintUnformatted(root);
  if (!json_str) {
    goto fail;
  }

  cJSON_Delete(root);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, json_str);

  cJSON_free(json_str);
  return ESP_OK;

fail:
  cJSON_Delete(root);

  LOGE("Failed to create JSON response due to memory constraints");
  httpd_resp_set_status(req, "507 Insufficient Storage");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

/**
 * @brief Wrapper function for actions to update the connection method based on
 * the validated JSON input.
 * @param connection The new network connection type, expected to be one of the
 * values defined in `config_connection_t`
 */
static void action_set_connection(int connection) {
  config_set_connection(connection);
}

/**
 * @brief Wrapper function for actions to update the IP method based on the
 * validated JSON input.
 * @param ip_method The new IP configuration method, expected to be one of the
 * values defined in `config_ip_method_t`
 */
static void action_set_ip_method(int ip_method) {
  config_set_ip_method(ip_method);
}

/**
 * @brief Wrapper function for actions to update the LED brightness based on the
 * validated JSON input.
 * @param brightness The new brightness level for the LED, expected to be in the
 * range of 0 to `APP_CONFIG_MAX_LED_BRIGHTNESS`
 */
static void action_set_led_brightness(int brightness) {
  config_set_led_brightness(brightness);
}

/**
 * @brief Function for actions to update the DMX port configuration
 * based on the validated JSON input.
 * @param array cJSON array containing the DMX port configurations
 */
static void action_set_port_config(const cJSON *array) {
  int len = cJSON_GetArraySize(array);
  LOGI("Setting config for %d DMX ports", len);

  for (int i = 0; i < len; i++) {
    cJSON *item = cJSON_GetArrayItem(array, i);
    const cJSON *port_index =
        cJSON_GetObjectItemCaseSensitive(item, KEY_DMX_PORT_INDEX);
    const cJSON *universe =
        cJSON_GetObjectItemCaseSensitive(item, KEY_UNIVERSE);
    const cJSON *direction =
        cJSON_GetObjectItemCaseSensitive(item, KEY_DMX_DIRECTION);
    LOGI("Port config - index: %d, universe: %d, direction: %d",
         port_index->valueint, universe ? universe->valueint : -1,
         direction ? direction->valueint : -1);

    if (universe != NULL) {
      config_set_dmx_universe(port_index->valueint, universe->valueint);
    }

    if (direction != NULL) {
      config_set_dmx_direction(port_index->valueint, direction->valueint);
    }
  }
}

/**
 * @brief Function for actions to update the WiFi station configuration
 * based on the validated JSON input.
 * @param json cJSON object containing the WiFi station configuration
 */
static void action_set_wifi_sta_config(const cJSON *json) {
  const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(json, KEY_SSID);
  const cJSON *password = cJSON_GetObjectItemCaseSensitive(json, KEY_PASSWORD);

  if (ssid != NULL) {
    config_set_wifi_sta_ssid(ssid->valuestring);
  }

  if (password != NULL) {
    config_set_wifi_sta_password(password->valuestring);
  }
}

/**
 * @brief Function for actions to update the WiFi access point
 * configuration based on the validated JSON input.
 * @param json cJSON object containing the WiFi access point configuration
 */
static void action_set_wifi_ap_config(const cJSON *json) {
  const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(json, KEY_SSID);
  const cJSON *password = cJSON_GetObjectItemCaseSensitive(json, KEY_PASSWORD);

  if (ssid != NULL) {
    config_set_wifi_ap_ssid(ssid->valuestring);
  }

  if (password != NULL) {
    config_set_wifi_ap_password(password->valuestring);
  }
}

/**
 * @brief Handler for POST requests to /api/config
 * @param req Pointer to the HTTP request object
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t post_config_handler(httpd_req_t *req) {
  LOGI("Received POST request for /api/config");

  cJSON *json;
  if (get_json_body(req, &json) != ESP_OK) {
    // Error response already sent by get_json_body
    return ESP_OK;
  }

  char *error_msg = NULL;
  esp_err_t validation_result =
      json_processor_process(json, &processor, &error_msg);

  cJSON_Delete(json);

  if (validation_result != ESP_OK) {
    if (validation_result == ESP_ERR_NO_MEM) {
      LOGE("Memory allocation failed while processing JSON");
      httpd_resp_set_status(req, "507 Insufficient Storage");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    }

    if (validation_result == ESP_ERR_INVALID_ARG) {
      LOGW("Received invalid JSON data in POST /api/config: %s", error_msg);
      httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, error_msg);
      free(error_msg);
      return ESP_OK;
    }

    LOGE("Unexpected error while processing JSON: %s", error_msg);
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, NULL, 0);
    free(error_msg);
    return ESP_OK;
  }

  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

/**
 * @brief Array of URI handlers for the web server configuration routes
 */
static const httpd_uri_t routes[] = {{
                                         .uri = "/api/config",
                                         .method = HTTP_GET,
                                         .handler = get_config_handler,
                                         .user_ctx = NULL,
                                     },
                                     {
                                         .uri = "/api/config",
                                         .method = HTTP_POST,
                                         .handler = post_config_handler,
                                         .user_ctx = NULL,
                                     }};

size_t get_config_routes(const httpd_uri_t **routes_out) {
  *routes_out = routes;
  return sizeof(routes) / sizeof(httpd_uri_t);
}

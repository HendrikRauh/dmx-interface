/**
 * @file web_server.h
 * @brief Simple HTTP web server component for ESP32 with async FreeRTOS support.
 */

#pragma once

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Web server configuration structure.
   */
  typedef struct
  {
    uint16_t port;             ///< HTTP server port (default: 80)
    size_t max_uri_handlers;   ///< Maximum number of URI handlers
    size_t stack_size;         ///< FreeRTOS task stack size
    UBaseType_t task_priority; ///< FreeRTOS task priority
  } webserver_config_t;

  /**
   * @brief Initialize and start the HTTP web server.
   *
   * This function creates a FreeRTOS task that manages the HTTP server.
   * It serves static files from the data/ folder and supports dynamic handler registration.
   *
   * @param config Configuration structure. If NULL, default values are used.
   * @return HTTP server handle on success, NULL on failure.
   */
  httpd_handle_t webserver_start(const webserver_config_t *config);

  /**
   * @brief Stop the web server and cleanup resources.
   *
   * @param server HTTP server handle returned by webserver_start().
   *                Safe to pass NULL.
   */
  void webserver_stop(httpd_handle_t server);

  /**
   * @brief Register a custom URI handler.
   *
   * This allows dynamic registration of API endpoints and other custom handlers.
   *
   * @param server HTTP server handle.
   * @param uri_handler Pointer to httpd_uri_t structure.
   * @return ESP_OK on success, error code otherwise.
   */
  esp_err_t webserver_register_handler(httpd_handle_t server, const httpd_uri_t *uri_handler);

#ifdef __cplusplus
}
#endif

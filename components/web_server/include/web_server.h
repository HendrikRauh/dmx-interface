/**
 * @file web_server.h
 * @brief Simple HTTP web server component
 *
 * This header defines the interface for a simple web server that serves static
 * files embedded in the firmware and allows dynamic registration of URI
 * handlers for API endpoints. The web server is built on top of the ESP-IDF
 * HTTP server library and provides a convenient way to start, stop, and manage
 * the server.
 */

#pragma once

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Web server configuration structure.
 */
typedef struct {
  uint16_t port;             ///< HTTP server port (default: 80)
  size_t max_uri_handlers;   ///< Maximum number of URI handlers
  size_t stack_size;         ///< FreeRTOS task stack size
  UBaseType_t task_priority; ///< FreeRTOS task priority
} webserver_config_t;

/**
 * @brief Initialize and start the HTTP web server.
 *
 * The web server serves static files that are embedded in the firmware and
 * supports dynamic handler registration.
 *
 * @param config Configuration structure. If NULL, default values are used.
 * @return HTTP server handle on success, NULL on failure.
 */
httpd_handle_t webserver_start(const webserver_config_t *config);

/**
 * @brief Stop the web server
 *
 * Stop the HTTP server and cleanup resources.
 */
void webserver_stop();

/**
 * @brief Register a custom URI handler.
 *
 * This allows dynamic registration of API endpoints and other custom handlers.
 *
 * @param server HTTP server handle.
 * @param uri_handler Pointer to httpd_uri_t structure.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t webserver_register_handler(httpd_handle_t server,
                                     const httpd_uri_t *uri_handler);

/**
 * @brief Register an array of custom URI handlers.
 *
 * This allows dynamic registration of API endpoints and other custom handlers.
 *
 * @param server HTTP server handle.
 * @param routes Array of URI handlers to register.
 * @param count Number of URI handlers in the array.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t webserver_register_handler_array(httpd_handle_t server,
                                           const httpd_uri_t *routes,
                                           size_t count);

#ifdef __cplusplus
}
#endif

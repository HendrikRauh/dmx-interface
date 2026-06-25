/**
 * @file web_server.c
 * @brief Implementation of a simple web server using ESP-IDF's HTTP server.
 */

/**
 * @brief Tag used for web server logging.
 */
#define LOG_TAG "WEBSRV"

#include "web_server.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config_routes.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logger.h"

// Default configuration values
/**
 * @brief Default port for the web server.
 */
#define WEBSERVER_DEFAULT_PORT 80
/**
 * @brief Default maximum number of URI handlers.
 */
#define WEBSERVER_DEFAULT_MAX_HANDLERS 32
/**
 * @brief Default stack size for the web server task.
 */
#define WEBSERVER_DEFAULT_STACK_SIZE (8 * 1024)
/**
 * @brief Default task priority for the web server task.
 */
#define WEBSERVER_DEFAULT_TASK_PRIORITY 5

/**
 * @brief Structure representing a static file embedded in the firmware.
 *
 * This structure holds metadata about the file, including its path, content,
 * and encoding. It is used to serve static files directly from the firmware
 * without relying on an external filesystem.
 */
typedef struct {
  const char *path; ///< The URI path for the static file (e.g., "/index.html")
  const uint8_t *start;     ///< Pointer to the start of the file data in memory
  const uint8_t *end;       ///< Pointer to the end of the file data in memory
  const char *content_type; ///< MIME type of the file (e.g., "text/html")
  const char *content_encoding; ///< Optional content encoding (e.g., "gzip"),
                                ///< NULL if not applicable
} static_file_t;

/** @brief Memory address of the start of the embedded index.html.gz file */
extern const uint8_t index_html_gz_start[] asm("_binary_index_html_gz_start");
/** @brief Memory address of the end of the embedded index.html.gz file */
extern const uint8_t index_html_gz_end[] asm("_binary_index_html_gz_end");

/** @brief Memory address of the start of the embedded Fredoka.ttf file */
extern const uint8_t fredoka_ttf_start[] asm("_binary_Fredoka_ttf_start");
/** @brief Memory address of the end of the embedded Fredoka.ttf file */
extern const uint8_t fredoka_ttf_end[] asm("_binary_Fredoka_ttf_end");

/**
 * @brief Array of static files embedded in the firmware.
 */
static const static_file_t static_files[] = {
    {.path = "/",
     .start = index_html_gz_start,
     .end = index_html_gz_end,
     .content_type = "text/html",
     .content_encoding = "gzip"},
    {.path = "/fonts/Fredoka.ttf",
     .start = fredoka_ttf_start,
     .end = fredoka_ttf_end,
     .content_type = "font/ttf",
     .content_encoding = NULL},
};

/**
 * @brief Number of static files embedded in the firmware.
 */
static const size_t static_files_count =
    sizeof(static_files) / sizeof(static_file_t);

/**
 * @brief Handle for the HTTP server instance.
 */
static httpd_handle_t s_server_handle = NULL;

/**
 * @brief Handle for the FreeRTOS web server task.
 */
static TaskHandle_t s_server_task_handle = NULL;

/**
 * @brief Retrieve static file data based on the requested path.
 * @param path The requested URI path
 * @return Pointer to the static_file_t structure if found, NULL otherwise
 */
static const static_file_t *get_file_data(const char *path) {
  for (size_t i = 0; i < static_files_count; i++) {
    if (strcmp(path, static_files[i].path) == 0) {
      return &static_files[i];
    }
  }
  return NULL;
}

/**
 * @brief HTTP handler for static files embedded in the firmware.
 * @param req Pointer to the HTTP request structure
 * @return ESP_OK on success, or an error code on failure
 */
static esp_err_t static_file_handler(httpd_req_t *req) {
  const static_file_t *file = get_file_data(req->uri);

  if (file == NULL) {
    LOGW("File not found: %s", req->uri);
    httpd_resp_send_404(req);
    return ESP_OK;
  }

  httpd_resp_set_type(req, file->content_type);
  if (file->content_encoding) {
    httpd_resp_set_hdr(req, "Content-Encoding", file->content_encoding);
  }

  const size_t file_len = file->end - file->start;
  httpd_resp_send(req, (const char *)file->start, file_len);

  return ESP_OK;
}

/**
 * @brief FreeRTOS task function for the HTTP server.
 * Allows non-blocking server operation and future extensibility.
 */
static void webserver_task(void *arg) {
  (void)arg; // Unused parameter
  LOGI("Web server task started");

  // Keep task alive - the server runs in the background
  while (s_server_handle != NULL) {
    vTaskDelay(pdMS_TO_TICKS(10000)); // 10 second check interval
  }

  LOGI("Web server task ending");
  vTaskDelete(NULL);
}

httpd_handle_t webserver_start(const webserver_config_t *config) {
  if (s_server_handle != NULL) {
    LOGW("Web server already running");
    return s_server_handle;
  }

  // Use provided config or defaults
  uint16_t port = WEBSERVER_DEFAULT_PORT;
  size_t max_handlers = WEBSERVER_DEFAULT_MAX_HANDLERS;
  size_t stack_size = WEBSERVER_DEFAULT_STACK_SIZE;
  UBaseType_t task_priority = WEBSERVER_DEFAULT_TASK_PRIORITY;

  if (config) {
    port = config->port;
    max_handlers = config->max_uri_handlers;
    stack_size = config->stack_size;
    task_priority = config->task_priority;
  }

  // Create HTTP server configuration
  httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
  http_config.server_port = port;
  http_config.max_uri_handlers = max_handlers;
  http_config.stack_size = stack_size;
  http_config.uri_match_fn = httpd_uri_match_wildcard;

  // Start HTTP server
  esp_err_t ret = httpd_start(&s_server_handle, &http_config);
  if (ret != ESP_OK) {
    LOGE("Failed to start HTTP server: %s", esp_err_to_name(ret));
    s_server_handle = NULL;
    return NULL;
  }

  LOGI("HTTP server started on port %d", port);

  const httpd_uri_t *routes = NULL;
  size_t route_count = 0;

  // Register config routes
  route_count = get_config_routes(&routes);
  webserver_register_handler_array(s_server_handle, routes, route_count);

  // Wildcard handler for embedded static files (must be last)
  httpd_uri_t file_uri = {
      .uri = "/*",
      .method = HTTP_GET,
      .handler = static_file_handler,
      .user_ctx = NULL,
  };
  httpd_register_uri_handler(s_server_handle, &file_uri);

  // Create FreeRTOS task for the server
  // This allows other tasks to continue running and makes the server
  // async-ready
  BaseType_t task_ret = xTaskCreate(webserver_task, "webserver", stack_size,
                                    (void *)s_server_handle, task_priority,
                                    &s_server_task_handle);

  if (task_ret != pdPASS) {
    LOGE("Failed to create web server task");
    httpd_stop(s_server_handle);
    s_server_handle = NULL;
    return NULL;
  }

  LOGI("Web server initialized successfully");
  return s_server_handle;
}

void webserver_stop() {
  if (s_server_handle == NULL) {
    return;
  }

  httpd_stop(s_server_handle);
  s_server_handle = NULL;

  // Wait for task to finish
  if (s_server_task_handle != NULL) {
    vTaskDelay(pdMS_TO_TICKS(100));
    s_server_task_handle = NULL;
  }

  LOGI("Web server stopped");
}

esp_err_t webserver_register_handler(httpd_handle_t server,
                                     const httpd_uri_t *uri_handler) {
  if (server == NULL || uri_handler == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = httpd_register_uri_handler(server, uri_handler);
  if (ret == ESP_OK) {
    LOGI("Registered handler: %s [%d]", uri_handler->uri, uri_handler->method);
  } else {
    LOGE("Failed to register handler %s: %s", uri_handler->uri,
         esp_err_to_name(ret));
  }

  return ret;
}

esp_err_t webserver_register_handler_array(httpd_handle_t server,
                                           const httpd_uri_t *routes,
                                           size_t count) {
  for (size_t i = 0; i < count; i++) {
    esp_err_t ret = webserver_register_handler(server, &routes[i]);
    if (ret != ESP_OK) {
      return ret;
    }
  }
  return ESP_OK;
}

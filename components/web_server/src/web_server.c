#define LOG_TAG "WEBSRV"

/**
 * @def LOG_TAG
 * @brief Tag used for web server logging.
 */

#include "web_server.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logger.h"
#include "storage.h"

// Default configuration values
/**
 * @brief Default port for the web server.
 */
#define WEBSERVER_DEFAULT_PORT 80
/**
 * @brief Default port for the web server.
 */
#define WEBSERVER_DEFAULT_MAX_HANDLERS 32
/**
 * @brief Default maximum number of URI handlers.
 */
#define WEBSERVER_DEFAULT_STACK_SIZE (8 * 1024)
/**
 * @brief Default stack size for the web server task.
 */
#define WEBSERVER_DEFAULT_TASK_PRIORITY 5
/**
 * @brief Default task priority for the web server task.
 */

/**
 * @brief Handle for the HTTP server instance.
 */
static httpd_handle_t s_server_handle = NULL;

/**
 * @brief Handle for the FreeRTOS web server task.
 */
static TaskHandle_t s_server_task_handle = NULL;

/**
 * @brief Get MIME type based on file extension
 */
static const char *get_mime_type(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot)
    return "application/octet-stream";

  if (strcmp(dot, ".html") == 0)
    return "text/html";
  if (strcmp(dot, ".css") == 0)
    return "text/css";
  if (strcmp(dot, ".js") == 0)
    return "application/javascript";
  if (strcmp(dot, ".json") == 0)
    return "application/json";
  if (strcmp(dot, ".png") == 0)
    return "image/png";
  if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
    return "image/jpeg";
  if (strcmp(dot, ".gif") == 0)
    return "image/gif";
  if (strcmp(dot, ".svg") == 0)
    return "image/svg+xml";
  if (strcmp(dot, ".ico") == 0)
    return "image/x-icon";
  if (strcmp(dot, ".txt") == 0)
    return "text/plain";
  if (strcmp(dot, ".xml") == 0)
    return "application/xml";
  if (strcmp(dot, ".wav") == 0)
    return "audio/wav";
  if (strcmp(dot, ".mp3") == 0)
    return "audio/mpeg";

  return "application/octet-stream";
}

/**
 * @brief HTTP handler for static files from LittleFS
 */
static esp_err_t static_file_handler(httpd_req_t *req) {
  // Build the file path
  char filepath[1024];
  snprintf(filepath, sizeof(filepath), "%s%s", storage_get_mount_point(),
           req->uri);

  // Handle root path
  if (strcmp(req->uri, "/") == 0) {
    snprintf(filepath, sizeof(filepath), "%s/index.html",
             storage_get_mount_point());
  }

  FILE *f = fopen(filepath, "r");
  if (!f) {
    LOGW("File not found: %s", filepath);
    httpd_resp_send_404(req);
    return ESP_OK;
  }

  // Get MIME type and set content type
  const char *mime_type = get_mime_type(filepath);
  httpd_resp_set_type(req, mime_type);

  // Send file in chunks
  char buf[1024];
  size_t read_len;
  while ((read_len = fread(buf, 1, sizeof(buf), f)) > 0) {
    if (httpd_resp_send_chunk(req, buf, read_len) != ESP_OK) {
      LOGW("Failed to send data chunk for %s", filepath);
      break;
    }
  }

  fclose(f);
  httpd_resp_send_chunk(req, NULL, 0); // Send end marker
  return ESP_OK;
}

/**
 * @brief HTTP handler for API health check (GET /api/health)
 */
static esp_err_t health_check_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
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

/**
 * @brief Start the web server with the given configuration.
 *
 * Initializes storage, configures the HTTP server, registers default handlers,
 * and starts the FreeRTOS task for async operation.
 *
 * @param config Pointer to webserver configuration struct (optional)
 * @return Handle to the running HTTP server, or NULL on failure
 */
httpd_handle_t webserver_start(const webserver_config_t *config) {
  if (s_server_handle != NULL) {
    LOGW("Web server already running");
    return s_server_handle;
  }

  // Initialize LittleFS
  esp_err_t ret = storage_init();
  if (ret != ESP_OK) {
    LOGE("Failed to initialize storage");
    return NULL;
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
  ret = httpd_start(&s_server_handle, &http_config);
  if (ret != ESP_OK) {
    LOGE("Failed to start HTTP server: %s", esp_err_to_name(ret));
    s_server_handle = NULL;
    return NULL;
  }

  LOGI("HTTP server started on port %d", port);

  // Register default handlers
  // Health check endpoint
  httpd_uri_t health_uri = {
      .uri = "/api/health",
      .method = HTTP_GET,
      .handler = health_check_handler,
      .user_ctx = NULL,
  };
  httpd_register_uri_handler(s_server_handle, &health_uri);

  // Wildcard handler for static files from LittleFS (must be last)
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

/**
 * @brief Stop the web server and clean up resources.
 *
 * Stops the HTTP server and deletes the FreeRTOS task.
 *
 * @param server Handle to the HTTP server instance
 */
void webserver_stop(httpd_handle_t server) {
  if (server == NULL) {
    return;
  }

  httpd_stop(server);
  s_server_handle = NULL;

  // Wait for task to finish
  if (s_server_task_handle != NULL) {
    vTaskDelay(pdMS_TO_TICKS(100));
    s_server_task_handle = NULL;
  }

  LOGI("Web server stopped");
}

/**
 * @brief Register a URI handler with the web server.
 *
 * @param server Handle to the HTTP server instance
 * @param uri_handler Pointer to the URI handler struct
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or other error codes on
 * failure
 */
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

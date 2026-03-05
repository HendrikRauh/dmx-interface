#include "web_server.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "WEBSERVER";

// Default configuration values
#define WEBSERVER_DEFAULT_PORT 80
#define WEBSERVER_DEFAULT_MAX_HANDLERS 32
#define WEBSERVER_DEFAULT_STACK_SIZE (8 * 1024)
#define WEBSERVER_DEFAULT_TASK_PRIORITY 5

static httpd_handle_t s_server_handle = NULL;
static TaskHandle_t s_server_task_handle = NULL;

/**
 * @brief HTTP handler for root (GET /)
 */
static esp_err_t root_handler(httpd_req_t *req)
{
    const char *html = "<!DOCTYPE html>"
                       "<html>"
                       "<head><title>DMX Interface</title></head>"
                       "<body>"
                       "<h1>DMX Interface</h1>"
                       "<p>Web server is running!</p>"
                       "<p><a href=\"/api/health\">Check Health</a></p>"
                       "</body>"
                       "</html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, html);
    return ESP_OK;
}

/**
 * @brief HTTP handler for API health check (GET /api/health)
 */
static esp_err_t health_check_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/**
 * @brief FreeRTOS task function for the HTTP server.
 * Allows non-blocking server operation and future extensibility.
 */
static void webserver_task(void *arg)
{
    httpd_handle_t server = (httpd_handle_t)arg;
    ESP_LOGI(TAG, "Web server task started");

    // Keep task alive - the server runs in the background
    while (s_server_handle != NULL)
    {
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10 second check interval
    }

    ESP_LOGI(TAG, "Web server task ending");
    vTaskDelete(NULL);
}

httpd_handle_t webserver_start(const webserver_config_t *config)
{
    if (s_server_handle != NULL)
    {
        ESP_LOGW(TAG, "Web server already running");
        return s_server_handle;
    }

    // Use provided config or defaults
    uint16_t port = WEBSERVER_DEFAULT_PORT;
    size_t max_handlers = WEBSERVER_DEFAULT_MAX_HANDLERS;
    size_t stack_size = WEBSERVER_DEFAULT_STACK_SIZE;
    UBaseType_t task_priority = WEBSERVER_DEFAULT_TASK_PRIORITY;

    if (config)
    {
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
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        s_server_handle = NULL;
        return NULL;
    }

    ESP_LOGI(TAG, "HTTP server started on port %d", port);

    // Register default handlers
    // Health check endpoint
    httpd_uri_t health_uri = {
        .uri = "/api/health",
        .method = HTTP_GET,
        .handler = health_check_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_server_handle, &health_uri);

    // Root / index.html handler
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_server_handle, &root_uri);

    // Wildcard handler for 404 (must be last)
    httpd_uri_t wildcard_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = NULL, // Let httpd handle as 404
        .user_ctx = NULL,
    };
    // Don't register wildcard - just let httpd default to 404

    // Create FreeRTOS task for the server
    // This allows other tasks to continue running and makes the server async-ready
    BaseType_t task_ret = xTaskCreate(
        webserver_task,
        "webserver",
        stack_size,
        (void *)s_server_handle,
        task_priority,
        &s_server_task_handle);

    if (task_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create web server task");
        httpd_stop(s_server_handle);
        s_server_handle = NULL;
        return NULL;
    }

    ESP_LOGI(TAG, "Web server initialized successfully");
    return s_server_handle;
}

void webserver_stop(httpd_handle_t server)
{
    if (server == NULL)
    {
        return;
    }

    httpd_stop(server);
    s_server_handle = NULL;

    // Wait for task to finish
    if (s_server_task_handle != NULL)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        s_server_task_handle = NULL;
    }

    ESP_LOGI(TAG, "Web server stopped");
}

esp_err_t webserver_register_handler(httpd_handle_t server, const httpd_uri_t *uri_handler)
{
    if (server == NULL || uri_handler == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = httpd_register_uri_handler(server, uri_handler);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Registered handler: %s [%d]", uri_handler->uri, uri_handler->method);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to register handler %s: %s", uri_handler->uri, esp_err_to_name(ret));
    }

    return ret;
}

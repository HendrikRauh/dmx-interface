/**
 * @file web_server_util.h
 * @brief Utility functions and definitions for the web server component.
 */

#pragma once

#include "esp_http_server.h"
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Helper function to read the entire request body and parse it as JSON.
 * @param[in] req The HTTP request object.
 * @param[out] json_out Pointer to a cJSON pointer that will receive the parsed
 * JSON object.
 * @return `ESP_OK` on success, `ESP_FAIL` or `ESP_ERR_NO_MEM` on failure.
 *
 * Reads the request body in chunks until the entire content is received, then
 * attempts to parse it as JSON using cJSON.
 * If error occur during reception or parsing, an appropriate HTTP error
 * response is sent back to the client. If successful, the parsed cJSON object
 * is returned via the `json_out` parameter. The caller is responsible for
 * freeing the cJSON object when done.
 */
esp_err_t get_json_body(httpd_req_t *req, cJSON **json_out);

#ifdef __cplusplus
}
#endif

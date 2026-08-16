/**
 * @file web_server_util.c
 * @brief Utility functions for the web server component.
 */

#define LOG_TAG "WEBSRV" ///< Logging tag for web server utilities

#define MAX_POST_DATA_SIZE (20 * 1024) ///< Maximum size for POST request data

#include "web_server_util.h"
#include "logger.h"

esp_err_t get_json_body(httpd_req_t *req, cJSON **json_out) {
  int total_len = req->content_len;

  if (total_len <= 0) {
    LOGW("Request body is empty");
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty request body");
    return ESP_FAIL;
  }

  if (total_len > MAX_POST_DATA_SIZE) {
    LOGW("Request body too large: %d bytes", total_len);
    httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE,
                        "Request body too large");
    return ESP_FAIL;
  }

  char *buf = malloc(total_len + 1);
  if (buf == NULL) {
    LOGE("Failed to allocate memory for request body");
    httpd_resp_set_status(req, "507 Insufficient Storage");
    httpd_resp_send(req, NULL, 0);
    return ESP_ERR_NO_MEM;
  }

  int cur_len = 0;
  int received = 0;
  while (cur_len < total_len) {
    received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
    if (received <= 0) {
      if (received == HTTPD_SOCK_ERR_TIMEOUT) {
        continue;
      }

      free(buf);
      LOGE("Failed to receive request body");
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                          "Failed to receive post data");
      return ESP_FAIL;
    }
    cur_len += received;
  }

  buf[total_len] = '\0'; // Null-terminate the buffer

  LOGD("Received post data: %s", buf);

  *json_out = cJSON_Parse(buf);
  free(buf);

  if (*json_out == NULL) {
    LOGW("Failed to parse JSON from request body");
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    free(*json_out);
    *json_out = NULL;
    return ESP_FAIL;
  }

  return ESP_OK;
}

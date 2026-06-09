/**
 * @file config_routes.h
 * @brief Header file for the web server configuration routes
 */

#pragma once

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the array of configuration routes for the web server.
 * @param[out] routes_out Output parameter that will point to the array of
 * routes
 * @return The number of routes in the array
 */
size_t get_config_routes(const httpd_uri_t **routes_out);

#ifdef __cplusplus
}
#endif

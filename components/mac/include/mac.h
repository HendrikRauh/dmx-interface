#pragma once

#include "config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Retrieves the MAC address for the specified connection type.
 * @param[out] mac Pointer to a 6-byte array to receive the MAC address.
 * @param[in] connection Connection type to use when selecting the MAC
 * address.
 */
void get_mac(uint8_t *mac, config_connection_t connection);

#ifdef __cplusplus
}
#endif

#pragma once

#include "esp_dmx.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the DMX interface.
 * @param port The DMX port to initialize.
 * @param pin_tx The pin for TX of the RS485.
 * @param pin_rx The pin for RX of the RS485.
 */
void init_dmx(dmx_port_t port, int pin_tx, int pin_rx);

/**
 * @brief Send some test DMX data on the specified port.
 * @param port The DMX port to send data on.
 */
void send_dmx(dmx_port_t port);

#ifdef __cplusplus
}
#endif

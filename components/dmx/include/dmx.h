#pragma once

#include "esp_dmx.h"

#ifdef __cplusplus
extern "C" {
#endif

void init_dmx(dmx_port_t port, int pin_tx, int pin_rx);

void send_dmx(dmx_port_t port);

#ifdef __cplusplus
}
#endif

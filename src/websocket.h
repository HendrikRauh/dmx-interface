#include <AsyncWebServer_ESP32_W5500.h>
#include "routes/status.h"

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

void initWebSocket(AsyncWebServer *server);

void webSocketLoop();

#endif

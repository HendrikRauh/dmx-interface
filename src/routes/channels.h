#include <Preferences.h>
#include <ArduinoJson.h>
#include <AsyncWebServer_ESP32_W5500.h>
#include "ESPDMX.h"

extern Preferences config;

void onGetChannels(AsyncWebServerRequest *request, DMXESPSerial dmx1, DMXESPSerial dmx2);
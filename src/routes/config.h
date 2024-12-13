#pragma once

#include <AsyncWebServer_ESP32_W5500.h>
#include <ESPDMX.h>
#include <Preferences.h>

// #ifndef CONFIG_h
// #define CONFIG_h

extern Preferences config;

enum IpMethod
{
    Static,
    DHCP
};
const uint8_t IP_METHOD_SIZE = 2;

enum Connection
{
    WiFiSta,
    WiFiAP,
    Ethernet
};
const uint8_t CONNECTION_SIZE = 3;

enum Direction
{
    Output,
    Input
};
const uint8_t DIRECTION_SIZE = 2;

void onGetConfig(AsyncWebServerRequest *request);

void onPutConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

void onGetNetworks(AsyncWebServerRequest *request);

// #endif
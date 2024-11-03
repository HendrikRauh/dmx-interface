#pragma once

#include <ESPAsyncWebServer.h>
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

enum Connection
{
    WiFiSta,
    WiFiAP,
    Ethernet
};

enum Direction
{
    Input,
    Output
};

void onGetConfig(
    String ssid,
    String pwd,
    uint32_t ip,
    uint32_t subnet,
    uint32_t gateway,
    uint8_t universe1,
    Direction direction1,
    uint8_t universe2,
    Direction direction2,
    AsyncWebServerRequest *request);

void onPutConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

// #endif
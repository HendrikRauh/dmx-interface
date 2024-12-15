#include <AsyncWebServer_ESP32_W5500.h>
#include <ESPDMX.h>
#include <Preferences.h>

#ifndef CONFIG_h
#define CONFIG_h

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

const Connection DEFAULT_CONNECTION = WiFiAP;
const IpMethod DEFAULT_IP_METHOD = DHCP;
extern String DEFAULT_SSID; // initialized in setup because it depends on the mac address
const String DEFAULT_PASSWORD = "mbgmbgmbg";
const IPAddress DEFAULT_IP(192, 168, 4, 1);
const IPAddress DEFAULT_SUBNET(255, 255, 255, 0);
const IPAddress DEFAULT_GATEWAY(2, 0, 0, 1);

const Direction DEFAULT_DIRECTION1 = Output;
const Direction DEFAULT_DIRECTION2 = Input;
const uint8_t DEFAULT_UNIVERSE1 = 1;
const uint8_t DEFAULT_UNIVERSE2 = 2;

const uint8_t DEFAULT_LED_BRIGHTNESS = 25;

void onGetConfig(AsyncWebServerRequest *request);

void onPutConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

#endif
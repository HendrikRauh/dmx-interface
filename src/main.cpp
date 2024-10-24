#include <ArtnetWiFi.h>
// #include <ArtnetEther.h>

#include "ESPDMX.h"
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

// WiFi stuff
const char *ssid = "artnet";
const char *pwd = "mbgmbgmbg";
const IPAddress ip(192, 168, 1, 201);
const IPAddress gateway(192, 168, 1, 1);
const IPAddress subnet(255, 255, 255, 0);

AsyncWebServer server(80);

// Art-Net stuff
ArtnetWiFi artnet;
// const String target_ip = "192.168.1.200";
uint8_t universe = 1; // 0 - 15
const uint16_t size = 512;
uint8_t data[size];
uint8_t value = 0;

// DMX stuff
DMXESPSerial dmx;

void setup()
{

    // Serial console
    Serial.begin(9600);

    // WiFi stuff
    // WiFi.begin(ssid, pwd);
    WiFi.softAP(ssid, pwd);
    WiFi.softAPConfig(ip, gateway, subnet);
    // WiFi.config(ip, gateway, subnet);
    // while (WiFi.status() != WL_CONNECTED) {
    //     Serial.print(".");
    delay(500);
    //}
    // Serial.print("WiFi connected, IP = ");
    // Serial.println(WiFi.localIP());

    // Initialize Art-Net
    artnet.begin();

    // Initialize DMX ports
    dmx.init();

    // if Artnet packet comes to this universe, this function is called
    artnet.subscribeArtDmxUniverse(universe, [&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote)
                                   {
                                    for (size_t i = 0; i < size; ++i)
                                    {
                                        dmx.write((i + 1), data[i]);
                                    }

                                    dmx.update(); });

    // if Artnet packet comes, this function is called to every universe
    artnet.subscribeArtDmx([&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {});

    if (!SPIFFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    server.begin();
    Serial.println("Server started!");
}

void loop()
{
    artnet.parse(); // check if artnet packet has come and execute callback
}
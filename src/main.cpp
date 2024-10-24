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
                                       /*Serial.print("lambda : artnet data from ");
                                       Serial.print(remote.ip);
                                       Serial.print(":");
                                       Serial.print(remote.port);
                                       Serial.print(", universe = ");
                                       Serial.print(universe);
                                       Serial.print(", size = ");
                                       Serial.print(size);
                                       Serial.print(") :");*/

                                       for (size_t i = 0; i < size; ++i)
                                       {
                                           dmx.write((i + 1), data[i]);
                                           //    Serial.print(data[i]);
                                           //    Serial.print(",");
                                       }
                                       // Serial.println();

                                       dmx.update(); });

    // if Artnet packet comes, this function is called to every universe
    artnet.subscribeArtDmx([&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote)
                           {
        /*Serial.print("received ArtNet data from ");
        Serial.print(remote.ip);
        Serial.print(":");
        Serial.print(remote.port);
        Serial.print(", net = ");
        Serial.print(metadata.net);
        Serial.print(", subnet = ");
        Serial.print(metadata.subnet);
        Serial.print(", universe = ");
        Serial.print(metadata.universe);
        Serial.print(", sequence = ");
        Serial.print(metadata.sequence);
        Serial.print(", size = ");
        Serial.print(size);
        Serial.println(")");*/ });

    if (!SPIFFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { 
            Serial.println("ESP32 Web Server: New request received:");  // for debugging 
            Serial.println("GET /");        // for debugging 
            request->send(SPIFFS, "/index.html"); });

    server.begin();
    Serial.println("Server started!");
}

void loop()
{
    artnet.parse(); // check if artnet packet has come and execute callback

    /*value = (millis() / 4) % 256;
    memset(data, value, size);

    artnet.setArtDmxData(data, size);
    artnet.streamArtDmxTo(target_ip, universe);  // automatically send set data in 40fps
    // artnet.streamArtDmxTo(target_ip, net, subnet, univ);  // or you can set net, subnet, and universe */
}
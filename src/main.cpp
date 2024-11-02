#include <ArtnetWiFi.h>
// #include <ArtnetEther.h>

#include <ArduinoJson.h>
#include "ESPDMX.h"
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Preferences.h>

Preferences config;

DMXESPSerial dmx1;
DMXESPSerial dmx2;

AsyncWebServer server(80);

ArtnetWiFi artnet;
const uint16_t size = 512;
uint8_t data[size];

void setup()
{
    Serial.begin(9600);

    config.begin("dmx", false);

    uint8_t universe = config.getUChar("universe", 1);

    String ssid = config.getString("ssid", "artnet");
    String pwd = config.getString("pwd", "mbgmbgmbg");
    IPAddress defaultIp(192, 168, 1, 201);
    IPAddress ip = config.getUInt("ip", defaultIp);

    IPAddress cidr = config.getUChar("cidr", 24);

    // TODO: \/ Herleiten \/ @psxde
    const IPAddress gateway(192, 168, 1, 1);
    const IPAddress subnet(255, 255, 255, 0);

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
    dmx1.init(19, -1);

    // if Artnet packet comes to this universe, this function is called
    artnet.subscribeArtDmxUniverse(universe, [&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote)
                                   {
                                    for (size_t i = 0; i < size; ++i)
                                    {
                                        dmx1.write((i + 1), data[i]);
                                    }

                                    dmx1.update(); });

    // if Artnet packet comes, this function is called to every universe
    artnet.subscribeArtDmx([&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {});

    if (!SPIFFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    server.on("/config", HTTP_GET, [&, defaultIp, ssid, pwd, universe](AsyncWebServerRequest *request)
              {
                JsonDocument doc;

                doc["ssid"] = ssid;
                doc["pwd"] = pwd;
                doc["ip"] = defaultIp;
                doc["universe"] = universe;

                String jsonString;
                serializeJson(doc, jsonString);

                request->send(200, "application/json", jsonString); });
    delay(1000);
    server.begin();
    Serial.println("Server started!");
}

void loop()
{
    artnet.parse(); // check if artnet packet has come and execute callback
}
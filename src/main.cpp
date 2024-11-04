#include <ArtnetWiFi.h>
// #include <ArtnetEther.h>
#include "ESPDMX.h"
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include "routes/config.h"

DMXESPSerial dmx1;
DMXESPSerial dmx2;

AsyncWebServer server(80);

ArtnetWiFi artnet;
DMXESPSerial dmx;

const uint16_t size = 512;
uint8_t data[size];

void setup()
{
    Serial.begin(9600);

    config.begin("dmx", true);

    uint8_t universe1 = config.getUChar("universe-1", 1);
    uint8_t universe2 = config.getUChar("universe-2", 1);

    Direction direction1 = static_cast<Direction>(config.getUInt("direction-1", 0));
    Direction direction2 = static_cast<Direction>(config.getUInt("direction-2", 1));

    Connection connection = static_cast<Connection>(config.getUInt("connection", WiFiSta));
    IpMethod ipMethod = static_cast<IpMethod>(config.getUInt("ip-method"), Static);

    String ssid = config.getString("ssid", "artnet");
    String pwd = config.getString("password", "mbgmbgmbg");
    IPAddress defaultIp(192, 168, 1, 201);
    IPAddress ip = config.getUInt("ip", defaultIp);
    IPAddress defaultSubnet(255, 255, 255, 0);
    IPAddress subnet = config.getUInt("subnet", defaultSubnet);
    IPAddress defaultGateway(192, 168, 1, 1);
    IPAddress gateway = config.getUInt("gateway", defaultGateway);

    config.end();

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
    artnet.subscribeArtDmxUniverse(universe1, [&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote)
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

    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request)
              { onGetConfig(config, request); });

    server.on("/config", HTTP_DELETE, [](AsyncWebServerRequest *request)
              {
                config.begin("dmx", false);
                config.clear();
                config.end();
                // respond with default config
                onGetConfig(config, request);

                ESP.restart(); });

    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                         {
                            if (request->url() == "/config" && request->method() == HTTP_PUT) {
                                onPutConfig(request, data, len, index, total);
                                ESP.restart();
                            } });

    delay(1000);
    server.begin();
    Serial.println("Server started!");
}

void loop()
{
    artnet.parse(); // check if artnet packet has come and execute callback
}

#include <ArtnetWiFi.h>
#include <ArtnetEther.h>
#include <ArduinoJson.h>
#include "ESPDMX.h"
#include "SPI.h"

#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif

#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Preferences.h>

Preferences config;
DMXESPSerial dmx;

// Ethernet stuff
#define SCK       39
#define SS        33
#define MOSI      35
#define MISO      37
#define SPI_FREQ  32000000
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
enum ethTypes {AP = 1, STA = 2, ETH = 3}; // to be changed to Raffaels(TM) enum

AsyncWebServer server(80);

ArtnetWiFi artnet;
const uint16_t size = 512;
uint8_t data[size];

void setup()
{
    // Serial console
    Serial.begin(9600);
    Serial.print("Start DMX-Interface");
    delay(1000);
    Serial.println("...");

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


    // Initialize Interface connection type - to be changed to Raffaels(TM) enum
    ethTypes ethType;
    ethType = ETH;

    switch (ethType) 
    {
    case STA:
        Serial.println("Initialize as WiFi-STA");
        WiFi.begin(ssid, pwd);
        WiFi.config(ip, gateway, subnet);
        while (WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
            delay(500);
        }
        Serial.print("WiFi connected, IP = ");
        Serial.println(WiFi.localIP());
        break;
    case ETH:
        Serial.println("Initialize as ETH");
    
        WiFi.mode(WIFI_STA); // Trotzdem wegen WebServer
        SPI.begin(SCK, MISO, MOSI, SS);
        SPI.setFrequency(SPI_FREQ);
    
        Ethernet.init(SS);
        delay(1000);

        if (Ethernet.begin(mac)) { // Dynamic IP setup
            Serial.println("DHCP OK!");
        }else{
            Serial.println("Failed to configure Ethernet using DHCP");
            // Check for Ethernet hardware present
            if (Ethernet.hardwareStatus() == EthernetNoHardware) {
            Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
            while (true) {
                delay(1); // do nothing, no point running without Ethernet hardware
            }
            }
            if (Ethernet.linkStatus() == LinkOFF) {
            Serial.println("Ethernet cable is not connected.");
            }
    
            Ethernet.begin(mac, ip, gateway, gateway, subnet);
            Serial.println("STATIC OK!");

        }
        Serial.print("Local IP : ");
        Serial.println(Ethernet.localIP());
        Serial.print("Subnet Mask : ");
        Serial.println(Ethernet.subnetMask());
        Serial.print("Gateway IP : ");
        Serial.println(Ethernet.gatewayIP());
        Serial.print("DNS Server : ");
        Serial.println(Ethernet.dnsServerIP());

        Serial.println("Ethernet Successfully Initialized"); 
        break;
    default:
        Serial.println("Initialize as WiFi-AP");
        WiFi.softAP(ssid, pwd);
        WiFi.softAPConfig(ip, gateway, subnet);
        Serial.print("WiFi AP enabled, IP = ");
        Serial.println(WiFi.localIP());
        break;
    }

    delay(500);
    

    // Initialize Art-Net
    Serial.println("Initialize Art-Net...");
    artnet.begin();

    // Initialize DMX ports
    Serial.println("Initialize DMX...");
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

    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                         {
                            if (request->url() == "/config" && request->method() == HTTP_PUT) {
                                Serial.printf("[REQUEST]\t%s\r\n", (const char *)data);

                                StaticJsonDocument<256> doc;
                                deserializeJson(doc, data);
                                request->send(200);
                            } });

    delay(1000);
    server.begin();
    Serial.println("Server started!");
}

void loop()
{
    artnet.parse(); // check if artnet packet has come and execute callback
}

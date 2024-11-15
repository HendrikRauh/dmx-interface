#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif

#include <AsyncWebServer_ESP32_W5500.h>
//#include "w5500/esp32_w5500.h"
//#include <ESPAsyncWebServer.h>

#include <ArtnetWiFi.h>
#include <ArduinoJson.h>

#include "ESPDMX.h"
#include "SPI.h"

#include <SPIFFS.h>
#include "routes/config.h"

DMXESPSerial dmx1;
DMXESPSerial dmx2;

// Button
#define PIN_LED     7
#define PIN_BUTTON  5

uint8_t brightness_led = 20;
bool status_led;

hw_timer_t * timer = NULL;      //H/W timer defining (Pointer to the Structure)
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
void IRAM_ATTR onTimer() {      //Defining Inerrupt function with IRAM_ATTR for faster access
 portENTER_CRITICAL_ISR(&timerMux);
 status_led = !status_led;
 if (!status_led) {
    analogWrite(PIN_LED, brightness_led);
 } else {
    analogWrite(PIN_LED, 0);
 }
 portEXIT_CRITICAL_ISR(&timerMux);
}

// Ethernet stuff
#define ETH_SCK       36
#define ETH_SS        34
#define ETH_MOSI      35
#define ETH_MISO      37
#define ETH_INT       38
#define ETH_SPI_HOST  SPI2_HOST
#define ETH_SPI_CLOCK_MHZ       25
byte mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

enum ethTypes {TYP_AP = 1, TYP_STA = 2, TYP_ETH = 3}; // to be changed to Raffaels(TM) enum
ethTypes ethType;

AsyncWebServer server(80);

ArtnetWiFi artnet;

const uint16_t size = 512;
uint8_t data[size];

void ledBlink(int ms) {
    if(timer == NULL) {
        timer = timerBegin(0, 80, true);           	    // timer 0, prescalar: 80, UP counting
        timerAttachInterrupt(timer, &onTimer, true); 	// Attach interrupt
    }
    if(ms == 0) {
        timerAlarmDisable(timer);
        analogWrite(PIN_LED, 0);
    } else if(ms == 1) {
        timerAlarmDisable(timer);
        analogWrite(PIN_LED, brightness_led);
    } else {
        ms = ms*1000;
        timerAlarmWrite(timer, ms, true);  // Match value= 1000000 for 1 sec. delay.
        timerAlarmEnable(timer);           // Enable Timer with interrupt (Alarm Enable)
    } 
}

void setup()
{
    // Get ETH mac
    esp_read_mac(mac, ESP_MAC_ETH);

    // LED
    analogWrite(PIN_LED, brightness_led);
    delay(5000);
    ledBlink(500);

    // Serial console
    Serial.begin(9600);
    Serial.print("Start DMX-Interface");
    delay(1000);
    Serial.println("...");

    config.begin("dmx", true);

    uint8_t universe1 = config.getUChar("universe-1", 1);
    uint8_t universe2 = config.getUChar("universe-2", 1);

    Direction direction1 = static_cast<Direction>(config.getUInt("direction-1", 0));
    Direction direction2 = static_cast<Direction>(config.getUInt("direction-2", 1));

    Connection connection = static_cast<Connection>(config.getUInt("connection", WiFiAP));
    IpMethod ipMethod = static_cast<IpMethod>(config.getUInt("ip-method"), Static);

    WiFi.macAddress(mac);
    char hostname[30];
    snprintf(hostname, sizeof(hostname), "ChaosDMX-%02X%02X", mac[4], mac[5]);
    Serial.print("Hostname: ");
    Serial.println(hostname);

    String ssid = config.getString("ssid", hostname);
    String pwd = config.getString("pwd", "mbgmbgmbg");

    IPAddress defaultIp(2, mac[3], mac[4], mac[5]);
    IPAddress ip = config.getUInt("ip", defaultIp);
    IPAddress defaultSubnet(255, 255, 255, 0);
    IPAddress subnet = config.getUInt("subnet", defaultSubnet);
    IPAddress defaultGateway(192, 168, 1, 1);
    IPAddress gateway = config.getUInt("gateway", defaultGateway);

    config.end();

    // wait for serial monitor
    delay(5000);

    switch (connection)
    {
    case WiFiSta:
        Serial.println("Initialize as WiFi-Station");
        WiFi.begin(ssid, pwd);
        if (ipMethod == Static)
        {
            WiFi.config(ip, gateway, subnet);
        }
        while (WiFi.status() != WL_CONNECTED)
        {
            Serial.print(".");
            delay(500);
        }
        Serial.println("");
        Serial.print("WiFi connected, IP = ");
        Serial.println(WiFi.localIP());
        break;

    case WiFiAP:
        Serial.println("Initialize as WiFi-Access-Point");
        WiFi.softAP(ssid, pwd);
        WiFi.softAPConfig(ip, gateway, subnet);
        Serial.print("WiFi AP enabled, IP = ");
        Serial.println(WiFi.softAPIP());
        break;

    case Ethernet:
        // TODO: Initialize Interface connection type - to be changed to Raffaels(TM) enum
        ethType = TYP_ETH;

        // Button
        pinMode(PIN_BUTTON,INPUT_PULLUP);
        if(digitalRead(PIN_BUTTON) == LOW){
            ledBlink(100);
            delay(2000);
            Serial.println("Start AP-Mode");
            ethType = TYP_AP;
        }

        switch (ethType) 
        {
        case TYP_STA:
            Serial.println("Initialize as WiFi-STA");
            WiFi.begin(ssid, pwd);
            WiFi.setHostname(hostname);
            WiFi.config(ip, gateway, subnet);
            while (WiFi.status() != WL_CONNECTED) {
                Serial.print(".");
                delay(500);
            }
            Serial.print("WiFi connected, IP = ");
            Serial.println(WiFi.localIP());
            Serial.print("MAC address: ");
            Serial.println(WiFi.macAddress());
            break;
        case TYP_ETH:{
            Serial.println("Initialize as ETH");
            ESP32_W5500_onEvent();
         

            if (ETH.begin( ETH_MISO, ETH_MOSI, ETH_SCK, ETH_SS, ETH_INT, ETH_SPI_CLOCK_MHZ, ETH_SPI_HOST, mac )) { // Dynamic IP setup
            }else{
                Serial.println("Failed to configure Ethernet");
            }
            ETH.setHostname(hostname);

            //ESP32_W5500_waitForConnect();
            uint8_t timeout = 5; // in s
            Serial.print("Wait for connect");
            while (!ESP32_W5500_eth_connected && timeout > 0) { 
                delay(1000);
                timeout--;
                Serial.print(".");
            }
            Serial.println();
            if (ESP32_W5500_eth_connected) {
                Serial.println("DHCP OK!");
            } else {
                Serial.println("Set static IP");
                ETH.config(ip, gateway, subnet);
            }


            Serial.print("Local IP : ");
            Serial.println(ETH.localIP());
            Serial.print("Subnet Mask : ");
            Serial.println(ETH.subnetMask());
            Serial.print("Gateway IP : ");
            Serial.println(ETH.gatewayIP());
            Serial.print("DNS Server : ");
            Serial.println(ETH.dnsIP());
            Serial.print("MAC address : ");
            Serial.println(ETH.macAddress());

            Serial.println("Ethernet Successfully Initialized");
            break;
    }
    default:
        Serial.println("Initialize as WiFi-AP");
        WiFi.softAP(ssid, pwd);
        WiFi.softAPsetHostname(hostname);
        WiFi.softAPConfig(ip, gateway, subnet);
        Serial.print("WiFi AP enabled, IP = ");
        Serial.println(WiFi.softAPIP());
        Serial.print("MAC address: ");
        Serial.println(WiFi.softAPmacAddress());
        break;
    }

    }
    
    // Initialize DMX ports
    Serial.println("Initialize DMX...");
    dmx1.init(19, -1);

    // Initialize Art-Net
    Serial.println("Initialize Art-Net...");
    artnet.begin();

    // if Artnet packet comes to this universe, this function is called
    artnet.subscribeArtDmxUniverse(universe, [&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote)
    {
        for (size_t i = 0; i < size; ++i)
        {
            dmx1.write((i + 1), data[i]);
        }

        dmx1.update();
    });


    // if Artnet packet comes, this function is called to every universe
    artnet.subscribeArtDmx([&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {});

    if (!SPIFFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request)
              { onGetConfig(request); });

    server.on("/config", HTTP_DELETE, [](AsyncWebServerRequest *request)
              {
                config.begin("dmx", false);
                config.clear();
                config.end();
                // respond with default config
                onGetConfig(request);

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

    ledBlink(1);
}

void loop()
{
    // check if artnet packet has come and execute callback
    artnet.parse();
}

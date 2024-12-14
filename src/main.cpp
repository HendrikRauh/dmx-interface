#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif

#include <AsyncWebServer_ESP32_W5500.h>
// #include "w5500/esp32_w5500.h"
// #include <ESPAsyncWebServer.h>

#include <ArtnetWiFi.h>
#include <ArduinoJson.h>

#include "ESPDMX.h"
#include <LittleFS.h>
#include "routes/config.h"
#include "routes/networks.h"
#include "routes/status.h"

DMXESPSerial dmx1;
DMXESPSerial dmx2;

// Button
#define PIN_LED 7
#define PIN_BUTTON 5

uint8_t brightness_led = 20;
bool status_led;

hw_timer_t *timer = NULL; // H/W timer defining (Pointer to the Structure)
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
void IRAM_ATTR onTimer()
{ // Defining Inerrupt function with IRAM_ATTR for faster access
    portENTER_CRITICAL_ISR(&timerMux);
    status_led = !status_led;
    if (!status_led)
    {
        analogWrite(PIN_LED, brightness_led);
    }
    else
    {
        analogWrite(PIN_LED, 0);
    }
    portEXIT_CRITICAL_ISR(&timerMux);
}

// Ethernet stuff
#define ETH_SCK 36
#define ETH_SS 34
#define ETH_MOSI 35
#define ETH_MISO 37
#define ETH_INT 38
#define ETH_SPI_HOST SPI2_HOST
#define ETH_SPI_CLOCK_MHZ 25
byte mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

AsyncWebServer server(80);

ArtnetWiFi artnet;

String broadcastIp;
uint8_t universe1;
uint8_t universe2;
Direction direction1;
Direction direction2;
// const uint16_t size = 512;
// uint8_t data[DMXCHANNELS];

void ledBlink(int ms)
{
    if (timer == NULL)
    {
        timer = timerBegin(0, 80, true);             // timer 0, prescalar: 80, UP counting
        timerAttachInterrupt(timer, &onTimer, true); // Attach interrupt
    }
    if (ms == 0)
    {
        timerAlarmDisable(timer);
        analogWrite(PIN_LED, 0);
    }
    else if (ms == 1)
    {
        timerAlarmDisable(timer);
        analogWrite(PIN_LED, brightness_led);
    }
    else
    {
        ms = ms * 1000;
        timerAlarmWrite(timer, ms, true); // Match value= 1000000 for 1 sec. delay.
        timerAlarmEnable(timer);          // Enable Timer with interrupt (Alarm Enable)
    }
}

void setup()
{
    Serial.begin(9600);

    // Get ETH mac
    esp_read_mac(mac, ESP_MAC_ETH);

    // LED
    config.begin("dmx", true);
    brightness_led = config.getUInt("led-brightness", DEFAULT_LED_BRIGHTNESS);
    config.end();
    analogWrite(PIN_LED, brightness_led);

    // Button
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    if (digitalRead(PIN_BUTTON) == LOW)
    {
        ledBlink(100);
        unsigned long startTime = millis();
        while (digitalRead(PIN_BUTTON) == LOW && (millis() - startTime <= 3000))
        {
        }
        if (digitalRead(PIN_BUTTON) == LOW)
        {
            ledBlink(1);
            Serial.println("Reset config");
            config.begin("dmx", false);
            config.clear();
            config.end();
            delay(2000);
        }
    }

    ledBlink(500);

    // wait for serial monitor
    delay(5000);
    Serial.println("Starting DMX-Interface...");

    config.begin("dmx", true);

    universe1 = config.getUInt("universe-1", DEFAULT_UNIVERSE1);
    universe2 = config.getUInt("universe-2", DEFAULT_UNIVERSE2);

    direction1 = static_cast<Direction>(config.getUInt("direction-1", DEFAULT_DIRECTION1));
    direction2 = static_cast<Direction>(config.getUInt("direction-2", DEFAULT_DIRECTION2));

    Serial.print("Port A: Universe ");
    Serial.print(universe1);
    Serial.print(" ");
    Serial.println((direction1 == Input) ? "DMX -> Art-Net" : "Art-Net -> DMX");

    Serial.print("Port B: Universe ");
    Serial.print(universe2);
    Serial.print(" ");
    Serial.println((direction2 == Input) ? "DMX -> Art-Net" : "Art-Net -> DMX");

    Connection connection = static_cast<Connection>(config.getUInt("connection", DEFAULT_CONNECTION));
    IpMethod ipMethod = static_cast<IpMethod>(config.getUInt("ip-method"), DEFAULT_IP_METHOD);

    char hostname[30];
    snprintf(hostname, sizeof(hostname), "ChaosDMX-%02X%02X", mac[4], mac[5]);
    DEFAULT_SSID = hostname;
    Serial.print("Hostname: ");
    Serial.println(hostname);

    String ssid = config.getString("ssid", DEFAULT_SSID);
    String pwd = config.getString("password", DEFAULT_PASSWORD);

    // Default IP as defined in standard https://art-net.org.uk/downloads/art-net.pdf, page 13
    IPAddress ip = config.getUInt("ip", DEFAULT_IP);
    IPAddress subnet = config.getUInt("subnet", DEFAULT_SUBNET);
    IPAddress gateway = config.getUInt("gateway", NULL);

    config.end();

    switch (connection)
    {
    case WiFiSta:
        Serial.println("Initialize as WiFi Station");
        WiFi.setHostname(hostname);
        WiFi.begin(ssid, pwd);
        Serial.println("SSID: " + ssid + ", pwd: " + pwd);
        if (ipMethod == Static)
        {
            WiFi.config(ip, gateway, subnet);
            Serial.println("IP: " + ip.toString() + ", gateway: " + gateway + ", subnet: " + subnet);
        }
        while (WiFi.status() != WL_CONNECTED)
        {
            Serial.print(".");
            delay(500);
        }
        broadcastIp = String(WiFi.broadcastIP().toString().c_str());
        Serial.println("");
        Serial.print("WiFi connected, IP = ");
        Serial.println(WiFi.localIP());
        Serial.print("MAC address: ");
        Serial.println(WiFi.macAddress());
        break;

    case Ethernet:
    {
        Serial.println("Initialize as ETH");
        ESP32_W5500_onEvent();

        if (ETH.begin(ETH_MISO, ETH_MOSI, ETH_SCK, ETH_SS, ETH_INT, ETH_SPI_CLOCK_MHZ, ETH_SPI_HOST, mac))
        { // Dynamic IP setup
        }
        else
        {
            Serial.println("Failed to configure Ethernet");
        }
        ETH.setHostname(hostname);

        // ESP32_W5500_waitForConnect();
        uint8_t timeout = 5; // in s
        Serial.print("Wait for connect");
        while (!ESP32_W5500_eth_connected && timeout > 0)
        {
            delay(1000);
            timeout--;
            Serial.print(".");
        }
        Serial.println();
        if (ESP32_W5500_eth_connected)
        {
            Serial.println("DHCP OK!");
        }
        else
        {
            Serial.println("Set static IP");
            ETH.config(ip, gateway, subnet);
        }
        broadcastIp = ETH.broadcastIP().toString();

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
        Serial.println("Initialize as WiFi AccessPoint");
        WiFi.softAPsetHostname(hostname);
        WiFi.softAP(ssid, pwd);
        // AP always with DHCP
        // WiFi.softAPConfig(ip, gateway, subnet);
        broadcastIp = WiFi.softAPBroadcastIP().toString();
        Serial.print("WiFi AP enabled, IP = ");
        Serial.println(WiFi.softAPIP());
        Serial.print("MAC address: ");
        Serial.println(WiFi.softAPmacAddress());
        break;
    }

    // Initialize DMX ports
    Serial.println("Initialize DMX...");
    dmx1.init(21, 33, Serial0);
    dmx2.init(17, 18, Serial1);

    // Initialize Art-Net
    Serial.println("Initialize Art-Net...");
    artnet.begin();

    // if Artnet packet comes to this universe, this function is called
    if (direction1 == Output)
    {
        artnet.subscribeArtDmxUniverse(universe1, [&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote)
                                       {
            for (size_t i = 0; i < size; ++i)
            {
                dmx1.write((i + 1), data[i]);
            }
            dmx1.update(); });
    }

    if (direction2 == Output)
    {
        artnet.subscribeArtDmxUniverse(universe2, [&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote)
                                       {
            for (size_t i = 0; i < size; ++i)
            {
                dmx2.write((i + 1), data[i]);
            }
            dmx2.update(); });
    }

    // if Artnet packet comes, this function is called to every universe
    // artnet.subscribeArtDmx([&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote) {});

    if (!LittleFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

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

    server.on("/networks", HTTP_GET, [](AsyncWebServerRequest *request)
              { onGetNetworks(request); });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
              { onGetStatus(request); });

    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                         {
                            if (request->url() == "/config" && request->method() == HTTP_PUT) {
                                onPutConfig(request, data, len, index, total);
                                Serial.println("restarting ESP...");
                                ESP.restart();
                            } });

    delay(1000);
    server.begin();
    Serial.println("Server started!");

    // scan networks and cache them
    WiFi.scanNetworks(true);

    ledBlink(1);
}

void loop()
{
    // check if artnet packet has come and execute callback
    artnet.parse();

    // Receive Callback/INT currently not implemented
    /*if (direction1 == Input) {
        artnet.setArtDmxData(dmx1.readAll(), DMXCHANNELS);
        artnet.streamArtDmxTo(broadcastIp, universe1);
    }

    if (direction2 == Input) {
        artnet.setArtDmxData(dmx2.readAll(), DMXCHANNELS);
        artnet.streamArtDmxTo(broadcastIp, universe2);
    }*/
}

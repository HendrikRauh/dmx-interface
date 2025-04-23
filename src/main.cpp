#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
// #include <USB.h>
// #include "USBCDC.h"
#include "driver/temp_sensor.h"
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif

#include <AsyncWebServer_ESP32_W5500.h>
// #include "w5500/esp32_w5500.h"
// #include <ESPAsyncWebServer.h>

#include <ArtnetWiFi.h>
#include <ArduinoJson.h>

// #include "ESPDMX.h"
#include <Arduino.h>
#include <esp_dmx.h>

#include <LittleFS.h>
#include "websocket.h"
#include "routes/config.h"
#include "routes/networks.h"
#include "routes/status.h"

dmx_port_t dmx1 = DMX_NUM_0; // for esp32s2
dmx_port_t dmx2 = DMX_NUM_1;
byte dmx1_data[DMX_PACKET_SIZE];
byte dmx2_data[DMX_PACKET_SIZE];

// Button
#define PIN_LED 7
#define PIN_BUTTON 5

uint8_t brightness_led = 20;
bool status_led;

// Ethernet stuff
#define ETH_SCK 36
#define ETH_SS 34
#define ETH_MOSI 35
#define ETH_MISO 37
#define ETH_INT 38
#define ETH_SPI_CLOCK_MHZ 25
byte mac[6];

AsyncWebServer server(80);

ArtnetWiFi artnet;

String broadcastIp;
uint8_t universe1;
uint8_t universe2;
Direction direction1;
Direction direction2;

enum class Status
{
    Starting,
    Resetting,
    Normal,
    Warning,
    Critical
};

Status status = Status::Starting;
struct BlinkingConfig
{
    int interval_ms;
    bool is_blinking;
    int brightness;
};

BlinkingConfig getBlinkingConfig(Status status)
{
    switch (status)
    {
    case Status::Starting:
        return {500, true, brightness_led};
    case Status::Resetting:
        return {100, true, brightness_led};
    case Status::Normal:
        return {0, false, brightness_led};
    case Status::Warning:
        return {500, true, 255};
    case Status::Critical:
        return {100, true, 255};
    default:
        return {0, false, 0};
    }
}

void updateLed()
{
    BlinkingConfig led_config = getBlinkingConfig(status);
    if (!led_config.is_blinking)
    {
        analogWrite(PIN_LED, led_config.brightness);
        return;
    }
    else
    {
        if (millis() % led_config.interval_ms < led_config.interval_ms / 2) //? blinks twice as fast?! & directly based on millis
        {
            analogWrite(PIN_LED, led_config.brightness);
        }
        else
        {
            analogWrite(PIN_LED, 0);
        }
    }
}

void onButtonPress()
{
    config.begin("dmx", true);
    ButtonAction action = static_cast<ButtonAction>(config.getUInt("button-action", DEFAULT_BUTTON_ACTION));
    config.end();

    switch (action)
    {
    case ResetConfig:
        config.begin("dmx", false);
        config.clear();
        config.end();

        ESP.restart();
        break;

    case Restart:
        config.begin("dmx", false);
        config.putBool("restart-via-btn", true);
        config.end();

        ESP.restart();
        break;
    case None:
        // do nothing
        break;
    }
}

void setup()
{
    Serial.begin(9600);

    // Get ETH mac
    delay(1000);

    esp_efuse_mac_get_default(mac);

    esp_read_mac(mac, ESP_MAC_ETH);
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x ESP MAC ETH\n",
                  mac[0], mac[1], mac[2],
                  mac[3], mac[4], mac[5]);

    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x ESP MAC SOFTAP\n",
                  mac[0], mac[1], mac[2],
                  mac[3], mac[4], mac[5]);

    esp_read_mac(mac, ESP_MAC_WIFI_STA); // ESP_MAC_BASE
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x ESP MAC BASE\n",
                  mac[0], mac[1], mac[2],
                  mac[3], mac[4], mac[5]);

    // LED
    config.begin("dmx", true);
    brightness_led = config.getUInt("led-brightness", DEFAULT_LED_BRIGHTNESS);
    bool restartViaButton = config.getBool("restart-via-btn", false);
    config.end();
    analogWrite(PIN_LED, brightness_led);

    // Button
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    if (digitalRead(PIN_BUTTON) == LOW && !restartViaButton)
    {
        status = Status::Resetting;
        unsigned long startTime = millis();
        while (digitalRead(PIN_BUTTON) == LOW && (millis() - startTime <= 3000))
        {
        }
        if (digitalRead(PIN_BUTTON) == LOW)
        {
            Serial.println("Reset config");
            config.begin("dmx", false);
            config.clear();
            config.end();
            status = Status::Normal;
            delay(2000);
        }
    }

    config.begin("dmx", false);
    config.putBool("restart-via-btn", false);
    config.end();

    status = Status::Starting;

    attachInterrupt(PIN_BUTTON, onButtonPress, FALLING);

    // wait for serial monitor
    delay(5000);
    Serial.println("Starting DMX-Interface...");

    config.begin("dmx", true);

    universe1 = config.getUInt("universe-1", DEFAULT_UNIVERSE1);
    universe2 = config.getUInt("universe-2", DEFAULT_UNIVERSE2);

    direction1 = static_cast<Direction>(config.getUInt("direction-1", DEFAULT_DIRECTION1));
    direction2 = static_cast<Direction>(config.getUInt("direction-2", DEFAULT_DIRECTION2));

    Serial.printf("Port A: Universe %d %s\n", universe1, (direction1 == Input) ? "DMX -> Art-Net" : "Art-Net -> DMX");
    Serial.printf("Port B: Universe %d %s\n", universe2, (direction2 == Input) ? "DMX -> Art-Net" : "Art-Net -> DMX");

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
    IPAddress gateway = config.getUInt("gateway", 0);

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
        Serial.print("Broadcast IP: ");
        Serial.println(broadcastIp);
        break;

    case Ethernet:
    {
        Serial.println("Initialize as ETH");
        ESP32_W5500_onEvent();

        if (ETH.begin(ETH_MISO, ETH_MOSI, ETH_SCK, ETH_SS, ETH_INT, ETH_SPI_CLOCK_MHZ, SPI2_HOST, mac))
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
        Serial.print("Broadcast IP: ");
        Serial.println(broadcastIp);
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
        Serial.print("Broadcast IP: ");
        Serial.println(broadcastIp);
        break;
    }

    // Initialize DMX ports
    Serial.println("Initialize DMX...");

#ifdef CONFIG_IDF_TARGET_ESP32S2

    dmx_config_t dmx_config = DMX_CONFIG_DEFAULT;
    dmx_personality_t personalities[] = {};
    int personality_count = 0;

    dmx_driver_install(dmx1, &dmx_config, personalities, personality_count);
    dmx_set_pin(dmx1, 21, 33, -1);
    dmx_driver_install(dmx2, &dmx_config, personalities, personality_count);
    dmx_set_pin(dmx2, 17, 18, -1);

    Serial.printf("DMX driver 1 installed: %d\n", dmx_driver_is_installed(dmx1));
    Serial.printf("DMX driver 2 installed: %d\n", dmx_driver_is_installed(dmx2));

    Serial.printf("DMX driver 1 enabled: %d\n", dmx_driver_is_enabled(dmx1));
    Serial.printf("DMX driver 2 enabled: %d\n", dmx_driver_is_enabled(dmx2));

#else
    dmx1.init(21, 33, Serial1);
    dmx2.init(17, 18, Serial2);
#endif

    // Initialize Art-Net
    Serial.println("Initialize Art-Net...");
    artnet.begin();

    // if Artnet packet comes to this universe, this function is called
    if (direction1 == Output)
    {
        artnet.subscribeArtDmxUniverse(universe1, [&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote)
                                       {
            dmx_write_offset(dmx1, 1, data, size);
            dmx_send(dmx1);
            dmx_wait_sent(dmx1, DMX_TIMEOUT_TICK); });
    }

    if (direction2 == Output)
    {
        artnet.subscribeArtDmxUniverse(universe2, [&](const uint8_t *data, uint16_t size, const ArtDmxMetadata &metadata, const ArtNetRemoteInfo &remote)
                                       {
            dmx_write_offset(dmx2, 1, data, size);
            dmx_send(dmx2);
            dmx_wait_sent(dmx2, DMX_TIMEOUT_TICK); });
    }

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

    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                         {
                            if (request->url() == "/config" && request->method() == HTTP_PUT) {
                                onPutConfig(request, data, len, index, total);
                                Serial.println("restarting ESP...");
                                ESP.restart();
                            } });

    initWebSocket(&server);

    server.begin();
    Serial.println("Server started!");

    // scan networks and cache them
    WiFi.scanNetworks(true);

    status = Status::Normal;

    // Internal temperature RP2040
    /*float tempC = analogReadTemp(); // Get internal temperature
    Serial.print("Temperature Celsius (ºC): ");
    Serial.println(tempC);*/
    // Internal temperature ESP32 https://www.espboards.dev/blog/esp32-inbuilt-temperature-sensor/
    Serial.print("Temperature: ");
    float result = 0;
    temp_sensor_read_celsius(&result);
    Serial.print(result);
    Serial.println(" °C");

    Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
    Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram());
    Serial.printf("ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
    Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
}

void transmitDmxToArtnet(dmx_port_t dmxPort, byte *dmx_data, uint8_t artnetUniverse)
{
    /* We need a place to store information about the DMX packets we receive. We
        will use a dmx_packet_t to store that packet information.  */
    dmx_packet_t dmx_packet;

    // check if there's a new DMX packet
    if (dmx_receive(dmxPort, &dmx_packet, 0))
    {
        /* We should check to make sure that there weren't any DMX errors. */
        if (!dmx_packet.err)
        {
            /* Don't forget we need to actually read the DMX data into our buffer so
                that we can print it out. */
            dmx_read_offset(dmxPort, 1, dmx_data, dmx_packet.size);
            artnet.sendArtDmx(broadcastIp, artnetUniverse, dmx_data, dmx_packet.size);
        }
        else
        {
            /* Oops! A DMX error occurred! Don't worry, this can happen when you first
                connect or disconnect your DMX devices. If you are consistently getting
                DMX errors, then something may have gone wrong with your code or
                something is seriously wrong with your DMX transmitter. */
            Serial.printf("A DMX error occurred on port %d.\n", dmxPort);
        }
    }
}

void loop()
{
    // only check for artnet packets if we expect to receive data
    if (direction1 == Output || direction2 == Output)
    {
        // check if artnet packet has come and execute callback
        artnet.parse();
    }

    if (direction1 == Input)
    {
        transmitDmxToArtnet(dmx1, dmx1_data, universe1);
    }

    if (direction2 == Input)
    {
        transmitDmxToArtnet(dmx2, dmx2_data, universe2);
    }

    webSocketLoop();
    updateLed();
}

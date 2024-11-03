#include "config.h"
#include <stdexcept>
#include <ArduinoJson.h>

Preferences config;

#pragma region Utility

uint32_t parseIp(String str)
{
    const int size = 4;

    String ipStrings[size];
    uint8_t ipIndex = 0;

    for (int i = 0; i < str.length(); i++)
    {
        if (str[i] == '.')
        {
            ipIndex++;
            continue;
        }
        ipStrings[ipIndex] += str[i];
    }

    String ip = "";
    for (int i = 0; i < size; i++)
    {
        String paddedString = ipStrings[i];
        while (paddedString.length() < 3)
        {
            paddedString = "0" + paddedString;
        }
        ip.concat(paddedString);
    }

    Serial.println("ip string: " + ip);
    return atoi(ip.c_str());
}

IpMethod parseIpMethod(uint8_t ipMethod)
{
    if (ipMethod > 0 || ipMethod < IP_METHOD_SIZE)
    {
        return static_cast<IpMethod>(ipMethod);
    }

    throw ::std::invalid_argument("Invalid IP method value" + ipMethod);
}

Connection parseConnection(uint8_t connection)
{
    if (connection > 0 || connection < CONNECTION_SIZE)
    {
        return static_cast<Connection>(connection);
    }

    throw ::std::invalid_argument("Invalid connection value: " + connection);
}

Direction parseDirection(uint8_t direction)
{
    if (direction > 0 || direction < DIRECTION_SIZE)
    {
        return static_cast<Direction>(direction);
    }

    throw ::std::invalid_argument("Invalid direction value: " + direction);
}

#pragma endregion

void onGetConfig(AsyncWebServerRequest *request)
{
    config.begin("dmx", true);

    IPAddress defaultIp(192, 168, 1, 201);
    IPAddress ip = config.getUInt("ip", defaultIp);

    IPAddress defaultSubnet(255, 255, 255, 0);
    IPAddress subnet = config.getUInt("subnet", defaultSubnet);

    IPAddress defaultGateway(192, 168, 1, 1);
    IPAddress gateway = config.getUInt("gateway", defaultGateway);

    JsonDocument doc;
    doc["connection"] = config.getUInt("connection", WiFiSta);
    doc["ssid"] = config.getString("ssid", "artnet");
    doc["password"] = config.getString("password", "mbgmbgmbg");
    doc["ip-method"] = config.getUInt("ip-method"), Static;
    doc["ip"] = ip.toString();
    doc["subnet"] = subnet.toString();
    doc["gateway"] = gateway.toString();
    doc["universe-1"] = config.getUChar("universe-1", 1);
    doc["direction-1"] = config.getUInt("direction-1", Output);
    doc["universe-2"] = config.getUChar("universe-2", 1);
    doc["direction-2"] = config.getUInt("direction-2", Input);

    config.end();

    String jsonString;
    serializeJson(doc, jsonString);

    request->send(200, "application/json", jsonString);
}

void onPutConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    Serial.printf("[REQUEST]\t%s\r\n", (const char *)data);

    JsonDocument doc;
    deserializeJson(doc, data);

    try
    {
        config.begin("dmx", false);

        IpMethod ipMethod = parseIpMethod(doc["ip-method"].as<uint8_t>());
        config.putUInt("ip-method", ipMethod);

        if (ipMethod == Static)
        {
            IPAddress ipAddress;
            ipAddress.fromString(doc["ip"].as<String>());
            config.putUInt("ip", ipAddress);

            IPAddress subnet;
            subnet.fromString(doc["subnet"].as<String>());
            config.putUInt("subnet", subnet);

            IPAddress gateway;
            gateway.fromString(doc["gateway"].as<String>());
            config.putUInt("gateway", gateway);
        }

        Connection connection = parseConnection(doc["connection"].as<uint8_t>());
        config.putUInt("connection", connection);

        if (connection == WiFiSta || connection == WiFiAP)
        {
            config.putString("ssid", doc["ssid"].as<String>());
            config.putString("password", doc["password"].as<String>());
        }

        Direction direction1 = parseDirection(doc["direction-1"].as<uint8_t>());
        config.putInt("direction-1", direction1);

        Direction direction2 = parseDirection(doc["direction-2"].as<uint8_t>());
        config.putInt("direction-2", direction2);

        config.end();

        request->send(200);
    }
    catch (::std::invalid_argument &e)
    {
        config.end();
        request->send(400, "text/plain", e.what());
    }
}

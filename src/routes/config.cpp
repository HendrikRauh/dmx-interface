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

void onGetConfig(
    Connection connection,
    String ssid,
    String pwd,
    IpMethod ipMethod,
    uint32_t ip,
    uint32_t subnet,
    uint32_t gateway,
    uint8_t universe1,
    Direction direction1,
    uint8_t universe2,
    Direction direction2,
    AsyncWebServerRequest *request)
{
    JsonDocument doc;

    IPAddress ipAddr = ip;
    String ipString = ipAddr.toString();

    ipAddr = subnet;
    String subnetString = ipAddr.toString();

    ipAddr = gateway;
    String gatewayString = ipAddr.toString();

    doc["connection"] = connection;
    doc["ssid"] = ssid;
    doc["password"] = pwd;
    doc["ip-method"] = ipMethod;
    doc["ip"] = ipString;
    doc["subnet"] = subnetString;
    doc["gateway"] = gatewayString;
    doc["universe-1"] = universe1;
    doc["direction-1"] = direction1;
    doc["universe-2"] = universe2;
    doc["direction-2"] = direction2;

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

        request->send(200);
    }
    catch (::std::invalid_argument &e)
    {
        request->send(400, "text/plain", e.what());
    }
}

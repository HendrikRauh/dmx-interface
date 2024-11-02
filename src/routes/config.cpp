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

IpMethod parseIpMethod(String ipMethod)
{
    if (ipMethod == "static")
    {
        return Static;
    }

    if (ipMethod == "dhcp")
    {
        return DHCP;
    }

    throw ::std::invalid_argument("Invalid IP method value");
}

Connection parseConnection(String connection)
{
    if (connection == "wifi-sta")
    {
        return WiFiSta;
    }
    if (connection == "wifi-ap")
    {
        return WiFiAP;
    }
    if (connection == "ethernet")
    {
        return Ethernet;
    }

    throw ::std::invalid_argument("Invalid connection value");
}

Direction parseDirection(uint8_t direction)
{
    if (direction == 0)
    {
        return Output;
    }
    if (direction == 1)
    {
        return Input;
    }

    throw ::std::invalid_argument("Invalid direction value: " + direction);
}

#pragma endregion

void onGetConfig(String ssid, String pwd, uint32_t ip, uint8_t universe1, Direction direction1, uint8_t universe2, Direction direction2, AsyncWebServerRequest *request)
{
    JsonDocument doc;

    IPAddress ipAddr = ip;
    String ipString = ipAddr.toString();

    doc["ssid"] = ssid;
    doc["password"] = pwd;
    doc["ip"] = ipString;
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
        IpMethod ipMethod = parseIpMethod(doc["ip-method"].as<String>());
        config.putUInt("ip-method", ipMethod);

        if (ipMethod == Static)
        {
            String ipString = doc["ip"].as<String>();
            IPAddress ipAddress;
            ipAddress.fromString(ipString);
            config.putUInt("ip", ipAddress);
        }

        Connection connection = parseConnection(doc["connection"].as<String>());
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

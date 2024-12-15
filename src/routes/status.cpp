#include "status.h"

int8_t getWiFiStrength()
{
    try
    {
        return WiFi.RSSI();
    }
    catch (...)
    {
        return 0;
    }
}

void onGetStatus(AsyncWebServerRequest *request)
{
    JsonDocument doc;

    doc["uptime"] = millis();
    doc["chip"]["model"] = ESP.getChipModel();
    doc["chip"]["mac"] = ESP.getEfuseMac();
    doc["chip"]["revision"] = ESP.getChipRevision();
    doc["chip"]["cpuFreqMHz"] = ESP.getCpuFreqMHz();
    doc["chip"]["cycleCount"] = ESP.getCycleCount();
    doc["sdkVersion"] = ESP.getSdkVersion();
    doc["sketch"]["size"] = ESP.getSketchSize();
    doc["sketch"]["md5"] = ESP.getSketchMD5();
    doc["heap"]["free"] = ESP.getFreeHeap();
    doc["heap"]["total"] = ESP.getHeapSize();
    doc["psram"]["free"] = ESP.getFreePsram();
    doc["psram"]["total"] = ESP.getPsramSize();
    doc["connection"]["signalStrength"] = getWiFiStrength();

    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
}
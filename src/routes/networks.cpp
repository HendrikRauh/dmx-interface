#include "networks.h"

void onGetNetworks(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    int numberOfNetworks = WiFi.scanComplete();
    if (numberOfNetworks == WIFI_SCAN_FAILED)
    {
        WiFi.scanNetworks(true);
    }
    else if (numberOfNetworks)
    {
        for (int i = 0; i < numberOfNetworks; ++i)
        {
            array.add(WiFi.SSID(i));
        }
        WiFi.scanDelete();
        if (WiFi.scanComplete() == WIFI_SCAN_FAILED)
        {
            WiFi.scanNetworks(true);
        }
    }

    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
}

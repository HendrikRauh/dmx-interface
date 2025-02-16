#include "channels.h"

void onGetChannels(AsyncWebServerRequest *request, DMXESPSerial dmx1, DMXESPSerial dmx2)
{
    JsonDocument doc;

    for (int channel = 1; channel <= DMXCHANNELS; channel++)
    {
        uint8_t value = dmx1.read(channel);
        if (value != 0)
        {
            doc["dmx1"][String(channel)] = value;
        }
    }

    for (int channel = 1; channel <= DMXCHANNELS; channel++)
    {
        uint8_t value = dmx2.read(channel);
        if (value != 0)
        {
            doc["dmx2"][String(channel)] = value;
        }
    }

    String jsonBuffer;
    serializeJson(doc, jsonBuffer);

    request->send(200, "application/json", jsonBuffer);
}
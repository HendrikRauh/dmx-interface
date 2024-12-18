#include "channels.h"

void onGetChannels(AsyncWebServerRequest *request, DMXESPSerial dmx1, DMXESPSerial dmx2)
{
    JsonDocument doc;

    for (int channel = 1; channel <= DMXCHANNELS; channel++)
    {
        doc["dmx1"][channel] = dmx1.read(channel);
    }

    for (int channel = 1; channel <= DMXCHANNELS; channel++)
    {
        doc["dmx2"][channel] = dmx2.read(channel);
    }

    String jsonBuffer;
    serializeJson(doc, jsonBuffer);

    request->send(200, "application/json", jsonBuffer);
}
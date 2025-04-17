#include "websocket.h"

AsyncWebSocket ws("/ws");

long webSocketLastUpdate = 0;
const int WS_UPDATE_INTERVAL = 10 * 1000; // 10 seconds

String buildStatusString()
{
    JsonDocument doc;
    doc["type"] = "status";
    doc["data"] = buildStatusJson();

    String jsonString = "";
    serializeJson(doc, jsonString);
    return jsonString;
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        Serial.printf("[WS] Client %u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        // directly send status to client
        ws.text(client->id(), buildStatusString());
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("[WS] Client %u disconnected\n", client->id());
        break;
    case WS_EVT_DATA:
        Serial.printf("[WS] Data received from client %u: %s\n", client->id(), (char *)data);
        break;
    default:
        break;
    }
}

void webSocketLoop()
{
    if (millis() - webSocketLastUpdate > WS_UPDATE_INTERVAL)
    {
        ws.textAll(buildStatusString());
        webSocketLastUpdate = millis();
    }
}

void initWebSocket(AsyncWebServer *server)
{
    ws.onEvent(onEvent);
    server->addHandler(&ws);
}

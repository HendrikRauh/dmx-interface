#include "log.h"

void setupLogger()
{
    Serial.begin(9600);
    while (!Serial)
    {
        // updateLed();
    }
    // delay(5000);
    Serial.println("Logger initialized");
}

void writeLogEntry(const log_level level, const log_tag tag, const char *message, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, message);
    vsnprintf(buffer, sizeof(buffer), message, args);
    va_end(args);

    Serial.printf("[%d] [%d] \t %s\n", level, tag, buffer);
}
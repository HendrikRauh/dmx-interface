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
    va_list args;
    va_start(args, message);
    int size = vsnprintf(nullptr, 0, message, args);
    va_end(args);

    char *buffer = new char[size + 1];
    va_start(args, message);
    vsnprintf(buffer, size + 1, message, args);
    Serial.printf("[%d] [%d] \t %s\n", level, tag, buffer);
    delete[] buffer;
}

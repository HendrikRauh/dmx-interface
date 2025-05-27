#include "log.h"

const char *getLogLevelString(log_level level)
{
    switch (level)
    {
    case EMERGENCY:
        return "EMRG";
    case ALERT:
        return "ALRT";
    case CRITICAL:
        return "CRIT";
    case ERROR:
        return "EROR";
    case WARNING:
        return "WRNG";
    case NOTICE:
        return "NOTI";
    case INFO:
        return "INFO";
    case DEBUG:
        return "DEBG";
    default:
        return "UKWN";
    }
}

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

void writeLogEntry(const log_level level, const char *tag, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    int size = vsnprintf(nullptr, 0, message, args);
    va_end(args);

    char *buffer = new char[size + 1];
    va_start(args, message);
    vsnprintf(buffer, size + 1, message, args);
    Serial.printf("[%s][%s](%d)> %s\n", getLogLevelString(level), tag, millis(), buffer);
    delete[] buffer;
}

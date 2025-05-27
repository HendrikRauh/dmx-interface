#include <Arduino.h>

enum log_level
{
    INFO,
    DEBUG,
    VERBOSE,
    ERROR,
    CRITICAL,
    WARNING,
    NOTICE,
    ALERT,
    EMERGENCY
};

enum log_tag
{
    SYSTEM,
    DMX,
    ARTNET,
    SERVER,
    WIFI,
    ETHERNET,
    CONFIG
};

void setupLogger();
void writeLogEntry(const log_level level, const log_tag tag, const char *message, ...);
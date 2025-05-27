#include <Arduino.h>

enum log_level
{
    EMERGENCY,
    ALERT,
    CRITICAL,
    ERROR,
    WARNING,
    NOTICE,
    INFO,
    DEBUG
};

namespace tag
{
    const char *const SYSTEM = "SYS";
    const char *const DMX = "DMX";
    const char *const ARTNET = "ART";
    const char *const SERVER = "SVR";
    const char *const WIFI = "WIF";
    const char *const ETHERNET = "ETH";
    const char *const CONFIG = "CNF";
}

void setupLogger();
void writeLogEntry(const log_level level, const char *tag, const char *message, ...);
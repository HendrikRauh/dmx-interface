#include "log.h"
#include <Elog.h>

void Log::setup()
{
    Logger.registerSerial(SYSTEM, 10, "SYSTEM");
    Logger.registerSerial(DMX, 10, "DMX");
    Logger.registerSerial(ARTNET, 10, "ARTNET");
    Logger.registerSerial(WIFI, 10, "WIFI");
    Logger.registerSerial(ETHERNET, 10, "ETHERNET");
    Logger.registerSerial(SERVER, 10, "SERVER");
    Logger.registerSerial(CONFIG, 10, "CONFIG");
};

void Log::info(int tag, String format, ...)
{
    Logger.info(tag, format, va_list());
}

void Log::debug(int tag, String format, ...)
{
    Logger.debug(tag, format, va_list());
}

void Log::verbose(int tag, String format, ...)
{
    Logger.verbose(tag, format, va_list());
}

void Log::error(int tag, String format, ...)
{
    Logger.error(tag, format, va_list());
}

void Log::critical(int tag, String format, ...)
{
    Logger.critical(tag, format, va_list());
}

void Log::warning(int tag, String format, ...)
{
    Logger.warning(tag, format, va_list());
}

void Log::notice(int tag, String format, ...)
{
    Logger.notice(tag, format, va_list());
}

void Log::alert(int tag, String format, ...)
{
    Logger.alert(tag, format, va_list());
}

void Log::emergency(int tag, String format, ...)
{
    Logger.emergency(tag, format, va_list());
}
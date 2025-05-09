#include <Arduino.h>

#define SYSTEM 0
#define DMX 1
#define ARTNET 2
#define SERVER 3
#define WIFI 4
#define ETHERNET 5
#define CONFIG 6

class Log
{
public:
    void setup();
    void info(int tag, String format, ...);
    void debug(int tag, String format, ...);
    void verbose(int tag, String format, ...);
    void error(int tag, String format, ...);
    void critical(int tag, String format, ...);
    void warning(int tag, String format, ...);
    void notice(int tag, String format, ...);
    void alert(int tag, String format, ...);
    void emergency(int tag, String format, ...);
};
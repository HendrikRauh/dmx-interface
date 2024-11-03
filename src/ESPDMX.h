#include <inttypes.h>

#ifndef ESPDMX_h
#define ESPDMX_h

#define DMXSPEED 250000
#define DMXFORMAT SERIAL_8N2
#define BREAKSPEED 83333
#define BREAKFORMAT SERIAL_8N1
#define SERIALPORT Serial0
#define DMXCHANNELS 512

class DMXESPSerial
{
public:
    int sendPin;
    int recvPin;
    bool dmxStarted;
    uint8_t dmxDataStore[DMXCHANNELS + 1];

    void init(int pinSend, int pinRecv);
    uint8_t read(int Channel);
    void write(int channel, uint8_t value);
    void update();
    void end();
};

#endif

#include <inttypes.h>
#include <HardwareSerial.h>

#ifndef ESPDMX_h
#define ESPDMX_h

#define DMXSPEED 250000
#define DMXFORMAT SERIAL_8N2
#define BREAKSPEED 83333
#define BREAKFORMAT SERIAL_8N1
#define DMX_DEFAULT_PORT Serial0
#define DMX_DEFAULT_TX 21
#define DMX_DEFAULT_RX 33
#define DMXCHANNELS 512

class DMXESPSerial
{
public:
    int sendPin;
    int recvPin;
    HardwareSerial *port;
    bool dmxStarted;
    uint8_t dmxDataStore[DMXCHANNELS + 1];

    void init(int pinSend, int pinRecv, HardwareSerial& port);
    uint8_t read(int channel);
    uint8_t* readAll();
    void write(int channel, uint8_t value);
    void update();
    void end();

private:
    // Member variables
    HardwareSerial* _Serial;
};

#endif

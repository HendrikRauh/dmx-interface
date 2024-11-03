// - - - - -
// ESPDMX - A Arduino library for sending and receiving DMX using the builtin serial hardware port.
// ESPDMX.cpp: Library implementation file
//
// Copyright (C) 2015  Rick <ricardogg95@gmail.com>
// This work is licensed under a GNU style license.
//
// Last change: Hendrik Rauh <https://github.com/Hendrik Rauh>
//
// Documentation and samples are available at https://github.com/Rickgg/ESP-Dmx
// - - - - -

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
    bool started;
    uint8_t dmxDataStore[DMXCHANNELS + 1];

    void init(int pinSend, int pinRecv);
    uint8_t read(int Channel);
    void write(int channel, uint8_t value);
    void update();
    void end();
};

#endif

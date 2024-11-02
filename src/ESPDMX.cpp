// - - - - -
// ESPDMX - A Arduino library for sending and receiving DMX using the builtin serial hardware port.
// ESPDMX.cpp: Library implementation file
//
// Copyright (C) 2015  Rick <ricardogg95@gmail.com>
// This work is licensed under a GNU style license.
//
// Last change: Hendrik Rauh <https://github.com/hendrikrauh>
//
// Documentation and samples are available at https://github.com/Rickgg/ESP-Dmx
// - - - - -

/* ----- LIBRARIES ----- */
#include <Arduino.h>
#include "ESPDMX.h"
#include <vector>

#define MAX_IDS 3

#define DMXSPEED 250000
#define DMXFORMAT SERIAL_8N2
#define BREAKSPEED 83333
#define BREAKFORMAT SERIAL_8N1
#define SERIALPORT Serial0
#define DMXCHANNELS 512

int sendPin[] = {};
int recvPin[] = {};

// DMX value array and size. Entry 0 will hold startbyte, so we need 512+1 elements
// std::vector<uint8_t[DMXCHANNELS + 1]> dmxDataStores(MAX_IDS);
// uint8_t dmxDataStores[MAX_IDS][DMXCHANNELS + 1];

struct dmxPorts
{
    uint8_t id;
    int sendPin;
    int recvPin;
    bool started;
    uint8_t dmxDataStore[DMXCHANNELS + 1];
};

// Set up the DMX-Protocol
void DMXESPSerial::init(int id, int pinSend, int pinRecv)
{
    sendPin[id] = pinSend;
    recvPin[id] = pinRecv;
    SERIALPORT.begin(DMXSPEED, DMXFORMAT, recvPin[id], sendPin[id]);
    pinMode(sendPin[id], OUTPUT);
}

// Function to read DMX data
uint8_t DMXESPSerial::read(int id, int Channel)
{
    if (Channel < 1)
        Channel = 1;
    if (Channel > DMXCHANNELS)
        Channel = DMXCHANNELS;
    return (dmxDataStores[id][Channel]);
}

// Function to send DMX data
void DMXESPSerial::write(int id, int Channel, uint8_t value)
{
    s if (Channel < 1)
        Channel = 1;
    if (Channel > DMXCHANNELS)
        Channel = DMXCHANNELS;
    if (value < 0)
        value = 0;
    if (value > 255)
        value = 255;

    dmxDataStores[id][Channel] = value;
}

void DMXESPSerial::end()
{
    SERIALPORT.end();
}

// Function to update the DMX bus
void DMXESPSerial::update(int id)
{
    // Send break
    digitalWrite(sendPin[id], HIGH);
    SERIALPORT.begin(BREAKSPEED, BREAKFORMAT, recvPin[id], sendPin[id]);
    SERIALPORT.write(0);
    SERIALPORT.flush();
    delay(1);
    SERIALPORT.end();

    // send data
    SERIALPORT.begin(DMXSPEED, DMXFORMAT, recvPin[id], sendPin[id]);
    digitalWrite(sendPin[id], LOW);
    SERIALPORT.write(dmxDataStores[id], DMXCHANNELS);
    SERIALPORT.flush();
    delay(1);
    SERIALPORT.end();
}
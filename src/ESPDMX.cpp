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

// DMX value array and size. Entry 0 will hold startbyte, so we need 512+1 elements
// std::vector<uint8_t[DMXCHANNELS + 1]> dmxDataStores(MAX_IDS);
// uint8_t dmxDataStores[MAX_IDS][DMXCHANNELS + 1];

// Set up the DMX-Protocol
void DMXESPSerial::init(int pinSend, int pinRecv)
{
    sendPin = pinSend;
    recvPin = pinRecv;
    SERIALPORT.begin(DMXSPEED, DMXFORMAT, recvPin, sendPin);
    pinMode(sendPin, OUTPUT);
}

// Function to read DMX data
uint8_t DMXESPSerial::read(int channel)
{
    if (dmxStarted == false)
        init();

    if (channel < 1)
        channel = 1;
    if (channel > DMXCHANNELS)
        channel = DMXCHANNELS;
    return (dmxDataStore[channel]);
}

// Function to send DMX data
void DMXESPSerial::write(int channel, uint8_t value)
{

    if (dmxStarted == false)
        init();

    if (channel < 1)
        channel = 1;
    if (channel > DMXCHANNELS)
        channel = DMXCHANNELS;
    if (value < 0)
        value = 0;
    if (value > 255)
        value = 255;

    dmxDataStore[channel] = value;
}

void DMXESPSerial::end()
{
    SERIALPORT.end();
}

// Function to update the DMX bus
void DMXESPSerial::update()
{
    // Send break
    digitalWrite(sendPin, HIGH);
    SERIALPORT.begin(BREAKSPEED, BREAKFORMAT, recvPin, sendPin);
    SERIALPORT.write(0);
    SERIALPORT.flush();
    delay(1);
    SERIALPORT.end();

    // send data
    SERIALPORT.begin(DMXSPEED, DMXFORMAT, recvPin, sendPin);
    digitalWrite(sendPin, LOW);
    SERIALPORT.write(dmxDataStore, DMXCHANNELS);
    SERIALPORT.flush();
    delay(1);
    SERIALPORT.end();
}
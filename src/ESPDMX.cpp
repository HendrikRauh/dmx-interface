// - - - - -
// ESPDMX - A Arduino library for sending and receiving DMX using the builtin serial hardware port.
// ESPDMX.cpp: Library implementation file
//
// Copyright (C) 2015  Rick <ricardogg95@gmail.com>
// This work is licensed under a GNU style license.
//
// Last change: Marcel Seerig <https://github.com/mseerig>
//
// Documentation and samples are available at https://github.com/Rickgg/ESP-Dmx
// - - - - -

/* ----- LIBRARIES ----- */
#include <Arduino.h>
#include "ESPDMX.h"

#define DMXSPEED 250000
#define DMXFORMAT SERIAL_8N2
#define BREAKSPEED 83333
#define BREAKFORMAT SERIAL_8N1
#define SERIALPORT Serial0
#define DMXCHANNELS 512

bool dmxStarted = false;
int sendPin = 33;
int receivePin = -1;

// DMX value array and size. Entry 0 will hold startbyte, so we need 512+1 elements
uint8_t dmxDataStore[DMXCHANNELS + 1] = {};

// Set up the DMX-Protocol
void DMXESPSerial::init()
{
    SERIALPORT.begin(DMXSPEED, DMXFORMAT, receivePin, sendPin);
    pinMode(sendPin, OUTPUT);
    dmxStarted = true;
}

// Function to read DMX data
uint8_t DMXESPSerial::read(int Channel)
{
    if (dmxStarted == false)
        init();

    if (Channel < 1)
        Channel = 1;
    if (Channel > DMXCHANNELS)
        Channel = DMXCHANNELS;
    return (dmxDataStore[Channel]);
}

// Function to send DMX data
void DMXESPSerial::write(int Channel, uint8_t value)
{
    if (dmxStarted == false)
        init();

    if (Channel < 1)
        Channel = 1;
    if (Channel > DMXCHANNELS)
        Channel = DMXCHANNELS;
    if (value < 0)
        value = 0;
    if (value > 255)
        value = 255;

    dmxDataStore[Channel] = value;
}

void DMXESPSerial::end()
{
    SERIALPORT.end();
    dmxStarted = false;
}

// Function to update the DMX bus
void DMXESPSerial::update()
{
    if (dmxStarted == false)
        init();

    // Send break
    digitalWrite(sendPin, HIGH);
    SERIALPORT.begin(BREAKSPEED, BREAKFORMAT, receivePin, sendPin);
    SERIALPORT.write(0);
    SERIALPORT.flush();
    delay(1);
    SERIALPORT.end();

    // send data
    SERIALPORT.begin(DMXSPEED, DMXFORMAT, receivePin, sendPin);
    digitalWrite(sendPin, LOW);
    SERIALPORT.write(dmxDataStore, DMXCHANNELS);
    SERIALPORT.flush();
    delay(1);
    SERIALPORT.end();
}
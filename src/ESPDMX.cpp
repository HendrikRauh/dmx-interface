/* ----- LIBRARIES ----- */
#include <Arduino.h>
#include "ESPDMX.h"

// DMX value array and size. Entry 0 will hold startbyte, so we need 512+1 elements
// std::vector<uint8_t[DMXCHANNELS + 1]> dmxDataStores(MAX_IDS);
// uint8_t dmxDataStores[MAX_IDS][DMXCHANNELS + 1];

// Set up the DMX-Protocol
void DMXESPSerial::init(int pinSend = DMX_DEFAULT_TX, int pinRecv = DMX_DEFAULT_RX, HardwareSerial& port = DMX_DEFAULT_PORT)
{
    sendPin = pinSend;
    recvPin = pinRecv;
    _Serial = &port;
    _Serial->begin(DMXSPEED, DMXFORMAT, recvPin, sendPin);
    Serial.print("Init DMX with pins (TX/RX): ");
    Serial.print(sendPin);
    Serial.print("/");
    Serial.println(recvPin);
    pinMode(sendPin, OUTPUT);
    dmxStarted = true;
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
uint8_t* DMXESPSerial::readAll()
{
    if (dmxStarted == false)
        init();

    return (&dmxDataStore[1]);
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

    dmxDataStore[channel] = value;
}

void DMXESPSerial::end()
{
    _Serial->end();
}

// Function to update the DMX bus
void DMXESPSerial::update()
{
    // Send break
    digitalWrite(sendPin, HIGH);
    _Serial->begin(BREAKSPEED, BREAKFORMAT, recvPin, sendPin);
    _Serial->write(0);
    _Serial->flush();
    delay(1);
    _Serial->end();

    // send data
    _Serial->begin(DMXSPEED, DMXFORMAT, recvPin, sendPin);
    digitalWrite(sendPin, LOW);
    _Serial->write(dmxDataStore, DMXCHANNELS);
    _Serial->flush();
    delay(1);
    _Serial->end();
}
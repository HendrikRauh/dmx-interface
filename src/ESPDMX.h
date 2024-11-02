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

// ---- Methods ----

class DMXESPSerial {
    public:
        void init(int id, int pinSend, int pinRecv);
        uint8_t read(int id, int Channel);
        void write(int id, int channel, uint8_t value);void update();
        void end();
        void update(int id);
};

#endif

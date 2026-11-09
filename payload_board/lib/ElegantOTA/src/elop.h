#ifndef elop_h
#define elop_h

#include <Arduino.h>

// Dichiarata sempre: serve sia alla modalita' async che a quella sync (WebServer),
// vedi ElegantOTA.cpp che la referenzia in entrambi i rami.
extern const uint8_t ELEGANT_HTML[10214] PROGMEM;
#endif

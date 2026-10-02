#pragma once
// Stub mínimo de Arduino para renderizar la UI en el ordenador.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PROGMEM
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))

extern uint32_t fakeMillis;
inline uint32_t millis() { return fakeMillis; }

#pragma once

// Textos para la pantalla, sin dependencias de Arduino.

#include <stddef.h>
#include <stdint.h>

#include "ft8x7_protocol.h"

namespace ft8x7 {

// 14074000 -> "14.074.00", 145500000 -> "145.500.00" (resolución de 10 Hz de la radio).
void formatFrequency(uint32_t hz, char* buf, size_t len);

// 0..15 -> "S0".."S9", "S9+10".."S9+60".
void formatSMeter(uint8_t sMeter, char* buf, size_t len);

// Modo con sufijo de filtro estrecho: "CW", "CW-N".
void formatMode(const FreqMode& fm, char* buf, size_t len);

}  // namespace ft8x7

#pragma once

#include <stdint.h>

// Colores de la interfaz (RGB565). El tema "emisora antigua" será otra instancia de esta estructura.
struct Theme {
  uint16_t bg;
  uint16_t header;
  uint16_t text;
  uint16_t textDim;
  uint16_t freq;
  uint16_t freqStale;  // sin enlace
  uint16_t box;        // modo y banda
  uint16_t boxStale;
  uint16_t button;
  uint16_t buttonOn;
  uint16_t buttonOnBorder;
  uint16_t buttonPressed;
  uint16_t buttonDisabledText;
  uint16_t field;  // fondo de valores (teclado, offsets)
  uint16_t linkOk;
  uint16_t linkLost;
  uint16_t rx;
  uint16_t tx;
  uint16_t flagSplit;
  uint16_t flagSql;
  uint16_t flagClar;
  uint16_t flagOff;
  uint16_t meterOff;
  uint16_t sMeter;
  uint16_t sMeterOver;  // por encima de S9
  uint16_t po;
  uint16_t poHigh;
  uint16_t warn;
  uint16_t accent;  // avisos y cuenta atrás
};

extern const Theme kDefaultTheme;

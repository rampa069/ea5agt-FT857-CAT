#pragma once

#include <TFT_eSPI.h>

#include "rig_poller.h"

// Pantalla principal 320x240: frecuencia, modo, indicadores y barra de S-meter / potencia.
// Sólo redibuja los elementos que cambian para evitar parpadeo.
class RigDisplay {
 public:
  explicit RigDisplay(TFT_eSPI& tft) : tft_(tft) {}

  void begin(const char* modelName, uint32_t baud);
  void update(const ft8x7::RigState& s);

 private:
  void drawHeader();
  void drawLink(bool linked);
  void drawFrequency(const ft8x7::RigState& s);
  void drawMode(const ft8x7::RigState& s);
  void drawFlag(int x, const char* label, bool on, uint16_t onColor);
  void drawTxRx(bool tx);
  void drawMeterTitle(const ft8x7::RigState& s);
  void drawMeterBar(uint8_t level, bool tx);
  void drawMeterScale(bool tx);
  void drawFooter(const ft8x7::RigState& s);

  TFT_eSPI& tft_;
  const char* model_ = "";
  uint32_t baud_ = 0;

  // Último valor dibujado de cada elemento (-1 = forzar redibujado).
  int lastLinked_ = -1;
  uint32_t lastFreqHz_ = 0;
  int lastFreqColor_ = -1;
  int lastRawMode_ = -1;
  int lastSplit_ = -1;
  int lastSql_ = -1;
  int lastTx_ = -1;
  int lastMeterLevel_ = -1;
  int lastMeterTx_ = -1;
  int lastSwr_ = -1;
  uint32_t lastFooterMs_ = 0;
};

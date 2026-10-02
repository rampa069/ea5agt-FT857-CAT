#pragma once

// Ajustes del display que se guardan en NVS (se serializan tal cual: subir kVersion al cambiarlos).

#include <stdint.h>

#include "rig_ui_logic.h"
#include "touch_filter.h"

namespace rigui {

constexpr uint32_t kBaudRates[] = {4800, 9600, 38400};
constexpr size_t kBaudCount = 3;

struct Settings {
  static constexpr uint16_t kVersion = 1;

  uint16_t version;
  RigModel model;
  uint32_t baud;
  uint8_t brightness;  // 10..100 %
  uint8_t stepIndex;
  TouchCal touch;
  BandMemory bands;
  // Últimos valores enviados de repetidor/tonos (la radio no los informa por CAT).
  uint32_t rptOffsetHz;
  uint8_t ctcssIndex;
  uint8_t dcsIndex;

  void setDefaults(const TouchCal& defaultTouch);
  bool valid() const;  // tras cargar de NVS
};

}  // namespace rigui

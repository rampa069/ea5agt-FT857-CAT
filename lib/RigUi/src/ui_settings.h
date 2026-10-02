#pragma once

// Ajustes del display que se guardan en NVS (se serializan tal cual: subir kVersion al cambiarlos).

#include <stdint.h>

#include "bt_types.h"
#include "rig_ui_logic.h"
#include "touch_filter.h"

namespace rigui {

constexpr uint32_t kBaudRates[] = {4800, 9600, 38400};
constexpr size_t kBaudCount = 3;

struct Settings {
  static constexpr uint16_t kVersion = 3;

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
  // Conexión con la radio
  Transport transport;
  bool btHaveDevice;
  BtDevice btDevice;
  char btPin[9];  // PIN clásico (HC-05/HC-06: 1234)
  // Pantalla (añadido en la versión 3: los campos nuevos siempre al final, ver migrate())
  bool invertColors;

  void setDefaults(const TouchCal& defaultTouch, bool defaultInvert);
  bool valid() const;  // tras cargar de NVS

  // Ajustes guardados por una versión anterior: `stored` bytes leídos al principio de la
  // estructura. Completa los campos nuevos y devuelve true si se pudieron aprovechar.
  bool migrate(size_t stored, bool defaultInvert);
  static size_t sizeOfVersion(uint16_t version);
};

}  // namespace rigui

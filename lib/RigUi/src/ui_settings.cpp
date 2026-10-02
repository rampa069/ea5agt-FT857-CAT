#include "ui_settings.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace rigui {

void Settings::setDefaults(const TouchCal& defaultTouch, bool defaultInvert) {
  // Todo a cero, relleno incluido: los arrays de texto quedan terminados y lo que se guarda
  // en NVS es determinista.
  memset(this, 0, sizeof(*this));
  version = kVersion;
  model = RigModel::FT857;
  baud = 4800;
  brightness = 80;
  stepIndex = 2;  // 1 kHz
  touch = defaultTouch;
  bands.reset();
  rptOffsetHz = 600000;
  ctcssIndex = 8;  // 88,5 Hz
  dcsIndex = 0;    // 023
  transport = Transport::Cable;
  btHaveDevice = false;
  btDevice = BtDevice{};
  snprintf(btPin, sizeof(btPin), "1234");
  invertColors = defaultInvert;
}

size_t Settings::sizeOfVersion(uint16_t v) {
  switch (v) {
    case 2: return offsetof(Settings, invertColors);  // v2 terminaba en btPin
    case kVersion: return sizeof(Settings);
    default: return 0;
  }
}

bool Settings::migrate(size_t stored, bool defaultInvert) {
  if (version == 2 && stored >= sizeOfVersion(2)) {
    invertColors = defaultInvert;
    version = kVersion;
    return valid();
  }
  return false;
}

bool Settings::valid() const {
  bool baudOk = false;
  for (uint32_t b : kBaudRates) {
    baudOk |= b == baud;
  }
  return version == kVersion && static_cast<uint8_t>(model) < kModelCount && baudOk &&
         brightness >= 10 && brightness <= 100 && stepIndex < kStepCount &&
         ctcssIndex < ft8x7::kCtcssToneCount && dcsIndex < ft8x7::kDcsCodeCount &&
         touch.xMax != touch.xMin && touch.yMax != touch.yMin &&
         static_cast<uint8_t>(transport) <= static_cast<uint8_t>(Transport::Bluetooth) &&
         btPin[sizeof(btPin) - 1] == '\0' && btDevice.name[sizeof(btDevice.name) - 1] == '\0';
}

}  // namespace rigui

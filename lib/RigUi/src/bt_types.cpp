#include "bt_types.h"

#include <stdio.h>

namespace rigui {

const char* btStateName(BtState state) {
  switch (state) {
    case BtState::Off: return "Apagado";
    case BtState::Idle: return "Sin dispositivo";
    case BtState::Scanning: return "Buscando...";
    case BtState::Connecting: return "Conectando...";
    case BtState::Connected: return "Conectado";
    case BtState::Failed: return "Sin conexion, reintentando";
  }
  return "?";
}

void formatBtAddr(const uint8_t a[6], char* buf, size_t len) {
  snprintf(buf, len, "%02X:%02X:%02X:%02X:%02X:%02X", a[0], a[1], a[2], a[3], a[4], a[5]);
}

}  // namespace rigui

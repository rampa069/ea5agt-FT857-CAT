#include "bt_types.h"

#include <ctype.h>
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

namespace {

bool containsNoCase(const char* text, const char* word) {
  for (; *text; ++text) {
    const char* a = text;
    const char* b = word;
    while (*a && *b && tolower(static_cast<unsigned char>(*a)) == *b) {
      ++a;
      ++b;
    }
    if (!*b) return true;
  }
  return false;
}

}  // namespace

int btAdapterScore(const BtFound& f) {
  static const char* const kAdapterWords[] = {"hc-0", "hc0", "linvor", "cat", "yaesu", "ft8", "spp", "serial"};
  for (const char* w : kAdapterWords) {
    if (containsNoCase(f.device.name, w)) return 3;
  }
  uint8_t major = (f.cod >> 8) & 0x1F;  // clase mayor del Class of Device
  if (f.cod != 0 && (major == 0x1F || major == 0x00)) return 2;
  if (f.cod == 0 && f.device.name[0] == '\0') return 1;
  return 0;
}

void sortBtFound(BtFound* items, size_t count) {
  for (size_t i = 1; i < count; ++i) {  // inserción: pocas entradas y orden estable
    BtFound x = items[i];
    int sx = btAdapterScore(x);
    size_t j = i;
    while (j > 0) {
      int sp = btAdapterScore(items[j - 1]);
      bool before = sx > sp || (sx == sp && x.device.rssi > items[j - 1].device.rssi);
      if (!before) break;
      items[j] = items[j - 1];
      --j;
    }
    items[j] = x;
  }
}

void formatBtAddr(const uint8_t a[6], char* buf, size_t len) {
  snprintf(buf, len, "%02X:%02X:%02X:%02X:%02X:%02X", a[0], a[1], a[2], a[3], a[4], a[5]);
}

}  // namespace rigui

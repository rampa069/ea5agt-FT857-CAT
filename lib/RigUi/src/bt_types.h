#pragma once

// Estado del enlace Bluetooth tal como lo ve la interfaz (sin depender de la pila BT).

#include <stddef.h>
#include <stdint.h>

namespace rigui {

enum class Transport : uint8_t { Cable, Bluetooth };

enum class BtState : uint8_t {
  Off,         // pila Bluetooth parada (transporte por cable)
  Idle,        // encendida, sin dispositivo configurado
  Scanning,    // buscando dispositivos
  Connecting,  // conectando (o esperando a que se confirme el emparejamiento)
  Connected,
  Failed,      // el último intento falló; se reintenta solo
};

struct BtDevice {
  uint8_t addr[6];
  char name[24];
  int8_t rssi;
};

struct BtStatus {
  BtState state = BtState::Off;
  BtDevice device{};          // dispositivo configurado o conectado
  bool haveDevice = false;
  uint32_t confirmCode = 0;   // código a confirmar en el otro equipo (0 = ninguno)
  uint32_t scanSerial = 0;    // cambia al terminar cada búsqueda
};

const char* btStateName(BtState state);
void formatBtAddr(const uint8_t addr[6], char* buf, size_t len);  // "98:D3:31:F5:A2:10"

}  // namespace rigui

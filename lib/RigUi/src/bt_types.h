#pragma once

// Estado del enlace Bluetooth tal como lo ve la interfaz (sin depender de la pila BT).

#include <stddef.h>
#include <stdint.h>

namespace rigui {

enum class Transport : uint8_t { Cable, Bluetooth };

// Duración de la búsqueda de dispositivos Bluetooth.
constexpr uint32_t kBtScanSeconds = 25;

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

// Resultado de una búsqueda: el dispositivo más su clase (Class of Device), que no se guarda.
struct BtFound {
  BtDevice device;
  uint32_t cod;  // 0 si no se conoce
};

// Cuánto se parece a un adaptador CAT (HC-05/HC-06 y similares): 3 nombre típico, 2 clase
// «sin categoría»/«varios» (como anuncian los HC-0x), 1 sin nombre ni clase, 0 resto
// (móviles, audio, ordenadores...).
int btAdapterScore(const BtFound& found);
// Ordena: primero los más parecidos a un adaptador, luego por señal.
void sortBtFound(BtFound* items, size_t count);

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

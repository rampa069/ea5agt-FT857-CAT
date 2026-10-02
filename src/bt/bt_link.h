#pragma once

// Enlace CAT por Bluetooth clásico (SPP) como maestro, hacia un adaptador tipo HC-05/HC-06.
// Todo lo que toca la pila BT corre en la tarea CAT (service()); la UI sólo pide cosas
// (requestScan/requestConnect/requestForget) y lee copias del estado.

#include <BluetoothSerial.h>

#include <mutex>

#include "bt_types.h"
#include "ft8x7_cat.h"

class BtLink {
 public:
  static constexpr size_t kMaxResults = 12;

  BtLink();

  // --- Tarea CAT ---
  void start(const char* localName);  // arranca la pila (idempotente)
  void stop();                        // la para (al volver al cable)
  // Atiende peticiones y la reconexión automática. Puede bloquear varios segundos.
  // device/pin: dispositivo configurado en ajustes (haveDevice=false si ninguno).
  void service(bool haveDevice, const rigui::BtDevice& device, const char* pin, uint32_t nowMs);
  ft8x7::CatPort& port() { return port_; }

  // --- Cualquier tarea ---
  void requestScan();
  void requestConnect(const rigui::BtDevice& device);
  void requestForget(const uint8_t addr[6]);
  rigui::BtStatus status() const;
  size_t results(rigui::BtFound* out, size_t max) const;
  // Dispositivo con el que se acaba de conectar a petición del usuario (para guardarlo en ajustes).
  bool takeNewlyPaired(rigui::BtDevice& out);

 private:
  class Port : public ft8x7::CatPort {
   public:
    explicit Port(BluetoothSerial& bt) : bt_(bt) {}
    void write(const uint8_t* data, size_t len) override;
    size_t read(uint8_t* data, size_t len, uint32_t timeoutMs) override;
    void discardInput() override;

   private:
    BluetoothSerial& bt_;
  };

  void setState(rigui::BtState s);
  void doScan();
  bool doConnect(const rigui::BtDevice& device, const char* pin);

  BluetoothSerial bt_;
  Port port_;
  bool started_ = false;

  mutable std::mutex mutex_;
  rigui::BtStatus status_;
  rigui::BtFound results_[kMaxResults];
  size_t resultCount_ = 0;
  bool wantScan_ = false;
  bool wantConnect_ = false;
  rigui::BtDevice connectTarget_{};
  bool wantForget_ = false;
  uint8_t forgetAddr_[6] = {};
  bool newlyPaired_ = false;
  rigui::BtDevice newlyPairedDevice_{};

  uint32_t lastAttemptMs_ = 0;
  bool attempted_ = false;
};

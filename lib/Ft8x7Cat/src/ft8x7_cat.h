#pragma once

// Cliente CAT de sólo lectura sobre un puerto serie abstracto.

#include "ft8x7_protocol.h"

namespace ft8x7 {

// Transporte serie mínimo (8N2). Implementado por ArduinoCatPort en el ESP32 y por mocks en tests.
class CatPort {
 public:
  virtual ~CatPort() = default;
  virtual void write(const uint8_t* data, size_t len) = 0;
  // Lee hasta len bytes esperando como máximo timeoutMs en total; devuelve los leídos.
  virtual size_t read(uint8_t* data, size_t len, uint32_t timeoutMs) = 0;
  virtual void discardInput() = 0;
};

enum class CatResult : uint8_t { Ok, Timeout, BadData };

const char* catResultName(CatResult result);

class Ft8x7Cat {
 public:
  explicit Ft8x7Cat(CatPort& port, uint32_t timeoutMs = 200, uint32_t ackTimeoutMs = 50)
      : port_(port), timeoutMs_(timeoutMs), ackTimeoutMs_(ackTimeoutMs) {}

  CatResult readFreqMode(FreqMode& out);
  CatResult readRxStatus(RxStatus& out);
  CatResult readTxStatus(TxStatus& out);
  CatResult readEeprom(uint16_t address, uint8_t out[2]);
  CatResult readEepromByte(uint16_t address, uint8_t& out);
  CatResult readTxMeters(TxMeters& out);  // sólo transmitiendo (en RX la radio responde 1 byte)

  // Envía un comando de escritura. Según el firmware la radio responde un byte de confirmación
  // o nada; se espera hasta ackTimeoutMs para que no se mezcle con la respuesta siguiente.
  // Devuelve true si llegó la confirmación (informativo: su ausencia no es un error).
  bool send(const Command& cmd);

  void setTimeoutMs(uint32_t timeoutMs) { timeoutMs_ = timeoutMs; }

 private:
  CatResult transact(const Command& cmd, uint8_t* response, size_t len);
  CatResult transact(const Command& cmd, uint8_t* response, size_t len, uint32_t timeoutMs);

  CatPort& port_;
  uint32_t timeoutMs_;
  uint32_t ackTimeoutMs_;
};

}  // namespace ft8x7

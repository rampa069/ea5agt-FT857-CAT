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
  explicit Ft8x7Cat(CatPort& port, uint32_t timeoutMs = 200) : port_(port), timeoutMs_(timeoutMs) {}

  CatResult readFreqMode(FreqMode& out);
  CatResult readRxStatus(RxStatus& out);
  CatResult readTxStatus(TxStatus& out);
  CatResult readEeprom(uint16_t address, uint8_t out[2]);

  void setTimeoutMs(uint32_t timeoutMs) { timeoutMs_ = timeoutMs; }

 private:
  CatResult transact(const Command& cmd, uint8_t* response, size_t len);

  CatPort& port_;
  uint32_t timeoutMs_;
};

}  // namespace ft8x7

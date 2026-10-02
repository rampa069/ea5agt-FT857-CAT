#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ft8x7_cat.h"

namespace ft8x7 {

// Puerto CAT sobre un HardwareSerial del ESP32 (UART2 en los pines libres de CN1).
class ArduinoCatPort : public CatPort {
 public:
  explicit ArduinoCatPort(HardwareSerial& serial) : serial_(serial) {}

  void begin(uint32_t baud, int rxPin, int txPin) { serial_.begin(baud, SERIAL_8N2, rxPin, txPin); }

  void write(const uint8_t* data, size_t len) override {
    serial_.write(data, len);
    serial_.flush();
  }

  size_t read(uint8_t* data, size_t len, uint32_t timeoutMs) override {
    size_t got = 0;
    uint32_t start = millis();
    while (got < len && millis() - start < timeoutMs) {
      int c = serial_.read();
      if (c >= 0) {
        data[got++] = static_cast<uint8_t>(c);
      } else {
        delay(1);
      }
    }
    return got;
  }

  void discardInput() override {
    while (serial_.available() > 0) {
      serial_.read();
    }
  }

 private:
  HardwareSerial& serial_;
};

}  // namespace ft8x7

#endif  // ARDUINO

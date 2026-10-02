#pragma once

#include "ft8x7_cat.h"

namespace ft8x7 {

// Puerto que delega en otro (UART o Bluetooth) elegido en caliente.
// Cambiar el destino sólo desde la tarea que usa el puerto.
class SwitchingCatPort : public CatPort {
 public:
  void setTarget(CatPort* target) { target_ = target; }
  CatPort* target() const { return target_; }

  void write(const uint8_t* data, size_t len) override {
    if (target_) target_->write(data, len);
  }
  size_t read(uint8_t* data, size_t len, uint32_t timeoutMs) override {
    return target_ ? target_->read(data, len, timeoutMs) : 0;
  }
  void discardInput() override {
    if (target_) target_->discardInput();
  }

 private:
  CatPort* target_ = nullptr;
};

}  // namespace ft8x7

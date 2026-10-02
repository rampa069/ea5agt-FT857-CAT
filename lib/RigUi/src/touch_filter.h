#pragma once

// Antirrebote del táctil resistivo y calibración, sin hardware (testeable en nativo).

#include <stdint.h>

namespace rigui {

enum class TouchEventType : uint8_t { None, Down, Repeat, Up };

struct TouchEvent {
  TouchEventType type = TouchEventType::None;
  int16_t x = 0, y = 0;  // posición de la pulsación en píxeles
  explicit operator bool() const { return type != TouchEventType::None; }
};

// Convierte muestras crudas (contacto sí/no + posición) en eventos limpios:
// Down tras kPressSamples muestras con contacto, Repeat al mantener (para la sintonía)
// y Up tras releaseMs sin contacto (el resistivo "parpadea" mientras se pulsa).
class TouchFilter {
 public:
  static constexpr uint8_t kPressSamples = 3;
  static constexpr uint32_t kReleaseMs = 150;
  static constexpr uint32_t kRepeatDelayMs = 450;
  static constexpr uint32_t kRepeatEveryMs = 120;

  TouchEvent feed(bool contact, int16_t x, int16_t y, uint32_t nowMs);
  bool pressed() const { return state_ == State::Held; }

 private:
  enum class State : uint8_t { Idle, Arming, Held };
  State state_ = State::Idle;
  uint8_t samples_ = 0;
  int32_t sumX_ = 0, sumY_ = 0;
  int16_t x_ = 0, y_ = 0;
  uint32_t lastContactMs_ = 0;
  uint32_t nextRepeatMs_ = 0;
};

// Rango crudo del XPT2046 en los bordes de la pantalla.
struct TouchCal {
  int16_t xMin, xMax, yMin, yMax;
  bool swapXY;
};

void mapTouch(const TouchCal& cal, int16_t rawX, int16_t rawY, int16_t width, int16_t height,
              int16_t& x, int16_t& y);

// Calibración con 5 puntos crudos tocados en: arriba-izq, arriba-der, abajo-der, abajo-izq, centro,
// dibujados a `margin` px de los bordes. Devuelve false si los puntos no tienen sentido.
struct RawPoint {
  int16_t x, y;
};
bool computeCalibration(const RawPoint raw[5], int16_t width, int16_t height, int16_t margin,
                        TouchCal& out);

}  // namespace rigui

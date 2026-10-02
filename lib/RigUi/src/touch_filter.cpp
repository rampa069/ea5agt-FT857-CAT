#include "touch_filter.h"

#include <stdlib.h>

namespace rigui {

TouchEvent TouchFilter::feed(bool contact, int16_t x, int16_t y, uint32_t nowMs) {
  TouchEvent ev;
  switch (state_) {
    case State::Idle:
      if (contact) {
        state_ = State::Arming;
        samples_ = 1;
        sumX_ = x;
        sumY_ = y;
        lastContactMs_ = nowMs;
      }
      break;

    case State::Arming:
      if (!contact) {
        if (nowMs - lastContactMs_ >= kReleaseMs) {
          state_ = State::Idle;  // roce: no llega a pulsación
        }
        break;
      }
      lastContactMs_ = nowMs;
      sumX_ += x;
      sumY_ += y;
      if (++samples_ >= kPressSamples) {
        state_ = State::Held;
        x_ = static_cast<int16_t>(sumX_ / samples_);
        y_ = static_cast<int16_t>(sumY_ / samples_);
        nextRepeatMs_ = nowMs + kRepeatDelayMs;
        ev.type = TouchEventType::Down;
      }
      break;

    case State::Held:
      if (contact) {
        lastContactMs_ = nowMs;
        if (static_cast<int32_t>(nowMs - nextRepeatMs_) >= 0) {
          nextRepeatMs_ = nowMs + kRepeatEveryMs;
          ev.type = TouchEventType::Repeat;
        }
      } else if (nowMs - lastContactMs_ >= kReleaseMs) {
        state_ = State::Idle;
        ev.type = TouchEventType::Up;
      }
      break;
  }
  ev.x = x_;
  ev.y = y_;
  return ev;
}

void mapTouch(const TouchCal& cal, int16_t rawX, int16_t rawY, int16_t width, int16_t height,
              int16_t& x, int16_t& y) {
  int32_t rx = cal.swapXY ? rawY : rawX;
  int32_t ry = cal.swapXY ? rawX : rawY;
  int32_t mx = (rx - cal.xMin) * (width - 1) / (cal.xMax - cal.xMin);
  int32_t my = (ry - cal.yMin) * (height - 1) / (cal.yMax - cal.yMin);
  x = static_cast<int16_t>(mx < 0 ? 0 : (mx >= width ? width - 1 : mx));
  y = static_cast<int16_t>(my < 0 ? 0 : (my >= height ? height - 1 : my));
}

namespace {

// Extrapola de los puntos (a margin px del borde) a los bordes 0 y size-1.
void edgeRange(int32_t rawLow, int32_t rawHigh, int16_t size, int16_t margin, int16_t& outMin,
               int16_t& outMax) {
  int32_t span = size - 1 - 2 * margin;
  outMin = static_cast<int16_t>(rawLow - (rawHigh - rawLow) * margin / span);
  outMax = static_cast<int16_t>(rawHigh + (rawHigh - rawLow) * margin / span);
}

}  // namespace

bool computeCalibration(const RawPoint raw[5], int16_t width, int16_t height, int16_t margin,
                        TouchCal& out) {
  TouchCal cal;
  // Si al ir a la derecha cambia más la Y cruda que la X, los ejes están cruzados.
  cal.swapXY = abs(raw[1].y - raw[0].y) > abs(raw[1].x - raw[0].x);
  auto rx = [&](const RawPoint& p) { return cal.swapXY ? p.y : p.x; };
  auto ry = [&](const RawPoint& p) { return cal.swapXY ? p.x : p.y; };

  int32_t left = (rx(raw[0]) + rx(raw[3])) / 2;
  int32_t right = (rx(raw[1]) + rx(raw[2])) / 2;
  int32_t top = (ry(raw[0]) + ry(raw[1])) / 2;
  int32_t bottom = (ry(raw[2]) + ry(raw[3])) / 2;
  if (abs(right - left) < 500 || abs(bottom - top) < 500) {
    return false;  // puntos casi iguales: rebote o toques fallidos
  }
  edgeRange(left, right, width, margin, cal.xMin, cal.xMax);
  edgeRange(top, bottom, height, margin, cal.yMin, cal.yMax);

  // El centro debe caer cerca del centro con la calibración nueva.
  int16_t cx, cy;
  mapTouch(cal, raw[4].x, raw[4].y, width, height, cx, cy);
  if (abs(cx - width / 2) > 20 || abs(cy - height / 2) > 20) {
    return false;
  }
  out = cal;
  return true;
}

}  // namespace rigui

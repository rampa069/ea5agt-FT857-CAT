#pragma once

// Utilidades compartidas por las pieles retro.

#include <TFT_eSPI.h>

#include "fonts/fonts.h"

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Carga una fuente suave mientras dura el bloque. Hay que descargarla siempre: con una fuente
// suave cargada, TFT_eSPI ignora el número de fuente de drawString y el resto de la UI
// (que usa las fuentes internas) se dibujaría mal.
class FontScope {
 public:
  FontScope(TFT_eSPI& tft, const uint8_t* font) : tft_(tft) { tft_.loadFont(font); }
  ~FontScope() { tft_.unloadFont(); }
  FontScope(const FontScope&) = delete;
  FontScope& operator=(const FontScope&) = delete;

 private:
  TFT_eSPI& tft_;
};

// Mezcla de dos colores RGB565 (t = 0 → a, 255 → b).
inline uint16_t mix565(uint16_t a, uint16_t b, uint8_t t) {
  int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  int r = ar + (br - ar) * t / 255, g = ag + (bg - ag) * t / 255, bl = ab + (bb - ab) * t / 255;
  return static_cast<uint16_t>((r << 11) | (g << 5) | bl);
}

// Cuadro de texto centrado con fuente suave (fondo uniforme bg).
inline void drawCentered(TFT_eSPI& tft, const uint8_t* font, const char* text, int16_t cx, int16_t cy,
                         uint16_t fg, uint16_t bg) {
  FontScope f(tft, font);
  tft.setTextColor(fg, bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(0);
  tft.drawString(text, cx, cy);
}

// Posición 0..1 de la aguja para un nivel 0..15: S0-S9 ocupan el 55 % de la escala y
// S9+10..+60 el resto (como en la esfera); en TX la potencia es lineal.
inline float levelToNeedle(uint8_t level, bool tx) {
  if (tx) return level / 15.0f;
  if (level <= 9) return level * 0.55f / 9.0f;
  return 0.55f + (level - 9) * 0.45f / 6.0f;
}

// Aguja con inercia: sube rápido y baja más despacio, como un galvanómetro.
struct NeedleMotion {
  float pos = 0.0f;
  uint32_t lastMs = 0;
  // Devuelve true si la aguja se ha movido lo bastante como para redibujarla.
  bool step(float target, uint32_t nowMs) {
    if (nowMs - lastMs < 25) return false;
    lastMs = nowMs;
    float d = target - pos;
    if (d > -0.003f && d < 0.003f) {
      if (pos == target) return false;
      pos = target;
      return true;
    }
    pos += d * (d > 0 ? 0.45f : 0.18f);
    return true;
  }
};

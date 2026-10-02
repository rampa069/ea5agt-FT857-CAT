#pragma once
// Imitación de TFT_eSPI que dibuja en un framebuffer RGB565, con las fuentes reales
// de la librería y la misma lógica de datum/padding que TFT_eSPI 2.5.
#include "Arduino.h"

#define TFT_BLACK 0x0000
#define TFT_NAVY 0x000F
#define TFT_DARKGREEN 0x03E0
#define TFT_LIGHTGREY 0xD69A
#define TFT_DARKGREY 0x7BEF
#define TFT_GREEN 0x07E0
#define TFT_CYAN 0x07FF
#define TFT_RED 0xF800
#define TFT_YELLOW 0xFFE0
#define TFT_WHITE 0xFFFF
#define TFT_ORANGE 0xFDA0
#define TFT_SKYBLUE 0x867D

#define TL_DATUM 0
#define TC_DATUM 1
#define TR_DATUM 2
#define ML_DATUM 3
#define MC_DATUM 4
#define MR_DATUM 5
#define BL_DATUM 6
#define BC_DATUM 7
#define BR_DATUM 8

class TFT_eSPI {
 public:
  static constexpr int W = 320, H = 240;
  uint16_t fb[W * H] = {};

  int16_t width() const { return W; }
  int16_t height() const { return H; }
  void setRotation(uint8_t) {}

  void drawPixel(int32_t x, int32_t y, uint16_t c) {
    if (x >= 0 && y >= 0 && x < W && y < H) fb[y * W + x] = c;
  }
  void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) {
    for (int32_t j = y; j < y + h; ++j)
      for (int32_t i = x; i < x + w; ++i) drawPixel(i, j, c);
  }
  void fillScreen(uint16_t c) { fillRect(0, 0, W, H, c); }
  void drawFastHLine(int32_t x, int32_t y, int32_t w, uint16_t c) { fillRect(x, y, w, 1, c); }
  void drawFastVLine(int32_t x, int32_t y, int32_t h, uint16_t c) { fillRect(x, y, 1, h, c); }
  void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c);
  void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c);

  void setTextColor(uint16_t fg, uint16_t bg) { fg_ = fg; bg_ = bg; }
  void setTextDatum(uint8_t d) { datum_ = d; }
  void setTextPadding(uint16_t p) { padX_ = p; }
  int16_t textWidth(const char* s, uint8_t font);
  int16_t fontHeight(uint8_t font);
  int16_t drawString(const char* s, int32_t x, int32_t y, uint8_t font);
  int16_t drawNumber(long n, int32_t x, int32_t y, uint8_t font) {
    char b[16];
    snprintf(b, sizeof(b), "%ld", n);
    return drawString(b, x, y, font);
  }

 private:
  int16_t drawChar(char c, int32_t x, int32_t y, uint8_t font);
  uint16_t fg_ = TFT_WHITE, bg_ = TFT_WHITE, padX_ = 0;
  uint8_t datum_ = TL_DATUM;
};

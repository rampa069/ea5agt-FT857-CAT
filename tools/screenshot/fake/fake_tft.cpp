#include "TFT_eSPI.h"

#include <Fonts/Font16.h>
#include <Fonts/Font32rle.h>
#include <Fonts/Font7srle.h>
#include <Fonts/glcdfont.c>

uint32_t fakeMillis = 0;

static inline uint8_t font_glcd(unsigned char c, int i) { return font[c * 5 + i]; }

namespace {
struct FontInfo {
  const unsigned char* const* chars;
  const unsigned char* widths;
  uint8_t height;
};
const FontInfo* fontInfo(uint8_t font) {
  static const FontInfo f2{chrtbl_f16, widtbl_f16, chr_hgt_f16};
  static const FontInfo f4{chrtbl_f32, widtbl_f32, chr_hgt_f32};
  static const FontInfo f7{chrtbl_f7s, widtbl_f7s, chr_hgt_f7s};
  switch (font) {
    case 2: return &f2;
    case 4: return &f4;
    case 7: return &f7;
    default: return nullptr;
  }
}
}  // namespace

void TFT_eSPI::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c) {
  for (int32_t j = 0; j < h; ++j) {
    for (int32_t i = 0; i < w; ++i) {
      int32_t dx = i < r ? r - i : (i >= w - r ? i - (w - r - 1) : 0);
      int32_t dy = j < r ? r - j : (j >= h - r ? j - (h - r - 1) : 0);
      if (dx * dx + dy * dy <= r * r + r) drawPixel(x + i, y + j, c);
    }
  }
}

void TFT_eSPI::drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c) {
  for (int32_t j = 0; j < h; ++j) {
    for (int32_t i = 0; i < w; ++i) {
      int32_t dx = i < r ? r - i : (i >= w - r ? i - (w - r - 1) : 0);
      int32_t dy = j < r ? r - j : (j >= h - r ? j - (h - r - 1) : 0);
      int32_t d2 = dx * dx + dy * dy;
      bool inside = d2 <= r * r + r;
      bool edge = (dx || dy) ? d2 > (r - 1) * (r - 1) + (r - 1) : (i == 0 || j == 0 || i == w - 1 || j == h - 1);
      if (inside && edge) drawPixel(x + i, y + j, c);
    }
  }
}

int16_t TFT_eSPI::textWidth(const char* s, uint8_t font) {
  const FontInfo* f = fontInfo(font);
  int16_t w = 0;
  for (; *s; ++s) {
    if (!f) {
      w += 6;
    } else {
      unsigned char c = *s;
      w += f->widths[(c > 31 && c < 128) ? c - 32 : 0];
    }
  }
  return w;
}

int16_t TFT_eSPI::fontHeight(uint8_t font) {
  const FontInfo* f = fontInfo(font);
  return f ? f->height : 8;
}

int16_t TFT_eSPI::drawChar(char ch, int32_t x, int32_t y, uint8_t font) {
  unsigned char uc = ch;
  if (font == 1) {  // GLCD 5x7 + columna de separación
    for (int i = 0; i < 6; ++i) {
      uint8_t line = i == 5 ? 0 : font_glcd(uc, i);
      for (int j = 0; j < 8; ++j, line >>= 1) {
        if (line & 1) drawPixel(x + i, y + j, fg_);
        else if (bg_ != fg_) drawPixel(x + i, y + j, bg_);
      }
    }
    return 6;
  }
  const FontInfo* f = fontInfo(font);
  if (!f || uc < 32 || uc > 127) return 0;
  const unsigned char* data = f->chars[uc - 32];
  int32_t width = f->widths[uc - 32];
  int32_t height = f->height;

  if (font == 2) {  // bitmap, fila a fila, MSB primero
    int32_t bytes = (width + 6) / 8;
    for (int32_t i = 0; i < height; ++i) {
      if (fg_ != bg_) fillRect(x, y + i, width, 1, bg_);
      for (int32_t k = 0; k < bytes; ++k) {
        uint8_t line = data[bytes * i + k];
        for (int b = 0; b < 8; ++b)
          if (line & (0x80 >> b)) drawPixel(x + k * 8 + b, y + i, fg_);
      }
    }
    return width;
  }

  // RLE: bit 7 = racha de primer plano, si no, de fondo; longitud (b & 0x7F) + 1
  int32_t total = width * height, pc = 0;
  while (pc < total) {
    uint8_t b = *data++;
    bool on = b & 0x80;
    int32_t run = (b & 0x7F) + 1;
    for (int32_t n = 0; n < run && pc < total; ++n, ++pc) {
      if (on) drawPixel(x + pc % width, y + pc / width, fg_);
      else if (fg_ != bg_) drawPixel(x + pc % width, y + pc / width, bg_);
    }
  }
  return width;
}

int16_t TFT_eSPI::drawString(const char* s, int32_t poX, int32_t poY, uint8_t font) {
  int16_t cwidth = textWidth(s, font);
  int16_t cheight = fontHeight(font);
  uint8_t padding = 1;
  switch (datum_) {
    case TC_DATUM: poX -= cwidth / 2; padding += 1; break;
    case TR_DATUM: poX -= cwidth; padding += 2; break;
    case ML_DATUM: poY -= cheight / 2; break;
    case MC_DATUM: poX -= cwidth / 2; poY -= cheight / 2; padding += 1; break;
    case MR_DATUM: poX -= cwidth; poY -= cheight / 2; padding += 2; break;
    case BL_DATUM: poY -= cheight; break;
    case BC_DATUM: poX -= cwidth / 2; poY -= cheight; padding += 1; break;
    case BR_DATUM: poX -= cwidth; poY -= cheight; padding += 2; break;
  }
  int16_t sumX = 0;
  for (const char* p = s; *p; ++p) sumX += drawChar(*p, poX + sumX, poY, font);

  if (padX_ > cwidth && fg_ != bg_) {
    int16_t padXc = poX + cwidth;
    switch (padding) {
      case 1: fillRect(padXc, poY, padX_ - cwidth, cheight, bg_); break;
      case 2:
        fillRect(padXc, poY, (padX_ - cwidth) >> 1, cheight, bg_);
        fillRect(poX - ((padX_ - cwidth) >> 1), poY, (padX_ - cwidth) >> 1, cheight, bg_);
        break;
      case 3:
        if (padXc > padX_) padXc = padX_;
        fillRect(poX + cwidth - padXc, poY, padXc - cwidth, cheight, bg_);
        break;
    }
  }
  return sumX;
}

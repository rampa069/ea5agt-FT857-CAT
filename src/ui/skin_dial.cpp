// Piel «Dial»: escala de banda retroiluminada con aguja deslizante (tocar la escala sintoniza),
// lectura digital, chapas de latón, S-meter horizontal de aguja, madera y teclas de marfil.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "rig_ui_logic.h"
#include "skin.h"
#include "skin_util.h"

namespace {

constexpr uint16_t kWoodA = rgb(0x4f, 0x2d, 0x13);
constexpr uint16_t kWoodB = rgb(0x6e, 0x41, 0x20);
constexpr uint16_t kWood = rgb(0x5a, 0x35, 0x17);
constexpr uint16_t kFrameDark = rgb(0x2a, 0x1a, 0x0c);
constexpr uint16_t kGlassTop = rgb(0xff, 0xf3, 0xcf);
constexpr uint16_t kGlassBottom = rgb(0xf0, 0xd9, 0x95);
constexpr uint16_t kInk = rgb(0x3b, 0x24, 0x10);
constexpr uint16_t kRed = rgb(0xc0, 0x28, 0x1e);
constexpr uint16_t kBrassTop = rgb(0xdd, 0xbb, 0x6a);
constexpr uint16_t kBrassBottom = rgb(0xb3, 0x8c, 0x3c);
constexpr uint16_t kBrassLine = rgb(0x7a, 0x5a, 0x22);
constexpr uint16_t kReadout = rgb(0x1d, 0x12, 0x0a);
constexpr uint16_t kReadoutText = rgb(0xf3, 0xd2, 0x7a);
constexpr uint16_t kIvory = rgb(0xff, 0xf1, 0xcc);
constexpr uint16_t kKey = rgb(0xe6, 0xdb, 0xc2);
constexpr uint16_t kKeyPressed = rgb(0xcd, 0xbf, 0x9f);
constexpr uint16_t kKeyShadow = rgb(0xa8, 0x96, 0x71);

const Theme kDialTheme = {
    kFrameDark,             // bg
    rgb(0x3b, 0x24, 0x10),  // header
    rgb(0xf3, 0xe6, 0xc8),  // text
    rgb(0xb8, 0x9f, 0x78),  // textDim
    kReadoutText,           // freq
    rgb(0x8a, 0x76, 0x50),  // freqStale
    kReadout,               // box
    rgb(0x24, 0x18, 0x0e),  // boxStale
    kKey,                   // button
    rgb(0xf3, 0xd2, 0x7a),  // buttonOn
    kRed,                   // buttonOnBorder
    kKeyPressed,            // buttonPressed
    kKeyShadow,             // buttonDisabledText
    kReadout,               // field
    rgb(0x2f, 0x6b, 0x3a),  // linkOk
    kRed,                   // linkLost
    rgb(0x2f, 0x6b, 0x3a),  // rx
    kRed,                   // tx
    kReadoutText,           // flagSplit
    rgb(0x8f, 0xbf, 0x6a),  // flagSql
    rgb(0xff, 0x8a, 0x1a),  // flagClar
    rgb(0x4a, 0x34, 0x20),  // flagOff
    rgb(0x3a, 0x26, 0x14),  // meterOff
    rgb(0x8f, 0xbf, 0x6a),  // sMeter
    kRed,                   // sMeterOver
    kReadoutText,           // po
    kRed,                   // poHigh
    kRed,                   // warn
    kReadoutText,           // accent
};

// Escala (sprite de cristal) y zona táctil de la escala
constexpr int16_t kGlassX = 10, kGlassY = 10, kGlassW = 300, kGlassH = 78;
constexpr int16_t kScaleX0 = 14, kScaleX1 = 286;  // dentro del sprite
constexpr int16_t kBase = 46;                     // línea base de las marcas (sprite)
constexpr Rect kDialZone{kGlassX + kScaleX0, kGlassY, kScaleX1 - kScaleX0, kGlassH};
// Fila central
constexpr Rect kMode{6, 96, 84, 38};
constexpr Rect kReadoutBox{94, 96, 132, 38};
constexpr Rect kBand{230, 96, 84, 38};
// Medidor horizontal
constexpr Rect kMeterFrame{10, 138, 300, 50};
constexpr Rect kMeterFace{14, 142, 292, 42};
constexpr int16_t kMx0 = 30, kMx1 = 296, kMBase = 160;

uint16_t glassAt(int16_t y) { return mix565(kGlassTop, kGlassBottom, static_cast<uint8_t>(255 * y / kGlassH)); }

void bandTitle(const char* name, uint32_t lo, char* buf, size_t len) {
  if (strcmp(name, "AIR") == 0) {
    snprintf(buf, len, "BANDA AEREA");
  } else if (strcmp(name, "FM-BC") == 0) {
    snprintf(buf, len, "RADIODIFUSION FM");
  } else if (strstr(name, "cm")) {
    snprintf(buf, len, "BANDA DE %.*s CENTIMETROS", static_cast<int>(strlen(name) - 2), name);
  } else if (name[0]) {
    snprintf(buf, len, "BANDA DE %.*s METROS", static_cast<int>(strlen(name) - 1), name);
  } else {
    snprintf(buf, len, "COBERTURA GENERAL  %lu MHz", static_cast<unsigned long>(lo / 1000000));
  }
}

class DialSkin : public Skin {
 public:
  const char* name() const override { return "Dial"; }
  const Theme& theme() const override { return kDialTheme; }
  const MainZones& zones() const override {
    static const MainZones z{kReadoutBox, kMode, kBand, kDialZone};
    return z;
  }

  void enter(TFT_eSPI& tft) override {
    glass_ = new TFT_eSprite(&tft);
    if (!glass_->createSprite(kGlassW, kGlassH)) {
      leave();
    }
  }

  void leave() override {
    if (glass_) {
      glass_->deleteSprite();
      delete glass_;
      glass_ = nullptr;
    }
  }

  void drawMainStatic(TFT_eSPI& tft) override {
    // Madera: vetas verticales con variaciones suaves y algunas líneas oscuras.
    for (int x = 0; x < 320; ++x) {
      float v = sinf(x * 0.13f) * 0.5f + sinf(x * 0.041f + 1.3f) * 0.35f + sinf(x * 0.29f + 2.0f) * 0.15f;
      tft.drawFastVLine(x, 0, 240, mix565(kWoodA, kWoodB, static_cast<uint8_t>((v + 1.0f) * 127.0f)));
    }
    uint32_t seed = 12345;
    for (int i = 0; i < 28; ++i) {
      seed = seed * 1103515245u + 12345u;
      int x = (seed >> 8) % 320;
      int drift = static_cast<int>((seed >> 20) % 17) - 8;
      tft.drawLine(x, 0, x + drift, 239, rgb(0x3f, 0x22, 0x0e));
    }
    tft.fillSmoothRoundRect(6, 6, 308, 86, 6, kFrameDark, kWood);
    tft.fillSmoothRoundRect(kMeterFrame.x, kMeterFrame.y, kMeterFrame.w, kMeterFrame.h, 4, kFrameDark, kWood);
    tft.fillRect(kMeterFace.x, kMeterFace.y, kMeterFace.w, kMeterFace.h, kIvory);
    lastRangeLo_ = lastRangeHi_ = 0;
    lastNeedleX_ = -1;
    lastPointerX_ = -1;
    meter_ = NeedleMotion{};
    scaleTx_ = -1;
  }

  void drawMainDynamic(TFT_eSPI& tft, const MainView& v, const MainView* p, uint32_t nowMs) override {
    const bool all = p == nullptr;
    const bool stale = !v.live;

    // Escala: se rehace al cambiar de banda o el texto de las esquinas
    uint32_t lo = 0, hi = 0;
    if (v.haveFreq) rigui::dialRange(v.hz, lo, hi);
    bool cornerChanged = all || p->link != v.link || p->model != v.model || p->lock != v.lock;
    if (all || lo != lastRangeLo_ || hi != lastRangeHi_ || cornerChanged) {
      buildGlass(v, lo, hi);
      lastRangeLo_ = lo;
      lastRangeHi_ = hi;
      lastNeedleX_ = -1;
      if (glass_) glass_->pushSprite(kGlassX, kGlassY);
    }
    // Aguja de la escala
    if (v.haveFreq && hi > lo) {
      int16_t nx = kGlassX + kScaleX0 +
                   static_cast<int16_t>((static_cast<uint64_t>(v.hz - lo) * (kScaleX1 - kScaleX0)) / (hi - lo));
      if (nx != lastNeedleX_) {
        if (lastNeedleX_ >= 0 && glass_) {  // borrar la anterior reponiendo el cristal
          int16_t sx = lastNeedleX_ - 7 - kGlassX;
          glass_->pushSprite(kGlassX + sx, kGlassY, sx, 0, 15, kGlassH);
        }
        tft.fillRect(nx - 1, kGlassY + 14, 3, kGlassH - 18, kRed);
        tft.fillRoundRect(nx - 6, kGlassY + 11, 13, 5, 1, rgb(0x7a, 0x1a, 0x12));
        lastNeedleX_ = nx;
      }
    }

    // Modo (con RX/TX) y banda (con VFO) en chapas de latón
    if (all || p->live != v.live || strcmp(p->mode, v.mode) != 0 || p->tx != v.tx) {
      plate(tft, kMode, v.mode, v.tx ? "EMITIENDO" : "RECEPCION", v.tx ? kRed : kInk);
    }
    if (all || p->live != v.live || strcmp(p->bandName, v.bandName) != 0 || p->vfo != v.vfo) {
      char b[12];
      size_t n = strlen(v.bandName);
      if (n > 1 && v.bandName[n - 1] == 'm' && v.bandName[n - 2] >= '0' && v.bandName[n - 2] <= '9') {
        snprintf(b, sizeof(b), "%.*s m", static_cast<int>(n - 1), v.bandName);
      } else {
        snprintf(b, sizeof(b), "%s", v.bandName);
      }
      plate(tft, kBand, b, v.vfo == 2 ? "VFO B" : v.vfo == 1 ? "VFO A" : "", kInk);
    }

    // Lectura digital con estados debajo
    if (all || p->live != v.live || p->hz != v.hz || p->haveFreq != v.haveFreq || p->split != v.split ||
        p->sqlClar != v.sqlClar) {
      const Rect& r = kReadoutBox;
      tft.fillSmoothRoundRect(r.x, r.y, r.w, r.h, 3, kReadout, kWood);
      tft.drawRoundRect(r.x, r.y, r.w, r.h, 3, rgb(0xc8, 0xa9, 0x6a));
      char buf[16];
      if (v.haveFreq) {
        uint32_t t = v.hz / 10;
        snprintf(buf, sizeof(buf), "%lu.%03lu,%02lu", static_cast<unsigned long>(t / 100000),
                 static_cast<unsigned long>((t / 100) % 1000), static_cast<unsigned long>(t % 100));
      } else {
        snprintf(buf, sizeof(buf), "---.---,--");
      }
      drawCentered(tft, oswaldSemi24, buf, r.x + r.w / 2, r.y + 15, stale ? rgb(0x8a, 0x76, 0x50) : kReadoutText,
                   kReadout);
      char st[32] = "";
      if (v.split) strcat(st, "SPLIT  ");
      if (v.sqlClar == 2) strcat(st, "CLAR");
      if (st[0]) drawCentered(tft, elite10, st, r.x + r.w / 2, r.y + r.h - 7, rgb(0xe0, 0x6a, 0x40), kReadout);
    }

    // S-meter horizontal: escala (cambia en TX) y puntero animado
    if (all || scaleTx_ != v.tx) {
      drawMeterScale(tft, v.tx);
      scaleTx_ = v.tx;
      lastPointerX_ = -1;
    }
    float target = v.live ? levelToNeedle(v.level, v.tx) : 0.0f;
    bool textChanged = all || strcmp(p->levelText, v.levelText) != 0 || p->highSwr != v.highSwr ||
                       p->haveMeters != v.haveMeters || p->meterSwr != v.meterSwr || p->meterAlc != v.meterAlc;
    if (meter_.step(target, nowMs) || textChanged || lastPointerX_ < 0) drawPointer(tft, v);
  }

  void drawButton(TFT_eSPI& tft, const Rect& r, const char* label, ButtonLook look, bool pressed,
                  bool big) override {
    uint16_t bg = pressed ? kKeyPressed : (look == ButtonLook::On ? kReadoutText : kKey);
    tft.fillSmoothRoundRect(r.x, r.y, r.w, r.h, 4, bg, kWood);
    tft.drawFastHLine(r.x + 4, r.y + r.h - 3, r.w - 8, kKeyShadow);
    if (look == ButtonLook::On) {
      tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, kRed);
      tft.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 3, kRed);
    }
    uint16_t fg = look == ButtonLook::Disabled ? kKeyShadow : kFrameDark;
    int16_t cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    if (isArrow(label)) {
      drawArrowShape(tft, cx, cy, label[0], fg, bg);
      return;
    }
    if (label[0] == '\x01') {
      drawArrowShape(tft, r.x + 16, cy, '<', fg, bg);
      FontScope f(tft, elite13);
      tft.setTextColor(fg, bg);
      tft.setTextDatum(ML_DATUM);
      tft.drawString(label + 1, r.x + 30, cy);
      return;
    }
    const char* nl = strchr(label, '\n');
    if (nl) {
      char top[16];
      snprintf(top, sizeof(top), "%.*s", static_cast<int>(nl - label), label);
      drawCentered(tft, elite10, top, cx, r.y + 12, rgb(0x7a, 0x5a, 0x22), bg);
      drawCentered(tft, elite13, nl + 1, cx, r.y + 29, fg, bg);
      return;
    }
    drawCentered(tft, big ? elite18 : elite13, label, cx, cy, fg, bg);
  }

  bool fitsBig(TFT_eSPI& tft, const char* label, const Rect& r) override {
    if (r.h < 30) return false;
    FontScope f(tft, elite18);
    return tft.textWidth(label) <= r.w - 10;
  }

 private:
  void buildGlass(const MainView& v, uint32_t lo, uint32_t hi) {
    if (!glass_) return;
    TFT_eSprite& s = *glass_;
    s.fillRectVGradient(0, 0, kGlassW, kGlassH, kGlassTop, kGlassBottom);
    s.loadFont(elite10);
    // Esquinas: modelo (y LOCK) a la izquierda, enlace a la derecha
    char left[24];
    snprintf(left, sizeof(left), "%s%s", v.model, v.lock ? "  LOCK" : "");
    s.setTextColor(rgb(0x6b, 0x4a, 0x1e), glassAt(6));
    s.setTextDatum(TL_DATUM);
    s.drawString(left, 6, 3);
    s.setTextDatum(TR_DATUM);
    s.setTextColor(v.linkKind == LinkKind::Ok ? rgb(0x2f, 0x6b, 0x3a) : kRed, glassAt(6));
    s.drawString(v.link, kGlassW - 6, 3);
    if (hi > lo) {
      char title[40];
      bandTitle(v.band >= 0 ? v.bandName : "", lo, title, sizeof(title));
      s.setTextDatum(TC_DATUM);
      s.setTextColor(rgb(0x6b, 0x4a, 0x1e), glassAt(16));
      s.drawString(title, kGlassW / 2, 15);
      // Paso de las marcas: el menor que deje como mucho 8 divisiones grandes
      static const uint32_t kMajors[] = {10000, 20000, 25000, 50000, 100000, 200000, 250000, 500000, 1000000, 2000000, 5000000};
      uint32_t major = kMajors[0];
      for (uint32_t m : kMajors) {
        major = m;
        if ((hi - lo) / m <= 8) break;
      }
      uint32_t minor = major / 5;
      s.drawFastHLine(kScaleX0, kBase, kScaleX1 - kScaleX0, kInk);
      for (uint32_t f = (lo + minor - 1) / minor * minor; f <= hi; f += minor) {
        int16_t x = kScaleX0 + static_cast<int16_t>((static_cast<uint64_t>(f - lo) * (kScaleX1 - kScaleX0)) / (hi - lo));
        bool big = f % major == 0;
        s.drawFastVLine(x, big ? kBase - 16 : kBase - 8, big ? 16 : 8, kInk);
        if (big) {
          char t[8];
          if (f % 1000000 == 0) snprintf(t, sizeof(t), "%lu", static_cast<unsigned long>(f / 1000000));
          else snprintf(t, sizeof(t), "%03lu", static_cast<unsigned long>(f / 1000 % 1000));
          s.setTextDatum(TC_DATUM);
          s.setTextColor(kInk, glassAt(kBase + 6));
          s.drawString(t, x, kBase + 5);
        }
      }
      s.setTextDatum(BR_DATUM);
      s.setTextColor(rgb(0x6b, 0x4a, 0x1e), glassAt(kGlassH - 4));
      s.drawString("MHz / kHz", kGlassW - 6, kGlassH - 3);
    }
    s.unloadFont();
  }

  void plate(TFT_eSPI& tft, const Rect& r, const char* big, const char* small, uint16_t smallColor) {
    tft.fillRectVGradient(r.x, r.y, r.w, r.h, kBrassTop, kBrassBottom);
    tft.drawRect(r.x, r.y, r.w, r.h, kBrassLine);
    uint16_t mid = mix565(kBrassTop, kBrassBottom, 110);
    drawCentered(tft, elite18, big, r.x + r.w / 2, r.y + 13, kFrameDark, mid);
    if (small[0]) drawCentered(tft, elite10, small, r.x + r.w / 2, r.y + r.h - 8, smallColor, kBrassBottom);
  }

  int16_t meterX(float pos) const { return kMx0 + static_cast<int16_t>(pos * (kMx1 - kMx0)); }

  void drawMeterScale(TFT_eSPI& tft, bool tx) {
    tft.fillRect(kMeterFace.x, kMeterFace.y, kMeterFace.w, kMeterFace.h, kIvory);
    tft.drawFastHLine(kMx0, kMBase, kMx1 - kMx0, kInk);
    FontScope f(tft, elite10);
    tft.setTextDatum(TC_DATUM);
    if (tx) {
      for (int i = 0; i <= 15; ++i) {
        int16_t x = meterX(i / 15.0f);
        tft.drawFastVLine(x, kMBase - (i % 5 ? 4 : 8), i % 5 ? 4 : 8, i > 10 ? kRed : kInk);
        if (i % 5 == 0) {
          char t[4];
          snprintf(t, sizeof(t), "%d", i);
          tft.setTextColor(kInk, kIvory);
          tft.drawString(t, x, kMeterFace.y + 2);
        }
      }
    } else {
      for (int i = 0; i <= 30; ++i) {
        float pos = i / 30.0f;
        int16_t x = meterX(pos);
        tft.drawFastVLine(x, kMBase - (i % 3 ? 4 : 8), i % 3 ? 4 : 8, pos > 0.55f ? kRed : kInk);
      }
      static const struct {
        const char* t;
        uint8_t level;
      } kMarks[] = {{"1", 1}, {"3", 3}, {"5", 5}, {"7", 7}, {"9", 9}, {"+20", 11}, {"+40", 13}, {"+60", 15}};
      for (const auto& m : kMarks) {
        tft.setTextColor(m.level > 9 ? kRed : kInk, kIvory);
        tft.drawString(m.t, meterX(levelToNeedle(m.level, false)), kMeterFace.y + 2);
      }
    }
    tft.setTextDatum(BL_DATUM);
    tft.setTextColor(kInk, kIvory);
    tft.drawString(tx ? "PO" : "S", kMeterFace.x + 4, kMeterFace.y + kMeterFace.h - 2);
  }

  void drawPointer(TFT_eSPI& tft, const MainView& v) {
    // Borrar la franja del puntero (debajo de la línea base) y redibujarlo con el valor
    tft.fillRect(kMx0 - 8, kMBase + 2, kMx1 - kMx0 + 10, kMeterFace.y + kMeterFace.h - kMBase - 2, kIvory);
    int16_t x = meterX(meter_.pos);
    tft.fillTriangle(x - 5, kMBase + 18, x + 5, kMBase + 18, x, kMBase + 2, kRed);
    char buf[32];
    if (!v.live) {
      snprintf(buf, sizeof(buf), "sin enlace");
    } else if (v.tx && v.highSwr) {
      snprintf(buf, sizeof(buf), "PO %s  ROE ALTA", v.levelText);
    } else if (v.tx && v.haveMeters) {
      snprintf(buf, sizeof(buf), "PO %s  SWR %u  ALC %u", v.levelText, v.meterSwr, v.meterAlc);
    } else {
      snprintf(buf, sizeof(buf), "%s", v.levelText);
    }
    FontScope f(tft, elite10);
    tft.setTextDatum(BR_DATUM);
    tft.setTextColor(v.tx && v.highSwr ? kRed : kInk, kIvory);
    tft.drawString(buf, kMeterFace.x + kMeterFace.w - 4, kMeterFace.y + kMeterFace.h - 2);
    lastPointerX_ = x;
  }

  TFT_eSprite* glass_ = nullptr;
  uint32_t lastRangeLo_ = 0, lastRangeHi_ = 0;
  int16_t lastNeedleX_ = -1;
  int16_t lastPointerX_ = -1;
  int scaleTx_ = -1;
  NeedleMotion meter_;
};

}  // namespace

Skin& dialSkin() {
  static DialSkin skin;
  return skin;
}

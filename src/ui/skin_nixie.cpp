// Piel «Nixie»: frecuencia en tubos Nixie naranjas con resplandor y S-meter como «ojo mágico»
// verde de radio de válvulas (ámbar en transmisión).
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "skin.h"
#include "skin_util.h"

namespace {

constexpr uint16_t kBg = rgb(0x07, 0x06, 0x05);
constexpr uint16_t kBox = rgb(0x0d, 0x09, 0x07);
constexpr uint16_t kBoxLine = rgb(0x3b, 0x2a, 0x1d);
constexpr uint16_t kOrange = rgb(0xff, 0x8a, 0x1a);
constexpr uint16_t kCore = rgb(0xff, 0xd0, 0x90);
constexpr uint16_t kGlow = rgb(0x9a, 0x3a, 0x04);
constexpr uint16_t kStaleCore = rgb(0x8a, 0x6a, 0x50);
constexpr uint16_t kStaleGlow = rgb(0x3a, 0x22, 0x14);
constexpr uint16_t kDimText = rgb(0x5a, 0x46, 0x36);
constexpr uint16_t kInfo = rgb(0x7a, 0x9a, 0x80);

const Theme kNixieTheme = {
    kBg,                    // bg
    rgb(0x16, 0x0e, 0x08),  // header
    rgb(0xff, 0xc2, 0x7a),  // text
    rgb(0x8a, 0x6a, 0x50),  // textDim
    kOrange,                // freq
    kStaleCore,             // freqStale
    kBox,                   // box
    rgb(0x12, 0x10, 0x0e),  // boxStale
    kBox,                   // button
    rgb(0x2a, 0x14, 0x06),  // buttonOn
    kOrange,                // buttonOnBorder
    rgb(0x2a, 0x14, 0x06),  // buttonPressed
    rgb(0x4a, 0x34, 0x24),  // buttonDisabledText
    kBox,                   // field
    rgb(0x2a, 0x8a, 0x3a),  // linkOk
    rgb(0xd0, 0x30, 0x20),  // linkLost
    rgb(0x2a, 0x8a, 0x3a),  // rx
    rgb(0xff, 0x40, 0x20),  // tx
    kOrange,                // flagSplit
    rgb(0x2a, 0xff, 0x6e),  // flagSql
    kOrange,                // flagClar
    kBoxLine,               // flagOff
    rgb(0x1a, 0x10, 0x08),  // meterOff
    rgb(0x2a, 0xff, 0x6e),  // sMeter
    kOrange,                // sMeterOver
    kOrange,                // po
    rgb(0xff, 0x40, 0x20),  // poHigh
    rgb(0xff, 0x40, 0x20),  // warn
    kOrange,                // accent
};

// Tubos: 8 dígitos «DDD.DDD.DD»
constexpr int16_t kTubeW = 33, kTubeH = 88, kTubeY = 6, kTubeGap = 4, kDotGap = 10;
constexpr Rect kTubes{6, kTubeY, 308, kTubeH};
// Ojo mágico
constexpr int16_t kEyeSize = 78, kEyeX = 14, kEyeY = 98, kEyeR = 33, kEyeIr = 10;
// Cajas y píldoras
constexpr Rect kMode{110, 100, 100, 40};
constexpr Rect kBand{214, 100, 100, 40};
constexpr int16_t kPillY = 146, kPillH = 22;
constexpr Rect kPills[3] = {{110, kPillY, 64, kPillH}, {180, kPillY, 64, kPillH}, {250, kPillY, 64, kPillH}};

class NixieSkin : public Skin {
 public:
  const char* name() const override { return "Nixie"; }
  const Theme& theme() const override { return kNixieTheme; }
  const MainZones& zones() const override {
    static const MainZones z{kTubes, kMode, kBand, {0, 0, 0, 0}};
    return z;
  }

  void enter(TFT_eSPI& tft) override {
    tube_ = new TFT_eSprite(&tft);
    work_ = new TFT_eSprite(&tft);
    eye_ = new TFT_eSprite(&tft);
    if (!tube_->createSprite(kTubeW, kTubeH) || !work_->createSprite(kTubeW, kTubeH) ||
        !eye_->createSprite(kEyeSize, kEyeSize)) {
      leave();
      return;
    }
    drawTubeGlass(*tube_);
  }

  void leave() override {
    for (TFT_eSprite** s : {&tube_, &work_, &eye_}) {
      if (*s) {
        (*s)->deleteSprite();
        delete *s;
        *s = nullptr;
      }
    }
  }

  void drawMainStatic(TFT_eSPI& tft) override {
    tft.fillScreen(kBg);
    lastDigits_[0] = '\0';
    eye_motion_ = NeedleMotion{};
  }

  void drawMainDynamic(TFT_eSPI& tft, const MainView& v, const MainView* p, uint32_t nowMs) override {
    const bool all = p == nullptr;
    const bool stale = !v.live;

    // Tubos
    char digits[9];
    if (v.haveFreq) {
      snprintf(digits, sizeof(digits), "%8lu", static_cast<unsigned long>(v.hz / 10 % 100000000UL));
      for (int i = 2; i < 8; ++i) {
        if (digits[i] == ' ') digits[i] = '0';
      }
    } else {
      memcpy(digits, "--------", 9);
    }
    if (all || p->live != v.live || strcmp(digits, lastDigits_) != 0) {
      int16_t x = kTubes.x + 2;
      for (int i = 0; i < 8; ++i) {
        if (all || p->live != v.live || digits[i] != lastDigits_[i]) drawTube(tft, x, digits[i], stale);
        x += kTubeW + kTubeGap;
        if (i == 2 || i == 5) {
          int16_t dx = x - kTubeGap / 2 + kDotGap / 2 - 2;
          tft.fillSmoothCircle(dx, kTubeY + kTubeH - 14, 5, stale ? kStaleGlow : kGlow, kBg);
          tft.fillSmoothCircle(dx, kTubeY + kTubeH - 14, 3, stale ? kStaleCore : kOrange, kStaleGlow);
          x += kDotGap - kTubeGap;
        }
      }
      memcpy(lastDigits_, digits, sizeof(digits));
    }

    // Modo y banda en nixie pequeño
    if (all || p->live != v.live || strcmp(p->mode, v.mode) != 0) box(tft, kMode, v.mode, stale);
    if (all || p->live != v.live || strcmp(p->bandName, v.bandName) != 0) box(tft, kBand, v.bandName, stale);

    // Píldoras: RX/TX, SPLIT, VFO
    if (all || p->tx != v.tx || p->live != v.live) {
      pill(tft, kPills[0], v.tx ? "TX" : "RX", v.live, v.tx ? rgb(0xff, 0x40, 0x20) : kOrange);
    }
    if (all || p->split != v.split) pill(tft, kPills[1], "SPLIT", v.split, kOrange);
    if (all || p->vfo != v.vfo) {
      pill(tft, kPills[2], v.vfo == 2 ? "VFO B" : v.vfo == 1 ? "VFO A" : "VFO", v.vfo != 0, kOrange);
    }

    // Línea de información: modelo, enlace, LOCK, CLAR
    if (all || p->link != v.link || p->model != v.model || p->lock != v.lock || p->sqlClar != v.sqlClar) {
      char buf[48];
      snprintf(buf, sizeof(buf), "%s  ·  %s%s%s", v.model, v.link, v.lock ? "  ·  LOCK" : "",
               v.sqlClar == 2 ? "  ·  CLAR" : "");
      tft.fillRect(110, 174, 210, 16, kBg);
      FontScope f(tft, oswald10);
      tft.setTextColor(v.linkKind == LinkKind::Ok ? kInfo : rgb(0xff, 0x8a, 0x1a), kBg);
      tft.setTextDatum(TL_DATUM);
      tft.drawString(buf, 112, 176);
    }

    // Ojo mágico y su rótulo
    float target = v.live ? levelToNeedle(v.level, v.tx) : 0.0f;
    bool txChanged = all || p->tx != v.tx || p->live != v.live;
    if (eye_motion_.step(target, nowMs) || txChanged) drawEye(tft, v.tx, v.live);
    if (all || strcmp(p->levelText, v.levelText) != 0 || p->highSwr != v.highSwr || p->tx != v.tx) {
      char buf[24];
      snprintf(buf, sizeof(buf), "%s%s%s", v.tx ? "PO " : "", v.levelText, v.highSwr ? "  ROE!" : "");
      tft.fillRect(0, 178, 106, 13, kBg);
      FontScope f(tft, oswald10);
      tft.setTextColor(v.highSwr ? rgb(0xff, 0x40, 0x20) : kInfo, kBg);
      tft.setTextDatum(TC_DATUM);
      tft.drawString(buf, kEyeX + kEyeSize / 2, 179);
    }
  }

  void drawButton(TFT_eSPI& tft, const Rect& r, const char* label, ButtonLook look, bool pressed,
                  bool big) override {
    uint16_t bg = pressed ? rgb(0x2a, 0x14, 0x06) : kBox;
    tft.fillSmoothRoundRect(r.x, r.y, r.w, r.h, 8, bg, kBg);
    uint16_t line = look == ButtonLook::On ? kOrange : rgb(0x5a, 0x2e, 0x0c);
    tft.drawSmoothRoundRect(r.x, r.y, 8, 7, r.w, r.h, line, kBg);
    uint16_t fg = look == ButtonLook::Disabled ? rgb(0x4a, 0x34, 0x24)
                  : look == ButtonLook::On     ? rgb(0xff, 0xc2, 0x7a)
                                               : rgb(0xff, 0x9a, 0x3a);
    int16_t cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    if (isArrow(label)) {
      drawArrowShape(tft, cx, cy, label[0], fg, bg);
      return;
    }
    if (label[0] == '\x01') {
      drawArrowShape(tft, r.x + 16, cy, '<', fg, bg);
      FontScope f(tft, oswald13);
      tft.setTextColor(fg, bg);
      tft.setTextDatum(ML_DATUM);
      tft.drawString(label + 1, r.x + 30, cy);
      return;
    }
    const char* nl = strchr(label, '\n');
    if (nl) {
      char top[16];
      snprintf(top, sizeof(top), "%.*s", static_cast<int>(nl - label), label);
      drawCentered(tft, oswald10, top, cx, r.y + 12, rgb(0x8a, 0x6a, 0x50), bg);
      drawCentered(tft, oswald13, nl + 1, cx, r.y + 29, fg, bg);
      return;
    }
    drawCentered(tft, big ? oswaldSemi20 : oswald13, label, cx, cy, fg, bg);
  }

  bool fitsBig(TFT_eSPI& tft, const char* label, const Rect& r) override {
    if (r.h < 30) return false;
    FontScope f(tft, oswaldSemi20);
    return tft.textWidth(label) <= r.w - 12;
  }

 private:
  // Cristal del tubo: degradado radial cálido, reflejo y rejilla del ánodo.
  void drawTubeGlass(TFT_eSprite& s) {
    s.fillSprite(kBg);
    s.fillSmoothRoundRect(0, 0, kTubeW, kTubeH, 14, rgb(0x16, 0x0c, 0x06), kBg);
    for (int r = 0; r < 12; ++r) {  // resplandor de fondo, más cálido en el centro
      uint16_t c = mix565(rgb(0x16, 0x0c, 0x06), rgb(0x34, 0x18, 0x06), r * 21);
      s.fillSmoothRoundRect(1 + r, 4 + r * 2, kTubeW - 2 - 2 * r, kTubeH - 8 - 4 * r, 12, c, kBg);
    }
    s.drawSmoothRoundRect(0, 0, 14, 13, kTubeW, kTubeH, rgb(0x3d, 0x33, 0x29), kBg);
    for (int k = 0; k < 7; ++k) s.drawFastHLine(6, 16 + k * 10, kTubeW - 12, rgb(0x2a, 0x1c, 0x12));
    s.fillRect(4, 10, 3, kTubeH - 24, rgb(0x2c, 0x26, 0x22));  // reflejo
  }

  void drawTube(TFT_eSPI& tft, int16_t x, char ch, bool stale) {
    if (!work_) {
      char t[2] = {ch, 0};
      drawCentered(tft, nixie54, t, x + kTubeW / 2, kTubeY + kTubeH / 2, kOrange, kBg);
      return;
    }
    memcpy(work_->getPointer(), tube_->getPointer(), kTubeW * kTubeH * 2);
    if (ch >= '0' && ch <= '9') {
      char t[2] = {ch, 0};
      TFT_eSprite& s = *work_;
      s.loadFont(nixie54);
      s.setTextDatum(MC_DATUM);
      // Halo: el dígito en naranja oscuro desplazado alrededor; encima, el núcleo claro.
      s.setTextColor(stale ? kStaleGlow : kGlow);
      for (int dx = -2; dx <= 2; dx += 2)
        for (int dy = -2; dy <= 2; dy += 2)
          if (dx || dy) s.drawString(t, kTubeW / 2 + dx, kTubeH / 2 + 4 + dy);
      s.setTextColor(stale ? kStaleCore : kOrange);
      s.drawString(t, kTubeW / 2, kTubeH / 2 + 4);
      s.setTextColor(stale ? kStaleCore : kCore);
      s.drawString(t, kTubeW / 2, kTubeH / 2 + 4);
      s.unloadFont();
    } else if (ch == '-') {
      work_->fillRoundRect(9, kTubeH / 2, kTubeW - 18, 3, 1, stale ? kStaleCore : kOrange);
    }
    work_->pushSprite(x, kTubeY);
  }

  void box(TFT_eSPI& tft, const Rect& r, const char* text, bool stale) {
    tft.fillSmoothRoundRect(r.x, r.y, r.w, r.h, 6, kBox, kBg);
    tft.drawSmoothRoundRect(r.x, r.y, 6, 5, r.w, r.h, kBoxLine, kBg);
    FontScope f(tft, nixie24);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(stale ? kStaleGlow : kGlow, kBox);
    tft.drawString(text, r.x + r.w / 2 + 1, r.y + r.h / 2 + 3);
    tft.setTextColor(stale ? kStaleCore : rgb(0xff, 0xb8, 0x6a), kBox);
    tft.drawString(text, r.x + r.w / 2, r.y + r.h / 2 + 2);
  }

  void pill(TFT_eSPI& tft, const Rect& r, const char* text, bool on, uint16_t color) {
    uint16_t fill = on ? rgb(0x1a, 0x0d, 0x06) : kBox;
    tft.fillSmoothRoundRect(r.x, r.y, r.w, r.h, r.h / 2, fill, kBg);
    tft.drawSmoothRoundRect(r.x, r.y, r.h / 2, r.h / 2 - 1, r.w, r.h, on ? color : kBoxLine, kBg);
    drawCentered(tft, oswald10, text, r.x + r.w / 2, r.y + r.h / 2, on ? color : kDimText, fill);
  }

  // Ojo mágico: disco verde con un sector oscuro que se cierra al subir la señal.
  void drawEye(TFT_eSPI& tft, bool tx, bool live) {
    int16_t cx = kEyeX + kEyeSize / 2, cy = kEyeY + kEyeSize / 2;
    if (!eye_) {
      tft.fillSmoothCircle(cx, cy, kEyeR, live ? rgb(0x2a, 0xff, 0x6e) : rgb(0x10, 0x30, 0x18), kBg);
      return;
    }
    TFT_eSprite& s = *eye_;
    int16_t c = kEyeSize / 2;
    uint16_t bright = tx ? rgb(0xff, 0xb0, 0x40) : rgb(0x3a, 0xff, 0x7a);
    uint16_t edge = tx ? rgb(0x7a, 0x3a, 0x06) : rgb(0x0d, 0x6e, 0x2a);
    uint16_t dark = tx ? rgb(0x1a, 0x0c, 0x04) : rgb(0x07, 0x14, 0x09);
    if (!live) {
      bright = rgb(0x20, 0x40, 0x28);
      edge = rgb(0x10, 0x22, 0x14);
    }
    // Fondo, anillo metálico y cristal oscuro; las esquinas del sprite quedan del color de fondo.
    s.fillSprite(kBg);
    s.fillSmoothCircle(c, c, kEyeR + 5, rgb(0x2a, 0x2a, 0x2a), kBg);
    s.fillSmoothCircle(c, c, kEyeR + 3, rgb(0x11, 0x11, 0x11), rgb(0x2a, 0x2a, 0x2a));
    for (int r = kEyeR; r > kEyeIr; --r) {  // del borde (oscuro) al centro (brillante)
      uint8_t t = static_cast<uint8_t>(255 * (kEyeR - r) / (kEyeR - kEyeIr));
      s.fillSmoothCircle(c, c, r, mix565(edge, bright, t), rgb(0x11, 0x11, 0x11));
    }
    // Sector en sombra: abierto (100°) sin señal, casi cerrado (6°) a fondo de escala.
    float span = 6.0f + (1.0f - eye_motion_.pos) * 94.0f;
    s.drawSmoothArc(c, c, kEyeR, kEyeIr, static_cast<uint32_t>(180 - span / 2), static_cast<uint32_t>(180 + span / 2),
                    dark, rgb(0x11, 0x11, 0x11));
    s.fillSmoothCircle(c, c, kEyeIr - 1, rgb(0x0b, 0x0b, 0x0b), dark);
    s.pushSprite(kEyeX, kEyeY);
  }

  TFT_eSprite* tube_ = nullptr;
  TFT_eSprite* work_ = nullptr;
  TFT_eSprite* eye_ = nullptr;
  char lastDigits_[9] = "";
  NeedleMotion eye_motion_;
};

}  // namespace

Skin& nixieSkin() {
  static NixieSkin skin;
  return skin;
}

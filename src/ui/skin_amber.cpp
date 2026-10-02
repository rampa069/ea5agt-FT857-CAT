// Piel «Ámbar»: display de 7 segmentos ámbar con segmentos apagados visibles, S-meter de aguja
// sobre esfera crema, LEDs de estado y teclas de baquelita.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "skin.h"
#include "skin_util.h"

namespace {

// Paleta
constexpr uint16_t kPanelTop = rgb(0x2a, 0x26, 0x20);
constexpr uint16_t kPanelBottom = rgb(0x17, 0x14, 0x0f);
constexpr uint16_t kWindow = rgb(0x0b, 0x07, 0x04);
constexpr uint16_t kFrame = rgb(0x5a, 0x4a, 0x2e);
constexpr uint16_t kAmber = rgb(0xff, 0xb0, 0x2e);
constexpr uint16_t kAmberDim = rgb(0x8a, 0x5c, 0x14);
constexpr uint16_t kGhost = rgb(0x2b, 0x1c, 0x08);
constexpr uint16_t kLabel = rgb(0x9b, 0x8c, 0x6c);
constexpr uint16_t kCream = rgb(0xe7, 0xdc, 0xc0);
constexpr uint16_t kFaceTop = rgb(0xf6, 0xe7, 0xbf);
constexpr uint16_t kFaceBottom = rgb(0xe2, 0xcf, 0x9c);
constexpr uint16_t kFaceMid = rgb(0xec, 0xdb, 0xad);
constexpr uint16_t kInk = rgb(0x2b, 0x22, 0x19);
constexpr uint16_t kRed = rgb(0xb3, 0x26, 0x1e);
constexpr uint16_t kNeedle = rgb(0x15, 0x15, 0x15);
constexpr uint16_t kStrip = rgb(0x2a, 0x26, 0x20);
constexpr uint16_t kKey = rgb(0x2e, 0x2b, 0x27);
constexpr uint16_t kKeyPressed = rgb(0x4a, 0x45, 0x3d);
constexpr uint16_t kKeyHighlight = rgb(0x5b, 0x57, 0x4f);
constexpr uint16_t kKeyShadow = rgb(0x12, 0x10, 0x0d);

const Theme kAmberTheme = {
    kPanelBottom,              // bg
    kPanelTop,                 // header
    kCream,                    // text
    kLabel,                    // textDim
    kAmber,                    // freq
    kAmberDim,                 // freqStale
    kWindow,                   // box
    rgb(0x1a, 0x16, 0x11),     // boxStale
    kKey,                      // button
    rgb(0x4a, 0x3a, 0x1a),     // buttonOn
    kAmber,                    // buttonOnBorder
    kKeyPressed,               // buttonPressed
    rgb(0x5a, 0x53, 0x48),     // buttonDisabledText
    kWindow,                   // field
    rgb(0x2f, 0x6b, 0x2a),     // linkOk
    kRed,                      // linkLost
    rgb(0x2f, 0x6b, 0x2a),     // rx
    kRed,                      // tx
    kAmber,                    // flagSplit
    rgb(0x8f, 0xbf, 0x6a),     // flagSql
    rgb(0xff, 0x8a, 0x1a),     // flagClar
    rgb(0x3a, 0x33, 0x29),     // flagOff
    kGhost,                    // meterOff
    kAmber,                    // sMeter
    kRed,                      // sMeterOver
    kAmber,                    // po
    kRed,                      // poHigh
    kRed,                      // warn
    kAmber,                    // accent
};

// Ventana de frecuencia y dígitos
constexpr Rect kWin{6, 6, 308, 62};
constexpr int16_t kDigW = 26, kDigH = 44, kSegT = 5, kDigPitch = 30, kDotCell = 10, kDigY = 18;
// Medidor
constexpr Rect kMeter{6, 74, 196, 112};
constexpr int16_t kFaceX = 12, kFaceY = 80, kFaceW = 184, kFaceH = 100;
constexpr float kCx = 92, kCy = 110, kR = 92;  // pivote de la aguja (coordenadas del sprite)
// Cajas
constexpr Rect kMode{208, 74, 106, 40};
constexpr Rect kBand{208, 118, 106, 40};
constexpr int16_t kLedY = 168;
constexpr int16_t kLedX[4] = {222, 250, 278, 304};

const char kSegs[10][8] = {"abcdef", "bc", "abged", "abgcd", "fgbc", "afgcd", "afgedc", "abc", "abcdefg", "abcdfg"};

float angleFor(float v) { return (-58.0f + v * 116.0f) * static_cast<float>(M_PI) / 180.0f; }

class AmberSkin : public Skin {
 public:
  const char* name() const override { return "Ambar"; }
  const Theme& theme() const override { return kAmberTheme; }
  const MainZones& zones() const override {
    static const MainZones z{kWin, kMode, kBand, {0, 0, 0, 0}};
    return z;
  }

  void enter(TFT_eSPI& tft) override {
    tft_ = &tft;
    face_ = new TFT_eSprite(&tft);
    work_ = new TFT_eSprite(&tft);
    if (!face_->createSprite(kFaceW, kFaceH) || !work_->createSprite(kFaceW, kFaceH)) {
      leave();  // sin memoria: el medidor se dibuja sin aguja animada
      return;
    }
    drawFace(*face_);
  }

  void leave() override {
    if (face_) {
      face_->deleteSprite();
      delete face_;
      face_ = nullptr;
    }
    if (work_) {
      work_->deleteSprite();
      delete work_;
      work_ = nullptr;
    }
  }

  void drawMainStatic(TFT_eSPI& tft) override {
    tft.fillRectVGradient(0, 0, 320, 240, kPanelTop, kPanelBottom);
    tft.fillSmoothRoundRect(kWin.x, kWin.y, kWin.w, kWin.h, 4, kWindow, kPanelTop);
    tft.drawRoundRect(kWin.x, kWin.y, kWin.w, kWin.h, 4, kFrame);
    tft.fillSmoothRoundRect(kMeter.x, kMeter.y, kMeter.w, kMeter.h, 4, rgb(0x0e, 0x0c, 0x09), kPanelTop);
    tft.drawRoundRect(kMeter.x, kMeter.y, kMeter.w, kMeter.h, 4, kFrame);
    {
      FontScope f(tft, oswald10);
      tft.setTextColor(rgb(0xa8, 0x74, 0x1e), kWindow);
      tft.setTextDatum(BR_DATUM);
      tft.drawString("MHz", kWin.x + kWin.w - 8, kWin.y + kWin.h - 4);
      tft.setTextDatum(TC_DATUM);
      const char* labels[3] = {"TX", "BUSY", "SPLIT"};
      for (int i = 0; i < 3; ++i) {
        tft.setTextColor(kLabel, kPanelBottom);
        tft.drawString(labels[i], kLedX[i], kLedY + 9);
      }
    }
    lastDigits_[0] = '\0';
    needle_ = NeedleMotion{};
  }

  void drawMainDynamic(TFT_eSPI& tft, const MainView& v, const MainView* p, uint32_t nowMs) override {
    const bool all = p == nullptr;

    // Línea de información dentro de la ventana: modelo, VFO, LOCK, CLAR
    if (all || p->model != v.model || p->vfo != v.vfo || p->lock != v.lock || p->sqlClar != v.sqlClar) {
      char buf[48];
      snprintf(buf, sizeof(buf), "%s   %s%s%s", v.model, v.vfo == 2 ? "VFO-B" : v.vfo == 1 ? "VFO-A" : "",
               v.lock ? "   LOCK" : "", v.sqlClar == 2 ? "   CLAR" : "");
      tft.fillRect(kWin.x + 6, kWin.y + 3, 200, 12, kWindow);
      FontScope f(tft, oswald10);
      tft.setTextColor(rgb(0x7a, 0x5a, 0x22), kWindow);
      tft.setTextDatum(TL_DATUM);
      tft.drawString(buf, kWin.x + 8, kWin.y + 3);
    }

    // Dígitos (sólo los que cambian)
    char digits[9];
    if (v.haveFreq) {
      snprintf(digits, sizeof(digits), "%8lu", static_cast<unsigned long>(v.hz / 10 % 100000000UL));
      for (int i = 2; i < 8; ++i) {  // al menos las unidades de MHz y lo que sigue
        if (digits[i] == ' ') digits[i] = '0';
      }
    } else {
      memcpy(digits, "--------", 9);
    }
    bool stale = !v.live;
    if (all || p->live != v.live || strcmp(digits, lastDigits_) != 0) {
      int16_t x = kWin.x + kWin.w - 6 - (8 * kDigPitch - 4 + 2 * kDotCell);
      for (int i = 0; i < 8; ++i) {
        if (all || p->live != v.live || digits[i] != lastDigits_[i]) drawDigit(tft, x, digits[i], stale);
        x += kDigPitch;
        if (i == 2 || i == 5) {
          tft.fillRect(x + 1, kDigY + kDigH - kSegT, kSegT, kSegT, stale ? kAmberDim : kAmber);
          x += kDotCell;
        }
      }
      memcpy(lastDigits_, digits, sizeof(digits));
    }

    // Modo y banda
    if (all || p->live != v.live || strcmp(p->mode, v.mode) != 0) box(tft, kMode, v.mode, stale);
    if (all || p->live != v.live || strcmp(p->bandName, v.bandName) != 0) {
      char b[12];
      size_t n = strlen(v.bandName);
      // «20m» -> «20 m» queda más de época
      if (n > 1 && v.bandName[n - 1] == 'm' && v.bandName[n - 2] >= '0' && v.bandName[n - 2] <= '9') {
        snprintf(b, sizeof(b), "%.*s m", static_cast<int>(n - 1), v.bandName);
      } else {
        snprintf(b, sizeof(b), "%s", v.bandName);
      }
      box(tft, kBand, b, stale);
    }

    // LEDs
    if (all || p->tx != v.tx) led(tft, 0, v.tx ? rgb(0xff, 0x30, 0x20) : rgb(0x3a, 0x14, 0x10));
    bool busy = v.live && !v.tx && v.sqlClar != 1;
    bool pBusy = p && p->live && !p->tx && p->sqlClar != 1;
    if (all || busy != pBusy) led(tft, 1, busy ? rgb(0x57, 0xf0, 0x4a) : rgb(0x1c, 0x3a, 0x12));
    if (all || p->split != v.split) led(tft, 2, v.split ? kAmber : rgb(0x2a, 0x26, 0x18));
    if (all || p->linkKind != v.linkKind || p->link != v.link) {
      uint16_t c = v.linkKind == LinkKind::Ok     ? rgb(0x57, 0xf0, 0x4a)
                   : v.linkKind == LinkKind::Lost ? rgb(0xa0, 0x18, 0x10)
                                                  : kAmber;
      led(tft, 3, c);
      tft.fillRect(kLedX[3] - 14, kLedY + 8, 28, 14, kPanelBottom);
      FontScope f(tft, oswald10);
      tft.setTextColor(kLabel, kPanelBottom);
      tft.setTextDatum(TC_DATUM);
      tft.drawString(v.link[0] == 'B' ? "BT" : "CAT", kLedX[3], kLedY + 9);
    }

    // Medidor de aguja
    float target = v.live ? levelToNeedle(v.level, v.tx) : 0.0f;
    bool textChanged = all || p->tx != v.tx || p->highSwr != v.highSwr || p->haveMeters != v.haveMeters ||
                       p->meterSwr != v.meterSwr || p->meterAlc != v.meterAlc ||
                       strcmp(p->levelText, v.levelText) != 0;
    if (needle_.step(target, nowMs) || textChanged) drawMeter(tft, v);
  }

  void drawButton(TFT_eSPI& tft, const Rect& r, const char* label, ButtonLook look, bool pressed,
                  bool big) override {
    uint16_t bg = pressed ? kKeyPressed : kKey;
    uint16_t panel = kPanelBottom;
    tft.fillSmoothRoundRect(r.x, r.y, r.w, r.h, 5, bg, panel);
    tft.drawFastHLine(r.x + 5, r.y + 2, r.w - 10, kKeyHighlight);
    tft.drawFastHLine(r.x + 5, r.y + r.h - 2, r.w - 10, kKeyShadow);
    if (look == ButtonLook::On) tft.drawRoundRect(r.x, r.y, r.w, r.h, 5, kAmber);
    uint16_t fg = look == ButtonLook::Disabled ? rgb(0x5a, 0x53, 0x48) : look == ButtonLook::On ? kAmber : kCream;
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
      drawCentered(tft, oswald10, top, cx, r.y + 12, kLabel, bg);
      drawCentered(tft, oswald13, nl + 1, cx, r.y + 29, fg, bg);
      return;
    }
    drawCentered(tft, big ? oswaldSemi20 : oswald13, label, cx, cy, fg, bg);
  }

  bool fitsBig(TFT_eSPI& tft, const char* label, const Rect& r) override {
    if (r.h < 30) return false;
    FontScope f(tft, oswaldSemi20);
    return tft.textWidth(label) <= r.w - 10;
  }

 private:
  void drawDigit(TFT_eSPI& tft, int16_t x, char ch, bool stale) {
    const char* on = ch >= '0' && ch <= '9' ? kSegs[ch - '0'] : (ch == '-' ? "g" : "");
    uint16_t lit = stale ? kAmberDim : kAmber;
    const int16_t T = kSegT, W = kDigW, H = kDigH, y = kDigY;
    const int16_t G = (H - T) / 2, hw = W - 2 * T + 2, vh = G - T + 2;
    struct S {
      char id;
      int16_t sx, sy, sw, sh;
    } segs[7] = {{'a', T - 1, 0, hw, T}, {'g', T - 1, G, hw, T},      {'d', T - 1, H - T, hw, T},
                 {'f', 0, T - 1, T, vh}, {'b', W - T, T - 1, T, vh},  {'e', 0, G + T - 1, T, vh},
                 {'c', W - T, G + T - 1, T, vh}};
    for (const S& s : segs) {
      bool isOn = strchr(on, s.id) != nullptr;
      tft.fillRoundRect(x + s.sx, y + s.sy, s.sw, s.sh, 1, isOn ? lit : kGhost);
    }
  }

  void box(TFT_eSPI& tft, const Rect& r, const char* text, bool stale) {
    tft.fillSmoothRoundRect(r.x, r.y, r.w, r.h, 3, kWindow, kPanelTop);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 3, kFrame);
    drawCentered(tft, oswaldSemi24, text, r.x + r.w / 2, r.y + r.h / 2, stale ? kAmberDim : kAmber, kWindow);
  }

  void led(TFT_eSPI& tft, int i, uint16_t color) {
    tft.fillSmoothCircle(kLedX[i], kLedY, 6, rgb(0x08, 0x07, 0x05), kPanelBottom);
    tft.fillSmoothCircle(kLedX[i], kLedY, 4, color, rgb(0x08, 0x07, 0x05));
    tft.drawPixel(kLedX[i] - 1, kLedY - 2, mix565(color, 0xFFFF, 140));  // brillo
  }

  // Esfera fija: degradado, arcos, marcas y números.
  void drawFace(TFT_eSprite& s) {
    s.fillRectVGradient(0, 0, kFaceW, kFaceH, kFaceTop, kFaceBottom);
    s.drawSmoothArc(kCx, kCy, kR, kR - 2, 122, 186, kInk, kFaceMid);
    s.drawSmoothArc(kCx, kCy, kR + 1, kR - 2, 186, 238, kRed, kFaceMid);
    for (int i = 0; i <= 20; ++i) {
      float v = i / 20.0f, a = angleFor(v), l = (i % 2) ? 5.0f : 9.0f;
      uint16_t c = v > 0.55f ? kRed : kInk;
      s.drawWideLine(kCx + kR * sinf(a), kCy - kR * cosf(a), kCx + (kR - l) * sinf(a), kCy - (kR - l) * cosf(a), 1.4f,
                     c, kFaceMid);
    }
    static const struct {
      const char* t;
      float v;
    } kMarks[] = {{"1", 0.06f}, {"3", 0.18f}, {"5", 0.30f}, {"7", 0.42f}, {"9", 0.55f}, {"+20", 0.72f}, {"+40", 0.86f}, {"+60", 0.98f}};
    s.loadFont(oswald10);
    s.setTextDatum(MC_DATUM);
    for (const auto& m : kMarks) {
      float a = angleFor(m.v);
      s.setTextColor(m.v > 0.55f ? kRed : kInk, kFaceMid);
      s.drawString(m.t, kCx + (kR - 18) * sinf(a), kCy - (kR - 18) * cosf(a));
    }
    s.unloadFont();
  }

  void drawMeter(TFT_eSPI& tft, const MainView& v) {
    if (!work_) {  // sin sprites: sólo el texto del nivel
      drawCentered(tft, oswaldSemi20, v.levelText, kMeter.x + kMeter.w / 2, kMeter.y + kMeter.h / 2, kAmber,
                   rgb(0x0e, 0x0c, 0x09));
      return;
    }
    memcpy(work_->getPointer(), face_->getPointer(), kFaceW * kFaceH * 2);
    TFT_eSprite& s = *work_;
    // Aguja
    float a = angleFor(needle_.pos);
    float sx = sinf(a), cy = cosf(a);
    s.drawWedgeLine(kCx, kCy, kCx + (kR - 4) * sx, kCy - (kR - 4) * cy, 2.4f, 1.0f, kNeedle, kFaceMid);
    s.drawWedgeLine(kCx + (kR - 30) * sx, kCy - (kR - 30) * cy, kCx + (kR - 4) * sx, kCy - (kR - 4) * cy, 1.6f, 1.0f,
                    rgb(0xc0, 0x28, 0x1e), kFaceMid);
    // Rótulos S / PO (el activo resaltado) y franja inferior con el valor
    s.loadFont(oswald10);
    s.setTextDatum(TL_DATUM);
    s.setTextColor(v.tx ? rgb(0xa8, 0x9c, 0x80) : kInk, kFaceTop);
    s.drawString("S", 8, 4);
    s.setTextDatum(TR_DATUM);
    s.setTextColor(v.tx ? kInk : rgb(0xa8, 0x9c, 0x80), kFaceTop);
    s.drawString("PO", kFaceW - 8, 4);
    s.fillRect(0, kFaceH - 14, kFaceW, 14, kStrip);
    char buf[32];
    if (!v.live) {
      snprintf(buf, sizeof(buf), "SIN ENLACE");
    } else if (v.tx && v.highSwr) {
      snprintf(buf, sizeof(buf), "PO %s   ROE ALTA", v.levelText);
    } else if (v.tx && v.haveMeters) {
      snprintf(buf, sizeof(buf), "PO %s   SWR %u   ALC %u", v.levelText, v.meterSwr, v.meterAlc);
    } else if (v.tx) {
      snprintf(buf, sizeof(buf), "POTENCIA %s", v.levelText);
    } else {
      snprintf(buf, sizeof(buf), "SEÑAL %s", v.levelText);
    }
    s.setTextDatum(MC_DATUM);
    s.setTextColor(v.tx && v.highSwr ? rgb(0xff, 0x50, 0x40) : kLabel, kStrip);
    s.drawString(buf, kFaceW / 2, kFaceH - 7);
    s.unloadFont();
    s.pushSprite(kFaceX, kFaceY);
  }

  TFT_eSPI* tft_ = nullptr;
  TFT_eSprite* face_ = nullptr;
  TFT_eSprite* work_ = nullptr;
  char lastDigits_[9] = "";
  NeedleMotion needle_;
};

}  // namespace

Skin& amberSkin() {
  static AmberSkin skin;
  return skin;
}

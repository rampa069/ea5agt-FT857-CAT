// Piel «Clásico»: el diseño original (7 segmentos amarillos, barra de segmentos, cajas azules).
#include <stdio.h>
#include <string.h>

#include "skin.h"

namespace {

constexpr int16_t W = 320;
constexpr int16_t HEADER_H = 20;
constexpr int16_t FREQ_Y = 26;  // fuente 7: 48 px
constexpr int16_t ROW_Y = 90;
constexpr int16_t ROW_H = 44;
constexpr int16_t METER_TITLE_Y = 138;
constexpr int16_t METER_Y = 155;
constexpr int16_t METER_H = 17;
constexpr int16_t SCALE_Y = 174;
constexpr int SEGMENTS = 15;
constexpr int16_t SEG_X0 = 40;
constexpr int16_t SEG_W = 16;
constexpr int16_t SEG_GAP = 2;

int16_t segmentX(int i) { return SEG_X0 + i * (SEG_W + SEG_GAP); }

class ClassicSkin : public Skin {
 public:
  const char* name() const override { return "Clasico"; }
  const Theme& theme() const override { return kDefaultTheme; }
  const MainZones& zones() const override {
    static const MainZones z{{0, 22, W, 56}, {4, ROW_Y, 92, ROW_H}, {100, ROW_Y, 76, ROW_H}, {0, 0, 0, 0}};
    return z;
  }

  void drawMainStatic(TFT_eSPI& tft) override {
    const Theme& th = theme();
    tft.fillScreen(th.bg);
    tft.fillRect(0, 0, W, HEADER_H, th.header);
    tft.setTextColor(th.textDim, th.bg);
    tft.setTextDatum(TR_DATUM);
    tft.setTextPadding(0);
    tft.drawString("MHz", W - 8, 78, 1);
  }

  void drawMainDynamic(TFT_eSPI& tft, const MainView& v, const MainView* p, uint32_t) override {
    const Theme& th = theme();
    const bool all = p == nullptr;
    const bool linkChanged = all || p->live != v.live;

    // Cabecera: modelo, VFO, LOCK y enlace
    if (all || p->model != v.model || p->vfo != v.vfo || p->lock != v.lock) {
      tft.fillRect(0, 0, 230, HEADER_H, th.header);
      tft.setTextColor(th.text, th.header);
      tft.setTextDatum(ML_DATUM);
      tft.setTextPadding(0);
      tft.drawString(v.model, 6, HEADER_H / 2, 2);
      if (v.vfo) tft.drawString(v.vfo == 2 ? "VFO B" : "VFO A", 96, HEADER_H / 2, 2);
      if (v.lock) {
        tft.setTextColor(th.accent, th.header);
        tft.drawString("LOCK", 170, HEADER_H / 2, 2);
      }
    }
    if (linkChanged || p->link != v.link) {
      bool noCat = v.linkKind == LinkKind::NoCat;
      uint16_t c = v.linkKind == LinkKind::Ok           ? th.linkOk
                   : noCat                              ? th.po
                   : v.linkKind == LinkKind::Connecting ? th.boxStale
                                                        : th.linkLost;
      tft.fillRoundRect(236, 1, 80, 18, 4, c);
      tft.setTextColor(noCat ? th.bg : th.text, c);
      tft.setTextDatum(MC_DATUM);
      tft.setTextPadding(0);
      tft.drawString(v.link, 276, 10, 2);
    }

    // Frecuencia (atenuada sin enlace)
    if (all || linkChanged || p->haveFreq != v.haveFreq || p->hz != v.hz) {
      char buf[16];
      tft.setTextDatum(TR_DATUM);
      tft.setTextPadding(W - 20);
      if (v.haveFreq) {
        formatFreq(v.hz, buf, sizeof(buf));
        tft.setTextColor(v.live ? th.freq : th.freqStale, th.bg);
      } else {
        snprintf(buf, sizeof(buf), "---.---.--");
        tft.setTextColor(th.freqStale, th.bg);
      }
      tft.drawString(buf, W - 10, FREQ_Y, 7);
      tft.setTextPadding(0);
    }

    // Modo y banda
    uint16_t box = v.live ? th.box : th.boxStale;
    if (linkChanged || strcmp(p->mode, v.mode) != 0) {
      tft.fillRoundRect(4, ROW_Y, 92, ROW_H, 6, box);
      tft.setTextColor(th.text, box);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(v.mode, 50, ROW_Y + ROW_H / 2 + 1, 4);
    }
    if (linkChanged || strcmp(p->bandName, v.bandName) != 0) {
      tft.fillRoundRect(100, ROW_Y, 76, ROW_H, 6, box);
      tft.setTextColor(th.text, box);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(v.bandName, 138, ROW_Y + ROW_H / 2 + 1, tft.textWidth(v.bandName, 4) <= 70 ? 4 : 2);
    }

    // Indicadores SPLIT y SQL/CLAR
    auto flag = [&](int16_t y, const char* label, bool on, uint16_t color) {
      tft.fillRoundRect(182, y, 58, 20, 3, on ? color : th.bg);
      tft.drawRoundRect(182, y, 58, 20, 3, on ? color : th.flagOff);
      tft.setTextColor(on ? th.bg : th.flagOff, on ? color : th.bg);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(label, 211, y + 10, 2);
    };
    if (all || p->split != v.split) flag(ROW_Y, "SPLIT", v.split, th.flagSplit);
    if (all || p->sqlClar != v.sqlClar) {
      if (v.sqlClar == 2) flag(ROW_Y + 24, "CLAR", true, th.flagClar);
      else flag(ROW_Y + 24, "SQL", v.sqlClar == 1, th.flagSql);
    }

    // RX / TX
    if (linkChanged || p->tx != v.tx) {
      uint16_t c = !v.live ? th.boxStale : (v.tx ? th.tx : th.rx);
      tft.fillRoundRect(246, ROW_Y, 70, ROW_H, 6, c);
      tft.setTextColor(th.text, c);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(v.tx ? "TX" : "RX", 281, ROW_Y + ROW_H / 2 + 1, 4);
    }

    // Medidor
    if (all || p->tx != v.tx) drawScale(tft, v.tx);
    if (all || p->level != v.level || p->tx != v.tx || p->highSwr != v.highSwr || p->haveMeters != v.haveMeters ||
        p->meterSwr != v.meterSwr || p->meterAlc != v.meterAlc || strcmp(p->levelText, v.levelText) != 0) {
      tft.fillRect(0, METER_TITLE_Y, W, 16, th.bg);
      tft.setTextPadding(0);
      tft.setTextColor(th.textDim, th.bg);
      tft.setTextDatum(TL_DATUM);
      tft.drawString(v.tx ? "PO" : "S", 8, METER_TITLE_Y, 2);
      if (v.highSwr) {
        tft.setTextColor(th.warn, th.bg);
        tft.setTextDatum(TC_DATUM);
        tft.drawString("HI SWR", W / 2, METER_TITLE_Y, 2);
      } else if (v.tx && v.haveMeters) {
        char buf[24];
        snprintf(buf, sizeof(buf), "SWR %u   ALC %u", v.meterSwr, v.meterAlc);
        tft.setTextColor(th.textDim, th.bg);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(buf, W / 2, METER_TITLE_Y, 2);
      }
      tft.setTextColor(th.text, th.bg);
      tft.setTextDatum(TR_DATUM);
      tft.drawString(v.levelText, W - 8, METER_TITLE_Y, 2);
      for (int i = 0; i < SEGMENTS; ++i) {
        uint16_t c = th.meterOff;
        if (i < v.level) c = v.tx ? (i < 10 ? th.po : th.poHigh) : (i < 9 ? th.sMeter : th.sMeterOver);
        tft.fillRect(segmentX(i), METER_Y, SEG_W, METER_H, c);
      }
    }
  }

  void drawButton(TFT_eSPI& tft, const Rect& r, const char* label, ButtonLook look, bool pressed,
                  bool big) override {
    const Theme& th = theme();
    uint16_t bg = pressed ? th.buttonPressed : (look == ButtonLook::On ? th.buttonOn : th.button);
    uint16_t fg = look == ButtonLook::Disabled ? th.buttonDisabledText : th.text;
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 5, bg);
    if (look == ButtonLook::On) {
      tft.drawRoundRect(r.x, r.y, r.w, r.h, 5, th.buttonOnBorder);
      tft.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 4, th.buttonOnBorder);
    }
    int16_t cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    tft.setTextPadding(0);
    tft.setTextColor(fg, bg);
    tft.setTextDatum(MC_DATUM);
    if (isArrow(label)) {
      drawArrowShape(tft, cx, cy, label[0], fg, bg);
      return;
    }
    if (label[0] == '\x01') {  // volver
      drawArrowShape(tft, r.x + 16, cy, '<', fg, bg);
      tft.drawString(label + 1, r.x + 50, cy, 2);
      return;
    }
    const char* nl = strchr(label, '\n');
    if (nl) {  // dos líneas: rótulo pequeño + valor
      char top[16];
      snprintf(top, sizeof(top), "%.*s", static_cast<int>(nl - label), label);
      tft.setTextColor(th.textDim, bg);
      tft.drawString(top, cx, r.y + 11, 1);
      tft.setTextColor(fg, bg);
      tft.drawString(nl + 1, cx, r.y + 28, 2);
      return;
    }
    tft.drawString(label, cx, cy + 1, big ? 4 : 2);
  }

  bool fitsBig(TFT_eSPI& tft, const char* label, const Rect& r) override {
    return r.h >= 30 && tft.textWidth(label, 4) <= r.w - 6;
  }

 private:
  static void formatFreq(uint32_t hz, char* buf, size_t len) {
    uint32_t t = hz / 10;
    snprintf(buf, len, "%lu.%03lu.%02lu", static_cast<unsigned long>(t / 100000),
             static_cast<unsigned long>((t / 100) % 1000), static_cast<unsigned long>(t % 100));
  }

  void drawScale(TFT_eSPI& tft, bool tx) {
    const Theme& th = theme();
    tft.fillRect(0, SCALE_Y, W, 16, th.bg);
    tft.setTextColor(th.textDim, th.bg);
    tft.setTextDatum(TC_DATUM);
    tft.setTextPadding(0);
    if (tx) {
      for (int v = 0; v <= SEGMENTS; v += 5) {
        int16_t x = v == 0 ? SEG_X0 : segmentX(v - 1) + SEG_W;
        tft.drawNumber(v, x, SCALE_Y, 2);
      }
      return;
    }
    static const struct {
      int level;
      const char* label;
    } kMarks[] = {{1, "1"}, {3, "3"}, {5, "5"}, {7, "7"}, {9, "9"}, {11, "+20"}, {13, "+40"}, {15, "+60"}};
    for (const auto& m : kMarks) {
      tft.drawString(m.label, segmentX(m.level - 1) + SEG_W / 2, SCALE_Y, 2);
    }
  }
};

}  // namespace

Skin& classicSkin() {
  static ClassicSkin skin;
  return skin;
}

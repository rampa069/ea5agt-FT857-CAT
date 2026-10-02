#include "rig_display.h"

#include "rig_format.h"

using ft8x7::RigState;

namespace {

constexpr int W = 320;

constexpr int HEADER_H = 22;
constexpr int FREQ_Y = 32;    // fuente 7: 48 px de alto
constexpr int MHZ_Y = 84;
constexpr int ROW_Y = 104;    // modo, indicadores y TX/RX
constexpr int ROW_H = 32;
constexpr int METER_TITLE_Y = 146;
constexpr int METER_Y = 166;
constexpr int METER_H = 26;
constexpr int SCALE_Y = 196;

constexpr int SEGMENTS = 15;
constexpr int SEG_X0 = 40;
constexpr int SEG_W = 16;
constexpr int SEG_GAP = 2;

constexpr uint16_t COLOR_BG = TFT_BLACK;
constexpr uint16_t COLOR_HEADER = 0x10A2;  // gris azulado oscuro
constexpr uint16_t COLOR_DIM = 0x2945;     // segmento apagado / indicador inactivo
constexpr uint16_t COLOR_FREQ = TFT_YELLOW;

int segmentX(int i) { return SEG_X0 + i * (SEG_W + SEG_GAP); }

uint16_t segmentColor(int i, bool tx) {
  if (tx) {
    return i < 10 ? TFT_ORANGE : TFT_RED;
  }
  return i < 9 ? TFT_GREEN : TFT_RED;  // por encima de S9 en rojo
}

}  // namespace

void RigDisplay::begin(const char* modelName, uint32_t baud) {
  model_ = modelName;
  baud_ = baud;
  tft_.fillScreen(COLOR_BG);
  drawHeader();

  tft_.setTextColor(TFT_DARKGREY, COLOR_BG);
  tft_.setTextDatum(TR_DATUM);
  tft_.setTextPadding(0);
  tft_.drawString("MHz", W - 10, MHZ_Y, 2);

  lastLinked_ = lastFreqColor_ = lastRawMode_ = lastSplit_ = lastSql_ = -1;
  lastTx_ = lastMeterLevel_ = lastMeterTx_ = lastSwr_ = -1;
  lastFreqHz_ = 0;
  lastFooterMs_ = 0;
}

void RigDisplay::update(const RigState& s) {
  if (lastLinked_ != s.linked) {
    drawLink(s.linked);
  }

  drawFrequency(s);
  drawMode(s);

  int split = s.linked && s.tx.split;  // la radio sólo lo informa en TX
  if (lastSplit_ != split) {
    drawFlag(112, "SPLIT", split, TFT_CYAN);
    lastSplit_ = split;
  }
  int sql = s.linked && !s.tx.transmitting && s.rx.squelched;
  if (lastSql_ != sql) {
    drawFlag(180, "SQL", sql, TFT_SKYBLUE);
    lastSql_ = sql;
  }

  int tx = s.linked && s.tx.transmitting;
  if (lastTx_ != tx) {
    drawTxRx(tx);
    lastTx_ = tx;
  }

  int level = !s.linked ? 0 : (tx ? s.tx.poMeter : s.rx.sMeter);
  int swr = tx && s.tx.highSwr;
  if (lastMeterTx_ != tx) {
    drawMeterScale(tx);
  }
  if (lastMeterLevel_ != level || lastMeterTx_ != tx || lastSwr_ != swr) {
    drawMeterTitle(s);
    drawMeterBar(level, tx);
    lastMeterLevel_ = level;
    lastMeterTx_ = tx;
    lastSwr_ = swr;
  }

  uint32_t now = millis();
  if (lastFooterMs_ == 0 || now - lastFooterMs_ >= 1000) {
    drawFooter(s);
    lastFooterMs_ = now;
  }
}

void RigDisplay::drawHeader() {
  tft_.fillRect(0, 0, W, HEADER_H, COLOR_HEADER);
  tft_.setTextColor(TFT_WHITE, COLOR_HEADER);
  tft_.setTextDatum(ML_DATUM);
  tft_.setTextPadding(0);
  tft_.drawString(model_, 6, HEADER_H / 2, 2);
}

void RigDisplay::drawLink(bool linked) {
  const int w = 78;
  uint16_t color = linked ? TFT_DARKGREEN : TFT_RED;
  tft_.fillRoundRect(W - w - 4, 2, w, HEADER_H - 4, 4, color);
  tft_.setTextColor(TFT_WHITE, color);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextPadding(0);
  tft_.drawString(linked ? "CAT OK" : "NO LINK", W - 4 - w / 2, HEADER_H / 2, 2);
  lastLinked_ = linked;
}

void RigDisplay::drawFrequency(const RigState& s) {
  if (!s.haveFreq) {
    if (lastFreqColor_ != -2) {
      tft_.setTextColor(TFT_DARKGREY, COLOR_BG);
      tft_.setTextDatum(TR_DATUM);
      tft_.setTextPadding(W - 20);
      tft_.drawString("---.---.--", W - 10, FREQ_Y, 7);
      lastFreqColor_ = -2;
    }
    return;
  }

  // Con el enlace caído se mantiene el último valor, atenuado.
  int color = s.linked ? COLOR_FREQ : TFT_DARKGREY;
  if (s.freq.hz == lastFreqHz_ && color == lastFreqColor_) {
    return;
  }
  char buf[16];
  ft8x7::formatFrequency(s.freq.hz, buf, sizeof(buf));
  tft_.setTextColor(color, COLOR_BG);
  tft_.setTextDatum(TR_DATUM);
  tft_.setTextPadding(W - 20);
  tft_.drawString(buf, W - 10, FREQ_Y, 7);
  lastFreqHz_ = s.freq.hz;
  lastFreqColor_ = color;
}

void RigDisplay::drawMode(const RigState& s) {
  int raw = s.haveFreq ? s.freq.rawMode : -2;
  if (raw == lastRawMode_) {
    return;
  }
  char buf[8] = "---";
  if (s.haveFreq) {
    ft8x7::formatMode(s.freq, buf, sizeof(buf));
  }
  tft_.fillRoundRect(8, ROW_Y, 96, ROW_H, 6, TFT_NAVY);
  tft_.setTextColor(TFT_WHITE, TFT_NAVY);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextPadding(0);
  tft_.drawString(buf, 8 + 48, ROW_Y + ROW_H / 2 + 1, 4);
  lastRawMode_ = raw;
}

void RigDisplay::drawFlag(int x, const char* label, bool on, uint16_t onColor) {
  const int w = 60;
  tft_.fillRoundRect(x, ROW_Y + 4, w, ROW_H - 8, 4, on ? onColor : COLOR_BG);
  tft_.drawRoundRect(x, ROW_Y + 4, w, ROW_H - 8, 4, on ? onColor : COLOR_DIM);
  tft_.setTextColor(on ? TFT_BLACK : COLOR_DIM, on ? onColor : COLOR_BG);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextPadding(0);
  tft_.drawString(label, x + w / 2, ROW_Y + ROW_H / 2, 2);
}

void RigDisplay::drawTxRx(bool tx) {
  const int x = 250;
  const int w = 62;
  uint16_t color = tx ? TFT_RED : TFT_DARKGREEN;
  tft_.fillRoundRect(x, ROW_Y, w, ROW_H, 6, color);
  tft_.setTextColor(TFT_WHITE, color);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextPadding(0);
  tft_.drawString(tx ? "TX" : "RX", x + w / 2, ROW_Y + ROW_H / 2 + 1, 4);
}

void RigDisplay::drawMeterTitle(const RigState& s) {
  bool tx = s.linked && s.tx.transmitting;
  tft_.fillRect(0, METER_TITLE_Y, W, 18, COLOR_BG);
  tft_.setTextPadding(0);

  tft_.setTextColor(TFT_LIGHTGREY, COLOR_BG);
  tft_.setTextDatum(TL_DATUM);
  tft_.drawString(tx ? "PO" : "S", 10, METER_TITLE_Y, 2);

  if (tx && s.tx.highSwr) {
    tft_.setTextColor(TFT_RED, COLOR_BG);
    tft_.setTextDatum(TC_DATUM);
    tft_.drawString("HI SWR", W / 2, METER_TITLE_Y, 2);
  }

  char buf[12] = "";
  if (!s.linked) {
    snprintf(buf, sizeof(buf), "--");
  } else if (tx) {
    snprintf(buf, sizeof(buf), "%u", s.tx.poMeter);
  } else {
    ft8x7::formatSMeter(s.rx.sMeter, buf, sizeof(buf));
  }
  tft_.setTextColor(TFT_WHITE, COLOR_BG);
  tft_.setTextDatum(TR_DATUM);
  tft_.drawString(buf, W - 10, METER_TITLE_Y, 2);
}

void RigDisplay::drawMeterBar(uint8_t level, bool tx) {
  for (int i = 0; i < SEGMENTS; ++i) {
    uint16_t color = i < level ? segmentColor(i, tx) : COLOR_DIM;
    tft_.fillRect(segmentX(i), METER_Y, SEG_W, METER_H, color);
  }
}

void RigDisplay::drawMeterScale(bool tx) {
  tft_.fillRect(0, SCALE_Y, W, 16, COLOR_BG);
  tft_.setTextColor(TFT_DARKGREY, COLOR_BG);
  tft_.setTextDatum(TC_DATUM);
  tft_.setTextPadding(0);
  if (tx) {
    for (int v = 0; v <= SEGMENTS; v += 5) {
      int x = v == 0 ? SEG_X0 : segmentX(v - 1) + SEG_W;
      tft_.drawNumber(v, x, SCALE_Y, 2);
    }
    return;
  }
  // Etiqueta centrada bajo el segmento que se enciende al alcanzar ese nivel.
  static const struct { int level; const char* label; } kMarks[] = {
      {1, "1"}, {3, "3"}, {5, "5"}, {7, "7"}, {9, "9"}, {11, "+20"}, {13, "+40"}, {15, "+60"},
  };
  for (const auto& m : kMarks) {
    tft_.drawString(m.label, segmentX(m.level - 1) + SEG_W / 2, SCALE_Y, 2);
  }
}

void RigDisplay::drawFooter(const RigState& s) {
  char buf[48];
  tft_.setTextColor(TFT_DARKGREY, COLOR_BG);
  tft_.setTextDatum(BL_DATUM);
  tft_.setTextPadding(200);
  snprintf(buf, sizeof(buf), "ok %lu  err %lu", static_cast<unsigned long>(s.okCount),
           static_cast<unsigned long>(s.errorCount));
  tft_.drawString(buf, 6, 238, 1);

  tft_.setTextDatum(BR_DATUM);
  tft_.setTextPadding(0);
  snprintf(buf, sizeof(buf), "CAT %lu 8N2", static_cast<unsigned long>(baud_));
  tft_.drawString(buf, W - 6, 238, 1);
}

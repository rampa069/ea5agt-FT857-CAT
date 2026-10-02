// Aplicación de prueba y calibración del táctil XPT2046 (env touchtest).
// 1) Pide tocar 5 cruces y calcula los rangos crudos (TOUCH_RAW_*), también por serie.
// 2) Modo prueba: punto donde se toca y una barra de botones de 44 px como la de la maqueta.

#include <Arduino.h>
#include <algorithm>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "board_cyd.h"

static TFT_eSPI tft;
static SPIClass touchSpi(VSPI);
static XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

namespace {

constexpr int W = 320;
constexpr int H = 240;
constexpr int MARGIN = 20;

struct Target {
  int x, y;
  const char* name;
};
constexpr Target kTargets[] = {
    {MARGIN, MARGIN, "arriba-izq"},
    {W - MARGIN, MARGIN, "arriba-der"},
    {W - MARGIN, H - MARGIN, "abajo-der"},
    {MARGIN, H - MARGIN, "abajo-izq"},
    {W / 2, H / 2, "centro"},
};
constexpr int kTargetCount = sizeof(kTargets) / sizeof(kTargets[0]);

struct Raw {
  int x, y;
};
Raw raws[kTargetCount];

struct Calibration {
  int xMin, xMax, yMin, yMax;
  bool swapXY;
} cal;

void drawCross(int x, int y, uint16_t color) {
  tft.drawLine(x - 12, y, x + 12, y, color);
  tft.drawLine(x, y - 12, x, y + 12, color);
  tft.drawCircle(x, y, 6, color);
}

void message(const char* line1, const char* line2 = "") {
  tft.fillRect(40, 90, 240, 60, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(line1, W / 2, 105, 2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString(line2, W / 2, 130, 2);
}

// Espera a que no haya contacto durante quietMs seguidos (el resistivo "parpadea" al soltar).
void waitRelease(uint32_t quietMs = 250) {
  uint32_t since = millis();
  while (millis() - since < quietMs) {
    if (touch.touched()) {
      since = millis();
    }
    delay(5);
  }
}

// Espera un toque firme (>= 5 lecturas con presión) y devuelve la mediana de las muestras.
Raw readPress() {
  for (;;) {
    waitRelease();
    while (!touch.touched()) {
      delay(5);
    }
    constexpr int kMax = 40;
    int xs[kMax], ys[kMax], n = 0;
    uint32_t start = millis();
    while (n < kMax && millis() - start < 500) {
      TS_Point p = touch.getPoint();
      if (p.z > 400) {
        xs[n] = p.x;
        ys[n] = p.y;
        ++n;
      }
      delay(8);
    }
    if (n < 5) {
      continue;  // roce o rebote: no cuenta
    }
    std::sort(xs, xs + n);
    std::sort(ys, ys + n);
    return {xs[n / 2], ys[n / 2]};
  }
}

// Extrapola de las cruces (a MARGIN px del borde) a los bordes 0 y size-1.
void edgeRange(int rawLow, int rawHigh, int size, int& outMin, int& outMax) {
  float perPx = float(rawHigh - rawLow) / float(size - 1 - 2 * MARGIN);
  outMin = int(rawLow - perPx * MARGIN);
  outMax = int(rawHigh + perPx * MARGIN);
}

void computeCalibration() {
  // Si al moverse a la derecha cambia más la Y cruda que la X, los ejes están cruzados.
  int dxRaw = abs(raws[1].x - raws[0].x);
  int dyRaw = abs(raws[1].y - raws[0].y);
  cal.swapXY = dyRaw > dxRaw;
  auto rx = [](const Raw& r) { return cal.swapXY ? r.y : r.x; };
  auto ry = [](const Raw& r) { return cal.swapXY ? r.x : r.y; };

  int left = (rx(raws[0]) + rx(raws[3])) / 2;
  int right = (rx(raws[1]) + rx(raws[2])) / 2;
  int top = (ry(raws[0]) + ry(raws[1])) / 2;
  int bottom = (ry(raws[2]) + ry(raws[3])) / 2;
  edgeRange(left, right, W, cal.xMin, cal.xMax);
  edgeRange(top, bottom, H, cal.yMin, cal.yMax);
}

void toScreen(const Raw& r, int& x, int& y) {
  int rx = cal.swapXY ? r.y : r.x;
  int ry = cal.swapXY ? r.x : r.y;
  x = constrain(map(rx, cal.xMin, cal.xMax, 0, W - 1), 0, W - 1);
  y = constrain(map(ry, cal.yMin, cal.yMax, 0, H - 1), 0, H - 1);
}

void runCalibration() {
  tft.fillScreen(TFT_BLACK);
  for (int i = 0; i < kTargetCount; ++i) {
    const Target& t = kTargets[i];
    char line[40];
    snprintf(line, sizeof(line), "Toca la cruz (%d/%d)", i + 1, kTargetCount);
    message(line, "con el dedo, firme");
    if (i == kTargetCount - 1) {
      tft.fillRect(40, 90, 240, 60, TFT_BLACK);  // el centro queda libre para la cruz
    }
    drawCross(t.x, t.y, TFT_YELLOW);
    raws[i] = readPress();
    // Un toque casi idéntico al de la cruz anterior es un rebote: repetir.
    if (i > 0 && abs(raws[i].x - raws[i - 1].x) < 200 && abs(raws[i].y - raws[i - 1].y) < 200) {
      Serial.printf("cruz %-10s descartada (igual que la anterior)\n", t.name);
      --i;
      continue;
    }
    drawCross(t.x, t.y, TFT_DARKGREEN);
    Serial.printf("cruz %-10s pantalla=(%3d,%3d) crudo=(%4d,%4d)\n", t.name, t.x, t.y, raws[i].x,
                  raws[i].y);
  }
  computeCalibration();

  Serial.println();
  Serial.println("// Pegar en include/board_cyd.h");
  Serial.printf("constexpr bool TOUCH_SWAP_XY = %s;\n", cal.swapXY ? "true" : "false");
  Serial.printf("constexpr int TOUCH_RAW_X_MIN = %d;\n", cal.xMin);
  Serial.printf("constexpr int TOUCH_RAW_X_MAX = %d;\n", cal.xMax);
  Serial.printf("constexpr int TOUCH_RAW_Y_MIN = %d;\n", cal.yMin);
  Serial.printf("constexpr int TOUCH_RAW_Y_MAX = %d;\n", cal.yMax);

  Serial.println("\nError de cada cruz con la calibración nueva:");
  for (int i = 0; i < kTargetCount; ++i) {
    int x, y;
    toScreen(raws[i], x, y);
    Serial.printf("  %-10s error=(%+d,%+d) px\n", kTargets[i].name, x - kTargets[i].x, y - kTargets[i].y);
  }
}

// --- Modo prueba ---
constexpr int BTN_Y = 192, BTN_H = 44;
constexpr int kBtnX[] = {4, 68, 132, 196, 258};
constexpr int kBtnW[] = {60, 60, 60, 58, 58};
const char* const kBtnLabel[] = {"<", "PASO", ">", "A/B", "MENU"};

void drawButtons(int pressed) {
  for (int i = 0; i < 5; ++i) {
    uint16_t bg = i == pressed ? 0x4A7F : 0x1946;
    tft.fillRoundRect(kBtnX[i], BTN_Y, kBtnW[i], BTN_H, 5, bg);
    tft.setTextColor(TFT_WHITE, bg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(kBtnLabel[i], kBtnX[i] + kBtnW[i] / 2, BTN_Y + BTN_H / 2, 2);
  }
}

void startTestMode() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Prueba: toca donde quieras", W / 2, 6, 2);
  char buf[64];
  snprintf(buf, sizeof(buf), "X %d..%d  Y %d..%d%s", cal.xMin, cal.xMax, cal.yMin, cal.yMax,
           cal.swapXY ? "  XY cruzados" : "");
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString(buf, W / 2, 26, 2);
  for (int i = 0; i < kTargetCount; ++i) {
    drawCross(kTargets[i].x, kTargets[i].y, TFT_DARKGREY);
  }
  drawButtons(-1);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSpi);
  touch.setRotation(1);
  Serial.println("\n=== Calibración táctil XPT2046 ===");
  runCalibration();
  startTestMode();
}

void loop() {
  if (!touch.touched()) {
    delay(10);
    return;
  }
  TS_Point p = touch.getPoint();
  if (p.z < 400) {
    return;
  }
  int x, y;
  toScreen({p.x, p.y}, x, y);

  int hit = -1;
  if (y >= BTN_Y && y < BTN_Y + BTN_H) {
    for (int i = 0; i < 5; ++i) {
      if (x >= kBtnX[i] && x < kBtnX[i] + kBtnW[i]) hit = i;
    }
  }
  if (hit >= 0) {
    drawButtons(hit);
    Serial.printf("boton %s (%d,%d)\n", kBtnLabel[hit], x, y);
    while (touch.touched()) delay(10);
    drawButtons(-1);
    return;
  }
  tft.fillCircle(x, y, 2, TFT_GREEN);
  char buf[32];
  snprintf(buf, sizeof(buf), "  x=%3d y=%3d z=%4d  ", x, y, p.z);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(buf, W / 2, 170, 2);
  delay(15);
}

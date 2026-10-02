#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "board_cyd.h"

static TFT_eSPI tft;
static SPIClass touchSpi(VSPI);
static XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

static void drawSplash() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Yaesu CAT Display", tft.width() / 2, 10, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("FT-817 / 818 / 857", tft.width() / 2, 45, 2);
  tft.drawString("Toca la pantalla", tft.width() / 2, 80, 2);
  tft.drawRect(0, 0, tft.width(), tft.height(), TFT_DARKGREY);
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, HIGH);

  tft.init();
  tft.setRotation(1);  // apaisado 320x240, USB a la derecha
  drawSplash();

  touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSpi);
  touch.setRotation(1);

  Serial.printf("CYD listo: %dx%d\n", tft.width(), tft.height());
}

void loop() {
  if (!touch.tirqTouched() || !touch.touched()) {
    return;
  }

  TS_Point p = touch.getPoint();
  int x = map(p.x, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, 0, tft.width() - 1);
  int y = map(p.y, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, 0, tft.height() - 1);
  x = constrain(x, 0, tft.width() - 1);
  y = constrain(y, 0, tft.height() - 1);

  Serial.printf("touch raw=(%d,%d) z=%d -> (%d,%d)\n", p.x, p.y, p.z, x, y);

  tft.fillCircle(x, y, 3, TFT_GREEN);
  char buf[32];
  snprintf(buf, sizeof(buf), "  x=%3d y=%3d  ", x, y);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(BC_DATUM);
  tft.drawString(buf, tft.width() / 2, tft.height() - 6, 2);

  delay(20);
}

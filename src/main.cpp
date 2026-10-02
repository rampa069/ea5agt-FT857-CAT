#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "arduino_cat_port.h"
#include "board_cyd.h"
#include "config.h"
#include "rig_display.h"
#include "rig_format.h"
#include "rig_poller.h"

static TFT_eSPI tft;
static SPIClass touchSpi(VSPI);
static XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
static RigDisplay display(tft);

static ft8x7::ArduinoCatPort catPort(Serial2);
static ft8x7::Ft8x7Cat cat(catPort);
static ft8x7::RigPoller poller(cat);

// Copia del estado que publica la tarea CAT y lee la UI.
static ft8x7::RigState sharedState;
static portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

// Tarea en el núcleo 0: el sondeo bloquea hasta 200 ms por comando y no debe frenar la UI.
static void catTask(void*) {
  for (;;) {
    if (poller.step(millis())) {
      portENTER_CRITICAL(&stateMux);
      sharedState = poller.state();
      portEXIT_CRITICAL(&stateMux);
    } else {
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
}

static ft8x7::RigState snapshot() {
  portENTER_CRITICAL(&stateMux);
  ft8x7::RigState s = sharedState;
  portEXIT_CRITICAL(&stateMux);
  return s;
}

static void logState(const ft8x7::RigState& s) {
  char freq[16] = "---";
  char mode[8] = "---";
  if (s.haveFreq) {
    ft8x7::formatFrequency(s.freq.hz, freq, sizeof(freq));
    ft8x7::formatMode(s.freq, mode, sizeof(mode));
  }
  Serial.printf("[%s] %s MHz %s %s S=%u PO=%u%s ok=%lu err=%lu\n", s.linked ? "LINK" : "----", freq,
                mode, s.tx.transmitting ? "TX" : "RX", s.rx.sMeter, s.tx.poMeter,
                s.tx.highSwr ? " HI-SWR" : "", static_cast<unsigned long>(s.okCount),
                static_cast<unsigned long>(s.errorCount));
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
  tft.setRotation(1);  // apaisado 320x240
  display.begin(RIG_MODEL_NAME, CAT_BAUD);

  touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSpi);
  touch.setRotation(1);

  catPort.begin(CAT_BAUD, CAT_RX_PIN, CAT_TX_PIN);
  xTaskCreatePinnedToCore(catTask, "cat", 4096, nullptr, 1, nullptr, 0);

  Serial.printf("CAT %s a %lu baudios, RX=IO%d TX=IO%d\n", RIG_MODEL_NAME,
                static_cast<unsigned long>(CAT_BAUD), CAT_RX_PIN, CAT_TX_PIN);
}

void loop() {
  static uint32_t lastLog = 0;

  ft8x7::RigState s = snapshot();
  display.update(s);

  // LED de la placa: rojo en TX, verde con enlace, apagado sin enlace (activo a nivel bajo).
  digitalWrite(LED_R, !(s.linked && s.tx.transmitting));
  digitalWrite(LED_G, !(s.linked && !s.tx.transmitting));

  if (millis() - lastLog >= 1000) {
    lastLog = millis();
    logState(s);
  }
  delay(30);
}

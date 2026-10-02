#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include <atomic>

#include "arduino_cat_port.h"
#include "board_cyd.h"
#include "rig_format.h"
#include "rig_poller.h"
#include "touch_filter.h"
#include "ui/ui.h"
#include "ui_settings.h"

static TFT_eSPI tft;
static SPIClass touchSpi(VSPI);
static XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

#if CAT_OVER_USB
// El CAT ocupa el UART0 del USB: no se puede usar Serial para log.
static ft8x7::ArduinoCatPort catPort(Serial);
#define LOG(...) \
  do {           \
  } while (0)
#else
static ft8x7::ArduinoCatPort catPort(Serial2);
#define LOG(...) Serial.printf(__VA_ARGS__)
#endif
static ft8x7::Ft8x7Cat cat(catPort);
static ft8x7::RigPoller poller(cat);

// Copia del estado que publica la tarea CAT y lee la UI.
static ft8x7::RigState sharedState;
static portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
// Cambio de baudios pedido desde Ajustes; lo aplica la tarea CAT, dueña del puerto.
static std::atomic<uint32_t> pendingBaud{0};

static rigui::Settings settings;
static Preferences prefs;
static bool settingsDirty = false;
static uint32_t settingsDirtySinceMs = 0;
constexpr uint32_t kLazySaveMs = 30000;  // la memoria de bandas cambia a menudo: no gastar la flash

static rigui::TouchFilter touchFilter;

static void beginCatPort(uint32_t baud) {
#if CAT_OVER_USB
  catPort.begin(baud, -1, -1);  // pines por defecto del UART0 (GPIO3/GPIO1)
#else
  catPort.begin(baud, CAT_RX_PIN, CAT_TX_PIN);
#endif
}

// Tarea en el núcleo 0: el sondeo bloquea hasta 200 ms por comando y no debe frenar la UI.
static void catTask(void*) {
  for (;;) {
    uint32_t baud = pendingBaud.exchange(0);
    if (baud) {
      beginCatPort(baud);
    }
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

static void applyBrightness() { analogWrite(TFT_BL, settings.brightness * 255 / 100); }

static void loadSettings() {
  const rigui::TouchCal defaultTouch{TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX,
                                     TOUCH_SWAP_XY};
  prefs.begin("cydcat", false);
  bool ok = prefs.getBytesLength("cfg") == sizeof(settings) &&
            prefs.getBytes("cfg", &settings, sizeof(settings)) == sizeof(settings) && settings.valid();
  if (!ok) {
    settings.setDefaults(defaultTouch);
  }
  LOG("Ajustes %s\n", ok ? "cargados de NVS" : "por defecto");
}

static void saveSettings() {
  prefs.putBytes("cfg", &settings, sizeof(settings));
  settingsDirty = false;
  LOG("Ajustes guardados\n");
}

class Host : public UiHost {
 public:
  bool sendCat(const ft8x7::Command& cmd) override { return poller.enqueue(cmd); }

  void settingsChanged(bool urgent) override {
    if (urgent) {
      applyBrightness();
      if (settings.baud != appliedBaud_) {
        appliedBaud_ = settings.baud;
        pendingBaud = settings.baud;
      }
      saveSettings();
    } else if (!settingsDirty) {
      settingsDirty = true;
      settingsDirtySinceMs = millis();
    }
  }

  uint32_t appliedBaud_ = 0;
};

static Host host;
static Ui ui(tft, kDefaultTheme, settings, host);

static void pollTouch(const ft8x7::RigState& s, uint32_t now) {
  int16_t rx = 0, ry = 0;
  bool contact = touch.touched();
  if (contact) {
    TS_Point p = touch.getPoint();
    contact = p.z > 400;
    rx = p.x;
    ry = p.y;
  }
  rigui::TouchEvent raw = touchFilter.feed(contact, rx, ry, now);
  if (!raw) {
    return;
  }
  rigui::TouchEvent ev = raw;
  rigui::mapTouch(settings.touch, raw.x, raw.y, tft.width(), tft.height(), ev.x, ev.y);
  ui.onTouch(ev, rigui::RawPoint{raw.x, raw.y}, s, now);
}

static void logState(const ft8x7::RigState& s) {
  char freq[16] = "---";
  char mode[8] = "---";
  if (s.haveFreq) {
    ft8x7::formatFrequency(s.freq.hz, freq, sizeof(freq));
    ft8x7::formatMode(s.freq, mode, sizeof(mode));
  }
  LOG("[%s] %s MHz %s %s S=%u PO=%u%s ok=%lu err=%lu wr=%lu\n", s.linked ? "LINK" : "----", freq, mode,
      s.tx.transmitting ? "TX" : "RX", s.rx.sMeter, s.tx.poMeter, s.tx.highSwr ? " HI-SWR" : "",
      static_cast<unsigned long>(s.okCount), static_cast<unsigned long>(s.errorCount),
      static_cast<unsigned long>(s.writeCount));
  (void)freq;
  (void)mode;
}

void setup() {
#if !CAT_OVER_USB
  Serial.begin(115200);
#endif

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, HIGH);

  loadSettings();

  tft.init();
  tft.setRotation(1);  // apaisado 320x240
  applyBrightness();

  touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSpi);
  touch.setRotation(1);

  host.appliedBaud_ = settings.baud;
  beginCatPort(settings.baud);
  xTaskCreatePinnedToCore(catTask, "cat", 4096, nullptr, 1, nullptr, 0);

  ui.begin();
  LOG("CAT %s a %lu baudios\n", rigui::modelName(settings.model), static_cast<unsigned long>(settings.baud));
}

void loop() {
  static uint32_t lastLog = 0;
  uint32_t now = millis();

  ft8x7::RigState s = snapshot();
  pollTouch(s, now);
  ui.update(s, now);

  // LED de la placa: rojo en TX, verde con enlace, apagado sin enlace (activo a nivel bajo).
  digitalWrite(LED_R, !(s.linked && s.tx.transmitting));
  digitalWrite(LED_G, !(s.linked && !s.tx.transmitting));

  if (settingsDirty && now - settingsDirtySinceMs >= kLazySaveMs) {
    saveSettings();
  }
  if (now - lastLog >= 1000) {
    lastLog = now;
    logState(s);
  }
  delay(8);
}

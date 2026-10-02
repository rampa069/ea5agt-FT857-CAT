#pragma once

// Interfaz táctil completa (docs/ui.md): pantalla principal y subpantallas de modo, banda,
// teclado, menú, clarificador, repetidor/tono, ajustes, diagnóstico y calibración.
// Sólo dibuja y decide; el CAT y la persistencia van por UiHost.

#include <TFT_eSPI.h>

#include "rig_poller.h"
#include "rig_ui_logic.h"
#include "bt_types.h"
#include "skin.h"
#include "theme.h"
#include "touch_filter.h"
#include "ui_settings.h"

class UiHost {
 public:
  virtual ~UiHost() = default;
  virtual bool sendCat(const ft8x7::Command& cmd) = 0;  // encola; false si la cola está llena
  // urgent: aplicar ya (brillo, baudios, táctil) y guardar; si no, guardar cuando convenga.
  virtual void settingsChanged(bool urgent) = 0;

  // Bluetooth (transporte alternativo al cable)
  virtual rigui::BtStatus btStatus() = 0;
  virtual size_t btResults(rigui::BtFound* out, size_t max) = 0;
  virtual void btScan() = 0;
  virtual void btConnect(const rigui::BtDevice& device) = 0;
  virtual void btForget() = 0;
};

class Ui {
 public:
  enum class Screen : uint8_t {
    Main, Mode, Band, Keypad, Menu, Clar, Repeater, Settings, Display, Bluetooth, Diag, Calibrate
  };

  Ui(TFT_eSPI& tft, rigui::Settings& settings, UiHost& host) : tft_(tft), settings_(settings), host_(host) {}

  void begin();
  void update(const ft8x7::RigState& s, uint32_t nowMs);
  // ev en píxeles de pantalla; raw con las coordenadas crudas (para calibrar).
  void onTouch(const rigui::TouchEvent& ev, rigui::RawPoint raw, const ft8x7::RigState& s, uint32_t nowMs);
  void show(Screen screen);

 private:
  enum class Action : uint8_t {
    None, OpenKeypad, OpenMode, OpenBand, OpenMenu, Back, TuneDown, TuneUp, Step, ToggleVfo,
    SetMode, SetBand, Key, KeyDel, KeyClear, KeyOk, Split, OpenClar, OpenRepeater, Lock,
    OpenSettings, OpenDiag, ClarToggle, ClarDelta, RptShift, RptOffset, ToneMode, ToneValue,
    SetModel, SetBaud, Brightness, Calibrate,
    SetTransport, OpenDisplay, OpenBluetooth, BtScan, BtSelect, BtPin, BtForget, SetInvert, BtPage,
    SetSkin, DialTune,
  };
  enum class Style : uint8_t { Normal, On, Disabled, Custom };

  struct Button {
    int16_t x, y, w, h;
    Action action;
    int16_t arg;
  };
  static constexpr size_t kMaxButtons = 24;

  // Disposición y dibujo
  void layout();
  void add(int16_t x, int16_t y, int16_t w, int16_t h, Action a, int16_t arg = 0);
  void redraw(const ft8x7::RigState& s);
  void drawButton(size_t i, bool pressed);
  void buttonLabel(const Button& b, char* buf, size_t len) const;
  Style buttonStyle(const Button& b) const;
  bool buttonBig(const Button& b) const;
  void drawHeader(const char* title);
  void drawLabel(int16_t x, int16_t y, const char* text);
  void drawField(int16_t x, int16_t y, int16_t w, int16_t h, const char* text, uint16_t color, uint8_t font);
  void drawToast();
  bool underToast(const Button& b) const;
  void drawCountdown(uint32_t nowMs);

  // Pantalla principal: la dibuja la piel a partir de esta vista
  MainView buildView(const ft8x7::RigState& s) const;
  void applySkin();
  const Theme& th() const { return skin_->theme(); }
  void drawKeypadField();
  void drawClarField();
  void drawRepeaterFields();
  void drawDiag(const ft8x7::RigState& s);
  void drawCalibrate();
  void drawBtStatus();
  const char* linkLabel(bool live) const;

  // Acciones
  void perform(const Button& b, const ft8x7::RigState& s, bool repeat);
  bool send(const ft8x7::Command& cmd, const ft8x7::RigState& s);
  void toast(const char* text, uint32_t durationMs = 1300);
  int hit(int16_t x, int16_t y) const;
  void calibrationTouch(rigui::RawPoint raw);
  void refreshButtons();

  TFT_eSPI& tft_;
  Skin* skin_ = nullptr;
  rigui::Settings& settings_;
  UiHost& host_;

  Screen screen_ = Screen::Main;
  Button buttons_[kMaxButtons];
  size_t buttonCount_ = 0;
  int pressed_ = -1;
  bool dirty_ = true;
  uint32_t nowMs_ = 0;
  uint32_t lastTouchMs_ = 0;
  uint32_t lastDiagMs_ = 0;
  int lastCountdownW_ = -1;

  char toastText_[48] = "";
  uint32_t toastUntilMs_ = 0;
  bool toastShown_ = false;

  // Estado que la radio no informa por CAT: lo último que envió el display.
  bool split_ = false;
  bool lock_ = false;
  bool clarOn_ = false;
  int16_t clarHz_ = 0;
  ft8x7::RepeaterShift rptShift_ = ft8x7::RepeaterShift::Simplex;
  ft8x7::ToneMode toneMode_ = ft8x7::ToneMode::Off;

  // Sintonía mantenida: se parte de la última frecuencia pedida, no de la leída (va por detrás).
  uint32_t tuneHz_ = 0;
  uint32_t lastTuneMs_ = 0;

  rigui::KeypadEntry keypad_;
  bool keypadError_ = false;
  bool keypadPin_ = false;  // el teclado edita el PIN Bluetooth en vez de una frecuencia

  // Bluetooth: última copia del estado y de la búsqueda
  static constexpr size_t kBtRows = 3;      // dispositivos por página
  static constexpr size_t kBtMaxResults = 12;
  rigui::BtStatus bt_{};
  rigui::BtFound btResults_[kBtMaxResults];
  size_t btResultCount_ = 0;
  size_t btPage_ = 0;
  // Bluetooth conectado pero la radio no contesta (velocidad o menú CAT mal): avisar.
  uint32_t btNoCatSinceMs_ = 0;
  bool btNoCat_ = false;
  uint32_t lastBtPollMs_ = 0;

  uint8_t calibStep_ = 0;
  rigui::RawPoint calibRaw_[5] = {};

  // Último estado de la radio y última vista dibujada de la pantalla principal
  ft8x7::RigState last_{};
  MainView lastView_{};
  bool haveLastView_ = false;
  int16_t touchX_ = 0;  // x del último toque (para sintonizar en la escala del tema Dial)
};

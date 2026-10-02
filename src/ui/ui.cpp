#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rig_format.h"

using ft8x7::Mode;
using ft8x7::RigState;
using rigui::kBands;

namespace {

constexpr int16_t W = 320;
constexpr int16_t H = 240;
constexpr uint32_t kReturnMs = 10000;
constexpr int16_t kToastY = 192;
constexpr int16_t kToastH = 44;

// Pantalla principal
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

// Subpantallas
constexpr int16_t SUB_HEADER_H = 40;
constexpr int16_t CAL_MARGIN = 20;
constexpr rigui::RawPoint kCalTargets[5] = {
    {CAL_MARGIN, CAL_MARGIN}, {W - CAL_MARGIN, CAL_MARGIN}, {W - CAL_MARGIN, H - CAL_MARGIN},
    {CAL_MARGIN, H - CAL_MARGIN}, {W / 2, H / 2}};

constexpr Mode kModeOrder[9] = {Mode::LSB, Mode::USB, Mode::CW,  Mode::CWR, Mode::AM,
                                Mode::FM,  Mode::DIG, Mode::PKT, Mode::WFM};
constexpr const char* kRptShiftLabels[3] = {"-", "SIMPLEX", "+"};
constexpr ft8x7::RepeaterShift kRptShifts[3] = {ft8x7::RepeaterShift::Minus, ft8x7::RepeaterShift::Simplex,
                                                ft8x7::RepeaterShift::Plus};
constexpr const char* kToneLabels[4] = {"OFF", "ENC", "TSQ", "DCS"};
constexpr ft8x7::ToneMode kToneModes[4] = {ft8x7::ToneMode::Off, ft8x7::ToneMode::Encoder,
                                           ft8x7::ToneMode::Ctcss, ft8x7::ToneMode::Dcs};
constexpr int16_t kClarDeltas[5] = {-100, -10, 0, 10, 100};
constexpr uint32_t kRptOffsetStep = 100000;
constexpr uint32_t kRptOffsetMax = 99900000;

int16_t segmentX(int i) { return SEG_X0 + i * (SEG_W + SEG_GAP); }

const char* screenTitle(Ui::Screen s) {
  switch (s) {
    case Ui::Screen::Mode: return "Modo";
    case Ui::Screen::Band: return "Banda";
    case Ui::Screen::Keypad: return "Frecuencia MHz";
    case Ui::Screen::Menu: return "Menu";
    case Ui::Screen::Clar: return "Clarificador";
    case Ui::Screen::Repeater: return "Repetidor/tono";
    case Ui::Screen::Settings: return "Ajustes";
    case Ui::Screen::Display: return "Pantalla";
    case Ui::Screen::Bluetooth: return "Bluetooth";
    case Ui::Screen::Diag: return "Diagnostico";
    default: return "";
  }
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Ciclo de vida

void Ui::begin() { show(Screen::Main); }

void Ui::show(Screen screen) {
  screen_ = screen;
  pressed_ = -1;
  dirty_ = true;
  lastTouchMs_ = nowMs_;
  lastCountdownW_ = -1;
  layout();
}

void Ui::update(const RigState& s, uint32_t nowMs) {
  nowMs_ = nowMs;
  last_ = s;

  // Memoria de bandas: también recoge lo que se sintoniza desde la propia radio.
  if (s.linked && s.haveFreq && s.freq.mode != Mode::Unknown) {
    int b = rigui::bandIndexFor(s.freq.hz);
    if (b != rigui::kNoBand) {
      uint32_t hz;
      Mode mode;
      settings_.bands.recall(b, hz, mode);
      if (hz != s.freq.hz || mode != s.freq.mode) {
        settings_.bands.remember(s.freq.hz, s.freq.mode);
        host_.settingsChanged(false);
      }
    }
  }

  // Estado Bluetooth: en su pantalla y, con transporte BT, para el recuadro de enlace.
  if ((screen_ == Screen::Bluetooth || settings_.transport == rigui::Transport::Bluetooth) &&
      nowMs - lastBtPollMs_ >= 250) {
    lastBtPollMs_ = nowMs;
    rigui::BtStatus st = host_.btStatus();
    if (screen_ == Screen::Bluetooth) {
      if (st.scanSerial != bt_.scanSerial) {
        btResultCount_ = host_.btResults(btResults_, kBtMaxResults);
        btPage_ = 0;
        layout();
        dirty_ = true;
      } else if (st.state != bt_.state || st.confirmCode != bt_.confirmCode || st.haveDevice != bt_.haveDevice) {
        bt_ = st;
        drawBtStatus();
        refreshButtons();
      }
      // No volver a la principal a mitad de una búsqueda o un emparejamiento.
      if (st.state == rigui::BtState::Scanning || st.state == rigui::BtState::Connecting || st.confirmCode) {
        lastTouchMs_ = nowMs;
      }
    }
    bt_ = st;
  }

  if (screen_ != Screen::Main && screen_ != Screen::Calibrate && nowMs - lastTouchMs_ > kReturnMs) {
    show(Screen::Main);
  }

  // Bluetooth bien pero la radio calla más de 4 s: casi siempre CAT RATE distinto del adaptador
  // o, en el FT-857, el menú 020 fuera de CAT.
  bool noCat = settings_.transport == rigui::Transport::Bluetooth && bt_.state == rigui::BtState::Connected &&
               !s.linked;
  if (!noCat) {
    btNoCatSinceMs_ = 0;
  } else if (btNoCatSinceMs_ == 0) {
    btNoCatSinceMs_ = nowMs ? nowMs : 1;
  }
  bool showNoCat = btNoCatSinceMs_ && nowMs - btNoCatSinceMs_ > 4000;
  if (showNoCat != btNoCat_) {
    btNoCat_ = showNoCat;
    if (screen_ == Screen::Bluetooth) drawBtStatus();
    if (showNoCat && (screen_ == Screen::Bluetooth || screen_ == Screen::Main)) {
      toast("Radio muda: CAT RATE 9600, menu 020 CAT", 8000);
    }
  }

  // Split leído de la radio: manda sobre lo que el display creía haber enviado.
  if (s.linked && s.haveSplit && split_ != s.split) {
    split_ = s.split;
    if (screen_ == Screen::Menu) refreshButtons();
  }

  if (toastText_[0] && static_cast<int32_t>(nowMs - toastUntilMs_) >= 0) {
    toastText_[0] = '\0';
    if (toastShown_) {
      toastShown_ = false;
      dirty_ = true;  // repintar lo que tapaba el aviso
    }
  }

  if (dirty_) {
    redraw(s);
  } else if (screen_ == Screen::Main) {
    drawMainDynamic(s, false);
  } else if (screen_ == Screen::Diag && nowMs - lastDiagMs_ >= 1000) {
    drawDiag(s);
  }

  if (toastText_[0] && !toastShown_) {
    drawToast();
    toastShown_ = true;
  }
  drawCountdown(nowMs);
}

void Ui::onTouch(const rigui::TouchEvent& ev, rigui::RawPoint raw, const RigState& s, uint32_t nowMs) {
  nowMs_ = nowMs;
  lastTouchMs_ = nowMs;
  last_ = s;

  if (screen_ == Screen::Calibrate) {
    if (ev.type == rigui::TouchEventType::Down) {
      calibrationTouch(raw);
    }
    return;
  }

  switch (ev.type) {
    case rigui::TouchEventType::Down: {
      // Un toque cierra el aviso en pantalla; repintar lo que tapaba.
      if (toastText_[0]) {
        toastText_[0] = '\0';
        if (toastShown_) {
          toastShown_ = false;
          dirty_ = true;
        }
      }
      int i = hit(ev.x, ev.y);
      if (i < 0) {
        return;
      }
      Style st = buttonStyle(buttons_[i]);
      if (st == Style::Disabled) {
        return;
      }
      Screen before = screen_;
      if (st != Style::Custom) {
        drawButton(i, true);
      }
      pressed_ = i;
      perform(buttons_[i], s, false);
      if (screen_ != before) {
        pressed_ = -1;
      }
      break;
    }
    case rigui::TouchEventType::Repeat:
      if (pressed_ >= 0) {
        Action a = buttons_[pressed_].action;
        if (a == Action::TuneDown || a == Action::TuneUp || a == Action::RptOffset || a == Action::ToneValue) {
          perform(buttons_[pressed_], s, true);
        }
      }
      break;
    case rigui::TouchEventType::Up:
      // Bajo un aviso visible no se repinta: se hará entero cuando el aviso caduque.
      if (pressed_ >= 0 && !dirty_ && buttonStyle(buttons_[pressed_]) != Style::Custom &&
          !(toastText_[0] && underToast(buttons_[pressed_]))) {
        drawButton(pressed_, false);
      }
      pressed_ = -1;
      break;
    case rigui::TouchEventType::None:
      break;
  }
}

// ---------------------------------------------------------------------------------------------
// Disposición

void Ui::add(int16_t x, int16_t y, int16_t w, int16_t h, Action a, int16_t arg) {
  if (buttonCount_ < kMaxButtons) {
    buttons_[buttonCount_++] = Button{x, y, w, h, a, arg};
  }
}

void Ui::layout() {
  buttonCount_ = 0;
  if (screen_ == Screen::Main) {
    add(0, 22, W, 56, Action::OpenKeypad);
    add(4, ROW_Y, 92, ROW_H, Action::OpenMode);
    add(100, ROW_Y, 76, ROW_H, Action::OpenBand);
    add(4, 192, 60, 44, Action::TuneDown);
    add(68, 192, 60, 44, Action::Step);
    add(132, 192, 60, 44, Action::TuneUp);
    add(196, 192, 58, 44, Action::ToggleVfo);
    add(258, 192, 58, 44, Action::OpenMenu);
    return;
  }
  if (screen_ == Screen::Calibrate) {
    return;
  }
  add(4, 2, 84, 36, Action::Back);

  switch (screen_) {
    case Screen::Mode:
      for (int i = 0; i < 9; ++i) {
        add(6 + (i % 3) * 104, 46 + (i / 3) * 64, 100, 58, Action::SetMode, i);
      }
      break;
    case Screen::Band:
      for (int i = 0; i < static_cast<int>(rigui::kBandCount); ++i) {
        add(4 + (i % 4) * 79, 44 + (i / 4) * 49, 75, 45, Action::SetBand, i);
      }
      break;
    case Screen::Keypad: {
      static const char kKeys[4][4] = {{'1', '2', '3', 'D'}, {'4', '5', '6', 'C'}, {'7', '8', '9', '.'}, {'0', 'K'}};
      for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
          char k = kKeys[r][c];
          int16_t x = 4 + c * 79, y = 90 + r * 37;
          if (k == 'D') add(x, y, 75, 34, Action::KeyDel);
          else if (k == 'C') add(x, y, 75, 34, Action::KeyClear);
          else if (k == 'K') add(x, y, 154, 34, Action::KeyOk);
          else if (k) add(x, y, 75, 34, Action::Key, k);
        }
      }
      break;
    }
    case Screen::Menu: {
      const Action items[6] = {Action::Split, Action::OpenClar, Action::OpenRepeater,
                               Action::Lock,  Action::OpenSettings, Action::OpenDiag};
      for (int i = 0; i < 6; ++i) {
        add(6 + (i % 2) * 156, 46 + (i / 2) * 64, 152, 58, items[i]);
      }
      break;
    }
    case Screen::Clar:
      add(6, 48, 100, 44, Action::ClarToggle);
      for (int i = 0; i < 5; ++i) {
        add(6 + i * 62, 104, 58, 50, Action::ClarDelta, kClarDeltas[i]);
      }
      break;
    case Screen::Repeater:
      for (int i = 0; i < 3; ++i) add(68 + i * 82, 44, 78, 40, Action::RptShift, i);
      add(68, 88, 50, 40, Action::RptOffset, -1);
      add(264, 88, 50, 40, Action::RptOffset, 1);
      for (int i = 0; i < 4; ++i) add(68 + i * 62, 132, 58, 40, Action::ToneMode, i);
      add(68, 176, 50, 44, Action::ToneValue, -1);
      add(264, 176, 50, 44, Action::ToneValue, 1);
      break;
    case Screen::Settings:
      for (int i = 0; i < static_cast<int>(rigui::kModelCount); ++i) add(68 + i * 62, 44, 58, 44, Action::SetModel, i);
      add(68, 94, 120, 44, Action::SetTransport, 0);
      add(192, 94, 122, 44, Action::SetTransport, 1);
      for (int i = 0; i < static_cast<int>(rigui::kBaudCount); ++i) add(68 + i * 82, 144, 78, 44, Action::SetBaud, i);
      add(68, 194, 120, 42, Action::OpenBluetooth);
      add(192, 194, 122, 42, Action::OpenDisplay);
      break;
    case Screen::Display:
      add(68, 46, 60, 44, Action::Brightness, -10);
      add(246, 46, 68, 44, Action::Brightness, 10);
      add(68, 100, 120, 44, Action::SetInvert, 0);
      add(192, 100, 122, 44, Action::SetInvert, 1);
      add(68, 160, 246, 44, Action::Calibrate);
      break;
    case Screen::Bluetooth: {
      bool paged = btResultCount_ > kBtRows;
      size_t first = btPage_ * kBtRows;
      for (size_t i = first; i < btResultCount_ && i < first + kBtRows; ++i) {
        add(6, 66 + (i - first) * 44, paged ? 250 : 308, 40, Action::BtSelect, i);
      }
      if (paged) {
        add(262, 66, 52, 62, Action::BtPage, -1);
        add(262, 132, 52, 62, Action::BtPage, 1);
      }
      add(6, 196, 98, 40, Action::BtScan);
      add(108, 196, 100, 40, Action::BtPin);
      add(212, 196, 102, 40, Action::BtForget);
      break;
    }
    default:
      break;
  }
}

int Ui::hit(int16_t x, int16_t y) const {
  for (size_t i = 0; i < buttonCount_; ++i) {
    const Button& b = buttons_[i];
    if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void Ui::buttonLabel(const Button& b, char* buf, size_t len) const {
  const char* t = "";
  switch (b.action) {
    case Action::TuneDown: t = "<"; break;
    case Action::TuneUp: t = ">"; break;
    case Action::RptOffset:
    case Action::ToneValue: t = b.arg < 0 ? "<" : ">"; break;
    case Action::Step: snprintf(buf, len, "PASO\n%s", rigui::stepLabel(settings_.stepIndex)); return;
    case Action::ToggleVfo: t = "A/B"; break;
    case Action::OpenMenu: t = "MENU"; break;
    case Action::Back: t = "Volver"; break;
    case Action::SetMode: t = ft8x7::modeName(kModeOrder[b.arg]); break;
    case Action::SetBand: t = kBands[b.arg].name; break;
    case Action::Key: snprintf(buf, len, "%c", static_cast<char>(b.arg)); return;
    case Action::KeyDel: t = "DEL"; break;
    case Action::KeyClear: t = "C"; break;
    case Action::KeyOk: t = "OK"; break;
    case Action::Split: t = split_ ? "SPLIT ON" : "SPLIT OFF"; break;
    case Action::OpenClar: t = "Clarificador"; break;
    case Action::OpenRepeater: t = "Repetidor/tono"; break;
    case Action::Lock: t = lock_ ? "LOCK ON" : "LOCK OFF"; break;
    case Action::OpenSettings: t = "Ajustes"; break;
    case Action::OpenDiag: t = "Diagnostico"; break;
    case Action::ClarToggle: t = clarOn_ ? "ON" : "OFF"; break;
    case Action::ClarDelta: snprintf(buf, len, b.arg > 0 ? "+%d" : "%d", b.arg); return;
    case Action::RptShift: t = kRptShiftLabels[b.arg]; break;
    case Action::ToneMode: t = kToneLabels[b.arg]; break;
    case Action::SetModel: t = rigui::modelName(static_cast<rigui::RigModel>(b.arg)) + 3; break;  // "817"
    case Action::SetBaud: snprintf(buf, len, "%lu", static_cast<unsigned long>(rigui::kBaudRates[b.arg])); return;
    case Action::Brightness: t = b.arg < 0 ? "-" : "+"; break;
    case Action::Calibrate: t = "Calibrar tactil"; break;
    case Action::SetTransport: t = b.arg ? "Bluetooth" : "Cable"; break;
    case Action::SetInvert: t = b.arg ? "Invertidos" : "Normal"; break;
    case Action::OpenBluetooth: t = "Emparejar"; break;
    case Action::OpenDisplay: t = "Pantalla"; break;
    case Action::BtScan: t = "Buscar"; break;
    case Action::BtPin: snprintf(buf, len, "PIN %s", settings_.btPin); return;
    case Action::BtForget: t = "Olvidar"; break;
    case Action::BtSelect: {
      const rigui::BtDevice& d = btResults_[b.arg].device;
      if (d.name[0]) {
        snprintf(buf, len, "%s", d.name);
      } else {
        char addr[18];
        rigui::formatBtAddr(d.addr, addr, sizeof(addr));
        snprintf(buf, len, "Sin nombre %s", addr);
      }
      return;
    }
    case Action::BtPage: t = b.arg < 0 ? "^" : "v"; break;
    default: break;
  }
  snprintf(buf, len, "%s", t);
}

Ui::Style Ui::buttonStyle(const Button& b) const {
  switch (b.action) {
    case Action::OpenKeypad:
    case Action::OpenMode:
    case Action::OpenBand:
      return Style::Custom;
    case Action::SetMode:
      return last_.haveFreq && last_.freq.mode == kModeOrder[b.arg] ? Style::On : Style::Normal;
    case Action::SetBand:
      if (!rigui::bandAvailable(b.arg, settings_.model)) return Style::Disabled;
      return last_.haveFreq && rigui::bandIndexFor(last_.freq.hz) == b.arg ? Style::On : Style::Normal;
    case Action::KeyOk: return Style::On;
    case Action::Split: return split_ ? Style::On : Style::Normal;
    case Action::Lock: return lock_ ? Style::On : Style::Normal;
    case Action::OpenRepeater: return last_.freq.mode == Mode::FM ? Style::Normal : Style::Disabled;
    case Action::ClarToggle: return clarOn_ ? Style::On : Style::Normal;
    case Action::RptShift: return kRptShifts[b.arg] == rptShift_ ? Style::On : Style::Normal;
    case Action::ToneMode: return kToneModes[b.arg] == toneMode_ ? Style::On : Style::Normal;
    case Action::SetModel: return static_cast<int>(settings_.model) == b.arg ? Style::On : Style::Normal;
    case Action::SetBaud:
      // Por Bluetooth la velocidad hacia la radio la fija el adaptador.
      if (settings_.transport == rigui::Transport::Bluetooth) return Style::Disabled;
      return rigui::kBaudRates[b.arg] == settings_.baud ? Style::On : Style::Normal;
    case Action::SetTransport:
      return static_cast<int>(settings_.transport) == b.arg ? Style::On : Style::Normal;
    case Action::SetInvert: return settings_.invertColors == (b.arg != 0) ? Style::On : Style::Normal;
    case Action::BtScan:
      return bt_.state == rigui::BtState::Scanning || bt_.state == rigui::BtState::Connecting ? Style::Disabled
                                                                                               : Style::Normal;
    case Action::BtForget: return settings_.btHaveDevice ? Style::Normal : Style::Disabled;
    case Action::BtSelect:
      return settings_.btHaveDevice && memcmp(settings_.btDevice.addr, btResults_[b.arg].device.addr, 6) == 0
                 ? Style::On
                 : Style::Normal;
    case Action::BtPage: {
      size_t pages = (btResultCount_ + kBtRows - 1) / kBtRows;
      bool can = b.arg < 0 ? btPage_ > 0 : btPage_ + 1 < pages;
      return can ? Style::Normal : Style::Disabled;
    }
    case Action::Key: return keypadPin_ && b.arg == '.' ? Style::Disabled : Style::Normal;
    default: return Style::Normal;
  }
}

// ---------------------------------------------------------------------------------------------
// Dibujo común

void Ui::drawArrow(int16_t cx, int16_t cy, int dir, uint16_t color) {
  tft_.fillTriangle(cx + dir * 8, cy, cx - dir * 8, cy - 10, cx - dir * 8, cy + 10, color);
}

void Ui::drawButton(size_t i, bool pressed) {
  const Button& b = buttons_[i];
  Style st = buttonStyle(b);
  if (st == Style::Custom) {
    return;
  }
  uint16_t bg = pressed ? th_.buttonPressed : (st == Style::On ? th_.buttonOn : th_.button);
  uint16_t fg = st == Style::Disabled ? th_.buttonDisabledText : th_.text;
  tft_.fillRoundRect(b.x, b.y, b.w, b.h, 5, bg);
  if (st == Style::On) {
    tft_.drawRoundRect(b.x, b.y, b.w, b.h, 5, th_.buttonOnBorder);
    tft_.drawRoundRect(b.x + 1, b.y + 1, b.w - 2, b.h - 2, 4, th_.buttonOnBorder);
  }

  char label[40];
  buttonLabel(b, label, sizeof(label));
  int16_t cx = b.x + b.w / 2, cy = b.y + b.h / 2;
  tft_.setTextPadding(0);
  tft_.setTextColor(fg, bg);
  tft_.setTextDatum(MC_DATUM);

  if (strcmp(label, "<") == 0 || strcmp(label, ">") == 0) {
    drawArrow(cx, cy, label[0] == '<' ? -1 : 1, fg);
    return;
  }
  if (strcmp(label, "^") == 0 || strcmp(label, "v") == 0) {  // flechas verticales (paginar)
    int dir = label[0] == '^' ? -1 : 1;
    tft_.fillTriangle(cx, cy + dir * 8, cx - 10, cy - dir * 8, cx + 10, cy - dir * 8, fg);
    return;
  }
  if (b.action == Action::Back) {
    drawArrow(b.x + 16, cy, -1, fg);
    tft_.drawString(label, b.x + 50, cy, 2);
    return;
  }
  char* nl = strchr(label, '\n');
  if (nl) {  // dos líneas: rótulo pequeño + valor
    *nl = '\0';
    tft_.setTextColor(th_.textDim, bg);
    tft_.drawString(label, cx, b.y + 11, 1);
    tft_.setTextColor(fg, bg);
    tft_.drawString(nl + 1, cx, b.y + 28, 2);
    return;
  }
  tft_.drawString(label, cx, cy + 1, buttonFont(b));
}

// Fuente 4 si la etiqueta cabe; en filas de botones iguales (pasos, baudios, tonos...) toda la
// fila usa la misma fuente para que no queden tamaños mezclados.
uint8_t Ui::buttonFont(const Button& b) const {
  char label[40];
  auto fits = [&](const Button& o) {
    buttonLabel(o, label, sizeof(label));
    return o.h >= 30 && tft_.textWidth(label, 4) <= o.w - 6;
  };
  if (b.action == Action::BtSelect) {
    return 2;  // nombres y direcciones largas: tamaño fijo para que no cambie entre páginas
  }
  if (!fits(b)) {
    return 2;
  }
  if (b.action == Action::SetBand) {
    return 4;
  }
  for (size_t i = 0; i < buttonCount_; ++i) {
    if (buttons_[i].action == b.action && !fits(buttons_[i])) {
      return 2;
    }
  }
  return 4;
}

void Ui::refreshButtons() {
  for (size_t i = 0; i < buttonCount_; ++i) {
    drawButton(i, static_cast<int>(i) == pressed_);
  }
}

void Ui::drawHeader(const char* title) {
  tft_.fillRect(0, 0, W, SUB_HEADER_H, th_.header);
  tft_.setTextColor(th_.text, th_.header);
  tft_.setTextDatum(ML_DATUM);
  tft_.setTextPadding(0);
  uint8_t font = tft_.textWidth(title, 4) <= W - 104 ? 4 : 2;
  tft_.drawString(title, 98, SUB_HEADER_H / 2, font);
}

void Ui::drawLabel(int16_t x, int16_t y, const char* text) {
  tft_.setTextColor(th_.textDim, th_.bg);
  tft_.setTextDatum(ML_DATUM);
  tft_.setTextPadding(58);
  tft_.drawString(text, x, y, 2);
  tft_.setTextPadding(0);
}

void Ui::drawField(int16_t x, int16_t y, int16_t w, int16_t h, const char* text, uint16_t color, uint8_t font) {
  tft_.fillRoundRect(x, y, w, h, 4, th_.field);
  tft_.setTextColor(color, th_.field);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextPadding(0);
  tft_.drawString(text, x + w / 2, y + h / 2 + 1, font);
}

void Ui::toast(const char* text, uint32_t durationMs) {
  snprintf(toastText_, sizeof(toastText_), "%s", text);
  toastUntilMs_ = nowMs_ + durationMs;
  if (toastShown_) {
    dirty_ = true;  // sustituir el aviso anterior
  }
  toastShown_ = false;
}

bool Ui::underToast(const Button& b) const { return b.y + b.h > kToastY && b.y < kToastY + kToastH; }

// Sobre la fila inferior de botones: es la única zona que no se repinta sola (los recuadros de modo,
// banda y medidores cambian con cada lectura y taparían el aviso). Tocar la pantalla lo cierra.
void Ui::drawToast() {
  const int16_t x = 4, y = kToastY, w = 312, h = kToastH;
  tft_.fillRoundRect(x, y, w, h, 6, th_.bg);
  tft_.drawRoundRect(x, y, w, h, 6, th_.accent);
  tft_.setTextColor(th_.accent, th_.bg);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextPadding(0);
  tft_.drawString(toastText_, x + w / 2, y + h / 2, 2);
}

void Ui::drawCountdown(uint32_t nowMs) {
  if (screen_ == Screen::Main || screen_ == Screen::Calibrate) {
    return;
  }
  uint32_t elapsed = nowMs - lastTouchMs_;
  int w = elapsed >= kReturnMs ? 0 : static_cast<int>(W * (kReturnMs - elapsed) / kReturnMs);
  if (lastCountdownW_ >= 0 && abs(w - lastCountdownW_) < 4) {
    return;
  }
  tft_.fillRect(0, H - 2, w, 2, th_.accent);
  tft_.fillRect(w, H - 2, W - w, 2, th_.bg);
  lastCountdownW_ = w;
}

void Ui::redraw(const RigState& s) {
  dirty_ = false;
  toastShown_ = false;  // el repintado lo borra: si sigue vigente, update() lo vuelve a dibujar
  lastCountdownW_ = -1;
  if (screen_ == Screen::Main) {
    drawMainStatic();
    drawMainDynamic(s, true);
    return;
  }
  if (screen_ == Screen::Calibrate) {
    drawCalibrate();
    return;
  }
  tft_.fillScreen(th_.bg);
  drawHeader(screen_ == Screen::Keypad && keypadPin_ ? "PIN Bluetooth" : screenTitle(screen_));
  refreshButtons();
  switch (screen_) {
    case Screen::Keypad: drawKeypadField(); break;
    case Screen::Clar:
      drawClarField();
      tft_.setTextColor(th_.textDim, th_.bg);
      tft_.setTextDatum(MC_DATUM);
      tft_.drawString("Hz por toque, maximo +-9.99 kHz", W / 2, 172, 2);
      break;
    case Screen::Repeater:
      drawLabel(6, 64, "Despl.");
      drawLabel(6, 108, "Offset");
      drawLabel(6, 152, "Tono");
      drawRepeaterFields();
      break;
    case Screen::Settings:
      drawLabel(6, 66, "Radio");
      drawLabel(6, 116, "Enlace");
      drawLabel(6, 166, "Baudios");
      break;
    case Screen::Display: {
      drawLabel(6, 68, "Brillo");
      drawLabel(6, 122, "Colores");
      char buf[8];
      snprintf(buf, sizeof(buf), "%u %%", settings_.brightness);
      drawField(132, 46, 110, 44, buf, th_.text, 4);
      break;
    }
    case Screen::Bluetooth:
      drawBtStatus();
      if (btResultCount_ == 0) {
        tft_.setTextColor(th_.textDim, th_.bg);
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextPadding(0);
        char msg[48];
        if (bt_.state == rigui::BtState::Scanning) {
          snprintf(msg, sizeof(msg), "Buscando dispositivos (%lu s)...", static_cast<unsigned long>(rigui::kBtScanSeconds));
        } else {
          snprintf(msg, sizeof(msg), "Pulsa Buscar con el adaptador encendido");
        }
        tft_.drawString(msg, W / 2, 120, 2);
      }
      break;
    case Screen::Diag: drawDiag(s); break;
    default: break;
  }
}

// ---------------------------------------------------------------------------------------------
// Pantalla principal

void Ui::drawMainStatic() {
  tft_.fillScreen(th_.bg);
  tft_.fillRect(0, 0, W, HEADER_H, th_.header);
  tft_.setTextColor(th_.textDim, th_.bg);
  tft_.setTextDatum(TR_DATUM);
  tft_.setTextPadding(0);
  tft_.drawString("MHz", W - 8, 78, 1);
  refreshButtons();
  cache_ = {};
  cache_.freqHz = 0;
}

void Ui::drawMeterScale(bool tx) {
  tft_.fillRect(0, SCALE_Y, W, 16, th_.bg);
  tft_.setTextColor(th_.textDim, th_.bg);
  tft_.setTextDatum(TC_DATUM);
  tft_.setTextPadding(0);
  if (tx) {
    for (int v = 0; v <= SEGMENTS; v += 5) {
      int16_t x = v == 0 ? SEG_X0 : segmentX(v - 1) + SEG_W;
      tft_.drawNumber(v, x, SCALE_Y, 2);
    }
    return;
  }
  static const struct {
    int level;
    const char* label;
  } kMarks[] = {{1, "1"}, {3, "3"}, {5, "5"}, {7, "7"}, {9, "9"}, {11, "+20"}, {13, "+40"}, {15, "+60"}};
  for (const auto& m : kMarks) {
    tft_.drawString(m.label, segmentX(m.level - 1) + SEG_W / 2, SCALE_Y, 2);
  }
}

void Ui::drawMainDynamic(const RigState& s, bool force) {
  const bool live = s.linked;
  const bool tx = live && s.tx.transmitting;
  const bool linkChanged = force || cache_.linked != live;
  char buf[24];

  // Cabecera: modelo, LOCK y estado del enlace
  int model = static_cast<int>(settings_.model);
  int vfo = live && s.haveVfo ? (s.vfoB ? 2 : 1) : 0;
  if (force || cache_.model != model || cache_.lock != lock_ || cache_.vfo != vfo) {
    tft_.fillRect(0, 0, 230, HEADER_H, th_.header);
    tft_.setTextColor(th_.text, th_.header);
    tft_.setTextDatum(ML_DATUM);
    tft_.setTextPadding(0);
    tft_.drawString(rigui::modelName(settings_.model), 6, HEADER_H / 2, 2);
    if (vfo) {
      tft_.setTextColor(th_.text, th_.header);
      tft_.drawString(vfo == 2 ? "VFO B" : "VFO A", 96, HEADER_H / 2, 2);
    }
    if (lock_) {
      tft_.setTextColor(th_.accent, th_.header);
      tft_.drawString("LOCK", 170, HEADER_H / 2, 2);
    }
    cache_.model = model;
    cache_.lock = lock_;
    cache_.vfo = vfo;
  }
  const char* link = linkLabel(live);
  if (linkChanged || link != cache_.link) {
    bool noCatLabel = !live && btNoCat_;
    uint16_t c = live ? th_.linkOk : noCatLabel ? th_.po : (link[0] == 'B' ? th_.boxStale : th_.linkLost);
    tft_.fillRoundRect(236, 1, 80, 18, 4, c);
    tft_.setTextColor(noCatLabel ? th_.bg : th_.text, c);
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextPadding(0);
    tft_.drawString(link, 276, 10, 2);
    cache_.linked = live;
    cache_.link = link;
  }

  // Frecuencia (atenuada sin enlace)
  int freqColor = !s.haveFreq ? -2 : (live ? th_.freq : th_.freqStale);
  if (force || freqColor != cache_.freqColor || (s.haveFreq && s.freq.hz != cache_.freqHz)) {
    tft_.setTextDatum(TR_DATUM);
    tft_.setTextPadding(W - 20);
    if (s.haveFreq) {
      ft8x7::formatFrequency(s.freq.hz, buf, sizeof(buf));
      tft_.setTextColor(freqColor, th_.bg);
    } else {
      snprintf(buf, sizeof(buf), "---.---.--");
      tft_.setTextColor(th_.freqStale, th_.bg);
    }
    tft_.drawString(buf, W - 10, FREQ_Y, 7);
    tft_.setTextPadding(0);
    cache_.freqColor = freqColor;
    cache_.freqHz = s.haveFreq ? s.freq.hz : 0;
  }

  // Modo y banda (zonas táctiles)
  uint16_t box = live ? th_.box : th_.boxStale;
  int rawMode = s.haveFreq ? s.freq.rawMode : -2;
  if (linkChanged || rawMode != cache_.rawMode) {
    snprintf(buf, sizeof(buf), "---");
    if (s.haveFreq) ft8x7::formatMode(s.freq, buf, sizeof(buf));
    tft_.fillRoundRect(4, ROW_Y, 92, ROW_H, 6, box);
    tft_.setTextColor(th_.text, box);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(buf, 50, ROW_Y + ROW_H / 2 + 1, 4);
    cache_.rawMode = rawMode;
  }
  int band = s.haveFreq ? rigui::bandIndexFor(s.freq.hz) : -2;
  if (linkChanged || band != cache_.band) {
    const char* name = band >= 0 ? kBands[band].name : (s.haveFreq ? "GEN" : "---");
    tft_.fillRoundRect(100, ROW_Y, 76, ROW_H, 6, box);
    tft_.setTextColor(th_.text, box);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(name, 138, ROW_Y + ROW_H / 2 + 1, tft_.textWidth(name, 4) <= 70 ? 4 : 2);
    cache_.band = band;
  }

  // Indicadores SPLIT y SQL/CLAR
  auto flag = [&](int16_t y, const char* label, bool on, uint16_t color) {
    tft_.fillRoundRect(182, y, 58, 20, 3, on ? color : th_.bg);
    tft_.drawRoundRect(182, y, 58, 20, 3, on ? color : th_.flagOff);
    tft_.setTextColor(on ? th_.bg : th_.flagOff, on ? color : th_.bg);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(label, 211, y + 10, 2);
  };
  // Con EEPROM, el split real; si no, lo último que envió el display (el bit de 0xF7 es dudoso).
  int split = live && (s.haveSplit ? s.split : split_);
  if (force || split != cache_.split) {
    flag(ROW_Y, "SPLIT", split, th_.flagSplit);
    cache_.split = split;
  }
  int sqlClar = clarOn_ ? 2 : (live && !tx && s.rx.squelched ? 1 : 0);
  if (force || sqlClar != cache_.sqlClar) {
    if (sqlClar == 2) flag(ROW_Y + 24, "CLAR", true, th_.flagClar);
    else flag(ROW_Y + 24, "SQL", sqlClar == 1, th_.flagSql);
    cache_.sqlClar = sqlClar;
  }

  // RX / TX
  int txState = !live ? 2 : (tx ? 1 : 0);
  if (force || txState != cache_.tx) {
    uint16_t c = txState == 2 ? th_.boxStale : (tx ? th_.tx : th_.rx);
    tft_.fillRoundRect(246, ROW_Y, 70, ROW_H, 6, c);
    tft_.setTextColor(th_.text, c);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(tx ? "TX" : "RX", 281, ROW_Y + ROW_H / 2 + 1, 4);
    cache_.tx = txState;
  }

  // Medidor
  int level = !live ? 0 : (tx ? s.tx.poMeter : s.rx.sMeter);
  int swr = tx && s.tx.highSwr;
  int meterInfo = tx && s.haveMeters ? (1 + s.meters.swr * 16 + s.meters.alc) : 0;
  if (force || cache_.meterTx != tx) {
    drawMeterScale(tx);
  }
  if (force || level != cache_.meterLevel || tx != cache_.meterTx || swr != cache_.swr ||
      meterInfo != cache_.meterInfo) {
    tft_.fillRect(0, METER_TITLE_Y, W, 16, th_.bg);
    tft_.setTextPadding(0);
    tft_.setTextColor(th_.textDim, th_.bg);
    tft_.setTextDatum(TL_DATUM);
    tft_.drawString(tx ? "PO" : "S", 8, METER_TITLE_Y, 2);
    if (swr) {
      tft_.setTextColor(th_.warn, th_.bg);
      tft_.setTextDatum(TC_DATUM);
      tft_.drawString("HI SWR", W / 2, METER_TITLE_Y, 2);
    } else if (meterInfo) {
      snprintf(buf, sizeof(buf), "SWR %u   ALC %u", s.meters.swr, s.meters.alc);
      tft_.setTextColor(th_.textDim, th_.bg);
      tft_.setTextDatum(TC_DATUM);
      tft_.drawString(buf, W / 2, METER_TITLE_Y, 2);
    }
    if (!live) snprintf(buf, sizeof(buf), "--");
    else if (tx) snprintf(buf, sizeof(buf), "%u", s.tx.poMeter);
    else ft8x7::formatSMeter(s.rx.sMeter, buf, sizeof(buf));
    tft_.setTextColor(th_.text, th_.bg);
    tft_.setTextDatum(TR_DATUM);
    tft_.drawString(buf, W - 8, METER_TITLE_Y, 2);

    for (int i = 0; i < SEGMENTS; ++i) {
      uint16_t c = th_.meterOff;
      if (i < level) c = tx ? (i < 10 ? th_.po : th_.poHigh) : (i < 9 ? th_.sMeter : th_.sMeterOver);
      tft_.fillRect(segmentX(i), METER_Y, SEG_W, METER_H, c);
    }
    cache_.meterLevel = level;
    cache_.meterTx = tx;
    cache_.swr = swr;
    cache_.meterInfo = meterInfo;
  }
}

// ---------------------------------------------------------------------------------------------
// Subpantallas

void Ui::drawKeypadField() {
  char buf[24];
  uint16_t color = th_.freq;
  if (keypadError_) {
    snprintf(buf, sizeof(buf), keypadPin_ ? "De 4 a 8 cifras" : "Fuera de rango");
    color = th_.warn;
  } else if (keypadPin_) {
    if (keypad_.empty()) {
      snprintf(buf, sizeof(buf), "%s", settings_.btPin);
      color = th_.textDim;
    } else {
      snprintf(buf, sizeof(buf), "%s_", keypad_.text());
    }
  } else if (keypad_.empty()) {
    if (last_.haveFreq) ft8x7::formatFrequency(last_.freq.hz, buf, sizeof(buf));
    else snprintf(buf, sizeof(buf), "MHz");
    color = th_.textDim;
  } else {
    snprintf(buf, sizeof(buf), "%s_", keypad_.text());
  }
  tft_.fillRoundRect(6, 44, 308, 40, 4, th_.field);
  tft_.setTextColor(color, th_.field);
  tft_.setTextDatum(MR_DATUM);
  tft_.setTextPadding(0);
  tft_.drawString(buf, 304, 65, 4);
}

void Ui::drawClarField() {
  char buf[16];
  int a = abs(clarHz_);
  snprintf(buf, sizeof(buf), "%c%d.%02d kHz", clarHz_ < 0 ? '-' : '+', a / 1000, (a % 1000) / 10);
  drawField(112, 48, 202, 44, buf, clarOn_ ? th_.flagClar : th_.textDim, 4);
}

void Ui::drawRepeaterFields() {
  char buf[20];
  snprintf(buf, sizeof(buf), "%lu.%03lu MHz", static_cast<unsigned long>(settings_.rptOffsetHz / 1000000),
           static_cast<unsigned long>(settings_.rptOffsetHz / 1000 % 1000));
  drawField(122, 88, 138, 40, buf, th_.text, 2);

  bool dcs = toneMode_ == ft8x7::ToneMode::Dcs;
  drawLabel(6, 198, dcs ? "DCS" : "CTCSS");
  if (dcs) {
    snprintf(buf, sizeof(buf), "%03u", ft8x7::kDcsCodes[settings_.dcsIndex]);
  } else {
    uint16_t t = ft8x7::kCtcssTones[settings_.ctcssIndex];
    snprintf(buf, sizeof(buf), "%u.%u Hz", t / 10, t % 10);
  }
  drawField(122, 176, 138, 44, buf, toneMode_ == ft8x7::ToneMode::Off ? th_.textDim : th_.text, 2);
}

void Ui::drawDiag(const RigState& s) {
  lastDiagMs_ = nowMs_;
  struct Row {
    const char* label;
    char value[40];
  } rows[8];
  snprintf(rows[0].value, sizeof(rows[0].value), "%s", s.linked ? "OK" : "SIN ENLACE");
  snprintf(rows[1].value, sizeof(rows[1].value), "%lu", static_cast<unsigned long>(s.okCount));
  snprintf(rows[2].value, sizeof(rows[2].value), "%lu", static_cast<unsigned long>(s.errorCount));
  snprintf(rows[3].value, sizeof(rows[3].value), "%lu", static_cast<unsigned long>(s.writeCount));
  if (settings_.transport == rigui::Transport::Bluetooth) {
    snprintf(rows[4].value, sizeof(rows[4].value), "BT %s",
             bt_.haveDevice && bt_.device.name[0] ? bt_.device.name : rigui::btStateName(bt_.state));
  } else {
    snprintf(rows[4].value, sizeof(rows[4].value), "Cable %lu 8N2", static_cast<unsigned long>(settings_.baud));
  }
  snprintf(rows[5].value, sizeof(rows[5].value), "%s", rigui::modelName(settings_.model));
  snprintf(rows[6].value, sizeof(rows[6].value), "EEPROM %s, medidores %s",
           s.eepromUnsupported ? "no" : (s.haveVfo ? "si" : "?"),
           s.metersUnsupported ? "no" : (s.haveMeters ? "si" : "?"));
  snprintf(rows[7].value, sizeof(rows[7].value), "%s", __DATE__);
  const char* labels[8] = {"Enlace", "Lecturas OK", "Errores", "Escrituras", "Conexion", "Radio", "Extras", "Firmware"};
  for (int i = 0; i < 8; ++i) {
    int16_t y = 48 + i * 24;
    tft_.setTextDatum(TL_DATUM);
    tft_.setTextColor(th_.textDim, th_.bg);
    tft_.setTextPadding(0);
    tft_.drawString(labels[i], 10, y, 2);
    tft_.setTextColor(i == 0 && !s.linked ? th_.warn : th_.text, th_.bg);
    tft_.setTextPadding(180);
    tft_.drawString(rows[i].value, 130, y, 2);
  }
  tft_.setTextPadding(0);
}

const char* Ui::linkLabel(bool live) const {
  bool viaBt = settings_.transport == rigui::Transport::Bluetooth;
  if (live) return viaBt ? "BT OK" : "CAT OK";
  if (viaBt && btNoCat_) return "BT sin CAT";
  if (viaBt && (bt_.state == rigui::BtState::Connecting || bt_.state == rigui::BtState::Scanning)) return "BT ...";
  return "NO LINK";
}

void Ui::drawBtStatus() {
  char buf[48];
  uint16_t color = th_.text;
  if (bt_.confirmCode) {
    snprintf(buf, sizeof(buf), "Confirma el codigo %06lu", static_cast<unsigned long>(bt_.confirmCode));
    color = th_.accent;
  } else if (settings_.transport != rigui::Transport::Bluetooth) {
    snprintf(buf, sizeof(buf), "Enlace por cable (cambialo en Ajustes)");
    color = th_.textDim;
  } else if (btNoCat_) {
    snprintf(buf, sizeof(buf), "Conectado, pero la radio no responde");
    color = th_.po;
  } else if ((bt_.state == rigui::BtState::Connected || bt_.state == rigui::BtState::Connecting ||
              bt_.state == rigui::BtState::Failed) && bt_.haveDevice) {
    const char* name = bt_.device.name[0] ? bt_.device.name : "adaptador";
    snprintf(buf, sizeof(buf), "%s: %s", rigui::btStateName(bt_.state), name);
    color = bt_.state == rigui::BtState::Connected ? th_.sMeter
            : bt_.state == rigui::BtState::Failed  ? th_.warn
                                                   : th_.text;
  } else {
    snprintf(buf, sizeof(buf), "%s", rigui::btStateName(bt_.state));
  }
  tft_.fillRect(0, 42, W, 22, th_.bg);
  tft_.setTextColor(color, th_.bg);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextPadding(0);
  tft_.drawString(buf, W / 2, 53, 2);
}

void Ui::drawCalibrate() {
  tft_.fillScreen(th_.bg);
  char buf[32];
  snprintf(buf, sizeof(buf), "Toca la cruz (%u/5)", calibStep_ + 1);
  tft_.setTextColor(th_.text, th_.bg);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextPadding(0);
  tft_.drawString(buf, W / 2, 70, 2);
  tft_.setTextColor(th_.textDim, th_.bg);
  tft_.drawString("con el dedo o el lapiz, firme", W / 2, 90, 2);
  const rigui::RawPoint& t = kCalTargets[calibStep_];
  tft_.drawFastHLine(t.x - 12, t.y, 25, th_.accent);
  tft_.drawFastVLine(t.x, t.y - 12, 25, th_.accent);
  tft_.drawCircle(t.x, t.y, 6, th_.accent);
}

void Ui::calibrationTouch(rigui::RawPoint raw) {
  if (calibStep_ > 0) {
    const rigui::RawPoint& prev = calibRaw_[calibStep_ - 1];
    if (abs(raw.x - prev.x) < 200 && abs(raw.y - prev.y) < 200) {
      return;  // rebote del toque anterior
    }
  }
  calibRaw_[calibStep_++] = raw;
  if (calibStep_ < 5) {
    drawCalibrate();
    return;
  }
  rigui::TouchCal cal;
  if (rigui::computeCalibration(calibRaw_, W, H, CAL_MARGIN, cal)) {
    settings_.touch = cal;
    host_.settingsChanged(true);
    show(Screen::Display);
    toast("Tactil calibrado");
  } else {
    calibStep_ = 0;
    show(Screen::Calibrate);
    toast("Repite la calibracion");
  }
}

// ---------------------------------------------------------------------------------------------
// Acciones

bool Ui::send(const ft8x7::Command& cmd, const RigState& s) {
  if (!s.linked) {
    toast("Sin enlace CAT");
    return false;
  }
  if (!host_.sendCat(cmd)) {
    toast("Cola CAT llena");
    return false;
  }
  return true;
}

void Ui::perform(const Button& b, const RigState& s, bool repeat) {
  ft8x7::Command cmd;
  char buf[32];
  switch (b.action) {
    case Action::OpenKeypad:
      keypad_.clear();
      keypadError_ = false;
      keypadPin_ = false;
      show(Screen::Keypad);
      break;
    case Action::OpenMode: show(Screen::Mode); break;
    case Action::OpenBand: show(Screen::Band); break;
    case Action::OpenMenu: show(Screen::Menu); break;
    case Action::OpenClar: show(Screen::Clar); break;
    case Action::OpenSettings: show(Screen::Settings); break;
    case Action::OpenDiag: show(Screen::Diag); break;
    case Action::OpenRepeater: show(Screen::Repeater); break;
    case Action::Back: show(screen_ == Screen::Keypad && keypadPin_ ? Screen::Bluetooth : Screen::Main); break;

    case Action::TuneDown:
    case Action::TuneUp: {
      if (!s.haveFreq) return;
      uint32_t base = (repeat || nowMs_ - lastTuneMs_ < 1000) && tuneHz_ ? tuneHz_ : s.freq.hz;
      uint32_t next;
      if (!rigui::tune(base, rigui::kSteps[settings_.stepIndex], b.action == Action::TuneUp ? 1 : -1, next)) {
        toast("Fuera de rango");
        return;
      }
      if (ft8x7::makeSetFrequency(next, cmd) && send(cmd, s)) {
        tuneHz_ = next;
        lastTuneMs_ = nowMs_;
      }
      break;
    }
    case Action::Step:
      settings_.stepIndex = (settings_.stepIndex + 1) % rigui::kStepCount;
      host_.settingsChanged(false);
      break;
    case Action::ToggleVfo:
      if (send(ft8x7::makeToggleVfo(), s)) toast("VFO A/B");
      break;

    case Action::SetMode: {
      Mode m = kModeOrder[b.arg];
      if (ft8x7::makeSetMode(m, cmd) && send(cmd, s)) {
        show(Screen::Main);
        snprintf(buf, sizeof(buf), "Modo %s", ft8x7::modeName(m));
        toast(buf);
      }
      break;
    }
    case Action::SetBand: {
      uint32_t hz;
      Mode mode;
      settings_.bands.recall(b.arg, hz, mode);
      if (!ft8x7::makeSetFrequency(hz, cmd) || !send(cmd, s)) return;
      if (!s.haveFreq || s.freq.mode != mode) {
        if (ft8x7::makeSetMode(mode, cmd)) send(cmd, s);
      }
      tuneHz_ = 0;
      show(Screen::Main);
      snprintf(buf, sizeof(buf), "Banda %s", kBands[b.arg].name);
      toast(buf);
      break;
    }

    case Action::Key:
      if (keypadPin_ && (b.arg == '.' || strlen(keypad_.text()) >= sizeof(settings_.btPin) - 1)) return;
      keypad_.press(static_cast<char>(b.arg));
      keypadError_ = false;
      drawKeypadField();
      break;
    case Action::KeyDel:
      keypad_.backspace();
      keypadError_ = false;
      drawKeypadField();
      break;
    case Action::KeyClear:
      keypad_.clear();
      keypadError_ = false;
      drawKeypadField();
      break;
    case Action::KeyOk: {
      if (keypadPin_) {
        size_t n = strlen(keypad_.text());
        if (n < 4) {
          keypadError_ = true;
          drawKeypadField();
          return;
        }
        snprintf(settings_.btPin, sizeof(settings_.btPin), "%s", keypad_.text());
        host_.settingsChanged(true);
        show(Screen::Bluetooth);
        toast("PIN guardado");
        return;
      }
      uint32_t hz;
      if (!keypad_.value(hz)) {
        keypadError_ = true;
        drawKeypadField();
        return;
      }
      if (ft8x7::makeSetFrequency(hz, cmd) && send(cmd, s)) {
        tuneHz_ = 0;
        show(Screen::Main);
        ft8x7::formatFrequency(hz, buf, sizeof(buf));
        toast(buf);
      }
      break;
    }

    case Action::Split:
      if (send(ft8x7::makeSplit(!split_), s)) split_ = !split_;
      break;
    case Action::Lock:
      if (send(ft8x7::makeLock(!lock_), s)) lock_ = !lock_;
      break;
    case Action::ClarToggle:
      if (send(ft8x7::makeClarifier(!clarOn_), s)) {
        clarOn_ = !clarOn_;
        drawClarField();
      }
      break;
    case Action::ClarDelta: {
      int v = b.arg == 0 ? 0 : clarHz_ + b.arg;
      v = v < -9990 ? -9990 : (v > 9990 ? 9990 : v);
      if (ft8x7::makeClarifierOffset(v, cmd) && send(cmd, s)) {
        clarHz_ = static_cast<int16_t>(v);
        drawClarField();
      }
      break;
    }

    case Action::RptShift:
      if (send(ft8x7::makeRepeaterShift(kRptShifts[b.arg]), s)) {
        rptShift_ = kRptShifts[b.arg];
        refreshButtons();
      }
      break;
    case Action::RptOffset: {
      uint32_t off = settings_.rptOffsetHz;
      if (b.arg < 0) off = off >= kRptOffsetStep ? off - kRptOffsetStep : 0;
      else off = off + kRptOffsetStep <= kRptOffsetMax ? off + kRptOffsetStep : kRptOffsetMax;
      if (ft8x7::makeRepeaterOffset(off, cmd) && send(cmd, s)) {
        settings_.rptOffsetHz = off;
        host_.settingsChanged(false);
        drawRepeaterFields();
      }
      break;
    }
    case Action::ToneMode:
      if (send(ft8x7::makeToneMode(kToneModes[b.arg]), s)) {
        toneMode_ = kToneModes[b.arg];
        refreshButtons();
        drawRepeaterFields();
      }
      break;
    case Action::ToneValue:
      if (toneMode_ == ft8x7::ToneMode::Dcs) {
        size_t i = rigui::wrapIndex(settings_.dcsIndex, b.arg, ft8x7::kDcsCodeCount);
        if (ft8x7::makeDcsCode(ft8x7::kDcsCodes[i], cmd) && send(cmd, s)) settings_.dcsIndex = i;
      } else {
        size_t i = rigui::wrapIndex(settings_.ctcssIndex, b.arg, ft8x7::kCtcssToneCount);
        if (ft8x7::makeCtcssTone(ft8x7::kCtcssTones[i], cmd) && send(cmd, s)) settings_.ctcssIndex = i;
      }
      host_.settingsChanged(false);
      drawRepeaterFields();
      break;

    case Action::SetModel:
      settings_.model = static_cast<rigui::RigModel>(b.arg);
      host_.settingsChanged(true);
      refreshButtons();
      break;
    case Action::SetBaud:
      settings_.baud = rigui::kBaudRates[b.arg];
      host_.settingsChanged(true);
      refreshButtons();
      snprintf(buf, sizeof(buf), "CAT a %lu", static_cast<unsigned long>(settings_.baud));
      toast(buf);
      break;
    case Action::Brightness: {
      int v = settings_.brightness + b.arg;
      settings_.brightness = static_cast<uint8_t>(v < 10 ? 10 : (v > 100 ? 100 : v));
      host_.settingsChanged(true);
      snprintf(buf, sizeof(buf), "%u %%", settings_.brightness);
      drawField(132, 46, 110, 44, buf, th_.text, 4);
      break;
    }
    case Action::SetInvert:
      settings_.invertColors = b.arg != 0;
      host_.settingsChanged(true);
      refreshButtons();
      break;
    case Action::Calibrate:
      calibStep_ = 0;
      show(Screen::Calibrate);
      break;
    case Action::OpenDisplay: show(Screen::Display); break;
    case Action::OpenBluetooth:
      bt_ = host_.btStatus();
      btResultCount_ = host_.btResults(btResults_, kBtMaxResults);
      show(Screen::Bluetooth);
      break;
    case Action::SetTransport:
      settings_.transport = b.arg ? rigui::Transport::Bluetooth : rigui::Transport::Cable;
      host_.settingsChanged(true);
      if (settings_.transport == rigui::Transport::Bluetooth && !settings_.btHaveDevice) {
        bt_ = host_.btStatus();
        btResultCount_ = host_.btResults(btResults_, kBtMaxResults);
        show(Screen::Bluetooth);
      } else {
        refreshButtons();
      }
      break;
    case Action::BtScan:
      if (settings_.transport != rigui::Transport::Bluetooth) {
        toast("Elige Bluetooth en Ajustes");
        return;
      }
      host_.btScan();
      bt_.state = rigui::BtState::Scanning;
      btResultCount_ = 0;
      layout();
      dirty_ = true;
      break;
    case Action::BtPage:
      btPage_ = b.arg < 0 ? btPage_ - 1 : btPage_ + 1;
      layout();
      dirty_ = true;
      break;
    case Action::BtSelect: {
      const rigui::BtDevice d = btResults_[b.arg].device;
      host_.btConnect(d);
      bt_.state = rigui::BtState::Connecting;
      snprintf(buf, sizeof(buf), "Conectando con %s", d.name[0] ? d.name : "adaptador");
      toast(buf);
      dirty_ = true;
      break;
    }
    case Action::BtPin:
      keypad_.clear();
      keypadError_ = false;
      keypadPin_ = true;
      show(Screen::Keypad);
      break;
    case Action::BtForget:
      host_.btForget();
      toast("Adaptador olvidado");
      dirty_ = true;
      break;
    case Action::None:
      break;
  }
}

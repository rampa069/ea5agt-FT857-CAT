// Renderiza las pantallas de la UI real (src/ui) en el ordenador y simula toques.
#include <stdio.h>

#include <utility>

#include "TFT_eSPI.h"
#include "rig_poller.h"
#include "ui/ui.h"

using namespace ft8x7;

static TFT_eSPI tft;
static rigui::Settings settings;

class PrintHost : public UiHost {
 public:
  bool sendCat(const Command& cmd) override {
    printf("  CAT -> %02X %02X %02X %02X %02X\n", cmd.bytes[0], cmd.bytes[1], cmd.bytes[2], cmd.bytes[3],
           cmd.bytes[4]);
    return true;
  }
  void settingsChanged(bool urgent) override { printf("  ajustes cambiados (%s)\n", urgent ? "ya" : "luego"); }

  // Bluetooth simulado
  rigui::BtStatus bt;
  rigui::BtDevice devices[3] = {
      {{0x98, 0xD3, 0x31, 0xF5, 0xA2, 0x10}, "HC-05", -48},
      {{0x9C, 0xB6, 0xD0, 0x93, 0xC3, 0x08}, "ea5iue-laptop", -61},
      {{0x28, 0x8F, 0xF6, 0xED, 0x3E, 0x11}, "", -80},
  };
  size_t deviceCount = 0;
  rigui::BtStatus btStatus() override { return bt; }
  size_t btResults(rigui::BtDevice* out, size_t max) override {
    size_t n = deviceCount < max ? deviceCount : max;
    for (size_t i = 0; i < n; ++i) out[i] = devices[i];
    return n;
  }
  void btScan() override { printf("  BT: buscar\n"); }
  void btConnect(const rigui::BtDevice& d) override { printf("  BT: conectar con %s\n", d.name); }
  void btForget() override { printf("  BT: olvidar\n"); }
};

static PrintHost host;
static Ui ui(tft, kDefaultTheme, settings, host);
static uint32_t now = 1000;
static const char* outDir = ".";

static void save(const char* name) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s.ppm", outDir, name);
  FILE* f = fopen(path, "wb");
  fprintf(f, "P6\n%d %d\n255\n", TFT_eSPI::W, TFT_eSPI::H);
  for (int i = 0; i < TFT_eSPI::W * TFT_eSPI::H; ++i) {
    uint16_t c = tft.fb[i];
    uint8_t rgb[3] = {uint8_t((c >> 11) * 255 / 31), uint8_t(((c >> 5) & 0x3F) * 255 / 63),
                      uint8_t((c & 0x1F) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  printf("%s\n", path);
}

static void frame(const RigState& s) {
  now += 50;
  fakeMillis = now;
  ui.update(s, now);
}

static void tap(const RigState& s, int16_t x, int16_t y) {
  printf("toque (%d,%d)\n", x, y);
  rigui::TouchEvent down;
  down.type = rigui::TouchEventType::Down;
  down.x = x;
  down.y = y;
  ui.onTouch(down, rigui::RawPoint{x, y}, s, now);
  frame(s);
  rigui::TouchEvent up = down;
  up.type = rigui::TouchEventType::Up;
  ui.onTouch(up, rigui::RawPoint{x, y}, s, now);
  frame(s);
}

static void settle(const RigState& s) {  // dejar caducar avisos
  for (int i = 0; i < 40; ++i) frame(s);
}

static RigState rigState(uint32_t hz, Mode mode, uint8_t rx, uint8_t tx) {
  RigState s;
  s.linked = true;
  s.haveFreq = true;
  Command m;
  makeSetMode(mode, m);
  s.freq = FreqMode{hz, mode, false, m.bytes[0]};
  s.rx = decodeRxStatus(rx);
  s.tx = decodeTxStatus(tx);
  s.okCount = 1873;
  s.errorCount = 2;
  s.writeCount = 14;
  return s;
}

int main(int argc, char** argv) {
  outDir = argc > 1 ? argv[1] : ".";
  settings.setDefaults(rigui::TouchCal{185, 3816, 323, 3887, false}, true);
  fakeMillis = now;

  RigState rx = rigState(14074000, Mode::USB, 0x07, 0xFF);
  ui.begin();
  frame(rx);
  save("ui_main_rx");

  RigState tx = rigState(145500000, Mode::FM, 0x00, 0x49);
  frame(tx);
  save("ui_main_tx");

  RigState down = rx;
  down.linked = false;
  frame(down);
  save("ui_main_nolink");

  frame(rx);
  tap(rx, 50, 110);  // modo
  save("ui_mode");
  tap(rx, 160, 120);  // FM
  settle(rx);

  tap(rx, 138, 110);  // banda
  save("ui_band");
  tap(rx, 120, 160);  // 10m -> 28.500 USB
  RigState band10 = rigState(28500000, Mode::USB, 0x05, 0xFF);
  frame(band10);  // la radio confirma el cambio mientras el aviso está visible
  save("ui_toast_band");
  settle(rx);

  tap(rx, 160, 50);  // teclado
  for (auto p : {std::pair<int, int>{40, 107}, {120, 107}, {200, 181}}) tap(rx, p.first, p.second);  // 1 2 .
  save("ui_keypad");
  tap(rx, 120, 218);  // OK -> 12. MHz
  settle(rx);

  tap(rx, 225, 214);  // A/B: el aviso no debe quedar tapado al soltar
  save("ui_toast_ab");
  settle(rx);
  tap(rx, 34, 214);   // < con paso 1k
  tap(rx, 98, 214);   // PASO -> 10k
  tap(rx, 162, 214);  // >
  settle(rx);
  save("ui_main_after");

  tap(rx, 287, 214);  // MENU
  save("ui_menu");
  tap(rx, 82, 75);  // SPLIT ON
  save("ui_menu_split");
  tap(rx, 238, 75);  // clarificador
  tap(rx, 56, 70);   // ON
  tap(rx, 221, 129);  // +10
  tap(rx, 283, 129);  // +100
  save("ui_clar");

  RigState fm = rigState(145500000, Mode::FM, 0x03, 0xFF);
  ui.show(Ui::Screen::Menu);
  frame(fm);
  tap(fm, 82, 139);   // repetidor
  tap(fm, 107, 64);   // desplazamiento -
  tap(fm, 159, 152);  // TSQ
  tap(fm, 289, 198);  // tono siguiente
  save("ui_repeater");

  ui.show(Ui::Screen::Settings);
  frame(rx);
  save("ui_settings");
  ui.show(Ui::Screen::Display);
  frame(rx);
  save("ui_display");

  // Bluetooth: elegir transporte, buscar, emparejar
  ui.show(Ui::Screen::Settings);
  frame(rx);
  tap(rx, 253, 116);  // [Bluetooth] -> abre la pantalla Bluetooth (sin adaptador guardado)
  settle(rx);
  save("ui_bt_empty");
  host.bt.state = rigui::BtState::Idle;
  host.deviceCount = 3;
  host.bt.scanSerial = 1;
  settle(rx);
  save("ui_bt_results");
  tap(rx, 160, 86);  // HC-05
  host.bt.state = rigui::BtState::Connecting;
  host.bt.haveDevice = true;
  host.bt.device = host.devices[0];
  host.bt.confirmCode = 482913;
  settle(rx);
  save("ui_bt_confirm");
  host.bt.confirmCode = 0;
  host.bt.state = rigui::BtState::Connected;
  settings.btHaveDevice = true;
  settings.btDevice = host.devices[0];
  settle(rx);
  save("ui_bt_connected");
  tap(rx, 158, 216);  // PIN
  tap(rx, 40, 107);
  tap(rx, 120, 107);
  tap(rx, 200, 107);
  tap(rx, 40, 144);
  save("ui_bt_pin");
  tap(rx, 160, 218);  // OK
  settle(rx);
  ui.show(Ui::Screen::Main);
  frame(rx);
  save("ui_main_bt");
  ui.show(Ui::Screen::Diag);
  frame(rx);
  save("ui_diag");
  ui.show(Ui::Screen::Calibrate);
  frame(rx);
  save("ui_calibrate");
  return 0;
}

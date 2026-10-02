// Renderiza la pantalla principal (src/rig_display.cpp) en el ordenador y la guarda como PPM.
#include <stdio.h>

#include "TFT_eSPI.h"
#include "rig_display.h"

using namespace ft8x7;

static void savePpm(const TFT_eSPI& tft, const char* path) {
  FILE* f = fopen(path, "wb");
  fprintf(f, "P6\n%d %d\n255\n", TFT_eSPI::W, TFT_eSPI::H);
  for (int i = 0; i < TFT_eSPI::W * TFT_eSPI::H; ++i) {
    uint16_t c = tft.fb[i];
    uint8_t rgb[3] = {uint8_t((c >> 11) * 255 / 31), uint8_t(((c >> 5) & 0x3F) * 255 / 63),
                      uint8_t((c & 0x1F) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

static void render(const RigState& s, const char* model, const char* path) {
  static TFT_eSPI tft;
  RigDisplay display(tft);
  fakeMillis = 1;
  display.begin(model, 4800);
  display.update(s);
  savePpm(tft, path);
  printf("%s\n", path);
}

static RigState linkedState(uint32_t hz, Mode mode, bool narrow) {
  RigState s;
  s.linked = true;
  s.haveFreq = true;
  s.freq = FreqMode{hz, mode, narrow, 0};
  s.tx = decodeTxStatus(0xFF);
  s.okCount = 1873;
  s.errorCount = 2;
  return s;
}

int main(int argc, char** argv) {
  const char* dir = argc > 1 ? argv[1] : ".";
  char path[512];

  RigState rx = linkedState(14074000, Mode::USB, false);
  rx.rx = decodeRxStatus(0x07);  // S7, squelch abierto
  snprintf(path, sizeof(path), "%s/screen_rx.ppm", dir);
  render(rx, "FT-857", path);

  RigState rxStrong = linkedState(7030000, Mode::CW, true);
  rxStrong.rx = decodeRxStatus(0x0B);  // S9+20
  snprintf(path, sizeof(path), "%s/screen_rx_s9plus.ppm", dir);
  render(rxStrong, "FT-857", path);

  RigState tx = linkedState(145500000, Mode::FM, false);
  tx.tx = decodeTxStatus(0x49);  // TX, SWR alta, split ON, PO=9
  snprintf(path, sizeof(path), "%s/screen_tx.ppm", dir);
  render(tx, "FT-817", path);

  RigState down = rx;
  down.linked = false;
  snprintf(path, sizeof(path), "%s/screen_nolink.ppm", dir);
  render(down, "FT-857", path);
  return 0;
}

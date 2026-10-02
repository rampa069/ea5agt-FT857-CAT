#pragma once

// Pieles (temas) de la interfaz: cómo se dibujan la pantalla principal y los botones.
// La lógica (qué mostrar, qué hace cada toque) está en Ui; la piel sólo dibuja y dice dónde
// caen sus zonas táctiles.

#include <TFT_eSPI.h>

#include "theme.h"

struct Rect {
  int16_t x, y, w, h;
  bool contains(int16_t px, int16_t py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

enum class ButtonLook : uint8_t { Normal, On, Disabled };

enum class LinkKind : uint8_t { Ok, Lost, Connecting, NoCat };

// Lo que la pantalla principal tiene que enseñar (lo prepara Ui a partir del estado de la radio).
struct MainView {
  bool live = false;      // hay enlace CAT
  bool haveFreq = false;
  uint32_t hz = 0;
  char mode[8] = "";      // "USB", "CW-N"...
  int band = -1;          // índice en rigui::kBands o -1
  char bandName[8] = "";  // "20m", "GEN", "---"
  bool tx = false;
  bool split = false;
  uint8_t sqlClar = 0;    // 0 nada, 1 squelch cerrado, 2 clarificador activo
  uint8_t vfo = 0;        // 0 desconocido, 1 A, 2 B
  bool lock = false;
  const char* model = "";
  const char* link = "";  // "CAT OK", "BT OK", "NO LINK", "BT ...", "BT sin CAT"
  LinkKind linkKind = LinkKind::Lost;
  uint8_t level = 0;      // 0..15: S-meter en RX, potencia en TX
  bool highSwr = false;
  bool haveMeters = false;
  uint8_t meterSwr = 0, meterAlc = 0;
  char levelText[8] = ""; // "S9+20", "9", "--"
};

struct MainZones {
  Rect freq;   // tocar: teclado de frecuencia
  Rect mode;   // tocar: selector de modo
  Rect band;   // tocar: selector de banda
  Rect dial;   // tocar: sintonizar a ese punto de la escala (w = 0 si la piel no tiene escala)
};

class Skin {
 public:
  virtual ~Skin() = default;

  virtual const char* name() const = 0;   // para Ajustes ("Clasico", "Ambar"...)
  virtual const Theme& theme() const = 0;  // colores de subpantallas y avisos
  virtual const MainZones& zones() const = 0;

  // Al activarse o dejar de usarse (sprites, memoria).
  virtual void enter(TFT_eSPI& tft) { (void)tft; }
  virtual void leave() {}

  // Fondo y partes fijas de la pantalla principal (sin los botones de abajo).
  virtual void drawMainStatic(TFT_eSPI& tft) = 0;
  // Partes que cambian. prev == nullptr: dibujar todo. Se llama en cada vuelta del bucle,
  // así que puede animar (agujas) aunque la vista no cambie.
  virtual void drawMainDynamic(TFT_eSPI& tft, const MainView& v, const MainView* prev, uint32_t nowMs) = 0;

  // Botón de cualquier pantalla. Etiquetas especiales: "<" ">" "^" "v" (flechas),
  // "\x01Volver" (botón volver) y "PASO\n1k" (rótulo pequeño + valor).
  virtual void drawButton(TFT_eSPI& tft, const Rect& r, const char* label, ButtonLook look, bool pressed,
                          bool big) = 0;
  // ¿Cabe la etiqueta con la letra grande de la piel?
  virtual bool fitsBig(TFT_eSPI& tft, const char* label, const Rect& r) = 0;

 protected:
  // Ayudas comunes para las etiquetas especiales.
  static bool isArrow(const char* label) {
    return label[1] == '\0' && (label[0] == '<' || label[0] == '>' || label[0] == '^' || label[0] == 'v');
  }
  static void drawArrowShape(TFT_eSPI& tft, int16_t cx, int16_t cy, char dir, uint16_t color, uint16_t bg) {
    switch (dir) {
      case '<': tft.fillTriangle(cx - 8, cy, cx + 8, cy - 10, cx + 8, cy + 10, color); break;
      case '>': tft.fillTriangle(cx + 8, cy, cx - 8, cy - 10, cx - 8, cy + 10, color); break;
      case '^': tft.fillTriangle(cx, cy - 8, cx - 10, cy + 8, cx + 10, cy + 8, color); break;
      default: tft.fillTriangle(cx, cy + 8, cx - 10, cy - 8, cx + 10, cy - 8, color); break;
    }
    (void)bg;
  }
};

constexpr const char* kBackLabel = "\x01Volver";

// Registro de pieles (skins.cpp).
constexpr size_t kSkinCount = 4;
Skin& skinAt(size_t index);

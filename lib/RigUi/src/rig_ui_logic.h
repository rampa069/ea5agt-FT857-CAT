#pragma once

// Lógica de la interfaz sin dibujo ni hardware: bandas, sintonía, teclado, modelos.

#include <stddef.h>
#include <stdint.h>

#include "ft8x7_protocol.h"

namespace rigui {

using ft8x7::Mode;

enum class RigModel : uint8_t { FT817, FT818, FT857, FT897 };
constexpr size_t kModelCount = 4;
const char* modelName(RigModel model);  // "FT-857"
bool modelHas60m(RigModel model);        // el FT-817 original no tiene 60 m

struct Band {
  const char* name;
  uint32_t loHz, hiHz;
  uint32_t defaultHz;
  Mode defaultMode;
};
constexpr size_t kBandCount = 15;
extern const Band kBands[kBandCount];
constexpr int kNoBand = -1;
int bandIndexFor(uint32_t hz);
bool bandAvailable(size_t index, RigModel model);

// Rango que muestra una escala de dial para esta frecuencia: la banda entera o, fuera de las
// bandas, el MHz que la contiene.
void dialRange(uint32_t hz, uint32_t& lo, uint32_t& hi);

// Rangos de recepción comunes a FT-817/818/857/897.
bool inRxRange(uint32_t hz);

constexpr size_t kStepCount = 6;
extern const uint32_t kSteps[kStepCount];  // 10 Hz .. 1 MHz
const char* stepLabel(size_t index);        // "10Hz", "1k"...

// Sube o baja un paso ajustando a la rejilla del paso (14.074.03 +1k -> 14.075.00).
// False si el resultado queda fuera de los rangos de recepción.
bool tune(uint32_t hz, uint32_t step, int direction, uint32_t& out);

// Última frecuencia y modo usados en cada banda (CAT no tiene comando de banda).
struct BandMemory {
  uint32_t hz[kBandCount];
  uint8_t mode[kBandCount];  // código CAT del modo

  void reset();
  void remember(uint32_t hz, Mode mode);  // ignora frecuencias fuera de banda
  void recall(size_t index, uint32_t& hz, Mode& mode) const;
};

// Entrada de frecuencia en MHz desde el teclado ("145.5", "7.0745").
class KeypadEntry {
 public:
  static constexpr size_t kMaxLen = 10;

  void clear();
  bool press(char key);  // dígito o '.'; false si no cabe o es un segundo punto
  void backspace();
  const char* text() const { return text_; }
  bool empty() const { return len_ == 0; }
  // Valida formato y rango; redondea a 10 Hz.
  bool value(uint32_t& hz) const;

 private:
  char text_[kMaxLen + 1] = "";
  size_t len_ = 0;
};

// Índice siguiente/anterior con vuelta, para recorrer listas de tonos.
size_t wrapIndex(size_t index, int delta, size_t count);

}  // namespace rigui

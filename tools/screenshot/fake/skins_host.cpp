// En el renderizador sólo existe el tema clásico: los retro usan sprites y fuentes suaves que la
// imitación de TFT_eSPI no implementa (se revisan en la placa).
#include "skin.h"

Skin& classicSkin();

Skin& skinAt(size_t) { return classicSkin(); }

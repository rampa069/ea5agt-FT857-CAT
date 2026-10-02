#include "skin.h"

Skin& classicSkin();
Skin& amberSkin();
Skin& nixieSkin();
Skin& dialSkin();

Skin& skinAt(size_t index) {
  switch (index) {
    case 1: return amberSkin();
    case 2: return nixieSkin();
    case 3: return dialSkin();
    default: return classicSkin();
  }
}

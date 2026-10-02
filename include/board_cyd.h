#pragma once

// Pines del ESP32-2432S028R que no gestiona TFT_eSPI.

// Táctil XPT2046 (bus SPI independiente del TFT)
constexpr int TOUCH_IRQ  = 36;
constexpr int TOUCH_MOSI = 32;
constexpr int TOUCH_MISO = 39;
constexpr int TOUCH_CLK  = 25;
constexpr int TOUCH_CS   = 33;

// Rango crudo del XPT2046 en los bordes de la pantalla (rotación 1), medido con
// el env touchtest el 2026-10-02: error < 5 px en esquinas y centro.
constexpr bool TOUCH_SWAP_XY = false;
constexpr int TOUCH_RAW_X_MIN = 185;
constexpr int TOUCH_RAW_X_MAX = 3816;
constexpr int TOUCH_RAW_Y_MIN = 323;
constexpr int TOUCH_RAW_Y_MAX = 3887;

// LED RGB (activo a nivel bajo) y LDR
constexpr int LED_R = 4;
constexpr int LED_G = 16;
constexpr int LED_B = 17;
constexpr int LDR_PIN = 34;

// Pines libres en CN1 reservados para el UART CAT (UART2)
constexpr int CAT_RX_PIN = 27;
constexpr int CAT_TX_PIN = 22;

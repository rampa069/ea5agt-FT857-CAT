#include "theme.h"

#include <TFT_eSPI.h>

const Theme kDefaultTheme = {
    TFT_BLACK,      // bg
    0x10A2,         // header
    TFT_WHITE,      // text
    TFT_DARKGREY,   // textDim
    TFT_YELLOW,     // freq
    TFT_DARKGREY,   // freqStale
    TFT_NAVY,       // box
    0x2104,         // boxStale
    0x1946,         // button
    0x2A6F,         // buttonOn
    TFT_CYAN,       // buttonOnBorder
    0x4A7F,         // buttonPressed
    0x4208,         // buttonDisabledText
    0x1082,         // field
    TFT_DARKGREEN,  // linkOk
    TFT_RED,        // linkLost
    TFT_DARKGREEN,  // rx
    TFT_RED,        // tx
    TFT_CYAN,       // flagSplit
    TFT_SKYBLUE,    // flagSql
    TFT_ORANGE,     // flagClar
    0x2945,         // flagOff
    0x2945,         // meterOff
    TFT_GREEN,      // sMeter
    TFT_RED,        // sMeterOver
    TFT_ORANGE,     // po
    TFT_RED,        // poHigh
    TFT_RED,        // warn
    0xFE60,         // accent
};

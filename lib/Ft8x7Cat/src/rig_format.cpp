#include "rig_format.h"

#include <stdio.h>

namespace ft8x7 {

void formatFrequency(uint32_t hz, char* buf, size_t len) {
  uint32_t tens = hz / 10;
  snprintf(buf, len, "%lu.%03lu.%02lu", static_cast<unsigned long>(tens / 100000),
           static_cast<unsigned long>((tens / 100) % 1000), static_cast<unsigned long>(tens % 100));
}

void formatSMeter(uint8_t sMeter, char* buf, size_t len) {
  if (sMeter <= 9) {
    snprintf(buf, len, "S%u", sMeter);
  } else {
    snprintf(buf, len, "S9+%d", sMeterDbOverS9(sMeter));
  }
}

void formatMode(const FreqMode& fm, char* buf, size_t len) {
  snprintf(buf, len, "%s%s", modeName(fm.mode), fm.narrow ? "-N" : "");
}

}  // namespace ft8x7

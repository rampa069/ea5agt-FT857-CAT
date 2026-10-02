#include "rig_ui_logic.h"

namespace rigui {

const Band kBands[kBandCount] = {
    {"160m", 1800000, 2000000, 1840000, Mode::LSB},
    {"80m", 3500000, 4000000, 3700000, Mode::LSB},
    {"60m", 5250000, 5450000, 5351500, Mode::USB},
    {"40m", 7000000, 7300000, 7100000, Mode::LSB},
    {"30m", 10100000, 10150000, 10120000, Mode::CW},
    {"20m", 14000000, 14350000, 14200000, Mode::USB},
    {"17m", 18068000, 18168000, 18120000, Mode::USB},
    {"15m", 21000000, 21450000, 21200000, Mode::USB},
    {"12m", 24890000, 24990000, 24940000, Mode::USB},
    {"10m", 28000000, 29700000, 28500000, Mode::USB},
    {"6m", 50000000, 54000000, 50150000, Mode::USB},
    {"2m", 144000000, 148000000, 145500000, Mode::FM},
    {"70cm", 430000000, 450000000, 433500000, Mode::FM},
    {"AIR", 118000000, 137000000, 125000000, Mode::AM},
    {"FM-BC", 76000000, 108000000, 98000000, Mode::WFM},
};

const uint32_t kSteps[kStepCount] = {10, 100, 1000, 10000, 100000, 1000000};

namespace {

struct Range {
  uint32_t lo, hi;
};
constexpr Range kRxRanges[] = {
    {100000, 56000000},
    {76000000, 108000000},
    {118000000, 164000000},
    {420000000, 470000000},
};

uint8_t modeToCode(Mode mode) {
  ft8x7::Command c;
  return ft8x7::makeSetMode(mode, c) ? c.bytes[0] : 0x01;
}

}  // namespace

const char* modelName(RigModel model) {
  switch (model) {
    case RigModel::FT817: return "FT-817";
    case RigModel::FT818: return "FT-818";
    case RigModel::FT857: return "FT-857";
    case RigModel::FT897: return "FT-897";
  }
  return "FT-8x7";
}

bool modelHas60m(RigModel model) { return model != RigModel::FT817; }

int bandIndexFor(uint32_t hz) {
  for (size_t i = 0; i < kBandCount; ++i) {
    if (hz >= kBands[i].loHz && hz <= kBands[i].hiHz) {
      return static_cast<int>(i);
    }
  }
  return kNoBand;
}

bool bandAvailable(size_t index, RigModel model) {
  return index < kBandCount && (index != 2 || modelHas60m(model));
}

void dialRange(uint32_t hz, uint32_t& lo, uint32_t& hi) {
  int b = bandIndexFor(hz);
  if (b != kNoBand) {
    lo = kBands[b].loHz;
    hi = kBands[b].hiHz;
  } else {
    lo = hz / 1000000 * 1000000;
    hi = lo + 1000000;
  }
}

bool inRxRange(uint32_t hz) {
  for (const Range& r : kRxRanges) {
    if (hz >= r.lo && hz <= r.hi) {
      return true;
    }
  }
  return false;
}

const char* stepLabel(size_t index) {
  static const char* const kLabels[kStepCount] = {"10Hz", "100Hz", "1k", "10k", "100k", "1M"};
  return index < kStepCount ? kLabels[index] : "?";
}

bool tune(uint32_t hz, uint32_t step, int direction, uint32_t& out) {
  if (step == 0 || direction == 0) {
    return false;
  }
  uint32_t base = hz / step * step;
  uint32_t next;
  if (direction > 0) {
    next = base + step;
  } else {
    if (base == hz) {
      if (hz < step) {
        return false;
      }
      next = hz - step;
    } else {
      next = base;
    }
  }
  if (!inRxRange(next)) {
    return false;
  }
  out = next;
  return true;
}

void BandMemory::reset() {
  for (size_t i = 0; i < kBandCount; ++i) {
    hz[i] = kBands[i].defaultHz;
    mode[i] = modeToCode(kBands[i].defaultMode);
  }
}

void BandMemory::remember(uint32_t freqHz, Mode m) {
  int b = bandIndexFor(freqHz);
  if (b == kNoBand || m == Mode::Unknown) {
    return;
  }
  hz[b] = freqHz;
  mode[b] = modeToCode(m);
}

void BandMemory::recall(size_t index, uint32_t& outHz, Mode& outMode) const {
  outHz = hz[index];
  outMode = ft8x7::decodeMode(mode[index]);
}

void KeypadEntry::clear() {
  len_ = 0;
  text_[0] = '\0';
}

bool KeypadEntry::press(char key) {
  bool isDigit = key >= '0' && key <= '9';
  if (!isDigit && key != '.') {
    return false;
  }
  if (len_ >= kMaxLen) {
    return false;
  }
  if (key == '.') {
    for (size_t i = 0; i < len_; ++i) {
      if (text_[i] == '.') {
        return false;
      }
    }
  }
  text_[len_++] = key;
  text_[len_] = '\0';
  return true;
}

void KeypadEntry::backspace() {
  if (len_ > 0) {
    text_[--len_] = '\0';
  }
}

bool KeypadEntry::value(uint32_t& hz) const {
  // Parseo a mano en Hz enteros para no arrastrar errores de coma flotante.
  uint64_t whole = 0;
  uint64_t frac = 0;
  int fracDigits = 0;
  bool seenDot = false;
  bool anyDigit = false;
  for (size_t i = 0; i < len_; ++i) {
    char c = text_[i];
    if (c == '.') {
      seenDot = true;
      continue;
    }
    anyDigit = true;
    if (!seenDot) {
      whole = whole * 10 + (c - '0');
    } else if (fracDigits < 6) {
      frac = frac * 10 + (c - '0');
      ++fracDigits;
    }
  }
  if (!anyDigit || whole > 999) {
    return false;
  }
  for (int i = fracDigits; i < 6; ++i) {
    frac *= 10;
  }
  uint64_t total = whole * 1000000ULL + frac;
  total = (total + 5) / 10 * 10;  // redondeo a 10 Hz
  if (!inRxRange(static_cast<uint32_t>(total))) {
    return false;
  }
  hz = static_cast<uint32_t>(total);
  return true;
}

size_t wrapIndex(size_t index, int delta, size_t count) {
  long v = (static_cast<long>(index) + delta) % static_cast<long>(count);
  return static_cast<size_t>(v < 0 ? v + count : v);
}

}  // namespace rigui

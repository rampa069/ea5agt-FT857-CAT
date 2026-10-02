#include "ft8x7_protocol.h"

namespace ft8x7 {

Command makeCommand(Opcode op, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4) {
  return Command{{p1, p2, p3, p4, static_cast<uint8_t>(op)}};
}

Command makeReadEepromCommand(uint16_t address) {
  // La radio devuelve 2 bytes empezando en una dirección par (igual que hamlib).
  return makeCommand(Opcode::ReadEeprom, static_cast<uint8_t>(address >> 8),
                     static_cast<uint8_t>(address & 0xFE));
}

const uint16_t kCtcssTones[kCtcssToneCount] = {
    670,  693,  719,  744,  770,  797,  825,  854,  885,  915,  948,  974,  1000, 1035, 1072, 1109, 1148,
    1188, 1230, 1273, 1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679, 1713, 1738, 1773, 1799,
    1835, 1862, 1899, 1928, 1966, 1995, 2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541,
};

const uint16_t kDcsCodes[kDcsCodeCount] = {
    23,  25,  26,  31,  32,  36,  43,  47,  51,  53,  54,  65,  71,  72,  73,  74,  114, 115, 116, 122, 125,
    131, 132, 134, 143, 145, 152, 155, 156, 162, 165, 172, 174, 205, 212, 223, 225, 226, 243, 244, 245, 246,
    251, 252, 255, 261, 263, 265, 266, 271, 274, 306, 311, 315, 325, 331, 332, 343, 346, 351, 356, 364, 365,
    371, 411, 412, 413, 423, 431, 432, 445, 446, 452, 454, 455, 462, 464, 465, 466, 503, 506, 516, 523, 526,
    532, 546, 565, 606, 612, 624, 627, 631, 632, 654, 662, 664, 703, 712, 723, 731, 732, 734, 743, 754,
};

namespace {

// value -> digits dígitos BCD empaquetados en digits/2 bytes (big-endian).
void encodeBcd(uint32_t value, uint8_t* out, int digits) {
  for (int i = digits / 2 - 1; i >= 0; --i) {
    uint8_t lo = value % 10;
    value /= 10;
    uint8_t hi = value % 10;
    value /= 10;
    out[i] = static_cast<uint8_t>(hi << 4 | lo);
  }
}

bool inList(const uint16_t* list, size_t len, uint16_t value) {
  for (size_t i = 0; i < len; ++i) {
    if (list[i] == value) {
      return true;
    }
  }
  return false;
}

uint8_t modeCode(Mode mode) {
  switch (mode) {
    case Mode::LSB: return 0x00;
    case Mode::USB: return 0x01;
    case Mode::CW: return 0x02;
    case Mode::CWR: return 0x03;
    case Mode::AM: return 0x04;
    case Mode::WFM: return 0x06;
    case Mode::FM: return 0x08;
    case Mode::DIG: return 0x0A;
    case Mode::PKT: return 0x0C;
    case Mode::Unknown: break;
  }
  return 0xFF;
}

}  // namespace

bool encodeBcdFrequency(uint32_t hz, uint8_t bcd[4]) {
  if (hz % 10 != 0 || hz / 10 > 99999999UL) {
    return false;
  }
  encodeBcd(hz / 10, bcd, 8);
  return true;
}

bool makeSetFrequency(uint32_t hz, Command& cmd) {
  uint8_t bcd[4];
  if (!encodeBcdFrequency(hz, bcd)) {
    return false;
  }
  cmd = makeCommand(Opcode::SetFrequency, bcd[0], bcd[1], bcd[2], bcd[3]);
  return true;
}

bool decodeSetFrequency(const Command& cmd, uint32_t& hz) {
  return cmd.bytes[4] == static_cast<uint8_t>(Opcode::SetFrequency) && decodeBcdFrequency(cmd.bytes, hz);
}

bool makeSetMode(Mode mode, Command& cmd) {
  uint8_t code = modeCode(mode);
  if (code == 0xFF) {
    return false;
  }
  cmd = makeCommand(Opcode::SetMode, code);
  return true;
}

Command makeToggleVfo() { return makeCommand(Opcode::ToggleVfo); }
Command makeSplit(bool on) { return makeCommand(on ? Opcode::SplitOn : Opcode::SplitOff); }
Command makeClarifier(bool on) { return makeCommand(on ? Opcode::ClarifierOn : Opcode::ClarifierOff); }
Command makeLock(bool on) { return makeCommand(on ? Opcode::LockOn : Opcode::LockOff); }

bool makeClarifierOffset(int32_t hz, Command& cmd) {
  uint32_t mag = hz < 0 ? -hz : hz;
  if (mag % 10 != 0 || mag > 9990) {
    return false;
  }
  uint8_t bcd[2];
  encodeBcd(mag / 10, bcd, 4);  // 12 34 = 12,34 kHz
  cmd = makeCommand(Opcode::ClarifierOffset, hz < 0 ? 0x01 : 0x00, 0x00, bcd[0], bcd[1]);
  return true;
}

Command makeRepeaterShift(RepeaterShift shift) {
  return makeCommand(Opcode::RepeaterShift, static_cast<uint8_t>(shift));
}

bool makeRepeaterOffset(uint32_t hz, Command& cmd) {
  if (hz > 99999999UL) {
    return false;
  }
  uint8_t bcd[4];
  encodeBcd(hz, bcd, 8);  // 05 43 21 00 = 5,4321 MHz
  cmd = makeCommand(Opcode::RepeaterOffset, bcd[0], bcd[1], bcd[2], bcd[3]);
  return true;
}

Command makeToneMode(ToneMode mode) { return makeCommand(Opcode::ToneMode, static_cast<uint8_t>(mode)); }

bool makeCtcssTone(uint16_t tenthsHz, Command& cmd) {
  if (!inList(kCtcssTones, kCtcssToneCount, tenthsHz)) {
    return false;
  }
  uint8_t bcd[2];
  encodeBcd(tenthsHz, bcd, 4);  // 08 85 = 88,5 Hz
  cmd = makeCommand(Opcode::CtcssTone, bcd[0], bcd[1]);
  return true;
}

bool makeDcsCode(uint16_t code, Command& cmd) {
  if (!inList(kDcsCodes, kDcsCodeCount, code)) {
    return false;
  }
  uint8_t bcd[2];
  encodeBcd(code, bcd, 4);  // 00 23 = 023
  cmd = makeCommand(Opcode::DcsCode, bcd[0], bcd[1]);
  return true;
}

bool decodeBcdFrequency(const uint8_t bcd[4], uint32_t& hz) {
  uint32_t value = 0;
  for (int i = 0; i < 4; ++i) {
    uint8_t hi = bcd[i] >> 4;
    uint8_t lo = bcd[i] & 0x0F;
    if (hi > 9 || lo > 9) {
      return false;
    }
    value = value * 100 + hi * 10 + lo;
  }
  hz = value * 10;
  return true;
}

Mode decodeMode(uint8_t raw) {
  switch (raw & 0x7F) {
    case 0x00: return Mode::LSB;
    case 0x01: return Mode::USB;
    case 0x02: return Mode::CW;
    case 0x03: return Mode::CWR;
    case 0x04: return Mode::AM;
    case 0x06: return Mode::WFM;
    case 0x08: return Mode::FM;
    case 0x0A: return Mode::DIG;
    case 0x0C: return Mode::PKT;
    default: return Mode::Unknown;
  }
}

bool decodeFreqMode(const uint8_t response[kFreqModeResponseLength], FreqMode& out) {
  uint32_t hz;
  if (!decodeBcdFrequency(response, hz)) {
    return false;
  }
  out.hz = hz;
  out.rawMode = response[4];
  out.mode = decodeMode(response[4]);
  out.narrow = (response[4] & 0x80) != 0;
  return true;
}

RxStatus decodeRxStatus(uint8_t raw) {
  RxStatus s;
  s.sMeter = raw & 0x0F;
  s.discriminatorOffCenter = (raw & 0x20) != 0;
  s.toneMismatch = (raw & 0x40) != 0;
  s.squelched = (raw & 0x80) != 0;
  return s;
}

TxMeters decodeTxMeters(const uint8_t r[2]) {
  return TxMeters{static_cast<uint8_t>(r[0] >> 4), static_cast<uint8_t>(r[0] & 0x0F),
                  static_cast<uint8_t>(r[1] >> 4), static_cast<uint8_t>(r[1] & 0x0F)};
}

uint8_t pickEepromByte(uint16_t address, const uint8_t response[2]) { return response[address & 1]; }

TxStatus decodeTxStatus(uint8_t raw) {
  TxStatus s;
  s.transmitting = (raw & 0x80) == 0;
  s.poMeter = s.transmitting ? (raw & 0x0F) : 0;
  s.highSwr = s.transmitting && (raw & 0x40) != 0;
  s.split = s.transmitting && (raw & 0x20) == 0;
  return s;
}

const char* modeName(Mode mode) {
  switch (mode) {
    case Mode::LSB: return "LSB";
    case Mode::USB: return "USB";
    case Mode::CW: return "CW";
    case Mode::CWR: return "CWR";
    case Mode::AM: return "AM";
    case Mode::WFM: return "WFM";
    case Mode::FM: return "FM";
    case Mode::DIG: return "DIG";
    case Mode::PKT: return "PKT";
    case Mode::Unknown: break;
  }
  return "???";
}

int sMeterDbOverS9(uint8_t sMeter) {
  if (sMeter >= 9) {
    return (sMeter - 9) * 10;
  }
  return sMeter * 6 - 54;
}

}  // namespace ft8x7

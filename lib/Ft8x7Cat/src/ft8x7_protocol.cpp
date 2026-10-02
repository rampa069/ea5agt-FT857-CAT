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

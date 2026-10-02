#pragma once

// Protocolo CAT de Yaesu FT-817/818/857/897: tramas de 5 bytes [P1 P2 P3 P4 OPCODE].
// Codificación y decodificación pura, sin dependencias de Arduino (testeable en nativo).

#include <stddef.h>
#include <stdint.h>

namespace ft8x7 {

constexpr size_t kCommandLength = 5;
constexpr size_t kFreqModeResponseLength = 5;

enum class Opcode : uint8_t {
  ReadFreqMode = 0x03,
  ReadRxStatus = 0xE7,
  ReadTxStatus = 0xF7,
  ReadEeprom = 0xBB,  // no documentado; nunca usar 0xBC (escritura)
};

enum class Mode : uint8_t { LSB, USB, CW, CWR, AM, WFM, FM, DIG, PKT, Unknown };

struct Command {
  uint8_t bytes[kCommandLength];
};

struct FreqMode {
  uint32_t hz;
  Mode mode;
  bool narrow;      // bit 7 del byte de modo (filtro estrecho en el 857/897)
  uint8_t rawMode;  // byte de modo tal cual lo envía la radio
};

struct RxStatus {
  uint8_t sMeter;  // 0..15: 0-9 = S0-S9, 10-15 = S9+10..S9+60 dB
  bool squelched;
  bool toneMismatch;  // CTCSS/DCS no coincide
  bool discriminatorOffCenter;
};

struct TxStatus {
  // La radio responde 0xFF en recepción; en TX el bit 7 vale 0 (criterio de hamlib).
  bool transmitting;
  uint8_t poMeter;  // 0..15, sólo válido si transmitting
  bool highSwr;
  bool split;  // manual: bit 5 a 0 = split ON (hamlib lo interpreta al revés; pendiente verificar)
};

Command makeCommand(Opcode op, uint8_t p1 = 0, uint8_t p2 = 0, uint8_t p3 = 0, uint8_t p4 = 0);
Command makeReadEepromCommand(uint16_t address);

// 4 bytes BCD big-endian en pasos de 10 Hz (43 97 00 00 = 439.700,00 MHz).
// Devuelve false si algún nibble no es un dígito decimal.
bool decodeBcdFrequency(const uint8_t bcd[4], uint32_t& hz);

bool decodeFreqMode(const uint8_t response[kFreqModeResponseLength], FreqMode& out);
Mode decodeMode(uint8_t raw);
RxStatus decodeRxStatus(uint8_t raw);
TxStatus decodeTxStatus(uint8_t raw);

const char* modeName(Mode mode);

// Nivel en dB relativo a S9 (S0 = -54, S9 = 0, S9+20 = +20), como hamlib.
int sMeterDbOverS9(uint8_t sMeter);

}  // namespace ft8x7

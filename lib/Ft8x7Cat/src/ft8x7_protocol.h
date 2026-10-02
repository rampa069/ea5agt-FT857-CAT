#pragma once

// Protocolo CAT de Yaesu FT-817/818/857/897: tramas de 5 bytes [P1 P2 P3 P4 OPCODE].
// Codificación y decodificación pura, sin dependencias de Arduino (testeable en nativo).

#include <stddef.h>
#include <stdint.h>

namespace ft8x7 {

constexpr size_t kCommandLength = 5;
constexpr size_t kFreqModeResponseLength = 5;

// Sin PTT (0x08/0x88) a propósito: el display nunca transmite. Nunca usar 0xBC (escritura EEPROM).
enum class Opcode : uint8_t {
  LockOn = 0x00,
  SetFrequency = 0x01,
  SplitOn = 0x02,
  ReadFreqMode = 0x03,
  ClarifierOn = 0x05,
  SetMode = 0x07,
  RepeaterShift = 0x09,
  ToneMode = 0x0A,
  CtcssTone = 0x0B,
  DcsCode = 0x0C,
  LockOff = 0x80,
  ToggleVfo = 0x81,
  SplitOff = 0x82,
  ClarifierOff = 0x85,
  ReadEeprom = 0xBB,  // no documentado
  ReadRxStatus = 0xE7,
  ClarifierOffset = 0xF5,
  ReadTxStatus = 0xF7,
  RepeaterOffset = 0xF9,
};

enum class Mode : uint8_t { LSB, USB, CW, CWR, AM, WFM, FM, DIG, PKT, Unknown };

enum class RepeaterShift : uint8_t { Minus = 0x09, Plus = 0x49, Simplex = 0x89 };
enum class ToneMode : uint8_t { Dcs = 0x0A, Ctcss = 0x2A, Encoder = 0x4A, Off = 0x8A };

// Tonos CTCSS (décimas de Hz) y códigos DCS que acepta la radio (manual FT-817, notas 1 y 2).
constexpr size_t kCtcssToneCount = 50;
constexpr size_t kDcsCodeCount = 104;
extern const uint16_t kCtcssTones[kCtcssToneCount];
extern const uint16_t kDcsCodes[kDcsCodeCount];

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

// --- Escritura. Las que validan devuelven false (sin tocar cmd) si el valor no es válido. ---
bool makeSetFrequency(uint32_t hz, Command& cmd);  // pasos de 10 Hz, hasta 999.999,99 MHz
bool makeSetMode(Mode mode, Command& cmd);
Command makeToggleVfo();
Command makeSplit(bool on);
Command makeClarifier(bool on);
Command makeLock(bool on);
bool makeClarifierOffset(int32_t hz, Command& cmd);  // ±9990 Hz en pasos de 10 Hz
Command makeRepeaterShift(RepeaterShift shift);
bool makeRepeaterOffset(uint32_t hz, Command& cmd);  // hasta 99,999999 MHz, en Hz
Command makeToneMode(ToneMode mode);
bool makeCtcssTone(uint16_t tenthsHz, Command& cmd);  // 885 = 88,5 Hz
bool makeDcsCode(uint16_t code, Command& cmd);        // 23 = DCS 023

// Inversa de decodeBcdFrequency: hz (múltiplo de 10) -> 4 bytes BCD.
bool encodeBcdFrequency(uint32_t hz, uint8_t bcd[4]);
bool decodeSetFrequency(const Command& cmd, uint32_t& hz);

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

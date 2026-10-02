#include "ft8x7_cat.h"

namespace ft8x7 {

const char* catResultName(CatResult result) {
  switch (result) {
    case CatResult::Ok: return "OK";
    case CatResult::Timeout: return "TIMEOUT";
    case CatResult::BadData: return "BAD_DATA";
  }
  return "?";
}

CatResult Ft8x7Cat::transact(const Command& cmd, uint8_t* response, size_t len) {
  // Sin checksum ni delimitadores: descartar restos de respuestas anteriores para no desalinear.
  port_.discardInput();
  port_.write(cmd.bytes, kCommandLength);
  if (port_.read(response, len, timeoutMs_) != len) {
    return CatResult::Timeout;
  }
  return CatResult::Ok;
}

CatResult Ft8x7Cat::readFreqMode(FreqMode& out) {
  uint8_t resp[kFreqModeResponseLength];
  CatResult r = transact(makeCommand(Opcode::ReadFreqMode), resp, sizeof(resp));
  if (r != CatResult::Ok) {
    return r;
  }
  return decodeFreqMode(resp, out) ? CatResult::Ok : CatResult::BadData;
}

CatResult Ft8x7Cat::readRxStatus(RxStatus& out) {
  uint8_t raw;
  CatResult r = transact(makeCommand(Opcode::ReadRxStatus), &raw, 1);
  if (r == CatResult::Ok) {
    out = decodeRxStatus(raw);
  }
  return r;
}

CatResult Ft8x7Cat::readTxStatus(TxStatus& out) {
  uint8_t raw;
  CatResult r = transact(makeCommand(Opcode::ReadTxStatus), &raw, 1);
  if (r == CatResult::Ok) {
    out = decodeTxStatus(raw);
  }
  return r;
}

CatResult Ft8x7Cat::readEeprom(uint16_t address, uint8_t out[2]) {
  return transact(makeReadEepromCommand(address), out, 2);
}

}  // namespace ft8x7

#include "rig_poller.h"

namespace ft8x7 {

namespace {

// El medidor (S-meter o PO) se refresca el doble que la frecuencia.
enum : uint8_t { kFreq, kTx, kRx };
constexpr uint8_t kSchedule[] = {kFreq, kTx, kRx, kTx, kRx};
constexpr uint8_t kScheduleLen = sizeof(kSchedule) / sizeof(kSchedule[0]);

// Comandos con valor: uno nuevo del mismo tipo deja obsoleto al pendiente.
bool isCoalescable(uint8_t opcode) {
  switch (static_cast<Opcode>(opcode)) {
    case Opcode::SetFrequency:
    case Opcode::SetMode:
    case Opcode::ClarifierOffset:
    case Opcode::RepeaterShift:
    case Opcode::RepeaterOffset:
    case Opcode::ToneMode:
    case Opcode::CtcssTone:
    case Opcode::DcsCode:
      return true;
    default:
      return false;
  }
}

}  // namespace

bool RigPoller::enqueue(const Command& cmd) {
  std::lock_guard<std::mutex> lock(queueMutex_);
  uint8_t op = cmd.bytes[4];
  if (isCoalescable(op)) {
    for (size_t i = 0; i < queueLen_; ++i) {
      if (queue_[i].bytes[4] == op) {
        queue_[i] = cmd;
        return true;
      }
    }
  }
  if (queueLen_ == kQueueSize) {
    return false;
  }
  queue_[queueLen_++] = cmd;
  return true;
}

bool RigPoller::popWrite(Command& cmd) {
  std::lock_guard<std::mutex> lock(queueMutex_);
  if (queueLen_ == 0) {
    return false;
  }
  cmd = queue_[0];
  for (size_t i = 1; i < queueLen_; ++i) {
    queue_[i - 1] = queue_[i];
  }
  --queueLen_;
  return true;
}

// Refleja en el estado lo que se acaba de pedir sin esperar a la siguiente lectura,
// para que la sintonía responda al instante aunque las escrituras retrasen el sondeo.
void RigPoller::applyOptimistic(const Command& cmd) {
  uint32_t hz;
  if (decodeSetFrequency(cmd, hz)) {
    state_.freq.hz = hz;
  } else if (cmd.bytes[4] == static_cast<uint8_t>(Opcode::SetMode)) {
    state_.freq.mode = decodeMode(cmd.bytes[0]);
    state_.freq.rawMode = cmd.bytes[0];
    state_.freq.narrow = false;
  }
}

void RigPoller::setExtras(const PollExtras& extras) {
  extras_ = extras;
  state_.haveVfo = state_.haveSplit = state_.haveMeters = false;
  state_.eepromUnsupported = state_.metersUnsupported = false;
  eepromErrors_ = meterErrors_ = 0;
  eepromDue_ = extras.eeprom;
}

bool RigPoller::wantEepromRead(uint32_t nowMs) const {
  // Sólo en RX con enlace, y nunca justo después de una escritura: primero se relee la frecuencia.
  return extras_.eeprom && !state_.eepromUnsupported && state_.linked && !state_.tx.transmitting &&
         slot_ != 0 && (eepromDue_ || nowMs - lastEepromMs_ >= kExtrasEveryMs);
}

// Las lecturas extra no cuentan para el enlace: si fallan mientras las básicas responden, la radio
// no las admite (algunos FT-857 no responden a 0xBB) y se dejan de pedir.
void RigPoller::recordExtra(Query q, CatResult r) {
  bool eeprom = q == Query::Vfo || q == Query::Split;
  uint8_t& errors = eeprom ? eepromErrors_ : meterErrors_;
  if (r == CatResult::Ok) {
    errors = 0;
    return;
  }
  if (++errors >= kExtraGiveUpErrors) {
    if (eeprom) {
      state_.eepromUnsupported = true;
      state_.haveVfo = state_.haveSplit = false;
    } else {
      state_.metersUnsupported = true;
      state_.haveMeters = false;
    }
  }
}

bool RigPoller::step(uint32_t nowMs) {
  uint32_t gap = state_.linked ? gapMs_ : retryGapMs_;
  if (started_ && nowMs - lastCmdMs_ < gap) {
    return false;
  }

  Command write;
  if (popWrite(write)) {
    cat_.send(write);
    ++state_.writeCount;
    if (state_.haveFreq) {
      applyOptimistic(write);
    }
    slot_ = 0;  // releer frecuencia y modo justo después
    eepromDue_ = extras_.eeprom;  // y VFO/split (A/B o SPLIT cambian lo que hay en EEPROM)
    started_ = true;
    lastCmdMs_ = nowMs;
    return true;
  }

  if (wantEepromRead(nowMs)) {
    Query q = nextIsSplit_ ? Query::Split : Query::Vfo;
    CatResult r = run(q);
    recordExtra(q, r);
    nextIsSplit_ = !nextIsSplit_;
    if (!nextIsSplit_) {
      eepromDue_ = false;  // leídos los dos
      lastEepromMs_ = nowMs;
    }
    started_ = true;
    lastCmdMs_ = nowMs;
    return true;
  }

  Query q;
  switch (kSchedule[slot_]) {
    case kFreq: q = Query::FreqMode; break;
    case kTx: q = Query::TxStatus; break;
    default:
      // En TX el estado RX no es válido: aprovechar el hueco para los medidores (o PO/SWR).
      if (!state_.tx.transmitting) {
        q = Query::RxStatus;
      } else if (extras_.txMeters && !state_.metersUnsupported) {
        q = Query::TxMeters;
      } else {
        q = Query::TxStatus;
      }
      break;
  }
  slot_ = (slot_ + 1) % kScheduleLen;

  CatResult r = run(q);
  if (q == Query::TxMeters) {
    recordExtra(q, r);
  } else {
    recordResult(r, nowMs);
    if (r != CatResult::Ok) {
      eepromErrors_ = meterErrors_ = 0;  // falla el enlace, no se puede culpar a las extras
    }
  }
  started_ = true;
  lastCmdMs_ = nowMs;
  return true;
}

CatResult RigPoller::run(Query q) {
  switch (q) {
    case Query::FreqMode: {
      FreqMode fm;
      CatResult r = cat_.readFreqMode(fm);
      if (r == CatResult::Ok) {
        state_.freq = fm;
        state_.haveFreq = true;
      }
      return r;
    }
    case Query::TxStatus: {
      TxStatus tx;
      CatResult r = cat_.readTxStatus(tx);
      if (r == CatResult::Ok) {
        state_.tx = tx;
        if (!tx.transmitting) {
          state_.haveMeters = false;
        }
      }
      return r;
    }
    case Query::TxMeters: {
      TxMeters m;
      CatResult r = cat_.readTxMeters(m);
      if (r == CatResult::Ok) {
        state_.meters = m;
        state_.haveMeters = true;
      }
      return r;
    }
    case Query::Vfo:
    case Query::Split: {
      bool vfo = q == Query::Vfo;
      uint8_t b;
      CatResult r = cat_.readEepromByte(vfo ? extras_.layout.vfoAddr : extras_.layout.splitAddr, b);
      if (r == CatResult::Ok) {
        if (vfo) {
          state_.vfoB = (b & 0x01) != 0;
          state_.haveVfo = true;
        } else {
          state_.split = (b & 0x80) != 0;
          state_.haveSplit = true;
        }
      }
      return r;
    }
    case Query::RxStatus: {
      RxStatus rx;
      CatResult r = cat_.readRxStatus(rx);
      if (r == CatResult::Ok) {
        state_.rx = rx;
      }
      return r;
    }
  }
  return CatResult::BadData;
}

void RigPoller::recordResult(CatResult r, uint32_t nowMs) {
  if (r == CatResult::Ok) {
    ++state_.okCount;
    state_.lastOkMs = nowMs;
    state_.linked = true;
    consecutiveErrors_ = 0;
    return;
  }
  ++state_.errorCount;
  if (consecutiveErrors_ < kLinkLossErrors) {
    ++consecutiveErrors_;
  }
  if (consecutiveErrors_ >= kLinkLossErrors) {
    state_.linked = false;
  }
}

}  // namespace ft8x7

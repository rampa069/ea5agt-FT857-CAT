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
    started_ = true;
    lastCmdMs_ = nowMs;
    return true;
  }

  Query q;
  switch (kSchedule[slot_]) {
    case kFreq: q = Query::FreqMode; break;
    case kTx: q = Query::TxStatus; break;
    default:
      // En TX el estado RX no es válido: aprovechar el hueco para refrescar PO/SWR.
      q = state_.tx.transmitting ? Query::TxStatus : Query::RxStatus;
      break;
  }
  slot_ = (slot_ + 1) % kScheduleLen;

  recordResult(run(q), nowMs);
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

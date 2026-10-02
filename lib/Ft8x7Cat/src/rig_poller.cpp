#include "rig_poller.h"

namespace ft8x7 {

namespace {

// El medidor (S-meter o PO) se refresca el doble que la frecuencia.
enum : uint8_t { kFreq, kTx, kRx };
constexpr uint8_t kSchedule[] = {kFreq, kTx, kRx, kTx, kRx};
constexpr uint8_t kScheduleLen = sizeof(kSchedule) / sizeof(kSchedule[0]);

}  // namespace

bool RigPoller::step(uint32_t nowMs) {
  uint32_t gap = state_.linked ? gapMs_ : retryGapMs_;
  if (started_ && nowMs - lastCmdMs_ < gap) {
    return false;
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

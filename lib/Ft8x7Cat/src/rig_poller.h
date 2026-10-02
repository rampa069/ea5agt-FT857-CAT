#pragma once

// Sondeo cíclico de la radio: alterna lectura de frecuencia/modo y estados RX/TX
// y mantiene un RigState con el último valor bueno de cada cosa.

#include <mutex>

#include "ft8x7_cat.h"

namespace ft8x7 {

struct RigState {
  bool linked = false;  // false tras kLinkLossErrors fallos seguidos
  bool haveFreq = false;
  FreqMode freq{};
  RxStatus rx{};
  TxStatus tx{};
  uint32_t okCount = 0;
  uint32_t errorCount = 0;
  uint32_t writeCount = 0;
  uint32_t lastOkMs = 0;
};

class RigPoller {
 public:
  static constexpr uint8_t kLinkLossErrors = 3;
  static constexpr size_t kQueueSize = 8;

  // gapMs: pausa entre comandos con enlace; retryGapMs: pausa sin enlace (no saturar la radio).
  explicit RigPoller(Ft8x7Cat& cat, uint32_t gapMs = 20, uint32_t retryGapMs = 500)
      : cat_(cat), gapMs_(gapMs), retryGapMs_(retryGapMs) {}

  // Ejecuta como mucho un comando CAT si ha pasado la pausa; devuelve true si lo ejecutó.
  // Bloquea hasta el timeout del Ft8x7Cat: llamar desde una tarea dedicada, no desde la UI.
  bool step(uint32_t nowMs);

  const RigState& state() const { return state_; }

  // Encola un comando de escritura (seguro desde otra tarea). Tiene prioridad sobre el sondeo.
  // Si ya hay pendiente uno del mismo tipo con valor (frecuencia, modo, tono...), lo sustituye:
  // al mantener pulsada la sintonía sólo se envía la última frecuencia. False si la cola está llena.
  bool enqueue(const Command& cmd);

 private:
  enum class Query : uint8_t { FreqMode, TxStatus, RxStatus };

  CatResult run(Query q);
  bool popWrite(Command& cmd);
  void applyOptimistic(const Command& cmd);
  void recordResult(CatResult r, uint32_t nowMs);

  Ft8x7Cat& cat_;
  uint32_t gapMs_;
  uint32_t retryGapMs_;
  RigState state_;
  uint8_t consecutiveErrors_ = 0;
  uint8_t slot_ = 0;
  bool started_ = false;
  uint32_t lastCmdMs_ = 0;

  std::mutex queueMutex_;
  Command queue_[kQueueSize];
  size_t queueLen_ = 0;
};

}  // namespace ft8x7

#include "bt_link.h"

#include <string.h>

#include <algorithm>
#include <atomic>

namespace {

constexpr uint32_t kScanMs = 10000;
constexpr uint32_t kReconnectEveryMs = 15000;
constexpr int kFallbackChannel = 1;  // si el equipo no anuncia SPP por SDP (p. ej. un servidor RFCOMM en Linux)

BtLink* gLink = nullptr;  // los callbacks de BluetoothSerial son funciones sin contexto
std::atomic<uint32_t> gConfirmCode{0};

}  // namespace

BtLink::BtLink() : port_(bt_) { gLink = this; }

// ---------------------------------------------------------------------------------------------
// Puerto CAT sobre SPP

void BtLink::Port::write(const uint8_t* data, size_t len) {
  if (bt_.connected()) {
    bt_.write(data, len);
  }
}

size_t BtLink::Port::read(uint8_t* data, size_t len, uint32_t timeoutMs) {
  if (!bt_.connected()) {
    return 0;  // sin enlace: fallar enseguida para no frenar la tarea CAT
  }
  size_t got = 0;
  uint32_t start = millis();
  while (got < len && millis() - start < timeoutMs) {
    int c = bt_.read();
    if (c >= 0) {
      data[got++] = static_cast<uint8_t>(c);
    } else {
      delay(1);
    }
  }
  return got;
}

void BtLink::Port::discardInput() {
  while (bt_.available() > 0) {
    bt_.read();
  }
}

// ---------------------------------------------------------------------------------------------
// Ciclo de vida (tarea CAT)

void BtLink::start(const char* localName) {
  if (started_) {
    return;
  }
  bt_.enableSSP();
  // Emparejamiento moderno (comparación numérica): el display acepta y muestra el código
  // para que el usuario lo confirme en el otro equipo.
  bt_.onConfirmRequest([](uint32_t code) {
    gConfirmCode = code;
    if (gLink) gLink->bt_.confirmReply(true);
  });
  bt_.onAuthComplete([](boolean) { gConfirmCode = 0; });
  started_ = bt_.begin(localName, true);
  attempted_ = false;
  setState(started_ ? rigui::BtState::Idle : rigui::BtState::Failed);
}

void BtLink::stop() {
  if (!started_) {
    return;
  }
  bt_.disconnect();
  bt_.end();
  started_ = false;
  setState(rigui::BtState::Off);
}

void BtLink::setState(rigui::BtState s) {
  std::lock_guard<std::mutex> lock(mutex_);
  status_.state = s;
}

void BtLink::service(bool haveDevice, const rigui::BtDevice& device, const char* pin, uint32_t nowMs) {
  if (!started_) {
    return;
  }

  bool scan, connect, forget;
  rigui::BtDevice target;
  uint8_t forgetAddr[6];
  {
    std::lock_guard<std::mutex> lock(mutex_);
    scan = wantScan_;
    connect = wantConnect_;
    forget = wantForget_;
    target = connectTarget_;
    memcpy(forgetAddr, forgetAddr_, 6);
    wantScan_ = wantConnect_ = wantForget_ = false;
    status_.haveDevice = haveDevice;
    if (haveDevice) status_.device = device;
  }

  if (forget) {
    bt_.disconnect();
    bt_.unpairDevice(forgetAddr);
    setState(rigui::BtState::Idle);
    return;
  }
  if (scan) {
    if (bt_.connected()) bt_.disconnect();
    doScan();
    attempted_ = true;  // dar tiempo a elegir dispositivo antes de reconectar con el guardado
    lastAttemptMs_ = millis();
    return;
  }
  if (connect) {
    if (doConnect(target, pin)) {
      std::lock_guard<std::mutex> lock(mutex_);
      newlyPaired_ = true;
      newlyPairedDevice_ = target;
    }
    attempted_ = true;
    lastAttemptMs_ = millis();
    return;
  }

  if (bt_.connected()) {
    setState(rigui::BtState::Connected);
    return;
  }
  // Reconexión automática con el dispositivo guardado.
  if (!haveDevice) {
    setState(rigui::BtState::Idle);
    return;
  }
  if (attempted_ && nowMs - lastAttemptMs_ < kReconnectEveryMs) {
    setState(rigui::BtState::Failed);  // se cayó o falló: esperar al siguiente intento
  }
  if (!attempted_ || nowMs - lastAttemptMs_ >= kReconnectEveryMs) {
    doConnect(device, pin);
    attempted_ = true;
    lastAttemptMs_ = millis();
  }
}

void BtLink::doScan() {
  setState(rigui::BtState::Scanning);
  BTScanResults* r = bt_.discover(kScanMs);
  rigui::BtDevice found[kMaxResults];
  size_t n = 0;
  int count = r ? r->getCount() : 0;
  for (int i = 0; i < count && n < kMaxResults; ++i) {
    BTAdvertisedDevice* d = r->getDevice(i);
    rigui::BtDevice dev{};
    memcpy(dev.addr, *d->getAddress().getNative(), 6);
    if (d->haveName()) {
      snprintf(dev.name, sizeof(dev.name), "%s", d->getName().c_str());
    }
    dev.rssi = d->haveRSSI() ? d->getRSSI() : -127;
    found[n++] = dev;
  }
  // Primero los que tienen nombre y mejor señal.
  std::sort(found, found + n, [](const rigui::BtDevice& a, const rigui::BtDevice& b) {
    bool an = a.name[0] != '\0', bn = b.name[0] != '\0';
    if (an != bn) return an;
    return a.rssi > b.rssi;
  });
  std::lock_guard<std::mutex> lock(mutex_);
  memcpy(results_, found, sizeof(found[0]) * n);
  resultCount_ = n;
  ++status_.scanSerial;
  status_.state = rigui::BtState::Idle;
}

bool BtLink::doConnect(const rigui::BtDevice& device, const char* pin) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    status_.state = rigui::BtState::Connecting;
    status_.device = device;
    status_.haveDevice = true;
  }
  bt_.setPin(pin);  // PIN clásico de los HC-05/HC-06
  uint8_t addr[6];
  memcpy(addr, device.addr, 6);
  // Primero buscando el servicio SPP por SDP (adaptadores reales); si no, canal fijo.
  // connected() cubre el caso visto en pruebas: el emparejamiento acaba justo después del timeout.
  bool ok = bt_.connect(addr) || bt_.connected() || bt_.connect(addr, kFallbackChannel);
  gConfirmCode = 0;
  setState(ok ? rigui::BtState::Connected : rigui::BtState::Failed);
  return ok;
}

// ---------------------------------------------------------------------------------------------
// Peticiones y estado (cualquier tarea)

void BtLink::requestScan() {
  std::lock_guard<std::mutex> lock(mutex_);
  wantScan_ = true;
}

void BtLink::requestConnect(const rigui::BtDevice& device) {
  std::lock_guard<std::mutex> lock(mutex_);
  wantConnect_ = true;
  connectTarget_ = device;
}

void BtLink::requestForget(const uint8_t addr[6]) {
  std::lock_guard<std::mutex> lock(mutex_);
  wantForget_ = true;
  memcpy(forgetAddr_, addr, 6);
}

rigui::BtStatus BtLink::status() const {
  std::lock_guard<std::mutex> lock(mutex_);
  rigui::BtStatus s = status_;
  s.confirmCode = gConfirmCode;
  return s;
}

size_t BtLink::results(rigui::BtDevice* out, size_t max) const {
  std::lock_guard<std::mutex> lock(mutex_);
  size_t n = std::min(max, resultCount_);
  memcpy(out, results_, sizeof(results_[0]) * n);
  return n;
}

bool BtLink::takeNewlyPaired(rigui::BtDevice& out) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!newlyPaired_) return false;
  newlyPaired_ = false;
  out = newlyPairedDevice_;
  return true;
}

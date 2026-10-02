#!/usr/bin/env python3
"""Simulador CAT de Yaesu FT-817/818/857/897.

Responde por un puerto serie (8N2) como lo haría la radio, para desarrollar el
firmware del display sin el equipo conectado. Uso típico con un adaptador
USB-TTL de 3.3V conectado al conector CN1 de la CYD:

    python3 tools/ft8x7_sim.py /dev/cu.usbserial-XXXX --baud 4800 -v

Sin hardware, --pty crea un puerto virtual y muestra su ruta:

    python3 tools/ft8x7_sim.py --pty -v
"""

import argparse
import os
import random
import sys
import threading
import time

FRAME_LEN = 5
# La radio descarta una trama incompleta si no recibe los 5 bytes en 200 ms.
FRAME_TIMEOUT_S = 0.2

MODES = {
    "LSB": 0x00, "USB": 0x01, "CW": 0x02, "CWR": 0x03, "AM": 0x04,
    "WFM": 0x06, "FM": 0x08, "DIG": 0x0A, "PKT": 0x0C,
}
MODE_NAMES = {v: k for k, v in MODES.items()}
NARROW_BIT = 0x80

# Escenario de demostración: (frecuencia Hz, modo)
DEMO_CHANNELS = [
    (7_074_000, "USB"),
    (14_074_000, "USB"),
    (7_030_000, "CW"),
    (3_650_000, "LSB"),
    (145_500_000, "FM"),
    (433_500_000, "FM"),
    (28_500_000, "AM"),
    (14_080_000, "DIG"),
]


def to_bcd(hz):
    """Hz -> 4 bytes BCD en pasos de 10 Hz (8 dígitos)."""
    digits = f"{hz // 10:08d}"
    if len(digits) > 8:
        raise ValueError(f"frecuencia fuera de rango: {hz}")
    return bytes(int(digits[i:i + 2], 16) for i in range(0, 8, 2))


def from_bcd(data):
    return int(data.hex(), 10) * 10


class Radio:
    """Estado de la radio y respuesta a cada trama CAT."""

    def __init__(self, model="857", ack=False):
        self.model = model
        self.ack = ack  # algunas radios confirman cada escritura con un byte 0x00
        self.lock = threading.Lock()
        self.vfo = {"A": [14_074_000, MODES["USB"]], "B": [7_074_000, MODES["LSB"]]}
        self.active = "A"
        self.narrow = False
        self.s_meter = 3
        self.squelched = False
        self.tone_mismatch = False
        self.disc_off_center = False
        self.ptt = False
        self.po = 0
        self.high_swr = False
        self.split = False
        self.locked = False
        self.clar_on = False
        self.clar_hz = 0
        self.rpt_shift = "SIMPLEX"
        self.rpt_offset_hz = 600_000
        self.tone_mode = "OFF"
        self.ctcss = "88.5"
        self.dcs = "023"
        self.eeprom = bytearray(0x1926)

    @property
    def freq(self):
        return self.vfo[self.active][0]

    @freq.setter
    def freq(self, hz):
        self.vfo[self.active][0] = hz

    @property
    def mode(self):
        return self.vfo[self.active][1]

    @mode.setter
    def mode(self, code):
        self.vfo[self.active][1] = code

    def rx_status(self):
        b = self.s_meter & 0x0F
        if self.disc_off_center:
            b |= 0x20
        if self.tone_mismatch:
            b |= 0x40
        if self.squelched:
            b |= 0x80
        return b

    def tx_status(self):
        if not self.ptt:
            return 0xFF
        b = self.po & 0x0F
        if not self.split:  # manual FT-817: bit 5 a 0 = split ON
            b |= 0x20
        if self.high_swr:
            b |= 0x40
        return b  # bit 7 a 0 = transmitiendo

    def handle(self, frame):
        """Procesa una trama de 5 bytes y devuelve la respuesta (b'' si no hay)."""
        reply = self._handle(frame)
        if reply is None:  # escritura aplicada
            return b"\x00" if self.ack else b""
        return reply

    def _handle(self, frame):
        p, op = frame[:4], frame[4]
        with self.lock:
            if op == 0x03:
                mode = self.mode | (NARROW_BIT if self.narrow and self.model in ("857", "897") else 0)
                return to_bcd(self.freq) + bytes([mode])
            if op == 0xE7:
                return bytes([self.rx_status()])
            if op == 0xF7:
                return bytes([self.tx_status()])
            if op == 0xBD and self.model in ("817", "818"):
                # Medidores TX (no documentado, sólo 817/818): [PWR<<4 | ALC] [SWR<<4 | MOD]
                if not self.ptt:
                    return b"\x00\x00"
                swr = 12 if self.high_swr else 2
                return bytes([(self.po << 4) | 3, (swr << 4) | 5])
            if op == 0xBB:
                addr = ((p[0] << 8) | p[1]) & 0xFFFE
                return bytes(self.eeprom[addr:addr + 2]).ljust(2, b"\x00")
            if op == 0xBC:
                return b""  # escritura EEPROM: se ignora a propósito
            if op in (0x08, 0x88):
                want = op == 0x08
                already = self.ptt == want
                self.ptt = want
                if not want:
                    self.po = 0
                    self.high_swr = False
                return b"\xF0" if already else b"\x00"
            if op == 0x01:
                self.freq = from_bcd(p)
                return None
            if op == 0x07:
                self.mode = p[0] & 0x7F
                return None
            if op == 0x81:
                self.active = "B" if self.active == "A" else "A"
                return None
            if op in (0x02, 0x82):
                self.split = op == 0x02
                return None
            if op in (0x05, 0x85):
                self.clar_on = op == 0x05
                return None
            if op == 0xF5:
                self.clar_hz = int(p[2:4].hex()) * 10 * (-1 if p[0] else 1)
                return None
            if op == 0x09:
                self.rpt_shift = {0x09: "-", 0x49: "+", 0x89: "SIMPLEX"}.get(p[0], self.rpt_shift)
                return None
            if op == 0xF9:
                self.rpt_offset_hz = int(p.hex())
                return None
            if op == 0x0A:
                self.tone_mode = {0x0A: "DCS", 0x2A: "TSQ", 0x4A: "ENC", 0x8A: "OFF"}.get(p[0], self.tone_mode)
                return None
            if op == 0x0B:
                self.ctcss = f"{int(p[:2].hex()) / 10:.1f}"
                return None
            if op == 0x0C:
                self.dcs = p[:2].hex()[1:]
                return None
            if op in (0x00, 0x80):
                self.locked = op == 0x00
                return None
        return b""

    def describe(self):
        mode = MODE_NAMES.get(self.mode, f"0x{self.mode:02X}")
        state = f"TX PO={self.po}{' SWR!' if self.high_swr else ''}" if self.ptt else f"RX S={self.s_meter}"
        extras = []
        if self.split:
            extras.append("SPLIT")
        if self.clar_on or self.clar_hz:
            extras.append(f"CLAR{'' if self.clar_on else '(off)'} {self.clar_hz:+d}Hz")
        if self.rpt_shift != "SIMPLEX":
            extras.append(f"RPT{self.rpt_shift}{self.rpt_offset_hz / 1e6:.3f}")
        if self.tone_mode != "OFF":
            extras.append(f"{self.tone_mode} {self.dcs if self.tone_mode == 'DCS' else self.ctcss}")
        if self.locked:
            extras.append("LOCK")
        tail = (" " + " ".join(extras)) if extras else ""
        return f"VFO-{self.active} {self.freq / 1e6:.5f} MHz {mode}{'-N' if self.narrow else ''} {state}{tail}"


class Scenario(threading.Thread):
    """Mueve el estado de la radio para que el display tenga algo que mostrar."""

    def __init__(self, radio, verbose):
        super().__init__(daemon=True)
        self.radio = radio
        self.verbose = verbose
        self.stop = threading.Event()

    def run(self):
        tick = 0
        channel = 0
        while not self.stop.wait(0.25):
            tick += 1
            r = self.radio
            with r.lock:
                if tick % 40 == 0:  # cada 10 s, cambio de canal
                    channel = (channel + 1) % len(DEMO_CHANNELS)
                    hz, mode = DEMO_CHANNELS[channel]
                    r.freq, r.mode = hz, MODES[mode]
                    r.narrow = mode == "CW" and channel % 2 == 0
                elif tick % 4 == 0:  # pequeño desplazamiento de sintonía
                    r.freq += random.choice((-100, -10, 0, 10, 100))

                cycle = tick % 80  # TX 4 s cada 20 s
                r.ptt = 40 <= cycle < 56
                if r.ptt:
                    r.po = min(15, max(0, r.po + random.choice((-1, 0, 1, 2))))
                    r.high_swr = r.mode == MODES["FM"] and cycle > 50
                else:
                    r.po = 0
                    r.high_swr = False
                    r.s_meter = min(15, max(0, r.s_meter + random.choice((-2, -1, 0, 1, 2))))
                    r.squelched = r.mode == MODES["FM"] and r.s_meter < 2
            if self.verbose and tick % 8 == 0:
                print(f"[estado] {r.describe()}", flush=True)


def serve(stream_read, stream_write, radio, verbose=False, drop_rate=0.0, stop=None):
    """Bucle principal: agrupa bytes en tramas de 5 y responde."""
    buf = bytearray()
    last = time.monotonic()
    while stop is None or not stop.is_set():
        data = stream_read()
        now = time.monotonic()
        if not data:
            continue
        if buf and now - last > FRAME_TIMEOUT_S:
            if verbose:
                print(f"[descarta] trama incompleta {buf.hex(' ')}", flush=True)
            buf.clear()
        last = now
        buf.extend(data)
        while len(buf) >= FRAME_LEN:
            frame, buf = bytes(buf[:FRAME_LEN]), buf[FRAME_LEN:]
            reply = radio.handle(frame)
            dropped = reply and random.random() < drop_rate
            if verbose:
                tag = " (PERDIDA)" if dropped else ""
                print(f"<- {frame.hex(' ')}  -> {reply.hex(' ') or '-'}{tag}", flush=True)
            if reply and not dropped:
                stream_write(reply)


def open_pty(baud):
    import termios
    import tty

    master, slave = os.openpty()
    tty.setraw(master)
    attrs = termios.tcgetattr(slave)
    speed = getattr(termios, f"B{baud}", termios.B4800)
    attrs[4] = attrs[5] = speed
    termios.tcsetattr(slave, termios.TCSANOW, attrs)
    return master, os.ttyname(slave), slave


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", nargs="?", help="puerto serie (p.ej. /dev/cu.usbserial-1410)")
    ap.add_argument("--pty", action="store_true", help="crear un puerto virtual en vez de usar uno real")
    ap.add_argument("--baud", type=int, default=4800, choices=(4800, 9600, 38400))
    ap.add_argument("--model", default="857", choices=("817", "818", "857", "897"))
    ap.add_argument("--static", action="store_true", help="no animar el estado (frecuencia fija, sin TX)")
    ap.add_argument("--ack", action="store_true", help="confirmar cada escritura con un byte 0x00")
    ap.add_argument("--drop-rate", type=float, default=0.0, help="fracción de respuestas que se pierden (0..1)")
    ap.add_argument("-v", "--verbose", action="store_true", help="mostrar cada trama")
    args = ap.parse_args(argv)

    if not args.port and not args.pty:
        ap.error("indica un puerto o usa --pty")

    radio = Radio(args.model, ack=args.ack)
    if not args.static:
        Scenario(radio, args.verbose).start()

    print(f"Simulador FT-{args.model} a {args.baud} baudios 8N2. Ctrl+C para salir.", flush=True)
    try:
        if args.pty:
            master, name, _slave = open_pty(args.baud)
            print(f"Puerto virtual: {name}", flush=True)
            serve(lambda: os.read(master, 64), lambda b: os.write(master, b), radio, args.verbose, args.drop_rate)
        else:
            import serial

            ser = serial.Serial(None, args.baud, bytesize=8, parity="N", stopbits=2, timeout=0.05)
            ser.port = args.port
            # DTR/RTS desactivados: en placas ESP32 con auto-reset, activarlos reinicia la placa.
            ser.dtr = False
            ser.rts = False
            with ser:
                serve(lambda: ser.read(64), ser.write, radio, args.verbose, args.drop_rate)
    except KeyboardInterrupt:
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())

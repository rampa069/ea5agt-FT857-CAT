"""Tests del simulador: python3 -m unittest discover -s tools"""

import os
import threading
import time
import unittest

import ft8x7_sim as sim


class RadioTest(unittest.TestCase):
    def setUp(self):
        self.r = sim.Radio("857")

    def test_bcd_roundtrip(self):
        self.assertEqual(sim.to_bcd(439_700_000), bytes([0x43, 0x97, 0x00, 0x00]))
        self.assertEqual(sim.to_bcd(14_234_560), bytes([0x01, 0x42, 0x34, 0x56]))
        self.assertEqual(sim.from_bcd(bytes([0x01, 0x42, 0x34, 0x56])), 14_234_560)

    def test_read_freq_mode(self):
        self.r.freq, self.r.mode = 7_030_000, sim.MODES["CW"]
        self.assertEqual(self.r.handle(bytes([0, 0, 0, 0, 0x03])), bytes([0x00, 0x70, 0x30, 0x00, 0x02]))

    def test_narrow_bit_only_on_857_897(self):
        self.r.mode, self.r.narrow = sim.MODES["CW"], True
        self.assertEqual(self.r.handle(bytes([0, 0, 0, 0, 0x03]))[4], 0x82)
        r817 = sim.Radio("817")
        r817.mode, r817.narrow = sim.MODES["CW"], True
        self.assertEqual(r817.handle(bytes([0, 0, 0, 0, 0x03]))[4], 0x02)

    def test_rx_status(self):
        self.r.s_meter, self.r.squelched = 9, True
        self.assertEqual(self.r.handle(bytes([0, 0, 0, 0, 0xE7])), bytes([0x89]))

    def test_tx_status_rx_is_ff(self):
        self.assertEqual(self.r.handle(bytes([0, 0, 0, 0, 0xF7])), bytes([0xFF]))

    def test_tx_status_transmitting(self):
        self.r.ptt, self.r.po, self.r.high_swr = True, 7, True
        self.assertEqual(self.r.handle(bytes([0, 0, 0, 0, 0xF7])), bytes([0x67]))
        self.r.split = True
        self.assertEqual(self.r.handle(bytes([0, 0, 0, 0, 0xF7])), bytes([0x47]))

    def test_tx_metering_817_818_only(self):
        r = sim.Radio("818")
        r.ptt, r.po = True, 9
        self.assertEqual(r.handle(bytes([0, 0, 0, 0, 0xBD])), bytes([0x93, 0x25]))
        self.r.ptt = True
        self.assertEqual(self.r.handle(bytes([0, 0, 0, 0, 0xBD])), b"")

    def test_ptt_reply(self):
        self.assertEqual(self.r.handle(bytes([0, 0, 0, 0, 0x08])), b"\x00")
        self.assertEqual(self.r.handle(bytes([0, 0, 0, 0, 0x08])), b"\xF0")
        self.assertEqual(self.r.handle(bytes([0, 0, 0, 0, 0x88])), b"\x00")

    def test_set_commands(self):
        self.assertEqual(self.r.handle(bytes([0x43, 0x97, 0x00, 0x00, 0x01])), b"")
        self.assertEqual(self.r.freq, 439_700_000)
        self.r.handle(bytes([0x08, 0, 0, 0, 0x07]))
        self.assertEqual(self.r.mode, sim.MODES["FM"])
        self.r.handle(bytes([0, 0, 0, 0, 0x81]))
        self.assertEqual(self.r.active, "B")

    def test_clarifier_repeater_tones(self):
        r = self.r
        self.assertEqual(r.handle(bytes([0, 0, 0, 0, 0x05])), b"")
        self.assertTrue(r.clar_on)
        r.handle(bytes([0x01, 0x00, 0x01, 0x23, 0xF5]))
        self.assertEqual(r.clar_hz, -1230)
        r.handle(bytes([0x49, 0, 0, 0, 0x09]))
        self.assertEqual(r.rpt_shift, "+")
        r.handle(bytes([0x05, 0x43, 0x21, 0x00, 0xF9]))
        self.assertEqual(r.rpt_offset_hz, 5_432_100)
        r.handle(bytes([0x2A, 0, 0, 0, 0x0A]))
        r.handle(bytes([0x08, 0x85, 0, 0, 0x0B]))
        r.handle(bytes([0x00, 0x23, 0, 0, 0x0C]))
        self.assertEqual((r.tone_mode, r.ctcss, r.dcs), ("TSQ", "88.5", "023"))

    def test_ack_option(self):
        r = sim.Radio("857", ack=True)
        self.assertEqual(r.handle(bytes([0, 0, 0, 0, 0x81])), b"\x00")
        self.assertEqual(r.handle(bytes([0, 0, 0, 0, 0xE7]))[0] & 0x0F, r.s_meter)  # lecturas sin cambios

    def test_eeprom_read_and_write_ignored(self):
        self.r.eeprom[0x78:0x7A] = b"\x12\x34"
        self.assertEqual(self.r.handle(bytes([0x00, 0x79, 0, 0, 0xBB])), b"\x12\x34")
        self.assertEqual(self.r.handle(bytes([0x00, 0x78, 0xAA, 0xBB, 0xBC])), b"")
        self.assertEqual(bytes(self.r.eeprom[0x78:0x7A]), b"\x12\x34")


class PtyEndToEndTest(unittest.TestCase):
    """El cliente pyserial hace de ESP32 contra el simulador en un pseudo-terminal."""

    def test_round_trip_over_pty(self):
        import serial

        radio = sim.Radio("817")
        radio.freq, radio.mode = 145_500_000, sim.MODES["FM"]
        master, name, slave = sim.open_pty(4800)
        threading.Thread(
            target=sim.serve,
            args=(lambda: os.read(master, 64), lambda b: os.write(master, b), radio),
            daemon=True,
        ).start()

        with serial.Serial(name, 4800, stopbits=2, timeout=0.5) as ser:
            ser.write(bytes([0, 0, 0, 0, 0x03]))
            self.assertEqual(ser.read(5), bytes([0x14, 0x55, 0x00, 0x00, 0x08]))

            # Trama incompleta + pausa > 200 ms: la radio la descarta y se resincroniza.
            ser.write(bytes([0, 0]))
            time.sleep(0.3)
            ser.write(bytes([0, 0, 0, 0, 0xE7]))
            self.assertEqual(len(ser.read(1)), 1)
            self.assertEqual(ser.read(1), b"")
        os.close(slave)


if __name__ == "__main__":
    unittest.main()

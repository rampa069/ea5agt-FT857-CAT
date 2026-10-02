#!/usr/bin/env python3
"""Prepara la web de instalación (ESP Web Tools) con los firmwares ya compilados.

Para cada entorno de PlatformIO copia bootloader, particiones, boot_app0 y firmware y escribe un
manifiesto con sus direcciones. Se graban por separado (no una imagen única desde 0x0) para no
pisar la NVS: actualizar conserva ajustes, calibración y emparejamiento Bluetooth. Además deja
una imagen única <env>-completo.bin para grabarla a mano con esptool. Uso, tras `pio run`:

    python3 tools/release/build_web.py [--out web] [--version v1.2.3]
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Entornos publicados en la web: (entorno, título, descripción)
VARIANTS = [
    ("cyd2usb", "CYD con micro-USB + USB-C", "Pantalla ILI9341, colores invertidos de fábrica."),
    ("cyd", "CYD con un solo micro-USB", "Pantalla ILI9341, colores normales."),
    ("cyd-st7789", "CYD con pantalla ST7789", "Si con las otras la imagen sale desplazada o con ruido."),
    ("cyd-usbcat", "Pruebas sin radio (CAT por el USB)",
     "El CAT va por el propio cable USB para usar el simulador del ordenador. No conectar a una radio."),
]


def esptool_command():
    """esptool instalado con pip, o el que trae PlatformIO (con su entorno)."""
    try:
        import esptool  # noqa: F401
        return [sys.executable, "-m", "esptool"]
    except ImportError:
        pass
    if shutil.which("pio"):
        return ["pio", "pkg", "exec", "-p", "tool-esptoolpy", "--", "esptool.py"]
    sys.exit("No encuentro esptool: pip install esptool")


def boot_app0():
    candidates = [
        Path.home() / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin",
    ]
    for c in candidates:
        if c.exists():
            return c
    sys.exit("No encuentro boot_app0.bin (¿se compiló con PlatformIO?)")


def git_version():
    try:
        return subprocess.check_output(["git", "describe", "--tags", "--always", "--dirty"], cwd=ROOT,
                                       text=True).strip()
    except Exception:
        return "dev"


def merge(esptool, env, out_bin):
    build = ROOT / ".pio/build" / env
    parts = {"bootloader.bin": "0x1000", "partitions.bin": "0x8000", "firmware.bin": "0x10000"}
    for name in parts:
        if not (build / name).exists():
            sys.exit(f"Falta {build / name}: ejecuta antes `pio run -e {env}`")
    subprocess.check_call(esptool + [
        "--chip", "esp32", "merge_bin", "-o", str(out_bin),
        "--flash_mode", "dio", "--flash_freq", "40m", "--flash_size", "4MB",
        "0x1000", str(build / "bootloader.bin"),
        "0x8000", str(build / "partitions.bin"),
        "0xe000", str(boot_app0()),
        "0x10000", str(build / "firmware.bin"),
    ], stdout=subprocess.DEVNULL)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "web"))
    ap.add_argument("--version", default=git_version())
    args = ap.parse_args()

    esptool = esptool_command()
    out = Path(args.out)
    fw_dir = out / "firmware"
    shutil.rmtree(fw_dir, ignore_errors=True)
    fw_dir.mkdir(parents=True)

    variants = []
    for env, title, desc in VARIANTS:
        env_dir = fw_dir / env
        env_dir.mkdir()
        build = ROOT / ".pio/build" / env
        parts = []
        for src, offset in ((build / "bootloader.bin", 0x1000), (build / "partitions.bin", 0x8000),
                            (boot_app0(), 0xE000), (build / "firmware.bin", 0x10000)):
            shutil.copy(src, env_dir / src.name)
            parts.append({"path": f"{env}/{src.name}", "offset": offset})
        merge(esptool, env, fw_dir / f"{env}-completo.bin")
        manifest = {
            "name": f"ea5agt FT-857 CAT - {title}",
            "version": args.version,
            "new_install_prompt_erase": True,
            "builds": [{"chipFamily": "ESP32", "parts": parts}],
        }
        (fw_dir / f"manifest-{env}.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False))
        variants.append({"env": env, "title": title, "description": desc})

    (fw_dir / "variants.json").write_text(
        json.dumps({"version": args.version, "variants": variants}, indent=2, ensure_ascii=False))
    print(f"Web preparada en {out} (versión {args.version})")


if __name__ == "__main__":
    main()

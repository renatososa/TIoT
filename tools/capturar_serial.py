#!/usr/bin/env python3
"""Captura un puerto serie en pantalla y en un archivo sin límite de líneas."""

from __future__ import annotations

import argparse
import sys
from datetime import datetime
from pathlib import Path

try:
    import serial
except ImportError as exc:
    raise SystemExit(
        "Falta pyserial. Instalarlo con: python -m pip install pyserial"
    ) from exc


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("port", help="Puerto serie, por ejemplo COM11")
    parser.add_argument("-b", "--baud", type=int, default=115200)
    parser.add_argument("-o", "--output", type=Path)
    parser.add_argument(
        "--stop-on-complete",
        action="store_true",
        help="Finaliza después de capturar 'Session complete'",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output = args.output
    if output is None:
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        output = Path("resultados") / f"preexperimento_{stamp}.txt"

    output.parent.mkdir(parents=True, exist_ok=True)
    marker = b"Session complete"
    recent = b""

    print(f"Capturando {args.port} a {args.baud} baud")
    print(f"Archivo: {output.resolve()}")
    print("Presionar Ctrl+C para finalizar.\n")

    try:
        with serial.Serial(
            args.port,
            args.baud,
            timeout=0.2,
            rtscts=False,
            dsrdtr=False,
        ) as port, output.open("wb") as log:
            port.dtr = False
            port.rts = False

            while True:
                chunk = port.read(port.in_waiting or 1)
                if not chunk:
                    continue

                log.write(chunk)
                log.flush()
                sys.stdout.write(chunk.decode("utf-8", errors="replace"))
                sys.stdout.flush()

                recent = (recent + chunk)[-max(256, len(marker)) :]
                if args.stop_on_complete and marker in recent:
                    print(f"\n\nCaptura completa: {output.resolve()}")
                    return 0
    except KeyboardInterrupt:
        print(f"\nCaptura detenida. Archivo guardado: {output.resolve()}")
        return 0
    except serial.SerialException as exc:
        print(f"Error al abrir o leer {args.port}: {exc}", file=sys.stderr)
        print("Cerrar Arduino IDE u otro monitor que esté usando el puerto.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

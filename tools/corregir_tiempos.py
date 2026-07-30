#!/usr/bin/env python3
"""Convierte timestamps del XIAO al reloj del central usando el registro C."""

from __future__ import annotations

import argparse
import csv
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


@dataclass(frozen=True)
class Calibration:
    xiao_start: int
    central_start: int
    xiao_end: int
    central_end: int

    @property
    def scale(self) -> float:
        """Factor por el que se multiplica un intervalo del XIAO."""
        return (self.central_end - self.central_start) / (
            self.xiao_end - self.xiao_start
        )

    @property
    def scale_drift_ppm(self) -> float:
        """Desviación del factor central/XIAO respecto de la unidad."""
        return (self.scale - 1.0) * 1_000_000.0

    @property
    def xiao_rate_error_ppm(self) -> float:
        """Cuánto más rápido avanza el XIAO respecto del reloj central."""
        return (
            (self.xiao_end - self.xiao_start)
            / (self.central_end - self.central_start)
            - 1.0
        ) * 1_000_000.0

    @property
    def offset_start_us(self) -> int:
        """Diferencia central - XIAO en el grupo inicial."""
        return self.central_start - self.xiao_start

    @property
    def offset_end_us(self) -> int:
        """Diferencia central - XIAO en el grupo final."""
        return self.central_end - self.xiao_end

    def convert(self, xiao_time: int) -> int:
        xiao_delta = self.xiao_end - self.xiao_start
        if xiao_delta <= 0:
            raise ValueError("intervalo de calibración inválido")

        numerator = (xiao_time - self.xiao_start) * (
            self.central_end - self.central_start
        )
        return self.central_start + round(numerator / xiao_delta)


def relevant_rows(lines: Iterable[str]) -> tuple[list[list[str]], Calibration]:
    packets: list[list[str]] = []
    calibration: Calibration | None = None

    for raw_line in lines:
        line = raw_line.strip()
        if line.startswith("D,"):
            fields = line.split(",")
            if len(fields) in (7, 8):
                packets.append(fields)
        elif line.startswith("C,"):
            fields = line.split(",")
            if len(fields) >= 5:
                calibration = Calibration(
                    xiao_start=int(fields[1]),
                    central_start=int(fields[2]),
                    xiao_end=int(fields[3]),
                    central_end=int(fields[4]),
                )

    if calibration is None:
        raise ValueError("no se encontró un registro de calibración C")
    if not packets:
        raise ValueError("no se encontraron registros de datos D")

    return packets, calibration


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Corrige timestamps y calcula latencia a partir del log central."
    )
    parser.add_argument("log", type=Path, help="archivo de texto capturado del central")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="CSV de salida; si se omite se escribe por stdout",
    )
    args = parser.parse_args()

    with args.log.open("r", encoding="utf-8", errors="replace") as stream:
        packets, calibration = relevant_rows(stream)

    if (
        calibration.xiao_end <= calibration.xiao_start
        or calibration.central_end <= calibration.central_start
    ):
        raise ValueError("intervalo de calibración inválido")

    packets_outside_session = [
        fields
        for fields in packets
        if not (
            calibration.xiao_start
            <= int(fields[3])
            <= calibration.xiao_end
        )
    ]
    if packets_outside_session:
        raise ValueError(
            f"el log contiene {len(packets_outside_session)} registros D "
            "fuera del intervalo de sincronización; limpiar el log antes "
            "de corregir los tiempos"
        )

    print(
        "Calibración temporal: "
        f"offset_inicial={calibration.offset_start_us} us, "
        f"offset_final={calibration.offset_end_us} us, "
        f"escala_central_por_xiao={calibration.scale:.12f}, "
        f"deriva_escala={calibration.scale_drift_ppm:.6f} ppm, "
        f"xiao_mas_rapido={calibration.xiao_rate_error_ppm:.6f} ppm",
        file=sys.stderr,
    )

    output_stream = (
        args.output.open("w", encoding="utf-8", newline="")
        if args.output
        else __import__("sys").stdout
    )

    try:
        writer = csv.writer(output_stream, lineterminator="\n")
        writer.writerow(
            [
                "config_id",
                "packet_id",
                "acquisition_time_xiao_us",
                "acquisition_time_central_us",
                "tx_time_xiao_us",
                "tx_time_central_us",
                "rx_time_us",
                "acquisition_latency_us",
                "transport_latency_us",
                "payload_bytes",
                "fragments",
            ]
        )

        for fields in packets:
            if len(fields) == 8:
                acquisition_xiao = int(fields[3])
                tx_xiao = int(fields[4])
                rx_central = int(fields[5])
                payload_bytes = fields[6]
                fragments = fields[7]
            else:
                # Compatibilidad con los ensayos preliminares sin timestamp
                # de adquisición independiente.
                acquisition_xiao = int(fields[3])
                tx_xiao = acquisition_xiao
                rx_central = int(fields[4])
                payload_bytes = fields[5]
                fragments = fields[6]

            acquisition_central = calibration.convert(acquisition_xiao)
            tx_central = calibration.convert(tx_xiao)
            writer.writerow(
                [
                    fields[1],
                    fields[2],
                    acquisition_xiao,
                    acquisition_central,
                    tx_xiao,
                    tx_central,
                    rx_central,
                    rx_central - acquisition_central,
                    rx_central - tx_central,
                    payload_bytes,
                    fragments,
                ]
            )
    finally:
        if args.output:
            output_stream.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Calcula capacidad BLE y utilización de las ventanas EMG."""

from __future__ import annotations

import argparse
import csv
import math
import statistics
from collections import defaultdict
from pathlib import Path


PAYLOAD_CONFIGS = {
    0xA1: (15, 1),
    0xA2: (30, 1),
    0xA3: (60, 1),
    0xA4: (120, 1),
    0xA5: (240, 2),
    0xA6: (480, 3),
    0xA7: (960, 5),
}

WINDOWS = [
    ("S1_E5", 3000, 5),
    ("S1_E10", 3000, 10),
    ("S1_E20", 3000, 20),
    ("S2_E5", 12000, 5),
    ("S2_E10", 12000, 10),
    ("S2_E20", 12000, 20),
    ("S3_E5", 48000, 5),
    ("S3_E10", 48000, 10),
    ("S3_E20", 48000, 20),
]


def percentile(values: list[int], quantile: float) -> float:
    ordered = sorted(values)
    position = (len(ordered) - 1) * quantile
    lower = int(position)
    upper = math.ceil(position)
    if lower == upper:
        return float(ordered[lower])
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (
        position - lower
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path, help="CSV producido por corregir_tiempos.py")
    parser.add_argument("--capacity-output", type=Path, required=True)
    parser.add_argument("--windows-output", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    groups: dict[int, list[dict[str, int]]] = defaultdict(list)

    with args.input.open(newline="", encoding="utf-8") as source:
        for row in csv.DictReader(source):
            parsed = {key: int(value, 0) for key, value in row.items()}
            groups[parsed["config_id"]].append(parsed)

    capacities: dict[int, dict[str, float | int]] = {}
    for config_id, expected in PAYLOAD_CONFIGS.items():
        rows = sorted(groups[config_id], key=lambda row: row["rx_time_us"])
        if len(rows) < 2:
            raise SystemExit(f"Datos insuficientes para config_id 0x{config_id:02x}")

        payload, fragments = expected
        if any(
            row["payload_bytes"] != payload or row["fragments"] != fragments
            for row in rows
        ):
            raise SystemExit(f"Formato inconsistente para config_id 0x{config_id:02x}")

        rx = [row["rx_time_us"] for row in rows]
        delta_rx = [rx[index] - rx[index - 1] for index in range(1, len(rx))]
        latency = [row["transport_latency_us"] for row in rows]
        packet_ids = [row["packet_id"] for row in rows]
        span_s = (rx[-1] - rx[0]) / 1_000_000
        capacity_pps = (len(rows) - 1) / span_s

        capacities[payload] = {
            "config_id": config_id,
            "payload_bytes": payload,
            "fragments": fragments,
            "packets_received": len(rows),
            "packet_id_first": min(packet_ids),
            "packet_id_last": max(packet_ids),
            "missing_packet_ids": (
                max(packet_ids) - min(packet_ids) + 1 - len(set(packet_ids))
            ),
            "rx_span_s": span_s,
            "capacity_pps": capacity_pps,
            "useful_throughput_kbps": capacity_pps * payload * 8 / 1000,
            "delta_rx_mean_us": statistics.mean(delta_rx),
            "delta_rx_median_us": statistics.median(delta_rx),
            "delta_rx_p95_us": percentile(delta_rx, 0.95),
            "delta_rx_max_us": max(delta_rx),
            "transport_latency_mean_us": statistics.mean(latency),
            "transport_latency_median_us": statistics.median(latency),
            "transport_latency_p95_us": percentile(latency, 0.95),
            "transport_latency_max_us": max(latency),
        }

    args.capacity_output.parent.mkdir(parents=True, exist_ok=True)
    capacity_fields = list(next(iter(capacities.values())).keys())
    with args.capacity_output.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=capacity_fields)
        writer.writeheader()
        writer.writerows(capacities.values())

    window_rows = []
    for name, rate_bytes_s, window_ms in WINDOWS:
        payload = rate_bytes_s * window_ms // 1000
        capacity = capacities[payload]
        required_pps = 1000 / window_ms
        utilization = required_pps / float(capacity["capacity_pps"])
        if utilization >= 1:
            status = "sobrecargada"
        elif utilization > 0.8:
            status = "sin_margen"
        else:
            status = "candidata"

        window_rows.append(
            {
                "profile": name,
                "rate_bytes_s": rate_bytes_s,
                "window_ms": window_ms,
                "payload_bytes": payload,
                "fragments": capacity["fragments"],
                "required_packets_s": required_pps,
                "measured_capacity_packets_s": capacity["capacity_pps"],
                "estimated_utilization": utilization,
                "status": status,
                "saturation_transport_p95_us": capacity[
                    "transport_latency_p95_us"
                ],
                "oldest_sample_latency_without_queue_p95_us": (
                    window_ms * 1000
                    + float(capacity["transport_latency_p95_us"])
                ),
            }
        )

    window_fields = list(window_rows[0].keys())
    with args.windows_output.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=window_fields)
        writer.writeheader()
        writer.writerows(window_rows)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

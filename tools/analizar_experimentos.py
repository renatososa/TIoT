#!/usr/bin/env python3
"""Resume los 11 perfiles del firmware post-preexperimento unificado."""

from __future__ import annotations

import argparse
import csv
import math
import statistics
from collections import defaultdict
from pathlib import Path


DURATION_S = 20
PROFILES = {
    0x13: ("S1-E20", 60, 20_000),
    0x21: ("S2-E5", 60, 5_000),
    0x22: ("S2-E10", 120, 10_000),
    0x23: ("S2-E20", 240, 20_000),
    0x31: ("S3-E5", 240, 5_000),
    0x32: ("S3-E10", 480, 10_000),
    0x33: ("S3-E20", 960, 20_000),
    0xB1: ("STRESS-960-E10", 960, 10_000),
    0xB2: ("STRESS-960-E8", 960, 8_000),
    0xB3: ("STRESS-960-E7", 960, 7_000),
    0xB4: ("STRESS-960-E6", 960, 6_000),
}


def percentile(values: list[int], fraction: float) -> int | str:
    if not values:
        return ""
    ordered = sorted(values)
    return ordered[max(0, math.ceil(fraction * len(ordered)) - 1)]


def mean(values: list[int]) -> float | str:
    return statistics.mean(values) if values else ""


def stdev(values: list[int]) -> float | str:
    return statistics.pstdev(values) if values else ""


def read_sender_stats(path: Path) -> dict[int, dict[str, int]]:
    names = (
        "generated_packets",
        "enqueued_packets",
        "transmitted_packets",
        "queue_dropped_packets",
        "stop_dropped_packets",
        "send_error_packets",
        "queue_high_watermark",
    )
    result: dict[int, dict[str, int]] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line.startswith("P,"):
            continue
        fields = line.strip().split(",")
        if len(fields) != 9:
            continue
        config_id = int(fields[1], 0)
        result[config_id] = {
            name: int(value) for name, value in zip(names, fields[2:])
        }
    return result


def analyze_profile(
    config_id: int,
    rows: list[dict[str, str]],
    sender: dict[str, int],
) -> dict[str, object]:
    name, payload, period_us = PROFILES[config_id]
    rows.sort(key=lambda row: int(row["packet_id"]))

    packet_ids = [int(row["packet_id"]) for row in rows]
    rx_times = [int(row["rx_time_us"]) for row in rows]
    acquisition_latencies = [
        int(row["acquisition_latency_us"]) for row in rows
    ]
    pre_tx_latencies = [
        int(row["tx_time_central_us"]) - int(row["acquisition_time_central_us"])
        for row in rows
    ]
    transport_latencies = [
        int(row["transport_latency_us"]) for row in rows
    ]
    delta_rx = [
        current - previous for previous, current in zip(rx_times, rx_times[1:])
    ]
    packet_id_delta = [
        current - previous
        for previous, current in zip(packet_ids, packet_ids[1:])
    ]
    jitter = [
        abs(delta - (identifier_delta * period_us))
        for delta, identifier_delta in zip(delta_rx, packet_id_delta)
    ]

    received = len(rows)
    generated = sender.get("generated_packets", "")
    nominal = DURATION_S * 1_000_000 // period_us
    internal_lost = (
        packet_ids[-1] - packet_ids[0] + 1 - len(set(packet_ids))
        if packet_ids
        else 0
    )
    useful_bytes = sum(int(row["payload_bytes"]) for row in rows)

    return {
        "order": list(PROFILES).index(config_id) + 1,
        "profile": name,
        "config_id": f"0x{config_id:02x}",
        "duration_s": DURATION_S,
        "period_ms": period_us / 1000,
        "payload_bytes": payload,
        "fragments": int(rows[0]["fragments"]) if rows else "",
        "nominal_packets": nominal,
        "generated_packets": generated,
        "received_packets": received,
        "delivery_pct": 100 * received / generated if generated else "",
        "loss_pct": (
            100 * (generated - received) / generated if generated else ""
        ),
        "nominal_generation_pct": 100 * generated / nominal if generated != "" else "",
        "internal_lost_packets": internal_lost,
        "enqueued_packets": sender.get("enqueued_packets", ""),
        "transmitted_packets": sender.get("transmitted_packets", ""),
        "queue_dropped_packets": sender.get("queue_dropped_packets", ""),
        "stop_dropped_packets": sender.get("stop_dropped_packets", ""),
        "send_error_packets": sender.get("send_error_packets", ""),
        "queue_high_watermark": sender.get("queue_high_watermark", ""),
        "throughput_effective_kbps": 8 * useful_bytes / DURATION_S / 1000,
        "delta_rx_mean_us": mean(delta_rx),
        "delta_rx_std_us": stdev(delta_rx),
        "jitter_abs_mean_us": mean(jitter),
        "jitter_abs_p95_us": percentile(jitter, 0.95),
        "jitter_abs_max_us": max(jitter) if jitter else "",
        "acquisition_latency_mean_us": mean(acquisition_latencies),
        "acquisition_latency_p95_us": percentile(acquisition_latencies, 0.95),
        "acquisition_latency_max_us": (
            max(acquisition_latencies) if acquisition_latencies else ""
        ),
        "pre_tx_latency_mean_us": mean(pre_tx_latencies),
        "pre_tx_latency_p95_us": percentile(pre_tx_latencies, 0.95),
        "pre_tx_latency_max_us": (
            max(pre_tx_latencies) if pre_tx_latencies else ""
        ),
        "transport_latency_mean_us": mean(transport_latencies),
        "transport_latency_p95_us": percentile(transport_latencies, 0.95),
        "transport_latency_max_us": (
            max(transport_latencies) if transport_latencies else ""
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Calcula métricas por perfil del experimento unificado."
    )
    parser.add_argument("csv", type=Path, help="CSV generado por corregir_tiempos.py")
    parser.add_argument("log", type=Path, help="log serie original con registros P")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("resultados/experimentos_metricas.csv"),
    )
    args = parser.parse_args()

    grouped: dict[int, list[dict[str, str]]] = defaultdict(list)
    with args.csv.open("r", encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream):
            grouped[int(row["config_id"], 0)].append(row)

    stats = read_sender_stats(args.log)
    unknown = sorted(set(grouped) - set(PROFILES))
    if unknown:
        values = ", ".join(f"0x{value:02x}" for value in unknown)
        raise SystemExit(f"El CSV contiene configuraciones desconocidas: {values}")

    results = [
        analyze_profile(config_id, grouped.get(config_id, []), stats.get(config_id, {}))
        for config_id in PROFILES
    ]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(results[0]))
        writer.writeheader()
        writer.writerows(results)

    missing_data = [row["profile"] for row in results if not row["received_packets"]]
    missing_stats = [row["profile"] for row in results if row["generated_packets"] == ""]
    print(f"{len(results)} perfiles -> {args.output}")
    if missing_data:
        print("ADVERTENCIA: sin paquetes recibidos: " + ", ".join(missing_data))
    if missing_stats:
        print("ADVERTENCIA: sin registro P: " + ", ".join(missing_stats))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

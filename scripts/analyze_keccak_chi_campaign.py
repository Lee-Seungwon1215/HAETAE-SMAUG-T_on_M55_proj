#!/usr/bin/env python3
"""Pool order-balanced STM32N657 Keccak Chi campaign samples."""

from __future__ import annotations

import argparse
import csv
import json
import math
import pathlib
import statistics
from collections import defaultdict


OPERATIONS = (
    "keccak_permutation_x4_mve",
    "shake128_x4_mve",
    "keypair",
    "sign",
    "verify",
)


def percentile(values: list[int], fraction: float) -> float:
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return float(ordered[lower])
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def distribution(values: list[int]) -> dict[str, int | float]:
    return {
        "n": len(values),
        "min": min(values),
        "p05": percentile(values, 0.05),
        "median": statistics.median(values),
        "mean": statistics.fmean(values),
        "p95": percentile(values, 0.95),
        "max": max(values),
        "stdev": statistics.stdev(values),
    }


def result_fields(line: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for item in next(csv.reader([line]))[1:]:
        if "=" in item:
            key, value = item.split("=", 1)
            fields[key] = value
    return fields


def write_csv(path: pathlib.Path, rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", type=pathlib.Path)
    args = parser.parse_args()
    campaign = args.campaign.resolve()

    pooled: dict[tuple[str, str, str], list[int]] = defaultdict(list)
    per_set: dict[tuple[str, str, str, str], list[int]] = defaultdict(list)
    for log in sorted(campaign.glob("runs/performance/set*/**/output.log")):
        descriptor = json.loads(log.with_name("run.json").read_text(encoding="utf-8"))
        group = descriptor["group"]
        set_name = group.rsplit("/", 1)[-1]
        config = descriptor["name"]
        algorithm = descriptor["algorithm"]
        for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
            if not line.startswith("RESULT,"):
                continue
            fields = result_fields(line)
            operation = fields.get("operation")
            if operation not in OPERATIONS:
                continue
            ticks = int(fields["ticks"])
            pooled[(config, algorithm, operation)].append(ticks)
            per_set[(set_name, config, algorithm, operation)].append(ticks)

    expected = 400
    if not pooled:
        raise SystemExit("no Keccak campaign samples found")
    bad = {key: len(values) for key, values in pooled.items() if len(values) != expected}
    if bad:
        raise SystemExit(f"expected {expected} pooled samples per cell, got {bad}")

    stats = {key: distribution(values) for key, values in pooled.items()}
    rows: list[dict[str, object]] = []
    for key in sorted(stats):
        config, algorithm, operation = key
        values = stats[key]
        default_median = float(stats[("hardware_default", algorithm, operation)]["median"])
        hand_median = float(stats[("hand_chi", algorithm, operation)]["median"])
        median = float(values["median"])
        rows.append({
            "config": config,
            "algorithm": algorithm,
            "operation": operation,
            **values,
            "speedup_vs_hardware_default": default_median / median,
            "change_pct_vs_hardware_default": (median / default_median - 1.0) * 100.0,
            "speedup_vs_hand_chi": hand_median / median,
            "change_pct_vs_hand_chi": (median / hand_median - 1.0) * 100.0,
        })

    set_rows: list[dict[str, object]] = []
    for key, values in sorted(per_set.items()):
        set_name, config, algorithm, operation = key
        set_rows.append({
            "set": set_name,
            "config": config,
            "algorithm": algorithm,
            "operation": operation,
            **distribution(values),
        })

    statistics_dir = campaign / "statistics"
    write_csv(statistics_dir / "keccak-chi-pooled.csv", rows)
    write_csv(statistics_dir / "keccak-chi-by-set.csv", set_rows)
    (statistics_dir / "keccak-chi-summary.json").write_text(
        json.dumps(rows, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()

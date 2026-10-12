#!/usr/bin/env python3
"""Analyze the physical four-backend STM32N657 final paper campaign."""

from __future__ import annotations

import argparse
import csv
import json
import math
import pathlib
import statistics


TOP_LEVEL = {
    "haetae2": ("keypair", "sign", "verify"),
    "haetae3": ("keypair", "sign", "verify"),
    "haetae5": ("keypair", "sign", "verify"),
    "smaugt1": ("keypair", "encapsulate", "decapsulate"),
    "smaugt3": ("keypair", "encapsulate", "decapsulate"),
    "smaugt5": ("keypair", "encapsulate", "decapsulate"),
}
CONFIGS = ("reference", "intrinsic", "assembly", "final")


def fields(line: str) -> dict[str, str]:
    return dict(
        part.split("=", 1)
        for part in line.strip().split(",")[1:]
        if "=" in part
    )


def parse_log(path: pathlib.Path) -> tuple[dict[str, list[int]], dict[str, int], str]:
    results: dict[str, list[int]] = {}
    stacks: dict[str, int] = {}
    text = path.read_text(encoding="utf-8", errors="replace")
    for line in text.splitlines():
        if line.startswith("RESULT,"):
            row = fields(line)
            results.setdefault(row["operation"], []).append(int(row["ticks"]))
        elif line.startswith("STACK,"):
            row = fields(line)
            stacks[row["operation"]] = int(row["peak_bytes"])
    return results, stacks, text


def parse_size(path: pathlib.Path) -> tuple[int, int, int]:
    lines = [line.split() for line in path.read_text().splitlines() if line.strip()]
    for parts in reversed(lines):
        if len(parts) >= 6 and all(part.isdigit() for part in parts[:4]):
            return int(parts[0]), int(parts[1]), int(parts[2])
    raise ValueError(f"cannot parse size output: {path}")


def quartiles(values: list[int]) -> tuple[float, float]:
    q1, _, q3 = statistics.quantiles(values, n=4, method="inclusive")
    return float(q1), float(q3)


def gmean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def write_csv(path: pathlib.Path, rows: list[dict[str, object]]) -> None:
    if not rows:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", type=pathlib.Path)
    args = parser.parse_args()
    campaign = args.campaign.resolve()
    manifest = json.loads(
        (campaign / "final-paper-campaign.json").read_text(encoding="utf-8")
    )
    expected = int(manifest["iterations_per_set"]) * int(manifest["sets"])

    pooled: dict[tuple[str, str, str], list[int]] = {}
    stack_sets: dict[tuple[str, str, str], set[int]] = {}
    text_sizes: dict[tuple[str, str], set[int]] = {}
    raw_rows: list[dict[str, object]] = []
    status_rows: list[dict[str, object]] = []

    performance_root = campaign / "runs" / "performance"
    for path in sorted(performance_root.glob("set*/*/*/*/output.log")):
        set_name, config, _implementation, algorithm, _ = path.relative_to(
            performance_root
        ).parts
        results, stacks, text = parse_log(path)
        status_rows.append(
            {
                "set": set_name,
                "config": config,
                "algorithm": algorithm,
                "selftest_ok": "SELFTEST,ok" in text,
                "status_ok": "STATUS,ok" in text,
            }
        )
        text_size, _, _ = parse_size(path.parent / "size-summary.txt")
        text_sizes.setdefault((algorithm, config), set()).add(text_size)
        for operation, values in results.items():
            pooled.setdefault((algorithm, operation, config), []).extend(values)
            for sample, ticks in enumerate(values, start=1):
                raw_rows.append(
                    {
                        "set": set_name,
                        "config": config,
                        "algorithm": algorithm,
                        "operation": operation,
                        "sample": sample,
                        "cycles": ticks,
                    }
                )
        for operation, peak in stacks.items():
            stack_sets.setdefault((algorithm, operation, config), set()).add(peak)

    top_rows: list[dict[str, object]] = []
    stack_rows: list[dict[str, object]] = []
    speedups_by_family: dict[str, dict[str, list[float]]] = {
        "haetae": {"assembly": [], "slothy": [], "total": [], "reference": []},
        "smaugt": {"assembly": [], "slothy": [], "total": [], "reference": []},
        "all": {"assembly": [], "slothy": [], "total": [], "reference": []},
    }
    for algorithm, operations in TOP_LEVEL.items():
        family = "haetae" if algorithm.startswith("haetae") else "smaugt"
        for operation in operations:
            summaries: dict[str, dict[str, float]] = {}
            for config in CONFIGS:
                values = pooled.get((algorithm, operation, config), [])
                if len(values) != expected:
                    raise SystemExit(
                        f"{algorithm}/{operation}/{config}: expected {expected} "
                        f"samples, found {len(values)}"
                    )
                q1, q3 = quartiles(values)
                summaries[config] = {
                    "median": float(statistics.median(values)),
                    "mean": statistics.fmean(values),
                    "q1": q1,
                    "q3": q3,
                    "stdev": statistics.stdev(values),
                }
            reference = summaries["reference"]["median"]
            intrinsic = summaries["intrinsic"]["median"]
            assembly = summaries["assembly"]["median"]
            final = summaries["final"]["median"]
            stage_speedups = {
                "assembly": intrinsic / assembly,
                "slothy": assembly / final,
                "total": intrinsic / final,
                "reference": reference / final,
            }
            for target in (family, "all"):
                for name, value in stage_speedups.items():
                    speedups_by_family[target][name].append(value)
            row: dict[str, object] = {
                "algorithm": algorithm,
                "operation": operation,
                "samples_per_config": expected,
            }
            for config in CONFIGS:
                summary = summaries[config]
                row[f"{config}_median"] = round(summary["median"], 1)
                row[f"{config}_q1"] = round(summary["q1"], 1)
                row[f"{config}_q3"] = round(summary["q3"], 1)
                row[f"{config}_mean"] = round(summary["mean"], 3)
                row[f"{config}_stdev"] = round(summary["stdev"], 3)
            row.update(
                {
                    "assembly_speedup_vs_intrinsic": round(stage_speedups["assembly"], 5),
                    "slothy_speedup_vs_assembly": round(stage_speedups["slothy"], 5),
                    "final_speedup_vs_intrinsic": round(stage_speedups["total"], 5),
                    "final_speedup_vs_reference": round(stage_speedups["reference"], 5),
                }
            )
            top_rows.append(row)

            stack_row: dict[str, object] = {
                "algorithm": algorithm,
                "operation": operation,
            }
            for config in CONFIGS:
                observed = stack_sets[(algorithm, operation, config)]
                stack_row[f"{config}_peak_bytes"] = max(observed)
                stack_row[f"{config}_observed_values"] = ";".join(
                    str(value) for value in sorted(observed)
                )
            intrinsic_stack = int(stack_row["intrinsic_peak_bytes"])
            for config in ("reference", "assembly", "final"):
                value = int(stack_row[f"{config}_peak_bytes"])
                stack_row[f"{config}_delta_vs_intrinsic_bytes"] = value - intrinsic_stack
                stack_row[f"{config}_delta_vs_intrinsic_percent"] = round(
                    (value / intrinsic_stack - 1.0) * 100.0, 3
                )
            stack_rows.append(stack_row)

    geomean_rows: list[dict[str, object]] = []
    for family, values in speedups_by_family.items():
        geomean_rows.append(
            {
                "scope": family,
                "cells": len(values["total"]),
                "assembly_speedup_vs_intrinsic": round(gmean(values["assembly"]), 5),
                "slothy_speedup_vs_assembly": round(gmean(values["slothy"]), 5),
                "final_speedup_vs_intrinsic": round(gmean(values["total"]), 5),
                "final_speedup_vs_reference": round(gmean(values["reference"]), 5),
            }
        )

    size_rows: list[dict[str, object]] = []
    for algorithm in TOP_LEVEL:
        row: dict[str, object] = {"algorithm": algorithm}
        for config in CONFIGS:
            sizes = text_sizes[(algorithm, config)]
            if len(sizes) != 1:
                raise SystemExit(f"non-constant text size: {algorithm}/{config}: {sizes}")
            row[f"{config}_text_bytes"] = next(iter(sizes))
        intrinsic_size = int(row["intrinsic_text_bytes"])
        for config in ("reference", "assembly", "final"):
            value = int(row[f"{config}_text_bytes"])
            row[f"{config}_delta_vs_intrinsic_bytes"] = value - intrinsic_size
            row[f"{config}_delta_vs_intrinsic_percent"] = round(
                (value / intrinsic_size - 1.0) * 100.0, 3
            )
        size_rows.append(row)

    correctness_rows: list[dict[str, object]] = []
    correctness_root = campaign / "runs" / "correctness"
    for config in CONFIGS:
        implementation = manifest["configs"][config]["implementation"]
        for algorithm in TOP_LEVEL:
            path = correctness_root / config / implementation / algorithm / "output.log"
            if not path.is_file():
                raise SystemExit(f"missing correctness log: {path}")
            text = path.read_text(encoding="utf-8", errors="replace")
            correctness_rows.append(
                {
                    "config": config,
                    "algorithm": algorithm,
                    "selftest_ok": "SELFTEST,ok" in text,
                    "kat_ok": None if config == "reference" else "KAT,status=ok" in text,
                    "diff_1000_ok": None
                    if config == "reference"
                    else "DIFF,iterations=1000,status=ok" in text,
                    "status_ok": "STATUS,ok" in text,
                }
            )

    statistics_dir = campaign / "statistics"
    write_csv(statistics_dir / "raw-cycles.csv", raw_rows)
    write_csv(statistics_dir / "top-level-performance.csv", top_rows)
    write_csv(statistics_dir / "geomean-speedups.csv", geomean_rows)
    write_csv(statistics_dir / "stack-comparison.csv", stack_rows)
    write_csv(statistics_dir / "code-size-comparison.csv", size_rows)
    write_csv(statistics_dir / "run-status.csv", status_rows)
    write_csv(statistics_dir / "correctness.csv", correctness_rows)
    print(f"Wrote final paper statistics under {statistics_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

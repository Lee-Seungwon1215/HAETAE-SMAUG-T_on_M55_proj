#!/usr/bin/env python3
"""Analyze pooled results from stm32n657_final_keccak_campaign.py."""

from __future__ import annotations

import argparse
import csv
import json
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
CONFIGS = ("reference", "m4_scalar", "m55_sched_mve_x4")
MICRO_OPERATIONS = (
    "keccak_permutation",
    "keccak_permutation_x4_sequential",
    "keccak_permutation_x4_mve",
)


def fields(line: str) -> dict[str, str]:
    return dict(part.split("=", 1) for part in line.strip().split(",")[1:] if "=" in part)


def result_values(path: pathlib.Path) -> dict[str, list[int]]:
    values: dict[str, list[int]] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line.startswith("RESULT,"):
            continue
        row = fields(line)
        values.setdefault(row["operation"], []).append(int(row["ticks"]))
    return values


def stack_values(path: pathlib.Path) -> dict[str, int]:
    values: dict[str, int] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("STACK,"):
            row = fields(line)
            values[row["operation"]] = int(row["peak_bytes"])
    return values


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", type=pathlib.Path)
    args = parser.parse_args()
    campaign = args.campaign.resolve()
    manifest = json.loads((campaign / "final-campaign.json").read_text(encoding="utf-8"))
    expected_per_config = int(manifest["iterations_per_set"]) * int(manifest["sets"])

    pooled: dict[tuple[str, str, str], list[int]] = {}
    per_set: dict[tuple[str, str, str, str], list[int]] = {}
    micro_pooled: dict[tuple[str, str], list[int]] = {}
    stacks: dict[tuple[str, str, str], set[int]] = {}
    status_rows: list[dict[str, object]] = []
    for path in sorted((campaign / "runs" / "performance").glob("set*/*/*/*/output.log")):
        relative = path.relative_to(campaign / "runs" / "performance").parts
        set_name, config, _implementation, algorithm, _ = relative
        text = path.read_text(encoding="utf-8", errors="replace")
        status_rows.append({
            "set": set_name,
            "config": config,
            "algorithm": algorithm,
            "selftest_ok": "SELFTEST,ok" in text,
            "status_ok": "STATUS,ok" in text,
        })
        results = result_values(path)
        stack = stack_values(path)
        for operation in TOP_LEVEL[algorithm]:
            pooled.setdefault((algorithm, operation, config), []).extend(results[operation])
            per_set[(set_name, algorithm, operation, config)] = results[operation]
            stacks.setdefault((algorithm, operation, config), set()).add(stack[operation])
        if config != "reference":
            for operation in MICRO_OPERATIONS:
                micro_pooled.setdefault((config, operation), []).extend(results[operation])

    correctness_rows: list[dict[str, object]] = []
    correctness_root = campaign / "runs" / "correctness"
    for algorithm in TOP_LEVEL:
        candidates: list[pathlib.Path] = []
        for implementation in ("optimization", "m55"):
            candidates.extend(
                correctness_root.glob(
                    f"m55_sched_mve_x4/{implementation}/{algorithm}/output.log"
                )
            )
        if not candidates:
            raise SystemExit(f"missing correctness log for {algorithm}")
        text = candidates[0].read_text(encoding="utf-8", errors="replace")
        correctness_rows.append({
            "algorithm": algorithm,
            "selftest_ok": "SELFTEST,ok" in text,
            "kat_ok": "KAT,status=ok" in text,
            "diff_1000_ok": "DIFF,iterations=1000,status=ok" in text,
            "status_ok": "STATUS,ok" in text,
        })

    rows: list[dict[str, object]] = []
    for algorithm, operations in TOP_LEVEL.items():
        for operation in operations:
            summaries: dict[str, dict[str, float]] = {}
            for config in CONFIGS:
                values = pooled.get((algorithm, operation, config), [])
                if len(values) != expected_per_config:
                    raise SystemExit(
                        f"{algorithm}/{operation}/{config}: expected {expected_per_config} "
                        f"samples, found {len(values)}"
                    )
                summaries[config] = {
                    "median": float(statistics.median(values)),
                    "mean": float(statistics.fmean(values)),
                    "stdev": float(statistics.stdev(values)),
                }
            reference = summaries["reference"]
            m4 = summaries["m4_scalar"]
            final = summaries["m55_sched_mve_x4"]
            final_stacks = stacks[(algorithm, operation, "m55_sched_mve_x4")]
            if len(final_stacks) != 1:
                raise SystemExit(f"non-constant stack: {algorithm}/{operation}: {final_stacks}")
            rows.append({
                "algorithm": algorithm,
                "operation": operation,
                "samples_per_config": expected_per_config,
                "reference_median": int(reference["median"]),
                "m4_scalar_median": int(m4["median"]),
                "final_median": int(final["median"]),
                "reference_mean": round(reference["mean"], 3),
                "m4_scalar_mean": round(m4["mean"], 3),
                "final_mean": round(final["mean"], 3),
                "final_stdev": round(final["stdev"], 3),
                "final_vs_reference_percent": round((final["median"] / reference["median"] - 1) * 100, 3),
                "final_vs_reference_speedup": round(reference["median"] / final["median"], 4),
                "final_vs_m4_percent": round((final["median"] / m4["median"] - 1) * 100, 3),
                "final_vs_m4_speedup": round(m4["median"] / final["median"], 4),
                "final_stack_bytes": next(iter(final_stacks)),
            })

    set_rows: list[dict[str, object]] = []
    for set_name in ("set1", "set2", "set3"):
        for algorithm, operations in TOP_LEVEL.items():
            for operation in operations:
                medians = {
                    config: float(statistics.median(
                        per_set[(set_name, algorithm, operation, config)]
                    ))
                    for config in CONFIGS
                }
                set_rows.append({
                    "set": set_name,
                    "algorithm": algorithm,
                    "operation": operation,
                    "reference_median": int(medians["reference"]),
                    "m4_scalar_median": int(medians["m4_scalar"]),
                    "final_median": int(medians["m55_sched_mve_x4"]),
                    "final_vs_reference_percent": round(
                        (medians["m55_sched_mve_x4"] / medians["reference"] - 1) * 100, 3
                    ),
                    "final_vs_m4_percent": round(
                        (medians["m55_sched_mve_x4"] / medians["m4_scalar"] - 1) * 100, 3
                    ),
                })

    expected_micro = expected_per_config * len(TOP_LEVEL)
    for values in micro_pooled.values():
        if len(values) != expected_micro:
            raise SystemExit(
                f"Keccak microbenchmark: expected {expected_micro} samples, found {len(values)}"
            )
    m4_single = statistics.median(micro_pooled[("m4_scalar", "keccak_permutation")])
    final_single = statistics.median(
        micro_pooled[("m55_sched_mve_x4", "keccak_permutation")]
    )
    final_sequential = statistics.median(
        micro_pooled[("m55_sched_mve_x4", "keccak_permutation_x4_sequential")]
    )
    final_mve = statistics.median(
        micro_pooled[("m55_sched_mve_x4", "keccak_permutation_x4_mve")]
    )
    micro_rows = [
        {
            "comparison": "single_m55_sched_vs_m4_scalar",
            "baseline_median": int(m4_single),
            "optimized_median": int(final_single),
            "change_percent": round((final_single / m4_single - 1) * 100, 3),
            "speedup": round(m4_single / final_single, 4),
            "samples_each": expected_micro,
        },
        {
            "comparison": "four_state_mve_vs_four_sequential_m55_sched",
            "baseline_median": int(final_sequential),
            "optimized_median": int(final_mve),
            "change_percent": round((final_mve / final_sequential - 1) * 100, 3),
            "speedup": round(final_sequential / final_mve, 4),
            "samples_each": expected_micro,
        },
    ]

    statistics_dir = campaign / "statistics"
    statistics_dir.mkdir(parents=True, exist_ok=True)
    with (statistics_dir / "final-performance.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    with (statistics_dir / "run-status.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(status_rows[0]))
        writer.writeheader()
        writer.writerows(status_rows)
    with (statistics_dir / "correctness.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(correctness_rows[0]))
        writer.writeheader()
        writer.writerows(correctness_rows)
    with (statistics_dir / "per-set-performance.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(set_rows[0]))
        writer.writeheader()
        writer.writerows(set_rows)
    with (statistics_dir / "keccak-microbench.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(micro_rows[0]))
        writer.writeheader()
        writer.writerows(micro_rows)

    print(f"Wrote {statistics_dir / 'final-performance.csv'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Recompute physical-board statistics from archived line-oriented samples."""

from __future__ import annotations

import argparse
import csv
import json
import math
import pathlib
import statistics
import sys
from collections import defaultdict


TOP_LEVEL = {
    "haetae2": {"keypair", "sign", "verify"},
    "haetae3": {"keypair", "sign", "verify"},
    "haetae5": {"keypair", "sign", "verify"},
    "smaugt1": {"keypair", "encapsulate", "decapsulate"},
    "smaugt3": {"keypair", "encapsulate", "decapsulate"},
    "smaugt5": {"keypair", "encapsulate", "decapsulate"},
}

PROFILE_CATEGORIES = (
    "multiplication", "keccak", "sampler", "encoding", "packing",
    "fft_fixpoint", "other",
)

REFERENCE_LABELS = ("reference", "clean")
OPTIMIZATION_LABELS = ("optimization", "m55")


def parse_line(line: str) -> tuple[str, dict[str, str]] | None:
    if "," not in line:
        return None
    row = next(csv.reader([line]))
    fields: dict[str, str] = {}
    for item in row[1:]:
        if "=" in item:
            key, value = item.split("=", 1)
            fields[key] = value
    return row[0], fields


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
        "stdev": statistics.stdev(values) if len(values) > 1 else 0.0,
    }


def write_csv(path: pathlib.Path, fieldnames: list[str], rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", type=pathlib.Path)
    parser.add_argument(
        "--baseline", type=pathlib.Path,
        help="campaign containing deployment reference/optimization baseline statistics",
    )
    arguments = parser.parse_args()
    campaign = arguments.campaign.resolve()
    samples: dict[tuple[str, ...], list[int]] = defaultdict(list)
    candidates: dict[tuple[str, ...], list[tuple[int, int]]] = defaultdict(list)
    profiles: dict[tuple[str, ...], list[int]] = defaultdict(list)
    stacks: list[dict[str, object]] = []
    whole_run_stacks: list[dict[str, object]] = []
    sections: list[dict[str, object]] = []
    image_sizes: list[dict[str, object]] = []
    run_statuses: list[dict[str, object]] = []

    for log in sorted(campaign.glob("runs/**/output.log")):
        descriptor_path = log.with_name("run.json")
        if not descriptor_path.is_file():
            continue
        descriptor = json.loads(descriptor_path.read_text(encoding="utf-8"))
        prefix = (
            descriptor["group"], descriptor["name"], descriptor["implementation"],
            descriptor["algorithm"],
        )
        log_text = log.read_text(encoding="utf-8", errors="replace")
        run_statuses.append({
            "group": prefix[0], "name": prefix[1],
            "implementation": prefix[2], "algorithm": prefix[3],
            "exit_code": descriptor.get("exit_code", ""),
            "descriptor_status_ok": int(descriptor.get("status_ok") is True),
            "selftest_ok": int("SELFTEST,ok" in log_text),
            "kat_ok": int("KAT,status=ok" in log_text),
            "diff_1000_ok": int("DIFF,iterations=1000,status=ok" in log_text),
            "target_status_ok": int("STATUS,ok" in log_text),
        })
        for line in log_text.splitlines():
            parsed = parse_line(line)
            if parsed is None:
                continue
            kind, fields = parsed
            if kind == "RESULT" and "operation" in fields and "ticks" in fields:
                samples[prefix + (fields["operation"],)].append(int(fields["ticks"]))
            elif kind == "CANDIDATES" and "operation" in fields:
                candidates[prefix + (fields["operation"],)].append(
                    (int(fields["attempts"]), int(fields["rejections"]))
                )
            elif kind == "PROFILE" and "operation" in fields:
                profile_prefix = prefix + (fields["operation"],)
                total = 0
                for category in PROFILE_CATEGORIES:
                    ticks = int(fields[category])
                    profiles[profile_prefix + (category,)].append(ticks)
                    total += ticks
                profiles[profile_prefix + ("total",)].append(total)
            elif kind == "STACK" and "operation" in fields:
                stacks.append({
                    "group": prefix[0], "name": prefix[1],
                    "implementation": prefix[2], "algorithm": prefix[3],
                    "operation": fields["operation"],
                    "peak_bytes": int(fields["peak_bytes"]),
                })
            elif kind == "MEMORY" and "stack_peak_bytes" in fields:
                whole_run_stacks.append({
                    "group": prefix[0], "name": prefix[1],
                    "implementation": prefix[2], "algorithm": prefix[3],
                    "stack_peak_bytes": int(fields["stack_peak_bytes"]),
                })

        sections_path = log.with_name("sections.txt")
        section_bytes: dict[str, int] = {}
        if sections_path.is_file():
            for line in sections_path.read_text(encoding="utf-8", errors="replace").splitlines():
                columns = line.split()
                if len(columns) < 3 or not columns[0].startswith("."):
                    continue
                try:
                    size = int(columns[1], 0)
                    address = int(columns[2], 0)
                except ValueError:
                    continue
                section_bytes[columns[0]] = size
                sections.append({
                    "group": prefix[0], "name": prefix[1],
                    "implementation": prefix[2], "algorithm": prefix[3],
                    "section": columns[0], "bytes": size,
                    "address": f"0x{address:08x}",
                })

        size_path = log.with_name("size-summary.txt")
        if size_path.is_file():
            lines = size_path.read_text(encoding="utf-8", errors="replace").splitlines()
            for line in lines[1:]:
                columns = line.split()
                if len(columns) < 6:
                    continue
                try:
                    text_bytes, data_bytes, bss_bytes, total_bytes = (
                        int(columns[index], 0) for index in range(4)
                    )
                except ValueError:
                    continue
                image_sizes.append({
                    "group": prefix[0], "name": prefix[1],
                    "implementation": prefix[2], "algorithm": prefix[3],
                    "text_bytes": text_bytes, "data_bytes": data_bytes,
                    "bss_bytes": bss_bytes, "total_bytes": total_bytes,
                    "output_transport_bytes": section_bytes.get(".bench_output", 0),
                    "runtime_bytes_excluding_output_transport": (
                        total_bytes - section_bytes.get(".bench_output", 0)
                    ),
                })
                break

    rows: list[dict[str, object]] = []
    for key, values in sorted(samples.items()):
        group, name, implementation, algorithm, operation = key
        row = {
            "group": group, "name": name, "implementation": implementation,
            "algorithm": algorithm, "operation": operation,
            "top_level": int(operation in TOP_LEVEL.get(algorithm, set())),
        }
        row.update(distribution(values))
        rows.append(row)
    summary_fields = [
        "group", "name", "implementation", "algorithm", "operation", "top_level",
        "n", "min", "p05", "median", "mean", "p95", "max", "stdev",
    ]
    write_csv(campaign / "statistics" / "cycles.csv", summary_fields, rows)

    rejection_rows: list[dict[str, object]] = []
    for key, values in sorted(candidates.items()):
        attempts = [value[0] for value in values]
        rejections = [value[1] for value in values]
        attempts_total = sum(attempts)
        rejection_rows.append({
            "group": key[0], "name": key[1], "implementation": key[2],
            "algorithm": key[3], "operation": key[4], "n": len(values),
            "attempts_mean": statistics.fmean(attempts),
            "attempts_median": statistics.median(attempts),
            "attempts_min": min(attempts), "attempts_max": max(attempts),
            "attempts_total": attempts_total,
            "rejections_total": sum(rejections),
            "rejection_rate": (sum(rejections) / attempts_total
                               if attempts_total else 0.0),
        })
    rejection_fields = [
        "group", "name", "implementation", "algorithm", "operation", "n",
        "attempts_mean", "attempts_median", "attempts_min", "attempts_max",
        "attempts_total", "rejections_total", "rejection_rate",
    ]
    write_csv(campaign / "statistics" / "rejections.csv", rejection_fields,
              rejection_rows)
    stack_fields = ["group", "name", "implementation", "algorithm", "operation", "peak_bytes"]
    write_csv(campaign / "statistics" / "stack.csv", stack_fields, stacks)
    whole_stack_fields = [
        "group", "name", "implementation", "algorithm", "stack_peak_bytes",
    ]
    write_csv(campaign / "statistics" / "whole-run-stack.csv", whole_stack_fields,
              whole_run_stacks)

    profile_rows: list[dict[str, object]] = []
    profile_totals = {
        key[:-1]: values for key, values in profiles.items() if key[-1] == "total"
    }
    for key, values in sorted(profiles.items()):
        group, name, implementation, algorithm, operation, category = key
        row = {
            "group": group, "name": name, "implementation": implementation,
            "algorithm": algorithm, "operation": operation, "category": category,
        }
        row.update(distribution(values))
        totals = profile_totals.get(key[:-1], [])
        total_ticks = sum(totals)
        row["share"] = sum(values) / total_ticks if total_ticks else 0.0
        profile_rows.append(row)
    profile_fields = [
        "group", "name", "implementation", "algorithm", "operation", "category",
        "n", "min", "p05", "median", "mean", "p95", "max", "stdev", "share",
    ]
    write_csv(campaign / "statistics" / "profiles.csv", profile_fields, profile_rows)

    section_fields = [
        "group", "name", "implementation", "algorithm", "section", "bytes", "address",
    ]
    write_csv(campaign / "statistics" / "sections.csv", section_fields, sections)
    image_size_fields = [
        "group", "name", "implementation", "algorithm", "text_bytes", "data_bytes",
        "bss_bytes", "total_bytes", "output_transport_bytes",
        "runtime_bytes_excluding_output_transport",
    ]
    write_csv(campaign / "statistics" / "image-size.csv", image_size_fields, image_sizes)
    run_status_fields = [
        "group", "name", "implementation", "algorithm", "exit_code",
        "descriptor_status_ok", "selftest_ok", "kat_ok", "diff_1000_ok",
        "target_status_ok",
    ]
    write_csv(campaign / "statistics" / "run-status.csv", run_status_fields,
              run_statuses)

    baseline = {
        (row["algorithm"], row["operation"], row["implementation"]): row
        for row in rows
        if row["group"] == "baseline" and row["name"] == "deployment"
        and row["top_level"] == 1
    }
    speedups: list[dict[str, object]] = []
    for algorithm, operations in TOP_LEVEL.items():
        for operation in sorted(operations):
            reference = next(
                (baseline.get((algorithm, operation, label))
                 for label in REFERENCE_LABELS
                 if baseline.get((algorithm, operation, label)) is not None),
                None,
            )
            optimization = next(
                (baseline.get((algorithm, operation, label))
                 for label in OPTIMIZATION_LABELS
                 if baseline.get((algorithm, operation, label)) is not None),
                None,
            )
            if reference and optimization:
                speedups.append({
                    "algorithm": algorithm, "operation": operation,
                    "clean_median": reference["median"],
                    "m55_median": optimization["median"],
                    "median_speedup": (
                        float(reference["median"]) / float(optimization["median"])
                    ),
                    "clean_mean": reference["mean"],
                    "m55_mean": optimization["mean"],
                    "mean_speedup": (
                        float(reference["mean"]) / float(optimization["mean"])
                    ),
                })
    speedup_fields = [
        "algorithm", "operation", "clean_median", "m55_median", "median_speedup",
        "clean_mean", "m55_mean", "mean_speedup",
    ]
    write_csv(campaign / "statistics" / "baseline-speedups.csv", speedup_fields, speedups)

    m55_baseline = {
        (row["algorithm"], row["operation"]): row
        for row in rows
        if row["group"] == "baseline" and row["name"] == "deployment"
        and row["implementation"] in OPTIMIZATION_LABELS
    }
    if arguments.baseline is not None:
        baseline_csv = arguments.baseline.resolve() / "statistics" / "cycles.csv"
        if not baseline_csv.is_file():
            parser.error(f"baseline statistics not found: {baseline_csv}")
        with baseline_csv.open(newline="", encoding="utf-8") as handle:
            for baseline_row in csv.DictReader(handle):
                if (baseline_row["group"] == "baseline"
                        and baseline_row["name"] == "deployment"
                        and baseline_row["implementation"] in OPTIMIZATION_LABELS):
                    m55_baseline[(baseline_row["algorithm"],
                                  baseline_row["operation"])] = baseline_row
    ablation_comparisons: list[dict[str, object]] = []
    for row in rows:
        if (row["group"] != "ablations"
                or row["implementation"] not in OPTIMIZATION_LABELS):
            continue
        baseline_row = m55_baseline.get((row["algorithm"], row["operation"]))
        if baseline_row is None:
            continue
        ablation_comparisons.append({
            "name": row["name"], "algorithm": row["algorithm"],
            "operation": row["operation"], "top_level": row["top_level"],
            "baseline_median": baseline_row["median"],
            "ablation_median": row["median"],
            "baseline_over_ablation": (
                float(baseline_row["median"]) / float(row["median"])
            ),
            "ablation_slowdown": (
                float(row["median"]) / float(baseline_row["median"])
            ),
        })
    ablation_fields = [
        "name", "algorithm", "operation", "top_level", "baseline_median",
        "ablation_median", "baseline_over_ablation", "ablation_slowdown",
    ]
    write_csv(campaign / "statistics" / "ablation-comparison.csv", ablation_fields,
              ablation_comparisons)
    print(f"Wrote recomputed statistics under {campaign / 'statistics'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

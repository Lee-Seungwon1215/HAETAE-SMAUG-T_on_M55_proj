#!/usr/bin/env python3
"""Analyze fixed-versus-random DWT timing traces from the STM32N657."""

from __future__ import annotations

import argparse
import csv
import json
import math
import pathlib
import re
import statistics


HEADER_RE = re.compile(
    r"^CT_TEST,version=1,scheme=(?P<scheme>[^,]+),backend=(?P<backend>[^,]+),"
    r"operation=(?P<operation>[^,]+),pairs=(?P<pairs>\d+),"
)
SAMPLE_RE = re.compile(
    r"^CT,c=(?P<class_id>[01]),p=(?P<pair>\d+),o=(?P<order>[01]),t=(?P<ticks>\d+)$"
)


def welch_t(left: list[int], right: list[int]) -> float:
    left_mean = statistics.fmean(left)
    right_mean = statistics.fmean(right)
    left_var = statistics.variance(left)
    right_var = statistics.variance(right)
    denominator = math.sqrt(left_var / len(left) + right_var / len(right))
    if denominator == 0.0:
        if left_mean == right_mean:
            return 0.0
        return math.copysign(math.inf, left_mean - right_mean)
    return (left_mean - right_mean) / denominator


def percentile(values: list[int], fraction: float) -> float:
    ordered = sorted(values)
    position = fraction * (len(ordered) - 1)
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return float(ordered[lower])
    weight = position - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def parse_log(path: pathlib.Path) -> dict[str, object]:
    text = path.read_text(encoding="utf-8", errors="replace")
    header = None
    classes: dict[int, list[int]] = {0: [], 1: []}
    positions: dict[int, list[int]] = {0: [], 1: []}
    for line in text.splitlines():
        match = HEADER_RE.match(line)
        if match:
            header = match.groupdict()
            continue
        match = SAMPLE_RE.match(line)
        if match:
            class_id = int(match.group("class_id"))
            classes[class_id].append(int(match.group("ticks")))
            positions[int(match.group("order"))].append(class_id)
    if header is None:
        raise ValueError(f"missing CT_TEST header: {path}")
    expected = int(header["pairs"])
    if len(classes[0]) != expected or len(classes[1]) != expected:
        raise ValueError(
            f"incomplete fixed/random populations in {path}: "
            f"{len(classes[0])}/{len(classes[1])}, expected {expected} each"
        )
    if "CT_STATUS,ok" not in text or "STATUS,ok" not in text:
        raise ValueError(f"target status missing: {path}")

    fixed = classes[0]
    random = classes[1]
    fixed_mean = statistics.fmean(fixed)
    random_mean = statistics.fmean(random)
    t_value = welch_t(fixed, random)
    descriptor = json.loads((path.parent / "run.json").read_text(encoding="utf-8"))
    return {
        "algorithm": descriptor["algorithm"],
        "scheme": header["scheme"],
        "backend": header["backend"],
        "operation": header["operation"],
        "samples_per_class": expected,
        "fixed_mean": fixed_mean,
        "random_mean": random_mean,
        "mean_delta_cycles": random_mean - fixed_mean,
        "relative_delta_percent":
            100.0 * (random_mean - fixed_mean) / fixed_mean,
        "fixed_median": statistics.median(fixed),
        "random_median": statistics.median(random),
        "fixed_q1": percentile(fixed, 0.25),
        "fixed_q3": percentile(fixed, 0.75),
        "random_q1": percentile(random, 0.25),
        "random_q3": percentile(random, 0.75),
        "welch_t": t_value,
        "abs_welch_t": abs(t_value),
        "result": "timing-signal" if abs(t_value) >= 4.5 else "no-signal-detected",
        "fixed_first_count": positions[0].count(0),
        "random_first_count": positions[0].count(1),
        "log": str(path),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", type=pathlib.Path)
    args = parser.parse_args()
    campaign = args.campaign.resolve()
    rows = [parse_log(path) for path in sorted(campaign.glob("runs/**/output.log"))]
    if not rows:
        parser.error(f"no timing logs under {campaign}")
    rows.sort(key=lambda row: (
        str(row["algorithm"]), str(row["operation"]), str(row["backend"])
    ))
    output = campaign / "statistics"
    output.mkdir(parents=True, exist_ok=True)
    fieldnames = list(rows[0])
    with (output / "fixed-vs-random-welch.csv").open(
        "w", newline="", encoding="utf-8"
    ) as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    signals = sum(row["result"] == "timing-signal" for row in rows)
    (output / "summary.txt").write_text(
        "STM32N657 fixed-versus-random timing screen\n"
        f"tests={len(rows)}\n"
        f"timing_signal={signals}\n"
        f"no_signal_detected={len(rows) - signals}\n"
        "threshold=abs(Welch t) >= 4.5\n"
        "interpretation=Statistical screening; no-signal-detected is not a proof "
        "of constant-time behavior.\n",
        encoding="utf-8",
    )
    print(f"Wrote {len(rows)} timing-test rows under {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

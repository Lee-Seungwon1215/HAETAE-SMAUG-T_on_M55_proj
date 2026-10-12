#!/usr/bin/env python3
"""Analyze the STM32N657 old-vs-all-path Slothy campaign."""

from __future__ import annotations

import argparse
import json
import pathlib
from collections import defaultdict

from analyze_keccak_chi_campaign import OPERATIONS, distribution, result_fields, write_csv


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", type=pathlib.Path)
    args = parser.parse_args()
    campaign = args.campaign.resolve()
    specification = json.loads((campaign / "campaign.json").read_text(encoding="utf-8"))
    expected = int(specification["iterations_per_run"]) * int(specification["sets"])

    pooled: dict[tuple[str, str, str], list[int]] = defaultdict(list)
    for log in sorted(campaign.glob("runs/performance/set*/**/output.log")):
        descriptor = json.loads(log.with_name("run.json").read_text(encoding="utf-8"))
        for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
            if not line.startswith("RESULT,"):
                continue
            fields = result_fields(line)
            if fields.get("operation") in OPERATIONS:
                pooled[(descriptor["name"], descriptor["algorithm"], fields["operation"])].append(int(fields["ticks"]))

    if not pooled:
        raise SystemExit("no all-path campaign samples found")
    bad = {key: len(values) for key, values in pooled.items() if len(values) != expected}
    if bad:
        raise SystemExit(f"expected {expected} pooled samples per cell, got {bad}")

    stats = {key: distribution(values) for key, values in pooled.items()}
    rows: list[dict[str, object]] = []
    for (config, algorithm, operation), values in sorted(stats.items()):
        old = float(stats[("old_selected", algorithm, operation)]["median"])
        median = float(values["median"])
        rows.append(
            {
                "config": config,
                "algorithm": algorithm,
                "operation": operation,
                **values,
                "speedup_vs_old_selected": old / median,
                "change_pct_vs_old_selected": (median / old - 1.0) * 100.0,
            }
        )

    statistics_dir = campaign / "statistics"
    write_csv(statistics_dir / "keccak-allpaths-pooled.csv", rows)
    (statistics_dir / "keccak-allpaths-summary.json").write_text(
        json.dumps(rows, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()

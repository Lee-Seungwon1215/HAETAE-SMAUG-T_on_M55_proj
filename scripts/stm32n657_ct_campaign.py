#!/usr/bin/env python3
"""Run fixed-versus-random timing screens on the physical STM32N657."""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import subprocess
import sys

import stm32n657_campaign as base
from stm32n657_final_paper_campaign import CONFIGS


ROOT = pathlib.Path(__file__).resolve().parents[1]
HAETAE = ("haetae2", "haetae3", "haetae5")
SMAUGT = ("smaugt1", "smaugt3", "smaugt5")
HAETAE_OPERATIONS = ("ntt", "invntt", "pointwise", "fft", "sampler")
SMAUGT_OPERATIONS = ("poly_mul_acc", "matrix_vec_mult")
SHARED_OPERATIONS = ("keccak_x4", "shake_x4")


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--pairs", type=int, default=5000)
    parser.add_argument("--probe-serial", default="")
    args = parser.parse_args()
    if args.pairs < 2:
        parser.error("--pairs must be at least 2")
    return args


def run_case(output: pathlib.Path, config: str, algorithm: str,
             operation: str, pairs: int, serial: str) -> None:
    implementation, variables = CONFIGS[config]
    extra = dict(variables)
    extra.update({
        "CT_TEST": "1",
        "CT_OPERATION": operation,
        "CT_PAIRS": str(pairs),
        "CT_BACKEND": config,
        "STM32_RUN_TIMEOUT": "900",
    })
    base.run_make(
        output, "fixed-random", f"{config}-{operation}", algorithm,
        implementation, 1, serial, extra,
    )


def main() -> int:
    args = arguments()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    base.write_json(output / "campaign.json", {
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "platform": "STM32N657 Cortex-M55",
        "timer": "DWT CYCCNT",
        "pairs_per_test": args.pairs,
        "classes": {"0": "fixed input", "1": "deterministic random input"},
        "ordering": "one sample from each class per pair; pair order randomized",
        "timed_region": "kernel only; input generation and copying excluded",
        "threshold": "abs(Welch t) >= 4.5",
        "scope": {
            "haetae": HAETAE_OPERATIONS,
            "smaugt": SMAUGT_OPERATIONS,
            "shared_optimized_only": SHARED_OPERATIONS,
        },
        "interpretation": "screening evidence, not a proof of constant time",
    })
    try:
        with base.persistent_probe_session(output, args.probe_serial):
            for algorithm in HAETAE:
                for operation in HAETAE_OPERATIONS:
                    for config in CONFIGS:
                        run_case(output, config, algorithm, operation,
                                 args.pairs, args.probe_serial)
            for algorithm in SMAUGT:
                for operation in SMAUGT_OPERATIONS:
                    for config in CONFIGS:
                        run_case(output, config, algorithm, operation,
                                 args.pairs, args.probe_serial)
            # The four-stream implementation does not exist in Reference C.
            for operation in SHARED_OPERATIONS:
                for config in ("intrinsic", "assembly", "final"):
                    run_case(output, config, "haetae2", operation,
                             args.pairs, args.probe_serial)
        subprocess.run(
            [sys.executable, str(ROOT / "scripts" / "analyze_stm32n657_ct.py"),
             str(output)], cwd=ROOT, check=True,
        )
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        print(f"Partial evidence preserved in {output}", file=sys.stderr)
        return 1
    print(f"Constant-time timing evidence: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Final order-balanced reference/M4/M55+MVE campaign on STM32N657."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys

import stm32n657_campaign as base


ALGORITHMS = base.ALGORITHMS

STACK_BASELINE = {
    "HAETAE2_STACK_CONFIG": "baseline",
    "HAETAE3_STACK_CONFIG": "baseline",
    "HAETAE5_STACK_CONFIG": "baseline",
    "SMAUGT1_STACK_CONFIG": "baseline",
    "SMAUGT3_STACK_CONFIG": "baseline",
    "SMAUGT5_STACK_CONFIG": "baseline",
}

CONFIGS = {
    "reference": ("reference", STACK_BASELINE),
    "m4_scalar": (
        "optimization",
        {
            **STACK_BASELINE,
            "KECCAK_VARIANT": "pqm4_persistent",
            "KECCAK_KERNEL": "m4_scalar",
            "KECCAK_RC": "memory",
            "KECCAK_X4": "0",
        },
    ),
    "m55_sched_mve_x4": (
        "optimization",
        {
            **STACK_BASELINE,
            "KECCAK_VARIANT": "pqm4_persistent",
            "KECCAK_KERNEL": "m55_scalar_sched",
            "KECCAK_RC": "memory",
            "KECCAK_X4": "1",
        },
    ),
}

# A three-treatment Latin square: each configuration appears once in every
# order position, avoiding a fixed early/late bias across the three sets.
ORDERS = (
    ("reference", "m4_scalar", "m55_sched_mve_x4"),
    ("m4_scalar", "m55_sched_mve_x4", "reference"),
    ("m55_sched_mve_x4", "reference", "m4_scalar"),
)


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--iterations", type=int, default=100)
    parser.add_argument("--probe-serial", default="")
    parser.add_argument("--skip-correctness", action="store_true")
    args = parser.parse_args()
    if args.iterations < 1:
        parser.error("--iterations must be positive")
    return args


def main() -> int:
    args = arguments()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)

    # Reuse the main campaign's immutable environment/source snapshot format.
    if not (output / "metadata.json").is_file():
        metadata_args = argparse.Namespace(
            probe_serial=args.probe_serial,
            command="final-keccak-3x100",
        )
        base.capture_metadata(output, metadata_args)

    base.write_json(
        output / "final-campaign.json",
        {
            "algorithms": ALGORITHMS,
            "iterations_per_set": args.iterations,
            "sets": len(ORDERS),
            "orders": ORDERS,
            "configs": {
                name: {"implementation": implementation, "variables": variables}
                for name, (implementation, variables) in CONFIGS.items()
            },
            "selection_unit": "pooled 300 board samples per parameter and operation",
            "compiler_flags": "-mcpu=cortex-m55 -mthumb -mfloat-abi=hard -O3",
            "stack_configuration": "baseline for all six parameter sets",
        },
    )

    try:
        with base.persistent_probe_session(output, args.probe_serial):
            if not args.skip_correctness:
                implementation, variables = CONFIGS["m55_sched_mve_x4"]
                for algorithm in ALGORITHMS:
                    base.run_make(
                        output,
                        "correctness",
                        "m55_sched_mve_x4",
                        algorithm,
                        implementation,
                        1,
                        args.probe_serial,
                        {**variables, "DIFF_TEST": "1", "STM32_RUN_TIMEOUT": "900"},
                        correctness_gate=True,
                    )

            for set_index, order in enumerate(ORDERS, start=1):
                for algorithm in ALGORITHMS:
                    for config_name in order:
                        implementation, variables = CONFIGS[config_name]
                        base.run_make(
                            output,
                            f"performance/set{set_index}",
                            config_name,
                            algorithm,
                            implementation,
                            args.iterations,
                            args.probe_serial,
                            variables,
                        )
    except Exception as error:  # Preserve resumable evidence on any board fault.
        print(f"ERROR: {error}", file=sys.stderr)
        print(f"Partial evidence preserved in {output}", file=sys.stderr)
        return 1

    print(f"Final campaign evidence: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

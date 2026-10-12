#!/usr/bin/env python3
"""Order-balanced old-vs-all-path Slothy campaign on STM32N657."""

from __future__ import annotations

import argparse
import pathlib
import sys

import stm32n657_campaign as base


ALGORITHMS = base.HAETAE

COMMON = {
    "HAETAE2_STACK_CONFIG": "baseline",
    "HAETAE3_STACK_CONFIG": "baseline",
    "HAETAE5_STACK_CONFIG": "baseline",
    "KECCAK_VARIANT": "pqm4_persistent",
    "KECCAK_KERNEL": "m55_scalar_sched",
    "KECCAK_RC": "memory",
    "KECCAK_X4": "1",
    "KECCAK_X4_BACKEND": "asm_slothy",
    "HAETAE_NTT_BACKEND": "asm_slothy",
    "HAETAE_NTT_ASM_VARIANT": "paired_vld4_u2_l34_hier",
    "HAETAE_NTT_ARITH_VARIANT": "d2",
    "HAETAE_NTT_INVERSE_VARIANT": "slothy_u1",
    "HAETAE_NTT_POINTWISE_VARIANT": "slothy_lowmem",
    "HAETAE_NTT_POINTWISE_ACC_VARIANT": "slothy_block2",
    "HAETAE_NTT_BARRETT_VARIANT": "slothy_u2_nostack",
    "HAETAE_NTT_CENTERED_VARIANT": "slothy_mask_u2",
    "HAETAE_FFT_BACKEND": "asm_slothy",
    "HAETAE_FFT_ASM_VARIANT": "soa_reuse_vecsq_u2_hier",
    "HAETAE_SAMPLER_BACKEND": "asm_hand",
    "HAETAE_SAMPLER_ASM_VARIANT": "unroll",
}

CONFIGS = {
    "old_selected": {
        **COMMON,
        "KECCAK_X4_ASM_VARIANT": "slothy_gather_u2_fullperm_chi5straight",
    },
    "allpaths_bytegather": {
        **COMMON,
        "KECCAK_X4_ASM_VARIANT": "slothy_gather_u2_fullperm_chi5_allpaths",
    },
}

# AB/BA makes each candidate run once in the early and late position.
ORDERS = (
    ("old_selected", "allpaths_bytegather"),
    ("allpaths_bytegather", "old_selected"),
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

    if not (output / "metadata.json").is_file():
        base.capture_metadata(
            output,
            argparse.Namespace(
                probe_serial=args.probe_serial,
                command="keccak-allpaths-2x2-order-balanced",
            ),
        )

    base.write_json(
        output / "campaign.json",
        {
            "algorithms": ALGORITHMS,
            "iterations_per_run": args.iterations,
            "sets": len(ORDERS),
            "orders": ORDERS,
            "configs": CONFIGS,
            "selection_unit": "pooled board samples per parameter and operation",
            "timer": "DWT CYCCNT",
            "core_clock_hz": 600_000_000,
        },
    )

    try:
        with base.persistent_probe_session(output, args.probe_serial):
            if not args.skip_correctness:
                for algorithm in ALGORITHMS:
                    base.run_make(
                        output,
                        "correctness",
                        "allpaths_bytegather",
                        algorithm,
                        "optimization",
                        1,
                        args.probe_serial,
                        {
                            **CONFIGS["allpaths_bytegather"],
                            "DIFF_TEST": "1",
                            "STM32_RUN_TIMEOUT": "900",
                        },
                        correctness_gate=True,
                    )

            for set_index, order in enumerate(ORDERS, start=1):
                for algorithm in ALGORITHMS:
                    for config_name in order:
                        base.run_make(
                            output,
                            f"performance/set{set_index}",
                            config_name,
                            algorithm,
                            "optimization",
                            args.iterations,
                            args.probe_serial,
                            CONFIGS[config_name],
                        )
    except Exception as error:
        print(f"ERROR: {error}", file=sys.stderr)
        print(f"Partial evidence preserved in {output}", file=sys.stderr)
        return 1

    print(f"Keccak all-path campaign evidence: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

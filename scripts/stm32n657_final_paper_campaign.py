#!/usr/bin/env python3
"""Run the four-backend final paper campaign on a physical STM32N657."""

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

# Dataflow choices common to the three optimized backends.  Only the backend
# implementation changes, so the intrinsic -> assembly -> final comparison
# does not accidentally mix in an unrelated algorithmic variant.
OPTIMIZED_COMMON = {
    **STACK_BASELINE,
    "KECCAK_VARIANT": "pqm4_persistent",
    "KECCAK_KERNEL": "m55_scalar_sched",
    "KECCAK_RC": "memory",
    "KECCAK_X4": "1",
    "NTT_VARIANT": "m32_l332_vld4_u2",
    "ACC_VARIANT": "aftersum_dmax_tight_precenter_poly",
    "FFT_VARIANT": "qrd_soa_l2222_vtbl_fused_u4",
    "SAMPLER_VARIANT": "cdt_mve_load",
    "MUL_VARIANT": "toom4_outer32",
    "MUL_CACHE": "1",
    "SHIFT_VARIANT": "mask",
}

INTRINSIC = {
    **OPTIMIZED_COMMON,
    "KECCAK_X4_BACKEND": "intrinsic",
    "HAETAE_NTT_BACKEND": "intrinsic",
    "HAETAE_FFT_BACKEND": "intrinsic",
    "HAETAE_SAMPLER_BACKEND": "intrinsic",
    "SMAUGT_TOOM_BACKEND": "intrinsic",
}

ASSEMBLY = {
    **OPTIMIZED_COMMON,
    "KECCAK_X4_BACKEND": "asm_hand",
    "KECCAK_X4_ASM_VARIANT": "hand_gather_v2",
    "HAETAE_NTT_BACKEND": "asm_hand",
    "HAETAE_NTT_ASM_VARIANT": "hand_l332_vld4_u2",
    "HAETAE_NTT_ARITH_VARIANT": "d1",
    "HAETAE_NTT_INVERSE_VARIANT": "hand",
    "HAETAE_NTT_POINTWISE_VARIANT": "hand",
    "HAETAE_NTT_POINTWISE_ACC_VARIANT": "hand",
    "HAETAE_NTT_BARRETT_VARIANT": "hand",
    "HAETAE_NTT_CENTERED_VARIANT": "hand",
    "HAETAE_FFT_BACKEND": "asm_hand",
    "HAETAE_FFT_ASM_VARIANT": "hand_soa_l2222",
    "HAETAE_SAMPLER_BACKEND": "asm_hand",
    "HAETAE_SAMPLER_ASM_VARIANT": "unroll",
    "SMAUGT_TOOM_BACKEND": "asm_hand",
    "SMAUGT_TOOM_ASM_VARIANT": "hand_fused",
    "SMAUGT_TOOM_ASM_COMPONENT": "all",
}

FINAL = {
    **OPTIMIZED_COMMON,
    "KECCAK_X4_BACKEND": "asm_slothy",
    "KECCAK_X4_ASM_VARIANT": "slothy_gather_u2_fullperm_chi5_allpaths",
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
    # The arithmetic-mask Slothy sampler regressed on the board.  The final
    # deployment therefore deliberately retains the selected manual schedule.
    "HAETAE_SAMPLER_BACKEND": "asm_hand",
    "HAETAE_SAMPLER_ASM_VARIANT": "unroll",
    "SMAUGT_TOOM_BACKEND": "asm_slothy",
    "SMAUGT_TOOM_ASM_VARIANT": "slothy_fused_tailpred_stable_u1",
    "SMAUGT_TOOM_ASM_COMPONENT": "all",
}

CONFIGS = {
    "reference": ("reference", STACK_BASELINE),
    "intrinsic": ("optimization", INTRINSIC),
    "assembly": ("optimization", ASSEMBLY),
    "final": ("optimization", FINAL),
}

# Four cyclic orders form a Latin square.  Every backend appears exactly once
# in every early/late position across the four sets.
ORDERS = (
    ("reference", "intrinsic", "assembly", "final"),
    ("intrinsic", "assembly", "final", "reference"),
    ("assembly", "final", "reference", "intrinsic"),
    ("final", "reference", "intrinsic", "assembly"),
)


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "command", choices=("smoke", "correctness", "performance", "all")
    )
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--iterations", type=int, default=100)
    parser.add_argument("--probe-serial", default="")
    parser.add_argument("--skip-correctness", action="store_true")
    args = parser.parse_args()
    if args.iterations < 1:
        parser.error("--iterations must be positive")
    return args


def capture_manifest(output: pathlib.Path, args: argparse.Namespace) -> None:
    if not (output / "metadata.json").is_file():
        metadata_args = argparse.Namespace(
            probe_serial=args.probe_serial,
            command="final-paper-four-backend",
        )
        base.capture_metadata(output, metadata_args)
    base.write_json(
        output / "final-paper-campaign.json",
        {
            "algorithms": ALGORITHMS,
            "iterations_per_set": args.iterations,
            "sets": len(ORDERS),
            "orders": ORDERS,
            "configs": {
                name: {"implementation": implementation, "variables": variables}
                for name, (implementation, variables) in CONFIGS.items()
            },
            "primary_baseline": "intrinsic",
            "context_baseline": "reference",
            "speedups": {
                "assembly": "intrinsic_cycles / assembly_cycles",
                "slothy_incremental": "assembly_cycles / final_cycles",
                "total": "intrinsic_cycles / final_cycles",
            },
            "timer": "DWT CYCCNT",
            "compiler_flags": "-mcpu=cortex-m55 -mthumb -mfloat-abi=hard -O3",
            "stack_metric": "per-operation runtime high-water mark in bytes",
            "qemu_used_for_reportable_results": False,
        },
    )


def run_config(
    output: pathlib.Path,
    group: str,
    config_name: str,
    algorithm: str,
    iterations: int,
    probe_serial: str,
    *,
    correctness: bool = False,
) -> None:
    implementation, variables = CONFIGS[config_name]
    extra = dict(variables)
    differential = correctness and config_name != "reference"
    if differential:
        extra.update({"DIFF_TEST": "1", "STM32_RUN_TIMEOUT": "900"})
    base.run_make(
        output,
        group,
        config_name,
        algorithm,
        implementation,
        iterations,
        probe_serial,
        extra,
        correctness_gate=differential,
    )


def run_smoke(output: pathlib.Path, args: argparse.Namespace) -> None:
    # HAETAE2 exercises every shared HAETAE/Keccak selector; SMAUG-T1 exercises
    # the Toom backend.  One iteration catches misselected or missing objects.
    for algorithm in ("haetae2", "smaugt1"):
        for config_name in CONFIGS:
            run_config(
                output, "smoke", config_name, algorithm, 1, args.probe_serial
            )


def run_correctness(output: pathlib.Path, args: argparse.Namespace) -> None:
    # DIFF_TEST links the reference implementation into an optimized image and
    # is consequently unavailable when IMPLEMENTATION=reference.  The reference
    # image runs its own self-test; every optimized backend then runs KAT plus a
    # 1,000-case differential test against that same reference code.
    for config_name in CONFIGS:
        for algorithm in ALGORITHMS:
            run_config(
                output,
                "correctness",
                config_name,
                algorithm,
                1,
                args.probe_serial,
                correctness=True,
            )


def run_performance(output: pathlib.Path, args: argparse.Namespace) -> None:
    for set_index, order in enumerate(ORDERS, start=1):
        for algorithm in ALGORITHMS:
            for config_name in order:
                run_config(
                    output,
                    f"performance/set{set_index}",
                    config_name,
                    algorithm,
                    args.iterations,
                    args.probe_serial,
                )


def main() -> int:
    args = arguments()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    capture_manifest(output, args)
    try:
        with base.persistent_probe_session(output, args.probe_serial):
            if args.command == "smoke":
                run_smoke(output, args)
            elif args.command == "correctness":
                run_correctness(output, args)
            elif args.command == "performance":
                if not args.skip_correctness:
                    run_correctness(output, args)
                run_performance(output, args)
            else:
                run_smoke(output, args)
                if not args.skip_correctness:
                    run_correctness(output, args)
                run_performance(output, args)
    except Exception as error:
        print(f"ERROR: {error}", file=sys.stderr)
        print(f"Partial evidence preserved in {output}", file=sys.stderr)
        return 1
    print(f"Final four-backend evidence: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

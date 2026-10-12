#!/usr/bin/env python3
"""Measure matched pre-Slothy and post-Slothy kernels on STM32N657."""

from __future__ import annotations

import argparse
import csv
import dataclasses
import json
import pathlib
import re
import statistics
import sys

import stm32n657_campaign as base


ALGORITHMS = base.ALGORITHMS
HAETAE = base.HAETAE
SMAUG = base.SMAUG

STACK_BASELINE = {
    "HAETAE2_STACK_CONFIG": "baseline",
    "HAETAE3_STACK_CONFIG": "baseline",
    "HAETAE5_STACK_CONFIG": "baseline",
    "SMAUGT1_STACK_CONFIG": "baseline",
    "SMAUGT3_STACK_CONFIG": "baseline",
    "SMAUGT5_STACK_CONFIG": "baseline",
}

COMMON = {
    **STACK_BASELINE,
    "KECCAK_VARIANT": "pqm4_persistent",
    "KECCAK_KERNEL": "m55_scalar_sched",
    "KECCAK_RC": "memory",
    "KECCAK_X4": "1",
}

KECCAK_POST = {
    "KECCAK_X4_BACKEND": "asm_slothy",
    "KECCAK_X4_ASM_VARIANT": "slothy_gather_u2_fullperm_chi5_allpaths",
}

KECCAK_PRE = {
    "KECCAK_X4_BACKEND": "asm_slothy",
    "KECCAK_X4_ASM_VARIANT": "preslothy_fullperm_chi5_allpaths",
}

HAETAE_POST = {
    **COMMON,
    **KECCAK_POST,
    "NTT_VARIANT": "m32_l332_vld4_u2",
    "ACC_VARIANT": "aftersum_dmax_tight_precenter_poly",
    "FFT_VARIANT": "qrd_soa_l2222_vtbl_fused_u4",
    "SAMPLER_VARIANT": "cdt_mve_load",
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

HAETAE_ALL_PRE = {
    **HAETAE_POST,
    **KECCAK_PRE,
    "HAETAE_NTT_ASM_VARIANT": "preslothy",
    "HAETAE_NTT_INVERSE_VARIANT": "preslothy",
    "HAETAE_NTT_POINTWISE_VARIANT": "preslothy_lowmem",
    "HAETAE_NTT_POINTWISE_ACC_VARIANT": "preslothy_block2",
    "HAETAE_NTT_BARRETT_VARIANT": "preslothy_nostack",
    "HAETAE_NTT_CENTERED_VARIANT": "preslothy_mask",
    "HAETAE_FFT_ASM_VARIANT": "preslothy_vecsq_u2",
    "HAETAE_SAMPLER_BACKEND": "asm_slothy",
    "HAETAE_SAMPLER_ASM_VARIANT": "preslothy_mask",
}

SMAUG_POST = {
    **COMMON,
    **KECCAK_POST,
    "MUL_VARIANT": "toom4_outer32",
    "MUL_CACHE": "1",
    "SHIFT_VARIANT": "mask",
    "SMAUGT_TOOM_BACKEND": "asm_slothy",
    "SMAUGT_TOOM_ASM_VARIANT": "slothy_fused_tailpred_stable_u1",
    "SMAUGT_TOOM_ASM_COMPONENT": "all",
}

SMAUG_ALL_PRE = {
    **SMAUG_POST,
    **KECCAK_PRE,
    "SMAUGT_TOOM_ASM_VARIANT": "preslothy_fused",
}


@dataclasses.dataclass(frozen=True)
class Pair:
    name: str
    algorithms: tuple[str, ...]
    operations: tuple[str, ...]
    pre_changes: dict[str, str]
    post_changes: dict[str, str]


PAIRS = (
    Pair(
        "keccak_x4", ALGORITHMS,
        ("keccak_permutation_x4_mve", "shake128_x4_mve"),
        KECCAK_PRE, KECCAK_POST,
    ),
    Pair(
        "haetae_forward_ntt", HAETAE, ("ntt",),
        {"HAETAE_NTT_ASM_VARIANT": "preslothy"},
        {"HAETAE_NTT_ASM_VARIANT": "paired_vld4_u2_l34_hier"},
    ),
    Pair(
        "haetae_inverse_ntt", HAETAE, ("invntt",),
        {"HAETAE_NTT_INVERSE_VARIANT": "preslothy"},
        {"HAETAE_NTT_INVERSE_VARIANT": "slothy_u1"},
    ),
    Pair(
        "haetae_pointwise", HAETAE, ("pointwise",),
        {"HAETAE_NTT_POINTWISE_VARIANT": "preslothy_lowmem"},
        {"HAETAE_NTT_POINTWISE_VARIANT": "slothy_lowmem"},
    ),
    Pair(
        "haetae_pointwise_acc", HAETAE,
        ("pointwise_acc", "pointwise_acc_precentered",
         "pointwise_acc_u16_precentered"),
        {"HAETAE_NTT_POINTWISE_ACC_VARIANT": "preslothy_block2"},
        {"HAETAE_NTT_POINTWISE_ACC_VARIANT": "slothy_block2"},
    ),
    Pair(
        "haetae_barrett", HAETAE, ("barrett_tight",),
        {"HAETAE_NTT_BARRETT_VARIANT": "preslothy_nostack"},
        {"HAETAE_NTT_BARRETT_VARIANT": "slothy_u2_nostack"},
    ),
    Pair(
        "haetae_centered", HAETAE, ("reduce2q", "freeze2q", "freeze"),
        {"HAETAE_NTT_CENTERED_VARIANT": "preslothy_mask"},
        {"HAETAE_NTT_CENTERED_VARIANT": "slothy_mask_u2"},
    ),
    Pair(
        "haetae_fft", HAETAE, ("fft", "fft_fused_sqabs"),
        {"HAETAE_FFT_ASM_VARIANT": "preslothy_vecsq_u2"},
        {"HAETAE_FFT_ASM_VARIANT": "soa_reuse_vecsq_u2_hier"},
    ),
    Pair(
        "haetae_sampler", HAETAE,
        ("sampler_cdt_leaf", "sampler_gauss_N"),
        {
            "HAETAE_SAMPLER_BACKEND": "asm_slothy",
            "HAETAE_SAMPLER_ASM_VARIANT": "preslothy_mask",
        },
        {
            "HAETAE_SAMPLER_BACKEND": "asm_slothy",
            "HAETAE_SAMPLER_ASM_VARIANT": "mask_u2",
        },
    ),
    Pair(
        "smaugt_toom", SMAUG, ("poly_mul_acc", "matrix_vec_mult"),
        {"SMAUGT_TOOM_ASM_VARIANT": "preslothy_fused"},
        {"SMAUGT_TOOM_ASM_VARIANT": "slothy_fused_tailpred_stable_u1"},
    ),
)

RESULT_RE = re.compile(
    r"^RESULT,.*operation=([^,]+),sample=([0-9]+),ticks=([0-9]+)$"
)


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "command", choices=("smoke", "correctness", "performance", "analyze", "all")
    )
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--iterations", type=int, default=100)
    parser.add_argument("--probe-serial", default="")
    parser.add_argument("--skip-correctness", action="store_true")
    parser.add_argument(
        "--resume",
        action="store_true",
        help=(
            "skip an existing performance run only when run.json is successful, "
            "the selectors match, and every target operation has the requested "
            "number of samples"
        ),
    )
    parser.add_argument(
        "--pairs", nargs="*", choices=tuple(pair.name for pair in PAIRS),
        help="run only the selected matched pairs",
    )
    args = parser.parse_args()
    if args.iterations < 1:
        parser.error("--iterations must be positive")
    return args


def selected_pairs(args: argparse.Namespace) -> tuple[Pair, ...]:
    if not args.pairs:
        return PAIRS
    names = set(args.pairs)
    return tuple(pair for pair in PAIRS if pair.name in names)


def base_for(algorithm: str) -> dict[str, str]:
    return dict(HAETAE_POST if algorithm in HAETAE else SMAUG_POST)


def variables_for(pair: Pair, algorithm: str, stage: str) -> dict[str, str]:
    variables = base_for(algorithm)
    variables.update(pair.pre_changes if stage == "pre" else pair.post_changes)
    return variables


def capture_manifest(output: pathlib.Path, args: argparse.Namespace) -> None:
    if not (output / "metadata.json").is_file():
        metadata_args = argparse.Namespace(
            probe_serial=args.probe_serial,
            command="slothy-matched-pre-post",
        )
        base.capture_metadata(output, metadata_args)
    campaign_path = output / "campaign.json"
    if campaign_path.is_file():
        existing = json.loads(campaign_path.read_text(encoding="utf-8"))
        if int(existing.get("iterations_per_set", -1)) != args.iterations:
            raise RuntimeError(
                "resume iteration mismatch: existing campaign uses "
                f"{existing.get('iterations_per_set')}, requested {args.iterations}"
            )
        return
    base.write_json(
        campaign_path,
        {
            "iterations_per_set": args.iterations,
            "orders": (("pre", "post"), ("post", "pre")),
            "pairs": [dataclasses.asdict(pair) for pair in selected_pairs(args)],
            "method": (
                "For each pair, every non-target selector is held at the selected "
                "post-Slothy configuration. The pre image links the exact assembly "
                "source supplied to Slothy; the post image links its generated output."
            ),
            "qemu_used_for_reportable_results": False,
        },
    )


def run_one(
    output: pathlib.Path,
    group: str,
    name: str,
    algorithm: str,
    iterations: int,
    probe_serial: str,
    variables: dict[str, str],
    *,
    correctness: bool = False,
) -> None:
    extra = dict(variables)
    if correctness:
        extra.update({"DIFF_TEST": "1", "STM32_RUN_TIMEOUT": "900"})
    base.run_make(
        output, group, name, algorithm, "optimization", iterations,
        probe_serial, extra, correctness_gate=correctness,
    )


def run_smoke(output: pathlib.Path, args: argparse.Namespace) -> None:
    for algorithm, variables in (
        ("haetae2", HAETAE_ALL_PRE), ("smaugt1", SMAUG_ALL_PRE),
    ):
        run_one(
            output, "smoke", "all_pre", algorithm, 1,
            args.probe_serial, variables,
        )


def run_correctness(output: pathlib.Path, args: argparse.Namespace) -> None:
    for algorithm in ALGORITHMS:
        configs = (
            ("all_pre", HAETAE_ALL_PRE if algorithm in HAETAE else SMAUG_ALL_PRE),
            ("all_post", HAETAE_POST if algorithm in HAETAE else SMAUG_POST),
        )
        for name, variables in configs:
            run_one(
                output, "correctness", name, algorithm, 1,
                args.probe_serial, variables, correctness=True,
            )


def run_performance(output: pathlib.Path, args: argparse.Namespace) -> None:
    orders = (("pre", "post"), ("post", "pre"))
    for pair in selected_pairs(args):
        for set_index, order in enumerate(orders, start=1):
            for algorithm in pair.algorithms:
                for stage in order:
                    if args.resume and performance_run_complete(
                        output, pair, set_index, stage, algorithm, args.iterations
                    ):
                        print(
                            "RESUME,skip," 
                            f"pair={pair.name},set={set_index},stage={stage},"
                            f"algorithm={algorithm}"
                        )
                        continue
                    run_one(
                        output,
                        f"performance/{pair.name}/set{set_index}",
                        stage,
                        algorithm,
                        args.iterations,
                        args.probe_serial,
                        variables_for(pair, algorithm, stage),
                    )


def performance_run_complete(
    output: pathlib.Path,
    pair: Pair,
    set_index: int,
    stage: str,
    algorithm: str,
    iterations: int,
) -> bool:
    run_dir = (
        output / "runs" / "performance" / pair.name / f"set{set_index}" /
        stage / "optimization" / algorithm
    )
    run_path = run_dir / "run.json"
    log_path = run_dir / "output.log"
    if not run_path.is_file() or not log_path.is_file():
        return False
    try:
        record = json.loads(run_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False
    if record.get("status_ok") is not True or record.get("exit_code") != 0:
        return False
    recorded_variables = record.get("variables", {})
    expected_variables = variables_for(pair, algorithm, stage)
    if any(
        str(recorded_variables.get(key)) != str(value)
        for key, value in expected_variables.items()
    ):
        return False
    if str(recorded_variables.get("ITERATIONS")) != str(iterations):
        return False
    if "STATUS,ok" not in log_path.read_text(encoding="utf-8", errors="replace"):
        return False
    return all(
        len(samples_from(log_path, operation)) == iterations
        for operation in pair.operations
    )


def samples_from(path: pathlib.Path, operation: str) -> list[int]:
    samples: list[int] = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = RESULT_RE.match(line)
        if match and match.group(1) == operation:
            samples.append(int(match.group(3)))
    return samples


def analyze(output: pathlib.Path, pairs: tuple[Pair, ...]) -> None:
    rows: list[dict[str, object]] = []
    for pair in pairs:
        for algorithm in pair.algorithms:
            for operation in pair.operations:
                values: dict[str, list[int]] = {"pre": [], "post": []}
                for set_index in (1, 2):
                    for stage in ("pre", "post"):
                        path = (
                            output / "runs" / "performance" / pair.name /
                            f"set{set_index}" / stage / "optimization" /
                            algorithm / "output.log"
                        )
                        if not path.is_file():
                            raise RuntimeError(f"missing measurement log: {path}")
                        values[stage].extend(samples_from(path, operation))
                if not values["pre"] or not values["post"]:
                    raise RuntimeError(
                        f"missing samples for {pair.name}/{algorithm}/{operation}"
                    )
                pre = float(statistics.median(values["pre"]))
                post = float(statistics.median(values["post"]))
                rows.append(
                    {
                        "pair": pair.name,
                        "algorithm": algorithm,
                        "operation": operation,
                        "pre_samples": len(values["pre"]),
                        "post_samples": len(values["post"]),
                        "pre_median_cycles": pre,
                        "post_median_cycles": post,
                        "slothy_speedup": pre / post,
                        "cycle_reduction_percent": (pre - post) * 100.0 / pre,
                    }
                )

    statistics_dir = output / "statistics"
    statistics_dir.mkdir(parents=True, exist_ok=True)
    csv_path = statistics_dir / "slothy-matched-pairs.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=tuple(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    report = [
        "# Matched pre-/post-Slothy STM32N657 results",
        "",
        "Each row pools two reversed-order sets. `Speedup` is pre-Slothy cycles "
        "divided by post-Slothy cycles.",
        "",
        "| Pair | Parameter | Operation | Pre cycles | Post cycles | Speedup | Reduction |",
        "|---|---|---|---:|---:|---:|---:|",
    ]
    for row in rows:
        report.append(
            f"| {row['pair']} | {row['algorithm']} | {row['operation']} | "
            f"{row['pre_median_cycles']:,.1f} | {row['post_median_cycles']:,.1f} | "
            f"{row['slothy_speedup']:.4f}x | "
            f"{row['cycle_reduction_percent']:.2f}% |"
        )
    (output / "REPORT.md").write_text("\n".join(report) + "\n", encoding="utf-8")


def main() -> int:
    args = arguments()
    output = args.output.resolve()
    pairs = selected_pairs(args)
    if args.command == "analyze":
        if not output.is_dir():
            raise SystemExit(f"measurement directory not found: {output}")
        analyze(output, pairs)
        return 0
    output.mkdir(parents=True, exist_ok=True)
    capture_manifest(output, args)
    try:
        with base.persistent_probe_session(output, args.probe_serial):
            if args.command in ("smoke", "all"):
                run_smoke(output, args)
            if args.command in ("correctness", "all") or (
                args.command == "performance" and not args.skip_correctness
            ):
                run_correctness(output, args)
            if args.command in ("performance", "all"):
                run_performance(output, args)
    except Exception as error:
        print(f"ERROR: {error}", file=sys.stderr)
        print(f"Partial evidence preserved in {output}", file=sys.stderr)
        return 1
    if args.command in ("performance", "all"):
        analyze(output, pairs)
    print(f"Matched Slothy evidence: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

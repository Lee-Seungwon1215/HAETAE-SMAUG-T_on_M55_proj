#!/usr/bin/env python3
"""Run and archive the STM32N657 physical-board evaluation campaign."""

from __future__ import annotations

import argparse
import contextlib
import datetime as dt
import hashlib
import json
import os
import pathlib
import platform
import shutil
import socket
import subprocess
import sys
import tarfile
import time
from dataclasses import dataclass


ROOT = pathlib.Path(__file__).resolve().parents[1]
ALGORITHMS = ("haetae2", "haetae3", "haetae5", "smaugt1", "smaugt3", "smaugt5")
HAETAE = ALGORITHMS[:3]
SMAUG = ALGORITHMS[3:]
PROBE_ID = "0483:3754"


@dataclass(frozen=True)
class Ablation:
    name: str
    algorithms: tuple[str, ...]
    variables: dict[str, str]


# Each row changes one public Make variable from the deployment configuration.
# Scalar controls are retained as separate, deliberately coarse contribution
# points and are not described as one-variable microarchitectural derivatives.
ABLATIONS = (
    Ablation("keccak_representation_pqm4", ALGORITHMS, {"KECCAK_VARIANT": "pqm4"}),
    Ablation("keccak_kernel_m4_scalar", ALGORITHMS, {"KECCAK_KERNEL": "m4_scalar"}),
    Ablation("ntt_pre_unroll", HAETAE, {"NTT_VARIANT": "m32_l332_vld4"}),
    Ablation("acc_pre_poly_layout", HAETAE, {"ACC_VARIANT": "aftersum_dmax_tight_precenter"}),
    Ablation("fft_pre_unroll", HAETAE, {"FFT_VARIANT": "qrd_soa_l2222_vtbl_fused"}),
    Ablation("sampler_pre_load", HAETAE, {"SAMPLER_VARIANT": "cdt_mve"}),
    Ablation("haetae_scalar_controls", HAETAE, {
        "NTT_VARIANT": "scalar", "ACC_VARIANT": "perterm_d1_centered",
        "FFT_VARIANT": "scalar", "SAMPLER_VARIANT": "scalar",
    }),
    Ablation("mul_outer64", SMAUG, {"MUL_VARIANT": "toom4_outer64"}),
    Ablation("mul_cache_off", SMAUG, {"MUL_CACHE": "0"}),
    Ablation("shift_copy", SMAUG, {"SHIFT_VARIANT": "copy"}),
    Ablation("smaugt_scalar_mul_control", SMAUG, {"MUL_VARIANT": "scalar"}),
)


def command_output(command: list[str]) -> str:
    result = subprocess.run(
        command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    return result.stdout.strip()


def write_json(path: pathlib.Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def source_files() -> list[pathlib.Path]:
    roots = (
        ROOT / "Makefile", ROOT / "README.md", ROOT / "bench", ROOT / "platform",
        ROOT / "scripts",
        ROOT / "reference", ROOT / "optimization", ROOT / "docs",
    )
    suffixes = {".c", ".h", ".S", ".s", ".ld", ".mk", ".py", ".md"}
    result: list[pathlib.Path] = []
    for source_root in roots:
        paths = [source_root] if source_root.is_file() else source_root.rglob("*")
        for path in paths:
            if not path.is_file():
                continue
            if path.name not in ("Makefile", "README.md") and path.suffix not in suffixes:
                continue
            result.append(path)
    return sorted(set(result))


def source_hashes(paths: list[pathlib.Path]) -> dict[str, str]:
    return {
        path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in paths
    }


def archive_sources(output: pathlib.Path, paths: list[pathlib.Path]) -> None:
    with tarfile.open(output / "source-snapshot.tar.gz", "w:gz") as archive:
        for path in paths:
            archive.add(path, arcname=path.relative_to(ROOT).as_posix(), recursive=False)


def capture_metadata(output: pathlib.Path, arguments: argparse.Namespace) -> None:
    sources = source_files()
    metadata = {
        "captured_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "host": platform.platform(),
        "python": sys.version,
        "git_head": command_output(["git", "rev-parse", "HEAD"]),
        "git_status": command_output(["git", "status", "--short"]),
        "arm_gcc": command_output(["arm-none-eabi-gcc", "--version"]).splitlines()[0],
        "arm_gdb": command_output(["arm-none-eabi-gdb", "--version"]).splitlines()[0],
        "binutils": command_output(["arm-none-eabi-size", "--version"]).splitlines()[0],
        "probe_rs": command_output(["probe-rs", "--version"]),
        "make": command_output(["make", "--version"]).splitlines()[0],
        "target": "STM32N657 Arm Cortex-M55",
        "probe_serial": arguments.probe_serial,
        "timer": "DWT CYCCNT",
        "core_clock_hz": 600000000,
        "hclk_hz": 200000000,
        "axisram_clock_hz": 400000000,
        "load_region": "secure AXISRAM 0x34180400..0x343c0000",
        "output_transport": "512 KiB AXISRAM capture buffer read through probe-rs GDB",
        "probe_session": "one persistent probe-rs GDB server per campaign",
        "external_flash_programmed": False,
        "requested_command": arguments.command,
    }
    write_json(output / "metadata.json", metadata)
    write_json(output / "source_sha256.json", source_hashes(sources))
    archive_sources(output, sources)
    (output / "git-diff.patch").write_text(
        command_output(["git", "diff", "--no-ext-diff"]) + "\n", encoding="utf-8"
    )


def probe_selector(serial: str) -> str:
    return f"{PROBE_ID}:{serial}" if serial else PROBE_ID


def unused_ipv6_port() -> int:
    with socket.socket(socket.AF_INET6, socket.SOCK_STREAM) as listener:
        listener.bind(("::1", 0))
        return int(listener.getsockname()[1])


@contextlib.contextmanager
def persistent_probe_session(output: pathlib.Path, serial: str):
    """Keep one USB debug session open across every target run in a campaign."""
    connection = f"[::1]:{unused_ipv6_port()}"
    command = [
        "probe-rs", "gdb", "--chip", "STM32N657", "--protocol", "swd",
        "--probe", probe_selector(serial), "--gdb-connection-string", connection,
    ]
    log_path = output / "probe-gdb-server.log"
    print("+", " ".join(command), flush=True)
    previous_connection = os.environ.get("STM32_GDB_CONNECTION")
    process: subprocess.Popen[str] | None = None
    try:
        with log_path.open("w", encoding="utf-8") as log:
            deadline = time.monotonic() + 120.0
            attempt = 0
            print(
                "Waiting up to 120 seconds to acquire the ST-Link USB interface...",
                flush=True,
            )
            while True:
                attempt += 1
                log.write(f"\n--- probe-rs startup attempt {attempt} ---\n")
                log.flush()
                attempt_offset = log.tell()
                process = subprocess.Popen(
                    command, cwd=ROOT, text=True, stdout=log,
                    stderr=subprocess.STDOUT,
                )
                attempt_deadline = min(deadline, time.monotonic() + 3.0)
                server_output = ""
                while time.monotonic() < attempt_deadline:
                    log.flush()
                    with log_path.open("r", encoding="utf-8", errors="replace") as reader:
                        reader.seek(attempt_offset)
                        server_output = reader.read()
                    if "Firing up GDB stub" in server_output:
                        break
                    if process.poll() is not None:
                        break
                    time.sleep(0.05)
                if "Firing up GDB stub" in server_output:
                    print(f"Acquired ST-Link on attempt {attempt}.", flush=True)
                    break
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=1.0)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
                if time.monotonic() >= deadline:
                    raise RuntimeError(
                        "could not acquire the ST-Link USB interface within 120 seconds; "
                        "last probe-rs output:\n" + server_output
                    )
                time.sleep(0.05)
            os.environ["STM32_GDB_CONNECTION"] = connection
            yield connection
    finally:
        if previous_connection is None:
            os.environ.pop("STM32_GDB_CONNECTION", None)
        else:
            os.environ["STM32_GDB_CONNECTION"] = previous_connection
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


def stream_command(command: list[str], log_path: pathlib.Path) -> int:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    print("+", " ".join(command), flush=True)
    environment = os.environ.copy()
    environment["NO_COLOR"] = "1"
    with log_path.open("w", encoding="utf-8") as log:
        process = subprocess.Popen(
            command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, bufsize=1, env=environment,
        )
        assert process.stdout is not None
        try:
            for line in process.stdout:
                sys.stdout.write(line)
                sys.stdout.flush()
                log.write(line)
                log.flush()
        except KeyboardInterrupt:
            process.terminate()
            process.wait()
            raise
        return process.wait()


def make_arguments(variables: dict[str, str]) -> list[str]:
    return [f"{key}={value}" for key, value in variables.items()]


def archive_build(run_dir: pathlib.Path, variables: dict[str, str]) -> None:
    make_vars = make_arguments(variables)
    elf_text = command_output(["make", "-s", *make_vars, "print-elf"])
    map_text = command_output(["make", "-s", *make_vars, "print-map"])
    elf = ROOT / elf_text.splitlines()[-1]
    map_file = ROOT / map_text.splitlines()[-1]
    if not elf.is_file() or not map_file.is_file():
        raise RuntimeError(f"missing ELF/map after build: {elf}, {map_file}")
    shutil.copy2(elf, run_dir / elf.name)
    shutil.copy2(map_file, run_dir / map_file.name)
    (run_dir / "sections.txt").write_text(
        command_output(["arm-none-eabi-size", "-A", str(elf)]) + "\n",
        encoding="utf-8",
    )
    (run_dir / "size-summary.txt").write_text(
        command_output(["arm-none-eabi-size", str(elf)]) + "\n",
        encoding="utf-8",
    )
    (run_dir / "elf-sections.txt").write_text(
        command_output(["arm-none-eabi-objdump", "-h", str(elf)]) + "\n",
        encoding="utf-8",
    )


def run_make(
    output: pathlib.Path,
    group: str,
    name: str,
    algorithm: str,
    implementation: str,
    iterations: int,
    serial: str,
    extra: dict[str, str] | None = None,
    correctness_gate: bool = False,
) -> None:
    variables = {
        "PLATFORM": "stm32n657",
        "IMPLEMENTATION": implementation,
        "ALGORITHM": algorithm,
        "ITERATIONS": str(iterations),
        "STM32_CLOCK": "600mhz",
    }
    if serial:
        variables["PROBE_SERIAL"] = serial
    if extra:
        variables.update(extra)
    run_dir = output / "runs" / group / name / implementation / algorithm
    run_dir.mkdir(parents=True, exist_ok=True)
    descriptor_path = run_dir / "run.json"
    output_path = run_dir / "output.log"
    if descriptor_path.is_file() and output_path.is_file():
        previous = json.loads(descriptor_path.read_text(encoding="utf-8"))
        previous_output = output_path.read_text(encoding="utf-8", errors="replace")
        artifacts = (
            run_dir / "sections.txt", run_dir / "size-summary.txt",
            run_dir / "elf-sections.txt",
        )
        gates_ok = (not correctness_gate or all(
            token in previous_output
            for token in ("KAT,status=ok", "DIFF,iterations=1000,status=ok")
        ))
        if (previous.get("variables") == variables
                and previous.get("exit_code") == 0
                and previous.get("status_ok") is True
                and "STATUS,ok" in previous_output
                and gates_ok
                and all(path.is_file() for path in artifacts)
                and any(run_dir.glob("*.elf"))
                and any(run_dir.glob("*.map"))):
            print(f"= reuse completed {group}/{name}/{implementation}/{algorithm}",
                  flush=True)
            return
    descriptor = {
        "group": group,
        "name": name,
        "algorithm": algorithm,
        "implementation": implementation,
        "variables": variables,
        "started_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
    }
    write_json(descriptor_path, descriptor)
    command = ["make", "-s", *make_arguments(variables), "run"]
    status = 1
    text = ""
    for target_attempt in range(1, 4):
        status = stream_command(command, output_path)
        text = output_path.read_text(encoding="utf-8", errors="replace")
        if status == 0 and "STATUS,ok" in text:
            break
        if target_attempt < 3:
            failed_log = run_dir / f"failed-attempt-{target_attempt}.log"
            shutil.copy2(output_path, failed_log)
            print(
                f"! target attempt {target_attempt} failed; retrying through the "
                "persistent GDB server",
                flush=True,
            )
            time.sleep(0.5)
    descriptor["finished_utc"] = dt.datetime.now(dt.timezone.utc).isoformat()
    descriptor["exit_code"] = status
    descriptor["status_ok"] = "STATUS,ok" in text
    write_json(descriptor_path, descriptor)
    if status != 0 or not descriptor["status_ok"]:
        raise RuntimeError(f"target run failed: {group}/{name}/{implementation}/{algorithm}")
    if correctness_gate:
        required = ("KAT,status=ok", "DIFF,iterations=1000,status=ok")
        if any(token not in text for token in required):
            raise RuntimeError(f"correctness evidence missing in {run_dir / 'output.log'}")
    archive_build(run_dir, variables)


def check_probe(output: pathlib.Path, serial: str) -> None:
    command = [
        "probe-rs", "read", "--chip", "STM32N657", "--protocol", "swd",
        "--probe", probe_selector(serial),
        "b32", "0xE000ED00", "1",
    ]
    status = stream_command(command, output / "probe-check.log")
    if status != 0:
        probe_log = (output / "probe-check.log").read_text(
            encoding="utf-8", errors="replace"
        )
        if ("interfaces are claimed" in probe_log
                or "IOKit PlugInInterface" in probe_log
                or "USB error" in probe_log):
            raise RuntimeError(
                "ST-Link USB interface is unavailable; close probe/debug tools or "
                "reconnect the board USB cable, then resume the same output directory."
            )
        raise RuntimeError(
            "Cortex-M55 debug access is closed. Set BOOT1 to DEV_BOOT, then press RESET: "
            "STM32N6570-DK=BOOT1 pins 1-3; NUCLEO-N657X0-Q=BOOT1 pins 2-3."
        )


def run_correctness(output: pathlib.Path, args: argparse.Namespace) -> None:
    for algorithm in ALGORITHMS:
        run_make(output, "correctness", "deployment", algorithm, "optimization", 1,
                 args.probe_serial, {"DIFF_TEST": "1"}, correctness_gate=True)


def run_baseline(output: pathlib.Path, args: argparse.Namespace) -> None:
    for implementation in ("reference", "optimization"):
        for algorithm in ALGORITHMS:
            run_make(output, "baseline", "deployment", algorithm, implementation,
                     args.iterations, args.probe_serial)


def run_rejections(output: pathlib.Path, args: argparse.Namespace) -> None:
    for implementation in ("reference", "optimization"):
        for algorithm in HAETAE:
            run_make(output, "rejections", "instrumented", algorithm, implementation,
                     args.rejection_iterations, args.probe_serial,
                     {"REJECTION_COUNT": "1", "STM32_RUN_TIMEOUT": "900"})


def run_profiles(output: pathlib.Path, args: argparse.Namespace) -> None:
    for algorithm in ALGORITHMS:
        run_make(output, "profiles", "deployment", algorithm, "optimization",
                 args.profile_iterations, args.probe_serial, {"PROFILE": "1"})


def run_ablations(output: pathlib.Path, args: argparse.Namespace) -> None:
    for ablation in ABLATIONS:
        if not args.skip_ablation_gates:
            for algorithm in ablation.algorithms:
                variables = dict(ablation.variables)
                variables["DIFF_TEST"] = "1"
                run_make(output, "ablation-gates", ablation.name, algorithm, "optimization", 1,
                         args.probe_serial, variables, correctness_gate=True)
        for algorithm in ablation.algorithms:
            run_make(output, "ablations", ablation.name, algorithm, "optimization",
                     args.iterations, args.probe_serial, ablation.variables)


def analyze(output: pathlib.Path, baseline: pathlib.Path | None = None) -> None:
    command = [
        sys.executable, str(ROOT / "scripts" / "analyze_measurements.py"), str(output),
    ]
    if baseline is not None:
        command.extend(("--baseline", str(baseline)))
    subprocess.run(command, cwd=ROOT, check=True)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "command", choices=("check", "correctness", "baseline", "rejections",
                            "profiles", "ablations", "all"))
    parser.add_argument("--iterations", type=int, default=100)
    parser.add_argument("--rejection-iterations", type=int, default=1000)
    parser.add_argument("--profile-iterations", type=int, default=100)
    parser.add_argument("--probe-serial", default="")
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument(
        "--baseline-output", type=pathlib.Path,
        help="completed baseline campaign used for ablation comparisons",
    )
    parser.add_argument("--skip-ablation-gates", action="store_true")
    arguments = parser.parse_args()
    for value in (arguments.iterations, arguments.rejection_iterations,
                  arguments.profile_iterations):
        if value < 1:
            parser.error("iteration counts must be positive")
    return arguments


def main() -> int:
    args = parse_arguments()
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    output = (args.output or ROOT / "measurements" / "stm32n657" / stamp).resolve()
    output.mkdir(parents=True, exist_ok=True)
    if not (output / "metadata.json").is_file():
        capture_metadata(output, args)
    try:
        if args.command == "check":
            check_probe(output, args.probe_serial)
        else:
            with persistent_probe_session(output, args.probe_serial):
                if args.command in ("correctness", "all"):
                    run_correctness(output, args)
                if args.command in ("baseline", "all"):
                    run_baseline(output, args)
                if args.command in ("rejections", "all"):
                    run_rejections(output, args)
                if args.command in ("profiles", "all"):
                    run_profiles(output, args)
                if args.command in ("ablations", "all"):
                    run_ablations(output, args)
        analyze(output, args.baseline_output)
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        print(f"Partial evidence preserved in {output}", file=sys.stderr)
        return 1
    print(f"Campaign evidence: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

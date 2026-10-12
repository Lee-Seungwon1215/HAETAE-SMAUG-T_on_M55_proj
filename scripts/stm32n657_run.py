#!/usr/bin/env python3
"""Load an STM32N657 ELF into RAM, run it, and recover its RAM log."""

from __future__ import annotations

import argparse
import pathlib
import socket
import struct
import subprocess
import sys
import tempfile
import time


OUTPUT_MAGIC = 0x4D35354F
OUTPUT_COMPLETE = 0x434F4D50
OUTPUT_HEADER = struct.Struct("<6I")


def symbols(elf: pathlib.Path) -> dict[str, int]:
    result = subprocess.run(
        ["arm-none-eabi-nm", "-n", str(elf)], text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
    )
    wanted = {
        "_estack", "Reset_Handler", "platform_exit_halt",
        "__bench_output_start__", "__bench_output_end__",
    }
    found: dict[str, int] = {}
    for line in result.stdout.splitlines():
        columns = line.split()
        if len(columns) == 3 and columns[2] in wanted:
            found[columns[2]] = int(columns[0], 16)
    missing = wanted - found.keys()
    if missing:
        raise RuntimeError(f"missing ELF symbols: {', '.join(sorted(missing))}")
    return found


def unused_ipv6_port() -> int:
    with socket.socket(socket.AF_INET6, socket.SOCK_STREAM) as listener:
        listener.bind(("::1", 0))
        return int(listener.getsockname()[1])


def wait_for_server(process: subprocess.Popen[str], timeout: float) -> str:
    assert process.stdout is not None
    output: list[str] = []
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = process.stdout.readline()
        if line:
            output.append(line)
            if "Firing up GDB stub" in line:
                return "".join(output)
        elif process.poll() is not None:
            break
    raise RuntimeError("probe-rs GDB server did not start:\n" + "".join(output))


def start_server(command: list[str], timeout: float) -> tuple[subprocess.Popen[str], str]:
    failures: list[str] = []
    for attempt in range(1, 4):
        server = subprocess.Popen(
            command, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, bufsize=1,
        )
        try:
            return server, wait_for_server(server, timeout)
        except RuntimeError as error:
            failures.append(f"attempt {attempt}: {error}")
            if server.poll() is None:
                server.terminate()
                try:
                    server.wait(timeout=3.0)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait()
            if attempt < 3:
                time.sleep(1.0)
    raise RuntimeError("probe-rs GDB server failed after 3 attempts:\n" +
                       "\n".join(failures))


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=pathlib.Path)
    parser.add_argument("--probe-rs", default="probe-rs")
    parser.add_argument("--gdb", default="arm-none-eabi-gdb")
    parser.add_argument("--probe", default="0483:3754")
    parser.add_argument(
        "--gdb-connection",
        help="reuse an already-running probe-rs GDB server (for example [::1]:1337)",
    )
    parser.add_argument("--timeout", type=float, default=300.0)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    args.elf = args.elf.resolve()
    if not args.elf.is_file():
        parser.error(f"ELF not found: {args.elf}")
    return args


def main() -> int:
    args = parse_arguments()
    table = symbols(args.elf)
    connection = args.gdb_connection
    server_command: list[str] | None = None
    if connection is None:
        port = unused_ipv6_port()
        connection = f"[::1]:{port}"
        server_command = [
            args.probe_rs, "gdb", "--chip", "STM32N657", "--protocol", "swd",
            "--probe", args.probe, "--gdb-connection-string", connection,
        ]

    with tempfile.TemporaryDirectory(prefix="stm32n657-run-") as directory:
        dump_path = pathlib.Path(directory) / "bench-output.bin"
        server: subprocess.Popen[str] | None = None
        try:
            if server_command is None:
                server_log = f"reused persistent GDB server at {connection}"
            else:
                server, server_log = start_server(
                    server_command, min(args.timeout, 15.0)
                )
            commands = [
                "set pagination off",
                "set confirm off",
                f"target extended-remote {connection}",
                "monitor reset halt",
                "load",
                "set $xpsr = 0x01000000",
                f"set $msp = 0x{table['_estack']:08x}",
                f"set $msp_s = 0x{table['_estack']:08x}",
                f"set $sp = 0x{table['_estack']:08x}",
                f"set $pc = 0x{table['Reset_Handler']:08x}",
                f"hbreak *0x{table['platform_exit_halt']:08x}",
                "continue",
                (f"dump binary memory {dump_path} "
                 f"0x{table['__bench_output_start__']:08x} "
                 f"0x{table['__bench_output_end__']:08x}"),
                # `detach` sends the GDB D packet. A bare `disconnect` closes the
                # socket and makes probe-rs 0.32 exit with "failed to fill whole
                # buffer", which prevents the next ELF from reusing the server.
                "detach",
                "quit",
            ]
            gdb_command = [args.gdb, "-q", "-nx", "-batch", str(args.elf)]
            for command in commands:
                gdb_command.extend(("-ex", command))
            try:
                gdb = subprocess.run(
                    gdb_command, text=True, stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, timeout=args.timeout, check=False,
                )
            except subprocess.TimeoutExpired as error:
                raise RuntimeError(f"target run exceeded {args.timeout:g} seconds") from error
            if gdb.returncode != 0 or not dump_path.is_file():
                raise RuntimeError("GDB RAM run failed:\n" + gdb.stdout + "\n" + server_log)

            raw = dump_path.read_bytes()
            if len(raw) < OUTPUT_HEADER.size:
                raise RuntimeError("RAM output header is truncated\nGDB output:\n" + gdb.stdout)
            magic, capacity, length, overflow, complete, exit_status = (
                OUTPUT_HEADER.unpack_from(raw)
            )
            payload = raw[OUTPUT_HEADER.size:]
            if magic != OUTPUT_MAGIC:
                raise RuntimeError(
                    f"bad RAM output magic: 0x{magic:08x}\nGDB output:\n{gdb.stdout}"
                )
            if capacity > len(payload) or length > capacity:
                raise RuntimeError(
                    f"invalid RAM output bounds: capacity={capacity}, length={length}, "
                    f"dump={len(payload)}"
                )
            payload = payload[:capacity]
            sys.stdout.buffer.write(payload[:length])
            sys.stdout.buffer.flush()
            if overflow != 0:
                raise RuntimeError(f"RAM output overflowed after {length} bytes")
            if complete != OUTPUT_COMPLETE:
                raise RuntimeError(f"target did not mark completion: 0x{complete:08x}")
            if exit_status != 0:
                raise RuntimeError(f"target exit status {exit_status}")
        except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
            print(f"ERROR: {error}", file=sys.stderr)
            return 1
        finally:
            if server is not None and server.poll() is None:
                server.terminate()
                try:
                    server.wait(timeout=3.0)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

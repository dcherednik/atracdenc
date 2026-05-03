#!/usr/bin/env python3
import argparse
import os
import struct
import subprocess
import sys
import wave
from pathlib import Path


def write_wav(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    samples = 2048
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(44100)
        wav.writeframes(struct.pack("<{}h".format(samples), *([0] * samples)))


def run_command(args):
    return subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def decode(data):
    return data.decode("utf-8", errors="replace")


def fail(message, proc=None):
    print(message, file=sys.stderr)
    if proc is not None:
        print("exit code: {}".format(proc.returncode), file=sys.stderr)
        print("stdout:\n{}".format(decode(proc.stdout)), file=sys.stderr)
        print("stderr:\n{}".format(decode(proc.stderr)), file=sys.stderr)
    sys.exit(1)


def check_missing_input(exe, work_dir):
    missing = work_dir / "missing-input.wav"
    out_file = work_dir / "missing-output.oma"
    proc = run_command([
        str(exe),
        "-e", "atrac3",
        "--nostdout",
        "-i", str(missing),
        "-o", str(out_file),
    ])

    if proc.returncode == 0:
        fail("encoding unexpectedly succeeded with a missing input file", proc)

    combined = decode(proc.stdout) + decode(proc.stderr)
    combined_lower = combined.lower()
    if "unsupported sample rate" in combined_lower:
        fail("missing input file was reported as unsupported sample rate", proc)
    if "unable to open input file" not in combined_lower:
        fail("missing input file error does not explain open failure", proc)
    if missing.name not in combined:
        fail("missing input file error does not include the input path", proc)


def check_utf8_input(exe, work_dir):
    in_file = work_dir / "utf8-input-\u00e9-\u5165\u529b-\u0442\u0435\u0441\u0442.wav"
    out_file = work_dir / "utf8-output.oma"
    write_wav(in_file)

    proc = run_command([
        str(exe),
        "-e", "atrac3",
        "--nostdout",
        "-i", str(in_file),
        "-o", str(out_file),
    ])

    if proc.returncode != 0:
        fail("encoding failed with a UTF-8 input filename", proc)
    if not out_file.exists() or os.path.getsize(str(out_file)) == 0:
        fail("encoding with a UTF-8 input filename did not create output", proc)


def check_utf8_output(exe, work_dir, suffix):
    in_file = work_dir / "utf8-output-input.wav"
    out_file = work_dir / ("utf8-output-\u00e9-\u5165\u529b-\u0442\u0435\u0441\u0442" + suffix)
    write_wav(in_file)

    proc = run_command([
        str(exe),
        "-e", "atrac3",
        "--nostdout",
        "-i", str(in_file),
        "-o", str(out_file),
    ])

    if proc.returncode != 0:
        fail("encoding failed with a UTF-8 output filename ({})".format(suffix), proc)
    if not out_file.exists() or os.path.getsize(str(out_file)) == 0:
        fail("encoding with a UTF-8 output filename ({}) did not create output".format(suffix), proc)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True)
    parser.add_argument("--work-dir", required=True)
    parser.add_argument("--case", required=True, choices=[
        "missing-input",
        "utf8-input",
        "utf8-output-oma",
        "utf8-output-at3",
        "utf8-output-rm",
    ])
    args = parser.parse_args()

    exe = Path(args.exe)
    work_dir = Path(args.work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)

    if args.case == "missing-input":
        check_missing_input(exe, work_dir)
    elif args.case == "utf8-input":
        check_utf8_input(exe, work_dir)
    elif args.case == "utf8-output-oma":
        check_utf8_output(exe, work_dir, ".oma")
    elif args.case == "utf8-output-at3":
        check_utf8_output(exe, work_dir, ".at3")
    elif args.case == "utf8-output-rm":
        check_utf8_output(exe, work_dir, ".rm")


if __name__ == "__main__":
    main()

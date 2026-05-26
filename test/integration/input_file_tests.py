#!/usr/bin/env python3
import argparse
import os
import struct
import subprocess
import sys
import wave
from pathlib import Path


UTF8_STEM = "\u00e9-\u5165\u529b-\u0442\u0435\u0441\u0442"


def write_wav(path, samples=2048):
    path.parent.mkdir(parents=True, exist_ok=True)
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


def require_output_file(path, proc, message):
    if not path.exists() or os.path.getsize(str(path)) == 0:
        fail(message, proc)


def encode(exe, in_file, out_file, codec, container=None):
    args = [
        str(exe),
        "-e", codec,
        "--nostdout",
        "-i", str(in_file),
        "-o", str(out_file),
    ]
    if container is not None:
        args.extend(["--container", container])
    return run_command(args)


def decode_atrac1(exe, in_file, out_file):
    return run_command([
        str(exe),
        "-d",
        "--nostdout",
        "-i", str(in_file),
        "-o", str(out_file),
    ])


def check_missing_input(exe, work_dir):
    missing = work_dir / "missing-input.wav"
    out_file = work_dir / "missing-output.oma"
    proc = encode(exe, missing, out_file, "atrac3")

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
    in_file = work_dir / ("utf8-input-" + UTF8_STEM + ".wav")
    out_file = work_dir / "utf8-output.oma"
    write_wav(in_file)

    proc = encode(exe, in_file, out_file, "atrac3")

    if proc.returncode != 0:
        fail("encoding failed with a UTF-8 input filename", proc)
    require_output_file(out_file, proc, "encoding with a UTF-8 input filename did not create output")


def check_utf8_atrac1_input(exe, work_dir):
    in_file = work_dir / ("utf8-input-atrac1-" + UTF8_STEM + ".wav")
    out_file = work_dir / "utf8-input-atrac1-output.aea"
    write_wav(in_file, samples=8192)

    proc = encode(exe, in_file, out_file, "atrac1")

    if proc.returncode != 0:
        fail("ATRAC1 encoding failed with a UTF-8 input filename", proc)
    require_output_file(out_file, proc, "ATRAC1 encoding with a UTF-8 input filename did not create output")


def check_utf8_output(exe, work_dir, suffix):
    in_file = work_dir / "utf8-output-input.wav"
    out_file = work_dir / ("utf8-output-" + UTF8_STEM + suffix)
    write_wav(in_file)

    proc = encode(exe, in_file, out_file, "atrac3")

    if proc.returncode != 0:
        fail("encoding failed with a UTF-8 output filename ({})".format(suffix), proc)
    require_output_file(
        out_file,
        proc,
        "encoding with a UTF-8 output filename ({}) did not create output".format(suffix))


def check_utf8_atrac1_output(exe, work_dir):
    in_file = work_dir / "utf8-output-atrac1-input.wav"
    out_file = work_dir / ("utf8-output-atrac1-" + UTF8_STEM + ".aea")
    write_wav(in_file, samples=8192)

    proc = encode(exe, in_file, out_file, "atrac1")

    if proc.returncode != 0:
        fail("ATRAC1 encoding failed with a UTF-8 output filename", proc)
    require_output_file(out_file, proc, "ATRAC1 encoding with a UTF-8 output filename did not create output")


def create_atrac1_file(exe, in_file, encoded_file):
    write_wav(in_file, samples=8192)
    proc = encode(exe, in_file, encoded_file, "atrac1")
    if proc.returncode != 0:
        fail("failed to create ATRAC1 fixture", proc)
    require_output_file(encoded_file, proc, "ATRAC1 fixture was not created")


def check_utf8_decode_input(exe, work_dir):
    wav_file = work_dir / "utf8-decode-input-source.wav"
    encoded_file = work_dir / ("utf8-decode-input-" + UTF8_STEM + ".aea")
    out_file = work_dir / "utf8-decode-input-output.wav"
    create_atrac1_file(exe, wav_file, encoded_file)

    proc = decode_atrac1(exe, encoded_file, out_file)

    if proc.returncode != 0:
        fail("ATRAC1 decoding failed with a UTF-8 input filename", proc)
    require_output_file(out_file, proc, "ATRAC1 decoding with a UTF-8 input filename did not create output")


def check_utf8_decode_output(exe, work_dir):
    wav_file = work_dir / "utf8-decode-output-source.wav"
    encoded_file = work_dir / "utf8-decode-output-input.aea"
    out_file = work_dir / ("utf8-decode-output-" + UTF8_STEM + ".wav")
    create_atrac1_file(exe, wav_file, encoded_file)

    proc = decode_atrac1(exe, encoded_file, out_file)

    if proc.returncode != 0:
        fail("ATRAC1 decoding failed with a UTF-8 output filename", proc)
    require_output_file(out_file, proc, "ATRAC1 decoding with a UTF-8 output filename did not create output")


def check_explicit_container(exe, work_dir):
    in_file = work_dir / "explicit-container-input.wav"
    write_wav(in_file, samples=8192)

    riff_out = work_dir / "explicit-riff-output.oma"
    proc = encode(exe, in_file, riff_out, "atrac3", container="riff")
    if proc.returncode != 0:
        fail("ATRAC3 encoding failed with explicit RIFF container", proc)
    require_output_file(riff_out, proc, "explicit RIFF container did not create output")
    with riff_out.open("rb") as stream:
        if stream.read(4) != b"RIFF":
            fail("explicit RIFF container did not override the .oma extension", proc)

    raw_out = work_dir / "explicit-raw-output.aea"
    proc = encode(exe, in_file, raw_out, "atrac1", container="raw")
    if proc.returncode != 0:
        fail("ATRAC1 encoding failed with explicit RAW container", proc)
    require_output_file(raw_out, proc, "explicit RAW container did not create output")

    invalid_atrac1 = work_dir / "invalid-atrac1.oma"
    proc = encode(exe, in_file, invalid_atrac1, "atrac1", container="oma")
    if proc.returncode == 0:
        fail("ATRAC1 encoding unexpectedly accepted OMA container", proc)
    if "container oma is not supported for atrac1" not in (decode(proc.stdout) + decode(proc.stderr)).lower():
        fail("ATRAC1 invalid container error did not explain the rejected combination", proc)

    invalid_atrac3plus = work_dir / "invalid-atrac3plus.rm"
    proc = encode(exe, in_file, invalid_atrac3plus, "atrac3plus", container="rm")
    if proc.returncode == 0:
        fail("ATRAC3PLUS encoding unexpectedly accepted RM container", proc)
    if "container rm is not supported for atrac3plus" not in (decode(proc.stdout) + decode(proc.stderr)).lower():
        fail("ATRAC3PLUS invalid container error did not explain the rejected combination", proc)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True)
    parser.add_argument("--work-dir", required=True)
    parser.add_argument("--case", required=True, choices=[
        "missing-input",
        "utf8-input",
        "utf8-input-atrac1",
        "utf8-output-oma",
        "utf8-output-at3",
        "utf8-output-rm",
        "utf8-output-aea",
        "utf8-decode-input",
        "explicit-container",
        "utf8-decode-output",
    ])
    args = parser.parse_args()

    exe = Path(args.exe)
    work_dir = Path(args.work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)

    if args.case == "missing-input":
        check_missing_input(exe, work_dir)
    elif args.case == "utf8-input":
        check_utf8_input(exe, work_dir)
    elif args.case == "utf8-input-atrac1":
        check_utf8_atrac1_input(exe, work_dir)
    elif args.case == "utf8-output-oma":
        check_utf8_output(exe, work_dir, ".oma")
    elif args.case == "utf8-output-at3":
        check_utf8_output(exe, work_dir, ".at3")
    elif args.case == "utf8-output-rm":
        check_utf8_output(exe, work_dir, ".rm")
    elif args.case == "utf8-output-aea":
        check_utf8_atrac1_output(exe, work_dir)
    elif args.case == "utf8-decode-input":
        check_utf8_decode_input(exe, work_dir)
    elif args.case == "utf8-decode-output":
        check_utf8_decode_output(exe, work_dir)
    elif args.case == "explicit-container":
        check_explicit_container(exe, work_dir)


if __name__ == "__main__":
    main()

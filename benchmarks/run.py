#!/usr/bin/env python3
"""Build, validate, measure and regenerate the report with one command."""
import argparse
import datetime as dt
import importlib.metadata
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def command(args, **kwargs):
    return subprocess.run(args, cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repetitions", type=int, default=21)
    args = parser.parse_args()
    if args.repetitions < 3:
        parser.error("--repetitions must be at least 3")
    build = ROOT / "build/benchmark-release"
    output = ROOT / "reports"
    output.mkdir(exist_ok=True)
    command(["cmake", "-S", str(ROOT), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"])
    command(["cmake", "--build", str(build), "--target", "pointer_benchmarks", "pointer_tests", "-j", "4"])
    validation = command(["ctest", "--test-dir", str(build), "--output-on-failure"], capture_output=True, text=True)
    print(validation.stdout, flush=True)
    cache = (build / "CMakeCache.txt").read_text()
    def cached(name):
        for line in cache.splitlines():
            if line.startswith(name + ":"):
                return line.split("=", 1)[1]
        return "unknown"
    compiler = cached("CMAKE_CXX_COMPILER")
    cpu = platform.processor() or platform.machine()
    if sys.platform == "darwin":
        probe = subprocess.run(["sysctl", "-n", "machdep.cpu.brand_string"], capture_output=True, text=True)
        if probe.returncode == 0:
            cpu = probe.stdout.strip()
    metadata = {
        "timestamp": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"),
        "platform": platform.platform(), "cpu": cpu, "architecture": platform.machine(),
        "logical_cpus": os.cpu_count(), "compiler": command([compiler, "--version"], capture_output=True, text=True).stdout.strip(),
        "build_type": "Release", "cxx_flags": cached("CMAKE_CXX_FLAGS"),
        "release_flags": cached("CMAKE_CXX_FLAGS_RELEASE"), "standard": "C++20",
        "sanitizers_in_benchmark": False, "lto": False, "threads": 1,
        "clock": "std::chrono::steady_clock", "warmups": 3,
        "repetitions": args.repetitions, "seed": 20261001,
        "sizes": [1000, 10000, 100000, 1000000],
        "validation": validation.stdout.strip(),
        "python": platform.python_version(),
        "libraries": {name: importlib.metadata.version(name) for name in ("seaborn", "matplotlib", "pandas", "numpy")},
    }
    command([str(build / "pointer_benchmarks"), str(output / "raw.csv"), str(args.repetitions)])
    (output / "environment.json").write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n")
    command([sys.executable, str(ROOT / "benchmarks/report.py")])


if __name__ == "__main__":
    main()

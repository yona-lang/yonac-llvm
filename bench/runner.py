#!/usr/bin/env python3
"""
Yona Benchmark Runner

Discovers .yona benchmarks, compiles with yonac, runs with timing + memory,
optionally compares against C references and across optimization levels.

Usage:
    python3 bench/runner.py                      # run all, -O2
    python3 bench/runner.py core/fibonacci        # specific benchmark
    python3 bench/runner.py --compare-c           # compare vs C
    python3 bench/runner.py --all-opt-levels      # compare O0/O1/O2/O3
    python3 bench/runner.py --json                # machine-readable output
"""

import argparse
import json
import os
import resource
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BENCH_DIR = ROOT / "bench"
YONAC = ROOT / "out" / "build" / "x64-debug-linux" / "yonac"
BUILD_DIR = Path("/tmp/yona_bench")

# Per-benchmark timeout (seconds) — prevents runaway benchmarks
BENCH_TIMEOUT = 10


def find_benchmarks(filter_name=None):
    benchmarks = []
    for category in sorted(BENCH_DIR.iterdir()):
        if not category.is_dir() or category.name in ("reference", "__pycache__"):
            continue
        for yona_file in sorted(category.glob("*.yona")):
            name = f"{category.name}/{yona_file.stem}"
            if filter_name and filter_name not in name:
                continue
            expected_file = yona_file.with_suffix(".expected")
            benchmarks.append({
                "name": name,
                "source": yona_file,
                "expected": expected_file if expected_file.exists() else None,
                "category": category.name,
            })
    return benchmarks


def compile_yona(source, opt_level, exe_path):
    cmd = [str(YONAC), f"-O{opt_level}", "-o", str(exe_path), str(source)]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        return result.returncode == 0
    except subprocess.TimeoutExpired:
        return False


def compile_c(c_file, exe_path):
    cmd = ["cc", "-O2", "-lm", "-o", str(exe_path), str(c_file)]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
        return result.returncode == 0
    except subprocess.TimeoutExpired:
        return False


def run_once(exe):
    """Run executable once. Returns (output, time_ms, peak_rss_kb) or None on failure."""
    try:
        # Use /usr/bin/time for memory measurement
        cmd = ["/usr/bin/time", "-v", str(exe)]
        start = time.perf_counter_ns()
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=BENCH_TIMEOUT)
        elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000
        output = result.stdout.strip()

        # Parse peak RSS from /usr/bin/time -v output
        peak_rss_kb = 0
        for line in result.stderr.split("\n"):
            if "Maximum resident set size" in line:
                try:
                    peak_rss_kb = int(line.strip().split()[-1])
                except (ValueError, IndexError):
                    pass
                break

        return output, elapsed_ms, peak_rss_kb
    except subprocess.TimeoutExpired:
        return None
    except Exception:
        return None


def run_benchmark(exe, iterations):
    """Run N times, return stats dict or None."""
    times = []
    rss_values = []
    output = ""
    for i in range(iterations):
        result = run_once(exe)
        if result is None:
            return None
        out, ms, rss = result
        if i == 0:
            output = out
        times.append(ms)
        rss_values.append(rss)
    return {
        "output": output,
        "times_ms": times,
        "min_ms": min(times),
        "avg_ms": sum(times) / len(times),
        "max_ms": max(times),
        "peak_rss_kb": max(rss_values) if rss_values else 0,
    }


def check_correctness(bench, output):
    if bench["expected"] is None:
        return True
    expected = bench["expected"].read_text().strip()
    return output == expected


def fmt_time(ms):
    if ms < 1:
        return f"{ms:.3f}ms"
    if ms < 100:
        return f"{ms:.1f}ms"
    return f"{ms:.0f}ms"


def fmt_rss(kb):
    if kb == 0:
        return "?"
    if kb < 1024:
        return f"{kb}KB"
    return f"{kb / 1024:.1f}MB"


def run_suite(benchmarks, opt_level, iterations, compare_c=False):
    """Run all benchmarks at one opt level. Returns list of result dicts."""
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    results = []
    current_category = None

    for bench in benchmarks:
        if bench["category"] != current_category:
            current_category = bench["category"]
            print(f"\n  [{current_category}]")

        stem = bench["source"].stem
        exe = BUILD_DIR / f"{stem}_O{opt_level}"

        if not compile_yona(bench["source"], opt_level, exe):
            print(f"    {stem:.<28} COMPILE FAILED")
            results.append({"name": bench["name"], "status": "compile_error"})
            continue

        stats = run_benchmark(exe, iterations)
        if stats is None:
            print(f"    {stem:.<28} TIMEOUT")
            results.append({"name": bench["name"], "status": "timeout"})
            continue

        if not check_correctness(bench, stats["output"]):
            expected = bench["expected"].read_text().strip() if bench["expected"] else "?"
            print(f"    {stem:.<28} WRONG (expected {expected}, got {stats['output'][:40]})")
            results.append({"name": bench["name"], "status": "wrong_output"})
            continue

        r = {
            "name": bench["name"],
            "status": "ok",
            "opt_level": opt_level,
            **stats,
        }

        line = f"    {stem:.<28} {fmt_time(stats['avg_ms']):>8}  {fmt_rss(stats['peak_rss_kb']):>8}"

        if compare_c:
            c_file = BENCH_DIR / "reference" / (stem + ".c")
            c_exe = BUILD_DIR / f"ref_{stem}"
            if c_file.exists() and compile_c(c_file, c_exe):
                c_stats = run_benchmark(c_exe, iterations)
                if c_stats:
                    ratio = stats["avg_ms"] / c_stats["avg_ms"] if c_stats["avg_ms"] > 0 else 0
                    r["c_avg_ms"] = c_stats["avg_ms"]
                    r["c_rss_kb"] = c_stats["peak_rss_kb"]
                    r["ratio"] = ratio
                    line += f"  C: {fmt_time(c_stats['avg_ms']):>8} {fmt_rss(c_stats['peak_rss_kb']):>6}  {ratio:.1f}x"

        print(line)
        results.append(r)

    return results


def get_git_info():
    """Get current git commit hash and subject."""
    try:
        sha = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            capture_output=True, text=True, cwd=ROOT
        ).stdout.strip()
        subject = subprocess.run(
            ["git", "log", "-1", "--format=%s"],
            capture_output=True, text=True, cwd=ROOT
        ).stdout.strip()
        dirty = subprocess.run(
            ["git", "diff", "--quiet"], cwd=ROOT
        ).returncode != 0
        return {"sha": sha, "subject": subject, "dirty": dirty}
    except Exception:
        return {"sha": "unknown", "subject": "", "dirty": False}


def save_results(all_results, git_info):
    """Append benchmark results to bench/history.jsonl."""
    import datetime
    history_file = BENCH_DIR / "history.jsonl"
    entry = {
        "timestamp": datetime.datetime.now().isoformat(),
        "git": git_info,
        "results": all_results,
    }
    with open(history_file, "a") as f:
        f.write(json.dumps(entry, default=str) + "\n")
    print(f"\n  Results saved to {history_file.relative_to(ROOT)}")


def main():
    parser = argparse.ArgumentParser(description="Yona Benchmark Runner")
    parser.add_argument("filter", nargs="?", help="Filter by benchmark name")
    parser.add_argument("-n", "--iterations", type=int, default=3)
    parser.add_argument("-O", "--opt-level", type=int, default=2)
    parser.add_argument("--compare-c", action="store_true")
    parser.add_argument("--all-opt-levels", action="store_true", help="Run O0, O1, O2, O3")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--save", action="store_true", help="Save results to bench/history.jsonl")
    args = parser.parse_args()

    if not YONAC.exists():
        print(f"Error: yonac not found at {YONAC}")
        print("Run: cmake --build --preset build-debug-linux")
        sys.exit(1)

    benchmarks = find_benchmarks(args.filter)
    if not benchmarks:
        print("No benchmarks found.")
        sys.exit(1)

    all_results = {}

    if args.all_opt_levels:
        for level in [0, 1, 2, 3]:
            header = f"O{level}"
            print(f"\n{'=' * 60}")
            print(f"  Optimization Level: -O{level}  ({args.iterations} iterations)")
            print(f"{'=' * 60}")
            all_results[f"O{level}"] = run_suite(benchmarks, level, args.iterations, args.compare_c)
    else:
        print(f"Yona Benchmarks — -O{args.opt_level}, {args.iterations} iterations")
        print(f"{'=' * 60}")
        all_results[f"O{args.opt_level}"] = run_suite(benchmarks, args.opt_level, args.iterations, args.compare_c)

    # Summary
    print(f"\n{'=' * 60}")
    for key, results in all_results.items():
        ok = sum(1 for r in results if r.get("status") == "ok")
        print(f"  {key}: {ok}/{len(results)} passed")

    if args.json:
        print("\n" + json.dumps(all_results, indent=2, default=str))

    if args.save:
        save_results(all_results, get_git_info())


if __name__ == "__main__":
    main()

#!/usr/bin/env python3

import argparse
import json
import math
import sys
from pathlib import Path


def load_metric_map(path: Path, metric: str, aggregate: str) -> dict[str, float]:
    data = json.loads(path.read_text())
    collected: dict[str, list[float]] = {}

    for row in data.get("benchmarks", []):
        if row.get("aggregate_name") not in (None, aggregate):
            continue
        if row.get("run_type") == "aggregate" and row.get("aggregate_name") != aggregate:
            continue

        name = row.get("name")
        value = row.get(metric)
        if not isinstance(name, str) or not isinstance(value, (int, float)):
            continue
        if name.endswith("_BigO") or name.endswith("_RMS"):
            continue
        collected.setdefault(name, []).append(float(value))

    return {name: sum(values) / len(values) for name, values in collected.items() if values}


def format_percent(delta: float) -> str:
    return f"{delta * 100:+.2f}%"


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare two Google Benchmark JSON outputs.")
    parser.add_argument("baseline", type=Path)
    parser.add_argument("contender", type=Path)
    parser.add_argument("--metric", default="cpu_time", choices=["cpu_time", "real_time"])
    parser.add_argument("--aggregate", default="mean", help="Aggregate to use when aggregate rows are present.")
    parser.add_argument("--warn-threshold", type=float, default=0.05)
    parser.add_argument("--fail-threshold", type=float, default=0.10)
    args = parser.parse_args()

    baseline = load_metric_map(args.baseline, args.metric, args.aggregate)
    contender = load_metric_map(args.contender, args.metric, args.aggregate)

    common_names = sorted(set(baseline) & set(contender))
    if not common_names:
        print("No overlapping benchmarks found.", file=sys.stderr)
        return 2

    print(f"Comparing metric: {args.metric}")
    print(f"Warning threshold: {args.warn_threshold * 100:.2f}%")
    print(f"Failure threshold: {args.fail_threshold * 100:.2f}%")
    print()
    print(f"{'Benchmark':60} {'Baseline':>12} {'Current':>12} {'Delta':>10} {'Status':>8}")
    print("-" * 108)

    exit_code = 0
    for name in common_names:
        old_value = baseline[name]
        new_value = contender[name]
        if old_value == 0:
            delta = math.inf if new_value != 0 else 0.0
        else:
            delta = (new_value - old_value) / abs(old_value)

        status = "OK"
        if delta >= args.fail_threshold:
            status = "FAIL"
            exit_code = 1
        elif delta >= args.warn_threshold:
            status = "WARN"

        print(f"{name:60} {old_value:12.2f} {new_value:12.2f} {format_percent(delta):>10} {status:>8}")

    missing_from_contender = sorted(set(baseline) - set(contender))
    missing_from_baseline = sorted(set(contender) - set(baseline))

    if missing_from_contender:
        print()
        print("Missing from contender:")
        for name in missing_from_contender:
            print(f"  {name}")

    if missing_from_baseline:
        print()
        print("Missing from baseline:")
        for name in missing_from_baseline:
            print(f"  {name}")

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Parse ImmersiveRenderStats log lines into one CSV benchmark row."""

import argparse
import csv
import re
import statistics
import sys


LINE_RE = re.compile(
    r"ImmersiveRenderStats: "
    r"mode=(?P<output>\w+) "
    r"projection=(?P<projection>\w+) "
    r"size=(?P<domemaster>\d+) "
    r"cubemap=(?P<cubemap>\d+) "
    r"fps=(?P<fps>[0-9.]+) "
    r"frame=(?P<frame>[0-9.]+)ms "
    r"last=(?P<last>[0-9.]+)ms "
    r"p95=(?P<p95>[0-9.]+)ms "
    r"p99=(?P<p99>[0-9.]+)ms "
    r"cubemap=(?P<cubemap_ms>[0-9.]+)ms "
    r"projection=(?P<projection_ms>[0-9.]+)ms "
    r"syphon=(?P<syphon_ms>[0-9.]+)ms "
    r"skipped=(?P<skipped>\d+)"
    r"(?: capture=(?P<capture_ms>[0-9.]+)ms "
    r"output=(?P<output_ms>[0-9.]+)ms)?"
)


def percentile(values, pct):
    if not values:
        return 0.0
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    index = round((len(ordered) - 1) * pct)
    return ordered[index]


def average(values):
    return statistics.fmean(values) if values else 0.0


def parse_log(path):
    rows = []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            match = LINE_RE.search(line)
            if not match:
                continue
            item = match.groupdict()
            rows.append({
                "output": item["output"],
                "projection": item["projection"],
                "domemaster": int(item["domemaster"]),
                "cubemap": int(item["cubemap"]),
                "fps": float(item["fps"]),
                "frame": float(item["frame"]),
                "last": float(item["last"]),
                "rolling_p95": float(item["p95"]),
                "rolling_p99": float(item["p99"]),
                "cubemap_ms": float(item["cubemap_ms"]),
                "projection_ms": float(item["projection_ms"]),
                "syphon_ms": float(item["syphon_ms"]),
                "capture_ms": float(item["capture_ms"] or item["cubemap_ms"]),
                "output_ms": float(item["output_ms"] or item["syphon_ms"]),
                "skipped": int(item["skipped"]),
            })
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--log", required=True)
    parser.add_argument("--mode", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--duration", required=True)
    parser.add_argument("--skip-samples", type=int, default=0)
    parser.add_argument("--notes", default="")
    parser.add_argument("--csv", required=True)
    parser.add_argument("--append", action="store_true")
    args = parser.parse_args()

    rows = parse_log(args.log)
    measured = rows[args.skip_samples:] if args.skip_samples < len(rows) else []

    fieldnames = [
        "mode", "cubemap", "domemaster", "output", "duration",
        "avg_fps", "avg_ms", "p50_ms", "p95_ms", "p99_ms",
        "min_fps", "max_fps", "cubemap_ms", "projection_ms",
        "syphon_ms", "capture_ms", "output_ms", "dropped", "samples",
        "notes"
    ]

    if measured:
        last = [row["last"] for row in measured if row["last"] > 0.0]
        cubemap_times = [row["cubemap_ms"] for row in measured]
        projection_times = [row["projection_ms"] for row in measured]
        syphon_times = [row["syphon_ms"] for row in measured]
        capture_times = [row["capture_ms"] for row in measured]
        output_times = [row["output_ms"] for row in measured]
        skipped_values = [row["skipped"] for row in measured]
        avg_ms = average(last)
        max_ms = max(last) if last else 0.0
        min_ms = min(last) if last else 0.0
        dropped = (max(skipped_values) - min(skipped_values)
                   if skipped_values else 0)
        row = {
            "mode": args.mode,
            "cubemap": measured[-1]["cubemap"],
            "domemaster": measured[-1]["domemaster"],
            "output": args.output,
            "duration": args.duration,
            "avg_fps": f"{1000.0 / avg_ms:.2f}" if avg_ms > 0 else "0.00",
            "avg_ms": f"{avg_ms:.2f}",
            "p50_ms": f"{percentile(last, 0.50):.2f}",
            "p95_ms": f"{percentile(last, 0.95):.2f}",
            "p99_ms": f"{percentile(last, 0.99):.2f}",
            "min_fps": f"{1000.0 / max_ms:.2f}" if max_ms > 0 else "0.00",
            "max_fps": f"{1000.0 / min_ms:.2f}" if min_ms > 0 else "0.00",
            "cubemap_ms": f"{average(cubemap_times):.2f}",
            "projection_ms": f"{average(projection_times):.2f}",
            "syphon_ms": f"{average(syphon_times):.2f}",
            "capture_ms": f"{average(capture_times):.2f}",
            "output_ms": f"{average(output_times):.2f}",
            "dropped": dropped,
            "samples": len(measured),
            "notes": args.notes,
        }
    else:
        row = {
            "mode": args.mode,
            "cubemap": "",
            "domemaster": "",
            "output": args.output,
            "duration": args.duration,
            "avg_fps": "not measured",
            "avg_ms": "not measured",
            "p50_ms": "not measured",
            "p95_ms": "not measured",
            "p99_ms": "not measured",
            "min_fps": "not measured",
            "max_fps": "not measured",
            "cubemap_ms": "not measured",
            "projection_ms": "not measured",
            "syphon_ms": "not measured",
            "capture_ms": "not measured",
            "output_ms": "not measured",
            "dropped": "not measured",
            "samples": 0,
            "notes": args.notes or "no ImmersiveRenderStats samples",
        }

    mode = "a" if args.append else "w"
    write_header = not args.append
    with open(args.csv, mode, newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        if write_header:
            writer.writeheader()
        writer.writerow(row)

    print(",".join(str(row[name]) for name in fieldnames))


if __name__ == "__main__":
    main()

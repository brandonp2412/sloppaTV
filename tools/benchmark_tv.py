#!/usr/bin/env python3
"""Repeatable real-device sloppaTV vs Jellyfin Android TV benchmark.

Measures process-cold Activity launch time, settled PSS/RSS, idle CPU snapshot,
and SurfaceFlinger present cadence during rapid DPAD focus movement.
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
import subprocess
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path


@dataclass(frozen=True)
class App:
    name: str
    package: str
    activity: str
    layer_contains: str
    setup_keys: tuple[str, ...] = ()


APPS = (
    App(
        "sloppaTV",
        "nz.presley.sloppatv",
        "nz.presley.sloppatv/nz.presley.sloppatv.SloppaNativeActivity",
        "SloppaNativeActivity",
        ("20",),  # Move from the top toolbar into the first Home media row.
    ),
    App(
        "Jellyfin",
        "org.jellyfin.androidtv",
        "org.jellyfin.androidtv/.ui.startup.StartupActivity",
        "MainActivity",
        ("20",),  # Move from top navigation into the first Home media row.
    ),
)


def adb(serial: str, *args: str, capture: bool = True) -> str:
    command = ["adb", "-s", serial, *args]
    if capture:
        return subprocess.check_output(command, text=True, stderr=subprocess.STDOUT)
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return ""


def force_stop(serial: str, app: App) -> None:
    adb(serial, "shell", "am", "force-stop", app.package, capture=False)


def launch(serial: str, app: App, wait: bool = False) -> str:
    args = ["shell", "am", "start"]
    if wait:
        args.append("-W")
    args.extend(("-n", app.activity))
    return adb(serial, *args)


def percentile(values: list[float], fraction: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(len(ordered) * fraction))]


def startup_benchmark(serial: str, runs: int) -> dict[str, dict[str, float | list[int]]]:
    samples: dict[str, list[int]] = {app.name: [] for app in APPS}
    for run in range(runs):
        order = APPS if run % 2 == 0 else tuple(reversed(APPS))
        for app in order:
            force_stop(serial, app)
            time.sleep(0.35)
            output = launch(serial, app, wait=True)
            match = re.search(r"TotalTime: (\d+)", output)
            if not match:
                # Some Android/Jellyfin launch paths report only WaitTime even for a
                # genuinely cold Activity start. Use it as the common end-to-end
                # Activity-manager latency metric when TotalTime is omitted.
                match = re.search(r"WaitTime: (\d+)", output)
            if match:
                samples[app.name].append(int(match.group(1)))
            time.sleep(0.6)

    result = {}
    for app in APPS:
        values = samples[app.name]
        if len(values) != runs:
            raise RuntimeError(
                f"Expected {runs} cold-launch samples for {app.name}, captured {len(values)}: {values}"
            )
        result[app.name] = {
            "samples_ms": values,
            "median_ms": statistics.median(values),
            "mean_ms": round(statistics.mean(values), 1),
            "min_ms": min(values),
            "max_ms": max(values),
        }
    return result


def settled_memory(serial: str, app: App, settle_seconds: float) -> dict[str, int]:
    force_stop(serial, app)
    launch(serial, app)
    time.sleep(settle_seconds)
    output = adb(serial, "shell", "dumpsys", "meminfo", app.package)

    def extract(pattern: str) -> int:
        match = re.search(pattern, output)
        return int(match.group(1)) if match else 0

    return {
        "total_pss_kb": extract(r"TOTAL PSS:\s+(\d+)"),
        "total_rss_kb": extract(r"TOTAL RSS:\s+(\d+)"),
        "java_heap_kb": extract(r"Java Heap:\s+(\d+)"),
        "native_heap_kb": extract(r"Native Heap:\s+(\d+)"),
    }


def idle_cpu(serial: str, app: App) -> float:
    pid = adb(serial, "shell", "pidof", app.package).strip()
    if not pid:
        return 0.0
    output = adb(serial, "shell", "top", "-b", "-n", "1", "-p", pid)
    for line in output.splitlines():
        if line.strip().startswith(pid + " "):
            parts = line.split()
            # Android top places %CPU after the process-state column.
            for index, part in enumerate(parts):
                if part in {"R", "S", "D", "T", "Z"} and index + 1 < len(parts):
                    try:
                        return float(parts[index + 1])
                    except ValueError:
                        break
    return 0.0


def active_layer(serial: str, app: App) -> str:
    layers = adb(serial, "shell", "dumpsys", "SurfaceFlinger", "--list").splitlines()
    candidates = [
        line
        for line in layers
        if app.package + "/" in line
        and app.layer_contains in line
        and "ActivityRecord" not in line
        and "InputSink" not in line
    ]
    if not candidates:
        raise RuntimeError(f"No SurfaceFlinger layer found for {app.name}")
    buffer_layers = [line for line in candidates if line.startswith("TID:")]
    return (buffer_layers or candidates)[-1]


def histogram_values(line: str) -> list[float]:
    values: list[float] = []
    for bucket, count in re.findall(r"(\d+)ms=(\d+)", line):
        values.extend([float(bucket)] * int(count))
    return values


def navigation_timestats(serial: str, app: App) -> dict[str, float | int]:
    output = adb(serial, "shell", "dumpsys", "SurfaceFlinger", "--timestats", "-dump")
    blocks = output.split("displayRefreshRate = ")
    matches: list[tuple[int, str]] = []
    for block in blocks:
        if app.package not in block:
            continue
        total_match = re.search(r"totalFrames = (\d+)", block)
        if total_match:
            matches.append((int(total_match.group(1)), block))
    if not matches:
        raise RuntimeError(f"No SurfaceFlinger TimeStats layer found for {app.name}")
    total_frames, block = max(matches, key=lambda entry: entry[0])
    dropped_match = re.search(r"droppedFrames = (\d+)", block)
    fps_match = re.search(r"averageFPS = ([0-9.]+)", block)
    histogram_match = re.search(r"present2present histogram is as below:\n([^\n]+)", block)
    values = histogram_values(histogram_match.group(1)) if histogram_match else []
    if not values:
        raise RuntimeError(f"No SurfaceFlinger TimeStats present histogram for {app.name}")
    return {
        "frames": total_frames,
        "median_interval_ms": round(statistics.median(values), 2),
        "p95_interval_ms": round(percentile(values, 0.95), 2),
        "over_20ms_pct": round(sum(value > 20 for value in values) * 100 / len(values), 1),
        "over_33_4ms_pct": round(sum(value > 33.4 for value in values) * 100 / len(values), 1),
        "dropped_frames": int(dropped_match.group(1)) if dropped_match else 0,
        "average_fps": round(float(fps_match.group(1)), 2) if fps_match else 0.0,
    }


def navigation_benchmark(serial: str, app: App, nav_events: int, settle_seconds: float) -> dict[str, float | int]:
    force_stop(serial, app)
    launch(serial, app)
    time.sleep(settle_seconds)
    for key in app.setup_keys:
        adb(serial, "shell", "input", "keyevent", key, capture=False)
    time.sleep(0.25)

    layer = active_layer(serial, app)
    adb(serial, "shell", "dumpsys", "SurfaceFlinger", "--timestats", "-enable", capture=False)
    adb(serial, "shell", "dumpsys", "SurfaceFlinger", "--timestats", "-clear", capture=False)
    adb(serial, "shell", "dumpsys", "SurfaceFlinger", "--latency-clear", layer, capture=False)
    for index in range(nav_events):
        adb(serial, "shell", "input", "keyevent", "22" if index % 2 == 0 else "21", capture=False)

    output = adb(serial, "shell", "dumpsys", "SurfaceFlinger", "--latency", layer)
    actual = []
    for line in output.splitlines()[1:]:
        columns = line.split()
        if len(columns) >= 2 and columns[1].isdigit() and int(columns[1]) > 0:
            actual.append(int(columns[1]))
    intervals = [(right - left) / 1_000_000 for left, right in zip(actual, actual[1:]) if right > left]
    if not intervals:
        return navigation_timestats(serial, app)
    return {
        "frames": len(actual),
        "median_interval_ms": round(statistics.median(intervals), 2),
        "p95_interval_ms": round(percentile(intervals, 0.95), 2),
        "over_20ms_pct": round(sum(value > 20 for value in intervals) * 100 / len(intervals), 1),
        "over_33_4ms_pct": round(sum(value > 33.4 for value in intervals) * 100 / len(intervals), 1),
        "dropped_frames": 0,
        "average_fps": round(1000.0 / statistics.mean(intervals), 2) if intervals else 0.0,
    }


def aggregate_samples(samples: list[dict[str, float | int]]) -> dict[str, float]:
    if not samples:
        return {}
    return {
        key: round(float(statistics.median([float(sample[key]) for sample in samples])), 2)
        for key in samples[0]
    }


def percentage_lower(candidate: float, baseline: float) -> float:
    if baseline <= 0:
        return 0.0
    return round((baseline - candidate) * 100.0 / baseline, 1)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True, help="ADB serial for the Android TV test target")
    parser.add_argument("--runs", type=int, default=10, help="Cold-launch samples per app")
    parser.add_argument("--memory-runs", type=int, default=1, help="Settled-memory samples per app")
    parser.add_argument("--nav-runs", type=int, default=1, help="DPAD SurfaceFlinger samples per app")
    parser.add_argument("--nav-events", type=int, default=80)
    parser.add_argument("--settle-seconds", type=float, default=6.0)
    parser.add_argument("--json-out", type=Path, help="Write machine-readable benchmark evidence")
    parser.add_argument(
        "--final-suite",
        action="store_true",
        help="Use the PERFORMANCE.md final-gate sample counts: 20 startup, 5 memory and 5 navigation runs",
    )
    args = parser.parse_args()
    if args.final_suite:
        args.runs = 20
        args.memory_runs = 5
        args.nav_runs = 5
    if args.runs < 1 or args.memory_runs < 1 or args.nav_runs < 1 or args.nav_events < 1:
        parser.error("all run/event counts must be at least 1")
    if args.settle_seconds < 0:
        parser.error("--settle-seconds cannot be negative")

    adb(args.serial, "shell", "input", "keyevent", "224", capture=False)
    print("STARTUP")
    startup = startup_benchmark(args.serial, args.runs)
    for app in APPS:
        data = startup[app.name]
        print(f"{app.name}: median={data['median_ms']}ms mean={data['mean_ms']}ms samples={data['samples_ms']}")

    print("\nSETTLED MEMORY / IDLE CPU")
    memory_samples: dict[str, list[dict[str, int]]] = {app.name: [] for app in APPS}
    cpu_samples: dict[str, list[float]] = {app.name: [] for app in APPS}
    for run in range(args.memory_runs):
        order = APPS if run % 2 == 0 else tuple(reversed(APPS))
        for app in order:
            memory_samples[app.name].append(settled_memory(args.serial, app, args.settle_seconds))
            cpu_samples[app.name].append(idle_cpu(args.serial, app))
    memory = {name: aggregate_samples(samples) for name, samples in memory_samples.items()}
    cpu = {name: round(float(statistics.median(samples)), 2) for name, samples in cpu_samples.items()}
    for app in APPS:
        data = memory[app.name]
        print(
            f"{app.name}: PSS={data['total_pss_kb']}KB RSS={data['total_rss_kb']}KB "
            f"JavaHeap={data['java_heap_kb']}KB NativeHeap={data['native_heap_kb']}KB idleCPU={cpu[app.name]:.1f}% "
            f"samples={len(memory_samples[app.name])}"
        )

    print("\nDPAD SURFACEFLINGER")
    navigation_samples: dict[str, list[dict[str, float | int]]] = {app.name: [] for app in APPS}
    for run in range(args.nav_runs):
        order = APPS if run % 2 == 0 else tuple(reversed(APPS))
        for app in order:
            navigation_samples[app.name].append(
                navigation_benchmark(args.serial, app, args.nav_events, args.settle_seconds)
            )
    navigation = {name: aggregate_samples(samples) for name, samples in navigation_samples.items()}
    for app in APPS:
        data = navigation[app.name]
        print(
            f"{app.name}: median={data['median_interval_ms']}ms p95={data['p95_interval_ms']}ms "
            f">20ms={data['over_20ms_pct']}% >33.4ms={data['over_33_4ms_pct']}% "
            f"runs={len(navigation_samples[app.name])}"
        )

    sloppa = "sloppaTV"
    jellyfin = "Jellyfin"
    comparison = {
        "startup_median_lower_pct": percentage_lower(float(startup[sloppa]["median_ms"]), float(startup[jellyfin]["median_ms"])),
        "startup_mean_lower_pct": percentage_lower(float(startup[sloppa]["mean_ms"]), float(startup[jellyfin]["mean_ms"])),
        "pss_lower_pct": percentage_lower(float(memory[sloppa]["total_pss_kb"]), float(memory[jellyfin]["total_pss_kb"])),
        "rss_lower_pct": percentage_lower(float(memory[sloppa]["total_rss_kb"]), float(memory[jellyfin]["total_rss_kb"])),
        "java_heap_lower_pct": percentage_lower(float(memory[sloppa]["java_heap_kb"]), float(memory[jellyfin]["java_heap_kb"])),
        "navigation_p95_lower_pct": percentage_lower(float(navigation[sloppa]["p95_interval_ms"]), float(navigation[jellyfin]["p95_interval_ms"])),
        "navigation_over_20ms_lower_pct": percentage_lower(float(navigation[sloppa]["over_20ms_pct"]), float(navigation[jellyfin]["over_20ms_pct"])),
    }
    print("\nCOMPARISON")
    for key, value in comparison.items():
        print(f"{key}={value}%")

    if args.json_out:
        payload = {
            "captured_at": datetime.now(timezone.utc).isoformat(),
            "parameters": {
                "runs": args.runs,
                "memory_runs": args.memory_runs,
                "nav_runs": args.nav_runs,
                "nav_events": args.nav_events,
                "settle_seconds": args.settle_seconds,
                "final_suite": args.final_suite,
            },
            "apps": [asdict(app) for app in APPS],
            "startup": startup,
            "memory": memory,
            "memory_samples": memory_samples,
            "idle_cpu_pct": cpu,
            "idle_cpu_samples_pct": cpu_samples,
            "navigation": navigation,
            "navigation_samples": navigation_samples,
            "comparison": comparison,
        }
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        print(f"\nWrote {args.json_out}")


if __name__ == "__main__":
    main()

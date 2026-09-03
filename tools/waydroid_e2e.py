#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import statistics
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ARTIFACTS = ROOT / "artifacts" / "e2e-waydroid"
DEFAULT_PACKAGE = "app.sloppatv"
DEFAULT_COMPONENT = "app.sloppatv/app.sloppatv.SloppaNativeActivity"
SERIAL = ""
PACKAGE = DEFAULT_PACKAGE
COMPONENT = DEFAULT_COMPONENT

LOG_TOKENS = (
    "sloppaTV",
    "ExoPlayer",
    "MediaCodec",
    "CCodec",
    "libass",
    "PlaybackInfo",
    "HTTP 4",
    "HTTP 5",
    "FATAL",
    "ANR",
    "surface",
)
FATAL_PATTERNS = (
    re.compile(r"FATAL EXCEPTION", re.IGNORECASE),
    re.compile(r"Fatal signal", re.IGNORECASE),
    re.compile(r"native crash", re.IGNORECASE),
)


def adb(*args: str, capture: bool = False, timeout: float = 30.0) -> str:
    command = ["adb", "-s", SERIAL, *args]
    result = subprocess.run(
        command,
        check=True,
        text=True,
        capture_output=capture,
        timeout=timeout,
    )
    return result.stdout.strip() if capture else ""


def model_matches_target(model: str, target: str) -> bool:
    if target == "waydroid":
        return "WayDroid" in model
    if target == "google-tv-streamer":
        return model.strip() == "Google TV Streamer"
    if target == "android-tv-emulator":
        normalized = model.strip().lower()
        return normalized.startswith("sdk_") or "aosp tv" in normalized
    return False


def verify_target(target: str) -> None:
    model = adb("shell", "getprop", "ro.product.model", capture=True).strip()
    if not model_matches_target(model, target):
        raise SystemExit(f"Refusing device {SERIAL}: model is {model!r}, expected target {target!r}")
    size = adb("shell", "wm", "size", capture=True)
    if "1920x1080" not in size:
        raise SystemExit(f"Target is not configured for a 1920x1080 UI surface: {size}")
    ARTIFACTS.mkdir(parents=True, exist_ok=True)


def key(name: str) -> None:
    key_name = name if name.startswith("KEYCODE_") else f"KEYCODE_{name.upper()}"
    adb("shell", "input", "keyevent", key_name)


def launch() -> None:
    adb("shell", "am", "start", "-n", COMPONENT)


def restart() -> None:
    adb("shell", "am", "force-stop", PACKAGE)
    launch()


def process_pid() -> str:
    output = adb("shell", "pidof", PACKAGE, capture=True).strip()
    return output.split()[0] if output else ""


def require_running() -> str:
    pid = process_pid()
    if not pid:
        raise RuntimeError(f"{PACKAGE} is not running")
    return pid


def require_playback_session() -> None:
    sessions = adb("shell", "dumpsys", "media_session", capture=True, timeout=60.0)
    if f"{PACKAGE}/sloppaTV" not in sessions:
        raise RuntimeError("sloppaTV playback is not active; open a title before running this player acceptance command")


def capture(name: str) -> Path:
    remote = f"/sdcard/{name}.png"
    local = ARTIFACTS / f"{name}.png"
    adb("shell", "screencap", "-p", remote)
    adb("pull", remote, str(local))
    return local


def png_dimensions(path: Path) -> tuple[int, int]:
    header = path.read_bytes()[:24]
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"invalid PNG screenshot: {path}")
    return struct.unpack(">II", header[16:24])


def screenshot_manifest_entry(path: Path) -> dict[str, int | str]:
    width, height = png_dimensions(path)
    return {
        "file": path.name,
        "width": width,
        "height": height,
        "bytes": path.stat().st_size,
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
    }


def load_screenshot_suite(path: Path) -> dict:
    suite = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(suite, dict) or not isinstance(suite.get("name"), str):
        raise ValueError("screenshot suite must have a name")
    steps = suite.get("steps")
    if not isinstance(steps, list) or not steps:
        raise ValueError("screenshot suite must have at least one step")
    allowed = {"launch", "restart", "key", "capture", "wait"}
    for index, step in enumerate(steps):
        if not isinstance(step, dict) or step.get("action") not in allowed:
            raise ValueError(f"unsupported screenshot step {index}")
        wait_seconds = step.get("wait_seconds", 0)
        if not isinstance(wait_seconds, (int, float)) or not 0 <= wait_seconds <= 30:
            raise ValueError(f"invalid wait_seconds in screenshot step {index}")
        if step["action"] == "key" and not re.fullmatch(r"[A-Z0-9_]+", str(step.get("key", ""))):
            raise ValueError(f"invalid key in screenshot step {index}")
        if step["action"] == "capture" and not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,63}", str(step.get("name", ""))):
            raise ValueError(f"invalid capture name in screenshot step {index}")
    return suite


def screenshot_suite(path: Path) -> Path:
    suite = load_screenshot_suite(path)
    screenshots: list[dict[str, int | str]] = []
    for step in suite["steps"]:
        action = step["action"]
        if action == "launch":
            launch()
        elif action == "restart":
            restart()
        elif action == "key":
            key(step["key"])
        elif action == "capture":
            screenshot = capture(step["name"])
            width, height = png_dimensions(screenshot)
            if (width, height) != (1920, 1080):
                raise RuntimeError(f"unexpected screenshot dimensions {width}x{height}: {screenshot}")
            screenshots.append(screenshot_manifest_entry(screenshot))
        wait_seconds = float(step.get("wait_seconds", 0))
        if action == "wait":
            wait_seconds = float(step.get("wait_seconds", 1))
        if wait_seconds:
            time.sleep(wait_seconds)
    if not screenshots:
        raise RuntimeError("screenshot suite produced no screenshots")
    manifest = {
        "suite": suite["name"],
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "serial": SERIAL,
        "package": PACKAGE,
        "screenshots": screenshots,
    }
    manifest_path = ARTIFACTS / "screenshots.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {manifest_path}")
    audit_logs("screenshots")
    return manifest_path


def roi_diff(a: Path, b: Path) -> float:
    # Pillow is only required when image evidence is actually captured. Keeping
    # this import local lets host-policy CI test the non-image harness helpers
    # without adding an unrelated Python dependency to the Android build job.
    from PIL import Image, ImageChops, ImageStat

    ia = Image.open(a).convert("RGB").crop((0, 120, 1920, 780))
    ib = Image.open(b).convert("RGB").crop((0, 120, 1920, 780))
    diff = ImageChops.difference(ia, ib)
    stat = ImageStat.Stat(diff)
    return sum(stat.mean) / 3.0


def pair(prefix: str, wait: float) -> float:
    a = capture(f"{prefix}-a")
    time.sleep(wait)
    b = capture(f"{prefix}-b")
    value = roi_diff(a, b)
    print(f"{prefix}: roi_mean_abs_diff={value:.2f} wait={wait:.1f}s")
    return value


def filtered_log_lines() -> list[str]:
    logs = adb("logcat", "-d", "-v", "time", capture=True, timeout=60.0)
    return [
        line
        for line in logs.splitlines()
        if any(token.lower() in line.lower() for token in LOG_TOKENS)
    ]


def filtered_logs(name: str) -> Path:
    lines = filtered_log_lines()
    log_path = ARTIFACTS / f"{name}.log"
    log_path.write_text("\n".join(lines[-1200:]) + "\n", encoding="utf-8")
    print(f"wrote {log_path}")
    return log_path


def fatal_lines(lines: list[str]) -> list[str]:
    result: list[str] = []
    for line in lines:
        if PACKAGE not in line and "sloppaTV" not in line:
            continue
        if re.search(rf"ANR in\s+{re.escape(PACKAGE)}", line, re.IGNORECASE) or any(
            pattern.search(line) for pattern in FATAL_PATTERNS
        ):
            result.append(line)
    return result


def audit_logs(name: str) -> None:
    path = filtered_logs(name)
    lines = path.read_text(encoding="utf-8").splitlines()
    fatals = fatal_lines(lines)
    if fatals:
        raise RuntimeError("fatal Android log entries detected:\n" + "\n".join(fatals[-20:]))
    print("log audit: no sloppaTV fatal exception/native-signal/ANR entries")


def memory_snapshot() -> dict[str, int | float | str]:
    pid = require_running()
    mem = adb("shell", "dumpsys", "meminfo", PACKAGE, capture=True)

    def extract(pattern: str) -> int:
        match = re.search(pattern, mem)
        return int(match.group(1)) if match else 0

    cpu = 0.0
    top = adb("shell", "top", "-b", "-n", "1", "-p", pid, capture=True)
    for line in top.splitlines():
        if not line.strip().startswith(pid + " "):
            continue
        parts = line.split()
        for index, value in enumerate(parts):
            if value in {"R", "S", "D", "T", "Z"} and index + 1 < len(parts):
                try:
                    cpu = float(parts[index + 1])
                except ValueError:
                    cpu = 0.0
                break
    return {
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "pid": pid,
        "total_pss_kb": extract(r"TOTAL PSS:\s+(\d+)"),
        "total_rss_kb": extract(r"TOTAL RSS:\s+(\d+)"),
        "java_heap_kb": extract(r"Java Heap:\s+(\d+)"),
        "native_heap_kb": extract(r"Native Heap:\s+(\d+)"),
        "cpu_pct": cpu,
    }


def wonder_core() -> None:
    adb("logcat", "-c")
    require_running()
    require_playback_session()
    key("DPAD_UP")
    time.sleep(0.4)
    capture("wonder-controls")
    key("DPAD_DOWN")
    pair("wonder-motion", 4.0)

    key("DPAD_CENTER")
    time.sleep(0.6)
    pair("wonder-paused", 3.0)
    key("DPAD_CENTER")
    time.sleep(1.0)
    pair("wonder-resumed", 3.0)

    key("DPAD_RIGHT")
    time.sleep(2.0)
    pair("wonder-seek-forward", 3.0)
    key("DPAD_LEFT")
    time.sleep(2.0)
    pair("wonder-seek-back", 3.0)

    for direction in ("RIGHT", "LEFT", "RIGHT", "LEFT", "RIGHT", "LEFT", "RIGHT", "RIGHT", "LEFT"):
        key(f"DPAD_{direction}")
        time.sleep(0.18)
    time.sleep(2.0)
    pair("wonder-rapid-seek", 3.0)
    audit_logs("wonder-core")


def planet_core() -> None:
    adb("logcat", "-c")
    require_running()
    require_playback_session()
    capture("planet-skip-before")
    key("DPAD_CENTER")
    time.sleep(2.0)
    pair("planet-after-skip", 4.0)
    time.sleep(10.0)
    pair("planet-after-skip-late", 4.0)

    key("DPAD_RIGHT")
    time.sleep(1.5)
    pair("planet-postskip-seek-forward", 3.0)
    key("DPAD_LEFT")
    time.sleep(1.5)
    pair("planet-postskip-seek-back", 3.0)
    for direction in ("RIGHT", "RIGHT", "LEFT", "RIGHT", "LEFT", "LEFT", "RIGHT"):
        key(f"DPAD_{direction}")
        time.sleep(0.2)
    time.sleep(1.5)
    pair("planet-postskip-repeated-seek", 3.0)

    key("DPAD_CENTER")
    time.sleep(0.5)
    pair("planet-postskip-paused", 2.5)
    key("DPAD_CENTER")
    time.sleep(0.8)
    pair("planet-postskip-resumed", 3.0)
    audit_logs("planet-core")


def lifecycle() -> None:
    adb("logcat", "-c")
    require_running()
    before = memory_snapshot()
    pair("lifecycle-before-home", 2.0)
    key("HOME")
    time.sleep(2.0)
    launch()
    time.sleep(2.0)
    after_return_diff = pair("lifecycle-after-return", 3.0)
    after = memory_snapshot()
    evidence = {
        "before": before,
        "after": after,
        "after_return_motion_diff": after_return_diff,
    }
    path = ARTIFACTS / "lifecycle.json"
    path.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {path}")
    audit_logs("lifecycle")


def action_view(item_id: str) -> None:
    if not re.fullmatch(r"[0-9a-fA-F-]{32,36}", item_id):
        raise SystemExit("--item-id must be a Jellyfin 32-hex ID or hyphenated UUID")
    adb("logcat", "-c")
    adb("shell", "am", "start", "-a", "android.intent.action.VIEW", "-d", item_id, "-n", COMPONENT)
    time.sleep(2.5)
    capture("runtime-view")
    audit_logs("runtime-view")


def action_search(query: str) -> None:
    adb("logcat", "-c")
    adb(
        "shell",
        "am",
        "start",
        "-a",
        "android.intent.action.SEARCH",
        "--es",
        "query",
        query,
        "-n",
        COMPONENT,
    )
    time.sleep(2.5)
    capture("runtime-search")
    audit_logs("runtime-search")


def media_session() -> None:
    require_running()
    output = adb("shell", "dumpsys", "media_session", capture=True, timeout=60.0)
    path = ARTIFACTS / "media-session.txt"
    path.write_text(output, encoding="utf-8")
    print(f"wrote {path}")
    relevant = [line.strip() for line in output.splitlines() if PACKAGE in line or "sloppaTV" in line]
    print("\n".join(relevant[:80]) if relevant else "no sloppaTV media-session lines")


def soak_summary(samples: list[dict[str, int | float | str]]) -> dict[str, int | float]:
    if not samples:
        return {}
    window = min(3, len(samples))

    def median_value(key: str, subset: list[dict[str, int | float | str]]) -> int:
        return int(round(statistics.median(float(sample.get(key, 0)) for sample in subset)))

    first = samples[:window]
    last = samples[-window:]
    baseline_pss = median_value("total_pss_kb", first)
    final_pss = median_value("total_pss_kb", last)
    baseline_rss = median_value("total_rss_kb", first)
    final_rss = median_value("total_rss_kb", last)

    def growth_percent(final: int, baseline: int) -> float:
        return round((final - baseline) * 100.0 / baseline, 1) if baseline > 0 else 0.0

    return {
        "sample_count": len(samples),
        "baseline_pss_kb": baseline_pss,
        "final_pss_kb": final_pss,
        "pss_growth_kb": final_pss - baseline_pss,
        "pss_growth_pct": growth_percent(final_pss, baseline_pss),
        "peak_pss_kb": max(int(sample.get("total_pss_kb", 0)) for sample in samples),
        "baseline_rss_kb": baseline_rss,
        "final_rss_kb": final_rss,
        "rss_growth_kb": final_rss - baseline_rss,
        "rss_growth_pct": growth_percent(final_rss, baseline_rss),
        "peak_rss_kb": max(int(sample.get("total_rss_kb", 0)) for sample in samples),
    }


def soak(seconds: int, sample_seconds: int) -> None:
    if seconds < sample_seconds or sample_seconds < 1:
        raise SystemExit("soak duration must be >= sample interval >= 1 second")
    adb("logcat", "-c")
    require_running()
    started = time.monotonic()
    samples: list[dict[str, int | float | str]] = []
    while True:
        elapsed = time.monotonic() - started
        sample = memory_snapshot()
        sample["elapsed_seconds"] = round(elapsed, 1)
        samples.append(sample)
        print(
            f"soak t={elapsed:.0f}s PSS={sample['total_pss_kb']}KB "
            f"RSS={sample['total_rss_kb']}KB CPU={sample['cpu_pct']}%"
        )
        if elapsed >= seconds:
            break
        time.sleep(min(sample_seconds, max(0.0, seconds - elapsed)))

    summary = soak_summary(samples)
    evidence = {
        "serial": SERIAL,
        "package": PACKAGE,
        "duration_seconds": seconds,
        "sample_seconds": sample_seconds,
        "summary": summary,
        "samples": samples,
    }
    print(
        f"soak summary PSS growth={summary.get('pss_growth_kb', 0)}KB "
        f"({summary.get('pss_growth_pct', 0.0)}%) RSS growth={summary.get('rss_growth_kb', 0)}KB "
        f"({summary.get('rss_growth_pct', 0.0)}%)"
    )
    path = ARTIFACTS / "soak.json"
    path.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {path}")
    capture("soak-final")
    audit_logs("soak")


def main() -> None:
    global SERIAL, PACKAGE, COMPONENT, ARTIFACTS
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True, help="ADB serial for the selected Android TV test target")
    parser.add_argument(
        "--target",
        choices=("waydroid", "google-tv-streamer", "android-tv-emulator"),
        default="waydroid",
        help="Explicit target identity guard; physical TV use must be opted into",
    )
    parser.add_argument("--package", default=DEFAULT_PACKAGE)
    parser.add_argument("--component", default=DEFAULT_COMPONENT)
    sub = parser.add_subparsers(dest="command", required=True)

    p_key = sub.add_parser("key")
    p_key.add_argument("keys", nargs="+")
    p_cap = sub.add_parser("capture")
    p_cap.add_argument("name")
    p_pair = sub.add_parser("pair")
    p_pair.add_argument("prefix")
    p_pair.add_argument("wait", type=float)
    p_screenshots = sub.add_parser("screenshots")
    p_screenshots.add_argument("--suite", type=Path, required=True)
    sub.add_parser("wonder-core")
    sub.add_parser("planet-core")
    sub.add_parser("lifecycle")
    p_view = sub.add_parser("view")
    p_view.add_argument("--item-id", required=True)
    p_search = sub.add_parser("search")
    p_search.add_argument("--query", required=True)
    sub.add_parser("media-session")
    p_audit = sub.add_parser("audit-logs")
    p_audit.add_argument("--name", default="manual-audit")
    p_soak = sub.add_parser("soak")
    p_soak.add_argument("--seconds", type=int, default=1800)
    p_soak.add_argument("--sample-seconds", type=int, default=60)

    args = parser.parse_args()
    SERIAL = args.serial
    PACKAGE = args.package
    COMPONENT = args.component
    ARTIFACTS = ROOT / "artifacts" / (
        "e2e-physical-tv"
        if args.target == "google-tv-streamer"
        else "ci-screenshots"
        if args.target == "android-tv-emulator"
        else "e2e-waydroid"
    )

    verify_target(args.target)
    if args.command == "key":
        for value in args.keys:
            key(value)
            time.sleep(0.2)
    elif args.command == "capture":
        print(capture(args.name))
    elif args.command == "pair":
        pair(args.prefix, args.wait)
    elif args.command == "screenshots":
        screenshot_suite(args.suite)
    elif args.command == "wonder-core":
        wonder_core()
    elif args.command == "planet-core":
        planet_core()
    elif args.command == "lifecycle":
        lifecycle()
    elif args.command == "view":
        action_view(args.item_id)
    elif args.command == "search":
        action_search(args.query)
    elif args.command == "media-session":
        media_session()
    elif args.command == "audit-logs":
        audit_logs(args.name)
    elif args.command == "soak":
        soak(args.seconds, args.sample_seconds)


if __name__ == "__main__":
    main()

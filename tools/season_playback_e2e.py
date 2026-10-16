#!/usr/bin/env python3
"""Exercise an explicitly selected season on a physical TV's current account.

Produces screenshots for human visual review alongside audio, progress, pause,
resume and seek evidence. A successful automated run still requires opening the
screenshots to confirm the expected episode has visible, changing video.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import tempfile
import time
import urllib.request
from pathlib import Path
from typing import Any

from offline_playback_e2e import (
    PAUSED,
    PLAYING,
    adb,
    audio_proof,
    capture,
    component,
    key,
    session_snapshot,
    wait_position_delta,
    wait_state,
)


def prepare_seek_helper(serial: str) -> None:
    root = Path(__file__).resolve().parents[1]
    sdk = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
    if not sdk:
        props = root / "local.properties"
        sdk = next(
            (line.split("=", 1)[1].strip() for line in props.read_text().splitlines() if line.startswith("sdk.dir=")),
            None,
        )
    if not sdk:
        raise RuntimeError("Set ANDROID_HOME to compile the ADB seek helper")
    android_jar = sorted(Path(sdk).glob("platforms/*/android.jar"))[-1]
    d8 = sorted(Path(sdk).glob("build-tools/*/d8"))[-1]
    with tempfile.TemporaryDirectory(prefix="sloppa-adb-seek-") as temp:
        subprocess.run(
            [
                "javac",
                "-source",
                "8",
                "-target",
                "8",
                "-cp",
                str(android_jar),
                "-d",
                temp,
                str(root / "tools/AdbMediaSeek.java"),
            ],
            check=True,
            capture_output=True,
        )
        dex = Path(temp) / "adb-media-seek.jar"
        subprocess.run(
            [
                str(d8),
                "--min-api",
                "26",
                "--lib",
                str(android_jar),
                "--output",
                str(dex),
                str(Path(temp) / "AdbMediaSeek.class"),
            ],
            check=True,
            capture_output=True,
        )
        adb(serial, "push", str(dex), "/data/local/tmp/adb-media-seek.jar")


def seek(serial: str, package: str, position_ms: int) -> None:
    if not re.fullmatch(r"[a-zA-Z0-9_.]+", package):
        raise ValueError("Invalid package name")
    adb(
        serial,
        "shell",
        f"CLASSPATH=/data/local/tmp/adb-media-seek.jar app_process /system/bin AdbMediaSeek {package} {position_ms}",
    )


def wait_actual_progress(
    serial: str, package: str, start_ms: int, delta_ms: int, samples: list[dict], timeout: float = 60
) -> dict:
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        last = session_snapshot(serial, package)
        samples.append({"t": round(time.time(), 3), "session": last})
        if last and last["state"] == PLAYING and last["raw_position_ms"] >= start_ms + delta_ms:
            return last
        time.sleep(0.5)
    raise RuntimeError(f"Actual playback clock did not advance {delta_ms} ms: {start_ms} -> {last}")


def run_episode(serial: str, package: str, item: dict, output: Path) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    samples: list[dict] = []
    result: dict[str, Any] = {"id": item["Id"], "name": item["Name"], "episode": item["IndexNumber"], "pass": False}
    try:
        adb(serial, "shell", "am", "force-stop", package)
        # Keep the device's existing log buffers intact; collect just this PID.
        adb(
            serial,
            "shell",
            "am",
            "start",
            "-a",
            "android.intent.action.VIEW",
            "-d",
            f"sloppatv://item/{item['Id']}",
            "-n",
            component(package),
        )
        time.sleep(3)
        key(serial, "DPAD_CENTER")
        started = time.monotonic()
        first = wait_state(serial, package, {PLAYING}, 115, samples)
        result["startup_seconds"] = round(time.monotonic() - started, 2)
        time.sleep(6)
        capture(serial, output / "playing-a.png")
        a = session_snapshot(serial, package)
        if not a:
            raise RuntimeError("No session during opening playback")
        wait_actual_progress(serial, package, int(a["raw_position_ms"]), 10000, samples)
        capture(serial, output / "playing-b.png")
        b = session_snapshot(serial, package)
        result["playing"] = [first, a, b]
        audio = audio_proof(serial, package)
        result["audio"] = audio
        if not audio["codec_initialized"] or not audio["output_active"]:
            raise RuntimeError(f"audio output unproven: {audio}")
        key(serial, "MEDIA_PAUSE")
        paused = wait_state(serial, package, {PAUSED}, 8, samples)
        time.sleep(2)
        held = session_snapshot(serial, package)
        if not held or abs(held["position_ms"] - paused["position_ms"]) > 450:
            raise RuntimeError(f"pause did not hold: {paused} -> {held}")
        key(serial, "MEDIA_PLAY")
        resumed = wait_state(serial, package, {PLAYING}, 8, samples)
        later = wait_position_delta(serial, package, int(resumed["position_ms"]), 1500, 20, samples)
        key(serial, "MEDIA_FAST_FORWARD")
        sought = wait_position_delta(serial, package, int(later["position_ms"]), 6000, 40, samples)
        time.sleep(8)
        capture(serial, output / "after-seek.png")
        after_seek = session_snapshot(serial, package)
        if (
            not after_seek
            or after_seek["state"] != PLAYING
            or after_seek["raw_position_ms"] <= sought["raw_position_ms"]
        ):
            raise RuntimeError(f"seek did not resume: {sought} -> {after_seek}")
        result["controls"] = {
            "paused": paused,
            "held": held,
            "resumed": resumed,
            "sought": sought,
            "after_seek": after_seek,
        }
        duration_ms = int(item.get("RunTimeTicks", 0)) // 10000
        if duration_ms < 180000:
            raise RuntimeError("Episode runtime is missing or too short for deep seek checks")
        result["deep_seeks"] = []
        for label, target in (("middle", duration_ms // 2), ("ending", duration_ms - 90000)):
            seek(serial, package, target)
            deadline = time.monotonic() + 65
            reached = None
            while time.monotonic() < deadline:
                reached = session_snapshot(serial, package)
                if (
                    reached
                    and reached["state"] == PLAYING
                    and target + 1000 < reached["raw_position_ms"] < target + 30000
                ):
                    break
                time.sleep(1)
            else:
                raise RuntimeError(f"{label} seek did not reach {target}: {reached}")
            time.sleep(6)
            capture(serial, output / f"{label}-a.png")
            before = session_snapshot(serial, package)
            if not before:
                raise RuntimeError(f"No session at {label}")
            wait_actual_progress(serial, package, int(before["raw_position_ms"]), 8000, samples)
            capture(serial, output / f"{label}-b.png")
            after = session_snapshot(serial, package)
            stage_audio = audio_proof(serial, package)
            if not stage_audio["output_active"]:
                raise RuntimeError(f"{label} audio output inactive")
            result["deep_seeks"].append(
                {"stage": label, "target_ms": target, "before": before, "after": after, "audio": stage_audio}
            )
        result["pass"] = True
    except Exception as exc:
        result["error"] = str(exc)
        capture(serial, output / "failure.png")
    finally:
        pid = adb(serial, "shell", "pidof", package).strip()
        if pid:
            logs = adb(serial, "logcat", "-d", "--pid", pid.split()[0], "-v", "threadtime")
            # URLs can contain authentication query strings. Keep only the
            # app's structured playback summaries in shareable evidence.
            lines = [
                line
                for line in logs.splitlines()
                if re.search(
                    r"PlaybackInfo |Playback target:|Playback telemetry |Embedded playback requested|core\[", line
                )
            ]
            (output / "playback.log").write_text("\n".join(lines) + "\n")
        result["samples"] = samples
        (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
        key(serial, "BACK")
    return result


def restore_user_data(session: dict, item: dict) -> None:
    original = item.get("UserData", {})
    fields = ("PlaybackPositionTicks", "PlayCount", "Played", "LastPlayedDate")
    body = {field: original[field] for field in fields if field in original}
    request = urllib.request.Request(
        session["server"].rstrip("/") + f"/UserItems/{item['Id']}/UserData",
        data=json.dumps(body).encode(),
        headers={"X-Emby-Token": session["token"], "Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        response.read()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--package", default="app.sloppatv")
    parser.add_argument("--episodes", type=Path, required=True, help="Jellyfin episode objects as a JSON array")
    parser.add_argument(
        "--session",
        type=Path,
        help="Native session JSON; restore the input episode objects' original watch state after each test",
    )
    parser.add_argument("--season", type=int, default=1)
    parser.add_argument("--only", type=int, nargs="*")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    model = adb(args.serial, "shell", "getprop", "ro.product.model").strip()
    if model != "Google TV Streamer":
        raise SystemExit(f"Expected Google TV Streamer, found {model}")
    items = [
        i
        for i in json.loads(args.episodes.read_text())
        if i.get("ParentIndexNumber") == args.season and (not args.only or i["IndexNumber"] in args.only)
    ]
    if not items:
        raise SystemExit("No matching episodes")
    session = json.loads(args.session.read_text()) if args.session else None
    prepare_seek_helper(args.serial)
    results = []
    for item in items:
        print(f"Starting S{args.season}E{item['IndexNumber']}: {item['Name']}", flush=True)
        result = run_episode(
            args.serial, args.package, item, args.output / f"s{args.season:02}e{item['IndexNumber']:02}"
        )
        if session:
            time.sleep(2)
            restore_user_data(session, item)
            result["watch_state_restored"] = True
            (args.output / f"s{args.season:02}e{item['IndexNumber']:02}" / "result.json").write_text(
                json.dumps(result, indent=2) + "\n"
            )
        results.append(result)
        print(
            json.dumps({k: v for k, v in result.items() if k in {"episode", "pass", "error", "startup_seconds"}}),
            flush=True,
        )
        (args.output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
        if not result["pass"]:
            raise SystemExit(1)


if __name__ == "__main__":
    main()

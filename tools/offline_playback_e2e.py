#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import subprocess
import time
from datetime import UTC, datetime
from pathlib import Path

from PIL import Image, ImageChops, ImageStat
from tv_remote_safety import check_tv_remote

PLAYING = 3
PAUSED = 2
BUFFERING = 6


def adb(serial: str, *args: str, timeout: float = 45) -> str:
    cp = subprocess.run(
        ["adb", "-s", serial, *args],
        check=True,
        text=True,
        capture_output=True,
        timeout=timeout,
    )
    return cp.stdout


def key(serial: str, name: str) -> None:
    code = name if name.startswith("KEYCODE_") else f"KEYCODE_{name}"
    adb(serial, "shell", "input", "keyevent", code)


def component(package: str) -> str:
    return f"{package}/app.sloppatv.SloppaNativeActivity"


def session_snapshot(serial: str, package: str) -> dict[str, int | float] | None:
    session_text = adb(serial, "shell", "dumpsys", "media_session", timeout=60)
    uptime_text = adb(serial, "shell", "cat", "/proc/uptime", timeout=15)
    try:
        elapsed_realtime_ms = int(float(uptime_text.split()[0]) * 1000)
    except (IndexError, ValueError):
        return None
    match = re.search(
        rf"(?s)\b{re.escape(package)}/sloppaTV\b.*?"
        r"state=PlaybackState \{state=(?:[A-Z_]+\()?(\d+)\)?"
        r".*?position=(-?\d+).*?speed=([0-9.+-]+).*?updated=(\d+)",
        session_text,
    )
    if not match:
        return None
    state = int(match.group(1))
    raw_position_ms = int(match.group(2))
    speed = float(match.group(3))
    updated_ms = int(match.group(4))
    position_ms = raw_position_ms
    if state == PLAYING and speed > 0:
        position_ms += int(max(0, elapsed_realtime_ms - updated_ms) * speed)
    return {
        "state": state,
        "position_ms": position_ms,
        "raw_position_ms": raw_position_ms,
        "updated_ms": updated_ms,
        "speed": speed,
    }


def wait_state(
    serial: str,
    package: str,
    states: set[int],
    timeout_seconds: float,
    samples: list[dict[str, object]],
) -> dict[str, int | float]:
    deadline = time.monotonic() + timeout_seconds
    last: dict[str, int | float] | None = None
    while time.monotonic() < deadline:
        last = session_snapshot(serial, package)
        samples.append({"t": round(time.time(), 3), "session": last})
        if last is not None and int(last["state"]) in states:
            return last
        time.sleep(0.2)
    raise RuntimeError(f"media session did not reach {sorted(states)}; last={last}")


def wait_position_delta(
    serial: str,
    package: str,
    start_ms: int,
    delta_ms: int,
    timeout_seconds: float,
    samples: list[dict[str, object]],
) -> dict[str, int | float]:
    deadline = time.monotonic() + timeout_seconds
    last: dict[str, int | float] | None = None
    while time.monotonic() < deadline:
        last = session_snapshot(serial, package)
        samples.append({"t": round(time.time(), 3), "session": last})
        if last is not None and int(last["state"]) == PLAYING and int(last["position_ms"]) >= start_ms + delta_ms:
            return last
        time.sleep(0.2)
    raise RuntimeError(f"media session position did not advance by {delta_ms}ms from {start_ms}; last={last}")


def capture(serial: str, path: Path) -> None:
    cp = subprocess.run(
        ["adb", "-s", serial, "exec-out", "screencap", "-p"],
        check=True,
        capture_output=True,
        timeout=45,
    )
    if not cp.stdout.startswith(b"\x89PNG\r\n\x1a\n"):
        raise RuntimeError("invalid screenshot")
    path.write_bytes(cp.stdout)


def roi_diff(a: Path, b: Path) -> float:
    ia = Image.open(a).convert("RGB")
    ib = Image.open(b).convert("RGB")
    w, h = ia.size
    box = (0, int(h * 0.1), w, int(h * 0.78))
    stat = ImageStat.Stat(ImageChops.difference(ia.crop(box), ib.crop(box)))
    return sum(stat.mean) / 3.0


def package_uid(serial: str, package: str) -> int:
    text = adb(serial, "shell", "cmd", "package", "list", "packages", "-U", package)
    match = re.search(r"\buid:(\d+)", text)
    if not match:
        raise RuntimeError(f"cannot resolve UID for {package}")
    return int(match.group(1))


def audio_proof(serial: str, package: str) -> dict[str, object]:
    logs = adb(serial, "logcat", "-d", "-v", "brief", timeout=60)
    telemetry = re.findall(r"Playback telemetry .*?audio=([^\s]+)", logs)
    codec_ok = any(value not in {"unknown", "none", "(null)"} for value in telemetry)
    uid = package_uid(serial, package)
    pid_text = adb(serial, "shell", "pidof", package, timeout=15).strip()
    pid = int(pid_text.split()[0]) if pid_text else -1
    flinger = adb(serial, "shell", "dumpsys", "media.audio_flinger", timeout=60)
    uid_lines = [line.strip() for line in flinger.splitlines() if re.search(rf"\b{uid}\b", line)]
    active_track_lines = [
        line.strip() for line in flinger.splitlines() if pid > 0 and re.search(rf"^\s*\d+\s+yes\s+{pid}\s+", line)
    ]
    return {
        "uid": uid,
        "pid": pid,
        "telemetry_audio": telemetry[-8:],
        "codec_initialized": codec_ok,
        "output_active": bool(active_track_lines),
        "audio_flinger_uid_lines": uid_lines[-12:],
        "audio_flinger_active_uid_lines": active_track_lines[-12:],
    }


def login(serial: str, package: str, server_url: str) -> None:
    adb(serial, "shell", "am", "force-stop", package)
    adb(serial, "shell", "am", "start", "-n", component(package))
    time.sleep(2)
    for action, value, wait in [
        ("key", "DPAD_CENTER", 0.7),
        ("text", server_url, 0.7),
        ("key", "BACK", 0.5),
        ("key", "DPAD_DOWN", 0.1),
        ("key", "DPAD_CENTER", 0.5),
        ("text", "viewer", 0.5),
        ("key", "BACK", 0.4),
        ("key", "DPAD_DOWN", 0.1),
        ("key", "DPAD_CENTER", 0.5),
        ("text", "demo", 0.5),
        ("key", "BACK", 0.4),
        ("key", "DPAD_DOWN", 0.1),
        ("key", "DPAD_CENTER", 2.5),
    ]:
        if action == "key":
            key(serial, value)
        else:
            adb(serial, "shell", "input", "text", value)
        time.sleep(wait)


def open_item(serial: str, package: str, item_id: str) -> None:
    # Each scenario must start from a clean process/media-session state.
    # Package data is preserved so fixture login remains intact.
    adb(serial, "shell", "am", "force-stop", package)
    adb(serial, "logcat", "-c")
    adb(
        serial,
        "shell",
        "am",
        "start",
        "-a",
        "android.intent.action.VIEW",
        "-d",
        f"sloppatv://item/{item_id}",
        "-n",
        component(package),
    )
    time.sleep(1.5)
    key(serial, "DPAD_CENTER")


def strict_playback(
    serial: str,
    package: str,
    item_id: str,
    scenario: str,
    artifacts: Path,
) -> dict[str, object]:
    samples: list[dict[str, object]] = []
    open_item(serial, package, item_id)
    result: dict[str, object] = {"scenario": scenario, "item_id": item_id}

    if scenario == "error":
        deadline = time.monotonic() + 15
        saw_playing = False
        observed: list[int | None] = []
        while time.monotonic() < deadline:
            snap = session_snapshot(serial, package)
            state = int(snap["state"]) if snap else None
            observed.append(state)
            saw_playing |= state == PLAYING
            time.sleep(0.25)
        result["observed_states"] = observed
        if saw_playing:
            raise RuntimeError("HTTP 503 fixture falsely reached PLAYING")
        result["pass"] = True
        return result

    observed_buffering = False
    if scenario in {"stall", "resume_stall"}:
        deadline = time.monotonic() + 25
        first_playing: dict[str, int | float] | None = None
        while time.monotonic() < deadline:
            snap = session_snapshot(serial, package)
            samples.append({"t": round(time.time(), 3), "session": snap})
            if snap:
                state = int(snap["state"])
                observed_buffering |= state == BUFFERING
                if state == PLAYING and first_playing is None:
                    first_playing = snap
                if observed_buffering and state == PLAYING:
                    break
            time.sleep(0.2)
        if first_playing is None:
            first_playing = wait_state(serial, package, {PLAYING}, 15, samples)
        result["saw_buffering"] = observed_buffering
        result["first_playing"] = first_playing
        if scenario == "stall" and not observed_buffering:
            raise RuntimeError("deterministic stall never surfaced BUFFERING")
    else:
        wait_state(serial, package, {PLAYING}, 18, samples)

    before = session_snapshot(serial, package)
    assert before is not None
    a = artifacts / f"{scenario}-frame-a.png"
    b = artifacts / f"{scenario}-frame-b.png"
    capture(serial, a)
    after = wait_position_delta(serial, package, int(before["position_ms"]), 1500, 9.0, samples)
    capture(serial, b)
    motion = roi_diff(a, b)
    if motion < 0.03:
        visual_deadline = time.monotonic() + 12.0
        while time.monotonic() < visual_deadline and motion < 0.03:
            time.sleep(1.0)
            capture(serial, b)
            motion = roi_diff(a, b)
    progress = int(after["position_ms"]) - int(before["position_ms"])
    logs = adb(serial, "logcat", "-d", "-v", "brief", timeout=60)
    frame_hashes = re.findall(r"E2E decoded-frame sample=\d+ .*?hash=([0-9a-fA-F]+)", logs)
    raw_frames_verified = len(set(frame_hashes)) >= 2
    compositor_motion_verified = motion >= 0.03
    if not raw_frames_verified and not compositor_motion_verified:
        raise RuntimeError(
            f"decoded video was not proven: raw_hashes={frame_hashes[-8:]}; compositor_motion={motion:.3f}"
        )
    result["raw_decoded_frames_verified"] = raw_frames_verified
    result["compositor_motion_verified"] = compositor_motion_verified

    audio = audio_proof(serial, package)
    if not bool(audio["codec_initialized"]) or not bool(audio["output_active"]):
        raise RuntimeError(f"audio was not proven active: {audio}")

    if scenario in {"normal", "slow", "decoder"}:
        key(serial, "DPAD_CENTER")
        paused = wait_state(serial, package, {PAUSED}, 4, samples)
        time.sleep(1.2)
        paused_later = session_snapshot(serial, package)
        if paused_later is None or abs(int(paused_later["position_ms"]) - int(paused["position_ms"])) > 450:
            raise RuntimeError(f"pause did not hold position: {paused} -> {paused_later}")
        key(serial, "DPAD_CENTER")
        resumed = wait_state(serial, package, {PLAYING}, 4, samples)
        resumed_later = wait_position_delta(serial, package, int(resumed["position_ms"]), 500, 9.0, samples)
        seek_before = int(resumed_later["position_ms"])
        key(serial, "DPAD_RIGHT")
        seek_after = wait_position_delta(serial, package, seek_before, 4000, 9.0, samples)
        result["controls"] = {
            "paused": paused,
            "paused_later": paused_later,
            "resumed": resumed,
            "resumed_later": resumed_later,
            "seek_before_ms": seek_before,
            "seek_after": seek_after,
        }

    if scenario == "decoder":
        logs = adb(serial, "logcat", "-d", "-v", "brief", timeout=60)
        fallback = "retrying current item with software video decoding" in logs
        result["software_decode_fallback"] = fallback
        if not fallback:
            raise RuntimeError("decoder fixture did not exercise software fallback")

    result.update(
        {
            "pass": True,
            "progress_ms": progress,
            "motion_diff": motion,
            "decoded_frame_hashes": frame_hashes[-8:],
            "audio": audio,
            "samples": samples,
        }
    )
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True)
    parser.add_argument("--package", required=True)
    parser.add_argument("--server-url", default="http://127.0.0.1:1024")
    parser.add_argument("--login", action="store_true")
    parser.add_argument("--item-id")
    parser.add_argument(
        "--scenario",
        choices=("normal", "slow", "stall", "resume_stall", "error", "decoder"),
        default="normal",
    )
    parser.add_argument("--artifacts", type=Path, required=True)
    args = parser.parse_args()
    # Offline fixtures must never rewrite the separately persisted live E2E login.
    if args.login and args.package != "app.sloppatv.offlinefixture":
        parser.error("offline fixture login requires app.sloppatv.offlinefixture; preserve live E2E app")
    if args.login and args.server_url != "http://127.0.0.1:1024":
        parser.error("offline fixture server must be http://127.0.0.1:1024")
    check_tv_remote(args.serial)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    if args.login:
        login(args.serial, args.package, args.server_url)
    if not args.item_id:
        return
    result = strict_playback(args.serial, args.package, args.item_id, args.scenario, args.artifacts)
    result["captured_at"] = datetime.now(UTC).isoformat()
    out = args.artifacts / f"{args.scenario}.json"
    out.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({k: v for k, v in result.items() if k != "samples"}, indent=2))
    print(out)


if __name__ == "__main__":
    main()

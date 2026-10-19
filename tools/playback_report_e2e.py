#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import ssl
import subprocess
import time
import urllib.request
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from tv_remote_safety import check_tv_remote

ROOT = Path(__file__).resolve().parents[1]
ARTIFACTS = ROOT / "artifacts" / "e2e-physical-tv"
CLIENT = "sloppaTV Report E2E"
DEVICE_ID = "sloppatv-report-e2e"
VERSION = "0.1.0"


def load_local_env() -> None:
    path = ROOT / ".env.local"
    if not path.exists():
        return
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        os.environ.setdefault(key.strip(), value.strip())


def authorization(token: str = "") -> str:
    value = f'MediaBrowser Client="{CLIENT}",Version="{VERSION}",DeviceId="{DEVICE_ID}",Device="Test Server"'
    if token:
        value += f',Token="{token}"'
    return value


class JellyfinObserver:
    def __init__(self, server: str, insecure: bool = False) -> None:
        self.server = server.rstrip("/")
        self.token = ""
        self.context = ssl._create_unverified_context() if insecure else ssl.create_default_context()

    def request(self, method: str, path: str, body: Any | None = None) -> Any:
        headers = {
            "Accept": "application/json",
            "Content-Type": "application/json",
            "Authorization": authorization(self.token),
            "User-Agent": f"sloppaTV-report-e2e/{VERSION}",
        }
        if self.token:
            headers["X-Emby-Token"] = self.token
        payload = None if body is None else json.dumps(body).encode("utf-8")
        request = urllib.request.Request(self.server + path, data=payload, headers=headers, method=method)
        with urllib.request.urlopen(request, context=self.context, timeout=20) as response:
            content = response.read()
            return json.loads(content) if content else None

    def login(self, username: str, password: str) -> None:
        result = self.request("POST", "/Users/AuthenticateByName", {"Username": username, "Pw": password})
        self.token = result["AccessToken"]

    def logout(self) -> None:
        if not self.token:
            return
        try:
            self.request("POST", "/Sessions/Logout")
        finally:
            self.token = ""

    def sessions(self) -> list[dict[str, Any]]:
        result = self.request("GET", "/Sessions")
        return result if isinstance(result, list) else []


def adb(serial: str, *args: str, capture: bool = False) -> str:
    result = subprocess.run(
        ["adb", "-s", serial, *args],
        check=True,
        text=True,
        capture_output=capture,
        timeout=30,
    )
    return result.stdout.strip() if capture else ""


def playback_session_for_item(sessions: list[dict[str, Any]], item_id: str) -> dict[str, Any] | None:
    normalized = item_id.replace("-", "").lower()
    matches = []
    for session in sessions:
        item = session.get("NowPlayingItem") or {}
        candidate = str(item.get("Id") or "").replace("-", "").lower()
        if session.get("Client") == "sloppaTV" and candidate == normalized:
            matches.append(session)
    if len(matches) > 1:
        raise RuntimeError(f"Multiple active sloppaTV sessions are playing item {item_id}")
    return matches[0] if matches else None


def snapshot(session: dict[str, Any]) -> dict[str, Any]:
    item = session.get("NowPlayingItem") or {}
    state = session.get("PlayState") or {}
    return {
        "captured_at": datetime.now(UTC).isoformat(),
        "user": session.get("UserName"),
        "item": item.get("Name"),
        "item_id": item.get("Id"),
        "position_ticks": int(state.get("PositionTicks") or 0),
        "runtime_ticks": int(item.get("RunTimeTicks") or 0),
        "paused": bool(state.get("IsPaused")),
        "play_method": state.get("PlayMethod"),
        "audio_stream_index": state.get("AudioStreamIndex"),
        "subtitle_stream_index": state.get("SubtitleStreamIndex"),
    }


def wait_for_session(
    observer: JellyfinObserver,
    item_id: str,
    *,
    paused: bool | None = None,
    minimum_position_ticks: int | None = None,
    timeout: float = 15.0,
) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    last: dict[str, Any] | None = None
    while time.monotonic() < deadline:
        session = playback_session_for_item(observer.sessions(), item_id)
        if session is not None:
            last = snapshot(session)
            if (paused is None or last["paused"] is paused) and (
                minimum_position_ticks is None or last["position_ticks"] >= minimum_position_ticks
            ):
                return last
        time.sleep(0.5)
    raise RuntimeError(f"Timed out waiting for Jellyfin playback session state; last={last}")


def wait_for_stopped(observer: JellyfinObserver, item_id: str, timeout: float = 15.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if playback_session_for_item(observer.sessions(), item_id) is None:
            return
        time.sleep(0.5)
    raise RuntimeError("Jellyfin session still reports the item after playback stopped")


def capture_frame(serial: str, phase: str, directory: Path) -> str:
    directory.mkdir(parents=True, exist_ok=True)
    remote = f"/sdcard/sloppatv-report-{phase}.png"
    target = directory / f"{phase}.png"
    adb(serial, "shell", "screencap", "-p", remote)
    try:
        adb(serial, "pull", remote, str(target))
    finally:
        adb(serial, "shell", "rm", remote)
    if not target.is_file() or target.stat().st_size == 0:
        raise RuntimeError(f"Missing physical TV screenshot: {target}")
    return str(target)


def sample_frame_luma(path: str) -> bytes:
    # Sample the central movie image; ignore letterboxing and player controls.
    result = subprocess.run(
        [
            "ffmpeg",
            "-nostdin",
            "-loglevel",
            "error",
            "-i",
            path,
            "-vf",
            "crop=iw*0.8:ih*0.6:iw*0.1:ih*0.2,scale=48:27,format=gray",
            "-frames:v",
            "1",
            "-f",
            "rawvideo",
            "pipe:1",
        ],
        check=True,
        capture_output=True,
        timeout=12,
    )
    if len(result.stdout) != 48 * 27:
        raise RuntimeError(f"Cannot sample TV screenshot: {path}")
    return result.stdout


def is_nonblank_frame(luma: bytes) -> bool:
    return sum(value > 30 for value in luma) >= len(luma) // 10 and max(luma) - min(luma) >= 24


def are_distinct_frames(first: bytes, second: bytes) -> bool:
    return sum(abs(a - b) > 16 for a, b in zip(first, second, strict=True)) >= len(first) // 10


def capture_video_candidates(
    serial: str, directory: Path, max_attempts: int = 12, *, prefix: str = "video"
) -> tuple[list[str], bool]:
    previous: tuple[str, bytes] | None = None
    saved: list[str] = []
    for attempt in range(max_attempts):
        path = capture_frame(serial, f"{prefix}-{attempt:02d}", directory)
        saved.append(path)
        pixels = sample_frame_luma(path)
        if is_nonblank_frame(pixels):
            if previous and are_distinct_frames(previous[1], pixels):
                return [previous[0], path], True
            if previous is None:
                previous = (path, pixels)
        if attempt < max_attempts - 1:
            time.sleep(2.5)
    return saved[-2:], False


def run_acceptance(
    serial: str, observer: JellyfinObserver, item_id: str, directory: Path | None = None
) -> dict[str, Any]:
    if directory is None:
        directory = ARTIFACTS / datetime.now(UTC).strftime("%Y%m%dT%H%M%S%fZ")
    adb(serial, "shell", "input", "keyevent", "KEYCODE_WAKEUP")
    initial = wait_for_session(observer, item_id)
    if initial["paused"]:
        adb(serial, "shell", "input", "keyevent", "KEYCODE_DPAD_CENTER")
        initial = wait_for_session(observer, item_id, paused=False)

    minimum = initial["position_ticks"] + 30_000_000
    progress = wait_for_session(observer, item_id, paused=False, minimum_position_ticks=minimum, timeout=16.0)
    screenshots, cold_start_frames = capture_video_candidates(serial, directory)
    pre_seek_screenshots = None
    seek_observation = None
    moving_frame_candidates = cold_start_frames
    if not cold_start_frames:
        # Black openings can outlast the sampling window while playback advances.
        # Keep cold-start decode unverified even if a later seek exposes video.
        pre_seek_screenshots = screenshots
        runtime = initial.get("runtime_ticks") or 0
        # Reserve 45 seconds at the end. Never seek unknown/short programmes
        # into their outro, particularly short episodes or specials.
        seek_steps = (
            min(12, max(0, (runtime - progress["position_ticks"] - 450_000_000) // 100_000_000)) if runtime else 0
        )
        if seek_steps:
            for _ in range(seek_steps):
                adb(serial, "shell", "input", "keyevent", "KEYCODE_DPAD_RIGHT")
                time.sleep(0.08)
            seek_observation = wait_for_session(
                observer,
                item_id,
                paused=False,
                minimum_position_ticks=progress["position_ticks"] + seek_steps * 5_000_000,
                timeout=18.0,
            )
            screenshots, moving_frame_candidates = capture_video_candidates(
                serial,
                directory,
                max_attempts=8,
                prefix="post-seek",
            )
    audio_telemetry = directory / "audio-flinger.txt"
    audio_telemetry.write_text(adb(serial, "shell", "dumpsys", "media.audio_flinger", capture=True), encoding="utf-8")

    adb(serial, "shell", "input", "keyevent", "KEYCODE_DPAD_CENTER")
    paused = wait_for_session(observer, item_id, paused=True)
    time.sleep(2.0)
    paused_stable = wait_for_session(observer, item_id, paused=True)
    if paused_stable["position_ticks"] - paused["position_ticks"] > 15_000_000:
        raise RuntimeError("Jellyfin paused position continued advancing unexpectedly")

    adb(serial, "shell", "input", "keyevent", "KEYCODE_DPAD_CENTER")
    resumed = wait_for_session(observer, item_id, paused=False)

    adb(serial, "shell", "input", "keyevent", "KEYCODE_BACK")
    time.sleep(1.5)
    if playback_session_for_item(observer.sessions(), item_id) is not None:
        adb(serial, "shell", "input", "keyevent", "KEYCODE_BACK")
    wait_for_stopped(observer, item_id)

    return {
        "serial": serial,
        "item_id": item_id,
        "initial": initial,
        "progress": progress,
        "paused": paused,
        "paused_stable": paused_stable,
        "resumed": resumed,
        "stopped": True,
        "screenshots": screenshots,
        "pre_seek_screenshots": pre_seek_screenshots,
        "seek_observation": seek_observation,
        "audio_telemetry": str(audio_telemetry),
        "verification": {
            "status": "SESSION_ONLY_UNVERIFIED",
            "decoded_video": False,
            "nonblank_moving_frame_candidates": moving_frame_candidates,
            "cold_start_frame_candidates": cold_start_frames,
            "post_seek_only": moving_frame_candidates and not cold_start_frames,
            "audio": False,
            "reason": "Frame sampling is only a precheck; visual review, decoder confirmation, and output audio verification remain required",
        },
    }


def main() -> int:
    load_local_env()
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True)
    parser.add_argument("--item-id", required=True)
    parser.add_argument("--server", default=os.getenv("JELLYFIN_LOCAL_SERVER", ""))
    parser.add_argument("--username", default=os.getenv("JELLYFIN_LOCAL_USERNAME", ""))
    parser.add_argument("--password", default=os.getenv("JELLYFIN_LOCAL_PASSWORD", ""))
    parser.add_argument("--insecure", action="store_true")
    parser.add_argument(
        "--session-only", action="store_true", help="Permit session controls check without decoded/audio proof"
    )
    args = parser.parse_args()
    if not args.server or not args.username:
        raise SystemExit("Set Jellyfin server credentials or pass --server/--username/--password")

    model = adb(args.serial, "shell", "getprop", "ro.product.model", capture=True)
    if model.strip() != "Google TV Streamer":
        raise SystemExit(f"Refusing device {args.serial}: expected Google TV Streamer, got {model!r}")

    check_tv_remote(args.serial)
    observer = JellyfinObserver(args.server, args.insecure)
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    try:
        observer.login(args.username, args.password)
        evidence = run_acceptance(args.serial, observer, args.item_id)
        path = ARTIFACTS / "playback-report.json"
        path.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(evidence, indent=2))
        print(f"wrote {path}")
    finally:
        observer.logout()
    if not args.session_only:
        print("INCOMPLETE: session state is not decoded video/audio evidence; manually review screenshots and audio.")
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

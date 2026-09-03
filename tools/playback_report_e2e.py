#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import re
import ssl
import subprocess
import time
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

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
    value = f'MediaBrowser Client="{CLIENT}",Version="{VERSION}",DeviceId="{DEVICE_ID}",Device="Astra"'
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


def stream_volume(serial: str) -> int:
    output = adb(serial, "shell", "cmd", "media_session", "volume", "--stream", "3", "--get", capture=True)
    match = re.search(r"volume is (\d+)", output)
    if not match:
        raise RuntimeError("Unable to read Android TV media volume")
    return int(match.group(1))


def set_stream_volume(serial: str, value: int) -> None:
    adb(serial, "shell", "cmd", "media_session", "volume", "--stream", "3", "--set", str(max(0, value)))


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
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "user": session.get("UserName"),
        "item": item.get("Name"),
        "item_id": item.get("Id"),
        "position_ticks": int(state.get("PositionTicks") or 0),
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


def run_acceptance(serial: str, observer: JellyfinObserver, item_id: str) -> dict[str, Any]:
    adb(serial, "shell", "input", "keyevent", "KEYCODE_WAKEUP")
    initial = wait_for_session(observer, item_id)
    if initial["paused"]:
        adb(serial, "shell", "input", "keyevent", "KEYCODE_DPAD_CENTER")
        initial = wait_for_session(observer, item_id, paused=False)

    minimum = initial["position_ticks"] + 30_000_000
    progress = wait_for_session(observer, item_id, paused=False, minimum_position_ticks=minimum, timeout=16.0)

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
    args = parser.parse_args()
    if not args.server or not args.username:
        raise SystemExit("Set Jellyfin server credentials or pass --server/--username/--password")

    model = adb(args.serial, "shell", "getprop", "ro.product.model", capture=True)
    if model.strip() != "Google TV Streamer":
        raise SystemExit(f"Refusing device {args.serial}: expected Google TV Streamer, got {model!r}")

    observer = JellyfinObserver(args.server, args.insecure)
    original_volume = stream_volume(args.serial)
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    try:
        set_stream_volume(args.serial, 0)
        observer.login(args.username, args.password)
        evidence = run_acceptance(args.serial, observer, args.item_id)
        path = ARTIFACTS / "playback-report.json"
        path.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(evidence, indent=2))
        print(f"wrote {path}")
    finally:
        observer.logout()
        set_stream_volume(args.serial, original_volume)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

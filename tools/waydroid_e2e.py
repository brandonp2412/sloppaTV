#!/usr/bin/env python3
import argparse
import subprocess
import time
from pathlib import Path

from PIL import Image, ImageChops, ImageStat

SERIAL = ""
ARTIFACTS = Path(__file__).resolve().parents[1] / "artifacts" / "e2e-waydroid"


def adb(*args: str, capture: bool = False) -> str:
    cmd = ["adb", "-s", SERIAL, *args]
    result = subprocess.run(cmd, check=True, text=True, capture_output=capture)
    return result.stdout.strip() if capture else ""


def verify_waydroid() -> None:
    model = adb("shell", "getprop", "ro.product.model", capture=True).strip()
    if "WayDroid" not in model:
        raise SystemExit(f"Refusing device {SERIAL}: model is {model!r}, not Waydroid")
    size = adb("shell", "wm", "size", capture=True)
    if "1920x1080" not in size:
        raise SystemExit(f"Waydroid is not configured for 1920x1080: {size}")
    ARTIFACTS.mkdir(parents=True, exist_ok=True)


def key(name: str) -> None:
    key_name = name if name.startswith("KEYCODE_") else f"KEYCODE_{name.upper()}"
    adb("shell", "input", "keyevent", key_name)


def capture(name: str) -> Path:
    remote = f"/sdcard/{name}.png"
    local = ARTIFACTS / f"{name}.png"
    adb("shell", "screencap", "-p", remote)
    adb("pull", remote, str(local))
    return local


def roi_diff(a: Path, b: Path) -> float:
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


def filtered_logs(name: str) -> None:
    logs = adb("logcat", "-d", "-v", "time", capture=True)
    filtered = [
        line for line in logs.splitlines()
        if any(token.lower() in line.lower() for token in (
            "sloppaTV", "MediaPlayer", "NuPlayer", "CCodec", "PlaybackInfo",
            "HTTP 4", "HTTP 5", "FATAL", "ANR", "surface"
        ))
    ]
    log_path = ARTIFACTS / f"{name}.log"
    log_path.write_text("\n".join(filtered[-700:]) + "\n", encoding="utf-8")
    print(f"wrote {log_path}")


def wonder_core() -> None:
    adb("logcat", "-c")
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
    filtered_logs("wonder-core")


def planet_core() -> None:
    adb("logcat", "-c")
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
    filtered_logs("planet-core")


def main() -> None:
    global SERIAL
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True, help="ADB serial for the Waydroid test target")
    sub = parser.add_subparsers(dest="command", required=True)
    p_key = sub.add_parser("key")
    p_key.add_argument("keys", nargs="+")
    p_cap = sub.add_parser("capture")
    p_cap.add_argument("name")
    p_pair = sub.add_parser("pair")
    p_pair.add_argument("prefix")
    p_pair.add_argument("wait", type=float)
    sub.add_parser("wonder-core")
    sub.add_parser("planet-core")
    args = parser.parse_args()
    SERIAL = args.serial

    verify_waydroid()
    if args.command == "key":
        for value in args.keys:
            key(value)
            time.sleep(0.2)
    elif args.command == "capture":
        print(capture(args.name))
    elif args.command == "pair":
        pair(args.prefix, args.wait)
    elif args.command == "wonder-core":
        wonder_core()
    elif args.command == "planet-core":
        planet_core()


if __name__ == "__main__":
    main()

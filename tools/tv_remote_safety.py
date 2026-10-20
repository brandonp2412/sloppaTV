"""Fail-closed preflight for physical Google TV input automation."""

from __future__ import annotations

import re
import subprocess


def ready(bt: str, inputs: str, accessibility: str) -> bool:
    hid = re.search(r"shim::legacy::hid status:BTIF_HH_DEV_CONNECTED num_devices:(\d+)", bt)
    if not hid or int(hid.group(1)) < 1 or "Google TV Remote Keyboard" not in inputs:
        return False
    mapper = "io.github.sds100.keymapper/" in accessibility
    bound = "Bound services:{Service[label=Key Mapper" in accessibility
    return not mapper or bound


def check_tv_remote(serial: str) -> None:
    def shell(*args: str) -> str:
        return subprocess.run(
            ["adb", "-s", serial, "shell", *args],
            check=True,
            capture_output=True,
            text=True,
            timeout=12,
        ).stdout

    if shell("getprop", "ro.product.model").strip() != "Google TV Streamer":
        return
    bt = shell("dumpsys", "bluetooth_manager")
    inputs = shell("dumpsys", "input")
    accessibility = shell("dumpsys", "accessibility")
    if not ready(bt, inputs, accessibility):
        raise RuntimeError(
            "Physical TV remote HID or Key Mapper accessibility service is unhealthy; "
            "refusing ADB input. Never reset, unpair, or grab the remote."
        )


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", default="192.168.1.114:5555")
    check_tv_remote(parser.parse_args().serial)
    print("Physical TV remote preflight passed")

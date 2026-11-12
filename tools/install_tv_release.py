#!/usr/bin/env python3
"""Build and install a signed SloppaTV release without downgrading a TV."""

import argparse
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = "app.sloppatv"
ACTIVITY = "app.sloppatv.SloppaNativeActivity"
APK = ROOT / "app/build/outputs/apk/release/app-release.apk"
AAPT = Path(os.environ.get("ANDROID_HOME", "/opt/android-sdk")) / "build-tools/36.0.0/aapt"


def run(args, *, cwd=None, capture=True):
    return subprocess.run(args, cwd=cwd, check=True, text=True, capture_output=capture)


def installed_version(serial):
    output = run(["adb", "-s", serial, "shell", "dumpsys", "package", PACKAGE]).stdout
    match = re.search(r"Package \[app\.sloppatv\][\s\S]*?versionCode=(\d+)", output)
    if not match:
        raise RuntimeError(f"Cannot identify installed {PACKAGE} on {serial}; refusing to install blindly")
    return int(match.group(1))


def baseline_version():
    text = (ROOT / "gradle.properties").read_text()
    match = re.search(r"^SLOPPATV_MIN_RELEASE_CODE=(\d+)$", text, re.MULTILINE)
    if not match:
        raise RuntimeError("Missing SLOPPATV_MIN_RELEASE_CODE in gradle.properties")
    return int(match.group(1))


def apk_identity(apk):
    output = run([str(AAPT), "dump", "badging", str(apk)]).stdout
    match = re.search(r"^package: name='([^']+)' versionCode='(\d+)'", output, re.MULTILINE)
    if not match:
        raise RuntimeError("Cannot read APK identity")
    return match.group(1), int(match.group(2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", default=os.environ.get("ANDROID_SERIAL"))
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()
    if not args.serial:
        parser.error("Set --serial to the TV's ADB serial or ANDROID_SERIAL")
    prior = installed_version(args.serial)
    code = max(baseline_version(), prior + 1)
    if not args.skip_build:
        print(f"Building signed production release {code} (installed: {prior})", flush=True)
        subprocess.run(
            [str(ROOT / "gradlew"), ":app:assembleRelease", f"-PSLOPPATV_VERSION_CODE={code}"], cwd=ROOT, check=True
        )
    package, actual = apk_identity(APK)
    if package != PACKAGE or actual <= prior:
        raise RuntimeError(f"Refusing APK {package} version {actual}: installed production version is {prior}")
    if not args.skip_build and actual != code:
        raise RuntimeError(f"Expected build {code}, APK is {actual}")
    print(f"Installing {PACKAGE} version {actual}, replacing {prior}, preserving app data", flush=True)
    run(["adb", "-s", args.serial, "install", "-r", str(APK)], capture=False)
    confirmed = installed_version(args.serial)
    if confirmed != actual:
        raise RuntimeError(f"Installation version mismatch: expected {actual}, found {confirmed}")
    run(["adb", "-s", args.serial, "shell", "am", "start", "-n", f"{PACKAGE}/{ACTIVITY}"], capture=False)
    print(f"Verified: {PACKAGE} versionCode={confirmed} on {args.serial}", flush=True)


if __name__ == "__main__":
    main()

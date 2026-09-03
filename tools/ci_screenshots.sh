#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly SCRIPT_DIR
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
readonly PROJECT_DIR
readonly SCREENSHOT_DIR="$PROJECT_DIR/artifacts/ci-screenshots"
readonly SCREENSHOT_SUITE="$SCRIPT_DIR/screenshot-suites/ci-login.json"

if [[ -z "${ANDROID_SERIAL:-}" ]]; then
    echo "ANDROID_SERIAL must be set" >&2
    exit 2
fi
if [[ "$ANDROID_SERIAL" != emulator-* ]]; then
    echo "Refusing non-emulator ANDROID_SERIAL: $ANDROID_SERIAL" >&2
    exit 2
fi
if [[ -z "${SLOPPATV_APK:-}" || ! -f "$SLOPPATV_APK" ]]; then
    echo "SLOPPATV_APK must point to a built APK" >&2
    exit 2
fi

diagnostics() {
    local status=$?
    if (( status != 0 )); then
        adb -s "$ANDROID_SERIAL" logcat -d -t 300 >&2 || true
        adb -s "$ANDROID_SERIAL" shell dumpsys activity top >&2 || true
    fi
    exit "$status"
}
trap diagnostics EXIT

mkdir -p "$SCREENSHOT_DIR"
find "$SCREENSHOT_DIR" -maxdepth 1 -type f -delete

adb -s "$ANDROID_SERIAL" install -r "$SLOPPATV_APK"
adb -s "$ANDROID_SERIAL" shell wm size 1920x1080
timeout --foreground -k 15 180 python3 "$SCRIPT_DIR/waydroid_e2e.py" \
    --serial "$ANDROID_SERIAL" \
    --target android-tv-emulator \
    screenshots --suite "$SCREENSHOT_SUITE"

test -s "$SCREENSHOT_DIR/01-login.png"
test -s "$SCREENSHOT_DIR/screenshots.json"
trap - EXIT
echo "Screenshot suite completed: $SCREENSHOT_DIR"

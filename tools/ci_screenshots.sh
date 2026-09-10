#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly SCRIPT_DIR
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
readonly PROJECT_DIR
readonly SCREENSHOT_TARGET="${SLOPPATV_SCREENSHOT_TARGET:-android-tv-emulator}"
readonly SCREENSHOT_DIR="$PROJECT_DIR/artifacts/$([[ "$SCREENSHOT_TARGET" == "waydroid" ]] && echo e2e-waydroid || echo ci-screenshots)"
readonly SCREENSHOT_SUITE="$SCRIPT_DIR/screenshot-suites/ci-login.json"
readonly FIXTURE_PORT="${SLOPPATV_FIXTURE_PORT:-18096}"

if ! [[ "$FIXTURE_PORT" =~ ^[0-9]+$ ]] || (( FIXTURE_PORT < 1024 || FIXTURE_PORT > 65535 )); then
    echo "SLOPPATV_FIXTURE_PORT must be a TCP port between 1024 and 65535" >&2
    exit 2
fi
if [[ -z "${ANDROID_SERIAL:-}" ]]; then
    echo "ANDROID_SERIAL must be set" >&2
    exit 2
fi
case "$SCREENSHOT_TARGET" in
    android-tv-emulator)
        if [[ "$ANDROID_SERIAL" != emulator-* ]]; then
            echo "Refusing non-emulator ANDROID_SERIAL for emulator screenshots: $ANDROID_SERIAL" >&2
            exit 2
        fi
        ;;
    waydroid)
        ;;
    *)
        echo "Unsupported screenshot target: $SCREENSHOT_TARGET" >&2
        exit 2
        ;;
esac
if [[ -z "${SLOPPATV_APK:-}" || ! -f "$SLOPPATV_APK" ]]; then
    echo "SLOPPATV_APK must point to a built APK" >&2
    exit 2
fi

diagnostics() {
    local status=$?
    timeout --foreground -k 2 8 adb -s "$ANDROID_SERIAL" reverse --remove tcp:1024 2>/dev/null || true
    if [[ -n "${fixture_pid:-}" ]]; then kill "$fixture_pid" 2>/dev/null || true; fi
    if (( status != 0 )); then
        timeout --foreground -k 2 8 adb -s "$ANDROID_SERIAL" logcat -d -t 300 >&2 || true
        timeout --foreground -k 2 8 adb -s "$ANDROID_SERIAL" shell dumpsys activity top >&2 || true
    fi
    exit "$status"
}
trap diagnostics EXIT

if ! timeout --foreground -k 2 10 adb -s "$ANDROID_SERIAL" shell true >/dev/null 2>&1; then
    echo "ADB shell is not authorized or responsive for $ANDROID_SERIAL" >&2
    exit 1
fi

mkdir -p "$SCREENSHOT_DIR"
find "$SCREENSHOT_DIR" -maxdepth 1 -type f -delete
python3 "$SCRIPT_DIR/screenshot_fixture_server.py" --port "$FIXTURE_PORT" >"$SCREENSHOT_DIR/fixture-server.log" 2>&1 &
fixture_pid=$!
fixture_ready=0
for _ in {1..50}; do
    if ! kill -0 "$fixture_pid" 2>/dev/null; then
        echo "Screenshot fixture server exited before becoming ready" >&2
        wait "$fixture_pid" 2>/dev/null || true
        exit 1
    fi
    if curl --fail --silent --show-error --connect-timeout 1 --max-time 1 "http://127.0.0.1:$FIXTURE_PORT/System/Info/Public" >/dev/null 2>&1; then
        fixture_ready=1
        break
    fi
    sleep 0.1
done
if (( fixture_ready == 0 )); then
    echo "Screenshot fixture server did not become ready" >&2
    exit 1
fi

if [[ "$SCREENSHOT_TARGET" == "waydroid" ]]; then
    # Waydroid is disposable test infrastructure; start from a deterministic
    # logged-out state so the full fixture-driven catalog is reproducible.
    adb -s "$ANDROID_SERIAL" uninstall app.sloppatv >/dev/null 2>&1 || true
fi
adb -s "$ANDROID_SERIAL" install -r "$SLOPPATV_APK"
adb -s "$ANDROID_SERIAL" shell wm size 1920x1080
# Route the Android target's loopback HTTP port to the host fixture. This avoids
# depending on target-specific host aliases and keeps URL entry deterministic.
adb -s "$ANDROID_SERIAL" reverse tcp:1024 "tcp:$FIXTURE_PORT"
timeout --foreground -k 15 240 python3 "$SCRIPT_DIR/waydroid_e2e.py" \
    --serial "$ANDROID_SERIAL" \
    --target "$SCREENSHOT_TARGET" \
    screenshots --suite "$SCREENSHOT_SUITE"

for screenshot in \
    01-login \
    02-home \
    03-search \
    04-search-catalog \
    05-movie-browse \
    06-movie-details \
    07-cast \
    08-person-titles \
    09-item-menu \
    10-player-cc-video \
    11-player-controls \
    12-series-browse \
    13-series-details \
    14-seasons \
    15-episodes \
    16-episode-details \
    17-settings \
    18-settings-options \
    19-profiles; do
    test -s "$SCREENSHOT_DIR/$screenshot.png"
done
test -s "$SCREENSHOT_DIR/screenshots.json"
grep -Fq 'POST /Users/AuthenticateByName' "$SCREENSHOT_DIR/fixture-server.log"
grep -Fq 'GET /Users/fixture-user/Views' "$SCREENSHOT_DIR/fixture-server.log"
grep -Fq 'POST /Items/movie-big-buck-bunny/PlaybackInfo' "$SCREENSHOT_DIR/fixture-server.log"
grep -Fq 'GET /Shows/series-open-classics/Seasons' "$SCREENSHOT_DIR/fixture-server.log"
grep -Fq 'GET /Shows/series-open-classics/Episodes' "$SCREENSHOT_DIR/fixture-server.log"
trap - EXIT
timeout --foreground -k 2 8 adb -s "$ANDROID_SERIAL" reverse --remove tcp:1024 2>/dev/null || true
kill "$fixture_pid" 2>/dev/null || true
echo "Screenshot suite completed: $SCREENSHOT_DIR"

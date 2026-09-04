#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CPP_DIR = ROOT / "app" / "src" / "main" / "cpp"
CPP_TEST_DIR = ROOT / "app" / "src" / "test" / "cpp"
PY_TEST_DIR = ROOT / "app" / "src" / "test" / "python"
BUILD_DIR = ROOT / "build" / "host-tests"

CPP_TESTS = [
    "account_screen_test.cpp",
    "app_settings_test.cpp",
    "audio_policy_test.cpp",
    "artwork_cache_test.cpp",
    "browse_screen_test.cpp",
    "deep_link_test.cpp",
    "details_screen_test.cpp",
    "external_playback_state_test.cpp",
    "external_player_policy_test.cpp",
    "http_cache_policy_test.cpp",
    "http_error_policy_test.cpp",
    "http_retry_policy_test.cpp",
    "home_image_disk_cache_test.cpp",
    "home_screen_test.cpp",
    "launch_intent_test.cpp",
    "media_player_policy_test.cpp",
    "playback_continuation_test.cpp",
    "playback_profile_test.cpp",
    "playback_session_test.cpp",
    "playback_telemetry_test.cpp",
    "playback_transition_test.cpp",
    "player_screen_test.cpp",
    "player_tracks_test.cpp",
    "ui_policy_test.cpp",
    "playback_queue_test.cpp",
    "request_epoch_test.cpp",
    "screensaver_policy_test.cpp",
    "search_screen_test.cpp",
    "session_registry_test.cpp",
    "settings_screen_test.cpp",
    "subtitle_policy_test.cpp",
    "trickplay_policy_test.cpp",
    "trickplay_preview_test.cpp",
    "unicode_text_test.cpp",
    "navigation_stack_test.cpp",
    "version_policy_test.cpp",
]

LINKED_CPP_TESTS = [
    ("jellyfin_item_parser_test.cpp", ["jellyfin_item_parser.cpp"], []),
    ("session_store_test.cpp", ["session_store.cpp"], []),
    ("task_runner_test.cpp", ["task_runner.cpp"], ["-pthread"]),
]


def run(command: list[str]) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)


def run_cpp_test(test_name: str, extra_sources: list[str] | None = None, extra_flags: list[str] | None = None) -> None:
    source = CPP_TEST_DIR / test_name
    binary = BUILD_DIR / source.stem
    command = [
        "g++",
        "-std=c++20",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        *(extra_flags or []),
        "-I",
        str(CPP_DIR),
        "-I",
        str(CPP_DIR / "third_party"),
        str(source),
        *(str(CPP_DIR / extra) for extra in (extra_sources or [])),
        "-o",
        str(binary),
    ]
    run(command)
    run([str(binary)])


def main() -> int:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    for test_name in CPP_TESTS:
        run_cpp_test(test_name)
    for test_name, extra_sources, extra_flags in LINKED_CPP_TESTS:
        run_cpp_test(test_name, extra_sources, extra_flags)

    run([sys.executable, "-m", "unittest", "discover", "-s", str(PY_TEST_DIR), "-p", "test_*.py"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

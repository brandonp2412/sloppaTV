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
    "app_settings_test.cpp",
    "audio_policy_test.cpp",
    "deep_link_test.cpp",
    "external_player_policy_test.cpp",
    "http_cache_policy_test.cpp",
    "http_error_policy_test.cpp",
    "http_retry_policy_test.cpp",
    "media_player_policy_test.cpp",
    "ui_policy_test.cpp",
    "playback_queue_test.cpp",
    "screensaver_policy_test.cpp",
    "subtitle_policy_test.cpp",
    "trickplay_policy_test.cpp",
    "unicode_text_test.cpp",
    "navigation_stack_test.cpp",
    "version_policy_test.cpp",
]


def run(command: list[str]) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)


def main() -> int:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    for test_name in CPP_TESTS:
        source = CPP_TEST_DIR / test_name
        binary = BUILD_DIR / source.stem
        run([
            "g++",
            "-std=c++20",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            "-I",
            str(CPP_DIR),
            "-I",
            str(CPP_DIR / "third_party"),
            str(source),
            "-o",
            str(binary),
        ])
        run([str(binary)])

    task_runner_test = CPP_TEST_DIR / "task_runner_test.cpp"
    task_runner_binary = BUILD_DIR / task_runner_test.stem
    run([
        "g++",
        "-std=c++20",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        "-pthread",
        "-I",
        str(CPP_DIR),
        str(task_runner_test),
        str(CPP_DIR / "task_runner.cpp"),
        "-o",
        str(task_runner_binary),
    ])
    run([str(task_runner_binary)])

    session_store_test = CPP_TEST_DIR / "session_store_test.cpp"
    session_store_binary = BUILD_DIR / session_store_test.stem
    run([
        "g++",
        "-std=c++20",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        "-I",
        str(CPP_DIR),
        "-I",
        str(CPP_DIR / "third_party"),
        str(session_store_test),
        str(CPP_DIR / "session_store.cpp"),
        "-o",
        str(session_store_binary),
    ])
    run([str(session_store_binary)])

    run([sys.executable, "-m", "unittest", "discover", "-s", str(PY_TEST_DIR), "-p", "test_*.py"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

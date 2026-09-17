#!/usr/bin/env python3
from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CPP_ROOT = ROOT / "app" / "src" / "main" / "cpp"
TEST_ROOT = ROOT / "app" / "src" / "test" / "cpp"
TOOLS_ROOT = ROOT / "tools"
CPP_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
FORMAT_DIRS = (CPP_ROOT, TEST_ROOT, TOOLS_ROOT)


def require_tool(name: str) -> str:
    path = shutil.which(name)
    if not path:
        raise RuntimeError(f"Required tool is not installed: {name}")
    return path


def run(command: list[str], *, capture: bool = False) -> subprocess.CompletedProcess[str]:
    print("+", " ".join(command), flush=True)
    return subprocess.run(
        command,
        cwd=ROOT,
        check=False,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.STDOUT if capture else None,
    )


def owned_format_files() -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for root in FORMAT_DIRS:
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if not path.is_file() or path.suffix not in CPP_SUFFIXES:
                continue
            if CPP_ROOT / "third_party" in path.parents:
                continue
            files.append(path)
    return sorted(files)


def changed_line_ranges(base: str) -> dict[pathlib.Path, list[tuple[int, int]]]:
    result = run(
        ["git", "diff", "--unified=0", "--no-color", base, "--", "app/src/main/cpp", "app/src/test/cpp", "tools"],
        capture=True,
    )
    if result.returncode != 0:
        sys.stdout.write(result.stdout or "")
        raise RuntimeError(f"Could not diff against formatting base {base}")

    ranges: dict[pathlib.Path, list[tuple[int, int]]] = {}
    current: pathlib.Path | None = None
    for line in (result.stdout or "").splitlines():
        if line.startswith("+++ b/"):
            relative = pathlib.Path(line[6:])
            candidate = ROOT / relative
            current = candidate if candidate.suffix in CPP_SUFFIXES and "third_party" not in candidate.parts else None
            continue
        if current is None or not line.startswith("@@"):
            continue
        match = re.search(r"\+(\d+)(?:,(\d+))?", line)
        if not match:
            continue
        start = int(match.group(1))
        count = int(match.group(2) or "1")
        if count <= 0:
            continue
        ranges.setdefault(current, []).append((start, start + count - 1))
    return ranges


def format_code(*, check: bool, base: str | None) -> int:
    clang_format = require_tool("clang-format")
    if base:
        file_ranges = changed_line_ranges(base)
        if not file_ranges:
            print("No changed C++ lines to format.")
            return 0
        failures = 0
        for path, ranges in sorted(file_ranges.items()):
            command = [clang_format, "--style=file"]
            if check:
                command += ["--dry-run", "--Werror"]
            else:
                command += ["-i"]
            for start, end in ranges:
                command.append(f"--lines={start}:{end}")
            command.append(str(path))
            failures |= run(command).returncode
        return failures

    files = owned_format_files()
    command = [clang_format, "--style=file"]
    if check:
        command += ["--dry-run", "--Werror"]
    else:
        command += ["-i"]
    command += [str(path) for path in files]
    return run(command).returncode


def find_compile_commands() -> pathlib.Path:
    candidates = list((ROOT / "app" / ".cxx").glob("Debug/**/arm64-v8a/compile_commands.json"))
    if not candidates:
        candidates = list((ROOT / "app" / ".cxx").glob("**/arm64-v8a/compile_commands.json"))
    if not candidates:
        raise RuntimeError("No Android compile_commands.json found. Run ./gradlew assembleDebug first.")
    return max(candidates, key=lambda path: path.stat().st_mtime)


def tidy_sources(database: pathlib.Path) -> list[pathlib.Path]:
    entries = json.loads(database.read_text())
    sources: set[pathlib.Path] = set()
    for entry in entries:
        raw = pathlib.Path(entry["file"])
        path = raw if raw.is_absolute() else pathlib.Path(entry.get("directory", database.parent)) / raw
        path = path.resolve()
        if path.parent != CPP_ROOT.resolve() or path.suffix not in {".cc", ".cpp", ".cxx"}:
            continue
        sources.add(path)
    return sorted(sources)


def run_tidy() -> int:
    clang_tidy = require_tool("clang-tidy")
    database = find_compile_commands()
    sources = tidy_sources(database)
    if not sources:
        raise RuntimeError(f"No sloppaTV C++ translation units found in {database}")
    print(f"clang-tidy: {len(sources)} translation units using {database.relative_to(ROOT)}")

    def check(path: pathlib.Path) -> tuple[pathlib.Path, int, str]:
        completed = subprocess.run(
            [clang_tidy, "-p", str(database.parent), "--quiet", str(path)],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        return path, completed.returncode, completed.stdout or ""

    failures = 0
    worker_count = min(4, os.cpu_count() or 1, len(sources))
    with concurrent.futures.ThreadPoolExecutor(max_workers=worker_count) as executor:
        futures = [executor.submit(check, source) for source in sources]
        for future in concurrent.futures.as_completed(futures):
            path, returncode, output = future.result()
            if output.strip():
                print(f"\n[{path.relative_to(ROOT)}]\n{output.rstrip()}")
            failures |= returncode
    return failures


def run_cppcheck() -> int:
    cppcheck = require_tool("cppcheck")
    sources = sorted(path for path in CPP_ROOT.glob("*.cpp") if path.is_file())
    command = [
        cppcheck,
        "--std=c++20",
        "--enable=warning,performance,portability",
        "--error-exitcode=1",
        "--inline-suppr",
        "--quiet",
        "--suppressions-list=.cppcheck-suppressions.txt",
        "-I",
        str(CPP_ROOT),
        "-I",
        str(CPP_ROOT / "third_party"),
        *[str(path) for path in sources],
    ]
    return run(command).returncode


def main() -> int:
    parser = argparse.ArgumentParser(description="sloppaTV C++ formatter, linter and static-analysis runner")
    subparsers = parser.add_subparsers(dest="command", required=True)

    format_parser = subparsers.add_parser("format", help="Auto-format owned C++ code")
    format_parser.add_argument("--changed-from", metavar="REF", help="Format only lines changed since REF")

    check_format_parser = subparsers.add_parser("check-format", help="Verify clang-format")
    check_format_parser.add_argument("--changed-from", metavar="REF", help="Check only lines changed since REF")

    subparsers.add_parser("tidy", help="Run clang-tidy and Clang Static Analyzer")
    subparsers.add_parser("cppcheck", help="Run cppcheck as an independent analyzer")

    check_parser = subparsers.add_parser("check", help="Run all C++ quality gates")
    check_parser.add_argument("--format-base", metavar="REF", help="Check formatting only on lines changed since REF")

    args = parser.parse_args()
    try:
        if args.command == "format":
            return format_code(check=False, base=args.changed_from)
        if args.command == "check-format":
            return format_code(check=True, base=args.changed_from)
        if args.command == "tidy":
            return run_tidy()
        if args.command == "cppcheck":
            return run_cppcheck()
        if args.command == "check":
            failures = format_code(check=True, base=args.format_base)
            failures |= run_tidy()
            failures |= run_cppcheck()
            return 1 if failures else 0
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    return 2


if __name__ == "__main__":
    raise SystemExit(main())

"""Read only the requested app-owned Android MediaSession and audio track."""

from __future__ import annotations

import re

SESSION_HEADER = re.compile(r"(?m)^[ \t]*[^ \t\r\n].*?\(userId=\d+\)[ \t]*$")


def app_session_block(output: str, package: str) -> str | None:
    """Bound a session to its own heading; never read a later app's state."""
    headings = list(SESSION_HEADER.finditer(output))
    expected = re.compile(rf"sloppaTV {re.escape(package)}/sloppaTV(?:/\d+)? \(userId=\d+\)")
    package_line = re.compile(rf"(?m)^\s*package={re.escape(package)}\s*$")
    for i, heading in enumerate(headings):
        end = headings[i + 1].start() if i + 1 < len(headings) else len(output)
        block = output[heading.end() : end]
        # Android builds can append a session instance id to the heading or
        # change its display name. The exact package line is a second anchor.
        if not (expected.fullmatch(heading.group().strip()) or package_line.search(block)):
            continue
        if re.search(r"(?m)^\s*active=true\s*$", block):
            return block
    return None


def active_audio_track_lines(output: str, pid: int) -> list[str]:
    """Filter AudioFlinger playback rows: app PID, live track, active state A."""
    if pid <= 0:
        return []
    pattern = re.compile(rf"^\s*\d+\s+yes\s+{pid}\s+\d+\s+\d+\s+A(?:\s|$)")
    return [line.strip() for line in output.splitlines() if pattern.match(line)]

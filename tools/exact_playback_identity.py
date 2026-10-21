#!/usr/bin/env python3
"""Identify the exact item a SloppaTV playback request launched from app logcat."""
import argparse
import json
import re
from pathlib import Path

REQUEST = re.compile(
    r"PlaybackInfo request item=([0-9a-f]{32}) title=(.*?) "
    r"(?:series=[0-9a-f]{32} S(\d+)E(\d+) )?preferredSource="
)
SELECTED = re.compile(r"PlaybackInfo selected item=([0-9a-f]{32}).*? sources=(\d+)")
TARGET = re.compile(r"Playback target: item=([0-9a-f]{32}).*? source=([0-9a-f]{32})")


def exact_playback_identity(log: str) -> dict | None:
    """Latest request only; a selected source/session without a target is not playback."""
    current = None
    for line in log.splitlines():
        if "sloppaTV/api" not in line:
            continue
        if match := REQUEST.search(line):
            item_id, title, season, episode = match.groups()
            current = {
                "item_id": item_id,
                "title": title,
                "season": int(season) if season is not None else None,
                "episode": int(episode) if episode is not None else None,
            }
        elif current and (match := SELECTED.search(line)):
            if match.group(1) == current["item_id"]:
                current["source_count"] = int(match.group(2))
        elif current and (match := TARGET.search(line)):
            if match.group(1) == current["item_id"] and current.get("source_count", 0) > 0:
                current["source_id"] = match.group(2)
    return current if current and "source_id" in current else None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--expected-item-id", default="")
    parser.add_argument("--expected-episode", help="S1E8, for example")
    args = parser.parse_args()
    identity = exact_playback_identity(args.log.read_text(errors="replace"))
    if identity is None:
        print("No complete exact playback identity in log", flush=True)
        return 2
    print(json.dumps(identity, sort_keys=True))
    if args.expected_item_id and identity["item_id"] != args.expected_item_id.replace("-", "").lower():
        return 2
    if args.expected_episode:
        actual = f"S{identity['season']}E{identity['episode']}"
        if actual.casefold() != args.expected_episode.casefold():
            return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

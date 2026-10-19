#!/usr/bin/env python3
"""Read-only playback ledger rotation; never treats a launch as decoded proof."""

from __future__ import annotations

import argparse
import json
from datetime import UTC, datetime, timedelta
from pathlib import Path
from typing import Any


def timestamp(value: str) -> datetime:
    result = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if result.tzinfo is None:
        raise ValueError("ledger timestamps must include a timezone")
    return result


def title_identity(item: dict[str, Any]) -> str:
    return " ".join(str(item.get("title") or item["key"]).casefold().split())


def unresolved(item: dict[str, Any]) -> bool:
    status = item.get("status", "open")
    return not (status.startswith("resolved") or status in {"fixed_verified", "verified_clean"})


def select_cases(
    ledger: dict[str, Any], run_id: str, now: datetime, material_changes: set[str] | None = None
) -> list[dict[str, Any]]:
    """Explain exclusions as well as eligible cases, preserving original ledger data.

    A material change is explicitly supplied by key/root cause after observation,
    never inferred from an unrelated new commit. Gumball needs its own backend fix.
    """
    changes = material_changes or set()
    items = [item for item in ledger["items"] if unresolved(item)]
    history = [(item, attempt) for item in ledger["items"] for attempt in item.get("attempts", [])]
    previous = [(item, a) for item, a in history if a.get("run_id") and a["run_id"] != run_id and a.get("at")]
    previous_run = max(previous, key=lambda pair: timestamp(pair[1]["at"]))[1]["run_id"] if previous else None
    previous_titles = {title_identity(item) for item, a in previous if a["run_id"] == previous_run}
    counts: dict[str, int] = {}
    for item, attempt in history:
        if attempt.get("run_id") == run_id:
            cause = attempt.get("root_cause") or item.get("root_cause")
            if cause:
                counts[cause] = counts.get(cause, 0) + 1
    output = []
    for item in items:
        key = item["key"]
        cause = item.get("root_cause")
        current = [
            a for other, a in history if title_identity(other) == title_identity(item) and a.get("run_id") == run_id
        ]
        dated = [
            timestamp(a["at"]) for other, a in history if title_identity(other) == title_identity(item) and a.get("at")
        ]
        checked = timestamp(item["last_checked_at"]) if item.get("last_checked_at") else None
        if cause:
            dated.extend(
                timestamp(a["at"])
                for other, a in history
                if a.get("at") and (a.get("root_cause") or other.get("root_cause")) == cause
            )
        if cause:
            dated.extend(
                timestamp(other["last_checked_at"])
                for other in ledger["items"]
                if other.get("root_cause") == cause and other.get("last_checked_at")
            )
        last = max(dated + ([checked] if checked else []), default=None)
        changed = key in changes or (cause is not None and cause in changes)
        reason = None
        if len(current) >= 2 or (cause and counts.get(cause, 0) >= 2):
            reason = "two-attempt cap"
        elif "gumball" in key.lower() and key not in changes:
            reason = "requires relevant isolated-backend fix"
        elif title_identity(item) in previous_titles and any(title_identity(x) not in previous_titles for x in items):
            reason = "rotate away from previous run"
        elif last and now - last < timedelta(hours=24) and not changed:
            reason = "unchanged evidence: 24-hour cooldown"
        output.append(
            {
                "key": key,
                "title": item.get("title"),
                "eligible": reason is None,
                "reason": reason,
                "last_attempt": last.isoformat() if last else None,
            }
        )
    return sorted(output, key=lambda row: (not row["eligible"], row["last_attempt"] or "", row["key"]))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ledger", type=Path)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--material-change", action="append", default=[])
    args = parser.parse_args()
    print(
        json.dumps(
            select_cases(
                json.loads(args.ledger.read_text()), args.run_id, datetime.now(UTC), set(args.material_change)
            ),
            indent=2,
        )
    )


if __name__ == "__main__":
    main()

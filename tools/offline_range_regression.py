#!/usr/bin/env python3
"""Real local media/HTTP Range decoder regression; not Android playback acceptance."""

from __future__ import annotations

import argparse
import hashlib
import http.server
import json
import re
import subprocess
import tempfile
import threading
import time
from pathlib import Path
from typing import ClassVar, TypedDict


class DecodeResult(TypedDict):
    ok: bool
    bytes: int
    sha256: str
    error: str


class ModeResult(TypedDict):
    video: DecodeResult
    audio: DecodeResult
    seek_video: DecodeResult
    ranges: list[tuple[int, int]]


class RangeHandler(http.server.BaseHTTPRequestHandler):
    media: bytes = b""
    mode = "normal"
    ranges: ClassVar[list[tuple[int, int]]] = []

    def log_message(self, format: str, *args: object) -> None:
        pass

    def do_GET(self) -> None:
        if self.path != "/media.mp4":
            self.send_error(404)
            return
        match = re.fullmatch(r"bytes=(\d+)-(\d*)", self.headers.get("Range", ""))
        start = int(match[1]) if match else 0
        end = min(int(match[2]) if match and match[2] else len(self.media) - 1, len(self.media) - 1)
        if start >= len(self.media) or end < start:
            self.send_response(416)
            self.send_header("Content-Range", f"bytes */{len(self.media)}")
            self.end_headers()
            return
        self.ranges.append((start, end))
        if self.mode == "error":
            self.send_error(503)
            return
        self.send_response(206 if match else 200)
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Type", "video/mp4")
        self.send_header("Content-Length", str(end - start + 1))
        if match:
            self.send_header("Content-Range", f"bytes {start}-{end}/{len(self.media)}")
        self.end_headers()
        try:
            for pos in range(start, end + 1, 16384):
                self.wfile.write(self.media[pos : min(pos + 16384, end + 1)])
                self.wfile.flush()
                if self.mode == "slow":
                    time.sleep(0.01)
                elif self.mode == "stall":
                    time.sleep(0.2)
        except (BrokenPipeError, ConnectionResetError):
            pass


def command(args: list[str], timeout: int = 30) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(args, capture_output=True, timeout=timeout)


def decoded(url: str, *, seek: float = 0, audio: bool = False, timeout: int = 30) -> DecodeResult:
    cmd = ["ffmpeg", "-v", "error", "-nostdin", "-rw_timeout", "4000000"]
    if seek:
        cmd += ["-ss", str(seek)]
    cmd += ["-i", url, "-t", "1"]
    if audio:
        cmd += ["-map", "0:a:0", "-vn", "-f", "s16le", "-ac", "1", "-ar", "8000", "-"]
    else:
        cmd += ["-map", "0:v:0", "-an", "-pix_fmt", "rgb24", "-f", "rawvideo", "-"]
    try:
        p = command(cmd, timeout)
        data = p.stdout
        return {
            "ok": p.returncode == 0 and len(data) >= (8000 if audio else 64 * 64 * 3),
            "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
            "error": p.stderr.decode(errors="replace")[-400:],
        }
    except subprocess.TimeoutExpired:
        return {"ok": False, "error": "decode timed out", "bytes": 0, "sha256": ""}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", type=Path)
    args = ap.parse_args()
    with tempfile.TemporaryDirectory() as directory:
        media = Path(directory) / "fixture.mp4"
        p = command(
            [
                "ffmpeg",
                "-v",
                "error",
                "-nostdin",
                "-f",
                "lavfi",
                "-i",
                "testsrc2=size=128x72:rate=12:duration=8",
                "-f",
                "lavfi",
                "-i",
                "sine=frequency=740:duration=8",
                "-c:v",
                "mpeg4",
                "-q:v",
                "4",
                "-c:a",
                "aac",
                "-movflags",
                "+faststart",
                "-y",
                str(media),
            ],
            35,
        )
        if p.returncode or not media.is_file():
            raise SystemExit("fixture generation failed: " + p.stderr.decode(errors="replace")[-500:])
        RangeHandler.media = media.read_bytes()
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), RangeHandler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        url = f"http://127.0.0.1:{server.server_port}/media.mp4"
        results: dict[str, ModeResult] = {}
        try:
            for mode in ("normal", "slow", "stall", "error"):
                RangeHandler.mode = mode
                RangeHandler.ranges = []
                results[mode] = {
                    "video": decoded(url, timeout=18 if mode == "stall" else 30),
                    "audio": decoded(url, audio=True, timeout=18 if mode == "stall" else 30),
                    "seek_video": decoded(url, seek=4, timeout=18 if mode == "stall" else 30),
                    "ranges": list(RangeHandler.ranges),
                }
            baseline = results["normal"]
            assert all(baseline[k]["ok"] for k in ("video", "audio", "seek_video")), "real baseline media decode failed"
            assert baseline["video"]["sha256"] != baseline["seek_video"]["sha256"], "seek returned identical frames"
            assert results["slow"]["video"]["ok"], "slow stream could not decode"
            assert not results["error"]["video"]["ok"], "HTTP 503 incorrectly succeeded"
        finally:
            server.shutdown()
        result = {
            "fixture_bytes": len(RangeHandler.media),
            "classification": "offline_host_decoder_only",
            "android_playback_verified": False,
            "results": results,
        }
        output = json.dumps(result, indent=2)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(output + "\n")
        print(output)


if __name__ == "__main__":
    main()

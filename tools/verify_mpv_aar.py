#!/usr/bin/env python3
"""Verify the pinned libmpv AAR before an Android build or dependency upgrade."""

from __future__ import annotations

import hashlib
import pathlib
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
AAR = ROOT / "app" / "libs" / "mpv-core-no-vulkan.aar"
SIDECAR = AAR.with_suffix(AAR.suffix + ".sha256")
REQUIRED_ABIS = ("arm64-v8a", "armeabi-v7a", "x86_64")


def main() -> int:
    expected = SIDECAR.read_text(encoding="utf-8").split()[0]
    actual = hashlib.sha256(AAR.read_bytes()).hexdigest()
    if actual != expected:
        raise SystemExit(f"{AAR.relative_to(ROOT)} SHA-256 mismatch: expected {expected}, got {actual}")
    with zipfile.ZipFile(AAR) as archive:
        names = set(archive.namelist())
    missing = [f"jni/{abi}/libmpv.so" for abi in REQUIRED_ABIS if f"jni/{abi}/libmpv.so" not in names]
    if missing:
        raise SystemExit(f"{AAR.relative_to(ROOT)} is missing required native libraries: {', '.join(missing)}")
    print(f"verified {AAR.relative_to(ROOT)} ({actual})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

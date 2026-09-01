#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
APK = ROOT / "app" / "build" / "outputs" / "apk" / "release" / "app-release-unsigned.apk"


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def clean_release() -> str:
    subprocess.run(
        ["./gradlew", "--no-daemon", "clean", "assembleRelease"],
        cwd=ROOT,
        check=True,
    )
    if not APK.is_file():
        raise RuntimeError(f"Expected unsigned release APK at {APK}")
    return sha256(APK)


def main() -> int:
    first = clean_release()
    second = clean_release()
    print(f"first:  {first}")
    print(f"second: {second}")
    if first != second:
        print("Release APKs differ")
        return 1
    print("Unsigned release APK is byte-for-byte reproducible across two clean builds")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

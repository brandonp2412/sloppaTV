#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import shutil
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_LOCALE = "en-US"
METADATA_ROOT = ROOT / "fastlane" / "metadata" / "android"
IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg"}


def png_dimensions(path: Path) -> tuple[int, int]:
    header = path.read_bytes()[:26]
    if len(header) < 26 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"invalid PNG: {path}")
    return struct.unpack(">II", header[16:24])


def jpeg_dimensions(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if not data.startswith(b"\xff\xd8"):
        raise ValueError(f"invalid JPEG: {path}")
    offset = 2
    while offset + 9 < len(data):
        if data[offset] != 0xFF:
            offset += 1
            continue
        marker = data[offset + 1]
        offset += 2
        if marker in {0xD8, 0xD9}:
            continue
        if offset + 2 > len(data):
            break
        length = int.from_bytes(data[offset : offset + 2], "big")
        if marker in {0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7, 0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF}:
            if offset + 7 > len(data):
                break
            return int.from_bytes(data[offset + 5 : offset + 7], "big"), int.from_bytes(data[offset + 3 : offset + 5], "big")
        if length < 2:
            break
        offset += length
    raise ValueError(f"unable to read JPEG dimensions: {path}")


def image_dimensions(path: Path) -> tuple[int, int]:
    if path.suffix.lower() == ".png":
        return png_dimensions(path)
    return jpeg_dimensions(path)


def png_has_alpha(path: Path) -> bool:
    header = path.read_bytes()[:26]
    if len(header) < 26 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"invalid PNG: {path}")
    return header[25] in {4, 6}


def screenshot_files(source: Path) -> list[Path]:
    manifest = source / "screenshots.json"
    if manifest.is_file():
        payload = json.loads(manifest.read_text(encoding="utf-8"))
        names = [entry["file"] for entry in payload.get("screenshots", [])]
        files = [source / name for name in names]
    else:
        files = sorted(path for path in source.iterdir() if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES)
    missing = [path for path in files if not path.is_file()]
    if missing:
        raise FileNotFoundError(f"screenshot manifest references missing files: {missing}")
    return files


def sync_screenshots(source: Path, destination: Path) -> None:
    if not source.is_dir():
        raise FileNotFoundError(f"screenshot source directory does not exist: {source}")
    files = screenshot_files(source)
    if not files:
        raise RuntimeError(f"no screenshots found in {source}")
    if len(files) > 8:
        raise RuntimeError(f"Google Play accepts at most 8 Android TV screenshots; found {len(files)} in {source}")
    for path in files:
        width, height = image_dimensions(path)
        if (width, height) != (1920, 1080):
            raise RuntimeError(f"expected 1920x1080 Android TV screenshot, got {width}x{height}: {path}")

    destination.mkdir(parents=True, exist_ok=True)
    for old in destination.iterdir():
        if old.is_file() and old.suffix.lower() in IMAGE_SUFFIXES:
            old.unlink()
    for index, source_path in enumerate(files, start=1):
        name = f"{index:02d}-{source_path.stem.removeprefix(str(index).zfill(2) + '-')}{source_path.suffix.lower()}"
        shutil.copy2(source_path, destination / name)


def require_text(path: Path, limit: int) -> str:
    if not path.is_file():
        raise FileNotFoundError(path)
    value = path.read_text(encoding="utf-8").strip()
    if not value:
        raise RuntimeError(f"metadata file is empty: {path}")
    if len(value) > limit:
        raise RuntimeError(f"metadata exceeds {limit} characters ({len(value)}): {path}")
    return value


def require_image(path: Path, dimensions: tuple[int, int], *, max_bytes: int | None = None, forbid_alpha: bool = False) -> None:
    if not path.is_file():
        raise FileNotFoundError(path)
    actual = image_dimensions(path)
    if actual != dimensions:
        raise RuntimeError(f"expected {dimensions[0]}x{dimensions[1]}, got {actual[0]}x{actual[1]}: {path}")
    if max_bytes is not None and path.stat().st_size > max_bytes:
        raise RuntimeError(f"image exceeds {max_bytes} bytes: {path}")
    if forbid_alpha and path.suffix.lower() == ".png" and png_has_alpha(path):
        raise RuntimeError(f"Google Play requires a non-alpha PNG for this asset: {path}")


def validate(locale: str) -> None:
    locale_root = METADATA_ROOT / locale
    require_text(locale_root / "title.txt", 30)
    require_text(locale_root / "short_description.txt", 80)
    require_text(locale_root / "full_description.txt", 4000)

    images = locale_root / "images"
    icon = images / "icon.png"
    require_image(icon, (512, 512), max_bytes=1024 * 1024)
    if not png_has_alpha(icon):
        raise RuntimeError(f"Google Play requires a 32-bit PNG with alpha for the app icon: {icon}")
    require_image(images / "featureGraphic.png", (1024, 500), forbid_alpha=True)
    require_image(images / "tvBanner.png", (1280, 720), forbid_alpha=True)

    screenshots = sorted(path for path in (images / "tvScreenshots").iterdir() if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES)
    if not 1 <= len(screenshots) <= 8:
        raise RuntimeError(f"expected 1-8 Android TV screenshots, found {len(screenshots)}")
    for path in screenshots:
        width, height = image_dimensions(path)
        if width < 320 or height < 320 or width > 3840 or height > 3840:
            raise RuntimeError(f"screenshot dimensions outside Google Play bounds: {width}x{height}: {path}")
        if max(width, height) > 2 * min(width, height):
            raise RuntimeError(f"screenshot aspect ratio exceeds Google Play 2:1 limit: {path}")

    changelogs = locale_root / "changelogs"
    if not changelogs.is_dir() or not any(path.suffix == ".txt" for path in changelogs.iterdir()):
        raise RuntimeError(f"no release changelog metadata found in {changelogs}")

    print(f"Google Play metadata valid: {locale_root}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Sync sloppaTV screenshots into Fastlane supply metadata and validate the store listing")
    parser.add_argument("--source", type=Path, help="directory containing generated 1920x1080 screenshots")
    parser.add_argument("--locale", default=DEFAULT_LOCALE)
    parser.add_argument("--validate-only", action="store_true")
    args = parser.parse_args()

    if args.validate_only and args.source is not None:
        parser.error("--validate-only cannot be combined with --source")
    if not args.validate_only:
        if args.source is None:
            parser.error("--source is required unless --validate-only is used")
        sync_screenshots(args.source.resolve(), METADATA_ROOT / args.locale / "images" / "tvScreenshots")
    validate(args.locale)


if __name__ == "__main__":
    main()

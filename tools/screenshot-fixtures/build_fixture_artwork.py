#!/usr/bin/env python3
"""Build deterministic screenshot-only artwork variants from vendored CC sources."""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageEnhance, ImageFilter, ImageOps

ROOT = Path(__file__).resolve().parent
ART = ROOT / "artwork"


def cover(source: str, output: str, size: tuple[int, int] = (600, 900)) -> None:
    image = Image.open(ART / source).convert("RGB")
    fitted = ImageOps.fit(image, size, method=Image.Resampling.LANCZOS)
    fitted.save(ART / output, quality=88, optimize=True)


def landscape(source: str, output: str, size: tuple[int, int] = (1280, 720)) -> None:
    image = Image.open(ART / source).convert("RGB")
    fitted = ImageOps.fit(image, size, method=Image.Resampling.LANCZOS)
    fitted.save(ART / output, quality=86, optimize=True)


def episode_variant(output: str, *, crop_shift: float, brightness: float) -> None:
    image = Image.open(ART / "caminandes-backdrop.png").convert("RGB")
    width, height = image.size
    crop_width = int(width * 0.82)
    max_left = width - crop_width
    left = int(max_left * crop_shift)
    image = image.crop((left, 0, left + crop_width, height))
    image = ImageEnhance.Brightness(image).enhance(brightness)
    fitted = ImageOps.fit(image, (960, 540), method=Image.Resampling.LANCZOS)
    fitted.save(ART / output, quality=86, optimize=True)


def library_tile(source: str, output: str) -> None:
    image = Image.open(ART / source).convert("RGB")
    image = ImageOps.fit(image, (960, 540), method=Image.Resampling.LANCZOS)
    image = image.filter(ImageFilter.GaussianBlur(radius=0.6))
    image.save(ART / output, quality=84, optimize=True)


def avatar(source: str, output: str) -> None:
    image = Image.open(ART / source).convert("RGB")
    fitted = ImageOps.fit(image, (320, 320), method=Image.Resampling.LANCZOS, centering=(0.52, 0.42))
    fitted.save(ART / output, quality=88, optimize=True)


def main() -> None:
    cover("caminandes-backdrop.png", "caminandes-poster.jpg")
    cover("caminandes-backdrop.png", "caminandes-season-1.jpg", (600, 900))
    landscape("tears-of-steel-poster.png", "tears-of-steel-backdrop.jpg")
    library_tile("big-buck-bunny-backdrop.png", "movies-library.jpg")
    library_tile("caminandes-backdrop.png", "shows-library.jpg")
    avatar("big-buck-bunny-backdrop.png", "fixture-user.jpg")
    episode_variant("caminandes-llama-drama.jpg", crop_shift=0.0, brightness=0.86)
    episode_variant("caminandes-gran-dillama.jpg", crop_shift=0.48, brightness=1.0)
    episode_variant("caminandes-llamigos.jpg", crop_shift=1.0, brightness=1.12)


if __name__ == "__main__":
    main()

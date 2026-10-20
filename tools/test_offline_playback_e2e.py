"""Strict device-video evidence guards. No TV or server required."""

from __future__ import annotations

import importlib.util
from pathlib import Path
from unittest import mock

from PIL import Image, ImageDraw

MODULE_PATH = Path(__file__).with_name("offline_playback_e2e.py")
SPEC = importlib.util.spec_from_file_location("offline_playback_e2e", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


def make_frame(path: Path, *, variant: int = 0, content: bool = True) -> None:
    picture = Image.new("RGB", (320, 180), "black")
    if content:
        draw = ImageDraw.Draw(picture)
        draw.rectangle((56 + variant, 42, 240, 112), fill=(126, 61 + variant, 30))
        draw.ellipse((100, 50, 160, 100), fill=(210, 185, 100))
    else:
        ImageDraw.Draw(picture).text((7, 142), "Resolving Remux stream", fill="white")
    picture.save(path)


def test_spinner_overlay_is_not_video(tmp_path: Path) -> None:
    frame = tmp_path / "spinner.png"
    make_frame(frame, content=False)
    assert not module.video_frame_nonblank(frame)


def test_dark_letterboxed_scene_counts_as_nonblank(tmp_path: Path) -> None:
    frame = tmp_path / "video.png"
    make_frame(frame)
    assert module.video_frame_nonblank(frame)


def test_dim_story_text_counts_as_picture(tmp_path: Path) -> None:
    frame = tmp_path / "intertitle.png"
    picture = Image.new("RGB", (320, 180), "black")
    draw = ImageDraw.Draw(picture)
    for y in range(37, 83, 9):
        draw.rectangle((48, y, 190, y + 1), fill=(190, 190, 190))
    picture.save(frame)
    assert module.video_frame_nonblank(frame)


def test_black_frame_never_counts_as_video(tmp_path: Path) -> None:
    frame = tmp_path / "black.png"
    Image.new("RGB", (320, 180), "black").save(frame)
    assert not module.video_frame_nonblank(frame)


def test_frame_pair_waits_for_two_different_nonblank_images(tmp_path: Path) -> None:
    positions = iter([0, 1700, 3700, 5900])
    count = iter(range(4))

    def fake_capture(serial: str, path: Path) -> None:
        index = next(count)
        make_frame(path, variant=index * 20, content=index != 0)

    with (
        mock.patch.object(module, "capture", side_effect=fake_capture),
        mock.patch.object(
            module, "session_snapshot", side_effect=lambda *_: {"state": 3, "position_ms": next(positions)}
        ),
        mock.patch.object(module.time, "sleep"),
    ):
        progress, motion = module.decoded_frame_pair("TV", "app", tmp_path, "normal", [], 3)

    assert progress == 2000
    assert motion >= 1.0


def test_frame_pair_rejects_changing_spinner(tmp_path: Path) -> None:
    positions = iter(range(0, 100000, 2500))

    def fake_capture(serial: str, path: Path) -> None:
        make_frame(path, content=False)

    with (
        mock.patch.object(module, "capture", side_effect=fake_capture),
        mock.patch.object(
            module, "session_snapshot", side_effect=lambda *_: {"state": 3, "position_ms": next(positions)}
        ),
        mock.patch.object(module.time, "sleep", side_effect=lambda *_: None),
        mock.patch.object(module.time, "monotonic", side_effect=[0, 0, 0.1, 2]),
    ):
        try:
            module.decoded_frame_pair("TV", "app", tmp_path, "normal", [], 1)
        except RuntimeError as error:
            assert "nonblank" in str(error)
        else:
            raise AssertionError("spinner must not satisfy decoded frame proof")

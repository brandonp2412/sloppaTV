import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

import playback_report_e2e as report


class StrictPlaybackReportTests(unittest.TestCase):
    def test_advancing_session_remains_unverified_without_decoded_audio_proof(self):
        states = [
            {"paused": False, "position_ticks": 10_000_000},
            {"paused": False, "position_ticks": 45_000_000},
            {"paused": True, "position_ticks": 45_000_000},
            {"paused": True, "position_ticks": 45_000_000},
            {"paused": False, "position_ticks": 45_000_000},
        ]
        calls = []

        def fake_adb(serial, *args, **kwargs):
            calls.append(args)
            return "audio telemetry" if kwargs.get("capture") else ""

        with tempfile.TemporaryDirectory() as temp:
            with (
                patch.object(report, "adb", side_effect=fake_adb),
                patch.object(report, "capture_video_candidates", return_value=(["first.png", "later.png"], True)),
                patch.object(report, "wait_for_session", side_effect=states),
                patch.object(report, "playback_session_for_item", return_value=None),
                patch.object(report, "wait_for_stopped"),
                patch.object(report.time, "sleep"),
            ):
                result = report.run_acceptance("test-device", Mock(sessions=lambda: []), "movie-id", Path(temp))
            self.assertEqual(result["screenshots"], ["first.png", "later.png"])
            self.assertEqual(result["verification"]["status"], "SESSION_ONLY_UNVERIFIED")
            self.assertFalse(result["verification"]["decoded_video"])
            self.assertTrue(result["verification"]["nonblank_moving_frame_candidates"])
            self.assertFalse(result["verification"]["audio"])
            self.assertEqual((Path(temp) / "audio-flinger.txt").read_text(), "audio telemetry")
            self.assertFalse(any(args[:4] == ("shell", "cmd", "media_session", "volume") for args in calls))

    def test_black_intro_retries_after_seek_without_cold_start_pass(self):
        states = [
            {"paused": False, "position_ticks": 40_000_000, "runtime_ticks": 80_000_000_000},
            {"paused": False, "position_ticks": 80_000_000},
            {"paused": False, "position_ticks": 200_000_000},
            {"paused": True, "position_ticks": 200_000_000},
            {"paused": True, "position_ticks": 200_000_000},
            {"paused": False, "position_ticks": 200_000_000},
        ]
        keys = []

        def fake_adb(serial, *args, **kwargs):
            if args[:3] == ("shell", "input", "keyevent"):
                keys.append(args[-1])
            return "audio telemetry" if kwargs.get("capture") else ""

        with tempfile.TemporaryDirectory() as temp:
            with (
                patch.object(report, "adb", side_effect=fake_adb),
                patch.object(
                    report,
                    "capture_video_candidates",
                    side_effect=[(["black1", "black2"], False), (["visible1", "visible2"], True)],
                ) as capture,
                patch.object(report, "wait_for_session", side_effect=states) as sessions,
                patch.object(report, "playback_session_for_item", return_value=None),
                patch.object(report, "wait_for_stopped"),
                patch.object(report.time, "sleep"),
            ):
                result = report.run_acceptance("test-device", Mock(sessions=lambda: []), "movie-id", Path(temp))
            self.assertEqual(keys.count("KEYCODE_DPAD_RIGHT"), 12)
            self.assertEqual(capture.call_args_list[1].kwargs["prefix"], "post-seek")
            self.assertTrue(result["verification"]["nonblank_moving_frame_candidates"])
            self.assertFalse(result["verification"]["cold_start_frame_candidates"])
            self.assertTrue(result["verification"]["post_seek_only"])
            self.assertFalse(result["verification"]["decoded_video"])
            self.assertEqual(result["screenshots"], ["visible1", "visible2"])
            self.assertEqual(result["pre_seek_screenshots"], ["black1", "black2"])
            self.assertEqual(sessions.call_args_list[2].kwargs["minimum_position_ticks"], 140_000_000)

    def test_failed_post_seek_video_remains_unverified(self):
        states = [
            {"paused": False, "position_ticks": n, "runtime_ticks": 80_000_000_000}
            for n in (10_000_000, 45_000_000, 160_000_000)
        ]
        states += [{"paused": True, "position_ticks": 160_000_000}] * 2
        states += [{"paused": False, "position_ticks": 160_000_000}]
        with (
            tempfile.TemporaryDirectory() as temp,
            patch.object(report, "adb", return_value="audio telemetry"),
            patch.object(
                report, "capture_video_candidates", side_effect=[(["b1", "b2"], False), (["b3", "b4"], False)]
            ),
            patch.object(report, "wait_for_session", side_effect=states),
            patch.object(report, "playback_session_for_item", return_value=None),
            patch.object(report, "wait_for_stopped"),
            patch.object(report.time, "sleep"),
        ):
            result = report.run_acceptance("test-device", Mock(sessions=lambda: []), "movie-id", Path(temp))
        self.assertFalse(result["verification"]["nonblank_moving_frame_candidates"])
        self.assertFalse(result["verification"]["post_seek_only"])
        self.assertFalse(result["verification"]["decoded_video"])

    def test_short_episode_does_not_seek_past_end(self):
        states = [
            {"paused": False, "position_ticks": 40_000_000, "runtime_ticks": 550_000_000},
            {"paused": False, "position_ticks": 80_000_000},
            {"paused": True, "position_ticks": 80_000_000},
            {"paused": True, "position_ticks": 80_000_000},
            {"paused": False, "position_ticks": 80_000_000},
        ]
        calls = []

        def fake_adb(serial, *args, **kwargs):
            calls.append(args)
            return "audio telemetry" if kwargs.get("capture") else ""

        with (
            tempfile.TemporaryDirectory() as temp,
            patch.object(report, "adb", side_effect=fake_adb),
            patch.object(report, "capture_video_candidates", return_value=(["black1", "black2"], False)) as capture,
            patch.object(report, "wait_for_session", side_effect=states),
            patch.object(report, "playback_session_for_item", return_value=None),
            patch.object(report, "wait_for_stopped"),
            patch.object(report.time, "sleep"),
        ):
            result = report.run_acceptance("test-device", Mock(sessions=lambda: []), "movie-id", Path(temp))
        self.assertEqual(capture.call_count, 1)
        self.assertNotIn(("shell", "input", "keyevent", "KEYCODE_DPAD_RIGHT"), calls)
        self.assertIsNone(result["seek_observation"])
        self.assertFalse(result["verification"]["post_seek_only"])

    def test_frame_probe_skips_blank_and_unchanged_frames(self):
        black = bytes(48 * 27)
        first = bytes([10, 160] * (48 * 27 // 2))
        moving = bytes([160, 10] * (48 * 27 // 2))
        with (
            patch.object(report, "capture_frame", side_effect=["black", "a", "same", "b"]),
            patch.object(report, "sample_frame_luma", side_effect=[black, first, first, moving]),
            patch.object(report.time, "sleep"),
        ):
            paths, verified = report.capture_video_candidates("tv", Path("/tmp"), max_attempts=4)
        self.assertTrue(verified)
        self.assertEqual(paths, ["a", "b"])

    def test_frame_probe_does_not_verify_static_or_blank_video(self):
        black = bytes(48 * 27)
        frame = bytes([10, 160] * (48 * 27 // 2))
        self.assertFalse(report.is_nonblank_frame(black))
        self.assertTrue(report.is_nonblank_frame(frame))
        with (
            patch.object(report, "capture_frame", side_effect=["blank", "still1", "still2"]),
            patch.object(report, "sample_frame_luma", side_effect=[black, frame, frame]),
            patch.object(report.time, "sleep"),
        ):
            paths, verified = report.capture_video_candidates("tv", Path("/tmp"), max_attempts=3)
        self.assertFalse(verified)
        self.assertEqual(paths, ["still1", "still2"])


if __name__ == "__main__":
    unittest.main()

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
                patch.object(report, "capture_frame", side_effect=["first.png", "later.png"]),
                patch.object(report, "wait_for_session", side_effect=states),
                patch.object(report, "playback_session_for_item", return_value=None),
                patch.object(report, "wait_for_stopped"),
                patch.object(report.time, "sleep"),
            ):
                result = report.run_acceptance("test-device", Mock(sessions=lambda: []), "movie-id", Path(temp))
            self.assertEqual(result["screenshots"], ["first.png", "later.png"])
            self.assertEqual(result["verification"]["status"], "SESSION_ONLY_UNVERIFIED")
            self.assertFalse(result["verification"]["decoded_video"])
            self.assertFalse(result["verification"]["audio"])
            self.assertEqual((Path(temp) / "audio-flinger.txt").read_text(), "audio telemetry")
            self.assertFalse(any(args[:4] == ("shell", "cmd", "media_session", "volume") for args in calls))


if __name__ == "__main__":
    unittest.main()

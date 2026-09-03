from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]


def load_tool(name: str):
    path = ROOT / "tools" / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


benchmark_tv = load_tool("benchmark_tv")
waydroid_e2e = load_tool("waydroid_e2e")
playback_report_e2e = load_tool("playback_report_e2e")


class BenchmarkToolingTest(unittest.TestCase):
    def test_percentage_lower(self) -> None:
        self.assertEqual(benchmark_tv.percentage_lower(25.0, 100.0), 75.0)
        self.assertEqual(benchmark_tv.percentage_lower(25.0, 0.0), 0.0)

    def test_percentile_uses_upper_bucket(self) -> None:
        self.assertEqual(benchmark_tv.percentile([10.0, 20.0, 30.0, 40.0], 0.95), 40.0)


class ManifestToolingTest(unittest.TestCase):
    def test_external_video_players_are_visible_to_package_manager(self) -> None:
        manifest = ET.parse(ROOT / "app" / "src" / "main" / "AndroidManifest.xml").getroot()
        android = "{http://schemas.android.com/apk/res/android}"
        intents = manifest.findall("./queries/intent")
        self.assertTrue(
            any(
                intent.find("action") is not None
                and intent.find("action").get(android + "name") == "android.intent.action.VIEW"
                and intent.find("data") is not None
                and intent.find("data").get(android + "mimeType") == "video/*"
                and intent.find("data").get(android + "scheme") == "*"
                for intent in intents
            )
        )


class PlaybackReportToolingTest(unittest.TestCase):
    def test_selects_exact_active_sloppatv_item(self) -> None:
        sessions = [
            {"Client": "sloppaTV", "NowPlayingItem": {"Id": "abc-def", "Name": "Target"}, "PlayState": {"IsPaused": False}},
            {"Client": "Jellyfin Android TV", "NowPlayingItem": {"Id": "abc-def", "Name": "Other"}},
            {"Client": "sloppaTV", "NowPlayingItem": {}},
        ]
        selected = playback_report_e2e.playback_session_for_item(sessions, "abcdef")
        self.assertIsNotNone(selected)
        self.assertEqual(selected["NowPlayingItem"]["Name"], "Target")

    def test_rejects_ambiguous_active_sloppatv_item(self) -> None:
        sessions = [
            {"Client": "sloppaTV", "NowPlayingItem": {"Id": "abcdef"}},
            {"Client": "sloppaTV", "NowPlayingItem": {"Id": "abc-def"}},
        ]
        with self.assertRaisesRegex(RuntimeError, "Multiple active sloppaTV sessions"):
            playback_report_e2e.playback_session_for_item(sessions, "abcdef")

    def test_snapshot_exposes_only_acceptance_state(self) -> None:
        value = playback_report_e2e.snapshot(
            {
                "UserName": "viewer",
                "NowPlayingItem": {"Id": "abc", "Name": "Episode"},
                "PlayState": {
                    "PositionTicks": 123,
                    "IsPaused": True,
                    "PlayMethod": "DirectStream",
                    "AudioStreamIndex": 2,
                    "SubtitleStreamIndex": -1,
                },
            }
        )
        self.assertEqual(value["position_ticks"], 123)
        self.assertTrue(value["paused"])
        self.assertEqual(value["play_method"], "DirectStream")


class WaydroidToolingTest(unittest.TestCase):
    def test_target_model_guard_requires_explicit_physical_target(self) -> None:
        self.assertTrue(waydroid_e2e.model_matches_target("WayDroid x86_64", "waydroid"))
        self.assertFalse(waydroid_e2e.model_matches_target("Google TV Streamer", "waydroid"))
        self.assertTrue(waydroid_e2e.model_matches_target("Google TV Streamer", "google-tv-streamer"))
        self.assertFalse(waydroid_e2e.model_matches_target("SM-S931B", "google-tv-streamer"))

    def test_target_model_guard_accepts_only_sdk_models_for_ci_emulator(self) -> None:
        self.assertTrue(waydroid_e2e.model_matches_target("sdk_google_atv_x86_64", "android-tv-emulator"))
        self.assertTrue(waydroid_e2e.model_matches_target("AOSP TV on x86_64", "android-tv-emulator"))
        self.assertFalse(waydroid_e2e.model_matches_target("Google TV Streamer", "android-tv-emulator"))

    def test_power_state_parser_requires_awake(self) -> None:
        self.assertTrue(waydroid_e2e.power_state_is_awake("mWakefulness=Awake\nmWakefulnessChanging=false"))
        self.assertFalse(waydroid_e2e.power_state_is_awake("mWakefulness=Asleep\nmWakefulnessChanging=false"))

    def test_ensure_awake_wakes_sleeping_target(self) -> None:
        with patch.object(
            waydroid_e2e,
            "adb",
            side_effect=["mWakefulness=Asleep", "mWakefulness=Awake"],
        ), patch.object(waydroid_e2e, "key") as key, patch.object(waydroid_e2e.time, "sleep"):
            waydroid_e2e.ensure_awake()
        key.assert_called_once_with("WAKEUP")

    def test_fatal_log_detection_is_scoped_to_app(self) -> None:
        waydroid_e2e.PACKAGE = waydroid_e2e.DEFAULT_PACKAGE
        app_fatal = (
            "09-02 22:00:00 E AndroidRuntime: FATAL EXCEPTION: main "
            "Process: app.sloppatv, PID: 123"
        )
        other_fatal = (
            "09-02 22:00:00 E AndroidRuntime: FATAL EXCEPTION: main "
            "Process: com.example.other, PID: 456"
        )
        self.assertEqual(waydroid_e2e.fatal_lines([app_fatal]), [app_fatal])
        self.assertEqual(waydroid_e2e.fatal_lines([other_fatal]), [])

    def test_anr_detection_uses_selected_package(self) -> None:
        waydroid_e2e.PACKAGE = "app.sloppatv.custom"
        line = "09-02 22:00:00 E ActivityManager: ANR in app.sloppatv.custom"
        self.assertEqual(waydroid_e2e.fatal_lines([line]), [line])

    def test_player_acceptance_requires_active_media_session(self) -> None:
        waydroid_e2e.PACKAGE = waydroid_e2e.DEFAULT_PACKAGE
        with patch.object(waydroid_e2e, "adb", return_value="Media button session is com.example.other/player"):
            with self.assertRaisesRegex(RuntimeError, "playback is not active"):
                waydroid_e2e.require_playback_session()
        with patch.object(
            waydroid_e2e,
            "adb",
            return_value=f"Media button session is {waydroid_e2e.DEFAULT_PACKAGE}/sloppaTV",
        ):
            waydroid_e2e.require_playback_session()

    def test_search_quotes_multi_word_query_for_adb_shell(self) -> None:
        with patch.object(waydroid_e2e, "adb") as adb, patch.object(waydroid_e2e, "capture"), patch.object(
            waydroid_e2e, "audit_logs"
        ), patch.object(waydroid_e2e.time, "sleep"):
            waydroid_e2e.action_search("FOLLOW MAMA AND PAPA")
        args = adb.call_args_list[1].args
        self.assertIn("'FOLLOW MAMA AND PAPA'", args)

    def test_soak_summary_uses_median_windows_and_reports_growth(self) -> None:
        samples = [
            {"total_pss_kb": 100, "total_rss_kb": 200},
            {"total_pss_kb": 110, "total_rss_kb": 210},
            {"total_pss_kb": 105, "total_rss_kb": 205},
            {"total_pss_kb": 140, "total_rss_kb": 260},
            {"total_pss_kb": 150, "total_rss_kb": 270},
            {"total_pss_kb": 145, "total_rss_kb": 265},
        ]
        summary = waydroid_e2e.soak_summary(samples)
        self.assertEqual(summary["baseline_pss_kb"], 105)
        self.assertEqual(summary["final_pss_kb"], 145)
        self.assertEqual(summary["pss_growth_kb"], 40)
        self.assertEqual(summary["peak_pss_kb"], 150)
        self.assertEqual(summary["baseline_rss_kb"], 205)
        self.assertEqual(summary["final_rss_kb"], 265)
        self.assertEqual(summary["rss_growth_kb"], 60)

    def test_load_screenshot_suite_validates_supported_steps(self) -> None:
        suite = {
            "name": "smoke",
            "steps": [
                {"action": "restart", "wait_seconds": 1.5},
                {"action": "capture", "name": "home"},
                {"action": "key", "key": "SEARCH", "wait_seconds": 0.2},
            ],
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "suite.json"
            path.write_text(json.dumps(suite), encoding="utf-8")
            self.assertEqual(waydroid_e2e.load_screenshot_suite(path), suite)

    def test_load_screenshot_suite_rejects_unsafe_capture_names(self) -> None:
        suite = {"name": "bad", "steps": [{"action": "capture", "name": "../escape"}]}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "suite.json"
            path.write_text(json.dumps(suite), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "capture name"):
                waydroid_e2e.load_screenshot_suite(path)

    def test_png_dimensions_reads_screenshot_header(self) -> None:
        png = b"\x89PNG\r\n\x1a\n" + b"\x00\x00\x00\rIHDR" + (1920).to_bytes(4, "big") + (1080).to_bytes(4, "big")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "screen.png"
            path.write_bytes(png)
            self.assertEqual(waydroid_e2e.png_dimensions(path), (1920, 1080))

    def test_screenshot_manifest_records_capture_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "home.png"
            path.write_bytes(
                b"\x89PNG\r\n\x1a\n" + b"\x00\x00\x00\rIHDR" + (1920).to_bytes(4, "big") + (1080).to_bytes(4, "big")
            )
            entry = waydroid_e2e.screenshot_manifest_entry(path)
            self.assertEqual(entry["file"], "home.png")
            self.assertEqual(entry["width"], 1920)
            self.assertEqual(entry["height"], 1080)
            self.assertRegex(entry["sha256"], r"^[0-9a-f]{64}$")

    def test_ci_screenshot_script_requires_emulator_serial(self) -> None:
        environment = os.environ.copy()
        environment.pop("ANDROID_SERIAL", None)
        result = subprocess.run(
            [str(ROOT / "tools" / "ci_screenshots.sh")],
            cwd=ROOT,
            env=environment,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("ANDROID_SERIAL must be set", result.stderr)


if __name__ == "__main__":
    unittest.main()

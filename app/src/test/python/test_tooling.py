from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import unittest
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


class BenchmarkToolingTest(unittest.TestCase):
    def test_percentage_lower(self) -> None:
        self.assertEqual(benchmark_tv.percentage_lower(25.0, 100.0), 75.0)
        self.assertEqual(benchmark_tv.percentage_lower(25.0, 0.0), 0.0)

    def test_percentile_uses_upper_bucket(self) -> None:
        self.assertEqual(benchmark_tv.percentile([10.0, 20.0, 30.0, 40.0], 0.95), 40.0)


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

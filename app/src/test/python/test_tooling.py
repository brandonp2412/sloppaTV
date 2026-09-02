from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path

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

    def test_fatal_log_detection_is_scoped_to_app(self) -> None:
        waydroid_e2e.PACKAGE = waydroid_e2e.DEFAULT_PACKAGE
        app_fatal = (
            "09-02 22:00:00 E AndroidRuntime: FATAL EXCEPTION: main "
            "Process: nz.presley.sloppatv.test, PID: 123"
        )
        other_fatal = (
            "09-02 22:00:00 E AndroidRuntime: FATAL EXCEPTION: main "
            "Process: com.example.other, PID: 456"
        )
        self.assertEqual(waydroid_e2e.fatal_lines([app_fatal]), [app_fatal])
        self.assertEqual(waydroid_e2e.fatal_lines([other_fatal]), [])

    def test_anr_detection_uses_selected_package(self) -> None:
        waydroid_e2e.PACKAGE = "nz.presley.sloppatv.custom"
        line = "09-02 22:00:00 E ActivityManager: ANR in nz.presley.sloppatv.custom"
        self.assertEqual(waydroid_e2e.fatal_lines([line]), [line])

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


if __name__ == "__main__":
    unittest.main()

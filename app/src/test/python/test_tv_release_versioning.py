"""The TV updater must target the canonical app, never an E2E package."""

import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))
import install_tv_release as installer


class TvReleaseVersioningTest(unittest.TestCase):
    def test_release_floor_is_above_old_tv_version(self):
        self.assertEqual(installer.baseline_version(), 100700)

    def test_only_reads_canonical_package_not_testing_variants(self):
        packages = """Package [app.sloppatv.remuxtest] (a):
            versionCode=26 minSdk=26
        Package [app.sloppatv] (b):
            versionCode=100699 minSdk=26"""
        with patch.object(installer, "run", return_value=SimpleNamespace(stdout=packages)):
            self.assertEqual(installer.installed_version("test-tv"), 100699)

    def test_no_canonical_package_fails_closed(self):
        packages = "Package [app.sloppatv.remuxtest] (a): versionCode=26"
        with (
            patch.object(installer, "run", return_value=SimpleNamespace(stdout=packages)),
            self.assertRaises(RuntimeError),
        ):
            installer.installed_version("test-tv")

    def test_apk_identity_excludes_testing_package(self):
        badging = "package: name='app.sloppatv' versionCode='100700' versionName='0.1.3'"
        with patch.object(installer, "run", return_value=SimpleNamespace(stdout=badging)):
            self.assertEqual(installer.apk_identity("sample.apk"), ("app.sloppatv", 100700))


if __name__ == "__main__":
    unittest.main()

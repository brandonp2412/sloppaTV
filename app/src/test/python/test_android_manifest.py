from __future__ import annotations

import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
ANDROID = "{http://schemas.android.com/apk/res/android}"


class AndroidTvManifestTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.root = ET.parse(ROOT / "app" / "src" / "main" / "AndroidManifest.xml").getroot()
        cls.application = cls.root.find("application")
        assert cls.application is not None
        cls.activity = cls.application.find("activity")
        assert cls.activity is not None

    def test_tv_banner_is_explicit_on_application_and_launcher_activity(self) -> None:
        self.assertEqual(self.application.get(ANDROID + "banner"), "@drawable/sloppatv_banner")
        self.assertEqual(self.activity.get(ANDROID + "banner"), "@drawable/sloppatv_banner")

    def test_launcher_activity_exposes_leanback_category(self) -> None:
        categories = {
            category.get(ANDROID + "name")
            for category in self.activity.findall("./intent-filter/category")
        }
        self.assertIn("android.intent.category.LEANBACK_LAUNCHER", categories)

    def test_native_activity_keeps_tv_system_bars_hidden(self) -> None:
        source = (ROOT / "app" / "src" / "main" / "java" / "app" / "sloppatv" / "SloppaNativeActivity.java").read_text()
        self.assertIn("WindowInsets.Type.navigationBars()", source)
        self.assertIn("BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE", source)
        self.assertIn("SYSTEM_UI_FLAG_HIDE_NAVIGATION", source)
        self.assertIn("onWindowFocusChanged", source)


if __name__ == "__main__":
    unittest.main()

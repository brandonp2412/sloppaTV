from __future__ import annotations

import struct
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
ANDROID = "{http://schemas.android.com/apk/res/android}"


def png_dimensions(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise AssertionError(f"Expected PNG launcher asset: {path}")
    return struct.unpack(">II", data[16:24])


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

    def test_launcher_activity_exposes_tv_and_standard_launcher_categories(self) -> None:
        categories = {
            category.get(ANDROID + "name")
            for category in self.activity.findall("./intent-filter/category")
        }
        self.assertIn("android.intent.category.LEANBACK_LAUNCHER", categories)
        self.assertIn("android.intent.category.LAUNCHER", categories)

    def test_launcher_assets_match_google_play_tv_dimensions(self) -> None:
        self.assertEqual(self.application.get(ANDROID + "icon"), "@mipmap/ic_launcher")
        icon = ROOT / "app" / "src" / "main" / "res" / "mipmap-nodpi" / "ic_launcher.png"
        banner = ROOT / "app" / "src" / "main" / "res" / "drawable-xhdpi" / "sloppatv_banner.png"
        self.assertEqual(png_dimensions(icon), (512, 512))
        self.assertEqual(png_dimensions(banner), (320, 180))

    def test_native_activity_keeps_tv_system_bars_hidden(self) -> None:
        source = (ROOT / "app" / "src" / "main" / "java" / "app" / "sloppatv" / "SloppaNativeActivity.java").read_text()
        self.assertIn("WindowInsets.Type.navigationBars()", source)
        self.assertIn("BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE", source)
        self.assertIn("SYSTEM_UI_FLAG_HIDE_NAVIGATION", source)
        self.assertIn("onWindowFocusChanged", source)


if __name__ == "__main__":
    unittest.main()

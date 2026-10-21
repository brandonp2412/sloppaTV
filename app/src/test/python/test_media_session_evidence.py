from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))
from media_session_evidence import active_audio_track_lines, app_session_block
from waydroid_e2e import media_session_playback_state

PACKAGE = "app.sloppatv"


def session(app_state: str, *, active: bool = True) -> str:
    return (
        f"    sloppaTV app.sloppatv/sloppaTV (userId=0)\n      active={str(active).lower()}\n      state={app_state}\n"
    )


OTHER = (
    "    BluetoothMediaBrowserService com.google.android.bluetooth/service (userId=0)\n"
    "      active=true\n"
    "      state=PlaybackState {state=PLAYING(3), position=999, speed=1.0, updated=800}\n"
)


class MediaSessionEvidenceTest(unittest.TestCase):
    def test_missing_app_state_never_falls_through_to_other_app(self):
        output = session("null") + OTHER
        self.assertIsNone(media_session_playback_state(output, PACKAGE))
        app_block = app_session_block(output, PACKAGE)
        assert app_block is not None
        self.assertEqual(app_block.strip().splitlines()[-1].strip(), "state=null")

    def test_inactive_app_session_never_proves_playback(self):
        output = session("PlaybackState {state=PLAYING(3), position=100, speed=1.0}", active=False) + OTHER
        self.assertIsNone(media_session_playback_state(output, PACKAGE))
        self.assertIsNone(app_session_block(output, PACKAGE))

    def test_active_app_session_proves_only_own_state(self):
        output = OTHER + session("PlaybackState {state=BUFFERING(6), position=100, speed=0.0}") + OTHER
        self.assertEqual(media_session_playback_state(output, PACKAGE), 6)

    def test_other_package_and_no_session(self):
        self.assertIsNone(media_session_playback_state(OTHER, PACKAGE))
        self.assertIsNone(app_session_block(OTHER, PACKAGE))

    def test_audio_requires_matching_live_active_track(self):
        output = "\n".join(
            (
                "    230 yes 19402 1377 236 A 0x000 48000",
                "    231 yes 19402 1377 236 S 0x000 48000",
                "    232 yes 19402 1377 236 I 0x000 48000",
                "    233 yes 18888 1377 236 A 0x000 48000",
                "    234 no 19402 1377 236 A 0x000 48000",
            )
        )
        self.assertEqual(len(active_audio_track_lines(output, 19402)), 1)
        self.assertEqual(active_audio_track_lines(output, -1), [])

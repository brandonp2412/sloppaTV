import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))
from exact_playback_identity import exact_playback_identity


def event(message):
    return "10-22 01:34:41 I/sloppaTV/api( 6284): " + message


class ExactPlaybackIdentityTest(unittest.TestCase):
    def test_series_play_records_actual_next_up_episode(self):
        log = "\n".join([
            event("PlaybackInfo request item=6228a484cc90dc5f5bcfda888e151185 title=The Beginning series=cec4352e7580477f303e92a35046dd2e S1E8 preferredSource=6228a484cc90dc5f5bcfda888e151185"),
            event("PlaybackInfo selected item=6228a484cc90dc5f5bcfda888e151185 S1E8 elapsedMs=23 sources=1 direct=0 selected=6228a484cc90dc5f5bcfda888e151185"),
            event("Playback target: item=6228a484cc90dc5f5bcfda888e151185 S1E8 source=6228a484cc90dc5f5bcfda888e151185 method=DirectPlay"),
        ])
        identity = exact_playback_identity(log)
        self.assertEqual((identity["season"], identity["episode"]), (1, 8))
        self.assertEqual(identity["title"], "The Beginning")

    def test_unfinished_new_request_cannot_inherit_old_playback(self):
        old = "a" * 32
        new = "b" * 32
        log = "\n".join([
            event(f"PlaybackInfo request item={old} title=Old preferredSource={old}"),
            event(f"PlaybackInfo selected item={old} sources=1"),
            event(f"Playback target: item={old} source={old}"),
            event(f"PlaybackInfo request item={new} title=New preferredSource={new}"),
            event(f"PlaybackInfo selected item={new} sources=0"),
        ])
        self.assertIsNone(exact_playback_identity(log))

    def test_mismatched_source_events_are_not_evidence(self):
        a, b = "a" * 32, "b" * 32
        log = "\n".join([
            event(f"PlaybackInfo request item={a} title=Film preferredSource={a}"),
            event(f"PlaybackInfo selected item={b} sources=1"),
            event(f"Playback target: item={a} source={a}"),
        ])
        self.assertIsNone(exact_playback_identity(log))


if __name__ == "__main__":
    unittest.main()

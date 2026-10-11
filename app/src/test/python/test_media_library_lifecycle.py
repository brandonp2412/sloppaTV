"""Regression guard for Android libmpv episode-transition TLS crashes."""

from __future__ import annotations

import pathlib
import unittest

SOURCE = (pathlib.Path(__file__).resolve().parents[2] / "main" / "cpp" / "media_player.cpp").read_text()


class MediaLibraryLifecycleTests(unittest.TestCase):
    def test_stop_keeps_ffmpeg_and_mpv_loaded_for_worker_tls_cleanup(self) -> None:
        release = SOURCE.split("void NativeMediaPlayer::releaseLocked(JNIEnv* env) {", 1)[1].split(
            "void NativeMediaPlayer::startAsync(", 1
        )[0]
        self.assertIn("symbols_->terminateDestroy(mpv_);", release)
        self.assertIn("DeleteGlobalRef(surface_)", release)
        self.assertNotIn("dlclose(", release)
        self.assertNotIn("delete symbols_", release)

    def test_player_reuses_loaded_symbols_across_episodes(self) -> None:
        loader = SOURCE.split("bool NativeMediaPlayer::loadLibrariesLocked(std::string& error) {", 1)[1].split(
            "bool NativeMediaPlayer::setOptionLocked(", 1
        )[0]
        self.assertIn("if (mpvLibrary_ && avcodecLibrary_ && symbols_) return true;", loader)
        destructor = SOURCE.split("NativeMediaPlayer::~NativeMediaPlayer() {", 1)[1].split(
            "bool NativeMediaPlayer::ensureAndroidCaBundleLocked(", 1
        )[0]
        self.assertIn("delete symbols_;", destructor)
        self.assertNotIn("dlclose(", destructor)


if __name__ == "__main__":
    unittest.main()

from __future__ import annotations

import http.client
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import threading
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
retry_fixture_server = load_tool("retry_fixture_server")
screenshot_fixture_server = load_tool("screenshot_fixture_server")
sync_play_store_screenshots = load_tool("sync_play_store_screenshots")


class BenchmarkToolingTest(unittest.TestCase):
    def test_percentage_lower(self) -> None:
        self.assertEqual(benchmark_tv.percentage_lower(25.0, 100.0), 75.0)
        self.assertEqual(benchmark_tv.percentage_lower(25.0, 0.0), 0.0)

    def test_percentile_uses_upper_bucket(self) -> None:
        self.assertEqual(benchmark_tv.percentile([10.0, 20.0, 30.0, 40.0], 0.95), 40.0)

    def test_surface_layer_falls_back_to_package_when_activity_name_changes(self) -> None:
        app = benchmark_tv.App("Jellyfin", "org.jellyfin.androidtv", "activity", "MainActivity")
        layers = [
            "ActivityRecord{abc org.jellyfin.androidtv/.ui.startup.StartupActivity#1",
            "org.jellyfin.androidtv/org.jellyfin.androidtv.ui.startup.StartupActivity#2",
        ]
        self.assertEqual(
            benchmark_tv.select_active_layer(layers, app),
            "org.jellyfin.androidtv/org.jellyfin.androidtv.ui.startup.StartupActivity#2",
        )


class WaydroidSurfaceToolingTest(unittest.TestCase):
    def test_1080p_surface_accepts_landscape_and_natural_orientation(self) -> None:
        self.assertTrue(waydroid_e2e.surface_size_is_1080p("Override size: 1920x1080"))
        self.assertTrue(waydroid_e2e.surface_size_is_1080p("Physical size: 1920x1200\nOverride size: 1080x1920"))
        self.assertTrue(waydroid_e2e.surface_size_is_1080p("Physical size: 1920x1080"))
        self.assertFalse(waydroid_e2e.surface_size_is_1080p("Override size: 1280x720"))
        self.assertFalse(
            waydroid_e2e.surface_size_is_1080p("Physical size: 1920x1080\nOverride size: 1280x720")
        )


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


class RetryFixtureToolingTest(unittest.TestCase):
    def test_views_request_aborts_once_then_recovers(self) -> None:
        retry_fixture_server.RetryFixtureHandler.failed_views_once = False
        server = retry_fixture_server.ThreadingHTTPServer(("127.0.0.1", 0), retry_fixture_server.RetryFixtureHandler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            port = server.server_address[1]
            first = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
            first.request("GET", "/Users/retry-user/Views?IncludeExternalContent=false")
            with self.assertRaises(http.client.RemoteDisconnected):
                first.getresponse()
            first.close()

            second = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
            second.request("GET", "/Users/retry-user/Views?IncludeExternalContent=false")
            response = second.getresponse()
            payload = json.loads(response.read())
            second.close()
            self.assertEqual(response.status, 200)
            self.assertEqual(payload["Items"][0]["Name"], "Retry Verified")
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)


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


class ScreenshotFixtureToolingTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.server = screenshot_fixture_server.ThreadingHTTPServer(
            ("127.0.0.1", 0), screenshot_fixture_server.Handler
        )
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.port = cls.server.server_address[1]

    @classmethod
    def tearDownClass(cls) -> None:
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=2)

    def request(self, method: str, path: str, headers: dict[str, str] | None = None) -> tuple[int, dict[str, str], bytes]:
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=2)
        connection.request(method, path, headers=headers or {})
        response = connection.getresponse()
        body = response.read()
        result = response.status, {key.lower(): value for key, value in response.getheaders()}, body
        connection.close()
        return result

    def test_catalog_has_movies_series_episodes_descriptions_and_artwork(self) -> None:
        status, _, body = self.request("GET", "/Users/fixture-user/Views")
        self.assertEqual(status, 200)
        views = json.loads(body)["Items"]
        self.assertEqual([view["CollectionType"] for view in views], ["movies", "tvshows"])

        status, _, body = self.request("GET", "/Users/fixture-user/Items?ParentId=movies")
        self.assertEqual(status, 200)
        movies = json.loads(body)["Items"]
        self.assertGreaterEqual(len(movies), 9)
        self.assertIn("Elephants Dream", {item["Name"] for item in movies})
        self.assertIn("Sprite Fright", {item["Name"] for item in movies})

        status, _, body = self.request("GET", "/Users/fixture-user/Items?ParentId=shows")
        self.assertEqual(status, 200)
        series = json.loads(body)["Items"]
        self.assertGreaterEqual(len(series), 5)
        self.assertIn("Open Movie Classics", {item["Name"] for item in series})
        self.assertIn("Blender Shorts", {item["Name"] for item in series})

        status, _, body = self.request("GET", "/Items?SearchTerm=Caminandes")
        self.assertEqual(status, 200)
        values = json.loads(body)["Items"]
        self.assertIn("Series", {value["Type"] for value in values})
        self.assertIn("Episode", {value["Type"] for value in values})
        self.assertTrue(all(value.get("Overview") for value in values))
        self.assertTrue(all(value.get("ImageTags", {}).get("Primary") for value in values))

        status, headers, image = self.request("GET", "/Items/movie-big-buck-bunny/Images/Primary?maxWidth=384")
        self.assertEqual(status, 200)
        self.assertTrue(headers["content-type"].startswith("image/"))
        self.assertGreater(len(image), 10_000)

    def test_series_navigation_and_cc_playback_fixture_are_servable(self) -> None:
        status, _, body = self.request("GET", "/Shows/series-caminandes/Seasons?UserId=fixture-user")
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body)["Items"][0]["Name"], "Season 1")
        status, _, body = self.request("GET", "/Shows/series-caminandes/Episodes?UserId=fixture-user&SeasonId=season-caminandes-1")
        self.assertEqual(status, 200)
        self.assertEqual([item["Name"] for item in json.loads(body)["Items"]], ["Llama Drama", "Gran Dillama", "Llamigos"])

        status, _, body = self.request("GET", "/Shows/series-open-classics/Episodes?UserId=fixture-user")
        self.assertEqual(status, 200)
        self.assertEqual(
            [item["Name"] for item in json.loads(body)["Items"]],
            ["Big Buck Bunny", "Sintel", "Tears of Steel", "Elephants Dream", "Spring", "Coffee Run", "Sprite Fright", "Glass Half", "The Daily Dweebs"],
        )

        status, headers, body = self.request("GET", "/Videos/movie-big-buck-bunny/stream.mp4", {"Range": "bytes=0-127"})
        self.assertEqual(status, 206)
        self.assertEqual(headers["content-type"], "video/mp4")
        self.assertEqual(len(body), 128)
        self.assertTrue(headers["content-range"].startswith("bytes 0-127/"))


class WaydroidToolingTest(unittest.TestCase):
    def test_ci_screenshot_suite_visually_covers_rich_catalog_and_selects_eight_store_screens(self) -> None:
        suite = json.loads((ROOT / "tools" / "screenshot-suites" / "ci-login.json").read_text(encoding="utf-8"))
        capture_steps = [step for step in suite["steps"] if step["action"] == "capture"]
        captures = [step["name"] for step in capture_steps]
        self.assertEqual(
            captures,
            [
                "01-login",
                "02-home",
                "03-search",
                "04-search-catalog",
                "05-movie-browse",
                "06-movie-details",
                "07-cast",
                "08-person-titles",
                "09-item-menu",
                "10-player-cc-video",
                "11-player-controls",
                "12-series-browse",
                "13-series-details",
                "14-seasons",
                "15-episodes",
                "16-episode-details",
                "17-settings",
                "18-settings-options",
                "19-profiles",
            ],
        )
        self.assertEqual(sum(step.get("store") is True for step in capture_steps), 8)
        search_capture_index = next(
            index for index, step in enumerate(suite["steps"])
            if step.get("action") == "capture" and step.get("name") == "04-search-catalog"
        )
        movie_capture_index = next(
            index for index, step in enumerate(suite["steps"])
            if step.get("action") == "capture" and step.get("name") == "05-movie-browse"
        )
        self.assertIn(
            "restart",
            [step["action"] for step in suite["steps"][search_capture_index + 1 : movie_capture_index]],
        )
        person_capture_index = next(
            index for index, step in enumerate(suite["steps"])
            if step.get("action") == "capture" and step.get("name") == "08-person-titles"
        )
        menu_capture_index = next(
            index for index, step in enumerate(suite["steps"])
            if step.get("action") == "capture" and step.get("name") == "09-item-menu"
        )
        self.assertIn(
            "restart",
            [step["action"] for step in suite["steps"][person_capture_index + 1 : menu_capture_index]],
        )
        player_capture_index = next(
            index for index, step in enumerate(suite["steps"])
            if step.get("action") == "capture" and step.get("name") == "10-player-cc-video"
        )
        self.assertIn(
            "restart",
            [step["action"] for step in suite["steps"][menu_capture_index + 1 : player_capture_index]],
        )
        actions = {step["action"] for step in suite["steps"]}
        self.assertIn("text", actions)
        self.assertIn("search", actions)
        self.assertIn("fixture_log", actions)
        text_steps = [step for step in suite["steps"] if step["action"] == "text"]
        self.assertTrue(all(step.get("wait_seconds", 0) >= 1 for step in text_steps))
        server_text = next(step["value"] for step in text_steps if step.get("value", "").startswith("http://"))
        self.assertEqual(server_text, "http://127.0.0.1:1024")
        fixture_assertions = [step["contains"] for step in suite["steps"] if step["action"] == "fixture_log"]
        self.assertIn("POST /Users/AuthenticateByName", fixture_assertions)
        self.assertIn("GET /Users/fixture-user/Views", fixture_assertions)
        self.assertIn("SearchTerm=Caminandes", fixture_assertions)
        self.assertIn("GET /Shows/series-open-classics/Seasons", fixture_assertions)
        self.assertIn("GET /Shows/series-open-classics/Episodes", fixture_assertions)
        self.assertIn("POST /Items/movie-big-buck-bunny/PlaybackInfo", fixture_assertions)

    def test_main_branch_pipeline_commits_generated_store_screenshots(self) -> None:
        workflow = (ROOT / ".github" / "workflows" / "android.yml").read_text(encoding="utf-8")
        self.assertIn("publish-screenshots:", workflow)
        self.assertIn("needs: screenshots", workflow)
        self.assertIn("python3 tools/sync_play_store_screenshots.py --source artifacts/ci-screenshots", workflow)
        self.assertIn("git commit -m \"Update Android TV screenshots [skip ci]\"", workflow)
        self.assertIn("target: android-tv", workflow)
        script = (ROOT / "tools" / "ci_screenshots.sh").read_text(encoding="utf-8")
        self.assertIn("screenshot_fixture_server.py", script)
        self.assertIn('SLOPPATV_FIXTURE_PORT:-18096', script)
        self.assertIn('reverse tcp:1024 "tcp:$FIXTURE_PORT"', script)
        self.assertIn("POST /Users/AuthenticateByName", script)

    def test_target_model_guard_requires_explicit_physical_target(self) -> None:
        self.assertTrue(waydroid_e2e.model_matches_target("WayDroid x86_64", "waydroid"))
        self.assertFalse(waydroid_e2e.model_matches_target("Google TV Streamer", "waydroid"))
        self.assertTrue(waydroid_e2e.model_matches_target("Google TV Streamer", "google-tv-streamer"))
        self.assertFalse(waydroid_e2e.model_matches_target("SM-S931B", "google-tv-streamer"))

    def test_target_model_guard_accepts_only_tv_models_for_ci_emulator(self) -> None:
        self.assertTrue(waydroid_e2e.model_matches_target("sdk_google_atv_x86_64", "android-tv-emulator"))
        self.assertTrue(waydroid_e2e.model_matches_target("sdk_google_atv64_x86_64", "android-tv-emulator"))
        self.assertTrue(waydroid_e2e.model_matches_target("AOSP TV on x86_64", "android-tv-emulator"))
        self.assertFalse(waydroid_e2e.model_matches_target("sdk_gphone64_x86_64", "android-tv-emulator"))
        self.assertFalse(waydroid_e2e.model_matches_target("Google TV Streamer", "android-tv-emulator"))

    def test_capture_guard_requires_sloppatv_to_be_foreground(self) -> None:
        resumed = "mResumedActivity: ActivityRecord{123 app.sloppatv/app.sloppatv.SloppaNativeActivity}"
        launcher = "mResumedActivity: ActivityRecord{123 com.google.android.tvlauncher/.MainActivity}"
        stopped = "ActivityRecord{123 app.sloppatv/app.sloppatv.SloppaNativeActivity}"
        self.assertTrue(waydroid_e2e.foreground_is_package(resumed, waydroid_e2e.DEFAULT_PACKAGE))
        self.assertFalse(waydroid_e2e.foreground_is_package(launcher, waydroid_e2e.DEFAULT_PACKAGE))
        self.assertFalse(waydroid_e2e.foreground_is_package(stopped, waydroid_e2e.DEFAULT_PACKAGE))

    def test_launch_waits_for_sloppatv_to_be_foreground(self) -> None:
        launcher = "topResumedActivity=ActivityRecord{123 com.android.launcher3/.Launcher}"
        sloppa = "topResumedActivity=ActivityRecord{456 app.sloppatv/.SloppaNativeActivity}"
        with patch.object(waydroid_e2e, "adb", side_effect=["", launcher, sloppa]) as adb, patch.object(
            waydroid_e2e.time, "sleep", return_value=None
        ):
            waydroid_e2e.launch()
        self.assertEqual(adb.call_count, 3)
        self.assertTrue(adb.call_args_list[1].kwargs["capture"])
        self.assertTrue(adb.call_args_list[2].kwargs["capture"])

    def test_process_pid_treats_missing_process_as_stopped(self) -> None:
        failure = subprocess.CalledProcessError(1, ["adb", "shell", "pidof", waydroid_e2e.DEFAULT_PACKAGE])
        with patch.object(waydroid_e2e, "adb", side_effect=failure):
            self.assertEqual(waydroid_e2e.process_pid(), "")

    def test_ensure_running_keeps_existing_process(self) -> None:
        with patch.object(waydroid_e2e, "process_pid", return_value="123") as process_pid, patch.object(
            waydroid_e2e, "launch"
        ) as launch, patch.object(waydroid_e2e, "require_running") as require_running:
            self.assertEqual(waydroid_e2e.ensure_running(), "123")
        process_pid.assert_called_once_with()
        launch.assert_not_called()
        require_running.assert_not_called()

    def test_ensure_running_launches_stopped_app(self) -> None:
        with patch.object(waydroid_e2e, "process_pid", return_value="") as process_pid, patch.object(
            waydroid_e2e, "launch"
        ) as launch, patch.object(waydroid_e2e, "require_running", return_value="456") as require_running:
            self.assertEqual(waydroid_e2e.ensure_running(), "456")
        process_pid.assert_called_once_with()
        launch.assert_called_once_with()
        require_running.assert_called_once_with()

    def test_power_state_parser_requires_awake(self) -> None:
        self.assertTrue(waydroid_e2e.power_state_is_awake("mWakefulness=Awake\nmWakefulnessChanging=false"))
        self.assertFalse(waydroid_e2e.power_state_is_awake("mWakefulness=Asleep\nmWakefulnessChanging=false"))

    def test_screenshot_pull_reconnects_after_transient_adb_disconnect(self) -> None:
        failure = subprocess.CalledProcessError(1, ["adb", "pull"])
        with tempfile.TemporaryDirectory() as directory:
            local = Path(directory) / "screen.png"
            local.write_bytes(b"partial")
            with patch.object(waydroid_e2e, "adb", side_effect=[failure, "", ""]) as adb, patch.object(
                waydroid_e2e.time, "sleep"
            ):
                waydroid_e2e.pull_with_reconnect("/sdcard/screen.png", local)
            self.assertFalse(local.exists())
            self.assertEqual(adb.call_args_list[1].args, ("wait-for-device",))
            self.assertEqual(adb.call_args_list[2].args[:2], ("pull", "/sdcard/screen.png"))

    def test_screencap_stream_retries_without_sdcard_staging(self) -> None:
        failure = subprocess.TimeoutExpired(["adb", "exec-out", "screencap"], 30)
        png = b"\x89PNG\r\n\x1a\nfixture"
        with tempfile.TemporaryDirectory() as directory:
            local = Path(directory) / "screen.png"
            local.write_bytes(b"partial")
            with patch.object(waydroid_e2e, "screencap_bytes", side_effect=[failure, png]), patch.object(
                waydroid_e2e, "adb", return_value=""
            ) as adb, patch.object(waydroid_e2e.time, "sleep"):
                waydroid_e2e.screencap_with_reconnect(local)
            self.assertEqual(local.read_bytes(), png)
            adb.assert_called_once_with("wait-for-device", timeout=45.0)

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

    def test_fatal_log_detection_handles_android_split_process_line(self) -> None:
        waydroid_e2e.PACKAGE = waydroid_e2e.DEFAULT_PACKAGE
        fatal = "09-02 22:00:00 E AndroidRuntime: FATAL EXCEPTION: main"
        process = "09-02 22:00:00 E AndroidRuntime: Process: app.sloppatv, PID: 123"
        self.assertEqual(waydroid_e2e.fatal_lines([fatal, process]), [fatal])

    def test_anr_detection_uses_selected_package(self) -> None:
        waydroid_e2e.PACKAGE = "app.sloppatv.custom"
        line = "09-02 22:00:00 E ActivityManager: ANR in app.sloppatv.custom"
        self.assertEqual(waydroid_e2e.fatal_lines([line]), [line])

    def test_player_acceptance_requires_active_media_session(self) -> None:
        waydroid_e2e.PACKAGE = waydroid_e2e.DEFAULT_PACKAGE
        with patch.object(waydroid_e2e, "adb", return_value="Media button session is com.example.other/player"), patch.object(
            waydroid_e2e.time, "sleep"
        ):
            with self.assertRaisesRegex(RuntimeError, "playback is not active"):
                waydroid_e2e.require_playback_session(0.001)
        active = (
            f"sloppaTV {waydroid_e2e.DEFAULT_PACKAGE}/sloppaTV (userId=0)\n"
            "  active=true\n"
            "  state=PlaybackState {state=3, position=1000, buffered position=0, speed=1.0, error=null}"
        )
        with patch.object(waydroid_e2e, "adb", return_value=active):
            waydroid_e2e.require_playback_session()

    def test_player_acceptance_parses_named_android_playback_state(self) -> None:
        waydroid_e2e.PACKAGE = waydroid_e2e.DEFAULT_PACKAGE
        active = (
            f"sloppaTV {waydroid_e2e.DEFAULT_PACKAGE}/sloppaTV (userId=0)\n"
            "  active=true\n"
            "  state=PlaybackState {state=PLAYING(3), position=1000, buffered position=0, speed=1.0, error=null}"
        )
        self.assertEqual(
            waydroid_e2e.media_session_playback_state(active, waydroid_e2e.DEFAULT_PACKAGE),
            3,
        )
        with patch.object(waydroid_e2e, "adb", return_value=active):
            waydroid_e2e.require_playback_session()

    def test_player_acceptance_rejects_registered_but_stopped_session(self) -> None:
        waydroid_e2e.PACKAGE = waydroid_e2e.DEFAULT_PACKAGE
        stopped = (
            f"sloppaTV {waydroid_e2e.DEFAULT_PACKAGE}/sloppaTV (userId=0)\n"
            "  active=true\n"
            "  state=PlaybackState {state=1, position=0, buffered position=0, speed=0.0, error=null}"
        )
        with patch.object(waydroid_e2e, "adb", return_value=stopped), patch.object(waydroid_e2e.time, "sleep"):
            with self.assertRaisesRegex(RuntimeError, "state was 1"):
                waydroid_e2e.require_playback_session(0.001)

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

    def test_load_screenshot_suite_accepts_deterministic_search_steps(self) -> None:
        suite = {"name": "search", "steps": [{"action": "search", "query": "Caminandes"}]}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "suite.json"
            path.write_text(json.dumps(suite), encoding="utf-8")
            self.assertEqual(waydroid_e2e.load_screenshot_suite(path), suite)

    def test_load_screenshot_suite_accepts_playback_assertion(self) -> None:
        suite = {"name": "player", "steps": [{"action": "playback_session", "timeout_seconds": 8}]}
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

    def test_wait_for_fixture_log_detects_expected_request(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            artifact_dir = Path(directory)
            (artifact_dir / "fixture-server.log").write_text(
                '"POST /Users/AuthenticateByName HTTP/1.1" 200 -\n',
                encoding="utf-8",
            )
            with patch.object(waydroid_e2e, "ARTIFACTS", artifact_dir):
                waydroid_e2e.wait_for_fixture_log("POST /Users/AuthenticateByName", 0.1)

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
            entry = waydroid_e2e.screenshot_manifest_entry(path, True)
            self.assertEqual(entry["file"], "home.png")
            self.assertEqual(entry["width"], 1920)
            self.assertEqual(entry["height"], 1080)
            self.assertTrue(entry["store"])
            self.assertRegex(entry["sha256"], r"^[0-9a-f]{64}$")

    def test_store_sync_uses_only_manifest_entries_marked_for_store(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / "visual-only.png").write_bytes(b"visual")
            (source / "store.png").write_bytes(b"store")
            (source / "screenshots.json").write_text(
                json.dumps(
                    {
                        "screenshots": [
                            {"file": "visual-only.png", "store": False},
                            {"file": "store.png", "store": True},
                        ]
                    }
                ),
                encoding="utf-8",
            )
            self.assertEqual(sync_play_store_screenshots.screenshot_files(source), [source / "store.png"])

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

    def test_ci_screenshot_script_rejects_non_emulator_by_default(self) -> None:
        environment = os.environ.copy()
        environment["ANDROID_SERIAL"] = "192.168.240.2:5555"
        environment.pop("SLOPPATV_SCREENSHOT_TARGET", None)
        result = subprocess.run(
            [str(ROOT / "tools" / "ci_screenshots.sh")],
            cwd=ROOT,
            env=environment,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Refusing non-emulator ANDROID_SERIAL", result.stderr)

    def test_ci_screenshot_script_allows_explicit_waydroid_target(self) -> None:
        environment = os.environ.copy()
        environment["ANDROID_SERIAL"] = "192.168.240.2:5555"
        environment["SLOPPATV_SCREENSHOT_TARGET"] = "waydroid"
        environment.pop("SLOPPATV_APK", None)
        result = subprocess.run(
            [str(ROOT / "tools" / "ci_screenshots.sh")],
            cwd=ROOT,
            env=environment,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("Refusing non-emulator ANDROID_SERIAL", result.stderr)
        self.assertIn("SLOPPATV_APK must point to a built APK", result.stderr)


if __name__ == "__main__":
    unittest.main()

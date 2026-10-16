from __future__ import annotations

import importlib.util
from pathlib import Path
from unittest import mock

MODULE_PATH = Path(__file__).with_name("waydroid_e2e.py")
SPEC = importlib.util.spec_from_file_location("waydroid_e2e", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


def test_navigation_cycle_sends_events_and_checks_foreground() -> None:
    foreground = f"topResumedActivity={module.PACKAGE}/.MainActivity"
    with (
        mock.patch.object(module, "key") as key,
        mock.patch.object(module, "adb", return_value=foreground) as adb,
        mock.patch.object(module.time, "sleep"),
    ):
        module.navigation_cycle(3)

    assert [call.args[0] for call in key.call_args_list] == ["RIGHT", "RIGHT", "RIGHT"]
    adb.assert_called_once_with(
        "shell",
        "dumpsys",
        "activity",
        "activities",
        capture=True,
        timeout=60.0,
    )


def test_soak_rejects_negative_navigation_events_before_adb() -> None:
    with mock.patch.object(module, "adb") as adb:
        try:
            module.soak(60, 60, -1)
        except SystemExit as error:
            assert "navigation events" in str(error)
        else:
            raise AssertionError("negative navigation events should be rejected")
    adb.assert_not_called()


def test_action_view_uses_registered_deep_link() -> None:
    with (
        mock.patch.object(module, "adb") as adb,
        mock.patch.object(module.time, "sleep"),
        mock.patch.object(module, "capture"),
        mock.patch.object(module, "audit_logs"),
    ):
        module.action_view("0404321c67af5cb294728fbc7ad250e0")

    adb.assert_any_call(
        "shell",
        "am",
        "start",
        "-a",
        "android.intent.action.VIEW",
        "-d",
        "sloppatv://item/0404321c67af5cb294728fbc7ad250e0",
        "-n",
        module.COMPONENT,
    )

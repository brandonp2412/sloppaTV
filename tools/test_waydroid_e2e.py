from __future__ import annotations

import importlib.util
from pathlib import Path
from unittest import mock

MODULE_PATH = Path(__file__).with_name("waydroid_e2e.py")
SPEC = importlib.util.spec_from_file_location("waydroid_e2e", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


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

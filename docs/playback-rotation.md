# Playback failure rotation

Run `python tools/playback_rotation.py /home/brandon/.local/state/prompta/sloppatv-remux-failures.json --run-id RUN` before choosing unresolved ledger titles. The command reads the ledger without changing it and prints eligibility and exclusion reasons. Preserve episode IDs and exact titles; do not count search/navigation as playback attempts.

Record every launched case promptly with `run_id`, timezone-aware `at`, evidence path, device/package/backend identity and, when established, a stable `root_cause`. Use the same root-cause identifier across titles affected by the same failure. Unknown causes should remain absent rather than guessing that a source is dead. Append strict video/audio/position proof separately from launch, metadata and session evidence.

The checker caps both title and known root cause at two attempts per run. It excludes unchanged evidence for 24 hours, and rotates away from the immediately previous run whenever other unresolved cases exist. Resolved variants such as `resolved_verified_device_playback` are excluded. Supply `--material-change KEY_OR_CAUSE` only after recording a relevant new fix or observation; this does not override the attempt cap or consecutive-run rotation. Gumball S6E1 requires its own exact key as a documented relevant isolated-backend fix.

Before acting, inspect worker state and fresh evidence directories too: an operator may not yet have appended its ledger results. The checker is advisory and does not reserve the TV, authenticate, drive playback or acquire device locks. Use the working main/release app immediately if isolated access fails. Physical Google TV owner authorization permits interrupting playback and changing resume state; never reset, uninstall or wipe accounts or packages, and never deploy an unverified app/service. Report physical and Waydroid evidence separately.

Validate changes through the shared queue:

```sh
buildq --wait -- env PYTHONPATH=tools python -m unittest discover -s tools -p test_playback_rotation.py -v
```

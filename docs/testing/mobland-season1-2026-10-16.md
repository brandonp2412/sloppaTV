# MobLand season 1 playback verification — 2026-10-16

All ten episodes passed on the physical Google TV Streamer using the existing SloppaTV Remux account. Each check launched the episode through ADB, selected Play in SloppaTV, verified actual playback-clock progress and active AudioFlinger output, exercised pause/resume and a short seek, then sought to the midpoint and near the end. Two screenshots from each region were opened and visually reviewed for changing decoded imagery. Original watched, play-count and resume state was restored after each episode.

These are end-to-end playback samples across each episode, rather than continuous full-runtime viewing. Cold torrent buffering and source fallback were allowed; the runner required measured clock advancement and audio output before accepting a sample.

## Findings and fixes

- The original S1E2 attempt returned PlaybackInfo HTTP 500 after 75.5 seconds. Healthy downloading sources could exhaust probing/readiness deadlines; idle cached torrents were also rejected using current swarm throughput. Remux now verifies the requested range, avoids unrelated EOF reads during startup, aligns socket and overall probe deadlines, and preserves verified pieces for retries.
- SloppaTV advertised Android audio decoders even though embedded FFmpeg decodes audio. Advertising the bundled software decoders allows TrueHD playback locally instead of unnecessary server audio conversion.
- Remux Matroska playback now uses FFmpeg demuxing. Native Matroska seeking produced corruption/premature EOF with sparse remote input.
- A render-frame comparison caused false buffering/stall labels during normal small clock increments. The indicator now accumulates progress against its last confirmed advance.
- S1E4 exposed premature EOF after a failed distant seek. SloppaTV now retries the same episode at the requested position instead of marking it complete and advancing. The physical-device test exercised this recovery at 40:30 and resumed video/audio through a replacement source.

## Results

| Episode | Title | Startup | Midpoint sample | Near-end sample | Result |
|---|---|---:|---|---|---|
| S1E1 | Stick or Twist | 9.3s | 30:10–30:20 | 58:40–58:50 | PASS |
| S1E2 | Jigsaw Puzzle | 25.1s | 26:10–26:20 | 50:40–50:50 | PASS |
| S1E3 | Plan B | 34.6s | 24:10–24:20 | 46:40–46:50 | PASS |
| S1E4 | Rat Trap | 30.9s | 21:10–21:20 | 40:40–40:50 | PASS |
| S1E5 | Funeral for a Friend | 21.6s | 21:08–21:18 | 40:40–40:50 | PASS |
| S1E6 | Antwerp Blues | 44.6s | 21:37–21:47 | 41:40–41:50 | PASS |
| S1E7 | The Crossroads | 7.2s | 20:43–20:53 | 39:40–39:51 | PASS |
| S1E8 | Helter Skelter | 3.6s | 22:40–22:50 | 43:40–43:50 | PASS |
| S1E9 | Beggars Banquet | 3.6s | 24:10–24:20 | 46:40–46:50 | PASS |
| S1E10 | The Beast in Me | 7.2s | 27:40–27:50 | 53:40–53:50 | PASS |

## Validation and evidence

- Android debug and signed release builds passed.
- The host test suite passed, including the audio policy, high-frequency buffer indicator, and premature seek EOF regressions.
- Remux: 38 probe tests and 11 stream/range tests passed.
- Python runner: Ruff, formatting, type checks, and real ADB execution passed.
- [Machine-readable results](mobland-season1-2026-10-16.json) contain item IDs, decoder telemetry, measured clock samples, hashes and local evidence paths.
- Local screenshots, complete per-episode samples and logs: `artifacts/e2e-physical-tv/mobland/`. The evidence directory is ignored by git; compact contact sheets are under its `review/` directory.
- Original failure and exercised episode-4 seek recovery logs are under the local evidence directory’s `diagnosis/` subdirectory.

## Deployment

App code: `31ca6353` (plus the preceding playback fix `e14d97c7`). Remux code: `c8a20eed` (Cargo development build, as authorized). The deployed Remux binary hash was checked against the compiled artifact, and `remux.service` remained active. Previous backend binary/database backup: `/var/lib/remux/e2e-backups/mobland-20261016/` on Nox.

Work stopped at the user’s request during the final recheck. All ten episodes passed the earlier debug-build samples. The subsequent release S1E2 check played video/audio but failed its midpoint seek after a direct-stream HTTP 400 led to server conversion. Commit `aa91d5e5` adds alternative-source retry before conversion; its build and host tests passed, but its physical-device playback recheck was interrupted and is unverified. Original watch state was independently checked/restored for all ten episodes. The latest signed release is installed as part of wrap-up; no successful final-release end-to-end claim is made.

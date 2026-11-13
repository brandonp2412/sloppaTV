# Subtitle dialogue matching on Glass — 13 November 2026

This follows [the release and decoder-clock investigation](subtitle-sync-validation-2026-11-13.md). The speech-activity matcher alone rejected valid Friends windows because laughter and other sounds produced competing peaks. English subtitles now have an independent on-device speech-recognition path.

## Matching and release boundaries

The app recognizes audio without supplying subtitle text as a prompt. It matches unique phrases of at least four words, then checks one offset or supported frame-rate conversion against three separated windows spanning at least ten minutes. Fixed corrections need four independent phrases per window. Speed corrections need six. At least 80% of each window’s anchors must lie within 500 ms of the fitted timeline, and its median residual must agree within 250 ms. Competing speed fits are rejected.

Candidates include 24/23.976 and both PAL conversions and their inverses. Conflicting recognized dialogue vetoes a coincidental speech-activity match. Unsupported drift, edit discontinuities, repeated phrases and wrong subtitles remain rejection cases. Sparse opening dialogue triggers bounded probes of the next two 90-second windows before a longer sample. Initial probes leave at least five minutes before EOF to avoid truncated end windows. This corrected a valid late-session rejection without changing the matching thresholds. Late HLS restarts can verify earlier windows of the same release through independent probe sessions.

Successful corrections are cached only for the same generation, item, source fingerprint, play session, media URL, HLS base, audio track, subtitle index, cue contents and timeline origin. Off/on can reuse a correction. Changing those identities requires fresh evidence. Each completed independent probe session is stopped before opening the next; the active playback session is never used for probe cleanup.

## Device observations

All installations and device commands used Glass Waydroid at `192.168.240.112:5555`, package `app.sloppatv.remuxtest`. The real TV streamer was not contacted.

| Test | Observed result |
| --- | --- |
| Friends embedded English SRT 6, HLS base 385,000 ms, v34 | Three windows beginning at 390,018, 715,710 and 1,041,423 ms supplied 26 matched phrases. Applied +5,193 ms, scale 1.0. |
| Friends late seek to 1,110,000 ms within HLS base 0, v36 | Three verification windows supplied 34 phrases. Applied +97 ms, scale 1.0. Opening music was skipped with a bounded next-window probe. |
| Friends resumed HLS base 1,173,000 ms, v37 | Three windows supplied 35 phrases. Applied +3,204 ms, scale 1.0; earlier-window probes retained the main session’s timeline origin. |
| Same v37 session, paused off/on | Position stayed exactly 1,252,329 ms, load/release counts unchanged; immediately reused +3,204 ms. |
| Provider SRT 7, same v36 session | Rejected: recognized dialogue did not establish one consistent episode timeline. |
| Provider-to-embedded switchback, v36 | Position stayed exactly 1,173,402 ms with unchanged load/release counts. Reused the embedded track’s +97 ms; no provider correction was stored. |
| Same session, paused off/on | Position stayed exactly 764,796 ms; player-load and release counts stayed unchanged. Reused +5,193 ms immediately. |
| Friends embedded English SRT 6, HLS base 0, v35 | Three windows supplied 36 phrases. Applied +123 ms, scale 1.0. The opening music caused a slow longer-sample retry, subsequently replaced by bounded forward probes. |

Independent RMS-envelope comparison of four v36 probe recordings against the retained full episode found onset differences of 40–144 ms (correlations 0.90–0.94), corroborating their clock anchors.

The different corrections belong to different HLS timelines. They are not interchangeable offsets for one immutable source clock.

An independent base.en transcription of retained v34 audio found 13 matching phrases with median onset difference 4,872 ms, 321 ms earlier than the app correction. Speech-model timestamps and editorial cue onsets have uncertainty of several hundred milliseconds. These observations support useful automatic alignment, not frame-perfect timing. A paused screenshot showed the expected corrected cue at 12:45. Waydroid often renders video as a solid colour, so decoded-frame telemetry, subtitle rendering, clocks and retained audio were used together.

The release investigation already established that Psychodrama’s embedded ASS tracks align at the beginning, middle and end and preserve libmpv styling. Friends provider SRT 7 has an approximately 19-second edit discontinuity; one global offset or supported frame-rate conversion cannot repair it. No claim is made that the external releases have been repaired.

## Model and resource use

The recognizer is whisper.cpp v1.9.5 at revision `d1be6fde11ac6e0407606b4e42fe72d34add8037`, CPU only. Its source archive is pinned by SHA-256. The English tiny Q5_1 model downloads on first use into private app storage, verified against its exact 32,166,155-byte size and SHA-256 before loading. A real cold download succeeded in the isolated app. Audio and recognized words remain on the device; transcripts are not logged or uploaded. Licenses ship in the app assets.

Optimized Android inference took about 9–15 seconds per 90-second clip on Glass, excluding server negotiation and audio capture. A full decision requires multiple probes and can take minutes when Remux is slow. Cancelling a request aborts recognition and invalidates its completion. English speech matching is currently the supported recognition path; other languages retain guarded speech-activity matching. Unavailable or corrupt models cannot authorize a speech correction.

## Regression and build coverage

Host tests cover real numeric speech anchors, fixed offsets, all supported speed ratios, incompatible edits, unsupported drift, ambiguous and repeated phrases, malformed evidence, advertisements and music. Separate tests cover verified model caching, download failure, cancellation, correction-cache identity changes, earlier HLS verification windows and independent-session cleanup. Media and transcripts remain in ignored local evidence, outside the repository.

Local checks passed: host C++ suite and 79 Python tests, cppcheck, clang-tidy over 46 translation units, and x86_64, arm64-v8a and armeabi-v7a Android compilation. Further live observations and final CI are recorded below.

## Cancellation and final session checks

Switching audio during recognition invalidated the old request. The following English request recognized fresh dialogue successfully. A later Friends provider probe was cancelled by leaving playback and opening Psychodrama; no Friends correction was applied after the source change. The v37 late restart obtained a new correction rather than reusing either v34 or v36 results.

Local evidence includes per-process probe snapshots and paired logs, independent transcripts, acoustic comparisons and paused screenshots under `build/host-tests/subtitle-finish-20261113/`. Counter-based WAV names are interpreted only with their paired process and request logs.

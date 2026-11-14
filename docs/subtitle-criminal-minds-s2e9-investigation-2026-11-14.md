# Criminal Minds S2E9: reference audio and HLS clock investigation

## Historical status at the first investigation

This section records the initial investigation before the LAPSE implementation.
See [the implementation and acceptance report](subtitle-lapse-validation-2026-11-14.md)
for subsequent work.

**At that point, no automatic piecewise correction had been implemented or verified.**
The five-region emulator acceptance test has not passed. No runtime synchronization
code was changed in this investigation. The existing rejection of an inconsistent
global timeline remains in place. No manual subtitle delay was applied.

The original hand-off measurements combined subtitle discontinuities with an
additional HLS probe clock error. They must not be used directly as episode-relative
retiming anchors.

## Reproduction

- Glass headless AVD `remux_subtitles_pixel5_api35`, serial `emulator-5580`.
- Installed the existing debug APK, versionCode 100709, package `app.sloppatv`.
- Criminal Minds S2E9, item/source `8bc8e2a4b416592db2940f70c0f31584`.
- Remux selected the HEVC/AAC MP4 release after 156,377 ms of source negotiation.
- External English SubRip stream 2 is byte-for-byte identical to the retained
  791-cue SRT. SHA-256:
  `9a247b017a3892310a095e4926cad2a9d81e6ecee99cfb2430d85634080a3f82`.
- Actual video playback and the subtitle picker were visually inspected. A paused
  19:14 capture shows the external subtitle enabled. Rendering is not sync proof.
- The app logged `recognized dialogue did not establish one consistent episode
  timeline`; no automatic correction was applied.
- The real Google TV Streamer was not contacted or installed on.

## Complete source reference

The static MP4 initially stopped at 598,815,062 bytes even though its container
advertised the full duration. FFmpeg decoded that partial file without a failing
exit status. The first transcription therefore stopped near 29:16 and is not a
complete reference.

A range request resumed the download, verifying an 8,192-byte overlap against the
existing file before appending. The completed file has 901,084,615 bytes and
SHA-256 `6883874db686c2242cf68cd6c6217bf65d0f4f6d64270b7dd61cd6a5543a1b8f`.
The stream duration is 2,666.539 seconds, versus the library's 2,580 seconds.
The complete independent `faster-whisper-base.en` transcription reaches the final
spoken scene at about 43:39. No subtitle text was supplied as a prompt.

The following are descriptive measurements against the MP4 audio clock. Positive
values mean the subtitle starts before the spoken phrase. Phrase matching used
normalized dialogue, rejected competing onsets, and retained similarity >= 0.95
for this table. These are not active-player residuals or a fitted correction.

| Reference audio region | Matched phrases | Median onset difference | Phrases within 500 ms of median |
| --- | ---: | ---: | ---: |
| 00:00–06:00 | 49 | -213 ms | 44 |
| 07:00–16:00 | 77 | +1,774 ms | 73 |
| 17:00–25:00 | 76 | +3,463.5 ms | 75 |
| 25:00–29:10 | 46 | +5,539.5 ms | 45 |
| 29:30–33:00 | 37 | +7,585 ms | 34 |
| 34:00–44:00 | 103 | +9,811 ms | 90 |

The roughly constant plateaus separated by jumps rule out one uniform playback
speed difference. They are consistent with edit discontinuities; this work has
not identified the exact inserted/deleted frames or bounded every scene boundary.
Some matched caption onsets differ editorially from speech or have recognition
timestamp errors. Applying an individual anchor without regional validation would
not be safe.

## Independently measured HLS probe label errors

RMS-envelope cross-correlation matched retained emulator PCM to the complete MP4
reference, independently of subtitle text. Each comparison used a 15-second audio
window after skipping the first five seconds of the sample. Correlations ranged
from 0.92 to 0.94.

| PCM recording | Labelled episode start | Acoustically matched MP4 start | Label minus reference | Correlation |
| --- | ---: | ---: | ---: | ---: |
| Prior `first-probe-snapshot.wav` | 1,154,976 ms | 1,153,979 ms | +997 ms | 0.936 |
| Prior `30min-retained.wav` | 1,714,442 ms | 1,709,999 ms | +4,443 ms | 0.944 |
| Fresh `subtitle-audio-5.wav` | 2,366,820 ms | 2,366,518 ms | +302 ms | 0.922 |
| Fresh `subtitle-audio-6.wav` | 2,457,314 ms | 2,456,799 ms | +515 ms | 0.924 |

The prior opening snapshot matches approximately -21 ms in the reference, with
correlation 0.933. This also corroborates the sampled release content.

For fresh sample 5, the app logged requested base 2,366,539 ms plus probe-local
start 281 ms. The recording actually starts within 21 ms of that requested base.
For sample 6, requested base 2,456,820 ms plus probe-local start 494 ms similarly
labels audio that starts within 21 ms of the base. Thus adding the independently
rebased probe's local clock to its requested episode base introduces an error.

A separate earlier probe logged local start 4,227 ms at requested base
1,325,441 ms and reported a +7,881 ms dialogue correction. Its PCM was not retained,
so that particular correction has not been independently validated.

**Remaining technical blocker:** establish and test the mapping from decoded probe
PCM into the *active player's* subtitle clock, including mux preroll, independent
PTS epochs and HLS restarts. Simply using a requested base, subtracting unrelated
transport origins, or applying the measured MP4 offsets to the current renderer
has not been proved safe. Region fitting and source/session cache protection must
then operate on anchors in that verified common clock.

## Retained evidence

Ignored local evidence is under `build/host-tests/s2e9-finish-20261114/`:

- `reference.mp4`, `reference-full-transcript.json`, `reference-anchors.json`;
- `current.srt`, source metadata, reference-download and range-resume scripts;
- independent transcription and phrase-matching scripts;
- prior/fresh PCM acoustic comparison scripts and JSON measurements;
- fresh `subtitle-audio-5.wav` and `subtitle-audio-6.wav`;
- sanitized playback/probe logs and visually inspected emulator screenshots.

The earlier partial transcription remains named `reference-transcript.json` and
must not be mistaken for `reference-full-transcript.json`.

No sub-500 ms corrected residuals are claimed for any acceptance region. Seeking,
subtitle off/on, reopening, correction-cache behavior and wrong-release protection
have not passed the required end-to-end acceptance test in this investigation.

## Checks and cleanup

The existing focused host suites passed: dialogue synchronization, automatic
synchronization, independent-HLS executor, correction cache, and player tracks.
These are baseline safeguard checks, not evidence that this episode is fixed.
No new Android build, full quality suite, commit, push or CI run was performed.

The app was force-stopped and `emulator-5580` was shut down with `adb emu kill`.
Unrelated worktree changes were preserved. This document records that initial
investigation; the linked acceptance report contains the subsequent implementation.

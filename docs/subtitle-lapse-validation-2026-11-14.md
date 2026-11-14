# Scene-aware subtitle alignment with LAPSE

## Acceptance status

The five required regions played with automatic scene correction on the Glass
headless emulator. Decoded audio, natural caption onset logs, playback videos and
visually inspected screenshots are retained. Independent small.en measurements
are below 500 ms in all five regions. Base-model timestamp outliers and their audio reviews are reported below. No manual subtitle delay was used.

## Engine selection

The exact S2E9 release and its 791-cue external English track were compared using
[alass](https://github.com/kaegi/alass),
[ffsubsync](https://github.com/smacke/ffsubsync), and
[LAPSE](https://github.com/Schwponaco-org/lapse).
LAPSE 2.3.2 with Silero produced the strongest result on this release. Its native
alignment core is vendored at commit
`b9daedc9083770b5930f42b45371fd357d7c0021`; source and licensing details are in
`app/src/main/cpp/third_party/lapse/UPSTREAM.md`.

The retained independent faster-whisper reference supplied no subtitle prompt.
In the five required offline regions, 67 of 67 reliably matched phrases had an
absolute onset residual below 500 ms with LAPSE and Silero. Across the complete
episode, 257 of 268 matched phrases met that threshold; the largest residual was
1,289 ms. These are offline file measurements, not emulator renderer measurements.
The comparison scripts, outputs and source recordings are retained under
`build/host-tests/s2e9-engine-comparison-20261114/`.

## Cause and clock conversion

The source has several nearly constant offset plateaus, increasing from roughly
zero at the opening to 9.8 seconds near the ending. One global delay or playback
speed change cannot correct them. This is consistent with different edits; exact
inserted or deleted frames have not been identified.

Independent HLS probes also introduced clock errors when their local playback
position was added to the requested seek position. The corrected conversion uses
probe-local position plus its demux origin, translated by the fixed main-player
base and demux origin. Independent PCM cross-correlation corroborates that clock.
An unrelated timestamp epoch is rejected, and a 90-second speech retry receives
its own correctly rebased context.

## Runtime behavior

LAPSE proposes bounded scene mappings from a complete, center-channel Silero
speech profile of the selected physical release and audio track. Normalization
uses three-minute chunks as upstream does. A static source request avoids the
short rolling window of a separately negotiated HLS stream.

Each proposed region then requires fresh, independently recognized dialogue.
Long regions need two separated successful windows. Each window needs at least
four distinct phrase anchors spanning 20 seconds, with at least 80 percent within
500 ms of the proposed mapping. Failed windows may be retried, with four attempts
per region and a ten-minute total scene-analysis deadline. Ambiguous repeated
phrases, conflicting anchors and insufficient evidence do not authorize a map.

The renderer maps time only inside verified regions. Original subtitle files
remain unchanged. Gaps use the original timing, and corrected boundary cues are
withheld outside their mapped interval to prevent an early flash or repeated cue.

Private cached source evidence includes account, item, physical source, selected
audio and subtitle tracks, and cue identity. Provider advertising text is excluded
from the source-evidence text hash because it changes independently of dialogue;
all timestamps, cue order and spoken text remain part of the identity. Applying a
cached map in a new playback session requires fresh recognized dialogue. The
applied correction also checks the current playback generation and HLS context.

Seeking negotiates the same release again when HLS requires it. Paused seeks
preserve pause intent explicitly. Premature HLS EOF has a bounded same-episode
recovery so a short transport stream does not automatically advance the show.

## Evidence and remaining checks

The final emulator run uses debug versionCode 100730 on
`remux_subtitles_pixel5_api35`, serial `emulator-5580`, on Glass. Evidence is retained
under `build/host-tests/s2e9-scene730-20261114/`. The cold six-region analysis and
independent full-profile clock checks are retained in
`build/host-tests/s2e9-scene726-20261114/`.

A library-duration bug found during acceptance advanced to S2E10 at 43:00,
although this release runs 44:26.539. Playback completion now uses the selected
source duration. The repeated ending capture stayed in S2E9 past 43 minutes and
reached the final dialogue.

All five regions have completed captures with four screenshots each, playback
video, natural renderer onset logs and decoded PCM samples. One screenshot in
each final-build region was visually inspected. No manual delay was set: the
application logs identify `scene-dialogue-revalidated`, six verified regions,
and a zero global offset. The backward/forward seeks, subtitle off/on and exit/reopen checks also completed.
The real Google TV Streamer has not been used.


## Measured renderer residuals

Renderer onsets are converted to physical source time using local playback PTS
plus the active demux origin. Independent PCM cross-correlation checks that
conversion. The full decoded profile has a constant 47 ms label error at seven
positions; the initial live-HLS checks have errors of 43–86 ms and correlations
of 0.925–0.995. Requested seek positions are not treated as decoded audio onsets.
The final
11 live-HLS samples have a maximum clock error of 86 ms; correlations range
from 0.770 in the quiet forest scene to 0.991.

Both reference recognizers ran independently of the application, without subtitle
prompts. Unique phrase matching requires similarity at least 0.95. The following
tables retain matches whose first recognized word has probability at least 0.8.
That probability measures recognition confidence, not guaranteed timestamp
accuracy. Raw matches and all excluded lower-confidence measurements remain in
the evidence.

| Final build region | base.en matches | Median residual | Maximum absolute residual |
| --- | ---: | ---: | ---: |
| Opening | 3 | +213 ms | 361 ms |
| Before 19:14 | 12 | +114 ms | 340 ms |
| 19:14–20:30 | 11 | +293 ms | 548 ms |
| 28:30–30:30 | 21 | +63 ms | 301 ms |
| Ending, including after 43:00 | 18 | +202.5 ms | 503 ms |

A separate small.en transcription of the retained decoded audio gives:

| Region | small.en matches | Median residual | Maximum absolute residual |
| --- | ---: | ---: | ---: |
| Opening | 1 | +5 ms | 5 ms |
| Before 19:14 | 12 | -15.5 ms | 180 ms |
| 19:14–20:30 | 16 | +69.5 ms | 329 ms |
| 28:30–30:30 | 22 | +57 ms | 248 ms |
| Ending | 18 | +85 ms | 397 ms |

The 548 ms base.en result is an inaccurate reference onset for “The Hollow Man
is motivated by external pressures.” The base model attaches the first word to
the preceding utterance at source 19:57.440. The independent small model places
it at 19:57.860. The PCM has a quiet interval from roughly 19:57.680 to 19:57.880,
then a new speech onset; the caption renders at 19:57.988, about 108 ms later.
`critical19/outlier-review.json` retains the 20 ms RMS measurements and both ASR
estimates. The raw 548 ms result has not been overwritten.

For the 503 ms ending estimate, the models differ by 140 ms: small.en measures
363 ms for the same rendered cue. This is close enough to the threshold that
base.en alone does not prove a sub-500 ms onset for that cue. The separate
transcription and retained audio support the corrected timing. These results do
not claim that every caption in the episode has frame-accurate editorial timing.

The first full source analysis took about 7 minutes 28 seconds on the emulator,
including global rejection, the full speech profile and independent verification
of all six regions. A fresh opening-session revalidation took about 22 seconds.
Typical 30-second recognitions took 3–7 seconds on these runs. Playback remained
responsive; recordings begin only after the automatic correction applies.


## Interaction checks, regression tests and cleanup

- Seeking backward from the ending to 19 minutes renegotiated the same release
  and revalidated its cached map. Six phrases measured against the independent
  small.en reference have a maximum absolute residual of 412 ms. The base-model
  raw estimates include 578 ms and 512 ms; the independent model measures 18 ms
  and 412 ms respectively for those same rendered cues.
- Seeking forward past 30 minutes produced nine high-confidence base.en matches,
  with a maximum absolute residual of 309 ms. The settle check compares decoded
  source PTS with the requested position, retaining an eight-second bound. HLS
  preroll can make the requested-base progress clock differ from source time.
- Exiting and reopening S2E9 resumed the same physical release. Fresh dialogue
  revalidated all six persisted regions automatically. Turning subtitles off
  showed Off selected; turning the same external English track back on logged
  `Subtitle auto-sync reused` immediately, without another inference pass.
- The subsequent off/on playback sample has three high-confidence base.en matches,
  all within 461 ms. Screenshots show the same English captions after reopening.

The compact [measurement artifact](data/subtitle-s2e9-residuals-2026-11-14.json)
contains both recognizers' onset rows, audio hashes, clock checks and interaction
measurements. The full local evidence includes every lower-confidence match,
recordings, transcripts and sanitized logs. The original SRT is unchanged.

The full host suite, 79 Python tests, Ruby tests, screenshot metadata validation,
mpv AAR validation, Gradle tests and debug builds passed. Final C++ formatting,
clang-tidy and cppcheck passed after the duration fix. Focused playback duration,
continuation, transition and coordinator regressions also passed. Native LAPSE
regressions cover multiple inserted/deleted scenes, silence and cancellation;
existing affine timing tests retain fixed-offset and gradual-drift coverage.
Source, account, audio, subtitle, cue and session invalidation tests prevent cached
maps from crossing releases or tracks. Incompatible-release protection was tested
on the host; an alternate release was not substituted during emulator acceptance.

Known S2E9 playback state was restored to its original 20:17 position, unwatched
and not a favorite. S2E10 was restored to the accepted 12:12 saved position,
unwatched and not a favorite. Both updates were read back and verified. Earlier
unattended investigation playback had also advanced through S2E11–S2E17; their
original states were not recorded, so no guessed state resets were performed.

The app was force-stopped, the test watchdog and recording monitor were stopped,
and emulator-5580 was shut down. The real Google TV Streamer was never contacted.
Unrelated worktree changes remain outside this commit.

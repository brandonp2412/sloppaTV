# Subtitle synchronization validation on Glass

Testing used Glass Waydroid at `192.168.240.112:5555` and the isolated package `app.sloppatv.remuxtest`. The Google TV Streamer was not inspected or controlled. The previous subtitle commit, `e20ac671`, passed [CI run 43193440595](https://github.com/brandonp2412/sloppaTV/actions/runs/43193440595).

## Release identity

Item IDs, source IDs and ETags are insufficient to identify these Remux releases. The Friends primary alias described four AC3 audio tracks, while direct reads returned a file with one English AAC track and embedded ASS. Requests under three source IDs returned the same first-megabyte SHA-256 and total size. Neither an ETag nor a filename header was supplied.

The completely downloaded Friends AAC/ASS file is 302,561,047 bytes, duration 1,336.341 seconds, SHA-256 `eb2f20a0db3bed549bf8bbe63239c457991bebdd734c3b88cdbd37e325d9e90f`. An initial download stopped early; it was completed with byte ranges, checking the overlap at the resume boundary. The Psychodrama file is 367,178,935 bytes, duration 2,546.711 seconds, SHA-256 `a46a6531a24035070c1c356cfc15d910130af1459e64003dd9cb7d5554b8fa91`.

Playback negotiation now carries a fingerprint of the source path, runtime, container and physical stream layout. External subtitle delivery URLs and alias IDs/ETags are excluded. HLS probes must negotiate matching metadata on a separate server device/session. Subtitle disk caches also require a playback session identity. This fingerprint cannot detect a server replacing content while keeping every fingerprinted field unchanged; it is not a cryptographic content hash.

## Dialogue anchors

Positive differences mean the spoken onset follows the subtitle cue. Whisper word timestamps are estimates; these medians describe matched dialogue, not frame-exact onset measurements.

| Track | Matched phrases | Early median | Middle median | Late median |
| --- | ---: | ---: | ---: | ---: |
| Psychodrama embedded ASS 2 | 45 | +420 ms | +305 ms | +310 ms |
| Psychodrama embedded ASS 3 | 43 | +415 ms | +320 ms | +320 ms |
| Psychodrama provider SRT 4 | 40 | +5,685 ms | +3,183 ms | +1,222 ms |
| Friends embedded English SRT 6, multi-audio release | 62 | −188 ms | +107 ms | −5 ms |
| Friends provider SRT 7 | 44 | +1,838 ms | +2,127 ms | −17,062 ms |

Psychodrama windows cover 0–180, 1,100–1,340 and 2,260–2,500 seconds. The middle ASS windows contain only two or three sufficiently long phrase matches. Friends windows cover 0–240, 540–780 and 1,050–1,290 seconds. The AAC release's embedded ASS also agrees with its audio across those windows.

The Friends provider SRT has an edit discontinuity: 94 matching dialogue anchors against embedded ASS show about +2 seconds through much of the first half and roughly −17 seconds after about 15 minutes. A fixed offset or known frame-rate ratio cannot repair it globally. The embedded English SRT has 373 cues; the provider track has 338.

For Psychodrama, the earlier retained recordings produced 101 provider-SRT audio anchors with a robust offset slope near −0.001014. That is compatible with 24/23.976 drift. Matching its dialogue directly against ASS still leaves changing residual offsets after applying `1000/1001`. Small drift does not explain all the differences, and the precise edit history remains unknown. PAL drift is much larger than the observed trend.

## HLS clocks and server isolation

A background probe must not open the active player's HLS session. Remux can interpret a master-playlist request with a nonzero start time as a seek and restart that transcode. Probe negotiation therefore uses a separate device ID and PlaySessionId, verifies the source fingerprint and audio index, and stops only that probe encoding afterward. A server response which echoes the active PlaySessionId is rejected without stopping it.

mpv normally rebases a stream's start timestamp to zero; see its [playback control documentation](https://mpv.io/manual/stable/#playback-control). Raw transport PTS and the requested restart base do not by themselves establish the main player's clock. The probe now measures its demuxer origin and converts its observed start into the active player's coordinate system. Applying the result requires the same playback URL, timeline base, track revision and demuxer origin. Playback-generation changes and audio/subtitle selection or off/on changes cancel capture work. Distant HLS verification negotiates a fresh server session at each requested window, rather than seeking far ahead through an unfinished playlist. All of those sessions must match the active source fingerprint and audio index, and all are cleaned up after verification.

Independent acoustic checks used ten-second audio envelopes against the completely downloaded AAC file:

| Requested HLS base | Probe seek | Matched source onset | Envelope correlation |
| ---: | ---: | ---: | ---: |
| 0 ms | 0 ms | 64 ms | 0.9996 |
| 0 ms | 30,000 ms | 34,219 ms | 0.9309 |
| 385,000 ms | 0 ms | 380,139 ms | 0.9591 |
| 385,000 ms | 30,000 ms | 410,032 ms | 0.9195 |
| 1,110,000 ms | 0 ms | 1,109,781 ms | 0.9450 |
| 1,110,000 ms | 30,000 ms | 1,139,675 ms | 0.9674 |

These results demonstrate why offsets from different HLS restart bases must not be compared as source-media offsets. In a repeated test with decoder logs disabled, the independent clock reported 34,199 ms for audio matching 34,219 ms. Two initial missing-log runs failed to establish a dependable timestamp and were rejected. The consistency thresholds were retained. Subsequent traces found occasional stale playback-clock values paired with newer PCM, even immediately after pausing the probe. Pausing only the independent probe and allowing 20 ms for its clock to refresh removed those outliers in the repeated Android tests. With decoder logs disabled, the three restart bases reported 34,199, 30,000 and 30,000 ms respectively. The retained audio independently matched source onsets 34,219, 410,032 and 1,139,675 ms. Each diagnostic session was explicitly stopped after its test.

## Regression coverage and visual checks

The host suite contains numeric speech masks and cue timings from the explicitly named historical Friends snapshots. The 385,000 ms snapshot must accept +2,250 ms at scale 1.0; the 547,579 ms snapshot must reject its conflicting peaks. Their original source identities were not recorded, so the tests do not compare those captures across releases.

Synthetic tests cover positive and negative offsets, PAL ratios, both 24/23.976 directions, edit jumps, wrong speech masks, metadata advertisements, repeated windows, missing observations, clock discontinuities, integer bounds and independent HLS-session cleanup. Global speed selection requires three separated windows, each passing the existing VAD confidence checks, with offsets agreeing within 500 ms. Correlation uses packed masks while preserving the original frame counts and phi calculation.

On isolated versionCode 25, both Psychodrama ASS tracks rendered the expected dialogue at 40.9 seconds. The second track included its speaker label. Switching tracks and off/on preserved paused position 40,900 ms, player-load count 2 and resource-release count 5. ASS styling continued through libmpv. Waydroid video rendering remains unreliable; captions, decoded-frame telemetry and retained audio supplied the acceptance evidence.

On versionCode 31, Friends English SRT off/on preserved paused position 492,357 ms and player-load count 3. Switching to provider stream 7 loaded 338 cues; switching off during its background work removed the probe WAV and prevented a stale correction. The 373-cue embedded track remains the release-associated choice established by the dialogue anchors.

The real app completed three independent HLS windows at restart bases 385,000, 1,036,384 and 710,692 ms. Their converted logical sample starts were 390,018, 1,041,402 and 715,731 ms. The earlier versionCode 30 captures independently matched source onsets 385,105, 1,080,105 and 732,605 ms; their player-to-source differences agreed within 21 ms. The global VAD decision still rejected this release-associated SRT: distant windows failed confidence/stability requirements. This remains a limitation, and the application did not apply the locally plausible correction globally. Negotiated release runtime now bounds samples instead of the padded HLS duration.

Final local checks passed the complete host suite (including 79 Python tests), Android x86_64 compilation, clang-tidy over 45 translation units and cppcheck. The committed fixtures contain numeric masks and timings only.

Evidence and reproduction scripts are on Glass under `build/host-tests/subtitle-finish-20261113/`. Probe snapshot names include the app PID and are paired with logs; counter-based WAV names alone do not establish their timeline. Full media, transcripts, screenshots, authentication data and native test runtimes are not committed.

## Limits

A rejected provider track can still be visibly mistimed until the user chooses a release-associated track. The application does not synthesize a piecewise repair for a different edit. Distant verification can reject a locally plausible match when another window has ambiguous speech or lacks a dependable timestamp. Short remaining HLS sessions retain the existing local guards and cannot establish 24/23.976 drift across the whole episode.

# Criminal Minds S2E9 — emulator subtitle-sync proof, 13 November 2026

## Outcome

**Not fixed end-to-end.** The emulator reproduces a changing error in the only available external English SubRip subtitle stream (791 cues). It is unsafe to apply one global fixed offset. No TV streamer commands or installations were performed.

## Device and build

- Glass headless Android 15 Pixel 5 emulator, AVD `remux_subtitles_pixel5_api35`, ADB serial `emulator-5580`.
- App `app.sloppatv`, diagnostic build versionCode `100708` (earlier captures on 100706/100707).
- Episode: Criminal Minds S2E9, The Last Word; subtitle stream 2, `SUBRIP - External`.
- Audio was captured from the app's *independent* libmpv PCM probe. An independent local `faster-whisper-base.en` transcription used no subtitle text as a prompt. Word onsets were matched against cues with similarity filtering; each offset is measured audio-onset minus caption-onset. WAV and transcript files remain in ignored local evidence under `build/host-tests/s2e9-proof-20261113/`.

## Measured errors

| Playback area | Matched caption phrases | Result |
| --- | ---: | --- |
| 19:14–20:10 | 19 | Median +4,520 ms; minimum +4,311 ms; maximum +4,901 ms |
| ~28:30–29:40 | 16 | About +10,000 ms early in the first part, increasing to ~+12,000 ms later within the sample |

The first sample contains exact phrase matches (similarity 1.0). Independently, the app's optimized on-emulator `whisper.cpp` recognizer matched **six** dialogue anchors near 19:14 with a local +4,580 ms correction; log entry at 22:43:08.994. These independently agree within 60 ms.

### Runtime findings

- The old 30-second Android recognition pass timed out with status `-9` at 90 seconds. Switching the same model to greedy decoding produced 113 segments, status `0`, in 50,292 ms for 30 seconds and six matched anchors. No transcript strings were logged.
- One failed probe had **33 consistent clock measurements around 4.4 seconds and one stale 328 ms reading**. The narrow, test-covered singleton-outlier tolerance prevents rejection of the otherwise coherent clock while continuing to reject genuine multi-observation discontinuities.
- A separately restarted Remux HLS probe can have an unrelated transport PTS epoch (for example, 2,366,281 ms versus the main playback's 1,153,069 ms). Absolute probe offsets must use their **requested episode start**, not subtraction of incomparable demuxer PTS origins. The implementation now retargets verification windows to that known base.
- Global correction remains rejected when distant dialogue evidence conflicts or is insufficient. This is desirable: the source is not demonstrably aligned by one offset or speed ratio.

## Verification and remaining requirement

The full host suite (including 79 Python tests), the focused subtitle C++ tests, and the Android x86_64/ARM32/ARM64 debug build pass. The actual episode and subtitles were visually inspected in the emulator at 19:14 and around 30 minutes.

**Still needed:** scene/edit-aware subtitle retiming, or a subtitle release matching this exact video edit. Positive completion requires a corrected overlay and independent spoken-to-caption onset residuals below 500 ms at the beginning, after 19:14, and past 30 minutes. Rendering a subtitle at those timestamps alone is not proof of synchronization.

## Follow-up: HLS speech-retry start position

The emulator exposed another clock rebasing error: when a short opening speech probe restarted its independent HLS transcoder 90 seconds later, the probe kept the previous logical sample base. Fixed the retry to carry the new episode base and rebase subsequent verification URL callbacks. A focused regression verifies 90-second rebasing and callback bounds. The updated x86_64/ARM32/ARM64 Android debug APK (versionCode `100709`) built and was installed on the headless emulator; the same episode and external English subtitles were visually inspected. The corrected timing of the episode has **not** passed end-to-end playback verification.

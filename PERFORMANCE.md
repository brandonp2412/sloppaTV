# sloppaTV performance evidence

Target device: the same Android TV test target used for both sloppaTV and the installed official `org.jellyfin.androidtv` client. Measurements must always compare like-for-like app states on the same hardware.

## Rules for final claims

1. Use an installable non-debuggable sloppaTV release candidate, not a debug build.
2. Alternate or repeat cold launches after `am force-stop`; report raw samples, median and mean.
3. Measure settled Home memory only after both clients have populated their Home UI.
4. Compare navigation frame cadence on the same Home-row left/right workload and report median, p95 and >20 ms rate.
5. Keep idle CPU effectively zero on static screens.
6. Compare playback only when both clients use the same source/direct-play/transcode condition.
7. Preserve raw samples and the exact benchmark command/tool version used for the final report.

`tools/benchmark_tv.py` is the canonical benchmark harness. It records startup, settled memory, idle CPU, SurfaceFlinger navigation cadence and percentage deltas, and can emit JSON evidence.

## Current release-candidate evidence — 2026-09-01

The measurements below predate the build-type split and used the then-current Android `release` build type (`isDebuggable=false`, native C++ optimized) signed locally with the standard Android debug key solely for installation on the test TV. Equivalent installable performance/device testing now uses the non-debuggable `benchmark` build type; `release` remains unsigned unless production signing credentials are explicitly supplied.

### Cold Activity-manager launch

Five process-cold launches after `am force-stop`:

- sloppaTV WaitTime samples: **260, 236, 249, 240, 236 ms**
- official Jellyfin TV WaitTime samples: **3022, 3014, 3015, 3016, 3017 ms**
- sloppaTV median: **240 ms**
- official Jellyfin TV median: **3016 ms**
- current delta: sloppaTV is about **92.0% lower latency / 12.6x faster** on this Activity-manager launch metric.

This metric includes each application's own startup flow. It does not by itself prove equivalent Home content is ready; sloppaTV separately logs its first useful Home-row readiness.

### sloppaTV Home readiness

On the real Jellyfin server after the staged Home-loader change:

- first useful rows (Continue Watching + Next Up): **351–398 ms** in observed cold process runs
- secondary Home enrichment (Recently Added, Recommended, Favorites): roughly **0.94–1.1 s**

The primary rows are published immediately and remain interactive while secondary rows append.

### Settled Home memory — release candidate

After a 6-second Home settle on each app:

| Metric | sloppaTV | Official Jellyfin TV | sloppaTV reduction |
| --- | ---: | ---: | ---: |
| Total PSS | **10,648 KB** | 31,532 KB | **66.2%** |
| Total RSS | **81,664 KB** | 109,388 KB | **25.3%** |
| Java Heap | **1,532 KB** | 7,412 KB | **79.3%** |
| Native Heap | **2,584 KB** | 5,240 KB | **50.7%** |

Memory measurements can vary with image-cache residency and Android shared pages; final evidence will repeat them over several runs rather than rely on one snapshot.

### Post-playback-feature release checkpoint — 2026-09-01

After adding direct dual-audio switching, native SRT/GLES subtitles, media-segment skipping, and HDR/profile/resolution capability gating, a fresh five-run release checkpoint still showed a large lead:

- sloppaTV cold-launch samples: **290, 697, 444, 729, 344 ms**; median **444 ms**, mean **500.8 ms**
- Jellyfin TV cold-launch samples: median **1920 ms**, mean **1989.6 ms**
- settled PSS: **52,397 KB** vs **143,721 KB** (**63.5% lower**)
- settled RSS: **114,068 KB** vs **215,608 KB** (**47.1% lower**)
- Java heap: **2,812 KB** vs **39,828 KB** (**92.9% lower**)
- rapid-DPAD p95: **16.72 ms** vs **66.66 ms**
- rapid-DPAD intervals over 20 ms: **0.8%** vs **21.4%**
- sampled idle CPU: **0.0%** for both clients

Raw machine-readable evidence is stored in `artifacts/perf-checkpoint-2026-09-01.json`. This remains a checkpoint rather than the final claim because the final gate requires the larger sample counts and frozen feature set below.

### Navigation frame cadence

The latest couch-readability redesign was rechecked on the physical Google TV Streamer using the non-debuggable, debug-signed Benchmark build after moving Home to three large cards and media grids to four larger columns:

- redesigned sloppaTV Benchmark: **16.67 ms median / 16.71 ms p95**, **0.8% >20 ms** over an 80-event rapid-DPAD SurfaceFlinger sample
- prior sloppaTV checkpoint: **16.66 ms median / 16.75 ms p95**, **0.8% >20 ms**
- official Jellyfin TV historical baseline: **16.67 ms median / 50.02 ms p95**, **17.5% >20 ms**

The larger cards/text therefore did not regress the sampled navigation tail latency. A final multi-run signed-release SurfaceFlinger suite is still required after the scoped feature set is frozen; this single Benchmark run is a regression checkpoint, not the final release claim.

### Idle behavior

sloppaTV static screens are event-driven. The last sampled populated Home measured **0.0% process CPU** while idle; playback remains vsync-driven. This prevents the 60 Hz navigation optimization from turning into continuous background rendering.

## Final evidence gate

Before declaring the project complete, run at least:

- 20 alternating cold launches per app.
- 5 settled Home-memory samples per app.
- 5 rapid-DPAD SurfaceFlinger runs per app.
- equal-source playback startup comparisons for at least H.264 and HEVC Main10 direct play.
- a 30-minute Home/navigation soak and a 60-minute playback soak while tracking PSS and crashes.
- the same suite against the final signed release APK.

The final report must include raw samples and must state any metric where sloppaTV does not beat Jellyfin TV rather than hiding it.

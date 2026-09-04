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

`tools/benchmark_tv.py` is the canonical benchmark harness. It records startup, settled memory, idle CPU, SurfaceFlinger navigation cadence and percentage deltas, and can emit JSON evidence. Pass `--final-suite` to enforce the final-gate 20 startup / 5 memory / 5 navigation sample counts; missing cold-launch samples fail explicitly instead of silently weakening the result.

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

A 2026-09-02 real-server payload audit then removed list/grid over-fetching of `Overview`, `PrimaryImageAspectRatio` and full `MediaSources`; full Details/playback data remains fetched on demand. With the same current account/server, representative JSON payloads changed as follows: Continue Watching 50.7 KB → 9.3 KB (**81.7% smaller**), 24-item Latest Movies 210.0 KB → 26.5 KB (**87.4% smaller**), 60-movie browse 394.0 KB → 65.9 KB (**83.3% smaller**), Friends search 229.8 KB → 70.1 KB (**69.5% smaller**), and the 97-record Friends Play All episode list 884.4 KB → 144.9 KB (**83.6% smaller**). The lighter responses retained the card/user/artwork/episode-index metadata consumed by those screens. Final device timing must still be refreshed because transport-payload measurements are not a substitute for the final cold-launch/navigation suite.

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

### UI centering and rounded-edge regression check — 2026-09-04

The card/button/text-field centering pass and rounded-edge antialiasing were checked against the exact clean `e53a913` tree on the same Google TV Streamer using the optimized Benchmark build and the canonical 20 startup / 5 settled-memory / 5 rapid-DPAD workload before and after the change.

- startup median: **252.5 ms → 246.0 ms**; mean: **249.7 ms → 249.1 ms**
- navigation: **16.67 ms median / 16.73 ms p95 / 0.8% >20 ms** both before and after
- sampled idle CPU: **0.0%** for all five sloppaTV samples before and after
- settled-memory medians: PSS **44,154 → 44,576 KB** (+0.96%), RSS **113,079 → 113,571 KB** (+0.44%), Java heap **4,036 → 4,052 KB** (+0.40%), native heap **9,040 → 9,140 KB** (+1.11%)

The memory sample ranges overlap strongly; exact 5-vs-5 permutation checks on the mean deltas were not distinguishable at this sample size (all p >= 0.119). The upward memory medians are therefore retained as measurement noise/inconclusive rather than hidden or claimed as an improvement. The hot-path result is no measurable latency, frame-cadence, dropped-frame, or idle-CPU regression. Raw before/after samples are tracked in [`docs/benchmarks/ui-centering-regression-2026-09-04.json`](docs/benchmarks/ui-centering-regression-2026-09-04.json).

### Home navigation, live search and Settings regression check — 2026-09-04

The Home viewport/context-menu/live-search/Settings pass was checked against the exact previously installed production-signed APK on the same physical Google TV Streamer. Both binaries used the same signing identity and persisted account/settings state. The canonical 20 startup / 5 settled-memory / 5 rapid-DPAD suite was run on the old APK and then on the new Release.

- startup median: **244.0 ms → 252.0 ms** (+3.28%); mean: **249.2 ms → 253.0 ms** (+1.52%). The 3.8 ms mean shift was not distinguishable in a deterministic 200,000-draw two-sided permutation check (**p ≈ 0.640**).
- navigation: **16.67 ms median → 16.67 ms**, **16.72 ms p95 → 16.73 ms**, and **0.8% >20 ms → 0.8%** across five 80-event runs per build.
- sampled idle CPU remained **0.0%** for every sloppaTV sample before and after.
- the first sequential settled-memory batches moved PSS **44,327 → 44,474 KB** (+0.33%) and RSS **109,146 → 110,919 KB** (+1.62%). Because that RSS shift was larger than the other memory changes, it was not dismissed as noise without a follow-up check.
- an immediate build-interleaved 5-old / 5-new memory control then reversed the direction: median PSS **46,153 → 44,952 KB** (-2.60%) and RSS **112,707 → 111,474 KB** (-1.09%). Java heap also fell **4,096 → 4,076 KB**, while graphics memory was effectively flat at **14,411 → 14,446 KB** (+0.24%). Exact 5-vs-5 permutation p-values for the interleaved mean deltas were 0.087 PSS, 0.071 RSS, 0.635 Java heap, and 0.921 graphics.

Taken together, the repeated same-device measurements establish no consistent startup, frame-cadence, dropped-frame, idle-CPU, PSS, RSS, Java-heap, or graphics regression attributable to this pass. The initial sequential RSS increase is retained in the evidence rather than hidden; its reversal under build interleaving is why it is classified as run-order/allocator variance instead of a demonstrated regression. Raw and interleaved samples are tracked in [`docs/benchmarks/home-navigation-settings-regression-2026-09-04.json`](docs/benchmarks/home-navigation-settings-regression-2026-09-04.json).

### Production-signed final-suite checkpoint — 2026-09-04

A fresh canonical `--final-suite` run was completed on the physical Google TV Streamer using the production-signed sloppaTV `0.1.0` APK at commit `fea00884daf1b616bb24bd41b1be1a25b1b833d0`. The comparator was the currently installed `org.jellyfin.androidtv` package, which reports version `0.0.0-dev.1`. Both apps used their persisted sessions against the same Jellyfin server. Because the installed comparator is a development build, these numbers describe this exact installed Jellyfin Android TV build rather than every upstream Jellyfin release.

The exact command was:

```sh
python3 tools/benchmark_tv.py \
  --serial 192.168.1.114:5555 \
  --final-suite \
  --json-out artifacts/e2e-physical-tv/final-benchmark-2026-09-04.json
```

Twenty alternating process-cold launch samples:

- sloppaTV: **293, 265, 222, 269, 294, 239, 243, 228, 249, 233, 276, 242, 219, 247, 221, 252, 227, 229, 244, 218 ms**; median **242.5 ms**, mean **245.5 ms**, range **218–294 ms**.
- Jellyfin Android TV: **552, 383, 396, 395, 406, 441, 418, 379, 404, 237, 388, 247, 408, 413, 410, 401, 638, 441, 397, 404 ms**; median **404.0 ms**, mean **407.9 ms**, range **237–638 ms**.
- Relative launch result: sloppaTV was **40.0% lower median latency** and **39.8% lower mean latency**; equivalently, Jellyfin's median took about **1.67x** as long on this Activity-manager metric.

Five-run settled-Home memory medians after a 6-second settle:

| Metric | sloppaTV | Jellyfin Android TV | sloppaTV reduction |
| --- | ---: | ---: | ---: |
| Total PSS | **40,192 KB** | 152,328 KB | **73.6%** |
| Total RSS | **112,878 KB** | 233,341 KB | **51.6%** |
| Java Heap | **4,036 KB** | 41,888 KB | **90.4%** |
| Native Heap | **8,980 KB** | 18,172 KB | **50.6%** |

Five 80-event rapid-DPAD runs per client:

| Metric | sloppaTV | Jellyfin Android TV | Result |
| --- | ---: | ---: | ---: |
| Median frame interval | 16.67 ms | 16.67 ms | tie |
| p95 frame interval | **16.73 ms** | 33.34 ms | **49.8% lower** |
| Intervals over 20 ms | **0.8%** | 10.3% | **9.5 percentage points lower** |
| Intervals over 33.4 ms | **0.8%** | 3.2% | **2.4 percentage points lower** |
| Sampled idle CPU | 0.0% | 0.0% | tie |

The five sloppaTV p95 samples were **16.71, 16.74, 16.73, 16.71, 16.73 ms**. The five Jellyfin p95 samples were **33.34, 33.34, 33.34, 33.34, 33.36 ms**. sloppaTV did not beat Jellyfin on median frame interval or sampled idle CPU; both tied on those measurements.

Machine-readable raw evidence is tracked in [`docs/benchmarks/google-tv-streamer-2026-09-04.json`](docs/benchmarks/google-tv-streamer-2026-09-04.json). Immediately before this benchmark, a 15-minute static physical-TV soak collected 31 samples and ended at **-0.1% PSS growth** and effectively flat RSS with no sloppaTV fatal/ANR audit finding; this is useful resilience evidence but does not replace the 30-minute Home/navigation and 60-minute playback soaks required by the final gate.

### Final-sample-count Benchmark checkpoint — 2026-09-03

The canonical `--final-suite` sample counts were run on the physical Google TV Streamer using the currently installed non-debuggable sloppaTV Benchmark package and the installed Jellyfin Android TV client. This satisfies the sample-count portion of the final gate, but it is still a checkpoint rather than the release claim because the installed benchmark predates the newest tiny UI/manifest/network-error commits and is not production-signed.

- 20 alternating process-cold sloppaTV launches: **218, 229, 209, 260, 228, 270, 207, 237, 219, 201, 210, 218, 293, 228, 211, 225, 216, 240, 337, 220 ms**; median **222.5 ms**, mean **233.8 ms**.
- 20 alternating Jellyfin launches: **551, 445, 403, 470, 397, 394, 405, 266, 410, 422, 404, 420, 415, 278, 417, 418, 413, 468, 415, 391 ms**; median **414.0 ms**, mean **410.1 ms**.
- Five-run settled-memory medians: sloppaTV **37,562 KB PSS / 114,766 KB RSS / 3,888 KB Java heap / 8,268 KB native heap**; Jellyfin **150,356 / 237,204 / 41,612 / 18,220 KB** respectively.
- Five-run rapid-DPAD aggregate: sloppaTV **16.67 ms median / 16.72 ms p95 / 0.8% >20 ms / 0.8% >33.4 ms**; Jellyfin **16.67 / 33.35 ms / 10.3% / 2.4%**.
- Sampled idle CPU remained **0.0%** for both applications.
- Relative result: sloppaTV was **46.3% lower startup median**, **75.0% lower PSS**, **51.6% lower RSS**, **90.7% lower Java heap**, and **49.9% lower navigation p95** in this run.

Machine-readable raw evidence is kept locally as `artifacts/e2e-physical-tv/final-benchmark-2026-09-03.json`; physical evidence directories are intentionally gitignored. This checkpoint was superseded by the production-signed 2026-09-04 startup/memory/navigation suite above; equal-source playback comparison and the long Home/playback soaks remain.

### Idle behavior

sloppaTV static screens are event-driven. The last sampled populated Home measured **0.0% process CPU** while idle; playback remains vsync-driven. This prevents the 60 Hz navigation optimization from turning into continuous background rendering.

## Final evidence gate

Before declaring the project complete, run at least:

- 20 alternating cold launches per app. **Completed on the production-signed 2026-09-04 run.**
- 5 settled Home-memory samples per app. **Completed on the production-signed 2026-09-04 run.**
- 5 rapid-DPAD SurfaceFlinger runs per app. **Completed on the production-signed 2026-09-04 run.**
- equal-source playback startup comparisons for at least H.264 and HEVC Main10 direct play.
- a 30-minute Home/navigation soak and a 60-minute playback soak while tracking PSS and crashes.
- complete the remaining playback and soak measurements against the final signed release APK.

The final report must include raw samples and must state any metric where sloppaTV does not beat Jellyfin TV rather than hiding it.

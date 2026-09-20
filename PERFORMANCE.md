# sloppaTV performance policy

This document defines the current performance contract and how to validate it. Historical measurements belong in Git history and machine-readable benchmark artifacts, not as an accumulating timeline here.

## Canonical benchmark

tools/benchmark_tv.py is the canonical device benchmark harness. Always compare sloppaTV and the installed Jellyfin Android TV comparator on the same physical Android TV hardware, against the same server and equivalent content/playback conditions.

Use an installable, non-debuggable sloppaTV build. Do not use debug builds for release performance claims. For release-grade validation, run tools/benchmark_tv.py with --final-suite and --json-out. The final-suite flag enforces the required startup, settled-memory and navigation sample counts.

## Required release gate

Before making a release performance claim:

- Run 20 alternating process-cold launches per app after am force-stop.
- Run 5 settled Home-memory samples per app after both clients have populated Home.
- Run 5 rapid-DPAD SurfaceFlinger navigation samples per app on the same Home-row workload.
- Confirm sampled idle CPU is effectively zero on static screens.
- Compare playback startup for at least H.264 and HEVC Main10 under equivalent direct-play/transcode conditions.
- Run a 30-minute Home/navigation soak while tracking memory, crashes and ANRs.
- Run a 60-minute playback soak while tracking memory, crashes and ANRs.

Startup reports must include raw samples, median and mean. Navigation reports must include median, p95 and the percentage of intervals over 20 ms. Memory reports must include at least PSS, RSS, Java heap and native heap.

## Evidence rules

- Preserve raw machine-readable samples for any published comparison.
- Record the exact sloppaTV commit, build type, device, comparator version and benchmark command with the evidence.
- Treat small sample checkpoints as regression checks, not final comparative claims.
- Do not hide regressions, ties or metrics where sloppaTV does not beat the comparator.
- Host-side microbenchmarks may support isolated hot-path work but never substitute for physical-device validation.
- Keep historical benchmark reports out of this file; use Git history, commit/PR notes and benchmark JSON artifacts instead.

## Quality target

Performance work must not trade away correctness, deterministic rendering, input responsiveness or idle efficiency. A change that improves one metric while materially regressing another requires explicit evidence and justification before release.

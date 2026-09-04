<p align="center">
  <img src="docs/brand/sloppatv-theme.png" alt="sloppaTV low-poly caracal artwork" width="100%">
</p>

# sloppaTV

sloppaTV is an experimental Android TV client for Jellyfin focused on a fast, remote-first, Netflix-style on-demand viewing experience.

The application, navigation, Jellyfin policy/state, queueing, playback reporting, subtitle rendering and primary UI rendering are implemented in C++20 with Android NDK `NativeActivity` and GLES3. JNI is used where Android exposes no practical NDK equivalent, including HTTP, Media3/ExoPlayer construction, audio-route probing, image decoding and codec/display capability discovery. Small Java platform bridges host ExoPlayer, forward activity/MediaSession callbacks, and own the system `DreamService` Surface lifecycle; they do not own application navigation, Jellyfin playback policy, or subtitle presentation.

## Scope

Supported scope is Movies, Series and Episodes. Music, photos, Live TV/DVR, launcher channels/recommendations and server administration are intentionally excluded.

See [ROADMAP.md](ROADMAP.md) for feature status and [PERFORMANCE.md](PERFORMANCE.md) for the current benchmark evidence and measurement rules.

## Performance vs Jellyfin Android TV

Fresh physical-device measurements from 2026-09-04 use the same Google TV Streamer, the same Jellyfin server, 20 alternating process-cold launches per app, five settled-Home memory samples per app, and five 80-event rapid-DPAD SurfaceFlinger runs per app. sloppaTV was the production-signed `0.1.0` build at commit [`fea0088`](https://github.com/brandonp2412/sloppaTV/commit/fea00884daf1b616bb24bd41b1be1a25b1b833d0). The comparator was the currently installed [Jellyfin Android TV](https://github.com/jellyfin/jellyfin-androidtv) package `org.jellyfin.androidtv`, which reports version `0.0.0-dev.1`; this is therefore a comparison with that installed Jellyfin development build rather than a claim about every upstream release.

| Metric | sloppaTV | Jellyfin Android TV | Result |
| --- | ---: | ---: | ---: |
| Cold launch median | **242.5 ms** | 404.0 ms | **40.0% lower** |
| Cold launch mean | **245.5 ms** | 407.9 ms | **39.8% lower** |
| Settled total PSS | **40,192 KB** | 152,328 KB | **73.6% lower** |
| Settled total RSS | **112,878 KB** | 233,341 KB | **51.6% lower** |
| Java heap | **4,036 KB** | 41,888 KB | **90.4% lower** |
| Native heap | **8,980 KB** | 18,172 KB | **50.6% lower** |
| DPAD frame interval median | 16.67 ms | 16.67 ms | tie |
| DPAD frame interval p95 | **16.73 ms** | 33.34 ms | **49.8% lower** |
| DPAD intervals over 20 ms | **0.8%** | 10.3% | **9.5 pp lower** |
| Sampled idle CPU | 0.0% | 0.0% | tie |

On these measurements Jellyfin's median cold launch took about **1.67x as long** as sloppaTV's; sloppaTV used roughly **one quarter of Jellyfin's PSS** and kept the sampled navigation p95 near one 60 Hz frame rather than two. Median navigation cadence and sampled idle CPU were ties, not wins.

The exact command, methodology, previous checkpoints and limitations are documented in [PERFORMANCE.md](PERFORMANCE.md). The machine-readable raw samples for this run are tracked at [`docs/benchmarks/google-tv-streamer-2026-09-04.json`](docs/benchmarks/google-tv-streamer-2026-09-04.json).

## Requirements

- Android SDK 36
- Android NDK `29.0.14206865`
- CMake `3.22.1`
- JDK 21
- Python 3 and a C++20 host compiler for the host policy tests

Create an ignored `local.properties` pointing at your Android SDK when `ANDROID_HOME` is not already configured:

```properties
sdk.dir=/path/to/android-sdk
```

## Build and test

Run the host-side native policy tests and renderer checks:

```sh
./tools/run_host_tests.py
```

When a gitignored `.env.local` contains `JELLYFIN_LOCAL_SERVER`, `JELLYFIN_LOCAL_USERNAME` and `JELLYFIN_LOCAL_PASSWORD`, read-only real-server API acceptance can also be run with:

```sh
./tools/jellyfin_server_e2e.py
```

Build the Android variants:

```sh
./gradlew test assembleDebug assembleBenchmark assembleRelease
```

`debug`, `benchmark` and `release` all use the single production application ID `app.sloppatv`; do not create parallel `.test` installs. `benchmark` is non-debuggable/optimized for performance and device acceptance testing. Every variant is signed with the same production key from `key.properties`; Gradle refuses to configure when it is absent or incomplete.

For device deployment, always use an in-place install/update so existing app data is preserved. If Android reports a signature mismatch such as `INSTALL_FAILED_UPDATE_INCOMPATIBLE`, stop and compare the installed APK certificate with the candidate certificate. Local debug keys can differ between environments or older installs; select the matching signing identity rather than replacing the app. Never uninstall the existing sloppaTV package, clear its data, or otherwise destroy the installed session merely to make a new APK install.

## Production signing

Copy `key.properties.example` to the ignored `key.properties` file, then set all four values:

- `storeFile`
- `storePassword`
- `keyAlias`
- `keyPassword`

An absent or partial signing configuration fails Gradle configuration instead of silently producing an unsigned or debug-signed APK.

Version code and version name are defined centrally in `gradle.properties` as `SLOPPATV_VERSION_CODE` and `SLOPPATV_VERSION_NAME`. The version name is also compiled into the native diagnostics screen.

## Device acceptance

Automated viewing acceptance supports Waydroid and an explicitly selected physical Google TV Streamer target. `tools/waydroid_e2e.py` requires an explicit ADB serial and target guard before running. In addition to the title-specific playback scenarios, it provides repeatable lifecycle/HOME restoration, runtime VIEW/SEARCH intent, MediaSession dump, fatal-log audit, and memory/CPU soak commands. `tools/playback_report_e2e.py` verifies an active physical-TV item against Jellyfin session state through playing progress, pause, resume, and stop while restoring the original media volume. Device testing uses the same `app.sloppatv` package as normal deployment and must update it in place. Evidence is written under `artifacts/e2e-waydroid/` or the physical-TV artifact directory selected by the harness. Real-device performance comparison uses `tools/benchmark_tv.py`; `--final-suite` selects the sample counts required by `PERFORMANCE.md`.

A roadmap item is not considered fully verified merely because it builds. Device-visible behavior remains marked partial until the relevant Android TV/Waydroid acceptance pass has been completed. A headless Waydroid session can expose an invalid/stale Android `AudioTrack` clock; when that occurs, video/audio motion results are recorded as an emulator limitation rather than adding Waydroid-specific playback behavior to the production client.

### Automated screenshots

The Android pipeline boots a disposable 1920x1080 TV emulator, installs the debug APK under the production application ID, runs the clean-install screenshot suite, validates the PNG dimensions, and uploads the images plus `screenshots.json` as the `sloppaTV-screenshots` artifact. The manifest records each image's dimensions, size, and SHA-256 digest.

Run the same clean-install suite on an emulator with:

```sh
ANDROID_SERIAL=emulator-5554 \
SLOPPATV_APK=app/build/outputs/apk/debug/app-debug.apk \
./tools/ci_screenshots.sh
```

For a logged-in Waydroid or Google TV session, use the non-destructive authenticated suite directly. It captures Home, Search, Settings, and restored Home without changing persisted settings:

```sh
./tools/waydroid_e2e.py \
  --serial 192.168.240.112:5555 \
  --target waydroid \
  screenshots --suite tools/screenshot-suites/authenticated.json
```

Use `--target google-tv-streamer` with the streamer's explicit ADB serial for physical-device evidence. Screenshot suites are declarative JSON files under `tools/screenshot-suites/`; generated evidence remains ignored under `artifacts/`.

## License

MIT. See [LICENSE.md](LICENSE.md).

# sloppaTV

sloppaTV is an experimental Android TV client for Jellyfin focused on a fast, remote-first, Netflix-style on-demand viewing experience.

The application and UI are implemented in C++20 with Android NDK `NativeActivity` and GLES3. JNI is used only where Android does not expose a practical NDK equivalent, including HTTP, media playback, image decoding and codec/display capability discovery.

## Scope

Supported scope is Movies, Series and Episodes. Music, photos, Live TV/DVR, launcher channels/recommendations and server administration are intentionally excluded.

See [ROADMAP.md](ROADMAP.md) for feature status and [PERFORMANCE.md](PERFORMANCE.md) for the current benchmark evidence and measurement rules.

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

Build the Android variants:

```sh
./gradlew test assembleDebug assembleBenchmark assembleRelease
```

`debug` is intended for development. `benchmark` is non-debuggable/optimized but uses the local Android debug key so it can be installed for performance and device acceptance testing. `release` is production-oriented and remains unsigned unless production signing credentials are supplied.

## Production signing

Set all four variables before building `assembleRelease`:

- `SLOPPATV_KEYSTORE_PATH`
- `SLOPPATV_KEYSTORE_PASSWORD`
- `SLOPPATV_KEY_ALIAS`
- `SLOPPATV_KEY_PASSWORD`

Partial signing configuration fails the Gradle configuration instead of silently producing a differently signed build.

Version code and version name are defined centrally in `gradle.properties` as `SLOPPATV_VERSION_CODE` and `SLOPPATV_VERSION_NAME`. The version name is also compiled into the native diagnostics screen.

## Device acceptance

Automated viewing acceptance is designed around Waydroid rather than a personal physical TV. `tools/waydroid_e2e.py` requires an explicit ADB serial and validates that the target is Waydroid before running. Real-device performance comparison uses `tools/benchmark_tv.py`.

A roadmap item is not considered fully verified merely because it builds. Device-visible behavior remains marked partial until the relevant Android TV/Waydroid acceptance pass has been completed.

## License

MIT. See [LICENSE.md](LICENSE.md).

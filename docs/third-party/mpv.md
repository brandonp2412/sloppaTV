# libmpv Android artifact

`app/libs/mpv-core-no-vulkan.aar` is the playback runtime shipped by sloppaTV. It packages libmpv, FFmpeg, and their
native dependencies for `arm64-v8a`, `armeabi-v7a`, and `x86_64`; Vulkan is intentionally excluded.

The imported baseline was added in commit `8c2c66e984b3879d8969484044d9c2968989c23d`. The original import did not
record an upstream source revision or build configuration, so it must not be relabelled as reproducible source. Its
immutable artifact identity is recorded in `app/libs/mpv-core-no-vulkan.aar.sha256` and enforced by
`tools/verify_mpv_aar.py`.

To upgrade it, create the AAR from a documented libmpv/FFmpeg source checkout and retain the exact source revisions,
NDK version, configure flags, patches, and build command in the upgrade commit. Then replace the AAR, update its
SHA-256 sidecar, and run:

```sh
python3 tools/verify_mpv_aar.py
./gradlew --no-daemon assembleDebug
python3 tools/run_host_tests.py
```

The verifier rejects a missing ABI, missing `libmpv.so`, or a byte mismatch. This makes the currently trusted binary
auditable and prevents an accidental local replacement. A future source build is required before changing its
provenance status from imported to reproducible.

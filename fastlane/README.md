# Google Play / Fastlane

sloppaTV uses Fastlane `supply` for Google Play metadata, screenshots, and AAB deployment.

## Setup

1. Create `app.sloppatv` in Google Play Console and complete the one-time Play Console setup. Fastlane's current `supply` setup documentation requires the app to have had a binary uploaded through Play Console at least once before API-managed publishing.
2. Create a Google Play Android Developer API service account with access to the app.
3. Install the pinned Ruby dependencies with `bundle install`.
4. Provide credentials with either `SUPPLY_JSON_KEY=/path/to/service-account.json` or `SUPPLY_JSON_KEY_DATA='<json contents>'`.
5. Keep the existing Android release signing configuration in `key.properties`.

Google Play credentials and signing files are intentionally not committed.

## Lanes

- `bundle exec fastlane android validate_metadata` validates listing text, required graphics, TV screenshots, and release notes without contacting Google Play.
- `SLOPPATV_SCREENSHOT_DIR=/path/to/screenshots bundle exec fastlane android screenshots` copies 1-8 generated 1920x1080 screenshots into `fastlane/metadata/android/en-US/images/tvScreenshots/` and validates them.
- `bundle exec fastlane android metadata` uploads listing text, graphics, and screenshots only. Set `PLAY_TRACK` to `internal`, `alpha`, `beta`, or `production` if needed; it defaults to `internal`.
- `bundle exec fastlane android internal` builds the signed release AAB and deploys it to the internal track.
- `PLAY_PRODUCTION=1 bundle exec fastlane android production` builds and deploys to production. The explicit confirmation variable is required to prevent accidental production releases.

`PLAY_RELEASE_STATUS` can override Fastlane's release status and defaults to `completed`.

## Screenshot flow

The existing screenshot harness remains the source of truth. It produces 1920x1080 PNGs and a `screenshots.json` manifest under an artifacts directory. The Fastlane `screenshots` lane invokes `tools/sync_play_store_screenshots.py` to copy those captures into Fastlane's required `tvScreenshots` metadata directory in deterministic order.

The committed screenshots are an initial curated set from the existing UI screenshot archive. Regenerating screenshots with the current build and running the `screenshots` lane replaces them.

## Play Console-only submission work

Fastlane `supply` does not replace the policy/app-content forms in Play Console. Before review, complete the privacy-policy URL, Data safety form, ads declaration, app-access/reviewer instructions if login is required, target audience/content declarations, content rating, category/tags, developer contact details, and Android TV form-factor opt-in. See `PLAY_CONSOLE_CHECKLIST.md`.

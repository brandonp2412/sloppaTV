# Google Play submission checklist

The repository now contains the Fastlane-managed store listing, release notes, TV screenshots, app icon, feature graphic, and TV banner. The following items are configured in Play Console rather than in Fastlane `supply` metadata and still need to be completed for the app record before review.

## App setup

- Create/select package `app.sloppatv` in Play Console.
- Select **App**, not Game.
- Choose an appropriate category such as Video Players & Editors or Entertainment.
- Add developer support email and any desired website/phone contact details.
- Opt into the Android TV form factor.
- Ensure the release uses the production signing identity and Play App Signing settings intended for future updates.

## App content and policy declarations

- Add a public privacy-policy URL and expose the same privacy policy from within the app.
- Complete Data safety based on the app's actual handling of Jellyfin credentials, server addresses, playback data, diagnostics, and any other user/device data.
- Declare whether the app contains ads.
- Complete target-audience and content declarations.
- Complete the content-rating questionnaire.
- Provide reviewer app-access instructions because normal use requires access to a Jellyfin server/account. Supply a stable review account/server or another review path that lets Google reach the app's functionality.
- Complete any additional declarations Play Console shows for the current app/version.

## Store listing verification

- Confirm the default language matches `fastlane/metadata/android/en-US` or add the desired locale before upload.
- Verify the app name, short description, and full description after the first metadata upload.
- Verify the 512x512 app icon and 1024x500 feature graphic.
- Verify the 1280x720 Android TV store banner.
- Verify at least one Android TV screenshot is present; the repository carries a curated 1920x1080 set and the screenshot sync lane can replace it with fresh captures.
- Confirm screenshots do not expose private server names, usernames, IP addresses, tokens, notifications, or other personal information before submission.

## Release verification

- Build with `bundle exec fastlane android internal` first.
- Install the Play-delivered internal-test build on a Google TV / Android TV device and verify login, remote navigation, playback, subtitles, seeking, lifecycle restore, and update compatibility.
- Check Play Console pre-launch reports and Android TV quality warnings.
- Promote/deploy to production only after the internal build is accepted and verified.

## Current technical baseline

The app currently targets Android SDK 36, which exceeds the current Google Play Android TV submission minimum. The manifest already declares Leanback support, no touchscreen requirement, landscape orientation, an Android TV launcher entry point, and an Android TV application/activity banner.

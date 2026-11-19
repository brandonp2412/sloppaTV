<div align="center">
  <h1>sloppaTV</h1>

  <p>A couch-first Jellyfin client for Android TV.</p>

  <p>
    <a href="https://play.google.com/store/apps/details?id=app.sloppatv"><img src="docs/brand/get-it-on-google-play.png" alt="Get sloppaTV on Google Play" height="80"></a>
    <a href="https://github.com/brandonp2412/sloppaTV/releases/latest"><img src="docs/brand/get_it_on_github.png" alt="Get sloppaTV on GitHub" height="80"></a>
  </p>
</div>

## Screenshots

<p align="center">
  <img src="fastlane/metadata/android/en-US/images/tvScreenshots/01-home.png" alt="sloppaTV home screen" width="100%">
  <br>
  <img src="fastlane/metadata/android/en-US/images/tvScreenshots/02-movie-browse.png" alt="sloppaTV movie browser" width="100%">
  <br>
  <img src="fastlane/metadata/android/en-US/images/tvScreenshots/03-search-catalog.png" alt="sloppaTV search catalog" width="100%">
  <br>
  <img src="fastlane/metadata/android/en-US/images/tvScreenshots/04-movie-details.png" alt="sloppaTV movie details" width="100%">
  <br>
  <img src="fastlane/metadata/android/en-US/images/tvScreenshots/05-player-controls.png" alt="sloppaTV player controls" width="100%">
  <br>
  <img src="fastlane/metadata/android/en-US/images/tvScreenshots/06-series-browse.png" alt="sloppaTV series browser" width="100%">
  <br>
  <img src="fastlane/metadata/android/en-US/images/tvScreenshots/07-series-details.png" alt="sloppaTV series details" width="100%">
  <br>
  <img src="fastlane/metadata/android/en-US/images/tvScreenshots/08-episodes.png" alt="sloppaTV episode browser" width="100%">
</p>

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

## Android TV release installation

Local builds and TV installs use `SLOPPATV_VERSION_CODE` as their starting version
code. GitHub Play releases use the workflow run number. The TV installer advances
the starting code when needed to update the installed production app.

For a connected TV, run `python3 tools/install_tv_release.py --serial <adb-serial>`
to build, validate, install and launch `app.sloppatv` without clearing data. The
installer uses the greater of the release floor and the installed version + 1.
Do not launch the separate `app.sloppatv.remuxtest` testing application as the
normal production client.

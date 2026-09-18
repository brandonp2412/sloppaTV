fastlane documentation
----

# Installation

Make sure you have the latest version of the Xcode command line tools installed:

```sh
xcode-select --install
```

For _fastlane_ installation instructions, see [Installing _fastlane_](https://docs.fastlane.tools/#installing-fastlane)

# Available Actions

## Android

### android screenshots

```sh
[bundle exec] fastlane android screenshots
```

Copy generated Android TV screenshots into Fastlane metadata

### android validate_metadata

```sh
[bundle exec] fastlane android validate_metadata
```

Validate local Google Play listing metadata and graphics

### android metadata

```sh
[bundle exec] fastlane android metadata
```

Upload listing text, graphics and TV screenshots without a binary

### android internal

```sh
[bundle exec] fastlane android internal
```

Build the signed AAB and deploy it to Google Play internal testing

### android production_artifact

```sh
[bundle exec] fastlane android production_artifact
```

Deploy a prebuilt signed AAB and the current listing to Google Play production

### android production

```sh
[bundle exec] fastlane android production
```

Build the signed AAB and deploy it to Google Play production

----

This README.md is auto-generated and will be re-generated every time [_fastlane_](https://fastlane.tools) is run.

More information about _fastlane_ can be found on [fastlane.tools](https://fastlane.tools).

The documentation of _fastlane_ can be found on [docs.fastlane.tools](https://docs.fastlane.tools).

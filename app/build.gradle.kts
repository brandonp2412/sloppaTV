plugins {
    id("com.android.application")
}

val sloppaVersionCode = providers.gradleProperty("SLOPPATV_VERSION_CODE").get().toInt()
val sloppaVersionName = providers.gradleProperty("SLOPPATV_VERSION_NAME").get()

val releaseSigningValues = listOf(
    System.getenv("SLOPPATV_KEYSTORE_PATH"),
    System.getenv("SLOPPATV_KEYSTORE_PASSWORD"),
    System.getenv("SLOPPATV_KEY_ALIAS"),
    System.getenv("SLOPPATV_KEY_PASSWORD"),
)
val releaseSigningConfigured = releaseSigningValues.all { !it.isNullOrBlank() }
require(releaseSigningConfigured || releaseSigningValues.all { it.isNullOrBlank() }) {
    "Release signing requires SLOPPATV_KEYSTORE_PATH, SLOPPATV_KEYSTORE_PASSWORD, SLOPPATV_KEY_ALIAS and SLOPPATV_KEY_PASSWORD together"
}

android {
    namespace = "nz.presley.sloppatv"
    compileSdk = 36
    ndkVersion = "29.0.14206865"

    defaultConfig {
        applicationId = "nz.presley.sloppatv"
        minSdk = 26
        targetSdk = 36
        versionCode = sloppaVersionCode
        versionName = sloppaVersionName

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20", "-fexceptions", "-frtti", "-O3")
                arguments += listOf(
                    "-DANDROID_STL=c++_shared",
                    "-DSLOPPATV_VERSION_NAME=$sloppaVersionName",
                )
            }
        }
        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86_64")
        }
    }

    signingConfigs {
        if (releaseSigningConfigured) {
            create("release") {
                storeFile = file(releaseSigningValues[0]!!)
                storePassword = releaseSigningValues[1]
                keyAlias = releaseSigningValues[2]
                keyPassword = releaseSigningValues[3]
            }
        }
    }

    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
            isDebuggable = false
            // Production release output stays unsigned unless all signing secrets are supplied.
            // This prevents a public/release APK from ever being silently signed with a debug key.
            signingConfig = if (releaseSigningConfigured) signingConfigs.getByName("release") else null
        }
        create("benchmark") {
            initWith(getByName("release"))
            isDebuggable = false
            signingConfig = signingConfigs.getByName("debug")
            matchingFallbacks += listOf("release")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    packaging {
        jniLibs.useLegacyPackaging = false
    }
}

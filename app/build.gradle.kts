import java.util.Properties

plugins {
    id("com.android.application")
}

val sloppaVersionCode = providers.gradleProperty("SLOPPATV_VERSION_CODE").get().toInt()
val sloppaVersionName = providers.gradleProperty("SLOPPATV_VERSION_NAME").get()

val releaseSigningPropertiesFile = rootProject.file("key.properties")
val releaseSigningProperties = Properties().apply {
    require(releaseSigningPropertiesFile.isFile) {
        "Signing requires ${releaseSigningPropertiesFile.path}; copy key.properties.example and supply the production key details"
    }
    releaseSigningPropertiesFile.inputStream().use(::load)
}
val releaseSigningValues = listOf("storeFile", "storePassword", "keyAlias", "keyPassword")
    .map { releaseSigningProperties.getProperty(it)?.trim() }
val releaseSigningConfigured = releaseSigningValues.all { !it.isNullOrBlank() }
require(releaseSigningConfigured) {
    "Signing requires non-empty storeFile, storePassword, keyAlias and keyPassword entries in ${releaseSigningPropertiesFile.path}"
}

android {
    namespace = "app.sloppatv"
    compileSdk = 36
    ndkVersion = "29.0.14206865"

    defaultConfig {
        applicationId = "app.sloppatv"
        minSdk = 26
        targetSdk = 36
        versionCode = sloppaVersionCode
        versionName = sloppaVersionName
        manifestPlaceholders["appLabel"] = "sloppaTV"

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
                storeFile = rootProject.file(releaseSigningValues[0]!!)
                storePassword = releaseSigningValues[1]
                keyAlias = releaseSigningValues[2]
                keyPassword = releaseSigningValues[3]
            }
        }
    }

    buildTypes {
        getByName("debug") {
            versionNameSuffix = "-debug"
            signingConfig = signingConfigs.getByName("release")
        }
        getByName("release") {
            isMinifyEnabled = false
            isDebuggable = false
            signingConfig = signingConfigs.getByName("release")
        }
        create("benchmark") {
            initWith(getByName("release"))
            versionNameSuffix = "-benchmark"
            isDebuggable = false
            signingConfig = signingConfigs.getByName("release")
            matchingFallbacks += listOf("release")
            externalNativeBuild {
                cmake {
                    arguments += "-DSLOPPATV_BENCHMARK=ON"
                }
            }
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
        jniLibs.pickFirsts += "**/libc++_shared.so"
    }
}


dependencies {
    val media3Version = "1.11.0"
    implementation("androidx.media3:media3-exoplayer:$media3Version")
    implementation("androidx.media3:media3-exoplayer-hls:$media3Version")
}

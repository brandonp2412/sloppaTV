import java.util.Properties

plugins {
    id("com.android.application")
}

val sloppaVersionCode = providers.gradleProperty("SLOPPATV_VERSION_CODE").get().toInt()
val sloppaVersionName = providers.gradleProperty("SLOPPATV_VERSION_NAME").get()
val splitReleaseApks = providers.gradleProperty("SLOPPATV_SPLIT_APKS")
    .orNull
    ?.toBooleanStrictOrNull()
    ?: false

val releaseSigningPropertiesFile = rootProject.file("key.properties")
val releaseSigningProperties = Properties().apply {
    if (releaseSigningPropertiesFile.isFile) {
        releaseSigningPropertiesFile.inputStream().use(::load)
    }
}
val releaseSigningValues = listOf("storeFile", "storePassword", "keyAlias", "keyPassword")
    .map { releaseSigningProperties.getProperty(it)?.trim() }
val releaseSigningConfigured = releaseSigningValues.all { !it.isNullOrBlank() }
require(!releaseSigningPropertiesFile.isFile || releaseSigningConfigured) {
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
                // Let CMake apply each variant's optimisation level: debug should not compile at -O3.
                cppFlags += listOf("-std=c++20", "-fexceptions", "-frtti")
                arguments += listOf(
                    "-DANDROID_STL=c++_shared",
                    "-DSLOPPATV_VERSION_NAME=$sloppaVersionName",
                )
            }
        }
        ndk {
            if (!splitReleaseApks) {
                // A single ABI is enough for an isolated E2E test APK.
                val e2eAbi = providers.gradleProperty("SLOPPATV_E2E_ABI").orNull
                    ?.takeIf { providers.gradleProperty("SLOPPATV_E2E_ISOLATED").orNull == "true" }
                require(e2eAbi == null || e2eAbi in listOf("armeabi-v7a", "arm64-v8a", "x86_64")) {
                    "Unsupported SloppaTV E2E ABI: $e2eAbi"
                }
                abiFilters += if (e2eAbi == null) listOf("armeabi-v7a", "arm64-v8a", "x86_64")
                    else listOf(e2eAbi)
            }
        }
    }

    splits {
        abi {
            isEnable = splitReleaseApks
            reset()
            include("armeabi-v7a", "arm64-v8a", "x86_64")
            isUniversalApk = false
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
            // Isolated E2E builds must never replace the household app or its watch state.
            if (providers.gradleProperty("SLOPPATV_E2E_ISOLATED").orNull == "true") {
                applicationIdSuffix = ".remuxtest"
                manifestPlaceholders["appLabel"] = "sloppaTV E2E"
            }
            // A local key.properties deliberately makes debug and release use the same
            // identity, so home devices can upgrade either build in place. A clean
            // checkout still gets Android's normal debug signing configuration.
            if (releaseSigningConfigured) signingConfig = signingConfigs.getByName("release")
        }
        getByName("release") {
            isMinifyEnabled = false
            isDebuggable = false
            if (releaseSigningConfigured) signingConfig = signingConfigs.getByName("release")
        }
        create("benchmark") {
            initWith(getByName("release"))
            versionNameSuffix = "-benchmark"
            isDebuggable = false
            if (releaseSigningConfigured) signingConfig = signingConfigs.getByName("release")
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


val signedArtifactTaskNames = setOf(
    "assembleRelease",
    "bundleRelease",
    "packageRelease",
    "assembleBenchmark",
    "packageBenchmark",
)
gradle.taskGraph.whenReady {
    val signedArtifactRequested = allTasks.any { task ->
        task.project == project && task.name in signedArtifactTaskNames
    }
    require(!signedArtifactRequested || releaseSigningConfigured) {
        "Release and benchmark builds require ${releaseSigningPropertiesFile.path}; copy key.properties.example and supply the signing key details"
    }
}

tasks.withType<JavaCompile>().configureEach {
    options.compilerArgs.add("-Xlint:deprecation")
}

dependencies {
    implementation(files("libs/mpv-core-no-vulkan.aar"))
}

plugins {
    id("com.android.application")
}

val nyamistBuildSha = providers.environmentVariable("GITHUB_SHA")
    .orElse("local")
    .get()
    .take(12)

android {
    buildFeatures {
        buildConfig = true
    }
    namespace = "dev.nyamist.g925surface"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "dev.nyamist.g925surface"
        minSdk = 26
        targetSdk = 35
        versionCode = 270
        versionName = "2.7.0"
        buildConfigField("String", "NYAMIST_BUILD_SHA", "\"$nyamistBuildSha\"")

        ndk {
            abiFilters += "arm64-v8a"
        }

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20", "-Wall", "-Wextra")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

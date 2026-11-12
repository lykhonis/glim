plugins {
    id("com.android.application") version "8.13.0"
}

android {
    namespace = "com.lykhonis.glim.sample"
    compileSdk = 35
    ndkVersion = "26.3.11579264"
    System.getenv("ANDROID_NDK_HOME")?.let { ndkPath = it }

    defaultConfig {
        applicationId = "com.lykhonis.glim.sample"
        minSdk = 24
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_static")
            }
        }
    }
    buildFeatures {
        prefab = true
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }
}

dependencies {
    implementation(files("../glim.aar"))
}

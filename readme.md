
# Android NDK + Vulkan Application

## Overview
This project demonstrates a Vulkan graphics application for Android using the Android NDK.

## Prerequisites
- Android Studio 4.1 or later
- Android NDK r21 or later
- Vulkan SDK
- Android device with Vulkan support (API 24+)

## Building
1. Open the project in Android Studio
2. Configure NDK path in `local.properties`
3. Build via Android Studio or command line:
    ```bash
    ./gradlew build
    ```

## Running
- Connect an Android device with Vulkan support
- Deploy and run the app from Android Studio or:
    ```bash
    ./gradlew installDebug
    ```

## Features
- Vulkan instance and device initialization
- Swapchain management
- Basic rendering pipeline
- Touch input handling

## Resources
- [Vulkan Documentation](https://www.khronos.org/vulkan/)
- [Android NDK Guide](https://developer.android.com/ndk/guides)


## Note
This project is built using Gradle from the command line only—not through Android Studio. It contains no Java code, layouts, XML files, or JNI calls. All functionality is implemented in C++ via the Android NDK.



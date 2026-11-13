# Using Glim from Gradle

`pnpm aar` builds `android/glim.aar`, a prefab package with the Vulkan toolkit stack as one static library per ABI (`arm64-v8a`, `x86_64`) and its headers. Depend on the AAR, enable `buildFeatures.prefab`, build with `ANDROID_STL=c++_static`, and in CMake:

```cmake
find_package(glim REQUIRED CONFIG)
target_link_libraries(app PRIVATE glim::glim)
```

The app defines `extern "C" int glim_app_main()` and points `NativeActivity` at its library. `android/sample` is a working Gradle app; `pnpm android-sample` assembles it.

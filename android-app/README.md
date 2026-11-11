# NYAMIST Live 3D Telemetry — Android

This installable Android application continuously renders an animated, depth-tested 3D cube using OpenGL ES 3.0. The cube is generated from rendered geometry; it is not a screenshot or a static image.

## On-screen telemetry

The application displays:
- Current and average FPS.
- Estimated 1% and 0.1% low FPS.
- Current and average frame time, plus P50/P95/P99 frame-time percentiles.
- Frame count and a frame-time stutter counter.
- OpenGL ES renderer and version.
- Application CPU usage and application PSS memory.
- Device memory information, battery temperature, resolution, and session duration.

Android's public SDK does not expose a portable, trustworthy GPU-utilization percentage. The application therefore displays GPU load as `N/A` instead of presenting an unsupported estimate.

## Graphics API distinction

This APK uses **Android OpenGL ES 3.0**. It is not a Direct3D 11 implementation. Direct3D 11 remains the primary application API for the separate Windows live-cube workload in `windows-dx11-cube/`.

## Build

The debug APK is built by `.github/workflows/build-android-apk.yml` using the Android Gradle project. The resulting artifact is intended for local installation and validation.

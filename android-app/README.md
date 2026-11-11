# NYAMIST Live 3D Telemetry APK

This installable Android companion app renders an animated, depth-tested 3D cube with OpenGL ES 3.0 and reports live Android device/session telemetry. It is **not** a Direct3D 11 implementation.

The primary DX11 workload is `windows-dx11-cube/NYAMIST-DX11-Live-Cube.exe`. Run that Windows executable in the selected Android Windows-game launcher/runtime where DXVK is available. DXVK translates the real D3D11 calls to the Android Vulkan stack selected by that runtime.

The APK reports current/average FPS, estimated 1% and 0.1% lows, current/average frame time, P50/P95/P99, frame count, a >2x-median stutter counter, GL renderer/version, app CPU usage, app PSS RAM, and battery temperature. Android's public SDK does not expose a portable GPU utilization percentage, so GPU load is displayed as `N/A` rather than guessed.

Build in CI with `.github/workflows/build-android-apk.yml`. The artifact is a debug-signed APK suitable for local installation and testing.

# Live 3D cube validation

## Primary DX11 path

`windows-dx11-cube` is the primary interactive validation workload. It uses D3D11 directly: hardware device creation, HLSL compilation, input layout, vertex/index buffers, constant buffer, depth testing, indexed cube draw, and swapchain Present. It is designed to run inside the selected Windows-game runtime on the target Android device.

The selected launcher decides which Android Vulkan driver is active. NYAMIST records observed translator/adapter/session evidence but does not select, replace, or tune that driver.

## Android companion

`android-app` is an independent native Android cube/telemetry APK using OpenGL ES 3.0. Its cube is live geometry rendered every frame, not an image. It is useful for an Android-side reference and must not be counted as D3D11 evidence.

## Metrics

Both paths report current/average FPS, estimated low-percentile FPS, current/average frame time, P50/P95/P99, CPU, memory, renderer/device identity, frame count, and session duration where exposed. Android battery temperature is available only to the APK. GPU utilization is `N/A` where a trustworthy supported public query is unavailable. The Windows DX11 executable writes `NYAMIST-DX11-Live-Cube.csv`.

The Windows workload defaults to a 60 FPS cap. F2 cycles through 30, 60, 90, 120, and uncapped. A normal exit is not treated as a crash by itself.

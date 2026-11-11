# CHORD OF NYAMIST — NYAMIST DX11 Runtime

**THE CREATIVE INTELLIGENCE UNIVERSE — V1**  
*Every Signal Has a Story.*  
Created by AMGNYA

## Project purpose

NYAMIST is a device-focused graphics runtime, validation, and live 3D workload project for the REDMI K80 Ultra (MediaTek MT6991, Arm Immortalis-G925 MC12).

**Direct3D 11 remains the primary application API and validation target.** The Windows workload is a real D3D11 application: it creates a hardware D3D11 device, compiles HLSL shaders, draws a rotating indexed 3D cube with depth testing, and presents frames. When used in an Android Windows-game environment that provides DXVK, DXVK translates the application's D3D11 calls to Vulkan. These are separate software layers and are documented as such.

## Target device

| Item | Target |
|---|---|
| Device | REDMI K80 Ultra · 25060RK16C |
| SoC | MediaTek MT6991 / Dimensity 9400+ |
| GPU | Arm Immortalis-G925 MC12 |
| Android | Android 16 · API 36 |
| ABI | arm64-v8a |
| Primary application API | Direct3D 11 |
| D3D11 translation layer, when used | DXVK |
| Lower-level Android graphics interface | Vulkan |

## Live 3D cube

The project contains two distinct live cube applications. They are not interchangeable and their results must not be attributed to the wrong graphics API.

### Windows D3D11 cube

`windows-dx11-cube/NYAMIST-DX11-Live-Cube.exe` is the primary Direct3D 11 workload. It creates a hardware D3D11 device, compiles HLSL, creates indexed cube geometry and a depth buffer, continuously rotates the cube, and presents rendered frames.

Its on-screen telemetry includes:
- Current and average FPS, plus estimated 1% and 0.1% low FPS.
- Current frame time and P50/P95/P99 frame-time percentiles.
- D3D11 feature level and GPU adapter identity.
- CPU usage, process working-set RAM, and dedicated VRAM reported by DXGI.
- Frame count, session duration, stutter counter, and selected frame cap.

The application writes CSV telemetry beside the executable. The default frame cap is 60 FPS; F2 cycles 30 → 60 → 90 → 120 → OFF. GPU utilization is shown as `N/A` when the available D3D11/DXGI interfaces do not provide a trustworthy portable value.

### Android live cube

`android-app/` is an installable Android application that continuously renders an animated, depth-tested 3D cube using OpenGL ES 3.0. The cube is rendered geometry, not a static image. Its on-screen display reports Android rendering and device/session measurements, including FPS, frame time, GL renderer/version, CPU, memory, frame count, stutter count, session duration, and battery temperature.

Android's public SDK does not provide a portable trustworthy GPU-utilization percentage, so GPU load is reported as `N/A` rather than estimated or invented.

**The Android application is an OpenGL ES 3.0 renderer, not a D3D11 renderer.** Its successful display confirms that the Android cube application runs; it does not by itself prove D3D11 execution.

## Runtime and validation components

The repository includes the C++20 NYAMIST core, hardware discovery, Vulkan loader/backend and resource components, compute and graphics-pipeline components, runtime component registry, D3D11 evidence adapter, session/performance importers, profile storage, A/B evaluation, and automated tests.

A successful unit test or Vulkan command submission is not represented as proof of a working D3D11 game. Device discovery, API/translator creation, graphics rendering, Present, sustained operation, and performance stability are distinct validation stages.

## Evidence rules

- Keep Direct3D 11 as the primary application-facing API.
- Identify the actual renderer/API shown by each application; do not label OpenGL ES output as D3D11.
- Report measured values only when they are present in captured output or telemetry. Do not invent missing metrics.
- Distinguish observed values from estimates. Do not treat GPU heap size as free VRAM or a device temperature as GPU temperature.
- GPU utilization is `N/A` where a trustworthy supported query is unavailable.
- A normal user-initiated application exit is not automatically a crash.
- Do not modify protected Android system partitions, kernel modules, device permissions, or SELinux policy as part of routine validation.

## Build and test

For the C++ project in Termux:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNYAMIST_BUILD_TESTS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

The device-side `nyamist-build-all.sh` script performs the supported Termux build and smoke checks. Vulkan header/loader availability is detected during configuration.

The Android APK and Windows x64 D3D11 executable are built by their respective GitHub Actions workflows, which publish downloadable artifacts.

## Project status

**Engineering preview.** The C++ runtime and its Vulkan/backend tests are separate from the live cube applications. The Windows cube is the real D3D11 workload. The Android application is a native OpenGL ES 3.0 live-cube application. Build success, application launch, visible rendering, and sustained performance are recorded as separate facts.

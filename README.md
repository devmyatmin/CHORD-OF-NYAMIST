# CHORD OF NYAMIST — NYAMIST DX11 Runtime

**THE CREATIVE INTELLIGENCE UNIVERSE — V1**  
*Every Signal Has a Story.*  
Created by AMGNYA

## DX11-first purpose

NYAMIST is a device-focused graphics runtime, validation, and performance project for the REDMI K80 Ultra (MediaTek MT6991, Arm Immortalis-G925 MC12). **Direct3D 11 is the primary application API and validation target.** On Android, Windows D3D11 workloads execute through a compatibility runtime such as Wine/FEX/Box64 and DXVK; DXVK translates D3D11 calls to the Android Vulkan driver selected by the launcher. NYAMIST keeps those layers distinct and records their evidence separately.

```text
D3D11 application / live 3D workload
        ↓
DXVK — D3D11 to Vulkan translation
        ↓
Android Vulkan loader and selected system/app-local driver
        ↓
REDMI K80 Ultra GPU
```

The selected Android graphics driver is supplied by the device/launcher configuration. NYAMIST does not replace it, patch it, or claim ownership of it. The stock device driver remains untouched and available as the user's fallback.

## Target device

| Item | Target |
|---|---|
| Device | REDMI K80 Ultra · 25060RK16C |
| SoC | MediaTek MT6991 / Dimensity 9400+ |
| GPU | Arm Immortalis-G925 MC12 |
| Android | Android 16 · API 36 |
| ABI | arm64-v8a |
| Primary graphics API | Direct3D 11 |
| Translation layer | DXVK |
| Hardware interface | Android Vulkan loader and the device-selected driver |

## Current implementation

The repository contains the C++20 NYAMIST core, hardware discovery and Vulkan validation tools, runtime component registry, DX11 translator evidence adapter, session/performance importers, profile storage, A/B evaluation, and automated tests. The Termux build also includes Vulkan backend/resource/compute/graphics-pipeline components and runtime smoke commands in the current development source bundle.

A successful unit test or Vulkan command submission is not presented as proof of a working D3D11 game. NYAMIST distinguishes device discovery, API/translator creation, graphics rendering, Present, sustained gameplay, and performance stability as separate gates.

## Live 3D validation target

The interactive workload now has two distinct deliverables:

1. `windows-dx11-cube/NYAMIST-DX11-Live-Cube.exe` is the **primary, real Direct3D 11 workload**. It creates a hardware D3D11 device, compiles HLSL, draws a depth-tested rotating indexed cube, presents frames, and exports CSV telemetry. Run it inside the selected Windows-game runtime on the target Android device.
2. `android-app/` is an installable companion APK. It renders its own live 3D cube using Android OpenGL ES 3.0 and displays device telemetry. It is a native Android reference test and is **not** presented as a D3D11 renderer.

The D3D11 HUD reports:

- FPS: current, average, minimum, 1% low, and 0.1% low.
- Frame time: current, average, P50, P95, and P99.
- D3D11 feature level and GPU adapter identity.
- Current/average FPS, 1%/0.1% low estimates, current frame time, and P50/P95/P99 frame-time percentiles.
- CPU usage normalized to logical processors, process working-set RAM, DXGI dedicated VRAM, session duration, and stutter counter.
- CSV telemetry written beside the executable. F2 cycles the frame cap 30 → 60 → 90 → 120 → OFF; default is 60 FPS.

GPU utilization is reported as `N/A` where a trustworthy portable query is not available. Battery temperature is displayed by the Android companion APK. Metrics not exposed by the applicable public API are not fabricated.

## Evidence and safety rules

- DX11 is the main application-facing API; DXVK is the D3D11 translation layer; Android Vulkan is the lower-level graphics interface.
- The launcher-selected driver is an external runtime dependency, not a NYAMIST-owned component.
- Do not write to `/vendor`, alter SELinux, change `/dev/mali0` permissions, replace kernel modules, or disable Android protections.
- Report the exact device, driver identity, DXVK version, Wine/FEX/Box64 version, resolution, frame cap, and source of each metric.
- Distinguish observed values from inferred values. Do not use GPU heap size as free VRAM or a device temperature as GPU temperature.
- A normal user-initiated game exit is not automatically a crash. Exit codes are interpreted with the session and user action context.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNYAMIST_BUILD_TESTS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

On Android/Termux, use `nyamist-build-all.sh` for the device-side C++ build and supported runtime smoke commands. Vulkan headers/loader availability is detected at configure time. The Android APK and Windows D3D11 executable are built by their GitHub Actions workflows; both workflows publish installable/downloadable artifacts. The DX11 executable is launched from the selected Android Windows-game launcher.

## Project status

**Engineering preview.** The C++ runtime and its existing Vulkan/backend test coverage remain separate from the live workload applications. The Windows cube is a real D3D11 workload; the Android APK is a native OpenGL ES companion. Neither one claims to be a replacement for the target device's selected graphics driver.

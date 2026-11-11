# NYAMIST real D3D11 live cube

This is the **primary Direct3D 11 workload**. It creates a hardware D3D11 device, compiles HLSL, creates a depth buffer and indexed cube geometry, and continuously rotates and presents the cube. It is not a picture or a Vulkan/GLES simulation.

## Build

On Windows with Visual Studio C++ and CMake:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The executable is `build/Release/NYAMIST-DX11-Live-Cube.exe`.

Run it in the selected Windows-game launcher/runtime on Android. When that runtime uses DXVK, the workload's actual D3D11 API calls are translated to Vulkan by DXVK. The app itself does not select or replace the device's graphics driver.

## Live HUD and CSV

The on-window HUD reports D3D11 feature level, adapter name, current/average FPS, 1%/0.1% low estimates, current frame time, P50/P95/P99, CPU use normalized to the device's logical processors, process working-set RAM, dedicated VRAM reported by DXGI, frame count, stutter counter, session time, and the selected FPS cap. GPU utilization is shown as `N/A` because a portable, trustworthy utilization query is not available through the D3D11/DXGI interfaces used here. Battery temperature is available only in the Android APK companion, not this Windows process.

CSV is written beside the executable as `NYAMIST-DX11-Live-Cube.csv`. Default cap is 60 FPS to avoid forcing an unnecessary uncapped load. Press **F2** to cycle 30 → 60 → 90 → 120 → OFF.

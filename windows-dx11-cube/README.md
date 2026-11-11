# NYAMIST — Direct3D 11 Live Cube

This is the project's primary **Direct3D 11 workload**. It creates a hardware D3D11 device, compiles HLSL shaders, creates a depth buffer and indexed cube geometry, continuously rotates the 3D cube, and presents rendered frames. It is a live D3D11 application, not a static picture and not an OpenGL ES or Vulkan substitute.

## Build

With Visual Studio C++ and CMake on a Windows build host:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The executable is `build/Release/NYAMIST-DX11-Live-Cube.exe`.

## Live HUD and CSV

The on-window HUD reports:
- D3D11 feature level and adapter name.
- Current/average FPS and estimated 1%/0.1% low FPS.
- Current frame time and P50/P95/P99 frame-time percentiles.
- CPU use normalized to logical processors.
- Process working-set RAM and dedicated VRAM reported by DXGI.
- Frame count, stutter counter, session time, and selected FPS cap.

GPU utilization is shown as `N/A` because the D3D11/DXGI interfaces used here do not provide a portable, trustworthy utilization query. The application writes `NYAMIST-DX11-Live-Cube.csv` beside the executable.

The default frame cap is 60 FPS. Press **F2** to cycle 30 → 60 → 90 → 120 → OFF.

## Runtime distinction

The executable uses Direct3D 11 at the application level. If the Windows runtime environment supplies DXVK, DXVK translates those D3D11 calls to Vulkan. This translation does not change the application's identity as a D3D11 workload, and the application does not replace or claim ownership of the device's graphics driver.

## Evidence

Only values visible in the HUD or recorded in the CSV should be reported as measured results. Build completion alone does not prove that the application rendered successfully on a target device, and a short captured session should not be described as a sustained performance test.

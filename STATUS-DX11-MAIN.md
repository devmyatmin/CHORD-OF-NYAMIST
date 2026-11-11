# NYAMIST DX11-first update status

## Source changes

- DX11 is the main application-facing API and the primary live workload.
- Removed driver-specific legacy adapter/importer/tests/configuration/documentation and replaced the driver evidence interface with a generic Android Vulkan driver adapter.
- Kept the Vulkan backend, resource manager, compute pipeline, graphics pipeline, loader, smoke commands, and their existing tests.
- Added a real Windows Direct3D 11 rotating cube workload with HLSL compilation, hardware D3D11 device creation, depth testing, indexed draw, Present, live HUD, FPS/frametime percentiles, CPU, RAM, adapter/VRAM facts, frame cap switching, stutter counter, and CSV output.
- Added an Android installable companion APK with an actual continuously rendered OpenGL ES 3.0 cube and Android telemetry. It is explicitly a native Android reference renderer, not a D3D11 renderer.
- Added GitHub Actions builds for the Android debug APK and Windows x64 D3D11 executable.
- Added a Termux source-sync script targeting the `dx11-live-cube-mainline` GitHub branch.

## Checks completed in the current host environment

- CTest: **17/17 passed**.
- Python importer/session tests: **35/35 passed**.
- Shell syntax checks: passed.
- Driver-specific source/docs/config reference scan: clean.

## Not yet device/build verified

- Android APK compile/install/run on the REDMI K80 Ultra: pending GitHub Actions artifact and device installation.
- Windows D3D11 executable compile/run under the selected Android game launcher: pending GitHub Actions artifact and device run.
- These two platform-specific deliverables have build workflows; this host did not have Android SDK/Gradle or Windows SDK/MSVC, so no APK/EXE binary is claimed as already built here.

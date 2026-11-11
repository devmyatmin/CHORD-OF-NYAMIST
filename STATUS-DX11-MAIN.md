# NYAMIST — DX11 Mainline Status

## Source changes

- Direct3D 11 remains the primary application-facing API and the primary live workload.
- The Windows D3D11 cube workload creates a hardware D3D11 device, compiles HLSL, renders a depth-tested indexed rotating cube, presents frames, and exposes live telemetry and CSV output.
- The Android application renders a separate live 3D cube using OpenGL ES 3.0 and displays Android device/session telemetry. It is not a D3D11 renderer.
- The C++ source includes the Vulkan backend, resource manager, compute pipeline, graphics pipeline, loader, smoke commands, and their existing tests.
- GitHub Actions workflows build the Android debug APK and Windows x64 D3D11 executable.

## Recorded build and test results

- CTest: **17/17 passed** in the recorded Termux test run.
- Vulkan compute verification: **PASS**, with GPU readback value 42 matching the expected value 42.
- Android debug APK GitHub Actions workflow: **completed successfully** for commit `ad2372226fce48f1f3fb6b6da47ce264ce422166`.
- Windows x64 D3D11 GitHub Actions build workflow: **completed successfully** for the same commit.
- The Android APK was installed and launched by the user; the live cube and telemetry display were confirmed visible.

## Evidence boundaries

- Android cube visibility confirms the OpenGL ES 3.0 application rendered on the device. It is not evidence of D3D11 execution.
- Windows D3D11 CI success confirms that the executable was built and the artifact uploaded. It does not establish a target-device runtime result.
- Performance values must be copied from the actual supplied screenshot or CSV. Do not infer or fabricate values that are not legible in the evidence.
- A build pass, a smoke-test pass, a visible cube, and a sustained performance result are separate outcomes.

## Current status

The mainline documentation now describes the two live cube applications according to their actual graphics APIs and records only the build, test, and runtime evidence stated above. No additional FPS benchmark is introduced by this documentation update.

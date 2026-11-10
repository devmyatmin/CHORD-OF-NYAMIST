# NYAMIST-Owned Runtime Validation Runbook

**Target:** REDMI K80 Ultra / Android 16 / arm64-v8a / MT6991 / Mali-G925  
**Architecture:** NYAMIST-owned Vulkan backend and D3D11 translation; Android public Vulkan loader and installed vendor driver remain the hardware interface.

This runbook supersedes the former PanPlay + PanVK + DXVK workflow. Those components are not part of the NYAMIST runtime path.

## 1. Build the development branch

```sh
pkg update
pkg install git clang cmake ninja pkg-config python
git clone https://github.com/devmyatmin/CHORD-OF-NYAMIST.git
cd CHORD-OF-NYAMIST
git fetch origin
git switch feature/nyamist-owned-runtime-foundation
```

Build and run tests:

```sh
cmake -S . -B build-termux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DNYAMIST_BUILD_TESTS=ON \
  -DNYAMIST_ENABLE_VULKAN_DISCOVERY=ON \
  -DNYAMIST_FORCE_VULKAN_DYNAMIC_LOADING=ON
cmake --build build-termux
ctest --test-dir build-termux --output-on-failure
```

## 2. Validation stages

Do not skip stages or treat one stage as proof of a later stage.

| Stage | Evidence | What it proves |
|---|---|---|
| Device discovery | Vulkan loader and physical-device report | Loader access and device enumeration |
| Device/queue | Logical-device and graphics-queue report | Device and queue lifecycle |
| Submission | Command-buffer submit and fence report | Queue submission and completion |
| Native rendering | NYAMIST triangle + pixel readback | NYAMIST-owned Vulkan render path |
| Presentation | Android Surface acquire/submit/present report | WSI and swapchain path |
| D3D11 slice | D3D11 calls create device/context, draw and present | Initial D3D11 translation path |
| Application | Named application, repeatable sustained run | Only the specific application tested |

The existing R1.1–R1.3 commands are diagnostics. An empty command-buffer submission is not a rendering test. A native Vulkan triangle is not a D3D11 compatibility test.

## 3. Capture device and backend reports

Use the report commands available in the checked-out revision:

```sh
./build-termux/nyamist-cli discover --output build-termux/device.json
./build-termux/nyamist-cli validate-device --output build-termux/device-validation.json
./build-termux/nyamist-cli validate-submit --output build-termux/submit-validation.json
```

A command may return a nonzero status when the platform does not expose Vulkan or a validation step fails. Preserve the JSON report; do not edit failure fields to make a test pass.

## 4. Rendering and presentation checks

When the native renderer and WSI stages are implemented:

- Render the same deterministic test scene at a fixed extent.
- Capture pixel readback and compare against a known expected image with a documented tolerance.
- Record acquire, submit, present and fence results separately.
- Exercise Surface destruction/recreation, portrait/landscape and size changes.
- Record actual SUBOPTIMAL/OUT_OF_DATE results and successful recovery; do not infer them from a resize alone.
- Check resource teardown and repeat the run after a cold process start.

## 5. D3D11 validation

When the NYAMIST D3D11 slice is implemented:

- Use a small test application that calls the NYAMIST D3D11 device and immediate-context interfaces directly.
- Record the exact shader bytecode format and supported feature subset.
- Verify resource creation, shader/pipeline setup, draw submission and Present.
- Compare output with a known reference image.
- Treat unsupported interfaces/features as explicit failures, not silent no-ops.

## 6. Sustained application validation

Only after the D3D11 smoke test passes should a named application be tested. Keep application version, scene, resolution and settings fixed. Record visible output, clean exit, device-loss events and frame-time data. A sustained run is evidence for that application/configuration only; it does not establish broad game compatibility.

## Safety and integrity

- No root, vendor partition, kernel, SELinux or GPU device-node modifications.
- Do not replace Android's installed Vulkan driver.
- Keep test reports and logs intact; redact personal identifiers before public sharing.
- Never describe the project as game-compatible or production-ready without repeatable target-device application evidence.

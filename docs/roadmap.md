# NYAMIST Roadmap

## Current objective

Build a NYAMIST-owned graphics runtime for Android, then implement a custom D3D11 compatibility layer on top of NYAMIST's Vulkan backend. The target device is REDMI K80 Ultra (MT6991 / Mali-G925).

NYAMIST uses Android's public Vulkan loader and the installed vendor GPU driver as the hardware interface. NYAMIST owns the API translation, runtime state, resource management, command generation, synchronization and presentation orchestration. It does not replace the vendor Vulkan driver.

See [Standalone Runtime Architecture](standalone-runtime-architecture.md) for the ownership contract.

## Verified foundation

- [x] Portable C++20 core, deterministic fingerprints and validated cache.
- [x] Runtime diagnostics, device capability discovery and evidence/reporting framework.
- [x] Android Vulkan dynamic-loader path in the R1.1 branch.
- [x] Logical-device and graphics-queue validation in the R1.2 branch.
- [x] Empty command-buffer submission and fence validation in the R1.3 branch.
- [x] Android Surface/WSI validation harness exists in the R2.x PR stack.
- [ ] Integrate the stacked R1.1–R1.3 work into the owned-runtime development line.
- [ ] Turn one-off Vulkan probes into a reusable backend with explicit lifetime and dispatch ownership.

These checks establish Vulkan plumbing only. They do not establish a complete renderer, D3D11 compatibility or game support.

## Milestone 1 — reusable NYAMIST Vulkan backend

- [ ] Reuse one process-local Vulkan loader/dispatch implementation.
- [ ] Own instance, physical-device selection, logical device and queue lifetimes.
- [ ] Add explicit backend state, error propagation and deterministic teardown.
- [ ] Add backend contract tests and keep Android dynamic loading covered in CI.

**Acceptance:** backend initializes and shuts down cleanly on the target device; failures are reported accurately.

## Milestone 2 — resources and render core

- [ ] Implement memory-type selection and buffer/image allocation.
- [ ] Implement buffer/image views, upload paths and resource lifetime tracking.
- [ ] Implement shader-module ingestion and pipeline/layout creation.
- [ ] Implement command recording, barriers and descriptor management.
- [ ] Render a NYAMIST-native triangle and verify output by readback.

**Acceptance:** a repeatable native render/readback pass on REDMI K80 Ultra. This is not yet a D3D11 pass.

## Milestone 3 — Android presentation

- [ ] Integrate ANativeWindow surface creation and swapchain ownership.
- [ ] Implement acquire → submit → present synchronization.
- [ ] Handle Surface recreation, resize, SUBOPTIMAL and OUT_OF_DATE recovery.
- [ ] Validate portrait/landscape and repeated lifecycle changes on-device.

**Acceptance:** rendered native frames remain correct across Surface changes and recovery.

## Milestone 4 — custom D3D11 compatibility slice

- [ ] Define NYAMIST-owned D3D11 device and immediate-context interfaces.
- [ ] Add a bounded DXBC-to-SPIR-V shader path; document unsupported bytecode/features.
- [ ] Map one vertex buffer, a basic vertex/pixel shader pair and one draw call.
- [ ] Map D3D11 Present to NYAMIST's swapchain.
- [ ] Build a small D3D11 test application that uses only the NYAMIST implementation.

**Acceptance:** D3D11 calls create a NYAMIST device/context, draw the expected triangle and present it. A native Vulkan triangle alone does not count.

## Milestone 5 — D3D11 feature expansion

- [ ] Constant buffers and resource bindings.
- [ ] Textures, shader-resource/render-target views and samplers.
- [ ] Input layouts and common blend/raster/depth-stencil states.
- [ ] Pipeline/descriptor caching and synchronization correctness.
- [ ] Expand feature-level coverage based on tests, not assumptions.

## Milestone 6 — real application validation

- [ ] Run named D3D11 applications through the NYAMIST implementation.
- [ ] Verify visible rendering, scene transitions, input, clean exit and device-loss handling.
- [ ] Run repeatable sustained sessions and capture frame-time/thermal data.
- [ ] Publish supported/unsupported feature lists and known limitations.

**Release gate:** do not claim game compatibility or production readiness until named applications have repeatable sustained rendering evidence on the target device.

## Explicit exclusions

- PanPlay, the selected Android Vulkan driver and DXVK are not NYAMIST runtime dependencies.
- No custom Vulkan ICD/HAL or replacement GPU driver in this project scope.
- No root, vendor partition, kernel, SELinux or direct GPU device-node modifications.
- No automatic tuning in the rendering path without separately reviewed evidence.

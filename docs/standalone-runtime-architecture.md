# NYAMIST-Owned Graphics Runtime — Architecture Contract

**Status:** foundation branch; implementation is incremental  
**Target:** Android 16 / arm64-v8a / REDMI K80 Ultra (MT6991, Mali-G925)  
**Runtime policy:** NYAMIST owns the graphics runtime and D3D11 translation. The device's installed Android Vulkan loader and vendor GPU driver remain the hardware interface.

## 1. Ownership boundary

NYAMIST is responsible for:
- D3D11-facing device, immediate-context, resource, view, sampler and state abstractions.
- Translating supported D3D11 operations into NYAMIST's Vulkan command stream.
- Resource lifetime, descriptor/pipeline caches, barriers and frame synchronization.
- Swapchain lifecycle, frame scheduling, diagnostics and capability reporting.
- A documented feature-level/subset contract. Unsupported D3D11 features must fail explicitly.

The platform is responsible for:
- Android's public `libvulkan.so` loader.
- The installed vendor Vulkan implementation for the physical GPU.
- Android `ANativeWindow` and platform surface lifecycle.

NYAMIST does **not** implement a replacement Vulkan ICD/HAL in this architecture. A custom ICD would be a separate GPU-driver project and is not required for NYAMIST to own the D3D11 runtime.

## 2. Explicit exclusions

- PanPlay is not a runtime dependency.
- PanVK is not selected or bundled by NYAMIST.
- DXVK is not used or wrapped as the D3D11 implementation.
- No root access, vendor partition modification, kernel driver replacement, or direct GPU device-node access.
- No claim of game compatibility until real application rendering and sustained tests pass.

## 3. Runtime layers

```text
D3D11 application interface
        |
NYAMIST D3D11 compatibility API
  Device / ImmediateContext / Resources / Views / State
        |
NYAMIST translation and state tracker
  DXBC ingestion -> shader IR/SPIR-V path
  D3D11 state/resource mapping -> Vulkan commands
        |
NYAMIST Vulkan backend
  Instance / PhysicalDevice / Device / Queue
  Memory / Buffers / Images / Pipelines / Descriptors
  Command buffers / Barriers / Fences / Semaphores
  Android Surface / Swapchain / Present
        |
Android libvulkan.so loader
        |
Installed vendor Vulkan GPU driver
        |
Mali-G925
```

## 4. Implementation sequence

1. **Backend foundation:** reusable Vulkan loader/dispatch and owned instance/device/queue lifetime. Reuse the verified process-local loader path; do not duplicate one-off probe implementations in the final backend.
2. **Resources:** memory-type selection, buffers, images, views, mapping and upload paths with deterministic cleanup.
3. **Render core:** shader-module ingestion, pipeline layouts, graphics pipelines, render targets and a NYAMIST-native triangle demo.
4. **WSI:** Android surface/swapchain lifecycle, acquire-submit-present synchronization, resize and out-of-date recovery.
5. **D3D11 slice:** a deliberately small D3D11-compatible device/context contract, one vertex buffer, one simple shader pair, one draw and Present mapped to the backend.
6. **Feature expansion:** constant buffers, textures/samplers, input layouts, blend/raster/depth state, then broader D3D11 coverage.
7. **Validation:** API-independent unit tests, host CI, target-device rendering, pixel/readback checks, resize recovery and sustained runs.

## 5. Acceptance rules

- A Vulkan enumeration or empty command submission is backend validation only; it is not a rendering pass.
- A triangle rendered by NYAMIST proves the native render path only; it is not D3D11 compatibility.
- A D3D11 smoke pass requires a D3D11 API call path that creates the NYAMIST device/context, submits a draw and presents the expected output.
- A game-compatibility claim requires named-game, repeatable, sustained target-device evidence.
- Every report distinguishes `not_attempted`, `unsupported`, `failed` and `passed`; missing evidence is never treated as success.

## 6. First implementation target

Create one reusable Vulkan backend module on top of the existing Android loader dispatch, with explicit ownership and teardown. Keep the existing R1.1–R1.3 validation commands as diagnostics while the backend becomes the runtime path. The first visual milestone is a NYAMIST-native triangle; D3D11 translation begins only after resource, pipeline and presentation contracts are stable.

# D3D11 → NYAMIST Vulkan validation

## Goal

Validate NYAMIST's own D3D11 runtime and Vulkan backend on REDMI K80 Ultra / MT6991 / Immortalis-G925 MC12.

The active path is:

```text
D3D11 application
  -> Windows API / ABI compatibility host
  -> NYAMIST D3D11 runtime and translator
  -> NYAMIST Vulkan backend
  -> Android Vulkan loader (libvulkan.so)
  -> installed vendor Vulkan GPU driver
  -> Immortalis-G925 MC12
```

PanPlay, PanVK, and DXVK are not part of the active path. Historical reports from those components may be retained as provenance/reference data, but they cannot count as NYAMIST runtime validation.

## Current evidence and limits

The device has previously enumerated its stock ARM proprietary Mali-G925 Vulkan driver through Termux. Recorded evidence includes Vulkan 1.3.278 device API and driver version 49.1.0. This proves the system loader/vendor-driver path can be queried; it does not prove that NYAMIST creates its own logical device, submits work, or presents frames.

A previous offscreen triangle/pixel-readback test was reported as passing in the engineering work. Treat that as a foundation, not as proof of a complete runtime or D3D11 compatibility. Android WSI resize/out-of-date recovery and NYAMIST D3D11 translation still require explicit end-to-end tests.

Legacy PanVK/DXVK game-session measurements are historical external-stack evidence only. They must not be presented as NYAMIST game-performance results.

## Gate A — NYAMIST Vulkan device backend

- Link to Android's existing Vulkan loader using the NDK Vulkan headers/library.
- Create a NYAMIST-owned VkInstance.
- Enumerate and record the physical device from live calls.
- Query queue-family properties and select a graphics queue.
- Create a NYAMIST-owned VkDevice and queue.
- Create a command pool and submit a minimal command buffer.
- Record every VkResult and clean up all handles on failure.

**Pass:** NYAMIST itself creates a logical device and successfully submits a command buffer on the target phone.

## Gate B — resources and offscreen rendering

- Create a buffer and image with validated usage flags.
- Query memory requirements and select a compatible memory type.
- Allocate and bind memory.
- Create image view, render pass/dynamic-rendering path, and minimal graphics pipeline.
- Draw a deterministic triangle.
- Synchronize with a fence and read back pixels.
- Compare selected pixels against expected values.

**Pass:** repeatable pixel output matches expected values on the target device.

## Gate C — Android WSI and presentation

- Create an app-owned Android surface.
- Query surface support, formats, capabilities, and present modes.
- Create a swapchain with a supported configuration.
- Acquire, submit, and present frames with correct semaphore/fence handling.
- Handle surface loss, pause/resume, resize, `VK_SUBOPTIMAL_KHR`, and `VK_ERROR_OUT_OF_DATE_KHR`.
- Record successful and failed acquire/present results.

**Pass:** stable visible presentation and successful swapchain recreation on the target device.

## Gate D — D3D11 translator vertical slice

Begin with a deliberately small supported subset:

- NYAMIST D3D11 device and immediate context.
- Vertex buffer and input layout.
- One vertex shader and one pixel shader.
- Render-target view.
- Basic viewport/scissor and rasterizer state.
- One draw and Present.

Choose a DXBC-to-SPIR-V compiler path only after checking source availability, license, Android ARM64 build support, and shader-model coverage. Do not silently substitute DXVK or another D3D11 runtime.

**Pass:** a standalone D3D11 test application calls NYAMIST interfaces and produces the expected pixels through the NYAMIST Vulkan backend.

## Gate E — sustained application validation

Only after Gate D passes:

- Validate the Windows API/ABI compatibility host independently.
- Select one named D3D11 application and fixed scene/settings.
- Run for at least 300 seconds.
- Record frame-time p50/p95/p99, memory, temperature, visual issues, crashes, and device-loss events where available.
- Confirm clean process exit.
- Repeat cold and warm runs under identical conditions.

A synthetic triangle or hosted CI pass is not a commercial-game compatibility result. Do not claim game compatibility or production readiness without repeatable, visually verified sustained execution.

## Memory and cache policy

- Do not treat total device-local heap size as free memory.
- Query memory properties and requirements before allocation.
- Use bounded NYAMIST-owned metadata/shader/pipeline caches with device, driver, compiler, and schema identity.
- Do not read, rewrite, or claim ownership of vendor-driver or third-party caches.
- Handle allocation failures explicitly and release resources deterministically.

## Non-goals

- Writing a custom Vulkan ICD/HAL or kernel driver in the first runtime milestones.
- Replacing Android's system Vulkan loader or vendor driver.
- Modifying protected Android/vendor partitions or GPU device-node permissions.
- Depending on PanPlay, PanVK, or DXVK.
- Claiming D3D11 feature-level support that has not been implemented and tested.
- D3D12/VKD3D support before the D3D11 path is stable.

# DX11 → Vulkan validation plan for REDMI K80 Ultra

## Goal

Run a real Direct3D 11 application through a maintained translation/runtime stack on Android, then add NYAMIST as a non-invasive profiling and policy sidecar.

This document distinguishes the layers:

- A D3D11 game issues Direct3D 11 and DXGI API calls.
- Wine plus a compatible CPU execution layer provides the Windows API / executable environment.
- DXVK implements the D3D11-to-Vulkan translation.
- The Android Vulkan loader selects an app-local Vulkan ICD.
- PanVK kbase is the experimental Vulkan ICD candidate for the Mali kbase kernel interface.
- The vendor `mali_kbase` kernel driver remains the kernel-side hardware interface.
- Immortalis-G925 executes the GPU work.

Vulkan does not parse or execute D3D11 API calls by itself. DXVK is the translation component. PanVK must implement the Vulkan features that the selected DXVK build actually uses.

## Current K80 Ultra evidence

The user's Termux `vulkaninfo` capture enumerated the stock ARM proprietary Mali-G925-Immortalis MC12 driver:

- Device API version: Vulkan 1.3.278
- Driver version: 49.1.0
- Driver ID: ARM proprietary
- Device-local heap 0: 10.97 GiB
- Device-local heap 1: 100 MiB
- Memory types: four
  - Type 0: device-local, host-visible, host-coherent
  - Type 1: device-local, host-visible, host-cached
  - Type 2: device-local, lazily allocated
  - Type 3: device-local, protected
- The filtered output did not contain `VK_EXT_memory_budget`, `heapBudget`, or `heapUsage`.

This proves physical-device enumeration through the stock Android Vulkan driver in Termux. It does not prove PanVK works, that the same features are exposed by PanVK, or that DXVK can create a D3D11 device.

The 10.97 GiB heap is not free memory and is not a per-game allocation guarantee. Since the memory-budget extension was not found in this capture, do not infer available GPU memory from heap size. Use allocation success/failure and process/runtime telemetry where available.

## Safe NYAMIST memory policy

Until a runtime exposes a reliable memory budget:

1. NYAMIST itself should not reserve large GPU allocations.
2. Keep the profiling database and recommendation cache CPU-side and size-capped.
3. Treat the Vulkan memory type flags as allocation constraints, not as performance rankings.
4. Use host-coherent memory only where the resource usage and memory-type compatibility permit it.
5. Host-cached memory may help CPU readback, but mapping and synchronization must follow Vulkan rules.
6. Lazily allocated memory is not a general upload/storage heap.
7. Protected memory is not ordinary CPU-mappable memory.
8. Let DXVK and the Vulkan driver own their shader, pipeline, and resource allocations. NYAMIST must not rewrite DXVK cache files.
9. If allocation telemetry is unavailable, use conservative fixed limits and graceful fallback rather than guessing free memory.

## DX11-first validation gates

### Gate A — Vulkan ICD

- Build or obtain a PanVK kbase package for arm64-v8a and the target Android environment.
- Keep the stock vendor driver intact.
- Select the experimental ICD only through a supported app-local/launcher mechanism.
- Run the PanVK device probe.
- Run a minimal Vulkan compute test and a graphics render/present test.
- Capture device name, driver identity, API version, extensions, feature bits, memory properties, and logs from that same process.

### Gate B — DXVK

- Pin the exact DXVK build and record its required Vulkan extensions/features.
- Confirm the compatibility runtime can launch a known D3D11 test application.
- Confirm DXVK creates a D3D11 device and selects the intended Vulkan physical device/ICD.
- Test shader compilation, resource creation, synchronization, and swapchain presentation.
- Save the complete logs and distinguish device-creation failures from shader, presentation, or game-specific failures.

### Gate C — game baseline

- Test one small, legally obtained/user-owned D3D11 application first.
- Record whether it launches, renders correctly, presents frames, survives scene changes, and exits cleanly.
- Repeat cold and warm launches.
- Record frame-time, shader-cache behavior, memory telemetry, and thermal state.
- Do not enable performance overrides until the unmodified DXVK baseline is repeatable.

### Gate D — NYAMIST sidecar

Only after Gates A–C pass:

- Import the device/driver/runtime profile.
- Associate observations with game identity, DXVK version, Vulkan driver version, and NYAMIST policy version.
- Generate advisory recommendations only.
- Validate each recommendation against the measured baseline.
- Cache only validated NYAMIST-owned metadata.
- Keep a one-step disable path that returns to unmodified DXVK settings.

## Explicit non-goals for the first milestone

- Reimplementing D3D11 inside NYAMIST.
- Replacing or patching the Mali kernel driver.
- Replacing Android's system Vulkan libraries.
- Claiming PanVK or DXVK support based only on stock-driver `vulkaninfo`.
- Enabling DX12 before repeatable D3D11 results.

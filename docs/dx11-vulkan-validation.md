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

This proves physical-device enumeration through the stock Android Vulkan driver in Termux. It does not prove that the same features are exposed by PanVK or that DXVK can create a D3D11 device.

### Separate app-local PanVK RC5 evidence

A separate PanPlay/PanProbe record reports that the app-local Mesa PanVK RC5 build loaded successfully, enumerated one device, identified the G925-family kbase GPU as `gpu_id 0xd8300015`, and passed PanProbe 41/41. This is separate from the stock-driver `vulkaninfo` capture above. It establishes PanVK load/device-probe success only; it does not establish Android surface/swapchain presentation, DXVK D3D11 device creation, or sustained real-game rendering.

The 10.97 GiB heap is not free memory and is not a per-game allocation guarantee. Since the memory-budget extension was not found in this capture, do not infer available GPU memory from heap size. Use allocation success/failure and process/runtime telemetry where available.

### Nov 8, 2026 — Stranded Deep DX11 real-game session

The user-provided PanPlay SessionLogs and matching PerfRecorder export contain a sustained Stranded Deep session on the REDMI K80 Ultra:

- Session duration: 519 seconds; PerfRecorder duration: 489.130544 seconds.
- DXVK: 3.1.1-5-arm64ec; API: D3D11; selected feature level: D3D_FEATURE_LEVEL_11_0; maximum supported feature level: D3D_FEATURE_LEVEL_11_1. The importer now records these as separate fields so the maximum capability cannot be mistaken for the level actually selected.
- DXGI log selected device: Mali-G925-Immortalis MC12, PanVK string 26.2.99. The separate session/device manifest reports Mesa PanVK 26.3.0-devel (git-795ca7ff3e). Treat this version-string difference as an unresolved provenance/version-reporting discrepancy until the exact ICD binary/build is matched.
- DXVK logged actual swapchain properties: BGRA8 UNORM, SRGB nonlinear color space, IMMEDIATE present mode, 1280x720 buffer, four images.
- PerfRecorder: 41,868 frames; average 85.5968 FPS; low1Fps 4.6416; low01Fps 0.6783; P50 3.867 ms; P95 43.983 ms; P99 63.733 ms; 11,766 stutters.
- Device telemetry: average/max GPU load 72.0%/100%; average/max game CPU 24.2%/41.1%; RAM peak 3,515.8 MiB; video-memory peak 2,208 MiB; minimum available RAM 2,043 MiB; battery temperature average/max 40.1/42.1 C; average/max power 5.13/8.46 W.
- Session ended by user through overlay; crash=false, but exit_code is null. Clean process exit is not confirmed.
- Assessment: sustained duration, frame samples, P95, and device telemetry pass; process exit remains incomplete. Vulkan Present success/failure counters are not exported.

Interpretation: the DXVK log proves D3D11 device creation and swapchain setup, not that a frame was successfully presented. The average FPS is not sufficient to describe smoothness: the reported P95/P99 frame times and low-FPS fields show severe frame-pacing concerns that need review against the raw PerfRecorder CSV. Keep render validation at discovered-only and production_ready=false until explicit Present evidence and clean process-exit evidence are available. Do not tune performance overrides from this single run.

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

- Keep the stock vendor driver intact and select PanVK only through the supported app-local/launcher mechanism.
- The recorded RC5 app-local test passed load and device enumeration with PanProbe 41/41.
- Still verify a graphics surface/swapchain render-and-present test in the actual PanPlay path; the PanProbe score alone does not prove presentation.
- Capture device name, driver identity, API version, extensions, feature bits, memory properties, and logs from that same app/container process.

### Gate B — DXVK

- Pin the exact DXVK build and record its required Vulkan extensions/features.
- Confirm the compatibility runtime can launch a known D3D11 test application.
- Confirm DXVK creates a D3D11 device and selects the intended Vulkan physical device/ICD. The Nov 8 Stranded Deep log now provides D3D11 feature-level and actual swapchain-setup evidence.
- Separately confirm successful swapchain presentation, frame output, resource creation, and synchronization; swapchain properties alone are not Present evidence.
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

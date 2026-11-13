# NYAMIST standalone graphics runtime architecture

## Product decision

NYAMIST owns its graphics runtime and its D3D11-to-Vulkan translation. PanPlay, PanVK, and DXVK are not runtime dependencies and must not appear in the active execution path.

This does **not** mean NYAMIST must replace Android's vendor Vulkan driver. On Android, NYAMIST should initially use the system Vulkan loader (`libvulkan.so`) and the installed vendor GPU driver as the hardware backend. NYAMIST owns the API translation, resource/state tracking, command scheduling, cache policy, and runtime lifecycle above that backend.

A custom Vulkan ICD/HAL is a separate driver-replacement project. It is explicitly out of scope for the first standalone runtime milestones.

## Target

- Device: REDMI K80 Ultra
- SoC: MediaTek MT6991
- GPU: Immortalis-G925 MC12
- OS: Android 16 / API 36
- ABI: arm64-v8a
- Development environment: Termux

## Execution model

```text
D3D11 application
      |
      v
Windows API / ABI compatibility host (separate concern)
      |
      v
NYAMIST D3D11 runtime
  - D3D11 device + immediate context
  - resource and view objects
  - pipeline/state translation
  - shader bytecode -> SPIR-V path
  - synchronization and command batching
      |
      v
NYAMIST Vulkan backend
  - instance/device/queue ownership
  - memory/resource allocation
  - command buffers and barriers
  - pipeline and descriptor caches
  - Android surface/swapchain lifecycle
      |
      v
Android Vulkan loader (libvulkan.so)
      |
      v
Installed vendor Vulkan GPU driver
      |
      v
Immortalis-G925 MC12
```

The compatibility host is not a graphics translator. It supplies the Windows process/API environment required by a Windows application. NYAMIST remains the sole D3D11-to-Vulkan translator in this design.

## Module boundaries

| Module | Owns |
|---|---|
| `runtime/host` | Host ABI boundary and runtime startup contract; no graphics translation |
| `runtime/d3d11` | D3D11-facing interfaces, device/context, resources, views, state objects |
| `translator/d3d11` | D3D11 semantic mapping and draw/dispatch translation |
| `translator/shaders` | DXBC/HLSL bytecode handling and SPIR-V generation/validation |
| `backend/vulkan` | Vulkan instance/device/queue, memory, command submission, synchronization |
| `backend/android` | ANativeWindow integration and swapchain lifecycle through Android Vulkan WSI |
| `runtime/cache` | NYAMIST-owned shader/pipeline cache with versioned identity and bounded storage |
| `diagnostics` | Logs, counters, validation and repeatable test evidence |

Existing hardware discovery, profile, telemetry, and A/B code remains control-plane functionality. It must not run in the per-draw-call or Present hot path.

Legacy PanVK/DXVK report parsers may be retained temporarily only as archived evidence readers. They must not be selected, loaded, built, or required by the active runtime.

## Initial implementation contract

1. Use the Android Vulkan loader and the currently installed vendor driver; do not write an ICD/HAL.
2. Create an NYAMIST-owned Vulkan context that enumerates the physical device, selects a graphics queue, creates a logical device, and owns its command pool.
3. Add a deterministic offscreen clear/triangle test and verify pixels by readback.
4. Add Android surface/swapchain presentation and explicit resize/out-of-date recovery tests.
5. Add the first D3D11 vertical slice: device/context, vertex buffer, minimal vertex/pixel shader path, render target, draw, and Present.
6. Expand feature coverage only after the vertical slice passes on the target device.

## Non-goals for the first milestones

- Replacing the Android vendor Vulkan driver.
- A custom Vulkan ICD, Android HAL module, kernel driver, or GPU firmware.
- Depending on PanPlay, PanVK, or DXVK.
- Claiming general D3D11 game compatibility from a synthetic triangle.
- D3D12/VKD3D support before D3D11 is independently validated.

## Readiness gates

- **discovered**: Android loader and physical device are enumerated.
- **device_ready**: NYAMIST creates a logical device and graphics queue.
- **render_smoke_tested**: NYAMIST submits its own command buffer and pixel readback matches expected output.
- **present_smoke_tested**: NYAMIST presents through Android WSI and recovers from resize/out-of-date.
- **d3d11_smoke_tested**: a D3D11 sample uses NYAMIST's own interfaces and produces verified output.
- **sustained_tested**: repeatable application run of at least 300 seconds passes with clean exit and no critical rendering/device-loss failure.

A passing hosted CI build is not proof of device execution. A synthetic sample is not proof of commercial-game compatibility.

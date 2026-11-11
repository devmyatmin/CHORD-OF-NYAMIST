# REDMI K80 Ultra — Vulkan Hardware Validation

Date: 2026-11-08
Validation source: user-run Termux commands on the target phone.
Status: The stock Android Vulkan driver enumerated successfully in Termux. Separately, app-local the selected Android Vulkan driver RC5 loaded and passed external capability probe 41/41. DXVK game rendering, PanPlay surface/swapchain presentation, and sustained game stability remain unvalidated.

## Device identification

- Manufacturer / brand: Xiaomi / Redmi
- Model code: 25060RK16C
- SoC identifier: MediaTek MT6991
- Android: 16 (API 36)
- Kernel: 6.6.118-android15-8-gc44b714366cc
- ABI: arm64-v8a

## Vulkan physical device

Reported by `vulkaninfo --summary`:

- Device: Mali-G925-Immortalis MC12
- Vendor ID: `0x13B5` (ARM)
- Device ID: `0xD8300010`
- Device type: integrated GPU
- Driver ID: `DRIVER_ID_ARM_PROPRIETARY`
- Driver name: Mali-G925-Immortalis MC12
- Driver version: `49.1.0`
- Driver info: `v1.r49p1-03bet0.4505ac1771886c7c25f13653ad4cdff7`
- Vulkan API version: `1.3.278`
- Conformance version: `1.3.8.0`

The Vulkan instance loader reports version `1.4.368`. This is the loader version, not the GPU driver's device API version.

## Selected reported capabilities

The user's filtered `vulkaninfo` output reports:

- `VK_KHR_swapchain`
- `VK_KHR_dynamic_rendering`
- `VK_KHR_synchronization2`
- `VK_KHR_buffer_device_address`
- `VK_KHR_shader_float16_int8`
- `VK_KHR_acceleration_structure`
- `VK_KHR_ray_query`
- `VK_KHR_fragment_shading_rate`
- `VK_EXT_descriptor_indexing`
- `VK_EXT_subgroup_size_control`
- `VK_EXT_shader_tile_image`
- `maxImageDimension2D = 16384`
- `timestampComputeAndGraphics = true`
- `geometryShader = true`
- `tessellationShader = true`
- `shaderFloat16 = true`
- `bufferDeviceAddress = true`
- `synchronization2 = true`
- `dynamicRendering = true`
- `sparseBinding = false`

These are reported capabilities, not proof that every extension or optional feature is enabled by a particular application. The full feature/extension dependency chain must be queried by the actual runtime before use.

## Interpretation and limits

- Termux successfully enumerated the phone's physical GPU through the Android system Vulkan loader.
- The active driver reported by this test is ARM proprietary Mali driver `v1.r49p1`, not the selected Android Vulkan driver.
- The earlier read-only probe saw `/dev/mali0` with read/write permission bits. This does not prove that arbitrary Termux processes can successfully create all required kernel contexts or issue every ioctl.
- `DISPLAY` not being set only caused surface information to be skipped in the Termux CLI context.
- The `large_page_conf` sysfs warning did not prevent physical-device enumeration.
- `sparseBinding = false` is one unsupported optional feature and does not, by itself, establish DXVK incompatibility.
- A separate PanPlay/external capability probe test loaded app-local the selected Android Vulkan driver RC5, enumerated one device, and passed 41/41 external capability probe checks. This is smoke-test evidence; it does not establish successful Android surface/swapchain presentation or sustained rendering.
- No sustained commercial-game D3D11 run through DXVK has been validated on this phone yet.

## Next validation gates

1. Keep the stock vendor Vulkan driver unchanged and retain app-local the selected Android Vulkan driver selection only.
2. In PanPlay, confirm the tested RC5 ICD is the one actually loaded for the session; record its build identity and logs.
3. Validate Android surface/swapchain presentation in the PanPlay path. The existing 41/41 external capability probe result does not cover this gate.
4. Confirm DXVK creates a D3D11 device on that same the selected Android Vulkan driver physical device.
5. Run one repeatable real D3D11 game scene for at least 300 seconds and preserve matching SessionLogs/PerfRecorder exports.
6. Record driver identity, test result, visual issues, logs, and rollback path for each experiment.

## Safety

Do not chmod `/dev/mali0`, replace vendor Vulkan libraries, modify kernel modules, flash a kernel, or globally force a non-stock ICD as part of this validation. Keep all experimental driver selection app-local and reversible.


## Vulkan memory heaps and memory types

The user's Termux capture from `vulkaninfo` reports:

- Memory heap count: 2
- Heap 0: 11,776,921,600 bytes (10.97 GiB), `DEVICE_LOCAL`
- Heap 1: 104,857,600 bytes (100 MiB), `DEVICE_LOCAL`
- Memory type count: 4

Reported memory types:

| Type | Heap | Properties |
|---|---:|---|
| 0 | 0 | `DEVICE_LOCAL`, `HOST_VISIBLE`, `HOST_COHERENT` |
| 1 | 0 | `DEVICE_LOCAL`, `HOST_VISIBLE`, `HOST_CACHED` |
| 2 | 0 | `DEVICE_LOCAL`, `LAZILY_ALLOCATED` |
| 3 | 1 | `DEVICE_LOCAL`, `PROTECTED` |

Interpretation:

- The reported heaps and types are Vulkan driver's memory model, not a promise that one game can allocate the full 10.97 GiB. On a mobile unified-memory device, heap size must not be treated as currently free memory or as a per-process allocation budget.
- Type 0 is host-visible and coherent; type 1 is host-visible and cached. The runtime must still select a compatible memory type for each resource and honor mapping/synchronization requirements.
- Type 2 is lazily allocated and is relevant only for resources and render-pass usage that support it; it is not a general-purpose upload heap.
- Type 3 is protected memory and should not be treated as ordinary CPU-mappable storage.
- NYAMIST should keep its own CPU-side profiling/cache footprint bounded and avoid reserving large GPU allocations merely because the reported heap is large.

Additional filtered limits captured in the same session:

- `maxStorageBufferRange = 2147483648`
- `maxPushConstantsSize = 256`
- `maxBoundDescriptorSets = 7`
- `maxComputeWorkGroupInvocations = 1024`
- `timestampPeriod = 76.9231` ns
- `subgroupSize = 16`
- One visible queue-family excerpt reported `queueCount = 2` and Graphics, Compute, Transfer, and Protected queue flags. The filtered excerpt is not sufficient to claim that all queue families were captured.

These values are driver-reported limits and properties; actual runtime use still requires querying the device and checking allocation results.

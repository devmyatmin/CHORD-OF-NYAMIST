# REDMI K80 Ultra — Vulkan Hardware Validation

Date: 2026-11-08
Validation source: user-run Termux commands on the target phone.
Status: Vulkan physical-device enumeration passed; DXVK/game rendering and PanVK remain unvalidated.

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
- The active driver reported by this test is ARM proprietary Mali driver `v1.r49p1`, not PanVK.
- The earlier read-only probe saw `/dev/mali0` with read/write permission bits. This does not prove that arbitrary Termux processes can successfully create all required kernel contexts or issue every ioctl.
- `DISPLAY` not being set only caused surface information to be skipped in the Termux CLI context.
- The `large_page_conf` sysfs warning did not prevent physical-device enumeration.
- `sparseBinding = false` is one unsupported optional feature and does not, by itself, establish DXVK incompatibility.
- No DXVK game, swapchain presentation in a game container, PanVK driver, or PanVK kbase backend has been tested on this phone yet.

## Next validation gates

1. Keep the stock vendor Vulkan driver unchanged.
2. Capture the complete `vulkaninfo` output and query device features/extensions in the intended Android game-container process.
3. Build or obtain an isolated PanVK test package for the exact Android ABI/API and G925 kbase path; do not replace system Vulkan libraries.
4. Run PanVK's own device probe first, then a minimal Vulkan rendering/presentation test.
5. Only after PanVK passes should DXVK D3D11 translation be tested with a known-good test application.
6. Record driver identity, test result, logs, and rollback path for each experiment.

## Safety

Do not chmod `/dev/mali0`, replace vendor Vulkan libraries, modify kernel modules, flash a kernel, or globally force a non-stock ICD as part of this validation. Keep all experimental driver selection app-local and reversible.

# NYAMIST G925 R2.6 — uint8 Index Enablement and Indexed-Draw Functional Probe

**Version:** 2.6.0 (versionCode 260)  
**Report schema:** 11  
**Branch:** `feature/g925-r2.6-index-type-uint8-functional-probe`  
**Target profile:** REDMI K80 Ultra / Android 16 / MT6991 / Immortalis-G925 MC12

## Objective

R2.6 advances the R2.5 capability query into a narrowly scoped, functional device-feature test. It enables `VK_KHR_index_type_uint8.indexTypeUint8` only when the selected physical device both advertises the extension (for pre-1.4 devices) and returned the corresponding feature bit as supported in the R2.5 Features2 query.

For Vulkan 1.4 physical devices, the extension name is not required because the functionality is core; the feature bit must still be reported as supported. The target REDMI K80 Ultra exposes physical-device API 1.3.278, so its expected path is the advertised `VK_KHR_index_type_uint8` extension.

## Device enablement

At `vkCreateDevice` time, R2.6:

- adds `VK_KHR_index_type_uint8` to `VkDeviceCreateInfo::ppEnabledExtensionNames` only on the pre-1.4 extension path and only when advertised and feature-supported;
- chains `VkPhysicalDeviceIndexTypeUint8FeaturesKHR` into `VkDeviceCreateInfo::pNext` with `indexTypeUint8 = VK_TRUE`;
- leaves the feature disabled and reports a nonfatal skip when it is not advertised or not supported;
- does not enable other Vulkan 1.4 promoted feature bits.

This follows the Vulkan feature enablement model: query support first, then explicitly enable the supported feature at device creation before using it.

## Functional probe

When the uint8 index feature is enabled, R2.6 creates a three-byte index buffer using `VK_BUFFER_USAGE_INDEX_BUFFER_BIT`, chooses `HOST_VISIBLE|HOST_COHERENT` memory, uploads the index sequence `[0, 1, 2]`, and unmaps the allocation.

On the first/readback frame, the graphics render pass performs an **indexed-only** draw:

- `vkCmdBindIndexBuffer(..., VK_INDEX_TYPE_UINT8_KHR)`
- `vkCmdDrawIndexed(3, 1, 0, 0, 0)`

The normal non-indexed draw is deliberately skipped on that frame, so it cannot mask a failed indexed draw. The existing triangle shader uses `gl_VertexIndex`; the acquired swapchain image is then copied to host-visible readback memory. The probe reports success only when the pixel readback verifies the triangle on the swapchain image. Later timing frames retain the existing non-indexed draw path.

The report records extension advertisement/core status, feature support and enablement, buffer create/allocate/bind/map/upload/unmap stages, entry-point availability, indexed draw recording, readback-confirmed probe status, and buffer/memory cleanup.

## Scope boundaries

- R2.6 exercises **one** promoted feature bit: `indexTypeUint8`. It does not functionally validate all 21 bits inventoried in R2.5.
- The probe is a minimal indexed triangle workload, not a performance benchmark.
- It does not claim Vulkan 1.4 physical-device API support or full Vulkan 1.4 compliance.
- It does not replace or modify the Android Vulkan loader or the ARM vendor driver.
- No root, private GPU device-node access, kernel/SELinux change, vendor-partition write, or custom ICD is used.
- Target-device validation is pending until a schema-v11 report confirms that the R2.6 indexed-only frame passed pixel readback on the REDMI K80 Ultra.

## Hosted validation

**PASS — 2026-11-10.** Core CI passed on R2.6 head `0bb310aa2fa7fe80e031e61fea529e9a04a8d551` (run [42772752854](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42772752854)). The R2.2–R2.6 contract-test suite passed, the arm64 Android APK build passed, and the APK artifact was uploaded by the R2.6 Android workflow (run [42772282941](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42772282941)). The build used pinned Khronos Vulkan-Headers v1.4.368, Android SDK 36 and NDK 28.2.13676358.

This is hosted CI/build evidence only; it does not establish target-device execution.

## Target-device validation

**Pending.** A target-device schema-v11 report must show `index_type_uint8_feature_enable_status=enabled`, successful index-buffer setup, `index_type_uint8_draw_recorded=true`, `index_type_uint8_draw_index_count=3`, `index_type_uint8_probe_status=success`, successful pixel verification, and successful buffer/memory destruction.

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
- Target-device validation: **PASS — 2026-11-10**. The schema-v11 report confirms feature enablement, indexed-only draw execution, acquired-swapchain-image pixel verification, and successful resource cleanup on the REDMI K80 Ultra. Full evidence: [R2.6 REDMI K80 Ultra hardware evidence](../evidence/NYAMIST-G925-R2.6-redmi-k80-ultra-2026-11-10.md).

## Hosted validation

**PASS — 2026-11-10.** Core CI passed on R2.6 head `0bb310aa2fa7fe80e031e61fea529e9a04a8d551` (run [42772752854](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42772752854)). The R2.2–R2.6 contract-test suite passed, the arm64 Android APK build passed, and the APK artifact was uploaded by the R2.6 Android workflow (run [42772282941](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42772282941)). The build used pinned Khronos Vulkan-Headers v1.4.368, Android SDK 36 and NDK 28.2.13676358.

This paragraph records hosted CI/build evidence only. Target-device execution is independently established by the schema-v11 hardware report summarized below.

## Target-device validation

**PASS — 2026-11-10.** The schema-v11 report from REDMI K80 Ultra / Android 16 / MT6991 / Mali-G925-Immortalis MC12 matched the target GPU and completed the uint8 indexed-draw functional probe successfully.

- Physical-device API: **1.3.278**; Android loader and instance API: 1.4.0. The device does not expose Vulkan 1.4 as its physical-device API.
- `VK_KHR_index_type_uint8`: advertised; `indexTypeUint8`: supported by the R2.5 capability query and enabled successfully at device creation.
- Three-byte index buffer: create, allocate, bind, map, upload `[0, 1, 2]`, and unmap all succeeded.
- Indexed-draw entry points and command recording: success; `index_type_uint8_draw_recorded=true`; index count: **3**.
- Functional probe: **`index_type_uint8_probe_status=success`**.
- Acquired swapchain-image pixel verification: **success**; four corner samples matched `[53, 105, 153, 255]`, and the triangle-center sample was `[200, 180, 180, 255]`.
- Index buffer destruction, index memory free, and overall cleanup: success.
- Repeated manual rebuild observations: 56 manual rebuild requests; 59 native sessions started, 59 completed, 59 successful, zero skipped, zero active at report time, maximum concurrent native sessions **1**. Requests were serialized in this run; this is not a long-duration concurrency/soak test.
- GPU timestamp samples: 16; minimum **35,692 ns**, median **51,807 ns**, p95 / maximum **84,692 ns**. CPU frame-total samples: 16; minimum **438 us**, median **8,114 us**, p95 / maximum **15,337 us**. These short samples are not sustained performance or thermal benchmarks.

See [R2.6 REDMI K80 Ultra hardware evidence](../evidence/NYAMIST-G925-R2.6-redmi-k80-ultra-2026-11-10.md) for the full result and evidence boundaries.

This is a hardware PASS for the R2.6 uint8 index-feature enablement and indexed-draw functional probe only. It does **not** establish physical-device Vulkan 1.4 API support (the device reports Vulkan 1.3.278), full Vulkan 1.4 compliance, or functional use of every promoted feature. Actual swapchain resize / OUT_OF_DATE recovery remains not observed.

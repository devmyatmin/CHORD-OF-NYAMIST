# NYAMIST G925 R2.5 — Vulkan 1.4 Promoted-Feature Capability Audit

**Version:** 2.5.0 (versionCode 250)  
**Report schema:** 10  
**Branch:** `feature/g925-r2.5-promoted-feature-capability-audit`  
**Target profile:** REDMI K80 Ultra / Android 16 / MT6991 / Immortalis-G925 MC12

## Objective

R2.5 advances the R2.4 extension-name inventory to a feature-bit capability audit for the 21 feature members introduced or promoted into `VkPhysicalDeviceVulkan14Features`.

This report describes feature-query evidence. It does not claim Vulkan 1.4 API support, complete Vulkan 1.4 compliance, feature enablement, or functional use of every reported capability.

## Query behavior

### Physical-device API below Vulkan 1.4

For a pre-1.4 physical device, the audit builds a `VkPhysicalDeviceFeatures2` chain from extension-specific feature structures only when the selected physical device advertises the corresponding extension. It queries the chain through the public Vulkan loader/driver dispatch and records each bit as supported or not supported.

The 13 extension feature structures cover 20 explicit feature bits:

- `VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR`
- `VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR`
- `VkPhysicalDeviceIndexTypeUint8FeaturesKHR`
- `VkPhysicalDeviceLineRasterizationFeaturesKHR`
- `VkPhysicalDeviceMaintenance5FeaturesKHR`
- `VkPhysicalDeviceMaintenance6FeaturesKHR`
- `VkPhysicalDeviceShaderExpectAssumeFeaturesKHR`
- `VkPhysicalDeviceShaderFloatControls2FeaturesKHR`
- `VkPhysicalDeviceShaderSubgroupRotateFeaturesKHR`
- `VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR`
- `VkPhysicalDeviceHostImageCopyFeaturesEXT`
- `VkPhysicalDevicePipelineProtectedAccessFeaturesEXT`
- `VkPhysicalDevicePipelineRobustnessFeaturesEXT`

The remaining 21st bit, `pushDescriptor`, was not represented by a feature bit in the pre-1.4 `VK_KHR_push_descriptor` extension. When that extension is advertised, R2.5 records the capability as **implicitly supported by the extension**, rather than incorrectly reporting a false/default value or calling it unsupported. When the extension is absent, it is recorded as not advertised.

`VK_KHR_load_store_op_none` and `VK_KHR_map_memory2` do not contribute feature bits to the 21-bit inventory; they are listed as not applicable to this feature-bit query.

### Physical-device API Vulkan 1.4 or later

The Vulkan 1.4 aggregate feature structure is queried only when both the physical-device API version and the negotiated instance API version are at least 1.4. Otherwise, the aggregate query is skipped and the feature bits are classified as not queryable for that path.

### Status categories

The JSON report keeps separate collections and counts for:

- queried and supported;
- queried and not supported;
- extension not advertised;
- implicitly supported by an advertised extension;
- query failed / entry point unavailable;
- not queryable on the selected API path;
- not applicable because an extension has no feature bit.

The `queried_count` counts explicit feature-bit queries. `implicitly_supported_count` is reported separately and is not folded into the explicit query count.

## Scope and limitations

- R2.5 is a query-only capability audit. It does not enable new device extensions or feature bits in `VkDeviceCreateInfo`, and it does not run feature-specific rendering or compute workloads.
- The Vulkan 1.4 promoted-properties inventory is not part of R2.5. In particular, the additional properties that qualify full dynamic-rendering-local-read coverage are not audited here.
- `pipelineProtectedAccess` is reported as a capability bit; the conditional requirement tied to `protectedMemory` is not a Vulkan 1.4 compliance determination in this milestone.
- Extension-name advertisement and feature-bit support do not establish that a feature was enabled or functionally exercised.

## Hosted validation

**Core CI: PASS** — workflow run [42765536179](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42765536179), commit `09c89ac04956d8d61862c099c754cf385c0bddb7`. Python importer tests: 71 passed. CTest: 15/15 passed in both standard and runtime-dispatch configurations; report smoke tests, runtime dispatch checks, and CLI/cache smoke tests passed.

**Android arm64 build: PASS** — workflow run [42765535962](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42765535962), same commit. Contract tests passed; Android APK built with pinned Khronos Vulkan-Headers v1.4.368, Android SDK 36 and NDK 28.2.13676358.

## Target-device validation

**Pending.** The R2.5 APK must be installed and run on the target REDMI K80 Ultra / Android 16 / MT6991 / Immortalis-G925 MC12. Hardware status will be updated only from the generated schema-v10 report. The prior R2.4 report is not treated as R2.5 feature-query evidence.

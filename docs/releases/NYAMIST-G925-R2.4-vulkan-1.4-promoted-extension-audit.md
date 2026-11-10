# NYAMIST G925 R2.4 — Vulkan 1.4 Promoted-Extension Inventory

## Objective

Inventory the 16 extension families promoted into Vulkan 1.4 on the selected Android physical device, especially when the physical-device API remains Vulkan 1.3.

## Implementation

- Enumerate `vkEnumerateDeviceExtensionProperties` for the exact physical device selected for the Android Surface graphics/present queue.
- Compare its advertised extension names against the Khronos Vulkan 1.4 promotion set:
  - `VK_KHR_dynamic_rendering_local_read`
  - `VK_KHR_global_priority`
  - `VK_KHR_index_type_uint8`
  - `VK_KHR_line_rasterization`
  - `VK_KHR_load_store_op_none`
  - `VK_KHR_maintenance5`
  - `VK_KHR_maintenance6`
  - `VK_KHR_map_memory2`
  - `VK_KHR_push_descriptor`
  - `VK_KHR_shader_expect_assume`
  - `VK_KHR_shader_float_controls2`
  - `VK_KHR_shader_subgroup_rotate`
  - `VK_KHR_vertex_attribute_divisor`
  - `VK_EXT_host_image_copy`
  - `VK_EXT_pipeline_protected_access`
  - `VK_EXT_pipeline_robustness`
- Serialize separate arrays for advertised and not-advertised extension names, plus total/supported/not-advertised counts.
- For physical devices exposing Vulkan 1.4 or newer, classify promoted functionality as core API instead of treating absent extension aliases as unsupported.
- Preserve R2.3 loader/instance/physical-device version separation, driver identity, Vulkan 1.4 feature-query rules, WSI rendering, readback, timestamps, and present-fence validation.
- Report wording limits this phase to extension-name advertisement. No full Vulkan 1.4 feature/limit compliance or CTS certification is implied.

## Hosted validation — PASS

Validated on head `32174daba4be00b408994c77dd6771751af6cf31`:

- Core CI push #42759560985: success; CTest 15/15, runtime Vulkan-dispatch build/test, and smoke checks passed.
- R2.4 Android push #42759561206: success; compatibility and R2.4 inventory contract tests passed; arm64 APK built and uploaded.
- R2.4 Android PR #42759567166: success; contract tests passed; arm64 APK built and uploaded.
- Artifact `nyamist-g925-r2.4-vulkan-1.4-audit-apk`, ID `13736912937`.
- Artifact ZIP SHA-256: `6d165b014926dc0de7b91117b4303f7ad5638236d887a39d6002a5293a6c5db9`.

## Hardware validation — PENDING

A fresh R2.4 APK installation and schema-v9 JSON report from the REDMI K80 Ultra are still required.

## Expected hardware interpretation

The previously observed stock G925 driver exposed physical-device API 1.3.278. R2.4 will enumerate extension names for that API-1.3 physical device. The results establish which promoted extension names the installed driver advertises; they will not upgrade or relabel the physical-device API version.

No root, direct GPU device-node access, kernel/SELinux modification, vendor-partition write, or replacement ICD is used.

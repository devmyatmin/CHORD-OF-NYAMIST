# NYAMIST G925 R2.4 — REDMI K80 Ultra Hardware Evidence

**Validation date:** 2026-11-10  
**Device:** REDMI K80 Ultra · Android 16 · MediaTek MT6991 · Mali-G925-Immortalis MC12  
**Application:** NYAMIST G925 R2.4 · versionCode 240 · versionName 2.4.0  
**Report schema:** 9  
**Overall report status:** `success`  
**Target GPU match:** `true`  
**Milestone:** Vulkan 1.4 promoted-extension-name inventory — target-device hardware PASS

## API and driver identity

| Layer / property | Observed value |
|---|---|
| Android public Vulkan loader | 1.4.0 |
| Created Vulkan instance | 1.4.0 |
| Selected physical-device API | **1.3.278** |
| Vendor / device ID | `0x13b5 / 0xd8300010` |
| Driver ID | 9 — `VK_DRIVER_ID_ARM_PROPRIETARY` |
| Driver name | Mali-G925-Immortalis MC12 |
| Driver info | `v1.r49p1-03bet0.4505ac1771886c7c25f13653ad4cdff7` |
| Raw driver version | `205524992 / 0x0c401000` |
| Driver conformance version | 1.3.8.0 |

The loader and instance expose API 1.4.0, but the selected physical device exposes API 1.3.278. The report correctly records `vulkan_1_4_support_status=not_exposed_by_physical_device` and `vulkan_1_4_features_query_status=skipped_physical_device_api_below_1_4`.

The Vulkan 1.4 feature query was skipped. Its false/default feature fields are **not** individual feature-query results and must not be interpreted as tested feature failures.

## R2.4 promoted-extension-name inventory

- Inventory status: `success`
- Inventory scope: `extension_name_advertisement_on_pre_1_4_device`
- Coverage: `partial_advertisement`
- Total names checked: **16**
- Advertised by selected physical device: **15**
- Not advertised: **1**

### Advertised names (15)

1. `VK_KHR_dynamic_rendering_local_read`
2. `VK_KHR_global_priority`
3. `VK_KHR_index_type_uint8`
4. `VK_KHR_line_rasterization`
5. `VK_KHR_load_store_op_none`
6. `VK_KHR_maintenance5`
7. `VK_KHR_maintenance6`
8. `VK_KHR_map_memory2`
9. `VK_KHR_push_descriptor`
10. `VK_KHR_shader_expect_assume`
11. `VK_KHR_shader_float_controls2`
12. `VK_KHR_shader_subgroup_rotate`
13. `VK_KHR_vertex_attribute_divisor`
14. `VK_EXT_pipeline_protected_access`
15. `VK_EXT_pipeline_robustness`

### Not advertised (1)

- `VK_EXT_host_image_copy`

This phase enumerates extension **names only**. An advertised name does not establish that its feature bits were queried, enabled on a logical device, or exercised successfully. In particular, the `VK_EXT_pipeline_protected_access` name alone does not prove protected-memory functionality.

Khronos specifies that `VK_EXT_host_image_copy` is promoted to Vulkan 1.4 but its functionality is optional in Vulkan 1.4. Its absence from this API-1.3 device's extension-name list is therefore an inventory observation, not a Vulkan 1.4 compliance failure. Vulkan 1.4 implementations must provide the host-image-copy feature or an additional transfer-capable queue as specified by Khronos:
- [Khronos Vulkan 1.4 feature proposal](https://docs.vulkan.org/features/latest/features/proposals/VK_VERSION_1_4.html)
- [Khronos VK_EXT_host_image_copy reference](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_host_image_copy.html)

## Rendering and WSI evidence retained

- Graphics/present queue family: 0; both graphics and present supported.
- Swapchain: success; six images; FIFO present mode; selected format `R8G8B8A8_SRGB / SRGB_NONLINEAR`.
- Actual acquired swapchain-image readback: success.
- Four background corner samples matched `[53,105,153,255]`.
- Triangle-center sample was `[200,180,180,255]`; triangle detected.
- Graphics pipeline and three-vertex draw recorded; present succeeded.
- Present-fence attaches: 17; waits: 17; resets: 16; shutdown fence signaled.
- Cleanup and resource destruction: success.
- Surface lifecycle: six native sessions started/completed, all six succeeded; maximum concurrent sessions was one.

## Timing sample

Sixteen GPU timestamp samples were valid:
- GPU frame-time minimum: 23,385 ns
- GPU frame-time median: 39,500 ns
- GPU frame-time p95 / maximum: 76,231 ns

Sixteen CPU frame-total samples were valid:
- CPU frame-total minimum: 622 us
- CPU frame-total median: 7,429 us
- CPU frame-total p95 / maximum: 15,867 us

These are short validation samples, not FPS, sustained-throughput, thermal, or battery benchmarks.

## Swapchain recovery evidence boundary

Same-device `oldSwapchain` handoff and replacement image enumeration succeeded. The replacement probe acquire, submit, present, and present-fence completion all succeeded.

The extent remained 1202 x 715 before and after. SUBOPTIMAL and OUT_OF_DATE counters remained zero; `swapchain_out_of_date_recovery_status=not_observed`. This proves the handoff path, not an actual surface resize or an observed OUT_OF_DATE recovery event. The replacement probe did not render scene content; the primary rendering sequence separately passed triangle pixel readback.

## Conclusion

R2.4 achieved a target-device hardware PASS for the 16-name Vulkan 1.4 promoted-extension inventory: **15 names advertised, one not advertised**. The physical GPU API remains Vulkan 1.3.278; no Vulkan 1.4 physical-device support claim is made.

This evidence intentionally omits the physical-device UUID. It uses the Android public Vulkan loader and stock vendor driver only; no root, private GPU device-node access, kernel/SELinux changes, vendor-partition writes, or replacement ICD are used.

# NYAMIST G925 R2.5 — REDMI K80 Ultra Hardware Evidence

**Validation date:** 2026-11-10  
**Device:** REDMI K80 Ultra · Android 16 · MediaTek MT6991 · Mali-G925-Immortalis MC12  
**Application:** NYAMIST G925 R2.5 · versionCode 250 · versionName 2.5.0  
**Report schema:** 10  
**Overall report status:** `success`  
**Target GPU match:** `true`  
**Milestone:** Vulkan 1.4 promoted-feature capability query — target-device hardware PASS

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

The public loader and instance expose API 1.4.0, while the selected physical device exposes API 1.3.278. Accordingly, the Vulkan 1.4 core feature query was correctly skipped with `vulkan_1_4_features_query_status=skipped_physical_device_api_below_1_4`. The schema-v10 promoted-feature query separately succeeded through the advertised extension-specific feature structures.

The report contained a physical-device UUID. It is intentionally omitted from this repository evidence because it is a device-specific identifier.

## R2.5 promoted-feature capability results

- Query status: **`success`**
- Query scope: `advertised_extension_feature_structs_on_pre_1_4_device`
- Total promoted feature bits in the audit: **21**
- Explicitly queried: **19**
- Queried and supported: **14**
- Queried and not supported: **5**
- Not advertised: **1**
- Implicitly supported by an advertised extension: **1**
- Query failed: **0**
- Not queryable: **0**
- Not applicable (promoted extension has no feature bit): **2**, excluded from the 21-bit total

The accounting is consistent: 19 explicit query results (14 supported + 5 not supported), one not-advertised bit, and one implicitly supported bit = 21 total feature bits. The two not-applicable entries are extension promotions without feature bits and are not part of that total.

### Supported (14)

1. `VK_KHR_dynamic_rendering_local_read.dynamicRenderingLocalRead`
2. `VK_KHR_global_priority.globalPriorityQuery`
3. `VK_KHR_index_type_uint8.indexTypeUint8`
4. `VK_KHR_line_rasterization.rectangularLines`
5. `VK_KHR_line_rasterization.bresenhamLines`
6. `VK_KHR_maintenance5.maintenance5`
7. `VK_KHR_maintenance6.maintenance6`
8. `VK_KHR_shader_expect_assume.shaderExpectAssume`
9. `VK_KHR_shader_float_controls2.shaderFloatControls2`
10. `VK_KHR_shader_subgroup_rotate.shaderSubgroupRotate`
11. `VK_KHR_shader_subgroup_rotate.shaderSubgroupRotateClustered`
12. `VK_KHR_vertex_attribute_divisor.vertexAttributeInstanceRateDivisor`
13. `VK_EXT_pipeline_protected_access.pipelineProtectedAccess`
14. `VK_EXT_pipeline_robustness.pipelineRobustness`

### Queried but not supported (5)

1. `VK_KHR_line_rasterization.smoothLines`
2. `VK_KHR_line_rasterization.stippledRectangularLines`
3. `VK_KHR_line_rasterization.stippledBresenhamLines`
4. `VK_KHR_line_rasterization.stippledSmoothLines`
5. `VK_KHR_vertex_attribute_divisor.vertexAttributeInstanceRateZeroDivisor`

### Not advertised (1)

- `VK_EXT_host_image_copy.hostImageCopy`

The device did not advertise `VK_EXT_host_image_copy`, so its feature structure was not chained or queried. Vulkan 1.4 makes this extension optional; this observation on a pre-1.4 physical device is not a Vulkan 1.4 compliance failure. See the [Khronos Vulkan 1.4 reference](https://docs.vulkan.org/refpages/latest/refpages/source/VK_VERSION_1_4.html).

### Implicitly supported (1)

- `VK_KHR_push_descriptor.pushDescriptor` — the advertised pre-1.4 `VK_KHR_push_descriptor` extension predates the explicit Vulkan 1.4 feature bit. It is therefore reported separately as implicit extension support, not as a queried false/default bit. Khronos documents that the feature bit was added on promotion to Vulkan 1.4: [VK_VERSION_1_4 differences](https://docs.vulkan.org/refpages/latest/refpages/source/VK_VERSION_1_4.html).

### Not applicable (2; outside the feature-bit total)

- `VK_KHR_load_store_op_none` — no feature bit.
- `VK_KHR_map_memory2` — no feature bit.

No feature query failed, and no queried feature was left in a default/unknown state.

## Rendering, presentation, and lifecycle evidence

The R2.5 run also retained the established rendering and WSI validation path:

- Android surface, native window, Vulkan instance, physical-device selection, swapchain, graphics queue, and present queue: success.
- Swapchain: 1202 × 715, six images, FIFO present mode, `R8G8B8A8_SRGB / SRGB_NONLINEAR`, with `COLOR_ATTACHMENT|TRANSFER_SRC` usage.
- Actual acquired swapchain-image pixel readback: success; all four corner samples matched `[53,105,153,255]`; triangle-center sample was `[200,180,180,255]`.
- Graphics pipeline creation, triangle draw command, queue submission, and presentation: success.
- `VK_EXT_swapchain_maintenance1` present-fence path: enabled; 17 attaches and 17 waits; 16 resets. The final present fence was signaled at shutdown; no reset was required after the final present.
- Surface lifecycle: three native sessions started, three completed, all three succeeded; maximum concurrent native sessions was one.
- Cleanup and resource destruction: success.

## Timing sample

Sixteen GPU timestamp samples were valid:

- GPU frame-time minimum: **62,462 ns**
- GPU frame-time median: **66,115 ns**
- GPU frame-time p95 / maximum: **130,846 ns**

Sixteen CPU frame-total samples were valid:

- CPU frame-total minimum: **484 us**
- CPU frame-total median: **8,148 us**
- CPU frame-total p95 / maximum: **14,241 us**

These are short validation samples, not sustained FPS, throughput, thermal, power, or battery benchmarks. The frame timing includes CPU-side acquire/present and wait behavior and must not be interpreted as a standalone GPU throughput benchmark.

## Swapchain recovery evidence boundary

Same-device `oldSwapchain` handoff and replacement image enumeration succeeded. The replacement probe acquire, submit, present, and present-fence completion succeeded.

The extent remained 1202 × 715 before and after. SUBOPTIMAL and OUT_OF_DATE counters remained zero; `swapchain_out_of_date_recovery_status=not_observed`. This confirms the handoff path but does not establish an actual resize or an observed OUT_OF_DATE recovery event. The replacement probe did not render scene content; the primary rendering sequence separately passed triangle pixel readback.

## Conclusion

R2.5 achieved a **target-device hardware PASS for its 21-bit promoted-feature capability query** on REDMI K80 Ultra / Android 16 / MT6991 / Mali-G925-Immortalis MC12: 19 explicit feature bits queried successfully, 14 supported and five not supported; one additional bit was not advertised, and one was classified as implicitly supported by its advertised extension. Two promoted extensions without feature bits were explicitly marked not applicable.

The physical GPU API remains Vulkan 1.3.278. This milestone does **not** claim Vulkan 1.4 physical-device API support, full Vulkan 1.4 compliance, feature enablement, or functional use of every supported feature. R2.2 actual resize/OUT_OF_DATE recovery remains not observed.

This validation used Android's public `libvulkan.so` loader and the stock ARM vendor driver. No root, private GPU device-node access, kernel/SELinux changes, vendor-partition writes, or replacement ICD were used.

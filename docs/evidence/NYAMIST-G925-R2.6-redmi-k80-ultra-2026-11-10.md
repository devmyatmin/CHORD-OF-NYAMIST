# NYAMIST G925 R2.6 — REDMI K80 Ultra Hardware Evidence

**Validation date:** 2026-11-10  
**Device:** REDMI K80 Ultra · Android 16 · MediaTek MT6991 · Mali-G925-Immortalis MC12  
**Application:** NYAMIST G925 R2.6 · versionCode 260 · versionName 2.6.0  
**Report schema:** 11  
**Overall report status:** `success`  
**Target GPU match:** `true`  
**Milestone:** `VK_KHR_index_type_uint8` enablement and indexed-draw functional probe — target-device hardware PASS

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

The public loader and instance expose API 1.4.0, while the selected physical device exposes API 1.3.278. The Vulkan 1.4 aggregate feature query was therefore correctly skipped with `skipped_physical_device_api_below_1_4`. R2.6 exercised the pre-1.4 extension path for the uint8 index feature.

The report included a physical-device UUID. It is intentionally omitted because it is a device-specific identifier.

## uint8 index feature enablement

The schema-v11 report confirms:

| Stage | Observed result |
|---|---|
| `VK_KHR_index_type_uint8` extension | `advertised` |
| `indexTypeUint8` feature query | `supported` |
| Device feature enablement | `enabled` |
| Index buffer creation | `success` |
| Memory allocation | `success` |
| Memory binding | `success` |
| Host mapping | `success` |
| Index upload | `success` |
| Host unmapping | `success` |
| Indexed-draw entry points | `success` |
| Indexed-draw command recording | `success` |
| Draw recorded | `true` |
| Index count | **3** |
| uint8 functional probe | **`success`** |
| Index buffer destruction | `success` |
| Index memory free | `success` |

The test used the three-byte index sequence `[0, 1, 2]` in a host-visible, host-coherent index buffer and recorded the indexed-only draw using `VK_INDEX_TYPE_UINT8_KHR`. The report's successful indexed probe is paired with readback verification from the acquired swapchain image.

## Pixel readback and presentation

- Swapchain: **1202 × 715**, six images, FIFO present mode, `R8G8B8A8_SRGB / SRGB_NONLINEAR`.
- Readback source: acquired swapchain image; transfer-source usage was supported.
- Readback: 20 bytes covering five RGBA8 samples; copy, map, and unmap all succeeded.
- Four background-corner samples matched the expected clear value: `[53, 105, 153, 255]`.
- Triangle-center sample: **`[200, 180, 180, 255]`**.
- `pixel_background_samples_match_clear=true`, `pixel_triangle_center_detected=true`, and `pixel_verification_status=success`.
- Queue submission, image presentation, and presentation-fence completion succeeded.

This pixel evidence is the functional confirmation that the uint8 indexed-only draw produced the expected triangle on the acquired swapchain image; a normal non-indexed draw did not mask the probe.

## Repeated manual rebuild observations

The app's lifecycle report records repeated manual rebuild requests during this run:

| Lifecycle observation | Result |
|---|---:|
| Manual rebuild requests | 56 |
| Native sessions started | 59 |
| Native sessions completed | 59 |
| Native session successes | 59 |
| Native sessions skipped | 0 |
| Native sessions active at report time | 0 |
| Maximum concurrent native sessions | **1** |
| Surface creations / recreations / destructions | 3 / 2 / 2 |
| Surface resize events | 0 |
| Activity resumes / pauses | 3 / 2 |

All 59 started native sessions completed successfully, with no skipped or active session at report time and maximum concurrency of one. This run therefore shows that the repeated manual rebuild requests were serialized rather than running native sessions concurrently. It is observational lifecycle evidence, not a dedicated long-duration concurrency or soak test.

The surface remained available at report time. No surface resize was recorded.

## Presentation completion and timing sample

- `VK_EXT_swapchain_maintenance1` present-fence path: enabled and used.
- Present-fence attaches / waits / resets: **17 / 17 / 16**. The final present fence was signaled; a reset after the final present was not required.
- GPU timestamp samples: 16; minimum **35,692 ns**, median **51,807 ns**, p95 / maximum **84,692 ns**.
- CPU frame-total samples: 16; minimum **438 us**, median **8,114 us**, p95 / maximum **15,337 us**.
- Cleanup: `success`; index buffer and memory, readback resources, synchronization objects, command resources, graphics resources, swapchain, device, surface, instance, and native window release all succeeded.

These are short validation samples, not sustained FPS, throughput, thermal, power, or battery benchmarks.

## Swapchain recovery evidence boundary

Same-device `oldSwapchain` handoff and replacement image enumeration succeeded. The replacement probe acquire, submit, present, and present-fence completion succeeded.

The extent remained 1202 × 715 before and after. No SUBOPTIMAL or OUT_OF_DATE event was observed; all related counters remained zero and `swapchain_out_of_date_recovery_status=not_observed`. This confirms the handoff path but does not establish an actual resize or an observed OUT_OF_DATE recovery event. The replacement probe did not render scene content; the primary R2.6 sequence separately passed triangle pixel readback.

## Conclusion

R2.6 achieved a **target-device hardware PASS for `VK_KHR_index_type_uint8.indexTypeUint8` enablement and a functional uint8 indexed-draw probe** on REDMI K80 Ultra / Android 16 / MT6991 / Mali-G925-Immortalis MC12. The extension was advertised, the feature was reported supported and enabled at device creation, the three-byte index-buffer setup succeeded, the indexed draw was recorded with index count three, and acquired-swapchain-image pixel readback detected the expected triangle. Resource cleanup succeeded.

This milestone does **not** claim Vulkan 1.4 physical-device API support (the device reports Vulkan 1.3.278), full Vulkan 1.4 compliance, or functional validation of every R2.5 promoted feature. Actual swapchain resize / OUT_OF_DATE recovery remains not observed.

This validation used Android's public `libvulkan.so` loader and the stock ARM vendor driver. No root, private GPU device-node access, kernel/SELinux changes, vendor-partition writes, or replacement ICD were used.

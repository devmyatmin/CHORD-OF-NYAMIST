# NYAMIST G925 R2.3 — Vulkan 1.4 and Driver Identity Audit

**Status:** implementation and hosted validation PASS; REDMI K80 Ultra Vulkan 1.4 audit completed on-device.

## Objective

Determine whether the stock Android Vulkan stack on the REDMI K80 Ultra exposes
Vulkan 1.4 at the physical-device level. Keep loader API, requested instance
API and physical-device API independent.

## Implementation

- Negotiate the instance API up to the maximum reported by the public Android
  loader, capped at Vulkan 1.4.
- Query `VkPhysicalDeviceDriverProperties` through
  `vkGetPhysicalDeviceProperties2` when supported by the physical device API.
- Record `driverID`, `driverName`, `driverInfo`, raw `driverVersion`
  (decimal and hexadecimal), conformance version and physical-device UUID.
- Query `VkPhysicalDeviceVulkan14Features` only when both physical-device
  and instance API versions are at least 1.4.
- Check Bresenham lines, clustered subgroup rotate, push descriptors and the
  conditional protected-pipeline-access feature.
- Preserve the existing G925 WSI triangle, pixel readback, timestamps,
  present-fence and Surface lifecycle validation.

## Interpretation

The authoritative hardware version is `physical_device_api_version`.
A loader or instance API version of 1.4 does not upgrade a physical device
that reports 1.3.

The feature-structure check is a focused audit, not Vulkan CTS, Android VTS or
full Android Baseline profile certification. Full conformance requires the
appropriate test suites and device certification evidence.

## Safety boundary

Use only Android's public Vulkan loader and stock vendor driver. No root,
direct GPU device-node access, SELinux/kernel changes, vendor partition
modification, replacement ICD or custom driver installation.

## Device validation procedure

1. Install the R2.3 APK from the successful Android workflow artifact.
2. Run the full G925 Surface validation session on the REDMI K80 Ultra.
3. Copy the complete JSON report.
4. Confirm loader/instance/device API versions, driver identity, raw driver
   version, conformance version, physical-device UUID and Vulkan 1.4 status.
5. If physical-device API is below 1.4, record that as the result; do not
   infer support from loader API.

## Hosted validation — PASS

- Core CI push #411: PASS
- Core CI PR #412: PASS
- Android arm64 APK push workflow #7: PASS
- Android arm64 APK PR workflow #8: PASS

Both Core CI and Android package workflows completed successfully on commit
`ce363860462a5d19c617029cf9d787a589b16c46`.



## Hardware validation — PASS (2026-11-10)

The user supplied a successful schema-v8 report from the REDMI K80 Ultra
(Mali-G925-Immortalis MC12, vendor `0x13b5`, device `0xd8300010`).

### Vulkan API and driver identity

| Field | Hardware result |
|---|---|
| Public loader API | `1.4.0` |
| Created instance API | `1.4.0` |
| Physical-device API | `1.3.278` |
| Driver ID | `9 / VK_DRIVER_ID_ARM_PROPRIETARY` |
| Driver name | `Mali-G925-Immortalis MC12` |
| Driver build info | `v1.r49p1-03bet0.4505ac1771886c7c25f13653ad4cdff7` |
| Driver version | `205524992 / 0x0c401000` |
| Driver conformance version | `1.3.8.0` |

**Audit conclusion:** `vulkan_1_4_support_status=not_exposed_by_physical_device`.
The physical device reports Vulkan 1.3.278, despite the loader and instance
supporting 1.4.0. The Vulkan 1.4 feature query was correctly skipped because
the physical-device API is below 1.4.

The false-valued Vulkan 1.4 feature fields in this report are default report
values, not evidence that each feature was queried and individually rejected.
The query status explicitly records `skipped_physical_device_api_below_1_4`.

### Retained rendering evidence

- Target GPU match, Vulkan instance/device creation, queue, swapchain and
  cleanup: success.
- Triangle graphics pipeline and `vkCmdDraw(3,1,0,0)`: success.
- Five-point swapchain-image pixel readback: success; four corners matched
  `[53,105,153,255]`, triangle center read `[200,180,180,255]`.
- GPU timestamp query: 16 samples; median `49,115 ns`, p95 `71,385 ns`.
- CPU frame-total timing: median `7,890 us`, p95 `16,105 us`.
- Present fences: 17 attaches, 17 waits, 16 resets; shutdown fence signaled.
- Cleanup: success.

These timing values are a short validation sample, not an FPS, sustained
performance, thermal or battery benchmark.

### R2.2 WSI handoff and event coverage

- Same-device `oldSwapchain` handoff: success; old handle destroyed.
- Replacement swapchain image query: success, six images.
- Replacement probe acquire/submit/present/completion: success.
- Surface extent before/after: `1202 x 715` -> `1202 x 715`; unchanged.
- Actual acquire/present `SUBOPTIMAL` and `OUT_OF_DATE` counters: all zero.
- `swapchain_out_of_date_recovery_status=not_observed`.

Therefore this run proves same-device oldSwapchain replacement, but it does
**not** prove an actual extent-changing resize or real driver-returned
OUT_OF_DATE recovery. The replacement probe is explicitly marked
`swapchain_recreation_probe_rendered=false`; triangle pixel verification
belongs to the primary render sequence.

The Android lifecycle block shows one surface-created event and no pause,
resume, destruction, recreation or resize in this run. A lifecycle stress
test remains separate.

The report's device UUID is deliberately not copied into this public summary.


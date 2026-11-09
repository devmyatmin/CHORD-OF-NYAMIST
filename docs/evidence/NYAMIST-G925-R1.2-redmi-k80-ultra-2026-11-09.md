# R1.2 Target-Device Validation Evidence — 2026-11-09

**Project:** CHORD OF NYAMIST / NYAMIST G925  
**Device:** REDMI K80 Ultra  
**SoC:** MediaTek MT6991 / Dimensity 9400+  
**GPU:** Mali-G925-Immortalis MC12  
**Platform:** Android 16 / arm64-v8a / Termux  
**Driver policy:** Existing stock Android Vulkan loader and vendor driver; no system or driver modification

## Build and test

The R1.2 branch was fetched and checked out in the user's Termux working tree. CMake configured the forced runtime-dispatch build successfully:

- Clang 21.1.8
- `NYAMIST_ENABLE_VULKAN_DISCOVERY=ON`
- `NYAMIST_FORCE_VULKAN_DYNAMIC_LOADING=ON`
- Build: all targets completed successfully
- CTest: **12/12 passed**
- R1.2 probe exit code: **0**

## Live Vulkan result

The user executed `nyamist-cli validate-device` on the reference device. The generated schema-v1 JSON reported:

| Field | Observed result |
|---|---|
| Overall status | `success` |
| Loader library | `libvulkan.so` |
| Loader API | `1.4.0` |
| Physical device | `Mali-G925-Immortalis MC12` |
| Vendor ID | `0x13b5` |
| Device ID | `0xd8300010` |
| Device API | `1.3.278` |
| G925 name match | `true` |
| Graphics queue family | `0` |
| Queue count in family | `2` |
| Queue flags (raw) | `23` |
| `vkCreateDevice` | `VK_SUCCESS` |
| Enabled device extensions | `0` |
| Enabled optional core features | `none` |
| `vkGetDeviceQueue`, index 0 | Non-null queue handle |
| `vkDestroyDevice` | Success |
| Cleanup | Success |

The full user-provided JSON report had `failure_reason: null`. Its SHA-256 was:

```text
73c54f47a392bcca414b97fb5c5701de122474e2fc949f4fce8db91cf0c32fd0
```

## Interpretation

R1.2 is **target-device validated** for the narrow scope of Vulkan logical-device creation, graphics queue selection/retrieval, and logical-device cleanup on the stock G925 driver path.

The reported Termux warning about `/sys/module/mali_kbase/parameters/large_page_conf` did not prevent the probe from completing and was not reported as a Vulkan validation failure. Its source is not attributed by this evidence record.

This result does not prove command-buffer submission, GPU execution workload, surface/swapchain creation, rendering, presentation, DXVK/game compatibility, sustained stability, or FPS improvement. Those remain separate validation gates.

## Next milestone

R1.3 is a separate scope for command-pool/command-buffer creation, empty command-buffer recording, queue submission, fence completion, and resource cleanup. It must retain the same stock-driver-only and process-local constraints.

**Evidence source:** User-provided Termux build output, CTest output, CLI output, JSON report, and SHA-256 on 2026-11-09.

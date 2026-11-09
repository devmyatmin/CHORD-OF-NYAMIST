# NYAMIST G925 R1.2 — Vulkan Logical Device & Queue Validation

**Project:** CHORD OF NYAMIST  
**Target:** Immortalis-G925 MC12 / MediaTek MT6991 / REDMI K80 Ultra  
**Baseline:** Unmodified stock MediaTek/ARM Vulkan loader and driver  
**Status:** Target-device validated on REDMI K80 Ultra / Android 16 / stock G925 Vulkan driver (2026-11-09)

## Purpose

R1.1 proved that the target device can enumerate the stock Vulkan physical device and collect its reported capabilities. R1.2 advances exactly one layer: create a minimal Vulkan logical device for a graphics-capable queue family, retrieve queue index 0, and destroy the logical device cleanly.

This is a process-local runtime smoke test. It does not install, select, replace, or modify a driver. On Android/Termux it loads the public Vulkan loader from the process and uses the stock vendor implementation selected by Android's normal loader configuration.

## Exact scope

1. Create a minimal Vulkan 1.0 instance.
2. Enumerate physical devices and record the selected device's reported identity.
3. Prefer a physical-device name containing G925 and a non-empty graphics-capable queue family.
4. Select the first graphics-capable queue family with queueCount greater than zero.
5. Call vkCreateDevice with one queue at priority 1.0.
6. Enable zero device extensions and no optional core features.
7. Retrieve queue index 0 using vkGetDeviceQueue and require a non-null queue handle.
8. Call vkDestroyDevice, then destroy the Vulkan instance.
9. Emit a separate schema-v1 JSON evidence report with explicit status and cleanup fields.

The target-device acceptance check requires physical_device.target_gpu_name_match=true and a reported device identity consistent with the recorded REDMI K80 Ultra / G925 stock-driver baseline. The name match is a guardrail, not cryptographic hardware identity.

## Deliberate non-goals

R1.2 does **not** submit command buffers, allocate application GPU buffers/images, create an Android surface or swapchain, render, present, run DXVK, launch a game, or measure FPS. Those are later independent gates. A successful vkCreateDevice and vkGetDeviceQueue proves only logical-device creation and queue retrieval.

No Vulkan feature or device extension is enabled. No undocumented kernel interface or GPU register is accessed. The implementation does not open /dev/mali0, change permissions, modify SELinux, flash a kernel, write protected Android/vendor partitions, or replace the stock driver.

## Build and run on the target device

Use the R1.2 branch and the existing Termux C++20 toolchain:

    cmake -S . -B build-termux-r1.2 \
      -DCMAKE_BUILD_TYPE=Release \
      -DNYAMIST_BUILD_TESTS=ON \
      -DNYAMIST_ENABLE_VULKAN_DISCOVERY=ON \
      -DNYAMIST_FORCE_VULKAN_DYNAMIC_LOADING=ON
    cmake --build build-termux-r1.2 --parallel 4
    ctest --test-dir build-termux-r1.2 --output-on-failure
    ./build-termux-r1.2/nyamist-cli validate-device \
      --output nyamist-g925-r1.2-logical-device.json
    python -m json.tool nyamist-g925-r1.2-logical-device.json > /dev/null
    sha256sum nyamist-g925-r1.2-logical-device.json

Keep the JSON report, SHA-256, exact commit, device build, loader/driver versions, and complete command output together.

## Result interpretation

- status=success: logical device was created, a non-null graphics queue was retrieved, and logical-device destruction was confirmed.
- status=failed: Vulkan integration was compiled, but one of the explicit operations failed or could not be cleaned up.
- status=unavailable: the binary was built without Vulkan development/loader integration.
- target_gpu_name_match=true: the reported physical-device name contains G925; still verify vendor/device IDs against the device baseline.

Inspect these fields separately: physical_device.query_status, queue_selection.status, logical_device.create_status, queue_retrieval.status, and cleanup.status. Do not infer a successful queue submission from a non-null queue handle.

## CI policy

Hosted CI builds and tests both the standard Vulkan linkage and the forced runtime-dispatch variant. The live smoke command emits a machine-readable report. A host runner without a usable Vulkan physical device is allowed to report failed or unavailable; the CI contract checks report structure and bounded status rather than pretending that hosted CI is the target G925 device.

## Next gate

After the target-device R1.2 report confirms device creation, queue retrieval, and cleanup on the stock G925 driver, a separate R1.3 milestone may test command-pool creation, command-buffer recording, queue submission, fence completion, and device-loss handling. Surface/swapchain rendering and DXVK remain separate later gates.

**CHORD OF NYAMIST — Every Signal Has a Story.**


## Confirmed target-device result — 2026-11-09

The reference REDMI K80 Ultra / MT6991 / Mali-G925-Immortalis MC12 was tested from Termux using the forced runtime-dispatch build.

- Clang 21.1.8 build: passed.
- CTest: 12/12 passed.
- Loader: `libvulkan.so`, API `1.4.0`.
- Physical device: Mali-G925-Immortalis MC12; vendor `0x13b5`; device `0xd8300010`; API `1.3.278`.
- Graphics queue family 0; queue count 2; raw queue flags 23.
- `vkCreateDevice`: `VK_SUCCESS`.
- `vkGetDeviceQueue` queue index 0: non-null queue handle.
- `vkDestroyDevice` and cleanup: success.
- CLI exit code: 0.
- Evidence JSON SHA-256: `73c54f47a392bcca414b97fb5c5701de122474e2fc949f4fce8db91cf0c32fd0`.

Detailed evidence is recorded in [R1.2 REDMI K80 Ultra device evidence](../evidence/NYAMIST-G925-R1.2-redmi-k80-ultra-2026-11-09.md).

R1.2 is closed for its defined logical-device, graphics-queue retrieval, and cleanup scope. This is not evidence of command submission, rendering, presentation, DXVK/game execution, sustained stability, or FPS improvement.

# NYAMIST G925 R1.3 — Vulkan Command Submission & Fence Validation

**Project:** CHORD OF NYAMIST  
**Target:** Immortalis-G925 MC12 / MediaTek MT6991 / REDMI K80 Ultra  
**Baseline:** Android public Vulkan loader and unmodified stock vendor driver  
**Status:** Implementation and CI validation pending; target-device run required

## Purpose

R1.2 proved that the target can create a Vulkan logical device, retrieve a non-null graphics queue, and destroy the device. R1.3 advances to the next gate: record a minimal primary command buffer, submit it to that queue, and prove completion using a Vulkan fence.

## Exact scope

1. Load the public Vulkan loader in-process and create a minimal Vulkan 1.0 instance.
2. Select a physical device with a non-empty graphics-capable queue family, preferring a name containing G925.
3. Create a logical device with one queue, zero enabled device extensions, and no optional core features.
4. Retrieve queue index 0.
5. Create a transient command pool and allocate one primary command buffer.
6. Begin and end the command buffer with one-time-submit usage. The command buffer intentionally contains no rendering or resource commands.
7. Create an unsignaled fence and submit the command buffer with vkQueueSubmit.
8. Wait up to 10 seconds with vkWaitForFences, then query vkGetFenceStatus.
9. Track VK_ERROR_DEVICE_LOST explicitly and use vkDeviceWaitIdle as a cleanup safeguard when a submitted operation has not been confirmed complete.
10. Destroy the fence, free the command buffer, destroy the command pool, destroy the logical device, and destroy the instance.
11. Emit schema-v1 JSON with each operation, fence state, device-loss state, and cleanup result separated.

## Deliberate non-goals

R1.3 does **not** create a surface or swapchain, allocate application textures/buffers, execute shaders, render a frame, present, run DXVK, launch a Windows game, or measure FPS. An empty command buffer tests the Vulkan queue submission and synchronization path only.

No root access, driver replacement, kernel modification, device-node access, SELinux changes, undocumented register access, or protected partition writes are used. All work is process-local through Android's public loader and stock driver selection.

## Build and run on REDMI K80 Ultra

From the R1.3 feature branch in Termux:

    cmake -S . -B build-termux-r1.3 \
      -DCMAKE_BUILD_TYPE=Release \
      -DNYAMIST_BUILD_TESTS=ON \
      -DNYAMIST_ENABLE_VULKAN_DISCOVERY=ON \
      -DNYAMIST_FORCE_VULKAN_DYNAMIC_LOADING=ON
    cmake --build build-termux-r1.3 --parallel 4
    ctest --test-dir build-termux-r1.3 --output-on-failure
    ./build-termux-r1.3/nyamist-cli validate-submit \
      --output nyamist-g925-r1.3-command-submission.json
    python -m json.tool nyamist-g925-r1.3-command-submission.json
    sha256sum nyamist-g925-r1.3-command-submission.json

Keep the report, SHA-256, exact commit SHA, device build fingerprint, Android version, loader/driver version, and full terminal output together.

## Target-device acceptance

A target-device pass requires all of the following:

- Physical device query succeeds and the name matches the G925 target; verify vendor/device IDs against the R1.2 stock-driver baseline.
- Logical-device creation succeeds.
- The graphics queue handle is non-null.
- Command pool creation, command-buffer allocation, and begin/end recording succeed.
- vkQueueSubmit returns VK_SUCCESS.
- vkWaitForFences returns VK_SUCCESS within the 10-second bound.
- vkGetFenceStatus returns VK_SUCCESS and fence_signaled=true.
- No device loss is reported.
- Fence, command buffer, command pool, logical device, and instance cleanup are all confirmed.

The hosted CI runner may have no usable Vulkan ICD. In that environment, a failed live probe is acceptable only when the JSON report is valid and the report contract test passes. Hosted CI is not G925 hardware evidence.

## Result interpretation

- status=success means the empty command buffer was submitted and the fence was confirmed signaled, followed by successful cleanup.
- status=failed means at least one Vulkan operation, synchronization check, or cleanup requirement did not pass.
- status=unavailable means the binary was built without Vulkan integration.
- VK_ERROR_DEVICE_LOST is recorded separately and is a hard failure.
- A timed-out fence is a failure even if later cleanup idles the device; it is not silently promoted to success.

## Next gate

After R1.3 passes on the target device, R1.4 can investigate Android surface/swapchain creation and a minimal clear/present loop. Rendering, DXVK compatibility, sustained game sessions, and FPS are later gates and must not be inferred from R1.3.

**CHORD OF NYAMIST — Every Signal Has a Story.**

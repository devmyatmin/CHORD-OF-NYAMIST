# NYAMIST G925 R1.3 — Vulkan Queue Submission & Fence Validation

**Project:** CHORD OF NYAMIST  
**Target:** Immortalis-G925 MC12 / MediaTek MT6991 / REDMI K80 Ultra  
**Baseline:** Stock Android Vulkan loader and MediaTek/ARM vendor driver  
**Status:** Implementation and host-CI validation in progress; target-device run required

## Purpose

R1.2 established successful logical-device creation, graphics queue retrieval, and cleanup on the REDMI K80 Ultra stock G925 driver path. R1.3 advances the runtime lifecycle by recording a minimal primary command buffer, submitting it to the selected graphics queue, waiting for a fence with a bounded timeout, and cleaning up the command resources and device.

## Exact scope

1. Repeat the R1.2 physical-device selection, queue selection, and logical-device creation.
2. Create a transient command pool for the selected graphics queue family.
3. Allocate one primary command buffer.
4. Begin the command buffer with one-time-submit usage and end it without recording draw, dispatch, copy, or barrier commands.
5. Create a fence and submit the empty command buffer with `vkQueueSubmit`.
6. Wait for the fence with a fixed 5-second timeout using `vkWaitForFences`.
7. Destroy the fence, command pool, logical device, and instance.
8. Emit schema-v1 JSON containing both the nested R1.2 device/queue lifecycle and R1.3 submission/fence statuses.

The empty command buffer deliberately avoids creating buffers, images, shaders, pipelines, descriptors, or a swapchain. It validates API object creation, command-buffer recording lifecycle, queue-submit acceptance, fence completion, and cleanup—not useful rendering or a measurable GPU compute workload.

## Acceptance criteria

A target-device R1.3 result is accepted only when all of these are true:

- Target G925 device is identified consistently with the R1.1/R1.2 stock-driver evidence.
- Logical device is created and graphics queue index 0 is non-null.
- Command pool is created and one primary command buffer is allocated.
- Command buffer begin/end both succeed.
- Fence creation and `vkQueueSubmit` succeed.
- `vkWaitForFences` returns `VK_SUCCESS` before the 5-second timeout.
- Fence and command pool are destroyed, followed by logical-device and instance cleanup.
- The output JSON is valid and its SHA-256 is retained.

A timeout, device loss, missing entry point, submit error, or incomplete cleanup must remain an explicit failure; it must not be converted to success.

## Build and run on REDMI K80 Ultra

    cmake -S . -B build-termux-r1.3 \
      -DCMAKE_BUILD_TYPE=Release \
      -DNYAMIST_BUILD_TESTS=ON \
      -DNYAMIST_ENABLE_VULKAN_DISCOVERY=ON \
      -DNYAMIST_FORCE_VULKAN_DYNAMIC_LOADING=ON
    cmake --build build-termux-r1.3 --parallel 4
    ctest --test-dir build-termux-r1.3 --output-on-failure
    ./build-termux-r1.3/nyamist-cli validate-submit \
      --output nyamist-g925-r1.3-queue-submission.json
    python -m json.tool nyamist-g925-r1.3-queue-submission.json > /dev/null
    sha256sum nyamist-g925-r1.3-queue-submission.json

Keep the complete CLI output, JSON report, SHA-256, branch commit, Android build, loader, and driver versions together.

## Safety and limits

- Stock Android Vulkan loader/vendor driver only; process-local use.
- No root, system modification, SELinux change, protected partition write, device-node access, kernel change, or driver replacement.
- No device extensions or optional core features are enabled.
- No command buffer contains GPU draw/dispatch/copy work.
- No surface, swapchain, render/present, DXVK, game launch, performance benchmark, or FPS claim is part of R1.3.

The `large_page_conf` message observed during R1.2 is not treated as a Vulkan failure unless the R1.3 probe itself reports a failed Vulkan operation. Its source must not be inferred without separate evidence.

## Next gate

After R1.3 target-device submission and fence completion are validated, R1.4 can define an offscreen resource and minimal deterministic GPU workload test. Android surface/swapchain presentation remains a separate gate, and DXVK/game execution comes only after the relevant rendering path is proven.

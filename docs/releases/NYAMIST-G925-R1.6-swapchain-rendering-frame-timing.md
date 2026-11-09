# NYAMIST G925 R1.6 — Swapchain Rendering & Frame Timing Validation

## Objective

Advance the target-device Vulkan path from R1.5 image acquisition/presentation
to a real swapchain image write on REDMI K80 Ultra / MT6991 /
Immortalis-G925 MC12, while collecting correctly labeled CPU frame-path timing.

## Implementation

- Retains the Android SurfaceView, JNI ANativeWindow, dynamically loaded
  public libvulkan.so, Android surface extensions and VK_KHR_swapchain.
- Requires swapchain VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT.
- Creates one swapchain image view, a single-color render pass, and a framebuffer.
- Uses attachment loadOp = CLEAR and storeOp = STORE.
  The clear color is RGBA (0.035, 0.14, 0.32, 1.0).
- The render pass transitions the acquired image from UNDEFINED to
  COLOR_ATTACHMENT_OPTIMAL and then to PRESENT_SRC_KHR.
- The queue submission waits on the acquire semaphore at
  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, executes the render-pass
  clear, signals a separate present semaphore, and presentation waits on it.
- Queue idle is confirmed before reporting the frame write complete and
  destroying the framebuffer, render pass, image view, command resources,
  semaphores, swapchain, device, surface, instance and native window.
- JSON schema v2 records render object creation/cleanup, render-pass recording,
  frame_content_written, and CPU-observed durations for acquire, submit call,
  present call, queue-idle wait and total acquire-to-idle.

## Timing contract

All timings use std::chrono::steady_clock and are reported in microseconds.
They are CPU wall-clock intervals around Vulkan calls/waits. They are not GPU
execution duration, GPU timestamp query results, refresh rate, or FPS. Only one
frame is sampled in R1.6. gpu_timestamp_status is not_queried.

## Acceptance criteria

- Target physical device is identified as Immortalis-G925.
- Android surface, FIFO swapchain, image acquisition and synchronization
  complete successfully.
- Image view, render pass and framebuffer creation succeed.
- Render-pass begin/end are recorded and the clear operation is submitted.
- Queue submission and presentation succeed; queue idle confirms completion.
- frame_content_written and frame_timing_valid are true.
- All Vulkan objects and the native window are cleaned up successfully.
- No claim of shader execution, geometry rendering, sustained FPS, or game
  compatibility is made.

## Evidence policy

The hosted GitHub Actions workflow is a compile/package check only. R1.6 is
not hardware-passed until the APK is run on the REDMI K80 Ultra and its full
schema-v2 JSON report is reviewed. Preserve the report and its SHA-256 as the
target-device evidence record.

## Deliberate non-claims

The render pass performs a deterministic color clear but does not bind a
graphics pipeline, execute shaders, or draw geometry. GPU timestamps are not
queried. A single blocking CPU timing sample is not a performance benchmark
and cannot establish FPS, sustained throughput, thermal stability, or game
compatibility.

## REDMI K80 Ultra hardware evidence — 2026-11-09

The target-device schema-v2 report is recorded at
docs/hardware-reports/mt6991-redmi-k80-ultra-r1.6-2026-11-09.json.

Observed on Mali-G925-Immortalis MC12 (vendor 0x13b5, device 0xd8300010,
Vulkan 1.3.278): Android surface and FIFO swapchain succeeded; six images were
enumerated; image index 0 was acquired; image view, render pass and framebuffer
creation succeeded; render-pass begin/end were recorded; queue submission,
presentation and queue idle succeeded; frame_content_written was true; all
reported cleanup operations succeeded; failure_reason was empty.

The submitted CPU-observed timings were: acquire 10 µs, queue-submit call
3,280 µs, present call 8,447 µs, queue-idle wait 294 µs, total acquire-to-idle
14,612 µs. This is one sample from std::chrono::steady_clock. It is not GPU
execution timing, a refresh-rate/FPS result, or a sustained performance
benchmark. gpu_timestamp_status remains not_queried.

**R1.6 target-device acceptance: PASS.** The milestone proves a successful
render-pass clear and presentation of the acquired swapchain image on the
target GPU. It does not prove shader execution, geometry rendering, game
compatibility, or sustained frame throughput.

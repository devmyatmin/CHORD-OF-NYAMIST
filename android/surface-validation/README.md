# NYAMIST G925 R1.8 — GPU Timestamp Query & Multi-Frame Validation

This Android application extends the R1.7 shader/pipeline/triangle test into a
repeatable 16-frame validation sequence with Vulkan timestamp queries on the
selected graphics queue.

## Execution model

1. Create the Android Vulkan instance, surface, G925 logical device, graphics /
   present queue, and FIFO swapchain through the stock Android loader.
2. Create one reusable render pass, shader modules, pipeline layout, graphics
   pipeline, command pool, and command buffer.
3. Submit one unmeasured warm-up frame to initialize pipeline/driver state.
4. Submit and present 16 measured frames. Each measured frame reacquires a
   swapchain image, records the same three-vertex triangle draw, submits,
   presents, waits for queue idle, and reuses the command pool only after it is
   no longer pending.
5. Allocate a timestamp query pool with two timestamp queries per measured
   frame. Reset each query pair in its command buffer, write a TOP_OF_PIPE
   timestamp immediately before the render pass, and a BOTTOM_OF_PIPE timestamp
   after the render pass. Read each pair only after queue idle.
6. Convert timestamp ticks to nanoseconds using the physical device's
   timestampPeriod. Apply timestampValidBits wrap masking for queues with fewer
   than 64 valid bits.
7. Record each frame's CPU acquire, queue-submit call, present call, queue-idle
   wait, acquire-to-idle total, plus each GPU timestamp interval.
8. Serialize raw sample arrays and min / median / nearest-rank P95 / max
   summaries. CPU wall-clock values and GPU timestamp values remain separate.
9. Destroy per-image framebuffer and image-view objects after each completed
   frame, then destroy the shared pipeline, shader, query pool, synchronization,
   command, swapchain, and device resources in dependency-safe order.

## Timing definitions

- frame_total_cpu_us_samples: CPU wall-clock from immediately before image
  acquisition to return from queue idle for each measured frame.
- acquire_cpu_us_samples: CPU time spent in vkAcquireNextImageKHR.
- queue_submit_cpu_us_samples: CPU time spent inside vkQueueSubmit.
- present_call_cpu_us_samples: CPU time spent inside vkQueuePresentKHR.
- queue_idle_wait_cpu_us_samples: CPU time spent inside vkQueueWaitIdle.
- gpu_frame_time_ns_samples: GPU timestamp tick interval converted to ns
  between TOP_OF_PIPE before the render pass and BOTTOM_OF_PIPE after it.

A GPU timestamp interval is not a display-refresh measurement and must not be
reported as FPS. The CPU acquire-to-idle interval is not GPU execution time.
The 16 samples are a short validation run, not sustained performance, thermal,
or game-compatibility evidence. Queue idle is deliberately used for deterministic
single-queue resource and semaphore reuse; this is not a throughput-optimized
renderer.

## Build and run

The R1.8 workflow builds an arm64-v8a debug APK and uploads the
nyamist-g925-r1.8-debug-apk artifact.

1. Download the APK from a successful R1.8 Android workflow run.
2. Install and open NYAMIST G925 R1.8 Multi-Frame GPU Timing Test.
3. Keep the app in the foreground until the 1 warm-up + 16 measured frames finish.
4. Tap Copy JSON report and paste the complete JSON into the project chat.

## Hardware acceptance

A full R1.8 pass requires the target G925 identity, shader modules and graphics
pipeline, 17 successful frame submissions/presentations (1 warm-up + 16 samples),
16 valid CPU samples, 16 valid GPU timestamp query samples, successful
presentation/queue-idle operations, and complete Vulkan cleanup.

Hosted CI validates C++ tests, Android native compilation, shader compilation,
and APK packaging only. Actual REDMI K80 Ultra JSON evidence is mandatory for
hardware acceptance.

No root, direct device-node access, SELinux/kernel changes, vendor partition
writes, or driver replacement are used.

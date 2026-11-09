# NYAMIST G925 R1.8 — GPU Timestamp Query & Multi-Frame Validation

**Implementation status: awaiting automated build/test and target-device validation.**

## Objective

Extend the R1.7 shader-module, graphics-pipeline, three-vertex draw, and present
path into a measured multi-frame sequence using the GPU timestamp query
capability confirmed on REDMI K80 Ultra / MT6991 / Immortalis-G925 MC12.

## Execution design

- Reuse one render pass, the R1.7 vertex/fragment shader modules, pipeline
  layout, and graphics pipeline for the whole sequence.
- Submit one warm-up frame without including it in timing statistics.
- Submit and present 16 measured frames. Reacquire a swapchain image for each
  frame and create a framebuffer/image view bound to that acquired image.
- Reuse the command pool only after the preceding frame's queue-idle wait, then
  reset it before recording the next frame.
- Use one timestamp query pool with two timestamp queries per measured frame.
  Record TOP_OF_PIPE immediately before the render pass and BOTTOM_OF_PIPE
  immediately after it. Read results only after the queue-idle completion point.
- Convert ticks to nanoseconds using timestampPeriod and mask counter deltas to
  timestampValidBits when the counter width is less than 64 bits.
- Record CPU acquire, queue-submit call, present call, queue-idle wait, and
  acquire-to-idle samples separately from GPU timestamp intervals.
- Serialize raw sample arrays and min/median/P95/max summaries.

## Timing contract

The CPU total is measured from just before image acquisition until queue idle
returns. It is not GPU execution time. GPU time is the timestamp delta between
TOP_OF_PIPE and BOTTOM_OF_PIPE around the recorded render pass. It is not
display refresh time and must not be converted into FPS.

The warm-up frame is excluded from the 16 timing samples. Queue-idle waits are
intentional for deterministic semaphore and command-pool reuse; this is a
validation harness, not a throughput-optimized renderer. The short 16-frame
sequence does not establish sustained FPS, thermal stability, battery impact,
or game compatibility.

## Report schema

Schema version 4 adds:

- warm-up count, target frame count, completed frame count, and sample count;
- raw CPU arrays for acquire, queue-submit, present, queue-idle and full-frame
  intervals;
- CPU full-frame min/median/P95/max;
- timestamp query-pool creation, query result, and destruction status;
- raw GPU frame-time nanosecond samples and min/median/P95/max.

A hardware pass requires 17 successful frame submissions/presentations (one
warm-up plus 16 measured), 16 valid CPU samples, 16 valid GPU timestamp samples,
successful queue-idle completion, and complete cleanup.

## CI and device-evidence boundary

Core CI validates serialization/tests and the dynamic-dispatch build. Android
CI builds the arm64-v8a APK and packages the shader assets. Neither hosted job
can establish that the REDMI K80 Ultra vendor driver executed the GPU queries.
The target-device JSON report is mandatory before R1.8 can be called a
hardware pass.

No root, direct device-node access, SELinux/kernel changes, vendor partition
writes, or driver replacement are used.

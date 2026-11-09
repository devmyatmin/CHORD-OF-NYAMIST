# NYAMIST G925 R1.8 — GPU Timestamp Query & Multi-Frame Validation

**Implementation and hosted CI: PASS. Target-device hardware validation: PENDING.**

## Objective

Extend the R1.7 shader-module, graphics-pipeline, three-vertex draw, and present
path into a measured multi-frame sequence using GPU timestamp queries on
REDMI K80 Ultra / MT6991 / Immortalis-G925 MC12.

## Execution design

- Reuse one render pass, shader modules, pipeline layout, and graphics pipeline.
- Submit one unmeasured warm-up frame.
- Submit and present 16 measured frames, reacquiring a swapchain image each time.
- Reuse the command pool only after the preceding queue-idle wait; reset it before
  recording the next command buffer.
- Use 32 timestamp queries: TOP_OF_PIPE immediately before each render pass and
  BOTTOM_OF_PIPE immediately after it. Read results after queue-idle completion.
- Convert tick deltas to nanoseconds using timestampPeriod and apply
  timestampValidBits wrap masking when the counter width is below 64 bits.
- Record CPU acquire, queue-submit call, present call, queue-idle wait, and
  acquire-to-idle time separately from GPU timestamp intervals.
- Serialize raw sample arrays and min/median/P95/max summaries.

## Timing contract

CPU acquire-to-idle time is not GPU execution time. GPU time is the timestamp
delta from TOP_OF_PIPE to BOTTOM_OF_PIPE around the render pass. It is not
display refresh time and must not be converted into FPS.

The warm-up is excluded from the 16 samples. Queue-idle waits are intentional
for deterministic semaphore and command-pool reuse; this is a validation harness,
not a throughput-optimized renderer. The short run does not establish sustained
FPS, thermal stability, battery impact, or game compatibility.

## Report schema

Schema version 4 adds warm-up/target/completed counts, raw CPU timing arrays,
CPU full-frame min/median/P95/max, query-pool create/read/destroy statuses,
and raw GPU timestamp-derived nanosecond samples with min/median/P95/max.

## Hosted CI — PASS

Latest tested PR head: 6d3cb47b537ed864dbc1a99f738937f61b207a50.

- NYAMIST Core CI Run #303 (42644880960): SUCCESS.
- R1.8 Android workflow Run #3 (42644881179): SUCCESS.
- R1.8 Android PR workflow Run #4 (42644914757): SUCCESS.
- APK artifact: nyamist-g925-r1.8-debug-apk, ID 13686123977, size 1,203,156 bytes.
- APK ZIP SHA-256: b7ba6d6e235cd9c0dc478dc93bc21551705dc743dbad4b638efc131cdc5a1658.
- Android workflow: https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42644914757
- Core CI: https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42644880960

## Target-device acceptance — PENDING

Hosted CI proves compilation, tests, shader compilation and APK packaging only.
It cannot establish that the REDMI K80 Ultra vendor driver executed the GPU
queries. A schema-v4 report captured on the actual phone is mandatory before
R1.8 can be called a hardware pass.

No root, direct device-node access, SELinux/kernel changes, vendor partition
writes, or driver replacement are used.

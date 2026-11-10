# NYAMIST G925 R1.8 — GPU Timestamp Query & Multi-Frame Validation

**Implementation and hosted CI: PASS. Initial target-device measurement: PASS (pre-semaphore-fix). Corrected-build target-device retest: PENDING.**

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
for deterministic command-pool reuse; presentation wait semaphores are now
associated per swapchain image so they are not reused before that image is
reacquired. This is a validation harness, not a throughput-optimized renderer.
The short run does not establish sustained FPS, thermal stability, battery
impact, or game compatibility.

## Hosted CI — PASS

Semaphore-corrected source commit: `ac529ddaf11465c59a4d69398f473fd3b09fe7d0`.

- NYAMIST Core CI Run #305 (42736979839): SUCCESS.
- R1.8 Android workflow Run #7 (42736980112): SUCCESS.
- R1.8 Android PR workflow Run #8 (42736987213): SUCCESS.
- APK artifact: `nyamist-g925-r1.8-debug-apk`, ID `13727816427`, size 1,204,937 bytes.
- APK ZIP SHA-256: `2ff0629199b38e2bdb6bbdcce5d778033ecd8c9323a94b74d0da1adc393e0160`.
- Extracted APK SHA-256: `db605c12252489cd035903eb01d78dcdba491d3e3eb2cc385441252a4f9206f0`.
- [Android workflow Run #8](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42736987213)
- [Core CI Run #305](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42736979839)

The APK ZIP and contained APK passed integrity checks. The APK contains
`lib/arm64-v8a/libnyamist_surface_validation.so`.

## Initial target-device evidence — PASS (pre-semaphore-fix)

The user's REDMI K80 Ultra / MT6991 / Immortalis-G925 MC12 report was captured
before the per-swapchain-image presentation semaphore correction:

- Schema version 4, status `success`, target GPU match true.
- One warm-up plus 16 completed frames.
- 16 CPU frame samples and 16 GPU timestamp samples.
- GPU timestamp: median 49,423 ns; P95 79,385 ns; min 37,154 ns; max 79,385 ns.
- CPU acquire-to-idle: median 7,666 us; P95 9,911 us; min 460 us; max 9,911 us.
- Shader modules, graphics pipeline, three-vertex draw, presentation, timestamp
  query pool, and cleanup all reported success.
- No sustained FPS, thermal stability, battery impact, pixel readback, or game
  compatibility claim is made.

Raw user-provided report:
`docs/hardware-reports/mt6991-redmi-k80-ultra-r1.8-2026-11-10-pre-semaphore-fix.json`.

## Corrected-build target-device acceptance — PENDING

Source review found that the initial R1.8 implementation reused a single
presentation wait semaphore across different swapchain images. The branch now
allocates one presentation semaphore per swapchain image and selects it using
the image index returned by `vkAcquireNextImageKHR`.

The corrected source passed Core CI and both Android APK workflows listed above.
A fresh schema-v4 report from the corrected APK on the actual REDMI K80 Ultra is
still required before final R1.8 hardware sign-off.

## Acceptance criteria

- One warm-up frame and 16 successful measured frames on the corrected APK.
- 16 valid CPU frame samples and 16 valid GPU timestamp samples.
- Successful image acquisition, draw submission, presentation and queue-idle
  completion for all frames.
- Query-pool and Vulkan object cleanup succeed.
- The corrected-build schema-v4 JSON report is committed under
  `docs/hardware-reports/`.

## Stack and safety

Draft PR #22 is stacked on R1.7 and remains unmerged. Stock Android Vulkan
loader/vendor driver only; no root, direct device-node access, SELinux/kernel
modifications, vendor partition writes, or driver replacement.

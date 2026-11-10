# NYAMIST G925 R2.2 — REDMI K80 Ultra Hardware Follow-up

**Result: same-device oldSwapchain handoff HARDWARE PASS. Actual extent change and real SUBOPTIMAL / OUT_OF_DATE recovery: NOT OBSERVED in this run.**

- Date: 2026-11-10
- Device: REDMI K80 Ultra
- Platform: Android 16 / API 36 / arm64-v8a
- SoC: MediaTek MT6991
- GPU: Mali-G925-Immortalis MC12
- Vulkan physical-device API: 1.3.278
- Android Vulkan loader API: 1.4.0
- Report schema: 11, including R2.2 swapchain-recreation fields
- Target GPU match: true; vendor 0x13b5, device 0xd8300010
- The physical-device UUID is deliberately omitted from this public record.

## R2.2 same-device oldSwapchain handoff — HARDWARE PASS

| Check | Observed | Result |
|---|---:|---|
| Swapchain recreation status | success | PASS |
| Recreation mode | old_swapchain_handoff | PASS |
| Recreation attempts / successes | 1 / 1 | PASS |
| Old swapchain handle supplied | true | PASS |
| Old swapchain destroyed | success | PASS |
| Logical device reused | true | PASS |
| Replacement swapchain image query | success | PASS |
| Replacement images | 6 | PASS |
| Probe image acquire | success | PASS |
| Probe command submission | success | PASS |
| Probe presentation | success | PASS |
| Probe presentation completion | present_fence_signaled | PASS |
| Probe present count | 1 | PASS |
| Result-policy self-test | success | PASS |

The replacement swapchain was created using the previous swapchain handle as
`oldSwapchain` while retaining the same VkDevice and VkSurfaceKHR. The old
swapchain was retired and destroyed successfully. A replacement image was
acquired, transitioned for presentation, submitted and presented; the
present-fence completion was observed.

The recreation probe itself does not draw scene content
(`swapchain_recreation_probe_rendered=false`). The following regular frame
sequence independently confirmed rendering and readback on the active
swapchain.

## Resize and WSI error-event boundary — NOT OBSERVED

| Signal | Observed | Interpretation |
|---|---:|---|
| Extent before | 1202 × 715 | Recorded |
| Extent after | 1202 × 715 | Recorded |
| Extent changed | false | No actual resize observed |
| Android Surface resize events | 0 | No resize observed |
| Acquire SUBOPTIMAL | 0 | Not observed |
| Present SUBOPTIMAL | 0 | Not observed |
| Create OUT_OF_DATE | 0 | Not observed |
| Acquire OUT_OF_DATE | 0 | Not observed |
| Present OUT_OF_DATE | 0 | Not observed |
| OUT_OF_DATE recovery attempts / successes | 0 / 0 | Not observed |
| OUT_OF_DATE recovery status | not_observed | Correct classification |

This run proves a successful oldSwapchain handoff at an unchanged 1202×715
extent. It does **not** prove an actual dimension-changing resize, nor that
the driver emitted VK_SUBOPTIMAL_KHR or VK_ERROR_OUT_OF_DATE_KHR. The
result-policy self-test validates the application's classification policy;
it is not evidence that the driver emitted either result.

## Rendering and pixel readback retained — PASS

- Selected surface format: R8G8B8A8_SRGB / SRGB_NONLINEAR.
- Present mode: FIFO.
- Swapchain extent: 1202×715; six images.
- Transfer-source usage: supported.
- Pixel source: acquired swapchain image.
- Five samples / 20 bytes copied successfully.
- Four background samples: [53,105,153,255], exact expected clear color.
- Triangle center: [200,180,180,255].
- `pixel_background_samples_match_clear=true`.
- `pixel_triangle_center_detected=true`.
- `pixel_verification_status=success`.

## R2.6 uint8 indexed-draw probe retained — PASS

- `VK_KHR_index_type_uint8`: advertised; feature supported and enabled.
- Three-byte index buffer creation, allocation, binding, mapping, upload,
  unmapping and cleanup: success.
- Indexed draw entry points and command recording: success.
- `index_type_uint8_probe_status=success`.
- The swapchain pixel readback independently detected the triangle.

## Presentation completion and lifecycle — PASS

- Present-fence attaches / waits / intermediate resets: 17 / 17 / 16.
- Final shutdown presentation completion: `present_fence_signaled`.
- Surface created / destroyed / recreated: 4 / 3 / 3.
- Surface changed / resize events: 4 / 0.
- Activity resumes / pauses: 4 / 3.
- Native WSI sessions started / completed / successful: 19 / 19 / 19.
- Native sessions skipped / active at report: 0 / 0.
- Maximum concurrent native sessions: 1.
- Manual rebuild requests: 15.
- Latest report request ID: 22; Surface generation: 4.
- Latest session: 1202×715, reason `manual_rebuild`, successful and current
  at completion.

The lifecycle data confirms repeated Surface destruction/recreation and
serialized fresh WSI sessions. This is distinct from the R2.2 same-device
oldSwapchain handoff and does not establish an actual Surface resize.

## Timing context

- Completed frames: 17, including one warm-up frame.
- Measured frame timing samples: 16.
- GPU frame-time min / median / p95 / max: 56,923 / 71,308 / 113,615 /
  113,615 ns.
- CPU frame-total min / median / p95 / max: 332 / 7,772 / 10,301 /
  10,301 µs.

These are short controlled triangle-validation samples, not game FPS,
sustained throughput, thermal, battery or power benchmarks.

## Cleanup and safety

All reported cleanup operations succeeded, including pixel-readback buffer
and memory, semaphores, command buffer/pool, image view, render pass,
framebuffer, graphics pipeline, shader modules, swapchain, device, surface,
instance and native-window release.

Validation used the stock Android public Vulkan loader and stock vendor
driver. No root, private GPU device-node access, SELinux/kernel changes,
vendor partition writes or driver replacement.

## Acceptance summary

- **Passed:** same-device oldSwapchain handoff, old-handle retirement,
  replacement image enumeration, acquire/submit/present/completion,
  unchanged-extent rendering/readback, uint8 indexed draw, present fence,
  serialized lifecycle sessions and cleanup.
- **Not observed:** actual extent-changing resize and real driver
  SUBOPTIMAL / OUT_OF_DATE emission/recovery.

Do not promote the not-observed items to hardware PASS without a subsequent
device report containing a nonzero actual-event counter and successful
recovery evidence.

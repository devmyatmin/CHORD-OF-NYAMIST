# NYAMIST G925 R2.1 — REDMI K80 Ultra Hardware Evidence

**Result: HARDWARE PASS**

- Date: 2026-11-10
- Device: REDMI K80 Ultra
- Platform: Android 16 / API 36 / arm64-v8a
- SoC: MediaTek MT6991 / Dimensity 9400+
- GPU: Mali-G925-Immortalis MC12
- Vendor/device: 0x13b5 / 0xd8300010
- Vulkan device API: 1.3.278
- Loader: libvulkan.so / API 1.4.0
- Report: native schema v6 plus android_surface_lifecycle schema v1

## Android Surface lifecycle

| Metric | Observed | Result |
|---|---:|---|
| Surface created | 6 | PASS |
| Surface destroyed | 5 | PASS |
| Surface recreated | 5 | PASS |
| Surface changed | 6 | PASS |
| Surface resize | 0 | No resize observed |
| Activity pauses | 5 | PASS |
| Activity resumes | 6 | PASS |
| Activity resumed at report | true | PASS |

The counts confirm five actual Surface destroy/recreate cycles and five
Activity pause events followed by six total resume events, including the
initial resume. This is distinct from manual WSI rebuild requests.

## Serialized Vulkan WSI sessions

| Metric | Observed | Result |
|---|---:|---|
| Native sessions started | 16 | PASS |
| Native sessions completed | 16 | PASS |
| Native sessions successful | 16 | PASS |
| Native sessions skipped | 0 | PASS |
| Active native sessions at report | 0 | PASS |
| Maximum concurrent native sessions | 1 | PASS |
| Manual rebuild requests | 10 | Recorded |
| Latest request ID | 21 | Recorded |
| Latest session generation | 6 | Recorded |
| Latest session current at completion | true | PASS |

Every native request creates a fresh Vulkan instance, Android VkSurface,
logical device and swapchain, then performs cleanup before the executor can
start the next native session. This establishes serialized WSI session
reconstruction across the observed Android Surface lifecycle. It is not an
in-place oldSwapchain handoff.

## Latest native rendering and readback

- Native report status: success; target_gpu_match: true.
- Surface format: R8G8B8A8_SRGB / SRGB_NONLINEAR.
- Present mode: FIFO; extent 1202×715; six swapchain images.
- Image usage: COLOR_ATTACHMENT|TRANSFER_SRC; transfer source supported.
- Pixel readback: five samples / 20 bytes from acquired swapchain image.
- Expected clear RGBA8: [53,105,153,255].
- top_left: [53,105,153,255] — exact match.
- top_right: [53,105,153,255] — exact match.
- bottom_left: [53,105,153,255] — exact match.
- bottom_right: [53,105,153,255] — exact match.
- triangle_center: [200,180,180,255] — triangle detected.
- pixel_background_samples_match_clear: true.
- pixel_triangle_center_detected: true.
- pixel_verification_status: success.

## Latest presentation completion and cleanup

- VK_EXT_swapchain_maintenance1 enabled and used.
- Present-fence attaches / waits / intermediate resets: 17 / 17 / 16.
- Shutdown presentation completion: present_fence_signaled.
- GPU timestamp samples: 16 / success.
- Pixel readback buffer and memory, semaphores, command buffer/pool,
  image view, render pass, framebuffer, graphics pipeline, pipeline layout,
  shader modules, swapchain, device, Vulkan surface, instance and native
  window cleanup: success.

## Timing context

- GPU frame-time samples: 16.
- GPU frame time min 61,538 ns; median 86,000 ns; p95 126,154 ns;
  max 126,154 ns.
- CPU frame-total samples: 16.
- CPU frame total min 505 µs; median 8,139 µs; p95 16,663 µs;
  max 16,663 µs.

These are diagnostic measurements from the controlled triangle workload,
not game FPS, sustained performance, thermal or battery evidence.

## Acceptance boundary

R2.1 is hardware-verified for Android Surface destruction/recreation,
Activity pause/resume accounting, serialized fresh Vulkan WSI session rebuilds,
R2.0 pixel readback, present-fence completion and cleanup on the target G925.

It does not prove physical-panel scanout, FPS, sustained performance,
thermal/battery behavior, full-frame correctness, Stranded Deep or any other
game compatibility. Resize count was zero in this test and remains a separate
unexercised event path.

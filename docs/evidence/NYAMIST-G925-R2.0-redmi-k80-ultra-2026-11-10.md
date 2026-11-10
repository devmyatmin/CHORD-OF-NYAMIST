# NYAMIST G925 R2.0 — REDMI K80 Ultra Hardware Evidence

**Result: HARDWARE PASS**

- Date: 2026-11-10
- Device: REDMI K80 Ultra (25060RK16C, dali)
- Platform: Android 16 / API 36 / arm64-v8a
- SoC: MediaTek MT6991 / Dimensity 9400+
- GPU: Mali-G925-Immortalis MC12
- Vulkan device API: 1.3.278
- Android Vulkan loader API: 1.4.0
- GPU identity: vendor 0x13b5, device 0xd8300010
- Report schema: 6; report status: success; target_gpu_match: true

## Swapchain and pixel readback

- Surface format: R8G8B8A8_SRGB / SRGB_NONLINEAR
- Present mode: FIFO
- Surface extent: 1202 × 715
- Image usage: COLOR_ATTACHMENT|TRANSFER_SRC
- Transfer-source usage: supported
- Readback source: acquired swapchain image
- Staging allocation: 20 bytes; HOST_VISIBLE|HOST_COHERENT
- Sample count: 5

Expected background clear RGBA8: [53,105,153,255].

| Sample | RGBA8 | Result |
|---|---|---|
| top_left | [53,105,153,255] | Exact match |
| top_right | [53,105,153,255] | Exact match |
| bottom_left | [53,105,153,255] | Exact match |
| bottom_right | [53,105,153,255] | Exact match |
| triangle_center | [200,180,180,255] | Triangle detected |

- pixel_background_samples_match_clear: true
- pixel_triangle_center_detected: true
- pixel_verification_status: success

## Presentation completion

VK_EXT_swapchain_maintenance1 was enabled and used.

- Present-fence attaches: 17
- Present-fence waits: 17
- Intermediate resets: 16
- Shutdown completion: present_fence_signaled
- present_completion_guaranteed: true

This is Vulkan presentation-resource completion evidence, not a
physical-panel scanout timestamp.

## Frame timing

- Completed frames: 17 (1 warm-up + 16 measured)
- GPU timestamp samples: 16, all successful
- GPU frame time: min 27,692 ns; median 46,192 ns; p95 97,154 ns;
  max 97,154 ns.
- CPU frame-total samples: 16
- CPU frame total: min 371 µs; median 7,647 µs; p95 14,719 µs;
  max 14,719 µs.

These are diagnostic values from the controlled triangle test. They must not
be converted into game FPS, sustained-performance, thermal or battery claims.

## Cleanup and acceptance boundary

The device report recorded successful cleanup for the readback buffer and
memory, semaphores, command buffer/pool, image view, render pass, framebuffer,
graphics pipeline, pipeline layout, shader modules, swapchain, device,
surface, Vulkan instance and native window.

R2.0 proves that five selected pixel values were read from the acquired
Android swapchain image and matched the expected clear-background / triangle
assertions on the target G925 device. It does not prove full-frame correctness,
physical-panel scanout, sustained performance, game-engine compatibility or
that Stranded Deep runs on Android.

R2.1 proceeds to Android Surface lifecycle and serialized WSI session
recreation validation.

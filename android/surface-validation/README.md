# NYAMIST G925 R2.0 — Swapchain Pixel Readback Verification

This Android diagnostic extends the accepted R1.9 present-fence path with
actual pixel readback from the acquired swapchain image. It verifies four
clear-color background locations and one triangle-interior point using a
small host-visible staging buffer.

## Execution model

1. Create the Android Vulkan instance, surface, G925 logical device, graphics /
   present queue, and FIFO swapchain through the stock Android loader.
2. Verify that the Android Surface supports both
   `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT` and
   `VK_IMAGE_USAGE_TRANSFER_SRC_BIT`. If transfer-source use is unavailable,
   report the capability failure explicitly; do not claim pixel validation.
3. Create the swapchain with `COLOR_ATTACHMENT | TRANSFER_SRC`, retain the
   R1.9 `VK_EXT_swapchain_maintenance1` present-fence path, and create a
   20-byte transfer-destination buffer for five RGBA8 sample values.
4. Render the existing three-vertex triangle during one warm-up frame. The
   render pass ends in `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`.
5. Copy five 1×1 regions from the acquired swapchain image to the staging
   buffer: four corners (background) and the center (triangle).
6. Transition the swapchain image back to
   `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`, submit and present it, then wait for
   rendering/presentation completion before mapping the staging buffer.
7. Prefer HOST_VISIBLE|HOST_COHERENT memory. If only non-coherent host-visible
   memory is selected, invalidate the mapped allocation before reading.
8. Normalize BGRA formats to RGBA, compare four background samples against the
   expected sRGB-encoded clear color, and require the center sample to differ
   from the clear color with opaque alpha.
9. Keep the pixel copy in the warm-up frame only. The 16 measured GPU timestamp
   intervals remain around the render pass and exclude the readback copy.
10. Submit 16 measured frames after warm-up, preserve the R1.9 present-fence
    lifecycle, serialize schema-v6 evidence, and destroy readback resources.

## Report fields

Schema v6 adds Surface transfer-source support, selected image usage,
staging-buffer/memory state, copy/map/invalidate/unmap results, five sample
labels, expected clear RGBA8, observed RGBA8 pixel values, background-match and
triangle-center booleans, final verification state, and readback cleanup state.

For a target-device pass, the report must show:

- `swapchain_transfer_src_support_status: supported`
- `pixel_readback_mode: swapchain_transfer_src`
- `pixel_readback_sample_count: 5`
- `pixel_background_samples_match_clear: true`
- `pixel_triangle_center_detected: true`
- `pixel_verification_status: success`
- Successful staging allocation, binding, copy, map, invalidation when
  required, unmap, buffer destruction and memory free.
- R1.9 present-fence completion evidence and 16 GPU timestamp samples.

## Timing definitions and limitations

The GPU timestamp interval is TOP_OF_PIPE through BOTTOM_OF_PIPE around the
render pass. The warm-up frame's readback copy is outside the measured 16 GPU
samples. CPU acquire-to-idle timings remain distinct from GPU timestamps.

The data is copied from the swapchain image before presentation; it is not a
screenshot or scanout readback of the physical display panel. Five pixel
samples are a focused output check, not exhaustive framebuffer validation.
The result does not establish FPS, sustained performance, thermal or battery
behavior, or game compatibility.

## Build and run

The R2.0 Android workflow builds an arm64-v8a debug APK and uploads the
`nyamist-g925-r2.0-pixel-readback-apk` artifact.

1. Download the APK from a successful R2.0 Android workflow run.
2. Uninstall an older debug build first if Android reports a signing conflict.
3. Install and open **NYAMIST G925 R2.0 Pixel Readback Validation**.
4. Keep the app in the foreground until the 1 warm-up + 16 measured frames
   finish.
5. Tap **Copy JSON report** and paste the complete schema-v6 JSON into the
   project chat.

Hosted CI validates C++ tests, Android native compilation, shader compilation,
and APK packaging only. Actual REDMI K80 Ultra JSON evidence is mandatory for
hardware acceptance.

No root, direct device-node access, SELinux/kernel changes, vendor partition
writes, or driver replacement are used.

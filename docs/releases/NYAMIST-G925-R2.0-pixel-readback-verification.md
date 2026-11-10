# NYAMIST G925 R2.0 — Swapchain Pixel Readback Verification

**Implementation, hosted validation and target-device validation: PASS.**

## Objective

Extend the accepted R1.9 G925 Android Vulkan path from recorded triangle draw
commands to actual pixel-value verification of the acquired swapchain image.

R2.0 does not claim that a draw call alone proves correct rendered output.
The target-device report must contain bytes copied from the real swapchain
image and checked against expected clear-background and triangle-center values.

## Implementation design

- Keep the R1.9 Android public Vulkan loader, G925 selection, FIFO swapchain,
  per-image presentation semaphores, and VK_EXT_swapchain_maintenance1
  present-fence path.
- Query VkSurfaceCapabilitiesKHR.supportedUsageFlags and require both
  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT and VK_IMAGE_USAGE_TRANSFER_SRC_BIT.
- Create the swapchain with COLOR_ATTACHMENT | TRANSFER_SRC. If the target
  Surface does not support transfer-source usage, fail explicitly and do not
  claim pixel verification.
- Create a 20-byte transfer-destination staging buffer for five RGBA8 pixels.
  Select HOST_VISIBLE memory, preferring HOST_COHERENT; when memory is not
  coherent, invalidate the mapped allocation before reading.
- In the single unmeasured warm-up frame, render the triangle, copy five
  1×1 regions from the acquired swapchain image using vkCmdCopyImageToBuffer,
  and transition the image back to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR.
- Sample four corners as clear-color background points and the center as a
  triangle-interior point. Normalize BGRA formats to RGBA before comparison.
- Keep the pixel copy outside the 16 measured GPU timestamp intervals.
- Wait for GPU completion before mapping the staging memory. Destroy the
  staging buffer and free its memory during orderly cleanup.

## Target-device validation — PASS

Validated on REDMI K80 Ultra / Android 16 / MediaTek MT6991 /
Immortalis-G925 MC12. The complete schema-v6 JSON report was returned by the
device app.

- Target identity: vendor 0x13b5, device 0xd8300010, Vulkan device API 1.3.278,
  Android loader API 1.4.0; target_gpu_match is true.
- Surface format R8G8B8A8_SRGB / SRGB_NONLINEAR, FIFO present mode,
  extent 1202×715, and COLOR_ATTACHMENT|TRANSFER_SRC usage.
- Pixel readback source: acquired_swapchain_image; five RGBA8 samples,
  20 bytes copied and mapped successfully.
- Expected clear RGBA8: [53,105,153,255]. All four corner samples matched
  exactly. Triangle center was [200,180,180,255] and was detected.
- pixel_background_samples_match_clear, pixel_triangle_center_detected,
  and pixel_verification_status all reported success.
- Present-fence path: 17 attaches, 17 waits, 16 intermediate resets;
  shutdown reported present_fence_signaled.
- One warm-up plus 16 measured frames completed; all 16 GPU timestamp queries
  succeeded. GPU frame-time median was 46,192 ns and p95 was 97,154 ns.
- CPU diagnostic frame-total median was 7,647 µs and p95 was 14,719 µs.
  These are diagnostic samples, not a game FPS or sustained-performance claim.
- Pixel buffer/memory, swapchain, device, surface, instance and native-window
  cleanup all reported success.

See the R2.0 device evidence file for the exact sample table and test boundary.

## Hosted validation — PASS

Validated implementation and documentation head cb7c0932e26e5c06ec3c34bf12e947aabde3a638.

- NYAMIST Core CI Run #354 (42745356372): SUCCESS.
- R2.0 Android push workflow Run #11 (42745355788): SUCCESS.
- R2.0 Android PR workflow Run #12 (42745362732): SUCCESS.
- Earlier APK artifact: R2.0 Android PR Run #10 (42745024407), artifact
  nyamist-g925-r2.0-pixel-readback-apk, ID 13731186970.
- [Core CI Run #354](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42745356372)
- [Android push Run #11](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42745355788)
- [Android PR Run #12](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42745362732)

Hosted CI verifies C++ tests, Android native compilation and APK packaging.
The device JSON independently establishes Surface transfer-source support
and actual pixel values produced by the MT6991 GPU driver.

## Measurement boundaries

Pixel verification reads the acquired swapchain image before presentation;
it is not a physical-panel scanout measurement. The present fence proves
presentation-resource lifetime completion, not that scanout has finished.

The five-pixel check is focused render-output verification, not exhaustive
framebuffer validation. It does not establish FPS, sustained performance,
thermal or battery behavior, or game compatibility.

## Safety

Stock Android public Vulkan loader and vendor driver only. No root, direct GPU
device-node access, SELinux/kernel modifications, vendor partition writes,
or driver replacement.

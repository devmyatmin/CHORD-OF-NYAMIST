# NYAMIST G925 R2.0 — Swapchain Pixel Readback Verification

**Implementation: in progress. Hosted validation and target-device validation: pending.**

## Objective

Extend the accepted R1.9 G925 Android Vulkan path from recorded triangle draw
commands to actual pixel-value verification of the acquired swapchain image.

R2.0 does not claim that a draw call alone proves correct rendered output.
The target-device report must contain bytes copied from the real swapchain
image and checked against expected clear-background and triangle-center values.

## Implementation design

- Keep the R1.9 Android public Vulkan loader, G925 selection, FIFO swapchain,
  per-image presentation semaphores, and `VK_EXT_swapchain_maintenance1`
  present-fence path.
- Query `VkSurfaceCapabilitiesKHR.supportedUsageFlags` and require both
  `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT` and
  `VK_IMAGE_USAGE_TRANSFER_SRC_BIT`.
- Create the swapchain with `COLOR_ATTACHMENT | TRANSFER_SRC`. If the target
  Surface does not support transfer-source usage, fail explicitly and do not
  claim pixel verification.
- Create a 20-byte transfer-destination staging buffer for five RGBA8 pixels.
  Select HOST_VISIBLE memory, preferring HOST_COHERENT; when memory is not
  coherent, invalidate the mapped allocation before reading.
- In the single unmeasured warm-up frame, render the triangle, copy five
  1×1 regions from the acquired swapchain image using
  `vkCmdCopyImageToBuffer`, and transition the image back to
  `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` before presentation.
- Sample the four corners as clear-color background points and the center as
  a triangle-interior point. Normalize BGRA formats to RGBA before comparison.
- Compare the four background pixels with the expected sRGB-encoded clear
  color within a small channel tolerance. Require the center point to differ
  from the clear color and have an opaque alpha channel.
- Keep the pixel copy outside the 16 measured GPU timestamp intervals. The
  timestamp interval remains TOP_OF_PIPE through BOTTOM_OF_PIPE around the
  render pass; readback is a separate validation operation.
- Wait for GPU completion before mapping the staging memory. Destroy the
  staging buffer and free its memory during orderly cleanup.

## Schema v6 evidence

The report adds:

- Surface transfer-source capability and selected swapchain image usage.
- Pixel readback path, source, staging-buffer and memory allocation state.
- Copy recording/result, map/invalidate/unmap states and cleanup states.
- Five sample labels, expected RGBA8 clear value, and five observed RGBA8 values.
- Independent booleans for background clear-color match and triangle-center
  detection, plus `pixel_verification_status`.

A full target-device pass requires:

- `swapchain_transfer_src_support_status: supported`
- `pixel_readback_mode: swapchain_transfer_src`
- 5 readback samples (20 bytes), four background samples matching expected
  clear color, and triangle-center detection.
- Successful staging-buffer creation, allocation, binding, copy, map and
  invalidate when needed; successful unmap and cleanup.
- R1.9 present-fence lifecycle remains successful (17 attaches / 17 waits /
  16 intermediate resets), along with 1 warm-up + 16 measured frames and
  16 GPU timestamp samples.
- No Vulkan failure, device loss, or incomplete resource cleanup.

## Measurement boundaries

Pixel verification reads the acquired swapchain image before presentation;
it is not a screenshot of the physical display panel. The present fence proves
presentation-resource lifetime completion, not that scanout/display of the
frame has finished.

The five-pixel check is a focused render-output assertion, not exhaustive
framebuffer validation. It does not establish full-scene correctness, FPS,
sustained performance, thermal or battery behavior, or game compatibility.

## Hosted validation

Pending. Core C++ tests and Android arm64 APK workflows must pass before
target-device installation.

## Target-device validation

Pending REDMI K80 Ultra schema-v6 JSON evidence. Hosted CI validates compilation
and APK packaging only; it cannot establish transfer-source support or actual
pixel values produced by the MT6991 vendor GPU driver.

## Safety and platform boundary

Stock Android public Vulkan loader and vendor driver only. No root, direct GPU
device-node access, SELinux/kernel modifications, vendor partition writes, or
driver replacement.

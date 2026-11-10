# NYAMIST G925 R2.2 — Swapchain Resize and OUT_OF_DATE Recovery

R2.2 extends R2.1 Surface lifecycle validation with a same-device Vulkan
swapchain replacement probe and bounded handling for WSI result changes.

## Same-device oldSwapchain handoff

1. Build the initial FIFO Android swapchain on the selected G925 device.
2. Re-query VkSurfaceCapabilitiesKHR, surface formats and present modes.
3. Recompute the current extent using currentExtent or the requested Android
   Surface dimensions clamped to the refreshed min/max extent.
4. Create a replacement VkSwapchainKHR with oldSwapchain set to the current
   swapchain handle, preserving the same VkDevice and Android VkSurfaceKHR.
5. Enumerate the replacement images and destroy the retired old swapchain.
6. Acquire one replacement image, record a layout transition from
   UNDEFINED to PRESENT_SRC_KHR, submit the command buffer, and present it.
7. Wait for VK_EXT_swapchain_maintenance1 present fence when available;
   otherwise use queue-idle fallback and report the weaker completion mode.
8. Run the regular R2.0 triangle render, timestamp and five-pixel readback
   sequence on the replacement swapchain.

The handoff probe intentionally does not draw scene content on its one
replacement image. Its purpose is to prove the new chain can be acquired,
submitted and presented while retaining the same logical device. The following
normal frame sequence performs the actual triangle draw and pixel readback.

## WSI result policy

- VK_SUCCESS continues the current operation.
- VK_SUBOPTIMAL_KHR permits the current operation to complete, then requests
  a replacement swapchain. The replacement probe retries boundedly if the
  new chain is still suboptimal.
- VK_ERROR_OUT_OF_DATE_KHR requests replacement before another acquire/present.
  The replacement probe retries up to two times after the first attempt,
  using the latest swapchain as oldSwapchain each time.
- Other errors remain fatal and are not silently converted to success.

The primary 17-frame renderer still treats an OUT_OF_DATE result as a failed
native session, with an explicit status. Android then queues one fresh full
WSI session retry per Surface generation after native cleanup. That fallback
recreates the VkDevice too; it is reported separately and is not described as
same-device swapchain recovery.

The deterministic native policy self-test verifies the result mapping.
Actual driver-returned OUT_OF_DATE and SUBOPTIMAL results are counted
separately. When neither occurs, the report says "not_observed"; this is not
evidence that the driver returned OUT_OF_DATE.

## R2.2 report schema v7

The native JSON schema v7 adds:

- oldSwapchain handoff mode, attempt/success counts and old-handle destruction.
- Replacement image query status/count.
- Extent before/after and whether the Surface extent changed.
- Same VkDevice reuse evidence.
- Replacement image acquire, command submission, present and completion status.
- Separate acquire/present SUBOPTIMAL and OUT_OF_DATE counters.
- Bounded recovery attempt/success counters and explicit not-observed/recovered
  status for real OUT_OF_DATE events.
- Android lifecycle metadata includes the distinct full-session retry counter
  used only when the primary render loop returns OUT_OF_DATE.

## R2.2 device test procedure

1. Install NYAMIST G925 R2.2 and open the app.
2. Wait for the initial SUCCESS report.
3. Rotate the device once and allow Android to recreate or resize the Surface.
4. Wait for the newest session to finish and copy the full JSON report.
5. Check swapchain_recreation_status, oldSwapchain supplied/destroyed,
   logical device reused, replacement probe acquire/submit/present and
   completion fields, plus the R2.0 pixel and R1.9 present-fence evidence.
6. Check swapchain_extent_changed and the actual WSI result counters. Do not
   call it a resize pass when the extent did not change. Do not claim a real
   OUT_OF_DATE recovery if the observed driver result count is zero.

A successful oldSwapchain handoff with identical before/after dimensions
proves same-device replacement but does not prove an actual resize event.
Android's fixed currentExtent may legitimately remain unchanged for a Surface.

## Validation boundaries

Hosted tests validate report serialization, policy classification, Java
session-retry wiring, native compilation and APK packaging. Hardware evidence
is required to establish replacement swapchain behavior on the MT6991 driver.

This milestone does not establish FPS, sustained performance, thermal/battery
behavior, physical-panel scanout, game-engine compatibility or Stranded Deep
Android compatibility.

Stock Android public Vulkan loader and vendor driver only. No root, direct GPU
device-node access, SELinux/kernel changes, vendor partition writes, or driver
replacement.

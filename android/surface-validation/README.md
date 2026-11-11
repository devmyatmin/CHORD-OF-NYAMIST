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

## Actual Surface resize test — Part 1

The Android lifecycle report is schema version 2. It preserves the last valid
Surface dimensions across Surface destruction/recreation, records the first
and latest dimensions, and reports the last observed resize from/to values.
The Activity also records orientation configuration changes.

For a deterministic Surface resize and live-WSI event test:

1. Install the Part 1 arm64 debug APK and open the app.
2. Wait for the initial R2.6 WSI session to finish successfully.
3. Tap **INCREASE SURFACE SIZE (WSI RESIZE TEST)**, then
   **RESTORE SURFACE SIZE (220dp)** once. Confirm Android reports a nonzero
   Surface resize count and distinct from/to dimensions.
4. Enable Android auto-rotate, rotate portrait → landscape → portrait, and
   wait for the Surface session to finish.
5. Tap **START LIVE WSI RESIZE WINDOW**. The native same-device oldSwapchain
   probe pauses for two seconds immediately before image acquire, then for
   two seconds after queue submit but before present. During the first pause,
   tap the Surface size button to increase the SurfaceView to 320dp. During
   the second pause, tap it again to restore 220dp. The Surface changes race
   the real Vulkan acquire/present calls while the existing device and
   swapchain probe remain active.
6. Allow the live native session and any serialized follow-up Surface session
   to finish. Use **COPY LIVE WSI REPORT** for the dedicated probe result and
   **COPY LATEST SESSION REPORT** for the newest Surface session. If the Live
   WSI report says `session_current_at_completion=false`, it is a preserved
   snapshot from the session superseded during the resize; use the latest-session
   report to inspect the final rendered frame.
7. Preserve both JSON reports and the separate app file
   `nyamist-g925-r2.2-live-wsi-resize-window.json`.

Part 1 acceptance requires a nonzero `surface_resize_count`, a true
`surface_resize_observed`, differing last-resize from/to dimensions, and
session dimensions matching the latest Surface callback. The controlled
toggle count should be at least two. A nonzero configuration-change count
confirms Android delivered orientation changes.

If Android destroys/recreates the Surface during rotation, the prior valid
dimensions are retained so a real size change is not lost when the new
Surface first reports its dimensions. Surface recreation alone is not a
The live-window mode records both pause points and their total duration. R2.7 provides a 5-second pause before acquire and a 5-second pause before present (10 seconds total) so the operator can resize the Surface during both windows. A real Vulkan VK_SUBOPTIMAL_KHR or VK_ERROR_OUT_OF_DATE_KHR result is proven only when the corresponding driver-result counter is nonzero. For OUT_OF_DATE, also require a nonzero recovery-success count and final same-device handoff success. A Surface resize with zero WSI result counters remains a resize PASS but WSI-event status is not_observed.

## Live WSI Resize Window mode

The live-window control is designed to expose the actual WSI operation to a Surface change while the same VkDevice and oldSwapchain probe are active. It opens two 2-second windows: before image acquire, and after queue submit but before present. Tap the Surface size control once in each window. This is an observation harness, not an event injector; the driver is free to return VK_SUCCESS, VK_SUBOPTIMAL_KHR or VK_ERROR_OUT_OF_DATE_KHR. The report includes the mode flag, pause counts, total pause duration, real acquire/present result counters, and bounded recovery counts. The live report is stored separately and exported by its dedicated copy action; it does not replace the latest-session report.

## Surface transform and aspect-preserving rendering — Part 1

R2.7 starts in sensor-landscape game mode. The on-screen orientation control can
temporarily switch the validation Activity to full-sensor mode so portrait and
landscape WSI transitions remain testable without making portrait the default.

R2.7 implements Android Vulkan pre-rotation rather than preferring IDENTITY.
The swapchain `preTransform` matches the Surface's reported
`currentTransform`. When the transform rotates the display by 90 or 270 degrees,
the swapchain extent is converted back to the display's natural/identity
orientation by swapping width and height. The current transform is then applied
to every frame through a vertex-stage push-constant matrix. This keeps the
swapchain image orientation and the geometry transform consistent and avoids
a second presentation-engine rotation.

The report records `surface_current_transform`,
`surface_supported_transforms`, `swapchain_pre_transform`,
`swapchain_pre_transform_identity_selected`,
`swapchain_pre_transform_policy`, `surface_prerotation_applied`, and
`surface_prerotation_status` for evidence. The matrix covers identity,
90/180/270-degree rotations and the corresponding horizontal-mirror transforms.
Unsupported transform values fail the frame explicitly rather than silently
rendering with a wrong orientation.

The graphics pipeline uses a centered square viewport whose side is the
smaller swapchain dimension. The render pass still clears and presents the
complete Surface. Validate portrait and landscape screenshots: the triangle
should retain the same proportions while the Surface background continues to
fill the full swapchain image.

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

## Hosted R2.2 build — PASS

The current R2.2 source passed Core CI push/PR and Android APK push/PR
workflows. The PR APK artifact (ID 13734383707) contains a valid arm64-v8a
debug APK with an ELF64/AArch64 native library built using NDK r28c.

Target-device validation is still pending. The REDMI K80 Ultra report must
show oldSwapchain supplied, replacement-image query/acquire/submit/present,
presentation completion, same logical device reuse, and separate actual
SUBOPTIMAL / OUT_OF_DATE counters. A swapchain handoff with unchanged extent
must not be described as a resize pass.

## R2.3 — Vulkan 1.4 and stock-driver identity audit

R2.3 requests the highest Vulkan instance API version exposed by the Android
public loader, capped at 1.4. It does not upgrade the physical-device driver:
loader API, instance API and physical-device API are reported separately.

The report queries `VkPhysicalDeviceDriverProperties` and
`VkPhysicalDeviceIDProperties` through `vkGetPhysicalDeviceProperties2`,
recording driver ID/name/info, raw driver version (decimal and hexadecimal),
conformance version and physical-device UUID.

When the physical device itself advertises Vulkan 1.4 and the instance API is
also 1.4, the app queries `VkPhysicalDeviceVulkan14Features` and checks
Bresenham lines, clustered subgroup rotate, push descriptors, and the
conditional protected-pipeline-access feature. This focused audit is not a
replacement for Vulkan CTS or Android VTS.

### R2.3 device test

1. Install NYAMIST G925 R2.3 and run it on the REDMI K80 Ultra.
2. Wait for the complete G925 Surface validation session.
3. Copy the full JSON report.
4. Review `loader_api_version`, `instance_api_version`,
   `physical_device_api_version`, `driver_id`, `driver_name`,
   `driver_info`, `driver_version_raw`, `driver_version_hex`,
   `driver_conformance_version`, `physical_device_uuid` and
   `vulkan_1_4_support_status`.

A 1.4 loader with a physical-device API of 1.3 remains a Vulkan 1.3 GPU
implementation. The app does not replace the stock vendor driver or modify
system partitions.


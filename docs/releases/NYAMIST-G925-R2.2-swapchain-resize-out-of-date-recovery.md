# NYAMIST G925 R2.2 — Swapchain Resize and OUT_OF_DATE Recovery

**Implementation status: staged for hosted validation. Target-device validation: pending.**

## Objective

Extend the R2.1 Surface lifecycle pass with a Vulkan swapchain-only
replacement path that retains the existing VkDevice and VkSurfaceKHR, plus
bounded handling for WSI SUBOPTIMAL and OUT_OF_DATE results.

## Implementation design

- Keep the R2.1 Android Surface callback accounting and serialized native
  session executor.
- Refresh VkSurfaceCapabilitiesKHR, supported image usage, surface formats,
  present modes and current extent before replacement.
- Create a new VkSwapchainKHR with oldSwapchain set to the current handle.
  Reuse the same Vulkan logical device and Android VkSurfaceKHR.
- Enumerate the new swapchain images, retire/destroy the old handle, then
  acquire a replacement image.
- Record an image layout transition from UNDEFINED to PRESENT_SRC_KHR,
  submit it, present the replacement image and wait for a present fence when
  VK_EXT_swapchain_maintenance1 is available.
- Continue with the R2.0 triangle draw, timestamp query and pixel readback on
  the replacement swapchain.
- Classify VK_SUCCESS, VK_SUBOPTIMAL_KHR and VK_ERROR_OUT_OF_DATE_KHR
  separately. SUBOPTIMAL allows the current operation to finish and requests
  a handoff. OUT_OF_DATE triggers bounded retries in the replacement probe.
- If the main 17-frame render sequence itself returns OUT_OF_DATE, the native
  session reports failure and the Android wrapper may queue one full WSI
  session retry per Surface generation, after native cleanup. This is a
  separate fallback because it recreates VkInstance/VkDevice; it is not
  claimed as same-device swapchain recovery.
- Report schema v7 records old/new handles, extent changes, replacement
  present evidence, actual result counts, retry attempts and outcomes.

## Hosted validation — PENDING

- Core CI includes R2.2 contract tests for schema v7, oldSwapchain handoff,
  image acquire/submit/present, bounded recovery and Java fallback.
- Android workflow builds an arm64-v8a debug APK with JDK 17, SDK 36,
  NDK 28.2.13676358 and CMake 3.22.1.
- Hosted checks do not prove the target vendor driver has returned
  SUBOPTIMAL or OUT_OF_DATE during the device workload.

## Device acceptance criteria

- R2.1 target GPU, Surface lifecycle and native-session serialization
  evidence remain intact.
- Swapchain recreation status is success.
- Old swapchain handle supplied and destroyed successfully.
- Replacement image enumeration succeeds and the same logical device is
  reported reused.
- Replacement chain image acquisition, queue submit, present and completion
  succeed.
- R2.0 triangle drawing, 16 GPU timestamp samples and five-pixel readback
  succeed on the replacement swapchain.
- Surface extent before/after is recorded; claim an actual resize only when
  swapchain_extent_changed is true.
- The result-policy self-test passes. Actual OUT_OF_DATE recovery is claimed
  only if a real OUT_OF_DATE counter is nonzero and recovery success is
  recorded. Otherwise report not_observed.
- If the primary renderer encounters OUT_OF_DATE, it must not misreport a
  hardware pass; the Android fallback retry is separately counted.

## Measurement boundaries

The replacement probe transitions and presents an acquired image but does
not draw scene content. The subsequent regular frame sequence is the actual
render-output/pixel-readback proof. A same-extent oldSwapchain handoff does not
establish a resize event. OUT_OF_DATE policy tests do not establish that the
driver emitted the result.

No FPS, sustained performance, thermal/battery, physical-panel scanout,
full-frame correctness, game compatibility or Stranded Deep Android claim.

## Safety

Stock Android public Vulkan loader and vendor driver only. No root, direct GPU
device-node access, SELinux/kernel modifications, vendor partition writes,
or driver replacement.

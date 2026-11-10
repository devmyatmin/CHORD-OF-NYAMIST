# NYAMIST G925 R2.2 — Swapchain Resize and OUT_OF_DATE Recovery

**Hosted validation: PASS. Target-device same-device oldSwapchain handoff: HARDWARE PASS (2026-11-10). Actual extent change and real SUBOPTIMAL / OUT_OF_DATE emission/recovery: NOT OBSERVED.**

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

## Hosted validation — PASS

Validated code head: a200bff1ac41f5729c6aca8ab204c233eca493a9.

- NYAMIST Core CI push Run #400: SUCCESS.
- R2.2 Android push workflow Run #12: SUCCESS.
- NYAMIST Core CI PR Run #401: SUCCESS.
- R2.2 Android PR workflow Run #13: SUCCESS.
- [Core CI push Run #400](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42753727260)
- [Android push Run #12](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42753727224)
- [Core CI PR Run #401](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42753732418)
- [Android PR Run #13](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42753732766)

R2.2 APK artifact from Android PR Run #13:
- Artifact: nyamist-g925-r2.2-swapchain-recovery-apk
- Artifact ID: 13734383707
- ZIP size: 1,229,809 bytes
- ZIP SHA-256: 2f2ceea13686c2b333b3a6aa40f3db61695ec2e00c6a5bf5bed676c093ce802c
- Extracted APK size: 2,050,974 bytes
- APK SHA-256: e0ef6919fd1c0cccff53ec134c1678c4e1b8169be925748c2d3f36e1820adf3c
- Native library: lib/arm64-v8a/libnyamist_surface_validation.so
- Native ELF: 64-bit AArch64, Android NDK r28c (28.2.13676358)

The artifact ZIP and the contained APK both passed ZIP integrity tests. Hosted
validation proves compilation, tests and packaging. It does not prove that
the MT6991 driver has emitted a real SUBOPTIMAL or OUT_OF_DATE result.

## Target-device follow-up — same-device handoff PASS

The schema-v11 REDMI K80 Ultra follow-up report confirms one successful
same-device `oldSwapchain` handoff: old handle supplied and destroyed,
replacement images enumerated, replacement image acquired/submitted/presented,
and present-fence completion observed. The physical extent remained
1202×715, Android Surface resize events were zero, and acquire/present
SUBOPTIMAL plus create/acquire/present OUT_OF_DATE counters were all zero.
This is a hardware PASS for same-device handoff only, not for actual resize
or event-triggered recovery. See
[the device evidence](../evidence/NYAMIST-G925-R2.2-redmi-k80-ultra-2026-11-10.md).

## Part 1 — actual Surface resize instrumentation

The Android lifecycle report is schema version 2. It retains the last valid
Surface dimensions across Surface recreation and records initial/latest
dimensions, resize from/to dimensions, and orientation changes. A controlled
220dp ↔ 320dp SurfaceView control verifies Android Surface dimension changes.
The native validation report is schema version 12 and includes a dedicated
Live WSI Resize Window mode: two timed pauses, one immediately before
swapchain-image acquire and one after queue submit but before present, while
the original VkDevice and swapchain probe remain active. This lets a Surface
resize race the real acquire/present operations. The app stores and copies
the live-window report separately from later queued sessions.

Hosted tests and an arm64 Android build cover this harness. **Target-device
resize, rotation and driver-result evidence remain pending** until a device
report proves them. SUBOPTIMAL / OUT_OF_DATE claims require nonzero respective
driver-result counters. OUT_OF_DATE recovery additionally requires an
explicit recovery-success count. The timed pause and policy self-test alone
are not hardware evidence.

## Part 1 live WSI resize-window harness

The dedicated Part 1 APK holds the same-device oldSwapchain probe at two points while the Android SurfaceView dimensions change: before vkAcquireNextImageKHR, then after vkQueueSubmit and before vkQueuePresentKHR. The report records whether the mode was enabled, the number of pauses at each point, and total pause duration. Java preserves the live-window report separately from the subsequently queued normal Surface session. This improves the chance of observing driver-returned WSI status codes; it cannot force the driver to return them.

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

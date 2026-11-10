# NYAMIST G925 R2.1 — Surface Lifecycle and WSI Session Recreation

R2.1 extends the R2.0 pixel-readback validation with Android Surface lifecycle
tracking and serialized Vulkan WSI session reconstruction.

## Execution model

- Android SurfaceHolder callbacks record surface creation, size changes,
  resizes and destruction.
- Activity pause/resume callbacks are recorded. If the Activity resumes while
  the same Surface remains available, a new WSI validation session is queued.
- The on-screen REBUILD VULKAN SURFACE / SWAPCHAIN button requests another
  explicit validation session without requiring an OS Surface destruction.
- Each request receives a copied Java Surface handle. The native validation
  function acquires its ANativeWindow reference and constructs a new Vulkan
  instance, Android VkSurfaceKHR, logical device, swapchain, rendering
  resources and presentation resources. It completes cleanup before returning.
- All native calls run on one single-thread executor. A newer Surface
  generation invalidates queued stale requests. An already-running native
  session is allowed to finish its orderly cleanup; the next valid Surface
  session runs afterward.
- Each invocation constructs a fresh WSI session. R2.1 does not yet claim an
  in-place vkCreateSwapchainKHR oldSwapchain handoff.

## Lifecycle report

The native schema-v6 report is preserved. Android adds a top-level
android_surface_lifecycle object with nested schema_version 1.

It records:

- Surface created, recreated, changed, destroyed and resize counts.
- Activity pause/resume counts and current resumed state.
- Session generation, request ID, reason, requested dimensions and result.
- Native session started, completed, successful and skipped counts.
- Active and maximum concurrent native session counts.
- Whether the session was still current when it completed.
- The serialized rebuild policy used by the Activity.

A successful serialized run should report
max_concurrent_native_sessions = 1 and native_session_active_count = 0.
Successful Vulkan resource cleanup is still required for each native session.

## Device test procedure

1. Install the R2.1 APK and open NYAMIST G925 R2.1.
2. Wait for the initial WSI session to finish and show SUCCESS.
3. Tap REBUILD VULKAN SURFACE / SWAPCHAIN once. Wait for the next SUCCESS.
4. Press Home, wait briefly, then return to the app. This records Activity
   pause/resume and automatically requests a fresh WSI session if the Surface
   is still available. Android may also destroy and recreate the Surface.
5. Rotate the device once if convenient; a size change should request another
   session. Rotate back if needed.
6. Wait for the latest session to finish, tap COPY JSON REPORT, and provide
   the complete JSON.

The device must show at least two completed native sessions, no more than one
concurrent native session, successful swapchain creation and cleanup for the
latest session, and a successful R2.0 pixel-readback check. To claim an actual
Surface recreation event, the report must show surface_destroyed_count >= 1
and surface_recreated_count >= 1. If Android keeps the same Surface while the
app is paused, the pause/resume plus manual rebuild evidence is recorded
separately and must not be mislabeled as a Surface destroy/create cycle.

## Validation boundaries

Hosted CI checks the Java/native build, shader compilation, APK packaging and
lifecycle contract smoke tests. Actual Surface destruction/recreation behavior
is device- and OS-lifecycle-dependent and requires the complete R2.1 report
from the REDMI K80 Ultra.

This milestone does not establish FPS, sustained performance, thermal/battery
behavior, physical-panel scanout, or game-engine compatibility.

Stock Android public Vulkan loader and vendor driver only. No root, direct GPU
device-node access, SELinux/kernel changes, vendor partition writes, or
driver replacement.

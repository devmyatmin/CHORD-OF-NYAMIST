# NYAMIST G925 R2.2 — Swapchain Resize and OUT_OF_DATE Recovery

R2.1 extends the R2.0 pixel-readback validation with Android Surface lifecycle
tracking and serialized Vulkan WSI session reconstruction.

## Execution model

- Android SurfaceHolder callbacks record surface creation, size changes,
  resizes and destruction.
- Activity pause/resume callbacks are recorded. If the Activity resumes while
  the same Surface remains available, a new WSI validation session is queued.
- The on-screen REBUILD VULKAN SURFACE / SWAPCHAIN button requests another
  explicit validation session without requiring an OS Surface destruction.
- Each request retains a strong Java reference to the framework-owned
  Surface object. No unsupported Surface copy constructor is used. The native
  validation function calls ANativeWindow_fromSurface and holds its own
  ANativeWindow reference for the full session.
- All native calls run on one single-thread executor. A newer Surface
  generation invalidates queued stale requests. If surfaceDestroyed arrives
  during an active native session, the callback waits for that session to
  finish Vulkan cleanup and release its ANativeWindow before returning.
  This keeps native use inside the SurfaceHolder validity interval.
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

## Current REDMI K80 Ultra result

The first R2.1 device run completed seven fresh native WSI sessions (7/7
successful, zero skipped) with max_concurrent_native_sessions=1. Pixel
readback and present-fence validation remained successful.

This run used six explicit manual rebuilds on Surface generation 1. It
reported surface_destroyed_count=0, surface_recreated_count=0 and
activity_pause_count=0. It is therefore a PASS for serialized WSI session
reconstruction, but not yet a Surface destruction/recreation or Activity
pause/resume pass. Complete the Home/return and rotation test before claiming
those lifecycle events.

See [R2.1 REDMI K80 Ultra evidence](../../docs/evidence/NYAMIST-G925-R2.1-redmi-k80-ultra-2026-11-10.md).

## R2.1 REDMI K80 Ultra hardware result — PASS

The latest supplied device report confirms six Surface creations, five
destructions and five recreations, plus five Activity pauses and six resumes.
It completed 16/16 native WSI sessions with zero skips, zero active sessions
at report time, and max_concurrent_native_sessions=1. The latest session
retained successful R2.0 pixel readback, 17/17 present-fence attaches/waits,
and successful Vulkan cleanup.

This is a Surface lifecycle and serialized WSI session HARDWARE PASS.
The report observed zero Surface resize events; resize remains separately
unvalidated. See [R2.1 hardware evidence](../../docs/evidence/NYAMIST-G925-R2.1-redmi-k80-ultra-2026-11-10.md).

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


## R2.2 — same-device swapchain handoff

R2.2 creates a replacement VkSwapchainKHR using oldSwapchain while retaining
the same VkDevice and Android VkSurfaceKHR. It re-queries surface capabilities,
format support, present modes and extent before building the replacement.

The replacement chain is then exercised with a real acquire, command-buffer
layout transition from UNDEFINED to PRESENT_SRC_KHR, queue submit, queue
present, and presentation completion wait. This probe intentionally does not
draw scene content; the R2.0 five-pixel draw/readback test continues on the
new swapchain immediately afterward.

OUT_OF_DATE is retried up to two times (three total replacement attempts).
SUBOPTIMAL causes a handoff after the current operation. Actual driver
OUT_OF_DATE/SUBOPTIMAL counts are reported separately from the deterministic
policy self-test. A run that observes neither result reports
swapchain_out_of_date_recovery_status = not_observed; this is not evidence that
the driver returned OUT_OF_DATE.

## R2.2 device test procedure

1. Install NYAMIST G925 R2.2 and run the first validation.
2. Rotate the device once so Android can change or recreate the Surface size.
3. Wait for the latest WSI session to complete and copy the full JSON report.
4. Confirm swapchain_recreation_status=success, old handle supplied and
   destroyed, logical device reused, replacement probe presentation success,
   plus the existing pixel-readback/present-fence evidence.
5. Record whether swapchain_extent_changed is true. A zero change is valid
   when Android reports a fixed/current extent; it must not be called a resize
   pass. Actual OUT_OF_DATE recovery is claimed only when the corresponding
   device result count is nonzero and recovery success is recorded.


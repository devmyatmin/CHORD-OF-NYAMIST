# NYAMIST G925 R2.1 — Surface Lifecycle and WSI Session Recreation

**Implementation status: staged for hosted validation. Target-device validation: pending.**

## Objective

Prove that the Android app can observe Surface lifecycle transitions and
reconstruct a fresh Vulkan WSI session after a Surface generation, size, or
Activity-resume event without overlapping native validation calls.

R2.1 builds on the R2.0 hardware pass. It retains the R2.0 native pixel
readback, present-fence path, GPU timestamp collection and cleanup logic.

## Implementation

- Record SurfaceHolder surfaceCreated, surfaceChanged and surfaceDestroyed.
- Count real surface recreation separately from resize and Activity
  pause/resume events.
- Record Activity onPause/onResume. On resume, queue a new WSI session when a
  previously validated Surface is still available.
- Provide an explicit rebuild control for deterministic repeat-session tests.
- Retain a strong Java reference to the framework-owned Surface object
  while a request is queued or running; do not attempt to call a nonexistent
  Surface(Surface) copy constructor. JNI obtains its own ANativeWindow
  reference with ANativeWindow_fromSurface and releases it during native cleanup.
- Run all native sessions through one single-thread executor. Requests for
  stale Surface generations are skipped before entering JNI. If
  surfaceDestroyed arrives during native work, that callback waits until the
  active JNI session has completed cleanup and released its ANativeWindow.
  The next queued generation can begin only after that callback boundary.
- Every native invocation recreates the Vulkan instance, Android surface,
  device and swapchain, then destroys them in order. This is a fresh WSI
  session per request, not an in-place oldSwapchain handoff.
- Preserve native report schema v6 and append android_surface_lifecycle
  schema v1 as a top-level JSON object.

## Lifecycle evidence fields

The nested object includes session generation/request/reason/dimensions,
current-at-completion status, Surface create/recreate/change/destroy/resize
counts, Activity pause/resume counts, manual rebuild requests, native session
started/completed/success/skipped counts, active/max concurrency, and the
rebuild policy string.

## Acceptance criteria

- Hosted core tests, Android Java compilation, native arm64 compilation,
  shader compilation and APK packaging pass.
- Device report includes the nested lifecycle object and valid JSON.
- At least two native WSI sessions complete; maximum concurrent native
  sessions is exactly one.
- Each accepted session shows successful Android surface creation,
  swapchain creation, present-fence completion, R2.0 pixel verification and
  orderly cleanup.
- For a formal Surface recreation claim, device evidence must show at least
  one surface destruction followed by a new Surface generation. An Activity
  resume or manual session rebuild alone is not labeled Surface recreation.
- Surface resize events are counted separately and cause a fresh WSI session.

## Device test procedure

Install the R2.1 APK, wait for the first SUCCESS, tap the rebuild button once,
then press Home and reopen the app. Rotate once to exercise resize handling.
Wait for the newest session, copy the complete JSON report and return it for
hardware review.

The OS may retain the Surface while the Activity is paused. In that case the
report must show Activity pause/resume and the explicitly requested session,
without asserting that surfaceDestroyed/surfaceCreated occurred.

## Measurement boundaries

R2.1 repeats the R2.0 validation workload. It is not a sustained benchmark and
does not establish FPS, thermal/battery behavior, physical-panel scanout,
full-frame correctness or game compatibility.

## Safety

Stock Android public Vulkan loader and vendor driver only. No root, direct GPU
device-node access, SELinux/kernel modifications, vendor partition writes,
or driver replacement.

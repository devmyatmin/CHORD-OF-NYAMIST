# NYAMIST G925 R2.1 — Surface Lifecycle and WSI Session Recreation

**Hosted validation: PASS. Target-device validation: PARTIAL PASS — serialized WSI session rebuild proven; Android Surface destroy/recreate still pending.**

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

## Target-device evidence — serialized WSI session PASS

The REDMI K80 Ultra returned a complete R2.1 app report. The native render
payload retains its R2.0 identity and schema-v6 fields; the R2.1 Android
wrapper appends the schema-v1 android_surface_lifecycle object. This is
intentional in the current implementation and the outer lifecycle object is
the R2.1-specific evidence.

- Device/GPU match: Mali-G925-Immortalis MC12, vendor 0x13b5, device
  0xd8300010, Vulkan device API 1.3.278; target_gpu_match=true.
- Latest request: 7, reason manual_rebuild, generation 1, extent 1202×715.
- Native WSI sessions: 7 started, 7 completed, 7 successful, 0 skipped.
- Native sessions active at report time: 0; maximum concurrent sessions: 1.
- All seven requests completed in the same Android Surface generation.
- Latest native session retained R2.0 pixel-readback success: four background
  samples exactly [53,105,153,255], triangle center [200,180,180,255].
- Present-fence path remained successful: 17 attaches, 17 waits, 16 resets;
  shutdown completion was present_fence_signaled.
- Latest native session reported successful cleanup for the readback buffer
  and memory, swapchain, device, Vulkan surface, instance and native window.
- Diagnostic GPU frame-time median: 92,538 ns; p95: 218,769 ns. Diagnostic
  CPU frame-total median: 7,074 µs; p95: 19,822 µs. These are not FPS or
  sustained-performance measurements.

**Lifecycle boundary:** surface_created_count=1, surface_destroyed_count=0,
surface_recreated_count=0, surface_resize_count=0, activity_pause_count=0,
activity_resume_count=1. Therefore this report proves repeated fresh WSI
session reconstruction and single-session serialization, but it does not
prove an Android Surface destroy/create cycle or an Activity pause/resume
cycle. The six explicit manual rebuild requests produced six additional
successful native sessions without overlap.

The next device check is to press Home and return to the app, then rotate the
device once. A formal Surface recreation pass still requires a report with
surface_destroyed_count >= 1 and surface_recreated_count >= 1. A pause/resume
pass requires activity_pause_count to increase and a subsequent
activity_resume_count to be recorded. Do not infer either event from manual
rebuilds.

See [R2.1 REDMI K80 Ultra evidence](../evidence/NYAMIST-G925-R2.1-redmi-k80-ultra-2026-11-10.md).

## Lifecycle evidence fields

The nested object includes session generation/request/reason/dimensions,
current-at-completion status, Surface create/recreate/change/destroy/resize
counts, Activity pause/resume counts, manual rebuild requests, native session
started/completed/success/skipped counts, active/max concurrency, and the
rebuild policy string.

## Hosted validation — PASS

On code head 193e10625f3c93da00c12e6a6055f40c587842f3:

- NYAMIST Core CI push Run #365: SUCCESS.
- NYAMIST Core CI PR Run #366: SUCCESS.
- R2.1 Android push workflow Run #9: SUCCESS.
- R2.1 Android PR workflow Run #10: SUCCESS.
- APK artifact from PR Run #10: id 13732041949,
  name nyamist-g925-r2.1-surface-lifecycle-apk, ZIP size 1,222,569 bytes.
- [Core CI PR Run #366](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42747556739)
- [R2.1 Android PR Run #10](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42747556448)

## Acceptance criteria

- Hosted core tests, Android Java compilation, native arm64 compilation,
  shader compilation and APK packaging pass.
- Device report includes the nested lifecycle object and valid JSON.
- At least two native WSI sessions complete; maximum concurrent native
  sessions is exactly one. This sub-criterion is now hardware-verified (7/7
  completed, maximum concurrency 1).
- Each accepted session shows successful Android surface creation,
  swapchain creation, present-fence completion, R2.0 pixel verification and
  orderly cleanup.
- For a formal Surface recreation claim, device evidence must show at least
  one surface destruction followed by a new Surface generation. This remains
  pending; the current device report records zero destroys and zero recreates. An Activity
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

# NYAMIST G925 R2.1 — Surface Lifecycle and WSI Session Recreation

**Hosted validation: PASS. Target-device validation: HARDWARE PASS.**

## Objective

Verify Android Surface lifecycle transitions and reconstruct fresh Vulkan WSI
sessions after Surface destruction/recreation, size changes, and Activity
resume events without overlapping native validation sessions.

R2.1 builds on the R2.0 hardware pass. It preserves swapchain pixel readback,
the VK_EXT_swapchain_maintenance1 present-fence path, GPU timestamp
collection, and orderly Vulkan cleanup.

## Implementation

- Record SurfaceHolder surfaceCreated, surfaceChanged and surfaceDestroyed.
- Count actual Surface recreation separately from resize and Activity
  pause/resume events.
- Record Activity onPause/onResume and queue a new WSI session when the
  Activity resumes with a valid Surface.
- Provide an explicit rebuild control for repeat-session validation.
- Retain a strong Java reference to the framework-owned Surface while the
  request is queued or running. JNI obtains its own ANativeWindow reference
  with ANativeWindow_fromSurface and releases it during native cleanup.
- Run all native sessions through one single-thread executor. Requests for
  stale Surface generations are skipped before entering JNI. If
  surfaceDestroyed arrives during native work, that callback waits until the
  active JNI session has completed Vulkan cleanup and released its
  ANativeWindow before returning.
- Every native invocation creates a fresh Vulkan instance, Android surface,
  logical device and swapchain, then destroys them in order. This is a fresh
  WSI session per request, not an in-place oldSwapchain handoff.
- Preserve the native report schema v6 and append an
  android_surface_lifecycle object with nested schema v1.

## Target-device validation — HARDWARE PASS

Validated on the REDMI K80 Ultra / Android 16 / MediaTek MT6991 /
Mali-G925-Immortalis MC12. The user supplied the complete R2.1 report.

- Target GPU match: true; vendor 0x13b5, device 0xd8300010,
  Vulkan device API 1.3.278; loader API 1.4.0.
- Android Surface generations: 6 created, 5 destroyed, 5 recreated.
- Activity lifecycle: 5 pauses, 6 resumes; Activity resumed at report time.
- Surface changes: 6; resize events: 0. No display-size change was observed
  in this run; Surface destruction/recreation was independently observed.
- Native WSI sessions: 16 started, 16 completed, 16 successful, 0 skipped.
- Native sessions active at report time: 0; maximum concurrent native
  sessions: 1.
- Manual rebuild requests: 10. Latest report was request ID 21 on generation
  6 at 1202×715, reason manual_rebuild.
- Latest native WSI session retained R2.0 pixel readback: four background
  samples exactly [53,105,153,255], triangle center [200,180,180,255];
  pixel_verification_status=success.
- Latest native session's present-fence validation completed 17 attaches,
  17 waits and 16 intermediate resets; shutdown reported
  present_fence_signaled.
- Latest native session completed 16 GPU timestamp samples. GPU frame-time
  median was 86,000 ns and p95 was 126,154 ns. CPU diagnostic frame-total
  median was 8,139 µs and p95 was 16,663 µs.
- Latest native session cleanup, including swapchain, device, surface,
  instance, pixel readback resources and native window, reported success.

The lifecycle counts establish actual Surface destruction and recreation,
not just explicit manual WSI rebuilds. The 16/16 native sessions and
max_concurrent_native_sessions=1 establish successful serialized execution
across the recorded lifecycle sequence.

## Hosted validation — PASS

Validated source head for the R2.1 APK and current implementation:
0277684cbf4a7b46eb14887c7d9d7f3db756b3a7.

- NYAMIST Core CI PR Run #371: SUCCESS.
- R2.1 Android PR workflow Run #15: SUCCESS.
- NYAMIST Core CI push Run #370: SUCCESS.
- R2.1 Android push workflow Run #14: SUCCESS.
- [Core CI PR Run #371](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42750071943)
- [R2.1 Android PR Run #15](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42750072083)

Hosted CI validates Java/native compilation, lifecycle contract tests, shader
compilation, APK packaging and core C++ tests. The Surface lifecycle counters
above are independent target-device evidence.

## Acceptance criteria

- Hosted core tests, Android Java compilation, native arm64 compilation,
  shader compilation and APK packaging pass.
- Device report contains valid native schema-v6 and Android lifecycle schema-v1
  evidence.
- At least two native WSI sessions complete; maximum concurrent native
  sessions is exactly one. **Verified: 16/16, concurrency 1.**
- Successful pixel readback, present-fence completion and orderly cleanup
  remain present in the latest session. **Verified.**
- At least one Surface destruction followed by a new Surface generation is
  required for a Surface recreation claim. **Verified: 5 destroys / 5 recreates.**
- Activity pause/resume is recorded separately. **Verified: 5 pauses / 6 resumes.**
- Surface resize is counted separately; this run observed zero resize events.

## Measurement boundaries

R2.1 repeats the controlled triangle validation workload. GPU timestamps and
CPU durations are diagnostic samples; they do not establish game FPS,
sustained performance, thermal/battery behavior, or full-scene correctness.

Pixel verification reads the acquired swapchain image before presentation;
it is not physical-panel scanout. Fresh WSI session reconstruction is not
an in-place oldSwapchain handoff and does not establish game-engine
compatibility or that Stranded Deep runs on Android.

## Safety

Stock Android public Vulkan loader and vendor driver only. No root, direct GPU
device-node access, SELinux/kernel modifications, vendor partition writes,
or driver replacement.

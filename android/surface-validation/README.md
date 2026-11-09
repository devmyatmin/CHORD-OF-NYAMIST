# NYAMIST G925 R1.6 — Android Rendering / Frame Timing Test

This Android application supplies a real SurfaceView/ANativeWindow and
validates the stock Android Vulkan presentation path on the target phone.

## What the test does

1. Receives the Activity's SurfaceView surface and obtains its native window.
2. Loads the public Android Vulkan loader through dlopen("libvulkan.so").
3. Creates the Android Vulkan surface and selects a G925 graphics+present queue.
4. Creates a FIFO swapchain and acquires one swapchain image.
5. Creates an image view, a one-color-attachment render pass and a framebuffer.
6. Records a render-pass loadOp = CLEAR with the deterministic RGBA color
   0.035, 0.14, 0.32, 1.0. This is a real image write by the GPU render pass;
   it does not require or claim a shader or graphics pipeline.
7. Submits the command buffer waiting on the acquire semaphore at
   COLOR_ATTACHMENT_OUTPUT, signals a separate present semaphore, and calls
   vkQueuePresentKHR waiting on that semaphore.
8. Waits for queue idle, records one CPU wall-clock timing sample, cleans up
   all Vulkan objects, and emits schema-v2 JSON.

## Timing interpretation

The report uses std::chrono::steady_clock to measure CPU-observed durations
for image acquisition, the vkQueueSubmit call, the vkQueuePresentKHR call,
the queue-idle wait, and the total acquire-to-idle interval. These are CPU
wall-clock durations, not GPU execution timestamps and not FPS. GPU timestamp
queries are explicitly marked not_queried; R1.6 collects one frame sample.

## Build and run

The r1.6-android-rendering.yml GitHub Actions workflow builds an arm64-v8a
debug APK and publishes the nyamist-g925-r1.6-debug-apk artifact.

1. Download app-debug.apk from the successful R1.6 Android workflow run.
2. Install and open NYAMIST G925 R1.6 Surface Test.
3. Keep the app foregrounded until its JSON report appears.
4. Tap Copy JSON report and paste the complete report into the project chat.
5. The app also writes nyamist-g925-r1.6-surface-swapchain.json under its
   app-specific external files directory and prints the path and SHA-256.

## R1.6 acceptance boundary

A hardware pass requires G925 identification, Android surface and swapchain
success, successful image acquisition, image-view/render-pass/framebuffer
creation, render-pass begin/end recording, successful queue submission and
presentation, queue idle, frame_content_written = true, one valid CPU timing
sample, and complete cleanup.

R1.6 does not execute a shader, draw geometry, query GPU timestamps, establish
sustained FPS, test thermal behavior, or prove game compatibility. Hosted CI
proves APK compilation/package creation only; the phone's JSON report is the
hardware evidence. No proprietary libraries, root, device-node access, SELinux
changes, kernel changes, or vendor partition writes are used.

# NYAMIST G925 R1.5 — Android Surface / Swapchain Test

This is a small Android application used to obtain a real `SurfaceView` and
pass its native window to Vulkan through JNI. A Termux command-line process
does not own an Android `Surface`, so a CLI-only probe cannot validate
`VK_KHR_android_surface` or a presentation swapchain.

## What the test does

1. Receives the Activity's `SurfaceView` surface.
2. Acquires an `ANativeWindow` with `ANativeWindow_fromSurface`.
3. Loads the public Android Vulkan loader at runtime.
4. Enables `VK_KHR_surface` and `VK_KHR_android_surface`.
5. Creates an Android Vulkan surface and selects a physical device with
   `VK_KHR_swapchain` and a graphics+present queue family.
6. Queries surface capabilities, formats and present modes.
7. Creates a FIFO swapchain and enumerates its images.
8. Acquires one image, records an `UNDEFINED → PRESENT_SRC_KHR` layout
   transition, submits it waiting on the acquire semaphore and signals a
   separate presentation semaphore.
9. Calls `vkQueuePresentKHR` waiting on the submitted presentation semaphore.
10. Waits for the queue, destroys command/synchronization/swapchain objects,
    releases the native window, and emits schema-v1 JSON.

No proprietary libraries are bundled. No root, device-node access, SELinux
changes, kernel changes, or vendor partition writes are performed.

## Build

The GitHub Actions workflow builds an arm64-v8a debug APK and publishes it as
the `nyamist-g925-r1.5-debug-apk` artifact.

## Run on REDMI K80 Ultra

1. Download `app-debug.apk` from the successful R1.5 Android workflow artifact.
2. Install and open **NYAMIST G925 R1.5 Surface Test**.
3. Keep the app foregrounded until the JSON report appears.
4. Tap **Copy JSON report** and paste the report into the project conversation.
5. The app also writes `nyamist-g925-r1.5-surface-swapchain.json` under its
   app-specific external files directory and prints the path and SHA-256 in logcat.

A valid R1.5 hardware pass requires `status: success`, the G925 target match,
both Android surface extensions, a graphics+present queue, successful surface
and swapchain creation, an acquired image, a recorded present-layout transition,
a successful queue submit, a signaled presentation semaphore consumed by
`vkQueuePresentKHR`, one presented image, queue idle, and successful cleanup.

## Scope boundary

R1.5 validates Android surface creation and the swapchain acquire/present
handshake. It does **not** write pixels to the acquired image, run shaders,
render a scene, measure frame time/FPS, or establish game compatibility.
`frame_content_written` is deliberately false. Those are later milestones.

# NYAMIST G925 R1.5 — Android Surface & Swapchain Validation

## Objective

Validate the stock Android presentation path on REDMI K80 Ultra / MT6991 /
Immortalis-G925 MC12 using an actual Android `SurfaceView`, rather than
pretending a Termux CLI process can manufacture a window surface.

## Implementation

- Android Activity owns a `SurfaceView`.
- JNI obtains an `ANativeWindow` from the Android `Surface`.
- Vulkan is loaded through `dlopen("libvulkan.so")` and
  `vkGetInstanceProcAddr`; there is no direct Vulkan loader dependency.
- Instance extensions: `VK_KHR_surface` and `VK_KHR_android_surface`.
- Device extension: `VK_KHR_swapchain`.
- A queue family must support both graphics and presentation.
- Surface capabilities, formats, and present modes are queried.
- A FIFO swapchain is created and its images enumerated.
- One image is acquired and transitioned from `UNDEFINED` to
  `PRESENT_SRC_KHR` with an image memory barrier.
- A queue submission waits on the acquire semaphore at
  `VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT`, records the image transition from
  `TOP_OF_PIPE` to `BOTTOM_OF_PIPE` with zero destination access, and signals
  a dedicated present semaphore; `vkQueuePresentKHR` waits on that semaphore.
- Queue idle is confirmed before command, semaphore, swapchain, device, surface,
  instance, and native-window teardown.
- The JSON report records extension, surface, queue, swapchain, acquire,
  present, and cleanup outcomes.

## Acceptance criteria

- Correct Android native window acquired and released.
- Android surface and required Vulkan extensions available.
- Physical device identifies as Immortalis-G925.
- Graphics+present queue family found.
- Surface capabilities/formats/present modes queried successfully.
- FIFO swapchain created and at least one image enumerated.
- Image acquire, present-layout transition, queue submission, and queue
  presentation all succeed.
- The present semaphore is consumed before cleanup.
- Queue idle and complete object cleanup succeed.
- No device loss.

## Evidence policy

A hosted Android APK build proves compilation/package creation only. R1.5 is
not hardware-passed until the app is run on the target phone and its JSON
report is reviewed. A Termux build cannot substitute for an Android Activity
surface.

## Deliberate non-claims

This milestone does not render pixels or execute shaders. It records only an
image layout transition to `PRESENT_SRC_KHR` and submits that transition before
presentation. It does not write frame content; `frame_content_written` remains false.
Rendering, presentation timing, FPS, thermal behavior, and game compatibility
remain out of scope.

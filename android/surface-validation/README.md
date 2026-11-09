# NYAMIST G925 R1.7 — Graphics Pipeline & Triangle Draw Validation

This Android application validates graphics-pipeline creation, shader-module
creation, a three-vertex draw call, and swapchain presentation through the
stock Android Vulkan loader on the target device.

## Rendering path

1. Receives the Activity SurfaceView and obtains an ANativeWindow.
2. Loads the public Android Vulkan loader with dlopen("libvulkan.so") and
   resolves Vulkan entry points through vkGetInstanceProcAddr and
   vkGetDeviceProcAddr.
3. Creates the Android surface, identifies the Immortalis-G925 device, and
   selects a graphics+present queue.
4. Creates a FIFO swapchain with COLOR_ATTACHMENT usage.
5. Creates an image view, one-color-attachment render pass, and framebuffer.
6. Compiles the checked-in GLSL vertex and fragment shaders to Vulkan 1.0
   SPIR-V during the native build; embeds the resulting words in a generated
   C++ header without loading a separate runtime shader file.
7. Creates vertex/fragment shader modules, an empty pipeline layout, and a
   graphics pipeline compatible with the render pass and swapchain format.
8. Records a render-pass clear, binds the graphics pipeline, and records
   vkCmdDraw(3, 1, 0, 0) using gl_VertexIndex. No vertex buffer, descriptors,
   textures, or external shader assets are required.
9. Submits the command buffer, presents the rendered swapchain image, waits
   for queue idle, records CPU-observed timings, and cleans up Vulkan objects.

## Shader build requirements

The CMake build requires the host-side glslangValidator executable and Python
3. GitHub Actions installs Ubuntu's glslang-tools package before Gradle invokes
the Android native build. The runtime APK only uses the generated SPIR-V
arrays; it does not invoke a shader compiler on the phone.

The source shaders are:
- app/src/main/shaders/triangle.vert
- app/src/main/shaders/triangle.frag

## Timing and GPU timestamp capability

R1.7 retains the R1.6 CPU wall-clock timings. It also records whether the
selected graphics queue and physical-device limits expose timestamp support.
A capability indication is not a GPU timestamp measurement. GPU timestamp
query-pool execution and multi-frame performance sampling are separate follow-up
work and must not be inferred from this milestone.

## Build and run

The r1.7-android-shader-pipeline.yml workflow builds an arm64-v8a debug APK
and uploads nyamist-g925-r1.7-debug-apk.

1. Download app-debug.apk from the successful R1.7 Android workflow run.
2. Install and open NYAMIST G925 R1.7 Triangle Draw Test.
3. Keep the app foregrounded until its JSON report appears.
4. Tap Copy JSON report and paste the complete report into the project chat.
5. The report is also written under the app-specific external files directory.

## Acceptance boundary

A hardware pass requires G925 identification, shader-module and pipeline-layout
creation, graphics-pipeline creation, render-pass begin/end, recorded pipeline
bind and three-vertex draw, successful queue submission and presentation,
queue idle, and complete cleanup.

Hosted CI proves shader compilation and Android APK packaging only. It cannot
prove that the phone's GPU accepted or executed the pipeline and draw. Actual
REDMI K80 Ultra report evidence is mandatory for hardware acceptance.

No root, device-node access, SELinux changes, kernel changes, vendor partition
writes, driver replacement, or proprietary libraries are used.

# NYAMIST G925 R1.7 — Graphics Pipeline & Triangle Draw Validation

**Status: HARDWARE PASS on REDMI K80 Ultra / MT6991 / Immortalis-G925 MC12.**

## Objective

Extend the R1.6 swapchain-image render-pass clear into an actual graphics-pipeline
execution path on the target Android device.

## Implementation

- Add minimal GLSL vertex and fragment shaders for a three-vertex colored triangle.
- Compile the shaders to Vulkan 1.0 SPIR-V during the host-side CMake build.
- Embed compiled SPIR-V words in a generated C++ header; the Android runtime does
  not compile shaders or load separate shader files.
- Resolve shader-module, pipeline-layout, graphics-pipeline, pipeline-bind, and
  draw entry points dynamically through vkGetDeviceProcAddr.
- Create vertex and fragment shader modules, an empty pipeline layout, and a
  render-pass-compatible graphics pipeline with no vertex buffer or descriptors.
- Record a deterministic render-pass clear, bind the graphics pipeline, issue
  vkCmdDraw(3, 1, 0, 0), submit, present, wait for queue idle, and clean up.
- Record the selected queue's timestampValidBits, the physical-device
  timestampComputeAndGraphics capability, and timestampPeriod. Capability
  discovery is not represented as a GPU timing measurement.

## Target-device hardware acceptance — PASS

Evidence captured on REDMI K80 Ultra / Android 16 / MT6991 / Mali-G925-Immortalis
MC12. The report identifies vendor 0x13b5, device 0xd8300010, Vulkan API 1.3.278,
and target_gpu_match=true.

- Loader, instance, Android surface, native window, physical-device discovery,
  swapchain, graphics/present queue and image acquisition: success.
- Vertex and fragment shader module creation: success.
- Pipeline layout and graphics pipeline creation: success.
- Graphics-pipeline bind and three-vertex draw command recording: success;
  draw_vertex_count=3.
- Queue submission, image presentation and queue-idle completion: success.
- Render-pass/framebuffer/image-view creation and complete Vulkan-object cleanup:
  success; failure_reason is empty.
- Frame timing: one CPU wall-clock sample from std::chrono::steady_clock.
  acquire=21 us, queue-submit call=48 us, present call=3080 us,
  queue-idle wait=429 us, acquire-to-idle total=8265 us (8.265 ms).
- The selected queue reports timestampValidBits=64,
  timestampComputeAndGraphics=true and timestampPeriod=76.92308044 ns.
  gpu_timestamp_status remains not_queried.

Machine-readable evidence:
`docs/hardware-reports/mt6991-redmi-k80-ultra-r1.7-2026-11-09.json`

## Interpretation and explicit limitations

This is a target-hardware pass for shader-module creation, graphics-pipeline
creation/binding, a three-vertex draw command submitted through the G925 graphics
queue, queue completion and presentation. It is stronger than the R1.6 clear-only
test.

The report contains no framebuffer readback or screenshot-based pixel assertion,
so it does not independently prove the displayed triangle's exact pixels. It
also does not provide a GPU timestamp-query measurement, multi-frame throughput,
sustained FPS, thermal stability, or Stranded Deep compatibility. The 8.265 ms
value is one CPU-side acquire-to-idle sample and must not be converted into an
FPS claim.

Hosted CI proves build, tests, shader compilation and APK packaging; the JSON
above is the target-device execution evidence.

No root, direct device-node access, SELinux changes, kernel changes, vendor
partition writes, or driver replacement are used.

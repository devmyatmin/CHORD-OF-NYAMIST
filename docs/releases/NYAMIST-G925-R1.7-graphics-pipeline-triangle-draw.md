# NYAMIST G925 R1.7 — Graphics Pipeline & Triangle Draw Validation

**Status: implementation in progress; target-device hardware acceptance pending.**

## Objective

Extend the R1.6 swapchain-image render-pass clear into an actual graphics-pipeline
execution path on REDMI K80 Ultra / MT6991 / Immortalis-G925 MC12.

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

## Explicit limitations

This milestone does not yet claim a GPU timestamp-query result, multi-frame
throughput, sustained FPS, thermal stability, or Stranded Deep compatibility.
The initial target-device run must confirm that the real G925 driver accepts
and executes the graphics pipeline and three-vertex draw. Hosted CI can prove
shader compilation and APK packaging only.

## Hardware acceptance

Pending REDMI K80 Ultra JSON report. Required evidence includes successful
vertex/fragment shader-module creation, pipeline layout and graphics pipeline
creation, pipeline bind and draw command recording, queue submission,
presentation, queue idle, and complete Vulkan-object cleanup.

No root, direct device-node access, SELinux changes, kernel changes, vendor
partition writes, or driver replacement are used.

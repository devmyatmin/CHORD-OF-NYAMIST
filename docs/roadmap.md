# NYAMIST Runtime Roadmap

## Current objective

Build NYAMIST as the owner of the graphics runtime and D3D11-to-Vulkan translation for the REDMI K80 Ultra / MT6991 / Immortalis-G925 MC12 target.

**Active architecture:** NYAMIST D3D11 runtime -> NYAMIST Vulkan backend -> Android Vulkan loader -> installed vendor GPU driver.

**Explicitly excluded:** PanPlay, PanVK, and DXVK as runtime dependencies. A custom Vulkan ICD/HAL and kernel driver are not part of the initial plan.

## Stage 0 — architecture migration

- [x] Confirm NYAMIST owns the D3D11 translation/runtime direction.
- [x] Separate the Android Vulkan loader/vendor driver backend from a custom ICD project.
- [ ] Remove obsolete active-path assumptions from remaining docs, build files, and component registry.
- [ ] Mark PanVK/DXVK adapters as archived evidence-only or remove them after references are migrated.
- [ ] Rework PR #1 before considering merge; it describes the previous sidecar architecture.

## Stage 1 — NYAMIST Vulkan backend

- [ ] Load Android's existing Vulkan loader from the normal NDK link path.
- [ ] Create Vulkan instance and enumerate the physical device.
- [ ] Select a graphics queue family and create a logical device.
- [ ] Create command pool/buffers and submit a minimal command buffer.
- [ ] Add NYAMIST-owned resource/memory wrappers for buffers and images.
- [ ] Add explicit synchronization and validation/error reporting.

**Pass condition:** on the REDMI K80 Ultra, NYAMIST creates its own device/queue and submits a command successfully. Device discovery alone is not enough.

## Stage 2 — native rendering and Android WSI

- [ ] Offscreen image clear and triangle rendering.
- [ ] Read back pixels and compare against deterministic expected values.
- [ ] Create Android surface and swapchain through the system loader.
- [ ] Acquire/submit/present loop with semaphores and fences.
- [ ] Handle resize, `VK_SUBOPTIMAL_KHR`, and `VK_ERROR_OUT_OF_DATE_KHR`.
- [ ] Repeat lifecycle tests across pause/resume and orientation changes.

**Pass condition:** verified pixels and stable on-screen presentation on the target phone.

## Stage 3 — D3D11 translator vertical slice

- [ ] Define NYAMIST-owned D3D11 device/context interfaces.
- [ ] Implement buffer, texture, view, sampler, and render-target object lifetimes.
- [ ] Map a minimal D3D11 graphics pipeline into Vulkan.
- [ ] Select and integrate a maintainable DXBC-to-SPIR-V compiler path; document license, Android ARM64 build, and feature limits before adoption.
- [ ] Translate one D3D11 draw call and Present into NYAMIST Vulkan commands.
- [ ] Add conformance tests for state mapping and resource lifetime.

**Pass condition:** a standalone D3D11 sample calls NYAMIST's interfaces and renders the expected image on the target device.

## Stage 4 — compatibility expansion

- [ ] Constant buffers and dynamic updates.
- [ ] Texture sampling and shader-resource views.
- [ ] Blend, rasterizer, depth/stencil and input-layout states.
- [ ] Multiple render targets and common DXGI formats.
- [ ] Shader/pipeline cache identity and invalidation.
- [ ] Multithreaded command recording after immediate-context correctness.
- [ ] Feature-level reporting based only on implemented and tested behavior.

## Stage 5 — application validation

- [ ] Validate the Windows API/ABI host separately from graphics translation.
- [ ] Run a named D3D11 sample end-to-end.
- [ ] Run one selected real game only after sample-level correctness.
- [ ] Capture at least 300 seconds, clean exit, frame-time distribution, memory, thermal and device-loss evidence.
- [ ] Repeat cold and warm runs with identical workload.
- [ ] Document unsupported features and per-title limitations.

## Release gate

Do not claim D3D11 game compatibility or production readiness until a named application has repeatable, sustained, visually verified rendering on the target device.

## Technical boundary

Android's Vulkan loader and the vendor GPU driver are the hardware backend in the initial design. NYAMIST owns the layer above them. Writing a replacement Vulkan ICD/HAL is a separate, much larger project and must not be confused with implementing a D3D11 translation runtime.

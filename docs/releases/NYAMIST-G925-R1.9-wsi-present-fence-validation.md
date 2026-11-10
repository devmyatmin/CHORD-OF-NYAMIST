# NYAMIST G925 R1.9 — WSI Presentation Completion & Present-Fence Validation

**Implementation, hosted CI, and REDMI K80 Ultra target-device validation: PASS.**

## Objective

Harden the Android Vulkan window-system integration (WSI) lifetime path after
R1.8 by detecting and, when supported, using
`VK_EXT_swapchain_maintenance1` presentation fences.

R1.8 remains accepted for its corrected 17-frame GPU timestamp diagnostic.
R1.9 is a separate follow-up focused on presentation completion and safe
release of present-related resources.

## Why this milestone exists

The R1.8 implementation now allocates one presentation wait semaphore per
swapchain image and selects it by the image index returned from
`vkAcquireNextImageKHR`. This makes semaphore reuse safe after that image is
reacquired.

The base `vkQueuePresentKHR` API does not itself provide a completion fence.
Khronos documents that `vkQueueWaitIdle` / `vkDeviceWaitIdle` alone do not
formally prove that the presentation engine has finished accessing
presentation resources. `VK_EXT_swapchain_maintenance1` adds
`VkSwapchainPresentFenceInfoEXT`. Its fence signals after the queued
presentation operations complete and the presentation engine has taken
references to the objects it uses. It is a resource-lifetime signal, not proof
that the image has finished being displayed. Once all past present fences for
a swapchain signal, the associated present wait semaphores and the swapchain
can be safely destroyed.

## Implementation design

- Query the loader's supported instance API version.
- Request Vulkan 1.1 when the loader supports it, enabling the core
  `vkGetPhysicalDeviceFeatures2` query path.
- Detect the required instance dependencies:
  `VK_KHR_get_surface_capabilities2` and
  `VK_EXT_surface_maintenance1`.
- Detect the device extension `VK_EXT_swapchain_maintenance1`.
- Query `VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT` and only enable
  the device extension when the feature is reported true.
- Create one unsignaled presentation fence for the serial diagnostic.
- Chain `VkSwapchainPresentFenceInfoEXT` to every `VkPresentInfoKHR`.
- Wait for the fence after each present operation. Reset it only after the
  previous fence has signaled and before submitting the next present operation.
- Leave the final present fence signaled; only then release presentation wait
  semaphores and destroy the swapchain.
- Record extension/feature states, whether the fence path was actually used,
  fence lifecycle statuses, explicit attach/wait/reset operation counters,
  measured fence wait CPU samples, and the shutdown completion method in
  schema-v5 JSON. A complete 17-frame run expects 17 attaches, 17 successful
  waits, 16 intermediate resets, and 16 measured wait samples.

## Fallback behavior

If the target loader/driver does not expose the required instance dependency,
device extension, feature, or query path, the app does not try to use the
present-fence structure. It keeps the R1.8 per-swapchain-image semaphore
strategy and reports:

- `present_completion_mode: per_image_reacquire_fallback`
- `present_completion_guaranteed: false`
- `shutdown_presentation_completion_status:
  not_guaranteed_without_present_fence`

That result is an explicit capability outcome, not a present-fence pass.
It must not be described as formal presentation-engine shutdown completion.

## Report schema v5

Schema v5 extends the R1.8 report with loader API version, maintenance
instance/device extension status, feature-query result, present-fence
create/attach/wait/reset/destroy status, wait CPU timings, selected completion
mode, a boolean completion guarantee, and shutdown presentation-completion
status.

## Hosted CI — PASS

Validated source head: `b6ed640af6fc7a0703004d42581799b3d258f63b`.

- NYAMIST Core CI Run #331 (42742094562): SUCCESS.
- R1.9 Android push workflow Run #15 (42742094790): SUCCESS.
- R1.9 Android PR workflow Run #16 (42742100724): SUCCESS.
- APK artifact: `nyamist-g925-r1.9-debug-apk`, ID `13728884325`, size 1,211,575 bytes.
- APK ZIP SHA-256: `49bce5e9b4d96e01b1cdcc4a464c24542404e66f2c9d2b239d4b318e47d75a90`.
- Extracted APK SHA-256: `256aa31f3bfede7079187e9b299082093c635044daee5c01719350b65bd1052e`.
- [Core CI Run #331](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42742094562)
- [Android build Run #15](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42742094790)
- [Android PR validation Run #16](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42742100724)

The artifact ZIP and contained APK passed integrity checks. The APK contains
`lib/arm64-v8a/libnyamist_surface_validation.so`.

The present-fence wait uses an infinite Vulkan fence wait rather than
abandoning a still-pending present fence after a host-side timeout. This
prevents the cleanup path from treating an unsignaled fence as safe to destroy.

## Target-device validation — HARDWARE PASS

Validated on REDMI K80 Ultra / Android 16 / MT6991 / Mali-G925-Immortalis MC12.

Evidence report:
[`mt6991-redmi-k80-ultra-r1.9-2026-11-10.json`](../hardware-reports/mt6991-redmi-k80-ultra-r1.9-2026-11-10.json)

The device report confirms:

- Vulkan loader API 1.4.0 and physical-device API 1.3.278; target GPU match true.
- `VK_EXT_swapchain_maintenance1` instance dependency, device extension,
  and feature all enabled/supported.
- Present-fence path actually used; 17 fence attachments and 17 successful
  waits across 1 warm-up plus 16 measured frames.
- 16 intermediate fence resets; the final fence was left signaled, so no
  reset was required after the final present.
- 16 measured present-fence wait CPU samples; the final sample was 280 us.
- `shutdown_presentation_completion_status: present_fence_signaled`.
- Triangle draw recorded (3 vertices), frame presented, 16 GPU timestamp
  samples collected, and all listed Vulkan/native cleanup operations succeeded.
- `failure_reason` is empty.

### Timing observations

| Metric | Result |
|---|---:|
| CPU frame-total median | 7.679 ms |
| CPU frame-total p95 / max | 14.341 ms |
| GPU timestamp median | 25.846 us |
| GPU timestamp p95 / max | 78.692 us |
| GPU timestamp minimum | 15.769 us |
| Present-fence wait samples | 16 |
| Present-fence waits | 17 |

The 16 timing samples exclude the single warm-up frame; the lifecycle counters
include all 17 present operations. The present fence establishes presentation
resource-completion/lifetime safety, not proof that the image has finished
being displayed on the physical panel. These measurements do not establish
FPS, visible-pixel correctness, sustained performance, thermal or battery
behavior, or game compatibility.

## Acceptance result

- Hosted Core CI and Android arm64 APK build passed.
- Actual REDMI K80 Ultra schema-v5 report confirms extension and feature support.
- 17 fence attaches, 17 successful waits, 16 intermediate resets, and 16
  measured wait samples are present.
- Final fence remained signaled; fence, semaphore, swapchain, Vulkan device,
  surface, instance, and native-window cleanup succeeded.
- Shutdown presentation-resource completion is explicitly marked guaranteed.
- No FPS, visible-pixel correctness, sustained performance, thermal, battery,
  or game-compatibility claim is inferred from this diagnostic.

## Safety and platform boundary

Use the Android public Vulkan loader and the stock vendor driver. No root,
direct GPU device-node access, SELinux/kernel modifications, vendor partition
writes, or driver replacement.

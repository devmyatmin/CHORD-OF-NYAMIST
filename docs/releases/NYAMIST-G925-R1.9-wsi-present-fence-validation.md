# NYAMIST G925 R1.9 — WSI Presentation Completion & Present-Fence Validation

**Implementation and hosted CI: PASS. Target-device validation: PENDING.**

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
`VkSwapchainPresentFenceInfoEXT`. Its fence signals after the queued presentation
operations complete and the presentation engine has taken references to the
objects it uses. It is a resource-lifetime signal, not proof that the image has
finished being displayed. Once all past present fences for a swapchain signal,
the associated present wait semaphores and the swapchain can be safely destroyed.

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

Tested branch head: `87b9ad4919ec2d7ad491add36eedc7b6ef366401`.

- NYAMIST Core CI Run #324 (42741470642): SUCCESS.
- R1.9 Android push workflow Run #3 (42741470588): SUCCESS.
- R1.9 Android PR workflow Run #4 (42741544909): SUCCESS.
- APK artifact: `nyamist-g925-r1.9-debug-apk`, ID `13729183079`, size 1,211,244 bytes.
- APK ZIP SHA-256: `b37bf8bf19903f49673ad18728e8277f6c9dd1315cb0edc62911eb1e2a81f59a`.
- Extracted APK SHA-256: `27bd5968842739a8569c97300c9b742cf814aa220cd35d3fb4ed7a5191bc2c8c`.
- [Core CI Run #324](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42741470642)
- [Android build Run #3](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42741470588)
- [Android PR validation Run #4](https://github.com/devmyatmin/CHORD-OF-NYAMIST/actions/runs/42741544909)

The artifact ZIP and contained APK both passed ZIP integrity checks. The APK
contains the arm64-v8a native library
`lib/arm64-v8a/libnyamist_surface_validation.so`.

## Target-device validation — PENDING

No schema-v5 report has yet been returned from the REDMI K80 Ultra. CI proves
compilation and packaging, not that the MT6991 vendor driver exposes or executes
`VK_EXT_swapchain_maintenance1` present fences. The phone report must decide
between the explicit present-fence path and the honest per-image-reacquire
fallback.

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

## Target-device validation — PENDING

No schema-v5 report has yet been returned from the REDMI K80 Ultra. Hosted CI
proves compilation and packaging, not whether the MT6991 vendor driver exposes
or executes the maintenance present-fence feature.

## Acceptance criteria

- Core CI and Android arm64 APK build pass on the validated source head.
- On the target phone, the report correctly identifies whether the maintenance
  dependencies, extension, and feature are available.
- If supported: the report shows 17 fence attaches, 17 successful waits, 16
  successful intermediate resets, and 16 measured wait samples; the final
  fence remains signaled; fence/semaphore/swapchain cleanup succeeds; shutdown
  resource completion is explicitly marked guaranteed.
- If unsupported: the report records the exact unsupported/dependency state,
  uses the per-image reacquire fallback, and does not claim a formal
  presentation-completion guarantee.
- No FPS, visible-pixel correctness, sustained performance, thermal, battery,
  or game-compatibility claim is inferred from this diagnostic.

## Safety and platform boundary

Use the Android public Vulkan loader and the stock vendor driver. No root,
direct GPU device-node access, SELinux/kernel modifications, vendor partition
writes, or driver replacement.

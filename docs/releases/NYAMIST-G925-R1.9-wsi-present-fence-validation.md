# NYAMIST G925 R1.9 — WSI Presentation Completion & Present-Fence Validation

**Implementation: IN PROGRESS. Hosted CI and target-device validation: PENDING.**

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
`VkSwapchainPresentFenceInfoEXT`, allowing an application to wait for the
present operation's references and semaphore payload use to be released.

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
  every fence lifecycle status, per-frame fence wait CPU samples, and the
  shutdown completion method in schema-v5 JSON.

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

## Acceptance criteria

- Core CI and Android arm64 APK build pass.
- On the target phone, the report correctly identifies whether the maintenance
  dependencies, extension, and feature are available.
- If supported: all 17 present operations use a fence; 16 measured fence-wait
  samples are recorded; every wait succeeds; intermediate fence resets succeed;
  the final fence remains signaled; fence/semaphore/swapchain cleanup succeeds;
  shutdown completion is explicitly marked guaranteed.
- If unsupported: the report records the exact unsupported/dependency state,
  uses the per-image reacquire fallback, and does not claim a formal
  presentation-completion guarantee.
- No FPS, visible-pixel correctness, sustained performance, thermal, battery,
  or game-compatibility claim is inferred from this diagnostic.

## Safety and platform boundary

Use the Android public Vulkan loader and the stock vendor driver. No root,
direct GPU device-node access, SELinux/kernel modifications, vendor partition
writes, or driver replacement.

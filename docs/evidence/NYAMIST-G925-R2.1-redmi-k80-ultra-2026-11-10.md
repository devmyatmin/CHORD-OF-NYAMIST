# NYAMIST G925 R2.1 — REDMI K80 Ultra Device Evidence

**Result: PARTIAL HARDWARE PASS — serialized WSI session recreation passed; Android Surface lifecycle recreation pending.**

- Date: 2026-11-10
- Device: REDMI K80 Ultra
- Platform: Android 16 / API 36 / arm64-v8a
- SoC: MediaTek MT6991 / Dimensity 9400+
- GPU: Mali-G925-Immortalis MC12
- Vendor/device: 0x13b5 / 0xd8300010
- Vulkan device API: 1.3.278
- Loader: libvulkan.so / API 1.4.0
- Outer report: native R2.0 schema v6 plus android_surface_lifecycle schema v1.

## What passed

The last request was request ID 7, reason manual_rebuild, on Surface
generation 1 at 1202×715.

| Counter | Observed | Result |
|---|---:|---|
| Native sessions started | 7 | PASS |
| Native sessions completed | 7 | PASS |
| Native sessions successful | 7 | PASS |
| Native sessions skipped | 0 | PASS |
| Native sessions active at report | 0 | PASS |
| Maximum concurrent native sessions | 1 | PASS |
| Manual rebuild requests | 6 | PASS |
| Latest session current at completion | true | PASS |

Each request rebuilds a fresh WSI session: Vulkan instance, Android VkSurface,
logical device and swapchain are created and then cleaned up before the
single-thread executor proceeds to the next request. This proves sequential
session reconstruction on the existing Surface generation. It is not an
in-place oldSwapchain handoff.

The latest session retained the R2.0 render-output proof:

- Four corner pixel samples exactly matched [53,105,153,255].
- Triangle center pixel was [200,180,180,255].
- Pixel readback status: success.
- Present-fence attaches/waits/resets: 17 / 17 / 16.
- Shutdown presentation completion: present_fence_signaled.
- Vulkan cleanup and native-window release: success.

## Android lifecycle events — pending

The received counters were:

| Lifecycle event | Observed |
|---|---:|
| Surface created | 1 |
| Surface changed | 1 |
| Surface destroyed | 0 |
| Surface recreated | 0 |
| Surface resized | 0 |
| Activity resumed | 1 |
| Activity paused | 0 |

The report therefore does not yet prove a Surface destroy/create cycle or a
Home/background/foreground pause-resume sequence. Six manual rebuild requests
are not counted as Surface recreation and must not be presented as such.

## Timing context

- GPU frame-time median: 92,538 ns; p95: 218,769 ns.
- CPU frame-total median: 7,074 µs; p95: 19,822 µs.

These are 16 diagnostic samples from the triangle validation workload. They
are not game FPS, sustained performance, thermal or battery measurements.

## Next device check

1. With the R2.1 app open and the latest session complete, press Home.
2. Wait briefly, then return to the app.
3. Rotate the device once and allow the newest session to finish.
4. Copy and return the complete JSON report.

A Surface lifecycle pass requires surface_destroyed_count >= 1 followed by
surface_recreated_count >= 1. Activity pause/resume evidence requires
activity_pause_count to increment and a later activity_resume_count. If
Android retains the Surface, report the pause/resume path separately and do
not claim Surface recreation.

R2.1 remains a diagnostic validation app; this evidence does not establish
physical-panel scanout, FPS, sustained performance or game compatibility.

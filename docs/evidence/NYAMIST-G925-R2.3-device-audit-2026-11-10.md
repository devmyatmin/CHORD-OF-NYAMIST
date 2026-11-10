# NYAMIST G925 R2.3 — REDMI K80 Ultra Hardware Audit Evidence

**Date:** 2026-11-10  
**Device:** REDMI K80 Ultra · Android 16 · MT6991 · Mali-G925-Immortalis MC12  
**Report schema:** 8  
**Overall report status:** success  
**Milestone:** Vulkan 1.4 driver audit — hardware evidence PASS

## API version conclusion

| Layer | Observed value |
|---|---|
| Android public Vulkan loader | 1.4.0 |
| Requested/created instance | 1.4.0 |
| Selected physical GPU API | 1.3.278 |
| Vulkan driver ID | 9 — ARM proprietary |
| Driver name | Mali-G925-Immortalis MC12 |
| Driver build info | `v1.r49p1-03bet0.4505ac1771886c7c25f13653ad4cdff7` |
| Driver version | `205524992 / 0x0c401000` |
| Conformance version | 1.3.8.0 |

The actual GPU does **not** expose Vulkan 1.4. The report correctly records
`vulkan_1_4_support_status=not_exposed_by_physical_device` and
`vulkan_1_4_features_query_status=skipped_physical_device_api_below_1_4`.

The Vulkan 1.4 feature booleans remain false defaults because the Vulkan 1.4
feature query was skipped; they must not be read as individually tested
feature failures.

## Rendering evidence retained

- Graphics pipeline, triangle draw and swapchain present: success.
- Pixel readback: success; four background samples matched
  `[53,105,153,255]`; triangle center was `[200,180,180,255]`.
- GPU timestamp: 16 samples, median 49,115 ns, p95 71,385 ns.
- CPU frame-total: median 7,890 us, p95 16,105 us.
- Present fence: 17 attaches, 17 waits, 16 resets; shutdown fence signaled.
- Resource cleanup: success.

Timing is a short validation sample and not an FPS or sustained-performance
benchmark.

## Swapchain evidence boundaries

Same-device `oldSwapchain` handoff passed. Six replacement images were
enumerated; replacement probe acquire/submit/present/completion passed.

The extent remained 1202 x 715 before and after. Actual SUBOPTIMAL and
OUT_OF_DATE counters were all zero, with
`swapchain_out_of_date_recovery_status=not_observed`. This is a handoff pass,
not an actual resize or OUT_OF_DATE recovery pass.

The replacement probe did not render scene content. Triangle pixel readback
was verified by the primary render sequence.

## Conclusion

R2.3 successfully identified the stock Arm proprietary G925 driver and
confirmed the current physical-device API level is Vulkan 1.3.278. Loader
and instance API 1.4.0 do not upgrade the GPU driver. A Vulkan 1.4 hardware
claim is not supported by this report.

No driver UUID is reproduced in this public evidence note.

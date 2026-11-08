# PanVK read-only adapter

## Scope

PanVkAdapter consumes a normalized key=value report produced from an app-local PanProbe/PanPlay export. It records driver identity, API/GPU metadata, ICD load status, device enumeration, PanProbe counters, and optional rendering evidence.

The adapter does not open or dynamically load a Vulkan ICD, select or change the Android Vulkan loader's active driver, modify /vendor or /system, change SELinux or /dev/mali0 permissions, patch PanVK/Mesa or Direct3D translators, or manage upstream shader/pipeline caches.

## Evidence format

See configs/panvk-rc5-redmi-k80-ultra.example.env. Required fields:

- driver_name, driver_info, api_version, gpu_id;
- load_success, device_count;
- panprobe_pass, panprobe_fail, panprobe_skip.

Unknown keys are ignored for forward compatibility. Duplicate keys, malformed booleans/integers, missing required fields, non-PanVK driver identity, oversized reports, and invalid SHA-256 values are rejected.

Optional metadata includes architecture, model, build_id, sha256, gralloc_hwmodule, and conformant. Render validation may include render_present_passed, render_present_frames, test_duration_seconds, process_exit_code, device_lost_events, and sustained_test_passed.

## Validation mapping

- unavailable: load failed or no device was enumerated.
- discovered: PanVK identity/load/device enumeration is reported. PanProbe pass counts alone do not imply surface rendering.
- smoke_tested: explicit render/present evidence, non-zero frame count, process exit code 0, and an explicitly reported zero device-lost counter. A missing counter is unknown, not zero.
- sustained_tested: smoke-test conditions plus an explicit sustained-test pass and at least 300 seconds of evidence.

The sustained flag is an evidence assertion supplied by the report producer; NYAMIST does not cryptographically attest the originating test. Keep the raw PanProbe/PanPlay archive and logs alongside each normalized report.

## Current REDMI K80 Ultra evidence

The checked-in example captures the RC5 imported-driver facts previously reported: Mesa 26.3.0-devel, Vulkan API 1.4.363, GPU ID 0xd8300015, one enumerated device, and PanProbe 41/41. It deliberately leaves render/present and sustained-test fields false. Separate D3D11/D3D12 session tests are recorded in docs/panvk-dx11-k80-validation.md; attach their raw logs to a matching driver identity before promoting the adapter to a higher validation level.

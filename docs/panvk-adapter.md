# PanVK read-only adapter

## Scope

PanVkAdapter consumes a normalized key=value report produced from an app-local PanProbe/PanPlay export. It records driver identity, API/GPU metadata, ICD load status, device enumeration, PanProbe counters, and optional rendering evidence.

Use the standard-library importer to convert a ZIP export:

```sh
python3 tools/import-panprobe.py panprobe-export.zip -o panvk-report.env
./build/nyamist-cli inspect-panvk panvk-report.env
```

The importer also writes `panvk-report.env.capabilities.json`, preserving discovered D3D feature-level records (when `d3d_fl.json` exists). It reads selected ZIP members without extracting paths, checks archive/member size limits, rejects ambiguous or malformed required JSON, and validates the PanVK identity. The generated report always leaves render/present and sustained-test evidence unset.

## Scope and safety

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
- discovered: PanVK identity/load/device enumeration is reported. PanProbe pass counts and D3D feature-level availability do not imply surface rendering.
- smoke_tested: explicit render/present evidence, non-zero frame count, process exit code 0, and an explicitly reported zero device-lost counter. A missing counter is unknown, not zero.
- sustained_tested: smoke-test conditions plus an explicit sustained-test pass and at least 300 seconds of evidence.

The sustained flag is an evidence assertion supplied by the report producer; NYAMIST does not cryptographically attest the originating test. Keep the raw PanProbe/PanPlay archive and logs alongside each normalized report.

## Current REDMI K80 Ultra evidence

The RC5 imported-driver report identifies PanVK, reports a successful load and one enumerated device, GPU ID 0xd8300015, and SHA-256 62c2b3f98ca987d5ab50b6f01e2d2c1c5b3ee995fe93666aa5da9415f65c3348. The supplied feature-level evidence marks D3D11 FL11_0 (hard) and FL11_1 (soft) available; D3D11 FL12_0/12_1 and D3D12 FL12_0/12_1/12_2 requirements are missing. These are capability results, not proof that a complete game renders correctly. Keep render/present and sustained-test fields false until a matching session log proves them.

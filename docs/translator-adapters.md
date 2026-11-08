# DXVK and VKD3D-Proton read-only adapters

## Purpose

The translator adapters normalize external DXVK and VKD3D-Proton evidence into the NYAMIST component contract. They inspect reports only. They do not load or launch translators, rewrite configuration, intercept Vulkan calls, or access upstream-owned shader/pipeline caches.

## Stable identities

| Adapter | Component ID | API |
|---|---|---|
| DxvkAdapter | dxvk | d3d11 |
| Vkd3dProtonAdapter | vkd3d-proton | d3d12 |

An adapter rejects reports whose component ID or API does not match its fixed identity.

## Normalized report format

Reports use UTF-8 key=value lines. Blank lines and lines beginning with # are ignored. Duplicate keys, malformed lines, invalid typed values, reports larger than 64 KiB, and lines larger than 4 KiB are rejected. Unknown optional keys are ignored.

Required fields:
- translator_id
- api
- translator_version
- device_created

Optional fields:
- evidence_source
- max_feature_level
- selected_feature_level
- supported_feature_levels (comma-separated)
- shader_models (comma-separated; separate from D3D feature levels)
- render_present_passed
- frame_count
- duration_seconds
- present_calls
- present_failures
- process_exit_code
- device_lost_events
- sustained_test_passed

## Validation rules

- A valid translator identity/version is classified as discovered.
- smoke_tested requires device creation, explicit render/present success, non-zero frame count and duration, at least one successful Present call, zero Present failures, normal process exit (0), and zero device-lost events.
- sustained_tested additionally requires an explicit sustained-test pass and at least 300 seconds.
- A sustained-test flag cannot bypass the duration threshold.
- Device creation, log messages, shader-model reports, or a successful process exit alone do not prove rendering correctness.

D3D12 shader-model messages (for example SM 6.6–6.8) are recorded separately from D3D12 feature levels. They must never be used to infer FL12_0/12_1/12_2 support.

## DXVK D3D11 sample

    translator_id=dxvk
    api=d3d11
    translator_version=3.1.1-5-arm64ec
    device_created=true
    max_feature_level=11_1
    selected_feature_level=11_1
    supported_feature_levels=10_0,10_1,11_0,11_1
    shader_models=5_0
    render_present_passed=true
    frame_count=27508
    duration_seconds=20
    present_calls=27508
    present_failures=0
    process_exit_code=0
    device_lost_events=0

This is a schema example only. Present counters must come from actual test instrumentation; they must not be synthesized from frame count.

## VKD3D-Proton D3D12 sample

    translator_id=vkd3d-proton
    api=d3d12
    translator_version=2.14-arm64ec
    device_created=true
    max_feature_level=11_1
    selected_feature_level=11_1
    supported_feature_levels=11_0,11_1
    shader_models=6_6,6_7,6_8
    render_present_passed=true
    frame_count=19003
    duration_seconds=20
    present_calls=19003
    present_failures=0
    process_exit_code=0
    device_lost_events=0

The version and Present counters above illustrate the schema; an exporter must replace them with values captured from the exact installed runtime/test session.

## Ownership and safety

DXVK and VKD3D-Proton retain ownership of their binaries, settings, logs, and caches. NYAMIST stores only normalized evidence and its own compatibility/profile metadata. The adapters remain read-only.

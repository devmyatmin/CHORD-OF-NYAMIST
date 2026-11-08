# NYAMIST module contracts

## Common adapter contract

Every adapter is read-only and returns a structured component report containing:
- stable component ID and kind;
- observed version and origin;
- readiness level;
- reported capabilities;
- diagnostics and evidence references.

A probe must be bounded and must not launch games, load/select a driver, mutate system configuration, or alter external component files.

## Current D3D11 stack

| Component ID | Owner | NYAMIST responsibility |
|---|---|---|
| dxvk | DXVK upstream | Inspect D3D11/DXGI version, capabilities and imported session evidence |
| panvk | Mesa / Android PanVK kbase project | Inspect ICD identity, Vulkan device, capabilities and PanProbe report |
| system-vulkan | Device vendor | Record stock driver as fallback |
| device-facts | NYAMIST adapter | Validate Android model, API, ABI, kernel and hardware facts |
| session-monitor | NYAMIST | Validate timestamps, sources, telemetry ranges and freshness |

## Evidence rules

- PanVK identity/load/device enumeration is **discovered**, not proof of surface rendering.
- PanProbe pass counts are recorded but do not prove all Vulkan features or game compatibility.
- DXVK feature-level availability is capability evidence, not render evidence.
- Smoke-test promotion requires an explicit successful render/Present result, non-zero frames, normal process exit, and an explicitly reported zero device-loss count.
- Sustained-test promotion additionally requires an explicit sustained-test pass and at least 300 seconds.
- Missing counters are unknown, never treated as zero.

## Session and telemetry contract

DeviceSessionSnapshot combines a validated device/Vulkan report with typed telemetry. Each observation has a source and timestamp. The aggregator rejects future, malformed, out-of-range and conflicting readings; stale readings remain diagnostic data and do not enter current policy inputs. Missing telemetry is not synthesized.

## Pipeline Gate

PipelineGate consumes a RuntimePlan and immutable DeviceSessionSnapshot. It blocks stale/future sessions, missing Vulkan device enumeration, and missing runtime prerequisites. Missing optional telemetry produces warnings. A smoke-test allowance never implies production readiness.

## Profile and cache contract

NYAMIST persists only its own versioned device/session/compatibility evidence and validated recommendations. Cache identity includes the relevant device, Vulkan driver, translator, workload and policy versions. FNV-1a is an identifier, not cryptographic authentication.

DXVK owns its shader/pipeline cache. PanVK/Mesa owns its driver cache. NYAMIST does not inspect, rewrite, delete, or duplicate those caches.

## Advisor contract

The current advisor is deterministic policy code, not a local AI model. It emits bounded proposals with evidence and rollback values. Automatic application is disabled. Any future model must remain subordinate to deterministic capability, safety, freshness, and A/B checks.

## Deferred interfaces

A read-only VKD3D-Proton adapter remains in the codebase for future experiments, but D3D12 is not part of the current runtime acceptance target.

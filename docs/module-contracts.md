# NYAMIST module contracts

## Common adapter contract

Every adapter is read-only and returns a structured component report containing:
- stable component ID and kind;
- observed version and origin;
- readiness level;
- reported capabilities;
- diagnostics and evidence references.

A probe must be bounded and must not launch games, load/select a driver, mutate system configuration, or alter external component files.

## Active G925 R1 architecture

| Component ID | Owner | NYAMIST responsibility |
|---|---|---|
| hardware-discovery | NYAMIST | Read Android platform identity and query physical Vulkan devices through the public loader |
| system-vulkan | Device vendor | Stock graphics driver baseline and fallback |
| nyamist-g925-icd | NYAMIST (future milestone) | Original driver implementation; not yet available or claimed |
| dxvk | DXVK upstream | Inspect D3D11/DXGI version, capabilities and imported session evidence |
| device-facts | NYAMIST adapter | Validate Android model, API, ABI, kernel and hardware facts |
| session-monitor | NYAMIST | Validate timestamps, sources, telemetry ranges and freshness |
| legacy-panvk-importer | Historical evidence | Parse archived PanVK reports only; not an active driver dependency |

## Vulkan capability inventory contract

- Extension enumeration, core feature support, and enabled features are distinct states. An extension name never substitutes for a queried feature bit.
- Vulkan 1.0 features are queried through the Vulkan 1.0 command; core 1.1–1.3 feature structures are chained only when the physical device API version supports them.
- Device limits are observations from physical-device properties. They are not tuning recommendations and do not guarantee workload performance.
- Memory types retain heap indices and raw property flags. Heap capacities are driver-reported capacities, not free VRAM or guaranteed process allocations.
- Queue and format masks retain raw values alongside decoded symbolic names.
- Query failure and unavailable states must remain explicit; missing data is never filled with assumed support.

## Evidence rules

- Vulkan physical-device enumeration is discovered, not proof of surface rendering.
- Legacy PanProbe pass counts do not validate the NYAMIST implementation.
- DXVK feature-level availability is capability evidence, not render evidence.
- Smoke-test promotion requires explicit successful render/Present evidence, non-zero frames, normal process exit, and an explicitly reported zero device-loss count.
- Sustained-test promotion additionally requires an explicit sustained-test pass and at least 300 seconds.
- Missing counters are unknown, never treated as zero.
- A hardware heap size is not free VRAM or a guaranteed per-process allocation.

## Session and telemetry contract

DeviceSessionSnapshot combines a validated device/Vulkan report with typed telemetry. Each observation has a source and timestamp. The aggregator rejects future, malformed, out-of-range and conflicting readings; stale readings remain diagnostic data and do not enter current policy inputs. Missing telemetry is not synthesized.

## Pipeline Gate

PipelineGate consumes a RuntimePlan and immutable DeviceSessionSnapshot. It blocks stale/future sessions, missing Vulkan device enumeration, and missing runtime prerequisites. Missing optional telemetry produces warnings. A smoke-test allowance never implies production readiness.

## Profile and cache contract

NYAMIST persists only its own versioned device/session/compatibility evidence and validated recommendations. Cache identity includes the relevant device, Vulkan driver, translator, workload and policy versions. FNV-1a is an identifier, not cryptographic authentication.

DXVK owns its shader/pipeline cache. Vendor Vulkan drivers own their driver caches. NYAMIST does not inspect, rewrite, delete, or duplicate those caches.

## Advisor contract

The current advisor is deterministic policy code, not a local AI model. It emits bounded proposals with evidence and rollback values. Automatic application is disabled. Any future model must remain subordinate to deterministic capability, safety, freshness, and A/B checks.

## Deferred interfaces

A read-only VKD3D-Proton adapter remains in the codebase for future experiments, but D3D12 is not part of the R1 acceptance target. A NYAMIST-owned Vulkan ICD is a separate unimplemented milestone.

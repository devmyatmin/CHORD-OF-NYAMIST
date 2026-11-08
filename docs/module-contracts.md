# NYAMIST module contracts

## Common adapter contract

Every integration adapter implements IComponentProbe and returns a ComponentReport containing:

- stable component ID;
- component kind;
- detected version and origin;
- validation level;
- observed capabilities;
- diagnostics and evidence references.

A probe must be read-only and bounded in time. It must not launch a game, mutate system configuration, load a driver, or change another component's files.

## Stable component IDs

| ID | Kind | External owner | Initial scope |
|---|---|---|---|
| dxvk | translator | DXVK upstream | D3D11/DXGI version and session report |
| vkd3d-proton | translator | VKD3D-Proton upstream | D3D12 version and feature-level report |
| panvk | Vulkan driver | Mesa / Android PanVK kbase project | ICD identity, API version, GPU and driver report |
| system-vulkan | Vulkan driver | Device vendor | Stock ICD as a safe fallback |
| ai-advisor | advisor | NYAMIST | Recommendation engine identity and policy version |
| session-monitor | monitor | NYAMIST | Session telemetry availability |

## PanVK adapter

PanVKAdapter consumes a normalized, read-only key=value report exported from an app-local PanProbe/PanPlay session. Required fields identify the driver, Vulkan API, GPU ID, load status, enumerated device count, and PanProbe pass/fail/skip counters.

Optional fields include architecture/model, build ID, binary SHA-256, gralloc availability, conformance status, and explicit render/present evidence. Unknown optional keys are ignored; duplicate keys and malformed required values are rejected.

ICD load and device enumeration map to discovered, not smoke_tested. A PanProbe pass count by itself does not establish Android surface presentation. smoke_tested requires explicit render/present evidence, non-zero frames, normal process exit, and zero device-lost events. sustained_tested additionally requires an explicit sustained-test pass and at least 300 seconds. See docs/panvk-adapter.md.

The adapter never loads the ICD, selects a Vulkan driver, modifies Android system files or permissions, or manages PanVK/Mesa caches. The stock vendor driver remains untouched.

## DXVK and VKD3D-Proton adapters

DxvkAdapter consumes a normalized D3D11/DXVK report; Vkd3dProtonAdapter consumes a normalized D3D12/VKD3D-Proton report. Both enforce stable component identity and API ownership. Their parser rejects duplicate keys, malformed typed values, and oversized input while ignoring unknown optional keys.

Device creation and reported feature levels remain discovered evidence. Smoke-test promotion requires explicit render/present success, frame/time counts, at least one successful Present call, explicit zero Present-failure status, process exit code 0, and explicitly reported zero device-lost events. Sustained validation additionally requires an explicit pass and at least 300 seconds. Shader-model capability strings are stored separately from D3D feature levels.

See docs/translator-adapters.md for the normalized report schema and CLI commands.

## Runtime selection

- D3D11 requests require dxvk plus the selected Vulkan driver.
- D3D12 requests require vkd3d-proton plus the selected Vulkan driver.
- DXVK and VKD3D-Proton are never treated as interchangeable.
- PanVK is selected only when its app-local ICD identity is positively observed.
- A missing PanVK report must not silently claim PanVK is active; the selected fallback must be explicit.

## AI advisor contract

The advisor receives structured, privacy-minimized observations such as GPU architecture, driver/API versions, shader workload class, frame-time percentiles, memory pressure, and validation history.

It returns a proposal with:
- a policy ID and version;
- the proposed setting and bounded value;
- supporting evidence;
- expected benefit and risk;
- rollback value.

The deterministic validation gate checks capability support, safety bounds, profile freshness, and A/B regression evidence. The advisor cannot directly write environment variables, driver settings, cache files owned by upstream components, or kernel controls.

## Cache contract

NYAMIST cache keys must include the NYAMIST schema/policy version and all relevant device, driver, translator, and workload identifiers. Cache entries are invalidated when any compatibility-relevant identity changes. FNV-1a fingerprints are identifiers only, not cryptographic integrity checks.

## Versioning and compatibility

Adapters must tolerate unknown optional fields and report unknown versions explicitly. A component version mismatch is a diagnostic, not permission to guess a compatible behavior. Breaking adapter contracts require a schema version bump.


## Device session and telemetry

The Device Facts and Vulkan Capability adapters consume the same bounded,
schema-v1 read-only report. Both are monitor components; Vulkan physical-device
enumeration is discovered evidence and never implies render/present success.

DeviceSessionSnapshot combines the validated report with a TelemetrySnapshot.
The typed TelemetryAggregator requires per-reading timestamps and source
identities, applies metric-specific bounds, rejects future readings, and
separates stale readings from fresh policy inputs. Missing metrics remain
absent. Equal-timestamp conflicting values are omitted. The snapshot is
immutable after construction and is an in-memory NYAMIST-owned contract only;
it is not a shared-memory or driver ABI. See docs/session-snapshot.md.

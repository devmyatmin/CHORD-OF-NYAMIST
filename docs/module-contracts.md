# NYAMIST module contracts

## Common adapter contract

Every integration adapter implements `IComponentProbe` and returns a `ComponentReport` containing:

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
| `dxvk` | translator | DXVK upstream | D3D11/DXGI version and session report |
| `vkd3d-proton` | translator | VKD3D-Proton upstream | D3D12 version and feature-level report |
| `panvk` | Vulkan driver | Mesa / Android PanVK kbase project | ICD identity, API version, GPU and driver report |
| `system-vulkan` | Vulkan driver | Device vendor | Stock ICD as a safe fallback |
| `ai-advisor` | advisor | NYAMIST | Recommendation engine identity and policy version |
| `session-monitor` | monitor | NYAMIST | Session telemetry availability |

## Runtime selection

- D3D11 requests require `dxvk` plus the selected Vulkan driver.
- D3D12 requests require `vkd3d-proton` plus the selected Vulkan driver.
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

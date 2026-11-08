# Architecture

## Responsibilities

| Component | Responsibility | Must not do |
|---|---|---|
| Wine / Proton + FEX / Box64 | Windows API and CPU-architecture compatibility | Pretend to be a D3D GPU driver |
| DXVK | Translate D3D9/10/11 and DXGI operations to Vulkan | Depend on NYAMIST for rendering correctness |
| VKD3D-Proton | Translate D3D12 operations to Vulkan | Be treated as interchangeable with DXVK |
| Android Vulkan loader | Select and load the intended Vulkan ICD | Silently replace the stock driver |
| PanVK kbase | Implement Vulkan on the vendor Mali kbase kernel interface | Replace the kernel driver |
| NYAMIST core | Discover components, collect profiles, produce advisory plans, validate metadata | Replace translators, Vulkan, or GPU rendering |
| NYAMIST AI advisor | Analyze structured telemetry and suggest bounded policy changes | Emit unchecked commands or block every frame |

## Execution paths

- D3D11: D3D11 game → Wine/FEX/Box64 → DXVK → Android Vulkan loader → selected ICD → Mali kbase → Immortalis-G925
- D3D12: D3D12 game → Wine/FEX/Box64 → VKD3D-Proton → Android Vulkan loader → selected ICD → Mali kbase → Immortalis-G925

PanVK is a Vulkan implementation, not a Direct3D translator. DXVK and VKD3D-Proton own their respective Direct3D translation paths. NYAMIST is a sidecar orchestration, profiling, validation, and metadata layer, not a GPU command-stream replacement or per-draw-call interceptor.

## Modular runtime

The Core exposes a common component-probe contract and registry. Separate adapters inspect DXVK, VKD3D-Proton, PanVK, the stock Vulkan ICD, and device/session telemetry. The Runtime Coordinator evaluates the required translator and selected Vulkan driver for a requested API.

The coordinator is advisory. It does not launch games, dynamically load ICDs, patch upstream components, or alter Android system configuration. A discovered component may be eligible for a controlled smoke test; production readiness requires sustained-test evidence from both translator and driver.

## Android and Termux boundary

Termux is used for source management, portable core builds, and read-only device probing. Android application sandboxing may prevent a Termux shell from opening or enumerating /dev/mali0; that does not by itself mean the GPU is unsupported. Validate the ICD through its intended Android app/launcher loader path.

Do not chmod the Mali device node, flash a kernel, overwrite vendor Vulkan libraries, or weaken SELinux. Keep the stock Vulkan driver available as a rollback path.

## Cache boundaries

- DXVK owns its shader and pipeline caches.
- VKD3D-Proton owns its own pipeline/shader cache.
- PanVK/Mesa owns its driver caches.
- NYAMIST stores only its own versioned device/game profiles, compatibility observations, recommendations, and validation evidence.

Cache identity must include source/workload fingerprint, source and target API, GPU architecture, Vulkan API version, driver version, translator version, and NYAMIST schema/policy version. A relevant version change causes a cache miss rather than unsafe reuse.

The current FNV-1a 64-bit fingerprint is deterministic naming only. It is not cryptographic and must never be used for signatures, trust, or executable integrity.

## AI boundary

A future local AI implementation consumes compact, privacy-minimized structured features, not raw frame data. Its output is a proposal. A deterministic validator checks every proposal against probed capabilities, safety bounds, profile freshness, and A/B regression evidence before it can be cached or enabled.

The shipping frame path remains deterministic: load a validated profile, use documented backend settings, fall back safely if stale or missing, and record telemetry for later analysis.

## Device probing

Collect actual runtime data rather than inferring from the SoC/GPU marketing name:

- Android API level and ABI;
- Vulkan instance/device API version;
- device name and vendor/device identifiers;
- available/enabled device extensions and relevant feature bits;
- driver name, driver info, and driver version;
- app-local ICD identity and kbase interface visibility;
- Wine/FEX/Box64, DXVK, and VKD3D-Proton versions;
- game executable architecture and required D3D feature levels.

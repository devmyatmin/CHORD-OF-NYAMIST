# Architecture

## Responsibilities

| Component | Responsibility | Must not do |
|---|---|---|
| Wine + Box64 | Windows API and x86-64 application compatibility | Pretend to be a D3D11 GPU driver |
| DXVK | Translate D3D11/DXGI operations to Vulkan | Depend on NYAMIST for correctness |
| Android Vulkan driver | Expose Vulkan capabilities and submit work to the GPU | Be assumed compatible from the phone name alone |
| NYAMIST core | Collect profiles, produce advisory plans, cache validated metadata | Replace D3D11, Vulkan, DXVK, or GPU rendering |
| Future local AI advisor | Analyze telemetry and suggest offline policy changes | Emit unchecked runtime commands or block every frame |

## DX11 execution path

The first supported graphics route is:

`D3D11 game → Wine/Box64 → DXVK → Vulkan driver → GPU`

DXVK is the D3D11-to-Vulkan backend. A D3D11 game will still fail if the Vulkan driver lacks a required feature, the translation runtime is incompatible, the game relies on unsupported behavior, or the Android execution environment cannot run the game's CPU architecture.

## Cache boundaries

DXVK already manages shader/pipeline cache behavior. NYAMIST must not shadow or corrupt those files.

The initial NYAMIST cache stores only NYAMIST-owned data, such as:
- validated device/game compatibility observations;
- optimization recommendations and their evidence;
- future translator-specific metadata, once a stable integration API exists.

Cache identity is versioned by source fingerprint, source API, target API, GPU architecture, Vulkan API version, driver version, and NYAMIST policy version. A driver or policy change therefore causes a cache miss rather than unsafe reuse.

The current fingerprint uses FNV-1a 64-bit for deterministic cache naming. It is not cryptographic and must never be used for signatures, trust, or executable integrity.

## AI boundary

A future `IOptimizationAdvisor` implementation may run a small local model. It should operate on compact structured features, not raw frame data. Its output is a proposal. A deterministic validator must check every proposal against probed Vulkan capabilities and regression tests before it can be cached or enabled.

The shipping frame path should remain deterministic:
1. load a validated profile/cache entry;
2. use the backend's documented settings;
3. fall back safely if the profile is missing or stale;
4. record telemetry for later analysis.

## Device probing

Before choosing a driver profile, collect actual runtime data:
- Vulkan instance/device API version;
- device name and vendor/device identifiers;
- enabled and available device extensions;
- relevant feature bits;
- driver name and driver version;
- Android build fingerprint and ABI;
- game executable architecture and D3D feature requirements.

Do not infer those values from "Dimensity 9400+" alone.

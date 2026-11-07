# Architecture

## Responsibilities

| Component | Responsibility | Must not do |
|---|---|---|
| Wine / FEX / Box64 | Windows API and CPU-architecture compatibility | Pretend to be a D3D11 GPU driver |
| DXVK | Translate D3D11/DXGI operations to Vulkan | Depend on NYAMIST for correctness |
| Android Vulkan loader | Select and load the intended Vulkan ICD | Silently replace the stock driver |
| PanVK kbase | Implement Vulkan on the vendor Mali kbase kernel interface | Replace the kernel driver |
| NYAMIST core | Collect profiles, produce advisory plans, cache validated metadata | Replace D3D11, Vulkan, DXVK, or GPU rendering |
| Future local AI advisor | Analyze telemetry and suggest offline policy changes | Emit unchecked runtime commands or block every frame |

## DX11 direct-hardware execution path

`D3D11 game → Wine/FEX/Box64 → DXVK → Android Vulkan loader → PanVK kbase → vendor mali_kbase (/dev/mali0) → Immortalis-G925`

PanVK is a Vulkan implementation. It is not a Direct3D 11 driver. DXVK remains the D3D11-to-Vulkan translation layer. PanVK's kbase backend submits Vulkan work through the phone's existing vendor kernel driver. NYAMIST is a sidecar policy/profile/cache component, not a GPU command-stream replacement.

The upstream Android PanVK kbase project lists Immortalis-G925 in its supported 5th-generation CSF GPU matrix. A separate G925 tablet test is useful evidence, but the exact REDMI K80 Ultra firmware and kbase interface must still be tested on-device.

## Android and Termux boundary

Termux is used for source management, portable core builds, and read-only device probing. Android application sandboxing may prevent a Termux shell from opening or enumerating `/dev/mali0`; that does not by itself mean the GPU is unsupported. Validate the ICD through its intended Android app/launcher loader path.

Do not chmod the Mali device node, flash a kernel, or overwrite vendor Vulkan libraries. Keep the stock Vulkan driver available as a rollback path.

## Cache boundaries

DXVK already manages shader/pipeline cache behavior. NYAMIST must not shadow or corrupt those files.

The initial NYAMIST cache stores only NYAMIST-owned data:
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

Collect actual runtime data:
- Android API level and ABI;
- Vulkan instance/device API version;
- device name and vendor/device identifiers;
- enabled and available device extensions;
- relevant feature bits;
- driver name and driver version;
- kbase interface visibility and launcher ICD path;
- game executable architecture and D3D feature requirements.

Do not infer these values from the SoC or GPU marketing name alone.

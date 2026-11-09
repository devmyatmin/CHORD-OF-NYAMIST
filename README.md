# CHORD OF NYAMIST

**THE CREATIVE INTELLIGENCE UNIVERSE — V1**

> *Every Signal Has a Story.*

**Created by AMGNYA**

---

# NYAMIST Immortalis-G925 MC12 Direct Hardware

**The G925-only Android Vulkan driver initiative of CHORD OF NYAMIST.**

| Target | Specification |
|---|---|
| GPU | Arm Immortalis-G925 MC12 |
| SoC | MediaTek MT6991 / Dimensity 9400+ |
| Primary device | REDMI K80 Ultra · 25060RK16C |
| Android | Android 16 · API 36 |
| ABI | arm64-v8a |
| Kernel interface | Existing vendor Mali kbase / CSF path |
| Driver direction | Original NYAMIST G925 Native Graphics Core (independent implementation; research/build phase) |
| Distribution target | Winlator-compatible WCP catalog/package where supported; ZIP driver-manager route where required |

## Project introduction

**NYAMIST Immortalis-G925 MC12 Direct Hardware** is the first focused hardware-engineering release line under CHORD OF NYAMIST.

The goal is to design and implement an original, device-specific Android graphics stack for the **Immortalis-G925 MC12**. The active direction is the **NYAMIST G925 Native Graphics Core**: NYAMIST-owned userspace components, hardware discovery, Vulkan integration, configuration, telemetry, validation, and runtime packaging. The stock MediaTek/ARM driver remains the known-good baseline and reversible fallback while the independent implementation is developed. No custom NYAMIST Vulkan ICD is claimed to exist until source code is built and device-tested.

This is a **single-GPU project**, not a universal driver collection. V1 targets the Immortalis-G925 MC12 on MediaTek MT6991. Adreno, Turnip, unrelated Mali generations, and generic multi-device claims are outside this release target.

### What “Direct Hardware” means here

The intended rendering path is:

```text
Windows D3D11 application
        |
        v
Wine + Box64 / FEX execution layer
        |
        v
DXVK (D3D11 -> Vulkan)
        |
        v
Android Vulkan loader / app-local ICD selection
        |
        v
NYAMIST G925 Native Graphics Core
        |
        v
Existing vendor mali_kbase kernel interface (/dev/mali0)
        |
        v
Immortalis-G925 MC12 GPU
```

“Direct Hardware” means a Vulkan userspace driver path that submits work through the device's existing kernel driver. It does **not** mean bypassing the Android kernel, directly controlling undocumented GPU registers, replacing the vendor kernel module, or modifying protected Android/vendor partitions.

NYAMIST's profiler, device profiles, evidence validator, and advisor operate beside the graphics path. They are not inserted into the per-draw-call or Present hot path.

## Winlator package and configuration plan

The release line is designed around one G925 driver build with compatible distribution adapters—not separate GPU driver variants.

### WCP / downloadable contents

For Winlator-family clients that support the required custom renderer-driver component type, the planned distribution is:

- A versioned `.wcp` component package.
- A `profile.json` manifest describing the package and its component type.
- A `contents.json` catalog entry with a stable component ID, display name, version name/code, channel, and release asset URL.
- The G925 userspace Vulkan ICD, its required userspace dependencies, device profile, installation notes, and SHA-256 checksums.

WCP support is **client/fork-specific**. The client must understand the relevant renderer-driver component type and installation mechanism. A WCP archive is a delivery container; it does not itself create a GPU driver or grant hardware access.

### ZIP driver-manager route

Where a compatible Android app or Winlator fork expects a driver ZIP / AdrenoTools-style package instead of WCP, the same validated G925 driver build may be distributed through that supported import path. The package format does not change the driver implementation or expand hardware support.

**No universal Winlator compatibility is claimed.** Import support, loader integration, and driver selection must be tested against each named client build before it is listed as supported.

### Planned configuration inputs

The package/profile and runtime test harness are intended to accept or record the following inputs:

| Input group | Values / behavior |
|---|---|
| Device match | SoC, GPU model, vendor/device ID, GPU architecture, MC count; discovered identity is read-only and cannot be spoofed by a profile |
| Android platform | Android API level, arm64-v8a ABI, kernel build, kbase/CSF compatibility information |
| Driver package | Driver build ID, Mesa/PanVK revision, package format, manifest version, SHA-256 |
| Vulkan capabilities | API version, extension list, feature bits, queue families, format support; only capabilities actually queried from the loaded device may be exposed |
| Loader / client | Android Vulkan loader, selected app-local ICD, Winlator fork/version, supported WCP component type or ZIP import path |
| Windows translation | Wine/Proton layer, Box64/FEX layer, DXVK version and D3D11 feature-level requirements; VKD3D-Proton/DX12 remains a later gated path |
| Rendering | Output resolution, surface format, present mode, VSync/frame cap, swapchain image count, and surface compatibility where the client exposes them |
| Memory / cache | Conservative memory budget, cache limits, cache location, and cleanup policy; reported heap size is never treated as free memory |
| Diagnostics | Validation level, debug logging, PanProbe, Vulkan smoke test, DXVK log capture, session ID, and evidence export |
| Safety / fallback | App-local driver selection, stock system-driver fallback, package verification, rollback status, and install/uninstall result |

Runtime options such as DXVK environment variables are separate from the GPU driver's hardware capabilities. They may be stored as test profiles, but must not be used to force unsupported Vulkan extensions or pretend that an unimplemented feature exists.

## Confirmed REDMI K80 Ultra hardware record

Device-side evidence recorded on **2026-11-08**:

- Physical GPU: **Mali-G925-Immortalis MC12**.
- SoC: **MediaTek MT6991**.
- Stock Android driver: ARM proprietary Mali driver, version **49.1.0**.
- Stock-driver Vulkan device API: **1.3.278**.
- Android Vulkan loader: **1.4.368**. This is the loader version, not the physical device's API version.
- ABI: **arm64-v8a**.
- Kernel: **6.6.118-android15**.
- Stock-driver reported device ID: `0xD8300010`.

The device also has a **legacy PanVK RC5 research record** from an earlier feasibility experiment. This is historical evidence only; PanVK is no longer the selected implementation or release dependency:

- PanVK loaded successfully in the PanPlay test path.
- One Vulkan physical device was enumerated.
- The imported PanVK report recorded GPU ID `0xd8300015`.
- PanProbe reported **41 passed, 0 failed, 0 skipped**.
- The report's driver SHA-256 was `62c2b3f98ca987d5ab50b6f01e2d2c1c5b3ee995fe93666aa5da9415f65c3348`.

The stock-driver test and the legacy PanVK experiment are separate records. Their reported device IDs differ and must remain separately identified until the exact device-ID mapping and binary provenance are reconciled. The legacy PanProbe result proves only the recorded load/device-probe coverage for that historical run; it does not validate the new NYAMIST implementation, surface presentation, or complete game compatibility.

## D3D11 real-game evidence

A user-provided PanPlay SessionLogs export and matching PerfRecorder export from **2026-11-08** recorded a Stranded Deep D3D11 session:

- Session duration: **519 seconds**.
- PerfRecorder duration: **489.13 seconds**.
- Captured frames: **41,868**.
- Average reported FPS: **85.60**.
- P95 frame time: **43.98 ms**.
- P99 frame time: **63.73 ms**.
- Reported stutters: **11,766**.
- DXVK created a D3D11 device at feature level **11_0** (maximum reported capability was 11_1).
- DXVK logged swapchain setup at **1280×720**, BGRA8 UNORM, IMMEDIATE present mode, four images.
- The DXGI log identified the Mali-G925-Immortalis MC12 and a legacy PanVK version string.

**Evidence limits:** the exported logs do not provide authoritative Vulkan Present success/failure counters; the session manifest and DXGI driver version strings also have an unresolved provenance mismatch. The session ended through the user overlay and has no confirmed clean process exit. The frame-time percentiles and stutter count show that average FPS alone is not a sufficient smoothness measure.

This is valuable sustained-session evidence, but it does **not** yet qualify as a production-ready driver validation or a confirmed clean-render release.

## Release status

<badge label="V1 — Engineering Preview" color="warning"/> <badge label="Driver package not released" color="secondary"/>

The repository currently contains the NYAMIST profiling/validation core, device probes, session importers, configuration templates, tests, and engineering documents. The earlier PanVK report adapter and imported-driver evidence are retained only as legacy research material. The repository does **not** yet contain a verified, installable NYAMIST G925 native driver release asset or a published WCP download.

The legacy PanVK RC5 evidence is not part of the new implementation and is not proof that a NYAMIST driver package exists. Do not install or advertise a finished NYAMIST driver until its independently developed source, release asset, manifest, checksums, target-device validation, and rollback procedure are published here.

### V1 release gates

1. Define the NYAMIST Native Graphics Core architecture and module boundaries; document all hardware interfaces and their licensing/redistribution constraints.
2. Establish the supported Android NDK/toolchain and build a minimal NYAMIST-owned arm64-v8a userspace prototype. Do not assume undocumented kbase/CSF interfaces are stable or publicly usable.
3. Implement hardware discovery and a safe integration path that can use the stock vendor Vulkan driver as a baseline while the independent ICD feasibility is evaluated.
3. Load it through a supported app-local driver-loading mechanism without altering the stock system driver.
4. Verify NYAMIST binary identity, GPU enumeration, Vulkan features/extensions, formats, queue families, and memory behavior on the REDMI K80 Ultra; distinguish stock-driver baseline results from NYAMIST-owned implementation results.
5. Pass surface/swapchain render-and-present tests with explicit Present evidence.
6. Validate DXVK D3D11 device creation and repeatable real-game rendering, including a sustained run of at least 300 seconds, frame-time review, device-loss checks, and clean process exit.
7. Build and test the WCP manifest/catalog and any required ZIP adapter against named Winlator-compatible clients.
8. Publish the single G925 release asset with SHA-256, source revision, build metadata, install instructions, known limitations, and a tested rollback path.

Until these gates pass, the status remains **engineering preview / pre-release**.

## Engineering rules

- Target **Immortalis-G925 MC12 only** for this release line.
- Preserve the Android stock Mali driver and keep experimental ICD selection app-local and reversible.
- Never chmod or bypass access controls on `/dev/mali0`; never modify SELinux, vendor partitions, kernel modules, or protected system libraries.
- Never claim a Vulkan extension or feature unless the loaded driver reports it and the relevant test passes.
- Never treat GPU heap size as free VRAM or a guaranteed per-game allocation.
- Keep DXVK, VKD3D-Proton, Wine, Box64/FEX, the Android Vulkan loader, stock vendor driver, and NYAMIST-owned graphics components as distinct layers with their own versions and logs. PanVK is legacy research only, not an active dependency.
- Distinguish device enumeration, render/present, sustained rendering, and production readiness as separate validation states.
- Keep all source revisions, licenses, build inputs, checksums, raw logs, and test conditions attached to each release.

## Repository engineering components

- C++20 device/runtime profiling and evidence core.
- Legacy PanVK report importer (historical evidence only; not used by the active driver architecture).
- Android/Termux device probe and configuration templates.
- PanPlay/PanProbe importers and session/PerfRecorder import tools.
- Deterministic CI build and test workflow.
- Separate LiteRT-LM + MediaTek NeuroPilot NPU experiment for future offline analysis; it is not part of the Vulkan driver and does not prove GPU execution.

### Development documents

- [NYAMIST G925 R1 — First Release Plan](docs/releases/NYAMIST-G925-R1.md)
- [NYAMIST G925 R1.0 — Hardware Discovery implementation](docs/releases/NYAMIST-G925-R1.0-hardware-discovery.md)
- [NYAMIST G925 R1.1 — Vulkan Capability Intelligence](docs/releases/NYAMIST-G925-R1.1-capability-intelligence.md)
- [Architecture and scope](docs/architecture.md)
- [Module contracts](docs/module-contracts.md)
- [REDMI K80 Ultra hardware validation](docs/device-validation-redmi-k80-ultra.md)
- [Legacy PanVK adapter and evidence archive](docs/panvk-adapter.md)
- [Legacy PanVK kbase research notes](docs/panvk-kbase-integration.md)
- [DX11/Vulkan validation](docs/dx11-vulkan-validation.md)
- [Game validation runbook](docs/game-validation-runbook.md)
- [PanPlay import format](docs/panplay-import.md)
- [Session profiles](docs/profile-persistence.md)
- [A/B regression evaluation](docs/ab-regression-evaluation.md)
- [Local Graphics Advisor design](docs/local-lightweight-graphics-advisor.md)
- [MediaTek NPU 890 experiment](docs/npu-890-proof-of-execution.md)
- [Roadmap](docs/roadmap.md)
- [Original README archive](README_OLD.md)

---

**CHORD OF NYAMIST — THE CREATIVE INTELLIGENCE UNIVERSE, V1**  
*Every Signal Has a Story.*  
**Created by AMGNYA**

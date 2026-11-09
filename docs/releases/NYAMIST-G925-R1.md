# NYAMIST G925 — R1
## Native Graphics Core · Engineering Preview

**Project:** CHORD OF NYAMIST — THE CREATIVE INTELLIGENCE UNIVERSE V1  
**Target GPU:** Arm Immortalis-G925 MC12  
**Target SoC:** MediaTek MT6991 / Dimensity 9400+  
**Reference device:** REDMI K80 Ultra · arm64-v8a · Android 16  
**Release line:** R1  
**Implementation policy:** Original NYAMIST-owned components. PanVK is not an active implementation dependency.

## R1 objective

Deliver the first reproducible, device-validated NYAMIST Native Graphics Core milestone for the Immortalis-G925 MC12.

R1 is an engineering preview, not a claim that a complete replacement Vulkan driver already exists. The first release establishes a testable foundation: device identification, safe access to documented Android graphics interfaces, Vulkan capability discovery, structured diagnostics, and a reproducible build/test package.

The stock MediaTek/ARM Vulkan driver remains the known-good baseline and fallback during R1. It is not modified or replaced by unsafe system changes.

## R1 scope

### Included
- G925/MT6991 target identity and compatibility checks.
- Read-only hardware and Android graphics environment discovery.
- Vulkan loader/device enumeration and capability reporting through supported Android APIs.
- NYAMIST-owned C++ core, configuration schema, diagnostics, and evidence export.
- DXVK/Wine/Winlator environment metadata capture, without treating those layers as part of the GPU driver.
- Reproducible arm64-v8a build, automated tests, checksums, and release notes.
- WCP/ZIP packaging research and validation against explicitly named client versions.

### Not included in R1
- A complete replacement Vulkan ICD unless an independent implementation is actually built and passes the release gates.
- Direct undocumented register access, proprietary firmware replacement, kernel-module replacement, or privileged system modification.
- SELinux bypass, root escalation, permission circumvention, or writes to protected vendor/system partitions.
- Claims of universal Winlator/WCP compatibility.
- VKD3D-Proton/DX12 support claims before separate validation.

## Initial architecture

```text
Winlator / Android application
          |
          +---- DXVK / D3D11 translation (separate layer)
          |
Android Vulkan Loader and supported app-local integration
          |
NYAMIST Native Graphics Core
  +-- DeviceIdentity       G925 / MT6991 identification
  +-- VulkanDiscovery      loader, physical device, features, extensions
  +-- RuntimeProfile       validated configuration and client metadata
  +-- Diagnostics          logs, errors, session metadata
  +-- Evidence             JSON report, hashes, test provenance
          |
Stock vendor Vulkan driver (R1 baseline / fallback)
          |
Android graphics kernel interface
          |
Immortalis-G925 MC12
```

A future NYAMIST-owned Vulkan ICD is a separate engineering milestone. Successful stock-driver enumeration is not evidence that a custom ICD exists.

## First implementation milestone: R1.0 — Hardware Discovery

1. Detect Android version, API level, ABI, SoC, GPU identity, kernel build, and available Vulkan loader.
2. Enumerate Vulkan physical devices using the supported Android Vulkan loader.
3. Record actual device API version, driver identity, vendor/device IDs, queue families, extensions, formats, and exposed memory heaps.
4. Emit a machine-readable JSON report with explicit source and confidence for each field.
5. Run on-device smoke tests without changing system state.
6. Build and test reproducibly in CI for arm64-v8a.

## Repository module proposal

- `src/core/` — NYAMIST-owned C++ core and shared types.
- `src/platform/android/` — Android platform discovery.
- `src/vulkan/` — Vulkan loader and physical-device discovery.
- `src/config/` — validated profile schema and parser.
- `src/diagnostics/` — structured logs and error reporting.
- `src/evidence/` — JSON evidence export and provenance.
- `tests/` — host-side unit tests and device smoke tests.
- `profiles/` — target-device profile templates.
- `packaging/` — WCP/ZIP format adapters, only after client compatibility is verified.
- `docs/` — architecture, test protocol, compatibility matrix, and release records.

This module layout is a proposal until reconciled with the existing repository tree; existing source files must be inventoried and preserved.

## R1 acceptance gates

- [ ] Existing repository contents inventoried and preserved.
- [ ] Build inputs, toolchain versions, licenses, and source provenance recorded.
- [ ] arm64-v8a build completes reproducibly.
- [ ] Unit tests pass in CI.
- [ ] Device identity correctly reports Immortalis-G925 MC12 / MT6991 on the reference device.
- [ ] Vulkan physical-device enumeration succeeds on the reference device.
- [ ] Capability report is generated from actual queried driver results, not hardcoded assumptions.
- [ ] Stock-driver baseline and any NYAMIST-owned implementation are clearly distinguished.
- [ ] No system/vendor partition or SELinux changes are required.
- [ ] Install/uninstall and stock-driver fallback procedures are documented.
- [ ] Release archive, SHA-256, build metadata, and test evidence are published.
- [ ] WCP/ZIP import is tested on named client versions before compatibility is claimed.

## Evidence and status policy

Device discovery, Vulkan enumeration, rendering, presentation, sustained game execution, and production readiness are separate validation states. Passing one does not imply the next.

Every report must identify the tested binary, device, OS build, driver version, test conditions, and source of each measurement. Unknown values remain unknown; guessed hardware capabilities are not acceptable.

## R1 release statement

**NYAMIST G925 R1 is an engineering-preview foundation for an original G925-focused graphics stack.** A downloadable replacement driver will only be announced after an independently built NYAMIST driver binary passes the relevant device-side validation gates.

**CHORD OF NYAMIST — THE CREATIVE INTELLIGENCE UNIVERSE V1**  
*Every Signal Has a Story.*  
**Created by AMGNYA**

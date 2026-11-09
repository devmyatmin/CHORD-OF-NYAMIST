# CHORD OF NYAMIST — NYAMIST G925 architecture and scope

## Product definition

The active R1 line is an original, G925-focused native graphics engineering project for Immortalis-G925 MC12 on MediaTek MT6991. The first implementation milestone is read-only hardware/Vulkan discovery inside the existing C++20 runtime.

NYAMIST also contains a graphics-runtime sidecar: device profiles, telemetry import, evidence validation, session analysis, and deterministic recommendations. These control-plane tools are separate from the driver and must not sit in the per-draw-call or Present hot path.

NYAMIST is not yet a complete Vulkan implementation or a released ICD. The stock MediaTek/ARM Vulkan driver remains the working baseline and rollback path.

## Intended graphics execution path

    D3D11 game
      → Wine / Box64 or FEX
      → DXVK
      → Android Vulkan loader
      → stock vendor Vulkan driver (R1 baseline)
      → vendor mali_kbase kernel interface
      → Immortalis-G925 MC12

A future NYAMIST-owned Vulkan ICD may replace the app-local driver selection only after it is independently implemented, built, and device-validated. That future path is not implemented by the R1.0 discovery tool.

## NYAMIST-owned modules

| Module | Responsibility | Boundary |
|---|---|---|
| Hardware discovery | Android identity, OS/ABI/kernel facts, Vulkan physical-device queries, JSON evidence | Read-only; no driver selection or device-node access |
| Core/cache | Configuration, fingerprints, NYAMIST metadata | Never owns upstream shader/pipeline caches |
| Component registry | Component identity and readiness state | Read-only discovery |
| Vulkan capability importer | Validate imported device/driver reports | Does not load or select an ICD |
| Legacy PanVK adapter | Parse archived PanVK report/evidence for historical comparison | Research archive only; not an active dependency |
| DXVK adapter | D3D11 translator/session report validation | Does not patch or configure DXVK |
| Device probe adapter | Validate imported Android device facts | Does not infer missing hardware facts |
| Session snapshot | Typed telemetry, freshness and source validation | Control-plane data, not a driver ABI |
| Pipeline gate | Deterministic readiness decisions | Does not launch games or change settings |
| Profile store | Versioned device/session/compatibility evidence | NYAMIST data only |
| A/B evaluator | Repeated workload comparison | Reports evidence; never applies settings |
| Advisor | Deterministic bounded proposals; optional local-model analysis after capture | Model is advisory; deterministic policy remains authoritative |

Readiness levels are deliberately strict:

- unavailable — required component or capability is absent.
- discovered — identity/version/device enumeration is observed.
- smoke_tested — explicit render/Present success and clean exit are demonstrated.
- sustained_tested — a repeatable test of at least 300 seconds passes with no critical failure.

Vulkan physical-device enumeration, surface creation, queue submission, render/Present, sustained game execution, and production readiness are separate validation states.

## Target-device evidence and limits

The recorded REDMI K80 Ultra evidence includes a stock vendor Vulkan device record and a separate legacy PanVK RC5 research experiment. Their reported device IDs differ and remain separately identified pending provenance reconciliation. Historical PanProbe results are not validation of a NYAMIST-owned driver.

The previous Stranded Deep/DXVK session is retained as historical performance evidence only. It does not demonstrate a clean, production-ready NYAMIST driver path.

## Ownership and safety

- Android's stock Vulkan driver remains intact as a rollback path.
- Select any experimental driver only through a supported app-local launcher mechanism.
- Never overwrite /vendor, change permissions on /dev/mali0, flash a kernel, or weaken SELinux.
- DXVK, Mesa/PanVK historical artifacts, VKD3D-Proton and vendor driver caches remain under their respective owners.
- A missing, stale, or rejected NYAMIST profile falls back to unmodified runtime defaults.

## Deferred scope

A complete NYAMIST-owned Vulkan ICD, WCP/ZIP distribution adapters, D3D12/VKD3D-Proton, and automatic tuning are separate gated milestones. None is implied by the R1.0 discovery executable.

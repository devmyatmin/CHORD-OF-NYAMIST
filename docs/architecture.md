# NYAMIST architecture and scope

## Product definition

NYAMIST is a device-aware Android graphics-runtime sidecar. The first target is a D3D11 game running through an existing Windows compatibility runtime, DXVK, an Android Vulkan loader, and an app-local PanVK kbase ICD on REDMI K80 Ultra.

NYAMIST is not:
- a D3D11 implementation or a Vulkan driver;
- a Wine/FEX/Box64 replacement;
- a DXVK or PanVK fork/distribution;
- a native Android port of a Windows game;
- a per-draw-call graphics interceptor.

## D3D11 execution path

```text
D3D11 game
  → Wine / FEX / Box64
  → DXVK
  → Android Vulkan loader
  → PanVK kbase ICD
  → vendor mali_kbase kernel interface
  → Immortalis-G925
```

DXVK translates D3D11/DXGI commands to Vulkan. PanVK implements Vulkan. The kernel driver exposes the device interface. NYAMIST sits alongside the stack and reads component reports, device facts, session evidence, and performance telemetry.

## NYAMIST-owned modules

| Module | Responsibility | Boundary |
|---|---|---|
| Core/cache | Configuration, fingerprints, NYAMIST metadata | Never owns upstream shader/pipeline caches |
| Component registry | Component identity and readiness state | Read-only discovery |
| PanVK adapter | Driver identity, Vulkan capabilities, PanProbe evidence | Does not load or select an ICD |
| DXVK adapter | D3D11 translator/session report validation | Does not patch or configure DXVK |
| Device probe | Android and Vulkan capability facts | No privileged device access |
| Session snapshot | Typed telemetry, freshness and source validation | Control-plane data, not a driver ABI |
| Pipeline gate | Deterministic readiness decisions | Does not launch games or change settings |
| Profile store | Versioned device/session/compatibility evidence | NYAMIST data only |
| A/B evaluator | Repeated workload comparison | Reports evidence; never applies settings |
| Advisor | Bounded optimization proposals | Automatic application remains disabled |

Readiness levels are deliberately strict:
- **unavailable** — required component or capability is absent.
- **discovered** — identity/version/device enumeration is observed.
- **smoke_tested** — explicit render/Present success and clean exit are demonstrated.
- **sustained_tested** — a repeatable test of at least 300 seconds passes with no critical failure.

PanProbe passing or a DXVK feature-level report alone cannot promote a configuration to smoke-tested.

## Target-device evidence and limits

The recorded REDMI K80 Ultra evidence (2026-11-08) includes PanVK RC5 loading and PanProbe 41/41, plus DXVK D3D11 feature-level results through 11_1 and a synthetic Cube test. These are device/runtime smoke-test observations, not evidence that a commercial game runs correctly for an extended session.

Current PanPlay exports provide useful session and frame-time data but do not expose all authoritative Vulkan Present success/failure and device-loss counters required by NYAMIST's promotion rules. Imported game sessions therefore remain discovery/review evidence.

## Ownership and safety

- Android's stock Vulkan driver remains intact as a rollback path.
- Select an experimental driver only through the supported app-local launcher mechanism.
- Never overwrite `/vendor`, chmod `/dev/mali0`, flash a kernel, or weaken SELinux.
- DXVK, PanVK/Mesa, VKD3D-Proton and vendor driver caches remain under their respective owners.
- A missing, stale, or rejected NYAMIST profile falls back to unmodified runtime defaults.

## Deferred scope

D3D12/VKD3D-Proton is retained only as experimental read-only code and is not part of the current acceptance target. Local AI inference is not implemented and is deferred. Neither should delay D3D11 real-game validation.

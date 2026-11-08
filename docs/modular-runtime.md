# NYAMIST Graphics Runtime — Modular Architecture

## Product definition

NYAMIST is an Android-oriented graphics runtime orchestrator and optimization layer. It combines independently maintained graphics components through explicit adapters and validated policy. It is not a forked or renamed PanVK/DXVK distribution, and it does not replace the Android Vulkan loader, vendor kernel driver, Wine, FEX, or Box64.

## Runtime topology

Windows game
  -> Wine / Proton + FEX / Box64
  -> DXVK for D3D11 or VKD3D-Proton for D3D12
  -> Android Vulkan loader
  -> selected Vulkan ICD (PanVK candidate)
  -> Mali kbase / CSF
  -> Immortalis-G925

NYAMIST sidecar: Core, Component Registry, Runtime Coordinator, read-only adapters, Device/Session Monitor, Validation Gate, AI Advisor, and NYAMIST metadata cache.

The translator produces Vulkan work. The Vulkan ICD implements Vulkan. The kernel driver exposes the GPU interface. NYAMIST discovers, profiles, validates, and coordinates these independently installed components; it is not in the per-draw-call path.

## Module boundaries

| Module | Owns | Does not own |
|---|---|---|
| Core | Configuration, component registry, policy, validation, metadata cache | GPU command submission |
| DX11 adapter | Read-only DXVK version/capability/session inspection | DXVK source or shader cache |
| D3D12 adapter | Read-only VKD3D-Proton inspection and feature reporting | VKD3D-Proton source or pipeline cache |
| PanVK adapter | ICD identity, driver version, Vulkan capability and test-report inspection | PanVK driver binary or Android Vulkan loader |
| AI advisor | Candidate profile recommendations from structured telemetry | Direct driver settings or unvalidated runtime commands |
| Monitor | Session logs, frame-time summaries, memory/thermal telemetry where accessible | Privileged kernel or SELinux changes |

The PanVK, DXVK, and VKD3D-Proton adapters read normalized evidence reports through bounded key=value parsers and expose CLI inspection commands. They do not open/load the ICD or launch/modify translators. Translator smoke-test promotion requires explicit Present counters and reported zero failure/device-loss status. See docs/panvk-adapter.md and docs/translator-adapters.md.

## Readiness levels

- unavailable: component is absent or explicitly unusable.
- discovered: identity/version is found; no rendering validation is implied.
- smoke_tested: a defined device-creation/render/present test passed.
- sustained_tested: a repeatable sustained test passed with logs and no critical rendering, device-loss, or stability failure.

The runtime coordinator may permit a controlled smoke test when required components are discovered. It must not label a configuration production-ready until both translator and driver have sustained-test evidence.

## Current device evidence (REDMI K80 Ultra)

As of 2026-11-08, user-run tests on the REDMI K80 Ultra / Immortalis-G925 report:

- PanVK RC5 PanProbe: 41/41 tests passed.
- D3D11 ARM64EC through DXVK: feature levels 10_0–11_1 passed; 27,508 frames over 20 seconds; normal process exit (code 0).
- D3D12 ARM64EC through VKD3D-Proton: feature levels 11_0 and 11_1 passed; 12_0–12_2 unsupported; 19,003 frames over 20 seconds; normal process exit (code 0).
- D3D12 Present calls reported hr=0x00000000.
- A synthetic D3D11 Cube test reported about 838 average FPS at 1280x720. This is a microbenchmark, not a game-performance claim.

These results justify a controlled experimental integration target. They do not prove sustained stability, broad game compatibility, correct rendering in every workload, or production readiness. The current 20-second test applications exit normally when their test run completes; this is not evidence of a crash by itself.

## Cache ownership

- DXVK owns DXVK shader and pipeline caches.
- VKD3D-Proton owns its own pipeline/shader cache.
- PanVK/Mesa owns its own driver caches.
- NYAMIST stores only its own versioned profiles, compatibility observations, recommendations, and validation evidence.

Never duplicate, rewrite, or delete another component's cache.

## Safety and rollback

- Keep the stock Android Vulkan driver intact.
- Do not overwrite /vendor libraries or globally replace the Vulkan ICD.
- Do not chmod /dev/mali0, flash a kernel, or weaken SELinux.
- Select app-local driver packages only through their supported launcher mechanism.
- Every optimization is advisory until it passes deterministic validation and an A/B regression test.
- If a profile is missing, stale, or rejected, fall back to the known working launcher/runtime defaults.

## Implementation sequence

1. Establish module contracts and registry (complete).
2. Add read-only adapters for device facts and raw PanProbe import (PanVK, DXVK, and VKD3D-Proton normalized-report adapters implemented).
3. Add a versioned session/compatibility profile store and persist validated evidence (store implemented; live collector integration remains pending).
4. Add sustained D3D11 test evidence and keep it as the default target.
5. Add the D3D12 adapter as experimental, preserving the observed FL11_1 ceiling.
6. Add monitor aggregation and deterministic A/B evaluation (evaluator implemented; live telemetry collector and controlled workload harness remain pending).
7. Introduce a local AI advisor only after structured telemetry and repeatable baselines exist.

# NYAMIST-LAYER

**A lightweight Android graphics-runtime sidecar for D3D11 → Vulkan validation and device-aware policy.**

Target device: **REDMI K80 Ultra · Dimensity 9400+ · Immortalis-G925 · Android 16 · 12 GB RAM**

NYAMIST is not a Windows-game-to-Android converter, a Vulkan driver, or a replacement for DXVK. It observes and validates the runtime components that already exist on Android, stores NYAMIST-owned device/session profiles, and keeps optimization decisions evidence-gated.

## Runtime path

```text
Windows D3D11 game
  → Wine / FEX / Box64
  → DXVK (D3D11 → Vulkan)
  → Android Vulkan loader
  → PanVK kbase ICD (experimental app-local driver)
  → vendor mali_kbase kernel interface
  → Immortalis-G925

NYAMIST sidecar:
  device facts · component probes · session profiles
  validation gate · telemetry · cache · advisory policy
```

- **DXVK** translates D3D11/DXGI to Vulkan.
- **PanVK** implements Vulkan on the Mali kbase interface.
- **NYAMIST** inspects, profiles, validates, and measures the stack. It is not in the per-draw-call path.
- The stock Android Vulkan driver remains the fallback. NYAMIST does not alter `/vendor`, `/dev/mali0`, SELinux, or upstream-owned caches.

## What is established

Device evidence recorded on 2026-11-08 for the REDMI K80 Ultra includes:

- PanVK RC5 loaded and enumerated one Vulkan device.
- PanProbe reported **41/41** tests passed.
- DXVK D3D11 ARM64EC feature-level testing reported **10_0–11_1** available.
- A synthetic D3D11 Cube test completed. Its reported performance is a microbenchmark, not a game-performance result.
- The portable C++20 core, runtime adapters, profile/evidence validators, Python importers, and hosted CI tests are implemented.

These results establish a promising test path, **not** sustained real-game compatibility.

## Current milestone

**Run one real D3D11 game repeatably through PanPlay + PanVK + DXVK on the target phone.**

The required evidence is a matching PanVK/DXVK/game session, correct visible rendering, clean process exit, at least 300 seconds of representative gameplay, frame-time data, and authoritative render/Present and device-loss evidence. Current PanPlay exports do not provide every Present counter required for an automatic validation promotion, so NYAMIST deliberately keeps imported game sessions at discovery/review level.

Start with the [game validation runbook](docs/game-validation-runbook.md).

## Build the NYAMIST sidecar

Requirements: CMake 3.20+ and a C++20 compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNYAMIST_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Termux device-side instructions are in [Termux workflow](docs/termux-workflow.md). The Termux build compiles the portable NYAMIST core; it does not build PanVK, DXVK, Wine, or the game runtime.

## Project boundaries

- Use maintained upstream PanVK, DXVK, Wine/FEX/Box64 and PanPlay components. Do not fork, rebrand, or rebuild them as part of the current milestone.
- Keep NYAMIST read-only with respect to external graphics components.
- Keep DXVK, PanVK/Mesa, and vendor-driver caches under their owners' control.
- Do not replace the stock system driver or request root access for the normal workflow.
- D3D12/VKD3D-Proton and local AI inference are parked until D3D11 game validation and repeatable performance baselines are established.
- Recommendations remain advisory. Automatic application is disabled.

## Development documents

- [Architecture and scope](docs/architecture.md)
- [Module contracts](docs/module-contracts.md)
- [Device validation](docs/device-validation-redmi-k80-ultra.md)
- [PanVK adapter](docs/panvk-adapter.md)
- [DX11/Vulkan validation](docs/dx11-vulkan-validation.md)
- [PanPlay import format](docs/panplay-import.md)
- [Session profiles](docs/profile-persistence.md)
- [A/B regression evaluation](docs/ab-regression-evaluation.md)
- [Roadmap](docs/roadmap.md)

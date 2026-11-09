# NYAMIST-LAYER

**A lightweight Android graphics-runtime sidecar for D3D11 → Vulkan validation and device-aware policy.**

Target device: **REDMI K80 Ultra · Dimensity 9400+ · Immortalis-G925 · Android 16 · 12 GB RAM**

NYAMIST is not a Windows-game-to-Android converter, a Vulkan driver, or a replacement for DXVK. It observes and validates the runtime components that already exist on Android, stores NYAMIST-owned device/session profiles, and keeps optimization decisions evidence-gated.

## Project status and continuity record

**This section is the current handoff source of truth. Read it before proposing work.**
The detailed contracts remain in the linked documents; this summary separates what is implemented, what is documented, and what is still unverified.

### Core project priorities

1. **D3D11 real-game validation** on the REDMI K80 Ultra through the existing PanPlay + DXVK + app-local PanVK path.
2. **Local Lightweight Graphics Advisor** as a first-class NYAMIST subsystem: offline, post-session graphics diagnosis and evidence-grounded DX11/DX12 optimization proposals. It is not a disposable experiment and must remain on the project roadmap.
3. Evidence integrity, deterministic policy validation, repeatable A/B measurement, and safe rollback.
4. D3D12/VKD3D-Proton remains a separate later validation path; DX11 results must never be presented as DX12 support.

### Local Lightweight Graphics Advisor — status

<badge label="Architecture documented" color="success"/> <badge label="Model/runtime not yet verified" color="warning"/>

The advisor design and safety contract are committed in [Local Lightweight Graphics Advisor](docs/local-lightweight-graphics-advisor.md), and Milestone 4 is explicitly retained in [Roadmap](docs/roadmap.md).

**Confirmed present in the repository**
- The advisor's role, component boundaries, proposal schema, evidence requirements, safety rules, and acceptance gates are documented.
- The design requires local/offline inference after the game session, using normalized SessionLogs, DXVK/VKD3D logs, Vulkan/device facts, and performance telemetry.
- A deterministic policy gate—not the model—has final authority. Model output is untrusted data and recommendations are advisory/dry-run only.
- CPU-first inference with bounded memory/thread use is the initial implementation direction. GGUF/llama.cpp is an example backend to evaluate, not a confirmed installed runtime or final model choice.
- Tiny and balanced model size tiers are evaluation categories, not selected models.

**Not confirmed / not to be claimed as complete**
- No model binary (GGUF or otherwise) has been verified as installed in the device workspace.
- No local inference runtime has been verified as installed.
- No offline inference run, memory benchmark, model-quality evaluation, or model-backed proposal has been verified.
- No final model has been selected. Earlier candidate names are not a decision.
- No NPU or PanVK/Vulkan compute inference path has been proven on the target device.

**Required next implementation sequence**
1. Preserve the advisor design and evidence pipeline; do not remove or silently de-scope it.
2. Inventory existing model files and inference runtimes before downloading, rebuilding, or reinstalling anything.
3. Complete and validate the D3D11 evidence normalization and provenance checks.
4. Benchmark a small CPU-first local model after game exit, within measured RAM/CPU/thermal limits.
5. Add an optional model adapter that emits schema-valid, evidence-linked proposals.
6. Enforce deterministic policy validation, human approval, rollback, and matched A/B evaluation. Never put inference in the frame/render/Present path.

The phone previously had about 2 GB free RAM during the discussion of this feature. Treat that as historical context, not a current measurement. Do not run inference concurrently with gameplay until resource contention is measured.

### Latest Stranded Deep diagnostic snapshot

The latest inspected PanPlay session is evidence of a **runtime crash after partial game initialization**, not a confirmed graphics-driver failure:

- Unity loaded game data, world seed/maps, and multiple game asset bundles before the crash report appeared.
- Unity Player log then recorded repeated `mono_os_sem_timedwait: mono_win32_wait_for_single_object_ex failed with error 87` and `Crash!!!`, followed by the Unity crash-handler notice.
- The inspected DXVK D3D11 log recorded device creation, feature-level selection (11_0 selected; 11_1 maximum reported), and a 1280×720 swapchain creation.
- The inspected DXVK log did not show a device-lost or out-of-memory failure in the searched output. This does not prove the GPU path is fault-free; it only means such a failure was not established by that evidence.
- The nearby Steam achievement failure is not proven causal. Windows error 87 is an invalid-parameter code, but the exact failing handle/API/module is not yet identified.
- Launcher session metadata reported a user-stop reason and `crash:false`; that metadata does not override the Unity Player log's explicit crash-handler report.

**Do not repeat already completed broad crash-file searches.** Prior searches of Android data and Download locations did not locate the actual Unity crash dump/report. The existing session ZIP and performance ZIP were already exported. Continue from the preserved logs and seek the crash call stack/module if it becomes available.

A proposed `PROTON_NO_NTSYNC=1` single-variable A/B test was discussed because the launcher reported no usable `/dev/ntsync` and fallback to userspace ntsync. **It has not been confirmed as applied or tested.** It is a diagnostic hypothesis, not an established fix. If tested, preserve all existing environment variables and change only that one variable; keep game files, resolution, driver, Proton, and FEX settings unchanged.

### Continuity rules for future project work

- Read this README and the relevant linked design/runbook before asking the user to repeat diagnostics or preferences.
- Never report a planned, documented, or proposed component as installed, implemented, tested, or removed without direct evidence.
- Keep confirmed facts, hypotheses, pending tests, and completed implementation explicitly separate.
- Do not repeat searches or commands already recorded as completed unless new evidence gives a specific reason.
- Preserve the original game assets and existing runtime components. Do not generate replacement game content or modify upstream/system/vendor graphics components as a shortcut.
- Keep the local advisor in project scope even while its inference backend is deferred. Deferral of implementation is not cancellation of the subsystem.
- Do not change multiple runtime variables in one diagnostic test. Preserve backups and logs before any change.

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

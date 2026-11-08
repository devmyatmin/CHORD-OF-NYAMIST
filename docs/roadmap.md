# Roadmap

## Current objective

Establish repeatable D3D11 real-game execution on REDMI K80 Ultra through the existing PanPlay + PanVK + DXVK stack, then use NYAMIST to record and evaluate evidence without taking ownership of the external components.

## Foundation — implemented

- [x] Portable C++20 core, deterministic fingerprints, validated NYAMIST cache.
- [x] Component registry and runtime prerequisite coordinator.
- [x] Read-only PanVK, DXVK, device-facts and Vulkan capability adapters.
- [x] PanProbe and PanPlay ZIP importers with bounded parsing and integrity checks.
- [x] Immutable session snapshot and typed telemetry freshness checks.
- [x] Pipeline Gate and versioned profile persistence.
- [x] Conservative game-session evidence checker.
- [x] Deterministic A/B evaluator and bounded advisory policy.
- [x] Unit tests, importer tests, and hosted Core CI.

## Milestone 1 — controlled D3D11 game validation

- [x] REDMI K80 Ultra PanVK RC5 load and one-device enumeration reported.
- [x] PanProbe 41/41 reported.
- [x] DXVK D3D11 ARM64EC feature-level test reported through FL11_1.
- [x] Synthetic D3D11 Cube test completed.
- [ ] Match a real game session to the exact PanVK binary/build identity.
- [ ] Run representative gameplay for at least 300 seconds.
- [ ] Confirm correct visible rendering, surface presentation, scene transitions, and clean exit.
- [ ] Capture authoritative Present success/failure and device-loss evidence.
- [ ] Repeat a cold and a warm run without changing the workload.

## Milestone 2 — performance baseline

- [ ] Collect p50/p95/p99 frame time, memory, CPU/GPU load and thermal data where accessible.
- [ ] Run at least three sustained baseline sessions under matched conditions.
- [ ] Verify that profile invalidation works after driver, translator, or device identity changes.
- [ ] Keep all upstream shader/pipeline caches under upstream ownership.

## Milestone 3 — measured optimization

- [ ] Select one bounded NYAMIST advisory policy at a time.
- [ ] Run at least three matched baseline and candidate sessions, each at least 300 seconds.
- [ ] Compare median p95 frame time and stability evidence.
- [ ] Retain a one-step rollback to the unmodified runtime profile.
- [ ] Keep automatic application disabled until repeatable evidence justifies a separately reviewed design.

## Milestone 4 — local lightweight graphics advisor

- [x] Define the offline/post-session model boundary and safety contract.
- [ ] Audit and normalize PerfRecorder typed CSV streams and metadata consistency.
- [ ] Benchmark a tiny mobile-first local model on CPU with bounded memory/thread use.
- [ ] Add an optional model adapter that emits schema-validated, evidence-linked proposals.
- [ ] Keep deterministic policy validation and A/B evaluation as final authority.
- [ ] Evaluate PanVK Vulkan compute and MediaTek NPU access only as separate, measured backends.
- [ ] Keep model inference out of the game frame/render/present path.

## Milestone 5 — separate D3D12/VKD3D-Proton validation

- [ ] Pin and record the exact VKD3D-Proton and Vulkan driver identities.
- [ ] Validate D3D12 capability requirements against the actual PanVK feature set.
- [ ] Prove D3D12 device creation, swapchain presentation, scene rendering, clean exit, and sustained gameplay.
- [ ] Establish repeated D3D12 baselines before any tuning.

## Explicitly excluded

- Custom PanVK/DXVK/VKD3D packaging or rebuilding upstream graphics components as a shortcut.
- Broad device support beyond the REDMI K80 Ultra target.

## Release gate

Do not call the project game-compatible or production-ready until a named game has repeatable sustained rendering evidence on the target device, the exact runtime identities are recorded, and failures/limitations are documented.

# Roadmap

## Phase 0 — Portable core and modular foundation
- [x] C++20 project skeleton
- [x] Deterministic cache fingerprint
- [x] Versioned validated-cache prototype
- [x] Compact bounded binary cache V2 with V1 read compatibility and corruption/length tests
- [x] V1-versus-V2 cache microbenchmark harness with file-size and latency reporting
- [x] Rule-based advisor interface
- [x] Unit tests and GitHub Actions CI
- [x] Termux probe and on-device core build scripts
- [x] Modular component contracts and registry
- [x] D3D11/D3D12 runtime prerequisite coordinator
- [x] Explicit upstream cache ownership and system-driver rollback policy

## Phase 1 — REDMI K80 Ultra PanVK + D3D11 baseline
- [x] Run read-only device probe and identify Android 16 / arm64-v8a / MT6991 / Immortalis-G925
- [x] Verify PanVK RC5 app-local ICD load and device enumeration
- [x] Run PanProbe 41-test suite (41/41 reported)
- [x] Run DXVK D3D11 ARM64EC feature-level test (10_0–11_1 pass)
- [x] Run D3D11 synthetic Cube rendering test
- [x] Add read-only PanVK report adapter, CLI inspection, and evidence-gated validation tests
- [x] Add read-only DXVK and VKD3D-Proton adapters, CLI inspection, and report-schema tests
- [x] Add automatic importer for raw PanProbe JSON/ZIP exports
- [ ] Attach D3D11 render/present evidence to the matching PanVK build identity
- [ ] Run sustained D3D11 rendering test for at least 5 minutes
- [ ] Verify repeatable game scenes, surface presentation, and visual correctness
- [ ] Record a complete rollback procedure and preserve stock Vulkan driver

## Phase 2 — Runtime integration and monitoring
- [x] Add read-only DXVK version/session adapter
- [ ] Add device facts and Vulkan capability adapter
- [ ] Persist versioned session and compatibility profiles
- [ ] Import PanPlay logs into the normalized adapter report schema
- [ ] Capture frame-time percentiles, memory, and thermal telemetry where accessible
- [ ] Keep DXVK shader/pipeline caches under DXVK control
- [ ] Add deterministic A/B evaluation and regression reporting

## Phase 3 — Experimental D3D12
- [x] Verify VKD3D-Proton D3D12 feature-level path reaches 11_0 and 11_1
- [x] Record 12_0–12_2 as unsupported on the current tested configuration
- [x] Add read-only VKD3D-Proton adapter
- [ ] Run sustained D3D12 render/present validation
- [ ] Keep D3D12 isolated from the D3D11 baseline until repeatable

## Phase 4 — Lightweight local AI advisor
- [ ] Select a small permissively licensed model/runtime after measuring memory and latency
- [ ] Consume structured compatibility and performance evidence only
- [ ] Emit bounded machine-readable recommendations with evidence and rollback values
- [ ] Validate every recommendation deterministically before caching
- [ ] Keep automatic application disabled until A/B regression evidence exists

## Phase 5 — Performance engineering and release
- [x] Run REDMI K80 Ultra Termux cache microbenchmark (4 MiB, 10 iterations): V2 file 4,194,363 bytes vs V1 8,388,660 bytes; median write 11.031 ms vs 227.076 ms; median warm read 10.703 ms vs 57.281 ms
- [ ] Repeat cache microbenchmark across payload sizes and fresh sessions
- [ ] Compare cold launch vs warm launch
- [ ] Measure shader compilation time and repeat-run stutter
- [ ] Measure CPU/GPU frame time, memory use, and thermal throttling
- [ ] Tune worker limits and cache size using measurements
- [ ] Publish a compatibility matrix with explicit test scope and confidence


## Phase 1.5 — Reproducible DXVK ARM64EC baseline
- [x] Pin upstream DXVK v3.1.1 source tag and exact commit
- [x] Pin an LLVM-MinGW toolchain release and verify its archive SHA-256
- [x] Add ARM64EC system32 + x86 syswow64 build workflow and package verifier
- [ ] Confirm the GitHub Actions cross-build succeeds and inspect the generated artifacts
- [ ] Install the verified package only in a separate test prefix/container
- [ ] Validate real D3D11 rendering, successful Present, and zero device-lost events
- [ ] Run at least 300 seconds of sustained game rendering before calling it playable
- [ ] Add NYAMIST Pipeline Gate / Command Governor patches only after the vanilla baseline

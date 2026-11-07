# Roadmap

## Phase 0 — Portable core (current)
- [x] C++20 project skeleton
- [x] Deterministic cache fingerprint
- [x] Versioned validated-cache prototype
- [x] Rule-based advisor interface
- [x] Unit tests and CI workflow

## Phase 1 — K80 Ultra capability baseline
- [ ] Capture Vulkan device/driver/API/extension report from the actual phone
- [ ] Confirm Android ABI, Wine/Box64 runtime, and DXVK build
- [ ] Record stock-driver baseline before installing experimental drivers
- [ ] Identify a small set of user-owned D3D11 test games
- [ ] Record launch result, rendering correctness, frame time, memory, and crash logs

## Phase 2 — DX11 integration
- [ ] Choose a supported integration boundary (external launcher/profile first)
- [ ] Add a DXVK version/build manifest and reproducible artifact workflow
- [ ] Translate actual device probes into NYAMIST profiles
- [ ] Keep DXVK shader and pipeline caches under DXVK control
- [ ] Add per-game regression results and safe fallback behavior

## Phase 3 — Local AI advisor
- [ ] Select a small permissively licensed model/runtime after measuring memory and latency
- [ ] Train or prompt only on structured compatibility/profile evidence
- [ ] Emit machine-readable recommendations
- [ ] Validate recommendations against a deterministic allow-list and game tests
- [ ] Cache only validated recommendations

## Phase 4 — Performance engineering
- [ ] Compare cold launch vs warm launch
- [ ] Measure shader compilation time and repeat-run stutter
- [ ] Measure CPU/GPU frame time and thermal throttling
- [ ] Tune worker limits and cache size using measurements, not assumptions

## Phase 5 — DX12 research
- [ ] Start only after Phase 1–4 DX11 results are repeatable
- [ ] Evaluate VKD3D-Proton and the target driver's required Vulkan feature set
- [ ] Keep DX12 work isolated from the stable DX11 path

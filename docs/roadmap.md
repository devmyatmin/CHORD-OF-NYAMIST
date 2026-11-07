# Roadmap

## Phase 0 — Portable core
- [x] C++20 project skeleton
- [x] Deterministic cache fingerprint
- [x] Versioned validated-cache prototype
- [x] Rule-based advisor interface
- [x] Unit tests and CI workflow
- [x] Termux probe and on-device core build scripts

## Phase 1 — K80 Ultra direct-hardware baseline
- [ ] Run the read-only Termux probe on the actual REDMI K80 Ultra
- [ ] Confirm Android API level, ABI, SoC/GPU identity, and kbase interface visibility
- [ ] Capture the Vulkan device, driver, extension list, and feature bits through the intended app/launcher
- [ ] Validate PanVK kbase ICD load without replacing the stock driver
- [ ] Run Vulkan smoke/feature tests and save logs
- [ ] Record stock-driver baseline and rollback procedure

## Phase 2 — DX11 integration
- [ ] Confirm Wine/FEX/Box64 and DXVK versions/builds
- [ ] Establish a small user-owned D3D11 game compatibility matrix
- [ ] Verify D3D11 device creation through DXVK + PanVK
- [ ] Reach repeatable game scenes with correct rendering
- [ ] Integrate NYAMIST profiles without changing DXVK rendering behavior
- [ ] Keep DXVK shader and pipeline caches under DXVK control

## Phase 3 — Local AI advisor
- [ ] Select a small permissively licensed model/runtime after measuring memory and latency
- [ ] Train or prompt only on structured compatibility/profile evidence
- [ ] Emit machine-readable recommendations
- [ ] Validate recommendations against deterministic rules and game tests
- [ ] Cache only validated recommendations

## Phase 4 — Performance engineering
- [ ] Compare cold launch vs warm launch
- [ ] Measure shader compilation time and repeat-run stutter
- [ ] Measure CPU/GPU frame time, memory use, and thermal throttling
- [ ] Tune worker limits and cache size using measurements

## Phase 5 — DX12 research
- [ ] Start only after Phase 1–4 DX11 results are repeatable
- [ ] Evaluate VKD3D-Proton and the target driver's required Vulkan feature set
- [ ] Keep DX12 work isolated from the stable DX11 path
